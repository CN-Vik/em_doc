GAP 全称是 **Generic Access Profile，通用访问配置文件**。

严格说，**GAP 不是专门用来传业务数据的协议**，它主要负责：

> **设备怎么被发现、怎么建立连接、连接角色是什么、广播怎么发、扫描怎么扫、连接参数怎么管理。**

你可以把 GAP 理解成 BLE 里面的：

> **“找设备 + 连设备 + 管连接状态”的管理层。**

---

## 1. GAP 在 BLE 协议栈中的位置

```text
应用层
  │
GATT        ← 负责：数据长什么样，怎么读写特征值
  │
ATT         ← 负责：Read / Write / Notify 这些属性传输
  │
GAP         ← 负责：广播、扫描、发现、连接、角色
SMP         ← 负责：配对、绑定、加密
  │
L2CAP
  │
Link Layer
  │
PHY
```

简单说：

```text
GAP：管“能不能找到你、怎么连接你”
GATT：管“连接后传什么数据”
```

---

## 2. GAP 主要干这几件事

### ① 设备发现：Advertising / Scanning

BLE 设备要让别人发现自己，就要发广播包。

比如你的温湿度传感器：

```text
温湿度传感器  →  一直广播：我是一个 BLE 设备，我叫 TempSensor
```

手机或者另一个 MCU 扫描：

```text
手机 / ESP32 Central  →  扫描附近 BLE 广播包
```

这个过程就是 GAP 管的。

```text
Peripheral 设备发广播
        ↓
Central 设备扫描
        ↓
Central 发现目标设备
```

---

### ② 广播数据管理

BLE 广播包里面可以放一些基础信息，例如：

```text
设备名称
设备地址
服务 UUID
厂商自定义数据
是否可连接
发射功率
```

例如一个 BLE 设备广播出来：

```text
Device Name: ESP32_BLE_SENSOR
Service UUID: 0x180F
Manufacturer Data: 01 02 03 04
Connectable: Yes
```

这些都是 GAP 负责配置和管理的。

---

### ③ 设备角色管理

GAP 里面有几个重要角色。

最常见的是：

```text
Peripheral 外设
Central 中心设备
```

比如：

```text
手环 / 传感器 / 蓝牙灯 / 蓝牙体温计  → Peripheral
手机 / 平板 / 网关 / 主控 ESP32      → Central
```

关系大概是：

```text
Peripheral：负责广播，等别人连接
Central：负责扫描，主动连接别人
```

举例：

```text
温湿度传感器 Peripheral
        ↑
        │  广播
        │
手机 Central 扫描并连接
```

---

### ④ 建立连接

当 Central 扫描到目标 Peripheral 后，可以发起连接。

流程大概是：

```text
Peripheral 开始广播
        ↓
Central 扫描到广播
        ↓
Central 判断是目标设备
        ↓
Central 发起连接请求
        ↓
BLE 连接建立成功
```

这个连接建立过程，也是 GAP 的核心职责。

---

### ⑤ 连接参数管理

BLE 为啥省电？一个关键点就是连接不是一直连续传输，而是按 **连接事件** 间隔传输。

GAP 会涉及这些连接参数：

```text
Connection Interval   连接间隔
Slave Latency         从机延迟
Supervision Timeout   连接超时时间
```

比如：

```text
连接间隔 30ms：响应快，但功耗高
连接间隔 500ms：响应慢，但功耗低
```

对于低功耗设备，比如手环、传感器，经常会把连接间隔调大一点。

---

## 3. GAP 和 GATT 的区别

这个非常重要。

很多新手容易把 GAP 和 GATT 搞混。

```text
GAP：负责发现和连接
GATT：负责连接后的数据交互
```

举个温湿度传感器例子。

### GAP 做的事：

```text
设备名：ESP32_TEMP_SENSOR
开始广播
等待手机扫描
手机发现设备
手机连接设备
```

### GATT 做的事：

```text
定义温度 Service
定义温度 Characteristic
手机读取温度
设备 Notify 温度变化
```

可以这样记：

```text
GAP = 先让别人找到你，并连上你
GATT = 连上之后，真正传业务数据
```

---

## 4. 用生活例子理解 GAP

假设你开了一家店。

```text
GAP 就像：
店铺招牌
店铺地址
开门营业
顾客找店
顾客进店
确认顾客能不能进来
```

```text
GATT 就像：
顾客进店后
货架怎么摆
商品叫什么
价格是多少
顾客怎么拿商品
```

所以：

```text
GAP 管“进门之前和建立连接”
GATT 管“进门之后的数据访问”
```

---

## 5. BLE 通信完整流程中 GAP 的位置

一个 BLE 温湿度传感器完整流程：

```text
1. 设备初始化 BLE 协议栈
        ↓
2. 配置 GAP 广播参数
        ↓
3. 设置广播数据：设备名、服务 UUID 等
        ↓
4. 开始广播
        ↓
5. 手机扫描到设备
        ↓
6. 手机发起连接
        ↓
7. GAP 连接建立成功
        ↓
8. 手机发现 GATT 服务
        ↓
9. 手机读取 / 订阅 Characteristic
        ↓
10. 设备通过 Notify 上报温湿度
```

其中：

```text
1 ~ 7 主要是 GAP
8 ~ 10 主要是 GATT / ATT
```

---

## 6. GAP 常见动作总结

| GAP 动作               | 作用                       |
| -------------------- | ------------------------ |
| Advertising          | 广播，让别人发现自己               |
| Scanning             | 扫描，发现附近设备                |
| Connection           | 建立 BLE 连接                |
| Role 管理              | Peripheral / Central 角色  |
| Device Name          | 设置设备名称                   |
| Advertising Data     | 设置广播包内容                  |
| Connection Parameter | 设置连接间隔、超时等参数             |
| Pairing 触发           | 参与配对流程，但真正安全细节通常由 SMP 负责 |

---

## 7. 嵌入式开发里你一般怎么接触 GAP？

比如 ESP32 BLE 开发中，你经常会看到类似这些东西：

```c
esp_ble_gap_set_device_name("ESP32_BLE_SENSOR");
esp_ble_gap_config_adv_data(&adv_data);
esp_ble_gap_start_advertising(&adv_params);
```

这些就是 GAP 相关接口。

它们分别表示：

```text
设置 BLE 设备名
配置广播数据
开始广播
```

还有 GAP 事件回调，比如：

```text
广播数据设置完成
广播开始成功
有设备连接
有设备断开
连接参数更新
扫描到设备
```

---

## 8. 一句话总结

**GAP 负责 BLE 设备的“发现、广播、扫描、连接、角色和连接参数管理”。**

你可以这样记：

```text
GAP：管连接前后的管理流程
GATT：管连接后的数据读写
ATT：管底层属性读写命令
SMP：管配对、绑定、加密
```

最重要的一句话：

> **BLE 设备能不能被手机发现、手机怎么连上它，这一套主要就是 GAP 在管。**
