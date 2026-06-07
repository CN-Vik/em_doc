# FOC 公式与调参流程笔记

## 1. FOC 是什么

FOC 全称是 Field Oriented Control，中文一般叫磁场定向控制。

它的核心思想是：

> 把三相电机的复杂交流量，转换到跟着转子磁场一起旋转的 DQ 坐标系里，然后像控制直流电机一样控制它。

对于永磁同步电机 / BLDC 电机来说：

* `d 轴`：对准转子永磁体磁场方向
* `q 轴`：和转子磁场垂直
* `Id`：d 轴电流，通常希望为 0
* `Iq`：q 轴电流，主要产生转矩

简单理解：

```text
Id 控磁场
Iq 控力矩
```

对于普通表贴式 BLDC/PMSM：

```text
Id = 0
Iq 越大，电机力矩越大
```

---

## 2. 角度相关公式

### 2.1 机械角度

机械角度就是编码器直接测出来的转子物理角度。

单位可以是角度制：

```text
theta_m_deg = 0° ~ 360°
```

也可以是弧度制：

```text
theta_m_rad = 0 ~ 2π
```

角度转弧度：

```text
theta_rad = theta_deg * π / 180
```

弧度转角度：

```text
theta_deg = theta_rad * 180 / π
```

C 语言写法：

```c
#define FOC_PI    3.14159265358979323846f
#define FOC_2PI   6.28318530717958647692f

float deg_to_rad(float deg)
{
    return deg * FOC_PI / 180.0f;
}

float rad_to_deg(float rad)
{
    return rad * 180.0f / FOC_PI;
}
```

---

### 2.2 电角度

FOC 计算真正需要的是电角度，不是机械角度。

公式：

```text
theta_e = pole_pairs * theta_m + theta_offset
```

其中：

```text
theta_e      电角度
theta_m      机械角度
pole_pairs   电机极对数
theta_offset 电角度零点偏移
```

如果用角度制：

```text
theta_e_deg = pole_pairs * theta_m_deg + theta_offset_deg
```

如果用弧度制：

```text
theta_e_rad = pole_pairs * theta_m_rad + theta_offset_rad
```

然后要归一化到一个周期内：

```text
theta_e_rad = theta_e_rad mod 2π
```

C 语言写法：

```c
float normalize_rad(float angle)
{
    while (angle >= FOC_2PI)
    {
        angle -= FOC_2PI;
    }

    while (angle < 0.0f)
    {
        angle += FOC_2PI;
    }

    return angle;
}

float get_electrical_angle(float theta_m_rad,
                           float pole_pairs,
                           float theta_offset_rad)
{
    float theta_e = pole_pairs * theta_m_rad + theta_offset_rad;

    return normalize_rad(theta_e);
}
```

---

## 3. Clarke 变换

Clarke 变换的作用是：

```text
三相静止坐标系 abc -> 两相静止坐标系 alpha/beta
```

如果有三相电流：

```text
Ia, Ib, Ic
```

并且满足：

```text
Ia + Ib + Ic = 0
```

常用公式：

```text
I_alpha = Ia

I_beta = (Ia + 2 * Ib) / sqrt(3)
```

C 语言写法：

```c
typedef struct
{
    float alpha;
    float beta;
} clarke_t;

clarke_t clarke_transform(float ia, float ib)
{
    clarke_t out;

    out.alpha = ia;
    out.beta = (ia + 2.0f * ib) / 1.73205080757f;

    return out;
}
```

说明：

```text
如果只有两个电流采样，通常采 Ia 和 Ib。
Ic 可以通过下面公式算出：

Ic = -Ia - Ib
```

---

## 4. Park 变换

Park 变换的作用是：

```text
两相静止坐标系 alpha/beta -> 两相旋转坐标系 d/q
```

也就是把电流转换到跟随转子旋转的坐标系里面。

公式：

```text
Id = I_alpha * cos(theta_e) + I_beta * sin(theta_e)

Iq = -I_alpha * sin(theta_e) + I_beta * cos(theta_e)
```

C 语言写法：

