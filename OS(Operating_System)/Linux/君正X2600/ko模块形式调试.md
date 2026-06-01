对，**现在先用 KO 模块调试最合适**。

先纠正一句：
你说的“先不加载进内核”，准确说应该是：

```text
不编进内核镜像里面，而是编译成 .ko，系统运行后手动 insmod 加载到内核。
```

KO 调试流程就是：

```text
PC 编译出 .ko
    ↓
adb push 到板子 /tmp
    ↓
insmod 手动加载
    ↓
看 dmesg
    ↓
看 /sys/bus/spi/devices/
    ↓
看 /sys/bus/iio/devices/
    ↓
读取 BMI160 原始数据
```

---

# 一、PC 端先确认 KO 都有哪些

在 PC 上执行：

```bash
cd ~/project/x2600

echo "===== kernel BMI160 modules ====="
find kernel/kernel -name "*bmi160*.ko"

echo "===== regmap spi modules ====="
find kernel/kernel -name "regmap-spi.ko" -o -name "*regmap*spi*.ko"

echo "===== module_driver bmi160 board modules ====="
find module_driver -name "*bmi160*.ko"

echo "===== module_driver spi modules ====="
find module_driver -name "*spi*.ko"
```

你至少应该有这两个：

```bash
kernel/kernel/drivers/iio/imu/bmi160/bmi160_core.ko
kernel/kernel/drivers/iio/imu/bmi160/bmi160_spi.ko
```

如果你这个目录下也有：

```bash
module_driver/bmi160_spi_board/xxx.ko
```

或者：

```bash
module_driver/output/bmi160_spi_board.ko
```

这个也要用。因为它负责注册 SPI 设备，也就是告诉内核：

```text
SPI0 上挂着 BMI160，片选是 PD05。
```

否则你只加载 `bmi160_spi.ko`，驱动只是注册好了，但不一定能 probe 到设备。

---

# 二、先看模块依赖

执行：

```bash
cd ~/project/x2600/kernel/kernel

modinfo drivers/iio/imu/bmi160/bmi160_core.ko | grep -E "depends|vermagic"
modinfo drivers/iio/imu/bmi160/bmi160_spi.ko  | grep -E "depends|vermagic"
```

如果提示没有 `modinfo`，就用：

```bash
strings drivers/iio/imu/bmi160/bmi160_spi.ko | grep vermagic
```

重点看：

```bash
depends:
vermagic:
```

如果 `depends` 里有：

```text
regmap-spi
industrialio-triggered-buffer
```

那这些依赖模块也要一起 push 到板子。

---

# 三、推到板子 /tmp 目录

先建目录：

```bash
adb shell "mkdir -p /tmp/bmi160_test"
```

推 BMI160 核心模块：

```bash
cd ~/project/x2600/kernel/kernel

adb push drivers/iio/imu/bmi160/bmi160_core.ko /tmp/bmi160_test/
adb push drivers/iio/imu/bmi160/bmi160_spi.ko  /tmp/bmi160_test/
```

如果有 `regmap-spi.ko`，也推：

```bash
REGMAP_SPI=$(find . -name "regmap-spi.ko" | head -1)
echo $REGMAP_SPI
adb push "$REGMAP_SPI" /tmp/bmi160_test/
```

推你的板级模块：

```bash
cd ~/project/x2600

BOARD_KO=$(find module_driver -name "*bmi160*.ko" | head -1)
echo $BOARD_KO
adb push "$BOARD_KO" /tmp/bmi160_test/
```

如果 `BOARD_KO` 是空的，说明你的 `bmi160_spi_board.ko` 还没编出来，需要先：

```bash
cd ~/project/x2600/build
make module_driver
```

然后再：

```bash
find ~/project/x2600/module_driver -name "*bmi160*.ko"
```

---

# 四、板子端加载前先检查 SPI 总线

进入板子：

```bash
adb shell
```

看内核版本：

```bash
uname -r
cat /proc/version
```

看 SPI master 是否存在：

```bash
ls /sys/class/spi_master/
ls /sys/bus/spi/devices/
ls /sys/bus/spi/drivers/
```

正常你希望看到类似：

```bash
spi0
```

或者：

```bash
spi1
```

如果 `/sys/class/spi_master/` 是空的，说明 **SPI 控制器驱动还没起来**。这时候 BMI160 肯定 probe 不起来。

那就要先加载君正 SPI 控制器 KO。你在 PC 端找：

```bash
find ~/project/x2600/module_driver -name "*spi*.ko"
```

