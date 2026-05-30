SVPWM 划分成 **6 段，每段 60°**，不是人为随便分的，而是因为 **三相逆变器本身只能产生 6 个有效电压矢量**，这 6 个矢量在空间上刚好围成一个六边形，相邻两个矢量夹角就是 **60°**。

---

## 1. 三相桥臂一共有 8 种开关状态

三相逆变器有 A、B、C 三个桥臂，每个桥臂只有两种状态：

```c
1 = 上桥臂导通，下桥臂关断
0 = 下桥臂导通，上桥臂关断
```

所以总状态数是：

```text
2^3 = 8 种
```

也就是：

```text
000
001
010
011
100
101
110
111
```

其中：

```text
000 和 111 是零矢量
```

因为三相都接到同一个电平上，电机三相之间没有有效线电压，所以不产生旋转磁场。

剩下 6 个是有效矢量：

```text
100
110
010
011
001
101
```

它们就是 SVPWM 里面的 6 个基本有效电压矢量。

---

## 2. 这 6 个有效矢量天然相差 60°

把这 6 个开关状态转换到 α-β 坐标系后，就会变成这样的空间电压矢量：

```text
        V2(110)
          /\
         /  \
 V3(010)    V1(100)
        \    /
         \  /
 V4(011) \/ V6(101)
        V5(001)
```

更标准一点看，就是一个六边形：

```text
              V2
             /  \
          V3      V1
          |        |
          V4      V6
             \  /
              V5
```

相邻两个有效矢量之间的夹角都是：

```text
360° / 6 = 60°
```

所以 SVPWM 就自然被划分成了 6 个扇区。

---

## 3. 为啥要分扇区？

因为电机控制算法算出来的是一个目标电压矢量，比如：

```text
Vref
```

这个 Vref 是连续旋转的，但是逆变器不能输出任意方向的电压，它只能输出这 6 个固定方向的有效矢量，再加上 2 个零矢量。

所以 SVPWM 的核心思想就是：

```text
用相邻的两个有效矢量 + 零矢量
在一个 PWM 周期内合成目标矢量 Vref
```

比如 Vref 落在第 1 扇区：

```text
第 1 扇区：V1 和 V2 之间
```

那么就用：

```text
V1 作用 T1 时间
V2 作用 T2 时间
V0/V7 作用 T0 时间
```

合成出目标电压：

```text
Vref ≈ V1 * T1 + V2 * T2 + V0 * T0
```

这里的 V0/V7 是零矢量，只是用来填满一个 PWM 周期。

---

## 4. 每个 60° 扇区里面只需要两个相邻矢量

比如目标矢量在这里：

```text
        V2
       /
      /   Vref
     /
    /
   V1
```

那你只需要 V1 和 V2 就能合成它。

如果目标矢量跑到了下一个区域：

```text
V2 和 V3 之间
```

那就改用：

```text
V2 + V3 + 零矢量
```

所以必须知道 Vref 当前在哪个扇区，才能知道应该用哪两个基本矢量来合成。

---

## 5. 可以这么理解

SPWM 像是：

```text
三相分别生成正弦 PWM
```

SVPWM 像是：

```text
从电压空间矢量角度看整个三相逆变器
```

三相逆变器实际能产生的非零电压矢量只有 6 个，所以空间自然被分成 6 个区域。

---

## 6. 为啥不是 3 段、12 段、360 段？

因为三相逆变器的有效开关状态只有 6 个。

```text
6 个有效状态 → 6 个有效矢量 → 六边形 → 6 个扇区
```

每个扇区的边界就是相邻两个有效矢量。

所以不是软件想分 6 段，而是硬件决定了它天然就是 6 段。

---

## 7. 一句话总结

**SVPWM 划分成 6 个 60° 扇区，是因为三相逆变器有 6 个非零有效开关状态，这 6 个状态在 α-β 平面中形成 6 个相隔 60° 的电压矢量。目标电压矢量落在哪个扇区，就用该扇区相邻的两个有效矢量加零矢量来合成。**


-----
-----
----
图里红圈的 **`(100)`、`(110)`、`(010)`**，表示的是三相逆变器 **A、B、C 三个桥臂的开关状态**。

