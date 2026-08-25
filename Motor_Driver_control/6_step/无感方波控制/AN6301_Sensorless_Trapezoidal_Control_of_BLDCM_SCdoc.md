# 公式(5)完整推导
先把已知全部列出来：
1. A相电压方程（式1第一行）
$$U_{AG}=Ri_A+L\frac{di_A}{dt}+e_A+U_N$$
变形得到：
$$\boldsymbol{e_A = U_{AG}-Ri_A-L\frac{di_A}{dt}-U_N}$$
同理B相：
$$\boldsymbol{e_B = U_{BG}-Ri_B-L\frac{di_B}{dt}-U_N}$$

2. C相电压方程（式1第三行）：
$$\boldsymbol{U_{CG}=Ri_C+L\frac{di_C}{dt}+e_C+U_N}$$
> C相浮空，$i_C=0$，所以电阻、电感项全部等于0：
$$U_{CG}=0+0+e_C+U_N$$
👉 **核心变形：$\boldsymbol{e_C = U_{CG}-U_N}$**
这就是式(5)最开头：$\boldsymbol{e_C=U_{CG}-U_N}$

---

## 代入式(4)的$U_N$
式(4)：
$$U_N=\frac{U_{AG}+U_{BG}}{2}-\frac{e_A+e_B}{2}$$

把这个$U_N$代入 $e_C = U_{CG}-U_N$：
$$
\begin{aligned}
e_C &= U_{CG} - \left( \frac{U_{AG}+U_{BG}}{2}-\frac{e_A+e_B}{2} \right) \\
&= \boldsymbol{U_{CG} - \frac{U_{AG}+U_{BG}}{2} + \frac{e_A+e_B}{2}}
\end{aligned}
$$
✅ 这就是公式(5)。

---

## 再用式(2)条件：$\boldsymbol{e_A+e_B+e_C=0 \Rightarrow e_A+e_B=-e_C}$
把 $e_A+e_B=-e_C$ 代入(5)：
$$
e_C = U_{CG}-\frac{U_{AG}+U_{BG}}{2}+\frac{-e_C}{2}
$$
把含$e_C$移到左边：
$$
e_C+\frac{e_C}{2}=U_{CG}-\frac{U_{AG}+U_{BG}}{2}
$$
$$
\frac{3}{2}e_C = U_{CG}-\frac{U_{AG}+U_{BG}}{2}
$$
$$
\boldsymbol{e_C=\frac{2}{3}\left(U_{CG}-\frac{U_{AG}+U_{BG}}{2}\right)}
$$

> 👉 这个就是**浮空相反电动势最终表达式**。

### 重点：过零条件 $e_C=0$
令 $e_C=0$：
$$
0=\frac{2}{3}\left(U_{CG}-\frac{U_{AG}+U_{BG}}{2}\right)
$$
得到：
$$\boldsymbol{U_{CG}=\frac{U_{AG}+U_{BG}}{2}}$$

> 也就是文档里写的：
> 1）C相过零点 $e_C=0$：$\boldsymbol{U_{CG} = \dfrac{U_{AG}+U_{BG}}{2}}$
> 2）$e_C>0$：$U_{CG} > \dfrac{U_{AG}+U_{BG}}{2}$
> 3）$e_C<0$：$U_{CG} < \dfrac{U_{AG}+U_{BG}}{2}$

---

# 工程上怎么理解这个式子？
- $U_{AG},U_{BG},U_{CG}$：三个相**对地ADC采样电压**，硬件直接读出来；
- 不需要知道真实中性点$U_N$，也不需要知道$e_A,e_B$；
- 只需要比较：**C相采样电压 VS A+B两相电压的平均值**。
- 当两者相等，就代表浮空C相反电动势过零，这就是**无刷无感方波过零检测的判据**。

### 一个容易踩坑点
这个推导前提：**C相完全浮空，$i_C=0$**；
如果C相有漏电流、有续流，这个公式就会有误差。

如果你需要，我可以给你写一段伪代码，模拟这个过零判断逻辑。


--------------
------------------

# 无感方波 浮空相过零检测伪代码
> 前提：当前A、B相导通，C相浮空（`iC = 0`）
> 采样：`Uag`、`Ubg`、`Ucg` 是ADC读到的三相对地电压；
> 判据：`eC = 0` ⇔ `Ucg == (Uag + Ubg)/2`

```c
// ADC采样得到三相对地电压（原始ADC数值，可直接用，不用换算成实际电压）
float Uag = adc_read(ADC_CH_A);
float Ubg = adc_read(ADC_CH_B);
float Ucg = adc_read(ADC_CH_C);

// 计算AB两相平均电压
float Uab_avg = (Uag + Ubg) / 2.0f;

// 比较C相电压与AB平均值，判断浮空相C的反电动势状态
if( Ucg > Uab_avg + HYSTERESIS )
{
    // eC > 0 ，反电动势为正
    state = EC_POSITIVE;
}
else if( Ucg < Uab_avg - HYSTERESIS )
{
    // eC < 0 ，反电动势为负
    state = EC_NEGATIVE;
}
else
{
    // eC ≈ 0 ，反电动势过零点！
    state = EC_ZERO_CROSS;
    // 过零后需要延时30°电角度，再换相（重要！）
    commutation_delay_flag = 1;
}
```

## 关键工程要点
1. **滞回 `HYSTERESIS`（回差）**
ADC采样有噪声，不能直接相等判断，设置回差，防止抖动反复触发过零。
> 例如：`HYSTERESIS = 50`（ADC 12bit，0‑4095范围）

2. **过零≠立刻换相！**
> 反电动势过零点，**不是换相点**。
> 过零之后要再延时 **30°电角度**，才执行换相。
> 原因：方波驱动，换相时刻在反电动势过零后30°电角度。

### 简单延时实现思路（两种）
#### 方案A：定时器计时（推荐）
检测到过零 `EC_ZERO_CROSS`，打开定时器，计时到30°电角度，触发换相。
电角度时间和转速挂钩：
$$T_{30^\circ} = \frac{1}{6\cdot f_{elec}}$$
$f_{elec}$ 是电频率（电机转速换算得到）

#### 方案B：开环计数（低速简易）
过零之后，固定若干PWM周期后换相，低速可用，高速误差大。

---

# 完整逻辑流程图文字版
1. 电机运行，A、B导通，C浮空
2. ADC循环采样三相对地电压 `Uag,Ubg,Ucg`
3. 计算`Uab_avg=(Uag+Ubg)/2`
4. 比较`Ucg`和`Uab_avg`，带滞回
5. 检测到过零 `eC=0`
6. **等待30°电角度延时**
7. 执行换相，切换导通相（比如切换到B、C导通，A浮空）
8. 下一轮，检测A相的反电动势过零

---

## 坑点（工程调试经常踩）
1. ❌ 直接过零就换相：电机抖动、转速上不去，转矩差
2. ❌ 不加滞回：ADC噪声导致频繁误过零
3. ❌ C相不是真正浮空：有续流二极管导通，`iC≠0`，公式失效，过零检测不准
4. ❌ 低速：反电动势幅值很小，ADC噪声淹没信号，过零检测失效，低速启动困难

如果你需要，我可以把**30°电角度延时怎么从转速算出来**，给你写一段C代码。