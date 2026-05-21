下面按**新手能落地开发/调试的顺序**讲 Linux camera 驱动。你可以把它理解成：**Linux camera 不是一个单独驱动，而是一条“视频采集链路”**。

---

# 1. 先建立整体概念：Camera 数据怎么进 Linux？

典型嵌入式 Linux camera 链路是这样：

```text
Camera Sensor
  │
  │ 1. I2C/SPI 控制通道
  │    用来配置寄存器：分辨率、曝光、增益、帧率、开流/关流
  │
  ├──────────────> Linux sensor driver
  │
  │ 2. 图像数据通道
  │    MIPI CSI-2 / DVP / BT.656 / LVDS 等
  │
  ▼
SoC CSI 接收器 / DVP 接收器
  ▼
ISP 图像处理
  ▼
DMA 搬运到内存
  ▼
/dev/videoX
  ▼
应用程序：v4l2-ctl / GStreamer / OpenCV / 自己写的 App
```

Linux 里常用 **V4L2** 管理 camera。V4L2 是 Linux 的视频设备用户态 API，应用通过它打开 `/dev/videoX`、设置格式、申请 buffer、启动采集等。官方文档明确把它定义为 Video for Linux API version 2。([Linux Kernel 文档][1])

---

# 2. Camera 驱动不是一个文件，而是几个部分配合

新手最容易误解的是：
**以为 camera 驱动就是写一个 `xxx_camera.c`。**

实际上通常分成这些部分：

| 层级           | 负责内容                                     | 常见文件位置                              |
| ------------ | ---------------------------------------- | ----------------------------------- |
| Sensor 驱动    | 控制外部摄像头芯片，例如 OV5640、GC2053、SC2336、IMX219 | `drivers/media/i2c/xxx.c`           |
| CSI/DVP 接收驱动 | 控制 SoC 内部视频接收硬件                          | `drivers/media/platform/...`        |
| ISP 驱动       | 3A、去噪、颜色处理、Raw 转 YUV 等                   | SoC 厂商 SDK 里                        |
| Device Tree  | 描述 sensor 接在哪个 I2C、哪个 MIPI/DVP、复位脚、电源、时钟 | `arch/xxx/boot/dts/*.dts`           |
| 用户态工具        | 测试和采集画面                                  | `media-ctl`、`v4l2-ctl`、`gst-launch` |

Linux 的 V4L2 框架可以同时管理外部 sensor 和 camera interface，采集 raw frame、JPEG 或其他视频格式；ST 的嵌入式 Linux 文档也把典型 camera 子系统分成 sensor driver、camera interface driver、V4L2 core、media-ctl、v4l2-ctl 等部分。([STMicroelectronics][2])

---

# 3. 你要先分清两个通道：控制通道和图像通道

## 3.1 控制通道：I2C

Camera sensor 一般通过 I2C 配置寄存器。

比如：

```text
I2C 写寄存器：
0x3008 = 0x42   // software power down
0x3103 = 0x11   // clock source
0x3808 = 0x05   // output width high byte
0x3809 = 0x00   // output width low byte
```

这个跟你之前搞 IMU 很像，都是通过 I2C 读写寄存器。

但是 camera sensor 的寄存器量非常大，一个分辨率模式可能有几百个寄存器配置。

---

## 3.2 图像数据通道：MIPI CSI-2 / DVP

图像数据不是从 I2C 读出来的。

I2C 只是控制；真正的图像流通常走：

```text
MIPI CSI-2
DVP 并口
BT.656
LVDS
```

MIPI CSI-2 是专门用于把 camera 图像数据传到 SoC 的高速总线。Linux 文档也说明，CSI-2 是用于把图像从摄像头传输到 host SoC 的数据总线。([Infradead][3])

所以你调 camera 的时候，要分清：

```text
I2C 通了 ≠ 有图像
sensor ID 读到了 ≠ 图像链路正常
/dev/video0 出来了 ≠ 一定能出图
能 stream on ≠ 图像颜色/格式一定正确
```

---

# 4. Linux camera 驱动的核心框架：V4L2 + Media Controller

现代嵌入式 Linux camera 多数不是简单的一个 `/dev/video0`，而是一个 **media graph**。

可以理解成这样：

