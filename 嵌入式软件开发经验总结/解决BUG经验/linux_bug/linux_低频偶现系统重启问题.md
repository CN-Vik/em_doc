嵌入式 Linux 产品“低频偶现重启”，最难的点不是原因复杂，而是**现场复现概率低，重启后现场信息丢失**。所以核心思路不是一上来猜原因，而是先回答三个问题：

1. **到底是不是 Linux 主动重启？**
2. **重启前有没有内核崩溃、看门狗、掉电、异常温度等证据？**
3. **重启后能不能保留上一次死亡现场？**

---

# 一、低频重启常见原因总分类

可以先按这几大类理解：

```text
低频重启
├── 1. 电源问题
│   ├── 瞬间掉电
│   ├── 电压跌落
│   ├── 电源纹波过大
│   ├── 外设拉低电源
│   └── 电池/适配器/USB供电不稳
│
├── 2. 看门狗复位
│   ├── 内核卡死
│   ├── 用户态喂狗程序卡死
│   ├── 高优先级线程死循环
│   ├── 中断风暴
│   └── I/O 阻塞导致喂狗超时
│
├── 3. Kernel Panic / Oops
│   ├── 空指针
│   ├── 越界访问
│   ├── 驱动 bug
│   ├── DMA 踩内存
│   ├── 中断上下文非法操作
│   └── 文件系统/块设备异常
│
├── 4. 用户态主动重启
│   ├── 程序调用 reboot
│   ├── OTA/升级逻辑
│   ├── systemd watchdog
│   ├── 守护进程检测异常后重启系统
│   └── 脚本误触发 reboot
│
├── 5. 内存问题
│   ├── 内存泄漏
│   ├── OOM killer
│   ├── 堆栈溢出
│   ├── slab 泄漏
│   └── 内存被 DMA/驱动踩坏
│
├── 6. 外设/驱动问题
│   ├── Wi-Fi/4G/USB/摄像头/音频驱动异常
│   ├── I2C/SPI 总线死锁
│   ├── SD卡/eMMC异常
│   ├── GPIO中断抖动
│   └── DMA cache一致性问题
│
├── 7. 温度/环境/EMC
│   ├── 高温触发保护
│   ├── 低温器件异常
│   ├── 静电 ESD
│   ├── 电机/继电器/无线模块干扰
│   └── 电源瞬态冲击
│
└── 8. 软件逻辑长期运行问题
    ├── 计数器溢出
    ├── 定时器异常
    ├── 日志写满
    ├── 文件句柄泄漏
    ├── 线程死锁
    └── 资源没有释放
```

---

# 二、第一步：先判断“重启类型”

低频偶现重启，最重要的不是马上查代码，而是先判断重启来源。

## 1. 是掉电复位？

典型特征：

```text
无 kernel panic 日志
无 reboot 命令记录
无 watchdog 记录
系统像突然断电一样重新启动
RTC 时间可能异常
电源芯片/PMIC 可能有掉电标志
```

这类问题通常和硬件、电源、外设瞬时大电流有关。

例如：

```text
4G 模块发射瞬间电流过大
Wi-Fi 发射峰值电流导致 3.3V 下跌
电机启动瞬间拉低系统电源
USB 外设热插拔导致电源抖动
摄像头开流时电源纹波变大
```

这类问题在软件日志里经常看不到明显错误，因为 CPU 已经直接复位了。

---

## 2. 是看门狗复位？

典型特征：

```text
系统运行中突然卡死
一段时间后重启
重启日志里可能有 watchdog reset / wdt reset
喂狗线程可能没有执行
内核可能卡在某个驱动或者中断里
```

看门狗复位常见于：

```text
内核死锁
驱动阻塞
中断风暴
高优先级线程占满 CPU
用户态喂狗进程卡住
文件系统 I/O 长时间阻塞
```

举个例子：

```c
while (1) {
    // 某个驱动等待硬件状态位
    while (!(readl(REG_STATUS) & READY)) {
        // 没有超时退出
    }
}
```

如果这个代码在内核驱动里，就可能导致系统卡死，最后被 watchdog 拉复位。

---

## 3. 是 Kernel Panic？

典型特征：

```text
日志里有 Kernel panic
Oops
Unable to handle kernel NULL pointer dereference
BUG:
Oops: 00000000
Call trace
Backtrace
```

例如：

```text
Unable to handle kernel NULL pointer dereference at virtual address 00000000
PC is at xxx_driver_probe+0x30/0x100
LR is at xxx_init+0x20/0x50
Kernel panic - not syncing: Fatal exception
```

