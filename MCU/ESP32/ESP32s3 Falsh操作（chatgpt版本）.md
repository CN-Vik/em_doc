# esp32 flash操作注意事项

有，而且 ESP32-S3 的 Flash 注意事项比普通 MCU 还多一点，因为它的程序本身也是从外部 SPI Flash 取指执行的。

你重点记这句话：

> **能用 NVS 就别裸写 Flash；要自己操作 Flash，就先划分区，再用 `esp_partition_*`；不要直接乱写物理地址。**

---

# 1. 不要直接操作物理地址

不推荐这样直接想当然地写：

```c
esp_flash_write(NULL, data, 0x10000, len);
```

因为你可能误写到：

```text
0x8000   分区表
0x9000   NVS
0x10000  app 程序
```

ESP-IDF 官方也建议，访问主 Flash 时优先用 `esp_partition_*`，因为它会按分区表做边界检查，并且能正确计算 Flash 偏移；底层 `esp_flash_*` 更适合外接第二颗 Flash。([Espressif Systems][1])

推荐写法：

```c
const esp_partition_t *part = esp_partition_find_first(
    ESP_PARTITION_TYPE_DATA,
    ESP_PARTITION_SUBTYPE_ANY,
    "user_data"
);

esp_partition_erase_range(part, 0, 4096);
esp_partition_write(part, 0, data, len);
esp_partition_read(part, 0, buf, len);
```

`esp_partition_read()`、`esp_partition_write()`、`esp_partition_erase_range()` 是基于分区表的高级 API，只会在对应分区范围内操作。([Espressif Systems][2])

---

# 2. 写之前必须擦除

Flash 不是 RAM。

RAM 可以反复改：

```c
x = 0x12;
x = 0x34;
x = 0x56;
```

Flash 不行。Flash 擦除后通常是：

```text
0xFF
```

写入只能把 bit 从：

```text
1 -> 0
```

不能直接：

```text
0 -> 1
```

所以标准流程是：

```text
擦除 erase
写入 write
读取 read
```

例如：

```c
esp_partition_erase_range(part, 0, 4096);
esp_partition_write(part, 0, data, len);
esp_partition_read(part, 0, buf, len);
```

---

# 3. 擦除要按 4KB sector 对齐

ESP32-S3 Flash 是按 sector 擦除的，常见 sector 是：

```text
4096 字节 = 4KB = 0x1000
```

所以这样可以：

```c
esp_partition_erase_range(part, 0, 4096);
esp_partition_erase_range(part, 4096, 4096);
esp_partition_erase_range(part, 0, 8192);
```

这样不建议/可能失败：

```c
esp_partition_erase_range(part, 1, 100);
```

因为 offset 和 size 没有按 sector 对齐。

实际工程里你可以这样定义：

```c
#define FLASH_SECTOR_SIZE 4096
```

---

# 4. 不要高频擦写 Flash

这是最重要的坑。

比如你采集重量、IMU、电机状态，千万不要这样：

```c
while (1) {
    esp_partition_erase_range(part, 0, 4096);
    esp_partition_write(part, 0, &weight, sizeof(weight));
    vTaskDelay(pdMS_TO_TICKS(10));
}
```

10ms 写一次，等于：

```text
100 次/秒
6000 次/分钟
360000 次/小时
```

Flash 是有擦写寿命的。官方 Wear Levelling 文档也说明，ESP32-S3 使用的 SPI Flash 通常是按 sector 组织，并且每个 sector 的擦除/修改次数有限；磨损均衡就是为了把磨损分散到多个 sector。([Espressif Systems][3])

正确做法：

```text
实时数据：放 RAM
配置参数：变化后延迟几秒再写
日志数据：攒一批再写
频繁文件写入：用 FATFS + wear levelling
小参数：用 NVS
```

比如音量调节：

```text
用户连续按音量 + + + + +
不要每按一次写一次 Flash
等用户 3 秒不操作后，再保存一次
```

---

# 5. 小参数优先用 NVS，不要自己造轮子

比如这些：

```text
Wi-Fi SSID / 密码
音量
亮度
称重校准系数
PID 参数
设备模式
开机次数
```

用 NVS 更合适。

NVS 本身就带有一定的磨损均衡设计，更新 key 时不是原地覆盖，而是追加新数据、旧数据标记失效，可以降低擦除频率。([Espressif Systems][4])

但是注意：**NVS 也不能高频写**。

错误：

```c
nvs_set_i32(handle, "weight", weight);
nvs_commit(handle);
```

如果你每 10ms 调一次，这也不行。

正确：

```text
weight 实时值放 RAM
只有校准系数、去皮值、最终设置结果才写 NVS
```

---

# 6. 大数据不要用 NVS

NVS 适合很多小值，不适合大文件、大日志、大 blob。官方文档也说明，NVS 更适合存很多小值；如果要存大 blob 或 string，建议考虑 FAT 文件系统加 wear levelling。([Espressif Systems][5])

简单分配：

```text
小参数            NVS
较大二进制数据     esp_partition
日志/文本/图片      LittleFS / FATFS
高频数据缓存        RAM / PSRAM
```

你的 ESP32-S3 N16R8 有 16MB Flash + 8MB PSRAM，所以可以：

```text
配置参数：NVS
天气图标/网页资源：LittleFS
音频资源：FATFS / LittleFS / SD 卡
运行时缓存：PSRAM
```

---

# 7. 不要在中断里写 Flash

千万不要这样：

```c
void IRAM_ATTR gpio_isr_handler(void *arg)
{
    esp_partition_write(part, 0, data, len);  // 错误
}
```

原因是 Flash 操作可能比较慢，而且 ESP32-S3 程序本身也从 SPI Flash 取指。官方文档说明，SPI0/1 总线同时服务指令/数据 cache 和 SPI1 Flash 驱动；当 cache 被禁用进行 Flash 操作时，其他任务会被挂起，非 IRAM-safe 中断也会被禁用，另一个 core 可能会忙等直到 Flash 操作完成。([Espressif Systems][6])

正确做法：

```text
中断里只发队列 / 置标志
普通 task 里再写 Flash
```

比如：

```c
void IRAM_ATTR gpio_isr_handler(void *arg)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    uint32_t event = 1;

    xQueueSendFromISR(flash_queue, &event, &xHigherPriorityTaskWoken);

    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}
```

然后在任务里：

```c
void flash_task(void *arg)
{
    uint32_t event;

    while (1) {
        if (xQueueReceive(flash_queue, &event, portMAX_DELAY)) {
            // 在 task 里写 Flash
            save_config_to_flash();
        }
    }
}
```

---

# 8. Flash 操作期间会影响系统实时性

这点对你做屏幕、音频、电机、IMU 很重要。

Flash 擦除/写入时，可能影响：

```text
Wi-Fi/BLE 时序
LVGL 刷屏
音频播放
电机控制任务
IMU 采样任务
高频定时器任务
```

尤其是擦除 sector 比写入更耗时。

所以建议：

```text
电机控制、IMU 采样、音频播放期间，尽量少擦 Flash
需要保存参数时，放到低优先级任务处理
日志先写 RAM ring buffer，再慢慢落盘
```

---

# 9. 掉电保护要考虑

如果写 Flash 写到一半突然断电，可能出现：

```text
数据只写了一半
结构体不完整
CRC 不对
新数据坏了，旧数据也被擦了
```

所以保存关键配置时，建议加：

```text
magic
version
length
crc
sequence
```

结构体例子：

```c
typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t length;
    uint32_t seq;
    uint32_t crc32;

    int32_t volume;
    int32_t brightness;
    float weight_cal;
} app_cfg_t;
```

读取时检查：

```text
magic 对不对
version 对不对
length 对不对
crc 对不对
```

更稳的做法是用 **双备份区**：

```text
slot A
slot B
```

每次写新配置时，不覆盖旧配置，而是写到另一个 slot。开机时选择：

```text
CRC 正确
seq 最大
```

的那份配置。

---

# 10. 不要随便擦整片 Flash

这个命令会清掉所有东西：

```bash
idf.py erase-flash
```

