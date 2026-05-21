下面给你一套**STM32F407 HAL 库版：开环无感 FOC 控制 BLDC 的代码骨架**。

它的核心思想是：

> MCU 不知道转子真实位置，也不用霍尔/编码器/反电动势估算，而是自己生成一个不断旋转的电角度 `theta_e`，然后用 FOC 反 Park + SVPWM 输出三相正弦电压，让电机跟着这个旋转磁场转。

---

# 1. 先说清楚：这不是严格意义的“真正无感 FOC”

真正无感 FOC 一般是：

```text
ADC采样三相电流/母线电压
        ↓
Clarke/Park变换
        ↓
电流PI环
        ↓
位置/速度观测器：SMO / PLL / Luenberger / HFI
        ↓
估算转子电角度
        ↓
SVPWM输出
```

你现在要的“开环无感 FOC”是：

```text
给定目标速度 rpm
        ↓
MCU自己积分出电角度 theta_e
        ↓
给一个固定 Vq 电压
        ↓
反Park变换
        ↓
SVPWM输出三相PWM
        ↓
电机被旋转磁场拖着转
```

也就是：

```c
theta_e += omega_e * Ts;
```

这里的 `theta_e` 不是测出来的，是 MCU 自己算出来的。

---

# 2. CubeMX / HAL 外设配置建议

以 STM32F407 + 三相半桥驱动为例。

## TIM1 配置

建议用高级定时器 TIM1：

```text
TIM1_CH1  -> U相高边 PWM
TIM1_CH1N -> U相低边 PWM

TIM1_CH2  -> V相高边 PWM
TIM1_CH2N -> V相低边 PWM

TIM1_CH3  -> W相高边 PWM
TIM1_CH3N -> W相低边 PWM
```

推荐参数：

```text
PWM频率：20kHz
计数模式：Center Aligned 中心对齐
PWM模式：PWM Mode 1
Dead Time：根据MOS和驱动芯片设置，例如 500ns ~ 1us
Break 功能：建议开启，用于过流保护
```

假设：

```text
定时器时钟 TIM1CLK = 168MHz
中心对齐 PWM
目标 PWM = 20kHz
```

中心对齐模式下：

```text
PWM频率 = TIM_CLK / (2 * ARR)
ARR = TIM_CLK / (2 * PWM_FREQ)
ARR = 168MHz / (2 * 20kHz) = 4200
```

所以：

```c
htim1.Init.Period = 4200 - 1;
```

---

## 控制中断

可以用一个普通定时器，比如 TIM6，1kHz 或 10kHz 调用一次 FOC 更新。

学习阶段建议：

```text
FOC控制频率：10kHz
Ts = 0.0001s
```

也就是每 100us 更新一次 SVPWM。

---

# 3. 工程文件结构建议

```text
Core
 ├── Inc
 │    └── open_loop_foc.h
 └── Src
      ├── main.c
      └── open_loop_foc.c
```

---

# 4. open_loop_foc.h

```c
#ifndef __OPEN_LOOP_FOC_H
#define __OPEN_LOOP_FOC_H

#include "stm32f4xx_hal.h"
#include <math.h>
#include <stdint.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

/*
 * 开环FOC电机控制结构体
 */
typedef struct
{
    TIM_HandleTypeDef *htim;     // 三相PWM使用的定时器，例如 TIM1

    uint32_t ch_u;               // U相PWM通道，例如 TIM_CHANNEL_1
    uint32_t ch_v;               // V相PWM通道，例如 TIM_CHANNEL_2
    uint32_t ch_w;               // W相PWM通道，例如 TIM_CHANNEL_3

    float pwm_period;            // PWM周期ARR，例如 4199 或 4200

    uint8_t pole_pairs;          // 电机极对数，例如 7对极

    float vbus;                  // 母线电压，例如 12V / 24V

    float vd;                    // d轴电压，开环一般给0
    float vq;                    // q轴电压，决定输出力矩大小

    float target_rpm;            // 目标机械转速，单位 rpm
    float current_rpm;           // 当前斜坡后的机械转速，单位 rpm

    float accel_rpm_per_s;       // 加速度限制，单位 rpm/s

    float theta_e;               // 电角度，单位 rad，范围 0 ~ 2π

    float control_period_s;      // 控制周期，例如 0.0001s

    uint8_t enable;              // 电机使能标志

} OpenLoopFOC_HandleTypeDef;


/*
 * 初始化开环FOC
 */
void OpenLoopFOC_Init(OpenLoopFOC_HandleTypeDef *foc);


/*
 * 设置目标机械速度，单位 rpm
 */
void OpenLoopFOC_SetTargetRPM(OpenLoopFOC_HandleTypeDef *foc, float rpm);


/*
 * 设置q轴电压
 */
void OpenLoopFOC_SetVq(OpenLoopFOC_HandleTypeDef *foc, float vq);


/*
 * 使能电机输出
 */
void OpenLoopFOC_Enable(OpenLoopFOC_HandleTypeDef *foc);


/*
 * 关闭电机输出
 */
void OpenLoopFOC_Disable(OpenLoopFOC_HandleTypeDef *foc);


/*
 * FOC周期更新函数
 * 建议在10kHz定时器中断里调用
 */
void OpenLoopFOC_Update(OpenLoopFOC_HandleTypeDef *foc);

#endif
```

---

# 5. open_loop_foc.c

```c
#include "open_loop_foc.h"


/*
 * 限幅函数
 */
static float foc_constrain(float x, float min, float max)
{
    if (x < min)
    {
        return min;
    }
    else if (x > max)
    {
        return max;
    }
    else
    {
        return x;
    }
}


/*
 * 角度归一化到 0 ~ 2π
 */
static float foc_normalize_angle(float angle)
{
    while (angle >= 2.0f * M_PI)
    {
        angle -= 2.0f * M_PI;
    }

    while (angle < 0.0f)
    {
        angle += 2.0f * M_PI;
    }

    return angle;
}


/*
 * 设置三相PWM占空比
 *
 * duty_u / duty_v / duty_w 范围：0.0 ~ 1.0
 */
static void OpenLoopFOC_SetPWMDuty(OpenLoopFOC_HandleTypeDef *foc,
                                   float duty_u,
                                   float duty_v,
                                   float duty_w)
{
    uint32_t ccr_u;
    uint32_t ccr_v;
    uint32_t ccr_w;

    /*
     * 限制占空比范围
     * 不建议直接允许 0% 和 100%，给驱动和死区留一点空间
     */
    duty_u = foc_constrain(duty_u, 0.02f, 0.98f);
    duty_v = foc_constrain(duty_v, 0.02f, 0.98f);
    duty_w = foc_constrain(duty_w, 0.02f, 0.98f);

    ccr_u = (uint32_t)(duty_u * foc->pwm_period);
    ccr_v = (uint32_t)(duty_v * foc->pwm_period);
    ccr_w = (uint32_t)(duty_w * foc->pwm_period);

    __HAL_TIM_SET_COMPARE(foc->htim, foc->ch_u, ccr_u);
    __HAL_TIM_SET_COMPARE(foc->htim, foc->ch_v, ccr_v);
    __HAL_TIM_SET_COMPARE(foc->htim, foc->ch_w, ccr_w);
}


/*
 * SVPWM输出
 *
 * 输入：
 *   v_alpha / v_beta：αβ静止坐标系下的电压
 *
 * 这里用的是比较容易理解的零序注入SVPWM：
 *
 *   1. 先由 αβ 得到三相相电压 va/vb/vc
 *   2. 找最大值和最小值
 *   3. 注入 offset = (max + min) / 2
 *   4. 转成 duty
 */
static void OpenLoopFOC_SVPWM(OpenLoopFOC_HandleTypeDef *foc,
                              float v_alpha,
                              float v_beta)
{
    float va;
    float vb;
    float vc;

    float v_max;
    float v_min;
    float v_offset;

    float duty_u;
    float duty_v;
    float duty_w;

    /*
     * αβ -> 三相电压
     *
     * va = v_alpha
     * vb = -1/2 * v_alpha + sqrt(3)/2 * v_beta
     * vc = -1/2 * v_alpha - sqrt(3)/2 * v_beta
     */
    va = v_alpha;
    vb = -0.5f * v_alpha + 0.86602540378f * v_beta;
    vc = -0.5f * v_alpha - 0.86602540378f * v_beta;

    /*
     * 找最大最小值
     */
    v_max = va;
    if (vb > v_max)
    {
        v_max = vb;
    }
    if (vc > v_max)
    {
        v_max = vc;
    }

    v_min = va;
    if (vb < v_min)
    {
        v_min = vb;
    }
    if (vc < v_min)
    {
        v_min = vc;
    }

    /*
     * 零序注入
     * 这样比普通正弦PWM的母线利用率高一些
     */
    v_offset = 0.5f * (v_max + v_min);

    va = va - v_offset;
    vb = vb - v_offset;
    vc = vc - v_offset;

    /*
     * 电压转占空比
     *
     * 中点电压是 0.5 * Vbus
     *
     * duty = 0.5 + v_phase / Vbus
     */
    duty_u = 0.5f + va / foc->vbus;
    duty_v = 0.5f + vb / foc->vbus;
    duty_w = 0.5f + vc / foc->vbus;

    OpenLoopFOC_SetPWMDuty(foc, duty_u, duty_v, duty_w);
}


/*
 * 反Park变换
 *
 * dq旋转坐标系 -> αβ静止坐标系
 *
 * v_alpha = vd*cos(theta) - vq*sin(theta)
 * v_beta  = vd*sin(theta) + vq*cos(theta)
 */
static void OpenLoopFOC_InversePark(float vd,
                                    float vq,
                                    float theta_e,
                                    float *v_alpha,
                                    float *v_beta)
{
    float sin_t;
    float cos_t;

    sin_t = sinf(theta_e);
    cos_t = cosf(theta_e);

    *v_alpha = vd * cos_t - vq * sin_t;
    *v_beta  = vd * sin_t + vq * cos_t;
}


/*
 * 初始化
 */
void OpenLoopFOC_Init(OpenLoopFOC_HandleTypeDef *foc)
{
    foc->theta_e = 0.0f;
    foc->current_rpm = 0.0f;
    foc->enable = 0;

    /*
     * 启动三相PWM
     * 如果你用了互补PWM，也要启动 PWMN
     */
    HAL_TIM_PWM_Start(foc->htim, foc->ch_u);
    HAL_TIM_PWM_Start(foc->htim, foc->ch_v);
    HAL_TIM_PWM_Start(foc->htim, foc->ch_w);

    /*
     * 如果使用 TIM1/TIM8 的互补输出，需要打开下面这些
     * 注意：只有高级定时器支持 PWMN
     */
    HAL_TIMEx_PWMN_Start(foc->htim, foc->ch_u);
    HAL_TIMEx_PWMN_Start(foc->htim, foc->ch_v);
    HAL_TIMEx_PWMN_Start(foc->htim, foc->ch_w);

    /*
     * 初始化时输出50%占空比，三相平均电压为0
     */
    OpenLoopFOC_SetPWMDuty(foc, 0.5f, 0.5f, 0.5f);
}


/*
 * 设置目标速度
 */
void OpenLoopFOC_SetTargetRPM(OpenLoopFOC_HandleTypeDef *foc, float rpm)
{
    foc->target_rpm = rpm;
}


/*
 * 设置q轴电压
 */
void OpenLoopFOC_SetVq(OpenLoopFOC_HandleTypeDef *foc, float vq)
{
    /*
     * 建议限制最大输出电压
     * 开环时不要一下子给太大，否则容易过流
     *
     * 一般先从 Vbus 的 5%~20% 试
     * 例如 12V 母线，先试 0.5V ~ 2V
     */
    float max_vq = foc->vbus * 0.4f;

    foc->vq = foc_constrain(vq, -max_vq, max_vq);
}


/*
 * 使能输出
 */
void OpenLoopFOC_Enable(OpenLoopFOC_HandleTypeDef *foc)
{
    foc->enable = 1;
}


/*
 * 关闭输出
 */
void OpenLoopFOC_Disable(OpenLoopFOC_HandleTypeDef *foc)
{
    foc->enable = 0;
    foc->current_rpm = 0.0f;
    foc->theta_e = 0.0f;

    /*
     * 关闭时给三相50%占空比
     * 对三相桥来说，平均输出电压为0
     */
    OpenLoopFOC_SetPWMDuty(foc, 0.5f, 0.5f, 0.5f);
}


/*
 * 开环FOC主更新函数
 *
 * 建议调用频率：
 *   1kHz  可以学习演示
 *   10kHz 更适合实际电机控制
 */
void OpenLoopFOC_Update(OpenLoopFOC_HandleTypeDef *foc)
{
    float rpm_error;
    float rpm_step;
    float mech_rps;
    float elec_rad_per_s;

    float v_alpha;
    float v_beta;

    if (foc->enable == 0)
    {
        return;
    }

    /*
     * 1. 速度斜坡
     *
     * 不能直接 current_rpm = target_rpm
     * 因为开环控制没有位置反馈，速度突变很容易失步
     */
    rpm_error = foc->target_rpm - foc->current_rpm;

    rpm_step = foc->accel_rpm_per_s * foc->control_period_s;

    if (rpm_error > rpm_step)
    {
        foc->current_rpm += rpm_step;
    }
    else if (rpm_error < -rpm_step)
    {
        foc->current_rpm -= rpm_step;
    }
    else
    {
        foc->current_rpm = foc->target_rpm;
    }

    /*
     * 2. 机械速度 rpm -> 机械角速度 rps
     */
    mech_rps = foc->current_rpm / 60.0f;

    /*
     * 3. 机械角速度 -> 电角速度
     *
     * 电角速度 = 机械转速 * 极对数 * 2π
     */
    elec_rad_per_s = mech_rps * (float)foc->pole_pairs * 2.0f * M_PI;

    /*
     * 4. 开环积分电角度
     *
     * theta_e += omega_e * Ts
     */
    foc->theta_e += elec_rad_per_s * foc->control_period_s;
    foc->theta_e = foc_normalize_angle(foc->theta_e);

    /*
     * 5. 反Park变换
     *
     * 开环一般：
     *   vd = 0
     *   vq = 一个固定值
     *
     * vq越大，磁场越强，电机越有力，但也越容易过流发热
     */
    OpenLoopFOC_InversePark(foc->vd,
                            foc->vq,
                            foc->theta_e,
                            &v_alpha,
                            &v_beta);

    /*
     * 6. SVPWM输出三相PWM
     */
    OpenLoopFOC_SVPWM(foc, v_alpha, v_beta);
}
```

