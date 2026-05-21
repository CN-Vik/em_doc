可以参考，而且**ESP32 播放音乐，ESP-ADF 正是官方比较推荐的音频框架之一**。

**ADF = ESP Audio Development Framework / Espressif Advanced Development Framework**，可以理解成：
在 ESP-IDF 基础上，乐鑫额外封装的一套“音频应用框架”。它里面已经帮你做好了很多音频常用模块，比如：

* MP3 / AAC / WAV / FLAC / OPUS 等解码
* HTTP 网络音乐播放
* SD 卡音乐播放
* SPIFFS 文件播放
* 蓝牙音箱 / A2DP
* I2S 输出
* 音频 codec 驱动抽象
* 播放器、录音机、网络收音机等应用框架

乐鑫官方 ESP-IDF 文档也说明，ESP-ADF 是面向音频应用的综合框架，包含 codec HAL、音乐播放器/录音机、音频处理、蓝牙音箱、网络收音机、免提设备、语音识别等功能。([Espressif Systems][1])

---

## 你要播放音乐，ADF 很适合参考

ADF 的核心思想叫 **Audio Pipeline，音频流水线**。

比如播放一个网络 MP3，大概就是：

```text
HTTP 读取音乐数据
        ↓
MP3 解码器
        ↓
I2S 输出
        ↓
外部功放 / 音频 Codec / 喇叭
```

官方文档里也明确举了这个例子：`HTTP reader stream -> MP3 decoder -> I2S writer stream`，并对应 `player/pipeline_http_mp3` 示例。([Espressif Systems][2])

所以你可以把 ADF 理解成：

```text
数据来源        解码器        输出设备
HTTP     ->    MP3     ->    I2S
SD卡     ->    WAV     ->    I2S
SPIFFS   ->    AAC     ->    I2S
蓝牙     ->    SBC     ->    I2S
```

---

## 但是要看你播放什么音乐

### 1. 只播放简单提示音 / 短 WAV

比如：

```text
开机音
按键音
报警音
短提示音
```

这种不一定非要上 ADF。你可以直接用 **ESP-IDF + I2S** 播放 PCM/WAV 数据，代码更轻，工程更简单。

适合方案：

```text
WAV / PCM 数据 -> i2s_channel_write() -> I2S功放
```

例如外接：

```text
ESP32 I2S -> MAX98357A -> 喇叭
```

这种最简单。

---

### 2. 播放 MP3 / 网络音乐 / SD 卡音乐

这种就很适合 ADF。

比如你想做：

```text
ESP32 播放 SD 卡里的 MP3
ESP32 播放 HTTP 网络音乐
ESP32 做蓝牙音箱
ESP32 做网络收音机
ESP32 播放多种音频格式
```

那 ADF 比自己从零写方便很多。ADF 官方仓库也说明，它支持 MP3、AAC、FLAC、WAV、OGG、OPUS 等音频格式，并支持 HTTP、HLS、SPIFFS、SDCARD、A2DP 等播放来源。([GitHub][3])

---

## ADF 和 ESP-IDF 是啥关系？

可以这样理解：

```text
ESP-IDF：底层开发框架
    包含 WiFi、蓝牙、FreeRTOS、I2S、SPI、GPIO、NVS 等

ESP-ADF：基于 ESP-IDF 的音频应用框架
    在 IDF 上面封装了播放器、解码器、音频流水线、codec 驱动等
```

官方 Get Started 文档也写到，ESP-ADF 是一组组件，用来扩展 ESP-IDF 提供的功能；使用 ADF 之前需要先配置 ESP-IDF 环境。([Espressif Systems][4])

---

## 播放音乐的硬件也要注意

ESP32 播放音乐一般不是直接接喇叭，而是走下面这种结构：

### 方案 A：I2S 数字功放，最简单

```text
ESP32
  ├── BCLK
  ├── LRCLK / WS
  └── DOUT
        ↓
MAX98357A / NS4168 / 其他 I2S功放
        ↓
喇叭
```

优点：简单，适合新手。
缺点：一般只有播放，没有录音输入。

---

### 方案 B：外部音频 Codec，功能更强

```text
ESP32 I2S + I2C
        ↓
ES8388 / WM8978 / AC101 等音频 Codec
        ↓
耳机 / 喇叭 / MIC
```

I2S 负责传音频数据，I2C 负责配置 codec 寄存器，比如音量、采样率、输入输出通道等。

这种适合做：

```text
音乐播放器
录音
语音识别
蓝牙音箱
对讲
AI语音助手
```

ADF 官方也提到，如果不用官方音频开发板，自己的板子需要有兼容的 audio codec 或 DSP 芯片，或者自己写驱动去适配具体芯片。([Espressif Systems][4])

---

## 你应该参考哪个 ADF 示例？

按你的目标选：

| 目标        | 推荐参考                                           |
| --------- | ---------------------------------------------- |
| 播放网络 MP3  | `player/pipeline_http_mp3`                     |
| 播放 SD 卡音乐 | `player/pipeline_sdcard_mp3` 或类似 sdcard player |
| 播放 WAV    | WAV decoder + I2S writer                       |
| 做蓝牙音箱     | A2DP sink 示例                                   |
| 录音        | recorder 相关示例                                  |
| 做语音助手     | ADF + codec + mic + 网络协议                       |

你现在如果只是想“先让喇叭响起来”，建议路线是：

```text
第一步：ESP-IDF I2S 播放固定 PCM/WAV
第二步：接 MAX98357A 或 ES8388
第三步：再上 ADF 播放 MP3
第四步：再做 SD卡 / HTTP / 蓝牙音乐
```

---

## 我给你的建议

你可以参考 ESP-ADF，但不要一上来就直接啃完整 ADF 工程。ADF 工程结构比普通 ESP-IDF 示例复杂，新手容易被这些东西绕晕：

```text
audio_pipeline
audio_element
audio_event_iface
i2s_stream
mp3_decoder
fatfs_stream
http_stream
board codec 配置
```

你应该先记住一句话：

```text
ADF 播放音乐 = 输入流 + 解码器 + 输出流
```

比如：

```text
SD卡文件流 + MP3解码器 + I2S输出流
```

或者：

```text
HTTP网络流 + MP3解码器 + I2S输出流
```

如果你用的是 **ESP32-S3 + ES8388 / WM8978 / MAX98357A**，ADF 非常值得参考；如果你只是播放几个提示音，用 ESP-IDF 的 I2S 就够了。

[1]: https://docs.espressif.com/projects/esp-idf/en/stable/esp32/libraries-and-frameworks/libs-frameworks.html "Espressif's Frameworks - ESP32 -  — ESP-IDF Programming Guide v6.0.1 documentation"
[2]: https://docs.espressif.com/projects/esp-adf/en/latest/api-reference/framework/audio_pipeline.html "Audio Pipeline -  -  — Espressif Audio Development Framework Guide latest documentation"
[3]: https://github.com/espressif/esp-adf "GitHub - espressif/esp-adf: Espressif Advanced Development Framework for Multimedia Applications · GitHub"
[4]: https://docs.espressif.com/projects/esp-adf/en/latest/get-started/ "Get Started -  -  — Espressif Audio Development Framework Guide latest documentation"


------
-------
-------

可以参考，而且**ESP32 播放音乐，ESP-ADF 正是官方比较推荐的音频框架之一**。

**ADF = ESP Audio Development Framework / Espressif Advanced Development Framework**，可以理解成：
在 ESP-IDF 基础上，乐鑫额外封装的一套“音频应用框架”。它里面已经帮你做好了很多音频常用模块，比如：

* MP3 / AAC / WAV / FLAC / OPUS 等解码
* HTTP 网络音乐播放
* SD 卡音乐播放
* SPIFFS 文件播放
* 蓝牙音箱 / A2DP
* I2S 输出
* 音频 codec 驱动抽象
* 播放器、录音机、网络收音机等应用框架

乐鑫官方 ESP-IDF 文档也说明，ESP-ADF 是面向音频应用的综合框架，包含 codec HAL、音乐播放器/录音机、音频处理、蓝牙音箱、网络收音机、免提设备、语音识别等功能。([Espressif Systems][1])

---

## 你要播放音乐，ADF 很适合参考

ADF 的核心思想叫 **Audio Pipeline，音频流水线**。

比如播放一个网络 MP3，大概就是：

```text
HTTP 读取音乐数据
        ↓
MP3 解码器
        ↓
I2S 输出
        ↓
外部功放 / 音频 Codec / 喇叭
```

官方文档里也明确举了这个例子：`HTTP reader stream -> MP3 decoder -> I2S writer stream`，并对应 `player/pipeline_http_mp3` 示例。([Espressif Systems][2])

所以你可以把 ADF 理解成：

```text
数据来源        解码器        输出设备
HTTP     ->    MP3     ->    I2S
SD卡     ->    WAV     ->    I2S
SPIFFS   ->    AAC     ->    I2S
蓝牙     ->    SBC     ->    I2S
```

---

## 但是要看你播放什么音乐

### 1. 只播放简单提示音 / 短 WAV

比如：

```text
开机音
按键音
报警音
短提示音
```

这种不一定非要上 ADF。你可以直接用 **ESP-IDF + I2S** 播放 PCM/WAV 数据，代码更轻，工程更简单。

适合方案：

```text
WAV / PCM 数据 -> i2s_channel_write() -> I2S功放
```

例如外接：

```text
ESP32 I2S -> MAX98357A -> 喇叭
```

这种最简单。

---

### 2. 播放 MP3 / 网络音乐 / SD 卡音乐

这种就很适合 ADF。

比如你想做：

```text
ESP32 播放 SD 卡里的 MP3
ESP32 播放 HTTP 网络音乐
ESP32 做蓝牙音箱
ESP32 做网络收音机
ESP32 播放多种音频格式
```

那 ADF 比自己从零写方便很多。ADF 官方仓库也说明，它支持 MP3、AAC、FLAC、WAV、OGG、OPUS 等音频格式，并支持 HTTP、HLS、SPIFFS、SDCARD、A2DP 等播放来源。([GitHub][3])

---

## ADF 和 ESP-IDF 是啥关系？

可以这样理解：

```text
ESP-IDF：底层开发框架
    包含 WiFi、蓝牙、FreeRTOS、I2S、SPI、GPIO、NVS 等

ESP-ADF：基于 ESP-IDF 的音频应用框架
    在 IDF 上面封装了播放器、解码器、音频流水线、codec 驱动等
```

官方 Get Started 文档也写到，ESP-ADF 是一组组件，用来扩展 ESP-IDF 提供的功能；使用 ADF 之前需要先配置 ESP-IDF 环境。([Espressif Systems][4])

---

## 播放音乐的硬件也要注意

ESP32 播放音乐一般不是直接接喇叭，而是走下面这种结构：

### 方案 A：I2S 数字功放，最简单

```text
ESP32
  ├── BCLK
  ├── LRCLK / WS
  └── DOUT
        ↓
MAX98357A / NS4168 / 其他 I2S功放
        ↓
喇叭
```

优点：简单，适合新手。
缺点：一般只有播放，没有录音输入。

---

### 方案 B：外部音频 Codec，功能更强

```text
ESP32 I2S + I2C
        ↓
ES8388 / WM8978 / AC101 等音频 Codec
        ↓
耳机 / 喇叭 / MIC
```

I2S 负责传音频数据，I2C 负责配置 codec 寄存器，比如音量、采样率、输入输出通道等。

这种适合做：

```text
音乐播放器
录音
语音识别
蓝牙音箱
对讲
AI语音助手
```

ADF 官方也提到，如果不用官方音频开发板，自己的板子需要有兼容的 audio codec 或 DSP 芯片，或者自己写驱动去适配具体芯片。([Espressif Systems][4])

---

## 你应该参考哪个 ADF 示例？

按你的目标选：

| 目标        | 推荐参考                                           |
| --------- | ---------------------------------------------- |
| 播放网络 MP3  | `player/pipeline_http_mp3`                     |
| 播放 SD 卡音乐 | `player/pipeline_sdcard_mp3` 或类似 sdcard player |
| 播放 WAV    | WAV decoder + I2S writer                       |
| 做蓝牙音箱     | A2DP sink 示例                                   |
| 录音        | recorder 相关示例                                  |
| 做语音助手     | ADF + codec + mic + 网络协议                       |

