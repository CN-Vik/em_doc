可以。**Linux IIO 可以理解成：Linux 内核专门给“传感器/数据采集类设备”设计的一套标准驱动框架。**

比如这些设备通常走 IIO：

```text
ADC
DAC
加速度计
陀螺仪
磁力计
气压计
光照传感器
温湿度传感器
电流/电压采样芯片
IMU，例如 ICM42688
```

按键不走 IIO，按键一般走 `input` 子系统。
LCD 不走 IIO，LCD 走 DRM/fbdev。
音频不走 IIO，音频走 ALSA。
IMU、ADC 这类“连续采样数据”的设备，才非常适合 IIO。

---

# 1. IIO 是干什么的？

IIO 全称是：

```text
Industrial I/O
```

直译是“工业输入输出”，但你不要被名字误导。它不是只给工业设备用，而是 Linux 里专门管理**传感器、ADC、DAC、IMU、采样设备**的一套框架。

Linux 官方文档对 IIO 的定位是：它给很多嵌入式传感器驱动提供统一框架，并且给用户态应用提供标准接口。IIO 的核心实现位于内核 `drivers/iio/industrialio-*` 相关代码中。([Linux Kernel 文档][1])

你可以把它理解成：

```text
IIO = Linux 传感器/采样设备的标准驱动框架
```

---

# 2. 为什么需要 IIO？

假设你现在有一个 ICM42688 IMU。

它内部有：

```text
accel_x
accel_y
accel_z
gyro_x
gyro_y
gyro_z
temperature
FIFO
中断
采样率配置
量程配置
```

如果每个厂家都自己做一个 `/dev/icm42688`，应用层就会很乱：

```text
厂家A：/dev/imu0
厂家B：/dev/icm42688
厂家C：/sys/class/gyro
厂家D：ioctl 自定义一堆命令
```

这样应用很难通用。

IIO 的作用就是把这些传感器抽象成统一格式：

```text
/sys/bus/iio/devices/iio:deviceX/
```

低速读取时：

```bash
cat /sys/bus/iio/devices/iio:device0/in_accel_x_raw
```

高速连续采样时：

```text
/dev/iio:deviceX
```

Linux 官方文档明确说，用户态和 IIO 驱动交互主要有两种方式：一种是 `/sys/bus/iio/devices/iio:deviceX/`，用于表示硬件传感器并组织同一芯片的数据通道；另一种是 `/dev/iio:deviceX` 字符设备节点，用于 buffered data transfer 和事件信息获取。([Linux Kernel 文档][1])

---

# 3. IIO 在 Linux 驱动体系里的位置

整体结构是：

```text
应用程序
  ↓
sysfs 或 /dev/iio:deviceX
  ↓
IIO core
  ↓
具体传感器驱动，例如 inv_icm42600
  ↓
I2C / SPI / platform bus
  ↓
SoC 控制器驱动，例如 X2600 I2C 控制器
  ↓
真实硬件，例如 ICM42688
```

拿 ICM42688 举例：

```text
ICM42688 硬件
  ↓ I2C
X2600 I2C 控制器
  ↓
Linux I2C core
  ↓
ICM42688 IIO 驱动
  ↓
IIO core
  ↓
/sys/bus/iio/devices/iio:deviceX
  ↓
你的应用程序
```

---

# 4. IIO 和普通字符设备驱动有什么区别？

你自己写字符设备驱动，可能是这样：

```text
/dev/icm42688
ioctl(ICM_SET_ODR)
ioctl(ICM_GET_ACCEL)
ioctl(ICM_GET_GYRO)
```

这个方式能做，但不标准。

IIO 是标准方式：

```text
/sys/bus/iio/devices/iio:device0/in_accel_x_raw
/sys/bus/iio/devices/iio:device0/in_accel_y_raw
/sys/bus/iio/devices/iio:device0/in_accel_z_raw
/sys/bus/iio/devices/iio:device0/in_anglvel_x_raw
/sys/bus/iio/devices/iio:device0/sampling_frequency
/sys/bus/iio/devices/iio:device0/buffer/enable
/dev/iio:device0
```

所以：

| 方式        | 特点                                  |
| --------- | ----------------------------------- |
| 自己写字符设备   | 灵活，但不标准，应用和驱动强绑定                    |
| IIO       | 标准、通用、适合传感器、支持 buffer/trigger/sysfs |
| input 子系统 | 适合按键、鼠标、触摸屏、键盘事件                    |
| hwmon     | 适合温度、电压、风扇等硬件监控                     |
| ALSA      | 适合音频                                |
| V4L2      | 适合摄像头                               |

所以 ICM42688 这种 IMU，**优先用 IIO，不建议自己造 `/dev/imu`。**

---

# 5. IIO 里面最重要的几个概念

你先记这几个：

```text
iio_dev
channel
raw
scale
offset
sampling_frequency
buffer
scan_elements
trigger
event
```

下面一个个讲。

---

# 6. `iio_dev`：一个 IIO 设备

`iio_dev` 可以理解成一个传感器设备对象。

比如一个 ICM42688 芯片，驱动注册后可能对应一个：

```text
/sys/bus/iio/devices/iio:device0/
```

Linux 官方文档说，一个 IIO device 通常对应一个硬件传感器，并包含驱动处理该设备所需的信息。典型 IIO 驱动通常注册为 I2C 或 SPI 驱动，在 probe 阶段分配 `iio_dev`、初始化设备名和 channels，然后调用 `iio_device_register()` 注册到 IIO core。([Linux Kernel 文档][1])

简单类比：

```text
struct iio_dev ≈ Linux 里代表这个传感器的对象
```

一个 IIO 设备下面有很多通道。

---

# 7. channel：数据通道

channel 就是传感器的一个测量通道。

比如 ICM42688：

```text
accel_x
accel_y
accel_z
gyro_x
gyro_y
gyro_z
temperature
timestamp
```

这些都可以是 IIO channel。

Linux 官方文档说，一个 IIO channel 表示一个数据通道；比如温度计有一个温度通道，加速度计最多有 X/Y/Z 三个加速度通道。([Linux Kernel 文档][1])

所以 IMU 的通道可以理解成：

```text
IIO channel 0：加速度 X
IIO channel 1：加速度 Y
IIO channel 2：加速度 Z
IIO channel 3：角速度 X
IIO channel 4：角速度 Y
IIO channel 5：角速度 Z
IIO channel 6：温度
IIO channel 7：时间戳
```

在驱动代码里，这些通道通常用：

```c
struct iio_chan_spec
```

描述。

---

# 8. raw：原始值

`raw` 就是芯片寄存器读出来的原始 ADC/传感器数值。

比如：

```bash
cat /sys/bus/iio/devices/iio:device0/in_accel_x_raw
```

可能输出：

```text
1234
```

这个 `1234` 不是直接等于 `1234 m/s²`，它只是芯片的原始码值。

IIO ABI 文档里定义了很多标准 raw 文件，例如 `in_accel_x_raw`、`in_accel_y_raw`、`in_accel_z_raw` 表示 X/Y/Z 方向加速度原始值，经过 scale 和 offset 换算后单位是 `m/s²`；`in_anglvel_x_raw`、`in_anglvel_y_raw`、`in_anglvel_z_raw` 表示角速度原始值，经过换算后单位是 `rad/s`。([Linux Kernel Archives][2])

---

# 9. scale：比例系数

`scale` 是把 raw 转成实际物理单位的比例。

例如：

```bash
cat /sys/bus/iio/devices/iio:device0/in_accel_scale
```

可能输出：

```text
0.000598
```

那么：

```text
真实加速度 = raw * scale
```

如果有 offset，则是：

```text
真实值 = (raw + offset) * scale
```

IIO ABI 文档里也说明，`offset` 是在缩放之前加到 raw 上的值，用来得到文档规定单位下的实际数值。([Linux Kernel Archives][2])

比如：

```text
in_accel_x_raw = 16384
in_accel_scale = 0.000598
```

