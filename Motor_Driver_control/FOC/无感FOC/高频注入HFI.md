你这个问题问得非常好。因为你前面一直在搞 **SMO无感FOC**，而高频注入（HFI，High Frequency Injection）和SMO最大的区别就是：

* **SMO靠反电动势估角**
* **HFI靠电机凸极性估角**

很多人讲HFI一句：

> “注入高频信号，通过电感变化检测转子位置”

然后没了。

这句话对于刚接触的人几乎没用。

下面我从：

1. 为什么需要HFI
2. 电机为什么能被高频信号检测角度
3. 高频注入完整数学推导
4. 实际FOC实现流程
5. 代码伪代码
6. HFI和SMO区别

一步一步讲。

---

# 一、为什么需要高频注入？

先回忆SMO。

SMO：

通过：

$$
E=U-RI-L\frac{di}{dt}
$$

得到：

$$
E_{\alpha},E_{\beta}
$$

然后：

$$
\theta_e=atan2(E)
$$

但是：

反电动势：

$$
E=K_e\omega
$$

也就是说：

速度越低：

$$
\omega\rightarrow0
$$

那么：

$$
E\rightarrow0
$$

例如：

电机：

1000rpm：

```
反电动势 10V
```

很好测。

但是：

10rpm：

```
反电动势 几乎0
```

SMO失效。

所以：

低速：

需要另一种方法。

就是：

# 高频注入 HFI

---

# 二、HFI核心思想（一句话）

> 给电机主动打一个高频小电压，看电机不同方向上的电流响应，根据电流大小判断转子方向。

简单理解：

拿手电筒照物体：

SMO：

```text
等天亮
看影子
```

HFI：

```text
主动打光
看反射
```

---

# 三、为什么电机能感知方向？

关键：

## 永磁同步电机有磁阻差异

尤其：

## IPMSM 内嵌式永磁同步电机

转子：

```
       N

   ┌─────┐
   │磁钢 │
   └─────┘


d轴：
磁钢方向


q轴：
磁钢旁边
```

两个方向磁路不同。

所以：

$$
L_d \neq L_q
$$

例如：

$$
L_d=0.5mH
$$

$$
L_q=1.2mH
$$

---

# 四、高频电压加进去以后发生什么？

电机电压：

正常：

$$
U=R i+L\frac{di}{dt}+E
$$

高频：

频率：

例如：

$$
f_h=1000Hz
$$

周期：

$$
1ms
$$

这个时候：

电阻影响小：

因为：

$$
\omega_h L >> R
$$

所以：

近似：

$$
U_h=L\frac{di_h}{dt}
$$

整理：

$$
i_h=
\frac{U_h}{\omega_h L}
$$

重点来了：

电流：

和电感：

反比。

---

如果：

方向是d轴：

$$
i_d=\frac{U_h}{\omega_h L_d}
$$

如果：

方向是q轴：

$$
i_q=\frac{U_h}{\omega_h L_q}
$$

因为：

$$
L_d\neq L_q
$$

所以：

电流幅值不同。

这就是位置信息来源。

---

# 五、怎么知道d轴在哪里？

假设：

真实转子：

```
          q

          ↑


d ----------------
```

但是我们不知道。

我们人为建立一个：

估计坐标：

$$
\hat\theta
$$

然后：

在这个方向注入：

$$
V_h
$$

比如：

```text
      注入方向

          ↑

          |
----------+----------

```

---

如果：

注入方向刚好是d轴：

那么：

看到：

$$
L=L_d
$$

电流：

$$
I=\frac{V}{\omega L_d}
$$

---

如果：

偏离：

角度误差：

$$
\Delta\theta
$$

那么：

实际看到的是：

d/q混合。

---

# 六、数学模型

定义：

真实转子角：

$$
\theta
$$

估计角：

$$
\hat\theta
$$

误差：

$$
\Delta\theta
=
\theta-\hat\theta
$$

---

注入电压：

一般：

$$
V_h=
V_m\sin(\omega_h t)
$$

在估计dq坐标：

$$
\begin{bmatrix}
V_d\\
V_q
\end{bmatrix}
=
\begin{bmatrix}
V_m\sin(\omega_ht)\\
0
\end{bmatrix}
$$

---

# 七、经过电机电感产生高频电流

真实坐标：

电感矩阵：

$$
L=
\begin{bmatrix}
L_d&0\\
0&L_q
\end{bmatrix}
$$

由于角度错位：

需要坐标转换。

得到：

q轴高频电流：

$$
i_q^h
$$

近似：

$$
\boxed{
i_q^h
=
K
\sin(2\Delta\theta)
}
$$

其中：

$$
K=
\frac{V_h(L_q-L_d)}
{2\omega_hL_dL_q}
$$

这个公式非常关键。

---

# 八、为什么是sin(2角度)？

因为：

电感是椭圆。

不是圆。

你转180°：

北极方向又回来了。

所以：

位置周期：

180°电角度。

数学：

$$
sin(2\theta)
$$

所以HFI天然：

检测：

$$
\theta
$$

但是有：

180°模糊。

---

# 九、同步解调过程（最重要）

注入：

$$
V_h=V\sin(\omega_ht)
$$

产生：

$$
i_h
$$

但是：

里面有：

高频：

```text
sin(ωt)
```

还有位置：

```text
sin(2Δθ)
```

所以：

需要解调。

---

## 第一步：采样高频电流

得到：

$$
i_h
$$

---

## 第二步：乘回注入信号

乘：

$$
sin(\omega_ht)
$$

得到：

$$
i_h sin(\omega_ht)
$$

---

## 第三步：低通

高频项消失。

留下：

$$
\boxed{
I_{demod}
=
K sin(2\Delta\theta)
}
$$

---

# 十、得到角度误差

因为：

小角度：

$$
sinx≈x
$$

所以：

$$
sin(2\Delta\theta)
≈2\Delta\theta
$$

因此：

$$
I_{demod}
\propto
\Delta\theta
$$

于是：

$$
\boxed{
error=K_i I_{demod}
}
$$

---

# 十一、然后怎么锁角？

和PLL类似。

误差：

$$
\Delta\theta
$$

进入：

PI。

$$
\omega=
K_p e+
K_i\int e dt
$$

积分：

$$
\theta_{est}
=
\int\omega dt
$$

最终：

$$
\Delta\theta=0
$$

锁定。

---

# 十二、完整HFI流程图

```
              给定高频电压
                    |
                    ↓
              Vh*sin(wt)
                    |
                    ↓
              逆Park
                    |
                    ↓
              SVPWM
                    |
                    ↓
                  电机
                    |
                    ↓
             高频电流响应
                    |
                    ↓
              Clarke
                    |
                    ↓
          同步解调
        ×sin(wt)
                    |
                    ↓
                LPF
                    |
                    ↓
          位置误差 Δθ
                    |
                    ↓
                  PI
                    |
                    ↓
              θ_est
                    |
                    ↓
                  FOC
```

