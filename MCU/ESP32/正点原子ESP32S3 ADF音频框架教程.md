可以搞，而且你这块板子**比普通 ESP32-S3 开发板更适合跑 ADF**，因为原理图里已经有 **ES8388 音频 Codec + MD8002A 功放 + 麦克风 + 耳机/喇叭输出**。你不是乐鑫官方 LyraT/Korvo 板，所以重点不是“会不会用 ADF”，而是**把 ADF 的板级引脚适配成正点原子的引脚**。

下面按“先跑通播放一段 MP3”的路线来，不要一上来搞 HTTP、SD 卡、语音识别。

---

## 1. 先确认你这块板子的音频硬件

从你上传的原理图看，音频链路是：

```text
ESP32-S3
  │
  ├── I2C 控制 ES8388 寄存器
  │
  ├── I2S 传输音频 PCM 数据
  │
ES8388 音频 Codec
  │
  ├── PHONE 耳机输出
  │
  └── SPK_IN → MD8002A 功放 → SPEAKER 喇叭
```

原理图第 3 页的 `I2S DAC&ADC` 模块里能看到 ES8388，旁边还有 MD8002A 喇叭功放；同一页左上角 `IO EXPANDER` 里有 XL9555，`SPK_EN` 接在 XL9555 的 `IO0_2` 上，所以**喇叭功放不是 ESP32-S3 的普通 GPIO 直接控制，而是 I2C 扩展 IO 控制**。

关键引脚表如下：

| 功能           |    原理图网络名 | ESP32-S3 GPIO | 方向                   |
| ------------ | --------: | ------------: | -------------------- |
| I2C SDA      |   IIC_SDA |        GPIO41 | ESP 控制 ES8388/XL9555 |
| I2C SCL      |   IIC_SCL |        GPIO42 | ESP 控制 ES8388/XL9555 |
| I2S MCLK     |  I2S_MCLK |         GPIO3 | ESP → ES8388         |
| I2S BCLK/SCK |   I2S_SCK |        GPIO46 | ESP → ES8388         |
| I2S LRCK/WS  |  I2S_LRCK |         GPIO9 | ESP → ES8388         |
| I2S 播放数据     |  I2S_SDIN |        GPIO10 | ESP → ES8388 DSDIN   |
| I2S 录音数据     | I2S_SDOUT |        GPIO14 | ES8388 ASDOUT → ESP  |
| 喇叭使能         |    SPK_EN |  XL9555 IO0_2 | I2C 扩展 IO            |

注意 GPIO3、GPIO46 是 ESP32-S3 的 strapping pin，乐鑫文档也说明 GPIO0/GPIO3/GPIO45/GPIO46 是启动配置相关引脚；板子既然已经这样设计，软件可以用，但不要外接乱拉高/拉低影响启动。([Espressif Systems][1])

---

## 2. 环境选择：建议先用 Linux/WSL/Ubuntu 命令行

ADF 官方文档强调，ESP-ADF 依赖特定 ESP-IDF 版本，Python 版本要求在 3.7 到 3.11 之间；不要随便拿你电脑里已经装好的某个 IDF 版本混着编，否则很容易出现莫名其妙的编译错误。([Espressif Systems][2])

先装 ADF：

```bash
mkdir -p ~/esp
cd ~/esp

# 国内网络可以优先用 gitee
git clone --recursive -b release/v2.x https://gitee.com/EspressifSystems/esp-adf.git

cd esp-adf
./install.sh
. ./export.sh

echo $ADF_PATH
idf.py --version
python --version
```

如果你新开一个终端后 `idf.py: command not found`，重新执行：

```bash
. ~/esp/esp-adf/export.sh
```

注意最前面是一个点 `.` 加空格，ADF 文档也专门强调了这个写法。([Espressif Systems][2])

---

## 3. 复制 ADF 官方 MP3 播放例程

ADF 官方入门文档就是用 `play_mp3_control` 作为第一个播放例程，它的流程是：

```text
内置 MP3 数据
  → MP3 解码器
  → I2S Stream
  → Codec 芯片
  → 耳机/喇叭出声
```

官方文档里的运行日志也能看到 `[mp3_music_read_cb]-->mp3_decoder-->i2s_stream-->[codec_chip]` 这个链路。([Espressif Systems][2])

