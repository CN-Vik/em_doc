Buildroot 可以理解成：

> **一个专门给嵌入式 Linux 生成根文件系统、交叉编译工具链、第三方库、应用程序和最终固件镜像的构建系统。**

它不是 Linux 内核，也不是发行版，而是一个“自动化构建工具”。

你现在的 X2600 SDK 里有：

```text
x2600/
├── bootloader
├── kernel
├── buildroot
├── module_driver
├── libmedia
├── libisp
├── demos
├── package
└── build
```

其中：

```text
bootloader   负责启动 Linux
kernel       负责 Linux 内核和驱动
buildroot    负责生成 rootfs，也就是根文件系统
```

---

# 1. Buildroot 是干啥的？

嵌入式 Linux 系统大概分成这样：

```text
Bootloader
    ↓
Linux Kernel
    ↓
RootFS 根文件系统
    ↓
你的应用程序
```

Buildroot 主要负责下面这部分：

```text
RootFS 根文件系统
第三方库
系统命令
BusyBox
应用程序
动态库
配置文件
启动脚本
最终 rootfs 镜像
```

比如它可以帮你生成：

```text
/bin/ls
/bin/sh
/sbin/init
/lib/libc.so
/etc/init.d/rcS
/usr/bin/your_app
/dev
/proc
/sys
```

这些东西最后会被打包成：

```text
rootfs.ext4
rootfs.squashfs
rootfs.tar
rootfs.cpio
rootfs.ubi
```

然后烧录到板子的 Flash、NAND、eMMC、SD 卡里。

---

# 2. Buildroot 解决了什么问题？

不用 Buildroot 的话，你要自己做这些事：

```text
1. 下载 busybox 源码
2. 配置 busybox
3. 交叉编译 busybox
4. 下载 zlib、openssl、alsa、ffmpeg 等源码
5. 一个个交叉编译
6. 处理头文件路径
7. 处理动态库路径
8. 手动制作 /bin /lib /etc /dev
9. 手动写启动脚本
10. 手动打包 rootfs 镜像
```

这非常麻烦。

Buildroot 帮你把这些自动化了。

你只需要配置：

```text
我要不要 BusyBox？
我要不要 OpenSSL？
我要不要 ALSA？
我要不要 FFmpeg？
我要不要 Python？
我要不要 SSH？
我要生成 ext4 还是 squashfs？
我要用哪个交叉编译器？
我要把哪些文件放进 rootfs？
```

然后执行编译，Buildroot 自动处理。

---

# 3. Buildroot 和 Linux Kernel 的区别

这是新手最容易混的地方。

| 部分         | 负责什么            | 例子                          |
| ---------- | --------------- | --------------------------- |
| Bootloader | 上电启动、加载内核       | U-Boot                      |
| Kernel     | 驱动、调度、内存、文件系统支持 | GPIO/I2C/SPI/V4L2/网卡驱动      |
| Buildroot  | 生成用户空间 rootfs   | BusyBox、libc、openssl、你的 app |
| App        | 你的业务程序          | 摄像头采集程序、AI 识别程序             |

比如你要用 I2C 读 ICM42688：

```text
Kernel 负责：
- I2C 控制器驱动
- ICM42688 驱动
- IIO 子系统
- /dev/iio:deviceX 或 /sys/bus/iio/...

Buildroot 负责：
- 把 iio_info、测试程序、你的 app 放进 rootfs
- 提供 libc、shell、启动脚本
- 开机自动运行你的程序
```

再比如你要搞摄像头：

```text
Kernel 负责：
- MIPI CSI 驱动
- ISP 驱动
- V4L2 框架
- Sensor 驱动
- /dev/video0

Buildroot 负责：
- v4l2-ctl 工具
- 你的摄像头采集 app
- libmedia/libisp 依赖库
- 启动脚本
- rootfs 镜像
```

---

# 4. Buildroot 最核心的作用：生成 RootFS

RootFS 就是 Linux 运行后的 `/` 根目录。