```c
typedef struct
{
    float d;
    float q;
} park_t;

park_t park_transform(float alpha, float beta, float theta_e)
{
    park_t out;

    float sin_t = sinf(theta_e);
    float cos_t = cosf(theta_e);

    out.d = alpha * cos_t + beta * sin_t;
    out.q = -alpha * sin_t + beta * cos_t;

    return out;
}
```

---

## 5. 电流环控制

FOC 标准闭环里，最内层是电流环。

目标一般是：

```text
Id_target = 0
Iq_target = 由速度环或者位置环给出
```

误差：

```text
err_d = Id_target - Id_meas
err_q = Iq_target - Iq_meas
```

PI 控制器：

```text
Ud = Kp_d * err_d + Ki_d * integral_d

Uq = Kp_q * err_q + Ki_q * integral_q
```

积分项：

```text
integral_d += err_d * dt
integral_q += err_q * dt
```

C 语言写法：

```c
typedef struct
{
    float kp;
    float ki;
    float integral;
    float out_min;
    float out_max;
} pi_ctrl_t;

float pi_update(pi_ctrl_t *pi, float target, float feedback, float dt)
{
    float err = target - feedback;

    pi->integral += err * dt;

    float out = pi->kp * err + pi->ki * pi->integral;

    if (out > pi->out_max)
    {
        out = pi->out_max;
    }

    if (out < pi->out_min)
    {
        out = pi->out_min;
    }

    return out;
}
```

---

## 6. Park 逆变换

Park 逆变换的作用是：

```text
d/q 旋转坐标系 -> alpha/beta 静止坐标系
```

如果你现在是电压模式 FOC，没有采样电流，那么一般直接给：

```text
Ud = 0
Uq = 位置环或者速度环输出
```

公式：

```text
U_alpha = Ud * cos(theta_e) - Uq * sin(theta_e)

U_beta = Ud * sin(theta_e) + Uq * cos(theta_e)
```

C 语言写法：

```c
typedef struct
{
    float alpha;
    float beta;
} alpha_beta_t;

alpha_beta_t inv_park_transform(float ud, float uq, float theta_e)
{
    alpha_beta_t out;

    float sin_t = sinf(theta_e);
    float cos_t = cosf(theta_e);

    out.alpha = ud * cos_t - uq * sin_t;
    out.beta  = ud * sin_t + uq * cos_t;

    return out;
}
```

---

## 7. Clarke 逆变换

Clarke 逆变换的作用是：

```text
alpha/beta -> 三相电压 Ua/Ub/Uc
```

公式：

```text
Ua = U_alpha

Ub = -0.5 * U_alpha + sqrt(3)/2 * U_beta

Uc = -0.5 * U_alpha - sqrt(3)/2 * U_beta
```

C 语言写法：

```c
typedef struct
{
    float ua;
    float ub;
    float uc;
} phase_voltage_t;

phase_voltage_t inv_clarke_transform(float alpha, float beta)
{
    phase_voltage_t out;

    out.ua = alpha;
    out.ub = -0.5f * alpha + 0.86602540378f * beta;
    out.uc = -0.5f * alpha - 0.86602540378f * beta;

    return out;
}
```

---

## 8. SVPWM 公式

### 8.1 零序注入 SVPWM

先通过 Park 逆变换和 Clarke 逆变换得到：

```text
Ua, Ub, Uc
```

然后找最大值和最小值：

```text
Umax = max(Ua, Ub, Uc)
Umin = min(Ua, Ub, Uc)
```

计算零序分量：

```text
Uzero = -0.5 * (Umax + Umin)
```

注入零序分量：

```text
Ua_svpwm = Ua + Uzero
Ub_svpwm = Ub + Uzero
Uc_svpwm = Uc + Uzero
```

这样做的作用是：

```text
把三相波形整体上下平移，让母线电压利用率更高。
```

C 语言写法：

