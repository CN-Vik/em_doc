# PID超通俗讲解 + 工程实用公式（嵌入式直接用）
## 一、PID 是什么（大白话）
**P比例**：现在偏差多大，立刻使劲调
**I积分**：慢慢把静态误差消掉，稳住不偏移
**D微分**：预判变化趋势，提前刹车防超调

三者合起来：**快、准、稳**

## 二、标准公式
### 1. 连续时域公式
$$u(t) = K_p\left[ e(t) + \frac{1}{T_i}\int_0^t e(\tau)d\tau + T_d \frac{de(t)}{dt} \right]$$
- $e(t)$：偏差 = 目标值 - 实际值
- $K_p$：比例系数
- $T_i$：积分时间
- $T_d$：微分时间

## 三、工程最常用：**离散位置式PID（单片机直接写代码）**
### 离散公式
$$
\begin{cases}
e(k) = set - now \\
u(k) = K_p\left[ e(k) + \dfrac{T}{T_i}\sum_{i=0}^k e(i) + \dfrac{T_d}{T}\big(e(k)-e(k-1)\big) \right]
\end{cases}
$$
- $T$：采样周期（固定）
- $e(k)$ 当前偏差，$e(k-1)$ 上一次偏差

简化参数写法（工程通用）
$$\boldsymbol{u(k) = K_p\cdot e(k) + K_i\cdot \sum e + K_d\cdot (e(k)-e(k-1))}$$
$$K_i=\dfrac{K_p\cdot T}{T_i},\quad K_d=\dfrac{K_p\cdot T_d}{T}$$

## 四、增量式PID（电机、温控最常用！）
只算**输出变化量**，不易积分饱和，工业首选

$$

\Delta u(k) =
K_p\big[e(k)-e(k-1)\big]
+ K_i e(k)
+ K_d\big[e(k)-2e(k-1)+e(k-2)\big]

$$
新输出：$u(k) = u(k-1) + \Delta u(k)$

## 五、三个参数作用（调参口诀）
1. **P增大**：响应变快，容易震荡超调
2. **I增大**：消除静差越快，超调变大
3. **D增大**：抑制震荡，太大容易抖动、抗干扰差

**调参顺序**：先P → 再加I消静差 → 最后D抑超调

## 六、嵌入式C语言最简代码（直接复制用）
```c
// PID结构体
typedef struct
{
    float Kp, Ki, Kd;
    float set;        // 目标值
    float now;        // 当前值
    float err,err1,err2;
    float out;
}PID_TypeDef;

// 增量式PID计算
float PID_Inc_Calc(PID_TypeDef *pid)
{
    pid->err = pid->set - pid->now;
    float delta = pid->Kp*(pid->err - pid->err1) 
                + pid->Ki*pid->err 
                + pid->Kd*(pid->err - 2*pid->err1 + pid->err2);
    
    pid->out += delta;
    // 输出限幅
    if(pid->out > 1000) pid->out = 1000;
    if(pid->out < 0)    pid->out = 0;
    
    pid->err2 = pid->err1;
    pid->err1 = pid->err;
    return pid->out;
}
```

## 七、实际工程应用场景
1. **电机调速/FOC速度环**：增量PID
2. **温控、恒温箱**：位置PID + 积分限幅
3. **无人机姿态、平衡车**：PD为主，弱积分
4. **压力、液位控制**：PI为主，少用D
5. **流量、风速**：纯P/PI即可

需要我给你**PID调参步骤**或者**温控/电机专用整定参数**吗？


---------
---------
------

# 嵌入式实际落地PID公式（纯工程版，无学术化简）
## 核心定义
$e = \boldsymbol{目标值 - 实测值}$
$T$ = 定时采样周期（ms，定时器固定）

---
## 1. 位置式PID（温控、稳压、恒压常用）
### 工程实用公式
$$Out = K_p\cdot e + K_i\cdot \sum e + K_d\cdot (e - e_{last})$$
- $\sum e$：历史偏差累加和
- $e_{last}$：上一次偏差
- 必加：**积分限幅 + 输出限幅**，防炸机

### 拆分写法（代码直接对应）
$$
\begin{align}
P_{out} &= K_p \cdot e \\
I_{out} &= K_i \cdot SumErr \\
D_{out} &= K_d \cdot (e - e_{last}) \\
PID_{out} &= P_{out}+I_{out}+D_{out}
\end{align}
$$

---
## 2. 增量式PID（电机调速、FOC环、舵机、运动控制**最常用**）
只算输出增量，无积分饱和，嵌入式首选
### 最终工程公式
$$
\Delta Out = K_p(e-e_1) + K_i \cdot e + K_d(e-2e_1+e_2)
$$
$$NowOut = LastOut + \Delta Out$$
- $e$：当前偏差
- $e_1$：上一拍偏差
- $e_2$：上上一拍偏差