你现在如果只是想“先让喇叭响起来”，建议路线是：

```text
第一步：ESP-IDF I2S 播放固定 PCM/WAV
第二步：接 MAX98357A 或 ES8388
第三步：再上 ADF 播放 MP3
第四步：再做 SD卡 / HTTP / 蓝牙音乐
```

---

## 我给你的建议

你可以参考 ESP-ADF，但不要一上来就直接啃完整 ADF 工程。ADF 工程结构比普通 ESP-IDF 示例复杂，新手容易被这些东西绕晕：

```text
audio_pipeline
audio_element
audio_event_iface
i2s_stream
mp3_decoder
fatfs_stream
http_stream
board codec 配置
```

你应该先记住一句话：

```text
ADF 播放音乐 = 输入流 + 解码器 + 输出流
```

比如：

```text
SD卡文件流 + MP3解码器 + I2S输出流
```

或者：

```text
HTTP网络流 + MP3解码器 + I2S输出流
```

如果你用的是 **ESP32-S3 + ES8388 / WM8978 / MAX98357A**，ADF 非常值得参考；如果你只是播放几个提示音，用 ESP-IDF 的 I2S 就够了。

[1]: https://docs.espressif.com/projects/esp-idf/en/stable/esp32/libraries-and-frameworks/libs-frameworks.html "Espressif's Frameworks - ESP32 -  — ESP-IDF Programming Guide v6.0.1 documentation"
[2]: https://docs.espressif.com/projects/esp-adf/en/latest/api-reference/framework/audio_pipeline.html "Audio Pipeline -  -  — Espressif Audio Development Framework Guide latest documentation"
[3]: https://github.com/espressif/esp-adf "GitHub - espressif/esp-adf: Espressif Advanced Development Framework for Multimedia Applications · GitHub"
[4]: https://docs.espressif.com/projects/esp-adf/en/latest/get-started/ "Get Started -  -  — Espressif Audio Development Framework Guide latest documentation"


------
-----
------

可以移植 ADF，但我建议你先分清楚一个核心问题：

> **ADF 不是“音质增强器”，ADF 是“音频框架”。**

你现在这套代码已经能做到：

```text
SD卡 WAV 文件
    ↓
FatFS 读取
    ↓
wav_play_song() 解析播放
    ↓
I2S 输出
    ↓
ES8388 DAC
    ↓
喇叭/耳机
```

你上传的 `audioplay.c` 里，当前 `audio_play_song()` 对 `T_WAV` 调用了 `wav_play_song(fname)`，但 `T_MP3` 分支还是“自行实现”，说明你现在这套播放器主要已经支持 WAV，MP3 还没实现。

---

# 1. ADF 可以移植到你这个工程上吗？

**可以。**

因为你现在的硬件条件很适合 ADF：

```text
ESP32-S3
ES8388 音频 Codec
I2S 输出
I2C 配置 Codec
SD 卡 / 文件系统
喇叭功放
```

ADF 本来就是乐鑫给音频产品做的框架，官方 ESP-IDF 文档里也把 ESP-ADF 描述为音频应用综合框架，包含 Codec HAL、播放器、录音机、音频处理、蓝牙音箱、网络收音机、语音识别等能力。([Espressif Systems][1])

ADF 的典型播放结构叫 **Audio Pipeline**，比如官方文档里举的例子就是：

```text
HTTP reader stream -> MP3 decoder -> I2S writer stream
```

也就是“输入流 + 解码器 + I2S 输出”。([Espressif Systems][2])

你如果播放 SD 卡或者 Flash 里的人声提示音，可以变成：

```text
FatFs Stream / SPIFFS Stream / Flash-Embedding Stream
        ↓
WAV Decoder / MP3 Decoder
        ↓
I2S Stream
        ↓
ES8388
        ↓
喇叭
```

ADF 官方的 Audio Streams 里也支持 FatFs Stream、I2S Stream、SPIFFS Stream、Flash-Embedding Stream、Tone Stream、TTS Stream 等类型。([Espressif Docs][3])

---

# 2. ADF 播放音效会不会更好？

**不一定。**

如果你播放的是同一个 WAV 文件：

```text
16bit / 16kHz / mono WAV
```

并且最终都是：

```text
I2S -> ES8388 -> 喇叭
```

那理论上：

```text
你当前 wav_play_song() 播放
和
ADF WAV Decoder 播放
```

**音质不会有本质区别。**

音质主要由这些因素决定：

```text
1. 音源文件质量
2. 采样率
3. 位深
4. 是否压缩失真
5. I2S 配置是否正确
6. ES8388 配置是否正确
7. 模拟电路噪声
8. 功放质量
9. 喇叭质量
10. 音量增益是否过大导致削波失真
```

ADF 能帮你解决的是：

```text
播放流程更标准
支持格式更多
播放器控制更方便
缓冲机制更成熟
文件流/网络流/Flash流更统一
以后扩展 MP3、AAC、网络音频更容易
```

但它不会把一个低质量音源自动变成高保真音源。

---

# 3. 人声提示音，用不用 ADF？

看你的目标。

## 情况 A：只播放几个固定人声提示音

比如：

```text
“开机成功”
“网络已连接”
“电量低”
“操作失败”
“请放入猫砂盆”
```

这种我更建议你**先不用 ADF**。

你可以直接用现在的方案：

```text
WAV 文件 -> wav_play_song() -> I2S -> ES8388
```

原因是简单、稳定、代码少。

你甚至可以把文件固定成：

```text
0:/VOICE/power_on.wav
0:/VOICE/wifi_ok.wav
0:/VOICE/error.wav
0:/VOICE/low_power.wav
```

然后封装：

```c
void voice_play_power_on(void)
{
    wav_play_song((uint8_t *)"0:/VOICE/power_on.wav");
}

void voice_play_wifi_ok(void)
{
    wav_play_song((uint8_t *)"0:/VOICE/wifi_ok.wav");
}

void voice_play_error(void)
{
    wav_play_song((uint8_t *)"0:/VOICE/error.wav");
}
```

这对你现在的工程改动最小。

---

## 情况 B：你以后要做完整音频系统

比如你以后想做：

```text
MP3 提示音
网络语音播报
TTS 文本转语音
蓝牙音箱
录音
语音识别
多个音效排队播放
播放中断/恢复
音量统一管理
```

那就值得上 ADF。

因为 ADF 支持多种音频格式，官方仓库也说明播放器/录音器支持 MP3、AAC、FLAC、WAV、OGG、OPUS 等格式，并且有 ESP-IDF 版本兼容表。([GitHub][4])

---

# 4. 你现在工程移植 ADF，不是“加一层”这么简单

这个地方很关键。

你现在已经有：

```c
es8388_init(i2c0_master);
i2s_init();
wav_play_song();
i2s_trx_start();
i2s_trx_stop();
```

如果你移植 ADF，通常会变成：

```text
ADF 初始化 codec
ADF 创建 audio pipeline
ADF 创建 file stream
ADF 创建 wav/mp3 decoder
ADF 创建 i2s stream
ADF 启动 pipeline
```

也就是说，**I2S 和 Codec 最好让 ADF 接管**。

不要这样混用：

```text
你自己的 i2s_init()
+
ADF 的 i2s_stream_init()
```

否则容易出现：

```text
I2S 端口重复初始化
采样率冲突
DMA buffer 冲突
播放没声音
声音变速
声音爆音
```

正确思路是二选一：

```text
方案 1：继续用正点原子现有 ES8388 + I2S + WAV 播放代码
方案 2：改成 ADF 的 codec_hal + i2s_stream + decoder pipeline
```

不要一半一半混着来。

---

# 5. ADF 在你这套硬件上的大概结构

如果你要用 ADF 播放 SD 卡 WAV 人声提示音，结构是：

```text
SD卡文件
    ↓
FatFs Stream
    ↓
WAV Decoder
    ↓
I2S Stream Writer
    ↓
ES8388 Codec
    ↓
喇叭
```

对应 ADF 思想：

```text
输入 element：fatfs_stream
解码 element：wav_decoder
输出 element：i2s_stream
控制接口：audio_pipeline
```

如果你把提示音放到 Flash 里，可以是：

```text
Flash Embedded Audio
    ↓
Flash-Embedding Stream
    ↓
WAV/MP3 Decoder
    ↓
I2S Stream
    ↓
ES8388
```

我反而更推荐人声提示音放 Flash，而不是 SD 卡。

因为产品里播放提示音，SD 卡不一定可靠：

```text
SD 卡没插怎么办？
SD 卡文件损坏怎么办？
SD 卡读取延迟怎么办？
用户误删文件怎么办？
```

提示音这种固定资源，更适合放：

```text
SPIFFS
LittleFS
自定义 Flash 分区
嵌入式数组
ADF Flash-Embedding Stream
```

---

# 6. ADF 音质不会自动更好，但可能减少“播放毛刺”

这里要讲细一点。

如果你当前代码播放 WAV 时存在这些问题：

```text
偶尔卡顿
开头有啪一声
结尾有爆音
播放中有断续
切换音频时有杂音
采样率切换不对
音量突然跳变
```

那 ADF 有可能让体验更好。

不是因为 ADF “音质更高”，而是因为它的：

```text
pipeline 缓冲
stream 管理
decoder 管理
event 事件处理
codec HAL 管理
```

比新手自己写的播放流程更完整。

ADF 的 Audio HAL 也提供了 codec I2S 接口配置、音量、静音、PA 使能等 API，比如可以配置采样率、位宽、I2S/PCM/DSP 格式，也可以设置音量和功放使能。([Espressif Systems][5])

所以结论是：

```text
源文件质量相同 + I2S/ES8388配置相同：
    ADF 音质 ≈ 你当前方案音质

你当前播放流程有卡顿/爆音/缓冲问题：
    ADF 可能让播放体验更稳定

你要支持 MP3/AAC/网络/Flash/队列播放：
    ADF 明显更方便
```

---

# 7. 人声提示音推荐格式

你播放“人声提示音”，不需要高规格音乐参数。

推荐：

```text
格式：WAV PCM
采样率：16000 Hz 或 22050 Hz
位深：16 bit
声道：mono 单声道
```

为什么？

因为人声主要频率集中在中低频，16kHz 采样率已经能满足普通提示音。

大概容量估算：

```text
16kHz × 16bit × 1声道 = 256 kbps
= 32 KB/s
```

也就是说：

```text
1 秒人声 ≈ 32 KB
3 秒人声 ≈ 96 KB
10 秒人声 ≈ 320 KB
```

如果你有 20 条提示音，每条 2 秒：

```text
20 × 2 × 32KB = 1280KB
```

大概 1.25MB，放 Flash 也不是不能接受。

如果你想省空间，可以用：

```text
MP3
OPUS
ADPCM
```

但这样就更适合 ADF，因为 ADF 已经有很多解码器。

---

# 8. 我的建议：你现在先别急着移植 ADF

你的需求是：

> 播放人声提示音

我建议路线是：

```text
第一阶段：
继续用现有 WAV 播放方案
做一个 voice_prompt.c
封装 play_voice("xxx.wav")

第二阶段：
把提示音从 SD 卡挪到 Flash / SPIFFS
避免依赖 SD 卡

第三阶段：
如果需要 MP3、TTS、网络语音、播放队列，再移植 ADF
```

这样最稳。

---

# 9. 你可以先这样封装

比如新建 `voice_prompt.c`：

```c
#include "voice_prompt.h"
#include "wavplay.h"     // 具体看你工程里 wav_play_song 声明在哪个头文件

void voice_prompt_power_on(void)
{
    wav_play_song((uint8_t *)"0:/VOICE/power_on.wav");
}

void voice_prompt_wifi_ok(void)
{
    wav_play_song((uint8_t *)"0:/VOICE/wifi_ok.wav");
}

void voice_prompt_error(void)
{
    wav_play_song((uint8_t *)"0:/VOICE/error.wav");
}

void voice_prompt_low_power(void)
{
    wav_play_song((uint8_t *)"0:/VOICE/low_power.wav");
}
```

然后你业务代码里只调用：

```c
voice_prompt_power_on();
```

不要业务代码到处写：

```c
wav_play_song((uint8_t *)"0:/VOICE/xxx.wav");
```