```text
sensor subdev  --->  mipi-csi receiver  --->  isp  --->  capture video node
```

每个模块是一个 entity，中间通过 pad/link 连接。

常见查看命令：

```bash
media-ctl -p
```

可能看到类似：

```text
Media controller API version 5.x.x

- entity 1: ov5640 1-003c
    pad0: Source
      -> "csi2":0 [ENABLED]

- entity 5: csi2
    pad0: Sink
    pad1: Source
      -> "isp":0 [ENABLED]

- entity 8: isp
    pad0: Sink
    pad1: Source
      -> "capture":0 [ENABLED]

- entity 12: capture
    type: Node
    device node name /dev/video0
```

Linux V4L2 sub-device 机制就是为这种 sensor、mux、decoder、camera controller 等子模块设计的。官方文档提到，camera sensor 和 camera controller 常见就是 V4L2 sub-device，很多情况下这些 sub-device 通过 I2C 控制。([Linux Kernel Archives][4])

---

# 5. 开发 camera 驱动的完整流程

下面按真正做项目的顺序来。

---

## 第一步：看硬件原理图和 sensor datasheet

你必须先搞清楚这些信息：

```text
1. sensor 型号
   例如：OV5640、GC2053、SC2336、IMX219、OS04A10

2. sensor I2C 地址
   例如：0x3c、0x36、0x10

3. 控制脚
   reset-gpio
   pwdn-gpio
   power-enable-gpio

4. 电源
   avdd
   dovdd
   dvdd

5. 主时钟
   xclk / mclk 是多少 MHz？
   常见 24MHz、27MHz

6. 图像接口
   MIPI CSI-2？
   DVP？
   几 lane？
   RAW8 / RAW10 / RAW12 / YUYV / UYVY？

7. sensor 输出分辨率和帧率
   1920x1080@30fps？
   1280x720@60fps？
   640x480@30fps？

8. 是否需要 ISP
   RAW sensor 一般需要 ISP
   YUV sensor 可以直接出 YUV
```

这一步非常重要。
很多 camera 问题不是代码问题，而是**硬件描述错了**。

---

## 第二步：确认内核里有没有现成 sensor 驱动

先查内核源码：

```bash
find kernel/drivers/media -name "*ov5640*"
find kernel/drivers/media -name "*gc2053*"
find kernel/drivers/media -name "*imx219*"
```

也可以：

```bash
grep -R "compatible.*ov5640" kernel/drivers/media -n
grep -R "ov5640" kernel/arch -n
```

如果有现成驱动，优先改设备树和寄存器模式。
新手不建议一上来从零写 sensor 驱动。

一般路径类似：

```text
kernel/drivers/media/i2c/ov5640.c
kernel/drivers/media/i2c/imx219.c
kernel/drivers/media/i2c/gc2145.c
```

如果是厂商 SDK，比如君正、瑞芯微、全志、NXP、海思，它们可能不完全走主线 Linux 结构，可能有自己的 ISP 框架和 sensor 适配层。

---

## 第三步：配置内核选项

你至少需要打开这些方向的配置：

```text
CONFIG_MEDIA_SUPPORT
CONFIG_VIDEO_DEV
CONFIG_V4L2_FWNODE
CONFIG_MEDIA_CONTROLLER
CONFIG_VIDEO_V4L2_SUBDEV_API
CONFIG_VIDEOBUF2_CORE
CONFIG_VIDEOBUF2_DMA_CONTIG
```

还需要：

```text
I2C 控制器驱动
GPIO 驱动
Regulator 框架
Clock 框架
SoC CSI/DVP/ISP 驱动
具体 sensor 驱动
```

在 menuconfig 里大概是：

```text
Device Drivers
  -> Multimedia support
      -> Media drivers
      -> Media ancillary drivers
          -> Camera sensor devices
```

ST 的文档里也给了类似思路：需要启用 Multimedia support、Media platform devices、具体 SoC camera interface driver，以及外部 camera sensor driver。([STMicroelectronics][2])

---

## 第四步：写设备树 DTS

设备树是 camera 驱动里最容易踩坑的地方。

典型 MIPI CSI-2 sensor 设备树大概长这样：

