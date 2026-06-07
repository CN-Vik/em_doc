可以，这一段就是 **FOC 数学链路最后一步**：

```text
Ud / Uq
  ↓ Park 逆变换
Uα / Uβ
  ↓ Clarke 逆变换
Ua / Ub / Uc
  ↓ SVPWM 或 SPWM
PWM 占空比 dutyA / dutyB / dutyC
```

下面这版可以直接加到你的 Markdown 里。

# Ud、Uq 到三相电压 Ua/Ub/Uc，再到 PWM 占空比的数学关系

## 1. FOC 电压输出的整体链路

FOC 最终控制的是 DQ 坐标系下的两个电压量：

```text
Ud：D 轴电压
Uq：Q 轴电压
```

然后通过坐标变换得到三相电压：

```text
Ud, Uq
  ↓
Uα, Uβ
  ↓
Ua, Ub, Uc
  ↓
PWM 占空比
```

完整数学链路是：

```text
Ud/Uq -> Uα/Uβ -> Ua/Ub/Uc -> dutyA/dutyB/dutyC
```

---

# 2. Ud、Uq 的合成电压矢量

Ud 和 Uq 本质上是 DQ 坐标系下的两个正交电压分量。

它们可以合成一个电压矢量：

```text
Us = √(Ud² + Uq²)
```

其中：

```text
Us = 合成电压矢量幅值
Ud = D 轴电压
Uq = Q 轴电压
```

普通 BLDC / 表贴式 PMSM 学习阶段一般：

```text
Ud = 0
```

所以：

```text
Us = |Uq|
```

也就是说，学习阶段你主要控制的是：

```text
Uq 的大小
```

---

# 3. Park 逆变换：Ud/Uq -> Uα/Uβ

FOC 里面，Ud 和 Uq 是跟着转子旋转的坐标系。

但是三相绕组是固定在电机定子上的，所以要把旋转坐标系的电压变回静止坐标系。

这个过程叫：

```text
Park 逆变换
```

公式：

```text
Uα = Ud × cos(θe) - Uq × sin(θe)

Uβ = Ud × sin(θe) + Uq × cos(θe)
```

其中：

```text
θe = 电角度
Uα = α 轴电压
Uβ = β 轴电压
```

如果普通情况下：

```text
Ud = 0
```

那么公式变成：

```text
Uα = -Uq × sin(θe)

Uβ =  Uq × cos(θe)
```

也就是说：

```text
Uq 通过电角度 θe，生成一个旋转的 αβ 电压矢量。
```

这个旋转电压矢量再作用到三相绕组上，就能让电机产生连续转矩。

---

# 4. Clarke 逆变换：Uα/Uβ -> Ua/Ub/Uc

得到 Uα 和 Uβ 之后，需要转换成三相电压：

```text
Ua, Ub, Uc
```

这个过程叫：

```text
Clarke 逆变换
```

公式：

```text
Ua = Uα

Ub = -1/2 × Uα + √3/2 × Uβ

Uc = -1/2 × Uα - √3/2 × Uβ
```

也可以写成：

```text
Ua = Uα

Ub = -0.5 × Uα + 0.866 × Uβ

Uc = -0.5 × Uα - 0.866 × Uβ
```

这三个电压满足：

```text
Ua + Ub + Uc = 0
```

这说明三相电压是平衡的。

---

# 5. 如果 Ud = 0，三相电压公式可以展开

当：

```text
Ud = 0
```

Park 逆变换为：

```text
Uα = -Uq × sin(θe)

Uβ =  Uq × cos(θe)
```

代入 Clarke 逆变换：

```text
Ua = -Uq × sin(θe)
```

```text
Ub = 0.5 × Uq × sin(θe) + 0.866 × Uq × cos(θe)
```

```text
Uc = 0.5 × Uq × sin(θe) - 0.866 × Uq × cos(θe)
```

所以当 Ud = 0 时：

```text
三相电压 Ua、Ub、Uc 本质上是三个相差 120° 的正弦电压。
```

Uq 越大：

```text
三相电压幅值越大
电机力矩趋势越大
电流越大
```

