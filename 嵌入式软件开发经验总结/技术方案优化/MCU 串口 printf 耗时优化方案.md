有，而且**优化空间非常大**。MCU 上 `printf` 慢，通常不是因为“串口外设慢一点”，而是因为你把 **格式化 + 串口阻塞发送** 都放在当前任务/中断里执行了。

核心结论：

> **不要让 `printf` 直接阻塞发送串口。应该改成：先把日志写入 RAM 缓冲区，再用 DMA / 中断慢慢发出去。**

---

## 1. 为什么串口 `printf` 特别耗时间？

假设你串口波特率是 **115200**，UART 常见格式是：

```c
1 起始位 + 8 数据位 + 1 停止位 = 10 bit / 字节
```

所以一个字节发送时间：

```text
10 / 115200 ≈ 86.8 us
```

如果你打印一行：

```c
printf("weight=%d raw=%d filter=%d state=%d\r\n", ...);
```

假设有 80 字节，那么光串口发出去就要：

```text
80 * 86.8 us ≈ 6.9 ms
```

如果你在 **1ms 周期任务**、**ADC采样任务**、**电机控制环** 里这么打印，系统直接被拖死。

如果是 921600 波特率：

```text
10 / 921600 ≈ 10.8 us / byte
80 字节 ≈ 0.86 ms
```

所以第一层优化就是：**提高波特率**。

---

## 2. 最糟糕的写法：阻塞式 `putchar`

很多工程里 `printf` 重定向是这样的：

```c
int fputc(int ch, FILE *f)
{
    while (usart_flag_get(USART0, USART_FLAG_TBE) == RESET);
    usart_data_transmit(USART0, ch);
    return ch;
}
```

或者 STM32 HAL 里：

```c
HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, 0xFFFF);
```

这种方式的问题是：

```text
printf() 打印 100 个字符
= 循环等待 100 次串口发送空
= 当前任务一直卡在那里
```

所以你会感觉 MCU 被 `printf` 拖慢。

---

## 3. 第一优先级优化：提高波特率

如果只是调试，建议直接用：

```text
921600
1000000
1500000
2000000
```

常用推荐：

```text
115200：太慢，只适合少量日志
460800：一般够用
921600：推荐
1000000：也很常见
2000000：看 USB 转串口芯片和线材质量
```

CH340、CP2102、FT232、DAPLink 虚拟串口一般都能跑到 921600 或 1M。

但是注意：**提高波特率只能缓解，不能从根本解决阻塞问题。**

---

## 4. 正确架构：日志环形缓冲区 + UART DMA 发送

推荐结构是这样：

```text
业务代码调用 LOG_INFO()
        ↓
vsnprintf 格式化到临时 buffer
        ↓
写入 log 环形缓冲区
        ↓
立即返回，不等待串口发送
        ↓
后台用 UART DMA 慢慢发送
        ↓
DMA 发送完成中断里继续发送下一段
```

这样业务代码不会被串口拖住。

---

## 5. 简化版代码思路

### 5.1 日志接口

```c
#define LOG_BUF_SIZE 2048

static uint8_t log_buf[LOG_BUF_SIZE];
static volatile uint16_t log_w = 0;
static volatile uint16_t log_r = 0;
static volatile uint8_t uart_dma_busy = 0;
```

---

### 5.2 写入环形缓冲区

```c
static void log_buf_put(uint8_t ch)
{
    uint16_t next = (log_w + 1) % LOG_BUF_SIZE;

    // 缓冲区满了，直接丢弃新数据
    // 也可以改成覆盖旧数据
    if (next == log_r)
    {
        return;
    }

    log_buf[log_w] = ch;
    log_w = next;
}
```

---

### 5.3 `log_printf`

```c
void log_printf(const char *fmt, ...)
{
    char temp[128];

    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(temp, sizeof(temp), fmt, args);
    va_end(args);

    if (len <= 0)
    {
        return;
    }

    if (len > sizeof(temp))
    {
        len = sizeof(temp);
    }

    __disable_irq();

    for (int i = 0; i < len; i++)
    {
        log_buf_put((uint8_t)temp[i]);
    }

    __enable_irq();

    uart_log_start_dma();
}
```

你以后不用直接：

```c
printf("xxx\r\n");
```

而是用：

```c
log_printf("weight=%d raw=%d\r\n", weight, raw);
```

---

### 5.4 启动 DMA 发送

这里是伪代码，不同 MCU 的 DMA API 不一样，GD32、STM32、ESP32 都要按自己的库改。

