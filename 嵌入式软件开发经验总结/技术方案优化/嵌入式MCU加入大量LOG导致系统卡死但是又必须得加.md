结论：**你卡的不是 `USART_GetFlagStatus()` 函数本身，而是这里死等了：**

```c
while(!USART_GetFlagStatus(USART3, USART_FLAG_TC));
```

调用链大概率是：

```c
printf()
  -> fputc()
      -> log_transmit_byte()
          -> drv_uart3_transmit_byte()
              -> while 等待 USART_FLAG_TC
```

你一加大量打印，CPU 就一个字节一个字节等串口发完，系统启动流程直接被拖死。尤其如果打印发生在 **USART3 还没初始化完成之前**，`TC` 标志可能永远等不到，就表现为“卡在 `USART_GetFlagStatus()`”。

---

## 1. 你现在这个发送函数问题很大

你现在是：

```c
void drv_uart3_transmit_byte(char Data) {
  USART_SendData(USART3, Data);
  while(!USART_GetFlagStatus(USART3,USART_FLAG_TC));
}
```

### 问题 1：每发 1 个字节都等 TC，太慢

`TC` 是 **整个字节真的从 TX 引脚发完了**。

假设 115200 波特率：

```c
1 字节约 10 bit
10 / 115200 ≈ 86.8 us
```

你打印 1000 字节，光阻塞就接近：

```c
86.8 ms
```

如果是 9600 波特率，1000 字节就是 **1 秒左右**。

系统启动时一堆打印，当然卡。

---

## 2. 先给你一个最低成本修复版

把 `drv_uart3_transmit_byte()` 改成下面这样。

核心改动：

1. **不要等 TC，改等 TXE**
2. **加超时，防止死卡**
3. **加初始化完成标志，防止串口没初始化就 printf**

```c
#include "stm32f10x.h"

static volatile u8 g_uart3_ready = 0;

#define UART3_TX_TIMEOUT   10000

void drv_uart3_init(u32 baudrate)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    g_uart3_ready = 0;

    RCC_APB2PeriphClockCmd(USART3_RX_RCC, ENABLE);
    RCC_APB2PeriphClockCmd(USART3_TX_RCC, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3, ENABLE);
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);

    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;

    GPIO_InitStructure.GPIO_Pin = USART3_TX_PINS;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(USART3_TX_PORT, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = USART3_RX_PINS;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(USART3_RX_PORT, &GPIO_InitStructure);

    USART_InitStructure.USART_BaudRate = baudrate;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(USART3, &USART_InitStructure);

    USART_ClearFlag(USART3, USART_FLAG_TC);

    USART_ITConfig(USART3, USART_IT_RXNE, ENABLE);
    USART_ITConfig(USART3, USART_IT_TXE, DISABLE);

    NVIC_InitStructure.NVIC_IRQChannel = USART3_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = USART3_INTERRUPT_PRIORITY;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    USART_Cmd(USART3, ENABLE);

    g_uart3_ready = 1;
}
```

然后发送单字节函数改成：

```c
int drv_uart3_transmit_byte_timeout(char Data)
{
    u32 timeout = UART3_TX_TIMEOUT;

    // 串口还没初始化，直接丢弃，绝对不能死等
    if (g_uart3_ready == 0)
    {
        return -1;
    }

    // 等待发送数据寄存器为空，而不是等整个字节发送完成
    while (USART_GetFlagStatus(USART3, USART_FLAG_TXE) == RESET)
    {
        if (--timeout == 0)
        {
            return -2;
        }
    }

    USART_SendData(USART3, (uint8_t)Data);

    return 0;
}
```

然后 `fputc()` 改成：

```c
int fputc(int ch, FILE *f)
{
#ifdef DEBUG_LOG
    drv_uart3_transmit_byte_timeout((char)ch);
#endif

    return ch;
}
```

这样至少不会死卡。

---

## 3. 更推荐：printf 不要直接阻塞发送

你现在最危险的地方是：

```c
int fputc(int ch, FILE *f)
{
#ifdef DEBUG_LOG
    log_transmit_byte(ch);
#endif
    return ch;
}
```

而：

```c
#define log_transmit_byte drv_uart3_transmit_byte
```

所以 `printf` 每输出一个字符，都会阻塞等待发送。

建议改成：

```c
#define log_transmit_byte drv_uart3_send_byte_nonblock
```

也就是 **printf 只把数据塞进环形缓冲区，然后由 TXE 中断慢慢发送**。

---

## 4. 把你的 `drv_uart3_send()` 改成非阻塞版

你现在这个函数还有个问题：缓冲区满了以后，它会在那里等。

这段代码不适合大量日志：

