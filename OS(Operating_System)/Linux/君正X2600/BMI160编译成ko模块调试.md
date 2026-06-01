可以。你现在要做的是：**把 BMI160 相关源码也放进 `module_driver` 目录里，让 `module_driver` 体系编出这些 KO**。

不过先说清楚一个关键点：

```text
soc_spi.ko / utils.ko 本来就在 module_driver 里
bmi160_spi_board.ko 也是你自己放在 module_driver 里的
bmi160_core.ko / bmi160_spi.ko 目前来自 kernel/drivers/iio/imu/bmi160
```

所以你想“全在 module_driver 里编译”，本质就是把：

```text
kernel/kernel/drivers/iio/imu/bmi160/bmi160_core.c
kernel/kernel/drivers/iio/imu/bmi160/bmi160_spi.c
kernel/kernel/drivers/iio/imu/bmi160/bmi160.h
```

复制到 `module_driver` 里，然后作为外部内核模块编译。

---

# 1. 推荐目录结构

建议这样放：

```text
module_driver/drivers/bmi160_iio/
├── bmi160_core.c
├── bmi160_spi.c
├── bmi160.h
├── bmi160_spi_board.c
└── Makefile
```

这样最后 `make` 会生成：

```text
bmi160_core.ko
bmi160_spi.ko
bmi160_spi_board.ko
```

然后统一复制到：

```text
module_driver/output/
```

---

# 2. 先创建目录并复制源码

在 PC 上执行：

```bash
cd ~/project/x2600

mkdir -p module_driver/drivers/bmi160_iio

cp -v kernel/kernel/drivers/iio/imu/bmi160/bmi160_core.c \
      module_driver/drivers/bmi160_iio/

cp -v kernel/kernel/drivers/iio/imu/bmi160/bmi160_spi.c \
      module_driver/drivers/bmi160_iio/

cp -v kernel/kernel/drivers/iio/imu/bmi160/bmi160.h \
      module_driver/drivers/bmi160_iio/

cp -v module_driver/bmi160_spi_board/bmi160_spi_board.c \
      module_driver/drivers/bmi160_iio/
```

检查：

```bash
ls -l module_driver/drivers/bmi160_iio
```

应该看到：

```text
bmi160_core.c
bmi160_spi.c
bmi160.h
bmi160_spi_board.c
```

---

# 3. 写 `module_driver/drivers/bmi160_iio/Makefile`

创建：

```bash
cd ~/project/x2600/module_driver/drivers/bmi160_iio

vim Makefile
```

内容写成：

```makefile
obj-m += bmi160_core.o
obj-m += bmi160_spi.o
obj-m += bmi160_spi_board.o

KERNEL_DIR ?= /home/vik/project/x2600/kernel/kernel
ARCH ?= mips
CROSS_COMPILE ?= /home/vik/project/x2600/build/../buildroot/buildroot24/output/host/usr/bin/mips-linux-gnu-

all:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules
	mkdir -p /home/vik/project/x2600/module_driver/output
	cp -v *.ko /home/vik/project/x2600/module_driver/output/

clean:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) clean
	rm -f /home/vik/project/x2600/module_driver/output/bmi160_core.ko
	rm -f /home/vik/project/x2600/module_driver/output/bmi160_spi.ko
	rm -f /home/vik/project/x2600/module_driver/output/bmi160_spi_board.ko
```

注意：`$(MAKE)` 前面必须是 **Tab**，不能是空格。

可以检查：

```bash
sed -n l Makefile
```

如果看到：

```text
\t$(MAKE) ...
```

说明是 Tab。

---

# 4. 单独编译 BMI160 这一组模块

先确保 kernel 已经编译过：

```bash
cd ~/project/x2600/build
make kernel
```

然后编 BMI160 module_driver 目录：

```bash
cd ~/project/x2600/module_driver/drivers/bmi160_iio

make clean
make V=1
```

成功后检查：

```bash
ls -l *.ko
```

应该有：

```text
bmi160_core.ko
bmi160_spi.ko
bmi160_spi_board.ko
```

再检查 `module_driver/output`：

```bash
ls -l ~/project/x2600/module_driver/output/bmi160*.ko
```

应该有：

