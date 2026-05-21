可以把它们先分成两组来看：

```text
BLE：
GAP 负责“发现设备、广播、扫描、建立连接”
GATT 负责“连接后，数据怎么组织、怎么读写”

TCP/UDP：
IP 负责“找到对方”
TCP/UDP 负责“端到端传输数据”
应用协议负责“数据内容怎么解释”
```

所以：

```text
BLE 里的 GAP ≈ 网络里的“发现/连接管理”
BLE 里的 GATT ≈ 应用层数据模型
TCP/UDP ≈ 传输层通信方式
```

它们不是完全一一对应，但可以类比理解。

---

# 1. 最核心区别

## TCP/UDP 是“网络通信模型”

TCP/UDP 面向的是：

```text
IP地址 + 端口号
```

例如：

```text
192.168.1.10:8080
```

通信双方通常叫：

```text
TCP Server / TCP Client
UDP Server / UDP Client
```

---

## BLE 是“近距离无线设备通信模型”

BLE 面向的是：

```text
蓝牙地址 + 广播 + 连接 + 服务 + 特征值
```

例如一个 BLE 温湿度传感器可能长这样：

```text
设备名：TempSensor
Service UUID：环境传感器服务
Characteristic UUID：温度特征值
Characteristic UUID：湿度特征值
```

BLE 里面常见角色有两套：

```text
GAP 角色：
Central / Peripheral

GATT 角色：
Client / Server
```

这点非常重要。

---

# 2. BLE 里面 GAP 和 GATT 的分工

## GAP：负责“找设备、连设备”

GAP 管这些事情：

```text
广播 Advertising
扫描 Scanning
连接 Connection
断开连接 Disconnect
设备名称
广播数据
连接参数
配对绑定
```

典型流程：

```text
BLE 外设 Peripheral：不断广播
BLE 中心 Central：扫描附近设备
Central 找到目标设备后发起连接
连接成功后，进入 GATT 数据交互
```

比如：

```text
手环 Peripheral：广播“我是手环”
手机 Central：扫描到手环
手机 Central：连接手环
连接后手机通过 GATT 读取心率、电量、步数
```

---

## GATT：负责“连接后怎么读写数据”

GATT 规定数据按这种结构组织：

```text
GATT Server
└── Service 服务
    ├── Characteristic 特征值
    │   ├── Value 数据值
    │   └── Descriptor 描述符
    └── Characteristic 特征值
        └── Value 数据值
```

例如：

```text
环境传感器 Service
├── 温度 Characteristic
│   └── Value: 26.5℃
└── 湿度 Characteristic
    └── Value: 60%
```

GATT 常用操作：

```text
Read：读特征值
Write：写特征值
Notify：服务端主动通知客户端，不需要确认
Indicate：服务端主动通知客户端，需要确认
```

---

# 3. TCP 客户端/服务端工作流程

TCP 是**面向连接、可靠、字节流**通信。

典型 TCP Server 流程：

```c
socket()
bind()
listen()
accept()
recv()
send()
close()
```

典型 TCP Client 流程：

```c
socket()
connect()
send()
recv()
close()
```

画成图：

```text
TCP Server                           TCP Client

socket()
bind()
listen()
accept()   <────── connect() ─────── socket()

recv()     <────── send()
send()     ──────> recv()

close()    <────── close()
```

TCP 的特点是：

```text
要先建立连接
数据可靠传输
保证顺序
丢包会重传
没有消息边界，只有字节流
可能有 TCP 粘包/拆包问题
```

比如你发：

```text
send("hello")
send("world")
```

对方可能一次收到：

```text
"helloworld"
```

也可能分几次收到：

```text
"he"
"llowor"
"ld"
```

所以 TCP 应用层经常需要自己设计协议帧：

```text
帧头 + 长度 + 数据 + 校验
```

---

# 4. UDP 客户端/服务端工作流程

UDP 是**无连接、不保证可靠、按数据报发送**通信。

UDP Server 通常：

```c
socket()
bind()
recvfrom()
sendto()
close()
```

UDP Client 通常：

```c
socket()
sendto()
recvfrom()
close()
```

画成图：