这类一般是驱动 bug、内核模块 bug、非法内存访问。

常见原因：

```text
空指针
野指针
数组越界
释放后继续使用
中断中访问已释放资源
DMA 写坏内存
用户态 ioctl 传入非法参数导致驱动崩溃
```

---

## 4. 是用户态主动重启？

有些产品会有守护进程，比如：

```text
app_monitor
system_watchdog
ota_service
upgrade_daemon
healthd
```

它们检测到主业务异常后，可能会直接执行：

```bash
reboot
reboot -f
echo b > /proc/sysrq-trigger
systemctl reboot
```

或者 C 代码：

```c
reboot(RB_AUTOBOOT);
system("reboot");
```

这类重启容易被误判成系统异常，其实是**应用层策略触发的重启**。

---

# 三、嵌入式 Linux 产品最常见的几个真实原因

## 1. 电源瞬断/电压跌落

这是低频重启里非常常见的一类，尤其是带这些模块的产品：

```text
Wi-Fi
4G/5G
摄像头
音频功放
电机
屏幕背光
USB 外设
SD 卡
继电器
```

比如你的设备正常跑没问题，但某一瞬间：

```text
Wi-Fi 高功率发包
摄像头开始采集
音频功放突然大音量输出
电机启动
USB 设备插拔
```

电流瞬间上升，3.3V 或 1.8V 电源跌落，SoC 直接复位。

### 怎么查？

可以看：

```bash
dmesg
cat /proc/interrupts
cat /sys/class/thermal/thermal_zone*/temp
```

但真正有效的是硬件侧：

```text
示波器看 5V / 3.3V / 1.8V / core 电压
看重启瞬间有没有跌落
看 PMIC reset reason
看 SoC reset reason 寄存器
```

很多芯片都有 reset reason 寄存器，例如：

```text
POR reset
Watchdog reset
Software reset
Brown-out reset
External reset
Thermal reset
```

如果芯片支持，一定要在 bootloader 或内核早期把它打印出来。

---

## 2. Watchdog 没喂上

嵌入式 Linux 产品一般会开硬件看门狗。

典型模式是：

```text
用户态 daemon 定时喂狗
或者 systemd watchdog 喂狗
或者内核驱动喂硬件 watchdog
```

如果系统某段时间卡住，没有喂狗，就会重启。

### 常见卡死原因

```text
CPU 被某线程占满
内核进入死锁
中断风暴
I2C/SPI 驱动等待硬件无超时
文件系统 I/O 卡死
eMMC/SD 卡异常
内存严重不足
高优先级实时线程跑飞
```

例如一个用户态进程：

```c
while (1) {
    // 忘记 sleep，疯狂跑
}
```

它可能把 CPU 占满，导致低优先级喂狗线程没机会运行。

再比如驱动里：

```c
mutex_lock(&lock);

/* 中间某个异常分支忘记 unlock */
if (error)
    return -EINVAL;

mutex_unlock(&lock);
```

下一次访问这个驱动，线程卡死，业务异常，最终被 watchdog 重启。

---

## 3. 内存泄漏导致 OOM

低频重启很容易和“长时间运行”有关：

```text
运行 1 天没事
运行 3 天偶现
运行 7 天必现
```

这就要怀疑内存泄漏。

### 用户态内存泄漏

比如应用每次请求都 malloc，但没有 free：

```c
void handle_msg(void)
{
    char *buf = malloc(1024);

    if (!buf)
        return;

    // 处理数据

    // 忘记 free(buf)
}
```

长时间运行后，内存越来越少，最后 OOM。

### 怎么看？

```bash
free -h
cat /proc/meminfo
top
ps aux --sort=-%mem
```

也可以周期性记录：

```bash
date >> /tmp/mem.log
free -m >> /tmp/mem.log
cat /proc/meminfo >> /tmp/mem.log
```

如果某个进程 RSS 一直上涨，就很可疑：

```bash
ps -o pid,ppid,comm,rss,vsz,stat -p <pid>
```

---

## 4. OOM Killer 杀了关键进程

Linux 内存不够时，不一定马上重启，而是先杀进程。

日志里可能有：

```text
Out of memory: Kill process 1234 (main_app)
Killed process 1234 (main_app)
```

如果被杀的是主业务进程，守护进程可能发现业务死了，然后重启系统。

所以你看到的是“设备重启”，但根因可能是：

```text
内存泄漏 -> OOM -> 主程序被杀 -> watchdog/daemon 触发 reboot
```

