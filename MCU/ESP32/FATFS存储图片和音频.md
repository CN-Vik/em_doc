可以。你这个需求可以按下面这条链路理解：

```text
SD 卡 / SPI Flash
        ↓
ESP-IDF FATFS 挂载到 VFS
        ↓
C 标准文件 API：fopen / fread / fwrite / fclose
        ↓
图片：LVGL 通过文件系统驱动读取 PNG
音频：应用层 fread 分块读取，送 I2S / 解码器 / codec
```

ESP-IDF 里 **FatFs 已经集成好了**，一般不叫“移植 FATFS”，而是叫“挂载 FATFS 到 VFS”。挂载成功后，就可以像 Linux 一样用 `/sdcard/xxx.wav`、`/sdcard/img/logo.png` 这种路径读写文件。ESP-IDF 官方文档也说明，FatFs 可以通过 VFS 使用 C 标准库和 POSIX API，例如 `fopen()`、`read()`、`write()` 等。([Espressif Systems][1])

---

# 1. 推荐目录结构

你要存音频和图片，我建议 SD 卡里面这样放：

```text
/sdcard
├── audio
│   ├── boot.wav
│   ├── click.wav
│   └── alarm.wav
│
├── image
│   ├── logo.png
│   ├── bg.png
│   └── icon_wifi.png
│
└── config
    └── ui.json
```

ESP32 程序里直接这样访问：

```c
fopen("/sdcard/audio/boot.wav", "rb");
fopen("/sdcard/image/logo.png", "rb");
```

LVGL 里一般这样访问：

```c
lv_img_set_src(img, "S:/image/logo.png");      // LVGL v8
lv_image_set_src(img, "S:/image/logo.png");    // LVGL v9
```

这里要注意：

```text
C 文件 API 路径： /sdcard/image/logo.png
LVGL 路径：      S:/image/logo.png
```

`S:` 是 LVGL 自己的文件系统盘符，不是 Windows 盘符。

---

# 2. FATFS 有几种用法？

ESP32 上 FATFS 主要有三种挂载方式。

## 方案 A：SD 卡 + FATFS，最推荐

适合：

```text
大量图片
大量音频
WAV / MP3
用户可替换资源
日志
升级包
```

ESP-IDF 官方 SDSPI SD 卡示例就是用 `esp_vfs_fat_sdspi_mount()` 初始化 SPI、探测 SD 卡、挂载 FATFS，并注册到 VFS，之后就可以使用 C 标准文件函数。([GitHub][2])

---

## 方案 B：SDMMC + FATFS，速度更快

适合：

```text
板子上有标准 SDMMC 走线
想要更高速度
有 1-bit / 4-bit SDIO 接法
```

但是 SDMMC 对 GPIO 和硬件连接要求更高。你如果用的是那种淘宝常见 TF 卡模块，通常是 **SDSPI**，不是 SDMMC。

---

## 方案 C：内部 SPI Flash + FATFS

适合：

```text
少量图片
少量音效
配置文件
出厂资源
不想加 SD 卡
```

内部 Flash 上用 FATFS 时，建议搭配 wear levelling。ESP-IDF 提供 `esp_vfs_fat_spiflash_mount_rw_wl()`，它会找到分区、初始化磨损均衡、挂载 FATFS，并注册到 VFS。([Espressif Systems][3])

但是你要存音频和图片，**优先 SD 卡**。内部 Flash 适合小资源，不适合大量媒体文件。

---

# 3. SD 卡 SDSPI 挂载 FATFS

假设你接线如下，GPIO 要按你自己的板子改：

```text
ESP32-S3        TF 卡模块
GPIO 11   ->    MOSI
GPIO 13   ->    MISO
GPIO 12   ->    SCLK
GPIO 10   ->    CS
3.3V      ->    VCC
GND       ->    GND
```

## CMakeLists.txt

`main/CMakeLists.txt`：

```cmake
idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES fatfs sdmmc esp_driver_sdspi esp_driver_gpio
)
```

---

## 挂载代码

