你已经学完**有感 FOC（编码器/霍尔反馈）**，进入无感 FOC 是非常自然的一步。其实无感 FOC 不是重新学一个控制算法，而是在你现在 FOC 框架里**去掉 AS5600，把“转子角度 θe 和速度 ωe 的来源”换成算法估计出来**。

你现在 ESP32 + AS5600 + 电流环20kHz 的架构，转无感非常合适。

---

## 1. 先理解：有感 FOC 和无感 FOC区别在哪里？

你现在：

```
AS5600
  |
  ↓
机械角度 θm
  |
极对数 p
  |
电角度 θe
  |
Park变换
  |
Id/Iq控制
  |
SVPWM
```

AS5600告诉你：

> “转子磁场现在在哪”

所以FOC很简单。

无感：

```
电压 Ud/Uq
电流 Id/Iq
      |
      ↓
  观测器 Observer
      |
      ↓
估计 θe_est
估计 ωe_est
      |
      ↓
Park变换
```

核心变成：

> 根据电机电流、电压，反推出转子位置。

这就是无感FOC的本质。

---

# 2. 学习路线不要一上来搞高频注入

无感FOC有很多方法：

| 方法                  | 难度    | 应用    |
| ------------------- | ----- | ----- |
| 反电动势 BEMF Observer  | ⭐     | 高速    |
| 滑模观测器 SMO           | ⭐⭐⭐   | 工业常用  |
| PLL锁相环              | ⭐⭐    | 配合SMO |
| Luenberger Observer | ⭐⭐⭐⭐  | 高级    |
| EKF卡尔曼              | ⭐⭐⭐⭐⭐ | 研究级   |
| 高频注入 HFI            | ⭐⭐⭐⭐  | 低速/零速 |

工业里常见：

```
SMO + PLL
```

或者：

```
Luenberger + PLL
```

SMO通过估计反电势，再由PLL得到角度和速度，是很多无感FOC方案的经典结构。

---

# 3. 最推荐你的学习顺序

结合你的情况：

ESP32
+
2208电机
+
20kHz电流环

建议：

---

## 第一阶段：理解电机数学模型

你现在已经有：

* Clarke
* Park
* PI电流环
* SVPWM

继续补：

### PMSM αβ模型

公式：

$$
U_\alpha=R i_\alpha + L\frac{di_\alpha}{dt}+e_\alpha
$$

$$
U_\beta=R i_\beta + L\frac{di_\beta}{dt}+e_\beta
$$

重点：

反电动势：

$$
e=K_e\omega
$$

也就是说：

速度越高：

反电势越明显。

所以：

低速：

```
没有e
↓
无法估计角度
```

高速：

```
e明显
↓
容易估计
```

这也是为什么普通BEMF无感需要先开环启动。

---

# 4. 第二阶段：实现最简单 BEMF Observer

你的FOC增加：

现在：

```
AS5600
 ↓
theta
```

替换：

```
Observer
 ↓
theta_est
```

流程：

### ① 已知：

你有：

```
Uα
Uβ
Iα
Iβ
```

### ② 计算反电势

根据：

$$
E_\alpha=U_\alpha-RI_\alpha-L\frac{dI_\alpha}{dt}
$$

$$
E_\beta=U_\beta-RI_\beta-L\frac{dI_\beta}{dt}
$$

得到：

```
e_alpha
e_beta
```

---

### ③ atan2得到角度

$$
\theta_e=atan2(e_\beta,e_\alpha)
$$

然后：

替换AS5600。

---

但是！

这个方法你马上会发现：

高速很好：

```
1000RPM
↑
正常
```

低速：

```
100RPM
↓
抖
```

因为：

反电势太小。

---

# 5. 第三阶段：学习 SMO（重点）

你的目标应该是：

```
SMO
+
PLL
+
FOC
```

结构：

```
          Iαβ
           |
           ↓
      滑模观测器
           |
           ↓
     eα eβ
           |
           ↓
        PLL
           |
     θ_est ω_est
           |
           ↓
       FOC
```

---

## SMO干什么？

简单理解：

它制造一个：

“虚拟电机”

输入：

你的：

```
电压
电流
电机参数
```

然后预测：

```
理论电流
```

和实际：

```
采样电流
```

