嵌入式 Linux 开发的难点，不是“会不会写 C”，而是**系统层级太多**：Bootloader、内核、设备树、驱动、根文件系统、应用、脚本、网络、存储、升级、量产工具都可能出问题。
你可以把它理解成：**MCU 是一个工程，嵌入式 Linux 是一整套系统工程。**

下面按实际开发中最常见的问题分类讲。

---

# 1. 编译环境问题

## 常见现象

比如你编译君正 X2600 SDK、Buildroot、Kernel、U-Boot 时，出现：

```bash
command not found
No such file or directory
libxxx.so not found
undefined reference
Permission denied
cannot execute binary file
```

或者你之前遇到过类似：

```bash
libmpfr.so.4: cannot open shared object file
```

## 常见原因

嵌入式 Linux 编译依赖很多东西：

1. 交叉编译工具链没配置好；
2. `PATH` 没加工具链路径；
3. Ubuntu 缺少依赖库；
4. SDK 要求 Ubuntu 18.04，但你用 Ubuntu 22.04；
5. 32 位库缺失；
6. 编译脚本权限不够；
7. 用 root 编译导致文件权限混乱。

## 排查方法

先看当前使用的是哪个编译器：

```bash
which gcc
which make
which mips-linux-gnu-gcc
which riscv64-unknown-elf-gcc
echo $PATH
```

看交叉编译器能不能执行：

```bash
mips-linux-gnu-gcc -v
```

看缺哪个动态库：

```bash
ldd /path/to/toolchain/bin/xxx-gcc
```

查系统有没有这个库：

```bash
find /usr/lib -name "libmpfr.so*"
find /lib -name "libmpfr.so*"
```

## 解决思路

安装依赖：

```bash
sudo apt update
sudo apt install build-essential git make gcc g++ \
    libncurses5-dev libssl-dev bison flex \
    unzip bc python3 python3-pip
```

如果是工具链找不到，就配置 PATH：

```bash
export PATH=/home/vik/project/x2600/tools/toolchains/mips-xburst2-gcc720-glibc238/bin:$PATH
```

想永久生效，写进：

```bash
~/.bashrc
```

例如：

```bash
echo 'export PATH=/home/vik/project/x2600/tools/toolchains/mips-xburst2-gcc720-glibc238/bin:$PATH' >> ~/.bashrc
source ~/.bashrc
```

---

# 2. 不知道该编译哪个模块

## 常见现象

嵌入式 Linux SDK 目录很大，比如：

```bash
bootloader
buildroot
kernel
module_driver
package
third_party
tools
wireless
```

新手会懵：

> 我到底改了代码以后，应该编译 kernel？buildroot？整个系统？还是只编译 app？

## 核心理解

嵌入式 Linux 一般分成这几层：

```text
Bootloader  ->  Kernel  ->  Rootfs  ->  App
启动加载器       Linux内核     根文件系统    应用程序
```

对应关系大概是：

| 你改了什么                 | 需要编译什么                           |
| --------------------- | -------------------------------- |
| U-Boot 启动流程           | bootloader / uboot               |
| 内核驱动                  | kernel 或 module_driver           |
| 设备树 dts               | kernel                           |
| 根文件系统里的库/脚本           | buildroot / rootfs               |
| 应用程序                  | app/package                      |
| busybox 配置            | buildroot                        |
| 开机启动脚本                | rootfs / package                 |
| Wi-Fi/Camera/Audio 驱动 | kernel/module_driver + rootfs 配置 |

## 排查思路

看 Makefile 支持哪些目标：

```bash
make help
```

或者：

```bash
grep -r "^[a-zA-Z0-9_-]*:" Makefile
```

看厂商 README：

```bash
find . -iname "*readme*"
find . -iname "*.md"
find . -iname "*.txt"
```

---

# 3. 烧录启动失败

## 常见现象

设备烧录后：

```text
不开机
串口无打印
卡在 bootloader
卡在 Starting kernel ...
Kernel panic
VFS: Unable to mount root fs
```

## 常见原因

启动链路任何一环错了都会失败：

```text
ROM Code -> Bootloader -> Kernel -> Device Tree -> Rootfs -> init
```

常见问题：

1. bootloader 没烧进去；
2. kernel 地址错；
3. dtb 地址错；
4. rootfs 分区错；
5. 启动参数 bootargs 错；
6. flash 分区表和实际镜像不匹配；
7. 文件系统格式不对；
8. 内核没打开对应存储驱动。

## 关键排查点

串口日志是第一信息源。

看 bootargs：

```bash
printenv
```

常见启动参数类似：

```bash
console=ttyS0,115200 root=/dev/mtdblock3 rootfstype=squashfs
```

检查 rootfs 挂载错误：

```text
VFS: Cannot open root device
Kernel panic - not syncing: VFS: Unable to mount root fs
```

这个一般说明：

```text
root= 参数错
分区错
文件系统类型错
内核没支持对应存储驱动
```

## 解决方向

重点检查：

```text
1. flash 分区表
2. bootloader 环境变量
3. kernel image 地址
4. dtb 地址
5. rootfs 分区
6. rootfs 文件系统格式
7. 内核是否支持 NAND/NOR/eMMC/SD
```

---

# 4. 设备树 DTS 问题

这是嵌入式 Linux 最常见难点之一。

## 常见现象

驱动写好了，但是 probe 不进：

```text
driver probe 没调用
设备节点没生成
GPIO/I2C/SPI/UART 不工作
中断不触发
电源不打开
```

## 设备树的作用

MCU 里你可能直接写：

```c
gpio_init(GPIOA, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_5);
```

Linux 里很多硬件信息不直接写死在驱动里，而是写在 DTS 里：

