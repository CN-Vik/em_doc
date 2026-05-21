可以，把它和 camera 类比一下更好理解：

```text
麦克风 / Codec / ADC
        │
        │ I2S / PDM / PCM / USB Audio
        ▼
SoC 音频控制器
        ▼
DMA
        ▼
ALSA / ASoC 驱动
        ▼
/dev/snd/pcmC0D0c
        ▼
用户态程序
        ▼
VAD / 降噪 / 语音识别库
        ▼
文字 / 命令词 / 控制指令
```

核心结论是：

**音频驱动负责“把声音稳定采成 PCM 数据”；AI 语音识别库负责“把 PCM 数据识别成文字或命令”。**

---

# 1. Linux 音频驱动整体架构

嵌入式 Linux 音频一般走 **ALSA + ASoC**。

ALSA 是 Linux 音频子系统；ASoC 是 ALSA 针对嵌入式 SoC 做的分层架构。Linux 官方文档里说，ASoC 把嵌入式音频系统拆成可复用组件：**Codec driver、Platform driver、Machine driver**。其中 Platform driver 包括 DMA、I2S/AC97/PCM 等 DAI 驱动，Machine driver 负责把 codec、platform、DAI 绑定成一个声卡设备。([Linux Kernel Archives][1])

你可以这样理解：

| 层                                  | 作用                                           | 类比                  |
| ---------------------------------- | -------------------------------------------- | ------------------- |
| Codec driver                       | 控制音频 Codec 芯片，比如 ES8388、WM8978、TLV320、ES8311 | 外部芯片驱动              |
| CPU DAI driver                     | 控制 SoC 内部 I2S/PDM/PCM 控制器                    | MCU 外设驱动            |
| DMA driver                         | 搬运音频数据到内存                                    | DMA 驱动              |
| Machine driver / simple-audio-card | 把 SoC I2S 和 Codec 连接起来                       | 板级适配                |
| ALSA PCM                           | 给用户态提供录音/播放接口                                | `/dev/snd/pcmC0D0c` |
| 用户程序                               | arecord、aplay、语音识别程序                         | App                 |

---

# 2. 音频采集链路怎么走？

以麦克风录音为例：

```text
模拟麦克风
   │
   ▼
音频 Codec 内部 ADC
   │
   │ I2S 数据
   ▼
SoC I2S 控制器
   │
   ▼
DMA 写入内存 ring buffer
   │
   ▼
ALSA PCM capture
   │
   ▼
用户态 read()
   │
   ▼
语音识别库
```

如果是数字麦克风，可能是：

```text
PDM Mic
   │
   ▼
SoC PDM/DMIC 控制器
   │
   ▼
DMA
   │
   ▼
ALSA PCM capture
```

如果是 USB 麦克风，就是：

```text
USB Mic
   │
   ▼
USB Audio Class driver
   │
   ▼
ALSA PCM capture
```

对你来说，最重要的是最终都变成：

```text
PCM 原始音频数据
```

常见语音识别输入格式一般是：

```text
采样率：16000 Hz
声道数：单声道 mono
格式：16-bit signed little-endian PCM
简称：S16_LE / 16k / mono
```

---

# 3. ALSA 里的播放和录音节点

Linux 里音频设备通常在：

```bash
/dev/snd/
```

例如：

```bash
ls /dev/snd/
```

可能看到：

```text
controlC0
pcmC0D0c
pcmC0D0p
timer
```

解释一下：

```text
controlC0     声卡 0 的控制节点
pcmC0D0c      card 0, device 0, capture，录音
pcmC0D0p      card 0, device 0, playback，播放
```

其中：

```text
c = capture 录音
p = playback 播放
```

用户态一般不会直接操作 `/dev/snd/pcmC0D0c`，而是通过 ALSA API 或 `arecord` 这种工具操作。

