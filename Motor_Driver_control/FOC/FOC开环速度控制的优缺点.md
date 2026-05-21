你说的 **BLDC FOC 开关速度控制**，我理解可能有两种意思：

1. **速度控制“打开/关闭”**：比如目标速度从 0 突然切到 1000rpm，或者从 1000rpm 突然切到 0。
2. **PWM 开关控制速度**：通过三相桥 PWM 开关调节电机电压/电流，从而控制速度。

FOC 里真正控制电机的不是“直接控制速度”，而是：

**速度环输出目标力矩电流 Iq → 电流环控制 Iq/Id → SVPWM/PWM 控制三相 MOS 管 → 电机产生转矩 → 速度变化**

---

## 一、FOC 速度控制时，主要波形长什么样？

### 1. 目标速度波形

如果你直接开关速度，比如：

```c
target_speed = 0;
突然变成
target_speed = 1000 rpm;
```

目标速度波形就是一个**阶跃波形**：

```text
目标速度
1000rpm ┌────────────────────
        │
        │
0rpm ───┘
        ↑
      突然启动
```

这对电机来说很粗暴。

---

### 2. 实际速度波形

实际速度不会瞬间上去，因为电机有惯量、负载、摩擦、电流限制。

```text
实际速度
1000rpm ────────────────
          ／￣￣￣￣￣￣
        ／
      ／
0rpm ─┘
```

如果控制参数不好，可能会变成这样：

```text
实际速度
1000rpm ──────┐   ┌─────
              │\ /│
              │ X │  ← 超调、振荡
              │/ \│
0rpm ─────────┘   └─────
```

也就是速度冲过头，再拉回来，甚至来回抖。

---

### 3. Iq 力矩电流波形

FOC 中，**Iq 基本可以理解为“产生力矩的电流”**。

目标速度突然变化时，速度 PI 会认为：

> 当前速度离目标速度差很多，需要很大力矩！

于是 Iq 会突然变大：

```text
Iq电流
最大限幅 ┌───────┐
        │       │
        │       │
0 ──────┘       └──────
        ↑
      启动瞬间大电流
```

如果没有限流、斜坡、软启动，Iq 可能会突然冲到限幅。

---

### 4. 三相电流波形

FOC 正常运行时，三相电流大概是接近正弦波的：

```text
Ia:   ／￣＼    ／￣＼
    ／     ＼／     ＼

Ib:      ／￣＼    ／￣＼
      ／     ＼／     ＼

Ic:  ＼     ／＼     ／
      ＼＿／    ＼＿／
```

三相相差 120°。

如果你开关速度太猛，三相电流会出现突变、畸变、尖峰：

```text
相电流
正常正弦：    ~~~~~~~~
突然加速：    ~~|^^^^|~~~~
突然刹车：    ~~|vvvv|~~~~
```

实际用示波器看电流，会看到启动瞬间电流明显变大，甚至有毛刺。

---

### 5. PWM 波形

FOC 最后会通过 SVPWM 输出三相 PWM。

单相桥臂上的 PWM 大概像这样：

```text
PWM输出
高电平 ┌─┐ ┌──┐ ┌────┐ ┌──────┐
      │ │ │  │ │    │ │      │
低电平┘ └─┘  └─┘    └─┘      └──
        占空比逐渐变化
```

FOC 调速时，PWM 占空比会随着电角度不断变化。低速时，占空比变化慢；高速时，占空比变化快。

如果速度突然开关，PWM 占空比可能会突然拉大：

```text
小占空比： ┌┐  ┌┐  ┌┐  ┌┐
          └┘  └┘  └┘  └┘

大占空比： ┌────┐ ┌────┐ ┌────┐
          └────┘ └────┘ └────┘
```

---

## 二、速度开关控制的核心风险

### 风险 1：启动瞬间电流过大

这是最常见的问题。

比如目标速度从 0 突然给到 3000rpm，速度环会疯狂加 Iq，电机电流瞬间增大。

后果可能是：

```text
目标速度突变
    ↓
速度误差很大
    ↓
速度PI输出很大Iq
    ↓
三相电流暴涨
    ↓
MOS管、电机、采样电阻、电源发热
```

严重时可能烧 MOS、烧驱动、触发过流保护。

---

### 风险 2：机械冲击大

对于平衡车、电机轮、风机、云台、机器人关节来说，速度突变会带来很大的机械冲击。

例如：

```text
0rpm → 2000rpm
```

电机会“猛地一拽”。

后果：

* 齿轮冲击；
* 皮带打滑；
* 轮子空转；
* 平衡车突然窜出去；
* 结构件震动；
* 电机啸叫。

对于你之前说的 **双轮平衡车 FOC**，这个风险尤其大。平衡车不能简单地“开关速度”，否则车体可能突然前冲或者后仰。

---

### 风险 3：速度环 PI 饱和，产生积分 windup

FOC 速度环一般是 PI 控制：

```text
速度误差 = 目标速度 - 实际速度
Iq_ref = Kp * 速度误差 + Ki * 误差积分
```

如果目标速度突然变很大，但电机由于惯量上不去，误差长时间很大，积分项会越积越多。

结果就是：

```text
积分项越积越大
    ↓
Iq_ref 长时间顶到最大
    ↓
等速度终于上来了
    ↓
积分项还没降下来
    ↓
速度冲过头
```

表现为：

```text
目标速度： ─────────────
实际速度：   ／￣￣＼＿
            ↑
          超调
```

这就是 **积分饱和 / 积分累积 / anti-windup 问题**。

---

