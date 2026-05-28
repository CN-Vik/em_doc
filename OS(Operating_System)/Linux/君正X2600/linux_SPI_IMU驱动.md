你要在 **Linux X2600 上写 SPI IMU 驱动**，推荐按这个路线：

```text
先用 spidev 验证 SPI 硬件通信没问题
再写内核 SPI 驱动
正式产品建议做成 IIO 驱动
最后通过设备树挂载，编译进 kernel 或编成 .ko
```

Linux 里 SPI 设备驱动本质上是一个 `struct spi_driver`，它会和设备树/板级代码生成的 `struct spi_device` 匹配绑定；官方 SPI 文档也把这种驱动称为 SPI protocol driver。([Linux内核文档][1])
IMU 这种传感器在 Linux 里更标准的归属是 **IIO 子系统**，IIO 设备一般会在 `/sys/bus/iio/devices/iio:deviceX/` 下暴露传感器通道数据。([Linux内核文档][2])

---

# 1. SPI IMU 驱动应该写在哪里？

你这个工程路径是：

```bash
~/project/x2600/kernel/kernel-x2000
```

正式写法建议放在：

```bash
kernel/kernel-x2000/drivers/iio/imu/
```

比如你的 IMU 是 `ICM42688`，可以新建：

```text
kernel/kernel-x2000/drivers/iio/imu/icm42688/
├── icm42688_core.c
├── icm42688_spi.c
├── icm42688.h
├── Kconfig
└── Makefile
```

如果你只是新手阶段验证，也可以先写成普通 SPI 字符设备驱动，放到：

```text
kernel/kernel-x2000/drivers/misc/
```

比如：

```text
kernel/kernel-x2000/drivers/misc/my_spi_imu.c
```

但是正式产品更建议走：

```text
drivers/iio/imu/
```

因为 IMU 是加速度计、陀螺仪、温度、FIFO、中断触发数据采集，这些都和 Linux IIO 框架匹配。IIO buffer 支持通过触发源连续采集多个通道，并可从 `/dev/iio:deviceX` 读取数据。([Linux内核文档][3])

---

# 2. 推荐开发顺序

不要一上来就写完整驱动，建议分 5 步：

```text
第 1 步：确认硬件 SPI 控制器打开
第 2 步：设备树添加 IMU 节点
第 3 步：先用 spidev 或简单驱动读 WHO_AM_I
第 4 步：写正式 IIO 驱动
第 5 步：编译、烧录、运行测试
```

---

# 3. 先确认内核打开 SPI

你前面已经进过：

```bash
make menuconfig
```

位置是：

```text
Device Drivers  --->
    [*] SPI support
```

这个必须选：

```text
[*] SPI support
```

也就是：

```bash
CONFIG_SPI=y
```

退出后确认：

```bash
cd ~/project/x2600/kernel/kernel-x2000

grep CONFIG_SPI .config
```

应该看到：

```text
CONFIG_SPI=y
```

如果你用 GPIO 模拟 SPI，还要打开类似：

```text
CONFIG_SPI_GPIO
```

如果你用 X2600 硬件 SPI，则还要打开君正对应 SPI 控制器驱动。可以搜：

```bash
cd ~/project/x2600/kernel/kernel-x2000

grep -R "INGENIC\|JZ\|XBURST\|SPI" -n drivers/spi arch/mips/configs .config | head -100
```

---

# 4. 设备树写在哪里？

先找 X2600 的 dts：

```bash
cd ~/project/x2600/kernel/kernel-x2000

find arch/mips -name "*.dts" -o -name "*.dtsi" | grep -i x2600
```

你可能会看到类似：

```text
arch/mips/boot/dts/ingenic/x2600_xxx.dts
arch/mips/boot/dts/ingenic/x2600.dtsi
```

具体名字以你工程为准。

---

## 假设你的 IMU 挂在 SPI0 CS0

设备树大概长这样：

```dts
&spi0 {
    status = "okay";

    imu@0 {
        compatible = "invensense,icm42688";
        reg = <0>;
        spi-max-frequency = <8000000>;

        /*
         * 如果有中断脚，比如 IMU INT1 接到某个 GPIO，
         * 后面再补 interrupt-parent / interrupts。
         */
    };
};
```

含义：

```text
&spi0                 使用 SPI0 控制器
status = "okay"       打开 SPI0
imu@0                 SPI 从设备，CS0
reg = <0>             片选号 CS0
spi-max-frequency     最大 SPI 频率
compatible            用来匹配驱动
```

