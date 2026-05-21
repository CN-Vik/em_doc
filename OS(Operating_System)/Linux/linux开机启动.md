# linux如何开机启动程序和脚本
可以的。Linux 里“开机启动应用/脚本”一般不是像 MCU 那样在 `main()` 里直接调用，而是由 **init 系统** 负责。

你可以先记住一句话：

> Linux 内核启动完以后，会启动第一个用户态进程 `/sbin/init`，后面的开机脚本、应用程序、服务，基本都由 init 系统拉起来。

---

# 1. Linux 开机启动大概流程

嵌入式 Linux 启动流程可以理解成：

```text
上电
  ↓
Bootloader，例如 U-Boot
  ↓
加载 Linux Kernel
  ↓
Kernel 初始化驱动、挂载 rootfs
  ↓
执行 /sbin/init
  ↓
init 执行开机脚本
  ↓
启动你的应用程序
```

关键点是：

```text
/sbin/init
```

它是 Linux 系统启动后的 **第一个用户态进程**，PID 通常是 1。

可以在板子上看：

```bash
ps
```

可能看到：

```text
PID   USER     COMMAND
1     root     init
```

---

# 2. 嵌入式 Linux 常见 init 类型

不同 Linux 系统，开机启动方式不一样。

常见有这几种：

| init 类型      | 常见场景                       |
| ------------ | -------------------------- |
| BusyBox init | Buildroot、很多嵌入式 Linux      |
| SysV init    | 老式 Linux、部分嵌入式系统           |
| systemd      | Ubuntu、Debian、新一点的复杂 Linux |
| rc.local     | 简单脚本启动方式，有些系统支持            |
| 自定义 init     | 极简嵌入式系统                    |

你现在这种 **X2600 + Buildroot SDK**，大概率是：

```text
BusyBox init
```

所以最常用的方式就是：

```text
/etc/init.d/Sxx脚本
```

---

# 3. 先判断你的系统用的是什么 init

板子启动后执行：

```bash
ps -p 1
```

或者：

```bash
ls -l /sbin/init
```

如果看到类似：

```text
/sbin/init -> ../bin/busybox
```

说明是 BusyBox init。

也可以看：

```bash
cat /etc/inittab
```

如果里面有：

```text
::sysinit:/etc/init.d/rcS
```

那基本就是 BusyBox init。

---

# 4. BusyBox init 的开机启动流程

Buildroot 生成的嵌入式系统常见流程是：

```text
/sbin/init
  ↓
读取 /etc/inittab
  ↓
执行 /etc/init.d/rcS
  ↓
rcS 按顺序执行 /etc/init.d/Sxx 脚本
  ↓
启动系统服务和你的应用
```

你可以查看：

```bash
cat /etc/inittab
```

可能看到：

```text
::sysinit:/etc/init.d/rcS
::respawn:/sbin/getty -L ttyS0 115200 vt100
::shutdown:/bin/umount -a -r
```

其中这句很重要：

```text
::sysinit:/etc/init.d/rcS
```

意思是：

> 系统初始化时执行 `/etc/init.d/rcS`。

然后你再看：

```bash
cat /etc/init.d/rcS
```

里面通常会遍历执行：

```bash
/etc/init.d/S*
```

也就是所有以 `S` 开头的脚本。

---

# 5. `/etc/init.d/Sxx` 是什么？

比如：

```text
/etc/init.d/S10udev
/etc/init.d/S20network
/etc/init.d/S40mount
/etc/init.d/S99myapp
```

这些脚本会按照名字顺序执行。

数字越小，越早执行：

```text
S10 比 S20 早
S20 比 S40 早
S99 最晚
```

所以你的应用一般放在比较靠后的位置，例如：

```text
/etc/init.d/S99myapp
```

意思是：

> 等系统基础服务、设备节点、网络、驱动都准备得差不多了，再启动我的应用。

---

# 6. 最简单：写一个开机启动脚本

假设你的应用是：

```bash
/usr/bin/my_app
```

你可以创建脚本：

```bash
/etc/init.d/S99myapp
```

内容如下：

```sh
#!/bin/sh

case "$1" in
    start)
        echo "Starting my_app..."
        /usr/bin/my_app &
        ;;
    stop)
        echo "Stopping my_app..."
        killall my_app
        ;;
    restart)
        echo "Restarting my_app..."
        killall my_app
        sleep 1
        /usr/bin/my_app &
        ;;
    *)
        echo "Usage: $0 {start|stop|restart}"
        exit 1
        ;;
esac

exit 0
```

然后加执行权限：

```bash
chmod +x /etc/init.d/S99myapp
```

重启后，系统就会自动执行：

```bash
/etc/init.d/S99myapp start
```

---

# 7. 为什么应用后面要加 `&`？

比如：

```sh
/usr/bin/my_app &
```

