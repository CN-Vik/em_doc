可以把增量式 AB 相编码器理解成：**它不给你“当前角度”，而是不断给你“走了多少格 + 往哪个方向走”。**

所以检测电机**转速**和**转向**，核心就是两件事：

> **A/B 相 → 判断每个脉冲是正向还是反向 → 统计单位时间内走了多少脉冲。**

---

## 1. AB 相到底是什么？

增量编码器通常输出两路方波：

```text
A相：  _|‾‾|_|‾‾|_|‾‾|_
B相：    _|‾‾|_|‾‾|_|‾‾
```

两者相位差 **90°电角度**。

比如正转：

```text
A：  ┌───┐   ┌───┐
     │   │   │   │
─────┘   └───┘   └──

B：    ┌───┐   ┌───┐
       │   │   │   │
───────┘   └───┘   └──
```

此时：

> **A领先B → 一个方向**

反转：

```text
A：    ┌───┐   ┌───┐
       │   │   │   │
───────┘   └───┘   └──

B：  ┌───┐   ┌───┐
     │   │   │   │
─────┘   └───┘   └──
```

变成：

> **B领先A → 另一个方向**

---

# 2. 最简单的转向判断

假设你检测 **A 的上升沿**。

当 A 出现上升沿时，读取 B：

```text
        A上升沿
           ↓
A： ───────┌──────
          ↑
          |
B： ──────?──────
```

规定：

```c
if (A上升沿)
{
    if (B == 0)
        direction = FORWARD;
    else
        direction = REVERSE;
}
```

为什么？

因为：

### 正转：A领先B

```text
A： ___|‾‾‾
       ↑
B： ____|‾‾
       此时 B=0
```

所以：

```text
A上升沿 + B=0
        ↓
     正转
```

而反转：

```text
B： ___|‾‾‾
A： ____|‾‾
       ↑
```

A上升沿的时候：

```text
B = 1
```

所以：

```text
A上升沿 + B=1
        ↓
     反转
```

**注意：具体正/反方向取决于你把 A、B 哪一路定义为正，以及电机/编码器安装方向。实际第一次调试时需要实测确认。**

---

# 3. 转速怎么计算？

这个就更简单了。

假设编码器：

> **1000 PPR**

你采用：

> **AB 四倍频**

那么：

```text
1000 × 4 = 4000 counts/rev
```

也就是说：

```text
电机转一圈
↓
编码器计数器增加/减少 4000
```

---

## 方法一：固定时间数脉冲

这是 MCU 里非常常见的方法。

比如：

```text
每 10ms 计算一次
```

假设这 10ms：

```text
counter = 200
```

那么：

```text
每秒脉冲数：

200 / 0.01
= 20000 count/s
```

然后：

```text
RPM = count_per_second / counts_per_rev × 60
```

所以：

```text
RPM
= 20000 / 4000 × 60
= 300 RPM
```

---

# 4. 代码其实非常简单

假设你已经有硬件 QEP / PCNT / 定时器把 AB 相转换成一个**有符号计数器**：

```c
int32_t encoder_count;
```

例如：

```text
正转：

1000
1001
1002
1003
...
```

反转：

```text
1003
1002
1001
1000
...
```

那么每 10ms：

```c
static int32_t last_count = 0;

void speed_update(void)
{
    int32_t count_now = encoder_count;

    int32_t delta = count_now - last_count;

    last_count = count_now;

    // delta：这10ms走了多少个count
    rpm = (float)delta * 60.0f
          / (COUNTS_PER_REV * 0.01f);
}
```

其中：

```c
COUNTS_PER_REV = PPR * 4;
```

---

# 5. 转向甚至不用单独判断

这是一个非常重要的点。

如果你的编码器计数器本身支持**上下计数**：

```text
正转：

1000 → 1010 → 1020 → 1030

delta > 0
```

那么：

```c
direction = FORWARD;
```

反转：

```text
1030 → 1020 → 1010 → 1000

delta < 0
```

所以：

```c
if (delta > 0)
    direction = FORWARD;
else if (delta < 0)
    direction = REVERSE;
else
    direction = STOP;
```

甚至：

> **转速直接就是带正负号的。**

比如：

```text
+300 RPM
```

表示：

```text
正方向 300 RPM
```

而：

```text
-300 RPM
```

表示：

```text
反方向 300 RPM
```

这对于你现在做的 **FOC 速度环**特别方便。

---

# 6. AB 相四倍频到底是什么意思？

这个你最好理解一下，因为做电机控制经常碰到。

AB 两路：

```text
A： ↑   ↓   ↑   ↓
B： ↑   ↓   ↑   ↓
```

一个完整周期里面：

```text
A上升
A下降
B上升
B下降
```

一共：

```text
4 个边沿
```

所以：

### 1倍频

只数：

```text
A上升沿
```

```text
1 PPR → 1 count
```

### 2倍频

数：

```text
A上升
A下降
```

```text
1 PPR → 2 count
```

### 4倍频

数：

```text
A上升
A下降
B上升
B下降
```

```text
1 PPR → 4 count
```

因此：