---

# 6. 三相电压到 PWM 占空比的基本关系

三相逆变桥的每一相，本质上都是一个半桥。

如果母线电压是：

```text
Vbus
```

某一相的 PWM 占空比是：

```text
duty
```

那么这一相相对于母线负极的平均电压大概是：

```text
Uout = duty × Vbus
```

但是 FOC 里面的 Ua、Ub、Uc 通常是以中心点 0V 为参考的电压。

也就是说：

```text
duty = 0.5
```

对应中点电压：

```text
U = 0V
```

所以相电压到 PWM 占空比的基本公式是：

```text
dutyA = Ua / Vbus + 0.5

dutyB = Ub / Vbus + 0.5

dutyC = Uc / Vbus + 0.5
```

反过来看：

```text
Ua = (dutyA - 0.5) × Vbus

Ub = (dutyB - 0.5) × Vbus

Uc = (dutyC - 0.5) × Vbus
```

---

# 7. SPWM 的占空比关系

如果直接用 Clarke 逆变换得到的三相电压：

```text
Ua, Ub, Uc
```

直接计算占空比：

```text
dutyA = 0.5 + Ua / Vbus

dutyB = 0.5 + Ub / Vbus

dutyC = 0.5 + Uc / Vbus
```

这种方式可以理解为：

```text
SPWM
```

它的特点是：

```text
三相 duty 是正弦变化的。
```

但是要保证：

```text
0 ≤ dutyA ≤ 1

0 ≤ dutyB ≤ 1

0 ≤ dutyC ≤ 1
```

所以必须满足：

```text
-Vbus/2 ≤ Ua ≤ Vbus/2

-Vbus/2 ≤ Ub ≤ Vbus/2

-Vbus/2 ≤ Uc ≤ Vbus/2
```

如果超过这个范围，占空比就会超出 0%~100%，产生削顶失真。

---

# 8. SVPWM 零序注入关系

SVPWM 常用做法是：

```text
先算出 Ua、Ub、Uc
然后注入一个共同的零序电压 Uzero
```

先求三相电压最大值和最小值：

```text
Umax = max(Ua, Ub, Uc)

Umin = min(Ua, Ub, Uc)
```

零序电压：

```text
Uzero = -1/2 × (Umax + Umin)
```

然后得到修正后的三相电压：

```text
Ua' = Ua + Uzero

Ub' = Ub + Uzero

Uc' = Uc + Uzero
```

最后计算占空比：

```text
dutyA = 0.5 + Ua' / Vbus

dutyB = 0.5 + Ub' / Vbus

dutyC = 0.5 + Uc' / Vbus
```

这就是常见的：

```text
零序注入 SVPWM
```

---

# 9. 为什么要加 Uzero？

原始三相电压：

```text
Ua, Ub, Uc
```

满足：

```text
Ua + Ub + Uc = 0
```

加上同一个 Uzero 后：

```text
Ua' = Ua + Uzero
Ub' = Ub + Uzero
Uc' = Uc + Uzero
```

三相之间的线电压不变：

```text
Uab = Ua' - Ub' = Ua - Ub

Ubc = Ub' - Uc' = Ub - Uc

Uca = Uc' - Ua' = Uc - Ua
```

也就是说：

```text
加 Uzero 不改变电机真正看到的线电压。
```

但是它可以把三相电压整体上移或下移，让 PWM 更好地利用母线电压。

所以 SVPWM 的作用可以理解成：

```text
不改变电机有效线电压的前提下，
把三相 PWM 占空比分配得更合理，
提高母线电压利用率。
```

---

# 10. SPWM 和 SVPWM 的占空比对比

## 10.1 SPWM

```text
dutyA = 0.5 + Ua / Vbus

dutyB = 0.5 + Ub / Vbus

dutyC = 0.5 + Uc / Vbus
```

特点：

```text
简单
好理解
母线利用率稍低
```

---

## 10.2 SVPWM

```text
Uzero = -0.5 × (Umax + Umin)
```