包括：

```text
bootloader
partition table
app
nvs
文件系统
ota 信息
```

开发阶段可以用，产品现场不要随便用。

如果只是想清 NVS，优先用：

```c
nvs_flash_erase();
```

如果只是想清某个自定义分区，用：

```c
esp_partition_erase_range(part, 0, part->size);
```

---

# 11. 改了分区表后，建议擦 Flash

你改了 `partitions.csv` 后，旧数据可能还留在原位置。

比如原来：

```text
nvs 在 0x9000，大小 0x6000
```

后来改成：

```text
nvs 在 0x9000，大小 0x10000
```

或者移动了 app、storage、spiffs 分区，旧数据可能导致奇怪问题。NVS 文档也提醒，如果 NVS 分区因为分区表变化被截断，应擦除其内容；ESP-IDF 提供 `idf.py erase-flash` 擦除整片 Flash。([Espressif Systems][5])

开发阶段建议：

```bash
idf.py erase-flash
idf.py flash monitor
```

---

# 12. Flash 加密时要注意 16 字节对齐

如果你开启了 Flash Encryption，并且某个分区带 `encrypted` 标志，那么写入加密 Flash 时要注意对齐。

官方文档说明，推荐使用 `esp_partition_write()` 写加密分区，它会在写入 encrypted 分区时自动加密；但加密数据的最小写入大小和对齐都是 **16 字节**。([Espressif Systems][7])

分区表里也可以给 data 分区加 `encrypted` flag；如果开启 Flash Encryption，这个分区会被加密，app 分区则无论是否设置 flag 都会加密。([Espressif Systems][8])

例如：

```csv
secret_data, data, 0x40, , 64K, encrypted
```

---

# 13. 分区大小和边界要规划好

你的 N16R8 是 16MB Flash，空间比较大，但也不要乱分。

典型可以这样规划：

```csv
# Name,     Type, SubType, Offset,   Size,    Flags
nvs,        data, nvs,     0x9000,   0x10000,
otadata,    data, ota,     0x19000,  0x2000,
phy_init,   data, phy,     0x1B000,  0x1000,
factory,    app,  factory, 0x20000,  2M,
ota_0,      app,  ota_0,   ,         3M,
ota_1,      app,  ota_1,   ,         3M,
storage,    data, spiffs,  ,         6M,
user_data,  data, 0x40,    ,         512K,
```

注意：

```text
app 分区要够大
OTA 至少要 ota_0 / ota_1 两个 app 分区
NVS 不要太小
文件系统单独分区
自定义数据单独分区
```

分区表的作用就是描述 Flash 中 bootloader、app、data、文件系统等不同区域。([Espressif Systems][2])

---

# 14. 写 Flash 前后建议做错误检查

不要忽略返回值。

错误示范：

```c
esp_partition_write(part, 0, data, len);
```

正确：

```c
esp_err_t err;

err = esp_partition_erase_range(part, 0, 4096);
if (err != ESP_OK) {
    ESP_LOGE(TAG, "erase failed: %s", esp_err_to_name(err));
    return;
}

err = esp_partition_write(part, 0, data, len);
if (err != ESP_OK) {
    ESP_LOGE(TAG, "write failed: %s", esp_err_to_name(err));
    return;
}

err = esp_partition_read(part, 0, buf, len);
if (err != ESP_OK) {
    ESP_LOGE(TAG, "read failed: %s", esp_err_to_name(err));
    return;
}
```

---

# 15. 最实用的工程建议

你可以按这个规则选方案：

```text
1. 保存配置参数：
   用 NVS

2. 保存较大结构体、二进制块：
   自定义 data 分区 + esp_partition

3. 保存日志、图片、音频、网页文件：
   LittleFS / FATFS / SPIFFS

4. 保存频繁变化的实时数据：
   RAM / PSRAM，不要直接写 Flash

5. 需要长期频繁写文件：
   FATFS + wear levelling

6. 需要安全保护：
   Flash Encryption + encrypted 分区 + NVS encryption
```

一句话总结：

> **ESP32 Flash 操作最怕三件事：写错分区、频繁擦写、掉电写坏数据。工程上要靠分区隔离、低频写入、CRC/双备份来解决。**

[1]: https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/peripherals/spi_flash/index.html "SPI Flash API - ESP32-S3 -  — ESP-IDF Programming Guide v6.0.1 documentation"
[2]: https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/storage/partition.html "Partitions API - ESP32-S3 -  — ESP-IDF Programming Guide v6.0.1 documentation"
[3]: https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/storage/wear-levelling.html "Wear Levelling API - ESP32-S3 -  — ESP-IDF Programming Guide v6.0.1 documentation"
[4]: https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/storage/nvs_flash.html "Non-Volatile Storage Library - ESP32-S3 -  — ESP-IDF Programming Guide v6.0.1 documentation"
[5]: https://docs.espressif.com/projects/esp-idf/en/v4.4.1/esp32s3/api-reference/storage/nvs_flash.html "Non-volatile storage library - ESP32-S3 -  — ESP-IDF Programming Guide v4.4.1 documentation"
[6]: https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/peripherals/spi_flash/spi_flash_concurrency.html "Concurrency Constraints for Flash on SPI1 - ESP32-S3 -  — ESP-IDF Programming Guide v6.0.1 documentation"
[7]: https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/security/flash-encryption.html "Flash Encryption - ESP32-S3 -  — ESP-IDF Programming Guide v6.0.1 documentation"
[8]: https://docs.espressif.com/projects/esp-idf/en/v5.1.3/esp32s3/api-guides/partition-tables.html "Partition Tables - ESP32-S3 -  — ESP-IDF Programming Guide v5.1.3 documentation"


----
----
----
下面按 **ESP-IDF** 来讲。ESP32 的 Flash 不是 MCU 内部像 STM32 那样的片上 Flash，而是外接的 **SPI NOR Flash**，程序、分区表、NVS 参数、文件系统、OTA 固件等都放在这颗 Flash 里。

## 1. ESP32 的 Flash 里一般放什么？

一个典型 ESP32 Flash 布局大概是这样：

```text
Flash 物理地址
0x0000      一些启动相关区域
0x1000      bootloader 二级启动程序
0x8000      partition table 分区表
0x9000      nvs 参数区
0xF000      phy_init 射频参数区
0x10000     factory app 应用程序
后面        ota_0 / ota_1 / spiffs / fatfs / 自定义数据区 ...
```

ESP-IDF 默认会在 **0x8000** 放分区表，分区表描述 Flash 里哪些区域是 app、哪些区域是 nvs、哪些区域是文件系统等；默认 factory app 通常从 **0x10000** 开始。官方文档也明确说，ESP32 单颗 Flash 可以包含多个 app 和多种 data 区域，比如校准数据、文件系统、参数存储等。([Espressif Systems][1])

---

## 2. Flash 的基本特性：为什么写之前要擦除？

SPI NOR Flash 有一个很重要的特点：

```text
擦除后：所有 bit = 1，也就是 0xFF
写入时：只能把 1 写成 0
不能直接把 0 写回 1
```

所以你不能像 RAM 一样随便改一个字节。

举个例子：

```text
原始：1111 1111 = 0xFF
写入：1010 0101 = 0xA5    可以，因为只是 1 -> 0

现在：1010 0101 = 0xA5
想写：1111 0000 = 0xF0    不行，因为有些 bit 要 0 -> 1
```

必须先擦除一个扇区，让它重新变成 0xFF，再写入。ESP-IDF 文档里 `esp_flash_write()` 也说明，写入地址必须提前擦除，因为 SPI NOR Flash 只能把 bit 从 1 写成 0。([Espressif Systems][2])

ESP32 Flash 擦除通常按 sector 对齐，典型扇区大小是 **4096 字节，也就是 4KB**。擦除接口要求起始地址和长度都按 sector 对齐，否则会返回错误。([Espressif Systems][2])

---

## 3. ESP32 上读写 Flash 有几种方式？

