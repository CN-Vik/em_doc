你问的这个公式：

$$
E_\alpha=-E\sin\theta_e
$$

$$
E_\beta=E\cos\theta_e
$$

其实是**无感FOC里最容易让人懵的地方**。

很多文章直接写出来，好像天生存在一样。

实际上它不是推导出来的“电机定律”，而是：

> **反电动势矢量在αβ静止坐标系上的投影关系。**

我从最基础开始讲。

---

# 1. 首先理解：反电动势是什么东西？

PMSM电机转动时：

转子磁钢产生磁场：

```
        N
        ↑
        |
        |
S -------+------- N
        |
        |
        ↓
        S
```

这个磁场旋转。

根据法拉第电磁感应：

旋转磁场切割定子绕组：

产生：

$$
E
$$

也就是反电动势。

这个反电动势本质是一个**旋转矢量**。

---

# 2. 什么叫反电动势矢量？

假设：

反电动势大小：

$$
|E|=10V
$$

方向：

当前电角度：

$$
\theta_e=30^\circ
$$

那么反电动势矢量：

```
          β轴

          ↑
          |
          |
          |       E
          |      /
          |     /
          |    /
          |   /
----------+----------------→ α轴


          θe
```

它在αβ平面里面旋转。

---

# 3. αβ坐标是什么？

FOC里面有两个坐标系。

## 静止坐标 αβ

固定不动：

```
          β

          ↑
          |
          |
          |
----------+--------→ α
```

## 转子坐标 dq

跟着转子转：

```
              q

              ↑

              |

d ------------+
```

---

# 4. 一个矢量怎么表示？

任何二维矢量：

都可以拆成：

水平分量：

$$
X
$$

垂直分量：

$$
Y
$$

比如：

```
              E
             /
            /
           /
          /
---------+------------

```

数学：

水平：

$$
E_x=E\cos\theta
$$

垂直：

$$
E_y=E\sin\theta
$$

这是初中三角函数。

---

# 5. 那为什么这里是：

$$
E_\alpha=-E\sin\theta
$$

而不是：

$$
E\cos\theta
$$

关键来了：

## 因为反电动势方向和磁链方向差90°

PMSM：

转子磁链：

$$
\psi_f
$$

方向：

```
          ψ

          ↑


----------+--------→
```

电角度：

$$
\theta_e
$$

但是：

反电动势：

$$
E
$$

满足：

$$
E=\frac{d\psi}{dt}
$$

也就是说：

磁链旋转产生电压。

微分一个正弦：

$$
\frac{d}{dt}\cos\theta
=
-\sin\theta
$$

所以：

反电动势天然领先磁链90°。

---

# 6. 用数学看

假设转子磁链：

$$
\psi_\alpha
=
\psi_f\cos\theta_e
$$

$$
\psi_\beta
=
\psi_f\sin\theta_e
$$

磁链：

```
             β

             ↑
             |
             |
             |
             /
            /
-----------+---------- α

```

---

反电动势：

$$
E=\frac{d\psi}{dt}
$$

因为：

$$
\frac{d\theta}{dt}=\omega_e
$$

所以：

α轴：

$$
E_\alpha
=
-\omega_e\psi_f\sin\theta_e
$$

β轴：

$$
E_\beta
=
\omega_e\psi_f\cos\theta_e
$$

定义：

$$
E=\omega_e\psi_f
$$

于是：

得到：

$$
\boxed{
E_\alpha=-E\sin\theta_e
}
$$

$$
\boxed{
E_\beta=E\cos\theta_e
}
$$

这就是来源。

---

# 7. 用一个具体角度验证

假设：

$$
\theta_e=0°
$$

那么：

$$
E_\alpha=-E\sin0
$$

$$
=0
$$

$$
E_\beta=E\cos0
$$

$$
=E
$$

所以：

```
β

↑
|
|
E
|
|
+------------→α
```

反电动势在β轴。

---

如果：

$$
\theta_e=90°
$$

那么：

$$
E_\alpha=-E
$$

$$
E_\beta=0
$$

变成：

```
β


+------------→α

← E
```

说明矢量旋转了90°。

---

# 8. 那为什么PLL里面能用Ed判断角度？

因为：

你拿一个错误角度：

$$
\hat\theta
$$

去Park。

如果：

$$
\hat\theta=\theta
$$

那么：

反电动势刚好落在q轴：

```
        q

        ↑
        |
        |
--------+--------d
```

所以：

d轴投影：

$$
E_d=0
$$

---

如果角度错了：

```
          E
         /
        /
-------+------d
```

会产生：

d轴分量：

$$
E_d\neq0
$$

PLL就是不断调整：

$$
\hat\theta
$$

直到：

$$
E_d=0
$$

---

# 9. 你现在只需要记住这一条逻辑

### 第一步：

电机旋转：

产生反电动势矢量：

$$
E
$$

### 第二步：

这个矢量在αβ坐标投影：