### 风险 4：低速或启动时转子位置不准

FOC 需要知道电角度。

有感 FOC 用霍尔、编码器、磁编码器，问题小一些。

无感 FOC 低速启动时，反电动势很小，位置估算困难。如果你突然开速度，可能出现：

```text
位置估算不准
    ↓
Park变换角度错
    ↓
Iq/Id控制错
    ↓
电机抖动、反转、堵转、过流
```

所以无感 FOC 通常要有：

* 预定位；
* 开环启动；
* 速度爬坡；
* 切闭环；
* 失败保护。

不能简单一开就给高速。

---

### 风险 5：母线电压波动，甚至再生过压

突然加速时，电机会从电源吸收大电流，母线电压可能下跌：

```text
母线电压
24V ─────┐  ┌────
         │  │
         └──┘
       加速瞬间掉压
```

突然减速时，电机可能进入发电状态，把能量回灌到母线，母线电压可能升高：

```text
母线电压
24V ─────┐  ┌────
         │  │
         └──┘  这是加速掉压

突然刹车时：

24V ─────┐
         └───╮
             ╰── 可能升到 30V、36V
```

如果没有制动电阻、能量泄放、电压保护，可能损坏驱动板。

---

## 三、正确做法：不要直接“开关速度”，要加斜坡

不要这样：

```c
target_speed = 3000;
```

应该这样：

```c
target_speed_cmd = 3000;       // 用户想要的目标速度
target_speed_ref += ramp_step; // 控制器实际使用的速度慢慢爬上去
```

波形变成：

```text
错误做法：速度阶跃

3000rpm ┌────────────
        │
0rpm ───┘


正确做法：速度斜坡

3000rpm      ／────────
            ／
          ／
0rpm ────┘
```

这样速度环不会瞬间输出巨大 Iq。

---

## 四、FOC 速度控制推荐结构

比较稳的结构是：

```text
用户目标速度
    ↓
速度斜坡限制
    ↓
速度环 PI
    ↓
Iq_ref 限幅
    ↓
电流环 PI
    ↓
Ud / Uq 限幅
    ↓
SVPWM
    ↓
三相逆变桥
    ↓
BLDC/PMSM 电机
```

也就是：

```text
Speed_cmd → Ramp → Speed_PI → Iq_ref → Current_PI → SVPWM → Motor
```

重点保护有这几个：

```text
速度斜坡限制
Iq最大限流
PI积分限幅
母线电压保护
堵转保护
过流保护
过温保护
启动失败保护
```

---

## 五、你用示波器/逻辑分析仪能看到什么？

### 1. 看三相 PWM

测量 U、V、W 三个桥臂对地 PWM。

你会看到三路占空比不断变化。

```text
U相PWM：占空比按正弦规律变化
V相PWM：比U相滞后120°
W相PWM：比V相滞后120°
```

注意：不要直接用普通示波器地夹去夹半桥高压节点，容易短路。低压开发板也要注意地线位置。

---

### 2. 看相电流

用电流探头、采样电阻差分测量，或者看驱动板的 ADC 采样波形。

正常 FOC 电流应该比较平滑，接近正弦。

异常情况：

```text
电流尖峰大：速度给得太猛 / PI太激进 / 采样有问题
电流毛刺多：PWM死区、MOS开关噪声、采样时刻不对
电流抖动：角度不准 / 电流环参数不好
电流顶死：限流触发 / 负载太大 / 堵转
```

---

### 3. 看母线电压

加速时可能掉压，减速时可能升压。

```text
加速：Vbus 下跌
减速：Vbus 上升
```

平衡车、轮毂电机、惯量大的负载尤其要注意这个。

---

## 六、最危险的场景

最危险的是这几种：

```text
1. 电机静止时，突然给很高速度
2. 高速运行时，突然给反向速度
3. 高速运行时，突然关闭PWM但没有合理制动策略
4. 无感FOC低速时直接闭环高速启动
5. 负载很重时强行加速
6. 母线电压高、限流没做好
7. PI参数太猛，没有积分限幅
```

尤其是这个：

```text
+3000rpm → -3000rpm
```

这不是简单调速，这是强制反向制动，电流和母线电压都可能非常吓人。

---

## 七、简单结论

BLDC FOC 速度开关控制时，典型波形是：

```text
目标速度：阶跃或斜坡
实际速度：缓慢跟随，可能超调
Iq电流：启动瞬间增大
三相电流：接近正弦，但突变时会有尖峰
PWM占空比：随电角度变化，负载突变时占空比快速变化
母线电压：加速下跌，减速回升
```

最大的风险是：

```text
速度突变 → 速度误差巨大 → Iq暴涨 → 电流过大 → MOS/电机/电源风险
```

所以工程上不要直接做硬开关速度，要做：

```text
速度斜坡 + 电流限幅 + PI防积分饱和 + 过流/过压/堵转保护
```

对平衡车这种应用来说，更不能简单用“速度开关控制”。它应该是：

```text
车身角度环 → 速度环/位置环 → 电流环 → FOC
```

而不是简单：

```text
开关 → 目标速度 → 电机
```

----------
--------
-----
不太建议用“开关速度控制”来做 BLDC/FOC 调速，尤其是平衡车、轮毂电机、惯量负载。**可以有“启停开关”，但速度目标不要硬切换，应该用斜坡慢慢给速度。**

你问的重点是：**FOC 三相 PWM 波形是什么样的？**

---

## 1. 先说结论：FOC 的三相 PWM 不是简单固定占空比