通常写成：

```text
(Sa Sb Sc)
```

也就是：

```text
(A相桥臂状态, B相桥臂状态, C相桥臂状态)
```

---

## 1. `1` 和 `0` 分别是什么意思？

对于一个两电平三相逆变器，每一相有上下两个 MOS 管 / IGBT：

```text
        +Vdc
         |
       上管
         |
A/B/C 相输出点
         |
       下管
         |
        GND / -Vdc
```

一般约定：

```text
1 = 上桥臂导通，下桥臂关断，该相接到母线正端
0 = 下桥臂导通，上桥臂关断，该相接到母线负端
```

所以：

```text
(100)
```

意思是：

```text
A相 = 1，上管导通，A相接正母线
B相 = 0，下管导通，B相接负母线
C相 = 0，下管导通，C相接负母线
```

也就是：

```text
A 高，B 低，C 低
```

---

## 2. 图里的几个例子

### `U0(100)`

```text
A = 1
B = 0
C = 0
```

表示：

```text
A相接 +Vdc
B相接 0 / -Vdc
C相接 0 / -Vdc
```

所以这个电压矢量方向指向 A 相方向，也就是图里的 0° 方向。

---

### `U60(110)`

```text
A = 1
B = 1
C = 0
```

表示：

```text
A相接 +Vdc
B相接 +Vdc
C相接 0 / -Vdc
```

这个状态经过 Clarke 变换之后，空间电压矢量方向是 60°。

所以它叫：

```text
U60
```

括号里的：

```text
(110)
```

表示开关状态。

---

### `U120(010)`

```text
A = 0
B = 1
C = 0
```

表示：

```text
A相接 0 / -Vdc
B相接 +Vdc
C相接 0 / -Vdc
```

这个矢量方向是 120°。

---

## 3. 六个有效矢量对应关系

图里这 6 个就是三相逆变器的 6 个有效开关状态：

```text
U0    = (100)
U60   = (110)
U120  = (010)
U180  = (011)
U240  = (001)
U300  = (101)
```

可以这样记：

| 矢量   | 开关状态 | 含义          |
| ---- | ---- | ----------- |
| U0   | 100  | A 高，B 低，C 低 |
| U60  | 110  | A 高，B 高，C 低 |
| U120 | 010  | A 低，B 高，C 低 |
| U180 | 011  | A 低，B 高，C 高 |
| U240 | 001  | A 低，B 低，C 高 |
| U300 | 101  | A 高，B 低，C 高 |

它们刚好绕一圈，每个间隔 60°。

---

## 4. 那 `000` 和 `111` 是什么？

图中间还有两个：

```text
Uz(000)
Uz(111)
```

这两个叫 **零矢量**。

### `000`

```text
A = 0
B = 0
C = 0
```

三相都接到负母线。

### `111`

```text
A = 1
B = 1
C = 1
```

三相都接到正母线。

这两种情况下，三相之间没有线电压差：

```text
Uab = 0
Ubc = 0
Uca = 0
```

所以它们不会产生有效的空间电压矢量，因此叫 **零矢量**。

---

## 5. 注意一个容易混淆的点

图里的：

```text
U0(100)
```

这里的 `U0` 不是零矢量。

它的意思是：

```text
0° 方向的有效矢量
```

而不是：

```text
零电压矢量
```

真正的零矢量是：

```text
Uz(000)
Uz(111)
```

所以：

```text
U0(100)    = 0°方向有效矢量
Uz(000)    = 零矢量
Uz(111)    = 零矢量
```

这个地方很多新手容易搞混。

---

## 6. 结合 SVPWM 来看

SVPWM 就是在一个 PWM 周期里面，用：

```text
相邻两个有效矢量 + 零矢量
```

去合成目标电压矢量。

比如目标矢量在第 I 扇区，也就是 `U0(100)` 和 `U60(110)` 之间：

```text
第 I 扇区：U0 和 U60 之间
```

那么就用：

```text
U0(100) 作用一段时间 T1
U60(110) 作用一段时间 T2
Uz(000) / Uz(111) 作用一段时间 T0
```

合成目标电压。

---

一句话总结：

