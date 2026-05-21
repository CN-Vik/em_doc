![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/abac91496d3d40d1bd25e41ae915402e.png)

---
![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/8e94536bb5164de7a6d62d32ce6e2ca4.png)


--------
------

从你这张图看，**不一定有问题**。你现在看到的是 **30kHz 左右的 PWM 载波波形**，不是三相电机真正的低频“相电压/相电流基波”。

你说“也不是间隔 120 度”，这个理解点很关键：

**FOC/SVPWM 的三相 PWM，不是三路方波整体错开 120°。**

正确情况通常是：

```c
U 相 PWM：30kHz 载波，占空比按正弦/空间矢量变化
V 相 PWM：30kHz 载波，占空比也在变化，但相位比 U 相差 120°电角度
W 相 PWM：30kHz 载波，占空比也在变化，但相位再差 120°电角度
```

也就是说，**相差 120°的是占空比包络/等效平均电压/电流，不是每个 PWM 脉冲的边沿。**

---

你图里显示：

```text
PWM 频率 ≈ 30.05kHz
周期 ≈ 33.3us
```

这个是正常的 PWM 频率。

如果你误以为三相 PWM 应该这样：

```text
U 相：|‾‾‾|___|‾‾‾|___
V 相：   |‾‾‾|___|‾‾‾|___
W 相：      |‾‾‾|___|‾‾‾|___
```

也就是三路 PWM 方波彼此延迟 `PWM周期 / 3`，那这个想法是不对的。

FOC 里更像这样：

```text
三路 PWM 载波：同频、同步
区别在于：每一路的占空比不一样

U: |‾‾‾‾‾‾|__
V: |‾‾|______
W: |‾‾‾‾|____
```

过一段时间后，占空比慢慢变成：

```text
U: |‾‾‾|_____
V: |‾‾‾‾‾‾|__
W: |‾|_______
```

再过一段时间继续变化。这个“占空比变化的包络”才是三相相差 120°。

---

你现在示波器时间档位是：

```text
20us / 格
```

全屏大概也就几百微秒，只能看到几个 30kHz PWM 周期。

但是电机的电角度周期一般可能是几毫秒、几十毫秒，甚至更长。比如电角频率 100Hz：

```text
电周期 = 10ms
120° = 10ms / 3 = 3.33ms
```

你现在一屏才几百微秒，当然看不出来 120°。

---

正确看三相 120°的方法有几个：

### 方法一：把时间档调大

比如调成：

```text
1ms/div
2ms/div
5ms/div
10ms/div
```

然后看三路 PWM 的**占空比包络**是不是在轮流变化。

你应该看到类似：

```text
U 相占空比：大 -> 中 -> 小 -> 中 -> 大
V 相占空比：中 -> 小 -> 中 -> 大 -> 中
W 相占空比：小 -> 中 -> 大 -> 中 -> 小
```

---

### 方法二：加 RC 低通滤波看等效电压

比如每一路 PWM 后面临时接一个简单 RC：

```text
PWM输出 —— 10k电阻 —— 测量点 —— 0.1uF电容 —— GND
```

截止频率大概：

```text
fc ≈ 1 / (2πRC)
   ≈ 1 / (2π * 10k * 0.1uF)
   ≈ 159Hz
```

这样 30kHz PWM 会被滤掉，你看到的是接近正弦的平均电压包络。

这时候三相应该大致是：

```text
U:   正弦波
V:   比 U 滞后/超前 120°
W:   再差 120°
```

注意：如果你用的是 SVPWM，滤出来可能不是完美正弦，但相位关系应该能看出来。

---

### 方法三：直接看代码里的三个占空比/CCR 值

比如你打印或者调试看：

```c
pwm_u
pwm_v
pwm_w
```

正常 FOC/SVPWM 开环时，它们应该随着电角度 `theta` 变化：

