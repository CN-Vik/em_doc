下面这次专门把 **Linux V4L2** 讲清楚。你可以先记住一句话：

```text
V4L2 = Linux 给“视频设备”提供的一套统一接口。
```

比如：

```text
USB 摄像头
MIPI CSI 摄像头
DVP 摄像头
HDMI 输入
TV 采集卡
视频解码器
视频编码器
ISP
```

这些设备在 Linux 里很多都会通过 V4L2 暴露给用户态。Linux 官方文档也明确说明 V4L2 是 “Video for Linux API version 2” 的规范。([Linux Kernel 文档][1])

---

# 1. V4L2 到底解决什么问题？

没有 V4L2 的话，每个摄像头、每个 SoC 厂商、每个采集设备都要自己定义一套接口：

```text
xxx_camera_open()
xxx_camera_set_resolution()
xxx_camera_start()
xxx_camera_get_frame()
```

这样应用程序就很难通用。

V4L2 的作用就是统一成这种方式：

```c
fd = open("/dev/video0", O_RDWR);

ioctl(fd, VIDIOC_QUERYCAP, ...);
ioctl(fd, VIDIOC_S_FMT, ...);
ioctl(fd, VIDIOC_REQBUFS, ...);
ioctl(fd, VIDIOC_QBUF, ...);
ioctl(fd, VIDIOC_STREAMON, ...);
ioctl(fd, VIDIOC_DQBUF, ...);
```

也就是说，应用程序不用太关心底层是 USB 摄像头、MIPI sensor、DVP sensor 还是 HDMI 输入。只要驱动按 V4L2 标准实现，用户态就能用统一方法采集图像。

---

# 2. V4L2 在 Linux camera 系统里的位置

典型 camera 系统是这样：

```text
应用程序
  │
  │ open /dev/video0
  │ ioctl / mmap / read / poll
  ▼
V4L2 用户态 API
  │
  ▼
V4L2 core
  │
  ├── video_device
  ├── v4l2_device
  ├── v4l2_subdev
  ├── v4l2_ctrl_handler
  ├── vb2_queue
  │
  ▼
SoC camera driver
  │
  ├── CSI / DVP 接收器
  ├── ISP
  ├── DMA
  │
  ▼
Camera sensor driver
  │
  ├── I2C 配置 sensor
  ├── reset / power / clock
  ├── exposure / gain / vblank
  │
  ▼
真实硬件
```

从用户态看，最直接的东西是：

```text
/dev/video0
/dev/video1
/dev/media0
/dev/v4l-subdev0
```

其中：

```text
/dev/videoX       真正采集图像的节点
/dev/mediaX       Media Controller 拓扑节点
/dev/v4l-subdevX  sensor、CSI、ISP 等子设备节点
```

Linux V4L2 驱动框架里面包含 `v4l2_device`、`video_device`、`v4l2_subdev`、controls、videobuf2、media controller、async、fwnode 等多个部分。官方 driver API 文档也把 V4L2 core、sub-device、controls、videobuf2、media controller、async、fwnode 等列为 V4L2 驱动框架的重要组成。([Linux Kernel Archives][2])

---

# 3. `/dev/video0` 是什么？

`/dev/video0` 是 V4L2 video device 节点。

你可以理解成：

```text
/dev/video0 = 应用程序拿图像帧的入口
```

比如：

```bash
v4l2-ctl -d /dev/video0 --all
```

这个命令就是通过 V4L2 ioctl 去问驱动：

```text
你是谁？
你支持什么格式？
当前分辨率是多少？
当前帧率是多少？
支持哪些 controls？
```

如果你的 camera 驱动完全没注册成功，就不会有 `/dev/video0`。

如果 sensor 驱动成功了，但 CSI/ISP/capture 驱动没成功，也可能只有 `/dev/v4l-subdevX`，没有 `/dev/video0`。

---

# 4. V4L2 的核心对象

## 4.1 `struct v4l2_device`

这是一个 V4L2 设备的总管理对象。

可以理解成：

```text
v4l2_device = 一个 V4L2 系统里的总管
```

它负责管理：

```text
video_device
v4l2_subdev
通知链
controls
```

在复杂 camera 里，一个 SoC camera 驱动通常会有一个 `v4l2_device`。

---

## 4.2 `struct video_device`

这个东西最终会注册成 `/dev/videoX`。

你可以理解成：

```text
video_device = 用户态能 open 的视频节点
```

驱动里通常会：

```c
struct video_device vdev;

video_register_device(&vdev, VFL_TYPE_VIDEO, -1);
```

注册成功后，用户态就会看到：

```text
/dev/video0
```

这个节点背后需要实现一组操作函数，比如：

```c
static const struct v4l2_file_operations xxx_fops = {
    .owner          = THIS_MODULE,
    .open           = v4l2_fh_open,
    .release        = vb2_fop_release,
    .unlocked_ioctl = video_ioctl2,
    .poll           = vb2_fop_poll,
    .mmap           = vb2_fop_mmap,
};
```

---

## 4.3 `struct v4l2_ioctl_ops`

这个非常关键。

用户态调用的很多 `ioctl()`，最后会进入驱动的 `v4l2_ioctl_ops`。

比如：

```c
static const struct v4l2_ioctl_ops xxx_ioctl_ops = {
    .vidioc_querycap      = xxx_querycap,
    .vidioc_enum_fmt_vid_cap = xxx_enum_fmt,
    .vidioc_g_fmt_vid_cap = xxx_g_fmt,
    .vidioc_s_fmt_vid_cap = xxx_s_fmt,
    .vidioc_reqbufs       = vb2_ioctl_reqbufs,
    .vidioc_querybuf      = vb2_ioctl_querybuf,
    .vidioc_qbuf          = vb2_ioctl_qbuf,
    .vidioc_dqbuf         = vb2_ioctl_dqbuf,
    .vidioc_streamon      = vb2_ioctl_streamon,
    .vidioc_streamoff     = vb2_ioctl_streamoff,
};
```

用户态：

```c
ioctl(fd, VIDIOC_S_FMT, &fmt);
```

内核态：

```c
xxx_s_fmt()
```

用户态：

```c
ioctl(fd, VIDIOC_STREAMON, &type);
```

内核态：

```c
vb2_ioctl_streamon()
    -> vb2 core
        -> driver start_streaming()
```

---

## 4.4 `struct v4l2_subdev`

这个对嵌入式 camera 特别重要。

`subdev` 的意思是子设备。比如：

```text
camera sensor
MIPI CSI receiver
ISP
video decoder
HDMI receiver
```

都可以是 `v4l2_subdev`。

典型 camera 链路：

```text
sensor subdev
    ↓
mipi csi subdev
    ↓
isp subdev
    ↓
video node /dev/video0
```

官方文档里专门有 V4L2 sub-device API，说明 subdev 有自己的状态、pad、format 等机制。([Linux Kernel 文档][3])

你之前问 camera 驱动时，我说：

```text
sensor driver 不一定直接生成 /dev/video0
```

原因就在这里。很多 sensor 驱动只是一个 `v4l2_subdev`，它本身负责通过 I2C 配置 sensor，但是**不负责 DMA，不负责生成 /dev/video0**。

---

## 4.5 `struct media_entity` / `media_pad` / `media_link`

这几个属于 Media Controller。

复杂 camera 里，V4L2 不只是一个 `/dev/video0`，还需要描述整条链路：

```text
sensor -> csi -> isp -> scaler -> capture
```

Media Controller 用三个概念描述它：

```text
entity = 一个硬件模块
pad    = 这个模块的输入口/输出口
link   = 两个 pad 之间的连接
```

比如：

```text
entity: ov5640 sensor
  pad0: source

entity: csi receiver
  pad0: sink
  pad1: source

entity: isp
  pad0: sink
  pad1: source

entity: capture
  pad0: sink
```

连接关系：

```text
ov5640:pad0 -> csi:pad0
csi:pad1    -> isp:pad0
isp:pad1    -> capture:pad0
```

用户态可以用：

```bash
media-ctl -p
```

查看这张图。Media Controller 的用户态 API 文档就是用来描述媒体设备模型、graph 元素和 request API 的。([Linux Kernel 文档][4])

---

# 5. V4L2 用户态采集一帧图像的流程

这是你一定要掌握的。

典型 mmap 采集流程：

```text
open
  ↓
VIDIOC_QUERYCAP
  ↓
VIDIOC_ENUM_FMT
  ↓
VIDIOC_S_FMT
  ↓
VIDIOC_REQBUFS
  ↓
VIDIOC_QUERYBUF
  ↓
mmap
  ↓
VIDIOC_QBUF
  ↓
VIDIOC_STREAMON
  ↓
poll/select
  ↓
VIDIOC_DQBUF
  ↓
处理图像
  ↓
VIDIOC_QBUF
  ↓
循环
  ↓
VIDIOC_STREAMOFF
```

