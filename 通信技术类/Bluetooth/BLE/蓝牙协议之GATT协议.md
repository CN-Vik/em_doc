![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/abbc57b1a1e74d3ba981b72421c8c0eb.png)

-------
GATT 全称是 **Generic Attribute Profile，通用属性配置文件**。

一句话理解：

> **GATT 负责 BLE 连接之后，业务数据怎么组织、怎么读、怎么写、怎么通知。**

也就是说：

```text
GAP：负责“发现设备、广播、扫描、连接”
GATT：负责“连上之后，数据怎么交互”
```

---

## 1. GATT 在 BLE 协议栈里的位置

```text
应用层 Application
        │
        ▼
GATT  ← 负责组织业务数据：服务、特征值、读写、通知
        │
        ▼
ATT   ← 负责真正的属性读写协议命令
        │
        ▼
L2CAP
        │
        ▼
Link Layer
        │
        ▼
PHY
```

可以简单理解成：

```text
GATT 是“数据模型”
ATT 是“具体读写命令”
```

---

## 2. GATT 主要解决什么问题？

BLE 不是像 TCP 那样直接发一串字节流，而是把数据组织成一种“表”。

这个表里面有：

```text
Service          服务
Characteristic  特征值
Descriptor       描述符
```

整体结构像这样：

```text
GATT Server
│
├── Service 1：电池服务
│   │
│   └── Characteristic：电量
│       ├── Value：85%
│       └── Descriptor：是否开启通知
│
├── Service 2：温湿度服务
│   │
│   ├── Characteristic：温度
│   │   └── Value：26.5℃
│   │
│   └── Characteristic：湿度
│       └── Value：57%
│
└── Service 3：设备信息服务
    │
    ├── Characteristic：设备型号
    └── Characteristic：固件版本
```

所以 GATT 的核心就是：

> **把 BLE 设备的数据，整理成一个可以被手机/主机访问的“属性数据库”。**

---

## 3. 用温湿度传感器理解 GATT

假设你做一个 ESP32 温湿度传感器。

你想让手机读取温度和湿度，那么可以设计一个 GATT 表：

```text
环境传感器 Service
│
├── 温度 Characteristic
│   ├── Read：手机可以读取温度
│   └── Notify：温度变化时主动通知手机
│
└── 湿度 Characteristic
    ├── Read：手机可以读取湿度
    └── Notify：湿度变化时主动通知手机
```

手机连接后，不是直接问：

```text
你给我发点数据
```

而是会先发现你的 GATT 服务：

```text
你有哪些 Service？
你每个 Service 下面有哪些 Characteristic？
这些 Characteristic 支持 Read、Write、Notify 吗？
```

然后手机再进行读写。

---

## 4. GATT 里面最重要的三个概念

### 1）Service：服务

Service 是一组相关数据的集合。

比如：

```text
电池服务 Battery Service
心率服务 Heart Rate Service
环境传感器服务 Environmental Sensing Service
设备信息服务 Device Information Service
自定义控制服务 Custom Control Service
```

你可以把 Service 理解为：

> **一个功能模块。**

比如一个智能小车 BLE 控制设备：

```text
电机控制服务
姿态数据服务
电池状态服务
参数配置服务
```

---

### 2）Characteristic：特征值

Characteristic 是真正承载数据的地方。

比如：

```text
温度 Characteristic：26.5
湿度 Characteristic：57
电量 Characteristic：85
LED开关 Characteristic：1 或 0
电机速度 Characteristic：1200 rpm
```

你可以把 Characteristic 理解为：

> **一个具体的数据项，类似一个变量。**

比如：

```c
float temperature;
float humidity;
uint8_t led_state;
uint16_t motor_speed;
```

这些变量映射到 BLE 里，就可以设计成 Characteristic。

---

### 3）Descriptor：描述符

Descriptor 是用来描述 Characteristic 的附加信息。

最常见的是 **CCCD**：

```text
Client Characteristic Configuration Descriptor
客户端特征配置描述符
```

它常用于控制 Notify / Indicate 是否打开。

比如手机想接收温度通知，就会往 CCCD 里面写：

```text
打开 Notify
```

然后设备才能主动给手机推送温度数据。

---

## 5. GATT 支持哪些数据操作？

GATT 常见操作有这几个：

| 操作       | 方向                 | 含义                |
| -------- | ------------------ | ----------------- |
| Read     | Client 读 Server    | 读取某个特征值           |
| Write    | Client 写 Server    | 修改某个特征值           |
| Notify   | Server 主动发给 Client | 不需要 Client 确认，速度快 |
| Indicate | Server 主动发给 Client | 需要 Client 确认，更可靠  |
| Discover | Client 查询 Server   | 发现服务和特征值          |

