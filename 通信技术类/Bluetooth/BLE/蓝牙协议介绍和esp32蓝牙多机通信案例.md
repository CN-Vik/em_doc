下面分两块讲：**蓝牙的起源和发展历史**，以及 **一个 ESP32 怎么给多台 ESP32 发数据**。

---

# 一、蓝牙 Bluetooth 的起源

蓝牙最早不是为了“耳机”发明的，而是为了替代各种短距离数据线。

早期电脑、手机、PDA、打印机之间通信，经常要用：

```text
串口线
并口线
红外 IrDA
专用数据线
```

这些方式很麻烦，所以 1990 年代，爱立信 Ericsson 开始研究一种短距离无线通信技术，目标是：

```text
低功耗
低成本
短距离
可以自动发现设备
可以替代线缆
```

“Bluetooth”这个名字来源于丹麦国王 **Harald Bluetooth**，他统一了丹麦和挪威。蓝牙技术借用这个名字，意思是“统一不同设备之间的通信”。

蓝牙标志也是由两个北欧古文字母组合而成的。

---

# 二、蓝牙发展历史时间线

## 1. 1998 年：Bluetooth SIG 成立

1998 年，爱立信、IBM、Intel、Nokia、Toshiba 等公司成立了 **Bluetooth SIG**，也就是蓝牙技术联盟。

这个组织负责制定蓝牙标准。

---

## 2. 1999 年：Bluetooth 1.0

早期蓝牙主要用于：

```text
手机和电脑传文件
蓝牙串口
蓝牙耳机
蓝牙鼠标键盘
```

但是早期版本兼容性差，连接慢，功耗也不算很低。

---

## 3. Bluetooth 2.0：EDR 高速率

Bluetooth 2.0 引入了 **EDR**，Enhanced Data Rate，增强数据率。

典型速率提升到大约：

```text
基础速率 BR：1 Mbps
EDR：2 Mbps / 3 Mbps 物理层速率
```

这时蓝牙耳机、蓝牙音箱、蓝牙串口模块开始大量普及。

经典蓝牙 SPP 串口透传模块，比如 HC-05、HC-06，就是这个时代很典型的应用。

---

## 4. Bluetooth 3.0：High Speed

Bluetooth 3.0 试图引入高速传输，配合 Wi-Fi 做大数据传输。

但实际工程里，Bluetooth 3.0 并没有像 4.0 BLE 那样产生革命性影响。

---

## 5. Bluetooth 4.0：BLE 出现，真正改变物联网

Bluetooth 4.0 最大变化是引入了 **BLE**，Bluetooth Low Energy，也叫低功耗蓝牙。

官方规格变更记录里也明确提到，Bluetooth 4.0 引入了 Bluetooth Low Energy，包括 LE PHY、LE Link Layer、ATT、GATT、GAP、SM 等 BLE 相关协议层。([Bluetooth® Technology Website][1])

BLE 和经典蓝牙区别很大：

| 对比   | 经典蓝牙 Bluetooth Classic | BLE                             |
| ---- | ---------------------- | ------------------------------- |
| 目标   | 音频、串口、大流量连续通信          | 低功耗、传感器、物联网                     |
| 连接方式 | 偏持续连接                  | 可广播、可短连接                        |
| 功耗   | 较高                     | 很低                              |
| 常见应用 | 耳机、音箱、SPP 串口           | 手环、传感器、配网、Beacon                |
| 协议模型 | SPP/A2DP/HFP 等         | GAP/GATT/Service/Characteristic |

你现在 ESP32 学 BLE 配网、GATT Server，其实主要就是 Bluetooth 4.0 之后的体系。

---

## 6. Bluetooth 5.x：距离、速度、广播能力增强

Bluetooth 5.x 主要强化了 BLE：

```text
2M PHY：提高 BLE 物理层速率
LE Coded PHY：提高通信距离
Extended Advertising：扩展广播
Periodic Advertising：周期广播
Direction Finding：方向定位，AOA/AOD
Isochronous Channels：支持 LE Audio
```

BLE 不再只是“小数据传感器”，后来也支持了 LE Audio、定位、广播网络等能力。

---

## 7. Bluetooth 6.x：继续增强定位、安全和链路能力

截至当前公开规格，Bluetooth Core Specification 已经到 **v6.1**，官方规格页面显示版本日期为 **2025-04-29**。([Bluetooth® Technology Website][2])

Bluetooth 6.0/6.1 这类新版本主要是继续增强 BLE 架构、通道探测、链路控制、安全性和效率，不是说普通 ESP32 立刻就都支持这些新功能。

这里要注意：

```text
蓝牙标准版本 ≠ 你的芯片都支持
```

比如 ESP32 原版支持经典蓝牙和 BLE，但 ESP32-S3 不支持经典蓝牙，只支持 BLE。

---

# 三、经典蓝牙和 BLE 的底层区别

你可以这么理解：

## 经典蓝牙：像“无线串口/无线音频线”

经典蓝牙更像一根无线数据线。

典型应用：

```text
蓝牙耳机
蓝牙音箱
蓝牙串口 SPP
蓝牙 HID 键盘鼠标
```

ESP-IDF 官方文档里，ESP32 经典蓝牙支持 GAP、L2CAP、SDP、SPP、A2DP、AVRCP、HFP、HID 等功能。([Espressif Systems][3])

如果你想做：

```text
ESP32 <-> 手机蓝牙串口
ESP32 <-> 电脑蓝牙串口
ESP32 <-> 另一个 ESP32 串口透传
```

经典蓝牙 SPP 比较像 UART。

---

## BLE：像“属性表 + 广播 + 低功耗数据通道”

BLE 不是传统串口思维，而是：

```text
设备广播自己
手机或主机扫描
建立连接
读取/写入服务 Service
读取/写入特征值 Characteristic
通过 Notify/Indicate 主动上报数据
```

ESP-IDF 支持 Bluedroid 和 NimBLE 两个蓝牙 Host 栈：Bluedroid 支持经典蓝牙和 BLE，NimBLE 是轻量级 BLE-only，适合资源紧张的 BLE 应用。([Espressif Systems][4])

---

# 四、你的问题重点：一个 ESP32 怎么给多台 ESP32 发数据？

这里要先分清楚：你说的“ESP32 给多台 ESP32 发数据”，不一定非要用蓝牙。

在 ESP32 之间通信，常见方案有 5 种：

```text
方案1：BLE 广播
方案2：BLE GATT 一主多从连接
方案3：经典蓝牙 SPP
方案4：ESP-NOW
方案5：Wi-Fi UDP/TCP
```

我按工程推荐程度讲。

---

# 五、方案 1：BLE 广播，一对多，最简单

## 适合场景