```dts
&i2c0 {
    status = "okay";

    icm42688@68 {
        compatible = "tdk,icm42688";
        reg = <0x68>;
        interrupt-parent = <&gpioa>;
        interrupts = <5 IRQ_TYPE_EDGE_RISING>;
    };
};
```

驱动通过 `compatible` 匹配设备。

## 排查命令

看设备树是否生效：

```bash
ls /proc/device-tree
```

看某个节点：

```bash
ls /proc/device-tree/soc/
```

看 compatible：

```bash
cat /proc/device-tree/xxx/compatible
```

反编译当前 dtb：

```bash
dtc -I fs -O dts /proc/device-tree > running.dts
```

查看内核日志：

```bash
dmesg | grep -i i2c
dmesg | grep -i spi
dmesg | grep -i gpio
dmesg | grep -i icm
```

## 常见解决方向

检查这几个点：

```text
compatible 是否和驱动 of_match_table 一致
status 是否为 "okay"
reg 地址是否正确
pinctrl 是否配置
clock 是否配置
reset 是否配置
interrupt 是否配置
电源 regulator 是否配置
```

比如驱动里是：

```c
static const struct of_device_id icm42688_of_match[] = {
    { .compatible = "tdk,icm42688" },
    { }
};
```

那 DTS 里必须能匹配：

```dts
compatible = "tdk,icm42688";
```

---

# 5. 驱动 probe 不执行

## 常见现象

你写了驱动，但启动日志里没有任何打印：

```c
static int xxx_probe(struct platform_device *pdev)
{
    printk("xxx probe\n");
    return 0;
}
```

结果 `dmesg` 里没有 `xxx probe`。

## 常见原因

1. 驱动没有编进内核；
2. 驱动模块没有加载；
3. 设备树没有对应节点；
4. compatible 不匹配；
5. `status = "disabled"`；
6. bus 类型不对，例如 I2C 驱动写成 platform 驱动；
7. Kconfig 没打开；
8. Makefile 没加 obj-y/obj-m。

## 排查方法

确认驱动是否编译进去：

```bash
zcat /proc/config.gz | grep XXX
```

如果是模块：

```bash
lsmod
find /lib/modules -name "*.ko"
```

手动加载模块：

```bash
insmod xxx.ko
dmesg | tail
```

看设备树 compatible：

```bash
dtc -I fs -O dts /proc/device-tree > running.dts
grep -i "xxx" running.dts
```

看驱动里 compatible：

```bash
grep -r "compatible" drivers/
```

## 解决方向

如果是内核内置：

```makefile
obj-y += xxx.o
```

如果是模块：

```makefile
obj-m += xxx.o
```

Kconfig 要选上：

```text
CONFIG_XXX=y
```

或者：

```text
CONFIG_XXX=m
```

---

# 6. GPIO / Pinctrl 复用问题

## 常见现象

GPIO 不输出，I2C/SPI/UART 没波形，按键中断不触发。

## 常见原因

嵌入式 Linux 里，一个引脚通常要经过：

```text
pinctrl 复用配置
GPIO 控制器配置
驱动申请 GPIO
电气属性配置
```

如果 pinmux 没配对，引脚可能还处于别的功能。

## 排查方法

看 pinctrl 日志：

```bash
dmesg | grep -i pin
```

看 GPIO 状态：

```bash
cat /sys/kernel/debug/gpio
```

如果 debugfs 没挂载：

```bash
mount -t debugfs none /sys/kernel/debug
```

用 libgpiod 工具：

```bash
gpioinfo
gpiodetect
gpioset
gpioget
```

## 解决方向

DTS 里配置 pinctrl：

```dts
&pinctrl {
    i2c0_pins: i2c0-pins {
        pins = "xxx", "yyy";
        function = "i2c0";
    };
};

&i2c0 {
    pinctrl-names = "default";
    pinctrl-0 = <&i2c0_pins>;
    status = "okay";
};
```

---

# 7. I2C / SPI 外设调不通

## I2C 常见现象

```text
i2cdetect 扫不到
probe 失败
读 ID 全是 0xff
读 ID 全是 0x00
Remote I/O error
```

## 常见原因

1. SDA/SCL 接反；
2. 没上拉电阻；
3. 设备地址错；
4. 7-bit / 8-bit 地址搞混；
5. 设备没供电；
6. reset 脚没拉对；
7. pinctrl 没配；
8. I2C 频率太高；
9. 驱动访问时序不对。

## 排查命令

查看 I2C 总线：

```bash
ls /dev/i2c*
```

安装工具：

```bash
i2cdetect -l
```

扫描：

```bash
i2cdetect -y 0
i2cdetect -y 1
```

读寄存器：

```bash
i2cget -y 0 0x68 0x75
```

写寄存器：

```bash
i2cset -y 0 0x68 0x10 0x01
```

## SPI 常见现象

```text
/dev/spidev 没有
片选不对
MISO 一直 0xff
数据错位
频率高就通信失败
```

## SPI 排查重点

```text
CPOL/CPHA 是否正确
CS 是否正确
MOSI/MISO/SCLK 是否有波形
频率是否过高
bits_per_word 是否正确
片选极性是否正确
设备是否需要 reset/power enable
```

查看设备：

```bash
ls /dev/spidev*
```

内核日志：

```bash
dmesg | grep -i spi
```

---

# 8. Camera / V4L2 调试困难

## 常见现象

```text
/dev/video0 没有
摄像头 probe 失败
打开黑屏
花屏
颜色异常
帧率不对
mipi error
```

## 摄像头链路

以 MIPI sensor 为例：

```text
Sensor -> MIPI CSI -> ISP -> V4L2 -> 应用
```

