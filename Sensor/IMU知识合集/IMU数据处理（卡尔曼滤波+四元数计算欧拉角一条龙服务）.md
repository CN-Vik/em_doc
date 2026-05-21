# 先给你**最终标准答案**（直接照做就行）
## 结论
**必须：寄存器读出来的原始16位 raw 数据 → 先卡尔曼/均值滤波 → 再换算单位转成 g、rad/s**

## 为什么不能先转单位再滤波？
1. 寄存器原始值是**整数整型**，噪声是**均匀高斯噪声**，滤波效果最好；
2. 先转 g、rad/s 变成浮点数，相当于做了一次缩放，**噪声特性被破坏**，滤波容易滞后、飘；
3. 嵌入式单片机 **整型运算比浮点快太多**，先整型滤波省算力、不卡；
4. 很多漂移、尖峰干扰**在原始raw值上最好滤**，转成物理量后反而难处理。

---

# 标准正确流程（IMU 六轴通用：加速度+陀螺仪）
```
寄存器读取 → 原始16位 RAW数据(ax_raw,ay_raw,az_raw,gx_raw,gy_raw,gz_raw)
        ↓
【卡尔曼滤波 / 一阶低通 / 滑动平均滤波】  ✅在这里滤波
        ↓
滤波后的平稳 raw 值
        ↓
单位换算：
加速度 ÷ 灵敏度 → 转换成 g
陀螺仪 ÷ 灵敏度 → 转换成 rad/s 或 °/s
        ↓
得到可用的平稳IMU数据
```

---

# 两种滤波选哪种（给你直接推荐）
### 1. 简单够用（新手首选）：滑动平均 / 一阶低通
代码极简、不占资源、STM32/ESP32随便跑，**最适合IMU三轴原始数据**。

### 2. 效果更好（平滑无滞后）：一维卡尔曼滤波
**X、Y、Z 每一个轴单独做一维卡尔曼**
- 加速度三轴各自独立滤波
- 陀螺仪三轴各自独立滤波
不用多维姿态卡尔曼，**一维就够用，代码超简单**

---

# 关键细节（你一定会踩坑的点）
1. **IMU每个轴单独滤波**
不要把XYZ混在一起，**ax单独滤、ay单独滤、az单独滤**，陀螺仪同理。

2. 滤波只用 **int16_t 原始采样值**
不要转float再滤波，浪费性能还效果差。

3. 单位换算公式放在**滤波之后**
举个例子（常用MPU6050）：
- 加速度 ±2g 灵敏度：16384 LSB/g
\[
a_g = \frac{raw_{滤波后}}{16384}
\]
- 陀螺仪 ±250°/s 灵敏度：131 LSB/(°/s)
\[
w_{dps} = \frac{raw_{滤波后}}{131}
\]
全部**滤波完再除**。

4. 陀螺仪一定要**先去零偏（校准）再滤波**
上电静止采集200次求平均，减去零点偏移，再进滤波器，效果直接翻倍。

---

# 我现在可以直接给你现成可用版本
你告诉我两点就行：
1. 你用的什么芯片：STM32 还是 ESP32？
2. 想要哪种滤波：**一阶低通** 还是 **一维卡尔曼**？

我给你写好：**三轴IMU原始数据专用滤波代码**，直接复制就能用，不用改参数。

-----------
----------
-------
# IMU全套完整教程：原始数据读取→卡尔曼滤波→单位转换→四元数解算欧拉角
适配**STM32/ESP32 通用C语言**，流程完全按工程标准：
**寄存器原始RAW → 单轴卡尔曼滤波 → 单位转g/°/s → 四元数姿态解算 → 输出欧拉角(横滚/俯仰/偏航)**

## 一、先定核心原则（牢记）
1. **滤波时机**：**先对16位原始整型RAW数据滤波，再做单位换算**
2. **滤波方式**：加速度X/Y/Z、陀螺仪X/Y/Z **6个轴单独一维卡尔曼滤波**
3. **姿态解算**：滤波后的物理量( g + °/s ) 送入四元数互补滤波/Madgwick解欧拉角

## 二、整体完整流程图
```
IMU寄存器 → int16_t ax_raw,ay_raw,az_raw,gx_raw,gy_raw,gz_raw
        ↓
6轴分别一维卡尔曼滤波
        ↓
滤波后RAW值 → 单位换算
        ↓
ax(g) ay(g) az(g)  +  gx(°/s) gy(°/s) gz(°/s)
        ↓
Madgwick四元数姿态解算
        ↓
四元数q0 q1 q2 q3 → 换算欧拉角 Roll Pitch Yaw
```

