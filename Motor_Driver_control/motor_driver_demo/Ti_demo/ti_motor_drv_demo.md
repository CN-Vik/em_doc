可以。你这个需求我建议**不要直接照搬 TI 的某一块 EVM 引脚分配**，而是按照你最终要做的产品功能，重新规划一套 MCU IO。

你的目标是：

> **F2800137 + 48V 三相 MOS 功率桥 + 栅极驱动 + 三相电流采样 + 母线电压 + 三相反电动势检测 + Hall + 增量编码器 + 有感/无感 FOC + 有感/无感六步换向 + CAN/UART/SPI**

这个需求下，**F2800137 完全够用**。它有 120 MHz C28x、2 个 12-bit 4-MSPS ADC、最多 21 个 ADC 输入、14 个 ePWM、1 个 eQEP、1 个 CAN、1 个 SPI 和 3 个 SCI。([Texas Instruments][1])

而且 TI 官方的 Universal Motor Control Lab 本身就支持 F280013x 的 **Sensored FOC、Sensorless FOC、Sensorless Trapezoidal**，以及 Hall、增量编码器、3-shunt 等方案。([软件下载台][2])

---

# 一、我先给你最终方案

我建议你直接选：

> **F2800137 64-pin 封装**

不要为了省一点封装成本选 32/48 pin。

你的需求已经比较多，**64 pin 更舒服**。

整体架构：

```text
                    F2800137
                120 MHz C28x
                     │
      ┌──────────────┼──────────────┐
      │              │              │
     ePWM           ADC            eQEP
      │              │              │
      │              │              └── 增量编码器
      │              │
      │              ├── IU
      │              ├── IV
      │              ├── IW
      │              ├── VBUS
      │              ├── BEMF_U
      │              ├── BEMF_V
      │              └── BEMF_W
      │
      ├── UH
      ├── UL
      ├── VH
      ├── VL
      ├── WH
      └── WL
      │
      ▼
   DRV8353RS
      │
      ▼
   6 × MOSFET
      │
      ▼
    U V W
      │
     电机


Hall U ────────────── GPIO
Hall V ────────────── GPIO
Hall W ────────────── GPIO

DRV FAULT ─────────── GPIO/TZ

DRV SPI ───────────── SPI

CAN ───────────────── CAN

UART ──────────────── SCI
```

这就是我认为最适合你这个项目的结构。

---

# 二、最重要：6 路 PWM 怎么分？

这个地方我建议你**严格按照 ePWM 模块一对一对应一个桥臂**。

不要搞成：

```text
ePWM1A → U上
ePWM2A → U下
```

而应该：

```text
ePWM1A → UH
ePWM1B → UL

ePWM2A → VH
ePWM2B → VL

ePWM3A → WH
ePWM3B → WL
```

也就是：

| 功能     | F2800137 |
| ------ | -------- |
| U High | EPWM1A   |
| U Low  | EPWM1B   |
| V High | EPWM2A   |
| V Low  | EPWM2B   |
| W High | EPWM3A   |
| W Low  | EPWM3B   |

这套结构特别漂亮。

而且 F2800137 的 ePWM 本身支持 **Dead-Band + Trip Zone**，非常适合直接做三相桥。([Texas Instruments][3])

---

# 三、具体 GPIO，我建议这样分

这里有一个非常关键的地方。

TI 自己在 **LAUNCHXL-F2800137 + BOOSTXL-DRV8323** 的方案中，就采用过：

```text
HIN A → GPIO0 / EPWM1A
LIN A → GPIO1 / EPWM1B

HIN B → GPIO2 / EPWM2A
LIN B → GPIO3 / EPWM2B

HIN C → GPIO10 / EPWM6A
LIN C → GPIO11 / EPWM6B
```

TI E2E 官方讨论里明确给出了这个对应关系。([E2E China][4])

但是：

**如果你自己重新画 PCB，我反而建议用 EPWM1/2/3 连续三组。**

也就是：

```text
GPIO0  → EPWM1A → UH
GPIO1  → EPWM1B → UL

GPIO2  → EPWM2A → VH
GPIO3  → EPWM2B → VL

GPIO4  → EPWM3A → WH
GPIO5  → EPWM3B → WL
```

F2800137 的引脚复用表支持这组 EPWM1/2/3。([Texas Instruments][5])

### 我更推荐这个。

因为你自己的 PCB 没必要受 LaunchPad 原有排针限制。

---

# 四、为什么一定要留 6 路独立 PWM？

因为你不仅要 FOC。

你还要求：

> 有感六步 + 无感六步。

所以我不建议你使用：

```text
3个PWM
+
DRV8353内部自动生成互补PWM
```

这种方案。

你的 MCU 最好直接拥有：

```text
UH
UL
VH
VL
WH
WL
```

六个独立 PWM 控制量。

这样软件里面可以非常自由：

### FOC

```text
UH = PWM
UL = complementary PWM

VH = PWM
VL = complementary PWM

WH = PWM
WL = complementary PWM
```

### 有感六步

例如：

```text
U+ V-

UH = PWM
VL = ON

W = Hi-Z
```

然后：

```text
下一步

U+ W-

UH = PWM
WL = ON

V = Hi-Z
```

等等。

### 无感六步

```text
U+ V-

UH = PWM
VL = ON

W = Hi-Z

          ↓

ADC读取W相BEMF
```

所以六个 PWM 对你这个项目非常重要。

---

# 五、三相电流采样

我建议你：

> **三电阻低边采样**

也就是：

```text
U MOS
 │
Rs_U
 │
GND

V MOS
 │
Rs_V
 │
GND

W MOS
 │
Rs_W
 │
GND
```

然后：

```text
Rs_U → Current Amp → ADC
Rs_V → Current Amp → ADC
Rs_W → Current Amp → ADC
```

例如：

```text
ADC
 │
 ├── IU
 ├── IV
 └── IW
```

我强烈建议你**三相都采**。

虽然 FOC 理论上两相就够：

```text
Iw = -Iu - Iv
```

但是你还要做：

* 六步
* 保护
* 调试
* 波形分析
* 电流故障判断

所以三路都留下。

TI 官方的 Universal Motor Control Lab 本身就支持 3-shunt current sensing。([软件下载台][6])

---

# 六、ADC我建议一次性留 7 路

这个非常重要。

你的 ADC 不要只设计：

```text
IU
IV
VBUS
```

这样以后会后悔。

我建议直接：

```text
ADC
│
├── ADC_CH0  IU
├── ADC_CH1  IV
├── ADC_CH2  IW
│
├── ADC_CH3  VBUS
│
├── ADC_CH4  BEMF_U
├── ADC_CH5  BEMF_V
└── ADC_CH6  BEMF_W
```

也就是：

> **3电流 + 1母线 + 3反电动势 = 7 ADC**

F2800137 有两个 12-bit ADC、最多 21 个外部 ADC 通道，所以这个资源非常宽裕。([Texas Instruments][1])

---

# 七、为什么 BEMF 一定要单独留？

因为你明确要求：

> **无感六步换向**

无感六步的核心就是：

```text
驱动两相

U+ V-

第三相：

W = Hi-Z

        ↓

检测 W 相 BEMF

        ↓

过零

        ↓

延迟 30°电角度

        ↓

换向
```

所以你必须有：

```text
BEMF_U
BEMF_V
BEMF_W
```

三个 ADC。

---

# 八、这里千万不要把 BEMF 和电流采样混为一谈

你最终硬件应该是：

```text
              48V
               │
        ┌──────┴──────┐
        │ 三相MOS桥   │
        └──────┬──────┘
               │
          U ───┼───→ 电机
          V ───┼───→ 电机
          W ───┼───→ 电机
               │
               │
     ┌─────────┼─────────┐
     │         │         │
   BEMF_U    BEMF_V    BEMF_W
     │         │         │
    ADC       ADC       ADC
```

和：

```text
MOS下桥臂
 │
Rs
 │
电流放大器
 │
ADC
```

完全是两套检测电路。

---

# 九、Hall 怎么规划？

你要求：

> 有感 FOC + 有感六步

所以至少：

```text
Hall_U
Hall_V
Hall_W
```

三个数字输入。

我建议：

```text
GPIO20 → Hall_U
GPIO21 → Hall_V
GPIO22 → Hall_W
```

**这里只是我给你的 PCB 规划建议，不是说必须用这三个 GPIO。**

原则是：

> 三个 Hall 尽量连续放在一起，并且全部进入普通 GPIO/XBAR 可用输入。

Hall：

```text
001
101
100
110
010
011
```

直接查表得到六步换向状态。

---

# 十、编码器怎么办？

你又要求：

> 有感 FOC

如果你的“有感”主要是：

* Hall

那么其实已经够。

但是如果你以后想做：