```text
UDP Server                           UDP Client

socket()
bind()

recvfrom() <────── sendto()
sendto()   ──────> recvfrom()

close()             close()
```

UDP 的特点是：

```text
不用建立连接
速度快
开销小
不保证送达
不保证顺序
可能丢包
保留数据报边界
支持广播/组播
```

比如你 `sendto()` 一包 20 字节，对方 `recvfrom()` 通常就是收这一包，不像 TCP 那样变成字节流粘在一起。

---

# 5. BLE GAP + GATT 工作流程

以一个 BLE 温湿度传感器为例。

## BLE Peripheral / GATT Server 流程

一般是设备端，比如 ESP32、手环、传感器。

```text
初始化 BLE 协议栈
设置设备名
创建 GATT Service
创建 Characteristic
设置广播数据
开始广播 Advertising
等待手机扫描
手机连接
手机发现服务
手机读/写特征值
设备通过 Notify 上报数据
断开后重新广播
```

画成图：

```text
BLE Peripheral / GATT Server

初始化 BLE
创建 Service / Characteristic
设置广播数据
开始广播
等待 Central 连接
连接成功
等待 GATT Read / Write
发送 Notify / Indicate
断开连接
重新广播
```

---

## BLE Central / GATT Client 流程

一般是手机、网关、主控设备。

```text
初始化 BLE
开始扫描
找到目标广播设备
发起连接
连接成功
发现服务 Service Discovery
发现特征值 Characteristic Discovery
读取 Characteristic
写入 Characteristic
订阅 Notify
接收 Notify 数据
断开连接
```

画成图：

```text
BLE Central / GATT Client

初始化 BLE
扫描设备
找到目标设备
连接 Peripheral
发现服务
发现特征值
Read / Write
订阅 Notify
接收 Notify
断开连接
```

---

# 6. BLE 和 TCP 的流程类比

可以这样类比，但不要完全等同。

| 网络 TCP                   | BLE                                |
| ------------------------ | ---------------------------------- |
| Server 开端口等待连接           | Peripheral 广播等待连接                  |
| Client 知道 IP/端口后 connect | Central 扫描到设备后 connect             |
| TCP 连接建立                 | BLE 连接建立                           |
| send/recv 传字节流           | Read/Write/Notify 传属性数据            |
| IP 地址                    | 蓝牙地址                               |
| 端口号                      | Service UUID / Characteristic UUID |
| 应用协议自定义数据格式              | GATT Service/Characteristic 数据模型   |
| close 断开连接               | GAP disconnect 断开连接                |

一个比较形象的类比：

```text
TCP Server：
    我在 8080 端口等你连接

BLE Peripheral：
    我在广播里告诉你，我这里有某些服务，你可以来连接我
```

```text
TCP Client：
    我连接 192.168.1.10:8080

BLE Central：
    我扫描到 TempSensor，然后连接它
```

```text
TCP send/recv：
    直接收发字节流

BLE GATT：
    对某个 Characteristic 进行 Read / Write / Notify
```

---

# 7. BLE 和 UDP 的流程类比

BLE 广播有一点像 UDP 广播，但也不是完全一样。

## UDP 广播

```text
设备向局域网广播：
255.255.255.255:端口
```

特点：

```text
不需要连接
一发多收
适合发现设备
适合局域网广播消息
```

---

## BLE 广播 Advertising

```text
Peripheral 周期性发广播包
Central 扫描广播包
```

特点：

```text
不需要连接
广播包很短
适合设备发现
可以带设备名、服务 UUID、厂商数据
```

所以可以粗略类比：

```text
BLE Advertising ≈ UDP 广播/设备发现
BLE Connection + GATT ≈ TCP 连接后的数据交互
```

但是区别是：

```text
BLE 广播数据量很小
BLE 广播不是 IP 网络
BLE 广播由 Link Layer 管理
UDP 广播是在 IP 网络里的传输层数据报
```

---

# 8. BLE 角色和 TCP/UDP 角色的区别

这是新手最容易混淆的地方。

## TCP 角色简单

TCP 里一般就是：

```text
Server：监听端口，等待连接
Client：主动连接 Server
```

角色和连接方向基本绑定。

---

## BLE 有两套角色

BLE 里面有：

```text
GAP 角色：
Central / Peripheral

GATT 角色：
Client / Server
```

