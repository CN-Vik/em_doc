PLL（Phase Locked Loop，**锁相环**）是无感 FOC 里面非常重要的一个模块。

很多人刚接触无感 FOC 会以为：

> SMO 得到反电动势 → atan2 得到角度 → 结束

实际上工程里面通常是：

```text
SMO/BEMF Observer
        |
        v
  反电动势 Eα Eβ
        |
        v
      atan2
        |
        v
    粗糙角度 θe
        |
        v
       PLL
        |
        +----------+
        |          |
        v          v
    平滑角度 θe    电角速度 ωe
```

**PLL 的作用：**

1. 去除观测角度噪声
2. 让角度连续
3. 估计电角速度
4. 给速度环提供速度反馈

---

# 1. 为什么需要 PLL？

假设 SMO 输出反电动势：

$$
E_\alpha,E_\beta
$$

计算：

$$
\theta_e=atan2(E_\beta,E_\alpha)
$$

理论：

```text
真实角度:

0°
10°
20°
30°
40°
```

实际：

```text
SMO:

0°
12°
19°
33°
38°
```

有抖动。

如果直接给 FOC：

Park：

$$
Id/Iq
$$

会出现：

* 电流波动
* 转矩脉动
* 噪声

所以需要 PLL。

---

# 2. PLL的本质是什么？

一句话：

> PLL 是一个自动调节的角度跟踪器。

目标：

让：

```
估计角度
     |
     |
     v
真实角度
```

保持一致。

类似：

汽车跟车：

前车：

真实转子角度

你的车：

估计角度

PLL不断调整：

```text
快了？
减速

慢了？
加速
```

最后锁定。

---

# 3. PLL数学模型

假设：

输入角度：

$$
\theta_{in}
$$

PLL内部：

$$
\hat{\theta}
$$

误差：

$$
e_\theta
=

\theta_{in}-\hat{\theta}
$$

然后：

PI控制器：

$$
\omega_e
=

K_p e_\theta
+
K_i\int e_\theta dt
$$

速度积分得到角度：

$$
\hat{\theta}
=

\int\omega_e dt
$$

结构：

```
          θ输入

             |
             v

        +---------+
        |  减法   |
        +---------+

             |
             v

           误差

             |
             v

            PI

             |
             v

          ωe速度

             |
             v

          积分器

             |
             v

          θ估计
```

---

# 4. 在FOC里面怎么用？

假设 SMO：

输出：

```c
theta_smo
```

PLL：

输入：

```c
theta_smo
```

输出：

```c
theta_pll
omega_pll
```

然后：

FOC：

```c
Park(theta_pll);
```

速度：

```c
rpm =
omega_pll / pole_pairs * 9.5493;
```

---

# 5. 一个简单PLL C代码

STM32/ESP32都类似。

## PLL结构体

```c
typedef struct
{

    float theta;       //估计角度

    float omega;       //估计速度


    float kp;

    float ki;


    float integral;


}PLL_Handle;
```

---

## 更新函数

```c
void PLL_Update(
        PLL_Handle *pll,
        float theta_input,
        float Ts)
{

    float error;


    //角度误差

    error =
        theta_input
        -
        pll->theta;



    //角度误差归一化

    if(error > PI)
        error -= 2*PI;


    if(error < -PI)
        error += 2*PI;



    //积分

    pll->integral +=
        pll->ki *
        error *
        Ts;



    //速度估计

    pll->omega =
        pll->kp *
        error
        +
        pll->integral;



    //角度积分

    pll->theta +=
        pll->omega *
        Ts;



    //角度限制

    if(pll->theta>2*PI)
        pll->theta-=2*PI;


    if(pll->theta<0)
        pll->theta+=2*PI;


}
```

---

# 6. 举例理解

假设：

实际：

```
θ=100°
```

PLL：

```
θhat=90°
```

误差：

```
+10°
```

PI：

输出：

```
ω增加
```

积分：

```
90
95
100
```

最后锁住。

如果：

PLL跑太快：

```
θhat=110°
```

误差：

```
-10°
```

PI：

降低速度。

---

# 7. PLL参数怎么调？

两个参数：

## Kp

比例：

决定响应速度。

大：

优点：

* 跟踪快

缺点：

* 抖

---

## Ki

积分：

消除稳态误差。

大：

容易：

* 振荡

---

经验：

高速：

```text
Kp大
Ki适中
```

低速：

```text
Kp小
Ki小
```

---

# 8. 为什么无感FOC特别喜欢 PLL？

因为：

SMO输出：

角度：

有噪声：

```
10
12
11
13
```

PLL：

变：

```
10
11
12
13
```

同时：

它直接给：

$$
\omega_e
$$

不用差分：

$$
\frac{\Delta\theta}{dt}
$$

速度更稳定。

---

# 9. PLL和速度环有什么关系？

非常重要：

FOC有两个速度：

## 电角速度

PLL输出：

$$
\omega_e
$$

单位：

rad/s 电角速度

---

## 机械速度

因为：

$$
\omega_e=p\omega_m
$$

所以：

$$
\omega_m=
\frac{\omega_e}{p}
$$

RPM：

$$
RPM=
\frac{\omega_e}{p}
\times
9.5493
$$

代码：

```c
rpm =
pll.omega
/
pole_pairs
*
9.5493f;
```

然后：

```text
RPM

↓

速度PID

↓

Iq_ref

↓

转矩
```

---

# 10. 工业无感FOC典型结构

例如 TI / ST：

```text
             ADC

              |

          Clarke

              |

          SMO/STO

              |

          Eα Eβ

              |

            atan2

              |

            PLL

        +------------+
        |            |
        v            v

      θe             ωe

        |            |

     Park        Speed PID

        |

    Current PI

        |

      SVPWM

        |

      电机
```

---

# 11. 对你现在项目的对应关系

你现在：

```
AS5600
 |
 θm
 |
×7
 |
θe
 |
FOC
```

如果换无感：

替换成：

```
SMO
 |
atan2
 |
PLL
 |
θe
 |
FOC
```

速度：

现在：

AS5600角度差分

↓

rpm

以后：

PLL：

$$
\omega_e
$$

直接：

↓

机械RPM

---

你目前已经在做 **20kHz FOC + AS5600 + 速度环**，下一步如果研究无感，建议顺序：

1. **先实现 PLL（独立模块）**
2. 用 AS5600 产生 θe，测试 PLL
3. 再接 SMO
4. 最后闭环速度

因为 PLL 本身和 SMO 是独立的，先把 PLL 调通，无感算法会简单很多。你后面看 TI FAST、ST STO，本质上都会看到类似“角度估计 + 速度锁定”的结构。