> 高性能有感 FOC

我建议直接把：

```text
A
B
Z
```

增量编码器接口也留出来。

F2800137 有一个 eQEP，并且支持 CW/CCW。([Texas Instruments][3])

例如规划：

```text
eQEP1A → Encoder A
eQEP1B → Encoder B
INDEX  → Encoder Z
```

这样以后：

```text
Hall FOC
```

和：

```text
Encoder FOC
```

都可以做。

---

# 十一、DRV8353RS怎么接？

我建议：

```text
F2800137
       │
       ├── UH
       ├── UL
       ├── VH
       ├── VL
       ├── WH
       └── WL
              │
              ▼
         DRV8353RS
              │
       ┌──────┼──────┐
       ↓      ↓      ↓
      U桥    V桥    W桥
       │      │      │
       └──────┼──────┘
              ↓
            Motor
```

另外：

```text
F2800137
    │
   SPI
    │
    ▼
DRV8353RS
```

SPI用于：

* 配置 DRV8353
* 读取状态
* 配置 OCP
* 配置 CSA
* 读取故障

---

# 十二、DRV8353的 FAULT 一定要接

这个我建议你直接：

```text
DRV8353 nFAULT
       │
       ▼
F2800137 GPIO
```

更进一步：

> **把过流保护直接接到 F2800137 的 ePWM Trip Zone。**

不要只靠软件：

```text
ADC
 ↓
CPU
 ↓
判断过流
 ↓
关闭PWM
```

太慢。

应该：

```text
过流
 ↓
硬件比较器/CMPSS
 ↓
ePWM TZ
 ↓
6路PWM硬件关断
```

F2800137 有 1 个完整 CMPSS 和 3 个 CMPSS_LITE，同时 ePWM 有硬件 Trip Zone。([Texas Instruments][3])

这个是 C2000 做电机控制非常爽的地方。

---

# 十三、所以我建议你把保护做成两级

### 一级：硬件立即保护

```text
电流采样
   ↓
CMPSS
   ↓
ePWM TZ
   ↓
PWM立即关闭
```

### 二级：软件保护

```text
ADC
 ↓
FOC
 ↓
软件判断
 ↓
Fault State
```

例如：

```text
过流
过压
欠压
过温
堵转
编码器异常
Hall非法状态
BEMF异常
```

---

# 十四、CAN / UART / SPI

我建议全部留下。

## SPI

```text
SPIA
 ↓
DRV8353RS
```

---

## UART

留一个：

```text
SCI
 ↓
USB-UART
```

用于：

```text
FOC参数
速度
Iq
Id
Uq
Ud
母线电压
电流
Fault
```

甚至你以后可以做类似：

> TI MotorControl GUI

的自己的上位机。

TI 的官方 F280013x 电机方案本身就有 UART GUI control/debug。([软件下载台][7])

---

## CAN

再留：

```text
CAN
 ↓
CANH
CANL
```

以后可以直接：

```text
CAN
 ↓
速度指令
位置指令
转矩指令
```

---

# 十五、我给你整理成一张“最终IO表”

这是我建议你 PCB 原理图直接采用的**功能分配表**：

| F2800137资源 | 建议功能      | 用途        |
| ---------- | --------- | --------- |
| EPWM1A     | UH        | U上管       |
| EPWM1B     | UL        | U下管       |
| EPWM2A     | VH        | V上管       |
| EPWM2B     | VL        | V下管       |
| EPWM3A     | WH        | W上管       |
| EPWM3B     | WL        | W下管       |
| ADC        | IU        | U相电流      |
| ADC        | IV        | V相电流      |
| ADC        | IW        | W相电流      |
| ADC        | VBUS      | 48V母线     |
| ADC        | BEMF_U    | U反电动势     |
| ADC        | BEMF_V    | V反电动势     |
| ADC        | BEMF_W    | W反电动势     |
| eQEP1A     | ENC_A     | 编码器A      |
| eQEP1B     | ENC_B     | 编码器B      |
| eQEP1I     | ENC_Z     | 编码器Z      |
| GPIO       | HALL_U    | Hall U    |
| GPIO       | HALL_V    | Hall V    |
| GPIO       | HALL_W    | Hall W    |
| GPIO/TZ    | DRV_FAULT | 驱动器故障     |
| SPIA       | DRV_SPI   | DRV8353配置 |
| SCI        | UART      | 调试/参数     |
| CAN        | CAN       | 通讯        |
| GPIO       | DRV_EN    | 驱动使能      |
| GPIO       | BRAKE     | 刹车/使能     |
| GPIO       | LED       | 状态指示      |

---

# 十六、这样你最终支持的模式就非常完整

### 模式1：有感 FOC

```text
Encoder
   ↓
eQEP
   ↓
电角度
   ↓
FOC
   ↓
SVPWM
```

---

### 模式2：Hall FOC

```text
Hall
 ↓
电角度
 ↓
FOC
 ↓
SVPWM
```

---

### 模式3：无感 FOC

```text
IU IV IW
   +
VBUS
   +
BEMF/Observer
      ↓
FAST/eSMO
      ↓
电角度
      ↓
FOC
      ↓
SVPWM
```

TI 官方 Universal Motor Control Lab 对 F280013x 就提供 Sensorless/Sensored FOC，以及 FAST/eSMO 等方案。([软件下载台][6])

---

### 模式4：有感六步

```text
Hall U/V/W
     ↓
Hall状态
     ↓
六步换向表
     ↓
6 PWM
     ↓
MOS
```

---

### 模式5：无感六步

```text
BEMF_U
BEMF_V
BEMF_W
   ↓
ADC
   ↓
过零检测
   ↓
30°电角延迟
   ↓
六步换向
```

---

# 十七、我特别建议你增加一个“模式切换状态机”

最终软件不要写成：

```c
if (foc)
...
else if (six_step)
...
```

到处乱切。

建议：

```text
MOTOR_MODE
│
├── SENSOR_FOC
│
├── SENSORLESS_FOC
│
├── HALL_FOC
│
├── SENSOR_SIX_STEP
│
└── SENSORLESS_SIX_STEP
```

然后统一：

```text
Motor_Init()
     ↓
Motor_Start()
     ↓
Motor_Mode_Select()
     ↓
┌──────────────────────┐
│                      │
FOC                Six-Step
│                      │
├─Sensored             ├─Hall
├─Sensorless           └─BEMF
└─Hall/Encoder
```

这样你后面调试会舒服很多。

---

# 十八、但是有一个地方我要特别提醒你

你说的是：

> **48V电机**

那么我建议你的硬件不要只做到“48V能跑”。

建议按：

> **60V甚至80V级功率级设计**

因为：

```text
48V母线
```

电机换向的时候会产生：

```text
MOSFET switching spike
motor back EMF
wiring inductance spike
```

所以如果你选 MOS：

> **100V MOSFET**

通常会比 60V MOS 更合理。

DRV8353RS 本身适合这种外置 MOS 的三相栅极驱动结构，而 TI 官方也已经把 F280013x + DRV8353RS-EVM 纳入 Universal Motor Control Lab 的硬件支持。([软件下载台][6])

---

# 十九、最终我建议你做成这个架构

```text
                       ┌─────────────────────┐
                       │     F2800137        │
                       │                     │
     Encoder A ───────►│ eQEP                │
     Encoder B ───────►│                     │
     Encoder Z ───────►│                     │
                       │                     │
     Hall U ──────────►│ GPIO                │
     Hall V ──────────►│ GPIO                │
     Hall W ──────────►│ GPIO                │
                       │                     │
     IU ──────────────►│ ADC                 │
     IV ──────────────►│ ADC                 │
     IW ──────────────►│ ADC                 │
     VBUS ────────────►│ ADC                 │
     BEMF U ──────────►│ ADC                 │
     BEMF V ──────────►│ ADC                 │
     BEMF W ──────────►│ ADC                 │
                       │                     │
     DRV FAULT ───────►│ TZ/GPIO             │
                       │                     │
                       │ SPI ──────────────┐  │
                       │ CAN               │  │
                       │ UART              │  │
                       │                   │  │
     UH ◄──────────────│ EPWM1A            │  │
     UL ◄──────────────│ EPWM1B            │  │
     VH ◄──────────────│ EPWM2A            │  │
     VL ◄──────────────│ EPWM2B            │  │
     WH ◄──────────────│ EPWM3A            │  │
     WL ◄──────────────│ EPWM3B            │  │
                       └─────────────────────┘
                                      │
                                      ▼
                              ┌──────────────┐
                              │ DRV8353RS    │
                              └──────┬───────┘
                                     │
                              ┌──────┴───────┐
                              │ 6 × 100V MOS │
                              └──────┬───────┘
                                     │
                                    U V W
                                     │
                                    电机
```

