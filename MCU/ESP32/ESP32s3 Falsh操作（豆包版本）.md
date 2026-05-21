# esp32 flash操作注意事项
# ESP32 Flash 操作 **10 条保命注意事项**
我给你总结**最关键、最容易踩坑、最容易把芯片搞坏/搞砖**的点，全部是实战经验，**ESP32-S3-N16R8 通用**，记住这些你就不会出事！

---

# 1. **写入前必须先擦除！**（最重要）
- Flash 只能把 **1 变成 0**，不能直接把 0 变 1
- 想重新写入，**必须先擦除**
- 擦除最小单位：**4096 字节（4KB）**
- 不擦除直接写 = **数据乱码、写入无效**

---

# 2. **绝对不要直接写 Flash 物理地址！**
- 比如 `0x000000` ~ `0x100000` 是系统区
- 乱写 = **直接变砖，程序报废**
- 正确做法：
  ✅ **NVS**
  ✅ **自定义分区**
  ✅ **SPIFFS**
  ❌ 拒绝直接操作地址

---

# 3. **擦除 & 写入是耗时操作**
- 擦 4KB：**50~100ms**
- 写入 256B：**1ms**
- 期间 **中断会被关闭**
- 不要在定时器、高优先级任务里频繁写 Flash

---

# 4. **写入/擦除时断电 = 数据损坏**
- 写 Flash 过程中掉电
  → 这块数据废了
  → 严重时分区表损坏
- 重要数据要做：
  - 双备份
  - 校验和

---

# 5. **Flash 寿命有限（10万次擦写）**
- 不要**频繁循环写 Flash**
  比如：10ms 写一次 → 几小时就坏
- 正确做法：
  - 内存缓存
  - 满一定次数再批量写入
- NVS 自带**磨损均衡**，最安全

---

# 6. **数据必须对齐**
- 写入最好 **4 字节对齐**
- 偏移必须是 **4 的倍数**
- 不对齐 = 效率低 / 写入失败

---

# 7. **不要存超大数据**
- Flash 适合存：
  ✅ WiFi 信息
  ✅ 配置参数
  ✅ 状态
- 不适合存：
  ❌ 日志（每秒写）
  ❌ 大文件频繁修改
  ❌ 大量传感器原始数据

---

# 8. **不同分区不能互相覆盖**
- app 分区（程序）
- nvs 分区（配置）
- spiffs 分区（文件）
- 自定义数据分区
**互相独立，越界写 = 炸机**

---

# 9. **加密开启后，Flash 内容不可直接读取**
- 如果开启 Flash 加密
- 用编程器读出来是**乱码**
- 调试时注意

---

# 10. **Arduino 平台优先用：**
- 存配置 → **NVS（Preferences库）** 最安全
- 存文件 → **SPIFFS**
- 存大量自定义数据 → **自定义分区**

---

# 我给你浓缩成 **5 句口诀**
1. 不写系统地址
2. 写入必先擦除
3. 不能频繁写入
4. 断电会坏数据
5. 能用 NVS 就不用底层

---

## 你现在属于哪个阶段？
1. 我已经懂了，要**实战代码**
2. 我要 **WiFi 配网 + NVS 保存**（最常用）
3. 我要 **自定义分区读写工具类**
4. 我要 **SPIFFS 文件系统**

你说一声，我直接给你最终版代码！

----
-----
----

# ESP32 Flash 详解与读写操作指南
ESP32 本身**无内置 Flash**，程序与持久化数据完全依赖**外部 SPI Flash**（常见 W25Q 系列），通过 QSPI 接口高速通信，是固件、配置、文件系统的核心存储载体。

---

## 一、ESP32 Flash 基础特性
### 1. 硬件规格
- **容量**：模组常用 **4MB / 8MB / 16MB**（如 ESP32-WROOM-32 默认为 4MB）
- **接口**：QSPI（4 线 SPI），最高 80MHz，支持 XIP（片内执行）
- **扇区结构**：
  - 最小擦除单位：**4KB（1 扇区）**
  - 页写入单位：**256 字节**
  - 寿命：约 **10 万次擦写循环**