**括号里的 `100`、`110`、`010` 表示三相逆变器 A、B、C 三个桥臂的开关状态。`1` 表示该相上管导通接正母线，`0` 表示该相下管导通接负母线。6 个非零状态形成 6 个有效空间电压矢量，另外 `000` 和 `111` 是零矢量。**

------
-----
-----
你可以把 **SVPWM 的 MOS 开关波形**理解成：

> 在一个 PWM 周期 `Ts` 里面，按照某个顺序切换几个开关状态，比如
> `000 → 100 → 110 → 111 → 110 → 100 → 000`

这里的 `000/100/110` 就是 **A、B、C 三相上桥臂 MOS 的开关状态**。

```text
1 = 该相上管导通，下管关断
0 = 该相下管导通，上管关断
```

实际下管是上管的互补信号，但要加死区，不能上下管同时开。

---

## 1. 先看第 I 扇区的典型 7 段式 SVPWM

第 I 扇区在：

```text
U0(100) 和 U60(110) 之间
```

一个 PWM 周期内的开关顺序通常是：

```text
000 → 100 → 110 → 111 → 110 → 100 → 000
```

对应时间一般是：

```text
T0/4 → T1/2 → T2/2 → T0/2 → T2/2 → T1/2 → T0/4
```

画成表就是：

```text
时间段:   | T0/4 | T1/2 | T2/2 | T0/2 | T2/2 | T1/2 | T0/4 |
状态:     | 000  | 100  | 110  | 111  | 110  | 100  | 000  |
A上管Sa: |  0   |  1   |  1   |  1   |  1   |  1   |  0   |
B上管Sb: |  0   |  0   |  1   |  1   |  1   |  0   |  0   |
C上管Sc: |  0   |  0   |  0   |  1   |  0   |  0   |  0   |
```

粗略画成波形就是：

```text
第 I 扇区：000 → 100 → 110 → 111 → 110 → 100 → 000

Sa: ___|‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾|___
Sb: ________|‾‾‾‾‾‾‾‾‾|________
Sc: _____________|‾‾‾|_____________
```

所以在第 I 扇区里面：

```text
A 相占空比最大
B 相占空比居中
C 相占空比最小
```

也就是：

```text
Da > Db > Dc
```

---

## 2. 六个扇区的开关状态顺序

为了减少开关损耗，SVPWM 一般要求相邻状态之间只变化一个桥臂。

常见的 **七段式对称 SVPWM** 六个扇区如下：

| 扇区  | 相邻有效矢量      | 一个 PWM 周期内的开关顺序                           |
| --- | ----------- | ----------------------------------------- |
| I   | `100`、`110` | `000 → 100 → 110 → 111 → 110 → 100 → 000` |
| II  | `110`、`010` | `000 → 010 → 110 → 111 → 110 → 010 → 000` |
| III | `010`、`011` | `000 → 010 → 011 → 111 → 011 → 010 → 000` |
| IV  | `011`、`001` | `000 → 001 → 011 → 111 → 011 → 001 → 000` |
| V   | `001`、`101` | `000 → 001 → 101 → 111 → 101 → 001 → 000` |
| VI  | `101`、`100` | `000 → 100 → 101 → 111 → 101 → 100 → 000` |

这张表非常重要，写 SVPWM 代码的时候，经常就是围绕这个表来的。

---

## 3. 每个扇区的 A/B/C 上管波形

下面只画 **上桥臂 MOS**，也就是 `Sa、Sb、Sc`。下桥臂是互补的。

### 第 I 扇区

```text
状态:     000   100   110   111   110   100   000
Sa:        0     1     1     1     1     1     0
Sb:        0     0     1     1     1     0     0
Sc:        0     0     0     1     0     0     0
```

占空比关系：

```text
Da > Db > Dc
```

---

### 第 II 扇区

```text
状态:     000   010   110   111   110   010   000
Sa:        0     0     1     1     1     0     0
Sb:        0     1     1     1     1     1     0
Sc:        0     0     0     1     0     0     0
```

占空比关系：

```text
Db > Da > Dc
```

---

### 第 III 扇区

```text
状态:     000   010   011   111   011   010   000
Sa:        0     0     0     1     0     0     0
Sb:        0     1     1     1     1     1     0
Sc:        0     0     1     1     1     0     0
```

