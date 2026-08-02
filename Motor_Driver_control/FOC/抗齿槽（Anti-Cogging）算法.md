实现抗齿槽（Anti-Cogging）算法，核心逻辑可以总结为一句话：**“离线测出一圈的波谷与波峰，在线运行时用 $I_q$ 前馈反向抵消。”**

在实际工程中，最标准、效果最好的实现方式是“查表插值前馈法”（LUT + Linear Interpolation）。

---

### 整体实现三步走

```
[步骤 1：双向慢速扫描] ➔ [步骤 2：数据处理提取纯齿槽波形] ➔ [步骤 3：实时 FOC 环插值前馈]

```

---

### 步骤 1：双向慢速扫描（剥离摩擦力）

如果你只朝着一个方向慢慢转去记录电流，你会把**摩擦力**和**齿槽转矩**混在一起。为了拿到“纯粹”的齿槽转矩，必须做**正反双向扫描**。

#### 物理数学原理：

* **正转时 $I_q$：** $I_{q\_fwd}(\theta) = I_{\text{cog}}(\theta) + I_{\text{fric}}$ （齿槽力 + 正向动摩擦）
* **反转时 $I_q$：** $I_{q\_rev}(\theta) = I_{\text{cog}}(\theta) - I_{\text{fric}}$ （齿槽力 - 反向动摩擦）

把两组数据对应角度相加求平均，**动摩擦力就被完美抵消掉了**：

$$I_{\text{cog}}(\theta) = \frac{I_{q\_fwd}(\theta) + I_{q\_rev}(\theta)}{2}$$

#### 标定流程：

1. **分割网格：** 将机械角 $0^\circ \sim 360^\circ$ 分成 $N$ 个网格点（比如 $N = 360$，即每 $1^\circ$ 一个点，占 360 个 float，ESP32 内存完全无压力）。
2. **正向极慢速旋转：** 开启位置环或极低速速度环（如 0.5 RPM），让电机正转一圈。记录每一个网格点上的稳定 $I_q$ 电流，存入数组 `iq_fwd[360]`。
3. **反向极慢速旋转：** 以同样的极低速反转一圈，记录每一个网格点上的稳定 $I_q$ 电流，存入数组 `iq_rev[360]`。

---

### 步骤 2：生成并平滑抗齿槽 LUT 表

在标定完成后，计算最终的补偿数组并做平滑处理：

```c
#define LUT_SIZE 360
float cogging_lut[LUT_SIZE];

void generate_cogging_lut(float *iq_fwd, float *iq_rev) {
    // 1. 抵消摩擦力，计算纯齿槽电流
    for (int i = 0; i < LUT_SIZE; i++) {
        cogging_lut[i] = (iq_fwd[i] + iq_rev[i]) / 2.0f;
    }

    // 2. 简单滑动平均滤波（消除采样高频噪声，避免补偿电流过于剧烈）
    float temp_lut[LUT_SIZE];
    for (int i = 0; i < LUT_SIZE; i++) {
        int prev = (i - 1 + LUT_SIZE) % LUT_SIZE;
        int next = (i + 1) % LUT_SIZE;
        temp_lut[i] = (cogging_lut[prev] + cogging_lut[i] + cogging_lut[next]) / 3.0f;
    }
    
    // 3. 写回 LUT
    for (int i = 0; i < LUT_SIZE; i++) {
        cogging_lut[i] = temp_lut[i];
    }
}

```

---

### 步骤 3：实时 FOC 电流环前馈注入

标定完成后，数组 `cogging_lut` 保存到了 Flash 中。在电机正常运行的 FOC 电流环计算中，**不要只取整数度的值，要做线性插值（Linear Interpolation）**，否则转起来电流会有阶跃突变。

#### 实时前馈算法代码：

```c
/**
 * @brief 根据当前的机械角度，线性插值计算抗齿槽补偿电流
 * @param mech_deg 当前机械角度 (0.0f ~ 360.0f)
 * @return float 需叠加的 Iq 补偿量
 */
float get_anti_cogging_iq_offset(float mech_deg) {
    // 确保角度归一化在 0~360
    while (mech_deg < 0.0f) mech_deg += 360.0f;
    while (mech_deg >= 360.0f) mech_deg -= 360.0f;

    // 计算网格索引
    int index_low = (int)mech_deg; 
    int index_high = (index_low + 1) % LUT_SIZE;

    // 计算插值权重 (小数部分)
    float remainder = mech_deg - (float)index_low;

    // 线性插值计算
    float iq_comp = cogging_lut[index_low] + 
                    remainder * (cogging_lut[index_high] - cogging_lut[index_low]);

    return iq_comp;
}

```

#### 在电流环中的调用位置：

在你的 FOC 任务或 PWM 采样中断里，将抗齿槽电流直接**前馈加到 $I_q$ 给定值**上：

```c
// 1. 速度环 PID 计算出的目标电流
float iq_target_from_speed_pid = speed_pid_calculate();

// 2. 获取当前机械角对应的抗齿槽补偿电流
float iq_cogging_comp = get_anti_cogging_iq_offset(get_vfoc_theta_m_deg());

// 3. 最终注入到 FOC 的 Iq*（叠加前馈）
curent_loop_park.Iq_ref = iq_target_from_speed_pid + iq_cogging_comp;

// 4. 送入电流环 PID 进行控制
vfoc_curent_loop(curent_loop_park.Iq_ref, 0.0f);

```

---

### 避坑指南

1. **绝对零点（Offset）必须先固化：**
齿槽转矩绑定的是**物理机械角度**。如果你重新标定了电角度零偏（Offset），之前录制的 `cogging_lut` 就会跟机械位置错位，算法会从“抗齿槽”变成“加剧齿槽”。所以**必须先做完零偏标定，再做抗齿槽标定**。
2. **标定速度一定要慢：**
正反扫描时速度一定要足够慢（极低速匀速），速度太快会引入电机的反电动势和电感影响，导致录制到的 $I_q$ 不准。
3. **网格数量选择：**
* 2208 电机 $N = 360$（每度一个点）精度已经绰绰有余。
* 占用 ESP32 内存：$360 \times 4 \text{ bytes} \approx 1.4 \text{ KB}$，完全不占用资源。