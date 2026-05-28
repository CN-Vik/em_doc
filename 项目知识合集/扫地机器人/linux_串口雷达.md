对，这个场景和 **SPI IMU** 不一样。
扫地机主控 SoC 和雷达用 **串口 UART** 通信时，通常不是你去写一个“SPI 那种总线驱动”，而是：

```text
Linux 内核已经有 UART 控制器驱动
          ↓
系统生成 /dev/ttyS0 /dev/ttyS1 /dev/ttyS2 ...
          ↓
应用程序打开串口设备文件
          ↓
read() 读取雷达数据帧
          ↓
解析雷达协议
          ↓
得到角度、距离、强度、转速等数据
```

也就是说，**雷达驱动很多时候不在 kernel 里，而是在应用层程序里解析串口数据。**

---

# 1. 硬件上是怎么连的？

雷达模块一般有这些线：

```text
雷达 TX  --->  X2600 UART_RX
雷达 RX  <---  X2600 UART_TX
雷达 GND --->  X2600 GND
雷达 VCC --->  电源
```

注意：

```text
TX 接 RX
RX 接 TX
GND 必须共地
```

有些雷达只发数据，不需要主控下发命令，那可能只接：

```text
雷达 TX ---> SoC RX
GND 共地
VCC 供电
```

---

# 2. 数据是怎么进 Linux 的？

数据流大概是这样：

```text
雷达内部 MCU
    ↓
UART TX 引脚输出字节流
    ↓
X2600 UART RX 引脚
    ↓
X2600 UART 控制器 FIFO
    ↓
UART 中断 / DMA
    ↓
Linux 串口驱动
    ↓
TTY 子系统
    ↓
/dev/ttySx
    ↓
应用程序 read()
    ↓
解析雷达协议
```

你可以理解成：

```text
内核 UART 驱动只负责“搬字节”
雷达协议解析通常由应用程序负责
```

---

# 3. 所以“雷达驱动”到底在哪里？

分两层看。

## 第一层：UART 硬件驱动

这个在 Linux 内核里，负责让 X2600 的 UART 能工作。

通常在类似目录：

```text
kernel/kernel-x2000/drivers/tty/serial/
```

或者君正自己的串口驱动目录里。

这个驱动做的事情是：

```text
配置 UART 寄存器
设置波特率
配置中断
收发 FIFO
注册 tty 设备
生成 /dev/ttyS0 /dev/ttyS1 ...
```

这个你一般不用自己写，SDK 已经有。

---

## 第二层：雷达协议驱动 / 应用程序

这个才是你扫地机项目真正要写或者改的地方。

它可能在：

```text
demos/
app/
package/
factory_test/
libhardware2/
libutils2/
```

或者厂家自己的业务程序里。

它做的事情是：

```text
打开 /dev/ttySx
设置波特率
读取串口数据
查找帧头
按协议解析一帧数据
校验 checksum / CRC
提取角度、距离、强度
送给导航 / 建图 / 避障算法
```

---

# 4. 举个例子

假设雷达接在：

```text
/dev/ttyS1
```

波特率是：

```text
230400
```

应用程序会类似这样：

```c
int fd = open("/dev/ttyS1", O_RDWR | O_NOCTTY);

设置波特率 230400
设置 8N1
关闭流控

while (1) {
    read(fd, buf, sizeof(buf));
    parse_lidar_frame(buf);
}
```

---

# 5. 串口雷达数据一般长什么样？

不同雷达协议不一样，但一般都是一包一包的数据帧，比如：

```text
帧头       长度       命令/类型     数据区        校验
0xAA55    len        type          payload       checksum
```

或者：

```text
帧头       角度       距离       强度       CRC
0x54      angle      dist       intensity   crc
```

应用层需要做的是：

```text
1. 从串口字节流里找帧头
2. 判断长度够不够
3. 校验 checksum
4. 解析距离/角度
5. 转成激光点云/障碍物数据
```

---

# 6. 为什么不用写 kernel 驱动？

因为 UART 雷达本质上传的是一串字节。

Linux 已经把 UART 抽象成文件：

```text
/dev/ttyS0
/dev/ttyS1
/dev/ttyS2
```

应用程序直接读这个文件就可以拿到数据。

所以很多扫地机雷达不是这样：

```text
雷达硬件驱动在 kernel 里
```

而是这样：

```text
UART 驱动在 kernel 里
雷达协议解析在 app 里
```

这和 SPI/I2C 传感器不一样。

---

# 7. 和 SPI IMU 的区别

SPI IMU 通常是：

```text
内核驱动主动读寄存器
```

例如：

```text
读 WHO_AM_I
读加速度寄存器
读陀螺仪寄存器
配置采样率
处理中断
```

