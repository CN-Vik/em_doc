裸机里不要用“死等 delay”的核心思路是：

> **不要让 CPU 卡在那里等时间过去，而是记录开始时间，然后主循环一直跑，时间到了再执行下一步。**

也就是把：

```c
LED_ON();
delay_ms(1000);
LED_OFF();
delay_ms(1000);
```

改成：

```c
if (时间到了)
{
    LED 翻转一次;
}
```

---

## 1. 为什么死等 delay 不好？

常见阻塞式 delay 是这样：

```c
void delay_ms(uint32_t ms)
{
    while (ms--)
    {
        delay_1ms();
    }
}
```

它的问题是：CPU 在这段时间里什么都干不了。

比如你写：

```c
LED_ON();
delay_ms(1000);
LED_OFF();
delay_ms(1000);
```

这 2 秒内，CPU 基本都在空转。期间：

```c
按键扫描不能及时做；
串口数据可能丢；
传感器状态不能及时处理；
电机保护逻辑可能响应慢；
通信超时判断不方便；
```

所以裸机项目稍微复杂一点，就要尽量少用阻塞式 delay。

---

# 2. 最常用替代方案：SysTick + 时间戳判断

先用一个定时器产生 1ms 中断。

以 Cortex-M MCU 为例，一般用 SysTick。

```c
volatile uint32_t g_ms_tick = 0;

void SysTick_Handler(void)
{
    g_ms_tick++;
}

uint32_t millis(void)
{
    return g_ms_tick;
}
```

初始化 SysTick：

```c
void systick_init(void)
{
    /*
     * SystemCoreClock 是系统主频，比如 72MHz、168MHz 等
     * SystemCoreClock / 1000 表示 1ms 进一次中断
     */
    SysTick_Config(SystemCoreClock / 1000);
}
```

然后主循环里这样写：

```c
int main(void)
{
    systick_init();

    while (1)
    {
        led_task();
        key_task();
        uart_task();
        sensor_task();
        motor_task();
    }
}
```

重点是：每个任务都不能长时间阻塞。

---

# 3. LED 闪烁例子：不用 delay

原来的阻塞写法：

```c
while (1)
{
    LED_ON();
    delay_ms(500);
    LED_OFF();
    delay_ms(500);
}
```

改成非阻塞写法：

```c
void led_task(void)
{
    static uint32_t last_time = 0;

    if ((uint32_t)(millis() - last_time) >= 500)
    {
        last_time = millis();

        LED_TOGGLE();
    }
}
```

这个写法的好处是：LED 每 500ms 翻转一次，但 CPU 不会卡住。

主循环仍然可以继续执行其他任务：

```c
while (1)
{
    led_task();
    key_task();
    uart_task();
    sensor_task();
}
```

---

# 4. 为什么要写成 `millis() - last_time >= 500`？

不要写成：

```c
if (millis() >= last_time + 500)
```

推荐写成：

```c
if ((uint32_t)(millis() - last_time) >= 500)
```

原因是 `g_ms_tick` 迟早会溢出。

比如 32 位变量：

```c
0xFFFFFFFF + 1 = 0
```

用减法判断可以天然兼容溢出。

这是嵌入式里非常常见的写法。

---

# 5. 状态机替代 delay

很多时候 delay 是这样用的：

```c
motor_start();
delay_ms(1000);
motor_stop();
```

这就是典型的阻塞逻辑。

非阻塞写法可以改成状态机。

```c
typedef enum
{
    MOTOR_IDLE = 0,
    MOTOR_RUNNING,
} motor_state_t;

void motor_task(void)
{
    static motor_state_t state = MOTOR_IDLE;
    static uint32_t start_time = 0;

    switch (state)
    {
        case MOTOR_IDLE:
        {
            if (需要启动电机())
            {
                motor_start();
                start_time = millis();
                state = MOTOR_RUNNING;
            }
        }
        break;

        case MOTOR_RUNNING:
        {
            if ((uint32_t)(millis() - start_time) >= 1000)
            {
                motor_stop();
                state = MOTOR_IDLE;
            }
        }
        break;

        default:
        {
            state = MOTOR_IDLE;
        }
        break;
    }
}
```