那么：

```text
accel_x = 16384 * 0.000598 ≈ 9.79 m/s²
```

这就接近 1g。

---

# 10. sampling_frequency：采样率

`sampling_frequency` 就是采样频率，单位一般是 Hz。

比如：

```bash
cat /sys/bus/iio/devices/iio:device0/sampling_frequency
```

可能输出：

```text
100
```

表示 100Hz。

设置：

```bash
echo 200 > /sys/bus/iio/devices/iio:device0/sampling_frequency
```

表示尝试把采样率设置成 200Hz。

IIO ABI 文档说明，某些设备有内部采样时钟，`sampling_frequency` 用于设置最终采样频率；如果设备只支持一组固定频率，驱动还可以暴露 `sampling_frequency_available`。([Linux Kernel Archives][2])

有些驱动会分开：

```text
in_accel_sampling_frequency
in_anglvel_sampling_frequency
```

有些是整个设备统一一个：

```text
sampling_frequency
```

这个取决于具体驱动。

---

# 11. IIO 用户态接口有两种

## 第一种：sysfs 低速接口

路径：

```text
/sys/bus/iio/devices/iio:deviceX/
```

适合：

```text
低速读取
调试
工厂测试
偶尔读一次传感器
配置采样率
配置量程
查看设备名
```

例如：

```bash
cd /sys/bus/iio/devices/iio:device0

cat name
cat in_accel_x_raw
cat in_accel_y_raw
cat in_accel_z_raw
cat in_accel_scale
cat sampling_frequency
```

这种方式简单，但不适合高频读取。

比如你用 shell 每 10ms `cat in_accel_x_raw`，效率很低，而且时间不准。

---

## 第二种：buffer 高速接口

路径：

```text
/dev/iio:deviceX
```

配套配置目录：

```text
/sys/bus/iio/devices/iio:deviceX/buffer/
/sys/bus/iio/devices/iio:deviceX/scan_elements/
/sys/bus/iio/devices/iio:deviceX/trigger/
```

适合：

```text
高频连续采样
IMU 姿态融合
电机控制采样
波形采集
ADC 多通道采集
FIFO 批量读取
```

Linux 官方文档说，IIO core 提供了基于 trigger source 的连续数据采集方式，可以一次从 `/dev/iio:deviceX` 读取多个数据通道，从而降低 CPU 负载。([Linux Kernel 文档][3])

---

# 12. buffer 是什么？

buffer 就是内核里的一块数据缓冲区。

普通 sysfs 读取是：

```text
应用 cat raw
  ↓
驱动读一次芯片寄存器
  ↓
返回一个值
```

buffer 模式是：

```text
传感器按固定频率采样
  ↓
触发中断 / trigger
  ↓
驱动把一帧数据放入 IIO buffer
  ↓
应用从 /dev/iio:deviceX 批量 read
```

例如一帧 IMU 数据可能是：

```text
accel_x
accel_y
accel_z
gyro_x
gyro_y
gyro_z
timestamp
```

应用一次 read 可以读到很多帧。

这就比不停 `cat raw` 高效很多。

---

# 13. scan_elements 是什么？

`scan_elements` 用来选择 buffer 里要采哪些通道。

路径类似：

```bash
ls /sys/bus/iio/devices/iio:device0/scan_elements/
```

可能看到：

```text
in_accel_x_en
in_accel_x_index
in_accel_x_type

in_accel_y_en
in_accel_y_index
in_accel_y_type

in_accel_z_en
in_accel_z_index
in_accel_z_type

in_timestamp_en
in_timestamp_index
in_timestamp_type
```

其中：

```text
*_en：是否开启这个通道
*_index：这个通道在一帧数据里的顺序
*_type：这个通道的数据格式
```

比如：

```bash
echo 1 > scan_elements/in_accel_x_en
echo 1 > scan_elements/in_accel_y_en
echo 1 > scan_elements/in_accel_z_en
echo 1 > scan_elements/in_timestamp_en
```

表示 buffer 里采集：

```text
accel_x + accel_y + accel_z + timestamp
```

`iio_chan_spec` 里面的 `scan_type` 用来描述 buffer 中通道数据格式，例如有符号/无符号、有效 bit 数、存储 bit 数、大小端等。Linux 官方文档把这些字段定义在 `struct iio_scan_type` 中，包括 `sign`、`realbits`、`storagebits`、`shift`、`endianness` 等。([Linux Kernel 文档][1])

这点很关键：
**读 `/dev/iio:deviceX` 不是读文本，而是读二进制帧。应用程序必须按 `scan_elements/*_type` 去解析。**

---

# 14. trigger 是什么？

trigger 是“什么时候采一次数据”的触发源。

常见 trigger：

```text
传感器 data ready 中断
FIFO watermark 中断
定时器 trigger
GPIO trigger
hrtimer trigger
软件 trigger
```

Linux 官方文档说，IIO trigger 用于根据外部事件捕获数据，而不是周期性轮询；trigger 可以来自硬件数据就绪/阈值事件，也可以来自独立中断源，比如 GPIO、定时器或用户态写 sysfs 文件。一个 trigger 还可以触发多个传感器采集。([Linux Kernel 文档][4])

对 ICM42688 来说，比较典型的是：

```text
ICM42688 内部按 ODR 采样
  ↓
数据准备好 / FIFO 达到水位
  ↓
INT1 触发中断
  ↓
IIO trigger 触发
  ↓
驱动读取 accel/gyro/FIFO
  ↓
数据进 IIO buffer
```

用户态有时可以看到：

```bash
ls /sys/bus/iio/devices/trigger0/
cat /sys/bus/iio/devices/trigger0/name
```

把 trigger 绑定给某个 IIO 设备：

```bash
echo trigger0_name > /sys/bus/iio/devices/iio:device0/trigger/current_trigger
```

官方文档也说明，`/sys/bus/iio/devices/iio:deviceX/trigger/current_trigger` 用来把 trigger 名字关联到某个 IIO 设备。([Linux Kernel 文档][4])

---

# 15. event 是什么？

event 是“事件通知”。

不是连续数据，而是某种事件发生了。

例如：

```text
温度超过阈值
加速度超过阈值
接近传感器触发
光照超过阈值
运动检测事件
方向改变事件
```

事件可以通过 IIO event 机制上报给用户态。

IMU 里可能会有：

```text
wake-on-motion
tap
double tap
free fall
motion detect
FIFO overflow
```

但具体能不能暴露成 IIO event，要看驱动是否实现。

对你当前新手阶段，先重点掌握：

```text
raw
scale
sampling_frequency
buffer
trigger
```

event 可以后面再看。

---

# 16. 一个 IIO 设备在板子上长什么样？

假设 ICM42688 驱动起来了，板子上可能看到：

```bash
ls /sys/bus/iio/devices/
```

输出：

```text
iio:device0
trigger0
```

查看设备名：

```bash
cat /sys/bus/iio/devices/iio:device0/name
```

可能是：

```text
icm42688
```

查看属性：

```bash
ls /sys/bus/iio/devices/iio:device0/
```

可能有：

```text
name
dev
sampling_frequency
sampling_frequency_available

in_accel_x_raw
in_accel_y_raw
in_accel_z_raw
in_accel_scale
in_accel_scale_available

in_anglvel_x_raw
in_anglvel_y_raw
in_anglvel_z_raw
in_anglvel_scale
in_anglvel_scale_available

in_temp_raw
in_temp_scale
in_temp_offset

buffer/
scan_elements/
trigger/
```

不同驱动暴露的文件不完全一样，但思想一样。

---

# 17. IIO 和 ICM42688 的对应关系

ICM42688 是 6 轴 IMU，对应到 IIO，大概是：