但是串口雷达通常是：

```text
雷达自己一直往外吐数据
Linux 应用程序被动接收
```

所以数据模式不同：

```text
SPI IMU：主控主动问，设备回答
UART 雷达：雷达持续发，主控一直收
```

---

# 8. 在 X2600 上你要先确认哪个串口

先看系统有哪些串口设备：

```bash
ls -l /dev/ttyS*
```

可能有：

```text
/dev/ttyS0
/dev/ttyS1
/dev/ttyS2
```

然后看内核日志：

```bash
dmesg | grep -i tty
dmesg | grep -i uart
```

你要找到雷达到底接在哪个 UART。

---

# 9. 先不用写代码，直接用命令测试

假设雷达是 `/dev/ttyS1`，波特率是 `230400`。

先配置串口：

```bash
stty -F /dev/ttyS1 230400 cs8 -cstopb -parenb -ixon -ixoff -crtscts raw
```

然后看有没有数据：

```bash
cat /dev/ttyS1 | hexdump -C
```

如果有数据，会看到类似：

```text
00000000  54 2c 68 08 12 34 56 78 ...
00000010  ...
```

如果什么都没有，可能是：

```text
串口号错
波特率错
TX/RX 接反
雷达没供电
雷达需要启动命令
设备树没打开对应 UART
引脚复用没配对
```

---

# 10. 应用层最小测试程序

可以先写一个 `lidar_uart_test.c`：

```c
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <string.h>
#include <errno.h>

static int set_uart(int fd, speed_t baud)
{
    struct termios tio;

    if (tcgetattr(fd, &tio) < 0) {
        perror("tcgetattr");
        return -1;
    }

    cfmakeraw(&tio);

    cfsetispeed(&tio, baud);
    cfsetospeed(&tio, baud);

    tio.c_cflag |= CLOCAL | CREAD;
    tio.c_cflag &= ~PARENB;
    tio.c_cflag &= ~CSTOPB;
    tio.c_cflag &= ~CSIZE;
    tio.c_cflag |= CS8;
    tio.c_cflag &= ~CRTSCTS;

    tio.c_cc[VMIN] = 1;
    tio.c_cc[VTIME] = 0;

    if (tcsetattr(fd, TCSANOW, &tio) < 0) {
        perror("tcsetattr");
        return -1;
    }

    return 0;
}

int main(int argc, char **argv)
{
    const char *dev = "/dev/ttyS1";
    int fd;
    unsigned char buf[256];

    if (argc >= 2)
        dev = argv[1];

    fd = open(dev, O_RDWR | O_NOCTTY);
    if (fd < 0) {
        perror("open uart");
        return -1;
    }

    /*
     * 这里先写 B230400。
     * 如果你的雷达是 115200，就改成 B115200。
     */
    if (set_uart(fd, B230400) < 0) {
        close(fd);
        return -1;
    }

    printf("open %s ok\n", dev);

    while (1) {
        int n = read(fd, buf, sizeof(buf));
        if (n < 0) {
            perror("read");
            break;
        }

        for (int i = 0; i < n; i++) {
            printf("%02X ", buf[i]);
        }
        printf("\n");
        fflush(stdout);
    }

    close(fd);
    return 0;
}
```

交叉编译大概类似：

```bash
mips-linux-gnu-gcc lidar_uart_test.c -o lidar_uart_test
```

然后拷到板子：

```bash
scp lidar_uart_test root@板子IP:/tmp/
```

板子上运行：

```bash
/tmp/lidar_uart_test /dev/ttyS1
```

---

# 11. 如果雷达需要主控发送启动命令

有些雷达不是上电就吐数据，而是需要发命令。

比如协议可能要求：

```text
A5 20
```

表示 start scan。

那应用程序里就要：

```c
unsigned char cmd[] = {0xA5, 0x20};
write(fd, cmd, sizeof(cmd));
```

然后再 `read()` 接收数据。

这个要看你的雷达协议文档。

---

# 12. X2600 设备树里要做什么？

如果某个 UART 默认没打开，要在设备树打开。

你先找 dts：

```bash
cd ~/project/x2600/kernel/kernel-x2000

find arch/mips -name "*.dts" -o -name "*.dtsi" | grep -i x2600
```

然后查 UART：

```bash
grep -R "uart\|ttyS\|serial" -n arch/mips/boot/dts 2>/dev/null | head -100
```

可能会有类似：

```dts
&uart1 {
    status = "okay";
};
```

或者：

```dts
serial@xxxx {
    status = "okay";
};
```

如果雷达接在 UART1，就要保证 UART1：

```text
status = "okay"
pinctrl 配对
波特率由应用程序设置
```

