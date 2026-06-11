现在你驱动改好了，下一步就按“**清空旧模块 → 加载 SPI 控制器 → 打开逻辑分析仪 → 加载 SC7U22 board 模块触发 probe**”来搞。

你现在目录里已经有这些：

```sh
industrialio-triggered-buffer.ko
sc7u22_core.ko
sc7u22_spi.ko
sc7u22_spi_board.ko
soc_spi.ko
utils.ko
```

可以直接在板子上测。

---

## 1. 先卸载旧模块

注意 `rmmod` 不带 `.ko`：

```sh
cd /lib/modules/5.10.186

rmmod sc7u22_spi_board 2>/dev/null
rmmod sc7u22_spi 2>/dev/null
rmmod sc7u22_core 2>/dev/null
rmmod industrialio_triggered_buffer 2>/dev/null
rmmod soc_spi 2>/dev/null
rmmod utils 2>/dev/null
```

然后清一下内核日志，方便看新日志：

```sh
dmesg -C
```

如果 `dmesg -C` 不支持，也没事，后面用 `dmesg | tail -n 100` 看最新的。

---

## 2. 先加载君正 SPI 控制器

```sh
insmod ./utils.ko

insmod ./soc_spi.ko \
    spi0_is_enable=1 \
    spi1_is_enable=0 \
    div_ssi_rate=10000000 \
    spi0_clk=PD00 \
    spi0_mosi=PD01 \
    spi0_miso=PD02
```

然后确认 SPI0 出来了：

```sh
ls /sys/class/spi_master/
```

你要看到：

```sh
spi0
```

再确认参数：

```sh
for p in /sys/module/soc_spi/parameters/*; do
    echo "$p = $(cat $p)"
done
```

重点应该是：

```sh
spi0_is_enable = 1
spi0_clk = PD00
spi0_mosi = PD01
spi0_miso = PD02
div_ssi_rate = 10000000
```

---

## 3. 先加载 SC7U22 core 和 spi 驱动

```sh
insmod ./industrialio-triggered-buffer.ko
insmod ./sc7u22_core.ko
insmod ./sc7u22_spi.ko
```

这一步一般不会真正访问芯片。真正访问芯片是在你加载：

```sh
sc7u22_spi_board.ko
```

的时候，因为它会注册 `spi0.0`，然后触发 `sc7u22_spi_probe()`。

---

## 4. 逻辑分析仪这样接

你现在抓这 4 根就够了：

```text
PD00 -> SCLK / SPC
PD01 -> MOSI / SDI
PD02 -> MISO / SDO
PD05 -> CS
GND  -> 逻辑分析仪 GND
```

逻辑分析仪设置建议：

```text
协议：SPI
位宽：8 bit
bit order：MSB first
采样率：至少 5MHz，建议 10MHz 或 20MHz
触发：CS 下降沿
先试 Mode 0，如果波形解码不对，再试 Mode 3
```

你的驱动里现在速率是：

```text
500 kHz
```

所以逻辑分析仪采样率用 10MHz 基本够了。

---

## 5. 开始抓包，然后加载 board 模块

先在逻辑分析仪软件里点 **Start / Capture**，设置 CS 下降沿触发。

然后板子上执行：

```sh
insmod ./sc7u22_spi_board.ko
```

这一下会触发：

```text
注册 spi0.0
    ↓
匹配 sc7u22_spi
    ↓
进入 sc7u22_spi_probe()
    ↓
raw read WHO_AM_I
    ↓
regmap_read WHO_AM_I
    ↓
chip init
```

也就是说你要抓的数据就在这次 `insmod sc7u22_spi_board.ko` 期间。

---

## 6. 看新日志

加载后马上执行：

```sh
dmesg | tail -n 120
```

重点看这些：

```text
sc7u22 spi probe enter
spi mode=?
raw write_then_read WHO_AM_I reg=0x01 cmd=0x81 ...
raw full_duplex WHO_AM_I reg=0x01 cmd=0x81 ...
debug regmap_read WHO_AM_I reg=0x01 ...
Wrong chip id...
```

如果成功，最后应该能看到 `/sys/bus/iio/devices/` 出现设备：

```sh
ls /sys/bus/spi/devices/
ls /sys/bus/iio/devices/
```

正常期望：

```sh
/sys/bus/spi/devices/spi0.0
/sys/bus/iio/devices/iio:device0
```

---

## 7. 如果想重复抓包

每次要重新触发 probe，就卸载 board 模块再重新加载：

```sh
rmmod sc7u22_spi_board
```

然后逻辑分析仪重新 Start，再：

```sh
insmod ./sc7u22_spi_board.ko
```

如果 `rmmod sc7u22_spi_board` 卸不掉或者提示不存在，看一下：

```sh
lsmod
ls /sys/bus/spi/devices/
```

如果 `spi0.0` 还在，可以先把几个模块反向卸掉：