**这套架构我认为非常适合你。**

而且你现在已经在 ESP32 上做过 AS5600、ADC、SVPWM、Id/Iq PI、六步换向，所以换到 F2800137 后，你可以把你现在的软件思想基本原样迁过去，只把底层换成：

```text
ESP32 MCPWM
      ↓
F2800137 ePWM

ESP32 ADC
      ↓
F2800137 ADC + PPB

ESP32普通GPIO
      ↓
F2800137 GPIO/XBAR

ESP32软件过流
      ↓
F2800137 CMPSS + ePWM TZ
```

**真正值得你利用的是最后这一项：CMPSS + ePWM Trip Zone。** 这会让你的 48V ESC 的硬件保护比你现在 ESP32 那套明显更专业。F2800137 本身有 14 个 ePWM、硬件死区/Trip Zone，以及 CMPSS 资源，正适合做这种实时电机控制。([Texas Instruments][3])

最后一个建议：**如果你准备正式画这块 PCB，我建议直接按 64-pin F2800137 做，并把 ADC、ePWM、eQEP、Hall、BEMF、DRV SPI/FAULT 全部一次性布好。** 这样以后你甚至可以在同一块板上同时支持 **FOC / 有感六步 / 无感六步 / Hall / 编码器 / 无感 FOC**，不会因为 IO 不够被迫改板。


----------------------------
---------------------------
---------------------------


对，那这样反而更简单。你这里的“有感”我建议明确分成两类：

1. **三相 Hall** → 有感六步 + Hall 有感 FOC
2. **AS5600 类磁编码器** → 高精度有感 FOC
3. **无感 FOC** → 走 TI 的观测器
4. **无感六步** → 三相 BEMF 检测

而且 **AS5600 这种 SPI/I²C 磁编码器不应该占用 eQEP**。eQEP 是给 ABZ 增量编码器用的，你现在可以把它空出来。

---

# 我会重新给你这样规划

```text
                         F2800137
                            │
       ┌────────────────────┼────────────────────┐
       │                    │                    │
      ePWM                 ADC                 GPIO/XBAR
       │                    │                    │
       │              ┌─────┴─────┐              │
       │              │           │              │
       │           电流/电压     BEMF            │
       │              │           │              │
       │              │           │              │
       │              │           │        Hall U/V/W
       │              │           │              │
       │              │           │              │
       │              │           │              │
       ▼              ▼           ▼              ▼
   DRV8353RS       FOC采样      无感六步       有感六步
       │
       ▼
    6×MOS
       │
       ▼
      电机
```

另外：

```text
          F2800137
              │
       ┌──────┴──────┐
       │             │
      SPI           I2C
       │             │
       ▼             ▼
  AS5600类磁编码器
       │
       ▼
   机械角度 θm
       │
       ▼
   电角度 θe
       │
       ▼
     FOC
```

---

# 1. 三相 Hall

直接三个 GPIO：

```text
HALL_U → GPIO
HALL_V → GPIO
HALL_W → GPIO
```

我建议三个放在连续的 GPIO 上，比如：

```text
GPIO20 → HALL_U
GPIO21 → HALL_V
GPIO22 → HALL_W
```

当然最终要以你选择的 **F2800137具体封装和 datasheet 的 PinMux 表**为准。

Hall主要给：

### 有感六步

```text
Hall
 ↓
001
 ↓
换相表
 ↓
PWM
```

以及：

### Hall FOC

```text
Hall
 ↓
六个状态
 ↓
估算电角度
 ↓
FOC
```

不过要注意：

**Hall 的角度分辨率只有 60°电角度量级。**

所以如果你已经有 AS5600，真正高性能的有感 FOC 应该优先使用 AS5600。

---

# 2. AS5600类磁编码器

这个就不要接 GPIO 读模拟电压。

直接：

```text
F2800137
   │
  SPI/I²C
   │
   ▼
磁编码器
```

如果你能选择，我甚至建议：

> **优先选 SPI 磁编码器，而不是 I²C。**

因为你的 FOC：

```text
20 kHz
```

而 AS5600 这类 I²C 器件本身更新速度和通信速度会成为瓶颈。

你之前用 AS5600 就已经遇到过：

> 编码器更新慢 → FOC 20 kHz，但角度实际上没有 20 kHz 更新。

这在 FOC 里面非常明显。

---

# 3. SPI 磁编码器会舒服很多

比如未来换成类似：

```text
AS5047
AS5048
MA730
MA702
```

这种 SPI 磁编码器。

结构：

```text
             SPI
F2800137 ─────────── Encoder
   │                   │
   │                   │
   │                Angle
   │                   │
   └───────────────────┘
```

然后每个 FOC 周期：

```text
20 kHz
 ↓
读取角度
 ↓
θm
 ↓
θe = p × θm + offset
 ↓
Park
```

这比你现在 AS5600 的情况更适合高性能 FOC。

---

# 4. 所以 ADC 就不需要给 AS5600 留通道

这一点非常重要。

你之前如果是：

```text
AS5600
 ↓
Analog OUT
 ↓
ADC
```

那才占 ADC。

但是你现在说的是：

> **SPI/I²C 的 AS5600 类型磁编码器**

那么：

```text
AS5600
 ↓
SPI/I²C
```

所以 ADC 可以全部留给电机控制。

---

# 5. ADC我依然建议保留这7路

```text
ADC
│
├── IU
├── IV
├── IW
│
├── VBUS
│
├── BEMF_U
├── BEMF_V
└── BEMF_W
```

这样你的 ADC 资源非常干净：

### FOC

```text
IU IV IW
VBUS
```

### 无感六步

```text
BEMF_U
BEMF_V
BEMF_W
```

---

# 6. AS5600和BEMF千万别共用输入

建议原理图明确分开：

```text
              ┌── ADC ── IU
              │
电流采样 ─────┼── ADC ── IV
              │
              └── ADC ── IW


电机UVW
 │
 ├── BEMF_U ── ADC
 ├── BEMF_V ── ADC
 └── BEMF_W ── ADC


磁编码器
 │
 └── SPI/I²C ── F2800137
```

这样软件架构也特别清晰。

---

# 7. 我反而建议你 SPI 和 I²C 都留

如果 PCB 空间允许：

```text
SPIA
 ↓
磁编码器

I2C
 ↓
预留
```

这样以后：

```text
AS5047
AS5048
MA730
MA702
```

可以直接 SPI。

如果客户/项目使用：

```text
AS5600
```

就走 I²C。

---

# 8. 最终传感器接口我会这样做

PCB上直接放：

### HALL接口

```text
HALL
┌─────────┐
│ 5V/3.3V │
│ GND     │
│ HALL_U  │
│ HALL_V  │
│ HALL_W  │
└─────────┘
```

### Encoder接口

```text
ENCODER
┌─────────┐
│ 3.3V    │
│ GND     │
│ SPI_SCK │
│ SPI_MISO│
│ SPI_MOSI│
│ SPI_CS  │
└─────────┘
```

另外可以再预留：

```text
I2C_SCL
I2C_SDA
```

---

# 9. 这样你的五种模式就非常清楚

| 模式       | 传感器           |
| -------- | ------------- |
| Hall 六步  | Hall U/V/W    |
| Hall FOC | Hall U/V/W    |
| 编码器 FOC  | SPI/I²C 磁编码器  |
| 无感 FOC   | 电流 + 电压 + 观测器 |
| 无感六步     | BEMF U/V/W    |

其中：

```text
                 电机
                   │
       ┌───────────┼───────────┐
       │           │           │
      Hall       Encoder      BEMF
       │           │           │
       ▼           ▼           ▼
    六步/FOC      FOC         六步
```

---

## 10. 对你这个项目，我甚至建议软件定义成这样

```c
typedef enum
{
    MOTOR_SENSOR_HALL,
    MOTOR_SENSOR_ENCODER,
    MOTOR_SENSORLESS,
} motor_sensor_type_t;

typedef enum
{
    MOTOR_CONTROL_FOC,
    MOTOR_CONTROL_SIX_STEP,
} motor_control_type_t;
```

组合以后就是：

```text
HALL + SIX_STEP
HALL + FOC

ENCODER + FOC

SENSORLESS + FOC
SENSORLESS + SIX_STEP
```

这样你这块板实际上就成了一块**通用48V BLDC/PMSM电机控制器**。

---

### 还有一个我非常建议你改的地方

既然你现在已经明确要 **AS5600 类型磁编码器**，那么我前面给你的 **eQEP A/B/Z** 可以直接删掉，不需要为了它浪费 IO。

把这几个 IO 留给：

```text
SPI磁编码器
I²C磁编码器
Hall
DRV8353 FAULT
DRV ENABLE
CAN
UART
```

更有价值。