ALSA PCM 接口内部用 ring buffer 管理录音/播放数据，硬件和应用各维护自己的读写位置；音频流会被分成 period，小块完成后通知应用处理。ALSA 官方文档对 PCM ring buffer、period 机制有说明。([alsa-project.org][2])

---

# 4. Linux 音频驱动开发流程

## 第一步：先确认硬件

你必须先看原理图，搞清楚：

```text
1. 麦克风类型
   模拟麦克风？
   数字 PDM 麦克风？
   USB 麦克风？

2. 是否有音频 Codec
   例如 ES8388、ES8311、WM8978、TLV320AIC310x

3. Codec 通过什么控制？
   I2C？
   SPI？

4. 音频数据接口是什么？
   I2S？
   PCM？
   TDM？
   PDM？
   USB？

5. 时钟怎么来？
   MCLK 是 SoC 输出？
   Codec 自己有晶振？
   BCLK/LRCK 谁做 master？

6. 麦克风接到哪个输入？
   MIC1？
   MIC2？
   LINEIN？
   Differential input？

7. 是否有功放？
   Speaker amplifier 有没有 enable GPIO？
```

这一步非常关键。音频驱动很多问题都不是代码问题，而是：

```text
时钟不对
I2S 主从关系不对
左右声道不对
Codec routing 没打开
Capture volume 没开
Mic bias 没开
```

---

## 第二步：确认内核已有驱动

在内核里查：

```bash
find kernel/sound -name "*es8388*"
find kernel/sound -name "*wm8978*"
find kernel/sound -name "*es8311*"
find kernel/sound -name "*tlv320*"
```

或者：

```bash
grep -R "compatible.*es8388" kernel/sound -n
grep -R "compatible.*wm8978" kernel/sound -n
```

如果你的 Codec 已经有驱动，优先用现成驱动。

一般路径类似：

```text
kernel/sound/soc/codecs/
kernel/sound/soc/ingenic/
kernel/sound/soc/generic/
```

以嵌入式 Linux 来说，真正需要你改的经常不是 codec 驱动，而是：

```text
设备树 DTS
声卡绑定
I2S 参数
amixer 路由
```

---

# 5. 设备树 DTS 怎么写？

很多板子可以用 `simple-audio-card` 绑定 SoC I2S 和 Codec。Linux 设备树绑定文档说明，`simple-audio-card` 用来描述 SoC 和 Codec 之间的 DAI 连接，常见属性包括声卡名、格式、routing、cpu、codec 等。([Linux Kernel Archives][3])

一个简化 DTS 模板如下：

```dts
&i2c1 {
    status = "okay";

    codec: audio-codec@10 {
        compatible = "everest,es8388";   /* 这里按你的 codec 改 */
        reg = <0x10>;

        #sound-dai-cells = <0>;

        /* 如果 codec 需要时钟、电源、reset，可以在这里加 */
        // clocks = <&clk_audio>;
        // clock-names = "mclk";
        // reset-gpios = <&gpio1 3 GPIO_ACTIVE_LOW>;
        // AVDD-supply = <&vcc_3v3>;
    };
};

&i2s0 {
    status = "okay";
    #sound-dai-cells = <0>;

    /* pinctrl 根据 SoC SDK 写 */
    // pinctrl-names = "default";
    // pinctrl-0 = <&i2s0_pins>;
};

sound {
    compatible = "simple-audio-card";
    simple-audio-card,name = "x2600-audio-card";

    /*
     * 常见格式：
     * i2s / left_j / right_j / dsp_a / dsp_b
     */
    simple-audio-card,format = "i2s";

    /*
     * 谁提供 BCLK/LRCK？
     * 有的板子 codec 做 master，有的 SoC 做 master。
     * 这个必须看硬件和 codec 配置。
     */
    simple-audio-card,bitclock-master = <&dailink_master>;
    simple-audio-card,frame-master = <&dailink_master>;

    simple-audio-card,widgets =
        "Microphone", "Mic Jack",
        "Speaker", "Speaker";

    simple-audio-card,routing =
        "MIC1", "Mic Jack",
        "Speaker", "LOUT1",
        "Speaker", "ROUT1";

    simple-audio-card,cpu {
        sound-dai = <&i2s0>;
    };

    dailink_master: simple-audio-card,codec {
        sound-dai = <&codec>;
    };
};
```

