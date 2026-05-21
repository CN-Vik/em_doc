
可以，先不用陷入公式和代码，先从**整体控制思想**理解平衡车 PID。

平衡车控制系统，本质上就干一件事：

> **让轮子不断移动到车身重心下面，防止车倒。**

车身往前倒，轮子就往前跑；车身往后倒，轮子就往后跑。只要轮子能及时追上车身重心，车就能站住。

---

# 一、平衡车一般有三个控制目标

一个完整平衡车不是只控制“站住”，它通常要同时实现：

```text
1. 不倒
2. 能前进后退
3. 能转弯
```

所以就分成三个环：

```text
直立环：负责不倒
速度环：负责前进、后退、停止
转向环：负责左转、右转
```

---

# 二、三个环的关系

这三个环不是平级关系。

它们大概是这种关系：

```text
速度环
  ↓
直立环
  ↓
电机输出
```

转向环单独叠加到左右轮：

```text
转向环
  ↓
左右轮差速
```

所以完整结构可以理解成：

```text
速度环：决定车身要前倾还是后仰
直立环：根据车身倾斜情况控制电机
转向环：让左右轮一个快一点，一个慢一点
```

---

# 三、直立环：平衡车的核心

直立环是最重要的。

它关心的是：

```text
当前车身角度是多少？
车身正在往哪边倒？
倒得有多快？
```

例如：

```text
车身前倾 → 电机往前转
车身后仰 → 电机往后转
```

这样轮子就会追到车身下面。

直立环一般用 **PD 控制**。

```text
P：看你歪了多少
D：看你倒得有多快
```

可以这样理解：

```text
P 是扶正力度
D 是刹车阻尼
```

如果只有 P，车可能像弹簧一样前后晃。

加上 D，车会稳很多。

---

# 四、速度环：不是直接控制电机，而是控制目标倾角

这是平衡车最关键、最容易想错的地方。

很多人一开始会以为：

```text
速度环输出电机 PWM
```

其实更合理的是：

```text
速度环输出目标倾角
```

比如你想让车往前走，不是直接命令轮子一直往前转，而是让车身稍微前倾。

```text
想前进 → 让车身目标角度稍微前倾
想后退 → 让车身目标角度稍微后仰
想停车 → 让车身目标角度回到接近 0°
```

车身一前倾，直立环为了防止它倒，就会让轮子往前跑。

于是车就前进了。

所以速度环的作用是：

> **告诉直立环：你现在不要完全直立，你应该稍微前倾或者后仰一点。**

---

# 五、转向环：控制左右轮差速

转向就简单很多。

双轮平衡车转弯靠的是左右轮速度不同。

```text
左轮快，右轮慢 → 往一边转
右轮快，左轮慢 → 往另一边转
```

所以转向环最终不是改变车身角度，而是改变左右轮输出差值。

例如：

```text
基础平衡输出 = balance_output
转向输出 = turn_output
```

那么：

```text
左轮 = balance_output + turn_output
右轮 = balance_output - turn_output
```

这样左右轮就产生了差速。

---

# 六、三个环合起来是什么样？

完整逻辑可以这样看：

```text
          目标速度
             ↓
          速度环
             ↓
          目标倾角
             ↓
当前角度 → 直立环 → 基础电机输出
当前角速度 →  ↑
             ↓
        左右轮混合控制 ← 转向环
             ↓
        左电机 / 右电机
```

换成大白话就是：

```text
速度环说：我想前进，所以车身应该前倾 3°。

直立环说：现在目标是前倾 3°，我来控制电机，让车既不倒，又按这个姿态运动。

转向环说：在这个基础上，左轮多一点，右轮少一点，让车转弯。
```

---

# 七、为什么不能三个 PID 输出直接加一起？

有些人会写成：

```text
电机输出 = 直立环输出 + 速度环输出 + 转向环输出
```

这不是最理想的思路。

因为速度环如果直接控制电机，就会和直立环抢控制权。

直立环想让车赶紧扶正，速度环又想让车减速或者加速，两个环都直接控制电机，就容易打架。

更合理的是：

```text
速度环不直接碰电机
速度环只改目标角度
直立环根据目标角度去控制电机
```

这样系统层级更清楚。

---

# 八、可以把平衡车想成人骑车

你可以把平衡车想象成一个人在控制身体：

## 直立环

像人的本能反应。

你身体快往前倒了，你会下意识往前迈一步。

```text
身体前倒 → 脚往前迈
```

平衡车也是：

