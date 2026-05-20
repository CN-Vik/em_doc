# SDIO协议详解
**SDIO（Secure Digital Input Output）** 是在SD存储卡协议基础上扩展的**通用输入输出接口协议**，核心价值是让同一SD卡槽既能插SD卡存储，也能连接Wi‑Fi、蓝牙、GPS等外设，广泛用于嵌入式、移动设备与IoT场景。


---

## 一、核心定位与特点
- **兼容SD**：物理引脚与SD卡完全一致，支持SD/MMC卡即插即用。
- **一接口多用**：分时复用总线，存储与外设I/O共享同一组引脚。
- **主机‑从机架构**：主机（HOST）发起所有命令，从机（DEVICE）被动响应，简化外设设计。
- **多设备挂载**：同一总线最多支持**32个SDIO设备**并联。
- **高速+低速双模式**：兼顾高速数据（如Wi‑Fi）与低速控制（如传感器）。

---

## 二、物理接口（9针标准）
核心信号为**CLK、CMD、DAT0‑3**，引脚定义如下：
| 引脚 | 名称 | 功能 |
|---|---|---|
| 1 | CD/DAT3 | 卡检测/数据线3（4位模式） |
| 2 | CMD | 命令/响应（双向，主机发命令、从机回响应） |
| 3 | VSS1 | 地 |
| 4 | VDD | 电源（3.3V） |
| 5 | CLK | 时钟（主机输出，所有同步基于此） |
| 6 | VSS2 | 地 |
| 7 | DAT0 | 数据线0（1/4位模式） |
| 8 | DAT1 | 数据线1（4位）/中断（1位） |
| 9 | DAT2 | 数据线2（4位模式） |

### 工作模式
- **1位模式**：仅DAT0传数据，DAT1作中断；低速常用（≤400kHz）。
- **4位模式**：DAT0‑3并行传输，带宽翻倍；高速常用（≤50MHz）。
- **SPI模式**：兼容SPI，用于低速、简易连接。

---

## 三、协议核心：命令与传输
### 1. 关键命令（SDIO扩展）
- **CMD52**：**单字节读写**（配置外设寄存器、状态查询），如设置Wi‑Fi模式、读取GPS状态。
- **CMD53**：**块数据传输**（批量收发，支持DMA），如Wi‑Fi数据流、摄像头图像传输。
- **CMD5**：设备识别，区分SD卡/SDIO设备。

### 2. 通信流程
1. **初始化**：主机发CMD0/CMD5，识别设备类型（SD/SDIO），配置时钟与总线宽度（1/4位）。
2. **命令‑响应**：主机发CMD→从机回响应（固定格式，含状态/数据）。
3. **数据传输**：
   - 1位：DAT0串行收发。
   - 4位：DAT0‑3并行，每时钟传4bit。
   - 中断：DAT1通知主机外设事件（如数据就绪）。
4. **结束/休眠**：主机发停止命令，设备进入低功耗。

### 3. 传输速率（常见等级）
| 模式 | 时钟 | 位宽 | 理论带宽 |
|---|---|---|---|
| 标准 | 25MHz | 1位 | 25Mbps |
| 高速HS | 50MHz | 4位 | 200Mbps |
| UHS‑I SDR104 | 52MHz | 4位 | 208Mbps |

---

## 四、关键能力
- **中断支持**：DAT1线实现异步事件通知，避免轮询，降低CPU占用。
- **DMA传输**：支持直接内存访问，大数据量（如Wi‑Fi）无需CPU干预。
- **低功耗**：支持休眠/唤醒，适配电池供电设备。
- **热插拔**：运行中可插入/移除设备，主机自动检测并初始化。

---

## 五、典型应用
- **嵌入式Linux**：STM32/ESP32等主控通过SDIO驱动Wi‑Fi（如RTL8723）、蓝牙模块。
- **移动设备**：手机SD卡槽兼容SD卡与SDIO外设（如早期Wi‑Fi SD卡）。
- **IoT模块**：GPS、摄像头、条码扫描器等通过SDIO接入主控。

---

## 六、与SD/ SPI对比
| 特性 | SDIO | SD卡 | SPI |
|---|---|---|---|
| 用途 | 存储+外设I/O | 仅存储 | 通用低速外设 |
| 带宽 | 最高208Mbps | 同SDIO | 低（≤10Mbps） |
| 中断 | 支持 | 不支持 | 需额外GPIO |
| 多设备 | 最多32个 | 单设备 | 多设备（片选） |

