记得，你之前这套大概是：

```text
X2600 SPI0:
  CLK  = PD00
  MOSI = PD01
  MISO = PD02
  CS   = PD05

BMI160:
  bmi160_core.ko
  bmi160_spi.ko
  bmi160_spi_board.ko

加载顺序大概是：
  utils.ko
  soc_spi.ko
  kfifo_buf.ko
  industrialio-triggered-buffer.ko
  bmi160_core.ko
  bmi160_spi.ko
  bmi160_spi_board.ko
```

你现在想从“手动 insmod 模块”改成“编进内核”，核心思路是：**SPI 控制器、IIO、BMI160 SPI 驱动、BMI160 板级设备注册，都要进入内核，不能只把 bmi160_spi 改成 y。**

---

## 先说结论

你这个场景不要走 `spidev/cmd_spi` 作为最终方案。
君正这个文档主要讲的是 **SPI 控制器配置、spidev 调试、动态注册 SPI 设备**，适合前期验证 SPI 通不通。文档里也说明了 IConfigTool 里要根据设备选择 SPI 控制器驱动或 GPIO_SPI 驱动，并进入 SPI 总线配置界面配置相关引脚；它还给了 `cmd_spi add_dev 0 pc16` 这种应用层调试方式。  

但是 BMI160 是 **内核 IIO 设备驱动**，最终应该是：

```text
SPI master 驱动起来
        ↓
注册 spi_device，modalias = "bmi160"
        ↓
bmi160_spi 驱动 probe
        ↓
生成 /sys/bus/iio/devices/iio:deviceX
```

---

# 1. 内核配置要从 m 改成 y

进入内核目录：

```sh
cd /home/vik/project/x2600/kernel/kernel
make menuconfig
```

建议目标配置：

```text
CONFIG_SPI=y
CONFIG_REGMAP_SPI=y

CONFIG_IIO=y
CONFIG_IIO_BUFFER=y
CONFIG_IIO_TRIGGERED_BUFFER=y

CONFIG_BMI160=y
CONFIG_BMI160_SPI=y
CONFIG_BMI160_I2C=n
```

也就是原来：

```text
CONFIG_BMI160=m
CONFIG_BMI160_SPI=m
```

改成：

```text
CONFIG_BMI160=y
CONFIG_BMI160_SPI=y
```

你可以直接查：

```sh
grep -E "CONFIG_SPI=|CONFIG_REGMAP_SPI=|CONFIG_IIO=|CONFIG_BMI160" .config
```

应该看到类似：

```text
CONFIG_SPI=y
CONFIG_REGMAP_SPI=y
CONFIG_IIO=y
CONFIG_BMI160=y
CONFIG_BMI160_SPI=y
```

---

# 2. SPI 控制器也要内建，不能还是 soc_spi.ko

这是最容易踩坑的地方。

你之前 SPI 总线是靠：

```text
utils.ko
soc_spi.ko
```

起来的。如果现在 BMI160 已经编进内核，但是 SPI 控制器还是模块，那启动阶段会出现这个问题：

```text
BMI160 板级注册代码执行了
        ↓
但是 SPI0 master 还没注册
        ↓
spi_busnum_to_master(0) 失败
        ↓
bmi160 设备注册失败
```

所以要么：

## 方案 A：SPI 控制器也编进内核

在 menuconfig 里找：

```text
Device Drivers
  SPI support
    <*> SPI support
    <*> Ingenic SPI Controller
```

不同君正 SDK 里名字可能不完全一样，你可以这样搜：

```sh
grep -R "Ingenic SPI" -n drivers/spi
grep -R "config .*SPI.*INGENIC\|config .*INGENIC.*SPI" -n drivers/spi
```

然后把对应的 SPI controller 改成 `y`。

---

## 方案 B：SPI master 仍然来自君正 module_driver，但 BMI160 不建议强行内建

如果 `soc_spi.ko` 是君正 `module_driver` 体系里的 SPI master，而且你暂时没法把它改成内核内建，那 BMI160 最好也继续保持模块方式。

