你这个疑惑很正常。**君正 X2600 的流程不是“标准 Linux 流程消失了”，而是它在标准 Linux 外面又套了一层君正自己的产品配置系统。**

你可以先这样理解：

```text id="qz9zty"
标准 Linux：
DTS + Kconfig + Driver code  -> 编译 kernel

君正 X2600：
IConfigTool + build 总 defconfig + kernel defconfig + Buildroot defconfig + Driver code
-> 生成/配置一堆东西
-> 再编译 uboot / kernel / buildroot / module_driver
-> 打包 output
```

---

## 1. 标准 Linux 是什么流程？

正常嵌入式 Linux 开发，一般是：

```text id="9q8vpk"
1. 写/改驱动代码
   drivers/iio/imu/bmi160/xxx.c

2. 配 Kconfig / Makefile
   让这个驱动可以被 menuconfig 选中

3. 配 kernel defconfig
   CONFIG_BMI160_I2C=y 或 m

4. 配 DTS
   告诉 kernel 板子上有这个设备：
   bmi160@68 {
       compatible = "bosch,bmi160";
       reg = <0x68>;
   };

5. 编译 kernel + dtb
6. 烧录 kernel/dtb/rootfs
7. 上板看 dmesg / sysfs
```

这个逻辑在君正 X2600 上仍然存在。

---

## 2. 那君正 IConfigTool 是干啥的？

IConfigTool 你可以理解成一个 **“板级配置生成器 / 产品配置 GUI”**。

它主要不是替代 Linux 驱动开发，而是帮你配置这些东西：

```text id="cueynl"
1. 这个产品启用哪些外设模块
   例如 I2C、SPI、LCD、Camera、Audio、GPIO Key 等。

2. 引脚怎么复用
   比如某几根 pin 是 I2C，还是 SPI，还是 GPIO。

3. 君正 module_driver 里哪些模块参与编译/启动
   例如 MD_XXX 这些配置。

4. 模块初始化顺序
   例如 MD_INIT_ORDER_FILE、S11module_driver_default。

5. 某些板级参数、脚本、rootfs 配置
```

你传的官方 SPI 文档里也体现了这个逻辑：SPI 要先在 IConfigTool 里配置 SPI 控制器或者 GPIO_SPI，然后 kernel 侧还要配置 SPI 相关驱动，应用层/驱动层再使用 SPI 设备。

也就是说：

```text id="9ppra7"
IConfigTool 更偏“板级资源配置 / 君正模块配置”
Kconfig 更偏“Linux kernel 编译哪些功能”
DTS 更偏“Linux 设备树硬件描述”
driver code 更偏“具体设备驱动逻辑”
```

---

## 3. 你现在这个工程的核心入口是 build 目录

你现在执行的是：

```bash id="k5mk5r"
cd ~/project/x2600/build
make x2600n_tiger_nor_5.10_defconfig
make
```

这个 `x2600n_tiger_nor_5.10_defconfig` 是 **君正工程总配置**，里面写了：

```bash id="2e745m"
APP_uboot_config=x2600e_base_xImage_sfc_nor
APP_kernel_dir=../kernel/kernel
APP_kernel_config=x2600_evb_module_base_linux_sfc_nor_defconfig
APP_br_config_file=configs/buildroot/buildroot24_xburst2_wifi_common_defconfig
APP_module_driver=y
MD_INIT_ORDER_FILE=package/default_driver_init_order.mk
MD_INIT_START_FILE=package/S11module_driver_default
```

所以这个总配置的意思是：

```text id="q8iuqp"
uboot 用哪个配置
kernel 用哪个目录
kernel 用哪个 defconfig
buildroot 用哪个 defconfig
是否启用 module_driver
module_driver 启动顺序是什么
```

它不是普通 kernel 的 defconfig，它是 **整机产品配置入口**。

---

## 4. 君正的完整开发流程大概是这样

### 第 1 层：选产品总配置

```bash id="l8hyzn"
cd ~/project/x2600/build
make x2600n_tiger_nor_5.10_defconfig
```

这个动作会把：

```bash id="rmcvjs"
build/configs/x2600n_tiger_nor_5.10_defconfig
```

复制/转换成当前工程配置，比如：

```bash id="9t3em8"
build/.config.in
build/.config
build/include/config.h
```

然后它会根据里面的：

```bash id="o5egfd"
APP_kernel_config=x2600_evb_module_base_linux_sfc_nor_defconfig
```

去设置 kernel 配置。

---

### 第 2 层：IConfigTool 配板级资源

比如你要用 SPI、I2C、LCD、Camera、GPIO、按键、音频这些东西，君正希望你通过 IConfigTool 配。

IConfigTool 保存之后，通常会影响：

```text id="hqy7ag"
build/configs/x2600n_tiger_nor_5.10_defconfig
module_driver 相关配置
某些板级头文件
某些 DTS stage 文件
某些启动脚本/初始化顺序
```

你的 build Makefile 里也能看到 module_driver 这层：

```makefile id="s8j49f"
MD_DIR := $(TOPDIR)/../module_driver
include cmd_module_driver.mk
```

而 kernel 镜像复制这里也有：

```makefile id="kwh1kx"
KERNEL_DTB_PATH=module_drivers/dts-stage
```

这说明君正确实把一部分设备树/模块配置放到了它自己的 `module_driver` 体系里。

---

### 第 3 层：kernel defconfig 配 Linux 内核功能

你真正的 kernel 配置文件是：

```bash id="mk0eca"
~/project/x2600/kernel/kernel/arch/mips/configs/x2600_evb_module_base_linux_sfc_nor_defconfig
```

比如 BMI160 这种 IIO IMU 驱动，最终还是要这里打开：

```text id="q8jwbl"
CONFIG_IIO=y
CONFIG_BMI160=y
CONFIG_BMI160_I2C=y
```

或者：

```text id="t5g5ja"
CONFIG_BMI160=m
CONFIG_BMI160_I2C=m
```

所以 BMI160 这个东西，**IConfigTool 不一定能直接帮你配出来**。IConfigTool 主要帮你把 I2C/SPI 控制器、pinmux、君正模块资源配好；BMI160 这个具体传感器驱动，还是要靠 kernel Kconfig 和设备注册。

---

### 第 4 层：DTS / 设备注册

这是你最容易困惑的地方。

在标准 Linux 里，I2C BMI160 通常应该在 DTS 里写：

```dts id="p4oqdk"
&i2c0 {
    status = "okay";

    bmi160@68 {
        compatible = "bosch,bmi160";
        reg = <0x68>;
    };
};
```

但是君正有些模块不是完全走主线 DTS 方式，而是走它自己的 `module_driver` 或板级注册方式。

你传的 SPI 官方文档里，君正给的示例不是单纯写 DTS，而是在驱动里用：

```c id="bo97ya"
spi_register_device(...)
spi_register_driver(...)
```

也就是手动注册 SPI device + SPI driver。

所以在君正 X2600 上，有两种可能：

```text id="i45zfh"
方式 A：走标准 DTS
DTS 里描述 bmi160@68，kernel 自动 probe。

方式 B：走君正 module_driver / 板级注册
IConfigTool 配总线和引脚，模块代码里注册设备，然后驱动 probe。
```

你要先看当前工程其他 I2C/SPI 设备是怎么做的。

---

## 5. 对 BMI160 来说，实际应该怎么理解？

BMI160 驱动开发要分成 4 件事：

```text id="vpm6ub"
1. I2C/SPI 控制器有没有打开？
   这个偏 IConfigTool + kernel 控制器配置。

2. BMI160 驱动有没有编译？
   这个偏 kernel Kconfig/defconfig。

3. 系统知不知道板子上挂了 BMI160？
   这个偏 DTS 或君正 module_driver 注册。

4. 上板有没有 probe 成功？
   这个看 dmesg 和 /sys/bus/iio/devices。
```

你不能只做第 2 步。

如果只打开：

