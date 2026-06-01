可以加。先明确一下：

```text
cmd_spi 不是 KO，不是内核驱动。
cmd_spi 是用户态命令行程序。
它依赖：
1. soc_spi.ko 已经创建 spi0
2. spidev_helper.ko / spidev.ko 已加载
3. libhardware2 的 SPI 接口
```

君正文档里说的 `cmd_spi add_dev 0 pc16`、`cmd_spi transfer /dev/spidev0.0 ...`、`cmd_spi set/info/del_dev` 就是为了在 shell 里调试 SPI 收发。文档还说明应用层接口包括 `spi_add_device()`、`spi_transfer()`、`spi_set_mode()`、`spi_set_speed()`、`spi_set_bits()`、`spi_set_lsb()`、`spi_del_device()` 等。

---

# 1. 先在工程里找现成的 cmd_spi

你先执行：

```bash
cd ~/project/x2600

find . -name "cmd_spi" -o -name "cmd_spi.c" -o -name "*cmd*spi*" -o -name "*spi*cmd*"
```

再搜源码关键字：

```bash
grep -R "cmd_spi" -n . 2>/dev/null | head -100
grep -R "spi_add_device" -n libhardware2 module_driver buildroot package . 2>/dev/null | head -100
```

如果能找到已经编好的 `cmd_spi` 可执行文件，比如：

```text
./buildroot/buildroot24/output/target/usr/bin/cmd_spi
```

或者：

```text
./xxx/cmd_spi
```

那直接推到板子：

```bash
adb push ./找到的路径/cmd_spi /tmp/
adb shell "chmod +x /tmp/cmd_spi"
```

板子上用：

```sh
/tmp/cmd_spi add_dev 0 pd05
/tmp/cmd_spi set /dev/spidev0.0 mode=0x00 speed=100000 bits=8 lsb=0
/tmp/cmd_spi transfer /dev/spidev0.0 0x80 0x00
```

你的 CS 是 `PD05`，所以文档里的：

```sh
cmd_spi add_dev 0 pc16
```

你要换成：

```sh
cmd_spi add_dev 0 pd05
```

文档里 `cmd_spi add_dev 0 pc16` 的含义就是：在 SPI bus 0 上，用 `pc16` 作为片选，创建 `/dev/spidev0.0`。

---

# 2. 如果找不到 cmd_spi，就自己写一个 `cmd_spi_lite`

你可以自己做一个简化版，专门用于调试 BMI160 或其他 SPI 设备。

新建目录：

```bash
cd ~/project/x2600

mkdir -p user_tools/cmd_spi_lite
cd user_tools/cmd_spi_lite
vim cmd_spi_lite.c
```

写入下面代码：

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <libhardware2/spi.h>

static void usage(const char *prog)
{
    printf("Usage:\n");
    printf("  %s add_dev <busnum> <cs_gpio>\n", prog);
    printf("  %s del_dev <spidev_path>\n", prog);
    printf("  %s set <spidev_path> mode=<hex> speed=<hz> bits=<bits> lsb=<0|1>\n", prog);
    printf("  %s info <spidev_path>\n", prog);
    printf("  %s transfer <spidev_path> <byte0> <byte1> ...\n", prog);
    printf("\n");
    printf("Examples:\n");
    printf("  %s add_dev 0 pd05\n", prog);
    printf("  %s set /dev/spidev0.0 mode=0x00 speed=100000 bits=8 lsb=0\n", prog);
    printf("  %s transfer /dev/spidev0.0 0x80 0x00\n", prog);
    printf("  %s del_dev /dev/spidev0.0\n", prog);
}

static int parse_int_auto(const char *s)
{
    return (int)strtol(s, NULL, 0);
}

static int cmd_add_dev(int argc, char **argv)
{
    struct spidev_register_data data;
    memset(&data, 0, sizeof(data));

    if (argc != 4) {
        usage(argv[0]);
        return -1;
    }

    data.busnum = parse_int_auto(argv[2]);
    data.cs_gpio = argv[3];

    printf("add spi device: bus=%d cs=%s\n", data.busnum, data.cs_gpio);

    if (spi_add_device(&data) < 0) {
        printf("spi_add_device failed\n");
        return -1;
    }

    printf("%s\n", data.spidev_path);
    return 0;
}

static int cmd_del_dev(int argc, char **argv)
{
    if (argc != 3) {
        usage(argv[0]);
        return -1;
    }

    if (spi_del_device(argv[2]) < 0) {
        printf("spi_del_device failed: %s\n", argv[2]);
        return -1;
    }

    printf("delete %s ok\n", argv[2]);
    return 0;
}