原因很简单：**总线都没起来，内核内建的 BMI160 也 probe 不到。**

---

# 3. CS=PD05 不是写在 bmi160_spi.c 里面

你之前问过这个点：BMI160 的 CS 是 `PD05`。
这个不要写进：

```text
drivers/iio/imu/bmi160/bmi160_spi.c
```

而是写在 **板级 SPI 设备注册** 里面。

君正文档的示例也是这个结构：

```c
static struct spi_board_info m_spi_device = {
    .modalias = "m_new_spi_device",
    .max_speed_hz = 10 * 1000 * 1000,
    .mode = SPI_MODE_3,
    .controller_data = (void *)(GPIO_PC(16)),
    .chip_select = 0,
};
```

它通过 `.controller_data` 设置 GPIO 片选，通过 `.chip_select` 设置逻辑片选号；然后再注册 SPI 设备。

你的 BMI160 应该改成：

```c
.controller_data = (void *)GPIO_PD(5),
.chip_select = 0,
.modalias = "bmi160",
```

---

# 4. 建议加一个内建版 bmi160_spi_board.c

为了学习阶段简单，你可以先把板级注册文件放这里：

```text
/home/vik/project/x2600/kernel/kernel/drivers/iio/imu/bmi160/bmi160_spi_board.c
```

内容参考这个：

```c
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

static struct spi_board_info bmi160_spi_board_info __initdata = {
    /*
     * 这个名字必须和 bmi160_spi.c 里的 spi_device_id 匹配。
     * 大多数内核里 BMI160 SPI 的 id 是 "bmi160"。
     */
    .modalias = "bmi160",

    /*
     * 调试阶段先低一点，稳定后再提高。
     * BMI160 SPI 可以先用 1MHz 或 5MHz。
     */
    .max_speed_hz = 1 * 1000 * 1000,

    /*
     * BMI160 常见先用 SPI_MODE_0。
     * 如果 WHO_AM_I 读不出来，再试 SPI_MODE_3。
     */
    .mode = SPI_MODE_0,

    /*
     * 君正 SPI 驱动里一般用 controller_data 传 GPIO CS。
     * 你的 CS 是 PD05。
     */
    .controller_data = (void *)GPIO_PD(5),

    /*
     * 逻辑片选号，同一条 SPI 总线上要唯一。
     */
    .chip_select = 0,

    /*
     * SPI 总线号，你之前用的是 SPI0。
     */
    .bus_num = 0,
};

static int __init x2600_bmi160_spi_board_init(void)
{
    pr_info("x2600-bmi160: register BMI160 on SPI0 CS=PD05\n");

    /*
     * 用 spi_register_board_info 比直接 spi_busnum_to_master 更适合内建。
     * 因为它允许先登记 board_info，等 SPI master 注册后自动创建设备。
     */
    return spi_register_board_info(&bmi160_spi_board_info, 1);
}
arch_initcall(x2600_bmi160_spi_board_init);
```

为什么这里不用文档里的 `spi_register_device()` 封装？

因为文档里的模块示例是 `insmod m_spi_dev.ko` 之后才注册设备，那时候 SPI master 通常已经存在；它的流程是先 `spi_register_driver()`，再 `spi_register_device()`。

但你现在是编进内核，初始化顺序更敏感。用 `spi_register_board_info()` 更稳一点。

---

# 5. 修改 BMI160 Makefile

打开：

```sh
vim /home/vik/project/x2600/kernel/kernel/drivers/iio/imu/bmi160/Makefile
```

原来大概类似：

```makefile
obj-$(CONFIG_BMI160) += bmi160_core.o
obj-$(CONFIG_BMI160_SPI) += bmi160_spi.o
obj-$(CONFIG_BMI160_I2C) += bmi160_i2c.o
```

加一行：

```makefile
obj-$(CONFIG_BMI160_SPI) += bmi160_spi_board.o
```

最终类似：