---

# 十三、代码伪代码

## 1. 高频注入

```c
// 高频频率
fh = 1000Hz;


// 产生注入电压

Vhf =
A * sin(2*pi*fh*t);



Ud += Vhf;

```

---

## 2. FOC正常运行

```c
Park(
Ialpha,
Ibeta,
theta_est
);


Id,Iq


PI控制


Ud += Vhf;


InvPark();


SVPWM();

```

---

## 3. 采集高频电流

```c
Ihf =
Id_measured - Id_filtered;

```

---

## 4. 同步解调

```c
demod =
Ihf *
sin(2*pi*fh*t);

```

---

## 5. 低通

```c
position_error =
LPF(demod);

```

---

## 6. PLL修正角度

```c
omega += Kp*error;

theta += omega*Ts;

```

---

# 十四、HFI和SMO区别

|         | SMO  | HFI |
| ------- | ---- | --- |
| 依据      | 反电动势 | 磁阻  |
| 需要转速    | 需要   | 不需要 |
| 低速      | 差    | 强   |
| 高速      | 强    | 一般  |
| 适合SPMSM | 可以   | 困难  |
| 适合IPMSM | 可以   | 非常好 |
| 复杂度     | 中    | 高   |

---

# 十五、为什么表贴式SPMSM HFI困难？

你的2208：

大概率：

SPMSM

也就是：

$$
L_d≈L_q
$$

那么：

$$
L_q-L_d≈0
$$

前面的：

$$
K=
\frac{V_h(L_q-L_d)}
{2\omega_hL_dL_q}
$$

接近：

$$
K≈0
$$

所以：

没有明显响应。

这就是为什么：

* IPMSM
* 埋磁电机

HFI效果好。

---

# 最后用一句话总结：

**SMO：**

$$
U,I
\rightarrow
电流模型
\rightarrow
反电动势E
\rightarrow
角度
$$

**HFI：**

$$
注入高频电压
\rightarrow
产生高频电流
\rightarrow
检测Ld/Lq差异
\rightarrow
转子方向
\rightarrow
角度
$$

你现在的2208-80T表贴电机，主路线应该还是：

**SMO + PLL**

而不是HFI。

HFI更多用于：

* 伺服电机
* IPMSM
* 新能源汽车电机
* 低速零速启动。


---------
-----------


把 **HFI（高频注入）无感FOC估角流程**压缩成数学公式，就是下面这一套。

---

# 1. 注入高频电压

在估计dq坐标系注入：

$$
\boxed{
V_h=V_m\sin(\omega_h t)
}
$$

通常注入d轴：

$$
\begin{bmatrix}
V_d^h\\
V_q^h
\end{bmatrix}
=
\begin{bmatrix}
V_m\sin(\omega_ht)\\
0
\end{bmatrix}
$$

---

# 2. 高频电流响应

高频下：

电阻和反电动势影响很小：

$$
U_h\approx L\frac{di_h}{dt}
$$

所以：

$$
\boxed{
I_h
=
\frac{V_h}{j\omega_h L}
}
$$

也就是：

$$
\boxed{
I_h
\propto
\frac1L
}
$$

---

# 3. 利用凸极性产生位置差异

IPMSM：

$$
L_d\neq L_q
$$

电感：

$$
\begin{bmatrix}
L_d&0\\
0&L_q
\end{bmatrix}
$$

如果估计角：

$$
\hat\theta
$$

真实角：

$$
\theta
$$

角度误差：

$$
\boxed{
\Delta\theta=\theta-\hat\theta
}
$$

---

# 4. 角度误差导致q轴电流

经过坐标变换：

得到：

$$
\boxed{
i_q^h
=
K\sin(2\Delta\theta)
}
$$

其中：

$$
\boxed{
K=
\frac{
V_h(L_q-L_d)
}{
2\omega_hL_dL_q
}
}
$$

这就是HFI的位置误差信号。

---

# 5. 同步解调

采样高频电流：

$$
i_h
$$

乘注入信号：

$$
\boxed{
i_h\sin(\omega_ht)
}
$$

得到：

高频项 + 位置项。

---

# 6. 低通滤波

去掉：

$$
2\omega_h
$$

留下：

$$
\boxed{
I_{demod}
=
K\sin(2\Delta\theta)
}
$$

---

# 7. 小角度线性化

当锁定附近：

$$
\Delta\theta\approx0
$$

有：

$$
\sin x\approx x
$$

所以：

$$
\boxed{
I_{demod}
\approx
2K\Delta\theta
}
$$

因此：

$$
\boxed{
error
=
I_{demod}
}
$$

就是角度误差。

---

# 8. PLL锁角

误差进入PI：

$$
\boxed{
\omega_e
=
K_p error
+
K_i\int error dt
}
$$

积分：

$$
\boxed{
\hat\theta_e(k)
=
\hat\theta_e(k-1)
+
\omega_eT_s
}
$$

最终：

$$
\boxed{
\Delta\theta\rightarrow0
}
$$

得到：

$$
\boxed{
\hat\theta_e=\theta_e
}
$$

---

# 整个HFI公式链（一行版）

$$
\boxed{
V_h=V_m\sin(\omega_ht)
}
$$

↓

$$
\boxed{
I_h=\frac{V_h}{j\omega_hL}
}
$$

↓

$$
\boxed{
i_q^h=K\sin(2(\theta-\hat\theta))
}
$$

↓

同步解调：

$$
\boxed{
LPF(i_h\sin\omega_ht)
}
$$

↓

$$
\boxed{
error\approx2K(\theta-\hat\theta)
}
$$

↓

PI：

$$
\boxed{
\omega_e=K_p e+K_i\int e dt
}
$$

↓

积分：

$$
\boxed{
\hat\theta_e=\int\omega_e dt
}
$$

---

一句话：

**HFI不是测反电动势，而是主动给电机一个高频扰动，通过 \(L_d\neq L_q\) 产生不同电流响应，再从响应里面反推出转子角度。**

核心公式只有三个：

$$
\boxed{
I_h=\frac{V_h}{\omega_hL}
}
$$

$$
\boxed{
i_q^h=K\sin(2\Delta\theta)
}
$$

$$
\boxed{
\hat\theta_e=\int(PI(i_q^h))dt
}
$$

这就是HFI无感FOC的数学本质。



---------
------------
# 结合我的FOC项目代码，具体我该怎么实现高频注入呢？

结合你的项目情况：

