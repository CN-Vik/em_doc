不是，**不是“用不了 IIO+SPI”**。
更准确地说：

```text
君正 X2600 这种 module_driver / IConfigTool / cmd_spi 方法
不是标准主线 Linux 设备树那种写法，
但只要它底层还是注册成 Linux SPI master，
就可以用 IIO + SPI。
```

你的文档里其实已经证明了这一点：它的示例代码包含 `<linux/spi/spi.h>`，用的是 `struct spi_driver`、`spi_register_driver()`、`spi_board_info`、`spi_write_then_read()` 这些 Linux SPI 框架接口，不是完全私有的一套 SPI API。文档第 4~7 页就是这种 SPI 设备驱动注册流程。

---

## 关键区别是：IIO+SPI 不走 cmd_spi

你现在容易混的是这几套东西：

```text
1. IConfigTool / module_driver
   负责配置 SPI 控制器、IO、模块编译。

2. cmd_spi / spidev
   是应用层调试 SPI 的工具。
   适合测试 SPI 通不通。

3. IIO + BMI160_SPI
   是内核传感器驱动。
   最终生成 /sys/bus/iio/devices/iio:deviceX。
```

所以不是：

```text
cmd_spi add_dev 0 pd05
    ↓
变成 BMI160 IIO 设备
```

而是：

```text
SPI 控制器驱动注册 spi_master
    ↓
注册一个 spi_device，名字叫 bmi160
    ↓
bmi160_spi 驱动匹配到这个设备
    ↓
bmi160_core 初始化
    ↓
生成 IIO 设备
```

---

## 文档里的方法和 IIO+SPI 是兼容的

文档第 4 页写了君正封装接口：

```c
struct spi_device *spi_register_device(struct spi_board_info *info, int spi_bus_num)
```

同时也写了内核接口：

```c
int spi_register_driver(struct spi_driver *sdrv)
void spi_unregister_device(struct spi_device *spi)
void spi_unregister_driver(struct spi_driver *sdrv)
```

这说明它还是接到 Linux SPI 设备/驱动模型上的。

第 5~7 页的例子里，它先写：

```c
static struct spi_driver m_spi_driver = {
    .driver = {
        .name = "m_new_spi_device",
        .bus = &spi_bus_type,
    },
    .id_table = id_table,
    .probe = m_spi_probe,
};
```

再写：

```c
static struct spi_board_info m_spi_device = {
    .modalias = "m_new_spi_device",
    .max_speed_hz = 10 * 1000 * 1000,
    .mode = SPI_MODE_3,
    .controller_data = (void *)(GPIO_PC(16)),
    .chip_select = 0,
};
```

这个结构和 BMI160 SPI 驱动要用的结构是一样的，只不过你要把 `modalias` 从 `"m_new_spi_device"` 换成 `"bmi160"`。

---

# 真正的问题在这里

## 情况 1：君正 SPI 控制器驱动注册了 Linux spi_master

这种情况 **可以用 IIO+SPI**。

你板子上如果能看到：

```sh
ls /sys/class/spi_master/
```

或者：

```sh
ls /sys/bus/spi/devices/
```

有类似：

```text
spi0
spi0.0
```

并且文档里的 `cmd_spi`、`m_spi_dev.ko` 能跑通，那就说明 SPI master 基本是接入 Linux SPI 框架的。

这时你要做的是：

```text
SPI master：君正 module_driver 或内建均可
BMI160_SPI：Linux IIO 驱动
BMI160 device：通过 spi_board_info 注册
```

---

## 情况 2：君正 SPI 只是 libhardware2 私有接口，不注册 spi_master

这种情况就 **不能直接用标准 IIO+SPI**。

因为 BMI160 的内核驱动需要的是：

```c
struct spi_device *spi
```

它内部会用：

```c
spi_write_then_read()
regmap_spi
```

如果底层没有 Linux SPI master，就没有 `struct spi_device`，那 BMI160 SPI 驱动就 probe 不起来。

但是从你这个文档看，它并不是这种情况。文档明确用了 `spi_busnum_to_master()`、`spi_new_device()`、`spi_register_driver()`、`spi_write_then_read()`，所以它大概率是能接 IIO+SPI 的。

---

# 你的 X2600 上有两条路线

## 路线 A：保留君正 SPI module_driver，只把 BMI160 做成模块

这是最稳、最适合你当前阶段的方式。

加载顺序类似：

```sh
insmod utils.ko
insmod soc_spi.ko

insmod industrialio.ko
insmod kfifo_buf.ko
insmod industrialio-triggered-buffer.ko

insmod bmi160_core.ko
insmod bmi160_spi.ko
insmod bmi160_spi_board.ko
```

其中 `bmi160_spi_board.ko` 负责注册：

```c
static struct spi_board_info bmi160_spi_board_info = {
    .modalias = "bmi160",
    .max_speed_hz = 1000000,
    .mode = SPI_MODE_0,
    .controller_data = (void *)GPIO_PD(5),
    .chip_select = 0,
};
```

这个方式的优点是：
SPI 控制器还是君正原来的 module_driver，不用大改内核。

---

## 路线 B：SPI 控制器和 BMI160 全部编进内核

这个也可以，但是难度更高。

你要保证：

```text
CONFIG_SPI=y
CONFIG_REGMAP_SPI=y
CONFIG_IIO=y
CONFIG_BMI160=y
CONFIG_BMI160_SPI=y
```

同时 SPI 控制器也必须是内建：

```text
CONFIG_INGENIC_SPI=y
```

或者君正对应的 SPI controller 配置是 `y`。

否则就会出现：

```text
BMI160 内建进 kernel
但是 SPI master 还是 soc_spi.ko
启动时 SPI master 不存在
BMI160 找不到 bus 0
probe 失败
```

所以全内建不是不能做，而是你得把整条链路都内建，不是只改 `CONFIG_BMI160_SPI=y`。

---

# 我建议你现在先别急着全内建

你现在最适合的学习路线是：