FOC 输出到三相桥的 PWM，大概是这种感觉：

```text
U相 PWM：  占空比从小 → 大 → 小 → 大... 周期性变化
V相 PWM：  同样变化，但相位比 U 相错开 120°
W相 PWM：  同样变化，但相位比 V 相错开 120°
```

也就是说，每一路本质上还是高频 PWM，例如 20kHz：

```text
单相桥臂 PWM 微观看：

高电平  ┌─┐ ┌──┐ ┌────┐ ┌──────┐ ┌────┐ ┌──┐ ┌─┐
        │ │ │  │ │    │ │      │ │    │ │  │ │ │
低电平──┘ └─┘  └─┘    └─┘      └─┘    └─┘  └─┘ └──
          小     中      大        大      中    小
              占空比在不断变化
```

宏观看，占空比包络接近三相正弦：

```text
U相占空比包络：      /‾‾‾\      /‾‾‾\
                  /       \    /       \
                 /         \__/         \__

V相占空比包络：  \__      /‾‾‾\      /‾‾‾\
                    \    /       \    /
                     \__/         \__/

W相占空比包络：    /‾‾‾\      /‾‾‾\      
                  /       \    /       \
             ____/         \__/         \__
```

三相之间相差 **120° 电角度**。

---

## 2. 如果是 SVPWM，三相 PWM 长得更像“马鞍形包络”

FOC 常用 SVPWM，不是最普通的三相正弦 PWM。

所以你如果用示波器看三相桥臂对地电压，看到的不是完美正弦，而是**高频 PWM 脉冲**。它的占空比包络大概像这样：

```text
U相桥臂对地：  高频PWM，占空比按某种规律变化
V相桥臂对地：  高频PWM，占空比延后120°
W相桥臂对地：  高频PWM，占空比再延后120°
```

更准确一点说，三相桥臂输出的是：

```text
UH / UL：互补PWM + 死区
VH / VL：互补PWM + 死区
WH / WL：互补PWM + 死区
```

比如 U 相上桥和下桥：

```text
U上桥 PWM：
高电平  ┌────┐      ┌────┐      ┌────┐
        │    │      │    │      │    │
低电平──┘    └──────┘    └──────┘    └──

U下桥 PWM：
高电平────┐    ┌──────┐    ┌──────┐
          │    │      │    │      │
低电平    └────┘      └────┘      └────

中间要插入死区，防止上下桥直通。
```

---

## 3. 示波器看三相 PWM，会看到什么？

假设你测的是：

```text
CH1：U相桥臂输出对 GND
CH2：V相桥臂输出对 GND
CH3：W相桥臂输出对 GND
```

你会看到三路都是高速方波：

```text
U相： ┌─┐ ┌──┐ ┌────┐ ┌──────┐ ┌────┐ ┌──┐
     ┘ └─┘  └─┘    └─┘      └─┘    └─┘  └─

V相： ┌──────┐ ┌────┐ ┌──┐ ┌─┐ ┌──┐ ┌────┐
     ┘      └─┘    └─┘  └─┘ └─┘  └─┘    └─

W相： ┌──┐ ┌─┐ ┌──┐ ┌────┐ ┌──────┐ ┌────┐
     ┘  └─┘ └─┘  └─┘    └─┘      └─┘    └─
```

它们不是同一个占空比，而是一直变化。

---

## 4. 相电压、线电压、相电流，波形不一样

这个很关键。

### 你测 U 相对地

看到的是 **PWM 方波**：

```text
U-GND：PWM方波
```

### 你测 U-V 两相之间

看到的是 **线电压 PWM**，会有正、负电压变化：

```text
U-V：一堆正负跳变的PWM脉冲
```

大概类似：

```text
+Vbus  ┌─┐   ┌────┐       ┌──┐
       │ │   │    │       │  │
0V  ───┘ └───┘    └───┬───┘  └──
                      │
-Vbus                 └───────
```

### 你测相电流

如果电流环调得好，电流才接近正弦：

```text
Ia：    /‾‾\      /‾‾\
      /     \    /     \
_____/       \__/       \____

Ib：  相差120°

Ic：  再相差120°
```

所以：

```text
桥臂电压 = PWM方波
线电压 = 正负跳变的PWM
相电流 = 接近正弦波
```

很多初学者会误以为 FOC 输出的是三相正弦电压，其实 MCU 输出的是 **PWM**，电机绕组电感把 PWM 电流“滤”成了接近正弦。

---

## 5. 低速和高速时，PWM 有啥区别？

### 低速时

电角度变化慢，所以三相占空比变化慢。

```text
低速：
PWM频率还是 20kHz
但是占空比包络变化慢
```

### 高速时

电角度变化快，所以三相占空比包络变化快。

```text
高速：
PWM频率还是 20kHz
但是占空比包络变化快
```

注意：**PWM 频率一般不随速度变化**，比如一直是 20kHz。变化的是每个 PWM 周期里的占空比。

---

## 6. “开关速度控制”下三相 PWM 会发生什么？

假设目标速度突然从 0 切到 3000rpm。

速度环会突然要求很大的 Iq 电流。于是 FOC 会让三相 PWM 占空比快速变化，输出更大的等效电压。

表现为：

```text
开关前：
三相PWM占空比较小，或者接近中间值

开关后：
三相PWM占空比突然拉大
三相电流突然上升
电机猛地加速
```

波形感觉是：

```text
开关前：
U相： ┌─┐ ┌─┐ ┌─┐ ┌─┐
     ┘ └─┘ └─┘ └─┘ └─

开关后：
U相： ┌──────┐ ┌────┐ ┌──────┐
     ┘      └─┘    └─┘      └─
```