Linux V4L2 userspace API 文档里把输入输出方式分成 read/write、Streaming I/O mmap、user pointer、DMA buffer importing，并且把 buffers、`VIDIOC_REQBUFS`、`VIDIOC_QBUF`、`VIDIOC_DQBUF`、`VIDIOC_STREAMON`、`VIDIOC_STREAMOFF` 等 ioctl 都作为标准接口列出。([Linux Kernel 文档][1])

---

# 6. 每一步到底在干什么？

## 6.1 `open("/dev/video0")`

打开视频设备。

```c
int fd = open("/dev/video0", O_RDWR);
```

底层进入驱动：

```c
xxx_open()
```

驱动可能会做：

```text
初始化 file handle
打开 runtime PM
准备 vb2 queue
检查设备状态
```

---

## 6.2 `VIDIOC_QUERYCAP`

查询设备能力。

```c
struct v4l2_capability cap;
ioctl(fd, VIDIOC_QUERYCAP, &cap);
```

它会告诉你：

```text
driver name
card name
bus info
是否支持 video capture
是否支持 streaming
是否支持 read/write
```

V4L2 API 文档把 `VIDIOC_QUERYCAP` 作为查询能力的标准 ioctl。([Linux Kernel 文档][1])

应用一般会检查：

```c
if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
    printf("not video capture device\n");
}

if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
    printf("not support streaming io\n");
}
```

---

## 6.3 `VIDIOC_ENUM_FMT`

枚举支持的像素格式。

```c
struct v4l2_fmtdesc fmtdesc;
fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

for (int i = 0; ; i++) {
    fmtdesc.index = i;
    if (ioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc) < 0)
        break;

    printf("format: %s\n", fmtdesc.description);
}
```

可能看到：

```text
YUYV
MJPEG
NV12
RGB565
BA10
RG10
```

这些是**用户态看到的 video node 输出格式**。

注意：这不一定等于 sensor 原始输出格式。

比如：

```text
sensor 输出 RAW10
    ↓
ISP 处理
    ↓
/dev/video0 输出 NV12
```

这时候 `/dev/video0` 可能看到的是 NV12，而 sensor subdev 上是 RAW10。

---

## 6.4 `VIDIOC_S_FMT`

设置图像格式。

```c
struct v4l2_format fmt;

fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
fmt.fmt.pix.width = 640;
fmt.fmt.pix.height = 480;
fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
fmt.fmt.pix.field = V4L2_FIELD_NONE;

ioctl(fd, VIDIOC_S_FMT, &fmt);
```

注意，驱动不一定完全接受你的设置。

比如你请求：

```text
641 x 479
```

驱动可能返回：

```text
640 x 480
```

所以 `S_FMT` 后要看返回值里的实际格式：

```c
printf("actual width=%d height=%d\n",
       fmt.fmt.pix.width,
       fmt.fmt.pix.height);
```

---

## 6.5 `VIDIOC_REQBUFS`

申请 buffer。

```c
struct v4l2_requestbuffers req;

req.count = 4;
req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
req.memory = V4L2_MEMORY_MMAP;

ioctl(fd, VIDIOC_REQBUFS, &req);
```

意思是：

```text
我要 4 个 mmap 类型的采集 buffer
```

为什么需要多个 buffer？

因为 camera 是连续出图的：

```text
硬件正在写 buffer0
应用正在处理 buffer1
buffer2 等待硬件使用
buffer3 等待应用取走
```

如果只有一个 buffer，很容易卡顿或丢帧。

---

## 6.6 `VIDIOC_QUERYBUF` + `mmap`

查询每个 buffer 的偏移和大小，然后映射到用户态。

```c
struct v4l2_buffer buf;

buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
buf.memory = V4L2_MEMORY_MMAP;
buf.index = i;

ioctl(fd, VIDIOC_QUERYBUF, &buf);

buffers[i].start = mmap(NULL,
                        buf.length,
                        PROT_READ | PROT_WRITE,
                        MAP_SHARED,
                        fd,
                        buf.m.offset);
```

这一步非常关键。

它不是 malloc 普通内存，而是把驱动分配的 DMA buffer 映射到用户态。这样 camera DMA 写进去的数据，应用程序可以直接读。

---

## 6.7 `VIDIOC_QBUF`

把空 buffer 放进驱动队列。

```c
ioctl(fd, VIDIOC_QBUF, &buf);
```

你可以理解成：

```text
应用：这个 buffer 给你，硬件可以往里面写图像。
```

一开始通常要把所有 buffer 都 QBUF：

```text
buffer0 -> queued
buffer1 -> queued
buffer2 -> queued
buffer3 -> queued
```

---

## 6.8 `VIDIOC_STREAMON`

开始采集。

```c
enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
ioctl(fd, VIDIOC_STREAMON, &type);
```

这一步之后，驱动内部一般会：

```text
启动 sensor stream
启动 CSI receiver
启动 ISP
启动 DMA
打开中断
等待 frame start / frame end
```

如果是嵌入式 camera，最终可能会调用到 sensor driver 的：

```c
s_stream(sd, 1);
```

也就是：

```text
sensor 开始吐图像数据
```

---

## 6.9 `poll()` / `select()`

等待一帧完成。

```c
poll(&fds, 1, 2000);
```

这相当于：

```text
应用：有采集完成的 buffer 了吗？
```

如果超时：

```text
select timeout
```

常见原因：

```text
sensor 没出图
CSI 没收到数据
DMA 没完成
中断没来
MIPI lane 配错
format 配错
```

---

## 6.10 `VIDIOC_DQBUF`

取出一帧完成的 buffer。

```c
ioctl(fd, VIDIOC_DQBUF, &buf);
```

这时候：

```text
buf.index       表示哪个 buffer 完成了
buf.bytesused   表示有效数据大小
buf.timestamp   表示时间戳
```

应用就可以处理：

```c
process(buffers[buf.index].start, buf.bytesused);
```

比如保存成文件：

```c
fwrite(buffers[buf.index].start, buf.bytesused, 1, fp);
```

---

## 6.11 再次 `VIDIOC_QBUF`

处理完以后，要把 buffer 还给驱动：

```c
ioctl(fd, VIDIOC_QBUF, &buf);
```

否则 buffer 会越来越少，最后采集停住。

这一步很像 FreeRTOS queue 里的“归还 buffer”。

---

# 7. V4L2 的 buffer 模型

V4L2 支持几种 buffer 方式：

```text
read/write
mmap
userptr
dmabuf
```

## 7.1 read/write

最简单：

```c
read(fd, buf, size);
```

优点：

```text
简单
适合快速验证
```

缺点：

```text
效率低
可能多一次拷贝
复杂 camera 不一定支持
```

---

## 7.2 mmap

最常用。

```text
驱动分配 DMA buffer
用户态 mmap 映射
硬件 DMA 写 buffer
用户态直接读 buffer
```

优点：

```text
效率高
通用
v4l2-ctl 默认常用
```

---

## 7.3 userptr

用户态自己分配 buffer，把地址传给驱动。

```text
应用 malloc 或 posix_memalign
驱动使用用户态 buffer
```

这个用得相对少，因为 DMA 对地址连续性/cache 一致性有要求。

---

## 7.4 dmabuf

高级方式。

适合零拷贝图像管线：

```text
camera -> GPU
camera -> DRM display
camera -> encoder
camera -> NPU
```

DMABUF 可以把 buffer 在多个设备之间共享。

比如：

```text
V4L2 camera 采集 NV12
    ↓ dmabuf fd
DRM/KMS 直接显示
```

这样可以避免 CPU 拷贝。

---

# 8. 单平面和多平面格式

V4L2 里有两套接口：

```text
single-planar API
multi-planar API
```

## 8.1 单平面

比如 YUYV：

```text
YUYVYUYVYUYV...
```

一个 buffer 里就是完整图像。

对应：

```c
V4L2_BUF_TYPE_VIDEO_CAPTURE
struct v4l2_pix_format
```

---

## 8.2 多平面

比如 NV12：

```text
Y plane
UV plane
```

有些驱动使用 multi-planar：

```c
V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE
struct v4l2_pix_format_mplane
```

新手常见问题：

```text
驱动是 MPLANE，你却用普通 VIDEO_CAPTURE；
或者驱动是普通单平面，你却用 VIDEO_CAPTURE_MPLANE。
```

表现可能是：

