下面我按**真实项目开发流程**讲：
不是一上来写代码，而是从**硬件确认 → Linux 驱动框架选择 → 设备树 → 内核配置 → 驱动加载 → IIO 测试 → FIFO/中断调试 → 应用层使用**完整走一遍。

你这个场景是：

```text
X2600 Linux 主核
  ↓ I2C 控制器
ICM42688 IMU
  ↓
Linux IIO 驱动
  ↓
/sys/bus/iio/devices/iio:deviceX/
  ↓
你的应用程序读取加速度/陀螺仪数据
```

---

# 1. 先理解：ICM42688 在 Linux 下应该走什么驱动框架？

ICM42688 是一个 6 轴 IMU：

```text
3 轴加速度计
3 轴陀螺仪
温度传感器
FIFO
中断引脚 INT1 / INT2
I2C / SPI / I3C 接口
```

TDK 数据手册里明确写了 ICM-42688-P 是 6-axis MEMS MotionTracking 器件，包含 3 轴 gyro 和 3 轴 accel，支持 I2C/SPI/I3C，并带 2KB FIFO 和两个可编程中断。

在 Linux 里，这种传感器**不建议自己做一个 `/dev/icm42688` 字符设备驱动**。标准做法是走：

```text
IIO：Industrial I/O subsystem
```

IIO 是 Linux 专门给 ADC、IMU、陀螺仪、加速度计、光照、气压、磁力计这类传感器用的框架。

也就是说，最终用户态不是直接访问寄存器，而是访问：

```text
/sys/bus/iio/devices/iio:deviceX/
```

或者高速连续采样时读：

```text
/dev/iio:deviceX
```

Linux IIO buffer 文档说明，IIO core 支持基于触发源的连续数据采集，可以从 `/dev/iio:deviceX` 字符设备一次读取多个通道数据，从而降低 CPU 负载。([Linux Kernel 文档][1])

---

# 2. 先确认一个重要事实：Linux 主线已经有 ICM42688 相关驱动

现在主线 Linux 里面已经有这个驱动目录：

```text
drivers/iio/imu/inv_icm42600/
```

它不是只支持 ICM42600，也支持同一系列的 ICM42602、ICM42605、ICM42686、ICM42688 等。

当前 Linux 的 `inv_icm42600_i2c.c` 里，I2C driver 的 `of_device_id` 已经包含：

```c
.compatible = "invensense,icm42688"
```

并且 `i2c_device_id` 里也有 `"icm42688"`。这个 I2C probe 过程会创建 regmap，然后调用 `inv_icm42600_core_probe()` 完成核心初始化。([cocalc.com][2])

所以开发路线优先级是：

```text
优先方案：直接使用内核已有 inv_icm42600 IIO 驱动
  ↓
如果 X2600 SDK 内核太旧，没有这个驱动
  ↓
从新内核 backport 这个驱动
  ↓
实在不行，再自己写一个简化 IIO 驱动
```

不要上来自己从零写，除非你的 SDK 内核特别老、接口差异很大，或者厂家驱动被魔改得很严重。

---

# 3. 整个驱动栈长什么样？

从硬件到应用，完整链路是这样的：

```text
ICM42688 芯片
  ↓
I2C 总线 SDA/SCL
  ↓
X2600 I2C 控制器
  ↓
X2600 I2C controller driver
  ↓
Linux I2C core
  ↓
inv_icm42600_i2c.c
  ↓
inv_icm42600_core.c
  ↓
IIO core
  ↓
/sys/bus/iio/devices/iio:deviceX/
  ↓
用户应用程序
```

你可以把它类比成 MCU：

```text
MCU：
main.c 直接 i2c_read_register()

Linux：
应用程序
  ↓
IIO sysfs / dev
  ↓
IMU 驱动
  ↓
I2C core
  ↓
I2C 控制器驱动
  ↓
ICM42688
```

Linux 里多了很多抽象层，但好处是标准化、可复用、应用层不用关心寄存器细节。

---

# 4. 硬件层先确认什么？

在写驱动前，必须先确认硬件连接。