```c
phase_voltage_t svpwm_zero_sequence(phase_voltage_t in)
{
    phase_voltage_t out;

    float u_max = fmaxf(in.ua, fmaxf(in.ub, in.uc));
    float u_min = fminf(in.ua, fminf(in.ub, in.uc));

    float u_zero = -0.5f * (u_max + u_min);

    out.ua = in.ua + u_zero;
    out.ub = in.ub + u_zero;
    out.uc = in.uc + u_zero;

    return out;
}
```

---

### 8.2 相电压转占空比

假设母线电压是：

```text
Vbus
```

三相电压范围大概是：

```text
-Vbus/2 ~ +Vbus/2
```

占空比公式：

```text
duty_a = Ua / Vbus + 0.5

duty_b = Ub / Vbus + 0.5

duty_c = Uc / Vbus + 0.5
```

C 语言写法：

```c
typedef struct
{
    float duty_a;
    float duty_b;
    float duty_c;
} pwm_duty_t;

pwm_duty_t phase_voltage_to_duty(phase_voltage_t u, float vbus)
{
    pwm_duty_t duty;

    duty.duty_a = u.ua / vbus + 0.5f;
    duty.duty_b = u.ub / vbus + 0.5f;
    duty.duty_c = u.uc / vbus + 0.5f;

    return duty;
}
```

需要限幅：

```c
float limit_float(float x, float min, float max)
{
    if (x > max)
    {
        return max;
    }

    if (x < min)
    {
        return min;
    }

    return x;
}
```

占空比限幅：

```c
duty.duty_a = limit_float(duty.duty_a, 0.02f, 0.98f);
duty.duty_b = limit_float(duty.duty_b, 0.02f, 0.98f);
duty.duty_c = limit_float(duty.duty_c, 0.02f, 0.98f);
```

---

## 9. 电压模式 FOC 完整流程

如果没有电流采样，只有编码器，可以先做电压模式 FOC。

流程：

```text
1. 读取机械角度 theta_m
2. 机械角度转电角度 theta_e
3. 位置环/速度环计算 Uq
4. Ud = 0
5. Park 逆变换：Ud/Uq -> Ualpha/Ubeta
6. Clarke 逆变换：Ualpha/Ubeta -> Ua/Ub/Uc
7. SVPWM 零序注入
8. 相电压转 PWM 占空比
9. 输出到三相桥
```

伪代码：

```c
void foc_voltage_control(float theta_m_rad,
                         float target_angle_deg,
                         float vbus)
{
    float theta_e = get_electrical_angle(theta_m_rad,
                                         pole_pairs,
                                         theta_offset_rad);

    float now_angle_deg = rad_to_deg(theta_m_rad);

    float err_angle = angle_error_deg(target_angle_deg, now_angle_deg);

    float uq = kp_pos * err_angle;

    uq = limit_float(uq, -UQ_LIMIT, UQ_LIMIT);

    float ud = 0.0f;

    alpha_beta_t ab = inv_park_transform(ud, uq, theta_e);

    phase_voltage_t uvw = inv_clarke_transform(ab.alpha, ab.beta);

    phase_voltage_t uvw_svpwm = svpwm_zero_sequence(uvw);

    pwm_duty_t duty = phase_voltage_to_duty(uvw_svpwm, vbus);

    set_pwm_duty(duty.duty_a, duty.duty_b, duty.duty_c);
}
```

---

## 10. 位置环公式

位置环输入：

```text
target_angle 目标角度
now_angle    当前角度
```

角度误差：

```text
err_angle = target_angle - now_angle
```

但是角度是 0°~360° 循环的，所以要取最短路径。

公式：

```text
如果 err > 180°，err -= 360°
如果 err < -180°，err += 360°
```

C 语言写法：

```c
float angle_error_deg(float target_deg, float now_deg)
{
    float err = target_deg - now_deg;

    while (err > 180.0f)
    {
        err -= 360.0f;
    }

    while (err < -180.0f)
    {
        err += 360.0f;
    }

    return err;
}
```

位置 P 控制：

```text
Uq = Kp_pos * err_angle
```

位置 PD 控制：

```text
Uq = Kp_pos * err_angle + Kd_pos * (err_angle - last_err_angle) / dt
```

