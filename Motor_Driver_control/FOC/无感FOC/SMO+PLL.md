可以。这次我们**不讲“PLL 是一种闭环”这种泛泛的东西**，直接按照你现在的 **ESP32 + FOC + SMO/龙伯格 + 无感位置估计**来讲。

你真正要实现的其实不是通信芯片里那种传统 PLL，而是：

> **PMSM 无感 FOC 的数字 PLL 位置/速度观测器。**

它的任务非常明确：

$$
\boxed{\text{输入一个“带噪声的角度/反电动势方向”}\rightarrow\text{输出平滑、连续的电角度和电角速度}}
$$

Microchip 的 PMSM 无感 FOC 方案也采用 PLL 型位置/速度估计器；基本思想是让估计的转子角度不断跟踪 BEMF 的空间位置。

---

# 1. 先从你现在的 SMO 开始

你现在的链路大概是：

```text
三相电流
   ↓
Clarke
   ↓
Iα Iβ

PWM电压
   ↓
Uα Uβ

       ↓
   ┌────────┐
   │   SMO  │
   └────┬───┘
        ↓
    Eα   Eβ
        ↓
     atan2f()
        ↓
    θ_BEMF
```

你之前就已经做到这里了。

比如：

$$
E_\alpha=2.0V
$$

$$
E_\beta=3.0V
$$

那么：

$$
\theta_{bemf}
=

atan2(E_\beta,E_\alpha)
$$

得到一个角度。

但是这个角度有几个问题：

### 问题 1：有噪声

SMO：

```text
Eα
│
│   /\/\/\/\/\/\
│  /
│ /
└──────────────→
```

所以：

```text
atan2()
↓
θ
```

也会抖。

---

### 问题 2：角度可能突然跳变

例如：

```text
359°
 ↓
0°
 ↓
1°
```

数学上没问题。

但是如果你直接：

```c
error = theta_ref - theta_est;
```

可能突然变成：

```text
0° - 359°
=
-359°
```

实际上真实误差只是：

```text
+1°
```

所以必须处理**角度环绕**。

---

### 问题 3：你想要速度

你可以：

```c
omega = (theta[k] - theta[k-1]) / Ts;
```

但：

> **角度稍微抖一下，微分以后速度会抖得非常厉害。**

例如：

```text
角度噪声：

+0.01°
-0.01°
+0.02°
-0.02°
```

一微分：

```text
+200 rad/s
-400 rad/s
+800 rad/s
...
```

所以：

> **不能简单地 atan2 → 差分 → RPM。**

PLL 就是专门解决这个问题的。

---

# 2. PLL到底在干什么？

先把它想成一个“追踪器”。

假设真正电机角度：

```text
真实角度：

0°
10°
20°
30°
40°
50°
...
```

但是 SMO 算出来：

```text
SMO：

1°
9°
21°
29°
42°
49°
...
```

你想让一个内部角度：

```text
PLL：

0°
9°
20°
30°
40°
50°
...
```

不断追着 SMO 的角度走。

所以 PLL 本质就是：

```text
             SMO估计角度
                  │
                  ↓
              ┌───────┐
              │  比较  │
              └───┬───┘
                  │
                角度误差
                  │
                  ↓
                 PI
                  │
                  ↓
              估计速度
                  │
                  ↓
              积分器
                  │
                  ↓
              估计角度
                  │
                  └─────────┐
                            │
                            └──反馈比较
```

这就是你真正要实现的东西。

传统 PLL 的基本结构也是“相位检测器 → 环路滤波器 → 振荡器 → 反馈”，锁定后输出频率跟踪输入，而相位差保持为有限的常数。

---

# 3. 但是电机PLL和普通PLL有个区别

普通 PLL：

```text
输入正弦波
     ↓
相位检测器
     ↓
误差
     ↓
滤波器
     ↓
VCO
```

电机数字 PLL：

```text
BEMF / 估计角度
       ↓
相位误差检测
       ↓
PI
       ↓
ω_est
       ↓
积分
       ↓
θ_est
```

所以你可以把：

> **PI + 角度积分器**

理解成电机 FOC 里的“数字 VCO”。

---

# 4. 为什么“速度”是PLL里面的核心？

因为：

$$
\omega=\frac{d\theta}{dt}
$$

反过来：

$$
\theta=\int\omega dt
$$

所以：

```text
速度
 ↓
积分
 ↓
角度
```

这件事情非常重要。

PLL 不直接相信：

```text
atan2() 得到的角度
```

而是自己维护一个：

```c
theta_est
```

然后通过：

```c
omega_est
```

不断积分：

```c
theta_est += omega_est * Ts;
```

因此它天然会产生：

```text
θ_est
ω_est
```

也就是：

> **角度 + 速度同时得到。**

---

# 5. 最简单的数字PLL

先不管 BEMF，假设你已经有：

```c
theta_meas
```

比如 SMO：

```c
theta_meas = atan2f(Ebeta, Ealpha);
```

那么最简单的 PLL：

```c
error = angle_error(theta_meas - theta_est);

omega_est += Ki * error * Ts;

theta_est += omega_est * Ts;
```

但是这还不是完整的 PLL。

通常我们会加比例项：

```c
omega_est = omega_est
          + Kp * error
          + integral;
```

更标准地写：

$$
\omega_{est}
=
K_p e_\theta
+
\xi
$$

$$
\dot{\xi}=K_i e_\theta
$$

然后：

$$
\dot{\theta}*{est}=\omega*{est}
$$

离散：

$$
\xi[k+1]
=
\xi[k]+K_iT_se_\theta[k]
$$

$$
\omega[k]
=

K_pe_\theta[k]+\xi[k]
$$

$$
\theta[k+1]
=

\theta[k]+\omega[k]T_s
$$

这就是一个非常经典的**二阶 Type-II 数字 PLL**结构。

---

# 6. 为什么这里一定要 PI？

这是重点。

假设没有积分：

$$
\omega=K_pe
$$

电机真实速度：

```text
100 rad/s
```

如果 PLL 要维持：

```text
ω_est = 100 rad/s
```

那么必须：

$$
e=\frac{100}{K_p}
$$

也就是说：

> **必须一直存在一个角度误差，才能产生正确的速度。**

例如：

```text
Kp = 100

ω = 100 rad/s

e = 1 rad
```

PLL 最后可能变成：

```text
θ_real
   ↑
   │ 1rad
   │
θ_est
```

这叫：

### 稳态相位误差

而加了积分以后：

$$
\dot{\xi}=K_i e
$$

积分器可以不断累积误差，直到产生：

```text
omega_est = omega_real
```

这时候：

$$
e_\theta\rightarrow0
$$

所以：

> **PI 的积分项负责消除稳态相位误差。**

---

# 7. 这就形成了完整闭环

现在你把它画成：

```text
                    ┌─────────────────────┐
                    │                     │
                    │       PLL           │
                    │                     │
SMO θ_meas ──→ (+) ├→ angle error → PI ─→ ω_est
               ↑    │                     │
               │    │                     ↓
               │    │                  积分器
               │    │                     │
               │    │                     ↓
               └────┤                  θ_est
                    │                     │
                    └─────────────────────┘
```

你应该能看出来：

### PLL里面其实有两个积分概念：