ICM42688 I2C 模式下主要管这些脚：

| 引脚     | 作用                     |
| ------ | ---------------------- |
| VDD    | 芯片模拟/内部电源              |
| VDDIO  | IO 电平电源                |
| GND    | 地                      |
| AP_SCL | I2C 时钟                 |
| AP_SDA | I2C 数据                 |
| AP_AD0 | I2C 地址最低位              |
| INT1   | 中断 1                   |
| INT2   | 中断 2 / FSYNC / CLKIN   |
| AP_CS  | SPI 片选，I2C 模式要接到 VDDIO |

数据手册里写了：AP_AD0 是 I2C slave address LSB，INT1 和 INT2 是中断脚，AP_CS 在使用 I2C/I3C 接口时要接到 VDDIO。

I2C 地址由 AP_AD0 决定：

```text
AP_AD0 = 0 → 二进制 1101000 → 0x68
AP_AD0 = 1 → 二进制 1101001 → 0x69
```

数据手册说明 ICM42688 的 7-bit I2C slave address 是 `b110100X`，最低位由 AP_AD0 决定。

另外，I2C 是开漏总线，SDA/SCL 需要上拉电阻。数据手册里也明确说 I2C lines are open drain，需要 pull-up resistors，例如 10kΩ。

---

# 5. Linux 设备树在这里起什么作用？

Linux 驱动不是靠你在 C 代码里手动写：

```c
icm42688_init(0x68);
```

而是通过设备树告诉内核：

```text
这个 I2C 总线上挂了一个 ICM42688
地址是 0x68
中断脚接在哪个 GPIO
电源 regulator 是哪个
compatible 是 invensense,icm42688
```

设备树大概长这样：

```dts
&i2c0 {
    status = "okay";
    clock-frequency = <400000>;

    icm42688@68 {
        compatible = "invensense,icm42688";
        reg = <0x68>;

        interrupt-parent = <&gpioX>;
        interrupts = <Y IRQ_TYPE_EDGE_FALLING>;
        interrupt-names = "INT1";

        vdd-supply = <&vdd_3v3>;
        vddio-supply = <&vddio_3v3>;
    };
};
```

这里每个字段的意思是：

```text
&i2c0
  表示挂在 X2600 的 I2C0 控制器下面

status = "okay"
  打开这个 I2C 控制器

clock-frequency = <400000>
  I2C 速率 400kHz

icm42688@68
  设备节点名，@68 对应 I2C 地址

compatible = "invensense,icm42688"
  用来匹配 Linux 驱动

reg = <0x68>
  I2C 7-bit 地址

interrupt-parent / interrupts
  告诉内核 INT1 接到哪个 GPIO/IRQ

vdd-supply / vddio-supply
  电源 regulator，可选但推荐规范写
```

Linux 官方设备树绑定文档里，`invensense,icm42600.yaml` 的 compatible 列表包含 `invensense,icm42688`，并且描述了 `reg`、`interrupts`、`interrupt-names`、`vdd-supply`、`vddio-supply` 等属性。([Linux Kernel Archives][3])

---

# 6. 驱动是怎么和设备树匹配上的？

这块你一定要理解。

设备树里写：

```dts
compatible = "invensense,icm42688";
```

驱动里有：

```c
static const struct of_device_id inv_icm42600_of_matches[] = {
    {
        .compatible = "invensense,icm42688",
        .data = (void *)INV_CHIP_ICM42688,
    },
    {}
};
```

Linux 启动时会做这个匹配：

```text
设备树节点 compatible
  ↓
Linux device model
  ↓
找到 of_match_table
  ↓
匹配到 inv-icm42600-i2c 驱动
  ↓
调用 probe()
```

probe 大概做这些事情：

```text
1. 确认 I2C adapter 支持需要的传输能力
2. 从 compatible 获取芯片类型
3. 创建 regmap
4. 做 I2C 接口配置
5. 调用 core_probe
6. 读取 WHO_AM_I
7. 初始化 gyro / accel / temp
8. 注册 IIO 设备
```

