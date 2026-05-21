可以裁剪，而且**嵌入式 Linux 项目基本都必须裁剪**。你现在感觉“Linux 内核每次编译很慢、代码太庞大”，这很正常。裁剪主要分两块：

1. **裁剪 Linux Kernel**：少编译不用的驱动、协议、文件系统、调试功能。
2. **裁剪 RootFS / Buildroot**：少放不用的命令、库、应用、服务。

你可以先记住一句话：

> Linux 裁剪不是随便删除源码文件，而是通过 `menuconfig / defconfig / buildroot config` 关闭不需要的功能。

---

# 一、Linux 系统到底哪里“庞大”？

嵌入式 Linux 一般由这些部分组成：

```text
Bootloader    例如 U-Boot
    ↓
Linux Kernel  内核：调度、内存、驱动、文件系统、网络协议等
    ↓
RootFS        根文件系统：busybox、libc、应用程序、配置文件
    ↓
App           你自己的应用程序
```

你说的“Linux 内核编译慢”，主要是 **Kernel** 这部分慢。

但系统体积大，不一定全是 kernel 的问题，也可能是 rootfs 里塞了很多库和工具。

---

# 二、Kernel 裁剪的核心：`.config`

Linux 内核编译靠一个配置文件控制：

```text
kernel/.config
```

里面有很多配置项，例如：

```text
CONFIG_USB=y
CONFIG_SPI=y
CONFIG_I2C=y
CONFIG_EXT4_FS=y
CONFIG_DEBUG_KERNEL=y
CONFIG_VIDEO_V4L2=y
```

它们有三种状态：

| 状态       | 含义          |
| -------- | ----------- |
| `y`      | 编译进内核镜像     |
| `m`      | 编译成模块 `.ko` |
| `n` / 没有 | 不编译         |

例如：

```text
CONFIG_USB=n
```

就表示 USB 相关功能不编译。

---

# 三、裁剪 Linux Kernel 的常用入口

进入 kernel 目录，一般有这些命令：

```bash
make menuconfig
```

或者带架构和交叉编译器：

```bash
make ARCH=mips menuconfig
```

如果是 ARM：

```bash
make ARCH=arm menuconfig
```

如果是 ARM64：

```bash
make ARCH=arm64 menuconfig
```

你君正 X2600 这种 SDK，一般是 MIPS / XBurst2 体系，通常不是你直接裸跑 `make menuconfig`，而是通过厂商 SDK 的 build 脚本间接配置。

你可以先找这些文件：

```bash
find kernel -name "*defconfig*"
find build -name "*config*"
find . -name ".config"
```

重点看：

```text
kernel/arch/mips/configs/
build/configs/
buildroot/configs/
```

---

# 四、裁剪 Kernel 的正确流程

不要一上来乱关配置。推荐流程是：

```text
1. 先用厂商默认配置，把系统正常跑起来
2. 确认你的硬件需求
3. 进入 menuconfig 逐步关闭不用的功能
4. 每次少关一部分
5. 编译、烧录、启动测试
6. 保存 defconfig
```

比如你做一个摄像头产品，可能需要：

```text
必须保留：
- MIPS CPU 支持
- 中断控制器
- 时钟 clk
- pinctrl / gpio
- i2c
- spi
- mipi csi
- isp
- v4l2
- framebuffer / drm
- mmc / nand / spi-nor
- ext4 / squashfs / ubifs
- serial console
- network 可选
```

如果你做一个没有 USB、没有蓝牙、没有 WiFi、没有声卡的设备，那这些可以考虑裁掉：

```text
USB
Bluetooth
WiFi
Sound
HID
Input 里不用的键盘鼠标触摸板
不用的网卡驱动
不用的摄像头 sensor 驱动
不用的文件系统
不用的调试功能
```

---

# 五、哪些 Kernel 功能常见可以裁剪？

## 1. 不用的设备驱动

在 `menuconfig` 里一般路径是：

```text
Device Drivers  --->
```

常见可以裁剪：

```text
USB support
Bluetooth support
Sound card support
WiFi drivers
Ethernet drivers
Touchscreen drivers
Keyboard / Mouse drivers
LED drivers
PWM drivers
RTC drivers
Camera sensor drivers
```