### 查法：

```bash
dmesg | grep -i oom
dmesg | grep -i "killed process"
```

或者：

```bash
logread | grep -i oom
journalctl -k | grep -i oom
```

不同系统日志命令不一样：

```text
BusyBox/OpenWrt 类：logread
systemd 系统：journalctl
普通系统：dmesg / /var/log/messages
```

---

## 5. 驱动 BUG / DMA 踩内存

嵌入式 Linux 很多低频问题都和驱动有关，尤其是：

```text
摄像头
LCD
Wi-Fi
USB
音频
I2C sensor
SPI flash
SDIO
DMA
NPU/ISP/VPU
```

### DMA 踩内存很隐蔽

比如驱动申请了一个 buffer：

```c
buf = kmalloc(1024, GFP_KERNEL);
```

但 DMA 实际写了 2048 字节，结果把旁边内存写坏。

短时间可能没事，长时间某次刚好踩到关键结构体，就出现：

```text
随机 panic
随机进程崩溃
随机重启
随机文件系统异常
```

这类 bug 很难复现，因为表现不固定。

### 常见错误

```text
DMA buffer 大小不对
cache 没有 flush/invalidate
物理地址/虚拟地址搞混
中断来了以后 buffer 已释放
多线程同时访问共享 buffer 没加锁
ring buffer 越界
```

---

## 6. 文件系统异常

嵌入式设备如果频繁写日志、写数据库、写配置，可能出现：

```text
flash/eMMC 坏块
SD 卡掉线
文件系统损坏
写满分区
日志刷爆存储
断电时正在写文件
```

表现可能是：

```text
应用卡死
I/O wait 很高
系统响应慢
watchdog 超时
重启后文件丢失
配置损坏
```

### 查法：

```bash
df -h
df -i
mount
dmesg | grep -i error
dmesg | grep -i mmc
dmesg | grep -i ext4
dmesg | grep -i ubi
dmesg | grep -i jffs2
dmesg | grep -i squashfs
```

重点看：

```text
mmc timeout
I/O error
EXT4-fs error
Buffer I/O error
UBI error
read-only filesystem
No space left on device
```

---

## 7. 日志写满导致系统异常

很多产品早期调试时会疯狂打日志：

```c
printf("xxx\n");
syslog(LOG_INFO, "xxx");
```

如果日志写到 flash 或 eMMC，可能导致：

```text
磁盘写满
flash 磨损
I/O 阻塞
系统变慢
关键服务异常
watchdog 重启
```

尤其是类似：

```bash
/var/log
/tmp/log
/data/log
/mnt/sdcard/log
```

要检查：

```bash
df -h
du -sh /var/log/*
du -sh /data/*
```

---

## 8. 温度导致重启

SoC 温度过高时，可能触发：

```text
降频
thermal throttle
thermal shutdown
PMIC 保护
系统重启
```

查温度：

```bash
cat /sys/class/thermal/thermal_zone*/temp
```

有的输出单位是毫摄氏度：

```text
65000 = 65℃
```

重点看产品在这些场景下是否重启：

```text
高温箱
长时间满负载
摄像头 + 编码 + Wi-Fi 同时工作
充电同时工作
密闭外壳
太阳直晒
```

---

## 9. EMC / ESD 干扰

低频偶现，而且和用户操作、环境有关时，要怀疑：

```text
静电
电机干扰
继电器吸合
插拔线缆
强无线发射
电源浪涌
外壳接地问题
```

典型现象：

```text
实验室不复现，客户现场复现
某些季节更容易复现
插拔 USB/网线/电源时复现
设备靠近电机/大功率设备时复现
```

这类问题软件日志通常也不明显。

---

# 四、重点：重启后要保留“死亡现场”

低频重启最大的问题是：

```text
重启之后，dmesg 清空了
内存日志没了
现场没了
只能靠猜
```

所以产品必须做“重启原因记录”。

---

## 1. 记录 reset reason

如果芯片有 reset reason 寄存器，应该在 bootloader 或内核启动早期读取。

例如打印成：

```text
Reset reason: watchdog reset
Reset reason: software reset
Reset reason: power-on reset
Reset reason: brown-out reset
Reset reason: external reset
```

这一步非常关键。

因为它能直接区分：

```text
掉电复位
看门狗复位
软件复位
外部复位
热复位
```

没有这个，你会非常痛苦。

---

## 2. 开启 pstore / ramoops

Linux 里可以用 `pstore/ramoops` 保存上一次 panic 日志。

原理是：