```text
Ua' = Ua + Uzero

Ub' = Ub + Uzero

Uc' = Uc + Uzero
```

```text
dutyA = 0.5 + Ua' / Vbus

dutyB = 0.5 + Ub' / Vbus

dutyC = 0.5 + Uc' / Vbus
```

特点：

```text
母线利用率更高
输出电压能力更强
工程上更常用
```

---

# 11. 电压限制关系

Ud 和 Uq 的合成电压是：

```text
Us = √(Ud² + Uq²)
```

这个电压不能无限大，受母线电压限制。

对于 SPWM，比较保守的线性范围可以理解为：

```text
Us ≤ 0.5 × Vbus
```

对于 SVPWM，理论线性范围大约可以到：

```text
Us ≤ Vbus / √3
```

也就是：

```text
Us ≤ 0.577 × Vbus
```

如果 Vbus = 12V：

```text
SPWM 线性区大约：

Us ≤ 6V
```

```text
SVPWM 线性区大约：

Us ≤ 6.9V
```

但是新手调试时不要贴着极限用，建议：

```text
Us ≤ 0.1 × Vbus ~ 0.3 × Vbus
```

对于 12V：

```text
Us ≈ 1.2V ~ 3.6V
```

学习阶段更安全。

---

# 12. 电压模式 FOC 的完整数学链路

电压模式 FOC 里，一般：

```text
Ud = 0
Uq = 控制器输出
```

完整公式如下：

## 12.1 DQ 电压

```text
Ud = 0

Uq = 位置环或速度环输出
```

合成电压：

```text
Us = √(Ud² + Uq²)
```

---

## 12.2 Park 逆变换

```text
Uα = Ud × cos(θe) - Uq × sin(θe)

Uβ = Ud × sin(θe) + Uq × cos(θe)
```

如果 Ud = 0：

```text
Uα = -Uq × sin(θe)

Uβ =  Uq × cos(θe)
```

---

## 12.3 Clarke 逆变换

```text
Ua = Uα

Ub = -0.5 × Uα + 0.866 × Uβ

Uc = -0.5 × Uα - 0.866 × Uβ
```

---

## 12.4 SVPWM 零序注入

```text
Umax = max(Ua, Ub, Uc)

Umin = min(Ua, Ub, Uc)

Uzero = -0.5 × (Umax + Umin)
```

```text
Ua' = Ua + Uzero

Ub' = Ub + Uzero

Uc' = Uc + Uzero
```

---

## 12.5 转换成 PWM 占空比

```text
dutyA = 0.5 + Ua' / Vbus

dutyB = 0.5 + Ub' / Vbus

dutyC = 0.5 + Uc' / Vbus
```

最终：

```text
dutyA、dutyB、dutyC 就是三相 PWM 占空比。
```

---

# 13. 一个简单数值例子

假设：

```text
Vbus = 12V

Ud = 0V

Uq = 2V

θe = 0°
```

因为：

```text
sin(0°) = 0

cos(0°) = 1
```

Park 逆变换：

```text
Uα = -Uq × sin(θe) = -2 × 0 = 0V

Uβ =  Uq × cos(θe) =  2 × 1 = 2V
```

Clarke 逆变换：

```text
Ua = Uα = 0V

Ub = -0.5 × 0 + 0.866 × 2 = 1.732V

Uc = -0.5 × 0 - 0.866 × 2 = -1.732V
```

所以：

```text
Ua = 0V

Ub = 1.732V

Uc = -1.732V
```

如果直接用 SPWM：

```text
dutyA = 0.5 + 0 / 12 = 0.5

dutyB = 0.5 + 1.732 / 12 ≈ 0.644

dutyC = 0.5 - 1.732 / 12 ≈ 0.356
```

所以：

```text
dutyA ≈ 50.0%

dutyB ≈ 64.4%

dutyC ≈ 35.6%
```

这就是某一个电角度下，Uq 经过变换后得到的三相 PWM 占空比。

---

# 14. 重点总结

FOC 从 Ud/Uq 到 PWM 的数学关系是：

```text
第一步：

Us = √(Ud² + Uq²)
```