```sh
rmmod sc7u22_spi_board 2>/dev/null
rmmod sc7u22_spi 2>/dev/null
rmmod sc7u22_core 2>/dev/null
rmmod industrialio_triggered_buffer 2>/dev/null
```

然后重新加载：

```sh
insmod ./industrialio-triggered-buffer.ko
insmod ./sc7u22_core.ko
insmod ./sc7u22_spi.ko
```

再准备抓包，最后：

```sh
insmod ./sc7u22_spi_board.ko
```

---

## 8. 抓包时重点看什么

读 WHO_AM_I 地址 `0x01`，SPI 读命令应该是：

```text
0x80 | 0x01 = 0x81
```

所以 MOSI 上你应该看到：

```text
0x81 0x00
```

MISO 上你期望看到：

```text
0x?? 0x6A
```

或者根据控制器时序，可能显示成：

```text
0x6A 0x??
```

关键是：**只要 MISO 里稳定出现 0x6A，说明硬件和 SPI 模式基本 OK；如果内核还是读到 0，那就是驱动取 rx 字节位置/ regmap 读法的问题。**

---

你现在直接跑这个最小测试流程：

```sh
cd /lib/modules/5.10.186

rmmod sc7u22_spi_board 2>/dev/null
rmmod sc7u22_spi 2>/dev/null
rmmod sc7u22_core 2>/dev/null
rmmod industrialio_triggered_buffer 2>/dev/null
rmmod soc_spi 2>/dev/null
rmmod utils 2>/dev/null

dmesg -C

insmod ./utils.ko
insmod ./soc_spi.ko spi0_is_enable=1 spi1_is_enable=0 div_ssi_rate=10000000 spi0_clk=PD00 spi0_mosi=PD01 spi0_miso=PD02

ls /sys/class/spi_master/

insmod ./industrialio-triggered-buffer.ko
insmod ./sc7u22_core.ko
insmod ./sc7u22_spi.ko

# 这里先打开逻辑分析仪，开始抓包
insmod ./sc7u22_spi_board.ko

dmesg | tail -n 120
ls /sys/bus/spi/devices/
ls /sys/bus/iio/devices/
```

现在你的关键目标就是抓到 `0x81` 这个读 WHO_AM_I 命令，以及 MISO 返回里有没有 `0x6A`。

-------
-------
-------
现在可以定性了：**不是 IIO 不行，是你这个 `sc7u22_core.c` 还基本是 BMI160 的驱动框架，很多寄存器定义和初始化流程不适合 SC7U22。**

你现在看到的现象：

```text id="1ytn8n"
raw write_then_read ONLY WHO_AM_I ... id=0x6a
custom reg_read ... reg=0x01 ... val0=0x00
```

说明第一笔 SPI 读能读到 ID，但后续读不稳定/读错。这个阶段还没到 IIO 注册成功，`/sys/bus/iio/devices/` 为空是因为 `sc7u22_chip_init()` 里 `chip id` 校验失败后直接返回了。你代码里确实是在 `sc7u22_chip_init()` 成功之后才继续执行 `devm_iio_device_register()`。

---

## 最大问题：你 core 里的寄存器是错的

你代码里现在是：

```c id="5h32u8"
#define SC7U22_REG_CMD          0x7E
#define SC7U22_CMD_SOFTRESET    0xB6

#define SC7U22_CMD_ACCEL_PM_NORMAL  0x11
#define SC7U22_CMD_GYRO_PM_NORMAL   0x15
```

这个很像 BMI160 的命令寄存器和 PMU 命令，不是 SC7U22 的寄存器体系。你手册第 20 页写的是：

```text id="vmbmxr"
SOFT_RST  = 0x4A
PWR_CTRL  = 0x7D
SEG_SEL   = 0x7F
ACC_CONF  = 0x40
ACC_RANGE = 0x41
GYR_CONF  = 0x42
GYR_RANGE = 0x43
```

而且手册还明确说，配置寄存器后建议延时 1ms 再读。

所以你现在这个：

```c id="vjqix4"
regmap_write(data->regmap, SC7U22_REG_CMD, SC7U22_CMD_SOFTRESET);
```

实际是在写：

```text id="hmrxqc"
0x7E = 0xB6
```

这对 SC7U22 来说不一定是 soft reset。真正 soft reset 寄存器是 `0x4A`。

---

## 第二个问题：`SC7U22_REG_DUMMY 0x7F` 这套逻辑也不该照搬

你现在有：

```c id="lzjby3"
#define SC7U22_REG_DUMMY 0x7F
```

然后 chip init 里：

```c id="jklvmt"
if (use_spi) {
    ret = regmap_read(data->regmap, SC7U22_REG_DUMMY, &val);
}
```

这个也是从 BMI160 抄过来的“dummy read”。但在 SC7U22 手册里，`0x7F` 是 `SEG_SEL`，不是普通 dummy 寄存器。它是切换特殊寄存器页的寄存器。手册写了：访问特殊寄存器要写 `0x7F` 为 `0x83` 或 `0x8C`，访问完要写 `0x7F` 为 `0x00` 退出。

