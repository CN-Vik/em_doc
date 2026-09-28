### 结论：分区表不仅严重重叠，而且几乎浪费了一半的 8MB 空间，必须重改。

---

### 一、当前分区表的致命错误与浪费

查看你的启动分区表打印：

```text
I (69) boot:  0 nvs              WiFi data        01 02 00009000 00004000
I (77) boot:  1 otadata          OTA data         01 00 0000d000 00002000
I (84) boot:  2 phy_init         RF data          01 01 0000f000 00001000
I (92) boot:  3 ota_0            OTA app          00 10 00020000 001e0000
I (99) boot:  4 ota_1            OTA app          00 11 00200000 001e0000
I (107) boot:  5 fctry           WiFi data        01 02 003e0000 00006000

```

1. **`ota_0` 与 `ota_1` 严重重叠（致命隐患）**：
* `ota_0` 起始地址是 `0x20000`，长度给的是 `0x1E0000`（1920 KB）。
* $0x20000 + 0x1E0000 = 0x200000$。
* 而 `ota_1` 的起始地址刚好就是 `0x200000`。
* 当固件只要超过 **1.875 MB** 时，写 `ota_0` 就会直接**踩坏写死 `ota_1**`，导致 OTA 校验崩溃。


2. **Flash 空间浪费了近一半**：
* 芯片物理 Flash 是 **8MB（`0x800000`）**。
* 目前分区表只规划到了 `0x3E6000`（不足 4MB），**后半截近 4.1MB 的 Flash 完全空置被扔掉了**。
* 你的当前编译固件大小已经有 **1.27 MB**（还在持续集成 FOC、网络、RainMaker、TCP），随着功能膨胀，1.875MB 的单分区很快就会撑爆。



---

### 二、8MB Flash 标准双 OTA 分区表推荐

将你的 `partitions.csv` 改为以下配置，给每个 App 预留高达 **3MB** 的充足空间，并留出存放参数与证书的独立区域：

```csv
# Name,    Type, SubType, Offset,   Size,     Flags
nvs,       data, nvs,     0x9000,   0x6000,
otadata,   data, ota,     0xf000,   0x2000,
phy_init,  data, phy,     0x11000,  0x1000,
ota_0,     app,  ota_0,   0x20000,  0x300000,
ota_1,     app,  ota_1,   0x320000, 0x300000,
fctry,     data, nvs,     0x620000, 0x6000,
storage,   data, spiffs,  0x626000, 0x1DA000,

```

#### 分区划分亮点：

* **`ota_0` 和 `ota_1` 各给 3MB（`0x300000`）**：后续再加算法、复杂逻辑也完全无后顾之忧。
* **`fctry` 偏移变为 `0x620000**`：专用于 RainMaker 凭证烧录。
* **末尾规划 `storage`（近 1.9MB）**：用于挂载 LittleFS/SPIFFS，存放电机零偏校准数据、离线网页、配置文件等。

---

### 三、SDK 配置（menuconfig）需要改吗？

**必须确认两项配置：**

1. **设置 Flash 物理尺寸为 8MB**：
* 进入 `idf.py menuconfig`。
* 路径：`Serial flasher config` -> `Flash size` -> 选择 **`8 MB`**。
* （必须选 8MB，否则 IDF 检查分区表若超出默认的 2MB/4MB 会直接拒绝编译报错）。


2. **确认启用自定义分区表**：
* 路径：`Partition Table` -> `Partition Table` -> 选择 **`Custom partition table CSV`**。
* `Custom partition CSV file` 确保填的是 `partitions.csv`。



---

### 四、修改后的执行流程（关键操作顺序）

由于改变了物理分区地址，**必须全擦重新烧录**：

1. **重新编译**：
```bash
idf.py build

```


2. **擦除整片 Flash 并全量烧录**：
```bash
idf.py -p /dev/ttyUSB0 erase-flash
idf.py -p /dev/ttyUSB0 flash

```


3. **针对新的 `fctry` 地址重新 Claiming 写入密钥**：
注意此时 `fctry` 地址改成了 **`0x620000`**：
```bash
esp-rainmaker-cli claim --addr 0x620000 /dev/ttyUSB0

```


4. **重新进入 monitor 验证**：
```bash
idf.py -p /dev/ttyUSB0 monitor

```


看启动日志中各分区的 Length 是否全部生效且 Node ID 成功初始化。


---------------
-------------

针对为你 8MB Flash 重新规划的新分区结构（`ota_0`/`ota_1` 各 3MB，`fctry` 迁移至 **`0x620000`**），以下是匹配该架构的 ESP RainMaker 凭据配置与全流程操作指南：

---