```text
第一步：确认君正 SPI module_driver 正常
        cmd_spi 或 m_spi_dev.ko 能读写

第二步：写 bmi160_spi_board.ko
        只负责注册 spi_device: modalias="bmi160", CS=PD05

第三步：加载 bmi160_core.ko + bmi160_spi.ko + bmi160_spi_board.ko
        看能不能生成 IIO 节点

第四步：稳定后再考虑把它们改成内建
```

也就是说：**先证明 IIO+SPI 能跑，再考虑内建。**

---

# 你可以这样判断能不能接 IIO+SPI

板子上执行：

```sh
ls /sys/class/spi_master/
```

如果有：

```text
spi0
```

再执行：

```sh
ls /sys/bus/spi/devices/
```

如果你用 `cmd_spi add_dev 0 pd05` 后能看到：

```text
spi0.0
```

说明 Linux SPI 设备模型是通的。

再看：

```sh
cat /sys/bus/spi/devices/spi0.0/modalias
```

如果是 spidev，那只是应用层调试设备。
你最终 BMI160 应该让它变成：

```text
bmi160
```

---

# 一句话总结

**君正 X2600 这种驱动方式可以用 IIO+SPI。**

但是你不能把 `cmd_spi/spidev` 当成 BMI160 驱动；`cmd_spi` 只是调 SPI 总线的工具。真正的 BMI160 IIO 驱动，需要注册一个 `modalias="bmi160"` 的 `spi_device`，然后让 Linux 的 `bmi160_spi` 驱动去 probe，最后才会生成 IIO 节点。


--------
-------
-------

可以，用君正 **IConfigTool + module_driver 的 SPI 控制器**，再接 Linux 标准 **IIO + BMI160 SPI 驱动**。
关键是要分清楚：**IConfigTool 配的是 SPI 总线和 IO；BMI160 IIO 驱动要靠内核里的 `bmi160_spi` 去 probe。**

整体链路是这样：

```text
IConfigTool 配 SPI0 引脚
        ↓
module_driver 编译出 soc_spi.ko
        ↓
soc_spi.ko 注册 Linux spi_master
        ↓
bmi160_spi_board.ko 注册 spi_device，名字叫 "bmi160"
        ↓
bmi160_spi.ko 匹配 probe
        ↓
bmi160_core.ko 注册 IIO 设备
        ↓
/sys/bus/iio/devices/iio:deviceX
        ↓
cat raw 节点读取加速度 / 陀螺仪
```

君正文档里的 SPI 设备驱动例子本质上也是走 Linux SPI 框架：用了 `struct spi_driver`、`spi_register_driver()`、`struct spi_board_info`、`spi_write_then_read()`，并通过 `.controller_data = GPIO_xx` 指定片选 GPIO。这个和我们注册 BMI160 的方法是同一套思路，只是把 demo 设备名换成 `"bmi160"`。

---

# 1. IConfigTool 里只配置 SPI0 总线 IO

你这个 BMI160 假设接线是：

```text
X2600 SPI0:
    SCLK = PD00
    MOSI = PD01
    MISO = PD02
    CS   = PD05
```

IConfigTool 里配置的是：

```text
[root]
  → [模块化驱动]
    → [x2600驱动列表]
      → [SPI 总线]
```

选择：

```text
选择 SPI0 = y

CLK  = PD00
MOSI = PD01
MISO = PD02
```

注意：这里主要是配 **SPI 控制器的三根总线线**。

`CS=PD05` 一般不在 SPI 总线那里配，而是在注册具体 SPI 设备的时候配：

```c
.controller_data = (void *)GPIO_PD(5),
.chip_select = 0,
```

文档里的 demo 也是这种写法，`.controller_data` 用来指定片选 GPIO。

---

# 2. IConfigTool 里 SPI 相关模块要勾上

在：

```text
[root]
  → [模块化驱动]
    → [驱动]
```

建议勾：

```text
spi设备辅助模块（动态增加或删除spi设备）
spi设备驱动模块（spi设备驱动端）
```

调试阶段建议保留，方便用 `cmd_spi` 或 spidev 测总线。
但是最终 BMI160 IIO 驱动 **不是靠 cmd_spi 读取数据**，cmd_spi 只是调试 SPI 总线用的。

---

# 3. 编译 module_driver，让 soc_spi.ko 真正更新

改完 IConfigTool 后，先确认配置真的保存了：

```sh
cd /home/vik/project/x2600

grep -R "SPI0\|SPI1\|PD00\|PD01\|PD02\|PD05\|MD_SPI\|MD_X2600.*SPI" \
    build/configs build/.config.in 2>/dev/null
```

然后编译 module_driver：

```sh
make help | grep -i module
make module_driver
```

编译完确认新的 SPI 模块：

```sh
find . -name "soc_spi.ko" -exec ls -l {} \;
find . -name "*spi*.ko" | sort
```

如果你只 `make kernel`，但是没有重新编译/打包 module_driver，那板子上还是旧的 `soc_spi.ko`，IConfigTool 改的 IO 就不会生效。

---

# 4. Kernel 里打开 IIO + BMI160 SPI

进入内核：

```sh
cd /home/vik/project/x2600/kernel/kernel
make menuconfig
```

配置目标：

```text
CONFIG_SPI=y 或 m
CONFIG_REGMAP_SPI=y 或 m

CONFIG_IIO=m
CONFIG_IIO_BUFFER=m
CONFIG_IIO_TRIGGERED_BUFFER=m

CONFIG_BMI160=m
CONFIG_BMI160_SPI=m
CONFIG_BMI160_I2C=n
```

你现在走君正 module_driver SPI 方式，我建议先都做成模块：

```text
soc_spi.ko
industrialio.ko
kfifo_buf.ko
industrialio-triggered-buffer.ko
bmi160_core.ko
bmi160_spi.ko
bmi160_spi_board.ko
```

先模块验证通了，再考虑全部内建。

---

# 5. 写一个 bmi160_spi_board.c 注册 BMI160 设备

这个文件的作用很简单：**告诉 Linux SPI 框架，SPI0 上挂了一个 BMI160，CS 是 PD05。**

可以放到：

```text
kernel/kernel/drivers/iio/imu/bmi160/bmi160_spi_board.c
```

代码示例：