```text
车身前倒 → 轮子往前跑
```

---

## 速度环

像人的主观意图。

你想往前走，不是直接让脚疯狂转，而是身体先微微前倾。

```text
想前进 → 身体前倾 → 脚自然往前跟
```

平衡车也是：

```text
想前进 → 目标角度前倾 → 直立环驱动车轮前进
```

---

## 转向环

像人走路时左右脚发力不同。

```text
一边快一点，一边慢一点，就能改变方向
```

平衡车也是：

```text
左轮和右轮速度不同，就能转弯
```

---

# 九、最终笼统总结

平衡车 PID 控制系统可以这样理解：

```text
直立环负责“站住”
速度环负责“走不走、走多快”
转向环负责“往哪边走”
```

三者整合方式是：

```text
速度环输出目标倾角
直立环根据目标倾角输出基础动力
转向环在左右轮上叠加差速
```

最终：

```text
左轮输出 = 平衡输出 + 转向输出
右轮输出 = 平衡输出 - 转向输出
```

最核心的一句话：

> **平衡车不是靠电机直接控制速度站住，而是靠“调节车身倾角”，让轮子不断追赶重心，从而实现平衡、前进、后退和转向。**

------
------
-----
下面这个思路非常关键：

**平衡车三个环不是三个 PID 各自算一个 PWM，然后简单相加。**

更合理的结构是：

```text
速度环：决定“车身应该往前/往后倾多少”
        ↓
直立环：根据目标倾角，快速控制电机把车立住
        ↓
转向环：在左右电机上叠加一个差速量，实现转弯
```

也就是：

```text
速度环输出：目标倾角 angle_target

直立环输出：基础电机力矩 balance_output

转向环输出：左右轮差速 turn_output

左轮输出  = balance_output + turn_output
右轮输出  = balance_output - turn_output
```

---

# 1. 平衡车到底要控制什么？

双轮平衡车本质上是一个**倒立摆**。

车身快往前倒了，轮子就要往前跑，把车轮追到车身下面。

车身快往后倒了，轮子就要往后跑。

所以最核心的控制目标不是速度，而是：

```text
让车身角度维持在接近 0°
```

假设：

```text
车身垂直时：angle = 0°
车身前倾时：angle > 0
车身后仰时：angle < 0
```

那么控制逻辑就是：

```text
前倾 → 电机向前转
后仰 → 电机向后转
```

这就是**直立环**要做的事情。

---

# 2. 三个环分别干什么？

平衡车一般有这三个控制环：

```text
1. 直立环 / 平衡环
2. 速度环
3. 转向环
```

## 2.1 直立环：让车不倒

输入：

```text
目标角度 angle_target
当前角度 angle
当前角速度 gyro_y
```

输出：

```text
balance_output
```

也就是基础电机输出。

直立环通常用 **PD** 就够了，不一定一开始就用完整 PID。

常见形式：

```c
balance_output = Kp_angle * (angle - angle_target)
               + Kd_angle * gyro_y;
```

这里假设：

```text
angle 前倾为正
gyro_y 前倾角速度为正
电机正输出表示轮子向前转
```

所以如果车身前倾：

```text
angle > 0
balance_output > 0
电机向前跑
```

轮子就会追到车身下面，把车扶起来。

这个环是整个平衡车最核心、最快的环。

---

## 2.2 速度环：决定车该往哪边倾

很多初学者容易想错：

```text
速度环是不是直接控制电机 PWM？
```

不推荐。

因为平衡车想往前走，不是直接让轮子一直往前转，而是要让车身**稍微前倾**。

车身前倾后，直立环为了防止它倒下，会驱动轮子向前追，于是车就往前走。

所以速度环更合理的输出不是 PWM，而是：

```text
目标倾角 angle_target
```

例如：

```text
想前进 → 速度环输出一个正的目标倾角，比如 +3°
想后退 → 速度环输出一个负的目标倾角，比如 -3°
想原地停住 → 速度环输出接近 0°
```

速度环输入：

```text
target_speed
actual_speed
```

输出：

```text
angle_target
```

代码形式类似：

```c
speed_error = target_speed - actual_speed;
speed_error_integral += speed_error;

angle_target = Kp_speed * speed_error
             + Ki_speed * speed_error_integral;
```

然后把这个 `angle_target` 送给直立环。

---

## 2.3 转向环：让左右轮产生差速

平衡车转弯的本质是：

```text
左轮和右轮速度不同
```

例如：