**而且如果你准备正式画 F2800137 这块 48V ESC，我建议下一步直接把“F2800137 64-pin 每一个 GPIO 的具体编号 + PinMux + ADC通道 + ePWM + SPI/I²C + Hall + DRV8353RS”给你排成一张可以直接放进原理图的 Pin Assignment 表。**这样就不是“功能规划”，而是可以直接开始画 PCB 的级别了。


------------------
--------------------
----------------------

对，你这个问题要**重新算一遍 IO 预算**。你前面这个方案里我把“ADC 检测运放”理解得不够准确。

你现在实际需求是：

* F2800137
* 48V 三相逆变
* 6 路 PWM
* DRV8353RS
* 三相电流采样 → **外部运放/电流采样放大器 → ADC**
* VBUS ADC
* 三相 BEMF ADC
* 三相 Hall
* AS5600 → **I²C**
* DRV8353 SPI
* CAN
* UART
* FAULT / ENABLE
* JTAG
* 可能还要温度 ADC

**结论先说：**

> **F2800137 64-pin 是够的，而且余量还可以；但 48-pin 我不建议。**
>
> 真正需要重点规划的不是“GPIO 数量不够”，而是 **ADC 引脚与 GPIO/外设复用冲突，以及 ADC 通道如何分配到 ADCA/ADCB、如何和 PWM 同步采样。**

F2800137 64-pin 版本有 **38 个 GPIO、10 个模拟引脚可作为数字输入、最多 21 个外部 ADC 通道、2 个 4-MSPS 12-bit ADC、14 路 ePWM、2 个 I²C、1 个 SPI、1 个 CAN、3 个 SCI**。所以资源本身相当充足。([Texas Instruments][1])

---

# 1. 先把你真正需要的 IO 数量算出来

我按照你最终这块板来算。

## PWM：6 个

```text
EPWM1A → UH
EPWM1B → UL

EPWM2A → VH
EPWM2B → VL

EPWM3A → WH
EPWM3B → WL
```

**6 IO**

F2800137 的 ePWM 本身带死区和硬件 Trip Zone，非常适合这个结构。([德州仪器][2])

---

# 2. AS5600：只需要 2 个 IO

你现在明确是：

> AS5600 I²C

那么：

```text
I2C_SCL
I2C_SDA
```

就是：

**2 IO**

而且 F2800137 有 **2 个 I²C**，所以完全没压力。([Texas Instruments][1])

这里我建议：

```text
I2CA_SCL
I2CA_SDA
```

专门给 AS5600。

---

# 3. Hall：3 个 IO

```text
HALL_U
HALL_V
HALL_W
```

**3 IO**

就是普通 GPIO 输入即可。

---

# 4. DRV8353RS SPI

DRV8353RS 需要 SPI 配置。

最基本：

```text
SPI_CLK
SPI_SIMO
SPI_SOMI
SPI_CS
```

**4 IO**

如果你还想单独：

```text
DRV_EN
DRV_FAULT
```

再：

**2 IO**

所以 DRV 这边：

> **6 IO**

---

# 5. CAN

CAN：

```text
CAN_TX
CAN_RX
```

**2 IO**

F2800137 有 1 路 CAN。([Texas Instruments][1])

---

# 6. UART

调试串口：

```text
UART_TX
UART_RX
```

**2 IO**

F2800137 有 3 路 SCI，所以非常宽裕。([Texas Instruments][1])

---

# 7. LED / 按键之类

比如：

```text
STATUS_LED
FAULT_LED
START
```

算：

**3 IO**

---

# 8. JTAG

这个不要忘。

正式 PCB 肯定需要：

```text
JTAG
```

调试接口会占用若干专用引脚/功能复用引脚。

所以我建议**不要为了省 GPIO 去动 JTAG**。

---

# 9. 到这里数字 IO 大概多少？

我们算：

```text
PWM                 6
AS5600 I2C           2
Hall                 3
DRV SPI              4
DRV EN/FAULT         2
CAN                  2
UART                 2
LED/控制             3
-----------------------
                    24
```

也就是说：

> **真正的数字功能大约 24 个 IO。**

而 64-pin F2800137 有 38 个 GPIO。([Texas Instruments][1])

所以：

```text
38 GPIO
-24 功能
----
14 个左右余量
```

而且实际还可以通过复用减少一些冲突。

**所以 GPIO 数量完全够。**

---

# 10. 真正需要注意的是 ADC

这个才是你的重点。

你现在至少需要：

### 三相电流

```text
IU
IV
IW
```

3 ADC

### 母线

```text
VBUS
```

1 ADC

### 三相 BEMF

```text
BEMF_U
BEMF_V
BEMF_W
```

3 ADC

所以：

```text
3 + 1 + 3 = 7 ADC
```

---

# 11. 如果你还有温度检测

我建议再加：

```text
MOS_TEMP
MOTOR_TEMP
```

2 ADC

那么：

```text
7 + 2 = 9 ADC
```

再加一个：

```text
VREF / 其他电源监控
```

甚至：

```text
10 ADC
```

都没问题。

F2800137 最多支持 **21 个外部 ADC 通道**。([Texas Instruments][1])

所以 ADC 资源同样很充足。

---

# 12. 你说的“ADC检测运放的IO”其实要分清楚

这个地方非常关键。

假设你三相电流检测是：

```text
U相分流电阻
    ↓
电流采样运放
    ↓
IU
    ↓
F2800137 ADC
```

那么 MCU 只需要：

```text
IU → 1个 ADC 输入
```

**运放本身不占 MCU GPIO。**

比如：

```text
        48V MOS桥
             │
             ↓
          Rsense
             │
             ↓
        INA240 / 运放
             │
             │ IU
             ↓
       F2800137 ADC
```

MCU根本不需要：

```text
OPAMP_IN+
OPAMP_IN-
OPAMP_OUT
```

这些都是**板上的模拟网络**。

MCU只看：

> **OPAMP_OUT → ADC**

---

# 13. 所以你的电流检测可能是这样

例如三路：

```text
          Rs_U
            │
            ▼
        ┌────────┐
        │ INA240 │────── IU ─── ADC
        └────────┘

          Rs_V
            │
            ▼
        ┌────────┐
        │ INA240 │────── IV ─── ADC
        └────────┘

          Rs_W
            │
            ▼
        ┌────────┐
        │ INA240 │────── IW ─── ADC
        └────────┘
```

那么 MCU：

```text
ADCINx = IU
ADCINy = IV
ADCINz = IW
```

就完事。

---

# 14. 但是你可能还需要一个特别重要的模拟量：运放参考/偏置

如果你采用：

```text
双向电流检测
```

通常：

```text
0A → 1.65V
+I → >1.65V
-I → <1.65V
```

那么你可能会有：

```text
VREF_CURRENT
```

但这个也不一定需要 ADC。

可以：

```text
1.65V参考
     │
     ├── INA240 U
     ├── INA240 V
     └── INA240 W
```

直接由硬件产生。

如果你希望软件实时监测这个参考，再占 1 个 ADC。

---

# 15. 所以我实际建议你留 10 个 ADC

不要只布 7 个。

我建议：

```text
ADC0  IU
ADC1  IV
ADC2  IW

ADC3  VBUS

ADC4  BEMF_U
ADC5  BEMF_V
ADC6  BEMF_W

ADC7  MOS_TEMP
ADC8  MOTOR_TEMP

ADC9  CURRENT_VREF
```

**10 ADC。**

F2800137 有 21 个 ADC 输入，所以你还剩一半以上。([Texas Instruments][1])

---

# 16. 而且 F2800137 的 ADC 速度非常够

它有：

> **2 × 4-MSPS 12-bit ADC**

所以你的：

```text
FOC = 20kHz
```

甚至：

```text
20kHz
↓
ADC同步采样
↓
三相电流
↓
FOC
```

完全没问题。

而且它每个 ADC 还有 **4 个 PPB（Post-Processing Block）**，非常适合做电流偏置/限值处理。([Texas Instruments][1])

---

# 17. 我反而建议你这样分 ADCA / ADCB

不要把所有电流和电压全扔到一个 ADC。

建议：

```text
ADCA

SOC0 → IU
SOC1 → IV
SOC2 → IW
SOC3 → VBUS
SOC4 → BEMF_U
```

然后：

```text
ADCB

SOC0 → BEMF_V
SOC1 → BEMF_W
SOC2 → MOS_TEMP
SOC3 → MOTOR_TEMP
SOC4 → CURRENT_VREF
```

这样：

```text
ADCA + ADCB
```

可以并行采样。

对于 FOC 尤其重要的是：

```text
IU
IV
IW
```

尽可能**同时采样/紧密采样**。

---

# 18. 但是 BEMF 不一定需要 20kHz

这个也可以优化。

你做：