常见组合是：

```text
手机：Central + GATT Client
ESP32传感器：Peripheral + GATT Server
```

但这不是绝对的。

理论上也可以：

```text
手机：Central + GATT Server
设备：Peripheral + GATT Client
```

不过实际产品里最常见的是：

```text
手机/网关 = Central + GATT Client
传感器/手环/外设 = Peripheral + GATT Server
```

---

# 9. TCP/UDP 传的是“字节流/数据报”，BLE GATT 传的是“属性”

## TCP 数据模型

TCP 只管字节流：

```text
0x01 0x02 0x03 0x04 ...
```

它不关心你这是什么：

```text
温度？
湿度？
JSON？
图片？
音频？
自定义帧？
```

应用层自己解释。

---

## UDP 数据模型

UDP 只管一包一包的数据报：

```text
Packet 1: 20 bytes
Packet 2: 15 bytes
Packet 3: 60 bytes
```

应用层自己解释。

---

## BLE GATT 数据模型

BLE GATT 先帮你规定数据放在哪：

```text
Service UUID
Characteristic UUID
Value
Permission
Descriptor
```

例如：

```text
Service UUID: 0x180F    电池服务
Characteristic UUID: 0x2A19    电池电量
Value: 85%
```

所以 BLE GATT 比 TCP/UDP 更“结构化”。

---

# 10. 数据发送方式对比

| 通信方式            | 是否连接     | 发送方式               | 是否可靠       | 是否有消息边界   |
| --------------- | -------- | ------------------ | ---------- | --------- |
| TCP             | 要连接      | send/recv          | 可靠         | 无消息边界，字节流 |
| UDP             | 不需要连接    | sendto/recvfrom    | 不可靠        | 有数据报边界    |
| BLE GATT Read   | 要 BLE 连接 | Client 主动读         | 链路层可靠      | 一次读一个属性   |
| BLE GATT Write  | 要 BLE 连接 | Client 写 Server    | 看 Write 类型 | 一次写一个属性   |
| BLE Notify      | 要 BLE 连接 | Server 主动发给 Client | 无应用层确认     | 一次通知一个属性值 |
| BLE Indicate    | 要 BLE 连接 | Server 主动发给 Client | 有确认        | 一次指示一个属性值 |
| BLE Advertising | 不需要连接    | 广播包                | 不保证接收      | 一包一包广播    |

---

# 11. 连接建立流程对比

## TCP 连接流程

TCP 有三次握手：

```text
Client                         Server

SYN        ───────────────────>
           <──────────────────  SYN + ACK
ACK        ───────────────────>

连接建立成功
```

然后：

```text
send / recv
```

---

## BLE 连接流程

BLE 不是 TCP 三次握手。BLE 是基于广播和连接请求建立链路。

大概流程：

```text
Peripheral                     Central

Advertising  ────────────────>
Advertising  ────────────────>
              <──────────────  Scan Request 可选
Scan Response ───────────────>
              <──────────────  Connect Request

BLE 连接建立
```

连接建立后，BLE 会按连接间隔通信：

```text
Connection Event 1
Connection Event 2
Connection Event 3
...
```

也就是说 BLE 连接不是一直疯狂收发，而是在一个个连接事件里交换数据。这也是 BLE 省电的重要原因。

---

# 12. 连接后数据流程对比

## TCP

```text
Client                         Server

send("hello") ───────────────>
             <─────────────── recv("hello")

recv("ok")   <─────────────── send("ok")
```

TCP 双方都可以随时 send/recv。

---

## BLE GATT

以手机读 ESP32 温度为例：

```text
Central / GATT Client          Peripheral / GATT Server

Read 温度 Characteristic ─────>
                            查找温度值
          <────────────────── Read Response: 26.5℃
```

以 ESP32 主动上报温度为例：

```text
Central / GATT Client          Peripheral / GATT Server

写 CCCD 开启 Notify ─────────>
                            记录客户端订阅了 Notify

          <────────────────── Notify: 26.5℃
          <────────────────── Notify: 26.6℃
          <────────────────── Notify: 26.7℃
```

所以 BLE GATT 常见模式是：

```text
Client Read
Client Write
Server Notify
```