注意：这个只是模板，真正项目里名字可能不是 `i2s0`，Codec 也不一定是 `es8388`。你要以你 SDK 的 DTS 和 codec driver binding 为准。

---

# 6. 内核配置需要打开什么？

大方向是：

```text
CONFIG_SOUND
CONFIG_SND
CONFIG_SND_PCM
CONFIG_SND_SOC
CONFIG_SND_SIMPLE_CARD
CONFIG_SND_SOC_你的SOC_I2S
CONFIG_SND_SOC_你的CODEC
CONFIG_I2C
CONFIG_DMA_ENGINE
```

如果是 PDM 数字麦克风，还要看 SoC 有没有：

```text
CONFIG_SND_SOC_XXX_DMIC
CONFIG_SND_SOC_DMIC
```

如果是 USB 麦克风：

```text
CONFIG_SND_USB_AUDIO
```

---

# 7. 编译烧录后怎么验证音频驱动？

## 7.1 看声卡有没有注册

```bash
cat /proc/asound/cards
```

正常可能看到：

```text
 0 [x2600audio     ]: x2600-audio-card - x2600-audio-card
                      x2600-audio-card
```

再看 PCM 设备：

```bash
cat /proc/asound/pcm
```

可能看到：

```text
00-00: I2S PCM : : playback 1 : capture 1
```

---

## 7.2 查看录音设备

```bash
arecord -l
```

可能看到：

```text
card 0: x2600audio [x2600-audio-card], device 0: I2S PCM
```

播放设备：

```bash
aplay -l
```

---

## 7.3 设置麦克风增益和通路

先看控制项：

```bash
amixer -c 0 contents
```

或者：

```bash
amixer -c 0 scontrols
```

常见需要打开：

```bash
amixer -c 0 sset 'Capture' 80%
amixer -c 0 sset 'Mic' 80%
amixer -c 0 sset 'Mic Boost' 20dB
amixer -c 0 sset 'Capture Switch' on
```

具体名字每个 Codec 不一样。
比如有的叫：

```text
ADC Capture Volume
Left Input Mixer MIC1
Right Input Mixer MIC1
Mic Bias
ADC PGA Gain
```

新手经常卡在这里：**驱动起来了，但是录出来全是 0，因为 Codec 内部路由没打开。**

---

## 7.4 录音测试

录 5 秒，16k 单声道：

```bash
arecord -D hw:0,0 -f S16_LE -r 16000 -c 1 -d 5 test.wav
```

播放：

```bash
aplay test.wav
```

如果板子没有喇叭，可以把文件拷到电脑听。

也可以看 WAV 信息：

```bash
file test.wav
```

正常应该类似：

```text
RIFF (little-endian) data, WAVE audio, Microsoft PCM, 16 bit, mono 16000 Hz
```

---

# 8. AI 语音识别库应该放在哪里？

不要放在内核驱动里。

正确结构是：

```text
kernel driver:
    负责 I2S / Codec / DMA / ALSA

user app:
    负责 ALSA 录音
    负责音频预处理
    负责调用 AI 语音识别库
    负责命令解析
```

也就是：

```text
arecord 能正常录音
        ↓
你自己的 C/C++ 程序用 ALSA 采集 PCM
        ↓
把 PCM 喂给 Vosk / whisper.cpp / 其他 ASR 库
        ↓
得到识别结果
        ↓
执行控制逻辑
```

---

# 9. 常见 AI 语音识别库选择

## 方案 1：Vosk

