可以，这个思路更对。你先别急着写代码。Linux 下写 **SPI 接口 IMU 的 IIO 驱动**，其实不是单纯“读 SPI 数据”这么简单，它背后牵涉到几层知识：

```text
硬件 IMU 芯片 QMI8658A
        ↓
SPI 总线通信
        ↓
Linux SPI 子系统
        ↓
设备树 DTS 描述硬件连接
        ↓
SPI client driver 匹配设备
        ↓
IIO 子系统注册传感器
        ↓
用户态通过 /sys/bus/iio/devices/iio:deviceX 读数据
```

QMI8658A 本身是一个 6 轴 IMU，包含 3 轴加速度计和 3 轴陀螺仪，主机接口支持 I2C、I3C、3 线 SPI、4 线 SPI。SPI 模式下，主机通过访问芯片内部寄存器来配置量程、ODR、使能加速度计/陀螺仪，并读取 X/Y/Z 轴原始数据。QMI8658A 的 SPI burst read 还涉及 `CTRL1.ADDR_AI` 地址自增位，默认是关闭的，所以连续读多个数据寄存器前通常要打开它。([Waveshare][1])

---

# 1. 先理解：QMI8658A 到底是什么？

QMI8658A 是一个 **IMU**，全称可以理解成：

```text
IMU = Inertial Measurement Unit = 惯性测量单元
```

它里面主要有：

```text
加速度计 Accelerometer
    测量 X/Y/Z 三个方向的加速度

陀螺仪 Gyroscope
    测量 X/Y/Z 三个方向的角速度

温度传感器
    有些 IMU 内部也带温度数据

内部寄存器
    用来配置芯片和读取数据
```

所以 Linux 驱动做的事情，本质上是：

```text
1. 通过 SPI 读 WHO_AM_I，确认芯片真的在
2. 写配置寄存器，让加速度计和陀螺仪工作
3. 读数据寄存器，拿到 ax/ay/az/gx/gy/gz
4. 把这些数据按照 Linux IIO 框架暴露给用户态
```

---

# 2. 再理解：SPI 在这里干什么？

SPI 只是一个通信通道。你可以把 SPI 想成 MCU/Linux SoC 和 QMI8658A 之间的一根“寄存器读写线”。

QMI8658A 里面有很多寄存器，例如：

```text
WHO_AM_I       芯片 ID
CTRL1          SPI/I2C 接口相关配置
CTRL2          加速度计量程、ODR 配置
CTRL3          陀螺仪量程、ODR 配置
CTRL7          传感器使能
AX_L/AX_H      加速度 X 轴低/高字节
AY_L/AY_H      加速度 Y 轴低/高字节
AZ_L/AZ_H      加速度 Z 轴低/高字节
GX_L/GX_H      陀螺仪 X 轴低/高字节
...
```

所以 SPI 驱动不是“凭空读出加速度”，而是不断做这种事：

```text
读寄存器 0x00  → 看 WHO_AM_I 是不是正确
写寄存器 0x03  → 配置加速度计
写寄存器 0x04  → 配置陀螺仪
写寄存器 0x08  → 使能 accel/gyro
读寄存器 0x35~0x40 → 拿到 6 轴原始数据
```

在 QMI8658A 的 SPI 协议里，读写寄存器时一般要根据最高位区分读写，读操作需要在寄存器地址上加读标志，这一点驱动里通常用 `regmap` 的 `read_flag_mask = 0x80` 来处理。QMI8658A 数据手册也说明 SPI 方式下主机通过 Host Serial Interface 访问、设置和控制 UI registers。([Waveshare][1])

---

# 3. 为什么不用普通字符设备，非要用 IIO？

因为 IMU 属于 Linux 里的“采样型传感器”。Linux 专门有一个子系统叫 **IIO**：

```text
IIO = Industrial I/O
```

它主要管理这些东西：

```text
ADC
DAC
加速度计
陀螺仪
磁力计
温度传感器
光照传感器
压力传感器
电流/电压采样芯片
```

IIO 的好处是：你不用自己随便定义 `/dev/qmi8658` 这种私有接口，而是按照 Linux 标准方式暴露传感器数据。

比如最终用户态可以看到：

```text
/sys/bus/iio/devices/iio:device0/name
/sys/bus/iio/devices/iio:device0/in_accel_x_raw
/sys/bus/iio/devices/iio:device0/in_accel_y_raw
/sys/bus/iio/devices/iio:device0/in_accel_z_raw
/sys/bus/iio/devices/iio:device0/in_accel_scale
/sys/bus/iio/devices/iio:device0/in_anglvel_x_raw
/sys/bus/iio/devices/iio:device0/in_anglvel_y_raw
/sys/bus/iio/devices/iio:device0/in_anglvel_z_raw
/sys/bus/iio/devices/iio:device0/in_anglvel_scale
```

Linux IIO 官方文档说明，一个 IIO device 通常对应一个硬件传感器，并通过 `/sys/bus/iio/devices/iio:deviceX/` 给用户态提供访问入口。([Linux内核文档][2])

---

# 4. IIO 里面几个关键概念

## 4.1 IIO device

这个对应一个真实芯片。

比如你的 QMI8658A 注册成功后，内核里就是一个：

```text
iio_dev
```

用户态看到：

```text
/sys/bus/iio/devices/iio:device0/
```

这个 `iio:device0` 就代表 QMI8658A 这个 IMU。

---

## 4.2 IIO channel

一个 IMU 有多个通道。

QMI8658A 至少有这些 channel：

```text
加速度 X 轴：in_accel_x_raw
加速度 Y 轴：in_accel_y_raw
加速度 Z 轴：in_accel_z_raw

陀螺仪 X 轴：in_anglvel_x_raw
陀螺仪 Y 轴：in_anglvel_y_raw
陀螺仪 Z 轴：in_anglvel_z_raw
```

也就是说，IIO 不是把整个 IMU 当成一个“大数据包”，而是把它拆成一个个标准通道。

---

## 4.3 raw 和 scale

`raw` 是芯片寄存器里读出来的原始值。

比如：

```text
in_accel_z_raw = 8192
```

但是这个 8192 不是直接等于 `8192 m/s²`。

它需要乘以 scale：