```text
编码器 1000 PPR

1倍频 → 1000 count/rev
2倍频 → 2000 count/rev
4倍频 → 4000 count/rev
```

---

# 7. 更专业的做法：硬件 QEP

如果你用的是 STM32 / TI C2000 这类 MCU，**最好不要自己 GPIO 中断判断 AB 相**。

因为编码器高速的时候：

```text
A/B
 ↓
GPIO ISR
 ↓
CPU
 ↓
判断A/B
 ↓
修改count
```

CPU 很容易被打爆。

更专业的方案是：

```text
       A ─────────┐
                  │
              ┌───▼────┐
       B ────►│ QEP    │
              │ 硬件   │
              └───┬────┘
                  │
             Position Counter
                  │
                  ▼
              encoder_count
```

例如 TI C2000 有 **eQEP**，STM32 某些系列有 **TIM Encoder Interface**。

硬件直接完成：

```text
A/B解码
   ↓
判断方向
   ↓
+1 / -1
   ↓
位置计数器
```

CPU只需要：

```c
count = TIMx->CNT;
```

然后计算速度。

---

# 8. 还有一种更适合高速电机的方法：测周期

上面讲的是：

> **固定时间数多少个脉冲**

它在高速时很好用。

但是低速的时候会有一个问题。

比如：

```text
10ms内：

没有脉冲
```

那么：

```text
delta = 0
```

你会认为：

```text
RPM = 0
```

但实际上电机可能：

```text
10 RPM
```

只是 10ms 内还没走完一个 encoder count。

所以低速情况下经常采用：

> **测两个编码器边沿之间的时间。**

例如：

```text
A边沿
  ↓
  ───────────
              ↓
            A边沿
```

测量：

```text
Δt = 两个脉冲之间的时间
```

那么：

```text
f = 1 / Δt
```

再：

```text
RPM = f × 60 / counts_per_rev
```

---

# 9. 实际工程经常两个方法结合

这才是比较实用的方案：

```text
             AB编码器
                 │
          ┌──────┴──────┐
          ↓             ↓
      位置计数         边沿时间
          │             │
          │             │
       高速测速        低速测速
          │             │
          └──────┬──────┘
                 ↓
             RPM估算
```

简单理解：

| 电机状态 | 推荐方法       |
| ---- | ---------- |
| 高速   | 固定周期数脉冲    |
| 中速   | 固定周期数脉冲    |
| 低速   | 测脉冲周期      |
| 极低速  | 测脉冲周期      |
| 静止   | 超时判断 0 RPM |

---

# 10. 放到你的 FOC 里面

你现在做 FOC 的话，整个链路可以直接这么设计：

```text
       增量式编码器
          A   B
          │   │
          ▼   ▼
       MCU Encoder
       Hardware QEP
          │
          ▼
     Position Counter
          │
          ├──────────────┐
          │              │
          ▼              ▼
      机械角度         机械转速
          │              │
          │              │
          ▼              ▼
       × 极对数         Speed PI
          │              │
          ▼              ▼
        θe            Iq_ref
          │              │
          └──────┬───────┘
                 ▼
               FOC
```

其中：

### 机械角度

```text
θm = count / counts_per_rev × 2π
```

### 电角度

如果电机：

```text
P = 极对数
```

那么：

```text
θe = P × θm + offset
```

### 机械转速

```text
RPM = Δcount / Δt / counts_per_rev × 60
```

### 电角速度

```text
ωe = P × ωm
```

这几个东西正好就是你前面做 **有感 FOC** 时需要的：

```text
AB编码器
   ↓
机械位置 θm
   ↓
× 极对数
   ↓
电角度 θe
   ↓
Park
```

以及：

```text
AB编码器
   ↓
Δcount / Δt
   ↓
机械RPM
   ↓
速度环
```

**一句话记住：**

> **AB 相的“相位先后”决定转向；单位时间内的“计数变化量”决定转速；累计计数决定位置。**

而且如果你用的是 STM32，**TIM 的 Encoder Interface 模式基本就是专门干这个的**，不需要自己写 AB 相 GPIO 中断解码。


----------
-------------
对，商家说的 **“7 PPR，一圈 7 个信号”**，通常意思是：

> **编码器轴转一圈，A 相输出 7 个完整脉冲周期，B 相也输出 7 个周期。**

比如：

```text
A相：  __|‾‾|__|‾‾|__|‾‾|__|‾‾|__|‾‾|__|‾‾|__|‾‾|__
          1      2      3      4      5      6      7

B相：    __|‾‾|__|‾‾|__|‾‾|__|‾‾|__|‾‾|__|‾‾|__|‾‾|__
```

所以：

```text
PPR = 7
```

---

## 但是这里有一个非常容易踩坑的地方

**7 PPR ≠ 一圈只有 7 个可计数位置。**

因为 AB 相可以做：

### 1 倍频

只检测 A 相上升沿：

```text
7 × 1 = 7 counts/rev
```

### 2 倍频

检测 A 相上升 + 下降：

```text
7 × 2 = 14 counts/rev
```

### 4 倍频

A、B 两相的所有边沿都计数：