比较：

```
误差
```

修正：

最后得到：

```
反电势
```

---

# 6. PLL是什么？

你之前问过PLL锁相环。

无感里面：

SMO输出：

```
eα
eβ
```

但是：

这个不是直接角度。

PLL负责：

锁住这个旋转磁场。

类似：

GPS锁定卫星信号。

输出：

```
theta
omega
```

---

# 7. 你的ESP32项目怎么改？

你现在：

```
AS5600 task
     |
 theta
     |
 FOC task
```

改成：

```
ADC ISR
 |
采样Ia Ib
 |
Clarke
 |
SMO
 |
PLL
 |
theta_est
 |
Park
 |
PI
 |
SVPWM
```

AS5600保留：

不要删。

用于：

```
对比
```

非常重要。

例如：

打印：

```
theta_encoder
theta_observer
```

看误差：

```
error = theta_encoder-theta_est
```

这是调无感最好的方法。

---

# 8. 你的2208电机适合吗？

你的：

```
KV100
12V
7极对
```

其实挺适合学习。

原因：

低KV：

反电势常数高

比如：

高速运行：

1000RPM

机械：

$$
1000/60=16.6Hz
$$

电频率：

$$
16.6*7=116Hz
$$

很好观察。

---

# 9. 不建议你现在学什么？

暂时不要：

❌ EKF卡尔曼无感

因为：

你会陷入：

* 状态矩阵
* Q/R矩阵
* 收敛性

你现在已经有卡尔曼滤重量经验，但是电机EKF难几个数量级。

---

# 10. 最适合你的路线

我建议：

```
有感FOC
    |
    ↓
BEMF Observer
    |
    ↓
SMO
    |
    ↓
PLL
    |
    ↓
无感FOC高速运行
    |
    ↓
高频注入HFI
    |
    ↓
零速启动
```

做到：

1. AS5600跑FOC ✅（你已经完成）
2. 去掉AS5600，高速无感运行
3. SMO+PLL稳定运行
4. 最后研究HFI零速启动

对于你现在这个 ESP32 FOC 项目，**下一步最有价值的不是换TI DSP，而是自己实现一个 SMO+PLL 无感FOC**。这个水平已经接近商业电调核心算法了。




---------------
------------------
--------------------

BEMF Observer（**反电动势观测器**）是无感 FOC 里面最基础的一类“转子位置估计器”。

一句话理解：

> **它通过测量电机的电流、电压，计算出电机产生的反电动势（Back Electromotive Force, BEMF），再根据反电动势方向推算转子角度。**

你现在有感 FOC：

```
AS5600
  |
  ↓
转子角度 θe
  |
  ↓
Park变换
  |
  ↓
FOC
```

BEMF Observer：

```
电压 Uαβ
电流 Iαβ
  |
  ↓
BEMF Observer
  |
  ↓
反电势 Eαβ
  |
  ↓
atan2()
  |
  ↓
转子角度 θe
  |
  ↓
FOC
```

---

## 1. 为什么反电动势能知道转子位置？

你的 BLDC/PMSM 电机：

转子永磁体旋转：

↓

切割定子磁场：

↓

产生反电动势：

$$
E = K_e \omega
$$

也就是：

**转得越快，反电势越大。**

例如：

你的2208电机：

KV=100

大概：

12V空载：

```
1200RPM左右
```

电频率：

你的极对数：

```
p=7
```

所以：

$$
f_e=\frac{1200}{60}\times7
$$

约：

```
140Hz电频率
```

这个旋转磁场会产生：

```
Eα
Eβ
```

两个正交信号：

类似：

```
       Eβ
       |
       |
       |
-------+-------- Eα
      /
     /
```

它的方向就是转子磁链方向。

---

# 2. BEMF Observer怎么算？

电机数学模型：

在αβ坐标：

$$
U_\alpha=R_sI_\alpha+L\frac{dI_\alpha}{dt}+E_\alpha
$$

移项：

$$
E_\alpha=
U_\alpha-R_sI_\alpha-L\frac{dI_\alpha}{dt}
$$

β轴：

$$
E_\beta=
U_\beta-R_sI_\beta-L\frac{dI_\beta}{dt}
$$

所以你需要：

已知：

### 电压

你的SVPWM输出：

