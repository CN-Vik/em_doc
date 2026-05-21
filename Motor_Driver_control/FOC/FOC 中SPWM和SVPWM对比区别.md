可以用，但**只能做一部分验证**。做 FOC / SVPWM 电机驱动时，虚拟示波器适合看“软件内部变量”，不适合完全替代真实示波器。

## 1. 虚拟示波器能看什么？

比如你用 STM32 做 FOC，可以把这些变量通过串口 / SWO / RTT 发出来，在上位机画波形：

```c
float id_ref;
float iq_ref;
float id;
float iq;

float theta_e;
float speed_rpm;

float u_alpha;
float u_beta;

float duty_a;
float duty_b;
float duty_c;
```

这些很适合用虚拟示波器看：

| 信号                         | 作用                |
| -------------------------- | ----------------- |
| `duty_a / duty_b / duty_c` | 看 SVPWM 三相占空比是否正常 |
| `u_alpha / u_beta`         | 看电压矢量是否旋转         |
| `theta_e`                  | 看电角度是否连续          |
| `id / iq`                  | 看电流环是否稳定          |
| `speed_rpm`                | 看速度控制是否抖动         |
| `bus_voltage`              | 看母线电压采样是否正常       |

所以如果你只是想看：

> SVPWM 算出来的三相 PWM 占空比对不对？

那虚拟示波器可以用。

---

## 2. 虚拟示波器不能替代真实示波器的地方

真正做电机驱动，最危险、最容易出问题的是**硬件实际波形**，虚拟示波器看不到。

比如这些必须用真实示波器更靠谱：

| 实际硬件信号       | 为什么重要                |
| ------------ | -------------------- |
| MCU PWM 引脚波形 | 看 PWM 频率、互补输出、死区是否正确 |
| MOS 上下桥驱动波形  | 看有没有上下管直通风险          |
| 三相半桥输出 U/V/W | 看逆变桥输出是否正常           |
| 电流采样放大器输出    | 看采样波形有没有尖峰、偏置错误      |
| ADC 触发点      | 看是否采在 PWM 噪声最小的位置    |
| 母线电压纹波       | 看电源是否扛得住             |
| 栅极波形         | 看 MOS 管开关速度、振铃、过冲    |

尤其是 SVPWM 驱动三相桥，**最怕上下桥直通**。这个虚拟示波器完全看不到。

---

## 3. SVPWM 调试时，虚拟示波器能帮你验证什么？

比如 SVPWM 最终输出三个占空比：

```c
PWM_A = 0.5 + something;
PWM_B = 0.5 + something;
PWM_C = 0.5 + something;
```

正常情况下你应该看到：

```text
duty_a、duty_b、duty_c 三个波形互相错开 120°
占空比在 0~100% 之间变化
三相 duty 不应该突然跳变
```

例如电角度 `theta_e` 从 0 到 2π 变化时：

```text
duty_a:  高 -> 中 -> 低 -> 中
duty_b:  中 -> 高 -> 中 -> 低
duty_c:  低 -> 中 -> 高 -> 中
```

如果你在虚拟示波器上看到：

```text
duty_a = 0
duty_b = 1
duty_c = 0
```

或者突然跳变、超过 1、小于 0，那说明你的 SVPWM 算法有问题。

---

## 4. 但是它看不到“PWM实际输出”

虚拟示波器看到的是：

```c
pwm_duty_a = 0.62;
pwm_duty_b = 0.31;
pwm_duty_c = 0.07;
```

但是 MCU 实际引脚可能是：

```text
TIM 配错了
通道没使能
互补输出没开
死区没配置
极性反了
PWM 频率错了
定时器没启动
GPIO 复用错了
```

这些虚拟示波器都看不出来。

所以虚拟示波器只能证明：

> 软件算出来的 duty 是对的。

不能证明：

> 实际功率板输出是对的。

---

## 5. 做 FOC / SVPWM 最少需要哪些工具？

### 最低配置

可以这样：

```text
虚拟示波器 + 万用表 + 串口日志
```

能学习算法、空载小功率测试，但风险比较大。

### 推荐配置

```text
虚拟示波器 + 双通道真实示波器 + 万用表 + 限流电源
```

如果预算允许，真实示波器最好有：

```text
带宽：50MHz 以上
通道：2 通道起步，4 通道更好
采样率：500MSa/s 或 1GSa/s 更好
探头：普通 10x 探头
```

做三相电机驱动，**4 通道示波器更舒服**，因为你可以同时看：

```text
PWM_A
PWM_B
PWM_C
ADC采样触发点 / 电流采样信号
```

---

## 6. 实际调试建议

你可以按这个顺序来：

### 第一步：不接电机，只看软件变量

用虚拟示波器看：

```c
theta_e
sin(theta_e)
cos(theta_e)
duty_a
duty_b
duty_c
```

确认 SVPWM 三相 duty 正常。

---

### 第二步：不接电机，只给驱动板低压供电

用真实示波器看：

```text
MCU PWM 引脚
MOS 驱动输入
上下桥互补 PWM
死区时间
```

确认没有上下管同时导通。

---

### 第三步：低压限流接电机

比如用：

```text
12V
限流 1A~2A
```

先做开环拖动，看电机能不能平稳转。

---

### 第四步：再闭环 FOC

看：

```c
id
iq
id_ref
iq_ref
theta_e
speed
```

这时候虚拟示波器非常有用。

---

## 7. 结论

做 FOC / SVPWM：

```text
虚拟示波器：可以用，但主要看软件变量。
真实示波器：强烈建议有，主要看硬件波形。
```

最实用的组合是：

```text
虚拟示波器看 FOC 变量：
id、iq、theta、电角度、duty

真实示波器看功率波形：
PWM、死区、相电压、电流采样、ADC触发点
```

你现在如果刚开始学，可以先用虚拟示波器验证 SVPWM 算法；但只要接三相桥和电机，最好还是上真实示波器，特别是检查**互补 PWM 和死区时间**。

-----
-------
--------

可以，**双通道示波器可以做 FOC/SVPWM 调试**，尤其是你作为入门学习、低压小功率电机驱动，双通道够用。

但它的缺点是：**一次只能同时看两个信号**，三相电机很多地方需要分批测。

---

## 1. 双通道最常用的看法

### 场景 1：看两路 PWM 是否互补、有死区

比如看 A 相上下桥：

```text
CH1：AH 高边 PWM
CH2：AL 低边 PWM
```

你要确认：

```text
AH 和 AL 不能同时为高
AH 关断后，AL 延迟一点再开
AL 关断后，AH 延迟一点再开
```

这个延迟就是**死区时间**。

这是双通道非常重要的用途。

---

### 场景 2：看 PWM 和电流采样

```text
CH1：PWM_A
CH2：电流采样放大器输出
```

你可以看：

```text
电流采样有没有尖峰
ADC 采样点是不是避开了 PWM 开关噪声
电流采样偏置是否正常
```

---

### 场景 3：看两相 PWM 相位关系

```text
CH1：PWM_A
CH2：PWM_B
```

可以粗略看三相 PWM 的变化是否正常。

但因为只有两个通道，不能同时看 A/B/C 三相。

---

## 2. 双通道不方便的地方

FOC/SVPWM 里经常想同时看：

```text
PWM_A
PWM_B
PWM_C
ADC 触发信号
电流采样信号
母线电压
```

双通道一次只能看两个，所以你要分批测：

```text
第一次：PWM_A + PWM_B
第二次：PWM_A + PWM_C
第三次：AH + AL
第四次：PWM_A + 电流采样
第五次：PWM_A + ADC触发
```

这能调，但是效率比 4 通道低。

---

## 3. 如果预算有限，双通道怎么选？

建议最低不要买太差的玩具示波器。

比较推荐的最低配置：

```text
带宽：50 MHz 以上
采样率：500 MSa/s 或 1 GSa/s 更好
通道：2 通道
探头：10x 探头
触发：边沿触发稳定
```

如果你只是看低频 PWM，比如：

```text
PWM频率：10 kHz ~ 30 kHz
死区：几百 ns ~ 1 us
```

那么双通道 50 MHz 起步基本能用。

如果你想更舒服一点：

```text
100 MHz 双通道
1 GSa/s
```

会更合适。

---

## 4. 双通道够不够看死区？

够。

比如 STM32 高级定时器 TIM1/TIM8 输出互补 PWM：

```text
PA8  ：TIM1_CH1   上桥 AH
PB13 ：TIM1_CH1N  下桥 AL
```

你用示波器这样接：

```text
CH1 探头尖：PA8
CH1 地夹：GND

CH2 探头尖：PB13
CH2 地夹：GND
```

然后看两个波形之间有没有一小段“都为低”的时间。

理想状态：

```text
AH:  ─────── HIGH ─────── LOW  ─────── HIGH
AL:  LOW ───────────── HIGH ───────── LOW
              ↑死区↑        ↑死区↑
```

只要没有这种情况就行：

```text
AH:  ───── HIGH ─────
AL:      ───── HIGH ─────
          ↑重叠↑  危险
```

上下桥同时导通就可能炸 MOS。

---

## 5. 做 FOC 入门，我建议你这样配

你可以先买：

```text
双通道真实示波器
虚拟示波器/串口波形工具
万用表
限流电源
```

这套就能开始做：

```text
SVPWM 输出验证
互补 PWM 死区验证
开环转动
电流采样检查
速度环/电流环调参
```

---

## 6. 结论

**双通道可以。**

对于你现在学 FOC / SVPWM：

```text
双通道示波器：够入门，够调低压小功率电机
四通道示波器：更舒服，更适合复杂调试和工程产品
```

优先级我建议这样：

```text
先买一个靠谱双通道示波器
不要买特别便宜、带宽虚标严重的玩具示波器
配合虚拟示波器看软件变量
低压限流慢慢调
```

真正最关键的是：**先用双通道确认上下桥互补 PWM 和死区时间正常，再接电机。**

----
----
-----
可以。你学 FOC 调电机，建议分成 **4 类信号**来看：

```text
1. 软件算法变量
2. MCU PWM 输出
3. 功率驱动硬件波形
4. 电机/整车控制状态
```

做平衡车的话，要求比普通“让电机转起来”高很多，因为它需要：

```text
电机能正反转
低速力矩大
响应快
控制平滑
刹车可靠
不能失控冲出去
```

---

# 一、学 FOC 最需要看的东西

## 1. 先看软件变量：用虚拟示波器最合适

这些是 FOC 里面最核心的变量：

```c
float ia, ib, ic;        // 三相电流
float i_alpha, i_beta;   // Clarke 变换后的电流
float id, iq;            // Park 变换后的 d/q 轴电流

float id_ref;            // d轴目标电流
float iq_ref;            // q轴目标电流

float theta_e;           // 电角度
float speed_rpm;         // 电机转速

float u_d, u_q;          // 电流环PI输出
float u_alpha, u_beta;   // 反Park后的电压矢量

float duty_a, duty_b, duty_c; // SVPWM输出三相占空比
```

你用虚拟示波器主要看这些：

| 变量              | 要看什么            |
| --------------- | --------------- |
| `theta_e`       | 电角度是否连续、有没有跳变   |
| `speed_rpm`     | 速度是否稳定、有没有剧烈抖动  |
| `ia/ib/ic`      | 电流采样是否正常        |
| `id/iq`         | FOC 电流环是否稳定     |
| `id_ref/iq_ref` | 目标电流是否合理        |
| `duty_a/b/c`    | SVPWM 三相占空比是否正常 |
| `u_d/u_q`       | PI 输出有没有饱和      |

对于表贴式 PMSM/BLDC，常见控制目标是：

```text
id_ref = 0
iq_ref = 根据力矩需求变化
```

简单理解：

```text
id：一般不希望它很大
iq：主要负责出力、扭矩
```

平衡车真正需要控制的主要是 `iq`，因为平衡车要的是**电机快速给出正反方向力矩**。

---

# 二、真实示波器需要看什么？

虚拟示波器只能看软件变量，真实示波器要看实际硬件有没有正常工作。

## 1. 先看 MCU PWM 引脚

比如 STM32 用 TIM1/TIM8 做三相互补 PWM：

```text
AH / AL：A相上桥/下桥
BH / BL：B相上桥/下桥
CH / CL：C相上桥/下桥
```

双通道示波器可以这样测：

```text
第一次：
CH1 看 AH
CH2 看 AL

第二次：
CH1 看 BH
CH2 看 BL

第三次：
CH1 看 CH
CH2 看 CL
```

你要确认：

```text
1. PWM 频率正确
2. 上下桥互补
3. 有死区时间
4. 极性没有反
5. 没有上下管同时导通
```

最重要的是这个：

```text
AH 和 AL 不能同时为高
BH 和 BL 不能同时为高
CH 和 CL 不能同时为高
```

一旦上下桥同时导通，就叫**直通**，很容易烧 MOS、烧驱动、烧电源。

---

## 2. 看死区时间

比如 A 相：

```text
CH1：AH
CH2：AL
```

你应该看到：

```text
AH:  ─────── 高 ─────── 低  ─────── 高
AL:  低 ───────────── 高 ───────── 低
              ↑死区↑        ↑死区↑
```

死区就是上下管切换时，中间留一点时间，防止两个 MOS 同时导通。

如果看到这样就危险：

```text
AH:  ───── 高 ─────
AL:      ───── 高 ─────
          ↑重叠↑
```

这个重叠就是直通风险。

---

## 3. 看电流采样波形

FOC 非常依赖电流采样。

常见采样方式有：

```text
1. 三电阻采样
2. 双电阻采样
3. 单电阻母线采样
```

你要看：

```text
电流采样信号有没有明显尖峰
静止时偏置是否在中点
ADC采样点有没有避开PWM开关噪声
采样值有没有饱和
```

比如你的电流采样放大器输出范围是 0~3.3V，那么静止时可能在中间：

```text
无电流时：1.65V 左右
正向电流：高于 1.65V
反向电流：低于 1.65V
```

如果静止时就是：

```text
0V
3.3V
乱跳
```

那电流采样肯定不对。

---

## 4. 看母线电压

平衡车电机启动、刹车、急转时，母线电压会波动。

你要看：

```text
电池电压有没有塌陷
刹车时母线电压有没有升高
电源纹波大不大
驱动板有没有复位
```

平衡车尤其要注意**再生制动**。

电机被拖动或者急刹车时，它会像发电机一样把能量灌回母线，母线电压可能升高。如果没有处理好，可能导致：

```text
驱动过压保护
电源芯片复位
MOS过压
电池保护板动作
系统突然断电
```

---

# 三、FOC 调试阶段应该按什么顺序看？

不要一上来就闭环 FOC + 接大电机 + 上车测试，这样很危险。

建议流程是：

```text
第一阶段：不接电机，只看 PWM
第二阶段：接低压限流电源，不装轮子，开环拖动
第三阶段：验证电流采样
第四阶段：闭环电流环
第五阶段：速度环
第六阶段：平衡车整车控制
```

---

# 四、第一阶段：不接电机，只检查 PWM

这时候你主要看：

```text
PWM频率
三相输出
互补输出
死区
使能脚
刹车脚
驱动芯片故障脚
```

常见 PWM 频率：

```text
10kHz ~ 30kHz
```

做平衡车，我建议：

```text
PWM频率：16kHz / 20kHz / 24kHz
```

因为频率太低会有明显电机啸叫，频率太高 MOS 开关损耗变大。

---

# 五、第二阶段：开环拖动，看电机能不能转

开环测试就是你不做复杂闭环，只给一个旋转电压矢量。

你要看：

```text
电机能否平稳转
正反转是否正常
有没有严重抖动
电流是否过大
低速是否卡顿
```

这时候虚拟示波器看：

```text
theta_e_openloop
duty_a
duty_b
duty_c
bus_voltage
```

真实示波器看：

```text
PWM输出
相电压
电流采样波形
```

如果开环都转不稳，先不要搞闭环 FOC。

---

# 六、第三阶段：验证角度

FOC 最核心的是角度。

你要搞清楚你用什么方式获得转子角度：

```text
1. 霍尔传感器
2. 编码器
3. 磁编码器
4. 无感算法
```

对于平衡车，我更建议你用：

```text
有感方案：霍尔 / 编码器 / 磁编码器
```

不太建议初学直接上无感 FOC 做平衡车。

原因是平衡车最需要的是：

```text
低速大扭矩
零速保持
快速正反转
原地小幅度修正
```

而无感 FOC 在低速/零速时最难，因为反电动势很小，角度估算困难。

所以平衡车更适合：

```text
有感 FOC
或者霍尔六步
或者有刷电机 + 编码器
```

如果是无刷电机做平衡车，最好电机带霍尔或者编码器。

---

# 七、第四阶段：看电流环

FOC 最关键的闭环是电流环。

你要看：

```text
id_ref 和 id
iq_ref 和 iq
```

理想状态：

```text
id 能跟上 id_ref
iq 能跟上 iq_ref
```

比如你给：

```text
id_ref = 0
iq_ref = 2A
```

那你希望看到：

```text
id ≈ 0A
iq ≈ 2A
```

如果出现：

```text
id 很大
iq 抖动
电流环震荡
u_d/u_q 饱和
```

说明你的角度、电流采样、PI 参数、SVPWM、电机参数里面有问题。

---

# 八、第五阶段：看速度环

速度环一般在电流环外面。

结构大概是：

```text
速度目标 speed_ref
        ↓
速度PI
        ↓
iq_ref
        ↓
电流环FOC
        ↓
电机输出力矩
```

也就是说：

```text
速度环输出的是 iq_ref
iq_ref 决定电机力矩
```

你要看：

```text
speed_ref
speed_meas
iq_ref
iq
```

如果速度环太猛，平衡车会：

```text
猛冲
急停
抖动
震荡
```

如果速度环太软，平衡车会：

```text
反应慢
站不住
推一下恢复很慢
```

---

# 九、做平衡车，还要看 IMU 数据

平衡车不是只靠电机控制，它的核心还有 IMU 姿态检测。

你至少要看：

```c
float acc_x, acc_y, acc_z;
float gyro_x, gyro_y, gyro_z;

float pitch;        // 车体前后倾角
float pitch_rate;   // 倾角速度
```

对于两轮平衡车，最关键的是：

```text
pitch：前后倾角
pitch_rate：前后倾角速度
```

一般控制逻辑类似：

```text
车往前倒 → 电机往前加速追上去
车往后倒 → 电机往后加速追上去
```

也就是：

```text
电机不是为了“保持不动”
而是为了“追上车体重心”
```

---

# 十、平衡车控制需要看哪些变量？

平衡车建议你用虚拟示波器同时看这些：

```c
float pitch;          // 倾角
float pitch_rate;     // 角速度
float balance_output; // 平衡环输出

float wheel_speed_l;  // 左轮速度
float wheel_speed_r;  // 右轮速度

float speed_ref;      // 目标速度
float speed_feedback; // 实际速度
float speed_output;   // 速度环输出

float turn_ref;       // 转向目标
float turn_output;    // 转向输出

float iq_ref_l;       // 左轮目标力矩电流
float iq_ref_r;       // 右轮目标力矩电流

float iq_l;           // 左轮实际q轴电流
float iq_r;           // 右轮实际q轴电流
```

核心关系可以理解成：

```text
平衡环输出 + 速度环输出 + 转向环输出
最后变成左右轮的 iq_ref
```

---

# 十一、平衡车的控制框架

一个常见结构是：

```text
                 IMU
                  ↓
          pitch / pitch_rate
                  ↓
              平衡环
                  ↓
              基础力矩输出
                  ↓
     ┌────────────┴────────────┐
     ↓                         ↓
  左轮 iq_ref               右轮 iq_ref
     ↓                         ↓
 左轮FOC电流环             右轮FOC电流环
     ↓                         ↓
 左电机输出力矩            右电机输出力矩
```