但是注意：
**不是所有 Device Drivers 都能关。**

比如下面这些经常是系统启动必须的：

```text
GPIO
Pinctrl
Clock driver
Reset controller
Interrupt controller
DMA
MTD / NAND / SPI NOR
MMC / SD
Serial driver
I2C
SPI
Regulator
Power management
```

这些别乱关。

---

## 2. 不用的文件系统

路径大概是：

```text
File systems  --->
```

常见文件系统：

| 文件系统     | 用途               |
| -------- | ---------------- |
| ext4     | SD 卡、eMMC 常用     |
| squashfs | 只读压缩 rootfs 常用   |
| jffs2    | NOR Flash 老方案    |
| ubifs    | NAND Flash 常用    |
| vfat     | FAT32，U 盘/SD 卡常用 |
| nfs      | 网络挂载调试用          |
| procfs   | 必须保留             |
| sysfs    | 必须保留             |
| tmpfs    | 常用，建议保留          |
| devtmpfs | 建议保留             |

可以裁剪：

```text
NTFS
HFS
HFS+
BTRFS
F2FS
NFS 如果产品不用网络挂载
CIFS/SMB
ISO9660 光盘文件系统
```

但这些一般要保留：

```text
CONFIG_PROC_FS
CONFIG_SYSFS
CONFIG_TMPFS
CONFIG_DEVTMPFS
```

---

## 3. 不用的网络协议

路径：

```text
Networking support  --->
```

如果产品不联网，可以裁掉很多：

```text
IPv6
WiFi
Bluetooth
CAN
PPP
Bridge
VLAN
Netfilter/iptables
```

如果你只用简单 TCP/IP，可以保留：

```text
CONFIG_INET
CONFIG_NET
CONFIG_UNIX
```

有些本地通信也依赖 Unix Socket，所以 `CONFIG_UNIX` 不建议乱关。

---

## 4. 调试功能可以大量裁剪

路径：

```text
Kernel hacking  --->
```

开发阶段可以开，量产阶段可以关一部分：

```text
CONFIG_DEBUG_KERNEL
CONFIG_DEBUG_INFO
CONFIG_KALLSYMS
CONFIG_FTRACE
CONFIG_LOCKDEP
CONFIG_DEBUG_SPINLOCK
CONFIG_DEBUG_MUTEXES
CONFIG_SCHED_DEBUG
CONFIG_FUNCTION_TRACER
```

这些会让内核变大，编译也更慢。

但开发阶段不建议全关，不然崩溃时很难查问题。

推荐：

```text
开发阶段：保留 printk、kallsyms、基本 debug
量产阶段：关闭 DEBUG_INFO、FTRACE、LOCKDEP 等重型调试项
```

---

# 六、RootFS 也要裁剪，不只是 Kernel

很多新手只盯着 kernel，其实 rootfs 也很大。

你的 SDK 里有：

```text
buildroot
```

那 rootfs 很可能由 Buildroot 生成。

Buildroot 也有配置：

```bash
make menuconfig
```

或者厂商封装成：

```bash
make buildroot-menuconfig
```

具体要看你 SDK 的 build 脚本。

RootFS 里可以裁：

```text
不用的 shell 工具
不用的网络工具
不用的 python
不用的 openssl
不用的 ffmpeg
不用的 gstreamer
不用的 alsa
不用的 qt
不用的 opencv
不用的调试工具
不用的 demo 程序
```

如果产品只跑一个应用，rootfs 可以非常小：

```text
busybox
libc
必要动态库
设备节点
启动脚本
你的 app
配置文件
```

---

# 七、裁剪前先搞清楚“你的产品需要什么”

比如你现在是君正 X2600，假设你要做摄像头产品，可能需要这些：

```text
必须：
- 串口控制台
- flash / mmc 启动
- i2c 连接 sensor
- mipi csi
- isp
- v4l2
- video buffer
- gpio / pinctrl / clk
- dma
- framebuffer 或者显示模块
- rootfs 文件系统
```

可能不需要：

```text
USB
Bluetooth
WiFi
声卡
复杂桌面 GUI
打印机驱动
键盘鼠标
大量网卡驱动
大量文件系统
IPv6
iptables
NFS
```

如果是摄像头相关，千万别乱裁这些：

