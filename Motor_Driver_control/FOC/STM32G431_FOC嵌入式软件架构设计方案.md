很好，这一步你问的是**真正进入工业FOC设计的分水岭问题**了。

STM32G431做FOC，其实是“标准答案级别”的MCU（TIM+ADC+COMP+DMA都很适合电机控制）。

我给你一套**工业级、VESC / ST Motor Control SDK 同级别的架构设计**，你照这个搭，基本不会再掉进 ESP32 那种“时间碎片地狱”。

---

# 🧠 一、STM32G431 FOC 的核心设计原则（先记住这个）

> ❗FOC系统不是“代码结构问题”，是“硬件触发时间系统”

STM32的优势就在于：

* TIM 可以触发 ADC
* ADC 可以触发 DMA
* TIM update = 控制周期基准
* 硬件保证同步

---

# 🚀 二、工业级FOC架构（标准答案）

## ⭐整体结构（非常重要）

```text id="g4f9ka"
TIM1 PWM (20kHz)
        ↓ TRGO
     ADC injected
        ↓ DMA
  current sampling ready
        ↓
 TIM1 update interrupt (FOC ISR)
        ↓
  Clarke / Park
  PI current loop
  SVPWM update
```

---

# ⚙️ 三、系统分层设计（必须这样分）

## ✔ 1. 硬件层（HAL/LL）

* TIM1 → PWM（三相）
* ADC1/2 → 注入通道（电流采样）
* DMA → 自动搬运ADC
* COMP → 过流保护（可选）

---

## ✔ 2. 实时控制层（FOC ISR）

👉 唯一实时线程

```c id="k3p8qz"
TIM1_UP_IRQHandler()
{
    FOC_Run();
}
```

---

## ✔ 3. 低频任务层（1kHz）

* speed loop
* angle observer
* telemetry
* CAN/UART

---

# 🧠 四、关键：STM32的“正确时序”

## ⭐工业标准时序（核心）

```text id="t9m1rz"
TIM1 CNT → PWM更新
        ↓
   TRGO触发ADC
        ↓
   ADC采样（中点）
        ↓
   DMA完成
        ↓
TIM1 Update IRQ
        ↓
FOC计算（使用“刚采样的数据”）
```

---

# ⚡ 五、FOC控制函数（标准工业写法）

```c id="f8k2ld"
void FOC_Run(void)
{
    // ===== 1. 读取电流（DMA已完成）=====
    float ia = adc_current.a;
    float ib = adc_current.b;

    // ===== 2. 电角度（必须实时）=====
    float angle = get_electrical_angle();

    float s = sinf(angle);
    float c = cosf(angle);

    // ===== 3. Clarke =====
    float alpha = ia;
    float beta  = (ia + 2*ib) * 0.57735f;

    // ===== 4. Park =====
    float id = alpha * c + beta * s;
    float iq = beta * c - alpha * s;

    // ===== 5. PI电流环 =====
    float iq_ref = iq_target;

    float err = iq_ref - iq;

    vq = PI_q(err);
    vd = 0;

    // ===== 6. 逆变换 =====
    float v_alpha = -vq * s;
    float v_beta  =  vq * c;

    // ===== 7. SVPWM =====
    SVPWM_Update(v_alpha, v_beta);
}
```

---

# ⚡ 六、STM32G431关键外设配置（非常关键）

---

## ✔ 1. TIM1（核心）

配置：

* PWM center aligned
* 20kHz
* TRGO = Update event

👉 用于：

* PWM输出
* ADC触发
* FOC周期基准

---

## ✔ 2. ADC Injected Mode（重点）

必须配置：

```text id="xq9v2m"
TIM1 TRGO → ADC injected trigger
```

采样：

* Ia
* Ib

👉 保证：

> 电流采样发生在PWM中点

---

## ✔ 3. DMA

自动搬运：

```text id="q7k3mz"
ADC → memory (adc_current)
```

👉 FOC ISR不用读ADC寄存器

---

# 🧠 七、为什么STM32比ESP32稳定（关键本质）

---

## ✔ STM32：

| 特性       | 结果            |
| -------- | ------------- |
| TIM触发ADC | 同步采样          |
| DMA搬运    | 无CPU延迟        |
| 单核实时     | 无调度抖动         |
| ISR固定周期  | deterministic |

---

## ❌ ESP32：

| 问题    | 结果     |
| ----- | ------ |
| 多核    | jitter |
| RTOS  | 延迟     |
| ADC异步 | 不同步    |
| cache | 不确定延迟  |

---

# 🚨 八、FOC稳定性的本质（最重要）

STM32能稳定FOC的原因是：

> ❗“时间由硬件定义，而不是软件定义”

---

# 🧭 九、工业级软件结构（推荐你照抄）

```text id="z2m8kp"
main()
 ├── HAL_Init()
 ├── PWM_Init(TIM1)
 ├── ADC_Init(injected + DMA)
 ├── FOC_Init()
 ├── Start TIM1
 └── while(1)
       ├── speed_loop(1kHz)
       ├── telemetry
       └── CAN/UART
```

---

# ⚡ 十、FOC ISR必须“极简”

👉 STM32工业规范：

```c id="p2k8vz"
void TIM1_UP_IRQHandler()
{
    FOC_Run();   // 只做这一件事
}
```

❗禁止：

* printf
* malloc
* 复杂计算
* delay

---

# 🧠 十一、PI设计建议（工业级）

```c id="r7m1qk"
vq = Kp*(err) + Ki * integral;
integral = clamp(integral);
```

---

# 🚀 十二、你现在最关键的升级点

和你 ESP32 最大差别是：

## ❗STM32 = 硬件时间闭环

## ❗ESP32 = 软件时间拼接

---

# 🧭 十三、最重要总结（记住这一句）

> STM32G431做FOC的核心不是“代码写得好”，
> 而是：
>
> ❗PWM → ADC → ISR 三者在同一个硬件时间链上闭环

---

# 🚀 如果你下一步要做更高级的（我可以帮你）

我可以继续帮你升级到：

## 👉 STM32G431工业FOC完整工程架构

包括：

* 电流环 + 速度环 + 位置环
* PLL无感估算
* 弱磁控制
* SVPWM优化
* 电流采样校准
* 启动策略（align + open loop + closed loop）

只要你说一句：

> “我要完整工业FOC工程”

我可以直接帮你搭一个接近 VESC / ST SDK 的结构。


----------
----------
----------


好，那我直接给你一套 **STM32G431 工业级FOC完整工程架构（可以直接按模块搭建）**，目标是：

> ✅ 电流环稳定
> ✅ 可扩展速度/位置环
> ✅ 对标 ST Motor Control SDK / VESC 架构
> ✅ 解决你现在 ESP32 的“时间碎片问题”

---

# 🚀 一、整体工程架构（工业标准）

```text id="g4k9zp"
            +----------------------+
            |   main.c (init)      |
            +----------+-----------+
                       |
        +--------------+--------------+
        |                             |
+-------v--------+         +----------v---------+
|  低频任务(1kHz) |         |   控制中断(20kHz)  |
| speed / pos     |         | TIM1_UP_IRQHandler |
+-----------------+         +----------+---------+
                                      |
                         +------------v------------+
                         |     FOC_CONTROL()       |
                         +------------+------------+
                                      |
          +---------------------------+---------------------------+
          |                           |                           |
   Clarke/Park               PI current loop              SVPWM
          |                           |                           |
          +---------------------------+---------------------------+
                                      |
                                PWM update (TIM1)
```