SPI 设备树里 `reg` 通常表示 SPI 片选号，`spi-max-frequency` 表示该从设备允许的最高 SPI 频率；SPI 设备是 SPI master/controller 的子节点。([内核源代码][4])

---

# 5. 先写一个最小 SPI 驱动读 WHO_AM_I

比如先写：

```bash
cd ~/project/x2600/kernel/kernel-x2000

mkdir -p drivers/iio/imu/icm42688
nano drivers/iio/imu/icm42688/icm42688_spi.c
```

最小测试版：

```c
#include <linux/module.h>
#include <linux/spi/spi.h>
#include <linux/of.h>

#define ICM42688_REG_WHO_AM_I  0x75

/*
 * 很多 SPI IMU 的读寄存器规则是：
 * bit7 = 1 表示读
 * bit7 = 0 表示写
 *
 * 例如读 0x75：
 * 发送 0x75 | 0x80
 */
static int icm42688_read_reg(struct spi_device *spi, u8 reg, u8 *val)
{
    int ret;
    u8 tx[2];
    u8 rx[2];

    tx[0] = reg | 0x80;
    tx[1] = 0x00;

    /*
     * spi_write_then_read 也能用，但很多 IMU 读寄存器需要同时收发。
     * 这里用 spi_sync_transfer 更直观。
     */
    {
        struct spi_transfer xfers[] = {
            {
                .tx_buf = tx,
                .rx_buf = rx,
                .len = 2,
            },
        };

        ret = spi_sync_transfer(spi, xfers, ARRAY_SIZE(xfers));
        if (ret)
            return ret;
    }

    /*
     * rx[0] 通常是发送地址阶段收到的无效数据
     * rx[1] 才是寄存器返回值
     */
    *val = rx[1];

    return 0;
}

static int icm42688_probe(struct spi_device *spi)
{
    int ret;
    u8 whoami = 0;

    dev_info(&spi->dev, "icm42688 spi probe enter\n");

    spi->mode = SPI_MODE_0;
    spi->bits_per_word = 8;
    spi->max_speed_hz = 8000000;

    ret = spi_setup(spi);
    if (ret) {
        dev_err(&spi->dev, "spi_setup failed: %d\n", ret);
        return ret;
    }

    ret = icm42688_read_reg(spi, ICM42688_REG_WHO_AM_I, &whoami);
    if (ret) {
        dev_err(&spi->dev, "read WHO_AM_I failed: %d\n", ret);
        return ret;
    }

    dev_info(&spi->dev, "WHO_AM_I = 0x%02x\n", whoami);

    return 0;
}

static int icm42688_remove(struct spi_device *spi)
{
    dev_info(&spi->dev, "icm42688 remove\n");
    return 0;
}

static const struct of_device_id icm42688_of_match[] = {
    { .compatible = "invensense,icm42688" },
    { }
};
MODULE_DEVICE_TABLE(of, icm42688_of_match);

static struct spi_driver icm42688_driver = {
    .driver = {
        .name = "icm42688",
        .of_match_table = icm42688_of_match,
    },
    .probe = icm42688_probe,
    .remove = icm42688_remove,
};

module_spi_driver(icm42688_driver);

MODULE_AUTHOR("Vik");
MODULE_DESCRIPTION("ICM42688 SPI IMU test driver");
MODULE_LICENSE("GPL");
```

这个版本先不做 IIO，只验证：

```text
SPI 驱动能不能 probe
SPI 时钟有没有出来
CS 有没有拉
MOSI/MISO 有没有数据
WHO_AM_I 能不能读到
```

---

# 6. 加 Kconfig

创建：

```bash
nano drivers/iio/imu/icm42688/Kconfig
```

内容：

```text
config ICM42688_SPI_TEST
    tristate "ICM42688 SPI test driver"
    depends on SPI
    help
      Simple SPI test driver for ICM42688 IMU.
```

---

# 7. 加 Makefile

创建：

```bash
nano drivers/iio/imu/icm42688/Makefile
```

内容：

```makefile
obj-$(CONFIG_ICM42688_SPI_TEST) += icm42688_spi.o
```

---

# 8. 把它挂到上级 Kconfig / Makefile

编辑：

```bash
nano drivers/iio/imu/Kconfig
```

末尾加：

```text
source "drivers/iio/imu/icm42688/Kconfig"
```

编辑：

```bash
nano drivers/iio/imu/Makefile
```

末尾加：

```makefile
obj-$(CONFIG_ICM42688_SPI_TEST) += icm42688/
```