---

# 6. main.c 里怎么用

下面是关键用法，不是完整 CubeMX 生成文件。

假设你已经有：

```c
TIM_HandleTypeDef htim1; // 三相PWM
TIM_HandleTypeDef htim6; // 10kHz控制定时器
```

## main.c 添加

```c
#include "open_loop_foc.h"

OpenLoopFOC_HandleTypeDef motor1;
```

---

## 初始化电机参数

在 `main()` 里，MX 初始化之后添加：

```c
int main(void)
{
    HAL_Init();

    SystemClock_Config();

    MX_GPIO_Init();
    MX_TIM1_Init();
    MX_TIM6_Init();

    /*
     * 开环FOC参数配置
     */
    motor1.htim = &htim1;

    motor1.ch_u = TIM_CHANNEL_1;
    motor1.ch_v = TIM_CHANNEL_2;
    motor1.ch_w = TIM_CHANNEL_3;

    /*
     * 这里填 TIM1 的 ARR + 1
     * 如果 CubeMX 里 Period = 4199，那么这里填 4200
     */
    motor1.pwm_period = 4200.0f;

    /*
     * 电机极对数
     *
     * 例如：
     *   14极电机 -> 7对极
     *   12极电机 -> 6对极
     */
    motor1.pole_pairs = 7;

    /*
     * 母线电压
     * 先写死，后面可以用ADC实时采样
     */
    motor1.vbus = 12.0f;

    /*
     * d轴电压，开环一般给0
     */
    motor1.vd = 0.0f;

    /*
     * q轴电压
     * 初学建议小一点
     * 12V母线可以先试 0.5V ~ 1.5V
     */
    motor1.vq = 1.0f;

    /*
     * 目标速度
     * 先从低速开始
     */
    motor1.target_rpm = 300.0f;

    /*
     * 加速度限制
     * 开环一定要慢慢加速
     */
    motor1.accel_rpm_per_s = 300.0f;

    /*
     * 控制周期
     * 如果 TIM6 是 10kHz，则周期为 0.0001s
     */
    motor1.control_period_s = 0.0001f;

    OpenLoopFOC_Init(&motor1);

    /*
     * 启动控制定时器中断
     */
    HAL_TIM_Base_Start_IT(&htim6);

    /*
     * 使能电机
     */
    OpenLoopFOC_Enable(&motor1);

    while (1)
    {
        /*
         * 你可以在这里修改目标速度
         *
         * 例如：
         * OpenLoopFOC_SetTargetRPM(&motor1, 500.0f);
         * OpenLoopFOC_SetVq(&motor1, 1.5f);
         */
    }
}
```

---

## TIM6 中断回调

```c
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM6)
    {
        OpenLoopFOC_Update(&motor1);
    }
}
```

---

# 7. 推荐启动流程

开环无感 FOC 最怕一上来速度太高、电压太大。

建议启动流程：

```text
1. vq = 0.5V
2. target_rpm = 100 rpm
3. 慢慢增加 target_rpm
4. 如果电机抖动、不转，稍微增加 vq
5. 如果电机发热、啸叫、电流大，降低 vq
6. 找到能稳定拖动的 vq 和加速度
```

比如可以这样改：

```c
OpenLoopFOC_SetVq(&motor1, 0.8f);
OpenLoopFOC_SetTargetRPM(&motor1, 200.0f);
```

稳定后再试：

```c
OpenLoopFOC_SetTargetRPM(&motor1, 500.0f);
```

---

# 8. 这个开环FOC的优点

## 1. 代码简单，适合学习 FOC 原理

它没有复杂的：

```text
ADC电流采样
Clarke变换
Park变换
电流PI环
速度PI环
位置估算器
```

所以你很容易理解：

```text
电角度 theta_e 怎么转
dq电压怎么变成 αβ 电压
αβ 电压怎么变成三相PWM
```

---

## 2. 不需要霍尔、编码器

硬件简单。

```text
不用霍尔
不用编码器
不用磁编码器
不用反电动势检测
```

只要：

```text
STM32F407
三相半桥驱动
BLDC电机
电源
```

就能尝试让电机转起来。

---

## 3. 适合风扇、泵、空载实验

如果负载很轻，而且对速度精度要求不高，可以用这种方式做简单驱动。

例如：

```text
小风扇
小水泵
演示电机
FOC学习板
空载转动测试
```

---

## 4. 波形比六步换向更平滑

六步换向是：

```text
每60电角度切一次相
```

开环FOC输出接近正弦三相电流/电压，声音和抖动一般比六步小。

---

# 9. 这个开环FOC的缺点

## 1. 不知道转子真实位置，容易失步

这是最大问题。

MCU 以为电机在这个角度：

```c
theta_e = 1.5 rad;
```

但真实转子可能还没跟上。

一旦磁场转得太快，转子跟不上，就会：

```text
抖动
堵转
啸叫
电流暴涨
发热
失步
```

---

## 2. 不能真正闭环控制速度

你设置：

```c
target_rpm = 1000;
```

只是代表：

```text
MCU输出的旋转磁场速度 = 1000 rpm
```

不代表：

```text
电机真实速度 = 1000 rpm
```

因为你没有测速度。

所以它不是严格的速度闭环。

---

## 3. 带负载能力差

负载一变，电机可能跟不上。

例如：

```text
空载能转
一压电机轴就停
加重负载就失步
上坡就不稳
堵转后继续发热
```

所以它不适合平衡车、无人机、电动车这类强动态负载。

---

## 4. 低速和启动不可靠

真正的无感 FOC 低速也很难，因为反电动势小。

开环 FOC 虽然可以启动，但启动成功率跟这些因素关系很大：

```text
电机负载
vq大小
加速度斜坡
电机惯量
电源能力
极对数设置
PWM频率
死区设置
```

---

## 5. 没有电流环，容易过流

上面这份代码只控制电压，不控制电流。

所以如果堵转：

```text
反电动势很小
绕组等效阻抗低
电流会变大
MOS容易发热
电机也容易发热
```

实际产品必须加：

```text
母线电流采样
相电流采样
过流保护
过压保护
欠压保护
温度保护
堵转保护
```

---

# 10. 适合你的学习路线

建议你按这个顺序做：

```text
第一步：开环六步换向
第二步：开环正弦PWM
第三步：开环FOC + SVPWM
第四步：加ADC采样母线电流，做保护
第五步：加两电阻/三电阻相电流采样
第六步：做电流闭环FOC
第七步：做无感位置估算器
第八步：做速度闭环
第九步：做完整无感FOC
```

---

# 11. 这个代码后续怎么升级成真正无感FOC？

现在是：

```text
theta_e = 自己积分出来的角度
```

真正无感FOC要变成：

```text
theta_e = 观测器估算出来的角度
```

也就是增加：

```text
ADC采样相电流
        ↓
Clarke变换
        ↓
Park变换
        ↓
Id/Iq电流环
        ↓
反电动势估算
        ↓
PLL锁相
        ↓
得到真实转子电角度
```

最终结构变成：

```text
速度目标
   ↓
速度PI
   ↓
Iq目标
   ↓
电流PI
   ↓
Vd/Vq
   ↓
反Park
   ↓
SVPWM
   ↓
三相桥
   ↓
电机
   ↓
电流采样 + 无感观测器
   ↓
估算角度 theta_e
```

---

# 12. 实际调试时最容易错的地方

重点检查这几个：

```text
1. 电机极对数 pole_pairs 是否正确
2. 三相线 U/V/W 顺序是否正确
3. TIM1 PWMN 是否真的输出
4. 死区是否配置
5. 驱动芯片 EN 引脚是否拉高
6. vq 是否太小，电机没力
7. vq 是否太大，电机发热
8. target_rpm 是否太高
9. accel_rpm_per_s 是否太大
10. TIM6中断频率是否和 control_period_s 对得上
```

尤其是三相顺序。

如果电机抖动不转，可以试着交换任意两根相线，或者在代码里把速度设成负数：

```c
OpenLoopFOC_SetTargetRPM(&motor1, -300.0f);
```

---

# 13. 简单总结

这份代码实现的是：

```text
STM32F407 HAL
TIM1三相互补PWM
TIM6周期控制
开环电角度积分
反Park变换
SVPWM输出
BLDC开环旋转
```

它适合：

```text
学习FOC
验证三相逆变器
空载转电机
理解SVPWM
```

不适合直接用于：

```text
平衡车
无人机电调
电动车
大负载电机
高可靠产品
高动态速度控制
```

因为它没有：

```text
电流闭环
速度闭环
位置估算
堵转检测
过流保护
真实转速反馈
```

--------
--------
--------
SVPWM 全名叫：

```text
Space Vector PWM
空间矢量脉宽调制
```

它本质上是：**一种把 FOC 算出来的电压指令，转换成三相 PWM 占空比的方法。**

一句话理解：

> **FOC 负责算“我想给电机一个什么方向、什么大小的磁场”；SVPWM 负责把这个磁场变成三相桥臂的 PWM 波形。**

---

# 1. FOC 和 SVPWM 的关系

FOC 的完整控制链路大概是这样：

```text
速度目标 / 力矩目标
        ↓
电流环 PI 控制
        ↓
得到 Vd、Vq
        ↓
反 Park 变换
        ↓
得到 Vα、Vβ
        ↓
SVPWM
        ↓
得到 Ua、Ub、Uc 三相 PWM 占空比
        ↓
驱动三相逆变桥
        ↓
BLDC / PMSM 电机转动
```

所以：

```text
FOC 是控制算法
SVPWM 是 PWM 调制算法
```

FOC 不一定必须用 SVPWM，也可以用普通正弦 PWM。

但是实际做 FOC 时，**SVPWM 很常用**，因为它母线电压利用率更高，输出更平滑。

---

# 2. 先用最通俗的话讲

你有一个三相逆变桥：

```text
       电源 +
        |
   U上  V上  W上
    |    |    |
    U    V    W  → 接电机三相
    |    |    |
   U下  V下  W下
        |
       GND
```

每一相都可以通过 PWM 控制它的平均电压。

比如：

```text
U相占空比 70%
V相占空比 40%
W相占空比 20%
```

电机看到的就是三个相电压。

问题是：

> FOC 算出来的不是 U/V/W 三相占空比，而是一个二维平面上的电压矢量 Vα、Vβ。

SVPWM 的任务就是：

```text
把 Vα、Vβ 这个“目标电压矢量”
转换成 U/V/W 三相 PWM 占空比
```

---

# 3. 什么叫“空间矢量”？