```c
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#include "esp_log.h"
#include "esp_err.h"
#include "esp_vfs_fat.h"

#include "driver/gpio.h"
#include "driver/sdspi_host.h"
#include "sdmmc_cmd.h"

#define PIN_NUM_MISO  GPIO_NUM_13
#define PIN_NUM_MOSI  GPIO_NUM_11
#define PIN_NUM_CLK   GPIO_NUM_12
#define PIN_NUM_CS    GPIO_NUM_10

#define MOUNT_POINT   "/sdcard"

static const char *TAG = "sdcard";

static sdmmc_card_t *s_card = NULL;

esp_err_t sdcard_fatfs_mount(void)
{
    esp_err_t ret;

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,     // 生产环境建议 false，避免误格式化 SD 卡
        .max_files = 8,                      // 同时最多打开几个文件
        .allocation_unit_size = 16 * 1024,   // 簇大小，音频/图片这种大文件可以稍微大点
    };

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = PIN_NUM_MISO,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = GPIO_NUM_NC,
        .quadhd_io_num = GPIO_NUM_NC,
        .max_transfer_sz = 16 * 1024,
    };

    ret = spi_bus_initialize(host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "spi_bus_initialize failed: %s", esp_err_to_name(ret));
        return ret;
    }

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = PIN_NUM_CS;
    slot_config.host_id = host.slot;

    ret = esp_vfs_fat_sdspi_mount(
        MOUNT_POINT,
        &host,
        &slot_config,
        &mount_config,
        &s_card
    );

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_vfs_fat_sdspi_mount failed: %s", esp_err_to_name(ret));

        spi_bus_free(host.slot);
        return ret;
    }

    ESP_LOGI(TAG, "SD card mounted");
    sdmmc_card_print_info(stdout, s_card);

    return ESP_OK;
}

void sdcard_fatfs_unmount(void)
{
    if (s_card) {
        esp_vfs_fat_sdcard_unmount(MOUNT_POINT, s_card);
        s_card = NULL;
        ESP_LOGI(TAG, "SD card unmounted");
    }
}
```

---

# 4. 测试 FATFS 读写文件

挂载成功后，直接用 `fopen()`：

```c
void fatfs_write_test(void)
{
    FILE *fp = fopen("/sdcard/test.txt", "w");
    if (fp == NULL) {
        ESP_LOGE("fatfs", "open file for write failed");
        return;
    }

    fprintf(fp, "hello fatfs from esp32\n");
    fclose(fp);

    ESP_LOGI("fatfs", "write done");
}

void fatfs_read_test(void)
{
    FILE *fp = fopen("/sdcard/test.txt", "r");
    if (fp == NULL) {
        ESP_LOGE("fatfs", "open file for read failed");
        return;
    }

    char line[128];

    while (fgets(line, sizeof(line), fp)) {
        ESP_LOGI("fatfs", "read: %s", line);
    }

    fclose(fp);
}
```

`app_main()`：

```c
void app_main(void)
{
    if (sdcard_fatfs_mount() == ESP_OK) {
        fatfs_write_test();
        fatfs_read_test();
    }
}
```

---

# 5. 内部 Flash 使用 FATFS

如果你不用 SD 卡，也可以在内部 Flash 建一个 FATFS 分区。

## partitions.csv 示例

```csv
# Name,    Type, SubType, Offset,   Size,   Flags
nvs,       data, nvs,     0x9000,   0x4000,
phy_init,  data, phy,     0xf000,   0x1000,
factory,   app,  factory, 0x10000,  2M,
storage,   data, fat,              1M,
```

`storage` 就是 FATFS 分区名。

## 挂载内部 Flash FATFS

```c
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "wear_levelling.h"

static const char *TAG = "fatfs_flash";
static wl_handle_t s_wl_handle = WL_INVALID_HANDLE;

esp_err_t spiflash_fatfs_mount(void)
{
    esp_vfs_fat_mount_config_t mount_config = {
        .format_if_mount_failed = true,
        .max_files = 4,
        .allocation_unit_size = CONFIG_WL_SECTOR_SIZE,
    };

    esp_err_t ret = esp_vfs_fat_spiflash_mount_rw_wl(
        "/spiflash",
        "storage",
        &mount_config,
        &s_wl_handle
    );

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "mount failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "SPI Flash FATFS mounted");
    return ESP_OK;
}

void spiflash_fatfs_unmount(void)
{
    if (s_wl_handle != WL_INVALID_HANDLE) {
        esp_vfs_fat_spiflash_unmount_rw_wl("/spiflash", s_wl_handle);
        s_wl_handle = WL_INVALID_HANDLE;
    }
}
```

