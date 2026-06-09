公司里对 **Linux 内核性能优化** 和 **系统裁剪**，一般不是上来就乱改内核，而是围绕产品目标来做：

比如你的 **君正 X2600 / 嵌入式 Linux 设备**，公司里通常关注这些东西：

1. **启动速度**
2. **内存占用**
3. **CPU 占用**
4. **IO / 文件系统速度**
5. **驱动稳定性**
6. **功耗**
7. **系统镜像大小**
8. **不需要的功能尽量去掉**

---

# 一、公司里一般怎么做 Linux 内核性能优化？

## 1. 先定目标，不是盲目优化

比如产品要求：

```text
开机到业务程序运行 < 8 秒
系统空闲内存 > 50MB
CPU 空闲率 > 80%
摄像头/IMU/SPI/I2C 不能卡顿
日志不能刷爆 CPU
系统镜像不能超过 64MB
```

所以公司优化一般先问：

```text
现在慢在哪里？
是内核启动慢？
是 rootfs 挂载慢？
是驱动初始化慢？
是应用启动慢？
是 CPU 占用高？
是内存不够？
```

不能一上来就说“优化内核”。

---

# 二、内核性能优化一般优化啥？

## 1. 启动时间优化

这个很常见。

看启动日志：

```bash
dmesg
dmesg -T
```

重点看哪些驱动初始化慢。

比如你会看到：

```text
[    0.800000] spi init...
[    1.500000] usb init...
[    3.200000] filesystem mounted
[    5.000000] app started
```

如果某个驱动初始化卡了 1 秒、2 秒，公司里就会重点查它。

常见优化点：

```text
关掉不用的驱动
把不急用的驱动延后加载
减少 probe 阻塞
减少设备扫描
减少文件系统检查
减少启动脚本
```

比如产品没有 USB，就不要开 USB Host、USB Gadget 一堆东西。

---

## 2. 内存优化

嵌入式 Linux 很看重内存。

常看：

```bash
free -m
cat /proc/meminfo
cat /proc/slabinfo
top
ps
```

内核里常优化：

```text
关闭不用的内核模块
关闭不用的文件系统
关闭不用的网络协议
关闭 debug 选项
关闭 tracing / ftrace / debugfs
减少 buffer/cache 占用
减少驱动静态内存申请
```

比如量产固件一般不会打开：

```text
CONFIG_DEBUG_KERNEL
CONFIG_KALLSYMS_ALL
CONFIG_FTRACE
CONFIG_FUNCTION_TRACER
CONFIG_KPROBES
CONFIG_LOCKDEP
CONFIG_DEBUG_INFO
```

这些调试功能对开发有用，但量产会增加体积和内存占用。

---

## 3. CPU 占用优化

常用命令：

```bash
top
cat /proc/interrupts
cat /proc/softirqs
cat /proc/stat
```

重点看：

```text
哪个进程 CPU 高？
哪个中断疯狂触发？
哪个驱动一直轮询？
哪个线程一直跑？
```

嵌入式里很常见的问题是：

```c
while (1) {
    read_sensor();
}
```

没有 sleep，CPU 直接 100%。

正确做法一般是：

```text
中断触发
阻塞等待
poll/select/epoll
定时器周期采样
工作队列
```

比如 SPI IMU 驱动，如果一直轮询读取，就可能吃 CPU。更好的方式是用数据就绪中断，或者合理降低采样周期。

---

## 4. 中断优化

看：

```bash
cat /proc/interrupts
```

如果某个中断疯狂增加，比如：

```text
gpio-irq: 1000000
spi-irq : 800000
```

那说明可能有问题：

```text
中断没清除
中断触发沿配置错
电平中断一直保持
驱动处理太慢
中断里做了太多事情
```

公司里一般要求：

```text
中断服务函数 ISR 越短越好
复杂处理放 threaded irq / workqueue
不要在中断里大量打印 log
不要在中断里做慢速 I2C/SPI 操作，除非设计允许
```