三相电机里面，U/V/W 三相电流会合成一个旋转磁场。

可以把这个旋转磁场想象成一个箭头：

```text
          β轴
           ↑
           |
           |    目标电压矢量
           |      ↗
           |    /
           |  /
-----------+------------→ α轴
```

这个箭头有两个信息：

```text
1. 方向：磁场朝哪里
2. 大小：磁场强不强
```

FOC 的目标，就是控制这个箭头。

---

# 4. FOC 里面的 Vd、Vq 是什么？

FOC 会把电机控制拆成两个方向：

```text
d轴：磁场方向
q轴：产生力矩的方向
```

简单理解：

```text
Vd：控制磁通，一般 BLDC/PMSM 常设为 0
Vq：控制力矩，越大电机越有劲
```

FOC 算出来的是：

```text
Vd、Vq
```

但 PWM 不能直接用 Vd、Vq。

所以要经过反 Park 变换：

```text
Vd、Vq  →  Vα、Vβ
```

然后再经过 SVPWM：

```text
Vα、Vβ  →  三相 PWM 占空比
```

---

# 5. SVPWM 到底在干啥？

三相逆变桥有 6 个 MOS 管。

每一时刻，每相上桥或者下桥导通。

所以三相桥一共有 8 种开关状态：

```text
000
001
010
011
100
101
110
111
```

其中：

```text
000 和 111 是零矢量
其他 6 个是有效矢量
```

这 6 个有效矢量在空间里组成一个六边形：

```text
             V2
          /      \
       V3          V1
       |            |
       |            |
       V4          V6
          \      /
             V5
```

SVPWM 的思想是：

> 我想要的目标电压矢量，可能在两个基本矢量之间。
> 那我就在一个 PWM 周期内，让两个相邻矢量各工作一段时间，再插入零矢量。
> 这样平均下来，就等效得到了目标矢量。

比如目标矢量在 V1 和 V2 之间：

```text
目标矢量 = V1作用一段时间 + V2作用一段时间 + 零矢量补时间
```

也就是：

```text
一个 PWM 周期 Ts 里面：
T1 时间输出 V1
T2 时间输出 V2
T0 时间输出零矢量

T1 + T2 + T0 = Ts
```

这样电机看到的是平均电压。

---

# 6. 类比一下

假设你要推一个箱子往右上方走。

但是你只有两个方向的力：

```text
向右推
向上推
```

那你可以：

```text
先向右推一会儿
再向上推一会儿
```

平均效果就是：

```text
箱子往右上方走
```

SVPWM 也是这个意思。

目标电压矢量不一定刚好等于某个桥臂状态，那就用相邻两个矢量轮流输出，平均合成出来。

---

# 7. SVPWM 和普通 SPWM 的区别

普通正弦 PWM，SPWM，是这样：

```text
直接生成三相正弦波占空比
```

比如：

```text
Ua = sin(θ)
Ub = sin(θ - 120°)
Uc = sin(θ + 120°)
```

SVPWM 是从空间矢量角度考虑三相逆变器的开关状态。

对比一下：

| 项目       | SPWM    | SVPWM   |
| -------- | ------- | ------- |
| 思路       | 三相正弦波调制 | 空间矢量合成  |
| 母线利用率    | 较低      | 更高      |
| 电机电压输出能力 | 稍弱      | 更强      |
| FOC中使用   | 可以用     | 很常用     |
| 实现难度     | 简单      | 稍复杂     |
| 输出效果     | 平滑      | 更适合电机控制 |

SVPWM 的母线利用率大约比 SPWM 高 15%。

这意味着：

```text
同样 24V 电源，用 SVPWM 能输出更高的有效相电压
电机高速能力更好
同样转速下可能更有力
```

---

# 8. FOC 没有 SVPWM 行不行？

可以。

FOC 只是一套控制思想：

```text
采样电流
坐标变换
PI控制
再变换回来
输出PWM
```

最后输出 PWM 的方式可以是：

```text
1. SPWM
2. SVPWM
3. DPWM
4. 六步换向
```

但是最常见的是：

```text
FOC + SVPWM
```

因为它比较适合三相逆变桥。

---

# 9. 用代码看它们的关系

FOC 前面部分：

```c
// 1. FOC算法算出 dq 电压
float vd = 0.0f;
float vq = 1.0f;

// 2. 根据电角度 theta_e 做反Park变换
v_alpha = vd * cosf(theta_e) - vq * sinf(theta_e);
v_beta  = vd * sinf(theta_e) + vq * cosf(theta_e);

// 3. 把 v_alpha / v_beta 交给SVPWM
SVPWM(v_alpha, v_beta);
```

SVPWM 里面做的事情：

```c
// 把 alpha/beta 电压变成三相电压
va = v_alpha;
vb = -0.5f * v_alpha + 0.866f * v_beta;
vc = -0.5f * v_alpha - 0.866f * v_beta;

// 注入零序分量，提高母线利用率
v_offset = (v_max + v_min) / 2;

// 转成三相PWM占空比
duty_u = 0.5f + va / vbus;
duty_v = 0.5f + vb / vbus;
duty_w = 0.5f + vc / vbus;
```

所以你可以理解为：

```text
FOC 负责算 v_alpha、v_beta
SVPWM 负责算 duty_u、duty_v、duty_w
```

---

# 10. 为什么叫“空间矢量 PWM”？

因为三相电压可以等效成一个二维空间里的矢量。

三相电压：

```text
Ua、Ub、Uc
```

经过 Clarke 变换后，变成：

```text
Vα、Vβ
```

这个 `Vα、Vβ` 就是空间里的一个矢量。

所以叫：

```text
Space Vector
空间矢量
```

然后通过 PWM 时间分配来合成这个矢量，所以叫：

```text
Space Vector PWM
空间矢量脉宽调制
```

---

# 11. 对 BLDC 来说，SVPWM 有啥作用？

BLDC 电机本质上需要三相电流形成旋转磁场。

SVPWM 可以让三相桥输出更接近理想旋转磁场。

好处是：

```text
1. 转矩更平滑
2. 噪声更小
3. 震动更小
4. 母线利用率更高
5. 更适合高速
6. 更适合FOC电流闭环
```

相比六步换向：

```text
六步换向：磁场一格一格跳
SVPWM/FOC：磁场连续旋转
```

所以六步换向声音更明显，转矩脉动更大。

---

# 12. 重点总结

你可以这样记：

```text
FOC：
    控制算法，决定电机磁场应该怎么转、力矩多大。

SVPWM：
    调制算法，把FOC算出的电压矢量，变成三相PWM占空比。

三相逆变桥：
    执行机构，根据PWM真正给电机三相绕组加电压。
```

最核心的一句话：

```text
FOC 是大脑，SVPWM 是把大脑命令翻译成三相PWM的翻译器。
```

FOC 没有 SVPWM 也能做，但实际工程里：

```text
FOC + SVPWM + 三相电流采样 + 电流PI环
```

才是比较标准的电机控制方案。

-----
-------
------

对，**STM32 没有“SVPWM 外设”**。

STM32 只有：

```text
定时器 TIMx
PWM通道 CH1/CH2/CH3
比较寄存器 CCR1/CCR2/CCR3
死区/互补输出/刹车保护
```

所谓 **SVPWM**，其实是你在软件里算出三相占空比，然后写到定时器的 `CCR1/CCR2/CCR3` 里。

也就是：

```text
FOC算法 / SVPWM算法
        ↓ 软件计算
duty_u duty_v duty_w
        ↓
写入 TIM1->CCR1 / CCR2 / CCR3
        ↓
TIM1 自动输出三相PWM
```

所以 STM32 不需要专门的 SVPWM 外设。

---

# 1. 最简单理解

你配置好 TIM1 三路 PWM：

```text
TIM1_CH1 -> U相 PWM
TIM1_CH2 -> V相 PWM
TIM1_CH3 -> W相 PWM
```

然后在控制中断里不断更新：

```c
TIM1->CCR1 = U相占空比;
TIM1->CCR2 = V相占空比;
TIM1->CCR3 = W相占空比;
```

这就是软件实现 SVPWM。

---

# 2. SVPWM 软件流程

完整流程是：

```text
1. FOC算出 Vd、Vq
2. 反Park变换得到 Vα、Vβ
3. SVPWM算法把 Vα、Vβ 转成 duty_u、duty_v、duty_w
4. 写入 TIM1 的 CCR1/CCR2/CCR3
5. TIM1 根据 CCR 自动输出三相PWM
```

STM32 定时器负责的是最后一步：

```text
根据 CCR 自动产生 PWM 波
```

SVPWM 算法本身是你代码算的。

---

# 3. 用零序注入法实现 SVPWM，最适合新手

传统 SVPWM 有扇区判断：

```text
判断目标矢量在哪个扇区
计算 T1、T2、T0
分配到三相占空比
```

这个方法理论清晰，但代码稍复杂。

实际工程里还有一种很常用、代码更简单的实现方式，叫：

```text
零序注入 SVPWM
```

它不用显式判断扇区，也能得到等效 SVPWM 效果。

---

# 4. 核心公式

你有 `v_alpha` 和 `v_beta`。

先变成三相电压：

```c
va = v_alpha;
vb = -0.5f * v_alpha + 0.8660254f * v_beta;
vc = -0.5f * v_alpha - 0.8660254f * v_beta;
```

然后找最大值和最小值：

```c
v_max = max(va, vb, vc);
v_min = min(va, vb, vc);
```

注入零序分量：

```c
v_offset = 0.5f * (v_max + v_min);

va = va - v_offset;
vb = vb - v_offset;
vc = vc - v_offset;
```

最后转成三相占空比：

```c
duty_u = 0.5f + va / vbus;
duty_v = 0.5f + vb / vbus;
duty_w = 0.5f + vc / vbus;
```

然后写入定时器比较寄存器：

```c
TIM1->CCR1 = duty_u * TIM1->ARR;
TIM1->CCR2 = duty_v * TIM1->ARR;
TIM1->CCR3 = duty_w * TIM1->ARR;
```

这就实现了 SVPWM。

---

# 5. STM32 HAL 版 SVPWM 函数

```c
#include "stm32f4xx_hal.h"
#include <math.h>

static float constrain_float(float x, float min, float max)
{
    if (x < min)
    {
        return min;
    }
    else if (x > max)
    {
        return max;
    }
    else
    {
        return x;
    }
}


/**
 * @brief  SVPWM输出函数
 *
 * @param  htim      PWM定时器，比如 &htim1
 * @param  v_alpha   α轴电压
 * @param  v_beta    β轴电压
 * @param  vbus      母线电压，比如 12.0f
 * @param  pwm_period PWM周期，也就是 ARR + 1
 *
 * @note   TIM1_CH1 -> U相
 *         TIM1_CH2 -> V相
 *         TIM1_CH3 -> W相
 */
void SVPWM_Output(TIM_HandleTypeDef *htim,
                  float v_alpha,
                  float v_beta,
                  float vbus,
                  float pwm_period)
{
    float va;
    float vb;
    float vc;

    float v_max;
    float v_min;
    float v_offset;

    float duty_u;
    float duty_v;
    float duty_w;

    uint32_t ccr_u;
    uint32_t ccr_v;
    uint32_t ccr_w;

    /*
     * 1. αβ坐标系 -> 三相电压
     *
     * va、vb、vc 是三相桥希望输出的平均相电压
     */
    va = v_alpha;

    vb = -0.5f * v_alpha + 0.8660254f * v_beta;

    vc = -0.5f * v_alpha - 0.8660254f * v_beta;

    /*
     * 2. 找三相电压中的最大值
     */
    v_max = va;

    if (vb > v_max)
    {
        v_max = vb;
    }

    if (vc > v_max)
    {
        v_max = vc;
    }

    /*
     * 3. 找三相电压中的最小值
     */
    v_min = va;

    if (vb < v_min)
    {
        v_min = vb;
    }

    if (vc < v_min)
    {
        v_min = vc;
    }

    /*
     * 4. 零序注入
     *
     * 普通SPWM是直接把 va/vb/vc 映射到占空比。
     * SVPWM会额外注入一个公共偏置，让三相整体上下平移。
     *
     * 注意：
     * 三相线电压不受这个公共偏置影响，
     * 但是可以提高母线电压利用率。
     */
    v_offset = 0.5f * (v_max + v_min);

    va = va - v_offset;
    vb = vb - v_offset;
    vc = vc - v_offset;

    /*
     * 5. 电压转占空比
     *
     * duty = 0.5 + v_phase / vbus
     *
     * 当 va = 0 时，占空比 = 50%
     * 当 va > 0 时，占空比 > 50%
     * 当 va < 0 时，占空比 < 50%
     */
    duty_u = 0.5f + va / vbus;
    duty_v = 0.5f + vb / vbus;
    duty_w = 0.5f + vc / vbus;

    /*
     * 6. 限幅
     *
     * 不建议让占空比到0%或100%，给死区和驱动芯片留余量。
     */
    duty_u = constrain_float(duty_u, 0.02f, 0.98f);
    duty_v = constrain_float(duty_v, 0.02f, 0.98f);
    duty_w = constrain_float(duty_w, 0.02f, 0.98f);

    /*
     * 7. 占空比转CCR
     */
    ccr_u = (uint32_t)(duty_u * pwm_period);
    ccr_v = (uint32_t)(duty_v * pwm_period);
    ccr_w = (uint32_t)(duty_w * pwm_period);

    /*
     * 8. 写入TIM比较寄存器
     *
     * HAL宏本质也是改 CCRx
     */
    __HAL_TIM_SET_COMPARE(htim, TIM_CHANNEL_1, ccr_u);
    __HAL_TIM_SET_COMPARE(htim, TIM_CHANNEL_2, ccr_v);
    __HAL_TIM_SET_COMPARE(htim, TIM_CHANNEL_3, ccr_w);
}
```