占空比关系：

```text
Db > Dc > Da
```

---

### 第 IV 扇区

```text
状态:     000   001   011   111   011   001   000
Sa:        0     0     0     1     0     0     0
Sb:        0     0     1     1     1     0     0
Sc:        0     1     1     1     1     1     0
```

占空比关系：

```text
Dc > Db > Da
```

---

### 第 V 扇区

```text
状态:     000   001   101   111   101   001   000
Sa:        0     0     1     1     1     0     0
Sb:        0     0     0     1     0     0     0
Sc:        0     1     1     1     1     1     0
```

占空比关系：

```text
Dc > Da > Db
```

---

### 第 VI 扇区

```text
状态:     000   100   101   111   101   100   000
Sa:        0     1     1     1     1     1     0
Sb:        0     0     0     1     0     0     0
Sc:        0     0     1     1     1     0     0
```

占空比关系：

```text
Da > Dc > Db
```

---

## 4. 这和实际 6 个 MOS 怎么对应？

三相半桥一般有 6 个 MOS：

```text
A_H  A_L
B_H  B_L
C_H  C_L
```

其中：

```text
Sa 控制 A_H
Sb 控制 B_H
Sc 控制 C_H
```

下管一般是互补：

```text
A_L = !Sa
B_L = !Sb
C_L = !Sc
```

但是实际驱动时必须加死区：

```text
A_H 关断一小段时间后，A_L 才能导通
A_L 关断一小段时间后，A_H 才能导通
```

否则上下管直通，会炸 MOS。

---

## 5. 拿 `100` 举例

状态 `100` 表示：

```text
Sa = 1
Sb = 0
Sc = 0
```

对应 MOS 状态：

```text
A_H = ON
A_L = OFF

B_H = OFF
B_L = ON

C_H = OFF
C_L = ON
```

也就是 A 相接母线正端，B/C 相接母线负端。

---

## 6. 最核心记忆方法

SVPWM 的六个扇区状态顺序可以这样记：

```text
I:   000 → 100 → 110 → 111 → 110 → 100 → 000
II:  000 → 010 → 110 → 111 → 110 → 010 → 000
III: 000 → 010 → 011 → 111 → 011 → 010 → 000
IV:  000 → 001 → 011 → 111 → 011 → 001 → 000
V:   000 → 001 → 101 → 111 → 101 → 001 → 000
VI:  000 → 100 → 101 → 111 → 101 → 100 → 000
```

一句话总结：

**SVPWM 每个扇区的 MOS 波形，本质就是在一个 PWM 周期内按照“零矢量 → 第一个有效矢量 → 第二个有效矢量 → 零矢量 → 第二个有效矢量 → 第一个有效矢量 → 零矢量”的顺序切换。括号里的 0/1 就直接对应 A/B/C 三个上桥臂 MOS 的开关状态。**

---
-----
-----
在 STM32 HAL 里实现 SVPWM，**不是用 GPIO 手动按 `000→100→110→111` 去翻转 MOS**。

实际工程里是这样做的：

> **CPU 算出 A/B/C 三相占空比 → 写 TIM1/TIM8 的 CCR1/CCR2/CCR3 → 定时器硬件自动输出 6 路互补 PWM → 驱动三相桥。**

也就是：

```text
SVPWM算法
   ↓
duty_a / duty_b / duty_c
   ↓
TIM1->CCR1 / TIM1->CCR2 / TIM1->CCR3
   ↓
CH1/CH1N、CH2/CH2N、CH3/CH3N
   ↓
A相上/下管、B相上/下管、C相上/下管
```

---

# 1. STM32 里一般用 TIM1 或 TIM8

因为 TIM1/TIM8 是 **高级定时器**，支持：

```text
三相 PWM 输出
互补输出 CHxN
死区时间 Dead Time
刹车保护 Break
中心对齐 PWM
```

三相桥 6 个 MOS 对应关系一般是：

| MOS   | STM32 PWM |
| ----- | --------- |
| A 相上管 | TIM1_CH1  |
| A 相下管 | TIM1_CH1N |
| B 相上管 | TIM1_CH2  |
| B 相下管 | TIM1_CH2N |
| C 相上管 | TIM1_CH3  |
| C 相下管 | TIM1_CH3N |