所以你现在读 `0x7F`：

```text id="n8kc2e"
custom reg_read OK: reg=0x7f cmd=0xff len=1 val0=0x80
```

这不是“dummy read”，这是在读 `SEG_SEL`。这一步没必要，而且可能干扰后续访问。

---

## 第三个问题：你的 SPI 读命令方向暂时没问题

手册第 16 页说 SPI read 是 16 个 clock，前 8bit 是读命令，后 8bit 是 slave 返回数据。WHO_AM_I 地址是 `0x01`，默认值是 `0x6A`。 

你现在：

```c id="0hqv0i"
cmd = 0x80 | 0x01;  // 0x81
```

这个从结果看是能读到 ID 的：

```text id="883n5p"
raw write_then_read ONLY WHO_AM_I reg=0x01 cmd=0x81 ret=0 id=0x6a
```

所以现在先别纠结 `0x81`，方向是对的。

---

# 现在建议你先别跑完整 IIO，先做最小 chip init

你现在目标不要一下子完整驱动。先把 `sc7u22_chip_init()` 改成最小版本：**只读 chip id，不 soft reset，不 dummy read，不开 accel/gyro。**

先改成这样：

```c id="bfbw9t"
static int sc7u22_chip_init(struct sc7u22_data *data, bool use_spi)
{
    int ret;
    unsigned int val;
    struct device *dev = regmap_get_device(data->regmap);

    ret = regulator_bulk_enable(ARRAY_SIZE(data->supplies), data->supplies);
    if (ret) {
        dev_err(dev, "Failed to enable regulators: %d\n", ret);
        return ret;
    }

    /*
     * 调试阶段先不要 soft reset。
     * 你原来的 0x7E/0xB6 是 BMI160 风格，不是 SC7U22 手册里的 SOFT_RST。
     */

    /*
     * 调试阶段也不要读 0x7F dummy。
     * SC7U22 的 0x7F 是 SEG_SEL，不是 dummy 寄存器。
     */

    ret = regmap_read(data->regmap, SC7U22_REG_CHIP_ID, &val);
    dev_info(dev, "SC7U22 chip id read ret=%d val=0x%x\n", ret, val);

    if (ret)
        goto disable_regulator;

    if (val != SC7U22_CHIP_ID_VAL) {
        dev_err(dev, "Wrong chip id, got 0x%x expected 0x%x\n",
                val, SC7U22_CHIP_ID_VAL);
        ret = -ENODEV;
        goto disable_regulator;
    }

    /*
     * 先不要打开 accel / gyro。
     * 先让 IIO 节点注册出来。
     */

    return 0;

disable_regulator:
    regulator_bulk_disable(ARRAY_SIZE(data->supplies), data->supplies);
    return ret;
}
```

这样做的目的不是最终版，而是先确认：

```text id="9edwyi"
regmap_read(0x01) 能不能稳定读出 0x6A
sc7u22_core_probe 能不能走到 devm_iio_device_register()
```

---

## 同时把 probe 里的预读全部去掉

你现在 `sc7u22_spi_probe()` 里有这些测试：

```c id="fqbe46"
sc7u22_spi_raw_read_id(spi);

direct custom_read

regmap_read(regmap, 0x01, &val);
```

建议全部先去掉。原因是你现在日志显示“第一笔读能读到 0x6A，后续读到 0”，所以不要在 probe 里把第一笔读消耗掉。

改成：

```c id="hf27il"
static int sc7u22_spi_probe(struct spi_device *spi)
{
    int ret;
    struct regmap *regmap;
    const struct spi_device_id *id = spi_get_device_id(spi);

    dev_info(&spi->dev, "vik sc7u22_spi custom regmap version 2026-06-10-02\n");
    dev_info(&spi->dev, "sc7u22 spi probe enter\n");

    spi->mode = SPI_MODE_3;
    spi->bits_per_word = 8;
    spi->max_speed_hz = 500000;

    ret = spi_setup(spi);
    if (ret) {
        dev_err(&spi->dev, "spi_setup failed: %d\n", ret);
        return ret;
    }

    dev_info(&spi->dev,
             "spi mode=%u bits=%u max_speed=%u\n",
             spi->mode, spi->bits_per_word, spi->max_speed_hz);

    regmap = devm_regmap_init(&spi->dev,
                              &sc7u22_spi_regmap_bus,
                              spi,
                              &sc7u22_regmap_config);
    if (IS_ERR(regmap)) {
        dev_err(&spi->dev, "Failed to register spi regmap: %pe\n", regmap);
        return PTR_ERR(regmap);
    }

    if (!id) {
        dev_err(&spi->dev, "spi_device_id is NULL\n");
        return -ENODEV;
    }

    return sc7u22_core_probe(&spi->dev, regmap, id->name, true);
}
```

---

## 如果这样 IIO 出来了，说明什么？

说明：

```text id="0o695d"
SPI + regmap + IIO 框架都没问题
之前失败是 core 初始化流程错了
```