这样电机启动后不会卡住 1 秒。

这 1 秒期间，CPU 还可以继续处理：

```c
按键；
串口；
显示；
传感器；
保护逻辑；
通信协议；
```

---

# 6. 软件定时器封装

你也可以自己封装一个小型软件定时器。

```c
typedef struct
{
    uint32_t start;
    uint32_t period;
    uint8_t  active;
} soft_timer_t;
```

启动定时器：

```c
void soft_timer_start(soft_timer_t *timer, uint32_t period_ms)
{
    timer->start = millis();
    timer->period = period_ms;
    timer->active = 1;
}
```

判断是否超时：

```c
uint8_t soft_timer_expired(soft_timer_t *timer)
{
    if (timer->active == 0)
    {
        return 0;
    }

    if ((uint32_t)(millis() - timer->start) >= timer->period)
    {
        timer->active = 0;
        return 1;
    }

    return 0;
}
```

使用方法：

```c
soft_timer_t buzzer_timer;

void buzzer_beep_start(void)
{
    BUZZER_ON();
    soft_timer_start(&buzzer_timer, 200);
}

void buzzer_task(void)
{
    if (soft_timer_expired(&buzzer_timer))
    {
        BUZZER_OFF();
    }
}
```

主循环：

```c
while (1)
{
    led_task();
    key_task();
    buzzer_task();
    motor_task();
    uart_task();
}
```

---

# 7. 多任务轮询调度写法

裸机里还可以写一个简单的“协作式调度器”。

```c
typedef struct
{
    void (*task_func)(void);
    uint32_t interval;
    uint32_t last_time;
} task_t;
```

任务表：

```c
task_t task_table[] =
{
    { led_task,    500, 0 },
    { key_task,     10, 0 },
    { sensor_task, 100, 0 },
    { display_task, 50, 0 },
};
```

调度器：

```c
void scheduler_run(void)
{
    uint32_t now = millis();

    for (uint32_t i = 0; i < sizeof(task_table) / sizeof(task_table[0]); i++)
    {
        if ((uint32_t)(now - task_table[i].last_time) >= task_table[i].interval)
        {
            task_table[i].last_time = now;
            task_table[i].task_func();
        }
    }
}
```

主循环：

```c
int main(void)
{
    systick_init();

    while (1)
    {
        scheduler_run();
    }
}
```

这样就变成：

```c
led_task 每 500ms 执行一次；
key_task 每 10ms 执行一次；
sensor_task 每 100ms 执行一次；
display_task 每 50ms 执行一次；
```

这就是裸机里很常见的“时间片轮询”思想。

---

# 8. 中断 + 标志位

有些事情可以在中断里只设置标志位，真正处理放在主循环。

比如定时器 1ms 中断：

```c
volatile uint8_t flag_10ms = 0;
volatile uint8_t flag_100ms = 0;

void SysTick_Handler(void)
{
    static uint16_t cnt_10ms = 0;
    static uint16_t cnt_100ms = 0;

    g_ms_tick++;

    cnt_10ms++;
    cnt_100ms++;

    if (cnt_10ms >= 10)
    {
        cnt_10ms = 0;
        flag_10ms = 1;
    }

    if (cnt_100ms >= 100)
    {
        cnt_100ms = 0;
        flag_100ms = 1;
    }
}
```

主循环：

```c
while (1)
{
    if (flag_10ms)
    {
        flag_10ms = 0;
        key_scan();
    }

    if (flag_100ms)
    {
        flag_100ms = 0;
        sensor_read();
    }

    uart_process();
    motor_process();
}
```

注意：中断里面尽量不要做复杂事情，最好只做：

```c
计数；
置标志；
清中断；
收发少量关键数据；
```