* ESP32 + ESP-IDF
* FOC 20kHz
* 电流采样 20kHz
* AS5600编码器
* 电机：2208-80T
* 12V母线
* 12槽14极，极对数 p=7
* 目前有：

  * Clarke
  * Park
  * PI电流环
  * SVPWM
  * SMO正在研究

我先说结论：

**你的这个2208-80T表贴BLDC，不建议优先实现HFI。**

原因：

你的电机大概率：

$$
L_d \approx L_q
$$

而HFI依赖：

$$
L_q-L_d
$$

如果：

$$
L_q-L_d≈0
$$

那么：

$$
K=
\frac{V_h(L_q-L_d)}
{2\omega_hL_dL_q}
≈0
$$

结果：

注入了高频：

有电流：

但是：

没有明显位置特征。

不过你既然想实现，我按照你的FOC架构讲完整实现流程。

---

# 一、你现在FOC结构

你现在大概：

```
ADC采样
 |
 |
Ia Ib
 |
Clarke
 |
Iα Iβ
 |
Park(θ)
 |
Id Iq
 |
PI
 |
Ud Uq
 |
反Park
 |
Uα Uβ
 |
SVPWM
```

现在加入HFI：

位置在：

## 电压输出之前

变成：

```
Id/Iq PI

    |
    ↓

Ud Uq

    |
    +
    |
    ↓

加入高频电压

Ud_h

    |
    ↓

反Park

    |
    ↓

SVPWM

```

---

# 二、第一步：产生高频注入信号

例如：

你的FOC：

20kHz

选择：

$$
f_h=1kHz
$$

周期：

$$
T_h=1ms
$$

对应：

20个FOC周期。

定义：

```c
float hfi_phase;
float hfi_freq = 1000.0f;
```

每次FOC：

```c
hfi_phase += 2*pi*hfi_freq*Ts;
```

你的：

$$
Ts=50us
$$

所以：

```c
hfi_phase += 0.314;
```

---

生成：

```c
Vhf = HFI_VOLTAGE * sinf(hfi_phase);
```

例如：

```c
HFI_VOLTAGE=1.0V;
```

---

# 三、第二步：叠加到Ud

你的原来：

```c
Ud = id_pid_out;

Uq = iq_pid_out;
```

现在：

```c
Ud += Vhf;
```

变成：

```c
Ud_total =
Ud_PI + Vhf;
```

注意：

不是：

加Uq。

一般：

d轴注入。

---

# 四、第三步：正常反Park

你的代码：

应该类似：

```c
Ualpha =
Ud*cos(theta)
-
Uq*sin(theta);


Ubeta =
Ud*sin(theta)
+
Uq*cos(theta);
```

现在：

用：

```c
Ud_total
```

即可。

---

# 五、第四步：电机响应

注入以后：

电机产生：

高频电流：

你的ADC：

还是正常采样：

```c
Ia
Ib
```

经过：

Clarke：

得到：

$$
I_\alpha,I_\beta
$$

---

# 六、第五步：提取高频电流

关键。

正常电流：

例如：

```text
Iq=0.5A
```

频率：

几十Hz。

HFI：

1kHz。

所以需要分离。

方法：

高通。

例如：

```c
Ih = I - LPF(I);
```

代码：

```c
Id_h =
Id - Id_lpf;
```

因为你的HFI在d轴注入。

---

# 七、第六步：同步解调

现在：

你有：

$$
I_d^h
$$

里面：

包含：

$$
sin(\omega_ht)
$$

乘回：

```c
demod =
Id_h * sin(hfi_phase);
```

---

# 八、第七步：低通

去掉：

2倍频。

```c
angle_error =
LPF(demod);
```

得到：

$$
error
$$

理论：

$$
error=Ksin(2\Delta\theta)
$$

---

# 九、第八步：PLL

和你SMO PLL一样。

```c
omega_hfi += Kp*error;


theta_hfi += omega_hfi*Ts;
```

输出：

$$
\theta_e
$$

---

# 十、代码结构大概

你的FOC Task：

20kHz：

```c
void FOC_Task()
{


//1 ADC
read_current();


//2 Clarke

Clarke();


//3 Park

Park(theta_est);


//4 电流PI

Ud =
PID_id(Id_ref-Id);


Uq =
PID_iq(Iq_ref-Iq);




//5 HFI注入

if(HFI_ENABLE)
{

    hfi_phase +=
    TWO_PI*1000*Ts;


    Vhf =
    HFI_A*sin(hfi_phase);


    Ud += Vhf;

}



//6 反Park


InvPark();



//7 SVPWM

SVPWM();



}
```

---

# 十一、HFI角度更新

不要20kHz里面乱更新。

建议：

和速度环类似：

1kHz：

```c
void HFI_Update()
{


//提取高频电流

Ih =
Id - Id_filter;



//同步解调

err =
Ih*sinf(hfi_phase);



//低通

err_filter=
LPF(err);



//PLL

omega+=Kp*err_filter;


theta+=omega*0.001;


}
```

---

# 十二、但是你的项目有一个关键问题

你的电机：

2208-80T

你测过：

$$
L=4.25mH
$$

但是：

你没有：

$$
L_d
$$

$$
L_q
$$

而HFI需要：

$$
L_q-L_d
$$

你需要测。

方法：

锁转子：

给d轴高频：

测：

$$
I_d
$$

然后：

换90°：

测：

$$
I_q
$$

计算：

$$
L=\frac{V}{\omega I}
$$

得到：

$$
L_d,L_q
$$

---

# 十三、你的实际开发路线建议

按照你的项目：

我建议：

## 第一阶段

继续：

```
编码器AS5600
+
FOC
```

把：

* 电流环
* 解耦
* MTPA
* 弱磁

搞稳定。

---

## 第二阶段

做：

```
SMO
+
PLL
```

你的电机：

更适合。

---

## 第三阶段

如果想玩：

HFI

建议换：

IPMSM。

---

# 最后对应你的项目一句话：

你现在代码里面实现HFI，其实只需要插入三个地方：

### ① Ud输出处

```c
Ud += Vhf;
```

### ② 电流采样后

```c
Id_h = Id - LPF(Id);
```

### ③ PLL估角

```c
theta += PI(Id_h*sin(hfi_phase))*Ts;
```

但是：

你的2208-80T大概率因为：

$$
L_d≈L_q
$$

导致：

```
能注入
能看到电流
但是无法得到可靠角度
```

所以你的项目路线，**SMO+PLL比HFI更匹配。**你现在研究的方向其实更接近工业无感FOC方案。

-----------
-----------
# 高频注入的核心精髓
你的理解**已经接近了，但是差一点点关键细节**。

不是：

> uq=0，ud完全变成高频正弦信号

而是：