```text
预留一块 RAM
kernel panic 时把日志写进去
重启后从 /sys/fs/pstore 读出来
```

重启后可以看：

```bash
ls /sys/fs/pstore
cat /sys/fs/pstore/*
```

如果有 kernel panic，它可能保留类似：

```text
dmesg-ramoops-0
console-ramoops-0
```

这对低频 panic 特别有用。

---

## 3. 用户态保存心跳日志

应用层可以每隔一段时间记录状态：

```text
当前时间
主进程是否存活
内存剩余
CPU占用
温度
磁盘空间
关键线程心跳
关键状态机状态
```

例如保存到：

```text
/data/health.log
/data/reboot_reason.log
/data/last_state.log
```

简单脚本示例：

```bash
#!/bin/sh

while true
do
    echo "===== $(date) =====" >> /data/health.log
    uptime >> /data/health.log
    free -m >> /data/health.log
    df -h >> /data/health.log
    top -b -n 1 | head -20 >> /data/health.log
    cat /sys/class/thermal/thermal_zone*/temp >> /data/health.log 2>/dev/null
    sleep 60
done
```

注意：不要无限写，要做日志轮转，不然会把 flash 写爆。

---

# 五、推荐排查流程

我建议按这个顺序查，不要一开始就乱改代码。

---

## 第 1 步：确认重启方式

重启后立刻看：

```bash
uptime
last reboot
dmesg | tail -100
journalctl -b -1
```

如果是 systemd 系统：

```bash
journalctl -b -1
journalctl -k -b -1
```

含义：

```text
journalctl -b -1     查看上一次启动周期的日志
journalctl -k -b -1  查看上一次启动周期的内核日志
```

如果是 BusyBox 系统，可能没有 journalctl，就看：

```bash
dmesg
logread
cat /var/log/messages
```

---

## 第 2 步：查有没有 panic/oops

```bash
dmesg | grep -i panic
dmesg | grep -i oops
dmesg | grep -i "null pointer"
dmesg | grep -i "call trace"
dmesg | grep -i "unable to handle"
```

如果有这些，基本就是内核或驱动问题。

---

## 第 3 步：查 watchdog

```bash
dmesg | grep -i watchdog
dmesg | grep -i wdt
dmesg | grep -i reset
```

如果 reset reason 显示 watchdog，那就重点查：

```text
谁负责喂狗？
喂狗周期是多少？
是否有喂狗日志？
喂狗线程优先级是否太低？
系统是否发生过长时间阻塞？
```

---

## 第 4 步：查 OOM

```bash
dmesg | grep -i oom
dmesg | grep -i "killed process"
journalctl -k | grep -i oom
```

如果有 OOM，要查：

```text
哪个进程占内存最高
是否内存持续上涨
是否有缓存没有释放
是否有大文件读入内存
是否有线程不断创建
```

---

## 第 5 步：查存储问题

```bash
df -h
df -i
dmesg | grep -i mmc
dmesg | grep -i ext4
dmesg | grep -i ubi
dmesg | grep -i "I/O error"
```

重点关注：

```text
分区是否写满
inode 是否耗尽
是否有 I/O error
是否变成只读文件系统
日志是否过大
```

---

## 第 6 步：查温度

```bash
cat /sys/class/thermal/thermal_zone*/temp
```

也可以持续记录：

```bash
watch -n 1 'cat /sys/class/thermal/thermal_zone*/temp'
```

如果压力测试时温度持续升高，就要怀疑热保护。

---

## 第 7 步：查应用层是否主动 reboot

全局搜索：

```bash
grep -R "reboot" -n /etc /usr /opt /data 2>/dev/null
grep -R "sysrq-trigger" -n /etc /usr /opt /data 2>/dev/null
grep -R "watchdog" -n /etc /usr /opt /data 2>/dev/null
```

代码里搜：

```bash
grep -R "reboot(" -n .
grep -R "system(\"reboot" -n .
grep -R "RB_AUTOBOOT" -n .
grep -R "sysrq-trigger" -n .
```

很多产品里会有：

```text
主应用异常 -> daemon 重启系统
升级失败 -> reboot
网络异常多次 -> reboot
内存异常 -> reboot
喂狗失败 -> 硬件复位
```

---

# 六、低频重启最推荐加的调试机制

产品阶段强烈建议加这些东西。

## 1. 启动时记录本次启动原因

启动后写一个文件：

```text
/data/boot_info.log
```

内容类似：