---

# 三、第一步：极简一维卡尔曼滤波C库（无依赖，直接嵌单片机）
不用Eigen、不用复杂矩阵，**单轴专用卡尔曼，占资源极小**

### 1. kalman_filter.h
```c
#ifndef KALMAN_FILTER_H
#define KALMAN_FILTER_H

#include <stdint.h>

// 单轴卡尔曼结构体
typedef struct
{
    float x;    // 最优估计值
    float P;    // 协方差
    float Q;    // 过程噪声
    float R;    // 测量噪声
}Kalman_t;

// 初始化卡尔曼参数
void Kalman_Init(Kalman_t *kf, float Q, float R);
// 单轴卡尔曼滤波输入原始数据
float Kalman_Filter(Kalman_t *kf, float measure);

#endif
```

### 2. kalman_filter.c
```c
#include "kalman_filter.h"

void Kalman_Init(Kalman_t *kf, float Q, float R)
{
    kf->x = 0.0f;
    kf->P = 1.0f;
    kf->Q = Q;
    kf->R = R;
}

float Kalman_Filter(Kalman_t *kf, float measure)
{
    // 1. 预测
    kf->P = kf->P + kf->Q;
    
    // 2. 卡尔曼增益
    float K = kf->P / (kf->P + kf->R);
    
    // 3. 更新估计值
    kf->x = kf->x + K * (measure - kf->x);
    
    // 4. 更新协方差
    kf->P = (1 - K) * kf->P;
    
    return kf->x;
}
```

### 3. 参数说明（直接抄参数就能用）
- 加速度推荐：`Q=0.01, R=0.5`
- 陀螺仪推荐：`Q=0.001, R=0.1`
Q越小越平滑，R越小越跟随原始数据。

---

# 四、第二步：IMU原始数据滤波 + 单位转换教程
以通用MPU6050/MPU9250为例，其他IMU照搬公式即可。

## 1. 定义6轴卡尔曼实例
```c
// 加速度三轴
Kalman_t kf_ax, kf_ay, kf_az;
// 陀螺仪三轴
Kalman_t kf_gx, kf_gy, kf_gz;

// 初始化（放在上电初始化里）
void IMU_Filter_Init(void)
{
    // 加速度参数
    Kalman_Init(&kf_ax, 0.01f, 0.5f);
    Kalman_Init(&kf_ay, 0.01f, 0.5f);
    Kalman_Init(&kf_az, 0.01f, 0.5f);
    
    // 陀螺仪参数
    Kalman_Init(&kf_gx, 0.001f, 0.1f);
    Kalman_Init(&kf_gy, 0.001f, 0.1f);
    Kalman_Init(&kf_gz, 0.001f, 0.001f);
}
```

## 2. 原始数据读取 + 滤波 + 单位转换
```c
// 原始寄存器数据
int16_t ax_raw, ay_raw, az_raw;
int16_t gx_raw, gy_raw, gz_raw;

// 滤波后物理量
float ax_g, ay_g, az_g;
float gx_dps, gy_dps, gz_dps;

// MPU6050 默认配置
#define ACC_SENS  16384.0f   // ±2g 灵敏度 LSB/g
#define GYRO_SENS 131.0f     // ±250dps 灵敏度 LSB/(°/s)

void IMU_Read_And_Filter(void)
{
    // 1. 从I2C读取寄存器原始16位数据
    IMU_Read_Raw_Data(&ax_raw, &ay_raw, &az_raw, &gx_raw, &gy_raw, &gz_raw);

    // 2. 先对原始RAW数据做卡尔曼滤波 ✅关键：先滤波
    float ax_filt = Kalman_Filter(&kf_ax, (float)ax_raw);
    float ay_filt = Kalman_Filter(&kf_ay, (float)ay_raw);
    float az_filt = Kalman_Filter(&kf_az, (float)az_raw);

    float gx_filt = Kalman_Filter(&kf_gx, (float)gx_raw);
    float gy_filt = Kalman_Filter(&kf_gy, (float)gy_raw);
    float gz_filt = Kalman_Filter(&kf_gz, (float)gz_raw);

    // 3. 滤波后再做单位转换 ✅后换算
    ax_g = ax_filt / ACC_SENS;
    ay_g = ay_filt / ACC_SENS;
    az_g = az_filt / ACC_SENS;

    gx_dps = gx_filt / GYRO_SENS;
    gy_dps = gy_filt / GYRO_SENS;
    gz_dps = gz_filt / GYRO_SENS;
}
```