```makefile
obj-$(CONFIG_BMI160) += bmi160_core.o
obj-$(CONFIG_BMI160_SPI) += bmi160_spi.o
obj-$(CONFIG_BMI160_SPI) += bmi160_spi_board.o
obj-$(CONFIG_BMI160_I2C) += bmi160_i2c.o
```

这样当：

```text
CONFIG_BMI160_SPI=y
```

时，`bmi160_spi_board.o` 也会跟着编进内核。

---

# 6. 确认 modalias 到底是不是 bmi160

你最好查一下你的内核源码：

```sh
grep -R "spi_device_id" -n drivers/iio/imu/bmi160
grep -R "\"bmi160" -n drivers/iio/imu/bmi160
```

如果看到类似：

```c
static const struct spi_device_id bmi160_spi_id[] = {
    { "bmi160", 0 },
    { }
};
```

那你的 board_info 就写：

```c
.modalias = "bmi160",
```

如果你的源码里写的是别的名字，就以源码为准。

---

# 7. spidev 这里怎么处理？

文档里说 kernel menuconfig 里取消内核 spidev，使用模块 spidev 驱动。

对于你现在这个 BMI160 内建方案，我建议：

```text
CONFIG_SPI_SPIDEV=n
```

原因是：

```text
BMI160 用的是内核 IIO 驱动
不是 /dev/spidev0.0 应用层裸读写
```

如果你还想用 `cmd_spi` 测 SPI 波形，可以临时用 spidev 调试，但注意：**不要让 spidev 和 BMI160 同时占用 SPI0 CS0/PD05**。同一个 SPI 设备不能一边被 spidev 占着，一边被 bmi160 驱动占着。

---

# 8. 重新编译

回到工程目录：

```sh
cd /home/vik/project/x2600
make kernel
```

如果需要重新打包系统镜像，就按你现在 SDK 的流程继续：

```sh
make
```

或者你之前单独烧 kernel 的话，就只更新 kernel 镜像。

---

# 9. 启动后验证

启动后看 dmesg：

```sh
dmesg | grep -iE "spi|bmi160|iio"
```

看 SPI 设备有没有出来：

```sh
ls /sys/bus/spi/devices/
```

期望看到类似：

```text
spi0.0
```

看 IIO 设备：

```sh
ls /sys/bus/iio/devices/
```

看名字：

```sh
cat /sys/bus/iio/devices/iio:device*/name
```

期望类似：

```text
bmi160
```

如果有 IIO 节点，再看原始数据：

```sh
find /sys/bus/iio/devices/iio:device* -maxdepth 1 -type f | grep raw
```

可能会有：

```text
in_accel_x_raw
in_accel_y_raw
in_accel_z_raw
in_anglvel_x_raw
in_anglvel_y_raw
in_anglvel_z_raw
```

读取：

```sh
cat /sys/bus/iio/devices/iio:device0/in_accel_x_raw
cat /sys/bus/iio/devices/iio:device0/in_accel_y_raw
cat /sys/bus/iio/devices/iio:device0/in_accel_z_raw
```

---

# 10. 如果失败，按这个倒推

## 情况 A：没有 `spi0.0`

查：

```sh
dmesg | grep -i spi
```

如果看到：

```text
failed to get spi master 0
```

说明 SPI0 master 没起来，或者 board 注册时机不对。

重点检查：

```sh
grep -E "CONFIG_SPI=|CONFIG_.*INGENIC.*SPI|CONFIG_.*SPI.*INGENIC" .config
```

---

## 情况 B：有 `spi0.0`，但是没有 IIO

查：

```sh
dmesg | grep -i bmi
```

可能是 `.modalias` 不匹配。

确认：

```sh
cat /sys/bus/spi/devices/spi0.0/modalias
```

应该类似：

```text
bmi160
```

如果不是，就改 board_info 的：

```c
.modalias = "bmi160",
```

---

## 情况 C：probe 失败，读不到芯片 ID

重点查这几个：