如果加上速度和转向，就是：

```text
平衡输出 balance_out
速度输出 speed_out
转向输出 turn_out
```

左右轮目标可以这样合成：

```c
iq_ref_left  = balance_out + speed_out - turn_out;
iq_ref_right = balance_out + speed_out + turn_out;
```

简单理解：

```text
平衡输出：负责不倒
速度输出：负责前进/后退
转向输出：负责左转/右转
```

---

# 十二、平衡车对电机驱动的要求

## 1. 要能低速大扭矩

平衡车很多时候不是高速跑，而是在很低速状态下不断修正。

所以电机驱动要支持：

```text
低速有力
堵转不失控
正反转切换快
电流控制稳定
```

这也是为什么**有感 FOC 更适合平衡车**。

---

## 2. 要响应快

平衡车倒得很快，如果控制慢了就摔。

你需要关注：

```text
IMU采样频率
姿态解算频率
平衡控制频率
电流环频率
PWM频率
```

可以参考：

```text
PWM频率：16kHz ~ 25kHz
电流环频率：10kHz ~ 20kHz
速度环频率：500Hz ~ 1kHz
平衡环频率：200Hz ~ 1kHz
IMU采样：500Hz ~ 1kHz
```

初学可以先这样：

```text
PWM：20kHz
电流环：10kHz
平衡环：500Hz
速度环：100Hz~200Hz
```

---

## 3. 要有限流保护

平衡车很容易出现大电流：

```text
上车瞬间
急加速
急刹车
卡住轮子
撞到障碍物
```

所以必须有：

```text
母线电流限制
相电流限制
iq_ref 限幅
PWM占空比限幅
过温保护
欠压保护
过压保护
```

例如：

```c
if (iq_ref > IQ_MAX) {
    iq_ref = IQ_MAX;
}

if (iq_ref < -IQ_MAX) {
    iq_ref = -IQ_MAX;
}
```

---

## 4. 要有失控保护

平衡车必须判断什么时候不能继续输出。

比如：

```text
倾角超过 30°~45°
IMU数据异常
电池电压太低
电机堵转
电流过大
驱动芯片报错
遥控/通信丢失
```

一旦异常，需要：

```text
关PWM
刹车
蜂鸣器报警
等待重新扶正
```

不要车已经倒了还疯狂给电机输出。

---

# 十三、你具体调试时应该看什么？

我给你按阶段列一个清单。

---

## 阶段 1：PWM/SVPWM 阶段

看这些：

```text
duty_a
duty_b
duty_c
PWM_AH / PWM_AL
PWM_BH / PWM_BL
PWM_CH / PWM_CL
deadtime
```

目标：

```text
三相占空比正常
上下桥互补
有死区
没有直通
```

---

## 阶段 2：电流采样阶段

看这些：

```text
ia_raw
ib_raw
ic_raw
ia
ib
ic
current_offset_a
current_offset_b
current_offset_c
```

目标：

```text
静止时电流接近 0
ADC raw 不乱跳
电流偏置正确
电流方向正确
```

重点是方向。

如果你给正向 `iq_ref`，结果电机反向用力，或者电流环发散，很可能是：

```text
电流采样方向反了
相序错了
角度方向反了
```

---

## 阶段 3：角度阶段

看这些：

```text
theta_mech
theta_elec
pole_pairs
zero_offset
direction
```

目标：

```text
机械角度连续
电角度连续
方向正确
零位偏移正确
```

电角度和机械角度关系：

```text
电角度 = 机械角度 × 极对数 + 零位偏移
```

例如电机是 7 对极：

```text
机械转一圈
电角度转 7 圈
```

---

## 阶段 4：电流环阶段

看这些：

```text
id_ref
id
iq_ref
iq
u_d
u_q
```

目标：

```text
id 跟随 id_ref
iq 跟随 iq_ref
电流不振荡
PI输出不长期饱和
```

如果 `id` 很大，通常说明：

```text
角度不准
电流采样有问题
Park变换方向错
相序错
```

---

## 阶段 5：速度环阶段

看这些：

```text
speed_ref
speed_meas
speed_error
iq_ref
```

目标：

```text
速度能跟随
不会来回震荡
加减速平滑
```

---

## 阶段 6：平衡环阶段

看这些：

```text
pitch
pitch_rate
balance_output
speed_output
turn_output
iq_ref_left
iq_ref_right
```

目标：

```text
车前倾时，电机往前追
车后倾时，电机往后追
扶正附近不剧烈抖动
轻推后能恢复
```

---

# 十四、平衡车建议你怎么选电机反馈？

## 不推荐初学方案

```text
无感 FOC + 平衡车
```

原因：

```text
零速难
低速难
启动难
堵转难
正反转频繁时难
```

平衡车经常工作在：

```text
0 rpm 附近
正反转切换
小角度来回修正
```

这正好是无感 FOC 最难的区域。

---

## 推荐方案 1：有霍尔 BLDC + FOC/六步

优点：

```text
低速能知道大概位置
启动简单
成本低
比纯无感可靠
```

缺点：

```text
霍尔分辨率低
低速力矩波动比编码器大
```

---

## 推荐方案 2：编码器/磁编码器 + FOC

优点：

```text
角度准确
低速控制好
力矩平滑
适合平衡车
```

缺点：

```text
成本高
结构复杂
安装要对准
```

如果你真想把平衡车做得比较稳，我建议：

```text
BLDC/PMSM + 磁编码器 + FOC
```

或者：

```text
有刷直流电机 + 编码器 + 电流环/速度环
```

有刷电机其实对初学平衡车更友好。

---

# 十五、平衡车最低硬件要求

做平衡车，你至少需要：

```text
1. MCU
2. 两个电机驱动
3. 两个电机
4. IMU
5. 电池
6. 电流采样
7. 电压采样
8. 过流/过压保护
9. 急停/使能控制
```

如果用 FOC，还需要：

```text
10. 电机位置反馈，最好霍尔/编码器
```

---

# 十六、示波器怎么看比较实际？

你双通道示波器可以这样用。

## 测 A 相上下桥

```text
CH1：A相上桥 PWM
CH2：A相下桥 PWM
```

目的：

```text
确认互补和死区
```

---

## 测 PWM 和电流采样

```text
CH1：PWM_A
CH2：A相电流采样输出
```

目的：

```text
看采样噪声
看ADC采样点是否合适
```

---

## 测母线电压和 PWM

```text
CH1：母线电压
CH2：PWM_A
```

目的：

```text
看加速/刹车时母线是否波动很大
```

注意：如果母线电压高，普通探头和示波器地线不能乱夹，避免短路。

---

## 测霍尔/编码器

```text
CH1：霍尔A
CH2：霍尔B
```

或者：

```text
CH1：编码器A
CH2：编码器B
```

目的：

```text
看方向
看脉冲是否正常
看有没有丢脉冲
```

---

# 十七、平衡车最关键的调试现象

## 1. 车往前倒，轮子也应该往前跑

这个逻辑非常重要。

如果车往前倒，轮子却往后转，那一定会直接摔。

所以你要验证：

```text
前倾 → 电机向前输出
后倾 → 电机向后输出
```

这一步可以先把车架悬空，不上人。

---

## 2. 轻轻推车，电机会反抗

正常现象：

```text
你把车身往前压
轮子会往前转，试图把车拉回来

你把车身往后压
轮子会往后转，试图把车拉回来
```

如果它顺着倒下去，说明反馈方向错了。

---

## 3. 上电不要立刻大力输出

平衡车上电时建议：

```text
检测车身角度
角度在安全范围内才使能电机
慢慢软启动
```

比如：

```c
if (fabsf(pitch) < 10.0f) {
    motor_enable = true;
} else {
    motor_enable = false;
}
```

不要车躺地上还输出力矩。

---

# 十八、推荐你调平衡车的顺序

你可以按这个顺序做：

```text
1. 单电机开环转动
2. 单电机电流闭环
3. 单电机速度闭环
4. 两个电机速度闭环
5. 读取 IMU，算 pitch
6. 悬空测试平衡输出方向
7. 小功率限流落地测试
8. 加速度环/转向环
9. 上人测试
```

一开始不要上人，先让车架在支架上测试。

---

# 十九、FOC + 平衡车你最该重点掌握的几个点

你优先学这些：

```text
1. 三相逆变桥原理
2. 互补PWM和死区
3. ADC电流采样
4. Clarke/Park变换
5. SVPWM输出
6. 电角度和机械角度
7. 电流环PI
8. 速度环PI
9. IMU姿态解算
10. 平衡环PD控制
```

对于平衡车来说，FOC 只是底层。

真正整车是：

```text
IMU姿态控制 + 速度控制 + 转向控制 + 电机力矩控制
```

---

# 二十、最简平衡车控制关系

你可以先这样理解：

```c
// 1. 平衡环：车倒多少、倒多快
balance_out = Kp_angle * pitch + Kd_angle * pitch_rate;

// 2. 速度环：希望车不要越跑越远
speed_out = Kp_speed * (speed_ref - speed_meas);

// 3. 转向环：左右轮差速
turn_out = Kp_turn * turn_ref;

// 4. 合成左右轮目标力矩
iq_ref_left  = balance_out + speed_out - turn_out;
iq_ref_right = balance_out + speed_out + turn_out;
```

最后：

```text
iq_ref_left  → 左电机 FOC
iq_ref_right → 右电机 FOC
```

也就是说：

```text
平衡车最终控制的不是“速度”
而是两个电机的“力矩”
```

而在 FOC 里面，力矩主要对应：

```text
iq 电流
```

---

# 总结

你学 FOC 调电机，主要看：

```text
PWM：频率、互补、死区
电流：ia/ib/ic、id/iq
角度：机械角度、电角度、方向、零位
SVPWM：duty_a/b/c
电流环：id_ref/id、iq_ref/iq
速度环：speed_ref/speed
```

做平衡车还要额外看：

```text
IMU倾角 pitch
角速度 pitch_rate
平衡环输出
左右轮速度
左右轮 iq_ref
母线电压
电流限制
故障保护
```

对平衡车来说，最重要的一句话是：

```text
FOC负责让电机“听话地输出力矩”；
平衡算法负责决定“现在该输出多大的力矩、往哪个方向输出”。
```

如果你是初学，强烈建议路线是：

```text
有感电机/编码器电机 + FOC
低压限流调试
先单电机，再双电机
先悬空，再落地
先不上人，再小心上人
```

不要一开始就搞：

```text
无感FOC + 大功率电机 + 直接上车
```

这个难度和风险都比较高。

-------
--------
-------
可以用，**Hantek 6254BD/BC/BE 这种四通道 USB 示波器，拿来学 FOC、SVPWM、平衡车电机调试是可以的**。它比普通双通道手持示波器更适合你，因为 **4 通道**对三相电机调试很有用。

但我不建议你把它当成“完全等价于台式示波器”的东西，尤其是调功率板、MOS 栅极、母线电压时要注意安全。

---

## 1. 这个型号大概是什么水平？

我查到 Hantek 6000 系列官方资料写的是：**4 个模拟通道、1GSa/s 实时采样率、250MHz 带宽、USB2.0 供电**，支持触发、波形记录、光标测量等功能。([汉特科技][1])

部分 6254BD 销售页面也标注为：

```text
4通道
250MHz带宽
1GSa/s实时采样率
带信号发生器/AWG
```

([亚马逊日本][2])

所以从参数上看，**看 STM32/ESP32 的 PWM、霍尔、编码器、SPI/IIC/UART、ADC采样波形，是够用的**。

---

# 2. 用来学 FOC/SVPWM：可以

做 FOC/SVPWM，你主要看这些低频到中频信号：

```text
PWM频率：10kHz ~ 30kHz
死区时间：几百 ns ~ 几 us
霍尔信号：几十 Hz ~ 几 kHz
编码器信号：几 kHz ~ 几百 kHz
ADC电流采样：PWM同步采样
```

对于这些，6254BD/BC/BE 这种 4 通道 USB 示波器基本够用。

尤其你可以同时看：

```text
CH1：A相上桥 PWM
CH2：A相下桥 PWM
CH3：B相上桥 PWM
CH4：B相下桥 PWM
```

或者：

```text
CH1：PWM_A
CH2：PWM_B
CH3：PWM_C
CH4：ADC触发信号
```

这比双通道舒服很多。

---

# 3. 对平衡车调试有什么用？

平衡车电机调试，你可以用它看：

## 看三相 PWM

```text
CH1：PWM_A
CH2：PWM_B
CH3：PWM_C
CH4：ADC触发/电流采样
```

用途：

```text
确认三相PWM是否正常
确认SVPWM占空比有没有跳变
确认ADC采样点是否在PWM中间
```

---

## 看上下桥互补和死区

```text
CH1：A相上桥 AH
CH2：A相下桥 AL
```

或者四通道同时看两相：

```text
CH1：AH
CH2：AL
CH3：BH
CH4：BL
```

重点看：

```text
上下桥有没有同时导通
死区时间够不够
PWM极性有没有反
```

这是电机驱动最重要的检查之一。

---

## 看霍尔/编码器

如果你的平衡车电机带霍尔：

```text
CH1：Hall A
CH2：Hall B
CH3：Hall C
CH4：PWM_A 或 电流采样
```

如果是编码器：

```text
CH1：Encoder A
CH2：Encoder B
CH3：Index Z
CH4：PWM/方向信号
```

可以判断：

```text
方向是否正确
是否丢脉冲
霍尔相序是否正确
编码器A/B相有没有反
```

---

## 看电流采样

```text
CH1：PWM_A
CH2：电流采样 A
CH3：电流采样 B
CH4：ADC触发
```

看：

```text
电流采样有没有尖峰
采样偏置是不是在中点
ADC采样点是否避开开关噪声
```

如果你电流采样是 0~3.3V 输入，静止无电流时一般应该在中点附近：

```text
约 1.65V
```

---

# 4. 但是有一个很重要的问题：USB示波器接电脑，不一定适合乱测功率板

这个要特别注意。

USB 示波器通过 USB 连接电脑，很多情况下示波器地、USB地、电脑地、电源地可能是连在一起的。

所以你不能随便把探头地夹夹到：

```text
三相半桥输出 U/V/W
高边MOS源极
母线正极
浮地节点
市电相关电路
```

否则可能短路。

安全测法是：

```text
探头地夹只接系统 GND / 电池负极 / 控制板地
探头尖去测 MCU PWM、驱动输入、霍尔、编码器、电流采样输出
```

不建议你直接用普通探头去测：

```text
高边栅极对源极 Vgs
半桥相线对地的高压尖峰
母线高压侧浮动波形
```

如果要测这些，最好用：

```text
差分探头
隔离探头
电流钳
台式隔离方案
```

尤其是平衡车电池可能是：

```text
24V / 36V / 48V
```

虽然电压不算特别高，但三相桥开关尖峰、地夹短路风险依然存在。

---

# 5. BD / BC / BE 怎么选？

你说的：

```text
Hantek 6254BD / 6254BC / 6254BE
```

不同销售页面对配置写法可能不完全一致。大体你要重点确认这几个参数：

```text
1. 是不是 4 通道
2. 带宽是不是 250MHz
3. 实时采样率是不是 1GSa/s
4. 每通道同时打开时采样率会不会下降
5. 有没有信号发生器
6. 支不支持 Windows 10/11
7. 安卓软件是否稳定
8. 探头是不是配齐，最好有 10x 探头
```

如果价格差不大，我更建议选：

```text
6254BD
```

原因是很多页面明确写它是：

```text
4通道 + 250MHz + 1GSa/s + 信号发生器
```

([电路专家][3])

但买之前你一定要看商家详情页，确认是不是正品、是不是完整套装、是否支持你电脑系统。

---

# 6. 它适合你目前做什么？

适合：

```text
学习 FOC / SVPWM
看 STM32 TIM1/TIM8 PWM
看三相互补PWM
看死区
看霍尔信号
看编码器AB相
看UART/SPI/IIC基本波形
看电流采样放大器输出
看ADC触发时序
看平衡车IMU中断/通信时序
```

不太适合直接裸测：

```text
高边MOS Vgs
三相相线大电流开关尖峰
功率母线高频纹波精确分析
非常严肃的EMI/开关损耗分析
```

---

# 7. 我的建议

如果你预算有限、主要是自己学 FOC 和做平衡车原型：

```text
可以买。
```

它的优点是：

```text
4通道很适合三相电机
参数比普通低端手持示波器强
价格通常比台式4通道便宜
便携
能配合电脑大屏看波形
```

缺点是：

```text
依赖电脑/软件
旋钮操作不如台式示波器顺手
USB接地安全要特别注意
触发和响应体验可能不如正规台式示波器
安卓端体验不要抱太高期望
```

---

## 最终结论

**Hantek 6254BD/BC/BE 这种四通道 USB 示波器，可以作为你学习 FOC、SVPWM、平衡车电机驱动的入门/中级工具。**

但你要记住一句话：

```text
它适合看控制信号和低压采样信号；
不要随便拿普通探头去测高边MOS和浮动功率节点。
```

你买回来后，优先用它测这些：

```text
1. MCU三相PWM
2. 上下桥互补PWM
3. 死区时间
4. 霍尔/编码器信号
5. 电流采样输出
6. ADC触发点
```

这几个能测清楚，对你做 FOC 和平衡车已经非常有帮助。

[1]: https://www.hantek.com/m/productDetail/9163?utm_source=chatgpt.com "Hantek6004BC Series -"
[2]: https://www.amazon.co.jp/-/en/Osciloscopio-Oscilloscope-4-Channel-Oscillograph-Generator/dp/B01N6LQX0Z?utm_source=chatgpt.com "Hantek 6254bd Osciloscopio PC Based Handheld ..."
[3]: https://www.circuitspecialists.com/hantek6254bd_250mhz_4_channel_pc_oscilloscope?srsltid=AfmBOorsZ1xFYCYp3KCyObWPDW_SguEZ_CQ8HCgBhIhCIsWKZP34B4Ch&utm_source=chatgpt.com "Hantek 6254BD 4 Ch 250 MHz USB Oscilloscope"

-----
------
------
可以买，但你这个说法要先纠正一下：

**“4位半”主要是万用表/DMM的指标，不是示波器的核心指标。**

示波器买不买，主要看：

```text
通道数：2通道 / 4通道
带宽：50MHz / 100MHz / 250MHz
采样率：500MSa/s / 1GSa/s
存储深度：越大越好
触发功能：越稳定越好
是否隔离：调电机功率板很重要
```

如果你是为了 **FOC + SVPWM + 平衡车电机驱动**，我更建议你优先买：

```text
4通道示波器 > 2通道示波器
台式示波器 > USB虚拟示波器
100MHz左右 > 50MHz
```

### [RIGOL DHO804 70MHz 4通道示波器]()

#### 更推荐

*JP¥58,850*

### [Hantek 6254BD 4通道 USB 示波器]()

#### 预算选择

*JP¥35,000*

### [OWON SDS1104 100MHz 4通道示波器]()

#### 性价比

*JP¥41,000*

### [RIGOL DS1104Z Plus 100MHz 4通道示波器]()

#### 经典型号

*JP¥61,760*

### [RIGOL DS1054Z 4通道示波器]()

#### 老牌经典

*JP¥62,700*

### [Hantek DSO4104B/4204B/4254B 4通道台式示波器]()

#### 汉泰克台式

*JP¥60,000*

## 我给你的结论

你如果预算够，我更推荐：

```text
第一推荐：RIGOL DHO804
第二推荐：RIGOL DS1104Z Plus / DS1054Z
第三推荐：Hantek 6254BD
```