然后就可以：

```c
fopen("/spiflash/config.txt", "w");
fopen("/spiflash/logo.png", "rb");
```

内部 Flash FATFS 可以用，但你存音频和大图片的话，还是 SD 卡更合适。

---

# 6. LVGL 显示 PNG 图片

这里分两步：

```text
第一步：ESP-IDF 挂载 SD 卡到 /sdcard
第二步：LVGL 注册文件系统驱动，把 S: 映射到 /sdcard
第三步：LVGL 启用 PNG 解码器
第四步：lv_img_set_src("S:/image/logo.png")
```

LVGL 文档说明，LVGL 有自己的文件系统抽象层，一个文件系统通过盘符识别，例如 SD 卡关联到 `'S'`，文件就可以通过 `"S:path/to/file.txt"` 访问。([LVGL][4])
LVGL 的 PNG 解码器启用后，可以直接把 PNG 文件作为图片源，但是从文件打开 PNG 时必须先注册文件系统驱动。PNG 解码时会整张图展开，RAM 需求约等于 `宽 x 高 x 4` 字节。([LVGL][5])

---

## LVGL v8 配置

如果你用 LVGL 8.x，`lv_conf.h` 里打开：

```c
#define LV_USE_PNG 1

#define LV_USE_FS_STDIO 1
#define LV_FS_STDIO_LETTER 'S'
#define LV_FS_STDIO_PATH "/sdcard"
#define LV_FS_STDIO_CACHE_SIZE 4096
```

这样 LVGL 的：

```text
S:/image/logo.png
```

会被映射到 ESP-IDF 的：

```text
/sdcard/image/logo.png
```

显示 PNG：

```c
#include "lvgl.h"

void ui_show_png_v8(void)
{
    lv_obj_t *img = lv_img_create(lv_scr_act());

    /*
     * 注意：
     * 这里不是 /sdcard/image/logo.png
     * 而是 LVGL 的 S: 盘符路径
     */
    lv_img_set_src(img, "S:/image/logo.png");

    lv_obj_center(img);
}
```

---

## LVGL v9 配置

如果你用 LVGL 9.x，PNG 推荐启用 LodePNG：

```c
#define LV_USE_LODEPNG 1

#define LV_USE_FS_STDIO 1
#define LV_FS_STDIO_DRIVER_LETTER 'S'
#define LV_FS_STDIO_PATH "/sdcard"
#define LV_FS_STDIO_CACHE_SIZE 4096
```

LVGL 9 的 LodePNG 文档说明，`LV_USE_LODEPNG` 置 1 后，PNG 可以直接作为 image source 使用；但从文件打开 PNG 时，仍然需要文件系统驱动。([LVGL 文档][6])

显示 PNG：

```c
#include "lvgl.h"

void ui_show_png_v9(void)
{
    lv_obj_t *img = lv_image_create(lv_screen_active());

    lv_image_set_src(img, "S:/image/logo.png");

    lv_obj_center(img);
}
```

---

## PNG 显示最容易踩的坑

### 坑 1：路径写错

C 语言文件测试用：

```c
fopen("/sdcard/image/logo.png", "rb");
```

LVGL 显示用：

```c
lv_img_set_src(img, "S:/image/logo.png");
```

不要写成：

```c
lv_img_set_src(img, "/sdcard/image/logo.png");
```

除非你自己写了 `/` 作为 LVGL 默认驱动盘符。

---

### 坑 2：SD 卡没有先挂载

必须先执行：

```c
sdcard_fatfs_mount();
```

再执行：

```c
lv_img_set_src(img, "S:/image/logo.png");
```

否则 LVGL 找不到文件。

---