第一个：

```text
PI 的 I 项
```

负责：

> 修正速度。

第二个：

```text
ω → 积分 → θ
```

负责：

> 从速度得到角度。

这也是为什么它是一个二阶系统。

---

# 8. 用你现在的电机举例

假设你的电机：

```text
p = 7
```

当前机械速度：

```text
1000 RPM
```

机械角速度：

$$
\omega_m
=

1000\frac{2\pi}{60}
$$

约：

$$
104.72 rad/s
$$

电角速度：

$$
\omega_e
=

p\omega_m
$$

所以：

$$
\omega_e
=

7\times104.72
$$

约：

$$
733.04rad/s
$$

那么 PLL 最终应该输出：

```text
omega_e ≈ 733 rad/s
```

然后：

```c
theta_e += omega_e * Ts;
```

你的：

```text
Ts = 50 us
```

所以每个 20kHz 周期：

$$
\Delta\theta
=

733\times50us
$$

约：

$$
0.03665rad
$$

换成角度：

$$
2.10°
$$

也就是说：

> **1000 RPM 时，你的电角度每个 20 kHz FOC 周期大约前进 2.1°。**

这对于你理解 PLL 非常重要。

---

# 9. 但是我们为什么不能直接把 atan2 当 θ_meas？

因为：

```text
SMO
 ↓
Eα Eβ
 ↓
atan2
 ↓
θ_meas
```

可能是：

```text
30.2°
30.8°
29.7°
31.1°
30.0°
```

而真实：

```text
30°
```

如果你直接把这个角度给 FOC：

```text
Park(theta_meas)
```

那么电角度会不断抖。

于是：

```text
Iq
Id
Torque
```

都会抖。

所以 PLL：

```text
SMO
 ↓
θ_meas
 ↓
PLL
 ↓
θ_est
 ↓
Park
```

相当于：

> **用一个动态系统把 noisy angle 变成平滑的连续 angle。**

SMO + PLL 用于 PMSM 无感位置速度估计也是很常见的组合。

---

# 10. 但是还有一个更专业的做法

对于你这种：

```text
SMO → BEMF → PLL
```

**我反而不推荐：**

```text
atan2(Eβ,Eα)
↓
角度
↓
PLL
```

作为第一选择。

更推荐：

```text
Eα
Eβ
 ↓
根据 PLL 当前 θ_est
 ↓
逆/正Park
 ↓
Ed
Eq
 ↓
用 Ed 做误差
 ↓
PI
 ↓
ω_est
 ↓
积分
 ↓
θ_est
```

这才是非常典型的**电机 BEMF PLL**。

Microchip 的 PMSM PLL estimator 就利用了一个非常关键的事实：

> 在正确的转子磁链坐标系下，稳态时 BEMF 的 d 轴分量应该为 0。

所以把 BEMF 转到估计的 dq 坐标系后，可以利用 `Ed` 作为 PLL 的误差信号。

这个方法你一定要理解。

---

# 11. 为什么 Ed 可以作为误差？

假设你的 PLL 估计角度：

```text
θ_est
```

刚好等于真实转子角度：

```text
θ_real
```

那么 BEMF 正好落在 q 轴：

```text
               q
               ↑
               │
               │    E
               │    ↑
               │    │
───────────────┼────────→ d
               │
```

所以：

$$
E_d=0
$$

而：

$$
E_q=E
$$

---

如果 PLL 角度错了：

```text
              q_est
                ↑
              ／│
            ／  │ E
          ／    │
─────────┼──────┼──────→ d_est
```

那么 BEMF 就会出现：

$$
E_d\neq0
$$

所以：

> **Ed 就是“我当前角度估错了多少”的一个指标。**

---

# 12. 这个思路非常漂亮

因为你根本不需要：

```text
atan2()
```

你只需要：

```text
当前估计角度 θ_est
+
BEMF αβ
```

然后：

$$
E_d
=

E_\alpha\cos\theta
+
E_\beta\sin\theta
$$

$$
E_q
=

-E_\alpha\sin\theta
+
E_\beta\cos\theta
$$

于是：

```text
Eα Eβ
 │
 ↓
Park(θ_est)
 │
 ├──── Ed
 │
 └──── Eq
       │
       ↓
```

其中：

```text
Ed
```

就是 PLL 的误差信号。

---

# 13. 整个 BEMF-PLL 就出来了

```text
                 SMO / ROLO
                     │
                Eα       Eβ
                 │       │
                 └───┬───┘
                     ↓
               Park(θ_est)
                     │
                 ┌───┴───┐
                 ↓       ↓
                Ed      Eq
                 │
                 ↓
              PLL误差
                 │
                 ↓
                PI
                 │
                 ↓
              ω_est
                 │
                 ↓
               积分
                 │
                 ↓
              θ_est
                 │
                 └─────────────┐
                               │
                               ↓
                         Park(θ_est)
```

**这才是我最推荐你实现的版本。**

---

# 14. 那 Ed 到底正负代表什么？

假设：

$$
E_d>0
$$

说明：

```text
θ_est
```

相对于真实角度偏了一边。

PI：

```text
Ed
 ↓
Kp
 ↓
ω correction
```

如果：

$$
E_d<0
$$

就往另外一个方向纠正。

所以：

```text
Ed > 0
 ↓
ω_est 增大/减小
 ↓
θ_est 加快/减慢
 ↓
Ed ↓
```

最终：

$$
E_d\rightarrow0
$$

这就是“锁相”。

---

# 15. 这里特别容易出现一个坑

**正负方向与你的 Park 定义有关。**

比如你使用：

$$
I_d=I_\alpha\cos\theta+I_\beta\sin\theta
$$

$$
I_q=-I_\alpha\sin\theta+I_\beta\cos\theta
$$

那么：

$$
E_d=E_\alpha\cos\theta+E_\beta\sin\theta
$$

如果你的 Clarke、Park、BEMF 定义换了：

```text
αβ方向
电机旋转正方向
Eα/Eβ定义
Park正负号
```

那么：

```text
Ed
```

的正负关系可能就变了。

所以 PLL 调试时：

> **如果发现 PLL 越调越跑飞，第一件事不是改 Kp/Ki，而是检查误差符号。**

---

# 16. PLL代码到底怎么写？

我们先写一个工程上很容易理解的版本。

```c
typedef struct
{
    float theta;
    float omega;

    float integral;

    float kp;
    float ki;

    float ts;

} pll_t;
```

初始化：

```c
void PLL_Init(pll_t *pll,
              float kp,
              float ki,
              float ts)
{
    pll->theta   = 0.0f;
    pll->omega   = 0.0f;
    pll->integral = 0.0f;

    pll->kp = kp;
    pll->ki = ki;

    pll->ts = ts;
}
```

---

# 17. 角度型PLL实现

如果你已经有：

```c
theta_bemf
```

那么：