这样以后你切 ADF 时，只需要改 `voice_prompt.c` 内部实现。

比如未来换成 ADF：

```c
void voice_prompt_power_on(void)
{
    adf_play_file("/spiffs/power_on.mp3");
}
```

业务层不用动。

---

# 10. 什么时候真正建议你上 ADF？

满足下面任意 2 条，我就建议你上 ADF：

```text
1. 要播放 MP3/AAC/OPUS，不只是 WAV
2. 要从 HTTP 网络播放音频
3. 要做蓝牙音箱/A2DP
4. 要做语音识别/录音
5. 要有播放队列
6. 要支持播放暂停/停止/下一条/音量统一控制
7. 要提示音和音乐混合管理
8. 要做产品级音频框架，而不是实验 demo
```

如果只是：

```text
按键响一句“操作成功”
WiFi 连上播一句“网络已连接”
低电量播一句“电量低”
```

那当前方案就够了。

---

# 11. 最终结论

可以这样判断：

```text
只做人声提示音：
    不急着移植 ADF
    继续用 WAV + I2S + ES8388
    建议把提示音封装成 voice_prompt.c

想做完整音频产品：
    可以移植 ADF
    让 ADF 接管 I2S、codec、decoder、pipeline

ADF 会不会音质更好：
    不会自动更好
    音质主要看音源、I2S配置、ES8388、功放、喇叭和模拟电路
    ADF 更强的是框架、格式、播放控制和稳定性
```

你现在最适合的方案是：

```text
保留当前 ES8388 + I2S 初始化
用 WAV 播放人声提示音
先封装 voice_prompt 层
以后真要 ADF，再把 voice_prompt 底层替换成 ADF pipeline
```

[1]: https://docs.espressif.com/projects/esp-idf/en/stable/esp32/libraries-and-frameworks/libs-frameworks.html?utm_source=chatgpt.com "Espressif's Frameworks - ESP32 - — ESP-IDF ..."
[2]: https://docs.espressif.com/projects/esp-adf/en/latest/api-reference/framework/audio_pipeline.html?utm_source=chatgpt.com "Audio Pipeline"
[3]: https://espressif-docs.readthedocs-hosted.com/projects/esp-adf/en/latest/api-reference/streams/index.html?utm_source=chatgpt.com "Audio Streams"
[4]: https://github.com/espressif/esp-adf?utm_source=chatgpt.com "espressif/esp-adf: Espressif Advanced Development ..."
[5]: https://docs.espressif.com/projects/esp-adf/en/latest/api-reference/abstraction/audio_hal.html?utm_source=chatgpt.com "Audio HAL"


------
--------
-------

可以，上 ADF 学习完全没问题。
你现在这套代码刚好适合拿来对照学习，因为它已经有完整的音频硬件链路：

```text
ESP32-S3
  ├── I2C 配置 ES8388
  ├── I2S 输出音频数据
  ├── SPI/SD卡读取音频文件
  ├── XL9555 控制喇叭使能
  └── FatFS 读取 WAV 文件
```

你现在工程里 `audio_play_song()` 对 WAV 是调用 `wav_play_song(fname)`，MP3 分支还没有实现，所以当前核心是“自己写的 WAV 播放器”，不是 ADF 播放器。

---

# 1. 先建立 ADF 的核心概念

ADF 最核心的东西就 4 个：

```text
Audio Pipeline   音频流水线
Audio Element    音频元素
Stream           数据流
Decoder          解码器
```

ADF 官方文档也说，ADF 应用是把不同的 Audio Element 组合成一个 Pipeline；音频数据通常从输入 Stream 获取，经过 Codec/Decoder 处理，最后输出到另一个 Stream。([Espressif Systems][1])

你可以把它理解成：

```text
输入源        解码器        输出
file ---> wav_decoder ---> i2s
```

对于你的人声提示音，就是：

```text
SD卡 WAV 文件
    ↓
FatFs Stream
    ↓
WAV Decoder
    ↓
I2S Stream Writer
    ↓
ES8388
    ↓
喇叭
```

ADF 文档里 WAV Decoder 的应用示例就包含 `player/pipeline_play_sdcard_music`，它就是很适合你学习 SD 卡播放音频的例子。([Espressif Systems][2])

---

# 2. 你原来的代码和 ADF 的对应关系

你现在代码里的模块，迁移到 ADF 后大概是这样：

| 你现在的模块                           | ADF 里对应的东西                                |
| -------------------------------- | ----------------------------------------- |
| `wav_play_song()`                | `wav_decoder`                             |
| `f_open()` / `f_read()`          | `fatfs_stream`                            |
| `i2s_init()` / `i2s_trx_start()` | `i2s_stream`                              |
| `es8388_init()`                  | ADF 的 ES8388 codec driver / Audio HAL     |
| `audio_play()`                   | `audio_pipeline_run()`                    |
| SD 卡初始化                          | `audio_board_sdcard_init()` 或自己移植 SD 卡初始化 |
| `xl9555_pin_write(SPK_EN_IO, 0)` | 仍然要保留，用来打开喇叭功放                            |

重点记住：

```text
ADF 不是替代 ES8388，也不是替代 I2S 硬件。
ADF 是接管“音频播放流程”的框架。
```

---

# 3. 你不能这样混用

迁移 ADF 时，最容易犯的错误是：

```c
i2s_init();              // 你自己的 I2S 初始化
es8388_init();           // 你自己的 ES8388 初始化

// 然后又创建 ADF 的 i2s_stream / codec
```

这样很容易冲突。

因为 I2S 外设、DMA、采样率、位宽、声道格式，ADF 也要配置。
所以迁移时建议变成：

```text
旧播放链路：
wav_play_song() -> i2s_write() -> ES8388

新播放链路：
fatfs_stream -> wav_decoder -> i2s_stream -> ES8388
```

也就是：

```text
保留：I2C 引脚、I2S 引脚、ES8388 芯片、SD 卡、喇叭使能
替换：播放逻辑、WAV 解码逻辑、I2S 写入逻辑
```

---

# 4. 学 ADF 的第一步：先跑官方例程

不要一上来就在你正点原子的工程里硬塞 ADF。
建议先单独拉一个 ADF 工程，把 ADF 编译环境跑通。

ESP-ADF 是一组组件，用来扩展 ESP-IDF；官方 Get Started 也明确说，使用 ADF 之前需要先配置 ESP-IDF。([Espressif Systems][3])

Linux 下大概是：

```bash
cd ~/esp

git clone --recursive -b release/v2.x https://github.com/espressif/esp-adf.git

cd esp-adf

./install.sh

. ./export.sh

echo $ADF_PATH
```

Windows 下官方文档也提供了 `install.bat` / `install.ps1` 和 `export.bat` / `export.ps1` 的流程。([Espressif Systems][3])

ADF 对 IDF 版本有要求，官方 Get Started 明确提醒：ADF 只支持特定 ESP-IDF 版本，Python 版本需要在 3.7 到 3.11 之间。([Espressif Systems][3])
ADF GitHub README 也说，IDF master 分支不建议直接用，因为 IDF master 的重大变化可能和 ADF 冲突。([GitHub][4])

---

# 5. 你应该先跑哪个例程？

你要学人声提示音，优先看这个：

```text
examples/player/pipeline_play_sdcard_music
```

这个比 `play_mp3_control` 更接近你的需求，因为你现在已经是：

```text
SD卡 -> 音频文件 -> I2S -> ES8388
```

官方 ADF 例程说明里也提到 `pipeline_play_sdcard_music` 默认板子是 `ESP32-LyraT V4.3`，其他板子需要在 `menuconfig -> Audio HAL` 里选择对应开发板；它还需要开启 FATFS 长文件名支持。([GitHub][5])

大概流程：

```bash
cd $ADF_PATH/examples/player/pipeline_play_sdcard_music

idf.py set-target esp32s3

idf.py menuconfig

idf.py build
```

menuconfig 里重点看：

```text
Audio HAL
    选择开发板/Codec 配置

Component config
    FAT Filesystem support
        Long filename support
```

---

# 6. 你的板子不是乐鑫官方音频板怎么办？

你的板子是正点原子 ESP32-S3 + ES8388，不一定在 ADF 默认 Audio HAL 板卡列表里。

官方文档也说，不使用官方音频开发板也可以用 ADF，但板子需要有兼容的 audio codec 或 DSP，或者自己开发驱动支持具体芯片。([Espressif Systems][3])

你这个板子有 ES8388，而 ADF 里本身就有 ES8388 驱动。ADF 的 ES8388 文档里有这些接口：

```text
es8388_init()
es8388_config_fmt()
es8388_i2s_config_clock()
es8388_set_bits_per_sample()
es8388_start()
es8388_stop()
es8388_set_voice_volume()
es8388_set_voice_mute()
es8388_config_i2s()
```

这些就是 ADF 里控制 ES8388 的接口。([Espressif Docs][6])

所以你真正要移植的是：

```text
ADF board 层适配
```

也就是把你正点原子的：

```text
I2C 引脚
I2S 引脚
ES8388 地址
SD卡引脚
喇叭功放使能脚
按键引脚
```

填到 ADF 的 board 配置里。

---

# 7. ADF 工程里最重要的几个文件

ADF 不是单个 `main.c` 就完事。你重点看这些：

```text
main/
  └── play_sdcard_music_example.c

components/
  └── audio_board/
        ├── board.c
        ├── board.h
        ├── board_def.h
        └── board_pins_config.c
```

不同 ADF 版本目录可能略有差异，但思想一样：

```text
main.c：写播放流程
board.c：初始化板子
board_def.h：定义引脚
board_pins_config.c：配置 I2S/I2C/SD 等引脚
```

你现在正点原子的初始化：

```c
i2c0_master = iic_init(I2C_NUM_0);
xl9555_init(i2c0_master);
es8388_init(i2c0_master);
i2s_init();
sd_spi_init();
```

在 ADF 里会变成：

```text
audio_board_init()
audio_board_sdcard_init()
audio_hal_ctrl_codec()
audio_pipeline_run()
```

---

# 8. ADF 播放 WAV 的核心代码长什么样？

你先看结构，不要死抠每一个 API。

ADF 播放 WAV 的核心思想大概是：

```c
#include "audio_pipeline.h"
#include "audio_element.h"
#include "audio_event_iface.h"
#include "fatfs_stream.h"
#include "i2s_stream.h"
#include "wav_decoder.h"
#include "board.h"

void app_main(void)
{
    audio_pipeline_handle_t pipeline;
    audio_element_handle_t fatfs_stream_reader;
    audio_element_handle_t wav_decoder;
    audio_element_handle_t i2s_stream_writer;

    /*
     * 1. 初始化开发板
     * 包括 codec、I2C、I2S 引脚、SD卡相关硬件等
     */
    audio_board_handle_t board_handle = audio_board_init();

    /*
     * 2. 打开 ES8388 播放通道
     */
    audio_hal_ctrl_codec(board_handle->audio_hal,
                         AUDIO_HAL_CODEC_MODE_DECODE,
                         AUDIO_HAL_CTRL_START);

    /*
     * 3. 创建 pipeline
     */
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    pipeline = audio_pipeline_init(&pipeline_cfg);

    /*
     * 4. 创建 FatFs 文件输入流
     */
    fatfs_stream_cfg_t fatfs_cfg = FATFS_STREAM_CFG_DEFAULT();
    fatfs_cfg.type = AUDIO_STREAM_READER;
    fatfs_stream_reader = fatfs_stream_init(&fatfs_cfg);

    /*
     * 5. 创建 WAV 解码器
     */
    wav_decoder_cfg_t wav_cfg = DEFAULT_WAV_DECODER_CONFIG();
    wav_decoder = wav_decoder_init(&wav_cfg);

    /*
     * 6. 创建 I2S 输出流
     */
    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
    i2s_cfg.type = AUDIO_STREAM_WRITER;
    i2s_stream_writer = i2s_stream_init(&i2s_cfg);

    /*
     * 7. 注册 element 到 pipeline
     */
    audio_pipeline_register(pipeline, fatfs_stream_reader, "file");
    audio_pipeline_register(pipeline, wav_decoder, "wav");
    audio_pipeline_register(pipeline, i2s_stream_writer, "i2s");

    /*
     * 8. 连接 pipeline
     * file -> wav -> i2s
     */
    const char *link_tag[3] = {"file", "wav", "i2s"};
    audio_pipeline_link(pipeline, &link_tag[0], 3);

    /*
     * 9. 设置要播放的文件路径
     * ADF 里 SD 卡一般挂载到 /sdcard
     */
    audio_element_set_uri(fatfs_stream_reader, "/sdcard/voice/power_on.wav");

    /*
     * 10. 启动播放
     */
    audio_pipeline_run(pipeline);

    /*
     * 11. 实际工程里还要监听播放结束事件，然后 stop/deinit
     */
}
```

