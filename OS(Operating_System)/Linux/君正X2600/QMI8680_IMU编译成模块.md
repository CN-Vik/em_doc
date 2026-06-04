可以，**不放进内核目录也能编译成 `.ko` 模块**。这叫 **out-of-tree 外部内核模块编译**。

但重点是：**不建议 push 到板子上编译**。君正 X2600 板子上一般没有完整 kernel headers、`.config`、`Module.symvers`、交叉/本机编译环境。正确方式是：

> 在 Ubuntu 主机上用 X2600 的内核源码 + 同一套交叉编译器编译 `.ko`，然后把 `.ko` push/scp 到板子上 `insmod`。

---

## 1. 你的目录可以这样用

你现在是：

```bash
/home/vik/project/x2600/module_driver/qmi8658a
├── Kconfig
├── Makefile
├── qmi8658a_core.c
├── qmi8658a.h
├── qmi8658a_i2c.c
└── qmi8658a_spi.c
```

如果你现在只想走 **SPI QMI8658A**，可以先不要管 `qmi8658a_i2c.c`。

建议外部模块 Makefile 先写成这样：

```makefile
# 当前只编译 SPI 版本 QMI8658A
obj-m += qmi8658a_core.o
obj-m += qmi8658a_spi.o

KERNEL_DIR ?= /home/vik/project/x2600/kernel/kernel
PWD := $(shell pwd)

ARCH ?= mips
CROSS_COMPILE ?= /home/vik/project/x2600/tools/toolchains/mips-xburst2-gcc720-glibc238/bin/你的交叉编译器前缀-

all:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules

clean:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) clean
```

其中这个地方你要确认一下：

```makefile
CROSS_COMPILE ?= /home/vik/project/x2600/tools/toolchains/mips-xburst2-gcc720-glibc238/bin/你的交叉编译器前缀-
```

你可以用这个命令看前缀：

```bash
ls /home/vik/project/x2600/tools/toolchains/mips-xburst2-gcc720-glibc238/bin/*gcc
```

比如如果显示：

```bash
mips-linux-gnu-gcc
```

那就是：

```makefile
CROSS_COMPILE ?= /home/vik/project/x2600/tools/toolchains/mips-xburst2-gcc720-glibc238/bin/mips-linux-gnu-
```

如果显示：

```bash
mipsel-linux-gnu-gcc
```

那就是：

```makefile
CROSS_COMPILE ?= /home/vik/project/x2600/tools/toolchains/mips-xburst2-gcc720-glibc238/bin/mipsel-linux-gnu-
```

---

## 2. 先确保内核已经编译过一次

外部模块编译依赖内核源码里的这些东西：

```bash
.config
include/generated/
scripts/
Module.symvers
```

所以你最好先在 X2600 项目里正常编译过一次内核：

```bash
cd /home/vik/project/x2600/build
make kernel
```

或者至少在内核源码目录执行过：

```bash
cd /home/vik/project/x2600/kernel/kernel

make ARCH=mips CROSS_COMPILE=你的交叉编译器前缀- modules_prepare
```

不过注意：`modules_prepare` 不一定生成完整的 `Module.symvers`。如果你要编译依赖 IIO、REGMAP、SPI 等符号的模块，最好还是完整编译过一次 kernel/modules。

---

## 3. 在你的 qmi8658a 目录直接编译

```bash
cd /home/vik/project/x2600/module_driver/qmi8658a
make
```

成功后应该生成：

```bash
qmi8658a_core.ko
qmi8658a_spi.ko
```

如果你只做 SPI，加载顺序一般是：

```bash
insmod qmi8658a_core.ko
insmod qmi8658a_spi.ko
```

---

## 4. 关键问题：core 里的函数要 EXPORT_SYMBOL

如果 `qmi8658a_spi.c` 调用了 `qmi8658a_core.c` 里的函数，比如：