---
## 3. 嵌入式常用精简变种（项目高频）
### ① PI控制（绝大多数恒温、电压、流量）
$$Out = K_p\cdot e + K_i\cdot \sum e$$
**去掉微分D**，抗干扰强，工业硬件最爱

### ② PD控制（平衡车、姿态角、倾角控制）
$$Out = K_p\cdot e + K_d\cdot (e-e_{last})$$
**去掉积分I**，响应快、无滞后、不累积误差

---
## 4. 系数换算（把书本参数转嵌入式参数）
书本：$K_p、T_i、T_d$
嵌入式实际用：$Kp、Ki、Kd$
$$
\begin{cases}
K_i = \dfrac{K_p \cdot T}{T_i}\\[6pt]
K_d = \dfrac{K_p \cdot T_d}{T}
\end{cases}
$$
$T$ = 采样周期（单位和$T_i、T_d$统一）

---
## 5. 嵌入式代码一对一公式映射
```c
// 增量式 完全对应上面公式
float pid_calc(PID *p)
{
    p->e = p->tar - p->val;

    // 直接套工程公式
    float delta = p->kp*(p->e - p->e1) 
                + p->ki * p->e
                + p->kd*(p->e - 2*p->e1 + p->e2);

    p->out += delta;

    // 实际工程必加限幅
    if(p->out > p->max) p->out = p->max;
    if(p->out < p->min) p->out = p->min;

    // 更新偏差缓存
    p->e2 = p->e1;
    p->e1 = p->e;
    return p->out;
}
```

---
## 6. 工程实战取值规律
1. 电机速度环：**增量PID**，D偏大抑制抖动
2. 电池温控：**位置PI**，关闭D防温度跳变误调
3. 位置闭环（云台、滑台）：**完整PID**
4. 电流环：**纯PI**

需要我给你**STM32/ESP32直接能用的PID完整结构体+定时调用模板**吗？


--------
--------
------

PID 本质上就是一个**闭环控制算法**。

它干的事情很简单：

> 我想要一个目标值，比如速度 1000rpm；
> 现在实际只有 800rpm；
> PID 根据“差多少、差了多久、变化快不快”来算出一个控制输出，比如 PWM 占空比。

---

# 1. PID 里面的几个量

假设：

```c
target   // 目标值，比如目标速度、目标角度、目标温度
measure  // 实际测量值，比如编码器速度、IMU角度、温度传感器值
output   // 控制输出，比如PWM占空比、电机电压、阀门开度
```

误差：

```c
error = target - measure;
```

也就是：

```c
误差 = 目标值 - 实际值
```

例如：

```c
目标速度 = 1000 rpm
实际速度 = 800 rpm
error = 1000 - 800 = 200
```

说明速度低了，需要加大 PWM。

---

# 2. PID 三个部分分别干啥？

PID = P + I + D

---

## P：比例控制

P 的公式：

```c
P = Kp * error;
```

意思是：

> 当前误差越大，控制力度越大。

例如速度差 200rpm，就多给点 PWM；差 20rpm，就少给点。

P 的特点：

| Kp 太小   | Kp 太大      |
| ------- | ---------- |
| 反应慢，没力气 | 容易震荡、抖动、超调 |

比如平衡车：

```c
角度偏差越大，电机越用力往回拉
```

---

## I：积分控制

I 的公式：

```c
integral += error * dt;
I = Ki * integral;
```

意思是：

> 误差如果长期存在，就不断累加，慢慢加大控制输出。

它主要解决**静态误差**。

比如电机速度控制：

目标 1000rpm，实际一直稳定在 950rpm。

P 控制可能觉得误差 50 不大，输出不够继续增加，于是永远差一点。

积分 I 会发现：

```c
你一直差 50，一直差，一直差……
```

于是它会慢慢把输出补上去，最后让速度接近 1000rpm。

I 的特点：

| Ki 太小 | Ki 太大        |
| ----- | ------------ |
| 消除误差慢 | 容易积分饱和、震荡、超调 |

---

## D：微分控制

D 的公式：

```c
D = Kd * (error - last_error) / dt;
```

意思是：

> 看误差变化速度，用来提前刹车、抑制震荡。

比如平衡车快要倒下时，角度变化速度很快，D 项可以提前反应。

D 的特点：

| Kd 太小   | Kd 太大      |
| ------- | ---------- |
| 抑制震荡不明显 | 对噪声敏感，输出抖动 |

在工程里，D 项很容易被传感器噪声影响，所以经常要滤波。

---

# 3. 连续 PID 数学公式

u(t)=K_p e(t)+K_i\int_0^t e(\tau)d\tau+K_d\frac{de(t)}{dt}

其中：