```c
void PLL_Update(pll_t *pll,
                float theta_ref)
{
    float err;

    err = theta_ref - pll->theta;

    // wrap到[-PI, PI]
    if (err > M_PI)
        err -= 2.0f * M_PI;

    if (err < -M_PI)
        err += 2.0f * M_PI;

    // PI积分
    pll->integral +=
        pll->ki * err * pll->ts;

    // 估计电角速度
    pll->omega =
        pll->kp * err +
        pll->integral;

    // 积分得到角度
    pll->theta +=
        pll->omega * pll->ts;

    // angle wrap
    if (pll->theta >= 2.0f * M_PI)
        pll->theta -= 2.0f * M_PI;

    if (pll->theta < 0.0f)
        pll->theta += 2.0f * M_PI;
}
```

这个版本非常容易理解。

---

# 18. 但是你真正做无感FOC，我更推荐这个

直接用：

```text
Eα
Eβ
θ_est
```

算：

```text
Ed
Eq
```

然后：

```c
void PLL_Update(
    pll_t *pll,
    float e_alpha,
    float e_beta)
{
    float sin_theta;
    float cos_theta;

    float e_d;

    sin_theta = sinf(pll->theta);
    cos_theta = cosf(pll->theta);

    // Park
    e_d =
        e_alpha * cos_theta +
        e_beta  * sin_theta;

    // PLL PI
    pll->integral +=
        pll->ki * e_d * pll->ts;

    pll->omega =
        pll->integral +
        pll->kp * e_d;

    // angle integration
    pll->theta +=
        pll->omega * pll->ts;

    // wrap
    if (pll->theta >= TWO_PI)
        pll->theta -= TWO_PI;

    if (pll->theta < 0.0f)
        pll->theta += TWO_PI;
}
```

这就是一个非常典型的：

> **BEMF-based PLL**

---

# 19. 但是这个版本还差一个很重要的东西

你现在的：

```c
e_d
```

单位是：

```text
Volt
```

而：

```c
kp
ki
```

的单位不是随便的。

如果直接：

```c
omega = kp * Ed;
```

那么：

$$
K_p
$$

实际上要承担：

$$
rad/s/V
$$

的单位。

这不是不能做。

但是调参会比较麻烦。

---

# 20. 更常见的方法：归一化

例如：

$$
e=
\frac{E_d}{E_{bemf}}
$$

或者：

$$
e=\frac{E_d}{K_e}
$$

这样误差就更接近一个：

```text
无量纲角度误差
```

然后 PLL 的参数更容易设计。

当然具体工程实现会根据 BEMF 估计方式不同而变化。

---

# 21. PLL参数到底怎么计算？

这个才是最重要的调参部分。

PLL 的线性化模型可以近似成：

$$
G(s)
=

\frac{K_p s+K_i}
{s^2+K_p s+K_i}
$$

因此标准二阶系统：

$$
s^2+2\zeta\omega_n s+\omega_n^2
$$

对比得到：

$$
\boxed{
K_p=2\zeta\omega_n
}
$$

$$
\boxed{
K_i=\omega_n^2
}
$$

其中：

* $\omega_n$：自然频率
* $\zeta$：阻尼比

通常可以先取：

$$
\zeta=0.707
$$

这是非常经典的选择。

---

# 22. 例如你的20kHz FOC

假设我们先想让 PLL 带宽：

```text
100 Hz
```

那么：

$$
\omega_n=2π100
$$

约：

$$
628.3rad/s
$$

取：

$$
\zeta=0.707
$$

那么：

$$
K_p
=

2\times0.707\times628.3
$$

约：

$$
888
$$

而：

$$
K_i
=

628.3^2
$$

约：

$$
394784
$$

所以可以先试：

```c
pll.kp = 888.0f;
pll.ki = 394784.0f;
```

**但这里有个前提：**

这是针对你采用的**归一化/单位定义和具体 PLL 方程**的理论值，不能把这个数字无脑塞进任意 BEMF PLL。不同实现的误差定义、增益、归一化方式不同，参数会跟着变化。

---

# 23. 为什么PLL带宽不能太大？

比如你：

```text
PLL带宽 = 3000Hz
```

你的 BEMF：

```text
SMO
 ↓
有噪声
```

PLL：

```text
3000Hz
 ↓
疯狂跟踪
```

结果：

```text
噪声
 ↓
PLL
 ↓
ω_est 抖
 ↓
θ_est 抖
 ↓
Park 抖
 ↓
Iq 抖
 ↓
Torque 抖
```

所以：

> **PLL不是越快越好。**

---

# 24. PLL带宽太小又怎么样？

例如：

```text
PLL BW = 5Hz
```

电机突然：

```text
500RPM
→
2000RPM
```

PLL：

```text
慢慢跟
```

结果：

```text
真实 θ
      /
     /
    /
   /
PLL θ
  /
 /
```

会产生明显的：

### 相位滞后

于是 FOC 使用的：

```text
theta_est
```

和真实转子角度差很多。

然后：

```text
Id/Iq
```

就会受到影响。

---

# 25. 所以PLL调参就是：

```text
PLL带宽太低
 ↓
跟踪慢
 ↓
相位滞后

PLL带宽太高
 ↓
跟踪快
 ↓
噪声大
```

所以你要找到：

```text
                最佳点
                  ●
                /   \
             噪声   跟踪
```

这个平衡。

环路滤波器/带宽本身就是 PLL 动态响应和噪声抑制之间的重要权衡。

---

# 26. 你这个20kHz系统，我建议先怎么做？

你的 FOC：

$$
f_s=20kHz
$$

不要一开始把 PLL 搞得特别快。

可以先从：

```text
PLL bandwidth：
50~200 Hz
```

这个数量级开始实验。

然后观察：

```text
θ_est
ω_est
Ed
Eq
```

如果：

```text
ω_est 很抖
```

降低 PLL 带宽。

如果：

```text
θ_est 明显跟不上
```

提高 PLL 带宽。

---

# 27. PLL和FOC频率也不要混淆

你现在：

```text
FOC = 20kHz
```

不代表：

```text
PLL带宽 = 20kHz
```

完全不是一个概念。

例如：

```text
FOC计算频率
20kHz

PLL带宽
100Hz

电机电角频率
500Hz
```

这些完全是不同东西。

---

# 28. 你这个7对极电机特别要注意“机械速度 vs 电角速度”

你的：

$$
p=7
$$

所以：

$$
\omega_e=7\omega_m
$$

比如：

### 100 RPM

$$
\omega_m=10.47rad/s
$$

$$
\omega_e=73.3rad/s
$$

### 1000 RPM

$$
\omega_e=733rad/s
$$

### 3000 RPM

$$
\omega_e=2199rad/s
$$

所以 PLL 输出如果用于：

```text
Park
```

应该是：

$$
\boxed{\omega_e}
$$

不是机械：

$$
\omega_m
$$

---

# 29. 最终你整个无感FOC应该是这个结构

这张图你可以重点记：

```text
                    ┌──────────────────────┐
                    │       PMSM           │
                    └──────────┬───────────┘
                               │
                       Ia Ib / Uα Uβ
                               │
                               ↓
                         ┌──────────┐
                         │   SMO    │
                         └────┬─────┘
                              │
                           Eα Eβ
                              │
                              ↓
                    ┌──────────────────┐
                    │      PLL         │
                    │                  │
                    │ Eα Eβ            │
                    │   ↓              │
                    │ Park(θ_est)      │
                    │   ↓              │
                    │  Ed              │
                    │   ↓              │
                    │  PI              │
                    │   ↓              │
                    │ ωe_est           │
                    │   ↓              │
                    │ ∫                │
                    │   ↓              │
                    │ θe_est           │
                    └────────┬─────────┘
                             │
                             ↓
                           Park
                             │
                       ┌─────┴─────┐
                       ↓           ↓
                      Id          Iq
                       │           │
                      PI          PI
                       │           │
                       └─────┬─────┘
                             ↓
                           SVPWM
                             ↓
                           电机
```