```
Uα
Uβ
```

### 电流

你的INA240：

```
Ia
Ib

Clarke

Iα
Iβ
```

### 电机参数：

你已经测过：

```
R = 8.25Ω

L = 4.25mH
```

然后计算：

```
Eα
Eβ
```

---

# 3. 得到角度

有了：

```
Eα
Eβ
```

直接：

$$
\theta=atan2(E_\beta,E_\alpha)
$$

例如：

假设：

```
Eα=2V
Eβ=3.46V
```

那么：

$$
atan2(3.46,2)
$$

得到：

```
60°
```

这就是：

转子电角度。

---

# 4. 为什么BEMF Observer不能低速？

因为：

$$
E=K_e\omega
$$

速度：

1000RPM：

```
反电势明显
```

容易算。

但是：

10RPM：

```
E≈0
```

你的采样噪声：

```
ADC噪声
电流采样误差
PWM纹波
```

可能比BEMF还大。

比如：

真实：

```
E=5mV
```

噪声：

```
20mV
```

结果：

角度完全乱。

所以：

普通BEMF无感：

启动：

```
开环拖动
```

达到一定速度：

```
切换Observer
```

---

# 5. 它和SMO有什么关系？

BEMF Observer：

属于：

```
简单观测器
```

直接：

```
U-I模型
       ↓
    Eαβ
```

结构：

```
电压
 |
 |
电流
 |
 ↓
计算BEMF
 |
 ↓
atan2
 |
θ
```

而SMO：

更高级：

```
电压
 |
电流
 |
 ↓
滑模模型
 |
 ↓
估计BEMF
 |
 ↓
PLL
 |
θ
```

SMO本质也是BEMF Observer。

区别：

|      | BEMF Observer | SMO    |
| ---- | ------------- | ------ |
| 算法   | 简单计算          | 非线性观测器 |
| 抗噪声  | 一般            | 强      |
| 低速   | 差             | 较好     |
| 工业应用 | 少             | 多      |
| 难度   | 低             | 中高     |

---

# 6. 结合你现在ESP32 FOC怎么实现？

你的20kHz FOC循环：

现在：

```c
void foc_loop()
{
    read_encoder();

    clarke();

    park(theta_encoder);

    current_pi();

    svpwm();
}
```

改成：

```c
void foc_loop()
{

    read_current();


    clarke();


    bemf_observer(
        Ualpha,
        Ubeta,
        Ialpha,
        Ibeta
    );


    theta_est =
        atan2(Ebeta,Ealpha);


    park(theta_est);


    current_pi();


    svpwm();

}
```

---

# 7. 你的学习顺序建议

不要直接上复杂算法：

建议：

```
第一步：
自己写BEMF Observer

↓


第二步：
发现低速抖动

↓

加入LPF

↓

第三步：
BEMF + PLL


↓

第四步：
SMO + PLL

↓

第五步：
HFI零速启动
```

以你现在已经完成：

* Clarke/Park
* 电流环
* SVPWM
* 编码器FOC
* ESP32 20kHz实时控制

其实 BEMF Observer 是进入无感FOC最合适的第一关。你会发现它本质就是**把AS5600换成数学模型**。





---------------
------------------
--------------------



你这个学习路线是对的。以你现在的基础（已经写过 ESP32 FOC、电流环、SVPWM、编码器角度），**直接进入 SMO + PLL 比先玩简单 BEMF 更有价值**。

先给你一个整体框架：

> **SMO（Sliding Mode Observer，滑模观测器）负责“估计反电势”**
> **PLL（Phase Locked Loop，锁相环）负责“从反电势中提取转子角度和速度”**

两者组合：

```
             Ia Ib
              |
              ↓
          Clarke变换
              |
          Iα Iβ
              |
              |
      Uα Uβ  电压
        |      |
        ↓      ↓
    +----------------+
    |      SMO       |
    | 滑模观测器      |
    +----------------+
              |
              ↓
        Eα   Eβ
        反电势估计
              |
              ↓
            PLL
              |
        θe_est  ωe_est
              |
              ↓
          FOC Park
```

这就是工业无感FOC经典结构。

---

# 一、为什么需要SMO？

先回顾BEMF Observer的问题。

你知道：