```text
1. CS 是否真的是 PD05
2. SPI mode 是否应该 SPI_MODE_0 或 SPI_MODE_3
3. SCLK/MOSI/MISO 是否和 IConfigTool 里一致
4. 供电是否正常
5. BMI160 SDO/SA0 引脚是否影响 SPI/I2C 模式
6. 逻辑分析仪看 CS/SCLK/MOSI/MISO 有没有波形
```

---

# 你这次最推荐的改法

我建议你走这个顺序：

```text
第一步：保持 IConfigTool 里 SPI0 引脚配置不变
        CLK=PD00 MOSI=PD01 MISO=PD02

第二步：kernel 里把 CONFIG_SPI / CONFIG_IIO / CONFIG_BMI160_SPI 全改成 y

第三步：加 bmi160_spi_board.c
        bus_num = 0
        controller_data = GPIO_PD(5)
        modalias = "bmi160"

第四步：编译烧录 kernel

第五步：启动后看：
        dmesg | grep -iE "spi|bmi160|iio"
        ls /sys/bus/spi/devices/
        ls /sys/bus/iio/devices/
```

一句话总结：**你要内建的不是单独一个 BMI160 驱动，而是“SPI master + BMI160 SPI driver + BMI160 board device”这一整条链路。**


-------
-------
------
你这个现象很正常，**不是你眼花，大概率是配置链路没走到真正生效的那个地方**。

你说的应该不是 `ifconfig`，而是 **IConfigTool**。君正文档里这个 SPI IO 配置是在 **[模块化驱动] → [驱动] → [SPI 总线]** 里面配的，而且文档也区分了 **SPI 控制器驱动** 和 **GPIO_SPI 驱动** 两套配置。也就是说：你在 IConfigTool 里改了，不代表 Linux kernel 里的 SPI pinctrl / DTS / 模块 ko 一定同步改了。

---

# 最可能的原因

## 1. 你只重新编译了 kernel，但 SPI 驱动实际还是 module_driver 里的 ko

你之前 SPI 是模块方式加载的，类似：

```sh
insmod utils.ko
insmod soc_spi.ko
```

这种情况下，SPI IO 很可能是 **module_driver 工程生成/编译出来的 soc_spi.ko** 里带的，不是单纯由 `kernel/kernel/.config` 决定的。

所以你只执行：

```sh
make kernel
```

然后烧录 kernel，可能没有任何用。

正确应该是：

```text
IConfigTool 改 SPI IO
        ↓
保存到 build/configs/xxx_defconfig 或 build/.config.in
        ↓
重新生成 module_driver 配置
        ↓
重新编译 module_driver
        ↓
重新打包 rootfs / system
        ↓
烧录包含新 soc_spi.ko 的系统
```

否则板子上加载的还是旧的：

```text
/lib/modules/.../soc_spi.ko
```

IO 当然还是旧的。

---

## 2. 你改的是 IConfigTool 的模块化驱动配置，但内核里实际用的是 DTS / pinctrl

如果你想把 SPI 和 BMI160 **编进内核**，那情况又变了。

内建进 kernel 后，SPI 控制器的 IO 可能来自：

```text
kernel/kernel/arch/mips/boot/dts/...
kernel/kernel/arch/mips/xburst2/...
kernel/kernel/arch/mips/ingenic/...
drivers/spi/...
```

而不是 IConfigTool 里那个 module_driver 配置。

也就是说：

```text
SPI 做成 ko 模块：
    可能吃 IConfigTool / module_driver 配置

SPI 编进内核：
    更可能吃 kernel DTS / board 文件 / pinctrl 配置
```

所以你如果只是改 IConfigTool，但实际启动的是内核内建 SPI controller，那 IO 不变也正常。

---

## 3. 你可能改错了板级 defconfig

文档第一页就强调：打开 IConfigTool 时要选择对应板级配置。它例子是 x2000，但你现在是 X2600 / X2600N。

你要确认你改的是实际编译使用的这个文件，比如类似：

