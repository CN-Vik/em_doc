STM32 的 **Timer 输入捕获（Input Capture）** 本质上就是：

> **利用定时器的高速计数器，在外部信号边沿到来的瞬间，自动把当前计数值保存下来，用两个或多个时间点计算信号的时间特征。**

它主要用来测：

* PWM 输入
* 脉冲宽度
* 频率
* 周期
* 编码器脉冲
* DShot 接收等

---

## 1. 先理解 Timer 本身

Timer 内部有一个不断递增的计数器：

例如：

```
TIMx_CNT

0
1
2
3
4
5
6
...
```

假设：

```
Timer 时钟 = 72MHz
```

那么：

```
一个计数周期：

1 / 72MHz = 13.8ns
```

也就是说：

每过 13.8ns：

```
CNT += 1
```

---

## 2. 输入捕获发生了什么？

假设外部输入：

```
GPIO

      ┌───────┐       ┌───────┐
      │       │       │       │
______│       │_______│       │______

      ↑       ↑       ↑
      |       |       |
     上升    下降    上升
```

你配置：

> 检测上升沿

当第一个上升沿来了：

```
GPIO ↑
```

硬件自动做：

```
TIM_CNT 当前值
        |
        v

Capture Register 保存
```

例如：

```
CNT = 10000


CCR1 = 10000
```

CPU 不需要参与。

---

第二个上升沿：

```
GPIO ↑
```

此时：

```
CNT = 25000
```

硬件：

```
CCR1 = 25000
```

然后你计算：

```
周期 = 第二次捕获值 - 第一次捕获值

=25000-10000

=15000 tick
```

换算：

```
时间 = 15000 × 13.8ns

=207us
```

那么频率：

```
f=1/T

=1/207us

≈4.83kHz
```

---

# 3. 输入捕获硬件结构

大概是：

```
GPIO
 |
 |
输入滤波
 |
边沿检测
 |
触发
 |
 +----------------+
 |
TIM Counter
 |
 +----------------+
 |
Capture寄存器(CCR)
 |
中断/DMA
```

重点：

**捕获动作完全硬件完成。**

不是：

```
GPIO中断
 |
CPU读取时间
```

这种。

因为 CPU 中断延迟：

几十 ns ~ 几 us

会产生误差。

---

# 4. 测 PWM 怎么测？

比如：

PWM:

```
      ┌───────┐
______│       │______

<----周期---->

<--高电平-->
```

需要两个时间：

### 周期

两个上升沿：

```
t2 - t1
```

得到：

```
Period
```

### 高电平时间

上升沿：

```
t1
```

下降沿：

```
t3
```

计算：

```
High time=t3-t1
```

所以：

```
占空比=

High time / Period
```

例如：

```
周期=1000us

高电平=250us


Duty=25%
```

---

# 5. STM32里面一个通道怎么同时测高电平？

STM32 有一个很巧妙的功能：

## PWM Input 模式

例如：

TIM3_CH1 输入：

```
PWM

     ┌─────┐
_____│     │____
     ↑     ↓
```

内部自动：

CH1：

捕获上升沿

```
CCR1 = 周期
```

CH2：

捕获下降沿

```
CCR2 = 高电平时间
```

硬件连接：

```
PA6
 |
 |
TIM3_CH1

内部:

CH1 捕获上升

CH2 捕获下降
```

然后：

```
周期=CCR1

高电平=CCR2
```

CPU只读取。

---

# 6. 用于 DShot 接收是什么原理？

DShot 本质：

高速 PWM 编码。

例如 DShot600：

bit 时间：

```
1.67us
```

0：

```
高电平 0.625us

低电平 1.04us
```

1：

```
高电平 1.25us

低电平 0.42us
```

波形：

```
bit0:

 ┌───┐
 │   │
_│   │_____

625ns


bit1:

 ┌──────┐
 │      │
_│      │__

1.25us
```

输入捕获：

记录：

```
上升沿时间

下降沿时间
```

得到：

```
High_time
```

判断：

```
High_time > 900ns

认为1

否则0
```

例如：

```
捕获:

rise CCR=10000

fall CCR=10600


high=600tick
```

判断 bit0。

---

# 7. 输入捕获 vs 外部中断

区别：

## GPIO中断

```
边沿
 |
CPU中断
 |
读取系统时间
```

缺点：

* 延迟不固定
* CPU负担大
* 高频不行

---

## Timer输入捕获

```
边沿
 |
硬件锁存CNT
 |
CPU慢慢读取
```

优点：

* 纳秒级精度
* 几乎无CPU开销
* 可以DMA

所以：

高速协议：

* DShot
* 编码器
* PWM

基本都是 Timer Capture。

---

# 8. 和你之前研究 DShot600 最相关的理解

你想用 STM32F103：

TIM1/TIM2/TIM3 输入捕获。

典型流程：

```
TIM输入捕获
       |
       |
检测下降沿
       |
读取CCR
       |
计算高电平时间
       |
判断0/1
       |
组成16bit
       |
CRC校验
       |
得到油门值
```

如果进一步优化：