一个 ESP32 周期性发数据，多台 ESP32 只接收，不需要回复。

例如：

```text
主机发送温度
主机发送状态
主机发送设备编号
主机发送控制命令
多个从机监听广播
```

工作方式：

```text
ESP32 A：BLE Advertiser，不断广播数据
ESP32 B/C/D：BLE Scanner，扫描广播包
```

类似这样：

```text
        BLE Advertising
ESP32 A  ))))))))))))))))

ESP32 B  扫描接收
ESP32 C  扫描接收
ESP32 D  扫描接收
```

优点：

```text
不需要连接
天然一对多
功耗低
实现简单
```

缺点：

```text
广播包数据量小
可靠性不如连接
默认没有 ACK
不适合大数据连续传输
```

适合你发：

```text
状态
传感器小数据
控制命令
设备心跳
```

不适合发：

```text
图片
音频
大 JSON
大量日志
```

---

# 六、方案 2：BLE GATT 一主多从连接

## 适合场景

一个 ESP32 作为中心设备 Central，同时连接多个 BLE 外设 Peripheral。

```text
ESP32 主机 Central
    |
    |--- ESP32 从机1 Peripheral
    |--- ESP32 从机2 Peripheral
    |--- ESP32 从机3 Peripheral
```

数据可以通过：

```text
Write Characteristic
Notify Characteristic
Indicate Characteristic
```

优点：

```text
比广播可靠
可以双向通信
可以做 ACK
可以做设备管理
```

缺点：

```text
实现复杂
连接数量有限
调度复杂
吞吐量不高
多连接会增加延迟
```

适合：

```text
一个主控板管理多个 BLE 传感器节点
每个节点数据量不大
需要双向通信
```

不太适合：

```text
高频控制
大量实时数据
多设备高速同步
```

---

# 七、方案 3：经典蓝牙 SPP，不太适合一对多

如果你用的是原版 ESP32，经典蓝牙 SPP 可以做类似串口通信。

但是：

```text
经典蓝牙更适合一对一
一对多管理复杂
ESP32-S3 不支持经典蓝牙
```

ESP32-S3 不支持 Classic Bluetooth，这一点在工程上很关键。你现在常用 ESP32-S3 的话，就不要想着用 SPP 蓝牙串口了。

所以如果是 ESP32-S3：

```text
不能用经典蓝牙 SPP
只能用 BLE / Wi-Fi / ESP-NOW
```

---

# 八、方案 4：ESP-NOW，ESP32 给多台 ESP32 发数据最推荐

如果你的目标是：

```text
ESP32 设备之间通信
一对多
低延迟
不想连路由器
数据量中小
控制命令/传感器数据
```

那我最推荐：

```text
ESP-NOW
```

ESP-NOW 是乐鑫定义的无连接 Wi-Fi 通信协议，数据封装在 vendor-specific action frame 中，不需要先连接 Wi-Fi 路由器。官方文档也说明 ESP-NOW 是一种 connectionless Wi-Fi communication protocol。([Espressif Systems][5])

乐鑫官方也说明 ESP-NOW 支持 **one-to-many** 和 **many-to-many** 设备控制。([Espressif Systems][6])

它的结构类似这样：

```text
             ESP-NOW
ESP32 主机  ---------> ESP32 从机1
          \----------> ESP32 从机2
           \---------> ESP32 从机3
            \--------> ESP32 从机4
```

## ESP-NOW 的优点

```text
不需要路由器
延迟低
适合一对多
适合多 ESP32 之间通信
比 BLE 广播数据量更灵活
比 BLE GATT 简单
可以单播，也可以广播
```

## ESP-NOW 的缺点

```text
不是标准蓝牙，只有乐鑫生态好用
手机一般不能直接收 ESP-NOW
需要管理 Wi-Fi 信道
数据包大小有限
复杂网络需要自己设计协议
```

---

# 九、ESP-NOW 一对多有两种发法

## 方式 A：广播发送

发送到广播 MAC：

```text
FF:FF:FF:FF:FF:FF
```

所有同信道、正在监听的 ESP32 都能收到。

适合：

```text
主机发全局命令
比如：全部开灯
比如：全部进入 OTA
比如：全部同步时间
```

缺点：

```text
广播一般没有可靠 ACK
谁收到了、谁没收到，需要自己设计确认机制
```

---

## 方式 B：单播轮询多个 Peer

主机保存多个从机 MAC 地址，然后一个一个发：

```text
发给从机1
发给从机2
发给从机3
发给从机4
```

优点：

```text
可以知道每台设备发送是否成功
可以对每台设备发不同数据
可以加密
可靠性更好
```

官方旧版 ESP-NOW 文档里提到，发送前要 `esp_now_add_peer()` 添加 peer，配对设备列表最大数量是 20；广播发送也需要添加广播 MAC 设备。([Espressif Systems][7])

实际工程建议：

```text
设备少，用单播
设备很多，用广播 + 应用层 ACK
```

---

# 十、方案 5：Wi-Fi UDP/TCP

如果多台 ESP32 都连接到同一个路由器，也可以用 Wi-Fi。

## UDP 广播/组播

适合一对多：

```text
ESP32 主机 UDP broadcast
多台 ESP32 接收
```

优点：

```text
数据量比 BLE 大
实现不难
同局域网内很方便
手机/电脑也能参与
```

缺点：

```text
需要路由器或者 SoftAP
UDP 不保证必达
广播可能被路由器限制
```

## TCP 多客户端

主 ESP32 开 TCP Server，多台 ESP32 作为 TCP Client 连接。

优点：

```text
可靠
双向
适合需要确认的数据传输
```

缺点：

```text
连接管理复杂
多客户端占资源
延迟和重连处理要做好
```

---

# 十一、怎么选？直接给你工程建议

## 如果你是 ESP32 发给多台 ESP32

优先级我建议这样：

```text
第一推荐：ESP-NOW
第二推荐：Wi-Fi UDP
第三推荐：BLE 广播
第四推荐：BLE GATT 多连接
不太推荐：经典蓝牙 SPP 一对多
```

---

## 场景 1：小车/平衡车/电机控制节点

比如：

```text
一个遥控器 ESP32
多个执行节点 ESP32
低延迟控制
不经过路由器
```

选：

```text
ESP-NOW
```

原因：

```text
延迟低
不需要配网
一对多方便
比 BLE GATT 简单
```

---

## 场景 2：天气助手/家居传感器广播状态

比如：

```text
一个 ESP32 发温湿度
多个 ESP32 显示屏接收
不要求每包必达
```

选：

```text
BLE 广播 或 ESP-NOW 广播
```

如果只在 ESP32 之间用：

```text
ESP-NOW 更合适
```

如果还想让手机 App 扫到：

```text
BLE 广播更合适
```

---

