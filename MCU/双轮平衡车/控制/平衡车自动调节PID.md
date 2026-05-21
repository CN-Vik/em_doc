可以写，但**平衡车不建议一上来就“全自动调 PID”**，尤其是直立环。原因是：平衡车本身是不稳定系统，参数一旦自动试错过头，车会直接倒、飞轮、撞人。

更靠谱的做法是：

**先人工调到能站住，再写程序做“小范围自动微调”。**

---

## 1. 平衡车一般有几个 PID 环？

典型两轮平衡车控制结构是：

```text
角度环 / 直立环
目标：让车身角度保持 0°
输入：pitch 角度、角速度
输出：电机目标速度/力矩/PWM

速度环
目标：让车不要一直往前/往后跑
输入：编码器速度
输出：给直立环一个角度偏置

转向环
目标：控制左右轮差速
输入：遥控/蓝牙/按键转向量
输出：左右轮 PWM 差值
```

最核心的是：

```text
直立环 > 速度环 > 转向环
```

直立环不稳，后面所有环都没意义。

---

## 2. 直立环能不能自动调？

能，但要非常小心。

直立环通常不一定用完整 PID，很多平衡车用 **PD 控制** 就能站起来：

```c
balance_out = Kp * angle_error + Kd * gyro_y;
```

其中：

```c
angle_error = target_angle - pitch_angle;
```

如果目标角度是 0°，那么：

```c
angle_error = 0 - pitch_angle;
```

`Kp` 负责把车拉回直立位置，`Kd` 负责抑制晃动。

一般直立环：

```text
Kp 太小：车软，扶不起来，慢慢倒
Kp 太大：剧烈抖动，来回抽搐
Kd 太小：容易过冲、来回摆
Kd 太大：响应变钝、电机发热、震动噪声大
Ki 通常先不要加
```

---

## 3. 自动调 PID 的思路

可以写一个程序，让小车自己测试不同的 `Kp`、`Kd`，然后根据表现打分。

比如判断这些指标：

```text
1. 车身最大倾角
2. 角度误差平均值
3. 角度误差平方和
4. 陀螺仪角速度大小
5. 电机输出是否过大
6. 有没有超过安全角度
7. 是否持续震荡
```

可以设计一个评分函数：

```c
score = angle_error_sum
      + gyro_error_sum
      + motor_output_sum * 0.1f
      + fall_penalty;
```

分数越低，说明参数越好。

---

## 4. 但是不能让车“乱试”

比如自动从：

```c
Kp = 0;
Kd = 0;
```

一路乱试到：

```c
Kp = 100;
Kd = 10;
```

这是很危险的。

应该这样：

```text
1. 人工先调到勉强能站
2. 设定一个小范围
3. 每次只改一点点
4. 测试 1~3 秒
5. 如果角度超过安全范围，立即停机
6. 只接受比当前更稳定的参数
```

例如：

```c
Kp 在当前值 ±10% 范围内搜索
Kd 在当前值 ±10% 范围内搜索
```

---

## 5. 一个简单的自动微调流程

```text
当前参数：
Kp = 25.0
Kd = 0.8

自动尝试：
Kp = 22.5, Kd = 0.72
Kp = 22.5, Kd = 0.80
Kp = 22.5, Kd = 0.88

Kp = 25.0, Kd = 0.72
Kp = 25.0, Kd = 0.80
Kp = 25.0, Kd = 0.88

Kp = 27.5, Kd = 0.72
Kp = 27.5, Kd = 0.80
Kp = 27.5, Kd = 0.88
```

每组参数测试一小段时间，计算稳定性分数，最后选最低分的。

---

## 6. 伪代码示例

```c
typedef struct
{
    float kp;
    float ki;
    float kd;
} PID_Param_t;

typedef struct
{
    float max_angle_abs;       // 最大倾角
    float angle_error_sum;     // 角度误差累计
    float gyro_abs_sum;        // 角速度累计
    float motor_abs_sum;       // 电机输出累计
    uint8_t is_fall;           // 是否倒车
    float score;               // 综合评分
} Tune_Result_t;
```