---

# ⚙️ 二、STM32G431 外设分工（关键）

## ✔ TIM1（核心心脏）

* 20kHz PWM
* Center-aligned mode
* TRGO = Update event

👉 控制周期来源

---

## ✔ ADC1（注入模式）

* Ia / Ib
* TRGO触发采样（来自TIM1）

👉 电流“硬同步”

---

## ✔ DMA

* 自动搬运ADC结果

👉 CPU不参与采样搬运

---

## ✔ TIM2 / TIM3（可选）

* 1kHz速度环
* 低频任务

---

# 🧠 三、最关键设计（工业FOC核心）

# ⭐ 关键原则：三件事必须绑定

```text id="xq9kz1"
PWM更新点 = ADC采样点 = FOC计算点
```

---

# 🚨 四、控制中断（20kHz核心）

```c id="foc_isr"
void TIM1_UP_IRQHandler(void)
{
    if (TIM1->SR & TIM_SR_UIF)
    {
        TIM1->SR &= ~TIM_SR_UIF;

        FOC_Control_Loop();
    }
}
```

---

# ⚡ 五、工业级FOC控制函数（完整核心）

```c id="foc_loop"
void FOC_Control_Loop(void)
{
    // =========================
    // 1. 读取电流（DMA已更新）
    // =========================
    float ia = adc_current.ia;
    float ib = adc_current.ib;

    // =========================
    // 2. 电角度（必须实时）
    // =========================
    float theta = Get_Electrical_Angle();

    float sin_t = sinf(theta);
    float cos_t = cosf(theta);

    // =========================
    // 3. Clarke
    // =========================
    float alpha = ia;
    float beta  = (ia + 2.0f * ib) * 0.57735027f;

    // =========================
    // 4. Park
    // =========================
    float id = alpha * cos_t + beta * sin_t;
    float iq = beta * cos_t  - alpha * sin_t;

    // =========================
    // 5. 电流环（工业PI）
    // =========================
    float iq_ref = iq_target;
    float id_ref = 0;

    float err_q = iq_ref - iq;
    float err_d = id_ref - id;

    vq = PI_Q_Update(err_q);
    vd = PI_D_Update(err_d);

    // =========================
    // 6. 解耦（可选但推荐）
    // =========================
    vq += omega_e * Ld * id + omega_e * psi_f;
    vd -= omega_e * Lq * iq;

    // =========================
    // 7. 逆Park
    // =========================
    float alpha_v = vd * cos_t - vq * sin_t;
    float beta_v  = vd * sin_t + vq * cos_t;

    // =========================
    // 8. SVPWM
    // =========================
    SVPWM_Update(alpha_v, beta_v);
}
```

---

# ⚡ 六、PI控制器（工业级必备）

```c id="pi"
typedef struct {
    float kp;
    float ki;
    float integral;
    float limit;
} PI_Controller;

float PI_Update(PI_Controller *pi, float err)
{
    pi->integral += pi->ki * err;

    // anti-windup
    if (pi->integral > pi->limit) pi->integral = pi->limit;
    if (pi->integral < -pi->limit) pi->integral = -pi->limit;

    return pi->kp * err + pi->integral;
}
```

---

# ⚙️ 七、PWM + ADC同步配置（STM32关键）

## ✔ TIM1配置

* PWM mode: center aligned
* ARR = 20kHz
* TRGO = Update event

---

## ✔ ADC配置（重点）

```text id="adc"
Injected channel trigger = TIM1 TRGO
```

👉 每次PWM中点自动采样电流

---

# 🧠 八、速度环（后续扩展）

```c id="speed"
void Speed_Loop_1kHz()
{
    float err = speed_ref - speed_meas;
    iq_target = PI_speed(err);
}
```

---

# 🚨 九、工程稳定性关键点（非常重要）

---

## ❗1. 电流采样必须在PWM中点

否则：

> iq = 假的

---

## ❗2. θ必须“实时或PLL更新”

不能：

* 1kHz更新 ❌

---

## ❗3. ISR必须极简

```text id="bad"
NO printf
NO malloc
NO RTOS delay
```

---

## ❗4. PI必须限幅

否则积分炸

---

# 🧠 十、为什么STM32比ESP32稳定（本质）

| 项目     | STM32G431 | ESP32       |
| ------ | --------- | ----------- |
| PWM    | 硬件定时      | 软件          |
| ADC触发  | TIM同步     | 异步          |
| DMA    | 强同步       | 不确定         |
| 控制延迟   | 固定50us    | 抖动100~300us |
| RTOS干扰 | 无         | 有           |

---

# 💥 十一、你现在的问题本质总结

你之前失败原因：

> ❌ “把一个硬实时控制系统拆成了多个异步系统”

STM32方案解决的是：

> ✔ 把所有变量锁进同一个硬件时间轴

---

# 🚀 十二、下一步我可以帮你升级

如果你这个架构你准备做，我可以继续帮你升级到：

## 👉 STM32G431工业FOC完整版（下一阶段）

包括：

* 无感FOC（SMO / PLL）
* 启动策略（open loop → closed loop）
* 弱磁控制
* 电流采样校准
* SVPWM优化（降低噪声）
* 电机参数在线辨识

甚至可以做到：

> 🚗 接近 VESC / ODrive 级别控制性能

---

如果你下一步想做：

👉 “无感FOC版本（不用编码器）”

或者

👉 “带速度环+位置环完整系统”

直接说，我可以继续帮你把整套电机系统搭到工业级。



----------
----------
----------

换到 **STM32G431** 是一个非常明智的决定！如果说 ESP32 是为物联网而生的，那么 STM32G4x1 系列简直就是**为电机 FOC 控制量身定制的“神兵利器”**。它自带的高级定时器（TIM1/TIM8）、超高速 ADC、硬件数学加速器（CORDIC）以及运放（OPAMP），能完美解决你之前遇到的所有“时序撕裂”问题。

在 STM32G431 上做 FOC，标准的工业级嵌入式软件架构可以概括为：**“硬件连轴转，软件分层跑”**。

这套架构的核心思想是：**坚决不用软件定时器去触发电流环，而是用 PWM 硬件本身去触发 ADC 采样，ADC 采完再硬件触发中断算 FOC。** 这叫做“事件驱动的绝对同步”。

以下是为你梳理的 STM32G431 FOC 标准软件架构设计：

---

### 第一层：底层硬件联动层 (The Hardware Heartbeat)

这是 FOC 的基石，全部通过配置 STM32 的寄存器（或 CubeMX）由硬件自动完成，不占 CPU 时间。

1. **PWM 生成 (TIM1)：** 配置为**中心对齐模式 (Center-aligned)**。中心对齐的好处是，在一个 PWM 周期内，所有下桥臂同时打开的瞬间（V0 矢量）或所有上桥臂同时打开的瞬间（V7 矢量），开关噪声最小。
2. **硬件触发 ADC (Hardware Trigger)：** 配置 TIM1 在计数器到达“顶部”或“底部”时，产生一个 TRGO (Trigger Output) 信号，**直接通过硬件线路触发 ADC 开始转换**。
3. **注入通道 (Injected ADC)：** 将读取相电流（$I_a, I_b$）的 ADC 通道配置为“注入通道”。注入通道的优先级最高，一旦被 TIM1 触发，立刻打断其他转换去测电流。