不是只有一个 sensor 驱动。

你要同时关注：

```text
1. sensor 驱动
2. I2C 通信
3. MIPI CSI 控制器
4. ISP 配置
5. clock
6. reset/pwdn GPIO
7. regulator 电源
8. V4L2 media graph
```

## 排查命令

查看 video 节点：

```bash
ls /dev/video*
```

查看 media 拓扑：

```bash
media-ctl -p
```

查看格式：

```bash
v4l2-ctl --list-devices
v4l2-ctl -d /dev/video0 --all
v4l2-ctl -d /dev/video0 --list-formats-ext
```

抓一帧：

```bash
v4l2-ctl -d /dev/video0 --stream-mmap --stream-count=1 --stream-to=test.raw
```

看日志：

```bash
dmesg | grep -i sensor
dmesg | grep -i mipi
dmesg | grep -i csi
dmesg | grep -i isp
dmesg | grep -i v4l2
```

## 解决方向

重点查：

```text
sensor ID 是否读到
mclk 是否输出
reset/pwdn 时序是否正确
MIPI lane 数是否正确
分辨率/帧率是否匹配
media graph 是否链接成功
pixel format 是否一致
```

---

# 9. 音频驱动问题

## 常见现象

```text
没有声卡
aplay 播放无声
arecord 录音无数据
声音有杂音
左右声道反了
采样率不对
I2S 没波形
```

## 音频链路

一般是：

```text
应用 ALSA
   ↓
ASoC machine driver
   ↓
CPU DAI
   ↓
I2S/TDM 控制器
   ↓
Codec 芯片
   ↓
功放/喇叭/麦克风
```

## 排查命令

查看声卡：

```bash
cat /proc/asound/cards
aplay -l
arecord -l
```

查看控件：

```bash
amixer
alsamixer
```

播放测试：

```bash
aplay test.wav
speaker-test -c 2 -t wav
```

录音测试：

```bash
arecord -D hw:0,0 -f S16_LE -r 16000 -c 1 test.wav
```

看日志：

```bash
dmesg | grep -i asoc
dmesg | grep -i i2s
dmesg | grep -i codec
dmesg | grep -i sound
```

## 解决方向

重点查：

```text
codec 是否 probe
I2C 是否能读写 codec
MCLK/BCLK/LRCK 是否有波形
采样率是否匹配
DAI format 是否正确
master/slave 配置是否正确
功放 enable GPIO 是否打开
amixer 通路是否打开
```

---

# 10. 文件系统问题

## 常见现象

```text
系统启动后缺文件
应用找不到库
配置保存不了
掉电后文件丢失
read-only file system
No space left on device
```

## 常见原因

1. rootfs 是只读 squashfs；
2. 配置文件应该放到 data 分区；
3. 动态库没打包进 rootfs；
4. overlayfs 没配置好；
5. flash 分区满了；
6. 应用路径写死；
7. 权限不对。

## 排查命令

查看挂载：

```bash
mount
df -h
cat /proc/mounts
```

查看分区：

```bash
cat /proc/mtd
lsblk
```

查看库依赖：

```bash
ldd ./your_app
```

查看文件权限：

```bash
ls -l
```

## 解决方向

如果应用报：

```bash
error while loading shared libraries: libxxx.so
```

就查：

```bash
ldd your_app
```

然后把库放到：

```bash
/usr/lib
/lib
```

或者配置：

```bash
export LD_LIBRARY_PATH=/your/lib/path:$LD_LIBRARY_PATH
```

---

# 11. 应用程序运行失败

## 常见现象

```text
No such file or directory
Permission denied
Segmentation fault
Illegal instruction
Killed
```

## 各种错误含义

### 1）No such file or directory

不一定是文件不存在，也可能是**动态链接器不存在**。

检查：

```bash
file your_app
readelf -l your_app
ldd your_app
```

### 2）Permission denied

加执行权限：

```bash
chmod +x your_app
```

### 3）Illegal instruction

常见原因：

```text
编译架构不对
CPU 指令集不兼容
硬浮点/软浮点 ABI 不匹配
```

检查：

```bash
file your_app
```

### 4）Segmentation fault

用：

```bash
gdbserver :1234 ./your_app
```

或者先看内核日志：

```bash
dmesg | tail
```

---

# 12. 开机自启动问题

## 常见现象

应用手动运行正常，但开机不运行。

## 常见原因

1. 启动脚本没加执行权限；
2. 脚本路径错；
3. 环境变量缺失；
4. 程序依赖的设备节点还没生成；
5. 程序启动太早；
6. systemd / init.d / rcS 机制没搞清。

## BusyBox init 常见方式

可能在：

```bash
/etc/init.d/
```

添加脚本：

```bash
/etc/init.d/S99app
```

内容类似：

```bash
#!/bin/sh

echo "start my app"

/usr/bin/my_app &
```

加权限：

```bash
chmod +x /etc/init.d/S99app
```

## systemd 方式

创建：

```bash
/etc/systemd/system/my_app.service
```

内容：

```ini
[Unit]
Description=My App
After=network.target

[Service]
ExecStart=/usr/bin/my_app
Restart=always

[Install]
WantedBy=multi-user.target
```

启用：

```bash
systemctl enable my_app
systemctl start my_app
```

---

# 13. 网络问题

## 常见现象

```text
ping 不通
有网卡但没 IP
DNS 不通
能 ping IP 不能 ping 域名
Wi-Fi 连不上
SSH 连不上
```

## 排查顺序

先看网卡：

```bash
ip link
ip addr
```

看路由：

```bash
ip route
```

测试 IP：

```bash
ping 8.8.8.8
```

测试 DNS：

```bash
ping www.baidu.com
```