---

## 七、总结
SDIO是SD标准的**外设扩展版**，用一套引脚解决“存储+高速外设”连接，兼顾兼容、性能与成本，是嵌入式与IoT领域的**常用高速接口**。

要不要我用STM32的SDIO驱动Wi‑Fi模块做一个极简流程示例（含关键寄存器配置与CMD52/CMD53调用）？

----------
---------
----------

SDIO 可以理解成 **SD 卡接口那套总线协议的扩展版**。

它最早是为了让 SD 卡槽不只插存储卡，还能插 **Wi-Fi 卡、蓝牙卡、GPS 卡、摄像头模块** 等 I/O 外设。所以名字叫：

> **SDIO = Secure Digital Input Output**

也就是：
**在 SD 总线上，不只传存储数据，也可以传外设 I/O 数据。**

---

# 1. SDIO 和 SD 卡是什么关系？

很多人容易混淆：

| 名称      | 含义                                 |
| ------- | ---------------------------------- |
| SD      | Secure Digital，主要指 SD 存储卡          |
| SDIO    | SD Input/Output，让 SD 总线支持 I/O 外设   |
| SDMMC   | SD / MMC 存储卡控制器，一般用于读写 SD 卡 / eMMC |
| SDIO 外设 | 比如 SDIO Wi-Fi 模块、SDIO 蓝牙模块         |
| SDIO 接口 | MCU/SoC 上的 SDIO 控制器外设              |

所以你可以这么理解：

```text
SD 总线协议
 ├── SD Memory：普通 SD 卡，读写块数据
 └── SDIO：I/O 设备，比如 WiFi / BT / GPS
```

有些 MCU 手册里把外设叫 **SDIO**，但它既可以接 SD 卡，也可以接 SDIO Wi-Fi 模块。
有些 Linux SoC 里叫 **SDMMC / MMC controller**，也可以跑 SDIO 设备。

---

# 2. SDIO 总线信号线

标准 SDIO 通常有这些线：

```text
CLK   时钟线，主机输出
CMD   命令线，双向
DAT0  数据线0，双向
DAT1  数据线1，双向
DAT2  数据线2，双向
DAT3  数据线3，双向
VDD   电源
GND   地
```

常见总线宽度：

| 模式       | 数据线              | 说明           |
| -------- | ---------------- | ------------ |
| 1-bit 模式 | DAT0             | 简单，线少        |
| 4-bit 模式 | DAT0~DAT3        | 速度更高         |
| SPI 模式   | MOSI/MISO/SCK/CS | 兼容 SPI，但性能较低 |

SDIO 的主机一般是 **主控 MCU/SoC**，外设是 **SD 卡或者 SDIO 模块**。

```text
MCU / SoC SDIO Host
        |
        | CLK
        | CMD
        | DAT0~DAT3
        |
SD Card / SDIO WiFi Module
```

---

# 3. SDIO 是主从结构

SDIO 不是像 CAN、I2C 那样多个设备随便抢总线。它是明显的 **Host 主机控制** 模型。

```text
主机发命令  ->  设备响应
主机读数据  <-  设备返回数据
主机写数据  ->  设备接收数据
```

也就是说，SDIO 设备不会主动乱发数据。
它一般通过 **中断信号** 或者 **DAT1 中断机制** 告诉主机：

```text
我有数据了，你来读我
```

然后主机再发命令把数据读出来。

---

# 4. SDIO 协议的核心：命令 + 响应 + 数据

SDIO 通信不是直接像 UART 那样发字节流，而是基于 **命令包** 的。

基本过程是：

```text
Host 发 CMD 命令
Device 返回 Response 响应
可选：Host 和 Device 之间传输 Data 数据块
```

典型结构：

```text
CMD 线：传输命令和响应
DAT 线：传输大量数据
CLK 线：所有传输都由 Host 时钟驱动
```

例如读 SD 卡一个扇区：

```text
Host -> CMD17：我要读某个地址的数据块
Card -> Response：可以
Card -> DAT0~DAT3：返回 512 字节数据
```

例如写 SD 卡一个扇区：

```text
Host -> CMD24：我要写某个地址的数据块
Card -> Response：可以
Host -> DAT0~DAT3：发送 512 字节数据
Card -> 返回写入状态
```

---

# 5. SDIO 命令格式

SD/SDIO 的命令通常是 48 bit，结构大概是：

```text
起始位 + 传输方向 + 命令号 + 参数 + CRC + 结束位
```

