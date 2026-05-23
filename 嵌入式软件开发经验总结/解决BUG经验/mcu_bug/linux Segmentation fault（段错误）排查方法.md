Linux 系统下遇到 **Segmentation fault（段错误）**，说明程序访问了**非法内存地址**，常见原因包括：

* 访问了野指针（未初始化的指针）。
* 内存越界。
* 空指针解引用。
* 栈溢出。
* 数组下标越界。

---

## 【标准排查思路】：

### ① **使用 gdb 调试**

这是最直接有效的办法。

假设你的程序叫 `file_transfer`：

```bash
gdb /userdata/file_transfer_module/file_transfer
```

然后在 gdb 内输入：

```bash
run
```

程序崩溃后，输入：

```bash
bt
```

查看函数调用栈，定位崩溃位置。

---

### ② **确认是否有 core dump 文件**

如果系统启用了 core dump，崩溃后会生成 core 文件，配合 gdb 也可以排查。

查看 core dump 设置：

```bash
ulimit -c
```

如果返回 `0`，说明不生成 core 文件，可以临时开启：

```bash
ulimit -c unlimited
```

程序再次崩溃后，查找 core 文件：

```bash
find / -name core
```

定位到 core 文件后：

```bash
gdb /userdata/file_transfer_module/file_transfer core
```

然后同样用 `bt` 查看调用栈。

---

### ③ **检查可疑代码**

根据你提供的日志：

```
[Unix_Socket_Server]: Client connected!.
[Unix_Socket_Server]: Client Disconnected.
[Unix_Socket_Server]: Client Rx data Faield!.
Segmentation fault
```

怀疑点：

* `Client Rx data Failed!`，说明接收数据出错，可能对空指针、非法数据结构进行了操作。
* Unix Socket 断开后，是否有释放、重连、数据缓存处理逻辑？如果没处理好，很容易出现空指针或野指针。

建议重点检查：

✔️ `recv()`、`read()` 后的数据校验
✔️ Socket 断连时，内存释放与清理逻辑
✔️ 涉及指针传递的函数是否检查了有效性

---

### ④ **添加打印辅助定位**

如果不方便调试，可以：

```c
printf("变量x地址: %p\n", x);
printf("数据指针: %p\n", data_ptr);
```

或者使用 `assert`：

```c
assert(data_ptr != NULL);
```

辅助定位崩溃前的状态。

---

### ⑤ **编译打开调试信息**

如果你自己编译程序，确保加了 `-g` 选项：

```bash
g++ -g -o file_transfer your_code.cpp ...
```

这样生成的可执行文件方便 gdb 精确定位源代码。

---

## 【总结】：

1. **gdb 跟踪**：优先推荐。
2. **core dump**：辅助检查。
3. **逻辑检查**：重点看 socket 断连、空指针。
4. **打印信息**：快速辅助排查。

---

如果你方便贴出 `unix_socket_server` 或相关接收数据部分的核心代码，我可以帮你一起看是否有明显风险。

要不要一起看下代码？