板子启动后你看到的：

```bash
/
├── bin
├── sbin
├── lib
├── usr
├── etc
├── dev
├── proc
├── sys
├── tmp
└── mnt
```

这些东西大部分就是 Buildroot 生成的。

比如：

```bash
ls
cd
cat
echo
ps
top
mount
ifconfig
sh
```

很多都是 BusyBox 提供的。

BusyBox 又是 Buildroot 编译并放进去的。

---

# 5. Buildroot 里面几个重要概念

## 5.1 Toolchain：交叉编译工具链

嵌入式板子一般不是 x86，而是 MIPS、ARM、RISC-V。

你的电脑是 x86_64：

```text
PC: x86_64
X2600: MIPS / XBurst2
```

所以不能直接用 PC 的 gcc 编译板子程序。

需要交叉编译器，比如：

```text
mips-linux-gnu-gcc
arm-linux-gnueabihf-gcc
riscv64-linux-gnu-gcc
```

Buildroot 可以：

```text
1. 自己生成交叉编译工具链
2. 使用厂商提供的外部交叉编译工具链
```

像君正 SDK 一般会使用厂商提供的工具链。

你之前看到过类似：

```bash
export PATH=/home/vik/project/x2600/tools/toolchains/mips-xburst2-gcc720-glibc238/bin:$PATH
```

这就是把君正 MIPS 工具链加入环境变量。

---

## 5.2 BusyBox：嵌入式命令集合

BusyBox 是嵌入式 Linux 里非常重要的东西。

它把很多常用命令打包到一个程序里：

```text
ls
cp
mv
cat
echo
mount
umount
ps
top
ifconfig
sh
dmesg
```

很多命令其实都是 BusyBox 的软链接。

例如：

```bash
ls -l /bin/ls
```

可能看到：

```text
/bin/ls -> busybox
```

意思是：

```text
你执行 ls，其实是在执行 busybox 的 ls 功能。
```

Buildroot 可以配置 BusyBox 里面要不要这些命令。

---

## 5.3 Target packages：目标板软件包

Buildroot 可以选择很多软件包，比如：

```text
openssl
zlib
alsa-lib
ffmpeg
gstreamer
v4l-utils
i2c-tools
strace
gdb
dropbear ssh
wpa_supplicant
bluez
python
lua
sqlite
```

这些都是给目标板运行的。

例如你勾选：

```text
Target packages  --->
    Hardware handling  --->
        i2c-tools
```

最后板子 rootfs 里可能就有：

```bash
i2cdetect
i2cget
i2cset
```

这对调试 I2C 设备很有用。

---

## 5.4 Filesystem images：文件系统镜像

Buildroot 不只是生成目录，还可以打包成镜像。

常见格式：

| 镜像                | 用途                |
| ----------------- | ----------------- |
| `rootfs.tar`      | 调试、手动解包用          |
| `rootfs.ext4`     | eMMC、SD 卡常用       |
| `rootfs.squashfs` | 只读压缩 rootfs，嵌入式常用 |
| `rootfs.cpio`     | initramfs 常用      |
| `rootfs.ubifs`    | NAND Flash 常用     |
| `rootfs.jffs2`    | NOR Flash 老方案     |

比如你的产品用 NAND Flash，可能会生成：

```text
rootfs.ubifs
```

如果用 squashfs，可能是：

```text
rootfs.squashfs
```

---

# 6. Buildroot 的基本目录结构

一个典型 Buildroot 目录大概这样：

```text
buildroot/
├── configs/
├── package/
├── board/
├── output/
├── system/
├── toolchain/
├── fs/
├── Makefile
└── Config.in
```

重点看这几个：

---

## 6.1 `configs/`

里面放默认配置：

```text
configs/
├── qemu_arm_defconfig
├── raspberrypi_defconfig
├── xxx_board_defconfig
└── x2600_xxx_defconfig
```

类似 Linux kernel 的 defconfig。

你可以理解为：