## 场景 3：一个主控管理多个从设备，并且需要确认

比如：

```text
主机设置从机参数
从机回复状态
需要知道每台设备是否成功
```

选：

```text
ESP-NOW 单播多个 peer
```

或者：

```text
Wi-Fi TCP 多客户端
```

---

## 场景 4：需要手机也能参与

比如：

```text
手机 App 控制 ESP32
ESP32 再控制其他 ESP32
```

推荐架构：

```text
手机 <--BLE/Wi-Fi--> 主 ESP32 <--ESP-NOW--> 多个从 ESP32
```

也就是：

```text
手机负责用户界面
主 ESP32 做网关
从 ESP32 通过 ESP-NOW 接收控制
```

---

# 十二、我最推荐你的架构

你现在经常搞 ESP32-S3、LVGL、天气助手、平衡车、FOC、传感器这些东西。

如果你要做 **一个 ESP32 给多台 ESP32 发数据**，我建议你优先用这个架构：

```text
                ESP-NOW
主 ESP32  -----------------> 从 ESP32 #1
        \------------------> 从 ESP32 #2
         \-----------------> 从 ESP32 #3
          \----------------> 从 ESP32 #4
```

数据包自己定义一个结构体：

```c
typedef struct {
    uint8_t  cmd;        // 命令类型
    uint8_t  target_id;  // 目标设备ID，0xFF 表示广播给所有设备
    uint16_t seq;        // 包序号，用来判断丢包
    float    value1;     // 数据1
    float    value2;     // 数据2
    uint8_t  checksum;   // 简单校验
} espnow_packet_t;
```

比如命令类型：

```c
typedef enum {
    CMD_LED_ON      = 1,
    CMD_LED_OFF     = 2,
    CMD_MOTOR_START = 3,
    CMD_MOTOR_STOP  = 4,
    CMD_SYNC_TIME   = 5,
    CMD_SENSOR_REQ  = 6,
} cmd_type_t;
```

这样比直接发字符串更适合嵌入式。

---

# 十三、ESP-NOW 一对多基本流程

## 主机端

```text
1. 初始化 Wi-Fi
2. 设置 STA 模式
3. 初始化 ESP-NOW
4. 注册发送回调
5. 添加多个 peer
6. 循环发送数据
```

## 从机端

```text
1. 初始化 Wi-Fi
2. 设置 STA 模式
3. 初始化 ESP-NOW
4. 注册接收回调
5. 收到数据后解析结构体
6. 根据 cmd 执行动作
```

---

# 十四、BLE 和 ESP-NOW 的本质区别

你可以这样理解：

```text
BLE 是标准蓝牙协议，手机兼容性好
ESP-NOW 是乐鑫私有协议，ESP32 之间通信爽
```

| 对比        | BLE 广播 | BLE GATT | ESP-NOW | Wi-Fi UDP    |
| --------- | ------ | -------- | ------- | ------------ |
| 是否需要连接    | 不需要    | 需要       | 不需要     | 通常需要联网/同网段   |
| 一对多       | 很适合    | 可以但复杂    | 很适合     | 很适合          |
| 手机兼容      | 好      | 好        | 差       | 好            |
| ESP32 间通信 | 可以     | 可以       | 非常适合    | 适合           |
| 延迟        | 低到中    | 中        | 低       | 低到中          |
| 可靠性       | 一般     | 较好       | 中到好     | UDP 一般/TCP 好 |
| 实现复杂度     | 中      | 高        | 中       | 中            |
| 数据量       | 小      | 小到中      | 小到中     | 中到大          |

---

# 十五、最终建议

如果你问的是：

```text
一个 ESP32 怎么给多台 ESP32 发数据？
```

我的建议是：

```text
不要优先用蓝牙。
ESP32 对 ESP32，一对多通信，优先用 ESP-NOW。
```

如果你还想手机也能控制，那么用：

```text
手机 <--BLE 或 Wi-Fi--> 主 ESP32
主 ESP32 <--ESP-NOW--> 多台从 ESP32
```

这套架构很适合：

```text
遥控器
小车
平衡车
多传感器节点
多灯控制
智能家居节点
无线按键
低延迟控制
```

简单一句话：

```text
蓝牙适合“手机/耳机/标准设备互联”；
ESP-NOW 更适合“多个 ESP32 之间直接通信”。
```

[1]: https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/Core-61/out/en/architecture%2C-change-history%2C-and-conventions/core-specification-change-history.html?utm_source=chatgpt.com "Part C Core Specification Change History"
[2]: https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/Core-61/out/en/index-en.html?utm_source=chatgpt.com "Bluetooth Core Specification"
[3]: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/bluetooth/classic_bt.html?utm_source=chatgpt.com "Bluetooth® Classic - ESP32 - — ESP-IDF Programming ..."
[4]: https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/bluetooth/index.html?utm_source=chatgpt.com "Bluetooth® API - ESP32 - — ESP-IDF Programming Guide ..."
[5]: https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/network/esp_now.html?utm_source=chatgpt.com "ESP-NOW - ESP32 - — ESP-IDF Programming Guide v6. ..."
[6]: https://www.espressif.com/en/solutions/low-power-solutions/esp-now?utm_source=chatgpt.com "ESP-NOW Wireless Communication Protocol"
[7]: https://docs.espressif.com/projects/esp-idf/en/v3.3/api-reference/network/esp_now.html?utm_source=chatgpt.com "ESP-NOW — ESP-IDF Programming Guide v3.3 ..."


----
---
----


下面这个表按 **蓝牙核心协议版本** 来整理。注意一个很重要的点：

**蓝牙版本号 ≠ 一定支持某个具体功能。**
比如设备写着 Bluetooth 5.2，不代表它一定支持 LE Audio；写着 Bluetooth 5.1，也不代表一定支持方向定位。Bluetooth SIG 也在强调，产品更应该标具体功能，而不是只看版本号。([株式会社ムセンコネクト][1])

---

# 1. 蓝牙协议版本发展表

