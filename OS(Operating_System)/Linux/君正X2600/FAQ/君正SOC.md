你这个问题问到点子上了：**这个 `build/configs/x2600n_tiger_nor_5.10_defconfig` 不是 Linux 设备树文件，也不是 Linux kernel `.config`。**

它是君正工程自己的“总工程配置 / IConfig 配置文件”，作用更像：

```text
告诉整个 X2600 工程：
1. 用哪个 u-boot 配置
2. 用哪个 kernel defconfig
3. 用哪个 buildroot 配置
4. 编哪些 module_driver 模块
5. module_driver 启动时按什么顺序加载
6. 某些模块加载时传哪些参数
```

你看到这里：

```bash
APP_kernel_config=x2600_evb_module_base_linux_sfc_nor_defconfig
# APP_kernel_dtb is not set
APP_module_driver=y
MD_INIT_ORDER_FILE=package/default_driver_init_order.mk
MD_INIT_START_FILE=package/S11module_driver_default
```

这几个非常关键。

---

## 1. 它不是设备树，它是“工程总配置”

你现在看的这个：

```bash
build/configs/x2600n_tiger_nor_5.10_defconfig
```

控制的是整个编译工程。它里面的：

```bash
APP_kernel_config=x2600_evb_module_base_linux_sfc_nor_defconfig
```

意思是：真正 Linux 内核用的 defconfig 是这个：

```bash
kernel/kernel/arch/mips/configs/x2600_evb_module_base_linux_sfc_nor_defconfig
```

你可以这样看：

```bash
cd ~/project/x2600

cat kernel/kernel/arch/mips/configs/x2600_evb_module_base_linux_sfc_nor_defconfig | less
```

这个里面才是类似：

```text
CONFIG_SPI=y
CONFIG_IIO=y
CONFIG_BMI160_SPI=m
CONFIG_OF=y
```

这种 Linux 内核配置。

---

## 2. `# APP_kernel_dtb is not set` 不代表系统没有设备树

你板子上之前已经能看到：

```text
/sys/firmware/devicetree/base/apb/spi@0x10044000
/sys/firmware/devicetree/base/apb/spi@0x10043000
/sys/firmware/devicetree/base/aliases/spi0
/sys/firmware/devicetree/base/aliases/spi1
```

这说明运行中的系统**确实有 device tree**。

所以：

```bash
# APP_kernel_dtb is not set
```

更可能表示：**这个顶层工程没有单独指定一个外部 dtb 文件路径**。

可能有几种情况：

```text
1. DTB 被内核构建系统默认选择了
2. DTB 被打包进 kernel image 里
3. u-boot/烧录镜像里已经包含默认 DTB
4. 君正这套 module_driver 主要不靠 DTB 配外设，而靠 IConfig + 模块参数
```

所以不能简单理解成“没有设备树”。

你可以在 PC 上这样找 DTS：

```bash
cd ~/project/x2600/kernel/kernel

find arch/mips -name "*x2600*.dts" -o -name "*x2600*.dtsi"
find arch/mips -name "*.dts" | grep -i x2600
```

也可以找 SPI 节点来源：

```bash
cd ~/project/x2600/kernel/kernel

grep -R "spi@0x10043000\|spi@0x10044000\|10043000\|10044000" \
    arch/mips -n | head -100
```

---

## 3. 君正这套 SPI 是“设备树 + module_driver”混合玩法

标准 Linux 常见玩法是：

```text
设备树 dts 里写：
    spi0 节点
    bmi160@0 子节点
        compatible = "bosch,bmi160"
        reg = <0>
        spi-max-frequency = <1000000>
```

然后内核启动时自动创建 `spi0.0`，驱动自动 probe。

但是君正 X2600 这套工程很多地方不是这样玩的。它用了自己的：

```text
module_driver
IConfigTool
MD_XXX 配置
S11module_driver_default 启动脚本
```

你配置的：

```bash
MD_X2600_510_SPI:=y
MD_X2600_510_SPI0=y
MD_X2600_510_SPI0_CLK=PD00
MD_X2600_510_SPI0_MISO=PD02
MD_X2600_510_SPI0_MOSI=PD01
```

这些不是 Linux DTS 语法，而是君正 `module_driver/soc/x2600_510/spi/soc_spi.ko` 的配置来源。

也就是说，X2600 上 SPI0 这个 master 不是单纯靠 DTS 自动起来，而是靠：

```text
utils.ko
    ↓
soc_spi.ko spi0_is_enable=1 spi0_clk=PD00 spi0_mosi=PD01 spi0_miso=PD02
    ↓
注册 Linux SPI master
    ↓
/sys/class/spi_master/spi0 出现
```

然后你的 BMI160 再靠：

```text
bmi160_spi_board.ko
    ↓
spi_new_device()
    ↓
创建 spi0.0
    ↓
匹配 bmi160_spi.ko
```

君正文档里也正是这种思路：SPI 驱动层示例通过 `spi_busnum_to_master()` 找到 SPI master，再用 `spi_new_device()` 注册 SPI 设备；如果 master 不存在，就会失败。

---

## 4. 你现在为什么 IConfig 配了，但是手动 insmod 没生效？

因为你现在是手动执行：

```sh
insmod soc_spi.ko
```