```text
第二步：

Uα = Ud × cos(θe) - Uq × sin(θe)

Uβ = Ud × sin(θe) + Uq × cos(θe)
```

```text
第三步：

Ua = Uα

Ub = -0.5 × Uα + 0.866 × Uβ

Uc = -0.5 × Uα - 0.866 × Uβ
```

```text
第四步，SVPWM：

Uzero = -0.5 × (max(Ua, Ub, Uc) + min(Ua, Ub, Uc))
```

```text
第五步：

Ua' = Ua + Uzero

Ub' = Ub + Uzero

Uc' = Uc + Uzero
```

```text
第六步：

dutyA = 0.5 + Ua' / Vbus

dutyB = 0.5 + Ub' / Vbus

dutyC = 0.5 + Uc' / Vbus
```

一句话理解：

```text
Ud/Uq 决定电压矢量的方向和大小；
θe 决定这个矢量在电机定子坐标系中的位置；
Ua/Ub/Uc 是这个矢量分配到三相绕组上的电压；
PWM duty 是用母线电压合成这些三相电压的占空比。
```

重点记这个就够了：

```text
Ud/Uq 是 FOC 想要的旋转电压矢量；
Ua/Ub/Uc 是实际三相绕组需要的电压；
PWM duty 是用母线电压把 Ua/Ub/Uc 合成出来。
```

普通学习阶段基本就是：

```text
Ud = 0
调 Uq
用 θe 做 Park 逆变换
再转成三相 PWM
```


-----
------
-----

# FOC 数学公式、Ud/Uq 作用意义与调参范围

## 1. FOC 的核心思想

FOC，也就是磁场定向控制，核心思想是：

```text
把三相电机的交流量，转换到跟随转子磁场旋转的 DQ 坐标系里控制。
```

在 DQ 坐标系中：

```text
D 轴：对准转子磁场方向
Q 轴：垂直于转子磁场方向
```

对于普通 BLDC / 表贴式 PMSM 来说，可以简单理解为：

```text
D 轴负责磁场
Q 轴负责力矩
```

所以常见控制目标是：

```text
Id = 0
Iq 用来控制力矩
```

---

## 2. 机械角度与电角度

编码器读到的一般是机械角度：

```text
θm = 机械角度
```

FOC 真正需要的是电角度：

```text
θe = 电角度
```

两者关系：

```text
θe = 极对数 × θm + 电角度偏移
```

也就是：

```text
θe = p × θm + θoffset
```

其中：

```text
p        = 电机极对数
θm       = 机械角度
θe       = 电角度
θoffset  = 电角度零点偏移
```

举例：

```text
如果电机是 7 对极：

机械角度转 1 圈，电角度转 7 圈。
```

所以：

```text
FOC 不是直接用机械角度控制三相电压，
而是用电角度控制三相电压。
```

---

## 3. Clarke 变换

Clarke 变换作用：

```text
三相静止坐标系 abc
转换成
两相静止坐标系 αβ
```

公式：

```text
Iα = Ia

Iβ = (Ia + 2Ib) / √3
```

如果三相电流满足：

```text
Ia + Ib + Ic = 0
```

那么只采样两相电流，也能推出第三相：

```text
Ic = -Ia - Ib
```

Clarke 变换的意义：

```text
把三相电流变成二维平面上的电流矢量。
```

---

## 4. Park 变换

Park 变换作用：

```text
两相静止坐标系 αβ
转换成
两相旋转坐标系 DQ
```

公式：

```text
Id = Iα × cos(θe) + Iβ × sin(θe)

Iq = -Iα × sin(θe) + Iβ × cos(θe)
```

其中：

```text
Id = D 轴电流
Iq = Q 轴电流
θe = 电角度
```

Park 变换的意义：

```text
把原本旋转变化的交流电流，转换成相对稳定的直流量 Id 和 Iq。
```

也就是说，FOC 的神奇之处就在这里：

```text
三相交流电机
经过坐标变换后
可以像直流电机一样控制
```

---

## 5. Id 和 Iq 的意义

## 5.1 Id 的意义