然后你再一项一项加回去：

```text id="2l9sr7"
1. 正确 soft reset：写 0x4A，具体写什么值按手册来
2. 正确 PWR_CTRL：写 0x7D，打开 ACC/GYR
3. 正确 ACC_CONF / GYR_CONF
4. 正确 ACC_RANGE / GYR_RANGE
5. 最后再实现 raw 数据读取
```

---

## 你这个驱动后面还要大改

因为现在的 `sc7u22_regs[]` 很多字段还是 BMI160 思路：

```c id="e0j8wo"
.pmu_cmd_normal = SC7U22_CMD_ACCEL_PM_NORMAL,
.pmu_cmd_suspend = SC7U22_CMD_ACCEL_PM_SUSPEND,
```

但 SC7U22 手册里是通过 `PWR_CTRL 0x7D`、`ACC_CONF 0x40`、`GYR_CONF 0x42` 这类寄存器配置工作模式，不是给 `0x7E` 发 `0x11/0x15` 这种命令。手册第 12 页也说明了工作模式由 `ACC_EN`、`GYR_EN`、`ACC_FILTER_PERF`、`GYR_FILTER_PERF`、`GYR_NOISE_PERF` 这些位组合决定。

也就是说：

```text id="w7emvm"
现在这个 sc7u22_core.c 不是完整 SC7U22 驱动
它只是 BMI160 驱动模板改名，还没真正按 SC7U22 手册移植完
```

---

## 当前结论

不是 IIO 不行。真正问题是：

```text id="maqv4q"
1. SPI 第一笔读已经能读出 WHO_AM_I = 0x6A
2. 但 probe 里预读/重复读会影响后续调试判断
3. core 初始化还在用 BMI160 的 0x7E/0xB6/0x7F dummy/PMU 命令
4. SC7U22 手册里 SOFT_RST 是 0x4A，PWR_CTRL 是 0x7D，SEG_SEL 是 0x7F
```

你下一步先不要完整初始化传感器，先把 `sc7u22_chip_init()` 简化成“只读 chip id”，同时去掉 `sc7u22_spi_probe()` 里的所有预读。目标先让：

```sh id="1q8wvg"
ls /sys/bus/iio/devices/
```

能出现：

```sh id="ou29w4"
iio:device0
```

然后再按 SC7U22 手册逐个补初始化寄存器。

-------
--------
--------

可以，你这个需求应该这样做：

**继续使用 IIO 框架，但是当前阶段只做一件事：probe 时读 `WHO_AM_I`，读到 `0x6A` 后注册一个最小 IIO 设备。**

也就是：

```text
SPI 通信
  ↓
regmap_read(WHO_AM_I)
  ↓
ID 正确
  ↓
devm_iio_device_register()
  ↓
/sys/bus/iio/devices/iio:device0
```

当前先不要：

```text
soft reset
PWR_CTRL
ACC/GYR 初始化
read_raw 读加速度/陀螺仪
trigger buffer
```

你现在旧 `sc7u22_core.c` 里虽然注释了 `set_mode()`，但是 `chip_init()` 仍然在写 soft reset、读 dummy、再读 ID，这不是“只读 ID”。

---

# 一、`sc7u22_spi_board.c` 不用改

你这个 board 文件可以保持：

```c
.modalias = "sc7u22",
.max_speed_hz = 5 * 100 * 1000,
.mode = SPI_MODE_3,
.chip_select = 0,
.controller_data = (void *)GPIO_PD(5),
```

它的作用就是创建 `spi0.0`。这个没问题。

---

# 二、`sc7u22_spi.c` 改成：只创建 regmap，不预读 ID

你现在 `sc7u22_spi.c` 里还有这些调试读：

```c
sc7u22_spi_raw_read_id(spi);

direct custom_read

regmap_read(regmap, 0x01, &val);
```

这些都先去掉。原因是你前面日志已经证明：**第一笔读能读到 0x6A，后续读可能变 0x00**。所以不要在 `spi_probe()` 里提前消耗第一笔读。

把 `sc7u22_spi.c` 改成下面这种结构：