```text
实际物理值 = raw * scale
```

IIO 用户态文档里也提到，IIO 通常用 `raw`、`offset`、`scale` 这类属性完成从原始值到标准物理量的转换。([STMicroelectronics][3])

比如：

```text
in_accel_z_raw   = 8192
in_accel_scale   = 0.001197

实际加速度 ≈ 8192 * 0.001197
           ≈ 9.8 m/s²
```

如果板子平放，某一个轴接近 `9.8 m/s²` 是正常的，因为地球重力加速度就在那。

---

## 4.4 direct mode 和 buffer mode

IIO 有两种常见用法。

第一种是 **direct mode**：

```text
用户态 cat in_accel_x_raw
驱动临时读一次寄存器
返回一个值
```

适合：

```text
低频读取
调试
验证芯片是否正常
简单应用
```

第二种是 **buffer mode**：

```text
IMU 按固定频率采样
中断触发
驱动批量读取数据
数据进入 IIO buffer
用户态从 /dev/iio:deviceX 连续读
```

适合：

```text
高频 IMU 数据
姿态解算
运动检测
数据记录
低 CPU 占用
```

IIO buffer 文档说明，IIO core 提供基于 trigger 的连续数据采集机制，可以一次读取多个通道，并通过 `/dev/iio:deviceX` 字符设备降低 CPU 负载。([DRI Wiki][4])

所以新手顺序应该是：

```text
第一阶段：direct mode，先能 cat raw 数据
第二阶段：triggered buffer，做连续采样
第三阶段：FIFO + 中断，优化性能
```

不要一上来就搞 FIFO 和中断，不然问题太多，不知道是 SPI 错、设备树错、IIO 错，还是 FIFO 配错。

---

# 5. 设备树 DTS 是干什么的？

Linux 驱动不像 MCU 裸机那样，直接在代码里写：

```c
SPI0
CS0
GPIOxx
```

Linux 更推荐通过 **设备树** 描述硬件。

设备树告诉内核：

```text
这个板子上启用了哪个 SPI 控制器
这个 SPI 控制器下面挂了哪个外设
这个外设用哪个 CS
最高 SPI 频率是多少
用 SPI mode 0 还是 mode 3
中断脚接到哪个 GPIO
电源 regulator 是哪个
```

SPI 设备树绑定文档里也说明，SPI 总线下面会有 SPI master 节点，外设作为 SPI bus 的子节点出现，并通过 `reg` 这类属性描述片选号。([Linux内核官网][5])

可以理解成：

```text
设备树负责“描述硬件”
驱动负责“控制硬件”
```

你写驱动时，不应该把板子连接信息写死在驱动代码里。

---

# 6. SPI controller driver 和 SPI device driver 的区别

这个地方新手很容易混。

## 6.1 SPI controller driver

这是 SoC 自己的 SPI 控制器驱动。

比如君正 X2600 里面的 SPI 控制器，Linux 内核可能已经有对应驱动。

它负责：

```text
配置 SPI 控制器寄存器
控制 SCLK
控制 MOSI/MISO
控制 CS
发起 SPI transfer
支持 DMA 或 PIO
```

这个驱动一般是 SoC 厂商已经写好的。

你通常不用写。

---

## 6.2 SPI device driver

这是你要写的 QMI8658A 驱动。

它负责：

```text
知道 QMI8658A 的寄存器地址
知道 WHO_AM_I 是多少
知道怎么配置 accel/gyro
知道怎么读取 X/Y/Z 数据
知道怎么注册 IIO 设备
```

关系是：

```text
qmi8658a_iio_driver
        ↓ 调用
spi_sync / regmap_read / regmap_write
        ↓ 进入
SoC SPI controller driver
        ↓ 控制硬件 SPI 波形
QMI8658A
```

所以你写的是 **SPI 外设驱动**，不是 SPI 控制器驱动。

---

# 7. regmap 是干什么的？

`regmap` 是 Linux 内核提供的一套“寄存器访问封装”。

不用 regmap 的话，你要自己写：

```text
spi_write_then_read()
拼读命令
拼写命令
处理读写标志位
处理 bulk read
处理锁
```

用了 regmap 之后，你可以更像这样理解：

```text
regmap_read(寄存器地址)
regmap_write(寄存器地址, 值)
regmap_bulk_read(起始寄存器, buffer, 长度)
```

对于 QMI8658A 这种“寄存器型 SPI 传感器”，用 regmap 非常合适。

尤其是 QMI8658A 的 SPI 读操作需要读标志位，regmap 可以通过类似：

```text
read_flag_mask = 0x80
```

帮你自动处理。

---

# 8. 一个 QMI8658A IIO 驱动内部大概长什么样？

不要看代码，先看结构。

一个最小驱动通常由这些部分组成：

```text
1. 寄存器地址定义
2. 私有数据结构 struct qmi8658_data
3. regmap 配置
4. IIO channel 定义
5. read_raw 回调函数
6. 芯片初始化函数
7. probe 函数
8. 设备树 compatible 匹配表
9. spi_driver 注册
10. Kconfig / Makefile
```

它们的关系大概是：

```text
内核启动
  ↓
解析设备树
  ↓
发现 spi0 下面有 compatible = "qst,qmi8658a"
  ↓
匹配到 qmi8658a SPI driver
  ↓
调用 probe()
  ↓
probe() 里初始化 regmap
  ↓
读 WHO_AM_I
  ↓
配置 CTRL 寄存器
  ↓
注册 iio_dev
  ↓
用户态出现 /sys/bus/iio/devices/iio:deviceX
```

---

# 9. 整体大步骤

你可以按这 7 个阶段理解。

## 阶段 1：确认硬件连接

先确认：

```text
QMI8658A 是接 SPI 还是 I2C
用的是 3 线 SPI 还是 4 线 SPI
接在哪个 SPI 控制器上
用哪个 CS
INT1/INT2 有没有接
VDD/VDDIO 电压是多少
SPI mode 是 mode 0 还是 mode 3
```

QMI8658A 支持 3-wire 和 4-wire SPI，但新手建议先用 **4-wire SPI**，因为 MOSI/MISO 分开，调试简单。([Waveshare][1])

---

## 阶段 2：确认 Linux SPI 控制器能工作