$$
E=U-RI-L\frac{dI}{dt}
$$

理论简单：

```
电压
 -
电阻压降
 -
电感压降

=反电势
```

但是实际：

## 电流微分：

$$
\frac{dI}{dt}
$$

非常麻烦。

因为：

ADC：

```
噪声
+
PWM纹波
+
采样误差
```

经过微分以后：

噪声会被无限放大。

比如：

电流：

```
1.000A
1.001A
0.999A
```

微分：

可能变成：

```
+20
-30
+15
```

所以直接BEMF很敏感。

---

# 二、SMO核心思想是什么？

一句话：

> 不直接计算反电势，而是建立一个“虚拟电机模型”，通过电流误差不断修正。

类似：

你有一个电机：

真实电机：

```
输入电压

↓

真实电流 Ia Ib
```

软件里面复制一个：

```
数学电机模型

↓

预测电流 Ia_est Ib_est
```

然后比较：

```
真实电流

       -

预测电流


=误差
```

如果：

```
误差 >0
```

说明模型慢了。

调整。

如果：

```
误差 <0
```

说明模型快了。

反向调整。

最终：

预测模型跟真实电机同步。

---

# 三、SMO数学模型

PMSM αβ模型：

$$
U_\alpha
========

R_s I_\alpha
+
L_s\frac{dI_\alpha}{dt}
+
E_\alpha
$$

整理：

$$
\frac{dI_\alpha}{dt}
====================

\frac{1}{L}
(U_\alpha-RI_\alpha-E_\alpha)
$$

β轴一样：

$$
\frac{dI_\beta}{dt}
===================

\frac{1}{L}
(U_\beta-RI_\beta-E_\beta)
$$

---

SMO增加一个：

滑模补偿项：

$$
Z=K \times sign(I-I_{est})
$$

这里：

```
I
```

是真实电流

```
Iest
```

模型预测电流

误差：

$$
e=I-I_{est}
$$

---

于是：

预测：

$$
\dot I_{est}
=

\frac1L
(U-RI_{est}-Z)
$$

重点来了：

这个：

```
Z
```

就是估计出来的反电势。

也就是说：

```
电流误差
     |
     ↓
滑模函数
     |
     ↓
反电势
```

---

# 四、sign函数是什么？

理想：

$$
sign(x)
$$

就是：

```
x>0

输出 +1


x<0

输出 -1
```

例如：

```c
if(error>0)
{
    z=K;
}
else
{
    z=-K;
}
```

但是：

这样会产生严重抖振：

```
++++----
++++----
```

所以工程里面不用：

sign

而用：

sat函数：

$$
sat(x)=
\begin{cases}
1&x>1\

x&|x|<1\

-1&x<-1
\end{cases}

$$

代码：

```c
float sat(float x)
{
    if(x>1)
        return 1;

    if(x<-1)
        return -1;

    return x;
}
```

---

# 五、SMO输出是什么？

SMO最后得到：

```
Zα

Zβ
```

经过低通：

得到：

```
Eα

Eβ
```

例如：

```
Eα = 2.5V

Eβ = 4.3V
```

但是：

这里还有问题：

这个角度：

```
atan2(Eβ,Eα)
```

会抖。

原因：

* 噪声
* 相位延迟
* 滤波

所以需要PLL。

---

# 六、PLL到底干什么？

PLL就是：

**锁相环**

作用：

> 跟踪一个旋转信号的相位。

输入：

```
Eα

Eβ
```

相当于：

一个旋转矢量：

```
       Eβ

        ↑

        |

        |

---------→ Eα

       θ
```

PLL输出：

```
theta
omega
```

---

# 七、PLL工作原理

假设：

SMO给：

```
θ_real
```

PLL内部有：

```
θ_est
```

两者比较：

误差：

$$
error=\theta_{real}-\theta_{est}
$$

进入PI：

```
error

 ↓

PI

 ↓

速度ω

 ↓

积分

 ↓

角度θ
```

结构：

```
        角度误差
             |
             ↓
            PI
             |
             ↓
          ω_est
             |
          积分器
             |
             ↓
          θ_est
```

代码大概：

```c
error = sin(theta_bemf - theta_est);


omega += kp*error + ki*sum(error);


theta += omega*dt;
```