### FOC

```text
20kHz

IU
IV
IW
VBUS
```

### 无感六步

```text
比如 10~20kHz

BEMF_U
BEMF_V
BEMF_W
```

所以 ADC SOC 可以由不同 ePWM 触发。

没必要所有东西都 20kHz。

---

# 19. 你的 AS5600 反而是 IO 最简单的部分

最终：

```text
F2800137
   │
   ├──── I2CA_SCL ──── AS5600 SCL
   │
   └──── I2CA_SDA ──── AS5600 SDA
```

只占：

> **2 个 IO**

AS5600 本身还需要：

```text
3.3V
GND
```

这两个当然不算 MCU IO。

---

# 20. 这里还有一个好消息

F2800137 的 I²C 引脚复用非常灵活。

例如 datasheet 中：

* GPIO0 可以复用为 I2CA_SDA
* GPIO1 可以复用为 I2CA_SCL
* GPIO4 可以复用为 I2CA_SCL
* GPIO5 可以复用为 I2CA_SDA
* GPIO8 可以复用为 I2CA_SCL
* GPIO10 可以复用为 I2CA_SDA

等等。([Texas Instruments][3])

所以你在画 PCB 的时候有相当大的 PinMux 调整空间。

---

# 21. 我建议你最终不要使用 eQEP

这个现在可以彻底确定：

你没有：

```text
A/B/Z增量编码器
```

而是：

```text
AS5600
 ↓
I²C
```

所以：

> **eQEP1 全部不用。**

这就给你省下了很多 IO 规划压力。

---

# 22. 我重新给你做一次最终资源预算

| 功能           | MCU IO/ADC |
| ------------ | ---------: |
| 三相 PWM       |          6 |
| AS5600 I²C   |          2 |
| Hall U/V/W   |          3 |
| DRV8353 SPI  |          4 |
| DRV EN       |          1 |
| DRV FAULT    |          1 |
| CAN          |          2 |
| UART         |          2 |
| LED/按键       |        2~3 |
| **数字 IO 合计** |  **23~24** |
| 三相电流         |      3 ADC |
| VBUS         |      1 ADC |
| 三相 BEMF      |      3 ADC |
| MOS 温度       |      1 ADC |
| 电机温度         |      1 ADC |
| 电流参考         |      1 ADC |
| **ADC 合计**   |     **10** |

而 F2800137：

```text
64-pin
│
├── 38 GPIO
├── 10 analog pins可数字输入
│
├── 21 ADC channels
├── 14 ePWM
├── 2 I2C
├── 1 SPI
├── 1 CAN
└── 3 SCI
```

所以从资源上：

# **完全够。**

([Texas Instruments][1])

---

# 23. 甚至还有很大的余量

你现在：

```text
6 PWM
```

但是 F2800137：

```text
14 PWM
```

你只用了：

```text
PWM1
PWM2
PWM3
```

剩下：

```text
PWM4
PWM5
PWM6
PWM7...
```

还可以拿来做：

* ADC 触发
* 同步
* 辅助 PWM
* 风扇
* 刹车
* DC/DC
* 预充电控制

所以非常富裕。

---

# 24. 真正让我比较担心的是另一个问题

不是 IO 数量。

而是：

> **你这个 48V + 三相 MOS + 三相电流采样 + BEMF + AS5600 + Hall 的 PCB，ADC 模拟布局会比 IO 规划重要得多。**

尤其：

```text
MOS高速开关节点
       ↓
      dv/dt
       ↓
     地弹
       ↓
电流采样运放
       ↓
ADC
```

如果布局不好：

```text
IU = 真实电流 + PWM噪声
```

然后你的：

```text
Id/Iq
```

都会抖。

所以我会把 ADC 设计成：

```text
                 ┌──────────────┐
IU ─────────────►│              │
IV ─────────────►│    F2800137 │
IW ─────────────►│     ADCA/B   │
VBUS ───────────►│              │
                 └──────────────┘
                       ▲
                       │
                  ADC模拟地
                       │
             ──────────┴────────
                  功率地/采样地
```

并且**三相电流运放输出端到 MCU ADC 引脚附近做 RC 抗混叠/去毛刺**，具体 R/C 要根据你选的电流采样放大器和 ADC 驱动要求来定。

---

## 最终结论

**你完全不用担心 F2800137 64-pin 的 IO 不够。**

而且按照你现在已经确定的传感器：

```text
三相 Hall
+
AS5600 I²C
```

比我之前假设的：

```text
Hall
+
ABZ编码器
```

还省 IO。

你真正应该采用的是：

```text
              F2800137 64pin

6 PWM ───────────→ DRV8353RS ─→ 6 MOS

3 ADC ←────────── 电流采样运放
1 ADC ←────────── VBUS
3 ADC ←────────── 三相BEMF
2 ADC ←────────── 温度
1 ADC ←────────── 电流参考

3 GPIO ←───────── Hall U/V/W

2 GPIO ────────── I2C ─→ AS5600

4 GPIO ────────── SPI ─→ DRV8353RS
2 GPIO ←───────── FAULT/EN

2 GPIO ←→ CAN
2 GPIO ←→ UART

JTAG ───────────── 调试
```

**这个资源配置是很宽裕的。**

而且 TI 官方数据也确认 64-pin F2800137 有 38 个 GPIO、21 个 ADC 输入、14 路 ePWM、2 路 I²C、1 路 SPI、1 路 CAN 和 3 路 SCI。

**下一步最有价值的不是继续数 IO，而是把 F2800137 64-pin 的“具体 GPIO 编号”逐个排出来。** 因为真正画 PCB 时会遇到 PinMux 冲突，例如 `GPIO0~5` 同时可以是 ePWM、I²C、SPI、CAN 等功能；这时候需要把 **6 PWM + I²C AS5600 + SPI DRV8353 + Hall + CAN + UART + 10路ADC** 一起做一次完整 PinMux，才能得到一张可以直接拿去画原理图的最终表。TI 的 datasheet 确实提供了完整的 64-pin PinMux 表。




-------------------
----------------
-----------------
# 软件example


有，而且**你现在这个 F2800137 方案，在 CCS 里不是从零开始写 FOC**。TI 官方已经给了可以直接导入 CCS 的完整工程。

最关键的就是：

## `Universal Motor Control Lab`

TI 目前的 **C2000Ware MotorControl SDK 6.00.00.00** 明确支持 **F280013x**，并且现在还有对应的 **SysConfig Lab**。([Texas Instruments][1])

官方工程目录大致就是：

```text
C2000Ware_MotorControl_SDK_6.00.00.00
│
└── solutions
    │
    └── universal_motorcontrol_lab
        │
        └── f280013x
            │
            └── ccs
                │
                └── motor_control
```

TI 官方用户指南明确写了 **F280013x 对应的 Universal Motor Control Lab 工程**，可以直接在 CCS 里面 Import CCS Projects。([Texas Instruments][2])

---

# 而且它不是普通 Example

这个很重要。

不是这种：

```text
examples
└── epwm
    └── epwm_ex1.c
```

而是完整的：

```text
Universal Motor Control Lab
        │
        ├── PWM
        ├── ADC
        ├── Current sensing
        ├── Clarke
        ├── Park
        ├── PI
        ├── SVPWM
        ├── Speed loop
        ├── Position/angle
        ├── Motor identification
        ├── Sensorless estimator
        └── GUI
```

TI 自己对这个 Lab 的定位就是：

> 用于实验各种电机控制算法，同时可以作为你自己产品设计的参考工程。([Texas Instruments][3])

---

# 你的 F2800137 可以直接用

官方文档甚至直接写了：

```text
F280025C
F280039C
F2800137
```

都可以使用 Universal Motor Control Lab。

对于 F2800137，工程路径就是：

```text
solutions
└── universal_motorcontrol_lab
    └── f280013x
        └── ccs
            └── motor_control
```

然后 CCS：

```text
Project
  ↓
Import CCS Projects
  ↓
选择上面的目录
  ↓
universal_motorcontrol_lab_f280013x
```

([Texas Instruments][2])

---

# 甚至有 DRV8323RS 的现成配置

这个和你现在的方案非常接近。

TI 官方用户指南给的例子就是：

```text
LAUNCHXL-F2800137
        +
BOOSTXL-DRV8323RS
        +
Universal Motor Control Lab
```

然后在 CCS 里面选择对应的 Build Configuration，例如：

```text
Flash_lib_DRV8323RS_3SC
```

也就是说：

```text
F2800137
   ↓
DRV8323RS
   ↓
三相MOS
   ↓
电机
```

**TI 已经给你准备好了软件工程。** ([Texas Instruments][2])

---

# 但是你要注意一个关键区别

你现在准备做的是：