```c
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/spi/spi.h>
#include <soc/gpio.h>

/*
 * X2600 + BMI160 SPI board info
 *
 * SPI0:
 *   SCLK = PD00
 *   MOSI = PD01
 *   MISO = PD02
 *   CS   = PD05
 */

#define BMI160_SPI_BUS_NUM     0

static struct spi_device *bmi160_spi_dev;

static struct spi_board_info bmi160_spi_board_info = {
    /*
     * 必须和 drivers/iio/imu/bmi160/bmi160_spi.c
     * 里面的 spi_device_id 匹配。
     */
    .modalias = "bmi160",

    /*
     * 调试先低速，稳定后再提高。
     */
    .max_speed_hz = 1 * 1000 * 1000,

    /*
     * BMI160 常见用 SPI_MODE_0。
     * 如果 probe 失败，可以再试 SPI_MODE_3。
     */
    .mode = SPI_MODE_0,

    /*
     * 君正 SPI 驱动一般通过 controller_data 传 GPIO 片选。
     * 你的 CS 是 PD05。
     */
    .controller_data = (void *)GPIO_PD(5),

    /*
     * 同一 SPI 总线上的逻辑片选号。
     */
    .chip_select = 0,

    /*
     * SPI0。
     */
    .bus_num = BMI160_SPI_BUS_NUM,
};

static int __init x2600_bmi160_spi_board_init(void)
{
    struct spi_master *master;

    pr_info("x2600-bmi160: register bmi160 on SPI%d CS=PD05\n",
            BMI160_SPI_BUS_NUM);

    master = spi_busnum_to_master(BMI160_SPI_BUS_NUM);
    if (!master) {
        pr_err("x2600-bmi160: failed to get spi master %d\n",
               BMI160_SPI_BUS_NUM);
        return -ENODEV;
    }

    bmi160_spi_dev = spi_new_device(master, &bmi160_spi_board_info);

    spi_master_put(master);

    if (!bmi160_spi_dev) {
        pr_err("x2600-bmi160: failed to create spi device\n");
        return -ENODEV;
    }

    pr_info("x2600-bmi160: spi device created: %s\n",
            dev_name(&bmi160_spi_dev->dev));

    return 0;
}

static void __exit x2600_bmi160_spi_board_exit(void)
{
    if (bmi160_spi_dev) {
        pr_info("x2600-bmi160: unregister spi device\n");
        spi_unregister_device(bmi160_spi_dev);
        bmi160_spi_dev = NULL;
    }
}

module_init(x2600_bmi160_spi_board_init);
module_exit(x2600_bmi160_spi_board_exit);

MODULE_DESCRIPTION("X2600 BMI160 SPI board device");
MODULE_AUTHOR("Vik");
MODULE_LICENSE("GPL");
```

然后改：

```text
kernel/kernel/drivers/iio/imu/bmi160/Makefile
```

加一行：

```makefile
obj-m += bmi160_spi_board.o
```

或者如果你想受 `CONFIG_BMI160_SPI` 控制：

```makefile
obj-$(CONFIG_BMI160_SPI) += bmi160_spi_board.o
```

---

# 6. 确认 modalias 是不是 `"bmi160"`

编译前先查源码：

```sh
cd /home/vik/project/x2600/kernel/kernel

grep -R "spi_device_id" -n drivers/iio/imu/bmi160
grep -R "\"bmi160" -n drivers/iio/imu/bmi160
```

如果看到：

```c
static const struct spi_device_id bmi160_spi_id[] = {
    { "bmi160", 0 },
    { }
};
```

那 board 文件里就写：

```c
.modalias = "bmi160",
```

如果你源码里名字不是 `"bmi160"`，就以源码为准。

---

# 7. 编译 kernel modules 和 module_driver

回到工程目录：

```sh
cd /home/vik/project/x2600
make kernel
make module_driver
```

然后重新打包 rootfs/system：

```sh
make
```

重点：你要把这些 ko 都烧进板子：

```text
soc_spi.ko
industrialio.ko
kfifo_buf.ko
industrialio-triggered-buffer.ko
bmi160_core.ko
bmi160_spi.ko
bmi160_spi_board.ko
```

如果只更新 kernel，不更新 rootfs，板子上还是旧 ko。

---

# 8. 板子启动后加载顺序

上板后先看 SPI master 有没有起来：

```sh
ls /sys/class/spi_master/
dmesg | grep -iE "spi|ssi"
```

然后按顺序加载：

```sh
insmod utils.ko
insmod soc_spi.ko
```

具体 `utils.ko` 是否需要，看你君正 SDK 依赖。你之前 SPI 模块大概率需要它。

再加载 IIO 相关模块：

```sh
insmod industrialio.ko
insmod kfifo_buf.ko
insmod industrialio-triggered-buffer.ko
```

再加载 BMI160：

```sh
insmod bmi160_core.ko
insmod bmi160_spi.ko
insmod bmi160_spi_board.ko
```

如果模块不在当前目录，就先找：

```sh
find / -name "soc_spi.ko" 2>/dev/null
find / -name "bmi160*.ko" 2>/dev/null
find / -name "*iio*.ko" 2>/dev/null
find / -name "industrialio*.ko" 2>/dev/null
```

---

# 9. 看 probe 是否成功

执行：

```sh
dmesg | grep -iE "bmi|iio|spi"
```

正常应该能看到类似：

```text
x2600-bmi160: register bmi160 on SPI0 CS=PD05
x2600-bmi160: spi device created: spi0.0
bmi160_spi spi0.0: ...
```

再看 SPI 设备：

```sh
ls -l /sys/bus/spi/devices/
```

期望有：

```text
spi0.0
```

看它的 modalias：

```sh
cat /sys/bus/spi/devices/spi0.0/modalias
```

期望是：

```text
bmi160
```

---

# 10. 读取 IIO 数据

看 IIO 设备：

```sh
ls /sys/bus/iio/devices/
```

找到设备名：

```sh
for d in /sys/bus/iio/devices/iio:device*; do
    echo "==== $d ===="
    cat $d/name 2>/dev/null
done
```

如果成功，应该能看到：

```text
bmi160
```

读取加速度原始值：