这个 `&` 表示后台运行。

如果不加 `&`：

```sh
/usr/bin/my_app
```

脚本会一直卡在这里，后面的启动流程可能不会继续。

所以对于长期运行的应用，通常要加：

```sh
&
```

例如：

```sh
/usr/bin/camera_app &
/usr/bin/voice_app &
/usr/bin/network_app &
```

---

# 8. 如果只是执行一次脚本怎么办？

比如你开机后只想配置 GPIO、创建目录、设置参数，不是长期运行程序。

可以这样写：

```sh
#!/bin/sh

case "$1" in
    start)
        echo "Do boot init..."

        mkdir -p /tmp/myapp
        echo 1 > /sys/class/gpio/gpio23/value
        echo performance > /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor

        ;;
    stop)
        ;;
esac

exit 0
```

这种脚本不需要加 `&`，因为它执行完就退出。

---

# 9. 在 Buildroot 里应该怎么添加这个脚本？

不要直接长期手动改板子的 `/etc/init.d/`，因为重新制作 rootfs 后会丢。

正确方式是放到 Buildroot 的 rootfs overlay 里。

比如你创建：

```bash
board/ingenic/x2600/rootfs_overlay/etc/init.d/S99myapp
```

目录结构：

```text
board/ingenic/x2600/rootfs_overlay/
└── etc
    └── init.d
        └── S99myapp
```

然后给权限：

```bash
chmod +x board/ingenic/x2600/rootfs_overlay/etc/init.d/S99myapp
```

Buildroot 配置里设置 overlay：

```text
System configuration  --->
    Root filesystem overlay directories
```

填入：

```text
board/ingenic/x2600/rootfs_overlay
```

重新编译 rootfs 后，最终板子上就会有：

```bash
/etc/init.d/S99myapp
```

---

# 10. 如果你的应用也要放进 rootfs

假设你有应用：

```bash
my_app
```

你可以放到 overlay：

```bash
mkdir -p board/ingenic/x2600/rootfs_overlay/usr/bin
cp my_app board/ingenic/x2600/rootfs_overlay/usr/bin/
chmod +x board/ingenic/x2600/rootfs_overlay/usr/bin/my_app
```

最终板子上就是：

```bash
/usr/bin/my_app
```

然后启动脚本里写：

```sh
/usr/bin/my_app &
```

完整 overlay 结构：

```text
rootfs_overlay/
├── usr
│   └── bin
│       └── my_app
└── etc
    └── init.d
        └── S99myapp
```

---

# 11. 完整例子：开机启动自己的应用

假设你要开机启动摄像头程序：

```bash
/usr/bin/camera_app
```

创建脚本：

```bash
board/ingenic/x2600/rootfs_overlay/etc/init.d/S99camera
```

内容：

```sh
#!/bin/sh

APP=/usr/bin/camera_app
LOG=/tmp/camera_app.log

case "$1" in
    start)
        echo "Starting camera_app..."

        if [ ! -x "$APP" ]; then
            echo "$APP not found or not executable"
            exit 1
        fi

        $APP > $LOG 2>&1 &
        echo $! > /var/run/camera_app.pid

        echo "camera_app started, pid=$(cat /var/run/camera_app.pid)"
        ;;
    stop)
        echo "Stopping camera_app..."

        if [ -f /var/run/camera_app.pid ]; then
            kill "$(cat /var/run/camera_app.pid)"
            rm -f /var/run/camera_app.pid
        else
            killall camera_app 2>/dev/null
        fi
        ;;
    restart)
        $0 stop
        sleep 1
        $0 start
        ;;
    *)
        echo "Usage: $0 {start|stop|restart}"
        exit 1
        ;;
esac

exit 0
```

这个脚本做了几件事：

```text
1. 检查应用是否存在
2. 后台启动应用
3. 把日志重定向到 /tmp/camera_app.log
4. 保存进程 PID
5. 支持 stop/restart
```

启动后你可以查看：

```bash
cat /tmp/camera_app.log
ps | grep camera_app
cat /var/run/camera_app.pid
```

---

# 12. 如果应用崩溃了，想自动重启怎么办？

`S99myapp` 这种方式只能开机启动一次。

如果程序崩溃退出，它不会自动重启。

如果你想让它崩溃后自动拉起，可以用 BusyBox init 的 `respawn`。

编辑：

```bash
/etc/inittab
```

添加类似：

```text
::respawn:/usr/bin/my_app
```

意思是：

> 如果 `/usr/bin/my_app` 退出了，init 会自动重新启动它。

但是这种方式要注意：

```text
1. my_app 不要自己后台化
2. 不要加 &
3. 程序崩溃太快会反复重启刷屏
4. 日志要处理好
```

例如：

```text
::respawn:/usr/bin/camera_app
```

如果想带日志：