```c
// SPDX-License-Identifier: GPL-2.0
/*
 * SC7U22 SPI driver
 */

#include <linux/module.h>
#include <linux/of.h>
#include <linux/acpi.h>
#include <linux/regmap.h>
#include <linux/spi/spi.h>
#include <linux/slab.h>

#include "sc7u22.h"

/*
 * SC7U22 SPI read:
 * bit7 = 1 表示读
 * bit6~bit0 = 寄存器地址
 *
 * 读 WHO_AM_I:
 * reg = 0x01
 * cmd = 0x80 | 0x01 = 0x81
 */
static int sc7u22_spi_reg_read(void *context,
                               const void *reg_buf, size_t reg_size,
                               void *val_buf, size_t val_size)
{
    struct spi_device *spi = context;
    const u8 *reg = reg_buf;
    u8 *tx;
    u8 *rx;
    int ret;

    if (reg_size != 1 || val_size == 0)
        return -EINVAL;

    tx = kzalloc(1, GFP_KERNEL);
    if (!tx)
        return -ENOMEM;

    rx = kzalloc(val_size, GFP_KERNEL);
    if (!rx) {
        kfree(tx);
        return -ENOMEM;
    }

    tx[0] = reg[0] | 0x80;

    ret = spi_write_then_read(spi, tx, 1, rx, val_size);
    if (ret) {
        dev_err(&spi->dev,
                "reg_read failed: reg=0x%02x cmd=0x%02x ret=%d\n",
                reg[0], tx[0], ret);
        goto out;
    }

    memcpy(val_buf, rx, val_size);

    dev_info(&spi->dev,
             "reg_read: reg=0x%02x cmd=0x%02x len=%zu val0=0x%02x\n",
             reg[0], tx[0], val_size, rx[0]);

out:
    kfree(rx);
    kfree(tx);
    return ret;
}

static int sc7u22_spi_reg_write(void *context,
                                const void *data, size_t count)
{
    struct spi_device *spi = context;
    u8 *tx;
    int ret;

    if (count == 0)
        return -EINVAL;

    tx = kmemdup(data, count, GFP_KERNEL);
    if (!tx)
        return -ENOMEM;

    dev_info(&spi->dev,
             "reg_write: reg=0x%02x count=%zu\n",
             tx[0], count);

    ret = spi_write(spi, tx, count);

    kfree(tx);
    return ret;
}

static const struct regmap_bus sc7u22_spi_regmap_bus = {
    .read = sc7u22_spi_reg_read,
    .write = sc7u22_spi_reg_write,
};

static int sc7u22_spi_probe(struct spi_device *spi)
{
    int ret;
    struct regmap *regmap;
    const struct spi_device_id *id = spi_get_device_id(spi);

    dev_info(&spi->dev, "vik sc7u22_spi IIO ID-only version 2026-06-10-02\n");
    dev_info(&spi->dev, "sc7u22 spi probe enter\n");

    spi->mode = SPI_MODE_3;
    spi->bits_per_word = 8;
    spi->max_speed_hz = 500000;

    ret = spi_setup(spi);
    if (ret) {
        dev_err(&spi->dev, "spi_setup failed: %d\n", ret);
        return ret;
    }

    dev_info(&spi->dev,
             "spi mode=%u bits=%u max_speed=%u\n",
             spi->mode, spi->bits_per_word, spi->max_speed_hz);

    regmap = devm_regmap_init(&spi->dev,
                              &sc7u22_spi_regmap_bus,
                              spi,
                              &sc7u22_regmap_config);
    if (IS_ERR(regmap)) {
        dev_err(&spi->dev, "Failed to register spi regmap: %pe\n", regmap);
        return PTR_ERR(regmap);
    }

    if (!id) {
        dev_err(&spi->dev, "spi_device_id is NULL\n");
        return -ENODEV;
    }

    return sc7u22_core_probe(&spi->dev, regmap, id->name, true);
}

static const struct spi_device_id sc7u22_spi_id[] = {
    { "sc7u22", 0 },
    { }
};
MODULE_DEVICE_TABLE(spi, sc7u22_spi_id);

static const struct acpi_device_id sc7u22_acpi_match[] = {
    { "SC7U22", 0 },
    { }
};
MODULE_DEVICE_TABLE(acpi, sc7u22_acpi_match);

#ifdef CONFIG_OF
static const struct of_device_id sc7u22_of_match[] = {
    { .compatible = "qst,sc7u22" },
    { }
};
MODULE_DEVICE_TABLE(of, sc7u22_of_match);
#endif

static struct spi_driver sc7u22_spi_driver = {
    .probe = sc7u22_spi_probe,
    .id_table = sc7u22_spi_id,
    .driver = {
        .name = "sc7u22_spi",
        .acpi_match_table = ACPI_PTR(sc7u22_acpi_match),
        .of_match_table = of_match_ptr(sc7u22_of_match),
    },
};

module_spi_driver(sc7u22_spi_driver);

MODULE_DESCRIPTION("SC7U22 SPI IIO ID-only driver");
MODULE_LICENSE("GPL v2");
```

关键点：**`sc7u22_spi_probe()` 里面不再提前读 ID。**

---

# 三、`sc7u22_core.c` 改成 IIO ID-only 版本

这个版本仍然使用 IIO 框架，但只做：

1. `regmap_read(0x01)` 读 ID；
2. ID 是 `0x6A` 就注册 IIO；
3. 提供一个 `/sys/bus/iio/devices/iio:deviceX/chip_id` 节点；
4. 不读加速度/陀螺仪数据；
5. 不 setup trigger buffer。

直接先把 `sc7u22_core.c` 改成这个最小版本：