这个代码是学习骨架，真正跑起来还要依赖你的 ADF 版本、board 适配、SD 卡挂载路径、ES8388 初始化是否正确。

你重点先记住这一句：

```text
file -> wav_decoder -> i2s
```

这就是 ADF 的核心。

---

# 9. 你的正点原子工程移植 ADF，按这个顺序做

## 第一步：不要动原工程，先跑通 ADF 官方例程

目标：

```text
ADF 能编译
ESP32-S3 target 能设置
官方例程能跑到日志
```

先不管有没有声音。

---

## 第二步：复制一个 ADF 例程出来改

不要直接改 `$ADF_PATH/examples` 里的原始例程。

```bash
cd ~/esp
cp -r $ADF_PATH/examples/player/pipeline_play_sdcard_music ./my_adf_voice
cd my_adf_voice
idf.py set-target esp32s3
idf.py menuconfig
```

---

## 第三步：适配你的 ES8388 和 I2S 引脚

你要从正点原子工程里找这些宏：

```text
ES8388 I2C SDA
ES8388 I2C SCL
ES8388 I2C 地址
I2S BCLK
I2S LRCLK / WS
I2S DOUT
I2S DIN
I2S MCLK
SPK_EN_IO
SD卡 SPI 引脚
```

然后填进 ADF 的 board 配置文件。

你现在代码里已经证明这些硬件可以工作：

```c
i2c0_master = iic_init(I2C_NUM_0);
xl9555_init(i2c0_master);
es8388_init(i2c0_master);
es8388_adda_cfg(1, 0);
es8388_output_cfg(1, 1);
xl9555_pin_write(SPK_EN_IO,0);
i2s_init();
```

迁移时不是丢掉这些信息，而是把这些信息变成 ADF 的 board 配置。

---

## 第四步：先不要播放 MP3，先播放 WAV

你的目标是人声提示音，我建议第一版就用：

```text
16kHz / 16bit / mono / PCM WAV
```

文件路径：

```text
/sdcard/voice/power_on.wav
/sdcard/voice/wifi_ok.wav
/sdcard/voice/error.wav
```

第一版跑通：

```text
/sdcard/voice/power_on.wav -> wav_decoder -> i2s_stream
```

---

## 第五步：封装成提示音接口

业务代码不要直接碰 ADF pipeline。

你应该封装一层：

```c
void voice_prompt_init(void);
void voice_prompt_play_power_on(void);
void voice_prompt_play_wifi_ok(void);
void voice_prompt_play_error(void);
void voice_prompt_set_volume(int volume);
```

以后你的猫砂盆项目里只调用：

```c
voice_prompt_play_power_on();
```

内部到底是 ADF、WAV、MP3、Flash 还是 SD 卡，业务层不用管。

---

# 10. ADF 的学习重点顺序

你不要从蓝牙音箱、TTS、语音识别开始学，会乱。

按这个顺序：

```text
1. audio_pipeline
2. audio_element
3. fatfs_stream
4. wav_decoder
5. i2s_stream
6. audio_event_iface
7. audio_hal / ES8388
8. board 适配
9. 播放结束事件处理
10. 播放队列
```

学习路线图：

```text
第一天：
    跑通 ADF 环境
    编译 play_mp3_control 或 pipeline_play_sdcard_music

第二步：
    看懂 file -> decoder -> i2s

第三步：
    改成播放自己的 power_on.wav

第四步：
    适配正点原子 ES8388 引脚

第五步：
    封装 voice_prompt_play()

第六步：
    做播放结束事件、停止、切换下一条提示音

第七步：
    把 SD 卡提示音换成 SPIFFS / Flash 内置提示音
```

---

# 11. 人声提示音用 ADF 的优势

你坚持上 ADF，是有价值的。

ADF 对人声提示音的价值不是“声音更高级”，而是：

```text
1. 可以统一管理 WAV / MP3 / OPUS 等格式
2. 可以做播放队列
3. 可以处理播放结束事件
4. 可以方便接 SD 卡、SPIFFS、Flash、HTTP
5. 可以以后扩展语音播报、TTS、蓝牙音频
6. 可以更规范地管理 I2S、codec、音量、mute
```

ADF 官方支持的 Stream 包括 FatFs、HTTP、I2S、SPIFFS、Tone、Flash-Embedding、TTS 等，所以以后提示音放 SD 卡、SPIFFS、Flash 或网络，都可以统一到 pipeline 模型里。([Espressif Systems][7])

---

# 12. 你当前项目迁移后的最终架构

最终你可以做成这样：

```text
main.c
  ↓
voice_prompt_init()
  ↓
ADF 初始化
  ├── audio_board_init()
  ├── ES8388 codec start
  ├── SD/SPIFFS/Flash 初始化
  └── 创建 pipeline

业务事件
  ↓
voice_prompt_play(VOICE_WIFI_OK)
  ↓
设置 URI
  ↓
启动 pipeline
  ↓
file/flash -> wav/mp3 decoder -> i2s -> ES8388 -> speaker
```

比如：

```c
typedef enum
{
    VOICE_POWER_ON,
    VOICE_WIFI_OK,
    VOICE_WIFI_FAIL,
    VOICE_LOW_POWER,
    VOICE_ERROR,
} voice_prompt_id_t;

void voice_prompt_play(voice_prompt_id_t id);
```

内部映射：

```c
static const char *voice_uri_table[] = {
    [VOICE_POWER_ON]  = "/sdcard/voice/power_on.wav",
    [VOICE_WIFI_OK]   = "/sdcard/voice/wifi_ok.wav",
    [VOICE_WIFI_FAIL] = "/sdcard/voice/wifi_fail.wav",
    [VOICE_LOW_POWER] = "/sdcard/voice/low_power.wav",
    [VOICE_ERROR]     = "/sdcard/voice/error.wav",
};
```

---

# 13. 你现在最应该做的第一件事

你现在不要先改正点原子这个 `main.c`。

先做这个：

```bash
cd ~/esp
git clone --recursive -b release/v2.x https://github.com/espressif/esp-adf.git
cd esp-adf
./install.sh
. ./export.sh

cd $ADF_PATH/examples/player/pipeline_play_sdcard_music
idf.py set-target esp32s3
idf.py menuconfig
idf.py build
```

这一步的目标只有一个：

```text
先让 ADF 工程能编译通过
```

然后第二步再改：

```text
board 引脚
ES8388 配置
SD 卡模式
I2S 引脚
播放自己的 /sdcard/voice/power_on.wav
```

---

# 14. 一句话总结

你要上 ADF，可以，而且很适合学习。

你这个项目的 ADF 目标应该定成：

```text
正点原子 ESP32-S3 + ES8388
使用 ADF audio_pipeline
播放 SD 卡或 Flash 里的人声 WAV/MP3 提示音
```

第一版 pipeline 就做：

```text
fatfs_stream -> wav_decoder -> i2s_stream -> ES8388
```

等这个跑通，再做：

```text
Flash-Embedding Stream -> wav/mp3_decoder -> i2s_stream
```

这样你就从“能播放”升级到了“真正掌握 ADF 音频框架”。

[1]: https://docs.espressif.com/projects/esp-adf/en/latest/api-reference/index.html "API Reference -  -  — Espressif Audio Development Framework Guide latest documentation"
[2]: https://docs.espressif.com/projects/esp-adf/en/latest/api-reference/codecs/wav_codecs.html?utm_source=chatgpt.com "WAV Decoder and Encoder"
[3]: https://docs.espressif.com/projects/esp-adf/en/latest/get-started/ "Get Started -  -  — Espressif Audio Development Framework Guide latest documentation"
[4]: https://github.com/espressif/esp-adf "GitHub - espressif/esp-adf: Espressif Advanced Development Framework for Multimedia Applications · GitHub"
[5]: https://github.com/espressif/esp-adf/tree/d049321/examples/player/pipeline_play_sdcard_music?utm_source=chatgpt.com "player/pipeline_play_sdcard_music"
[6]: https://espressif-docs.readthedocs-hosted.com/projects/esp-adf/en/latest/api-reference/abstraction/es8388.html "ES8388 Driver — Audio Development Framework  documentation"
[7]: https://docs.espressif.com/projects/esp-adf/en/latest/api-reference/streams/index.html?utm_source=chatgpt.com "Audio Streams"


-----
------
------


可以，下面给你一套**从你现在正点原子 ESP32-S3 + ES8388 工程迁移到 ADF 播放人声提示音**的完整方案。

先给结论：
你不要一上来就把所有东西都换成 ADF。最稳路线是：

```text
第一阶段：
保留你现有 ES8388 / XL9555 / 喇叭使能初始化
去掉你自己的 wav_play_song() 和 i2s_init()
用 ADF 的 fatfs_stream + wav_decoder + i2s_stream 播放人声 WAV

第二阶段：
跑通后，再把 ES8388 初始化也换成 ADF audio_hal / custom board
```

你当前 `audio_play()` 是扫描 `0:/MUSIC` 文件夹，然后调用 `audio_play_song()` 播放文件；而 `audio_play_song()` 里面 WAV 是调用 `wav_play_song(fname)`，MP3 分支还是“自行实现”，所以你现在实际是“自写 WAV 播放器”，不是 ADF。

ADF 官方的播放模型正好是你要学的：

```text
输入流 -> 解码器 -> 输出流
```

ADF 的 Audio Pipeline 负责连接多个 audio element，并按顺序启动和传递数据；官方文档里举的就是 `HTTP reader stream -> MP3 decoder -> I2S writer stream` 这种结构。([Espressif Systems][1])
你做人声提示音，就改成：

```text
SD卡文件
  ↓
FatFs Stream
  ↓
WAV Decoder
  ↓
I2S Stream
  ↓
ES8388
  ↓
喇叭
```

ADF 官方 `pipeline_play_sdcard_music` 例程说明也明确说，它就是用 FatFs element 读 microSD 音乐文件，再用 decoder 解码，最后通过 I2S element 输出。([GitHub][2])

---

# 一、你这块板子迁移 ADF 的总体结构

你现在硬件链路是：

```text
ESP32-S3
 ├── I2C -> ES8388 配置寄存器
 ├── I2S -> ES8388 发送 PCM 音频数据
 ├── SPI -> SD 卡读取音频文件
 ├── XL9555 -> 控制喇叭使能 SPK_EN_IO
 └── ES8388 -> DAC -> 功放/喇叭
```

迁移 ADF 后，第一阶段建议这样分工：

```text
你原来的代码继续负责：
 ├── I2C 初始化
 ├── XL9555 初始化
 ├── ES8388 初始化
 ├── ES8388 音量设置
 └── 喇叭使能

ADF 负责：
 ├── FatFs Stream 读取 /sdcard/voice/power_on.wav
 ├── WAV Decoder 解码
 ├── Audio Pipeline 管理播放流程
 └── I2S Stream 输出 PCM 数据
```

也就是说，第一阶段不要调用你原来的：

```c
i2s_init();
audio_play();
wav_play_song();
```

因为 I2S 要交给 ADF 的 `i2s_stream` 接管。ADF 支持 FatFs Stream、I2S Stream、SPIFFS Stream、Flash-Embedding Stream、TTS Stream 等，正好适合后面扩展提示音来源。([Espressif Systems][3])

---

# 二、准备 ADF 环境

ADF 是在 ESP-IDF 上扩展出来的一套组件，官方说明它是扩展 ESP-IDF 功能的组件集合；使用 ADF 前需要先配置 ESP-IDF，而且 ADF 只支持特定 ESP-IDF 版本，Python 版本也要求在 3.7 到 3.11 之间。([Espressif Systems][4])