```text
VIDIOC_S_FMT 失败
VIDIOC_REQBUFS 失败
v4l2-ctl 参数不对
```

---

# 9. 常见像素格式解释

## 9.1 YUYV / UYVY

这是 YUV422 packed 格式。

```text
YUYV:
Y0 U0 Y1 V0

UYVY:
U0 Y0 V0 Y1
```

两者数据内容类似，但是字节顺序不同。

如果顺序错了：

```text
颜色异常
发绿
发紫
人脸颜色怪
```

---

## 9.2 NV12 / NV21

这是 YUV420 semi-planar。

```text
NV12:
Y plane + UV plane

NV21:
Y plane + VU plane
```

NV12 很常见，ISP、编码器、显示管线都喜欢用。

如果 UV/VU 搞反：

```text
红蓝反
肤色异常
```

---

## 9.3 RGB565 / RGB888

显示屏常用。

```text
RGB565:
R 5bit
G 6bit
B 5bit
```

camera 直接输出 RGB 的情况相对少，更多是 YUV 或 RAW。

---

## 9.4 RAW Bayer

比如：

```text
RGGB
BGGR
GRBG
GBRG
RAW8
RAW10
RAW12
```

RAW Bayer 是 sensor 原始数据。它不是直接能正常显示的 RGB 图。

必须经过：

```text
黑电平校正
坏点修复
去马赛克 demosaic
白平衡 AWB
颜色校正 CCM
Gamma
降噪
锐化
YUV 转换
```

这就是 ISP 干的事情。

---

# 10. V4L2 Controls 是什么？

V4L2 controls 是控制项。

比如 camera 上常见：

```text
brightness
contrast
saturation
hue
exposure
gain
white balance
focus
zoom
test pattern
horizontal flip
vertical flip
```

用户态查看：

```bash
v4l2-ctl -d /dev/video0 --list-ctrls
```

设置曝光：

```bash
v4l2-ctl -d /dev/video0 --set-ctrl=exposure=100
```

设置增益：

```bash
v4l2-ctl -d /dev/video0 --set-ctrl=analogue_gain=64
```

在 sensor 驱动里，一般用：

```c
struct v4l2_ctrl_handler ctrl_handler;

v4l2_ctrl_new_std(&ctrl_handler,
                  &xxx_ctrl_ops,
                  V4L2_CID_EXPOSURE,
                  min, max, step, default_value);
```

control 改变时，会进入：

```c
static int xxx_set_ctrl(struct v4l2_ctrl *ctrl)
{
    switch (ctrl->id) {
    case V4L2_CID_EXPOSURE:
        // I2C 写 sensor 曝光寄存器
        break;

    case V4L2_CID_ANALOGUE_GAIN:
        // I2C 写 sensor 增益寄存器
        break;
    }
}
```

所以你可以理解成：

```text
V4L2 control = 用户态调 camera 参数的统一入口
```

---

# 11. V4L2 subdev 和 video node 的区别

这个非常重要。

## 11.1 `v4l2_subdev`

代表链路中的一个模块：

```text
sensor
csi
isp
scaler
decoder
```

它通常通过：

```text
/dev/v4l-subdev0
/dev/v4l-subdev1
```

暴露，或者只在内核内部使用。

sensor subdev 常见操作：

```text
设置 mbus code
设置 frame size
设置 frame interval
设置 controls
s_stream 开流/关流
```

---

## 11.2 `video_device`

代表最终采集节点：

```text
/dev/video0
```

它负责：

```text
buffer 管理
DMA 队列
给用户态输出图像帧
```

---

## 11.3 二者关系

例如：

```text
ov5640 sensor subdev
    ↓
mipi csi subdev
    ↓
isp subdev
    ↓
capture video node /dev/video0
```

用户态从 `/dev/video0` 拿帧，但 `/dev/video0` 的数据来自前面多个 subdev 组成的 pipeline。

---

# 12. Media bus format 和 pixel format 的区别

这个也是新手很容易混的地方。

## 12.1 media bus format

media bus format 是模块之间传输的数据格式。

比如 sensor 到 CSI：

```text
MEDIA_BUS_FMT_SBGGR10_1X10
MEDIA_BUS_FMT_SRGGB10_1X10
MEDIA_BUS_FMT_UYVY8_2X8
```

它描述的是：

```text
sensor 输出到 MIPI/DVP 总线上的格式
```

---

## 12.2 V4L2 pixel format

V4L2 pixel format 是 `/dev/video0` 给用户态的格式。

比如：

```text
V4L2_PIX_FMT_YUYV
V4L2_PIX_FMT_NV12
V4L2_PIX_FMT_RGB565
V4L2_PIX_FMT_SBGGR10
```

---

## 12.3 举例

一个 RAW sensor：

```text
sensor subdev:
    MEDIA_BUS_FMT_SBGGR10_1X10

ISP input:
    MEDIA_BUS_FMT_SBGGR10_1X10

ISP output / video node:
    V4L2_PIX_FMT_NV12
```

所以：

```text
sensor 的 mbus_code 不等于 /dev/video0 的 pixelformat
```

---

# 13. videobuf2，简称 VB2

VB2 是 V4L2 里专门管理 buffer 队列的框架。

你可以把它理解成：

```text
VB2 = V4L2 的 DMA buffer 队列管理器
```

它负责：

```text
申请 buffer
释放 buffer
mmap buffer
QBUF
DQBUF
STREAMON
STREAMOFF
poll
buffer 状态切换
```

典型 buffer 状态：

```text
DEQUEUED
    ↓ QBUF
QUEUED
    ↓ 驱动拿去给 DMA
ACTIVE
    ↓ 一帧完成
DONE
    ↓ DQBUF
DEQUEUED
```

驱动只需要实现一些关键回调：

```c
static const struct vb2_ops xxx_vb2_ops = {
    .queue_setup     = xxx_queue_setup,
    .buf_prepare     = xxx_buf_prepare,
    .buf_queue       = xxx_buf_queue,
    .start_streaming = xxx_start_streaming,
    .stop_streaming  = xxx_stop_streaming,
};
```

## 13.1 `queue_setup`

告诉 VB2：

```text
需要几个 plane？
每个 buffer 多大？
最少几个 buffer？
```

比如 640x480 YUYV：

```text
size = 640 * 480 * 2
```

---

## 13.2 `buf_prepare`

检查 buffer 是否够大。

```text
如果用户申请的 buffer 太小，返回错误。
```

---

## 13.3 `buf_queue`

用户态 QBUF 后，buffer 会进入这个函数。

驱动通常会：

```text
把 buffer 挂到自己的 pending list
等待 DMA 使用
```

---

## 13.4 `start_streaming`

用户态 STREAMON 后，进入这里。

驱动通常会：

```text
配置 DMA
启动 CSI
启动 ISP
调用 sensor s_stream(1)
```

---

## 13.5 `stop_streaming`

用户态 STREAMOFF 后，进入这里。

驱动通常会：

```text
停止 DMA
关闭中断
调用 sensor s_stream(0)
把所有未完成 buffer 返回给 VB2
```

---

# 14. 一帧图像从硬件到应用的完整过程

以 MIPI camera 为例：

```text
1. 应用 VIDIOC_STREAMON

2. V4L2 core 调用 vb2 streamon

3. vb2 调用驱动 start_streaming()

4. 驱动启动 pipeline：
   sensor s_stream(1)
   CSI enable
   ISP enable
   DMA enable

5. sensor 通过 MIPI CSI-2 输出图像

6. CSI receiver 收到 frame start

7. DMA 把数据写入某个 vb2 buffer

8. frame end 中断到来

9. 驱动在 ISR 或 tasklet/workqueue 里：
   vb2_buffer_done(buf, VB2_BUF_STATE_DONE)

10. 应用 poll 返回可读

11. 应用 VIDIOC_DQBUF 拿到 buffer

12. 应用处理图像

13. 应用 VIDIOC_QBUF 归还 buffer

14. 下一帧继续
```

这个流程你一定要理解。
以后你看到采集卡住，就知道该查哪里：

```text
有没有 STREAMON？
有没有调用 sensor s_stream？
CSI 有没有 frame start？
DMA 有没有完成中断？
有没有 vb2_buffer_done？
应用有没有 DQBUF？
应用有没有重新 QBUF？
```

---

# 15. `v4l2-ctl` 常用命令

## 15.1 查看设备

```bash
v4l2-ctl --list-devices
```

---

## 15.2 查看设备所有信息

```bash
v4l2-ctl -d /dev/video0 --all
```

---

## 15.3 查看支持格式

```bash
v4l2-ctl -d /dev/video0 --list-formats-ext
```

---

## 15.4 查看 controls