---

### 测试一组参数

```c
Tune_Result_t Test_Balance_PID(PID_Param_t param, uint32_t test_time_ms)
{
    Tune_Result_t result = {0};

    uint32_t start_time = millis();

    Set_Balance_PID(param.kp, param.ki, param.kd);

    while ((millis() - start_time) < test_time_ms)
    {
        float angle = Get_Pitch_Angle();      // 当前俯仰角
        float gyro  = Get_Gyro_Y();           // 当前角速度
        float motor = Get_Motor_Output();     // 当前电机输出

        float angle_abs = fabsf(angle);

        result.angle_error_sum += angle_abs;
        result.gyro_abs_sum    += fabsf(gyro);
        result.motor_abs_sum   += fabsf(motor);

        if (angle_abs > result.max_angle_abs)
        {
            result.max_angle_abs = angle_abs;
        }

        // 安全保护：倾角太大，说明快倒了
        if (angle_abs > 25.0f)
        {
            result.is_fall = 1;
            Stop_Motor();
            break;
        }

        delay_ms(5);
    }

    // 评分函数
    result.score = result.angle_error_sum
                 + result.gyro_abs_sum * 0.05f
                 + result.motor_abs_sum * 0.01f;

    // 如果倒车，直接加巨大惩罚
    if (result.is_fall)
    {
        result.score += 100000.0f;
    }

    return result;
}
```

---

### 搜索更好的参数

```c
PID_Param_t Auto_Tune_Balance_PID(PID_Param_t base)
{
    PID_Param_t best_param = base;
    Tune_Result_t best_result;

    best_result.score = 99999999.0f;

    float kp_list[3] = {
        base.kp * 0.9f,
        base.kp,
        base.kp * 1.1f
    };

    float kd_list[3] = {
        base.kd * 0.9f,
        base.kd,
        base.kd * 1.1f
    };

    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            PID_Param_t test_param;

            test_param.kp = kp_list[i];
            test_param.ki = 0.0f;        // 直立环先不要加 Ki
            test_param.kd = kd_list[j];

            Tune_Result_t result = Test_Balance_PID(test_param, 2000);

            if (result.score < best_result.score)
            {
                best_result = result;
                best_param = test_param;
            }

            Stop_Motor();
            delay_ms(1000);
        }
    }

    Set_Balance_PID(best_param.kp, best_param.ki, best_param.kd);

    return best_param;
}
```

这个就是最简单的自动微调思路。

---

## 7. 速度环可以更适合自动调

相比直立环，速度环更适合自动调。

因为速度环不直接决定小车会不会瞬间倒，它一般是给直立环一个角度偏置：

```c
speed_out = speed_kp * speed_error + speed_ki * speed_integral;
target_angle = speed_out;
```

速度环常见 PI 控制：

```c
speed_error = target_speed - actual_speed;

speed_integral += speed_error;

target_angle = Kp * speed_error + Ki * speed_integral;
```

注意速度环输出不能太大，要限幅：

```c
if (target_angle > 8.0f)
    target_angle = 8.0f;

if (target_angle < -8.0f)
    target_angle = -8.0f;
```

否则速度环一发疯，直接给直立环一个很大的目标角度，小车就冲出去了。

---

## 8. 不建议自动调所有环

不建议这样：

```text
自动调直立环 Kp Ki Kd
自动调速度环 Kp Ki Kd
自动调转向环 Kp Ki Kd
```

这样参数维度太高，很容易乱。

建议顺序是：

```text
第一步：手动调直立环 PD
第二步：自动微调直立环 Kp、Kd
第三步：手动调速度环 PI
第四步：自动微调速度环 Kp、Ki
第五步：最后调转向环
```

---

## 9. 更推荐的工程方案

对于你现在搞平衡车，我建议这样做：