### 坑 3：PNG 太大，RAM 不够

比如 480x800 PNG：

```text
480 x 800 x 4 = 1,536,000 字节 ≈ 1.5MB
```

所以如果没有 PSRAM，直接显示大 PNG 很容易失败。

我的建议：

```text
小图标：PNG 可以
大背景：不要直接 PNG，建议转 RGB565 bin
大量 UI 资源：建议用 LVGL 图片转换工具转 bin
```

---

# 7. 播放音频：先从 WAV PCM 开始

音频这块建议你先别一上来搞 MP3。最简单稳定的是：

```text
WAV PCM 16bit stereo
        ↓
fread 分块读取
        ↓
i2s_channel_write()
        ↓
I2S DAC / ES8388 / PCM5102 / 功放
```

ESP-IDF 的 I2S 文档说明，I2S 是数字音频设备之间传输音频数据的同步串行协议；标准 I2S 总线通常包括 MCLK、BCLK、WS、DIN/DOUT 等信号，ESP32 的 I2S 外设可以通过 DMA 传输采样数据。([Espressif Systems][7])
在 ESP-IDF 新 I2S 驱动里，发送数据通常调用 `i2s_channel_write()`，这个函数会把源 buffer 的数据拷贝到 DMA TX buffer 并等待发送。([Espressif Systems][8])

---

## I2S 初始化示例

下面示例按 **16bit stereo** 写。GPIO 你要按自己的原理图改。

```c
#include "esp_log.h"
#include "driver/i2s_std.h"

static const char *TAG = "audio";

#define I2S_BCLK_IO    GPIO_NUM_4
#define I2S_WS_IO      GPIO_NUM_5
#define I2S_DOUT_IO    GPIO_NUM_6
#define I2S_MCLK_IO    GPIO_NUM_NC

static i2s_chan_handle_t s_i2s_tx_chan = NULL;

esp_err_t audio_i2s_init(uint32_t sample_rate)
{
    if (s_i2s_tx_chan != NULL) {
        return ESP_OK;
    }

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(
        I2S_NUM_AUTO,
        I2S_ROLE_MASTER
    );

    esp_err_t ret = i2s_new_channel(&chan_cfg, &s_i2s_tx_chan, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2s_new_channel failed: %s", esp_err_to_name(ret));
        return ret;
    }

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(sample_rate),

        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
            I2S_DATA_BIT_WIDTH_16BIT,
            I2S_SLOT_MODE_STEREO
        ),

        .gpio_cfg = {
            .mclk = I2S_MCLK_IO,
            .bclk = I2S_BCLK_IO,
            .ws   = I2S_WS_IO,
            .dout = I2S_DOUT_IO,
            .din  = GPIO_NUM_NC,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };

    ret = i2s_channel_init_std_mode(s_i2s_tx_chan, &std_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2s_channel_init_std_mode failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = i2s_channel_enable(s_i2s_tx_chan);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2s_channel_enable failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "I2S init ok, sample_rate=%lu", sample_rate);
    return ESP_OK;
}
```

如果你用的是 **ES8388** 这种 codec，还要额外通过 I2C 初始化 ES8388 的寄存器，例如音量、DAC 开关、I2S 数据格式、采样率相关配置。I2S 只负责送数字音频数据，不负责 codec 芯片内部配置。

---

## 简单 WAV 头结构

这个是简化版，只适合标准 PCM WAV：

```c
#include <stdint.h>

typedef struct __attribute__((packed)) {
    char riff_id[4];          // "RIFF"
    uint32_t riff_size;
    char wave_id[4];          // "WAVE"

    char fmt_id[4];           // "fmt "
    uint32_t fmt_size;
    uint16_t audio_format;    // 1 = PCM
    uint16_t num_channels;    // 1 mono, 2 stereo
    uint32_t sample_rate;     // 44100, 16000 ...
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample; // 16

    char data_id[4];          // "data"
    uint32_t data_size;
} wav_header_t;
```

注意：有些 WAV 文件 `fmt` 后面还有额外字段，或者中间有 `LIST` chunk。上面这个结构是入门简化版。如果你播放某些 WAV 失败，要写更完整的 chunk 扫描解析。