---

# 八、放到你的ESP32 FOC里面

你现在：

```c
theta = AS5600_angle;
```

改成：

```c
theta = smo_pll.theta;
```

流程：

20kHz：

```
ADC采样

↓

Clarke

↓

SMO更新

↓

PLL更新

↓

Park(theta_est)

↓

Id/Iq PI

↓

SVPWM
```

你的20kHz刚好适合。

---

# 九、调试顺序（非常重要）

不要直接闭环。

建议：

## 第一步

保留AS5600：

同时运行：

```
theta_encoder

theta_smo
```

打印：

```
误差角
```

目标：

高速：

<5°

---

## 第二步

让SMO只观察：

不要控制。

比如：

```c
Park(theta_encoder)
```

但是：

打印：

```
theta_smo
```

---

## 第三步

切换：

```c
Park(theta_smo)
```

---

# 十、你这个2208参数需要哪些？

SMO需要：

```
Rs
Ls
采样周期dt
```

你的：

```
R = 8.25Ω

L=4.25mH

dt=50us
```

都有。

所以硬件不用改。

---

总结一句：

**BEMF Observer 是“公式计算反电势”。**

**SMO 是“通过电流误差逼近反电势”。**

**PLL 是“把反电势变成稳定角度和速度”。**

工业无感FOC基本就是：

```
SMO
 ↓
BEMF
 ↓
PLL
 ↓
θe
 ↓
FOC
```

你现在已经到了学习无感FOC最关键的一步。下一步建议深入：**SMO离散化公式 + C代码实现（20kHz ISR版本）**，这个才是从理论到你的ESP32项目的连接点。

---------------
------------------
--------------------

好的，我们继续往**工程实现层面**深入。你现在不要停留在“知道SMO是什么”，真正做无感FOC，需要搞懂：

1. **SMO离散化怎么跑在20kHz控制周期里**
2. **SMO代码结构怎么写**
3. **PLL如何从BEMF得到角度**
4. **怎么和你现在ESP32 FOC框架融合**

---

# 一、先回顾SMO整体位置

你的有感FOC：

```text
ADC
 |
 ↓
Ia Ib
 |
 ↓
Clarke
 |
 ↓
Iα Iβ
 |
 ↓
Park(AS5600角度)
 |
 ↓
Id/Iq PI
 |
 ↓
SVPWM
```

无感：

变成：

```text
ADC
 |
 ↓
Ia Ib
 |
 ↓
Clarke
 |
 ↓
      ┌─────────┐
      │   SMO   │
      └─────────┘
            |
            ↓
          Eα Eβ
            |
            ↓
          PLL
            |
       θ_est ω_est
            |
            ↓
     Park(θ_est)

            ↓
       电流环
```

重点：

**SMO必须运行在你的20kHz FOC周期里面。**

你的：

```
dt=50us
```

刚好。

---

# 二、SMO数学模型离散化

电机αβ模型：

$$
U_\alpha=R_sI_\alpha+L_s\frac{dI_\alpha}{dt}+E_\alpha
$$

整理：

$$
\frac{dI_\alpha}{dt}
====================

\frac{1}{L_s}
(U_\alpha-R_sI_\alpha-E_\alpha)
$$

但是：

Eα不知道。

怎么办？

SMO：

用：

$$
Z_\alpha
$$

代替：

$$
E_\alpha
$$

所以：

$$
\frac{dI_{\alpha est}}{dt}
=

\frac1L
(U_\alpha-RI_{\alpha est}-Z_\alpha)
$$

离散：

$$
I(k+1)
=

I(k)+
\frac{dt}{L}
(U-RI-Z)
$$

对应代码：

```c
Ialpha_est += 
(dt/Ls)*
(
 Ualpha
 - Rs*Ialpha_est
 - Zalpha
);
```

---

# 三、滑模核心：电流误差

你有两个电流：

## 实际采样

来自ADC：

```c
Ialpha
```

## 模型预测

SMO内部：

```c
Ialpha_est
```

计算：

```c
error_alpha =
Ialpha - Ialpha_est;
```

如果：

实际：

```
1.2A
```

模型：

```
0.8A
```

说明模型偏小。

需要增加修正。

---

# 四、滑模函数

理论：

