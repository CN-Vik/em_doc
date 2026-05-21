我们来详细梳理一下 MQTT（Message Queuing Telemetry Transport）协议，尽量用嵌入式开发的角度和易懂的方式讲清楚，并配一个实际案例。我们一步步来。

---

## 1. MQTT 是什么？

MQTT 是一种轻量级的 **发布/订阅（Pub/Sub）消息协议**，常用于物联网（IoT）、嵌入式设备、传感器等对带宽和功耗敏感的场景。

特点：

* **轻量**：协议头很小，通信开销低
* **发布/订阅模式**：解耦发送端和接收端
* **支持 QoS（消息服务质量）**：保证消息的可靠性
* **长连接**：通常基于 TCP 维持一个持久连接

在嵌入式开发中，MQTT 特别适合 MCU、传感器或小型设备上传数据到云端，或者接收控制命令。

---

## 2. MQTT 架构

MQTT 有三个核心角色：

1. **Broker（消息代理）**

   * 中心服务器，负责接收、存储、转发消息
   * 常见开源 Broker：Mosquitto、EMQX、HiveMQ

2. **Publisher（发布者）**

   * 发送消息的设备或应用
   * 只负责把消息发给 Broker，不关心谁订阅

3. **Subscriber（订阅者）**

   * 接收消息的设备或应用
   * 通过订阅主题（Topic）获得 Broker 转发的消息

4. **Topic（主题）**

   * MQTT 的“消息分类”，订阅者通过 Topic 接收感兴趣的消息
   * 类似嵌入式项目里的信号通道

---

## 3. MQTT 通信模式

MQTT 使用 **发布/订阅模式**，与传统的点对点 TCP 不同：

```
Publisher --> Broker --> Subscriber
```

流程：

1. **客户端连接 Broker**（CONNECT）
2. **订阅主题**（SUBSCRIBE）
3. **发布消息**（PUBLISH）
4. **Broker 转发消息**到订阅了该主题的客户端
5. 可选择**断开连接**（DISCONNECT）

---

## 4. MQTT 协议帧格式（基本概念）

MQTT 帧（Packet）由 **固定报头 + 可变报头 + 有效载荷** 组成：

### 1) 固定报头（Fixed Header）

* **报文类型（4 bits）**：CONNECT, PUBLISH, SUBSCRIBE 等
* **标志位（4 bits）**：如 QoS, DUP, Retain
* **剩余长度（Variable Length）**：表示剩下报文长度

### 2) 可变报头（Variable Header）

不同类型报文字段不同，例如：

* CONNECT：协议名、协议版本、客户端ID、保活时间等
* PUBLISH：Topic 名、消息标识符（Packet Identifier）等

### 3) 有效载荷（Payload）

* PUBLISH 消息：消息内容
* CONNECT 消息：用户名、密码等

> 简单理解：固定报头像信封，可变报头是信封上的信息，有效载荷是信里的内容。

---

## 5. QoS（消息服务等级）

MQTT 提供三种消息传输保证：

| QoS | 含义   | 特点               |
| --- | ---- | ---------------- |
| 0   | 最多一次 | “发了就算”，不保证到达     |
| 1   | 至少一次 | 确保消息到达，可能重复      |
| 2   | 仅一次  | 确保消息到达且不会重复（最可靠） |

---

## 6. 实际应用案例

### 场景：智能家居温湿度监测

* MCU（ESP32）采集温湿度数据
* 通过 MQTT 将数据发送到云端
* 手机 App 订阅该主题，实时显示温湿度

**主题设计**：

```
home/livingroom/temperature
home/livingroom/humidity
```

**通信流程**：

1. ESP32 启动，连接 Wi-Fi
2. ESP32 向 Broker 发送 **CONNECT** 请求
3. 手机 App 启动，连接 Broker，并发送 **SUBSCRIBE home/livingroom/#** 订阅该房间的所有数据
4. ESP32 每隔 10 秒读取温湿度，并发送 **PUBLISH** 消息到对应 Topic
5. Broker 接收到消息，转发给所有订阅该 Topic 的客户端（手机 App）
6. 手机 App 显示实时温湿度

---

## 7. MQTT 通信流程图（简化版）

```
  ESP32 (Publisher)          Broker              手机 App (Subscriber)
        |                     |                       |
        |--- CONNECT --------->|                       |
        |                     |                       |
        |                     |<--- CONNACK --------- |
        |                     |                       |
        |--- PUBLISH -------->|                       |
        |                     |                       |
        |                     |--- PUBLISH --------->|
        |                     |                       |
        |                     |                       |
```