```c
qmi8658a_core_probe()
qmi8658a_core_remove()
```

那你在 `qmi8658a_core.c` 里需要导出符号：

```c
EXPORT_SYMBOL_GPL(qmi8658a_core_probe);
EXPORT_SYMBOL_GPL(qmi8658a_core_remove);
```

并且 `qmi8658a_core.c` 里要有：

```c
#include <linux/export.h>
#include <linux/module.h>

MODULE_LICENSE("GPL");
```

不然你编译或者加载 `qmi8658a_spi.ko` 时可能会遇到：

```bash
Unknown symbol qmi8658a_core_probe
Unknown symbol qmi8658a_core_remove
```

---

## 5. IIO / SPI / REGMAP 依赖不能少

虽然你把驱动拿出来了，但是它还是依赖内核里的框架：

```text
IIO
SPI
REGMAP
REGMAP_SPI
IIO_BUFFER
IIO_TRIGGERED_BUFFER
```

所以内核 `.config` 里至少要有类似这些：

```text
CONFIG_MODULES=y
CONFIG_IIO=y 或 m
CONFIG_SPI=y
CONFIG_REGMAP=y
CONFIG_REGMAP_SPI=y
CONFIG_IIO_BUFFER=y
CONFIG_IIO_TRIGGERED_BUFFER=y
```

如果这些框架没有编进内核或者模块，你的 `qmi8658a_spi.ko` 会加载失败。

可以在主机上查：

```bash
cd /home/vik/project/x2600/kernel/kernel

grep -E "CONFIG_MODULES|CONFIG_IIO|CONFIG_SPI|CONFIG_REGMAP|CONFIG_IIO_BUFFER|CONFIG_IIO_TRIGGERED_BUFFER" .config
```

---

## 6. 编译成功后 push 到板子

比如用 `scp`：

```bash
scp qmi8658a_core.ko qmi8658a_spi.ko root@板子IP:/tmp/
```

或者如果你用 adb：

```bash
adb push qmi8658a_core.ko /tmp/
adb push qmi8658a_spi.ko /tmp/
```

板子上加载：

```bash
cd /tmp

insmod qmi8658a_core.ko
insmod qmi8658a_spi.ko
```

看日志：

```bash
dmesg | tail -n 80
```

看模块：

```bash
lsmod
```

看 IIO 设备：

```bash
ls /sys/bus/iio/devices/
```

如果成功，可能会出现：

```bash
/sys/bus/iio/devices/iio:device0
```

继续看：

```bash
cat /sys/bus/iio/devices/iio:device0/name
ls /sys/bus/iio/devices/iio:device0/
```

---

## 7. 还有一个重点：SPI 设备本身要被注册

光加载 `qmi8658a_spi.ko` 不一定会自动 probe。SPI 驱动加载后，还需要系统里存在对应的 SPI device。

也就是内核需要有类似设备树或者 board info：

```dts
&qspi0 {
    status = "okay";

    qmi8658a@0 {
        compatible = "qst,qmi8658a";
        reg = <0>;
        spi-max-frequency = <5000000>;
    };
};
```

或者你之前 BMI160 用的是单独的 `bmi160_spi_board.ko` 注册 SPI 设备，那 QMI8658A 也需要类似一个：

```text
qmi8658a_spi_board.ko
```

用于动态创建 SPI 设备。

否则会出现这种情况：

```bash
insmod qmi8658a_spi.ko
```

没有报错，但是也没有 probe 日志，`/sys/bus/iio/devices/` 里也没有设备。

因为驱动只是注册了：

```c
spi_register_driver()
```

但系统里没有匹配的 SPI 从设备。

---

## 8. 所以你的实际路线应该是这样

你现在最稳的路线：

```text
1. qmi8658a_core.ko
   ↓
2. qmi8658a_spi.ko
   ↓
3. qmi8658a_spi_board.ko  或者设备树里添加 qmi8658a 节点
   ↓
4. /sys/bus/iio/devices/iio:deviceX 出现
   ↓
5. 读取 accel / gyro 数据
```