| 蓝牙版本                |    大致时期 | 核心特点                                        | 主要解决的问题                  | 典型应用                  |
| ------------------- | ------: | ------------------------------------------- | ------------------------ | --------------------- |
| Bluetooth 1.0 / 1.1 | 1999 左右 | 最早期标准，支持短距离无线连接                             | 替代串口线、数据线                | 早期手机、电脑、PDA           |
| Bluetooth 1.2       | 2003 左右 | 引入 AFH 自适应跳频，抗干扰更好                          | 减少和 Wi-Fi 等 2.4GHz 设备的干扰 | 早期蓝牙耳机、鼠标、手机          |
| Bluetooth 2.0 + EDR | 2004 左右 | 加入 EDR，Enhanced Data Rate，速率提升              | 传输速度更高                   | 蓝牙耳机、音箱、文件传输、SPP 串口   |
| Bluetooth 2.1 + EDR | 2007 左右 | 加入 SSP，Secure Simple Pairing，配对更简单          | 解决配对麻烦、安全性弱的问题           | 手机、电脑、蓝牙外设            |
| Bluetooth 3.0 + HS  | 2009 左右 | High Speed，高速模式，可借助 802.11 传大数据             | 想提升大文件传输速度               | 文件传输，但实际普及一般          |
| Bluetooth 4.0       | 2010 左右 | **引入 BLE，Bluetooth Low Energy**             | 低功耗物联网设备                 | 手环、传感器、Beacon、BLE 配网  |
| Bluetooth 4.1       | 2013 左右 | BLE 连接能力增强，设备角色更灵活                          | BLE 多角色、多连接改进            | 物联网设备                 |
| Bluetooth 4.2       | 2014 左右 | BLE 数据包长度、安全、IPv6 支持增强                      | BLE 吞吐和安全性提升             | 智能家居、可穿戴              |
| Bluetooth 5.0       | 2016 左右 | BLE 增强：2M PHY、Coded PHY、扩展广播                | 提升 BLE 速度、距离、广播能力        | 长距离传感器、Beacon、低功耗设备   |
| Bluetooth 5.1       | 2019 左右 | 引入 Direction Finding，AOA/AOD 方向定位           | 蓝牙定位精度增强                 | 室内定位、资产追踪             |
| Bluetooth 5.2       | 2020 左右 | 引入 LE Isochronous Channels，支持 LE Audio 基础能力 | 让 BLE 支持低功耗音频            | LE Audio、助听器、Auracast |
| Bluetooth 5.3       | 2021 左右 | 连接效率、广播、加密 key size 控制等改进                   | 降功耗、提升链路效率               | 耳机、手机、IoT             |
| Bluetooth 5.4       | 2023 左右 | PAwR、Encrypted Advertising Data 等           | 更适合大规模低功耗广播网络            | 电子价签 ESL、低功耗广播系统      |
| Bluetooth 6.0       | 2024 左右 | Channel Sounding，距离测量/高精度测距能力增强             | 更精确的距离感知和定位              | 数字钥匙、寻物、定位            |
| Bluetooth 6.1       |    2025 | 在 6.x 基础上继续增强规范和实现细节                        | 规范迭代、安全和链路优化             | 新一代 BLE 设备            |

Bluetooth Core Specification 6.1 已由 Bluetooth SIG 发布，官方页面说明它定义了创建可互操作蓝牙设备所需的技术；Bluetooth SIG 也在 2025 年发布文章介绍 6.1 到来。([Bluetooth® Technology Website][2])

---

# 2. 蓝牙发展主线：其实分成两条路线

蓝牙不是简单地“版本越高，速度越快”。它的发展大致可以分成两条主线：

```text
Bluetooth Classic 经典蓝牙
    主要用于：音频、串口、大流量连续连接
    典型：耳机、音箱、SPP 串口、车载蓝牙

Bluetooth Low Energy / BLE 低功耗蓝牙
    从 Bluetooth 4.0 开始出现
    主要用于：低功耗、短数据、传感器、配网、Beacon
    典型：手环、温湿度传感器、智能门锁、BLE 配网
```

也就是说：

```text
1.x / 2.x / 3.x：主要是经典蓝牙时代
4.0：BLE 出现，蓝牙进入低功耗物联网时代
5.x：BLE 能力大幅增强
6.x：继续增强定位、测距、安全和效率
```

---

# 3. 经典蓝牙和 BLE 的关系

| 对比项         | 经典蓝牙 Bluetooth Classic  | BLE Bluetooth Low Energy |
| ----------- | ----------------------- | ------------------------ |
| 出现时间        | 早期蓝牙就有                  | Bluetooth 4.0 引入         |
| 设计目标        | 连续数据流、音频、串口             | 低功耗、小数据、传感器              |
| 典型协议        | SPP、A2DP、HFP、HID        | GAP、GATT、ATT、SMP         |
| 连接模型        | 偏“一直连着”                 | 可以广播，也可以短连接              |
| 功耗          | 较高                      | 很低                       |
| 速率          | 经典蓝牙 EDR 物理层可到 2/3 Mbps | BLE 1M、2M、Coded PHY      |
| 手机兼容        | 老设备兼容好                  | 新设备普遍支持                  |
| ESP32 支持    | 原版 ESP32 支持             | 原版 ESP32、ESP32-S3 等支持    |
| ESP32-S3 支持 | **不支持经典蓝牙**             | **支持 BLE**               |

ESP-IDF 官方文档中，经典蓝牙部分列出了 ESP32 支持 GAP、L2CAP、SDP、SPP、A2DP、AVRCP、HFP、HID 等经典蓝牙协议；而 ESP-IDF 蓝牙总览也说明 Bluedroid 支持经典蓝牙和 BLE，NimBLE 是 BLE-only Host 栈。

---

# 4. 重点版本详细理解

## Bluetooth 1.x：能用，但早期体验一般

早期蓝牙主要目标是替代线缆。

比如：

```text
手机 ↔ 电脑
手机 ↔ 耳机
电脑 ↔ 鼠标键盘
```

但早期蓝牙有几个问题：

```text
连接慢
配对麻烦
兼容性一般
抗干扰能力较弱
```

Bluetooth 1.2 加入 AFH 自适应跳频后，抗干扰能力更好。

---

## Bluetooth 2.x：经典蓝牙开始成熟

Bluetooth 2.0 + EDR 是经典蓝牙很重要的一步。

EDR 的意思是：

```text
Enhanced Data Rate
增强数据率
```

它让经典蓝牙的数据速率更高，更适合：

```text
蓝牙耳机
蓝牙音箱
串口透传
文件传输
```

Bluetooth 2.1 + EDR 又加入 SSP，也就是 Secure Simple Pairing，让配对更简单、安全性更好。2007 年 Bluetooth SIG 正式批准 Bluetooth 2.1 + EDR，当时重点就是更容易配对和降低功耗。([WIRED][3])

---

## Bluetooth 3.0：想做高速，但实际影响有限

Bluetooth 3.0 + HS 的 HS 是 High Speed。

它的想法是：

```text
蓝牙负责发现、连接、控制
真正大数据传输借助 802.11 高速链路
```

但是实际工程里，这个版本存在感不如 4.0 BLE。

因为后面市场真正爆发的方向不是“蓝牙传大文件”，而是：

```text
低功耗传感器
智能手环
智能家居
Beacon
```

这些都是 BLE 的地盘。

---

## Bluetooth 4.0：BLE 出现，最重要的分水岭