---

## 8. 嵌入式开发注意点

* **资源占用**：MQTT 很轻量，但仍需注意 MCU RAM/Flash
* **长连接维护**：通常用心跳（PINGREQ / PINGRESP）保持连接
* **断线重连**：MCU 通信可能不稳定，需要处理重连逻辑
* **消息大小**：根据 MCU 内存，Payload 通常不要太大

---


![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/585c1832d22a421597a9734b652b3abc.png)

----------
--------
----------
下面按**嵌入式新人视角**讲 MQTT：你可以把 MQTT 理解成“专门给物联网设备用的轻量级消息协议”。它不是替代 TCP/IP，而是跑在 TCP 之上的**应用层协议**，常用于 ESP32、STM32+4G 模块、Linux 网关、云平台、手机 App 之间传消息。

---

# 1. MQTT 是什么？

MQTT 全称常说是 **MQ Telemetry Transport**，它是一个轻量级的**发布/订阅**消息协议，特别适合低功耗传感器、低带宽、不稳定网络、物联网设备等场景。MQTT.org 也把它定义为面向 IoT 的 OASIS 标准消息协议，特点是轻量、发布/订阅、适合小代码量和低带宽设备。([MQTT][1])

一句话理解：

> MQTT 就是让设备不用互相直连，而是都连到一个“消息中转站”，谁想发消息就发布到某个主题，谁想收消息就订阅这个主题。

这个“消息中转站”叫 **Broker**。

---

# 2. MQTT 和 HTTP 最大区别

你之前问过 HTTP 获取天气，那个是：

```text
ESP32 主动问服务器：天气是多少？
服务器回答：天气是 25℃
```

这是典型的**请求/响应模型**。

MQTT 是：

```text
设备 A 把消息发布到 broker
broker 再转发给订阅这个主题的设备 B、App、服务器
```

是典型的**发布/订阅模型**。

对比一下：

| 对比项    | HTTP             | MQTT             |
| ------ | ---------------- | ---------------- |
| 通信模型   | 请求/响应            | 发布/订阅            |
| 谁主动    | 一般客户端主动请求        | 设备和云都可以主动发消息     |
| 连接方式   | 常见短连接，也可长连接      | 长连接为主            |
| 适合场景   | 获取天气、下载文件、OTA 固件 | 设备状态上报、远程控制、实时消息 |
| 数据方向   | 客户端请求，服务器响应      | 双向都方便            |
| 嵌入式适合度 | 能用，但频繁通信开销大      | 很适合物联网小设备        |

举例：

* **HTTP 更适合**：ESP32 获取天气、下载 OTA 固件、请求 REST API。
* **MQTT 更适合**：温湿度实时上报、App 控制设备开关、设备在线状态、告警推送。

---

# 3. MQTT 的核心角色

MQTT 里主要有 3 个概念：

## 3.1 Client：客户端

所有连接 Broker 的都叫客户端。

比如：

```text
ESP32         是 MQTT Client
手机 App      是 MQTT Client
云端服务程序  是 MQTT Client
Linux 网关    是 MQTT Client
```

注意：MQTT 里面设备之间**通常不直接通信**，都通过 Broker 转发。

---

## 3.2 Broker：消息服务器 / 中转站

Broker 就像一个“快递驿站”或者“微信群服务器”。

它负责：

1. 接收客户端连接。
2. 接收客户端发布的消息。
3. 根据主题，把消息转发给订阅者。
4. 管理在线状态、心跳、会话、遗嘱消息等。

常见 Broker：

```text
Mosquitto
EMQX
HiveMQ
RabbitMQ MQTT 插件
阿里云 IoT
腾讯云 IoT
AWS IoT
```

---

## 3.3 Topic：主题

Topic 就是 MQTT 里的“消息频道”。

比如：

```text
home/livingroom/temperature
home/livingroom/humidity
device/esp32_001/status
device/esp32_001/cmd/led
device/esp32_001/telemetry
```

你可以理解成：

```text
topic = 消息的地址 / 分类 / 频道
```

MQTT 的 Topic 是层级结构，一般用 `/` 分隔，例如 `sensors/room1/temperature`。订阅时还支持 `+` 和 `#` 通配符：`+` 匹配单层，`#` 匹配后续多层。([Eclipse Mosquitto][2])

例如：

```text
device/+/temperature
```

可以匹配：

```text
device/esp32_001/temperature
device/esp32_002/temperature
```

再比如：

```text
device/esp32_001/#
```