如果看到类似：

```bash
x2600_spi.ko
spi_hal.ko
spidev_helper.ko
```

要根据实际名字先推到板子再 insmod。

---

# 五、开始 insmod 加载

在板子上：

```bash
cd /tmp/bmi160_test
ls -l
```

建议先清一下旧模块，避免重复加载：

```bash
rmmod bmi160_spi_board 2>/dev/null
rmmod bmi160_spi 2>/dev/null
rmmod bmi160_core 2>/dev/null
rmmod regmap_spi 2>/dev/null
```

然后按顺序加载。

### 1. 先加载依赖

如果有：

```bash
regmap-spi.ko
```

执行：

```bash
insmod regmap-spi.ko
```

如果没有这个文件，可能是已经编进内核了，不用管。

### 2. 加载 BMI160 core

```bash
insmod bmi160_core.ko
```

看结果：

```bash
dmesg | tail -30
```

### 3. 加载 BMI160 SPI 驱动

```bash
insmod bmi160_spi.ko
```

看结果：

```bash
dmesg | tail -50
```

### 4. 加载你的板级注册模块

如果你有：

```bash
bmi160_spi_board.ko
```

执行：

```bash
insmod bmi160_spi_board.ko
```

再看：

```bash
dmesg | tail -100
```

---

# 六、加载成功后怎么看？

## 1. 看模块是否加载

```bash
lsmod | grep -E "bmi160|regmap"
```

或者：

```bash
ls /sys/module/ | grep bmi160
```

正常应该看到类似：

```bash
bmi160_core
bmi160_spi
bmi160_spi_board
```

---

## 2. 看 SPI 设备有没有出来

```bash
ls /sys/bus/spi/devices/
```

正常可能看到：

```bash
spi0.0
```

或者：

```bash
spi1.0
```

继续看：

```bash
for dev in /sys/bus/spi/devices/*; do
    echo "===== $dev ====="
    cat $dev/modalias 2>/dev/null
    readlink $dev/driver 2>/dev/null
done
```

如果看到某个设备绑定到了：

```bash
bmi160_spi
```

说明 SPI 设备和 BMI160 驱动匹配上了。

---

## 3. 看 IIO 设备有没有出来

```bash
ls /sys/bus/iio/devices/
```

如果成功，应该有类似：

```bash
iio:device0
```

看名字：

```bash
for dev in /sys/bus/iio/devices/iio:device*; do
    echo "===== $dev ====="
    cat $dev/name 2>/dev/null
done
```

可能输出：

```bash
bmi160
```

---

## 4. 读取 BMI160 数据

进入 IIO 目录：

```bash
cd /sys/bus/iio/devices/iio:device0
ls
```

尝试读取：

```bash
cat name

cat in_accel_x_raw
cat in_accel_y_raw
cat in_accel_z_raw

cat in_anglvel_x_raw
cat in_anglvel_y_raw
cat in_anglvel_z_raw
```

如果这些能读出数字，说明：

```text
SPI 通了
BMI160 识别了
IIO 驱动工作了
```

---

# 七、常见报错怎么判断？

## 1. `insmod: invalid module format`

看：

```bash
dmesg | tail -30
```

如果有：

```text
version magic ... should be ...
```

说明 **KO 和板子当前运行的 kernel 版本不匹配**。

解决方法：

```text
要么把你新编译的 kernel 烧到板子；
要么用板子当前正在运行的 kernel 配置和源码重新编译 KO。
```

这点很重要。KO 必须和当前板子运行的内核匹配。

---

## 2. `Unknown symbol xxx`

例如：

```text
Unknown symbol regmap_init_spi
Unknown symbol iio_triggered_buffer_setup
```

说明依赖模块没加载。

处理方法：

```bash
modinfo bmi160_core.ko | grep depends
modinfo bmi160_spi.ko | grep depends
```

然后找到依赖 `.ko`，一起 push，先 insmod 依赖。

常见可能需要：

```bash
regmap-spi.ko
industrialio-triggered-buffer.ko
kfifo_buf.ko
```

具体以 `modinfo depends` 为准。

---

## 3. `bmi160_spi.ko` 加载成功，但是没有 `/sys/bus/iio/devices/iio:device0`

这种情况最常见。

原因是：

```text
驱动加载了，但是没有 SPI 设备来匹配它。
```

你需要检查：

```bash
ls /sys/bus/spi/devices/
```