> 这是某块板子的 Buildroot 默认配置。

---

## 6.2 `package/`

这个目录放软件包规则。

比如：

```text
package/
├── busybox/
├── openssl/
├── zlib/
├── ffmpeg/
├── alsa-lib/
├── dropbear/
└── ...
```

每个 package 里面一般有：

```text
Config.in
xxx.mk
```

例如：

```text
package/i2c-tools/
├── Config.in
└── i2c-tools.mk
```

其中：

```text
Config.in   负责 menuconfig 里显示选项
xxx.mk      负责怎么下载、配置、编译、安装这个包
```

---

## 6.3 `board/`

这个目录一般放板级相关文件。

比如：

```text
board/ingenic/x2600/
├── rootfs_overlay/
├── post-build.sh
├── post-image.sh
├── genimage.cfg
└── device_table.txt
```

它可能负责：

```text
1. 往 rootfs 里额外复制文件
2. 修改 /etc/inittab
3. 添加启动脚本
4. 生成最终烧录镜像
5. 创建设备节点
```

---

## 6.4 `output/`

这是 Buildroot 编译输出目录。

非常重要。

```text
output/
├── build/
├── host/
├── staging/
├── target/
└── images/
```

这几个目录一定要理解。

---

# 7. Buildroot 的 output 目录详解

## 7.1 `output/build/`

这里是软件包的编译临时目录。

比如：

```text
output/build/busybox-1.36.1/
output/build/openssl-3.x/
output/build/zlib-1.x/
```

每个软件包下载、解压、编译，大多在这里发生。

---

## 7.2 `output/host/`

这里放的是给 PC 主机用的工具。

比如：

```text
output/host/bin/
```

可能有：

```text
mips-linux-gcc
pkg-config
fakeroot
mkimage
genimage
```

注意：

> `host` 里的程序是给你的 Ubuntu 编译机运行的，不是给板子运行的。

---

## 7.3 `output/staging/`

这里像是目标板的“开发环境”。

里面有目标平台的：

```text
头文件
动态库
静态库
pkgconfig 文件
```

例如你编译自己的 app，需要链接 openssl，那么头文件和库可能在：

```text
output/staging/usr/include/
output/staging/usr/lib/
```

---

## 7.4 `output/target/`

这个最重要。

它就是还没打包前的 rootfs 目录。

例如：

```text
output/target/
├── bin
├── sbin
├── lib
├── usr
├── etc
├── dev
├── proc
├── sys
└── tmp
```

你最终板子上看到的 `/bin`、`/etc`、`/usr`，很多就来自这里。

但是注意：

> 不建议你手动长期修改 `output/target/`，因为重新编译可能会被覆盖。

正确做法是通过：

```text
rootfs overlay
post-build script
package
```

把东西放进去。

---

## 7.5 `output/images/`

这里放最终镜像。

例如：

```text
output/images/
├── rootfs.tar
├── rootfs.ext4
├── rootfs.squashfs
├── uImage
├── zImage
├── sdcard.img
└── firmware.img
```

真正烧录时，经常用这里的文件。

---

# 8. Buildroot 怎么用？

最标准的 Buildroot 使用流程是：

```bash
cd buildroot
make menuconfig
make
```

但是厂商 SDK 经常封装了一层。

你这种 X2600 SDK，可能不是直接在 `buildroot` 里 make，而是通过：

```bash
cd build
make
```

或者：

```bash
./buildall.sh
```

或者：

```bash
make x2600_xxx_defconfig
make
```

具体要看 SDK 的 README 和 Makefile。

不过底层原理都是一样的：

```text
读取 defconfig
生成 .config
根据 .config 编译工具链、库、busybox、rootfs
输出到 output/images
```

---

# 9. Buildroot 常用命令

## 9.1 进入配置界面

```bash
make menuconfig
```

界面类似 Linux kernel 的 menuconfig。

你可以选择：

```text
Toolchain
System configuration
Target packages
Filesystem images
Bootloaders
Kernel
```

---