```text
boot_count=1024
last_reset_reason=watchdog
kernel_version=xxx
app_version=xxx
uptime_before_reset=36000
last_app_state=wifi_streaming
last_temperature=72C
last_free_mem=38MB
```

---

## 2. 应用定期写心跳

例如主应用每 10 秒更新：

```text
/data/app_heartbeat
```

内容：

```text
timestamp=2026-05-22 16:20:00
main_loop_alive=1
wifi_state=connected
camera_state=streaming
free_mem=80MB
thread_audio=alive
thread_video=alive
thread_net=alive
```

重启后看最后一次心跳时间，就知道：

```text
应用先死了？
系统直接掉电了？
卡死多久后才重启？
```

---

## 3. 喂狗前后打点

不要只喂狗，要记录：

```text
准备喂狗
喂狗成功
距离上次喂狗间隔
当前系统状态
```

例如：

```c
LOG("feed watchdog, tick=%ld, state=%d", tick, app_state);
```

如果重启前最后一条日志是：

```text
feed watchdog, tick=10000, state=CAMERA_START
```

那就可以重点怀疑 `CAMERA_START` 状态附近。

---

## 4. 关键线程心跳

不要只看进程活着，进程活着不代表线程正常。

比如：

```text
main thread alive
network thread alive
camera thread alive
audio thread alive
storage thread alive
```

每个线程定期更新时间戳。

监控线程检查：

```c
if (now - camera_thread_heartbeat > 30) {
    LOG_ERROR("camera thread stuck");
}
```

这样可以知道到底哪个线程卡死。

---

# 七、低频重启和 MCU 思维的区别

在 MCU 里你经常看：

```text
HardFault
看门狗
栈溢出
中断卡死
电源复位
```

Linux 里类似，但层次更多：

```text
Bootloader
Kernel
Driver
Device Tree
Rootfs
Systemd/Init
Daemon
Application
Watchdog
PMIC
Filesystem
```

所以嵌入式 Linux 重启问题要分层看：

```text
硬件层：电源、晶振、复位脚、PMIC、温度、EMC
Boot层：uboot、启动参数、reset reason
内核层：panic、oops、driver、watchdog、OOM
文件系统层：分区满、I/O error、只读
用户层：daemon、主应用、脚本、OTA
业务层：摄像头、网络、音频、AI、存储等状态机
```

---

# 八、实际排查优先级

如果你现在手上有一台偶现重启的嵌入式 Linux 设备，我建议优先级这样排：

## 第一优先级

```text
1. 查 reset reason
2. 查 /sys/fs/pstore 有没有 panic 日志
3. 查上一次启动周期日志
4. 查 watchdog 记录
5. 查 OOM 记录
```

## 第二优先级

```text
6. 查电源稳定性
7. 查温度
8. 查存储是否写满或 I/O error
9. 查应用是否主动 reboot
10. 查关键外设驱动日志
```

## 第三优先级

```text
11. 做压力测试
12. 长稳测试
13. 高低温测试
14. ESD/插拔测试
15. 外设组合场景测试
```

---

# 九、几个典型案例

## 案例 1：Wi-Fi 发包导致偶现重启

现象：

```text
设备平时正常
视频上传时偶现重启
日志没有 panic
```

可能原因：

```text
Wi-Fi 发射瞬间电流过大
3.3V 电源跌落
SoC brown-out reset
```

排查：

```text
示波器看 3.3V
读取 reset reason
降低 Wi-Fi 发射功率验证
单独压力测试 Wi-Fi
```

---

## 案例 2：运行三天后重启

现象：

```text
设备运行 2~3 天后重启
重启前系统越来越卡
```

可能原因：

```text
内存泄漏
文件句柄泄漏
日志写满
OOM killer
```

排查：

```bash
free -m
top
ps aux --sort=-%mem
ls /proc/<pid>/fd | wc -l
df -h
dmesg | grep -i oom
```

---

## 案例 3：摄像头打开时低概率重启

可能原因：

```text
camera driver 空指针
MIPI/ISP 驱动异常
DMA buffer 越界
电源瞬间负载变大
```

排查：

```text
开 pstore
加 camera 状态机日志
压力循环 open/close camera
示波器看 camera AVDD/DVDD/IOVDD
打开 kernel debug 日志
```

---

## 案例 4：客户现场偶现，实验室不复现

可能原因：

```text
ESD
电源环境差
温度高
外部设备干扰
网络异常触发 bug
客户操作路径特殊
```

排查：

```text
加远程日志
加重启原因记录
加现场状态快照
做 ESD/浪涌/高温/弱网测试
```

---