```bash
v4l2-ctl -d /dev/video0 --list-ctrls
```

---

## 15.5 设置格式

```bash
v4l2-ctl -d /dev/video0 \
  --set-fmt-video=width=640,height=480,pixelformat=YUYV
```

---

## 15.6 采集 100 帧到空设备

```bash
v4l2-ctl -d /dev/video0 \
  --stream-mmap \
  --stream-count=100 \
  --stream-to=/dev/null
```

这个很适合测试驱动是否稳定。

---

## 15.7 采集一帧到文件

```bash
v4l2-ctl -d /dev/video0 \
  --set-fmt-video=width=640,height=480,pixelformat=YUYV \
  --stream-mmap \
  --stream-count=1 \
  --stream-to=frame.yuv
```

---

## 15.8 RAW10 采集示例

具体 pixelformat 要看驱动支持什么：

```bash
v4l2-ctl -d /dev/video0 \
  --set-fmt-video=width=1920,height=1080,pixelformat=RG10 \
  --stream-mmap \
  --stream-count=1 \
  --stream-to=frame.raw
```

---

# 16. `media-ctl` 常用命令

## 16.1 查看 media graph

```bash
media-ctl -p -d /dev/media0
```

你想看到类似：

```text
entity 1: sensor
  pad0: Source
    -> "csi":0 [ENABLED]

entity 2: csi
  pad0: Sink
  pad1: Source
    -> "isp":0 [ENABLED]

entity 3: isp
  pad0: Sink
  pad1: Source
    -> "capture":0 [ENABLED]

entity 4: capture
  pad0: Sink
  device node name /dev/video0
```

---

## 16.2 设置 subdev 格式

例如设置 sensor 输出：

```bash
media-ctl -d /dev/media0 \
  --set-v4l2 '"ov5640 1-003c":0[fmt:UYVY8_2X8/640x480]'
```

设置 CSI sink：

```bash
media-ctl -d /dev/media0 \
  --set-v4l2 '"csi":0[fmt:UYVY8_2X8/640x480]'
```

这几个必须匹配：

```text
sensor source pad
csi sink pad
csi source pad
isp sink pad
```

否则链路格式不一致，可能 STREAMON 失败或者无图。

---

# 17. 新手最容易混淆的几个点

## 17.1 `/dev/video0` 不等于 sensor

`/dev/video0` 通常是最终 capture 节点。

sensor 常常是：

```text
/dev/v4l-subdev0
```

所以你读到 sensor ID 成功，不代表 `/dev/video0` 就一定正常。

---

## 17.2 I2C 成功不等于 V4L2 成功

I2C 只是控制通道：

```text
读 chip id
写寄存器
设置曝光
设置 stream on
```

图像数据通道是：

```text
MIPI CSI-2
DVP
BT.656
LVDS
```

所以：

```text
I2C OK
≠ sensor 正在输出图像
≠ CSI 收到图像
≠ DMA 有 buffer done
```

---

## 17.3 `/dev/video0` 出现不等于能采集

`/dev/video0` 出现只能说明 video device 注册成功。

采集成功还要满足：

```text
media graph 连接正确
格式协商正确
sensor stream on
CSI 正常接收
DMA 正常完成
buffer 正常返回
```

---

## 17.4 `S_FMT` 设置的不一定就是实际格式

应用请求：

```text
1920x1080 YUYV
```

驱动可能调整成：

```text
1280x720 NV12
```

所以应用必须检查 `VIDIOC_S_FMT` 返回后的 `fmt`。

---

## 17.5 RAW 图像不能直接当 RGB 看

RAW10 Bayer 文件直接打开通常会像：

```text
灰色
绿色
花纹
马赛克
很暗
```

不是坏了，而是还没 ISP 处理。

---

# 18. 一个最小 V4L2 采集程序框架

下面是简化版，主要看流程：

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>

#define BUFFER_COUNT 4

struct buffer {
    void *start;
    size_t length;
};

int main(void)
{
    int fd;
    struct v4l2_capability cap;
    struct v4l2_format fmt;
    struct v4l2_requestbuffers req;
    struct buffer buffers[BUFFER_COUNT];

    fd = open("/dev/video0", O_RDWR);
    if (fd < 0) {
        perror("open");
        return -1;
    }

    /*
     * 1. 查询设备能力
     */
    if (ioctl(fd, VIDIOC_QUERYCAP, &cap) < 0) {
        perror("VIDIOC_QUERYCAP");
        return -1;
    }

    printf("driver: %s\n", cap.driver);
    printf("card:   %s\n", cap.card);

    /*
     * 2. 设置采集格式
     */
    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = 640;
    fmt.fmt.pix.height = 480;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;

    if (ioctl(fd, VIDIOC_S_FMT, &fmt) < 0) {
        perror("VIDIOC_S_FMT");
        return -1;
    }

    printf("actual: %d x %d\n",
           fmt.fmt.pix.width,
           fmt.fmt.pix.height);

    /*
     * 3. 请求 buffer
     */
    memset(&req, 0, sizeof(req));
    req.count = BUFFER_COUNT;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (ioctl(fd, VIDIOC_REQBUFS, &req) < 0) {
        perror("VIDIOC_REQBUFS");
        return -1;
    }

    /*
     * 4. mmap buffer
     */
    for (int i = 0; i < req.count; i++) {
        struct v4l2_buffer buf;

        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;

        if (ioctl(fd, VIDIOC_QUERYBUF, &buf) < 0) {
            perror("VIDIOC_QUERYBUF");
            return -1;
        }

        buffers[i].length = buf.length;
        buffers[i].start = mmap(NULL,
                                buf.length,
                                PROT_READ | PROT_WRITE,
                                MAP_SHARED,
                                fd,
                                buf.m.offset);

        if (buffers[i].start == MAP_FAILED) {
            perror("mmap");
            return -1;
        }
    }

    /*
     * 5. 把 buffer 入队
     */
    for (int i = 0; i < req.count; i++) {
        struct v4l2_buffer buf;

        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;

        if (ioctl(fd, VIDIOC_QBUF, &buf) < 0) {
            perror("VIDIOC_QBUF");
            return -1;
        }
    }

    /*
     * 6. 开始采集
     */
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (ioctl(fd, VIDIOC_STREAMON, &type) < 0) {
        perror("VIDIOC_STREAMON");
        return -1;
    }

    /*
     * 7. 取一帧
     */
    struct v4l2_buffer buf;
    memset(&buf, 0, sizeof(buf));

    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;

    if (ioctl(fd, VIDIOC_DQBUF, &buf) < 0) {
        perror("VIDIOC_DQBUF");
        return -1;
    }

    printf("got frame, index=%d, bytesused=%d\n",
           buf.index,
           buf.bytesused);

    FILE *fp = fopen("frame.yuv", "wb");
    fwrite(buffers[buf.index].start, buf.bytesused, 1, fp);
    fclose(fp);

    /*
     * 8. 归还 buffer
     */
    ioctl(fd, VIDIOC_QBUF, &buf);

    /*
     * 9. 停止采集
     */
    ioctl(fd, VIDIOC_STREAMOFF, &type);

    for (int i = 0; i < req.count; i++) {
        munmap(buffers[i].start, buffers[i].length);
    }

    close(fd);

    return 0;
}
```

编译：

```bash
gcc v4l2_capture.c -o v4l2_capture
```

板子上交叉编译时换成你的交叉编译器。

---

# 19. 驱动侧最小框架怎么理解？

一个 V4L2 capture 驱动大概分成这些部分：

```text
probe()
  ↓
解析设备树
  ↓
初始化硬件资源
  ↓
初始化 v4l2_device
  ↓
初始化 video_device
  ↓
初始化 vb2_queue
  ↓
注册 /dev/videoX
```

核心代码结构：

```c
static int xxx_probe(struct platform_device *pdev)
{
    struct xxx_dev *dev;

    dev = devm_kzalloc(...);

    /*
     * 1. 获取 clock/reset/gpio/irq/regmap 等资源
     */

    /*
     * 2. 注册 v4l2_device
     */
    v4l2_device_register(&pdev->dev, &dev->v4l2_dev);

    /*
     * 3. 初始化 vb2 queue
     */
    dev->queue.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    dev->queue.io_modes = VB2_MMAP | VB2_DMABUF | VB2_READ;
    dev->queue.ops = &xxx_vb2_ops;
    dev->queue.mem_ops = &vb2_dma_contig_memops;
    dev->queue.drv_priv = dev;
    vb2_queue_init(&dev->queue);

    /*
     * 4. 初始化 video_device
     */
    strscpy(dev->vdev.name, "xxx-camera", sizeof(dev->vdev.name));
    dev->vdev.v4l2_dev = &dev->v4l2_dev;
    dev->vdev.fops = &xxx_fops;
    dev->vdev.ioctl_ops = &xxx_ioctl_ops;
    dev->vdev.queue = &dev->queue;
    video_set_drvdata(&dev->vdev, dev);

    /*
     * 5. 注册 /dev/videoX
     */
    video_register_device(&dev->vdev, VFL_TYPE_VIDEO, -1);

    return 0;
}
```

---

# 20. V4L2 和 libcamera 的关系

现在很多复杂 camera 系统，尤其是 RAW sensor + ISP，会引入 `libcamera`。

关系大概是：

```text
应用
  ↓