```text
CONFIG_MEDIA_SUPPORT
CONFIG_VIDEO_DEV
CONFIG_V4L2
CONFIG_VIDEOBUF2_CORE
CONFIG_VIDEOBUF2_DMA_CONTIG
I2C
MIPI CSI 控制器驱动
ISP 驱动
Sensor 驱动
DMA
```

---

# 八、怎么让 Kernel 编译变快？

裁剪可以变快，但更重要的是不要每次全量编译。

## 1. 不要动不动 `make clean` / `fullclean`

很多新手编译慢，是因为每次都全清了。

这些会导致重新编译大量代码：

```bash
make clean
make distclean
make mrproper
make fullclean
```

平时开发驱动时，尽量增量编译。

---

## 2. 用多线程编译

```bash
make -j$(nproc)
```

例如你的电脑 8 核：

```bash
make -j8
```

不要只用：

```bash
make
```

那样可能只开一个线程，很慢。

---

## 3. 驱动尽量编译成模块

比如你开发某个驱动，可以配置成：

```text
CONFIG_XXX=m
```

这样只生成：

```text
xxx.ko
```

修改驱动后可以只编译模块，不用整个 kernel 镜像都重编。

常见命令：

```bash
make ARCH=mips M=drivers/xxx modules
```

例如：

```bash
make ARCH=mips M=drivers/media/i2c modules
```

这样比全量编译快很多。

---

## 4. 使用 ccache

安装：

```bash
sudo apt install ccache
```

查看：

```bash
ccache -s
```

有些 SDK 支持：

```bash
export CCACHE=ccache
```

或者：

```bash
export PATH="/usr/lib/ccache:$PATH"
```

`ccache` 的作用是缓存编译结果。
你第二次编译同样的文件，会快很多。

---

## 5. 不要频繁改公共头文件

比如你改了这种文件：

```text
include/linux/xxx.h
include/uapi/xxx.h
arch/mips/include/xxx.h
```

可能导致大量源码重新编译。

如果你只是调一个驱动，尽量只改该驱动自己的 `.c/.h` 文件。

---

# 九、实际裁剪示例

假设你不需要 USB：

```text
Device Drivers  --->
    USB support  --->
        < > Support for Host-side USB
```

对应 `.config` 可能变成：

```text
# CONFIG_USB_SUPPORT is not set
```

假设你不需要蓝牙：

```text
Networking support  --->
    Bluetooth subsystem support  --->
        [ ] Bluetooth subsystem support
```

假设你不需要 IPv6：

```text
Networking support  --->
    Networking options  --->
        [ ] The IPv6 protocol
```

假设你不需要大量调试：

```text
Kernel hacking  --->
    [ ] Compile the kernel with debug info
    [ ] Tracers
    [ ] Lock debugging
```

---

# 十、保存裁剪后的配置

你用 `menuconfig` 改完后，会生成：

```text
kernel/.config
```

但 `.config` 不适合直接长期维护。

推荐生成 defconfig：

```bash
make ARCH=mips savedefconfig
```

会生成：

```text
defconfig
```

然后你可以保存成：

```bash
cp defconfig arch/mips/configs/x2600_myboard_defconfig
```

以后恢复配置：

```bash
make ARCH=mips x2600_myboard_defconfig
```

这就是专业的做法。

---

# 十一、裁剪最容易踩的坑

## 坑 1：把串口裁掉了

结果启动后黑屏，没有任何日志。

必须保留：

```text
Serial driver
Console on serial port
early printk / earlycon 可选
```

---

## 坑 2：把 rootfs 文件系统裁掉了

比如你的 rootfs 是 squashfs，但是你关了：

```text
CONFIG_SQUASHFS
```

结果启动报：

```text
VFS: Unable to mount root fs
Kernel panic - not syncing
```

---

## 坑 3：把 flash / mmc 驱动裁掉了

比如系统从 SD/eMMC/NAND 启动，你把对应驱动关了，内核就找不到 rootfs。

常见报错：

```text
VFS: Cannot open root device
unknown-block(0,0)
```

---

## 坑 4：把 devtmpfs 裁掉了

如果没有 `/dev/console`，系统可能起不来。

建议保留：

