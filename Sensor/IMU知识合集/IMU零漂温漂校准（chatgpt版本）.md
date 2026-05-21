IMU（陀螺仪 + 加速度计）输出的 **XYZ角速度（gyro）和加速度（acc）**，在实际产品里必须做 **零偏（bias）校准** 和 **温漂（temperature drift）校准**，否则误差会越来越大，尤其是你做 **头部追踪 / 空间音频** 这种应用，对稳定性要求很高。

我按 **工程实际常用方法** 给你讲（耳机、手机、无人机、VR基本都是类似方法）。

---

# 一、零飘校准（Zero Bias Calibration）

零飘就是：

**设备完全静止时，理论输出应该是：**

陀螺仪：

```
gyro_x = 0
gyro_y = 0
gyro_z = 0
```

加速度计：

```
acc_x = 0
acc_y = 0
acc_z = 1g
```

但实际会变成：

```
gyro_x = 0.15 dps
gyro_y = -0.23 dps
gyro_z = 0.07 dps
```

这个就是 **零偏 bias**。

---

## 1 静态平均法（最常用）

生产和启动时都可以做。

步骤：

1. **设备保持静止**
2. 采集 N 次数据
3. 求平均值
4. 作为 bias

### 陀螺仪

公式：

```
bias_gx = mean(gx)
bias_gy = mean(gy)
bias_gz = mean(gz)
```

校正：

```
gx_correct = gx_raw - bias_gx
gy_correct = gy_raw - bias_gy
gz_correct = gz_raw - bias_gz
```

---

### 加速度计

如果设备水平放置：

```
bias_ax = mean(ax)
bias_ay = mean(ay)
bias_az = mean(az) - 1g
```

校正：

```
ax_correct = ax_raw - bias_ax
ay_correct = ay_raw - bias_ay
az_correct = az_raw - bias_az
```

---

### 代码示例

```c
#define CALI_SAMPLES 500

float gx_sum=0, gy_sum=0, gz_sum=0;

for(int i=0;i<CALI_SAMPLES;i++)
{
    imu_read(&gx,&gy,&gz);

    gx_sum += gx;
    gy_sum += gy;
    gz_sum += gz;

    delay_ms(2);
}

gyro_bias_x = gx_sum / CALI_SAMPLES;
gyro_bias_y = gy_sum / CALI_SAMPLES;
gyro_bias_z = gz_sum / CALI_SAMPLES;
```

---

# 二、动态零偏估计（运行时校准）

很多产品（耳机 / 手机）会 **自动更新 bias**。

当检测到 **设备静止**：

条件例如：

```
|gyro| < 1 dps
|acc| ≈ 1g
```

就更新 bias：

```
bias = (1-α)*bias + α*gyro
```

例如：

```
α = 0.01
```

优点：

* 自动修正
* 长时间稳定

---

# 三、温漂校准（Temperature Calibration）

IMU **bias 会随温度变化**。

例如：

```
25°C bias = 0.1 dps
50°C bias = 0.6 dps
```

这就是 **温漂**。

解决方法是：

**建立 bias vs temperature 模型**

---

## 方法1：线性温度模型（最常用）

实验步骤：

在温箱中测试：

```
温度      gyro_bias_x
0°C       0.20
25°C      0.10
50°C      0.40
70°C      0.60
```

拟合：

```
bias(T) = k*T + b
```

运行时：

```
gyro_correct = gyro_raw - (k*T + b)
```

---

## 方法2：查表（LUT）

建立温度表：

```
temp_table

T      bias_x
0      0.2
20     0.1
40     0.3
60     0.5
```

运行时：

**插值**

```
bias = interpolate(temp)
```

---

# 四、耳机 / 手机 常见算法结构

实际产品通常这样：

```
             +-------------+
IMU RAW ---> | temp comp   |
             +-------------+
                    |
                    v
             +-------------+
             | bias remove |
             +-------------+
                    |
                    v
             +-------------+
             | filter      |
             +-------------+
                    |
                    v
              Sensor Fusion
```

流程：

```
RAW → 温漂补偿 → 零偏补偿 → 滤波 → 姿态算法
```

---

# 五、加速度计完整校准（6面校准）

如果要高精度（无人机常用）：

需要 **六面校准**。

设备分别放置：

```
+X
-X
+Y
-Y
+Z
-Z
```

求：

```
scale + bias
```

模型：

```
A_correct = scale * (A_raw - bias)
```

---

# 六、耳机头部追踪常见策略（你这个行业）

TWS耳机一般：

### 陀螺仪

```
启动校准 + 静止自动更新
```

### 加速度计

```
低通滤波
```

### 温漂

很多 IMU（比如 **TDK ICM45686**）内部已经有 **温度补偿**。

所以很多产品：

**只做 gyro bias 校准**。

---

# 七、IMU误差模型（完整工程模型）

IMU模型：

```
ω_meas = ω_true + bias + noise + temp_drift
```

补偿：

```
ω_correct = ω_meas - bias(T)
```

---

# 八、你做耳机最关键的一件事

空间音频头追踪最重要的是：

**陀螺仪 bias 稳定**

因为角速度积分：

```
angle = ∫ gyro dt
```

如果：

```
bias = 0.5 dps
```

一分钟误差：

```
30°
```

所以 **bias补偿是核心**。

---

如果你愿意，我可以再给你讲一个 **行业里真正用的 IMU 校准算法**：

* **Allan Variance（陀螺噪声分析）**
* **9参数 IMU校准**
* **手机 / 苹果AirPods头追踪算法结构**

这些是 **IMU算法工程师级别的知识**。