> **正常FOC产生的Ud/Uq控制量继续存在，在Ud上叠加一个小幅高频正弦扰动。**

也就是：

$$
\boxed{
U_d=U_{d\_PI}+U_{hf}\sin(\omega_ht)
}
$$

$$
\boxed{
U_q=U_{q\_PI}
}
$$

---

## 1. 正常FOC是什么样？

你的正常流程：

```
Id_ref
 |
PI
 |
Ud


Iq_ref
 |
PI
 |
Uq


Ud/Uq
 |
反Park
 |
Uα/Uβ
 |
SVPWM
```

例如：

正常运行：

```
Ud = 0.2V

Uq = 3.5V
```

反Park：

$$
U_\alpha
=
U_d\cos\theta
-
U_q\sin\theta
$$

$$
U_\beta
=
U_d\sin\theta
+
U_q\cos\theta
$$

然后SVPWM。

---

# 2. 高频注入做了什么？

它只是偷偷加一个“小扰动”。

比如：

原来：

$$
U_d=0.2V
$$

现在：

加入：

$$
U_{hf}=1V\sin(2\pi1000t)
$$

变成：

$$
\boxed{
U_d=0.2+1\sin(2\pi1000t)
}
$$

而：

$$
U_q=3.5V
$$

不变。

所以实际：

```
        Ud
         |
         |
         +------ 高频抖动
         |
---------+------------时间


        Uq
         |
         |
        3.5V
----------------------
```

---

# 3. 为什么加在d轴？

因为d轴是：

```
          q轴

          ↑


          |
          |
----------+----------→ d轴
          转子磁链方向
```

HFI目的：

就是探测：

```
这个方向是不是转子磁链方向
```

所以：

沿d轴打一个高频“敲击”。

类似：

你拿锤子敲东西：

不同方向敲：

声音不同。

---

# 4. 那如果真的：

```
Uq=0
Ud=正弦
```

会发生什么？

这个其实叫：

## 高频注入启动/定位模式

比如：

电机静止：

没有反电动势：

$$
E=0
$$

SMO：

没办法。

这时候：

你可以：

```
Uq=0

Ud=Vhf*sin(wt)
```

相当于：

只发送探测信号。

电机产生：

```
高频电流响应
```

然后：

检测：

```
Id_h
Iq_h
```

估计转子位置。

这种常用于：

### 零速启动

---

# 5. 但是正常运行不是这样

正常运行：

例如：

```c
//电流环输出

Ud_pi = 0.1f;

Uq_pi = 4.0f;


// HFI

Uhf = 0.5f*sinf(hfi_phase);


//叠加

Ud = Ud_pi + Uhf;

Uq = Uq_pi;
```

然后：

反Park：

```c
Ualpha =
Ud*cos(theta)
-
Uq*sin(theta);


Ubeta =
Ud*sin(theta)
+
Uq*cos(theta);
```

---

# 6. 用你的代码结构理解

你现在：

大概：

```c
// dq电流环

ud = PID_ID();


uq = PID_IQ();


//反Park

inv_park(
    ud,
    uq,
    theta,
    &u_alpha,
    &u_beta
);


//SVPWM

```

改成：

```c
//正常电流环

ud = PID_ID();

uq = PID_IQ();


//============
// 高频注入
//============

hfi_phase +=
2*pi*1000*Ts;


ud += 
HFI_AMP*sinf(hfi_phase);


//============


//反Park

inv_park(
    ud,
    uq,
    theta,
    &u_alpha,
    &u_beta
);



//SVPWM

```

核心就这一句：

```c
ud += HFI_AMP*sinf(hfi_phase);
```

---

# 7. 但是还有一个非常重要的问题

你可能会问：

> 那我没有准确theta，反Park怎么办？

这就是HFI刚开始最绕的地方。

HFI启动阶段：

没有角度。

所以：

通常有：

### 方案1：已有编码器

比如你的AS5600：

```
AS5600角度
      |
      ↓
生成dq坐标
      |
      ↓
HFI测试
```

---

### 方案2：固定初始角度

启动：

假设：

$$
\theta=0
$$

注入：

```
Ud高频
```

检测响应：

修正角度。

---

# 8. 最简单总结

你理解成：

> **在FOC的d轴电压上加一个高频正弦扰动。**

公式：

正常：

$$
U_d,U_q
$$

变：

$$
\boxed{
U_d'=U_d+V_h\sin(\omega_ht)
}
$$

$$
\boxed{
U_q'=U_q
}
$$

然后：

$$
U_d',U_q'
$$

进入：

$$
反Park
$$

得到：

$$
U_\alpha,U_\beta
$$

再：

$$
SVPWM
$$

电机响应产生：

$$
I_h
$$

通过解调：

得到：

$$
\Delta\theta
$$

最后：

$$
\theta_{est}
$$

---

所以你的理解：

> "uq=0，ud变成高频正弦"

✅ **在零速探测阶段可以这么理解**

但是：

> "正常HFI运行"

❌ 不是这样。

正常HFI：

$$
\boxed{
Ud=电流环输出+高频正弦
}
$$

$$
\boxed{
Uq=正常力矩控制输出
}
$$

这就是工业里面的做法。你这个理解已经抓到HFI入口了。你后面如果继续研究，会发现HFI本质就是“给FOC加一个小交流测试信号，然后从响应里识别转子位置”。



---------
------------
---------

对，这里确实是 **HFI 最难理解的一步**。你前面已经理解了“打高频 → 看高频电流”，但还差最后一步：

> **为什么“高频电流”经过解调以后，竟然能告诉我们“转子角度错了多少”？**

我们从最简单的数学模型一步一步推。

---

# 1. 先不要管复杂的HFI，想象一个“指南针”

假设电机真实转子位置是：

$$
\theta_e
$$

但是你的FOC认为转子位置是：

$$
\hat\theta_e
$$

两者有误差：

$$
\Delta\theta=\theta_e-\hat\theta_e
$$

例如：

```text
真实转子d轴：
       ↑
       │
       │
       │
       ●──────→

估计d轴：
       ↗
      /
     /
    /
```

假设：

$$
\Delta\theta=20^\circ
$$

那么问题来了：

> **怎么知道这个20°？**

HFI就是拿一个高频“探针”去测。

---

# 2. 电机可以想象成一个“方向不同，电感不同”的东西

先假设是IPMSM：

$$
L_d=1mH
$$

$$
L_q=2mH
$$

也就是说：

```text
真实d轴：

L = 1mH


真实q轴：

L = 2mH
```

所以：

**沿不同方向打一模一样的高频电压，产生的高频电流是不一样的。**

因为：

$$
V=L\frac{di}{dt}
$$

所以：

$$
\frac{di}{dt}=\frac{V}{L}
$$

