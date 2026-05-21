下面按**嵌入式开发视角**讲 SDIO：你可以把它理解成一种“高速、带命令体系的同步外设总线”，常用于 **Wi-Fi/蓝牙模块、SDIO 网卡、SD 卡、eMMC/SDMMC 类控制器**等。

---

# 1. SDIO 是什么？

**SDIO = Secure Digital Input Output**。
它和 SD 卡使用相似的物理接口，但 SDIO 不一定是存储卡，它可以是一个 I/O 外设，比如：

* Wi-Fi 模块：Realtek RTL8189/RTL8723、Broadcom、Marvell、ATWILC、ESP32 SDIO Slave
* 蓝牙/Wi-Fi Combo 模块
* GPS、摄像头、传感器扩展模块
* SD 卡插槽里的特殊 I/O 卡

核心区别是：

| 类型                      | 主要作用                  |
| ----------------------- | --------------------- |
| SD Memory Card          | 存储数据，读写扇区             |
| SDIO Card / SDIO Device | 外设通信，读写设备寄存器/FIFO     |
| Combo Card              | 同时有 SD 存储 + SDIO 外设功能 |

Linux 文档里也明确说，SDIO 设备通过 CMD 命令和 Response 响应交互，CMD3、CMD5、CMD7 用于初始化，CMD52、CMD53 用于访问设备；CMD52/CMD53 可以访问最多 8 个 Function，其中 Function 0 是标准公共区域，Function 1~7 通常由设备厂商定义。([android.googlesource.com][1])

---

# 2. SDIO 的硬件信号线

典型 SDIO 接口如下：

```text
Host / SoC / MCU                         SDIO Device
┌────────────────┐                     ┌────────────────┐
│ SDIO/SDMMC Host│                     │ Wi-Fi/SDIO模块  │
│                │ CLK  -------------> │ CLK            │
│                │ CMD  <------------> │ CMD            │
│                │ DAT0 <------------> │ DAT0           │
│                │ DAT1 <------------> │ DAT1 / IRQ     │
│                │ DAT2 <------------> │ DAT2           │
│                │ DAT3 <------------> │ DAT3           │
│                │ VDD  -------------> │ VDD            │
│                │ GND  -------------  │ GND            │
└────────────────┘                     └────────────────┘
```

常见信号含义：

| 信号        | 方向            | 作用                                |
| --------- | ------------- | --------------------------------- |
| CLK       | Host → Device | 时钟，所有通信由 Host 提供时钟                |
| CMD       | 双向            | 命令和响应线                            |
| DAT0      | 双向            | 1-bit 模式的数据线                      |
| DAT0~DAT3 | 双向            | 4-bit 模式的数据线                      |
| DAT1      | 双向            | 在 SDIO 中常兼作 in-band interrupt 中断线 |
| VDD/GND   | 电源            | 常见 3.3V，也可能支持 1.8V UHS            |

注意：**SDIO 是 Host 主导的同步总线**。
设备不能像 UART 那样随便自己吐一串数据。设备最多通过 DAT1 拉中断告诉 Host：“我有事，你来读我”。真正的数据读写还是 Host 发 CMD52/CMD53 来完成。ESP-IDF 文档也说明，SDIO 中断是 level sensitive，设备会通过拉低 DAT1 让 Host 检测到中断，然后 Host 再读中断状态寄存器并清除中断。([Espressif Systems][2])

---

# 3. SDIO 通信整体模型

SDIO 有点像“更复杂、更高速的 I2C 寄存器访问”。

I2C 常见流程：

```text
主机发设备地址 -> 发寄存器地址 -> 读/写数据
```

SDIO 常见流程：

```text
Host 发 CMD52/CMD53 -> Device 回 Response -> 必要时通过 DAT0~DAT3 传数据
```

可以分成三层理解：

```text
应用层/厂商协议
    例如 Wi-Fi 驱动协议、固件下载协议、数据包 FIFO 协议

SDIO 命令层
    CMD0 / CMD3 / CMD5 / CMD7 / CMD52 / CMD53

物理总线层
    CLK / CMD / DAT0~DAT3 / CRC / Response / DMA
```

很关键的一点：**SDIO 标准只规定怎么访问 Function 1~7 的寄存器/FIFO，但不规定每个 Wi-Fi 芯片内部寄存器是什么意思**。比如 ESP 的文档就说，SDIO 规范只说明 Host 如何通过 CMD52/CMD53 访问 Function 1~7 的自定义区域，底层硬件实现和上层通信协议要由厂商自己定义。([Espressif Systems][2])

所以你调 SDIO Wi-Fi 模块时，光有 SDIO 协议还不够，还必须有：

```text
SDIO Host 控制器驱动
        +
SDIO 总线枚举/初始化
        +
具体 Wi-Fi 芯片厂商驱动
        +
固件加载/寄存器/FIFO协议
```

---

# 4. SDIO 的 Function 概念

SDIO 设备内部不是简单一个地址空间，而是分 Function。

```text
Function 0：公共控制区，标准定义
Function 1：厂商功能 1，例如 Wi-Fi 数据通道
Function 2：厂商功能 2，例如 BT 或额外通道
...
Function 7：厂商功能 7
```

Function 0 里有很多标准寄存器，比如：

| 区域                    | 作用                                         |
| --------------------- | ------------------------------------------ |
| CCCR                  | Card Common Control Registers，公共控制寄存器      |
| FBR                   | Function Basic Registers，每个 Function 的基础信息 |
| CIS                   | Card Information Structure，设备信息结构          |
| IOEx                  | 使能某个 Function                              |
| IORx                  | 查看某个 Function 是否 Ready                     |
| IENx                  | 中断使能                                       |
| INTx                  | 中断 pending                                 |
| Bus Interface Control | 设置 1-bit / 4-bit                           |
| Block Size            | 设置块大小                                      |

Linux 的 `sdio.h` 里也能看到这些 CCCR/FBR 相关定义，比如 `SDIO_CCCR_IOEx`、`SDIO_CCCR_IORx`、`SDIO_CCCR_IENx`、`SDIO_CCCR_IF`、`SDIO_BUS_WIDTH_4BIT`、`SDIO_FBR_BASE`、`SDIO_FBR_BLKSIZE` 等。([docs.huihoo.com][3])

---

# 5. SDIO 上电初始化流程

典型初始化流程大概是这样：