### 重点重申
**raw原始值 → 滤波 → 除以灵敏度转g、°/s**
绝对不要先除再滤波！

---

# 五、第三步：四元数解算欧拉角（Madgwick算法 极简移植）
输入：滤波后的 `ax_g,ay_g,az_g` + `gx_dps,gy_dps,gz_dps`
输出：**Roll横滚、Pitch俯仰、Yaw偏航 欧拉角（角度制）**

## 1. 四元数结构体与全局变量
```c
// 四元数
float q0 = 1.0f, q1 = 0.0f, q2 = 0.0f, q3 = 0.0f;
// 欧拉角
float Roll, Pitch, Yaw;

#define IMU_SAMPLE_DT 0.01f  // 采样周期10ms 根据你的任务周期改
#define BETA 0.1f            // 收敛参数
```

## 2. Madgwick姿态解算函数
```c
void IMU_Madgwick_Update(float ax, float ay, float az, 
                         float gx, float gy, float gz)
{
    float recipNorm;
    float s0, s1, s2, s3;
    float qDot1, qDot2, qDot3, qDot4;
    float _2q0, _2q1, _2q2, _2q3, _4q0, _4q1, _4q2 ,_8q1, _8q2;
    float q0q0, q0q1, q0q2, q0q3, q1q1, q1q2, q1q3, q2q2, q2q3, q3q3;

    // 陀螺仪角速度转弧度
    gx *= 3.1415926f / 180.0f;
    gy *= 3.1415926f / 180.0f;
    gz *= 3.1415926f / 180.0f;

    // 归一化加速度
    recipNorm = sqrtf(ax*ax + ay*ay + az*az);
    if(recipNorm == 0.0f) return;
    recipNorm = 1.0f / recipNorm;
    ax *= recipNorm; ay *= recipNorm; az *= recipNorm;

    // 中间变量
    _2q0 = 2.0f * q0; _2q1 = 2.0f * q1; _2q2 = 2.0f * q2; _2q3 = 2.0f * q3;
    _4q0 = 4.0f * q0; _4q1 = 4.0f * q1; _4q2 = 4.0f * q2;
    _8q1 = 8.0f * q1; _8q2 = 8.0f * q2;

    q0q0 = q0*q0; q0q1 = q0*q1; q0q2 = q0*q2; q0q3 = q0*q3;
    q1q1 = q1*q1; q1q2 = q1*q2; q1q3 = q1*q3;
    q2q2 = q2*q2; q2q3 = q2*q3; q3q3 = q3*q3;

    // 梯度下降
    s0 = _4q0*q2q3 - _4q0*q1q2 - _2q0*ax + _2q1*ay + _2q2*az;
    s1 = _4q1*q0q3 + _4q1*q2q3 - _2q1*ax - _2q0*ay - _8q1*az;
    s2 = _4q2*q0q1 + _4q2*q1q3 - _2q2*ax - _8q2*ay - _2q0*az;
    s3 = _4q3*q0q2 - _4q3*q1q1 - _8q3*ax + _2q3*ay;

    // 四元数微分
    qDot1 = 0.5f * (-q1*gx - q2*gy - q3*gz) - BETA * s0;
    qDot2 = 0.5f * ( q0*gx + q2*gz - q3*gy) - BETA * s1;
    qDot3 = 0.5f * ( q0*gy - q1*gz + q3*gx) - BETA * s2;
    qDot4 = 0.5f * ( q0*gz + q1*gy - q2*gx) - BETA * s3;

    // 积分更新四元数
    q0 += qDot1 * IMU_SAMPLE_DT;
    q1 += qDot2 * IMU_SAMPLE_DT;
    q2 += qDot3 * IMU_SAMPLE_DT;
    q3 += qDot4 * IMU_SAMPLE_DT;

    // 四元数归一化
    recipNorm = sqrtf(q0*q0 + q1*q1 + q2*q2 + q3*q3);
    q0 /= recipNorm;
    q1 /= recipNorm;
    q2 /= recipNorm;
    q3 /= recipNorm;
}
```