$$
Z=K*sign(error)
$$

代码：

```c
if(error>0)
{
    Z=K;
}
else
{
    Z=-K;
}
```

但是实际不能这么干。

为什么？

因为：

20kHz：

每周期：

50us

误差：

```
+
-
+
-
+
-
```

会导致：

MOS：

疯狂抖动。

所以工业：

用饱和函数：

$$
Z=K*sat(error)
$$

代码：

```c
float sat(float x)
{
    if(x>1)
        return 1;

    else if(x<-1)
        return -1;

    else
        return x;
}
```

然后：

```c
Zalpha =
Kslide * sat(error_alpha / E0);
```

其中：

E0：

滑模边界。

---

# 五、完整SMO代码框架

类似：

```c
typedef struct
{

float Ialpha_est;
float Ibeta_est;


float Zalpha;
float Zbeta;


float Ealpha;
float Ebeta;


float Rs;
float Ls;

float dt;


float Kslide;

}SMO_t;
```

---

更新函数：

```c
void smo_update(
SMO_t *smo,
float Ualpha,
float Ubeta,
float Ialpha,
float Ibeta)
{


float err_alpha;
float err_beta;


//1. 电流误差

err_alpha =
Ialpha -
smo->Ialpha_est;


err_beta =
Ibeta -
smo->Ibeta_est;



//2. 滑模控制


smo->Zalpha =
smo->Kslide *
sat(err_alpha);


smo->Zbeta =
smo->Kslide *
sat(err_beta);



//3. 电流模型更新


smo->Ialpha_est +=
smo->dt/smo->Ls *
(
Ualpha
-
smo->Rs*smo->Ialpha_est
-
smo->Zalpha
);



smo->Ibeta_est +=
smo->dt/smo->Ls *
(
Ubeta
-
smo->Rs*smo->Ibeta_est
);



//4. Z就是反电势估计

smo->Ealpha =
smo->Zalpha;

smo->Ebeta =
smo->Zbeta;


}
```

这就是最基本SMO。

---

# 六、但是这里有一个坑：Z不是直接角度

很多初学者：

看到：

```c
Ealpha
Ebeta
```

直接：

```c
theta=atan2(Ebeta,Ealpha);
```

然后发现：

电机乱跳。

原因：

SMO输出：

有：

## 高频抖动

↓

## 相位延迟

↓

## 噪声

所以需要PLL。

---

# 七、PLL工程实现

PLL输入：

```c
Ealpha
Ebeta
```

先归一化：

$$
sin(\theta)
$$

误差：

其实不用atan。

常用：

$$
error=
E_\alpha cosθ
+
E_\beta sinθ
$$

为什么？

假设：

真实角度：

θe

估计：

θ

两个向量：

如果一致：

误差0。

如果偏：

产生误差。

---

代码：

```c
error =
Ealpha*cos(theta)
+
Ebeta*sin(theta);
```

然后：

PI：

```c
omega +=
Kp*error
+
Ki*integral;
```

积分：

```c
theta += omega*dt;
```

完整：

```c
void pll_update(
PLL_t *pll,
float Ealpha,
float Ebeta)
{


float error;


//相位误差

error =
Ealpha*cos(pll->theta)
+
Ebeta*sin(pll->theta);



//PI

pll->integral +=
pll->Ki*
error*
pll->dt;


pll->omega =
pll->Kp*error
+
pll->integral;



//积分得到角度

pll->theta +=
pll->omega*
pll->dt;


}
```

输出：

```c
pll.theta
```

就是：

无感角度。

---

# 八、和你的ESP32 FOC结合

你现在大概：

20kHz：

```
FOC Task

↓

current adc

↓

clarke

↓

park(theta_encoder)

↓

PI

↓

svpwm
```

改：

```
FOC Task


ADC


↓

Clarke


↓

SMO


↓

PLL


↓

theta_est


↓

Park(theta_est)


↓

PI


↓

SVPWM

```

---

# 九、参数怎么设置？

你的电机：

2208：

```
Rs=8.25Ω

Ls=4.25mH

dt=50us
```

SMO：

重点三个参数：

---

## 1. Kslide

太小：

```
响应慢
角度跟不上
```

太大：

```
抖振严重
```

一般：

慢慢增加。

---

