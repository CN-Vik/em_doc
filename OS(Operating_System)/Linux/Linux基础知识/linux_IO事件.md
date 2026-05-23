可以把 **epoll** 理解成：

> Linux 应用层用来“同时等待很多个事件”的机制。
> 它常用于网络服务器、串口、管道、socket、驱动设备节点、timerfd、eventfd 等场景。

嵌入式 Linux 产品里，`epoll` 很常见，比如：

```text
等待 TCP 客户端数据
等待串口数据
等待按键/驱动上报事件
等待定时器超时
等待进程间通信消息
```

它的核心作用就是：**一个线程同时管理很多个 fd，不用给每个 fd 单独开线程死等。**

---

# 1. 先理解 fd 是什么

Linux 里有个很重要的思想：

> 一切皆文件。

应用程序打开很多东西时，都会得到一个 **文件描述符 fd**。

例如：

```c
int fd1 = open("/dev/ttyS1", O_RDWR);      // 串口 fd
int fd2 = socket(AF_INET, SOCK_STREAM, 0); // TCP socket fd
int fd3 = open("/dev/input/event0", O_RDONLY); // 按键/触摸 fd
```

这些 `fd1`、`fd2`、`fd3` 都可以理解成应用程序和内核之间的“通道”。

你可以对它们：

```c
read(fd, buf, len);
write(fd, buf, len);
close(fd);
```

问题是：**什么时候该 read？**

比如 TCP socket 没有数据时，你直接 `read()` 可能会阻塞。

```c
read(socket_fd, buf, len); // 没数据时可能卡在这里
```

如果你有 100 个 socket、1 个串口、1 个按键设备，总不能每个都开一个线程去阻塞等吧？

这时候就需要 `select` / `poll` / `epoll` 这类机制。

---

# 2. epoll 是干啥的？

`epoll` 的作用是：

> 让应用程序告诉内核：“帮我盯着这些 fd，一旦哪个 fd 有事件了，就通知我。”

例如你关心：

```text
socket_fd 有没有客户端连接？
client_fd 有没有网络数据？
uart_fd 有没有串口数据？
key_fd 有没有按键事件？
timer_fd 有没有定时器超时？
```

你可以把这些 fd 都交给 epoll。

然后应用程序阻塞在：

```c
epoll_wait()
```

当任何一个 fd 有事件时，`epoll_wait()` 就返回，告诉你：

```text
fd = 5 有数据可读
fd = 8 可以写了
fd = 10 连接断开了
```

所以 epoll 本质上是：

```text
多路 I/O 复用机制
```

所谓“多路”，就是多个 fd。

所谓“I/O 复用”，就是一个线程同时管理多个 I/O 通道。

---

# 3. 没有 epoll 会怎样？

假设你写一个 TCP 服务器，要同时处理 100 个客户端。

## 方案一：每个客户端一个线程

```text
客户端1 -> 线程1
客户端2 -> 线程2
客户端3 -> 线程3
...
客户端100 -> 线程100
```

缺点：

```text
线程多，内存占用大
上下文切换开销大
嵌入式 Linux 设备资源有限
线程同步复杂
```

对于小型嵌入式设备，可能不太合适。

---

## 方案二：一个线程轮询所有 fd

伪代码：

```c
while (1)
{
    read(fd1);
    read(fd2);
    read(fd3);
    read(fd4);
}
```

问题是：

```text
某个 fd 没数据，read 可能阻塞
如果设置非阻塞，又会疯狂空转，占 CPU
fd 多了效率很低
```

---

## 方案三：用 epoll

```text
一个线程
    |
    +-- epoll_wait()
            |
            +-- 有事件才醒来
            +-- 没事件就睡眠
```

优点：

```text
CPU 占用低
fd 数量多时效率高
代码结构清晰
适合网络服务器和事件驱动程序
```

---

# 4. epoll 和 MCU 中断有点像，但不是一回事

你可以类比理解：

## MCU 裸机 / RTOS

```text
外设有数据 -> 触发中断 -> ISR 执行
```

比如 UART 接收到数据：

```text
UART RX 中断 -> 进入中断函数 -> 读取数据
```

---

## Linux 应用层 epoll

```text
内核发现 fd 有数据 -> epoll_wait 返回 -> 应用层处理
```

比如 socket 收到数据：

```text
网卡中断 -> 内核协议栈处理 -> socket 可读 -> epoll_wait 返回 -> 应用 read()
```

所以 epoll 可以粗略理解成：

> 应用层的“事件等待器”。

但是注意：

```text
epoll 不是硬件中断
epoll 不运行在中断上下文
epoll 不保证微秒级实时性
epoll 是应用层事件通知机制
```

对于嵌入式 Linux 产品来说，epoll 适合做业务事件处理，不适合做强实时控制。

---

# 5. epoll 的三个核心函数

epoll 主要就三个 API：

```c
epoll_create1()
epoll_ctl()
epoll_wait()
```

---

## 5.1 epoll_create1：创建 epoll 实例

```c
int epfd = epoll_create1(0);
```

这个函数会返回一个 epoll fd。

可以理解成：

```text
创建一个事件管理器
```

例如：

```c
int epfd = epoll_create1(0);
if (epfd < 0) {
    perror("epoll_create1");
}
```

---

## 5.2 epoll_ctl：添加、修改、删除监听 fd

例如你想监听 `sockfd` 是否有数据可读：

```c
struct epoll_event ev;

ev.events = EPOLLIN;
ev.data.fd = sockfd;

epoll_ctl(epfd, EPOLL_CTL_ADD, sockfd, &ev);
```

这里的意思是：

```text
把 sockfd 加入 epoll 管理
我关心它的 EPOLLIN 事件
```

`EPOLLIN` 表示：

```text
这个 fd 有数据可读
```

常见操作有三个：

```c
EPOLL_CTL_ADD  // 添加 fd
EPOLL_CTL_MOD  // 修改 fd 监听事件
EPOLL_CTL_DEL  // 删除 fd
```

---

## 5.3 epoll_wait：等待事件发生

```c
struct epoll_event events[32];

int n = epoll_wait(epfd, events, 32, -1);
```

参数含义：

```c
epfd        // epoll 实例
events      // 用来接收发生事件的数组
32          // 最多接收 32 个事件
-1          // 一直等待，不超时
```

返回值 `n` 表示有几个 fd 发生了事件。

例如：