简单代码：

```c
float pos_loop_update(float target_deg,
                      float now_deg,
                      float dt)
{
    static float last_err = 0.0f;

    float err = angle_error_deg(target_deg, now_deg);

    float d_err = (err - last_err) / dt;

    float uq = kp_pos * err + kd_pos * d_err;

    uq = limit_float(uq, -UQ_LIMIT, UQ_LIMIT);

    last_err = err;

    return uq;
}
```

新手阶段建议先不用 D：

```text
Uq = Kp_pos * err_angle
```

---

## 11. 速度计算公式

机械角速度：

```text
omega_m = delta_theta_m / dt
```

如果角度单位是弧度：

```text
omega_m_rad_s = delta_theta_m_rad / dt
```

RPM 转换：

```text
rpm = omega_m_rad_s * 60 / 2π
```

如果角度单位是度：

```text
rpm = delta_theta_deg / 360 / dt * 60
```

需要处理跨 0° 问题：

```c
float angle_delta_deg(float now_deg, float last_deg)
{
    float delta = now_deg - last_deg;

    while (delta > 180.0f)
    {
        delta -= 360.0f;
    }

    while (delta < -180.0f)
    {
        delta += 360.0f;
    }

    return delta;
}
```

速度计算：

```c
float get_rpm_by_angle(float now_deg, float last_deg, float dt)
{
    float delta_deg = angle_delta_deg(now_deg, last_deg);

    float rpm = delta_deg / 360.0f / dt * 60.0f;

    return rpm;
}
```

---

## 12. 速度环公式

速度环输入：

```text
target_rpm 目标速度
now_rpm    当前速度
```

速度误差：

```text
err_speed = target_rpm - now_rpm
```

速度 PI 控制：

```text
Uq = Kp_speed * err_speed + Ki_speed * integral_speed
```

积分：

```text
integral_speed += err_speed * dt
```

C 语言写法：

```c
float speed_loop_update(float target_rpm,
                        float now_rpm,
                        float dt)
{
    static float integral = 0.0f;

    float err = target_rpm - now_rpm;

    integral += err * dt;

    integral = limit_float(integral, -INTEGRAL_LIMIT, INTEGRAL_LIMIT);

    float uq = kp_speed * err + ki_speed * integral;

    uq = limit_float(uq, -UQ_LIMIT, UQ_LIMIT);

    return uq;
}
```

---

## 13. 位置环 + 速度环串级控制

更标准的闭环结构是：

```text
位置环输出目标速度
速度环输出目标 Uq
```

也就是：

```text
position_error = target_position - now_position

target_speed = Kp_pos * position_error

speed_error = target_speed - now_speed

Uq = Kp_speed * speed_error + Ki_speed * integral_speed
```

结构：

```text
目标位置
   ↓
位置环 P
   ↓
目标速度
   ↓
速度环 PI
   ↓
Uq
   ↓
FOC/SVPWM
   ↓
电机
```

C 语言伪代码：

```c
float position_speed_loop(float target_pos_deg,
                          float now_pos_deg,
                          float now_rpm,
                          float dt)
{
    float pos_err = angle_error_deg(target_pos_deg, now_pos_deg);

    float target_rpm = kp_pos * pos_err;

    target_rpm = limit_float(target_rpm,
                             -TARGET_RPM_LIMIT,
                             TARGET_RPM_LIMIT);

    float uq = speed_loop_update(target_rpm, now_rpm, dt);

    return uq;
}
```

这种比直接：

```text
位置误差 -> Uq
```

更稳。

---

## 14. 完整三环 FOC 结构

标准高性能 FOC 是三环：

```text
位置环 -> 速度环 -> 电流环 -> SVPWM
```

完整结构：

```text
target_position
      ↓
位置环 P/PI/PD
      ↓
target_speed
      ↓
速度环 PI
      ↓
target_Iq
      ↓
电流环 PI
      ↓
Uq
      ↓
Park 逆变换
      ↓
Clarke 逆变换
      ↓
SVPWM
      ↓
三相 PWM
      ↓
电机
      ↓
编码器/电流采样反馈
```