```c
for (u16 exit = 0; exit < 0xffff; exit++) {
    u8 tx_pos = uart_send_info.tx_start_pos;
    if (tx_pos != uart_send_info.tx_curr_pos) {
        break;
    }
}
```

大量打印时，系统还是会被拖住。

建议改成：**缓冲区满了就丢日志，不要卡业务。**

```c
static volatile tUartSendData uart_send_info;

void drv_uart3_send_nonblock(u8 *buff, u8 length)
{
    u8 i;

    if (g_uart3_ready == 0)
    {
        return;
    }

    for (i = 0; i < length; i++)
    {
        u8 next_pos = uart_send_info.tx_curr_pos + 1;

        if (next_pos >= UART_SEND_LEN)
        {
            next_pos = 0;
        }

        // 缓冲区满了，直接丢弃后面的日志
        // 关键：日志不能影响主业务
        if (next_pos == uart_send_info.tx_start_pos)
        {
            break;
        }

        uart_send_info.tx_buff[uart_send_info.tx_curr_pos] = buff[i];
        uart_send_info.tx_curr_pos = next_pos;
    }

    // 开启发送空中断，由中断继续发送
    USART_ITConfig(USART3, USART_IT_TXE, ENABLE);
}
```

然后加一个单字节版本：

```c
void drv_uart3_send_byte_nonblock(char data)
{
    drv_uart3_send_nonblock((u8 *)&data, 1);
}
```

然后 `board.h` 里改成：

```c
void drv_uart3_send_byte_nonblock(char Data);
#define log_transmit_byte drv_uart3_send_byte_nonblock
```

这样 `printf()` 就不会长时间卡住系统。

---

## 5. USART3 中断发送部分可以这样保留

你的中断发送逻辑基本可以用：

```c
void USART3_IRQHandler(void)
{
    // 接收中断
    if (USART_GetITStatus(USART3, USART_IT_RXNE) != RESET)
    {
        u8 data = USART_ReceiveData(USART3);
        drv_uart3_receive_fun(data);
    }

    // 发送寄存器空中断
    if (USART_GetITStatus(USART3, USART_IT_TXE) != RESET)
    {
        if (uart_send_info.tx_curr_pos != uart_send_info.tx_start_pos)
        {
            USART_SendData(USART3, uart_send_info.tx_buff[uart_send_info.tx_start_pos]);

            uart_send_info.tx_start_pos++;

            if (uart_send_info.tx_start_pos >= UART_SEND_LEN)
            {
                uart_send_info.tx_start_pos = 0;
            }
        }
        else
        {
            USART_ITConfig(USART3, USART_IT_TXE, DISABLE);
        }
    }

    // 溢出错误处理
    if (USART_GetFlagStatus(USART3, USART_FLAG_ORE) == SET)
    {
        volatile u16 temp;

        // 清 ORE 的标准方式：先读 SR，再读 DR
        temp = USART3->SR;
        temp = USART3->DR;
        (void)temp;
    }

    // 发送完成中断
    if (USART_GetITStatus(USART3, USART_IT_TC) == SET)
    {
        USART_ClearITPendingBit(USART3, USART_IT_TC);
    }
}
```

这里接收中断里不建议：

```c
USART_ClearITPendingBit(USART3, USART_IT_RXNE);
```

`RXNE` 正常是通过读 `DR` 清除。

---

## 6. 如果用了 FreeRTOS，最推荐这样做

你现在这个思路其实是对的：

```c
xQueueSend(g_queue_debug, &item, 0);
```

但是要注意三点：

### 第一，队列没创建前不能打印

否则直接出问题。

```c
if (g_queue_debug == NULL)
{
    return;
}
```

### 第二，启动早期不要打印太多

比如这些地方不要疯狂打印：

```c
system_init();
drv_uart3_init();
各种外设初始化阶段;
中断服务函数;
1ms 定时任务;
```

### 第三，专门建一个日志任务

比如：

```c
void debug_uart_task(void *param)
{
    tLogItem item;

    while (1)
    {
        if (xQueueReceive(g_queue_debug, &item, portMAX_DELAY) == pdPASS)
        {
            drv_uart3_send_nonblock((u8 *)item.log, item.length);
        }
    }
}
```

然后 `debug_printf_string()` 只负责格式化和入队：