你要先确认 SoC 的 SPI 控制器本身没问题。

比如：

```text
SPI 控制器驱动有没有启用
设备树 pinctrl 对不对
CS 有没有拉低
SCLK 有没有波形
MOSI 有没有输出
MISO 有没有返回
```

如果 SPI 控制器本身都没波形，那你写 QMI8658A 驱动没意义。

---

## 阶段 3：设备树添加 QMI8658A 节点

设备树里需要告诉 Linux：

```text
spi0 下面挂了 qmi8658a
它用 CS0
最大 SPI 频率先设置低一点
compatible 是 qst,qmi8658a
```

设备树的作用不是驱动逻辑，而是“把硬件连接关系交给内核”。

---

## 阶段 4：写最小 SPI + IIO 驱动

第一版驱动只做这些：

```text
probe()
  ↓
初始化 regmap
  ↓
读取 WHO_AM_I
  ↓
配置 CTRL1/CTRL2/CTRL3/CTRL7
  ↓
注册 IIO device
  ↓
支持 read_raw()
```

第一版不要做：

```text
不要做 FIFO
不要做中断
不要做 buffer
不要做姿态解算
不要做校准
不要做复杂电源管理
```

因为你现在的目标只有一个：

```text
/sys/bus/iio/devices/iio:deviceX/in_accel_x_raw 能读出变化
```

---

## 阶段 5：编译进内核或者编译成 ko

有两种方式：

```text
方式 1：编译成内核模块 .ko
    适合调试
    修改快
    insmod/rmmod 方便

方式 2：编译进内核
    适合产品
    开机自动加载
```

新手建议先用 `.ko`。

---

## 阶段 6：用户态验证

驱动注册成功后，用户态应该看到：

```text
/sys/bus/iio/devices/iio:device0/name
/sys/bus/iio/devices/iio:device0/in_accel_x_raw
/sys/bus/iio/devices/iio:device0/in_accel_y_raw
/sys/bus/iio/devices/iio:device0/in_accel_z_raw
/sys/bus/iio/devices/iio:device0/in_anglvel_x_raw
...
```

验证方式：

```text
静止放桌上：
    某一个加速度轴接近 ±9.8 m/s²
    其他两个轴接近 0

旋转板子：
    陀螺仪 raw 数据有明显变化

晃动板子：
    加速度 raw 数据变化明显
```

---

## 阶段 7：升级成中断 + buffer

等第一版能正常读 raw 之后，再升级：

```text
1. 加 DRDY 中断
2. 加 IIO trigger
3. 加 IIO buffer
4. 用户态从 /dev/iio:deviceX 连续读取
5. 最后再考虑 FIFO
```

这时才适合做姿态解算、运动检测、数据记录。

---

# 10. 更具体一点：实际开发顺序

你真正干活时，推荐按这个顺序来：

## 第一步：先看原理图

你要从原理图确认：

```text
QMI8658A_CS   接 SoC 哪个 GPIO / SPI CS
QMI8658A_SCLK 接 SoC 哪个 SPI_CLK
QMI8658A_SDI  接 SoC MOSI
QMI8658A_SDO  接 SoC MISO
QMI8658A_INT1 接没接
QMI8658A_INT2 接没接
VDD/VDDIO     是 1.8V 还是 3.3V
```

这一步很重要。很多驱动问题最后发现是：

```text
MOSI/MISO 接反
CS 配错
pinctrl 配错
电压域不对
SPI mode 不对
```

---

## 第二步：确认 SDK 里有没有现成驱动

在你的 Linux SDK 里搜：

```text
qmi8658
qmi8658a
qmi8658c
qst
imu
```

如果已经有现成驱动，就先看它是不是：

```text
I2C 版本
SPI 版本
input 子系统版本
IIO 子系统版本
私有 misc 设备版本
```

如果是 I2C 版本，可以参考寄存器配置逻辑，但 SPI 读写部分要改。

---

## 第三步：确认 SPI 控制器设备树已经启用

你需要确认你的板级 DTS 里面：

```text
spi 控制器 status = "okay"
pinctrl 配好了
CS 配好了
时钟、电源没有问题
```

这一步是为了让 Linux 先能驱动 SoC 自己的 SPI 控制器。

---

## 第四步：先用 spidev 或临时测试程序读 WHO_AM_I

虽然产品里不建议长期用 spidev 访问 IMU，但 bring up 阶段可以用它确认硬件通不通。

目标只有一个：

```text
读 QMI8658A WHO_AM_I
```

如果 WHO_AM_I 都读不对，就不要继续写 IIO。

先排查：

```text
SPI mode
CS
频率
供电
MOSI/MISO
地址读写格式
```

---

## 第五步：开始写最小 IIO 驱动

驱动第一版只支持：

```text
in_accel_x_raw
in_accel_y_raw
in_accel_z_raw
in_anglvel_x_raw
in_anglvel_y_raw
in_anglvel_z_raw
scale
```

内部流程：

```text
probe()
    分配 iio_dev
    初始化 regmap
    读取 WHO_AM_I
    软件复位，可选
    写 CTRL1，开启地址自增
    写 CTRL2，配置 accel 量程/ODR
    写 CTRL3，配置 gyro 量程/ODR
    写 CTRL7，使能 accel/gyro
    注册 iio_dev

read_raw()
    根据 channel 判断读哪个寄存器
    regmap_bulk_read() 读 2 字节
    合成 s16
    返回给 IIO core
```

---

## 第六步：用户态验证 raw 数据

你应该先验证：

```text
cat name
cat in_accel_x_raw
cat in_accel_y_raw
cat in_accel_z_raw
cat in_anglvel_x_raw
cat in_anglvel_y_raw
cat in_anglvel_z_raw
```

判断是否正常：

```text
静止时：
    陀螺仪应该接近 0
    加速度某一轴应该有重力

晃动时：
    加速度变化明显

旋转时：
    陀螺仪变化明显
```

---

## 第七步：补充 scale、sampling_frequency、量程配置

第一版只读 raw。

第二版可以补：

```text
in_accel_scale
in_anglvel_scale
sampling_frequency
可配置量程
可配置 ODR
```

这样用户态就不只是读原始值，还能知道如何换算成真实物理单位。