---

## 5. IO / 文件系统优化

常看：

```bash
mount
df -h
cat /proc/mounts
```

常见优化点：

```text
选择合适文件系统
优化挂载参数
减少频繁写 Flash
减少日志落盘
使用 tmpfs 存临时文件
关键配置再写 Flash
```

比如嵌入式产品常见：

```text
只读 rootfs + 可写 data 分区
squashfs 作为 rootfs
jffs2 / ubifs / ext4 作为数据区
/tmp 用 tmpfs
```

这样可以：

```text
减少 Flash 磨损
提高启动速度
减少文件系统损坏风险
```

---

## 6. 调度和实时性优化

如果产品有电机、音频、视频、传感器采集，就会关心实时性。

常见优化方向：

```text
提高关键线程优先级
使用 SCHED_FIFO / SCHED_RR
减少低优先级任务抢占
减少长时间关中断
减少驱动里的大锁
避免大量 printk
```

但是普通嵌入式产品一般不一定会上 PREEMPT_RT。

常见配置：

```text
CONFIG_PREEMPT
CONFIG_HZ_100
CONFIG_HZ_250
CONFIG_HZ_1000
```

如果是普通 Linux 设备，`HZ=100` 或 `250` 就够了；如果实时性要求高，可能会用 `HZ=1000`，但功耗和 CPU 开销也会上升。

---

## 7. printk / 日志优化

开发阶段疯狂打印：

```c
printk(KERN_INFO "xxx\n");
```

量产阶段就不行。

大量 printk 会影响：

```text
启动速度
CPU 占用
串口输出速度
实时性
Flash 寿命
```

量产一般会：

```text
降低 loglevel
关闭大量 debug log
用动态调试控制
关键错误才打印
```

常见命令：

```bash
cat /proc/sys/kernel/printk
echo 4 > /proc/sys/kernel/printk
```

---

# 三、系统裁剪一般裁剪啥？

系统裁剪分两块：

```text
1. 内核裁剪
2. rootfs 用户空间裁剪
```

---

# 四、内核裁剪一般裁剪啥？

内核裁剪主要改：

```text
kernel defconfig
.config
menuconfig
```

比如你之前说的：

```text
x2600_vast_module_base_linux_sfc_nand_defconfig
```

公司里就是基于这个 defconfig 裁剪。

---

## 1. 裁剪不用的 CPU / 平台支持

嵌入式 SoC 一般只保留自己平台：

```text
只保留 X2600/X2600N 相关平台
其他板级支持不要
其他 SoC 支持不要
```

---

## 2. 裁剪不用的驱动

这个最常见。

比如产品不用这些，就关掉：

```text
不用摄像头：关 V4L2 sensor 驱动
不用音频：关 ALSA / codec 驱动
不用蓝牙：关 Bluetooth
不用 WiFi：关 WLAN
不用 USB：关 USB Host / Gadget
不用输入设备：关 keyboard / mouse / joystick
不用显示：关 DRM / framebuffer / LCD
不用触摸：关 touchscreen
不用电池：关 power supply
不用 RTC：关 RTC
不用 LED：关 LED class
不用 PWM：关 PWM
```

但是注意：

**不要看到不用就乱关。**

有些驱动虽然你没直接用，但系统依赖它，比如：

```text
GPIO
PINCTRL
CLOCK
RESET
REGULATOR
DMA
SPI
I2C
IRQ
TTY
SERIAL
```

这些不能随便关。

---

## 3. 裁剪不用的文件系统

常见文件系统：

```text
ext4
squashfs
jffs2
ubifs
vfat
ntfs
nfs
tmpfs
procfs
sysfs
debugfs
```

量产一般保留：

```text
procfs
sysfs
tmpfs
rootfs 实际使用的文件系统
data 分区实际使用的文件系统
```

不用的可以关：

```text
NTFS
FAT
NFS
CIFS
ISO9660
FUSE
```

如果产品不插 U 盘、不挂网络文件系统，这些很多都可以不要。