```c
d_u = 0.5 + k * sin(theta);
d_v = 0.5 + k * sin(theta - 120°);
d_w = 0.5 + k * sin(theta + 120°);
```

或者 SVPWM 下会有零序注入，但本质还是三相相差 120°的等效电压。

---

你这张图里还有一个要注意的地方：右边通道耦合好像是 **交流 AC 耦合**。

测 PWM 建议改成：

```text
DC 耦合
探头倍率设置正确：x1 或 x10 要和实际探头一致
垂直档位调到 1V/div 或 2V/div
```

如果你测的是 MCU PWM 引脚，正常应该能看到：

```text
0V ~ 3.3V
```

或者：

```text
0V ~ 5V
```

但你图上测出来 Vmax 只有几百 mV，这可能是因为：

```text
1. 示波器通道用了 AC 耦合
2. 探头倍率设置不对
3. 测量点不是 MCU PWM 原始引脚
4. 地线接法有问题
5. 信号被驱动芯片/外围电路影响
```

所以先把示波器通道改成 **DC 耦合** 再看。

---

结论：

你这张图**不能说明 FOC 三相有问题**。
你看到的是三路同步 30kHz PWM，这是正常现象。

真正要判断对不对，要看：

```text
1. 三路 PWM 频率是否一致：你这里看起来一致，约 30kHz
2. 三路占空比是否随电角度连续变化
3. 三路占空比包络是否相差 120°电角度
4. 高低桥是否互补，并且有死区
5. 电机三相电流是否接近正弦
6. 开环加速时电机是否平滑旋转、不抖、不啸叫、不过流
```

另外，测功率桥三相输出时要小心：**普通示波器地夹不要随便夹到电机相线/半桥输出上**，容易把桥臂或电源短路。测 MCU 低压 PWM 引脚没问题；测三相功率输出最好用差分探头或隔离测量。


----
----

-----
对，你这个理解已经接近核心了：

**一个 PWM 周期内的占空比 ≈ 这一瞬间你想给某一相的“等效平均电压”。**

所以每个 PWM 周期，其实就是在输出三相电压波形上的一个采样点：

```c
U相：当前这个点的电压
V相：比 U 相差 120° 电角度的那个点
W相：比 U 相差 240° 电角度的那个点
```

如果你不断更新这三个占空比，电机看到的就不是一个点，而是一组连续变化的三相电压。

---

## 你问：输出三个相差 120° 的 PWM，能不能驱动 BLDC？

答案是：**能，但要看你说的“相差 120°”是哪一种。**

---

## 情况 1：三路 PWM 载波本身错开 120°

比如你想让 30kHz PWM 这样：

```text
U PWM：|‾‾‾|___|‾‾‾|___
V PWM：   |‾‾‾|___|‾‾‾|___
W PWM：      |‾‾‾|___|‾‾‾|___
```

这个**不是 FOC 的正确做法**。

FOC 里面三路 PWM 载波通常是同步的，差别是三个通道的**占空比不同**。

---

## 情况 2：三个相电压的目标值相差 120°

这个就可以驱动 BLDC。

比如开环正弦控制：

```c
duty_u = 0.5f + amp * sin(theta);
duty_v = 0.5f + amp * sin(theta - 2.0f * PI / 3.0f);
duty_w = 0.5f + amp * sin(theta + 2.0f * PI / 3.0f);
```

这里真正相差 120° 的是：

```text
U 相等效电压
V 相等效电压
W 相等效电压
```

不是 PWM 载波边沿。

这三个占空比送进三相桥，经过电机绕组电感滤波以后，会形成接近三相正弦电流，从而产生旋转磁场，电机就能转。

---

## 关键点：BLDC 不是直接接三根 PWM 线

不能这样：

```text
MCU PWM_U 直接接电机 U 相
MCU PWM_V 直接接电机 V 相
MCU PWM_W 直接接电机 W 相
```

必须是：

```text
MCU PWM
   ↓
三相栅极驱动器
   ↓
6 个 MOSFET / 三相半桥
   ↓
BLDC 三相绕组
```