```c
static uint8_t dma_temp[256];
static uint16_t dma_len = 0;

void uart_log_start_dma(void)
{
    if (uart_dma_busy)
    {
        return;
    }

    if (log_r == log_w)
    {
        return;
    }

    dma_len = 0;

    while ((log_r != log_w) && (dma_len < sizeof(dma_temp)))
    {
        dma_temp[dma_len++] = log_buf[log_r];
        log_r = (log_r + 1) % LOG_BUF_SIZE;
    }

    if (dma_len > 0)
    {
        uart_dma_busy = 1;

        /*
         * 这里换成你的 MCU 串口 DMA 发送函数
         * 例如：
         * HAL_UART_Transmit_DMA(&huart1, dma_temp, dma_len);
         * 或 GD32 的 DMA 配置 + USART DMA enable
         */
        uart_dma_send(dma_temp, dma_len);
    }
}
```

---

### 5.5 DMA 发送完成中断

```c
void uart_dma_tx_complete_callback(void)
{
    uart_dma_busy = 0;

    // 如果缓冲区里还有日志，继续发下一包
    uart_log_start_dma();
}
```

这样你的 `log_printf()` 只是把数据扔进 RAM，然后立即返回。真正耗时间的串口发送交给 DMA。

---

## 6. 如果不想上 DMA，也可以用串口发送中断

第二选择是：

```text
环形缓冲区 + USART TBE/TXE 中断
```

原理：

```text
printf 写入 ring buffer
打开 TXE 中断
TXE 中断每次发 1 字节
发完关闭 TXE 中断
```

优点：

```text
实现比 DMA 简单
```

缺点：

```text
每个字节进一次中断
日志量大时中断频率高
```

所以更推荐：

```text
日志量少：中断发送可以
日志量大：DMA 发送更好
```

---

## 7. 避免在中断里 printf

这个非常重要。

不要这样：

```c
void ADC_IRQHandler(void)
{
    printf("adc=%d\r\n", adc_value);
}
```

也不要这样：

```c
void DMA_IRQHandler(void)
{
    printf("dma done\r\n");
}
```

中断里打印会导致：

```text
1. 中断执行时间变长
2. 影响其他中断响应
3. 容易和串口发送中断/DMA冲突
4. 可能造成死锁
5. 实时控制系统抖动严重
```

正确做法：

```c
volatile uint8_t adc_flag = 0;

void ADC_IRQHandler(void)
{
    adc_flag = 1;
}
```

主循环或任务里：

```c
if (adc_flag)
{
    adc_flag = 0;
    log_printf("adc=%d\r\n", adc_value);
}
```

---

## 8. 少打印，控制打印频率

比如你采集重量 ADC，可能 5ms、10ms 一次。

不要每次都打印：

```c
log_printf("weight=%d\r\n", weight);
```

可以改成每 100ms 或 200ms 打印一次：

```c
static uint32_t last_log_time = 0;

if (sys_ms - last_log_time >= 100)
{
    last_log_time = sys_ms;
    log_printf("raw=%d filter=%d weight=%d\r\n",
               raw, filter, weight);
}
```

尤其是你做猫砂盆称重、FOC、电机控制、ADC滤波时，日志频率要限制。

推荐：

```text
普通状态日志：500ms ~ 1000ms 一次
ADC波形调试：20ms ~ 100ms 一次
电机控制日志：不要在控制环里直接打
错误日志：立即打印或保存
```

---

## 9. 避免打印浮点数

这个也很关键。

下面这个很耗资源：

```c
printf("weight=%.2f\r\n", weight);
```

原因是：

```text
1. 浮点格式化慢
2. 占 Flash 大
3. 占栈空间
4. 小 MCU 上可能拖垮实时性
```

建议改成整数放大法：

```c
int weight_x100 = (int)(weight * 100);

log_printf("weight=%d.%02d kg\r\n",
           weight_x100 / 100,
           weight_x100 % 100);
```

比如 3.25kg 打印成：

```text
weight=3.25 kg
```

这样比 `%f` 快很多。

---

## 10. 日志等级控制

不要所有日志一直开着。

可以这样：

```c
#define LOG_LEVEL_DEBUG  0
#define LOG_LEVEL_INFO   1
#define LOG_LEVEL_WARN   2
#define LOG_LEVEL_ERROR  3

#define CURRENT_LOG_LEVEL LOG_LEVEL_INFO

#if CURRENT_LOG_LEVEL <= LOG_LEVEL_DEBUG
#define LOG_DEBUG(fmt, ...) log_printf("[D] " fmt, ##__VA_ARGS__)
#else
#define LOG_DEBUG(fmt, ...)
#endif

#if CURRENT_LOG_LEVEL <= LOG_LEVEL_INFO
#define LOG_INFO(fmt, ...)  log_printf("[I] " fmt, ##__VA_ARGS__)
#else
#define LOG_INFO(fmt, ...)
#endif

#define LOG_WARN(fmt, ...)  log_printf("[W] " fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) log_printf("[E] " fmt, ##__VA_ARGS__)
```

