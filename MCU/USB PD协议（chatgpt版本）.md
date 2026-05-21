好的 👍，我来系统、详细地给你介绍 **USB Power Delivery（USB PD）协议** 的内容。

---

## 1. **USB PD 协议的背景**

* USB 最初只能提供 **5V / 500mA (USB 2.0)** 的供电能力；
* USB 3.0 提升到 **5V / 900mA**；
* 之后出现了 **BC1.2（Battery Charging）** 标准，可以到 **5V / 1.5A**；
* 但随着 **笔记本、显示器、手机快充** 的发展，功率需求远超 7.5W，所以引入了 **USB Power Delivery（USB PD）协议**；
* 通过 PD 协议，电源和设备可以**协商电压和电流**，最高能支持 **240W（48V/5A, PD 3.1 EPR）**。

---

## 2. **USB PD 协议的核心概念**

1. **角色 (Roles)**

   * **Source（供电端）**：提供电力（如充电器、笔记本电源适配器）。
   * **Sink（受电端）**：消耗电力（如手机、笔记本）。
   * **Dual Role Power (DRP)**：既能做 Source 也能做 Sink（如手机既能充电，也能给耳机充电）。

2. **电压档位（PDO：Power Data Object）**

   * **PD 2.0/3.0 SPR（Standard Power Range）**：

     * 5V, 9V, 15V, 20V（最高 100W）
   * **PD 3.1 EPR（Extended Power Range）**：

     * 新增 28V, 36V, 48V 档位（最高 240W）

3. **通信方式**

   * 使用 **CC1/CC2 引脚**（Configuration Channel）进行通信。
   * 物理层采用 **BMC 编码（Biphase Mark Code）**，速率约 300kbps。
   * 通过 **握手 & 协商** 达成一致的供电电压/电流。

---

## 3. **USB PD 协议的工作流程**

1. **插入检测**

   * 通过 **CC 引脚电阻**，检测设备是 **Source / Sink / DRP**。
   * 确认线缆能力（是否支持 3A/5A、E-Marker 芯片是否存在）。

2. **能力广播（Source Capabilities）**

   * Source 广播自己支持的电压电流组合（PDO 列表）。
   * 例如：

     * PDO1: 5V / 3A
     * PDO2: 9V / 3A
     * PDO3: 15V / 3A
     * PDO4: 20V / 5A

3. **能力请求（Request）**

   * Sink 选择一个电压电流组合（RDO：Request Data Object）。
   * 例如手机只要 9V/2A，则向 Source 请求对应的 PDO。

4. **确认（Accept & PS\_RDY）**

   * Source 确认是否接受；
   * 如果接受，则切换到相应电压，并发送 **Power Supply Ready (PS\_RDY)**。

5. **供电开始**

   * Sink 端检测到电压稳定后，进入正常供电模式。

---

## 4. **USB PD 协议的数据报文**

PD 报文有两大类：

1. **控制报文（Control Messages）**

   * GoodCRC（确认报文）
   * Accept / Reject
   * Ping / PS\_RDY / Soft\_Reset
   * 等等

2. **数据报文（Data Messages）**

   * Source\_Capabilities（源端能力）
   * Request（请求电压电流）
   * Sink\_Capabilities
   * Vendor Defined Message (VDM)（厂商自定义，比如 DP Alt Mode 切换）

---

## 5. **USB PD 版本演进**

1. **PD 1.0**

   * 只支持最高 20V / 2A = 40W。

2. **PD 2.0 / 3.0**

   * SPR 档位：5V, 9V, 15V, 20V；
   * 最大 100W（20V / 5A）；
   * 引入 **DRP、线缆 E-Marker、Alternate Mode（DP/Thunderbolt）**。

3. **PD 3.1**

   * 引入 **EPR 档位**（28V, 36V, 48V）；
   * 最大功率 240W；
   * 更适用于笔记本、高性能显卡扩展坞、显示器。

---

## 6. **USB PD 的典型应用**

* **手机快充**（9V/12V/20V 快速充电）
* **笔记本充电**（USB-C 适配器取代圆孔电源）
* **显示器/扩展坞供电**（显示器反向给笔记本供电）
* **数据扩展**：通过 VDM 切换到 **DisplayPort Alt Mode**，实现视频输出

---

## 7. **总结**

USB PD 协议的本质是：
👉 **通过 CC 通道进行通信**，让 Source 和 Sink **协商合适的电压电流**，实现灵活、安全、高功率的供电，同时还能扩展数据/视频传输功能。

