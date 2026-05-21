非常好的问题！

在 Linux / Unix 系统中，**进程之间的通信（IPC，Inter-Process Communication）** 是操作系统中非常重要的一部分，允许不同进程协作、交换数据。

---

## 🧠 一、什么是进程间通信（IPC）？

> 当两个或多个独立运行的程序（进程）需要 **共享数据、交换信息或协同工作** 时，它们就需要通过 IPC 来“对话”。

---

## 📦 二、常见的 7 种 IPC 方式（经典总结）

| IPC 方法              | 是否跨进程      | 特点       |
| ------------------- | ---------- | -------- |
| 1️⃣ 管道（pipe）        | ❌ 父子进程     | 简单，单向    |
| 2️⃣ 命名管道（FIFO）      | ✅ 任意进程     | 可用于不相关进程 |
| 3️⃣ 信号（signal）      | ✅ 任意进程     | 通知型，轻量级  |
| 4️⃣ 消息队列（msg queue） | ✅ 任意进程     | 有序消息传递   |
| 5️⃣ 共享内存（shm）       | ✅ 任意进程     | 最高效，需要同步 |
| 6️⃣ 信号量（semaphore）  | ✅ 任意进程     | 控制同步/互斥  |
| 7️⃣ 套接字（socket）     | ✅ 本地 or 网络 | 最灵活，可跨主机 |

---

## ✅ 三、每种通信方式简单举例说明

---

### 🔹 1. 匿名管道 `pipe`

```c
int pipefd[2];
pipe(pipefd);

pid_t pid = fork();
if (pid == 0) {
    // 子进程
    close(pipefd[1]); // 关闭写端
    read(pipefd[0], buf, sizeof(buf));
} else {
    // 父进程
    close(pipefd[0]); // 关闭读端
    write(pipefd[1], "hello", 5);
}
```

> 父子进程间一对一通信，**半双工**，不能跨进程。

---

### 🔹 2. 命名管道 `mkfifo`

```bash
mkfifo /tmp/myfifo
```

一个进程写入 `/tmp/myfifo`，另一个读取。

```c
int fd = open("/tmp/myfifo", O_WRONLY);
write(fd, "hello", 5);
```

---

### 🔹 3. 信号 signal（通知类）

```c
signal(SIGUSR1, handler); // 注册信号处理函数
kill(pid, SIGUSR1);       // 向另一个进程发送信号
```

> 只能传“一个事件”，不能传复杂数据。

---

### 🔹 4. 消息队列（`msgget`/`msgsnd`/`msgrcv`）

```c
msgget(), msgsnd(), msgrcv()
```

优点是：**可传结构体**、消息可分优先级、FIFO 顺序。

---

### 🔹 5. 共享内存（最高效！）

```c
int shmid = shmget(IPC_PRIVATE, 1024, IPC_CREAT | 0666);
char *data = (char*) shmat(shmid, NULL, 0);
```

* 缺点是要搭配 `sem` 或 `mutex` 同步
* 多用于大数据传输场景（如图像、音频）

---

### 🔹 6. 信号量（用于互斥与同步）

```c
semget(), semop()
```

可实现类似 “锁” 的机制，用于多个进程间的同步控制。

---

### 🔹 7. 本地套接字（Unix Socket）

```c
// 服务端创建 AF_UNIX socket
socket(AF_UNIX, SOCK_STREAM, 0)
bind(), listen(), accept()
// 客户端 connect()
```

非常类似 TCP 套接字，但用于本机进程间通信。

> docker daemon、X11、pulseaudio 都用它。

---

## 🧰 四、场景选型推荐表

| 场景       | 推荐方式          |
| -------- | ------------- |
| 父子进程通信   | pipe（匿名管道）    |
| 任意进程点对点  | FIFO / socket |
| 需要传结构化消息 | 消息队列          |
| 大量数据共享   | 共享内存 + 信号量    |
| 本地服务程序   | Unix Socket   |
| 需要简单通知   | signal        |

---

## ✅ 五、现代开发常用组合

* **轻量型后台通信服务**：Unix Socket + select/poll/epoll
* **图像/视频进程通信**：共享内存 + 信号量
* **进程间控制指令**：FIFO + select
* **分布式通信**：TCP/UDP Socket

---

## 📘 总结一句话：

> Unix 提供了一套丰富的 IPC 手段，从简单的 `pipe` 到高级的 `socket`，可按场景组合使用，关键在于是否需要 **数据、同步、效率、跨进程、跨主机**。

---

如果你感兴趣，我可以：

✅ 每种方式都给你写一份小 demo 示例
✅ 重点讲 Unix Socket 的高效多进程架构
✅ 讲如何封装 IPC 通道为模块化框架（比如 server/client 模式）

你希望先从哪一种方式深入？我可以按顺序展开。



太好了！下面我将为你提供 7 种经典进程间通信（IPC）方式的 **最小可运行 demo 示例**，每个都包含注释说明，确保你可以 copy 编译运行并理解其工作原理。

---

## ✅ 1. 匿名管道 `pipe()` 示例（父子进程通信）

```cpp
// file: pipe_demo.cpp
#include <unistd.h>
#include <iostream>
#include <cstring>

int main() {
    int pipefd[2];
    pipe(pipefd);
    pid_t pid = fork();

    if (pid == 0) { // 子进程
        close(pipefd[1]);
        char buf[128] = {0};
        read(pipefd[0], buf, sizeof(buf));
        std::cout << "[Child] Received: " << buf << std::endl;
    } else { // 父进程
        close(pipefd[0]);
        const char *msg = "Hello from parent";
        write(pipefd[1], msg, strlen(msg));
    }
    return 0;
}
```

---