```text
左轮快，右轮慢 → 右转
右轮快，左轮慢 → 左转
```

转向环输入可以是：

```text
target_turn
gyro_z
```

或者：

```text
target_turn
left_encoder - right_encoder
```

输出：

```text
turn_output
```

然后叠加到左右电机：

```c
left_output  = balance_output + turn_output;
right_output = balance_output - turn_output;
```

这样：

```text
turn_output = 0
左右轮输出相同，车直走或原地平衡

turn_output > 0
左轮输出大，右轮输出小，车转向一边
```

具体正负方向需要你根据实际电机安装调一下。

---

# 3. 三个环怎么整合成完整系统？

完整控制框架是这样：

```text
                 目标速度 target_speed
                         │
                         ▼
                  ┌─────────────┐
                  │   速度环 PI  │
                  └─────────────┘
                         │
                         ▼
                 目标倾角 angle_target
                         │
                         ▼
当前角度 angle ───► ┌─────────────┐
角速度 gyro_y ───► │   直立环 PD  │
                  └─────────────┘
                         │
                         ▼
                  balance_output
                         │
                         │
目标转向 target_turn      │
gyro_z / 编码器差速       │
        │                │
        ▼                ▼
 ┌─────────────┐    ┌───────────────┐
 │   转向环 P/PD │    │   左右轮混控    │
 └─────────────┘    └───────────────┘
        │                │
        ▼                ▼
  turn_output      left_pwm / right_pwm
```

最终代码关系：

```c
angle_target = speed_pid(target_speed, actual_speed);

balance_output = balance_pd(angle, angle_target, gyro_y);

turn_output = turn_pid(target_turn, actual_turn);

left_motor  = balance_output + turn_output;
right_motor = balance_output - turn_output;
```

这就是三个环的整合方式。

---

# 4. 最重要的理解：速度环为什么要输出角度？

假设车原地平衡，目标速度是 0。

但是车因为地面不平或者重心问题，慢慢往前跑了。

这时候：

```text
target_speed = 0
actual_speed > 0
speed_error = target_speed - actual_speed < 0
```

速度环发现：

```text
车不该往前跑，但是它往前跑了
```

于是速度环输出一个负的目标角度：

```text
angle_target < 0
```

也就是让车稍微后仰。

然后直立环看到：

```text
目标是后仰一点
当前可能还是直立或前倾
```

于是控制电机往后修正，车速慢下来，最终停住。

所以：

```text
速度环不是直接让电机停
而是通过改变“目标倾角”间接控制速度
```

这是平衡车控制系统的核心思想。

---

# 5. 控制周期怎么安排？

三个环不一定同频率。

建议：

```text
直立环：500Hz ~ 1000Hz
速度环：50Hz ~ 100Hz
转向环：50Hz ~ 200Hz
```

例如你用 1ms 定时器：

```c
每 1ms 执行一次直立环，也就是 1000Hz

每 10ms 执行一次速度环，也就是 100Hz

每 5ms 或 10ms 执行一次转向环
```

伪代码：

```c
void control_loop_1ms(void)
{
    static uint16_t speed_cnt = 0;
    static uint16_t turn_cnt = 0;

    // 1. 读取 IMU
    imu_update();

    // 2. 获取车身角度和角速度
    float angle = imu_get_pitch_angle();
    float gyro_y = imu_get_pitch_gyro();

    // 3. 读取编码器速度
    encoder_update();

    float actual_speed = get_average_wheel_speed();
    float actual_turn  = get_turn_feedback();

    // 4. 速度环，周期慢一点
    if (++speed_cnt >= 10)
    {
        speed_cnt = 0;

        angle_target = speed_pid_calc(target_speed, actual_speed);

        // 限制目标倾角，防止速度环把车身压得太狠
        angle_target = limit(angle_target, -8.0f, 8.0f);
    }

    // 5. 转向环
    if (++turn_cnt >= 5)
    {
        turn_cnt = 0;

        turn_output = turn_pid_calc(target_turn, actual_turn);

        turn_output = limit(turn_output, -300.0f, 300.0f);
    }

    // 6. 直立环，每 1ms 都执行
    balance_output = balance_pd_calc(angle, angle_target, gyro_y);

    // 7. 左右轮混控
    left_output  = balance_output + turn_output;
    right_output = balance_output - turn_output;

    // 8. 限幅
    left_output  = limit(left_output, -PWM_MAX, PWM_MAX);
    right_output = limit(right_output, -PWM_MAX, PWM_MAX);

    // 9. 异常保护
    if (angle > 35.0f || angle < -35.0f)
    {
        motor_stop();
        return;
    }

    // 10. 输出给电机
    motor_set(left_output, right_output);
}
```