主线驱动的 I2C probe 里面确实会检查 I2C 功能、获取 match data、创建 regmap，然后进入 core probe。([cocalc.com][2])

---

# 7. regmap 是什么？

你在 MCU 里通常这么读寄存器：

```c
i2c_read(ICM42688_ADDR, WHO_AM_I_REG, &val, 1);
```

Linux 驱动里一般不直接到处写 `i2c_transfer()`，而是用：

```text
regmap
```

regmap 是 Linux 提供的寄存器映射抽象层。

它帮你统一处理：

```text
寄存器读
寄存器写
bit 更新
缓存
锁
大小端
I2C/SPI 差异
调试接口
```

比如：

```c
regmap_read(st->map, REG_WHO_AM_I, &val);
regmap_update_bits(st->map, REG_PWR_MGMT0, mask, value);
```

这样同一个 core driver 可以同时支持 I2C 和 SPI。

所以 inv_icm42600 这一类驱动通常分成：

```text
inv_icm42600_i2c.c
  只处理 I2C 总线差异

inv_icm42600_spi.c
  只处理 SPI 总线差异

inv_icm42600_core.c
  处理芯片公共逻辑

inv_icm42600_accel.c
  处理加速度

inv_icm42600_gyro.c
  处理陀螺仪

inv_icm42600_temp.c
  处理温度
```

---

# 8. 驱动起来后，用户态看到什么？

驱动注册成功后，Linux 会出现 IIO 设备：

```text
/sys/bus/iio/devices/iio:device0/
/sys/bus/iio/devices/iio:device1/
...
```

你可以看：

```bash
for d in /sys/bus/iio/devices/iio:device*; do
    echo "$d: $(cat $d/name 2>/dev/null)"
done
```

可能看到类似：

```text
/sys/bus/iio/devices/iio:device0: icm42688_accel
/sys/bus/iio/devices/iio:device1: icm42688_gyro
```

具体名字要看你的内核驱动实现。

然后里面会有类似：

```text
in_accel_x_raw
in_accel_y_raw
in_accel_z_raw
in_accel_scale
in_anglvel_x_raw
in_anglvel_y_raw
in_anglvel_z_raw
in_anglvel_scale
in_temp_raw
sampling_frequency
```

IIO ABI 文档里规定了加速度原始数据一般通过 `in_accel_x_raw`、`in_accel_y_raw`、`in_accel_z_raw` 暴露，单位经过 scale/offset 转换后是 m/s²；角速度通过 `in_anglvel_x_raw`、`in_anglvel_y_raw`、`in_anglvel_z_raw` 暴露，单位经过转换后是 rad/s。([Linux Kernel Archives][4])

---

# 9. 最基础测试：先验证 I2C 能不能读到芯片

在驱动还没加载前，可以用 `i2c-tools` 先扫总线。

假设 IMU 挂在 I2C0：

```bash
i2cdetect -y 0
```

如果 AP_AD0 接地，应该看到 `0x68`。
如果 AP_AD0 拉高，应该看到 `0x69`。

然后读 WHO_AM_I：

```bash
i2cget -y 0 0x68 0x75
```

ICM42688 的 `WHO_AM_I` 寄存器地址是 `0x75`，默认值是 `0x47`。数据手册里写明 WHO_AM_I 地址为 117，也就是 `0x75`，reset value 是 `0x47`。

所以正常应该看到：

```text
0x47
```

这一步非常关键。

如果这一步失败，不要急着调 Linux 驱动，先查硬件：

```text
VDD 有没有电
VDDIO 有没有电
SDA/SCL 有没有上拉
AP_CS 是否接 VDDIO
AP_AD0 地址是否符合预期
I2C bus number 是否选对
X2600 I2C pinmux 是否配置对
示波器/逻辑分析仪看 SDA/SCL 有没有波形
```

注意：如果内核驱动已经绑定了这个设备，`i2cdetect` 可能显示 `UU`，表示这个地址已经被内核驱动占用。不要随便用 `i2cget -f` 强制访问，可能和驱动同时操作寄存器导致状态混乱。