# 十、你可以记住一句话

嵌入式 Linux 低频重启不要先猜代码，应该先建立证据链：

```text
reset reason
+ 上一次内核日志
+ pstore/ramoops
+ watchdog记录
+ OOM记录
+ 应用心跳
+ 电源波形
```

有了这些，才能判断是：

```text
电源问题
看门狗问题
内核 panic
用户态主动重启
内存泄漏
驱动踩内存
温度/EMC
```

没有这些，只能靠猜，排查会非常痛苦。

---

# 十一、最小排查命令清单

你可以先让设备重启后执行这些：

```bash
uptime
last reboot

dmesg | grep -i panic
dmesg | grep -i oops
dmesg | grep -i watchdog
dmesg | grep -i wdt
dmesg | grep -i reset
dmesg | grep -i oom
dmesg | grep -i "killed process"
dmesg | grep -i "I/O error"
dmesg | grep -i mmc
dmesg | grep -i ext4

free -h
df -h
df -i
cat /proc/meminfo
cat /sys/class/thermal/thermal_zone*/temp 2>/dev/null

ls /sys/fs/pstore 2>/dev/null
cat /sys/fs/pstore/* 2>/dev/null
```

如果是 systemd 系统，再加：

```bash
journalctl -b -1
journalctl -k -b -1
journalctl -b -1 | grep -i reboot
journalctl -b -1 | grep -i watchdog
journalctl -b -1 | grep -i oom
journalctl -b -1 | grep -i panic
```

如果你能把重启后的 `dmesg`、`journalctl -b -1`、`/sys/fs/pstore` 内容贴出来，基本就能进一步判断是哪一类。

-------
------
------
可以，但要分清楚：**Linux 本身没有一个统一的“复位原因寄存器”接口**，真正的复位原因一般来自 **SoC/PMU/RTC/Watchdog/BootROM/Bootloader** 这些硬件或早期启动阶段。Linux 只能通过驱动、设备树、bootloader 传参、pstore 日志等方式把这些信息暴露出来。

对 **君正 X2600** 来说，我能确认公开硬件设计资料里明确提到：X2600 支持 **软件复位**，其中软件复位是通过 **WDT timeout** 触发；也支持通过芯片 **PPRST_ 引脚**进行硬件复位；另外资料还建议某些产品使用外部硬件看门狗芯片拉低 PPRST_ 做全局复位。也就是说，X2600 至少有 watchdog/reset 相关机制，但公开资料里我没有直接查到完整的“reset reason register 表”。([Lumissil][1])

---

# 1. MCU 和 Linux 的区别

你说的 GD32 MCU 这种，一般是：

```c
if (RESET != rcu_flag_get(RCU_FLAG_WDTRST)) {
    // 看门狗复位
}

if (RESET != rcu_flag_get(RCU_FLAG_PORRST)) {
    // 上电复位
}
```

也就是 MCU 的 RCC/RCU/PMU 模块里有专门的复位标志位。

但是嵌入式 Linux 里通常不是这样直接给应用层用的。Linux 是多层结构：

```text
BootROM
  ↓
SPL / Bootloader / U-Boot
  ↓
Linux Kernel
  ↓
Rootfs / Init / Application
```

复位原因最可靠的读取时机，通常是在：

```text
Bootloader 阶段
或者 Linux kernel very early 阶段
```

因为有些复位状态寄存器可能在后续初始化时被清掉。

---

# 2. X2600 大概率应该从哪里看？

对 X2600 这种 SoC，你要重点找这几个模块：

```text
PMU
RTC
WDT
Reset controller
Clock/reset unit
BootROM/SPL/U-Boot early init
```

关键词可以在 SDK 里搜：

```bash
grep -R "reset reason" -n .
grep -R "reboot reason" -n .
grep -R "rst" -n bootloader kernel module_driver libmcu 2>/dev/null
grep -R "wdt" -n bootloader kernel module_driver libmcu 2>/dev/null
grep -R "watchdog" -n bootloader kernel module_driver libmcu 2>/dev/null
grep -R "pmu" -n bootloader kernel module_driver libmcu 2>/dev/null
grep -R "rtc" -n bootloader kernel module_driver libmcu 2>/dev/null
```

你这个 X2600 SDK 目录之前大概是：

```text
x2600/
├── bootloader
├── buildroot
├── kernel
├── libmcu
├── module_driver
├── tools
└── ...
```

所以可以优先搜：