$$
\boxed{
E_\alpha=-E\sin\theta_e
}
$$

$$
\boxed{
E_\beta=E\cos\theta_e
}
$$

### 第三步：

SMO估计：

$$
E_\alpha,E_\beta
$$

### 第四步：

PLL根据这个矢量方向追踪：

$$
\theta_e
$$

---

你之前困惑的核心其实就在这里：

**SMO不是直接输出角度，它输出的是一个旋转的反电动势矢量。**

而：

$$
E_\alpha=-E\sin\theta
$$

$$
E_\beta=E\cos\theta
$$

就是：

> “这个旋转矢量在固定αβ坐标系里的数学表示。”

理解这个，后面的 atan2 和 PLL 就全部串起来了。

---------
---------
可以。把 **SMO输出反电动势 → PLL → 电角度** 的过程，用最核心数学公式总结如下。

假设你的 SMO 定义：

$$
E_\alpha=-E\sin\theta_e
$$

$$
E_\beta=E\cos\theta_e
$$

---

# 1. 输入反电动势

SMO输出：

$$
E_\alpha
$$

$$
E_\beta
$$

目标：

求：

$$
\theta_e
$$

---

# 2. 用当前估计角度做Park变换

PLL内部有一个估计角：

$$
\hat{\theta}_e
$$

把反电动势旋转到PLL坐标系：

d轴：

$$
E_d
=
E_\alpha\cos\hat\theta_e
+
E_\beta\sin\hat\theta_e
$$

q轴：

$$
E_q
=
-E_\alpha\sin\hat\theta_e
+
E_\beta\cos\hat\theta_e
$$

---

# 3. 得到相位误差

代入：

$$
E_\alpha=-E\sin\theta_e
$$

$$
E_\beta=E\cos\theta_e
$$

得到：

$$
E_d
=
E\sin(\hat\theta_e-\theta_e)
$$

当角度有误差：

$$
\hat\theta_e\neq\theta_e
$$

则：

$$
E_d\neq0
$$

当锁定：

$$
\boxed{E_d=0}
$$

说明：

$$
\boxed{\hat\theta_e=\theta_e}
$$

---

# 4. 归一化误差

为了消除反电动势幅值影响：

$$
error
=
-\frac{E_d}{|E|}
$$

其中：

$$
|E|=
\sqrt{E_\alpha^2+E_\beta^2}
$$

---

# 5. PI控制器

误差进入PI：

比例：

$$
P=K_p error
$$

积分：

$$
I=K_i\int error\,dt
$$

输出电角速度：

$$
\boxed{
\omega_e
=
K_p error
+
K_i\int error dt
}
$$

---

# 6. 角速度积分得到角度

离散实现：

$$
\boxed{
\hat\theta_e(k)
=
\hat\theta_e(k-1)
+
\omega_e(k)T_s
}
$$

其中：

* \(T_s\)：PLL周期
* \(\omega_e\)：估计电角速度

---

# 7. 完整PLL数学流程

压缩成一条：

$$
\boxed{
E_{\alpha\beta}
}
$$

↓

Park：

$$
\boxed{
E_d=
E_\alpha\cos\hat\theta
+
E_\beta\sin\hat\theta
}
$$

↓

误差：

$$
\boxed{
error=-\frac{E_d}{|E|}
}
$$

↓

PI：

$$
\boxed{
\omega_e
=
K_p error
+
K_i\int error dt
}
$$

↓

积分：

$$
\boxed{
\hat\theta_e
=
\int\omega_e dt
}
$$

↓

输出：

$$
\boxed{\theta_e}
$$

---

# 8. 对应代码伪代码

```c
// 输入 SMO反电动势
Ealpha;
Ebeta;


// 当前PLL角度
theta;


// Park变换
Ed =
    Ealpha*cos(theta)
    +
    Ebeta*sin(theta);


// 反电动势幅值
E_mag =
sqrt(Ealpha*Ealpha+
     Ebeta*Ebeta);


// 相位误差
error =
-Ed/E_mag;


// PI
omega =
Kp*error
+
Ki*integral(error);


// 积分得到角度
theta += omega*Ts;
```

---

# 9. 最终整个无感FOC公式链

合起来就是：

$$
\boxed{
U_{\alpha\beta},I_{\alpha\beta}
}
$$

↓

**SMO**

$$
\boxed{
E_{\alpha\beta}
}
$$

↓

**PLL**

$$
\boxed{
E_d
}
$$

↓

$$
\boxed{
PI
}
$$

↓

$$
\boxed{
\omega_e
}
$$

↓

积分：

$$
\boxed{
\theta_e
}
$$

↓

FOC：

$$
Park(\theta_e)
$$

---

一句话总结：

**SMO负责从“电压、电流”推算出反电动势矢量；PLL负责让自己的旋转坐标系不断追踪这个反电动势矢量，当 \(E_d=0\) 时，说明PLL角度已经和电机真实电角度同步。**