Bluetooth 4.0 最大意义就是引入 BLE。

BLE 的思路和经典蓝牙不一样。

经典蓝牙像：

```text
一根无线串口线 / 无线音频线
```

BLE 更像：

```text
设备广播自己
中心设备扫描
连接后访问服务 Service
读写特征值 Characteristic
```

比如一个 BLE 温湿度传感器：

```text
Service：环境传感器服务
Characteristic：温度
Characteristic：湿度
Characteristic：电池电量
```

ESP32 做 BLE GATT Server 的时候，其实就是在建这个“属性表”。

---

## Bluetooth 5.0：BLE 距离、速度、广播增强

Bluetooth 5.0 对 BLE 很关键，主要增强了三个方向：

| 功能                   | 作用          | 你可以怎么理解           |
| -------------------- | ----------- | ----------------- |
| LE 2M PHY            | BLE 物理层速率提升 | 比传统 BLE 1M PHY 更快 |
| LE Coded PHY         | 远距离通信       | 降低速率，换更强抗干扰和更远距离  |
| Extended Advertising | 扩展广播        | 广播能力更强，数据承载更灵活    |

Bluetooth 5.0 加入了 2M PHY 和 Coded PHY，其中 Coded PHY 通过前向纠错等方式，用更低速率换更远距离和更强可靠性。([Macnica][4])

简单理解：

```text
2M PHY：跑得快
Coded PHY：跑得远
Extended Advertising：广播能力更强
```

---

## Bluetooth 5.1：方向定位

Bluetooth 5.1 主要新增方向定位能力，常见关键词是：

```text
AOA：Angle of Arrival，到达角
AOD：Angle of Departure，出发角
Direction Finding：方向寻找
```

它解决的问题是：

```text
不只是知道“远近”
还想知道“方向”
```

典型场景：

```text
室内定位
仓库资产追踪
找东西
导航
```

但注意：
**设备标 Bluetooth 5.1，不等于一定支持 AOA/AOD。**
方向定位需要芯片、射频、天线阵列、协议栈、算法一起支持。

---

## Bluetooth 5.2：LE Audio 的基础

Bluetooth 5.2 很重要，因为它引入了 LE Isochronous Channels，也就是 LE 等时通道。

这为 LE Audio 打基础。

官方 LE Audio FAQ 说明，LE Audio 需要 Bluetooth Core 5.2 引入的核心功能，也就是支持 BLE 上等时传输的 LE Isochronous Channels。([Bluetooth® Technology Website][5])

你可以这样理解：

```text
经典蓝牙音频：A2DP / HFP
BLE 音频新时代：LE Audio
```

LE Audio 不是简单的“蓝牙版本高一点”，它涉及：

```text
LE Isochronous Channels
LC3 编解码器
低功耗音频
广播音频 Auracast
助听器支持
```

所以买耳机不能只看 Bluetooth 5.2，还要看它是否明确支持：

```text
LE Audio
LC3
Auracast
```

---

## Bluetooth 5.3：效率优化

Bluetooth 5.3 不是那种“革命性变化”，更多是细节增强：

```text
降低功耗
提升连接效率
优化广播
改善链路控制
```

很多手机、耳机、芯片会标 Bluetooth 5.3，但你不能简单理解成：

```text
5.3 音质一定比 5.2 好
```

音质更多取决于：

```text
编码器：SBC / AAC / aptX / LDAC / LC3
射频设计
天线
协议栈
耳机声学设计
```

---

## Bluetooth 5.4：面向大规模低功耗广播

Bluetooth 5.4 里比较重要的是：

```text
PAwR：Periodic Advertising with Responses
Encrypted Advertising Data：加密广播数据
```

它更适合：

```text
电子价签 ESL
大量低功耗节点
低功耗广播网络
```

比如商场里成千上万个电子价签，不可能每个都长期连接。
这类场景更适合广播 + 响应机制。

---

## Bluetooth 6.0：Channel Sounding，高精度距离测量

Bluetooth 6.0 重点之一是 Channel Sounding。

它可以让设备之间做更精确的距离测量。The Verge 对 Bluetooth 6.0 的报道中提到，Bluetooth SIG 发布 Bluetooth 6.0，其中 Channel Sounding 可通过相位测距等方式实现厘米级距离测量，用于寻物网络、数字钥匙等场景。([The Verge][6])

适合场景：

```text
数字车钥匙
门锁靠近检测
防中继攻击
寻物标签
室内定位
```

简单说：

```text
Bluetooth 5.1 更强调“方向”
Bluetooth 6.0 更强调“精确距离”
```

---

# 5. 蓝牙版本之间的继承关系

可以这样画：

```text
Bluetooth 1.x
   |
   v
Bluetooth 2.x + EDR
   |
   v
Bluetooth 3.x + HS
   |
   +----------------------------+
   |                            |
   v                            v
经典蓝牙 Classic              Bluetooth 4.0 引入 BLE
A2DP / SPP / HFP              GAP / GATT / ATT
耳机 / 音箱 / 串口             低功耗传感器 / Beacon / 配网
                                |
                                v
                           Bluetooth 5.x
                           BLE 速度 / 距离 / 广播 / 音频增强
                                |
                                v
                           Bluetooth 6.x
                           测距 / 定位 / 安全 / 效率增强
```

核心理解：

```text
经典蓝牙不是被 BLE 完全替代。
BLE 是另一个低功耗方向。
很多设备可以同时支持 Classic + BLE。
```

比如手机通常同时支持：

```text
经典蓝牙：连耳机、音箱、车机
BLE：连手环、手表、传感器、配网
```

---

# 6. 对 ESP32 开发最重要的版本关系

你搞 ESP32，要重点记住这几个：

| 芯片/方向               | 重点                                   |
| ------------------- | ------------------------------------ |
| 原版 ESP32            | 支持经典蓝牙 + BLE                         |
| ESP32-S3            | 只支持 BLE，不支持经典蓝牙                      |
| ESP32-C3 / C6       | 主要是 BLE / Wi-Fi，不支持经典蓝牙              |
| BLE 配网              | 用 BLE GATT，不是经典蓝牙 SPP                |
| 手机通信                | BLE 更通用                              |
| ESP32 对 ESP32 多设备通信 | ESP-NOW 往往比 BLE 更合适                  |
| 蓝牙音频                | 原版 ESP32 可做经典蓝牙音频，ESP32-S3 不适合经典蓝牙音频 |

---

# 7. 版本选择建议