**效果：** 硬件保证了你每次采到的电流，绝对是 PWM 发波最干净、最能反映当前周期的真实电流。

---

### 第二层：极速实时控制层 (20kHz FOC ISR)

这里是你之前“发疯死循环”的终结者。这一层**必须是一个裸机硬件中断 (NVIC)**，绝对不能放在 RTOS 的任务里（RTOS 的上下文切换会带来不可控的微秒级延迟）。

当 ADC 采集完 $I_a, I_b$ 后，会产生一个 **ADC 注入转换完成中断 (JEOC ISR)**。在这个中断里，你必须雷厉风行地完成以下步骤：

```c
// ADC JEOC 中断服务函数 (运行频率 20kHz，每 50us 触发一次)
void ADC1_2_IRQHandler(void) {
    // 1. 【极速获取角度】
    // 强烈建议使用 TIM 的编码器模式读取 ABZ，或者用纯硬件 SPI 读磁编码器
    // 此时读到的机械角度，与刚才 ADC 采到的电流在时间上是绝对对齐的！
    theta_mech = Encoder_Read(); 
    theta_elec = theta_mech * motor_PP;

    // 2. 读取相电流
    I_a = ADC1->JDR1; // 直接读寄存器
    I_b = ADC2->JDR1;

    // 3. FOC 数学运算 (使用 G431 自带的 CORDIC 协处理器)
    // 利用硬件 CORDIC 可以在 10 个时钟周期内算完 sin 和 cos，碾压标准库！
    cordic_sincos(theta_elec, &sin_theta, &cos_theta);
    
    Clark_Park_Transform(...);
    PI_Current_Loop(...); // 运行 Iq, Id 的 PID
    InvPark_SVPWM(...);

    // 4. 更新 PWM 占空比 (写入影子寄存器)
    TIM1->CCR1 = pwm_a;
    TIM1->CCR2 = pwm_b;
    TIM1->CCR3 = pwm_c;

    // 5. 降频分发 (处理速度环)
    speed_loop_counter++;
    if(speed_loop_counter >= 20) { // 20kHz 降频到 1kHz
        speed_loop_counter = 0;
        // 触发低频软中断，或通过 RTOS 信号量唤醒速度环任务
        Trigger_Speed_Loop(); 
    }
    
    // 清除中断标志位
}

```

**关键约束：** 这个中断里的所有代码执行时间加起来不能超过 10~15 微秒，把剩下的 35 微秒留给 CPU 去干别的事。

---

### 第三层：应用与运动控制层 (1kHz RTOS Task / Main Loop)

这一层可以放在 FreeRTOS 的 Task 里，或者 `main` 函数的 `while(1)` 里。这里对时间的容忍度较高。

1. **速度环 / 位置环 (1kHz)：** 接收从底层 ISR 传上来的降频信号。计算速度，运行位置 PID 和速度 PID，输出目标电流 `Iq_ref` 给第二层的电流环。
2. **轨迹规划：** 比如 T型加减速、S型曲线规划。
3. **安全与保护：** 监控过压、欠压、过温（通过普通 ADC 轮询采样）。
4. **通信接口：** 处理 CAN、UART，与上位机交互。

---

### 💡 针对你现状的特别提醒 (避坑指南)

1. **编码器的生死抉择：** 如果你换了 STM32G431，**千万不要再用 I2C 的 AS5600 去跑 20kHz 的闭环了！** I2C 通信太慢，哪怕你把 I2C 时钟拉到 1MHz，读一次角度也需要几十微秒，会直接把你的 JEOC 中断卡死。
* **最佳搭档：** 换支持 **SPI** 的芯片（如 AS5047P、MT6816），或者支持 **ABZ 正交**的芯片。STM32 的定时器有专属的 Encoder Mode，不需要 CPU 干预，随时去寄存器里读，读出来的角度永远是零延迟的最新值。


2. **利用 CORDIC：** G431 的最大卖点就是 CORDIC 协处理器。标准 C 库的 `sinf()` 和 `cosf()` 会消耗几百个 CPU 周期，而 CORDIC 只需要配置几个寄存器，瞬间出结果，这是你 20kHz 顺畅运行的保障。
3. **死区时间 (Dead-Time)：** ESP32 可能需要你软件或外部硬件做死区，但 STM32 的 TIM1 自带硬件死区发生器 (Dead-Time Generator)。务必在 CubeMX 里配置好死区时间，防止上下桥臂直通炸管。

**总结：** STM32G431 的灵魂在于“硬件联动”。通过 `TIM1 -> 硬件触发 -> 注入 ADC -> 硬件中断 -> 极速 SPI/ABZ -> 硬件 CORDIC -> 写入 CCR 寄存器` 这一条黄金链路，你就能构建出一个丝般顺滑、转速上万也不会发散的工业级 FOC 底层架构。



----------
----------
----------

# STM32G431 FOC 嵌入式软件架构设计方案
结合你之前踩过的「异步失步、跨核抖动、积分饱和」等坑，针对 STM32G431 的硬件特性（高级定时器、硬件ADC同步、编码器接口、CORDIC/FPU加速），**工业界最成熟、最稳定的方案是：硬件级同步触发 + 中断式电流内环 + 分层状态机架构**。
核心原则：**能硬件做的绝不软件做，能中断做的绝不任务做，内环绝对优先、外环绝对不干扰内环**，从根源上杜绝时序不同步的问题。

---

## 一、整体设计核心原则（先定调，避免走弯路）
1. **同步优先**：电流采样、角度读取、FOC计算三者必须硬件级同频对齐，延迟固定且小于1μs级，杜绝随机抖动。
2. **环带分离**：电流环（内环）> 速度环（中环）> 位置环（外环），频率依次降低，绝对不能反向干扰。
3. **软硬分层**：硬件驱动与算法完全分离，算法层无硬件依赖，便于调试移植。
4. **安全兜底**：硬件级故障保护优先于软件，异常时微秒级关断PWM。
5. **循序渐进**：先跑通单Iq环无耦合，再加D环，最后加解耦前馈，避免一步炸机。

STM32G431 主频170MHz、带单精度FPU、CORDIC三角函数加速器、TIM1高级定时器+硬件ADC注入触发，**20kHz电流环纯浮点数运算仅需3~5μs**，性能完全充裕，无需刻意用定点数。

---

## 二、标准三层控制环架构（频率与执行载体）
这是工业级FOC的通用规范，严格遵守就不会出现架构性错误。

| 控制环 | 执行频率 | 执行载体 | 核心功能 | 优先级 |
| :--- | :--- | :--- | :--- | :--- |
| 电流环（内环） | 20kHz（与PWM同频） | ADC转换完成中断 | Clark/Park、PI、解耦、SVPWM、占空比更新 | 最高（仅低于故障刹车） |
| 速度环（中环） | 1kHz | 基本定时器中断 / 高优先级任务 | 速度PI、输出Iq给定 | 次高 |
| 位置环（外环） | 100~200Hz | 主循环 / 普通任务 | 位置PI、输出速度给定 | 最低 |