```sh
/home/vik/project/x2600/build/configs/x2600n_tiger_nor_5.10_defconfig
```

而不是改到了：

```sh
x2000_evb_xxx_defconfig
x2600_evb_xxx_defconfig
其他测试 defconfig
```

这个很常见：IConfigTool 里看着改了，实际 `make` 用的是另一个 defconfig。

---

## 4. `cmd_spi add_dev` 只设置片选 CS，不设置 CLK/MOSI/MISO

文档里的：

```sh
cmd_spi add_dev 0 pc16
```

这里的 `pc16` 是 **片选 CS 引脚**，不是 SPI 总线的 CLK/MOSI/MISO。文档后面也说明 `cmd_spi set` 只能设置 mode、speed、bits、lsb 这些传输参数，不是改 SPI 总线复用引脚。

所以如果你是通过 `cmd_spi` 看或测 SPI，不要误会：

```text
cmd_spi add_dev 0 pd05
```

只能说明：

```text
SPI0 使用 PD05 作为 CS
```

不代表：

```text
SCLK/MOSI/MISO 也被改成你想要的 IO
```

---

# 你现在应该这样倒推

## 第一步：确认你改的配置文件到底有没有变

在 Ubuntu 编译环境里执行：

```sh
cd /home/vik/project/x2600

grep -R "SPI0\|SPI1\|MD_X2600.*SPI\|MD_SPI\|PB29\|PB30\|PB31\|PD00\|PD01\|PD02\|PD05" \
    build/configs build/.config.in 2>/dev/null
```

重点看你改的 IO 有没有真的写进去。

如果这里查不到你配置的 IO，说明 IConfigTool 没保存到当前工程使用的配置里。

---

## 第二步：确认 kernel 里有没有同步

```sh
cd /home/vik/project/x2600/kernel/kernel

grep -E "CONFIG_SPI|CONFIG_SPI_INGENIC|CONFIG_REGMAP_SPI|CONFIG_BMI160" .config
```

然后查 DTS / board 文件里有没有 SPI IO：

```sh
grep -R "spi0\|spi1\|ssi0\|ssi1\|PD05\|PD0\|PB29\|PB30\|PB31" \
    arch/mips drivers/spi 2>/dev/null | head -100
```

如果你发现 SPI IO 在 DTS 或 board 文件里写死了，那你改 IConfigTool 就没用，需要改 DTS / board 文件。

---

## 第三步：确认板子上加载的是不是旧 ko

板子串口里执行：

```sh
lsmod
```

看有没有：

```text
soc_spi
utils
spidev
```

然后找 ko 文件：

```sh
find /lib/modules -name "*spi*.ko"
find / -name "soc_spi.ko" 2>/dev/null
```

看时间：

```sh
ls -l /lib/modules/*/*spi*.ko 2>/dev/null
ls -l /lib/modules/*/extra/*spi*.ko 2>/dev/null
```

如果时间还是旧的，说明你烧录进去的 rootfs 里还是旧模块。

---

## 第四步：在板子上看真实 pinmux

如果内核开了 debugfs：

```sh
mount -t debugfs none /sys/kernel/debug 2>/dev/null
```

然后查：

```sh
cat /sys/kernel/debug/gpio
```

再试：

```sh
find /sys/kernel/debug/pinctrl -type f | xargs grep -i "spi\|ssi\|PD0\|PB29\|PB30\|PB31" 2>/dev/null
```

这个比看配置更准。
因为它看的是 **运行时真实 pinmux 状态**。

---

# 我建议你现在先判断 SPI 到底是哪条链路

你可以按这个表判断：

| 现象                                 | 说明                                |
| ---------------------------------- | --------------------------------- |
| `lsmod` 里有 `soc_spi`               | SPI 控制器还是模块方式                     |
| `/lib/modules/.../soc_spi.ko` 时间没变 | 你烧进去的是旧 SPI 模块                    |
| `.config` 里 `CONFIG_SPI_XXX=y`     | SPI 控制器可能已经内建                     |
| DTS / board 文件里有 spi0 pinctrl      | IConfigTool 改 module_driver 可能不生效 |
| `cmd_spi add_dev 0 pd05` 生效        | 只是 CS 生效，不代表总线 IO 改了              |