你用 ESP32-S3，推荐直接用 ADF 的 `release/v2.x` 分支。官方 README 也说明 ESP-ADF v2.8 及之后主要更新在 `release/v2.x` 分支，`master` 和 `release/v2.x` 不再兼容。([GitHub][5])

## Linux / WSL 操作

```bash
mkdir -p ~/esp
cd ~/esp

git clone -b release/v2.x --recursive https://github.com/espressif/esp-adf.git

cd ~/esp/esp-adf

./install.sh

. ./export.sh

echo $ADF_PATH
echo $IDF_PATH
idf.py --version
```

注意这句前面有一个点和空格：

```bash
. ./export.sh
```

官方文档也强调 Linux/macOS 下需要执行 `. $HOME/esp/esp-adf/export.sh`，并且点号和路径之间有空格。([Espressif Systems][4])

## Windows ESP-IDF Command Prompt 操作

```bat
cd %userprofile%\esp

git clone -b release/v2.x --recursive https://github.com/espressif/esp-adf.git

cd %userprofile%\esp\esp-adf

install.bat

export.bat

echo %ADF_PATH%
echo %IDF_PATH%
idf.py --version
```

---

# 三、先跑官方 ADF 例程，不要直接改你原工程

先验证 ADF 环境能编译。

```bash
cd ~/esp

cp -r $ADF_PATH/examples/player/pipeline_play_sdcard_music ./alientek_adf_voice

cd ~/esp/alientek_adf_voice

idf.py set-target esp32s3

idf.py menuconfig

idf.py build
```

ADF 官方文档建议可以从 examples 里复制一个项目出来跑，而且 ESP-IDF/ADF 路径不要有空格。([Espressif Systems][4])

`pipeline_play_sdcard_music` 这个例程默认支持多种格式，官方 README 列了 MP3、OPUS、OGG、FLAC、AAC、WAV 等格式，并且 WAV 文件名默认可以是 `test.wav`。([GitHub][2])
你第一次学习不要搞 MP3，先用 WAV：

```text
/sdcard/test.wav
```

或者后面改成：

```text
/sdcard/voice/power_on.wav
```

---

# 四、menuconfig 需要重点配置的地方

进入：

```bash
idf.py menuconfig
```

重点看这些：

## 1. Target

你已经执行过：

```bash
idf.py set-target esp32s3
```

所以 target 应该是：

```text
esp32s3
```

## 2. Audio Codec 支持 ES8388

进入：

```text
Component config
    Audio Codec Device Configuration
        Support ES8388 Codec Chip
```

打开 ES8388 支持。ADF 的 Kconfig 里面有 `CODEC_ES8388_SUPPORT`，说明它就是用来支持 ES8388 codec 芯片的。([Espressif Docs][6])

## 3. Audio Board

进入：

```text
Audio HAL
    Audio board
```

你的正点原子板子不是乐鑫官方音频板，后面正式做纯 ADF board 层时应该选：

```text
Custom audio board
```

ADF 的 Audio HAL 里确实有 `AUDIO_BOARD_CUSTOM` 这个选项。([Espressif Docs][6])

不过**第一阶段我建议先不急着用 ADF audio_hal 初始化 ES8388**。
第一阶段先保留你原来的 `es8388_init()`，只让 ADF 管 pipeline、decoder、i2s_stream。

## 4. FATFS 长文件名

进入：

```text
Component config
    FAT Filesystem support
        Long filename support
```

打开长文件名支持。官方 `pipeline_play_sdcard_music` README 也要求开启 FATFS long filename support。([GitHub][2])

## 5. Decoder 选择

第一版只开 WAV：

```text
Audio Codec
    WAV decoder
```

WAV Decoder 官方文档说明它用于把 WAV 格式数据流解码，并且应用示例就是 `player/pipeline_play_sdcard_music`。([Espressif Systems][7])

---

# 五、准备人声提示音文件

第一版推荐：

```text
格式：WAV PCM
采样率：16000Hz 或 22050Hz
位深：16bit
声道：先用 stereo 双声道，跑通后再试 mono
```

为什么第一版先用 stereo？
因为 ES8388 是立体声音频 codec，很多开发板的 I2S/Codec 默认就是双声道。你用 mono 有时候会出现只左声道响、只右声道响，或者数据格式没对齐。

用 ffmpeg 转换：

```bash
ffmpeg -i power_on.mp3 -ar 16000 -ac 2 -sample_fmt s16 power_on.wav
ffmpeg -i wifi_ok.mp3   -ar 16000 -ac 2 -sample_fmt s16 wifi_ok.wav
ffmpeg -i error.mp3     -ar 16000 -ac 2 -sample_fmt s16 error.wav
```

SD 卡目录建议：

```text
/sdcard/voice/power_on.wav
/sdcard/voice/wifi_ok.wav
/sdcard/voice/error.wav
```

实际 SD 卡里就是：

```text
voice/power_on.wav
voice/wifi_ok.wav
voice/error.wav
```

ADF 挂载后路径写：

```c
"/sdcard/voice/power_on.wav"
```

---

# 六、把 ADF 加到你自己的 ESP-IDF 工程

你的工程顶层 `CMakeLists.txt` 要改。
原来普通 IDF 工程一般是：

```cmake
cmake_minimum_required(VERSION 3.5)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(xxx)
```

加 ADF 后，改成：

```cmake
cmake_minimum_required(VERSION 3.5)

include($ENV{ADF_PATH}/CMakeLists.txt)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(alientek_adf_voice)
```

ADF 官方例程的顶层 CMake 也是先 `include($ENV{ADF_PATH}/CMakeLists.txt)`，再 include IDF 的 `project.cmake`。([GitHub][8])

你的 `main/CMakeLists.txt` 可以先这样写：

```cmake
idf_component_register(
    SRCS
        "app_main.c"
        "voice_prompt_adf.c"

    INCLUDE_DIRS
        "."
)
```

如果编译提示缺少组件，再补：

```cmake
idf_component_register(
    SRCS
        "app_main.c"
        "voice_prompt_adf.c"

    INCLUDE_DIRS
        "."

    PRIV_REQUIRES
        nvs_flash
        driver
        fatfs
        sdmmc
        audio_pipeline
        audio_stream
        esp_peripherals
        audio_hal
        esp-adf-libs
)
```

---

# 七、第一阶段代码结构

建议你新增两个文件：

```text
main/
 ├── app_main.c
 ├── voice_prompt_adf.c
 └── voice_prompt_adf.h
```

## `voice_prompt_adf.h`

```c
#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t voice_prompt_adf_init(void);
esp_err_t voice_prompt_play_wav(const char *uri);

esp_err_t voice_prompt_power_on(void);
esp_err_t voice_prompt_wifi_ok(void);
esp_err_t voice_prompt_error(void);

#ifdef __cplusplus
}
#endif
```

---

# 八、第一阶段核心代码：保留你原来的 ES8388，ADF 播放 WAV

下面这版思路是：

```text
1. 用你原来的 iic_init / es8388_init / xl9555 打开 codec 和喇叭
2. 用 ESP-IDF 挂载 SD 卡到 /sdcard
3. 用 ADF 创建 pipeline
4. fatfs_stream -> wav_decoder -> i2s_stream
5. 播放 /sdcard/voice/power_on.wav
```

你需要把下面这些 GPIO 改成你正点原子工程里的真实引脚。
这些引脚去你的工程里找：

```text
iic.h
i2s.h
spi_sdcard.h
lcd.h
xl9555.h
board.h
```

尤其是：

```text
I2S_MCLK
I2S_BCLK
I2S_WS
I2S_DOUT
SD_SPI_MOSI
SD_SPI_MISO
SD_SPI_CLK
SD_SPI_CS
I2C_SDA
I2C_SCL
SPK_EN_IO
```

## `voice_prompt_adf.c`