```text
1. 上电，Host 给低速 SDIO clock
2. CMD0：让卡进入 idle/reset 状态
3. CMD5：查询 SDIO 设备工作电压和 ready 状态
4. 重复 CMD5：直到设备 ready
5. CMD3：设备发布 RCA 相对地址
6. CMD7：Host 选中该设备
7. CMD52：设置总线宽度，例如 1-bit 切到 4-bit
8. CMD52：使能 Function 1
9. CMD52：使能 SDIO interrupt
10. CMD52/CMD53：设置 Function block size
11. 开始正常 CMD52/CMD53 数据通信
```

ESP-IDF 的 SDIO Slave 初始化示例里也给了类似流程：先 SDIO reset、CMD0 reset，然后 CMD5 查询 op condition 并轮询 ready，接着 CMD3 获取 RCA、CMD7 选卡，再用 CMD52 设置 4-bit、使能 Function1、使能 SDIO interrupt、设置 Function0/Function1 block size。([Espressif Systems][2])

简化成流程图：

```text
Power On
   │
   ▼
CMD0：进入 Idle
   │
   ▼
CMD5：查询 OCR / IO Function 数量 / Ready
   │
   ├── not ready：继续 CMD5 轮询
   │
   ▼
CMD3：获取 RCA
   │
   ▼
CMD7：选中设备
   │
   ▼
CMD52：配置 CCCR，例如 4-bit / enable function / interrupt
   │
   ▼
CMD52/CMD53：正常访问寄存器/FIFO
```

---

# 6. 常见 SDIO 命令

## 6.1 CMD0：GO_IDLE_STATE

作用：复位/初始化 SD 总线设备，让设备进入 idle 状态。

对于 SDIO 设备，CMD0 通常出现在初始化早期。

---

## 6.2 CMD5：IO_SEND_OP_COND

这是 SDIO 很重要的命令。
SD Memory Card 通常使用 ACMD41 初始化，而 SDIO 使用 **CMD5**。

CMD5 作用：

```text
Host：你支持什么电压？你 ready 了吗？你有几个 I/O Function？
Device：通过 R4 Response 回复 OCR、Ready bit、Function 数量等信息
```

Infineon 的 SDIO 平台文档也说明，CMD5 用于查询 SDIO card 的 I/O 电压范围，正常响应是 R4；R4 会包含 OCR、Memory Present、I/O Function 数量等信息。([英飞凌][4])

CMD5 典型逻辑：

```c
send CMD5 arg = 0x00000000;     // 先查询设备支持什么
read R4;

send CMD5 arg = supported_ocr;  // 选择合适电压范围
while (!(R4 & READY_BIT)) {
    send CMD5 again;
}
```

---

## 6.3 CMD3：SEND_RELATIVE_ADDR

作用：让设备发布 RCA，Relative Card Address，相对卡地址。

后面 CMD7 选中设备时要用这个 RCA。

---

## 6.4 CMD7：SELECT/DESELECT_CARD

作用：选中某个 RCA 对应的设备。

选中之后，Host 才能对它做 Function 访问。

---

## 6.5 CMD52：IO_RW_DIRECT

CMD52 是 SDIO 最核心命令之一。
作用：**读/写 1 个字节的 SDIO 寄存器**。

适合用来：

* 读 CCCR/FBR 寄存器
* 使能 Function
* 设置总线宽度
* 使能中断
* 读写状态寄存器
* 小量控制寄存器访问

Linux 头文件里 CMD52 参数格式如下：bit31 是读写标志，bit30~28 是 Function number，bit27 是 RAW flag，bit25~9 是 register address，bit7~0 是 data。([docs.huihoo.com][3])

CMD52 可以理解成：

```text
Host：我要访问 Function X 的地址 addr，读/写 1 字节
Device：返回 R5 响应，里面带状态和数据
```

例如：

```text
CMD52 Write:
Host -> CMD52: function=0, addr=0x07, data=0x02
Device -> R5: OK

含义可能是：设置 Function0 CCCR 的 bus width = 4-bit
```

---

## 6.6 CMD53：IO_RW_EXTENDED

CMD53 是 SDIO 高速数据传输命令。
作用：**一次读/写多个字节，或者多个 block**。

适合用来：

* Wi-Fi 数据包传输
* 批量读写 FIFO
* 下载固件
* 高速搬运数据
* 读写一段连续寄存器

Linux 头文件里 CMD53 参数格式如下：bit31 是读写标志，bit30~28 是 Function number，bit27 是 Block mode，bit26 是 Increment address，bit25~9 是 register address，bit8~0 是 byte/block count。([docs.huihoo.com][3])

CMD53 有几个关键配置：

| 字段                          | 作用                           |
| --------------------------- | ---------------------------- |
| R/W                         | 读还是写                         |
| Function Number             | 访问哪个 Function                |
| Block Mode                  | 0 = byte mode，1 = block mode |
| OP Code / Increment Address | 地址是否自动递增                     |
| Register Address            | 起始地址                         |
| Byte/Block Count            | 传多少字节或多少块                    |

### Byte Mode

```text
CMD53 byte mode
一次传 N 字节
适合几十字节、几百字节的小数据
```

### Block Mode

```text
CMD53 block mode
一次传 N 个 block
block size 例如 512 字节
适合大块数据，比如 Wi-Fi 包、固件
```

ESP-IDF 文档里也说明，其 SDIO Slave 协议基于 CMD52/CMD53，并提供 FIFO、共享寄存器、Host/Slave 中断等服务；开始通信前，Host 需要使能 I/O Function 1 并访问对应寄存器。([Espressif Systems][2])

---

# 7. CMD52 和 CMD53 的本质区别

| 对比项   | CMD52            | CMD53            |
| ----- | ---------------- | ---------------- |
| 访问大小  | 1 字节             | 多字节 / 多 block    |
| 数据走哪里 | CMD/Response 里完成 | DAT0~DAT3 数据线    |
| 适合场景  | 配置寄存器、状态寄存器      | 大数据、FIFO、Wi-Fi 包 |
| 响应类型  | R5               | R5 + 数据阶段        |
| 类似理解  | I2C 单寄存器读写       | DMA 批量搬运         |

可以简单记：

```text
CMD52 = 控制命令，小数据，寄存器级访问
CMD53 = 数据搬运，大数据，FIFO/Buffer 访问
```

---

# 8. 一次 CMD52 通信流程