```c
for (int i = 0; i < n; i++) {
    int fd = events[i].data.fd;

    if (events[i].events & EPOLLIN) {
        // fd 有数据可读
        read(fd, buf, sizeof(buf));
    }
}
```

---

# 6. 常见 epoll 事件

常用事件如下：

```c
EPOLLIN   // 可读，有数据来了
EPOLLOUT  // 可写，可以发送数据
EPOLLERR  // 出错
EPOLLHUP  // 对端关闭、挂断
EPOLLET   // 边沿触发模式
EPOLLONESHOT // 只触发一次
```

嵌入式里最常用的是：

```c
EPOLLIN
EPOLLERR
EPOLLHUP
```

例如 TCP 客户端发数据：

```text
client_fd 触发 EPOLLIN
```

串口收到数据：

```text
uart_fd 触发 EPOLLIN
```

对端断开连接：

```text
client_fd 触发 EPOLLHUP / EPOLLERR / read 返回 0
```

---

# 7. 一个最小 epoll 示例

下面这个例子监听标准输入，也就是键盘输入。

你在终端输入内容后，`epoll_wait()` 就会返回。

```c
#include <stdio.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <string.h>
#include <errno.h>

int main(void)
{
    int epfd;
    struct epoll_event ev;
    struct epoll_event events[8];

    // 1. 创建 epoll 实例
    epfd = epoll_create1(0);
    if (epfd < 0) {
        perror("epoll_create1");
        return -1;
    }

    // 2. 添加标准输入 fd：0
    // fd = 0 表示 stdin，也就是键盘输入
    ev.events = EPOLLIN;
    ev.data.fd = STDIN_FILENO;

    if (epoll_ctl(epfd, EPOLL_CTL_ADD, STDIN_FILENO, &ev) < 0) {
        perror("epoll_ctl");
        close(epfd);
        return -1;
    }

    printf("等待输入...\n");

    while (1) {
        // 3. 等待事件发生
        int n = epoll_wait(epfd, events, 8, -1);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }

            perror("epoll_wait");
            break;
        }

        // 4. 处理发生事件的 fd
        for (int i = 0; i < n; i++) {
            if (events[i].data.fd == STDIN_FILENO) {
                char buf[128];

                int len = read(STDIN_FILENO, buf, sizeof(buf) - 1);
                if (len > 0) {
                    buf[len] = '\0';
                    printf("收到输入: %s", buf);
                }
            }
        }
    }

    close(epfd);
    return 0;
}
```

编译：

```bash
gcc epoll_demo.c -o epoll_demo
```

运行：

```bash
./epoll_demo
```

输入：

```text
hello
```

输出：

```text
收到输入: hello
```

---

# 8. epoll 在 TCP 服务器里的典型用法

一个 TCP 服务器大概这样：

```text
创建 server socket
bind
listen
把 server_fd 加入 epoll
while (1)
{
    epoll_wait();

    如果 server_fd 有事件：
        accept 新客户端
        把 client_fd 加入 epoll

    如果 client_fd 有事件：
        read 客户端数据
        处理数据
        write 回复
}
```

结构类似这样：

```c
while (1)
{
    int n = epoll_wait(epfd, events, MAX_EVENTS, -1);

    for (int i = 0; i < n; i++)
    {
        int fd = events[i].data.fd;

        if (fd == listen_fd)
        {
            // 有新的客户端连接
            int client_fd = accept(listen_fd, NULL, NULL);

            // 把新客户端加入 epoll
            ev.events = EPOLLIN;
            ev.data.fd = client_fd;
            epoll_ctl(epfd, EPOLL_CTL_ADD, client_fd, &ev);
        }
        else
        {
            // 某个客户端发数据来了
            int len = read(fd, buf, sizeof(buf));

            if (len > 0)
            {
                // 处理客户端数据
            }
            else if (len == 0)
            {
                // 对端关闭连接
                epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
                close(fd);
            }
            else
            {
                // 出错
                epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
                close(fd);
            }
        }
    }
}
```

这就是典型的 **Reactor 事件驱动模型**。

---

# 9. select、poll、epoll 的区别

Linux 里有三个类似机制：

```text
select
poll
epoll
```

它们都是干类似事情的：

> 同时等待多个 fd 的 I/O 事件。

但是实现方式和性能不同。

---

## 9.1 select

`select` 比较老。

缺点：

```text
fd 数量有限，通常受 FD_SETSIZE 限制
每次调用都要重新传入 fd 集合
fd 多时效率低
```

伪代码：

```c
select(maxfd + 1, &readfds, NULL, NULL, NULL);
```

适合：

```text
简单小程序
fd 数量很少的场景
```

---

## 9.2 poll

`poll` 比 `select` 好一些。

优点：

```text
没有 select 那种固定 FD_SETSIZE 限制
使用数组传 fd
```

缺点：

```text
每次调用还是要把 fd 数组传给内核
fd 很多时仍然要遍历
```

伪代码：

```c
poll(fds, nfds, timeout);
```

适合：

```text
中等规模 fd
代码兼容性要求较高的场景
```

---

## 9.3 epoll

`epoll` 是 Linux 下更适合大量 fd 的机制。

优点：

```text
fd 先注册到内核
事件发生后内核通知应用
适合大量连接
效率高
```

适合：

```text
高并发 TCP 服务器
嵌入式网关
IPC 事件循环
多设备节点监听
```

简单对比：

| 机制     | 特点           | 适合场景           |
| ------ | ------------ | -------------- |
| select | 老，fd 数量有限    | 小程序            |
| poll   | 比 select 灵活  | 中小规模 fd        |
| epoll  | Linux 高效事件通知 | 大量 fd、服务器、事件循环 |

---

# 10. epoll 的两种触发模式

epoll 有两种重要模式：

```text
LT：Level Trigger，水平触发
ET：Edge Trigger，边沿触发
```

---

## 10.1 LT 水平触发

默认就是 LT。

特点：

> 只要 fd 里还有数据没读完，epoll_wait 每次都会提醒你。

例如 socket 收到 100 字节，你只读了 20 字节。

那么剩下还有 80 字节，下一次 `epoll_wait()` 还会继续返回这个 fd。

优点：

```text
安全
简单
不容易漏事件
适合新手
```

缺点：

```text
性能略低
事件可能重复提醒
```

建议新手和普通嵌入式项目先用 LT。

---

## 10.2 ET 边沿触发

使用方式：