公式：

```text
位置环：

err_pos = target_pos - now_pos
target_speed = Kp_pos * err_pos


速度环：

err_speed = target_speed - now_speed
target_Iq = Kp_speed * err_speed + Ki_speed * integral_speed


电流环：

err_q = target_Iq - Iq
Uq = Kp_q * err_q + Ki_q * integral_q

err_d = 0 - Id
Ud = Kp_d * err_d + Ki_d * integral_d
```

最终：

```text
Ud, Uq -> Ualpha, Ubeta -> Ua, Ub, Uc -> PWM
```

---

# FOC 调参流程

## 1. 调参前先检查硬件

先不要急着调 PID。

必须确认下面这些东西是对的：

```text
1. 三相 PWM 输出正常
2. 三相顺序正确
3. MOS 驱动芯片 EN 脚正常
4. 母线电压 Vbus 正确
5. 编码器角度读取正常
6. 编码器方向和电机实际方向一致
7. 极对数 pole_pairs 设置正确
8. 电角度零点 offset 校准正确
9. PWM 占空比没有打到 0% 或 100%
10. 电源电流限制合适
```

如果这些不对，PID 怎么调都不稳。

---

## 2. 第一步：开环测试

目的：

```text
确认三相桥、SVPWM、电机相序基本正常。
```

开环给定：

```text
Ud = 0
Uq = 0.5V ~ 1.5V
theta_e 按固定速度递增
```

机械角速度：

```text
omega_m = target_rpm * 2π / 60
```

电角速度：

```text
omega_e = pole_pairs * omega_m
```

角度积分：

```text
theta_e += omega_e * dt
```

C 语言思路：

```c
theta_e += omega_e * dt;
theta_e = normalize_rad(theta_e);

vfoc_set_svpwm(uq, 0.0f, vbus);
```

新手推荐参数：

```text
Uq = 0.5V ~ 1.0V
RPM = 10 ~ 100 rpm
```

现象判断：

```text
能平滑低速转动：基本正确
只抖不转：相序、电角度、Uq 太小或太大有问题
发热很快：角度不对、相序不对、Uq 太大
声音很大：SVPWM/角度/相序/频率可能有问题
```

---

## 3. 第二步：校准电角度零点

FOC 需要知道：

```text
转子的机械 0 点对应哪个电角度位置
```

常见方法：

```text
1. 给一个固定电角度 theta_e = 0
2. 给一个较小 Uq 或 Ud
3. 让转子吸到固定位置
4. 读取编码器机械角度 theta_m
5. 计算 theta_offset
```

电角度公式：

```text
theta_e = pole_pairs * theta_m + theta_offset
```

所以：

```text
theta_offset = theta_e_target - pole_pairs * theta_m
```

如果校准时固定：

```text
theta_e_target = 0
```

那么：

```text
theta_offset = -pole_pairs * theta_m
```

---

## 4. 第三步：验证闭环力矩模式

先不做位置环，也不做速度环。

直接给：

```text
Ud = 0
Uq = 小电压
```

比如：

```text
Uq = 0.3V
Uq = 0.5V
Uq = 0.8V
```

现象：

```text
用手转动电机，应该有明显阻力或者回弹感。
```

如果：

```text
正 Uq 和负 Uq 方向反了
```

可以改：

```c
uq = -uq;
```

或者修正编码器方向 / 相序 / 电角度方向。

---

## 5. 第四步：调位置环 P

先使用最简单的位置环：

```text
Uq = Kp_pos * err_angle
```

不要加 I，不要加 D。

推荐初始参数：

```text
Kp_pos = 0.01 ~ 0.05 V/deg
Uq_limit = 0.5V ~ 1.0V
```

现象判断：

```text
Kp 太小：
电机回目标很慢，手一扭就偏很多。

Kp 合适：
能回到目标位置，有回弹，抖动不明显。

Kp 太大：
目标附近来回抖，启动猛冲，声音变大。
```

调参方法：