```text
CONFIG_DEVTMPFS
CONFIG_DEVTMPFS_MOUNT
```

---

## 坑 5：把 proc/sysfs 裁掉了

很多工具依赖：

```text
/proc
/sys
```

比如：

```bash
ps
top
ifconfig
lsmod
dmesg
```

建议保留：

```text
CONFIG_PROC_FS
CONFIG_SYSFS
```

---

# 十二、建议你的裁剪顺序

不要一口气乱裁。建议按这个顺序来：

```text
第一步：先不要动 CPU/内存/总线/存储相关配置
第二步：裁掉明显不用的外设驱动
第三步：裁掉不用的网络协议
第四步：裁掉不用的文件系统
第五步：裁掉重型调试功能
第六步：裁剪 rootfs 里的包
第七步：优化编译速度
```

优先裁这些：

```text
USB
Bluetooth
WiFi
Sound
不相关网卡驱动
不相关 camera sensor 驱动
不相关触摸屏驱动
不相关文件系统
IPv6
iptables
NFS
debug info
ftrace
lockdep
```

先别动这些：

```text
arch/mips
timer
irq
clk
pinctrl
gpio
dma
i2c
spi
mtd
mmc
serial
procfs
sysfs
devtmpfs
rootfs 对应文件系统
```

---

# 十三、针对你这种嵌入式 Linux 新手的建议

你可以把 Linux 裁剪理解成 MCU 里的“组件开关”。

MCU 工程里你可能会这样做：

```text
不用 ADC，就不初始化 ADC
不用 SPI，就不加 SPI 驱动文件
不用 USB，就不加 USB 协议栈
```

Linux 里类似，只不过不是手动删 `.c` 文件，而是通过：

```text
menuconfig
.config
defconfig
Kconfig
Makefile
```

来控制哪些源码参与编译。

Linux 的驱动目录虽然很大，但不代表都会编译。
真正会不会编译，看的是 `.config`。

例如：

```text
drivers/usb/
drivers/media/
drivers/net/
drivers/gpu/
```

这些源码在目录里存在，但如果 `.config` 没开，就不会参与编译。

---

# 十四、你现在可以先做这几个命令检查

在你的 X2600 SDK 根目录下：

```bash
find . -name ".config"
```

看有哪些配置文件。

再找 defconfig：

```bash
find . -name "*defconfig*"
```

查看当前 kernel 配置大小：

```bash
grep "^CONFIG_" kernel/.config | wc -l
```

查看打开了哪些 USB：

```bash
grep "CONFIG_USB" kernel/.config
```

查看打开了哪些调试功能：

```bash
grep "CONFIG_DEBUG" kernel/.config
```

查看打开了哪些文件系统：

```bash
grep "_FS" kernel/.config
```

---

# 十五、最推荐你的学习路线

你现在不要追求一下子把系统裁到最小。推荐先做到：

```text
第一阶段：
能正常编译、烧录、启动

第二阶段：
知道 kernel、rootfs、bootloader 分别是什么

第三阶段：
会用 menuconfig 看配置

第四阶段：
能关闭 USB、蓝牙、IPv6、debug info 这类明显不用的功能

第五阶段：
会保存 defconfig

第六阶段：
会单独编译驱动模块 .ko

第七阶段：
会裁剪 buildroot rootfs
```

---

# 总结

可以裁剪，而且嵌入式 Linux 必须裁剪。

重点记住：

```text
Kernel 裁剪：改 kernel .config / defconfig
RootFS 裁剪：改 buildroot 配置
不要删除源码文件
不要一上来乱关底层驱动
不要每次 fullclean
驱动开发尽量用模块编译
```

你现在最应该先做的是：

```bash
find . -name ".config"
find . -name "*defconfig*"
```

然后确认你的 SDK 里到底是哪个配置在控制 kernel，哪个配置在控制 buildroot。

------
-----
-----
需要裁剪，**尤其是嵌入式 Linux 系统**。但要注意：**开发初期不要急着大裁剪，产品稳定后再裁剪**。

可以这样理解：

| 阶段      | 是否建议裁剪              |
| ------- | ------------------- |
| 刚拿到 SDK | 不建议大裁剪，先保证能编译、烧录、启动 |
| 驱动调试阶段  | 少量裁剪明显不用的东西         |
| 产品功能确定后 | 强烈建议裁剪              |
| 量产阶段    | 必须裁剪、固定配置、减少风险      |