```dts
&i2c1 {
    status = "okay";

    camera@36 {
        compatible = "ovti,ov5640";
        reg = <0x36>;

        clocks = <&clk_cam>;
        clock-names = "xclk";

        reset-gpios = <&gpio1 5 GPIO_ACTIVE_LOW>;
        powerdown-gpios = <&gpio1 6 GPIO_ACTIVE_HIGH>;

        avdd-supply = <&vcc_cam_avdd>;
        dovdd-supply = <&vcc_cam_dovdd>;
        dvdd-supply = <&vcc_cam_dvdd>;

        port {
            camera_out: endpoint {
                remote-endpoint = <&csi_in>;
                data-lanes = <1 2>;
                clock-lanes = <0>;
                link-frequencies = /bits/ 64 <456000000>;
            };
        };
    };
};

&csi {
    status = "okay";

    port {
        csi_in: endpoint {
            remote-endpoint = <&camera_out>;
            data-lanes = <1 2>;
            clock-lanes = <0>;
        };
    };
};
```

设备树里的 `port` / `endpoint` 非常关键。Linux 的 video interface 绑定文档说明，视频数据链路里的设备通过 `port` 节点描述接口，通过 `endpoint` 描述连接参数，两个 endpoint 通过 `remote-endpoint` 互相连接。([Android Git Repositories][5])

你可以把它理解成：

```text
sensor 的输出口  <----remote-endpoint---->  CSI 的输入口
```

如果这个没配对，驱动可能能 probe，但是 media graph 连接不起来，最后没有 `/dev/videoX` 或者不能采集。

---

# 6. Sensor 驱动内部流程

一个 sensor 驱动大概做这些事情：

```text
probe()
  ├── 获取设备树资源
  │     clock
  │     reset-gpio
  │     powerdown-gpio
  │     regulators
  │
  ├── 上电 sensor
  │     enable regulator
  │     enable xclk/mclk
  │     reset gpio 拉高/拉低
  │     delay
  │
  ├── I2C 读 chip ID
  │     确认 sensor 存在
  │
  ├── 初始化 v4l2_subdev
  │     v4l2_i2c_subdev_init()
  │
  ├── 初始化 media pad
  │     sensor 一般只有 source pad
  │
  ├── 注册 controls
  │     exposure
  │     gain
  │     hblank
  │     vblank
  │     test pattern
  │     link_freq
  │     pixel_rate
  │
  ├── 注册 subdev
  │     v4l2_async_register_subdev_sensor()
  │
  └── probe 完成
```

V4L2 的异步注册很重要。官方文档说，sub-device 驱动在满足条件后通过 `v4l2_async_register_subdev()` 注册，bridge/host driver 通过 notifier 匹配这些 sub-device；而且 sub-device 在注册前必须完成初始化，因为注册后它就可能被访问。([Linux Kernel 文档][6])

---

## Sensor 驱动里最核心的几个函数

### 1. `probe()`

负责初始化。

```c
static int xxx_probe(struct i2c_client *client)
{
    /* 1. 分配私有结构体 */
    /* 2. 获取 clock、gpio、regulator */
    /* 3. 上电 */
    /* 4. 读 chip id */
    /* 5. 初始化 v4l2_subdev */
    /* 6. 注册 media entity */
    /* 7. 注册 controls */
    /* 8. 注册 subdev */
}
```

---

### 2. `s_stream()`

负责开流和关流。

```c
static int xxx_s_stream(struct v4l2_subdev *sd, int enable)
{
    if (enable) {
        /* 写 sensor 初始化寄存器表 */
        /* 设置当前分辨率寄存器 */
        /* 设置曝光/增益 */
        /* sensor stream on */
    } else {
        /* sensor stream off */
    }

    return 0;
}
```

这个函数非常关键。
用户态执行：

```bash
v4l2-ctl --stream-mmap
```

最后内核链路里通常会调用到 sensor 的 `s_stream(1)`。

MIPI CSI-2 相关文档也提到，CSI-2 transmitter，例如 sensor，需要向 receiver 提供 link frequency、pixel rate 等信息，并通过 `s_stream()` callback 启停视频流。([Infradead][3])

---

### 3. `enum_mbus_code()`

告诉上层 sensor 支持什么数据格式。

比如：