---

# 1. 如果你说的是“4位半万用表”，可以买，但不是替代示波器

4位半万用表一般是这种显示能力：

```text
19999 counts
或者 20000 counts
```

它适合测：

```text
电池电压
母线电压
5V/3.3V电源
电阻
电流
电机相间电阻大概值
电源芯片输出
```

但是它看不了：

```text
PWM波形
死区时间
MOS驱动波形
电流采样尖峰
编码器AB相波形
霍尔信号时序
SVPWM三相输出
```

所以做 FOC，**万用表必须有，但不能替代示波器**。

---

# 2. 做 FOC，示波器建议买什么规格？

最低建议：

```text
通道：2通道
带宽：50MHz以上
采样率：500MSa/s以上
探头：10x探头
```

更推荐：

```text
通道：4通道
带宽：100MHz左右
采样率：1GSa/s左右
```

做平衡车/三相电机，**4通道真的很有用**。

你可以同时看：

```text
CH1：PWM_A
CH2：PWM_B
CH3：PWM_C
CH4：ADC触发 / 电流采样
```

或者：

```text
CH1：A相上桥AH
CH2：A相下桥AL
CH3：B相上桥BH
CH4：B相下桥BL
```

这样检查互补 PWM 和死区很方便。

---

# 3. Hantek 6254BD 可以买吗？

可以，尤其是你预算有限。

优点：

```text
4通道
参数看起来比较高
价格比台式4通道便宜
适合看MCU PWM、霍尔、编码器、电流采样
电脑大屏看波形舒服
```

缺点：

```text
依赖电脑/软件
操作手感不如台式示波器
触发体验一般不如力科/泰克/普源/鼎阳
USB接电脑，地线安全要特别小心
```

如果你买 Hantek 6254BD，我建议你主要拿它看：

```text
MCU PWM输出
驱动芯片输入PWM
霍尔信号
编码器AB相
电流采样放大器输出
ADC触发点
UART/SPI/IIC基础波形
```

不要乱测：

```text
高边MOS的Vgs
三相半桥浮动节点
母线高压尖峰
市电相关电路
```

测功率板高边波形最好要差分探头。

---

# 4. 我更推荐 RIGOL DHO804 的原因

如果预算能上到 5~6 万日元左右，我个人更建议你考虑：

```text
RIGOL DHO804
```

原因：

```text
4通道
台式机，操作更顺手
12bit垂直分辨率
看电流采样、电源纹波更舒服
不依赖电脑
触发和保存波形更方便
```

它带宽只有 70MHz，但对你现在做：

```text
FOC
SVPWM
STM32 PWM
ESP32调试
平衡车电机驱动
霍尔/编码器
UART/SPI/IIC
```

已经够用了。

FOC 的 PWM 一般也就：

```text
10kHz ~ 30kHz
```

你真正需要的是稳定触发、多通道、好操作，而不是盲目追求 250MHz。

---

# 5. 不建议买太便宜的“手持玩具示波器”

那种几百块人民币的手持小示波器，有些标着：

```text
100MHz
500MSa/s
示波器+万用表
```

但很多实际体验一般，问题常见：

```text
触发不稳
波形刷新慢
存储深度小
噪声大
旋钮/菜单难用
探头质量差
测死区不放心
```

学简单 PWM 可以，但做 FOC 功率板调试，我不太建议你作为主力。

---

# 6. 你应该怎么买？

## 预算比较紧

选：

```text
Hantek 6254BD
```

适合：

```text
想要4通道
主要看MCU控制信号
能接受电脑软件操作
预算有限
```

---

## 想买一个更稳妥的长期工具

选：

```text
RIGOL DHO804
```

适合：

```text
做FOC
做平衡车
做ESP32/STM32调试
想长期用
想操作方便
```

---

## 想要经典耐用

选：

```text
RIGOL DS1104Z Plus
```

这个是比较经典的 4 通道 100MHz 示波器，但相比 DHO804，分辨率和新功能不一定占优势。

---

# 7. 我的最终建议

你现在用途是：

```text
学FOC
调SVPWM
做平衡车
调STM32/ESP32
看PWM/霍尔/编码器/电流采样
```

我建议优先级：

```text
第一选择：RIGOL DHO804
第二选择：Hantek 6254BD
第三选择：RIGOL DS1104Z Plus / DS1054Z
```

如果你预算只有三四万日元左右，**Hantek 6254BD可以买**。

如果你预算能到五六万日元，**直接上 RIGOL DHO804 更舒服**。

另外万用表也要买一个，但不用太夸张：

```text
4位半万用表 + 4通道示波器
```

这个组合对你做 FOC/平衡车比较合适。

-------
-------
--------
在 FOC 里面，**SPWM 和 SVPWM 都属于最后一步：把 FOC 算出来的电压矢量，变成三相 PWM 占空比**。

整个 FOC 大流程可以理解成：

```text
采样三相电流 ia/ib/ic
        ↓
Clarke 变换：ia/ib/ic → iα/iβ
        ↓
Park 变换：iα/iβ → id/iq
        ↓
PI 电流环：id/iq 和 id_ref/iq_ref 比较
        ↓
得到 ud/uq
        ↓
反 Park：ud/uq → uα/uβ
        ↓
调制算法：uα/uβ → PWM_A/PWM_B/PWM_C
        ↓
TIM1/TIM8 输出三相互补 PWM
        ↓
三相逆变桥驱动电机
```

**SPWM 和 SVPWM 的区别就在这里：**

```text
uα/uβ → 三相 PWM 占空比
```

---

# 1. SPWM 是什么？

SPWM 全称是：

```text
Sinusoidal PWM
正弦脉宽调制
```

简单理解：

> 用三个相差 120° 的正弦波，分别生成 A/B/C 三相 PWM 占空比。

例如：

```c
Ua = sin(theta);
Ub = sin(theta - 120°);
Uc = sin(theta + 120°);
```

然后把 `-1 ~ +1` 的正弦值映射成 `0 ~ 1` 的 PWM 占空比：

```c
duty_a = 0.5f + 0.5f * Ua;
duty_b = 0.5f + 0.5f * Ub;
duty_c = 0.5f + 0.5f * Uc;
```

比如：

```text
Ua =  1.0  → duty_a = 100%
Ua =  0.0  → duty_a = 50%
Ua = -1.0  → duty_a = 0%
```

所以 SPWM 的思想很直观：

```text
正弦波高 → PWM占空比大
正弦波低 → PWM占空比小
```

---

# 2. SVPWM 是什么？

SVPWM 全称是：

```text
Space Vector PWM
空间矢量脉宽调制
```

它不是直接从三条正弦波出发，而是从**三相逆变桥的开关状态**出发。

三相桥有 6 个 MOS：

```text
A相：上管 AH，下管 AL
B相：上管 BH，下管 BL
C相：上管 CH，下管 CL
```

每一相上管/下管不能同时开，所以一相有两个状态：

```text
上管开，下管关 → 1
上管关，下管开 → 0
```

三相组合起来有：

```text
2 × 2 × 2 = 8 种开关状态
```

分别是：

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

SVPWM 的核心思想是：

> 你想要一个电压矢量，就用相邻两个有效矢量 + 零矢量，在一个 PWM 周期内按时间比例合成出来。

也就是：

```text
目标电压矢量 = T1时间的矢量1 + T2时间的矢量2 + T0时间的零矢量
```

所以 SVPWM 更像是：

```text
从逆变桥开关状态的角度，合成一个旋转电压矢量
```

而 SPWM 更像是：

```text
从三相正弦波角度，生成三路 PWM
```

---

# 3. FOC 里面 SPWM 和 SVPWM 的位置

FOC 算到后面会得到：

```c
u_alpha;
u_beta;
```

这两个量表示：

```text
定子坐标系下，你希望逆变器输出的电压矢量
```

接下来有两种做法。

## 用 SPWM

```text
uα/uβ → 反 Clarke 得到 Ua/Ub/Uc → 三相 duty
```

流程是：

```text
uα/uβ
  ↓
三相电压 Ua/Ub/Uc
  ↓
正弦 PWM 占空比 duty_a/b/c
```

## 用 SVPWM

```text
uα/uβ → 判断扇区 → 计算 T1/T2/T0 → 三相 duty
```

流程是：

```text
uα/uβ
  ↓
判断在哪个扇区
  ↓
计算相邻两个矢量作用时间
  ↓
分配零矢量时间
  ↓
得到 duty_a/b/c
```

---

# 4. SPWM 和 SVPWM 的核心区别

| 对比项     | SPWM    | SVPWM     |
| ------- | ------- | --------- |
| 思想来源    | 三相正弦波   | 三相逆变桥空间矢量 |
| 实现难度    | 简单      | 稍复杂       |
| 直观程度    | 很直观     | 初学稍难      |
| 母线电压利用率 | 较低      | 更高        |
| 输出电压能力  | 低一些     | 高一些       |
| 谐波表现    | 一般      | 更好        |
| 电机转矩脉动  | 稍大      | 更小        |
| 工程应用    | 入门/简单控制 | FOC 主流方案  |
| 适合平衡车   | 能用      | 更推荐       |

---

# 5. 母线电压利用率区别

这是 SPWM 和 SVPWM 最重要的区别之一。

假设你的母线电压是：

```text
Vbus = 24V
```

SPWM 不能把母线电压用得特别满。

SVPWM 可以比 SPWM 多利用大约：

```text
15.5%
```

也就是说，在同样 24V 电池下，SVPWM 可以输出更大的有效相电压。

这对电机有什么好处？

```text
同样电池电压下，电机最高转速更高
同样速度下，电流压力更小
高速区更不容易电压饱和
力矩输出更充分
```

对于平衡车，低速力矩更重要，但母线利用率高也有好处，尤其是急加速、急刹、爬坡时。

---

# 6. SPWM 的优点

## 优点 1：简单好理解

SPWM 很适合初学。

你只需要知道：

```text
三相正弦波相差 120°
正弦值映射成 PWM 占空比
```

伪代码非常简单：

```c
duty_a = 0.5f + 0.5f * sin(theta);
duty_b = 0.5f + 0.5f * sin(theta - 2π/3);
duty_c = 0.5f + 0.5f * sin(theta + 2π/3);
```

---

## 优点 2：调试方便

你用虚拟示波器看 `duty_a/b/c`，应该能看到三条类似正弦变化的曲线。

```text
A、B、C 三相互差 120°
```

很容易判断有没有错。

---

## 优点 3：适合入门开环测试

比如你刚开始做 BLDC 开环拖动，用 SPWM 也能让电机转起来。

---

# 7. SPWM 的缺点

## 缺点 1：母线电压利用率低

同样 24V 电池，SPWM 输出能力比 SVPWM 低。

这会导致：

```text
高速区更早进入电压饱和
同样目标转速下需要更大电流
电机最高转速略低
```

---

## 缺点 2：不是最适合三相桥的调制方式

SPWM 是从“三条正弦波”角度设计的，不是从三相逆变桥实际开关矢量设计的。

所以在三相逆变器里面，SVPWM 更自然、更工程化。

---

## 缺点 3：转矩脉动和谐波表现不如 SVPWM

实际电机运行时，SVPWM 通常更平滑一些。

---

# 8. SVPWM 的优点

## 优点 1：母线电压利用率高

SVPWM 比 SPWM 能多利用约 15.5% 的直流母线电压。

对你的平衡车来说，意味着：

```text
同样电池，电机输出能力更强
急加速时更有余量
高速时不容易电压不够
爬坡时更有优势
```

---

## 优点 2：更适合 FOC

FOC 最后得到的是一个空间电压矢量：

```text
uα/uβ
```

SVPWM 正好就是根据这个空间矢量来生成 PWM，所以它和 FOC 天然匹配。

---

## 优点 3：电机运行更平滑

SVPWM 的输出矢量分布更合理，转矩脉动相对更小。

---

## 优点 4：工程里最常用

大多数成熟 FOC 电机库里，最终调制都更偏向 SVPWM。

例如：

```text
电动车控制器
伺服驱动器
无人机电调
平衡车驱动器
工业变频器
```

---

# 9. SVPWM 的缺点

## 缺点 1：理解难度高一点

它涉及：

```text
空间矢量
六个扇区
T1/T2/T0
零矢量分配
三相桥开关状态
```

初学比 SPWM 难一些。

---

## 缺点 2：代码实现更容易写错

常见错误包括：

```text
扇区判断错
T1/T2 算错
三相占空比分配错
相序错
电角度方向反
占空比超过 0~1
```

---

## 缺点 3：调试时不如 SPWM 直观

SPWM 三相 duty 看起来就是三条正弦波。

SVPWM 的三相 duty 看起来有点像：

```text
正弦波 + 零序分量
```

所以初学看到它可能感觉“不像标准正弦”。

但这是正常的。

---

# 10. SPWM 和 SVPWM 本质关系

这里有个重点：

**SVPWM 可以理解成在 SPWM 的基础上，注入了一定的零序分量。**

也就是说，SVPWM 输出的三相电压不是简单的：

```text
Ua
Ub
Uc
```

而是：

```text
Ua + Uzero
Ub + Uzero
Uc + Uzero
```

这个 `Uzero` 对三相线电压没有影响，但可以让占空比更合理，使母线利用率更高。

所以你可以这样理解：

```text
SPWM：三条标准正弦波
SVPWM：三条正弦波整体上下移动，让母线电压用得更满
```

这个理解对写代码很有用。

---

# 11. SPWM 的实现方法：基于 uα/uβ

FOC 后面一般得到：

```c
u_alpha;
u_beta;
```

先通过反 Clarke 得到三相电压：

```c
Ua = u_alpha;
Ub = -0.5f * u_alpha + 0.8660254f * u_beta;
Uc = -0.5f * u_alpha - 0.8660254f * u_beta;
```

这里的：

```c
0.8660254 = sqrt(3) / 2
```

然后根据母线电压归一化：

```c
duty_a = 0.5f + Ua / Vbus;
duty_b = 0.5f + Ub / Vbus;
duty_c = 0.5f + Uc / Vbus;
```

最后限幅：

```c
duty_a = limit(duty_a, 0.0f, 1.0f);
duty_b = limit(duty_b, 0.0f, 1.0f);
duty_c = limit(duty_c, 0.0f, 1.0f);
```

---

# 12. STM32 SPWM 伪代码

假设你用 TIM1 输出三相 PWM：

```text
TIM1_CH1  → A相
TIM1_CH2  → B相
TIM1_CH3  → C相
TIM1_CH1N → A相下桥
TIM1_CH2N → B相下桥
TIM1_CH3N → C相下桥
```

伪代码：

```c
#define SQRT3_BY_2   0.86602540378f

typedef struct
{
    float duty_a;
    float duty_b;
    float duty_c;
} pwm_duty_t;

static float limit_float(float x, float min, float max)
{
    if (x > max) return max;
    if (x < min) return min;
    return x;
}

/*
 * SPWM 调制
 *
 * 输入：
 *   u_alpha, u_beta：FOC 反Park之后的 αβ 电压
 *   vbus：母线电压
 *
 * 输出：
 *   三相 PWM 占空比，范围 0.0 ~ 1.0
 */
pwm_duty_t spwm_generate(float u_alpha, float u_beta, float vbus)
{
    pwm_duty_t out;

    float ua;
    float ub;
    float uc;

    /*
     * 反 Clarke：
     * αβ 电压 → 三相电压
     */
    ua = u_alpha;
    ub = -0.5f * u_alpha + SQRT3_BY_2 * u_beta;
    uc = -0.5f * u_alpha - SQRT3_BY_2 * u_beta;

    /*
     * 转成占空比
     *
     * 这里简单理解：
     *   0.5 表示 PWM 中点
     *   ua/vbus 表示在中点基础上增加或减少
     */
    out.duty_a = 0.5f + ua / vbus;
    out.duty_b = 0.5f + ub / vbus;
    out.duty_c = 0.5f + uc / vbus;

    /*
     * 限幅，防止超过 0~100%
     */
    out.duty_a = limit_float(out.duty_a, 0.0f, 1.0f);
    out.duty_b = limit_float(out.duty_b, 0.0f, 1.0f);
    out.duty_c = limit_float(out.duty_c, 0.0f, 1.0f);

    return out;
}

/*
 * 更新 STM32 定时器 PWM
 *
 * arr 是定时器自动重装值
 * duty = 0.0 ~ 1.0
 */
void pwm_update_tim1(pwm_duty_t duty)
{
    uint32_t arr = __HAL_TIM_GET_AUTORELOAD(&htim1);

    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, (uint32_t)(duty.duty_a * arr));
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, (uint32_t)(duty.duty_b * arr));
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_3, (uint32_t)(duty.duty_c * arr));
}
```

---

# 13. SVPWM 的实现方法一：传统扇区法

传统 SVPWM 步骤是：

```text
1. 根据 uα/uβ 判断当前电压矢量在哪个扇区
2. 根据扇区计算 T1、T2、T0
3. 根据扇区分配 Ta、Tb、Tc
4. 得到 duty_a/b/c
5. 更新 TIM CCR
```

空间矢量分成 6 个扇区：

```text
       β
       ↑
   V3  |  V2
       |
V4 ----+---- V1 → α
       |
   V5  |  V6
```

每 60° 一个扇区：

```text
Sector 1：0°   ~ 60°
Sector 2：60°  ~ 120°
Sector 3：120° ~ 180°
Sector 4：180° ~ 240°
Sector 5：240° ~ 300°
Sector 6：300° ~ 360°
```

---

# 14. SVPWM 的实现方法二：零序注入法，更适合初学写代码

实际工程里还有一种更简单的 SVPWM 写法，叫：

```text
零序注入法
min-max SVPWM
```

它不用显式判断扇区，也不用手动算 T1/T2/T0。

流程是：

```text
1. uα/uβ 反 Clarke 得到 Ua/Ub/Uc
2. 找到三相中的最大值 Umax
3. 找到三相中的最小值 Umin
4. 注入零序电压 Uzero = -0.5 * (Umax + Umin)
5. 三相都加上 Uzero
6. 再转换成占空比
```

也就是：

```c
Uzero = -0.5f * (Umax + Umin);

Ua = Ua + Uzero;
Ub = Ub + Uzero;
Uc = Uc + Uzero;
```

这就是 SVPWM 的一种简单实现方式。

你可以理解成：

```text
把三相电压整体上下平移
让最大值和最小值尽量居中
从而更充分利用 0~Vbus 的 PWM 范围
```

这个方法非常适合你先写 STM32 FOC。

---

# 15. STM32 SVPWM 零序注入伪代码