---

# 10. 内核配置要开哪些？

如果你的 X2600 SDK 内核已经有 `drivers/iio/imu/inv_icm42600/`，那要打开相关配置。

大概需要：

```text
CONFIG_IIO
CONFIG_IIO_BUFFER
CONFIG_IIO_TRIGGERED_BUFFER
CONFIG_INV_ICM42600
CONFIG_INV_ICM42600_I2C
CONFIG_REGMAP_I2C
```

其中最关键的是：

```text
CONFIG_INV_ICM42600_I2C
```

这个配置项对应 InvenSense ICM-426xx I2C driver，模块名通常是：

```text
inv-icm42600-i2c
```

LKDDb 对这个配置项的说明是：它支持 InvenSense ICM-426xx motion tracking devices over I2C，并且可以编译为模块 `inv-icm42600-i2c`。([Cateee][5])

在 X2600 SDK 里你可以这样找：

```bash
grep -R "INV_ICM42600" -n kernel/
```

如果找得到，说明内核里有这个驱动。
如果找不到，说明你要 backport 或者自己写。

---

# 11. 驱动开发分三种情况

## 情况 A：内核已经有驱动

这是最理想的。

你只需要做：

```text
1. 设备树添加 icm42688 节点
2. 内核配置打开 INV_ICM42600_I2C
3. 编译 kernel/dtb/rootfs
4. 烧录
5. 看 dmesg 和 /sys/bus/iio/devices/
```

驱动代码不用动。

---

## 情况 B：内核有 inv_icm42600，但没有 icm42688 compatible

有些旧内核可能有 ICM42600/42605，但没有 ICM42688。

这种情况要做小改动：

```c
{
    .compatible = "invensense,icm42688",
    .data = (void *)INV_CHIP_ICM42688,
},
```

同时可能还要在芯片枚举里加：

```c
INV_CHIP_ICM42688
```

并确认 WHO_AM_I ID 是 `0x47`。

这种属于“扩展已有驱动”。

---

## 情况 C：内核完全没有这个驱动

这时有两条路。

第一条，推荐：

```text
从新内核 backport drivers/iio/imu/inv_icm42600/
```

优点：

```text
正规
功能完整
支持 IIO
支持 FIFO / buffer / scale / sampling_frequency
后续维护方便
```

缺点：

```text
老内核 API 可能不兼容
要改 devm、pm_runtime、regmap、iio buffer 相关接口
```

第二条，临时验证：

```text
先写一个最小 I2C 驱动，只读 WHO_AM_I 和原始寄存器
```

这个适合 bring-up，但不适合作为最终产品驱动。

---

# 12. 如果自己写最小驱动，代码结构是什么？

最小 Linux I2C 驱动大概长这样：

```c
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/regmap.h>
#include <linux/of.h>

#define ICM42688_REG_WHO_AM_I  0x75
#define ICM42688_WHO_AM_I_VAL  0x47

struct icm42688_data {
    struct regmap *regmap;
};

static const struct regmap_config icm42688_regmap_config = {
    .reg_bits = 8,
    .val_bits = 8,
};

static int icm42688_probe(struct i2c_client *client)
{
    struct icm42688_data *data;
    unsigned int val;
    int ret;

    data = devm_kzalloc(&client->dev, sizeof(*data), GFP_KERNEL);
    if (!data)
        return -ENOMEM;

    data->regmap = devm_regmap_init_i2c(client, &icm42688_regmap_config);
    if (IS_ERR(data->regmap))
        return PTR_ERR(data->regmap);

    ret = regmap_read(data->regmap, ICM42688_REG_WHO_AM_I, &val);
    if (ret)
        return ret;

    dev_info(&client->dev, "WHO_AM_I = 0x%02x\n", val);

    if (val != ICM42688_WHO_AM_I_VAL)
        return -ENODEV;

    i2c_set_clientdata(client, data);

    dev_info(&client->dev, "ICM42688 probe ok\n");
    return 0;
}

static const struct of_device_id icm42688_of_match[] = {
    { .compatible = "invensense,icm42688" },
    { }
};
MODULE_DEVICE_TABLE(of, icm42688_of_match);

static struct i2c_driver icm42688_driver = {
    .driver = {
        .name = "icm42688-simple",
        .of_match_table = icm42688_of_match,
    },
    .probe = icm42688_probe,
};

module_i2c_driver(icm42688_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Simple ICM42688 I2C driver");
```

