看这个结果：

```bash
ls -l /sys/class/spi_master/
ls -l /sys/bus/spi/devices/
```

两个都是空的，说明现在系统里**一个普通 SPI master 都没有注册**。

所以现在结论很明确：

```text
BMI160 依赖模块：已经 OK
bmi160_core.ko：已经 OK
bmi160_spi.ko：已经 OK
bmi160_spi_board.ko：失败，因为系统没有 spi0
```

你现在的 `bmi160_spi_board.ko` 失败点就是这里：

```c
master = spi_busnum_to_master(0);
```

它要找：

```text
/sys/class/spi_master/spi0
```

但是现在根本没有 `spi0`，所以报：

```text
failed to get spi master 0
```

---

## 现在不要继续 insmod board 模块了

在 SPI master 没出来之前，这个命令一定失败：

```bash
insmod bmi160_spi_board.ko
```

board 模块只能做这件事：

```text
在已有的 spi0/spi1/spi2 上挂一个 bmi160 设备
```

它不能凭空创建 SPI 控制器。

现在缺的是这个：

```text
X2600N 普通 SPI 控制器驱动/板级配置
```

---

## 板子上再查这几个

板子执行：

```bash
cat /proc/devices | grep -i spi

find /sys -name "*spi*" 2>/dev/null

lsmod | grep -i spi
```

再看设备树里有没有 SPI 节点：

```bash
find /proc/device-tree -iname "*spi*" -o -iname "*ssi*" 2>/dev/null
```

如果这些都基本没有普通 SPI 信息，就说明当前烧进去的 kernel/device tree 没启用普通 SPI 控制器。

---

## PC 上查内核有没有 SPI master 驱动

在 PC 上执行：

```bash
cd ~/project/x2600

find kernel/kernel/drivers/spi -name "*.ko"
```

再查配置：

```bash
cd ~/project/x2600/kernel/kernel

grep -E "CONFIG_SPI|CONFIG_SPI_MASTER|CONFIG_SPI_.*INGENIC|CONFIG_SPI_.*JZ|CONFIG_SPI_.*XBURST|CONFIG_SPI_BITBANG" .config
```

如果只有：

```text
CONFIG_SPI=y
CONFIG_SPI_MASTER=y
```

但是没有君正普通 SPI 控制器相关配置，那还不够。

再全工程搜一下：

```bash
cd ~/project/x2600

grep -R "SPI0\|SPI1\|SPI2\|MD_SPI\|MD_X2600.*SPI\|APP_.*SPI" \
    -n build/configs build/.config.in kernel/kernel/.config module_driver 2>/dev/null | head -200
```

这个是为了确认 IConfigTool 里到底有没有把普通 SPI0 打开。

---

## 关键点：SFC 不是普通 SPI

你之前日志里有过：

```text
ingenic-sfc 13440000.sfc: SPI NOR MTD LOAD OK
```

这个是 SPI NOR Flash 控制器，叫 SFC。

它不是：

```text
/sys/class/spi_master/spi0
```

BMI160 不能挂在 SFC 上。BMI160 需要普通 SPI master，比如：

```text
spi0
spi1
spi2
```

所以现在你要打开的是普通 SPI，不是 SFC。

---

## 你现在大概率需要回 IConfigTool 开 SPI0

你 board 代码写的是：

```c
static int spi_bus_num = 0;
```

所以你需要让系统启动后出现：

```bash
ls /sys/class/spi_master/
```

结果里面有：

```text
spi0
```

如果 IConfigTool 里有类似这些选项，要打开：

```text
SPI
SPI0
X2600 SPI0
SPI Device
SPI Controller
```

然后重新：

```bash
cd ~/project/x2600/build
make kernel
```

如果 SPI 控制器是编进 kernel 的，还要重新烧录 kernel。

---

## 如果普通 SPI 是 KO 模块

如果你执行：

```bash
find kernel/kernel/drivers/spi -name "*.ko"
```

能找到类似：

```text
spi-xxx.ko
jz-spi.ko
ingenic-spi.ko
```

那就先把这个 SPI master KO 推到板子上加载：

```bash
adb push 找到的SPI_MASTER.ko /tmp/kernel_drv_mode/
```

板子上：

```bash
cd /tmp/kernel_drv_mode

insmod SPI_MASTER.ko

ls /sys/class/spi_master/
```

如果这时出现：

```text
spi0
```

再执行：

```bash
insmod bmi160_spi_board.ko
```

---

## 建议你先把 board 模块改成可传参

现在写死：

```c
static int spi_bus_num = 0;
```

建议改成：

```c
static int spi_bus_num = 0;
module_param(spi_bus_num, int, 0644);
MODULE_PARM_DESC(spi_bus_num, "SPI bus number for BMI160");
```

这样后面如果系统出现的是 `spi1`，不用改代码，直接：

```bash
insmod bmi160_spi_board.ko spi_bus_num=1
```

如果是 `spi2`：

```bash
insmod bmi160_spi_board.ko spi_bus_num=2
```

---

## 当前下一步

你现在先执行 PC 端这两组：

```bash
cd ~/project/x2600

find kernel/kernel/drivers/spi -name "*.ko"
```

```bash
cd ~/project/x2600/kernel/kernel

grep -E "CONFIG_SPI|CONFIG_SPI_MASTER|CONFIG_SPI_.*INGENIC|CONFIG_SPI_.*JZ|CONFIG_SPI_.*XBURST|CONFIG_SPI_BITBANG" .config
```

再执行：

```bash
cd ~/project/x2600

grep -R "SPI0\|SPI1\|SPI2\|MD_SPI\|MD_X2600.*SPI\|APP_.*SPI" \
    -n build/configs build/.config.in kernel/kernel/.config module_driver 2>/dev/null | head -200
```

看完这个就能判断：是 SPI master 驱动没编，还是 IConfigTool 没开 SPI0，还是开了但没重新烧 kernel。


-------
-------
-------
你这个信息已经很关键了：**不是 BMI160 驱动的问题了，是 X2600N 普通 SPI 控制器没有起来。**

现在状态是：

```text id="quug5h"
bmi160_core.ko      已加载
bmi160_spi.ko       已加载
bmi160_spi_board.ko 失败：failed to get spi master 0
```

你板子上：

```bash id="jswm8i"
ls -l /sys/class/spi_master/
ls -l /sys/bus/spi/devices/
```

都是空的；`find /sys -name "*spi*"` 只看到 device-tree 里的 `spi@0x10043000`、`spi@0x10044000`，但没有真正注册出来的 `spi0/spi1` master。这说明 **设备树里有 SPI 节点描述，但运行时 SPI master 驱动没有绑定成功/没有启用**。

你 PC 端 `.config` 只有：

```text id="o82t1s"
CONFIG_SPI=y
CONFIG_SPI_MASTER=y
CONFIG_SPI_BITBANG=y
```

但是没有看到君正 X2600 SPI 控制器驱动对应的内核项，而且：

```bash id="uui5qg"
find kernel/kernel/drivers/spi -name "*.ko"
```