```c
#define SQRT3_BY_2   0.86602540378f

typedef struct
{
    float duty_a;
    float duty_b;
    float duty_c;
} pwm_duty_t;

static float limit_float(float x, float min, float max)
{
    if (x > max) return max;
    if (x < min) return min;
    return x;
}

static float max3(float a, float b, float c)
{
    float m = a;

    if (b > m) m = b;
    if (c > m) m = c;

    return m;
}

static float min3(float a, float b, float c)
{
    float m = a;

    if (b < m) m = b;
    if (c < m) m = c;

    return m;
}

/*
 * SVPWM：零序注入法 / min-max 法
 *
 * 输入：
 *   u_alpha, u_beta：FOC 反Park之后的 αβ 电压
 *   vbus：母线电压
 *
 * 输出：
 *   三相 PWM 占空比
 */
pwm_duty_t svpwm_generate_minmax(float u_alpha, float u_beta, float vbus)
{
    pwm_duty_t out;

    float ua;
    float ub;
    float uc;

    float u_max;
    float u_min;
    float u_zero;

    /*
     * 第一步：反 Clarke
     *
     * 得到三相电压指令
     */
    ua = u_alpha;
    ub = -0.5f * u_alpha + SQRT3_BY_2 * u_beta;
    uc = -0.5f * u_alpha - SQRT3_BY_2 * u_beta;

    /*
     * 第二步：找到三相中的最大值和最小值
     */
    u_max = max3(ua, ub, uc);
    u_min = min3(ua, ub, uc);

    /*
     * 第三步：计算零序注入量
     *
     * 这个量会同时加到 A/B/C 三相上。
     * 它不会改变线电压 Uab/Ubc/Uca，
     * 但能提高母线电压利用率。
     */
    u_zero = -0.5f * (u_max + u_min);

    /*
     * 第四步：三相同时加上零序分量
     */
    ua += u_zero;
    ub += u_zero;
    uc += u_zero;

    /*
     * 第五步：转换成 PWM 占空比
     *
     * 中心对齐 PWM 下：
     *   0.5 是中点
     *   ua/vbus 是正负偏移量
     */
    out.duty_a = 0.5f + ua / vbus;
    out.duty_b = 0.5f + ub / vbus;
    out.duty_c = 0.5f + uc / vbus;

    /*
     * 第六步：占空比限幅
     */
    out.duty_a = limit_float(out.duty_a, 0.0f, 1.0f);
    out.duty_b = limit_float(out.duty_b, 0.0f, 1.0f);
    out.duty_c = limit_float(out.duty_c, 0.0f, 1.0f);

    return out;
}
```

更新 STM32 PWM：

```c
void pwm_update_tim1(pwm_duty_t duty)
{
    uint32_t arr = __HAL_TIM_GET_AUTORELOAD(&htim1);

    /*
     * 如果使用中心对齐 PWM：
     * CCR = duty * ARR
     *
     * TIM1 自动根据比较值产生 PWM。
     */
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, (uint32_t)(duty.duty_a * arr));
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, (uint32_t)(duty.duty_b * arr));
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_3, (uint32_t)(duty.duty_c * arr));
}
```

---

# 16. FOC 主循环伪代码：带 SVPWM

下面是一个完整一点的 FOC 执行流程伪代码。

```c
void foc_current_loop_isr(void)
{
    /*
     * 这个函数一般在 PWM 定时器中断或者 ADC 注入转换完成中断中运行。
     * 频率一般是 10kHz ~ 20kHz。
     */

    float ia;
    float ib;
    float ic;

    float i_alpha;
    float i_beta;

    float id;
    float iq;

    float ud;
    float uq;

    float u_alpha;
    float u_beta;

    float theta_e;

    pwm_duty_t pwm;

    /*
     * 1. 读取 ADC 电流采样
     */
    ia = current_get_phase_a();
    ib = current_get_phase_b();
    ic = current_get_phase_c();

    /*
     * 2. 获取电角度
     *
     * 有感 FOC：
     *   来自编码器/霍尔/磁编码器
     *
     * 无感 FOC：
     *   来自观测器/PLL/反电动势估算
     */
    theta_e = motor_get_electrical_angle();

    /*
     * 3. Clarke 变换
     *
     * 三相电流 → αβ 坐标
     */
    clarke_transform(ia, ib, ic, &i_alpha, &i_beta);

    /*
     * 4. Park 变换
     *
     * αβ 坐标 → dq 坐标
     */
    park_transform(i_alpha, i_beta, theta_e, &id, &iq);

    /*
     * 5. 电流 PI 环
     *
     * id_ref 一般为 0
     * iq_ref 来自速度环/平衡环
     */
    ud = pi_controller(&id_pi, id_ref - id);
    uq = pi_controller(&iq_pi, iq_ref - iq);

    /*
     * 6. 电压限幅
     *
     * 防止 ud/uq 超过母线电压能力
     */
    voltage_limit(&ud, &uq, vbus);

    /*
     * 7. 反 Park 变换
     *
     * dq 电压 → αβ 电压
     */
    inv_park_transform(ud, uq, theta_e, &u_alpha, &u_beta);

    /*
     * 8. SVPWM 或 SPWM
     */
#if USE_SVPWM
    pwm = svpwm_generate_minmax(u_alpha, u_beta, vbus);
#else
    pwm = spwm_generate(u_alpha, u_beta, vbus);
#endif

    /*
     * 9. 更新 PWM 占空比
     */
    pwm_update_tim1(pwm);
}
```

---

# 17. Clarke/Park 伪代码补充

为了让你看清楚完整链路，我把变换也写一下。

## Clarke 变换

```c
void clarke_transform(float ia, float ib, float ic,
                      float *i_alpha, float *i_beta)
{
    /*
     * 常见简化写法：
     * 假设 ia + ib + ic = 0
     */
    *i_alpha = ia;
    *i_beta  = (ia + 2.0f * ib) * 0.57735026919f;
}
```

其中：

```c
0.57735026919 = 1 / sqrt(3)
```

---

## Park 变换

```c
void park_transform(float i_alpha, float i_beta, float theta,
                    float *id, float *iq)
{
    float sin_t = sinf(theta);
    float cos_t = cosf(theta);

    /*
     * αβ 静止坐标 → dq 旋转坐标
     */
    *id =  i_alpha * cos_t + i_beta * sin_t;
    *iq = -i_alpha * sin_t + i_beta * cos_t;
}
```

---

## 反 Park 变换

```c
void inv_park_transform(float ud, float uq, float theta,
                        float *u_alpha, float *u_beta)
{
    float sin_t = sinf(theta);
    float cos_t = cosf(theta);

    /*
     * dq 旋转坐标 → αβ 静止坐标
     */
    *u_alpha = ud * cos_t - uq * sin_t;
    *u_beta  = ud * sin_t + uq * cos_t;
}
```

---

# 18. SPWM 和 SVPWM 波形看起来有什么区别？

## SPWM 的 duty 波形

三相 duty 大概像：

```text
duty_a：正弦
duty_b：正弦，滞后120°
duty_c：正弦，超前120°
```

你在虚拟示波器上看会很舒服：

```text
三条圆滑正弦波
```

---

## SVPWM 的 duty 波形

SVPWM 看起来不是特别标准的正弦。

它像是：

```text
正弦波 + 上下平移的零序分量
```

所以你会看到三相 duty 有点“压顶”或者“平台感”。

这不是错，反而是 SVPWM 正常表现。

---

# 19. 用 SPWM 还是 SVPWM？

## 学习阶段

可以先用：

```text
SPWM
```

因为它简单，容易看懂。

你先让电机开环转起来，理解：

```text
三相正弦 → PWM → 旋转磁场
```

---

## 真正做 FOC

建议用：

```text
SVPWM
```

因为它更适合 FOC 和三相逆变桥。

---

## 做平衡车

更建议：

```text
SVPWM + 有感 FOC
```

原因：

```text
电压利用率更高
低速力矩控制更好
FOC输出更平滑
工程上更主流
```

平衡车不是只要电机转，而是要：

```text
低速大力矩
快速正反转
小角度频繁修正
堵转附近也能控制
```

所以我建议你最终目标用：

```text
磁编码器/霍尔 + FOC + SVPWM
```

---

# 20. 两种方法的调试重点

## SPWM 调试重点

看：

```text
duty_a/duty_b/duty_c 是否三相相差 120°
占空比是否在 0~1
电机是否平稳转
正反转方向是否正确
```

常见问题：

```text
相序错
角度方向反
占空比限幅不对
电压给太大导致电流过大
```

---

## SVPWM 调试重点

看：

```text
duty_a/duty_b/duty_c 是否平滑
有没有超过 0~1
有没有突然跳变
PWM_A/B/C 是否正常
电流是否比 SPWM 平滑
```

常见问题：

```text
零序注入公式写错
u_alpha/u_beta 尺度不对
vbus 单位不对
电压限幅过大
相序和角度方向不匹配
```

---

# 21. 一个非常重要的细节：FOC 里面不要一开始给太大 uq

不管 SPWM 还是 SVPWM，调试时都要小心。

例如开环测试：

```c
ud = 0.0f;
uq = 1.0f;  // 先给很小
```

不要一上来给：

```c
uq = 12.0f;
```

尤其是你用 24V/36V/48V 电池时，很容易过流。

建议：

```text
低压电源
限流
空载
小电压
慢慢加
```

---

# 22. TIM1/TIM8 配置要点

STM32 做三相 FOC，一般用高级定时器：

```text
TIM1 或 TIM8
```

需要配置：

```text
中心对齐 PWM
三相 PWM 输出
互补输出 CH1N/CH2N/CH3N
死区时间
刹车输入 BKIN
ADC 触发点
更新中断
```

大概伪代码：

```c
void motor_pwm_start(void)
{
    /*
     * 启动三相上桥 PWM
     */
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_3);

    /*
     * 启动三相下桥互补 PWM
     */
    HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_1);
    HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_2);
    HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_3);

    /*
     * 如果有 ADC 同步采样，也要启动 ADC 触发
     */
    HAL_ADCEx_InjectedStart_IT(&hadc1);
}
```

---

# 23. 最终总结

SPWM：

```text
优点：
简单、直观、适合入门、代码容易写

缺点：
母线电压利用率低，输出能力不如 SVPWM，工程性能略差
```

SVPWM：

```text
优点：
母线利用率高，输出能力强，电机运行更平滑，更适合 FOC

缺点：
理解稍难，实现细节容易写错
```

对你做 FOC 和平衡车，我建议路线是：

```text
先用 SPWM 理解三相正弦和 PWM 输出
再切到 SVPWM/min-max 零序注入法
最终用 SVPWM 做平衡车电机驱动
```

如果只记住一句话：

```text
SPWM 是“用三条正弦波生成 PWM”；
SVPWM 是“用三相桥的空间矢量合成目标电压”，母线利用率更高，更适合 FOC。
```

实际 STM32 里最终都是做同一件事：

```c
__HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, duty_a * arr);
__HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, duty_b * arr);
__HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_3, duty_c * arr);
```

区别只是：

```text
duty_a/duty_b/duty_c 是怎么计算出来的。
```



--------
-------

------
下面我直接用一个**无感 FOC 开环速度控制案例**讲。

你先记住一句话：

```text
FOC 前面负责算出“我要给电机一个多大的旋转电压矢量”
SPWM/SVPWM 负责把这个电压矢量变成三相 PWM 占空比
```

也就是说：

```text
FOC 算法输出：
u_alpha
u_beta

SPWM/SVPWM 输入：
u_alpha
u_beta

SPWM/SVPWM 输出：
duty_a
duty_b
duty_c
```

---

# 1. 无感开环 FOC 速度控制是什么？

严格来说，**开环速度控制还不是真正完整的无感闭环 FOC**。

因为它不估算真实转子角度，只是自己造一个旋转角度：

```c
theta += speed_target * dt;
```

然后让电压矢量按照这个角度转。

简单理解：

```text
我不管电机真实转到哪里了
我自己按设定速度生成一个旋转电角度
然后按这个角度输出三相 PWM
希望电机跟着这个旋转磁场跑
```

这就像你拿着磁铁绕着电机转，希望转子跟上。

---

# 2. 开环 FOC 速度控制大流程

先看整体伪代码：

```c
while (1)
{
    // 1. 设定目标速度，单位可以是 rad/s
    speed_target = 100.0f;

    // 2. 根据目标速度累加电角度
    theta_e += speed_target * dt;

    // 3. 限制 theta_e 在 0 ~ 2π
    theta_e = normalize_angle(theta_e);

    // 4. 设定电压
    ud = 0.0f;
    uq = 2.0f;

    // 5. 反 Park 变换：dq 电压 -> alpha/beta 电压
    inverse_park(ud, uq, theta_e, &u_alpha, &u_beta);

    // 6. 调制算法：二选一
    if (use_spwm)
    {
        spwm(u_alpha, u_beta, vbus, &duty_a, &duty_b, &duty_c);
    }
    else
    {
        svpwm(u_alpha, u_beta, vbus, &duty_a, &duty_b, &duty_c);
    }

    // 7. 更新 STM32 三相 PWM
    pwm_update(duty_a, duty_b, duty_c);

    delay_us(100);
}
```

你会发现，**SPWM 和 SVPWM 只有第 6 步不同**。

前面的：

```c
theta_e
ud
uq
u_alpha
u_beta
```

都是一样的。

---

# 3. 先讲开环 FOC 里的 ud / uq

在 FOC 里面常见两个电压：

```c
ud;  // d轴电压
uq;  // q轴电压
```

对初学可以这样理解：

```text
ud：一般先给 0
uq：负责让电机转起来
```

所以开环拖动电机时，一般这样写：

```c
ud = 0.0f;
uq = 2.0f;
```

如果母线电压是 24V，`uq = 2V` 就是先给一个比较小的电压。

不要一开始给太大，比如：

```c
uq = 12.0f;
```

容易大电流、抖动甚至烧驱动。

---

# 4. 反 Park 变换在干什么？

你现在有：

```c
ud = 0;
uq = 2;
theta_e = 不断增加的角度;
```

然后通过反 Park 变换得到：

```c
u_alpha;
u_beta;
```

伪代码：

```c
void inverse_park(float ud, float uq, float theta,
                  float *u_alpha, float *u_beta)
{
    float sin_t = sinf(theta);
    float cos_t = cosf(theta);

    *u_alpha = ud * cos_t - uq * sin_t;
    *u_beta  = ud * sin_t + uq * cos_t;
}
```

由于 `theta_e` 一直增加，所以 `u_alpha/u_beta` 就形成了一个旋转电压矢量。

简单理解：

```text
theta_e 不断变大
        ↓
u_alpha/u_beta 跟着旋转
        ↓
三相 PWM 跟着变化
        ↓
电机磁场旋转
        ↓
转子跟着转
```

---

# 5. SPWM 实现方式：最直观

## SPWM 的思想

SPWM 就是：

```text
把 u_alpha/u_beta 先变成三相正弦电压 Ua/Ub/Uc
再把 Ua/Ub/Uc 变成三相 PWM 占空比
```

也就是：

```text
u_alpha/u_beta
      ↓
Ua / Ub / Uc
      ↓
duty_a / duty_b / duty_c
```

---

## SPWM 伪代码

```c
void spwm(float u_alpha, float u_beta, float vbus,
          float *duty_a, float *duty_b, float *duty_c)
{
    float ua;
    float ub;
    float uc;

    // 1. alpha/beta 电压转成三相电压
    ua = u_alpha;
    ub = -0.5f * u_alpha + 0.8660254f * u_beta;
    uc = -0.5f * u_alpha - 0.8660254f * u_beta;

    // 2. 三相电压转成 PWM 占空比
    // 0.5 表示占空比中心
    *duty_a = 0.5f + ua / vbus;
    *duty_b = 0.5f + ub / vbus;
    *duty_c = 0.5f + uc / vbus;

    // 3. 限幅，不能小于0，也不能大于1
    *duty_a = limit(*duty_a, 0.0f, 1.0f);
    *duty_b = limit(*duty_b, 0.0f, 1.0f);
    *duty_c = limit(*duty_c, 0.0f, 1.0f);
}
```

---

## SPWM 输出长什么样？

如果你把 `duty_a/duty_b/duty_c` 用虚拟示波器画出来，会看到：

```text
duty_a：正弦变化
duty_b：正弦变化，和 A 相差 120°
duty_c：正弦变化，和 A 相差 240°
```

大概像这样：

```text
A相：    /‾‾‾\____/‾‾‾\____
B相：__/‾‾‾\____/‾‾‾\____/
C相：‾\____/‾‾‾\____/‾‾‾\
```

所以 SPWM 很好理解：

```text
三条正弦波控制三相 PWM
```

---

# 6. SVPWM 实现方式：更会利用母线电压

## SVPWM 的直观理解

SVPWM 不只是简单生成三条正弦波。

它会做一件事：

```text
把三相电压整体上下移动一下
让三相占空比更充分地利用 0% ~ 100% 范围
```

这个“整体上下移动”的量叫：

```c
u_zero
```

也可以理解成：

```text
零序电压注入
```

注意：

```text
A/B/C 三相同时加同一个 u_zero
不会改变电机真正看到的线电压
但可以提高母线电压利用率
```

为什么不会改变？

因为电机主要看的是相与相之间的电压，比如：

```text
Uab = Ua - Ub
Ubc = Ub - Uc
Uca = Uc - Ua
```

如果三相都加同一个量：

```text
Ua_new = Ua + u_zero
Ub_new = Ub + u_zero
Uc_new = Uc + u_zero
```

那么：

```text
Uab_new = Ua_new - Ub_new
        = (Ua + u_zero) - (Ub + u_zero)
        = Ua - Ub
```

所以电机实际看到的线电压没变。

---

## SVPWM 简单实现：min-max 法

这个方法最适合你现在理解和写代码。

步骤：

```text
1. alpha/beta 转三相 ua/ub/uc
2. 找出三相最大值 u_max
3. 找出三相最小值 u_min
4. 计算 u_zero = -0.5 * (u_max + u_min)
5. ua/ub/uc 都加上 u_zero
6. 转成 duty_a/b/c
```

---

## SVPWM 伪代码

```c
void svpwm(float u_alpha, float u_beta, float vbus,
           float *duty_a, float *duty_b, float *duty_c)
{
    float ua;
    float ub;
    float uc;

    float u_max;
    float u_min;
    float u_zero;

    // 1. alpha/beta 电压转成三相电压
    ua = u_alpha;
    ub = -0.5f * u_alpha + 0.8660254f * u_beta;
    uc = -0.5f * u_alpha - 0.8660254f * u_beta;

    // 2. 找最大值
    u_max = ua;
    if (ub > u_max) u_max = ub;
    if (uc > u_max) u_max = uc;

    // 3. 找最小值
    u_min = ua;
    if (ub < u_min) u_min = ub;
    if (uc < u_min) u_min = uc;

    // 4. 计算零序注入量
    // 这一步就是 SVPWM 和 SPWM 最核心的区别
    u_zero = -0.5f * (u_max + u_min);

    // 5. 三相同时加上这个零序量
    ua = ua + u_zero;
    ub = ub + u_zero;
    uc = uc + u_zero;

    // 6. 转成占空比
    *duty_a = 0.5f + ua / vbus;
    *duty_b = 0.5f + ub / vbus;
    *duty_c = 0.5f + uc / vbus;

    // 7. 限幅
    *duty_a = limit(*duty_a, 0.0f, 1.0f);
    *duty_b = limit(*duty_b, 0.0f, 1.0f);
    *duty_c = limit(*duty_c, 0.0f, 1.0f);
}
```

---

# 7. SPWM 和 SVPWM 代码差别在哪里？

你看这两段。

SPWM：

```c
ua = u_alpha;
ub = -0.5f * u_alpha + 0.8660254f * u_beta;
uc = -0.5f * u_alpha - 0.8660254f * u_beta;

duty_a = 0.5f + ua / vbus;
duty_b = 0.5f + ub / vbus;
duty_c = 0.5f + uc / vbus;
```

SVPWM：

```c
ua = u_alpha;
ub = -0.5f * u_alpha + 0.8660254f * u_beta;
uc = -0.5f * u_alpha - 0.8660254f * u_beta;

u_max = max(ua, ub, uc);
u_min = min(ua, ub, uc);

u_zero = -0.5f * (u_max + u_min);

ua += u_zero;
ub += u_zero;
uc += u_zero;

duty_a = 0.5f + ua / vbus;
duty_b = 0.5f + ub / vbus;
duty_c = 0.5f + uc / vbus;
```

核心区别就这一句：

```c
u_zero = -0.5f * (u_max + u_min);
```

然后：