比如 Host 要读 Function 1 的某个状态寄存器：

```text
Host                                 SDIO Device
 │                                        │
 │ -- CMD52 Read FN1 addr=0x100 -------> │
 │                                        │
 │ <--------------- R5 + data ----------- │
 │                                        │
```

特点：

* 没有额外 DAT 数据阶段
* 数据直接放在 R5 response 里
* 很适合读状态位

比如：

```text
读 Wi-Fi 模块状态寄存器：
CMD52 Read Function1, addr = STATUS_REG
返回：0x01 表示 ready
```

---

# 9. 一次 CMD53 读流程

比如 Host 要从 Wi-Fi 模块读一个数据包：

```text
Host                                 SDIO Wi-Fi
 │                                        │
 │ -- CMD53 Read FN1 FIFO addr,count --> │
 │                                        │
 │ <---------------- R5 ---------------- │
 │                                        │
 │ <========= DAT0~DAT3 数据 =========== │
 │                                        │
 │ <============ CRC16 ================= │
 │                                        │
```

软件视角：

```c
// 伪代码
sdio_claim_host(func);

len = read_packet_len_by_cmd52_or_cmd53();
sdio_readsb(func, rx_buf, FIFO_ADDR, len);  // 底层一般对应 CMD53

sdio_release_host(func);
```

硬件视角：

```text
1. SDIO Host 控制器发 CMD53
2. 设备返回 R5
3. 设备通过 DAT0~DAT3 输出数据
4. SDIO Host 控制器接收数据
5. 硬件校验 CRC16
6. DMA 搬到内存
7. 触发传输完成中断
8. 驱动处理 rx_buf
```

---

# 10. 一次 CMD53 写流程

比如 Host 要给 Wi-Fi 模块发送一个网络包：

```text
Host                                 SDIO Wi-Fi
 │                                        │
 │ -- CMD53 Write FN1 FIFO addr,count -> │
 │                                        │
 │ <---------------- R5 ---------------- │
 │                                        │
 │ ========= DAT0~DAT3 数据 ===========> │
 │                                        │
 │ ============ CRC16 =================> │
 │                                        │
 │ <----------- CRC Status ------------- │
 │                                        │
```

软件视角：

```c
sdio_claim_host(func);

sdio_writesb(func, FIFO_ADDR, tx_buf, len); // 底层通常对应 CMD53

sdio_release_host(func);
```

---

# 11. SDIO 中断机制

SDIO 设备如果有数据要 Host 读取，通常不会直接“主动发数据”。
它会这样做：

```text
1. SDIO Device 内部收到数据，例如 Wi-Fi 空口收到网络包
2. Device 设置内部 interrupt status
3. Device 拉低 DAT1，产生 SDIO in-band interrupt
4. Host SDIO 控制器检测到 DAT1 中断
5. Host 驱动进入中断处理
6. Host 用 CMD52/CMD53 读取 interrupt status
7. Host 判断中断来源
8. Host 用 CMD53 读取数据 FIFO
9. Host 清除 interrupt status
10. DAT1 恢复
```

ESP 文档里描述得很典型：Host 检测 DAT1 被拉低后，读取 INT_ST 判断中断源，再写 INT_CLR 清中断；如果所有中断源都被清掉或屏蔽，DAT1 线恢复 inactive。([Espressif Systems][2])

所以 SDIO 中断和 MCU 外部中断很像：

```text
DAT1 相当于一个 IRQ 线
但中断原因还要通过 SDIO 寄存器读出来
```

---

# 12. 以 Wi-Fi SDIO 模块为例的完整通信流程

假设你的 Linux 板子上接了一个 SDIO Wi-Fi 模块。

## 12.1 上电枚举

```text
Linux MMC/SDIO Host Driver
        │
        ▼
给 SDIO clock
        │
        ▼
CMD0 / CMD5 / CMD3 / CMD7
        │
        ▼
读 CCCR / FBR / CIS
        │
        ▼
识别 Vendor ID / Device ID
        │
        ▼
匹配具体 Wi-Fi 驱动
```

Linux 文档中也描述了类似模型：Linux SDIO Host Driver 负责初始化 SDIO 设备、读取识别信息、枚举 Function，并在已注册的 SDIO 驱动列表中查找匹配驱动；用户 SDIO 驱动通常不直接操作 CMD52/CMD53，而是使用 Linux SDIO Host Driver 提供的抽象接口。([android.googlesource.com][1])

---

## 12.2 驱动 probe

```text
sdio_driver.probe()
        │
        ▼
sdio_enable_func(func1)
        │
        ▼
sdio_set_block_size(func1, 512)
        │
        ▼
注册 SDIO IRQ handler
        │
        ▼
下载 Wi-Fi 固件
        │
        ▼
初始化芯片寄存器
        │
        ▼
创建 wlan0 网络接口
```

---

## 12.3 正常收包

```text
Wi-Fi 空口收到数据
        │
        ▼
Wi-Fi 芯片放入 SDIO FIFO
        │
        ▼
拉低 DAT1 产生 SDIO IRQ
        │
        ▼
Host 中断
        │
        ▼
驱动读取中断状态
        │
        ▼
CMD53 从 FIFO 读 packet
        │
        ▼
交给 Linux 网络协议栈
        │
        ▼
应用层 socket recv() 收到数据
```

---

## 12.4 正常发包

```text
应用层 send()
        │
        ▼
Linux TCP/IP 协议栈
        │
        ▼
Wi-Fi 驱动
        │
        ▼
封装成芯片要求的数据包格式
        │
        ▼
CMD53 写入 SDIO FIFO
        │
        ▼
Wi-Fi 芯片从 FIFO 取包
        │
        ▼
通过无线射频发出去
```

---

# 13. SDIO 和 SPI / I2C / UART 的区别

| 接口   | 主从关系      | 时钟    | 数据线          | 速度  | 典型用途           |
| ---- | --------- | ----- | ------------ | --- | -------------- |
| UART | 点对点       | 无同步时钟 | TX/RX        | 中低速 | 日志、串口通信        |
| I2C  | 主从        | SCL   | SDA          | 中低速 | 传感器、EEPROM     |
| SPI  | 主从        | SCLK  | MOSI/MISO    | 中高速 | Flash、屏幕、ADC   |
| SDIO | Host/Card | CLK   | CMD + DAT0~3 | 高速  | Wi-Fi、SD卡、IO模块 |