```text
Id 是 D 轴电流。
D 轴方向和转子永磁体磁场方向一致。
```

对于普通 BLDC / 表贴式 PMSM：

```text
Id 一般希望控制为 0。
```

原因：

```text
Id 不直接产生主要力矩。
Id 过大会增加发热。
Id 过大还可能影响永磁体磁场。
```

所以常规控制目标是：

```text
Id_target = 0
```

简单理解：

```text
Id = 0，表示不额外增强或削弱转子磁场。
```

---

## 5.2 Iq 的意义

```text
Iq 是 Q 轴电流。
Q 轴方向和转子磁场垂直。
```

Iq 主要负责产生电磁转矩。

简化理解：

```text
Iq 越大，电机力矩越大。
Iq 方向改变，电机力矩方向改变。
```

转矩近似公式：

```text
Te ≈ 1.5 × p × ψf × Iq
```

其中：

```text
Te = 电磁转矩
p  = 极对数
ψf = 永磁体磁链
Iq = Q 轴电流
```

所以在真正的电流闭环 FOC 里：

```text
Iq 基本可以理解成力矩控制量。
```

---

# 6. Ud 和 Uq 的意义

## 6.1 Ud 是什么

```text
Ud 是 D 轴电压。
```

它对应 D 轴方向的电压控制量。

对于普通 BLDC / 表贴式 PMSM：

```text
Ud 通常设置为 0。
```

也就是：

```text
Ud = 0
```

原因是：

```text
普通位置控制、速度控制、力矩控制，一般不需要主动调 Ud。
```

Ud 不为 0 时，会产生 D 轴电流趋势。

可能影响：

```text
1. 增加电机发热
2. 降低效率
3. 改变磁场状态
4. 严重时可能导致力矩变差
5. 高速弱磁时才可能有特殊用途
```

---

## 6.2 Uq 是什么

```text
Uq 是 Q 轴电压。
```

它对应 Q 轴方向的电压控制量。

在没有电流采样的电压模式 FOC 里：

```text
Uq 越大，电机输出力矩趋势越大。
```

但是要注意：

```text
电压模式下，Uq 不等于真实力矩。
```

因为真实力矩主要和 Iq 有关：

```text
力矩 ∝ Iq
```

而电压模式只是通过给 Uq 电压，让电机产生电流。

所以：

```text
电压模式 FOC：
Uq 是力矩趋势控制量。

电流模式 FOC：
Iq 才是真正的力矩控制量。
```

---

# 7. Ud/Uq 到 αβ 的变换

FOC 控制最终需要把 DQ 坐标系下的电压：

```text
Ud, Uq
```

转换回静止坐标系：

```text
Uα, Uβ
```

公式：

```text
Uα = Ud × cos(θe) - Uq × sin(θe)

Uβ = Ud × sin(θe) + Uq × cos(θe)
```

这一步叫：

```text
Park 逆变换
```

意义：

```text
把跟随转子旋转的 DQ 电压矢量，
转换回电机定子静止坐标系中的 αβ 电压矢量。
```

---

# 8. αβ 到三相电压的变换

然后把：

```text
Uα, Uβ
```

转换成三相电压：

```text
Ua, Ub, Uc
```

公式：

```text
Ua = Uα

Ub = -0.5 × Uα + √3/2 × Uβ

Uc = -0.5 × Uα - √3/2 × Uβ
```

这一步叫：

```text
Clarke 逆变换
```

意义：

```text
把二维电压矢量，转换成三相电机需要的三相电压。
```

---

# 9. SVPWM 的基本数学关系

得到三相电压后：

```text
Ua, Ub, Uc
```

可以通过 SVPWM 转成三相 PWM 占空比。

常见的零序注入 SVPWM 公式是：

```text
Umax = max(Ua, Ub, Uc)

Umin = min(Ua, Ub, Uc)

Uzero = -0.5 × (Umax + Umin)
```

然后：

```text
Ua' = Ua + Uzero

Ub' = Ub + Uzero

Uc' = Uc + Uzero
```

最后把电压转换成占空比：