结构大概是：

```text
          VMOTOR+
            |
       Q1   Q3   Q5
        |    |    |
        U    V    W  → 接电机三相
        |    |    |
       Q2   Q4   Q6
            |
          GND
```

每一相都是一个半桥。

通常有两种控制方式：

```text
1. 6 路 PWM：
   UH / UL
   VH / VL
   WH / WL

2. 3 路 PWM + 驱动芯片自动生成互补下桥
   U_PWM
   V_PWM
   W_PWM
```

但是一定要有：

```text
死区时间
过流保护
欠压保护
电流采样
合适的 MOSFET 驱动
```

否则很容易上下桥直通，直接炸 MOS。

---

## 你现在说的这种，本质上就是开环三相电压控制

流程是：

```c
theta += speed_elec * Ts;   // 电角度不断往前走

U = sin(theta);
V = sin(theta - 120°);
W = sin(theta + 120°);

把 U/V/W 转换成三个 PWM 占空比
```

效果是：你人为制造了一个旋转磁场。

如果转子能跟上这个旋转磁场，电机就会转。

---

## 但是：能转 ≠ 稳定好用

开环三相 PWM 可以让 BLDC 转起来，尤其是空载、小负载时。

但是问题是：**你不知道转子实际在哪里。**

比如你让磁场这样转：

```text
0° → 10° → 20° → 30° → 40° ...
```

但转子可能实际还在：

```text
0° → 3° → 8° → 15° ...
```

如果磁场跑太快，转子跟不上，就会：

```text
抖动
啸叫
失步
堵转
电流暴涨
发热
启动失败
```

所以开环速度控制一般只能用于：

```text
轻载启动
风扇类负载
简单演示
FOC 启动前的预定位/强拖阶段
```

不适合直接用于：

```text
平衡车
大负载电机
低速大扭矩控制
需要可靠启动的产品
```

---

## BLDC 还有一种更传统的 120° 方波驱动

这个叫六步换相，不是 FOC。

典型换相表是：

```text
步骤    U相    V相    W相
1       +      -      悬空
2       +      悬空   -
3       悬空   +      -
4       -      +      悬空
5       -      悬空   +
6       悬空   -      +
```

每 60° 电角度切换一次状态。

这个也能驱动 BLDC，而且很多普通无刷电调就是这么干的。

但是它的特点是：

```text
优点：简单
缺点：转矩脉动大，噪声大，低速不够平滑
```

FOC 的目标是让三相电流更接近正弦，让磁场和转子磁场角度更合理，所以更平滑、更安静、更高效。

---

## 所以你的问题最终答案是

可以驱动，但要分清楚：

```text
三路 30kHz PWM 载波错开 120°：
不推荐，不是 FOC 的核心。

三路等效相电压相差 120°：
可以，这就是三相正弦开环/SPWM/FOC 的基础。

六步 120° 导通：
也可以，这是传统 BLDC 方波驱动。
```

你现在做 FOC 开环速度控制，建议你的思路是：

```c
1. PWM 频率固定，比如 20kHz~30kHz
2. 每个 PWM 周期或每隔几个 PWM 周期更新一次 theta
3. 根据 theta 算 U/V/W 三相占空比
4. 三相桥输出到电机
5. 启动时 theta 不能突然高速旋转，要慢慢加速
6. amp 电压幅值也要从小慢慢加
```

最核心的一句话：

**BLDC 能不能转，不看三路 PWM 边沿是不是差 120°，而是看三相绕组里最终形成的电流磁场是不是按 120° 电角度旋转。**


-----
-------
--------
# DengFOC开环无刷电机控制代码完整解读
这是**灯哥开源的纯手写ESP32无刷电机开环FOC控制代码**，不依赖任何第三方FOC库，仅使用ESP32原生LED PWM外设实现三相PWM输出，是学习FOC原理的绝佳入门代码。