| ICM42688 功能     | IIO 表示                                            |
| --------------- | ------------------------------------------------- |
| 加速度 X           | `in_accel_x_raw`                                  |
| 加速度 Y           | `in_accel_y_raw`                                  |
| 加速度 Z           | `in_accel_z_raw`                                  |
| 加速度量程           | `in_accel_scale` / `in_accel_scale_available`     |
| 陀螺仪 X           | `in_anglvel_x_raw`                                |
| 陀螺仪 Y           | `in_anglvel_y_raw`                                |
| 陀螺仪 Z           | `in_anglvel_z_raw`                                |
| 陀螺仪量程           | `in_anglvel_scale` / `in_anglvel_scale_available` |
| 温度              | `in_temp_raw` / `in_temp_input`                   |
| 采样率             | `sampling_frequency`                              |
| FIFO            | IIO buffer                                        |
| INT1 data ready | IIO trigger                                       |
| FIFO watermark  | trigger + buffer                                  |
| 坐标安装方向          | mount matrix / 应用层坐标转换                            |

---

# 18. IIO 驱动开发时，内核代码结构是什么？

典型 IIO 驱动结构：

```text
xxx_i2c.c
xxx_spi.c
xxx_core.c
xxx_buffer.c
xxx_trigger.c
xxx.h
```

以 IMU 为例：

```text
xxx_i2c.c
  负责 I2C probe
  创建 regmap
  调用 core probe

xxx_spi.c
  负责 SPI probe
  创建 regmap
  调用 core probe

xxx_core.c
  负责芯片初始化
  WHO_AM_I 检查
  量程/ODR 配置
  iio_dev 注册
  read_raw/write_raw

xxx_buffer.c
  负责 FIFO / IIO buffer

xxx_trigger.c
  负责中断和 trigger
```

为什么要拆？

因为同一颗芯片可能支持 I2C 和 SPI。
I2C/SPI 只是总线不同，芯片内部寄存器逻辑基本相同。

所以常见设计是：

```text
bus driver：处理 I2C/SPI 差异
core driver：处理芯片公共逻辑
```

---

# 19. 一个最小 IIO 驱动 probe 流程

典型流程：

```text
i2c_driver.probe()
  ↓
devm_iio_device_alloc()
  ↓
iio_priv()
  ↓
初始化 regmap
  ↓
读取 WHO_AM_I
  ↓
初始化芯片工作模式
  ↓
填充 indio_dev->name
  ↓
填充 indio_dev->channels
  ↓
填充 indio_dev->num_channels
  ↓
填充 indio_dev->info
  ↓
设置 buffer / trigger，可选
  ↓
iio_device_register()
```

核心代码味道大概是这样：

```c
static int xxx_probe(struct i2c_client *client)
{
    struct iio_dev *indio_dev;
    struct xxx_data *data;
    int ret;

    indio_dev = devm_iio_device_alloc(&client->dev, sizeof(*data));
    if (!indio_dev)
        return -ENOMEM;

    data = iio_priv(indio_dev);

    data->regmap = devm_regmap_init_i2c(client, &xxx_regmap_config);
    if (IS_ERR(data->regmap))
        return PTR_ERR(data->regmap);

    ret = xxx_check_whoami(data);
    if (ret)
        return ret;

    indio_dev->name = "xxx_imu";
    indio_dev->modes = INDIO_DIRECT_MODE;
    indio_dev->channels = xxx_channels;
    indio_dev->num_channels = ARRAY_SIZE(xxx_channels);
    indio_dev->info = &xxx_iio_info;

    return devm_iio_device_register(&client->dev, indio_dev);
}
```

这里面关键对象是：

```text
indio_dev：IIO 设备对象
data：驱动私有数据
regmap：寄存器访问抽象
channels：通道描述
info：read_raw/write_raw 等回调
```

---

# 20. `read_raw()` 是干什么的？

当用户执行：

```bash
cat in_accel_x_raw
```

IIO core 最终会调用驱动的：

```c
.read_raw = xxx_read_raw
```

驱动里面判断用户要读什么：

```c
static int xxx_read_raw(struct iio_dev *indio_dev,
                        struct iio_chan_spec const *chan,
                        int *val, int *val2, long mask)
{
    switch (mask) {
    case IIO_CHAN_INFO_RAW:
        // 读取原始值
        break;

    case IIO_CHAN_INFO_SCALE:
        // 返回 scale
        break;

    case IIO_CHAN_INFO_SAMP_FREQ:
        // 返回采样率
        break;
    }

    return -EINVAL;
}
```

简单理解：

```text
用户读 sysfs 文件
  ↓
IIO core 判断读的是 raw/scale/sampling_frequency
  ↓
调用驱动 read_raw
  ↓
驱动读寄存器或返回配置值
```

---

# 21. `write_raw()` 是干什么的？

当用户执行：

```bash
echo 200 > sampling_frequency
```

IIO core 会调用驱动的：

```c
.write_raw = xxx_write_raw
```

驱动根据写入值配置芯片寄存器：

```text
200Hz
  ↓
查表找到芯片对应 ODR 配置
  ↓
写 ICM42688 ODR 寄存器
```

同理，设置量程也可能是：

```bash
echo 0.000598 > in_accel_scale
```

驱动把它转换成寄存器配置：

```text
±2g / ±4g / ±8g / ±16g
```

---

# 22. `iio_chan_spec` 是怎么生成 sysfs 文件的？

你在驱动里定义 channel，比如：

```c
static const struct iio_chan_spec xxx_channels[] = {
    {
        .type = IIO_ACCEL,
        .modified = 1,
        .channel2 = IIO_MOD_X,
        .info_mask_separate = BIT(IIO_CHAN_INFO_RAW),
        .info_mask_shared_by_type = BIT(IIO_CHAN_INFO_SCALE),
    },
};
```

IIO core 会自动生成类似：

```text
in_accel_x_raw
in_accel_scale
```

所以很多 sysfs 文件不是你手动 `device_create_file()` 创建的，而是 IIO core 根据 channel 描述自动生成的。

这就是 IIO 的强大之处。

---

# 23. IIO buffer 的开发流程

如果你要支持高速连续采样，驱动要做 buffer。

大概流程：

```text
定义 scan_elements
  ↓
定义 trigger handler
  ↓
配置 triggered buffer
  ↓
中断触发后读取一帧数据
  ↓
iio_push_to_buffers_with_timestamp()
```

典型思想：

```c
static irqreturn_t xxx_trigger_handler(int irq, void *p)
{
    struct iio_poll_func *pf = p;
    struct iio_dev *indio_dev = pf->indio_dev;
    struct xxx_data *data = iio_priv(indio_dev);

    // 读取 accel/gyro 数据
    // 按 scan_index 顺序放到 buffer
    // 推给 IIO core

    iio_push_to_buffers_with_timestamp(indio_dev, data->scan_buf,
                                       iio_get_time_ns(indio_dev));

    iio_trigger_notify_done(indio_dev->trig);

    return IRQ_HANDLED;
}
```

如果用 IIO triggered buffer，官方文档说明 `iio_triggered_buffer_setup()` 会完成一些公共工作，比如分配 buffer 和 pollfunc；调用它之前 `indio_dev` 应该已经初始化完成，但尚未注册，所以通常在 `iio_device_register()` 之前调用。([Linux Kernel 文档][5])

---

# 24. 用户态怎么用 IIO buffer？

大概流程是：

```bash
cd /sys/bus/iio/devices/iio:device0

# 1. 关闭 buffer
echo 0 > buffer/enable

# 2. 选择需要的通道
echo 1 > scan_elements/in_accel_x_en
echo 1 > scan_elements/in_accel_y_en
echo 1 > scan_elements/in_accel_z_en
echo 1 > scan_elements/in_timestamp_en

# 3. 设置 buffer 长度
echo 128 > buffer/length

# 4. 如果需要 trigger，绑定 trigger
cat /sys/bus/iio/devices/trigger0/name
echo xxx_trigger_name > trigger/current_trigger

# 5. 打开 buffer
echo 1 > buffer/enable

# 6. 从字符设备读二进制数据
cat /dev/iio:device0 > /tmp/imu.bin
```

注意：

```text
/dev/iio:device0 里面是二进制数据，不是文本。
```