复杂逻辑放到主循环。

---

# 9. 什么时候可以保留 delay？

不是所有 delay 都一定不能用。

下面这些场景可以适当保留短 delay：

## 1. 芯片上电初始化

比如 LCD、传感器、Flash 初始化：

```c
sensor_reset();
delay_ms(10);
sensor_init();
```

这种一般在开机阶段，问题不大。

## 2. 很短的 us 级时序

比如模拟 I2C、模拟 SPI、某些特殊芯片时序：

```c
SCL_H();
delay_us(5);
SCL_L();
delay_us(5);
```

这种为了满足硬件时序，可以用短 delay。

## 3. 简单测试代码

临时验证 LED、GPIO、蜂鸣器时，可以用。

但是正式产品逻辑里，不建议到处写：

```c
delay_ms(100);
delay_ms(500);
delay_ms(1000);
```

尤其是主循环和通信处理中。

---

# 10. 总结一句话

裸机取代阻塞式 delay，主要靠这几种方法：

| 方法            | 适合场景                     |
| ------------- | ------------------------ |
| SysTick 时间戳   | 最常用，适合大部分裸机任务            |
| 状态机           | 替代“执行一步、delay、再执行下一步”的流程 |
| 软件定时器         | 蜂鸣器、LED、超时判断、电机动作        |
| 定时器中断 + 标志位   | 周期任务，比如 10ms 按键扫描        |
| 硬件定时器/PWM/DMA | 精准波形、通信、采样、电机控制          |

最重要的思想是：

```c
不要在原地等；
记录时间；
主循环一直跑；
时间到了再处理。
```

你以后看到这种代码：

```c
do_something();
delay_ms(500);
do_next();
```

脑子里就要自动转换成：

```c
状态机 + 时间戳判断
```

这就是裸机从“简单 demo 写法”走向“产品代码写法”的关键一步。


-------
-----
-----
有的。除了前面说的 **SysTick 时间戳、状态机、软件定时器、定时器标志位**，裸机里还有很多“取代 delay”的方法。核心都是一句话：

> **让硬件、定时器、中断、DMA、事件机制去等，CPU 不要傻等。**

---

# 1. 硬件定时器一次性定时

比如你想：

```c
BUZZER_ON();
delay_ms(200);
BUZZER_OFF();
```

可以改成：

```c
BUZZER_ON();
start_timer_200ms();
```

然后在定时器中断里关蜂鸣器：

```c
void TIMER_IRQHandler(void)
{
    if (timer_update_flag_get())
    {
        timer_update_flag_clear();

        BUZZER_OFF();

        timer_disable();
    }
}
```

这种适合：

```c
蜂鸣器响 200ms；
继电器吸合 500ms；
电机运行 3s；
LED 点亮一段时间；
某个 GPIO 输出脉冲；
```

优点是精度比主循环轮询更高，CPU 不需要一直判断。

---

# 2. 外部中断取代“等待按键”

错误写法：

```c
while (KEY_READ() == 0)
{
    // 一直等按键
}
```

更好的方法是用 EXTI 外部中断。

```c
volatile uint8_t key_pressed = 0;

void EXTI_IRQHandler(void)
{
    if (exti_interrupt_flag_get(KEY_EXTI_LINE))
    {
        exti_interrupt_flag_clear(KEY_EXTI_LINE);

        key_pressed = 1;
    }
}
```

主循环里处理：

```c
while (1)
{
    if (key_pressed)
    {
        key_pressed = 0;
        key_process();
    }

    uart_process();
    sensor_process();
    motor_process();
}
```

这样按键来了才处理，没按键时 CPU 可以干别的。

不过按键有抖动，实际项目里通常还要加 10ms~20ms 消抖。

---

# 3. 串口中断 / 空闲中断取代“等待接收”

错误写法：

```c
while (usart_flag_get(USART0, USART_FLAG_RBNE) == RESET)
{
    // 死等串口数据
}

data = usart_data_receive(USART0);
```