```text
module_driver/output/bmi160_core.ko
module_driver/output/bmi160_spi.ko
module_driver/output/bmi160_spi_board.ko
```

---

# 5. 建议把 kernel 原来的 BMI160 关闭

既然你要放到 `module_driver` 编译，建议不要让 kernel 那边也编一份，避免以后搞混。

进入内核配置：

```bash
cd ~/project/x2600/kernel/kernel
make menuconfig
```

把这两个关掉：

```text
CONFIG_BMI160
CONFIG_BMI160_SPI
```

也就是：

```text
# CONFIG_BMI160 is not set
# CONFIG_BMI160_SPI is not set
```

但是这些还要保留：

```text
CONFIG_IIO=y
CONFIG_SPI=y
CONFIG_SPI_MASTER=y
CONFIG_REGMAP=y
CONFIG_REGMAP_SPI=y
```

如果你想减少依赖 KO，建议把 IIO buffer 依赖编进内核：

```text
CONFIG_KFIFO_BUF=y
CONFIG_IIO_TRIGGERED_BUFFER=y
```

如果它们只能是 `m`，那就还需要加载：

```text
kfifo_buf.ko
industrialio-triggered-buffer.ko
```

---

# 6. IIO 依赖要不要也放 module_driver？

这里要分清楚：

```text
bmi160_core.ko / bmi160_spi.ko 是 BMI160 驱动，可以搬到 module_driver
kfifo_buf.ko / industrialio-triggered-buffer.ko 是 Linux IIO 框架通用模块，不建议搬
```

推荐方案：

```text
CONFIG_KFIFO_BUF=y
CONFIG_IIO_TRIGGERED_BUFFER=y
```

这样这两个依赖直接编进 kernel，就不用加载：

```text
kfifo_buf.ko
industrialio-triggered-buffer.ko
```

最终加载顺序会简化成：

```text
utils.ko
soc_spi.ko
bmi160_core.ko
bmi160_spi.ko
bmi160_spi_board.ko
```

如果你暂时不改 kernel，那就继续保留：

```text
kfifo_buf.ko
industrialio-triggered-buffer.ko
```

但是它们还是从 kernel 编出来，不建议复制源码到 module_driver。

---

# 7. 推送到板子

如果用 module_driver/output 里的 KO，推送就统一很多：

```bash
cd ~/project/x2600

adb shell "mkdir -p /tmp/kernel_drv_mode"

adb push module_driver/output/utils.ko /tmp/kernel_drv_mode/
adb push module_driver/output/soc_spi.ko /tmp/kernel_drv_mode/

adb push module_driver/output/bmi160_core.ko /tmp/kernel_drv_mode/
adb push module_driver/output/bmi160_spi.ko /tmp/kernel_drv_mode/
adb push module_driver/output/bmi160_spi_board.ko /tmp/kernel_drv_mode/
```

如果 IIO buffer 还是模块，再推：

```bash
adb push kernel/kernel/drivers/iio/buffer/kfifo_buf.ko /tmp/kernel_drv_mode/
adb push kernel/kernel/drivers/iio/buffer/industrialio-triggered-buffer.ko /tmp/kernel_drv_mode/
```

---

# 8. 加载顺序

## 情况 A：IIO 依赖已经编进 kernel

如果你把：

```text
CONFIG_KFIFO_BUF=y
CONFIG_IIO_TRIGGERED_BUFFER=y
```

编进了内核，那么加载：

```sh
cd /tmp/kernel_drv_mode

rmmod bmi160_spi_board 2>/dev/null
rmmod bmi160_spi 2>/dev/null
rmmod bmi160_core 2>/dev/null
rmmod soc_spi 2>/dev/null

dmesg -c > /dev/null

insmod utils.ko 2>/dev/null

insmod ./soc_spi.ko spi0_is_enable=1 spi1_is_enable=0 div_ssi_rate=10000000 spi0_clk=PD00 spi0_mosi=PD01 spi0_miso=PD02

ls -l /sys/class/spi_master/

insmod bmi160_core.ko
insmod bmi160_spi.ko
insmod bmi160_spi_board.ko

dmesg | tail -100
ls -l /sys/bus/spi/devices/
ls -l /sys/bus/iio/devices/
```

---

## 情况 B：IIO 依赖还是 KO