正式应用一般要用 C 程序：

```text
open("/dev/iio:device0")
poll()
read()
按照 scan_elements 解析一帧数据
```

---

# 25. sysfs 读取和 buffer 读取的区别

| 对比项          | sysfs raw        | IIO buffer         |
| ------------ | ---------------- | ------------------ |
| 接口           | `/sys/.../*_raw` | `/dev/iio:deviceX` |
| 数据格式         | 文本               | 二进制                |
| 适合频率         | 低频               | 高频                 |
| 是否连续采样       | 不适合              | 适合                 |
| CPU 开销       | 高                | 低                  |
| 使用难度         | 简单               | 较复杂                |
| 典型用途         | 调试、工厂测试          | 姿态融合、波形采集、稳定采样     |
| IMU 100Hz 以上 | 不推荐              | 推荐                 |

对于 ICM42688：

```text
调试阶段：
cat in_accel_x_raw

产品阶段：
FIFO/trigger/IIO buffer
```

---

# 26. IIO 和设备树的关系

设备树负责告诉 Linux：

```text
这个传感器挂在哪条总线
I2C 地址是多少
中断脚接到哪里
电源 regulator 是哪个
compatible 是什么
```

例如：

```dts
&i2c0 {
    status = "okay";
    clock-frequency = <400000>;

    imu@68 {
        compatible = "invensense,icm42688";
        reg = <0x68>;

        interrupt-parent = <&gpioa>;
        interrupts = <5 IRQ_TYPE_EDGE_FALLING>;

        vdd-supply = <&vdd_3v3>;
        vddio-supply = <&vddio_3v3>;
    };
};
```

然后 Linux 的匹配过程是：

```text
设备树 compatible = "invensense,icm42688"
  ↓
I2C core 创建 i2c_client
  ↓
匹配到 ICM42688 IIO 驱动
  ↓
调用 probe
  ↓
驱动注册 IIO device
  ↓
出现 /sys/bus/iio/devices/iio:deviceX
```

---

# 27. IIO 和 regmap 的关系

IIO 是传感器框架。
regmap 是寄存器访问框架。

它们不是一个东西。

典型关系：

```text
IIO core
  管 sysfs、buffer、channel、trigger

regmap
  管 I2C/SPI 寄存器读写
```

驱动里可能这样：

```c
regmap_read(data->regmap, WHO_AM_I, &val);
regmap_update_bits(data->regmap, PWR_MGMT0, mask, value);
```

然后 IIO 负责把这些数据暴露出去：

```text
in_accel_x_raw
in_anglvel_x_raw
sampling_frequency
```

所以完整关系是：

```text
应用
  ↓
IIO sysfs / buffer
  ↓
IIO driver
  ↓
regmap
  ↓
I2C/SPI
  ↓
芯片寄存器
```

---

# 28. IIO 和 input 子系统的区别

这个你刚问过按键，所以这里对比一下。

| 设备    | 推荐 Linux 子系统    |
| ----- | --------------- |
| 普通按键  | input           |
| 触摸屏   | input           |
| 鼠标键盘  | input           |
| 加速度计  | IIO             |
| 陀螺仪   | IIO             |
| ADC   | IIO             |
| 光照传感器 | IIO             |
| 气压计   | IIO             |
| 温度传感器 | IIO 或 hwmon，看用途 |
| 电池电量  | power_supply    |
| 音频    | ALSA            |
| 摄像头   | V4L2            |
| 显示屏   | DRM/fbdev       |

一句话：

```text
离散事件，比如按下/松开：input
连续物理量，比如加速度/角速度/电压：IIO
```

---

# 29. IIO 驱动调试流程

假设你在 X2600 上调 ICM42688。

## 第一步：看 IIO 设备有没有出来

```bash
ls /sys/bus/iio/devices/
```

如果没有 `iio:deviceX`，说明驱动没注册成功。

---

## 第二步：看设备名

```bash
for d in /sys/bus/iio/devices/iio:device*; do
    echo "$d: $(cat $d/name 2>/dev/null)"
done
```

期望看到：

```text
icm42688
```

或者类似名字。

---

## 第三步：看 raw 属性

```bash
cd /sys/bus/iio/devices/iio:device0
ls
```

找：

```text
in_accel_x_raw
in_accel_y_raw
in_accel_z_raw
in_anglvel_x_raw
in_anglvel_y_raw
in_anglvel_z_raw
```

---

## 第四步：读静态加速度

```bash
cat in_accel_x_raw
cat in_accel_y_raw
cat in_accel_z_raw
cat in_accel_scale
```

板子静止时，应该有一个轴接近 ±1g，另外两个轴接近 0g。

---

## 第五步：读陀螺仪

```bash
cat in_anglvel_x_raw
cat in_anglvel_y_raw
cat in_anglvel_z_raw
cat in_anglvel_scale
```

板子静止时，gyro 三轴应该接近 0。

如果偏差很大，可能需要校准。

---

## 第六步：看采样率

```bash
cat sampling_frequency
cat sampling_frequency_available
```

设置：

```bash
echo 100 > sampling_frequency
```

---

## 第七步：看 buffer 能不能用

```bash
ls buffer
ls scan_elements
```

如果有：

```text
buffer/enable
buffer/length
scan_elements/*_en
```

说明支持 buffer。

---

## 第八步：看 trigger

```bash
ls /sys/bus/iio/devices/trigger*
ls trigger
```

如果支持 trigger，会看到相关目录和 `current_trigger`。

---

# 30. 常见问题

## 问题 1：`iio:deviceX` 不出现

可能原因：

```text
设备树没有加传感器节点
compatible 写错
I2C 地址错
WHO_AM_I 读失败
驱动 config 没开
驱动模块没加载
I2C 控制器没启用
```

查：

```bash
dmesg | grep -i iio
dmesg | grep -i icm
dmesg | grep -i inv
dmesg | grep -i i2c
ls /sys/bus/i2c/devices/
```

---

## 问题 2：有 `iio:deviceX`，但是没有 accel/gyro 文件

可能原因：

```text
驱动不是完整 IMU 驱动
只注册了部分 channel
内核驱动版本太旧
驱动 probe 后降级成了某种兼容模式
```

查：

```bash
cat name
ls -l
```

---

## 问题 3：raw 值一直不变

可能原因：

```text
芯片还在 sleep
ODR 没打开
power mode 没配置
runtime PM 自动 suspend
读错 iio device
I2C 读寄存器失败但驱动没有报明显错误
```

查：

```bash
dmesg -w
cat sampling_frequency
cat in_accel_x_raw
```

---

## 问题 4：buffer 开不了

可能原因：

```text
没有选择 scan_elements
没有绑定 trigger
驱动不支持 buffer
buffer length 设置不合法
触发源没有注册
```

可以检查：

```bash
ls scan_elements
cat buffer/enable
cat buffer/length
ls trigger
cat trigger/current_trigger
```

---

## 问题 5：读 `/dev/iio:deviceX` 是乱码

正常。
因为它是二进制数据。

你要根据：

```text
scan_elements/*_type
scan_elements/*_index
```

解析。

---

# 31. 用一句话记住 IIO

你可以这样记：

```text
IIO = Linux 给 ADC/IMU/传感器这类“连续物理量采集设备”准备的标准框架。
```

低速调试：

```text
/sys/bus/iio/devices/iio:deviceX/*_raw
```

高速采样：

```text
/dev/iio:deviceX + buffer + scan_elements + trigger
```

驱动核心：

```text
iio_dev + iio_chan_spec + read_raw/write_raw + buffer/trigger
```

对于你的 X2600 + ICM42688：

```text
设备树描述 ICM42688
  ↓
I2C probe 读 WHO_AM_I
  ↓
注册 IIO 设备
  ↓
sysfs 读 raw/scale 调试
  ↓
trigger + buffer 做高频采样
  ↓
应用层做姿态解算/滤波/坐标转换
```

