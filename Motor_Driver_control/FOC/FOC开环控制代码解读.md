这段代码实现了一个开环速度控制系统，用于控制电机转速。它通过PWM控制器输出电压信号，来驱动电机转动。具体来说，它在指定目标速度下，持续通过电压信号进行控制。下面是对该代码详细流程的逐步解析：

### 1. **宏定义与变量初始化**

```cpp
#define _constrain(amt,low,high) ((amt)<(low)?(low):((amt)>(high)?(high):(amt)))
```

* 该宏定义用于限制一个值在一个指定的范围内。对于PWM占空比，它确保其值不会超过0到1的范围。

### 2. **全局变量**

```cpp
float voltage_power_supply=12.6;
float shaft_angle=0, open_loop_timestamp=0;
float zero_electric_angle=0, Ualpha, Ubeta=0, Ua=0, Ub=0, Uc=0, dc_a=0, dc_b=0, dc_c=0;
```

* `voltage_power_supply` 表示电源电压，设为12.6V。
* `shaft_angle` 是电机的机械角度。
* `open_loop_timestamp` 用来存储上次控制的时间戳。
* `Ualpha`, `Ubeta`, `Ua`, `Ub`, `Uc` 是三相电压和控制信号。
* `dc_a`, `dc_b`, `dc_c` 是各个PWM通道的占空比。

### 3. **`setup()` 函数**

```cpp
void setup() {
  Serial.begin(115200);
  pinMode(pwmA, OUTPUT);
  pinMode(pwmB, OUTPUT);
  pinMode(pwmC, OUTPUT);
  ledcAttachPin(pwmA, 0);
  ledcAttachPin(pwmB, 1);
  ledcAttachPin(pwmC, 2);
  ledcSetup(0, 30000, 8);
  ledcSetup(1, 30000, 8);
  ledcSetup(2, 30000, 8);
  Serial.println("完成PWM初始化设置");
  delay(3000);
}
```

* `setup()`函数是Arduino中的初始化函数。它初始化串口通信、PWM输出引脚以及PWM频率和精度。
* `ledcAttachPin()` 用于绑定引脚和PWM通道，`ledcSetup()` 设置了PWM的频率和精度。

### 4. **`_electricalAngle()` 函数**

```cpp
float _electricalAngle(float shaft_angle, int pole_pairs) {
  return (shaft_angle * pole_pairs);
}
```

* 计算电机的电角度。电机的电角度与机械角度的关系是：`电角度 = 机械角度 * 极对数`。这里假设极对数为7。

### 5. **`_normalizeAngle()` 函数**

```cpp
float _normalizeAngle(float angle) {
  float a = fmod(angle, 2*PI);   
  return a >= 0 ? a : (a + 2*PI);  
}
```

* 该函数将角度归一化到 `0` 到 `2π` 的范围。`fmod()` 用于求余数，确保角度不超过 `2π`。

### 6. **`setPwm()` 函数**

```cpp
void setPwm(float Ua, float Ub, float Uc) {
  dc_a = _constrain(Ua / voltage_power_supply, 0.0f , 1.0f );
  dc_b = _constrain(Ub / voltage_power_supply, 0.0f , 1.0f );
  dc_c = _constrain(Uc / voltage_power_supply, 0.0f , 1.0f );
  ledcWrite(0, dc_a*255);
  ledcWrite(1, dc_b*255);
  ledcWrite(2, dc_c*255);
}
```

* 该函数将计算得到的电压值 `Ua`, `Ub`, `Uc` 转化为PWM信号的占空比。
* `ledcWrite()` 将占空比输出到PWM通道。

### 7. **`setPhaseVoltage()` 函数**

```cpp
void setPhaseVoltage(float Uq, float Ud, float angle_el) {
  angle_el = _normalizeAngle(angle_el + zero_electric_angle);
  Ualpha = -Uq*sin(angle_el); 
  Ubeta = Uq*cos(angle_el);
  Ua = Ualpha + voltage_power_supply / 2;
  Ub = (sqrt(3) * Ubeta - Ualpha) / 2 + voltage_power_supply / 2;
  Uc = (-Ualpha - sqrt(3) * Ubeta) / 2 + voltage_power_supply / 2;
  setPwm(Ua, Ub, Uc);
}
```

* 该函数负责将 `Uq` 和 `Ud`（两相电压分量）转换为三相电压 `Ua`, `Ub`, `Uc`。
* 通过帕克变换将 `Uq` 转换为 `Ualpha` 和 `Ubeta`。
* 再通过克拉克变换将这两个分量转换为三相电压输出，并调用 `setPwm()` 输出PWM信号。

### 8. **`velocityOpenloop()` 函数**

```cpp
float velocityOpenloop(float target_velocity) {
  unsigned long now_us = micros();
  float Ts = (now_us - open_loop_timestamp) * 1e-6f;
  if (Ts <= 0 || Ts > 0.5f) Ts = 1e-3f;
  
  shaft_angle = _normalizeAngle(shaft_angle + target_velocity * Ts);
  
  float Uq = voltage_power_supply / 3;
  setPhaseVoltage(Uq, 0, _electricalAngle(shaft_angle, 7));
  
  open_loop_timestamp = now_us;
  return Uq;
}
```

* 该函数是开环速度控制的核心部分。

  * 获取当前时间（单位为微秒），计算出每次调用的时间间隔 `Ts`。
  * 根据目标速度 `target_velocity` 和时间间隔计算出新的轴角度 `shaft_angle`。
  * 使用固定的 `Uq` 输出电压来驱动电机。
  * 调用 `setPhaseVoltage()` 计算三相电压并输出。

### 9. **`loop()` 函数**

```cpp
void loop() {
  velocityOpenloop(5);
}
```

* `loop()` 函数是Arduino中的主循环。它持续调用 `velocityOpenloop()` 函数，设定目标速度为 5 rad/s，从而保持电机以该速度运行。

### **总结流程：**

1. **初始化**：

   * 设置PWM引脚和通道。
   * 初始化串口通信。

2. **电角度计算**：通过机械角度和极对数计算电角度。

3. **角度归一化**：将角度转换到 `0` 到 `2π` 之间。

4. **PWM控制**：

   * 计算电压值。
   * 计算对应的PWM占空比，并输出PWM信号到电机。

5. **开环速度控制**：

   * 根据目标速度计算角度增量。
   * 输出固定电压，驱动电机转动。

6. **主循环**：每次循环更新电机转速，确保电机按目标速度运行。

### **优缺点分析：**

* **优点**：

  * 简单、易于理解，适用于无闭环反馈的开环控制系统。
  * 不需要传感器反馈，适合简单应用。
* **缺点**：

  * 没有速度和位置反馈，无法纠正外部扰动或负载变化带来的误差。
  * 精度较低，适用于不需要高精度控制的场合。