可以匹配：

```text
device/esp32_001/status
device/esp32_001/temperature
device/esp32_001/cmd/led
device/esp32_001/sensor/imu/data
```

---

# 4. MQTT 的发布/订阅模型

MQTT 最核心的思想是：

```text
发布者 Publisher：往某个 topic 发消息
订阅者 Subscriber：订阅某个 topic，等待接收消息
Broker：负责中转
```

比如：

```text
ESP32 发布：
topic: device/esp32_001/temperature
payload: {"temp":25.6}

手机 App 订阅：
topic: device/esp32_001/temperature
```

流程：

```text
ESP32  ---> Broker ---> 手机 App
```

ESP32 不需要知道手机 App 的 IP，也不需要和手机 App 建立 TCP 连接。

这就是 MQTT 很适合物联网的原因。

---

# 5. MQTT 在协议栈里的位置

嵌入式里要把 MQTT 放到协议栈里看：

```text
你的应用代码
   ↓
MQTT 协议层
   ↓
TLS/SSL，可选，加密用
   ↓
TCP
   ↓
IP
   ↓
Wi-Fi / 以太网 / 4G / NB-IoT
   ↓
物理层
```

如果不用加密：

```text
MQTT over TCP
```

常见端口：

```text
1883：MQTT 明文 TCP
8883：MQTT over TLS/SSL
```

MQTT.org FAQ 里也说明 TCP/IP 1883 是 MQTT 常用保留端口，8883 通常用于 MQTT over SSL/TLS。([MQTT][3])

---

# 6. MQTT 为什么适合嵌入式？

因为它解决了嵌入式设备常见的几个痛点。

## 6.1 设备资源小

MQTT 报文头很小，协议简单，比 HTTP 头部轻很多。比如 HTTP 请求里有一堆字段：

```http
GET /weather HTTP/1.1
Host: api.xxx.com
User-Agent: ESP32
Connection: close
...
```

而 MQTT 的普通 PUBLISH 报文，固定头最少可以只有 2 个字节。

---

## 6.2 长连接，适合实时控制

设备连上 Broker 后，可以一直保持 TCP 长连接。

云端要控制设备时，不需要等设备下一次 HTTP 轮询，而是直接：

```text
云端发布命令 -> Broker -> 设备立即收到
```

比如：

```text
topic: device/esp32_001/cmd/led
payload: {"led":1}
```

ESP32 订阅了这个 topic，就能立刻收到开灯命令。

---

## 6.3 支持消息质量 QoS

MQTT 有 3 种 QoS，也就是消息可靠性等级。Mosquitto 文档里也说明 MQTT 定义了 QoS 0、1、2 三个等级，等级越高越可靠，但延迟和带宽开销也越大。([Eclipse Mosquitto][2])

| QoS   | 含义   | 特点           | 嵌入式场景       |
| ----- | ---- | ------------ | ----------- |
| QoS 0 | 最多一次 | 发出去就不管，可能丢   | 温度周期上报      |
| QoS 1 | 至少一次 | 一定尽量送到，但可能重复 | 控制命令、告警     |
| QoS 2 | 刚好一次 | 最可靠，但流程最复杂   | 很少在小 MCU 上用 |

嵌入式实际建议：

```text
普通传感器数据：QoS 0
重要控制命令：QoS 1
极重要且不能重复的业务：QoS 2，但 MCU 上慎用
```

为什么 QoS 1 可能重复？

因为它是“至少一次”。比如设备发了消息，Broker 收到了，也回了 PUBACK，但是设备没收到 PUBACK，设备可能会重发。这时 Broker 可能收到两次相同消息。所以 QoS 1 的业务最好设计成**幂等**。

比如开灯命令：

```json
{"led":1}
```

执行两次也没事。

但如果是：

```json
{"motor_move_step":100}
```

重复执行可能会多走 100 步，就要小心。

---

# 7. MQTT 的几个重要机制

## 7.1 Retain：保留消息

Retain 可以让 Broker 保存某个 topic 的最后一条消息。

比如 ESP32 上报：

```text
topic: device/esp32_001/status
payload: online
retain: true
```

然后手机 App 后上线，刚订阅这个 topic，就能马上收到：

```text
online
```

不需要等 ESP32 下一次上报。

Retained Message 的作用就是让新订阅者立即拿到某个 topic 的最后状态，常用作“最后一次已知状态”。([Eclipse Mosquitto][2])

适合：

```text
设备当前状态
灯当前开关状态
温控器目标温度
设备是否在线
```

不太适合：