```c
void debug_printf_string(char type, char *format, ...)
{
    va_list aptr;
    tLogItem item;
    int ret;

    if (g_queue_debug == NULL)
    {
        return;
    }

    memset(&item, 0, sizeof(tLogItem));
    item.log[0] = type;
    item.log[1] = ' ';

    va_start(aptr, format);
    ret = vsnprintf(&(item.log[2]), sizeof(item.log) - 5, format, aptr);
    va_end(aptr);

    if (ret <= 0)
    {
        return;
    }

    item.length = strlen(item.log);

    if (item.length >= 2 &&
        item.log[item.length - 2] == '\r' &&
        item.log[item.length - 1] == '\n')
    {
        // 已经有 \r\n
    }
    else
    {
        if (item.length >= 1 && item.log[item.length - 1] == '\n')
        {
            item.length--;
        }

        item.log[item.length++] = '\r';
        item.log[item.length++] = '\n';
    }

    // 队列满了就丢，不要卡业务
    xQueueSend(g_queue_debug, &item, 0);
}
```

---

## 7. 你现在最应该立刻改的地方

优先级从高到低：

### 第一优先级：改掉这个死等

原来：

```c
while(!USART_GetFlagStatus(USART3,USART_FLAG_TC));
```

改成：

```c
while (USART_GetFlagStatus(USART3, USART_FLAG_TXE) == RESET)
{
    if (--timeout == 0)
    {
        return;
    }
}
```

### 第二优先级：`fputc()` 不要直接阻塞发送

原来：

```c
log_transmit_byte(ch);
```

建议改成：

```c
drv_uart3_send_byte_nonblock((char)ch);
```

### 第三优先级：打印必须限速

不要这样：

```c
while (1)
{
    printf("xxx\r\n");
}
```

改成：

```c
static u32 last_log_time = 0;

if (timer_elapsed_ms(last_log_time) >= 500)
{
    last_log_time = mode.sys_time_ms;
    printf("state = %d\r\n", sensor.work_state);
}
```

或者：

```c
static u16 log_cnt = 0;

if (++log_cnt >= 100)
{
    log_cnt = 0;
    printf("state = %d\r\n", sensor.work_state);
}
```

---

## 8. 一句话方案

你这个问题的本质是：

> **日志打印用了阻塞式串口发送，而且每个字节都等待 TC 完成。打印一多，CPU 全耗在串口等待里；如果串口未初始化就打印，还可能永久死等。**

最终建议架构：

```c
业务代码
  ↓
debug_printf_string / printf
  ↓
日志队列 / 环形缓冲区
  ↓
USART TXE 中断 或 DMA
  ↓
串口慢慢发
```

原则就是：

```c
日志可以丢，系统不能卡。
```

你先把 `drv_uart3_transmit_byte()` 的 `TC死等` 改掉，再把 `fputc()` 换成 `drv_uart3_send_byte_nonblock()`，这个启动卡死问题基本就能解决。



--------
---------
--------

对，**你这个才是主要凶手**。

你现在是：

```c
// 0.1ms
void SysTick_Handler(void)
{
    ...
    if (mode.giv_sys_start > 0)
    {
        drv_display_state();
        drv_display_wifi();
    }
    ...
}
```

而 `drv_display_state()` 和 `drv_display_wifi()` 里面都有：

```c
printf("xxx\r\n");
```

这就变成：

```c
0.1ms 中断
  -> drv_display_state()
      -> printf()
          -> fputc()
              -> 串口阻塞发送
  -> drv_display_wifi()
      -> printf()
          -> fputc()
              -> 串口阻塞发送
```

**0.1ms = 10kHz，也就是一秒进 10000 次中断。**

你里面两个函数各打印一次，相当于：

```c
一秒 20000 条 printf
```

STM32F103 根本扛不住。
所以你看到卡在：

```c
USART_GetFlagStatus()
```

本质是：**系统一直在中断里等串口发送完成，主程序跑不动了。**

---

# 1. 立即止血：中断里绝对不要 printf

先把这两个地方的 `printf` 直接干掉：

```c
void drv_display_state(void)
{
    ...
#if 0
    printf("[%d/%d/%d-%d:%d:%d]:led_state:%d\r\n",
        g_rtc_time.syear,
        g_rtc_time.smon,
        g_rtc_time.sday,
        g_rtc_time.hour,
        g_rtc_time.min,
        g_rtc_time.sec,
        led_state
    );
#endif
}
```

还有：

```c
void drv_display_wifi(void)
{
    ...
#if 0
    printf("[%d/%d/%d-%d:%d:%d]:led_wifi:%d\r\n",
        g_rtc_time.syear,
        g_rtc_time.smon,
        g_rtc_time.sday,
        g_rtc_time.hour,
        g_rtc_time.min,
        g_rtc_time.sec,
        led_wifi
    );
#endif
}
```

先这样改，系统大概率马上不卡了。

---

# 2. 正确做法：SysTick 只置标志，别干活