---

# 6. PID 公式在这里怎么用？

标准 PID 公式：

$$
u(t)=K_p e(t)+K_i \int e(t)\,dt+K_d \frac{de(t)}{dt}
$$

但平衡车里面，不是三个环都必须完整 PID。

更常见的是：

```text
直立环：PD
速度环：PI
转向环：P / PD / PI
```

## 6.1 直立环一般用 PD

直立环最怕慢，所以不能太依赖积分。

```c
balance_output = Kp_angle * (angle - angle_target)
               + Kd_angle * gyro_y;
```

`Kp_angle` 负责：

```text
车歪了多少，就给多少纠正力
```

`Kd_angle` 负责：

```text
车倒得有多快，就提前刹住
```

所以：

```text
Kp 是扶正力度
Kd 是阻尼
```

如果只有 Kp，车容易来回摆。

加上 Kd 后，动作会稳很多。

---

## 6.2 速度环一般用 PI

```c
speed_error = target_speed - actual_speed;

speed_integral += speed_error;

angle_target = Kp_speed * speed_error
             + Ki_speed * speed_integral;
```

`Kp_speed` 负责：

```text
速度不对时，马上让车身倾斜一点
```

`Ki_speed` 负责：

```text
长时间有速度偏差时，慢慢修正
```

例如车停不住、一直慢慢往前溜，速度积分就可以慢慢把目标倾角拉回来。

但速度环积分一定要限幅：

```c
speed_integral = limit(speed_integral, -I_MAX, I_MAX);
```

否则一旦车被卡住、拿起来、倒地，积分会越积越大，恢复时电机会突然猛冲。

---

## 6.3 转向环可以先用 P

最简单：

```c
turn_error = target_turn - actual_turn;

turn_output = Kp_turn * turn_error;
```

如果你用陀螺仪 Z 轴做转向反馈：

```c
turn_error = target_yaw_rate - gyro_z;

turn_output = Kp_turn * turn_error;
```

如果你先不做闭环转向，也可以直接：

```c
turn_output = remote_turn_cmd;
```

也就是遥控器给一个转向量，直接混到左右轮上。

新手阶段可以先这样。

---

# 7. 具体调试步骤

这个非常重要。

不要一上来三个环全开。

正确顺序是：

```text
第一步：确认方向
第二步：只调直立环
第三步：加入速度环
第四步：加入转向环
第五步：整体微调
```

---

## 第一步：确认电机方向、编码器方向、角度方向

这是最容易出问题的地方。

先定义：

```text
车身前倾：angle > 0
轮子向前转：motor_output > 0
编码器向前：encoder_speed > 0
```

然后做三个检查。

### 检查 1：角度方向

把车往前倾。

串口打印：

```c
printf("angle = %.2f\n", angle);
```

你希望看到：

```text
前倾 angle 变大，最好是正数
后仰 angle 变小，最好是负数
```

如果反了，要在软件里取反。

---

### 检查 2：电机方向

手动给电机一个正输出：

```c
motor_set(200, 200);
```

看轮子是不是向前转。

如果不是，调换电机方向。

---

### 检查 3：平衡修正方向

把车拿在手里，轮子悬空。

车身往前倾。

你希望电机反应是：

```text
前倾 → 轮子向前转
后仰 → 轮子向后转
```

如果前倾时轮子反而向后转，那就是直立环输出方向反了。

这时候可以把：

```c
balance_output = Kp_angle * (angle - angle_target)
               + Kd_angle * gyro_y;
```

改成：

```c
balance_output = -balance_output;
```

或者调电机方向。

这一步没对，后面 PID 怎么调都没用。

---

# 8. 只调直立环

先关闭速度环和转向环：

```c
target_speed = 0;
angle_target = 0;
turn_output = 0;
```

只保留：

```c
balance_output = Kp_angle * angle + Kd_angle * gyro_y;
```

也就是让车保持垂直。

## 8.1 先调 Kp_angle

`Kd_angle = 0`

慢慢增大 `Kp_angle`。

现象：

```text
Kp 太小：车软绵绵，扶不住，直接倒
Kp 合适：你用手推它，它会有明显反抗
Kp 太大：车疯狂抖动、来回抽搐
```

目标：

```text
车有明显扶正力量，但还没有剧烈高频抖动
```

---

## 8.2 再加 Kd_angle