---

# 1. Linux 系统为什么要裁剪？

嵌入式 Linux 裁剪主要有几个目的：

```text
1. 减小固件体积
2. 减少 Flash 占用
3. 减少 RAM 占用
4. 减少启动时间
5. 减少无用功能带来的安全风险
6. 减少编译内容
7. 减少系统复杂度
```

比如你的君正 X2600 SDK 里面有：

```text
bootloader
kernel
buildroot
module_driver
libmedia
libisp
libmcu
demos
factory_test
third_party
```

这不是一个简单 MCU 工程，而是一个完整嵌入式 Linux 系统。

真正需要裁剪的主要是这几块：

```text
Linux Kernel      内核
RootFS            根文件系统
BusyBox           基础命令集合
驱动模块           .ko
第三方库           openssl、ffmpeg、alsa、opencv 等
Demo/测试程序      demos、factory_test
```

---

# 2. 裁剪不是删除源码

新手最容易误解这一点。

不要这样做：

```bash
rm -rf drivers/usb
rm -rf drivers/net
rm -rf buildroot/package/xxx
```

这很危险。

正确方式是通过配置裁剪：

```text
Kernel 用：
.config
defconfig
menuconfig
Kconfig

Buildroot 用：
.config
defconfig
menuconfig

BusyBox 用：
busybox menuconfig
```

也就是：

> 源码可以还在，但配置里没打开，它就不会参与编译，也不会进入最终系统。

---

# 3. Linux 系统裁剪分 4 层

## 第一层：裁剪 Kernel

Kernel 负责：

```text
CPU 调度
内存管理
中断
驱动
文件系统
网络协议栈
设备模型
```

Kernel 裁剪主要是关掉不用的：

```text
不用的驱动
不用的文件系统
不用的网络协议
不用的调试功能
不用的安全模块
不用的总线支持
```

例如：

```text
USB 不用，可以关 USB
蓝牙不用，可以关 Bluetooth
IPv6 不用，可以关 IPv6
声卡不用，可以关 ALSA/Sound
不需要 WiFi，可以关无线驱动
不需要 NFS，可以关 NFS
不需要调试信息，可以关 DEBUG_INFO
```

---

## 第二层：裁剪 RootFS

RootFS 是根文件系统。

里面有：

```text
/bin
/sbin
/lib
/usr
/etc
/dev
/proc
/sys
你的应用程序
动态库
启动脚本
配置文件
```

RootFS 裁剪主要是减少：

```text
不用的命令
不用的库
不用的服务
不用的 demo
不用的测试程序
不用的脚本
不用的语言运行环境
```

比如很多产品根本不需要：

```text
python
perl
gcc
gdb
strace
tcpdump
openssl 命令行工具
ffmpeg 命令行工具
大量网络工具
```

---

## 第三层：裁剪 BusyBox

BusyBox 可以理解为嵌入式 Linux 里的“命令工具合集”。

比如这些命令可能都来自 BusyBox：

```bash
ls
cp
mv
cat
echo
mount
ifconfig
ps
top
sh
dmesg
```

BusyBox 也可以裁剪。

比如产品不需要这些命令：

```text
telnet
ftp
vi
wget
nc
traceroute
udhcpc
syslogd
crond
```

就可以在 BusyBox 配置里关闭。

---

## 第四层：裁剪应用和第三方库

你 SDK 里可能有：

```text
libmedia
libisp
libmcu
lib2d
libutils2
third_party
demos
factory_test
```

这些也会影响最终系统大小。

比如产品不需要：

```text
factory_test
sample demo
调试工具
测试 app
多余媒体库
```

就不应该打包进 rootfs。

---

# 4. 对你这种 X2600 SDK，优先看这几个目录

你之前的目录是：

```text
x2600
├── bootloader
├── build
├── buildroot
├── kernel
├── libmcu
├── libmedia
├── libisp
├── module_driver
├── package
├── tools
└── demos
```

优先关注：