---

## 第八步：加中断

如果你的硬件接了 INT1 或 INT2，可以做 DRDY 中断。

中断版大概是：

```text
IMU 采样完成
    ↓
INT1/INT2 拉中断
    ↓
Linux IRQ handler 触发
    ↓
IIO trigger handler 读取 6 轴数据
    ↓
push 到 IIO buffer
```

---

## 第九步：加 IIO buffer

buffer 适合高频连续数据。

最终用户态可以：

```text
打开 scan_elements
设置 buffer/length
enable buffer
从 /dev/iio:deviceX 读取连续数据
```

这个阶段才适合做：

```text
姿态算法
轨迹记录
实时运动检测
连续采样
```

---

# 11. 新手最容易混的几个点

## 误区 1：以为 SPI 驱动就是 IMU 驱动

不是。

```text
SPI 控制器驱动：控制 SoC 的 SPI 硬件
QMI8658A 驱动：知道怎么操作 QMI8658A 芯片
```

你要写的是后者。

---

## 误区 2：以为读到 raw 就是实际加速度

不是。

```text
raw 只是 ADC/传感器内部的数字量
真实物理值 = raw * scale
```

---

## 误区 3：一上来就做中断和 FIFO

不要。

正确顺序是：

```text
WHO_AM_I
    ↓
单次 raw 读取
    ↓
scale 换算
    ↓
中断
    ↓
buffer
    ↓
FIFO
```

---

## 误区 4：设备树 compatible 随便写

不能随便写。

设备树里：

```text
compatible = "qst,qmi8658a";
```

驱动里也要有对应匹配：

```text
.of_match_table = qmi8658_of_match
```

两边对不上，`probe()` 根本不会进。

---

## 误区 5：以为 cat sysfs 适合高频读取

不适合。

```text
cat in_accel_x_raw
```

适合调试，不适合高频 IMU 应用。

高频场景应该用：

```text
IIO buffer + /dev/iio:deviceX
```

---

# 12. 你现在应该建立的心智模型

你可以把整个事情理解成这样：

```text
QMI8658A 是一个“寄存器设备”
SPI 是“读写寄存器的总线”
设备树是“告诉 Linux 它接在哪里”
SPI driver probe 是“Linux 找到了这个设备”
regmap 是“简化寄存器读写”
IIO 是“把传感器数据标准化暴露给用户态”
sysfs 是“低频调试入口”
buffer 是“高频连续采样入口”
```

最小成功标准不是“驱动写得很完整”，而是：

```text
1. probe 能进
2. WHO_AM_I 能读对
3. IIO device 能注册出来
4. in_accel_x_raw 能读
5. 晃动板子 raw 数据会变化
```

只要这 5 个成立，说明主链路通了。

---

# 13. 推荐学习路线

你可以按这个顺序学：

```text
第一天：
    理解 QMI8658A 寄存器
    理解 SPI 读写寄存器
    理解 WHO_AM_I

第二天：
    理解 Linux 设备树
    理解 SPI controller 和 SPI device 的区别

第三天：
    理解 IIO device / channel / raw / scale

第四天：
    写最小 probe + WHO_AM_I

第五天：
    加 read_raw，能 cat 6 轴数据

第六天：
    加 scale / sampling_frequency

第七天以后：
    再考虑中断、buffer、FIFO
```

对于你现在的新手阶段，最重要的一句话是：

**先把 QMI8658A 当成“一个 SPI 寄存器芯片”读通，再把它包装成 Linux IIO 传感器。**

[1]: https://files.waveshare.com/upload/5/5f/QMI8658A_Datasheet_Rev_A.pdf?utm_source=chatgpt.com "QMI8658A Datasheet"
[2]: https://docs.kernel.org/driver-api/iio/core.html?utm_source=chatgpt.com "Core elements"
[3]: https://wiki.st.com/stm32mpu/wiki/How_to_use_the_IIO_user_space_interface?utm_source=chatgpt.com "How to use the IIO user space interface - stm32mpu - ST wiki"
[4]: https://dri.freedesktop.org/docs/drm/iio/iio_devbuf.html?utm_source=chatgpt.com "Industrial IIO device buffers — The Linux Kernel ..."
[5]: https://www.kernel.org/doc/Documentation/devicetree/bindings/spi/spi-controller.yaml?utm_source=chatgpt.com "spi-controller.yaml"


-------
------
------


下面给你一个**从 0 到能读数**的教程。建议你先做“**轮询读取 raw 数据的最小 IIO 驱动**”，先别上 FIFO、DRDY 中断、buffer。等 `in_accel_x_raw`、`in_anglvel_z_raw` 能稳定变化后，再加中断/FIFO。

## 1. 整体架构先搞清楚

QMI8658A 是 6 轴 IMU，内部有 3 轴加速度计 + 3 轴陀螺仪，主机接口支持 I2C、I3C、3-wire SPI、4-wire SPI。SPI 最大频率 15 MHz，支持 SPI Mode 0 和 Mode 3，默认是 4 线 SPI。([Waveshare][1])

Linux 下推荐走 **IIO 子系统**，因为 IMU、ADC、温度传感器、光照传感器这类“采样型设备”都适合放到 IIO。IIO 设备通常会在用户态暴露成 `/sys/bus/iio/devices/iio:deviceX/`，也可能有 `/dev/iio:deviceX` 用于 buffer 连续采样。([Linux内核官网][2])

本教程先实现这个路径：

```text
QMI8658A 硬件
   ↓ SPI
Linux SPI controller driver
   ↓ spi_driver probe
QMI8658A IIO driver
   ↓
/sys/bus/iio/devices/iio:deviceX/
   ├── in_accel_x_raw
   ├── in_accel_y_raw
   ├── in_accel_z_raw
   ├── in_accel_scale
   ├── in_anglvel_x_raw
   ├── in_anglvel_y_raw
   ├── in_anglvel_z_raw
   └── in_anglvel_scale
```

---

## 2. 硬件连接

先用 **4 线 SPI**，不要一开始搞 3 线 SPI。