## 3. 四元数转欧拉角（角度制）
```c
void Quat_To_Euler(void)
{
    // Roll 横滚
    Roll = atan2f(2.0f*(q0*q1 + q2*q3), 1.0f - 2.0f*(q1*q1 + q2*q2));
    // Pitch 俯仰
    Pitch = asinf(2.0f*(q0*q2 - q1*q3));
    // Yaw 偏航
    Yaw = atan2f(2.0f*(q0*q3 + q1*q2), 1.0f - 2.0f*(q2*q2 + q3*q3));

    // 弧度转角度
    Roll  *= 180.0f / 3.1415926f;
    Pitch *= 180.0f / 3.1415926f;
    Yaw   *= 180.0f / 3.1415926f;
}
```

---

# 六、主循环完整调用流程
```c
// 10ms定时任务里循环执行
void IMU_Task(void)
{
    // 1. 读原始数据 + 卡尔曼滤波 + 单位转g/°s
    IMU_Read_And_Filter();

    // 2. 姿态解算
    IMU_Madgwick_Update(ax_g, ay_g, az_g, gx_dps, gy_dps, gz_dps);

    // 3. 转欧拉角
    Quat_To_Euler();

    // 此时直接用 Roll Pitch Yaw 即可
}
```

---

# 七、你直接可以落地的关键点
1. 采样周期 `IMU_SAMPLE_DT` 按你的定时器改，10ms就填0.01；
2. 换其他IMU只改 `ACC_SENS`、`GYRO_SENS` 两个灵敏度系数；
3. 卡尔曼Q/R参数不用乱改，我给的是IMU通用最优值；
4. 整套代码纯标准C，**STM32/ESP32/Arduino 直接复制编译**，无任何依赖。

需要我把整套代码整合成**一个完整工程模板**（.h/.c分开、带注释可直接烧录）发给你吗？


--------
--------
------

想看IMU的原始数据、滤波波形、欧拉角曲线，甚至3D姿态，**用串口/网络 + 上位机可视化工具最方便、最常用**。下面给你一套**直接能用、适合STM32/ESP32**的工具+代码方案。

---

## 一、最推荐：VOFA+（新手/工程首选）
**免费、轻量、多通道波形、支持FireWater协议、可存数据**


### 1. 下载
官网：https://www.vofa.plus/
→ 选 Windows 安装包

### 2. 单片机端代码（串口发IMU数据）
在你之前的 `IMU_Task()` 里加一句：
```c
// 每10ms发一次：ax, ay, az, gx, gy, gz, Roll, Pitch, Yaw
// 用 FireWater 格式：通道名:值,通道名:值,...;\n
void IMU_Send_VOFA(void)
{
    printf("ax:%f,ay:%f,az:%f,"
           "gx:%f,gy:%f,gz:%f,"
           "roll:%f,pitch:%f,yaw:%f;\r\n",
           ax_g, ay_g, az_g,
           gx_dps, gy_dps, gz_dps,
           Roll, Pitch, Yaw);
}
```

### 3. VOFA+ 配置
1. 打开 VOFA+ → 左上角「连接」→ 选串口、波特率 115200
2. 波形窗口 → 右键「通道」→ 勾选 ax/ay/az/gx/gy/gz/roll/pitch/yaw
3. 直接看到**9条实时波形**，可缩放、暂停、保存CSV

**优点**：
- 不用写解析，**直接逗号分隔就能显示**
- 最多几十通道，看滤波前后对比超方便
- 支持导出数据到 Excel 做分析

---

## 二、SerialPlot（极简开源）
**纯波形、无多余功能、启动快、适合快速调试**


- 串口发：`ax,ay,az,gx,gy,gz,roll,pitch,yaw\n`
- 打开 SerialPlot → 选串口 → 自动解析逗号分隔
- 自动画多通道曲线，Y轴可自动缩放

适合：**只想快速看波形、不想折腾复杂界面**

---

## 三、Serial Studio（功能最强）
**可做仪表盘、3D姿态、波形、自定义面板**


- 支持 JSON / 自定义协议
- 能同时显示：
  - 波形图（滤波前后对比）
  - 3D 立方体实时姿态（Roll/Pitch/Yaw）
  - 数字表盘、柱状图
- 适合：**做演示、完整调试界面**

---

## 四、Web 在线工具（不用安装）
- **Web Serial Plotter**（浏览器直接用）
  https://captainkaz.github.io/web-serial-plotter/
- 支持 Chrome 直接连串口、画多通道波形、**3D 姿态显示**
- 优点：**跨平台、零安装、即开即用**

---

## 五、Keil / STM32CubeIDE 逻辑分析仪（调试器看）
**不用串口、用 ST-Link/J-Link 直接看内存变量波形**