```text
DutyA = Ua' / Vbus + 0.5

DutyB = Ub' / Vbus + 0.5

DutyC = Uc' / Vbus + 0.5
```

其中：

```text
Vbus = 母线电压
```

SVPWM 的作用：

```text
1. 把目标电压矢量转换成三相 PWM
2. 提高母线电压利用率
3. 让三相输出更平滑
4. 比普通 SPWM 更适合电机控制
```

---

# 10. 电压矢量大小

Ud 和 Uq 可以合成一个电压矢量：

```text
Us = √(Ud² + Uq²)
```

其中：

```text
Us = DQ 坐标系下的合成电压矢量大小
```

FOC 输出不能无限大，必须受母线电压限制。

理论上：

```text
Us 不能超过逆变器和 SVPWM 能输出的最大电压范围。
```

工程上常用安全限制：

```text
Us ≤ 0.3 × Vbus     新手调试安全区
Us ≤ 0.5 × Vbus     常规可用区
Us ≤ 0.577 × Vbus   SVPWM 理论线性区附近
```

对于 12V 母线：

```text
0.3 × Vbus ≈ 3.6V

0.5 × Vbus ≈ 6V

0.577 × Vbus ≈ 6.9V
```

新手阶段不建议一上来接近极限。

---

# 11. Ud 的调节范围建议

对于普通 BLDC / 表贴式 PMSM，学习阶段建议：

```text
Ud = 0
```

不要轻易调 Ud。

推荐范围：

```text
新手调试：
Ud = 0V

普通位置控制：
Ud = 0V

普通速度控制：
Ud = 0V

普通力矩控制：
Ud = 0V
```

特殊情况：

```text
高速弱磁控制时，可能使用负 Ud。
```

但是弱磁控制需要更完整的电流采样、电流环和保护，不适合新手阶段直接尝试。

所以可以记住一句话：

```text
普通 BLDC FOC：
Ud 默认等于 0。
主要调 Uq。
```

---

# 12. Uq 的调节范围建议

Uq 是主要调节量。

它影响：

```text
1. 电机力矩
2. 加速度
3. 位置回弹力度
4. 速度响应
5. 堵转电流
6. 发热
```

Uq 越大：

```text
力矩越大
响应越快
电流越大
发热越明显
震动风险越大
```

Uq 越小：

```text
力矩越小
响应越慢
更安全
不容易发热
可能带不动负载
```

---

## 12.1 12V 小电机学习阶段 Uq 建议

对于 12V 母线的小 BLDC，初期建议：

```text
空载测试：
Uq = 0.3V ~ 0.8V

轻微转动：
Uq = 0.8V ~ 1.5V

有明显力矩：
Uq = 1.5V ~ 3.0V

较强力矩：
Uq = 3.0V ~ 5.0V
```

新手阶段建议限制在：

```text
Uq ≤ 1.0V ~ 2.0V
```

调通以后再逐步增加。

---

## 12.2 Uq 和 Vbus 的比例关系

可以按母线电压比例理解。

安全调试区：

```text
Uq ≈ 5% ~ 15% × Vbus
```

常规使用区：

```text
Uq ≈ 10% ~ 30% × Vbus
```

较大输出区：

```text
Uq ≈ 30% ~ 50% × Vbus
```

例如 Vbus = 12V：

```text
5% × 12V = 0.6V

10% × 12V = 1.2V

20% × 12V = 2.4V

30% × 12V = 3.6V

50% × 12V = 6.0V
```

学习阶段建议：

```text
Uq = 0.5V ~ 2.0V
```

不要一上来就给：

```text
Uq = 4V、5V、6V
```

---

# 13. 位置环里的 Uq 公式

如果直接做简单位置闭环，可以用：

```text
位置误差：

err_pos = target_pos - now_pos
```

因为角度是 0°~360° 环形的，所以误差要取最短路径：

```text
如果 err_pos > 180°，err_pos = err_pos - 360°

如果 err_pos < -180°，err_pos = err_pos + 360°
```

位置环输出 Uq：

```text
Uq = Kp_pos × err_pos
```