这就是最核心的 SVPWM 实现。

---

# 6. 和 PWM 外设的关系

比如你 TIM1 配置：

```text
ARR = 4199
```

那么 PWM 计数范围是：

```text
0 ~ 4199
```

如果你想让 U 相输出 50%：

```c
TIM1->CCR1 = 2100;
```

如果想让 U 相输出 75%：

```c
TIM1->CCR1 = 3150;
```

所以 SVPWM 最后只需要算出：

```text
U相占空比
V相占空比
W相占空比
```

然后转成 CCR。

---

# 7. 一个完整调用例子

假设你已经有 FOC 算出来的：

```c
float v_alpha = 1.0f;
float v_beta  = 0.5f;
```

母线电压：

```c
float vbus = 12.0f;
```

PWM ARR + 1：

```c
float pwm_period = 4200.0f;
```

调用：

```c
SVPWM_Output(&htim1, v_alpha, v_beta, 12.0f, 4200.0f);
```

它内部会自动更新：

```c
TIM1_CCR1
TIM1_CCR2
TIM1_CCR3
```

然后 TIM1 的 PWM 引脚就会输出三相 PWM。

---

# 8. TIM1 推荐配置

三相电机建议用 TIM1 或 TIM8，因为它们是高级定时器，支持：

```text
互补PWM
死区时间
刹车保护
中心对齐PWM
更新事件
```

建议：

```text
TIM1_CH1  -> U相高边
TIM1_CH1N -> U相低边

TIM1_CH2  -> V相高边
TIM1_CH2N -> V相低边

TIM1_CH3  -> W相高边
TIM1_CH3N -> W相低边
```

推荐 PWM 频率：

```text
16kHz ~ 25kHz
```

常用：

```text
20kHz
```

中心对齐模式更适合电机控制：

```text
Center Aligned PWM
```

好处是：

```text
电流采样更稳定
EMI较小
PWM波形对称
```

---

# 9. 控制中断里怎么用

一般不是在 `while(1)` 里面乱调，而是在固定周期中断里调。

比如 TIM6 10kHz 中断：

```c
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM6)
    {
        /*
         * 这里周期性执行FOC/SVPWM
         */
        Motor_Control_Loop();
    }
}
```

控制函数：

```c
void Motor_Control_Loop(void)
{
    float vd;
    float vq;
    float theta_e;

    float v_alpha;
    float v_beta;

    /*
     * 示例：开环FOC
     */
    vd = 0.0f;
    vq = 1.0f;
    theta_e += 0.01f;

    if (theta_e > 6.2831853f)
    {
        theta_e -= 6.2831853f;
    }

    /*
     * 反Park变换
     */
    v_alpha = vd * cosf(theta_e) - vq * sinf(theta_e);
    v_beta  = vd * sinf(theta_e) + vq * cosf(theta_e);

    /*
     * SVPWM输出
     */
    SVPWM_Output(&htim1,
                 v_alpha,
                 v_beta,
                 12.0f,
                 4200.0f);
}
```

---

# 10. 注意：TIM1 的 PWM 要先启动

初始化后要启动 PWM：

```c
HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);
HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_3);
```

如果你用互补输出，还要启动 PWMN：

```c
HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_1);
HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_2);
HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_3);
```

否则你可能只看到高边有波形，低边没有波形。

---

# 11. CubeMX 里 TIM1 重点配置

你大概这样配：

```text
TIM1
 ├── Clock Source: Internal Clock
 ├── Channel1: PWM Generation CH1 CH1N
 ├── Channel2: PWM Generation CH2 CH2N
 ├── Channel3: PWM Generation CH3 CH3N
 ├── Counter Mode: Center Aligned mode 1
 ├── Prescaler: 0
 ├── Period: 4199
 ├── PWM Mode: PWM mode 1
 ├── Pulse: 2100
 ├── Dead Time: 根据驱动设，比如 84 对应约 500ns，具体看时钟
 └── Break State: Enable，实际产品建议开
```

如果你的 TIM1 时钟是 168MHz，中心对齐模式下：

```text
PWM频率 = 168MHz / (2 * 4200) = 20kHz
```

---

# 12. 为什么零序注入能等效 SVPWM？

普通三相正弦 PWM 是：

```text
Ua、Ub、Uc 分别是三个正弦波
```

SVPWM 会在三相上一起加/减一个公共量。

这个公共量叫：

```text
零序分量
```

关键点是：

```text
电机真正关心的是线电压：
U-V
V-W
W-U
```

如果三相都一起加一个相同电压：

```text
Ua + offset
Ub + offset
Uc + offset
```

那么线电压变成：

```text
(Ua + offset) - (Ub + offset) = Ua - Ub
```

offset 抵消了。

所以：

```text
加公共偏置不会改变电机线电压
但可以让三相PWM更充分利用0~100%的占空比范围
```

这就是 SVPWM 母线利用率更高的原因。

---

# 13. 你可以先不用管扇区法

网上很多 SVPWM 教程会讲：

```text
第一扇区
第二扇区
第三扇区
T1/T2/T0
七段式SVPWM
五段式SVPWM
```

这些是更传统、更底层的推导方式。

但是你刚开始写代码，建议先用这个：

```text
反Clarke + 零序注入
```

它代码短，效果接近标准 SVPWM，很适合先把电机转起来。

---

# 14. 真正写工程时，SVPWM 函数放在哪里？

建议结构：

```text
motor_control.c
 ├── Clarke()
 ├── Park()
 ├── InvPark()
 ├── SVPWM_Output()
 ├── CurrentLoop()
 └── Motor_Control_Loop()
```

其中：

```text
SVPWM_Output()
```

只负责一件事：

```text
把 v_alpha/v_beta 转成三相CCR
```

不要把速度环、电流环、ADC采样都塞进去。

---

# 15. 最后一句话总结

STM32 只有普通 PWM 外设没问题，因为：

```text
SVPWM不是硬件外设
SVPWM是软件算法
它最终输出的仍然是三相PWM占空比
STM32定时器只负责根据CCR产生PWM波形
```

你可以这样记：

```text
SVPWM算法：算 duty_u / duty_v / duty_w
TIM1外设：根据 duty 输出真实PWM
三相驱动：把PWM变成电机三相电压
```

所以实现 SVPWM 的核心就是：

```c
__HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, ccr_u);
__HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, ccr_v);
__HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_3, ccr_w);
```

只要你能动态更新三路 PWM 占空比，STM32F407 就可以做 SVPWM。


-----
-------
-------

电机的**机械角度**，就是：

> **电机转子在真实物理世界里转过的角度。**

也就是你用眼睛看电机轴、轮子、磁钢转了多少度，这个角度就叫**机械角度**。

---

# 1. 最通俗理解

假设电机轴上画一条白线：

```text
       白线
        ↑
        |
   -----●-----   电机轴
```

刚开始白线朝上，定义为：

```text
机械角度 = 0°
```

电机轴转了四分之一圈：

```text
机械角度 = 90°
```

转了半圈：

```text
机械角度 = 180°
```

转了一整圈：

```text
机械角度 = 360°
```

所以：

```text
机械角度 = 电机转子真实转过的物理角度
```

---

# 2. 机械角度和机械转速的关系

如果电机轴一秒转一圈：

```text
1圈 = 360°机械角度
```

那么机械转速就是：

```text
1 rps = 60 rpm
```

也就是：

```text
每秒转 360°机械角度
每分钟转 60 圈
```

所以机械角度描述的是：

```text
转子转到哪里了
```

机械速度描述的是：

```text
转子转得多快
```

---

# 3. 机械角度和电角度不是一回事

做 FOC 的时候，经常会看到两个词：

```text
机械角度 mechanical angle
电角度 electrical angle
```

它们很容易混。

区别是：

```text
机械角度：电机轴真实转了多少度
电角度：电机磁场周期对应的角度
```

机械角度是物理转轴角度。

电角度是电机内部磁场、电流、反电动势用的角度。

---

# 4. 为什么会有电角度？

因为 BLDC/PMSM 电机的转子上有很多磁极。

比如一个常见无刷电机是：

```text
14极电机 = 7对极
```

一对极就是：

```text
一个N极 + 一个S极
```

如果电机有 7 对极，那么转子机械转一圈，电机内部的磁场周期会变化 7 圈。

所以：

```text
机械角度转 360°
电角度转 360° × 极对数
```

公式是：

```text
电角度 = 机械角度 × 极对数
```

---

# 5. 举个例子：7对极电机

假设你的 BLDC 电机是：

```text
7对极
```

那么：

```text
机械角度转 1 圈 = 360°
电角度转 7 圈 = 2520°
```

对应关系：

|   机械角度 |   电角度 |
| -----: | ----: |
|     0° |    0° |
|    10° |   70° |
|    30° |  210° |
| 51.43° |  360° |
|    90° |  630° |
|   180° | 1260° |
|   360° | 2520° |

但是电角度一般会取模到 0°~360°。

所以机械角度每转：

```text
360° / 7 = 51.43°
```

电角度就已经完成一圈。

---

# 6. 用公式表示

如果用角度制：

```text
θe = θm × P
```

其中：

```text
θe = 电角度
θm = 机械角度
P  = 极对数
```

如果用弧度制：

```text
θe = θm × P
```

形式还是一样。

例如：

```c
float theta_e = theta_m * pole_pairs;
```

然后通常要归一化：

```c
while (theta_e >= 2.0f * PI)
{
    theta_e -= 2.0f * PI;
}
```

---

# 7. 机械速度和电角速度的关系

角度有机械角度和电角度。

速度也有机械速度和电角速度。

公式类似：

```text
电角速度 = 机械角速度 × 极对数
```

例如电机机械转速：

```text
机械转速 = 1000 rpm
极对数 = 7
```

那么电角速度对应：

```text
电角频率 = 1000 × 7 = 7000 电rpm
```

意思是：

```text
转子机械每分钟转1000圈
电磁周期每分钟变化7000圈
```

---

# 8. FOC 里到底用哪个角度？

FOC 里面做 Park/反 Park 变换时，用的是：

```text
电角度
```

不是机械角度。

因为电机电流和磁场是按电磁周期变化的。

FOC 代码里常见：

```c
Park_Transform(i_alpha, i_beta, theta_e, &id, &iq);
InvPark_Transform(vd, vq, theta_e, &v_alpha, &v_beta);
```

这里的 `theta_e` 是电角度。

不是机械角度。

---

# 9. 编码器测到的一般是什么角度？

编码器装在电机轴上，所以编码器直接测到的是：

```text
机械角度
```

例如磁编码器 AS5600 输出 0~360°，这通常表示：

```text
电机轴机械角度 0~360°
```

然后 FOC 要用电角度，就需要转换：

```c
theta_e = theta_m * pole_pairs + offset;
```

其中 `offset` 是电角度零点偏移。

实际代码一般类似：

```c
float theta_m = Encoder_GetMechanicalAngle();   // 机械角度
float theta_e = theta_m * pole_pairs + zero_offset;
theta_e = normalize_angle(theta_e);             // 转成 0~2π
```

---

# 10. 为什么还要 zero_offset？

因为编码器的 0° 不一定等于电机 d 轴的 0°。

比如编码器上电读到：

```text
机械角度 = 0°
```