```text
7 × 4 = 28 counts/rev
```

所以你的编码器：

> **7 PPR → 如果 MCU 使用 AB 四倍频，那么一圈就是 28 count。**

---

## 举个非常直观的例子

假设电机转一圈：

```text
       编码器
         ↓
       7 PPR
         ↓
    A、B各7个周期
```

如果 STM32 TIM 使用：

```text
Encoder Mode
TI1 + TI2
```

通常相当于四倍频计数：

```text
7 PPR
 ↓
28 counts/rev
```

那么机械角度：

```text
θm = count / 28 × 360°
```

比如：

```text
count = 7
```

那么：

```text
θm = 7 / 28 × 360°
   = 90°
```

---

## 这对你做 FOC 有一个很大的影响

你的编码器如果真的是 **7 PPR**，分辨率其实非常低。

一圈只有：

```text
28 counts
```

四倍频以后每个 count：

```text
360° / 28
≈ 12.86°机械角
```

如果你的电机是 **7 对极**，那么：

```text
θe = 7 × θm
```

机械转一圈：

```text
7 × 360°
= 2520°电角度
```

也就是：

```text
7个电周期
```

这时候一个 encoder count 对应：

```text
12.86°机械
× 7
≈ 90°电角度
```

也就是说：

> **你的 FOC 电角度每次编码器跳变，可能直接跳约 90°！**

这对于高性能 FOC 来说**分辨率非常低**。

所以你一定要确认商家这个 **7 PPR** 到底指的是：

1. **7 PPR / channel**：A、B各7周期；
2. **7 CPR**：四倍频后总共7个count；
3. 或者商家说的“7个信号”其实是别的定义。

**如果你把商家的编码器具体型号/链接/参数截图发我，我可以直接帮你判断这个“7PPR”到底应该按 7、14 还是 28 count/rev 来算。**



-----------
-----------
可以，但这里有一个**关键点先纠正**：

> **ESP32-C3 没有 PCNT（Pulse Counter）硬件。**
> 所以不能像普通 ESP32 那样直接用 PCNT 做 AB 相正交解码。([GitHub][1])

你之前用的是 **ESP-IDF 5.3.x + ESP32-C3**，那我建议你这样做。

---

# 1. 你的 7PPR AB 编码器

假设商家说：

> **7 PPR，一圈 7 个信号**

按照前面讨论的常见定义：

```text
A：  _|‾|_|‾|_|‾|_|‾|_|‾|_|‾|_|‾|_

B：    _|‾|_|‾|_|‾|_|‾|_|‾|_|‾|_|‾|_
```

A、B 两路相差 90°。

如果你自己实现 **四倍频**：

```text
7 PPR × 4 = 28 count/rev
```

所以：

```text
电机转一圈
      ↓
28 个有效计数
```

---

# 2. ESP32-C3 最简单方案：GPIO 中断 + AB 状态机

因为 C3 没有 PCNT，所以可以：

```text
          编码器
         A     B
         │     │
         ▼     ▼
       GPIO   GPIO
         │     │
         └──┬──┘
            ↓
        GPIO ISR
            ↓
       AB状态判断
            ↓
       encoder_count
```

核心其实就一句：

> **每次 A/B 任意一路发生变化，读取 A+B 当前状态，然后根据“上一个状态 → 当前状态”判断 +1 还是 -1。**

---

# 3. AB 四状态

AB 两个 GPIO 一共只有 4 种状态：

```text
00
01
11
10
```

正转可能是：

```text
00 → 01 → 11 → 10 → 00
```

反转就是：

```text
00 → 10 → 11 → 01 → 00
```

这就是 AB 编码器最核心的东西。

---

# 4. 直接做一个查表

最推荐你在 ESP32-C3 上这么写。

```c
static const int8_t encoder_table[16] =
{
     0, -1, +1,  0,
    +1,  0,  0, -1,
    -1,  0,  0, +1,
     0, +1, -1,  0
};
```

然后：

```c
static volatile int32_t encoder_count = 0;
static volatile uint8_t encoder_state = 0;
```

GPIO ISR：

```c
void IRAM_ATTR encoder_isr(void *arg)
{
    uint8_t A = gpio_get_level(ENC_A_GPIO);
    uint8_t B = gpio_get_level(ENC_B_GPIO);

    uint8_t new_state = (A << 1) | B;

    uint8_t index = (encoder_state << 2) | new_state;

    encoder_count += encoder_table[index];

    encoder_state = new_state;
}
```

初始化时：

```c
encoder_state =
    (gpio_get_level(ENC_A_GPIO) << 1) |
     gpio_get_level(ENC_B_GPIO);
```

然后给：

```text
A
B
```

两个 GPIO 都开启：

```text
上升沿 + 下降沿中断
```

这样：

```text
A ↑
A ↓
B ↑
B ↓
```

全部参与计数。

也就是：

> **4 倍频。**

---

# 5. 转向怎么得到？

这个时候你甚至**不用额外检测转向**。

因为：

```c
encoder_count += +1;
```

代表：

```text
正方向
```

而：

```c
encoder_count += -1;
```

代表：