看 DNS：

```bash
cat /etc/resolv.conf
```

## 常见判断

### 有 IP，但不能上网

查网关：

```bash
ip route
```

需要有类似：

```text
default via 192.168.1.1 dev eth0
```

### 能 ping 8.8.8.8，不能 ping 域名

DNS 问题，配置：

```bash
echo "nameserver 8.8.8.8" > /etc/resolv.conf
```

### SSH 连不上

板端确认 sshd 是否启动：

```bash
ps | grep ssh
netstat -tlnp | grep 22
```

Ubuntu 上：

```bash
sudo systemctl status ssh
sudo systemctl start ssh
```

---

# 14. 低频偶现重启 / 死机

这个是最难排查的一类。

## 常见现象

```text
设备运行几小时/几天后重启
没有明显日志
偶现卡死
偶现 watchdog reset
偶现 kernel panic
```

## 常见原因

### 硬件类

```text
电源跌落
电池瞬断
电源纹波大
DDR 不稳定
温度过高
ESD 干扰
外设短路或浪涌
```

### 软件类

```text
内存泄漏
野指针
数组越界
死锁
线程栈溢出
文件系统写坏
驱动中断风暴
watchdog 没喂
内核 panic
OOM killer
```

## 排查思路

第一步，看上一次日志：

```bash
dmesg
logread
journalctl -b -1
```

嵌入式小系统可能没有 journalctl，可以自己做日志持久化。

第二步，看是否 OOM：

```bash
dmesg | grep -i oom
dmesg | grep -i killed
```

第三步，看是否 watchdog：

```bash
dmesg | grep -i watchdog
```

第四步，看内存变化：

```bash
free -m
cat /proc/meminfo
top
```

第五步，看进程是否异常退出：

```bash
ps
top
```

## 解决方向

建议产品里加入：

```text
1. 持久化日志
2. 看门狗复位原因记录
3. 应用异常退出自动重启
4. 内存监控
5. 温度/电压监控
6. coredump
7. panic log 保存
```

如果芯片支持 reset reason 寄存器，启动后第一时间读取并保存。

---

# 15. 内存泄漏 / OOM

## 常见现象

系统越跑越慢，最后应用被 kill：

```text
Out of memory
Killed process xxx
```

## 排查命令

看内存：

```bash
free -m
cat /proc/meminfo
top
```

看某个进程：

```bash
cat /proc/<pid>/status
cat /proc/<pid>/smaps
```

持续观察：

```bash
watch -n 1 free -m
```

## 解决方向

应用层：

```text
malloc/free 配对
线程退出是否释放资源
文件句柄是否关闭
socket 是否关闭
图片/音频 buffer 是否释放
```

工具：

```bash
valgrind
asan
mtrace
```

嵌入式设备资源紧张，很多时候不能直接跑 valgrind，可以在 Ubuntu x86 上先复现一部分逻辑。

---

# 16. CPU 占用高 / 实时性差

## 常见现象

```text
界面卡顿
音频卡顿
摄像头丢帧
网络延迟大
控制响应慢
```

## 排查命令

```bash
top
htop
ps
cat /proc/interrupts
```

看线程：

```bash
top -H -p <pid>
```

看中断：

```bash
watch -n 1 cat /proc/interrupts
```

## 常见原因

```text
某个线程死循环
日志打印太多
中断风暴
驱动频繁唤醒
内存拷贝太多
没有用 DMA
优先级设置不合理
```

## 解决方向

```text
减少高频日志
用 DMA/零拷贝
调整线程优先级
绑定 CPU affinity
优化锁
减少轮询，改成阻塞等待
```

---

# 17. 驱动中断问题

## 常见现象

```text
中断不触发
中断一直触发
系统卡死
CPU 占用很高
```

## 排查方法

看中断计数：

```bash
cat /proc/interrupts
```

如果某个中断数字疯狂增长，可能是中断风暴。

常见原因：

```text
中断极性配置错
GPIO 电平中断没有清除
外设中断状态位没有清除
硬件引脚悬空
驱动里没有 ack interrupt
```

## 解决方向

中断函数里要注意：

```c
irqreturn_t xxx_irq_handler(int irq, void *data)
{
    /*
     * 1. 读取外设中断状态
     * 2. 清除外设中断标志
     * 3. 唤醒线程/调度 workqueue
     * 4. 不要在中断里做耗时操作
     */
    return IRQ_HANDLED;
}
```

耗时操作放到：

```text
workqueue
threaded irq
tasklet
kthread
```

---

# 18. 版本管理和 SDK 太大

## 常见现象

```text
代码几十 GB
VSCode 打开卡死
Git 管理困难
不知道哪些是自己改的
多人协作混乱
```

## 解决方向

### 1）不要整个 SDK 都用 VSCode 索引

`.vscode/settings.json` 可以排除大目录：

```json
{
  "files.watcherExclude": {
    "**/build/**": true,
    "**/output/**": true,
    "**/.git/**": true,
    "**/dl/**": true
  },
  "search.exclude": {
    "**/build/**": true,
    "**/output/**": true,
    "**/dl/**": true
  }
}
```

### 2）用 repo 管理多仓库

大型嵌入式 Linux SDK 很适合拆成：

```text
manifest.git
bootloader.git
kernel.git
buildroot.git
module_driver.git
app.git
third_party.git
```

### 3）保留 patch

对厂商 SDK 的修改建议：

```bash
git diff > my_fix.patch
```

或者每个模块单独建分支：

```bash
git checkout -b feature/icm42688-driver
```

---

# 19. OTA / 升级问题

## 常见现象

```text
升级失败变砖
升级后启动不了
断电后系统损坏
版本回滚失败
```