---

要不要我帮你画一个 **PD 协商的时序图（Source ↔ Sink 的报文交互过程）**？这样你能更直观地理解整个 PD 协议握手流程。

------
--------
------
It seems like I can’t do more advanced data analysis right now. Please try again later.

不过我可以先用文字和简化的“ASCII 时序图”来表示 **PD 协议的握手过程**，你能快速直观地理解：

```
Source (供电端)                           Sink (受电端)
      |                                         |
      |---[ Source_Capabilities ]-------------->|
      |                                         |
      |<--------------[ Request ]---------------|
      |                                         |
      |---[ Accept ]--------------------------->|
      |                                         |
      |---[ PS_RDY (电源准备就绪) ]------------->|
      |                                         |
      |========= 开始按照请求电压/电流供电 =======>|
```

流程说明：

1. **Source 广播能力**：比如它能提供 5V/3A，9V/2A，15V/3A，20V/5A。
2. **Sink 请求**：比如手机只请求 9V/2A。
3. **Source 确认**：Accept 表示同意这个请求。
4. **Source 切换电压并准备好**：PS\_RDY 表示电压已经切换到 9V，电源稳定。
5. **正式供电**：之后就进入稳定的 PD 模式。

要不要我给你画一个 **更详细的版本**（带上 GoodCRC、Soft Reset、Ping 等报文）？这样能看到更接近真实 PD 协议的完整时序。


---------
---------
--------


好 — 我把 **USB Power Delivery (PD)** 在 USB-C 上的 **通信流程（从物理层到协议层、时序与报文）** 用系统但易读的方式讲清楚，包含时序、报文结构、常见报文、异常/重置与实现要点。重要/可查的技术点我在段落末加了权威来源引用，方便你进一步查验或抓规范看细节。

# 1) 总体分层与通信介质（一句话概览）

PD 通信是在 USB-C 的 **CC 引脚** 上以 **Biphase-Mark Code (BMC)** 做半双工串行通信（约 300 kb/s）。协议可以分为：**物理层 (PHY)** → **协议层 (protocol / message framing)** → **策略层 / Policy Engine (决定要发什么报文、何时切换电源/角色)**。([STMicroelectronics][1])

# 2) 物理层细节（时序 / 编码 / SOP）

* 线：PD 报文通过 CC1 或 CC2（取决于插头朝向）单线发送，发送方在发送时把另一条 CC 保持为空闲（half-duplex）。PHY 一般是独立的 TCPC/PHY 芯片或 MCU 的 UCPD 模块去做基带。([usb.org][2])
* 编码：使用 **BMC (4b/5b + Biphase-Mark)**，先发一段 **preamble（同步位，规范通常是 64 位交替）** 以便接收端锁相，然后发 SOP、Header、DataObjects、CRC、EOP。速率约 **300 kbit/s ±10%**。([Embedded][3])
* SOP 类型（寻址）：报文用不同的 **SOP**\* 前缀来定位接收对象：

  * **SOP**：端口对端（DFP/UFP）
  * **SOP' / SOP''**：用于与电子标记（E-Marked）线缆内的 cable-plug 通信（例如获取 cable capabilities）。SOP’/SOP’’ 只在支持 E-Marker 的场景出现，并且需要 VCONN 等条件。([blog.teledynelecroy.com][4])

# 3) 报文格式（高层框架）

一个典型 PD 包（逻辑上）包含：
`Preamble (sync)` → `SOP (address/routing)` → `Message Header (16 bits)` → `N × 32-bit Data Objects (如果有)` → `CRC` → `EOP`。

* **Message Header** 里包含关键字段（举例：NumberOfDataObjects (bits14..12)、MessageID (bits11..9)、Port Power Role (bit8)、SpecRev (bits7..6)、MessageType (bits3..0) 等），用来识别报文类型 & 长度 & 消息 id。规范对这些位的定义、extended/chunked 消息规则等有严格检查。([Embedded][3])

# 4) 报文类型（Control vs Data / 常见报文）