你现在 `SysTick_Handler()` 里面做太多事情了。

建议改成这样：

```c
void SysTick_Handler(void)
{
    static unsigned short ms = 0, second = 0, minute = 0;
    static u8 ms_1cnt = 0;
    static u8 ms_5cnt = 0;
    static u8 ms_10cnt = 0;
    static u8 ms_20cnt = 0;
    static u8 ms_25cnt = 0;
    static u8 ms_100cnt = 0;

    // 0.1ms计数
    mode.ring_dete_time++;

    // 蜂鸣器这种高频翻转可以保留，但也要尽量短
    BuzzerInterruptLoop();

    // 1ms
    ms_1cnt++;
    if (ms_1cnt > 9)
    {
        ms_1cnt = 0;

        mode.sys_time_1ms = 1;
        mode.sys_time_ms++;

        if (mode.sys_time_ms > 0xfff6d83f)
        {
            mode.sys_time_ms = 0;
        }

        ms++;
        if (ms >= 1000)
        {
            ms = 0;
            second++;
            mode.sys_time_sec++;

            if (second >= 60)
            {
                second = 0;
                minute++;
                mode.sys_time_min++;

                if (minute >= 60)
                {
                    minute = 0;
                    mode.sys_time_hour++;
                }
            }
        }

        // 5ms标志
        ms_5cnt++;
        if (ms_5cnt > 4)
        {
            ms_5cnt = 0;
            mode.sys_time_5ms = 1;
        }

        // 10ms标志
        ms_10cnt++;
        if (ms_10cnt > 9)
        {
            ms_10cnt = 0;
            mode.sys_time_10ms = 1;
        }

        // 20ms刷新状态灯
        ms_20cnt++;
        if (ms_20cnt > 19)
        {
            ms_20cnt = 0;
            mode.state_led_cycle = 1;
        }

        // 25ms标志
        ms_25cnt++;
        if (ms_25cnt > 24)
        {
            ms_25cnt = 0;
            mode.sys_time_25ms = 1;
        }

        // 100ms标志
        ms_100cnt++;
        if (ms_100cnt > 99)
        {
            ms_100cnt = 0;
            mode.sys_time_100ms = 1;
        }
    }
}
```

重点是：**不要在 SysTick 里面调用 `drv_display_state()` / `drv_display_wifi()`。**

---

# 3. 在 main while 里刷新显示

比如你的主循环里这样写：

```c
while (1)
{
    if (mode.state_led_cycle)
    {
        mode.state_led_cycle = 0;

        if (mode.giv_sys_start > 0)
        {
            drv_display_state();
            drv_display_wifi();
        }
    }

    // 其他任务
}
```

这样显示刷新频率变成 **20ms 一次，也就是 50Hz**，已经够用了。

原来是：

```c
0.1ms 一次 = 10000Hz
```

太夸张了。

LED 显示根本不需要 10000Hz 刷新。

---

# 4. 打印也不要每次刷新都打印

就算你把显示函数移出中断，也不要每 20ms 打一次。

20ms 一次就是：

```c
一秒 50 次
```

两个函数就是：

```c
一秒 100 条 printf
```

也没必要。

建议只在 **状态变化时打印**。

## drv_display_state 里这样改