---

# 3. 假设我们现在沿“估计d轴”打一发高频

你的FOC认为：

```text
估计d轴
   ↑
   │
   │  VHF
   │
   ●
```

所以：

$$
V_d=V_{HF}\sin(\omega_ht)
$$

但是！

真实d轴其实偏了20°。

因此这个高频电压实际上并不是纯粹沿真实d轴。

它会分解成：

$$
V_{real,d}=V_{HF}\cos(\Delta\theta)
$$

$$
V_{real,q}=V_{HF}\sin(\Delta\theta)
$$

这一步特别重要。

---

# 4. 举个具体数字

假设：

$$
\Delta\theta=20^\circ
$$

那么：

$$
\cos20^\circ\approx0.94
$$

$$
\sin20^\circ\approx0.342
$$

所以你以为自己打的是：

```text
100% d轴
```

实际上真实电机看到：

```text
94% d轴

34.2% q轴
```

所以：

> **只要你的角度错了，高频电压就一定会“串到另一个轴”。**

这就是HFI能够测角度的根本原因。

---

# 5. 然后电机产生高频电流

由于：

$$
L_d\neq L_q
$$

所以：

$$
i_{hf,d}\approx
\frac{V_{hf,d}}{j\omega_hL_d}
$$

$$
i_{hf,q}\approx
\frac{V_{hf,q}}{j\omega_hL_q}
$$

代进去：

$$
i_{hf,d}\propto
\frac{\cos\Delta\theta}{L_d}
$$

$$
i_{hf,q}\propto
\frac{\sin\Delta\theta}{L_q}
$$

这时候：

> **电流里面已经包含了Δθ的信息。**

---

# 6. 但为什么不能直接看电流？

因为实际电流太复杂。

你测到的电流：

```text
I =
正常FOC电流
+
PWM纹波
+
电流环变化
+
高频注入响应
+
ADC噪声
```

所以我们要把：

> **高频注入产生的那部分电流**

单独扒出来。

---

# 7. 这就是“同步解调”

假设我们注入：

$$
V_{HF}=A\sin(\omega_ht)
$$

那么产生的高频电流也会包含：

$$
I_{HF}\sim B\sin(\omega_ht+\phi)
$$

现在你想知道：

> 这个B到底是多少？

以及：

> 相位φ是多少？

于是我们拿测出来的电流：

$$
I_{HF}
$$

乘：

$$
\sin(\omega_ht)
$$

得到：

$$
I_{HF}\sin(\omega_ht)
$$

---

# 8. 为什么乘完再低通就能把它“扒出来”？

这一步其实就是通信里面的**同步检波**。

假设最简单：

$$
I_{HF}=B\sin(\omega_ht)
$$

乘：

$$
\sin(\omega_ht)
$$

得到：

$$
B\sin^2(\omega_ht)
$$

利用：

$$
\sin^2x=\frac{1-\cos2x}{2}
$$

所以：

$$
I_{HF}\sin(\omega_ht)
=
\frac B2 -
\frac B2\cos(2\omega_ht)
$$

注意！

这里出现了两部分：

```text
B/2
+
一个2倍频的交流信号
```

然后：

### 低通滤波器

把：

$$
\cos(2\omega_ht)
$$

过滤掉。

留下：

$$
\boxed{\frac B2}
$$

于是：

> **我们就把高频电流的幅值B提取出来了。**

这就是同步解调。

---

# 9. 那为什么还要乘cos？

因为如果：

$$
I_{HF}=B\sin(\omega_ht+\phi)
$$

那么只乘sin，只能得到一部分信息。

所以一般同时做：

$$
I_{HF}\times\sin(\omega_ht)
$$

以及：

$$
I_{HF}\times\cos(\omega_ht)
$$

然后分别低通。

得到：

$$
I_s
$$

和：

$$
I_c
$$

这样就可以恢复：

$$
B
$$

和：

$$
\phi
$$

类似于：

> **锁相放大器 / IQ解调。**

---

# 10. 但是关键来了：这些东西怎么变成Δθ？

这里才是HFI真正的核心。

我们前面说：

$$
i_{hf,d}\propto\frac{\cos\Delta\theta}{L_d}
$$

$$
i_{hf,q}\propto\frac{\sin\Delta\theta}{L_q}
$$

所以：

**如果Δθ变化，高频电流响应就会变化。**

例如：

### Δθ = 0°

```text
估计d轴 = 真实d轴
```

那么：

$$
\sin0=0
$$

所以：

```text
q轴高频响应 ≈ 0
```

---

### Δθ = 10°

$$
\sin10^\circ\approx0.174
$$

有一点q轴响应。

---

### Δθ = 30°

$$
\sin30^\circ=0.5
$$

q轴响应明显增大。

---

### Δθ = 90°

$$
\sin90^\circ=1
$$

q轴响应最大。

---

所以：

```text
Δθ
 ↓

高频电流响应
 ↓

解调
 ↓

角度误差信号
```

---

# 11. 工程上不会真的直接算arcsin

这点非常重要。

理论上你可以：

$$
\Delta\theta
=

asin(KI_{hf,q})
$$

但实际控制器通常不会这么干。

因为：

* 非线性
* 磁饱和
* 参数变化
* 噪声
* 死区
* PWM非理想

都会让这个关系变复杂。

所以实际一般做：

> **构造一个与角度误差近似成正比的误差信号，然后丢给PLL/PI。**

也就是：

$$
e_\theta \approx K\sin(2\Delta\theta)
$$

在小角度范围：

$$
\sin(2\Delta\theta)\approx2\Delta\theta
$$

于是：

$$
e_\theta\approx2K\Delta\theta
$$

这就非常漂亮了：

```text
真实角度
   ↓
角度误差 Δθ
   ↓
HFI产生误差信号
   ↓
PI / PLL
   ↓
修正估计角度
```

---

# 12. 为什么这里经常出现“2倍角”？

你以后看HFI论文，大概率会碰到：

$$
\sin(2\theta)
$$

不要懵。

因为电感具有：

> **180°周期性**

电机转子：

```text
0°
```

和：

```text
180°
```

从磁阻/电感角度看可能表现一样。

所以HFI经常得到：

$$
2\theta
$$

而不是：

$$
\theta
$$

这就是HFI的一个经典问题：

> **极性模糊（polarity ambiguity）**

---

# 13. 所以HFI实际上是在干什么？

现在你可以把它理解成：

```text
                 估计转子角度
                       │
                       ↓
                 打高频探针
                       │
                       ↓
              电机产生高频电流
                       │
                       ↓
                滤掉普通电流
                       │
                       ↓
                  同步解调
                       │
                       ↓
              得到高频响应信息
                       │
                       ↓
                 判断Δθ
                       │
                       ↓
                 PLL / PI
                       │
                       ↓
                 修正θ̂e
```