Vosk 是离线开源语音识别工具包，官方介绍支持 20 多种语言/方言，有 C、C++、Python、Java、Node、Go、Rust 等绑定，并且可以用于小设备到服务器。([GitHub][4])

适合：

```text
离线命令识别
智能家居
简单中文语音控制
资源中等的嵌入式 Linux
```

优点：

```text
可以流式识别
模型相对小
适合命令词场景
有 C API
```

缺点：

```text
准确率通常不如 Whisper 大模型
交叉编译依赖相对复杂
MIPS 平台可能没有现成预编译包
```

---

## 方案 2：whisper.cpp

`whisper.cpp` 是 Whisper 的 C/C++ 实现。官方 quick start 里给出的流程是下载 ggml 模型、用 CMake 编译，然后用 `whisper-cli -f samples/jfk.wav` 转写音频；它也要求输入转换成 16-bit WAV，例如 16k 单声道 PCM。([GitHub][5])

适合：

```text
离线语音转文字
准确率要求较高
CPU/RAM 比较充足
```

但是要注意资源占用。官方 README 里列了模型内存参考：tiny 磁盘约 75 MiB、内存约 273 MB；base 磁盘约 142 MiB、内存约 388 MB；small 内存约 852 MB。([GitHub][5])

所以对很多嵌入式板子来说：

```text
tiny 模型勉强可能
base 以上可能吃力
small/medium/large 基本不适合低端 SoC
```

你的 X2600 如果是 MIPS XBurst2 工具链，`whisper.cpp` 能不能顺利跑，要看 CPU 算力、内存、是否支持需要的指令优化。实际项目里建议先从 Vosk、小模型命令识别、或者云端识别开始。

---

## 方案 3：云端语音识别

流程：

```text
ALSA 采集 PCM
    ↓
编码成 wav/opus/amr
    ↓
HTTP/WebSocket 上传
    ↓
云端返回文字
```

优点：

```text
准确率高
本地算力压力小
模型不用放板子上
```

缺点：

```text
需要网络
有延迟
有隐私问题
依赖第三方服务
```

---

# 10. 最推荐你的落地路线

对新手，我建议按这个顺序做：

```text
第一步：先把 Linux 音频驱动跑通
第二步：用 arecord 能录出正常 wav
第三步：把 wav 文件丢给识别库测试
第四步：再写 C 程序实时采集 ALSA PCM
第五步：把 PCM 流喂给 ASR 库
第六步：识别结果做命令解析
```

千万不要一上来就想：

```text
驱动 + AI + 实时识别 + 唤醒词 + 降噪 + UI
```

这样很容易乱。

---

# 11. 最小验证流程：先录音，再识别

## 11.1 先录音

```bash
arecord -D hw:0,0 -f S16_LE -r 16000 -c 1 -d 5 test.wav
```

确认 `test.wav` 在电脑上听起来正常。

如果听起来是：

```text
全是噪声
声音很小
左右声道反了
速度变快/变慢
全是 0
```

先别管 AI，先修音频驱动和 codec 配置。

---

## 11.2 用 whisper.cpp 测 wav 文件

开发机上可以这样测试：

```bash
git clone https://github.com/ggml-org/whisper.cpp.git
cd whisper.cpp

sh ./models/download-ggml-model.sh tiny

cmake -B build
cmake --build build -j

./build/bin/whisper-cli -m models/ggml-tiny.bin -f test.wav
```

如果输入不是 16k 单声道 16-bit WAV，先转：

```bash
ffmpeg -i input.wav -ar 16000 -ac 1 -c:a pcm_s16le output.wav
```

这个命令格式也和 whisper.cpp 官方 README 里建议的转换方式一致。([GitHub][5])

---

# 12. 真正产品里的程序结构

你的程序可以这样设计：