正式版本可以关掉 DEBUG：

```c
#define CURRENT_LOG_LEVEL LOG_LEVEL_WARN
```

这样很多调试日志编译后就没了，不占时间。

---

## 11. 用二进制日志代替字符串日志

如果你要大量采样，比如 ADC 波形：

不推荐：

```c
log_printf("adc=%d weight=%d filter=%d\r\n", adc, weight, filter);
```

可以改成二进制帧：

```c
typedef struct
{
    uint16_t head;
    int16_t adc;
    int16_t weight;
    int16_t filter;
    uint16_t crc;
} log_frame_t;
```

发送：

```c
log_frame_t frame;

frame.head = 0xA55A;
frame.adc = adc;
frame.weight = weight;
frame.filter = filter;
frame.crc = 0;

uart_send_dma((uint8_t *)&frame, sizeof(frame));
```

优势：

```text
字符串日志：几十到几百字节
二进制日志：十几个字节
```

缺点是 PC 端需要写 Python 脚本解析。

如果你想看 ADC/重量波形，二进制日志 + Python 上位机/串口绘图，比 `printf` 高效很多。

---

## 12. 使用 RTT / SWO / ITM

如果你用的是 ARM Cortex-M，可以考虑：

```text
SEGGER RTT
SWO / ITM
J-Link RTT Viewer
Ozone
SystemView
```

### SEGGER RTT 优点

```text
速度快
不占 UART
适合大量调试日志
对实时性影响比 printf 小
```

### 缺点

```text
需要 J-Link
产品现场调试不如 UART 通用
```

如果你用 J-Link 调试 STM32/GD32，RTT 很推荐。

---

## 13. 千万注意 semihosting

如果你用了 Keil/IAR/GCC 的半主机模式 `semihosting`，`printf` 会特别慢，甚至离开调试器后直接卡死。

现象：

```text
1. Debug 时 printf 能输出
2. 不接调试器程序卡住
3. printf 特别慢
```

解决：

```text
关闭 semihosting
重定向 printf 到 UART / RTT
```

Keil 里经常要注意：

```c
#pragma import(__use_no_semihosting)
```

或者实现：

```c
void _sys_exit(int x)
{
    while (1);
}
```

---

## 14. 推荐优化优先级

按照实战优先级，我建议你这么做：

```text
第一步：串口波特率改到 921600 或 1000000

第二步：不要在中断里 printf

第三步：控制打印频率，不要每次采样都打印

第四步：把阻塞式 printf 改成 ring buffer + DMA

第五步：关闭浮点 printf，用整数放大打印

第六步：增加 LOG_DEBUG / LOG_INFO / LOG_ERROR 等等级

第七步：大量数据改用二进制帧

第八步：条件允许就用 SEGGER RTT / SWO
```

---

## 15. 最推荐的最终方案

你的 MCU 项目里，我建议日志系统做成这样：

```text
应用层：
LOG_INFO("cat in weight=%d\r\n", weight);

日志层：
log_printf()
vsnprintf()
写 ring buffer

驱动层：
UART DMA 发送

中断层：
DMA发送完成中断继续发下一包
```

不要再让业务代码直接调用阻塞式 `printf`。

---

## 16. 一句话总结

MCU 上 `printf` 慢的本质是：

> **字符串格式化耗 CPU，串口发送耗时间，阻塞式发送会卡住当前任务。**

最有效的优化是：

> **高波特率 + 日志限频 + 禁止中断 printf + 环形缓冲区 + UART DMA 异步发送。**


----------
-----------
----------
# MCU串口printf耗时优化方案
MCU串口`printf`**慢的核心原因**：串口是**低速异步通信**（常见波特率9600~115200），默认`printf`是**阻塞发送**——CPU要一直等待数据逐字节发完，期间啥都干不了，直接卡死主线程。

下面给你**从最简单到最进阶**的优化方法，按优先级使用，基本能解决99%的卡顿问题。

---

## 一、最快见效：硬件层优化（零代码改动）
### 1. 拉满串口波特率
这是最直接的优化，**不写一行代码**。
- 默认常用：9600/19200（极慢）
- 推荐设置：**115200 / 460800 / 921600**
- 高速MCU：**2M / 4M 波特率**（只要硬件支持）