```c
ev.events = EPOLLIN | EPOLLET;
```

特点：

> 只有状态变化的瞬间通知一次。

例如：

```text
原来没数据 -> 现在有数据
```

这时通知一次。

但是如果你这次没有把数据全部读完，后面可能不会再提醒。

所以 ET 模式下必须配合：

```text
非阻塞 fd
while 循环一直读到 EAGAIN
```

典型写法：

```c
while (1)
{
    int len = read(fd, buf, sizeof(buf));

    if (len > 0)
    {
        // 处理数据
    }
    else if (len == 0)
    {
        // 对端关闭
        close(fd);
        break;
    }
    else
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            // 数据读完了
            break;
        }
        else
        {
            // 真正出错
            close(fd);
            break;
        }
    }
}
```

ET 优点：

```text
性能高
减少重复通知
```

ET 缺点：

```text
代码复杂
容易漏读数据
新手容易写出 bug
```

所以建议：

```text
刚开始用 epoll，先用 LT。
等理解清楚非阻塞 I/O 后，再用 ET。
```

---

# 11. epoll 和阻塞/非阻塞的关系

fd 可以分为：

```text
阻塞模式
非阻塞模式
```

默认很多 fd 是阻塞的。

---

## 阻塞模式

```c
read(fd, buf, len);
```

如果没有数据，程序可能卡住。

---

## 非阻塞模式

设置方式：

```c
#include <fcntl.h>

int flags = fcntl(fd, F_GETFL, 0);
fcntl(fd, F_SETFL, flags | O_NONBLOCK);
```

没有数据时，`read()` 不会卡住，而是返回 -1，并设置：

```c
errno = EAGAIN
```

在 epoll 里，尤其是 ET 模式，通常要求 fd 设置为非阻塞。

---

# 12. 嵌入式 Linux 里 epoll 常见使用场景

## 12.1 TCP/UDP 网络通信

例如智能网关、摄像头、音箱、IoT 设备：

```text
手机 App 连接设备
设备接收控制命令
设备上传状态
设备处理多个客户端连接
```

这类场景很适合 epoll。

---

## 12.2 串口数据接收

例如 Linux 主控和 MCU 通信：

```text
/dev/ttyS1
/dev/ttyUSB0
/dev/ttyACM0
```

可以把串口 fd 加入 epoll：

```c
ev.events = EPOLLIN;
ev.data.fd = uart_fd;
epoll_ctl(epfd, EPOLL_CTL_ADD, uart_fd, &ev);
```

串口收到数据后：

```text
epoll_wait 返回
read(uart_fd, ...)
解析协议包
```

这种比单独一个线程一直阻塞读串口更适合统一事件管理。

---

## 12.3 input 子系统按键/触摸

比如：

```text
/dev/input/event0
/dev/input/event1
```

这些设备也可以 epoll 监听。

```text
按键按下 -> input 事件 -> fd 可读 -> epoll_wait 返回
```

然后应用读取：

```c
struct input_event ev;
read(input_fd, &ev, sizeof(ev));
```

---

## 12.4 timerfd 定时器

Linux 有一种 fd 形式的定时器：`timerfd`。

你可以把定时器也加入 epoll。

这样主循环里可以同时处理：

```text
网络事件
串口事件
按键事件
定时器事件
```

而不是到处写线程和 sleep。

---

## 12.5 eventfd 线程间通知

`eventfd` 可以用于线程之间通知。

比如一个工作线程处理完任务后，通知主 epoll 线程：

```text
worker thread -> write(eventfd)
main thread -> epoll_wait 返回
```

这样可以把多线程事件统一进 epoll 主循环。

---

## 12.6 自定义驱动设备节点

如果你自己写 Linux 字符设备驱动，比如：

```text
/dev/my_sensor
/dev/my_key
/dev/my_imu
```

你希望应用层可以用 epoll 监听它，那么驱动里通常要实现：

```c
.poll = xxx_poll
```

驱动层要配合：

```c
poll_wait()
wake_up_interruptible()
```

逻辑类似：

```text
应用 epoll_wait 睡眠
驱动中断来了
驱动唤醒等待队列
epoll_wait 返回
应用 read 数据
```

这在 Linux 驱动开发里非常重要。

---

# 13. epoll 不能监听普通文件吗？

严格说，普通文件 fd 可以放进去，但通常没意义。

例如：

```c
open("test.txt", O_RDONLY);
```

普通磁盘文件通常一直被认为“可读”或“可写”，不会像 socket、串口那样等外部事件。

所以 epoll 常用于：

```text
socket
pipe
fifo
tty
eventfd
timerfd
signalfd
inotify fd
input event fd
支持 poll 的字符设备
```

不太适合：

```text
普通文本文件
普通配置文件
```

---

# 14. 一个嵌入式产品里的典型事件循环

假设你做一个嵌入式 Linux 设备：

```text
TCP socket：接收手机 App 命令
UART：和下位机 MCU 通信
input：按键
timerfd：周期任务
eventfd：线程通知
```

可以设计成：

```text
main()
{
    初始化网络
    初始化串口
    初始化按键
    初始化 timerfd
    初始化 epoll

    把所有 fd 加入 epoll

    while (1)
    {
        epoll_wait();

        如果是网络 fd:
            处理 App 命令

        如果是串口 fd:
            读取 MCU 数据

        如果是按键 fd:
            处理按键事件

        如果是 timerfd:
            执行周期任务

        如果是 eventfd:
            处理线程通知
    }
}
```

这种结构非常适合嵌入式 Linux 应用。

---

# 15. epoll 和多线程的关系

epoll 不等于多线程。

epoll 是：

```text
一个线程管理多个 fd
```

多线程是：

```text
多个线程并行执行任务
```

实际项目可以组合使用。

例如：

```text
主线程：epoll 事件循环
工作线程：处理耗时任务，比如 AI 推理、图像处理、文件写入
```

主线程不要做太耗时的事情，否则会影响事件响应。

推荐结构：

```text
epoll 主线程
    |
    +-- 负责接收事件
    +-- 负责分发任务
    +-- 不做耗时处理

worker 线程
    |
    +-- 做复杂计算
    +-- 处理完成后用 eventfd 通知主线程
```

---

# 16. epoll 常见坑

## 16.1 忘记处理 EPOLLERR / EPOLLHUP

很多人只写：

```c
if (events[i].events & EPOLLIN)
```

但是实际项目必须处理错误和断开。

建议：