核心区别：

```text
UART：像两个人直接聊天
I2C：主机问寄存器，设备答
SPI：主机片选后高速移位
SDIO：主机发标准命令，设备回响应，数据阶段走 DAT 总线
```

SDIO 比 SPI 更复杂，但优势是：

* 支持标准枚举
* 支持 1-bit / 4-bit
* 支持中断
* 支持 block 传输
* Host 控制器常带 DMA
* Linux 有 MMC/SDIO 框架

---

# 14. SDIO 速度

SD 总线速度等级大概可以这样理解：

| 模式                  | 理论总线速度    |
| ------------------- | --------- |
| Default Speed       | 12.5 MB/s |
| High Speed          | 25 MB/s   |
| UHS-I SDR50 / DDR50 | 50 MB/s   |
| UHS-I SDR104        | 104 MB/s  |

SD Association 官方页面说明，Default Mode 是 12.5MB/s，High Speed Mode 是 25MB/s，UHS-I 可到 50MB/s 或 104MB/s 等级。([SD Association][5])

不过工程里实际速度会受很多因素影响：

```text
实际速度 = 总线频率
         + 1-bit/4-bit 模式
         + block size
         + DMA效率
         + Host控制器能力
         + PCB走线质量
         + 设备内部FIFO性能
         + 驱动实现
```

例如 SDIO Wi-Fi 模块，理论上 4-bit 50MHz 很快，但实际吞吐可能被驱动、固件、协议栈、Wi-Fi 空口速率限制。

---

# 15. SDIO Host 控制器在 MCU/SoC 里干什么？

你写代码时通常不会自己手动翻转 CLK/CMD/DAT。
MCU/SoC 里一般有 SDIO/SDMMC Host 控制器。

它负责：

```text
1. 产生 CLK
2. 发送 CMD 命令帧
3. 接收 Response
4. 控制 DAT0~DAT3 数据传输
5. 自动 CRC7 / CRC16 校验
6. 处理超时、CRC错误、非法响应
7. 支持 FIFO / DMA
8. 产生命令完成/数据完成/错误中断
```

所以裸机驱动大概长这样：

```c
sdio_host_init();                  // 初始化 SDIO Host 控制器
sdio_set_clock(400000);            // 初始低速
sdio_send_cmd(CMD0, 0);
sdio_send_cmd(CMD5, 0);
sdio_poll_cmd5_ready();
sdio_send_cmd(CMD3, 0);
sdio_send_cmd(CMD7, rca << 16);

sdio_cmd52_write(0, CCCR_IF, 0x02);    // 切 4-bit
sdio_set_bus_width(4);

sdio_cmd52_write(0, CCCR_IOEx, 0x02);  // enable function 1
sdio_wait_func1_ready();

sdio_set_block_size(1, 512);

sdio_cmd53_read(1, FIFO_ADDR, buf, len);
```

---

# 16. SDIO 协议里几个非常重要的寄存器

以 Function 0 的 CCCR 为例：

| 寄存器       | 常见作用                              |
| --------- | --------------------------------- |
| 0x00      | CCCR/SDIO version                 |
| 0x01      | SD spec version                   |
| 0x02      | IOEx，使能 Function                  |
| 0x03      | IORx，Function ready 状态            |
| 0x04      | IENx，中断使能                         |
| 0x05      | INTx，中断 pending                   |
| 0x06      | Abort / Reset                     |
| 0x07      | Bus Interface Control，1-bit/4-bit |
| 0x08      | Capability                        |
| 0x09~0x0B | Common CIS pointer                |
| 0x10~0x11 | Function 0 block size             |
| 0x13      | High Speed 控制                     |
| 0x14      | UHS 控制                            |

Linux `sdio.h` 中能看到这些地址定义：例如 IOEx 是 0x02，IORx 是 0x03，IENx 是 0x04，INTx 是 0x05，ABORT 是 0x06，Bus Interface Control 是 0x07，Function0 block size 是 0x10。([docs.huihoo.com][3])

---

# 17. 一个“寄存器访问”的具体例子

假设要把 SDIO 切换成 4-bit 模式。

Function 0 的 `CCCR_IF` 地址是 0x07。
4-bit 模式值通常是 0x02。

流程：

```text
Host 发送 CMD52 Write:
Function = 0
Address  = 0x07
Data     = 0x02

Device 返回 R5:
Status OK
```

伪代码：

```c
sdio_cmd52_write(0, 0x07, 0x02);
host_set_bus_width(4);
```

注意：
**不是只给设备写 0x02 就完事了，Host 控制器自己也要切到 4-bit 模式。**

否则会出现：

```text
设备以为是 4-bit
Host 还按 1-bit 收
结果通信直接乱掉
```

---

# 18. 一个“FIFO 数据传输”的具体例子

假设 Wi-Fi 模块 Function 1 有一个 RX FIFO，地址是 `0x10000`。

读取 512 字节：

```text
Host 发送 CMD53 Read:
Function = 1
Block Mode = 1
Increment = 0 或 1，取决于 FIFO/寄存器设计
Address = FIFO_ADDR
Count = 1 block
Block Size = 512
```

如果是 FIFO，很多芯片会要求：

```text
Increment address = 0
```

因为 FIFO 地址固定，连续读同一个地址就能吐出数据。

如果是普通内存/寄存器窗口，则可能要求：

```text
Increment address = 1
```

因为要读连续地址。

这点很关键：

| OP Code / Increment | 含义     | 适合         |
| ------------------- | ------ | ---------- |
| 0                   | 固定地址访问 | FIFO       |
| 1                   | 地址递增访问 | 连续寄存器/内存窗口 |

---

# 19. 为什么 SDIO 适合 Wi-Fi？

Wi-Fi 模块数据量大，I2C 不够，UART 也比较吃力，SPI 可以但很多 Linux 平台对 SDIO Wi-Fi 支持更成熟。

SDIO 适合 Wi-Fi 的原因：

```text
1. 速度比 UART/I2C 高
2. 支持 4-bit 并行数据
3. 支持中断通知
4. 支持 DMA
5. Linux 有 MMC/SDIO 子系统
6. Wi-Fi 模块厂商常提供 SDIO 驱动
```

典型 Wi-Fi SDIO 通信模型：

```text
控制命令：CMD52 读写寄存器
数据传输：CMD53 读写 FIFO
中断通知：DAT1 in-band interrupt
```

---