```text
1. Kp 从很小开始
2. 慢慢加大 Kp
3. 加到开始轻微抖动
4. 再回退 30% 左右
```

比如：

```text
0.01 -> 0.02 -> 0.03 -> 0.05 -> 0.08
```

发现 `0.08` 开始抖，那就退到：

```text
0.05 左右
```

---

## 6. 第五步：加 Uq 限幅

位置环必须加限幅。

公式：

```text
Uq = limit(Uq, -Uq_limit, Uq_limit)
```

推荐：

```text
空载学习阶段：Uq_limit = 0.5V ~ 1.5V
小电机轻载：Uq_limit = 1V ~ 3V
12V 母线初期：不要一上来给 4V、6V
```

C 语言：

```c
uq = limit_float(uq, -UQ_LIMIT, UQ_LIMIT);
```

---

## 7. 第六步：加目标角度斜坡

不要让目标角度突然从当前角度跳到目标角度。

错误做法：

```text
当前角度 = 280°
目标角度 = 10°
一上电直接 target = 10°
```

更好的做法：

```text
刚启动时：
soft_target = now_angle

然后：
soft_target 慢慢靠近 target_angle
```

公式：

```text
soft_target += limit(target_angle - soft_target, -step, +step)
```

对于环形角度，要用最短路径误差：

```c
soft_target = angle_move_towards_deg(soft_target,
                                     target_angle,
                                     max_step_deg);
```

这样可以避免刚上电猛震。

---

## 8. 第七步：根据需要加 D

位置环的 D 项可以增加阻尼。

公式：

```text
Uq = Kp * err + Kd * d_err
```

其中：

```text
d_err = (err - last_err) / dt
```

但是新手阶段要注意：

```text
D 项很容易放大编码器噪声。
D 项刚启动容易产生微分冲击。
```

所以启动第一拍要做：

```c
last_err = err;
```

不要让 D 项第一拍突然爆出来。

推荐：

```text
先不用 D。
P 调顺以后，再小幅度加 D。
```

---

## 9. 第八步：速度环调参

如果做速度闭环，建议结构是：

```text
速度环输出 Uq
```

速度环公式：

```text
err_speed = target_rpm - now_rpm

Uq = Kp_speed * err_speed + Ki_speed * integral
```

调参顺序：

```text
1. Ki = 0
2. 只调 Kp_speed
3. Kp_speed 从小到大
4. 加到速度响应比较快，但不明显震荡
5. 再慢慢加 Ki 消除稳态误差
```

现象：

```text
Kp_speed 太小：
速度跟随慢。

Kp_speed 太大：
速度来回波动，声音大。

Ki_speed 太小：
带负载后速度达不到目标。

Ki_speed 太大：
速度上下摆动，容易积分饱和。
```

---

## 10. 第九步：位置环 + 速度环调参

推荐结构：

```text
位置环输出目标速度
速度环输出 Uq
```

调参顺序：

```text
1. 先单独调好速度环
2. 再外面套位置环
3. 位置环先只用 P
4. 位置环 Kp 从小到大
5. 限制位置环输出的目标速度
6. 限制速度环输出的 Uq
```

公式：

```text
target_rpm = Kp_pos * err_pos

target_rpm = limit(target_rpm, -rpm_limit, rpm_limit)

Uq = speed_loop(target_rpm, now_rpm)
```

这样比直接：

```text
Uq = Kp_pos * err_pos
```

更稳。

---

## 11. 第十步：电流环调参

如果硬件有电流采样，最好做电流环。

电流环目标：

```text
Id_target = 0
Iq_target = 速度环输出
```

电流环输出：

```text
Ud = PI(Id_target - Id)
Uq = PI(Iq_target - Iq)
```

调参顺序：

```text
1. 先确认电流采样方向正确
2. 确认 Ia + Ib + Ic 接近 0
3. 确认 Clarke/Park 后 Id/Iq 合理
4. 先调 Iq 环
5. 再调 Id 环
6. 最后再套速度环和位置环
```

---

# 新手推荐总调参顺序