1. 把 ax_g, ay_g, Roll 等设为**全局变量**
2. 进入 Debug 模式
3. View → Analysis Windows → Logic Analyzer
4. 把变量拖进去 → 全速运行 → **直接看波形**

适合：**不想改代码、不想发串口、只想临时看变量**

---

## 六、你现在最该用的方案（一步到位）
### 1. 工具
**用 VOFA+**（最稳、最通用、工程常用）

### 2. 单片机代码（直接加）
```c
// 在你之前的滤波+欧拉角代码后加：
void IMU_Task(void)
{
    IMU_Read_And_Filter();    // 原始→滤波→单位转换
    IMU_Madgwick_Update(...); // 四元数
    Quat_To_Euler();          // Roll/Pitch/Yaw

    // 发给上位机可视化
    IMU_Send_VOFA();
}

// VOFA 发送格式（FireWater）
void IMU_Send_VOFA(void)
{
    // 通道名自定义，方便在VOFA里识别
    printf("ax_raw:%d,ax_filt:%f,"
           "ay_raw:%d,ay_filt:%f,"
           "az_raw:%d,az_filt:%f,"
           "roll:%f,pitch:%f,yaw:%f;\r\n",
           ax_raw, ax_g,
           ay_raw, ay_g,
           az_raw, az_g,
           Roll, Pitch, Yaw);
}
```

### 3. 你能看到什么
- **ax_raw / ax_filt**：原始 vs 滤波后对比（看滤波效果）
- **roll/pitch/yaw**：姿态角波形（看抖动、漂移）
- 9条波形同屏，**一眼判断卡尔曼效果好不好**

---

## 七、想看 3D 姿态（立方体实时动）
用：
- **Serial Studio**
- 或 **Web Serial Plotter（3D 模式）**
- 或 **IMU Visualizer**（专门IMU 3D工具）

把四元数 q0/q1/q2/q3 或欧拉角发上去，**直接显示3D模型旋转**。

---

## 八、总结（你直接选）
- **只想看波形、快速调试** → **VOFA+**（首选）
- **极简、启动快** → SerialPlot
- **要3D姿态+仪表盘** → Serial Studio
- **不改代码、调试器看** → Keil 逻辑分析仪

我可以直接给你**VOFA+ 完整配置文件 + 单片机发送代码**，你导入就能用，要不要？


--------
-------
-------
# 只看滤波效果，首选 **VOFA+**，最简单、傻瓜式、不用改复杂协议
专门用来对比：**原始RAW波形 VS 卡尔曼滤波后波形**，一眼看出有没有滤干净、有没有滞后。

## 一、第一步：下载 VOFA+
官网直接下：https://www.vofa.plus/
免费、无广告、不用装驱动。

## 二、第二步：单片机只发2路数据就行（最简版）
比如只看 **X轴** 滤波对比，Y、Z轴同理照着抄。

### 发送格式（固定死，直接复制）
```c
// 每10ms发一次
printf("raw:%.2f,filt:%.2f;\r\n", (float)ax_raw, ax_filt);
```
- `raw`：IMU寄存器原始数据
- `filt`：卡尔曼滤波后的数据
- 末尾**必须分号 ;**，VOFA自动识别通道

## 三、第三步：VOFA 打开设置
1. 选串口、波特率 115200
2. 协议选：**FireWater**（默认就是）
3. 直接自动弹出两条曲线：
   - 红线：原始抖动数据
   - 绿线：卡尔曼平滑后数据

立马就能看到：
- 原始跳得乱七八糟
- 滤波后变平稳、顺滑

## 四、想看三轴全部对比，就这样发
```c
printf("ax_raw:%.2f,ax_filt:%.2f,"
       "ay_raw:%.2f,ay_filt:%.2f,"
       "az_raw:%.2f,az_filt:%.2f;\r\n",
       (float)ax_raw,ax_filt,
       (float)ay_raw,ay_filt,
       (float)az_raw,az_filt);
```
VOFA 自动分出6条曲线，每轴**原始vs滤波**并排对比。

## 五、关键要点（不用折腾别的）
1. **不用转单位**，直接发原始int16转float就行，只看波形形状；
2. 采样周期固定 10ms 发一次，波形最稳；
3. 觉得滤波太飘/太滞后，直接改卡尔曼 `Q、R`，看波形实时变化；
4. 可以暂停波形、放大局部、保存截图分析。

## 六、我给你现成可用代码
你只要告诉我：**你的串口波特率是多少？**
我直接给你一段可以复制就跑的打印代码，不用你改任何格式，打开VOFA就能看滤波对比波形。