不要一上来就直接操作物理地址。ESP-IDF 推荐优先用高级 API，而不是直接裸操作主 Flash。官方 Storage API 也建议使用 `esp_partition` 或文件系统这类高级 API 来访问 SPI NOR Flash。([Espressif Systems][3])

常见方式如下：

| 使用场景                       | 推荐方式                          |
| -------------------------- | ----------------------------- |
| 保存 Wi-Fi 账号、设备配置、校准参数、小结构体 | **NVS**                       |
| 保存较大的二进制数据、日志块、升级标志、自定义数据区 | **esp_partition**             |
| 保存图片、音频、网页文件、配置文件          | **SPIFFS / LittleFS / FATFS** |
| 直接访问外部第二颗 SPI Flash        | **esp_flash**                 |
| 直接操作主 Flash 物理地址           | 不推荐，除非你非常清楚地址布局               |

---

# 4. 最推荐：用 NVS 保存参数

NVS 可以理解成 ESP32 官方提供的 **Flash 版 key-value 数据库**。

适合保存：

```text
wifi_ssid
wifi_password
volume
brightness
calibration_value
device_id
boot_count
```

NVS 内部已经做了磨损均衡设计。官方文档说明，NVS 写入新 key-value 时会追加写入，旧值会被标记无效，不会每次更新都立刻擦除；对适合单 entry 存储的数据，能显著降低擦除频率。([Espressif Systems][4])

## NVS 写入一个整数

```c
#include <stdio.h>
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_err.h"

void nvs_write_example(void)
{
    // 1. 初始化 NVS Flash
    esp_err_t err = nvs_flash_init();

    // 如果 NVS 分区满了，或者版本不匹配，通常需要擦除后重新初始化
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }

    ESP_ERROR_CHECK(err);

    // 2. 打开一个 namespace
    // namespace 可以理解成一个分类，比如 "storage"
    nvs_handle_t handle;
    err = nvs_open("storage", NVS_READWRITE, &handle);
    ESP_ERROR_CHECK(err);

    // 3. 写入一个 key-value
    int32_t boot_count = 123;
    err = nvs_set_i32(handle, "boot_cnt", boot_count);
    ESP_ERROR_CHECK(err);

    // 4. 提交
    // 注意：nvs_set_xxx 后必须 nvs_commit，数据才保证真正写入 Flash
    err = nvs_commit(handle);
    ESP_ERROR_CHECK(err);

    // 5. 关闭句柄
    nvs_close(handle);
}
```

ESP-IDF 里 `nvs_open()` 用于打开 NVS 命名空间，后续通过 `nvs_get_*`、`nvs_set_*`、`nvs_commit()` 操作对应数据。([Espressif Systems][4])
另外，官方文档明确说，调用 `nvs_set_*` 后，底层存储不一定马上更新，必须调用 `nvs_commit()` 才能确保写入非易失存储。([Espressif Systems][4])

## NVS 读取一个整数

```c
#include <stdio.h>
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_err.h"

void nvs_read_example(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());

    nvs_handle_t handle;
    ESP_ERROR_CHECK(nvs_open("storage", NVS_READONLY, &handle));

    int32_t boot_count = 0;

    esp_err_t err = nvs_get_i32(handle, "boot_cnt", &boot_count);

    if (err == ESP_OK) {
        printf("读取到 boot_cnt = %ld\n", boot_count);
    } else if (err == ESP_ERR_NVS_NOT_FOUND) {
        printf("boot_cnt 还没有写入过，使用默认值 0\n");
    } else {
        printf("读取失败 err = %s\n", esp_err_to_name(err));
    }

    nvs_close(handle);
}
```

官方文档里 `nvs_get_i32()` 的例子也说明，如果 key 不存在，会返回 `ESP_ERR_NVS_NOT_FOUND`，输出变量可以保留默认值。([Espressif Systems][4])

---

# 5. 保存结构体：用 NVS Blob

比如你有设备配置：

```c
typedef struct {
    int volume;
    int brightness;
    float weight_cal;
} device_config_t;
```

可以用 `nvs_set_blob()` / `nvs_get_blob()` 保存。

```c
#include <stdio.h>
#include <string.h>
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_err.h"

typedef struct {
    int volume;
    int brightness;
    float weight_cal;
} device_config_t;

void save_config(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());

    nvs_handle_t handle;
    ESP_ERROR_CHECK(nvs_open("config", NVS_READWRITE, &handle));

    device_config_t cfg = {
        .volume = 60,
        .brightness = 80,
        .weight_cal = 1.234f,
    };

    // 把整个结构体当成二进制 blob 写入
    ESP_ERROR_CHECK(nvs_set_blob(handle, "dev_cfg", &cfg, sizeof(cfg)));

    // 提交到 Flash
    ESP_ERROR_CHECK(nvs_commit(handle));

    nvs_close(handle);
}

void load_config(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());

    nvs_handle_t handle;
    ESP_ERROR_CHECK(nvs_open("config", NVS_READONLY, &handle));

    device_config_t cfg = {0};
    size_t required_size = sizeof(cfg);

    esp_err_t err = nvs_get_blob(handle, "dev_cfg", &cfg, &required_size);

    if (err == ESP_OK && required_size == sizeof(cfg)) {
        printf("volume = %d\n", cfg.volume);
        printf("brightness = %d\n", cfg.brightness);
        printf("weight_cal = %.3f\n", cfg.weight_cal);
    } else {
        printf("没有读取到配置，使用默认配置\n");
    }

    nvs_close(handle);
}
```

这个方式很适合保存你的产品参数，比如称重校准系数、音量、屏幕亮度、设备模式、PID 参数等。

---

# 6. 自定义 Flash 分区读写：esp_partition

如果你想自己规划一块 Flash，比如专门存日志：

```text
log_data 分区：64KB
用于保存产品运行日志、错误码、传感器历史数据
```

那就可以用 `esp_partition_read()`、`esp_partition_write()`、`esp_partition_erase_range()`。

ESP-IDF 的 `esp_partition` API 是基于底层 SPI Flash API 的高级封装，可以按分区边界操作 Flash；官方文档说明 `esp_partition_read()`、`esp_partition_write()`、`esp_partition_erase_range()` 类似底层 flash 读写擦除，但会在分区范围内操作。([Espressif Systems][5])

## 6.1 自定义分区表

例如新建一个 `partitions.csv`：

```csv
# Name,     Type, SubType, Offset,   Size,   Flags
nvs,        data, nvs,     0x9000,   0x6000,
phy_init,   data, phy,     0xf000,   0x1000,
factory,    app,  factory, 0x10000,  1M,
log_data,   data, 0x40,    ,         64K,
```

这里的 `log_data` 就是你自己的数据区。ESP-IDF 文档里说明，分区表每一项包含 name、type、subtype、offset、size 等字段；如果 offset 留空，工具会根据前一个分区自动计算。([Espressif Systems][1])

然后在：

```text
idf.py menuconfig
```

里面选择：

```text
Partition Table
    -> Custom partition table CSV
```

填入你的 `partitions.csv`。

---

## 6.2 esp_partition 写入和读取示例

```c
#include <stdio.h>
#include <string.h>
#include "esp_partition.h"
#include "esp_err.h"

#define LOG_PARTITION_NAME "log_data"

void partition_write_read_example(void)
{
    // 1. 查找名为 log_data 的分区
    const esp_partition_t *part = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA,      // 分区类型：data
        ESP_PARTITION_SUBTYPE_ANY,    // 子类型：任意
        LOG_PARTITION_NAME            // 分区名字
    );

    if (part == NULL) {
        printf("没有找到 log_data 分区\n");
        return;
    }

    printf("找到分区: label=%s, address=0x%lx, size=%lu\n",
           part->label, part->address, part->size);

    // 2. 要写入的数据
    const char *write_data = "Hello ESP32 Flash Partition!";

    // 3. 写 Flash 前必须先擦除
    // 注意：擦除范围必须 4KB 对齐
    ESP_ERROR_CHECK(esp_partition_erase_range(part, 0, 4096));

    // 4. 写入数据
    // offset = 0，表示写到这个分区内部的起始位置
    ESP_ERROR_CHECK(esp_partition_write(
        part,
        0,
        write_data,
        strlen(write_data) + 1
    ));

    // 5. 读取数据
    char read_buf[64] = {0};
    ESP_ERROR_CHECK(esp_partition_read(
        part,
        0,
        read_buf,
        sizeof(read_buf)
    ));

    printf("读取到: %s\n", read_buf);
}
```