那加载：

```sh
cd /tmp/kernel_drv_mode

rmmod bmi160_spi_board 2>/dev/null
rmmod bmi160_spi 2>/dev/null
rmmod bmi160_core 2>/dev/null
rmmod industrialio_triggered_buffer 2>/dev/null
rmmod kfifo_buf 2>/dev/null
rmmod soc_spi 2>/dev/null

dmesg -c > /dev/null

insmod utils.ko 2>/dev/null

insmod ./soc_spi.ko spi0_is_enable=1 spi1_is_enable=0 div_ssi_rate=10000000 spi0_clk=PD00 spi0_mosi=PD01 spi0_miso=PD02

ls -l /sys/class/spi_master/

insmod kfifo_buf.ko
insmod industrialio-triggered-buffer.ko

insmod bmi160_core.ko
insmod bmi160_spi.ko
insmod bmi160_spi_board.ko

dmesg | tail -100
ls -l /sys/bus/spi/devices/
ls -l /sys/bus/iio/devices/
```

---

# 9. 接入 `make module_driver`

上面只是你进入目录手动：

```bash
make
```

如果你想让：

```bash
cd ~/project/x2600/build
make module_driver
```

自动编译这个 BMI160 目录，就要把它挂进君正 `module_driver` 编译系统。

先找 `spidev` 是怎么挂进去的：

```bash
cd ~/project/x2600

grep -R "drivers/spidev" -n module_driver | head -50
grep -R "spidev_helper" -n module_driver | head -50
grep -R "soc/x2600_510/spi" -n module_driver | head -50
```

找到类似上层 Makefile 或 package 文件后，照着加一项。

如果你暂时不想研究君正整套 Makefile，最简单粗暴是在 `module_driver/Makefile` 的合适位置加一行调用：

```makefile
	$(MAKE) -C drivers/bmi160_iio
```

但是这个要看你 `module_driver/Makefile` 实际结构，不能乱加。你可以先贴：

```bash
sed -n '1,200p' module_driver/Makefile
```

我可以按你的实际 Makefile 帮你插。

---

# 10. 更稳的折中方案

我建议你先分两阶段：

## 阶段 1：先手动在 module_driver 目录里编译

```bash
cd ~/project/x2600/module_driver/drivers/bmi160_iio
make clean
make V=1
```

确保这三个能出来：

```text
bmi160_core.ko
bmi160_spi.ko
bmi160_spi_board.ko
```

并复制到：

```text
module_driver/output/
```

这一步先跑通。

## 阶段 2：再接入 `make module_driver`

等确认这几个 KO 能加载，再把 `drivers/bmi160_iio` 加入君正上层 Makefile / Config.in。

否则你会同时遇到两个问题：

```text
1. BMI160 KO 本身能不能编
2. 君正 module_driver 编译系统有没有接对
```

先解决第 1 个，再解决第 2 个。

---

# 11. 最终你想要的效果

后面你希望一条命令：

```bash
cd ~/project/x2600/build
make module_driver
```

自动生成：

```text
module_driver/output/utils.ko
module_driver/output/soc_spi.ko
module_driver/output/bmi160_core.ko
module_driver/output/bmi160_spi.ko
module_driver/output/bmi160_spi_board.ko
```

然后板子上统一从：

```text
/tmp/kernel_drv_mode/
```

或者 rootfs 里的：

```text
/module_driver/
```

加载。

这个方向是对的。

---

# 12. 一句话建议

你现在先按这个做：

```bash
cd ~/project/x2600

mkdir -p module_driver/drivers/bmi160_iio

cp kernel/kernel/drivers/iio/imu/bmi160/bmi160_core.c module_driver/drivers/bmi160_iio/
cp kernel/kernel/drivers/iio/imu/bmi160/bmi160_spi.c module_driver/drivers/bmi160_iio/
cp kernel/kernel/drivers/iio/imu/bmi160/bmi160.h module_driver/drivers/bmi160_iio/
cp module_driver/bmi160_spi_board/bmi160_spi_board.c module_driver/drivers/bmi160_iio/
```

然后写上面的 `Makefile`，执行：

```bash
cd ~/project/x2600/module_driver/drivers/bmi160_iio
make clean
make V=1
```

先把：