---

# 2. CubeMX / HAL 里 TIM1 要这么配

以 TIM1 为例。

## 定时器模式

```text
TIM1
Mode: PWM Generation CH1 CH1N
      PWM Generation CH2 CH2N
      PWM Generation CH3 CH3N
```

## 计数模式

建议用：

```text
Center Aligned Mode 1
```

也就是中心对齐 PWM。

不要一开始就用边沿对齐，FOC/SVPWM 常用中心对齐，电流采样也更方便。

## PWM 频率

比如你要 20kHz PWM。

如果 TIM1 时钟是 168MHz，中心对齐模式下频率公式大概是：

```text
PWM频率 = TIM_CLK / (2 * ARR)
```

所以：

```text
ARR = TIM_CLK / (2 * PWM_FREQ)
ARR = 168MHz / (2 * 20kHz)
ARR = 4200
```

也就是：

```c
htim1.Init.Period = 4200;
```

## 死区时间

比如先设置 500ns~1us 左右，具体看 MOS/驱动芯片。

CubeMX 里配置：

```text
Break and Dead-Time Management
Dead Time: 按你的驱动需求设置
```

如果没有死区，上下管可能直通，MOS 会炸。

---

# 3. HAL 启动 6 路 PWM

初始化完成后，在 `main()` 里启动：

```c
HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);
HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_3);

HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_1);
HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_2);
HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_3);
```

如果你的功率驱动芯片有使能脚，比如 `EN_GATE`，也要拉高：

```c
HAL_GPIO_WritePin(EN_GATE_GPIO_Port, EN_GATE_Pin, GPIO_PIN_SET);
```

---

# 4. SVPWM 核心就是更新 CCR

假设：

```c
#define PWM_PERIOD 4200
```

那么：

```c
TIM1->CCR1 = A相占空比 * PWM_PERIOD;
TIM1->CCR2 = B相占空比 * PWM_PERIOD;
TIM1->CCR3 = C相占空比 * PWM_PERIOD;
```

比如：

```c
TIM1->CCR1 = 3000;
TIM1->CCR2 = 2000;
TIM1->CCR3 = 1000;
```

就表示：

```text
A 相占空比最大
B 相占空比居中
C 相占空比最小
```

这可能就是某个扇区内的 SVPWM 输出状态。

---

# 5. 推荐你先用这种 SVPWM 写法：零序注入法

这个方法不用你手动判断 6 个扇区，代码简单，非常适合入门。

输入是：

```c
Ualpha
Ubeta
Vdc
```

输出是：

```c
duty_a
duty_b
duty_c
```

完整代码如下：

```c
#include <math.h>
#include <stdint.h>

#define SQRT3_BY_2  0.86602540378f

static float clampf(float x, float min, float max)
{
    if (x < min) return min;
    if (x > max) return max;
    return x;
}

/**
 * @brief SVPWM 计算三相占空比
 *
 * @param Ualpha alpha轴电压
 * @param Ubeta  beta轴电压
 * @param Vdc    直流母线电压
 * @param da     A相占空比输出，范围 0~1
 * @param db     B相占空比输出，范围 0~1
 * @param dc     C相占空比输出，范围 0~1
 */
void SVPWM_Calc(float Ualpha, float Ubeta, float Vdc,
                float *da, float *db, float *dc)
{
    float Ua, Ub, Uc;
    float Umax, Umin;
    float Uoffset;

    /*
     * 1. 逆 Clarke 变换
     *
     * 把 alpha-beta 电压转换成三相电压
     */
    Ua = Ualpha;
    Ub = -0.5f * Ualpha + SQRT3_BY_2 * Ubeta;
    Uc = -0.5f * Ualpha - SQRT3_BY_2 * Ubeta;

    /*
     * 2. 找最大值和最小值
     */
    Umax = Ua;
    if (Ub > Umax) Umax = Ub;
    if (Uc > Umax) Umax = Uc;

    Umin = Ua;
    if (Ub < Umin) Umin = Ub;
    if (Uc < Umin) Umin = Uc;

    /*
     * 3. 注入零序分量
     *
     * 这一步就是 SVPWM 和普通 SPWM 的重要区别之一。
     */
    Uoffset = -0.5f * (Umax + Umin);

    Ua += Uoffset;
    Ub += Uoffset;
    Uc += Uoffset;

    /*
     * 4. 转成 PWM 占空比
     *
     * duty = 0.5 + Uphase / Vdc
     */
    *da = 0.5f + Ua / Vdc;
    *db = 0.5f + Ub / Vdc;
    *dc = 0.5f + Uc / Vdc;

    /*
     * 5. 限幅，防止越界
     */
    *da = clampf(*da, 0.0f, 1.0f);
    *db = clampf(*db, 0.0f, 1.0f);
    *dc = clampf(*dc, 0.0f, 1.0f);
}
```