```text
kernel       内核裁剪
buildroot    根文件系统裁剪
build        厂商构建脚本和产品配置
package      打包规则
module_driver 外部驱动模块
demos        示例程序，很多量产不用
factory_test 工厂测试程序，视产品决定
```

你可以先查配置文件：

```bash
cd ~/project/x2600

find . -name ".config"
find . -name "*defconfig*"
find . -name "Config.in"
find . -name "Kconfig"
```

这些文件基本就是裁剪入口。

---

# 5. Kernel 怎么裁剪？

一般流程是：

```bash
cd kernel
make ARCH=mips menuconfig
```

你的 X2600 是君正平台，通常是 MIPS / XBurst2 架构，所以大概率是：

```bash
make ARCH=mips menuconfig
```

但是厂商 SDK 可能封装了命令，比如：

```bash
cd build
make menuconfig
make kernel-menuconfig
./build.sh menuconfig
./buildall.sh menuconfig
```

具体命令要看你 SDK 的 `README` 和 `build/Makefile`。

---

## Kernel 里常裁剪的地方

进入 `menuconfig` 后，重点看这些：

```text
General setup
Bus options
Networking support
Device Drivers
File systems
Kernel hacking
```

---

## 可以优先裁剪的 Kernel 项

### 1. USB

如果产品不用 USB：

```text
Device Drivers  --->
    USB support  --->
```

可以关掉。

但如果你用 USB 烧录、USB 摄像头、USB 网卡、U 盘，就不能关。

---

### 2. Bluetooth

不用蓝牙就关：

```text
Networking support  --->
    Bluetooth subsystem support
```

---

### 3. WiFi

不用 WiFi 就关：

```text
Device Drivers  --->
    Network device support  --->
        Wireless LAN
```

---

### 4. 声卡

如果产品不做音频：

```text
Device Drivers  --->
    Sound card support
```

如果你要做 AI 语音识别、麦克风采集，就不能随便关。

---

### 5. 不用的文件系统

```text
File systems  --->
```

常见可以裁：

```text
NTFS
HFS
HFS+
BTRFS
F2FS
ISO9660
CIFS
NFS
```

但这些一般别乱关：

```text
procfs
sysfs
tmpfs
devtmpfs
你 rootfs 正在使用的文件系统
```

比如你的 rootfs 是 squashfs，就必须保留：

```text
CONFIG_SQUASHFS
```

如果是 ubifs，就必须保留：

```text
CONFIG_UBIFS_FS
```

如果是 ext4，就必须保留：

```text
CONFIG_EXT4_FS
```

---

### 6. 调试功能

开发阶段建议保留一部分，量产可以裁掉很多。

路径：

```text
Kernel hacking  --->
```

可以考虑关闭：

```text
CONFIG_DEBUG_INFO
CONFIG_FTRACE
CONFIG_FUNCTION_TRACER
CONFIG_LOCKDEP
CONFIG_DEBUG_MUTEXES
CONFIG_DEBUG_SPINLOCK
CONFIG_SCHED_DEBUG
```

这些会增加内核体积，也会拖慢编译。

---

# 6. Kernel 里不能乱裁的东西

这些不要轻易关：

```text
CPU 架构支持
中断控制器
定时器
时钟 clk
pinctrl
gpio
reset controller
dma
serial console
mtd / nand / spi-nor
mmc / sd
i2c
spi
regulator
procfs
sysfs
tmpfs
devtmpfs
rootfs 对应文件系统
```

否则可能出现：

```text
内核启动不了
串口没有日志
找不到 rootfs
找不到 flash
找不到 mmc
驱动 probe 失败
应用无法启动
```

---

# 7. RootFS 怎么裁剪？

如果你的 SDK 用的是 Buildroot，进入：

```bash
cd buildroot
make menuconfig
```

或者厂商可能封装成：

```bash
make buildroot-menuconfig
./build.sh buildroot-menuconfig
```

在 Buildroot 里重点裁这些：

```text
Target packages
Toolchain
System configuration
Filesystem images
```

重点看：

```text
Target packages  --->
```

这里面会有大量软件包。

比如：

```text
Audio and video applications
Graphic libraries and applications
Networking applications
Debugging, profiling and benchmark
Libraries
Interpreter languages and scripting
```

---

## RootFS 常见可以裁掉的东西

如果产品不用，可以关闭：

