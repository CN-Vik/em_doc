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
