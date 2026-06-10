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