# 20. 常见调试问题

## 问题 1：CMD5 没响应

常见原因：

```text
1. 电源没起来
2. 时钟没给
3. CMD/DAT 上拉不对
4. 复位脚没释放
5. 设备不是 SDIO 模式
6. Host 引脚复用配置错
7. 电压不匹配，1.8V/3.3V 问题
```

---

## 问题 2：能 CMD5，但 CMD52/CMD53 失败

常见原因：

```text
1. CMD3/CMD7 没正确选卡
2. Function 没 enable
3. Function ready 没等到
4. block size 没设置
5. 4-bit Host 和 Device 配置不一致
6. 访问了错误 Function
7. 地址不对
```

---

## 问题 3：中断不来

常见原因：

```text
1. IENx 没使能
2. 设备内部 interrupt mask 没开
3. DAT1 没接对
4. 4-bit 模式下 DAT1 中断机制不兼容
5. Host 控制器没开 SDIO IRQ
6. 中断状态没清，导致电平一直异常
```

---

## 问题 4：小包能读，大包读失败

常见原因：

```text
1. block size 不匹配
2. DMA buffer 没对齐
3. cache 没 clean/invalidate
4. byte mode / block mode 配错
5. CMD53 count 超过 Host 控制器限制
6. FIFO 长度判断错
7. CRC error / timeout
```

---

## 问题 5：切 4-bit 后就挂

重点检查：

```text
1. DAT1/DAT2/DAT3 线有没有接
2. 设备端 CCCR_IF 是否设置成 4-bit
3. Host 控制器是否也设置成 4-bit
4. DAT 线是否有上拉
5. PCB 走线是否过长/干扰
6. 时钟是否太高，先降到低速试
```

---

# 21. 你可以这样记 SDIO

最简单的心智模型：

```text
SDIO = CMD 命令 + Response 响应 + DAT 数据通道
```

更工程一点：

```text
初始化靠 CMD0/CMD5/CMD3/CMD7
配置靠 CMD52
大数据靠 CMD53
中断靠 DAT1
高速靠 4-bit + DMA + block mode
```

再对应到 Wi-Fi 模块：

```text
CMD52：读写 Wi-Fi 芯片控制寄存器
CMD53：读写 Wi-Fi 数据 FIFO
DAT1：Wi-Fi 芯片通知 Host 有数据/事件
DMA：Host 把 SDIO 数据搬到内存
驱动：把 SDIO 数据包转换成网络包
```

---

# 22. 最后用一句话总结

**SDIO 的本质是：Host 通过标准 SDIO 命令枚举设备，然后用 CMD52 做寄存器级控制，用 CMD53 通过 DAT0~DAT3 做高速数据搬运，设备通过 DAT1 中断提醒 Host 来读写数据。**

[1]: https://android.googlesource.com/kernel/msm/%2B/android-msm-flo-3.4-kitkat-mr1/Documentation/csdio.txt "Documentation/csdio.txt - kernel/msm - Git at Google"
[2]: https://docs.espressif.com/projects/esp-idf/en/v4.3-rc/esp32/api-reference/protocols/esp_sdio_slave_protocol.html "Communication with ESP SDIO Slave - ESP32 -  — ESP-IDF Programming Guide v4.3-rc documentation"
[3]: https://docs.huihoo.com/doxygen/linux/kernel/3.7/include_2linux_2mmc_2sdio_8h_source.html "Linux Kernel: include/linux/mmc/sdio.h Source File"
[4]: https://www.infineon.com/dgdl/Infineon-SDIO_platform_support_guide-UserManual-v01_00-EN.pdf?fileId=8ac78c8c8c3de074018c8ba3a9973619&utm_source=chatgpt.com "SDIO platform support guide"
[5]: https://www.sdcard.org/developers/sd-standard-overview/bus-speed-default-speed-high-speed-uhs-sd-express/ "Bus Speed (Default Speed/High Speed/UHS/SD Express) - SD Association"


---------
---------
--------

## 1. 先说清楚：SDIO 这个词有两层意思

你平时在 MCU/SoC 手册里看到的 **SDIO / SDMMC 外设**，一般指：

> MCU 里面专门用来和 SD 卡、eMMC、SDIO WiFi 模块通信的硬件控制器。

而协议层面有两类设备：

| 名称                 | 作用              | 常见例子                       |
| ------------------ | --------------- | -------------------------- |
| **SD Memory Card** | 存储卡协议           | TF 卡、SD 卡                  |
| **SDIO Card**      | I/O 设备协议        | SDIO WiFi、SDIO 蓝牙、SDIO 摄像头 |
| **eMMC**           | 类似 SD/MMC 的板载存储 | 手机、Linux 板子上的 eMMC         |

你问“如何跟 SD card 通信”，重点就是 **SD Memory Card 通信流程**。

---

# 2. SD 卡通信硬件线

SD 卡有两种常见通信模式：

## 方式一：SPI 模式

这个比较简单，很多小 MCU 用这个。

| SD 卡引脚 | SPI 模式作用 |
| ------ | -------- |
| CLK    | SPI SCK  |
| CMD    | SPI MOSI |
| DAT0   | SPI MISO |
| DAT3   | SPI CS   |

优点：简单，普通 SPI 外设就能驱动。
缺点：速度低，只能 1bit 数据线，性能一般。

---

## 方式二：SDIO / SDMMC 原生模式

这是 STM32、GD32、ESP32、Linux SoC 常用的高速模式。

常见 1bit / 4bit 模式：

```text
Host / MCU / SoC                  SD Card
------------------------------------------------
SD_CLK      --------------------> CLK
SD_CMD      <-------------------> CMD
SD_D0       <-------------------> DAT0
SD_D1       <-------------------> DAT1
SD_D2       <-------------------> DAT2
SD_D3       <-------------------> DAT3
```

| 线         | 作用              |
| --------- | --------------- |
| CLK       | 主机提供时钟          |
| CMD       | 发送命令、接收响应       |
| DAT0      | 数据线 0，也可表示 busy |
| DAT1~DAT3 | 扩展数据线，4bit 模式使用 |
| DAT3      | 有时也用于卡检测/上拉检测   |

SD 卡通信本质是：

> 主机通过 CMD 线发命令，SD 卡通过 CMD 线回响应，真正的数据通过 DAT0~DAT3 传输。

---

# 3. SDIO 原生模式的通信结构