如果你不想改设备树，那就模仿你之前的：

```text
bmi160_spi_board.ko
```

做一个：

```text
qmi8658a_spi_board.ko
```

这个模块负责告诉 SPI core：

> SPI0/1 上挂了一个 compatible/name 为 qmi8658a 的设备，CS 用哪个引脚，最大 SPI 频率是多少。

---

## 结论

你这个目录完全可以直接编译 `.ko`，但推荐流程是：

```bash
主机交叉编译 ko
    ↓
push 到板子
    ↓
insmod 加载
    ↓
dmesg / sysfs / iio 验证
```

不是：

```bash
push 源码到板子上编译
```

板子上编译不推荐，除非你把完整 kernel build 环境、源码、`.config`、Module.symvers、编译器全部搬上去。

你下一步可以先做这三个检查：

```bash
cd /home/vik/project/x2600/kernel/kernel

grep CONFIG_MODULES .config
grep CONFIG_IIO .config
grep CONFIG_REGMAP_SPI .config
```

然后在 `module_driver/qmi8658a` 里先编译出：

```text
qmi8658a_core.ko
qmi8658a_spi.ko
```

之后再解决 **SPI device 注册 / CS 引脚 / probe 匹配** 这个问题。


-------
-----
----

影响不大，但要分清 **编译阶段** 和 **加载阶段**。

`CONFIG_IIO_TRIGGERED_BUFFER=m` 的意思是：

```text
IIO triggered buffer 框架被编译成模块
模块名一般是：industrialio-triggered-buffer.ko
```

它的作用是给 IIO 驱动提供触发缓冲区辅助函数，比如很多 IMU 驱动里会用：

```c
iio_triggered_buffer_setup()
iio_triggered_buffer_cleanup()
devm_iio_triggered_buffer_setup()
```

这些接口通常就依赖 `CONFIG_IIO_TRIGGERED_BUFFER`。它是 IIO buffer 的辅助框架，内核 Kconfig 说明也是 “Provides helper functions for setting up triggered buffers”。([Cateee][1])

---

## 对你现在 QMI8658A 外部 KO 的影响

### 情况 1：你的 QMI8658A 驱动用了 triggered buffer

比如代码里有这些：

```c
#include <linux/iio/triggered_buffer.h>
#include <linux/iio/trigger_consumer.h>
```

或者函数里有：

```c
iio_triggered_buffer_setup(...)
iio_triggered_buffer_cleanup(...)
```

那就有影响。

加载顺序要变成类似：

```bash
insmod industrialio.ko
insmod industrialio-trigger.ko
insmod kfifo_buf.ko
insmod industrialio-triggered-buffer.ko

insmod qmi8658a_core.ko
insmod qmi8658a_spi.ko
```

如果这些 IIO 基础模块已经被内核自动加载了，那你不用手动管。

---

### 情况 2：你的 QMI8658A 驱动只是普通 sysfs 读取

比如只是注册 IIO device，然后读：

```bash
cat /sys/bus/iio/devices/iio:device0/in_accel_x_raw
cat /sys/bus/iio/devices/iio:device0/in_anglvel_x_raw
```

没有用 triggered buffer，那影响不大。

这种情况下，主要依赖是：

```text
CONFIG_IIO
CONFIG_SPI
CONFIG_REGMAP
CONFIG_REGMAP_SPI
```

`CONFIG_IIO_TRIGGERED_BUFFER=m` 不一定必须用到。

---

## `=m` 和 `=y` 的区别

如果是：

```text
CONFIG_IIO_TRIGGERED_BUFFER=y
```

代表它直接编进内核，驱动加载时不会缺这个符号。

如果是：

```text
CONFIG_IIO_TRIGGERED_BUFFER=m
```

