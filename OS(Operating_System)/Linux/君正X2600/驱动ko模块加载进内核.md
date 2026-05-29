可以，按你这个目录结构，**正式产品方式**应该这样做：

```text id="tf9a87"
1. kernel 编译出 bmi160_core.ko / bmi160_spi.ko
2. module_driver 编译出 bmi160_spi_board.ko
3. 把这些 .ko 放进 buildroot 的 rootfs 目录
4. 在 rootfs 里加 /etc/init.d/Sxx 脚本自动加载
5. 重新打包 rootfs / 系统镜像
6. 烧录后板子启动自动 insmod / modprobe
```

你现在目录里已经有：

```bash id="u1v0ez"
module_driver/bmi160_spi_board
```

这个很关键。它大概率就是你自己写的 **BMI160 SPI 板级注册模块**，用于告诉内核：

```text id="b4lhbg"
SPI0 上挂了一个 BMI160
CS 是 PD05
```

所以正式集成时，不只是放：

```bash id="9nf5du"
bmi160_spi.ko
```

还要放：

```bash id="kzdxz5"
bmi160_core.ko
bmi160_spi.ko
bmi160_spi_board.ko
```

必要时还要放：

```bash id="s0r2s9"
regmap-spi.ko
```

---

# 一、先把 rootfs 目录搞出来

你前面 `make clean` 报过：

```bash id="1lc4e0"
cp: 无法创建普通文件 '/home/vik/project/x2600/build/../buildroot/buildroot24/output/target/etc/init.d/': 没有那个文件或目录
```

这说明你当前这个目录可能还没生成：

```bash id="uljcqy"
~/project/x2600/buildroot/buildroot24/output/target
```

先检查：

```bash id="wv151d"
cd ~/project/x2600

ls -ld buildroot/buildroot24/output/target
ls -ld buildroot/buildroot24/output/target/etc/init.d
```

如果不存在，先编译一次 rootfs/buildroot。你可以先查工程支持什么命令：

```bash id="9ck3hr"
cd ~/project/x2600/build

grep -nE "^[a-zA-Z0-9_%-]+:" Makefile cmd_*.mk | grep -E "buildroot|rootfs|image|system|all"
```

大概率有类似：

```bash id="xzx1b3"
make buildroot
make rootfs
make image
make all
```

如果你不知道先用哪个，可以先试：

```bash id="2vj7gu"
cd ~/project/x2600/build
make buildroot
```

如果没有这个目标，再看 `grep` 输出。

---

# 二、先确认 ko 都编出来了

执行：

```bash id="o51zi8"
cd ~/project/x2600

echo "===== kernel BMI160 ko ====="
find kernel/kernel -name "*bmi160*.ko"

echo "===== regmap spi ko ====="
find kernel/kernel -name "*regmap*spi*.ko" -o -name "regmap-spi.ko"

echo "===== module_driver board ko ====="
find module_driver -name "*bmi160*.ko"
```

你理想情况下应该看到类似：

```bash id="506qa5"
kernel/kernel/drivers/iio/imu/bmi160/bmi160_core.ko
kernel/kernel/drivers/iio/imu/bmi160/bmi160_spi.ko
module_driver/output/bmi160_spi_board.ko
```

如果 `module_driver` 下面没有 `bmi160_spi_board.ko`，说明你的板级模块还没编出来，需要：

```bash id="78vp3g"
cd ~/project/x2600/build
make module_driver
```

然后再查：

```bash id="5yc29d"
find ~/project/x2600/module_driver -name "*bmi160*.ko"
```

---

# 三、把 ko 安装进 rootfs

先取内核版本号：

```bash id="rer0si"
cd ~/project/x2600/kernel/kernel
KREL=$(make -s kernelrelease)
echo $KREL
```

假设输出是：

```bash id="2p0q3m"
5.10.21
```

那板子上的模块目录应该是：

```bash id="4p7jv8"
/lib/modules/5.10.21/
```

PC 端 rootfs 对应目录是：

```bash id="l20vcu"
~/project/x2600/buildroot/buildroot24/output/target/lib/modules/5.10.21/
```

执行下面这套：

```bash id="measuv"
cd ~/project/x2600

TARGET=$PWD/buildroot/buildroot24/output/target
KREL=$(make -s -C kernel/kernel kernelrelease)

echo "TARGET=$TARGET"
echo "KREL=$KREL"

mkdir -p $TARGET/lib/modules/$KREL/extra/bmi160

cp -v kernel/kernel/drivers/iio/imu/bmi160/bmi160_core.ko \
      $TARGET/lib/modules/$KREL/extra/bmi160/

cp -v kernel/kernel/drivers/iio/imu/bmi160/bmi160_spi.ko \
      $TARGET/lib/modules/$KREL/extra/bmi160/
```