```c
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"
#include "nvs_flash.h"

#include "driver/gpio.h"
#include "driver/spi_common.h"
#include "driver/sdspi_host.h"
#include "sdmmc_cmd.h"
#include "esp_vfs_fat.h"

#include "audio_pipeline.h"
#include "audio_element.h"
#include "audio_event_iface.h"
#include "audio_common.h"
#include "fatfs_stream.h"
#include "i2s_stream.h"
#include "wav_decoder.h"

/*
 * 下面这些是你原工程已有的头文件。
 * 用来初始化 ES8388、I2C、XL9555、喇叭使能。
 */
#include "iic.h"
#include "es8388.h"
#include "xl9555.h"

/* ===================== 你需要改成自己板子的真实引脚 ===================== */

/* I2S 引脚：去你原来的 i2s.h / i2s.c 里找 */
#define BSP_I2S_PORT        I2S_NUM_0

#define BSP_I2S_MCLK_IO     GPIO_NUM_0      // TODO: 改成你的 MCLK
#define BSP_I2S_BCLK_IO     GPIO_NUM_0      // TODO: 改成你的 BCLK
#define BSP_I2S_WS_IO       GPIO_NUM_0      // TODO: 改成你的 LRCLK / WS
#define BSP_I2S_DOUT_IO     GPIO_NUM_0      // TODO: 改成你的 DOUT
#define BSP_I2S_DIN_IO      I2S_GPIO_UNUSED // 只播放，不录音

/* SD 卡 SPI 引脚：去你原来的 spi_sdcard.h / spi.c 里找 */
#define BSP_SD_SPI_HOST     SPI2_HOST

#define BSP_SD_MOSI_IO      GPIO_NUM_0      // TODO: 改成你的 SD MOSI
#define BSP_SD_MISO_IO      GPIO_NUM_0      // TODO: 改成你的 SD MISO
#define BSP_SD_CLK_IO       GPIO_NUM_0      // TODO: 改成你的 SD CLK
#define BSP_SD_CS_IO        GPIO_NUM_0      // TODO: 改成你的 SD CS

/* ======================================================================= */

static const char *TAG = "VOICE_ADF";

extern i2c_obj_t i2c0_master;

static sdmmc_card_t *s_sd_card = NULL;
static bool s_sd_mounted = false;
static bool s_audio_hw_inited = false;

/**
 * @brief 挂载 SD 卡到 /sdcard
 *
 * ADF 的 fatfs_stream 推荐用 /sdcard/xxx.wav 这种路径。
 */
static esp_err_t voice_sdcard_mount(void)
{
    if (s_sd_mounted) {
        return ESP_OK;
    }

    esp_err_t ret;

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024,
    };

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = BSP_SD_SPI_HOST;

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = BSP_SD_MOSI_IO,
        .miso_io_num = BSP_SD_MISO_IO,
        .sclk_io_num = BSP_SD_CLK_IO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 8 * 1024,
    };

    ret = spi_bus_initialize(host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "spi_bus_initialize failed: %s", esp_err_to_name(ret));
        return ret;
    }

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = BSP_SD_CS_IO;
    slot_config.host_id = host.slot;

    ret = esp_vfs_fat_sdspi_mount("/sdcard",
                                  &host,
                                  &slot_config,
                                  &mount_config,
                                  &s_sd_card);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SD mount failed: %s", esp_err_to_name(ret));
        return ret;
    }

    s_sd_mounted = true;
    ESP_LOGI(TAG, "SD card mounted at /sdcard");
    return ESP_OK;
}

/**
 * @brief 初始化你原来的 ES8388 + XL9555 + 喇叭使能
 *
 * 注意：
 * 这里保留你原来的 ES8388 初始化；
 * 但是不要调用你原来的 i2s_init()，因为 I2S 要交给 ADF i2s_stream。
 */
static esp_err_t voice_audio_hw_init(void)
{
    if (s_audio_hw_inited) {
        return ESP_OK;
    }

    /*
     * 这部分基本照搬你原 main.c 的音频初始化逻辑。
     * 但不要调用 i2s_init()。
     */
    i2c0_master = iic_init(I2C_NUM_0);

    xl9555_init(i2c0_master);

    es8388_init(i2c0_master);
    es8388_adda_cfg(1, 0);          // 开启 DAC，关闭 ADC
    es8388_input_cfg(0);            // 不使用输入
    es8388_output_cfg(1, 1);        // 打开 DAC 输出通道
    es8388_hpvol_set(20);           // 耳机音量
    es8388_spkvol_set(20);          // 喇叭音量

    /*
     * 你原来的代码是：
     * xl9555_pin_write(SPK_EN_IO, 0);
     * 说明这个喇叭使能大概率是低电平有效。
     */
    xl9555_pin_write(SPK_EN_IO, 0);

    s_audio_hw_inited = true;
    ESP_LOGI(TAG, "ES8388 and speaker enabled");
    return ESP_OK;
}

/**
 * @brief 初始化 ADF 播放人声提示音所需资源
 */
esp_err_t voice_prompt_adf_init(void)
{
    esp_err_t ret;

    ret = voice_audio_hw_init();
    if (ret != ESP_OK) {
        return ret;
    }

    ret = voice_sdcard_mount();
    if (ret != ESP_OK) {
        return ret;
    }

    return ESP_OK;
}

/**
 * @brief 播放一个 WAV 文件
 *
 * uri 例子：
 * "/sdcard/voice/power_on.wav"
 */
esp_err_t voice_prompt_play_wav(const char *uri)
{
    if (uri == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Play voice: %s", uri);

    audio_pipeline_handle_t pipeline = NULL;
    audio_element_handle_t fatfs_stream_reader = NULL;
    audio_element_handle_t wav_decoder = NULL;
    audio_element_handle_t i2s_stream_writer = NULL;
    audio_event_iface_handle_t evt = NULL;

    esp_err_t ret = ESP_OK;

    /*
     * 1. 创建 pipeline
     */
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    pipeline = audio_pipeline_init(&pipeline_cfg);
    if (pipeline == NULL) {
        ESP_LOGE(TAG, "audio_pipeline_init failed");
        return ESP_FAIL;
    }

    /*
     * 2. 创建 FatFs 输入流：负责从 /sdcard 读取 WAV 文件
     */
    fatfs_stream_cfg_t fatfs_cfg = FATFS_STREAM_CFG_DEFAULT();
    fatfs_cfg.type = AUDIO_STREAM_READER;
    fatfs_stream_reader = fatfs_stream_init(&fatfs_cfg);
    if (fatfs_stream_reader == NULL) {
        ESP_LOGE(TAG, "fatfs_stream_init failed");
        ret = ESP_FAIL;
        goto exit;
    }

    /*
     * 3. 创建 WAV 解码器
     */
    wav_decoder_cfg_t wav_cfg = DEFAULT_WAV_DECODER_CONFIG();
    wav_decoder = wav_decoder_init(&wav_cfg);
    if (wav_decoder == NULL) {
        ESP_LOGE(TAG, "wav_decoder_init failed");
        ret = ESP_FAIL;
        goto exit;
    }

    /*
     * 4. 创建 I2S 输出流
     *
     * 这里不要调用你原来的 i2s_init()。
     * ADF 的 i2s_stream 会初始化 I2S。
     */
    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT_WITH_PARA(
        BSP_I2S_PORT,
        16000,
        16,
        AUDIO_STREAM_WRITER
    );

    i2s_cfg.type = AUDIO_STREAM_WRITER;

    /*
     * IDF v5 / ADF v2.x 下 i2s_stream_cfg_t 内部包含 std_cfg。
     * 这里直接设置 I2S 标准模式 GPIO。
     */
    i2s_cfg.std_cfg.gpio_cfg.mclk = BSP_I2S_MCLK_IO;
    i2s_cfg.std_cfg.gpio_cfg.bclk = BSP_I2S_BCLK_IO;
    i2s_cfg.std_cfg.gpio_cfg.ws = BSP_I2S_WS_IO;
    i2s_cfg.std_cfg.gpio_cfg.dout = BSP_I2S_DOUT_IO;
    i2s_cfg.std_cfg.gpio_cfg.din = BSP_I2S_DIN_IO;

    i2s_stream_writer = i2s_stream_init(&i2s_cfg);
    if (i2s_stream_writer == NULL) {
        ESP_LOGE(TAG, "i2s_stream_init failed");
        ret = ESP_FAIL;
        goto exit;
    }

    /*
     * 5. 注册三个 element 到 pipeline
     */
    audio_pipeline_register(pipeline, fatfs_stream_reader, "file");
    audio_pipeline_register(pipeline, wav_decoder, "wav");
    audio_pipeline_register(pipeline, i2s_stream_writer, "i2s");

    /*
     * 6. 连接 pipeline：
     * file -> wav -> i2s
     */
    const char *link_tag[3] = {"file", "wav", "i2s"};
    audio_pipeline_link(pipeline, link_tag, 3);

    /*
     * 7. 设置播放文件路径
     */
    audio_element_set_uri(fatfs_stream_reader, uri);

    /*
     * 8. 创建事件监听器
     */
    audio_event_iface_cfg_t evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
    evt = audio_event_iface_init(&evt_cfg);
    if (evt == NULL) {
        ESP_LOGE(TAG, "audio_event_iface_init failed");
        ret = ESP_FAIL;
        goto exit;
    }

    audio_pipeline_set_listener(pipeline, evt);

    /*
     * 9. 启动播放
     */
    ret = audio_pipeline_run(pipeline);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "audio_pipeline_run failed: %s", esp_err_to_name(ret));
        goto exit;
    }

    /*
     * 10. 等待播放结束
     *
     * 很关键：
     * WAV decoder 解析到文件采样率/位宽/声道数后，
     * 要把 I2S 的时钟设置成和音频文件一致。
     * 否则可能声音变速、变调。
     */
    while (1) {
        audio_event_iface_msg_t msg;

        ret = audio_event_iface_listen(evt, &msg, portMAX_DELAY);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "audio_event_iface_listen error: %s", esp_err_to_name(ret));
            break;
        }

        if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT &&
            msg.source == (void *)wav_decoder &&
            msg.cmd == AEL_MSG_CMD_REPORT_MUSIC_INFO) {

            audio_element_info_t music_info = {0};
            audio_element_getinfo(wav_decoder, &music_info);

            ESP_LOGI(TAG,
                     "WAV info: rate=%d, bits=%d, channels=%d",
                     music_info.sample_rates,
                     music_info.bits,
                     music_info.channels);

            audio_element_setinfo(i2s_stream_writer, &music_info);

            i2s_stream_set_clk(i2s_stream_writer,
                               music_info.sample_rates,
                               music_info.bits,
                               music_info.channels);

            continue;
        }

        /*
         * I2S stream 播放完成，说明一条提示音播完了。
         */
        if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT &&
            msg.source == (void *)i2s_stream_writer &&
            msg.cmd == AEL_MSG_CMD_REPORT_STATUS &&
            ((int)msg.data == AEL_STATUS_STATE_STOPPED ||
             (int)msg.data == AEL_STATUS_STATE_FINISHED)) {

            ESP_LOGI(TAG, "Voice finished");
            break;
        }
    }

exit:
    if (pipeline) {
        audio_pipeline_stop(pipeline);
        audio_pipeline_wait_for_stop(pipeline);
        audio_pipeline_terminate(pipeline);

        if (fatfs_stream_reader) {
            audio_pipeline_unregister(pipeline, fatfs_stream_reader);
        }

        if (wav_decoder) {
            audio_pipeline_unregister(pipeline, wav_decoder);
        }

        if (i2s_stream_writer) {
            audio_pipeline_unregister(pipeline, i2s_stream_writer);
        }

        audio_pipeline_remove_listener(pipeline);
    }

    if (evt) {
        audio_event_iface_destroy(evt);
    }

    if (pipeline) {
        audio_pipeline_deinit(pipeline);
    }

    if (fatfs_stream_reader) {
        audio_element_deinit(fatfs_stream_reader);
    }

    if (wav_decoder) {
        audio_element_deinit(wav_decoder);
    }

    if (i2s_stream_writer) {
        audio_element_deinit(i2s_stream_writer);
    }

    return ret;
}

esp_err_t voice_prompt_power_on(void)
{
    return voice_prompt_play_wav("/sdcard/voice/power_on.wav");
}

esp_err_t voice_prompt_wifi_ok(void)
{
    return voice_prompt_play_wav("/sdcard/voice/wifi_ok.wav");
}

esp_err_t voice_prompt_error(void)
{
    return voice_prompt_play_wav("/sdcard/voice/error.wav");
}
```

ADF 的 I2S Stream 文档说明，`i2s_stream_init()` 会创建 I2S Audio Element，`i2s_stream_set_clk()` 可以设置 I2S 的采样率、位宽和声道数，这正是上面代码里播放 WAV 后根据文件信息设置 I2S 时钟的原因。([Espressif Systems][3])

---

# 九、`app_main.c` 第一版

```c
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"

#include "voice_prompt_adf.h"

static const char *TAG = "APP_MAIN";

void app_main(void)
{
    esp_err_t ret;

    /*
     * 1. 初始化 NVS
     */
    ret = nvs_flash_init();

    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {

        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    /*
     * 2. 初始化 ADF 人声提示音模块
     *
     * 内部会初始化：
     * ES8388
     * XL9555
     * 喇叭使能
     * SD 卡挂载到 /sdcard
     */
    ret = voice_prompt_adf_init();

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "voice_prompt_adf_init failed: %s", esp_err_to_name(ret));
        return;
    }

    /*
     * 3. 上电播放一次人声提示音
     */
    voice_prompt_power_on();

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(3000));

        voice_prompt_wifi_ok();

        vTaskDelay(pdMS_TO_TICKS(3000));

        voice_prompt_error();
    }
}
```

---

# 十、第一版工程里哪些旧代码不要调用

你现在工程里这些初始化可以继续保留或挪进 `voice_audio_hw_init()`：

```c
i2c0_master = iic_init(I2C_NUM_0);
xl9555_init(i2c0_master);
es8388_init(i2c0_master);
es8388_adda_cfg(1, 0);
es8388_input_cfg(0);
es8388_output_cfg(1, 1);
es8388_hpvol_set(20);
es8388_spkvol_set(20);
xl9555_pin_write(SPK_EN_IO, 0);
```

但是第一阶段不要调用：

```c
i2s_init();
audio_play();
audio_play_song();
wav_play_song();
i2s_trx_start();
i2s_trx_stop();
```

原因：

```text
ADF 的 i2s_stream 会初始化和管理 I2S
你自己的 i2s_init() 再初始化一次，容易冲突
```

---

# 十一、编译和烧录

```bash
cd ~/esp/alientek_adf_voice

. $HOME/esp/esp-adf/export.sh

idf.py set-target esp32s3

idf.py menuconfig

idf.py build

idf.py -p /dev/ttyUSB0 flash monitor
```

Windows 示例：

```bat
cd %userprofile%\esp\alientek_adf_voice

%userprofile%\esp\esp-adf\export.bat

idf.py set-target esp32s3

idf.py menuconfig

idf.py build

idf.py -p COM5 flash monitor
```

官方文档里的构建和烧录流程也是 `idf.py build`、`idf.py -p PORT flash monitor`。([Espressif Systems][4])

---

# 十二、跑通后你应该看到的日志

正常日志大概会有：

```text
I VOICE_ADF: ES8388 and speaker enabled
I VOICE_ADF: SD card mounted at /sdcard
I VOICE_ADF: Play voice: /sdcard/voice/power_on.wav
I VOICE_ADF: WAV info: rate=16000, bits=16, channels=2
I VOICE_ADF: Voice finished
```

如果你听到了人声，第一阶段就成功了。

---

# 十三、第二阶段：换成纯 ADF board/audio_hal

第一阶段是：

```text
旧 ES8388 驱动 + ADF pipeline
```

第二阶段再做：

```text
ADF audio_hal + ADF custom board + ADF pipeline
```

这一步要把你板子的硬件信息写成 ADF board 配置。

你要准备这些信息：

```text
I2C:
  SDA
  SCL
  I2C port
  ES8388 地址

I2S:
  MCLK
  BCLK
  LRCLK/WS
  DOUT
  DIN

SD:
  SPI CLK
  SPI MOSI
  SPI MISO
  SPI CS

功放/喇叭:
  SPK_EN_IO 是 ESP32 GPIO？
  还是 XL9555 扩展 IO？

Codec:
  ES8388
  DAC 输出到喇叭/耳机
```

ADF 自带 ES8388 驱动，官方 ES8388 文档里有 `es8388_init()`、`es8388_config_fmt()`、`es8388_i2s_config_clock()`、`es8388_set_voice_volume()`、`es8388_start()` 等接口。([Espressif Systems][9])