```text
直立环：手动调为主，程序小范围微调
速度环：可以半自动调
转向环：手动调即可
```

不要直接搞复杂的遗传算法、粒子群、神经网络调参。那些听起来高级，但对嵌入式初学阶段不实用。

你可以先做一个**串口调参系统**：

```text
串口发送：
kp=25.5
kd=0.8
speed_kp=0.6
speed_ki=0.02

单片机实时修改 PID 参数
然后保存到 Flash
```

这个比全自动 PID 更实用。

---

## 10. 最终建议

可以写自动调 PID 程序，但策略应该是：

```text
不是让车自己从 0 开始找参数，
而是在已有可用参数附近自动微调。
```

平衡车最推荐路线：

```text
1. IMU 姿态解算稳定
2. 电机 PWM/FOC/编码器正常
3. 手动调直立环 PD，让车能站住
4. 加速度环 PI，让车不乱跑
5. 加转向环
6. 最后再写自动微调程序
```

核心一句话：

**直立环不能完全靠自动试错，太危险；速度环和转向环可以做半自动调参；最实用的是串口/蓝牙实时调参 + Flash 保存参数。**


-----
-------
------

可以，**平衡车可以写程序自动调 PID**，但要分清楚：

**可以自动调 ≠ 可以完全让车自己乱试参数。**

对平衡车来说，最合理的是：

```text
人工调到基本能站住
        ↓
程序在小范围内自动微调
        ↓
找到更稳的 Kp、Kd、Ki
        ↓
保存到 Flash
```

---

## 1. 哪些环可以自动调？

平衡车一般有这些环：

```text
直立环：控制车别倒
速度环：控制车别一直往前/往后跑
转向环：控制左右转
```

自动调 PID 的安全程度大概是：

| 控制环 |        能否自动调 | 风险 |
| --- | -----------: | -: |
| 直立环 |  可以，但只能小范围微调 |  高 |
| 速度环 |      比较适合自动调 |  中 |
| 转向环 | 可以自动调，但必要性不高 |  低 |

最危险的是**直立环**，因为参数错了车会直接倒。

---

## 2. 直立环最好先用 PD

平衡车直立环很多时候先不用完整 PID，而是用 **PD**：

```c
balance_out = Kp * angle_error + Kd * gyro_y;
```

其中：

```c
angle_error = target_angle - pitch_angle;
```

一般：

```text
Kp：负责把车拉回直立位置
Kd：负责抑制来回晃动
Ki：直立环初期不要加，容易积分发散
```

所以自动调直立环，优先只调：

```text
Kp 和 Kd
```

不要一开始就调 `Ki`。

---

## 3. 自动调 PID 的核心原理

自动调参本质就是：

```text
试一组 PID 参数
        ↓
运行一小段时间
        ↓
判断稳不稳
        ↓
给这组参数打分
        ↓
换下一组参数
        ↓
选择分数最低的参数
```

比如评分依据：

```text
角度偏差越小越好
角速度越小越好
电机输出越平滑越好
不能超过安全倾角
不能剧烈震荡
```

可以定义一个分数：

```c
score = angle_error_sum
      + gyro_abs_sum * 0.05f
      + motor_abs_sum * 0.01f
      + fall_penalty;
```

分数越低，参数越好。

---

## 4. 自动调参绝对要加保护

比如：

```c
if (fabsf(pitch_angle) > 25.0f)
{
    Stop_Motor();
    test_failed = 1;
}
```

也就是车身倾角超过 25°，直接停机。

还要限制电机输出：

```c
if (balance_out > PWM_MAX)
    balance_out = PWM_MAX;

if (balance_out < -PWM_MAX)
    balance_out = -PWM_MAX;
```

否则自动调参时参数太大，电机会突然猛冲。

---

## 5. 推荐的自动调法：小范围九宫格搜索

假设你现在手动调出来一组参数：

```c
Kp = 25.0f;
Kd = 0.8f;
```