```sh
cat /sys/bus/iio/devices/iio:device0/in_accel_x_raw
cat /sys/bus/iio/devices/iio:device0/in_accel_y_raw
cat /sys/bus/iio/devices/iio:device0/in_accel_z_raw
```

读取陀螺仪原始值：

```sh
cat /sys/bus/iio/devices/iio:device0/in_anglvel_x_raw
cat /sys/bus/iio/devices/iio:device0/in_anglvel_y_raw
cat /sys/bus/iio/devices/iio:device0/in_anglvel_z_raw
```

看 scale：

```sh
cat /sys/bus/iio/devices/iio:device0/in_accel_scale
cat /sys/bus/iio/devices/iio:device0/in_anglvel_scale
```

实际物理值大概是：

```text
加速度物理值 = in_accel_x_raw   × in_accel_scale
角速度物理值 = in_anglvel_x_raw × in_anglvel_scale
```

---

# 11. 如果没有 IIO 节点，按这个倒推

## A. 没有 `/sys/class/spi_master/spi0`

说明 `soc_spi.ko` 没起来，或者 IConfigTool 的 SPI0 没配好。

查：

```sh
lsmod
dmesg | grep -iE "spi|ssi|gpio"
```

重点看 `soc_spi` 有没有加载。

---

## B. 有 spi0，但加载 `bmi160_spi_board.ko` 报错

如果看到：

```text
failed to get spi master 0
```

说明 board 模块加载太早了，`soc_spi.ko` 还没加载，或者总线号不是 0。

先确认：

```sh
ls /sys/class/spi_master/
```

如果只有：

```text
spi1
```

那你的代码里就要改：

```c
#define BMI160_SPI_BUS_NUM 1
```

---

## C. 有 `spi0.0`，但是没有 IIO

查：

```sh
cat /sys/bus/spi/devices/spi0.0/modalias
dmesg | grep -i bmi
```

如果 modalias 不是 `bmi160`，说明 board 文件写错。

如果 modalias 是 `bmi160`，但是驱动没绑定，查：

```sh
ls /sys/bus/spi/drivers/
ls /sys/bus/spi/drivers/bmi160*
```

---

## D. probe 失败，读 chip id 失败

重点查：

```text
1. CS 是否真的是 PD05
2. IConfigTool 里的 SPI0 IO 是否真的是 PD00/PD01/PD02
3. SPI mode 先试 SPI_MODE_0，再试 SPI_MODE_3
4. max_speed_hz 先降到 500k 或 1MHz
5. BMI160 供电是否正常
6. SCLK/MOSI/MISO/CS 是否有波形
7. spidev/cmd_spi 是否占用了同一个 SPI0 CS0
```

不要让这个同时存在：

```text
/dev/spidev0.0
spi0.0 modalias=spidev
```

和：

```text
BMI160 使用 spi0.0
```

同一个 SPI0 CS0 只能绑定一个设备。

---

# 12. 最小验证路线

你现在最建议按这个顺序做：

```text
第一步：
    IConfigTool 配 SPI0:
        CLK=PD00
        MOSI=PD01
        MISO=PD02

第二步：
    编译 module_driver，确认 soc_spi.ko 更新。

第三步：
    上板加载 soc_spi.ko，确认：
        ls /sys/class/spi_master/
        能看到 spi0

第四步：
    加载 IIO:
        industrialio.ko
        kfifo_buf.ko
        industrialio-triggered-buffer.ko

第五步：
    加载:
        bmi160_core.ko
        bmi160_spi.ko
        bmi160_spi_board.ko

第六步：
    确认：
        ls /sys/bus/iio/devices/
        cat /sys/bus/iio/devices/iio:device0/name

第七步：
    读取：
        cat in_accel_x_raw
        cat in_anglvel_x_raw
```

一句话总结：**君正 IConfigTool 负责把 X2600 的 SPI0 总线配置出来；BMI160 则通过一个 `bmi160_spi_board.ko` 注册到这条 SPI0 总线上，再由 Linux 的 `bmi160_spi + bmi160_core` 变成 IIO 设备。**


--------
------
------
可以，**SPI 驱动可以编进内核里，不用模块化 `insmod soc_spi.ko` 的方式**。但是你要注意一个关键点：

```text
不搞模块化调用后，
IConfigTool 里 module_driver 那套 SPI IO 配置，
不一定还会自动生效。
```

君正文档主要讲的是 **IConfigTool 配 module_driver、再通过模块/辅助命令注册 SPI 设备** 的流程；文档里的示例也是编译 `m_spi_dev.ko` 后 `insmod` 加载。它说明了君正 SPI 设备注册仍然是基于 Linux SPI 框架，比如 `spi_register_driver()`、`spi_board_info`、`spi_write_then_read()`，但文档本身不是“全内建到 kernel”的流程。

---

# 结论先说

你可以做成这种最终形态：

```text
Linux Kernel 内建 SPI Controller
        ↓
Linux Kernel 内建 IIO
        ↓
Linux Kernel 内建 REGMAP_SPI
        ↓
Linux Kernel 内建 BMI160 core
        ↓
Linux Kernel 内建 BMI160 SPI
        ↓
Kernel 启动时注册 spi_device: bmi160
        ↓
生成 /sys/bus/iio/devices/iio:deviceX
```

这样板子启动后，不需要：

```sh
insmod soc_spi.ko
insmod bmi160_core.ko
insmod bmi160_spi.ko
insmod bmi160_spi_board.ko
```

---

# 但是不能只把 BMI160 改成 y

这个坑很重要。

你不能只改：

```text
CONFIG_BMI160=y
CONFIG_BMI160_SPI=y
```

然后 SPI 控制器还是 `soc_spi.ko`。

因为启动时顺序会变成：

```text
BMI160 内建驱动启动
        ↓
想找 SPI0 master
        ↓
但是 soc_spi.ko 还没加载
        ↓
找不到 spi0
        ↓
BMI160 probe 失败
```

所以如果你想彻底不搞模块，必须整条链路都内建：

```text
SPI controller = y
REGMAP_SPI     = y
IIO            = y
BMI160         = y
BMI160_SPI     = y
BMI160 board   = y
```