而 TCP 是：

```text
双方 send/recv
```

---

# 13. “服务端主动发数据”的区别

## TCP Server 主动发数据

TCP 连接建立后，Server 可以直接：

```c
send(client_sock, data, len, 0);
```

只要连接还在，双方都能发。

---

## BLE GATT Server 主动发数据

BLE GATT Server 不能随便“像 TCP send 一样发任意数据”。

它一般要通过：

```text
Notify
Indicate
```

而且通常需要 Client 先开启订阅：

```text
Client 写 CCCD = 开启 Notify
Server 才开始 Notify 数据
```

流程：

```text
手机连接 ESP32
手机发现温度 Characteristic
手机写 CCCD，开启 Notify
ESP32 才周期性 Notify 温度
```

这点和 TCP 很不一样。

---

# 14. BLE GATT 为什么不像 TCP 那样直接 send/recv？

因为 BLE GATT 是围绕“属性数据库”设计的。

它的核心不是：

```text
我给你发一串字节
```

而是：

```text
你来读/写我某个 Characteristic
或者我通过某个 Characteristic 通知你
```

所以 BLE GATT 更像：

```text
远程变量表
```

比如：

```text
温度变量
湿度变量
电池变量
LED控制变量
设备状态变量
```

手机可以读写这些变量，设备可以通知这些变量变化。

---

# 15. 类比成嵌入式寄存器会更好理解

你可以把 GATT Server 想象成一个“远程寄存器表”。

```text
GATT Server = 一张寄存器表

Service = 外设模块
Characteristic = 寄存器
Value = 寄存器值
Read = 读寄存器
Write = 写寄存器
Notify = 寄存器值变化中断通知
```

例如：

```text
电池服务 Service
└── 电量 Characteristic = 电量寄存器

LED 控制服务 Service
└── LED 开关 Characteristic = LED控制寄存器

传感器服务 Service
├── 温度 Characteristic = 温度寄存器
└── 湿度 Characteristic = 湿度寄存器
```

所以：

```text
TCP/UDP：更像直接发裸数据包
BLE GATT：更像远程读写寄存器/变量
```

这个理解对嵌入式很有用。

---

# 16. BLE 和 TCP/UDP 在实际项目里的流程对比

## 场景 1：ESP32 通过 TCP 上报温湿度

```text
ESP32 TCP Client                  服务器 TCP Server

连接 Wi-Fi
socket()
connect(server_ip, port) ───────>
send("{temp:26.5,hum:60}") ─────>
                              recv()
                              解析 JSON
```

特点：

```text
需要 Wi-Fi / IP 网络
可以发大量数据
适合云服务器通信
功耗较高
距离取决于 Wi-Fi
```

---

## 场景 2：ESP32 通过 UDP 上报温湿度

```text
ESP32 UDP Client                  UDP Server

连接 Wi-Fi
socket()
sendto(server_ip, port, data) ──>
                              recvfrom()
```

特点：

```text
不用建立连接
速度快
可能丢包
适合局域网广播发现、实时小数据
```

---

## 场景 3：ESP32 通过 BLE GATT 上报温湿度

```text
ESP32 BLE Peripheral/GATT Server      手机 Central/GATT Client

创建温湿度 Service
开始广播  ────────────────────────>
                                 扫描
                                 连接
                                 发现服务
                                 写 CCCD 开启 Notify
Notify 温度湿度 ──────────────────>
Notify 温度湿度 ──────────────────>
```

特点：

```text
不需要路由器
适合手机近距离连接
低功耗
数据量较小
适合传感器、配网、遥控器、手环
```

---

# 17. 什么时候 BLE 像 TCP？什么时候像 UDP？

## BLE 连接态 + GATT 通信，有点像 TCP

因为：

```text
需要连接
有连接状态
双方有会话
断开后需要重连
可以持续交互
```

但不同点是：

```text
TCP 是字节流
BLE GATT 是属性读写/通知
TCP 双方都 send/recv
BLE 常见是 Client 发起读写，Server notify
```

---

## BLE 广播，有点像 UDP 广播

因为：

```text
不需要连接
一方广播，另一方扫描
不保证对方一定收到
适合发现设备
```

但不同点是：