如果没有 `spi0.0` 或类似设备，说明你的 `bmi160_spi_board.ko` 没有成功注册 SPI 设备，或者 SPI 控制器没起来。

检查：

```bash
dmesg | grep -iE "spi|bmi|iio"
```

---

## 4. 有 `spi0.0`，但是没有绑定 BMI160 驱动

看：

```bash
readlink /sys/bus/spi/devices/spi0.0/driver
```

如果没有输出，说明设备没绑定驱动。

再看有哪些 SPI 驱动：

```bash
ls /sys/bus/spi/drivers/
```

如果有：

```bash
bmi160_spi
```

可以尝试手动 bind：

```bash
echo bmi160_spi > /sys/bus/spi/devices/spi0.0/driver_override
echo spi0.0 > /sys/bus/spi/drivers/bmi160_spi/bind
```

然后看：

```bash
dmesg | tail -50
ls /sys/bus/iio/devices/
```

---

# 八、给你一套最小调试命令

PC 端：

```bash
cd ~/project/x2600

adb shell "mkdir -p /tmp/bmi160_test"

adb push kernel/kernel/drivers/iio/imu/bmi160/bmi160_core.ko /tmp/bmi160_test/
adb push kernel/kernel/drivers/iio/imu/bmi160/bmi160_spi.ko  /tmp/bmi160_test/

BOARD_KO=$(find module_driver -name "*bmi160*.ko" | head -1)
echo "BOARD_KO=$BOARD_KO"
adb push "$BOARD_KO" /tmp/bmi160_test/
```

板子端：

```bash
adb shell
cd /tmp/bmi160_test

rmmod bmi160_spi_board 2>/dev/null
rmmod bmi160_spi 2>/dev/null
rmmod bmi160_core 2>/dev/null

insmod bmi160_core.ko
dmesg | tail -20

insmod bmi160_spi.ko
dmesg | tail -30

insmod bmi160_spi_board.ko
dmesg | tail -100

lsmod | grep bmi160
ls /sys/bus/spi/devices/
ls /sys/bus/iio/devices/
```

如果出现：

```bash
/sys/bus/iio/devices/iio:device0
```

继续：

```bash
cd /sys/bus/iio/devices/iio:device0

cat name
cat in_accel_x_raw
cat in_accel_y_raw
cat in_accel_z_raw
cat in_anglvel_x_raw
cat in_anglvel_y_raw
cat in_anglvel_z_raw
```

---

# 九、调试顺序你就记这个

不要一上来就看数据。按这个顺序排：

```text
1. KO 能不能 insmod？
   ↓
2. 有没有 Unknown symbol / invalid module format？
   ↓
3. SPI master 有没有起来？
   /sys/class/spi_master/
   ↓
4. SPI device 有没有注册？
   /sys/bus/spi/devices/spi0.0
   ↓
5. SPI device 有没有绑定 bmi160_spi？
   readlink /sys/bus/spi/devices/spi0.0/driver
   ↓
6. IIO 设备有没有出现？
   /sys/bus/iio/devices/iio:device0
   ↓
7. raw 数据能不能读？
   in_accel_x_raw / in_anglvel_x_raw
```

你现在最先做的是：**把 `bmi160_core.ko`、`bmi160_spi.ko`、`bmi160_spi_board.ko` 推到 `/tmp`，按顺序 insmod，然后把 `dmesg | tail -100` 的输出贴出来。**


----------
-----------
----------
你现在这个结果：

```bash
bmi160_core.ko
bmi160_spi.ko
```

说明 **BMI160 驱动本体已经编出来了**。现在不是少两个都必须处理，而是：

```text
regmap-spi.ko          不一定缺，可能已经编进内核了
bmi160_spi_board.ko    这个确实没编出来，需要处理
```

---

## 1. 先确认 regmap-spi 到底是不是内置了

执行：

```bash
cd ~/project/x2600/kernel/kernel

grep -E "CONFIG_REGMAP|CONFIG_REGMAP_SPI|CONFIG_BMI160" .config
```

重点看这个：

```bash
CONFIG_REGMAP_SPI=y
```

如果是 `y`，那就正常，**不会生成 `regmap-spi.ko`**，因为它已经编进内核了。

如果是：

```bash
CONFIG_REGMAP_SPI=m
```

但是你又找不到 `regmap-spi.ko`，那才需要重新编：

```bash
cd ~/project/x2600/kernel/kernel
make M=drivers/base/regmap modules
find drivers/base/regmap -name "*regmap*.ko"
```