但此时转子磁极不一定刚好对准你定义的 U 相磁场方向。

所以 FOC 里要做一个校准：

```text
编码器机械零点
和
电机电角度零点
对齐
```

这个偏差就是：

```text
zero_offset
```

所以真实使用时：

```text
电角度 = 机械角度 × 极对数 + 零点偏移
```

---

# 11. 用生活例子理解

你可以把机械角度理解成：

```text
车轮真实转了多少度
```

把电角度理解成：

```text
电机内部磁场周期走到了哪里
```

如果这个电机只有 1 对极：

```text
机械转一圈
电角度也转一圈
```

如果这个电机有 7 对极：

```text
机械转一圈
电角度转 7 圈
```

所以极对数越多，电角度变化越快。

---

# 12. 六步换向里也有机械角度和电角度吗？

有。

六步换向通常每：

```text
60°电角度
```

换一次相。

注意是**电角度 60°**，不是机械角度 60°。

对于 7 对极电机：

```text
60°电角度 = 60° / 7 = 8.57°机械角度
```

所以转子机械只转了 8.57°，六步换向就要切一次。

---

# 13. FOC 开环代码里的 current_rpm 是机械转速吗？

一般我们说的电机转速 rpm，默认是：

```text
机械转速
```

比如：

```c
motor1.target_rpm = 300.0f;
```

通常表示：

```text
电机轴机械转速 = 300 rpm
```

然后代码里要换算成电角速度：

```c
mech_rps = current_rpm / 60.0f;

elec_rad_per_s = mech_rps * pole_pairs * 2.0f * PI;
```

意思是：

```text
机械每秒多少圈
× 极对数
= 电角每秒多少圈
```

---

# 14. 最容易记错的地方

不要把这两个搞反：

```text
机械角度：转子轴真实位置
电角度：电机磁场位置
```

FOC 里面：

```text
编码器读出来：机械角度
Park变换用：电角度
```

转换关系：

```text
电角度 = 机械角度 × 极对数 + 零点偏移
```

---

# 15. 一句话总结

**机械角度**就是电机轴真实转过的物理角度。

比如轴转了一圈：

```text
机械角度 = 360°
```

但是如果电机是 7 对极，那么电磁周期已经转了 7 圈：

```text
电角度 = 360° × 7 = 2520°
```

FOC 控制最终用的是**电角度**，因为它要控制电机内部磁场；编码器直接测到的一般是**机械角度**，需要乘以极对数再加零点偏移，才能变成 FOC 需要的电角度。


---
----
---

你这里应该是想说：**讲解下 SVPWM 的来源历史、具体实现方法，用 STM32 伪代码，并和 SPWM 优缺点对比。**

下面我按嵌入式电机控制的角度讲。

---

# 1. SVPWM 是怎么来的？

早期三相电机控制里，最直观的方法是 **SPWM**。

SPWM 全名：

```text
Sinusoidal PWM
正弦脉宽调制
```

它的想法很简单：

> 我想让三相电机得到三相正弦电压，那我就分别生成三路相差 120° 的正弦 PWM。

也就是：

```c
Ua = sin(theta);
Ub = sin(theta - 120°);
Uc = sin(theta + 120°);
```

然后把这三个正弦值变成 PWM 占空比。

这个方法简单、直观，所以早期变频器、电机驱动里用得很多。

---

但是后来工程师发现一个问题：

> 三相逆变桥本质上不是“连续电压源”，而是由 6 个 MOS/IGBT 组成的开关网络。

三相逆变桥每一相只有两种状态：

```text
上桥导通：相线接到 VBUS+
下桥导通：相线接到 GND
```

三相一共就是：

```text
U相状态：0 / 1
V相状态：0 / 1
W相状态：0 / 1
```

所以总共有：

```text
2 × 2 × 2 = 8 种开关状态
```

也就是：

```text
000
001
010
011
100
101
110
111
```

后来人们把这 8 种开关状态映射到二维 αβ 坐标平面，发现：

```text
6 个有效矢量形成一个六边形
2 个零矢量在中心点
```

这个思路就发展成了 **SVPWM**。

SVPWM 全名：

```text
Space Vector PWM
空间矢量脉宽调制
```

它的核心不是“分别看三相正弦波”，而是：

> 把三相逆变桥整体看成一个可以产生空间电压矢量的系统。

---

# 2. SPWM 和 SVPWM 的思想区别

## SPWM 的思路

SPWM 是从“三相波形”角度看问题：

```text
我要 U/V/W 三相都是正弦波
所以我分别生成三路正弦 PWM
```

结构是：

```text
正弦表 / sin计算
        ↓
Ua、Ub、Uc
        ↓
转换成 duty_u、duty_v、duty_w
        ↓
写 TIMx_CCR1/2/3
```

---

## SVPWM 的思路

SVPWM 是从“逆变桥开关矢量”角度看问题：

```text
三相逆变桥只有 8 种开关状态
这些状态在 αβ 平面里对应 8 个空间矢量
我要的目标矢量可以用相邻两个有效矢量 + 零矢量平均合成
```

结构是：

```text
FOC算出的 Vα、Vβ
        ↓
判断目标矢量在哪个扇区
        ↓
计算 T1、T2、T0
        ↓
分配三相PWM占空比
        ↓
写 TIMx_CCR1/2/3
```

---

# 3. 为什么 SVPWM 更适合 FOC？

FOC 里面经过反 Park 变换后，得到的是：

```text
Vα、Vβ
```

也就是一个二维平面里的电压矢量。

而 SVPWM 本来就是处理这个东西的：

```text
输入：Vα、Vβ
输出：三相PWM占空比
```

所以 FOC 和 SVPWM 天然很配。

FOC 链路是：

```text
Id/Iq电流控制
        ↓
得到 Vd/Vq
        ↓
反Park变换
        ↓
得到 Vα/Vβ
        ↓
SVPWM
        ↓
TIM1三相PWM
        ↓
三相逆变桥
        ↓
BLDC/PMSM
```

一句话：

> **FOC 负责算电机需要什么电压矢量，SVPWM 负责把这个电压矢量变成三相桥臂 PWM。**

---

# 4. 三相逆变桥的 8 种状态

假设：

```text
1 = 上桥导通，下桥关闭
0 = 下桥导通，上桥关闭
```

那么三相桥状态可以写成：

```text
U V W
```

比如：

```text
100
```

表示：

```text
U相接 VBUS+
V相接 GND
W相接 GND
```

8 种状态是：

| 状态  | 类型      |
| --- | ------- |
| 000 | 零矢量     |
| 111 | 零矢量     |
| 100 | 有效矢量 V1 |
| 110 | 有效矢量 V2 |
| 010 | 有效矢量 V3 |
| 011 | 有效矢量 V4 |
| 001 | 有效矢量 V5 |
| 101 | 有效矢量 V6 |

6 个有效矢量围成一个六边形：

```text
              V2(110)
             /      \
            /        \
     V3(010)          V1(100)
          |            |
          |     O      |
     V4(011)          V6(101)
            \        /
             \      /
              V5(001)
```

中心 O 是零矢量：

```text
000 或 111
```

---

# 5. SVPWM 的核心原理

假设目标电压矢量在第一扇区：

```text
位于 V1 和 V2 之间
```

那么一个 PWM 周期 `Ts` 里，可以这样分配：

```text
T1 时间输出 V1
T2 时间输出 V2
T0 时间输出零矢量
```

满足：

```text
T1 + T2 + T0 = Ts
```

这样一个周期平均下来，就等效得到目标矢量。

类似这样：

```text
目标矢量 Vref = V1 作用一段时间 + V2 作用一段时间 + 零矢量补时间
```

这就是 SVPWM 的本质。

---

# 6. 传统扇区法 SVPWM 实现步骤

传统 SVPWM 一般按下面流程：

```text
1. 输入 Vα、Vβ
2. 计算目标矢量角度 angle
3. 判断 angle 在哪个扇区
4. 根据扇区计算 T1、T2、T0
5. 根据七段式PWM分配 Ta、Tb、Tc
6. 转成 CCR1、CCR2、CCR3
7. STM32 TIM1 输出三相互补PWM
```

---

# 7. STM32 上怎么实现 SVPWM？

STM32 没有 SVPWM 外设，只有 PWM 定时器。

所以你要做的是：

```text
软件算出 duty_u、duty_v、duty_w
        ↓
写入 TIM1->CCR1
写入 TIM1->CCR2
写入 TIM1->CCR3
        ↓
TIM1 输出 PWM
```

也就是：

```c
TIM1->CCR1 = ccr_u;
TIM1->CCR2 = ccr_v;
TIM1->CCR3 = ccr_w;
```

STM32 的 TIM1/TIM8 很适合做三相电机控制，因为它们支持：

```text
互补PWM
死区时间
刹车输入
中心对齐PWM
更新事件
```

---

# 8. 方法一：传统扇区法 SVPWM 伪代码

这个方法更接近教材。

## 8.1 伪代码结构

```c
void SVPWM_Sector_Method(float v_alpha, float v_beta)
{
    // 1. 计算目标矢量角度
    angle = atan2f(v_beta, v_alpha);

    // 2. 角度归一化到 0 ~ 2π
    if (angle < 0)
    {
        angle += 2.0f * PI;
    }

    // 3. 判断扇区
    sector = (int)(angle / (PI / 3.0f)) + 1;

    // sector 范围限制到 1~6
    if (sector > 6)
    {
        sector = 6;
    }

    // 4. 计算目标矢量幅值
    v_ref = sqrtf(v_alpha * v_alpha + v_beta * v_beta);

    // 5. 计算扇区内角度
    angle_in_sector = angle - (sector - 1) * (PI / 3.0f);

    // 6. 计算 T1、T2、T0
    T1 = K * sinf(PI / 3.0f - angle_in_sector);
    T2 = K * sinf(angle_in_sector);
    T0 = Ts - T1 - T2;

    // 7. 根据扇区计算三相作用时间
    switch (sector)
    {
        case 1:
            Ta = T1 + T2 + T0 / 2;
            Tb = T2 + T0 / 2;
            Tc = T0 / 2;
            break;

        case 2:
            Ta = T1 + T0 / 2;
            Tb = T1 + T2 + T0 / 2;
            Tc = T0 / 2;
            break;

        case 3:
            Ta = T0 / 2;
            Tb = T1 + T2 + T0 / 2;
            Tc = T2 + T0 / 2;
            break;

        case 4:
            Ta = T0 / 2;
            Tb = T1 + T0 / 2;
            Tc = T1 + T2 + T0 / 2;
            break;

        case 5:
            Ta = T2 + T0 / 2;
            Tb = T0 / 2;
            Tc = T1 + T2 + T0 / 2;
            break;

        case 6:
            Ta = T1 + T2 + T0 / 2;
            Tb = T0 / 2;
            Tc = T1 + T0 / 2;
            break;
    }

    // 8. 转成占空比
    duty_u = Ta / Ts;
    duty_v = Tb / Ts;
    duty_w = Tc / Ts;

    // 9. 转成 CCR
    TIM1->CCR1 = duty_u * PWM_PERIOD;
    TIM1->CCR2 = duty_v * PWM_PERIOD;
    TIM1->CCR3 = duty_w * PWM_PERIOD;
}
```

---

## 8.2 里面的 K 是什么？

`K` 用来把电压幅值换成时间。

可以粗略理解为：

```c
K = sqrt(3) * Ts * v_ref / vbus;
```

所以：

```c
T1 = sqrt(3) * Ts * v_ref / vbus * sin(60° - angle_in_sector);
T2 = sqrt(3) * Ts * v_ref / vbus * sin(angle_in_sector);
```

伪代码：

```c
k = 1.7320508f * Ts * v_ref / vbus;

T1 = k * sinf(PI / 3.0f - angle_in_sector);
T2 = k * sinf(angle_in_sector);
T0 = Ts - T1 - T2;
```

注意：

```text
如果 T0 < 0，说明电压给大了，已经过调制了，需要限幅。
```

---

# 9. 方法二：零序注入法 SVPWM，工程里更简单

刚开始学习，我更建议你先用这个。

它不需要判断扇区，不需要算 T1/T2/T0，代码更短。

核心思想：

> 先把 Vα/Vβ 转成三相电压 Va/Vb/Vc，然后给三相一起注入一个公共偏置，让 PWM 更充分利用母线电压。

---

## 9.1 零序注入 SVPWM 伪代码