---

## 6. GATT Server 和 GATT Client 是什么？

BLE GATT 通信里面有两个角色：

```text
GATT Server：提供数据的一方
GATT Client：访问数据的一方
```

常见情况：

```text
温湿度传感器 / 手环 / BLE外设  → GATT Server
手机 / 网关 / 主控设备          → GATT Client
```

例如：

```text
ESP32 温湿度传感器：GATT Server
手机 APP：GATT Client
```

流程：

```text
手机连接 ESP32
        ↓
手机发现 ESP32 的 GATT 服务
        ↓
手机读取温度 Characteristic
        ↓
ESP32 通过 Notify 主动上报温湿度
```

---

## 7. GATT 和 GAP 的关系

这个很关键。

```text
GAP：负责连接前和连接管理
GATT：负责连接后的数据通信
```

完整流程大概是：

```text
1. Peripheral 开始广播              ← GAP
2. Central 扫描到设备               ← GAP
3. Central 发起连接                 ← GAP
4. BLE 连接建立成功                 ← GAP
5. Client 发现 Server 的服务        ← GATT
6. Client 读取 / 写入特征值          ← GATT / ATT
7. Server Notify 上报数据           ← GATT / ATT
```

所以：

```text
没 GAP，设备可能连不上
没 GATT，连上以后不知道怎么传业务数据
```

---

## 8. GATT 和 ATT 的区别

这个也容易混。

```text
GATT：定义数据结构和访问方式
ATT：真正执行属性读写的底层协议
```

举个例子：

```text
GATT 说：
这里有一个温度 Characteristic，支持 Read 和 Notify。

ATT 负责：
发送 Read Request
返回 Read Response
发送 Handle Value Notification
```

可以类比：

```text
GATT 像菜单：
    有温度、有湿度、有电量

ATT 像服务员：
    帮你把“读温度”的请求送过去，再把结果带回来
```

---

## 9. 从嵌入式开发角度怎么理解 GATT？

你在写 BLE 外设时，通常要干这些事：

```text
1. 创建 GATT Server
2. 创建 Service
3. 添加 Characteristic
4. 设置 Characteristic 权限
5. 设置 Characteristic 属性
6. 处理 Read / Write 事件
7. 需要时发送 Notify / Indicate
```

比如你做一个 BLE LED 控制：

```text
LED控制服务 Service
│
└── LED状态 Characteristic
    ├── Read：手机读取当前 LED 状态
    └── Write：手机写 1 开灯，写 0 关灯
```

手机写入：

```text
0x01
```

ESP32 收到 Write 事件后：

```c
gpio_set_level(LED_GPIO, 1);
```

这就是 GATT 在嵌入式开发里的实际作用。

---

## 10. BLE GATT 数据结构图

```text
GATT Server
│
├── Service：LED 控制服务
│   │
│   └── Characteristic：LED 状态
│       ├── Properties：Read / Write
│       ├── Value：0x00 或 0x01
│       └── Descriptor：描述 LED 状态
│
├── Service：传感器服务
│   │
│   ├── Characteristic：温度
│   │   ├── Properties：Read / Notify
│   │   └── Value：26.5℃
│   │
│   └── Characteristic：湿度
│       ├── Properties：Read / Notify
│       └── Value：57%
│
└── Service：设备信息服务
    │
    ├── Characteristic：设备名称
    └── Characteristic：固件版本
```

---

## 11. GATT 的核心作用总结

GATT 主要负责：

```text
1. 定义 BLE 设备有哪些服务 Service
2. 定义每个服务下面有哪些特征值 Characteristic
3. 定义这些特征值能不能读、能不能写、能不能 Notify
4. 让客户端发现这些服务和特征值
5. 让客户端读取、写入特征值
6. 让服务端主动通知客户端数据变化
```

---

## 12. 最简单的一句话

你可以这样记：

```text
GAP：负责让设备被发现、被连接
GATT：负责连接后，把数据做成 Service / Characteristic 给别人读写
ATT：负责底层真正的读写命令
```

再通俗一点：

```text
GAP = 找到设备并连上
GATT = 连上后看到设备有哪些“数据接口”
ATT = 真正执行读写这些“数据接口”
```

对于嵌入式开发者来说，**GATT 就是你设计 BLE 数据接口的地方**。你要传温度、湿度、电量、按键、电机速度、配置参数，基本都要设计成 GATT 的 Service 和 Characteristic。