[1]: https://docs.kernel.org/driver-api/iio/core.html "Core elements — The Linux Kernel  documentation"
[2]: https://www.kernel.org/doc/Documentation/ABI/testing/sysfs-bus-iio "www.kernel.org"
[3]: https://docs.kernel.org/driver-api/iio/buffers.html "Buffers — The Linux Kernel  documentation"
[4]: https://docs.kernel.org/driver-api/iio/triggers.html "Triggers — The Linux Kernel  documentation"
[5]: https://docs.kernel.org/driver-api/iio/triggered-buffers.html "Triggered Buffers — The Linux Kernel  documentation"


-------
--------
-------
下面按**真实板子上的使用流程**讲。你可以把 IIO 使用分成三层：

```text
第一层：确认 IIO 设备是否出来
第二层：用 sysfs 低速读取 raw/scale，适合调试
第三层：用 buffer + trigger + /dev/iio:deviceX 高速连续采样，适合正式应用
```

Linux 官方文档里也把 IIO 的用户态接口分成两类：`/sys/bus/iio/devices/iio:deviceX/` 用来表示传感器和通道属性，`/dev/iio:deviceX` 用于 buffer 数据传输和事件信息获取。IIO core 本身是给嵌入式传感器驱动和用户态应用提供统一接口的框架。([Linux Kernel 文档][1])

---

# 1. 先记住 IIO 的使用场景

IIO 适合这些设备：

```text
ADC
DAC
加速度计
陀螺仪
磁力计
气压计
光照传感器
温湿度传感器
IMU，比如 ICM42688
```

它不适合按键、LCD、音频、摄像头。

```text
按键       → input 子系统
触摸屏     → input 子系统
IMU/ADC    → IIO 子系统
音频       → ALSA
摄像头     → V4L2
显示屏     → DRM/fbdev
```

对于你的 **X2600 + I2C ICM42688**，正常链路是：

```text
ICM42688
  ↓ I2C
X2600 I2C 控制器
  ↓
Linux I2C core
  ↓
ICM42688 IIO 驱动
  ↓
IIO core
  ↓
/sys/bus/iio/devices/iio:deviceX
  ↓
你的应用程序
```

---

# 2. IIO 设备起来后，系统里长什么样？

驱动正常 probe 后，系统里会出现：

```bash
/sys/bus/iio/devices/iio:device0
/sys/bus/iio/devices/iio:device1
...
```

如果支持 trigger，还可能有：

```bash
/sys/bus/iio/devices/trigger0
/sys/bus/iio/devices/trigger1
...
```

IIO 官方文档里说明，一个 IIO device 通常对应一个硬件传感器；常见属性包括 `name`、`dev`、`sampling_frequency_available` 等。IIO channel 则表示一个数据通道，比如温度、光照，或者加速度计的 X/Y/Z 三轴。([Linux Kernel 文档][1])

比如 IMU 可能有这些通道：

```text
in_accel_x_raw
in_accel_y_raw
in_accel_z_raw

in_anglvel_x_raw
in_anglvel_y_raw
in_anglvel_z_raw

in_temp_raw
in_accel_scale
in_anglvel_scale
sampling_frequency
```

Linux ABI 文档里也列出了 IIO 的常见 sysfs 属性，例如 `sampling_frequency`、`sampling_frequency_available`、`in_accel_x_raw`、`in_accel_y_raw`、`in_accel_z_raw`、`in_anglvel_x_raw` 等。([Linux Kernel 文档][2])

---

# 3. 第一步：先找 IIO 设备

板子启动后，先执行：

```bash
ls /sys/bus/iio/devices/
```

可能看到：

```text
iio:device0
iio:device1
trigger0
```

然后看每个设备名字：

```bash
for d in /sys/bus/iio/devices/iio:device*; do
    echo "===== $d ====="
    cat $d/name 2>/dev/null
done
```

可能输出：

```text
===== /sys/bus/iio/devices/iio:device0 =====
icm42688
```

或者：

```text
icm42688_accel
icm42688_gyro
```

不同驱动暴露方式不完全一样，有些驱动把 accel 和 gyro 放在同一个 `iio:deviceX`，有些可能拆成多个设备。

---

# 4. 第二步：看这个 IIO 设备有哪些属性

进入设备目录：

```bash
cd /sys/bus/iio/devices/iio:device0
ls
```

你重点看这些：

```text
name
dev
sampling_frequency
sampling_frequency_available

in_accel_x_raw
in_accel_y_raw
in_accel_z_raw
in_accel_scale

in_anglvel_x_raw
in_anglvel_y_raw
in_anglvel_z_raw
in_anglvel_scale

in_temp_raw
in_temp_scale
in_temp_offset

buffer/
scan_elements/
trigger/
```

不是每个驱动都有全部文件。**以实际 `ls` 出来的为准。**

---

# 5. 第三步：低速读取 raw 数据

最简单的读取方式：

```bash
cat in_accel_x_raw
cat in_accel_y_raw
cat in_accel_z_raw
```

比如输出：

```text
120
-80
16320
```

这三个值不是直接的物理单位，而是芯片原始值。

你还要读 scale：

```bash
cat in_accel_scale
```

假设输出：

```text
0.000598
```

实际加速度换算：

```text
accel_x = in_accel_x_raw * in_accel_scale
accel_y = in_accel_y_raw * in_accel_scale
accel_z = in_accel_z_raw * in_accel_scale
```

例如：

```text
16320 * 0.000598 ≈ 9.75 m/s²
```

这就接近 1g。

---

# 6. 加速度静态测试方法

把板子平放在桌子上，然后读：

```bash
cat in_accel_x_raw
cat in_accel_y_raw
cat in_accel_z_raw
cat in_accel_scale
```

正常现象：

```text
某一个轴接近 ±1g
另外两个轴接近 0g
```

比如 Z 轴朝上时：

```text
X ≈ 0
Y ≈ 0
Z ≈ +9.8 m/s² 或 -9.8 m/s²
```

正负号取决于 IMU 安装方向和驱动坐标定义。

如果三个轴都接近 0，说明可能：

```text
传感器没启动
power mode 没开
ODR 没配置
驱动读错设备
芯片还在 sleep
```

如果三个轴乱跳很大，可能：

```text
电源噪声
I2C 通信异常
量程/滤波配置不对
机械振动
焊接问题
```

---

# 7. 陀螺仪测试方法

读角速度：

```bash
cat in_anglvel_x_raw
cat in_anglvel_y_raw
cat in_anglvel_z_raw
cat in_anglvel_scale
```

静止时，陀螺仪三轴应该接近 0。

然后你手动绕某个轴旋转板子：

```text
绕 X 轴转，in_anglvel_x_raw 明显变化
绕 Y 轴转，in_anglvel_y_raw 明显变化
绕 Z 轴转，in_anglvel_z_raw 明显变化
```

如果轴对应不上，可能不是驱动错，而是：

```text
IMU 芯片在 PCB 上的方向
设备树 mounting matrix
应用层坐标系转换
```

---

# 8. 采样率怎么查看和设置？

先看当前采样率：

```bash
cat sampling_frequency
```

看支持哪些采样率：

```bash
cat sampling_frequency_available
```

设置采样率：

```bash
echo 100 > sampling_frequency
```

或者：

```bash
echo 200 > sampling_frequency
```

有些驱动可能不是统一的 `sampling_frequency`，而是分成：

```text
in_accel_sampling_frequency
in_anglvel_sampling_frequency
```

所以你要先 `ls` 看实际文件。

注意：`echo 100 > sampling_frequency` 不一定代表真的成功。写完后要再读一次：

```bash
cat sampling_frequency
```

因为驱动可能会把你写的值四舍五入到芯片支持的最近档位。

---

# 9. sysfs 低速读取适合什么？

适合：

```text
调试阶段
工厂测试
偶尔读取一次传感器
低频状态检测
确认 raw/scale/ODR 是否正常
```

不适合：

