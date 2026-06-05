你这个 **BMI160 + IIO + SPI 驱动方案里，最终不需要用 `cmd_spi`**。

`cmd_spi` 的作用只是 **调试 SPI 总线通不通**，不是正式驱动 BMI160 的方式。

---

## 你的最终链路应该是这个

```text
IConfigTool 配 SPI0 的 SCLK/MOSI/MISO
        ↓
soc_spi.ko 注册 Linux SPI master
        ↓
bmi160_spi_board.ko 注册 spi_device，modalias="bmi160"
        ↓
bmi160_spi.ko 匹配设备并 probe
        ↓
bmi160_core.ko 注册 IIO
        ↓
读取 /sys/bus/iio/devices/iio:deviceX/xxx_raw
```

这里面没有 `cmd_spi`。

---

## 那 `cmd_spi` 是干啥的？

它是君正文档里给的 **SPI 应用层测试工具**，比如：

```sh
cmd_spi add_dev 0 pd05
cmd_spi set /dev/spidev0.0 mode=0x03 speed=500000 bits=8 lsb=0
cmd_spi transfer /dev/spidev0.0 0x00 0x01 0x02
cmd_spi del_dev /dev/spidev0.0
```

文档里也写了，`cmd_spi add_dev` 是添加一个 `/dev/spidevX.X` 设备，然后用 `transfer` 做 SPI 收发测试。它属于应用层调试方式。

---

## 什么时候可以用 `cmd_spi`？

只在你想验证这些东西时用：

```text
1. SPI0 有没有起来
2. PD00/PD01/PD02 有没有波形
3. PD05 作为 CS 有没有拉低
4. SPI mode / speed 设置后能不能收发
```

比如你想先用逻辑分析仪看波形，可以临时：

```sh
cmd_spi add_dev 0 pd05
cmd_spi set /dev/spidev0.0 mode=0x00 speed=1000000 bits=8 lsb=0
cmd_spi transfer /dev/spidev0.0 0x80 0x00
```

但这只是测试。

---

## 正式用 BMI160 IIO 时不要用 `cmd_spi`

因为 `cmd_spi add_dev 0 pd05` 会创建：

```text
/dev/spidev0.0
/sys/bus/spi/devices/spi0.0
```

这个时候 `spi0.0` 可能已经被 `spidev` 占用了。

而你的 BMI160 也想注册成：

```text
spi0.0
modalias = bmi160
```

同一个 SPI0 CS0 不能同时给两个设备用。

所以正式跑 BMI160 时，不要先执行：

```sh
cmd_spi add_dev 0 pd05
```

而是加载：

```sh
insmod soc_spi.ko
insmod industrialio.ko
insmod kfifo_buf.ko
insmod industrialio-triggered-buffer.ko
insmod bmi160_core.ko
insmod bmi160_spi.ko
insmod bmi160_spi_board.ko
```

然后读：

```sh
cat /sys/bus/iio/devices/iio:device0/in_accel_x_raw
cat /sys/bus/iio/devices/iio:device0/in_accel_y_raw
cat /sys/bus/iio/devices/iio:device0/in_accel_z_raw
```

---

一句话：**`cmd_spi` 你可以用来前期测 SPI 波形，但 BMI160 正式走 IIO 驱动时不用它，而且最好不要同时用。**


--------
--------
------
对，你板子里**可能确实没有 `cmd_spi`**。而且先说结论：**BMI160 走 IIO+SPI 不需要 `cmd_spi`，它只是 SPI 调试工具。**

文档里 `cmd_spi` 的用法是：`cmd_spi add_dev 0 pc16` 添加 `/dev/spidev0.0`，再用 `cmd_spi transfer` 做 SPI 收发测试，最后 `cmd_spi del_dev` 删除设备。也就是说它是应用层测试 SPI 的命令，不是 BMI160 IIO 驱动必须依赖的东西。

---

## 1. 先确认板子上到底有没有

板子串口里执行：

```sh
which cmd_spi
```