代表它是独立模块，**你的 QMI8658A 模块加载前，它必须已经被加载**。

否则 `insmod qmi8658a_core.ko` 或 `insmod qmi8658a_spi.ko` 时可能会报：

```bash
Unknown symbol iio_triggered_buffer_setup
Unknown symbol iio_triggered_buffer_cleanup
Unknown symbol devm_iio_triggered_buffer_setup
```

或者 `dmesg` 里看到类似：

```bash
qmi8658a_core: Unknown symbol iio_triggered_buffer_setup
```

---

## 你现在最建议怎么做？

在板子上先查有没有这个模块：

```bash
find /lib/modules/$(uname -r) -name "*triggered*"
find /lib/modules/$(uname -r) -name "*iio*"
```

如果找得到，可以用：

```bash
modprobe industrialio-triggered-buffer
```

如果板子没有 `modprobe`，就手动：

```bash
insmod /lib/modules/$(uname -r)/kernel/drivers/iio/industrialio.ko
insmod /lib/modules/$(uname -r)/kernel/drivers/iio/industrialio-trigger.ko
insmod /lib/modules/$(uname -r)/kernel/drivers/iio/kfifo_buf.ko
insmod /lib/modules/$(uname -r)/kernel/drivers/iio/industrialio-triggered-buffer.ko
```

不同内核版本路径可能不一样，用 `find` 找一下。

---

## 最简单结论

你这个配置：

```text
CONFIG_IIO_TRIGGERED_BUFFER=m
```

**可以用，不是问题。**

但是你要记住：

```text
=m：需要先加载 industrialio-triggered-buffer.ko
=y：已经在内核里，不用管加载顺序
```

如果你是新手调试阶段，我更建议先配置成：

```text
CONFIG_IIO=y
CONFIG_IIO_BUFFER=y
CONFIG_IIO_TRIGGER=y
CONFIG_IIO_TRIGGERED_BUFFER=y
CONFIG_IIO_KFIFO_BUF=y
CONFIG_REGMAP=y
CONFIG_REGMAP_SPI=y
```

这样先减少 `Unknown symbol` 和模块加载顺序问题。等驱动跑通了，再把它们改成 `m` 做模块化。

[1]: https://cateee.net/lkddb/web-lkddb/IIO_TRIGGERED_BUFFER.html?utm_source=chatgpt.com "CONFIG_IIO_TRIGGERED_BUF..."



-------
--------
--------

你现在先按这个顺序加载。**注意：先不要同时加载 BMI160 和 QMI8658A**，它们可能抢同一个 SPI/CS。

QMI8658A 的 `WHO_AM_I` 寄存器地址是 `0x00`，默认 ID 是 `0x05`，`REVISION_ID` 是 `0x01`，默认值常见是 `0x7C`。手动 SPI 读 ID 时，一般读命令发 `0x80 | reg`，所以读 `WHO_AM_I` 就是发 `0x80`，再读回一个 dummy 后面的数据。QMI8658A 数据手册里写明 `WHO_AM_I Register Address: 0x00`，默认值 `0x05`。([files.waveshare.com][1])

---

## 1. 先卸载旧模块

在板子上执行：

```bash
cd /tmp

rmmod qmi8658_spi_board 2>/dev/null
rmmod qmi8658a_spi 2>/dev/null
rmmod qmi8658a_core 2>/dev/null

rmmod bmi160_spi_board 2>/dev/null
rmmod bmi160_spi 2>/dev/null
rmmod bmi160_core 2>/dev/null

rmmod spidev_helper 2>/dev/null
rmmod spidev 2>/dev/null
```

然后清一下日志，方便看新日志：

```bash
dmesg -c
```

---

## 2. 加载 QMI8658A IIO 驱动

你可以先这样加载：

```bash
cd /tmp

insmod utils.ko
insmod soc_spi.ko

insmod kfifo_buf.ko
insmod industrialio-triggered-buffer.ko

insmod qmi8658a_core.ko
insmod qmi8658a_spi.ko
insmod qmi8658_spi_board.ko
```