```text
高频传感器流数据
日志流
一次性告警
```

---

## 7.2 Keep Alive：心跳

MQTT 是长连接，所以需要判断设备是否还活着。

设备连接时会告诉 Broker：

```text
Keep Alive = 60 秒
```

如果一段时间没有任何 MQTT 数据，客户端会发：

```text
PINGREQ
```

Broker 回复：

```text
PINGRESP
```

这样双方就知道连接还正常。

对嵌入式来说，这个很重要。因为 Wi-Fi、4G 网络可能断掉，如果不做心跳，设备以为自己还连着，其实服务器早就收不到它的数据了。

---

## 7.3 Last Will：遗嘱消息

这个名字听起来奇怪，但非常实用。

设备连接 Broker 时可以提前告诉 Broker：

```text
如果我异常掉线了，请你帮我发布一条消息：
topic: device/esp32_001/status
payload: offline
```

如果设备正常断开，会发送 DISCONNECT，Broker 不会发遗嘱。

如果设备突然断电、Wi-Fi 崩了、程序死了，Broker 检测到超时后，就会自动发布这个遗嘱消息。Mosquitto 文档也说明，客户端连接 Broker 时可以设置 Will，当客户端异常断开时由 Broker 发布该消息。([Eclipse Mosquitto][2])

典型用途：

```text
设备在线/离线状态
网关异常掉线通知
传感器失联告警
```

---

## 7.4 Clean Session / 持久会话

MQTT 3.1.1 里有 Clean Session。

简单理解：

```text
Clean Session = 1：每次连接都是新会话，断开后订阅关系清掉
Clean Session = 0：Broker 记住这个客户端，断线期间的重要消息可以保留
```

Mosquitto 文档说明，Clean Session 为 false 时，连接会被看作持久连接，断开后订阅仍可保留，后续 QoS 1/2 消息可被存储到客户端未来重连。([Eclipse Mosquitto][2])

嵌入式建议：

如果是普通传感器：

```text
Clean Session = 1
```

如果是重要设备，需要断线后补收命令：

```text
Clean Session = 0
```

但要注意 Broker 资源占用和消息堆积。

---

# 8. 实际应用案例：ESP32 温湿度上报 + 手机远程控制 LED

假设你做一个 ESP32 设备：

功能：

```text
1. ESP32 连接 Wi-Fi
2. ESP32 连接 MQTT Broker
3. ESP32 每 5 秒上报温湿度
4. 手机 App 可以远程控制 ESP32 的 LED
5. 手机 App 可以看到 ESP32 在线/离线状态
```

## 8.1 设计 Topic

可以这样设计：

```text
设备属性：
device/esp32_001/status

传感器数据：
device/esp32_001/telemetry

LED 控制命令：
device/esp32_001/cmd/led

LED 状态反馈：
device/esp32_001/state/led
```

---

## 8.2 消息内容

ESP32 上报温湿度：

```text
topic:
device/esp32_001/telemetry

payload:
{
  "temp": 25.6,
  "hum": 60,
  "battery": 87
}
```

手机控制 LED：

```text
topic:
device/esp32_001/cmd/led

payload:
{
  "led": 1
}
```

ESP32 执行后反馈状态：

```text
topic:
device/esp32_001/state/led

payload:
{
  "led": 1
}
```

设备上线：

```text
topic:
device/esp32_001/status

payload:
online

retain:
true
```

设备异常掉线遗嘱：

```text
topic:
device/esp32_001/status

payload:
offline

retain:
true
```

---

# 9. 这个案例的完整通信流程

## 9.1 设备启动流程

```text
ESP32 上电
  ↓
初始化 Wi-Fi
  ↓
连接路由器
  ↓
拿到 IP 地址
  ↓
DNS 解析 MQTT Broker 地址
  ↓
建立 TCP 连接
  ↓
如果用 TLS，进行 TLS 握手
  ↓
发送 MQTT CONNECT 报文
  ↓
Broker 回复 CONNACK
  ↓
MQTT 连接成功
```

---

## 9.2 订阅控制命令

ESP32 要能被手机控制，所以 ESP32 要订阅命令 topic：

```text
ESP32 -> Broker:
SUBSCRIBE device/esp32_001/cmd/led

Broker -> ESP32:
SUBACK
```

之后只要有人往这个 topic 发消息，ESP32 就能收到。

---

## 9.3 上报传感器数据

ESP32 每 5 秒读取一次温湿度：

```text
ESP32 -> Broker:
PUBLISH device/esp32_001/telemetry
payload: {"temp":25.6,"hum":60}
```