可以简单理解成：

```c
CMD_INDEX + ARGUMENT
```

例如：

```text
CMD0   复位卡
CMD8   检查电压和版本
CMD55  告诉卡下一个是 ACMD 命令
ACMD41 初始化 SD 存储卡
CMD17  读单块
CMD24  写单块
CMD52  SDIO 读写单个寄存器
CMD53  SDIO 批量读写数据
```

这里面非常重要的是：

| 命令    | 作用             |
| ----- | -------------- |
| CMD52 | SDIO 单字节寄存器读写  |
| CMD53 | SDIO 多字节 / 块传输 |

如果你接的是 SDIO Wi-Fi 模块，驱动里会大量用到 **CMD52 / CMD53**。

---

# 6. SD 卡通信流程

以普通 SD 卡为例，初始化流程大概是：

```text
1. 上电
2. Host 给 SD 卡提供低速时钟，一般 400kHz 左右
3. CMD0：让卡进入空闲状态
4. CMD8：确认 SD 版本和电压范围
5. ACMD41：反复查询卡是否初始化完成
6. CMD2：获取 CID
7. CMD3：获取 RCA 相对地址
8. CMD7：选择这张卡
9. ACMD6：设置 4-bit 总线模式
10. 提高 SDIO 时钟频率
11. CMD17 / CMD18 读数据
12. CMD24 / CMD25 写数据
```

可以画成这样：

```text
上电
  |
  v
低速时钟 400kHz
  |
  v
CMD0 复位
  |
  v
CMD8 检查版本/电压
  |
  v
ACMD41 等待卡 ready
  |
  v
CMD2/CMD3 获取卡信息和地址
  |
  v
CMD7 选中卡
  |
  v
切换 4-bit + 提高时钟
  |
  v
开始读写数据块
```

---

# 7. SDIO 外设通信流程

如果接的是 SDIO Wi-Fi 模块，比如一些 Wi-Fi 芯片，流程会不一样。

SDIO 设备内部通常分为多个 **Function**：

```text
Function 0：公共功能区，必须有
Function 1：WiFi 数据通道
Function 2：蓝牙或者其他功能
Function 3~7：扩展功能
```

SDIO 设备最多支持 7 个 I/O Function：

```text
Function 0：管理/配置
Function 1：I/O 功能1
Function 2：I/O 功能2
...
Function 7：I/O 功能7
```

SDIO 设备内部有几类重要寄存器：

| 区域                | 作用                                          |
| ----------------- | ------------------------------------------- |
| CCCR              | Common Card Configuration Registers，公共配置寄存器 |
| FBR               | Function Basic Registers，功能基本寄存器            |
| CIS               | Card Information Structure，设备信息结构           |
| Function Register | 具体功能自己的寄存器                                  |

SDIO Wi-Fi 模块通信时，大概是：

```text
1. 主机初始化 SDIO 总线
2. 识别这是 SDIO I/O 设备
3. 读取 CCCR / FBR / CIS 信息
4. 使能对应 Function
5. 配置块大小
6. 使能中断
7. 通过 CMD52 读写寄存器
8. 通过 CMD53 批量传输 WiFi 数据包
```

简单理解：

```text
CMD52：配置寄存器，小数据
CMD53：搬运数据包，大数据
```

---

# 8. CMD52 和 CMD53 是 SDIO 的重点

## CMD52：单字节读写

CMD52 用于访问 SDIO 设备的寄存器。

类似：

```text
读某个 Function 的某个地址的 1 字节
写某个 Function 的某个地址的 1 字节
```

比如：

```text
Host -> CMD52：读取 Function 0 的 CCCR 寄存器
Device -> Response：返回寄存器值
```

它很像 MCU 读写外设寄存器：

```c
value = sdio_readb(func, addr);
sdio_writeb(func, addr, value);
```

---

## CMD53：批量数据传输

CMD53 用于传输大量数据。

它支持：

| 模式                   | 说明             |
| -------------------- | -------------- |
| Byte Mode            | 按字节传输          |
| Block Mode           | 按块传输           |
| Fixed Address        | 固定地址读写，比如 FIFO |
| Incrementing Address | 地址递增，类似内存读写    |

例如 Wi-Fi 模块收发数据包：

```text
Host -> CMD53：从 WiFi 模块的 RX FIFO 读 1536 字节
WiFi -> DAT0~DAT3：返回一帧网络数据包
```

或者：