| 你要做的东西                | 推荐技术                                                             |
| --------------------- | ---------------------------------------------------------------- |
| ESP32 和手机传少量控制数据      | BLE GATT                                                         |
| ESP32 给手机做配网          | BLE Provisioning                                                 |
| ESP32 和多个 ESP32 发控制命令 | ESP-NOW                                                          |
| 蓝牙串口透传                | 原版 ESP32 + Classic SPP                                           |
| ESP32-S3 做蓝牙通信        | BLE，不要用 SPP                                                      |
| 低功耗传感器                | BLE 广播 / BLE GATT                                                |
| 多个设备广播状态              | BLE Advertising 或 ESP-NOW                                        |
| 蓝牙耳机/音箱               | Classic Audio 或 LE Audio，看芯片是否支持                                 |
| 室内定位                  | Bluetooth 5.1 Direction Finding 或 Bluetooth 6.0 Channel Sounding |

---

# 8. 最关键总结

你可以把蓝牙版本这样记：

```text
1.x：蓝牙诞生，替代线缆
2.x：EDR，提高经典蓝牙速度
3.x：HS，尝试高速传输
4.0：BLE 出现，低功耗物联网爆发
5.0：BLE 跑得更快、更远、广播更强
5.1：方向定位
5.2：LE Audio 基础
5.3：效率和连接优化
5.4：大规模低功耗广播网络
6.0：高精度测距 Channel Sounding
6.1：继续完善 6.x 规范
```

对你做 ESP32 来说，最实用的判断是：

```text
手机 ↔ ESP32：优先 BLE
ESP32 ↔ 多个 ESP32：优先 ESP-NOW
ESP32-S3：只能 BLE，不能经典蓝牙 SPP
原版 ESP32：经典蓝牙 + BLE 都能玩
```

[1]: https://www.musen-connect.co.jp/blog/course/other/communicating-supported-bluetooth-functionality-2025/?utm_source=chatgpt.com "【これはNG】Bluetoothのスペック表記ルールが変わりました ..."
[2]: https://www.bluetooth.com/specifications/specs/core-specification-6-1/?utm_source=chatgpt.com "Core Specification 6.1 Adopted"
[3]: https://www.wired.com/2007/08/bluetooth-21-fi?utm_source=chatgpt.com "Bluetooth 2.1 Finally Arrives"
[4]: https://www.macnica.co.jp/business/semiconductor/articles/cassia_networks/136502/?utm_source=chatgpt.com "Bluetooth Low Energy ver.5.0 (BLE5)について - 半導体事業"
[5]: https://www.bluetooth.com/wp-content/uploads/2020/03/LE-Audio_FAQ_JPN.pdf?utm_source=chatgpt.com "LE Audio の概要"
[6]: https://www.theverge.com/2024/9/5/24236565/bluetooth-6-security-precision-tracking-finding?utm_source=chatgpt.com "Bluetooth 6.0 has more security and precision for tracking and finding"


---------
----------
-------

下面我按**嵌入式开发视角**给你讲：蓝牙发展历史、经典蓝牙和低功耗蓝牙区别，以及 **ESP32 一个设备怎么通过蓝牙给多个 ESP32 发数据**。

---

# 1. 蓝牙是怎么来的？

蓝牙最早是为了解决一个问题：

> **不同设备之间，短距离无线通信，不想插线。**

比如手机、耳机、电脑、键盘、鼠标之间通信。

蓝牙名字 **Bluetooth** 来自丹麦国王 **Harald Bluetooth**，意思是“把不同部落连接起来”。蓝牙技术的寓意就是：

> 把不同厂商、不同设备统一连接起来。

早期主要由 Ericsson、Nokia、Intel、IBM、Toshiba 等公司推动，后来成立了 Bluetooth SIG 蓝牙联盟，统一蓝牙标准。

---

# 2. 蓝牙发展大致历史

## 蓝牙 1.x：能连，但慢

早期蓝牙主要解决“有没有无线连接”的问题。

典型应用：

```text
手机传文件
无线耳机
无线鼠标键盘
串口透传
```

缺点：

```text
速率低
连接慢
功耗不算低
抗干扰一般
```

---

## 蓝牙 2.0 + EDR：经典蓝牙成熟

EDR = Enhanced Data Rate，增强速率。

这一代开始，经典蓝牙比较成熟了。

典型应用：

```text
蓝牙耳机
蓝牙音箱
车载蓝牙
蓝牙串口 SPP
```

经典蓝牙很多音频场景就是从这里发展起来的。

---

## 蓝牙 3.0：高速蓝牙概念

蓝牙 3.0 引入高速传输思路，但实际嵌入式开发中你不一定常用。

---

## 蓝牙 4.0：BLE 低功耗蓝牙出现

这是非常关键的一代。

蓝牙 4.0 引入了：

```text
BLE：Bluetooth Low Energy
低功耗蓝牙
```

从这一代开始，蓝牙分成两条路线：

```text
经典蓝牙 Classic Bluetooth
低功耗蓝牙 BLE
```

BLE 主要面向：

```text
手环
传感器
遥控器
体温计
智能门锁
低功耗 IoT 设备
```

特点是：

```text
不需要一直保持大流量连接
可以睡眠
间隔广播
低功耗
小数据包
```

---

## 蓝牙 5.x：BLE 增强

蓝牙 5.x 主要增强 BLE 能力：

```text
更远距离
更高速率
更强广播能力
更好的多设备能力
```

比如：

```text
BLE 2M PHY
Long Range
扩展广播
周期广播
BLE Mesh
```

但是注意：**不是所有 ESP32 都支持所有蓝牙 5.x 特性**，要看芯片型号。

---

# 3. 经典蓝牙是什么？

经典蓝牙一般指：

```text
BR / EDR
Basic Rate / Enhanced Data Rate
```

它适合：

```text
连续数据流
音频
串口透传
较高吞吐
持续连接
```

比如：

```text
蓝牙耳机
蓝牙音箱
车载蓝牙
蓝牙串口 SPP
```

对于 ESP32 来说，经典蓝牙常见玩法是：

```text
SPP 蓝牙串口
A2DP 蓝牙音频
HFP 蓝牙通话相关
```

你可以把经典蓝牙 SPP 理解成：

```text
无线串口
```

MCU 之间如果只是传普通数据，经典蓝牙 SPP 很容易理解：

```text
ESP32 A 发送数据 -> 蓝牙 SPP -> ESP32 B 接收数据
```

---

# 4. BLE 低功耗蓝牙是什么？

BLE 是低功耗蓝牙，核心不是“无线串口”，而是：

```text
广播 + 连接 + 服务 + 特征值
```

BLE 里面常见概念：

```text
Peripheral  外设
Central     中心设备
Service     服务
Characteristic 特征值
Notify      通知
Write       写入
Read        读取
Advertisement 广播
```

举个例子：

一个温湿度传感器 ESP32：

```text
Service: 环境传感器服务
Characteristic: 温度
Characteristic: 湿度
Characteristic: 电池电量
```