```c
// SPDX-License-Identifier: GPL-2.0
/*
 * SC7U22 IIO core driver - ID only stage
 */

#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/delay.h>

#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>

#include "sc7u22.h"

const struct regmap_config sc7u22_regmap_config = {
    .reg_bits = 8,
    .val_bits = 8,
};
EXPORT_SYMBOL_GPL(sc7u22_regmap_config);

/*
 * IIO sysfs:
 * /sys/bus/iio/devices/iio:deviceX/chip_id
 */
static ssize_t sc7u22_chip_id_show(struct device *dev,
                                   struct device_attribute *attr,
                                   char *buf)
{
    struct iio_dev *indio_dev = dev_to_iio_dev(dev);
    struct sc7u22_data *data = iio_priv(indio_dev);
    unsigned int val;
    int ret;

    ret = regmap_read(data->regmap, SC7U22_REG_CHIP_ID, &val);
    if (ret)
        return ret;

    return sprintf(buf, "0x%02x\n", val);
}

static IIO_DEVICE_ATTR(chip_id, 0444,
                       sc7u22_chip_id_show,
                       NULL,
                       0);

static struct attribute *sc7u22_attrs[] = {
    &iio_dev_attr_chip_id.dev_attr.attr,
    NULL,
};

static const struct attribute_group sc7u22_attrs_group = {
    .attrs = sc7u22_attrs,
};

static const struct iio_info sc7u22_info = {
    .attrs = &sc7u22_attrs_group,
};

static int sc7u22_chip_init_id_only(struct sc7u22_data *data)
{
    int ret;
    unsigned int val;
    struct device *dev = regmap_get_device(data->regmap);

    dev_info(dev, "SC7U22 chip_init: ID only mode\n");

    /*
     * 先延时一下，避免设备刚注册后马上读。
     */
    msleep(5);

    ret = regmap_read(data->regmap, SC7U22_REG_CHIP_ID, &val);
    dev_info(dev,
             "SC7U22 WHO_AM_I ret=%d val=0x%02x expected=0x%02x\n",
             ret, val, SC7U22_CHIP_ID_VAL);

    if (ret)
        return ret;

    if (val != SC7U22_CHIP_ID_VAL) {
        dev_err(dev,
                "SC7U22 wrong chip id: got 0x%02x expected 0x%02x\n",
                val, SC7U22_CHIP_ID_VAL);
        return -ENODEV;
    }

    dev_info(dev, "SC7U22 chip id OK\n");

    return 0;
}

int sc7u22_core_probe(struct device *dev, struct regmap *regmap,
                      const char *name, bool use_spi)
{
    struct iio_dev *indio_dev;
    struct sc7u22_data *data;
    int ret;

    dev_info(dev, "SC7U22 core probe: IIO ID-only mode\n");

    indio_dev = devm_iio_device_alloc(dev, sizeof(*data));
    if (!indio_dev)
        return -ENOMEM;

    data = iio_priv(indio_dev);
    dev_set_drvdata(dev, indio_dev);

    data->regmap = regmap;

    ret = sc7u22_chip_init_id_only(data);
    if (ret)
        return ret;

    /*
     * ID-only 阶段：
     * 不配置 channels
     * 不实现 read_raw
     * 不配置 trigger buffer
     */
    indio_dev->name = name ? name : "sc7u22";
    indio_dev->modes = INDIO_DIRECT_MODE;
    indio_dev->info = &sc7u22_info;

    ret = devm_iio_device_register(dev, indio_dev);
    if (ret) {
        dev_err(dev, "failed to register IIO device: %d\n", ret);
        return ret;
    }

    dev_info(dev, "SC7U22 IIO device registered, ID-only mode\n");

    return 0;
}
EXPORT_SYMBOL_GPL(sc7u22_core_probe);

MODULE_DESCRIPTION("SC7U22 IIO core driver - ID only");
MODULE_LICENSE("GPL v2");
```

---

# 四、`sc7u22.h` 需要保留这些宏

你现在头文件里已经有这些，OK：

```c
#define SC7U22_CHIP_ID_VAL      0x6A
#define SC7U22_REG_CHIP_ID      0x01
```

并且 `struct sc7u22_data` 里有：

```c
struct regmap *regmap;
```

这就够 ID-only 阶段用了。

---

# 五、加载模块流程

这个版本还是用 IIO，所以要加载：

```text
sc7u22_core.ko
sc7u22_spi.ko
sc7u22_spi_board.ko
```

但是不需要加载：

```text
industrialio-triggered-buffer.ko
```

因为这个 ID-only 版没有调用 `devm_iio_triggered_buffer_setup()`。

完整加载：