```c
ua += u_zero;
ub += u_zero;
uc += u_zero;
```

所以你可以这样记：

```text
SPWM：alpha/beta 转三相后，直接算 duty

SVPWM：alpha/beta 转三相后，先加一个 u_zero，再算 duty
```

---

# 8. 为什么 SVPWM 母线利用率更高？

举个很通俗的例子。

假设三相电压算出来是：

```text
ua =  10V
ub =  -2V
uc =  -8V
```

SPWM 直接拿这三个去算占空比。

但 SVPWM 会先找：

```text
u_max = 10V
u_min = -8V
```

然后：

```text
u_zero = -0.5 * (10 + -8)
       = -0.5 * 2
       = -1V
```

三相都加上 `-1V`：

```text
ua =  9V
ub = -3V
uc = -9V
```

你会发现：

```text
原来范围：-8V ~ 10V
现在范围：-9V ~ 9V
```

它把三相电压尽量居中，让最大值和最小值更对称。

这样 PWM 占空比不容易提前顶到 0% 或 100%。

所以同样的母线电压下，SVPWM 能输出更大的有效线电压。

---

# 9. 完整开环速度控制伪代码：SPWM 版本

下面是一个更完整的版本。

```c
#define PI        3.1415926f
#define TWO_PI    6.2831853f

float theta_e = 0.0f;         // 开环电角度
float speed_e = 100.0f;       // 目标电角速度 rad/s
float uq_open = 2.0f;         // 开环 q轴电压
float ud_open = 0.0f;         // d轴电压一般为0
float vbus = 24.0f;           // 母线电压

void foc_open_loop_spwm_task(void)
{
    float dt = 0.0001f;       // 100us执行一次，也就是10kHz

    float u_alpha;
    float u_beta;

    float duty_a;
    float duty_b;
    float duty_c;

    while (1)
    {
        // 1. 累加电角度
        theta_e += speed_e * dt;

        // 2. 角度限制在 0~2π
        if (theta_e >= TWO_PI)
        {
            theta_e -= TWO_PI;
        }

        // 3. 反 Park 变换
        inverse_park(ud_open, uq_open, theta_e,
                     &u_alpha, &u_beta);

        // 4. 使用 SPWM 生成三相占空比
        spwm(u_alpha, u_beta, vbus,
             &duty_a, &duty_b, &duty_c);

        // 5. 更新 STM32 PWM
        pwm_update(duty_a, duty_b, duty_c);

        // 6. 等待下一个控制周期
        delay_us(100);
    }
}
```

---

# 10. 完整开环速度控制伪代码：SVPWM 版本

你会发现，只有一个地方不同。

```c
#define PI        3.1415926f
#define TWO_PI    6.2831853f

float theta_e = 0.0f;
float speed_e = 100.0f;
float uq_open = 2.0f;
float ud_open = 0.0f;
float vbus = 24.0f;

void foc_open_loop_svpwm_task(void)
{
    float dt = 0.0001f;

    float u_alpha;
    float u_beta;

    float duty_a;
    float duty_b;
    float duty_c;

    while (1)
    {
        // 1. 累加电角度
        theta_e += speed_e * dt;

        // 2. 角度限制
        if (theta_e >= TWO_PI)
        {
            theta_e -= TWO_PI;
        }

        // 3. 反 Park 变换
        inverse_park(ud_open, uq_open, theta_e,
                     &u_alpha, &u_beta);

        // 4. 使用 SVPWM 生成三相占空比
        svpwm(u_alpha, u_beta, vbus,
              &duty_a, &duty_b, &duty_c);

        // 5. 更新 STM32 PWM
        pwm_update(duty_a, duty_b, duty_c);

        delay_us(100);
    }
}
```

对比一下：

SPWM 版本：

```c
spwm(u_alpha, u_beta, vbus,
     &duty_a, &duty_b, &duty_c);
```

SVPWM 版本：

```c
svpwm(u_alpha, u_beta, vbus,
      &duty_a, &duty_b, &duty_c);
```

其他都一样。

---

# 11. STM32 更新 PWM 的伪代码

假设你用 TIM1：

```text
TIM1_CH1  → A相上桥
TIM1_CH2  → B相上桥
TIM1_CH3  → C相上桥

TIM1_CH1N → A相下桥
TIM1_CH2N → B相下桥
TIM1_CH3N → C相下桥
```

更新占空比：

```c
void pwm_update(float duty_a, float duty_b, float duty_c)
{
    uint32_t arr;

    arr = __HAL_TIM_GET_AUTORELOAD(&htim1);

    // 防止 duty 越界
    duty_a = limit(duty_a, 0.0f, 1.0f);
    duty_b = limit(duty_b, 0.0f, 1.0f);
    duty_c = limit(duty_c, 0.0f, 1.0f);

    // 更新三相比较值
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, duty_a * arr);
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, duty_b * arr);
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_3, duty_c * arr);
}
```

启动 PWM：

```c
void motor_pwm_start(void)
{
    // 启动三相主 PWM
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_3);

    // 启动三相互补 PWM
    HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_1);
    HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_2);
    HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_3);
}
```

---

# 12. SPWM 和 SVPWM 优缺点，用人话讲

## SPWM 优点

```text
1. 简单
2. 好理解
3. 代码短
4. 波形直观
5. 适合初学和开环测试
```

你刚开始学 FOC，可以先用 SPWM，因为你容易看懂：

```text
三相 duty 就是三条正弦波
```

---

## SPWM 缺点

```text
1. 母线电压利用率低
2. 同样电池电压下，最高输出能力低一点
3. 高速时更容易电压不够
4. 工程上不如 SVPWM 常用
```

比如你是 24V 电池，SPWM 可能还没把 24V 用满，就已经有某一相占空比顶到 0% 或 100% 了。

---

## SVPWM 优点

```text
1. 母线电压利用率更高
2. 同样 Vbus 下可以输出更大的有效电压
3. 更适合三相逆变桥
4. 更适合 FOC
5. 电机运行通常更平滑
6. 工程产品更常用
```

对平衡车、电动车、无人机电调、伺服驱动这些，SVPWM 更常见。

---

## SVPWM 缺点

```text
1. 比 SPWM 难理解
2. 代码多几步
3. 调试时 duty 波形看起来不像标准正弦
4. 写错零序注入、限幅、相序时容易出问题
```

不过如果你用我上面这个 **min-max 零序注入法**，其实实现并不复杂。

---

# 13. 一个特别简单的记忆方法

你可以这样记：

```text
SPWM：
三相正弦直接上

SVPWM：
三相正弦先整体平移一下，再上
```

对应代码：

```c
// SPWM
ua = ua;
ub = ub;
uc = uc;

// SVPWM
u_zero = -0.5f * (max(ua, ub, uc) + min(ua, ub, uc));

ua += u_zero;
ub += u_zero;
uc += u_zero;
```

所以最核心的区别就是：

```c
u_zero
```

---

# 14. 开环无感 FOC 里面，两个方法实际效果差别

假设你做开环拖动：

```c
speed_e = 100 rad/s;
uq_open = 2V;
vbus = 24V;
```

用 SPWM：

```text
电机可以转
波形容易理解
适合初次验证
```

用 SVPWM：

```text
电机也可以转
同样母线下电压余量更大
以后做闭环 FOC 更合适
```

如果只是让小电机空载转：

```text
SPWM 和 SVPWM 都能用
```

如果你要做真正产品，比如平衡车：

```text
推荐 SVPWM
```

因为平衡车需要：

```text
低速力矩
急加速
急刹车
快速正反转
电压利用率和控制平滑性
```

---

# 15. 开环速度控制注意点

你这个案例是“无感开环速度控制”，要注意：

```text
1. 没有真实转子角度反馈
2. 低速可能抖
3. 负载大了可能失步
4. 起步电压太小转不动
5. 起步电压太大容易过流
6. 不能真正精准控速度
```

所以它适合：

```text
学习 FOC
验证 PWM
验证 SPWM/SVPWM
让电机空载转起来
做无感闭环启动前的开环拖动
```

不适合直接做：

```text
平衡车最终控制
低速大扭矩闭环
零速保持
快速正反转力矩控制
```

平衡车最终最好用：

```text
有感 FOC：霍尔/编码器/磁编码器
```

如果必须无感，难度会大很多，尤其是零速和低速。

---

# 16. 完整代码骨架：可以这样组织

```c
typedef enum
{
    MOD_SPWM,
    MOD_SVPWM
} modulation_t;

typedef struct
{
    float theta_e;
    float speed_e;
    float ud;
    float uq;
    float vbus;
    modulation_t mod;
} motor_open_loop_t;

motor_open_loop_t motor =
{
    .theta_e = 0.0f,
    .speed_e = 100.0f,
    .ud = 0.0f,
    .uq = 2.0f,
    .vbus = 24.0f,
    .mod = MOD_SVPWM,
};

void motor_open_loop_update(motor_open_loop_t *m, float dt)
{
    float u_alpha;
    float u_beta;

    float duty_a;
    float duty_b;
    float duty_c;

    // 1. 开环角度累加
    m->theta_e += m->speed_e * dt;
    m->theta_e = normalize_angle(m->theta_e);

    // 2. dq -> alpha/beta
    inverse_park(m->ud, m->uq, m->theta_e,
                 &u_alpha, &u_beta);

    // 3. 选择 SPWM 或 SVPWM
    if (m->mod == MOD_SPWM)
    {
        spwm(u_alpha, u_beta, m->vbus,
             &duty_a, &duty_b, &duty_c);
    }
    else
    {
        svpwm(u_alpha, u_beta, m->vbus,
              &duty_a, &duty_b, &duty_c);
    }

    // 4. 更新三相 PWM
    pwm_update(duty_a, duty_b, duty_c);
}
```

定时器中断里调用：

```c
void TIM1_UP_IRQHandler(void)
{
    // 清中断标志
    __HAL_TIM_CLEAR_IT(&htim1, TIM_IT_UPDATE);

    // 10kHz 控制周期
    motor_open_loop_update(&motor, 0.0001f);
}
```

---

# 17. 你调试时应该看什么？

虚拟示波器看：

```c
theta_e
u_alpha
u_beta
duty_a
duty_b
duty_c
```

真实示波器看：

```text
PWM_A
PWM_B
PWM_C
A相上桥/下桥互补波形
死区时间
电流采样信号
```

SPWM 下：

```text
duty_a/b/c 比较像三条标准正弦波
```

SVPWM 下：

```text
duty_a/b/c 不是标准正弦，会有零序注入造成的形状变化
这是正常的
```

---

# 18. 最后用一句话总结

**SPWM：**

```text
简单粗暴：把旋转电压矢量变成三条正弦 PWM
适合学习、开环测试
但母线电压利用率低一些
```

**SVPWM：**

```text
更聪明：在三相电压上加一个共同偏移量 u_zero
不改变电机线电压
但能把 PWM 范围用得更满
更适合真正 FOC 电机驱动
```

对你现在学习路线，我建议：

```text
第一步：先用 SPWM 跑通开环转动
第二步：把 spwm() 换成 svpwm()
第三步：对比 duty_a/b/c 波形
第四步：再加入电流采样、电流环、速度环
```

真正做平衡车，最终建议用：

```text
SVPWM + 电流闭环 + 速度/平衡环
```

不要只靠这个“无感开环速度控制”直接上车。


--------
------
-----
你这里说的应该是 **“FOC 开环速度控制的 SVPWM 实现”**，不是“开关速度控制”。我按这个来讲。

行业里真正常用的 FOC + SVPWM，大体是这个流程：

```text
目标速度 speed_ref
        ↓
开环累加电角度 theta_e
        ↓
给定 uq 电压，ud = 0
        ↓
反 Park 变换：ud/uq → u_alpha/u_beta
        ↓
SVPWM：u_alpha/u_beta → duty_a/duty_b/duty_c
        ↓
STM32 TIM1/TIM8 更新 CCR
        ↓
三相半桥输出 PWM
```

注意：**SVPWM 不负责速度控制**。
它只负责最后一步：

```text
把 u_alpha / u_beta 变成三相 PWM 占空比
```

---

# 1. 行业里常用的 SVPWM 实现有两种

## 方法 1：传统扇区法

这个是教科书常讲的：

```text
判断电压矢量在哪个扇区
计算 T1、T2、T0
根据扇区分配 Ta/Tb/Tc
更新 PWM
```

优点：

```text
原理清楚
方便理解六个空间矢量
适合教材和底层研究
```

缺点：

```text
代码分支多
扇区判断容易写错
T1/T2分配容易写错
```

---

## 方法 2：min-max 零序注入法

这个在工程里非常常用，尤其是代码实现上。

它的核心是：

```text
先算出三相电压 ua/ub/uc
找最大值和最小值
注入一个共同偏移量 u_zero
最后算三相 duty
```

这个方法本质上也是 SVPWM，效果和标准 SVPWM 在正常线性调制区基本等价。

优点：

```text
代码短
不需要判断扇区
不需要手算 T1/T2/T0
不容易写错
适合 STM32 实际工程
```

所以你真正写代码时，我更建议你先用 **min-max SVPWM**。

---

# 2. 先看 FOC 开环速度控制整体伪代码

假设你没有编码器，没有霍尔，没有无感观测器，只是开环拖动。

核心思想是：

```text
自己生成一个不断增加的电角度 theta_e
然后让电压矢量按这个角度旋转
电机转子如果跟得上，它就会转起来
```

伪代码：

```c
#define TWO_PI 6.283185307f

typedef struct
{
    float theta_e;       // 开环电角度，单位 rad
    float speed_e_ref;   // 目标电角速度，单位 rad/s

    float ud;            // d轴电压
    float uq;            // q轴电压

    float vbus;          // 母线电压，例如 24V

    float duty_a;
    float duty_b;
    float duty_c;
} motor_t;

motor_t motor;
```

初始化：

```c
void motor_openloop_init(void)
{
    motor.theta_e = 0.0f;

    // 电角速度，先给小一点
    motor.speed_e_ref = 50.0f;   // rad/s

    // 开环 FOC 一般 ud = 0，uq 给一个小电压
    motor.ud = 0.0f;
    motor.uq = 2.0f;             // 2V，先小电压启动

    motor.vbus = 24.0f;
}
```

控制循环：

```c
void foc_openloop_speed_loop(float dt)
{
    float u_alpha;
    float u_beta;

    // 1. 开环累加电角度
    motor.theta_e += motor.speed_e_ref * dt;

    // 2. 限制角度在 0 ~ 2π
    if (motor.theta_e >= TWO_PI)
    {
        motor.theta_e -= TWO_PI;
    }

    // 3. dq 电压反变换到 alpha/beta
    inverse_park(
        motor.ud,
        motor.uq,
        motor.theta_e,
        &u_alpha,
        &u_beta
    );

    // 4. 使用 SVPWM 生成三相占空比
    svpwm_minmax(
        u_alpha,
        u_beta,
        motor.vbus,
        &motor.duty_a,
        &motor.duty_b,
        &motor.duty_c
    );

    // 5. 更新 STM32 三相 PWM
    pwm_update_3phase(
        motor.duty_a,
        motor.duty_b,
        motor.duty_c
    );
}
```

这个就是最核心的结构。

---

# 3. 反 Park 变换：dq → alpha/beta

开环 FOC 里，你一般给：

```c
ud = 0;
uq = 某个小电压;
```

然后通过角度 `theta_e` 旋转成 `u_alpha/u_beta`。

伪代码：

```c
void inverse_park(float ud,
                  float uq,
                  float theta,
                  float *u_alpha,
                  float *u_beta)
{
    float sin_t = sinf(theta);
    float cos_t = cosf(theta);

    /*
     * dq 旋转坐标系 → alpha/beta 静止坐标系
     */
    *u_alpha = ud * cos_t - uq * sin_t;
    *u_beta  = ud * sin_t + uq * cos_t;
}
```

如果 `ud = 0`，那么实际就是：

```c
u_alpha = -uq * sin(theta);
u_beta  =  uq * cos(theta);
```

也就是说：

```text
theta_e 不断增加
u_alpha/u_beta 就形成一个旋转电压矢量
```

---

# 4. 行业常用 SVPWM：min-max 零序注入法

这个方法非常适合你先写 STM32 版本。

## 第一步：alpha/beta 转三相电压

```c
ua = u_alpha;
ub = -0.5 * u_alpha + sqrt(3)/2 * u_beta;
uc = -0.5 * u_alpha - sqrt(3)/2 * u_beta;
```

这一步你可以理解成：

```text
把二维电压矢量 u_alpha/u_beta
变成 A/B/C 三相电压指令
```

## 第二步：找最大值和最小值

```c
u_max = max(ua, ub, uc);
u_min = min(ua, ub, uc);
```

## 第三步：算零序注入量

```c
u_zero = -0.5 * (u_max + u_min);
```

这一步是 SVPWM 的关键。

## 第四步：三相同时加上 u_zero

```c
ua = ua + u_zero;
ub = ub + u_zero;
uc = uc + u_zero;
```

这不会改变电机真正看到的线电压，因为三相都加了同一个量。

比如：

```text
Uab = Ua - Ub
```

三相都加 `u_zero` 后：

```text
Uab_new = (Ua + u_zero) - (Ub + u_zero)
        = Ua - Ub
```

所以线电压不变。

但它可以让 PWM 占空比更合理，母线电压利用率更高。

## 第五步：转成 duty

```c
duty_a = 0.5 + ua / vbus;
duty_b = 0.5 + ub / vbus;
duty_c = 0.5 + uc / vbus;
```

---

# 5. 完整 SVPWM min-max 伪代码

```c
#define SQRT3_BY_2 0.86602540378f

static float limit_float(float x, float min, float max)
{
    if (x > max) return max;
    if (x < min) return min;
    return x;
}

static float max3(float a, float b, float c)
{
    float m = a;

    if (b > m) m = b;
    if (c > m) m = c;

    return m;
}

static float min3(float a, float b, float c)
{
    float m = a;

    if (b < m) m = b;
    if (c < m) m = c;

    return m;
}

/*
 * 行业常用简化 SVPWM：min-max 零序注入法
 *
 * 输入：
 *   u_alpha：alpha轴电压
 *   u_beta ：beta轴电压
 *   vbus   ：母线电压
 *
 * 输出：
 *   duty_a / duty_b / duty_c：三相 PWM 占空比，范围 0~1
 */
void svpwm_minmax(float u_alpha,
                  float u_beta,
                  float vbus,
                  float *duty_a,
                  float *duty_b,
                  float *duty_c)
{
    float ua;
    float ub;
    float uc;

    float u_max;
    float u_min;
    float u_zero;

    /*
     * 1. alpha/beta → 三相电压
     */
    ua = u_alpha;
    ub = -0.5f * u_alpha + SQRT3_BY_2 * u_beta;
    uc = -0.5f * u_alpha - SQRT3_BY_2 * u_beta;

    /*
     * 2. 找三相中的最大值和最小值
     */
    u_max = max3(ua, ub, uc);
    u_min = min3(ua, ub, uc);

    /*
     * 3. 计算零序注入量
     *
     * 这是 SVPWM 和普通 SPWM 最核心的区别。
     */
    u_zero = -0.5f * (u_max + u_min);

    /*
     * 4. 三相同时加上零序量
     */
    ua += u_zero;
    ub += u_zero;
    uc += u_zero;

    /*
     * 5. 三相电压转 PWM 占空比
     *
     * 0.5 表示中心点。
     * ua / vbus 表示在中心点基础上的偏移。
     */
    *duty_a = 0.5f + ua / vbus;
    *duty_b = 0.5f + ub / vbus;
    *duty_c = 0.5f + uc / vbus;

    /*
     * 6. 限幅，避免超过 0%~100%
     */
    *duty_a = limit_float(*duty_a, 0.0f, 1.0f);
    *duty_b = limit_float(*duty_b, 0.0f, 1.0f);
    *duty_c = limit_float(*duty_c, 0.0f, 1.0f);
}
```