```text
main()
  ├── audio_capture_init()
  │     ├── snd_pcm_open()
  │     ├── 设置采样率 16000
  │     ├── 设置格式 S16_LE
  │     ├── 设置单声道
  │     └── 设置 period/buffer
  │
  ├── asr_init()
  │     ├── 加载模型
  │     └── 创建 recognizer
  │
  ├── while(1)
  │     ├── snd_pcm_readi() 读取一小段 PCM
  │     ├── VAD 判断是否有人声
  │     ├── 喂给 ASR
  │     ├── 得到 partial/final result
  │     └── 命令解析
  │
  └── cleanup()
```

核心思想：

```text
ALSA 采一帧 PCM
        ↓
ASR 库处理一帧 PCM
        ↓
循环
```

---

# 13. ALSA 采集代码大概长什么样？

这是简化版逻辑：

```c
#include <stdio.h>
#include <stdlib.h>
#include <alsa/asoundlib.h>

#define SAMPLE_RATE     16000
#define CHANNELS        1
#define PERIOD_FRAMES   160     // 16000Hz 下 160 帧约等于 10ms

int main(void)
{
    snd_pcm_t *pcm = NULL;
    snd_pcm_hw_params_t *params = NULL;

    short buffer[PERIOD_FRAMES];

    int ret;

    /*
     * 打开录音设备。
     *
     * "hw:0,0" 表示：
     * card 0, device 0
     *
     * SND_PCM_STREAM_CAPTURE 表示录音方向。
     */
    ret = snd_pcm_open(&pcm, "hw:0,0", SND_PCM_STREAM_CAPTURE, 0);
    if (ret < 0) {
        printf("snd_pcm_open failed: %s\n", snd_strerror(ret));
        return -1;
    }

    snd_pcm_hw_params_malloc(&params);
    snd_pcm_hw_params_any(pcm, params);

    /*
     * 交错模式。
     * 单声道时影响不大。
     * 双声道时数据排列为 L R L R L R...
     */
    snd_pcm_hw_params_set_access(pcm, params, SND_PCM_ACCESS_RW_INTERLEAVED);

    /*
     * 16bit 小端有符号 PCM。
     * 语音识别最常用。
     */
    snd_pcm_hw_params_set_format(pcm, params, SND_PCM_FORMAT_S16_LE);

    /*
     * 单声道。
     */
    snd_pcm_hw_params_set_channels(pcm, params, CHANNELS);

    /*
     * 采样率 16k。
     */
    unsigned int rate = SAMPLE_RATE;
    snd_pcm_hw_params_set_rate_near(pcm, params, &rate, 0);

    /*
     * 设置硬件参数。
     */
    ret = snd_pcm_hw_params(pcm, params);
    if (ret < 0) {
        printf("snd_pcm_hw_params failed: %s\n", snd_strerror(ret));
        snd_pcm_close(pcm);
        return -1;
    }

    snd_pcm_hw_params_free(params);

    printf("start capture, rate=%u\n", rate);

    while (1) {
        /*
         * 读取 PERIOD_FRAMES 帧。
         * 单声道 S16_LE 下：
         * 1 frame = 1 sample = 2 bytes
         */
        ret = snd_pcm_readi(pcm, buffer, PERIOD_FRAMES);

        if (ret == -EPIPE) {
            /*
             * XRUN，应用处理太慢或调度不及时导致溢出。
             * 需要 prepare 重新开始。
             */
            printf("overrun occurred\n");
            snd_pcm_prepare(pcm);
            continue;
        } else if (ret < 0) {
            printf("snd_pcm_readi failed: %s\n", snd_strerror(ret));
            continue;
        }

        /*
         * 这里 buffer 里就是 PCM 音频数据。
         *
         * 下一步：
         * 1. 可以写入 wav 文件
         * 2. 可以喂给 VAD
         * 3. 可以喂给 Vosk / whisper.cpp
         */
    }

    snd_pcm_close(pcm);
    return 0;
}
```

编译：

```bash
gcc audio_capture.c -o audio_capture -lasound
```