## 常见方案

产品级系统一般要做：

```text
A/B 分区
boot flag
升级校验
断电保护
回滚机制
版本号管理
签名校验
```

典型结构：

```text
bootloader
kernel_A
rootfs_A
kernel_B
rootfs_B
data
factory
```

升级流程：

```text
当前运行 A
下载升级包到 B
校验 B
设置 boot flag = B
重启
B 启动成功后标记 confirmed
如果 B 启动失败，bootloader 回滚 A
```

---

# 20. 新手最容易忽略的排查原则

## 原则 1：先看日志

```bash
dmesg
logread
journalctl
```

不要一上来就改代码。

---

## 原则 2：先确认硬件通不通

比如 I2C 设备：

```bash
i2cdetect -y 0
```

SPI 设备：

```bash
看示波器/逻辑分析仪
```

GPIO：

```bash
cat /sys/kernel/debug/gpio
```

---

## 原则 3：先确认设备树是否生效

很多时候不是驱动错，是 DTS 根本没生效。

```bash
dtc -I fs -O dts /proc/device-tree > running.dts
```

---

## 原则 4：先确认驱动有没有加载

```bash
lsmod
dmesg | grep xxx
```

---

## 原则 5：先分层定位

不要一句“摄像头不工作”就直接改 sensor 驱动。

应该拆成：

```text
I2C 能读 sensor ID 吗？
MCLK 有吗？
reset/pwdn 对吗？
MIPI 有信号吗？
CSI 有报错吗？
V4L2 节点出来了吗？
应用打开格式对吗？
```

---

# 21. 一个通用排查模板

以后你遇到嵌入式 Linux 问题，可以按这个模板排查：

```text
1. 问题现象是什么？
   - 完全不启动？
   - 驱动不 probe？
   - 应用崩溃？
   - 外设没反应？

2. 最近改了什么？
   - DTS？
   - Kernel config？
   - 驱动？
   - rootfs？
   - 应用？

3. 日志是什么？
   - 串口日志
   - dmesg
   - 应用日志

4. 硬件基础是否正常？
   - 电源
   - 时钟
   - reset
   - 通信波形

5. 软件链路是否正常？
   - DTS 是否生效
   - 驱动是否加载
   - 设备节点是否生成
   - 应用是否调用正确

6. 是否能最小化复现？
   - 单独 i2cget
   - 单独 v4l2-ctl
   - 单独 aplay
   - 单独 gpio 操作

7. 最后再改代码。
```

---

# 22. 对你这种 MCU 转嵌入式 Linux 的建议

你之前做 MCU，思维一般是：

```text
配置寄存器 -> 初始化外设 -> 写业务逻辑
```

Linux 里要换成：

```text
设备树描述硬件
驱动匹配设备
内核生成设备节点
用户态通过标准接口访问
应用程序跑业务
```

比如 ICM42688 IMU，在 MCU 里你可能这样：

```text
I2C 初始化
读 WHO_AM_I
配置寄存器
定时读取加速度/陀螺仪
```

Linux 里更像这样：

```text
DTS 添加 I2C 设备节点
驱动通过 compatible 匹配
probe 中读取 WHO_AM_I
注册 IIO 设备
用户态通过 /sys/bus/iio 或 /dev/iio 读取数据
```

所以嵌入式 Linux 最重要的是建立“分层思维”。

---

# 最后总结

嵌入式 Linux 常见难题主要集中在这几类：

```text
1. 编译环境问题
2. 烧录启动问题
3. 设备树问题
4. 驱动 probe 问题
5. GPIO/Pinctrl 复用问题
6. I2C/SPI/UART 通信问题
7. Camera/V4L2 问题
8. Audio/ALSA/ASoC 问题
9. 文件系统和动态库问题
10. 开机自启动问题
11. 网络问题
12. 内存泄漏/OOM
13. 低频死机/重启
14. OTA 升级可靠性
15. 大型 SDK 管理问题
```

你可以记住一句话：

> 嵌入式 Linux 排查问题，不要一上来就改代码。先看日志，再看设备树，再看驱动加载，再看设备节点，再看用户态调用，最后才改代码。


--------
------
------
明白了，你问的不是“开发时编译/驱动调试难题”，而是**产品上市/测试/量产后，用户能感受到的 Bug**。
嵌入式 Linux 产品上的 Bug，通常表现为：**卡、死、重启、连不上、识别不到、声音/画面异常、配置丢失、升级变砖、偶现异常**。

下面按“产品现象 → 可能原因 → 排查方案 → 解决方案”讲。

---

# 1. 低频偶现重启

这是产品上最常见、也最难查的问题。

## 产品表现

用户反馈：

```text
设备用着用着自动重启
一天重启一次
几天重启一次
插拔外设后重启
晚上没人操作也重启
运行一段时间后突然回到开机界面
```

## 常见原因

分两大类：**硬件复位**和**软件崩溃复位**。

### 硬件类

```text
电源瞬间跌落
适配器带载能力不够
电源纹波大
电池接触不良
DDR 不稳定
温度过高
ESD 静电干扰
外设短路/浪涌
```

### 软件类

```text
应用程序崩溃后系统重启
kernel panic
内存泄漏导致 OOM
watchdog 没喂狗
驱动死锁
文件系统损坏
线程卡死触发看门狗
```

## 排查方案

产品上一定要做“重启原因记录”。

启动后第一时间记录：

```text
1. 本次启动时间
2. 上次运行时长
3. reset reason
4. watchdog 标志
5. kernel panic 日志
6. 应用崩溃日志
7. 内存剩余值
8. CPU 温度/电压
```

Linux 侧可以查：