## 9.2 保存 defconfig

```bash
make savedefconfig
```

它会把当前 `.config` 精简保存成：

```text
defconfig
```

然后你可以复制到：

```bash
cp defconfig configs/my_x2600_defconfig
```

以后恢复：

```bash
make my_x2600_defconfig
```

---

## 9.3 编译全部

```bash
make
```

多线程：

```bash
make -j$(nproc)
```

不过 Buildroot 官方一般不太建议强行 `make -j` 乱搞，很多版本内部会自己处理并行。厂商 SDK 怎么要求，要看它的说明。

---

## 9.4 单独重新编译某个包

比如重新编译 busybox：

```bash
make busybox-rebuild
```

重新配置 busybox：

```bash
make busybox-menuconfig
```

重新编译 i2c-tools：

```bash
make i2c-tools-rebuild
```

清理某个包：

```bash
make i2c-tools-dirclean
```

---

## 9.5 查看帮助

```bash
make help
```

这个命令很有用，可以看到当前 Buildroot 支持哪些目标。

---

# 10. Buildroot 配置界面里重点看什么？

执行：

```bash
make menuconfig
```

你会看到类似：

```text
Target options
Build options
Toolchain
System configuration
Kernel
Target packages
Filesystem images
Bootloaders
Host utilities
```

---

## 10.1 Target options

这里配置目标平台。

比如：

```text
CPU 架构
ABI
浮点类型
大小端
```

例如 X2600 可能是：

```text
MIPS
XBurst2
little endian
```

这个地方不要乱改。

一改错，整个系统可能都不能运行。

---

## 10.2 Toolchain

配置工具链。

可以选择：

```text
Buildroot toolchain
External toolchain
```

厂商 SDK 一般用：

```text
External toolchain
```

比如君正提供了：

```text
mips-xburst2-gcc720-glibc238
```

不要随便换工具链，否则可能出现：

```text
ABI 不匹配
libc 不匹配
动态库不兼容
程序运行报 not found
Illegal instruction
段错误
```

---

## 10.3 System configuration

配置系统级东西：

```text
主机名
root 密码
init 系统
设备节点管理
登录串口
启动脚本
```

比如：

```text
System hostname
Root password
Run a getty after boot
/dev management
```

如果你板子启动后想从串口登录，这里很关键。

---

## 10.4 Target packages

这是裁剪和加功能最常用的地方。

比如：

```text
Target packages  --->
    Audio and video applications
    Debugging, profiling and benchmark
    Hardware handling
    Libraries
    Networking applications
    Shell and utilities
```

你要加 `i2c-tools`，一般就在这里。

你要加 `v4l-utils`，也在这里。

---

## 10.5 Filesystem images

这里决定生成什么 rootfs 镜像。

例如：

```text
ext2/3/4 root filesystem
squashfs root filesystem
tar the root filesystem
cpio the root filesystem
ubifs root filesystem
jffs2 root filesystem
```

如果你烧录需要 `rootfs.squashfs`，这里就要打开 squashfs。

---

# 11. Buildroot 怎么裁剪系统？

裁剪主要就是关闭不用的软件包。

比如你的产品不需要 Python：

```text
Target packages  --->
    Interpreter languages and scripting  --->
        [ ] python3
```

不需要 SSH：

```text
Target packages  --->
    Networking applications  --->
        [ ] dropbear
        [ ] openssh
```

不需要调试工具：

```text
Target packages  --->
    Debugging, profiling and benchmark  --->
        [ ] gdb
        [ ] strace
        [ ] ltrace
        [ ] tcpdump
```

不需要音视频库：

```text
Target packages  --->
    Audio and video applications  --->
        [ ] ffmpeg
        [ ] gstreamer
```

不需要图形界面：

```text
Target packages  --->
    Graphic libraries and applications  --->
        [ ] qt
        [ ] weston
        [ ] xserver
```

---

# 12. Buildroot 里哪些工具对你很有用？