---

# 对你这个 BMI160 最稳的路线

如果你想把 BMI160 彻底编进内核，我建议别再混着走：

```text
IConfigTool module_driver SPI
+
kernel 内建 BMI160
```

这样最容易乱。

建议统一成：

```text
SPI controller 内建进 kernel
IIO 内建进 kernel
BMI160 core 内建进 kernel
BMI160 spi 内建进 kernel
BMI160 board_info / DTS 里注册设备
```

然后 SPI IO 也在 kernel 这边固定下来。

也就是你要找并确认这几个东西：

```text
1. SPI0 master 是谁注册的？
2. SPI0 CLK/MOSI/MISO 是在哪里配置的？
3. BMI160 CS=PD05 是在哪里配置的？
4. BMI160 设备是 DTS 注册，还是 spi_board_info 注册？
```

---

# 你现在可以先执行这组命令，把结果发我

编译主机上：

```sh
cd /home/vik/project/x2600

echo "===== build configs ====="
grep -R "MD_X2600.*SPI\|MD_SPI\|SPI0\|SPI1\|PD05\|PB29\|PB30\|PB31\|PD00\|PD01\|PD02" \
    build/configs build/.config.in 2>/dev/null | head -200

echo "===== kernel config ====="
grep -E "CONFIG_SPI|CONFIG_REGMAP_SPI|CONFIG_IIO|CONFIG_BMI160" \
    kernel/kernel/.config

echo "===== kernel spi pin source ====="
grep -R "spi0\|spi1\|ssi0\|ssi1\|PD05\|PB29\|PB30\|PB31\|PD00\|PD01\|PD02" \
    kernel/kernel/arch/mips kernel/kernel/drivers/spi 2>/dev/null | head -200
```

板子上：

```sh
echo "===== kernel version ====="
uname -a

echo "===== loaded modules ====="
lsmod

echo "===== spi dmesg ====="
dmesg | grep -iE "spi|ssi|pinctrl|gpio|bmi"

echo "===== spi devices ====="
ls -l /sys/bus/spi/devices/ 2>/dev/null

echo "===== gpio debug ====="
mount -t debugfs none /sys/kernel/debug 2>/dev/null
cat /sys/kernel/debug/gpio 2>/dev/null
```

你把这几段输出贴出来，我基本就能判断：**到底是你改错配置、没重新编译 module_driver、没烧进去新 rootfs，还是 kernel 内建 SPI 根本不吃 IConfigTool 的 IO 配置。**


--------
------
--------
`module_driver 配置` 不是 Linux 内核里的 `make menuconfig`，它是君正 SDK 自己那套 **模块化驱动配置系统**。你之前 SPI 是 `soc_spi.ko` 这种模块方式，那 SPI IO 很可能就是从这里生成/编译进去的。

你可以按这个理解：

```text
IConfigTool 改配置
        ↓
保存到 build/configs/xxx_defconfig
        ↓
生成 build/.config.in
        ↓
编译 module_driver
        ↓
生成新的 soc_spi.ko / gpio_spi.ko / spi_device_helper.ko
        ↓
重新打包 rootfs/system
        ↓
烧录到板子
```

所以重点是：**只重新编译 kernel 不够，必须重新编译 module_driver，并且把新的 ko 烧到板子 rootfs 里。**

---

## 1. 先确认你打开的是正确板级配置

在你的工程目录下：

```sh
cd /home/vik/project/x2600
```

找一下实际用的 defconfig：

```sh
ls build/configs/
```

你之前大概率用的是类似：

```text
build/configs/x2600n_tiger_nor_5.10_defconfig
```

打开 IConfigTool 时，应该选：

```text
Config.in:
    /home/vik/project/x2600/build/Config.in

Config:
    /home/vik/project/x2600/build/configs/x2600n_tiger_nor_5.10_defconfig
```