libcamera
  ↓
V4L2 / Media Controller / subdev
  ↓
kernel camera drivers
```

V4L2 是内核接口。
libcamera 是用户态 camera 框架，负责更复杂的 pipeline 管理、ISP 参数、3A 算法等。

对新手来说，不要一开始就冲 libcamera。建议先掌握：

```text
v4l2-ctl
media-ctl
/dev/video0
/dev/media0
V4L2 buffer 流程
subdev 格式协商
```

然后再看 libcamera。

---

# 21. 调试 V4L2 的标准路线

以后你调 camera，可以按这个顺序：

```bash
dmesg | grep -i -E "v4l2|video|camera|csi|isp|sensor"
```

看驱动有没有 probe。

```bash
ls /dev/video*
ls /dev/media*
ls /dev/v4l-subdev*
```

看节点有没有出来。

```bash
media-ctl -p -d /dev/media0
```

看链路有没有连起来。

```bash
v4l2-ctl --list-devices
```

看 video node 属于哪个设备。

```bash
v4l2-ctl -d /dev/video0 --all
```

看当前配置。

```bash
v4l2-ctl -d /dev/video0 --list-formats-ext
```

看支持格式。

```bash
v4l2-ctl -d /dev/video0 --stream-mmap --stream-count=100 --stream-to=/dev/null
```

看是否能稳定采集。

如果卡住，看：

```text
VIDIOC_STREAMON 是否成功？
sensor s_stream(1) 是否执行？
CSI 是否有中断？
DMA 是否有完成？
vb2_buffer_done 是否调用？
应用是否 DQBUF？
```

---

# 22. 用 MCU 思维类比 V4L2

你以前做 MCU 可能是：

```text
GPIO 初始化
I2C 初始化
DMA 初始化
中断初始化
启动采集
DMA 完成中断
处理 buffer
```

Linux V4L2 对应：

```text
设备树
    ≈ 板级硬件描述

sensor subdev
    ≈ I2C 配置外部 sensor

CSI/DVP driver
    ≈ MCU 的图像接收外设

VB2
    ≈ DMA buffer 队列

/dev/video0
    ≈ 应用拿数据的统一接口

v4l2-ctl
    ≈ 调试工具
```

核心变化是：

```text
MCU：你自己控制所有流程
Linux：驱动按 V4L2 框架注册，用户态通过标准 ioctl 控制
```

---

# 23. 新手学习 V4L2 的顺序

我建议你按这个路线：

```text
第一步：会用 v4l2-ctl 采集
第二步：理解 open/ioctl/mmap/QBUF/DQBUF/STREAMON
第三步：理解 /dev/videoX 和 /dev/v4l-subdevX 的区别
第四步：理解 media-ctl -p 看到的 graph
第五步：理解 pixel format 和 media bus format
第六步：看一个简单 USB camera V4L2 采集程序
第七步：看一个 SoC CSI capture driver
第八步：看一个 sensor subdev driver
第九步：看 vb2_queue 和 vb2_ops
第十步：再研究 ISP/libcamera
```

---

# 24. 一句话总结

V4L2 可以分成三层理解：

```text
用户态：
    /dev/video0
    ioctl
    mmap
    QBUF / DQBUF
    v4l2-ctl

驱动框架：
    v4l2_device
    video_device
    v4l2_ioctl_ops
    v4l2_subdev
    v4l2_ctrl
    vb2_queue

硬件链路：
    sensor
    MIPI CSI / DVP
    ISP
    DMA
    memory buffer
```

最关键的一条采集主线是：

```text
应用申请 buffer
    ↓
QBUF 交给驱动
    ↓
STREAMON 启动硬件
    ↓
DMA 写入 buffer
    ↓
驱动 vb2_buffer_done
    ↓
应用 DQBUF 拿到一帧
    ↓
应用 QBUF 归还 buffer
```

只要你把这条主线理解了，V4L2 就不再神秘了。

[1]: https://docs.kernel.org/userspace-api/media/v4l/v4l2.html "Part I - Video for Linux API — The Linux Kernel  documentation"
[2]: https://www.kernel.org/doc/html/v6.1/driver-api/media/v4l2-core.html "2. Video4Linux devices — The Linux Kernel  documentation"
[3]: https://docs.kernel.org/driver-api/media/v4l2-subdev.html?utm_source=chatgpt.com "3.7. V4L2 sub-devices"
[4]: https://docs.kernel.org/userspace-api/media/mediactl/media-controller.html?utm_source=chatgpt.com "Part IV - Media Controller API"


-------
-------
-------


你这个问题问到关键点了。结合 **SC3335 这种 MIPI image sensor** 来看，Linux 里 camera 大概分成三块：

```text
SC3335 sensor 驱动
    负责 I2C 配置 sensor 寄存器、开流、关流、曝光、增益

MIPI CSI-2 / D-PHY 接收驱动
    负责 SoC 侧接收 MIPI 高速图像数据

ISP / Video capture 驱动
    负责 RAW 图像处理、DMA 到内存、生成 /dev/videoX
```

先说结论：

**搞 SC3335 这种 MIPI 彩色 sensor，通常需要配置 Linux 的 MIPI CSI 接收链路，但“调颜色、调画质”主要不是调 MIPI，而是调 sensor 寄存器和 ISP 参数。**

---

# 1. 先纠正一个概念：SC3335 的 “RGB sensor” 不一定是直接输出 RGB888

你说的 **RGB image sensor**，大概率意思是：
它是一个**彩色 CMOS 图像传感器**。

但是这不等于它从 MIPI 线上直接输出：

```text
RGB888
RGB565
```

很多安防类 MIPI sensor，比如 SmartSens、OmniVision、Sony 的很多型号，本质输出的是：

```text
RAW Bayer
```

例如：

```text
RAW10 BGGR
RAW10 RGGB
RAW12 BGGR
```

它只是 sensor 像素上有彩色滤光片，最终要经过 ISP 处理，才会变成人眼正常看的 RGB/YUV 图像。

SC3335 模组资料里能看到它是 3MP、2312 x 1304、1/2.8 英寸、2.45 µm 像素，并且接口支持 MIPI 和 DVP Parallel。这个资料能证明它是一个 MIPI/DVP camera module，但不能简单理解成 Linux 端直接拿 RGB888。([kailaptech.en.made-in-china.com][1])

所以你要先确认你的 SC3335 输出格式：

```text
SC3335 输出 RAW10？
SC3335 输出 RAW12？
SC3335 输出 YUV422？
DVP 模式还是 MIPI 模式？
几 lane？
多少 fps？
Bayer 顺序是 BGGR / RGGB / GRBG / GBRG？
```

对嵌入式 Linux 来说，最常见是：

```text
SC3335
  ↓ MIPI CSI-2 RAW10
SoC MIPI CSI RX
  ↓ RAW10
ISP
  ↓ NV12 / YUYV / RGB
/dev/video0
```

---

# 2. Linux 里的 MIPI 是怎么驱动的？

Linux 里一般没有一个单独叫“通用 MIPI camera 驱动”的东西，而是分成几层。

## 2.1 Sensor 驱动

比如：

```text
drivers/media/i2c/sc3335.c
```

或者厂商 SDK 里可能是：

```text
kernel/drivers/media/i2c/sc3335.c
kernel/drivers/media/platform/xxx/sensor/sc3335.c
module_driver/sensor/sc3335/
```

它负责：

```text
1. I2C 读写 SC3335 寄存器
2. 上电时序
3. reset / pwdn GPIO
4. MCLK 输入
5. 读 chip id
6. 配置分辨率寄存器表
7. 配置 MIPI 输出寄存器
8. 配置曝光、增益、vts、hts
9. stream on / stream off
10. 注册成 v4l2_subdev
```

---

## 2.2 MIPI CSI-2 接收驱动

这是 SoC 内部的硬件驱动。

比如不同平台可能叫：

```text
mipi-csi2
csi-rx
csi2host
csi-bridge
dphy
csi
vic
vin
isp-csi
```

它负责：

```text
1. 配置 MIPI data lane 数量
2. 配置 clock lane
3. 配置 lane 顺序
4. 配置 D-PHY settle timing
5. 配置输入数据类型 RAW10 / RAW12 / YUV422
6. 配置 virtual channel
7. 处理中断：
   frame start
   frame end
   CRC error
   ECC error
   FIFO overflow
   lane error