```text
Host -> CMD53：向 WiFi 模块的 TX FIFO 写 1500 字节
WiFi 模块：拿到数据后通过无线发出去
```

所以 SDIO Wi-Fi 比 SPI Wi-Fi 快，主要就是因为 CMD53 可以用 4-bit 数据线高速搬大块数据。

---

# 9. SDIO 的速度

SDIO 速度跟时钟频率、总线宽度有关。

理论吞吐可以粗略算：

```text
吞吐率 = 时钟频率 × 数据线位宽
```

例如：

```text
25MHz × 4bit = 100Mbit/s = 12.5MB/s
50MHz × 4bit = 200Mbit/s = 25MB/s
```

但实际速度会低一些，因为有：

```text
命令开销
响应开销
等待时间
CRC
驱动处理
DMA搬运
文件系统开销
设备内部处理速度
```

常见模式：

| 模式            |     时钟 |    位宽 |      理论速度 |
| ------------- | -----: | ----: | --------: |
| Default Speed |  25MHz | 4-bit |  12.5MB/s |
| High Speed    |  50MHz | 4-bit |    25MB/s |
| SDR50         |  50MHz | 4-bit |  约 50MB/s |
| SDR104        | 208MHz | 4-bit | 约 104MB/s |

MCU 上常见的是 25MHz 或 50MHz。
高端 SoC 上可以更高，比如 Linux 平台的 SDMMC 控制器。

---

# 10. SDIO 和 SPI 的区别

| 对比     | SDIO                 | SPI                |
| ------ | -------------------- | ------------------ |
| 线数     | 多，CMD + CLK + DAT0~3 | 少，MOSI/MISO/SCK/CS |
| 速度     | 高                    | 相对低                |
| 协议复杂度  | 高                    | 低                  |
| DMA 适配 | 很适合                  | 也适合                |
| 常见用途   | SD卡、eMMC、WiFi模块      | 传感器、Flash、屏幕、小外设   |
| 软件驱动   | 复杂                   | 简单                 |
| 硬件要求   | 需要 SDIO/SDMMC 控制器    | 普通 SPI 即可          |

简单说：

```text
SPI：简单、通用、低成本
SDIO：复杂、高速、适合大数据量传输
```

如果是 Wi-Fi 模块：

```text
UART WiFi：最慢，适合 AT 指令
SPI WiFi：中等
SDIO WiFi：最快，适合高吞吐网络数据
```

---

# 11. SDIO 和 eMMC 的关系

eMMC 不是 SDIO，但它们很像。

```text
SD 卡：可插拔存储卡
eMMC：焊在板子上的嵌入式 MMC 存储
SDIO：SD 总线上的 I/O 外设
```

很多 SoC 里的控制器叫：

```text
SDMMC 控制器
```

它可能同时支持：

```text
SD Card
SDIO Device
MMC/eMMC
```

比如嵌入式 Linux 里经常看到：

```text
mmc0: SD card
mmc1: SDIO WiFi
mmc2: eMMC
```

---

# 12. 从 MCU 软件角度看 SDIO

如果你是 MCU 裸机/RTOS 开发，SDIO 驱动一般分几层：

```text
应用层
  |
文件系统 FATFS / WiFi 协议栈
  |
SD 卡驱动 / SDIO WiFi 驱动
  |
SDIO Host 驱动
  |
SDIO 控制器寄存器
  |
硬件引脚 CLK/CMD/DAT
```

如果是读 SD 卡：

```text
应用层 fopen/fread/fwrite
  |
FATFS
  |
disk_read / disk_write
  |
SD 卡块读写 CMD17/CMD24
  |
SDIO Host + DMA
  |
SD 卡
```

如果是 SDIO Wi-Fi：

```text
TCP/IP 协议栈
  |
WiFi 驱动
  |
SDIO Function 驱动
  |
CMD52/CMD53
  |
SDIO Host + DMA
  |
WiFi 模块
```

---

# 13. SD 卡读写为什么通常是 512 字节？

SD 卡底层通常按 **块 block** 读写，常见块大小是：

```text
512 字节
```

也就是一个扇区。

所以文件系统读一个文件时，底层可能变成：

```text
读 FAT 表
读目录项
读数据簇
每次底层读 512 字节或者多个 512 字节
```

SDIO 驱动常见函数类似：

```c
sd_read_blocks(buffer, sector_addr, block_count);
sd_write_blocks(buffer, sector_addr, block_count);
```

比如：