```text id="h0gm31"
CONFIG_BMI160_I2C=y
```

但是没有 DTS 或设备注册，那么结果是：

```text id="49xqau"
驱动代码编进去了，但是没有设备匹配它，所以 probe 不会执行。
```

这就像 MCU 里你写了 `bmi160_init()` 函数，但是没人调用它。

---

## 6. 君正 X2600 的流程可以画成这样

```text id="p8oukb"
                    ┌────────────────────────────┐
                    │ build/configs/xxx_defconfig │
                    │ 产品总配置                   │
                    └──────────────┬─────────────┘
                                   │
             ┌─────────────────────┼─────────────────────┐
             │                     │                     │
             v                     v                     v
   ┌─────────────────┐   ┌──────────────────┐   ┌──────────────────┐
   │ U-Boot config    │   │ Kernel defconfig  │   │ Buildroot config  │
   │ APP_uboot_config │   │ APP_kernel_config │   │ APP_br_config     │
   └─────────────────┘   └─────────┬────────┘   └──────────────────┘
                                    │
                                    v
                       ┌──────────────────────────┐
                       │ kernel/kernel/.config     │
                       │ 实际 kernel 编译配置       │
                       └────────────┬─────────────┘
                                    │
                                    v
                       ┌──────────────────────────┐
                       │ kernel driver 编译         │
                       │ drivers/iio/imu/bmi160    │
                       └──────────────────────────┘


             ┌────────────────────────────────────────┐
             │ IConfigTool                             │
             │ 配 pinmux / bus / module_driver / 板级资源 │
             └───────────────────┬────────────────────┘
                                 │
                                 v
             ┌────────────────────────────────────────┐
             │ module_driver / dts-stage / init script │
             │ 君正自己的板级模块体系                    │
             └────────────────────────────────────────┘
```

所以它不是一条线，而是多层配置一起生效。

---

## 7. 你现在做 BMI160，推荐按这个顺序排

### 第一步：先确认 BMI160 是 I2C 还是 SPI

比如如果是 I2C，确认地址：

```text id="mkxae1"
0x68 或 0x69
```

如果是 SPI，确认：

```text id="plgd8t"
SPI bus 几
CS 哪个 GPIO
SPI mode 几
最大频率多少
```

---

### 第二步：用 IConfigTool 打开总线和引脚

如果 BMI160 接 I2C0：

```text id="nepfvk"
IConfigTool:
打开 I2C0
配置 I2C0 SCL/SDA 对应 pinmux
保存到 x2600n_tiger_nor_5.10_defconfig
```

如果 BMI160 接 SPI：

```text id="gsyi0g"
IConfigTool:
打开 SPI 控制器或者 GPIO_SPI
配置 CLK/MOSI/MISO/CS 引脚
保存到 x2600n_tiger_nor_5.10_defconfig
```

官方 SPI 文档也是这个思路：先在 IConfigTool 里配置 SPI 总线，再在 kernel 或应用层使用 SPI。

---

### 第三步：打开 kernel BMI160 配置

```bash id="icfca2"
cd ~/project/x2600/kernel/kernel

make ARCH=mips x2600_evb_module_base_linux_sfc_nor_defconfig
make ARCH=mips menuconfig
```

路径大概是：

```text id="mstzx4"
Device Drivers
  ---> Industrial I/O support
       ---> Inertial measurement units
            ---> Bosch BMI160 I2C driver
```

保存后：

```bash id="ry42hr"
make ARCH=mips savedefconfig
cp defconfig arch/mips/configs/x2600_evb_module_base_linux_sfc_nor_defconfig
```

---

### 第四步：添加设备描述或者设备注册

这一步最关键。

你要找项目里现在的 I2C/SPI 设备是怎么添加的：

```bash id="bjy1ia"
cd ~/project/x2600

grep -R "i2c_board_info\|i2c_register_board_info\|of_device_id\|compatible.*bmi\|spi_register_device\|spi_board_info" \
kernel/kernel module_driver -n | head -100
```

如果项目大量使用 DTS，那就找 DTS：

```bash id="k15nql"
find kernel/kernel -name "*.dts" -o -name "*.dtsi"
find module_driver -name "*.dts" -o -name "*.dtsi"
```

如果项目大量使用君正 module_driver 注册设备，那你就要模仿它的写法。

---

### 第五步：编译

```bash id="jnyda1"
cd ~/project/x2600/build

make x2600n_tiger_nor_5.10_defconfig
make clean_kernel
make kernel
```

或者整包：

```bash id="wbd054"
make -j4
```

---

### 第六步：烧录并验证

上板后：

```bash id="bdlkqn"
dmesg | grep -i bmi
dmesg | grep -i iio
dmesg | grep -i i2c

ls /sys/bus/iio/devices/
for f in /sys/bus/iio/devices/iio:device*/name; do echo $f; cat $f; done
```

如果没有 `bmi160`，就说明：

```text id="qccdzj"
要么驱动没编进去；
要么设备没有注册；
要么 I2C/SPI 总线没起来；
要么地址/compatible 不匹配；
要么 pinmux 没配对。
```

---

## 8. 你可以把君正流程记成这个口诀

```text id="t2fflo"
IConfigTool 管板级资源；
build defconfig 管整个产品；
kernel defconfig 管内核功能；
DTS/module_driver 管设备出现；
driver code 管设备怎么工作。
```

对于 BMI160：

```text id="t9gdm9"
IConfigTool：让 I2C/SPI 引脚和控制器能用。
kernel defconfig：让 BMI160 驱动参与编译。
DTS/module_driver：告诉系统板子上真的有 BMI160。
BMI160 driver：实现数据读取、IIO 节点、probe/remove。
```

---

## 9. 你现在最应该做的不是乱改，而是先判断你的工程走哪条路

直接执行：

```bash id="b01ncb"
cd ~/project/x2600

echo "===== 找 DTS ====="
find kernel/kernel module_driver -name "*.dts" -o -name "*.dtsi" | head -100

echo "===== 找 I2C 设备注册 ====="
grep -R "i2c_board_info\|i2c_register_board_info\|i2c_new_client_device\|i2c_new_device" \
kernel/kernel module_driver -n | head -100

echo "===== 找 SPI 设备注册 ====="
grep -R "spi_board_info\|spi_register_device\|spi_new_device" \
kernel/kernel module_driver -n | head -100

echo "===== 找已有 IMU/传感器写法 ====="
grep -R "bmi160\|mpu\|icm\|qmi\|lsm6\|imu\|gyro\|accel" \
kernel/kernel module_driver -n | head -100
```

你看它搜出来的是 DTS 多，还是 `module_driver` 代码多。
这个决定你 BMI160 应该按“标准 DTS 方式”做，还是按“君正模块注册方式”做。

---

## 10. 和标准 Linux 的最大区别

标准 Linux 你一般只关心：

```text id="20c7py"
DTS + Kconfig + Driver
```

君正 X2600 你还要关心：

```text id="1sj1pk"
IConfigTool + build/configs/xxx_defconfig + module_driver + Buildroot
```

所以你现在感觉复杂，是因为它把“产品工程配置”和“Linux 内核配置”拆开了。

你现在这个项目：

```bash id="wni42q"
make x2600n_tiger_nor_5.10_defconfig
```

不是普通 kernel defconfig，而是：

```text id="ov1p9i"
选中整个 X2600 产品工程配置。
```

真正 kernel 配置还是：

```bash id="r8gd0z"
kernel/kernel/arch/mips/configs/x2600_evb_module_base_linux_sfc_nor_defconfig
```

IConfigTool 保存的主要是：

```bash id="mur4pr"
build/configs/x2600n_tiger_nor_5.10_defconfig
```

以及君正模块化驱动相关配置。

所以你后面开发 BMI160，建议按这个顺序来：