- **默认布局（4MB Flash）**
  - 0x000000 ~ 0x001000：Bootloader（4KB）
  - 0x008000 ~ 0x009000：分区表（4KB）
  - 0x010000 ~ ...：应用程序（APP）
  - 剩余：数据区（SPIFFS、NVS、OTA 等）

### 2. 分区表（核心）
Flash 按功能划分为多个分区，由 **分区表** 管理（默认 0x8000 地址）：
- **类型**：`app`（程序）、`data`（数据）
- **常用子类型**：
  - `factory`：出厂固件
  - `ota_0/ota_1`：OTA 升级分区
  - `nvs`：参数存储（键值对）
  - `spiffs`：文件系统
- **自定义分区**：创建 `partitions.csv`，在 `menuconfig` 中选择 `Custom partition table`

**示例分区表（4MB）**
```csv
# Name,   Type, SubType, Offset,  Size, Flags
nvs,      data, nvs,     0x9000,  0x6000
otadata,  data, ota,     0xf000,  0x2000
app0,     app,  ota_0,   0x10000, 0x180000
app1,     app,  ota_1,   0x190000,0x180000
spiffs,   data, spiffs,  0x310000,0xF0000
```

---

## 二、Flash 读写核心规则（必看）
1. **写入前必须先擦除**：Flash 只能从 `1` 写为 `0`，擦除后恢复全 `1`
2. **擦除对齐**：必须按 **4KB 扇区** 对齐擦除
3. **写入对齐**：推荐按 **4 字节** 对齐写入（效率最高）
4. **禁止覆盖程序区**：直接操作 Flash 地址易破坏固件，**优先用分区 API**
5. **线程安全**：读写时会关闭中断，避免耗时操作

---

## 三、ESP-IDF 环境读写（推荐）
### 1. 核心 API（分区层，安全）
```c
// 1. 查找分区（如 SPIFFS、NVS、自定义 data 分区）
const esp_partition_t *part = esp_partition_find_first(
    ESP_PARTITION_TYPE_DATA,    // 类型：data/app
    ESP_PARTITION_SUBTYPE_DATA_SPIFFS, // 子类型
    NULL                        // 分区名（NULL 匹配第一个）
);

// 2. 擦除范围（必须 4KB 对齐）
esp_partition_erase_range(part, 0, 4096); // 偏移0，擦4KB

// 3. 写入数据
uint8_t w_buf[] = {0x11,0x22,0x33,0x44};
esp_partition_write(part, 0, w_buf, sizeof(w_buf));

// 4. 读取数据
uint8_t r_buf[4];
esp_partition_read(part, 0, r_buf, sizeof(r_buf));
```

### 2. 底层 SPI Flash API（慎用）
```c
// 直接操作主 Flash（风险高，易破坏固件）
esp_flash_erase_region(NULL, 0x100000, 4096); // 偏移0x100000擦4KB
esp_flash_write(NULL, 0x100000, w_buf, 4);
esp_flash_read(NULL, 0x100000, r_buf, 4);
```

### 3. 完整示例（自定义数据分区）
**步骤 1：添加自定义分区**
```csv
# partitions.csv
my_data, data, 0x40, 0x310000, 0x10000 // 64KB 自定义数据区
```

**步骤 2：读写代码**
```c
#include "esp_partition.h"
#include "esp_log.h"

#define TAG "FLASH_TEST"
#define PART_NAME "my_data"

void flash_test(void) {
    // 查找分区
    const esp_partition_t *part = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, 0x40, PART_NAME);
    if(!part) { ESP_LOGE(TAG,"分区不存在"); return; }

    // 数据
    uint8_t w_data[] = "ESP32 Flash Write Test";
    size_t len = sizeof(w_data);
    uint8_t r_data[len];

    // 擦除 + 写入
    if(esp_partition_erase_range(part, 0, 4096) == ESP_OK) {
        esp_partition_write(part, 0, w_data, len);
        ESP_LOGI(TAG, "写入成功");
    }

    // 读取
    esp_partition_read(part, 0, r_data, len);
    ESP_LOGI(TAG, "读取：%s", r_data);
}
```

---