```c
if (events[i].events & (EPOLLERR | EPOLLHUP)) {
    epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
    close(fd);
    continue;
}
```

---

## 16.2 read 返回 0 没处理

TCP socket 中：

```c
read(fd, buf, len)
```

如果返回 0，通常表示：

```text
对端关闭连接
```

需要：

```c
epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
close(fd);
```

否则容易 fd 泄漏。

---

## 16.3 ET 模式没有读到 EAGAIN

ET 模式下，如果你只读一次：

```c
read(fd, buf, sizeof(buf));
```

可能会漏数据。

正确思路是：

```c
while (1) {
    read();
    直到 errno == EAGAIN
}
```

---

## 16.4 没有设置非阻塞

ET 模式通常必须配合：

```c
O_NONBLOCK
```

否则可能某次 `read()` 卡住，整个事件循环都停了。

---

## 16.5 忘记从 epoll 删除 fd

关闭 fd 前建议：

```c
epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
close(fd);
```

尤其是复杂项目里，最好显式删除，避免逻辑混乱。

---

## 16.6 EPOLLOUT 不能一直监听

`EPOLLOUT` 表示 fd 可写。

大多数 socket 大部分时间都是可写的。

如果你一直监听：

```c
EPOLLIN | EPOLLOUT
```

可能导致 `epoll_wait()` 频繁返回，CPU 占用升高。

正确做法通常是：

```text
平时只监听 EPOLLIN
只有发送缓冲区有待发送数据时，才临时监听 EPOLLOUT
发送完后，取消 EPOLLOUT
```

---

# 17. epoll 和实时性

epoll 很适合高效等待事件，但它不是硬实时机制。

它受这些因素影响：

```text
Linux 调度延迟
线程优先级
系统负载
中断处理
驱动实现
CPU 占用
是否有其他高优先级任务
```

所以如果你要做：

```text
电机 PWM 精准控制
微秒级采样
高速实时闭环
严格实时协议
```

不要依赖用户态 epoll。

更合适的是：

```text
MCU
内核驱动
RT Linux
专用协处理器
硬件定时器
DMA
```

但如果是：

```text
网络通信
业务控制
串口协议
UI 事件
按键输入
低频传感器事件
```

epoll 非常合适。

---

# 18. 总结一句话

`epoll` 就是 Linux 应用层的高效事件等待机制。

它解决的问题是：

```text
一个程序要同时等待很多 fd 的事件，怎么高效、不浪费 CPU、不乱开线程？
```

它的基本流程是：

```text
epoll_create1() 创建 epoll
epoll_ctl() 添加 fd
epoll_wait() 等待事件
read/write/accept 处理事件
```

对于嵌入式 Linux 产品开发，你可以把它理解成：

> 应用层主循环的“事件中枢”。
> 网络、串口、按键、定时器、驱动上报，都可以统一放到 epoll 里管理。


-------
------
-------
`select`、`poll`、`epoll` 这三个东西，本质上都是：

> **I/O 多路复用机制**
> 让一个线程同时等待多个 fd 的事件。

也就是说，它们解决的是同一个问题：

```text
一个程序里有很多 fd：
    TCP socket
    UART 串口
    input 按键
    pipe 管道
    timerfd 定时器
    eventfd 线程通知
    驱动设备节点 /dev/xxx

我怎么知道哪个 fd 有数据可以读？
我总不能每个 fd 开一个线程一直 read 吧？
```

所以 `select`、`poll`、`epoll` 的作用就是：

```text
把一堆 fd 交给内核
让内核帮你等
哪个 fd 有事件了，内核通知应用程序
应用程序再去 read/write/accept
```

---

# 1. 先理解“等待 fd 事件”是什么意思

比如你打开一个串口：

```c
int uart_fd = open("/dev/ttyS1", O_RDWR);
```

然后你想读取数据：

```c
read(uart_fd, buf, sizeof(buf));
```

如果没有数据，`read()` 可能会阻塞在那里。

再比如 TCP 服务器：

```c
int client_fd = accept(server_fd, NULL, NULL);
```

某个客户端有没有发数据，你也不知道。

如果你有很多 fd：

```text
fd 3：TCP 监听 socket
fd 4：客户端 1
fd 5：客户端 2
fd 6：串口
fd 7：按键
fd 8：timerfd
```

你希望程序变成这样：

```c
while (1)
{
    等待所有 fd;

    如果 fd3 有事件：
        accept 新客户端

    如果 fd4 有事件：
        read 客户端1数据

    如果 fd6 有事件：
        read 串口数据

    如果 fd7 有事件：
        read 按键事件

    如果 fd8 有事件：
        执行周期任务
}
```

这就是 `select` / `poll` / `epoll` 的使用场景。

---

# 2. select

`select` 是最老的一种 I/O 多路复用接口。

它的函数原型大概是：

```c
#include <sys/select.h>

int select(
    int nfds,
    fd_set *readfds,
    fd_set *writefds,
    fd_set *exceptfds,
    struct timeval *timeout
);
```

---

## 2.1 select 可以监听什么事件？

主要监听三类：

```text
readfds    关心哪些 fd 可读
writefds   关心哪些 fd 可写
exceptfds  关心哪些 fd 有异常
```

最常用的是 `readfds`。

例如：

```text
socket 有数据可读
server socket 有新连接可 accept
串口有数据可读
按键设备有 input_event 可读
```

---

## 2.2 select 的核心数据结构：fd_set

`select` 使用 `fd_set` 来表示一组 fd。

常用宏：

```c
FD_ZERO(&set);      // 清空 fd 集合
FD_SET(fd, &set);   // 把 fd 加入集合
FD_CLR(fd, &set);   // 从集合删除 fd
FD_ISSET(fd, &set); // 判断 fd 是否在集合中
```

示例：

```c
fd_set readfds;

FD_ZERO(&readfds);
FD_SET(uart_fd, &readfds);
FD_SET(sock_fd, &readfds);
```

---

## 2.3 select 的 nfds 参数是什么意思？

`select` 第一个参数叫 `nfds`。

它不是 fd 数量，而是：

```text
最大 fd 值 + 1
```

比如你监听这些 fd：

```text
3, 4, 8, 10
```

那么：

```c
nfds = 10 + 1;
```

如果写错了，比如最大 fd 是 10，你传 8，那么 fd 10 就不会被监听。

---

## 2.4 select 的 timeout

最后一个参数是超时时间：