交叉编译大概是：

```bash
mips-linux-gnu-gcc audio_capture.c -o audio_capture -lasound
```

具体编译器名字要换成你 X2600 SDK 里的工具链，例如你之前环境里类似：

```text
mips-xburst2-gcc720-glibc238
```

---

# 14. 加入 Vosk 的程序结构

Vosk 流式识别大概是：

```c
#include <stdio.h>
#include <alsa/asoundlib.h>
#include "vosk_api.h"

#define SAMPLE_RATE     16000
#define CHANNELS        1
#define PERIOD_FRAMES   160

int main(void)
{
    snd_pcm_t *pcm;
    short buffer[PERIOD_FRAMES];

    /*
     * 1. 初始化 ALSA 录音
     * 这里省略，使用上一节的 audio_capture_init 更好。
     */

    /*
     * 2. 加载 Vosk 模型
     */
    VoskModel *model = vosk_model_new("/usr/share/vosk/model-cn");
    if (!model) {
        printf("failed to load vosk model\n");
        return -1;
    }

    /*
     * 3. 创建识别器
     * 第二个参数必须和 PCM 采样率一致。
     */
    VoskRecognizer *recognizer = vosk_recognizer_new(model, 16000.0);

    while (1) {
        int frames = snd_pcm_readi(pcm, buffer, PERIOD_FRAMES);
        if (frames < 0) {
            snd_pcm_prepare(pcm);
            continue;
        }

        /*
         * buffer 是 short，也就是 int16_t PCM。
         * Vosk 接收字节流，所以长度 = frames * channels * 2。
         */
        int bytes = frames * CHANNELS * sizeof(short);

        if (vosk_recognizer_accept_waveform(recognizer,
                                            (const char *)buffer,
                                            bytes)) {
            const char *result = vosk_recognizer_result(recognizer);
            printf("final: %s\n", result);
        } else {
            const char *partial = vosk_recognizer_partial_result(recognizer);
            printf("partial: %s\n", partial);
        }
    }

    vosk_recognizer_free(recognizer);
    vosk_model_free(model);

    return 0;
}
```

实际工程里你应该拆成：

```text
audio_capture.c
audio_capture.h
asr_vosk.c
asr_vosk.h
main.c
```

---

# 15. 加入 whisper.cpp 的方式

whisper.cpp 更常见两种接法。

## 方式 1：先录 wav，再调用 whisper-cli

适合新手验证：

```bash
arecord -D hw:0,0 -f S16_LE -r 16000 -c 1 -d 5 test.wav

./whisper-cli -m models/ggml-tiny.bin -f test.wav
```

优点：

```text
简单
容易验证
不用马上写 C++ 集成
```

缺点：

```text
不是实时
延迟大
每次要生成文件
```

---

## 方式 2：C/C++ 里集成 whisper.cpp

流程：

```text
ALSA 实时采集 PCM
    ↓
缓存几秒音频
    ↓
转成 whisper.cpp 需要的 float 数组
    ↓
whisper_full()
    ↓
取 segment text
```

但是 whisper.cpp 对低端嵌入式板子压力比较大。
如果你的板子 RAM 只有 128MB 或 256MB，Whisper tiny 都可能比较吃力，因为官方给出的 tiny 内存参考约 273 MB。([GitHub][5])

所以产品上更建议：

```text
低端板子：Vosk / 命令词识别 / 云端识别
中高端板子：whisper.cpp tiny/base
高性能平台：Whisper small 以上
```

---

# 16. 放到 Buildroot SDK 里怎么做？

你现在这种嵌入式 Linux SDK，大概率有 Buildroot。加入语音识别库通常有两种方式。

---

## 方式 A：先手动交叉编译，拷到 rootfs

适合新手第一阶段。