---

# 你需要改这 3 块

## 1. SPI 控制器驱动内建

进入 kernel：

```sh
cd /home/vik/project/x2600/kernel/kernel
make menuconfig
```

找：

```text
Device Drivers
  SPI support
```

目标是类似：

```text
CONFIG_SPI=y
CONFIG_SPI_MASTER=y
CONFIG_INGENIC_SPI=y
```

名字不一定叫 `CONFIG_INGENIC_SPI`，你可以搜：

```sh
grep -R "Ingenic SPI\|X2600.*SPI\|config .*SPI.*INGENIC\|config .*INGENIC.*SPI" \
    drivers/spi arch/mips -n
```

如果能找到君正 SPI controller 的 Kconfig，就把它改成 `y`。

---

## 2. IIO + BMI160 内建

目标配置：

```text
CONFIG_REGMAP_SPI=y

CONFIG_IIO=y
CONFIG_IIO_BUFFER=y
CONFIG_IIO_TRIGGERED_BUFFER=y

CONFIG_BMI160=y
CONFIG_BMI160_SPI=y
CONFIG_BMI160_I2C=n
```

确认：

```sh
grep -E "CONFIG_SPI=|CONFIG_REGMAP_SPI=|CONFIG_IIO=|CONFIG_BMI160" .config
```

期望看到：

```text
CONFIG_SPI=y
CONFIG_REGMAP_SPI=y
CONFIG_IIO=y
CONFIG_BMI160=y
CONFIG_BMI160_SPI=y
```

---

## 3. BMI160 这个 SPI 设备也要内建注册

BMI160 驱动只是“驱动”，它不会凭空知道你的板子上 SPI0 挂了 BMI160。

你还需要告诉内核：

```text
SPI0 上有一个设备
名字叫 bmi160
CS 是 PD05
mode 是 SPI_MODE_0
speed 是 1MHz
```

有两种方式：

---

# 方式 A：用 DTS 注册，偏标准

如果你的 X2600 kernel 已经用设备树管理 SPI，可以在对应 dts 里加类似：

```dts
&spi0 {
    status = "okay";

    bmi160@0 {
        compatible = "bosch,bmi160";
        reg = <0>;
        spi-max-frequency = <1000000>;
    };
};
```

但是你这个君正 SDK 未必完全按主线 DTS 走，而且 CS=PD05 可能还需要 pinctrl/gpio-cs 配置。
如果你现在是学习阶段，我更建议先用方式 B。

---

# 方式 B：写一个内建 board_info 文件，最直接

新建：

```sh
vim /home/vik/project/x2600/kernel/kernel/drivers/iio/imu/bmi160/bmi160_spi_board.c
```

写入：

```c
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/spi/spi.h>
#include <soc/gpio.h>

/*
 * X2600 + BMI160
 *
 * SPI0:
 *   SCLK = PD00
 *   MOSI = PD01
 *   MISO = PD02
 *   CS   = PD05
 */

static struct spi_board_info x2600_bmi160_spi_board_info __initdata = {
    /*
     * 必须和 bmi160_spi.c 里的 spi_device_id 匹配
     */
    .modalias = "bmi160",

    /*
     * 调试阶段先低速，稳定后可以提高
     */
    .max_speed_hz = 1 * 1000 * 1000,

    /*
     * BMI160 常见先用 SPI_MODE_0
     * 如果读不到芯片，再试 SPI_MODE_3
     */
    .mode = SPI_MODE_0,

    /*
     * 君正 SPI 驱动常用 controller_data 传 GPIO 片选
     * 你的 CS 是 PD05
     */
    .controller_data = (void *)GPIO_PD(5),

    /*
     * SPI0 CS0
     */
    .chip_select = 0,

    /*
     * SPI 总线号
     */
    .bus_num = 0,
};

static int __init x2600_bmi160_spi_board_init(void)
{
    pr_info("x2600-bmi160: register BMI160 on SPI0 CS=PD05\n");

    /*
     * 内建场景建议用 spi_register_board_info。
     * 它可以先登记 board_info，等 spi master 注册后再创建设备。
     */
    return spi_register_board_info(&x2600_bmi160_spi_board_info, 1);
}
arch_initcall(x2600_bmi160_spi_board_init);
```

然后改：

```sh
vim /home/vik/project/x2600/kernel/kernel/drivers/iio/imu/bmi160/Makefile
```

加：

```makefile
obj-$(CONFIG_BMI160_SPI) += bmi160_spi_board.o
```

---

# 关键问题：SPI0 的 IO 从哪里配？

如果不用 module_driver，那么你不能再单纯依赖：

```text
IConfigTool -> module_driver -> soc_spi.ko
```

这套了。

你要找 kernel 里 SPI0 的 IO 配置在哪里：

```sh
cd /home/vik/project/x2600/kernel/kernel

grep -R "spi0\|ssi0\|PD00\|PD01\|PD02\|PB29\|PB30\|PB31" \
    arch/mips drivers/spi 2>/dev/null | head -200
```

如果搜到 DTS：

```text
arch/mips/boot/dts/xxx.dts
```

那就改 DTS/pinctrl。

如果搜到 board 文件：

```text
arch/mips/xxx/board-xxx.c
```

那就改 board 文件。

如果完全搜不到，那说明你原来 SPI0 的 IO 很可能只在 `module_driver/soc_spi.ko` 那套里配置。
这种情况下，要全内建就得把君正 `soc_spi` 的配置逻辑迁进 kernel，或者找到它生成的头文件/宏是怎么被使用的。

---

# 最稳的操作路线

你可以按这个顺序来，不容易乱：

```text
第一阶段：确认 SPI controller 能内建
    只改 SPI controller = y
    编译烧录
    启动后看 /sys/class/spi_master/

第二阶段：确认 SPI0 IO 正确
    看 dmesg
    看 debugfs pinctrl/gpio
    用临时 spi 测试确认 CLK/MOSI/MISO/CS 有波形

第三阶段：内建 IIO + BMI160
    CONFIG_IIO=y
    CONFIG_BMI160=y
    CONFIG_BMI160_SPI=y

第四阶段：加入 bmi160_spi_board.c
    注册 SPI0 CS=PD05 的 bmi160 设备

第五阶段：启动后读取 IIO
    cat /sys/bus/iio/devices/iio:device*/name
    cat /sys/bus/iio/devices/iio:device0/in_accel_x_raw
```