```c
struct timeval timeout;

timeout.tv_sec = 5;
timeout.tv_usec = 0;
```

含义是最多等 5 秒。

常见写法：

```c
select(maxfd + 1, &readfds, NULL, NULL, NULL);
```

最后传 `NULL` 表示：

```text
一直阻塞等待，直到有 fd 发生事件
```

如果传：

```c
timeout.tv_sec = 0;
timeout.tv_usec = 0;
```

表示：

```text
立即返回，不阻塞
```

---

## 2.5 select 最小示例：监听标准输入

```c
#include <stdio.h>
#include <unistd.h>
#include <sys/select.h>

int main(void)
{
    fd_set readfds;
    char buf[128];

    while (1)
    {
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);

        printf("select 等待输入...\n");

        int ret = select(STDIN_FILENO + 1, &readfds, NULL, NULL, NULL);
        if (ret < 0)
        {
            perror("select");
            break;
        }

        if (FD_ISSET(STDIN_FILENO, &readfds))
        {
            int len = read(STDIN_FILENO, buf, sizeof(buf) - 1);
            if (len > 0)
            {
                buf[len] = '\0';
                printf("收到输入: %s", buf);
            }
        }
    }

    return 0;
}
```

运行后，你在终端输入内容，`select()` 就会返回。

---

## 2.6 select 的重要特点

`select` 每次调用前，都要重新设置 fd 集合：

```c
FD_ZERO(&readfds);
FD_SET(fd1, &readfds);
FD_SET(fd2, &readfds);
select(...);
```

因为 `select()` 返回后，会修改你传进去的 `fd_set`。

也就是说：

```text
调用前：
    readfds 里有 fd1 fd2 fd3

调用后：
    readfds 里只剩下真正有事件的 fd
```

所以每轮循环都要重新 `FD_ZERO`、`FD_SET`。

---

## 2.7 select 的缺点

### 缺点 1：fd 数量有限

很多系统里 `select` 受 `FD_SETSIZE` 限制，常见是 1024。

也就是说，fd 太大或者 fd 太多，会有问题。

### 缺点 2：每次都要重新传 fd 集合

每次调用 `select()`，都要把 fd 集合从用户态拷贝到内核态。

返回时，内核还要把结果拷贝回用户态。

### 缺点 3：需要遍历判断哪个 fd 有事件

`select()` 只告诉你：

```text
有 n 个 fd 发生事件
```

但它不会直接告诉你是哪几个。

你还得自己遍历：

```c
for (int fd = 0; fd <= maxfd; fd++)
{
    if (FD_ISSET(fd, &readfds))
    {
        // 这个 fd 有事件
    }
}
```

fd 多了以后效率低。

---

# 3. poll

`poll` 是对 `select` 的改进。

函数原型：

```c
#include <poll.h>

int poll(
    struct pollfd *fds,
    nfds_t nfds,
    int timeout
);
```

它不再使用 `fd_set`，而是使用结构体数组。

---

## 3.1 poll 的核心结构体

```c
struct pollfd {
    int fd;         // 要监听的 fd
    short events;   // 你关心的事件
    short revents;  // 实际发生的事件
};
```

例如：

```c
struct pollfd fds[2];

fds[0].fd = uart_fd;
fds[0].events = POLLIN;

fds[1].fd = sock_fd;
fds[1].events = POLLIN;
```

其中：

```text
events   是你告诉内核：我关心什么事件
revents  是内核告诉你：实际发生了什么事件
```

---

## 3.2 poll 常见事件

常用事件：

```c
POLLIN    // 有数据可读
POLLOUT   // 可以写
POLLERR   // 出错
POLLHUP   // 挂断
POLLNVAL  // fd 无效
```

最常用的是：

```c
POLLIN
POLLERR
POLLHUP
```

---

## 3.3 poll 的 timeout

`poll` 的 timeout 是毫秒单位。

```c
poll(fds, nfds, -1);   // 一直等待
poll(fds, nfds, 0);    // 立即返回
poll(fds, nfds, 1000); // 最多等 1000 ms
```

这个比 `select` 的 `struct timeval` 更简单。

---

## 3.4 poll 最小示例：监听标准输入

```c
#include <stdio.h>
#include <unistd.h>
#include <poll.h>

int main(void)
{
    struct pollfd fds[1];
    char buf[128];

    fds[0].fd = STDIN_FILENO;
    fds[0].events = POLLIN;

    while (1)
    {
        printf("poll 等待输入...\n");

        int ret = poll(fds, 1, -1);
        if (ret < 0)
        {
            perror("poll");
            break;
        }

        if (fds[0].revents & POLLIN)
        {
            int len = read(STDIN_FILENO, buf, sizeof(buf) - 1);
            if (len > 0)
            {
                buf[len] = '\0';
                printf("收到输入: %s", buf);
            }
        }
    }

    return 0;
}
```

---

## 3.5 poll 相比 select 的优点

### 优点 1：没有 fd_set 那种固定大小限制

`poll` 使用数组：

```c
struct pollfd fds[1000];
```

理论上你可以放很多 fd，不像 `select` 那样受 `FD_SETSIZE` 明显限制。

当然，fd 太多也会慢。

### 优点 2：不需要计算 maxfd + 1

`select` 要传：

```c
maxfd + 1
```

`poll` 直接传数组长度：

```c
poll(fds, nfds, timeout);
```

更直观。

### 优点 3：events 和 revents 分离

`poll` 里：

```text
events  是输入
revents 是输出
```

所以不像 `select` 那样每次都要重新构造 fd 集合。

不过如果 fd 列表有变化，还是要维护这个数组。

---

## 3.6 poll 的缺点

### 缺点 1：每次还是要把 fd 数组传给内核

每次调用：

```c
poll(fds, nfds, timeout);
```

内核都要检查这个数组。

### 缺点 2：fd 多时仍然要线性遍历

假设你有 10000 个 fd，只有 1 个有数据。

`poll` 仍然可能需要扫描很多 fd。

应用层也要遍历：

```c
for (int i = 0; i < nfds; i++)
{
    if (fds[i].revents & POLLIN)
    {
        // 处理
    }
}
```

### 缺点 3：大量连接场景效率不如 epoll

所以 `poll` 比 `select` 好，但在高并发大量 fd 场景下，还是不如 `epoll`。

---

# 4. epoll

`epoll` 是 Linux 特有的高性能 I/O 多路复用机制。