这种会卡住。

更好的方法是串口中断：

```c
volatile uint8_t uart_rx_flag = 0;
uint8_t uart_rx_data;

void USART0_IRQHandler(void)
{
    if (usart_interrupt_flag_get(USART0, USART_INT_FLAG_RBNE))
    {
        uart_rx_data = usart_data_receive(USART0);
        uart_rx_flag = 1;
    }
}
```

主循环：

```c
while (1)
{
    if (uart_rx_flag)
    {
        uart_rx_flag = 0;
        uart_parse_byte(uart_rx_data);
    }

    key_task();
    led_task();
    sensor_task();
}
```

如果是接收一包数据，更推荐：

```c
串口 DMA 接收 + 空闲中断 IDLE
```

也就是你之前接触过的那套：

```c
rx_len = RX_BUF_SIZE - dma_transfer_number_get();
```

这比一个字节一个字节阻塞等强很多。

---

# 4. DMA 取代 CPU 等待搬运数据

比如 SPI 发送一大段数据，错误写法：

```c
for (i = 0; i < len; i++)
{
    while (spi_i2s_flag_get(SPI0, SPI_FLAG_TBE) == RESET);
    spi_i2s_data_transmit(SPI0, buf[i]);
}
```

这个虽然不是 delay，但是本质也是阻塞等待。

更好的做法：

```c
spi_dma_send(buf, len);
```

DMA 发送完成后进中断：

```c
volatile uint8_t spi_tx_done = 0;

void DMA_IRQHandler(void)
{
    if (dma_interrupt_flag_get(DMA_CH, DMA_INT_FLAG_FTF))
    {
        dma_interrupt_flag_clear(DMA_CH, DMA_INT_FLAG_FTF);

        spi_tx_done = 1;
    }
}
```

主循环：

```c
while (1)
{
    if (spi_tx_done)
    {
        spi_tx_done = 0;
        spi_send_complete_process();
    }

    other_task();
}
```

适合这些场景：

```c
SPI 刷屏；
UART 大量发送；
ADC 连续采样；
I2S 音频播放；
定时器 PWM 波形输出；
DShot 波形发送；
```

你之前做 DShot600，其实就是典型的：

```c
定时器 + DMA
```

让硬件自己输出波形，CPU 不要用 delay_us 去翻 GPIO。

---

# 5. 硬件 PWM 取代软件翻转 GPIO

错误写法：

```c
while (1)
{
    GPIO_HIGH();
    delay_us(10);
    GPIO_LOW();
    delay_us(10);
}
```

这种用 delay 模拟 PWM 很差。

正确方式是用定时器 PWM：

```c
timer_pwm_init(freq, duty);
```

然后硬件自动输出。

适合：

```c
电机 PWM；
蜂鸣器 PWM；
LED 调光；
舵机控制；
背光亮度控制；
DShot/红外等部分波形；
```

CPU 只需要修改占空比：

```c
timer_channel_output_pulse_value_config(TIMERx, TIMER_CHx, duty);
```

不需要自己 delay 翻 GPIO。

---

# 6. 输入捕获取代“等待电平变化”

比如测脉宽，有些新手会写：

```c
while (GPIO_READ() == 0);
start = micros();

while (GPIO_READ() == 1);
end = micros();

pulse_width = end - start;
```

这个问题很大：CPU 完全卡住。

更好的方法是定时器输入捕获：

```c
上升沿捕获一次计数值；
下降沿捕获一次计数值；
两者相减得到高电平宽度；
```

适合：

```c
超声波 HC-SR04；
遥控 PWM 输入；
编码器；
频率测量；
脉宽测量；
DShot 回传；
红外解码；
```

也就是：

```c
不要软件 while 等边沿；
让定时器捕获边沿时间。
```

---

# 7. 事件队列

这个比普通 flag 更高级一点。

前面说的 flag 是这样：