---

# 30. 你现在最容易犯的三个错误

### 错误一：把 PLL 当成低通滤波器

不是。

PLL 不只是：

```text
theta
 ↓
LPF
```

它实际上是：

```text
相位误差
 ↓
PI
 ↓
速度估计
 ↓
积分
 ↓
角度
 ↓
反馈
```

它是一个**动态闭环系统**。

---

### 错误二：直接微分 atan2 得速度

不要：

```c
theta = atan2f(...);

omega =
    (theta - theta_last) / Ts;
```

这对噪声非常敏感。

PLL 的意义之一就是：

> **通过闭环估计得到速度，而不是对 noisy angle 直接微分。**

---

### 错误三：Ed符号弄反

如果你发现：

```text
Ed > 0
```

然后 PLL：

```text
ω越来越大
θ越来越偏
Ed越来越大
```

说明你的反馈方向是**正反馈**。

正确应该：

```text
Ed
 ↓
PI
 ↓
θ修正
 ↓
Ed减小
```

所以第一次调 PLL：

> **一定先低速、小 Uq、看 Ed 的正负和 θ 是否朝正确方向收敛。**

---

# 31. 结合你之前的SMO，我建议你现在这么实现

你的第一版不要搞复杂。

### 第一步

SMO：

```text
Uα Uβ
Iα Iβ
 ↓
SMO
 ↓
Eα Eβ
```

### 第二步

先不要 PLL，验证：

```text
Eα
Eβ
```

是否是干净的旋转矢量。

### 第三步

实现：

```c
Ed = Ealpha*cos(theta)
   + Ebeta*sin(theta);
```

### 第四步

实现：

```c
integral += Ki * Ed * Ts;

omega = Kp * Ed + integral;
```

### 第五步

实现：

```c
theta += omega * Ts;
```

### 第六步

把：

```c
theta
```

送给：

```text
Park
```

### 第七步

再观察：

```text
Ed
Eq
theta
omega
```

---

# 32. 最后把PLL压缩成一句你能记住的话

对于你现在的无感 FOC：

> **SMO/龙伯格负责“看见”反电动势；PLL负责“跟住”反电动势的旋转方向。**

更具体：

$$
\boxed{
E_{\alpha,\beta}
\rightarrow
Park(\hat\theta)
\rightarrow
E_d
\rightarrow
PI
\rightarrow
\hat\omega_e
\rightarrow
\int
\rightarrow
\hat\theta_e
}
$$

然后：

$$
\boxed{
\hat\theta_e
\rightarrow
FOC\ Park
}
$$

而 PLL 锁定以后：

$$
E_d\rightarrow0
$$

同时：

$$
\hat\omega_e\rightarrow\omega_e
$$

以及：

$$
\hat\theta_e\rightarrow\theta_e
$$

这就是**“锁相”真正的数学含义**。Microchip 的 PMSM PLL estimator 正是利用 BEMF 在正确转子坐标系下 d 轴分量趋近于零这一特性来形成闭环。

**你下一步最值得搞明白的是：为什么 `Ed` 能代表“角度误差”，以及从 PMSM 的 `Eα/Eβ` 方程严格推导出 `Ed ≈ E·sin(θ真实−θ估计)`，再由这个式子推导 PLL 的 Kp、Ki。**这一层一旦推通，你就不只是“会用 PLL”，而是能自己设计 PLL。

-----------------------
------------------------
----------------------------


可以。我们这次**完全不讲龙伯格**，就研究你现在最直接的这条链：

$$
\boxed{\text{SMO} \rightarrow \text{PLL} \rightarrow \theta_e,\omega_e}
$$

而且我会按照你现在的 **ESP32、20 kHz FOC、2208、7 对极、SMO 已经能算出 (E_\alpha/E_\beta)** 来讲。

你真正要实现的是：

```text
                    ┌───────────────┐
                    │    电机       │
                    └───────┬───────┘
                            │
                      Ia Ib / Uα Uβ
                            │
                            ↓
                      ┌─────────┐
                      │   SMO   │
                      └────┬────┘
                           │
                     Eα_est Eβ_est
                           │
                           ↓
                    ┌─────────────┐
                    │     PLL     │
                    └──────┬──────┘
                           │
                    ┌──────┴──────┐
                    ↓             ↓
                  θe_est        ωe_est
                    │             │
                    ↓             ↓
                  Park          RPM
```

**SMO负责估计反电动势，PLL负责从反电动势中锁定转子电角度和电角速度。**

---

# 一、先搞清楚：SMO到底给PLL什么？

你现在 SMO 的输出应该类似：

```c
float Ealpha;
float Ebeta;
```

也就是：

$$
\hat E_\alpha
$$

$$
\hat E_\beta
$$

把它们想象成一个二维向量：

```text
                β
                ↑
                │       E
                │      ↗
                │    ↗
                │  ↗
────────────────┼────────────→ α
```

这个反电动势矢量一直随着转子旋转。

所以：

> **SMO实际上已经告诉了你“反电动势矢量现在指向哪里”。**

---

# 二、最简单的方法：SMO → atan2 → PLL

第一版最容易理解的方法：

$$
\boxed{
\theta_{bemf}
=

atan2(E_\beta,E_\alpha)
}
$$

代码：

```c
theta_bemf = atan2f(Ebeta, Ealpha);
```

比如：

```text
Eα = 3V
Eβ = 3V
```

那么：

$$
\theta_{bemf}=45°
$$

再比如：

```text
Eα = -3V
Eβ = 3V
```

得到：

$$
135°
$$

所以：

```text
Eα/Eβ
 ↓
atan2
 ↓
BEMF角度
```

这一步你之前已经在做。

---

# 三、但是这个角度不是转子磁链角度

这是你现在必须搞清楚的。

PMSM 反电动势和磁链之间相差：

$$
90°
$$

假设：

$$
E_\alpha=-E\sin\theta_e
$$

$$
E_\beta=E\cos\theta_e
$$

那么：

$$
atan2(E_\beta,E_\alpha)
$$

得到：

$$
\theta_{bemf}
=

\theta_e+\frac{\pi}{2}
$$

因此：

$$
\boxed{
\theta_e
=

\theta_{bemf}-\frac{\pi}{2}
}
$$

所以第一版：

```c
theta_bemf = atan2f(Ebeta, Ealpha);

theta_e = theta_bemf - M_PI_2;
```

但是！

**你实际代码里的正负号必须根据你自己的 Clarke、Park、SMO 电压方程和电机旋转方向验证。**

你之前就遇到过：

```text
+Uq → 一个方向
-Uq → 另一个方向
```

所以不能直接死记 `-PI/2`。

---

# 四、为什么有了 atan2 还需要PLL？

这是关键。

假设真实电角度：

```text
100°
101°
102°
103°
104°
```

SMO算出来：