```text
bmi160_core.ko
bmi160_spi.ko
bmi160_spi_board.ko
```

全部从 `module_driver` 里编出来。接入 `make module_driver` 是下一步。



-----
--------
-------

可以，但先纠正一个概念：**编译成 `.ko` 模块加载，仍然是内核态驱动**。区别只是：

```text
built-in：编进 kernel 镜像，启动时就在内核里
module：编成 .ko，运行后手动 insmod / modprobe 加载
```

你现在要的就是 **KO 模块调试方式**，这个是对的，方便反复改、反复加载。

---

# 1. BMI160 不是一个 `bmi160.ko`

Linux 里面 BMI160 一般会拆成两个模块：

```text
bmi160_core.ko    BMI160 核心驱动，负责 IIO、寄存器配置、数据通道
bmi160_spi.ko     BMI160 的 SPI 总线适配驱动
```

再加上你自己写的板级注册模块：

```text
bmi160_spi_board.ko   告诉系统：SPI0 上有一个 BMI160，CS 是 PD05
```

所以你后面不是加载一个 `bmi160.ko`，而是加载这一组：

```text
kfifo_buf.ko
industrialio-triggered-buffer.ko
bmi160_core.ko
bmi160_spi.ko
bmi160_spi_board.ko
```

---

# 2. 依赖关系大概是这样

你的完整模块链路是：

```text
utils.ko
    ↓
soc_spi.ko
    ↓
注册 spi0 master
    ↓
kfifo_buf.ko
industrialio-triggered-buffer.ko
    ↓
bmi160_core.ko
    ↓
bmi160_spi.ko
    ↓
bmi160_spi_board.ko
    ↓
创建 spi0.0
    ↓
bmi160_spi probe
    ↓
成功后生成 /sys/bus/iio/devices/iio:device0
```

也就是分两部分。

第一部分是君正 SPI 控制器：

```text
utils.ko
soc_spi.ko
```

第二部分是 BMI160 + IIO：

```text
kfifo_buf.ko
industrialio-triggered-buffer.ko
bmi160_core.ko
bmi160_spi.ko
bmi160_spi_board.ko
```

---

# 3. 内核配置要选成 M

进入 kernel menuconfig：

```bash
cd ~/project/x2600/kernel/kernel
make menuconfig
```

确认这些是 `m` 或 `y`：

```text
CONFIG_SPI=y
CONFIG_SPI_MASTER=y

CONFIG_IIO=y
CONFIG_KFIFO_BUF=m
CONFIG_IIO_TRIGGERED_BUFFER=m

CONFIG_BMI160=m
CONFIG_BMI160_SPI=m
```

重点是：

```text
CONFIG_BMI160=m
CONFIG_BMI160_SPI=m
```

这样才会生成：

```text
bmi160_core.ko
bmi160_spi.ko
```

如果是 `y`，就是编进内核，不会有 `.ko`。

---

# 4. 编译 BMI160 KO

你可以整体编译 kernel：

```bash
cd ~/project/x2600/build
make kernel
```

然后查：

```bash
cd ~/project/x2600

find kernel/kernel/drivers/iio/imu/bmi160 -name "*.ko"
find kernel/kernel/drivers/iio/buffer -name "*.ko"
```

正常应该有：

```text
kernel/kernel/drivers/iio/imu/bmi160/bmi160_core.ko
kernel/kernel/drivers/iio/imu/bmi160/bmi160_spi.ko

kernel/kernel/drivers/iio/buffer/kfifo_buf.ko
kernel/kernel/drivers/iio/buffer/industrialio-triggered-buffer.ko
```

也可以只编 BMI160 目录：

```bash
cd ~/project/x2600/kernel/kernel

make M=drivers/iio/imu/bmi160 modules V=1
make M=drivers/iio/buffer modules V=1
```

---

# 5. 编译君正 SPI 控制器 KO

你的 SPI 控制器不是在：

```text
kernel/kernel/drivers/spi/
```

而是在君正：

```text
module_driver/soc/x2600_510/spi/soc_spi.ko
```

整体编译：

```bash
cd ~/project/x2600/build
make module_driver
```

查：

```bash
cd ~/project/x2600

find module_driver -name "utils.ko"
find module_driver -name "soc_spi.ko"
find module_driver -name "spidev*.ko"
```