### 关键铁则
- 电流环**绝对不能放到RTOS任务里**，必须在中断中执行，杜绝任务调度带来的随机抖动。
- 外环输出只能修改内环的给定值，绝对不能直接操作PWM寄存器。
- 内环中断里禁止打印、延时、串口发送、内存分配等慢操作，纯计算+寄存器操作。

---

## 三、核心时序：硬件级同步采样方案（最核心，解决你之前的所有同步问题）
STM32G4 的最大优势就是**高级定时器可以硬件触发ADC注入转换**，完全不需要软件干预，实现「PWM中点采样电流 → 转换完成自动进中断算FOC」的零抖动同步。

### 完整时序链路（单周期闭环）
```
TIM1 中心对齐PWM计数 → 计数到中点（下溢/上溢前） → 硬件触发ADC1注入通道转换
    ↓
ADC 自动采样Ia、Ib两相电流 → 硬件转换完成 → 触发ADC注入中断
    ↓
ADC中断内：
  1. 读取ADC电流值（刚采样完成的新鲜数据）
  2. 立刻读取编码器角度（硬件编码器寄存器/SPI，几ns~几us）
  3. 计算电角度 → Clark → Park → PI → 解耦 → 反Park → SVPWM
  4. 计算结果写入TIM1 CCR寄存器，下一个PWM周期自动生效
```

### 硬件配置要点
1. **PWM生成：TIM1 高级定时器**
   - 中心对齐模式，20kHz频率，互补三路输出，硬件死区（死区时间1~2μs，根据MOS管参数调整）。
   - 开启更新事件触发ADC（或设置比较匹配在周期中点），保证电流采样在PWM中点，避开开关尖峰噪声。
   - 开启刹车功能（BKIN引脚），硬件级过流/过压保护，故障时微秒级关断PWM。

2. **电流采样：ADC1 注入通道**
   - 两路注入通道对应两相下桥臂采样电阻（双电阻方案），硬件触发，无需软件启动。
   - 可开启G4的ADC硬件过采样，提升精度，减少软件滤波压力。
   - 转换完成后自动产生中断，中断入口就是电流环计算起点。

3. **编码器读取：硬件编码器接口（优先）**
   - 若用ABI增量编码器：直接用 TIM2/TIM3 硬件编码器模式，硬件自动计数，读角度就是读CNT寄存器，耗时几ns，零软件开销，同步性拉满。
   - 若用AS5600等绝对式编码器：用SPI1（DMA模式），速度比I2C快10倍，读一次仅需几μs；在ADC中断内触发SPI读取，保证和电流采样时间差最小。

### 为什么这个架构不会出现你之前的问题？
- 电流采样和FOC计算是**因果绑定**的：采完立刻算，时间差固定且极小，没有随机调度延迟。
- 角度读取和电流采样在同一个中断里紧挨着执行，时间差可控（<5μs），相当于固定相位偏移，可被零点校准抵消，不会随转速累积发散。
- 20kHz电流环对应20kHz角度更新，完全同频，不存在“20个电流周期才更一次角度”的频率错配。

---

## 四、软件分层架构（代码组织方式）
严格分层，避免硬件和算法耦合，调试、移植、改需求都不容易出问题。

### 第1层：BSP硬件驱动层（board support package）
**职责**：所有和STM32硬件寄存器相关的操作，封装成统一接口，算法层绝不直接碰寄存器。
- `bsp_pwm.c/h`：TIM1初始化、死区配置、占空比设置、PWM使能/失能、刹车控制。
- `bsp_adc.c/h`：ADC初始化、注入通道配置、电流读取、零点校准。
- `bsp_encoder.c/h`：编码器初始化、机械角度读取、转速计算、零点校准。
- `bsp_uart.c/h`：串口通信、上位机数据交互。
- `bsp_gpio.c/h`：使能引脚、LED、故障输入输出。

### 第2层：算法算法层（纯计算，无硬件依赖）
**职责**：所有FOC相关数学运算，输入输出都是浮点数，可单独单元测试，和硬件完全解耦。
- `foc_math.c/h`：Clark变换、Park变换、反Park、SVPWM、角度归一化。
  - 优先用G4的CORDIC硬件计算sin/cos、sqrt，比软件快3~5倍。
- `pi_controller.c/h`：PI控制器，带积分限幅、抗积分饱和、输出限幅。
- `lowpass_filter.c/h`：一阶低通滤波，用于电流、转速平滑。
- `encoder_process.c/h`：角度换算、电角度计算、转速差分计算、角度插值（可选）。

### 第3层：应用状态机层（业务逻辑）
**职责**：电机状态管理、外环控制、给定处理、故障保护，所有慢逻辑都放这一层。
#### 核心：电机状态机（必做，避免状态混乱）
定义枚举状态，严格按条件切换，杜绝非法跳转：
```c
typedef enum {
    MOTOR_STOP,       // 停止：PWM关闭，积分清零
    MOTOR_ALIGN,      // 校准：通直流锁转子，记录编码器零点
    MOTOR_OPEN_LOOP,  // 开环：V/f控制，低速拉转
    MOTOR_CLOSED_LOOP,// 闭环：电流环+速度/位置环
    MOTOR_FAULT       // 故障：PWM关断，等待复位
} motor_state_t;
```
- 所有状态切换都在主循环（或低优先级任务）中处理，不干扰电流环中断。
- 故障状态优先级最高，任何状态触发过流、过压、堵转都直接切FAULT。

#### 外环控制
- 速度环、位置环的PI计算放在1kHz定时器中断或高优先级任务中，输出结果仅修改电流环的`iq_ref`、`id_ref`。
- 给定值（目标转速、目标位置）通过串口、按键、CAN等输入，在主循环中更新。

---

## 五、两种落地方案（根据项目复杂度选）
### 方案A：极简前后台方案（推荐新手/追求稳定，复刻DengFOC思路但硬件升级）
#### 特点
- 无RTOS，纯前后台架构，零任务调度抖动，稳定性最高。
- 代码量小，调试简单，完全避开操作系统带来的不确定性。
#### 执行框架
```c
// 前台：中断级（强实时）
void ADC1_2_IRQHandler(void) // 20kHz 电流环
{
    // 1. 读电流
    ia = bsp_adc_get_ia();
    ib = bsp_adc_get_ib();
    // 2. 读角度，算电角度
    theta_mech = bsp_enc_get_angle();
    theta_e = pole_pairs * theta_mech - zero_angle;
    // 3. FOC全流程计算
    foc_calculate(ia, ib, theta_e, &duty_a, &duty_b, &duty_c);
    // 4. 更新PWM占空比
    bsp_pwm_set_duty(duty_a, duty_b, duty_c);
}

void TIM6_DAC_IRQHandler(void) // 1kHz 速度环
{
    speed_fdb = bsp_enc_get_speed();
    iq_ref = speed_pi_run(speed_ref - speed_fdb);
}

// 后台：主循环（慢逻辑）
int main(void)
{
    // 硬件初始化
    bsp_init_all();
    // 状态机主循环
    while(1)
    {
        motor_state_machine();  // 状态切换
        uart_process();         // 串口通信、上位机数据
        fault_detect();         // 故障检测
    }
}
```
#### 适用场景
- 单电机、功能简单、对稳定性要求高。
- 刚入门FOC，想先跑通闭环，避免操作系统干扰。