你做嵌入式 Linux 驱动开发，建议开发阶段保留这些：

## I2C 调试

```text
i2c-tools
```

板子上可以用：

```bash
i2cdetect -y 0
i2cget -y 0 0x68 0x75
i2cset -y 0 0x68 0x6B 0x00
```

调 IMU、sensor、codec 很有用。

---

## V4L2 摄像头调试

```text
v4l-utils
```

板子上可以用：

```bash
v4l2-ctl --list-devices
v4l2-ctl -d /dev/video0 --all
v4l2-ctl -d /dev/video0 --stream-mmap --stream-count=100
```

调摄像头、MIPI、ISP 很常用。

---

## ALSA 音频调试

```text
alsa-utils
```

板子上可以用：

```bash
arecord -l
aplay -l
arecord -D hw:0,0 -f S16_LE -r 16000 test.wav
aplay test.wav
```

调音频采集、AI 语音识别前端很有用。

---

## 网络调试

```text
iproute2
busybox ifconfig
ping
ethtool
tcpdump
```

开发阶段有用，量产可以裁掉一部分。

---

## 系统调试

```text
strace
gdbserver
procps-ng
htop
```

开发阶段有用，量产可以裁掉。

---

# 13. 怎么把自己的程序放进 Buildroot？

常见有三种方式。

---

## 方式一：rootfs overlay，适合放脚本、配置文件

比如你想放一个启动脚本：

```text
board/ingenic/x2600/rootfs_overlay/etc/init.d/S99myapp
```

Buildroot 配置里设置：

```text
System configuration  --->
    Root filesystem overlay directories
```

填：

```text
board/ingenic/x2600/rootfs_overlay
```

这样 Buildroot 生成 rootfs 时，会把 overlay 里的文件复制进去。

比如：

```text
rootfs_overlay/
├── etc
│   └── init.d
│       └── S99myapp
└── usr
    └── bin
        └── test.sh
```

最终 rootfs 里会有：

```text
/etc/init.d/S99myapp
/usr/bin/test.sh
```

适合放：

```text
启动脚本
配置文件
小脚本
默认参数文件
```

不太适合放复杂 C 程序源码。

---

## 方式二：post-build script，适合编译后做额外处理

比如你想在 rootfs 生成前额外复制文件，可以写：

```bash
board/ingenic/x2600/post-build.sh
```

内容类似：

```bash
#!/bin/sh

TARGET_DIR="$1"

cp -a board/ingenic/x2600/my_config.json "$TARGET_DIR/etc/my_config.json"
cp -a output/my_app "$TARGET_DIR/usr/bin/my_app"

chmod +x "$TARGET_DIR/usr/bin/my_app"
```

Buildroot 配置里设置：

```text
System configuration  --->
    Custom scripts to run before creating filesystem images
```

填：

```text
board/ingenic/x2600/post-build.sh
```

---

## 方式三：写成 Buildroot package，最正规

如果你的程序是一个正式项目，推荐做成 package。

比如你的应用叫：

```text
my_camera_app
```

可以新建：

```text
package/my_camera_app/
├── Config.in
└── my_camera_app.mk
```

然后在 `package/Config.in` 里加入：

```makefile
source "package/my_camera_app/Config.in"
```

`Config.in` 示例：

```text
config BR2_PACKAGE_MY_CAMERA_APP
    bool "my_camera_app"
    help
      My camera application for X2600.
```

`my_camera_app.mk` 示例：

```makefile
MY_CAMERA_APP_SITE = $(TOPDIR)/../demos/my_camera_app
MY_CAMERA_APP_SITE_METHOD = local

define MY_CAMERA_APP_BUILD_CMDS
	$(TARGET_CC) $(TARGET_CFLAGS) \
		-o $(@D)/my_camera_app \
		$(@D)/main.c
endef

define MY_CAMERA_APP_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/my_camera_app \
		$(TARGET_DIR)/usr/bin/my_camera_app
endef

$(eval $(generic-package))
```

然后进入：

