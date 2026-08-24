上午我们讲 SMO（Sliding Mode Observer，滑模观测器）的时候，讲到一个关键点：

> **SMO负责“估计反电动势 Eα、Eβ”，然后通过反正切 atan2 得到转子电角度。**

但是实际工程里，**SMO后面几乎不会直接 atan2 输出角度给FOC用**，而是：

```
SMO
 |
 | 估计反电动势
 |
Eα、Eβ
 |
PLL锁相环
 |
θe（电角度）
ωe（电角速度）
```

这个 PLL 就是今天重点。

---

# 一、为什么SMO后面需要PLL？

先回顾SMO输出。

永磁同步电机数学模型：

$$
U_\alpha=R_sI_\alpha+L_s\frac{dI_\alpha}{dt}+E_\alpha
$$

$$
U_\beta=R_sI_\beta+L_s\frac{dI_\beta}{dt}+E_\beta
$$

SMO通过：

* 输入电压 U
* 输入电流 I

估计：

$$
E_\alpha
$$

$$
E_\beta
$$

而反电动势：

$$
E_\alpha=-\psi_f\omega_e\sin(\theta_e)
$$

$$
E_\beta=\psi_f\omega_e\cos(\theta_e)
$$

所以：

```
      Eβ
      |
      |
      |
------O------ Eα

```

实际上反电动势向量就在旋转。

角度：

$$
\theta_e=atan2(E_\beta,E_\alpha)
$$

看起来很简单。

但是问题来了：

## atan2的问题

假设：

```
θ=179°

```

下一瞬：

```
θ=181°

```

atan2输出：

```
179°
-179°

```

直接跳变。

而且：

SMO有：

* 高频抖振
* 噪声
* 开关误差
* 电流采样误差

所以：

```
SMO角度：

175
176
180
-179
-178

```

FOC直接用：

电流环瞬间炸。

所以工程上加：

# PLL锁相环

---

# 二、PLL是什么？

PLL：

Phase Locked Loop

中文：

> 锁相环

它本质是一个：

**自动跟踪输入旋转相位的闭环控制器。**

最经典结构：

```
              +----------------+
              |                |
              |                v
输入角度 ---> 相位比较 ---> PI ---> 速度
 θSMO              |
                  |
                  v
              积分器
                  |
                  v
              θPLL输出

```

和电机FOC非常像。

其实PLL就是一个小型控制系统。

---

# 三、PLL核心思想

假设：

SMO给：

$$
\theta_{SMO}
$$

PLL内部有：

预测角：

$$
\theta_{PLL}
$$

两个比较：

$$
error=\theta_{SMO}-\theta_{PLL}
$$

例如：

实际：

```
SMO:

90°

PLL:

85°

```

误差：

$$
error=5°
$$

然后：

PI调节。

---

# 四、PLL数学公式

## 1. 相位误差

$$
e_\theta=
\theta_{SMO}-\theta_{PLL}
$$

但是实际不用角度比较。

因为角度有360跳变。

通常使用：

$$
e_\theta=
E_\alpha cos\theta
---

E_\beta sin\theta
$$

为什么？

因为：

如果PLL角度正确：

反电动势坐标变换以后：

```
Ed = 0

```

如果角度错误：

出现：

```
Ed ≠0

```

所以：

直接用d轴反电动势作为误差。

这个叫：

> SRF-PLL（同步旋转坐标系锁相环）

FOC里面大量使用。

---

# 五、SRF-PLL结构

```
             Eα
              |
              |
              v

        Clarke坐标

             

Eα,Eβ

              |
              v

        Park变换

              |
              |

        Ed      Eq

              |
              |
             Ed

              |
              v

             PI

              |
              v

          ωe估计

              |
              v

          积分

              |
              v

          θe

```

目标：

让：

$$
E_d=0
$$

也就是说：

让PLL角度一直跟着电机转子磁场。

---

# 六、PLL里面PI干什么？

假设：

电机：

1000rpm

电角速度：

$$
\omega_e
$$

突然加速：

1200rpm

那么：

SMO检测到：

```
反电动势旋转变快

```