```text
F2800137
+
DRV8353RS
+
6 MOS
+
三相电流
+
Hall
+
AS5600 I2C
+
BEMF
```

而 TI Universal Lab **不是一个“一键支持你全部功能”的现成产品固件**。

它更准确地说是：

> **TI 给你的 FOC 基础框架 + 多种电机控制算法 + 多种硬件配置。**

你可以在它上面改。

---

# 你现在最适合这样用

## 第一阶段：先完全跑官方 Demo

买：

```text
LAUNCHXL-F2800137
        +
BOOSTXL-DRV8323RS
```

然后：

```text
CCS
 ↓
C2000Ware MotorControl SDK
 ↓
Universal Motor Control Lab
 ↓
F280013x
 ↓
DRV8323RS
 ↓
电机
```

先让官方程序把电机转起来。

这一步非常重要。

因为你可以先确认：

```text
PWM
ADC
电流采样
PI
SVPWM
FOC
```

整个 C2000 环境没问题。

---

# 第二阶段：把你的 AS5600 接进去

官方现成工程通常不是直接针对：

```text
AS5600 I2C
```

所以你自己增加：

```text
I2C
 ↓
AS5600
 ↓
机械角度
 ↓
电角度
 ↓
FOC
```

这个反而不难。

你现在已经自己做过 AS5600 + FOC，这部分迁移过去就行。

---

# 第三阶段：增加 Hall

增加：

```text
GPIO
├── Hall_U
├── Hall_V
└── Hall_W
```

然后：

```text
Hall状态
 ↓
六步换向表
```

或者：

```text
Hall
 ↓
电角度估算
 ↓
FOC
```

---

# 第四阶段：增加无感六步

这个是你自己的功能：

```text
UVW
 ↓
BEMF分压
 ↓
ADC
 ↓
过零检测
 ↓
换向
```

这个不要期待 TI Universal Lab 给你直接“一键配置”。

它和 TI 的 Sensorless FOC 是两套东西。

---

# 第五阶段：最终做成你自己的 Motor Control Framework

最终你的软件可以变成：

```text
MotorControl
│
├── FOC
│   ├── sensored
│   │   ├── Hall
│   │   └── AS5600
│   │
│   └── sensorless
│       ├── FAST
│       └── eSMO
│
└── SixStep
    ├── Hall
    └── BEMF
```

然后统一：

```c
motor_mode = MOTOR_MODE_FOC_AS5600;
```

或者：

```c
motor_mode = MOTOR_MODE_FOC_HALL;
```

或者：

```c
motor_mode = MOTOR_MODE_FOC_SENSORLESS;
```

或者：

```c
motor_mode = MOTOR_MODE_SIXSTEP_HALL;
```

或者：

```c
motor_mode = MOTOR_MODE_SIXSTEP_BEMF;
```

---

# 还有一个你会很喜欢的东西：SysConfig

现在的 MotorControl SDK 已经明确加入：

> **Universal Motor Control SysConfig Lab supports F280013x**

所以你不用全部手撸寄存器。

可以在 SysConfig 里配置：

```text
ePWM
ADC
GPIO
PinMux
SPI
I2C
```

然后生成初始化代码。([Texas Instruments][1])

这对于你这种**自己做一块 F2800137 48V ESC**特别有价值。

---

# 甚至 TI 官方还有专门的视频

TI 有一套：

**Motor Control SDK – Universal Project and Lab**

里面专门讲：

1. Universal Lab 是什么
2. 怎么导入 CCS
3. 怎么运行
4. 怎么用 SysConfig

([Texas Instruments][3])

---

## 所以最终答案

**有，而且是你要的那种“可以直接在 CCS 里面打开的完整 FOC 工程”。**

你现在应该下载：