手机或者另一个 ESP32 连接它之后，可以：

```text
读取温度
读取湿度
订阅 Notify
写入配置参数
```

---

# 5. 经典蓝牙 vs BLE 区别

## 核心区别

| 对比项        | 经典蓝牙      | BLE 低功耗蓝牙         |
| ---------- | --------- | ----------------- |
| 主要目标       | 连续数据传输    | 低功耗、小数据           |
| 典型应用       | 音频、串口、文件  | 传感器、手环、IoT        |
| 功耗         | 较高        | 很低                |
| 数据模型       | 更像流式通信    | GATT 服务/特征值       |
| 连接方式       | 点对点为主     | 广播、连接、多角色         |
| 音频支持       | 传统蓝牙音频    | BLE Audio 是新体系    |
| ESP32 常用协议 | SPP、A2DP  | GATT、Notify、Write |
| 开发难度       | SPP 比较像串口 | GATT 模型要理解        |

---

# 6. 用嵌入式思维理解两者

## 经典蓝牙更像 UART

比如经典蓝牙 SPP：

```text
ESP32 A printf("hello")
        |
        v
蓝牙 SPP
        |
        v
ESP32 B 收到 "hello"
```

你可以把它理解成：

```text
无线串口
```

适合：

```text
持续发送日志
较大数据
简单点对点通信
```

---

## BLE 更像“无线寄存器表”

BLE 的 GATT 更像这样：

```c
struct ble_device {
    uint8_t led_state;
    int16_t temperature;
    int16_t humidity;
    uint8_t battery;
};
```

手机或者另一个 ESP32 可以：

```text
读 temperature
写 led_state
订阅 battery notify
```

所以 BLE 更像：

```text
远程访问某些变量
```

不是传统意义上的“串口流”。

---

# 7. ESP32 蓝牙能力要注意

经典 ESP32，比如 ESP32-WROOM-32，通常支持：

```text
Wi-Fi
Classic Bluetooth
BLE
```

但是一些型号不一样：

| 芯片       |    经典蓝牙 |               BLE |
| -------- | ------: | ----------------: |
| ESP32    |      支持 |                支持 |
| ESP32-S3 | 不支持经典蓝牙 |            支持 BLE |
| ESP32-C3 | 不支持经典蓝牙 |            支持 BLE |
| ESP32-C6 | 不支持经典蓝牙 |            支持 BLE |
| ESP32-H2 | 不支持经典蓝牙 | 支持 BLE / 802.15.4 |

所以你如果用的是 **ESP32-S3**，一般不能用经典蓝牙 SPP，只能用 BLE。

---

# 8. 一个 ESP32 怎么给多个 ESP32 发数据？

你这句“通过蓝牙黑其他多个设备发数据”，我理解你是想说：

> 一个 ESP32 通过蓝牙和其他多个 ESP32 通信，给多个设备发数据。

这个有几种方案。

---

# 方案一：BLE 广播模式，一发多收

这是最简单的“一对多”。

## 原理

一个 ESP32 不建立连接，只是不断广播数据：

```text
主 ESP32：广播数据
    |
    |---- ESP32 设备 1 扫描收到
    |---- ESP32 设备 2 扫描收到
    |---- ESP32 设备 3 扫描收到
    |---- ESP32 设备 4 扫描收到
```

类似：

```text
电台广播
```

发的人不管谁收到了。

## 优点

```text
一对多简单
不需要连接
功耗可以低
多个设备都能收到
主设备压力小
```

## 缺点

```text
不能保证一定收到
数据量小
没有可靠 ACK
不适合大数据
不适合强实时闭环控制
```

## 适合场景

```text
同步时间
广播状态
广播控制命令
简单传感器数据
附近设备发现
```

比如你要让多个 ESP32 同时收到：

```text
开灯
关灯
切换模式
同步时间
设备状态广播
```

BLE 广播很合适。

---

# 方案二：BLE Central 连接多个 Peripheral

这是比较正规的多设备通信方案。

## 架构

一个 ESP32 当中心设备 Central：

```text
ESP32 主机：Central
```

多个 ESP32 当外设 Peripheral：

```text
ESP32 从机 1：Peripheral
ESP32 从机 2：Peripheral
ESP32 从机 3：Peripheral
```

连接关系：

```text
          BLE连接1
主 ESP32  --------  从 ESP32 1

          BLE连接2
主 ESP32  --------  从 ESP32 2

          BLE连接3
主 ESP32  --------  从 ESP32 3
```

主设备可以分别给每个设备写 Characteristic：

```text
write device 1
write device 2
write device 3
```

从设备也可以通过 Notify 回复数据。

---

## 优点

```text
通信可靠性更好
可以双向通信
可以知道哪个设备在线
可以做 ACK/重发
可以分设备控制
```

## 缺点

```text
开发复杂
连接数量有限
连接越多，延迟越大
需要管理连接状态
```

---

# 方案三：一个 ESP32 做 BLE Peripheral，多个手机/ESP32 连接它

也可以反过来：

```text
ESP32 主设备做 Peripheral
其他设备做 Central
```

但是 BLE 里面更常见的是：

```text
手机 / 主控设备 = Central
传感器 / 从设备 = Peripheral
```

ESP32 也可以多角色，但要看 ESP-IDF 配置和资源。

---

# 方案四：BLE Mesh

如果你是真的想做很多设备，比如几十个、上百个节点，可以考虑：

```text
BLE Mesh
```

拓扑大概是：

```text
节点1 <-> 节点2 <-> 节点3 <-> 节点4
```

特点：

```text
多跳转发
适合灯控
适合楼宇设备
适合大规模节点
```

缺点：

```text
协议复杂
调试复杂
学习成本高
不适合新手一上来就搞
```

---

# 方案五：ESP-NOW 其实更适合多个 ESP32

虽然你问的是蓝牙，但如果你的设备全部都是 ESP32，我反而更推荐你了解：

```text
ESP-NOW
```

ESP-NOW 是 Espressif 自家的无线通信协议，基于 Wi-Fi PHY，不需要路由器。

它非常适合：

```text
一个 ESP32 给多个 ESP32 发数据
低延迟
一对多
遥控
传感器网络
小车控制
平衡车遥控
```

拓扑：

```text
主 ESP32
  |---- ESP32 1
  |---- ESP32 2
  |---- ESP32 3
  |---- ESP32 4
```

优点：

```text
比 BLE 广播更适合 ESP32 多设备通信
延迟低
开发相对简单
支持广播和单播
可以做 ACK
```

如果不是必须兼容手机蓝牙，ESP32 多设备之间通信，我更推荐：

```text
ESP-NOW > BLE > 经典蓝牙
```

---

# 9. 三种方案怎么选？

## 你只是想一个 ESP32 发命令给多个 ESP32