| 符号     | 含义   |
| ------ | ---- |
| `u(t)` | 控制输出 |
| `e(t)` | 当前误差 |
| `Kp`   | 比例系数 |
| `Ki`   | 积分系数 |
| `Kd`   | 微分系数 |

但是单片机里不能直接用这个连续公式，因为 MCU 是周期性运行的，比如 1ms 算一次、5ms 算一次、10ms 算一次。

所以工程里一般用**离散 PID**。

---

# 4. 实际工程常用：位置式 PID

这是最常见的写法。

## 公式

```c
error = target - measure;

P = Kp * error;

integral += error * dt;
I = Ki * integral;

D = Kd * (error - last_error) / dt;

output = P + I + D;

last_error = error;
```

完整公式可以理解成：

u_k=K_p e_k+K_i T_s\sum_{i=0}^{k} e_i+K_d\frac{e_k-e_{k-1}}{T_s}

其中：

| 符号          | 含义        |
| ----------- | --------- |
| `k`         | 当前第 k 次计算 |
| `Ts` / `dt` | 控制周期      |
| `e_k`       | 当前误差      |
| `e_{k-1}`   | 上一次误差     |

---

# 5. 工程代码模板：位置式 PID

```c
typedef struct
{
    float kp;
    float ki;
    float kd;

    float target;       // 目标值
    float measure;      // 测量值

    float error;        // 当前误差
    float last_error;   // 上一次误差

    float integral;     // 积分累加值

    float output;       // PID输出

    float output_max;   // 输出限幅
    float output_min;

    float integral_max; // 积分限幅
    float integral_min;

    float dt;           // 控制周期，单位：秒
} PID_t;


static float limit_float(float value, float min, float max)
{
    if (value > max)
        return max;
    if (value < min)
        return min;
    return value;
}


float PID_Calc(PID_t *pid, float target, float measure)
{
    float p;
    float i;
    float d;

    pid->target = target;
    pid->measure = measure;

    // 1. 计算误差
    pid->error = pid->target - pid->measure;

    // 2. P项：当前误差越大，输出越大
    p = pid->kp * pid->error;

    // 3. I项：误差累加，用来消除长期静态误差
    pid->integral += pid->error * pid->dt;

    // 4. 积分限幅，防止积分饱和
    pid->integral = limit_float(pid->integral,
                                pid->integral_min,
                                pid->integral_max);

    i = pid->ki * pid->integral;

    // 5. D项：误差变化速度，用来抑制超调和震荡
    d = pid->kd * (pid->error - pid->last_error) / pid->dt;

    // 6. PID总输出
    pid->output = p + i + d;

    // 7. 输出限幅，比如PWM只能是 0~1000
    pid->output = limit_float(pid->output,
                              pid->output_min,
                              pid->output_max);

    // 8. 保存当前误差，供下次计算D项
    pid->last_error = pid->error;

    return pid->output;
}
```

---

# 6. 实际用在 PWM 控制里

假设你控制电机速度。

```c
float target_speed = 1000.0f;   // 目标速度 1000rpm
float real_speed   = get_motor_speed();

float pwm = PID_Calc(&speed_pid, target_speed, real_speed);

motor_set_pwm((int)pwm);
```

PID 输出可以直接对应：

| 控制对象         | PID 输出          |
| ------------ | --------------- |
| 直流电机速度       | PWM 占空比         |
| BLDC FOC 速度环 | 目标电流 Iq         |
| 温度控制         | 加热功率            |
| 舵机位置控制       | PWM 或目标角速度      |
| 平衡车角度控制      | 电机力矩 / 电流 / PWM |
| 水泵压力控制       | 电机转速            |
| 灯光亮度控制       | PWM 占空比         |

---

# 7. 更工程化的写法：D 项不要对 error 微分

很多实际工程里，D 项不推荐这么写：

```c
D = Kd * (error - last_error) / dt;
```

因为目标值 target 突然变化时，error 会突然跳变，D 项也会突然很大，输出会猛冲。

更常用的是对测量值微分：

```c
D = -Kd * (measure - last_measure) / dt;
```

为什么前面有负号？

因为：

```c
error = target - measure
```

如果 target 不变，那么：

```c
error变化 = -measure变化
```

所以工程写法：

```c
d = -pid->kd * (measure - pid->last_measure) / pid->dt;
```

这种叫：

```c
微分先行
```

或者：

```c
Derivative on Measurement
```

它的优点是目标值突变时，不容易让 D 项产生尖峰。

---

# 8. 带测量微分的工程 PID