注意这个 offset 不是 Flash 物理绝对地址，而是 **分区内部偏移**。比如 `log_data` 实际物理地址是 `0x110000`，你传入 offset `0`，实际访问的是：

```text
0x110000 + 0
```

传入 offset `100`，实际访问的是：

```text
0x110000 + 100
```

---

# 7. 底层 esp_flash_* 接口

底层接口包括：

```c
esp_flash_read()
esp_flash_write()
esp_flash_erase_region()
esp_flash_erase_chip()
esp_flash_get_chip_size()
```

官方文档说明这些函数分别用于 Flash 到 RAM 读取、RAM 到 Flash 写入、擦除指定区域、擦除整片 Flash、获取 Flash 大小。([Espressif Systems][2])

但是对主 Flash，也就是运行程序的那颗 Flash，官方建议尽量避免直接用裸 `esp_flash_*`，而是优先用分区 API。([Espressif Systems][2])

底层接口更适合这些情况：

```text
1. 你外接了第二颗 SPI Flash
2. 你正在写底层存储驱动
3. 你非常清楚物理地址布局
4. 你需要做特殊的 Flash 测试
```

---

# 8. 什么时候用 NVS，什么时候用 esp_partition？

简单记：

```text
小参数：NVS
大块数据：esp_partition
文件：SPIFFS / LittleFS / FATFS
```

比如你的项目里：

| 数据               | 推荐方式                     |
| ---------------- | ------------------------ |
| Wi-Fi SSID / 密码  | NVS                      |
| 设备 SN / MAC 绑定信息 | NVS                      |
| PID 参数           | NVS                      |
| 称重校准系数           | NVS                      |
| 错误日志             | esp_partition 或 FATFS    |
| 图片资源             | SPIFFS / LittleFS        |
| 音频文件             | FATFS / SPIFFS / 外部 SD 卡 |
| OTA 固件           | OTA app 分区               |
| 大量连续采样数据         | esp_partition / FATFS    |

---

# 9. 写 Flash 的几个坑

## 坑 1：不要高频写 Flash

比如你 10ms 写一次 Flash：

```text
100 次/秒
6000 次/分钟
360000 次/小时
```

这样很快就会把 Flash 某些扇区磨坏。

Flash 适合保存“偶尔变化”的东西，不适合当 RAM 用。官方 Wear Levelling 文档也说明，SPI Flash 每个扇区擦写次数有限，磨损均衡的作用就是把磨损尽量分散到多个扇区。([Espressif Systems][6])

## 坑 2：写之前要擦除

裸 `esp_partition_write()` 不会帮你自动擦除。你要自己先：

```c
esp_partition_erase_range(part, offset, size);
```

再：

```c
esp_partition_write(part, offset, data, len);
```

## 坑 3：擦除很慢，别在中断里操作 Flash

Flash 擦除可能比较耗时。官方 SPI Flash 文档提到，较长的擦除操作会占用 CPU，可能影响任务执行，甚至和任务看门狗有关。([Espressif Systems][2])

所以不要这样：

```c
void IRAM_ATTR gpio_isr_handler(void *arg)
{
    // 错误示范：不要在中断里写 Flash
    esp_partition_write(...);
}
```

正确做法是：

```text
中断里只置标志位 / 发队列
任务里再写 Flash
```

## 坑 4：改了分区表后，最好整片擦除一次

如果你改了 `partitions.csv`，旧数据可能还留在原来的 Flash 地址上。ESP-IDF 文档也提醒，更新分区表不会自动擦除旧分区数据，可以用 `idf.py erase-flash` 或 `esptool erase-flash` 擦除整片 Flash。([Espressif Systems][1])

---

# 10. 最小实践建议

你现在初学 ESP32 Flash，我建议这样用：

第一阶段，只用 NVS：

```text
保存配置参数
保存校准参数
保存开机次数
保存设备模式
```

第二阶段，再用自定义 partition：

```text
保存日志
保存历史采样数据
保存较大的二进制块
```

第三阶段，再上文件系统：

```text
网页文件
图片
音频
JSON 配置文件
```

最常用的 Flash 写入逻辑可以记成：

```text
NVS:
nvs_flash_init()
nvs_open()
nvs_set_xxx()
nvs_commit()
nvs_close()

自定义分区:
esp_partition_find_first()
esp_partition_erase_range()
esp_partition_write()
esp_partition_read()
```

一句话总结：**ESP32 的 Flash 不建议直接当普通数组随便读写；小参数用 NVS，大数据用分区，文件用文件系统，底层裸 Flash API 尽量少碰。**

[1]: https://docs.espressif.com/projects/esp-idf/zh_CN/stable/esp32/api-guides/partition-tables.html "分区表 - ESP32 -  — ESP-IDF 编程指南 v6.0.1 文档"
[2]: https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/spi_flash/index.html "SPI Flash API - ESP32 -  — ESP-IDF Programming Guide v6.0.1 documentation"
[3]: https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/storage/index.html "Storage API - ESP32 -  — ESP-IDF Programming Guide v6.0.1 documentation"
[4]: https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/storage/nvs_flash.html "Non-Volatile Storage Library - ESP32 -  — ESP-IDF Programming Guide v6.0.1 documentation"
[5]: https://docs.espressif.com/projects/esp-idf/en/stable/esp32c2/api-reference/storage/partition.html "Partitions API - ESP32-C2 -  — ESP-IDF Programming Guide v6.0.1 documentation"
[6]: https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/storage/wear-levelling.html "Wear Levelling API - ESP32 -  — ESP-IDF Programming Guide v6.0.1 documentation"

------
------
----

你的 **ESP32-S3 N16R8** 可以理解成：

```text
N16 = 16MB Flash
R8  = 8MB PSRAM
```

官方 ESP32-S3-WROOM-1 表格里，`ESP32-S3-WROOM-1-N16R8` 对应 **16 MB Quad SPI Flash + 8 MB Octal SPI PSRAM**。NVS 存在 **Flash** 里面，不存在 PSRAM 里面；PSRAM 是运行时内存，掉电就没了。

---

# 1. NVS 是什么？

NVS 全称是：

```text
Non-Volatile Storage
非易失性存储
```

你可以把它理解成 ESP32 官方给你封装好的 **Flash 小型数据库**。

它不是文件系统，而是 **key-value 键值对存储**。

比如：

```text
key = "volume"       value = 80
key = "wifi_ssid"    value = "TP-LINK_xxx"
key = "weight_cal"   value = 一个校准系数
key = "pid_cfg"      value = 一个结构体 blob
```

官方文档说明，NVS 就是设计用来把 key-value 数据存到 Flash 里的；key 是 ASCII 字符串，目前最大长度是 15 个字符，value 支持整数、字符串和 blob 二进制数据。([Espressif Systems][1])

---

# 2. NVS 适合存什么？

非常适合存这些：

```text
Wi-Fi SSID / 密码
设备 SN
音量
亮度
屏幕方向
语言设置
PID 参数
称重校准系数
猫砂盆去皮重量
用户配置
开机次数
OTA 标志位
```

不适合存这些：

```text
大量日志
高频传感器数据
音频文件
图片文件
大 JSON 文件
每 10ms 更新一次的数据
```

官方也明确说，NVS 更适合保存很多小值，不适合保存少量很大的 string/blob；大文件更建议用 FAT 文件系统加 wear levelling。([Espressif Systems][1])

---

# 3. NVS 和你的 16MB Flash 有什么关系？

你的 N16R8 有 16MB Flash，但这不代表 NVS 默认就是 16MB。

Flash 会被分区表切成很多块：

```text
bootloader
partition table
nvs
phy_init
factory app
ota_0
ota_1
spiffs / littlefs
自定义数据区
```