```text
Linux SoC SPI_MOSI  ---> QMI8658A SDI
Linux SoC SPI_MISO  <--- QMI8658A SDO
Linux SoC SPI_CLK   ---> QMI8658A SPC
Linux SoC SPI_CS    ---> QMI8658A CS
GND                 ---> GND
VDD / VDDIO          ---> 按你的板子设计供电
INT1/INT2            ---> 可先不接，后面做中断再接
```

QMI8658A 的 SPI 地址格式是 **bit7 表示读写方向，bit6~bit0 是寄存器地址**。读操作 bit7 = 1，写操作 bit7 = 0。

所以裸 SPI 读 WHO_AM_I 时，本质是：

```c
tx[0] = 0x80 | 0x00;   // 读 0x00 寄存器
```

WHO_AM_I 寄存器地址是 `0x00`，期望值是 `0x05`。([Waveshare][1])

---

## 3. 设备树 DTS 配置

假设你的 IMU 接在 `spi0` 的 CS0 上，设备树可以这样写：

```dts
&spi0 {
    status = "okay";

    /*
     * pinctrl-0 这里要换成你平台自己的 SPI 引脚组。
     * 不同 SoC 写法不同，比如君正、全志、瑞芯微、STM32 都不一样。
     */
    pinctrl-names = "default";
    pinctrl-0 = <&spi0_pins>;

    qmi8658a@0 {
        compatible = "qst,qmi8658a";
        reg = <0>;                         /* CS0 */
        spi-max-frequency = <1000000>;     /* 先用 1MHz，调通后再升到 8MHz/10MHz */
        status = "okay";

        /*
         * 默认就是 SPI mode 0：CPOL=0, CPHA=0
         * 所以这里不写 spi-cpol/spi-cpha。
         *
         * 如果你硬件/逻辑分析仪确认要用 mode 3，再加：
         * spi-cpol;
         * spi-cpha;
         */
    };
};
```

先用 `1MHz` 是为了 bring up 稳一点。QMI8658A 最大 SPI 频率是 15 MHz，但刚调驱动时别一上来就跑满。([Waveshare][1])

---

## 4. QMI8658A 关键寄存器

最小驱动只需要这些寄存器：

```c
#define QMI8658_REG_WHO_AM_I     0x00
#define QMI8658_REG_CTRL1        0x02
#define QMI8658_REG_CTRL2        0x03
#define QMI8658_REG_CTRL3        0x04
#define QMI8658_REG_CTRL5        0x06
#define QMI8658_REG_CTRL7        0x08
#define QMI8658_REG_STATUS0      0x2E

#define QMI8658_REG_AX_L         0x35
#define QMI8658_REG_AY_L         0x37
#define QMI8658_REG_AZ_L         0x39
#define QMI8658_REG_GX_L         0x3B
#define QMI8658_REG_GY_L         0x3D
#define QMI8658_REG_GZ_L         0x3F

#define QMI8658_REG_RESET        0x60
```

加速度输出寄存器是 `0x35 ~ 0x3A`，陀螺仪输出寄存器是 `0x3B ~ 0x40`，都是 16 bit 二进制补码。([Waveshare][1])

`CTRL1` 里面 bit6 是 `ADDR_AI`，也就是地址自动递增。默认是 0。你要做 burst read，建议初始化时打开它。配置寄存器不支持 burst write，所以 `CTRL1/CTRL2/CTRL3/CTRL7` 这类寄存器要一个一个单独写。([Waveshare][1])

本教程使用这个配置：

```text
CTRL1 = 0x40
    bit6 ADDR_AI = 1，打开地址自增
    bit7 SIM = 0，4 线 SPI
    bit5 BE = 0，按小端方式读 16bit 数据

CTRL2 = 0x15
    加速度 ±4g
    ODR 档位 0101

CTRL3 = 0x55
    陀螺仪 ±512 dps
    ODR 档位 0101

CTRL7 = 0x03
    bit0 aEN = 1，打开加速度计
    bit1 gEN = 1，打开陀螺仪
```

`CTRL2` 负责加速度量程和 ODR，`CTRL3` 负责陀螺仪量程和 ODR，`CTRL7` 负责 enable accel/gyro。([Waveshare][1])

---

## 5. 最小 IIO 驱动代码

文件名建议：

```text
qmi8658a_iio.c
```

代码如下：