---

# 启动后验证

启动后执行：

```sh
dmesg | grep -iE "spi|bmi|iio"
```

看 SPI master：

```sh
ls /sys/class/spi_master/
```

看 SPI device：

```sh
ls /sys/bus/spi/devices/
cat /sys/bus/spi/devices/spi0.0/modalias
```

期望：

```text
bmi160
```

看 IIO：

```sh
ls /sys/bus/iio/devices/

for d in /sys/bus/iio/devices/iio:device*; do
    echo "==== $d ===="
    cat $d/name
done
```

期望：

```text
bmi160
```

读取数据：

```sh
cat /sys/bus/iio/devices/iio:device0/in_accel_x_raw
cat /sys/bus/iio/devices/iio:device0/in_accel_y_raw
cat /sys/bus/iio/devices/iio:device0/in_accel_z_raw

cat /sys/bus/iio/devices/iio:device0/in_anglvel_x_raw
cat /sys/bus/iio/devices/iio:device0/in_anglvel_y_raw
cat /sys/bus/iio/devices/iio:device0/in_anglvel_z_raw
```

---

# 我的建议

你现在不要一上来就全部内建。建议先这样走：

```text
1. 先保持 soc_spi.ko 模块方式，把 BMI160 IIO 跑通
2. 确认能生成 iio:deviceX，能读 accel/gyro raw
3. 再把 soc_spi、IIO、BMI160 逐步改成内建
```

因为如果你一次性全内建，出问题时很难判断是：

```text
SPI controller 没起来
SPI IO 没配对
CS 不对
BMI160 modalias 不匹配
IIO 依赖没开
probe 顺序不对
```

一句话：**可以不搞模块化，SPI 驱动能进内核；但你要把 SPI controller、SPI IO 配置、IIO、BMI160、board 注册这整条链都放到 kernel 体系里，不能只改 BMI160。**


---------
-------
-----

对，你这个理解基本对。

**如果不走君正 module_driver / IConfigTool 那套模块化 SPI 配置，那 SPI 的 IO 就必须在 kernel 体系里配置出来。**
这个“kernel 体系”一般有两种写法：

```text
方式 1：设备树 DTS / DTSI 配 SPI0、pinctrl、CS、BMI160 子节点
方式 2：老式 board.c / platform data 里写 SPI 控制器、pinmux、spi_board_info
```

不一定 100% 必须是设备树，但必须有一个地方告诉内核：

```text
SPI0 控制器启用
SPI0 的 SCLK/MOSI/MISO 复用到哪些 GPIO
BMI160 挂在 SPI0 的 CS 几
BMI160 的 CS GPIO 是 PD05
BMI160 的 mode/speed 是多少
```

君正文档里的 IConfigTool 是在“模块化驱动”里配置 SPI 控制器或 GPIO_SPI，并且示例里 CS 是通过 `spi_board_info.controller_data = GPIO_PC(16)` 这种板级信息传进去的；如果你不再走这个模块化配置，那这些信息就要搬到 kernel 的 DTS 或 board 文件里。

---

# 先分清两类 IO 配置

## 1. SCLK / MOSI / MISO 属于 SPI 控制器 pinmux

比如你的 BMI160：

```text
SCLK = PD00
MOSI = PD01
MISO = PD02
```

这三个不是 BMI160 驱动管的，而是 **SPI0 控制器驱动** 管的。

所以应该配置在：

```text
spi0 节点的 pinctrl
```

或者老式平台代码里的：

```text
SPI0 pinmux 配置表
```

---

## 2. CS 属于 SPI 设备片选

比如：

```text
CS = PD05
```

这个可以有两种方式：

```text
方式 A：SPI 控制器原生 CS
方式 B：普通 GPIO 模拟 CS
```

你之前君正文档里的方式更像是 **GPIO CS**：

```c
.controller_data = (void *)GPIO_PC(16)
.chip_select = 0
```

你换成 BMI160 就是：

```c
.controller_data = (void *)GPIO_PD(5)
.chip_select = 0
```

如果走设备树，一般会变成类似：

```dts
cs-gpios = <&gpd 5 GPIO_ACTIVE_LOW>;
```

但具体写法要看君正 X2600 这套 kernel 的 gpio/pinctrl 绑定格式。

---

# 如果 X2600 支持设备树，大概是这种结构

注意：下面是**思路模板**，不是保证你 SDK 里可以直接复制。不同君正内核的 pinctrl/gpio 名字可能不一样。

```dts
&spi0 {
    status = "okay";

    pinctrl-names = "default";
    pinctrl-0 = <&spi0_pd_pins>;

    cs-gpios = <&gpd 5 GPIO_ACTIVE_LOW>;

    bmi160@0 {
        compatible = "bosch,bmi160";
        reg = <0>;
        spi-max-frequency = <1000000>;
    };
};

&pinctrl {
    spi0_pd_pins: spi0-pd-pins {
        /* 这里具体格式要看 X2600 pinctrl 驱动 */
        pins = "PD00", "PD01", "PD02";
        function = "spi0";
    };
};
```

逻辑是：

```text
&spi0:
    启用 SPI0 控制器

pinctrl-0:
    把 PD00/PD01/PD02 复用成 SPI0 SCLK/MOSI/MISO

cs-gpios:
    使用 PD05 作为片选

bmi160@0:
    SPI0 CS0 上挂了一个 BMI160
```

---

# 但是你这个君正 SDK 可能不是纯设备树

你之前日志里有：

```text
OF: fdt: No chosen node found, continuing without
MIPS: machine is ingenic,x2600
```

这说明它有 FDT 痕迹，但不一定像主线 Linux 那样完整使用设备树管理所有外设。

所以你要先查源码里 SPI0 到底在哪里配置。

在编译主机上执行：