所以你现在先不要管 `regmap-spi.ko`，大概率它没问题。

---

## 2. 重点是 bmi160_spi_board.ko 没出来

你现在这个命令没找到：

```bash
find module_driver -name "*bmi160*.ko"
```

说明你的板级模块没有生成。

先查源码目录还在不在：

```bash
cd ~/project/x2600

find module_driver -maxdepth 4 -type d | grep -i bmi
find module_driver -maxdepth 5 -type f | grep -i bmi
```

如果你能看到类似：

```bash
module_driver/bmi160_spi_board
module_driver/bmi160_spi_board/bmi160_spi_board.c
module_driver/bmi160_spi_board/Makefile
```

说明源码在，只是没编出来。

---

## 3. 先重新编 module_driver

```bash
cd ~/project/x2600/build

make module_driver V=1 2>&1 | tee ~/module_driver_build.log
```

然后看有没有编 BMI160：

```bash
grep -i "bmi160" ~/module_driver_build.log
```

再找 ko：

```bash
cd ~/project/x2600
find module_driver -name "*bmi160*.ko"
```

如果还是没有，说明 **module_driver 总 Makefile 没把 bmi160_spi_board 目录加进去**。

---

## 4. 如果 module_driver 没自动编，就手动编这个目录

假设你的目录是：

```bash
~/project/x2600/module_driver/bmi160_spi_board
```

那执行：

```bash
cd ~/project/x2600/module_driver/bmi160_spi_board

make -C ~/project/x2600/kernel/kernel M=$PWD modules V=1
```

然后再查：

```bash
find . -name "*.ko"
```

理想结果：

```bash
./bmi160_spi_board.ko
```

如果报交叉编译器找不到，再加工具链。先查工具链 gcc 名字：

```bash
ls ~/project/x2600/tools/toolchains/mips-xburst2-gcc720-glibc238/bin/*gcc
```

假设看到的是：

```bash
mips-linux-gnu-gcc
```

那就这样编：

```bash
cd ~/project/x2600/module_driver/bmi160_spi_board

export PATH=~/project/x2600/tools/toolchains/mips-xburst2-gcc720-glibc238/bin:$PATH

make -C ~/project/x2600/kernel/kernel \
    M=$PWD \
    ARCH=mips \
    CROSS_COMPILE=mips-linux-gnu- \
    modules V=1
```

如果你的 gcc 前缀不是 `mips-linux-gnu-`，就按实际名字改。比如看到：

```bash
mipsel-linux-gnu-gcc
```

那就是：

```bash
CROSS_COMPILE=mipsel-linux-gnu-
```

---

## 5. 如果 bmi160_spi_board 目录根本不存在

那就说明你现在只有 Linux 原生 BMI160 驱动，还没有“板级注册模块”。

这种情况下，`bmi160_core.ko` 和 `bmi160_spi.ko` 可以加载，但**不会真正 probe BMI160**，因为系统不知道：

```text
哪一路 SPI 总线？
哪个 CS？
设备名字是什么？
SPI mode 是多少？
最大频率是多少？
```

也就是说，少了这个模块：

```text
bmi160_spi_board.ko
```

系统里不会自动出现：

```text
/sys/bus/iio/devices/iio:device0
```

---

## 6. 板级模块的作用

你可以理解成：

```text
bmi160_core.ko
    ↓
BMI160 通用核心驱动

bmi160_spi.ko
    ↓
BMI160 通过 SPI 访问的驱动

bmi160_spi_board.ko
    ↓
告诉内核：我的板子上 SPIx 的 CSx 接了一个 BMI160
```

所以你的加载顺序应该是：

```bash
insmod bmi160_core.ko
insmod bmi160_spi.ko
insmod bmi160_spi_board.ko
```

注意：最后这个 `bmi160_spi_board.ko` 才会真正触发设备创建和 probe。

---

## 7. 一个最小 bmi160_spi_board.c 参考

如果你现在没有这个文件，可以先用类似这种方式写一个调试版。

路径：

```bash
mkdir -p ~/project/x2600/module_driver/bmi160_spi_board
cd ~/project/x2600/module_driver/bmi160_spi_board
```

创建：

```bash
vim bmi160_spi_board.c
```

内容参考：