## 四、Arduino 环境读写（简单）
### 1. 方法 1：EEPROM 库（模拟 EEPROM）
```cpp
#include <EEPROM.h>

#define EEPROM_SIZE 512 // 分配 512 字节 Flash

void setup() {
    Serial.begin(115200);
    EEPROM.begin(EEPROM_SIZE);

    // 写入
    EEPROM.write(0, 0xAA);    // 地址0写1字节
    EEPROM.put(1, 12345);     // 地址1写整型
    EEPROM.commit();          // 保存到 Flash（必须）

    // 读取
    uint8_t val1 = EEPROM.read(0);
    int val2;
    EEPROM.get(1, val2);

    Serial.printf("val1:0x%X, val2:%d", val1, val2);
}
```

### 2. 方法 2：Preferences（键值对，推荐）
```cpp
#include <Preferences.h>
Preferences prefs;

void setup() {
    Serial.begin(115200);
    prefs.begin("my_config", false); // 命名空间，读写模式

    // 写入
    prefs.putUInt("counter", 100);
    prefs.putString("wifi", "ssid123");

    // 读取（不存在则返回默认值）
    uint32_t cnt = prefs.getUInt("counter", 0);
    String ssid = prefs.getString("wifi", "default");

    Serial.printf("cnt:%u, ssid:%s", cnt, ssid.c_str());
    prefs.end();
}
```

### 3. 方法 3：SPIFFS 文件系统
```cpp
#include <SPIFFS.h>

void setup() {
    Serial.begin(115200);
    SPIFFS.begin(true); // 挂载，失败则格式化

    // 写入文件
    File f = SPIFFS.open("/test.txt", "w");
    if(f) { f.println("Hello Flash"); f.close(); }

    // 读取文件
    f = SPIFFS.open("/test.txt", "r");
    if(f) {
        String s = f.readStringUntil('\n');
        Serial.println(s);
        f.close();
    }
}
```

---

## 五、关键注意事项
1. **擦写均衡**：避免频繁擦写同一扇区（易损坏）
2. **掉电保护**：写入/擦除时掉电易导致数据损坏
3. **加密支持**：ESP32 支持 Flash 加密，启用后自动加解密
4. **容量查看**：
   ```c
   // ESP-IDF
   size_t flash_size = esp_flash_get_chip_size(NULL);
   // Arduino
   uint32_t flash_size = ESP.getFlashChipSize();
   ```

---

## 六、常见问题
- **写入失败**：未擦除、地址/长度未对齐、超出分区范围
- **数据错乱**：覆盖系统分区、掉电中断写入
- **性能**：擦除 4KB 约 50ms；写入 256 字节约 1ms

---

要不要我帮你生成一份可直接使用的 **ESP-IDF Flash 读写驱动库**（含初始化、读写、擦除、校验函数）？

---
---
-----
# ESP32-S3-N16R8 专属 Flash + NVS 完整指南
我给你**完全针对你的芯片型号（ESP32-S3-N16R8）**重新讲清楚，**简单、实用、能直接用**。

---

## 一、先明确：你的芯片 Flash 规格
**ESP32-S3-N16R8**
- **Flash = 16MB（16M字节）**
- **PSRAM = 8MB（8M字节）**
- Flash 芯片：**QSPI Flash**，4KB 扇区，寿命 10万次擦写
- 系统自动分区，**你完全不用管底层地址**，直接用 NVS 最安全

> 重点：**16MB 超大 Flash = NVS 空间非常充足**，随便存配置、参数、计数器、WiFi信息都没问题。

---

# 二、什么是 NVS？（最简单的解释）
**NVS = Non-Volatile Storage**
中文：**掉电不丢失的键值对存储区**

你可以把它理解成：
**ESP32 内置的、安全的、小型数据库**，存在 Flash 里，专门存：
- WiFi 账号密码
- 设备配置参数
- 开机计数
- 阈值、模式、状态
- 蓝牙名称
- 任何小数据

### NVS 优点（必看）
1. **不用管 Flash 地址**（不会把固件搞坏）
2. **不用手动擦除**（NVS 自动管理）
3. **掉电不丢**
4. **支持擦写均衡**（延长 Flash 寿命）
5. **支持多种数据类型**（int、string、char、float 等）
6. **16MB Flash 里默认给 NVS 分了 24KB～64KB**，足够存几百个键值对
7. **Arduino / ESP-IDF 都能用**

---