如果手机 App 或云服务器订阅了：

```text
device/esp32_001/telemetry
```

Broker 就会转发给它们。

---

## 9.4 手机控制 LED

手机 App 发布：

```text
topic:
device/esp32_001/cmd/led

payload:
{"led":1}
```

Broker 转发给 ESP32：

```text
Broker -> ESP32:
PUBLISH device/esp32_001/cmd/led
payload: {"led":1}
```

ESP32 收到后：

```c
if led == 1:
    gpio_set_level(LED_GPIO, 1);
```

然后 ESP32 反馈：

```text
topic:
device/esp32_001/state/led

payload:
{"led":1}
```

这样手机 App 就知道命令真的执行了。

---

## 9.5 通信流程图

```text
┌──────────────┐        MQTT         ┌──────────────┐        MQTT         ┌──────────────┐
│   ESP32设备   │  <-------------->  │ MQTT Broker  │  <-------------->  │   手机App/云端 │
└──────────────┘                     └──────────────┘                     └──────────────┘
       │                                      │                                      │
       │ CONNECT                              │                                      │
       │─────────────────────────────────────>│                                      │
       │ CONNACK                              │                                      │
       │<─────────────────────────────────────│                                      │
       │                                      │                                      │
       │ SUBSCRIBE cmd/led                    │                                      │
       │─────────────────────────────────────>│                                      │
       │ SUBACK                               │                                      │
       │<─────────────────────────────────────│                                      │
       │                                      │                                      │
       │ PUBLISH telemetry                    │                                      │
       │─────────────────────────────────────>│                                      │
       │                                      │ PUBLISH telemetry                    │
       │                                      │─────────────────────────────────────>│
       │                                      │                                      │
       │                                      │ PUBLISH cmd/led                      │
       │                                      │<─────────────────────────────────────│
       │ PUBLISH cmd/led                      │                                      │
       │<─────────────────────────────────────│                                      │
       │                                      │                                      │
       │ PUBLISH state/led                    │                                      │
       │─────────────────────────────────────>│                                      │
       │                                      │ PUBLISH state/led                    │
       │                                      │─────────────────────────────────────>│
```

---

# 10. MQTT 协议帧格式

重点来了：MQTT 的协议帧，也叫 **MQTT Control Packet**。

OASIS MQTT 3.1.1 规范定义了多种 MQTT Control Packet，其中 PUBLISH 用来传应用消息。([OASIS开放文档][4])

MQTT 报文大体结构：

```text
┌────────────────────┐
│ Fixed Header        │ 固定头，所有 MQTT 报文都有
├────────────────────┤
│ Variable Header     │ 可变头，不同报文不一样
├────────────────────┤
│ Payload             │ 负载，有些报文有，有些没有
└────────────────────┘
```

不是所有 MQTT 报文都有 Variable Header 和 Payload，但一定有 Fixed Header。

---

# 11. Fixed Header 固定头格式

固定头至少 2 字节。

```text
Byte 1:
┌───────┬───────┬───────┬───────┬──────┬──────┬──────┬──────┐
│ bit7  │ bit6  │ bit5  │ bit4  │ bit3 │ bit2 │ bit1 │ bit0 │
├───────┴───────┴───────┴───────┼──────┴──────┴──────┴──────┤
│ MQTT Control Packet Type       │ Flags                    │
└────────────────────────────────┴───────────────────────────┘

Byte 2...:
Remaining Length
```

## 11.1 第 1 字节

高 4 位：

```text
MQTT Control Packet Type
```

低 4 位：

```text
Flags
```

例如 PUBLISH 报文：

```text
bit7 bit6 bit5 bit4 | bit3 bit2 bit1 bit0
 0    0    1    1   | DUP  QoS  QoS  RETAIN
```

PUBLISH 的类型值是 3。

所以普通 QoS0、不重复、不保留的 PUBLISH：

```text
0011 0000 = 0x30
```

---

## 11.2 Remaining Length

Remaining Length 表示：

```text
Variable Header + Payload 的总长度
```

注意：它不包含 Fixed Header 自己的长度。

它使用 MQTT 自己的变长编码，1 到 4 个字节。

比如 Remaining Length = 39：

```text
0x27
```

---

# 12. MQTT 常见报文类型

MQTT 3.1.1 常见报文类型如下：