```c
typedef struct
{
    float kp;
    float ki;
    float kd;

    float error;
    float integral;

    float last_measure;

    float output;

    float output_max;
    float output_min;

    float integral_max;
    float integral_min;

    float dt;
} PID_Real_t;


float PID_RealCalc(PID_Real_t *pid, float target, float measure)
{
    float p;
    float i;
    float d;

    // 当前误差
    pid->error = target - measure;

    // P项
    p = pid->kp * pid->error;

    // I项
    pid->integral += pid->error * pid->dt;

    pid->integral = limit_float(pid->integral,
                                pid->integral_min,
                                pid->integral_max);

    i = pid->ki * pid->integral;

    // D项：对测量值求微分，而不是对误差求微分
    d = -pid->kd * (measure - pid->last_measure) / pid->dt;

    // 输出
    pid->output = p + i + d;

    // 输出限幅
    pid->output = limit_float(pid->output,
                              pid->output_min,
                              pid->output_max);

    // 保存上一次测量值
    pid->last_measure = measure;

    return pid->output;
}
```

---

# 9. 增量式 PID

位置式 PID 算的是：

```c
当前应该输出多少
```

增量式 PID 算的是：

```c
这一次输出应该比上一次增加多少
```

公式：

\Delta u_k=K_p(e_k-e_{k-1})+K_iT_s e_k+\frac{K_d}{T_s}(e_k-2e_{k-1}+e_{k-2})

然后：

```c
output += delta_output;
```

工程代码：

```c
typedef struct
{
    float kp;
    float ki;
    float kd;

    float error;
    float last_error;
    float prev_error;

    float output;

    float output_max;
    float output_min;

    float dt;
} PID_Inc_t;


float PID_IncCalc(PID_Inc_t *pid, float target, float measure)
{
    float delta_output;

    pid->error = target - measure;

    delta_output =
        pid->kp * (pid->error - pid->last_error)
        + pid->ki * pid->dt * pid->error
        + pid->kd * (pid->error - 2.0f * pid->last_error + pid->prev_error) / pid->dt;

    pid->output += delta_output;

    pid->output = limit_float(pid->output,
                              pid->output_min,
                              pid->output_max);

    pid->prev_error = pid->last_error;
    pid->last_error = pid->error;

    return pid->output;
}
```

---

# 10. 位置式 PID 和增量式 PID 怎么选？

| 类型      | 特点      | 适合场景            |
| ------- | ------- | --------------- |
| 位置式 PID | 直接算最终输出 | 温度、位置、速度、电机控制常用 |
| 增量式 PID | 算输出变化量  | 电机速度控制、执行器连续调节  |

你作为嵌入式初学者，建议先掌握**位置式 PID**。

---

# 11. 工程里最重要的几个细节

## 1. PID 必须固定周期调用

比如：

```c
1ms 调一次
5ms 调一次
10ms 调一次
```

不能一会儿 2ms，一会儿 30ms，否则 D 项和 I 项都会乱。

例如：

```c
pid.dt = 0.01f; // 10ms
```

如果 10ms 调一次：

```c
dt = 0.01 秒
```

---

## 2. 输出必须限幅

例如 PWM 范围是 0~1000：

```c
pid.output_min = 0;
pid.output_max = 1000;
```

否则 PID 可能算出：

```c
output = 5000
```

但你的 PWM 根本输出不了 5000。

---

## 3. 积分必须限幅

如果积分不限幅，容易出现“积分饱和”。

比如电机堵转了，速度一直上不去：

```c
target = 1000
measure = 0
error = 1000
```

积分一直累加：

```c
integral += 1000;
integral += 1000;
integral += 1000;
...
```

最后积分巨大。等电机突然恢复后，输出会猛冲，系统会严重超调。

所以要加：

```c
integral = limit_float(integral, integral_min, integral_max);
```

---

## 4. D 项最好加滤波

传感器噪声会导致 D 项抖动。

简单低通滤波：

```c
d_filter = 0.8f * d_filter + 0.2f * d_raw;
```

然后用：

```c
D = d_filter;
```

---

# 12. PID 调参经验

最简单的工程调参顺序：

## 第一步：先只开 P

```c
Ki = 0;
Kd = 0;
```

慢慢加 Kp。

现象：

| 现象   | 说明    |
| ---- | ----- |
| 反应慢  | Kp 太小 |
| 开始震荡 | Kp 偏大 |
| 剧烈震荡 | Kp 太大 |

---

## 第二步：加 D 抑制震荡

如果系统有明显超调和震荡，加 Kd。

| 现象    | 处理           |
| ----- | ------------ |
| 超调大   | 增大 Kd        |
| 来回抖   | 适当增大 Kd      |
| 输出毛刺多 | Kd 太大或传感器噪声大 |

---

## 第三步：加 I 消除静态误差

如果系统稳定后还差一点，比如目标 1000rpm，实际 950rpm，就加 Ki。

| 现象      | 处理         |
| ------- | ---------- |
| 长期有误差   | 增大 Ki      |
| 慢慢来回晃   | Ki 偏大      |
| 超调越来越严重 | Ki 太大或积分饱和 |