---

# 13. 怎么判断现在业务程序已经在用雷达串口？

在板子上可以查：

```bash
lsof /dev/ttyS1
```

或者：

```bash
fuser /dev/ttyS1
```

如果某个进程正在打开这个串口，会显示进程号。

也可以：

```bash
ps | grep -i lidar
ps | grep -i radar
ps | grep -i scan
```

再用：

```bash
strace -p 进程号 -e read,write,open
```

看它是不是在读 `/dev/ttySx`。

---

# 14. 你可以这样理解“驱动”

在 Linux 开发里，“驱动”这个词容易混。

扫地机雷达这里可能有两种叫法：

```text
底层驱动：
    Linux UART 驱动
    负责 /dev/ttySx 收发字节

业务驱动：
    雷达协议解析程序
    负责把串口字节解析成雷达点云
```

你实际项目里说的“雷达驱动”，大概率是第二种。

---

# 15. 一句话总结

扫地机主控 SoC 和雷达用串口通信时，通常流程是：

```text
雷达通过 UART 持续输出数据帧
X2600 的 UART 驱动把数据送到 /dev/ttySx
应用程序打开 /dev/ttySx
read() 读取字节流
按雷达协议解析出角度、距离、强度
再交给导航/建图/避障算法
```

所以你现在重点不是先写 kernel 驱动，而是先确认：

```text
1. 雷达接的是哪个 UART
2. 波特率是多少
3. 是否需要启动命令
4. 协议帧格式是什么
5. /dev/ttySx 能不能收到原始十六进制数据
```

能用 `cat /dev/ttySx | hexdump -C` 看到数据，后面就是协议解析问题。


--------
------
---------
不一定。**实时性高 ≠ 一定要把雷达协议解析放到内核驱动层。**

要先分清楚你说的“实时性”是哪一种：

```text
1. 不能丢串口数据
2. 数据延迟要低
3. 时间戳要准
4. 雷达数据要和电机/里程计/IMU 严格同步
5. 要做闭环控制，比如避障马上刹车
```

不同要求，方案不一样。

---

# 1. 串口收数据本身，内核已经实时处理了

雷达通过 UART 发数据时，底层流程是：

```text
雷达 TX
  ↓
X2600 UART RX FIFO
  ↓
UART 中断 / DMA
  ↓
Linux UART 驱动
  ↓
TTY 缓冲区
  ↓
/dev/ttySx
  ↓
用户态 read()
```

这里面 **UART 中断收字节** 本来就在内核驱动层完成。

也就是说，即使你应用程序晚一点 `read()`，数据也会先进入内核 TTY 缓冲区。

所以如果你的要求只是：

```text
雷达 115200 / 230400 / 460800 波特率
持续输出数据
应用程序不要丢包
解析角度距离
送给导航算法
```

那通常**应用层读串口就够了**。

---

# 2. 什么情况下应用层就够？

扫地机雷达多数是这种：

```text
雷达自己转
雷达持续吐串口数据帧
主控接收
解析距离、角度、强度
送给 SLAM / 避障 / 建图
```

这种情况下，推荐先这样做：

```text
内核层：使用已有 UART 驱动
用户层：高优先级线程 read / poll / epoll 读取 /dev/ttySx
用户层：解析雷达协议
用户层：放入环形缓冲区，给算法线程用
```

应用层可以做得很实时，比如：

```c
open("/dev/ttyS1", O_RDWR | O_NOCTTY);
设置 raw 模式；
设置波特率；
一个独立线程专门 read；
线程设置 SCHED_FIFO；
解析线程和算法线程用 ring buffer 通信；
```

---

# 3. 什么情况下才需要放到驱动层？

下面这些情况，才更适合考虑内核驱动层处理。

## 情况 1：必须打非常准的时间戳

比如你需要知道：

```text
某个雷达点到底是在几微秒前收到的
```

如果在应用层打时间戳：

```text
read() 返回时间 ≠ UART 收到这个字节的真实时间
```

因为中间有：

```text
中断处理
TTY 缓冲
进程调度
用户态 read
```

这时如果你要做严格同步，比如：

```text
雷达 + IMU + 编码器 + 电机控制
```

那可以考虑在内核 UART 中断/DMA 收包时打时间戳。

---

## 情况 2：串口速率非常高，用户态处理不过来

比如：

```text
921600
1500000
3000000
```

而且雷达持续高速输出，用户态线程经常被调度打断，导致 TTY buffer 溢出。

这时可以考虑：

```text
DMA 环形缓冲
内核层快速分帧
用户态 mmap/read 大块数据
```

---

## 情况 3：协议解析和中断/硬件动作强绑定

比如收到某个雷达危险帧后，必须马上：