| 类型值 | 报文名         | 方向               | 作用          |
| --: | ----------- | ---------------- | ----------- |
|   1 | CONNECT     | Client -> Broker | 客户端请求连接     |
|   2 | CONNACK     | Broker -> Client | 连接确认        |
|   3 | PUBLISH     | 双向               | 发布消息        |
|   4 | PUBACK      | 双向               | QoS 1 发布确认  |
|   5 | PUBREC      | 双向               | QoS 2 第一步确认 |
|   6 | PUBREL      | 双向               | QoS 2 第二步释放 |
|   7 | PUBCOMP     | 双向               | QoS 2 完成    |
|   8 | SUBSCRIBE   | Client -> Broker | 订阅主题        |
|   9 | SUBACK      | Broker -> Client | 订阅确认        |
|  10 | UNSUBSCRIBE | Client -> Broker | 取消订阅        |
|  11 | UNSUBACK    | Broker -> Client | 取消订阅确认      |
|  12 | PINGREQ     | Client -> Broker | 心跳请求        |
|  13 | PINGRESP    | Broker -> Client | 心跳响应        |
|  14 | DISCONNECT  | Client -> Broker | 正常断开连接      |

MQTT 5.0 在 MQTT 3.1.1 基础上增加了更多能力，例如属性、原因码、增强认证等，MQTT 5.0 标准中也包含 AUTH 等机制。([OASIS开放文档][5])

---

# 13. CONNECT 报文格式

CONNECT 是客户端连接 Broker 的第一个 MQTT 报文。

结构大概是：

```text
Fixed Header
  ├─ Packet Type = CONNECT
  └─ Remaining Length

Variable Header
  ├─ Protocol Name = "MQTT"
  ├─ Protocol Level = 4    // MQTT 3.1.1
  ├─ Connect Flags
  └─ Keep Alive

Payload
  ├─ Client ID
  ├─ Will Topic，可选
  ├─ Will Message，可选
  ├─ Username，可选
  └─ Password，可选
```

通俗理解：

```text
ESP32 对 Broker 说：

你好，我是 client_id = esp32_001
我使用 MQTT 3.1.1
我的心跳时间是 60 秒
我可能带用户名密码
如果我异常掉线，请帮我发布 offline
```

Broker 如果同意，就回复：

```text
CONNACK
```

---

# 14. CONNACK 报文格式

CONNACK 是 Broker 对 CONNECT 的响应。

大致结构：

```text
Fixed Header
  ├─ Packet Type = CONNACK
  └─ Remaining Length

Variable Header
  ├─ Connect Acknowledge Flags
  └─ Return Code / Reason Code
```

如果连接成功：

```text
CONNACK: success
```

如果失败，可能是：

```text
用户名密码错误
Client ID 不合法
Broker 不支持协议版本
Broker 不允许连接
```

---

# 15. SUBSCRIBE 报文格式

设备要接收某个 topic 的消息，就要发送 SUBSCRIBE。

比如 ESP32 想订阅：

```text
device/esp32_001/cmd/led
```

SUBSCRIBE 大概结构：

```text
Fixed Header
  ├─ Packet Type = SUBSCRIBE
  └─ Flags = 0010

Variable Header
  └─ Packet Identifier

Payload
  ├─ Topic Filter
  └─ Requested QoS
```

注意 SUBSCRIBE 报文的固定头第一个字节通常是：

```text
1000 0010 = 0x82
```

其中：

```text
1000 = SUBSCRIBE 类型值 8
0010 = 固定要求的 flags
```

Broker 收到后回复：

```text
SUBACK
```

告诉客户端订阅是否成功，以及最终授予的 QoS。

---

# 16. PUBLISH 报文格式，最重要

PUBLISH 是 MQTT 最核心的报文，因为真正的业务数据就是靠它传。

结构：

```text
Fixed Header
  ├─ Packet Type = PUBLISH
  ├─ DUP
  ├─ QoS
  ├─ RETAIN
  └─ Remaining Length

Variable Header
  ├─ Topic Name
  └─ Packet Identifier，仅 QoS > 0 时存在

Payload
  └─ 真实业务数据
```

比如：

```text
topic:
home/room1/temp

payload:
{"temp":25.6,"hum":60}
```

---

# 17. PUBLISH 固定头第 1 字节详解

PUBLISH 的第 1 字节：

```text
bit7 bit6 bit5 bit4 | bit3 | bit2 bit1 | bit0
 0    0    1    1   | DUP  |   QoS    | RETAIN
```

也就是：

```text
高 4 位：0011，表示 PUBLISH
bit3：DUP，是否重复发送
bit2~bit1：QoS 等级
bit0：RETAIN，是否保留消息
```