* **Control Messages（控制报文）**：短且无额外数据，用于控制流程（如 `GoodCRC`, `Accept`, `Reject`, `Ping`, `PS_RDY`, `Soft_Reset`, `Hard_Reset` 等）。([blog.teledynelecroy.com][4])
* **Data Messages（数据报文）**：带 Data Objects 的报文，典型的有 `Source_Capabilities`（源能力）、`Request`（请求）、`Sink_Capabilities`、`Vendor Defined Message (VDM)`（用于 Alternate Modes / 厂商扩展）等。Source\_Capabilities 每个 PDO 是 32-bit。([Embedded][3])

# 5) 典型握手 / 协商时序（最核心的流程）

下面是最常见的 **电源协商流程**（简化、已省略每个报文之间都要有 GoodCRC 的 ACK）：

1. **Attach & CC 检测**：插入后通过 CC 电阻检测出对端是 Source / Sink / DRP，并判断线缆能力（是否 3A/5A）与是否有 E-Marker。（这决定是否需要 SOP' 之类的后续交互） 。([usb.org][2])
2. **Source → Source\_Capabilities**：Source 主动广播它支持的 PDO 列表（例如 5V/3A、9V/3A、15V/3A、20V/5A）。接收端先回复 **GoodCRC**（确认收到并 CRC 校验通过）。([Microchip][5])
3. **Sink → Request**：Sink 选择其中一个 PDO，用 `Request` 报文（含 RDO）请求特定电压/电流。接收方（Source）收到后先发 **GoodCRC**。([Microchip][5])
4. **Source → Accept**：如果 Source 同意该请求，会发 `Accept`（Control），接收者回 `GoodCRC`。([Embedded][3])
5. **Source 切换电压并发送 PS\_RDY**：Source 在硬件端把 VBUS 调整到目标电压、稳定后发送 `PS_RDY` 表示电源准备好了；Sink 检测到新 VBUS 历史并确认后开始按新电压工作。([Embedded][3])

> 注：在上面每一次“收到报文 → 立即发送 GoodCRC”是 PD 的要求之一（PHY/TCPC 通常会自动做 CRC 校验并生成 GoodCRC），GoodCRC 只是单包确认，不代表后续逻辑同意；`Accept`/`Reject` 才是协议层的“是否通过”。([onsemi][6])

# 6) Atomic Message Sequence (AMS)、MessageID、重发与冲突处理

* PD 报文被组织为 **AMS（Atomic Message Sequence）**：有些控制报文会启动一个 AMS（例如 Request 开始一次协商序列），在 AMS 期间必须保持报文序列的原子性与消息 ID 管理（message\_id 用于识别重复和重发）。规范对 AMS 的超时、错误回复（如在不对的时间收到某种消息时应返回 `Soft_Reset`）有严格要求（CTS 测试里很多用例）。([usb.org][7])

# 7) 错误处理 / Reset / 安全退回

* **GoodCRC**：对每个完整收包立即响应（确认 CRC 正确）。([onsemi][6])
* **Soft\_Reset**：当协议层检测到协议错误或不期望的消息时，发送 Soft\_Reset 恢复到已定义的状态机。([usb.org][7])
* **Hard\_Reset**：用于更严重的问题（例如 VBUS 需要回退到默认 5V），会强制切换回默认状态并要求双方重新协商。([usb.org][7])

# 8) Cable / E-Marker / SOP' 场景

* 如果使用带 **E-Marker** 的主动/超高速线缆，Port 可以与线缆内的 E-Marker 芯片通信（SOP' / SOP''），获得线缆的最大额定电流、支持的规范修订等。只有在满足 VCONN 或相应条件下才允许与 cable-plug 交互。([blog.teledynelecroy.com][4])

# 9) Alternate Modes / VDM（比如 DisplayPort）

* **VDM（Vendor Defined Message）** 用于协商 Alternate Mode（例如把 USB-C 端口切到 DisplayPort 模式）、Vendor 自定义命令等。VDM 有 Structured VDM 结构，用来发现 SVID / mode 等。交互是在 PD 报文之上完成（仍然使用 SOP\*、Message Header、VDOs）。([STMicroelectronics][8])

# 10) PD 版本与高功率（举例）

* PD 3.1 引入 **EPR（Extended Power Range）**，增加了 28V/36V/48V 等档位，把最大功率扩展到 **240W（48V\@5A）**，这会改变某些能力广播与电源切换规则。实现和兼容性需参考 PD3.1 的具体字段与测试要求。([usb.org][9])

# 11) 实装要点（工程角度）