没有任何结果。结合你 grep 出来的工程配置，很多 X2600 配置里都是：

```text id="vfic38"
# MD_X2600_510_SPI is not set
# MD_X2600_510_SPI0 is not set
# MD_X2600_510_SPI1 is not set
# MD_SPI_DEVICE_HELPER is not set
# MD_SPI_DEVICE is not set
```

这些就非常像问题根因。你上传的配置搜索结果里，X2600 相关 defconfig 多处显示 `MD_X2600_510_SPI`、`MD_X2600_510_SPI0`、`MD_X2600_510_SPI1` 未启用。

---

## 1. 你现在缺的是这个链路

你现在已经有：

```text id="z7w5w3"
Linux SPI core：CONFIG_SPI=y
BMI160 SPI driver：bmi160_spi.ko
BMI160 core driver：bmi160_core.ko
```

但缺：

```text id="1a2hfl"
X2600N 普通 SPI master controller driver
```

所以系统里没有：

```text id="2l69zu"
/sys/class/spi_master/spi0
```

你的 `bmi160_spi_board.ko` 只是做这个事情：

```c id="h6vq5k"
master = spi_busnum_to_master(0);
```

它需要系统里已经存在 `spi0`。如果 `spi0` 不存在，它只能失败。

---

## 2. 设备树里有 SPI 节点，不代表 SPI 已经能用

你板子上能看到：

```text id="lxaxq7"
/sys/firmware/devicetree/base/aliases/spi0
/sys/firmware/devicetree/base/aliases/spi1
/sys/firmware/devicetree/base/apb/spi@0x10044000
/sys/firmware/devicetree/base/apb/spi@0x10043000
```

这只能说明：**dtb 里面有 SPI 节点**。

但真正能用，要满足：

```text id="p0wl8v"
1. SPI 节点 status = "okay"
2. 对应 compatible 的 SPI controller driver 被编进内核或模块
3. pinctrl 选对，例如 spi0-pd / spi0-pb
4. 驱动 probe 成功
5. /sys/class/spi_master/spi0 出现
```

现在第 5 步没有，所以前面某一步没通。

你先在板子上查这两个 SPI 节点状态：

```bash id="fy24h0"
echo "===== aliases ====="
for f in /sys/firmware/devicetree/base/aliases/spi0 \
         /sys/firmware/devicetree/base/aliases/spi1
do
    echo "$f:"
    strings "$f" 2>/dev/null
done

echo "===== spi nodes ====="
for n in /sys/firmware/devicetree/base/apb/spi@0x10043000 \
         /sys/firmware/devicetree/base/apb/spi@0x10044000
do
    echo
    echo "NODE=$n"
    echo -n "compatible: "
    strings "$n/compatible" 2>/dev/null
    echo -n "status: "
    strings "$n/status" 2>/dev/null || echo "no status"
done
```

如果看到：

```text id="rnhzgy"
status: disabled
```

那就很明确：SPI 节点被禁用了。

---

## 3. 按君正文档，应该先在 IConfigTool 打开 SPI 总线

你发的君正 SPI 文档说得很明确：进入 IConfigTool 选择对应板级配置，然后到 `[模块化驱动] -> [驱动]`，根据设备使用的是 **SPI 控制器驱动** 还是 **GPIO_SPI 驱动** 选择配置；如果 SPI 控制器驱动不能正常使用，可以用 GPIO_SPI 做验证。

所以你现在应该优先走：

```text id="p3d1dc"
IConfigTool
  -> 模块化驱动
    -> 驱动
      -> SPI总线
        -> 打开 X2600 SPI
        -> 打开 SPI0 或 SPI1
        -> 配置 SPI 引脚组
```

按你的需求，应该至少要有类似配置：

```text id="xi07d6"
MD_X2600_510_SPI=y
MD_X2600_510_SPI0=y
```

如果你实际接的是 SPI1，那就是：

```text id="zh41lu"
MD_X2600_510_SPI=y
MD_X2600_510_SPI1=y
```

如果你还想用 `cmd_spi` 做用户态测试，再打开：

```text id="kfh6k9"
MD_SPI_DEVICE_HELPER=y
MD_SPI_DEVICE=y
```

你现在 grep 的结果里，这些基本都是 `not set`，所以 `spi0` 起不来很合理。

---

## 4. 注意：你要配置的是 SPI0 的 CLK/MOSI/MISO，不只是 PD05 CS

你现在一直说 CS 是 `PD05`，这个只是片选。

BMI160 SPI 至少还需要：

```text id="c8eel0"
SCLK
MOSI
MISO
CS
```

你的 `bmi160_spi_board.c` 里：

```c id="cx9kl3"
.controller_data = (void *)GPIO_PD(5),
```

这个只是告诉 SPI 控制器片选 GPIO 用 `PD05`。

但是 SPI 控制器本身的 SCLK/MOSI/MISO 需要通过 IConfigTool / pinctrl 配好。你板子上能看到 pinctrl 里有：

```text id="jwfdol"
spi0-pd
spi0-pb
spi1-pc
spi1-pc-2
```

所以你要确认你的硬件实际接线到底是：

```text id="su43ze"
SPI0-PD 这一组？
SPI0-PB 这一组？
SPI1-PC 这一组？
```

如果你只配置 CS=PD05，但 SCLK/MOSI/MISO 选错组，后面即使 `spi0` 出来了，也会读不到 BMI160。

---

## 5. 修改方向：先让 `/sys/class/spi_master/spi0` 出现

你的第一目标不是马上加载 BMI160，而是先让这个命令有输出：

```bash id="gin29x"
ls /sys/class/spi_master/
```

理想看到：

```text id="uwhm41"
spi0
```

或者：

```text id="vcv547"
spi1
```

你应该这样做：

```bash id="plw8kq"
cd ~/project/x2600/build

# 进入君正配置工具，打开 X2600 SPI0
# 具体命令看你工程，一般可能是 make menuconfig / make config / IConfigTool 图形界面
```

配置完成后，重新编译：

```bash id="b0p7us"
cd ~/project/x2600/build

make kernel
make module_driver
```

如果 SPI master 是编进 kernel 的，**必须重新烧录 kernel/dtb**。只推 `.ko` 不够。

如果 SPI master 是模块形式，编译完以后先找：

```bash id="a4he0b"
cd ~/project/x2600

find module_driver -name "*spi*.ko"
find kernel/kernel -name "*spi*.ko"
```

如果出现类似：

```text id="mxbx4b"
xxx_spi.ko
spi_device.ko
spi_device_helper.ko
```

就先推到板子加载它们。

---

## 6. 如果你想先用君正官方 `cmd_spi` 验证 SPI 总线

君正文档里提供了 `cmd_spi add_dev` 的测试方式，例如：

```bash id="zmubz1"
cmd_spi add_dev 0 pc16
/dev/spidev0.0
```

然后可以 `cmd_spi transfer` 测试传输。文档里也说明 `cmd_spi add_dev 0 pc16` 里的 `0` 是 SPI 总线号，`pc16` 是片选引脚，并会生成 `/dev/spidev0.0`。