```text
python
perl
lua
gdb
strace
ltrace
tcpdump
iperf
wget
curl
openssh
samba
bluez
ffmpeg
gstreamer
opencv
qt
mesa
x11
wayland
```

如果你的产品只是跑一个摄像头采集应用，RootFS 可以很小：

```text
busybox
libc
必要动态库
启动脚本
设备节点
你的 app
必要配置文件
```

---

# 8. BusyBox 怎么裁剪？

一般命令：

```bash
cd buildroot
make busybox-menuconfig
```

或者：

```bash
make busybox-menuconfig
```

在里面可以关闭不用的命令。

比如不需要网络调试工具，可以关：

```text
telnet
ftp
tftp
nc
wget
traceroute
nslookup
```

不需要编辑器，可以关：

```text
vi
awk
sed
less
```

不过开发阶段建议保留一些常用命令：

```text
sh
ls
cat
echo
mount
umount
dmesg
ps
top
ifconfig/ip
insmod
rmmod
lsmod
```

否则调试会很痛苦。

---

# 9. 驱动模块也可以裁剪

Linux 驱动有两种方式：

```text
=y   编译进内核
=m   编译成 .ko 模块
=n   不编译
```

例如：

```text
CONFIG_XXX=y
CONFIG_XXX=m
# CONFIG_XXX is not set
```

如果你还在调试驱动，建议先用模块：

```text
CONFIG_XXX=m
```

这样修改驱动后可以只编译 `.ko`，不用每次重编整个 kernel。

例如：

```bash
make ARCH=mips M=drivers/xxx modules
```

或者：

```bash
make ARCH=mips M=drivers/media/i2c modules
```

这样编译速度会快很多。

---

# 10. 裁剪的推荐顺序

不要一上来乱裁，建议这样来：

```text
第一步：先保证原始 SDK 能完整编译、烧录、启动
第二步：记录当前 kernel 镜像和 rootfs 大小
第三步：列出产品需要的硬件功能
第四步：先裁明显不用的外设
第五步：再裁不用的网络协议
第六步：再裁不用的文件系统
第七步：再裁调试功能
第八步：再裁 rootfs 软件包
第九步：再裁 busybox 命令
第十步：保存 defconfig
```

---

# 11. 你可以先从这些地方下手

比较安全的第一批裁剪对象：

```text
不用 USB 就关 USB
不用蓝牙就关 Bluetooth
不用 WiFi 就关 WLAN
不用声卡就关 Sound
不用 IPv6 就关 IPv6
不用 NFS 就关 NFS
不用 CIFS/SMB 就关 CIFS
不用 NTFS/HFS/BTRFS 就关这些文件系统
不用 ftrace/lockdep 就关这些调试功能
不用 demo 就不要打包 demo
不用 factory_test 就不要打包 factory_test
```

暂时别动：

```text
serial console
mtd
nand
spi-nor
mmc
i2c
spi
gpio
pinctrl
clk
dma
procfs
sysfs
tmpfs
devtmpfs
rootfs 文件系统
```

---

# 12. 怎么判断系统哪里大？

可以看 kernel 镜像大小：

```bash
ls -lh kernel/arch/mips/boot/
```

也可以找最终镜像：

```bash
find . -name "*.bin" -o -name "*.img" -o -name "vmlinux" -o -name "uImage" -o -name "zImage"
```

看 rootfs 哪些目录大：

```bash
du -sh buildroot/output/target/*
```

或者：

```bash
du -ah buildroot/output/target | sort -rh | head -50
```

这个命令很有用，它能告诉你 rootfs 里面最大的 50 个文件/目录。

比如你可能会看到：

```text
/usr/lib
/usr/bin
/lib
/bin
/sbin
```

然后继续查：

```bash
du -ah buildroot/output/target/usr/lib | sort -rh | head -50
```

这样就知道到底是哪个库占空间。

---

# 13. 怎么判断 Kernel 哪些配置打开了？

查看 USB：

```bash
grep "CONFIG_USB" kernel/.config
```

查看蓝牙：

```bash
grep "CONFIG_BT" kernel/.config
```

查看 WiFi：

```bash
grep "CONFIG_WLAN" kernel/.config
```

查看文件系统：