第二阶段你可以创建：

```text
components/audio_board/
 ├── CMakeLists.txt
 ├── board.c
 ├── board.h
 ├── board_def.h
 └── board_pins_config.c
```

然后在 menuconfig 里选：

```text
Audio HAL
    Audio board
        Custom audio board
```

这一步比较麻烦，尤其是你喇叭使能走 XL9555，不是普通 ESP32 GPIO。
所以我建议你先把第一阶段跑通，因为第一阶段已经能真正学到 ADF 的核心：

```text
audio_pipeline
audio_element
fatfs_stream
wav_decoder
i2s_stream
audio_event_iface
```

---

# 十四、常见问题排查

## 1. 编译提示找不到 ADF 组件

检查：

```bash
echo $ADF_PATH
echo $IDF_PATH
```

重新执行：

```bash
. $HOME/esp/esp-adf/export.sh
```

顶层 `CMakeLists.txt` 必须有：

```cmake
include($ENV{ADF_PATH}/CMakeLists.txt)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
```

---

## 2. 文件打不开

错误类似：

```text
FATFS_STREAM: Failed to open file /sdcard/voice/power_on.wav
```

检查：

```text
1. SD 卡是不是 FAT32
2. 文件路径是不是 voice/power_on.wav
3. 代码里路径是不是 /sdcard/voice/power_on.wav
4. SD SPI 引脚是否正确
5. SD CS 是否正确
6. SD 卡是否已经 mount 成功
```

---

## 3. 没声音，但是日志显示播放完成

优先查：

```text
1. 你有没有误调用 i2s_init()
2. I2S_MCLK/BCLK/WS/DOUT 引脚是否填对
3. ES8388 初始化是否成功
4. es8388_adda_cfg(1, 0) 是否执行
5. es8388_output_cfg(1, 1) 是否执行
6. SPK_EN_IO 是否真的打开
7. xl9555_pin_write(SPK_EN_IO, 0) 是否低电平有效
8. 音量是否太小
```

用示波器看：

```text
BCLK：播放时应该有时钟
WS/LRCLK：播放时应该有左右声道帧时钟
DOUT：播放时应该有数据跳变
```

如果这三个都有，说明 ESP32-S3 已经在输出音频，问题多半在 ES8388 配置、功放使能、喇叭线路。

---

## 4. 声音变快、变慢、变调

重点看这个事件有没有处理：

```c
AEL_MSG_CMD_REPORT_MUSIC_INFO
```

并且有没有执行：

```c
i2s_stream_set_clk(i2s_stream_writer,
                   music_info.sample_rates,
                   music_info.bits,
                   music_info.channels);
```

WAV 文件是 16kHz，你 I2S 却按 44.1kHz 播，就会变快变尖。
WAV 文件是 44.1kHz，你 I2S 按 16kHz 播，就会变慢变低。

---

## 5. 开头“啪”一声

常见原因：

```text
1. 功放打开太早
2. ES8388 DAC 没静音就上电
3. 音频文件开头不是从 0 电平渐入
4. 音量太大
```

处理办法：

```text
1. 先初始化 ES8388
2. 设置较低音量
3. 启动播放前再打开 SPK_EN
4. 人声文件开头加 20ms fade in
5. 结尾加 20ms fade out
```

---

# 十五、你现在最推荐的落地顺序

按这个顺序做：

```text
第 1 步：
搭好 ADF 环境，能 build 官方 pipeline_play_sdcard_music

第 2 步：
把 SD 卡放入 /voice/power_on.wav

第 3 步：
在你的工程顶层 CMakeLists.txt 加 ADF include

第 4 步：
新增 voice_prompt_adf.c / voice_prompt_adf.h

第 5 步：
保留你的 ES8388 + XL9555 初始化
删除/屏蔽 i2s_init()、audio_play()、wav_play_song()

第 6 步：
填入真实 I2S 和 SD SPI GPIO

第 7 步：
调用 voice_prompt_power_on()

第 8 步：
看日志和示波器：
BCLK、WS、DOUT 是否有波形

第 9 步：
听到声音后，再做 wifi_ok.wav、error.wav 等多条提示音

第 10 步：
最后再研究 custom audio board 和 ADF audio_hal
```

---

# 十六、一句话总结

你现在这块 ESP32-S3 + ES8388 板子可以上 ADF。
第一版最稳方案是：

```text
保留原 ES8388/XL9555 初始化
去掉原 i2s_init()/wav_play_song()
新增 ADF pipeline：

fatfs_stream -> wav_decoder -> i2s_stream
```

你的第一个目标就是让它播放：

```text
/sdcard/voice/power_on.wav
```

跑通这个后，你就真正进入 ADF 了。

[1]: https://docs.espressif.com/projects/esp-adf/en/latest/api-reference/framework/audio_pipeline.html "Audio Pipeline -  -  — Espressif Audio Development Framework Guide latest documentation"
[2]: https://github.com/espressif/esp-adf/tree/release/v2.x/examples/player/pipeline_play_sdcard_music "esp-adf/examples/player/pipeline_play_sdcard_music at release/v2.x · espressif/esp-adf · GitHub"
[3]: https://docs.espressif.com/projects/esp-adf/en/latest/api-reference/streams/index.html "Audio Streams -  -  — Espressif Audio Development Framework Guide latest documentation"
[4]: https://docs.espressif.com/projects/esp-adf/en/latest/get-started/ "Get Started -  -  — Espressif Audio Development Framework Guide latest documentation"
[5]: https://github.com/espressif/esp-adf "GitHub - espressif/esp-adf: Espressif Advanced Development Framework for Multimedia Applications · GitHub"
[6]: https://espressif-docs.readthedocs-hosted.com/projects/esp-adf/en/latest/api-reference/kconfig.html "Configuration Options — Audio Development Framework  documentation"
[7]: https://docs.espressif.com/projects/esp-adf/en/latest/api-reference/codecs/wav_codecs.html "WAV Decoder and Encoder -  -  — Espressif Audio Development Framework Guide latest documentation"
[8]: https://raw.githubusercontent.com/espressif/esp-adf/release/v2.x/examples/player/pipeline_play_sdcard_music/CMakeLists.txt "raw.githubusercontent.com"
[9]: https://docs.espressif.com/projects/esp-adf/en/latest/api-reference/abstraction/es8388.html "ES8388 Driver -  -  — Espressif Audio Development Framework Guide latest documentation"


------
----------
----------
明白了，你想把你的 ESP32-S3 + ES8388 音频方案，从目前的 **FatFS + 自己解析 WAV + I2S** 升级到 **ESP-ADF**，然后播放 SD 卡上的音乐或人声。我可以帮你梳理一套详细、具体、可操作的流程，包括环境搭建、硬件配置、ADF 工程创建、WAV 播放到 I2S 输出的完整流程。我们分模块来讲，这样你跟着做就能跑起来。

---

## 1️⃣ 准备工作

### 硬件

* **开发板**：ESP32-S3（你已有）
* **DAC**：ES8388（I2S 接口）
* **输出**：喇叭或耳机
* **存储**：SD 卡（WAV 文件）

### 软件

* **ESP-IDF**：建议 v5.1 以上
* **ESP-ADF**：对应 ESP-IDF 版本
* **工具链**：ESP32S3 对应的工具链，推荐 Windows 用 VSCode + ESP-IDF 插件，Linux 用 Terminal + IDF 环境

---

## 2️⃣ 安装 ESP-IDF + ESP-ADF

1. 安装 ESP-IDF

   ```bash
   # Linux 示例
   git clone -b release/v5.1 --recursive https://github.com/espressif/esp-idf.git
   cd esp-idf
   ./install.sh
   source ./export.sh
   ```

2. 下载 ESP-ADF

   ```bash
   git clone -b release/v3.2 --recursive https://github.com/espressif/esp-adf.git
   export ADF_PATH=~/esp/esp-adf
   ```

   > 注意 ADF 版本需要对应 IDF 版本，否则容易报依赖错误

3. 确认环境：

   ```bash
   idf.py --version
   python -V
   ```

---

## 3️⃣ 创建 ADF 工程

ESP-ADF 自带多个示例，推荐用 **wav_play** 示例作为基础：

```bash
cd $ADF_PATH/examples/player/wav_play
idf.py set-target esp32s3
idf.py menuconfig
```

### menuconfig 配置

1. **Audio HAL** → 选择 ES8388

   * 默认 I2S 引脚配置和你的板子可能不一致，需要手动修改
2. **I2S Configuration**

   * 设置 `I2S_NUM`、`BCLK/MCLK/LRCK` 对应你的 ESP32-S3 接线
3. **SD Card**

   * SPI 或 SDMMC 模式，根据你板子接法选择
4. **Audio Pipeline**

   * ADF 使用 pipeline 模式，wav_play 默认就有 pipeline，里面有：

     ```
     SD卡读取 -> WAV解码 -> I2S输出 -> ES8388
     ```

---

## 4️⃣ 调整 WAV 播放示例

ADF 中 WAV 播放主要涉及 **audio_pipeline** + **audio_element**：

```c
// pipeline example
audio_pipeline_handle_t pipeline;
audio_element_handle_t i2s_stream_writer, sd_reader, wav_decoder;

// 初始化 pipeline
audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
pipeline = audio_pipeline_init(&pipeline_cfg);

// 初始化 SD 卡 reader
sd_reader_cfg_t sd_cfg = DEFAULT_SD_READER_CONFIG();
sd_reader = sd_reader_init(&sd_cfg);

// 初始化 WAV 解码
wav_decoder_cfg_t wav_cfg = DEFAULT_WAV_DECODER_CONFIG();
wav_decoder = wav_decoder_init(&wav_cfg);

// 初始化 I2S
i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
i2s_cfg.type = AUDIO_STREAM_WRITER;
i2s_cfg.i2s_port = 0; // 根据 menuconfig 修改
i2s_stream_writer = i2s_stream_init(&i2s_cfg);

// pipeline 连接
audio_pipeline_register(pipeline, sd_reader, "sd");
audio_pipeline_register(pipeline, wav_decoder, "wav");
audio_pipeline_register(pipeline, i2s_stream_writer, "i2s");

audio_pipeline_link(pipeline, (const char *[]) {"sd", "wav", "i2s"}, 3);

// 播放
audio_pipeline_run(pipeline);
```

> ADF 已经封装了 WAV 解码和 I2S 输出，你只需要确保 SD 卡挂载正确，文件路径无误。

---

## 5️⃣ SD 卡挂载

ADF 里有 `sdcard_init()` 或 `sdcard_mount()`，例如：

```c
sdcard_config_t sd_cfg = {
    .mount_point = "/sdcard",
    .mode = SD_MODE_1LINE,
    .gpio_cd = -1,   // 根据硬件修改
    .gpio_wp = -1,
    .clk = 14,       // SCK
    .cmd = 15,       // CMD
    .d0 = 2,         // D0
    // 如果 4 线 SD 卡，再加 d1-d3
};
sdcard_mount(&sd_cfg);
```

然后 WAV 文件放到 `/sdcard/music.wav`。

---

## 6️⃣ I2S + ES8388 配置

ADF 里有 `audio_hal_codec_config_t`，你需要把 ES8388 初始化和 I2S 引脚改成你的硬件接线：

```c
audio_hal_codec_config_t audio_cfg = {
    .adc_input  = AUDIO_HAL_ADC_INPUT_LINE1,
    .dac_output = AUDIO_HAL_DAC_OUTPUT_ALL,
    .codec_mode = AUDIO_HAL_CODEC_MODE_BOTH,
    .i2s_iface = {
        .mode = AUDIO_HAL_MODE_SLAVE, // 或 MASTER
        .fmt  = AUDIO_HAL_I2S_NORMAL,
        .sample_rate = 44100,
        .bits = 16,
    }
};
audio_hal_init(codec_dev, &audio_cfg);
```

---

## 7️⃣ 播放 WAV / 人声音

1. SD 卡里放一个 WAV 文件，比如 `voice.wav`
2. pipeline 播放：

   ```c
   audio_element_set_uri(sd_reader, "/sdcard/voice.wav");
   audio_pipeline_run(pipeline);
   ```