* 一般做法：把 **PHY/TCPC 芯片**（如 FUSB302B、UPD301、ST 的 TCPC 等）放在最底层处理 BMC 解码、CRC 检查与 GoodCRC 自动生成，**TCPM（Host MCU）/Policy Engine** 负责解析 header、选择能力、决定是否 Accept/Reject、处理 VDM、做角色切换策略等。这样可以把严格的时序/电平工作交给专用芯片，策略层写在 MCU/固件上。([onsemi][6])

# 12) 常见抓包/排查点（实战小贴士）

* 抓 PD 信号用示波器看 CC1/CC2：先看 preamble → SOP K-codes → header → payload → CRC → EOP。示波器或专用 CC 解码器可以把 BMC 解成消息并显示 MessageType / MessageID / PDO。([Embedded][3])
* 如果看到 `Source_Capabilities` 之后 Sink 没发 `Request`：检查是否被定时器（tFirstSourceCap）触发的超时或 Policy 决策拒绝。查看是否有硬件限流、线缆不支持请求的电流等。([usb.org][7])

# 13) 简化的时序 ASCII（核心交互 — 含 GoodCRC 表示位置）

```
Source --> [Source_Capabilities] --> Sink
Sink  --> [GoodCRC]              --> Source
Sink  --> [Request]              --> Source
Source--> [GoodCRC]              --> Sink
Source--> [Accept]               --> Sink
Sink  --> [GoodCRC]              --> Source
Source--> [PS_RDY]               --> Sink
Sink  --> (reads new VBUS, 开始使用)
```

（注意：每个收到的包都会有一个 GoodCRC ack；Accept/Reject/PS\_RDY 才是协商决议。）

---

如果你想，我可以继续做下面任一件事（你挑一项，我马上给出结果）：

* 把上面的流程画成更精确的 **分层时序图**（包含 preamble/header/DataObjects/GoodCRC 的位置）——我可以画 ASCII 图或生成一张 PNG（需要你允许我生成图像）。
* 给你 **抓包解析示例**（带真实的 PD 报文字段解析与如何在示波器/逻辑分析仪上解码）；我可以提供用 FUSB302 / ST UCPD 的抓包示例与解析脚本思路。
* 给你精确的 **Message Header / RDO / PDO 位字段说明表**（带位域位置与解释），便于你做固件解析或构造报文。

你要哪一种？或者直接告诉我：想看某个报文的位级解析（比如 Request 的 RDO）？

[1]: https://www.st.com/resource/en/application_note/an5225-introduction-to-usb-typec-power-delivery-for-stm32-mcus-and-mpus-stmicroelectronics.pdf?utm_source=chatgpt.com "AN5225 - Introduction to USB Type-C® Power Delivery for ..."
[2]: https://www.usb.org/sites/default/files/documents/usb-port_controller_specification_rev2.0_v1.0_0.pdf?utm_source=chatgpt.com "USB Type-C Port Controller Interface Specification"
[3]: https://www.embedded.com/usb-type-c-and-power-delivery-101-power-delivery-protocol/?utm_source=chatgpt.com "USB Type-C and power delivery 101 – Power delivery protocol"
[4]: https://blog.teledynelecroy.com/2016/05/usb-type-c-and-power-delivery-messaging.html?utm_source=chatgpt.com "USB Type-C and Power Delivery Messaging"
[5]: https://ww1.microchip.com/downloads/aemDocuments/documents/OTH/ApplicationNotes/ApplicationNotes/00001974A.pdf?utm_source=chatgpt.com "AN1974- Introduction to USB Power Delivery ..."
[6]: https://www.onsemi.com/download/data-sheet/pdf/fusb302b-d.pdf?utm_source=chatgpt.com "FUSB302B Programmable USB Type‐C Controller w/PD"
[7]: https://www.usb.org/sites/default/files/USB%20PD3%20CTS%20rev1%20v1.2%20RC20.pdf?utm_source=chatgpt.com "USB Power Delivery Compliance Test Specification"
[8]: https://www.st.com/resource/en/technical_article/ta0356-usb-typec-and-power-delivery-displayport-alternate-mode-stmicroelectronics.pdf?utm_source=chatgpt.com "USB Type-C™ and Power Delivery DisplayPort Alternate ..."
[9]: https://www.usb.org/sites/default/files/2021-05/USB%20PG%20USB%20PD%203.1%20DevUpdate%20Announcement_FINAL.pdf?utm_source=chatgpt.com "[PDF] USB Promoter Group Announces USB Power Delivery Specification ..."