但三相不是同时变大，而是按照当前电角度和 SVPWM 算法分配到 U/V/W 三相。

---

## 7. 建不建议开关速度控制？

**不建议把速度目标硬开关。**

不建议这样：

```c
if (start)
{
    target_speed = 3000;
}
else
{
    target_speed = 0;
}
```

建议这样：

```c
if (start)
{
    speed_cmd = 3000;
}
else
{
    speed_cmd = 0;
}

// 实际送入速度环的 speed_ref 慢慢靠近 speed_cmd
speed_ref = ramp_to(speed_ref, speed_cmd, step);
```

也就是：

```text
错误：速度阶跃

3000rpm ┌────────────
        │
0rpm ───┘


正确：速度斜坡

3000rpm      /────────
            /
           /
0rpm ─────┘
```

---

## 8. 一个简单判断

如果你用示波器看三相 PWM：

### 正常 FOC

```text
三相PWM：占空比周期性变化
相电流：接近正弦
电机：平滑转动
声音：比较顺
```

### 速度硬开关

```text
三相PWM：占空比突然变化
相电流：突然冲高
母线电压：可能下跌
电机：猛冲、抖动、啸叫
```

---

## 最核心一句话

**FOC 的三相 PWM 是三路高频 PWM，占空比按照电角度连续变化，三相相差 120°；真正接近正弦的是电机相电流，不是 MCU 引脚上的 PWM 波形。**

速度控制不建议硬开关，建议：

```text
开关命令 → 速度斜坡 → 速度环 → Iq限幅 → 电流环 → SVPWM → 三相PWM
```

-------
-------
----
可以开环速度控制，但**不建议把它作为正式产品的主控制方式**，尤其是平衡车、轮毂电机、负载变化大的设备。

你要先分清楚两个东西：

```text
开环速度控制 ≠ FOC闭环速度控制
```

---

## 1. 什么叫开环速度控制？

开环速度控制大概是：

```text
我想让电机转 1000rpm
    ↓
我直接按 1000rpm 的电角度速度去旋转磁场
    ↓
不管电机真实有没有跟上
```

也就是控制器自己生成一个旋转角度：

```c
theta += omega_cmd * Ts;
```

然后根据这个角度输出三相 PWM。

```text
控制器心里想：
“我现在电角度转到这里了，电机应该跟着我走。”
```

但是问题是：**电机真实转子位置不一定在你想的位置。**

---

## 2. FOC 为什么怕开环速度？

FOC 的核心是：**你必须知道转子磁场在哪里。**

FOC 要做 Park 变换：

```text
三相电流 Ia/Ib/Ic
    ↓
Clarke变换
    ↓
得到 α/β 电流
    ↓
根据转子电角度做 Park 变换
    ↓
得到 Id/Iq
```

关键就在这个电角度。

```text
电角度准：
Iq 真的是力矩电流
Id 真的是励磁方向电流

电角度错：
Iq 和 Id 混了
力矩变小
电流变大
电机发热、抖动、失步
```

可以这样理解：

```text
FOC = 你要一直推着转子的“侧面”走
```

如果角度准确：

```text
转子磁场 ↑

你施加的定子磁场 →
二者接近 90°
力矩最大
```

如果角度错了：

```text
转子磁场 ↑

你施加的定子磁场 ↑
二者方向差太小
基本不产生转矩，只会发热
```

如果角度差太大：

```text
转子磁场 ↑

你施加的定子磁场 ↓
可能反向制动、抖动、失步
```

---

## 3. 开环速度最大的问题：你不知道电机有没有跟上

开环控制是：

```text
给多少速度 → 你就假设电机正在按这个速度转
```

但真实情况可能是：

```text
负载变重
摩擦变大
电池电压下降
启动时惯量大
地面阻力变化
轮子卡住
```

这些都会导致真实速度跟不上。

比如你让磁场按 1000rpm 转：

```text
控制器磁场：1000rpm
真实转子：  300rpm
```

这时电角度误差会越来越大。

结果就是：

```text
角度误差越来越大
    ↓
电流方向越来越不对
    ↓
有效力矩下降
    ↓
电机更跟不上
    ↓
最后失步、抖动、堵转、过流
```

这个就是开环速度控制最危险的地方。

---

## 4. 开环速度控制下，三相 PWM 是什么样？

开环时，三相 PWM 仍然可以长得很“漂亮”。

因为 MCU 是按照自己生成的角度输出波形：

```text
U相PWM：占空比按正弦/SVPWM规律变化
V相PWM：相差120°
W相PWM：相差120°
```

也就是说，示波器看三相 PWM，可能是正常的：

```text
U相占空比包络：   /‾‾‾\      /‾‾‾\
               /       \    /       \

V相占空比包络：     /‾‾‾\      /‾‾‾\
                 /       \    /

W相占空比包络：       /‾‾‾\      /‾‾‾\
                   /       \    /
```

但是注意：

**PWM 好看，不代表电机真的跟上了。**

这是关键。

开环控制下，控制器只是“自嗨式输出三相旋转磁场”。

```text
控制器输出：正常三相PWM
电机实际：可能没跟上、抖动、失步、发热
```

---

## 5. 为什么启动阶段经常可以开环？

因为无感 FOC 启动时，低速反电动势很弱，控制器很难估算转子位置。

所以常见流程是：

```text
预定位
    ↓
开环拖动
    ↓
转速上来
    ↓
反电动势足够
    ↓
切入闭环估算
```