8. 把数据送给 ISP 或 DMA
```

Linux 官方文档里也把 CSI-2 定义为从 camera 向 host SoC 传输图像的数据总线，并且说明 sensor 这类 CSI-2 transmitter 需要向 receiver 提供 link frequency、pixel rate 等配置信息。([Linux Kernel Archives][2])

---

## 2.3 ISP / Capture 驱动

这层负责真正生成：

```text
/dev/video0
```

它负责：

```text
1. 接收 RAW 图像
2. 调 ISP pipeline
3. demosaic 去马赛克
4. AWB 自动白平衡
5. AE 自动曝光
6. CCM 颜色校正
7. Gamma
8. 降噪
9. 锐化
10. 输出 NV12 / YUYV / RGB
11. DMA 到内存
12. V4L2 buffer 管理
```

如果没有 ISP，RAW sensor 出来的图直接看会很怪：

```text
发绿
发紫
马赛克
偏暗
颜色完全不正常
```

不是 MIPI 坏了，而是 RAW 没经过 ISP。

---

# 3. SC3335 + Linux V4L2 的完整链路

你可以把整条链路画成这样：

```text
SC3335
  │
  │ I2C 控制通道
  │    读 chip id
  │    写寄存器
  │    设置曝光/增益/分辨率
  │    stream on/off
  │
  ├──────────────> sc3335 sensor driver
  │
  │ MIPI CSI-2 图像数据通道
  │    RAW10 / RAW12
  │    1 lane / 2 lane
  │    clock lane + data lanes
  │
  ▼
SoC MIPI CSI-2 RX / D-PHY
  │
  ▼
ISP
  │
  ▼
DMA
  │
  ▼
V4L2 video node
  │
  ▼
/dev/video0
  │
  ▼
v4l2-ctl / GStreamer / OpenCV / 自己的应用
```

所以 `/dev/video0` 不是 SC3335 直接生成的。
通常是 SoC 的 ISP/capture 驱动生成的。

SC3335 驱动更多是：

```text
/dev/v4l-subdev0
```

或者厂商 SDK 里不一定显式暴露 subdev，但逻辑上也是 sensor 子设备。

---

# 4. 设备树里 MIPI 主要配什么？

MIPI camera 最关键的是设备树里的 `port` / `endpoint`。

Linux 设备树 video interface binding 里说明，外部 camera sensor 通常挂在 I2C/SPI/UART 控制总线上，图像数据接口通过 `port` 节点描述，连接参数放在 `endpoint` 子节点里，两个 endpoint 通过 `remote-endpoint` 互相连接。([Linux Kernel Archives][3])

一个简化例子：

```dts
&i2c1 {
    status = "okay";

    sc3335: camera@30 {
        compatible = "smartsens,sc3335";
        reg = <0x30>;

        clocks = <&clk_cam>;
        clock-names = "xvclk";

        reset-gpios = <&gpio1 5 GPIO_ACTIVE_LOW>;
        pwdn-gpios  = <&gpio1 6 GPIO_ACTIVE_HIGH>;

        avdd-supply  = <&vcc_2v8>;
        dvdd-supply  = <&vcc_1v2>;
        dovdd-supply = <&vcc_1v8>;

        port {
            sc3335_out: endpoint {
                remote-endpoint = <&mipi_csi_in>;

                /*
                 * MIPI CSI-2 D-PHY 常见：
                 * clock lane = 0
                 * data lane = 1, 2
                 */
                clock-lanes = <0>;
                data-lanes = <1 2>;

                /*
                 * 根据 sensor MIPI PLL / 模式配置填写。
                 * 不是随便写。
                 */
                link-frequencies = /bits/ 64 <240000000>;
            };
        };
    };
};

&mipi_csi {
    status = "okay";

    port {
        mipi_csi_in: endpoint {
            remote-endpoint = <&sc3335_out>;
            clock-lanes = <0>;
            data-lanes = <1 2>;
        };
    };
};
```

注意：这个只是模板。你的君正 X2600 SDK 可能不完全长这样，厂商可能有自己的 sensor 配置表、ISP 配置文件、board 配置文件。

---

# 5. MIPI 里你真正需要关心哪些参数？

## 5.1 data-lanes

这个非常关键。

比如：

```dts
data-lanes = <1 2>;
```

意思是 2 条 MIPI 数据 lane。

如果硬件实际接线是 lane1/lane2，但你写成：

```dts
data-lanes = <2 1>;
```

可能就会：

```text
无图
花屏
偶尔有帧
MIPI CRC/ECC 错误
```

有些平台支持 lane swap，有些不支持。

---

## 5.2 clock-lanes

常见：

```dts
clock-lanes = <0>;
```

表示 clock lane 使用物理 lane 0。

这个一般跟硬件原理图一致。

---

## 5.3 link-frequencies

这个是 MIPI 链路频率。

不是随便填一个数。
它要跟 sensor 寄存器里配置的 MIPI PLL、输出分辨率、帧率、RAW bit 数对应。

Linux 文档里说，`V4L2_CID_LINK_FREQ` 用来告诉 receiver 总线频率，receiver 可以通过它获取 transmitter 的 link frequency；D-PHY 下 pixel rate 和 link frequency、lane 数、bits per sample 有关系。([Linux Kernel 文档][4])

核心关系可以粗略理解成：

\text{pixel_rate} = \frac{\text{link_freq} \times 2 \times \text{lanes}}{\text{bits_per_sample}}

为什么乘 2？
因为 MIPI D-PHY 高速模式是 DDR，时钟上升沿和下降沿都传数据。

举个粗略例子，假设 SC3335 跑：

```text
2312 x 1304
30 fps
RAW10
2 lanes
```

有效像素速率大约：

```text
2312 * 1304 * 30 ≈ 90.4 Mpixel/s
```

RAW10 总数据量约：

```text
90.4M * 10bit ≈ 904 Mbps
```

2 lane 平摊后：

```text
每 lane ≈ 452 Mbps
```

D-PHY 因为 DDR：

```text
link_freq ≈ 452 / 2 ≈ 226 MHz
```

但是这只是**有效像素粗算**。实际还要考虑：

```text
水平 blanking
垂直 blanking
MIPI packet 开销
sensor PLL 配置
HTS / VTS
厂商寄存器表
```

所以实际 `link-frequencies` 可能是：

```text
216 MHz
225 MHz
240 MHz
297 MHz
360 MHz
```

具体必须看 SC3335 寄存器表和平台 SDK。

---

## 5.4 MIPI 数据类型

MIPI CSI-2 包里有 Data Type。

常见：

```text
RAW8
RAW10
RAW12
YUV422 8-bit
RGB888
```

SC3335 如果输出 RAW10，那么 SoC MIPI RX 要按 RAW10 接。

Linux sensor 驱动里一般表现为：

```c
MEDIA_BUS_FMT_SBGGR10_1X10
MEDIA_BUS_FMT_SRGGB10_1X10
MEDIA_BUS_FMT_SGBRG10_1X10
MEDIA_BUS_FMT_SGRBG10_1X10
```

这些是 media bus code，不是 `/dev/video0` 最终输出格式。

---

## 5.5 Bayer pattern

如果 SC3335 输出 RAW Bayer，要确认 Bayer 排列：

```text
BGGR
RGGB
GRBG
GBRG
```

这个极其重要。

如果 Bayer pattern 配错，表现是：

```text
颜色严重不对
整体偏绿
偏紫
红蓝反
白平衡很怪
```

这个问题很多时候不是 MIPI 问题，而是：

```text
sensor driver 的 mbus_code 配错
ISP 输入 Bayer order 配错
镜像/翻转之后 Bayer order 没同步改变
```

尤其注意：

```text
开启 mirror / flip 后，Bayer 顺序可能会变。
```

例如原来是：

```text
BGGR
```

水平翻转后可能变成：

```text
GBRG
```

垂直翻转后可能变成：

```text
GRBG
```

水平+垂直都翻转后可能变成：

```text
RGGB
```

不同 sensor 要看 datasheet 和寄存器说明。

---

# 6. 搞 SC3335 需要“调 Linux MIPI”吗？

答案分情况。

## 情况 1：完全没图

比如：

```text
v4l2-ctl --stream-mmap 卡住
select timeout
DQBUF 超时
dmesg 有 csi timeout
```

这时要重点查 MIPI：

```text
1. sensor 是否真的 stream on
2. data-lanes 是否对
3. lane 顺序是否对
4. clock-lanes 是否对
5. MIPI RX 是否 enable
6. D-PHY 是否 lock
7. link-frequencies 是否对
8. RAW10/RAW12 数据类型是否对
9. virtual channel 是否对
10. sensor MIPI PLL 是否对
```

这属于 **MIPI 链路问题**。

---

## 情况 2：有图，但是花屏、横纹、丢帧

这时也可能要调 MIPI：

```text
1. MIPI 速率太高，信号质量不够
2. D-PHY settle timing 不合适
3. lane skew 问题
4. SoC MIPI RX FIFO overflow
5. sensor 输出速率和接收端配置不一致
6. DDR/DMA 带宽不够
7. RAW bit width 配错
8. bytesperline/stride 配错
```

表现可能是：

```text
一条条横线
画面撕裂
一半正常一半花
偶尔丢帧
高温后异常
线长一点就异常
```

这类问题 MIPI、CSI、DMA、硬件信号都要查。

---

## 情况 3：有稳定图像，但是颜色不对

大概率不是 MIPI。

优先查：

```text
1. Bayer order 是否对
2. ISP 参数是否匹配 SC3335
3. AWB 是否工作
4. CCM 是否正确
5. 黑电平 BLC 是否正确
6. lens shading correction 是否正确
7. gamma 是否正确
8. sensor mirror/flip 后 Bayer 顺序是否改了
```

MIPI 只负责把 bit 流接进来。
MIPI 不知道“这张图颜色好不好看”。

所以：

```text
无图 / 花屏 / CRC ECC error     优先查 MIPI/CSI
颜色差 / 偏色 / 暗 / 噪声大     优先查 ISP/sensor 寄存器
```

---

## 情况 4：能采 RAW，但是 ISP 后画面很差

这时重点是 ISP tuning，不是 MIPI。

SC3335 这类 sensor 上产品，通常需要厂商给：

```text
SC3335 sensor driver
SC3335 register table
SC3335 ISP IQ 文件
SC3335 AE 参数
SC3335 AWB 参数
SC3335 CCM 参数
SC3335 LSC 参数
SC3335 DPC 参数
SC3335 NR 参数
```

如果你只有 sensor 驱动，没有 ISP tuning 文件，可能可以出图，但画质很差。

---

# 7. V4L2 在 SC3335 里具体怎么体现？

假设你的链路是：

```text
SC3335 -> MIPI CSI -> ISP -> /dev/video0
```

V4L2 里可能看到：

```bash
media-ctl -p
```

类似：

```text
entity 1: sc3335 1-0030
    pad0: Source
        [fmt:SBGGR10_1X10/2312x1304]
        -> "mipi-csi2":0 [ENABLED]