不要选到文档里的 x2000 示例配置。文档第一页也是让先选择对应板级配置，再进模块化驱动界面配置 SPI；它示例是 x2000，不是你现在的 X2600。

---

## 2. IConfigTool 里 SPI 控制器配置怎么选

进入路径大概是：

```text
[root]
  → [模块化驱动]
    → [x2600驱动列表]
      → [SPI 总线]
```

然后配置：

```text
选择 SPI0：勾选

MISO 输入引脚：PD02
MOSI 输出引脚：PD01
CLK  时钟引脚：PD00
```

你之前 BMI160 是：

```text
SPI0:
  CLK  = PD00
  MOSI = PD01
  MISO = PD02
  CS   = PD05
```

这里注意：**SPI 总线这里只配 CLK/MOSI/MISO，不一定配 CS。**

CS 一般是在 SPI 设备注册的时候配，比如：

```c
.controller_data = (void *)GPIO_PD(5),
.chip_select = 0,
```

或者用 `cmd_spi add_dev 0 pd05` 临时添加 spidev 设备。

文档里 SPI 控制器驱动配置界面也是让你选 SPI0/SPI1，并配置 MISO、MOSI、CLK 这些 SPI 引脚。

---

## 3. IConfigTool 里 SPI 设备辅助模块也要勾

回到：

```text
[root]
  → [模块化驱动]
    → [驱动]
```

这里一般要勾：

```text
spi设备辅助模块（动态增加或删除spi设备）
spi设备驱动模块（spi设备驱动端）
```

文档里也圈出来了这两个选项，尤其是你要用 `cmd_spi add_dev` 这种辅助命令时，`spi设备辅助模块` 必须勾。

如果你最终不用 spidev/cmd_spi，只让 BMI160 走内核 IIO 驱动，那辅助模块不是最终必须项；但调试阶段建议先勾上，方便验证 SPI 总线。

---

## 4. 如果你用的是 GPIO_SPI，不要和 SPI 控制器混了

IConfigTool 里有两套东西：

```text
SPI 控制器驱动：
    使用 X2600 硬件 SPI 控制器

GPIO_SPI 驱动：
    用普通 GPIO 模拟 SPI
```

你 BMI160 正常应该优先走 **SPI 控制器驱动**，也就是硬件 SPI0。

只有当你怀疑硬件 SPI 控制器驱动有问题时，才用 GPIO_SPI 临时验证。文档里也说了：当 SPI 控制器不能正常使用时，可以用相同 IO 配置 GPIO_SPI 来判断是硬件异常还是 SPI 控制器驱动异常。

---

## 5. 保存后先检查配置有没有真的写进去

IConfigTool 点 Save 之后，在编译主机上查：

```sh
cd /home/vik/project/x2600

grep -R "SPI0\|SPI1\|MD_X2600.*SPI\|MD_SPI\|PD00\|PD01\|PD02\|PD05\|PB29\|PB30\|PB31" \
    build/configs build/.config.in 2>/dev/null
```

重点看这几个文件：

```text
build/configs/x2600n_tiger_nor_5.10_defconfig
build/.config.in
```

如果 `build/configs/x2600n_tiger_nor_5.10_defconfig` 里有你改的 PD00/PD01/PD02，说明 IConfigTool 保存进去了。

如果只有 IConfigTool 界面里改了，但 grep 查不到，说明你没保存成功，或者打开的不是实际使用的配置文件。

---

## 6. 重新编译 module_driver

你先看你的工程支持什么 make 目标：

```sh
cd /home/vik/project/x2600

make help | grep -i module
```

如果有 `module_driver` 目标，一般就是：

```sh
make module_driver
```

如果有 clean 目标，建议先清一下：

```sh
make module_driver-clean
make module_driver
```

如果没有 `module_driver-clean`，就先别乱删，直接：

```sh
make module_driver
```

编译完后找新的 ko：