这就是你可以直接照着理解的工程版本。

---

# 6. STM32 PWM 更新伪代码

STM32 做三相 FOC，一般用：

```text
TIM1 或 TIM8
```

因为它们是高级定时器，支持：

```text
三相 PWM
互补输出
死区
刹车保护
中心对齐
ADC 触发
```

PWM 更新函数：

```c
void pwm_update_3phase(float duty_a,
                       float duty_b,
                       float duty_c)
{
    uint32_t arr;

    arr = __HAL_TIM_GET_AUTORELOAD(&htim1);

    duty_a = limit_float(duty_a, 0.0f, 1.0f);
    duty_b = limit_float(duty_b, 0.0f, 1.0f);
    duty_c = limit_float(duty_c, 0.0f, 1.0f);

    /*
     * 中心对齐 PWM 下：
     * CCR = duty * ARR
     */
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, (uint32_t)(duty_a * arr));
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, (uint32_t)(duty_b * arr));
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_3, (uint32_t)(duty_c * arr));
}
```

启动 PWM：

```c
void motor_pwm_start(void)
{
    /*
     * 启动三相上桥 PWM
     */
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_3);

    /*
     * 启动三相下桥互补 PWM
     */
    HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_1);
    HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_2);
    HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_3);
}
```

---

# 7. 推荐的定时器中断结构

行业里一般不是在 `while(1)` 里随便 delay 控制，而是用 PWM 定时器中断或者 ADC 中断。

例如：

```text
PWM 频率：20kHz
FOC 控制频率：20kHz 或 10kHz
```

常见结构：

```c
void TIM1_UP_TIM10_IRQHandler(void)
{
    /*
     * 1. 清除 TIM1 更新中断标志
     */
    if (__HAL_TIM_GET_FLAG(&htim1, TIM_FLAG_UPDATE) != RESET)
    {
        if (__HAL_TIM_GET_IT_SOURCE(&htim1, TIM_IT_UPDATE) != RESET)
        {
            __HAL_TIM_CLEAR_IT(&htim1, TIM_IT_UPDATE);

            /*
             * 2. 执行一次 FOC 开环速度控制
             *
             * 如果 PWM 是 20kHz：
             * dt = 1 / 20000 = 0.00005s
             */
            foc_openloop_speed_loop(0.00005f);
        }
    }
}
```

更工程化一点，会用 ADC 注入转换完成中断：

```text
TIM1 产生 PWM
TIM1 在 PWM 中间点触发 ADC
ADC 采完电流后进中断
在 ADC 中断里执行 FOC
最后更新下一周期 PWM
```

对于开环测试，可以先不管 ADC，先在 TIM1 更新中断里跑。

---

# 8. 加上速度斜坡，避免电机突然抖动

开环速度控制不要一上来就给很大速度。

不要这样：

```c
motor.speed_e_ref = 500.0f;
```

建议做一个速度斜坡：

```c
float speed_cmd = 0.0f;       // 当前实际用于控制的速度
float speed_target = 100.0f;  // 最终目标速度
float speed_ramp = 500.0f;    // 每秒增加多少 rad/s

void speed_ramp_update(float dt)
{
    if (speed_cmd < speed_target)
    {
        speed_cmd += speed_ramp * dt;

        if (speed_cmd > speed_target)
        {
            speed_cmd = speed_target;
        }
    }
    else if (speed_cmd > speed_target)
    {
        speed_cmd -= speed_ramp * dt;

        if (speed_cmd < speed_target)
        {
            speed_cmd = speed_target;
        }
    }

    motor.speed_e_ref = speed_cmd;
}
```

然后在 FOC 周期里：

```c
void foc_openloop_speed_loop(float dt)
{
    float u_alpha;
    float u_beta;

    speed_ramp_update(dt);

    motor.theta_e += motor.speed_e_ref * dt;

    if (motor.theta_e >= TWO_PI)
    {
        motor.theta_e -= TWO_PI;
    }

    inverse_park(
        motor.ud,
        motor.uq,
        motor.theta_e,
        &u_alpha,
        &u_beta
    );

    svpwm_minmax(
        u_alpha,
        u_beta,
        motor.vbus,
        &motor.duty_a,
        &motor.duty_b,
        &motor.duty_c
    );

    pwm_update_3phase(
        motor.duty_a,
        motor.duty_b,
        motor.duty_c
    );
}
```

这样电机启动会平滑很多。

---

# 9. 加上电压斜坡，避免启动电流过大

开环启动时，`uq` 也不要突变。

可以这样：

```c
float uq_cmd = 0.0f;
float uq_target = 2.0f;
float uq_ramp = 10.0f;    // 每秒增加 10V

void voltage_ramp_update(float dt)
{
    if (uq_cmd < uq_target)
    {
        uq_cmd += uq_ramp * dt;

        if (uq_cmd > uq_target)
        {
            uq_cmd = uq_target;
        }
    }

    motor.uq = uq_cmd;
}
```

然后：

```c
void foc_openloop_speed_loop(float dt)
{
    float u_alpha;
    float u_beta;

    speed_ramp_update(dt);
    voltage_ramp_update(dt);

    motor.theta_e += motor.speed_e_ref * dt;

    if (motor.theta_e >= TWO_PI)
    {
        motor.theta_e -= TWO_PI;
    }

    inverse_park(
        motor.ud,
        motor.uq,
        motor.theta_e,
        &u_alpha,
        &u_beta
    );

    svpwm_minmax(
        u_alpha,
        u_beta,
        motor.vbus,
        &motor.duty_a,
        &motor.duty_b,
        &motor.duty_c
    );

    pwm_update_3phase(
        motor.duty_a,
        motor.duty_b,
        motor.duty_c
    );
}
```

这个更像一个能实际跑起来的开环启动流程。

---

# 10. 加上电压限幅，避免过调制

SVPWM 虽然母线利用率高，但也不能无限大。

在进入 SVPWM 前，最好限制 `u_alpha/u_beta` 的幅值。

```c
void voltage_vector_limit(float *u_alpha,
                          float *u_beta,
                          float vbus)
{
    float mag;
    float max_mag;
    float scale;

    mag = sqrtf((*u_alpha) * (*u_alpha) + (*u_beta) * (*u_beta));

    /*
     * 线性 SVPWM 区域大致可以用 vbus / sqrt(3)
     * 这里保守一点，乘 0.95
     */
    max_mag = 0.95f * vbus * 0.577350269f;

    if (mag > max_mag)
    {
        scale = max_mag / mag;

        *u_alpha *= scale;
        *u_beta  *= scale;
    }
}
```

然后在 SVPWM 前调用：

```c
inverse_park(
    motor.ud,
    motor.uq,
    motor.theta_e,
    &u_alpha,
    &u_beta
);

voltage_vector_limit(&u_alpha, &u_beta, motor.vbus);

svpwm_minmax(
    u_alpha,
    u_beta,
    motor.vbus,
    &motor.duty_a,
    &motor.duty_b,
    &motor.duty_c
);
```

---

# 11. 最完整的行业风格开环 SVPWM 伪代码

把上面的组合起来：

```c
void foc_openloop_svpwm_control_isr(float dt)
{
    float u_alpha;
    float u_beta;

    /*
     * 1. 速度斜坡
     * 防止速度目标突变
     */
    speed_ramp_update(dt);

    /*
     * 2. 电压斜坡
     * 防止 uq 突然变大导致过流
     */
    voltage_ramp_update(dt);

    /*
     * 3. 开环累加电角度
     */
    motor.theta_e += motor.speed_e_ref * dt;

    while (motor.theta_e >= TWO_PI)
    {
        motor.theta_e -= TWO_PI;
    }

    while (motor.theta_e < 0.0f)
    {
        motor.theta_e += TWO_PI;
    }

    /*
     * 4. 开环 FOC：
     * ud = 0
     * uq = 给定电压
     */
    motor.ud = 0.0f;

    /*
     * 5. 反 Park：
     * dq 电压 → alpha/beta 电压
     */
    inverse_park(
        motor.ud,
        motor.uq,
        motor.theta_e,
        &u_alpha,
        &u_beta
    );

    /*
     * 6. 电压矢量限幅
     * 防止过调制
     */
    voltage_vector_limit(
        &u_alpha,
        &u_beta,
        motor.vbus
    );

    /*
     * 7. SVPWM：
     * alpha/beta 电压 → 三相 duty
     */
    svpwm_minmax(
        u_alpha,
        u_beta,
        motor.vbus,
        &motor.duty_a,
        &motor.duty_b,
        &motor.duty_c
    );

    /*
     * 8. 更新 TIM1/TIM8 CCR
     */
    pwm_update_3phase(
        motor.duty_a,
        motor.duty_b,
        motor.duty_c
    );
}
```

这个就已经是比较标准的开环 FOC + SVPWM 软件结构了。

---

# 12. 传统扇区法 SVPWM 思路

你要是想理解“正统 SVPWM”，可以看这个思路。

三相桥一共有 8 种开关状态：

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

这 6 个有效矢量把平面分成 6 个扇区：

```text
Sector 1：0°   ~ 60°
Sector 2：60°  ~ 120°
Sector 3：120° ~ 180°
Sector 4：180° ~ 240°
Sector 5：240° ~ 300°
Sector 6：300° ~ 360°
```

传统 SVPWM 的流程：

```c
void svpwm_sector_method(float u_alpha,
                         float u_beta,
                         float vbus,
                         float *duty_a,
                         float *duty_b,
                         float *duty_c)
{
    /*
     * 1. 根据 u_alpha/u_beta 算角度 angle
     */
    angle = atan2f(u_beta, u_alpha);

    if (angle < 0)
    {
        angle += TWO_PI;
    }

    /*
     * 2. 判断扇区
     */
    sector = (int)(angle / (PI / 3.0f)) + 1;

    if (sector > 6)
    {
        sector = 6;
    }

    /*
     * 3. 算当前扇区内的小角度
     */
    angle_in_sector = angle - (sector - 1) * (PI / 3.0f);

    /*
     * 4. 根据电压幅值计算 T1/T2/T0
     *
     * 这里是概念伪代码，不纠结具体系数。
     */
    vref = sqrtf(u_alpha * u_alpha + u_beta * u_beta);

    T1 = PWM_PERIOD * K * vref / vbus * sinf(PI / 3.0f - angle_in_sector);
    T2 = PWM_PERIOD * K * vref / vbus * sinf(angle_in_sector);
    T0 = PWM_PERIOD - T1 - T2;

    if (T0 < 0)
    {
        T0 = 0;
    }

    /*
     * 5. 七段式对称分配
     * T0/2 放两边，中间放 T1/T2
     */
    switch (sector)
    {
        case 1:
            Ta = T0 / 2 + T1 + T2;
            Tb = T0 / 2 + T2;
            Tc = T0 / 2;
            break;

        case 2:
            Ta = T0 / 2 + T1;
            Tb = T0 / 2 + T1 + T2;
            Tc = T0 / 2;
            break;

        case 3:
            Ta = T0 / 2;
            Tb = T0 / 2 + T1 + T2;
            Tc = T0 / 2 + T2;
            break;

        case 4:
            Ta = T0 / 2;
            Tb = T0 / 2 + T1;
            Tc = T0 / 2 + T1 + T2;
            break;

        case 5:
            Ta = T0 / 2 + T2;
            Tb = T0 / 2;
            Tc = T0 / 2 + T1 + T2;
            break;

        case 6:
            Ta = T0 / 2 + T1 + T2;
            Tb = T0 / 2;
            Tc = T0 / 2 + T1;
            break;
    }

    /*
     * 6. 转换成 duty
     */
    *duty_a = Ta / PWM_PERIOD;
    *duty_b = Tb / PWM_PERIOD;
    *duty_c = Tc / PWM_PERIOD;
}
```

这个是传统扇区法的思路。

但是你实际写 STM32 项目时，我建议先用：

```text
min-max 零序注入法
```

因为它更简单、稳定、容易调通。

---

# 13. min-max 法和扇区法是什么关系？

你可以这样理解：

```text
扇区法：
从三相桥的 6 个空间矢量出发，算 T1/T2/T0。

min-max 法：
从三相电压出发，注入零序分量，让占空比等效变成 SVPWM。
```

它们最后的效果非常接近。

工程代码里很多人不会真的显式写：

```text
sector
T1
T2
T0
```

而是用 min-max 算法快速得到 duty。

---

# 14. 对你来说应该怎么学？

建议顺序：

```text
第一步：先理解开环角度 theta_e 怎么转
第二步：理解 inverse_park 得到 u_alpha/u_beta
第三步：用 SPWM 跑起来
第四步：把 SPWM 换成 min-max SVPWM
第五步：用示波器看 duty_a/b/c
第六步：再加电流采样、电流闭环
```

你现在重点记住这个核心：

```c
// alpha/beta 转三相
ua = u_alpha;
ub = -0.5f * u_alpha + 0.866f * u_beta;
uc = -0.5f * u_alpha - 0.866f * u_beta;

// SVPWM 零序注入
u_zero = -0.5f * (max(ua, ub, uc) + min(ua, ub, uc));

ua += u_zero;
ub += u_zero;
uc += u_zero;

// 转 duty
duty_a = 0.5f + ua / vbus;
duty_b = 0.5f + ub / vbus;
duty_c = 0.5f + uc / vbus;
```

这段就是你最该背下来的 SVPWM 工程实现核心。

---

# 15. 平衡车最终不能只用开环速度控制

这个很重要。

开环速度控制只能用于：

```text
学习
空载测试
验证 PWM
验证 SVPWM
无感闭环启动前的预定位/拖动
```

不能直接作为平衡车最终方案。

平衡车最终需要：

```text
IMU 姿态环
速度环
电流/力矩环
左右轮差速
保护逻辑
```

如果你用 FOC，最终结构应该是：

```text
IMU 算 pitch/pitch_rate
        ↓
平衡环输出 torque_ref
        ↓
速度环修正 torque_ref
        ↓
转换成 iq_ref
        ↓
FOC 电流环
        ↓
SVPWM
        ↓
电机
```

而不是：

```text
给一个开环 speed_e_ref 让电机自己转
```

平衡车需要的是**力矩控制**，不是简单开环速度。

---

# 最后总结

行业标准 FOC 里面，SVPWM 通常这样实现：

```text
1. FOC 算出 u_alpha/u_beta
2. alpha/beta 转三相 ua/ub/uc
3. 找 u_max/u_min
4. 注入 u_zero = -0.5 * (u_max + u_min)
5. 三相加 u_zero
6. 转 duty_a/duty_b/duty_c
7. 更新 TIM1/TIM8 CCR
```

对 STM32 工程来说，最推荐你先实现这个：

```text
min-max 零序注入 SVPWM
```

因为它：

```text
代码简单
效果接近标准 SVPWM
不用判断扇区
适合 FOC 入门和工程项目
```

你现在可以先做：

```text
开环 theta_e 累加
ud = 0
uq = 小电压
inverse_park
svpwm_minmax
pwm_update
```

等这个跑通后，再去加：

```text
电流采样
Park/Clarke
id/iq 电流环
速度环
平衡车姿态环
```


----------
-----
-----
传统扇区法里的**“扇区”**，你可以先别想复杂，就把它理解成：

> **SVPWM 把一圈 360° 分成了 6 块区域，每一块就是一个扇区。**

就像切披萨一样：

```text
一整圈 = 360°
切成 6 份
每份 = 60°
每一份就叫一个扇区
```

---

# 1. 为什么 SVPWM 要分 6 个扇区？

因为三相逆变桥有 6 个有效电压矢量。

三相桥有 A/B/C 三相，每相上桥/下桥可以形成一个开关状态：

```text
A相 B相 C相
 0   0   1
 0   1   0
 0   1   1
 1   0   0
 1   0   1
 1   1   0
```

这 6 个状态会在空间里形成 6 个固定方向的电压矢量。

你可以把它想象成一个六边形：

```text
              V2
             /  \
            /    \
        V3 /      \ V1
          |        |
          |        |
        V4 \      / V6
            \    /
             \  /
              V5
```

每两个相邻矢量之间夹角是：

```text
60°
```

所以一圈刚好分成：

```text
6 个扇区
```

---

# 2. 扇区到底是什么意思？

假设你现在 FOC 算出来一个目标电压矢量：

```text
Uref
```

这个矢量会指向某个方向。

SVPWM 要做的事情是：

> 看这个 Uref 落在哪两个相邻的基本矢量中间，然后用这两个基本矢量合成它。

比如：

```text
Uref 落在 V1 和 V2 之间
```

那就说：

```text
Uref 在第 1 扇区
```

然后 SVPWM 就用：

```text
V1 + V2 + 零矢量
```

来合成这个目标电压。

---

# 3. 用最通俗的比喻讲

你可以把 6 个有效矢量理解成 6 个固定方向的“力”：

```text
V1：往右推
V2：右上推
V3：左上推
V4：往左推
V5：左下推
V6：右下推
```

但是你想要的方向可能不是刚好这 6 个方向之一，比如你想往“右偏上一点”推。

那怎么办？

就用两个相邻方向一起合成：

```text
想往右偏上一点推
= V1 推一会儿 + V2 推一会儿
```

这就是 SVPWM 的核心。

---

# 4. 第 1 扇区举例

假设目标电压矢量在 0°~60° 之间：

```text
             V2
            /
           /
          /   Uref
         /   /
        /   /
       /   /
      O---------- V1
```

这个区域就是：

```text
第 1 扇区
```

这时候 SVPWM 会用：

```text
V1 作用一段时间 T1
V2 作用一段时间 T2
零矢量作用一段时间 T0
```

合成目标矢量。

也就是：

```text
一个 PWM 周期 Ts = T1 + T2 + T0
```

---

# 5. 为什么不是只用 V1 或 V2？

因为目标方向可能在 V1 和 V2 中间。

比如：

```text
V1 方向是 0°
V2 方向是 60°
Uref 方向是 30°
```

如果只用 V1，方向太低。

如果只用 V2，方向太高。

所以用：

```text
V1 用一半时间
V2 用一半时间
```

平均效果就是 30° 左右。

如果目标更靠近 V1，比如 10°：

```text
V1 用久一点
V2 用短一点
```

如果目标更靠近 V2，比如 50°：

```text
V1 用短一点
V2 用久一点
```

这就是 `T1` 和 `T2` 的意义。

---

# 6. T1、T2、T0 是啥？

在一个 PWM 周期里面，比如：

```text
PWM周期 Ts = 50us
```

SVPWM 会分配：

```text
T1 = 20us
T2 = 10us
T0 = 20us
```

意思是：

```text
20us 使用第一个有效矢量
10us 使用第二个有效矢量
20us 使用零矢量
```

平均下来，就合成了你要的目标电压矢量。

---

# 7. 零矢量是啥？

三相桥有两个特殊状态：

```text
000
111
```

这两个叫零矢量。

简单理解：

```text
000：三相都接到下桥
111：三相都接到上桥
```

这时候三相之间没有有效线电压，所以叫：

```text
零矢量
```

它的作用是：

```text
补足 PWM 周期剩余时间
让波形对称
减少谐波
方便产生中心对齐 PWM
```

---

# 8. 6 个扇区怎么分？

可以简单这样分：

```text
第1扇区：0°   ~ 60°
第2扇区：60°  ~ 120°
第3扇区：120° ~ 180°
第4扇区：180° ~ 240°
第5扇区：240° ~ 300°
第6扇区：300° ~ 360°
```