entity 2: mipi-csi2
    pad0: Sink
        [fmt:SBGGR10_1X10/2312x1304]
    pad1: Source
        [fmt:SBGGR10_1X10/2312x1304]
        -> "isp":0 [ENABLED]

entity 3: isp
    pad0: Sink
        [fmt:SBGGR10_1X10/2312x1304]
    pad1: Source
        [fmt:NV12/2304x1296]
        -> "capture":0 [ENABLED]

entity 4: capture
    device node name /dev/video0
```

这个里面有两个格式概念：

```text
sensor -> csi -> isp:
    MEDIA_BUS_FMT_SBGGR10_1X10

/dev/video0 输出:
    V4L2_PIX_FMT_NV12 / YUYV / RGB565
```

所以你用：

```bash
v4l2-ctl -d /dev/video0 --list-formats-ext
```

看到的可能是：

```text
NV12
YUYV
```

但 sensor subdev 上可能是：

```text
SBGGR10_1X10
```

这两个不是一回事。

---

# 8. SC3335 sensor 驱动里面要实现什么？

一个 SC3335 V4L2 subdev 驱动通常有这些内容。

## 8.1 mode 表

比如：

```c
struct sc3335_mode {
    u32 width;
    u32 height;
    u32 hts;
    u32 vts;
    u32 fps;
    u32 bus_fmt;
    u64 link_freq;
    u64 pixel_rate;
    const struct regval *reg_list;
};
```

可能有多个模式：

```text
2312x1304@30 RAW10
2304x1296@25 RAW10
1920x1080@30 RAW10
1280x720@60 RAW10
```

每个模式对应一大堆寄存器：

```c
static const struct regval sc3335_2312x1304_30fps_regs[] = {
    {0x0103, 0x01},  // software reset
    {0x0100, 0x00},  // stream off
    ...
    {0x3208, ...},  // width high
    {0x3209, ...},  // width low
    ...
    {0x0100, 0x01},  // stream on
};
```

具体寄存器不能乱写，要用 sensor datasheet 或厂商给的初始化表。

---

## 8.2 probe 阶段

probe 做这些：

```text
1. 解析设备树
2. 获取 mclk/xvclk
3. 获取 reset/pwdn GPIO
4. 获取 regulator 电源
5. 上电
6. 延时
7. I2C 读 chip id
8. 初始化 v4l2_subdev
9. 初始化 media pad
10. 初始化 controls
11. 注册 subdev
```

---

## 8.3 s_stream 阶段

`VIDIOC_STREAMON` 最后会一路调用到 sensor：

```c
static int sc3335_s_stream(struct v4l2_subdev *sd, int on)
{
    if (on) {
        sc3335_power_on();
        sc3335_write_array(sc3335->cur_mode->reg_list);
        sc3335_apply_exposure_gain();
        sc3335_write_reg(0x0100, 0x01); // stream on
    } else {
        sc3335_write_reg(0x0100, 0x00); // stream off
    }

    return 0;
}
```

你调试时可以在这里加 log：

```c
dev_info(dev, "SC3335 stream on\n");
dev_info(dev, "SC3335 stream off\n");
```

如果用户态执行：

```bash
v4l2-ctl -d /dev/video0 --stream-mmap --stream-count=100
```

但是 sensor 驱动没有打印 `stream on`，说明 pipeline 没有真正启动到 sensor。

---

# 9. MIPI CSI RX 驱动具体在 Linux 干什么？

SoC 的 MIPI 驱动一般会在 stream on 时做：

```text
1. 从 media graph 找到上游 sensor
2. 读取 sensor mbus format
3. 读取 endpoint 里的 data-lanes
4. 获取 link_freq / pixel_rate
5. 配置 D-PHY
6. 配置 CSI-2 controller
7. 配置数据类型 RAW10
8. 配置虚拟通道 VC0
9. 打开中断
10. enable receiver
11. 调 sensor s_stream(1)
```

正常工作时，中断里能看到：

```text
frame start
frame end
dma done
```

异常时可能看到：

```text
ECC error
CRC error
FIFO overflow
line length error
frame sync error
timeout
```

如果你的 SDK 有 MIPI CSI debug 寄存器，建议重点看：

```text
PHY lock 状态
lane stop state
HS receive 状态
frame start 计数
frame end 计数
ECC error 计数
CRC error 计数
FIFO overflow 计数
```

---

# 10. D-PHY settle timing 是什么？

MIPI D-PHY 有 LP 低功耗状态和 HS 高速状态。

sensor 开流时，大致过程是：

```text
LP state
  ↓
HS prepare
  ↓
HS zero
  ↓
HS data transmission
  ↓
HS trail
  ↓
LP state
```

SoC 接收端要在合适时间点开始采样高速数据。
这个时间窗口就跟 `hs-settle`、`clk-settle`、`settle time` 这类参数有关。

如果 settle 不合适，可能：

```text
低分辨率正常，高分辨率不正常
低 fps 正常，高 fps 不正常
短线正常，长线不正常
偶尔 CRC/ECC 错误
开机偶现无图
```

有些平台设备树里会有类似：

```dts
hs-settle = <13>;
clk-settle = <2>;
```

有些平台驱动里根据 lane rate 自动算。
有些厂商 SDK 直接写在 board 配置或者寄存器表里。

这部分就是“调 Linux MIPI”最典型的地方。

---

# 11. SC3335 调试时的标准流程

## 第一步：确认硬件

先确认：

```text
SC3335 电源：
    AVDD 2.8V？
    DVDD 1.2V？
    DOVDD 1.8V？

MCLK：
    24MHz？
    27MHz？

I2C：
    地址是多少？
    SCL/SDA 上拉有没有？
    能否读 chip id？

控制脚：
    RESET
    PWDN
    XSHUTDN