```text
100Hz 以上连续姿态解算
高频 IMU 数据采集
稳定时间戳采样
长时间高速读取
```

不要在正式应用里这样高频读：

```bash
while true; do
    cat in_accel_x_raw
    cat in_accel_y_raw
    cat in_accel_z_raw
done
```

这样开销大、时间不准、容易抖。

正式高频读取要用 IIO buffer。

---

# 10. 第四步：理解 IIO buffer

IIO buffer 是用来做连续采样的。

普通 sysfs 读取是：

```text
应用 cat raw
  ↓
驱动临时读一次寄存器
  ↓
返回一个值
```

buffer 读取是：

```text
传感器按固定频率采样
  ↓
data ready / FIFO watermark / trigger 触发
  ↓
驱动把一帧数据放进 IIO buffer
  ↓
应用从 /dev/iio:deviceX 批量读取
```

Linux 官方文档说明，IIO core 支持基于 trigger source 的连续数据采集，并且可以从 `/dev/iio:deviceX` 一次读取多个通道数据，从而降低 CPU 负载。([Linux Kernel 文档][3])

对于 ICM42688 这种 IMU，高频读取建议：

```text
ICM42688 FIFO
  ↓
INT1 中断
  ↓
IIO trigger
  ↓
IIO buffer
  ↓
/dev/iio:deviceX
  ↓
应用 read/poll
```

---

# 11. 第五步：查看 buffer 和 scan_elements

进入 IIO 设备目录：

```bash
cd /sys/bus/iio/devices/iio:device0
ls buffer
ls scan_elements
```

你可能看到：

```text
buffer/enable
buffer/length
buffer/watermark
```

以及：

```text
scan_elements/in_accel_x_en
scan_elements/in_accel_x_index
scan_elements/in_accel_x_type

scan_elements/in_accel_y_en
scan_elements/in_accel_y_index
scan_elements/in_accel_y_type

scan_elements/in_accel_z_en
scan_elements/in_accel_z_index
scan_elements/in_accel_z_type

scan_elements/in_timestamp_en
scan_elements/in_timestamp_index
scan_elements/in_timestamp_type
```

`scan_elements` 的作用是：**决定 buffer 里一帧数据包含哪些通道，以及这些通道的数据格式。**

官方文档说明，`scan_elements` 目录里的 `*_en` 用于启用通道，`*_index` 表示该通道在一帧数据中的顺序，`*_type` 描述从用户态读取 buffer 时该通道的数据存储格式，例如大小端、有符号/无符号、有效 bit、存储 bit、shift 等。([Linux Kernel 文档][3])

---

# 12. buffer 使用标准流程

标准顺序：

```text
关闭 buffer
  ↓
选择 scan_elements
  ↓
设置 buffer length
  ↓
设置 trigger，如果驱动需要
  ↓
打开 buffer
  ↓
从 /dev/iio:deviceX 读取二进制数据
  ↓
关闭 buffer
```

命令示例：

```bash
cd /sys/bus/iio/devices/iio:device0

# 1. 先关闭 buffer
echo 0 > buffer/enable

# 2. 开启你想采的通道
echo 1 > scan_elements/in_accel_x_en
echo 1 > scan_elements/in_accel_y_en
echo 1 > scan_elements/in_accel_z_en

# 有 timestamp 的话建议打开
echo 1 > scan_elements/in_timestamp_en

# 3. 设置 buffer 长度
echo 128 > buffer/length

# 4. 打开 buffer
echo 1 > buffer/enable

# 5. 读取数据
cat /dev/iio:device0 > /tmp/imu.bin

# 6. 停止时关闭 buffer
echo 0 > buffer/enable
```

注意：
`/dev/iio:device0` 里面是**二进制数据**，不是文本。你用 `cat` 只是为了确认有数据流，不是为了直接看内容。

---

# 13. 为什么要先关闭 buffer 再改 scan_elements？

这是非常重要的注意事项。

修改这些配置前，建议先：

```bash
echo 0 > buffer/enable
```

然后再改：

```text
scan_elements/*_en
buffer/length
trigger/current_trigger
sampling_frequency
```

原因是：buffer 已经打开时，数据帧格式已经确定。如果你边采集边改通道，用户态解析会乱。

正确流程：

```text
buffer/enable = 0
配置通道
配置长度
配置 trigger
buffer/enable = 1
开始 read
```

---

# 14. 怎么看一帧数据的格式？

查看所有开启通道：

```bash
for f in scan_elements/*_en; do
    echo "$f = $(cat $f)"
done
```

查看 index：

```bash
for f in scan_elements/*_index; do
    echo "$f = $(cat $f)"
done
```

查看 type：

```bash
for f in scan_elements/*_type; do
    echo "$f = $(cat $f)"
done
```

比如你可能看到：

```text
scan_elements/in_accel_x_type = le:s16/16>>0
scan_elements/in_accel_y_type = le:s16/16>>0
scan_elements/in_accel_z_type = le:s16/16>>0
scan_elements/in_timestamp_type = le:s64/64>>0
```

解释：

```text
le       little endian，小端
s16/16   有符号 16 位，有效 16 位，占用 16 位
s64/64   有符号 64 位，占用 64 位
>>0      不需要右移
```

如果看到：

```text
le:s12/16>>4
```

意思是：

```text
小端
有符号
有效 12 bit
实际占用 16 bit
读取后需要右移 4 bit
```

官方文档也给了类似例子：`le:s12/16>>4` 表示用户态要按 2 字节小端有符号数据解释，并在屏蔽有效 12 bit 之前右移 4 bit。([Linux Kernel 文档][3])

---

# 15. `/dev/iio:deviceX` 不存在怎么办？

先看：

```bash
cat /sys/bus/iio/devices/iio:device0/dev
```

可能输出：

```text
250:0
```

这个是字符设备的：

```text
major:minor
```

如果 rootfs 没有自动创建设备节点，可以手动：

```bash
mknod /dev/iio:device0 c 250 0
```

实际数字用你自己 `cat dev` 看到的。

如果系统有 `udev` 或 `mdev`，通常会自动创建 `/dev/iio:device0`。

---

# 16. trigger 是什么？什么时候需要？

trigger 是“什么时候采一帧数据”的触发源。

常见 trigger：

```text
IMU data ready 中断
FIFO watermark 中断
GPIO 中断
定时器
hrtimer
软件 trigger
```

官方文档说明，trigger 用于根据外部事件捕获数据，而不是周期轮询；trigger 可以来自传感器自己的 data ready/threshold 事件，也可以来自独立 GPIO、定时器或用户态写 sysfs 文件。([Linux Kernel 文档][4])

查看 trigger：

```bash
ls /sys/bus/iio/devices/trigger*
```

查看 trigger 名字：

```bash
for t in /sys/bus/iio/devices/trigger*; do
    echo "$t: $(cat $t/name 2>/dev/null)"
done
```

如果当前设备支持 trigger，会有：

```bash
ls /sys/bus/iio/devices/iio:device0/trigger
```

可能有：

```text
current_trigger
```

绑定 trigger：

```bash
cat /sys/bus/iio/devices/trigger0/name
echo "trigger_name" > /sys/bus/iio/devices/iio:device0/trigger/current_trigger
```

官方文档也说明，trigger 注册后会在 `/sys/bus/iio/devices/triggerY/` 下出现，IIO 设备支持 triggered buffer 时，会在 `iio:deviceX/trigger/` 下提供 `current_trigger`，可以把 trigger 名称写进去完成关联。([Linux Kernel 文档][4])

---

# 17. ICM42688 这类 IMU 推荐哪种读取方式？

分阶段：

## 调试阶段

用 sysfs：

```bash
cat in_accel_x_raw
cat in_accel_y_raw
cat in_accel_z_raw
cat in_anglvel_x_raw
cat in_anglvel_y_raw
cat in_anglvel_z_raw
```

目的：

```text
确认驱动起来了
确认三轴数据正常
确认 scale 正常
确认采样率可设置
确认坐标方向
```

## 正式应用阶段