static int cmd_set(int argc, char **argv)
{
    int fd;
    int mode = -1;
    int speed = -1;
    int bits = -1;
    int lsb = -1;

    if (argc < 3) {
        usage(argv[0]);
        return -1;
    }

    fd = spi_open(argv[2]);
    if (fd < 0) {
        printf("spi_open failed: %s\n", argv[2]);
        return -1;
    }

    for (int i = 3; i < argc; i++) {
        if (strncmp(argv[i], "mode=", 5) == 0) {
            mode = parse_int_auto(argv[i] + 5);
        } else if (strncmp(argv[i], "speed=", 6) == 0) {
            speed = parse_int_auto(argv[i] + 6);
        } else if (strncmp(argv[i], "bits=", 5) == 0) {
            bits = parse_int_auto(argv[i] + 5);
        } else if (strncmp(argv[i], "lsb=", 4) == 0) {
            lsb = parse_int_auto(argv[i] + 4);
        } else {
            printf("unknown option: %s\n", argv[i]);
        }
    }

    if (mode >= 0 && spi_set_mode(fd, mode) < 0) {
        printf("spi_set_mode failed\n");
        spi_close(fd);
        return -1;
    }

    if (speed > 0 && spi_set_speed(fd, speed) < 0) {
        printf("spi_set_speed failed\n");
        spi_close(fd);
        return -1;
    }

    if (bits > 0 && spi_set_bits(fd, bits) < 0) {
        printf("spi_set_bits failed\n");
        spi_close(fd);
        return -1;
    }

    if (lsb >= 0 && spi_set_lsb(fd, lsb) < 0) {
        printf("spi_set_lsb failed\n");
        spi_close(fd);
        return -1;
    }

    printf("set ok\n");

    spi_close(fd);
    return 0;
}

static int cmd_info(int argc, char **argv)
{
    int fd;
    struct spi_info info;

    if (argc != 3) {
        usage(argv[0]);
        return -1;
    }

    fd = spi_open(argv[2]);
    if (fd < 0) {
        printf("spi_open failed: %s\n", argv[2]);
        return -1;
    }

    memset(&info, 0, sizeof(info));
    spi_get_info(fd, &info);

    printf("mode speed bits lsb\n");
    printf("0x%02x %u %u %u\n",
           info.spi_mode,
           info.spi_speed,
           info.spi_bits,
           info.spi_lsb);

    spi_close(fd);
    return 0;
}

static int cmd_transfer(int argc, char **argv)
{
    int fd;
    int len;
    unsigned char tx[256];
    unsigned char rx[256];

    if (argc < 4) {
        usage(argv[0]);
        return -1;
    }

    len = argc - 3;
    if (len > (int)sizeof(tx)) {
        printf("too many bytes, max=%zu\n", sizeof(tx));
        return -1;
    }

    memset(tx, 0, sizeof(tx));
    memset(rx, 0, sizeof(rx));

    for (int i = 0; i < len; i++) {
        tx[i] = (unsigned char)parse_int_auto(argv[3 + i]);
    }

    fd = spi_open(argv[2]);
    if (fd < 0) {
        printf("spi_open failed: %s\n", argv[2]);
        return -1;
    }

    struct spi_ioc_transfer tr;
    memset(&tr, 0, sizeof(tr));

    tr.tx_buf = (unsigned long)tx;
    tr.rx_buf = (unsigned long)rx;
    tr.len = len;
    tr.speed_hz = 0;
    tr.bits_per_word = 8;

    if (spi_transfer(fd, &tr, 1) < 0) {
        printf("spi_transfer failed\n");
        spi_close(fd);
        return -1;
    }

    for (int i = 0; i < len; i++) {
        printf("receive[%d]: %02x\n", i, rx[i]);
    }

    spi_close(fd);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        usage(argv[0]);
        return -1;
    }

    if (strcmp(argv[1], "add_dev") == 0) {
        return cmd_add_dev(argc, argv);
    } else if (strcmp(argv[1], "del_dev") == 0) {
        return cmd_del_dev(argc, argv);
    } else if (strcmp(argv[1], "set") == 0) {
        return cmd_set(argc, argv);
    } else if (strcmp(argv[1], "info") == 0) {
        return cmd_info(argc, argv);
    } else if (strcmp(argv[1], "transfer") == 0) {
        return cmd_transfer(argc, argv);
    }

    usage(argv[0]);
    return -1;
}
```

这个程序就是根据君正文档里的应用层接口写的：用 `spi_add_device()` 添加设备，用 `spi_set_mode/speed/bits/lsb` 配置 SPI，用 `spi_transfer()` 收发数据，用 `spi_del_device()` 删除设备。

---

# 3. 编译 `cmd_spi_lite`

先找 `libhardware2` 的头文件和库：

```bash
cd ~/project/x2600

find . -path "*libhardware2*" -type f | head -50
find . -name "spi.h" | grep libhardware2
find . -name "libhardware2*.so" -o -name "libhardware2*.a"
```

常见路径可能是：

```text
libhardware2/include/libhardware2/spi.h
buildroot/buildroot24/output/target/usr/lib/libhardware2.so
```

然后编译：

```bash
cd ~/project/x2600/user_tools/cmd_spi_lite

/home/vik/project/x2600/buildroot/buildroot24/output/host/usr/bin/mips-linux-gnu-gcc \
    cmd_spi_lite.c \
    -I/home/vik/project/x2600/libhardware2/include \
    -L/home/vik/project/x2600/buildroot/buildroot24/output/target/usr/lib \
    -lhardware2 \
    -o cmd_spi_lite