## 阶段 1：只验证 SVPWM

```text
目标：三相 PWM 正常，电机能开环转。
```

参数：

```text
Uq = 0.5V ~ 1.0V
RPM = 10 ~ 100 rpm
```

---

## 阶段 2：验证编码器

```text
目标：角度连续，方向正确，无明显跳变。
```

检查：

```text
手动顺时针转，角度应该连续增加或者连续减小。
跨 0° 时不能突然算出巨大速度。
```

---

## 阶段 3：校准电角度

```text
目标：theta_e 和转子真实磁场方向对上。
```

如果没校准好，表现为：

```text
电机抖
发热
力矩小
有些角度有力，有些角度没力
```

---

## 阶段 4：力矩模式

```text
目标：给小 Uq，有稳定力矩。
```

参数：

```text
Ud = 0
Uq = 0.3V ~ 1.0V
```

---

## 阶段 5：位置 P 环

```text
目标：能回到目标角度。
```

参数：

```text
Kp_pos = 0.01 ~ 0.05 V/deg
Kd_pos = 0
Uq_limit = 0.5V ~ 1.0V
```

---

## 阶段 6：加软启动

```text
目标：上电不猛震。
```

做法：

```text
soft_target = now_angle
soft_target 慢慢靠近 target_angle
Uq 加限幅
Uq 加斜率限制
```

---

## 阶段 7：加速度环

```text
目标：位置控制更平滑。
```

结构：

```text
位置环 -> 目标速度
速度环 -> Uq
```

---

## 阶段 8：加电流环

```text
目标：力矩真正可控，保护更完善。
```

结构：

```text
位置环 -> 速度环 -> 电流环 -> SVPWM
```

---

# 常见问题排查表

## 1. 电机只抖不转

可能原因：

```text
1. 相序错
2. 电角度方向错
3. 极对数错
4. 编码器方向错
5. theta_offset 没校准
6. Uq 太大或者太小
```

---

## 2. 电机发热快

可能原因：

```text
1. 电角度不准
2. Ud/Uq 方向错
3. 相序错
4. 长时间大 Uq 堵转
5. 没有电流限制
```

---

## 3. 位置闭环猛地一震

可能原因：

```text
1. 目标角度阶跃太大
2. Kp 太大
3. Uq 没有限幅
4. D 项第一拍冲击
5. 没有目标角度斜坡
```

解决：

```text
1. 上电 soft_target = now_angle
2. soft_target 慢慢靠近 target_angle
3. Uq 限幅
4. Uq 斜率限制
5. 先关闭 D 项
```

---

## 4. 目标附近来回抖

可能原因：

```text
1. Kp 太大
2. 编码器噪声大
3. 死区太小
4. D 项太大
5. 速度估算噪声大
```

解决：

```text
1. 降低 Kp
2. 加位置死区
3. 降低 D
4. 对速度做低通滤波
5. 提高控制周期稳定性
```

---

# 推荐初始参数

对于 12V 小 BLDC，新手电压模式 FOC 可以先这样：

```c
#define MOTOR_DRV_VBUS      12.0f

#define UQ_LIMIT_START      0.8f
#define UQ_LIMIT_RUN        1.5f

#define POS_KP_START        0.02f
#define POS_KD_START        0.0f

#define POS_DEADBAND_DEG    0.5f

#define SOFT_STEP_DEG       0.02f
```

如果控制周期是 1ms：

```text
SOFT_STEP_DEG = 0.02°/ms
等效目标速度 = 20°/s
```

如果觉得回零太慢，可以改成：

```text
0.05°/ms = 50°/s
0.10°/ms = 100°/s
```

---

# 最重要的调参原则

```text
先小电压
先开环
先确认方向
先确认角度
先 P 后 I/D
先限幅再加大参数
先慢后快
先空载后带载
```

FOC 调参不是一上来就猛调 PID。

正确顺序是：

```text
硬件正确
角度正确
方向正确
零点正确
SVPWM 正确
小 Uq 有力矩
位置 P 环稳定
速度环稳定
最后再追求响应速度
```