所以HFI不是：

> “直接测电角度。”

而是：

> **不断问电机：“我现在认为你的d轴在这里，对不对？”**

电机通过高频电流响应告诉你：

> “不对，你偏了XX°。”

然后控制器不断修正。

---

# 14. 最后把SMO和HFI放一起，你就彻底明白了

### SMO：

```text
电机自己转
    ↓
产生BEMF
    ↓
我观察BEMF
    ↓
推算θ
```

所以：

$$
\omega\rightarrow0
\Rightarrow E\rightarrow0
$$

SMO没东西看。

---

### HFI：

```text
电机转不转无所谓
        ↓
我主动打高频
        ↓
观察电感响应
        ↓
判断转子方向
```

所以：

$$
\omega=0
$$

依然可以工作。

---

## 一句话记忆

> **SMO是在“听电机自己说话”；HFI是在“主动敲电机一下，然后听它怎么回应”。**

而你现在真正需要掌握的HFI核心，就三步：

**① 注入高频 → ② 同步解调 → ③ 从解调结果构造角度误差信号。**

其中真正最值得你继续搞的是第③步：**到底怎么从 $$(i_{hf})$$ 数学推导出 $$ (e_\theta\propto\sin(2\Delta\theta))$$，以及这个误差信号怎么进入PLL把角度锁住。**这一步搞懂了，HFI基本就通了。


--------------
------------
-------------

高频注入（HFI，High Frequency Injection）和你前面研究的 **SMO+PLL** 是两条完全不同路线。

先用一句话概括：

> **SMO 是“看电机产生的反电动势”，HFI 是“主动给电机打一个探测信号，然后通过电机磁特性判断转子在哪”。**

所以：

* SMO：靠 **速度产生的信息**
* HFI：靠 **电机本身磁阻/电感变化的信息**

HFI 最大优势：

$$
\boxed{0RPM也能知道转子角度}
$$

因为它不依赖反电动势。高频注入正是利用电机的磁阻凸极性或饱和凸极性，通过高频电压产生带有转子位置的信息的电流响应。

---

# 1. 为什么需要高频注入？

先回顾 SMO。

PMSM：

$$
E=K_e\omega
$$

反电动势：

$$
E
$$

跟速度成正比。

比如你的2208：

低速：

```
0 RPM
 |
 ↓
BEMF≈0
```

SMO：

```
输入:
Uα Uβ
Iα Iβ

输出:
Eα Eβ

但是:
E≈0
```

所以：

```
0~几百RPM

SMO:
❌ 很困难
```

---

而 HFI：

它不看：

$$
E=K_e\omega
$$

它看：

$$
L(\theta)
$$

也就是：

> 不同转子角度，电机呈现的高频电感不同。

---

# 2. HFI核心原理

假设给电机注入一个高频电压：

$$
u_h=A\sin(\omega_ht)
$$

例如：

```
20kHz FOC

注入:
1kHz 高频信号
```

电机响应：

$$
i_h=\frac{u_h}{j\omega_h L}
$$

关键来了：

如果：

$$
L
$$

随着转子角度变化：

那么：

$$
i_h
$$

也会随着角度变化。

于是：

```
注入电压
      |
      ↓
 电机磁路
      |
      ↓
 高频电流响应
      |
      ↓
 解调
      |
      ↓
 转子角度
```

---

# 3. 电机为什么会有角度相关的电感？

核心：

## 凸极效应 Saliency

---

## IPMSM

内嵌式：

```
        N

   ┌────────┐
   │  磁钢  │
   └────────┘

        S
```

d轴：

磁阻小

q轴：

磁阻大

所以：

$$
L_d \neq L_q
$$

例如：

$$
L_d=0.3mH
$$

$$
L_q=1mH
$$

明显。

---

## SPMSM（你的2208）

表贴式：

```
   N

 [磁钢]

   S
```

理论：

$$
L_d≈L_q
$$

所以：

没有明显凸极。

但是：

实际：

因为磁饱和：

```
大电流
 ↓
铁芯磁导变化
 ↓
产生饱和凸极
```

也存在一点：

$$
L_d\neq L_q
$$

不过弱很多。

所以：

> HFI最喜欢IPMSM，不太喜欢SPMSM。


---

# 4. HFI完整系统结构

整体：

```
             速度低
               |
               ↓

       高频注入信号
               |
               ↓
          +-----------+
          |           |
FOC电压 → |  SVPWM    |
          |           |
          +-----------+
               |
               ↓
             电机
               |
               ↓
          高频电流响应
               |
               ↓
          解调算法
               |
               ↓
          位置误差
               |
               ↓
             PLL
               |
               ↓
          θe_est
```

---

# 5. 高频注入有哪些方式？

主要三个：

---

## ① 脉振注入（最常用）

也叫：

Pulsating Injection

在估计d轴注入：

$$
u_d^h=A\sin(\omega_ht)
$$

例如：

```
估计d轴:

↑
|
|
+ 高频
|
|
```

---

优点：

* 实现简单
* 对SPMSM也比较友好

现在很多工程使用这种。

---

## ② 旋转注入

在αβ坐标：

产生旋转高频电压：

$$
u_\alpha=A\cos(\omega_ht)
$$

$$
u_\beta=A\sin(\omega_ht)
$$

像一个高速小电压矢量：

```
      ↑

   ↗

←     →

   ↘
```

---

## ③ 方波注入

不用sin：

直接：

```
+V
-V
+V
-V
```

优点：

计算简单。

很多MCU喜欢。

---

# 6. 具体实现流程（重点）

假设：

你的FOC：

20kHz

那么：

加入：

1kHz 高频注入。

---

## Step 1：产生高频信号

例如：

$$
u_h=A\sin(2\pi f_ht)
$$

代码：

```c
hf_angle += 2*pi*fh*Ts;

uh = Amp*sinf(hf_angle);
```

比如：

```
fh=1000Hz

Ts=50us
```

---

## Step 2：叠加到FOC电压

正常：

$$
U_d
$$

变：

$$
U_d'=U_d+u_h
$$

代码：

```c
Ud += uh;
```

然后：

```
Ud/Uq

↓

SVPWM

↓

电机
```

---

# 7. 电流采样

注入以后：

电流：

$$
i=i_f+i_h
$$

包含：

低频：

```
正常FOC电流
```

高频：

```
注入响应
```

所以：

ADC采到：

```
Iq
     /\/\/\
----/\/\/\/\----
```

---

# 8. 高频电流提取（核心）

你不能直接用：

因为：

里面：

```
FOC电流
+
高频电流
```