加 `Kd_angle` 后，车会从“硬顶”变成“有阻尼”。

现象：

```text
Kd 太小：车来回晃，像弹簧
Kd 合适：车身回正比较稳
Kd 太大：电机噪声大，动作发僵，甚至高频震
```

直立环调好后的感觉应该是：

```text
你拿着车轻轻前后晃，它会顺着你的动作快速反向修正
```

但是不要指望这一步就完全站住。

只靠直立环，它可能会慢慢往前跑或者往后跑。

这很正常，因为还没有速度环。

---

# 9. 加入速度环

直立环能基本站住后，再加速度环。

速度环输出目标倾角：

```c
angle_target = speed_pid(target_speed, actual_speed);
```

然后直立环变成：

```c
balance_output = Kp_angle * (angle - angle_target)
               + Kd_angle * gyro_y;
```

注意，这里不是：

```c
motor_output = balance_output + speed_output;
```

而是：

```c
angle_target = speed_output;
```

这是重点。

---

## 9.1 速度环的实际意义

车往前跑了：

```text
actual_speed > 0
target_speed = 0
speed_error < 0
```

速度环输出：

```text
angle_target < 0
```

意思是：

```text
你给我稍微后仰一点，把速度拉回来
```

车想前进：

```text
target_speed > 0
actual_speed = 0
speed_error > 0
```

速度环输出：

```text
angle_target > 0
```

意思是：

```text
你给我稍微前倾一点，车自然会往前走
```

---

## 9.2 速度环目标倾角必须限幅

例如：

```c
angle_target = limit(angle_target, -8.0f, 8.0f);
```

新手建议更保守：

```c
angle_target = limit(angle_target, -5.0f, 5.0f);
```

不要让速度环输出几十度目标角度。

否则车会猛冲，很危险。

---

## 9.3 速度环积分要慢

速度环可以用 PI，但积分不能太猛。

```c
speed_integral += speed_error;
speed_integral = limit(speed_integral, -I_MAX, I_MAX);
```

如果出现：

```text
车停不住，慢慢往一个方向溜
```

可以慢慢加 `Ki_speed`。

如果出现：

```text
车突然越跑越快
车倒地后扶起来猛冲
```

大概率是速度环积分太大，或者没有积分限幅。

---

# 10. 加入转向环

最后再加转向。

最简单的转向方式：

```c
turn_output = target_turn * K_turn;
```

然后：

```c
left_output  = balance_output + turn_output;
right_output = balance_output - turn_output;
```

例如：

```c
target_turn > 0
```

让左轮多一点，右轮少一点。

这样车就转向。

---

## 10.1 转向不要影响平衡主逻辑

转向环不要去改 `angle_target`。

它只做左右轮差速。

```text
速度环：改目标倾角
直立环：输出基础力矩
转向环：改左右轮差值
```

这三个职责要分清。

---

# 11. 推荐的完整 C 代码框架

下面是一个简化版结构，适合你实际移植到 MCU。

```c
typedef struct
{
    float kp;
    float ki;
    float kd;

    float integral;
    float last_error;

    float out_min;
    float out_max;
    float integral_min;
    float integral_max;
} PID_t;
```

通用 PID：

```c
float pid_calc(PID_t *pid, float target, float feedback)
{
    float error = target - feedback;

    pid->integral += error;

    if (pid->integral > pid->integral_max)
        pid->integral = pid->integral_max;
    else if (pid->integral < pid->integral_min)
        pid->integral = pid->integral_min;

    float derivative = error - pid->last_error;
    pid->last_error = error;

    float output = pid->kp * error
                 + pid->ki * pid->integral
                 + pid->kd * derivative;

    if (output > pid->out_max)
        output = pid->out_max;
    else if (output < pid->out_min)
        output = pid->out_min;

    return output;
}
```

但是直立环建议单独写，因为直立环的 D 项最好直接用陀螺仪，不要用角度差分。

```c
float balance_pd_calc(float angle, float angle_target, float gyro_y)
{
    float output;

    output = Kp_angle * (angle - angle_target)
           + Kd_angle * gyro_y;

    return output;
}
```

主控制：