用 buffer：

```text
scan_elements
  ↓
buffer/enable
  ↓
poll/read /dev/iio:deviceX
  ↓
解析二进制帧
  ↓
乘 scale
  ↓
坐标转换
  ↓
滤波/姿态融合
```

原因：

```text
sysfs 适合低速调试
buffer 适合连续高频采样
```

---

# 18. 应用层读取 sysfs 的 C 代码

低速测试可以这样：

```c
#include <stdio.h>
#include <stdlib.h>

static int read_int_file(const char *path, int *value)
{
    FILE *fp = fopen(path, "r");
    if (!fp) {
        perror(path);
        return -1;
    }

    if (fscanf(fp, "%d", value) != 1) {
        fclose(fp);
        return -1;
    }

    fclose(fp);
    return 0;
}

static int read_double_file(const char *path, double *value)
{
    FILE *fp = fopen(path, "r");
    if (!fp) {
        perror(path);
        return -1;
    }

    if (fscanf(fp, "%lf", value) != 1) {
        fclose(fp);
        return -1;
    }

    fclose(fp);
    return 0;
}

int main(void)
{
    const char *base = "/sys/bus/iio/devices/iio:device0";
    char path[256];

    int ax_raw, ay_raw, az_raw;
    double accel_scale;

    snprintf(path, sizeof(path), "%s/in_accel_x_raw", base);
    read_int_file(path, &ax_raw);

    snprintf(path, sizeof(path), "%s/in_accel_y_raw", base);
    read_int_file(path, &ay_raw);

    snprintf(path, sizeof(path), "%s/in_accel_z_raw", base);
    read_int_file(path, &az_raw);

    snprintf(path, sizeof(path), "%s/in_accel_scale", base);
    read_double_file(path, &accel_scale);

    printf("raw: ax=%d ay=%d az=%d\n", ax_raw, ay_raw, az_raw);
    printf("m/s^2: ax=%f ay=%f az=%f\n",
           ax_raw * accel_scale,
           ay_raw * accel_scale,
           az_raw * accel_scale);

    return 0;
}
```

这个适合：

```text
低频读取
工厂测试
确认数据
```

不适合高频姿态解算。

---

# 19. 应用层读取 buffer 的基本思路

正式应用不要用 `cat`，要用 C 程序：

```text
open("/dev/iio:device0")
  ↓
poll()
  ↓
read()
  ↓
按照 scan_elements/*_index 和 *_type 解析
```

伪代码：

```c
int fd = open("/dev/iio:device0", O_RDONLY);

while (1) {
    poll(fd);
    len = read(fd, buf, sizeof(buf));

    /*
     * buf 是二进制数据。
     * 不能直接 printf("%s", buf)。
     *
     * 你必须根据：
     *   scan_elements/*_index
     *   scan_elements/*_type
     * 去解析每一帧。
     */
}
```

实际工程里，你通常启动时先扫描：

```text
哪些通道 enable 了
每个通道 index 是多少
每个通道 type 是什么
一帧总共有多少字节
```

然后按一帧一帧解析。

---

# 20. 一个简化版 buffer 解析例子

假设你已经确认：

```text
in_accel_x_type = le:s16/16>>0
in_accel_y_type = le:s16/16>>0
in_accel_z_type = le:s16/16>>0
in_timestamp_type = le:s64/64>>0
```

并且 index 顺序是：

```text
accel_x index 0
accel_y index 1
accel_z index 2
timestamp index 3
```

那么一帧可能是：

```c
struct imu_sample {
    int16_t ax;
    int16_t ay;
    int16_t az;
    int64_t timestamp;
};
```

但是注意：
真实 IIO buffer 里可能有对齐和 padding，不要盲目假设结构体一定这样。正式代码应该根据 `scan_elements/*_type` 计算。

简化读取代码：

```c
#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <string.h>
#include <errno.h>

struct imu_sample {
    int16_t ax;
    int16_t ay;
    int16_t az;
    int64_t timestamp;
} __attribute__((packed));

int main(void)
{
    int fd = open("/dev/iio:device0", O_RDONLY);
    if (fd < 0) {
        perror("open /dev/iio:device0");
        return -1;
    }

    struct pollfd pfd = {
        .fd = fd,
        .events = POLLIN,
    };

    while (1) {
        int ret = poll(&pfd, 1, 1000);
        if (ret < 0) {
            perror("poll");
            break;
        }

        if (ret == 0) {
            printf("timeout\n");
            continue;
        }

        struct imu_sample samples[16];
        ssize_t n = read(fd, samples, sizeof(samples));
        if (n < 0) {
            perror("read");
            break;
        }

        int count = n / sizeof(struct imu_sample);
        for (int i = 0; i < count; i++) {
            printf("ax=%d ay=%d az=%d ts=%lld\n",
                   samples[i].ax,
                   samples[i].ay,
                   samples[i].az,
                   (long long)samples[i].timestamp);
        }
    }

    close(fd);
    return 0;
}
```

这个代码只是帮助你理解流程。真实项目一定要先确认：

```bash
cat scan_elements/*_type
cat scan_elements/*_index
```

然后再定义数据结构。

---

# 21. IIO 使用完整流程：以 ICM42688 为例

## 阶段 1：确认驱动有没有起来

```bash
dmesg | grep -i iio
dmesg | grep -i icm
dmesg | grep -i inv
dmesg | grep -i i2c
```

看 IIO 设备：

```bash
ls /sys/bus/iio/devices/
```

看名字：

```bash
for d in /sys/bus/iio/devices/iio:device*; do
    echo "$d: $(cat $d/name 2>/dev/null)"
done
```

---

## 阶段 2：确认属性

```bash
cd /sys/bus/iio/devices/iio:device0
ls
```

看有没有：

```text
in_accel_x_raw
in_accel_y_raw
in_accel_z_raw
in_anglvel_x_raw
in_anglvel_y_raw
in_anglvel_z_raw
```

---

## 阶段 3：确认 raw 和 scale

```bash
cat in_accel_x_raw
cat in_accel_y_raw
cat in_accel_z_raw
cat in_accel_scale
```

快速换算：

```bash
AX=$(cat in_accel_x_raw)
SCALE=$(cat in_accel_scale)
awk "BEGIN {print $AX * $SCALE}"
```

---

## 阶段 4：确认采样率

```bash
cat sampling_frequency_available
cat sampling_frequency
echo 100 > sampling_frequency
cat sampling_frequency
```

---

## 阶段 5：确认 buffer 能不能用

```bash
ls buffer
ls scan_elements
```

如果没有 `buffer/` 或 `scan_elements/`，说明这个驱动可能没支持 buffer，或者内核配置没开 IIO buffer。

常见内核配置需要：

```text
CONFIG_IIO
CONFIG_IIO_BUFFER
CONFIG_IIO_TRIGGER
CONFIG_IIO_TRIGGERED_BUFFER
```

具体还要看你的传感器驱动配置。

---

## 阶段 6：配置 buffer

```bash
cd /sys/bus/iio/devices/iio:device0

echo 0 > buffer/enable

echo 1 > scan_elements/in_accel_x_en
echo 1 > scan_elements/in_accel_y_en
echo 1 > scan_elements/in_accel_z_en

# 有 timestamp 就打开
[ -e scan_elements/in_timestamp_en ] && echo 1 > scan_elements/in_timestamp_en

echo 128 > buffer/length

echo 1 > buffer/enable
```

---

## 阶段 7：读二进制数据

```bash
dd if=/dev/iio:device0 of=/tmp/imu.bin bs=256 count=10
```

如果 `/tmp/imu.bin` 大小不是 0，说明 buffer 有数据。

停止：

```bash
echo 0 > buffer/enable
```

---

# 22. 常见问题和排查

## 问题 1：`/sys/bus/iio/devices/` 下面没有 `iio:deviceX`

查：

```bash
dmesg | grep -i iio
dmesg | grep -i icm
dmesg | grep -i i2c
ls /sys/bus/i2c/devices/
```