## 2. PLL Kp

大：

响应快

但是：

噪声大。

小：

稳定

但是：

跟踪慢。

---

## 3. PLL Ki

决定：

速度估计稳定性。

---

# 十、你现在最应该怎么做？

不要马上切无感。

你的开发路线：

### Step1

保持AS5600：

运行：

```c
theta_encoder
```

同时：

计算：

```c
theta_smo
```

打印：

```
encoder=120.5°
smo=117.8°
```

看误差。

---

### Step2

高速切换：

例如：

```
1000RPM以上

使用SMO
```

低速：

```
AS5600
```

这就是很多商业产品做法。

---

### Step3

最后：

启动：

开环：

↓

SMO接管

↓

全无感。

---

以你现在的项目水平，我建议下一步重点研究：

**“SMO为什么能估计BEMF，以及为什么PLL不用atan2而用q轴误差锁相”**

因为这两个地方，是从“会抄代码”到“真正理解无感FOC”的分水岭。你已经到了这个阶段。



---------------
------------------
--------------------


继续深入。现在进入 **SMO + PLL 的核心原理**。这一部分是无感 FOC 最容易卡住的地方。

你现在已经知道：

* SMO → 得到反电势 (E_{\alpha},E_{\beta})
* PLL → 得到角度 (\theta_e) 和速度 (\omega_e)

但是还有几个关键问题：

1. **为什么滑模输出 Z 就是反电势？**
2. **为什么PLL不用 atan2？**
3. **为什么无感FOC必须补偿相位延迟？**
4. **SMO实际工程代码为什么和理论公式差很多？**

我们继续。

---

# 一、先理解SMO为什么能估计反电势？

回到电机模型：

$$
U=RI+L\frac{dI}{dt}+E
$$

三个部分：

```
电机端电压

=

电阻压降

+

电感压降

+

反电势
```

其中：

```
R*I
```

你知道。

```
L*dI/dt
```

理论可以计算。

唯一不知道：

```
E
```

也就是转子磁场产生的电压。

---

传统方法：

直接算：

$$
E=U-RI-L\frac{dI}{dt}
$$

问题：

$$
\frac{dI}{dt}
$$

噪声巨大。

---

SMO换一个思路：

不要计算：

> “我觉得反电势是多少”

而是：

> “我建立一个模型，如果反电势猜错了，那么预测电流一定和真实电流不一样，我通过误差不断调整。”

---

## 举个例子

真实电机：

```
U = 6V

实际电流 = 0.5A
```

你的软件模型：

假设：

```
E=0
```

那么模型认为：

```
电流应该 = 1A
```

但是实际：

```
0.5A
```

误差：

```
0.5A
```

说明：

模型少考虑了一部分电压。

这部分是什么？

就是：

```
反电势
```

所以SMO不断调整：

```
Z
```

直到：

```
预测电流 ≈ 实际电流
```

此时：

```
Z≈E
```

这就是滑模观测器核心。

---

# 二、SMO其实是一个闭环控制器

很多人误解：

SMO是计算器。

不是。

它其实像一个控制环：

```
             +
真实电流 I ----|
              |
              ↓
          电流误差
              |
              ↓
          滑模控制器
              |
              ↓
        Z(反电势估计)
              |
              ↓
        电机模型
              |
              ↓
       I_est预测电流
              |
              └────────
```

是不是很像PID？

其实：

SMO里面也有：

* 增益 K
* 误差
* 反馈

只是目标不是控制速度，而是观察状态。

---

# 三、为什么PLL不用atan2？

初学者都会想：

既然：

$$
\theta=atan2(E_\beta,E_\alpha)
$$

为什么还搞PLL？

因为实际：

SMO输出：

```
Eα
Eβ
```

不是干净正弦。

比如：

理想：

```
Eα:

    /\
   /  \
__/    \__


Eβ:

相差90°
```

实际：

```
PWM纹波

噪声

相位延迟
```

直接：

```c
theta=atan2()
```

角度：

```
120°
123°
118°
125°
```

抖。

FOC：

直接炸。

---

PLL相当于：

给角度加一个“惯性”。

类似：

编码器：

1000Hz采样：

不会每次角度跳。

PLL：

认为：

```
角度不会瞬间乱跳
```