然后把你的板级模块也复制进去。先查实际路径：

```bash id="afvirr"
find module_driver -name "bmi160_spi_board.ko"
```

假如找到的是：

```bash id="evuevf"
module_driver/output/bmi160_spi_board.ko
```

就复制：

```bash id="xsm9cg"
cp -v module_driver/output/bmi160_spi_board.ko \
      $TARGET/lib/modules/$KREL/extra/bmi160/
```

如果 `regmap-spi.ko` 也是模块，也复制：

```bash id="cdf294"
find kernel/kernel -name "regmap-spi.ko"
```

如果有结果，比如：

```bash id="5a7doz"
kernel/kernel/drivers/base/regmap/regmap-spi.ko
```

就执行：

```bash id="t9s4k4"
cp -v kernel/kernel/drivers/base/regmap/regmap-spi.ko \
      $TARGET/lib/modules/$KREL/extra/bmi160/
```

---

# 四、加启动脚本，随系统启动自动加载

创建 init 脚本：

```bash id="08j1e0"
cd ~/project/x2600

TARGET=$PWD/buildroot/buildroot24/output/target
KREL=$(make -s -C kernel/kernel kernelrelease)

mkdir -p $TARGET/etc/init.d

cat > $TARGET/etc/init.d/S45bmi160 <<'EOF'
#!/bin/sh

case "$1" in
    start)
        echo "[BMI160] loading bmi160 modules..."

        KREL=$(uname -r)
        MODDIR=/lib/modules/$KREL/extra/bmi160

        # 有些系统没有 modprobe，所以这里用 insmod，顺序很重要
        [ -f $MODDIR/regmap-spi.ko ] && insmod $MODDIR/regmap-spi.ko

        [ -f $MODDIR/bmi160_core.ko ] && insmod $MODDIR/bmi160_core.ko
        [ -f $MODDIR/bmi160_spi.ko ] && insmod $MODDIR/bmi160_spi.ko

        # 这个是你自己的板级注册模块，通常最后加载
        [ -f $MODDIR/bmi160_spi_board.ko ] && insmod $MODDIR/bmi160_spi_board.ko

        echo "[BMI160] done"
        ;;
    stop)
        echo "[BMI160] unloading bmi160 modules..."

        rmmod bmi160_spi_board 2>/dev/null
        rmmod bmi160_spi 2>/dev/null
        rmmod bmi160_core 2>/dev/null
        rmmod regmap_spi 2>/dev/null

        echo "[BMI160] stopped"
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
EOF

chmod +x $TARGET/etc/init.d/S45bmi160
```

这个脚本会被放进板子的：

```bash id="2ozzhd"
/etc/init.d/S45bmi160
```

系统启动时，如果你的 rootfs 是 BusyBox init 风格，一般会自动按 `Sxx` 顺序执行。

---

# 五、重新打包 rootfs / 系统镜像

复制进 `output/target` 之后，还没有真正进入烧录镜像。你还需要重新打包。

先查目标：

```bash id="c4es9l"
cd ~/project/x2600/build

grep -nE "^[a-zA-Z0-9_%-]+:" Makefile cmd_*.mk | grep -E "rootfs|image|system|all|pack"
```

常见可能是：

```bash id="r120gd"
make rootfs
make image
```

或者：

```bash id="l1b67o"
make buildroot
make image
```

或者直接：

```bash id="jz5x82"
make all
```

你可以先试：

```bash id="fsvwer"
cd ~/project/x2600/build
make rootfs
make image
```

如果没有 `rootfs` 目标，就用刚才 `grep` 找到的实际目标。

---

# 六、烧录后板子上检查

板子启动后：

```bash id="cykm8r"
adb shell
```

先看模块文件有没有进系统：

```bash id="z0iakn"
uname -r
find /lib/modules/$(uname -r) -name "*bmi160*.ko"
find /lib/modules/$(uname -r) -name "regmap-spi.ko"
```

应该能看到：

```bash id="tpljkr"
/lib/modules/5.10.xx/extra/bmi160/bmi160_core.ko
/lib/modules/5.10.xx/extra/bmi160/bmi160_spi.ko
/lib/modules/5.10.xx/extra/bmi160/bmi160_spi_board.ko
```