```c
void drv_display_state(void)
{
    static u32 blink_cnt = 0, blink_red_cnt = 0, blink_box_cnt = 0;
    static u8 last_led_state = 0;
    static u8 last_print_led_state = 0xff;

    if (last_led_state != led_state || blink_red_flag)
    {
        blink_cnt = 0;
        blink_box_cnt = 0;
    }

    last_led_state = led_state;

    if (!sensor.dust_box && mode.sys_time_sec > 3)
    {
        blink_cnt = 0;
        blink_red_flag = 0;

        if (++blink_box_cnt > 50)
        {
            blink_box_cnt = 0;
        }

        if (blink_box_cnt < 25)
        {
            set_state_led_r(0);
            set_state_led_g(0);
            set_state_led_b(0);
        }
        else
        {
            set_state_led_r(0);
            set_state_led_g(0);
            set_state_led_b(0);
            set_state_led_r(0xff);
        }

        return;
    }

    if (blink_red_flag)
    {
        if (timer_elapsed_ms(dis_start) >= 3000)
        {
            dis_start = mode.sys_time_ms;
            blink_red_cnt = 0;
            blink_red_flag = 0;
        }

        if (++blink_red_cnt > 50)
        {
            blink_red_cnt = 0;
        }

        if (blink_red_cnt < 25)
        {
            set_state_led_r(0);
            set_state_led_g(0);
            set_state_led_b(0);
        }
        else
        {
            set_state_led_r(0xff);
        }

        return;
    }

    if (led_state == 1)
    {
        if (++blink_cnt > 50)
        {
            blink_cnt = 50;
        }

        if (blink_cnt < 25 && mode.sys_time_sec > 2)
        {
            set_state_led_r(0);
            set_state_led_g(0);
            set_state_led_b(0);
        }
        else
        {
            set_state_led_r(0);
            set_state_led_g(0x0f);
            set_state_led_b(0);
        }
    }
    else if (led_state == 3)
    {
        set_state_led_r(0xff);
        set_state_led_g(0x0f);
        set_state_led_b(0);
    }
    else if (led_state == 4)
    {
        blink_box_cnt = 0;

        if (++blink_cnt > 50)
        {
            blink_cnt = 0;
        }

        if (blink_cnt < 25)
        {
            set_state_led_r(0);
            set_state_led_g(0);
            set_state_led_b(0);
        }
        else
        {
            set_state_led_g(0x0f);
        }
    }
    else if (led_state == 5)
    {
        if (++blink_cnt > 50)
        {
            blink_cnt = 0;
        }

        if (blink_cnt < 25)
        {
            set_state_led_r(0);
            set_state_led_g(0);
            set_state_led_b(0);
        }
        else
        {
            set_state_led_r(0xff);
            set_state_led_g(0x0f);
        }
    }
    else if (led_state == 6)
    {
        if (++blink_cnt > 50)
        {
            blink_cnt = 0;
        }

        if (blink_cnt < 25)
        {
            set_state_led_r(0);
            set_state_led_g(0);
            set_state_led_b(0);
        }
        else
        {
            set_state_led_r(0xff);
        }
    }

    // 只在状态变化时打印
    if (last_print_led_state != led_state)
    {
        last_print_led_state = led_state;

        printf("[%d/%d/%d-%d:%d:%d]:led_state:%d\r\n",
               g_rtc_time.syear,
               g_rtc_time.smon,
               g_rtc_time.sday,
               g_rtc_time.hour,
               g_rtc_time.min,
               g_rtc_time.sec,
               led_state);
    }
}
```

注意我这里把原来的：

```c
10000
5000
```

改成了：

```c
50
25
```

因为你原来是 **0.1ms 调一次**：

```c
10000 * 0.1ms = 1000ms
5000  * 0.1ms = 500ms
```

现在如果改成 **20ms 调一次**：

```c
50 * 20ms = 1000ms
25 * 20ms = 500ms
```

所以逻辑节奏基本不变。

---

# 5. drv_display_wifi 也一样改

```c
void drv_display_wifi(void)
{
    static u32 blink_cnt = 0;
    static u8 last_led_wifi = 0;
    static u8 last_print_led_wifi = 0xff;

    if (last_led_wifi != led_wifi)
    {
        blink_cnt = 0;
    }

    last_led_wifi = led_wifi;

    if (led_wifi == 1)
    {
        set_wifi_led_r(0);
        set_wifi_led_g(0);
        set_wifi_led_b(0xff);
    }
    else if (led_wifi == 2)
    {
        if (timer_elapsed_ms(start_wifi_bule * 1000) >= 590000)
        {
            start_wifi_bule = mode.sys_time_sec;
            blink_cnt = 0;
            led_wifi = 1;
        }

        if (++blink_cnt > 70)
        {
            blink_cnt = 0;
        }

        if (blink_cnt < 35)
        {
            set_wifi_led_r(0);
            set_wifi_led_g(0);
            set_wifi_led_b(0);
        }
        else
        {
            set_wifi_led_b(0xff);
        }
    }
    else if (led_wifi == 3)
    {
        set_wifi_led_r(0);
        set_wifi_led_g(0x0f);
        set_wifi_led_b(0);
    }
    else if (led_wifi == 4)
    {
        if (timer_elapsed_ms(start_wifi_red) >= 4200)
        {
            start_wifi_red = mode.sys_time_ms;
            blink_cnt = 0;
            led_wifi = 1;
        }

        if (++blink_cnt > 70)
        {
            blink_cnt = 0;
        }

        if (blink_cnt < 35)
        {
            set_wifi_led_r(0);
            set_wifi_led_g(0);
            set_wifi_led_b(0);
        }
        else
        {
            set_wifi_led_r(0xff);
        }
    }

    // 只在wifi灯状态变化时打印
    if (last_print_led_wifi != led_wifi)
    {
        last_print_led_wifi = led_wifi;

        printf("[%d/%d/%d-%d:%d:%d]:led_wifi:%d\r\n",
               g_rtc_time.syear,
               g_rtc_time.smon,
               g_rtc_time.sday,
               g_rtc_time.hour,
               g_rtc_time.min,
               g_rtc_time.sec,
               led_wifi);
    }
}
```