```text id="0mk39n"
1. IConfigTool 打开 I2C/SPI 控制器和引脚
2. kernel menuconfig 打开 IIO + BMI160
3. savedefconfig 固化到 kernel arch/mips/configs/xxx_defconfig
4. 添加 DTS 或 module_driver 设备注册
5. make x2600n_tiger_nor_5.10_defconfig
6. make clean_kernel && make kernel
7. 烧 kernel/dtb/rootfs
8. 上板看 dmesg 和 /sys/bus/iio/devices
```

你把它看成“标准 Linux 外面套了一层君正产品配置壳”，就清楚很多了。


-------
---------
--------

可以，这个在 X2600 上应该按 **“平台 SPI 总线 + Linux 通用 BMI160 IIO 驱动”** 这个思路做。

你不要去重写 BMI160 驱动，Linux 里已经有：

```text
drivers/iio/imu/bmi160/
├── bmi160_core.c
├── bmi160_i2c.c
├── bmi160_spi.c
├── bmi160.h
├── Kconfig
└── Makefile
```

你要做的是把它接到君正 X2600 的 SPI 控制器上。

---

# 一、整体流程

你要改 4 类东西：

```text
1. 君正 iconfig/build 配置
   作用：打开 X2600 SPI0 控制器、SPI0 引脚、SPI device helper

2. Linux kernel 配置
   作用：打开 CONFIG_MODULES、IIO、SPI、BMI160_SPI

3. 设备树 DTS
   作用：告诉内核 SPI0 总线上挂了一个 BMI160

4. 编译、烧录、加载、测试
   作用：生成 .ko，板子上 modprobe/insmod，然后看 /sys/bus/iio/devices/
```

你之前 diff 里已经动到了 `build/configs/x2600n_tiger_nor_5.10_defconfig`，里面打开了 `MD_SPI_DEVICE_HELPER`、`MD_SPI_DEVICE`、`MD_X2600_510_SPI`、`MD_X2600_510_SPI0` 以及 SPI0 的 CLK/MISO/MOSI 引脚，这是方向对的；但是你写成了 `MD_SPI_DEVICE:=y` 和 `MD_X2600_510_SPI:=y`，这个不太像 defconfig 语法，建议改成普通 `=y`。

---

# 二、第一步：先把你搞坏的 kernel defconfig 恢复

先别急着加 BMI160，先把平台配置恢复正常。

你的 diff 里最严重的问题是把：

```text
CONFIG_MACH_XBURST2=y
CONFIG_SOC_X2600=y
CONFIG_DT_X2600_MODULE_BASE=y
```

改没了，还打开了：

```text
CONFIG_MIPS_GENERIC=y
CONFIG_MIPS_GENERIC_KERNEL=y
```

这会导致 `war.h` 找不到。这个必须恢复。

执行：

```bash
cd /home/vik/project/x2600

git restore kernel/kernel/arch/mips/configs/x2600_evb_module_base_linux_sfc_nor_defconfig
```

如果 `git restore` 不支持：

```bash
git checkout -- kernel/kernel/arch/mips/configs/x2600_evb_module_base_linux_sfc_nor_defconfig
```

然后确认：

```bash
git diff kernel/kernel/arch/mips/configs/x2600_evb_module_base_linux_sfc_nor_defconfig
```

没有输出才对。

---

# 三、第二步：只改 build defconfig，打开 X2600 SPI0

你现在应该只保留这类修改：

```text
build/configs/x2600n_tiger_nor_5.10_defconfig
```

里面建议这样写：

```text
MD_SPI_DEVICE_HELPER=y
MD_SPI_DEVICE=y

MD_X2600_510_SPI=y
MD_X2600_510_SPI_CLK_RATE=100000000

MD_X2600_510_SPI0=y
MD_X2600_510_SPI0_CLK=PD00
MD_X2600_510_SPI0_MISO=PD02
MD_X2600_510_SPI0_MOSI=PD01
```

注意，不要写：

```text
MD_SPI_DEVICE:=y
MD_X2600_510_SPI:=y
```

改一下：

```bash
cd /home/vik/project/x2600

sed -i 's/MD_SPI_DEVICE:=y/MD_SPI_DEVICE=y/' build/configs/x2600n_tiger_nor_5.10_defconfig
sed -i 's/MD_X2600_510_SPI:=y/MD_X2600_510_SPI=y/' build/configs/x2600n_tiger_nor_5.10_defconfig
```

你这个配置的作用大概是：**让君正平台层生成/启用 SPI0 控制器和相关 pinmux**。

---

# 四、第三步：不要让 IConfigTool 乱改 kernel defconfig

你之前的 diff 里，`projects.ini` 新增了一个 Project12，并且把 kernel defconfig 也加入到了 IConfigTool 的配置路径里。这样很危险，因为它可能把 Linux kernel defconfig 当成 iconfig 配置去改，结果就出现你刚才那种大面积平台配置错乱。

建议恢复：

```bash
cd /home/vik/project/x2600

git restore tools/iconfigtool/IConfigToolApp/projects.ini
```

如果不支持：

```bash
git checkout -- tools/iconfigtool/IConfigToolApp/projects.ini
```

以后原则是：

```text
IConfigTool 可以改 build/configs/x2600n_tiger_nor_5.10_defconfig

不要让它直接改 kernel/kernel/arch/mips/configs/x2600_evb_module_base_linux_sfc_nor_defconfig
```

---

# 五、第四步：打开 Linux kernel 的 BMI160 SPI 驱动

进入 kernel：

```bash
cd /home/vik/project/x2600/kernel/kernel
```

先确认平台没坏：

```bash
grep -E "CONFIG_MACH_XBURST2|CONFIG_SOC_X2600|CONFIG_MIPS_GENERIC|CONFIG_CPU_LITTLE_ENDIAN|CONFIG_CPU_BIG_ENDIAN" .config
```

你希望看到类似：

```text
CONFIG_MACH_XBURST2=y
CONFIG_SOC_X2600=y
# CONFIG_MIPS_GENERIC_KERNEL is not set
CONFIG_CPU_LITTLE_ENDIAN=y
# CONFIG_CPU_BIG_ENDIAN is not set
```

然后打开模块支持、SPI、IIO、BMI160：

```bash
./scripts/config --enable MODULES

./scripts/config --enable IIO
./scripts/config --enable IIO_BUFFER
./scripts/config --enable IIO_TRIGGERED_BUFFER

./scripts/config --enable SPI
./scripts/config --enable SPI_MASTER
./scripts/config --enable REGMAP
./scripts/config --enable REGMAP_SPI

./scripts/config --module BMI160_SPI
```

然后同步配置：

```bash
make olddefconfig
```

再检查：

```bash
grep -E "CONFIG_MODULES|CONFIG_IIO|CONFIG_SPI|CONFIG_REGMAP_SPI|CONFIG_BMI160" .config
```

你希望看到类似：

```text
CONFIG_MODULES=y
CONFIG_IIO=y
CONFIG_SPI=y
CONFIG_SPI_MASTER=y
CONFIG_REGMAP_SPI=y
CONFIG_BMI160=m
CONFIG_BMI160_SPI=m
```

如果 `CONFIG_BMI160_SPI` 变不成 `m`，说明它依赖的 `CONFIG_SPI` 没开成功。

---

# 六、还要确认君正 SPI 控制器驱动是否打开

BMI160 SPI 驱动只是 **SPI 设备驱动**，还需要 X2600 的 **SPI 控制器驱动**。

你可以搜一下君正 SPI 控制器配置名：

```bash
cd /home/vik/project/x2600/kernel/kernel

grep -R "config .*SPI" -n drivers/ arch/mips/ | grep -i ingenic | head -50
```

你之前 diff 里能看到 kernel defconfig 里有这一段：

```text
# [SPI] Master/Slave Drivers
# CONFIG_INGENIC_SPI is not set
# CONFIG_INGENIC_SLV is not set
```

所以大概率要打开：

```text
CONFIG_INGENIC_SPI
```

推荐控制器驱动先编进内核：