可能原因：

```text
设备树没写
compatible 写错
I2C 地址错
驱动没开
模块没加载
probe 失败
WHO_AM_I 读失败
I2C 控制器没启用
```

---

## 问题 2：有 `iio:deviceX`，但是没有 `/dev/iio:deviceX`

先看：

```bash
cat /sys/bus/iio/devices/iio:device0/dev
```

然后手动建：

```bash
mknod /dev/iio:device0 c 主设备号 次设备号
```

比如：

```bash
mknod /dev/iio:device0 c 250 0
```

---

## 问题 3：`cat *_raw` 有数据，但 buffer 没数据

查：

```bash
ls buffer
ls scan_elements
cat buffer/enable
cat buffer/length
cat trigger/current_trigger 2>/dev/null
```

可能原因：

```text
没有开启 scan_elements
buffer length 太小
没有绑定 trigger
驱动不支持 buffer
中断没触发
FIFO 没启动
用户态读错 /dev/iio 设备
```

---

## 问题 4：`echo 1 > buffer/enable` 报错

常见原因：

```text
没有选择任何 scan_elements
trigger 没配置
当前 trigger 不合法
设备正忙
驱动内部 start buffer 失败
采样率/通道组合不支持
```

排查：

```bash
dmesg | tail -50
cat scan_elements/*_en
cat trigger/current_trigger 2>/dev/null
```

---

## 问题 5：读出来的数据解析不对

第一反应不要怀疑传感器，先查：

```bash
cat scan_elements/*_index
cat scan_elements/*_type
```

常见错误：

```text
把小端当大端
把有符号当无符号
没处理 shift
没处理 padding
没按 index 顺序解析
timestamp 对齐没算
结构体被编译器自动 padding
```

正式代码里不要随便：

```c
struct imu_sample {
    int16_t ax;
    int16_t ay;
    int16_t az;
    int64_t timestamp;
};
```

除非你已经确认 buffer 布局就是这样。

---

## 问题 6：sysfs 读数正常，高速时丢数据

可能原因：

```text
I2C 速度太低
ODR 太高
用户态 read 不及时
buffer length 太小
FIFO watermark 不合理
系统调度延迟大
应用线程优先级太低
CPU 负载太高
```

优化方向：

```text
提高 I2C 到 400kHz 或 1MHz，前提是硬件支持
使用 FIFO
使用 watermark
增大 buffer/length
应用用 poll/read
降低 ODR
减少 printf
必要时提高线程调度优先级
```

---

## 问题 7：加速度方向不对

这不一定是驱动 bug。

要确认：

```text
芯片封装坐标系
PCB 安装方向
整机坐标系
Linux 驱动是否支持 mount matrix
应用层是否需要坐标转换
```

很多产品里，IMU 芯片的 X/Y/Z 和整机定义的前后左右上下不一致，需要做矩阵变换。

例如：

```text
sensor_x = body_y
sensor_y = -body_x
sensor_z = body_z
```

这个转换可以在：

```text
设备树 mount matrix
驱动里
应用层
```

具体取决于驱动支持情况。

---

# 23. IIO 使用注意事项总结

## 1. sysfs 只适合低速调试

```text
cat *_raw
```

适合确认数据，不适合高频采样。

---

## 2. raw 不是物理量

必须乘 scale：

```text
physical_value = raw * scale
```

有 offset 时：

```text
physical_value = (raw + offset) * scale
```

---

## 3. buffer 是二进制，不是字符串

不要用字符串方式解析：

```c
printf("%s", buf);   // 错
```

应该按 `scan_elements/*_type` 解析。

---

## 4. 改 buffer 配置前先关闭 buffer

正确：

```bash
echo 0 > buffer/enable
# 改 scan_elements / length / trigger
echo 1 > buffer/enable
```

---

## 5. 通道顺序看 index，不要看文件名顺序

不要以为：

```text
ls 显示 x y z
```

就代表 buffer 里一定是 x y z。

要看：

```bash
cat scan_elements/*_index
```

---

## 6. 数据格式看 type

必须看：

```bash
cat scan_elements/*_type
```

尤其注意：

```text
le / be
s / u
realbits / storagebits
shift
repeat
```

---

## 7. 高速 IMU 尽量使用 timestamp

没有 timestamp，你只能靠应用 read 的时间估算采样时间，不够准。

建议开启：

```bash
echo 1 > scan_elements/in_timestamp_en
```

前提是驱动支持。

---

## 8. 高频时少打印

这点很重要。

IMU 100Hz、200Hz、1kHz 时，应用里疯狂：

```c
printf()
```

会严重影响实时性。

建议：

```text
调试时低频打印
正式运行时缓存/统计/降频打印
```

---

## 9. 不要应用和驱动同时抢寄存器

如果 IIO 驱动已经绑定 ICM42688，不要再用：

```bash
i2cget -f
i2cset -f
```

强行访问同一个 I2C 地址。

这可能破坏驱动状态。

---

## 10. 先确认驱动，再写应用

顺序不要反：

```text
先 dmesg
再 /sys/bus/iio/devices
再 raw/scale
再 buffer
最后应用
```

不要 IIO 设备还没出来，就开始写复杂应用。

---

# 24. 推荐你实际调试时照这个 checklist 走

```bash
# 1. 看 IIO 设备
ls /sys/bus/iio/devices/

# 2. 看设备名
for d in /sys/bus/iio/devices/iio:device*; do
    echo "$d: $(cat $d/name 2>/dev/null)"
done

# 3. 进入设备
cd /sys/bus/iio/devices/iio:device0

# 4. 看属性
ls

# 5. 读加速度
cat in_accel_x_raw
cat in_accel_y_raw
cat in_accel_z_raw
cat in_accel_scale

# 6. 读陀螺仪
cat in_anglvel_x_raw
cat in_anglvel_y_raw
cat in_anglvel_z_raw
cat in_anglvel_scale

# 7. 看采样率
cat sampling_frequency
cat sampling_frequency_available

# 8. 看 buffer
ls buffer
ls scan_elements

# 9. 看 scan 格式
cat scan_elements/*_index
cat scan_elements/*_type

# 10. 配置 buffer
echo 0 > buffer/enable
echo 1 > scan_elements/in_accel_x_en
echo 1 > scan_elements/in_accel_y_en
echo 1 > scan_elements/in_accel_z_en
[ -e scan_elements/in_timestamp_en ] && echo 1 > scan_elements/in_timestamp_en
echo 128 > buffer/length
echo 1 > buffer/enable

# 11. 读取二进制数据
dd if=/dev/iio:device0 of=/tmp/imu.bin bs=256 count=10

# 12. 关闭 buffer
echo 0 > buffer/enable
```

---

# 25. 最后给你一句工程经验

IIO 的正确学习顺序是：

```text
先会看 /sys/bus/iio/devices
再会读 raw/scale
再理解 sampling_frequency
再理解 scan_elements
再理解 buffer/enable
再理解 /dev/iio:deviceX 二进制读取
最后再研究 trigger/FIFO/中断
```

对于你的 **ICM42688 IMU**，最实用的路线就是：

```text
sysfs raw/scale 调通基本数据
  ↓
确认三轴方向和单位
  ↓
确认采样率
  ↓
开启 buffer + timestamp
  ↓
应用 poll/read /dev/iio:deviceX
  ↓
解析二进制帧
  ↓
乘 scale
  ↓
坐标转换
  ↓
滤波/姿态融合
```

[1]: https://docs.kernel.org/driver-api/iio/core.html "Core elements — The Linux Kernel  documentation"
[2]: https://docs.kernel.org/admin-guide/abi-testing-files.html "Testing ABI Files — The Linux Kernel  documentation"
[3]: https://docs.kernel.org/driver-api/iio/buffers.html "Buffers — The Linux Kernel  documentation"
[4]: https://docs.kernel.org/driver-api/iio/triggers.html "Triggers — The Linux Kernel  documentation"