不过这个地方我更建议你把 10 分钟判断改简单点。

你原来：

```c
start_wifi_bule = mode.sys_time_sec;
```

那判断也用秒：

```c
if ((mode.sys_time_sec - start_wifi_bule) >= 590)
{
    start_wifi_bule = mode.sys_time_sec;
    blink_cnt = 0;
    led_wifi = 1;
}
```

所以建议这一段改成：

```c
if ((mode.sys_time_sec - start_wifi_bule) >= 590)
{
    start_wifi_bule = mode.sys_time_sec;
    blink_cnt = 0;
    led_wifi = 1;
}
```

---

# 6. 更推荐的打印方式：统一限频

你可以定义一个宏，避免某个函数不小心狂刷：

```c
#define LOG_INTERVAL_MS(start, interval_ms)        \
    do                                             \
    {                                              \
        if (timer_elapsed_ms(start) < interval_ms) \
        {                                          \
            return;                                \
        }                                          \
        start = mode.sys_time_ms;                  \
    } while (0)
```

然后专门写一个调试函数，不要放在中断里：

```c
void display_debug_report(void)
{
    static u32 last_time = 0;

    if (timer_elapsed_ms(last_time) < 1000)
    {
        return;
    }

    last_time = mode.sys_time_ms;

    printf("[%d/%d/%d-%d:%d:%d]:led_state:%d, led_wifi:%d\r\n",
           g_rtc_time.syear,
           g_rtc_time.smon,
           g_rtc_time.sday,
           g_rtc_time.hour,
           g_rtc_time.min,
           g_rtc_time.sec,
           led_state,
           led_wifi);
}
```

主循环调用：

```c
while (1)
{
    if (mode.state_led_cycle)
    {
        mode.state_led_cycle = 0;

        if (mode.giv_sys_start > 0)
        {
            drv_display_state();
            drv_display_wifi();
        }
    }

    display_debug_report();
}
```

这样最多：

```c
1秒打印1次
```

系统压力就很小了。

---

# 7. 你现在必须遵守这个原则

以后嵌入式里记住：

```c
中断里不要 printf
中断里不要 sprintf / snprintf
中断里不要 malloc
中断里不要 delay
中断里不要 while 等外设状态
中断里不要做耗时逻辑
```

中断里只做这种事：

```c
计数
置标志
读写简单寄存器
清中断标志
```

也就是：

```c
SysTick_Handler()
{
    mode.sys_time_ms++;
    mode.sys_time_10ms = 1;
    mode.sys_time_100ms = 1;
}
```

然后主循环里再做：

```c
if (mode.sys_time_100ms)
{
    mode.sys_time_100ms = 0;
    业务处理();
}
```

---

# 8. 最终改法总结

你这个问题的根因是：

> `SysTick_Handler()` 每 0.1ms 调用 `drv_display_state()` 和 `drv_display_wifi()`，而这两个函数内部有 `printf()`，导致中断里高频阻塞串口发送，系统卡死在 `USART_GetFlagStatus()`。

建议你这样改：

```c
SysTick中断：
    只更新时间
    只置 1ms/10ms/20ms/100ms 标志

main while：
    根据标志调用 drv_display_state()
    根据标志调用 drv_display_wifi()
    根据限频函数打印日志

drv_display_state / drv_display_wifi：
    不要无脑 printf
    只在状态变化时打印
    或者最多 1 秒打印一次
```

最小修复就是这三步：

```c
1. 注释掉 drv_display_state() 里的 printf
2. 注释掉 drv_display_wifi() 里的 printf
3. SysTick_Handler() 里不要直接调用这两个函数，改成置标志，主循环调用
```

这个改完，你系统启动卡死的问题基本就会消失。


---------
---------
--------
这些都是 **USART 状态寄存器 SR 里的标志位**，用来告诉你串口当前发生了什么事。

你这个：

```c
#define USART_FLAG_TXE   ((uint16_t)0x0080)
#define USART_FLAG_TC    ((uint16_t)0x0040)
#define USART_FLAG_RXNE  ((uint16_t)0x0020)
```

本质就是拿这些 bit 去和 `USARTx->SR` 做与运算：

```c
if ((USARTx->SR & USART_FLAG_TXE) != 0)
{
    // TXE 置位了
}
```

---

## 最常用的 3 个：TXE / TC / RXNE

### 1. `USART_FLAG_TXE`：发送数据寄存器空

```c
#define USART_FLAG_TXE  0x0080
```

意思是：

> USART 的发送数据寄存器 `DR` 空了，可以写入下一个字节。