```text
MEDIA_BUS_FMT_SBGGR10_1X10
MEDIA_BUS_FMT_SRGGB10_1X10
MEDIA_BUS_FMT_UYVY8_2X8
MEDIA_BUS_FMT_YUYV8_2X8
```

如果 sensor 输出 RAW10，你不能在这里乱填 YUYV。
否则后面 CSI/ISP 解析格式会错。

---

### 4. `set_fmt()` / `get_fmt()`

设置/获取当前 sensor 输出格式。

比如：

```text
1920x1080
MEDIA_BUS_FMT_SBGGR10_1X10
field = V4L2_FIELD_NONE
colorspace = V4L2_COLORSPACE_SRGB
```

---

### 5. controls 控制项

常见 controls：

```text
V4L2_CID_EXPOSURE
V4L2_CID_ANALOGUE_GAIN
V4L2_CID_DIGITAL_GAIN
V4L2_CID_VBLANK
V4L2_CID_HBLANK
V4L2_CID_PIXEL_RATE
V4L2_CID_LINK_FREQ
V4L2_CID_TEST_PATTERN
```

曝光、自动曝光、对焦等属于 V4L2 camera control 范围。比如官方文档里 `V4L2_CID_EXPOSURE_ABSOLUTE` 用于设置曝光时间，并说明了曝光时间单位解释。([Linux Kernel Archives][7])

---

# 7. CSI/DVP/ISP 驱动负责什么？

Sensor 驱动只负责 sensor 本身。
真正把数据接进内存的是 SoC 侧驱动。

例如：

```text
MIPI CSI-2 receiver driver
DVP receiver driver
ISP driver
DMA capture driver
```

它们负责：

```text
1. 配置 MIPI lane 数量
2. 配置输入格式 RAW10/YUV422
3. 配置分辨率
4. 配置 DMA buffer
5. 处理中断
6. 检查帧开始/帧结束
7. 把数据送到 /dev/videoX
8. 参与 media graph
```

你可以这么理解：

```text
sensor driver：让摄像头吐数据
CSI driver：把数据接住
ISP driver：把 Raw 图像处理成人能看的图
video node driver：把图像交给用户程序
```

---

# 8. 用户态测试流程

驱动起来后，不要一上来写 App。
先用工具测。

---

## 8.1 看内核日志

```bash
dmesg | grep -i camera
dmesg | grep -i sensor
dmesg | grep -i csi
dmesg | grep -i isp
dmesg | grep -i v4l2
```

重点看：

```text
sensor 是否 probe 成功？
I2C 读 ID 是否成功？
clock 是否获取失败？
regulator 是否找不到？
reset gpio 是否有效？
CSI 是否绑定到 sensor？
是否创建 /dev/videoX？
```

---

## 8.2 看 video 节点

```bash
ls /dev/video*
ls /dev/media*
ls /dev/v4l-subdev*
```

可能看到：

```text
/dev/video0
/dev/media0
/dev/v4l-subdev0
/dev/v4l-subdev1
```

---

## 8.3 看 media graph

```bash
media-ctl -p
```

你要确认链路是通的：

```text
sensor -> csi -> isp -> video
```

如果没有 link，或者 link 没 enabled，说明设备树或 async bind 有问题。

---

## 8.4 查看支持格式

```bash
v4l2-ctl -d /dev/video0 --list-formats-ext
```

看输出有没有：

```text
YUYV
NV12
RGB565
BA10
RG10
```

RAW sensor 可能看到 Bayer 格式；带 ISP 后可能看到 NV12、YUYV。

---

## 8.5 采集一帧

```bash
v4l2-ctl -d /dev/video0 \
  --set-fmt-video=width=640,height=480,pixelformat=YUYV \
  --stream-mmap \
  --stream-count=1 \
  --stream-to=frame.yuv
```

如果是 NV12：

```bash
v4l2-ctl -d /dev/video0 \
  --set-fmt-video=width=1280,height=720,pixelformat=NV12 \
  --stream-mmap \
  --stream-count=30 \
  --stream-to=test.nv12
```

---

## 8.6 连续采集看帧率

```bash
v4l2-ctl -d /dev/video0 \
  --stream-mmap \
  --stream-count=100 \
  --stream-to=/dev/null
```