如果某个模块提示：

```text
File exists
```

不用管，说明已经加载过了。

如果提示：

```text
Unknown symbol xxx
```

马上看：

```bash
dmesg | tail -n 80
```

---

## 3. 看 probe 有没有成功

加载完后立刻看日志：

```bash
dmesg | tail -n 100
```

正常情况下你希望看到类似：

```text
qmi8658a spi probe
who_am_i = 0x05
iio device registered
```

具体打印内容取决于你代码里有没有 `dev_info()` / `printk()`。

然后看 SPI 设备有没有出来：

```bash
ls /sys/bus/spi/devices/
```

可能会看到类似：

```text
spi0.0
spi1.0
```

再看 SPI driver：

```bash
ls /sys/bus/spi/drivers/
```

找一下有没有类似：

```text
qmi8658a
qmi8658a_spi
qmi8658
```

---

## 4. 看 IIO 节点

```bash
ls /sys/bus/iio/devices/
```

如果成功，应该有：

```text
iio:device0
```

然后：

```bash
cat /sys/bus/iio/devices/iio:device0/name
```

如果输出类似：

```text
qmi8658a
```

说明 IIO 设备注册成功了。

继续看有哪些属性：

```bash
ls /sys/bus/iio/devices/iio:device0/
```

常见可能有：

```text
in_accel_x_raw
in_accel_y_raw
in_accel_z_raw
in_anglvel_x_raw
in_anglvel_y_raw
in_anglvel_z_raw
in_temp_raw
```

然后直接读：

```bash
cat /sys/bus/iio/devices/iio:device0/in_accel_x_raw
cat /sys/bus/iio/devices/iio:device0/in_accel_y_raw
cat /sys/bus/iio/devices/iio:device0/in_accel_z_raw

cat /sys/bus/iio/devices/iio:device0/in_anglvel_x_raw
cat /sys/bus/iio/devices/iio:device0/in_anglvel_y_raw
cat /sys/bus/iio/devices/iio:device0/in_anglvel_z_raw
```

如果这些 raw 值能变化，说明驱动已经基本跑通。

---

## 5. 怎么专门测试读 ID？

### 方法一：看驱动 probe 日志，最推荐

你可以在 `qmi8658a_core.c` 的 probe 里加打印：

```c
ret = regmap_read(data->regmap, 0x00, &chip_id);
if (ret) {
    dev_err(dev, "read WHO_AM_I failed: %d\n", ret);
    return ret;
}

dev_info(dev, "QMI8658A WHO_AM_I = 0x%02x\n", chip_id);

if (chip_id != 0x05) {
    dev_err(dev, "bad chip id: 0x%02x\n", chip_id);
    return -ENODEV;
}
```

然后重新编译 `.ko`，板子上重新加载：

```bash
rmmod qmi8658_spi_board 2>/dev/null
rmmod qmi8658a_spi 2>/dev/null
rmmod qmi8658a_core 2>/dev/null

dmesg -c

insmod qmi8658a_core.ko
insmod qmi8658a_spi.ko
insmod qmi8658_spi_board.ko

dmesg | tail -n 80
```

如果通信正常，你应该能看到：

```text
QMI8658A WHO_AM_I = 0x05
```

这个是最稳的方式。

---

### 方法二：暂时不用 qmi8658a 驱动，用 spidev 直接读

这个方法适合排查硬件 SPI 通不通。

先卸载 QMI8658A：

```bash
rmmod qmi8658_spi_board 2>/dev/null
rmmod qmi8658a_spi 2>/dev/null
rmmod qmi8658a_core 2>/dev/null
```

再加载 spidev：

```bash
insmod utils.ko 2>/dev/null
insmod soc_spi.ko 2>/dev/null
insmod spidev.ko
insmod spidev_helper.ko
```