看启动脚本有没有：

```bash id="skx97z"
ls -l /etc/init.d/S45bmi160
```

看模块有没有加载：

```bash id="jcpg4y"
lsmod | grep -E "bmi160|regmap"
```

看日志：

```bash id="aq635o"
dmesg | grep -iE "bmi160|iio|spi"
```

看 IIO 设备：

```bash id="7vyyy2"
ls /sys/bus/iio/devices/
```

如果有：

```bash id="4tl8jk"
iio:device0
```

继续看：

```bash id="wbacyg"
cd /sys/bus/iio/devices/iio:device0
cat name
ls
```

读取数据：

```bash id="ouurfa"
cat in_accel_x_raw
cat in_accel_y_raw
cat in_accel_z_raw
cat in_anglvel_x_raw
cat in_anglvel_y_raw
cat in_anglvel_z_raw
```

---

# 七、但是我更建议你做成“源码级永久集成”

上面这种是**直接往 output/target 塞文件**，可以快速一步到位验证。
但是如果你以后 `make clean` 或重新 buildroot，`output/target` 可能会被清掉。

正式产品更稳的是：把复制逻辑写进工程 Makefile。

你这个工程已经有：

```bash id="8th48y"
module_driver/bmi160_spi_board
module_driver/Makefile
module_driver/output
```

所以更合理的是在 `module_driver` 的安装阶段，把 `bmi160_spi_board.ko` 复制到 rootfs。

而 kernel 自带的：

```bash id="kaf44y"
bmi160_core.ko
bmi160_spi.ko
```

更标准的方式是走：

```bash id="4zkx8p"
make modules_install INSTALL_MOD_PATH=...
```

你可以手动执行一次：

```bash id="x50fk9"
cd ~/project/x2600/kernel/kernel

export PATH=/home/vik/project/x2600/tools/toolchains/mips-xburst2-gcc720-glibc238/bin:$PATH

make ARCH=mips modules_install \
    INSTALL_MOD_PATH=/home/vik/project/x2600/buildroot/buildroot24/output/target
```

这会把内核模块安装到：

```bash id="6mg0pe"
/home/vik/project/x2600/buildroot/buildroot24/output/target/lib/modules/$(kernelrelease)/
```

然后你只需要额外复制：

```bash id="np84pg"
module_driver/output/bmi160_spi_board.ko
```

和添加启动脚本。

---

# 八、你现在最推荐走这个流程

你现在先不要管自动 `modprobe`，先用 `insmod` 脚本最稳：

```bash id="xeb0c4"
cd ~/project/x2600/build

make kernel
make module_driver
```

然后：

```bash id="wbf18i"
cd ~/project/x2600

TARGET=$PWD/buildroot/buildroot24/output/target
KREL=$(make -s -C kernel/kernel kernelrelease)

mkdir -p $TARGET/lib/modules/$KREL/extra/bmi160
mkdir -p $TARGET/etc/init.d

cp -v kernel/kernel/drivers/iio/imu/bmi160/bmi160_core.ko $TARGET/lib/modules/$KREL/extra/bmi160/
cp -v kernel/kernel/drivers/iio/imu/bmi160/bmi160_spi.ko  $TARGET/lib/modules/$KREL/extra/bmi160/

BOARD_KO=$(find module_driver -name "bmi160_spi_board.ko" | head -1)
echo "BOARD_KO=$BOARD_KO"
cp -v "$BOARD_KO" $TARGET/lib/modules/$KREL/extra/bmi160/
```

然后创建 `/etc/init.d/S45bmi160`，重新打包镜像，烧录。

---

## 最关键的一句话

你想“正式产品一样随系统启动”，不是只处理这个文件：

```bash id="hm55ah"
bmi160_spi.ko
```

而是要处理完整链路：

```text id="7xaw3x"
kernel 配置打开 BMI160_SPI=m
        ↓
make kernel 生成 bmi160_core.ko / bmi160_spi.ko
        ↓
make module_driver 生成 bmi160_spi_board.ko
        ↓
复制到 buildroot/buildroot24/output/target/lib/modules/$(uname -r)/extra/bmi160/
        ↓
添加 buildroot/buildroot24/output/target/etc/init.d/S45bmi160
        ↓
重新打包 rootfs / 系统镜像
        ↓
烧录
        ↓
启动后 lsmod / dmesg / /sys/bus/iio/devices 检查
```

这样才是接近正式产品的集成方式。