ESP-IDF 默认单 factory app 分区里，NVS 通常是：

```csv
nvs, data, nvs, 0x9000, 0x6000
```

也就是：

```text
起始地址：0x9000
大小：0x6000 = 24KB
```

官方分区表示例里也能看到默认 `nvs` 分区在 `0x9000`，大小可能是 `0x6000` 或 OTA 表里的 `0x4000`。([Espressif Systems][2])

所以你要记住一句话：

> **ESP32-S3 N16R8 有 16MB Flash，但 NVS 能用多大，取决于 partition table 里 nvs 分区配置多大。**

如果你想让 NVS 更大，可以改分区表。

---

# 4. NVS 的几个核心概念

## 4.1 Partition：NVS 分区

默认分区名字一般叫：

```text
nvs
```

分区表里通常长这样：

```csv
nvs, data, nvs, 0x9000, 0x6000
```

意思是：

```text
分区名字：nvs
分区类型：data
分区子类型：nvs
起始地址：0x9000
大小：0x6000
```

---

## 4.2 Namespace：命名空间

namespace 可以理解成“分类”。

比如你可以这样分：

```text
"wifi"      专门存 Wi-Fi 参数
"motor"     专门存电机参数
"weight"    专门存称重参数
"system"    专门存系统参数
```

官方文档说明，每个 key-value 都属于某个 namespace，namespace 名字规则和 key 类似。([Espressif Systems][1])

举例：

```text
namespace = "weight"
key       = "cal"
value     = 1.2345
```

再比如：

```text
namespace = "motor"
key       = "kp"
value     = 0.8
```

这样不同模块之间不会乱。

---

## 4.3 Key：键名

key 是字符串，比如：

```text
"boot_cnt"
"volume"
"ssid"
"pwd"
"cal"
"pid"
```

注意：**key 名字不要太长**。官方文档说 key 最大长度目前是 15 个字符。([Espressif Systems][1])

所以不要这样：

```c
"cat_weight_calibration_value"
```

太长了。

可以改成：

```c
"weight_cal"
"cal"
"cat_cal"
```

---

## 4.4 Value：值

NVS 支持：

```text
整数：u8 / i8 / u16 / i16 / u32 / i32 / u64 / i64
字符串：string
二进制：blob
```

结构体一般用 blob 保存。

---

# 5. NVS 最基本读写流程

写入流程：

```text
nvs_flash_init()
nvs_open()
nvs_set_xxx()
nvs_commit()
nvs_close()
```

读取流程：

```text
nvs_flash_init()
nvs_open()
nvs_get_xxx()
nvs_close()
```

重点是：

```c
nvs_commit()
```

只要你调用了 `nvs_set_xxx()`，后面一定要 `nvs_commit()`。

官方文档说明，设置值之后必须调用 `nvs_commit()`，才能确保更改写入非易失性存储；`nvs_close()` 并不会自动保证提交。([Espressif Systems][1])

---

# 6. 最小 NVS 初始化代码

建议你工程里封装一个初始化函数：

```c
#include "nvs_flash.h"
#include "esp_err.h"

void app_nvs_init(void)
{
    esp_err_t err = nvs_flash_init();

    /*
     * ESP_ERR_NVS_NO_FREE_PAGES:
     * NVS 分区没有空闲页了，可能是分区表变化导致旧数据不兼容。
     *
     * ESP_ERR_NVS_NEW_VERSION_FOUND:
     * 当前固件使用的 NVS 版本比 Flash 里旧数据版本不兼容。
     */
    if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
        err == ESP_ERR_NVS_NEW_VERSION_FOUND) {

        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }

    ESP_ERROR_CHECK(err);
}
```

然后在 `app_main()` 一开始调用：

```c
void app_main(void)
{
    app_nvs_init();

    // 后面再初始化 Wi-Fi、LVGL、传感器等
}
```

---

# 7. 保存一个 int 参数

比如保存音量：

```c
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_err.h"
#include <stdio.h>

void save_volume(int32_t volume)
{
    nvs_handle_t handle;

    // 打开 namespace = "system"
    ESP_ERROR_CHECK(nvs_open("system", NVS_READWRITE, &handle));

    // 写入 key = "volume"
    ESP_ERROR_CHECK(nvs_set_i32(handle, "volume", volume));

    // 提交到 Flash，这一步非常重要
    ESP_ERROR_CHECK(nvs_commit(handle));

    // 关闭句柄
    nvs_close(handle);
}
```

读取音量：

```c
int32_t load_volume(void)
{
    nvs_handle_t handle;
    int32_t volume = 50;   // 默认值

    ESP_ERROR_CHECK(nvs_open("system", NVS_READONLY, &handle));

    esp_err_t err = nvs_get_i32(handle, "volume", &volume);

    if (err == ESP_ERR_NVS_NOT_FOUND) {
        printf("NVS 没有 volume，使用默认值 %ld\n", volume);
    } else if (err == ESP_OK) {
        printf("NVS 读取 volume = %ld\n", volume);
    } else {
        printf("读取 volume 失败: %s\n", esp_err_to_name(err));
    }

    nvs_close(handle);

    return volume;
}
```

---

# 8. 保存字符串，比如 Wi-Fi SSID

```c
void save_wifi_ssid(const char *ssid)
{
    nvs_handle_t handle;

    ESP_ERROR_CHECK(nvs_open("wifi", NVS_READWRITE, &handle));

    ESP_ERROR_CHECK(nvs_set_str(handle, "ssid", ssid));

    ESP_ERROR_CHECK(nvs_commit(handle));

    nvs_close(handle);
}
```

读取字符串：

```c
void load_wifi_ssid(void)
{
    nvs_handle_t handle;
    char ssid[64] = {0};
    size_t len = sizeof(ssid);

    ESP_ERROR_CHECK(nvs_open("wifi", NVS_READONLY, &handle));

    esp_err_t err = nvs_get_str(handle, "ssid", ssid, &len);

    if (err == ESP_OK) {
        printf("ssid = %s\n", ssid);
    } else if (err == ESP_ERR_NVS_NOT_FOUND) {
        printf("没有保存过 ssid\n");
    } else {
        printf("读取 ssid 失败: %s\n", esp_err_to_name(err));
    }

    nvs_close(handle);
}
```

字符串也有限制。官方文档说，NVS string 当前限制为 **4000 字节**，包括字符串结尾的 `\0`。([Espressif Systems][1])

---

# 9. 保存结构体：最适合嵌入式项目

比如你做猫砂盆称重，有一堆参数：

```c
typedef struct {
    uint32_t magic;
    uint32_t version;

    float weight_cal;       // 称重校准系数
    int32_t weight_tare;    // 去皮重量
    uint32_t cat_count;     // 猫数量

    int32_t volume;         // 提示音音量
    int32_t brightness;     // 屏幕亮度
} app_config_t;
```

建议加 `magic` 和 `version`，这样以后结构体升级时不容易乱。

```c
#define APP_CFG_MAGIC   0xA5A55A5A
#define APP_CFG_VERSION 1
```

保存配置：

```c
void save_app_config(const app_config_t *cfg)
{
    nvs_handle_t handle;

    ESP_ERROR_CHECK(nvs_open("app_cfg", NVS_READWRITE, &handle));

    ESP_ERROR_CHECK(nvs_set_blob(
        handle,
        "cfg",
        cfg,
        sizeof(app_config_t)
    ));

    ESP_ERROR_CHECK(nvs_commit(handle));

    nvs_close(handle);
}
```

读取配置：

```c
void load_app_config(app_config_t *cfg)
{
    nvs_handle_t handle;

    // 先给默认值
    app_config_t default_cfg = {
        .magic = APP_CFG_MAGIC,
        .version = APP_CFG_VERSION,
        .weight_cal = 1.0f,
        .weight_tare = 0,
        .cat_count = 0,
        .volume = 60,
        .brightness = 80,
    };

    *cfg = default_cfg;

    esp_err_t err = nvs_open("app_cfg", NVS_READONLY, &handle);
    if (err != ESP_OK) {
        return;
    }

    size_t size = sizeof(app_config_t);

    err = nvs_get_blob(handle, "cfg", cfg, &size);

    nvs_close(handle);

    if (err != ESP_OK || size != sizeof(app_config_t)) {
        // 没读到，或者大小不对，使用默认配置
        *cfg = default_cfg;
        return;
    }

    if (cfg->magic != APP_CFG_MAGIC ||
        cfg->version != APP_CFG_VERSION) {
        // 配置格式不匹配，使用默认配置
        *cfg = default_cfg;
        return;
    }
}
```