---

# 6. 把 duty 写入 TIM1

```c
#define PWM_PERIOD 4200

void SVPWM_SetDuty(float da, float db, float dc)
{
    uint32_t ccr1;
    uint32_t ccr2;
    uint32_t ccr3;

    da = clampf(da, 0.0f, 1.0f);
    db = clampf(db, 0.0f, 1.0f);
    dc = clampf(dc, 0.0f, 1.0f);

    ccr1 = (uint32_t)(da * PWM_PERIOD);
    ccr2 = (uint32_t)(db * PWM_PERIOD);
    ccr3 = (uint32_t)(dc * PWM_PERIOD);

    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, ccr1);
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, ccr2);
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_3, ccr3);
}
```

实际也可以直接写寄存器，速度更快：

```c
TIM1->CCR1 = ccr1;
TIM1->CCR2 = ccr2;
TIM1->CCR3 = ccr3;
```

---

# 7. 做一个开环 SVPWM 测试

先不要上来就闭环 FOC，可以先开环让电压矢量旋转。

比如：

```c
float angle = 0.0f;
float voltage = 3.0f;
float vbus = 12.0f;

void FOC_OpenLoop_Test(void)
{
    float Ualpha;
    float Ubeta;
    float da, db, dc;

    /*
     * 生成旋转电压矢量
     */
    Ualpha = voltage * cosf(angle);
    Ubeta  = voltage * sinf(angle);

    /*
     * SVPWM 计算占空比
     */
    SVPWM_Calc(Ualpha, Ubeta, vbus, &da, &db, &dc);

    /*
     * 更新 TIM1 三相 PWM
     */
    SVPWM_SetDuty(da, db, dc);

    /*
     * 更新角度
     */
    angle += 0.001f;

    if (angle > 6.28318530718f)
    {
        angle -= 6.28318530718f;
    }
}
```

然后你在定时器中断或者主循环里周期性调用：

```c
FOC_OpenLoop_Test();
```

比如 10kHz 控制频率：

```text
PWM 频率：20kHz
FOC/SVPWM 更新频率：10kHz 或 20kHz
```

---

# 8. 中断里更新 SVPWM

可以用 TIM1 Update 中断：

```c
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM1)
    {
        FOC_OpenLoop_Test();
    }
}
```

但是注意：

如果你的 PWM 是中心对齐模式，Update 中断触发频率和边沿对齐不完全一样，后面做电流采样时要特别注意采样点。

入门阶段可以先不用太纠结，先看三相 PWM 波形是否正常。

---

# 9. 一个最小 main 流程

大概是这样：

```c
int main(void)
{
    HAL_Init();

    SystemClock_Config();

    MX_GPIO_Init();
    MX_TIM1_Init();

    /*
     * 启动三相上桥臂 PWM
     */
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_3);

    /*
     * 启动三相下桥臂互补 PWM
     */
    HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_1);
    HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_2);
    HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_3);

    /*
     * 如果有驱动使能脚，打开驱动
     */
    // HAL_GPIO_WritePin(DRV_EN_GPIO_Port, DRV_EN_Pin, GPIO_PIN_SET);

    while (1)
    {
        FOC_OpenLoop_Test();

        /*
         * 简单测试可以先 delay
         * 真正 FOC 不建议用 HAL_Delay
         */
        HAL_Delay(1);
    }
}
```

