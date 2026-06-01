可以，**你现在 SPI0 master 已经出来了，就可以通过 `spidev`/`cmd_spi` 直接操作 SPI0，手动读写寄存器**。

但是要注意一个关键点：

```text
同一个 spi0.0 不能同时给 BMI160 驱动和 spidev 用。
```

也就是说，如果你想手动用 `cmd_spi` 读写 BMI160，就要先卸载 BMI160 驱动，释放 `spi0.0`。

---

## 1. 先卸载 BMI160 驱动，释放 spi0.0

```sh
cd /tmp/kernel_drv_mode

rmmod bmi160_spi_board 2>/dev/null
rmmod bmi160_spi 2>/dev/null
rmmod bmi160_core 2>/dev/null
```

然后确认：

```sh
ls -l /sys/bus/spi/devices/
```

如果 `spi0.0` 还在，可能是 board 模块没卸干净或者 spidev 已经创建过。

---

## 2. 加载 spidev 驱动

```sh
cd /tmp/kernel_drv_mode

insmod spidev_helper.ko 2>/dev/null
insmod spidev.ko 2>/dev/null
```

然后添加 SPI 设备，CS 用你的 `PD05`：

```sh
cmd_spi add_dev 0 pd05
```

成功后应该有：

```sh
ls -l /dev/spidev*
ls -l /sys/bus/spi/devices/
```

正常会看到：

```text
/dev/spidev0.0
/sys/bus/spi/devices/spi0.0
```

---

## 3. 设置 SPI 参数

先用低速，MODE0：

```sh
cmd_spi set /dev/spidev0.0 mode=0x00 speed=100000 bits=8 lsb=0
cmd_spi info /dev/spidev0.0
```

如果 MODE0 读不到，再试 MODE3：

```sh
cmd_spi set /dev/spidev0.0 mode=0x03 speed=100000 bits=8 lsb=0
cmd_spi info /dev/spidev0.0
```

---

# 4. SPI 读寄存器怎么发？

以 BMI160 为例，SPI 读寄存器时，一般是：

```text
读命令 = 寄存器地址 | 0x80
然后再发一个 dummy byte 0x00，用来接收返回数据
```

比如读寄存器 `0x00`：

```text
0x00 | 0x80 = 0x80
```

命令：

```sh
cmd_spi transfer /dev/spidev0.0 0x80 0x00
```

返回里一般看第二个字节：

```text
receive[0]: xx
receive[1]: d1
```

BMI160 的 chip id 正常应该是：

```text
0xD1
```

---

## 5. 你说的“读 0x01 寄存器”

读寄存器地址 `0x01`，就是：

```text
0x01 | 0x80 = 0x81
```

执行：

```sh
cmd_spi transfer /dev/spidev0.0 0x81 0x00
```

返回第二个字节就是寄存器 `0x01` 的值。

例如：

```text
receive[0]: 00
receive[1]: xx
```

其中 `xx` 就是读到的数据。

---

# 6. 连续读多个寄存器

比如从 `0x00` 开始连续读 4 个字节：

```sh
cmd_spi transfer /dev/spidev0.0 0x80 0x00 0x00 0x00 0x00
```

含义：

```text
tx[0] = 0x80    读 0x00
tx[1] = 0x00    dummy，接收 reg0x00
tx[2] = 0x00    dummy，接收 reg0x01
tx[3] = 0x00    dummy，接收 reg0x02
tx[4] = 0x00    dummy，接收 reg0x03
```

所以返回通常从 `receive[1]` 开始看。

---

# 7. SPI 写寄存器怎么发？

写寄存器时，一般是：

```text
写命令 = 寄存器地址 & 0x7F
然后发送要写入的数据
```

比如写寄存器 `0x01`，写入 `0x55`：

```sh
cmd_spi transfer /dev/spidev0.0 0x01 0x55
```

但是注意：**不要随便写 BMI160 寄存器**，有些寄存器是只读，有些会改变电源模式、量程、ODR，甚至软复位。

更安全的操作顺序是：

```text
先只读，不写。
先确认 0x00 能读到 0xD1。
再确认 MODE0/MODE3。
再考虑写配置寄存器。
```

---

# 8. 当前最关键的测试

你现在最应该先执行这几个：

```sh
cmd_spi set /dev/spidev0.0 mode=0x00 speed=100000 bits=8 lsb=0
cmd_spi transfer /dev/spidev0.0 0x80 0x00
```

如果返回第二字节不是 `d1`，再试：

```sh
cmd_spi set /dev/spidev0.0 mode=0x03 speed=100000 bits=8 lsb=0
cmd_spi transfer /dev/spidev0.0 0x80 0x00
```