这套结构很适合你以后保存：

```text
称重校准系数
去皮重量
小猫信息
勿扰时间
电机参数
PID 参数
屏幕设置
设备运行模式
```

---

# 10. NVS 底层是怎么保护 Flash 的？

NVS 不是你每改一次参数就原地覆盖 Flash。

它内部更像一个日志结构：

```text
旧值不直接覆盖
新值追加写入
旧值标记为 erased
空间不够时再整理 / 擦除页
```

官方文档说明，NVS 会顺序存储 key-value；当某个 key 更新时，新 key-value 会追加到日志末尾，旧的 key-value 会被标记为 erased。([Espressif Systems][1])

所以 NVS 比你自己裸写 Flash 更安全，也更适合保存参数。

但是注意：这不代表可以高频写。

比如：

```text
10ms 写一次 NVS
1 秒 100 次
1 分钟 6000 次
1 小时 36 万次
```

这非常危险。

正确做法：

```text
参数变化后，先存在 RAM 变量里
延迟几秒再写 NVS
或者只在关机前 / 模式切换 / 用户确认时写
```

比如音量调节：

```text
用户连续按音量 +++++
不要每按一次就写 Flash
可以 3 秒没再操作后，再统一写一次
```

---

# 11. 你的 N16R8 建议怎么配置 NVS？

因为你是 **16MB Flash**，空间比较大。如果你只是普通配置，默认 24KB NVS 够用。

如果你的产品参数较多，比如：

```text
Wi-Fi 配网信息
天气城市配置
用户偏好
多只猫的信息
称重校准历史
错误码
少量运行记录
```

可以把 NVS 调大一点，比如 64KB 或 128KB。

例如自定义 `partitions.csv`：

```csv
# Name,     Type, SubType, Offset,   Size,    Flags
nvs,        data, nvs,     0x9000,   0x10000,
phy_init,   data, phy,     0x19000,  0x1000,
factory,    app,  factory, 0x20000,  2M,
```

这里：

```text
nvs size = 0x10000 = 64KB
```

如果你要 OTA，可以这样：

```csv
# Name,     Type, SubType, Offset,   Size,    Flags
nvs,        data, nvs,     0x9000,   0x10000,
otadata,    data, ota,     0x19000,  0x2000,
phy_init,   data, phy,     0x1B000,  0x1000,
factory,    app,  factory, 0x20000,  2M,
ota_0,      app,  ota_0,   ,         3M,
ota_1,      app,  ota_1,   ,         3M,
storage,    data, spiffs,  ,         6M,
```

你这个 N16R8 是 16MB Flash，所以可以比较舒服地分 OTA、文件系统和 NVS。

---

# 12. menuconfig 里要注意 Flash Size

你是 N16R8，建议检查：

```text
idf.py menuconfig
```

里面：

```text
Serial flasher config
    Flash size
        16 MB
```

否则有些工程默认 2MB / 4MB，可能出现：

```text
app 分区太小
烧录地址不对
分区表没有充分使用 16MB
文件系统空间很小
```

---

# 13. NVS 常见错误

## 13.1 忘记 nvs_commit()

错误写法：

```c
nvs_set_i32(handle, "volume", 80);
nvs_close(handle);
```

正确写法：

```c
nvs_set_i32(handle, "volume", 80);
nvs_commit(handle);
nvs_close(handle);
```

---

## 13.2 key 名字太长

错误：

```c
nvs_set_i32(handle, "cat_weight_calibration_value", 100);
```

建议：

```c
nvs_set_i32(handle, "weight_cal", 100);
```

---

## 13.3 频繁写 NVS

错误：

```c
while (1) {
    nvs_set_i32(handle, "weight", weight);
    nvs_commit(handle);
    vTaskDelay(pdMS_TO_TICKS(10));
}
```

正确思路：

```text
实时重量存在 RAM
稳定后的结果再保存 NVS
历史曲线数据不要用 NVS，应该用文件系统或自定义分区
```

---

## 13.4 结构体升级没做版本管理

第一版：

```c
typedef struct {
    int volume;
} app_config_t;
```

第二版变成：

```c
typedef struct {
    int volume;
    int brightness;
    float weight_cal;
} app_config_t;
```

如果不加 `version`，旧 Flash 里的数据可能被新固件误读。

所以建议结构体里加：

```c
uint32_t magic;
uint32_t version;
```

---

# 14. 一个适合你项目的 NVS 封装模板

你可以建两个文件：

```text
app_nvs.h
app_nvs.c
```

## app_nvs.h

```c
#pragma once

#include <stdint.h>

#define APP_CFG_MAGIC   0xA5A55A5A
#define APP_CFG_VERSION 1

typedef struct {
    uint32_t magic;
    uint32_t version;

    float weight_cal;
    int32_t weight_tare;

    int32_t volume;
    int32_t brightness;

    int32_t pid_kp;
    int32_t pid_ki;
    int32_t pid_kd;
} app_config_t;

void app_nvs_init(void);
void app_config_load(app_config_t *cfg);
void app_config_save(const app_config_t *cfg);
```

## app_nvs.c

```c
#include "app_nvs.h"

#include <string.h>
#include <stdio.h>

#include "nvs.h"
#include "nvs_flash.h"
#include "esp_err.h"

#define NVS_NAMESPACE   "app_cfg"
#define NVS_KEY_CONFIG  "cfg"

void app_nvs_init(void)
{
    esp_err_t err = nvs_flash_init();

    if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
        err == ESP_ERR_NVS_NEW_VERSION_FOUND) {

        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }

    ESP_ERROR_CHECK(err);
}

static void app_config_set_default(app_config_t *cfg)
{
    memset(cfg, 0, sizeof(app_config_t));

    cfg->magic = APP_CFG_MAGIC;
    cfg->version = APP_CFG_VERSION;

    cfg->weight_cal = 1.0f;
    cfg->weight_tare = 0;

    cfg->volume = 60;
    cfg->brightness = 80;

    cfg->pid_kp = 100;
    cfg->pid_ki = 0;
    cfg->pid_kd = 0;
}

void app_config_load(app_config_t *cfg)
{
    app_config_t default_cfg;
    app_config_set_default(&default_cfg);

    *cfg = default_cfg;

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);

    if (err != ESP_OK) {
        printf("NVS open failed, use default config\n");
        return;
    }

    size_t size = sizeof(app_config_t);

    err = nvs_get_blob(handle, NVS_KEY_CONFIG, cfg, &size);

    nvs_close(handle);

    if (err != ESP_OK) {
        printf("NVS config not found, use default config\n");
        *cfg = default_cfg;
        return;
    }

    if (size != sizeof(app_config_t)) {
        printf("NVS config size mismatch, use default config\n");
        *cfg = default_cfg;
        return;
    }

    if (cfg->magic != APP_CFG_MAGIC ||
        cfg->version != APP_CFG_VERSION) {
        printf("NVS config version mismatch, use default config\n");
        *cfg = default_cfg;
        return;
    }

    printf("NVS config loaded\n");
}

void app_config_save(const app_config_t *cfg)
{
    nvs_handle_t handle;

    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);

    if (err != ESP_OK) {
        printf("NVS open failed: %s\n", esp_err_to_name(err));
        return;
    }

    err = nvs_set_blob(handle, NVS_KEY_CONFIG, cfg, sizeof(app_config_t));

    if (err != ESP_OK) {
        printf("NVS set blob failed: %s\n", esp_err_to_name(err));
        nvs_close(handle);
        return;
    }

    err = nvs_commit(handle);

    if (err != ESP_OK) {
        printf("NVS commit failed: %s\n", esp_err_to_name(err));
    }

    nvs_close(handle);
}
```