```text
::respawn:/bin/sh -c "/usr/bin/camera_app >> /tmp/camera_app.log 2>&1"
```

但是产品里更推荐做一个专门的守护进程或者看门狗机制。

---

# 13. `S99脚本` 和 `inittab respawn` 的区别

| 方式                     | 特点        | 适合                 |
| ---------------------- | --------- | ------------------ |
| `/etc/init.d/S99xxx`   | 开机执行一次    | 初始化脚本、启动普通应用       |
| `/etc/inittab respawn` | 程序退出后自动重启 | 核心常驻应用             |
| watchdog               | 系统异常时复位   | 产品可靠性              |
| systemd service        | 功能强大      | Ubuntu/Debian/复杂系统 |

如果只是普通应用开机启动：

```text
用 /etc/init.d/S99myapp
```

如果是产品主业务，必须一直活着：

```text
可以考虑 inittab respawn 或 watchdog
```

---

# 14. 如果系统是 systemd 怎么办？

如果你的系统不是 Buildroot/BusyBox，而是 systemd，比如 Ubuntu、Debian、Yocto 某些系统，就用 `.service`。

创建：

```bash
/etc/systemd/system/myapp.service
```

内容：

```ini
[Unit]
Description=My Application
After=network.target

[Service]
ExecStart=/usr/bin/my_app
Restart=always
RestartSec=2

[Install]
WantedBy=multi-user.target
```

启用：

```bash
systemctl enable myapp.service
```

启动：

```bash
systemctl start myapp.service
```

查看状态：

```bash
systemctl status myapp.service
```

但是你现在 X2600 Buildroot 系统，大概率不是 systemd，所以重点还是：

```text
/etc/init.d/Sxx
/etc/inittab
```

---

# 15. 如果系统支持 rc.local 怎么办？

有些系统支持：

```bash
/etc/rc.local
```

可以在里面写：

```sh
#!/bin/sh

/usr/bin/my_app &

exit 0
```

但嵌入式 Buildroot 不一定默认支持 `rc.local`。

所以我不建议你优先用它。

更标准的是：

```text
/etc/init.d/S99myapp
```

---

# 16. 多个应用启动顺序怎么控制？

靠脚本名字的数字。

例如：

```text
S10mount
S20network
S30driver_init
S40media_server
S90ai_service
S99main_app
```

执行顺序就是：

```text
S10 → S20 → S30 → S40 → S90 → S99
```

比如你的主程序依赖网络和摄像头：

```text
网络初始化脚本：S40network
摄像头服务脚本：S80camera
主应用脚本：S99main_app
```

主程序就放晚一点：

```text
S99main_app
```

---

# 17. 脚本里要不要 sleep？

有时候驱动节点或网络还没准备好，应用启动太早会失败。

比如你的程序需要：

```text
/dev/video0
/dev/i2c-0
网络 IP
音频设备 /dev/snd/*
```

可以在脚本里等待。

例如等 `/dev/video0`：

```sh
#!/bin/sh

case "$1" in
    start)
        echo "Waiting for /dev/video0..."

        COUNT=0
        while [ ! -e /dev/video0 ]; do
            sleep 1
            COUNT=$((COUNT + 1))

            if [ "$COUNT" -ge 10 ]; then
                echo "/dev/video0 not found"
                exit 1
            fi
        done

        echo "/dev/video0 ready"
        /usr/bin/camera_app &
        ;;
esac

exit 0
```

比直接写死：

```sh
sleep 5
```

更可靠。

---

# 18. 开机启动脚本常见问题

## 问题 1：脚本没有执行权限

错误写法：

```bash
-rw-r--r-- S99myapp
```

正确权限：

```bash
chmod +x /etc/init.d/S99myapp
```

查看：

```bash
ls -l /etc/init.d/S99myapp
```

应该类似：

```text
-rwxr-xr-x
```

---

## 问题 2：脚本格式是 Windows CRLF

如果你在 Windows 上编辑脚本，可能有 `\r\n` 换行，板子上执行时报：

```text
not found
bad interpreter
```

解决：

```bash
dos2unix S99myapp
```

或者：

```bash
sed -i 's/\r$//' S99myapp
```

---

## 问题 3：路径写错

脚本里不要依赖当前目录。

不要这样：

```sh
./my_app &
```

建议写绝对路径：

```sh
/usr/bin/my_app &
```

---

## 问题 4：动态库找不到

开机启动时运行失败，可能是库找不到：

```text
error while loading shared libraries: libxxx.so
```

检查：

```bash
readelf -d /usr/bin/my_app | grep NEEDED
```

确认库在：

```text
/lib
/usr/lib
```

如果库在特殊路径，可以临时：

```sh
export LD_LIBRARY_PATH=/usr/lib:/lib:/opt/lib
/usr/bin/my_app &
```

