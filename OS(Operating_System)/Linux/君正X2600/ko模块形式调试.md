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