SD 卡通信不是像 UART 那样随便收发字节，而是 **命令 + 响应 + 数据块** 的结构。

大概是这样：

```text
主机发命令 CMD17
        ↓
SD 卡返回响应 R1
        ↓
SD 卡通过 DAT0~DAT3 返回 512 字节数据
        ↓
主机 SDIO 外设/DMA 接收完成
```

例如读取一个扇区：

```text
CMD17: 读单个 block
参数: block 地址
响应: R1
数据: 512 字节
```

写入一个扇区：

```text
CMD24: 写单个 block
参数: block 地址
响应: R1
数据: 主机发送 512 字节
然后 SD 卡内部写 Flash，DAT0 可能拉低表示 busy
```

---

# 4. SD 命令格式

SD 原生模式下，一个普通命令是 **48 bit**：

```text
| Start | Trans | CMD Index | Argument | CRC7 | End |
|  1bit | 1bit  |   6bit    |  32bit   | 7bit | 1bit |
```

比如：

```text
CMD17 读取单块
Argument = 要读取的地址
```

常见响应类型：

| 响应类型 | 说明                     |
| ---- | ---------------------- |
| R1   | 普通状态响应，最常见             |
| R1b  | 带 busy 的响应，比如擦除、写入后可能忙 |
| R2   | 136bit 长响应，CID/CSD 信息  |
| R3   | OCR 电压信息               |
| R6   | RCA 地址响应               |
| R7   | CMD8 电压检测响应            |

---

# 5. SD 卡上电初始化流程

这是重点。SD 卡上电后不能直接读写，必须先初始化。

典型流程如下：

```text
1. 上电，提供低速 SD_CLK，通常 <= 400 kHz
2. 发送至少 74 个时钟，让 SD 卡进入可通信状态
3. CMD0：复位卡，进入 Idle 状态
4. CMD8：检测卡版本、电压范围
5. 循环发送 ACMD41，等待卡初始化完成
6. CMD2：读取 CID
7. CMD3：获取 RCA 相对地址
8. CMD9：读取 CSD，获取容量等信息
9. CMD7：选中卡
10. ACMD6：切换 4bit 模式
11. 提高 SD_CLK 频率
12. 开始读写 block
```

更细一点：

## 第一步：低速时钟

初始化阶段 SD_CLK 不能太高，一般设置为：

```text
100 kHz ~ 400 kHz
```

等初始化完成后，才能提高到：

```text
25 MHz
50 MHz
更高速度模式
```

普通 SD High Speed 常见是 50 MHz。

---

## 第二步：CMD0 复位

```text
CMD0: GO_IDLE_STATE
Argument: 0
Response: 无响应
```

作用：

> 让 SD 卡进入 idle 状态。

类似你给外设发一个软件复位命令。

---

## 第三步：CMD8 检测卡版本

```text
CMD8: SEND_IF_COND
Argument: 0x000001AA
Response: R7
```

作用：

> 判断是不是 SD v2.0 及以上的卡，以及电压是否支持。

如果 CMD8 有正确响应，一般说明是新卡，比如 SDHC/SDXC。

---

## 第四步：ACMD41 等待卡准备好

ACMD 是 Application Command，需要先发 CMD55。

所以 ACMD41 实际发送流程是：

```text
CMD55
ACMD41
```

ACMD41 作用：

> 让 SD 卡开始初始化，并查询它是否 ready。

伪代码：

```c
do {
    send_cmd(CMD55, 0);
    resp = send_cmd(ACMD41, HCS_BIT);
} while (!(resp & READY_BIT));
```

这个过程可能要循环几十次，直到卡初始化完成。

---

## 第五步：CMD2 读取 CID

```text
CMD2: ALL_SEND_CID
Response: R2
```

CID 是卡的身份信息，比如：

```text
厂家 ID
产品名
序列号
生产日期
```

---

## 第六步：CMD3 获取 RCA

```text
CMD3: SEND_RELATIVE_ADDR
Response: R6
```

RCA 是 Relative Card Address，相对卡地址。

后面选中卡要用这个地址：

```text
RCA << 16
```

可以理解成：

> SD 总线上理论上可以有多个卡，RCA 就像这个卡的临时地址。

---

## 第七步：CMD9 读取 CSD

```text
CMD9: SEND_CSD
Argument: RCA << 16
Response: R2
```

CSD 里面有卡的容量、块大小、速度能力等信息。

驱动会根据 CSD 算出：

```text
总容量
block 数量
block 大小
支持的速度模式
```

---

## 第八步：CMD7 选中卡

```text
CMD7: SELECT_CARD
Argument: RCA << 16
Response: R1b
```

只有选中之后，才能正式读写数据。

---

## 第九步：ACMD6 切换 4bit 模式

默认是 1bit 模式，只用 DAT0。

要切换成 4bit：

```text
CMD55
ACMD6
Argument = 2
```

同时 MCU 的 SDIO 外设也要配置成 4bit 模式。

注意：

> 卡和主机必须两边都切到 4bit，否则通信会乱。

---

# 6. SD 卡读数据流程

## 单块读取：CMD17

```text
CMD17: READ_SINGLE_BLOCK
Argument: block 地址
Response: R1
Data: 512 字节
```

流程：

```text
主机发送 CMD17
        ↓
SD 卡返回 R1 响应
        ↓
SD 卡通过 DAT 线发送 512 字节数据
        ↓
SDIO 外设接收
        ↓
DMA 搬运到内存 buffer
        ↓
触发传输完成中断
```

类似这样：

```c
uint8_t buf[512];

sd_send_cmd(CMD17, sector);
sd_wait_response(R1);

sdio_start_read(buf, 512);
dma_wait_done();
```

这里的 `sector` 通常就是扇区号。

---

## 多块读取：CMD18

```text
CMD18: READ_MULTIPLE_BLOCK
Argument: 起始 block 地址
Response: R1
Data: 连续多个 512 字节 block
```

停止多块读取需要：

```text
CMD12: STOP_TRANSMISSION
```

流程：

```text
CMD18 读多个 block
DAT 线连续返回 block0
DAT 线连续返回 block1
DAT 线连续返回 block2
...
CMD12 停止
```

多块读写比单块效率高。

---

# 7. SD 卡写数据流程

## 单块写入：CMD24

```text
CMD24: WRITE_BLOCK
Argument: block 地址
Response: R1
Data: 主机发送 512 字节
```