```c
volatile uint8_t key_flag;
volatile uint8_t uart_flag;
volatile uint8_t timer_flag;
```

事件多了以后会变乱。可以做一个简单事件队列。

```c
typedef enum
{
    EVENT_NONE = 0,
    EVENT_KEY_DOWN,
    EVENT_UART_RX,
    EVENT_SENSOR_READY,
    EVENT_MOTOR_TIMEOUT,
} event_t;
```

简单队列：

```c
#define EVENT_QUEUE_SIZE 16

volatile event_t event_queue[EVENT_QUEUE_SIZE];
volatile uint8_t event_head = 0;
volatile uint8_t event_tail = 0;
```

投递事件：

```c
void event_post(event_t event)
{
    uint8_t next = (event_head + 1) % EVENT_QUEUE_SIZE;

    if (next != event_tail)
    {
        event_queue[event_head] = event;
        event_head = next;
    }
}
```

取事件：

```c
event_t event_get(void)
{
    event_t event = EVENT_NONE;

    if (event_tail != event_head)
    {
        event = event_queue[event_tail];
        event_tail = (event_tail + 1) % EVENT_QUEUE_SIZE;
    }

    return event;
}
```

中断里只投递事件：

```c
void EXTI_IRQHandler(void)
{
    exti_interrupt_flag_clear(KEY_EXTI_LINE);

    event_post(EVENT_KEY_DOWN);
}
```

主循环统一处理：

```c
while (1)
{
    event_t event = event_get();

    switch (event)
    {
        case EVENT_KEY_DOWN:
            key_process();
            break;

        case EVENT_UART_RX:
            uart_process();
            break;

        case EVENT_SENSOR_READY:
            sensor_process();
            break;

        case EVENT_MOTOR_TIMEOUT:
            motor_stop();
            break;

        default:
            break;
    }

    background_task();
}
```

这种方式很像简化版 RTOS 消息队列。

---

# 8. 回调函数 callback

比如某个驱动启动后，不等待结果，而是等完成后自动调用回调。

阻塞式写法：

```c
sensor_start();
delay_ms(20);
sensor_read();
```

回调式写法：

```c
sensor_start_async(sensor_done_callback);
```

完成后调用：

```c
void sensor_done_callback(void)
{
    sensor_read();
    sensor_data_ready = 1;
}
```

主循环：

```c
while (1)
{
    if (sensor_data_ready)
    {
        sensor_data_ready = 0;
        sensor_data_process();
    }
}
```

适合：

```c
ADC 转换完成；
I2C 读写完成；
SPI DMA 完成；
UART 发送完成；
Flash 擦写完成；
传感器采样完成；
```

很多 HAL 库里的：

```c
xxx_CompleteCallback()
xxx_ErrorCallback()
```

本质就是这个思路。

---

# 9. 协程 / protothread 风格

这是裸机里比较高级的写法，可以让代码看起来像顺序执行，但底层不阻塞。

普通状态机可能是这样：

```c
switch (state)
{
    case STEP_1:
        do_step1();
        start_time = millis();
        state = STEP_2;
        break;

    case STEP_2:
        if (millis() - start_time >= 100)
        {
            do_step2();
            state = STEP_3;
        }
        break;
}
```

协程风格可以写得像这样：

```c
void task_demo(void)
{
    PT_BEGIN();

    do_step1();
    PT_DELAY_MS(100);

    do_step2();
    PT_DELAY_MS(200);

    do_step3();

    PT_END();
}
```

注意，这不是操作系统线程，而是用宏和状态保存实现的“伪协程”。

优点是：

```c
代码看起来顺序清晰；
实际不会卡死 CPU；
适合复杂流程；
```

缺点是：

```c
实现复杂一点；
调试比普通状态机绕；
新手容易误用；
```

所以初学阶段建议先把普通状态机练熟。

---

# 10. WFI / WFE 低功耗等待中断

如果主循环没事干，不一定非要一直空转。

可以这样：