```text
1. 在 Ubuntu 上交叉编译 libasound / ASR 库 / 自己的 app
2. 拷贝到 rootfs：
   /usr/bin/voice_app
   /usr/lib/libvosk.so
   /usr/share/vosk/model-cn/
3. 重新打包 rootfs
4. 烧录系统
5. 板端运行 voice_app
```

比如：

```bash
cp voice_app rootfs/usr/bin/
cp libvosk.so rootfs/usr/lib/
cp -r model-cn rootfs/usr/share/vosk/
```

然后重新打包文件系统。

---

## 方式 B：写成 Buildroot package

适合后期工程化。

目录类似：

```text
buildroot/package/voice_app/
    Config.in
    voice_app.mk
```

`Config.in`：

```makefile
config BR2_PACKAGE_VOICE_APP
    bool "voice_app"
    select BR2_PACKAGE_ALSA_LIB
    help
      Voice recognition application using ALSA capture.
```

`voice_app.mk`：

```makefile
VOICE_APP_VERSION = 1.0
VOICE_APP_SITE = $(TOPDIR)/../package/voice_app/src
VOICE_APP_SITE_METHOD = local

VOICE_APP_DEPENDENCIES = alsa-lib

define VOICE_APP_BUILD_CMDS
    $(TARGET_CC) \
        -I$(@D) \
        $(@D)/main.c \
        $(@D)/audio_capture.c \
        -o $(@D)/voice_app \
        -lasound
endef

define VOICE_APP_INSTALL_TARGET_CMDS
    $(INSTALL) -D -m 0755 $(@D)/voice_app \
        $(TARGET_DIR)/usr/bin/voice_app
endef

$(eval $(generic-package))
```

然后在 Buildroot 菜单里选中：

```text
Target packages
  -> voice_app
```

重新编译 rootfs。

---

# 17. 实时语音识别程序的推荐架构

建议你不要把录音和识别放在一个阻塞循环里。更合理的是：

```text
线程 1：audio_capture_thread
    从 ALSA 读取 PCM
    放入 ring buffer / queue

线程 2：asr_thread
    从 queue 取 PCM
    做 VAD / ASR
    输出文字

线程 3：command_thread
    根据识别结果执行动作
```

结构：

```text
ALSA Capture
     │
     ▼
PCM Ring Buffer
     │
     ▼
VAD
     │
     ▼
ASR
     │
     ▼
Command Parser
     │
     ▼
控制设备 / UI / 网络
```

为什么要分线程？

```text
ALSA 采集必须及时
AI 识别可能耗时很长
如果识别阻塞采集，就会 overrun
```

---

# 18. VAD 是什么？为什么需要？

VAD 是 Voice Activity Detection，语音活动检测。

作用：

```text
判断当前音频里有没有人在说话
```

没有 VAD 的话，ASR 会一直识别环境噪声，浪费 CPU。

推荐流程：

```text
ALSA 采集 10ms PCM
    ↓
VAD 判断是否有人声
    ↓
有人声才送给 ASR
    ↓
静音一段时间后认为一句话结束
```

常见一帧长度：

```text
10ms：160 samples @ 16kHz
20ms：320 samples @ 16kHz
30ms：480 samples @ 16kHz
```

---

# 19. 新手最容易踩的坑

## 坑 1：驱动没问题，但录音全是 0

常见原因：

```text
Codec capture route 没打开
Mic Bias 没打开
ADC 没打开
Capture Switch 是 off
Capture Volume 是 0
麦克风电源没开
```

先用：

```bash
amixer -c 0 contents
```

把跟 MIC、ADC、Capture 相关的控制项都看一遍。

---

## 坑 2：声音速度不对

比如你说一句话，播放出来变快或变慢。

常见原因：

```text
实际采样率和设置采样率不一致
MCLK/BCLK/LRCK 时钟配置错误
I2S master/slave 配错
```

比如你以为是 16k，但硬件实际跑成了 8k 或 48k。

---

## 坑 3：录出来全是噪声

常见原因：