```c
sd_read_blocks(buf, 0, 1);
```

表示从第 0 个扇区读 1 个 block，也就是 512 字节。

---

# 14. SDIO 通信里的 DMA

SDIO 很适合配合 DMA，因为传输量大。

例如读 SD 卡 4KB：

```text
SDIO 外设收到数据
      |
      v
DMA 自动搬到 RAM buffer
      |
      v
中断通知 CPU：传输完成
```

CPU 不需要一个字节一个字节读。

典型流程：

```text
1. 配置 DMA 目标地址
2. 配置 SDIO 数据长度
3. 发送读命令
4. SD 卡返回数据
5. SDIO 接收 FIFO 收到数据
6. DMA 从 SDIO FIFO 搬到 RAM
7. DMA 完成中断
8. 软件检查状态
```

这和你之前学的 ADC + DMA、SPI + DMA 很像。

---

# 15. SDIO 的几个关键概念

## 1）CMD 线

用于传输命令和响应。

```text
Host -> Device：命令
Device -> Host：响应
```

## 2）DAT 线

用于传输数据。

```text
DAT0：1-bit 数据模式
DAT0~DAT3：4-bit 数据模式
```

## 3）RCA

Relative Card Address，相对卡地址。
初始化过程中主机会给卡分配/读取一个地址，后续通过这个地址选中卡。

## 4）Block

数据块。SD 卡常见 512 字节一个 block。

## 5）CRC

命令和数据都有 CRC 校验，用来保证数据可靠性。

## 6）Clock

SDIO 时钟由主机提供。初始化阶段低速，初始化完成后再切高速。

---

# 16. SDIO 常见问题

## 问题 1：为什么初始化阶段要用低速时钟？

因为刚上电时，卡还没进入高速工作状态。

一般流程是：

```text
初始化阶段：约 400kHz
初始化完成：切到 25MHz / 50MHz
```

类似你跟一个新设备通信，刚开始先慢慢确认身份和能力，确认没问题后再高速传输。

---

## 问题 2：为什么 SDIO 要上拉电阻？

CMD 和 DAT 线通常需要上拉，尤其是 SD 卡场景。

常见上拉：

```text
CMD   上拉
DAT0  上拉
DAT1  上拉
DAT2  上拉
DAT3  上拉
```

因为 SD 总线有开漏/状态检测等需求。
如果上拉不对，可能导致：

```text
初始化失败
卡检测异常
CMD 无响应
数据 CRC 错误
偶发读写失败
```

---

## 问题 3：为什么 SDIO 走线要求比普通 GPIO 高？

因为 SDIO 时钟可能到几十 MHz，边沿很快。

需要注意：

```text
CLK/CMD/DAT 走线尽量短
数据线长度尽量一致
避免强干扰
CLK 线注意串阻
上拉电阻合适
地参考完整
```

否则可能出现：

```text
低速正常，高速失败
1-bit 正常，4-bit 失败
小数据正常，大数据 CRC 错误
热插拔不稳定
```

---

# 17. SDIO 和你熟悉的 SPI 类比

你可以把 SDIO 粗略类比成更复杂、更高速的 SPI：

```text
SPI:
CS 拉低
发送命令
发送/接收数据
CS 拉高
```

SDIO：

```text
CMD 线上发送命令
设备返回响应
DAT 线上传输数据
传输完成后检查状态
```

区别是 SDIO 有更复杂的状态机、命令响应、CRC、块传输、总线宽度切换、初始化流程。

---

# 18. 总结一句话

SDIO 本质上是：

> **一种基于 SD 总线的高速主从通信协议，通过 CMD 线传命令，通过 DAT0~DAT3 传数据，既可以读写 SD 卡，也可以连接 Wi-Fi/蓝牙等 I/O 设备。**

对于嵌入式开发来说，重点掌握这几件事：

```text
1. SDIO 是 Host 主导的协议
2. CMD 线负责命令和响应
3. DAT 线负责数据传输
4. SD 卡常用 CMD17/CMD24 读写块
5. SDIO 外设常用 CMD52/CMD53 读写寄存器和批量数据
6. 初始化阶段低速，工作阶段高速
7. 高速传输一般配合 DMA
8. SDIO 比 SPI 快，但驱动和硬件要求更复杂
```

可以把它记成：

```text
CMD 负责“说我要干啥”
DAT 负责“真正搬数据”
CLK 负责“给整个总线打节拍”
```

这样理解 SDIO 就比较清楚了。