```c
void balance_car_control_1ms(void)
{
    static uint8_t speed_div = 0;
    static uint8_t turn_div = 0;

    float angle;
    float gyro_y;
    float gyro_z;
    float actual_speed;
    float actual_turn;

    float balance_output;
    float left_output;
    float right_output;

    // 1. 更新传感器
    imu_update();
    encoder_update();

    // 2. 获取姿态
    angle  = imu_get_pitch_angle();
    gyro_y = imu_get_pitch_gyro();
    gyro_z = imu_get_yaw_gyro();

    // 3. 获取速度
    actual_speed = encoder_get_average_speed();
    actual_turn  = gyro_z;

    // 4. 速度环，10ms 一次
    if (++speed_div >= 10)
    {
        speed_div = 0;

        angle_target = pid_calc(&speed_pid, target_speed, actual_speed);

        // 速度环输出的是目标倾角
        angle_target = limit(angle_target, -6.0f, 6.0f);
    }

    // 5. 转向环，5ms 一次
    if (++turn_div >= 5)
    {
        turn_div = 0;

        turn_output = pid_calc(&turn_pid, target_turn, actual_turn);

        turn_output = limit(turn_output, -TURN_MAX, TURN_MAX);
    }

    // 6. 直立环，1ms 一次
    balance_output = balance_pd_calc(angle, angle_target, gyro_y);

    // 7. 左右轮混控
    left_output  = balance_output + turn_output;
    right_output = balance_output - turn_output;

    // 8. 输出限幅
    left_output  = limit(left_output, -PWM_MAX, PWM_MAX);
    right_output = limit(right_output, -PWM_MAX, PWM_MAX);

    // 9. 倒地保护
    if (angle > 35.0f || angle < -35.0f)
    {
        motor_stop();

        speed_pid.integral = 0;
        turn_pid.integral = 0;

        return;
    }

    // 10. 电机输出
    motor_set_pwm(left_output, right_output);
}
```

---

# 12. 如果你用 FOC，输出就不是 PWM，而是 Iq

如果你后面做的是：

```text
STM32G474 + 双电机 FOC 平衡车
```

那么直立环最终不要直接输出 PWM，而是输出：

```text
目标转矩电流 Iq_ref
```

结构变成：

```text
速度环输出 angle_target
直立环输出 torque_target / Iq_target
转向环输出 Iq_diff
```

左右电机：

```c
left_iq_ref  = balance_iq + turn_iq;
right_iq_ref = balance_iq - turn_iq;
```

FOC 内部还有自己的电流环：

```text
Iq_ref → 电流环 PI → SVPWM → 三相桥
```

也就是说，如果用 FOC，整体会变成四层：

```text
速度环
  ↓
直立环
  ↓
电流/力矩环
  ↓
电机三相 PWM
```

平衡控制本身不直接管三相 PWM，FOC 底层负责把 `Iq_ref` 转成三相电流。

---

# 13. 一个完整的调试流程

建议你按这个顺序来：

## 第 1 步：只跑传感器

先不要开电机。

串口打印：

```text
angle
gyro_y
left_encoder
right_encoder
```

确认：

```text
前倾 angle 变正
后仰 angle 变负
轮子前转 encoder 变正
```

---

## 第 2 步：只测电机方向

给固定输出：

```c
motor_set_pwm(200, 200);
```

确认：

```text
两个轮子都向前
```

然后：

```c
motor_set_pwm(-200, -200);
```

确认：

```text
两个轮子都向后
```

---

## 第 3 步：只开直立环

关闭速度环：

```c
angle_target = 0;
```

关闭转向环：

```c
turn_output = 0;
```

只调：

```c
Kp_angle
Kd_angle
```

目标：

```text
车可以短时间站住
或者你用手扶着时，它有明显自我纠正能力
```

---

## 第 4 步：加入速度环，但目标速度为 0

```c
target_speed = 0;
```

让速度环帮你解决：

```text
车慢慢往前溜
车慢慢往后退
```

调：

```text
Kp_speed
Ki_speed
```

注意：

```text
Kp_speed 不要太大
Ki_speed 更要小
angle_target 一定限幅
```

---

## 第 5 步：加入遥控速度

例如按键或遥控器给：

```c
target_speed = 100;
```

速度环会输出：

```c
angle_target = +几度
```

车就会前进。

松开按键：

```c
target_speed = 0;
```

车就会重新停下来。

---

## 第 6 步：加入转向

先简单开环：

```c
turn_output = remote_turn * K_turn;
```

能转以后，再做闭环：

```c
turn_output = turn_pid(target_yaw_rate, gyro_z);
```

---

# 14. 新手最容易犯的几个错误

## 错误 1：三个 PID 输出直接相加

错误做法：

```c
motor = balance_pid + speed_pid + turn_pid;
```