```bash
dmesg
logread
journalctl -b -1
cat /proc/meminfo
cat /proc/uptime
```

如果是嵌入式裁剪系统，没有 journalctl，就要自己加日志持久化。

## 解决方案

产品级建议加：

```text
1. reset reason 记录
2. watchdog 复位前保存关键状态
3. kernel panic log 保存到 pstore/ramoops
4. 应用 coredump
5. 内存泄漏监控
6. 关键业务心跳
7. 电压/温度采样日志
```

实际排查时，重点判断：

```text
是整机硬复位？
是 kernel panic？
是 watchdog reset？
是应用崩溃后被守护进程拉起？
是 OOM killer 杀进程？
```

---

# 2. 产品偶现卡死 / 无响应

## 产品表现

```text
屏幕卡住
按键没反应
网络连不上
摄像头画面停住
音频停止
设备还亮着但不能操作
只能断电重启
```

## 常见原因

```text
主应用死锁
驱动阻塞
线程优先级设计不合理
CPU 被某个进程打满
内存耗尽
中断风暴
文件系统阻塞
网络 socket 阻塞
日志打印阻塞
```

## 排查方案

如果还能进 shell：

```bash
top
ps
cat /proc/interrupts
free -m
dmesg | tail
```

看 CPU 是否被打满：

```bash
top
```

看中断是否疯狂增长：

```bash
watch -n 1 cat /proc/interrupts
```

看内存是否耗尽：

```bash
free -m
cat /proc/meminfo
```

如果完全卡死，要靠：

```text
串口日志
硬件看门狗
内核 hung task 检测
sysrq
ramoops
外部 MCU 监控
```

## 解决方案

产品上建议：

```text
1. 主应用加心跳
2. watchdog 不要只在一个线程喂
3. 关键线程互相监控
4. 避免主线程做阻塞 IO
5. socket/read/write 设置超时
6. 日志异步输出
7. 驱动中断里不要做耗时操作
8. 定期记录 CPU/内存/线程状态
```

---

# 3. 开机慢 / 卡 logo / 启动失败

## 产品表现

```text
开机时间很长
卡在 logo
偶现开不了机
升级后无法启动
冷启动失败，热启动正常
```

## 常见原因

```text
bootloader 等待超时
内核启动慢
rootfs 挂载慢
应用等待网络/外设超时
文件系统检查耗时
某个服务启动阻塞
外设初始化失败卡住
```

## 排查方案

看启动耗时：

```bash
dmesg
dmesg | less
```

如果有 systemd：

```bash
systemd-analyze
systemd-analyze blame
systemd-analyze critical-chain
```

如果是 BusyBox init，就在启动脚本里加时间戳：

```sh
echo "$(date) start wifi" >> /tmp/boot_time.log
echo "$(date) start app" >> /tmp/boot_time.log
```

## 解决方案

```text
1. 外设初始化加超时
2. 网络初始化不要阻塞主界面
3. 不重要服务延后启动
4. 减少启动脚本里的 sleep
5. 开机阶段日志分级
6. 应用启动流程异步化
```

最怕这种代码：

```c
while (!device_ready) {
    sleep(1);
}
```

产品上应该改成：

```text
等待有限时间
失败后降级运行
后台继续重试
界面提示异常
```

---

# 4. Wi-Fi / 网络偶现连不上

## 产品表现

```text
首次配网成功，重启后连不上
Wi-Fi 偶现掉线
连上 Wi-Fi 但没有网
能 ping IP，不能访问域名
弱信号下频繁断线
DHCP 获取不到 IP
```

## 常见原因

```text
Wi-Fi 驱动不稳定
DHCP 超时
DNS 配置错误
路由器兼容性问题
2.4G/5G 支持问题
国家码/信道问题
低功耗导致断连
网络状态机设计差
断线重连逻辑不完善
```

## 排查方案

```bash
ifconfig
ip addr
ip route
cat /etc/resolv.conf
ping 网关
ping 8.8.8.8
ping 域名
dmesg | grep -i wlan
dmesg | grep -i wifi
```

重点区分：

```text
没连上 AP
连上 AP 但没拿到 IP
有 IP 但没网关
有网关但 DNS 不通
DNS 通但业务服务器不通
```

## 解决方案

产品网络状态机要设计成这样：

```text
未连接
正在连接
已连接无 IP
已获取 IP
互联网可达
业务服务器可达
断线重连
配网失败
```

不要只判断：

```c
wifi_connected == true
```

因为“连上 Wi-Fi”不等于“能上网”。

---

# 5. 摄像头黑屏 / 花屏 / 偶现打不开

## 产品表现

```text
打开摄像头黑屏
画面花屏
颜色异常
画面卡住
偶现 /dev/video0 不存在
切换分辨率后异常
长时间预览后停止
```

## 常见原因

```text
sensor 初始化时序不稳定
MIPI lane 配置错误
sensor 时钟不稳定
reset/pwdn 时序不对
ISP 参数不匹配
V4L2 buffer 泄漏
应用没有释放 video 设备
多进程抢占摄像头
内存带宽不足
```

## 排查方案

```bash
ls /dev/video*
v4l2-ctl --list-devices
v4l2-ctl -d /dev/video0 --all
media-ctl -p
dmesg | grep -i sensor
dmesg | grep -i mipi
dmesg | grep -i csi
dmesg | grep -i isp
```

测试抓帧：

```bash
v4l2-ctl -d /dev/video0 --stream-mmap --stream-count=100
```

## 解决方案

```text
1. sensor 上电/复位时序严格按 datasheet
2. 打开失败后释放资源再重试
3. 应用退出时 close video fd
4. buffer 队列异常时重新初始化
5. 降低分辨率/帧率验证是否带宽问题
6. 记录 camera open/close/stream on/off 日志
```