```c
// SPDX-License-Identifier: GPL-2.0
/*
 * Minimal QMI8658A SPI IIO driver
 *
 * 功能：
 * 1. 通过 SPI 读取 QMI8658A WHO_AM_I
 * 2. 初始化 accel + gyro
 * 3. 注册成 Linux IIO 设备
 * 4. 通过 sysfs 暴露：
 *    in_accel_x_raw / y / z
 *    in_anglvel_x_raw / y / z
 *
 * 说明：
 * 这个版本是最小轮询版，不包含 FIFO、不包含中断、不包含 triggered buffer。
 */

#include <linux/module.h>
#include <linux/spi/spi.h>
#include <linux/regmap.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>
#include <asm/unaligned.h>

/* ========== QMI8658A register map ========== */
#define QMI8658_REG_WHO_AM_I     0x00
#define QMI8658_WHO_AM_I_VAL     0x05

#define QMI8658_REG_CTRL1        0x02
#define QMI8658_REG_CTRL2        0x03
#define QMI8658_REG_CTRL3        0x04
#define QMI8658_REG_CTRL5        0x06
#define QMI8658_REG_CTRL7        0x08
#define QMI8658_REG_RESET        0x60

#define QMI8658_REG_AX_L         0x35
#define QMI8658_REG_AY_L         0x37
#define QMI8658_REG_AZ_L         0x39
#define QMI8658_REG_GX_L         0x3B
#define QMI8658_REG_GY_L         0x3D
#define QMI8658_REG_GZ_L         0x3F

/* CTRL1 */
#define QMI8658_CTRL1_ADDR_AI    BIT(6)  /* address auto increment */
#define QMI8658_CTRL1_4WIRE      0       /* SIM=0, 4-wire SPI */

/* CTRL7 */
#define QMI8658_CTRL7_G_EN       BIT(1)
#define QMI8658_CTRL7_A_EN       BIT(0)

/*
 * 本驱动固定配置：
 * accel: ±4g
 * gyro : ±512 dps
 *
 * IIO scale 单位：
 * accel scale: m/s^2 per LSB
 * gyro  scale: rad/s per LSB
 *
 * accel ±4g:
 * scale = 4 * 9.80665 / 32768 = 0.0011971008
 *
 * gyro ±512 dps:
 * scale = 512 * pi / 180 / 32768 = 0.0002727077
 */
#define QMI8658_ACCEL_SCALE_NANO     1197101
#define QMI8658_GYRO_SCALE_NANO      272708

struct qmi8658_data {
    struct regmap *regmap;
    struct mutex lock;
};

/*
 * QMI8658A SPI 地址格式：
 * bit7 = 1 表示 read
 * bit7 = 0 表示 write
 * bit6~0 = register address
 *
 * regmap 的 read_flag_mask 可以自动帮我们在读寄存器时 OR 0x80。
 */
static const struct regmap_config qmi8658_regmap_config = {
    .reg_bits = 8,
    .val_bits = 8,
    .read_flag_mask = 0x80,
    .max_register = QMI8658_REG_RESET,
    .cache_type = REGCACHE_NONE,
};

/*
 * IIO channel 定义：
 * type:
 *   IIO_ACCEL     -> in_accel_x_raw
 *   IIO_ANGL_VEL  -> in_anglvel_x_raw
 *
 * modified + channel2:
 *   表示 x/y/z 轴
 *
 * address:
 *   对应每个轴的低字节寄存器地址
 */
#define QMI8658_CHANNEL(_type, _mod, _addr, _scan_idx)       \
    {                                                        \
        .type = (_type),                                     \
        .modified = 1,                                       \
        .channel2 = IIO_MOD_##_mod,                          \
        .info_mask_separate = BIT(IIO_CHAN_INFO_RAW),        \
        .info_mask_shared_by_type = BIT(IIO_CHAN_INFO_SCALE),\
        .address = (_addr),                                  \
        .scan_index = (_scan_idx),                           \
        .scan_type = {                                       \
            .sign = 's',                                     \
            .realbits = 16,                                  \
            .storagebits = 16,                               \
            .endianness = IIO_LE,                            \
        },                                                   \
    }

static const struct iio_chan_spec qmi8658_channels[] = {
    QMI8658_CHANNEL(IIO_ACCEL,    X, QMI8658_REG_AX_L, 0),
    QMI8658_CHANNEL(IIO_ACCEL,    Y, QMI8658_REG_AY_L, 1),
    QMI8658_CHANNEL(IIO_ACCEL,    Z, QMI8658_REG_AZ_L, 2),
    QMI8658_CHANNEL(IIO_ANGL_VEL, X, QMI8658_REG_GX_L, 3),
    QMI8658_CHANNEL(IIO_ANGL_VEL, Y, QMI8658_REG_GY_L, 4),
    QMI8658_CHANNEL(IIO_ANGL_VEL, Z, QMI8658_REG_GZ_L, 5),
};

static int qmi8658_read_axis(struct qmi8658_data *data,
                             const struct iio_chan_spec *chan,
                             int *val)
{
    u8 buf[2];
    int ret;
    s16 raw;

    mutex_lock(&data->lock);

    /*
     * 读取两个字节：
     * accel x: AX_L, AX_H
     * gyro  x: GX_L, GX_H
     *
     * 因为初始化时 CTRL1.BE = 0，所以这里按 little endian 解析。
     */
    ret = regmap_bulk_read(data->regmap, chan->address, buf, sizeof(buf));

    mutex_unlock(&data->lock);

    if (ret)
        return ret;

    raw = (s16)get_unaligned_le16(buf);
    *val = raw;

    return IIO_VAL_INT;
}

static int qmi8658_read_raw(struct iio_dev *indio_dev,
                            struct iio_chan_spec const *chan,
                            int *val,
                            int *val2,
                            long mask)
{
    struct qmi8658_data *data = iio_priv(indio_dev);

    switch (mask) {
    case IIO_CHAN_INFO_RAW:
        return qmi8658_read_axis(data, chan, val);

    case IIO_CHAN_INFO_SCALE:
        *val = 0;

        if (chan->type == IIO_ACCEL) {
            *val2 = QMI8658_ACCEL_SCALE_NANO;
            return IIO_VAL_INT_PLUS_NANO;
        }

        if (chan->type == IIO_ANGL_VEL) {
            *val2 = QMI8658_GYRO_SCALE_NANO;
            return IIO_VAL_INT_PLUS_NANO;
        }

        return -EINVAL;

    default:
        return -EINVAL;
    }
}

static const struct iio_info qmi8658_info = {
    .read_raw = qmi8658_read_raw,
};

static int qmi8658_chip_init(struct qmi8658_data *data, struct device *dev)
{
    unsigned int whoami;
    int ret;

    /*
     * 读取 WHO_AM_I。
     * 正常应该读到 0x05。
     */
    ret = regmap_read(data->regmap, QMI8658_REG_WHO_AM_I, &whoami);
    if (ret)
        return dev_err_probe(dev, ret, "failed to read WHO_AM_I\n");

    if (whoami != QMI8658_WHO_AM_I_VAL)
        return dev_err_probe(dev, -ENODEV,
                             "bad WHO_AM_I: 0x%02x, expected 0x%02x\n",
                             whoami, QMI8658_WHO_AM_I_VAL);

    /*
     * 可选：软件复位。
     * 如果你的板子上电时序比较稳，也可以先注释掉这两行试试。
     */
    ret = regmap_write(data->regmap, QMI8658_REG_RESET, 0xB0);
    if (ret)
        return ret;

    msleep(20);

    /*
     * 复位后再读一次 WHO_AM_I，确认 SPI 还正常。
     */
    ret = regmap_read(data->regmap, QMI8658_REG_WHO_AM_I, &whoami);
    if (ret)
        return ret;

    if (whoami != QMI8658_WHO_AM_I_VAL)
        return -ENODEV;

    /*
     * 先关掉 accel/gyro，再配置寄存器。
     */
    ret = regmap_write(data->regmap, QMI8658_REG_CTRL7, 0x00);
    if (ret)
        return ret;

    /*
     * CTRL1:
     * bit6 ADDR_AI = 1，允许 burst read 地址自增
     * bit7 SIM = 0，4-wire SPI
     * bit5 BE = 0，小端读 16bit 数据
     */
    ret = regmap_write(data->regmap, QMI8658_REG_CTRL1,
                       QMI8658_CTRL1_ADDR_AI | QMI8658_CTRL1_4WIRE);
    if (ret)
        return ret;

    /*
     * CTRL2:
     * aFS = 001 -> ±4g
     * aODR = 0101
     * value = 0b0001_0101 = 0x15
     */
    ret = regmap_write(data->regmap, QMI8658_REG_CTRL2, 0x15);
    if (ret)
        return ret;

    /*
     * CTRL3:
     * gFS = 101 -> ±512 dps
     * gODR = 0101
     * value = 0b0101_0101 = 0x55
     */
    ret = regmap_write(data->regmap, QMI8658_REG_CTRL3, 0x55);
    if (ret)
        return ret;

    /*
     * CTRL5:
     * 先不打开 LPF，最小化变量。
     * 后面需要低通滤波再配置。
     */
    ret = regmap_write(data->regmap, QMI8658_REG_CTRL5, 0x00);
    if (ret)
        return ret;

    /*
     * CTRL7:
     * bit0 aEN = 1
     * bit1 gEN = 1
     */
    ret = regmap_write(data->regmap, QMI8658_REG_CTRL7,
                       QMI8658_CTRL7_A_EN | QMI8658_CTRL7_G_EN);
    if (ret)
        return ret;

    msleep(20);

    dev_info(dev, "QMI8658A initialized, WHO_AM_I=0x%02x\n", whoami);

    return 0;
}

static int qmi8658_probe(struct spi_device *spi)
{
    struct iio_dev *indio_dev;
    struct qmi8658_data *data;
    int ret;

    /*
     * QMI8658A 支持 SPI mode 0 和 mode 3。
     * 如果设备树没写 spi-cpol/spi-cpha，通常就是 mode 0。
     */
    if ((spi->mode & (SPI_CPOL | SPI_CPHA)) != SPI_MODE_0 &&
        (spi->mode & (SPI_CPOL | SPI_CPHA)) != SPI_MODE_3) {
        dev_warn(&spi->dev, "unsupported SPI mode, try mode0 or mode3\n");
    }

    spi->bits_per_word = 8;

    /*
     * QMI8658A 最大 15MHz。
     * 调试阶段建议设备树写 1MHz。
     */
    if (!spi->max_speed_hz || spi->max_speed_hz > 15000000)
        spi->max_speed_hz = 1000000;

    ret = spi_setup(spi);
    if (ret)
        return dev_err_probe(&spi->dev, ret, "spi_setup failed\n");

    indio_dev = devm_iio_device_alloc(&spi->dev, sizeof(*data));
    if (!indio_dev)
        return -ENOMEM;

    data = iio_priv(indio_dev);
    mutex_init(&data->lock);

    data->regmap = devm_regmap_init_spi(spi, &qmi8658_regmap_config);
    if (IS_ERR(data->regmap))
        return dev_err_probe(&spi->dev, PTR_ERR(data->regmap),
                             "failed to init regmap\n");

    ret = qmi8658_chip_init(data, &spi->dev);
    if (ret)
        return ret;

    indio_dev->name = "qmi8658a";
    indio_dev->info = &qmi8658_info;
    indio_dev->modes = INDIO_DIRECT_MODE;
    indio_dev->channels = qmi8658_channels;
    indio_dev->num_channels = ARRAY_SIZE(qmi8658_channels);

    ret = devm_iio_device_register(&spi->dev, indio_dev);
    if (ret)
        return dev_err_probe(&spi->dev, ret, "failed to register iio device\n");

    dev_info(&spi->dev, "QMI8658A IIO device registered\n");

    return 0;
}

static const struct of_device_id qmi8658_of_match[] = {
    { .compatible = "qst,qmi8658a" },
    { }
};
MODULE_DEVICE_TABLE(of, qmi8658_of_match);

static const struct spi_device_id qmi8658_id[] = {
    { "qmi8658a", 0 },
    { }
};
MODULE_DEVICE_TABLE(spi, qmi8658_id);

static struct spi_driver qmi8658_driver = {
    .driver = {
        .name = "qmi8658a",
        .of_match_table = qmi8658_of_match,
    },
    .probe = qmi8658_probe,
    .id_table = qmi8658_id,
};

module_spi_driver(qmi8658_driver);

MODULE_AUTHOR("Vik");
MODULE_DESCRIPTION("Minimal QMI8658A SPI IIO driver");
MODULE_LICENSE("GPL");
```