---

# 四、PLL数学本质

假设：

SMO给：

一个旋转矢量：

$$
E=
\begin{bmatrix}
E_\alpha\
E_\beta
\end{bmatrix}
$$

PLL内部：

生成：

$$
\theta_{pll}
$$

然后做Park：

注意：

这里不是电流Park。

而是：

对反电势做：

```
反Park
```

公式：

$$
E_q=
-E_\alpha sinθ
+
E_\beta cosθ
$$

重点：

如果：

角度正确：

反电势应该全部在d轴：

```
Ed = E

Eq = 0
```

也就是说：

目标：

让：

$$
E_q=0
$$

所以：

PLL误差：

就是：

$$
error=E_q
$$

这就是为什么很多代码：

```c
pll_error =
-Ealpha*sinf(theta)
+
Ebeta*cosf(theta);
```

---

# 五、PLL完整结构

看：

```
             Eαβ
              |
              ↓
          Park变换
              |
              ↓
             Eq
              |
              ↓
             PI
              |
              ↓
          ω_est
              |
              ↓
          积分器
              |
              ↓
          θ_est
```

是不是和FOC速度环类似？

其实：

PLL就是一个：

**角度误差控制器**

---

# 六、为什么需要相位补偿？

这是很多开源FOC调不好的原因。

SMO输出：

不是实时角度。

为什么？

因为：

1. 电流采样
2. 滤波
3. SMO计算
4. PLL

都有延迟。

假设：

电机：

1000RPM

电频：

你的：

7极对

机械：

$$
1000/60=16.7Hz
$$

电频：

$$
16.7*7=116.7Hz
$$

周期：

$$
8.5ms
$$

如果系统延迟：

200us

角度误差：

$$
360*0.2/8.5
$$

约：

8.5°

这会直接影响：

Iq控制。

所以商业FOC都有：

```c
theta_compensation =
omega_e * delay;
```

例如：

```c
theta_use =
theta_pll
+
omega_e*0.0002;
```

---

# 七、为什么高速无感容易，低速难？

现在你应该理解了：

SMO靠什么？

靠：

```
反电势
```

而：

$$
E=K_e\omega
$$

速度越高：

```
E越大
```

低速：

```
E≈0
```

但是：

电流采样：

```
±几十mA噪声
```

所以：

低速：

SMO看到的是：

```
噪声
```

不是：

```
转子信息
```

---

# 八、实际商业无感FOC启动流程

所以不会：

0RPM直接SMO。

流程：

```
0 RPM

↓

强制角度开环

↓

给Iq电流

↓

拖起来

↓

达到速度阈值

↓

SMO开始可靠

↓

PLL锁定

↓

切换无感
```

例如：

```
0~500RPM

开环


500RPM以后

SMO
```

---

# 九、对应你的ESP32代码结构

你现在：

```c
foc_loop()
{

read_adc();


clarke();


park(theta_encoder);


current_pi();


svpwm();

}
```

未来：

```c
foc_loop()
{

read_adc();


clarke();


//估计角度

smo_update();


pll_update();


//补偿

theta =
pll.theta
+
pll.omega*delay;



park(theta);


current_pi();


svpwm();


}
```

---

# 十、你下一步应该研究什么？

现在你的知识链：

✅ 有感FOC
✅ 电流环
✅ SVPWM
✅ BEMF概念
✅ SMO思想
✅ PLL思想

下一步就是工程关键：

## 1. SMO离散公式推导

搞懂：

```c
I_est += dt/L*(U-RI-Z)
```

每个变量怎么来的。

## 2. SMO参数调节

重点：

* Kslide怎么定
* LPF截止频率怎么选

## 3. PLL参数设计

重点：

* Kp
* Ki

怎么根据：

电频率

采样频率

计算。

## 4. 无感启动状态机

这是商业电调真正的核心：

```
ALIGN

↓

OPEN_LOOP

↓

CLOSE_LOOP

↓

RUN
```

下一节我们可以继续讲：

**“如何根据你的2208电机参数，计算SMO的Kslide、PLL Kp/Ki，并写一个ESP32 20kHz可运行版本。”**

这个就可以直接开始写代码了。你现在距离真正实现无感FOC已经很近了。






---------------
------------------
--------------------