如果正常，应该看到类似：

```text
<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
100 frames captured
```

如果卡住，说明：

```text
sensor 没真正出图
CSI 没收到帧
MIPI lane 配置错
format 不匹配
中断没来
DMA 没配置好
```

---

# 9. 新手最容易遇到的问题

## 问题 1：I2C 能读到 ID，但就是没图

这非常常见。

原因可能是：

```text
1. sensor 没 stream on
2. MIPI lane 数量错
3. data-lanes 顺序错
4. clock-lanes 配错
5. link-frequency 配错
6. pixel-rate 配错
7. sensor 输出 RAW10，但 CSI 以 RAW8/YUV 接收
8. 分辨率不匹配
9. reset/pwdn 时序不对
10. xclk 频率不对
```

结论：

```text
I2C OK 只说明控制通道通了，不代表图像链路通了。
```

---

## 问题 2：有 `/dev/video0`，但是采集卡死

常见原因：

```text
1. sensor 没有真正输出 frame
2. CSI 没收到 frame start / frame end
3. IRQ 没触发
4. DMA buffer 没完成
5. media pipeline 没 enable
6. v4l2-ctl 设置的格式和 sensor 输出不一致
```

调试方向：

```bash
dmesg -w
media-ctl -p
v4l2-ctl --all -d /dev/video0
v4l2-ctl --list-formats-ext -d /dev/video0
```

---

## 问题 3：画面有，但是颜色不对

比如：

```text
绿色
紫色
偏红
黑白
条纹
颜色反了
```

常见原因：

```text
1. Bayer pattern 错
   BGGR / GBRG / GRBG / RGGB 搞反了

2. YUV 顺序错
   YUYV / UYVY / VYUY / YVYU 搞错

3. RAW bit width 错
   RAW8 / RAW10 / RAW12 不匹配

4. ISP 参数不对
   AWB / CCM / Gamma / LSC 参数没适配

5. stride / bytesperline 不对
```

---

## 问题 4：画面有横纹、花屏、撕裂

常见原因：

```text
1. MIPI 信号质量不好
2. MIPI lane 配置错
3. sensor 输出时钟过高
4. link_freq / pixel_rate 不对
5. DVP pclk 极性错
6. hsync/vsync 极性错
7. DMA stride 错
8. DDR 带宽不够
9. ISP 输入尺寸和 sensor 输出尺寸不一致
```

---

## 问题 5：probe 失败，提示 `-EPROBE_DEFER`

这个不是一定错误。

通常表示某些依赖资源还没准备好：

```text
clock 还没注册
regulator 还没注册
gpio controller 还没准备好
pinctrl 还没准备好
```

V4L2 subdev 文档也提到，异步 probe 时，如果 master clock 等条件还没满足，驱动可以返回 `-EPROBE_DEFER` 等待后续重新 probe。([Linux Kernel 文档][6])

---

# 10. 新手开发顺序建议

不要一上来就改一堆东西。推荐这个顺序：

```text
第一步：确认硬件信息
  sensor 型号、I2C 地址、reset、pwdn、电源、mclk、MIPI/DVP

第二步：确认内核已有驱动
  有现成 sensor 驱动就先复用

第三步：先让 I2C probe 成功
  dmesg 能看到 sensor detected

第四步：确认 /dev/mediaX 出现
  media-ctl -p 能看到 sensor entity

第五步：确认 media graph 连接正确
  sensor -> csi -> isp -> video

第六步：确认 /dev/videoX 出现
  v4l2-ctl --list-devices

第七步：先用最低分辨率测试
  640x480 或 1280x720

第八步：再上目标分辨率和帧率
  1080p30 / 720p60 等

第九步：再调颜色、曝光、增益、ISP 参数
```

---

# 11. 对 MCU 开发者的类比

你以前写 MCU 外设，大概是：

```text
初始化 GPIO
初始化 I2C
配置寄存器
启动外设
中断/DMA 收数据
应用处理数据
```

Linux camera 可以类比成：