```text
关电机
刹车
触发 GPIO
喂给 MCU
```

如果这是安全闭环，放应用层可能延迟不可控。
可以在驱动层或者 MCU 侧做快速保护。

---

## 情况 4：要做产品级统一设备抽象

比如公司希望雷达变成一个标准设备：

```text
/dev/lidar0
/sys/class/lidar/...
```

或者希望驱动统一输出点云、状态、错误码，那可以写一个专门的雷达字符设备驱动。

---

# 4. 但不建议一开始就写内核雷达协议驱动

原因是：

```text
内核驱动调试麻烦
内核崩了系统可能挂
printf/dmesg 不能太多
协议经常变，放用户态更好改
浮点计算/复杂算法不适合放内核
SLAM/导航算法一般本来就在用户态
```

扫地机这种雷达协议解析，一般还是：

```text
UART 驱动在内核
雷达协议解析在用户态
实时性优化靠线程优先级、缓冲区、DMA、调度策略
```

---

# 5. 推荐架构

比较靠谱的结构是：

```text
Linux UART 驱动
    ↓
/dev/ttySx
    ↓
lidar_rx_thread，实时线程
    ↓
串口字节环形缓冲区
    ↓
协议解析状态机
    ↓
一圈雷达数据 / 点云数据
    ↓
SLAM / 避障 / 路径规划
```

线程可以这样分：

```text
线程 1：串口接收线程，高优先级，只负责 read + 放 buffer
线程 2：协议解析线程，中高优先级，负责找帧头、校验、解析
线程 3：算法线程，负责建图/避障/导航
```

不要让一个线程同时做：

```text
read 串口 + printf 大量日志 + 解析 + 建图 + 写文件
```

这样容易卡。

---

# 6. 用户态也可以做实时优化

可以给接收线程设置实时优先级：

```c
#include <sched.h>

struct sched_param param;
param.sched_priority = 80;
sched_setscheduler(0, SCHED_FIFO, &param);
```

运行程序时需要 root 权限。

还可以：

```text
1. 串口设置 raw 模式
2. 关闭软件流控
3. 使用 poll/select/epoll
4. read 大一点的 buffer
5. 少 printf
6. 用环形缓冲区
7. 解析状态机不要阻塞
8. 算法线程和串口线程分开
```

串口配置类似：

```bash
stty -F /dev/ttyS1 230400 cs8 -cstopb -parenb -ixon -ixoff -crtscts raw
```

看数据：

```bash
cat /dev/ttyS1 | hexdump -C
```

---

# 7. 内核层方案有哪些？

如果后面真的确认用户态不够，可以考虑三种方案。

## 方案 A：改 UART 驱动，加低延迟/DMA优化

这个是底层优化：

```text
drivers/tty/serial/xxx_uart.c
```

目标是：

```text
减少丢字节
优化 FIFO/DMA
优化中断触发
增大 buffer
```

但它还是通过 `/dev/ttySx` 给用户态。

---

## 方案 B：写一个雷达专用字符设备驱动

比如暴露：

```text
/dev/lidar0
```

驱动内部做：

```text
接收串口数据
分帧
打时间戳
缓存完整雷达帧
read 给用户态
```

但是这个需要你直接管理 UART 硬件或者深度接入 TTY，复杂度高。

---

## 方案 C：内核只打时间戳，协议解析仍在用户态

这个比较折中：

```text
内核负责接收时间戳
用户态负责协议解析
```

适合雷达、IMU、轮速计需要同步的情况。

---

# 8. 对你这个扫地机项目，我建议这样判断

先问自己这几个问题：

```text
雷达波特率是多少？115200？230400？921600？
雷达每秒多少帧？
每帧多少字节？
现在实际有丢包吗？
允许延迟多少？10ms？5ms？1ms？
雷达数据是否要和轮速/IMU严格时间同步？
收到障碍物是否要马上控制电机停？
```

如果是普通扫地机激光雷达：

```text
5Hz / 10Hz 转速
串口 115200 / 230400 / 460800
应用层能稳定读完
允许几毫秒到十几毫秒延迟
```

那就先放用户态。

如果是：

```text
高速雷达
高频避障
严格同步
应用层经常丢帧
```

再考虑驱动层。

---

# 9. 一句话结论

**串口雷达不一定要写内核驱动。**

更常见、更好维护的做法是：

```text
内核 UART 驱动负责实时收字节；
用户态高优先级线程负责读 /dev/ttySx 和解析雷达协议。
```

只有当你需要：

```text
微秒级时间戳
极低延迟
强实时闭环
高速串口不丢包
和 IMU/编码器硬同步
```

才值得把一部分逻辑下沉到驱动层。