```text
BLE 广播包小
BLE 广播不是 IP 网络
BLE 广播有特定广播信道和扫描机制
UDP 广播在局域网 IP 层工作
```

---

# 18. 重要概念对照表

| 概念    | BLE                                     | TCP/UDP                   |
| ----- | --------------------------------------- | ------------------------- |
| 地址    | 蓝牙 MAC 地址 / 随机地址                        | IP 地址                     |
| 找设备   | Advertising / Scanning                  | IP 扫描、DNS、mDNS、广播         |
| 建立连接  | GAP Connect                             | TCP connect               |
| 无连接发送 | Advertising                             | UDP sendto / 广播           |
| 数据组织  | GATT Service/Characteristic             | 应用层自定义协议                  |
| 数据发送  | Read/Write/Notify/Indicate              | send/recv/sendto/recvfrom |
| 服务标识  | UUID                                    | 端口号 / URL / Topic         |
| 角色    | Central/Peripheral + GATT Client/Server | Client/Server             |
| 可靠性   | BLE 链路层有重传；Notify 无应用确认，Indicate 有确认    | TCP 可靠，UDP 不可靠            |
| 数据量   | 小数据为主                                   | 可小可大，TCP 适合大数据            |
| 功耗    | 低功耗设计                                   | Wi-Fi/TCP 通常功耗更高          |
| 距离    | 近距离                                     | 局域网/互联网                   |
| 典型应用  | 手环、传感器、遥控器、配网                           | HTTP、MQTT、OTA、视频、云通信      |

---

# 19. BLE GATT 和 TCP/UDP 的“服务”概念不同

## TCP 里的服务

TCP Server 通过端口提供服务：

```text
80    HTTP
443   HTTPS
1883  MQTT
8080  自定义服务
```

你访问的是：

```text
IP + Port
```

---

## BLE GATT 里的服务

BLE GATT Server 通过 UUID 提供服务：

```text
0x180F：Battery Service
0x180A：Device Information Service
自定义 UUID：厂商私有服务
```

你访问的是：

```text
Service UUID + Characteristic UUID
```

所以可以类比：

```text
TCP 端口号 ≈ BLE Service UUID / Characteristic UUID
```

但不完全一样。TCP 端口是通信入口；BLE UUID 是数据对象标识。

---

# 20. 数据大小和吞吐量区别

## TCP

TCP 很适合大数据：

```text
文件传输
HTTP 下载
OTA 固件升级
音视频流
MQTT 长连接
```

TCP 可以持续传很多数据，吞吐量高。

---

## UDP

UDP 适合实时、小包、允许丢一点的场景：

```text
音视频实时传输
局域网发现
游戏同步
广播/组播
传感器快速上报
```

---

## BLE GATT

BLE GATT 更适合小数据：

```text
传感器数据
按键状态
电池电量
设备配置
蓝牙配网
简单控制命令
少量日志
```

虽然 BLE 也可以做较大数据传输，但要考虑：

```text
ATT MTU
连接间隔
每个连接事件可发包数
PHY 速率
Notify 频率
手机系统限制
协议栈 buffer
```

所以如果你要传大文件、图片、音频流，BLE GATT 通常不是最舒服的选择。

---

# 21. 可靠性区别

## TCP 可靠性

TCP 保证：

```text
不丢
不乱序
重复包处理
拥塞控制
流量控制
```

应用层看到的是有序字节流。

---

## UDP 可靠性

UDP 不保证：

```text
可能丢包
可能乱序
可能重复
没有重传
```

要可靠就自己在应用层做：

```text
序号
ACK
超时重传
滑动窗口
校验
```

---

## BLE GATT 可靠性

BLE 底层 Link Layer 本身有确认和重传机制，所以连接态下的数据比 UDP 更可靠。

但是 GATT 操作也有区别：

| BLE 操作        | 可靠性特点         |
| ------------- | ------------- |
| Read          | 有请求和响应        |
| Write Request | 有响应，可靠性更强     |
| Write Command | 无 GATT 响应，速度快 |
| Notify        | 无应用层确认        |
| Indicate      | 有应用层确认        |

简单记：