```sh
cd /home/vik/project/x2600/kernel/kernel

echo "===== 查设备树 ====="
grep -R "spi0\|spi1\|ssi0\|ssi1\|bmi160\|cs-gpios\|pinctrl" \
    arch/mips/boot/dts 2>/dev/null | head -200

echo "===== 查板级 C 文件 ====="
grep -R "spi0\|spi1\|ssi0\|ssi1\|spi_board_info\|spi_register_board_info\|GPIO_PD\|PD05" \
    arch/mips 2>/dev/null | head -200

echo "===== 查 SPI 控制器驱动 ====="
grep -R "Ingenic SPI\|ingenic.*spi\|jz.*spi\|ssi" \
    drivers/spi arch/mips 2>/dev/null | head -200
```

如果搜到 `.dts/.dtsi` 里有 `spi0`，那就优先改设备树。
如果搜到 `board-xxx.c` 里注册 SPI，那就改 board 文件。
如果只在 `module_driver` 里搜得到，那说明你要么继续用 module_driver，要么得把那部分 SPI controller/pinmux 注册逻辑迁进 kernel。

---

# 如果不用设备树，也可以用 board_info 注册 BMI160

即使 SCLK/MOSI/MISO 走 kernel 里的 SPI0 配置，BMI160 设备本身也可以用 `spi_board_info` 注册：

```c
static struct spi_board_info x2600_bmi160_info __initdata = {
    .modalias = "bmi160",
    .bus_num = 0,
    .chip_select = 0,
    .max_speed_hz = 1000000,
    .mode = SPI_MODE_0,
    .controller_data = (void *)GPIO_PD(5),
};

static int __init x2600_bmi160_init(void)
{
    return spi_register_board_info(&x2600_bmi160_info, 1);
}
arch_initcall(x2600_bmi160_init);
```

这个只解决：

```text
SPI0 上有 BMI160，CS=PD05
```

它不解决：

```text
PD00/PD01/PD02 复用成 SPI0
```

所以 SCLK/MOSI/MISO 仍然要在 SPI0 controller 的 pinmux 里配置。

---

# 你的最终内建方案应该长这样

```text
kernel DTS / board.c:
    配 SPI0 controller
    配 SPI0 pinmux:
        SCLK = PD00
        MOSI = PD01
        MISO = PD02
    配 CS:
        CS = PD05
    注册 BMI160:
        modalias / compatible = bmi160

kernel .config:
    CONFIG_SPI=y
    CONFIG_REGMAP_SPI=y
    CONFIG_IIO=y
    CONFIG_IIO_BUFFER=y
    CONFIG_IIO_TRIGGERED_BUFFER=y
    CONFIG_BMI160=y
    CONFIG_BMI160_SPI=y
```

启动后不需要 `insmod soc_spi.ko`，直接应该能看到：

```sh
ls /sys/class/spi_master/
ls /sys/bus/spi/devices/
ls /sys/bus/iio/devices/
```

---

# 我的建议

你现在先别急着写 DTS。先查 X2600 当前 SPI0 是哪里注册的：

```sh
cd /home/vik/project/x2600/kernel/kernel

grep -R "spi0\|ssi0\|SPI0\|MD_X2600_SPI\|GPIO_PD" \
    arch/mips drivers/spi include 2>/dev/null | head -300
```

如果你把这个输出贴出来，我就能判断你这套 SDK 应该走：

```text
A. 改 dts/dtsi
B. 改 board.c
C. 从 module_driver 抽 SPI pinmux 配置到 kernel
D. 继续 module_driver 更合适
```

一句话：**不搞 module_driver 后，SPI 的 SCLK/MOSI/MISO/CS 必须在 kernel 里重新落地；能用设备树最好，不能用设备树就用 board 文件/pinctrl/platform data。**



---------
----------
-------

不是，不用设备树也**能用 IIO + SPI 驱动 BMI160**。

你现在这个判断差了一点点：**君正推荐的 SPI 用法，不等于只能用 `cmd_spi/spidev` 应用层读写。** 它文档里的 SPI 设备驱动示例本身就用了 Linux 标准 SPI 框架：`struct spi_driver`、`spi_register_driver()`、`spi_board_info`、`spi_write_then_read()`。这说明君正这套 SPI 控制器模块最终还是会注册成 Linux 的 `spi_master`，只要有 `spi_master`，就可以接 Linux 的 `bmi160_spi` IIO 驱动。

真正关系是这样：

```text
IConfigTool 配 SPI0 的 CLK/MOSI/MISO
        ↓
编译 module_driver
        ↓
加载 soc_spi.ko
        ↓
soc_spi.ko 注册 Linux spi_master
        ↓
自己写 bmi160_spi_board.ko 注册 spi_device
        ↓
bmi160_spi.ko 匹配 modalias="bmi160"
        ↓
bmi160_core.ko 注册 IIO 设备
        ↓
/sys/bus/iio/devices/iio:deviceX
```

所以答案是：**可以用，不需要设备树；但是要多写一个 BMI160 的板级注册模块。**

---

## 你不能直接这样理解

错误理解是：

```text
君正 SPI 推荐方法 = cmd_spi/spidev
所以不能用 IIO
```

正确理解是：

```text
君正 SPI 推荐方法里面有两层：

1. 底层：soc_spi.ko 注册 Linux SPI master
2. 上层：cmd_spi/spidev 只是调试工具

IIO + BMI160 只需要第 1 层，不需要 cmd_spi。
```

`cmd_spi` 是给你测试 SPI 通不通的，不是最终传感器驱动方案。

---

# 你用君正推荐方式驱动 BMI160，应该这么搞

## 第一步：IConfigTool 配 SPI0

比如你的接线：

```text
SPI0_CLK   = PD00
SPI0_MOSI  = PD01
SPI0_MISO  = PD02
BMI160_CS  = PD05
```

IConfigTool 里只配 SPI 总线：

```text
[root]
  → [模块化驱动]
    → [x2600驱动列表]
      → [SPI 总线]
```

配置：

```text
选择 SPI0

CLK  = PD00
MOSI = PD01
MISO = PD02
```

`PD05` 这个 CS 不一定在这里配，它可以在 BMI160 的 board 模块里配。

---

## 第二步：编译 module_driver