```text
反方向
```

例如：

```text
正转：

0
1
2
3
4
5
...
```

反转：

```text
5
4
3
2
1
0
...
```

所以：

```c
delta = count_now - count_last;
```

就能直接得到方向。

```c
if (delta > 0)
{
    direction = MOTOR_FORWARD;
}
else if (delta < 0)
{
    direction = MOTOR_REVERSE;
}
else
{
    direction = MOTOR_STOP;
}
```

---

# 6. 转速怎么计算？

你的编码器：

```text
7 PPR
```

四倍频：

```text
CPR = 7 × 4 = 28
```

假设你的 FOC / 速度任务每：

```text
1 ms
```

计算一次速度。

那么：

```c
delta_count = count_now - count_last;
```

然后：

```c
rpm =
    delta_count
    / 28.0f
    / 0.001f
    * 60.0f;
```

也就是：

```text
RPM = Δcount × 60
      ─────────────
       28 × Δt
```

---

# 7. 举个实际例子

假设 1 ms 内：

```text
encoder_count:

100 → 102
```

那么：

```text
Δcount = 2
```

所以：

```text
RPM
= 2 / 28 / 0.001 × 60
≈ 4285.7 RPM
```

如果：

```text
100 → 99
```

那么：

```text
Δcount = -1
```

得到：

```text
RPM ≈ -2142.9 RPM
```

负号直接就是反转。

---

# 8. 但是你的 7PPR 有一个大问题

这个一定要注意。

你的编码器四倍频也只有：

```text
28 count/rev
```

所以：

```text
360° / 28
≈ 12.86°/count
```

也就是说编码器位置非常粗。

例如：

```text
count = 0
     ↓
count = 1
     ↓
机械角度跳 12.86°
```

如果你拿这个直接给 FOC：

```text
encoder
   ↓
机械角度
   ↓
× 7极对
   ↓
电角度
```

那么一个 count 对应：

```text
12.86° × 7
≈ 90°电角度
```

**这对你现在做 FOC 是非常低的分辨率。**

---

# 9. 速度测量还有一个坑

比如你速度只有：

```text
100 RPM
```

电机一圈需要：

```text
600 ms
```

而你的编码器：

```text
28 count/rev
```

所以一个 count 大约：

```text
600 / 28
≈ 21.4 ms
```

如果你每 1ms 算一次：

```text
Δcount = 0
```

你就会得到：

```text
RPM = 0
```

然后突然：

```text
Δcount = 1
```

得到：

```text
RPM = 2142 RPM
```

于是速度会：

```text
0
0
0
2142
0
0
2142
...
```

这显然不能拿去直接做漂亮的速度环。

---

# 10. 所以你这种低 PPR 编码器，我建议这么测速

不要只用：

> **固定时间数脉冲**

而是采用：

```text
                  AB编码器
                     │
              GPIO双边沿中断
                     │
              ┌──────┴──────┐
              ↓             ↓
         encoder_count    edge_time
              │             │
              │             │
              ↓             ↓
          位置变化       脉冲周期
              │             │
              └──────┬──────┘
                     ↓
                  RPM
```

也就是：

### 高速

用：

```text
Δcount / Δt
```

### 低速

用：

```text
相邻编码器边沿之间的时间
```

这样低速也能测得比较平滑。

---

# 11. 如果你只是想做“转速 + 转向”

实际上可以更加简单。

甚至不用四倍频。

只检测：

```text
A 上升沿
```

然后读取：

```text
B
```

例如：

```c
void IRAM_ATTR encoder_isr(void *arg)
{
    int A = gpio_get_level(ENC_A_GPIO);

    if (A)
    {
        int B = gpio_get_level(ENC_B_GPIO);

        if (B == 0)
            encoder_count++;
        else
            encoder_count--;
    }
}
```

这样：

```text
7 PPR
```

就是：

```text
7 count/rev
```

转速：

```text
RPM = Δcount × 60
      ─────────────
       7 × Δt
```

优点是：

> **中断频率只有四倍频方案的 1/4。**

对于你这个 **ESP32-C3 + 7PPR** 的场景，我反而比较推荐先用这个方案把**转速和转向跑通**。

---

# 12. 如果最终是拿来做你的 FOC

我会这样分：

```text
          AB编码器
          7 PPR
             │
       ┌─────┴─────┐
       ↓           ↓
    位置/方向     速度
       │           │
       ↓           ↓
   θm / direction   RPM
       │           │
       ↓           ↓
    × 极对数       Speed PI
       │           │
       ↓           ↓
      θe          Iq_ref
       │           │
       └─────┬─────┘
             ↓
            FOC
```

但**7PPR 对 FOC 的电角度反馈确实太粗**。

如果只是：

> **测转速 + 判断正反转**

完全可以。

如果是：

> **拿它直接作为你现在无感/有感 FOC 的角度传感器**

那我建议换高 PPR/CPR 编码器，或者继续用你之前的 AS5600 / 更高分辨率磁编码器。