### 方案B：FreeRTOS 分层方案（适合复杂项目，多任务多外设）
#### 特点
- 保留电流环中断的绝对优先级，外环和业务逻辑用任务实现。
- 适合带屏幕、按键、CAN、多电机的复杂项目。
#### 任务与中断划分
1. **最高优先级：ADC注入中断（电流环）** → 纯计算，不调用任何OS接口。
2. **高优先级任务：速度环任务** → 由1kHz定时器中断发任务通知唤醒，执行速度PI，更新电流给定。
3. **中优先级任务：应用任务** → 状态机、位置环、按键处理、逻辑控制。
4. **低优先级任务：通信任务** → 串口/上位机数据收发、日志打印。

#### 关键注意
- 绝对禁止把电流环放到任务里，哪怕是最高优先级任务也不行，任务调度天生有微秒级抖动。
- 跨任务共享变量（如`iq_ref`、`speed_ref`）用volatile修饰，或用队列传递，避免数据竞争。

---

## 六、电流环中断内的标准执行流程（一步都不能乱）
严格按这个顺序写，保证时序和逻辑正确，和你之前的混乱架构彻底区分开。
```c
void foc_calculate(float ia, float ib, float theta_e)
{
    // 1. Clark 变换：Ia Ib → Iα Iβ
    clark_transform(ia, ib, &i_alpha, &i_beta);

    // 2. Park 变换：Iα Iβ → Id Iq
    park_transform(i_alpha, i_beta, theta_e, &id_fdb, &iq_fdb);

    // 3. 电流低通滤波（可选，G4 ADC精度高可弱化）
    id_fdb = lpf_run(&id_lpf, id_fdb);
    iq_fdb = lpf_run(&iq_lpf, iq_fdb);

    // 4. PI 控制（先只开Iq环，Id=0，稳定后再加D环）
    vd_pi = pi_run(&pi_d, id_ref - id_fdb);
    vq_pi = pi_run(&pi_q, iq_ref - iq_fdb);

    // 5. 交叉解耦前馈（稳定后再加，初期注释掉）
    // vd_comp = -omega_e * Lq * iq_fdb;
    // vq_comp =  omega_e * Ld * id_fdb + omega_e * psi_m;
    // vd = vd_pi + vd_comp;
    // vq = vq_pi + vq_comp;
    vd = vd_pi;
    vq = vq_pi;

    // 6. 圆形电压限幅（绝对不用d轴优先）
    circle_voltage_limit(&vd, &vq, v_max);

    // 7. 反Park变换：Vd Vq → Vα Vβ
    inv_park_transform(vd, vq, theta_e, &v_alpha, &v_beta);

    // 8. SVPWM 计算三相占空比
    svpwm_calculate(v_alpha, v_beta, &duty_a, &duty_b, &duty_c);
}
```

---

## 七、必做的保护与兜底设计（避免烧板+失控）
结合你之前“转速无限飙升”的教训，这些保护必须提前做好。
1. **PI积分硬限幅**：每个PI控制器的积分项单独限幅，和输出限幅一致，杜绝积分饱和。
2. **硬件刹车保护**：过流比较器输出接TIM1的BKIN引脚，硬件直接关PWM，比软件保护快100倍。
3. **软件过流/过压检测**：1kHz周期检测母线电压、相电流峰值，超阈值切故障状态。
4. **转速上限保护**：速度超过设定最大值时，强制降电流给定，避免飞车。
5. **通信丢失保护**：上位机给定超时无更新，自动切停止状态。

---

## 八、调试步骤（循序渐进，避免一步炸机）
1. **硬件验证阶段**：不开PWM，验证电流采样零点、编码器角度读数连续正确。
2. **开环验证阶段**：固定Ud=0、Uq=小电压，电角度缓慢递增，电机平稳低速旋转，验证相序、PWM输出正确。
3. **单环闭环阶段**：只开Iq环，Id=0，无解耦，小电流堵转测试，验证电流能稳定跟踪给定。
4. **带载旋转测试**：松开电机轴，验证旋转时电流不发散、转速不飞车。
5. **逐步升级阶段**：加D轴闭环 → 加交叉解耦前馈 → 加速度环 → 加位置环，每一步都验证稳定再往下走。

---

## 总结
STM32G431 做FOC的最大优势就是**外设高度联动，能实现纯硬件级的采样同步**，这是ESP32纯软件架构比不了的。只要你严格遵守「电流环放中断、硬件触发采样、分层解耦、循序渐进调试」这几个原则，完全不会出现之前ESP32上的异步失步、飞车发散问题。

需要我帮你整理一份STM32G431的FOC工程最小代码框架（包含PI、SVPWM、寄存器配置要点）吗？




----------
----------
----------

下面给你一套STM32G431的FOC最小工程代码框架，基于HAL库，严格遵循「硬件同步采样+中断级电流环+分层架构」，默认先跑**单Iq闭环（Id=0、无解耦）**，保证先稳定跑起来，后续再逐步加功能。代码做了最小化裁剪，核心逻辑完整，可直接往工程里移植。

---

## 一、工程整体文件结构
```
FOC_G431/
├── foc_config.h          // 全局配置：电机参数、硬件参数
├── algorithm/
│   ├── pi_controller.h/c // PI控制器（带积分限幅、抗饱和）
│   └── foc_math.h/c      // FOC核心数学运算
├── bsp/
│   ├── bsp_pwm.h/c       // TIM1 互补PWM + 死区 + 硬件触发ADC
│   ├── bsp_adc.h/c       // ADC注入通道 硬件同步电流采样
│   └── bsp_encoder.h/c   // 编码器角度/转速读取
└── main.c                // 主函数 + 电机状态机 + 外环控制
```

---

## 二、全局配置文件 `foc_config.h`
所有参数集中管理，避免散落在代码里。
```c
#ifndef FOC_CONFIG_H
#define FOC_CONFIG_H

#include "stm32g4xx_hal.h"
#include <math.h>

// ========== 电机参数 ==========
#define POLE_PAIRS        7     // 极对数
#define BUS_VOLTAGE       12.6f // 母线电压 V
#define MAX_CURRENT       5.0f  // 最大电流 A

// ========== 硬件参数 ==========
#define PWM_FREQ          20000 // PWM频率 20kHz
#define PWM_PERIOD_US     50    // 周期 50us
#define CURRENT_KI        20.0f // 电流环I参数
#define CURRENT_KP        1.2f  // 电流环P参数
#define VOLTAGE_MAX       (BUS_VOLTAGE * 0.95f) // 最大可用电压（留余量）

// ========== 数学常量 ==========
#define _2PI              6.28318530718f
#define _PI_2             1.57079632679f
#define _3PI_2            4.71238898038f
#define _1_SQRT3          0.57735026919f
#define _2_SQRT3          1.15470053838f

// 角度归一化到 [0, 2π)
static inline float normalize_angle(float angle)
{
    float a = fmodf(angle, _2PI);
    return a >= 0 ? a : (a + _2PI);
}

#endif
```

---

## 三、算法层：PI控制器 `pi_controller.h/c`
带积分硬限幅，从根源杜绝积分饱和飞车。
### pi_controller.h
```c
#ifndef PI_CONTROLLER_H
#define PI_CONTROLLER_H

#include "foc_config.h"

typedef struct {
    float kp;
    float ki;
    float limit;       // 输出&积分限幅值
    float integral;    // 积分项
    float out_prev;
} pi_t;

void pi_init(pi_t *pi, float kp, float ki, float limit);
float pi_run(pi_t *pi, float error, float dt);

#endif
```