```bash
cd ~/project/x2600

grep -R "reset reason" -n bootloader kernel libmcu module_driver 2>/dev/null
grep -R "reboot reason" -n bootloader kernel libmcu module_driver 2>/dev/null
grep -R "watchdog" -n bootloader kernel libmcu module_driver 2>/dev/null
grep -R "wdt" -n bootloader kernel libmcu module_driver 2>/dev/null
grep -R "PPRST" -n . 2>/dev/null
grep -R "POR_RST" -n . 2>/dev/null
```

如果君正 SDK 已经做了这套功能，你可能会看到类似：

```text
reset_reason
reboot_reason
boot_reason
rst_reason
wdt_reset
poweron_reset
software_reset
```

---

# 3. Linux 下有哪些“通用手段”？

## 方法一：看上一次启动日志

如果系统是 systemd：

```bash
journalctl -b -1
journalctl -k -b -1
```

含义是：

```text
journalctl -b -1     查看上一次启动周期日志
journalctl -k -b -1  查看上一次启动周期内核日志
```

如果是 BusyBox/init 系统，可能没有 journalctl，可以看：

```bash
dmesg
logread
cat /var/log/messages
```

不过注意：
如果是突然掉电复位，普通日志经常来不及落盘。

---

## 方法二：看 watchdog 信息

Linux 里可以查 watchdog 设备：

```bash
ls -l /dev/watchdog*
```

看内核日志：

```bash
dmesg | grep -i watchdog
dmesg | grep -i wdt
```

看内核配置：

```bash
zcat /proc/config.gz | grep -i watchdog
```

如果没有 `/proc/config.gz`，可以试：

```bash
cat /boot/config-$(uname -r) | grep -i watchdog
```

或者在内核源码里看 `.config`：

```bash
grep -i WATCHDOG kernel/.config
```

Linux watchdog 一般需要有用户态或 systemd 喂狗。很多平台虽然加载了 watchdog 驱动，但 watchdog 不一定自动工作，通常需要打开 `/dev/watchdog` 并周期性写入；否则不会真的进入监控状态。([digi.com][2])

---

## 方法三：看 pstore / ramoops

这个是排查 Linux panic 特别重要的。

```bash
ls /sys/fs/pstore
cat /sys/fs/pstore/* 2>/dev/null
```

如果有：

```text
console-ramoops-0
dmesg-ramoops-0
```

说明上一次可能有 kernel panic / oops / watchdog 前的内核日志被保存下来了。

如果目录不存在，说明可能没开：

```bash
mount | grep pstore
zcat /proc/config.gz | grep -E "PSTORE|RAMOOPS"
```

常见内核配置：

```text
CONFIG_PSTORE=y
CONFIG_PSTORE_RAM=y
CONFIG_PSTORE_CONSOLE=y
CONFIG_PSTORE_PMSG=y
CONFIG_RAMOOPS=y
```

这个不是“复位原因寄存器”，但它能回答：

```text
是不是 kernel panic？
是不是 oops？
是不是死在某个驱动？
是不是 watchdog 前内核已经卡死？
```

---

## 方法四：看 U-Boot / kernel cmdline 有没有传 boot reason

有些平台会在 U-Boot 里读 reset reason，然后通过 bootargs 传给 Linux：

```bash
cat /proc/cmdline
```

你可以看看里面有没有类似：

```text
boot_reason=
reset_reason=
reboot_reason=
androidboot.bootreason=
```

如果有，例如：

```text
boot_reason=watchdog
```

那应用层就可以直接读 `/proc/cmdline`。

---

# 4. 真正可靠的方案：在 U-Boot 里读 X2600 reset reason

如果 X2600 手册里有类似寄存器，比如：

```text
PMU_RST_STATUS
RTC_RESET_STATUS
WDT_RESET_STATUS
RESET_STATUS
RSTSR
```

最推荐的做法是：

```text
U-Boot 早期读取 reset reason
↓
打印出来
↓
保存到环境变量
↓
通过 bootargs 传给 Linux
↓
Linux 用户态读取 /proc/cmdline 或 /proc/device-tree/chosen
```

逻辑类似这样：

```c
uint32_t reason;

reason = readl(X2600_RESET_STATUS_REG);

if (reason & WDT_RESET_FLAG)
    boot_reason = "watchdog";
else if (reason & POR_RESET_FLAG)
    boot_reason = "power_on";
else if (reason & SOFTWARE_RESET_FLAG)
    boot_reason = "software";
else if (reason & EXTERNAL_RESET_FLAG)
    boot_reason = "external";
else
    boot_reason = "unknown";

printf("Reset reason: %s, raw=0x%08x\n", boot_reason, reason);

env_set("boot_reason", boot_reason);
```