```c
while (1)
{
    scheduler_run();

    __WFI();
}
```

`__WFI()` 的意思是：

```c
Wait For Interrupt
等待中断唤醒
```

这和 delay 不一样。

delay 是：

```c
CPU 一直空转；
```

WFI 是：

```c
CPU 进入低功耗；
定时器中断、串口中断、外部中断来了再醒；
```

适合低功耗产品：

```c
电池设备；
传感器节点；
遥控器；
门锁；
穿戴设备；
```

但是注意：用了 WFI 后，中断配置必须正确，否则系统可能睡下去不醒。

---

# 11. 硬件外设自动完成任务

更高阶的思想是：

> **能让硬件做的，尽量不要 CPU 做。**

比如：

| 需求     | 不推荐             | 推荐                   |
| ------ | --------------- | -------------------- |
| 输出 PWM | GPIO + delay 翻转 | Timer PWM            |
| 采集 ADC | while 等 ADC 完成  | ADC + DMA            |
| 串口接收   | while 等字节       | UART 中断 / DMA        |
| SPI 发送 | while 等 TBE     | SPI DMA              |
| 测脉宽    | while 等 GPIO 电平 | Timer 输入捕获           |
| 定时执行   | delay_ms        | Timer / SysTick      |
| 电机控制   | 软件延时换相          | Timer + PWM + ADC 触发 |
| 波形输出   | delay_us 翻 GPIO | Timer + DMA          |

这就是嵌入式从“软件死等”升级到“硬件驱动”的关键。

---

# 12. 实在复杂，就上 RTOS

虽然你问的是裸机，但实际项目复杂后，FreeRTOS 这种 RTOS 就是专门解决这类问题的。

裸机：

```c
while (1)
{
    task1();
    task2();
    task3();
}
```

RTOS：

```c
LED_Task();
Key_Task();
Uart_Task();
Sensor_Task();
Motor_Task();
```

每个任务里可以用：

```c
vTaskDelay(100);
```

注意，`vTaskDelay()` 不是裸机 delay 那种死等。

它的意思是：

```c
当前任务让出 CPU；
其他任务继续运行；
时间到后再调度回来；
```

所以 RTOS 里的 delay 和裸机死等 delay 不是一个概念。

不过对于小项目，裸机状态机就够了。不要一上来就为了不用 delay 而上 RTOS。

---

# 最实用的选择建议

你可以按场景选：

| 场景       | 推荐方法                  |
| -------- | --------------------- |
| LED 闪烁   | SysTick 时间戳           |
| 按键扫描     | 10ms 定时任务 / EXTI + 消抖 |
| 蜂鸣器响一会儿  | 软件定时器 / 硬件一次性定时       |
| 电机运行几秒   | 状态机 + 软件定时器           |
| 串口接收     | 中断 / DMA + IDLE       |
| SPI 大量发送 | DMA                   |
| ADC 连续采样 | ADC + DMA             |
| PWM 输出   | 硬件 Timer PWM          |
| 测脉宽/频率   | Timer 输入捕获            |
| 复杂流程控制   | 状态机 / 事件队列 / 协程       |
| 低功耗等待    | WFI / RTC / EXTI 唤醒   |
| 多模块复杂项目  | 简单调度器 / RTOS          |

---

# 对你来说，建议优先掌握这 5 个

按照嵌入式裸机开发的实用程度，建议你优先练：

```c
1. SysTick 毫秒计时
2. 状态机
3. 软件定时器
4. 中断 + flag
5. DMA + 完成中断
```

再往后学：

```c
事件队列；
输入捕获；
WFI 低功耗；
协程；
RTOS；
```

真正产品代码里，最常见的组合是：

```c
主循环跑任务
+
SysTick 提供时间基准
+
中断只置标志
+
状态机推进流程
+
DMA 处理大数据搬运
+
硬件定时器输出精确波形
```

也就是说，取代 delay 不是只有一种办法，而是一整套裸机软件架构思想。