```text
设备树描述硬件
  ≈ MCU 里你手动写 GPIO/I2C/时钟初始化参数

sensor driver
  ≈ I2C 配置 camera 寄存器

CSI/DVP driver
  ≈ MCU 外设接收数据

DMA/videobuf2
  ≈ DMA 把数据搬到内存 buffer

V4L2
  ≈ 给应用程序提供统一 API

media controller
  ≈ 描述 sensor、CSI、ISP、video node 的连接关系
```

所以你不要只盯着 sensor 驱动。
Camera 出图要同时满足：

```text
sensor 配置对
设备树连接对
CSI 接收配置对
ISP 格式对
V4L2 buffer 流程对
```

---

# 12. 一个最小 sensor 驱动框架长什么样？

简化版逻辑如下：

```c
struct xxx_sensor {
    struct i2c_client *client;
    struct v4l2_subdev sd;
    struct media_pad pad;

    struct clk *xclk;
    struct gpio_desc *reset_gpio;
    struct gpio_desc *pwdn_gpio;

    struct regulator_bulk_data supplies[3];

    struct v4l2_ctrl_handler ctrl_handler;

    const struct xxx_mode *cur_mode;
};

static int xxx_power_on(struct xxx_sensor *sensor)
{
    /* 1. 打开电源 */
    /* 2. 打开 xclk */
    /* 3. 控制 reset/pwdn GPIO */
    /* 4. 延时等待 sensor 稳定 */

    return 0;
}

static int xxx_read_chip_id(struct xxx_sensor *sensor)
{
    /* 通过 I2C 读取 sensor ID 寄存器 */
    return 0;
}

static int xxx_start_stream(struct xxx_sensor *sensor)
{
    /* 1. 写初始化寄存器表 */
    /* 2. 写当前分辨率寄存器 */
    /* 3. 应用 exposure/gain/vblank 等参数 */
    /* 4. 写 stream on 寄存器 */

    return 0;
}

static int xxx_stop_stream(struct xxx_sensor *sensor)
{
    /* 写 stream off 寄存器 */
    return 0;
}

static int xxx_s_stream(struct v4l2_subdev *sd, int enable)
{
    struct xxx_sensor *sensor = container_of(sd, struct xxx_sensor, sd);

    if (enable)
        return xxx_start_stream(sensor);
    else
        return xxx_stop_stream(sensor);
}
```

真正项目里的驱动会更复杂，但是主线就是这些。

---

# 13. Camera 驱动调试优先级

建议你按这个顺序排查：

```text
1. 电源是否正常
2. MCLK/XCLK 是否正常
3. reset/pwdn 时序是否正常
4. I2C 是否能读 chip ID
5. sensor 是否 probe 成功
6. media graph 是否完整
7. /dev/videoX 是否生成
8. sensor 是否执行 s_stream(1)
9. MIPI/CSI 是否收到 frame start/frame end
10. DMA 是否有 buffer done
11. 图像格式是否匹配
12. ISP 参数是否正确
```

千万不要反过来，一上来就怀疑 ISP 算法。

---

# 14. 必须掌握的工具

## 1. `dmesg`

```bash
dmesg -w
```

看驱动日志。

---

## 2. `i2cdetect`

```bash
i2cdetect -y 1
```

看 I2C 总线上有没有设备。

注意：
有些 camera sensor 不一定适合直接被 `i2cdetect` 扫，有些设备在没上电、没时钟、没退出 reset 时不会响应。

---

## 3. `media-ctl`

```bash
media-ctl -p
```

看 camera pipeline。

---

## 4. `v4l2-ctl`

```bash
v4l2-ctl --list-devices
v4l2-ctl -d /dev/video0 --all
v4l2-ctl -d /dev/video0 --list-formats-ext
v4l2-ctl -d /dev/video0 --stream-mmap --stream-count=100
```

---

## 5. `gst-launch`

如果系统有 GStreamer，可以测试预览：

```bash
gst-launch-1.0 v4l2src device=/dev/video0 ! videoconvert ! autovideosink
```

嵌入式板子没桌面时，可以保存文件或推流。

---

# 15. 新手最需要注意的几个关键点

## 关键点 1：设备树比代码更容易错

尤其是：

```text
I2C 地址
reset GPIO 极性
pwdn GPIO 极性
MCLK 频率
MIPI data-lanes
remote-endpoint
link-frequencies
电源 supply 名字
pinctrl
```

---