比如：

```text
开灯
关灯
开始采集
停止采集
切换模式
同步状态
```

推荐：

```text
BLE 广播
或者 ESP-NOW 广播
```

更推荐：

```text
ESP-NOW
```

---

## 你想一对多，而且要知道每个设备有没有收到

推荐：

```text
BLE Central 多连接
或者 ESP-NOW 单播 + ACK
```

如果全是 ESP32：

```text
ESP-NOW 更方便
```

---

## 你想兼容手机

比如手机 App 也要连：

```text
手机 <-> ESP32
```

推荐：

```text
BLE GATT
```

---

## 你想传音频

推荐：

```text
经典蓝牙 A2DP
```

但 ESP32-S3 不支持经典蓝牙音频。

---

## 你想像串口一样传数据

如果是普通 ESP32：

```text
经典蓝牙 SPP
```

如果是 ESP32-S3：

```text
BLE UART Service
```

比如自定义一个：

```text
TX Characteristic: Notify
RX Characteristic: Write
```

模拟蓝牙串口。

---

# 10. ESP32 一对多 BLE 通信推荐架构

假设你有：

```text
1 个主 ESP32
5 个从 ESP32
```

我建议这样设计：

```text
主 ESP32：BLE Central
从 ESP32：BLE Peripheral
```

每个从设备暴露一个服务：

```text
Service UUID: 自定义服务
Characteristic 1: RX，主机写入命令
Characteristic 2: TX，从机 Notify 上传数据
Characteristic 3: Device Info，设备信息
```

通信流程：

```text
1. 从 ESP32 开机后开始广播
2. 主 ESP32 扫描附近设备
3. 根据设备名 / MAC / Service UUID 筛选目标设备
4. 主 ESP32 分别连接多个从设备
5. 主 ESP32 对每个从设备写 RX Characteristic
6. 从 ESP32 收到命令后执行
7. 从 ESP32 用 Notify 返回结果
```

大概像这样：

```text
                 Write CMD
主 ESP32 Central  --------->  从 ESP32 1 Peripheral
                 <---------  Notify ACK

                 Write CMD
主 ESP32 Central  --------->  从 ESP32 2 Peripheral
                 <---------  Notify ACK

                 Write CMD
主 ESP32 Central  --------->  从 ESP32 3 Peripheral
                 <---------  Notify ACK
```

---

# 11. BLE 一对多通信的数据包建议

不要直接乱发字符串，建议做协议包。

比如：

```c
typedef struct {
    uint8_t  header;      // 帧头，比如 0xAA
    uint8_t  dev_id;      // 目标设备ID
    uint8_t  cmd;         // 命令
    uint8_t  len;         // 数据长度
    uint8_t  payload[20]; // 数据
    uint8_t  checksum;    // 校验
} ble_packet_t;
```

比如主设备发：

```text
AA 01 10 01 01 CS
```

含义：

```text
AA      帧头
01      设备 ID
10      命令：控制 LED
01      长度
01      打开
CS      校验
```

---

# 12. BLE 多设备连接要注意什么？

## 1. 连接数量有限

ESP32 同时连接多少个 BLE 设备，跟下面有关：

```text
芯片型号
ESP-IDF 配置
内存大小
NimBLE / Bluedroid 协议栈
连接间隔
MTU
任务优先级
```

不要一上来就想连几十个。

新手建议：

```text
先做 1 对 1
再做 1 对 2
再做 1 对 3
```

---

## 2. 连接间隔影响延迟

BLE 不是像 UART 一样一直发。

BLE 有连接间隔：

```text
connection interval
```

比如：

```text
20ms
30ms
50ms
100ms
```

连接越多，调度越复杂。

如果你连 5 个设备，每个设备都要频繁发数据，延迟会增加。

---

## 3. 单包数据不要太大

BLE 默认 ATT MTU 较小。

常见情况下一个 Notify 包有效数据可能是：

```text
20 字节左右
```

可以通过协商 MTU 增大，但不要一开始就依赖大包。

---

## 4. 不适合高实时控制

比如：

```text
平衡车姿态闭环
FOC 电机实时控制
高速 IMU 数据流
```

这种不要用 BLE 做实时闭环控制。

BLE 更适合：

```text
配置参数
状态上报
低速控制
传感器数据
```

---

# 13. 经典蓝牙能不能一个 ESP32 连多个？

经典蓝牙理论上也有多连接概念，但对于 ESP32 开发来说，常见的 SPP 更适合：

```text
点对点
```

也就是：

```text
ESP32 A <-> ESP32 B
```

你想一个 ESP32 同时连接多个 ESP32，用经典蓝牙 SPP 会比较麻烦，不如 BLE Central 多连接，或者直接 ESP-NOW。

---

# 14. 最推荐你的路线

你是做 ESP32 多设备通信，我建议按这个路线学：

## 第一阶段：BLE 广播

先实现：

```text
ESP32 A 广播 "hello"
多个 ESP32 扫描到 "hello"
```

你会理解：

```text
advertising
scanning
广播包
扫描响应包
```

---

## 第二阶段：BLE 一对一 GATT

实现：

```text
ESP32 Central 写数据
ESP32 Peripheral 收到数据
Peripheral Notify 回 ACK
```

---

## 第三阶段：BLE 一对多

实现：

```text
Central 扫描多个 Peripheral
连接多个 Peripheral
分别 write
分别接收 notify
```

---

## 第四阶段：ESP-NOW 一对多

如果设备全是 ESP32，重点学这个。

你会发现很多场景：

```text
ESP-NOW 更简单、更直接、更适合 ESP32 群控
```

---

# 15. 简单结论

经典蓝牙：

```text
适合音频、串口、连续数据流
更像无线 UART
功耗较高
ESP32 支持，ESP32-S3 不支持经典蓝牙
```

BLE：

```text
适合低功耗、小数据、传感器、设备配置
基于广播、连接、Service、Characteristic
可以做一对多，但需要管理连接
```

ESP32 多设备通信：

```text
简单一对多广播：BLE 广播 / ESP-NOW 广播
可靠一对多通信：BLE Central 多连接 / ESP-NOW 单播 ACK
大量设备组网：BLE Mesh
全是 ESP32：优先考虑 ESP-NOW
```

你可以这样记：

```text
手机兼容选 BLE
音频串口选经典蓝牙
多个 ESP32 通信优先看 ESP-NOW
多个 BLE 设备管理选 Central 多连接
```

对于你这个需求，**一个 ESP32 给多个 ESP32 发数据**，我最推荐：

```text
第一选择：ESP-NOW
第二选择：BLE 广播
第三选择：BLE Central 多连接
```

如果你后面要做手机 App 也参与控制，那就选：

```text
BLE GATT
```

