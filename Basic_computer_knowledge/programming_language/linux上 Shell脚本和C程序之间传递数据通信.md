你问的是 Linux 上 **Shell 脚本和 C 程序之间传递数据通信**，这其实是个很常见的问题，常见方法很多，具体选用哪种取决于你数据的复杂度、实时性和应用场景。

下面我总结一些常见的方式，并简要说明它们适合什么场景：

---

## 1. 通过环境变量传递（简单参数）

* **Shell 设置环境变量**，然后执行 C 程序，C 程序通过 `getenv()` 读取。
* **缺点**：环境变量只在进程启动时生效，无法中途更改通信。

示例：

Shell 脚本：

```sh
export MYVAR="hello_from_shell"
./my_c_program
```

C 程序：

```c
#include <stdio.h>
#include <stdlib.h>

int main() {
    char *val = getenv("MYVAR");
    if (val)
        printf("Got env var: %s\n", val);
    return 0;
}
```

---

## 2. 命令行参数传递

* Shell 调用 C 程序时，带参数，C 程序用 `argv[]` 读取。
* 适合简单参数传递。

Shell:

```sh
./my_c_program "hello_from_shell"
```

C:

```c
#include <stdio.h>

int main(int argc, char **argv) {
    if (argc > 1)
        printf("Arg: %s\n", argv[1]);
    return 0;
}
```

---

## 3. 标准输入/输出 (stdin/stdout) 交互

* Shell 脚本通过管道（`|`）或者重定向把数据写给 C 程序。
* C 程序通过 `scanf`, `fgets` 等从 `stdin` 读，或者写到 `stdout`。
* 适合文本数据流式交互。

Shell 发送数据给 C 程序：

```sh
echo "hello from shell" | ./my_c_program
```

C 程序读取 stdin：

```c
#include <stdio.h>

int main() {
    char buf[100];
    if (fgets(buf, sizeof(buf), stdin) != NULL)
        printf("Received: %s\n", buf);
    return 0;
}
```

---

## 4. 通过临时文件传递

* Shell 脚本写入一个文件，C 程序读取文件，或者反过来。
* 简单，跨进程，适合较大数据，但会有磁盘I/O开销。

Shell:

```sh
echo "data" > /tmp/data.txt
./my_c_program /tmp/data.txt
```

C:

```c
#include <stdio.h>

int main(int argc, char **argv) {
    if (argc < 2) return 1;
    FILE *f = fopen(argv[1], "r");
    if (!f) return 1;
    char buf[256];
    while (fgets(buf, sizeof(buf), f)) {
        printf("Read line: %s", buf);
    }
    fclose(f);
    return 0;
}
```

---

## 5. 进程间通信（IPC）

更高级的场景，可以用以下机制：

* **管道（pipe）**：匿名管道或有名管道（FIFO）
* **消息队列（System V 或 POSIX）**
* **共享内存**
* **Unix domain socket**

举个最简单的 \*\*有名管道（FIFO）\*\*示例：

Shell 端写入：

```sh
mkfifo /tmp/myfifo
echo "hello from shell" > /tmp/myfifo
```

C 程序读：

```c
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

int main() {
    int fd = open("/tmp/myfifo", O_RDONLY);
    if (fd < 0) return 1;
    char buf[100];
    int n = read(fd, buf, sizeof(buf)-1);
    if (n > 0) {
        buf[n] = 0;
        printf("Read from fifo: %s\n", buf);
    }
    close(fd);
    return 0;
}
```

---

## 6. 使用 Socket

* 可以用 TCP 或 Unix 域套接字在 Shell 和 C 程序之间传递数据。
* 适合需要网络通信或多进程复杂交互。

---

## 7. 通过信号（Signal）

* 传递非常有限的简单通知，通常携带少量数据（信号值）。
* 不适合传递复杂数据。

---

# 总结建议

| 场景        | 推荐方式               | 说明          |
| --------- | ------------------ | ----------- |
| 简单参数      | 环境变量 / 命令行参数       | 方便快速传值      |
| 文本数据流     | 标准输入输出             | 简单、灵活       |
| 较大数据或持久存储 | 临时文件               | 简单，易于调试     |
| 复杂实时通信    | 管道 / FIFO / Socket | 适合进程间持续通信   |
| 网络通信      | TCP/Unix socket    | 跨机器或复杂服务间通信 |

---

如果你告诉我你具体想实现哪种交互，比如“一次性传一个字符串”，还是“持续双向通信”，我可以帮你写具体示例代码哦。你觉得呢？

非常明确了，你是在 **本地嵌入式 Linux 设备**上，C 程序和 Shell 脚本之间需要数据传输。

**总结下你的前提**：

✅ 本地通信，非网络
✅ 嵌入式 Linux，资源可能有限
✅ C 程序 和 Shell 脚本 需要互相传递数据