---

# 13. 平衡车里的 PID 怎么用？

平衡车一般不是一个 PID，而是多个环。

常见结构：

```text
角度环 PID/PD
    ↓
速度环 PI
    ↓
电机控制 PWM / 电流环 / FOC
```

更具体一点：

```text
IMU 测角度
    ↓
角度 PID 算出需要的电机力矩
    ↓
编码器测速度
    ↓
速度 PID 修正目标角度或目标力矩
    ↓
输出 PWM / FOC Iq
```

平衡车角度环常见用法：

```c
angle_error = target_angle - real_angle;

output = Kp * angle_error
       + Kd * gyro_y;
```

很多平衡车角度环其实更像 PD：

```c
output = Kp * 角度误差 + Kd * 角速度;
```

其中：

```c
角度误差：车身倒了多少
角速度：车身正在倒得有多快
```

P 负责把车拉回来，D 负责提前刹车防止冲过头。

---

# 14. 一个平衡车角度环例子

```c
float Balance_PID(float target_angle, float real_angle, float gyro_y)
{
    float angle_error;
    float output;

    angle_error = target_angle - real_angle;

    output = balance_kp * angle_error
           + balance_kd * gyro_y;

    return output;
}
```

注意：

`gyro_y` 的正负号要根据你的 IMU 安装方向、电机方向实际调整。

有时候是：

```c
output = Kp * angle_error + Kd * gyro_y;
```

有时候是：

```c
output = Kp * angle_error - Kd * gyro_y;
```

这要看你的坐标系。

---

# 15. 电机速度环例子

```c
float Speed_PID(float target_speed, float real_speed)
{
    static float integral = 0;
    static float last_error = 0;

    float error;
    float output;

    error = target_speed - real_speed;

    integral += error * 0.01f;  // 10ms周期

    integral = limit_float(integral, -500, 500);

    output = speed_kp * error
           + speed_ki * integral
           + speed_kd * (error - last_error) / 0.01f;

    output = limit_float(output, -1000, 1000);

    last_error = error;

    return output;
}
```

速度环很多时候用 PI 就够了：

```c
output = Kp * error + Ki * integral;
```

因为速度信号本身可能有噪声，D 项不一定好用。

---

# 16. 实际工程推荐版本

对于你这种嵌入式开发，建议先这么用：

| 控制对象     | 推荐       |
| -------- | -------- |
| 温度控制     | PI       |
| 普通电机速度控制 | PI 或 PID |
| 舵机/位置控制  | PID      |
| 平衡车角度环   | PD       |
| 平衡车速度环   | PI       |
| FOC 电流环  | PI       |
| FOC 速度环  | PI       |
| FOC 位置环  | P / PID  |

---

# 17. 最重要的一句话

PID 不是玄学，它就是根据这三个东西控制输出：

```text
P：现在差多少
I：过去一直差多少
D：误差变化有多快
```

实际工程公式核心就是：

```c
error = target - measure;

P = Kp * error;
I += Ki * error * dt;
D = Kd * (error - last_error) / dt;

output = P + I + D;
```

但是工程上一定要加：

```c
固定周期
输出限幅
积分限幅
D项滤波
方向检查
```

不然 PID 很容易出现震荡、冲过头、输出乱跳、积分爆炸等问题。

---------
----------
-------
PID 本质上就是一个**闭环控制算法**。

它干的事情很简单：

> 我想要一个目标值，比如速度 1000rpm；
> 现在实际只有 800rpm；
> PID 根据“差多少、差了多久、变化快不快”来算出一个控制输出，比如 PWM 占空比。

---

# 1. PID 里面的几个量

假设：

```c
target   // 目标值，比如目标速度、目标角度、目标温度
measure  // 实际测量值，比如编码器速度、IMU角度、温度传感器值
output   // 控制输出，比如PWM占空比、电机电压、阀门开度
```

误差：

```c
error = target - measure;
```

也就是：

```c
误差 = 目标值 - 实际值
```

例如：

```c
目标速度 = 1000 rpm
实际速度 = 800 rpm
error = 1000 - 800 = 200
```

说明速度低了，需要加大 PWM。

---

# 2. PID 三个部分分别干啥？

PID = P + I + D

---

## P：比例控制

P 的公式：

```c
P = Kp * error;
```

意思是：

> 当前误差越大，控制力度越大。

例如速度差 200rpm，就多给点 PWM；差 20rpm，就少给点。

P 的特点：

| Kp 太小   | Kp 太大      |
| ------- | ---------- |
| 反应慢，没力气 | 容易震荡、抖动、超调 |

比如平衡车：

```c
角度偏差越大，电机越用力往回拉
```

---