如果没输出，再找：

```sh
find / -name "cmd_spi" 2>/dev/null
```

如果还是没有，说明 rootfs 里确实没带这个工具。

---

## 2. 在源码里找 cmd_spi

编译主机上执行：

```sh
cd /home/vik/project/x2600

find . -name "*cmd_spi*" -o -name "cmd_spi.c"
```

再搜关键字：

```sh
grep -R "cmd_spi" -n . 2>/dev/null | head -100
```

如果能搜到类似：

```text
libhardware2/...
libutils2/...
module_driver/...
tools/...
```

说明源码里有，只是没编进 rootfs。

---

## 3. IConfigTool 里可能要勾 SPI 辅助工具/模块

文档里提到 SPI 应用层接口包含 `spi_add_device()`、`spi_del_device()`、`spi_transfer()`、`spi_set_mode()`、`spi_set_speed()` 等，而 `cmd_spi` 就是基于这些接口做的命令行工具。

你可以在 IConfigTool 里找这些选项：

```text
[root]
  → [模块化驱动]
    → [驱动]
```

重点看有没有：

```text
spi设备辅助模块（动态增加或删除spi设备）
spi设备驱动模块（spi设备驱动端）
utils / 基础实用工具集
cmd_spi
SPI command
SPI test
```

如果有 `cmd_spi` 或 `utils spi` 这种选项，就勾上。

---

## 4. 用命令查具体配置项名字

因为不同君正 SDK 配置名可能不一样，你可以直接 grep：

```sh
cd /home/vik/project/x2600

grep -R "CMD_SPI\|cmd_spi\|UTILS_SPI\|SPI_DEVICE_HELPER\|MD_SPI_DEVICE\|APP_libhardware2_spi" \
    build module_driver libhardware2 libutils2 2>/dev/null | head -200
```

如果看到类似：

```text
MD_SPI_DEVICE_HELPER
MD_SPI_DEVICE
MD_UTILS_SPI
APP_libhardware2_spi
```

那就说明这几个配置和 SPI 工具/辅助模块有关。

---

## 5. 重新编译并打包 rootfs

如果只是想让板子有 `cmd_spi`，不能只 `make kernel`。
因为 `cmd_spi` 是用户态工具，应该进 rootfs。

你先看 make 目标：

```sh
cd /home/vik/project/x2600
make help | grep -Ei "module|hardware|utils|rootfs|image|app"
```

然后按你工程支持的目标编译。大概率需要类似：

```sh
make module_driver
make libhardware2
make libutils2
make
```

如果不确定，最笨但最稳的是：

```sh
make
```

然后重新烧录包含 rootfs/userdata/system 的完整系统。

烧完后板子上确认：

```sh
which cmd_spi
find / -name "cmd_spi" 2>/dev/null
```

---

## 6. 没有 cmd_spi 也不影响 BMI160

你正式跑 BMI160 IIO 的时候，不需要：

```sh
cmd_spi add_dev 0 pd05
```

而是走这个：

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

然后读：

```sh
ls /sys/bus/iio/devices/

cat /sys/bus/iio/devices/iio:device0/name
cat /sys/bus/iio/devices/iio:device0/in_accel_x_raw
cat /sys/bus/iio/devices/iio:device0/in_accel_y_raw
cat /sys/bus/iio/devices/iio:device0/in_accel_z_raw
```

---

## 7. cmd_spi 和 BMI160 不要同时抢同一个 CS

这个很重要。

如果你执行了：

```sh
cmd_spi add_dev 0 pd05
```

它可能会创建：

```text
/dev/spidev0.0
/sys/bus/spi/devices/spi0.0
```

这个时候 `spi0.0` 已经被 spidev 占了。

而你的 BMI160 也想注册：

```text
spi0.0
modalias = bmi160
```

所以正式跑 BMI160 时，不要用 `cmd_spi add_dev 0 pd05`。
`cmd_spi` 只适合前期临时看 SPI 波形，不是必须工具。