---

# **实际可行方案推荐：**

根据嵌入式场景，我建议你优先考虑这几种：

---

## 【1】简单传参：脚本调用 C 程序

### 脚本向 C 程序传递数据：

Shell：

```sh
./my_program "data_from_shell"
```

C：

```c
#include <stdio.h>

int main(int argc, char **argv) {
    if (argc > 1)
        printf("Received: %s\n", argv[1]);
    return 0;
}
```

---

## 【2】标准输入输出 + 管道

### Shell 脚本给 C 程序传数据：

```sh
echo "hello from shell" | ./my_program
```

### C 程序读取：

```c
#include <stdio.h>

int main() {
    char buf[100];
    if (fgets(buf, sizeof(buf), stdin))
        printf("C program got: %s\n", buf);
    return 0;
}
```

### 反过来：C 程序输出，Shell 脚本接收：

```sh
output=$(./my_program)
echo "C 返回了: $output"
```

---

## 【3】通过临时文件

Shell：

```sh
echo "hello data" > /tmp/data.txt
./my_program /tmp/data.txt
```

C：

```c
#include <stdio.h>

int main(int argc, char **argv) {
    char buf[100];
    if (argc < 2) return 1;
    FILE *f = fopen(argv[1], "r");
    if (!f) return 1;
    fgets(buf, sizeof(buf), f);
    printf("读取到: %s\n", buf);
    fclose(f);
    return 0;
}
```

---

## 【4】有名管道（FIFO）【适合持续数据通信】

Shell 创建管道：

```sh
mkfifo /tmp/myfifo
```

Shell 写数据：

```sh
echo "hello fifo" > /tmp/myfifo
```

C 程序读数据：

```c
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

int main() {
    int fd = open("/tmp/myfifo", O_RDONLY);
    char buf[100];
    int n = read(fd, buf, sizeof(buf) - 1);
    if (n > 0) {
        buf[n] = 0;
        printf("收到: %s\n", buf);
    }
    close(fd);
    return 0;
}
```

可以反过来，C 写，Shell 读。

---

## 【5】如果需要复杂、可靠、实时通信

可以用：

✅ Unix Socket
✅ 消息队列（msg queue）
✅ 共享内存（shm）

不过这些用在嵌入式上稍微重一点，如果只是简单 Shell 和 C 交互，**前面几种已经够用了**。

---

# 【总结推荐】

| 需求        | 推荐方式       |
| --------- | ---------- |
| 一次性简单传值   | 参数 / 环境变量  |
| 文本数据流     | 标准输入输出     |
| 大点数据或临时存储 | 临时文件       |
| 持续数据交互    | 有名管道（FIFO） |

---

# 【直接建议】

如果你就是：

✅ Shell 脚本启动 C 程序，传个值
✅ C 程序跑起来，持续等 Shell 脚本传数据

那最靠谱是：

**有名管道（FIFO）**
或者
**Shell 用 echo | ./C程序**（适合简单传参）

---

你具体是单次数据，还是持续交互？
如果想，我可以直接帮你写完整的 demo，告诉你两边怎么配合。

你确认下：
👉 需要单次传值？
👉 还是持续数据通信？

告诉我，立马写给你。

非常明确了！你说 **不要传参**，那就排除了 **命令行参数、环境变量** 这一类方式。

所以，重点推荐以下这几种纯粹的、**不通过参数** 的 C 程序和 Shell 脚本之间数据传输方案：

---

# ✅ **方案 1：标准输入输出 + 管道（推荐，简单、轻量）**

### Shell 脚本把数据通过管道传给 C 程序：

```sh
echo "data from shell" | ./my_program
```

### C 程序读标准输入：

```c
#include <stdio.h>

int main() {
    char buf[100];
    if (fgets(buf, sizeof(buf), stdin)) {
        printf("C程序收到：%s\n", buf);
    }
    return 0;
}
```

---

# ✅ **方案 2：临时文件传递**

Shell 脚本写入文件：

```sh
echo "data from shell" > /tmp/data.txt
./my_program
```

C 程序自己固定读这个文件：

```c
#include <stdio.h>

int main() {
    char buf[100];
    FILE *f = fopen("/tmp/data.txt", "r");
    if (!f) {
        perror("打开文件失败");
        return 1;
    }
    if (fgets(buf, sizeof(buf), f))
        printf("从文件读取：%s\n", buf);
    fclose(f);
    return 0;
}
```

---

# ✅ **方案 3：有名管道 FIFO（持续通信适用）**

### 只需在系统中提前创建一次：

```sh
mkfifo /tmp/myfifo
```

### Shell 脚本发送数据：

```sh
echo "data from shell" > /tmp/myfifo
```

### C 程序持续读取：