## I：积分控制

I 的公式：

```c
integral += error * dt;
I = Ki * integral;
```

意思是：

> 误差如果长期存在，就不断累加，慢慢加大控制输出。

它主要解决**静态误差**。

比如电机速度控制：

目标 1000rpm，实际一直稳定在 950rpm。

P 控制可能觉得误差 50 不大，输出不够继续增加，于是永远差一点。

积分 I 会发现：

```c
你一直差 50，一直差，一直差……
```

于是它会慢慢把输出补上去，最后让速度接近 1000rpm。

I 的特点：

| Ki 太小 | Ki 太大        |
| ----- | ------------ |
| 消除误差慢 | 容易积分饱和、震荡、超调 |

---

## D：微分控制

D 的公式：

```c
D = Kd * (error - last_error) / dt;
```

意思是：

> 看误差变化速度，用来提前刹车、抑制震荡。

比如平衡车快要倒下时，角度变化速度很快，D 项可以提前反应。

D 的特点：

| Kd 太小   | Kd 太大      |
| ------- | ---------- |
| 抑制震荡不明显 | 对噪声敏感，输出抖动 |

在工程里，D 项很容易被传感器噪声影响，所以经常要滤波。

---

# 3. 连续 PID 数学公式

u(t)=K_p e(t)+K_i\int_0^t e(\tau)d\tau+K_d\frac{de(t)}{dt}

其中：

| 符号     | 含义   |
| ------ | ---- |
| `u(t)` | 控制输出 |
| `e(t)` | 当前误差 |
| `Kp`   | 比例系数 |
| `Ki`   | 积分系数 |
| `Kd`   | 微分系数 |

但是单片机里不能直接用这个连续公式，因为 MCU 是周期性运行的，比如 1ms 算一次、5ms 算一次、10ms 算一次。

所以工程里一般用**离散 PID**。

---

# 4. 实际工程常用：位置式 PID

这是最常见的写法。

## 公式

```c
error = target - measure;

P = Kp * error;

integral += error * dt;
I = Ki * integral;

D = Kd * (error - last_error) / dt;

output = P + I + D;

last_error = error;
```

完整公式可以理解成：

u_k=K_p e_k+K_i T_s\sum_{i=0}^{k} e_i+K_d\frac{e_k-e_{k-1}}{T_s}

其中：

| 符号          | 含义        |
| ----------- | --------- |
| `k`         | 当前第 k 次计算 |
| `Ts` / `dt` | 控制周期      |
| `e_k`       | 当前误差      |
| `e_{k-1}`   | 上一次误差     |

---

# 5. 工程代码模板：位置式 PID

```c
typedef struct
{
    float kp;
    float ki;
    float kd;

    float target;       // 目标值
    float measure;      // 测量值

    float error;        // 当前误差
    float last_error;   // 上一次误差

    float integral;     // 积分累加值

    float output;       // PID输出

    float output_max;   // 输出限幅
    float output_min;

    float integral_max; // 积分限幅
    float integral_min;

    float dt;           // 控制周期，单位：秒
} PID_t;


static float limit_float(float value, float min, float max)
{
    if (value > max)
        return max;
    if (value < min)
        return min;
    return value;
}


float PID_Calc(PID_t *pid, float target, float measure)
{
    float p;
    float i;
    float d;

    pid->target = target;
    pid->measure = measure;

    // 1. 计算误差
    pid->error = pid->target - pid->measure;

    // 2. P项：当前误差越大，输出越大
    p = pid->kp * pid->error;

    // 3. I项：误差累加，用来消除长期静态误差
    pid->integral += pid->error * pid->dt;

    // 4. 积分限幅，防止积分饱和
    pid->integral = limit_float(pid->integral,
                                pid->integral_min,
                                pid->integral_max);

    i = pid->ki * pid->integral;

    // 5. D项：误差变化速度，用来抑制超调和震荡
    d = pid->kd * (pid->error - pid->last_error) / pid->dt;

    // 6. PID总输出
    pid->output = p + i + d;

    // 7. 输出限幅，比如PWM只能是 0~1000
    pid->output = limit_float(pid->output,
                              pid->output_min,
                              pid->output_max);

    // 8. 保存当前误差，供下次计算D项
    pid->last_error = pid->error;

    return pid->output;
}
```

---

# 6. 实际用在 PWM 控制里

假设你控制电机速度。

```c
float target_speed = 1000.0f;   // 目标速度 1000rpm
float real_speed   = get_motor_speed();

float pwm = PID_Calc(&speed_pid, target_speed, real_speed);

motor_set_pwm((int)pwm);
```

PID 输出可以直接对应：