### pi_controller.c
```c
#include "pi_controller.h"

void pi_init(pi_t *pi, float kp, float ki, float limit)
{
    pi->kp = kp;
    pi->ki = ki;
    pi->limit = limit;
    pi->integral = 0.0f;
    pi->out_prev = 0.0f;
}

float pi_run(pi_t *pi, float error, float dt)
{
    // 比例项
    float p_out = pi->kp * error;
    
    // 积分项 + 硬限幅（核心：积分不会无限堆积）
    pi->integral += error * pi->ki * dt;
    if(pi->integral >  pi->limit) pi->integral =  pi->limit;
    if(pi->integral < -pi->limit) pi->integral = -pi->limit;
    
    // 输出合成 + 输出限幅
    float out = p_out + pi->integral;
    if(out >  pi->limit) out =  pi->limit;
    if(out < -pi->limit) out = -pi->limit;
    
    pi->out_prev = out;
    return out;
}
```

---

## 四、算法层：FOC核心数学运算 `foc_math.h/c`
包含Clark、Park、反Park、SVPWM，初期只保留Iq相关计算，最简优先。
### foc_math.h
```c
#ifndef FOC_MATH_H
#define FOC_MATH_H

#include "foc_config.h"

// Clark变换：两相电流 → αβ 坐标
void clark_transform(float ia, float ib, float *i_alpha, float *i_beta);

// Park变换：αβ → dq 坐标
void park_transform(float i_alpha, float i_beta, float theta_e, float *id, float *iq);

// 反Park变换：dq → αβ 坐标
void inv_park_transform(float vd, float vq, float theta_e, float *v_alpha, float *v_beta);

// SVPWM计算：输入αβ电压，输出三相占空比 [0,1]
void svpwm_calc(float v_alpha, float v_beta, float *ta, float *tb, float *tc);

// 圆形电压限幅
void voltage_circle_limit(float *vd, float *vq, float v_max);

#endif
```

### foc_math.c
```c
#include "foc_math.h"

void clark_transform(float ia, float ib, float *i_alpha, float *i_beta)
{
    *i_alpha = ia;
    *i_beta  = _1_SQRT3 * ia + _2_SQRT3 * ib;
}

void park_transform(float i_alpha, float i_beta, float theta_e, float *id, float *iq)
{
    float ct = cosf(theta_e);
    float st = sinf(theta_e);
    *id = i_alpha * ct + i_beta * st;
    *iq = i_beta * ct - i_alpha * st;
}

void inv_park_transform(float vd, float vq, float theta_e, float *v_alpha, float *v_beta)
{
    float ct = cosf(theta_e);
    float st = sinf(theta_e);
    *v_alpha = vd * ct - vq * st;
    *v_beta  = vd * st + vq * ct;
}

void svpwm_calc(float v_alpha, float v_beta, float *ta, float *tb, float *tc)
{
    float u1, u2, u3;
    float t1, t2, t0;
    uint8_t sector = 0;

    // 扇区判断
    if(v_beta > 0) sector += 1;
    if((_1_SQRT3 * v_alpha - v_beta) > 0) sector += 2;
    if((-_1_SQRT3 * v_alpha - v_beta) > 0) sector += 4;

    float k = 1.0f / BUS_VOLTAGE; // 电压→占空比系数

    switch(sector)
    {
        case 3: // 扇区1
            t1 = k * (v_alpha - _1_SQRT3 * v_beta);
            t2 = k * (_2_SQRT3 * v_beta);
            t0 = 0.5f * (1.0f - t1 - t2);
            *ta = t0 + t1 + t2;
            *tb = t0 + t2;
            *tc = t0;
            break;
        case 1: // 扇区2
            t1 = k * (v_alpha + _1_SQRT3 * v_beta);
            t2 = k * (-v_alpha + _1_SQRT3 * v_beta);
            t0 = 0.5f * (1.0f - t1 - t2);
            *ta = t0 + t1;
            *tb = t0 + t1 + t2;
            *tc = t0;
            break;
        case 5: // 扇区3
            t1 = k * (_2_SQRT3 * v_beta);
            t2 = k * (-v_alpha - _1_SQRT3 * v_beta);
            t0 = 0.5f * (1.0f - t1 - t2);
            *ta = t0;
            *tb = t0 + t1 + t2;
            *tc = t0 + t2;
            break;
        case 4: // 扇区4
            t1 = k * (-v_alpha + _1_SQRT3 * v_beta);
            t2 = k * (-_2_SQRT3 * v_beta);
            t0 = 0.5f * (1.0f - t1 - t2);
            *ta = t0;
            *tb = t0 + t1;
            *tc = t0 + t1 + t2;
            break;
        case 6: // 扇区5
            t1 = k * (-v_alpha - _1_SQRT3 * v_beta);
            t2 = k * (v_alpha - _1_SQRT3 * v_beta);
            t0 = 0.5f * (1.0f - t1 - t2);
            *ta = t0 + t2;
            *tb = t0;
            *tc = t0 + t1 + t2;
            break;
        case 2: // 扇区6
            t1 = k * (_2_SQRT3 * v_beta);
            t2 = k * (v_alpha + _1_SQRT3 * v_beta);
            t0 = 0.5f * (1.0f - t1 - t2);
            *ta = t0 + t1 + t2;
            *tb = t0;
            *tc = t0 + t1;
            break;
        default:
            *ta = 0.5f; *tb = 0.5f; *tc = 0.5f;
            break;
    }
}

void voltage_circle_limit(float *vd, float *vq, float v_max)
{
    float v_mag = sqrtf((*vd)*(*vd) + (*vq)*(*vq));
    if(v_mag > v_max)
    {
        float ratio = v_max / v_mag;
        *vd *= ratio;
        *vq *= ratio;
    }
}
```

> 优化提示：STM32G431自带CORDIC硬件加速器，可替换`cosf/sinf/sqrtf`，运算速度提升3~5倍，跑通后可逐步替换。

---

## 五、硬件驱动层
### 1. PWM驱动 `bsp_pwm.h/c`
使用TIM1高级定时器，中心对齐模式，互补输出+硬件死区，硬件触发ADC采样。
#### bsp_pwm.h
```c
#ifndef BSP_PWM_H
#define BSP_PWM_H

#include "foc_config.h"

extern TIM_HandleTypeDef htim1;
extern uint32_t tim1_arr; // 定时器自动重装载值

void bsp_pwm_init(void);
void bsp_pwm_set_duty(float da, float db, float dc); // 输入占空比 0~1
void bsp_pwm_enable(void);
void bsp_pwm_disable(void);

#endif
```