```bash
grep "_FS" kernel/.config
```

查看调试配置：

```bash
grep "CONFIG_DEBUG" kernel/.config
```

查看 V4L2 / Camera：

```bash
grep "CONFIG_VIDEO" kernel/.config
grep "CONFIG_MEDIA" kernel/.config
```

---

# 14. 裁剪后一定要保存配置

Kernel 裁剪后，`.config` 会变。

建议保存成 defconfig：

```bash
cd kernel
make ARCH=mips savedefconfig
```

会生成：

```text
defconfig
```

然后你可以保存：

```bash
cp defconfig arch/mips/configs/x2600_my_product_defconfig
```

以后恢复：

```bash
make ARCH=mips x2600_my_product_defconfig
```

Buildroot 也一样：

```bash
cd buildroot
make savedefconfig
```

不要只保存 `.config`，最好把 defconfig 放到版本管理里。

---

# 15. 裁剪和编译速度的关系

裁剪能让编译变快，但不是唯一办法。

Linux 内核编译慢，很多时候是因为你每次都全量编译了。

尽量不要频繁执行：

```bash
make clean
make distclean
make mrproper
make fullclean
```

这些会导致重新编译大量文件。

平时改驱动，尽量增量编译。

可以用多线程：

```bash
make -j$(nproc)
```

可以用 ccache：

```bash
sudo apt install ccache
```

然后：

```bash
ccache -s
```

如果 SDK 支持 ccache，第二次编译会快很多。

---

# 16. 一个实际裁剪思路示例

假设你的产品是：

```text
X2600 + 摄像头 sensor + MIPI CSI + ISP + V4L2 + 本地应用
不用 USB
不用蓝牙
不用 WiFi
不用声卡
不用 HDMI
不用桌面 GUI
不用 Python
不用 GDB
```

那么 Kernel 保留：

```text
MIPS/XBurst2
串口 console
GPIO
Pinctrl
Clock
Reset
DMA
I2C
SPI
MIPI CSI
ISP
V4L2
Media framework
Sensor driver
Flash/MMC 启动相关
RootFS 对应文件系统
procfs/sysfs/tmpfs/devtmpfs
```

Kernel 可裁：

```text
USB
Bluetooth
WiFi
Sound
不相关网卡
不相关 sensor
不相关触摸屏
不相关文件系统
IPv6
Netfilter
NFS
Ftrace
Lockdep
Debug info
```

RootFS 可裁：

```text
python
perl
gdb
strace
tcpdump
ffmpeg 命令行
demo 程序
factory_test
不用的网络服务
不用的脚本
```

---

# 17. 你现在最该做的事情

在你的 X2600 SDK 根目录执行：

```bash
cd ~/project/x2600

find . -name ".config"
find . -name "*defconfig*"
```

然后再看：

```bash
du -sh kernel
du -sh buildroot
du -sh buildroot/output/target 2>/dev/null
find . -name "*.img" -o -name "*.bin" -o -name "uImage" -o -name "vmlinux"
```

接着看当前 Kernel 打开了哪些大功能：

```bash
grep "CONFIG_USB" kernel/.config
grep "CONFIG_BT" kernel/.config
grep "CONFIG_IPV6" kernel/.config
grep "CONFIG_SOUND" kernel/.config
grep "CONFIG_DEBUG_INFO" kernel/.config
grep "CONFIG_MEDIA" kernel/.config
```

---

# 18. 最重要的结论

Linux 系统当然需要裁剪，尤其是嵌入式 Linux。

但是裁剪顺序要对：

```text
不要删源码
不要一开始乱裁
不要裁掉启动必须项
先裁明显不用的驱动
再裁 rootfs 软件包
再裁 BusyBox 命令
最后保存 defconfig
```

你可以把 Linux 裁剪理解成 MCU 工程里的“组件开关”：

```text
MCU：
不用 ADC 就不加 ADC 驱动文件

Linux：
不用 USB 就在 menuconfig 里关闭 CONFIG_USB
```

只是 Linux 的配置系统更复杂，靠的是：

```text
Kconfig
Makefile
.config
defconfig
menuconfig
```

你下一步可以先查出你 SDK 里到底是哪几个 `.config` 在控制系统，然后再开始裁剪。