```sh
find . -name "soc_spi.ko" -o -name "*spi*.ko" | sort
```

看时间：

```sh
find . -name "soc_spi.ko" -exec ls -l {} \;
```

你要确认生成的 `soc_spi.ko` 时间是新的。

---

## 7. 重新打包 rootfs/system，不是只烧 kernel

这是你之前最可能踩坑的地方。

如果 SPI 是模块：

```text
soc_spi.ko
gpio_spi.ko
spi_device_helper.ko
```

这些文件在 rootfs 里，不在 kernel image 里。

所以你只做：

```sh
make kernel
```

再烧 kernel，**不会更新 soc_spi.ko**。

你需要执行你 SDK 对应的 rootfs/system 打包流程，可能是：

```sh
make
```

或者：

```sh
make rootfs
make image
```

具体看你工程 Makefile 支持什么：

```sh
make help
```

烧录后，在板子上确认 ko 时间：

```sh
find / -name "soc_spi.ko" 2>/dev/null
ls -l /lib/modules/
```

如果板子上的 `soc_spi.ko` 时间还是旧的，那就说明你烧进去的还是旧 rootfs。

---

## 8. 板子上确认模块是不是新的

板子启动后执行：

```sh
lsmod
```

看有没有：

```text
soc_spi
spi_device_helper
spidev
```

然后查 SPI 日志：

```sh
dmesg | grep -iE "spi|ssi|gpio"
```

如果能找到模块文件，查时间：

```sh
find / -name "*spi*.ko" 2>/dev/null
```

例如：

```sh
ls -l /lib/modules/*/*spi*.ko 2>/dev/null
ls -l /lib/modules/*/extra/*spi*.ko 2>/dev/null
```

---

## 9. 用 md5 确认主机和板子是不是同一个 ko

编译主机上：

```sh
md5sum $(find . -name "soc_spi.ko" | head -n 1)
```

板子上：

```sh
md5sum $(find / -name "soc_spi.ko" 2>/dev/null | head -n 1)
```

两个 md5 一样，才说明你烧进去的是新模块。

---

## 10. 临时验证 SPI0 + PD05 CS

如果你保留了 `cmd_spi`，可以在板子上试：

```sh
cmd_spi add_dev 0 pd05
```

正常会出来类似：

```text
/dev/spidev0.0
```

然后设置：

```sh
cmd_spi set /dev/spidev0.0 mode=0x00 speed=1000000 bits=8 lsb=0
cmd_spi info /dev/spidev0.0
```

注意：这个 `pd05` 只是 CS。
真正的 SCLK/MOSI/MISO 还是来自 module_driver 里 SPI0 的 IO 配置。

---

# 你现在最推荐的操作顺序

```sh
cd /home/vik/project/x2600

# 1. 用 IConfigTool 打开：
#    build/Config.in
#    build/configs/x2600n_tiger_nor_5.10_defconfig
#
# 2. 配：
#    模块化驱动 -> x2600驱动列表 -> SPI总线 -> SPI0
#    CLK=PD00 MOSI=PD01 MISO=PD02
#
# 3. 配：
#    模块化驱动 -> 驱动
#    勾 spi设备辅助模块
#    勾 spi设备驱动模块

# 4. 保存后检查
grep -R "SPI0\|SPI1\|MD_X2600.*SPI\|MD_SPI\|PD00\|PD01\|PD02\|PD05" \
    build/configs build/.config.in 2>/dev/null

# 5. 编译 module_driver
make help | grep -i module
make module_driver

# 6. 查新 ko
find . -name "soc_spi.ko" -exec ls -l {} \;

# 7. 重新打包系统/rootfs
make

# 8. 重新烧录完整系统，或者至少烧录包含 /lib/modules 的 rootfs/system 分区
```

一句话：**module_driver 配置就是用 IConfigTool 改 `build/configs/xxx_defconfig`，然后重新编译 module_driver 和 rootfs；只 make kernel 不会让 soc_spi.ko 的 IO 配置生效。**