# 三、你的 ESP32-S3-N16R8 默认分区（NVS 位置）
16MB Flash 默认分区表中：
- **nvs**：24KB～64KB（存配置）
- **otadata**：OTA 数据
- **app**：程序区
- **spiffs**：文件系统（剩下的大部分空间）

你**完全不用改分区表**，NVS 开箱即用。

---

# 四、NVS 怎么用？（Arduino 最简洁示例）
你的 ESP32-S3 用 **Preferences 库** = NVS 的高级封装，最简单。

## 1. 写入数据
```cpp
#include <Preferences.h>
Preferences prefs;

void setup() {
  Serial.begin(115200);

  // 打开命名空间（自定义名字，false=可写）
  prefs.begin("my_config", false);

  // 写入各种类型数据
  prefs.putInt("count", 100);        // 整数
  prefs.putString("wifi_ssid", "TEST_WIFI"); // 字符串
  prefs.putFloat("temp", 25.5);      // 浮点数
  prefs.putBool("enable", true);     // 布尔

  prefs.end(); // 关闭
  Serial.println("写入 NVS 完成");
}
```

## 2. 读取数据
```cpp
#include <Preferences.h>
Preferences prefs;

void setup() {
  Serial.begin(115200);
  prefs.begin("my_config", true); // true=只读

  // 读取（不存在返回默认值）
  int count = prefs.getInt("count", 0);
  String ssid = prefs.getString("wifi_ssid", "default");
  float temp = prefs.getFloat("temp", 0.0);
  bool enable = prefs.getBool("enable", false);

  Serial.printf("count=%d\n", count);
  Serial.printf("ssid=%s\n", ssid.c_str());
  Serial.printf("temp=%.1f\n", temp);
  Serial.printf("enable=%d\n", enable);

  prefs.end();
}
```

## 3. 删除某个键
```cpp
prefs.remove("count");
```

## 4. 清空整个命名空间
```cpp
prefs.clear();
```

## 5. 查看 NVS 剩余空间
```cpp
size_t free = prefs.freeEntries();
Serial.printf("NVS 剩余空间: %d\n", free);
```

---

# 五、NVS 结构（简单理解）
NVS 内部结构：
- **命名空间（namespace）**
  比如 `my_config`、`wifi_info`、`device`
- **键（key）**
  比如 `count`、`ssid`、`password`
- **值（value）**
  你存的数据

---

# 六、NVS 支持的数据类型
```
putInt / getInt
putUInt / getUInt
putLong / getLong
putString / getString
putBytes / getBytes
putFloat / getFloat
putDouble / getDouble
putBool / getBool
```

几乎所有配置都能存！

---

# 七、NVS 重要规则（必看）
1. **key 名称最长 15 个字符**
2. **字符串最长 4095 字节**
3. 不用关心擦除，NVS 自动处理
4. 不用关心地址，绝对不会破坏程序
5. 适合存**配置、参数、小数据**
6. 不适合存**大量日志、大文件**（大文件用 SPIFFS）

---

# 八、ESP32-S3-N16R8 最佳实践
- **配置参数 → NVS（Preferences）**
- **网页、图片、固件 → SPIFFS**
- **程序本身 → 应用分区**

你这款 16MB Flash，**NVS 空间非常充足**，放心用！

---

# 九、我给你总结成 3 句话
1. **NVS 是 Flash 上的安全配置区**
2. **ESP32-S3-N16R8 自带 24KB+ NVS 空间**
3. **Arduino 用 Preferences 库 3 行代码就能读写**

---

如果你需要，我可以直接给你写：
- **WiFi 账号密码存在 NVS，掉电不丢失**
- **开机自动计数**
- **NVS 配置保存与恢复工具类**

你想要哪个？

-----
----
----
我给你写一个**专门适配 ESP32‑S3‑N16R8、可直接复制使用、超稳定的 NVS 配置保存/恢复工具类**。

特点：
✅ 掉电不丢失  
✅ 自动管理命名空间  
✅ 支持读取默认值  
✅ 支持清空配置、删除键  
✅ 代码极简、一行调用  
✅ 适合存 WiFi、阈值、开关、计数、设备参数  

直接复制就能用！

---