### 第一步：固化新的 8MB 分区表与 SDK 配置

#### 1. 确认 `partitions.csv`

确保项目根目录的自定义分区表文件已保存为新结构：

```csv
# Name,    Type, SubType, Offset,   Size,     Flags
nvs,       data, nvs,     0x9000,   0x6000,
otadata,   data, ota,     0xf000,   0x2000,
phy_init,  data, phy,     0x11000,  0x1000,
ota_0,     app,  ota_0,   0x20000,  0x300000,
ota_1,     app,  ota_1,   0x320000, 0x300000,
fctry,     data, nvs,     0x620000, 0x6000,
storage,   data, spiffs,  0x626000, 0x1DA000,

```

#### 2. 检查 `menuconfig`

在终端执行 `idf.py menuconfig`，核对以下配置项：

* **`Serial flasher config` $\to$ `Flash size**`：选择 **`8 MB`**。
* **`Partition Table` $\to$ `Partition Table**`：勾选 **`Custom partition table CSV`**。
* **`Custom partition CSV file`**：填入 `partitions.csv`。

---

### 第二步：全新板子全擦与主固件烧录

因为物理分区偏移发生了根本性变化，为了防止旧分区的残余数据干扰（特别是 `otadata` 指向错误以及旧的未对齐 NVS），必须整片全擦：

```bash
# 1. 完整重新构建
idf.py build

# 2. 全擦整片 8MB Flash 并烧录当前新固件
idf.py -p /dev/ttyUSB0 erase-flash
idf.py -p /dev/ttyUSB0 flash

```

---

### 第三步：终端登录与账号鉴权（免重复登录说明）

如果当前 Linux/WSL 环境尚未登录过 RainMaker 账号：

1. **环境准备与登录**：
```bash
# 安装 CLI 工具（如果未安装）
pip3 install esp-rainmaker-cli --break-system-packages

# 授权登录
esp-rainmaker-cli login

```


2. **快捷同步已有的 Windows 凭证**：
若之前在 Windows 宿主机已成功登录，直接复制凭据目录，免去终端调起浏览器的繁琐流程：
```bash
mkdir -p ~/.espressif/rainmaker/
cp -r /mnt/c/Users/ufo28/.espressif/rainmaker/* ~/.espressif/rainmaker/

```


3. **验证登录有效性**：
```bash
esp-rainmaker-cli getuserinfo

```


终端返回你的账户 ID 与邮箱即表示认证已处于有效状态。

---

### 第四步：执行设备认领并向新地址写入密钥（核心）

给串口赋予读写权限，并调用 CLI 向乐鑫云申请证书。**注意此处的物理偏移地址必须指定为你新分区表里的 `0x620000**`：

```bash
# 赋予串口权限
sudo chmod 666 /dev/ttyUSB0

# 向 0x620000 分区执行 Claiming 烧录
esp-rainmaker-cli claim --addr 0x620000 /dev/ttyUSB0

```

> **底层执行的逻辑**：
> 1. CLI 读取芯片出厂内置的物理 MAC 地址。
> 2. 云端 API 注册新节点并颁发该设备专属的 **AWS IoT 客户端证书**、**私钥**与 22 位 **Node ID**。
> 3. CLI 自动将证书和 Node ID 封装为 NVS 二进制，直接写入芯片 Flash 的 **`0x620000`** 扇区。
> 
> 

---

### 第五步：串口监控与全流程验证

运行 Monitor 监控设备启动全生命周期：

```bash
idf.py -p /dev/ttyUSB0 monitor

```

正常情况下，日志将依次输出：

1. **分区表加载正确**：
```text
I (...) boot:  3 ota_0            OTA app          00 10 00020000 00300000
I (...) boot:  4 ota_1            OTA app          00 11 00320000 00300000
I (...) boot:  5 fctry           WiFi data        01 02 00620000 00006000

```


2. **Node ID 初始化成功（不再提示 Node ID 缺失报错）**：
```text
I (...) esp_rmaker_factory: Initialized factory partition
I (...) esp_rmaker_node: Node ID ----- <专属于该芯片的新Node_ID>

```


3. **Wi-Fi 与云端 MQTT 建连**：
完成 SmartConfig 配网并同步 SNTP 时间后：
```text
I (...) esp_mqtt_glue: Connecting to mqtt.rainmaker.espressif.com
I (...) esp_mqtt_glue: MQTT Connected
I (...) esp_rmaker_ota_using_topics: Subscribing to: node/<你的新Node_ID>/otaurl

```



此时新板子已具备单分区 **3MB** 的充足空间，并与 RainMaker 云端完成物理安全绑定，后续在 Web 控制台下发 OTA 固件升级即可顺利双区无缝轮换。