```sh
cd /lib/modules/5.10.186

rmmod sc7u22_spi_board 2>/dev/null
rmmod sc7u22_spi 2>/dev/null
rmmod sc7u22_core 2>/dev/null
rmmod industrialio_triggered_buffer 2>/dev/null
rmmod soc_spi 2>/dev/null
rmmod utils 2>/dev/null

insmod ./utils.ko

insmod ./soc_spi.ko spi0_is_enable=1 spi1_is_enable=0 div_ssi_rate=10000000 spi0_clk=PD00 spi0_mosi=PD01 spi0_miso=PD02

echo "==== spi master ===="
ls /sys/class/spi_master/

insmod ./sc7u22_core.ko
insmod ./sc7u22_spi.ko
insmod ./sc7u22_spi_board.ko

echo "==== dmesg ===="
dmesg | tail -n 120

echo "==== spi devices ===="
ls /sys/bus/spi/devices/

echo "==== iio devices ===="
ls /sys/bus/iio/devices/
```

---

# 六、成功后怎么验证

如果成功，你应该看到：

```sh
ls /sys/bus/iio/devices/
```

输出类似：

```text
iio:device0
```

然后读 ID：

```sh
cat /sys/bus/iio/devices/iio:device0/name
cat /sys/bus/iio/devices/iio:device0/chip_id
```

期望：

```text
sc7u22
0x6a
```

dmesg 里应该有：

```text
SC7U22 core probe: IIO ID-only mode
SC7U22 chip_init: ID only mode
SC7U22 WHO_AM_I ret=0 val=0x6a expected=0x6a
SC7U22 chip id OK
SC7U22 IIO device registered, ID-only mode
```

---

## 当前你要注意的点

你之前的 `sc7u22_core_probe()` 是先 `sc7u22_chip_init()`，成功后才配置 IIO 并 `devm_iio_device_register()`；所以只要 chip init 返回失败，`/sys/bus/iio/devices/` 就一定是空的。

现在这个 ID-only 版的目的就是：**让 IIO 框架先跑通，只暴露一个 chip_id 节点。等这个通了，再慢慢加 soft reset、PWR_CTRL、ACC/GYR 初始化、read_raw。**


-----
------
-------
可以了，这次**已经成功了**。

关键日志不是前面的 `utils: Unknown symbol`，那是之前失败的旧日志。你真正这次成功的关键在后面：

```text
[ 579.128120] sc7u22_spi spi0.0: SC7U22 core probe: IIO ID-only mode
[ 579.128145] sc7u22_spi spi0.0: SC7U22 chip_init: ID only mode
[ 579.150231] sc7u22_spi spi0.0: reg_read: reg=0x01 cmd=0x81 len=1 val0=0x6a
[ 579.150248] sc7u22_spi spi0.0: SC7U22 WHO_AM_I ret=0 val=0x6a expected=0x6a
[ 579.150256] sc7u22_spi spi0.0: SC7U22 chip id OK
[ 579.150466] sc7u22_spi spi0.0: SC7U22 IIO device registered, ID-only mode
```

然后你这里也出来了：

```sh
ls /sys/bus/spi/devices/
spi0.0

ls /sys/bus/iio/devices/
iio:device0
```

这说明这几条链路全部通了：

```text
utils.ko / soc_spi.ko
  ↓
spi0 master
  ↓
sc7u22_spi_board.ko 创建 spi0.0
  ↓
sc7u22_spi.ko probe
  ↓
regmap 通过 SPI 读 WHO_AM_I = 0x6A
  ↓
sc7u22_core.ko 注册 IIO
  ↓
/sys/bus/iio/devices/iio:device0
```

所以之前担心的“君正这种方式用不了 IIO”可以排除了，**君正 SPI + board 注册 + regmap + IIO 是可以跑通的。**

---

## 现在马上验证一下 sysfs

执行：

```sh
cat /sys/bus/iio/devices/iio:device0/name
cat /sys/bus/iio/devices/iio:device0/chip_id
```

期望大概是：

```text
sc7u22
0x6a
```

如果 `chip_id` 能读到 `0x6a`，那 ID-only IIO 阶段就彻底 OK。

---

## 前面的 `utils Unknown symbol` 怎么看？

不用管，那是旧日志。因为你的 `dmesg` 没清掉，之前失败的记录还在里面。

这次实际已经加载成功了，否则不会有：

```text
spi0.0
iio:device0
```

后面看日志时可以用这种方式过滤：

```sh
dmesg | grep -i sc7u22
```

或者：

```sh
dmesg | tail -n 40
```

---

## 下一步建议

现在先别急着读 IMU 数据。先把这个版本保存下来，作为稳定基线：

```text
阶段 1：IIO 框架 + SPI + regmap + WHO_AM_I 只读 ID 成功
```

然后下一步再慢慢加初始化，顺序建议是：

```text
1. 加 soft reset，reset 后再读 WHO_AM_I
2. 加 PWR_CTRL，只打开 ACC/GYR
3. 加 ACC_CONF / ACC_RANGE
4. 加 GYR_CONF / GYR_RANGE
5. 每写一个寄存器，就 readback 验证
6. 最后再实现 read_raw 读加速度/陀螺仪数据
```

不要一次全加。你现在这个状态已经很关键了，说明底层框架没问题，后面就是按 SC7U22 手册补初始化和数据寄存器读取。



---------
--------
-------


这不算全失败。**IIO 节点已经注册成功了**，说明这条链路已经通了：