执行：

```bash
cd ~/esp
cp -r $ADF_PATH/examples/get-started/play_mp3_control .
cd ~/esp/play_mp3_control

idf.py set-target esp32s3
idf.py menuconfig
```

这里一定是：

```bash
idf.py set-target esp32s3
```

不要照官方老示例写 `esp32`，因为你是 ESP32-S3 板子。ESP-ADF 当前官方仓库也明确支持 ESP32-S3。([GitHub][3])

---

## 4. 重点：适配正点原子板级引脚

ADF 的例程默认是给乐鑫官方音频板用的。你的板子不是官方板，但有 ES8388，所以建议第一阶段先拿 **LyraT V4.3 的 ES8388 板级文件**改。ADF 官方 ES8388 驱动本来就是给 LyraT 这类 ES8388 音频板用的。([Espressif Systems][4])

### 方案 A：最快跑通法，直接改 ADF 里的 LyraT V4.3 板级文件

先备份：

```bash
cd $ADF_PATH/components/audio_board/lyrat_v4_3

cp board_pins_config.c board_pins_config.c.bak
cp board_def.h board_def.h.bak
```

打开：

```bash
nano board_pins_config.c
```

找到 `get_i2c_pins()`，改成：

```c
esp_err_t get_i2c_pins(i2c_port_t port, i2c_config_t *i2c_config)
{
    AUDIO_NULL_CHECK(TAG, i2c_config, return ESP_FAIL);

    if (port == I2C_NUM_0 || port == I2C_NUM_1) {
        // 正点原子 ESP32-S3 板：ES8388 / XL9555 / EEPROM 共用 I2C
        i2c_config->sda_io_num = GPIO_NUM_41;
        i2c_config->scl_io_num = GPIO_NUM_42;
    } else {
        i2c_config->sda_io_num = -1;
        i2c_config->scl_io_num = -1;
        ESP_LOGE(TAG, "i2c port %d is not supported", port);
        return ESP_FAIL;
    }

    return ESP_OK;
}
```

找到 `get_i2s_pins()`，改成：

```c
esp_err_t get_i2s_pins(int port, board_i2s_pin_t *i2s_config)
{
    AUDIO_NULL_CHECK(TAG, i2s_config, return ESP_FAIL);

    if (port == 0 || port == 1) {
        // MCLK：主时钟
        i2s_config->mck_io_num = GPIO_NUM_3;

        // BCLK / SCK：位时钟
        i2s_config->bck_io_num = GPIO_NUM_46;

        // LRCK / WS：左右声道时钟
        i2s_config->ws_io_num = GPIO_NUM_9;

        // ESP32-S3 输出到 ES8388 DSDIN，用于播放
        i2s_config->data_out_num = GPIO_NUM_10;

        // ES8388 ASDOUT 输出到 ESP32-S3，用于录音
        i2s_config->data_in_num = GPIO_NUM_14;
    } else {
        memset(i2s_config, -1, sizeof(board_i2s_pin_t));
        ESP_LOGE(TAG, "i2s port %d is not supported", port);
        return ESP_FAIL;
    }

    return ESP_OK;
}
```

再打开：

```bash
nano board_def.h
```

建议先把功放 GPIO 改成 `-1`，因为你的 `SPK_EN` 不是直接 GPIO，而是在 XL9555 上：

```c
#define PA_ENABLE_GPIO -1
```

ES8388 默认配置建议保持这种思路：

```c
#define AUDIO_CODEC_DEFAULT_CONFIG(){                       \
    .adc_input = AUDIO_HAL_ADC_INPUT_LINE1,                 \
    .dac_output = AUDIO_HAL_DAC_OUTPUT_ALL,                 \
    .codec_mode = AUDIO_HAL_CODEC_MODE_BOTH,                \
    .i2s_iface = {                                          \
        .mode = AUDIO_HAL_MODE_SLAVE,                       \
        .fmt = AUDIO_HAL_I2S_NORMAL,                        \
        .samples = AUDIO_HAL_48K_SAMPLES,                   \
        .bits = AUDIO_HAL_BIT_LENGTH_16BITS,                \
    },                                                      \
};
```