它常用于：

```text
高并发 TCP 服务器
嵌入式 Linux 网关
多 socket 通信
多个设备节点统一事件循环
```

`epoll` 主要有三个函数：

```c
epoll_create1()
epoll_ctl()
epoll_wait()
```

---

# 4.1 epoll_create1：创建 epoll 实例

```c
#include <sys/epoll.h>

int epfd = epoll_create1(0);
```

可以理解成：

```text
创建一个内核里的事件管理器
```

这个 `epfd` 也是一个 fd，最后也要 `close(epfd)`。

---

# 4.2 epoll_ctl：添加、修改、删除 fd

函数原型：

```c
int epoll_ctl(
    int epfd,
    int op,
    int fd,
    struct epoll_event *event
);
```

`op` 有三个常用值：

```c
EPOLL_CTL_ADD  // 添加 fd
EPOLL_CTL_MOD  // 修改 fd 监听事件
EPOLL_CTL_DEL  // 删除 fd
```

例如监听一个 socket 是否可读：

```c
struct epoll_event ev;

ev.events = EPOLLIN;
ev.data.fd = sock_fd;

epoll_ctl(epfd, EPOLL_CTL_ADD, sock_fd, &ev);
```

意思是：

```text
把 sock_fd 加入 epoll
我关心它的 EPOLLIN 事件
```

---

# 4.3 epoll_wait：等待事件

函数原型：

```c
int epoll_wait(
    int epfd,
    struct epoll_event *events,
    int maxevents,
    int timeout
);
```

例如：

```c
struct epoll_event events[32];

int n = epoll_wait(epfd, events, 32, -1);
```

含义：

```text
最多接收 32 个事件
一直等待，直到有事件发生
返回值 n 表示实际发生了几个事件
```

然后处理：

```c
for (int i = 0; i < n; i++)
{
    int fd = events[i].data.fd;

    if (events[i].events & EPOLLIN)
    {
        read(fd, buf, sizeof(buf));
    }
}
```

---

# 4.4 epoll 常见事件

常用事件：

```c
EPOLLIN      // 可读
EPOLLOUT     // 可写
EPOLLERR     // 出错
EPOLLHUP     // 挂断
EPOLLRDHUP   // 对端关闭写方向，TCP 常用
EPOLLET      // 边沿触发
EPOLLONESHOT // 事件只触发一次
```

普通嵌入式应用最常用：

```c
EPOLLIN | EPOLLERR | EPOLLHUP
```

TCP 服务器里经常还会加：

```c
EPOLLRDHUP
```

---

# 4.5 epoll 最小示例：监听标准输入

```c
#include <stdio.h>
#include <unistd.h>
#include <sys/epoll.h>

int main(void)
{
    int epfd;
    struct epoll_event ev;
    struct epoll_event events[8];
    char buf[128];

    epfd = epoll_create1(0);
    if (epfd < 0)
    {
        perror("epoll_create1");
        return -1;
    }

    ev.events = EPOLLIN;
    ev.data.fd = STDIN_FILENO;

    if (epoll_ctl(epfd, EPOLL_CTL_ADD, STDIN_FILENO, &ev) < 0)
    {
        perror("epoll_ctl");
        close(epfd);
        return -1;
    }

    while (1)
    {
        printf("epoll 等待输入...\n");

        int n = epoll_wait(epfd, events, 8, -1);
        if (n < 0)
        {
            perror("epoll_wait");
            break;
        }

        for (int i = 0; i < n; i++)
        {
            if (events[i].data.fd == STDIN_FILENO)
            {
                int len = read(STDIN_FILENO, buf, sizeof(buf) - 1);
                if (len > 0)
                {
                    buf[len] = '\0';
                    printf("收到输入: %s", buf);
                }
            }
        }
    }

    close(epfd);
    return 0;
}
```

---

# 5. 三者使用方式对比

## select 的使用模型

```c
while (1)
{
    FD_ZERO(&readfds);
    FD_SET(fd1, &readfds);
    FD_SET(fd2, &readfds);
    FD_SET(fd3, &readfds);

    select(maxfd + 1, &readfds, NULL, NULL, NULL);

    if (FD_ISSET(fd1, &readfds)) {
        // 处理 fd1
    }

    if (FD_ISSET(fd2, &readfds)) {
        // 处理 fd2
    }

    if (FD_ISSET(fd3, &readfds)) {
        // 处理 fd3
    }
}
```

特点：

```text
每次都要重新 FD_SET
每次都要遍历判断
```

---

## poll 的使用模型

```c
struct pollfd fds[3];

fds[0].fd = fd1;
fds[0].events = POLLIN;

fds[1].fd = fd2;
fds[1].events = POLLIN;

fds[2].fd = fd3;
fds[2].events = POLLIN;

while (1)
{
    poll(fds, 3, -1);

    for (int i = 0; i < 3; i++)
    {
        if (fds[i].revents & POLLIN)
        {
            // 处理 fds[i].fd
        }
    }
}
```

特点：

```text
用数组管理 fd
比 select 清晰
但仍然要遍历数组
```

---

## epoll 的使用模型

```c
int epfd = epoll_create1(0);

epoll_ctl(epfd, EPOLL_CTL_ADD, fd1, &ev1);
epoll_ctl(epfd, EPOLL_CTL_ADD, fd2, &ev2);
epoll_ctl(epfd, EPOLL_CTL_ADD, fd3, &ev3);

while (1)
{
    int n = epoll_wait(epfd, events, MAX_EVENTS, -1);

    for (int i = 0; i < n; i++)
    {
        int fd = events[i].data.fd;
        // 处理这个发生事件的 fd
    }
}
```

特点：

```text
fd 先注册到内核
epoll_wait 直接返回发生事件的 fd
不用每次传全部 fd
适合大量 fd
```

---

# 6. 它们底层思想的区别

## 6.1 select 的思想

每次调用 `select`，应用程序都把 fd 集合告诉内核：

```text
应用：内核，你帮我看看 fd 3、4、5、6 有没有事件
内核：我检查一下
内核：fd 4 有数据
应用：我自己再遍历判断一下是谁
```

下一轮又要重新来一次。

---

## 6.2 poll 的思想

`poll` 和 `select` 类似，只是数据结构换成数组：

```text
应用：内核，这是一个 fd 数组，你帮我检查一下
内核：我检查整个数组
内核：把结果写到 revents
应用：我遍历数组，看哪个 revents 非 0
```