你的 CS 是 PD05，所以后面可以试：

```bash id="b05gtw"
cmd_spi add_dev 0 pd05
```

如果成功，应该生成：

```bash id="irwq28"
/dev/spidev0.0
```

但前提还是：**系统里必须已经有 spi0 master**。如果 `/sys/class/spi_master/` 还是空的，`cmd_spi add_dev 0 pd05` 大概率也会失败。

---

## 7. 你的 board 模块建议改成君正文档那种封装接口

君正文档给了一个 `spi_register_device()` 封装，本质也是：

```c id="c2e3dv"
master = spi_busnum_to_master(spi_bus_num);
dev = spi_new_device(master, info);
```

并且 `controller_data` 用来配置 GPIO CS。

你当前代码逻辑基本等价，但可以整理成更像官方示例的写法：

```c id="h07ob7"
static int spi_bus_num = 0;
module_param(spi_bus_num, int, 0644);
MODULE_PARM_DESC(spi_bus_num, "SPI bus number for BMI160");

static struct spi_device *bmi160_spi_dev;

static struct spi_board_info bmi160_spi_board_info = {
    .modalias = "bmi160",
    .max_speed_hz = 1 * 1000 * 1000,
    .mode = SPI_MODE_0,
    .controller_data = (void *)GPIO_PD(5),
    .chip_select = 0,
};

static struct spi_device *bmi160_register_spi_device(struct spi_board_info *info,
                                                     int bus_num)
{
    struct spi_master *master;
    struct spi_device *dev;

    master = spi_busnum_to_master(bus_num);
    if (!master) {
        pr_err("bmi160_spi_board: failed to get spi master %d\n", bus_num);
        return NULL;
    }

    info->bus_num = bus_num;

    dev = spi_new_device(master, info);
    if (!dev) {
        pr_err("bmi160_spi_board: failed to create spi device on bus %d\n",
               bus_num);
    }

    spi_master_put(master);
    return dev;
}
```

然后 init 里：

```c id="smiu6d"
bmi160_spi_dev = bmi160_register_spi_device(&bmi160_spi_board_info,
                                            spi_bus_num);
if (!bmi160_spi_dev)
    return -ENODEV;
```

但是注意：**这不能解决当前问题**。当前问题不是你 board 模块写法错，而是 `spi_busnum_to_master(0)` 找不到 `spi0`。

---

## 8. 最推荐你现在按这个顺序走

### 第一步：确认 DT 节点状态

板子执行：

```bash id="vhmobc"
for n in /sys/firmware/devicetree/base/apb/spi@0x10043000 \
         /sys/firmware/devicetree/base/apb/spi@0x10044000
do
    echo
    echo "NODE=$n"
    echo -n "compatible: "
    strings "$n/compatible" 2>/dev/null
    echo -n "status: "
    strings "$n/status" 2>/dev/null || echo "no status"
done
```

### 第二步：IConfigTool 打开 X2600 SPI0

目标配置应该类似：

```text id="f8cvxi"
MD_X2600_510_SPI=y
MD_X2600_510_SPI0=y
MD_X2600_510_SPI_CLK_RATE=100000000
```

如果要用 `cmd_spi` 辅助测试：

```text id="luimak"
MD_SPI_DEVICE_HELPER=y
MD_SPI_DEVICE=y
```

### 第三步：重新编译并烧录 kernel/dtb

```bash id="at7miz"
cd ~/project/x2600/build

make kernel
make module_driver
```

如果你的 SPI 配置影响 dtb 或内核，必须重新烧录对应镜像。

### 第四步：启动后先确认 SPI master

板子：

```bash id="f9i96h"
ls /sys/class/spi_master/
dmesg | grep -iE "spi|ssi|x2600"
```

目标：

```text id="a6apms"
spi0
```

### 第五步：再加载 BMI160

```bash id="fwxkja"
cd /tmp/kernel_drv_mode

insmod kfifo_buf.ko
insmod industrialio-triggered-buffer.ko
insmod bmi160_core.ko
insmod bmi160_spi.ko
insmod bmi160_spi_board.ko

dmesg | tail -100
```

如果成功，应该会继续出现：

```bash id="oztjw3"
ls /sys/bus/iio/devices/
```

看到：

```text id="q3bih5"
iio:device0
```

---

## 结论

你现在的问题可以一句话概括：

```text id="sbqos1"
BMI160 驱动已经加载通了，但 X2600N 的普通 SPI0 master 没有注册，所以 bmi160_spi_board.ko 无法把 BMI160 挂到 spi0.0 上。
```

下一步不要继续折腾 BMI160，先专门把 SPI0 调出来：

```bash id="y6b5di"
ls /sys/class/spi_master/
```

这个目录里出现 `spi0` 以后，`bmi160_spi_board.ko` 才有继续调试的意义。

-------
-------
-------
--------
-------

==========

========

======

对，你理解得基本对：**IConfig / 内核配置 / 烧录是第一步，文档下面那段代码是“怎么往 SPI 总线上挂设备”的示例。**

但是重点来了：

```text
文档里的 m_spi_dev.c = 示例 SPI 设备驱动
你的 BMI160 = 已经有现成设备驱动 bmi160_spi.ko
```

所以你**不要照抄文档再写一个完整 BMI160 SPI 驱动**。你的情况只需要完成这件事：

```text
把 BMI160 这个设备注册到 spi0 上
让 bmi160_spi.ko 自动匹配并 probe
```

这就是你现在写的：

```text
bmi160_spi_board.ko
```

它和文档下边代码的关系是：**文档示例里同时写了“SPI 设备驱动 + SPI 设备注册”；而你这里只需要“SPI 设备注册”这一半。**

---

## 1. 文档代码和 BMI160 的对应关系

君正文档第 2 章讲的是 SPI 设备驱动层。它给的接口核心是：

```c
spi_busnum_to_master(spi_bus_num);
spi_new_device(master, info);
```

也就是把一个 SPI 设备挂到某条 SPI 总线上。文档示例里还自己写了 `spi_register_driver()` 和 `probe()`，用于做一个测试驱动。

但你现在已经有：

```text
bmi160_core.ko
bmi160_spi.ko
```

其中：

```text
bmi160_spi.ko 就是 SPI 设备驱动
```

所以你不需要再写：

```c
static struct spi_driver m_spi_driver = { ... };
spi_register_driver(&m_spi_driver);
```

因为这个工作已经由：

```bash
insmod bmi160_spi.ko
```

完成了。

你只需要写：

```c
struct spi_board_info bmi160_spi_board_info = {
    .modalias = "bmi160",
    .max_speed_hz = 1000000,
    .mode = SPI_MODE_0,
    .controller_data = (void *)GPIO_PD(5),
    .chip_select = 0,
};
```

然后：

```c
spi_new_device(master, &bmi160_spi_board_info);
```

这就是 `bmi160_spi_board.ko` 的意义。

---

## 2. 你现在 IConfig 配置看起来是对的

你贴的 diff 里已经打开了：