开环启动不是不能用，而是要小心用。

例如无感 FOC 里常见：

```text
阶段1：给固定电角度，把转子吸到某个位置
阶段2：慢慢旋转电角度，让电机跟着启动
阶段3：达到一定转速后，切到闭环
```

这个开环速度一般是**斜坡**，不是猛地给高速。

```text
错误：
0rpm → 3000rpm 硬切

正确：
0rpm → 100rpm → 200rpm → 300rpm → ... → 闭环
```

---

## 6. 开环速度控制适合什么场景？

适合这些：

```text
1. 风扇、泵类负载
2. 负载变化小
3. 对速度精度要求不高
4. 允许丢速
5. 只作为启动阶段
6. 简单成本优先的产品
```

比如小风扇，转快点慢点没那么严重。即使转速不是特别准，用户可能也不敏感。

---

## 7. 不适合什么场景？

不适合这些：

```text
1. 平衡车
2. 机器人关节
3. 轮毂电机
4. 云台
5. 高动态响应负载
6. 负载突然变化的设备
7. 需要准确速度/位置的系统
8. 可能堵转、卡滞的设备
```

平衡车尤其不适合。

因为平衡车不是单纯要“转起来”，而是要根据车身姿态实时修正力矩。

平衡车真正需要的是：

```text
姿态角度环
    ↓
速度/位置辅助环
    ↓
Iq力矩电流
    ↓
FOC电流环
    ↓
电机输出力矩
```

它核心控制量应该是**力矩/电流**，不是开环速度。

---

## 8. 开环速度控制的主要风险

### 风险一：失步

控制器磁场转得太快，转子跟不上。

```text
定子磁场：越转越快
转子：跟不上
结果：抖动、尖叫、停转
```

---

### 风险二：电流大但没力

角度错了以后，电流不一定产生有效转矩。

```text
电流很大
电机不怎么转
MOS发热
电机发热
电池电流暴涨
```

这就像你用力推门，但是方向推错了，力都浪费了。

---

### 风险三：堵转过流

如果轮子被卡住，但你还在开环输出旋转磁场：

```text
控制器以为电机在转
真实电机没转
    ↓
持续加电
    ↓
大电流
    ↓
过热/烧MOS/烧电机
```

所以开环控制必须加：

```text
母线电流保护
相电流保护
堵转判断
温度保护
超时保护
```

---

### 风险四：速度不准

开环速度的“速度”其实只是：

```text
控制器旋转磁场的速度
```

不一定等于：

```text
电机真实机械转速
```

尤其负载变化后，真实速度会掉。

---

### 风险五：低速抖动

低速时，摩擦、齿槽转矩、负载扰动影响明显。开环磁场慢慢走，转子可能一卡一卡地跟。

表现是：

```text
低速爬行
抖动
一顿一顿
有啸叫
```

---

## 9. 开环速度控制和闭环速度控制的区别

### 开环速度控制

```text
目标速度
    ↓
直接生成电角度
    ↓
SVPWM
    ↓
电机

没有速度反馈
没有位置反馈
不知道电机实际情况
```

### 闭环速度控制

```text
目标速度
    ↓
和真实速度比较
    ↓
速度PI输出 Iq_ref
    ↓
电流环控制
    ↓
SVPWM
    ↓
电机
    ↑
编码器/霍尔/无感估算反馈
```

闭环知道：

```text
电机到底转没转
转得快还是慢
有没有跟上目标
```

所以闭环能修正误差。

---

## 10. 一句话总结

**不是绝对不能开环速度控制，而是开环速度控制只是在“假设电机跟得上”。**

一旦负载变化、启动太猛、轮子卡住、电池电压下降，真实转子跟不上你生成的旋转磁场，就会出现：

```text
失步、抖动、堵转、大电流、发热、烧MOS、速度不准
```

所以工程上通常是：

```text
开环：用于无感启动、简单风扇、低要求场景
闭环：用于正式调速、平衡车、轮毂电机、机器人、电动车
```

对你做平衡车来说，建议方向是：

```text
有感FOC/编码器角度反馈
    +
电流闭环
    +
速度/姿态闭环
```

不要把平衡车做成：

```text
开环给三相PWM，让它自己转
```

这样车很难稳，而且风险很大。

-------
------
-----
闭环速度控制的核心是这条链路：

```text
目标速度 speed_ref
        ↓
和真实速度 speed_meas 做比较
        ↓
速度 PI 控制器
        ↓
输出目标力矩电流 Iq_ref
        ↓
FOC 电流环
        ↓
输出 Ud/Uq
        ↓
SVPWM
        ↓
三相 PWM
        ↓
电机转动
        ↑
编码器/霍尔/无感估算反馈真实速度
```

一句话：

> **速度环不直接控制 PWM，速度环只负责算出“需要多大力矩电流 Iq”。真正控制 PWM 的是电流环和 SVPWM。**

---

# 1. 闭环速度控制需要哪些东西？

至少需要这几个模块：

```text
1. 速度目标 speed_cmd
2. 速度斜坡 speed_ref
3. 速度反馈 speed_meas
4. 速度 PI 控制器
5. Iq_ref 限幅
6. 电流环 PI
7. SVPWM
8. 三相 PWM 输出
```

更完整一点：

```text
用户目标速度
    ↓
速度斜坡 Ramp
    ↓
speed_ref
    ↓
速度误差 = speed_ref - speed_meas
    ↓
速度 PI
    ↓
Iq_ref
    ↓
Iq 限幅
    ↓
电流环：Id/Iq PI
    ↓
Ud/Uq
    ↓
SVPWM
    ↓
三相桥 PWM
```