```c
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

int main() {
    int fd = open("/tmp/myfifo", O_RDONLY);
    if (fd < 0) {
        perror("打开fifo失败");
        return 1;
    }
    char buf[100];
    int n = read(fd, buf, sizeof(buf)-1);
    if (n > 0) {
        buf[n] = 0;
        printf("C程序收到：%s\n", buf);
    }
    close(fd);
    return 0;
}
```

注意：`read()` 是阻塞的，直到 Shell 端写入数据。

---

# ✅ **方案 4：Unix Socket**（如果需要复杂、可靠通信）

但嵌入式一般简单需求不建议走这套，前3种已够用。

---

# 【总结推荐】

| 需求     | 推荐方式        |
| ------ | ----------- |
| 一次性数据  | 标准输入输出、临时文件 |
| 持续数据交互 | 有名管道 FIFO   |

---

# 【我可以给你完整demo】

如果你告诉我：
👉 你更倾向用 `标准输入`、`文件` 还是 `FIFO`
👉 Shell 脚本控制流程，还是 C 程序主动等待

我立刻写完整可运行的 C 程序 + Shell 脚本示例，嵌入式设备直接用。

你想要哪种？直接说方案编号，我写完整示例。



很好的问题！Shell脚本和C程序之间传递数据通信，除了环境变量（只适合启动时传值）外，还有多种方法，根据数据量、实时性和复杂度不同，常见方案包括：

---

## 1. **命令行参数传递**

* Shell脚本调用C程序时，把参数传给它

```bash
./my_c_program "some data" 123
```

C程序里用 `argv` 读取

**优点：简单方便，适合少量参数传递**

---

## 2. **环境变量传递**

* Shell脚本设置环境变量，C程序启动时通过 `getenv()` 读取

**优点：无须传递参数，适合配置类信息**

**缺点：仅启动时传值，不能动态更新**

---

## 3. **标准输入输出（stdin/stdout）管道**

* Shell通过管道向C程序传数据，或者C程序输出数据给Shell

```bash
echo "hello from shell" | ./my_c_program
```

C程序读取标准输入：

```c
char buf[100];
fgets(buf, sizeof(buf), stdin);
```

反过来：

```bash
./my_c_program | grep "something"
```

**优点：灵活，实时数据流传输**

---

## 4. **文件**

* Shell脚本写入文件，C程序读文件
* 或者C程序写文件，Shell脚本读取

```bash
echo "data" > /tmp/data.txt
./my_c_program /tmp/data.txt
```

**优点：简单，跨进程持久通信**

**缺点：I/O性能和同步控制需自己管理**

---

## 5. **命名管道（FIFO）**

* 进程间通信的特殊文件，数据通过文件流传递

```bash
mkfifo /tmp/myfifo
echo "hello" > /tmp/myfifo &  # shell写入
./my_c_program /tmp/myfifo    # C程序读取
```

**优点：实时，半双工通信**

---

## 6. **匿名管道（pipe）**

* 父进程创建管道，fork后子进程读写

* Shell可用管道命令符 `|` 与 C 程序结合

---

## 7. **信号（signal）**

* 发送简单通知，参数有限，不能传大量数据

* Shell用 `kill -SIGUSR1 pid` 触发，C程序注册信号处理函数

**多用于触发事件，不传递复杂数据**

---

## 8. **Unix域套接字（Unix Domain Socket）**

* 进程间基于文件系统的Socket通信，支持双向通信

* 适合复杂数据交互

---

## 9. **共享内存（shm）**

* 进程共享一块内存区域，读写高效

* 需要同步机制（信号量等）防止冲突

---

## 10. **消息队列**

* 操作系统提供的消息机制

* 适合结构化消息传递

---

## 简单对比表

| 方法           | 适用场景     | 优点        | 缺点       |
| ------------ | -------- | --------- | -------- |
| 命令行参数        | 简单启动时传值  | 简单        | 参数量受限    |
| 环境变量         | 启动时传配置   | 简单，无需修改代码 | 仅启动传递    |
| stdin/stdout | 流式数据传递   | 灵活，实时     | 需要进程间同步  |
| 文件           | 持久数据交换   | 简单，跨进程    | 慢，需处理同步  |
| 命名管道 FIFO    | 半双工实时    | 实时，简单     | 只能单向通信   |
| 信号           | 事件通知     | 简单，实时     | 只能传递有限信息 |
| Unix Socket  | 复杂双向通信   | 高效，灵活     | 编程复杂     |
| 共享内存         | 高性能大数据共享 | 快速        | 复杂同步和管理  |
| 消息队列         | 结构化消息传递  | 方便消息管理    | 编程复杂     |

---

如果你需要，我可以帮你写几种通信方式的**Shell脚本 + C示例代码**，或者帮你选适合你的具体场景的方案。

你具体想实现什么样的数据通信？实时的还是启动时？大数据还是简单参数？我帮你定制方案。