正常有：

```text
module_driver/output/utils.ko
module_driver/output/soc_spi.ko
module_driver/output/spidev.ko
module_driver/output/spidev_helper.ko
```

---

# 6. 编译你的 board 模块

你的目录是：

```bash
~/project/x2600/module_driver/bmi160_spi_board
```

里面应该有：

```text
bmi160_spi_board.c
Makefile
```

单独编译：

```bash
cd ~/project/x2600/module_driver/bmi160_spi_board

make clean
make V=1
```

或者：

```bash
cd ~/project/x2600/module_driver/bmi160_spi_board

make -C ~/project/x2600/kernel/kernel M=$PWD ARCH=mips CROSS_COMPILE=/home/vik/project/x2600/build/../buildroot/buildroot24/output/host/usr/bin/mips-linux-gnu- modules V=1
```

编完确认：

```bash
ls -l bmi160_spi_board.ko
```

---

# 7. 推送到板子

PC 端：

```bash
cd ~/project/x2600

adb shell "mkdir -p /tmp/kernel_drv_mode"

adb push module_driver/output/utils.ko /tmp/kernel_drv_mode/
adb push module_driver/output/soc_spi.ko /tmp/kernel_drv_mode/

adb push kernel/kernel/drivers/iio/buffer/kfifo_buf.ko /tmp/kernel_drv_mode/
adb push kernel/kernel/drivers/iio/buffer/industrialio-triggered-buffer.ko /tmp/kernel_drv_mode/

adb push kernel/kernel/drivers/iio/imu/bmi160/bmi160_core.ko /tmp/kernel_drv_mode/
adb push kernel/kernel/drivers/iio/imu/bmi160/bmi160_spi.ko /tmp/kernel_drv_mode/

adb push module_driver/bmi160_spi_board/bmi160_spi_board.ko /tmp/kernel_drv_mode/
```

如果你还要用 spidev 手动测试 SPI，再推：

```bash
adb push module_driver/output/spidev_helper.ko /tmp/kernel_drv_mode/
adb push module_driver/output/spidev.ko /tmp/kernel_drv_mode/
```

---

# 8. 手动加载顺序

板子上：

```sh
cd /tmp/kernel_drv_mode
```

先卸载旧模块：

```sh
rmmod bmi160_spi_board 2>/dev/null
rmmod bmi160_spi 2>/dev/null
rmmod bmi160_core 2>/dev/null
rmmod industrialio_triggered_buffer 2>/dev/null
rmmod kfifo_buf 2>/dev/null
rmmod soc_spi 2>/dev/null
```

先加载 SPI0 master：

```sh
dmesg -c > /dev/null

insmod utils.ko 2>/dev/null

insmod ./soc_spi.ko spi0_is_enable=1 spi1_is_enable=0 div_ssi_rate=10000000 spi0_clk=PD00 spi0_mosi=PD01 spi0_miso=PD02

dmesg | tail -100
ls -l /sys/class/spi_master/
```

必须看到：

```text
spi0
```

然后加载 BMI160：

```sh
dmesg -c > /dev/null

insmod kfifo_buf.ko
insmod industrialio-triggered-buffer.ko

insmod bmi160_core.ko
insmod bmi160_spi.ko
insmod bmi160_spi_board.ko

dmesg | tail -100
ls -l /sys/bus/spi/devices/
ls -l /sys/bus/iio/devices/
```

成功后应该看到：

```text
/sys/bus/spi/devices/spi0.0
/sys/bus/iio/devices/iio:device0
```

---

# 9. 你可以写一个加载脚本

板子上创建：

```sh
vi /tmp/load_bmi160.sh
```

内容：