---

## 4. 裁剪不用的网络协议

Linux 网络协议很多。

如果产品只用基础 TCP/IP，很多可以关：

```text
IPv6
Bluetooth
CAN
NFC
IRDA
PPP
SLIP
VLAN
Bridge
Netfilter
IP tables
Wireless extensions
```

但是如果产品需要：

```text
WiFi
MQTT
HTTP
OTA
ADB
NFS 调试
```

那相关协议不能关。

开发阶段可以开多一点，量产阶段再裁剪。

---

## 5. 裁剪调试功能

开发阶段常开：

```text
debugfs
procfs 扩展
ftrace
kprobes
lockdep
kernel hacking
dynamic debug
debug info
```

量产一般关掉大部分：

```text
CONFIG_DEBUG_KERNEL=n
CONFIG_DEBUG_INFO=n
CONFIG_FTRACE=n
CONFIG_KPROBES=n
CONFIG_LOCKDEP=n
CONFIG_PROFILING=n
```

这样可以减少：

```text
内核体积
内存占用
CPU 开销
攻击面
```

---

## 6. 裁剪模块支持

有些产品会关闭模块加载：

```text
CONFIG_MODULES=n
```

好处：

```text
系统更简单
安全性更高
防止乱加载 ko
内核和驱动固定
```

坏处：

```text
调试麻烦
驱动不能动态加载
升级某个驱动不方便
```

开发阶段一般保留模块，量产可以考虑关闭。

---

# 五、rootfs 系统裁剪一般裁剪啥？

rootfs 裁剪比内核裁剪更常见，也更安全。

## 1. 裁剪命令工具

BusyBox 里很多命令可以裁剪。

比如量产不一定需要：

```text
vi
telnet
ftp
tftp
wget
nc
nslookup
traceroute
top
ps
killall
hexdump
strings
```

但是开发阶段这些很有用。

所以常见做法是：

```text
开发版 rootfs：工具多，方便调试
量产版 rootfs：工具少，只保留必要功能
```

---

## 2. 裁剪库文件

看应用依赖：

```bash
ldd app
```

只保留实际需要的库：

```text
libc
libpthread
libm
libdl
libstdc++
libgcc_s
驱动/业务需要的库
```

不用的库删掉。

---

## 3. 裁剪服务和启动脚本

很多系统启动慢，不是内核慢，而是脚本慢。

常见裁剪：

```text
不用的 daemon
不用的网络服务
不用的 shell 脚本
不用的自动挂载
不用的调试脚本
不用的测试程序
```

比如：

```text
telnetd
sshd
ftpd
httpd
udevd
dbus
avahi
cron
syslogd
klogd
```

产品不用就可以去掉。

---

## 4. 裁剪日志系统

量产一般不希望一直写 Flash。

常见做法：

```text
日志默认只输出到串口或内存
错误日志按需保存
限制日志大小
用环形日志
/tmp/log 放 tmpfs
```

否则频繁写 Flash 会影响寿命。

---

## 5. 裁剪测试程序

开发阶段会有很多测试程序：

```text
spi_test
i2c_test
gpio_test
sensor_test
camera_test
factory_test
iperf
memtester
stress
```

量产时一般只保留必要的：

```text
产测程序
升级程序
业务程序
恢复工具
```

---

# 六、嵌入式公司常见版本划分

一般会分：

## 开发版

```text
log 多
debug 功能多
工具多
支持 ko 动态加载
可以 telnet/ssh
方便排查问题
```

## 测试版

```text
接近量产
保留部分日志
保留部分调试工具
方便测试定位
```

## 量产版

```text
log 少
工具少
关闭 debug
关闭不用服务
关闭不用驱动
镜像更小
启动更快
安全性更好
```

---

# 七、以内核配置来说，公司一般怎么操作？

一般流程是：

```bash
make xxx_defconfig
make menuconfig
make savedefconfig
```

然后把差异合入公司的默认配置。