使用：

```c
void app_main(void)
{
    app_nvs_init();

    app_config_t cfg;
    app_config_load(&cfg);

    printf("volume = %ld\n", cfg.volume);
    printf("brightness = %ld\n", cfg.brightness);
    printf("weight_cal = %.3f\n", cfg.weight_cal);

    cfg.volume = 80;
    cfg.weight_cal = 1.234f;

    app_config_save(&cfg);
}
```

---

# 15. 总结

你的 **ESP32-S3 N16R8**：

```text
16MB Flash：程序、NVS、文件系统、OTA 都在这里
8MB PSRAM：运行时扩展内存，掉电丢失
```

NVS 可以理解成：

```text
Flash 上的小型 key-value 参数数据库
```

最常用流程：

```c
nvs_flash_init();

nvs_open("namespace", NVS_READWRITE, &handle);

nvs_set_i32(handle, "key", value);
// 或 nvs_set_str()
// 或 nvs_set_blob()

nvs_commit(handle);

nvs_close(handle);
```

实际项目建议：

```text
小参数、配置、校准值：用 NVS
大日志、大文件、图片、音频：不用 NVS，用 LittleFS / SPIFFS / FATFS
高频实时数据：先放 RAM，别频繁写 Flash
结构体配置：用 nvs_set_blob + magic + version
```

一句话：**NVS 是 ESP32 保存“掉电不丢的小参数”的首选方式。**

[1]: https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/storage/nvs_flash.html "Non-Volatile Storage Library - ESP32-S3 -  — ESP-IDF Programming Guide v6.0.1 documentation"
[2]: https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-guides/partition-tables.html "Partition Tables - ESP32-S3 -  — ESP-IDF Programming Guide v6.0.1 documentation"



----
----
-----

ESP32-S3 上“操作 Flash”一般不要直接拿物理地址乱写，最推荐分 3 层理解：

```text
第 1 层：NVS
保存小参数，比如 Wi-Fi、音量、校准值、PID 参数。

第 2 层：esp_partition
自己划一块 Flash 分区，然后读、写、擦除。
适合日志、较大的结构体、二进制数据。

第 3 层：esp_flash
最底层 Flash API，直接按物理地址操作。
初学阶段不建议直接用。
```

你现在真正想“操作 Flash”，建议先学 **esp_partition**。它比 NVS 更接近裸 Flash，但又不会让你误写到 bootloader、app、partition table 等危险区域。

---

# 1. ESP32 Flash 操作的本质

Flash 和 RAM 不一样。

RAM 可以这样：

```c
buf[0] = 0x12;
buf[0] = 0x34;
buf[0] = 0x56;
```

Flash 不行。Flash 有这个特点：

```text
擦除后：0xFF
写入时：只能 1 -> 0
不能直接 0 -> 1
```

所以 Flash 写入流程一定是：

```text
先擦除 erase
再写入 write
再读取 read
```

ESP-IDF 官方 `esp_partition_write()` 文档也说明，写 Flash 前要先用 `esp_partition_erase_range()` 擦除对应区域；擦除 offset 要按分区的 erase size 对齐，擦除 size 也必须是 erase size 的整数倍。一般 ESP32-S3 主 Flash 的扇区大小是 0x1000，也就是 4KB。([Espressif Systems][1])

---

# 2. 推荐方式：自己建一个 data 分区

比如你想专门留一块 Flash 给自己用，叫：

```text
user_data
```

新建一个 `partitions.csv`：

```csv
# Name,     Type, SubType, Offset,   Size,     Flags
nvs,        data, nvs,     0x9000,   0x10000,
phy_init,   data, phy,     0x19000,  0x1000,
factory,    app,  factory, 0x20000,  2M,
user_data,  data, 0x40,    ,         64K,
```

解释一下：

```text
nvs       ：NVS 参数区
phy_init  ：射频校准参数区
factory   ：你的 app 程序区
user_data ：你自己操作的 Flash 数据区
```

ESP-IDF 的分区表就是用来描述 Flash 里哪些区域是 app、哪些区域是 data、文件系统、参数区等；默认分区表在 Flash 的 0x8000 位置。([Espressif Systems][2])

然后进入：

```bash
idf.py menuconfig
```

选择：

```text
Partition Table
    -> Custom partition table CSV
```

填你的：

```text
partitions.csv
```

你的 ESP32-S3 N16R8 是 16MB Flash，建议也确认：

```text
Serial flasher config
    -> Flash size
        -> 16 MB
```

---

# 3. 最小 Flash 读写例程

这个例子会做 3 件事：

```text
1. 找到 user_data 分区
2. 擦除前 4KB
3. 写入字符串
4. 再读出来
```

```c
#include <stdio.h>
#include <string.h>

#include "esp_err.h"
#include "esp_log.h"
#include "esp_partition.h"

static const char *TAG = "FLASH_DEMO";

#define USER_PARTITION_NAME "user_data"

void flash_partition_demo(void)
{
    /*
     * 1. 查找分区表中名为 user_data 的分区
     *
     * ESP_PARTITION_TYPE_DATA:
     *      表示查找 data 类型分区
     *
     * ESP_PARTITION_SUBTYPE_ANY:
     *      表示不关心 subtype
     *
     * USER_PARTITION_NAME:
     *      分区名字，也就是 partitions.csv 里的 user_data
     */
    const esp_partition_t *part = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA,
        ESP_PARTITION_SUBTYPE_ANY,
        USER_PARTITION_NAME
    );

    if (part == NULL) {
        ESP_LOGE(TAG, "没有找到 user_data 分区");
        return;
    }

    ESP_LOGI(TAG, "找到分区: label=%s, address=0x%lx, size=%lu",
             part->label,
             part->address,
             part->size);

    /*
     * 2. 准备要写入的数据
     */
    const char *write_data = "Hello ESP32-S3 Flash!";

    /*
     * 3. 写 Flash 前，必须先擦除
     *
     * 注意：
     * 这里的 offset = 0，不是 Flash 物理地址 0。
     * 它表示 user_data 分区内部的偏移 0。
     *
     * 4096 = 4KB，一个 Flash sector。
     */
    esp_err_t err = esp_partition_erase_range(part, 0, 4096);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "擦除失败: %s", esp_err_to_name(err));
        return;
    }

    /*
     * 4. 写入 Flash
     *
     * 参数含义：
     * part                  ：目标分区
     * 0                     ：分区内部偏移
     * write_data            ：要写入的数据
     * strlen(write_data)+1  ：写入长度，+1 是为了把字符串结尾 '\0' 也写进去
     */
    err = esp_partition_write(
        part,
        0,
        write_data,
        strlen(write_data) + 1
    );

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "写入失败: %s", esp_err_to_name(err));
        return;
    }

    /*
     * 5. 从 Flash 读回来
     */
    char read_buf[64] = {0};

    err = esp_partition_read(
        part,
        0,
        read_buf,
        sizeof(read_buf)
    );

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "读取失败: %s", esp_err_to_name(err));
        return;
    }

    ESP_LOGI(TAG, "读取到的数据: %s", read_buf);
}
```

在 `app_main()` 里调用：

```c
void app_main(void)
{
    flash_partition_demo();
}
```

---

# 4. offset 是什么意思？

这是初学者最容易懵的地方。

假设你的分区表最终生成后，`user_data` 的物理地址是：

```text
0x220000
```

那你代码里：

```c
esp_partition_write(part, 0, data, len);
```

实际写的是：

```text
0x220000 + 0
```

你代码里：

```c
esp_partition_write(part, 100, data, len);
```

实际写的是：

```text
0x220000 + 100
```

所以 `esp_partition_write()` 里的 offset 是：

```text
分区内部偏移
```

不是整片 Flash 的绝对地址。

---

# 5. 保存结构体到 Flash

比如你想保存一个设备配置：

```c
typedef struct {
    uint32_t magic;
    uint32_t version;
    int32_t volume;
    int32_t brightness;
    float weight_cal;
} device_cfg_t;
```