---

# 6. 音频无声 / 杂音 / 卡顿

## 产品表现

```text
播放无声
录音无声
偶现爆音
音频卡顿
左右声道反
声音变调
切歌后无声
通话一段时间后没声音
```

## 常见原因

```text
ALSA 通路没打开
codec 寄存器配置错误
I2S 主从模式错
采样率不匹配
buffer underrun
CPU 忙导致音频线程调度不及时
功放 enable 时序不对
音频资源没有释放
```

## 排查方案

```bash
aplay -l
arecord -l
amixer
alsamixer
dmesg | grep -i asoc
dmesg | grep -i i2s
dmesg | grep -i codec
```

播放测试：

```bash
aplay test.wav
speaker-test -c 2 -t wav
```

录音测试：

```bash
arecord -f S16_LE -r 16000 -c 1 test.wav
```

## 解决方案

```text
1. 固定音频状态机
2. 播放前检查声卡是否存在
3. 播放异常后重新打开 PCM
4. 关键音频线程提高优先级
5. 减少高频日志
6. 设置合适 ALSA buffer/period
7. 功放 enable/disable 加延时保护
```

---

# 7. 屏幕显示异常

## 产品表现

```text
花屏
白屏
黑屏
闪屏
颜色不对
横竖屏错乱
UI 卡顿
触摸位置偏移
休眠唤醒后不亮屏
```

## 常见原因

```text
LCD 初始化参数错误
RGB/MIPI 时序错误
背光 PWM 异常
显存 buffer 异常
LVGL 刷新区域错误
双缓冲同步问题
触摸坐标映射错误
休眠唤醒流程不完整
```

## 排查方案

```text
1. 先确认背光是否亮
2. 再确认 LCD reset 是否正常
3. 再确认时钟/数据线是否有波形
4. 再确认 framebuffer 是否正常
5. 最后看 UI 应用
```

Linux 下可查：

```bash
ls /dev/fb*
cat /sys/class/graphics/fb0/modes
dmesg | grep -i drm
dmesg | grep -i fb
dmesg | grep -i lcd
```

## 解决方案

```text
1. 严格匹配屏厂 init code
2. 休眠/唤醒重新走 panel resume
3. UI 刷新和显示驱动做好同步
4. 降低刷新率验证带宽问题
5. 背光单独做异常保护
```

---

# 8. 按键 / 触摸偶现失效

## 产品表现

```text
按键没反应
按一次触发多次
长按识别错误
触摸漂移
触摸无响应
唤醒后按键失效
```

## 常见原因

```text
GPIO 中断极性错
按键消抖不合理
按键状态机异常
输入事件丢失
触摸 IC I2C 通信异常
休眠后没有重新初始化
中断脚悬空
```

## 排查方案

看输入设备：

```bash
cat /proc/bus/input/devices
ls /dev/input/
```

测试事件：

```bash
evtest
hexdump /dev/input/event0
```

看中断：

```bash
cat /proc/interrupts
```

## 解决方案

```text
1. 按键消抖 20~50ms
2. 长按/短按状态机分离
3. 唤醒后重新同步按键状态
4. 触摸 I2C 异常后 reset IC
5. 中断里只上报事件，不做复杂逻辑
```

---

# 9. 配置丢失 / 参数保存失败

## 产品表现

```text
设置了 Wi-Fi，重启后丢失
音量设置丢失
用户配置恢复默认
设备 ID 丢失
升级后配置消失
```

## 常见原因

```text
rootfs 是只读
配置写到了临时目录 /tmp
写入后没有 sync
掉电时正在写 flash
配置文件损坏
升级覆盖了配置分区
没有做版本兼容
```

## 排查方案

```bash
mount
df -h
cat /proc/mounts
```

看配置保存路径：

```bash
ls -l /etc
ls -l /data
ls -l /userdata
```

## 解决方案

产品中配置应该放在独立可写分区，例如：

```text
/data
/userdata
/config
```

保存配置建议：

```text
1. 写临时文件
2. fsync
3. rename 原子替换
4. sync
5. 加 CRC 校验
6. 保留备份配置
```

不要直接这样写：

```c
fopen("config.json", "w");
fprintf(...);
fclose(...);
```

更可靠的方式是：

```text
config.json.tmp -> fsync -> rename config.json -> sync
```

---

# 10. 升级失败 / OTA 变砖

## 产品表现

```text
升级过程中断电后不开机
升级后卡 logo
升级后配置丢失
升级后版本号没变
升级失败无法回退
```

## 常见原因

```text
升级包校验不完整
写 flash 中断
分区地址错误
boot flag 错误
升级脚本没有异常处理
没有 A/B 分区
没有回滚机制
```

## 排查方案

升级流程必须记录：

```text
下载开始
下载完成
MD5/SHA 校验
解包完成
写入分区
写入完成
设置 boot flag
重启
新系统启动确认
```

## 解决方案

产品级 OTA 推荐：

```text
A/B 分区
升级包签名
断电保护
启动确认机制
失败回滚
版本兼容检查
配置分区独立
```

典型流程：

```text
当前运行 A
下载新版本到 B
校验 B
设置下次启动 B
重启
B 启动成功后标记 confirmed
如果 B 启动失败，bootloader 回滚 A
```

---

# 11. 长时间运行后越来越慢

## 产品表现

```text
刚开机很流畅
运行几小时后变卡
运行几天后操作延迟明显
最后死机或重启
```

## 常见原因

```text
内存泄漏
文件句柄泄漏
线程泄漏
socket 泄漏
日志文件过大
数据库越来越大
flash 空间满
CPU 长期高负载
```