```bash
make menuconfig
```

勾选：

```text
Target packages  --->
    my_camera_app
```

以后 Buildroot 就会自动编译并安装你的应用。

---

# 14. Buildroot 怎么设置开机自启动？

常见方式是添加 `/etc/init.d/Sxx` 脚本。

例如：

```bash
/etc/init.d/S99myapp
```

内容：

```sh
#!/bin/sh

case "$1" in
  start)
    echo "Starting my app..."
    /usr/bin/my_camera_app &
    ;;
  stop)
    echo "Stopping my app..."
    killall my_camera_app
    ;;
  restart)
    $0 stop
    $0 start
    ;;
  *)
    echo "Usage: $0 {start|stop|restart}"
    exit 1
    ;;
esac

exit 0
```

文件权限要可执行：

```bash
chmod +x S99myapp
```

为什么叫 `S99`？

因为很多嵌入式系统启动脚本会按文件名顺序执行：

```text
S10xxx
S20xxx
S40network
S99myapp
```

数字越大，越晚启动。

你的应用通常等驱动、网络、文件系统准备好之后再启动，所以叫 `S99myapp` 很常见。

---

# 15. Buildroot 和 BusyBox init 的关系

很多 Buildroot 系统默认用 BusyBox init。

启动流程大概是：

```text
kernel 启动
    ↓
挂载 rootfs
    ↓
执行 /sbin/init
    ↓
读取 /etc/inittab
    ↓
执行 /etc/init.d/rcS
    ↓
执行 /etc/init.d/Sxx 脚本
    ↓
启动你的应用
```

你可以在 rootfs 里看：

```bash
cat /etc/inittab
ls /etc/init.d/
```

如果是 BusyBox init，通常会看到：

```text
::sysinit:/etc/init.d/rcS
```

意思是系统初始化时执行：

```bash
/etc/init.d/rcS
```

---

# 16. Buildroot 和动态库的关系

假设你的程序依赖：

```text
libmedia.so
libisp.so
libpthread.so
libm.so
libc.so
```

那么这些库必须存在于 rootfs 里，比如：

```text
/lib
/usr/lib
```

否则板子上运行程序会报错：

```bash
./my_app: error while loading shared libraries: libxxx.so: cannot open shared object file
```

你可以在 PC 上用交叉工具链查看：

```bash
mips-linux-readelf -d my_app
```

或者板子上用：

```bash
ldd my_app
```

有些嵌入式 rootfs 可能没有 `ldd`，也可以用：

```bash
readelf -d my_app | grep NEEDED
```

Buildroot 的作用就是把需要的库一起安装进 rootfs。

---

# 17. Buildroot 常见输出关系图

你可以这样理解：

```text
Buildroot .config
    ↓
选择 toolchain
    ↓
编译 host tools
    ↓
编译 target packages
    ↓
安装到 output/target
    ↓
执行 rootfs overlay
    ↓
执行 post-build.sh
    ↓
打包 rootfs
    ↓
生成 output/images/rootfs.xxx
```

最终你烧录的一般是：

```text
output/images/rootfs.xxx
```

或者厂商再把它和 kernel、uboot 打成一个总镜像：

```text
firmware.img
```

---

# 18. Buildroot 和 Yocto 的区别

你以后可能会听到 Yocto。

简单对比：

| 项目    | Buildroot      | Yocto            |
| ----- | -------------- | ---------------- |
| 学习难度  | 相对简单           | 很复杂              |
| 构建方式  | 一次性生成完整 rootfs | 类似发行版构建系统        |
| 包管理   | 通常没有运行时包管理     | 可以支持 ipk/rpm/deb |
| 适合    | 小型/中型嵌入式产品     | 大型复杂 Linux 产品    |
| 编译速度  | 相对快            | 通常更慢             |
| 定制复杂度 | 中等             | 很强但复杂            |

对你现在这种阶段：

```text
君正 X2600
嵌入式 Linux 入门
驱动 + 应用开发
摄像头 / I2C / IIO / V4L2
```