---

# 9. menuconfig 里打开驱动

执行：

```bash
cd ~/project/x2600/kernel/kernel-x2000

make menuconfig
```

然后找：

```text
Device Drivers
  Industrial I/O support
    Inertial measurement units
      ICM42688 SPI test driver
```

选成模块：

```text
<M> ICM42688 SPI test driver
```

或者编进内核：

```text
<*> ICM42688 SPI test driver
```

新手建议先选：

```text
<M>
```

这样生成 `.ko`，调试方便，不用每次换内核。

保存退出后确认：

```bash
grep ICM42688 .config
```

应该看到：

```text
CONFIG_ICM42688_SPI_TEST=m
```

或者：

```text
CONFIG_ICM42688_SPI_TEST=y
```

---

# 10. 编译 kernel / modules

你这个 SDK 顶层应该从：

```bash
cd ~/project/x2600/build
```

编译。

一般顺序建议：

```bash
make kernel
make modules
```

如果没有 `make modules` 目标，就直接：

```bash
make kernel
```

也可以在内核目录单独编：

```bash
cd ~/project/x2600/kernel/kernel-x2000

make ARCH=mips CROSS_COMPILE=mips-linux-gnu- modules
```

但君正 SDK 里通常已经封装了工具链变量，所以更建议用：

```bash
cd ~/project/x2600/build
make kernel
```

---

# 11. 编译出来的 .ko 在哪里？

查一下：

```bash
cd ~/project/x2600

find kernel/kernel-x2000 -name "*icm42688*.ko"
```

如果你选的是 `M`，应该能找到类似：

```text
kernel/kernel-x2000/drivers/iio/imu/icm42688/icm42688_spi.ko
```

如果你选的是 `Y`，不会生成 `.ko`，驱动直接进内核镜像。

---

# 12. 烧录方式

这个要看你现在的板子支持什么烧录方式。一般有几种：

```text
1. 烧整个固件镜像
2. 只更新 kernel
3. 通过 adb / scp / tftp 拷贝 .ko 到板子上
4. 挂 NFS rootfs 调试
```

新手阶段最推荐：

```text
先不要反复烧整个系统。
先把 .ko 拷贝到板子上 insmod 测试。
```

比如你的板子网络通了，可以：

```bash
scp kernel/kernel-x2000/drivers/iio/imu/icm42688/icm42688_spi.ko root@板子IP:/tmp/
```

如果没有网络，可以用 U 盘、SD 卡、串口 rz/sz、adb，根据你板子支持情况选。

在板子上执行：

```bash
cd /tmp
insmod icm42688_spi.ko
dmesg | tail -50
```

如果设备树也对了，应该看到：

```text
icm42688 spi probe enter
WHO_AM_I = 0xXX
```

---

# 13. 运行时测试命令

## 看 SPI 设备有没有出来

板子上执行：

```bash
ls /sys/bus/spi/devices/
```

可能看到：

```text
spi0.0
```

其中：

```text
spi0.0 = SPI0 总线 CS0 设备
spi1.0 = SPI1 总线 CS0 设备
spi0.1 = SPI0 总线 CS1 设备
```

---

## 看驱动有没有绑定

```bash
ls /sys/bus/spi/drivers/
```

应该能看到：

```text
icm42688
```

然后：

```bash
ls -l /sys/bus/spi/drivers/icm42688/
```

如果绑定成功，里面可能有：

```text
spi0.0
```

---

## 看内核日志

```bash
dmesg | grep -i icm
dmesg | grep -i spi
```

重点看：

```text
probe enter
WHO_AM_I
read failed
chip select
timeout
```

---

# 14. 如果你做正式 IIO 驱动，用户空间怎么读？

正式 IIO 驱动注册成功后，会出现：

```bash
ls /sys/bus/iio/devices/
```

例如：

```text
iio:device0
```

读原始数据：

```bash
cd /sys/bus/iio/devices/iio:device0

cat name
cat in_accel_x_raw
cat in_accel_y_raw
cat in_accel_z_raw
cat in_anglvel_x_raw
cat in_anglvel_y_raw
cat in_anglvel_z_raw
cat in_temp_raw
```

很多 IIO IMU 驱动会暴露加速度、角速度、温度、采样频率、滤波等 sysfs 节点；Linux IIO 文档中的 IMU 驱动示例也展示了 `/sys/bus/iio/devices/iio:deviceX` 下读取加速度、角速度、温度、采样频率等节点的使用方式。([Linux内核文档][5])

---