`AUDIO_HAL_MODE_SLAVE` 这里的意思是 **ES8388 作为 I2S 从机，ESP32-S3 产生 I2S 时钟**。ADF 的 ES8388 驱动会通过 I2C 初始化 ES8388，然后通过 I2S 播放数据；ES8388 驱动里也提供初始化、I2S 格式配置、启动、音量设置等 API。([Espressif Systems][4])

---

## 5. menuconfig 配置

进入你的例程目录：

```bash
cd ~/esp/play_mp3_control
idf.py menuconfig
```

重点看这些：

```text
Audio HAL
  → Audio board
      → ESP32-LyraT V4.3
```

因为我们第一阶段是“借 LyraT V4.3 的 ES8388 板级框架，只把引脚改成正点原子的”。

再看：

```text
Serial flasher config
  → Flash size
```

不确定就先用默认或者 `detect`，先不要乱改。

保存退出：

```text
S 保存
Q 退出
```

---

## 6. 编译、烧录、监视

Linux 下先看串口：

```bash
ls /dev/ttyUSB*
ls /dev/ttyACM*
```

如果是 CH340，一般是：

```bash
/dev/ttyUSB0
```

烧录：

```bash
idf.py -p /dev/ttyUSB0 build flash monitor
```

Windows 下可能是：

```bash
idf.py -p COM5 build flash monitor
```

ADF 官方文档也说明了烧录命令形式是 `idf.py -p PORT flash monitor`，如果卡在 `Connecting...`，按住 BOOT，点一下 RESET，再松开 BOOT。([Espressif Systems][2])

正常你应该看到类似：

```text
I (...) PLAY_FLASH_MP3_CONTROL: [ 1 ] Start audio codec chip
I (...) PLAY_FLASH_MP3_CONTROL: [2.1] Create mp3 decoder
I (...) PLAY_FLASH_MP3_CONTROL: [2.2] Create i2s stream
I (...) PLAY_FLASH_MP3_CONTROL: [ * ] Receive music info from mp3 decoder
```

先插耳机测试。耳机能响，说明：

```text
ADF 环境 OK
ES8388 I2C OK
I2S 引脚 OK
MP3 解码 OK
```

---

## 7. 如果耳机有声，喇叭没声：打开 XL9555 的 SPK_EN

这是你这块板子的特殊点。原理图里 `SPK_EN` 接在 XL9555 的 `IO0_2`，不是直接接 ESP32-S3 GPIO。

可以在 `play_mp3_control_example.c` 里加一个喇叭使能函数。

头文件加：

```c
#include "driver/i2c.h"
```

加这段代码：

```c
#define XL9555_ADDR        0x20    // 7-bit I2C 地址，A0/A1/A2 接地时通常是 0x20
#define XL9555_REG_OUT0    0x02
#define XL9555_REG_CFG0    0x06
#define XL9555_SPK_EN_BIT  BIT(2)  // IO0_2 = SPK_EN

static esp_err_t xl9555_read_reg(uint8_t reg, uint8_t *data)
{
    return i2c_master_write_read_device(
        I2C_NUM_0,
        XL9555_ADDR,
        &reg,
        1,
        data,
        1,
        pdMS_TO_TICKS(100)
    );
}

static esp_err_t xl9555_write_reg(uint8_t reg, uint8_t data)
{
    uint8_t buf[2] = {reg, data};

    return i2c_master_write_to_device(
        I2C_NUM_0,
        XL9555_ADDR,
        buf,
        sizeof(buf),
        pdMS_TO_TICKS(100)
    );
}

static esp_err_t atk_speaker_enable(bool enable)
{
    uint8_t cfg0 = 0;
    uint8_t out0 = 0;

    // 1. 读配置寄存器，把 IO0_2 配成输出
    ESP_ERROR_CHECK(xl9555_read_reg(XL9555_REG_CFG0, &cfg0));
    cfg0 &= ~XL9555_SPK_EN_BIT;
    ESP_ERROR_CHECK(xl9555_write_reg(XL9555_REG_CFG0, cfg0));

    // 2. 读输出寄存器，控制 IO0_2 高低电平
    ESP_ERROR_CHECK(xl9555_read_reg(XL9555_REG_OUT0, &out0));

    if (enable) {
        out0 |= XL9555_SPK_EN_BIT;    // 拉高 SPK_EN，打开功放
    } else {
        out0 &= ~XL9555_SPK_EN_BIT;   // 拉低 SPK_EN，关闭功放
    }

    ESP_ERROR_CHECK(xl9555_write_reg(XL9555_REG_OUT0, out0));

    return ESP_OK;
}
```