另外，官方 ESP-IDF 的 PCNT 本身确实支持用 AB 的 edge + level 配置做正交解码，但这条路在 **ESP32-C3 上不可用**；因此 C3 应该走 GPIO 中断/其他硬件外设实现。([docs.espressif.com][2])

[1]: https://github.com/madhephaestus/ESP32Encoder?utm_source=chatgpt.com "GitHub - madhephaestus/ESP32Encoder: A Quadrature and half quadrature PCNT peripheral driven encoder library supporting 8 encoders · GitHub"
[2]: https://docs.espressif.com/projects/esp-idf/en/v5.0/esp32/api-reference/peripherals/pcnt.html?utm_source=chatgpt.com "Pulse Counter (PCNT) - ESP32 - — ESP-IDF Programming Guide v5.0 documentation"


------------------
-----------
因为 **AB 两相不是只有“脉冲个数”有用，它的每一次跳变都包含位置变化信息**。

所以“四倍频”本质上就是：

> **把 A、B 两路的上升沿和下降沿全部利用起来，让位置分辨率提高 4 倍。**

---

## 先别管四倍频，看一个 AB 周期

假设编码器转动过程中，AB 状态依次变化：

```text
正转：

00 → 01 → 11 → 10 → 00
```

注意！

一次完整周期里，实际上发生了 **4 次状态变化**：

```text
        ①       ②       ③       ④
00  →   01  →   11  →   10  →   00
```

每一次变化，都意味着编码器又往前走了一小段。

所以就有：

```text
1个完整脉冲周期
        ↓
4个可利用的边沿/状态变化
```

---

# 那为什么叫“四倍频”？

假设商家说：

> **7 PPR**

通常表示 A 相一圈有 7 个完整周期：

```text
A：

   ┌───┐     ┌───┐
───┘   └─────┘   └─── ...
    ↑             ↑
   一个周期       一个周期
```

一圈：

```text
7 个完整周期
```

如果你只数 A 相上升沿：

```text
↑       ↑       ↑
1       2       3 ...
```

就是：

```text
7 × 1 = 7 count/rev
```

---

## 二倍频

你把 A 的：

```text
↑ 上升沿
↓ 下降沿
```

都数了：

```text
A：

↑    ↓    ↑    ↓
│    │    │    │
1    2    3    4
```

那么：

```text
7 × 2 = 14 count/rev
```

---

## 四倍频

再把 B 也利用起来：

```text
A： ↑    ↓
B：   ↑    ↓
```

一个周期：

```text
A↑
B↑
A↓
B↓
```

一共 4 个边沿：

```text
7 × 4 = 28 count/rev
```

---

# 关键：不是“凭空制造了4倍分辨率”

这个特别重要。

**四倍频不是让编码器真的多输出了4倍信号。**

而是：

```text
编码器本来给你：

A + B
 ↓
4个边沿/周期

你以前只用了：

1个边沿

现在：

4个边沿全部利用
```

所以 MCU 获取的位置变化信息变多了。

---

# 为什么 AB 两相能做到这个？

因为 AB 两相相差 90°：

```text
正转：

A： ──┌────┐────┌────┐──
      ↑    ↓    ↑    ↓

B： ────┌────┐────┌────┐
        ↑    ↓    ↑
```

所以它们共同构成：

```text
00
 ↓
01
 ↓
11
 ↓
10
 ↓
00
```

你可以把它理解成一个**四格的尺子**：

```text
        一个PPR周期

       ┌───────────┐
       │           │
       ↓           │
      00 → 01 → 11 → 10
       ↑           │
       └───────────┘
```

每走一格：

```text
+1 count
```

因此一个 PPR：

```text
1 PPR
 ↓
4个状态区间
 ↓
4 count
```

---

# 对你这个 7PPR 编码器

如果商家定义：

```text
7 PPR
```

那么：

### 1倍频

```text
7 count/rev
```

### 2倍频

```text
14 count/rev
```

### 4倍频

```text
28 count/rev
```

于是机械角度分辨率：

### 1倍频

```text
360° / 7
≈ 51.4°
```

### 2倍频

```text
360° / 14
≈ 25.7°
```

### 4倍频

```text
360° / 28
≈ 12.86°
```

所以四倍频的意义就是：

> **同一个编码器，把能利用的位置信息尽可能全部利用起来。**

---

## 还有一个非常重要的点

你可能会问：

> **那我为什么不搞八倍频、十六倍频？**

因为 AB 两相本身一个周期就只有这几个离散状态：

```text
00 → 01 → 11 → 10
```

正常正交解码最多就是利用：

```text
A上升
A下降
B上升
B下降
```

也就是：

> **4×**

再往上并不会凭空产生新的真实位置信息。

---

### 最后记一个最简单的图

```text
          AB编码器一个周期

A：  ┌───────┐
────┘       └────
    ↑       ↓
    ①       ③

B：    ┌───────┐
───────┘       └──
      ↑       ↓
      ②       ④


① A上升
② B上升
③ A下降
④ B下降

       ↓

   4 个有效位置变化

       ↓

      4×
```

所以你这个 **7PPR AB编码器**，如果做四倍频，就是：

**7 × 4 = 28 count/rev。**