流程：

```text
主机发送 CMD24
        ↓
SD 卡返回 R1
        ↓
主机通过 DAT 线发送 512 字节
        ↓
SD 卡接收数据
        ↓
SD 卡内部写 Flash
        ↓
DAT0 可能拉低表示 busy
        ↓
写完成后 DAT0 释放
```

注意：

> 写入 SD 卡不是主机发完 512 字节就立刻结束，SD 卡内部还要把数据写入 Flash，这个时候会 busy。

---

## 多块写入：CMD25

```text
CMD25: WRITE_MULTIPLE_BLOCK
Argument: 起始 block 地址
Response: R1
Data: 连续写多个 block
```

结束时发送：

```text
CMD12
```

或者按照控制器/协议模式发送停止 token。

多块写入之前常用：

```text
ACMD23: SET_WR_BLK_ERASE_COUNT
```

告诉 SD 卡接下来要写多少块，这样 SD 卡可以提前擦除，提高写入速度。

---

# 8. 地址问题：SDSC 和 SDHC/SDXC 不一样

这个很重要，很多驱动 bug 就出在这里。

| 卡类型         | 地址单位                   |
| ----------- | ---------------------- |
| SDSC 老卡     | 字节地址                   |
| SDHC / SDXC | block 地址，也就是 512 字节扇区号 |

例如你要读第 100 个扇区：

对于 SDHC/SDXC：

```c
argument = 100;
```

对于老 SDSC：

```c
argument = 100 * 512;
```

现在常见 TF 卡基本都是 SDHC/SDXC，所以大部分时候直接用扇区号。

---

# 9. SDIO 外设在 MCU 里面负责什么？

以 STM32/GD32/ESP32 这类为例，SDIO/SDMMC 外设负责：

```text
1. 产生 SD_CLK
2. 发送 CMD 命令
3. 接收 response
4. 控制 DAT0~DAT3 数据收发
5. 校验 CRC
6. 检测超时
7. 配合 DMA 搬运数据
8. 产生中断
```

驱动分层大概是：

```text
应用层
  ↓
FATFS 文件系统
  ↓
diskio.c
  ↓
SD 卡 block read/write 驱动
  ↓
SDIO/SDMMC 控制器驱动
  ↓
SD 卡硬件
```

你在应用层调用：

```c
f_open()
f_read()
f_write()
```

底层可能会变成：

```c
disk_read()
    ↓
sd_read_blocks()
    ↓
CMD17 / CMD18
```

写文件时：

```c
f_write()
    ↓
disk_write()
    ↓
sd_write_blocks()
    ↓
CMD24 / CMD25
```

---

# 10. SDIO + DMA 是怎么配合的？

SD 卡一个 block 通常是 512 字节，如果靠 CPU 一个字节一个字节搬，效率很低。

所以一般用 DMA：

## 读数据

```text
SD 卡 DAT 线收到数据
        ↓
SDIO FIFO
        ↓
DMA
        ↓
内存 buffer
```

## 写数据

```text
内存 buffer
        ↓
DMA
        ↓
SDIO FIFO
        ↓
DAT 线
        ↓
SD 卡
```

SDIO 控制器内部通常有 FIFO，比如：

```text
SDIO FIFO: 32 word / 64 word / 更大
```

DMA 的作用就是避免 FIFO 空/满导致丢数据。

---

# 11. 一次完整读扇区的底层流程

假设读取 SD 卡第 1000 个扇区：

```text
应用层：
    f_read()

文件系统：
    发现要读 sector 1000

块设备层：
    disk_read(1000, buf, 1)

SD 驱动：
    sd_read_block(1000, buf)

SDIO 协议：
    CMD17 arg=1000

SD 卡：
    返回 R1
    发送 512 字节数据

MCU：
    SDIO 接收数据
    DMA 搬运到 buf
    产生完成中断

文件系统：
    解析 FAT/exFAT 目录、文件内容

应用层：
    得到文件数据
```

---

# 12. 速度怎么算？

SDIO 速度主要看：

```text
总线宽度 × 时钟频率
```

比如 4bit 模式，25 MHz：

```text
25 MHz × 4 bit = 100 Mbit/s
100 / 8 = 12.5 MB/s 理论速度
```

4bit 50 MHz：

```text
50 MHz × 4 bit = 200 Mbit/s
200 / 8 = 25 MB/s 理论速度
```

实际速度会低一些，因为有：

```text
命令开销
响应等待
CRC
SD 卡内部擦写延迟
文件系统开销
DMA/Cache 开销
```

---

# 13. 常见 SD 命令表

| 命令     | 名称                     | 作用              |
| ------ | ---------------------- | --------------- |
| CMD0   | GO_IDLE_STATE          | 复位卡             |
| CMD8   | SEND_IF_COND           | 检测电压和版本         |
| CMD55  | APP_CMD                | 表示下一个是 ACMD     |
| ACMD41 | SD_SEND_OP_COND        | 初始化卡            |
| CMD2   | ALL_SEND_CID           | 读取 CID          |
| CMD3   | SEND_RELATIVE_ADDR     | 获取 RCA          |
| CMD7   | SELECT_CARD            | 选中卡             |
| CMD9   | SEND_CSD               | 读取 CSD          |
| CMD13  | SEND_STATUS            | 读取卡状态           |
| ACMD6  | SET_BUS_WIDTH          | 设置 4bit 模式      |
| CMD16  | SET_BLOCKLEN           | 设置 block 长度，老卡用 |
| CMD17  | READ_SINGLE_BLOCK      | 读单块             |
| CMD18  | READ_MULTIPLE_BLOCK    | 读多块             |
| CMD24  | WRITE_BLOCK            | 写单块             |
| CMD25  | WRITE_MULTIPLE_BLOCK   | 写多块             |
| CMD12  | STOP_TRANSMISSION      | 停止多块传输          |
| ACMD23 | SET_WR_BLK_ERASE_COUNT | 多块写入前预擦除        |

---

# 14. SDIO 通信中的几个关键状态

SD 卡内部有状态机，大概可以理解成：

```text
上电
 ↓
Idle
 ↓
Ready
 ↓
Identification
 ↓
Standby
 ↓
Transfer
 ↓
Sending-data / Receive-data / Programming
```

正常读写必须在：

```text
Transfer 状态
```

写入过程中可能进入：

```text
Programming 状态
```

这时卡可能 busy，不能立刻进行下一次写操作。