```bash
./scripts/config --enable INGENIC_SPI
make olddefconfig
```

再检查：

```bash
grep CONFIG_INGENIC_SPI .config
```

希望看到：

```text
CONFIG_INGENIC_SPI=y
```

为什么控制器建议 `y`，BMI160 建议 `m`？

```text
SPI 控制器 = SoC 总线基础设施，建议内建 y
BMI160 设备驱动 = 外设，方便调试，建议模块 m
```

---

# 七、第五步：设备树添加 BMI160 节点

你这个平台原来 defconfig 里用的是：

```text
CONFIG_DT_X2600_MODULE_BASE_DTS_FILE="x2600_vast_module_base.dts"
```

所以你先找到这个 DTS：

```bash
cd /home/vik/project/x2600/kernel/kernel

find arch/mips -name "x2600_vast_module_base.dts"
```

假设路径是类似：

```text
arch/mips/boot/dts/ingenic/x2600_vast_module_base.dts
```

打开这个 DTS，找到 SPI0 控制器节点，可能叫：

```dts
&spi0 {
};
```

或者类似：

```dts
&ssi0 {
};
```

名字不确定，要用 grep 找：

```bash
grep -R "spi0\|ssi0\|ingenic.*spi" -n arch/mips/boot/dts
```

然后加 BMI160 节点，示例：

```dts
&spi0 {
    status = "okay";

    bmi160@0 {
        compatible = "bosch,bmi160";
        reg = <0>;
        spi-max-frequency = <10000000>;
    };
};
```

解释一下：

```text
&spi0
    表示使用 SPI0 控制器

status = "okay"
    打开 SPI0

bmi160@0
    表示 SPI0 片选 0 上挂了 BMI160

compatible = "bosch,bmi160"
    让 Linux 匹配 bmi160_spi 驱动

reg = <0>
    SPI chip select 0

spi-max-frequency = <10000000>
    SPI 通信最大频率，先保守写 10MHz
```

如果你的 BMI160 有中断脚，比如 INT1 接到了某个 GPIO，还可以后面再加：

```dts
interrupt-parent = <&gpd>;
interrupts = <5 IRQ_TYPE_LEVEL_LOW>;
```

但是新手阶段建议先不加中断，先把 SPI 通信和 IIO 节点跑起来。

---

# 八、第六步：编译 kernel 和模块

回到 build 目录：

```bash
cd /home/vik/project/x2600/build
make kernel
```

编译完成后找 BMI160 模块：

```bash
cd /home/vik/project/x2600

find . -name "*bmi160*.ko"
find . -name "*industrialio*.ko"
```

你可能会看到：

```text
bmi160_core.ko
bmi160_spi.ko
```

也可能只有：

```text
bmi160_spi.ko
```

具体看这个 vendor kernel 怎么组织。

---

# 九、第七步：把模块放到 rootfs

如果你的 buildroot 会自动安装 kernel modules，通常执行完整编译即可：

```bash
cd /home/vik/project/x2600/build
make
```

然后 rootfs 里应该有：

```text
/lib/modules/$(uname -r)/
```

如果没有自动进去，可以手动找输出目录：

```bash
find /home/vik/project/x2600 -path "*lib/modules*" -name "*bmi160*.ko"
```

如果只是临时测试，也可以直接拷到板子：

```bash
scp bmi160_core.ko root@板子IP:/tmp/
scp bmi160_spi.ko root@板子IP:/tmp/
```

---

# 十、第八步：板子上加载测试

上板后先看内核版本：

```bash
uname -a
```

看模块目录：

```bash
ls /lib/modules/
```

加载模块：

```bash
modprobe bmi160_spi
```

如果没有 modprobe，就手动 insmod：

```bash
insmod /tmp/bmi160_core.ko
insmod /tmp/bmi160_spi.ko
```

然后看日志：

```bash
dmesg | grep -i bmi
dmesg | grep -i spi
dmesg | grep -i iio
```

看 IIO 设备：

```bash
ls /sys/bus/iio/devices/
```

正常应该出现：

```text
iio:device0
```

查看名字：

```bash
cat /sys/bus/iio/devices/iio:device0/name
```

可能输出：

```text
bmi160
```

再看通道：

```bash
ls /sys/bus/iio/devices/iio:device0/
```

可能有：

```text
in_accel_x_raw
in_accel_y_raw
in_accel_z_raw
in_anglvel_x_raw
in_anglvel_y_raw
in_anglvel_z_raw
```

读取测试：

```bash
cat /sys/bus/iio/devices/iio:device0/in_accel_x_raw
cat /sys/bus/iio/devices/iio:device0/in_accel_y_raw
cat /sys/bus/iio/devices/iio:device0/in_accel_z_raw
```

如果晃动板子，数值有变化，就说明驱动起来了。

---

# 十一、你现在最推荐的改法

先不要大改 kernel defconfig，只做这些：

## 1. build defconfig

```text
build/configs/x2600n_tiger_nor_5.10_defconfig
```

保留：

```text
MD_SPI_DEVICE_HELPER=y
MD_SPI_DEVICE=y
MD_X2600_510_SPI=y
MD_X2600_510_SPI0=y
MD_X2600_510_SPI0_CLK=PD00
MD_X2600_510_SPI0_MISO=PD02
MD_X2600_510_SPI0_MOSI=PD01
```

## 2. kernel `.config`

增加：

```text
CONFIG_MODULES=y
CONFIG_SPI=y
CONFIG_SPI_MASTER=y
CONFIG_INGENIC_SPI=y
CONFIG_IIO=y
CONFIG_IIO_BUFFER=y
CONFIG_IIO_TRIGGERED_BUFFER=y
CONFIG_REGMAP_SPI=y
CONFIG_BMI160=m
CONFIG_BMI160_SPI=m
```

## 3. DTS

增加：

```dts
&spi0 {
    status = "okay";

    bmi160@0 {
        compatible = "bosch,bmi160";
        reg = <0>;
        spi-max-frequency = <10000000>;
    };
};
```

---

# 十二、调试时按这个顺序排查

如果失败，不要一上来怀疑 BMI160，按顺序查：

```text
1. 内核是否还是 X2600/XBurst2 平台？
   grep CONFIG_MACH_XBURST2 .config
   grep CONFIG_SOC_X2600 .config

2. SPI 控制器有没有起来？
   dmesg | grep -i spi

3. 设备树里有没有 bmi160 节点？
   strings /proc/device-tree/... 或者看 dmesg

4. bmi160_spi.ko 有没有编出来？
   find . -name "*bmi160*.ko"

5. 驱动有没有 probe？
   dmesg | grep -i bmi160

6. IIO 节点有没有出现？
   ls /sys/bus/iio/devices/
```

---

一句话总结：**君正 X2600 上加 BMI160 SPI IIO，不是重写驱动，而是：恢复正确 X2600 平台配置 → 打开 SPI0 → 打开内核 IIO/BMI160_SPI → DTS 挂 `bosch,bmi160` 节点 → 编译模块 → 上板看 `/sys/bus/iio/devices/`。**


--------
--------
-------
可以。你现在最稳的做法是：**先把所有 Git 修改回退到干净状态，然后重新按“最小改动”加入 SPI + IIO + BMI160**。

你之前的 diff 里，`build/configs/x2600n_tiger_nor_5.10_defconfig` 加了 SPI 配置，但写成了 `:=y`，这不建议；更严重的是 `kernel/.../x2600_evb_module_base_linux_sfc_nor_defconfig` 被改成了 `CONFIG_MIPS_GENERIC=y`、`CONFIG_MIPS_GENERIC_KERNEL=y`，并且把原来的 `CONFIG_MACH_XBURST2=y` 改没了，这就是 `war.h` 找不到的根因。  你之前还把 kernel defconfig 加进了 IConfigTool 的 `projects.ini`，这也很危险，因为它可能继续污染 kernel defconfig。

下面按步骤来。