```c
void SVPWM_ZeroSequence_Method(float v_alpha, float v_beta)
{
    float va;
    float vb;
    float vc;

    float v_max;
    float v_min;
    float v_offset;

    float duty_u;
    float duty_v;
    float duty_w;

    uint32_t ccr_u;
    uint32_t ccr_v;
    uint32_t ccr_w;

    /*
     * 1. alpha-beta 转三相电压
     */
    va = v_alpha;

    vb = -0.5f * v_alpha + 0.8660254f * v_beta;

    vc = -0.5f * v_alpha - 0.8660254f * v_beta;

    /*
     * 2. 找最大值
     */
    v_max = va;

    if (vb > v_max)
    {
        v_max = vb;
    }

    if (vc > v_max)
    {
        v_max = vc;
    }

    /*
     * 3. 找最小值
     */
    v_min = va;

    if (vb < v_min)
    {
        v_min = vb;
    }

    if (vc < v_min)
    {
        v_min = vc;
    }

    /*
     * 4. 计算零序偏置
     */
    v_offset = 0.5f * (v_max + v_min);

    /*
     * 5. 注入零序分量
     */
    va = va - v_offset;
    vb = vb - v_offset;
    vc = vc - v_offset;

    /*
     * 6. 转成占空比
     *
     * 当相电压为0时，占空比是50%
     */
    duty_u = 0.5f + va / vbus;
    duty_v = 0.5f + vb / vbus;
    duty_w = 0.5f + vc / vbus;

    /*
     * 7. 限幅，避免0%/100%
     */
    duty_u = limit(duty_u, 0.02f, 0.98f);
    duty_v = limit(duty_v, 0.02f, 0.98f);
    duty_w = limit(duty_w, 0.02f, 0.98f);

    /*
     * 8. 转成定时器CCR
     */
    ccr_u = duty_u * PWM_PERIOD;
    ccr_v = duty_v * PWM_PERIOD;
    ccr_w = duty_w * PWM_PERIOD;

    /*
     * 9. 写入 STM32 TIM1 比较寄存器
     */
    TIM1->CCR1 = ccr_u;
    TIM1->CCR2 = ccr_v;
    TIM1->CCR3 = ccr_w;
}
```

这个就是你最容易落地的 SVPWM。

---

# 10. 为什么三相一起加偏置不会影响电机？

因为电机主要看的是**线电压**。

三相相电压是：

```text
Va、Vb、Vc
```

线电压是：

```text
Vab = Va - Vb
Vbc = Vb - Vc
Vca = Vc - Va
```

如果三相一起加一个偏置 `offset`：

```text
Va' = Va + offset
Vb' = Vb + offset
Vc' = Vc + offset
```

线电压变成：

```text
Vab' = Va' - Vb'
     = (Va + offset) - (Vb + offset)
     = Va - Vb
```

所以：

```text
三相共同偏移不会改变线电压
```

但它可以让三相占空比更靠近 0~100% 的可用范围，从而提高母线利用率。

这就是 SVPWM 比 SPWM 电压利用率高的原因。

---

# 11. STM32 F407 HAL 风格伪代码

假设：

```c
TIM_HandleTypeDef htim1;
```

PWM 三相：

```text
TIM1_CH1 -> U相
TIM1_CH2 -> V相
TIM1_CH3 -> W相
```

互补输出：

```text
TIM1_CH1N -> U相下桥
TIM1_CH2N -> V相下桥
TIM1_CH3N -> W相下桥
```

---

## 11.1 初始化启动 PWM

```c
void Motor_PWM_Start(void)
{
    /*
     * 启动三相高边PWM
     */
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_3);

    /*
     * 启动三相低边互补PWM
     */
    HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_1);
    HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_2);
    HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_3);

    /*
     * 初始输出50%占空比
     */
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, PWM_PERIOD / 2);
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, PWM_PERIOD / 2);
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_3, PWM_PERIOD / 2);
}
```

---

## 11.2 控制中断

比如用 TIM6 10kHz 中断跑控制环：

```c
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM6)
    {
        Motor_Control_Loop();
    }
}
```

---

## 11.3 开环 FOC + SVPWM 伪代码

```c
#define PI          3.1415926f
#define TWO_PI      6.2831853f
#define VBUS        12.0f
#define PWM_PERIOD  4200.0f
#define TS          0.0001f      // 10kHz控制周期

float theta_e = 0.0f;            // 电角度
float target_rpm = 300.0f;       // 机械转速
float pole_pairs = 7.0f;         // 极对数
float vq = 1.0f;                 // q轴电压
float vd = 0.0f;                 // d轴电压

void Motor_Control_Loop(void)
{
    float mech_rps;
    float elec_rad_per_sec;

    float sin_t;
    float cos_t;

    float v_alpha;
    float v_beta;

    /*
     * 1. 机械转速 rpm -> 机械 rps
     */
    mech_rps = target_rpm / 60.0f;

    /*
     * 2. 机械速度 -> 电角速度
     */
    elec_rad_per_sec = mech_rps * pole_pairs * TWO_PI;

    /*
     * 3. 积分得到电角度
     */
    theta_e += elec_rad_per_sec * TS;

    if (theta_e >= TWO_PI)
    {
        theta_e -= TWO_PI;
    }

    /*
     * 4. 反Park变换
     */
    sin_t = sinf(theta_e);
    cos_t = cosf(theta_e);

    v_alpha = vd * cos_t - vq * sin_t;
    v_beta  = vd * sin_t + vq * cos_t;

    /*
     * 5. SVPWM输出
     */
    SVPWM_ZeroSequence_Output(v_alpha, v_beta);
}
```

---

## 11.4 SVPWM 输出函数

```c
float limit_float(float x, float min, float max)
{
    if (x < min)
    {
        return min;
    }

    if (x > max)
    {
        return max;
    }

    return x;
}


void SVPWM_ZeroSequence_Output(float v_alpha, float v_beta)
{
    float va;
    float vb;
    float vc;

    float v_max;
    float v_min;
    float v_offset;

    float duty_u;
    float duty_v;
    float duty_w;

    uint32_t ccr_u;
    uint32_t ccr_v;
    uint32_t ccr_w;

    /*
     * alpha-beta 转三相
     */
    va = v_alpha;
    vb = -0.5f * v_alpha + 0.8660254f * v_beta;
    vc = -0.5f * v_alpha - 0.8660254f * v_beta;

    /*
     * 求最大值
     */
    v_max = va;

    if (vb > v_max)
    {
        v_max = vb;
    }

    if (vc > v_max)
    {
        v_max = vc;
    }

    /*
     * 求最小值
     */
    v_min = va;

    if (vb < v_min)
    {
        v_min = vb;
    }

    if (vc < v_min)
    {
        v_min = vc;
    }

    /*
     * 零序注入
     */
    v_offset = 0.5f * (v_max + v_min);

    va = va - v_offset;
    vb = vb - v_offset;
    vc = vc - v_offset;

    /*
     * 转占空比
     */
    duty_u = 0.5f + va / VBUS;
    duty_v = 0.5f + vb / VBUS;
    duty_w = 0.5f + vc / VBUS;

    /*
     * 限幅
     */
    duty_u = limit_float(duty_u, 0.02f, 0.98f);
    duty_v = limit_float(duty_v, 0.02f, 0.98f);
    duty_w = limit_float(duty_w, 0.02f, 0.98f);

    /*
     * 占空比转 CCR
     */
    ccr_u = (uint32_t)(duty_u * PWM_PERIOD);
    ccr_v = (uint32_t)(duty_v * PWM_PERIOD);
    ccr_w = (uint32_t)(duty_w * PWM_PERIOD);

    /*
     * 写入 TIM1 CCR
     */
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, ccr_u);
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, ccr_v);
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_3, ccr_w);
}
```

---

# 12. STM32 TIM1 推荐配置

对于 STM32F407，建议用 TIM1 或 TIM8。

推荐：

```text
PWM频率：16kHz ~ 25kHz，常用20kHz
计数模式：中心对齐 Center-aligned
通道：CH1/CH1N、CH2/CH2N、CH3/CH3N
死区：根据 MOS 和驱动芯片设置，常见 500ns ~ 1us
Break：建议开启，用于硬件过流保护
```

如果 TIM1 时钟是 168MHz，中心对齐 PWM，20kHz：

```text
ARR = 168MHz / (2 × 20kHz)
ARR = 4200
```

CubeMX 里大概：

```text
Prescaler = 0
Counter Mode = Center Aligned
Period = 4199
Pulse = 2100
PWM Mode = PWM mode 1
```

代码里：

```c
#define PWM_PERIOD 4200.0f
```

---

# 13. SPWM 实现伪代码

SPWM 比 SVPWM 更直观。

```c
void SPWM_Output(float theta)
{
    float ua;
    float ub;
    float uc;

    float duty_u;
    float duty_v;
    float duty_w;

    /*
     * 1. 生成三相正弦
     */
    ua = sinf(theta);
    ub = sinf(theta - 2.0f * PI / 3.0f);
    uc = sinf(theta + 2.0f * PI / 3.0f);

    /*
     * 2. 乘以调制系数
     * modulation 范围一般 0~1
     */
    ua = modulation * ua;
    ub = modulation * ub;
    uc = modulation * uc;

    /*
     * 3. 转占空比
     */
    duty_u = 0.5f + 0.5f * ua;
    duty_v = 0.5f + 0.5f * ub;
    duty_w = 0.5f + 0.5f * uc;

    /*
     * 4. 写CCR
     */
    TIM1->CCR1 = duty_u * PWM_PERIOD;
    TIM1->CCR2 = duty_v * PWM_PERIOD;
    TIM1->CCR3 = duty_w * PWM_PERIOD;
}
```

SPWM 就是直接生成三相正弦波。

---

# 14. SPWM 和 SVPWM 优缺点对比

| 对比项    | SPWM    | SVPWM           |
| ------ | ------- | --------------- |
| 思想     | 三相正弦波调制 | 空间矢量合成          |
| 实现难度   | 简单      | 稍复杂             |
| 是否适合新手 | 很适合     | 需要理解 αβ 矢量      |
| 母线利用率  | 较低      | 更高              |
| 输出电压能力 | 较弱      | 较强              |
| FOC适配度 | 可以用     | 更常用             |
| 谐波表现   | 一般      | 较好              |
| 转矩脉动   | 比六步小    | 通常更优            |
| 高速能力   | 稍弱      | 更好              |
| 算法复杂度  | 低       | 中等              |
| 工程应用   | 简单变频/学习 | FOC/PMSM/BLDC主流 |

---

# 15. 母线利用率差异

这是 SVPWM 最大优势之一。

简单说：

```text
SPWM 最大线电压基波利用率较低
SVPWM 比 SPWM 大约高 15%
```

举例：

假设母线电压是：

```text
VBUS = 24V
```

SPWM 可能在还没充分利用 24V 时就进入饱和了。

SVPWM 通过零序注入，把三相波形整体上下移动，让 PWM 范围用得更满。

结果就是：

```text
同样 24V 电源
SVPWM 能输出更高的有效线电压
电机高速时更不容易“没电压余量”
```

对于高速电机尤其重要。

---

# 16. SPWM 的优点

SPWM 最大好处是简单。

适合：

```text
学习三相PWM
简单开环调速
简单变频器
低成本电机驱动
对性能要求不高的场合
```

优点：

```text
1. 原理直观
2. 代码简单
3. 容易调试
4. 不需要理解扇区
5. 很适合刚开始学习
```

缺点：

```text
1. 母线电压利用率低
2. 高速输出能力差一些
3. 不如 SVPWM 适合现代 FOC
4. 同样母线下最大输出电压较小
```

---

# 17. SVPWM 的优点

SVPWM 适合工程化电机控制。

优点：

```text
1. 母线利用率高
2. 输出电压能力强
3. 更适合 FOC 的 Vα/Vβ 输出
4. 转矩脉动小
5. 三相桥开关状态利用更合理
6. 高速性能更好
7. 是 PMSM/BLDC FOC 常用调制方式
```

缺点：

```text
1. 理论比 SPWM 难
2. 传统扇区法代码更复杂
3. 需要注意过调制和限幅
4. 对 ADC 采样时刻、死区、PWM同步要求更高
```

---

# 18. 六步换向、SPWM、SVPWM、FOC 的关系

你可以这样理解：

```text
六步换向：
    最粗糙，60°电角度换一次相，简单、鲁棒、噪声大。

SPWM：
    三相正弦PWM，波形平滑，简单，但母线利用率较低。

SVPWM：
    从三相桥空间矢量角度合成电压，母线利用率高，适合FOC。

FOC：
    是控制算法，控制磁场方向和力矩。
    FOC最后可以接 SPWM，也可以接 SVPWM。
```