这个驱动只能说明：

```text
I2C 通了
WHO_AM_I 对了
probe 能进来
```

但是它还不是完整 IMU 驱动。

完整驱动还要实现：

```text
加速度 raw 读取
陀螺仪 raw 读取
温度读取
量程设置
ODR 设置
低功耗/低噪声模式
中断配置
FIFO 配置
IIO channel
IIO buffer
runtime PM
suspend/resume
```

所以正式项目还是建议走 IIO。

---

# 13. IIO 驱动核心要实现什么？

一个完整 IMU IIO 驱动，核心是这几块：

```text
1. i2c_driver
2. regmap
3. chip init
4. iio_dev
5. iio_chan_spec
6. iio_info read_raw/write_raw
7. triggered buffer / hardware FIFO
8. interrupt handler
9. power management
```

大概关系：

```text
i2c_driver.probe()
  ↓
devm_iio_device_alloc()
  ↓
配置 channels
  ↓
配置 read_raw/write_raw
  ↓
配置 buffer
  ↓
iio_device_register()
```

channel 可能长这样：

```text
accel x
accel y
accel z
gyro x
gyro y
gyro z
temperature
timestamp
```

用户态读取：

```bash
cat in_accel_x_raw
cat in_accel_scale
```

应用换算：

```text
accel_mps2 = raw * scale
gyro_radps = raw * scale
```

---

# 14. 普通读取和 FIFO 读取有什么区别？

ICM42688 有两种常见工作方式。

## 方式 1：直接读寄存器

流程：

```text
应用读 sysfs
  ↓
IIO read_raw()
  ↓
驱动通过 I2C 读取当前 accel/gyro 寄存器
  ↓
返回 raw 值
```

优点：

```text
简单
适合调试
适合低频读取
```

缺点：

```text
高频时 CPU/I2C 压力大
采样时间不够均匀
容易丢数据
```

适合：

```text
10Hz
50Hz
100Hz 简单姿态显示
调试阶段
```

---

## 方式 2：FIFO + 中断 + IIO buffer

流程：

```text
ICM42688 内部按 ODR 采样
  ↓
样本进入 FIFO
  ↓
FIFO 达到 watermark
  ↓
INT1 触发中断
  ↓
Linux 驱动中断线程读取 FIFO
  ↓
数据推入 IIO buffer
  ↓
应用从 /dev/iio:deviceX 批量读取
```

优点：

```text
适合高频
时间更稳定
I2C 批量读取效率高
CPU 负载低
不容易丢样
```

缺点：

```text
驱动复杂
中断和时间戳要调
FIFO overflow 要处理
应用解析 /dev/iio:deviceX 数据格式更麻烦
```

ICM42688 自身有 2KB FIFO 和两个可编程中断，Linux IIO 的 buffer 机制就是用来做这种连续采样的。

---

# 15. 测试调试流程应该怎么走？

不要一上来就测 FIFO。应该分阶段。

## 阶段 1：硬件 I2C 通信测试

目标：

```text
确认 X2600 能通过 I2C 找到 ICM42688
```

测试点：

```bash
i2cdetect -y 0
i2cget -y 0 0x68 0x75
```

期望：

```text
i2cdetect 能看到 0x68 或 0x69
WHO_AM_I 返回 0x47
```

如果失败，优先查：

```text
电源
地址脚 AP_AD0
AP_CS 是否接 VDDIO
SDA/SCL 上拉
I2C pinmux
设备树里的 I2C 控制器是否 enable
```

---

## 阶段 2：设备树匹配测试

目标：

```text
确认 Linux 创建了 i2c_client，并调用了 IMU 驱动 probe
```