而且对于你现在的 **ESP32-C3**，四倍频并不是“必须”，只是**想提高位置/测速分辨率时可以这么做**。如果只是测转速和方向，1×完全可以先跑通。


---------------
---------
# ESP32C3 可以借助定时器来检测电机转速吗？？

可以，而且对于你这个 **ESP32-C3 + 7PPR AB 编码器**，**定时器测转速反而是很合适的方案**。

但要区分两个概念：

> **定时器本身不负责解码 AB 相。**
> 定时器负责“计时”，GPIO 中断负责“发现编码器来了一个边沿”。

整体就是：

```text
        AB编码器
        A     B
        │     │
        ▼     ▼
      GPIO中断
         │
         │ 每来一个边沿
         ▼
      读取A/B
         │
         ├──→ 判断方向
         │
         └──→ 读取定时器时间
                    │
                    ▼
              两个边沿的时间差
                    │
                    ▼
                   RPM
```

---

# 1. 最核心的思路

比如你只检测 A 相上升沿。

编码器：

```text
A：____|‾‾‾‾|____|‾‾‾‾|____
        ↑          ↑
       t1         t2
```

ESP32-C3 的定时器一直在计数：

```text
t1 = 100000
t2 = 250000
```

那么：

```text
Δt = t2 - t1
```

假设定时器频率：

```text
1 MHz
```

那么：

```text
Δt = 50000 / 1000000
   = 0.05 s
```

---

# 2. 然后根据 PPR 算 RPM

你的编码器：

```text
7 PPR
```

如果只检测：

```text
A相上升沿
```

那么：

```text
7 个脉冲 = 1圈
```

因此：

```text
RPM = 60 / (Δt × 7)
```

刚才：

```text
Δt = 0.05s
```

所以：

```text
RPM = 60 / (0.05 × 7)
     ≈ 171.4 RPM
```

---

# 3. 转向怎么搞？

这就用到 B 相了。

你在：

```text
A相上升沿
```

发生的时候读取 B。

例如你定义：

```text
A上升 + B=0 → 正转
A上升 + B=1 → 反转
```

那么：

```c
void encoder_isr(void)
{
    uint32_t now = timer_get();

    uint32_t dt = now - last_time;

    last_time = now;

    int B = gpio_get_level(B_GPIO);

    if (B == 0)
    {
        direction = +1;
    }
    else
    {
        direction = -1;
    }

    rpm = 60.0f / (dt * timer_period * 7.0f);

    rpm *= direction;
}
```

最终：

```text
正转 → +RPM
反转 → -RPM
```

---

# 4. 这其实比“固定时间数脉冲”更适合你的 7PPR

你这个编码器只有：

```text
7 PPR
```

非常低。

如果采用：

### 方法A：1ms数脉冲

```text
1ms
 ↓
有没有脉冲？
 ↓
没有
 ↓
RPM = 0
```

低速时会非常抖。

比如：

```text
100 RPM
```

7PPR：

```text
7 × 100 / 60
≈ 11.67 Hz
```

也就是说：

```text
一个脉冲 ≈ 85.7ms
```

你 1ms 采样：

```text
0
0
0
0
...
0
1
0
0
...
```

测速会非常难看。

---

# 5. 用“测周期”就舒服很多

直接测：

```text
A上升沿
   ↓
   ↓
   ↓
   ↓
下一个A上升沿
```

得到：

```text
Δt
```

那么：

```text
转速 = 60 / (PPR × Δt)
```

比如：

```text
Δt = 85.7ms
PPR = 7
```

那么：

```text
RPM ≈ 100
```

所以：

> **低 PPR 编码器，测脉冲周期通常比固定时间数脉冲更适合低速测速。**

---

# 6. ESP32-C3 可以怎么实现？

你可以使用 ESP-IDF 的：

```text
GPTimer
```

让定时器一直跑。

例如：

```text
GPTimer
频率 = 1 MHz
```

那么：

```text
1 tick = 1 μs
```

然后 GPIO 中断发生：

```c
uint64_t now;
```

读取 GPTimer 当前计数值。

然后：

```c
dt_us = now - last_time;
```

RPM：

```c
rpm = 60,000,000.0f / (dt_us * 7.0f);
```

因为：

```text
60秒 × 1,000,000 μs
```

所以：

```text
RPM = 60,000,000 / (dt_us × PPR)
```

---

# 7. 甚至可以做到“四倍频测速”

如果你希望利用 AB 四倍频：

```text
A↑
A↓
B↑
B↓
```

每一个边沿都触发 GPIO 中断。

那么：

```text
7 PPR × 4
=
28 count/rev
```

两个相邻 count：

```text
count1
   ↓
   Δt
   ↓
count2
```

于是：

```text
RPM = 60,000,000 / (Δt_us × 28)
```

这样分辨率更高。

但是有个问题：

> **你的编码器只有 7PPR，所以即使四倍频，也只有 28 count/rev。**

---

# 8. 我更推荐你的实现方式

结合你现在这个项目：

```text
ESP32-C3
+
7PPR AB编码器
+
FOC
```

我建议：