```text
100.8°
100.2°
102.7°
102.1°
104.9°
```

你直接：

```c
theta = atan2f(Ebeta, Ealpha);
```

然后送给：

```c
Park(theta);
```

那么 FOC 的角度就会：

```text
100.8
100.2
102.7
102.1
104.9
```

不断抖。

而且如果：

```text
359°
→
0°
```

还存在角度跳变。

---

# 五、速度更加不能直接差分

很多新手会这么写：

```c
theta_now = atan2f(Ebeta, Ealpha);

omega =
    (theta_now - theta_last) / Ts;
```

这是非常容易炸的。

因为：

```text
θ噪声
 ↓
微分
 ↓
噪声被放大
```

比如角度噪声：

$$
±0.01rad
$$

而你的：

$$
T_s=50us
$$

那么微分噪声量级：

$$
\frac{0.01}{50\times10^{-6}}
=

200rad/s
$$

你看：

> **角度只有 0.01 rad 的噪声，微分以后就能变成 200 rad/s。**

所以你之前如果发现：

```text
SMO角度看起来还行
但是RPM非常抖
```

完全正常。

PLL就是解决这个问题的。

---

# 六、PLL到底接在哪里？

正确的第一版结构：

```text
SMO
 │
 ├── Eα
 └── Eβ
      ↓
    atan2
      ↓
 θ_bemf
      ↓
 ± π/2
      ↓
 θ_measure
      ↓
 ┌──────────────┐
 │     PLL      │
 │              │
 │  θ_measure   │
 │      ↓       │
 │  angle error │
 │      ↓       │
 │     PI       │
 │      ↓       │
 │   ω_est      │
 │      ↓       │
 │    积分      │
 │      ↓       │
 │   θ_est      │
 └──────┬───────┘
        │
        ├── θ_est
        └── ω_est
```

---

# 七、PLL第一部分：相位检测器

PLL首先比较：

```text
θ_measure
```

和：

```text
θ_est
```

所以：

$$
e_\theta
=
\theta_{measure}*
\theta_{est}
$$

但是这里一定要做：

## 角度 wrap

例如：

```text
θ_measure = 1°
θ_est = 359°
```

直接减：

$$
1-359=-358°
$$

这是错的。

真正的最短角度误差应该是：

$$
+2°
$$

所以：

```c
float AngleWrap(float angle)
{
    while (angle > M_PI)
        angle -= 2.0f * M_PI;

    while (angle < -M_PI)
        angle += 2.0f * M_PI;

    return angle;
}
```

然后：

```c
error =
    AngleWrap(theta_measure - theta_est);
```

---

# 八、PLL第二部分：PI

得到：

$$
e_\theta
$$

之后：

$$
e_\theta
\rightarrow
PI
$$

PI：

$$
u=K_pe+K_i\int e,dt
$$

在 PLL 里面，这个 `u` 就不是普通电流环的电压。

而是：

$$
\boxed{\omega_e}
$$

也就是：

> **估计电角速度。**

所以：

$$
\omega_e
=

K_pe_\theta
+
K_i\int e_\theta dt
$$

---

# 九、为什么PI输出的是速度？

假设：

```text
真实电机
ω = 500 rad/s
```

而 PLL：

```text
ω_est = 400 rad/s
```

那么：

```text
θ_est
```

会越来越落后。

于是：

```text
θ_measure - θ_est
```

越来越大。

PLL：

```text
误差
 ↓
PI
 ↓
ω_est 增大
 ↓
θ_est追上去
```

所以：

> **PLL实际上通过角度误差来调节速度。**

---

# 十、第三部分：积分器

有了：

$$
\omega_e
$$

就可以得到：

$$
\theta_e
$$

因为：

$$
\frac{d\theta_e}{dt}=\omega_e
$$

所以：

$$
\boxed{
\theta_e(k+1)
=

\theta_e(k)+
\omega_e(k)T_s
}
$$

代码：

```c
pll->theta +=
    pll->omega * pll->Ts;
```

这就是 PLL 里面最关键的“振荡器”。

---

# 十一、所以PLL完整数学模型就是这三步

## ① 相位误差

$$
e_\theta
=

wrap(\theta_{measure}-\theta_{est})
$$

## ② PI产生速度

$$
\omega_{est}
=

K_pe_\theta+
I
$$

其中：

$$
I[k+1]
=

I[k]+K_iT_se_\theta
$$

## ③ 速度积分产生角度

$$
\theta_{est}[k+1]
=
\theta_{est}[k]
+
\omega_{est}T_s
$$

所以：

$$
\boxed{
\theta_{measure}
\rightarrow
error
\rightarrow
PI
\rightarrow
\omega
\rightarrow
integrator
\rightarrow
\theta
}
$$

这就是你真正要写的 PLL。

---

# 十二、直接给你一个第一版可运行的PLL

```c
typedef struct
{
    float theta;       // 估计电角度 rad
    float omega;       // 估计电角速度 rad/s

    float integral;    // PI积分项

    float kp;
    float ki;

    float Ts;

} pll_t;
```

初始化：

```c
void PLL_Init(pll_t *pll,
              float kp,
              float ki,
              float Ts)
{
    pll->theta = 0.0f;
    pll->omega = 0.0f;
    pll->integral = 0.0f;

    pll->kp = kp;
    pll->ki = ki;

    pll->Ts = Ts;
}
```

角度 wrap：

```c
static float Angle_Wrap_PI(float angle)
{
    if (angle > M_PI)
        angle -= 2.0f * M_PI;

    else if (angle < -M_PI)
        angle += 2.0f * M_PI;

    return angle;
}
```

PLL：

```c
void PLL_Update(pll_t *pll,
                float theta_measure)
{
    float error;

    // 1. 相位误差
    error = theta_measure - pll->theta;

    // 2. wrap到[-PI, PI]
    error = Angle_Wrap_PI(error);

    // 3. PI积分
    pll->integral +=
        pll->ki *
        error *
        pll->Ts;

    // 4. 得到电角速度
    pll->omega =
        pll->kp * error +
        pll->integral;

    // 5. 速度积分得到角度
    pll->theta +=
        pll->omega *
        pll->Ts;

    // 6. 角度wrap到[0, 2PI)
    if (pll->theta >= 2.0f * M_PI)
        pll->theta -= 2.0f * M_PI;

    else if (pll->theta < 0.0f)
        pll->theta += 2.0f * M_PI;
}
```

---

# 十三、你的20kHz FOC里面怎么调用？

你现在：

$$
T_s=\frac1{20000}=50us
$$

所以：

```c
#define FOC_FREQ    20000.0f
#define FOC_TS      (1.0f / FOC_FREQ)
```

然后：

```c
PLL_Init(
    &pll,
    Kp,
    Ki,
    FOC_TS
);
```

20 kHz FOC循环：