```

如果报找不到 `libhardware2.so`，用你 `find` 查到的实际路径替换 `-L`。

如果头文件路径不对，也用 `find . -name spi.h | grep libhardware2` 查到的路径替换 `-I`。

---

# 4. 推到板子

```bash
adb push cmd_spi_lite /tmp/
adb shell "chmod +x /tmp/cmd_spi_lite"
```

如果板子上没有 `libhardware2.so`，还要推库：

```bash
adb push buildroot/buildroot24/output/target/usr/lib/libhardware2.so /usr/lib/
```

或者临时推到 `/tmp` 后这样运行：

```sh
export LD_LIBRARY_PATH=/tmp:$LD_LIBRARY_PATH
```

---

# 5. 板子上使用流程

先确保 SPI0 master 已经起来：

```sh
cd /tmp/kernel_drv_mode

insmod utils.ko 2>/dev/null

insmod ./soc_spi.ko spi0_is_enable=1 spi1_is_enable=0 div_ssi_rate=10000000 spi0_clk=PD00 spi0_mosi=PD01 spi0_miso=PD02 2>/dev/null

ls /sys/class/spi_master/
```

必须看到：

```text
spi0
```

再加载 spidev 相关模块：

```sh
insmod spidev_helper.ko 2>/dev/null
insmod spidev.ko 2>/dev/null
```

添加你的 BMI160 SPI 设备：

```sh
/tmp/cmd_spi_lite add_dev 0 pd05
```

看是否生成：

```sh
ls -l /dev/spidev*
ls -l /sys/bus/spi/devices/
```

应该有：

```text
/dev/spidev0.0
/sys/bus/spi/devices/spi0.0
```

设置 SPI 参数：

```sh
/tmp/cmd_spi_lite set /dev/spidev0.0 mode=0x00 speed=100000 bits=8 lsb=0
/tmp/cmd_spi_lite info /dev/spidev0.0
```

读 BMI160 chip id：

```sh
/tmp/cmd_spi_lite transfer /dev/spidev0.0 0x80 0x00
```

期望：

```text
receive[1]: d1
```

如果 MODE0 读不到，再试 MODE3：

```sh
/tmp/cmd_spi_lite set /dev/spidev0.0 mode=0x03 speed=100000 bits=8 lsb=0
/tmp/cmd_spi_lite transfer /dev/spidev0.0 0x80 0x00
```

---

# 6. 读其他寄存器

读寄存器 `0x01`：

```sh
/tmp/cmd_spi_lite transfer /dev/spidev0.0 0x81 0x00
```

读寄存器 `0x02`：

```sh
/tmp/cmd_spi_lite transfer /dev/spidev0.0 0x82 0x00
```

连续读从 `0x00` 开始的 8 个寄存器：

```sh
/tmp/cmd_spi_lite transfer /dev/spidev0.0 0x80 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00
```

返回一般从 `receive[1]` 开始看。

写寄存器 `0x7E=0x11` 这种格式：

```sh
/tmp/cmd_spi_lite transfer /dev/spidev0.0 0x7E 0x11
```

不过 BMI160 不要乱写，先只读 `0x00` chip id。

---

# 7. 测完后删除 spidev

后面你要重新加载 BMI160 驱动时，必须删除 spidev，不然 `spi0.0` 会被占用：

```sh
/tmp/cmd_spi_lite del_dev /dev/spidev0.0
```

然后再加载 BMI160：

```sh
cd /tmp/kernel_drv_mode

insmod kfifo_buf.ko 2>/dev/null
insmod industrialio-triggered-buffer.ko 2>/dev/null
insmod bmi160_core.ko 2>/dev/null
insmod bmi160_spi.ko 2>/dev/null
insmod bmi160_spi_board.ko
```

---

# 8. 后续正式加入 rootfs

等 `cmd_spi_lite` 编译好，可以复制到 rootfs：

```bash
cd ~/project/x2600

mkdir -p buildroot/buildroot24/output/target/usr/bin

cp -v user_tools/cmd_spi_lite/cmd_spi_lite \
      buildroot/buildroot24/output/target/usr/bin/cmd_spi
```

这样板子上以后就能直接：

```sh
cmd_spi add_dev 0 pd05
cmd_spi transfer /dev/spidev0.0 0x80 0x00
```

然后重新打包 rootfs/镜像。

---

# 9. 你现在最快路线

你先执行这个找一下有没有官方现成工具：

```bash
cd ~/project/x2600
find . -name "cmd_spi" -o -name "cmd_spi.c" -o -name "*cmd*spi*"
```

如果找不到，就直接用上面的 `cmd_spi_lite.c` 自己编一个。

这样你就能在 shell 里做：

```sh
cmd_spi add_dev 0 pd05
cmd_spi set /dev/spidev0.0 mode=0x00 speed=100000 bits=8 lsb=0
cmd_spi transfer /dev/spidev0.0 0x80 0x00
cmd_spi del_dev /dev/spidev0.0
```

这套就是你现在排查 `Wrong chip id, got 0 expected d1` 最有用的工具。