#### bsp_pwm.c 核心要点
```c
#include "bsp_pwm.h"

TIM_HandleTypeDef htim1;
uint32_t tim1_arr;

void bsp_pwm_init(void)
{
    // 1. 计算ARR值：170MHz主频，中心对齐 = 上下计数，频率=主频/(2*ARR)
    tim1_arr = (SystemCoreClock / 2) / PWM_FREQ; // 170M/2/20k = 4250

    // 2. TIM1基础配置：中心对齐模式1，向上向下计数
    htim1.Instance = TIM1;
    htim1.Init.Prescaler = 0;
    htim1.Init.CounterMode = TIM_COUNTERMODE_CENTERALIGNED1;
    htim1.Init.Period = tim1_arr - 1;
    htim1.Init.RepetitionCounter = 0;
    HAL_TIM_PWM_Init(&htim1);

    // 3. 通道配置：PWM模式1，三路互补输出
    TIM_OC_InitTypeDef sConfigOC = {0};
    sConfigOC.OCMode = TIM_OCMODE_PWM1;
    sConfigOC.Pulse = tim1_arr / 2; // 初始50%占空比
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
    sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
    sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
    HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_1);
    HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_2);
    HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_3);

    // 4. 死区配置（约1.5us，根据MOS管参数调整）
    TIM_BreakDeadTimeConfigTypeDef sBreakDeadTime = {0};
    sBreakDeadTime.OffStateRunMode = TIM_OSSR_DISABLE;
    sBreakDeadTime.OffStateIDLEMode = TIM_OSSI_DISABLE;
    sBreakDeadTime.LockLevel = TIM_LOCKLEVEL_OFF;
    sBreakDeadTime.DeadTime = 0x50; // 死区时间值
    sBreakDeadTime.BreakState = TIM_BREAK_DISABLE;
    sBreakDeadTime.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
    HAL_TIMEx_ConfigBreakDeadTime(&htim1, &sBreakDeadTime);

    // 5. 关键：配置更新事件触发ADC（PWM计数到中点时触发采样）
    TIM1->CR2 |= TIM_CR2_MMS_1; // 主模式：更新事件作为TRGO输出
}

void bsp_pwm_set_duty(float da, float db, float dc)
{
    // 占空比转CCR值，限幅保护
    uint32_t ccr1 = (uint32_t)(da * tim1_arr);
    uint32_t ccr2 = (uint32_t)(db * tim1_arr);
    uint32_t ccr3 = (uint32_t)(dc * tim1_arr);
    
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, ccr1);
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, ccr2);
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_3, ccr3);
}

void bsp_pwm_enable(void)
{
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
    HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);
    HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_2);
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_3);
    HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_3);
}

void bsp_pwm_disable(void)
{
    HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_1);
    HAL_TIMEx_PWMN_Stop(&htim1, TIM_CHANNEL_1);
    // 其余通道同理
}
```

### 2. ADC电流采样 `bsp_adc.h/c`
用ADC1注入通道，由TIM1硬件触发，转换完成后触发中断，电流环就在这个中断里执行。
#### bsp_adc.h
```c
#ifndef BSP_ADC_H
#define BSP_ADC_H

#include "foc_config.h"

extern ADC_HandleTypeDef hadc1;

void bsp_adc_init(void);
void bsp_adc_calibrate_offset(void); // 电流零点校准
void bsp_adc_get_current(float *ia, float *ib); // 读取两相电流

#endif
```

#### bsp_adc.c 核心要点
```c
#include "bsp_adc.h"

ADC_HandleTypeDef hadc1;
static float adc_offset_a = 0, adc_offset_b = 0;
static const float ADC_TO_CURRENT = 0.1f; // 根据采样电阻+运放增益计算

void bsp_adc_init(void)
{
    // ADC1配置：12位分辨率，注入通道硬件触发
    hadc1.Instance = ADC1;
    hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
    hadc1.Init.Resolution = ADC_RESOLUTION_12B;
    hadc1.Init.ScanConvMode = ENABLE;
    hadc1.Init.ContinuousConvMode = DISABLE;
    hadc1.Init.NbrOfConversion = 0;
    hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    HAL_ADC_Init(&hadc1);

    // 关键：注入通道配置，由TIM1 TRGO触发
    ADC_InjectionConfTypeDef sConfigInjected = {0};
    sConfigInjected.InjectedChannel = ADC_CHANNEL_1; // A相电流
    sConfigInjected.InjectedRank = ADC_INJECTED_RANK_1;
    sConfigInjected.InjectedSamplingTime = ADC_SAMPLETIME_1CYCLE_5;
    sConfigInjected.ExternalTrigInjecConv = ADC_EXTERNALTRIGINJEC_T1_TRGO; // TIM1触发
    sConfigInjected.AutoInjectedConv = DISABLE;
    HAL_ADCEx_InjectedConfigChannel(&hadc1, &sConfigInjected);

    // 第二路注入通道：B相电流
    sConfigInjected.InjectedChannel = ADC_CHANNEL_2;
    sConfigInjected.InjectedRank = ADC_INJECTED_RANK_2;
    HAL_ADCEx_InjectedConfigChannel(&hadc1, &sConfigInjected);

    // 开启注入转换完成中断
    HAL_ADCEx_InjectedStart_IT(&hadc1);
}

// 上电静止时校准零点
void bsp_adc_calibrate_offset(void)
{
    uint32_t sum_a = 0, sum_b = 0;
    for(int i=0; i<1000; i++)
    {
        sum_a += HAL_ADCEx_InjectedGetValue(&hadc1, ADC_INJECTED_RANK_1);
        sum_b += HAL_ADCEx_InjectedGetValue(&hadc1, ADC_INJECTED_RANK_2);
        HAL_Delay(1);
    }
    adc_offset_a = sum_a / 1000.0f;
    adc_offset_b = sum_b / 1000.0f;
}

void bsp_adc_get_current(float *ia, float *ib)
{
    float raw_a = HAL_ADCEx_InjectedGetValue(&hadc1, ADC_INJECTED_RANK_1) - adc_offset_a;
    float raw_b = HAL_ADCEx_InjectedGetValue(&hadc1, ADC_INJECTED_RANK_2) - adc_offset_b;
    *ia = raw_a * ADC_TO_CURRENT;
    *ib = raw_b * ADC_TO_CURRENT;
}
```

### 3. 编码器驱动 `bsp_encoder.h/c`
优先推荐**硬件编码器接口（TIM2）**，零软件开销、读取仅需几ns，同步性最好；如果用AS5600等绝对式编码器，换成SPI读取即可。
#### bsp_encoder.h
```c
#ifndef BSP_ENCODER_H
#define BSP_ENCODER_H

#include "foc_config.h"

void bsp_encoder_init(void);
float bsp_encoder_get_mech_angle(void); // 读取机械角度 0~2π
float bsp_encoder_get_speed(void);      // 读取机械转速 rad/s

#endif
```