其中：

```text
Kp_pos 的单位可以理解为 V/deg
```

例如：

```text
Kp_pos = 0.02 V/deg

err_pos = 30°

Uq = 0.02 × 30 = 0.6V
```

如果：

```text
err_pos = 90°
```

则：

```text
Uq = 0.02 × 90 = 1.8V
```

所以 Kp_pos 不能乱设太大。

---

# 14. 位置环 Kp 的大概范围

对于电压模式位置控制：

```text
Uq = Kp_pos × err_pos
```

Kp_pos 推荐从很小开始。

12V 小 BLDC 学习阶段：

```text
Kp_pos = 0.005 ~ 0.02 V/deg     很柔和
Kp_pos = 0.02  ~ 0.05 V/deg     常用初调范围
Kp_pos = 0.05  ~ 0.10 V/deg     力度明显，可能开始抖
Kp_pos > 0.10 V/deg             新手阶段容易震动
```

举例：

```text
Kp_pos = 0.05 V/deg

误差 10°：
Uq = 0.5V

误差 30°：
Uq = 1.5V

误差 90°：
Uq = 4.5V
```

这就是为什么位置误差大时，电机会猛地一抽。

所以位置环一定要有：

```text
1. Uq 限幅
2. 目标角度斜坡
3. 小 Kp 起步
```

---

# 15. 速度环里的 Uq 公式

速度环常见形式：

```text
err_speed = target_speed - now_speed
```

速度环输出：

```text
Uq = Kp_speed × err_speed + Ki_speed × ∫err_speed dt
```

其中：

```text
Kp_speed = 比例系数
Ki_speed = 积分系数
```

速度环的作用：

```text
根据速度误差调节 Uq。
```

如果速度低于目标：

```text
增大 Uq。
```

如果速度高于目标：

```text
减小 Uq。
```

---

# 16. 电流环里的 Ud/Uq 公式

真正标准的 FOC 是电流环控制。

目标：

```text
Id_target = 0

Iq_target = 由速度环或力矩指令给出
```

误差：

```text
err_d = Id_target - Id

err_q = Iq_target - Iq
```

PI 输出：

```text
Ud = Kp_d × err_d + Ki_d × ∫err_d dt

Uq = Kp_q × err_q + Ki_q × ∫err_q dt
```

其中：

```text
Ud 用来控制 Id
Uq 用来控制 Iq
```

也就是：

```text
D 轴电流环输出 Ud
Q 轴电流环输出 Uq
```

电流环才是标准 FOC 的核心。

---

# 17. 位置环、速度环、电流环的关系

标准高性能结构是：

```text
位置环 -> 速度环 -> 电流环 -> SVPWM -> 电机
```

数学关系：

```text
位置环：

err_pos = target_pos - now_pos

target_speed = Kp_pos × err_pos
```

```text
速度环：

err_speed = target_speed - now_speed

target_Iq = Kp_speed × err_speed + Ki_speed × ∫err_speed dt
```

```text
电流环：

err_q = target_Iq - Iq

Uq = Kp_q × err_q + Ki_q × ∫err_q dt
```

```text
D 轴电流环：

err_d = 0 - Id

Ud = Kp_d × err_d + Ki_d × ∫err_d dt
```

最终：

```text
Ud, Uq -> Uα, Uβ -> Ua, Ub, Uc -> SVPWM -> PWM
```

---

# 18. 调参流程

## 18.1 第一阶段：先确认角度和方向

调参前必须确认：

```text
1. 编码器角度连续
2. 角度方向正确
3. 极对数正确
4. 电角度零点正确
5. 三相相序正确
```

否则：

```text
PID 参数调得再好也会抖。
```

---

## 18.2 第二阶段：先固定 Ud

普通 BLDC 学习阶段：

```text
Ud = 0
```

不要先调 Ud。

此时只关注：

```text
Uq
```

---

## 18.3 第三阶段：小 Uq 测试力矩

建议从小 Uq 开始：

```text
Uq = 0.3V
Uq = 0.5V
Uq = 0.8V
Uq = 1.0V
```