| 控制对象         | PID 输出          |
| ------------ | --------------- |
| 直流电机速度       | PWM 占空比         |
| BLDC FOC 速度环 | 目标电流 Iq         |
| 温度控制         | 加热功率            |
| 舵机位置控制       | PWM 或目标角速度      |
| 平衡车角度控制      | 电机力矩 / 电流 / PWM |
| 水泵压力控制       | 电机转速            |
| 灯光亮度控制       | PWM 占空比         |

---

# 7. 更工程化的写法：D 项不要对 error 微分

很多实际工程里，D 项不推荐这么写：

```c
D = Kd * (error - last_error) / dt;
```

因为目标值 target 突然变化时，error 会突然跳变，D 项也会突然很大，输出会猛冲。

更常用的是对测量值微分：

```c
D = -Kd * (measure - last_measure) / dt;
```

为什么前面有负号？

因为：

```c
error = target - measure
```

如果 target 不变，那么：

```c
error变化 = -measure变化
```

所以工程写法：

```c
d = -pid->kd * (measure - pid->last_measure) / pid->dt;
```

这种叫：

```c
微分先行
```

或者：

```c
Derivative on Measurement
```

它的优点是目标值突变时，不容易让 D 项产生尖峰。

---

# 8. 带测量微分的工程 PID

```c
typedef struct
{
    float kp;
    float ki;
    float kd;

    float error;
    float integral;

    float last_measure;

    float output;

    float output_max;
    float output_min;

    float integral_max;
    float integral_min;

    float dt;
} PID_Real_t;


float PID_RealCalc(PID_Real_t *pid, float target, float measure)
{
    float p;
    float i;
    float d;

    // 当前误差
    pid->error = target - measure;

    // P项
    p = pid->kp * pid->error;

    // I项
    pid->integral += pid->error * pid->dt;

    pid->integral = limit_float(pid->integral,
                                pid->integral_min,
                                pid->integral_max);

    i = pid->ki * pid->integral;

    // D项：对测量值求微分，而不是对误差求微分
    d = -pid->kd * (measure - pid->last_measure) / pid->dt;

    // 输出
    pid->output = p + i + d;

    // 输出限幅
    pid->output = limit_float(pid->output,
                              pid->output_min,
                              pid->output_max);

    // 保存上一次测量值
    pid->last_measure = measure;

    return pid->output;
}
```

---

# 9. 增量式 PID

位置式 PID 算的是：

```c
当前应该输出多少
```

增量式 PID 算的是：

```c
这一次输出应该比上一次增加多少
```

公式：

\Delta u_k=K_p(e_k-e_{k-1})+K_iT_s e_k+\frac{K_d}{T_s}(e_k-2e_{k-1}+e_{k-2})

然后：

```c
output += delta_output;
```

工程代码：

```c
typedef struct
{
    float kp;
    float ki;
    float kd;

    float error;
    float last_error;
    float prev_error;

    float output;

    float output_max;
    float output_min;

    float dt;
} PID_Inc_t;


float PID_IncCalc(PID_Inc_t *pid, float target, float measure)
{
    float delta_output;

    pid->error = target - measure;

    delta_output =
        pid->kp * (pid->error - pid->last_error)
        + pid->ki * pid->dt * pid->error
        + pid->kd * (pid->error - 2.0f * pid->last_error + pid->prev_error) / pid->dt;

    pid->output += delta_output;

    pid->output = limit_float(pid->output,
                              pid->output_min,
                              pid->output_max);

    pid->prev_error = pid->last_error;
    pid->last_error = pid->error;

    return pid->output;
}
```

---

# 10. 位置式 PID 和增量式 PID 怎么选？

| 类型      | 特点      | 适合场景            |
| ------- | ------- | --------------- |
| 位置式 PID | 直接算最终输出 | 温度、位置、速度、电机控制常用 |
| 增量式 PID | 算输出变化量  | 电机速度控制、执行器连续调节  |

你作为嵌入式初学者，建议先掌握**位置式 PID**。

---

# 11. 工程里最重要的几个细节

## 1. PID 必须固定周期调用

比如：

```c
1ms 调一次
5ms 调一次
10ms 调一次
```

不能一会儿 2ms，一会儿 30ms，否则 D 项和 I 项都会乱。

例如：

```c
pid.dt = 0.01f; // 10ms
```

如果 10ms 调一次：

```c
dt = 0.01 秒
```

---

## 2. 输出必须限幅

例如 PWM 范围是 0~1000：

```c
pid.output_min = 0;
pid.output_max = 1000;
```

否则 PID 可能算出：

```c
output = 5000
```

但你的 PWM 根本输出不了 5000。

---

## 3. 积分必须限幅

如果积分不限幅，容易出现“积分饱和”。

比如电机堵转了，速度一直上不去：

```c
target = 1000
measure = 0
error = 1000
```

积分一直累加：

```c
integral += 1000;
integral += 1000;
integral += 1000;
...
```