```text
MD_SPI_DEVICE_HELPER=y
MD_SPI_DEVICE:=y
MD_X2600_510_SPI:=y
MD_X2600_510_SPI0=y
MD_X2600_510_SPI0_CLK=PD00
MD_X2600_510_SPI0_MISO=PD02
MD_X2600_510_SPI0_MOSI=PD01
```

这说明你在 IConfig 里已经启用了 SPI 设备辅助模块、SPI 设备模块，以及 X2600 SPI0 总线配置。

但是注意：这些是 **MD_ 开头的 module_driver 配置**，不是普通 Linux 内核 `.config` 里的 `CONFIG_`。所以你光 `make kernel` 不一定够，必须还要：

```bash
make module_driver
```

然后确认 SPI 相关的君正 KO 有没有出来。

---

## 3. 你现在应该找的是君正 SPI 控制器/辅助模块 KO

在 PC 上执行：

```bash
cd ~/project/x2600

echo "===== module_driver spi ko ====="
find module_driver -name "*spi*.ko"

echo "===== output spi ko ====="
find . -path "*output*" -name "*spi*.ko"

echo "===== all spi ko ====="
find . -name "*spi*.ko"
```

你要重点找类似这些名字的 KO：

```text
spi_device.ko
spi_device_helper.ko
x2600_spi.ko
ingenic_spi.ko
spi_gpio.ko
```

名字不一定完全一样，先全部找出来。

因为你之前：

```bash
find kernel/kernel/drivers/spi -name "*.ko"
```

没有结果，这很正常。君正这个 SPI 很可能不是走 `kernel/kernel/drivers/spi/*.ko`，而是走他们自己的：

```text
module_driver
```

也就是 `MD_X2600_510_SPI` 这一套。

---

## 4. 你现在板子上没有 spi0，说明 SPI 控制器模块没起来

你之前板子上查：

```bash
ls /sys/class/spi_master/
```

是空的。

这说明当前运行系统里还没有：

```text
spi0
```

而你的 board 模块失败：

```text
bmi160_spi_board: failed to get spi master 0
```

正是因为：

```c
spi_busnum_to_master(0)
```

找不到 `spi0`。

所以现在要先让这个命令有输出：

```bash
ls /sys/class/spi_master/
```

理想结果：

```text
spi0
```

在这个之前，不要继续纠结 BMI160。

---

## 5. 先按君正文档用 cmd_spi 验证 SPI0

文档第 3 章提供了应用层测试方式：

```bash
cmd_spi add_dev 0 pc16
cmd_spi transfer /dev/spidev0.0 0x00 0x01 0x02 0x03 0x04
cmd_spi set /dev/spidev0.0 mode=0x03 speed=500000 bits=8 lsb=0
cmd_spi info /dev/spidev0.0
cmd_spi del_dev /dev/spidev0.0
```

文档里 `cmd_spi add_dev 0 pc16` 的意思是：在 SPI 总线 0 上，用 `pc16` 作为 CS，动态添加 `/dev/spidev0.0`。

你这里 CS 是 PD05，所以可以试：

```bash
cmd_spi add_dev 0 pd05
```

但是前提是系统里有 `cmd_spi` 和 SPI helper 模块。

板子上先查：

```bash
which cmd_spi
ls /usr/bin/cmd_spi
ls /bin/cmd_spi
```

如果有，执行：

```bash
cmd_spi add_dev 0 pd05
```

如果成功，应该看到类似：

```text
/dev/spidev0.0
```

然后：

```bash
ls /sys/class/spi_master/
ls /sys/bus/spi/devices/
ls /dev/spidev*
```

如果这一步都失败，说明 SPI0 master 还是没起来，或者 helper 模块没加载。

---

## 6. 正确调试顺序应该是这样

### 第一步：重新编译 module_driver

PC：

```bash
cd ~/project/x2600/build

make module_driver
```

然后找 SPI 相关 KO：

```bash
cd ~/project/x2600

find module_driver -name "*spi*.ko"
find . -path "*output*" -name "*spi*.ko"
```

---

### 第二步：把君正 SPI 相关 KO 推到板子

假设你找到了：

```text
spi_device_helper.ko
spi_device.ko
x2600_spi.ko
```

就推：

```bash
adb push 路径/x2600_spi.ko /tmp/kernel_drv_mode/
adb push 路径/spi_device_helper.ko /tmp/kernel_drv_mode/
adb push 路径/spi_device.ko /tmp/kernel_drv_mode/
```

具体名字以你 `find` 出来的为准。

---

### 第三步：先加载 SPI 控制器/辅助模块

板子：

```bash
cd /tmp/kernel_drv_mode

dmesg -c > /dev/null

insmod x2600_spi.ko
insmod spi_device_helper.ko
insmod spi_device.ko

dmesg | tail -100
ls /sys/class/spi_master/
```

目标是看到：

```text
spi0
```

如果 `x2600_spi.ko` 名字不是这个，就换成你实际找到的 KO。

---

### 第四步：再加载 BMI160

等 `spi0` 出现以后：

```bash
cd /tmp/kernel_drv_mode

insmod kfifo_buf.ko
insmod industrialio-triggered-buffer.ko
insmod bmi160_core.ko
insmod bmi160_spi.ko
insmod bmi160_spi_board.ko

dmesg | tail -100
```

成功后看：

```bash
ls /sys/bus/iio/devices/
cat /sys/bus/iio/devices/iio:device0/name
```

---

## 7. 你的 `bmi160_spi_board.c` 应该怎么对应文档

你现在的 `bmi160_spi_board.c` 不需要写文档里完整的 `m_spi_driver`。推荐改成这种更贴近君正文档的版本：

```c
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/spi/spi.h>
#include <linux/errno.h>
#include <soc/gpio.h>

static int spi_bus_num = 0;
module_param(spi_bus_num, int, 0644);
MODULE_PARM_DESC(spi_bus_num, "SPI bus number for BMI160");

static struct spi_device *bmi160_spi_dev;

static struct spi_board_info bmi160_spi_board_info = {
    .modalias = "bmi160",
    .max_speed_hz = 1 * 1000 * 1000,
    .mode = SPI_MODE_0,
    .controller_data = (void *)GPIO_PD(5),
    .chip_select = 0,
};

static struct spi_device *bmi160_register_spi_device(struct spi_board_info *info,
                                                     int bus_num)
{
    struct spi_master *master;
    struct spi_device *dev;

    master = spi_busnum_to_master(bus_num);
    if (!master) {
        pr_err("bmi160_spi_board: failed to get spi master %d\n", bus_num);
        return NULL;
    }

    info->bus_num = bus_num;

    dev = spi_new_device(master, info);
    if (!dev) {
        pr_err("bmi160_spi_board: failed to create spi device on bus %d\n",
               bus_num);
    }

    spi_master_put(master);

    return dev;
}

static int __init bmi160_spi_board_init(void)
{
    pr_info("bmi160_spi_board: init, bus=%d, cs=PD05\n", spi_bus_num);

    bmi160_spi_dev = bmi160_register_spi_device(&bmi160_spi_board_info,
                                                spi_bus_num);
    if (!bmi160_spi_dev)
        return -ENODEV;

    pr_info("bmi160_spi_board: registered bmi160 on spi%d.%d, cs=PD05\n",
            spi_bus_num, bmi160_spi_board_info.chip_select);

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

MODULE_DESCRIPTION("BMI160 SPI board registration for X2600, CS=PD05");
MODULE_LICENSE("GPL");
```