```text
             AB编码器
             A     B
             │     │
             ▼     ▼
          GPIO ISR
             │
       ┌─────┴─────┐
       ↓           ↓
    A/B状态      GPTimer
       │           │
       ↓           ↓
      方向       Δ时间
       │           │
       └─────┬─────┘
             ↓
            RPM
             │
             ↓
          速度滤波
             │
             ↓
          Speed PI
```

其中：

### 方向

AB 相判断：

```text
A领先B → +
B领先A → -
```

### 转速

测相邻边沿：

```text
RPM = 60 / (N × Δt)
```

### 低速

用：

```text
周期测速
```

### 高速

可以切换成：

```text
固定时间窗口计数
```

---

## 最后一个很重要的工程问题

**不要把 GPIO ISR 里面的 RPM 计算、浮点运算、PID 全部做掉。**

ISR 最好只干：

```text
① 读取 A/B
② 判断方向
③ 读取 timer
④ 保存 dt
```

然后你的 **1kHz 速度任务**再做：

```text
dt
 ↓
RPM
 ↓
低通滤波
 ↓
Speed PI
```

这样特别适合你现在的 FOC 架构。

如果你愿意，我可以直接给你写一套 **ESP-IDF 5.3 + ESP32-C3 的“AB 7PPR + GPTimer + GPIO ISR + RPM + 正反转”完整 C 代码**，可以直接塞进你的工程里。


可以。下面给你一套**ESP-IDF 5.3 + ESP32-C3 + 7PPR AB 增量编码器**的实用版本。

我建议先用 **1×测速**：只在 A 相上升沿触发，B 相负责判断方向。这样 ISR 很轻，而且对于你这个 7PPR 编码器已经足够把功能跑通。

### 接线

```text
编码器
 ├── A ── GPIO4
 ├── B ── GPIO5
 ├── VCC
 └── GND
```

假设：

```text
PPR = 7
```

所以：

```text
电机转一圈
↓
A相产生7个上升沿
↓
7 count
```

---

## 完整代码

```c
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "driver/gptimer.h"
#include "esp_attr.h"
#include "esp_log.h"


/*=========================================================
 * Encoder Configuration
 *=========================================================*/

#define ENC_A_GPIO              GPIO_NUM_4
#define ENC_B_GPIO              GPIO_NUM_5

#define ENCODER_PPR             7

/* GPTimer frequency:
 * 1 MHz -> 1 tick = 1 us
 */
#define TIMER_FREQ_HZ           1000000


/*=========================================================
 * Global Variables
 *=========================================================*/

static const char *TAG = "ENCODER";

static gptimer_handle_t encoder_timer = NULL;

/*
 * ISR 和任务之间共享的数据
 */
static volatile uint64_t encoder_last_time_us = 0;
static volatile uint32_t encoder_period_us = 0;

static volatile int8_t encoder_direction = 0;

/*
 * 新脉冲到来的标志
 */
static volatile bool encoder_new_pulse = false;


/*=========================================================
 * GPIO ISR
 *=========================================================*/

static void IRAM_ATTR encoder_a_isr_handler(void *arg)
{
    /*
     * 读取当前GPTimer计数值
     */
    uint64_t now_us = 0;

    gptimer_get_raw_count(
        encoder_timer,
        &now_us
    );

    /*
     * 第一次脉冲没有周期数据
     */
    if (encoder_last_time_us != 0)
    {
        uint64_t dt = now_us - encoder_last_time_us;

        /*
         * 防止异常值
         */
        if (dt > 0 && dt < 100000000)
        {
            encoder_period_us = (uint32_t)dt;
            encoder_new_pulse = true;
        }
    }

    encoder_last_time_us = now_us;


    /*
     * A上升沿的时候读取B判断方向
     *
     * 注意：
     * 正反方向取决于你的编码器接线以及机械安装方向。
     */
    int b_level = gpio_get_level(ENC_B_GPIO);

    if (b_level == 0)
    {
        encoder_direction = +1;
    }
    else
    {
        encoder_direction = -1;
    }
}


/*=========================================================
 * GPTimer Init
 *=========================================================*/

static void encoder_timer_init(void)
{
    gptimer_config_t timer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,

        /*
         * 1 MHz
         */
        .resolution_hz = TIMER_FREQ_HZ,

        .direction = GPTIMER_COUNT_UP,

        .flags = {
            .intr_shared = false,
        },
    };

    ESP_ERROR_CHECK(
        gptimer_new_timer(
            &timer_config,
            &encoder_timer
        )
    );

    ESP_ERROR_CHECK(
        gptimer_enable(encoder_timer)
    );

    ESP_ERROR_CHECK(
        gptimer_start(encoder_timer)
    );
}


/*=========================================================
 * Encoder GPIO Init
 *=========================================================*/

static void encoder_gpio_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask =
            (1ULL << ENC_A_GPIO) |
            (1ULL << ENC_B_GPIO),

        .mode = GPIO_MODE_INPUT,

        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,

        /*
         * A：
         * 只检测上升沿
         *
         * B：
         * 不需要中断
         */
        .intr_type = GPIO_INTR_DISABLE,
    };

    ESP_ERROR_CHECK(
        gpio_config(&io_conf)
    );


    /*
     * A相上升沿中断
     */
    ESP_ERROR_CHECK(
        gpio_set_intr_type(
            ENC_A_GPIO,
            GPIO_INTR_POSEDGE
        )
    );


    ESP_ERROR_CHECK(
        gpio_install_isr_service(ESP_INTR_FLAG_IRAM)
    );


    ESP_ERROR_CHECK(
        gpio_isr_handler_add(
            ENC_A_GPIO,
            encoder_a_isr_handler,
            NULL
        )
    );
}


/*=========================================================
 * Calculate RPM
 *=========================================================*/

static float encoder_get_rpm(void)
{
    uint32_t period_us;
    int8_t direction;

    /*
     * 简单读取共享变量
     */
    period_us = encoder_period_us;
    direction = encoder_direction;

    if (period_us == 0)
    {
        return 0.0f;
    }

    /*
     * RPM = 60 / (period × PPR)
     *
     * period单位：us
     *
     * 所以：
     *
     * RPM =
     * 60,000,000
     * ----------------
     * period_us × PPR
     */

    float rpm =
        60000000.0f /
        ((float)period_us * ENCODER_PPR);

    rpm *= direction;

    return rpm;
}


/*=========================================================
 * Speed Task
 *=========================================================*/

static void encoder_speed_task(void *arg)
{
    while (1)
    {
        /*
         * 1kHz速度任务
         */
        if (encoder_new_pulse)
        {
            encoder_new_pulse = false;

            float rpm = encoder_get_rpm();

            ESP_LOGI(
                TAG,
                "RPM = %.2f, DIR = %d, period = %lu us",
                rpm,
                encoder_direction,
                (unsigned long)encoder_period_us
            );
        }

        /*
         * 1ms
         */
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}


/*=========================================================
 * Encoder Init
 *=========================================================*/

void encoder_init(void)
{
    encoder_timer_init();

    encoder_gpio_init();

    xTaskCreate(
        encoder_speed_task,
        "encoder_speed",
        2048,
        NULL,
        5,
        NULL
    );
}
```