# NVSConfig.h（工具类头文件）
```cpp
#ifndef NVSConfig_h
#define NVSConfig_h

#include <Arduino.h>
#include <Preferences.h>

class NVSConfig {
private:
    Preferences prefs;
    const char* _namespace;

public:
    // 初始化，传入命名空间
    NVSConfig(const char* ns = "device_config");

    // 打开 / 关闭
    void begin(bool readOnly = false);
    void end();

    // 读取
    int getInt(const char* key, int def = 0);
    uint32_t getUInt(const char* key, uint32_t def = 0);
    float getFloat(const char* key, float def = 0.0f);
    String getString(const char* key, const char* def = "");
    bool getBool(const char* key, bool def = false);

    // 保存
    void setInt(const char* key, int value);
    void setUInt(const char* key, uint32_t value);
    void setFloat(const char* key, float value);
    void setString(const char* key, String value);
    void setBool(const char* key, bool value);

    // 删除 & 清空
    void remove(const char* key);
    void clear();
};

#endif
```

---

# NVSConfig.cpp（工具类实现）
```cpp
#include "NVSConfig.h"

NVSConfig::NVSConfig(const char* ns) {
    _namespace = ns;
}

void NVSConfig::begin(bool readOnly) {
    prefs.begin(_namespace, readOnly);
}

void NVSConfig::end() {
    prefs.end();
}

// ================== 读取 ==================
int NVSConfig::getInt(const char* key, int def) {
    return prefs.getInt(key, def);
}

uint32_t NVSConfig::getUInt(const char* key, uint32_t def) {
    return prefs.getUInt(key, def);
}

float NVSConfig::getFloat(const char* key, float def) {
    return prefs.getFloat(key, def);
}

String NVSConfig::getString(const char* key, const char* def) {
    return prefs.getString(key, def);
}

bool NVSConfig::getBool(const char* key, bool def) {
    return prefs.getBool(key, def);
}

// ================== 保存 ==================
void NVSConfig::setInt(const char* key, int value) {
    prefs.putInt(key, value);
}

void NVSConfig::setUInt(const char* key, uint32_t value) {
    prefs.putUInt(key, value);
}

void NVSConfig::setFloat(const char* key, float value) {
    prefs.putFloat(key, value);
}

void NVSConfig::setString(const char* key, String value) {
    prefs.putString(key, value);
}

void NVSConfig::setBool(const char* key, bool value) {
    prefs.putBool(key, value);
}

// ================== 删除 ==================
void NVSConfig::remove(const char* key) {
    prefs.remove(key);
}

void NVSConfig::clear() {
    prefs.clear();
}
```

---

# 使用示例（主程序 INO）
```cpp
#include "NVSConfig.h"

// 创建配置对象
NVSConfig config("my_device");

void setup() {
    Serial.begin(115200);
    delay(500);

    // ========== 1. 保存配置 ==========
    config.begin(false); // 可写
    config.setString("wifi_ssid", "MyHomeWiFi");
    config.setString("wifi_pwd",  "12345678");
    config.setInt("brightness", 75);
    config.setFloat("temp_limit", 28.5);
    config.setBool("auto_mode", true);
    config.end();

    Serial.println("配置保存完成！");


    // ========== 2. 读取配置（带默认值） ==========
    config.begin(true); // 只读
    String ssid     = config.getString("wifi_ssid", "default_ssid");
    String pwd      = config.getString("wifi_pwd",  "default_pwd");
    int bright      = config.getInt("brightness", 50);
    float tempLimit = config.getFloat("temp_limit", 25.0);
    bool autoMode   = config.getBool("auto_mode", false);
    config.end();

    Serial.println("===== 读取配置 =====");
    Serial.println("SSID: " + ssid);
    Serial.println("PWD:  " + pwd);
    Serial.println("亮度: " + String(bright));
    Serial.println("温度阈值: " + String(tempLimit));
    Serial.println("自动模式: " + String(autoMode));


    // ========== 3. 删除单个键 ==========
    // config.begin(false);
    // config.remove("temp_limit");
    // config.end();


    // ========== 4. 清空所有配置 ==========
    // config.begin(false);
    // config.clear();
    // config.end();
}

void loop() {
}
```

---