这个代码就是文档示例里“注册 SPI 设备”的那部分，只是把：

```text
m_new_spi_device
```

换成：

```text
bmi160
```

让它匹配 `bmi160_spi.ko`。

---

## 8. 它和 BMI160 SPI IMU 驱动的关系

关系是这个：

```text
bmi160_core.ko
    ↓
BMI160 通用核心驱动，负责 IIO、寄存器配置、accel/gyro 数据通道

bmi160_spi.ko
    ↓
BMI160 的 SPI 总线驱动，负责通过 SPI 读写 BMI160 寄存器

bmi160_spi_board.ko
    ↓
你自己写的板级注册模块，告诉系统：
SPI0 上有一个叫 "bmi160" 的设备
CS 是 PD05
SPI mode 是 0
速率是 1MHz
```

加载顺序：

```bash
insmod kfifo_buf.ko
insmod industrialio-triggered-buffer.ko
insmod bmi160_core.ko
insmod bmi160_spi.ko
insmod bmi160_spi_board.ko
```

其中最后一步会触发：

```text
bmi160_spi_board.ko 创建 spi0.0
        ↓
modalias = "bmi160"
        ↓
bmi160_spi.ko 匹配成功
        ↓
调用 bmi160_core_probe()
        ↓
创建 /sys/bus/iio/devices/iio:deviceX
```

---

## 9. 你现在最关键的检查命令

你现在先别继续改 BMI160，先执行：

```bash
cd ~/project/x2600

find . -name "*spi*.ko" | sort
```

再执行：

```bash
cd ~/project/x2600/build

make module_driver
```

再找一次：

```bash
cd ~/project/x2600

find . -name "*spi*.ko" | sort
```

如果这时候有新的 SPI 相关 KO 出来，把名字贴出来。

因为你当前的问题本质是：

```text
IConfig 已经配置了 MD_X2600_510_SPI0
但是板子运行后没有 spi0
```

这通常说明：

```text
1. module_driver 没重新编
2. SPI 控制器 KO 没加载/没打进 rootfs
3. 烧录的不是你刚改的那个 kernel/rootfs/dtb
4. 配置保存到了 x2600n_tiger_nor_5.10_defconfig，但当前 build 没用这个 defconfig
```

现在最该确认的就是：**`MD_X2600_510_SPI0=y` 最终有没有生成 SPI 控制器模块，并且板子有没有加载它。**


--------
--------
-------

对，现在方向已经很清楚了：**你已经把君正 SPI 模块编出来了，后面要先加载 `soc_spi.ko`，让系统出现 `spi0`，再加载 BMI160。**

你现在找到的这几个 KO 里：

```text
soc_spi.ko           君正 X2600 SPI 控制器驱动，最关键
spidev_helper.ko     君正 spidev 辅助模块，给 cmd_spi / 用户态测试用
spidev.ko            君正模块版 spidev，给 /dev/spidev0.0 用
bmi160_spi_board.ko  你自己写的 BMI160 板级注册模块
bmi160_spi.ko        Linux BMI160 SPI 驱动
bmi160_core.ko       Linux BMI160 IIO 核心驱动
```

文档里的示例其实也是这个逻辑：先有 SPI 控制器，然后通过 `spi_register_device/spi_new_device` 把一个 SPI 设备挂到某个 bus 上；文档示例还自己写了 `spi_register_driver()` 做测试驱动，但你的 BMI160 驱动已经由 `bmi160_spi.ko` 提供，所以你不用再写完整的 `m_spi_dev.c` 测试驱动。

---

## 1. 先把这几个 KO 推到板子

PC 端执行：

```bash
cd ~/project/x2600

adb shell "mkdir -p /tmp/kernel_drv_mode"

# 君正 SPI 控制器和 spidev 辅助模块
adb push module_driver/output/soc_spi.ko /tmp/kernel_drv_mode/
adb push module_driver/output/spidev_helper.ko /tmp/kernel_drv_mode/
adb push module_driver/output/spidev.ko /tmp/kernel_drv_mode/

# BMI160 相关模块
adb push kernel/kernel/drivers/iio/buffer/kfifo_buf.ko /tmp/kernel_drv_mode/
adb push kernel/kernel/drivers/iio/buffer/industrialio-triggered-buffer.ko /tmp/kernel_drv_mode/
adb push kernel/kernel/drivers/iio/imu/bmi160/bmi160_core.ko /tmp/kernel_drv_mode/
adb push kernel/kernel/drivers/iio/imu/bmi160/bmi160_spi.ko /tmp/kernel_drv_mode/
adb push module_driver/bmi160_spi_board/bmi160_spi_board.ko /tmp/kernel_drv_mode/
```

然后板子上确认：

```bash
cd /tmp/kernel_drv_mode
ls -l
```

---

## 2. 第一步只加载 `soc_spi.ko`

现在先不要急着上 BMI160。先让系统出现 `spi0`。

板子上执行：

```bash
cd /tmp/kernel_drv_mode

dmesg -c > /dev/null

insmod soc_spi.ko

dmesg | tail -100
ls -l /sys/class/spi_master/
ls -l /sys/bus/spi/devices/
```

理想结果是：

```text
/sys/class/spi_master/spi0
```

或者 `ls /sys/class/spi_master/` 能看到：

```text
spi0
```

只要 `spi0` 出来了，你之前的：

```text
bmi160_spi_board: failed to get spi master 0
```

这个问题就解决一大半了。

---

## 3. 如果 `soc_spi.ko` 加载成功，但还是没有 `spi0`

马上看日志：

```bash
dmesg | grep -iE "spi|soc|x2600|ssi|error|fail"
```

然后再看模块是否真的加载：

```bash
lsmod | grep -i spi
```

如果 `soc_spi` 没在 `lsmod` 里，说明没加载成功。

如果 `soc_spi` 在 `lsmod` 里，但 `/sys/class/spi_master/` 还是空的，说明 SPI0 配置可能没生效，或者你烧录的不是这份配置生成的系统。

你贴的 defconfig diff 里已经有：

```text
MD_X2600_510_SPI:=y
MD_X2600_510_SPI0=y
MD_X2600_510_SPI0_CLK=PD00
MD_X2600_510_SPI0_MISO=PD02
MD_X2600_510_SPI0_MOSI=PD01
MD_SPI_DEVICE_HELPER=y
MD_SPI_DEVICE:=y
```

这些配置本身看起来是对的，说明你已经在 IConfigTool 里打开了 X2600 SPI0 和 spidev helper。

---

## 4. 加载 BMI160 的正确顺序

等你确认：

```bash
ls /sys/class/spi_master/
```

里面有：