---

# 2. 真实速度 speed_meas 怎么来？

闭环速度控制最重要的是：**你要知道电机真实转得多快。**

常见有三种方式。

---

## 方式一：编码器 / 磁编码器

比如 AS5600、MT6701、TLE5012、ABI 编码器。

你每隔固定时间读取一次角度：

```c
theta_now = encoder_get_angle();
```

然后计算角度差：

```c
delta_theta = theta_now - theta_last;
```

速度：

```c
speed = delta_theta / dt;
```

例如：

```text
上一次角度：10°
这一次角度：13°
间隔时间：1ms

速度 = 3° / 1ms
```

这就是速度反馈。

---

## 方式二：霍尔传感器

BLDC 三个霍尔一般每 60° 电角度跳变一次。

可以通过两种方法算速度：

```text
1. 固定周期内数霍尔跳变次数
2. 测量两次霍尔跳变之间的时间
```

低速时一般用测周期，高速时可以用计数法。

霍尔速度反馈精度比编码器差，但便宜、简单。

---

## 方式三：无感估算

无感 FOC 通过反电动势、观测器、PLL 等方法估算转子位置和速度。

缺点是：

```text
低速难
启动难
参数依赖强
调试复杂
```

做平衡车我更建议你先用**有感方案**，比如磁编码器或者 ABI 增量编码器。

---

# 3. 速度闭环和电流闭环的关系

这个非常关键。

FOC 一般是双闭环，甚至三闭环。

```text
外环：速度环
内环：电流环
```

速度环输出的是：

```text
Iq_ref
```

也就是“我希望电机输出多少力矩”。

电流环负责让真实 Iq 跟上 Iq_ref。

```text
速度环：
speed_ref - speed_meas → PI → Iq_ref

电流环：
Iq_ref - Iq_meas → PI → Uq
Id_ref - Id_meas → PI → Ud
```

通常：

```text
Id_ref = 0
Iq_ref = 速度环输出
```

对于普通表贴式永磁电机，Id 一般给 0，Iq 负责出力矩。

---

# 4. 控制频率怎么安排？

一般这样安排：

```text
PWM / 电流环：10kHz ~ 40kHz
速度环：500Hz ~ 2kHz
位置/姿态环：100Hz ~ 1kHz
```

比如你用 20kHz PWM：

```text
PWM 中断：20kHz，每 50us 进一次
电流环：20kHz 或 10kHz
速度环：1kHz，每 1ms 跑一次
```

不要每个 PWM 周期都跑速度环，因为速度变化没那么快，而且速度采样容易有噪声。

推荐：

```text
电流环：20kHz
速度环：1kHz
```

---

# 5. 最小闭环速度控制流程

假设你已经有 FOC 电流环和 SVPWM。

那么速度环只需要加在外面：

```c
// 用户想要的速度，单位可以是 rpm
float speed_cmd_rpm = 1000.0f;

// 斜坡后的速度目标
float speed_ref_rpm = 0.0f;

// 实际测得速度
float speed_meas_rpm = 0.0f;

// 速度环输出：目标Iq
float iq_ref = 0.0f;

// d轴电流目标，一般为0
float id_ref = 0.0f;
```

每 1ms 跑一次速度环：

```c
void speed_loop_1khz(void)
{
    // 1. 速度斜坡，避免目标速度突然跳变
    speed_ref_rpm = ramp_update(speed_ref_rpm, speed_cmd_rpm, 5.0f);

    // 2. 获取真实速度
    speed_meas_rpm = motor_get_speed_rpm();

    // 3. 速度误差
    float speed_err = speed_ref_rpm - speed_meas_rpm;

    // 4. 速度PI，输出目标Iq
    iq_ref = pi_speed_update(&speed_pi, speed_err);

    // 5. Iq限幅，防止电流过大
    iq_ref = constrain(iq_ref, -IQ_MAX, IQ_MAX);

    // 6. Id一般给0
    id_ref = 0.0f;
}
```

然后电流环里使用：

```c
void current_loop_20khz(void)
{
    // 1. 采样三相电流
    get_phase_current(&ia, &ib, &ic);

    // 2. Clarke变换
    clarke_transform(ia, ib, ic, &i_alpha, &i_beta);

    // 3. 获取电角度
    theta_e = get_electrical_angle();

    // 4. Park变换，得到Id/Iq
    park_transform(i_alpha, i_beta, theta_e, &id_meas, &iq_meas);

    // 5. 电流环PI
    ud = pi_current_d_update(&id_pi, id_ref - id_meas);
    uq = pi_current_q_update(&iq_pi, iq_ref - iq_meas);

    // 6. 电压限幅
    voltage_limit(&ud, &uq, U_MAX);

    // 7. 反Park变换
    inv_park_transform(ud, uq, theta_e, &u_alpha, &u_beta);

    // 8. SVPWM
    svpwm_calc(u_alpha, u_beta, vbus, &duty_u, &duty_v, &duty_w);

    // 9. 更新三相PWM
    pwm_set_duty(duty_u, duty_v, duty_w);
}
```

整体就是：

```text
speed_loop_1khz() 只算 iq_ref
current_loop_20khz() 负责真正输出 PWM
```

---

# 6. 速度 PI 怎么写？

一个基础速度 PI：

```c
typedef struct
{
    float kp;
    float ki;
    float integral;
    float out_max;
    float out_min;
} PI_Controller_t;
```

更新函数：