观察：

```text
电机是否有力矩
是否发热
是否抖动
方向是否正确
```

如果：

```text
小 Uq 就严重抖动
```

优先怀疑：

```text
1. 电角度错
2. 相序错
3. 编码器方向错
4. 极对数错
```

---

## 18.4 第四阶段：调位置环 Kp

先用最简单关系：

```text
Uq = Kp_pos × err_pos
```

初始范围：

```text
Kp_pos = 0.005 ~ 0.02 V/deg
```

然后慢慢增加：

```text
0.005
0.01
0.02
0.03
0.05
```

观察现象：

```text
Kp 太小：
回正慢，手感软。

Kp 合适：
有回弹，能稳定回目标。

Kp 太大：
目标附近抖动，声音变大，上电猛冲。
```

---

## 18.5 第五阶段：限制 Uq 最大值

位置环必须限制 Uq。

建议：

```text
学习阶段：

Uq_limit = 0.8V ~ 1.5V
```

调顺后：

```text
Uq_limit = 2V ~ 3V
```

更大值需要看：

```text
1. 电机额定电流
2. MOS 管能力
3. 电源能力
4. 是否发热
5. 是否有电流采样保护
```

对于 12V 小 BLDC，新手阶段不建议超过：

```text
Uq_limit = 3V
```

---

## 18.6 第六阶段：加目标斜坡

如果目标角度突然变化很大，比如：

```text
当前角度 = 280°
目标角度 = 10°
```

虽然最短路径只有：

```text
90°
```

但是如果直接给位置环：

```text
Uq = Kp × 90°
```

就会产生很大的启动力矩。

所以目标角度应该慢慢变化：

```text
soft_target 慢慢靠近 target
```

这样可以避免：

```text
1. 上电猛震
2. 大角度跳变
3. 瞬间大 Uq
4. 机械冲击
```

---

## 18.7 第七阶段：再考虑 D 项或速度环

位置环只用 P 时，可能有回弹和超调。

可以有两种改进方向：

```text
方案 1：位置环加 D 项
方案 2：位置环外面套速度环
```

位置 PD：

```text
Uq = Kp × err_pos + Kd × d(err_pos)/dt
```

D 项作用：

```text
增加阻尼
减少过冲
降低来回摆动
```

但是 D 项缺点：

```text
容易放大编码器噪声
启动瞬间容易冲击
参数不好调
```

所以更推荐的结构是：

```text
位置环输出目标速度
速度环输出 Uq
```

---

# 19. Ud/Uq 调整总结

## 19.1 Ud 总结

```text
Ud 控制 D 轴方向的电压。
```

普通 BLDC / 表贴式 PMSM：

```text
Ud = 0
```

一般不要乱调 Ud。

Ud 主要用于：

```text
1. 电流环控制 Id
2. 高速弱磁
3. 特殊电机优化
```

新手阶段：

```text
不要调 Ud。
```

---

## 19.2 Uq 总结

```text
Uq 控制 Q 轴方向的电压。
```

它主要影响：

```text
1. 力矩
2. 加速度
3. 速度
4. 位置回弹力度
```

电压模式下：

```text
Uq 是力矩趋势，不是真实力矩。
```

电流模式下：

```text
Iq 才是真实力矩控制量。
```

学习阶段推荐：

```text
12V 母线：

Uq = 0.3V ~ 1.0V      安全测试
Uq = 1.0V ~ 2.0V      轻载位置控制
Uq = 2.0V ~ 3.0V      力矩明显
Uq > 3.0V             需要谨慎
```

---

# 20. 最重要的一句话

对于普通 BLDC FOC 学习阶段：

```text
Ud 固定为 0。

主要调 Uq。

Uq 从小到大慢慢加。

位置环 Kp 从小到大慢慢加。

必须限制 Uq 最大值。

目标角度不能突然大跳变。
```

最终记住这个关系：

```text
位置误差 -> 目标速度或 Uq

速度误差 -> Uq 或 Iq

Iq -> 力矩

Ud 一般保持 0

Uq 主要产生力矩
```