看启动日志：

```bash
dmesg | grep -i icm
dmesg | grep -i inv
dmesg | grep -i iio
dmesg | grep -i i2c
```

看设备树是否生效：

```bash
ls /proc/device-tree/
```

看 I2C 设备是否存在：

```bash
ls /sys/bus/i2c/devices/
```

可能看到：

```text
0-0068
```

表示：

```text
I2C bus 0
address 0x68
```

如果 `0-0068` 存在，但没有 IIO 设备，说明：

```text
设备树创建了设备
但是驱动没有匹配/没有加载/配置没打开
```

如果 `0-0068` 都不存在，说明：

```text
设备树节点可能没编进 dtb
I2C 控制器可能没开
reg 地址可能写错
```

---

## 阶段 3：驱动绑定测试

查看绑定的 driver：

```bash
readlink /sys/bus/i2c/devices/0-0068/driver
```

正常可能指向：

```text
/sys/bus/i2c/drivers/inv-icm42600-i2c
```

如果没有 driver，说明没绑定上。

常见原因：

```text
compatible 写错
CONFIG_INV_ICM42600_I2C 没打开
驱动是模块但没加载
设备树没烧进去
I2C 地址不对
probe 读取 WHO_AM_I 失败
```

---

## 阶段 4：IIO 节点测试

查看 IIO 设备：

```bash
ls /sys/bus/iio/devices/
```

查看名字：

```bash
for d in /sys/bus/iio/devices/iio:device*; do
    echo "$d"
    cat $d/name
done
```

查看有哪些属性：

```bash
ls /sys/bus/iio/devices/iio:device0/
```

你重点找：

```text
in_accel_x_raw
in_accel_y_raw
in_accel_z_raw
in_accel_scale

in_anglvel_x_raw
in_anglvel_y_raw
in_anglvel_z_raw
in_anglvel_scale

sampling_frequency
```

---

## 阶段 5：静态数据测试

板子静止放桌上。

读加速度：

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

因为板子静止时，IMU 会测到重力加速度。

如果三个轴都乱跳很大：

```text
量程/ODR/电源/焊接/机械振动/驱动配置可能有问题
```

如果某一个轴方向反了：

```text
不一定是驱动错，可能是 IMU 在板子上的安装方向和你定义的机体坐标系不一致
```

这时要用 mounting matrix 或应用层坐标转换处理。

---

## 阶段 6：动态姿态测试

手动转动板子：

```text
绕 X 轴转
看 gyro x 有没有明显变化

绕 Y 轴转
看 gyro y 有没有明显变化

绕 Z 轴转
看 gyro z 有没有明显变化
```

同时看加速度：

```text
翻转板子
原来 +1g 的轴应该变成 -1g
```

这一步主要验证：

```text
轴向是否正确
正负方向是否正确
单位换算是否正确
数据是否实时变化
```

---

## 阶段 7：采样率测试

看是否支持：

```bash
cat sampling_frequency
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

注意：具体路径可能在 accel 和 gyro 各自的 iio device 下，不同内核实现略有差异。

测试重点：

```text
设置后是否生效
实际输出频率是否接近设置值
高频下是否丢样
CPU 占用是否过高
```

---

## 阶段 8：buffer / FIFO 测试

IIO buffer 的典型配置思路是：

```text
1. 关闭 buffer
2. 选择 scan_elements
3. 设置 buffer length
4. 设置 watermark
5. 打开 buffer
6. 从 /dev/iio:deviceX 读取二进制数据
```

比如：

```bash
cd /sys/bus/iio/devices/iio:device0

echo 0 > buffer/enable

echo 1 > scan_elements/in_accel_x_en
echo 1 > scan_elements/in_accel_y_en
echo 1 > scan_elements/in_accel_z_en
echo 1 > scan_elements/in_timestamp_en