如果目标矢量角度是：

```text
30°
```

那就在：

```text
第1扇区
```

如果目标矢量角度是：

```text
100°
```

那就在：

```text
第2扇区
```

如果目标矢量角度是：

```text
250°
```

那就在：

```text
第5扇区
```

伪代码就是：

```c
float angle;   // 0 ~ 2π
int sector;

sector = (int)(angle / (PI / 3.0f)) + 1;

if (sector > 6)
{
    sector = 6;
}
```

因为：

```text
PI / 3 = 60°
```

---

# 9. 扇区判断以后要干嘛？

判断扇区以后，SVPWM 就知道应该用哪两个相邻矢量。

比如：

```text
第1扇区：用 V1 和 V2
第2扇区：用 V2 和 V3
第3扇区：用 V3 和 V4
第4扇区：用 V4 和 V5
第5扇区：用 V5 和 V6
第6扇区：用 V6 和 V1
```

也就是：

```c
switch (sector)
{
    case 1:
        // 用 V1 和 V2 合成
        break;

    case 2:
        // 用 V2 和 V3 合成
        break;

    case 3:
        // 用 V3 和 V4 合成
        break;

    case 4:
        // 用 V4 和 V5 合成
        break;

    case 5:
        // 用 V5 和 V6 合成
        break;

    case 6:
        // 用 V6 和 V1 合成
        break;
}
```

---

# 10. 结合开环 FOC 看扇区怎么变化

开环 FOC 里，你会这样累加角度：

```c
theta_e += speed_e * dt;
```

这个 `theta_e` 会一直从：

```text
0° → 60° → 120° → 180° → 240° → 300° → 360°
```

所以目标电压矢量也会跟着转。

对应扇区会这样变化：

```text
第1扇区 → 第2扇区 → 第3扇区 → 第4扇区 → 第5扇区 → 第6扇区 → 回到第1扇区
```

这个循环不断重复，电机磁场就旋转起来了。

电机转子就会跟着这个旋转磁场跑。

---

# 11. 伪代码演示：只判断扇区

```c
#define PI      3.1415926f
#define TWO_PI  6.2831853f

float normalize_angle(float angle)
{
    while (angle >= TWO_PI)
    {
        angle -= TWO_PI;
    }

    while (angle < 0.0f)
    {
        angle += TWO_PI;
    }

    return angle;
}

int svpwm_get_sector(float angle)
{
    int sector;

    angle = normalize_angle(angle);

    /*
     * 每个扇区 60°
     * 60° = PI / 3
     */
    sector = (int)(angle / (PI / 3.0f)) + 1;

    if (sector < 1)
    {
        sector = 1;
    }

    if (sector > 6)
    {
        sector = 6;
    }

    return sector;
}
```

测试：

```c
printf("%d\r\n", svpwm_get_sector(0.1f));   // 第1扇区
printf("%d\r\n", svpwm_get_sector(1.5f));   // 大约第2扇区
printf("%d\r\n", svpwm_get_sector(3.5f));   // 大约第4扇区
```

---

# 12. 更完整一点：FOC 开环里看扇区变化

```c
void foc_openloop_debug(void)
{
    float theta_e = 0.0f;
    float speed_e = 100.0f;
    float dt = 0.0001f;

    int sector;

    while (1)
    {
        /*
         * 1. 开环角度累加
         */
        theta_e += speed_e * dt;
        theta_e = normalize_angle(theta_e);

        /*
         * 2. 判断当前电压矢量在哪个扇区
         */
        sector = svpwm_get_sector(theta_e);

        /*
         * 3. 打印观察
         */
        printf("theta = %.3f rad, sector = %d\r\n", theta_e, sector);

        delay_us(100);
    }
}
```

你会看到 sector 会这样变化：

```text
sector = 1
sector = 1
sector = 1
...
sector = 2
sector = 2
...
sector = 3
...
sector = 4
...
sector = 5
...
sector = 6
...
sector = 1
```

这说明电压矢量正在一圈一圈旋转。

---

# 13. 初学者最容易误解的地方

## 误解 1：扇区不是电机物理上的区域

扇区不是电机里面真实切了 6 块。

它是：

```text
电压矢量平面上的 6 个数学区域
```

不是：

```text
电机定子真的分成 6 个扇区
```

---

## 误解 2：扇区不是霍尔传感器的 6 个状态

BLDC 霍尔也有 6 个状态，六步换向也有 6 步。

SVPWM 也有 6 个扇区。

它们看起来很像，但不是同一个概念。

可以这样理解：

```text
霍尔六步：
根据转子位置切换六个通电状态

SVPWM 六扇区：
根据目标电压矢量方向选择相邻两个空间矢量
```

有关系，但不能完全混为一谈。

---

## 误解 3：SVPWM 不一定必须显式判断扇区

传统扇区法要判断：

```text
第几扇区
T1
T2
T0
```

但工程里也可以用：

```text
min-max 零序注入法
```

它不用显式判断扇区，但效果还是 SVPWM。

所以你现在学原理，理解扇区很重要。

但真正写代码，先用 min-max 法更容易跑通。

---

# 14. 用一句话总结“扇区”

**扇区就是 SVPWM 把 360° 电压矢量圆分成 6 个 60° 区域。**

目标电压矢量落在哪个区域，就用这个区域两边的两个基本矢量去合成它。

例如：

```text
目标电压在 0°~60°
就是第1扇区
用 V1 和 V2 合成

目标电压在 60°~120°
就是第2扇区
用 V2 和 V3 合成
```

你先把它理解成：

```text
“看目标电压箭头指在哪一块披萨里”
```

这一块披萨，就是扇区。

--------
-------
------

没事，咱们换个**更土、更直观**的讲法。

你先别管公式，先记住：

> **SVPWM 的“扇区”，就是判断：你现在想让电机磁场往哪个方向推。**

---

# 1. 先想象一个圆盘

假设你站在圆心，周围有 6 个方向：

```text
           2号方向
              ↗
             /
3号方向 ←  O  → 1号方向
             \
              ↘
           6号方向
```

再完整一点：

```text
              V2
              ↑↗
          V3  |  V1
              |
       V4 ----O---- V1
              |
          V5  |  V6
              ↓↘
```

不太好画，我们用角度表示更清楚：

```text
V1：0°
V2：60°
V3：120°
V4：180°
V5：240°
V6：300°
```

也就是一圈有 6 个固定方向：

```text
0°、60°、120°、180°、240°、300°
```

这 6 个方向就是三相桥能直接产生的 6 个主要电压方向。

---

# 2. “扇区”就是两个方向之间的区域

比如：

```text
V1 = 0°
V2 = 60°
```

那么 `0° ~ 60°` 中间这块区域，就叫：

```text
第 1 扇区
```

像这样：

```text
        V2 60°
        /
       /
      /   ← 这一块就是第1扇区
     /
O---------------- V1 0°
```

如果你的目标电压方向是：

```text
30°
```

它就在 `0° ~ 60°` 中间。

所以它在：

```text
第 1 扇区
```

---

# 3. 再举几个例子

一圈 360°，切成 6 份：

```text
第1扇区：0°   ~ 60°
第2扇区：60°  ~ 120°
第3扇区：120° ~ 180°
第4扇区：180° ~ 240°
第5扇区：240° ~ 300°
第6扇区：300° ~ 360°
```

所以：

```text
目标角度 = 20°   → 第1扇区
目标角度 = 45°   → 第1扇区

目标角度 = 80°   → 第2扇区
目标角度 = 110°  → 第2扇区

目标角度 = 150°  → 第3扇区

目标角度 = 210°  → 第4扇区

目标角度 = 260°  → 第5扇区

目标角度 = 330°  → 第6扇区
```

这就是“扇区”。

---

# 4. 用披萨理解

你把一个圆形披萨切成 6 块：

```text
一整张披萨：360°
切成6块
每块：60°
每一块就是一个扇区
```

SVPWM 的问题就是：

```text
我现在这个目标电压箭头，落在哪一块披萨里？
```

落在第一块，就是第 1 扇区。

落在第二块，就是第 2 扇区。

---

# 5. 为什么要知道它在哪个扇区？

因为 SVPWM 不是随便输出 PWM。

它要先判断：

```text
目标电压方向在第几扇区？
```

然后用这个扇区两边的两个方向来合成它。

比如目标方向是 `30°`：

```text
30° 在第1扇区
第1扇区两边是：
V1 = 0°
V2 = 60°
```

所以 SVPWM 就用：

```text
V1 推一会儿
V2 推一会儿
```

最后平均出来的效果就是：

```text
30°方向
```

---

# 6. 这个“推一会儿”是什么意思？

三相桥在一个 PWM 周期里面可以快速切换状态。

比如一个 PWM 周期是：

```text
50us
```

目标方向是 `30°`，在 V1 和 V2 中间。

那可以这样：

```text
V1 状态保持 20us
V2 状态保持 20us
零矢量保持 10us
```

平均效果就像输出了一个 30° 方向的电压。

你可以把它想成：

```text
V1 是往右推
V2 是往右上推
两个轮流推
最后平均效果就是往右上偏一点推
```

---

# 7. 用人推车的例子

假设有两个人推车：

```text
A人：只能往右推，方向 0°
B人：只能往右上推，方向 60°
```

现在你想让车往 `30°` 方向走。

怎么办？

让 A 推一会儿，让 B 推一会儿。

如果：

```text
A 推 50% 时间
B 推 50% 时间
```

平均方向差不多就是 30°。

如果你想要 `10°` 方向：

```text
A 多推一点
B 少推一点
```

如果你想要 `50°` 方向：

```text
A 少推一点
B 多推一点
```

这就是 SVPWM 的核心。

---

# 8. 扇区就是告诉你“该用哪两个人推”

比如：

## 目标方向 30°

```text
30° 在第1扇区
第1扇区两边是 V1 和 V2
所以用 V1、V2 合成
```

## 目标方向 90°

```text
90° 在第2扇区
第2扇区两边是 V2 和 V3
所以用 V2、V3 合成
```

## 目标方向 150°

```text
150° 在第3扇区
第3扇区两边是 V3 和 V4
所以用 V3、V4 合成
```

所以扇区的作用就是：

```text
判断当前目标电压应该用哪两个相邻基本矢量来合成
```

---

# 9. 直接看伪代码

最简单的扇区判断就是这样：

```c
// angle 是目标电压矢量角度，单位：度，范围 0~360

if (angle >= 0 && angle < 60)
{
    sector = 1;
}
else if (angle >= 60 && angle < 120)
{
    sector = 2;
}
else if (angle >= 120 && angle < 180)
{
    sector = 3;
}
else if (angle >= 180 && angle < 240)
{
    sector = 4;
}
else if (angle >= 240 && angle < 300)
{
    sector = 5;
}
else if (angle >= 300 && angle < 360)
{
    sector = 6;
}
```

这就是扇区判断。

没有别的神秘东西。

---

# 10. 换成弧度就是这样

在 C 代码里一般不用角度，用弧度：

```text
360° = 2π
60°  = π / 3
```

所以：

```c
sector = (int)(angle / (PI / 3.0f)) + 1;
```

如果你不理解这个公式，就先别用它，直接用角度版 `if else` 理解。

---

# 11. 结合开环 FOC

开环 FOC 里，你会有一个电角度：

```c
theta_e += speed_e * dt;
```

这个 `theta_e` 会一直增加。

比如：

```text
0°
10°
20°
30°
40°
50°
60°
70°
80°
...
```

那么扇区就会这样变化：

```text
0°  ~ 59°   → 第1扇区
60° ~ 119°  → 第2扇区
120°~179°   → 第3扇区
180°~239°   → 第4扇区
240°~299°   → 第5扇区
300°~359°   → 第6扇区
```

然后再回到第 1 扇区。

也就是：

```text
第1扇区 → 第2扇区 → 第3扇区 → 第4扇区 → 第5扇区 → 第6扇区 → 第1扇区
```

这说明电压矢量在旋转。

电压矢量旋转，电机磁场就旋转。

磁场旋转，电机转子就跟着转。

---

# 12. 一句话版本

**扇区 = 目标电压箭头现在落在哪个 60° 区域。**

比如：

```text
目标电压箭头指向 30°
它在 0°~60°
所以是第1扇区
用 V1 和 V2 合成

目标电压箭头指向 100°
它在 60°~120°
所以是第2扇区
用 V2 和 V3 合成
```

---

# 13. 你现在只需要记住这个图

```text
第1扇区：0°   ~ 60°    用 V1、V2
第2扇区：60°  ~ 120°   用 V2、V3
第3扇区：120° ~ 180°   用 V3、V4
第4扇区：180° ~ 240°   用 V4、V5
第5扇区：240° ~ 300°   用 V5、V6
第6扇区：300° ~ 360°   用 V6、V1
```

这就是传统扇区法里的“扇区”。

再通俗一点：

```text
SVPWM 有 6 个固定方向。
目标方向夹在哪两个固定方向中间，
那一块区域就叫扇区。
```

你先把“扇区”理解到这个程度就够了。


-----
-----
-----
下面直接写**传统扇区法 SVPWM 的伪代码**。你先不用纠结公式来源，先看代码流程。

核心目的：

```text
输入：u_alpha, u_beta
输出：duty_a, duty_b, duty_c
```

也就是：

```text
FOC 算出来的电压矢量
        ↓
判断在哪个扇区
        ↓
计算 T1/T2/T0
        ↓
换算成 A/B/C 三相占空比
        ↓
更新 STM32 PWM
```

---

# 1. 最简单的扇区判断伪代码

假设我们已经有目标电压矢量角度 `angle_deg`，范围是 `0° ~ 360°`。

```c
int get_sector_by_angle(float angle_deg)
{
    int sector;

    if (angle_deg >= 0.0f && angle_deg < 60.0f)
    {
        sector = 1;
    }
    else if (angle_deg >= 60.0f && angle_deg < 120.0f)
    {
        sector = 2;
    }
    else if (angle_deg >= 120.0f && angle_deg < 180.0f)
    {
        sector = 3;
    }
    else if (angle_deg >= 180.0f && angle_deg < 240.0f)
    {
        sector = 4;
    }
    else if (angle_deg >= 240.0f && angle_deg < 300.0f)
    {
        sector = 5;
    }
    else
    {
        sector = 6;
    }

    return sector;
}
```

这段代码就是：

```text
0°~60°    → 第1扇区
60°~120°  → 第2扇区
120°~180° → 第3扇区
180°~240° → 第4扇区
240°~300° → 第5扇区
300°~360° → 第6扇区
```

---

# 2. 真实代码里一般用弧度

C 语言里三角函数一般用弧度，不用角度。

```c
#define PI      3.1415926535f
#define TWO_PI  6.283185307f

float normalize_angle(float angle)
{
    while (angle >= TWO_PI)
    {
        angle -= TWO_PI;
    }

    while (angle < 0.0f)
    {
        angle += TWO_PI;
    }

    return angle;
}

int get_sector_by_rad(float angle_rad)
{
    int sector;

    angle_rad = normalize_angle(angle_rad);

    /*
     * 一个扇区 = 60°
     * 60° = PI / 3
     */
    sector = (int)(angle_rad / (PI / 3.0f)) + 1;

    if (sector < 1)
    {
        sector = 1;
    }

    if (sector > 6)
    {
        sector = 6;
    }

    return sector;
}
```

比如：

```c
angle = 0.3 rad       → 第1扇区
angle = 1.5 rad       → 第2扇区
angle = 3.5 rad       → 第4扇区
angle = 5.8 rad       → 第6扇区
```

---

# 3. 开环 FOC 里角度怎么来？

开环速度控制里，没有真实位置反馈，我们自己造一个电角度：

```c
theta_e += speed_e * dt;
```

完整伪代码：

```c
float theta_e = 0.0f;       // 电角度，单位 rad
float speed_e = 100.0f;     // 电角速度，单位 rad/s
float dt = 0.0001f;         // 控制周期，100us = 10kHz

void open_loop_angle_update(void)
{
    theta_e += speed_e * dt;

    theta_e = normalize_angle(theta_e);
}
```

这个 `theta_e` 会不断转圈：

```text
0° → 30° → 60° → 90° → 120° → ... → 360° → 0°
```

扇区也会跟着变：

```text
第1扇区 → 第2扇区 → 第3扇区 → 第4扇区 → 第5扇区 → 第6扇区 → 第1扇区
```

---

# 4. 反 Park：生成 u_alpha / u_beta

开环 FOC 一般这样：

```c
ud = 0;
uq = 一个小电压;
```

比如：

```c
ud = 0.0f;
uq = 2.0f;
```

然后反 Park：

```c
void inverse_park(float ud,
                  float uq,
                  float theta,
                  float *u_alpha,
                  float *u_beta)
{
    float sin_t = sinf(theta);
    float cos_t = cosf(theta);

    *u_alpha = ud * cos_t - uq * sin_t;
    *u_beta  = ud * sin_t + uq * cos_t;
}
```

如果 `ud = 0`，那就是：

```c
u_alpha = -uq * sin(theta);
u_beta  =  uq * cos(theta);
```

这两个量组成一个旋转电压矢量。

---

# 5. 传统扇区法 SVPWM 伪代码

下面是重点。

输入：

```c
u_alpha
u_beta
vbus
```

输出：

```c
duty_a
duty_b
duty_c
```

---

## 5.1 先根据 u_alpha/u_beta 算角度

```c
float get_voltage_angle(float u_alpha, float u_beta)
{
    float angle;

    angle = atan2f(u_beta, u_alpha);

    if (angle < 0.0f)
    {
        angle += TWO_PI;
    }

    return angle;
}
```

---

## 5.2 传统扇区法 SVPWM 主函数

```c
void svpwm_sector_method(float u_alpha,
                         float u_beta,
                         float vbus,
                         float pwm_period,
                         float *duty_a,
                         float *duty_b,
                         float *duty_c)
{
    float angle;
    int sector;

    float angle_in_sector;
    float vref;

    float T1;
    float T2;
    float T0;

    float Ta;
    float Tb;
    float Tc;

    /*
     * 1. 计算目标电压矢量角度
     */
    angle = atan2f(u_beta, u_alpha);

    if (angle < 0.0f)
    {
        angle += TWO_PI;
    }

    /*
     * 2. 判断在哪个扇区
     */
    sector = get_sector_by_rad(angle);

    /*
     * 3. 计算当前扇区内部的小角度
     *
     * 比如：
     *   angle = 80°
     *   它在第2扇区
     *   第2扇区起点是 60°
     *   angle_in_sector = 80° - 60° = 20°
     */
    angle_in_sector = angle - (sector - 1) * (PI / 3.0f);

    /*
     * 4. 计算目标电压矢量幅值
     */
    vref = sqrtf(u_alpha * u_alpha + u_beta * u_beta);

    /*
     * 5. 计算 T1 / T2
     *
     * 这里的意思：
     *   T1：第一个相邻有效矢量作用时间
     *   T2：第二个相邻有效矢量作用时间
     *
     * 注意：
     *   这是概念伪代码，实际工程中还会根据归一化系数修正。
     */
    T1 = pwm_period * vref / vbus * sinf(PI / 3.0f - angle_in_sector);
    T2 = pwm_period * vref / vbus * sinf(angle_in_sector);

    /*
     * 6. 计算零矢量时间
     */
    T0 = pwm_period - T1 - T2;

    if (T0 < 0.0f)
    {
        T0 = 0.0f;
    }

    /*
     * 7. 根据扇区分配 Ta/Tb/Tc
     *
     * Ta/Tb/Tc 可以理解成：
     *   A相、B相、C相在一个PWM周期中的等效高电平时间
     */
    switch (sector)
    {
        case 1:
            /*
             * 第1扇区：V1 + V2 合成
             */
            Ta = T0 / 2.0f + T1 + T2;
            Tb = T0 / 2.0f + T2;
            Tc = T0 / 2.0f;
            break;

        case 2:
            /*
             * 第2扇区：V2 + V3 合成
             */
            Ta = T0 / 2.0f + T1;
            Tb = T0 / 2.0f + T1 + T2;
            Tc = T0 / 2.0f;
            break;

        case 3:
            /*
             * 第3扇区：V3 + V4 合成
             */
            Ta = T0 / 2.0f;
            Tb = T0 / 2.0f + T1 + T2;
            Tc = T0 / 2.0f + T2;
            break;

        case 4:
            /*
             * 第4扇区：V4 + V5 合成
             */
            Ta = T0 / 2.0f;
            Tb = T0 / 2.0f + T1;
            Tc = T0 / 2.0f + T1 + T2;
            break;

        case 5:
            /*
             * 第5扇区：V5 + V6 合成
             */
            Ta = T0 / 2.0f + T2;
            Tb = T0 / 2.0f;
            Tc = T0 / 2.0f + T1 + T2;
            break;

        case 6:
        default:
            /*
             * 第6扇区：V6 + V1 合成
             */
            Ta = T0 / 2.0f + T1 + T2;
            Tb = T0 / 2.0f;
            Tc = T0 / 2.0f + T1;
            break;
    }

    /*
     * 8. 转成 duty，占空比范围 0~1
     */
    *duty_a = Ta / pwm_period;
    *duty_b = Tb / pwm_period;
    *duty_c = Tc / pwm_period;

    /*
     * 9. 限幅
     */
    *duty_a = limit_float(*duty_a, 0.0f, 1.0f);
    *duty_b = limit_float(*duty_b, 0.0f, 1.0f);
    *duty_c = limit_float(*duty_c, 0.0f, 1.0f);
}
```