可以这样写。

```c
#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "esp_partition.h"
#include "esp_err.h"

#define USER_PARTITION_NAME "user_data"

#define CFG_MAGIC   0xA5A55A5A
#define CFG_VERSION 1

typedef struct {
    uint32_t magic;
    uint32_t version;

    int32_t volume;
    int32_t brightness;

    float weight_cal;
} device_cfg_t;

static const char *TAG = "CFG_FLASH";

void flash_save_cfg(const device_cfg_t *cfg)
{
    const esp_partition_t *part = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA,
        ESP_PARTITION_SUBTYPE_ANY,
        USER_PARTITION_NAME
    );

    if (part == NULL) {
        ESP_LOGE(TAG, "没有找到 user_data 分区");
        return;
    }

    /*
     * 保存结构体前，先擦除一个 4KB sector。
     * 注意：这里只是简单演示。
     * 实际产品不要每次改一个参数就擦一整个 sector。
     */
    ESP_ERROR_CHECK(esp_partition_erase_range(part, 0, 4096));

    ESP_ERROR_CHECK(esp_partition_write(
        part,
        0,
        cfg,
        sizeof(device_cfg_t)
    ));

    ESP_LOGI(TAG, "配置已保存到 Flash");
}

void flash_load_cfg(device_cfg_t *cfg)
{
    const esp_partition_t *part = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA,
        ESP_PARTITION_SUBTYPE_ANY,
        USER_PARTITION_NAME
    );

    if (part == NULL) {
        ESP_LOGE(TAG, "没有找到 user_data 分区");
        return;
    }

    ESP_ERROR_CHECK(esp_partition_read(
        part,
        0,
        cfg,
        sizeof(device_cfg_t)
    ));

    if (cfg->magic != CFG_MAGIC || cfg->version != CFG_VERSION) {
        ESP_LOGW(TAG, "Flash 里的配置无效，使用默认配置");

        cfg->magic = CFG_MAGIC;
        cfg->version = CFG_VERSION;
        cfg->volume = 60;
        cfg->brightness = 80;
        cfg->weight_cal = 1.0f;

        return;
    }

    ESP_LOGI(TAG, "配置读取成功");
}
```

使用：

```c
void app_main(void)
{
    device_cfg_t cfg;

    flash_load_cfg(&cfg);

    printf("volume = %ld\n", cfg.volume);
    printf("brightness = %ld\n", cfg.brightness);
    printf("weight_cal = %.3f\n", cfg.weight_cal);

    cfg.volume = 80;
    cfg.brightness = 90;
    cfg.weight_cal = 1.234f;

    flash_save_cfg(&cfg);
}
```

---

# 6. 为什么保存结构体要加 magic 和 version？

因为 Flash 里可能是：

```text
1. 第一次烧录，里面全是 0xFF
2. 以前旧版本固件保存的数据
3. 乱数据
4. 结构体升级前的数据
```

所以你不能直接相信读出来的结构体。

加这个：

```c
uint32_t magic;
uint32_t version;
```

可以判断：

```text
magic 不对：说明不是你的配置
version 不对：说明结构体版本变了
```

这在嵌入式产品里很常用。

---

# 7. 擦除、写入、读取的关系

## 读取

读取最简单：

```c
esp_partition_read(part, offset, buf, len);
```

读取不需要擦除。

---

## 写入

写入前必须确保对应区域已经是擦除状态，也就是里面大部分是：

```text
0xFF
```

否则可能写失败，或者写出来的数据不是你想要的。

```c
esp_partition_erase_range(part, 0, 4096);
esp_partition_write(part, 0, data, len);
```

---

## 擦除

擦除必须按 sector 对齐。

一般这样可以：

```c
esp_partition_erase_range(part, 0, 4096);
```

这样不行：

```c
esp_partition_erase_range(part, 1, 100);
```

因为：

```text
offset 不是 4KB 对齐
size 不是 4KB 整数倍
```

---

# 8. 不建议你直接用 esp_flash 操作物理地址

底层 API 大概长这样：

```c
esp_flash_read(...)
esp_flash_write(...)
esp_flash_erase_region(...)
```

`esp_partition_read()`、`esp_partition_write()`、`esp_partition_erase_range()` 本质上就是基于更底层 `esp_flash_*` 的封装，但它们会限制你只能在指定分区范围内操作，不容易写坏其他区域。官方文档也说明 `esp_partition` 是工作在分区表上的高级 API，底层基于 SPI Flash API。([Espressif Systems][1])

直接用物理地址有风险。比如你不小心擦了：

```text
0x10000
```

可能就是你的 app 固件起始位置。

擦了：

```text
0x8000
```

可能就是分区表。

擦了：

```text
0x9000
```

可能就是 NVS 参数区。

所以初学阶段建议：

```text
不要直接操作物理地址
不要直接擦整片 Flash
不要乱写 app 分区
```

---

# 9. NVS 和 esp_partition 的区别

| 项目           | NVS          | esp_partition     |
| ------------ | ------------ | ----------------- |
| 使用难度         | 简单           | 中等                |
| 数据形式         | key-value    | 原始二进制             |
| 适合保存         | 小参数          | 大块数据              |
| 是否需要自己擦除     | 不需要          | 需要                |
| 是否需要自己设计数据格式 | 基本不需要        | 需要                |
| 是否适合高频写      | 不适合          | 也不适合              |
| 典型用途         | Wi-Fi、配置、校准值 | 日志、固件数据、大结构体、历史数据 |

简单说：

```text
保存几个参数：用 NVS
想自己管理一块 Flash：用 esp_partition
想做文件：用 LittleFS / FATFS
```

---

# 10. Flash 操作千万注意这几个坑

## 10.1 不要在中断里写 Flash

不要这样：

```c
void IRAM_ATTR gpio_isr_handler(void *arg)
{
    esp_partition_write(...);  // 错
}
```

正确做法：

```text
中断里发队列 / 置标志
任务里写 Flash
```

---

## 10.2 不要高频写 Flash

比如你做称重、IMU、电机控制，不要这样：

```c
while (1) {
    esp_partition_erase_range(...);
    esp_partition_write(...);
    vTaskDelay(pdMS_TO_TICKS(10));
}
```

这相当于疯狂擦写 Flash，很容易把 Flash 某些扇区磨坏。

正确做法：

```text
实时数据放 RAM
稳定后的结果再保存
参数变化后延迟几秒再保存
日志做缓存，攒一批再写
```

---

## 10.3 改了分区表后，建议擦整片 Flash

如果你改了 `partitions.csv`，旧数据可能还在原地址上，导致读取异常。

可以执行：

```bash
idf.py erase-flash
idf.py flash monitor
```

NVS 文档也提到，如果分区表改变导致 NVS 分区被截断，应擦除对应内容；ESP-IDF 提供 `idf.py erase-flash` 擦除整片 Flash。([Espressif Systems][3])

---

# 11. 你可以这样记

ESP32-S3 操作 Flash 的正常套路是：

```text
第一步：在 partitions.csv 里划一块区域
第二步：esp_partition_find_first() 找到这块区域
第三步：esp_partition_erase_range() 擦除
第四步：esp_partition_write() 写入
第五步：esp_partition_read() 读取
```

核心代码就这三个：

```c
esp_partition_erase_range(part, offset, erase_size);
esp_partition_write(part, offset, data, len);
esp_partition_read(part, offset, buf, len);
```

一句话总结：**ESP32-S3 真正推荐的 Flash 操作方式，不是直接写物理地址，而是先在分区表里划一块自己的 data 分区，然后通过 `esp_partition` API 在这个分区里擦、写、读。**

[1]: https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/storage/partition.html "Partitions API - ESP32-S3 -  — ESP-IDF Programming Guide v6.0.1 documentation"
[2]: https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-guides/partition-tables.html "Partition Tables - ESP32-S3 -  — ESP-IDF Programming Guide v6.0.1 documentation"
[3]: https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/storage/nvs_flash.html "Non-Volatile Storage Library - ESP32-S3 -  — ESP-IDF Programming Guide v6.0.1 documentation"