这不会自动读取：

```bash
build/.config.in
```

也不会自动读取：

```bash
build/configs/x2600n_tiger_nor_5.10_defconfig
```

IConfig 里的参数通常会在这类文件里被用到：

```bash
MD_INIT_START_FILE=package/S11module_driver_default
MD_INIT_ORDER_FILE=package/default_driver_init_order.mk
```

也就是说，正常系统启动时，君正的启动脚本可能会生成类似：

```sh
insmod utils.ko
insmod soc_spi.ko spi0_is_enable=1 spi0_clk=PD00 spi0_mosi=PD01 spi0_miso=PD02 ...
```

但是你现在手动调试时，如果只写：

```sh
insmod soc_spi.ko spi0_is_enable=1
```

那 `spi0_clk` 没传，它就用默认值。你看到的：

```text
SPI spi0_clk(PA00) is invaild!
```

就是因为它用了默认 `PA00`。

---

## 5. 怎么确认系统启动脚本到底传了啥参数？

PC 上查：

```bash
cd ~/project/x2600

grep -R "soc_spi\|spi0_clk\|spi0_mosi\|spi0_miso\|MD_X2600_510_SPI0" \
    -n module_driver package buildroot/buildroot24/output/target 2>/dev/null | head -200
```

重点看这些文件：

```bash
module_driver/package/S11module_driver_default
module_driver/package/default_driver_init_order.mk
buildroot/buildroot24/output/target/module_driver/
buildroot/buildroot24/output/target/etc/init.d/
```

再查 rootfs 里有没有启动脚本：

```bash
cd ~/project/x2600

find buildroot/buildroot24/output/target -name "*module*" -o -name "S11*"
```

板子上也查：

```sh
find / -name "*module*" 2>/dev/null
find /etc/init.d -type f 2>/dev/null
```

然后：

```sh
grep -R "soc_spi\|spi0_clk\|spi0_mosi\|spi0_miso" /etc /module_driver 2>/dev/null
```

如果板子上没有这些脚本，说明 rootfs 没烧对，或者 module_driver 自动加载没集成进去。

---

## 6. 你现在手动调试，直接绕过启动脚本就行

你现在最直接的办法就是手动传参数：

```sh
cd /tmp/kernel_drv_mode

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
ls /sys/class/spi_master/
```

如果成功，你会看到：

```text
spi0
```

然后再加载 BMI160：

```sh
insmod kfifo_buf.ko 2>/dev/null
insmod industrialio-triggered-buffer.ko 2>/dev/null
insmod bmi160_core.ko 2>/dev/null
insmod bmi160_spi.ko 2>/dev/null
insmod bmi160_spi_board.ko

dmesg | tail -100
ls /sys/bus/spi/devices/
ls /sys/bus/iio/devices/
```

---

## 7. 这个配置文件到底怎么工作？

用一张流程图说就是：

```text
build/configs/x2600n_tiger_nor_5.10_defconfig
        │
        ├── APP_kernel_config=xxx
        │       ↓
        │   选择 kernel/kernel/arch/mips/configs/xxx
        │       ↓
        │   生成 kernel/kernel/.config
        │       ↓
        │   编译 Linux kernel
        │
        ├── APP_br_config_file=xxx
        │       ↓
        │   选择 buildroot 配置
        │       ↓
        │   生成 rootfs
        │
        └── MD_X2600_510_SPI0=y
                ↓
            module_driver 读取 MD_ 配置
                ↓
            编译 soc_spi.ko / spidev.ko / spidev_helper.ko
                ↓
            生成 module_driver/output/*.ko
                ↓
            拷贝到 rootfs 的 /module_driver/
                ↓
            启动脚本按参数 insmod
```

所以它不是 DTS，但是它会影响模块驱动怎么编译、怎么启动、怎么传参。

---

## 8. 你现在应该检查的是“启动脚本有没有传参数”

PC 上执行：

```bash
cd ~/project/x2600

grep -R "soc_spi" -n module_driver buildroot/buildroot24/output/target 2>/dev/null | head -100
grep -R "spi0_clk\|spi0_mosi\|spi0_miso" -n module_driver buildroot/buildroot24/output/target 2>/dev/null | head -100
```

如果能找到类似：

```sh
insmod /module_driver/soc_spi.ko spi0_is_enable=1 spi0_clk=PD00 ...
```

说明配置生成了。

如果找不到，那就说明 module_driver 的启动脚本没有把这些参数生成进去，你手动 `insmod` 就必须自己传。

---

## 结论

你这个 X2600 工程是：

```text
设备树：有，但主要描述 SoC 基础硬件节点
IConfig/module_driver：负责很多板级外设模块的启用、引脚、加载参数
bmi160_spi_board.ko：负责把 BMI160 这个 SPI 从设备挂到已经存在的 spi0 master 上
```

你现在不是“没设备树”，而是**君正这套 SPI 用的是 module_driver 参数化加载方式**。你手动调试时，不会自动吃 `x2600n_tiger_nor_5.10_defconfig` 里的 `MD_X2600_510_SPI0_CLK=PD00`，必须手动给 `soc_spi.ko` 传：

```sh
spi0_clk=PD00 spi0_mosi=PD01 spi0_miso=PD02
```