# 你可以直接用它来保存：
- WiFi 名称 & 密码  
- 屏幕亮度、报警阈值  
- 运行次数、开机次数  
- 设备模式、开关状态  
- 蓝牙名称、MQTT 配置  
- 任何掉电需要保存的小数据  

---

# 我还能帮你进一步做成：
1. **结构体批量保存/恢复**（一次存所有配置）  
2. **出厂默认配置自动恢复**  
3. **Web 配网 + NVS 保存**  

你需要我把**【结构体批量保存】**也加上吗？这个最实用！

-----
----
----
我直接给你**最简单、最安全、ESP32-S3-N16R8 通用的 Flash 操作方法**，不讲废话，**直接能跑、不会炸机、不会破坏程序**。

你只需要记住一句话：
# **ESP32 操作 Flash 永远用「分区」，不要直接写地址！**

我给你 3 套最实用的方案：
1. **NVS（存配置，最简单）**
2. **自定义分区读写（存任意数据）**
3. **SPIFFS（存文件、网页、图片）**

---

# 一、最安全：直接读写自定义 Flash 分区（你要的底层操作）
这是**真正操作 Flash**，但又**绝对安全**的方法。

## 1. 先创建分区表（必须）
在项目文件夹里新建文件：
**`partitions.csv`**

```csv
nvs,      data, nvs,     ,        0x6000
otadata,  data, ota,     ,        0x2000
phy_init, data, phy,     ,        0x1000
factory,  app,  factory, ,        4M
my_flash, data, 0x40,    ,        1M
```
作用：
- 给你分了 **1MB 的独立 Flash 区域**
- 名字叫 `my_flash`
- 随便读写，**绝对不会破坏程序**

---

## 2. Flash 读写代码（直接复制）
```cpp
#include <esp_partition.h>
#include <esp_flash.h>
#include <Arduino.h>

const esp_partition_t* flash_part;

// 初始化分区
void initFlashPartition() {
  flash_part = esp_partition_find_first(
    ESP_PARTITION_TYPE_DATA,
    (esp_partition_subtype_t)0x40,
    "my_flash"
  );
}

// 写入 Flash（偏移0开始，自动擦4K）
void writeFlash(uint32_t offset, uint8_t* data, size_t len) {
  if (!flash_part) return;
  
  // Flash 必须先擦除！最小单位 4096 字节
  esp_partition_erase_range(flash_part, offset, 4096);
  
  // 写入
  esp_partition_write(flash_part, offset, data, len);
}

// 读取 Flash
void readFlash(uint32_t offset, uint8_t* buf, size_t len) {
  if (!flash_part) return;
  esp_partition_read(flash_part, offset, buf, len);
}
```

---

## 3. 使用示例（超级简单）
```cpp
void setup() {
  Serial.begin(115200);
  initFlashPartition();

  // ================== 写入 ==================
  char writeData[] = "Hello ESP32 Flash! 你好！";
  writeFlash(0, (uint8_t*)writeData, sizeof(writeData));


  // ================== 读取 ==================
  char readData[128] = {0};
  readFlash(0, (uint8_t*)readData, sizeof(readData));
  
  Serial.print("读取到：");
  Serial.println(readData);
}

void loop() {
}
```

**输出：**
```
读取到：Hello ESP32 Flash! 你好！
```

---

# 二、你必须记住的 4 条 Flash 规则
1. **写入前必须擦除**（最小擦除单位：4096 字节）
2. **不能写到程序区**（否则直接变砖）
3. **用分区操作最安全**
4. 你的 ESP32-S3-N16R8：**16MB Flash，随便用**

---

# 三、3 种 Flash 使用场景（你直接对照用）
## 1. 存配置参数 → **NVS**
最简单，键值对，自动管理，不用擦除
```cpp
prefs.putInt("count", 10);
int val = prefs.getInt("count");
```

## 2. 存自定义数据/二进制 → **自定义分区**
你刚才用的就是这个，最灵活。

## 3. 存文件/网页/图片 → **SPIFFS**
像电脑磁盘一样用
```cpp
File f = SPIFFS.open("/test.txt", "w");
```

---