```text
I2S 数据位宽错
左右对齐格式错
I2S / left_j / right_j 配错
BCLK/LRCK 极性错
DMA sample format 错
模拟增益过大
地噪声/电源噪声
```

---

## 坑 4：AI 识别很差

先不要怀疑 AI 模型，先确认音频质量。

检查：

```text
1. 采样率是否 16k
2. 是否单声道
3. 是否 S16_LE
4. 人声是否清晰
5. 音量是否太小
6. 有没有削顶 clipping
7. 有没有底噪
8. 麦克风离人是否太远
9. 是否有回声
```

语音识别效果高度依赖前端音频质量。

---

## 坑 5：识别程序运行后 ALSA overrun

表现：

```text
overrun occurred
snd_pcm_readi failed
录音断断续续
```

原因：

```text
AI 识别太耗时
同一个线程里阻塞太久
period/buffer 太小
CPU 被其他任务抢占
```

解决：

```text
录音线程和识别线程分开
加 PCM ring buffer
适当增大 ALSA buffer
降低模型大小
降低采样率到 16k
使用 VAD 减少识别量
```

---

# 20. 你可以按这个具体流程做

## 阶段 1：只搞音频驱动

目标：

```text
arecord 能录出正常 wav
```

操作：

```bash
cat /proc/asound/cards
cat /proc/asound/pcm
arecord -l
amixer -c 0 contents

arecord -D hw:0,0 -f S16_LE -r 16000 -c 1 -d 5 test.wav
```

---

## 阶段 2：PC 上验证 ASR 库

把板子录出来的 `test.wav` 拷到 Ubuntu PC：

```bash
scp root@board:/tmp/test.wav .
```

用 Vosk 或 whisper.cpp 识别。

这样可以判断：

```text
音频数据是否适合识别
模型效果是否满足需求
```

---

## 阶段 3：板端跑 ASR demo

先不要实时，先做文件识别：

```bash
voice_asr_file test.wav
```

成功后再做实时。

---

## 阶段 4：板端实时识别

流程：

```text
ALSA 采集
    ↓
VAD
    ↓
ASR streaming
    ↓
命令词解析
```

命令词可以先做简单字符串匹配：

```c
if (strstr(result, "打开灯")) {
    light_on();
} else if (strstr(result, "关闭灯")) {
    light_off();
}
```

---

# 21. 一句话总结

Linux 音频识别项目可以拆成两部分：

```text
音频驱动：
    Codec + I2S/PDM + DMA + ALSA/ASoC
    目标是让 arecord 能稳定录到 16k mono S16_LE PCM

AI 语音识别：
    用户态程序读取 ALSA PCM
    经过 VAD/降噪/缓存
    喂给 Vosk/whisper.cpp/云端 ASR
    得到文字或命令
```

你真正动手时，第一目标不是 AI，而是先做到：

```bash
arecord -D hw:0,0 -f S16_LE -r 16000 -c 1 -d 5 test.wav
```

只要这个 `test.wav` 清晰、采样率正确、音量合适，后面接 AI 语音识别库才有意义。

[1]: https://www.kernel.org/doc/html/v5.9/sound/soc/overview.html "ALSA SoC Layer Overview — The Linux Kernel  documentation"
[2]: https://www.alsa-project.org/alsa-doc/alsa-lib/pcm.html?utm_source=chatgpt.com "the C library reference: PCM (digital audio) interface"
[3]: https://www.kernel.org/doc/Documentation/devicetree/bindings/sound/simple-card.txt "www.kernel.org"
[4]: https://github.com/alphacep/vosk-api "GitHub - alphacep/vosk-api: Offline speech recognition API for Android, iOS, Raspberry Pi and servers with Python, Java, C# and Node · GitHub"
[5]: https://github.com/ggml-org/whisper.cpp "GitHub - ggml-org/whisper.cpp: Port of OpenAI's Whisper model in C/C++ · GitHub"