程序不要从 0 开始试，而是在附近试：

```text
Kp = 当前值 * 0.9
Kp = 当前值
Kp = 当前值 * 1.1

Kd = 当前值 * 0.9
Kd = 当前值
Kd = 当前值 * 1.1
```

也就是测试 9 组参数：

```text
Kp 22.5, Kd 0.72
Kp 22.5, Kd 0.80
Kp 22.5, Kd 0.88

Kp 25.0, Kd 0.72
Kp 25.0, Kd 0.80
Kp 25.0, Kd 0.88

Kp 27.5, Kd 0.72
Kp 27.5, Kd 0.80
Kp 27.5, Kd 0.88
```

每组测试 1~2 秒，然后选择评分最好的。

---

## 6. 简化版程序框架

```c
typedef struct
{
    float kp;
    float ki;
    float kd;
} PID_Param_t;

typedef struct
{
    float angle_error_sum;
    float gyro_abs_sum;
    float motor_abs_sum;
    float score;
    uint8_t failed;
} PID_Tune_Result_t;
```

测试一组参数：

```c
PID_Tune_Result_t Balance_PID_Test(PID_Param_t param)
{
    PID_Tune_Result_t result = {0};

    Set_Balance_PID(param.kp, param.ki, param.kd);

    uint32_t start_time = millis();

    while (millis() - start_time < 2000)
    {
        float angle = Get_Pitch_Angle();
        float gyro  = Get_Gyro_Y();
        float motor = Get_Motor_Output();

        result.angle_error_sum += fabsf(angle);
        result.gyro_abs_sum    += fabsf(gyro);
        result.motor_abs_sum   += fabsf(motor);

        if (fabsf(angle) > 25.0f)
        {
            result.failed = 1;
            Stop_Motor();
            break;
        }

        delay_ms(5);
    }

    result.score = result.angle_error_sum
                 + result.gyro_abs_sum * 0.05f
                 + result.motor_abs_sum * 0.01f;

    if (result.failed)
    {
        result.score += 100000.0f;
    }

    return result;
}
```

自动搜索：

```c
PID_Param_t Auto_Tune_Balance_PID(PID_Param_t base)
{
    PID_Param_t best = base;
    float best_score = 99999999.0f;

    float kp_list[3] = {
        base.kp * 0.9f,
        base.kp,
        base.kp * 1.1f
    };

    float kd_list[3] = {
        base.kd * 0.9f,
        base.kd,
        base.kd * 1.1f
    };

    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            PID_Param_t test;

            test.kp = kp_list[i];
            test.ki = 0.0f;
            test.kd = kd_list[j];

            PID_Tune_Result_t result = Balance_PID_Test(test);

            if (result.score < best_score)
            {
                best_score = result.score;
                best = test;
            }

            Stop_Motor();
            delay_ms(1000);
        }
    }

    Set_Balance_PID(best.kp, best.ki, best.kd);

    return best;
}
```

---

## 7. 但是实际工程更推荐这样做

最实用的方案不是一上来全自动调，而是：

```text
串口/蓝牙实时调参
        ↓
人工调到大概稳定
        ↓
按一个按键进入自动微调
        ↓
程序在 ±10% 范围内搜索
        ↓
找到更优参数
        ↓
保存到 Flash
```

比如你可以做几个命令：

```text
kp=25.0
kd=0.8
speed_kp=0.5
speed_ki=0.02
save
auto_tune
```

这样比“全自动 PID”靠谱很多。

---

## 8. 结论

**可以写程序自动调 PID。**

但推荐原则是：

```text
直立环：手动为主，程序小范围微调
速度环：可以半自动调
转向环：手动调就够了
```

不要让小车从 `Kp=0, Ki=0, Kd=0` 开始自己乱试，那样很容易摔车。

你可以把它理解成：

```text
自动调 PID 不是让程序自己瞎蒙，
而是让程序帮你在已有可用参数附近找更优解。
```