```text
spi0
```

再加载 BMI160：

```bash
cd /tmp/kernel_drv_mode

dmesg -c > /dev/null

insmod kfifo_buf.ko
insmod industrialio-triggered-buffer.ko

insmod bmi160_core.ko
insmod bmi160_spi.ko

insmod bmi160_spi_board.ko

dmesg | tail -100
```

然后检查：

```bash
lsmod | grep -E "spi|bmi160|iio|kfifo"
ls /sys/bus/spi/devices/
ls /sys/bus/iio/devices/
```

成功的话，应该能看到类似：

```text
/sys/bus/spi/devices/spi0.0
/sys/bus/iio/devices/iio:device0
```

再读：

```bash
cat /sys/bus/iio/devices/iio:device0/name
cat /sys/bus/iio/devices/iio:device0/in_accel_x_raw
cat /sys/bus/iio/devices/iio:device0/in_accel_y_raw
cat /sys/bus/iio/devices/iio:device0/in_accel_z_raw
```

---

## 5. `spidev_helper.ko` 和 `spidev.ko` 要不要加载？

**BMI160 正式驱动不需要它们。**

这两个是给文档里的 `cmd_spi` / 用户态测试用的。文档第 3 章给的 `cmd_spi add_dev 0 pc16`，本质是临时创建 `/dev/spidev0.0`，然后用用户态命令测试 SPI 收发。

所以有两条路线：

### 路线 A：直接走 BMI160 驱动

只需要：

```bash
insmod soc_spi.ko

insmod kfifo_buf.ko
insmod industrialio-triggered-buffer.ko
insmod bmi160_core.ko
insmod bmi160_spi.ko
insmod bmi160_spi_board.ko
```

### 路线 B：先用 `cmd_spi` 验证 SPI0

加载：

```bash
insmod soc_spi.ko
insmod spidev_helper.ko
insmod spidev.ko
```

然后：

```bash
cmd_spi add_dev 0 pd05
ls /dev/spidev*
cmd_spi set /dev/spidev0.0 mode=0x00 speed=1000000 bits=8 lsb=0
cmd_spi info /dev/spidev0.0
```

但是注意：如果你用 `cmd_spi add_dev 0 pd05` 创建了 `/dev/spidev0.0`，它会占用 `spi0.0`。这时你再加载 `bmi160_spi_board.ko` 也想创建 `spi0.0`，可能会冲突。

所以测试完要删除：

```bash
cmd_spi del_dev /dev/spidev0.0
```

然后再加载 BMI160。

---

## 6. 推荐你现在按这个最小流程来

板子上执行：

```bash
cd /tmp/kernel_drv_mode

# 清日志
dmesg -c > /dev/null

# 先加载君正 SPI 控制器
insmod soc_spi.ko

echo "===== dmesg after soc_spi ====="
dmesg | tail -80

echo "===== spi masters ====="
ls -l /sys/class/spi_master/

echo "===== spi devices ====="
ls -l /sys/bus/spi/devices/
```

如果这里已经能看到 `spi0`，继续：

```bash
dmesg -c > /dev/null

insmod kfifo_buf.ko
insmod industrialio-triggered-buffer.ko
insmod bmi160_core.ko
insmod bmi160_spi.ko
insmod bmi160_spi_board.ko

dmesg | tail -100

ls /sys/bus/spi/devices/
ls /sys/bus/iio/devices/
```

---

## 7. 如果 `bmi160_spi_board.ko` 还是失败

如果这次变成别的错误，比如：

```text
spi_new_device failed
```

先查是不是 `spi0.0` 已经被占了：

```bash
ls /sys/bus/spi/devices/
```

如果已经有：

```text
spi0.0
```

那你先卸载/删除 spidev 设备，或者重启后不要先执行 `cmd_spi add_dev`。

如果还是：

```text
failed to get spi master 0
```

说明 `soc_spi.ko` 没有创建 `spi0`，继续看：

```bash
dmesg | grep -iE "soc_spi|spi|ssi|x2600|fail|error"
lsmod | grep -i spi
```

---

## 8. 你现在这一步的核心结论

你已经找到关键 KO 了：

```text
module_driver/output/soc_spi.ko
```

后面不是去 `kernel/kernel/drivers/spi` 里找了。君正这个 SPI 控制器驱动在 `module_driver/soc/x2600_510/spi/soc_spi.ko`，所以板子上必须先：

```bash
insmod soc_spi.ko
```

让 `/sys/class/spi_master/spi0` 出现。

然后你的 BMI160 链路才是：

```text
soc_spi.ko
    ↓
创建 spi0 master
    ↓
bmi160_spi_board.ko
    ↓
创建 spi0.0，modalias = "bmi160"，CS = PD05
    ↓
bmi160_spi.ko
    ↓
probe BMI160
    ↓
bmi160_core.ko
    ↓
生成 IIO 设备
```

先把 `soc_spi.ko` 加载后的 `dmesg` 和 `ls /sys/class/spi_master/` 结果贴出来，就能继续判断下一步。


-------
-------
--------

这次问题很明确了：**`soc_spi.ko` 本身没有加载成功，因为它还依赖其他君正基础模块。**

日志里这几个就是关键：

```text
soc_spi: Unknown symbol param_gpio_ops
soc_spi: Unknown symbol gpio_to_str
soc_spi: Unknown symbol __assert
soc_spi: Unknown symbol gpio_set_func
```

这说明：

```text
soc_spi.ko 依赖某些 module_driver 基础 KO
但是你现在直接 insmod soc_spi.ko
insmod 不会自动加载依赖
所以 soc_spi.ko 起不来
所以 /sys/class/spi_master/ 还是空
```

这不是你 BMI160 代码的问题，也不是 SPI0 配置完全没生效。你现在已经把 `soc_spi.ko` 编出来了，说明 IConfig 的 SPI 配置已经起作用了一部分。文档里也说 SPI 控制器驱动和 SPI 设备辅助模块是在 IConfigTool 的“模块化驱动/驱动”里配置的；驱动层示例里注册设备前必须先能拿到 SPI master。

---

## 1. 现在要找谁提供这些符号

在 PC 端执行：

```bash
cd ~/project/x2600

for s in param_gpio_ops gpio_to_str __assert gpio_set_func
do
    echo
    echo "===== find symbol: $s ====="
    grep -R "$s" -n module_driver kernel/kernel 2>/dev/null | head -30
done
```

再用 `nm` 查到底哪个 `.ko` 导出了这些符号：

```bash
cd ~/project/x2600

for s in param_gpio_ops gpio_to_str __assert gpio_set_func
do
    echo
    echo "===== KO exports/contains: $s ====="
    for ko in $(find module_driver -name "*.ko")
    do
        nm -A "$ko" 2>/dev/null | grep -w "$s" | grep -v " U " && echo "    -> $ko"
    done
done
```

你大概率会找到类似这些模块之一：

```text
utils.ko
soc_utils.ko
gpio.ko
gpio_func.ko
param_gpio.ko
```

具体名字以你命令输出为准。

---