---

# 0. 先备份当前错误改动

虽然你说要全回退，但建议先保存一份补丁，万一后面想看还能看。

```bash
cd /home/vik/project/x2600

git status --short

mkdir -p ~/x2600_backup_patch
git diff > ~/x2600_backup_patch/bad_changes_$(date +%Y%m%d_%H%M%S).patch
git diff --cached > ~/x2600_backup_patch/bad_staged_changes_$(date +%Y%m%d_%H%M%S).patch
```

---

# 1. 回退所有 Git 修改

先回退已暂存和未暂存的修改：

```bash
cd /home/vik/project/x2600

git restore --staged .
git restore .
```

老版本 Git 如果不支持 `git restore`，用：

```bash
cd /home/vik/project/x2600

git reset
git checkout -- .
```

然后看状态：

```bash
git status --short
```

如果还有未跟踪文件，比如你自己生成的临时文件，会显示：

```text
?? xxx
```

先预览要删除哪些未跟踪文件：

```bash
git clean -nd
```

确认没有你想保留的文件后，再执行：

```bash
git clean -fd
```

注意：`git clean -fd` 会删除未被 Git 跟踪的文件，不要乱用在你不确定的目录里。

---

# 2. 重点：恢复 kernel `.config`

Git 回退只能恢复被 Git 管理的文件，但 `kernel/kernel/.config` 很可能不是 Git 管理文件。

你现在报 `war.h`，大概率当前 `.config` 也被污染了，所以必须重建。

执行：

```bash
cd /home/vik/project/x2600/kernel/kernel

mv .config .config.bad 2>/dev/null

make ARCH=mips x2600_evb_module_base_linux_sfc_nor_defconfig
```

然后检查平台配置是否正确：

```bash
grep -E "CONFIG_MACH_XBURST2|CONFIG_SOC_X2600|CONFIG_MIPS_GENERIC|CONFIG_CPU_LITTLE_ENDIAN|CONFIG_CPU_BIG_ENDIAN" .config
```

你希望看到类似：

```text
CONFIG_MACH_XBURST2=y
CONFIG_SOC_X2600=y
CONFIG_CPU_LITTLE_ENDIAN=y
# CONFIG_CPU_BIG_ENDIAN is not set
```

不要看到这种：

```text
CONFIG_MIPS_GENERIC=y
CONFIG_MIPS_GENERIC_KERNEL=y
# CONFIG_MACH_XBURST2 is not set
CONFIG_CPU_BIG_ENDIAN=y
```

如果还是看到 MIPS generic，说明 defconfig 还没恢复干净。

---

# 3. 确认 BMI160 驱动源码存在

你的内核里应该已经有 BMI160 驱动。

```bash
cd /home/vik/project/x2600/kernel/kernel

ls drivers/iio/imu/bmi160
cat drivers/iio/imu/bmi160/Kconfig
```

正常应该看到类似：

```text
bmi160_core.c
bmi160_i2c.c
bmi160_spi.c
bmi160.h
Kconfig
Makefile
```

这里重点是要有：

```text
bmi160_spi.c
```

如果有，就不用自己写 BMI160 驱动。

---

# 4. 重新打开 X2600 平台 SPI0

这一步改的是君正 SDK 的 build 配置，不是 Linux kernel defconfig。

文件：

```text
/home/vik/project/x2600/build/configs/x2600n_tiger_nor_5.10_defconfig
```

你可以直接用 `vim` 改，也可以用命令改。

打开：

```bash
cd /home/vik/project/x2600

vim build/configs/x2600n_tiger_nor_5.10_defconfig
```

找到 SPI 相关位置，改成：

```text
MD_SPI_DEVICE_HELPER=y
MD_SPI_DEVICE=y

MD_X2600_510_SPI=y
MD_X2600_510_SPI_CLK_RATE=100000000

MD_X2600_510_SPI0=y
MD_X2600_510_SPI0_CLK=PD00
MD_X2600_510_SPI0_MISO=PD02
MD_X2600_510_SPI0_MOSI=PD01
```

你之前写的是：

```text
MD_SPI_DEVICE:=y
MD_X2600_510_SPI:=y
```

这个不要用，改成：

```text
MD_SPI_DEVICE=y
MD_X2600_510_SPI=y
```

另外你还要确认 **CS 片选引脚**。SPI 不是只有 CLK/MISO/MOSI，还需要 CS。

查一下配置里有没有 SPI0 CS 选项：

```bash
grep -n "SPI0.*CS\|SPI0.*SS\|SPI0.*SEL\|SPI0.*CHIP" build/Config.in build/configs/x2600n_tiger_nor_5.10_defconfig
```

如果有类似：

```text
MD_X2600_510_SPI0_CS0
```

那就按你的原理图配置它。比如如果 CS 接 PD03，可能是类似：

```text
MD_X2600_510_SPI0_CS0=PD03
```

具体名字以你 grep 出来的为准。

---

# 5. 打开 kernel 里的 IIO + SPI + BMI160

进入 kernel：

```bash
cd /home/vik/project/x2600/kernel/kernel
```

先确认 `scripts/config` 能用：

```bash
ls scripts/config
```

然后执行：

```bash
./scripts/config --enable MODULES
./scripts/config --enable MODULE_UNLOAD

./scripts/config --enable IIO
./scripts/config --enable IIO_BUFFER
./scripts/config --enable IIO_TRIGGERED_BUFFER

./scripts/config --enable SPI
./scripts/config --enable SPI_MASTER
./scripts/config --enable REGMAP
./scripts/config --enable REGMAP_SPI

./scripts/config --module BMI160
./scripts/config --module BMI160_SPI
```

然后同步配置：

```bash
make ARCH=mips olddefconfig
```

检查结果：

```bash
grep -E "CONFIG_MODULES|CONFIG_IIO|CONFIG_SPI|CONFIG_REGMAP_SPI|CONFIG_BMI160" .config
```

你希望看到：

```text
CONFIG_MODULES=y
CONFIG_IIO=y
CONFIG_SPI=y
CONFIG_SPI_MASTER=y
CONFIG_REGMAP_SPI=y
CONFIG_BMI160=m
CONFIG_BMI160_SPI=m
```

如果 `CONFIG_BMI160_SPI` 没变成 `m`，说明依赖没满足。

---

# 6. 打开君正 SPI 控制器驱动

BMI160 是 **SPI 从设备驱动**，还需要 X2600 的 **SPI 控制器驱动**。

先查配置名字：

```bash
cd /home/vik/project/x2600/kernel/kernel

grep -R "config .*SPI" -n drivers arch/mips | grep -i ingenic | head -50
```

如果你看到：

```text
config INGENIC_SPI
```

那就打开它：

```bash
./scripts/config --enable INGENIC_SPI
make ARCH=mips olddefconfig
```

检查：

```bash
grep CONFIG_INGENIC_SPI .config
```

建议 SPI 控制器用 `y`，BMI160 用 `m`：

```text
CONFIG_INGENIC_SPI=y
CONFIG_BMI160_SPI=m
```

原因是：

```text
SPI 控制器：SoC 基础总线，建议编进内核
BMI160：外设驱动，调试阶段建议编成模块
```

---

# 7. 找到当前 X2600 使用的 DTS

你原来的 X2600 defconfig 里使用的是：

```text
CONFIG_DT_X2600_MODULE_BASE_DTS_FILE="x2600_vast_module_base.dts"
```

所以先找这个文件：

```bash
cd /home/vik/project/x2600/kernel/kernel

find arch/mips -name "x2600_vast_module_base.dts"
```

假设找到：

```text
arch/mips/boot/dts/ingenic/x2600_vast_module_base.dts
```

你再找 SPI0 节点叫什么：

```bash
grep -R "spi0\|ssi0\|ingenic.*spi" -n arch/mips/boot/dts | head -100
```

有些厂商会把 SPI 叫 `spi0`，有些可能叫 `ssi0`。你要以实际 DTS 里的节点名为准。