Buildroot 更适合入门。

---

# 19. Buildroot 和 Ubuntu 的区别

Ubuntu 是完整 Linux 发行版。

Buildroot 不是传统发行版。

Ubuntu 有：

```text
apt
dpkg
systemd
大量库
大量工具
桌面环境
在线安装软件
```

Buildroot 生成的系统一般很小：

```text
没有 apt
没有 dpkg
没有完整桌面
通常没有 gcc
通常没有 python
通常没有包管理器
```

它追求的是：

```text
小
可控
启动快
适合量产
适合嵌入式
```

所以你不能指望在板子上：

```bash
apt install xxx
```

Buildroot 的思路是：

> 需要什么，就在 PC 上配置好，交叉编译进去，然后重新生成 rootfs。

---

# 20. 新手最容易踩的坑

## 坑 1：直接改 `output/target`

比如你手动复制：

```bash
cp my_app buildroot/output/target/usr/bin/
```

短期可以调试，但不推荐长期这样做。

因为下次重新构建可能被覆盖。

正规做法：

```text
rootfs overlay
post-build script
Buildroot package
```

---

## 坑 2：忘记把动态库放进 rootfs

程序编译成功，不代表板子能运行。

比如你的 app 编译时链接了：

```text
libmedia.so
libisp.so
libai.so
```

但 rootfs 没有这些库，运行就会失败。

---

## 坑 3：工具链不一致

比如：

```text
kernel 用一个工具链
buildroot 用另一个工具链
app 用第三个工具链
```

可能会导致 ABI、libc、动态库不匹配。

建议：

> kernel、buildroot、app 尽量使用 SDK 指定的同一套交叉编译器。

---

## 坑 4：裁剪太狠

比如你裁掉：

```text
/dev 管理
procfs
sysfs
busybox sh
串口登录
```

结果系统启动后你进不去，也没法调试。

开发阶段不要裁得太狠。

---

## 坑 5：以为 Buildroot 会自动编译 kernel 驱动

Buildroot 主要管用户空间。

Kernel 驱动通常由 kernel 自己的 Kconfig/Makefile 管。

当然 Buildroot 也可以配置 Linux kernel，但厂商 SDK 里经常把 kernel 编译单独封装了。

所以你要分清：

```text
驱动开关：多半在 kernel menuconfig
用户空间工具：多半在 buildroot menuconfig
```

---

# 21. 针对你的 X2600，建议怎么学 Buildroot？

你可以按这个顺序来。

## 第一步：先找配置文件

在 SDK 根目录执行：

```bash
cd ~/project/x2600

find buildroot -name "*defconfig*"
find buildroot -name ".config"
find buildroot -name "Config.in"
find buildroot -name "*.mk" | head
```

再找输出目录：

```bash
find buildroot -maxdepth 3 -type d -name output
```

或者：

```bash
find . -path "*output/images*" -type d
```

---

## 第二步：看最终 rootfs 输出在哪里

```bash
find . -name "rootfs*"
find . -name "*.squashfs" -o -name "*.ext4" -o -name "*.ubi" -o -name "*.tar"
```

你要搞清楚：

```text
最终烧录的是哪个 rootfs？
是在 buildroot/output/images 下面？
还是被厂商 build 脚本拷贝到了 build/output 下面？
```

---

## 第三步：看 rootfs 里有什么

如果有：

```text
buildroot/output/target
```

可以执行：

```bash
du -sh buildroot/output/target/*
```

看哪个目录大。

再看最大文件：

```bash
du -ah buildroot/output/target | sort -rh | head -50
```

这个命令非常适合裁剪系统。

---

## 第四步：进入配置界面

尝试：

```bash
cd buildroot
make menuconfig
```

如果不行，说明厂商封装了命令。

那就看：

```bash
cd ~/project/x2600
grep -R "buildroot-menuconfig" -n .
grep -R "menuconfig" -n build Makefile buildroot/Makefile
```