## 2. 也可以直接看 `soc_spi.ko` 依赖

PC 端：

```bash
cd ~/project/x2600

modinfo module_driver/output/soc_spi.ko
```

重点看：

```text
depends:
```

如果能看到依赖名，比如：

```text
depends: soc_utils,utils
```

那板子上就要先加载这些，再加载 `soc_spi.ko`。

---

## 3. 先把所有 module_driver/output 里的 KO 推到板子

为了省事，先全部推过去：

```bash
cd ~/project/x2600

adb shell "mkdir -p /tmp/kernel_drv_mode/module_driver"

adb push module_driver/output/*.ko /tmp/kernel_drv_mode/module_driver/
```

板子上看一下：

```bash
cd /tmp/kernel_drv_mode/module_driver
ls -l
```

---

## 4. 板子上先找基础模块

在板子上执行：

```bash
cd /tmp/kernel_drv_mode/module_driver

ls | grep -iE "utils|gpio|soc|param|common"
```

你重点找可能的基础模块，比如：

```text
utils.ko
soc_utils.ko
gpio.ko
```

然后先加载这些基础模块，再加载 `soc_spi.ko`。

比如如果目录里有这些：

```bash
insmod utils.ko
insmod soc_utils.ko
insmod soc_spi.ko
```

如果没有这些名字，就把 `ls` 输出贴出来。

---

## 5. 更稳的方式：看 `dmesg` 缺啥补啥

现在你缺：

```text
param_gpio_ops
gpio_to_str
__assert
gpio_set_func
```

找到提供它们的 KO 后，加载顺序应该类似：

```bash
cd /tmp/kernel_drv_mode/module_driver

dmesg -c > /dev/null

# 举例，实际名字以你 find/nm 找到的为准
insmod utils.ko
insmod soc_utils.ko

insmod soc_spi.ko

dmesg | tail -100
ls -l /sys/class/spi_master/
```

目标是看到：

```text
spi0
```

只要 `spi0` 出来，`bmi160_spi_board.ko` 的 `failed to get spi master 0` 就会消失。

---

## 6. 如果你想快速暴力试一下

可以先在板子上试这个小脚本，让它按顺序加载 module_driver/output 里的模块：

```bash
cd /tmp/kernel_drv_mode/module_driver

dmesg -c > /dev/null

for ko in *.ko
do
    echo "===== insmod $ko ====="
    insmod "$ko" 2>/dev/null
done

dmesg | tail -120
ls -l /sys/class/spi_master/
lsmod | grep -iE "spi|gpio|utils|soc"
```

这个方法可能会有一些模块加载失败，但没关系，目的是先把基础依赖模块加载上去。**如果最后 `/sys/class/spi_master/` 出现 `spi0`，就说明方向对了。**

---

## 7. 等 `spi0` 出现后，再加载 BMI160

假设你已经看到：

```bash
ls /sys/class/spi_master/
```

输出：

```text
spi0
```

然后再执行：

```bash
cd /tmp/kernel_drv_mode

dmesg -c > /dev/null

insmod kfifo_buf.ko
insmod industrialio-triggered-buffer.ko
insmod bmi160_core.ko
insmod bmi160_spi.ko
insmod bmi160_spi_board.ko

dmesg | tail -100
ls /sys/bus/spi/devices/
ls /sys/bus/iio/devices/
```

成功链路应该是：

```text
soc_spi.ko 加载成功
    ↓
/sys/class/spi_master/spi0 出现
    ↓
bmi160_spi_board.ko 创建 spi0.0
    ↓
bmi160_spi.ko 匹配 "bmi160"
    ↓
BMI160 probe
    ↓
/sys/bus/iio/devices/iio:device0 出现
```

---

## 8. 你现在下一步就跑这两个命令

PC 端先跑：

```bash
cd ~/project/x2600

for s in param_gpio_ops gpio_to_str __assert gpio_set_func
do
    echo
    echo "===== $s ====="
    for ko in $(find module_driver -name "*.ko")
    do
        nm -A "$ko" 2>/dev/null | grep -w "$s" | grep -v " U "
    done
done
```

再跑：

```bash
find module_driver/output -name "*.ko" -printf "%f\n" | sort
```

把这两个输出贴出来，我就能直接告诉你板子上 `insmod` 的准确顺序。


------
-------
------
不是这里不行，**是板子上的 `modinfo` 用不了本地 ko 文件**。它在找：

```text
/lib/modules/5.10.186/modules.dep
```

你这个 rootfs 里没有 `/lib/modules` 目录，所以 `modinfo soc_spi.ko` 报错很正常。**先别管 modinfo，直接 insmod 传参数测试。**

你现在直接这样做。

---

## 1. 板子上先确认当前有没有加载 soc_spi

```sh
lsmod | grep -E "soc_spi|utils"
```

如果看到 `soc_spi`，先卸载：

```sh
rmmod soc_spi
```

`utils` 不用卸载。

---

## 2. 直接重新加载 soc_spi，传 SPI0 引脚参数

在板子上执行：

```sh
cd /tmp/kernel_drv_mode

dmesg -c > /dev/null

insmod utils.ko 2>/dev/null

insmod soc_spi.ko \
spi0_is_enable=1 \
spi1_is_enable=0 \
div_ssi_rate=10000000 \
spi0_clk=PD00 \
spi0_mosi=PD01 \
spi0_miso=PD02

dmesg | tail -100
ls -l /sys/class/spi_master/
```

重点看有没有：

```text
spi0
```

如果出现 `spi0`，说明 SPI0 控制器起来了。

---

## 3. 如果还是不行，试小写参数

有些君正 GPIO 参数解析可能吃小写。再试：

```sh
rmmod soc_spi 2>/dev/null

dmesg -c > /dev/null

insmod soc_spi.ko \
spi0_is_enable=1 \
spi1_is_enable=0 \
div_ssi_rate=10000000 \
spi0_clk=pd00 \
spi0_mosi=pd01 \
spi0_miso=pd02

dmesg | tail -100
ls -l /sys/class/spi_master/
```

---

## 4. 你不用在板子上 modinfo，可以在 PC 上看

Ubuntu 上执行：

```bash
cd ~/project/x2600

modinfo module_driver/output/soc_spi.ko
```

或者：

```bash
strings module_driver/output/soc_spi.ko | grep "__param"
```

你之前已经看到：

```text
__param_spi0_clk
__param_spi0_mosi
__param_spi0_miso
```

所以 `soc_spi.ko` 明显是支持这些参数的。

---

## 5. SPI0 成功后，再加载 BMI160

只有当这个命令能看到 `spi0`：

```sh
ls /sys/class/spi_master/
```

再继续：

```sh
cd /tmp/kernel_drv_mode

dmesg -c > /dev/null

insmod kfifo_buf.ko 2>/dev/null
insmod industrialio-triggered-buffer.ko 2>/dev/null
insmod bmi160_core.ko 2>/dev/null
insmod bmi160_spi.ko 2>/dev/null
insmod bmi160_spi_board.ko

dmesg | tail -100

ls -l /sys/bus/spi/devices/
ls -l /sys/bus/iio/devices/
```