**对比**：9600波特率发10字节需要≈10ms，115200只需要≈0.8ms，速度提升10倍+。

### 2. 关闭不必要的打印
调试完立刻关闭冗余`printf`，用宏开关一键控制：
```c
// 调试开启，发布关闭
#define DEBUG_EN 1
#if DEBUG_EN
#define debug_printf printf
#else
#define debug_printf(...)
#endif
```

---

## 二、核心优化：把阻塞printf改成**非阻塞+DMA发送**（推荐！）
这是**工业级最优解**，能让`printf`耗时从**毫秒级降到微秒级**。

### 原理
- 阻塞printf：CPU等待每一个字节发送完成
- **DMA+非阻塞**：CPU把数据丢给DMA，直接去干别的事，硬件自动发完，CPU完全不等待

### 标准实现（STM32示例，兼容所有MCU）
1. 开启串口**TX DMA**（非循环模式）
2. 重写`fputc`，使用**DMA+环形缓冲区**
3. `printf`只把数据塞进缓冲区，立刻返回

```c
// 环形缓冲区大小（根据需求调整）
#define UART_TX_BUF_SIZE 256
uint8_t uart_tx_buf[UART_TX_BUF_SIZE];
volatile uint16_t tx_w = 0, tx_r = 0;

// 重定向printf，非阻塞写入缓冲区
int fputc(int ch, FILE *f) {
    uint16_t next_w = (tx_w + 1) % UART_TX_BUF_SIZE;
    // 缓冲区满可以选择阻塞/丢弃，调试建议阻塞，正式版丢弃
    while(next_w == tx_r); 
    
    uart_tx_buf[tx_w] = ch;
    tx_w = next_w;
    
    // 启动DMA发送（空闲时才启动）
    if(__HAL_UART_GET_FLAG(&huart1, UART_FLAG_TC) && !busy) {
        start_dma_tx();
    }
    return ch;
}

// DMA发送完成中断，自动发送下一段数据
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart) {
    if(huart->Instance == USART1) {
        if(tx_r != tx_w) start_dma_tx();
        else busy = 0;
    }
}
```

**效果**：调用`printf`几乎**不耗时**，CPU完全解放。

---

## 三、轻量优化：不用DMA，用**串口发送中断**
如果不想用DMA，用**TX中断**也能实现非阻塞：
- 数据写入缓冲区
- 串口硬件空时，中断自动发下一个字节
- CPU不等待，只在中断里处理

适合**资源极少的小MCU**（如51、AVR、STM32G031）。

---

## 四、减少数据量：打印内容精简优化
同样波特率下，**发得越少，速度越快**。
1. **精简浮点数打印**
   `printf("%.2f", 1.23)` 浮点运算+解析极慢，MCU尽量用**整数打印**：
   ```c
   // 慢
   printf("temp:%.2f\n", temp); 
   // 快（整数代替）
   printf("temp:%d.%02d\n", temp/100, temp%100); 
   ```
2. **缩短字符串**
   `debug:ok` → `ok`，少发一个字节快一个字节。
3. **关闭printf冗余功能**
   编译器配置关闭**浮点支持、长整型支持**，库函数体积更小、解析更快。

---

## 五、进阶优化：批量发送 + 降低打印频率
1. **批量打印**
   不要循环里逐字节打印，攒成一包再发：
   ```c
   // 慢
   for(int i=0;i<10;i++) printf("%d",i);
   // 快
   char buf[32];
   sprintf(buf,"%d%d%d...",a,b,c);
   printf("%s",buf);
   ```
2. **降低打印频率**
   1ms中断里别打印，改成**10ms/100ms**打印一次。

---

## 六、终极方案：使用**专用日志库**
如果项目打印量大，直接替换标准`printf`：
- **tinyprintf**：超轻量，速度比原生快30%+
- **log_printf**：带等级过滤、时间戳
- **RTT日志**（J-Link）：**速度比串口快100倍**，不占用串口

---

# 优化方案优先级（直接照做）
1. **先改波特率到115200+**（1分钟见效）
2. **加宏开关关闭无用打印**
3. **串口TX + DMA非阻塞printf**（最推荐，根治卡顿）
4. 精简打印内容、避免浮点数
5. 批量发送、降低打印频率

---

### 总结
- 阻塞printf慢是因为**CPU死等硬件**；
- 最优解：**高波特率 + DMA非阻塞发送**；
- 优化后`printf`耗时可从**毫秒级降到10us内**，完全不影响实时性。

我可以给你**直接可用的STM32/51/ESP32 DMA printf完整工程代码**，你用的是什么MCU？