最后积分巨大。等电机突然恢复后，输出会猛冲，系统会严重超调。

所以要加：

```c
integral = limit_float(integral, integral_min, integral_max);
```

---

## 4. D 项最好加滤波

传感器噪声会导致 D 项抖动。

简单低通滤波：

```c
d_filter = 0.8f * d_filter + 0.2f * d_raw;
```

然后用：

```c
D = d_filter;
```

---

# 12. PID 调参经验

最简单的工程调参顺序：

## 第一步：先只开 P

```c
Ki = 0;
Kd = 0;
```

慢慢加 Kp。

现象：

| 现象   | 说明    |
| ---- | ----- |
| 反应慢  | Kp 太小 |
| 开始震荡 | Kp 偏大 |
| 剧烈震荡 | Kp 太大 |

---

## 第二步：加 D 抑制震荡

如果系统有明显超调和震荡，加 Kd。

| 现象    | 处理           |
| ----- | ------------ |
| 超调大   | 增大 Kd        |
| 来回抖   | 适当增大 Kd      |
| 输出毛刺多 | Kd 太大或传感器噪声大 |

---

## 第三步：加 I 消除静态误差

如果系统稳定后还差一点，比如目标 1000rpm，实际 950rpm，就加 Ki。

| 现象      | 处理         |
| ------- | ---------- |
| 长期有误差   | 增大 Ki      |
| 慢慢来回晃   | Ki 偏大      |
| 超调越来越严重 | Ki 太大或积分饱和 |

---

# 13. 平衡车里的 PID 怎么用？

平衡车一般不是一个 PID，而是多个环。

常见结构：

```text
角度环 PID/PD
    ↓
速度环 PI
    ↓
电机控制 PWM / 电流环 / FOC
```

更具体一点：

```text
IMU 测角度
    ↓
角度 PID 算出需要的电机力矩
    ↓
编码器测速度
    ↓
速度 PID 修正目标角度或目标力矩
    ↓
输出 PWM / FOC Iq
```

平衡车角度环常见用法：

```c
angle_error = target_angle - real_angle;

output = Kp * angle_error
       + Kd * gyro_y;
```

很多平衡车角度环其实更像 PD：

```c
output = Kp * 角度误差 + Kd * 角速度;
```

其中：

```c
角度误差：车身倒了多少
角速度：车身正在倒得有多快
```

P 负责把车拉回来，D 负责提前刹车防止冲过头。

---

# 14. 一个平衡车角度环例子

```c
float Balance_PID(float target_angle, float real_angle, float gyro_y)
{
    float angle_error;
    float output;

    angle_error = target_angle - real_angle;

    output = balance_kp * angle_error
           + balance_kd * gyro_y;

    return output;
}
```

注意：

`gyro_y` 的正负号要根据你的 IMU 安装方向、电机方向实际调整。

有时候是：

```c
output = Kp * angle_error + Kd * gyro_y;
```

有时候是：

```c
output = Kp * angle_error - Kd * gyro_y;
```

这要看你的坐标系。

---

# 15. 电机速度环例子

```c
float Speed_PID(float target_speed, float real_speed)
{
    static float integral = 0;
    static float last_error = 0;

    float error;
    float output;

    error = target_speed - real_speed;

    integral += error * 0.01f;  // 10ms周期

    integral = limit_float(integral, -500, 500);

    output = speed_kp * error
           + speed_ki * integral
           + speed_kd * (error - last_error) / 0.01f;

    output = limit_float(output, -1000, 1000);

    last_error = error;

    return output;
}
```

速度环很多时候用 PI 就够了：

```c
output = Kp * error + Ki * integral;
```

因为速度信号本身可能有噪声，D 项不一定好用。

---

# 16. 实际工程推荐版本

对于你这种嵌入式开发，建议先这么用：

| 控制对象     | 推荐       |
| -------- | -------- |
| 温度控制     | PI       |
| 普通电机速度控制 | PI 或 PID |
| 舵机/位置控制  | PID      |
| 平衡车角度环   | PD       |
| 平衡车速度环   | PI       |
| FOC 电流环  | PI       |
| FOC 速度环  | PI       |
| FOC 位置环  | P / PID  |

---

# 17. 最重要的一句话

PID 不是玄学，它就是根据这三个东西控制输出：

```text
P：现在差多少
I：过去一直差多少
D：误差变化有多快
```

实际工程公式核心就是：

```c
error = target - measure;

P = Kp * error;
I += Ki * error * dt;
D = Kd * (error - last_error) / dt;

output = P + I + D;
```

但是工程上一定要加：

```c
固定周期
输出限幅
积分限幅
D项滤波
方向检查
```

不然 PID 很容易出现震荡、冲过头、输出乱跳、积分爆炸等问题。