但更推荐把库放到标准路径。

---

## 问题 5：程序需要环境变量

开机脚本环境变量很少，不像你手动登录 shell 那样完整。

所以脚本里最好显式写：

```sh
export PATH=/bin:/sbin:/usr/bin:/usr/sbin
export LD_LIBRARY_PATH=/lib:/usr/lib
```

例如：

```sh
#!/bin/sh

export PATH=/bin:/sbin:/usr/bin:/usr/sbin
export LD_LIBRARY_PATH=/lib:/usr/lib

case "$1" in
    start)
        /usr/bin/my_app &
        ;;
esac

exit 0
```

---

## 问题 6：应用启动太早，设备节点还没出现

比如你的程序需要：

```text
/dev/video0
/dev/iio:device0
/dev/input/event0
/dev/snd/pcmC0D0c
```

解决方式：

```text
1. 调整 Sxx 顺序，让它晚启动
2. 脚本里等待设备节点出现
3. 确认驱动 probe 成功
4. 确认 devtmpfs/mdev/udev 正常工作
```

---

# 19. 怎么确认脚本有没有执行？

在脚本里加打印：

```sh
echo "S99myapp start" > /tmp/S99myapp.log
```

或者：

```sh
echo "S99myapp start at $(date)" >> /tmp/S99myapp.log
```

然后开机后查看：

```bash
cat /tmp/S99myapp.log
```

也可以看进程：

```bash
ps | grep my_app
```

看日志：

```bash
dmesg
cat /tmp/my_app.log
```

---

# 20. Buildroot 里更正规的 package 写法

如果你把自己的应用做成 Buildroot package，可以在 `.mk` 里面安装启动脚本。

例如：

```makefile
define MY_APP_INSTALL_INIT_SYSV
	$(INSTALL) -D -m 0755 package/my_app/S99myapp \
		$(TARGET_DIR)/etc/init.d/S99myapp
endef
```

这个是比较正规的方式。

意思是：

> 当 Buildroot 编译安装 `my_app` 包时，顺便把 `S99myapp` 安装到 rootfs 的 `/etc/init.d/` 目录。

新手阶段可以先用：

```text
rootfs_overlay
```

熟练以后再用：

```text
Buildroot package
```

---

# 21. 你现在应该怎么做？

对你现在这个阶段，推荐先用最简单可靠的方式：

```text
rootfs_overlay + /etc/init.d/S99xxx
```

流程如下：

```bash
cd ~/project/x2600
mkdir -p board/ingenic/x2600/rootfs_overlay/etc/init.d
mkdir -p board/ingenic/x2600/rootfs_overlay/usr/bin
```

把应用放进去：

```bash
cp my_app board/ingenic/x2600/rootfs_overlay/usr/bin/
chmod +x board/ingenic/x2600/rootfs_overlay/usr/bin/my_app
```

创建启动脚本：

```bash
vi board/ingenic/x2600/rootfs_overlay/etc/init.d/S99myapp
```

写入：

```sh
#!/bin/sh

export PATH=/bin:/sbin:/usr/bin:/usr/sbin
export LD_LIBRARY_PATH=/lib:/usr/lib

case "$1" in
    start)
        echo "Starting my_app..."
        /usr/bin/my_app > /tmp/my_app.log 2>&1 &
        ;;
    stop)
        echo "Stopping my_app..."
        killall my_app 2>/dev/null
        ;;
    restart)
        $0 stop
        sleep 1
        $0 start
        ;;
    *)
        echo "Usage: $0 {start|stop|restart}"
        exit 1
        ;;
esac

exit 0
```

加权限：

```bash
chmod +x board/ingenic/x2600/rootfs_overlay/etc/init.d/S99myapp
```

重新编译 rootfs，然后烧录。

开机后检查：

```bash
ps | grep my_app
cat /tmp/my_app.log
```

---

# 22. 总结

Linux 开机启动程序主要靠 init 系统。

对你的 Buildroot 嵌入式 Linux，最常用方式是：

```text
/etc/init.d/S99myapp
```

常见做法：

```text
1. 把应用放到 /usr/bin/my_app
2. 写启动脚本 /etc/init.d/S99myapp
3. 脚本里用 /usr/bin/my_app & 后台启动
4. 用 rootfs overlay 放进 Buildroot
5. 重新生成 rootfs
6. 烧录后开机自动启动
```

如果只是开机执行一次初始化动作：

```text
写 Sxx 脚本即可
```

如果应用崩溃后还要自动重启：

```text
用 /etc/inittab respawn
```

如果是 Ubuntu/Debian 那种系统：

```text
用 systemd service
```

你现在优先掌握这个就够了：

```text
Buildroot rootfs_overlay
    ↓
/etc/init.d/S99myapp
    ↓
开机自动启动 /usr/bin/my_app
```