```text
Notify ≈ 快，但没有应用确认
Indicate ≈ 慢一点，但有确认
Write Request ≈ 写完对方会响应
Write Command ≈ 只管写，不等响应
```

---

# 22. “粘包”问题对比

## TCP 有粘包/拆包

TCP 是字节流，所以必须自己定义帧边界：

```text
帧头 + 长度 + 数据 + CRC
```

---

## UDP 没有 TCP 那种粘包

UDP 是数据报，一次 `sendto()` 对应一个 UDP 数据报。

但 UDP 可能：

```text
丢包
乱序
被截断
```

---

## BLE GATT 一般不是 TCP 那种粘包

BLE GATT 一次 Read/Write/Notify 通常对应一次 GATT 操作。

但如果你自己在一个 Characteristic 里面传长数据，比如分包传文件，也需要自己设计：

```text
包序号
总长度
分片长度
CRC
ACK
重传
```

所以：

```text
TCP：天然需要解决流边界问题
UDP：天然保留包边界，但要解决丢包
BLE GATT：属性操作有边界，但大数据分包要自己管理
```

---

# 23. 从 ESP32 开发角度怎么选？

## 用 TCP 的场景

```text
ESP32 连接云服务器
HTTP 请求天气
MQTT 连接 Broker
OTA 固件升级
局域网 TCP 透传
需要可靠传输大量数据
```

例如：

```text
ESP32 → Wi-Fi → 路由器 → 云服务器
```

---

## 用 UDP 的场景

```text
局域网设备发现
低延迟小包通信
广播/组播
不要求每包都可靠
自己愿意做重传机制
```

例如：

```text
遥控器局域网广播寻找设备
设备周期性 UDP 上报状态
```

---

## 用 BLE GATT 的场景

```text
手机近距离配置设备
BLE 配网
低功耗传感器
手机读取设备状态
手机控制设备开关
设备上报少量数据
没有 Wi-Fi 也要通信
```

例如：

```text
手机 App 连接 ESP32
写入 Wi-Fi SSID/密码
ESP32 收到后连接路由器
```

---

# 24. 三者通信流程总图

```text
一、TCP

Server:
socket → bind → listen → accept → recv/send

Client:
socket → connect → send/recv


二、UDP

Server:
socket → bind → recvfrom/sendto

Client:
socket → sendto/recvfrom


三、BLE

Peripheral / GATT Server:
初始化 BLE → 创建服务/特征值 → 广播 → 等待连接 → 被读写/Notify

Central / GATT Client:
初始化 BLE → 扫描 → 连接 → 发现服务 → 读/写/订阅 Notify
```

---

# 25. 最简单的记忆方式

```text
TCP：
像打电话。
先拨号连接，接通后双方持续说话，内容可靠有顺序。

UDP：
像发传单。
不建立连接，直接发，快，但不保证对方收到。

BLE GAP：
像举牌招人。
设备广播“我在这里”，手机扫描发现它，然后建立连接。

BLE GATT：
像远程读写寄存器。
连接后，手机读/写设备的 Service/Characteristic，设备也可以 Notify 状态变化。
```

---

# 26. 最关键结论

```text
TCP/UDP 是网络传输层协议：
    核心是 IP + Port + send/recv

BLE GAP/GATT 是蓝牙设备发现和数据模型：
    核心是 Advertising + Connection + Service + Characteristic

BLE GAP 类似“发现和连接管理”
BLE GATT 类似“连接后的远程变量/寄存器读写”
TCP 类似“可靠字节流通道”
UDP 类似“无连接数据报通道”
```

如果你从嵌入式开发角度入门，可以这样对应：

```text
TCP Server 等连接
≈ BLE Peripheral 广播等连接

TCP Client 主动 connect
≈ BLE Central 扫描后主动 connect

TCP send/recv
≈ BLE GATT Read/Write/Notify

UDP 广播发现设备
≈ BLE Advertising 被扫描发现

TCP/UDP 的端口
≈ BLE 的 Service/Characteristic UUID

TCP 自定义协议帧
≈ BLE 自定义 GATT Service/Characteristic
```

但要记住最后一句：**BLE 不是 IP 网络，GATT 不是 socket。BLE GATT 更像“远程变量表/寄存器表”，TCP/UDP 更像“数据通道”。**