## 17.1 普通 QoS0 PUBLISH

```text
DUP = 0
QoS = 0
RETAIN = 0
```

所以：

```text
0011 0000 = 0x30
```

## 17.2 QoS1 PUBLISH

```text
DUP = 0
QoS = 1
RETAIN = 0
```

QoS 1 对应 bit2~bit1 = `01`：

```text
0011 0010 = 0x32
```

## 17.3 Retain PUBLISH

```text
DUP = 0
QoS = 0
RETAIN = 1
```

所以：

```text
0011 0001 = 0x31
```

---

# 18. 实际 PUBLISH 报文举例

假设 ESP32 发布：

```text
topic:
home/room1/temp

payload:
{"temp":25.6,"hum":60}
```

QoS0，不保留。

那么 MQTT 报文大概是：

```text
Fixed Header:
0x30 0x27

Variable Header:
0x00 0x0F
h  o  m  e  /  r  o  o  m  1  /  t  e  m  p

Payload:
{  "  t  e  m  p  "  :  2  5  .  6  ,  "  h  u  m  "  :  6  0  }
```

完整十六进制大概是：

```text
30 27
00 0F
68 6F 6D 65 2F 72 6F 6F 6D 31 2F 74 65 6D 70
7B 22 74 65 6D 70 22 3A 32 35 2E 36 2C 22 68 75 6D 22 3A 36 30 7D
```

拆开看：

```text
30
```

表示：

```text
PUBLISH，QoS0，DUP=0，RETAIN=0
```

```text
27
```

表示 Remaining Length = 39 字节。

```text
00 0F
```

表示 topic 长度是 15 字节。

```text
68 6F 6D 65 2F 72 6F 6F 6D 31 2F 74 65 6D 70
```

对应 ASCII 字符串：

```text
home/room1/temp
```

后面的：

```text
7B 22 74 65 6D 70 22 3A 32 35 2E 36 2C 22 68 75 6D 22 3A 36 30 7D
```

对应 JSON：

```json
{"temp":25.6,"hum":60}
```

---

# 19. QoS 0 / QoS 1 / QoS 2 的通信流程

## 19.1 QoS 0：最多一次

流程最简单：

```text
Client A                Broker
   │                      │
   │ PUBLISH              │
   │─────────────────────>│
   │                      │
```

没有确认包。

特点：

```text
快
省流量
可能丢
```

适合：

```text
温度每秒上报
IMU 数据上报
周期性电池电量
```

---

## 19.2 QoS 1：至少一次

```text
Client A                Broker
   │                      │
   │ PUBLISH              │
   │─────────────────────>│
   │ PUBACK               │
   │<─────────────────────│
```

特点：

```text
可靠性更高
可能重复
需要处理重复消息
```

适合：

```text
开关命令
报警消息
设备配置下发
```

---

## 19.3 QoS 2：刚好一次

```text
Client A                Broker
   │                      │
   │ PUBLISH              │
   │─────────────────────>│
   │ PUBREC               │
   │<─────────────────────│
   │ PUBREL               │
   │─────────────────────>│
   │ PUBCOMP              │
   │<─────────────────────│
```

特点：

```text
最可靠
流程复杂
开销大
延迟高
```

嵌入式小设备里不是不能用，但一般不优先用。

---

# 20. 心跳报文格式

MQTT 心跳很简单。

客户端发：

```text
PINGREQ
```

十六进制：

```text
C0 00
```

Broker 回复：

```text
PINGRESP
```

十六进制：

```text
D0 00
```

解释：

```text
C0 = PINGREQ 类型
D0 = PINGRESP 类型
00 = Remaining Length 为 0
```

这类报文没有 Variable Header，也没有 Payload。

---

# 21. DISCONNECT 报文格式

客户端正常断开连接时，会发：

```text
DISCONNECT
```

MQTT 3.1.1 下常见格式：

```text
E0 00
```

意思是：

```text
E0 = DISCONNECT
00 = Remaining Length 为 0
```

如果设备正常发了 DISCONNECT，Broker 就知道它是正常下线，不会触发 Last Will。

如果设备直接断电，没有 DISCONNECT，Broker 检测超时后才会触发遗嘱消息。

---

# 22. MQTT 载荷 Payload 一定是 JSON 吗？

不是。

MQTT 的 Payload 本质上是：

```text
一串二进制数据
```

你可以传：

```text
JSON
纯文本
二进制结构体
Protobuf
CBOR
自定义协议帧
图片片段
音频片段
```

但嵌入式常用 JSON，因为好调试：