常用于连续发送数据：

```c
while (USART_GetFlagStatus(USART3, USART_FLAG_TXE) == RESET);

USART_SendData(USART3, data);
```

注意：**TXE 置位，不代表这个字节已经从 TX 引脚发完了。**

它只表示：

```text
DR 空了，可以塞下一个字节了
```

所以大量发送、printf、发送字符串时，应该主要用 `TXE`。

---

### 2. `USART_FLAG_TC`：发送完成

```c
#define USART_FLAG_TC  0x0040
```

意思是：

> 最后一个字节已经真正从 TX 引脚发送完成了。

也就是说：

```text
DR 空了
移位寄存器也空了
最后一个停止位也发出去了
```

这个比 `TXE` 更“彻底”。

适合用在这些场景：

```c
发送完整一帧后，等待 TC
然后关闭 RS485 发送使能
然后切换半双工方向
然后进入低功耗
```

比如 RS485：

```c
RS485_TX_EN();

发送一整帧数据();

while (USART_GetFlagStatus(USART3, USART_FLAG_TC) == RESET);

RS485_RX_EN();
```

但是你之前的问题就在这里：

```c
while(!USART_GetFlagStatus(USART3, USART_FLAG_TC));
```

你每发 1 个字节都等 `TC`，效率特别低。大量打印时就卡死一样。

**普通 printf 不建议每字节等 TC，应该等 TXE。**

---

### 3. `USART_FLAG_RXNE`：接收数据寄存器非空

```c
#define USART_FLAG_RXNE  0x0020
```

意思是：

> 收到了一个字节，`DR` 里有数据可以读。

常用在接收中断里：

```c
if (USART_GetITStatus(USART3, USART_IT_RXNE) != RESET)
{
    uint8_t data = USART_ReceiveData(USART3);
}
```

或者轮询：

```c
if (USART_GetFlagStatus(USART3, USART_FLAG_RXNE) == SET)
{
    uint8_t data = USART_ReceiveData(USART3);
}
```

`RXNE` 一般通过读 `DR` 清除：

```c
data = USARTx->DR;
```

---

## 发送相关标志区别：TXE 和 TC

这个最重要。

| 标志  | 名字                           | 含义                | 什么时候用                 |
| --- | ---------------------------- | ----------------- | --------------------- |
| TXE | Transmit data register empty | 发送数据寄存器空，可以写下一个字节 | 连续发送、printf、中断发送      |
| TC  | Transmission complete        | 整个字节真正从 TX 引脚发完   | 一帧发完后，关闭发送器/RS485方向切换 |

可以这么理解：

```text
USART_SendData()
    ↓
数据先进 DR
    ↓
DR 数据转入移位寄存器
    ↓
TXE = 1，说明 DR 空了，可以写下一个字节
    ↓
移位寄存器把数据一位一位发出去
    ↓
最后停止位发完
    ↓
TC = 1，说明真的发完了
```

所以：

```c
while (TXE == RESET);  // 等可以写下一个字节
USART_SendData(...);
```

适合普通发送。

而：

```c
while (TC == RESET);   // 等最后一个字节完全发完
```

适合整包发送完之后再用。

---

## 接收相关标志

### `USART_FLAG_IDLE`：空闲帧检测

```c
#define USART_FLAG_IDLE  0x0010
```

意思是：

> 串口接收线上出现了一段空闲时间。

这个在 **串口 DMA 接收不定长数据** 时特别常用。

比如上位机发：

```text
01 03 00 00 00 02 C4 0B
```

你不知道它到底发几个字节，就可以用 DMA 一直接收，然后用 `IDLE` 中断判断“一帧结束”。

清除 IDLE 通常要：

```c
volatile uint32_t temp;

temp = USARTx->SR;
temp = USARTx->DR;
(void)temp;
```

也就是 **先读 SR，再读 DR**。

---

### `USART_FLAG_ORE`：接收溢出错误

```c
#define USART_FLAG_ORE  0x0008
```

意思是：

> 上一个字节还没读走，新字节又来了，导致数据被覆盖/丢失。

常见原因：

```text
中断处理太慢
关中断太久
printf 太多卡住系统
接收没有及时读 DR
波特率太高
```

你现在这种“中断里 printf”就很容易导致 ORE。

清除 ORE 的标准方式：

```c
volatile uint16_t temp;

temp = USART3->SR;
temp = USART3->DR;
(void)temp;
```

你代码里这个思路是对的：

```c
if (USART_GetFlagStatus(USART3, USART_FLAG_ORE) == SET)
{
    USART_ClearFlag(USART3, USART_FLAG_ORE);
    USART_ReceiveData(USART3);
}
```