# 四、我再给你一个**万能工具类**
## FlashTool.h
```cpp
#include <esp_partition.h>

class FlashTool {
private:
  const esp_partition_t* part;
public:
  bool begin(const char* partName = "my_flash") {
    part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, (esp_partition_subtype_t)0x40, partName);
    return part != NULL;
  }

  void write(uint32_t offset, uint8_t* data, size_t len) {
    if (!part) return;
    esp_partition_erase_range(part, offset, 4096);
    esp_partition_write(part, offset, data, len);
  }

  void read(uint32_t offset, uint8_t* buf, size_t len) {
    if (!part) return;
    esp_partition_read(part, offset, buf, len);
  }
};
```

## 使用
```cpp
FlashTool flash;
flash.begin();

flash.write(0, (uint8_t*)"test", 4);
flash.read(0, buf, 4);
```

---

# 你现在只要回答我一个问题：
你想把 Flash 用来存什么？
1. 配置参数
2. 传感器记录
3. WiFi信息
4. 大文件/图片

我直接给你**对应最适合的完整代码**。

---
----
----
# 给你：**ESP32-S3 专用 → 用 NVS 保存 WiFi 账号密码（掉电不丢）**
这是**最安全、最简单、最稳定**的 WiFi 信息存储方案，**直接复制就能用**！

不用分区、不用操作底层 Flash，**完全不会弄坏程序**，专门存 WiFi 账号+密码。

---

# 完整代码（可直接上传）
```cpp
#include <Preferences.h>
#include <WiFi.h>

// NVS 操作对象
Preferences prefs;

// 【你要保存的 WiFi 信息】
const char* WIFI_SSID_DEFAULT = "你的WiFi名称";
const char* WIFI_PASS_DEFAULT = "你的WiFi密码";

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("ESP32 WiFi + NVS 存储测试");

  // ================== 1. 把 WiFi 存入 NVS（只需要执行一次）==================
  prefs.begin("wifi_config", false); // 打开命名空间，可写
  prefs.putString("ssid", WIFI_SSID_DEFAULT);  // 存 SSID
  prefs.putString("pwd", WIFI_PASS_DEFAULT);   // 存 密码
  prefs.end();                                 // 关闭
  Serial.println("✅ WiFi 信息已保存到 NVS (Flash)");


  // ================== 2. 从 NVS 读取 WiFi（每次开机自动读）==================
  prefs.begin("wifi_config", true); // 只读模式
  String ssid = prefs.getString("ssid", "");  // 读账号
  String pwd  = prefs.getString("pwd", "");   // 读密码
  prefs.end();

  Serial.print("📶 读取到 SSID：");
  Serial.println(ssid);
  Serial.print("🔐 读取到 密码：");
  Serial.println(pwd);


  // ================== 3. 连接 WiFi ==================
  WiFi.begin(ssid.c_str(), pwd.c_str());
  Serial.print("正在连接 WiFi...");
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\n✅ WiFi 连接成功！");
  Serial.print("IP 地址：");
  Serial.println(WiFi.localIP());
}

void loop() {
  // 正常运行
}
```

---

# 这个代码做了什么？
1. **把 WiFi 信息存在 Flash 的 NVS 分区**
   - 掉电、重启、重新上传程序 **都不会丢失**
2. **每次开机自动读取并连接 WiFi**
3. **绝对安全**，不会破坏固件
4. 你的 **ESP32-S3-N16R8（16MB Flash）** 完美支持

---

# 你只需要改 2 个地方
```cpp
const char* WIFI_SSID_DEFAULT = "你的WiFi名称";
const char* WIFI_PASS_DEFAULT = "你的WiFi密码";
```

---

# 进阶：想修改已保存的 WiFi？
直接运行这段代码即可覆盖：
```cpp
prefs.begin("wifi_config", false);
prefs.putString("ssid", "新WiFi名");
prefs.putString("pwd", "新WiFi密码");
prefs.end();
```

---

# 想清空 WiFi 信息？
```cpp
prefs.begin("wifi_config", false);
prefs.clear();
prefs.end();
```

---

## 总结
1. 存 WiFi 信息 **首选 NVS**，最简单安全
2. 代码直接用，**不用管 Flash 地址**
3. 掉电不丢失，适合设备开机自动连WiFi

需要我再给你做一个 **WiFi 配网（手机配网）+ NVS 保存** 的完整版吗？