它比 `select` 接口更好用，但核心还是“每次检查一批 fd”。

---

## 6.3 epoll 的思想

`epoll` 不一样。

它先把 fd 注册到内核：

```text
应用：内核，以后你长期帮我盯着 fd 3、4、5、6
```

后面应用只需要：

```text
应用：内核，有事件了吗？
内核：有，fd 4 和 fd 6 有事件
```

所以 `epoll_wait()` 返回的就是已经发生事件的 fd 列表。

这就是 `epoll` 高效的主要原因。

---

# 7. 性能差异

简单理解：

| 机制     | 每次是否要传全部 fd | 返回后是否要遍历全部 fd | 适合 fd 数量 |
| ------ | ----------: | ------------: | -------: |
| select |           是 |             是 |       少量 |
| poll   |           是 |             是 |    少量/中等 |
| epoll  |       否，先注册 |     否，只遍历活跃事件 |       大量 |

举例：

假设有 10000 个 socket，只有 5 个有数据。

`select` / `poll` 的感觉是：

```text
每次都检查 10000 个
然后你再从 10000 个里面找出 5 个
```

`epoll` 的感觉是：

```text
内核直接告诉你这 5 个
```

所以大量 fd 时，`epoll` 优势明显。

---

# 8. select、poll、epoll 的完整对比

| 项目          | select            | poll        | epoll        |
| ----------- | ----------------- | ----------- | ------------ |
| 出现时间        | 最早                | 后来改进        | Linux 高性能机制  |
| 跨平台         | 好                 | 好           | Linux 专有     |
| 数据结构        | fd_set            | pollfd 数组   | epoll 实例     |
| fd 数量限制     | 通常受 FD_SETSIZE 限制 | 理论限制少       | 支持大量 fd      |
| 是否每次传全部 fd  | 是                 | 是           | 否            |
| 是否每次遍历全部 fd | 是                 | 是           | 通常只遍历活跃事件    |
| 使用复杂度       | 中等                | 比 select 清晰 | 稍复杂          |
| 高并发性能       | 差                 | 一般          | 好            |
| 适合场景        | 小程序               | 中小程序        | 服务器/网关/大量 fd |
| 是否 Linux 专有 | 否                 | 否           | 是            |

---

# 9. 在嵌入式 Linux 中怎么选？

## 情况 1：只是简单等几个 fd

例如：

```text
一个串口
一个 socket
一个按键设备
```

用 `poll` 就挺好。

`poll` 代码比 `select` 清晰。

---

## 情况 2：你要写 TCP 服务器

例如：

```text
设备作为 TCP server
手机 App 多个客户端连接
后台服务通信
局域网控制
```

建议直接用 `epoll`。

---

## 情况 3：你要做统一事件循环

例如：

```text
网络 socket
串口 fd
timerfd
eventfd
input event
自定义驱动 fd
```

也建议用 `epoll`。

嵌入式 Linux 产品里可以设计成：

```text
main loop:
    epoll_wait()

    socket 事件
    串口事件
    按键事件
    定时器事件
    线程通知事件
```

这种架构很舒服。

---

## 情况 4：代码要兼容非 Linux 系统

比如你的代码可能跑在：

```text
Linux
Unix
BSD
macOS
某些 RTOS 的 POSIX 层
```

那 `select` / `poll` 更通用。

因为 `epoll` 是 Linux 专有。

---

# 10. LT 和 ET：epoll 最重要的两个模式

`select` 和 `poll` 基本是水平触发思想。

`epoll` 有两种模式：

```text
LT：Level Trigger，水平触发
ET：Edge Trigger，边沿触发
```

---

## 10.1 LT 水平触发

默认就是 LT。

特点：

> 只要 fd 里还有数据没读完，epoll_wait 就会继续提醒你。

比如 socket 收到 100 字节。

你只读了 20 字节：

```text
socket 缓冲区还剩 80 字节
```

下次 `epoll_wait()` 还会返回这个 fd。

优点：

```text
安全
简单
不容易漏事件
适合新手
```

建议你刚开始用 `epoll`，先用 LT。

---

## 10.2 ET 边沿触发

使用方式：

```c
ev.events = EPOLLIN | EPOLLET;
```

特点：

> 只有状态变化时通知一次。

比如：

```text
原来没有数据
现在来了数据
通知一次
```

但是如果你这次没有把数据读完，后面可能不会再通知。

所以 ET 必须配合：

```text
非阻塞 fd
循环 read，直到 EAGAIN
```

典型写法：

```c
while (1)
{
    int len = read(fd, buf, sizeof(buf));

    if (len > 0)
    {
        // 处理数据
    }
    else if (len == 0)
    {
        // 对端关闭
        close(fd);
        break;
    }
    else
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            // 数据读完了
            break;
        }
        else
        {
            // 真正错误
            close(fd);
            break;
        }
    }
}
```

新手最容易在 ET 模式踩坑：

```text
只 read 一次
没有读到 EAGAIN
导致后续数据卡在内核缓冲区里
epoll 不再提醒
程序像“死了一样”
```

---

# 11. 阻塞和非阻塞的关系

这三个机制都经常和非阻塞 fd 配合使用。

设置非阻塞：

```c
#include <fcntl.h>

int flags = fcntl(fd, F_GETFL, 0);
fcntl(fd, F_SETFL, flags | O_NONBLOCK);
```

非阻塞后：

```c
read(fd, buf, len);
```

如果没有数据，不会卡死，而是返回 `-1`，并且：

```c
errno == EAGAIN || errno == EWOULDBLOCK
```

对于 `epoll ET` 模式，非阻塞几乎是必须的。

对于 `select` / `poll` / `epoll LT`，也建议很多 socket 使用非阻塞，防止某次 `read` / `write` 把整个事件循环卡住。

---

# 12. TCP 服务器中三者的区别

假设你写 TCP server：

```text
listen_fd 负责监听新连接
client_fd 负责收发数据
```

---

## 12.1 select 写法思路

```text
每轮循环：
    清空 fd_set
    加入 listen_fd
    加入所有 client_fd
    select 等待
    判断 listen_fd 是否有事件
    遍历所有 client_fd，看谁有事件
```

问题：

```text
客户端越多，遍历越慢
每轮都要重新 FD_SET
fd 数量受限制
```

---

## 12.2 poll 写法思路