```
Timer Capture + DMA

↓

DMA自动搬CCR

↓

CPU一次解析16个bit
```

这就是很多 ESC 接收 DShot 的方式。

所以一句话总结：

> **Timer 输入捕获就是让定时器在外部信号边沿发生的瞬间，自动保存当前计数值，用时间差计算脉宽、周期、频率。它把“测时间”这件事从软件变成了硬件。**


--------------
--------------
----------------

这个要看你用哪种方式。

## 1. 普通输入捕获（一个边沿一个通道）

例如你只测周期：

PWM：

```
      ┌──────┐      ┌──────┐
______│      │______│      │______

      ↑             ↑
      上升沿        上升沿
```

只需要：

```
PWM信号 ----> TIMx_CH1
```

**1个IO**

每次捕获上升沿：

```
CCR1 = 当前CNT
```

两次差值：

```
周期 = CCR1_2 - CCR1_1
```

可以测频率。

---

## 2. 测PWM高电平时间（普通双通道方式）

例如：

```
      ┌────────┐
______│        │________
      ↑        ↓
      上升     下降
```

你需要同时捕获：

* 上升沿时间
* 下降沿时间

传统方法：

```
PWM

 |
 +---- TIM_CH1
 |
 +---- TIM_CH2
```

两个输入：

```
IO1 -> TIM_CH1 (上升沿)
IO2 -> TIM_CH2 (下降沿)
```

需要：

**2个IO**

计算：

```
高电平时间 = 下降捕获 - 上升捕获
```

---

## 3. STM32 的 PWM Input 模式（推荐）

STM32 有一个特殊功能：

> 一个IO，同时完成周期和高电平捕获。

比如 STM32F103：

```
PA6

 |
 |
TIM3_CH1
```

配置：

```
TIM3 PWM Input Mode
```

内部自动：

```
PA6输入

        ┌─────────┐
        │         │
        ↓         ↓

TIM3_CH1       TIM3_CH2
捕获上升       捕获下降

CCR1           CCR2

周期           高电平
```

注意：

**外部只接一个IO。**

也就是说：

```
PWM信号

    |
    |
   PA6


只需要1根信号线
```

---

## 4. 对 DShot600 接收呢？

DShot 和普通 PWM 不一样。

DShot：

```
bit1:

     ┌───────┐
_____│       │_____

bit0:

     ┌───┐
_____│   │_______
```

你主要关心：

```
高电平持续时间
```

所以一般：

### 方法1：一个IO + 输入捕获

```
DShot
 |
 |
TIM_CHx
```

捕获：

```
上升沿 CCR
下降沿 CCR
```

然后：

```
high_time = fall - rise
```

需要：

**1个IO**

---

### 方法2：输入捕获 + DMA

更高级：

```
DShot IO

 |
TIM Capture

 |
DMA

 |
RAM数组
```

DMA自动保存：

```
10000
10450
11200
...
```

CPU一次解析。

仍然：

**1个IO**

---

## 5. 以你的 STM32F103C8T6 为例

LQFP48：

TIM1:

```
PA8  CH1
PA9  CH2
PA10 CH3
```

TIM2:

```
PA0 CH1
PA1 CH2
PA2 CH3
PA3 CH4
```

TIM3:

```
PA6 CH1
PA7 CH2
PB0 CH3
PB1 CH4
```

例如 DShot600：

```
ESC信号线

    |
    |
   PA6

TIM3_CH1 输入捕获
```

一个IO即可。

---

总结：

| 用途                   | 需要IO |
| -------------------- | ---: |
| 测频率                  |   1个 |
| 测PWM周期               |   1个 |
| 测PWM占空比（PWM Input模式） |   1个 |
| 双通道普通捕获              |   2个 |
| DShot600接收           |   1个 |
| DShot600 + DMA       |   1个 |

你前面研究的 **STM32F103C8T6 接收 DShot600**，最合适的方案就是：

**TIM 输入捕获 + DMA，一个IO，捕获所有边沿时间，然后软件解析16bit。** 这也是很多开源 ESC 的做法。


------------
-------------
----------

使用 MCU 的 Timer 来解析 DShot600，本质上就是：

> **测量每个 bit 的高电平时间，然后判断它是 0 还是 1。**

DShot600 实际上就是一种**固定周期 PWM 编码协议**。

## 1. DShot600 时序

DShot600：

速度：

$$
600000bit/s
$$

所以：

$$
T_{bit}=1/600k
$$

得到：

$$
T_{bit}=1.667us
$$

每个bit总长度固定：

```text
1.667us
```

区别只在高电平宽度。

| bit | 高电平     | 低电平     |
| --- | ------- | ------- |
| 0   | 0.625us | 1.042us |
| 1   | 1.25us  | 0.417us |

大约：

```text
0 = 37.5%

1 = 75%
```

这就是 DShot。

并不是传统 PWM。

而是：

> 固定周期，不同占空比编码数据

类似：

```text
111001010...

↓

■■■■□□□□
■■■■□□□□
■■□□□□□□
■■□□□□□□
■■■■□□□□
...
```

16bit 一帧。 ([NuttX][1])

---