---

## 6. 外部模块 Makefile

如果你暂时不想改内核源码树，可以先用外部模块方式编译。

文件名：

```text
Makefile
```

内容：

```makefile
obj-m += qmi8658a_iio.o

KDIR ?= /lib/modules/$(shell uname -r)/build
PWD  := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
```

本机编译：

```bash
make
```

交叉编译时，核心是换成你的 SDK 内核目录，例如：

```bash
make \
  ARCH=你的ARCH \
  CROSS_COMPILE=你的交叉编译器前缀 \
  KDIR=/path/to/your/linux-kernel \
  M=$PWD \
  modules
```

比如你的 SDK 如果是 MIPS，就可能类似：

```bash
make \
  ARCH=mips \
  CROSS_COMPILE=mips-linux-gnu- \
  KDIR=/home/vik/project/x2600/kernel \
  M=$PWD \
  modules
```

具体 `ARCH` 和 `CROSS_COMPILE` 要看你君正 SDK 的编译脚本里怎么写，别自己乱猜。

---

## 7. 内核配置要打开

至少要有这些：

```text
CONFIG_SPI=y
CONFIG_SPI_MASTER=y

CONFIG_IIO=y
CONFIG_REGMAP=y
CONFIG_REGMAP_SPI=y
```

如果编译成模块也可以：

```text
CONFIG_IIO=m
CONFIG_REGMAP_SPI=m
```

如果缺 `IIO`，你的驱动即使编出来也注册不了 `/sys/bus/iio/devices/iio:deviceX/`。

---

## 8. 加载驱动和查看日志

把 `.ko` 放到板子上：

```bash
insmod qmi8658a_iio.ko
dmesg | grep -i qmi
```

正常日志应该类似：