改完 IConfigTool 后，不能只 `make kernel`。

要重新编译 module_driver：

```sh
cd /home/vik/project/x2600

make module_driver
```

然后重新打包系统/rootfs：

```sh
make
```

因为 `soc_spi.ko` 在 rootfs 里，不在 kernel image 里。
如果你只烧 kernel，SPI IO 配置可能还是旧的。

---

## 第三步：内核里打开 IIO + BMI160

进入 kernel：

```sh
cd /home/vik/project/x2600/kernel/kernel
make menuconfig
```

建议先全部做成模块：

```text
CONFIG_SPI=y 或 m
CONFIG_REGMAP_SPI=m

CONFIG_IIO=m
CONFIG_IIO_BUFFER=m
CONFIG_IIO_TRIGGERED_BUFFER=m

CONFIG_BMI160=m
CONFIG_BMI160_SPI=m
CONFIG_BMI160_I2C=n
```

也就是最后你会有这些 ko：

```text
industrialio.ko
kfifo_buf.ko
industrialio-triggered-buffer.ko
bmi160_core.ko
bmi160_spi.ko
```

---

## 第四步：写一个 `bmi160_spi_board.ko`

这个模块专门做一件事：

```text
告诉 Linux：
SPI0 上挂了一个 BMI160，
CS 是 PD05，
设备名字是 bmi160。
```

示例：

```c
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/spi/spi.h>
#include <soc/gpio.h>

/*
 * X2600 + BMI160
 *
 * SPI0:
 *   SCLK = PD00
 *   MOSI = PD01
 *   MISO = PD02
 *   CS   = PD05
 */

#define BMI160_SPI_BUS_NUM  0

static struct spi_device *bmi160_spi_dev;

static struct spi_board_info bmi160_spi_board_info = {
    .modalias = "bmi160",
    .max_speed_hz = 1 * 1000 * 1000,
    .mode = SPI_MODE_0,

    /*
     * 君正 SPI demo 里就是用 controller_data 传 CS GPIO。
     * 这里换成你的 PD05。
     */
    .controller_data = (void *)GPIO_PD(5),

    .chip_select = 0,
    .bus_num = BMI160_SPI_BUS_NUM,
};

static int __init x2600_bmi160_spi_board_init(void)
{
    struct spi_master *master;

    pr_info("x2600-bmi160: register bmi160 on SPI%d CS=PD05\n",
            BMI160_SPI_BUS_NUM);

    master = spi_busnum_to_master(BMI160_SPI_BUS_NUM);
    if (!master) {
        pr_err("x2600-bmi160: failed to get spi master %d\n",
               BMI160_SPI_BUS_NUM);
        return -ENODEV;
    }

    bmi160_spi_dev = spi_new_device(master, &bmi160_spi_board_info);
    spi_master_put(master);

    if (!bmi160_spi_dev) {
        pr_err("x2600-bmi160: failed to create spi device\n");
        return -ENODEV;
    }

    pr_info("x2600-bmi160: created %s\n",
            dev_name(&bmi160_spi_dev->dev));

    return 0;
}

static void __exit x2600_bmi160_spi_board_exit(void)
{
    if (bmi160_spi_dev) {
        spi_unregister_device(bmi160_spi_dev);
        bmi160_spi_dev = NULL;
    }
}

module_init(x2600_bmi160_spi_board_init);
module_exit(x2600_bmi160_spi_board_exit);

MODULE_DESCRIPTION("X2600 BMI160 SPI board device");
MODULE_LICENSE("GPL");
```

这个模块和设备树作用类似，但不需要你写 DTS。

---

# 上板加载顺序

大概这样：

```sh
insmod utils.ko
insmod soc_spi.ko
```

确认 SPI master：

```sh
ls /sys/class/spi_master/
```

应该能看到类似：

```text
spi0
```

然后加载 IIO：

```sh
insmod industrialio.ko
insmod kfifo_buf.ko
insmod industrialio-triggered-buffer.ko
```

再加载 BMI160：

```sh
insmod bmi160_core.ko
insmod bmi160_spi.ko
insmod bmi160_spi_board.ko
```

注意顺序：**一定是先 soc_spi.ko，再 bmi160_spi_board.ko。**

因为 `bmi160_spi_board.ko` 里面要找：

```c
spi_busnum_to_master(0)
```

如果 `soc_spi.ko` 还没加载，SPI0 master 不存在，它就会失败。

---

# 成功后应该看到什么

看 SPI 设备：

```sh
ls /sys/bus/spi/devices/
```

应该有：

```text
spi0.0
```

看它的名字：

```sh
cat /sys/bus/spi/devices/spi0.0/modalias
```

应该是：

```text
bmi160
```

看 IIO：

```sh
ls /sys/bus/iio/devices/
```

然后：

```sh
for d in /sys/bus/iio/devices/iio:device*; do
    echo "==== $d ===="
    cat $d/name
done
```

应该看到：

```text
bmi160
```

读取数据：

```sh
cat /sys/bus/iio/devices/iio:device0/in_accel_x_raw
cat /sys/bus/iio/devices/iio:device0/in_accel_y_raw
cat /sys/bus/iio/devices/iio:device0/in_accel_z_raw

cat /sys/bus/iio/devices/iio:device0/in_anglvel_x_raw
cat /sys/bus/iio/devices/iio:device0/in_anglvel_y_raw
cat /sys/bus/iio/devices/iio:device0/in_anglvel_z_raw
```

---

# 一句话给你定心

**君正 X2600 推荐的 IConfigTool + module_driver SPI 方式，可以用 IIO+SPI 驱动 BMI160。**

你不用设备树也可以，关键是：

```text
IConfigTool 负责 SPI0 总线 IO
soc_spi.ko 负责注册 Linux spi_master
bmi160_spi_board.ko 负责注册 SPI 设备
bmi160_spi.ko + bmi160_core.ko 负责生成 IIO 节点
```

你之前卡住的点不是“IIO+SPI 不能用”，而是少了中间这个：

```text
bmi160_spi_board.ko
```

它就是把君正 SPI 总线和 Linux BMI160 IIO 驱动接起来的桥。