---

# 8. 在 DTS 里添加 BMI160 节点

假设你的 SPI 控制器节点叫 `&spi0`，BMI160 接在 SPI0 的 CS0 上，可以加：

```dts
&spi0 {
    status = "okay";

    bmi160@0 {
        compatible = "bosch,bmi160";
        reg = <0>;
        spi-max-frequency = <1000000>;
    };
};
```

新手阶段建议先用低一点的 SPI 频率：

```text
1000000 = 1MHz
```

等通信确认正常后，再调到：

```text
5000000
10000000
```

如果你的 SPI 控制器没有硬件 CS，或者 CS 用 GPIO 控制，就需要加 `cs-gpios`。比如 CS 接 PD03，DTS 可能类似：

```dts
#include <dt-bindings/gpio/gpio.h>

&spi0 {
    status = "okay";
    cs-gpios = <&gpd 3 GPIO_ACTIVE_LOW>;

    bmi160@0 {
        compatible = "bosch,bmi160";
        reg = <0>;
        spi-max-frequency = <1000000>;
    };
};
```

注意：`&gpd` 这个名字不一定对，要看你们 DTS 里 GPIO 控制器怎么命名。用这个查：

```bash
grep -R "gpd\|gpio" -n arch/mips/boot/dts/ingenic | head -100
```

---

# 9. 编译 kernel

回到 build 目录：

```bash
cd /home/vik/project/x2600/build

make kernel
```

如果这一步又报 `war.h`，说明 kernel `.config` 还是坏的，回到第 2 步重新检查。

编译成功后找 BMI160 模块：

```bash
cd /home/vik/project/x2600

find . -name "*bmi160*.ko"
```

正常应该有类似：

```text
bmi160_core.ko
bmi160_spi.ko
```

也可能路径在：

```text
kernel/kernel/drivers/iio/imu/bmi160/
buildroot/buildroot24/output/target/lib/modules/...
```

---

# 10. 编译完整系统，把模块放进 rootfs

只编 `make kernel` 不一定会把 `.ko` 安装到 rootfs。

建议再执行：

```bash
cd /home/vik/project/x2600/build

make
```

然后查 rootfs 里有没有模块：

```bash
cd /home/vik/project/x2600

find . -path "*lib/modules*" -name "*bmi160*.ko"
```

如果 rootfs 里没有，就临时手动拷贝到板子测试。

---

# 11. 上板测试

板子启动后，先看 SPI 控制器有没有起来：

```bash
dmesg | grep -i spi
```

再看模块：

```bash
find /lib/modules -name "*bmi160*.ko"
```

如果有 `modprobe`：

```bash
modprobe bmi160_spi
```

如果没有 `modprobe`，手动加载：

```bash
insmod /lib/modules/$(uname -r)/kernel/drivers/iio/imu/bmi160/bmi160_core.ko
insmod /lib/modules/$(uname -r)/kernel/drivers/iio/imu/bmi160/bmi160_spi.ko
```

如果路径不一样，用：

```bash
find /lib/modules -name "*bmi160*.ko"
```

然后看日志：

```bash
dmesg | grep -i bmi
dmesg | grep -i iio
dmesg | grep -i spi
```

看 IIO 设备：

```bash
ls /sys/bus/iio/devices/
```

如果成功，会看到：

```text
iio:device0
```

查看名字：

```bash
cat /sys/bus/iio/devices/iio:device0/name
```

应该类似：

```text
bmi160
```

读取数据：

```bash
cat /sys/bus/iio/devices/iio:device0/in_accel_x_raw
cat /sys/bus/iio/devices/iio:device0/in_accel_y_raw
cat /sys/bus/iio/devices/iio:device0/in_accel_z_raw

cat /sys/bus/iio/devices/iio:device0/in_anglvel_x_raw
cat /sys/bus/iio/devices/iio:device0/in_anglvel_y_raw
cat /sys/bus/iio/devices/iio:device0/in_anglvel_z_raw
```

如果你晃动 IMU，数值变化，说明基本成功。

---

# 12. 推荐你最后检查 Git diff

重新加入后，正常 Git diff 应该只包含这些文件：

```bash
cd /home/vik/project/x2600

git diff --name-only
```

理想情况类似：

```text
build/configs/x2600n_tiger_nor_5.10_defconfig
kernel/kernel/arch/mips/configs/x2600_evb_module_base_linux_sfc_nor_defconfig
kernel/kernel/arch/mips/boot/dts/ingenic/x2600_vast_module_base.dts
```

不要再出现：

```text
tools/iconfigtool/IConfigToolApp/projects.ini
.vscode/settings.json
```

尤其不要让 `x2600_evb_module_base_linux_sfc_nor_defconfig` 出现几千行巨大 diff。

你可以检查 kernel defconfig diff：

```bash
git diff kernel/kernel/arch/mips/configs/x2600_evb_module_base_linux_sfc_nor_defconfig
```

正常应该只是增加少量类似：

```text
CONFIG_MODULES=y
CONFIG_MODULE_UNLOAD=y
CONFIG_IIO=y
CONFIG_SPI=y
CONFIG_REGMAP_SPI=y
CONFIG_BMI160=m
CONFIG_BMI160_SPI=m
CONFIG_INGENIC_SPI=y
```

如果又看到：

```text
CONFIG_MIPS_GENERIC=y
CONFIG_MIPS_GENERIC_KERNEL=y
# CONFIG_MACH_XBURST2 is not set
```

立刻停下，说明又改坏了。

---

# 13. 你现在可以直接照这个顺序执行

```bash
cd /home/vik/project/x2600

# 备份当前错误改动
mkdir -p ~/x2600_backup_patch
git diff > ~/x2600_backup_patch/bad_changes_$(date +%Y%m%d_%H%M%S).patch

# 回退 Git 修改
git restore --staged .
git restore .

# 预览未跟踪文件
git clean -nd
```

确认没问题后：

```bash
git clean -fd
```

重建 kernel `.config`：

```bash
cd /home/vik/project/x2600/kernel/kernel

mv .config .config.bad 2>/dev/null
make ARCH=mips x2600_evb_module_base_linux_sfc_nor_defconfig

grep -E "CONFIG_MACH_XBURST2|CONFIG_SOC_X2600|CONFIG_MIPS_GENERIC|CONFIG_CPU_LITTLE_ENDIAN|CONFIG_CPU_BIG_ENDIAN" .config
```

打开内核功能：

```bash
./scripts/config --enable MODULES
./scripts/config --enable MODULE_UNLOAD
./scripts/config --enable IIO
./scripts/config --enable IIO_BUFFER
./scripts/config --enable IIO_TRIGGERED_BUFFER
./scripts/config --enable SPI
./scripts/config --enable SPI_MASTER
./scripts/config --enable REGMAP
./scripts/config --enable REGMAP_SPI
./scripts/config --enable INGENIC_SPI
./scripts/config --module BMI160
./scripts/config --module BMI160_SPI

make ARCH=mips olddefconfig

grep -E "CONFIG_MODULES|CONFIG_IIO|CONFIG_SPI|CONFIG_INGENIC_SPI|CONFIG_REGMAP_SPI|CONFIG_BMI160" .config
```

然后改 DTS 加 `bmi160@0`，最后：

```bash
cd /home/vik/project/x2600/build

make kernel
```

一句话总结：**先彻底回退，重建正确的 X2600 `.config`，然后只做三类最小改动：build defconfig 打开 SPI0，kernel config 打开 IIO/BMI160_SPI/INGENIC_SPI，DTS 在 SPI0 下挂 `bosch,bmi160` 节点。**


-------
---------
--------
你这个输出先别慌，**你这次 grep 没搜到关键内容，不代表项目没有设备注册方式**。主要问题是你搜得太宽了，结果被这些无关目录抢占了：