echo 128 > buffer/length
echo 1 > buffer/enable
```

然后读：

```bash
cat /dev/iio:device0 > /tmp/imu.bin
```

IIO buffer 文档说明，scan element 通过 `/sys/bus/iio/devices/iio:deviceX/scan_elements/` 选择，buffer 的 `enable` 用来启动采集，并且通常应该在配置 length 和 scan elements 之后再打开。([Linux Kernel 文档][6])

实际项目里通常不用 `cat` 直接看，因为这是二进制数据。你要写 C 程序或者用 libiio 解析。

---

# 16. 常见问题怎么排查？

## 问题 1：`i2cdetect` 看不到 0x68/0x69

优先查硬件：

```text
VDD/VDDIO 是否正常
SDA/SCL 是否接反
有没有上拉
AP_CS 是否接 VDDIO
AP_AD0 是高还是低
I2C 控制器 pinmux 是否配置正确
I2C bus 编号是否选错
```

用示波器看：

```text
SCL 有没有时钟
SDA 有没有 ACK
ACK 位是不是被拉低
```

没有 ACK 基本就是：

```text
地址错
芯片没上电
线错
芯片没进 I2C 模式
```

---

## 问题 2：能扫到地址，但 WHO_AM_I 不是 0x47

可能原因：

```text
读错寄存器
总线干扰
I2C 速度太高
芯片型号不是 ICM42688
驱动/用户态同时访问导致状态混乱
```

先降到 100kHz 或 400kHz 测。
虽然 ICM42688 I2C 最大支持 1MHz，但板级走线、上拉、电容、控制器能力不一定一开始就适合跑满速。数据手册写 I2C maximum bus speed 是 1MHz。

---

## 问题 3：设备树写了，但 probe 没进

查：

```bash
dmesg | grep -i "icm\|inv\|iio\|i2c"
ls /sys/bus/i2c/devices/
cat /sys/firmware/devicetree/base/xxx
```

重点确认：

```text
dtb 是不是新的
烧录的是不是新的 dtb
compatible 有没有拼错
I2C 控制器 status 是否 okay
reg 地址是否 0x68/0x69
内核 config 是否打开
驱动是 built-in 还是 module
```

---

## 问题 4：probe 进了，但报 `-ENODEV`

通常说明驱动读 WHO_AM_I 失败或者 ID 不匹配。

查：

```text
WHO_AM_I 是否 0x47
regmap 读写是否成功
I2C transfer 是否报错
电源 regulator 是否打开
复位后等待时间是否足够
```

数据手册里写 power-up 后 register read/write 的 start-up time 是 1ms。

---

## 问题 5：IIO 节点有，但 raw 数据一直是 0

可能原因：

```text
sensor 没从 sleep 切到 low-noise/low-power 模式
ODR 没配置
量程没配置
驱动 runtime PM 把芯片 suspend 了
读的是错误的 iio device
```

查：

```bash
cat name
cat sampling_frequency
cat *_raw
cat *_scale
```

必要时打开动态调试：

```bash
echo 'file drivers/iio/imu/inv_icm42600/* +p' > /sys/kernel/debug/dynamic_debug/control
dmesg -w
```

前提是内核开了 dynamic debug。

---

## 问题 6：中断不触发

查：

```bash
cat /proc/interrupts | grep -i icm
cat /proc/interrupts | grep gpio
```

然后晃动板子或者打开 FIFO buffer，看中断计数是否增加。

如果不增加：

```text
INT1 线没接对
GPIO 中断号写错
触发边沿写错
INT 极性配置不对
INT 是 open-drain 但没上拉
设备树 interrupt-parent 错
pinctrl 没把该 GPIO 配成输入
```

ICM42688 的 INT1 可以是 push-pull 或 open-drain，INT2 也可以作为中断脚。

---

## 问题 7：低速正常，高速丢数据

这通常和这几个有关：

```text
I2C 速度不够
FIFO watermark 太小
用户态读取不及时
线程调度延迟
中断风暴
buffer length 太小
I2C 控制器 DMA/PIO 性能不足
```

解决思路：

```text
提高 I2C 到 400kHz / 1MHz
使用 FIFO + watermark
增大 buffer length
降低 ODR
应用用 poll/select 读 /dev/iio:deviceX
避免用 shell 高频 cat raw
```

---

# 17. 应用层应该怎么读？

应用层有两种读法。

## 低速调试：读 sysfs

比如：

```c
int read_int_file(const char *path)
{
    FILE *fp = fopen(path, "r");
    int val = 0;

    if (!fp)
        return 0;

    fscanf(fp, "%d", &val);
    fclose(fp);
    return val;
}
```

读取：

```text
/sys/bus/iio/devices/iio:deviceX/in_accel_x_raw
/sys/bus/iio/devices/iio:deviceX/in_accel_scale
```

换算：

```text
实际加速度 = raw * scale
实际角速度 = raw * scale
```

这种适合：

```text
调试
工厂测试
低频状态检测
```

不适合：

```text
高频姿态解算
高频轨迹积分
头部追踪
平衡控制
```

---

## 高频采样：读 IIO buffer

高频时应用应该：

```text
配置 scan_elements
打开 buffer
poll /dev/iio:deviceX
批量 read
按 scan_elements 的 type/index 解析数据
```

这种才适合：

```text
100Hz
200Hz
500Hz
1kHz
姿态融合
运动检测
机器人控制
```

---

# 18. 对你这种新手，建议的开发路线

你不要一开始就写完整驱动。按这个顺序来：

```text
第一步：硬件 I2C 通
  i2cdetect 能看到 0x68/0x69
  i2cget 0x75 能读到 0x47