---

## 播放 WAV

```c
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "driver/i2s_std.h"

#define AUDIO_BUF_SIZE 4096

static const char *AUDIO_TAG = "wav";

esp_err_t play_wav_from_sdcard(const char *path)
{
    FILE *fp = fopen(path, "rb");
    if (fp == NULL) {
        ESP_LOGE(AUDIO_TAG, "open wav failed: %s", path);
        return ESP_FAIL;
    }

    wav_header_t header;

    size_t n = fread(&header, 1, sizeof(header), fp);
    if (n != sizeof(header)) {
        ESP_LOGE(AUDIO_TAG, "read wav header failed");
        fclose(fp);
        return ESP_FAIL;
    }

    if (memcmp(header.riff_id, "RIFF", 4) != 0 ||
        memcmp(header.wave_id, "WAVE", 4) != 0 ||
        memcmp(header.fmt_id, "fmt ", 4) != 0 ||
        memcmp(header.data_id, "data", 4) != 0) {
        ESP_LOGE(AUDIO_TAG, "not a simple PCM WAV file");
        fclose(fp);
        return ESP_FAIL;
    }

    if (header.audio_format != 1) {
        ESP_LOGE(AUDIO_TAG, "only PCM WAV supported, format=%u", header.audio_format);
        fclose(fp);
        return ESP_FAIL;
    }

    if (header.bits_per_sample != 16) {
        ESP_LOGE(AUDIO_TAG, "only 16bit WAV supported, bits=%u", header.bits_per_sample);
        fclose(fp);
        return ESP_FAIL;
    }

    if (header.num_channels != 2) {
        ESP_LOGW(AUDIO_TAG, "this demo expects stereo, channels=%u", header.num_channels);
        /*
         * 如果是 mono，需要把单声道复制成左右声道，
         * 或者把 I2S slot 配成 mono。
         */
    }

    ESP_LOGI(AUDIO_TAG, "WAV info:");
    ESP_LOGI(AUDIO_TAG, "sample_rate=%lu", header.sample_rate);
    ESP_LOGI(AUDIO_TAG, "channels=%u", header.num_channels);
    ESP_LOGI(AUDIO_TAG, "bits=%u", header.bits_per_sample);
    ESP_LOGI(AUDIO_TAG, "data_size=%lu", header.data_size);

    esp_err_t ret = audio_i2s_init(header.sample_rate);
    if (ret != ESP_OK) {
        fclose(fp);
        return ret;
    }

    uint8_t buf[AUDIO_BUF_SIZE];
    uint32_t remain = header.data_size;

    while (remain > 0) {
        size_t to_read = remain > AUDIO_BUF_SIZE ? AUDIO_BUF_SIZE : remain;

        size_t read_len = fread(buf, 1, to_read, fp);
        if (read_len == 0) {
            break;
        }

        size_t bytes_written = 0;

        ret = i2s_channel_write(
            s_i2s_tx_chan,
            buf,
            read_len,
            &bytes_written,
            portMAX_DELAY
        );

        if (ret != ESP_OK) {
            ESP_LOGE(AUDIO_TAG, "i2s write failed: %s", esp_err_to_name(ret));
            break;
        }

        remain -= read_len;
    }

    fclose(fp);

    ESP_LOGI(AUDIO_TAG, "play wav done");

    return ESP_OK;
}
```

调用：

```c
void app_main(void)
{
    if (sdcard_fatfs_mount() != ESP_OK) {
        return;
    }

    play_wav_from_sdcard("/sdcard/audio/boot.wav");
}
```

---

# 8. MP3 怎么播放？

MP3 不能像 WAV PCM 那样直接送 I2S。MP3 文件是压缩数据，需要：

```text
fread 读取 MP3 文件
        ↓
MP3 解码器
        ↓
得到 PCM 数据
        ↓
i2s_channel_write()
        ↓
喇叭
```

常见路线：

```text
简单项目：
    minimp3 / helix mp3 decoder

复杂音频项目：
    ESP-ADF audio pipeline

只播放提示音：
    直接用 WAV PCM，最省事
```