```c
#include <linux/module.h>
#include <linux/init.h>
#include <linux/spi/spi.h>

#define BMI160_SPI_BUS_NUM      0
#define BMI160_SPI_CHIP_SELECT  0
#define BMI160_SPI_MAX_HZ       1000000

static struct spi_device *bmi160_spi_dev;

static int __init bmi160_spi_board_init(void)
{
    struct spi_master *master;
    struct spi_board_info spi_info = {
        .modalias = "bmi160",
        .max_speed_hz = BMI160_SPI_MAX_HZ,
        .bus_num = BMI160_SPI_BUS_NUM,
        .chip_select = BMI160_SPI_CHIP_SELECT,
        .mode = SPI_MODE_0,
    };

    pr_info("bmi160_spi_board: init\n");

    master = spi_busnum_to_master(BMI160_SPI_BUS_NUM);
    if (!master) {
        pr_err("bmi160_spi_board: spi bus %d not found\n", BMI160_SPI_BUS_NUM);
        return -ENODEV;
    }

    bmi160_spi_dev = spi_new_device(master, &spi_info);
    put_device(&master->dev);

    if (!bmi160_spi_dev) {
        pr_err("bmi160_spi_board: failed to create spi device\n");
        return -ENODEV;
    }

    pr_info("bmi160_spi_board: created spi device bus=%d cs=%d\n",
            BMI160_SPI_BUS_NUM, BMI160_SPI_CHIP_SELECT);

    return 0;
}

static void __exit bmi160_spi_board_exit(void)
{
    pr_info("bmi160_spi_board: exit\n");

    if (bmi160_spi_dev) {
        spi_unregister_device(bmi160_spi_dev);
        bmi160_spi_dev = NULL;
    }
}

module_init(bmi160_spi_board_init);
module_exit(bmi160_spi_board_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("vik");
MODULE_DESCRIPTION("BMI160 SPI board device register module");
```

再创建 Makefile：

```bash
vim Makefile
```

内容：

```makefile
obj-m += bmi160_spi_board.o
```

然后编译：

```bash
cd ~/project/x2600/module_driver/bmi160_spi_board

make -C ~/project/x2600/kernel/kernel M=$PWD modules V=1
```

编译完应该有：

```bash
bmi160_spi_board.ko
```

---

## 8. 注意你的 PD05 不是直接写到 chip_select 里面

你说你的 CS 是 `PD05`，这里要注意：

```c
.chip_select = 0
```

这里的 `0` 不是 GPIO 编号，也不是 PD05。

它表示：

```text
SPI 控制器的第 0 个片选
```

所以你要确认 PD05 在君正 X2600 平台里是不是被配置成了这个 SPI 控制器的 CS0。

如果 PD05 只是普通 GPIO，而不是 SPI 控制器管理的 CS，那上面的普通写法可能还不够，需要在平台 pinctrl / SPI controller / DTS / 板级代码里配置 `cs-gpios` 或者君正自己的片选配置。

先别急着改，调试第一步可以先查板子上有哪些 SPI master：

```bash
ls /sys/class/spi_master/
ls /sys/bus/spi/devices/
```

如果看到：

```bash
spi0
```

那板级模块里：

```c
#define BMI160_SPI_BUS_NUM 0
```

如果看到：

```bash
spi1
```

那就改成：

```c
#define BMI160_SPI_BUS_NUM 1
```

---

## 9. 你现在应该按这个顺序做

### 第一步：确认 regmap

```bash
cd ~/project/x2600/kernel/kernel
grep -E "CONFIG_REGMAP_SPI|CONFIG_BMI160" .config
```

如果是：

```bash
CONFIG_REGMAP_SPI=y
```

不用管它。

---

### 第二步：查 board 源码

```bash
cd ~/project/x2600
find module_driver -maxdepth 5 -type f | grep -i bmi
```

如果有源码，编它。

如果没有源码，新建 `module_driver/bmi160_spi_board`。

---

### 第三步：编 board ko

```bash
cd ~/project/x2600/module_driver/bmi160_spi_board
make -C ~/project/x2600/kernel/kernel M=$PWD modules V=1
```

---

### 第四步：最终你应该有这三个

```bash
cd ~/project/x2600

find kernel/kernel -name "bmi160_core.ko"
find kernel/kernel -name "bmi160_spi.ko"
find module_driver -name "bmi160_spi_board.ko"
```

结果应该类似：

```bash
kernel/kernel/drivers/iio/imu/bmi160/bmi160_core.ko
kernel/kernel/drivers/iio/imu/bmi160/bmi160_spi.ko
module_driver/bmi160_spi_board/bmi160_spi_board.ko
```

这三个有了，就可以推到板子 `/tmp` 手动加载了。