只要这一步能读到：

```text
receive[1]: d1
```

就说明：

```text
CS 正确
CLK/MOSI/MISO 正确
SPI mode 正确
BMI160 供电正常
```

然后再回去加载 BMI160 驱动，就有希望生成 IIO 设备。

---

## 9. 测完 spidev 后要删除设备

如果后面要重新加载 `bmi160_spi_board.ko`，要先删除 spidev 设备：

```sh
cmd_spi del_dev /dev/spidev0.0
```

然后卸载：

```sh
rmmod spidev 2>/dev/null
rmmod spidev_helper 2>/dev/null
```

再加载 BMI160：

```sh
insmod kfifo_buf.ko 2>/dev/null
insmod industrialio-triggered-buffer.ko 2>/dev/null
insmod bmi160_core.ko 2>/dev/null
insmod bmi160_spi.ko 2>/dev/null
insmod bmi160_spi_board.ko
```

---

一句话：**可以直接操作 SPI0 读写寄存器。读寄存器地址 `0x01` 就发 `0x81 0x00`；写 `0x01=0x55` 就发 `0x01 0x55`。但先别乱写，优先用 `0x80 0x00` 确认 BMI160 的 chip id 能不能读到 `0xD1`。**


-----
----
-----
你这个现象不是 SPI0 不行，而是 **`cmd_spi` 这个用户态工具不在板子 rootfs 里**。

现在你已经有了：

```text
/dev/spidev_helper
```

说明：

```text
spidev_helper.ko 已经加载成功
```

但是没有：

```text
/dev/spidev0.0
```

原因是你还没有执行“添加 SPI 设备”的动作。官方文档里这个动作是靠：

```sh
cmd_spi add_dev 0 pd05
```

完成的。可是你板子上：

```sh
cmd_spi: command not found
```

所以后面当然不会生成 `/dev/spidev0.0`。

---

# 1. 先在板子上确认有没有 cmd_spi

板子执行：

```sh
find / -name "cmd_spi" 2>/dev/null
find / -name "*spi*" 2>/dev/null | grep cmd
```

如果没有任何输出，说明 rootfs 里确实没带这个工具。

---

# 2. PC 端找 cmd_spi 源码或可执行文件

在 Ubuntu 编译环境里执行：

```bash
cd ~/project/x2600

find . -name "cmd_spi" -o -name "cmd_spi.c" -o -name "*cmd*spi*" -o -name "*spi_test*"
```

再搜源码：

```bash
grep -R "cmd_spi" -n . 2>/dev/null | head -100
grep -R "spi_add_device" -n libhardware2 module_driver buildroot package 2>/dev/null | head -100
```

君正文档里说应用层接口在：

```text
libhardware2/src/lib/spi/spi.c
```

并且提供了 `spi_add_device()`、`spi_transfer()`、`spi_set_mode()` 这些接口；`cmd_spi add_dev 0 pc16` 只是官方封装好的命令行工具。

所以你现在要么找到 `cmd_spi` 工具推到板子，要么自己写一个小测试程序调用 `libhardware2/spi.h`。

---

# 3. 如果找到了 cmd_spi，直接推到板子

假设找到：

```text
./某个路径/cmd_spi
```

执行：

```bash
adb push ./某个路径/cmd_spi /tmp/
adb shell "chmod +x /tmp/cmd_spi"
```

板子上：

```sh
/tmp/cmd_spi add_dev 0 pd05
ls -l /dev/spidev*
```

如果成功，会出现：

```text
/dev/spidev0.0
```

然后就可以：

```sh
/tmp/cmd_spi set /dev/spidev0.0 mode=0x00 speed=100000 bits=8 lsb=0
/tmp/cmd_spi info /dev/spidev0.0
/tmp/cmd_spi transfer /dev/spidev0.0 0x80 0x00
```

读 BMI160 chip id，期望第二个返回字节是：

```text
d1
```

---

# 4. 如果找不到 cmd_spi，就自己写一个小程序

PC 端新建：

```bash
cd ~/project/x2600
mkdir -p test_spi_bmi160
cd test_spi_bmi160
vim demo_spi_bmi160.c
```

内容：