## 排查方案

长期压测时周期记录：

```bash
date
free -m
df -h
top -n 1
cat /proc/meminfo
ls /proc/进程pid/fd | wc -l
```

看文件句柄：

```bash
ls /proc/<pid>/fd
```

看线程数：

```bash
ls /proc/<pid>/task | wc -l
```

## 解决方案

```text
1. 内存分配必须有释放
2. socket close
3. 文件 close
4. 线程退出 join/detach
5. 日志轮转
6. 限制缓存大小
7. 异常路径也要释放资源
```

很多泄漏不是正常流程漏，而是**异常流程漏**：

```text
网络断开
设备打开失败
文件写失败
摄像头 start 失败
音频播放中断
```

这些路径最容易忘记释放资源。

---

# 12. 量产后个别机器异常

## 产品表现

```text
大部分机器正常
少数机器不开机
少数机器 Wi-Fi 差
少数机器摄像头不稳定
少数机器按键不灵
```

## 常见原因

```text
硬件一致性问题
焊接问题
器件批次差异
flash 坏块
DDR 体质差
电源余量不足
传感器 ID 不同
屏幕/摄像头模组混料
```

## 排查方案

产品要记录：

```text
硬件版本
PCB 版本
BOM 版本
软件版本
bootloader 版本
kernel 版本
rootfs 版本
设备 SN
传感器 ID
屏幕 ID
摄像头 ID
Wi-Fi 模组版本
```

## 解决方案

```text
1. 启动时读取关键外设 ID
2. 根据硬件版本选择不同配置
3. 量产测试覆盖电源/通信/老化
4. 异常机器保留完整日志
5. 关键器件做兼容表
```

---

# 13. 产品时间异常

## 产品表现

```text
日志时间不对
证书校验失败
HTTPS 连不上
定时任务不执行
录像文件时间错乱
```

## 常见原因

```text
没有 RTC
RTC 电池没电
NTP 同步失败
时区错误
系统每次开机从 1970 开始
```

## 排查方案

```bash
date
hwclock
timedatectl
```

看 NTP：

```bash
ntpd -q
chronyc sources
```

## 解决方案

```text
1. 有网络时 NTP 校时
2. 无网络时使用 RTC
3. 没 RTC 的产品保存最后一次时间
4. 日志里同时记录 uptime
5. HTTPS 前确保时间有效
```

尤其是摄像头、门铃、记录仪这类产品，时间错误会导致日志完全没法分析。

---

# 14. 日志太少，问题无法复现

## 产品表现

用户说：

```text
偶尔坏
偶尔卡
偶尔重启
偶尔连不上
```

但研发拿回来没复现。

## 根本问题

不是 Bug 不存在，而是**产品没有可诊断能力**。

## 解决方案

产品必须内置诊断系统：

```text
1. 环形日志
2. 崩溃日志
3. 重启原因
4. 关键状态机日志
5. 网络状态日志
6. 外设异常计数
7. 最近 N 次错误码
8. OTA 过程日志
9. 应用版本/配置快照
```

建议日志分区结构：

```text
/data/log/
    app.log
    kernel.log
    boot.log
    crash.log
    ota.log
    net.log
    last_state.json
```

---

# 15. 产品级 Bug 的通用排查方法

以后你遇到产品 Bug，可以按这个流程：

```text
第一步：确认现象
- 是重启？
- 是卡死？
- 是应用崩？
- 是外设异常？
- 是网络异常？

第二步：确认范围
- 单台机器？
- 多台机器？
- 某个批次？
- 某个版本？
- 某个场景？

第三步：确认时间规律
- 开机马上出现？
- 运行几小时出现？
- 弱网出现？
- 高温出现？
- 低电量出现？
- 升级后出现？

第四步：收集日志
- boot log
- kernel log
- app log
- crash log
- reset reason
- 版本信息

第五步：分层定位
- 硬件？
- bootloader？
- kernel？
- driver？
- rootfs？
- app？
- server？

第六步：做压力复现
- 长时间运行
- 断网重连
- 频繁开关外设
- 断电重启
- 高低温
- 弱信号
- 大数据量
```

---

# 16. 产品上最该提前做的“防 Bug 设计”

真正成熟的嵌入式 Linux 产品，不是没有 Bug，而是**出问题能定位、能恢复、不会变砖**。

建议你重点做这些：

```text
1. 看门狗 watchdog
2. 应用守护进程
3. 崩溃自动重启
4. coredump 保存
5. kernel panic 保存
6. reset reason 记录
7. OTA A/B 回滚
8. 日志持久化
9. 配置文件双备份
10. 网络断线重连状态机
11. 外设异常自动恢复
12. 关键线程心跳
13. flash 空间监控
14. 内存泄漏监控
15. 版本/硬件信息记录
```

---

# 17. 简单总结

产品上常见的嵌入式 Linux Bug，可以归纳成这几句话：

```text
重启：查 reset reason、watchdog、panic、OOM、电源。
卡死：查 CPU、内存、死锁、中断风暴、阻塞 IO。
连不上网：分清 Wi-Fi、IP、网关、DNS、服务器。
外设异常：查电源、reset、设备树、驱动、设备节点、应用释放。
配置丢失：查写入路径、sync、掉电保护、配置分区。
升级变砖：查 OTA 校验、A/B 分区、boot flag、回滚。
长期变慢：查内存、fd、线程、日志、flash 空间。
偶现问题：靠日志、心跳、coredump、ramoops、状态快照。
```

最核心的一句话是：

> 产品级嵌入式 Linux 开发，重点不是“出了 Bug 再猜”，而是提前设计好日志、恢复、回滚、监控，让产品出问题后能自己留下证据。