# 15. SPI IMU 驱动核心代码结构

一个正式驱动大概分这些函数：

```text
probe
remove
read_reg
write_reg
read_raw
set_scale
set_sample_rate
irq_handler
buffer_push
suspend/resume
```

最小结构：

```text
icm42688_spi.c
    负责 SPI 总线读写、spi_driver 注册

icm42688_core.c
    负责芯片初始化、寄存器配置、IIO 通道、数据读取

icm42688.h
    寄存器地址、结构体、函数声明
```

建议不要把所有东西都塞进一个文件，后面会很乱。

---

# 16. 你现在最应该先做什么？

按这个顺序来：

## 第一步：确认 SPI 控制器打开

```bash
cd ~/project/x2600/kernel/kernel-x2000
grep CONFIG_SPI .config
```

至少要有：

```text
CONFIG_SPI=y
```

---

## 第二步：找设备树

```bash
find arch/mips -name "*.dts" -o -name "*.dtsi" | grep -i x2600
```

确认你板子的 dts 是哪个。

---

## 第三步：设备树加 IMU 节点

比如：

```dts
&spi0 {
    status = "okay";

    imu@0 {
        compatible = "invensense,icm42688";
        reg = <0>;
        spi-max-frequency = <8000000>;
    };
};
```

---

## 第四步：写最小 SPI 驱动读 WHO_AM_I

先放这里：

```text
kernel/kernel-x2000/drivers/iio/imu/icm42688/icm42688_spi.c
```

先不要搞 FIFO、中断、IIO buffer。

---

## 第五步：编译成 ko，拷到板子 insmod

```bash
find kernel/kernel-x2000 -name "*icm42688*.ko"
scp xxx.ko root@板子IP:/tmp/
```

板子上：

```bash
insmod /tmp/icm42688_spi.ko
dmesg | tail -50
```

---

# 17. 常见坑

## 坑 1：设备树 compatible 不匹配

驱动里是：

```c
.compatible = "invensense,icm42688"
```

设备树也必须是：

```dts
compatible = "invensense,icm42688";
```

一个字符不一样都不会 probe。

---

## 坑 2：SPI mode 错

IMU 可能要求：

```text
SPI_MODE_0
SPI_MODE_3
```

如果 mode 错，读 WHO_AM_I 可能一直是：

```text
0x00
0xff
随机值
```

---

## 坑 3：片选 CS 错

设备树：

```dts
reg = <0>;
```

表示 CS0。
如果硬件接的是 CS1，就要：

```dts
reg = <1>;
```

---

## 坑 4：MISO/MOSI 接反

表现：

```text
示波器看 SCLK 有，MOSI 有，但是 MISO 永远没返回
```

---

## 坑 5：电源/复位没开

有些 IMU 需要：

```text
VDD
VDDIO
RESET
INT
```

如果 VDDIO 没电，SPI 读不到。

---

## 坑 6：内核版本太老

你这个日志里是 Linux 4.4.94。这个内核比较老，可能没有现成的 ICM42688 驱动。所以：

```text
短期：自己写最小 SPI 驱动读寄存器
长期：移植新内核里的 IIO IMU 驱动，或者自己写 IIO 驱动
```

---

# 18. 一句话总结

你在 X2600 上写 SPI IMU 驱动，正式位置建议放：

```text
kernel/kernel-x2000/drivers/iio/imu/你的芯片名/
```

开发流程是：

```text
设备树添加 SPI IMU 节点
menuconfig 打开 SPI/IIO/你的驱动
写 spi_driver 的 probe，先读 WHO_AM_I
编译成 .ko
拷到板子 insmod
用 dmesg 看是否 probe 成功
正式阶段再补 IIO raw 数据、scale、采样率、中断和 buffer
```

先把 **WHO_AM_I 能读出来**，这一步成功了，后面才谈 FIFO、中断、IIO。

[1]: https://docs.kernel.org/driver-api/spi.html?utm_source=chatgpt.com "Serial Peripheral Interface (SPI)"
[2]: https://docs.kernel.org/driver-api/iio/core.html?utm_source=chatgpt.com "Core elements"
[3]: https://docs.kernel.org/driver-api/iio/buffers.html?utm_source=chatgpt.com "Buffers"
[4]: https://www.kernel.org/doc/Documentation/devicetree/bindings/spi/spi-controller.yaml?utm_source=chatgpt.com "spi-controller.yaml"
[5]: https://docs.kernel.org/iio/adis16550.html?utm_source=chatgpt.com "ADIS16550 driver"