这个版本只是为了让你理解流程。

真正做电机控制，不建议在 `while(1)` 里 `HAL_Delay(1)`，而是用定时器中断固定频率更新。

---

# 10. 重点：HAL 不是输出 000/100/110，而是输出占空比

你前面问的这个：

```text
第 I 扇区：
000 → 100 → 110 → 111 → 110 → 100 → 000
```

在 STM32 里不是 CPU 一步一步切 GPIO。

而是你写入：

```c
CCR1
CCR2
CCR3
```

然后高级定时器根据中心对齐 PWM 自动生成三相波形。

例如第 I 扇区可能有：

```text
Da > Db > Dc
```

于是：

```c
CCR1 最大
CCR2 中间
CCR3 最小
```

这时候定时器产生出来的比较波形，就等效于 SVPWM 的七段式开关序列。

---

# 11. 真正驱动 MOS 前要注意

刚开始调试不要直接接大电流电源。

建议顺序：

```text
1. 不接电机，不接高压，只看 TIM1_CH1/2/3 波形
2. 确认 CH1 和 CH1N 互补
3. 确认有死区
4. 确认三相占空比随 angle 变化
5. 限制 voltage 很小，比如 0.5V、1V、2V
6. 接低压电源
7. 串限流电源
8. 再接电机
```

非常关键：

```text
上管和下管绝对不能同时导通
```

不然三相桥会直通。

---

# 12. 你现在可以先按这个路线做

对于你这种 STM32 HAL 入门实现 SVPWM，建议顺序是：

```text
第一步：CubeMX 配 TIM1 三相互补 PWM + 死区
第二步：示波器看 CH1/CH1N、CH2/CH2N、CH3/CH3N
第三步：写 SVPWM_Calc()
第四步：写 SVPWM_SetDuty()
第五步：开环 angle 旋转
第六步：接驱动板低压测试
第七步：再加 Clarke/Park/PI 闭环
```

一句话总结：

**STM32 HAL 实现 SVPWM，本质就是用 TIM1/TIM8 产生三相互补 PWM，SVPWM 算法只负责计算 `CCR1/CCR2/CCR3`。不要用 GPIO 手动切 MOS 状态，MOS 的互补、死区、中心对齐这些都交给高级定时器硬件完成。**

-----
-----
----
# PWM互补输出的作用？？？
互补输出的作用就是：**一相半桥有上下两个 MOS，STM32 用一组 PWM 自动生成“上管 PWM”和“下管反向 PWM”，并且自动插入死区，防止上下管同时导通炸管。**

以 A 相为例：

```text
        +Vbus
          |
        A_H 上管 MOS
          |
A相输出点 ───── 接电机A相
          |
        A_L 下管 MOS
          |
         GND
```

如果 A 相状态是 `1`：

```text
A_H = ON
A_L = OFF
```

如果 A 相状态是 `0`：

```text
A_H = OFF
A_L = ON
```

所以 A 相上下管天然就是“互补”的。

---

## 1. 互补输出长啥样？

比如 TIM1_CH1 控制 A 相上管，TIM1_CH1N 控制 A 相下管：

```text
TIM1_CH1   : ___|‾‾‾‾‾‾‾‾|_____
             A相上管

TIM1_CH1N  : ‾‾‾|________|‾‾‾‾‾
             A相下管
```

但是实际不是完全同时反相，中间要加死区：

```text
TIM1_CH1   : ___|‾‾‾‾‾‾‾|________
                 ↑       ↓
               上管开   上管关

TIM1_CH1N  : ‾‾‾‾_____|‾‾‾‾‾‾‾‾
                  ↑   ↑
                死区 死区
```

这个“空白时间”就是 **dead time 死区**。

---

## 2. 为啥必须有死区？

因为 MOS 不是理想开关。

你让上管关断，它不是瞬间关断；你让下管导通，它也不是瞬间导通。假如没有死区，可能出现一瞬间：

```text
A_H = ON
A_L = ON
```

这就等于：

```text
+Vbus 直接短接到 GND
```

结果就是：

```text
轻则过流保护
重则 MOS / 驱动芯片炸掉
```

所以互补输出 + 死区，是三相逆变器里面非常重要的安全机制。