```c
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <libhardware2/spi.h>

int main(void)
{
    int spi_fd = -1;

    struct spidev_register_data register_data;
    memset(&register_data, 0, sizeof(register_data));

    /*
     * 你的 BMI160 CS 是 PD05。
     * 注意这里用小写字符串，和文档里的 pc16 类似。
     */
    register_data.busnum = 0;
    register_data.cs_gpio = "pd05";

    printf("add spi device: bus=%d cs=%s\n",
           register_data.busnum, register_data.cs_gpio);

    if (spi_add_device(&register_data) < 0) {
        printf("spi_add_device failed\n");
        return -1;
    }

    usleep(1000 * 1000);

    printf("spidev path: %s\n", register_data.spidev_path);

    spi_fd = spi_open(register_data.spidev_path);
    if (spi_fd < 0) {
        printf("spi_open failed, fd=%d\n", spi_fd);
        return -1;
    }

    /*
     * 先用 MODE0 + 100kHz。
     * 如果读不到，再改 SPI_MODE_3。
     */
    if (spi_set_mode(spi_fd, SPI_MODE_0) < 0) {
        printf("spi_set_mode failed\n");
        return -1;
    }

    if (spi_set_speed(spi_fd, 100000) < 0) {
        printf("spi_set_speed failed\n");
        return -1;
    }

    if (spi_set_bits(spi_fd, 8) < 0) {
        printf("spi_set_bits failed\n");
        return -1;
    }

    if (spi_set_lsb(spi_fd, 0) < 0) {
        printf("spi_set_lsb failed\n");
        return -1;
    }

    /*
     * BMI160 chip id register = 0x00
     * SPI read: addr | 0x80
     * tx[0] = 0x80
     * tx[1] = dummy 0x00
     * rx[1] 应该返回 0xD1
     */
    unsigned char tx[2] = {0x80, 0x00};
    unsigned char rx[2] = {0};

    struct spi_ioc_transfer tr;
    memset(&tr, 0, sizeof(tr));

    tr.tx_buf = (unsigned long)tx;
    tr.rx_buf = (unsigned long)rx;
    tr.len = 2;
    tr.speed_hz = 100000;
    tr.bits_per_word = 8;

    if (spi_transfer(spi_fd, &tr, 1) < 0) {
        printf("spi_transfer failed\n");
        return -1;
    }

    printf("rx[0] = 0x%02x\n", rx[0]);
    printf("rx[1] = 0x%02x\n", rx[1]);

    if (rx[1] == 0xD1) {
        printf("BMI160 chip id OK\n");
    } else {
        printf("BMI160 chip id wrong, expected 0xD1\n");
    }

    spi_close(spi_fd);

    if (spi_del_device(register_data.spidev_path) < 0) {
        printf("spi_del_device failed\n");
        return -1;
    }

    printf("spi test done\n");

    return 0;
}
```

---

# 5. 编译这个小程序

先找交叉编译器：

```bash
cd ~/project/x2600

ls buildroot/buildroot24/output/host/usr/bin/*gcc
```

大概率是：

```text
mips-linux-gnu-gcc
```

编译：

```bash
cd ~/project/x2600/test_spi_bmi160

/home/vik/project/x2600/buildroot/buildroot24/output/host/usr/bin/mips-linux-gnu-gcc \
    demo_spi_bmi160.c \
    -I/home/vik/project/x2600/libhardware2/include \
    -L/home/vik/project/x2600/buildroot/buildroot24/output/target/usr/lib \
    -lhardware2 \
    -o demo_spi_bmi160
```

如果 `-lhardware2` 找不到，再查库位置：

```bash
cd ~/project/x2600

find . -name "libhardware2*.so" -o -name "libhardware2*.a"
```

然后把 `-L` 路径改成实际路径。

---

# 6. 推到板子运行

```bash
adb push demo_spi_bmi160 /tmp/
adb shell "chmod +x /tmp/demo_spi_bmi160"
```

板子上先确保 SPI0 master 已经起来：

```sh
ls /sys/class/spi_master/
```

必须看到：

```text
spi0
```

再运行：

```sh
/tmp/demo_spi_bmi160
```

如果输出：

```text
rx[1] = 0xd1
BMI160 chip id OK
```

说明 SPI 通信正常。

如果输出：

```text
rx[1] = 0x00
```

那就和 BMI160 驱动里的报错一致：

```text
Wrong chip id, got 0 expected d1
```

说明问题在硬件连接、CS、mode、供电、MISO/MOSI，不是 Linux 驱动加载链路。

---

# 7. 你现在的问题总结

现在已经确认：

```text
SPI0 master 已经出来了
spidev_helper.ko 也加载了
/dev/spidev_helper 出来了
但是 cmd_spi 工具缺失
所以无法用官方命令 add_dev
```

你下一步有两个选择：

```text
方案 A：找到 cmd_spi，push 到板子
方案 B：自己编 demo_spi_bmi160.c，用 libhardware2/spi.h 直接测试
```

最推荐先执行这个：

```bash
cd ~/project/x2600
find . -name "cmd_spi" -o -name "cmd_spi.c" -o -name "*cmd*spi*"
```

找到就推过去；找不到就编上面那个 demo。