## ✅ 2. 命名管道 FIFO 示例（无血缘进程通信）

创建 FIFO：

```bash
mkfifo /tmp/myfifo
```

写端程序：

```cpp
// file: fifo_write.cpp
#include <fcntl.h>
#include <unistd.h>
int main() {
    int fd = open("/tmp/myfifo", O_WRONLY);
    write(fd, "Hello FIFO", 10);
    close(fd);
}
```

读端程序：

```cpp
// file: fifo_read.cpp
#include <fcntl.h>
#include <unistd.h>
#include <iostream>
int main() {
    int fd = open("/tmp/myfifo", O_RDONLY);
    char buf[128] = {0};
    read(fd, buf, sizeof(buf));
    std::cout << "Received: " << buf << std::endl;
    close(fd);
}
```

---

## ✅ 3. 信号 `kill()` + `signal()` 示例

```cpp
// file: signal_demo.cpp
#include <signal.h>
#include <unistd.h>
#include <iostream>

void handler(int sig) {
    std::cout << "Received signal: " << sig << std::endl;
}

int main() {
    signal(SIGUSR1, handler);
    pid_t pid = fork();
    if (pid == 0) {
        sleep(1);
        kill(getppid(), SIGUSR1);
    } else {
        pause();  // 等待信号
    }
    return 0;
}
```

---

## ✅ 4. 消息队列（System V）`msgget` 示例

```cpp
// file: msg_queue.cpp
#include <sys/ipc.h>
#include <sys/msg.h>
#include <cstring>
#include <iostream>

struct msgbuf {
    long mtype;
    char mtext[100];
};

int main() {
    key_t key = ftok(".", 65);
    int msgid = msgget(key, 0666 | IPC_CREAT);

    pid_t pid = fork();
    if (pid == 0) { // 子进程：发送
        msgbuf msg = {1, "Hello from child"};
        msgsnd(msgid, &msg, sizeof(msg.mtext), 0);
    } else { // 父进程：接收
        msgbuf msg;
        msgrcv(msgid, &msg, sizeof(msg.mtext), 1, 0);
        std::cout << "[Parent] Got message: " << msg.mtext << std::endl;
        msgctl(msgid, IPC_RMID, nullptr); // 删除队列
    }
    return 0;
}
```

---

## ✅ 5. 共享内存 + 同步（最简单示例）

```cpp
// file: shm_demo.cpp
#include <sys/ipc.h>
#include <sys/shm.h>
#include <cstring>
#include <iostream>
#include <unistd.h>

int main() {
    key_t key = ftok(".", 65);
    int shmid = shmget(key, 1024, 0666 | IPC_CREAT);
    char *data = (char*)shmat(shmid, nullptr, 0);

    pid_t pid = fork();
    if (pid == 0) {
        sleep(1);
        std::cout << "[Child] Read: " << data << std::endl;
    } else {
        strcpy(data, "Shared memory says hi");
        wait(nullptr);
        shmdt(data);
        shmctl(shmid, IPC_RMID, nullptr);
    }
    return 0;
}
```

---

## ✅ 6. 信号量（System V `semget`）用于同步示例

```cpp
// file: sem_demo.cpp
#include <sys/ipc.h>
#include <sys/sem.h>
#include <unistd.h>
#include <iostream>
#include <cstring>

union semun {
    int val;
};

int main() {
    key_t key = ftok(".", 66);
    int semid = semget(key, 1, 0666 | IPC_CREAT);
    semun u; u.val = 0;
    semctl(semid, 0, SETVAL, u);

    pid_t pid = fork();
    if (pid == 0) {
        sleep(1);
        std::cout << "[Child] Doing some work\n";
        struct sembuf op = {0, 1, 0}; // V操作
        semop(semid, &op, 1);
    } else {
        struct sembuf op = {0, -1, 0}; // P操作
        std::cout << "[Parent] Waiting...\n";
        semop(semid, &op, 1);
        std::cout << "[Parent] Done\n";
        semctl(semid, 0, IPC_RMID);
    }
    return 0;
}
```

---

## ✅ 7. Unix Socket（本地进程 socket 通信）

### 服务器：

```cpp
// file: unix_server.cpp
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <iostream>

int main() {
    int server = socket(AF_UNIX, SOCK_STREAM, 0);
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, "/tmp/unix.sock");
    unlink("/tmp/unix.sock");

    bind(server, (sockaddr*)&addr, sizeof(addr));
    listen(server, 1);

    int client = accept(server, nullptr, nullptr);
    char buf[128];
    read(client, buf, sizeof(buf));
    std::cout << "Server received: " << buf << std::endl;
    close(client); close(server);
    unlink("/tmp/unix.sock");
}
```

### 客户端：

```cpp
// file: unix_client.cpp
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

int main() {
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, "/tmp/unix.sock");

    connect(sock, (sockaddr*)&addr, sizeof(addr));
    write(sock, "Hello socket!", 14);
    close(sock);
}
```

---

## ✅ 编译方式示例

```bash
g++ pipe_demo.cpp -o pipe_demo
g++ fifo_read.cpp -o fifo_read
g++ fifo_write.cpp -o fifo_write
g++ signal_demo.cpp -o signal_demo
g++ msg_queue.cpp -o msg_queue
g++ shm_demo.cpp -o shm_demo
g++ sem_demo.cpp -o sem_demo
g++ unix_server.cpp -o unix_server
g++ unix_client.cpp -o unix_client
```

---

如果你希望：

* 我整理为一个 `Makefile` 一键编译；
* 拓展某一种实现（比如 UNIX socket 的 `epoll` 版本）；
* 或封装成一个多进程模块化通信框架（如 Server/Client 模式）；

随时告诉我！我们可以基于这些 demo 深入工程实践。