然后在：

```c
app_main()
```

里面：

```c
void app_main(void)
{
    encoder_init();

    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

---

# 但是这里有个很重要的问题

上面这个版本：

```text
A ↑
 ↓
读取B
 ↓
判断方向
```

是 **1× 解码**。

所以：

```text
7 PPR
↓
7 count/rev
```

例如：

```text
100 RPM
```

那么：

```text
脉冲频率

7 × 100 / 60
≈ 11.67 Hz
```

所以：

```text
一个A上升沿
间隔 ≈ 85.7 ms
```

程序最终会看到类似：

```text
RPM = 99.8
RPM = 100.2
RPM = 99.5
RPM = 100.4
```

这就能拿来做基本速度检测。

---

# 但还需要补一个“停止检测”

上面代码有个问题：

如果电机突然停止：

```text
最后一个脉冲
    ↓
没有下一个脉冲
```

那么：

```c
encoder_period_us
```

会一直保持上一次的值。

于是程序可能继续认为：

```text
RPM = 100
```

所以实际工程必须加：

```text
超时判断
```

例如：

```c
uint64_t now;
gptimer_get_raw_count(encoder_timer, &now);

if ((now - encoder_last_time_us) > 200000)
{
    rpm = 0;
}
```

这里：

```text
200000 us = 200 ms
```

如果 200ms 都没有新的编码器脉冲：

```text
认为电机已经停止
```

---

# 再往你的 FOC 里面接

你的架构可以变成：

```text
                    AB编码器
                     A   B
                     │   │
                     ▼   ▼
                 GPIO ISR
                     │
          ┌──────────┴─────────┐
          │                    │
       GPTimer              B电平
          │                    │
          ▼                    ▼
       Δt(us)                方向
          │                    │
          └──────────┬─────────┘
                     ▼
                    RPM
                     │
                     ▼
                 低通滤波
                     │
                     ▼
                 Speed PI
                     │
                     ▼
                   Iq_ref
```

---

## 不过，我建议你下一步直接升级成这个版本

你现在毕竟是在做 **FOC**，所以最好不要停在“只有 RPM”。

最终建议做成一个：

```c
typedef struct
{
    int32_t count;

    int8_t direction;

    uint32_t period_us;

    float rpm;

    float mechanical_angle;

} encoder_t;
```

然后同时输出：

```text
count
  ↓
机械角 θm

AB相
  ↓
方向

GPTimer
  ↓
RPM
```

这样你就可以直接得到：

```c
encoder.count
encoder.direction
encoder.rpm
encoder.mechanical_angle
```

甚至进一步：

```text
θm
 ↓
× 7极对
 ↓
θe
 ↓
FOC Park变换
```

**不过你这个“7PPR + 7对极”的组合有一个非常大的坑：四倍频后电角度每跳一次仍然约 90°。**
所以如果你是准备把这个 AB 编码器直接拿来做你现在的 **FOC 电角度反馈**，我建议我们下一步重点把这个问题解决掉。