# 2. MCU 如何接收

最简单方案：

## Timer Input Capture

捕获：

```text
上升沿
下降沿
```

记录 CNT 值。

计算：

```text
Ton = falling-rising

Tbit = next_rising-current_rising
```

然后：

```c
duty = Ton/Tbit
```

判断：

```c
if(duty>0.55)
    bit=1;
else
    bit=0;
```

结束。

---

# 3. 更简单的方法

实际上 DShot600：

bit长度固定：

```text
1.667us
```

所以根本不用测周期。

只测：

```text
高电平时间
```

即可。

```c
Ton<0.9us

→ 0

Ton>0.9us

→ 1
```

阈值取：

```text
(0.625+1.25)/2

≈0.94us
```

即可。

---

# 4. STM32 推荐配置

例如：

TIM2

84MHz

PSC=0

计数精度：

```text
11.9ns
```

那么：

```text
0.625us

≈52counts

1.25us

≈105counts
```

完全够。

---

配置：

CH1：

```text
Input Capture Rising
```

CH2：

```text
Input Capture Falling
```

或者：

```text
PWM Input Mode
```

STM32 自带。

它会自动：

```text
CCR1 = period

CCR2 = high_time
```

直接得到：

```c
Ton=CCR2;

T=CCR1;

duty=(float)Ton/T;
```

这是 STM32 官方推荐方法。 ([Stm32World][2])

---

# 5. 边沿捕获模式（最常见）

很多 ESC 固件都是这么干。

例如 AM32。

DMA 保存所有边沿时间戳。

然后软件解码。 ([DeepWiki][3])

假设：

```text
R F R F R F

↑ ↑ ↑ ↑ ↑ ↑

100
152
240
292
380
485
```

代表：

```text
Ton1=152-100=52

Ton2=292-240=52

Ton3=485-380=105
```

对应：

```text
52

52

105
```

得到：

```text
0

0

1
```

---

# 6. DMA方案（推荐）

DShot600：

```text
1.67us/bit
```

16bit：

```text
26.7us/frame
```

CPU中断每个边沿：

```text
32次IRQ

26us
```

很容易爆。

所以推荐：

```text
TIM IC + DMA
```

配置：

```text
Capture both edges

DMA->buffer[64]
```

自动记录：

```text
t0
t1
t2
t3
...
```

DMA完成：

```c
for(i=0;i<16;i++)
{
   Ton=t[2*i+1]-t[2*i];

   if(Ton>threshold)
       bit=1;
   else
       bit=0;
}
```

得到：

```text
16bit frame
```

---

# 7. DShot数据格式

DShot：

```text
BBBBBBBBBBB T CCCC

11bit throttle

1bit telemetry

4bit crc
```

例如：

```text
10101010101
1
0110
```

解析：

```c
uint16_t frame;

throttle=(frame>>5)&0x7FF;

telemetry=(frame>>4)&1;

crc=frame&0xF;
```

---

CRC：

```c
crc =
(value ^
(value>>4) ^
(value>>8))
&0x0F;
```

验证：

```c
if(crc_calc==crc_rx)
{
   valid=1;
}
```

---

# 8. 最推荐的实现

对于 STM32G431、STM32F4、ESP32-S3：

### 方案A

```text
TIM PWM Input

CCR1=Period

CCR2=HighTime

IRQ解析
```

优点：

简单。

缺点：

一次只能测一个PWM。

---

### 方案B（最佳）

```text
TIM Input Capture

Both Edge

DMA

buffer[64]

↓

软件解码

↓

16bit frame

↓

CRC校验
```

这是目前很多 ESC（如 AM32、Bluejay）的接收思路。 ([DeepWiki][3])

---

# 9. STM32 代码框架

```c
volatile uint16_t cap[64];

HAL_TIM_IC_Start_DMA(
    &htim2,
    TIM_CHANNEL_1,
    cap,
    32
);
```

DMA完成：

```c
void decode()
{
    uint16_t frame=0;

    for(int i=0;i<16;i++)
    {
        uint16_t Ton=
        cap[2*i+1]-cap[2*i];

        frame<<=1;

        if(Ton>80)
            frame|=1;
    }

    check_crc(frame);
}
```

---

其实 DShot 接收可以总结成一句话：

> **DShot600 = 用 Timer 输入捕获测高电平宽度，再根据 0.625us 和 1.25us 判断 0/1，拼出16bit数据，最后做CRC校验。**

这个方案比传统 UART 解码更稳，也比 EXTI 中断可靠得多，是 MCU 接收 DShot600 的标准做法。

[1]: https://nuttx.apache.org/docs/latest/components/drivers/character/timers/dshot.html?utm_source=chatgpt.com "DShot Drivers — NuttX latest documentation"
[2]: https://stm32world.com/wiki/STM32_Timer_PWM_Input_Capture?utm_source=chatgpt.com "STM32 Timer PWM Input Capture - Stm32World Wiki"
[3]: https://deepwiki.com/am32-firmware/AM32/4-communication-protocols?utm_source=chatgpt.com "Communication Protocols | am32-firmware/AM32 | DeepWiki"