```c
void FOC_Loop(void)
{
    // 1. ADC
    Read_Current();

    // 2. Clarke
    Clarke();

    // 3. 得到Ualpha/Ubeta
    ...

    // 4. SMO
    SMO_Update(
        &smo,
        Ualpha,
        Ubeta,
        Ialpha,
        Ibeta
    );

    // 5. SMO输出BEMF
    float Ealpha = smo.Ealpha;
    float Ebeta  = smo.Ebeta;

    // 6. BEMF角度
    float theta_bemf =
        atan2f(Ebeta, Ealpha);

    // 7. BEMF角度 → 转子磁链角度
    float theta_measure =
        theta_bemf - M_PI_2;

    // 8. PLL
    PLL_Update(
        &pll,
        theta_measure
    );

    // 9. PLL输出
    float theta_e =
        pll.theta;

    float omega_e =
        pll.omega;

    // 10. Park
    Park(
        Ialpha,
        Ibeta,
        theta_e
    );

    // 11. Id/Iq PI
    ...

    // 12. SVPWM
    ...
}
```

这就是完整的：

$$
\boxed{
SMO+PLL
}
$$

---

# 十四、但是这里有一个非常重要的问题

上面这个版本虽然好理解，但：

```text
SMO
 ↓
atan2
 ↓
PLL
```

存在一个问题：

### atan2本身会产生角度噪声。

更高级、更适合电机控制的方式是：

```text
SMO
 ↓
Eα Eβ
 ↓
根据PLL当前θ
 ↓
Park
 ↓
Ed
 ↓
PI
 ↓
ω
 ↓
积分
 ↓
θ
```

也就是：

> **不把 atan2 得到的角度作为 PLL 输入，而是直接把 BEMF 的 d轴分量作为 PLL 的相位误差。**

这个方法我非常建议你最终采用。

---

# 十五、为什么Ed就是相位误差？

这个地方我们认真推导。

假设真实电角度：

$$
\theta_e
$$

反电动势：

$$
E_\alpha=-E\sin\theta_e
$$

$$
E_\beta=E\cos\theta_e
$$

PLL当前估计：

$$
\hat\theta_e
$$

Park：

$$
E_d
=

E_\alpha\cos\hat\theta_e
+
E_\beta\sin\hat\theta_e
$$

代进去：

$$
E_d
=

-E\sin\theta_e\cos\hat\theta_e
+
E\cos\theta_e\sin\hat\theta_e
$$

利用：

$$
\sin(A-B)
=
\sin A\cos B
-
\cos A\sin B
$$

得到：

$$
E_d
=

-E\sin(\theta_e-\hat\theta_e)
$$

也就是说：

$$
\boxed{
E_d
=

-E\sin(\theta_e-\hat\theta_e)
}
$$

如果误差很小：

$$
\sin x\approx x
$$

所以：

$$
\boxed{
E_d
\approx
-E(\theta_e-\hat\theta_e)
}
$$

这就非常漂亮了。

因为：

> **Ed 与角度误差成正比。**

所以：

```text
Ed
 ↓
就是PLL的相位误差信号
```

---

# 十六、因此推荐你的最终PLL结构

你的 SMO：

```text
Uα Uβ
Iα Iβ
 ↓
SMO
 ↓
Eα Eβ
```

然后：

```text
              Eα Eβ
                │
                ↓
         Park(θ_PLL)
                │
           ┌────┴────┐
           ↓         ↓
          Ed        Eq
           │
           ↓
        PLL PI
           │
           ↓
         ω_est
           │
           ↓
       积分器
           │
           ↓
         θ_PLL
           │
           └──────────┐
                      │
                      ↓
                 Park反馈
```

这样：

### 不需要：

```text
atan2
```

作为 PLL 的核心输入。

---

# 十七、代码就是这样

```c
void PLL_Update_BEMF(
    pll_t *pll,
    float Ealpha,
    float Ebeta)
{
    float sin_theta;
    float cos_theta;

    float Ed;

    sin_theta = sinf(pll->theta);
    cos_theta = cosf(pll->theta);

    // Park变换
    Ed =
        Ealpha * cos_theta +
        Ebeta  * sin_theta;

    // PI积分
    pll->integral +=
        pll->ki *
        Ed *
        pll->Ts;

    // 电角速度
    pll->omega =
        pll->kp * Ed +
        pll->integral;

    // 积分得到电角度
    pll->theta +=
        pll->omega *
        pll->Ts;

    // wrap
    if (pll->theta >= TWO_PI)
        pll->theta -= TWO_PI;

    else if (pll->theta < 0.0f)
        pll->theta += TWO_PI;
}
```

不过注意：

**`Ed` 的符号如果和你的 SMO/Clarke/Park 定义相反，就需要把误差反馈符号取反。**

例如：

```c
pll->omega =
    -(pll->kp * Ed + pll->integral);
```

到底用正还是负，必须通过你的坐标定义验证。

---

# 十八、为什么这个方法比 atan2 更适合你？

因为：

### atan2方法

```text
Eα Eβ
 ↓
atan2
 ↓
θ
 ↓
PLL
```

中间多了：

```text
非线性 atan2
```

而且 `Eα/Eβ` 很小时：

```text
噪声
 ↓
atan2
 ↓
角度乱跳
```

---

### BEMF-PLL

```text
Eα Eβ
 ↓
Park(θ_est)
 ↓
Ed
 ↓
PI
```

它只关心：

> **BEMF 有没有完全落在 q轴上。**

如果：

```text
Ed = 0
```

就说明：

```text
PLL角度 ≈ 正确角度
```

所以这是一种非常自然的闭环。

---

# 十九、但是SMO + PLL的完整启动过程怎么办？

这个对你非常重要。

因为：

$$
E=K_e\omega
$$

电机：

```text
0 RPM
 ↓
BEMF ≈ 0
```

所以：

```text
SMO
 ↓
没有足够BEMF
 ↓
PLL无法可靠锁相
```

因此不能：

```text
0 RPM
 ↓
SMO
 ↓
PLL
 ↓
启动
```

一般应该：

```text
                  启动
                    │
                    ↓
                对齐转子
                    │
                    ↓
                开环强拖
                    │
                    ↓
                提高转速
                    │
                    ↓
              BEMF逐渐变大
                    │
                    ↓
             SMO开始可靠工作
                    │
                    ↓
             PLL开始锁定
                    │
                    ↓
             判断锁定成功
                    │
                    ↓
                切换无感
```

---

# 二十、SMO和PLL什么时候切换？

比如你可以设：

```text
机械转速 > 300 RPM
```

或者：

```text
BEMF幅值 > 某个阈值
```

才允许 PLL 接管。

比如：

```c
if (bemf_amplitude > BEMF_MIN)
{
    pll_enable = true;
}
```

BEMF幅值：

$$
E=
\sqrt{E_\alpha^2+E_\beta^2}
$$

代码：

```c
float bemf =
    sqrtf(
        Ealpha * Ealpha +
        Ebeta  * Ebeta
    );
```

当：

```text
BEMF足够大
+
PLL误差足够小
```

再切换。

---

# 二十一、真正工程上还要做“无扰切换”

这是很多新手容易忽略的。

假设现在：

```text
开环角度 = 120°
```

PLL刚开始：

```text
PLL角度 = 60°
```

你直接：

```c
theta_use = pll.theta;
```

电角度瞬间：

```text
120°
 ↓
60°
```

FOC会直接：

```text
Iq/Id
 ↓
瞬间变化
 ↓
电流冲击
 ↓
电机抖一下
```

所以切换前应该：