需要：

## 带通滤波

例如：

注入：

1kHz

设计：

```
500Hz~2000Hz BPF
```

得到：

$$
i_h
$$

---

# 9. 同步解调

这是HFI最核心。

假设：

注入：

$$
u_h=A\sin(\omega_ht)
$$

得到：

$$
i_h
$$

乘回来：

$$
i_h\sin(\omega_ht)
$$

为什么？

因为：

$$
sin^2(x)
$$

平均：

$$
\frac12
$$

于是：

高频变：

直流量。

流程：

```
高频电流

      ×

参考sin

      ↓

低通

      ↓

幅值/相位信息
```

这叫：

同步解调。

---

# 10. 怎么得到角度？

重点来了。

假设：

估计角：

$$
\hat\theta
$$

真实：

$$
\theta
$$

两个坐标：

```
真实d-q

        d
        |
        |
        ●


估计d-q

       /
      /
```

有误差：

$$
\Delta\theta
$$

那么高频响应：

会出现：

q轴分量。

理想：

```
高频电流
完全d轴

Iq_h=0
```

错误：

```
有Iq_h
```

所以：

$$
I_{qh}
$$

就是误差信号。

---

# 11. HFI里的PLL

结构：

```
Iq_h

 ↓

PI

 ↓

角速度修正

 ↓

积分

 ↓

θ_est
```

和你前面学的PLL非常像。

区别：

SMO PLL：

误差：

$$
E_d
$$

HFI PLL：

误差：

$$
I_{qh}
$$

---

# 12. HFI代码结构

大概：

```c
void HFI_Update()
{

    //1.产生高频信号

    hf_phase += hf_omega*Ts;

    float uh =
        amp*sinf(hf_phase);



    //2.加入d轴

    Ud += uh;



    //3.SVPWM输出

    SVPWM(Ud,Uq);



    //4.采样电流

    Read_ADC();



    //5.提取高频电流

    ih =
       BandPassFilter(I);



    //6.同步解调

    demod =
       ih*sinf(hf_phase);



    //7.低通

    pos_error =
       LPF(demod);



    //8.PLL

    theta += PLL(pos_error);

}
```

---

# 13. HFI启动过程

这个是它最大优势。

因为：

0转：

```
没有BEMF
```

但是：

注入：

```
有高频电流
```

所以：

启动：

```
静止

↓

HFI检测初始角

↓

得到θ

↓

FOC闭环启动

↓

加速

↓

切SMO
```

---

# 14. 为什么实际产品喜欢 HFI+SMO？

因为：

两者互补。

低速：

```
HFI
|
0~几百RPM
```

高速：

```
SMO
|
几百RPM以上
```

组合：

```
0RPM
 |
 |
HFI
 |
300RPM
 |
 |
SMO
 |
高速
```

这是工业常见方案。

---

# 15. HFI和你的2208适合吗？

你的：

```
2208-80T
KV100
7对极
SPMSM
Rs≈8Ω
Ls≈4.25mH
```

评价：

## 优点

你的电机：

小功率

电感较大：

$$
4.25mH
$$

高频响应容易测。

---

## 缺点

表贴磁钢：

$$
Ld≈Lq
$$

凸极弱。

所以：

HFI效果：

```
IPMSM:
★★★★★

你的2208:
★★★
```

可能需要：

* 较大注入电压
* 更好的滤波
* 饱和凸极利用

---

# 16. 和你的SMO+PLL比较

|        | SMO+PLL | HFI    |
| ------ | ------- | ------ |
| 0RPM   | ❌       | ⭐⭐⭐⭐⭐  |
| 低速     | 差       | 强      |
| 高速     | 强       | 一般     |
| 算法复杂度  | 中       | 高      |
| 噪声     | 低       | 有啸叫    |
| SPMSM  | 好       | 一般     |
| 需要电机参数 | 需要      | 较少     |
| ESP32  | 适合      | 可以但压力大 |

---

# 17. 如果我是你这个项目

你的路线：

```
现在：
有感FOC
      |
      ↓
SMO
      |
      ↓
SMO+PLL
      |
      ↓
高速无感


以后：
加入HFI

      ↓

HFI低速启动

      ↓

SMO高速接管
```

不要一开始搞HFI。

原因：

你现在最大的问题其实不是低速，而是：

* 电角度方向
* SMO参数
* PLL
* 电流环

这些搞定后再加HFI。

---

# 最后一句总结：

**HFI的本质不是“观察反电动势”，而是“用高频电压主动敲一下电机，然后听电机磁路的回声，通过这个回声判断转子位置”。**

数学核心：

$$
\boxed{
注入u_h
\rightarrow
产生i_h
\rightarrow
同步解调
\rightarrow
位置误差
\rightarrow
PLL
\rightarrow
\theta_e
}
$$

而你现在的：

$$
\boxed{
SMO+PLL
}
$$

是：

$$
\boxed{
BEMF
\rightarrow
PLL
\rightarrow
\theta_e
}
$$

两个最大的区别：

> **SMO靠“速度产生的信息”，HFI靠“磁极结构产生的信息”。**

对于你的2208，我建议最终目标是：

**低速 HFI + 高速 SMO-PLL 混合无感。** 

-----------------------
-----------------------
--------------------------

# 高频无感FOC使用步骤

你这个问题问到了**真正工程应用层面**。

前面讲的是 HFI 原理，现在我们讲：

> **实际无感 FOC 产品里面，高频注入到底怎么放进去？什么时候用？怎么切换？代码流程是什么？**

先说结论：

**工业上几乎不会单独用HFI跑全速。**

实际方案通常是：

$$
\boxed{
低速：HFI
\rightarrow
中高速：SMO/PLL
}
$$

也就是：

```
0RPM
  |
  |  HFI负责找角度、启动
  |
200~1000RPM
  |
  |  切换
  |
高速
  |
  |  SMO/PLL负责运行
```

---

# 1. 为什么HFI只负责低速？

原因：

## HFI依赖电感信息

高频注入：

$$
i_h=\frac{u_h}{j\omega_h L}
$$

它测的是：

$$
L(\theta)
$$

和速度没有关系。

所以：

0 RPM：

```
转子静止

HFI:
有响应
```

---

但是高速：

电机本身：

$$
E=K_e\omega
$$

已经有很强BEMF：

```
高速:

SMO
 ↓
很准
```

继续HFI：

会产生：

* 电流纹波
* 噪声
* 转矩脉动

所以：

高速关闭HFI。

---

# 2. 一个真实无感FOC结构

实际产品大概：