比如你现在这种情况：

```text
主配置：
x2600e_vast_v10_nand_5.10_factory_defconfig

内核配置：
x2600_vast_module_base_linux_sfc_nand_defconfig
```

公司里一般会：

```text
1. 先基于默认 defconfig 编译通过
2. menuconfig 修改内核功能
3. 对比 .config 和原 defconfig 差异
4. 确认每个配置项为什么要改
5. 把必要项合入默认 defconfig
6. 全量编译验证
7. 烧录测试
8. 回归功能
```

重点是：

```text
每个配置项都要知道为什么打开、为什么关闭
```

而不是随便裁。

---

# 八、裁剪最容易出问题的地方

## 1. 关掉了驱动依赖

比如你想用 SPI IMU，但是误关了：

```text
CONFIG_SPI
CONFIG_IIO
CONFIG_IIO_BUFFER
CONFIG_TRIGGERED_BUFFER
CONFIG_KFIFO_BUF
```

结果驱动就加载不了。

---

## 2. 关掉了 sysfs/procfs

很多应用依赖：

```text
/sys
/proc
```

比如查看 GPIO、IIO、网络、进程状态都需要。

一般不能关。

---

## 3. 关掉了 devtmpfs / mdev / udev

如果 `/dev` 设备节点不生成，应用可能打不开设备：

```text
/dev/spidev0.0
/dev/ttyS0
/dev/video0
/dev/input/event0
```

---

## 4. 关掉了网络功能导致 OTA 失败

比如 OTA 需要：

```text
TCP/IP
HTTP
DNS
网卡驱动
WiFi/以太网驱动
```

这些不能乱裁。

---

# 九、一个比较实际的裁剪思路

你可以按这个顺序来：

```text
第一步：先保证系统完整跑通
第二步：确认产品实际需要哪些功能
第三步：先裁 rootfs，不先动内核
第四步：再裁内核中明显不用的驱动
第五步：关闭 debug 配置
第六步：优化启动脚本
第七步：看 dmesg、free、top 验证效果
第八步：做完整功能回归
```

我不建议新手一开始就疯狂裁内核。

因为内核裁坏了，问题很隐蔽，比如：

```text
设备节点没了
驱动 probe 不到
模块依赖缺失
文件系统挂不上
网络不可用
应用启动失败
```

---

# 十、公司里真正常用的性能分析命令

你可以先记这些：

```bash
dmesg                 # 看内核启动日志
free -m               # 看内存
top                   # 看 CPU
ps                    # 看进程
cat /proc/meminfo     # 看详细内存
cat /proc/interrupts  # 看中断
cat /proc/cpuinfo     # 看 CPU
cat /proc/slabinfo    # 看内核 slab 内存
df -h                 # 看文件系统空间
mount                 # 看挂载参数
lsmod                 # 看加载的模块
du -sh *              # 看 rootfs 哪些文件占空间
```

如果系统支持，也可以用：

```bash
bootchart
perf
ftrace
strace
iotop
vmstat
```

不过嵌入式裁剪系统里不一定都有。

---

# 十一、简单总结

公司里 Linux 内核性能优化主要优化：

```text
启动速度
CPU 占用
内存占用
中断负载
驱动效率
文件系统 IO
日志开销
实时性
功耗
```

系统裁剪主要裁：

```text
不用的驱动
不用的文件系统
不用的网络协议
不用的调试功能
不用的命令工具
不用的库
不用的后台服务
不用的测试程序
不用的日志输出
```

但是核心原则是：

```text
先跑通，再测量，再定位瓶颈，再裁剪优化。
不要凭感觉乱关配置。
```

对你现在的 X2600 来说，最实用的路线是：

```text
1. 保留开发版配置，方便调试
2. 单独做一份量产 defconfig
3. 先裁 rootfs 和启动脚本
4. 再裁内核不用的驱动和 debug
5. 每裁一批就编译、烧录、回归测试
```

这样是公司里比较稳的做法。