```text
开环角度
     ↓
初始化PLL的theta
     ↓
PLL从当前角度继续运行
     ↓
PLL逐渐锁定
     ↓
再切换
```

例如：

```c
pll.theta = open_loop_theta;
pll.omega = open_loop_omega;
pll.integral = open_loop_omega;
```

然后开始 PLL。

---

# 二十二、你这个7对极电机速度转换

PLL输出：

$$
\omega_e
$$

是：

### 电角速度

你如果需要机械 RPM：

$$
\omega_m=
\frac{\omega_e}{p}
$$

然后：

$$
RPM=
\omega_m\frac{60}{2\pi}
$$

所以：

$$
\boxed{
RPM=
\omega_e
\frac{60}{2\pi p}
}
$$

你的：

$$
p=7
$$

所以：

$$
RPM
=

\omega_e
\frac{60}{14\pi}
$$

代码：

```c
float mech_rpm =
    pll.omega *
    60.0f /
    (2.0f * M_PI * 7.0f);
```

---

# 二十三、你最终调试时不要只看“电机转没转”

要同时看：

```text
1. Ealpha
2. Ebeta
3. Ed
4. Eq
5. theta_PLL
6. omega_PLL
7. mech_rpm
```

其中最重要：

### 第一：

$$
E_\alpha/E_\beta
$$

是不是旋转矢量。

### 第二：

$$
E_d
$$

是不是趋近：

$$
0
$$

### 第三：

$$
E_q
$$

是不是比较稳定。

### 第四：

$$
\omega_e
$$

是不是平滑。

### 第五：

```text
theta_PLL
```

是不是连续。

---

# 二十四、给你一个完整的SMO+PLL伪代码

你现在 ESP32 20kHz FOC 里可以按这个思路：

```c
void Sensorless_FOC_Update(void)
{
    /***************
     * 1. ADC
     ***************/
    Read_Current();

    /***************
     * 2. Clarke
     ***************/
    Clarke(Ia, Ib,
           &Ialpha,
           &Ibeta);

    /***************
     * 3. 当前PWM电压
     ***************/
    Get_Valpha_Vbeta(
        &Ualpha,
        &Ubeta);

    /***************
     * 4. SMO
     ***************/
    SMO_Update(
        &smo,
        Ualpha,
        Ubeta,
        Ialpha,
        Ibeta
    );

    float Ealpha = smo.Ealpha;
    float Ebeta  = smo.Ebeta;

    /***************
     * 5. BEMF amplitude
     ***************/
    float bemf =
        sqrtf(
            Ealpha * Ealpha +
            Ebeta * Ebeta
        );

    /***************
     * 6. PLL
     ***************/
    if (bemf > BEMF_MIN)
    {
        PLL_Update_BEMF(
            &pll,
            Ealpha,
            Ebeta
        );
    }

    /***************
     * 7. PLL输出
     ***************/
    float theta_e =
        pll.theta;

    float omega_e =
        pll.omega;

    /***************
     * 8. Park
     ***************/
    Park(
        Ialpha,
        Ibeta,
        theta_e
    );

    /***************
     * 9. Current PI
     ***************/
    Current_PI();

    /***************
     * 10. SVPWM
     ***************/
    SVPWM();
}
```

这就是：

$$
\boxed{
ADC
\rightarrow Clarke
\rightarrow SMO
\rightarrow E_{\alpha\beta}
\rightarrow PLL
\rightarrow \theta_e
\rightarrow Park
\rightarrow PI
\rightarrow SVPWM
}
$$

---

# 二十五、你现在最应该按这个顺序实现

别一次性把所有东西堆进去。

## 第1阶段：只验证SMO

输入：

```text
Uα Uβ
Iα Iβ
```

观察：

```text
Eα
Eβ
```

要求：

```text
Eα/Eβ能够形成旋转波形
```

---

## 第2阶段：加 atan2

```c
theta_bemf =
    atan2f(Ebeta, Ealpha);
```

观察：

```text
theta_bemf
```

确认：

```text
角度连续
方向正确
```

---

## 第3阶段：先做最简单角度PLL

```text
atan2
 ↓
theta_measure
 ↓
angle error
 ↓
PI
 ↓
omega
 ↓
integrator
 ↓
theta
```

先不要接 FOC。

只看：

```text
theta_measure
theta_PLL
omega_PLL
```

---

## 第4阶段：验证PLL锁定

你应该看到：

```text
theta_measure
    ╱╱╱╱╱╱
   ╱
  ╱

theta_PLL
 ╱╱╱╱╱╱╱╱
```

两者逐渐靠近。

并且：

```text
error → 0
```

---

## 第5阶段：改成真正BEMF PLL

最终：

```text
Eα Eβ
 ↓
Park(θ_PLL)
 ↓
Ed
 ↓
PI
 ↓
ω
 ↓
积分
 ↓
θ_PLL
```

这个版本才是我建议你最终放进无感 FOC 的版本。

---

## 第6阶段：接FOC

```text
theta_PLL
 ↓
Park
 ↓
Id/Iq
 ↓
PI
 ↓
SVPWM
```

然后再做：

```text
开环
 ↓
SMO
 ↓
PLL
 ↓
无扰切换
```

---

# 二十六、最后你只需要真正理解这句话

**SMO 和 PLL 是两个完全不同的东西：**

### SMO：

回答：

> **“电机现在产生了一个什么样的反电动势？”**

得到：

$$
E_\alpha,E_\beta
$$

---

### PLL：

回答：

> **“这个反电动势现在转到哪里了？它转多快？”**

得到：

$$
\theta_e,\omega_e
$$

所以：

$$
\boxed{
SMO
=

\text{BEMF观测器}
}
$$

$$
\boxed{
PLL
=

\text{角度/速度跟踪器}
}
$$

最终：

$$
\boxed{
U_{\alpha\beta},I_{\alpha\beta}
\overset{SMO}{\longrightarrow}
E_{\alpha\beta}
\overset{PLL}{\longrightarrow}
\theta_e,\omega_e
}
$$

**你现在如果准备真正写代码，我建议下一步就直接做一件事：把你之前那版 SMO 代码贴出来，我可以在你现有的 `SMO_Update()` 基础上，直接给你接一个完整的 `SMO + BEMF-PLL` C API，并且按照你的 `20kHz / Ts=50us / 7对极 / ESP-IDF` 参数，把 `Kp、Ki` 的计算、限幅、启动、锁定判断和无扰切换一起设计进去。**


----------------
---------------
-----------------

# 无感FOC观测器效果对比


你这个问题非常关键。你现在研究的是**无感FOC的位置观测算法**，常见的几个方案：

1. **SMO（滑模观测器）+ PLL**
2. **龙伯格观测器（Luenberger Observer）+ PLL**
3. **MRAS（模型参考自适应）**
4. **HFI 高频注入**
5. **EKF 扩展卡尔曼滤波**
6. **TI FAST / InstaSPIN**
7. **滑模二阶超扭曲（STA-SMO）+ PLL**

我按照你这个场景：

> ESP32 + 20kHz FOC + 2208小电机 + 低成本 + 自己实现

来比较。

---

# 先给结论

如果是工程实际：