如果你只是做 UI 提示音、按键音、报警音，我强烈建议先用：

```text
16bit PCM WAV
采样率 16000 / 22050 / 44100
双声道或单声道固定一种格式
```

这样不用引入 MP3 解码库，调试难度低很多。

---

# 9. 图片资源建议

虽然你问的是 PNG，但在 MCU 上我建议这样选：

```text
小图标：
    PNG 可以

透明图标：
    PNG 可以

大背景图：
    不建议 PNG，建议 RGB565 bin

开机 logo：
    可以 PNG，也可以转成 LVGL bin

大量 UI 图片：
    建议提前转成 LVGL 图片格式或 RGB565 bin
```

原因是 PNG 显示时要解码，RAM 和 CPU 都会增加。LVGL 文档明确说 PNG 解码会整张图展开，RAM 约等于 `width x height x 4` 字节。([LVGL][5])

例如：

```text
100 x 100 PNG：
    100 x 100 x 4 = 40KB

240 x 240 PNG：
    240 x 240 x 4 = 230KB

480 x 800 PNG：
    480 x 800 x 4 = 1.5MB
```

ESP32-S3 有 PSRAM 还能扛一下；没有 PSRAM 就很危险。

---

# 10. 你的最终推荐方案

你这个场景我建议这样搭：

```text
ESP32-S3
├── 内部 Flash
│   ├── app
│   ├── nvs
│   └── littlefs / fatfs
│       ├── config.json
│       └── small_logo.bin
│
├── PSRAM
│   ├── LVGL draw buffer
│   ├── PNG decode buffer
│   └── audio buffer
│
└── SD 卡 FATFS
    ├── image
    │   ├── logo.png
    │   ├── icon_wifi.png
    │   └── bg_480x800.bin
    │
    └── audio
        ├── boot.wav
        ├── click.wav
        └── alarm.wav
```

工程初始化顺序建议：

```c
void app_main(void)
{
    // 1. 初始化 NVS
    // 2. 初始化 SD 卡 FATFS，挂载 /sdcard
    // 3. 初始化 LCD
    // 4. 初始化 LVGL
    // 5. 初始化 LVGL 文件系统驱动
    // 6. 初始化 I2S / codec
    // 7. 显示 PNG / 播放 WAV
}
```

一句话总结：

```text
FATFS 负责存储和读文件；
LVGL 负责通过 S: 盘符读取 PNG 并显示；
音频播放负责 fread 分块读取 WAV，再送 I2S。
```

对你这种“图片 + 音频”的 ESP32 项目，**SD 卡 + FATFS 是主存储，内部 Flash 只放小资源和配置**，这是最稳的方案。

[1]: https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/storage/fatfs.html "FAT Filesystem Support - ESP32 -  — ESP-IDF Programming Guide v6.0.1 documentation"
[2]: https://github.com/espressif/esp-idf/blob/master/examples/storage/sd_card/sdspi/README.md "esp-idf/examples/storage/sd_card/sdspi/README.md at master · espressif/esp-idf · GitHub"
[3]: https://docs.espressif.com/projects/esp-idf/en/v5.2/esp32s3/api-reference/storage/fatfs.html "FAT Filesystem Support - ESP32-S3 -  — ESP-IDF Programming Guide v5.2 documentation"
[4]: https://lvgl.io/docs/open/8.3/overview/file-system?utm_source=chatgpt.com "File system — LVGL documentation"
[5]: https://lvgl.io/docs/open/8.3/libs/png "PNG decoder — LVGL  documentation"
[6]: https://docs.lvgl.io/9.5/libs/image_support/lodepng.html?utm_source=chatgpt.com "LodePNG Decoder - LVGL 9.5 documentation"
[7]: https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/i2s.html "Inter-IC Sound (I2S) - ESP32 -  — ESP-IDF Programming Guide v6.0.1 documentation"
[8]: https://docs.espressif.com/projects/esp-idf/en/v5.0/esp32/api-reference/peripherals/i2s.html?utm_source=chatgpt.com "Inter-IC Sound (I2S) - ESP32 - — ESP-IDF Programming ..."