然后拼到 bootargs：

```bash
setenv bootargs "${bootargs} boot_reason=${boot_reason}"
```

Linux 起来后：

```bash
cat /proc/cmdline
```

就能看到：

```text
boot_reason=watchdog
```

---

# 5. 如果没有 reset reason 寄存器怎么办？

那就只能自己做“辅助判断”。

## 方案一：用 RTC backup / 保留寄存器

有些 SoC 的 RTC 域在普通 reset 后不掉电，可以保存少量信息。

比如：

```text
正常启动时写 magic
正常关机前写 shutdown flag
喂狗前写状态
应用运行时写 heartbeat
```

重启后读这些值，推断上次发生了什么。

例如：

```text
last_state = CAMERA_RUNNING
last_feed_watchdog_time = xxx
last_shutdown_flag = 0
```

如果重启后发现：

```text
没有正常关机标志
最后状态是 CAMERA_RUNNING
最后心跳停在 30 秒前
```

就说明可能是卡死/看门狗/掉电。

---

## 方案二：用文件记录，但可靠性差一些

比如写到：

```text
/data/boot_reason.log
/data/heartbeat.log
/data/last_state
```

缺点是：

```text
突然掉电时文件可能没刷盘
频繁写 flash 会磨损
文件系统异常时可能丢
```

所以最好结合：

```text
RTC backup / pstore / U-Boot early log / 外部 MCU
```

---

## 方案三：外部硬件 watchdog / PMIC 记录

X2600 硬件设计资料里明确提到，某些产品为了过 ESD 或需要 watchdog 芯片，可以用外部硬件看门狗芯片输出 reset 信号到 X2600 的 PPRST_ 引脚。([Lumissil][1])

这种方案的好处是：

```text
Linux 完全死机也能拉复位
内核卡死也能拉复位
SoC 内部 watchdog 失效时也能拉复位
某些外部 watchdog/PMIC 还可以提供 reset cause
```

如果产品可靠性要求高，这个比单靠 Linux watchdog 更稳。

---

# 6. 你现在可以怎么查？

你可以先在板子上执行：

```bash
cat /proc/cmdline
dmesg | grep -i reset
dmesg | grep -i reboot
dmesg | grep -i watchdog
dmesg | grep -i wdt
ls /sys/fs/pstore
cat /sys/fs/pstore/* 2>/dev/null
ls -l /dev/watchdog*
```

然后在 SDK 里搜：

```bash
cd ~/project/x2600

grep -R "reset_reason" -n . 2>/dev/null
grep -R "reboot_reason" -n . 2>/dev/null
grep -R "boot_reason" -n . 2>/dev/null
grep -R "watchdog" -n bootloader kernel libmcu module_driver 2>/dev/null
grep -R "wdt" -n bootloader kernel libmcu module_driver 2>/dev/null
grep -R "PPRST" -n . 2>/dev/null
grep -R "POR_RST" -n . 2>/dev/null
```

如果搜到类似 `reset status`、`rtc status`、`pmu status` 的寄存器定义，就基本有戏。

---

# 7. 我的判断

**X2600 硬件层面大概率有一些 reset/watchdog/power 状态相关寄存器，但是否能像 GD32 那样直接读到“上电复位、外部复位、看门狗复位、软件复位”这些标志，要看君正 X2600 的 datasheet / PMU / RTC / WDT 手册。**

你不能只指望 Linux 通用命令直接告诉你：

```text
上次是 WDT reset
上次是 POR reset
上次是 brown-out reset
```

更靠谱的产品级做法是：

```text
U-Boot 早期读 SoC reset status
+
Linux 开 pstore/ramoops
+
应用层心跳日志
+
watchdog 喂狗打点
+
必要时外部 watchdog/PMIC 记录
```

这样才能真正定位低频重启。

最关键一句：

```text
Linux 可以辅助判断复位原因，但真正的复位原因通常要从 SoC/PMU/RTC/WDT 的硬件寄存器或 bootloader 早期读取。
```

[1]: https://www.lumissil.com/assets/pdf/core/X2600%20Hardware%20Design%20Guide.pdf?utm_source=chatgpt.com "Ingenic® X2600"
[2]: https://www.digi.com/Support/knowledge-base/why-does-the-linux-watchdog-driver-not-reset-my-sy?utm_source=chatgpt.com "Why does the Linux watchdog driver not reset my system when I freeze it e.g. with a fork bomb? | Digi International"