PLL发现：

```
Ed出现误差

```

PI输出：

增加：

$$
\omega_e
$$

然后：

积分：

$$
\theta_e=\int \omega_e dt
$$

得到新的角度。

所以：

PLL同时得到：

## 1. 电角度

$$
\theta_e
$$

## 2. 电角速度

$$
\omega_e
$$

这个速度非常重要。

因为：

速度环需要机械速度：

$$
\omega_m
=

\frac{\omega_e}{p}
$$

p：

极对数。

你的2208：

12槽14极：

$$
p=7
$$

所以：

$$
RPM=
\frac{\omega_e}{7}
\times
\frac{60}{2\pi}
$$

---

# 七、为什么SMO+PLL比SMO+atan2强？

比较：

## 方法1：

SMO + atan2

流程：

```
Eα,Eβ

 |

atan2

 |

θ

```

缺点：

* 噪声大
* 角度跳变
* 无速度输出
* 低速差

---

## 方法2：

SMO + PLL

流程：

```
Eα,Eβ

 |

PLL

 |

θ
ω

```

优点：

* 角度连续
* 自动滤波
* 有速度
* 抗噪声

所以工业：

TI FAST

VESC

ODrive

基本都是类似思想。

---

# 八、VF是什么意思？

这个你经常看到：

```
V/F控制

VF启动

开环VF

```

这里：

V：

Voltage

电压

F：

Frequency

频率

也就是：

> 电压-频率控制。

---

# 九、为什么无感FOC需要VF？

因为SMO有一个致命问题：

低速：

反电动势：

$$
E=\omega\psi
$$

速度接近0：

$$
E≈0
$$

SMO没有信号。

例如：

0 rpm:

```
没有反电动势

SMO瞎了

```

所以启动阶段：

不用SMO。

采用：

## VF强拖启动

例如：

给：

```
频率慢慢增加

5Hz
10Hz
20Hz

```

同时：

电压增加：

```
1V
2V
4V

```

形成旋转磁场：

```
定子磁场

↓

拖动转子

↓

产生反电动势

↓

SMO接管

```

这个过程叫：

> Open-loop startup（开环启动）

或者：

> I/F启动

---

# 十、VF和FOC关系

传统VF：

```
频率
 |
 |
电压

```

没有电流闭环。

例如：

变频器控制异步电机。

而无感FOC里面：

VF只是启动阶段。

完整流程：

```
上电

 |

转子定位

 |

VF/IF强拖

 |

达到一定速度

 |

SMO启动

 |

PLL锁定

 |

切换闭环FOC

```

---

# 十一、结合你的ESP32 + 2208电机

你的架构应该是：

```
启动:

VF
|
|
达到200~500rpm


运行:

ADC采样

↓

SMO

↓

PLL

↓

θe

↓

Park

↓

Id/Iq PI

↓

SVPWM

```

你的2208：

7极对

比如：

机械：

1000RPM

电角频率：

$$
f_e=
\frac{1000}{60}
\times7
$$

=

116.7Hz

PLL跟踪的是：

116Hz电角速度。

不是机械速度。

---

# 十二、工程里面常用参数

PLL：

```
float pll_kp;
float pll_ki;

float theta;
float omega;

```

每个FOC周期：

20kHz：

```
error = Ed;

omega += kp*error + ki*sum(error);

theta += omega*Ts;

```

最后：

```
theta = wrap(theta);

```

---

所以总结：

| 模块      | 作用            |
| ------- | ------------- |
| SMO     | 根据电流电压估计反电动势  |
| atan2   | 直接算角度（简单但噪声大） |
| PLL     | 平滑跟踪角度，同时得到速度 |
| VF      | 启动阶段开环产生旋转磁场  |
| SMO+PLL | 工业无感FOC主流方案   |

你现在的状态（**SMO已经有角度波形，但没加PLL，电机嗡嗡响**）其实正处于无感FOC最典型阶段：下一步不是继续优化SMO，而是加 **SRF-PLL + 角度补偿 + 速度估计**。这一步完成后，电机声音通常会明显改善。