完整关系：

```text
FOC 是控制大脑
SVPWM/SPWM 是 PWM调制方式
TIM1 是硬件执行器
三相桥是功率输出级
电机是被控对象
```

---

# 19. 工程上我建议你怎么学？

你的学习路线可以这样：

```text
第一步：先用 TIM1 输出三相互补 PWM
第二步：写 SPWM，让电机空载正弦开环转起来
第三步：把 SPWM 换成零序注入 SVPWM
第四步：加 ADC 采样母线电流，做过流保护
第五步：加相电流采样，做 Clarke/Park
第六步：做 Id/Iq 电流 PI 闭环
第七步：加编码器，做有感 FOC
第八步：再研究无感观测器
```

不要一上来就搞完整无感 FOC，否则会被这些东西一起卡住：

```text
PWM
ADC采样
电流环
位置估算
坐标变换
SVPWM
死区补偿
过流保护
启动策略
```

---

# 20. 最后总结

SVPWM 的来源是：

```text
从三相逆变桥的 8 种开关状态出发，
把它们看成 αβ 平面里的空间电压矢量，
再用相邻两个有效矢量 + 零矢量，
在一个PWM周期内平均合成目标电压矢量。
```

STM32 上实现 SVPWM 不需要专用外设：

```text
软件算 duty_u、duty_v、duty_w
然后写 TIM1->CCR1/CCR2/CCR3
```

SPWM 和 SVPWM 的核心区别：

```text
SPWM：直接生成三相正弦波
SVPWM：从逆变桥空间矢量角度合成目标电压
```

工程上：

```text
初学：SPWM更容易理解
做FOC：SVPWM更常用
做高性能BLDC/PMSM：FOC + SVPWM 是主流组合
```


-----------
-----
----

说实话，你没听懂很正常。因为很多人讲 SVPWM 和 SPWM，一上来就讲“空间矢量、扇区、T1/T2/T0”，很容易抽象。

我换一种**非常接地气**的方式讲。

---

# 1. 先记住一句话

**SPWM 和 SVPWM 都是为了同一个目的：**

> 把三相逆变桥变成“可控的三相交流电源”，让 BLDC/PMSM 电机转起来。

它们最终做的事情都一样：

```text
算出 U 相占空比
算出 V 相占空比
算出 W 相占空比
然后写到 STM32 的 TIM1->CCR1/CCR2/CCR3
```

区别只是：

```text
SPWM：用“正弦波思路”算三相占空比
SVPWM：用“三相桥开关状态思路”算三相占空比
```

---

# 2. SPWM 是怎么想的？

SPWM 很好理解。

你想让三相电压是正弦波：

```text
U 相：sin(θ)
V 相：sin(θ - 120°)
W 相：sin(θ + 120°)
```

所以代码就是：

```c
u = sin(theta);
v = sin(theta - 2.0f * PI / 3.0f);
w = sin(theta + 2.0f * PI / 3.0f);

duty_u = 0.5f + 0.5f * u;
duty_v = 0.5f + 0.5f * v;
duty_w = 0.5f + 0.5f * w;
```

然后写 PWM：

```c
TIM1->CCR1 = duty_u * PWM_PERIOD;
TIM1->CCR2 = duty_v * PWM_PERIOD;
TIM1->CCR3 = duty_w * PWM_PERIOD;
```

所以 SPWM 的思路是：

> 我要三相正弦电压，所以我直接生成三相正弦占空比。

很直观。

---

# 3. SVPWM 是怎么想的？

SVPWM 不从“正弦波”出发。

它从“三相逆变桥到底能输出哪些状态”出发。

三相桥每一相只有两种状态：

```text
1：上管导通，这一相接电源正极
0：下管导通，这一相接地
```

三相 U/V/W 一组合，就有 8 种状态：

```text
000
001
010
011
100
101
110
111
```

SVPWM 的思路是：

> 我不直接生成三条正弦波。
> 我研究三相桥的 8 种开关状态，然后用这些开关状态在一个 PWM 周期里“拼”出我想要的电压矢量。

听起来复杂，但你可以先这样理解：

```text
SPWM：直接画三条正弦曲线
SVPWM：从三相桥能做到的开关组合里，聪明地安排导通时间
```

---

# 4. 最关键的区别：SVPWM 更会“榨干母线电压”

假设你的电池是：

```text
24V
```

你希望电机高速转。

电机高速时，需要更高的输出电压，因为电机反电动势变大了。

这时问题来了：

```text
SPWM 比较保守，还没把 24V 完全用满，就到极限了。
SVPWM 更聪明，可以把 24V 用得更充分。
```

所以经常说：

```text
SVPWM 母线电压利用率比 SPWM 高大约 15%
```

这句话换成人话就是：

> 同样 24V 电源，用 SVPWM，电机能获得更高的有效输出电压，所以高速能力更好。

---

# 5. 用一个水桶比喻

假设母线电压是一桶水：

```text
电池 24V = 一桶水
```

SPWM 像是用一个普通勺子舀水：

```text
能舀，但舀不干净，还剩一点用不上
```

SVPWM 像是换了一个更贴合桶底的勺子：

```text
还是同一桶水，但能多舀出一点
```

所以不是 SVPWM 让电池变成 28V 了，而是：

```text
同样 24V，SVPWM 用得更充分
```

---

# 6. 为什么 SVPWM 能多用一点电压？

这个地方是重点。

SPWM 生成三相正弦波时，每一相都要限制在 PWM 的 0%~100% 之间。

假设某一时刻三相正弦是：

```text
U = 0.9
V = -0.2
W = -0.7
```

SPWM 就老老实实按这个来。

但是 SVPWM 会发现：

> 电机真正关心的是 U-V、V-W、W-U 这些线电压，不是单独某一相相对地的电压。

如果三相一起加一个相同的偏移量，线电压不会变。

比如三相都加 0.1：

```text
U' = U + 0.1
V' = V + 0.1
W' = W + 0.1
```

那么：

```text
U' - V' = (U + 0.1) - (V + 0.1) = U - V
```

偏移量抵消了。

所以 SVPWM 会做一件聪明的事：

> 把三相波形整体上下挪一挪，让它们更充分利用 0%~100% 的 PWM 范围。

这就是**零序注入**。

---

# 7. SPWM 和 SVPWM 输出波形看起来有什么区别？

SPWM 的三相占空比比较像标准正弦：

```text
U 相：标准正弦
V 相：标准正弦，滞后 120°
W 相：标准正弦，超前 120°
```

SVPWM 的每一相占空比看起来**不是完美正弦**，会有点“压扁/变形”。

但是注意：

> 单独看每一相，SVPWM 不是标准正弦；
> 但是电机真正关心的线电压和合成磁场，效果更适合电机驱动。

这也是很多新手容易懵的地方。

你可能会想：

```text
SVPWM 每相都不是标准正弦，那为啥更好？
```

原因是：

```text
电机主要看线电压和三相合成磁场，不是看某一相对地电压是否完美正弦。
```

---

# 8. 用代码对比，最容易懂

## SPWM

```c
ua = sinf(theta);
ub = sinf(theta - 2.0f * PI / 3.0f);
uc = sinf(theta + 2.0f * PI / 3.0f);

duty_u = 0.5f + 0.5f * modulation * ua;
duty_v = 0.5f + 0.5f * modulation * ub;
duty_w = 0.5f + 0.5f * modulation * uc;
```

特点：

```text
直接三相正弦
简单
好理解
```

---

## SVPWM，零序注入版

先算出三相原始电压：

```c
va = v_alpha;
vb = -0.5f * v_alpha + 0.866f * v_beta;
vc = -0.5f * v_alpha - 0.866f * v_beta;
```

然后找最大最小值：

```c
vmax = max(va, vb, vc);
vmin = min(va, vb, vc);
```

计算偏移：

```c
offset = 0.5f * (vmax + vmin);
```

三相一起减去这个偏移：

```c
va = va - offset;
vb = vb - offset;
vc = vc - offset;
```

再变占空比：

```c
duty_u = 0.5f + va / vbus;
duty_v = 0.5f + vb / vbus;
duty_w = 0.5f + vc / vbus;
```

特点：

```text
三相一起挪位置
线电压不变
占空比范围用得更充分
母线利用率更高
```

---

# 9. 最直观优缺点对比

## SPWM 优点

```text
1. 最好理解
2. 代码最简单
3. 适合入门
4. 适合简单开环调速
5. 调试方便
```

比如你刚开始学三相电机，先用 SPWM 很合适。

---

## SPWM 缺点

```text
1. 母线电压利用率低
2. 高速时电压不够用
3. 同样电池电压下，最高转速可能低一点
4. 工程 FOC 里不如 SVPWM 常用
```

说人话就是：

> SPWM 简单，但有点浪费电池电压。

---

## SVPWM 优点

```text
1. 母线电压利用率更高
2. 同样电池电压下，电机高速能力更好
3. 更适合 FOC，因为 FOC 本来输出的就是 Vα/Vβ 电压矢量
4. 工程上 PMSM/BLDC FOC 常用
5. 三相桥开关状态利用更合理
```

说人话就是：

> SVPWM 更会用三相桥，更能榨干母线电压，更适合实际电机控制。

---

## SVPWM 缺点

```text
1. 理论比 SPWM 难
2. 初学者不如 SPWM 好理解
3. 传统扇区法代码更复杂
4. 调试时要注意限幅、死区、过调制
```

说人话就是：

> SVPWM 更强，但理解门槛高一点。

---

# 10. 什么时候用 SPWM？什么时候用 SVPWM？

## 刚开始学习

用 SPWM。

原因：

```text
好理解
代码短
波形直观
适合先让电机转起来
```

你可以先做：

```text
STM32 TIM1 三相 PWM
        ↓
SPWM 三相正弦
        ↓
电机空载旋转
```

---

## 做 FOC

建议用 SVPWM。

原因：

```text
FOC 算出来的是 Vα/Vβ
SVPWM 正好吃 Vα/Vβ
母线利用率更高
工程上更常见
```

FOC 里通常是：

```text
Vd/Vq
  ↓ 反Park
Vα/Vβ
  ↓ SVPWM
三相PWM占空比
```

---

## 做简单风扇/水泵

SPWM 可以。

尤其是：

```text
低成本
低性能要求
轻载
不追求极限高速
```

---

## 做平衡车/无人机/伺服/高性能电机

优先 SVPWM。

因为这些场景需要：

```text
更高电压利用率
更好动态性能
更平滑转矩
更适合电流闭环
```

---

# 11. 你可以这样记

最简单版：

```text
SPWM：
    我直接生成三条正弦波。
    优点：简单。
    缺点：电压利用率低。

SVPWM：
    我根据三相桥的开关状态，聪明地合成目标电压。
    优点：电压利用率高，更适合FOC。
    缺点：理论复杂。
```

再口语一点：

```text
SPWM = 老实巴交画正弦波
SVPWM = 更聪明地安排三相桥导通时间
```

---

# 12. 一个非常重要的结论

你不要以为：

```text
SPWM 是错的
SVPWM 才是对的
```

不是。

它们都能驱动电机。

区别是：

```text
SPWM：简单、直观、够用
SVPWM：更高效、更工程化、更适合FOC
```

所以你的学习顺序最好是：

```text
先懂 SPWM
再懂 SVPWM
最后再懂 FOC + SVPWM
```

---

# 13. 最终一句话总结

**SPWM 和 SVPWM 的本质区别：**

> SPWM 是“直接生成三相正弦 PWM”；
> SVPWM 是“从三相逆变桥的开关状态出发，更高效地合成同样的旋转电压矢量”。

**优缺点一句话：**

> SPWM 简单好懂，但母线电压利用率低；SVPWM 难一点，但能把电池电压用得更充分，所以更适合 FOC 和高性能 BLDC/PMSM 控制。


-----------

----
---
好，这次不单独讲 SVPWM / SPWM，而是**放到 FOC 整条链路里讲**。

你先记住一句话：

> **FOC 是控制电机磁场方向的算法；SPWM/SVPWM 是把 FOC 算出来的电压，变成三相 PWM 的方法。**

也就是说：

```text
FOC：算我要给电机什么电压矢量
SPWM/SVPWM：把这个电压矢量变成 U/V/W 三相 PWM 占空比
```

---

# 1. FOC 到底在干啥？

BLDC/PMSM 电机想转起来，本质是：

```text
定子产生一个旋转磁场
转子磁铁跟着这个旋转磁场转
```

FOC 的目标是：