这样速度环会直接和直立环抢电机控制权，容易乱。

更推荐：

```c
angle_target = speed_pid输出;

balance_output = 直立环(angle, angle_target);

left  = balance_output + turn_output;
right = balance_output - turn_output;
```

---

## 错误 2：速度环太快

速度环如果和直立环一样快，容易把系统搞震荡。

建议：

```text
直立环 1ms
速度环 10ms
```

速度环是外环，要慢一点。

---

## 错误 3：速度环输出不限幅

速度环输出的是目标倾角。

目标倾角一定要限制：

```c
angle_target = limit(angle_target, -6.0f, 6.0f);
```

新手不建议超过 ±8°。

---

## 错误 4：直立环加太多积分

直立环不建议一开始用积分。

因为积分会让平衡车变得很怪：

```text
倒地后积分爆炸
扶起来突然猛冲
长时间抖动
```

直立环先用 PD。

积分主要放速度环。

---

## 错误 5：没有倒地保护

必须加：

```c
if (fabs(angle) > 35.0f)
{
    motor_stop();
}
```

否则车倒了以后，轮子可能疯狂转，很危险。

---

# 15. 最终你可以这样理解整个系统

平衡车三环整合后的逻辑是：

```text
速度环像“大脑”：
我想前进、后退、停止，所以我决定车身该倾斜几度。

直立环像“身体反射”：
车身歪了，我马上让轮子追过去，防止倒下。

转向环像“方向盘”：
在不破坏平衡的前提下，让左右轮产生差速。
```

最终控制公式就是：

```c
angle_target = speed_pid(target_speed, actual_speed);

balance_output = Kp_angle * (angle - angle_target)
               + Kd_angle * gyro_y;

turn_output = turn_pid(target_turn, actual_turn);

left_motor  = balance_output + turn_output;
right_motor = balance_output - turn_output;
```

你真正要记住的是这句话：

**速度环不直接控制电机，它控制目标倾角；直立环才是直接决定基础电机输出的核心环；转向环只在左右轮上叠加差速。**


-------
-----
-----

# 为啥有的说速度环，是控制电机速度呢？？
因为“速度环”这个词在平衡车里有两种说法，容易混在一起。

它们说的**不一定错**，只是站的层级不同。

---

# 1. 从控制目标上说：速度环确实是在控制车轮速度

平衡车要实现：

```text
目标速度 = 0
实际速度 = 车轮编码器测出来的速度
```

如果车自己往前溜了：

```text
实际速度 > 0
目标速度 = 0
```

速度环就要想办法让车停下来。

所以从结果上看，速度环确实是在控制：

```text
车轮速度 / 电机速度 / 小车速度
```

因此有人说：

> 平衡车速度环是控制电机速度的。

这个说法从**控制目标**角度看，是对的。

---

# 2. 但从控制手段上说：速度环通常不直接控制 PWM

关键区别在这里。

速度环的目标是控制速度，但是它不一定直接输出电机 PWM。

更好的结构是：

```text
速度环的目标：控制速度
速度环的输出：目标倾角
```

也就是：

```text
我想控制速度
但我不是直接命令电机转多快
而是命令车身该前倾还是后仰
```

比如车往前溜了：

```text
目标速度 = 0
实际速度 > 0
```

速度环发现车在往前跑，于是输出：

```text
目标倾角 = 后仰一点
```

然后直立环接到这个目标倾角后，会控制电机往后修正。

所以你可以这样理解：

```text
速度环控制的是速度误差
但是输出的是目标倾角
```

---

# 3. 为啥不能简单理解成“速度环直接控制电机速度”？

因为平衡车不是普通小车。

普通小车是这样的：

```text
目标速度 → 速度 PID → 电机 PWM
```

比如普通四轮车：

```text
想跑快点 → 电机转快点
想停下来 → 电机转慢点
```

但平衡车是倒立摆，它首先要保证不倒。

它的核心矛盾是：

```text
车身角度必须稳定
```

如果你直接让电机进入传统速度控制：

```text
目标电机速度 → 电机速度 PID → PWM
```

那可能会出现问题：

```text
车身快倒了，需要电机马上冲出去救车
但是电机速度环却说：目标速度是 0，你不能转
```

这两个目标就打架了。

所以平衡车里面，直立环优先级最高。

---

# 4. 更准确的说法

平衡车速度环更准确应该叫：

```text
车体速度外环
```

它不是底层电机速度闭环，而是平衡控制系统的外环。

它检测的是：

```text
小车整体是不是在移动
```