```text
维护 pollfd 数组
fds[0] = listen_fd
fds[1..n] = client_fd

每轮：
    poll(fds)
    遍历 fds 数组
    看 revents
```

比 `select` 清晰，但仍然要遍历数组。

---

## 12.3 epoll 写法思路

```text
epoll_ctl 添加 listen_fd

每轮：
    epoll_wait 返回活跃 fd

如果是 listen_fd：
    accept 新连接
    epoll_ctl 添加 client_fd

如果是 client_fd：
    read 数据
```

它不会让你每轮扫描所有客户端。

---

# 13. 普通文件能不能用它们监听？

比如：

```c
int fd = open("test.txt", O_RDONLY);
```

普通文件通常不适合用 `select` / `poll` / `epoll` 等事件机制。

因为普通磁盘文件通常一直被认为是：

```text
可读
可写
```

它不像 socket、串口、管道那样有“等外部事件”的意义。

常见适合监听的 fd：

```text
socket
pipe
FIFO
串口 tty
/dev/input/eventX
timerfd
eventfd
signalfd
inotify fd
支持 poll 的字符设备驱动
```

---

# 14. 驱动和 poll/epoll 的关系

这个对嵌入式 Linux 很重要。

如果你自己写一个字符设备驱动：

```text
/dev/my_key
/dev/my_sensor
/dev/my_imu
```

应用层希望这样用：

```c
poll(fd, ...);
epoll_wait(...);
```

那驱动里通常要实现：

```c
.poll = my_drv_poll
```

驱动内部一般配合：

```c
poll_wait()
wake_up_interruptible()
```

大概逻辑：

```text
应用层调用 poll/epoll_wait
    ↓
内核进入驱动的 .poll
    ↓
如果当前没数据，把进程挂到等待队列
    ↓
硬件中断来了，驱动收到数据
    ↓
驱动 wake_up_interruptible()
    ↓
poll/epoll_wait 返回
    ↓
应用层 read()
```

所以对于嵌入式 Linux 驱动开发来说：

> `.poll` 是让用户态可以用 select/poll/epoll 等待设备事件的关键接口。

---

# 15. 常见坑

## 15.1 select 每次都要重新 FD_SET

错误理解：

```c
FD_SET(fd, &readfds);

while (1)
{
    select(..., &readfds, ...);
}
```

这是不可靠的。

因为 `select` 会修改 `readfds`。

正确方式：

```c
while (1)
{
    FD_ZERO(&readfds);
    FD_SET(fd, &readfds);

    select(..., &readfds, ...);
}
```

---

## 15.2 select 的 nfds 写错

错误：

```c
select(fd, &readfds, NULL, NULL, NULL);
```

正确：

```c
select(fd + 1, &readfds, NULL, NULL, NULL);
```

如果监听多个 fd：

```c
int maxfd = fd1;

if (fd2 > maxfd) maxfd = fd2;
if (fd3 > maxfd) maxfd = fd3;

select(maxfd + 1, &readfds, NULL, NULL, NULL);
```

---

## 15.3 poll 忘记检查 revents

`events` 是你关心的事件。

`revents` 是实际发生的事件。

应该检查：

```c
if (fds[i].revents & POLLIN)
```

不是：

```c
if (fds[i].events & POLLIN)
```

---

## 15.4 epoll 忘记处理断开

TCP 中 `read()` 返回 0 通常表示对端关闭。

```c
int len = read(fd, buf, sizeof(buf));

if (len == 0)
{
    epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
    close(fd);
}
```

不能不处理，否则 fd 会泄漏。

---

## 15.5 一直监听 EPOLLOUT / POLLOUT

很多人会这样写：

```c
ev.events = EPOLLIN | EPOLLOUT;
```

问题是：

```text
大多数 socket 大多数时候都是可写的
```

所以 `epoll_wait()` 可能频繁返回，导致 CPU 占用高。

正确思路：

```text
平时只监听 EPOLLIN
只有发送缓冲区里有待发送数据时，才监听 EPOLLOUT
发送完后，再取消 EPOLLOUT
```

---

## 15.6 epoll ET 模式没读干净

ET 模式下不能只读一次：

```c
read(fd, buf, sizeof(buf));
```

而要一直读到：

```c
errno == EAGAIN
```

否则可能漏事件。

---

## 15.7 忘记处理 EINTR

`select` / `poll` / `epoll_wait` 都可能被信号打断，返回 `-1`，并且：

```c
errno == EINTR
```

常见写法：

```c
int ret = epoll_wait(epfd, events, MAX_EVENTS, -1);

if (ret < 0)
{
    if (errno == EINTR)
    {
        continue;
    }

    perror("epoll_wait");
    break;
}
```

---

# 16. 嵌入式 Linux 产品里的典型架构

假设你做一个智能设备：

```text
TCP socket：手机 App 控制
UART：和 MCU 通信
/dev/input/event0：按键
timerfd：周期任务
eventfd：工作线程通知主线程
```

可以这样设计：

```text
主线程：
    创建 epoll
    添加 tcp_server_fd
    添加 uart_fd
    添加 key_fd
    添加 timer_fd
    添加 event_fd

    while (1)
    {
        epoll_wait();

        如果是 tcp_server_fd：
            accept 新客户端

        如果是 client_fd：
            read App 命令

        如果是 uart_fd：
            read MCU 数据

        如果是 key_fd：
            read 按键事件

        如果是 timer_fd：
            做周期检查

        如果是 event_fd：
            处理工作线程通知
    }
```

这就是 Linux 应用层常见的 **事件驱动架构**。

---

# 17. 最后总结

一句话区分：

```text
select：
    老接口，用 fd_set，fd 数量有限，每次重建集合，小规模可以用。

poll：
    用 pollfd 数组，比 select 清晰，没有明显 FD_SETSIZE 限制，但仍然每次遍历数组。

epoll：
    Linux 高性能机制，fd 先注册到内核，事件发生后直接返回活跃 fd，适合大量 fd 和事件驱动程序。
```

实际项目建议：

```text
简单小 demo：
    select / poll 都可以

普通嵌入式应用，监听几个 fd：
    poll 很合适

TCP server、多客户端、统一事件循环：
    epoll 更合适

刚学 epoll：
    先用 LT 模式，不要一上来就用 EPOLLET
```

你可以把它们理解成从老到新的演进：

```text
select  ->  poll  ->  epoll

越来越适合大量 fd
越来越适合事件驱动服务器
但使用复杂度也略有上升
```