不过更标准写法是直接读 SR 再读 DR。

---

## 错误相关标志

### `USART_FLAG_NE`：噪声错误

```c
#define USART_FLAG_NE  0x0004
```

意思是：

> 串口接收过程中检测到噪声，采样不稳定。

常见原因：

```text
串口线太长
地线不好
波特率不匹配
干扰大
电平转换芯片问题
```

---

### `USART_FLAG_FE`：帧错误

```c
#define USART_FLAG_FE  0x0002
```

意思是：

> 接收帧格式错误，通常是停止位不对。

常见原因：

```text
波特率不一致
数据位/停止位配置不一致
串口线干扰
对方设备发送异常
```

比如你这边配置：

```c
115200, 8N1
```

对方实际是：

```c
9600, 8N1
```

就容易出现 FE。

---

### `USART_FLAG_PE`：奇偶校验错误

```c
#define USART_FLAG_PE  0x0001
```

意思是：

> 奇偶校验失败。

只有你开启了奇偶校验才有意义，比如：

```c
USART_Parity_Even
USART_Parity_Odd
```

你现在配置的是：

```c
USART_Parity_No
```

所以一般不用管 `PE`。

---

## 其他不常用标志

### `USART_FLAG_CTS`：CTS 变化标志

```c
#define USART_FLAG_CTS  0x0200
```

CTS 是硬件流控相关的。

比如串口有这些引脚：

```text
TX
RX
RTS
CTS
```

`CTS` 用于告诉发送方：

```text
对方现在能不能接收数据
```

普通 MCU 串口通信基本不用。
你配置里也是：

```c
USART_HardwareFlowControl_None
```

所以这个不用管。

---

### `USART_FLAG_LBD`：LIN Break 检测

```c
#define USART_FLAG_LBD  0x0100
```

LIN 总线相关，普通 UART 通信基本不用。

---

## 总表

| 标志                | 含义           | 常用程度 | 典型用途       |
| ----------------- | ------------ | ---- | ---------- |
| `USART_FLAG_TXE`  | 发送数据寄存器空     | 很常用  | 可以写下一个字节   |
| `USART_FLAG_TC`   | 发送完成         | 很常用  | 最后一字节真正发完  |
| `USART_FLAG_RXNE` | 接收寄存器非空      | 很常用  | 收到一个字节     |
| `USART_FLAG_IDLE` | 空闲线检测        | 常用   | DMA 接收不定长帧 |
| `USART_FLAG_ORE`  | 接收溢出         | 常用   | 接收太慢，数据丢了  |
| `USART_FLAG_NE`   | 噪声错误         | 偶尔用  | 线路干扰       |
| `USART_FLAG_FE`   | 帧错误          | 偶尔用  | 波特率/格式错误   |
| `USART_FLAG_PE`   | 奇偶校验错误       | 少用   | 开启校验时使用    |
| `USART_FLAG_CTS`  | CTS 变化       | 少用   | 硬件流控       |
| `USART_FLAG_LBD`  | LIN Break 检测 | 少用   | LIN 总线     |

---

## 结合你现在的代码，应该这样用

### 发送单字节，别用 TC 死等

不要这样：

```c
USART_SendData(USART3, Data);
while(!USART_GetFlagStatus(USART3, USART_FLAG_TC));
```

建议这样：

```c
void drv_uart3_transmit_byte(char Data)
{
    uint32_t timeout = 10000;

    while (USART_GetFlagStatus(USART3, USART_FLAG_TXE) == RESET)
    {
        if (--timeout == 0)
        {
            return;
        }
    }

    USART_SendData(USART3, Data);
}
```

### 发完整字符串后，最后可以等一次 TC

```c
void uart3_send_buf(uint8_t *buf, uint16_t len)
{
    uint16_t i;

    for (i = 0; i < len; i++)
    {
        while (USART_GetFlagStatus(USART3, USART_FLAG_TXE) == RESET);
        USART_SendData(USART3, buf[i]);
    }

    // 只在整包最后等一次 TC
    while (USART_GetFlagStatus(USART3, USART_FLAG_TC) == RESET);
}
```

---

## 你记住一句话就行

```text
TXE：可以塞下一个字节了。
TC ：最后一个字节真的发完了。
RXNE：收到一个字节了。
ORE：你接收太慢，数据溢出了。
IDLE：一帧数据可能结束了。
```

你现在打印卡死，核心就是 **把 TC 当成 TXE 用了，而且还在 SysTick 高频中断里 printf**。
正确做法是：**普通打印用 TXE / 中断 / DMA，只有整帧最后才考虑 TC。**