看看 SDK 提供的正确入口。

---

# 22. 对你来说，Buildroot 最常用的操作

你以后大概率会经常干这些事：

```text
1. 添加调试工具
   i2c-tools、v4l-utils、alsa-utils、strace

2. 删除不用的软件
   python、gdb、ssh、ffmpeg、demo

3. 添加自己的应用
   my_app 放到 /usr/bin

4. 添加启动脚本
   /etc/init.d/S99myapp

5. 添加配置文件
   /etc/my_config.json

6. 生成 rootfs 镜像
   rootfs.squashfs / rootfs.ext4 / rootfs.ubifs

7. 查依赖库
   确认 libxxx.so 是否进入 rootfs

8. 保存 defconfig
   保证别人能复现你的系统
```

---

# 23. 一个完整例子：添加 I2C 调试工具

假设你想在板子上用：

```bash
i2cdetect
i2cget
i2cset
```

流程大概是：

```bash
cd buildroot
make menuconfig
```

进入：

```text
Target packages  --->
    Hardware handling  --->
        [*] i2c-tools
```

保存退出。

然后：

```bash
make
```

生成 rootfs 后，烧录到板子。

板子启动后：

```bash
i2cdetect -y 0
```

如果能执行，说明 i2c-tools 已经进入系统。

---

# 24. 一个完整例子：添加自己的 app

假设你有程序：

```text
my_app
```

你想让它出现在板子：

```bash
/usr/bin/my_app
```

最简单可以用 rootfs overlay。

建立目录：

```bash
mkdir -p board/ingenic/x2600/rootfs_overlay/usr/bin
cp my_app board/ingenic/x2600/rootfs_overlay/usr/bin/
chmod +x board/ingenic/x2600/rootfs_overlay/usr/bin/my_app
```

然后在 Buildroot 里配置 overlay：

```text
System configuration  --->
    Root filesystem overlay directories
```

填：

```text
board/ingenic/x2600/rootfs_overlay
```

重新编译 rootfs。

板子启动后：

```bash
/usr/bin/my_app
```

就能运行。

更正规的方式是写 package，但 overlay 方式适合新手先理解。

---

# 25. 一个完整例子：开机启动自己的 app

建立：

```bash
mkdir -p board/ingenic/x2600/rootfs_overlay/etc/init.d
```

创建：

```bash
board/ingenic/x2600/rootfs_overlay/etc/init.d/S99myapp
```

内容：

```sh
#!/bin/sh

case "$1" in
  start)
    echo "start my_app"
    /usr/bin/my_app &
    ;;
  stop)
    echo "stop my_app"
    killall my_app
    ;;
  *)
    echo "Usage: $0 {start|stop}"
    exit 1
    ;;
esac

exit 0
```

加权限：

```bash
chmod +x board/ingenic/x2600/rootfs_overlay/etc/init.d/S99myapp
```

重新生成 rootfs。

板子开机时就会执行它。

---

# 26. 总结一句话

Buildroot 就是：

```text
给嵌入式 Linux 自动构建 rootfs 的工具。
```

它主要管：

```text
交叉编译工具链
BusyBox
第三方库
用户空间程序
启动脚本
配置文件
rootfs 镜像
```

它不主要管：

```text
CPU 启动
中断
内存管理
底层驱动
```

这些更多是 Bootloader 和 Kernel 的事情。

你可以这样记：

```text
Kernel 解决“硬件怎么驱动”
Buildroot 解决“系统里有什么软件”
App 解决“产品具体干什么”
```

对你现在来说，学习 Buildroot 最重要的是掌握这几个点：

```text
1. 会找到 buildroot 的 defconfig
2. 会进入 menuconfig
3. 会添加/删除软件包
4. 会看 output/target
5. 会看 output/images
6. 会用 rootfs overlay 添加文件
7. 会添加开机启动脚本
8. 会保存 savedefconfig
```

等你能把自己的程序放进 rootfs，并且开机自动运行，你就真正入门 Buildroot 了。