```text
kernel/kernel/scripts/
kernel/kernel/arch/arm64/
kernel/kernel/arch/arm/
kernel/kernel/scripts/dtc/include-prefixes/
```

你现在是 X2600，MIPS 平台，所以应该重点搜：

```text
kernel/kernel/arch/mips/
kernel/kernel/drivers/
module_driver/
```

你这份输出里大部分都是 kernel 自带脚本和别的平台 DTS，不是你板子的代码。

---

# 先讲核心结论

你要加 **IIO + SPI + BMI160 IMU**，一般不是重新写一个完整 BMI160 驱动。

Linux 里已经有：

```bash
kernel/kernel/drivers/iio/imu/bmi160/
```

你真正要做的是两件事：

```text
1. 让 BMI160_SPI 驱动被编译进去
2. 让系统知道 SPI 总线上挂了一个 BMI160 设备
```

第 1 件事是 kernel Kconfig/defconfig。

第 2 件事有两种方式：

```text
方式 A：标准 DTS 方式
在设备树里添加 bmi160 节点。

方式 B：君正 module_driver / 板级注册方式
写一个小模块，用 spi_new_device / spi_register_device 注册一个 SPI 设备。
```

你现在问的 **“君正 module_driver 注册设备咋操作”**，本质就是方式 B。

---

# 一、先确认 BMI160 SPI 驱动匹配名

你不能乱写 `modalias`。SPI 设备注册时的名字必须和 BMI160 SPI 驱动里的 `spi_device_id` 对上。

先执行：

```bash
cd ~/project/x2600/kernel/kernel

grep -n "spi_device_id\|bmi160_spi_id\|of_device_id\|compatible" \
drivers/iio/imu/bmi160/bmi160_spi.c -A30
```

你大概率会看到类似：

```c
static const struct spi_device_id bmi160_spi_id[] = {
    { "bmi160", 0 },
    { }
};
```

如果是这样，后面注册 SPI 设备的时候就应该写：

```c
.modalias = "bmi160",
```

不是随便写 `"bmi160_spi"`。

---

# 二、先用更准确的 grep 找 X2600 自己的设备注册方式

你之前这个命令太宽：

```bash
grep -R "i2c_board_info\|i2c_register_board_info\|of_device_id\|compatible.*bmi\|spi_register_device\|spi_board_info" \
kernel/kernel module_driver -n | head -100
```

建议换成这组：

```bash
cd ~/project/x2600

echo "===== 1. 搜君正/MIPS相关DTS ====="
find kernel/kernel/arch/mips module_driver -name "*.dts" -o -name "*.dtsi" | head -100

echo
echo "===== 2. 搜X2600/X2000/XBurst相关DTS内容 ====="
grep -R "x2600\|x2000\|ingenic\|jz\|ssi\|spi\|i2c" \
kernel/kernel/arch/mips module_driver -n \
--include="*.dts" --include="*.dtsi" | head -100

echo
echo "===== 3. 搜SPI设备注册代码，排除无关scripts ====="
grep -R "spi_board_info\|spi_new_device\|spi_register_device\|spi_busnum_to_master" \
kernel/kernel/arch/mips kernel/kernel/drivers module_driver -n \
--exclude-dir=scripts \
--exclude-dir=.git \
--exclude-dir=Documentation | head -100

echo
echo "===== 4. 搜I2C设备注册代码 ====="
grep -R "i2c_board_info\|i2c_register_board_info\|i2c_new_device\|i2c_new_client_device" \
kernel/kernel/arch/mips kernel/kernel/drivers module_driver -n \
--exclude-dir=scripts \
--exclude-dir=.git \
--exclude-dir=Documentation | head -100
```

重点看第 3 个命令有没有搜到君正自己的示例。

如果还是没有，那你就直接按君正 SPI 文档的方式写一个“注册 BMI160 SPI 设备的小模块”。你之前上传的君正 SPI 文档里，官方就是用 `spi_register_device()` / `spi_register_driver()` 这种板级注册方式做 SPI 设备添加示例的。

---

# 三、BMI160 不建议自己再写 `spi_driver`

官方文档里的示例是这样：

```text
自己写 spi_driver
自己写 spi_board_info
自己注册 spi_device
```

但是你这里不一样。

因为 BMI160 已经有现成驱动：

```bash
drivers/iio/imu/bmi160/bmi160_spi.c
drivers/iio/imu/bmi160/bmi160_core.c
```

所以你不要重复写：

```c
static struct spi_driver xxx_driver = { ... };
```

你只需要注册一个 SPI 设备，让它匹配到已有的 `bmi160_spi_driver`。

也就是：

```text
你写的小模块：负责创建 SPI device
Linux 自带 BMI160 SPI 驱动：负责 probe、读写寄存器、注册 IIO 设备
```

关系是这样：

```text
bmi160_spi_board.ko
    |
    | spi_new_device()
    v
创建一个 SPI device：modalias = "bmi160"
    |
    | SPI core 自动匹配
    v
drivers/iio/imu/bmi160/bmi160_spi.c
    |
    | probe()
    v
注册 IIO 设备
/sys/bus/iio/devices/iio:deviceX/
```

---

# 四、写一个 BMI160 SPI 设备注册模块

可以先建一个简单测试模块，比如：

```bash
cd ~/project/x2600/module_driver
mkdir -p bmi160_spi_board
cd bmi160_spi_board
```

创建文件：

```bash
vim bmi160_spi_board.c
```

内容如下，注意里面的 SPI 总线号、CS 引脚你要按你的硬件改：

```c
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/spi/spi.h>
#include <linux/errno.h>
#include <soc/gpio.h>

/*
 * 这个模块只负责“声明板子上有一个 BMI160 SPI 设备”。
 * 真正的 BMI160 驱动在 kernel/kernel/drivers/iio/imu/bmi160/bmi160_spi.c
 */

/*
 * TODO: 改成你的实际 SPI 总线号。
 * 例如 SPI0 就是 0，SPI1 就是 1。
 */
static int spi_bus_num = 0;

/*
 * 保存 spi_new_device() 创建出来的设备指针，
 * 模块卸载时要用 spi_unregister_device() 注销。
 */
static struct spi_device *bmi160_spi_dev;

/*
 * SPI 设备板级信息。
 *
 * modalias 必须和 bmi160_spi.c 里的 spi_device_id 匹配。
 * 你要先 grep 确认：
 * grep -n "spi_device_id" drivers/iio/imu/bmi160/bmi160_spi.c -A20
 *
 * 如果里面是 { "bmi160", 0 }，这里就写 "bmi160"。
 */
static struct spi_board_info bmi160_spi_board_info = {
    .modalias = "bmi160",

    /*
     * BMI160 SPI 最高频率先不要设置太高。
     * 调通阶段建议 1MHz 或 2MHz。
     */
    .max_speed_hz = 1 * 1000 * 1000,

    /*
     * BMI160 常见 SPI mode 是 MODE_0。
     * 如果你板级原理图或者芯片手册要求别的模式，再改。
     */
    .mode = SPI_MODE_0,

    /*
     * 同一条 SPI 总线上的片选编号。
     * 如果只有一个设备，一般用 0。
     */
    .chip_select = 0,

    /*
     * 君正文档里用 controller_data 指定片选 GPIO。
     * 这里 GPIO_PC(16) 只是例子。
     *
     * 你必须改成 BMI160 实际 CS 引脚。
     * 例如 PC16、PC09 之类。
     */
    .controller_data = (void *)GPIO_PC(16),
};

static int __init bmi160_spi_board_init(void)
{
    struct spi_master *master;

    pr_info("bmi160_spi_board: init, bus=%d\n", spi_bus_num);

    /*
     * 1. 根据总线号获取 SPI master。
     * 如果这里失败，说明 SPI 控制器没有注册成功，
     * 要回去检查 IConfigTool 是否打开 SPI 控制器、pinmux 是否正确。
     */
    master = spi_busnum_to_master(spi_bus_num);
    if (!master) {
        pr_err("bmi160_spi_board: failed to get spi master %d\n", spi_bus_num);
        return -ENODEV;
    }

    /*
     * 2. 设置 bus_num。
     */
    bmi160_spi_board_info.bus_num = spi_bus_num;

    /*
     * 3. 创建 SPI 设备。
     * 创建成功后，SPI core 会尝试匹配 modalias = "bmi160" 的驱动。
     */
    bmi160_spi_dev = spi_new_device(master, &bmi160_spi_board_info);

    /*
     * 4. 释放 master 引用。
     */
    spi_master_put(master);

    if (!bmi160_spi_dev) {
        pr_err("bmi160_spi_board: spi_new_device failed\n");
        return -ENODEV;
    }

    pr_info("bmi160_spi_board: registered bmi160 on spi%d.%d\n",
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

MODULE_DESCRIPTION("BMI160 SPI board device registration for Ingenic X2600");
MODULE_AUTHOR("Vik");
MODULE_LICENSE("GPL");
```