然后看设备：

```bash
ls /dev/spidev*
```

如果出来类似：

```text
/dev/spidev0.0
```

就可以用 SPI 工具读。

读 QMI8658A ID 时，SPI 发送两个字节：

```text
0x80 0x00
```

其中：

```text
0x80 = 0x00 | 0x80
```

意思是读寄存器 `0x00`。

期望收到大概：

```text
xx 05
```

第一个字节通常是 dummy，第二个字节才是 ID。

如果你的板子上有 `spidev_test`，可以试：

```bash
spidev_test -D /dev/spidev0.0 -s 1000000 -b 8 -p "\x80\x00"
```

期望 RX 里有：

```text
05
```

如果没有 `spidev_test`，那就先用驱动里的 `regmap_read()` 打印 ID，更简单。

---

## 6. 如果加载后没有任何 probe 日志

重点查这个：

```bash
ls /sys/bus/spi/devices/
```

如果 SPI device 没有出现，说明你的：

```text
qmi8658_spi_board.ko
```

没有成功注册 SPI 设备。

这时候看：

```bash
dmesg | tail -n 100
```

常见原因有：

```text
SPI bus 编号写错
CS 写错
modalias / compatible 和 qmi8658a_spi.c 不匹配
soc_spi.ko 没加载
SPI 控制器没起来
```

尤其要检查 `qmi8658_spi_board.c` 里的名字，必须和 `qmi8658a_spi.c` 的 `spi_device_id` 对得上。

比如驱动里如果是：

```c
static const struct spi_device_id qmi8658a_id[] = {
    { "qmi8658a", 0 },
    { }
};
MODULE_DEVICE_TABLE(spi, qmi8658a_id);
```

那 board 模块注册时也应该是：

```c
.modalias = "qmi8658a",
```

如果一个叫：

```text
qmi8658
```

另一个叫：

```text
qmi8658a
```

就不会 probe。

---

## 7. 你现在最应该先跑这一组命令

```bash
cd /tmp

rmmod qmi8658_spi_board 2>/dev/null
rmmod qmi8658a_spi 2>/dev/null
rmmod qmi8658a_core 2>/dev/null
rmmod bmi160_spi_board 2>/dev/null
rmmod bmi160_spi 2>/dev/null
rmmod bmi160_core 2>/dev/null

dmesg -c

insmod utils.ko 2>/dev/null
insmod soc_spi.ko 2>/dev/null
insmod kfifo_buf.ko
insmod industrialio-triggered-buffer.ko
insmod qmi8658a_core.ko
insmod qmi8658a_spi.ko
insmod qmi8658_spi_board.ko

dmesg | tail -n 100

echo "==== spi devices ===="
ls /sys/bus/spi/devices/

echo "==== iio devices ===="
ls /sys/bus/iio/devices/

echo "==== iio name ===="
cat /sys/bus/iio/devices/iio:device0/name 2>/dev/null
```

判断标准：

```text
1. dmesg 里有 WHO_AM_I = 0x05  最好
2. /sys/bus/spi/devices/ 有 spiX.Y
3. /sys/bus/iio/devices/ 有 iio:device0
4. cat name 能看到 qmi8658a
```

如果第 1 步就读不到 `0x05`，优先怀疑：

```text
CS 引脚不对
SPI bus 编号不对
SPI mode 不对，常见先试 mode 0
MISO/MOSI 接反
供电 / VDDIO 不对
qmi8658_spi_board.ko 里的 modalias 不匹配
```

QMI8658A 支持 I2C/I3C 以及 3-wire/4-wire SPI，SPI 速度数据手册给到最高 15 MHz；调试阶段建议先用 500 kHz 或 1 MHz，降低硬件时序问题。([files.waveshare.com][1])

[1]: https://files.waveshare.com/upload/5/5f/QMI8658A_Datasheet_Rev_A.pdf "QMI8658A Datasheet"