## 一、完整IO配置清单（核心问题）
代码中总共使用了**4个GPIO引脚**，全部配置为输出模式，具体如下：

| 引脚号 | 功能定义 | 工作模式 | 详细配置 |
|--------|----------|----------|----------|
| GPIO12 | 电机驱动使能引脚 | 数字输出 | 初始电平：HIGH（高电平使能V4版本电机驱动板） |
| GPIO32 | PWM A相输出 | LED PWM输出 | 绑定通道0，频率30kHz，8位分辨率（0-255） |
| GPIO33 | PWM B相输出 | LED PWM输出 | 绑定通道1，频率30kHz，8位分辨率（0-255） |
| GPIO25 | PWM C相输出 | LED PWM输出 | 绑定通道2，频率30kHz，8位分辨率（0-255） |

### IO配置关键说明
1. **30kHz PWM频率**：选择这个频率是为了**完全避开人耳听觉范围**（20Hz-20kHz），消除电机运行时的高频啸叫声
2. **8位分辨率**：对应占空比范围0-255，对于开环FOC控制已经足够，同时能保证30kHz的高频率输出
3. **GPIO12使能逻辑**：DengFOC V4驱动板采用**高电平使能**，低电平会切断电机电源
4. **ESP32 LED PWM特性**：最多支持16个独立PWM通道，可任意映射到GPIO引脚，非常适合多相电机控制

## 二、代码整体结构与核心逻辑
### 1. 全局变量与宏定义
```c
// 三相PWM输出引脚定义
int pwmA = 32;
int pwmB = 33;
int pwmC = 25;

// 数值约束宏，替代Arduino原生constrain函数
#define _constrain(amt, low, high) ((amt) < (low) ? (low) : ((amt) > (high) ? (high) : (amt)))

// 系统参数
float voltage_power_supply = 12.6;  // 电源电压（3S锂电池满电电压）
float shaft_angle = 0;              // 电机机械角度（开环积分得到）
float open_loop_timestamp = 0;      // 上一次循环的时间戳
float zero_electric_angle = 0;      // 电角度零点偏移（开环时无需校准）
```

### 2. 初始化函数setup()
```c
void setup() {
  Serial.begin(115200);  // 串口初始化，用于调试输出
  
  // 电机驱动使能
  pinMode(12, OUTPUT);
  digitalWrite(12, HIGH);  // 开启电机驱动电源
  
  // PWM引脚模式设置
  pinMode(pwmA, OUTPUT);
  pinMode(pwmB, OUTPUT);
  pinMode(pwmC, OUTPUT);
  
  // LED PWM控制器配置
  ledcSetup(0, 30000, 8);  // 通道0，30kHz，8位分辨率
  ledcSetup(1, 30000, 8);  // 通道1，30kHz，8位分辨率
  ledcSetup(2, 30000, 8);  // 通道2，30kHz，8位分辨率
  
  // 将PWM引脚绑定到对应的LED PWM通道
  ledcAttachPin(pwmA, 0);
  ledcAttachPin(pwmB, 1);
  ledcAttachPin(pwmC, 2);
  
  Serial.println("完成PWM初始化设置");
  delay(3000);  // 3秒延时，方便观察初始化完成
}
```

### 3. 核心数学辅助函数
```c
// 机械角度转换为电角度
// 电角度 = 机械角度 × 极对数
float _electricalAngle(float shaft_angle, int pole_pairs) {
  return (shaft_angle * pole_pairs);
}

// 将任意角度归一化到 [0, 2π] 范围
// 解决角度溢出和负数问题
float _normalizeAngle(float angle) {
  float a = fmod(angle, 2 * PI);
  return a >= 0 ? a : (a + 2 * PI);
}
```