```
              电流采样
                  |
                  |
                  ↓

             无感位置估计

        ┌───────────────┐
        │               │
        │               │
     HFI估计器       SMO估计器
        │               │
        │               │
        ↓               ↓
    低速角度        高速角度


             ↓

          角度选择器

             ↓

          FOC Park

             ↓

          电流环

             ↓

          SVPWM
```

---

# 3. 启动阶段怎么用HFI？

假设电机静止。

此时：

```
转子角度未知
```

不能直接：

```
Park(theta)
```

因为theta不知道。

所以：

## 第一步：HFI注入

例如：

给估计d轴：

$$
V_d=V_{dc}+A\sin(\omega_ht)
$$

比如：

```
高频：

1kHz

幅值：

0.5V
```

叠加：

```
Ud = Ud_PI + Uh
```

---

# 4. HFI如何找到初始角？

假设：

你猜：

```
theta_est=0°
```

实际上：

```
theta_real=30°
```

那么：

你的注入轴：

```
错误
```

于是高频电流：

出现：

```
Iq_h
```

类似：

```
角度错误
      |
      ↓
高频q轴电流
      |
      ↓
PLL修正
```

不断调整：

```
theta_est

0°
10°
20°
30°
```

直到：

```
Iq_h≈0
```

说明：

```
注入轴 = 转子磁轴
```

---

# 5. HFI启动完成以后怎么办？

假设：

```
HFI找到:

theta=45°
```

然后：

进入：

```
开环加速
```

为什么？

因为：

HFI虽然知道角度：

但是：

它不擅长高速。

所以：

下一步：

```
HFI角度

↓

给FOC

↓

逐渐提高转速

↓

BEMF增加
```

---

# 6. 什么时候切SMO？

判断条件一般：

不是固定RPM。

看：

## 条件1：BEMF大小

计算：

$$
E=\sqrt{E_\alpha^2+E_\beta^2}
$$

例如：

```
E > 阈值
```

说明：

SMO可靠。

---

## 条件2：SMO PLL锁定

例如：

看：

PLL误差：

$$
E_d
$$

或者：

角度误差：

```
theta_HFI

和

theta_SMO
```

差距：

小于：

5°

---

## 条件3：速度

例如：

```
500RPM以上
```

---

实际：

三个条件一起。

---

# 7. 切换过程非常关键

很多人这里炸。

错误：

```
HFI theta

突然

SMO theta
```

比如：

```
HFI:

100°


SMO:

130°
```

突然切：

```
100→130°
```

FOC：

瞬间30°误差。

结果：

```
Iq爆炸
电机抖
```

---

正确：

叫：

## 无扰切换

方法：

---

### 方法1：SMO角度校准HFI角度

切换前：

```
theta_SMO
```

不要直接用。

而是：

校正：

$$
\theta_{SMO}
=
\theta_{SMO}
+
offset
$$

让：

```
theta_SMO≈theta_HFI
```

---

### 方法2：渐变融合

例如：

权重：

$$
\theta=
(1-k)\theta_{HFI}
+k\theta_{SMO}
$$

k:

0→1

过程：

```
HFI 100%

↓

HFI 50%
SMO 50%

↓

SMO100%
```

---

# 8. 实际代码结构

比如你的ESP32：

20kHz FOC。

---

## 状态机

```c
typedef enum
{
    SENSORLESS_HFI,
    SENSORLESS_BLEND,
    SENSORLESS_SMO

}sensorless_state_t;
```

---

## 主流程

```c
switch(state)
{


case SENSORLESS_HFI:


    HFI_Update();


    theta =
        hfi.theta;



    if(SMO_Locked())
    {
        state =
        SENSORLESS_BLEND;
    }


break;




case SENSORLESS_BLEND:


    blend++;

    theta =
       (1-blend)*hfi.theta
       +
       blend*smo.theta;


    if(blend>=1)
    {
        state=
        SENSORLESS_SMO;
    }

break;



case SENSORLESS_SMO:


    theta=
       smo.theta;


break;


}
```

---

# 9. HFI和FOC电流环怎么结合？

重点：

HFI不是独立控制器。

它只是：

修改电压。

正常：

```
Id PI
Iq PI

↓

Ud
Uq

↓

SVPWM
```

加入HFI：

变成：

```
Id PI
Iq PI

↓

Ud
Uq


+

Ud_hfi

↓

SVPWM

```

也就是：

$$
U_d=U_{dPI}+U_{HF}
$$

---

# 10. HFI注入频率怎么选？

你的：

FOC：

20kHz

一般：

注入：

500Hz~2kHz

例如：

```
PWM 20kHz

HFI:

1kHz
```

原因：

太低：

```
容易和控制频率混
```

太高：

```
电机阻抗太大
响应小
```

---

# 11. HFI对ADC采样有什么要求？

这个非常重要。

你现在：

ADC:

20kHz

HFI:

1kHz

理论：

可以。

但是：

最好：

采样同步。

比如：

PWM中心：

```
↑

ADC采样

↑

ADC采样
```

因为：

HFI电流：

幅值可能很小。

---

# 12. HFI最难的地方是什么？

不是注入。

注入很简单：

```c
Ud += sin()
```

真正难：

## ① 高频电流提取

需要：

带通滤波。

## ② 同步解调

乘：

```
sin
cos
```

## ③ 角度误差生成

得到：

```
position error
```

## ④ 切换

这是工程难点。

---

# 13. 对你的ESP32项目，我建议这样设计

你的硬件：

```
ESP32
20kHz PWM
INA240
AS5600
2208
```

推荐架构：

```
启动:

HFI

↓

100~300RPM

↓

HFI+SMO同时运行

↓

SMO PLL锁定

↓

切SMO


高速:

SMO+PLL
```

---

# 14. 但是你的2208有一个现实问题

你的电机：

2208-80T

KV100

12V

大概率：

SPMSM。

HFI效果：

不是最优。

原因：

$$
Ld≈Lq
$$

没有明显凸极。

所以：

可能：

```
IPMSM:

HFI★★★★★


你的:

HFI★★★
```

如果做实验：

可以。

但是如果目标：

稳定无感运行：

你的主线还是：

$$
\boxed{
SMO+PLL
}
$$

---

# 15. 工业方案总结

现在很多电机控制：

```
                位置估计

        ┌──────────────┐

        HFI          SMO

        │             │

    0~低速        高速


        └──────┬───────┘

               ↓

            角度融合

               ↓

             FOC
```

也就是：

> **HFI解决“看不见的时候”，SMO解决“跑起来以后”。**

---

结合你现在的学习路线：

你已经在搞：

* Clarke/Park
* 电流环
* SMO
* PLL

那么下一步正确顺序：

1. **先把SMO+PLL跑通**
2. 做速度闭环
3. 再研究HFI启动
4. 最后做HFI/SMO融合

这样路线最接近工业无感FOC开发流程。