```c
float pi_speed_update(PI_Controller_t *pi, float err)
{
    float p_out;
    float i_out;
    float out;

    // 比例项
    p_out = pi->kp * err;

    // 积分项
    pi->integral += pi->ki * err;

    // 积分限幅，防止积分越积越大
    if (pi->integral > pi->out_max)
    {
        pi->integral = pi->out_max;
    }
    else if (pi->integral < pi->out_min)
    {
        pi->integral = pi->out_min;
    }

    // PI输出
    i_out = pi->integral;
    out = p_out + i_out;

    // 输出限幅
    if (out > pi->out_max)
    {
        out = pi->out_max;
    }
    else if (out < pi->out_min)
    {
        out = pi->out_min;
    }

    return out;
}
```

初始化：

```c
PI_Controller_t speed_pi =
{
    .kp = 0.01f,
    .ki = 0.0001f,
    .integral = 0.0f,
    .out_max = IQ_MAX,
    .out_min = -IQ_MAX,
};
```

注意：这个参数只是示例，不是最终参数。实际要根据电机、电流单位、速度单位慢慢调。

---

# 7. 速度斜坡怎么写？

不要让目标速度突然跳变。

```c
float ramp_update(float current, float target, float step)
{
    if (current < target)
    {
        current += step;

        if (current > target)
        {
            current = target;
        }
    }
    else if (current > target)
    {
        current -= step;

        if (current < target)
        {
            current = target;
        }
    }

    return current;
}
```

例如 1ms 调用一次：

```c
speed_ref_rpm = ramp_update(speed_ref_rpm, speed_cmd_rpm, 5.0f);
```

意思是：

```text
每 1ms 最多变化 5rpm
1秒最多变化 5000rpm
```

如果你觉得太猛，就改小：

```c
step = 1.0f;  // 每1ms变化1rpm，1秒变化1000rpm
```

---

# 8. 速度反馈计算示例：编码器

假设磁编码器一圈是 0 ~ 4095。

```c
#define ENCODER_CPR 4096.0f
#define SPEED_LOOP_DT 0.001f   // 1ms
```

速度计算：

```c
float motor_get_speed_rpm(void)
{
    static int32_t last_count = 0;

    int32_t now_count;
    int32_t delta;
    float mechanical_rev;
    float rpm;

    now_count = encoder_get_count();

    delta = now_count - last_count;

    // 处理编码器跨圈问题
    if (delta > ENCODER_CPR / 2)
    {
        delta -= ENCODER_CPR;
    }
    else if (delta < -ENCODER_CPR / 2)
    {
        delta += ENCODER_CPR;
    }

    last_count = now_count;

    // 本周期转过的机械圈数
    mechanical_rev = (float)delta / ENCODER_CPR;

    // 转/秒
    float rps = mechanical_rev / SPEED_LOOP_DT;

    // 转/分钟
    rpm = rps * 60.0f;

    return rpm;
}
```

但是直接这样算会有噪声，建议加一阶低通滤波：

```c
float low_pass_filter(float input)
{
    static float y = 0.0f;
    float alpha = 0.1f;

    y = y + alpha * (input - y);

    return y;
}
```

使用：

```c
speed_raw = motor_get_speed_rpm();
speed_meas_rpm = low_pass_filter(speed_raw);
```

---

# 9. 电角度怎么来？

FOC 电流环需要电角度：

```text
theta_e = theta_m * pole_pairs + offset
```

其中：

```text
theta_m：机械角度
pole_pairs：极对数
offset：零位偏移
```

例如：

```c
float get_electrical_angle(void)
{
    float theta_m;
    float theta_e;

    theta_m = encoder_get_mechanical_angle_rad();

    theta_e = theta_m * POLE_PAIRS + encoder_offset;

    theta_e = normalize_angle(theta_e);

    return theta_e;
}
```

如果电角度不准，FOC 会出现：

```text
电流大
没力
抖动
发热
反转
啸叫
```

所以有感 FOC 一定要做零位校准。

---

# 10. 速度环 PI 参数怎么调？

建议顺序：

```text
先调电流环
再调速度环
最后调位置/姿态环
```

不要一上来就调速度环。因为速度环依赖电流环，如果电流环都不稳，速度环必炸。

---

## 第一步：先保证电流环正常

测试：

```text
Id_ref = 0
Iq_ref 给小值，比如 0.2A、0.5A、1A
观察电机是否平滑出力
观察电流是否能跟随
```

如果 Iq_ref 一给，电机疯狂抖：

```text
先别调速度环
先检查电角度、相序、电流采样、Park变换符号
```

---

## 第二步：速度环只开 P，不开 I

先设置：

```c
speed_pi.kp = 小值;
speed_pi.ki = 0;
```

然后给一个小速度，比如：

```text
100rpm
200rpm
500rpm
```

现象：

```text
Kp 太小：速度跟不上，软绵绵
Kp 太大：速度抖动、啸叫、震荡
```

调到电机能比较顺地跟随，但不抖。

---

## 第三步：慢慢加 I

加积分是为了消除稳态误差。

```text
只有P：可能目标1000rpm，实际只能到900rpm
加I：慢慢补到1000rpm
```

但 Ki 太大会：

```text
超调
震荡
速度忽快忽慢
刹不住
```

所以 Ki 要慢慢加。

---

# 11. 闭环速度控制的几个保护必须有

不要只写 PI，保护也很关键。

建议至少有：