> 让定子磁场始终和转子磁场保持一个合适角度，这样电机出力最大、效率高、转矩平滑。

FOC 会把电流分成两个方向：

```text
Id：励磁方向
Iq：出力矩方向
```

简单理解：

```text
Id：控制磁场强弱，一般设为 0
Iq：控制电机力矩，Iq 越大，电机越有劲
```

所以 FOC 最核心就是控制：

```text
Id ≈ 0
Iq = 目标力矩电流
```

---

# 2. FOC 的完整流程

标准 FOC 大概是这样：

```text
三相电流采样 ia、ib、ic
        ↓
Clarke 变换
        ↓
得到 iα、iβ
        ↓
Park 变换，使用转子电角度 θe
        ↓
得到 Id、Iq
        ↓
Id/Iq 和目标值比较
        ↓
电流 PI 控制器
        ↓
输出 Vd、Vq
        ↓
反 Park 变换
        ↓
得到 Vα、Vβ
        ↓
SPWM 或 SVPWM
        ↓
得到 U/V/W 三相 PWM 占空比
        ↓
TIM1 输出 PWM
        ↓
三相桥驱动电机
```

注意最后这一步：

```text
Vα、Vβ → 三相 PWM
```

这一步才是 SPWM/SVPWM 干的活。

---

# 3. FOC 和 SPWM/SVPWM 的位置关系

你可以把 FOC 分成两段：

```text
前半段：控制算法
后半段：PWM调制算法
```

具体是：

```text
FOC前半段：
    电流采样
    Clarke变换
    Park变换
    PI电流环
    反Park变换

FOC最后输出：
    Vα、Vβ

调制算法：
    SPWM 或 SVPWM

最终结果：
    duty_u、duty_v、duty_w
```

所以关系是：

```text
FOC 不直接输出 PWM
FOC 输出的是 Vα、Vβ 这个电压矢量
SPWM/SVPWM 把 Vα、Vβ 翻译成 PWM
```

---

# 4. 重点：FOC 算出来的 Vα、Vβ 是啥？

FOC 算完电流环后，会得到：

```text
Vd：d轴电压
Vq：q轴电压
```

然后通过反 Park 变换：

```c
v_alpha = vd * cos(theta_e) - vq * sin(theta_e);
v_beta  = vd * sin(theta_e) + vq * cos(theta_e);
```

得到：

```text
Vα、Vβ
```

这个 `Vα、Vβ` 可以理解成：

> 在电机定子平面上，我希望输出的那个电压箭头。

比如：

```text
          β轴
           ↑
           |
           |       Vref
           |        ↗
           |      /
           |    /
-----------+------------→ α轴
```

这个箭头有两个信息：

```text
方向：电压磁场朝哪里
大小：输出电压有多强
```

FOC 算到这里以后，问题来了：

> STM32 的 TIM1 只能输出三路 PWM，占空比是 CCR1/CCR2/CCR3。
> 那 Vα、Vβ 怎么变成 CCR1/CCR2/CCR3？

这就是 SPWM/SVPWM 的作用。

---

# 5. 如果 FOC 后面接 SPWM

FOC 输出：

```text
Vα、Vβ
```

你可以先把它变成三相电压：

```c
va = v_alpha;
vb = -0.5f * v_alpha + 0.866f * v_beta;
vc = -0.5f * v_alpha - 0.866f * v_beta;
```

然后直接映射成三相占空比：

```c
duty_u = 0.5f + va / vbus;
duty_v = 0.5f + vb / vbus;
duty_w = 0.5f + vc / vbus;
```

这就相当于 **FOC + SPWM**。

它的思路是：

```text
FOC算出一个旋转电压矢量
        ↓
反推出三相正弦电压
        ↓
直接生成三相正弦PWM
```

优点：

```text
简单
直观
容易写
容易调试
```

缺点：

```text
母线电压利用率低一点
高速时电压余量差一点
```

---

# 6. 如果 FOC 后面接 SVPWM

FOC 同样先输出：

```text
Vα、Vβ
```

也先变成三相电压：

```c
va = v_alpha;
vb = -0.5f * v_alpha + 0.866f * v_beta;
vc = -0.5f * v_alpha - 0.866f * v_beta;
```

但是 SVPWM 不会直接用这三个值，而是会加一步：

```c
vmax = max(va, vb, vc);
vmin = min(va, vb, vc);

offset = 0.5f * (vmax + vmin);

va = va - offset;
vb = vb - offset;
vc = vc - offset;
```

然后再算占空比：

```c
duty_u = 0.5f + va / vbus;
duty_v = 0.5f + vb / vbus;
duty_w = 0.5f + vc / vbus;
```

这就是**零序注入 SVPWM**。

它的核心是：

> 三相一起上下平移一下，不改变电机线电压，但能更充分利用 PWM 范围。

---

# 7. FOC + SPWM 和 FOC + SVPWM 的本质区别

它们前面的 FOC 部分基本一样：

```text
采样电流
Clarke
Park
PI控制
反Park
得到 Vα、Vβ
```

区别只在最后一步：

```text
Vα、Vβ → 三相PWM
```

也就是：

```text
FOC + SPWM：
    直接把 Vα/Vβ 转成三相正弦占空比

FOC + SVPWM：
    把 Vα/Vβ 转成三相占空比时，额外注入零序分量，提高母线利用率
```

所以你可以这样理解：

```text
FOC 是前面的控制逻辑
SPWM/SVPWM 是最后的输出翻译方式
```

---

# 8. 为什么 FOC 更常配 SVPWM？

因为 FOC 最后天然输出的是一个**空间电压矢量**：

```text
Vα、Vβ
```

而 SVPWM 本身就是：

```text
空间矢量 PWM
```

它就是专门处理这种二维电压矢量的。

所以：

```text
FOC 输出空间电压矢量
SVPWM 合成空间电压矢量
```

两者非常匹配。

---

# 9. 用“人话”解释 FOC + SPWM

FOC 说：

```text
我现在需要一个朝 30° 方向、大小为 5V 的电压磁场
```

SPWM 说：

```text
好，我直接算三相正弦：
U相多少
V相多少
W相多少
然后输出PWM
```

这很老实。

但是它比较保守，电池电压没有完全用满。

---

# 10. 用“人话”解释 FOC + SVPWM

FOC 也说：

```text
我现在需要一个朝 30° 方向、大小为 5V 的电压磁场
```

SVPWM 说：

```text
好，我看看三相桥现在怎么组合最合适。
我不只是机械地输出三相正弦。
我会把三相PWM整体挪一挪，
保证电机真正看到的线电压不变，
但让PWM范围用得更满。
```

所以 SVPWM 更“聪明”。

---

# 11. 举个具体数值例子

假设 FOC 算出来某一时刻三相原始电压是：

```text
Va = 5V
Vb = -1V
Vc = -4V
```

如果用 SPWM，就直接转占空比。

假设母线电压是 12V：

```text
duty_u = 0.5 + 5/12  = 91.7%
duty_v = 0.5 - 1/12  = 41.7%
duty_w = 0.5 - 4/12  = 16.7%
```

这还没超。

但是如果电压再大一点，比如：

```text
Va = 6.5V
Vb = -1.5V
Vc = -5.0V
```

SPWM：

```text
duty_u = 0.5 + 6.5/12 = 104.2%
```

超过 100% 了。

这时 SPWM 饱和了。

---

SVPWM 怎么做？

先找最大最小：

```text
vmax = 6.5V
vmin = -5.0V
offset = (6.5 + -5.0) / 2 = 0.75V
```

三相一起减去 offset：

```text
Va' = 6.5 - 0.75 = 5.75V
Vb' = -1.5 - 0.75 = -2.25V
Vc' = -5.0 - 0.75 = -5.75V
```

再算占空比：

```text
duty_u = 0.5 + 5.75/12  = 97.9%
duty_v = 0.5 - 2.25/12  = 31.3%
duty_w = 0.5 - 5.75/12  = 2.1%
```

这时候没有超过 100%。

重点来了：

虽然三相相电压变了，但是线电压没变。

比如原来：

```text
Va - Vb = 6.5 - (-1.5) = 8.0V
```

现在：

```text
Va' - Vb' = 5.75 - (-2.25) = 8.0V
```

线电压一样。

所以电机看到的有效驱动效果一样，但 PWM 没超限。

这就是 SVPWM 的强处。

---

# 12. 为什么这个对 FOC 很重要？

FOC 电流环会不断算：

```text
Vd、Vq
```

当电机速度变高时，反电动势变大，电流环为了维持目标电流，会需要更高的输出电压。

也就是：

```text
电机越高速
需要的 Vq 越大
```

如果你用 SPWM，可能比较早就遇到：

```text
PWM占空比到头了
电压给不上去了
电流跟不上目标
转矩下降
最高速度上不去
```

如果用 SVPWM，可以晚一点饱和：

```text
同样 24V 母线
SVPWM 能输出更高有效线电压
所以高速能力更好
```

这就是为什么 FOC 工程里常用 SVPWM。

---

# 13. FOC 里面 SPWM 和 SVPWM 的代码区别

## FOC 前半段一样

```c
// 电流环 PI 输出
vd = id_pi_output;
vq = iq_pi_output;

// 反 Park 变换
v_alpha = vd * cosf(theta_e) - vq * sinf(theta_e);
v_beta  = vd * sinf(theta_e) + vq * cosf(theta_e);
```

---

## 如果接 SPWM

```c
va = v_alpha;
vb = -0.5f * v_alpha + 0.8660254f * v_beta;
vc = -0.5f * v_alpha - 0.8660254f * v_beta;

duty_u = 0.5f + va / vbus;
duty_v = 0.5f + vb / vbus;
duty_w = 0.5f + vc / vbus;
```

---

## 如果接 SVPWM

```c
va = v_alpha;
vb = -0.5f * v_alpha + 0.8660254f * v_beta;
vc = -0.5f * v_alpha - 0.8660254f * v_beta;

vmax = max3(va, vb, vc);
vmin = min3(va, vb, vc);

offset = 0.5f * (vmax + vmin);

va = va - offset;
vb = vb - offset;
vc = vc - offset;

duty_u = 0.5f + va / vbus;
duty_v = 0.5f + vb / vbus;
duty_w = 0.5f + vc / vbus;
```

你看，SVPWM 相比 SPWM，多的核心就是：

```c
offset = 0.5f * (vmax + vmin);

va = va - offset;
vb = vb - offset;
vc = vc - offset;
```

就是这一步让它母线利用率更高。

---

# 14. 放到 STM32 里是什么关系？

STM32 里面实际是这样：

```text
FOC 算法
    ↓
得到 duty_u、duty_v、duty_w
    ↓
写 TIM1 CCR
    ↓
TIM1 输出三相 PWM
    ↓
三相驱动芯片 / MOS
    ↓
电机
```

代码最后都是：

```c
__HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, duty_u * PWM_PERIOD);
__HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, duty_v * PWM_PERIOD);
__HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_3, duty_w * PWM_PERIOD);
```

所以：

```text
STM32 不关心你用 SPWM 还是 SVPWM
STM32 只认 CCR 值
```

区别在于：

```text
你的软件怎么算 duty_u/duty_v/duty_w
```

---

# 15. 最终对比，结合 FOC 说

| 项目          | FOC + SPWM | FOC + SVPWM        |
| ----------- | ---------- | ------------------ |
| 前面的 FOC 电流环 | 一样         | 一样                 |
| 输入          | Vα、Vβ      | Vα、Vβ              |
| 输出          | 三相 PWM 占空比 | 三相 PWM 占空比         |
| 实现难度        | 简单         | 稍复杂                |
| 母线利用率       | 低一点        | 高一点                |
| 高速能力        | 差一点        | 好一点                |
| 工程常用程度      | 可以用        | 更常用                |
| 新手理解难度      | 低          | 中等                 |
| 本质区别        | 直接三相正弦调制   | 三相共同偏移，充分利用 PWM 范围 |

---

# 16. 最简单一句话

在 FOC 里：

```text
FOC 负责算 Vα、Vβ。
SPWM/SVPWM 负责把 Vα、Vβ 变成三相 PWM。
```

区别是：

```text
SPWM：
    老老实实按三相正弦输出，占空比容易先碰到 0% 或 100%。

SVPWM：
    把三相占空比整体挪一挪，线电压不变，但更不容易碰到 0%/100%，所以母线电压利用率更高。
```

再更口语一点：

> **FOC 是导航，告诉你目标方向；SPWM 是普通司机；SVPWM 是老司机，走同样目标，但更会利用路面空间。**