---

你现在这一步不要再纠结 `modinfo`，直接跑这个最小命令：

```sh
rmmod soc_spi 2>/dev/null
insmod soc_spi.ko spi0_is_enable=1 spi1_is_enable=0 div_ssi_rate=10000000 spi0_clk=PD00 spi0_mosi=PD01 spi0_miso=PD02
dmesg | tail -100
ls /sys/class/spi_master/
```

如果日志还是报 `PA00`，说明参数没吃进去；如果报 `PD00 invalid`，说明参数吃进去了但 PD00 这组引脚不支持 SPI0；如果出现 `spi0`，就可以继续上 BMI160。


--------
-------
-------
你这个 BMI160 驱动现在要分 **3 层解决**，不要混在一起看：

```text
第 1 层：先让 X2600 的 SPI0 master 出来
第 2 层：再把 BMI160 挂到 spi0.0 上
第 3 层：最后让 bmi160_spi.ko probe 成功，生成 IIO 设备
```

你现在卡在 **第 1 层**。`bmi160_core.ko` 和 `bmi160_spi.ko` 已经能加载了，真正没解决的是：

```text
/sys/class/spi_master/ 里面没有 spi0
```

所以 `bmi160_spi_board.ko` 才会报：

```text
failed to get spi master 0
```

君正文档里那段 `spi_busnum_to_master()` / `spi_new_device()` 示例，和你现在的 `bmi160_spi_board.ko` 是同一个逻辑：先找 SPI master，再注册 SPI device。如果 master 不存在，注册设备必然失败。

---

## 一、先解决 SPI0 master

你现在已经找到了关键模块：

```text
utils.ko
soc_spi.ko
```

而且 `soc_spi.ko` 依赖 `utils.ko`，你也已经确认缺失符号都由 `utils.ko` 提供。

现在板子上这样操作，**一行一行执行**：

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

insmod soc_spi.ko \
spi0_is_enable=1 \
spi1_is_enable=0 \
div_ssi_rate=10000000 \
spi0_clk=PD00 \
spi0_mosi=PD01 \
spi0_miso=PD02

dmesg | tail -100
ls -l /sys/class/spi_master/
```

目标是看到：

```text
spi0
```

如果看到 `spi0`，说明 SPI0 控制器已经起来了。

如果还是报：

```text
SPI spi0_clk(PA00) is invaild!
```

说明参数没有吃进去。那改用小写再试：

```sh
rmmod soc_spi 2>/dev/null
dmesg -c > /dev/null

insmod soc_spi.ko \
spi0_is_enable=1 \
spi1_is_enable=0 \
div_ssi_rate=10000000 \
spi0_clk=pd00 \
spi0_mosi=pd01 \
spi0_miso=pd02

dmesg | tail -100
ls -l /sys/class/spi_master/
```

如果变成：

```text
SPI spi0_clk(PD00) is invaild!
```

那说明参数已经传进去了，但是 **PD00 这个脚在 X2600 的 SPI0 功能表里不支持**，需要换 IConfigTool 里支持的 SPI0 引脚组，比如 `spi0-pb` 或者 `spi0-pd` 里真实支持的组合。

---

## 二、SPI0 出现后，再加载 BMI160

只有当这个命令能看到 `spi0`：

```sh
ls /sys/class/spi_master/
```

再继续加载 BMI160：

```sh
cd /tmp/kernel_drv_mode

dmesg -c > /dev/null

insmod kfifo_buf.ko 2>/dev/null
insmod industrialio-triggered-buffer.ko 2>/dev/null

insmod bmi160_core.ko 2>/dev/null
insmod bmi160_spi.ko 2>/dev/null

insmod bmi160_spi_board.ko

dmesg | tail -100

ls -l /sys/bus/spi/devices/
ls -l /sys/bus/iio/devices/
```

成功的话，应该出现：

```text
/sys/bus/spi/devices/spi0.0
/sys/bus/iio/devices/iio:device0
```

然后读数据：

```sh
cat /sys/bus/iio/devices/iio:device0/name
cat /sys/bus/iio/devices/iio:device0/in_accel_x_raw
cat /sys/bus/iio/devices/iio:device0/in_accel_y_raw
cat /sys/bus/iio/devices/iio:device0/in_accel_z_raw
```

---

## 三、你的 `bmi160_spi_board.c` 只负责注册设备，不负责 SPI 控制器

你这个 BMI160 板级模块应该保持这个思路：

```c
.modalias = "bmi160",
.max_speed_hz = 1 * 1000 * 1000,
.mode = SPI_MODE_0,
.controller_data = (void *)GPIO_PD(5),
.chip_select = 0,
```

含义是：

```text
.modalaias = "bmi160"        匹配 bmi160_spi.ko
.controller_data = PD05      BMI160 的 CS 引脚
.chip_select = 0             创建 spi0.0
spi_bus_num = 0              挂到 spi0 master 上
```

注意：`PD05` 是 **CS**，不是 SPI0 的 CLK/MOSI/MISO。你 IConfig 里配置的是：

```text
SPI0 CLK  = PD00
SPI0 MOSI = PD01
SPI0 MISO = PD02
```

你的 board 模块里配置的是：

```text
BMI160 CS = PD05
```

这两个配置分别负责不同的东西。

---

## 四、现在最可能的真实问题

结合你的日志，现在最可能是这个：

```text
soc_spi.ko 是参数化模块；
你手动 insmod 时只传了 spi0_is_enable；
没传 spi0_clk / spi0_mosi / spi0_miso；
所以 soc_spi 用默认 PA00；
PA00 不支持 SPI0；
因此 spi0 master 没注册；
因此 bmi160_spi_board.ko 找不到 spi master 0。
```

所以当前解决关键不是改 BMI160，而是让这句成功：

```sh
insmod soc_spi.ko spi0_is_enable=1 spi0_clk=PD00 spi0_mosi=PD01 spi0_miso=PD02
```

并确认：

```sh
ls /sys/class/spi_master/
```

能看到：

```text
spi0
```

---

## 五、最终完整加载顺序

等你参数确认无误后，完整顺序就是：

```sh
cd /tmp/kernel_drv_mode

dmesg -c > /dev/null

insmod utils.ko 2>/dev/null

insmod soc_spi.ko \
spi0_is_enable=1 \
spi1_is_enable=0 \
div_ssi_rate=10000000 \
spi0_clk=PD00 \
spi0_mosi=PD01 \
spi0_miso=PD02

ls /sys/class/spi_master/

insmod kfifo_buf.ko 2>/dev/null
insmod industrialio-triggered-buffer.ko 2>/dev/null
insmod bmi160_core.ko 2>/dev/null
insmod bmi160_spi.ko 2>/dev/null
insmod bmi160_spi_board.ko

dmesg | tail -100
ls /sys/bus/spi/devices/
ls /sys/bus/iio/devices/
```

现在先只看第一步结果：**`soc_spi.ko` 加载后 `/sys/class/spi_master/` 里有没有 `spi0`。**