第二步：设备树生效
  /sys/bus/i2c/devices/0-0068 出现

第三步：驱动绑定
  readlink driver 指到 inv-icm42600-i2c

第四步：IIO 节点出现
  /sys/bus/iio/devices/iio:deviceX/name 正确

第五步：raw 数据能读
  accel/gyro/temp 能变化

第六步：scale/ODR 正常
  sampling_frequency 可设置

第七步：中断正常
  /proc/interrupts 计数变化

第八步：FIFO/buffer 正常
  /dev/iio:deviceX 能连续读数据

第九步：应用层解析
  坐标系转换
  单位换算
  时间戳处理
  滤波/姿态融合
```

---

# 19. 总结成一句话

ICM42688 的 Linux 驱动开发，不是简单写一个 `i2c_read()` 循环，而是这条链：

```text
硬件连线确认
  ↓
I2C 地址和 WHO_AM_I 验证
  ↓
设备树描述 ICM42688
  ↓
内核启用 inv_icm42600 IIO 驱动
  ↓
probe 通过 regmap 初始化芯片
  ↓
注册 IIO accel/gyro/temp 设备
  ↓
sysfs 读 raw/scale 做低速测试
  ↓
FIFO + interrupt + IIO buffer 做高速采样
  ↓
应用层做单位换算、坐标转换、滤波和姿态融合
```

对 X2600 项目来说，你现在最应该先确认的是：

```bash
grep -R "INV_ICM42600" -n kernel/
grep -R "invensense,icm42688" -n kernel/
```

如果 SDK 内核已经有，那就是**设备树 + 配置 + 测试**问题；如果没有，才进入**驱动 backport/移植**阶段。

[1]: https://docs.kernel.org/iio/iio_devbuf.html?utm_source=chatgpt.com "Industrial IIO device buffers"
[2]: https://cocalc.com/github/torvalds/linux/blob/master/drivers/iio/imu/inv_icm42600/inv_icm42600_i2c.c "CoCalc -- inv_icm42600_i2c.c"
[3]: https://www.kernel.org/doc/Documentation/devicetree/bindings/iio/imu/invensense%2Cicm42600.yaml "www.kernel.org"
[4]: https://www.kernel.org/doc/Documentation/ABI/testing/sysfs-bus-iio?utm_source=chatgpt.com "sysfs-bus-iio"
[5]: https://cateee.net/lkddb/web-lkddb/INV_ICM42600_I2C.html?utm_source=chatgpt.com "InvenSense ICM-426xx I2C driver - cateee.net Homepage"
[6]: https://docs.kernel.org/driver-api/iio/buffers.html?utm_source=chatgpt.com "Buffers"