---

# 五、这个模块和 BMI160 驱动的关系

你需要让这几个模块/配置同时存在：

```text
1. SPI master 驱动
   让 /sys/bus/spi/devices/ 里面能出现 SPI 控制器

2. BMI160 SPI 驱动
   drivers/iio/imu/bmi160/bmi160_spi.c

3. 你这个 bmi160_spi_board 小模块
   负责创建 spi0.0 这个设备
```

如果 BMI160 SPI 是编成模块，那么加载顺序可以是：

```bash
insmod bmi160_core.ko
insmod bmi160_spi.ko
insmod bmi160_spi_board.ko
```

也可以先注册 board，再加载 driver。SPI core 后面也会匹配。

---

# 六、先别急着集成 module_driver，先单独编译测试

最稳的方式是先把这个小模块当普通 kernel module 编译出来，推到板子上验证。

创建 `Makefile`：

```makefile
obj-m += bmi160_spi_board.o

KERNEL_DIR ?= /home/vik/project/x2600/kernel/kernel
CROSS_COMPILE ?= /home/vik/project/x2600/build/../buildroot/buildroot24/output/host/usr/bin/mips-linux-gnu-

all:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) ARCH=mips CROSS_COMPILE=$(CROSS_COMPILE) modules

clean:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) ARCH=mips CROSS_COMPILE=$(CROSS_COMPILE) clean
```

然后：

```bash
cd ~/project/x2600/module_driver/bmi160_spi_board

make
```

看是否生成：

```bash
ls -l *.ko
```

---

# 七、上板测试

假设生成了：

```bash
bmi160_spi_board.ko
```

推到板子：

```bash
adb push bmi160_spi_board.ko /tmp/
```

如果 BMI160 驱动是模块，也推上去：

```bash
cd ~/project/x2600/kernel/kernel

find . -name "*bmi160*.ko"
```

比如：

```bash
adb push drivers/iio/imu/bmi160/bmi160_core.ko /tmp/
adb push drivers/iio/imu/bmi160/bmi160_spi.ko /tmp/
```

板子上：

```bash
cd /tmp

insmod bmi160_core.ko
insmod bmi160_spi.ko
insmod bmi160_spi_board.ko

dmesg | grep -i "bmi160\|spi\|iio"
```

然后看 IIO：

```bash
ls /sys/bus/iio/devices/

for f in /sys/bus/iio/devices/iio:device*/name; do
    echo "$f"
    cat "$f"
done
```

如果成功，应该能看到类似：

```text
bmi160
```

然后读数据：

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

# 八、如果你想集成进君正 module_driver 工程

等单独 `.ko` 验证成功后，再集成进君正 `module_driver`。

因为我现在还没看到你 `module_driver` 目录结构，所以不能乱编具体路径。你先执行：

```bash
cd ~/project/x2600

echo "===== module_driver 目录结构 ====="
find module_driver -maxdepth 3 -type f | head -100

echo
echo "===== module_driver 里的 Makefile/mk ====="
find module_driver -maxdepth 4 \( -name "Makefile" -o -name "*.mk" -o -name "Kconfig" -o -name "Config.in" \) -print

echo
echo "===== 搜 MD_ 配置 ====="
grep -R "MD_.*=" build module_driver package -n \
--include="*.mk" --include="Config.in" --include="Kconfig" --include="*_defconfig" | head -100
```

你需要找一个已经存在的小模块，比如 gpio key、rtc、sensor、spidev 之类，模仿它的目录结构。

君正这类工程通常是：

```text
module_driver/xxx/
    xxx.c
    Makefile
    Config.in 或 Kconfig

build/configs/x2600n_tiger_nor_5.10_defconfig
    MD_XXX=y

package/default_driver_init_order.mk
    控制加载顺序

package/S11module_driver_default
    控制开机 insmod 顺序
```

你这个 build 总配置里已经有：

```bash
APP_module_driver=y
MD_INIT_ORDER_FILE=package/default_driver_init_order.mk
MD_INIT_START_FILE=package/S11module_driver_default
```

说明它确实有一个“模块化驱动自动编译/自动加载”的框架。

---

# 九、为什么你不能只打开 BMI160 驱动？

因为只打开：

```text
CONFIG_BMI160_SPI=y
```

只是让驱动存在。

但是 Linux 还不知道板子上有这个硬件。

必须再有一个东西创建这个设备：

```text
DTS 方式：
bmi160@0 {
    compatible = "bosch,bmi160";
    reg = <0>;
};

或者 module_driver 方式：
spi_new_device(master, &bmi160_spi_board_info);
```

否则 `bmi160_spi_probe()` 不会执行。

---

# 十、SPI BMI160 的几个坑

## 1. CS 引脚一定要对

这个地方你必须改：

```c
.controller_data = (void *)GPIO_PC(16),
```

`GPIO_PC(16)` 只是我按君正文档风格写的例子。官方 SPI 文档里也是通过 `controller_data` 指定片选 GPIO。

你要根据原理图确认 BMI160 的 CS 接到了哪个 GPIO。

---

## 2. SPI mode 可能要改

先用：

```c
.mode = SPI_MODE_0,
```

如果读不到 ID，再试：

```c
.mode = SPI_MODE_3,
```

官方 SPI 示例里用了 `SPI_MODE_3`，但 BMI160 通常常见用法可能是 MODE_0，最终以你的硬件和驱动实际通信为准。

---

## 3. 频率先低后高

调试阶段不要直接 10MHz。

建议先：

```c
.max_speed_hz = 1 * 1000 * 1000,
```

稳定后再提高。

---

## 4. 要确认 SPI 控制器已经出来

板子上看：

```bash
ls /sys/bus/spi/devices/
ls /sys/class/spi_master/
dmesg | grep -i spi
```

如果连 SPI master 都没有，那说明 IConfigTool / pinmux / SPI 控制器没有配好。

---

# 十一、你现在按这个最短路径做

先别纠结完整集成，先跑通：

```text
1. IConfigTool 打开 SPI 控制器和引脚
2. kernel defconfig 打开：
   CONFIG_SPI
   CONFIG_IIO
   CONFIG_BMI160
   CONFIG_BMI160_SPI
   CONFIG_REGMAP_SPI

3. 单独写 bmi160_spi_board.ko
4. 推到板子 insmod
5. 看 dmesg 是否 probe
6. 看 /sys/bus/iio/devices 是否出现 bmi160
```

等这个跑通了，再集成进 `module_driver` 自动加载体系。

你现在最该做的下一步是执行：

```bash
cd ~/project/x2600/kernel/kernel

grep -n "spi_device_id\|bmi160_spi_id\|of_device_id\|compatible" \
drivers/iio/imu/bmi160/bmi160_spi.c -A30
```

确认 `modalias` 到底应该写 `"bmi160"` 还是别的。然后再根据你的原理图，把 `spi_bus_num`、`GPIO_PC(16)`、`chip_select` 改成真实值。