然后在 `app_main()` 里：

```c
ESP_LOGI(TAG, "[ 1 ] Start audio codec chip");

audio_board_handle_t board_handle = audio_board_init();

// audio_board_init() 内部已经初始化了 I2C，
// 所以这里再去访问 XL9555，打开喇叭功放。
atk_speaker_enable(true);

audio_hal_ctrl_codec(board_handle->audio_hal,
                     AUDIO_HAL_CODEC_MODE_DECODE,
                     AUDIO_HAL_CTRL_START);
```

如果喇叭声音特别大，先把音量调小：

```c
audio_hal_set_volume(board_handle->audio_hal, 30);
```

---

## 8. 常见问题排查

### 现象 1：`idf.py: command not found`

新终端没有加载 ADF 环境，执行：

```bash
. ~/esp/esp-adf/export.sh
```

### 现象 2：编译报 IDF/ADF 版本不兼容

不要混用你之前装的 ESP-IDF。ADF 官方明确说 ESP-ADF 只支持特定 ESP-IDF 版本，版本不匹配可能不能编译。([Espressif Systems][2])

重新开干净终端：

```bash
cd ~/esp/esp-adf
. ./export.sh
cd ~/esp/play_mp3_control
idf.py fullclean
idf.py set-target esp32s3
idf.py build
```

### 现象 3：I2C 报错，ES8388 初始化失败

重点查这两个：

```c
i2c_config->sda_io_num = GPIO_NUM_41;
i2c_config->scl_io_num = GPIO_NUM_42;
```

如果 I2C 对了，常见 7-bit 扫描地址应该能看到：

```text
0x10  ES8388
0x20  XL9555
0x50  24C02 EEPROM
```

ES8388 官方 ADF 宏里用的是 8-bit 写法：`0x20: CE=0`、`0x22: CE=1`，而普通 I2C 扫描器一般显示 7-bit 地址，所以你看到 `0x10` 是正常的。([Espressif Systems][4])

### 现象 4：耳机没声、喇叭也没声

按顺序查：

```text
1. menuconfig 里是否选了 LyraT V4.3
2. board_pins_config.c 是否改成 GPIO3/46/9/10/14
3. I2C 是否是 GPIO41/GPIO42
4. 串口有没有 ES8388 I2C write error
5. 示波器看 GPIO9 是否有 LRCK，GPIO46 是否有 BCLK，GPIO10 是否有数据
```

### 现象 5：耳机有声，喇叭没声

基本就是 `SPK_EN` 没开。你的板子要通过 XL9555 的 `IO0_2` 打开 MD8002A 功放。

---

## 9. 后面正规做法

上面是“先跑通”的做法。后面项目正规化时，不建议长期改 `$ADF_PATH/components/audio_board/lyrat_v4_3`，应该新建自己的板级目录，例如：

```text
play_mp3_control/
  components/
    atk_dnesp32s3_audio_board/
      board.c
      board.h
      board_def.h
      board_pins_config.c
      CMakeLists.txt
```

然后 menuconfig 选 `Custom audio board`。第一阶段别急着搞这个，先把 **ES8388 + I2S + 耳机/喇叭播放** 跑通。跑通以后，再把板级适配独立出来，这样后面做小智 AI 语音、录音、唤醒词、WebSocket 音频流都会顺很多。

[1]: https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/peripherals/gpio.html "GPIO & RTC GPIO - ESP32-S3 -  — ESP-IDF Programming Guide v6.0.1 documentation"
[2]: https://docs.espressif.com/projects/esp-adf/en/latest/get-started/ "Get Started -  -  — Espressif Audio Development Framework Guide latest documentation"
[3]: https://github.com/espressif/esp-adf "GitHub - espressif/esp-adf: Espressif Advanced Development Framework for Multimedia Applications · GitHub"
[4]: https://docs.espressif.com/projects/esp-adf/en/latest/api-reference/abstraction/es8388.html "ES8388 Driver -  -  — Espressif Audio Development Framework Guide latest documentation"