---

## 3. STM32 里的互补输出对应什么？

高级定时器 TIM1/TIM8 可以输出：

```text
CH1   -> A相上管
CH1N  -> A相下管

CH2   -> B相上管
CH2N  -> B相下管

CH3   -> C相上管
CH3N  -> C相下管
```

也就是 6 路 PWM：

```text
A_H  A_L
B_H  B_L
C_H  C_L
```

SVPWM 里面你只算：

```text
CCR1 = A相占空比
CCR2 = B相占空比
CCR3 = C相占空比
```

然后 TIM1 自动生成：

```text
CH1 / CH1N
CH2 / CH2N
CH3 / CH3N
```

这就是互补输出的意义。

---

## 4. 不用互补输出可以吗？

**可以，但要看你的驱动板/驱动芯片是什么输入方式。**

### 情况 1：驱动芯片需要 6 路输入

比如驱动芯片需要：

```text
AH、AL
BH、BL
CH、CL
```

这种情况下，强烈建议用 STM32 的 TIM1/TIM8 互补输出。

也就是：

```text
TIM1_CH1  -> AH
TIM1_CH1N -> AL
TIM1_CH2  -> BH
TIM1_CH2N -> BL
TIM1_CH3  -> CH
TIM1_CH3N -> CL
```

这种叫 **6PWM 控制方式**。

这种方式最适合 SVPWM / FOC。

---

### 情况 2：驱动芯片内部自带互补和死区

有些三相驱动芯片只需要 MCU 给它 3 路 PWM：

```text
PWM_A
PWM_B
PWM_C
```

然后芯片内部自动生成上下管互补信号和死区。

这种情况下，STM32 不需要输出 CHxN，只需要：

```text
TIM1_CH1 -> PWM_A
TIM1_CH2 -> PWM_B
TIM1_CH3 -> PWM_C
```

这种叫 **3PWM 控制方式**。

这时“不用互补输出”是可以的，因为互补和死区已经被驱动芯片接管了。

---

### 情况 3：自己用普通 GPIO 模拟上下管

理论上可以，实际不建议。

因为你要自己处理：

```text
上管 PWM
下管反向 PWM
死区
刹车保护
异常关断
更新同步
```

一旦代码卡顿、中断延迟、更新不同步，就可能直通。

所以做电机控制时，一般不建议用普通 GPIO 软件模拟 6 路 MOS 控制。

---

## 5. SVPWM 里的 `100` 和互补输出怎么对应？

比如状态：

```text
100
```

表示：

```text
A相上管开，B相上管关，C相上管关
```

如果使用互补输出，那么对应 6 个 MOS 是：

```text
A_H = 1    A_L = 0
B_H = 0    B_L = 1
C_H = 0    C_L = 1
```

也就是：

```text
A 相接 +Vbus
B 相接 GND
C 相接 GND
```

所以 `100` 里面的 `1/0`，本质上只描述了三相上管状态。下管状态一般就是互补出来的。

---

## 6. 用 STM32 HAL 时推荐怎么做？

如果你是自己做三相桥，或者驱动芯片支持 6PWM 输入，推荐：

```text
TIM1/TIM8
中心对齐 PWM
CH1/CH1N
CH2/CH2N
CH3/CH3N
Dead Time
Break 保护
```

启动代码大概是：

```c
HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);
HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_3);

HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_1);
HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_2);
HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_3);
```

然后 SVPWM 只更新：

```c
TIM1->CCR1 = ccr_a;
TIM1->CCR2 = ccr_b;
TIM1->CCR3 = ccr_c;
```

---

## 7. 结论

**互补输出就是为了控制每一相半桥的上下两个 MOS：上管开的时候下管关，下管开的时候上管关，并且中间自动插入死区。**

不用互补输出也可以，但前提是：

```text
你的驱动芯片内部已经帮你做了互补和死区
```

如果你的驱动芯片需要 MCU 直接给 6 路 MOS 控制信号，那最好必须用 STM32 的 TIM1/TIM8 互补输出。对于 SVPWM/FOC，新手建议直接走 **TIM1/TIM8 + 6PWM + 死区 + Break保护** 这条路线。