[C2000Ware MotorControl SDK 6.00.00.00](https://www.ti.com/tool/download/C2000WARE-MOTORCONTROL-SDK?utm_source=chatgpt.com)

然后找：

```text
solutions/
└── universal_motorcontrol_lab/
    └── f280013x/
        └── ccs/
            └── motor_control/
```

TI 官方文档已经明确说明 **F2800137 + Universal Motor Control Lab**，而且官方示例就是拿 **F2800137 + DRV8323RS** 来跑。

**不过你现在真正需要搞清楚的是：F2800137 + DRV8353RS 有没有“现成配置”，以及 Universal Lab 里面到底有没有 Hall、AS5600、Sensorless FOC、六步这些具体代码。** 这个我可以继续帮你把 **SDK 6.0.0.0 里面的 `f280013x` 工程目录和各个 `.c/.h/.syscfg` 文件逐个扒出来**，告诉你哪些能直接拿，哪些必须自己写。


--------------------------
-------------------
# Ti官方 FOC example项目解读
对，这个目录一看就比较清楚了。你现在已经不是在看“普通 C2000 外设 Demo”，而是在看 TI **Universal Motor Control Lab 的完整电机控制框架**。

而且你这个目录里有几个东西特别值得注意：

```text
observers/
├── encoder
├── esmo
├── hall
├── isbldc
├── speedcalc
├── speedfr
└── ssipd
```

这说明 **TI 已经把你想要的很多“有感/无感”基础算法模块直接放进来了**。

下面我结合你实际要做的：

> **F2800137 + DRV8353RS + 48V + 三电阻采样 + Hall + AS5600 + 无感 FOC + 无感六步**

把整个工程拆开讲。

---

# 一、先看整个工程的总架构

你现在：

```text
Universal Motor Control Lab_FOC_137
│
├── Flash_lib_DRV8329A_1SC
├── Flash_lib_DRV8353RS_3SC      ← 你现在应该使用这个
│
├── libraries
├── src_board
├── src_control
├── src_device
│
├── sys_main.c
├── sys_main.h
├── sys_settings.h
│
├── .project
├── .cproject
└── *.ccxml
```

可以把它理解成：

```text
                    Universal Motor Control Lab
                              │
          ┌───────────────────┼───────────────────┐
          │                   │                   │
       应用/主程序           控制算法             硬件
          │                   │                   │
      sys_main.c          src_control          src_board
          │                   │                   │
          │                   ├─ FOC             ├─ PWM
          │                   ├─ PI              ├─ ADC
          │                   ├─ SVPWM           ├─ DRV8353
          │                   ├─ Hall            └─ GPIO
          │                   ├─ Encoder
          │                   └─ ESMO
          │
          └───────────────────────────────┐
                                          │
                                      src_device
                                          │
                                   F2800137底层驱动
```

再往下一层：

```text
src_control
      ↓
libraries
      ↓
数学算法/控制算法
```

所以 TI 这个工程实际上是分层的。

---

# 二、最重要的两个目录：`src_control` 和 `src_board`

你以后自己移植的时候，**90%的时间都会在这两个地方活动。**

---

# 三、`src_board`：板级硬件层

你的目录：

```text
src_board
└── drvic
```

以及：

```text
Flash_lib_DRV8353RS_3SC
└── src_board
    └── drvic
```

这个 `drvic` 可以理解成：

> **DRVIC = Driver IC**

也就是电机驱动芯片相关的板级代码。

对于你选择的：

```text
Flash_lib_DRV8353RS_3SC
```

这里就是重点。

---

## 它负责什么？

比如：

```text
F2800137
   │
   ├── PWM
   │
   ├── ADC
   │
   ├── SPI
   │
   └── GPIO
          │
          ↓
      DRV8353RS
          │
          ↓
       6 MOS
```

所以 `src_board` 主要负责：

* PWM配置
* ADC配置
* GPIO配置
* 电流采样
* 母线电压采样
* DRV8353RS初始化
* SPI配置
* Fault
* Enable
* PWM输出
* 板级参数

---

# 四、你以后改自己PCB，最主要改这里

例如 TI 原版：

```text
LAUNCHXL-F2800137
+
BOOSTXL-DRV8323/DRV8353
```

但是你的板：

```text
F2800137
+
DRV8353RS
+
自己设计的MOS
+
自己设计的电流采样
```

那么：

```text
src_board
```

就是你重点修改的地方。

例如你最终可能变成：

```text
src_board
│
├── pwm.c
├── adc.c
├── hall.c
├── as5600.c
├── drv8353.c
└── board.c
```

---

# 五、`src_control`：真正的“电机控制逻辑”

这个更重要。

你这里：

```text
src_control
├── common
│   ├── debug
│   └── include
│
└── drivers
    └── include
```

这个目录更多是：

> **把底层硬件和上层控制算法连接起来的控制框架。**

它不是单纯放 Clarke/Park。

Clarke/Park 这些更加底层、通用的算法实际上放到了：

```text
libraries
```

里面。

---

# 六、`libraries` 才是算法宝库

你这个目录非常有价值。

```text
libraries
├── control
├── dacs
├── filter
├── math
├── observers
├── sfra
├── transforms
└── utilities
```

这就是 TI 把通用电机控制算法拆成模块后的结果。

---

# 七、`libraries/transforms`

你这里：

```text
transforms
├── clarke
├── ipark
├── park
├── svgen
└── volts
```

这就是你熟悉的 FOC 数学基础。

---

## Clarke

```text
Ia Ib Ic
   ↓
Clarke
   ↓
Iα Iβ
```

---

## Park

```text
Iα Iβ
  ↓
Park
  ↓
Id Iq
```

---

## IPark

```text
Vd Vq
 ↓
Inverse Park
 ↓
Vα Vβ
```

---

## SVGEN

```text
Vα Vβ
 ↓
SVGEN
 ↓
Ta Tb Tc
 ↓
PWM
```

所以整个 FOC 的核心链条就是：

```text
             电流采样
                ↓
             Ia Ib Ic
                ↓
             Clarke
                ↓
             Iα Iβ
                ↓
             Park
                ↓
             Id Iq
                ↓
          Id/Iq PI控制器
                ↓
             Vd Vq
                ↓
             IPark
                ↓
             Vα Vβ
                ↓
              SVGEN
                ↓
             Ta Tb Tc
                ↓
              ePWM
                ↓
             DRV8353
                ↓
               MOS
                ↓
               电机
```

这部分就是你以前 ESP32 FOC 自己写的东西。

**TI 已经模块化好了。**

---

# 八、`libraries/control`

这里：

```text
control
├── dclink_ss
├── fwc
├── mtpa
├── pi
└── vs_freq
```

这就开始进入控制算法了。

---

## `pi`

这个不用多说：

```text
PI
```

就是你现在自己写的：

```c
id_pid
iq_pid
speed_pid
```

里面应该有 TI 自己优化的 PI 实现。

---

# 九、`fwc`

通常是：

> Field Weakening Control

弱磁控制。

也就是高速区域：

```text
电压达到极限
       ↓
不能继续增加Uq
       ↓
增加负Id
       ↓
弱磁
       ↓
提高高速能力
```

对于你48V电机，如果以后做到高速，这个很有用。

---

# 十、`mtpa`

Maximum Torque Per Ampere：

> 最大转矩/安培控制。

主要用于 PMSM/IPMSM。

例如：

```text
Id ≠ 0
Iq
```

通过优化 Id/Iq 比例，在同样电流下获得更大转矩。

对于普通表贴式 BLDC/PMSM：

```text
Id ≈ 0
```

不一定马上需要。

---

# 十一、`vs_freq`

这个一般是：

> Voltage-Speed/Frequency 相关控制模块

主要用于 V/f、开环启动、频率/电压轨迹等。

你以后做：

```text
开环启动
无感启动
```

可能会看到它。

---

# 十二、`dclink_ss`

这个非常值得你注意。

`DC Link Single Shunt`。

也就是：

> **单电阻母线电流采样/电流重构**

你现在选的是：

```text
DRV8353RS_3SC
```

也就是：

```text
Three Shunt Current
```

所以你当前主要不用它。

但是 TI 把它也放进来了。

---

# 十三、最有意思的是 `observers`

你这个目录：

```text
observers
├── encoder
├── esmo
├── est_lib
├── hall
├── isbldc
├── speedcalc
├── speedfr
└── ssipd
```

这个对于你的项目非常重要。

因为你想做：

```text
有感FOC
无感FOC
Hall六步
无感六步
```

这里已经出现了几个对应模块。

---

# 十四、`observers/encoder`

这个就是：

> 编码器相关的角度/速度处理。

你的：

```text
AS5600
```

虽然不是 ABZ/eQEP 编码器，但是**算法层面完全可以借鉴这里的接口思想**。

你的最终结构：

```text
AS5600
 ↓
I2C
 ↓
机械角度 θm
 ↓
× 极对数
 ↓
电角度 θe
 ↓
FOC
```

你不一定能直接把 `encoder` 文件原封不动拿来，因为 AS5600 是 I²C。

但是控制层面的：

```text
angle
speed
direction
```

可以复用。

---

# 十五、`observers/hall`

这个非常直接：

> **Hall 传感器**

你要做：

```text
Hall U
Hall V
Hall W
```

然后：

```text
Hall状态
 ↓
电角度
 ↓
速度
```

这里 TI 已经有对应模块。

所以你的 Hall FOC：

```text
Hall
 ↓
hall observer
 ↓
θe
 ↓
Park
```

有很大概率可以直接参考。

---

# 十六、`observers/esmo` ⭐⭐⭐

这个就是你非常关心的：

> Enhanced Sliding Mode Observer

增强型滑模观测器。

用于：

# **Sensorless FOC**

也就是：

```text
没有Hall
没有编码器

        ↓

三相电流
+
电机电压
+
电机参数

        ↓

ESMO

        ↓

估算电角度
估算速度
```

所以你要做：

```text
无感FOC
```

这里就是重点研究对象。

---

# 十七、`observers/est_lib`

这个一般属于 TI 的电机参数/状态估算相关库。

你以后看到：

```text
estimator
```

相关代码时会碰到它。

它和：

```text
电机参数
磁链
转速
位置
```

这些估算有关。

---

# 十八、`observers/isbldc` ⭐

这个也非常有意思。

名字：

```text
isbldc
```

基本就是：

> Sensorless BLDC

也就是：

# **无感BLDC六步控制**

你想要的：

```text
无感六步换向
```

正好对应这一类东西。

核心思想：

```text
三相端电压
      ↓
BEMF
      ↓
过零检测
      ↓
换向时刻
      ↓
六步换向
```

所以你前面规划的：

```text
BEMF_U
BEMF_V
BEMF_W
```

就是给这一类算法用的。

---

# 十九、这就非常有意思了

你原来自己规划的是：

```text
Hall六步
AS5600 FOC
无感FOC
无感六步
```

而 TI 这个目录里恰好出现：

```text
hall
encoder
esmo
isbldc
```

对应关系几乎就是：

| 你的需求       | TI库                     |
| ---------- | ----------------------- |
| Hall传感器    | `observers/hall`        |
| AS5600类编码器 | `observers/encoder` 可参考 |
| 无感FOC      | `observers/esmo`        |
| 无感BLDC六步   | `observers/isbldc`      |

**这就是为什么我之前说 Universal Motor Control Lab 很适合你。**

---

# 二十、`speedcalc`

这个就是速度计算相关。

比如：

```text
θ(k)
 ↓
θ(k)-θ(k-1)
 ↓
速度
```

然后再：

```text
速度滤波
```

这和你之前 ESP32：

```c
rpm = delta_angle / dt
```

是同一个问题。

---

# 二十一、`speedfr`

这个通常是速度滤波/速度反馈相关模块。

你的：

```text
AS5600
 ↓
角度
 ↓
速度
```

里面很可能会用到这一类模块。

---

# 二十二、`ssipd`

这是另一个无感启动/位置检测相关算法。

名字对应：

> Sensorless Initial Position Detection

也就是：

**无感初始位置检测。**

这个非常重要，因为：

```text
电机静止
 ↓
没有反电动势
 ↓
ESMO无法正常工作
```

怎么办？

需要：

```text
初始位置检测
/
对齐
/
启动
```

这类算法就属于这个范畴。

---

# 二十三、`libraries/filter`

```text
filter
└── filter_fo
```

滤波器。

比如：

```text
电流
速度
电压
```

这些反馈信号的滤波。

---

# 二十四、`libraries/utilities`

这个就是一些辅助工具。

你这里：

```text
utilities
├── angle_gen
├── cpu_time
├── datalog
├── mod6cnt
├── rimpulse
├── step_response
└── traj
```

几个特别值得你认识：

### `angle_gen`

角度发生器。

可以做：

```text
θ = ωt
```

用于：

* 开环
* 测试
* 算法验证

---

### `cpu_time`

CPU运行时间测量。

这对于 F2800137 很重要。

比如你的 FOC：

```text
20 kHz
```

每次只有：

```text
50 μs
```

那么可以测：

```text
FOC执行用了多少 μs
```

---

### `datalog`

数据记录。

类似你之前：

```text
VOFA+
FreeMASTER
```

拿：

```text
Id
Iq
Uq
Ud
Speed
Angle
```

出来观察。

---

### `mod6cnt`

Mod 6 counter。

这个和六步换向很相关。

```text
0
1
2
3
4
5
0
...
```

---

### `traj`

Trajectory。

做：

```text
速度给定变化
位置给定变化
```

避免目标值瞬间跳变。

比如：

```text
0 RPM
 ↓
2000 RPM
```

不是瞬间给2000，而是：

```text
0
100
200
300
...
2000
```

---

# 二十五、`libraries/sfra`

这个是：

> Software Frequency Response Analyzer

TI 非常牛的一个东西。

主要用于：

**测控制环路频率响应。**

比如你想知道：

```text
电流环带宽
速度环带宽
相位裕度
增益裕度
```

可以用 SFRA。

这对于你之前一直折腾：

```text
PI参数
电流环
速度环
```

非常有用。

---

# 二十六、`libraries/dacs`

你这里：

```text
dacs
└── dac128s085
```

这是外部 DAC。

可以把内部变量：

```text
Id
Iq
Speed
Uq
```

输出成模拟电压：

```text
变量
 ↓
DAC
 ↓
示波器
```

非常适合调试。

---

# 二十七、`src_device`

这个就不要和 `src_board` 混了。

```text
src_device
├── device_support
│   └── headers
└── driverlib
```

它属于：

# **F2800137芯片本身的底层支持**

也就是：

```text
F2800137
 ↓
寄存器
 ↓
DriverLib
```

比如：

```text
EPWM
ADC
GPIO
SPI
I2C
CAN
CPU Timer
```

这些外设的底层驱动都在这里。

---

# 二十八、所以三个目录的关系你一定要记住

这是整个工程最重要的理解。

```text
              应用
               │
          sys_main.c
               │
               ▼
          src_control
               │
               ▼
           libraries
        ┌──────┼───────┐
        ↓      ↓       ↓
      FOC    Observer  PI
        │      │       │
        └──────┼───────┘
               ↓
           src_board
               │
        ┌──────┼───────┐
        ↓      ↓       ↓
       PWM    ADC     DRV8353
        │      │       │
        └──────┼───────┘
               ↓
          src_device
               │
               ▼
           F2800137
```

简单说：

> **`src_device` = 芯片怎么控制**
>
> **`src_board` = 你的硬件怎么接**
>
> **`src_control` = 电机控制程序怎么组织**
>
> **`libraries` = TI已经写好的算法模块**
>
> **`sys_main.c` = 把这些东西串起来运行**

---

# 二十九、现在再看 `Flash_lib_DRV8353RS_3SC`

这个目录：

```text
Flash_lib_DRV8353RS_3SC
│
├── libraries
├── src_board
│   └── drvic
├── src_control
└── src_device
```

你可以把它理解成：

> **针对“DRV8353RS + 三电阻采样”的这一套完整 Build Configuration。**

它不是单纯“只有 DRV8353RS”。

它实际上把：

```text
FOC算法
+
电机观测器
+
ADC
+
PWM
+
DRV8353RS
+
三电阻采样
```

组合成一套可以编译的方案。

---

# 三十、为什么外面还有一份 `libraries/src_board/src_control/src_device`？

你可能马上会问：

> “那根目录下面为什么也有一份？”

就是：

```text
根目录
├── libraries
├── src_board
├── src_control
└── src_device
```

同时：

```text
Flash_lib_DRV8353RS_3SC
├── libraries
├── src_board
├── src_control
└── src_device
```

这个是 **Universal Motor Control Lab 的多 Build Configuration 工程结构**导致的。

你可以简单理解为：

```text
根目录
   ↓
公共/工程组织层

Flash_lib_DRV8353RS_3SC
   ↓
具体编译配置
```

真正你现在运行：

```text
Flash_lib_DRV8353RS_3SC
```

时，重点看这一套配置下面对应的：

```text
src_board
src_control
libraries
```

而不是看到根目录那一套就以为要全部复制。

**不要随便删除根目录的东西。**

CCS 的 `.cproject` / build configuration 会决定实际编译哪些文件。

---

# 三十一、你这个工程里 `sys_main.c` 反而非常值得研究

你的：

```text
sys_main.c
```

有：

```text
37 KB
```

这已经不是小程序了。

它很可能就是：

> **整个 Universal Motor Control Lab 的应用层主流程。**

你以后不要一上来研究：

```text
clarke.c
park.c
```

因为这些都是数学模块。

应该先研究：

# `sys_main.c`

看清楚：

```text
初始化
 ↓
PWM初始化
 ↓
ADC初始化
 ↓
DRV8353初始化
 ↓
控制对象初始化
 ↓
中断
 ↓
FOC
```

---

# 三十二、你现在可以把 TI 工程和你自己的 ESP32 工程对应起来

你现在 ESP32 大概是：

```text
ESP32 FOC
│
├── motor_pwm.c
├── motor_adc.c
├── motor_current.c
├── motor_angle.c
├── motor_foc.c
├── motor_pid.c
├── motor_svpwm.c
└── motor_speed.c
```

TI：

```text
C2000
│
├── src_board
│   └── PWM/ADC/DRV
│
├── src_control
│   └── 控制框架
│
├── libraries
│   ├── transforms
│   ├── control
│   ├── observers
│   ├── filter
│   └── utilities
│
└── sys_main.c
```

实际上思想是一模一样。

---

# 三十三、而且你的最终目标可以直接映射到这个框架

你最终想做：

```text
                    Motor Control
                         │
             ┌───────────┴───────────┐
             │                       │
            FOC                    Six-Step
             │                       │
      ┌──────┼──────┐          ┌─────┴─────┐
      │      │      │          │           │
     Hall   AS5600 无感       Hall        BEMF
      │      │      │          │           │
      ↓      ↓      ↓          ↓           ↓
     有感FOC 有感FOC 无感FOC  有感六步    无感六步
```

TI 现在这个 `libraries/observers` 已经给你铺好了相当大一部分：

```text
Hall
Encoder
ESMO
ISBLDC
Speed
Initial Position
```

你需要做的主要是：

### AS5600 I²C驱动

```text
I2C
 ↓
AS5600
 ↓
angle
```

### 你自己的硬件板级适配

```text
F2800137
 ↓
你的ADC
你的PWM
你的GPIO
你的DRV8353RS
```

### 最终模式管理

```text
FOC / Six-Step
```

---

# 三十四、所以我建议你现在先不要急着改代码

你现在已经拿到了一个非常好的东西：

```text
Universal Motor Control Lab
+
F2800137
+
DRV8353RS
+
3 Shunt
```

**第一阶段目标只有一个：**

> 把 `Flash_lib_DRV8353RS_3SC` 原封不动编译通过。

然后第二阶段：

```text
阅读 sys_main.c
```

搞清楚：

```text
main
 ↓
初始化
 ↓
ISR
 ↓
FOC执行
```

第三阶段再看：

```text
src_board
```

搞清楚：

```text
PWM
ADC
DRV8353RS
```

第四阶段再看：

```text
libraries/observers
```

搞清楚：

```text
hall
encoder
esmo
isbldc
```

**这个顺序比直接钻进几百个 `.c/.h` 文件有效得多。**

---

## 最后给你一张“你这个项目应该重点关注什么”的地图

```text
Universal Motor Control Lab
│
├─ ⭐ sys_main.c
│    └─ 整个程序怎么跑
│
├─ ⭐ Flash_lib_DRV8353RS_3SC
│    └─ 你当前48V硬件对应的Build Configuration
│
├─ ⭐ src_board
│    └─ 你的硬件
│       ├─ PWM
│       ├─ ADC
│       ├─ DRV8353RS
│       └─ GPIO
│
├─ ⭐ src_control
│    └─ 控制框架
│
├─ ⭐ libraries
│   │
│   ├─ ⭐ transforms
│   │   ├─ Clarke
│   │   ├─ Park
│   │   ├─ IPark
│   │   └─ SVGEN
│   │
│   ├─ ⭐ control
│   │   ├─ PI
│   │   ├─ MTPA
│   │   └─ FWC
│   │
│   ├─ ⭐ observers
│   │   ├─ Hall       ← 你的Hall
│   │   ├─ Encoder    ← AS5600可参考
│   │   ├─ ESMO       ← 无感FOC
│   │   ├─ ISBLDC     ← 无感六步
│   │   ├─ SSIPD      ← 无感初始位置
│   │   └─ Speed      ← 速度计算
│   │
│   ├─ filter
│   ├─ SFRA           ← 环路分析
│   └─ utilities
│
└─ src_device
     └─ F2800137底层
        ├─ ADC
        ├─ ePWM
        ├─ GPIO
        ├─ SPI
        ├─ I2C
        └─ CAN
```

**你这个目录已经非常明确地说明：TI 这套 Universal Motor Control Lab 基本就是可以拿来当你这个 F2800137 电机控制器的“软件底座”的。**

尤其是你看到的 **`hall + encoder + esmo + isbldc`**，这几个目录和你要求的 **Hall 有感、AS5600 有感、无感 FOC、无感六步**几乎是一一对应的。真正需要你自己补的核心东西，反而主要是 **AS5600 I²C 驱动 + 你自己48V硬件的 `src_board` 适配 + 最终模式切换逻辑**。


