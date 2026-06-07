# FOC 完整流程 + $u_d、u_q$ 通俗总结（工程版，少纯推导、重流程+公式）
## 一、FOC 整体四大核心环节（顺序固定）
**采样 → 坐标变换 → 电流环PID → 反变换 + PWM输出**

---
## 二、各环节公式 + 物理意义（$u_d/u_q$ 重点标注）
### 1. 相电流采样（硬件）
采集**三相定子电流**：$i_a、i_b、i_c$
三相电流满足：
$$i_a+i_b+i_c=0$$
工程一般只采两相，第三相计算得出。

---
### 2. Clark 变换（三相静止 → 两相静止 $\alpha\beta$）
把三相电流转成**正交静止坐标系**电流 $i_\alpha、i_\beta$
$$
\begin{cases}
i_\alpha = \displaystyle \frac{2}{3}\big(i_a - \frac{1}{2}i_b - \frac{1}{2}i_c\big) \\[6pt]
i_\beta = \displaystyle \frac{\sqrt{3}}{3}\big(i_b - i_c\big)
\end{cases}
$$

---
### 3. Park 变换（两相静止 $\alpha\beta$ → 两相旋转 $dq$）【**$d/q$ 轴由来**】
依靠**电机电角度 $\theta_e$**，把静止坐标转成**和转子同步旋转的 dq 坐标系**
> $d$ 轴：**直轴（励磁轴）**，对应转子磁场方向
> $q$ 轴：**交轴（转矩轴）**，垂直转子磁场，**决定输出转矩**

#### Park 电流变换（得到 $i_d、i_q$）
$$
\begin{cases}
i_d = i_\alpha\cos\theta_e + i_\beta\sin\theta_e \\
i_q = -i_\alpha\sin\theta_e + i_\beta\cos\theta_e
\end{cases}
$$

---
### 4. 电流环 PID 计算（**生成 $u_d、u_q$ 核心步骤**）
FOC 标准双环：**速度环外环 → 电流环内环**
#### （1）给定值设定
- $i_d^\ast$：直轴电流给定
  永磁同步电机**表贴式SPMSM**：通常设 $\boldsymbol{i_d^\ast = 0}$（最大转矩电流比）
- $i_q^\ast$：交轴电流给定
  由**速度环PID输出**得到，转速偏差越大，$i_q^\ast$ 越大，转矩越大。

#### （2）电流环 PID（输出就是 $\boldsymbol{u_d、u_q}$）
对 $d、q$ 轴电流分别做闭环调节：
$$
\begin{cases}
u_d = \text{PID}\big(i_d^\ast - i_d\big) \\
u_q = \text{PID}\big(i_q^\ast - i_q\big)
\end{cases}
$$

> 物理意义：
> - $\boldsymbol{u_d}$：**直轴电压**，用来控制励磁磁场，SPMSM 里基本维持 0 附近
> - $\boldsymbol{u_q}$：**交轴电压**，**直接决定电机转矩/出力**，电机加减速主要靠调节 $u_q$

> 补充（解耦项，高阶FOC）：
> 考虑反电动势耦合时，会加入前馈解耦项：
> $$
> \begin{cases}
> u_d = \text{PID}(i_d^\ast-i_d) - \omega_e L_q i_q \\
> u_q = \text{PID}(i_q^\ast-i_q) + \omega_e L_d i_d + \omega_e \psi_f
> \end{cases}
> $$
> $\omega_e$：电角速度，$L_d/L_q$：dq轴电感，$\psi_f$：永磁磁链。

---
### 5. 反 Park 变换（$dq$ 旋转 → $\alpha\beta$ 静止）
将电压 $u_d、u_q$ 转回两相静止电压 $u_\alpha、u_\beta$
$$
\begin{cases}
u_\alpha = u_d\cos\theta_e - u_q\sin\theta_e \\
u_\beta = u_d\sin\theta_e + u_q\cos\theta_e
\end{cases}
$$

---
### 6. SVPWM 调制（$\alpha\beta$ → 三相PWM）
由 $u_\alpha、u_\beta$ 计算三相桥臂占空比，输出 PWM 驱动逆变器，完成一整个 FOC 周期。

---
## 三、$u_d$ / $u_q$ 一句话总结
1. **$i_d、i_q$**：变换得到的**反馈电流**，$i_q$ 对应转矩电流；
2. **$u_d、u_q$**：**电流环PID最终输出的控制电压**：
   - $u_d$：控制励磁，表贴永磁机一般趋近于0；
   - $u_q$：控制转矩，电机快慢、出力全由 $u_q$ 主导。

## 四、FOC 极简总流程串讲
采样三相电流 → Clark→$i_\alpha i_\beta$ → Park→$i_d i_q$ → 电流PID算出 $\boldsymbol{u_d、u_q}$ → 反Park→$u_\alpha u_\beta$ → SVPWM→三相PWM输出。