## 关键点 2：Sensor 输出格式必须和 CSI/ISP 输入格式一致

比如 sensor 输出：

```text
RAW10 BGGR
```

CSI/ISP 却按：

```text
YUYV
```

那肯定不对。

---

## 关键点 3：RAW sensor 和 YUV sensor 完全不是一个难度

### YUV sensor

比如有些 OV5640 可以直接输出 YUV：

```text
sensor -> CSI/DVP -> memory -> YUYV
```

相对简单。

### RAW sensor

比如很多高清 sensor 输出 RAW10：

```text
sensor -> CSI -> ISP -> NV12/YUYV
```

这就需要 ISP 调参。

RAW 图像如果不经过 ISP，直接看通常是灰的、绿的、马赛克状的。

---

## 关键点 4：先低分辨率，再高分辨率

调试时不要一上来就 4MP、5MP、1080p60。

推荐先：

```text
640x480@30
1280x720@30
1920x1080@30
```

分辨率越高，对 MIPI、DDR、ISP、DMA 压力越大。

---

## 关键点 5：不要随便抄寄存器表

Camera sensor 寄存器表强依赖：

```text
输入 MCLK
输出分辨率
帧率
MIPI lane 数
bit depth
PLL 配置
曝光时序
水平/垂直 blanking
```

同一个 sensor，不同板子寄存器表可能不完全一样。

---

# 16. 最推荐你的学习路线

你现在是嵌入式软件新手阶段，可以按这个路线学：

```text
第一阶段：理解 camera 硬件链路
I2C 控制 + MIPI/DVP 图像传输

第二阶段：学 V4L2 基本使用
v4l2-ctl 打开设备、设置格式、采集 buffer

第三阶段：学 media controller
media-ctl -p 看 sensor/csi/isp/video 链路

第四阶段：看一个现成 sensor 驱动
比如 ov5640.c / imx219.c / gc2145.c

第五阶段：改设备树适配一个 sensor
让 probe 成功，media graph 出来

第六阶段：调 s_stream
确认 stream on 后 CSI 有数据

第七阶段：调格式和画面
解决颜色、花屏、帧率、曝光问题

第八阶段：再深入 ISP
3A、AWB、AE、AF、CCM、Gamma、降噪
```

---

# 17. 一句话总结

Linux camera 驱动的本质是：

```text
设备树描述硬件连接；
sensor driver 通过 I2C 配置摄像头；
CSI/DVP/ISP driver 接收和处理图像数据；
V4L2/media controller 把整条视频链路暴露给用户态；
v4l2-ctl/media-ctl/GStreamer 用来测试和采集。
```

对新手来说，最重要的不是马上写代码，而是先把这条链路看懂：

```text
sensor 读 ID
  -> subdev 注册
  -> media graph 连接
  -> /dev/videoX 出现
  -> stream on
  -> CSI 收到数据
  -> DMA buffer done
  -> 用户态拿到图像
```

[1]: https://docs.kernel.org/userspace-api/media/v4l/v4l2.html "Part I - Video for Linux API — The Linux Kernel  documentation"
[2]: https://wiki.st.com/stm32mpu/wiki/STM32MP13_V4L2_camera_overview "STM32MP13 V4L2 camera overview - stm32mpu"
[3]: https://www.infradead.org/~mchehab/kernel_docs/driver-api/media/csi2.html "6. MIPI CSI-2 — The Linux Kernel 5.10.0-rc1+ documentation"
[4]: https://www.kernel.org/doc/html/v4.14/media/kapi/v4l2-subdev.html?utm_source=chatgpt.com "1.7. V4L2 sub-devices — The Linux Kernel documentation"
[5]: https://android.googlesource.com/kernel/msm/%2B/android-wear-5.1.1_r0.6/Documentation/devicetree/bindings/media/video-interfaces.txt "Documentation/devicetree/bindings/media/video-interfaces.txt - kernel/msm - Git at Google"
[6]: https://docs.kernel.org/driver-api/media/v4l2-subdev.html "3.7. V4L2 sub-devices — The Linux Kernel  documentation"
[7]: https://www.kernel.org/doc/html/next/userspace-api/media/v4l/ext-ctrls-camera.html "1.11. Camera Control Reference — The Linux Kernel  documentation"