#### bsp_encoder.c（硬件编码器模式）
```c
#include "bsp_encoder.h"

TIM_HandleTypeDef htim2;
static int32_t enc_cnt_prev = 0;
static float speed_prev = 0;
static const float ENC_CPR = 4096.0f; // 编码器线数*4（四倍频）
static const float ANGLE_PER_CNT = _2PI / ENC_CPR;

void bsp_encoder_init(void)
{
    // TIM2配置为编码器接口模式，AB相四倍频
    htim2.Instance = TIM2;
    htim2.Init.Prescaler = 0;
    htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim2.Init.Period = ENC_CPR - 1;
    HAL_TIM_Encoder_Init(&htim2);
    
    TIM_Encoder_InitTypeDef sEncoder = {0};
    sEncoder.EncoderMode = TIM_ENCODERMODE_TI12; // AB相都计数，四倍频
    sEncoder.IC1Polarity = TIM_ICPOLARITY_RISING;
    sEncoder.IC2Polarity = TIM_ICPOLARITY_RISING;
    HAL_TIM_Encoder_Init(&htim2, &sEncoder);
    
    HAL_TIM_Encoder_Start(&htim2, TIM_CHANNEL_ALL);
}

float bsp_encoder_get_mech_angle(void)
{
    int32_t cnt = __HAL_TIM_GET_COUNTER(&htim2);
    return normalize_angle(cnt * ANGLE_PER_CNT);
}

// 1kHz调用一次计算转速
float bsp_encoder_get_speed(void)
{
    int32_t cnt_now = __HAL_TIM_GET_COUNTER(&htim2);
    int32_t diff = cnt_now - enc_cnt_prev;
    
    // 处理过零跳变
    if(diff >  ENC_CPR/2) diff -= ENC_CPR;
    if(diff < -ENC_CPR/2) diff += ENC_CPR;
    
    float speed = diff * ANGLE_PER_CNT * 1000.0f; // 1kHz调用，rad/s
    enc_cnt_prev = cnt_now;
    return speed;
}
```

---

## 六、核心：中断级电流环（最关键部分）
在 `stm32g4xx_it.c` 的ADC注入转换完成中断里执行电流环，保证采完立刻算，零抖动、完全同步。

```c
#include "main.h"
#include "foc_config.h"
#include "bsp_adc.h"
#include "bsp_encoder.h"
#include "pi_controller.h"
#include "foc_math.h"

// 全局FOC变量
static pi_t pi_q;
static float iq_ref = 0.5f; // 初始给定小电流，安全
static float zero_e_angle = 0; // 编码器电角度零点

void ADC1_2_IRQHandler(void)
{
    HAL_ADC_IRQHandler(&hadc1);
    
    // ========== 20kHz 电流环核心 ==========
    float ia, ib;
    float i_alpha, i_beta;
    float id_fdb, iq_fdb;
    float vd, vq;
    float v_alpha, v_beta;
    float da, db, dc;

    // 1. 读取刚转换完成的电流（新鲜数据，硬件同步）
    bsp_adc_get_current(&ia, &ib);

    // 2. 读取当前编码器角度，计算电角度（和电流采样紧挨着，时差<1us）
    float theta_mech = bsp_encoder_get_mech_angle();
    float theta_e = normalize_angle(POLE_PAIRS * theta_mech - zero_e_angle);

    // 3. Clark + Park 变换
    clark_transform(ia, ib, &i_alpha, &i_beta);
    park_transform(i_alpha, i_beta, theta_e, &id_fdb, &iq_fdb);

    // 4. Iq 单环PI控制（初期Id强制0，不控制）
    float err = iq_ref - iq_fdb;
    vq = pi_run(&pi_q, err, PWM_PERIOD_US * 1e-6f);
    vd = 0.0f; // D轴电压强制0，最简闭环

    // 5. 电压圆形限幅
    voltage_circle_limit(&vd, &vq, VOLTAGE_MAX);

    // 6. 反Park + SVPWM
    inv_park_transform(vd, vq, theta_e, &v_alpha, &v_beta);
    svpwm_calc(v_alpha, v_beta, &da, &db, &dc);

    // 7. 更新PWM占空比，下一个周期自动生效
    bsp_pwm_set_duty(da, db, dc);
}
```

> 注意：中断内只做纯计算，绝对不能加延时、打印、HAL_Delay，保证执行时间<10us。

---

## 七、主函数与状态机 `main.c`
负责初始化、零点校准、状态切换、外环控制，所有慢逻辑都放主循环，不干扰电流环。

```c
#include "foc_config.h"
#include "bsp_pwm.h"
#include "bsp_adc.h"
#include "bsp_encoder.h"
#include "pi_controller.h"

// 电机状态枚举
typedef enum {
    MOT_STOP,
    MOT_ALIGN,
    MOT_RUN,
    MOT_FAULT
} motor_state_t;

static motor_state_t motor_state = MOT_STOP;
extern float iq_ref;
extern float zero_e_angle;
extern pi_t pi_q;

// 编码器零点校准
void motor_align(void)
{
    // 固定输出电压到270°，把转子吸到固定位置
    float v_alpha, v_beta, da, db, dc;
    inv_park_transform(0, 2.0f, _3PI_2, &v_alpha, &v_beta);
    svpwm_calc(v_alpha, v_beta, &da, &db, &dc);
    bsp_pwm_set_duty(da, db, dc);
    bsp_pwm_enable();
    
    HAL_Delay(1000); // 等待转子稳定1秒
    
    // 记录此时电角度为零点
    float theta_mech = bsp_encoder_get_mech_angle();
    zero_e_angle = normalize_angle(POLE_PAIRS * theta_mech);
    
    bsp_pwm_disable();
    HAL_Delay(200);
}

int main(void)
{
    HAL_Init();
    SystemClock_Config(); // 170MHz系统时钟

    // 硬件初始化
    bsp_pwm_init();
    bsp_adc_init();
    bsp_encoder_init();
    
    // 电流零点校准（电机静止不通电）
    HAL_Delay(500);
    bsp_adc_calibrate_offset();
    
    // PI控制器初始化
    pi_init(&pi_q, CURRENT_KP, CURRENT_KI, VOLTAGE_MAX);

    while(1)
    {
        switch(motor_state)
        {
            case MOT_STOP:
                bsp_pwm_disable();
                pi_q.integral = 0; // 停止时清积分，避免启动冲击
                // 等待启动指令
                if(/* 接收到启动信号 */) {
                    motor_align();      // 先校准零点
                    motor_state = MOT_RUN;
                    iq_ref = 0.5f;      // 初始小电流
                    bsp_pwm_enable();
                }
                break;

            case MOT_RUN:
                // 这里可放1kHz速度环、故障检测、给定更新
                // 速度环输出只修改 iq_ref，不直接操作PWM
                break;

            case MOT_FAULT:
                bsp_pwm_disable();
                pi_q.integral = 0;
                // 故障处理、等待复位
                break;
        }
        
        // 其他慢任务：串口通信、按键、指示灯
        HAL_Delay(1);
    }
}
```

---

## 八、调试步骤（严格按顺序，避免炸机）
1. **硬件验证**：不开PWM，旋转电机，确认编码器角度连续、电流采样零点接近0。
2. **开环验证**：注释掉PI控制，固定`vq=1V`，电角度手动缓慢递增，电机应平稳低速旋转，验证相序和PWM输出。
3. **堵转闭环**：手捏住电机轴，给小电流`iq_ref=0.5A`，观察Iq反馈能否稳定跟踪给定。
4. **旋转测试**：松开电机轴，确认电机平稳旋转，Iq不发散、转速不无限飙升。
5. **逐步升级**：稳定后再加Id闭环 → 加交叉解耦前馈 → 加速度环 → 加位置环。

---

## 九、后续优化方向
1. 启用G431的CORDIC硬件加速，替换三角函数和开方运算；
2. 加入交叉解耦前馈，提升高速性能；
3. 加入Id=0控制，完善双电流环；
4. 增加硬件刹车（BKIN引脚），实现微秒级过流保护；
5. 加入速度环、位置环，完善完整伺服控制。

需要我再补充一下**速度环的完整实现**，或者**CORDIC硬件加速的替换代码**吗？



----------
----------
----------



----------
----------
----------