```sh
#!/bin/sh

cd /tmp/kernel_drv_mode || exit 1

echo "===== unload old modules ====="
rmmod bmi160_spi_board 2>/dev/null
rmmod bmi160_spi 2>/dev/null
rmmod bmi160_core 2>/dev/null
rmmod industrialio_triggered_buffer 2>/dev/null
rmmod kfifo_buf 2>/dev/null
rmmod soc_spi 2>/dev/null

echo "===== clear dmesg ====="
dmesg -c > /dev/null

echo "===== load x2600 spi0 master ====="
insmod utils.ko 2>/dev/null

insmod ./soc_spi.ko spi0_is_enable=1 spi1_is_enable=0 div_ssi_rate=10000000 spi0_clk=PD00 spi0_mosi=PD01 spi0_miso=PD02

echo "===== spi master ====="
ls -l /sys/class/spi_master/

echo "===== load bmi160 ====="
insmod kfifo_buf.ko
insmod industrialio-triggered-buffer.ko
insmod bmi160_core.ko
insmod bmi160_spi.ko
insmod bmi160_spi_board.ko

echo "===== dmesg ====="
dmesg | tail -100

echo "===== spi devices ====="
ls -l /sys/bus/spi/devices/

echo "===== iio devices ====="
ls -l /sys/bus/iio/devices/
```

执行：

```sh
chmod +x /tmp/load_bmi160.sh
/tmp/load_bmi160.sh
```

---

# 10. 如何判断缺依赖？

如果 `insmod xxx.ko` 报：

```text
unknown symbol in module
```

马上看：

```sh
dmesg | tail -100
```

比如你之前遇到：

```text
bmi160_core: Unknown symbol devm_iio_triggered_buffer_setup
```

说明缺：

```text
industrialio-triggered-buffer.ko
```

比如：

```text
soc_spi: Unknown symbol gpio_set_func
soc_spi: Unknown symbol param_gpio_ops
```

说明缺：

```text
utils.ko
```

在 PC 上可以查模块依赖：

```bash
cd ~/project/x2600

modinfo module_driver/output/soc_spi.ko
modinfo kernel/kernel/drivers/iio/imu/bmi160/bmi160_core.ko
modinfo kernel/kernel/drivers/iio/imu/bmi160/bmi160_spi.ko
```

也可以查哪个 KO 提供符号：

```bash
cd ~/project/x2600

for ko in $(find module_driver kernel/kernel -name "*.ko"); do
    nm -A "$ko" 2>/dev/null | grep "gpio_set_func"
done
```

---

# 11. 能不能用 modprobe 自动加载依赖？

理论上可以，但你现在板子上没有完整：

```text
/lib/modules/$(uname -r)/modules.dep
```

所以你之前看到：

```text
modinfo: can't open '/lib/modules/5.10.186/modules.dep'
modprobe: can't change directory to '/lib/modules'
```

这说明目标系统现在不适合直接 `modprobe`。

调试阶段建议继续用：

```sh
insmod A.ko
insmod B.ko
insmod C.ko
```

等正式集成时，再考虑把 KO 放到：

```text
/lib/modules/$(uname -r)/
```

然后生成 `modules.dep`，用 `modprobe`。

---

# 12. 正式集成时可以这样做

PC 端安装内核模块到 rootfs：

```bash
cd ~/project/x2600/kernel/kernel

make ARCH=mips modules_install INSTALL_MOD_PATH=/home/vik/project/x2600/buildroot/buildroot24/output/target
```

然后把君正模块放进 rootfs：

```bash
cd ~/project/x2600

cp -v module_driver/output/utils.ko buildroot/buildroot24/output/target/module_driver/
cp -v module_driver/output/soc_spi.ko buildroot/buildroot24/output/target/module_driver/
cp -v module_driver/bmi160_spi_board/bmi160_spi_board.ko buildroot/buildroot24/output/target/module_driver/
```

再写启动脚本，顺序按手动加载那套。

---

# 13. 你现在这个阶段的关键结论

你想要的“KO 模块方式”就是：

```text
不要把 BMI160 编进 kernel
CONFIG_BMI160=m
CONFIG_BMI160_SPI=m
编出 bmi160_core.ko / bmi160_spi.ko
运行时手动 insmod 依赖
```

加载顺序必须是：

```text
utils.ko
soc_spi.ko 带 SPI0 参数

kfifo_buf.ko
industrialio-triggered-buffer.ko
bmi160_core.ko
bmi160_spi.ko
bmi160_spi_board.ko
```

你现在 `Wrong chip id, got 0 expected d1` 已经不是 KO 依赖问题，而是 SPI 真实通信问题。等你用 `cmd_spi` 或小测试程序能读到 `0xD1`，这套 KO 加载链路就能继续生成 IIO 设备。