MIPI：
    几 lane？
    lane 是否交叉？
    clock lane 是哪根？
    是否有接反？
    是否走线太长？
```

SC3335 模组资料里还列出了 MCLK、PWDNB、XSHUTDN、AVDD、DVDD、DOVDD、MIPI/DVP 引脚等连接信息，说明你看原理图时必须把控制脚、电源和 MIPI/DVP 引脚都对上。([kailaptech.net][5])

---

## 第二步：确认 I2C probe

板端看日志：

```bash
dmesg | grep -i sc3335
dmesg | grep -i sensor
dmesg | grep -i camera
```

如果 I2C 读 ID 失败：

```text
先别管 MIPI。
```

优先查：

```text
电源
MCLK
reset/pwdn 极性
I2C 地址
pinctrl
I2C 上拉
sensor 是否退出 standby
```

---

## 第三步：确认 media graph

```bash
media-ctl -p -d /dev/media0
```

你要看到：

```text
sc3335 -> mipi csi -> isp -> video
```

如果 SC3335 有了，但是没连到 CSI：

```text
设备树 remote-endpoint 大概率有问题。
```

---

## 第四步：确认 subdev 格式

看 sensor 输出是不是 RAW10：

```bash
media-ctl -p -d /dev/media0
```

重点看：

```text
fmt:SBGGR10_1X10/2312x1304
```

如果 sensor 是 RAW10，但 CSI sink 变成了 RAW8 或 YUYV，那肯定不对。

---

## 第五步：测试 stream

```bash
v4l2-ctl -d /dev/video0 \
  --stream-mmap \
  --stream-count=100 \
  --stream-to=/dev/null
```

如果成功，说明大链路至少能跑。

如果 timeout，查：

```text
sensor s_stream 是否执行
MIPI RX 是否有 frame start
MIPI RX 是否有 frame end
DMA 是否 done
```

---

# 12. 你到底要不要改 MIPI 驱动代码？

通常优先级是这样的：

```text
第一优先级：改设备树
    data-lanes
    clock-lanes
    link-frequencies
    remote-endpoint
    mclk
    reset/pwdn
    power supplies

第二优先级：改 sensor 驱动
    寄存器表
    RAW10/RAW12 mbus code
    width/height
    hts/vts
    pixel_rate/link_freq
    exposure/gain
    stream on/off

第三优先级：改 ISP 配置
    sensor IQ 文件
    Bayer order
    black level
    AE/AWB/CCM/Gamma/LSC

第四优先级：改 MIPI CSI 驱动
    只有当平台驱动本身不支持你的 lane/速率/格式/settle 时才改
```

所以答案是：

**大多数情况下，不是直接改 Linux MIPI 驱动代码，而是配置好设备树 + sensor 驱动里的 MIPI 参数。**

只有这些情况才可能需要改 MIPI CSI 驱动：

```text
1. SoC 驱动不支持 SC3335 的 RAW10/RAW12 data type
2. 驱动没有正确解析 data-lanes
3. 驱动没有根据 link_freq 配 D-PHY
4. D-PHY settle 算法不适合你的 sensor
5. 驱动没有暴露/处理 MIPI 错误中断
6. 厂商驱动 bug
7. 你的硬件 lane 接法和驱动假设不一致
```

---

# 13. SC3335 常见问题应该怎么判断？

## 13.1 I2C 读不到 ID

不是 MIPI 问题。

查：

```text
电源
MCLK
reset/pwdn
I2C 地址
I2C pinmux
上拉电阻
```

---

## 13.2 I2C 能读 ID，但 stream timeout

大概率是：

```text
sensor 没输出 MIPI
MIPI lane 配错
link_freq 配错
MIPI RX 没起来
sensor stream on 寄存器没写
```

---

## 13.3 有 frame start，没有 frame end

可能是：

```text
分辨率/行长不匹配
MIPI packet 异常
RAW bit width 配错
CSI 接收端配置错
sensor 输出不完整
```

---

## 13.4 有 frame end，但是没有 DMA done

可能是：

```text
CSI 到 ISP 链路没通
ISP 输入格式不匹配
DMA buffer 配置错
bytesperline/stride 错
VB2 buffer 没入队
```

---

## 13.5 有图但颜色怪

优先查：

```text
Bayer order
ISP tuning
AWB
CCM
mirror/flip
黑电平
```

---

## 13.6 图像上下左右反

查：

```text
sensor mirror/flip 寄存器
ISP mirror/flip
应用显示方向
```

注意 mirror/flip 后 Bayer order 可能也要跟着改。

---

# 14. 结合 SC3335 的开发步骤

你真正做的时候可以按这个顺序：

```text
1. 找到 SC3335 datasheet / 厂商寄存器表
2. 确认板子硬件连接：
   I2C、MCLK、RESET、PWDN、电源、MIPI lane

3. 在 DTS 里添加 SC3335 节点
4. 配好 sensor endpoint：
   data-lanes
   clock-lanes
   link-frequencies
   remote-endpoint

5. 打开 SoC MIPI CSI / ISP 节点
6. 编译内核和设备树
7. 启动后确认 I2C chip id
8. media-ctl -p 看 graph
9. v4l2-ctl --list-formats-ext 看格式
10. v4l2-ctl --stream-mmap 测试采集
11. 如果无图，查 MIPI/CSI 中断和错误寄存器
12. 如果有图但颜色差，调 Bayer order + ISP IQ
13. 最后再调曝光、增益、帧率、画质
```

---

# 15. 你可以这样理解 V4L2 + MIPI 的关系

```text
V4L2：
    Linux 给用户态的统一 camera 接口

Media Controller：
    描述 sensor、mipi、isp、video node 的连接关系

SC3335 driver：
    通过 I2C 控制 SC3335，让它输出 MIPI RAW 图像

MIPI CSI RX driver：
    SoC 侧接收 MIPI bit 流，把 RAW 图像接进来

ISP driver：
    把 RAW Bayer 处理成正常图像

/dev/video0：
    应用最终拿图像帧的地方
```

一句话：

```text
SC3335 负责“吐图”；
MIPI CSI 负责“接图”；
ISP 负责“把 RAW 变漂亮”；
V4L2 负责“把图交给应用”。
```

---

# 16. 最关键的判断口诀

以后你调 SC3335，可以按这个口诀判断：

```text
读不到 chip id：
    查 I2C、电源、MCLK、reset、pwdn

读到 chip id 但没图：
    查 stream on、MIPI lane、link_freq、CSI RX、D-PHY

有图但花屏：
    查 MIPI 速率、settle、RAW 位宽、stride、DMA

有图但颜色错：
    查 Bayer order、ISP tuning、AWB、CCM

有图但画质差：
    查 ISP IQ 文件、sensor 曝光增益、镜头参数

fps 不对：
    查 HTS、VTS、pixel_rate、link_freq、帧率寄存器
```

所以你问“搞 RGB image sensor 需要调 Linux MIPI 啥东西吗？”——更准确的回答是：

**需要把 MIPI 的 lane、速率、格式、D-PHY、endpoint 配对调正确，让数据稳定进来；但颜色和画质主要靠 sensor 输出格式、Bayer 顺序和 ISP 调参，不是靠 MIPI 调出来的。**

[1]: https://kailaptech.en.made-in-china.com/product/hwNTfcDGLOkB/China-Klt-4ampf-Sc3335-V1-0-WiFi-Camera-3MP-Sc3335-Mipi-and-Dvp-Parallel-Interface-M14-Fixed-Focus-Camera-Module.html "Klt-4ampf-Sc3335 V1.0 WiFi Camera 3MP Sc3335 Mipi and Dvp Parallel Interface M14 Fixed Focus Camera Module - Sc3335 Camera Module and Fixed Focus"
[2]: https://www.kernel.org/doc/html/v4.10/media/kapi/csi2.html "13. MIPI CSI-2 — The Linux Kernel  documentation"
[3]: https://www.kernel.org/doc/Documentation/devicetree/bindings/media/video-interfaces.yaml?utm_source=chatgpt.com "https://www.kernel.org/doc/Documentation/devicetre..."
[4]: https://docs.kernel.org/driver-api/media/tx-rx.html "8. Pixel data transmitter and receiver drivers — The Linux Kernel  documentation"
[5]: https://kailaptech.net/KLT/EN/PDF/KLT-4AMPF-SC3335%20V1.0%203MP%20SmartSens%20SC3335%20MIPI%20and%20DVP%20Parallel%20Interface%20M12%20Fixed%20Focus%20Camera%20Module.pdf "KLT-4AMPF-SC3335 V1.0 3MP SmartSens SC3335 MIPI and DVP Parallel Interface M12 Fixed Focus Camera Module.pub"