---

# 6. 完整开环 FOC + 传统扇区 SVPWM 伪代码

下面这个更完整，你可以从上往下看。

```c
#define PI      3.1415926535f
#define TWO_PI  6.283185307f

typedef struct
{
    float theta_e;       // 开环电角度
    float speed_e;       // 电角速度 rad/s

    float ud;            // d轴电压
    float uq;            // q轴电压

    float u_alpha;
    float u_beta;

    float vbus;          // 母线电压

    float duty_a;
    float duty_b;
    float duty_c;
} motor_t;

motor_t motor;
```

初始化：

```c
void motor_init(void)
{
    motor.theta_e = 0.0f;

    /*
     * 开环电角速度，先小一点
     */
    motor.speed_e = 50.0f;

    /*
     * 开环电压
     * ud = 0
     * uq 给一个小电压
     */
    motor.ud = 0.0f;
    motor.uq = 2.0f;

    motor.vbus = 24.0f;
}
```

控制周期函数：

```c
void foc_openloop_svpwm_update(float dt)
{
    /*
     * 1. 累加开环电角度
     */
    motor.theta_e += motor.speed_e * dt;

    motor.theta_e = normalize_angle(motor.theta_e);

    /*
     * 2. 反 Park 变换
     * ud/uq → u_alpha/u_beta
     */
    inverse_park(motor.ud,
                 motor.uq,
                 motor.theta_e,
                 &motor.u_alpha,
                 &motor.u_beta);

    /*
     * 3. 传统扇区法 SVPWM
     */
    svpwm_sector_method(motor.u_alpha,
                        motor.u_beta,
                        motor.vbus,
                        1.0f,                 // 这里用1表示一个归一化PWM周期
                        &motor.duty_a,
                        &motor.duty_b,
                        &motor.duty_c);

    /*
     * 4. 更新 STM32 PWM
     */
    pwm_update_3phase(motor.duty_a,
                      motor.duty_b,
                      motor.duty_c);
}
```

---

# 7. STM32 更新 PWM 伪代码

假设用 TIM1：

```c
void pwm_update_3phase(float duty_a,
                       float duty_b,
                       float duty_c)
{
    uint32_t arr;

    arr = __HAL_TIM_GET_AUTORELOAD(&htim1);

    duty_a = limit_float(duty_a, 0.0f, 1.0f);
    duty_b = limit_float(duty_b, 0.0f, 1.0f);
    duty_c = limit_float(duty_c, 0.0f, 1.0f);

    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, (uint32_t)(duty_a * arr));
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, (uint32_t)(duty_b * arr));
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_3, (uint32_t)(duty_c * arr));
}
```

---

# 8. TIM1 中断里调用

比如 PWM 频率 20kHz：

```text
周期 = 1 / 20000 = 0.00005s
```

```c
void TIM1_UP_IRQHandler(void)
{
    if (__HAL_TIM_GET_FLAG(&htim1, TIM_FLAG_UPDATE) != RESET)
    {
        if (__HAL_TIM_GET_IT_SOURCE(&htim1, TIM_IT_UPDATE) != RESET)
        {
            __HAL_TIM_CLEAR_IT(&htim1, TIM_IT_UPDATE);

            /*
             * 20kHz 控制周期
             */
            foc_openloop_svpwm_update(0.00005f);
        }
    }
}
```

---

# 9. 重点再解释一下这几个变量

## `sector`

```c
sector = get_sector_by_rad(angle);
```

意思是：

```text
目标电压箭头现在在哪一块 60° 区域
```

例如：

```text
30°  → 第1扇区
90°  → 第2扇区
150° → 第3扇区
210° → 第4扇区
270° → 第5扇区
330° → 第6扇区
```

---

## `T1`

```text
当前扇区第一个相邻矢量作用多久
```

例如第 1 扇区：

```text
T1 对应 V1
```

---

## `T2`

```text
当前扇区第二个相邻矢量作用多久
```

例如第 1 扇区：

```text
T2 对应 V2
```

---

## `T0`

```text
剩下的时间，不输出有效矢量，用零矢量补足
```

```c
T0 = pwm_period - T1 - T2;
```

---

## `Ta/Tb/Tc`

```text
最终 A/B/C 三相的等效高电平时间
```

最后变成：

```c
duty_a = Ta / pwm_period;
duty_b = Tb / pwm_period;
duty_c = Tc / pwm_period;
```

---

# 10. 初学者版本：只看逻辑

你可以先这样理解整段代码：

```c
// 1. 我有一个旋转角度 theta_e
theta_e += speed_e * dt;

// 2. 这个角度决定我要输出哪个方向的电压
inverse_park(ud, uq, theta_e, &u_alpha, &u_beta);

// 3. 根据 u_alpha/u_beta 算出这个电压箭头的角度
angle = atan2(u_beta, u_alpha);

// 4. 判断箭头落在哪个 60° 扇区
sector = get_sector(angle);

// 5. 根据扇区，决定用哪两个相邻基本矢量
// 第1扇区用 V1/V2
// 第2扇区用 V2/V3
// 第3扇区用 V3/V4
// ...

// 6. 计算这两个矢量各用多久
T1 = ...
T2 = ...

// 7. 剩下的时间给零矢量
T0 = Ts - T1 - T2;

// 8. 换算成 A/B/C 三相 PWM 占空比
duty_a = ...
duty_b = ...
duty_c = ...

// 9. 写入 STM32 TIM1 CCR
TIM1->CCR1 = duty_a * ARR;
TIM1->CCR2 = duty_b * ARR;
TIM1->CCR3 = duty_c * ARR;
```

---

# 11. 你现在真正要记住的

传统扇区法 SVPWM 就是这几步：

```text
1. 算目标电压角度 angle
2. 判断 sector = 第几个 60° 区域
3. 算 T1、T2、T0
4. 根据 sector 分配 Ta、Tb、Tc
5. duty = Ta/Ts, Tb/Ts, Tc/Ts
6. 更新 PWM
```

最核心的是这句：

```text
扇区决定“用哪两个相邻电压矢量合成目标电压”
```

比如：

```text
第1扇区：用 V1 + V2
第2扇区：用 V2 + V3
第3扇区：用 V3 + V4
第4扇区：用 V4 + V5
第5扇区：用 V5 + V6
第6扇区：用 V6 + V1
```

实际工程里，我还是建议你先用 **min-max 零序注入法** 跑通；等电机能转，再回来理解这个传统扇区法会容易很多。


-----------

----
---

可以，**三通道当然可以**。
只是市场上常见示波器基本是：

```text
2通道
4通道
```

真正的 **3 通道示波器很少见**，所以大家通常会在 **2通道和4通道**之间选。

---

# 1. 为啥 FOC 更推荐 4 通道？

因为 FOC / SVPWM 是三相电机控制，天然有很多信号是“三个一组”的：

```text
A相
B相
C相
```

你如果只有 2 通道，就只能一次看两相。
如果有 4 通道，就可以同时看三相，还能多看一个关键参考信号。

比如：

```text
CH1：PWM_A
CH2：PWM_B
CH3：PWM_C
CH4：ADC触发点 / 电流采样 / 故障信号
```

这就是 4 通道的优势。

---

# 2. 三通道能不能看三相 PWM？

可以。

如果有三通道，你可以这样接：

```text
CH1：PWM_A
CH2：PWM_B
CH3：PWM_C
```

这样可以同时看三相 PWM 是否正常。

你可以检查：

```text
三相 PWM 是否都有输出
三相占空比是否变化正常
A/B/C 相序是否大概正确
有没有某一路不输出
有没有某一路占空比异常
```

所以单纯看三相 PWM，**三通道够用**。

---

# 3. 那为什么还要第 4 通道？

因为调 FOC 时，经常不是只看三相 PWM。

你还想同时看一个“参考信号”或“问题信号”。

例如：

## 情况 1：三相 PWM + ADC 触发点

```text
CH1：PWM_A
CH2：PWM_B
CH3：PWM_C
CH4：ADC触发点
```

FOC 电流采样很重要，ADC 最好在 PWM 噪声较小的位置采样。

第 4 通道可以帮你看：

```text
ADC是不是在PWM中间采样
ADC触发有没有错位
采样点是不是刚好踩到开关尖峰
```

这对 FOC 很关键。

---

## 情况 2：三相 PWM + 电流采样信号

```text
CH1：PWM_A
CH2：PWM_B
CH3：PWM_C
CH4：电流采样放大器输出
```

这样你可以看：

```text
电流采样有没有尖峰
电流采样和PWM开关边沿的关系
采样信号有没有饱和
采样信号是否在合理范围内
```

---

## 情况 3：三路霍尔 + PWM/相电流

如果电机带霍尔：

```text
CH1：Hall_A
CH2：Hall_B
CH3：Hall_C
CH4：PWM_A / 相电流 / 编码器Z相
```

三个霍尔刚好占 3 个通道。
第 4 通道可以看一个参考，比如 PWM 或相电压。

---

## 情况 4：编码器 A/B/Z + 方向/PWM

如果是增量编码器：

```text
CH1：Encoder_A
CH2：Encoder_B
CH3：Encoder_Z
CH4：PWM / DIR / 电流采样
```

如果只有 3 通道，看完 A/B/Z 就没法同时看控制输出了。

---

## 情况 5：两相上下桥互补 PWM

做三相桥最怕上下管直通。

比如同时看 A、B 两相上下桥：

```text
CH1：AH
CH2：AL
CH3：BH
CH4：BL
```

这样你可以同时确认：

```text
A相上下桥有没有死区
B相上下桥有没有死区
两相之间时序有没有异常
```

如果只有 3 通道，就差一点。

---

# 4. 两通道、三通道、四通道怎么区别？

## 2 通道

能用，但效率低。

比如看三相 PWM，你要分两次：

```text
第一次：CH1 = PWM_A，CH2 = PWM_B
第二次：CH1 = PWM_A，CH2 = PWM_C
```

看死区：

```text
第一次：CH1 = AH，CH2 = AL
第二次：CH1 = BH，CH2 = BL
第三次：CH1 = CH，CH2 = CL
```

缺点是：

```text
不能同时看完整三相
不好判断瞬间问题
排查效率低
```

---

## 3 通道

比 2 通道好，能同时看三相。

适合：

```text
PWM_A / PWM_B / PWM_C
Hall_A / Hall_B / Hall_C
三相电流采样 Ia / Ib / Ic
```

但缺点是：

```text
没有多余通道看ADC触发
没有多余通道看故障脚
没有多余通道看母线电压/电流
没有多余通道做参考
```

---

## 4 通道

最舒服。

因为它可以：

```text
三相 + 一个参考
两相上下桥
三霍尔 + 一个PWM
编码器ABZ + 一个参考
两路电流 + PWM + ADC触发
```

所以大家推荐 4 通道，不是因为 3 通道不能用，而是因为：

```text
三相电机刚好需要3路；
调试时通常还需要第4路参考信号。
```

---

# 5. FOC 最常见的 4 通道接法

## 接法 A：看三相 PWM 和 ADC 触发

```text
CH1：PWM_A
CH2：PWM_B
CH3：PWM_C
CH4：ADC_TRGO
```

用途：

```text
确认SVPWM三相输出
确认ADC采样点位置
```

---

## 接法 B：看三相 PWM 和电流采样

```text
CH1：PWM_A
CH2：PWM_B
CH3：PWM_C
CH4：Ia_sample
```

用途：

```text
看电流采样尖峰
看采样和PWM边沿关系
```

---

## 接法 C：看两相互补 PWM

```text
CH1：AH
CH2：AL
CH3：BH
CH4：BL
```

用途：

```text
检查上下桥互补
检查死区时间
检查是否有直通风险
```

---

## 接法 D：看三路霍尔和相电压

```text
CH1：Hall_A
CH2：Hall_B
CH3：Hall_C
CH4：Phase_A_PWM / 相电压
```

用途：

```text
确认霍尔相序
确认霍尔和电机相位关系
```

---

## 接法 E：看编码器和控制输出

```text
CH1：Encoder_A
CH2：Encoder_B
CH3：Encoder_Z
CH4：PWM_A / 电流采样
```

用途：

```text
确认编码器方向
确认Z相位置
确认PWM和编码器关系
```

---

# 6. 做平衡车为什么更建议 4 通道？

平衡车比普通风扇、电机转速控制更麻烦，因为它要求：

```text
低速大扭矩
快速正反转
电流响应快
不能失控
IMU姿态和电机力矩配合
```

你调试时可能同时想看：

```text
CH1：左电机PWM
CH2：右电机PWM
CH3：IMU中断/控制周期GPIO
CH4：电流采样/故障脚
```

或者：

```text
CH1：Hall_A
CH2：Hall_B
CH3：Hall_C
CH4：电流采样
```

多一个通道，排查问题会轻松很多。

---

# 7. 结论

**三通道可以。**

如果真有靠谱三通道示波器，用来做 FOC 可以看：

```text
三相PWM
三路霍尔
三相电流采样
```

但是实际购买时更推荐 4 通道，因为：

```text
三相电机本来就占3路；
第4路可以看ADC触发、电流采样、故障脚、编码器Z相、控制周期GPIO等参考信号。
```

所以不是“三通道不行”，而是：

```text
2通道：能用，但麻烦
3通道：够看三相，但少一个参考
4通道：最适合FOC调试
```

你做 FOC / 平衡车，我建议：

```text
预算紧：2通道也能入门
想舒服：4通道更合适
三通道：理论可以，但市面少，不如直接买4通道
```


------
------

------

够用。**做 FOC / SVPWM / 平衡车电机调试，70MHz 示波器带宽基本够用**，尤其你主要看的是 MCU PWM、霍尔、编码器、ADC 采样、电流采样这些信号。

你要看的 FOC 信号大多是：

```text
PWM频率：10kHz ~ 30kHz
电流环频率：10kHz ~ 20kHz
霍尔信号：几十 Hz ~ 几 kHz
编码器信号：几 kHz ~ 几百 kHz
SPI/IIC/UART：几百 kHz ~ 几十 MHz
```

所以 70MHz 对这些是够的。

---

## 1. 70MHz 能看什么？

可以放心看：

```text
STM32 TIM1/TIM8 PWM
三相 PWM_A / PWM_B / PWM_C
上下桥互补 PWM
死区时间
霍尔 A/B/C
编码器 A/B/Z
ADC 触发点
电流采样放大器输出
母线电压低频波动
UART / IIC / SPI 普通信号
```

比如你做 FOC，PWM 频率一般：

```text
16kHz / 20kHz / 24kHz
```

这个频率对 70MHz 示波器来说很低。

---

## 2. 关键不是 PWM 频率，而是边沿速度

虽然 PWM 只有 20kHz，但是 PWM 是方波，方波边沿很快。

示波器带宽影响你看到的边沿形状。

示波器上升时间大概可以这样估：

```text
上升时间 ≈ 0.35 / 带宽
```

70MHz 示波器：

```text
0.35 / 70MHz ≈ 5ns
```

也就是说，70MHz 示波器理论上能分辨大约 5ns 量级的边沿变化。

对于普通 MCU PWM、霍尔、编码器信号，足够了。

---

## 3. 看死区够不够？

够。

FOC 里面常见死区时间大概：

```text
300ns ~ 2us
```

70MHz 示波器看这个完全够。

比如你测：

```text
CH1：AH 上桥 PWM
CH2：AL 下桥 PWM
```

你要看的是：

```text
AH 关断后，AL 是否延迟一段时间再打开
AL 关断后，AH 是否延迟一段时间再打开
```

这个延迟通常几百 ns 到 1us，70MHz 可以看出来。

---

## 4. 70MHz 不太适合看什么？

不太适合严肃分析这些：

```text
MOS 栅极振铃细节
开关节点高频尖峰
EMI 高频噪声
几十 ns 以下的过冲
非常快的 GaN/SiC 驱动波形
高速差分信号
```

比如你想精确看：

```text
MOS管 Vgs 振铃
相线开关节点尖峰
几十 MHz 以上的噪声成分
开关损耗细节
```

那 70MHz 就不够专业了，最好 100MHz、200MHz、350MHz 以上，甚至配差分探头。

但你现在是学习 FOC、做平衡车原型，不是做电源 EMI 认证，70MHz 可以。

---

## 5. 70MHz 和 100MHz 差别大吗？

对你目前用途来说，差别不大。

```text
70MHz：够用
100MHz：更舒服一点
200MHz+：更适合看功率开关细节
```

真正影响使用体验的，除了带宽，还有：

```text
通道数
采样率
存储深度
触发稳定性
屏幕/操作体验
探头质量
是否有数学测量功能
```

做 FOC，我宁愿选：

```text
4通道 70MHz 好用台式示波器
```

也不一定选：

```text
2通道 100MHz 但操作不方便的示波器
```

因为 FOC 更需要多通道同时看。

---

## 6. 你的场景怎么选？

如果你看的是：

```text
STM32 F407 / G4 / FOC
PWM 20kHz
普通三相 BLDC/PMSM
霍尔/编码器
电流采样
平衡车低压 24V/36V/48V
```

那：

```text
70MHz 4通道：可以
```

如果你以后要深入做：

```text
大功率电机控制器
MOS驱动波形优化
栅极电阻调参
开关损耗分析
EMI整改
高频振铃分析
```

那再考虑：

```text
200MHz 以上 + 差分探头 + 电流探头
```

---

## 结论

**70MHz 带宽够你学 FOC、调 SVPWM、做平衡车原型。**

更重要的是：

```text
优先 4通道
采样率最好 1GSa/s 左右
触发要稳定
配 10x 探头
低压限流调试
不要乱夹功率高边/浮地节点
```

如果你说的是 RIGOL DHO804 这种 **70MHz 4通道**，我觉得比很多便宜 USB 示波器更适合你长期学习用。