```text
SPI master -> spi0.0 -> sc7u22_spi probe -> regmap -> IIO 注册
```

现在的问题是：**probe 阶段第一次读 ID 是 0x6A，但你 cat `chip_id` 时又重新读了一次寄存器，这次读成了 0x00。**

你日志里已经证明 probe 时读 ID 成功：

```text
reg_read: reg=0x01 cmd=0x81 len=1 val0=0x6a
SC7U22 WHO_AM_I ret=0 val=0x6a expected=0x6a
SC7U22 chip id OK
SC7U22 IIO device registered, ID-only mode
```

所以 `/sys/bus/iio/devices/iio:device0` 能出来。你现在 `cat chip_id = 0x00`，说明 `chip_id_show()` 里面又做了一次 `regmap_read(0x01)`，而这次读失败成 0 了。这个和你前面遇到的“第一笔读到 0x6A，后续读到 0x00”是同一个问题。

---

## 先确认一下

你执行：

```sh
dmesg | grep -i "reg_read\|SC7U22\|chip_id" | tail -n 80
```

大概率会看到你 `cat chip_id` 之后又出现一条：

```text
reg_read: reg=0x01 cmd=0x81 len=1 val0=0x00
```

---

## 当前 ID-only 阶段，建议先用“缓存 ID”

既然 probe 阶段已经读到 `0x6A`，那 ID-only IIO 阶段可以先把 ID 缓存起来。`cat chip_id` 不要再实时读 SPI，直接返回 probe 时读到的值。

这样可以先确认：

```text
IIO 框架注册 OK
sysfs 节点 OK
chip_id 属性 OK
```

后面再单独解决“重复 SPI 读有时变 0”的问题。

---

## 1. 修改 `sc7u22.h`

在 `struct sc7u22_data` 里加一个缓存字段：

```c
struct sc7u22_data {
    struct regmap *regmap;
    struct iio_trigger *trig;
    struct regulator_bulk_data supplies[2];
    struct iio_mount_matrix orientation;

    /*
     * ID-only 调试阶段：
     * probe 时读到的 WHO_AM_I 缓存在这里。
     */
    u8 chip_id;

    __le16 buf[12] __aligned(8);
};
```

---

## 2. 修改 `sc7u22_chip_init_id_only()`

把 probe 阶段读到的 ID 存起来：

```c
static int sc7u22_chip_init_id_only(struct sc7u22_data *data)
{
    int ret;
    unsigned int val;
    struct device *dev = regmap_get_device(data->regmap);

    dev_info(dev, "SC7U22 chip_init: ID only mode\n");

    msleep(5);

    ret = regmap_read(data->regmap, SC7U22_REG_CHIP_ID, &val);
    dev_info(dev,
             "SC7U22 WHO_AM_I ret=%d val=0x%02x expected=0x%02x\n",
             ret, val, SC7U22_CHIP_ID_VAL);

    if (ret)
        return ret;

    if (val != SC7U22_CHIP_ID_VAL) {
        dev_err(dev,
                "SC7U22 wrong chip id: got 0x%02x expected 0x%02x\n",
                val, SC7U22_CHIP_ID_VAL);
        return -ENODEV;
    }

    data->chip_id = val & 0xff;

    dev_info(dev, "SC7U22 chip id OK, cached chip_id=0x%02x\n",
             data->chip_id);

    return 0;
}
```

---

## 3. 修改 `chip_id_show()`

不要实时 `regmap_read()`，先返回缓存值：

```c
static ssize_t sc7u22_chip_id_show(struct device *dev,
                                   struct device_attribute *attr,
                                   char *buf)
{
    struct iio_dev *indio_dev = dev_to_iio_dev(dev);
    struct sc7u22_data *data = iio_priv(indio_dev);

    return sprintf(buf, "0x%02x\n", data->chip_id);
}
```

这样再执行：

```sh
cat /sys/bus/iio/devices/iio:device0/chip_id
```

应该就是：

```text
0x6a
```

---

## 这个修改代表什么？

这代表 **IIO ID-only 阶段完成**。但它不代表“重复 SPI 读寄存器已经完全稳定”。

你现在实际状态是：

```text
probe 第一次读 WHO_AM_I：成功 0x6A
IIO 注册：成功
sysfs chip_id 实时再读：变 0x00
```

所以后面要单独解决的是：

```text
为什么同样读 0x01，第一次是 0x6A，后续读成 0x00？
```

这个可能和 SC7U22 的 SPI 时序、CS 间隔、君正 SPI 控制器连续读、或者芯片进入某种状态有关。

---

## 下一步建议

先把 `chip_id` 改成缓存值，让 IIO 框架阶段彻底稳定。然后我们再加一个新的 sysfs 节点，比如：

```text
chip_id_live
```

专门用来实时读寄存器调试。这样可以同时区分：

```text
chip_id       = probe 缓存值，验证 IIO
chip_id_live  = 实时 SPI 读值，调试重复读问题
```