### 4. PWM输出设置函数
```c
void setPwm(float Ua, float Ub, float Uc) {
  // 将三相电压转换为占空比（0-1）
  // 占空比 = 相电压 / 电源电压
  dc_a = _constrain(Ua / voltage_power_supply, 0.0f, 1.0f);
  dc_b = _constrain(Ub / voltage_power_supply, 0.0f, 1.0f);
  dc_c = _constrain(Uc / voltage_power_supply, 0.0f, 1.0f);

  // 写入PWM寄存器（8位分辨率对应0-255）
  ledcWrite(0, dc_a * 255);
  ledcWrite(1, dc_b * 255);
  ledcWrite(2, dc_c * 255);
}
```

### 5. FOC核心变换函数
```c
void setPhaseVoltage(float Uq, float Ud, float angle_el) {
  // 电角度归一化，加上零点偏移
  angle_el = _normalizeAngle(angle_el + zero_electric_angle);

  // 帕克逆变换（d-q坐标系 → α-β坐标系）
  // 开环控制时Ud=0，只需要Uq分量
  Ualpha = -Uq * sin(angle_el);
  Ubeta = Uq * cos(angle_el);

  // 克拉克逆变换（α-β坐标系 → 三相静止坐标系）
  // 这里使用了中心对齐PWM的简化计算，加上电源电压的一半作为直流偏置
  Ua = Ualpha + voltage_power_supply / 2;
  Ub = (sqrt(3) * Ubeta - Ualpha) / 2 + voltage_power_supply / 2;
  Uc = (-Ualpha - sqrt(3) * Ubeta) / 2 + voltage_power_supply / 2;
  
  // 设置三相PWM输出
  setPwm(Ua, Ub, Uc);
}
```

### 6. 开环速度控制函数
```c
float velocityOpenloop(float target_velocity) {
  // 获取当前系统时间（微秒）
  unsigned long now_us = micros();
  
  // 计算两次循环之间的时间间隔（秒）
  float Ts = (now_us - open_loop_timestamp) * 1e-6f;

  // 时间戳溢出保护（micros()约70分钟溢出一次）
  if (Ts <= 0 || Ts > 0.5f) Ts = 1e-3f;

  // 开环角度积分：通过目标速度和时间间隔计算当前机械角度
  // 这是开环控制的核心，没有编码器反馈，纯靠时间积分估算角度
  shaft_angle = _normalizeAngle(shaft_angle + target_velocity * Ts);

  // 设置q轴电压（决定输出力矩）
  // 最大安全值为 voltage_power_supply/2，超过会导致电压限幅失真
  float Uq = voltage_power_supply / 3;

  // 计算并输出三相电压
  // 极对数为7，对应DengFOC配套的2208电机
  setPhaseVoltage(Uq, 0, _electricalAngle(shaft_angle, 7));

  // 更新时间戳
  open_loop_timestamp = now_us;

  return Uq;
}
```

### 7. 主循环函数
```c
void loop() {
  // 以5rad/s的角速度开环运行
  // 1rad/s ≈ 9.55rpm，5rad/s ≈ 47.75rpm（机械转速）
  velocityOpenloop(5);
}
```

## 三、代码关键特性与注意事项
1. **纯手写无库依赖**：代码不依赖SimpleFOC等任何第三方库，所有FOC算法都由基础数学运算实现，非常适合学习原理
2. **开环控制特性**：
   - 不需要编码器或霍尔传感器
   - 低速力矩较小，高速容易丢步
   - 适合入门学习和简单应用
3. **可修改参数**：
   - `voltage_power_supply`：根据实际电源电压修改（如12V电源改为12.0）
   - `pole_pairs`：根据电机极对数修改（代码中为7，对应2208电机）
   - `Uq`：调整输出力矩，最大不超过`voltage_power_supply/2`
   - `target_velocity`：修改电机目标转速（单位：rad/s）
4. **硬件兼容性**：仅在DengFOC V4开发板上测试过，其他ESP32开发板需要根据实际引脚定义修改

需要我帮你把这段代码改成**支持正反转切换**和**串口调速**的版本吗？