```json
{
  "temp": 25.6,
  "hum": 60
}
```

缺点是 JSON 字符串比较占空间。

如果设备资源紧张、数据量大，可以用二进制格式：

```c
typedef struct {
    int16_t temp_x10;
    uint8_t hum;
    uint16_t battery_mv;
} sensor_payload_t;
```

然后直接作为 payload 发出去。

---

# 23. 嵌入式开发时怎么选参数？

## 23.1 Client ID

每个设备应该唯一：

```text
esp32_001
esp32_002
productA_sn_123456
```

不要多个设备用同一个 Client ID，否则会互相踢下线。

---

## 23.2 Keep Alive

常见设置：

```text
30 秒
60 秒
120 秒
```

建议：

```text
Wi-Fi 设备：60 秒左右
4G/NB-IoT 低功耗设备：可以更长
实时性高：可以短一点
```

---

## 23.3 QoS

建议：

```text
传感器周期上报：QoS 0
控制命令：QoS 1
告警消息：QoS 1
金融级/强一致业务：QoS 2，但 MCU 上少用
```

---

## 23.4 Retain

建议：

```text
设备状态：retain = true
当前配置：retain = true
当前 LED 状态：retain = true
实时传感器流：retain = false，除非你只关心最后值
```

---

## 23.5 Last Will

设备在线状态强烈建议用：

```text
Will topic:
device/esp32_001/status

Will payload:
offline

Will retain:
true
```

设备连接成功后主动发布：

```text
topic:
device/esp32_001/status

payload:
online

retain:
true
```

这样 App 随时订阅状态，都能知道设备当前是在线还是离线。

---

# 24. MQTT 和 WebSocket 的关系

很多新手会混淆：

```text
MQTT 是协议
WebSocket 也是协议/传输通道
```

MQTT 可以跑在：

```text
TCP 上
TLS 上
WebSocket 上
```

常见形式：

```text
MQTT over TCP
MQTT over TLS
MQTT over WebSocket
MQTT over WebSocket Secure
```

嵌入式设备一般直接用：

```text
MQTT over TCP/TLS
```

网页前端因为浏览器不能随便开原生 TCP socket，所以常用：

```text
MQTT over WebSocket
```

也就是说：

```text
ESP32 ←→ Broker：MQTT over TCP/TLS
网页前端 ←→ Broker：MQTT over WebSocket
```

---

# 25. 总结成一句嵌入式开发视角

MQTT 对你这种做 ESP32/MCU 的人来说，可以这样理解：

```text
TCP 只是提供一条可靠字节流通道；
MQTT 在 TCP 上面规定了：
1. 怎么连接服务器
2. 怎么订阅消息
3. 怎么发布消息
4. 怎么确认消息是否送到
5. 怎么保持心跳
6. 怎么判断设备异常掉线
7. 怎么把消息按 topic 分发给不同设备
```

最常用开发套路就是：

```text
Wi-Fi 连接成功
  ↓
MQTT 初始化
  ↓
设置 Client ID、用户名、密码、Broker 地址、端口
  ↓
设置 Last Will
  ↓
MQTT connect
  ↓
收到 connected 事件
  ↓
subscribe 控制命令 topic
  ↓
周期 publish 传感器数据
  ↓
收到命令 topic 后控制 GPIO / 电机 / 屏幕 / 音频
  ↓
断线后自动重连
```

你做 AI 语音助手、智能硬件、ESP32 屏幕设备、远程控制设备时，MQTT 很适合用来做：

```text
设备在线状态
设备属性上报
云端控制命令
App 控制设备
设备日志上报
告警推送
配置下发
```

但如果是：

```text
获取天气
下载 OTA 固件
请求大文件
调用普通 REST API
```

那 HTTP/HTTPS 更合适。

[1]: https://mqtt.org/?utm_source=chatgpt.com "MQTT - The Standard for IoT Messaging"
[2]: https://mosquitto.org/man/mqtt-7.html "MQTT man page | Eclipse Mosquitto"
[3]: https://mqtt.org/faq/?utm_source=chatgpt.com "FAQ"
[4]: https://docs.oasis-open.org/mqtt/mqtt/v3.1.1/os/mqtt-v3.1.1-os.html?utm_source=chatgpt.com "mqtt-v3.1.1-os.html - MQTT Version 3.1.1 - OASIS Open"
[5]: https://docs.oasis-open.org/mqtt/mqtt/v5.0/mqtt-v5.0.html?utm_source=chatgpt.com "MQTT Version 5.0 | OASIS Standard"