通常用编码器得到：

```text
左轮速度
右轮速度
平均速度
```

例如：

```c
actual_speed = (left_encoder_speed + right_encoder_speed) / 2;
```

然后速度环计算：

```c
speed_error = target_speed - actual_speed;
```

最后速度环输出：

```c
angle_target = speed_pid_output;
```

也就是说：

```text
速度环调的是车速
但它通过改变目标倾角来调车速
```

---

# 5. 为什么有些代码是 speed_output 直接加到 motor_output？

你可能会看到这种代码：

```c
balance_output = angle_pd();
speed_output   = speed_pi();

motor_output = balance_output + speed_output;
```

于是很多教程就说：

```text
速度环控制电机速度
```

这种写法也很常见。

但是从本质上看，`speed_output` 其实是在给直立环加一个偏置，让车产生前倾或后仰趋势。

它看起来像是直接加到了电机输出上：

```text
motor = 直立输出 + 速度输出
```

但实际效果还是：

```text
让车身姿态产生偏移
从而修正速度
```

这种写法可以用，但层级没有“速度环输出目标倾角”那么清晰。

---

# 6. 两种结构对比

## 结构 A：速度环输出目标倾角

这个逻辑更清楚：

```text
目标速度
  ↓
速度环
  ↓
目标倾角
  ↓
直立环
  ↓
电机输出
```

代码类似：

```c
angle_target = speed_pid(target_speed, actual_speed);

balance_output = angle_pd(angle_target, angle, gyro);

motor_output = balance_output;
```

这个适合理解和调试。

---

## 结构 B：速度环输出直接叠加到电机

很多教学代码会这么写：

```text
目标速度
  ↓
速度环
  ↓
速度修正量
  ↓
和直立环输出相加
```

代码类似：

```c
balance_output = angle_pd(angle, gyro);

speed_output = speed_pi(target_speed, actual_speed);

motor_output = balance_output + speed_output;
```

这种写法不是完全错，但新手容易误解成：

```text
速度环在直接控制电机转速
```

其实它更多是在给平衡控制增加一个慢速修正。

---

# 7. 那到底哪种说法更推荐？

我建议你这样记：

```text
速度环的控制目标是车速/轮速
速度环的推荐输出是目标倾角
直立环才是直接决定电机基础输出的核心
```

一句话：

> **速度环是为了控制电机/车轮速度，但在平衡车里，它最好不要像普通电机速度环那样直接管 PWM，而是通过目标倾角间接影响电机。**

---

# 8. 举个直观例子

假设你站在平衡车上，车本来应该原地不动。

但是因为重心偏了，车慢慢往前走。

速度环看到：

```text
实际速度：往前
目标速度：0
```

它不会直接说：

```text
电机立刻停转！
```

因为电机如果硬停，车身可能直接往前摔。

更合理的是：

```text
车有点往前跑，那我让目标姿态稍微后仰一点
```

于是直立环为了达到这个后仰目标，会让轮子往后调整。

最终车速降下来。

---

# 9. 如果你用 FOC 呢？

如果你做 STM32G474 + FOC 平衡车，底层 FOC 里面可能本来就有：

```text
电流环
速度环
位置环
```

但平衡车不建议直接把电机速度环当成最外层主控制。

更合理的是：

```text
平衡车速度环 → 目标倾角
直立环 → 目标力矩 / Iq
FOC 电流环 → 三相 PWM
```

也就是：

```text
车体速度环
  ↓
目标倾角
  ↓
直立环
  ↓
Iq_ref
  ↓
FOC 电流环
  ↓
电机
```

这里的底层 FOC 速度环可以不用，或者只在特殊结构里用。

因为平衡车最需要的是：

```text
快速控制力矩
```

而不是让电机死板地保持某个转速。

---

# 10. 最后总结

别人说：

```text
速度环控制电机速度
```

这个说法不算错，因为速度环确实用编码器速度做反馈，目标也是让车轮速度符合目标。

但更准确地说：

```text
平衡车的速度环控制的是“小车运动速度”
它不一定直接控制电机 PWM
更推荐让速度环输出“目标倾角”
再由直立环去控制电机
```

你可以这样区分：

```text
普通电机速度环：
目标速度 → PID → PWM / 电流

平衡车速度环：
目标速度 → PID → 目标倾角 → 直立环 → 电机输出
```

所以核心区别是：

> **平衡车速度环的目标是速度，但它的控制入口最好是“倾角”，不是直接粗暴地控制电机速度。**