| 算法          | 低速    | 中高速   | 实现难度  | 抗噪声   | 算力需求 | 综合    |
| ----------- | ----- | ----- | ----- | ----- | ---- | ----- |
| SMO+PLL     | 差     | ⭐⭐⭐⭐⭐ | 中     | ⭐⭐⭐⭐  | 低    | ⭐⭐⭐⭐  |
| 龙伯格+PLL     | 一般    | ⭐⭐⭐   | 中     | ⭐⭐⭐   | 低    | ⭐⭐⭐   |
| MRAS        | 一般    | ⭐⭐⭐   | 高     | ⭐⭐⭐   | 中    | ⭐⭐    |
| HFI         | ⭐⭐⭐⭐⭐ | 一般    | 高     | ⭐⭐⭐⭐  | 高    | ⭐⭐⭐⭐  |
| EKF         | ⭐⭐⭐⭐  | ⭐⭐⭐⭐  | 非常高   | ⭐⭐⭐⭐⭐ | 很高   | ⭐⭐⭐⭐  |
| FAST        | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐⭐ | 低(黑盒) | ⭐⭐⭐⭐⭐ | 低    | ⭐⭐⭐⭐⭐ |
| STA-SMO+PLL | 一般    | ⭐⭐⭐⭐⭐ | 中高    | ⭐⭐⭐⭐⭐ | 中    | ⭐⭐⭐⭐⭐ |

---

如果你问：

> **自己做无感FOC，哪个最值得研究？**

我的排序：

### 第一梯队：

```
TI FAST
        ↓
STA-SMO + PLL
        ↓
传统SMO + PLL
```

---

# 1. SMO + PLL（你现在研究的）

结构：

```
电机模型
  |
  ↓
滑模观测器
  |
  ↓
BEMF
  |
  ↓
PLL
  |
  ↓
角度速度
```

## 优点

### ① 计算量小

ESP32非常适合。

主要：

```c
乘法
加法
sin/cos
PI
```

20kHz完全可以。

---

### ② 中高速效果很好

比如：

你的2208：

```
1000RPM以上
```

反电动势：

$$
E=K_e\omega
$$

明显。

SMO：

```
BEMF明显
↓
估计准确
```

可以做到：

±几度电角度误差。

---

## 缺点

最大问题：

### 低速垃圾

因为：

$$
E=K_e\omega
$$

速度：

```
0 RPM
```

那么：

```
BEMF≈0
```

SMO：

```
没有信号
```

所以：

```
0~300RPM
```

通常不可靠。

---

# 2. 龙伯格观测器 + PLL

结构：

```
电机数学模型
        |
        ↓
状态观测器
        |
        ↓
估计反电动势
        |
        ↓
PLL
```

它本质：

预测：

$$
x(k+1)=Ax+Bu
$$

然后：

实际测量修正：

$$
L(y-\hat y)
$$

---

优点：

### 模型清晰

控制理论味道很强。

适合：

学习现代控制。

---

缺点：

对于PMSM：

电机参数：

```
Rs
Ld
Lq
Ψf
```

影响非常大。

你的电机：

```
Rs厂家8.25Ω
你测15Ω
```

这种误差：

龙伯格很敏感。

---

所以：

工业里面：

龙伯格不是主流无感FOC方案。

---

# 3. MRAS

模型参考自适应。

思想：

两个模型：

```
参考模型
      |
      ↓
实际电流


可调模型
      |
      ↓
估计速度
```

比较误差：

```
误差
 ↓
PI
 ↓
调整速度
```

---

优点：

速度估计很好。

缺点：

设计复杂。

PMSM里面：

```
参考模型
自适应律
稳定性证明
```

比较麻烦。

---

# 4. HFI 高频注入

这个非常特殊。

原理：

给电机打高频信号：

例如：

$$
V_h=A\sin(\omega_ht)
$$

利用：

转子磁阻差异。

---

优势：

低速神器。

例如：

```
0 RPM
50 RPM
100 RPM
```

仍然可以知道角度。

---

但是：

它依赖：

### IPMSM凸极性

也就是：

$$
Ld \neq Lq
$$

你的2208：

大概率：

SPMSM

表贴式。

```
Ld≈Lq
```

HFI效果差。

---

# 5. EKF

扩展卡尔曼滤波。

它估计：

状态：

$$
x=
$$
i_d,
i_q,
\theta,
\omega
$$
$$

优点：

理论最强。

可以同时估：

```
角度
速度
磁链
参数
```

---

缺点：

计算量巨大。

ESP32：

20kHz：

基本不现实。

一般：

```
DSP
ARM M7
C2000
```

使用。

---

# 6. TI FAST

这个是工业神器。

TI InstaSPIN：

核心：

FAST：

```
Flux
Angle
Speed
Torque
```

估计器。

它直接输出：

```
磁链
角度
速度
转矩
```

---

优点：

非常强。

低速：

可以做到：

几十RPM。

动态：

很好。

---

缺点：

不开源。

只能TI平台。

例如：

```
TMS320F280xx
```

---

# 7. STA-SMO（超扭曲滑模）

这是现在比较新的。

传统SMO：

符号函数：

$$
sign(e)
$$

问题：

抖振：

```
++++----
```

产生高频噪声。

STA：

二阶滑模：

连续：

$$
u=k_1|s|^{1/2}sign(s)+z
$$

优点：

比传统SMO：

* 噪声低
* 抖振小
* 精度高

现在很多论文：

```
STA-SMO + PLL
```

效果非常好。

---

# 对你这个ESP32项目，我推荐路线

你的条件：

```
ESP32
20kHz
2208
12V
AS5600已有
准备无感
```

不要一开始搞EKF。

建议：

---

## 第一阶段

先：

```
SMO + atan2
```

目的：

验证：

```
Eα
Eβ
```

是不是正确。

---

## 第二阶段

升级：

```
SMO
 ↓
BEMF PLL
 ↓
θe
```

这是工业经典方案。

---

## 第三阶段

优化：

传统：

```
SMO
```

换：

```
STA-SMO
```

效果明显提升。

---

## 第四阶段

如果以后做产品：

考虑：

```
HFI + SMO
```

形成：

```
低速HFI
+
高速SMO
```

这是现在很多电动车方案。

---

# 对你的2208电机，我预计效果

你的参数：

```
KV=100
7对极
12V
```

如果：

## SMO+PLL

大概：

```
0~300RPM
困难

300~3000RPM
很好
```

---

## STA-SMO+PLL

可能：

```
100~3000RPM
很好
```

---

## HFI

你的：

SPMSM

可能：

```
低速一般
```

---

## EKF

理论最好：

但是：

ESP32不划算。

---

# 所以如果我是你：

你的学习路线：

```
Clarke/Park
        ↓
电流环
        ↓
有感FOC跑通
        ↓
SMO
        ↓
PLL
        ↓
SMO+PLL无感
        ↓
STA-SMO优化
        ↓
弱磁/MTPA
```

这是最接近工业FOC工程师成长路线。

一句话总结：

> **低成本MCU自己做无感FOC：SMO+PLL是性价比最高；追求极致效果：STA-SMO+PLL；追求工业商业级：TI FAST；低速性能：HFI；理论最强：EKF。**

你现在ESP32这个平台，继续深挖 **SMO + PLL** 是最正确的路线。你已经有SMO代码了，下一步就是把PLL调出来。