3. 通过 UART log 可以看到播放进度，音频会输出到 ES8388，然后喇叭或耳机能听到声音。

---

## 8️⃣ 调试注意事项

* **采样率**：WAV 和 I2S 一致，否则会跑速/失真
* **I2S 引脚**：SCK、WS、SD 对应你的 ESP32S3
* **SD 卡**：确认 FATFS 可以挂载并能读文件
* **ES8388**：有些板子需要先复位或供电稳定

---

✅ 总结

通过 ADF，你可以把你的自制 `wav_play_song()` 完全替换成 **ADF pipeline**，好处是：

* 自动管理 SD 卡、WAV 解码、I2S 输出
* 支持 PCM/WAV/MP3/AAC 等格式扩展
* 可以方便加入 EQ、音效处理、录音等模块

---

如果你想，我可以帮你写一份 **完整可跑的 ESP32-S3 + ES8388 + SD卡 WAV 播放 ADF 示例工程**，直接改好 pin、I2S 配置，放 WAV 就能跑，这样你省了很多调试时间。

你希望我直接给这个完整工程吗？


-------
-----------
-----

太好了，既然你是 **Windows + VSCode + ESP-IDF 插件**，那我给你整理一份 **完整可直接跑的 ESP32-S3 + ES8388 + SD卡 WAV 播放 ADF 工程** 的思路和配置，你只需要按步骤导入 VSCode 就能跑起来。我们一步步来，保证你少走弯路。

---

## 1️⃣ 环境要求确认

* **ESP-IDF**：v5.1 或以上
* **ESP-ADF**：v3.2 对应 IDF v5.1
* **VSCode 插件**：ESP-IDF 插件已安装并配置好 IDF/ADF 环境变量

---

## 2️⃣ 文件结构

在你的 workspace 下创建 ADF 工程，结构类似：

```
esp32s3_es8388_wav/
├─ main/
│  ├─ main.c
│  └─ sdkconfig.defaults
├─ components/       (可选，放自定义组件)
├─ CMakeLists.txt
├─ sdkconfig.defaults
└─ README.md
```

---

## 3️⃣ sdkconfig.defaults 配置

```ini
CONFIG_IDF_TARGET="esp32s3"
CONFIG_AUDIO_HAL_CODEC_ES8388=y
CONFIG_AUDIO_HAL_I2S_SCLK=14       # 根据你硬件接线修改
CONFIG_AUDIO_HAL_I2S_LRCK=15
CONFIG_AUDIO_HAL_I2S_DOUT=2
CONFIG_AUDIO_HAL_I2S_DIN=3         # 如果只输出 DAC，可不接
CONFIG_SD_SPI_MISO=19
CONFIG_SD_SPI_MOSI=23
CONFIG_SD_SPI_CLK=18
CONFIG_SD_SPI_CS=5
CONFIG_SD_SPI_MODE=y
CONFIG_SD_SPI_1_LINE=y
CONFIG_WAV_DECODER=y
CONFIG_I2S_STREAM=y
```

> PIN 号要对照你板子的实际接线修改。

---

## 4️⃣ main.c 示例代码

这是核心播放逻辑：

```c
#include "esp_log.h"
#include "esp_system.h"
#include "audio_pipeline.h"
#include "audio_element.h"
#include "audio_event_iface.h"
#include "i2s_stream.h"
#include "wav_decoder.h"
#include "sdcard_stream.h"
#include "board.h"
#include "esp_vfs_fat.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"

static const char *TAG = "ADF_WAV_PLAYER";

void app_main(void)
{
    ESP_LOGI(TAG, "Initializing SD card");
    // SD卡挂载
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5
    };
    sdmmc_card_t* card;
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = 23,
        .miso_io_num = 19,
        .sclk_io_num = 18,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1
    };
    spi_bus_initialize(host.slot, &bus_cfg, SPI_DMA_CH_AUTO);
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = 5;
    esp_err_t ret = esp_vfs_fat_sdspi_mount("/sdcard", &host, &slot_config, &mount_config, &card);
    if(ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount SD card");
        return;
    }

    ESP_LOGI(TAG, "Creating audio pipeline");
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    audio_pipeline_handle_t pipeline = audio_pipeline_init(&pipeline_cfg);

    // 创建 SD Reader
    sdcard_stream_cfg_t sd_cfg = SDSCARD_STREAM_CFG_DEFAULT();
    sd_cfg.type = AUDIO_STREAM_READER;
    sd_cfg.task_core = 0;
    audio_element_handle_t sd_reader = sdcard_stream_init(&sd_cfg);

    // 创建 WAV 解码器
    wav_decoder_cfg_t wav_cfg = DEFAULT_WAV_DECODER_CONFIG();
    audio_element_handle_t wav_decoder = wav_decoder_init(&wav_cfg);

    // 创建 I2S 输出
    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
    i2s_cfg.type = AUDIO_STREAM_WRITER;
    i2s_cfg.i2s_port = 0; // I2S_NUM_0
    audio_element_handle_t i2s_writer = i2s_stream_init(&i2s_cfg);

    // 注册 pipeline
    audio_pipeline_register(pipeline, sd_reader, "sd");
    audio_pipeline_register(pipeline, wav_decoder, "wav");
    audio_pipeline_register(pipeline, i2s_writer, "i2s");

    const char *link_tag[3] = {"sd", "wav", "i2s"};
    audio_pipeline_link(pipeline, link_tag, 3);

    // 设置 WAV 文件路径
    audio_element_set_uri(sd_reader, "/sdcard/voice.wav");

    ESP_LOGI(TAG, "Starting audio pipeline");
    audio_pipeline_run(pipeline);

    // 等待播放结束
    while (1) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
```

> 这段代码实现了：
>
> 1. SD 卡挂载
> 2. 创建 ADF pipeline
> 3. SD 卡读取 -> WAV 解码 -> I2S 输出
> 4. 播放 `/sdcard/voice.wav`

---

## 5️⃣ VSCode 导入与编译

1. 打开 VSCode → 打开工程文件夹
2. 选择 **ESP-IDF 插件** → 配置环境变量（ADF、IDF 路径）
3. 在菜单上执行：

   ```
   idf.py set-target esp32s3
   idf.py menuconfig       # 检查 Pin 配置、SD卡模式、I2S配置
   idf.py build
   idf.py flash monitor
   ```
4. 确保 SD 卡已插入，并放入 `voice.wav` 文件

---

## 6️⃣ 测试

* 烧录完成后，UART 会显示日志：

  ```
  I (1234) ADF_WAV_PLAYER: Starting audio pipeline
  ```
* 如果 I2S 接线和 ES8388 初始化没问题，你就能听到 WAV 文件播放

---

我可以帮你直接做一份 **带 ESP32-S3 pin、ES8388、SD 卡 SPI 配置完整的 VSCode 工程压缩包结构**，你只需要把 `voice.wav` 放入 SD 卡，直接 build + flash 就能跑。

你希望我帮你整理这个完整工程吗？

-------
完美！我帮你整理一份**完整可跑的 ESP32-S3 + ES8388 + SD 卡 WAV 播放 ADF 工程结构**，针对你在 **Windows + VSCode + ESP-IDF 插件** 环境下。你只需要按我给的结构建好文件夹、放入 WAV 文件，就能直接编译、烧录、播放。

---

## 1️⃣ 工程结构

```text
esp32s3_es8388_wav/
├─ main/
│  ├─ main.c
│  └─ sdkconfig.defaults
├─ CMakeLists.txt
├─ components/        (可选，自定义组件放这里)
├─ sdkconfig.defaults (全局默认配置，可覆盖 main 目录)
└─ README.md
```

---

## 2️⃣ main.c

```c
#include "esp_log.h"
#include "esp_system.h"
#include "audio_pipeline.h"
#include "audio_element.h"
#include "audio_event_iface.h"
#include "i2s_stream.h"
#include "wav_decoder.h"
#include "sdcard_stream.h"
#include "board.h"
#include "esp_vfs_fat.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"

static const char *TAG = "ADF_WAV_PLAYER";

void app_main(void)
{
    ESP_LOGI(TAG, "Initializing SD card");

    // SD 卡挂载配置
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5
    };

    sdmmc_card_t* card;
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = 23,
        .miso_io_num = 19,
        .sclk_io_num = 18,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1
    };
    spi_bus_initialize(host.slot, &bus_cfg, SPI_DMA_CH_AUTO);

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = 5;

    esp_err_t ret = esp_vfs_fat_sdspi_mount("/sdcard", &host, &slot_config, &mount_config, &card);
    if(ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount SD card");
        return;
    }

    ESP_LOGI(TAG, "Creating audio pipeline");
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    audio_pipeline_handle_t pipeline = audio_pipeline_init(&pipeline_cfg);

    // SD 卡读取
    sdcard_stream_cfg_t sd_cfg = SDSCARD_STREAM_CFG_DEFAULT();
    sd_cfg.type = AUDIO_STREAM_READER;
    audio_element_handle_t sd_reader = sdcard_stream_init(&sd_cfg);

    // WAV 解码
    wav_decoder_cfg_t wav_cfg = DEFAULT_WAV_DECODER_CONFIG();
    audio_element_handle_t wav_decoder = wav_decoder_init(&wav_cfg);

    // I2S 输出
    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
    i2s_cfg.type = AUDIO_STREAM_WRITER;
    i2s_cfg.i2s_port = 0; // I2S_NUM_0
    audio_element_handle_t i2s_writer = i2s_stream_init(&i2s_cfg);

    // 注册 pipeline
    audio_pipeline_register(pipeline, sd_reader, "sd");
    audio_pipeline_register(pipeline, wav_decoder, "wav");
    audio_pipeline_register(pipeline, i2s_writer, "i2s");

    const char *link_tag[3] = {"sd", "wav", "i2s"};
    audio_pipeline_link(pipeline, link_tag, 3);

    // WAV 文件路径
    audio_element_set_uri(sd_reader, "/sdcard/voice.wav");

    ESP_LOGI(TAG, "Starting audio pipeline");
    audio_pipeline_run(pipeline);

    while (1) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
```

---

## 3️⃣ sdkconfig.defaults (main 目录下)

```ini
CONFIG_IDF_TARGET="esp32s3"

# Audio HAL
CONFIG_AUDIO_HAL_CODEC_ES8388=y
CONFIG_AUDIO_HAL_I2S_SCLK=14
CONFIG_AUDIO_HAL_I2S_LRCK=15
CONFIG_AUDIO_HAL_I2S_DOUT=2
CONFIG_AUDIO_HAL_I2S_DIN=3

# SD 卡 SPI
CONFIG_SD_SPI_MISO=19
CONFIG_SD_SPI_MOSI=23
CONFIG_SD_SPI_CLK=18
CONFIG_SD_SPI_CS=5
CONFIG_SD_SPI_MODE=y
CONFIG_SD_SPI_1_LINE=y

# Audio pipeline
CONFIG_WAV_DECODER=y
CONFIG_I2S_STREAM=y
```

> 注意：PIN号根据你板子实际接线修改。

---

## 4️⃣ CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.16)
include($ENV{ADF_PATH}/tools/cmake/project.cmake)
project(esp32s3_es8388_wav)
```

---

## 5️⃣ 步骤总结（VSCode）

1. **打开 VSCode** → 打开工程目录
2. **菜单选择 ESP-IDF** → 配置环境变量（ADF_PATH / IDF_PATH）
3. **Menuconfig**：

   ```
   idf.py menuconfig
   ```

   * 检查 SD 卡模式
   * 检查 I2S 和 ES8388 引脚
4. **编译烧录**：

   ```
   idf.py build
   idf.py flash monitor
   ```
5. **SD 卡准备**：

   * 插入 SD 卡，路径 `/sdcard/voice.wav`
   * WAV 文件建议 16bit / 44.1kHz PCM

---

✅ 这样就能直接用 ESP-ADF 播放 SD 卡上的 WAV 或人声音了。
播放完成后，如果想循环播放或支持更多格式（MP3、AAC），ADF pipeline 可以很容易扩展。

---

我可以再帮你加一个 **循环播放 SD 卡所有 WAV 文件 + 支持按键切歌的版本**，这样你的开发板就能变成一个完整小音频播放器。

你希望我帮你加这个功能吗？