```text
qmi8658a spi0.0: QMI8658A initialized, WHO_AM_I=0x05
qmi8658a spi0.0: QMI8658A IIO device registered
```

然后看 IIO 设备：

```bash
ls /sys/bus/iio/devices/
```

找到名字：

```bash
for d in /sys/bus/iio/devices/iio:device*; do
    echo "$d: $(cat $d/name 2>/dev/null)"
done
```

如果正常，会看到：

```text
/sys/bus/iio/devices/iio:device0: qmi8658a
```

---

## 9. 用户态读取 raw 数据

假设设备是 `iio:device0`：

```bash
cd /sys/bus/iio/devices/iio:device0

cat name

cat in_accel_x_raw
cat in_accel_y_raw
cat in_accel_z_raw
cat in_accel_scale

cat in_anglvel_x_raw
cat in_anglvel_y_raw
cat in_anglvel_z_raw
cat in_anglvel_scale
```

换算方法是：

```text
实际物理值 = raw * scale
```

IIO ABI 里加速度 raw 经过 scale 后单位是 `m/s^2`，角速度 raw 经过 scale 后单位是 `rad/s`。([Android Git Repositories][3])

比如：

```bash
ACC_X_RAW=$(cat in_accel_x_raw)
ACC_SCALE=$(cat in_accel_scale)

awk "BEGIN {print $ACC_X_RAW * $ACC_SCALE}"
```

如果板子静止平放，你应该能看到某一个轴接近：

```text
9.8 m/s^2
```

其他两个轴接近 0。具体哪个轴是 9.8，取决于你的芯片贴装方向。

---

## 10. 如果 probe 失败，按这个顺序排查

### 10.1 WHO_AM_I 读不到 0x05

这是最常见问题。

重点查：

```text
1. SPI_MOSI / SPI_MISO 有没有接反
2. CS 脚是否真的拉低
3. SPI mode 是 0 还是 3
4. spi-max-frequency 先降到 500k 或 1MHz
5. 供电 VDD / VDDIO 是否正常
6. QMI8658A 是不是被接成 I2C 模式了
7. 逻辑分析仪看第一个字节是不是 0x80
```

读 WHO_AM_I 时，MOSI 应该发：

```text
0x80
```

因为读寄存器 `0x00` 时，需要：

```text
0x80 | 0x00
```

返回的第二个字节应该是：

```text
0x05
```

### 10.2 能读 WHO_AM_I，但是 raw 一直是 0

重点查：

```text
1. CTRL7 有没有写成 0x03
2. CTRL2 / CTRL3 有没有配置 ODR
3. CTRL1 的 ADDR_AI 有没有打开
4. 读寄存器是不是从 0x35、0x37、0x39、0x3B、0x3D、0x3F 开始
5. 16bit 解析大小端是否正确
```

本教程里 `CTRL1 = 0x40`，表示打开地址自增，并且按小端读取 16bit 数据。

### 10.3 数据跳动很大

先别怀疑驱动，先看硬件：

```text
1. IMU 供电是否干净
2. VDD/VDDIO 旁边有没有 100nF 去耦电容
3. SPI 线是否太长
4. GND 是否可靠
5. 采样频率是否过高
6. 是否需要打开低通滤波 CTRL5
```

等基本数据稳定后，可以把 `CTRL5` 改成打开 accel/gyro LPF，例如：

```c
regmap_write(data->regmap, QMI8658_REG_CTRL5, 0x11);
```

`CTRL5` 里 bit0 是加速度低通使能，bit4 是陀螺仪低通使能。([Waveshare][1])

---

## 11. 后面升级成中断 + buffer 的方向

你现在先不要急着写 FIFO。等轮询版稳定后，再做第二版：

```text
第一版：IIO direct mode
    cat in_accel_x_raw

第二版：IIO triggered buffer
    /dev/iio:deviceX 连续读取

第三版：QMI8658A FIFO + watermark interrupt
    更低 CPU 占用
```

中断版大概流程是：

```text
1. 设备树加 interrupt-parent / interrupts
2. probe 里拿 spi->irq
3. CTRL1 使能 INT2 或 INT1
4. CTRL7 保持 DRDY 使能
5. devm_iio_triggered_buffer_setup()
6. IRQ handler 里触发 iio_trigger_poll()
7. trigger handler 里一次性 bulk read 12 字节 accel + gyro
8. iio_push_to_buffers_with_timestamp()
```

IIO buffer 是给连续采样用的，数据会走 `/dev/iio:deviceX`，比你一直 `cat sysfs` 更适合高频 IMU。Linux IIO 文档也说明 buffer 用于连续数据采集，可以通过 `/dev/iio:deviceX` 一次读取多个通道，降低 CPU 负载。([DRI Wiki][4])

---

## 12. 你现在最推荐的落地顺序

按这个顺序做，最快：

```text
1. 先确认 SPI 控制器本身能工作
2. DTS 加 qmi8658a@0
3. 编译/加载 qmi8658a_iio.ko
4. dmesg 确认 WHO_AM_I = 0x05
5. cat /sys/bus/iio/devices/iio:deviceX/in_accel_x_raw
6. 晃动板子，看 raw 是否变化
7. 再加 LPF
8. 再加 DRDY 中断
9. 最后再做 FIFO + IIO buffer
```

对于新手，**不要一上来就 FIFO + 中断 + 多线程应用一起搞**。先把 `WHO_AM_I` 和 6 个 raw 通道读通，后面就好推进了。

[1]: https://files.waveshare.com/upload/5/5f/QMI8658A_Datasheet_Rev_A.pdf "QMI8658A Datasheet"
[2]: https://www.kernel.org/doc/html/v4.12/driver-api/iio/core.html "Core elements — The Linux Kernel  documentation"
[3]: https://android.googlesource.com/kernel/msm/%2B/android-msm-flo-3.4-kitkat-mr1/drivers/staging/iio/Documentation/sysfs-bus-iio?utm_source=chatgpt.com "drivers/staging/iio/Documentation/sysfs-bus-iio - kernel/msm"
[4]: https://dri.freedesktop.org/docs/drm/iio/iio_devbuf.html?utm_source=chatgpt.com "Industrial IIO device buffers — The Linux Kernel ..."