---

# 15. SDIO 和普通 SPI 的本质区别

| 对比       | SPI 模式        | SDIO/SDMMC 模式     |
| -------- | ------------- | ----------------- |
| 数据线      | 1bit          | 1bit / 4bit       |
| 速度       | 较低            | 高                 |
| 驱动复杂度    | 简单            | 复杂                |
| 是否需要专用外设 | 不需要，普通 SPI 即可 | 需要 SDIO/SDMMC 控制器 |
| DMA 支持   | 可以有           | 基本都会配 DMA         |
| 文件系统支持   | 可以接 FATFS     | 可以接 FATFS         |
| 初始化流程    | SPI 版 SD 命令   | 原生 SD 命令          |

小 MCU 上常用 SPI 模式，因为简单。
性能要求高的项目，比如音频播放、Linux 存储、摄像头录像，一般用 SDIO/SDMMC 4bit 模式。

---

# 16. 嵌入式软件开发里你真正要写什么？

一般分几层。

## 最底层：SDIO 控制器驱动

负责：

```c
sdio_set_clock();
sdio_send_cmd();
sdio_get_response();
sdio_read_data_dma();
sdio_write_data_dma();
sdio_check_crc();
sdio_check_timeout();
```

---

## 中间层：SD 卡协议驱动

负责：

```c
sd_card_init();
sd_card_read_blocks();
sd_card_write_blocks();
sd_card_get_capacity();
sd_card_get_status();
```

里面会封装：

```text
CMD0
CMD8
ACMD41
CMD2
CMD3
CMD7
CMD17
CMD24
...
```

---

## 文件系统适配层

如果用 FATFS，一般要实现：

```c
DSTATUS disk_initialize(BYTE pdrv);
DRESULT disk_read(BYTE pdrv, BYTE *buff, LBA_t sector, UINT count);
DRESULT disk_write(BYTE pdrv, const BYTE *buff, LBA_t sector, UINT count);
DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff);
```

FATFS 不关心你底层是 SPI 还是 SDIO，它只需要你能按 sector 读写。

---

# 17. 一个简化版 SD 卡初始化伪代码

```c
int sd_card_init(void)
{
    /* 1. 初始化 SDIO GPIO、时钟、DMA */
    sdio_gpio_init();
    sdio_dma_init();

    /* 2. 初始化阶段使用低速时钟，不能一开始就跑高速 */
    sdio_set_clock(400000);

    /* 3. 给 SD 卡提供一段空时钟 */
    sdio_send_dummy_clock();

    /* 4. CMD0，让卡进入 idle */
    sdio_send_cmd(CMD0, 0, RESP_NONE);

    /* 5. CMD8，检测 SD v2.0 和电压 */
    if (sdio_send_cmd(CMD8, 0x1AA, RESP_R7) == OK) {
        card.version = SD_VERSION_2;
    } else {
        card.version = SD_VERSION_1_OR_MMC;
    }

    /* 6. ACMD41，等待卡初始化完成 */
    do {
        sdio_send_cmd(CMD55, 0, RESP_R1);
        resp = sdio_send_cmd(ACMD41, HCS_BIT, RESP_R3);
    } while (!(resp & CARD_READY_BIT));

    /* 7. 判断是不是高容量卡 */
    if (resp & CCS_BIT) {
        card.type = SDHC_OR_SDXC;
    } else {
        card.type = SDSC;
    }

    /* 8. 读取 CID */
    sdio_send_cmd(CMD2, 0, RESP_R2);

    /* 9. 获取 RCA */
    resp = sdio_send_cmd(CMD3, 0, RESP_R6);
    card.rca = parse_rca(resp);

    /* 10. 读取 CSD */
    sdio_send_cmd(CMD9, card.rca << 16, RESP_R2);

    /* 11. 选中卡 */
    sdio_send_cmd(CMD7, card.rca << 16, RESP_R1B);

    /* 12. 设置 4bit 模式 */
    sdio_send_cmd(CMD55, card.rca << 16, RESP_R1);
    sdio_send_cmd(ACMD6, 2, RESP_R1);
    sdio_set_bus_width(4);

    /* 13. 初始化完成后提高时钟 */
    sdio_set_clock(25000000);

    return 0;
}
```

---

# 18. 常见问题和坑

## 坑 1：初始化阶段时钟太快

初始化阶段必须低速。

错误：

```text
一上来 SD_CLK = 25 MHz
```

可能导致 CMD8、ACMD41 超时。

---

## 坑 2：4bit 模式只配置了 MCU，没有配置 SD 卡

必须两边都配置：

```text
SD 卡：ACMD6 设置 4bit
MCU：SDIO 外设设置 4bit
```

否则数据线解释不一致。

---

## 坑 3：SDHC 地址没有乘 512

对于 SDHC/SDXC：

```c
cmd_arg = sector;
```

对于 SDSC：

```c
cmd_arg = sector * 512;
```

如果搞错，会读错位置。

---

## 坑 4：写完没有等 busy 结束

写入后 SD 卡内部要编程 Flash。

如果不等 busy 结束，下一条命令可能失败。

---

## 坑 5：Cache 问题

在带 DCache 的 MCU/SoC 上，SDIO + DMA 要特别注意 cache 一致性。

例如 Cortex-M7、ESP32-S3、Linux SoC 场景中，可能需要：

```text
DMA 发送前：clean cache
DMA 接收后：invalidate cache
buffer 地址对齐
buffer 放到 DMA 可访问内存
```

否则会出现：

```text
读出来的数据不对
写进去的数据是旧的
偶发错误
```

---

# 19. 最后用一句话总结

SDIO 跟 SD 卡通信的核心就是：

> MCU 通过 CMD 线发送标准 SD 命令，SD 卡通过 CMD 线返回响应，真正的数据通过 DAT0~DAT3 以 block 为单位传输；初始化时先低速识别卡、分配 RCA、选中卡、配置总线宽度，然后用 CMD17/CMD18 读块，用 CMD24/CMD25 写块，上层再接 FATFS 或 Linux block 设备。

你可以把它类比成：

```text
SDIO 控制器 = 专用高速搬运硬件
SD 协议命令 = 控制指令
DAT 数据线 = 真正搬数据的高速通道
DMA = 从 SDIO FIFO 到内存的搬运工
FATFS/Linux 文件系统 = 把 block 读写封装成文件读写
```