```text
1. Iq_ref 限幅
2. speed_ref 斜坡
3. PI 积分限幅
4. 母线电压保护
5. 相电流/母线电流过流保护
6. 堵转保护
7. 速度反馈丢失保护
8. 编码器异常保护
9. MOS/电机温度保护
```

尤其是堵转：

```text
如果 Iq_ref 很大
但是 speed_meas 长时间接近 0
说明可能堵转
必须降电流或停机
```

伪代码：

```c
if ((fabsf(iq_ref) > IQ_STALL_THRESHOLD) &&
    (fabsf(speed_meas_rpm) < SPEED_STALL_THRESHOLD))
{
    stall_time_ms++;

    if (stall_time_ms > 500)
    {
        motor_fault = MOTOR_FAULT_STALL;
        motor_stop();
    }
}
else
{
    stall_time_ms = 0;
}
```

---

# 12. 三相 PWM 在闭环速度控制时是什么样？

闭环速度控制下，三相 PWM 还是高频 PWM。

区别是：

```text
开环：PWM 占空比只根据你“假想的角度/速度”变化
闭环：PWM 占空比根据真实速度误差、电流误差动态修正
```

比如目标速度 1000rpm，负载突然变大。

闭环会这样：

```text
负载变大
    ↓
实际速度下降
    ↓
速度误差变大
    ↓
速度 PI 输出更大的 Iq_ref
    ↓
电流环增加 Uq
    ↓
SVPWM 占空比变大
    ↓
电机力矩变大
    ↓
速度被拉回目标值
```

PWM 上的表现：

```text
负载变大前：
U/V/W 三相 PWM 占空比正常周期变化

负载变大后：
三相 PWM 的调制幅值变大
相电流变大
速度恢复
```

你在示波器上看三相桥臂对地，仍然是：

```text
U相：高频 PWM，占空比不断变化
V相：高频 PWM，占空比相差 120°
W相：高频 PWM，占空比相差 120°
```

真正变化明显的是：

```text
1. 占空比包络幅度变大
2. 相电流幅度变大
3. 速度波形先掉一点，然后恢复
```

---

# 13. 一个最小工程结构建议

你可以这样分层：

```text
motor_control.c

motor_control_init()
motor_control_start()
motor_control_stop()
motor_control_set_speed(float rpm)
motor_control_speed_loop_1khz()
motor_control_current_loop_20khz()
motor_control_fault_check()
```

示例：

```c
typedef struct
{
    float speed_cmd_rpm;       // 用户目标速度
    float speed_ref_rpm;       // 斜坡后的目标速度
    float speed_meas_rpm;      // 实际速度

    float id_ref;
    float iq_ref;

    float id_meas;
    float iq_meas;

    PI_Controller_t speed_pi;
    PI_Controller_t id_pi;
    PI_Controller_t iq_pi;

    uint8_t enable;
    uint8_t fault;
} MotorControl_t;
```

设置速度：

```c
void motor_control_set_speed(float rpm)
{
    motor.speed_cmd_rpm = rpm;
}
```

1kHz 速度环：

```c
void motor_control_speed_loop_1khz(void)
{
    float err;

    if (motor.enable == 0 || motor.fault)
    {
        motor.iq_ref = 0.0f;
        motor.speed_ref_rpm = 0.0f;
        motor.speed_pi.integral = 0.0f;
        return;
    }

    motor.speed_ref_rpm = ramp_update(
        motor.speed_ref_rpm,
        motor.speed_cmd_rpm,
        SPEED_RAMP_STEP_RPM
    );

    motor.speed_meas_rpm = motor_get_speed_rpm_filtered();

    err = motor.speed_ref_rpm - motor.speed_meas_rpm;

    motor.iq_ref = pi_speed_update(&motor.speed_pi, err);

    motor.iq_ref = constrain(motor.iq_ref, -IQ_MAX, IQ_MAX);

    motor.id_ref = 0.0f;
}
```

20kHz 电流环：

```c
void motor_control_current_loop_20khz(void)
{
    if (motor.enable == 0 || motor.fault)
    {
        pwm_set_duty(0.5f, 0.5f, 0.5f);
        return;
    }

    foc_current_loop(
        motor.id_ref,
        motor.iq_ref,
        &motor.id_meas,
        &motor.iq_meas
    );
}
```

---

# 14. 对平衡车来说，速度环不是最外层

如果你是做平衡车，最终不是这样：

```text
目标速度 → 速度环 → 电机
```

而更像这样：

```text
车身倾角目标
    ↓
姿态环
    ↓
速度/位置辅助环
    ↓
输出 Iq_ref
    ↓
FOC 电流环
    ↓
电机力矩
```

平衡车最关键的是让车身站住，所以最核心的是：

```text
角度闭环 / 角速度闭环 / 力矩控制
```

速度环更多是辅助：

```text
不要越跑越远
不要一直漂移
控制前进后退速度
```

所以平衡车里，一般最终输出还是：

```text
Iq_ref
```

不是直接输出速度，也不是直接输出 PWM。

---

# 15. 最终记住这句话

闭环速度控制的实现公式可以简单理解成：

```text
速度误差 = 目标速度 - 实际速度

Iq_ref = Kp × 速度误差 + Ki × 速度误差积分
```

然后：

```text
Iq_ref → 电流环 → SVPWM → 三相 PWM
```

所以你真正要实现的是：

```text
1. 测真实速度
2. 做速度斜坡
3. 速度 PI 算 Iq_ref
4. Iq_ref 限幅
5. 电流环跟踪 Iq_ref
6. SVPWM 输出三相 PWM
```

这就是 BLDC FOC 的闭环速度控制。
