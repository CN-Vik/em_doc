# 检测文件的大小

你可以通过标准 C/C++ 的文件 API 很方便地获取文件的字节大小，以下是几种常用方法：

---

## ✅ 方法一：使用 `stat()` 函数（推荐）

```cpp
#include <sys/stat.h>
#include <stdio.h>

off_t get_file_size(const char* file_path) {
    struct stat st;
    if (stat(file_path, &st) == 0) {
        return st.st_size;  // 返回字节数
    } else {
        perror("stat failed");
        return -1;
    }
}
```

### ✅ 用法示例：

```cpp
const char* path = "myfile.zip";
off_t size = get_file_size(path);
printf("File size = %ld bytes\n", size);
```

---

## ✅ 方法二：使用 `fseek()` / `ftell()`

```cpp
long get_file_size_fp(FILE* fp) {
    long current = ftell(fp);   // 记录当前读写位置
    fseek(fp, 0, SEEK_END);     // 跳到末尾
    long size = ftell(fp);      // 获取位置
    fseek(fp, current, SEEK_SET); // 恢复位置
    return size;
}
```

### ⚠️ 注意：

* `ftell()` 返回的是 `long`，有系统兼容限制（大文件时注意 `off_t`）
* 推荐用于你已经打开了文件的场景（不适合只用路径的情况）

---

## ✅ 方法三：C++17 的 `std::filesystem`（仅限 C++17 及以上）

```cpp
#include <filesystem>
namespace fs = std::filesystem;

uintmax_t get_file_size(const std::string& path) {
    return fs::file_size(path);
}
```

---

## ✅ 总结：推荐使用 `stat()`

```cpp
off_t get_file_size(const char* path) {
    struct stat st;
    return stat(path, &st) == 0 ? st.st_size : -1;
}
```

* ✅ 简洁
* ✅ 跨平台（Linux/Unix）
* ✅ 不依赖文件流，不怕破坏文件位置

---

需要我一并把“读取文件内容 + 填充结构体 + 发送”整合成一个发送函数吗？例如：

```cpp
bool send_file(const char* src_path, const char* dst_path, int fd);
```

我可以直接帮你写好。是否继续？

✅ 明白了 —— 你要实现的是 **一次性发送整个压缩包文件（如 `.zip`）**，那么结构体就可以设计得更简单，**不需要 offset / 分包机制**，只传一整个文件内容。

---

## ✅ 简洁版结构体设计：一次性发送文件

```cpp
#define MAX_FILENAME_LEN 128
#define MAX_FILEPATH_LEN 256
#define MAX_FILEDATA_LEN (4 * 1024 * 1024)  // 例如最多传 4MB，可自定义

struct FilePacket {
    uint32_t cmd_id;             // 指令编号，比如 0x01 表示发送文件
    uint32_t file_size;          // 文件实际大小
    uint32_t file_crc32;         // 文件整体 CRC32，用于校验
    char     file_name[MAX_FILENAME_LEN];   // 文件名（如 data.zip）
    char     file_path[MAX_FILEPATH_LEN];   // 保存路径（如 /tmp/data.zip）
    uint8_t  file_data[MAX_FILEDATA_LEN];   // 实际文件内容
};
```

---

## ✅ 使用流程说明

### 1. PC 端

* 读取压缩包内容到 `file_data`
* 计算文件实际大小和 CRC32
* 设置目标保存路径
* 一次性发送整个结构体（例如通过 USB FunctionFS 或 UNIX socket）

---

### 2. 嵌入式设备端

* 接收整个结构体
* 先校验 `file_size` 和 `file_crc32`
* 保存 `file_data[0..file_size-1]` 到 `file_path`
* 成功后用 `unzip` 或 `tar` 解压

---

## 🧠 注意事项

| 项目         | 建议                               |
| ---------- | -------------------------------- |
| **最大文件大小** | 控制在 MAX\_FILEDATA\_LEN 之内（如 4MB） |
| **传输协议**   | USB、UNIX Socket、Pipe 等都可         |
| **内存分配**   | 接收端需要足够大缓冲区，否则应动态申请              |
| **CRC校验**  | 推荐使用 CRC32（或 MD5）校验完整性           |

---

## ✅ 示例：文件接收逻辑（伪代码）

```cpp
FilePacket pkt;
recv(fd, &pkt, sizeof(FilePacket), 0);

// 校验
if (pkt.file_crc32 != calc_crc32(pkt.file_data, pkt.file_size)) {
    fprintf(stderr, "CRC mismatch\n");
    return -1;
}

// 保存文件
FILE *fp = fopen(pkt.file_path, "wb");
fwrite(pkt.file_data, 1, pkt.file_size, fp);
fclose(fp);

// 解压
char cmd[512];
sprintf(cmd, "unzip -o %s -d /tmp/unpack_dir/", pkt.file_path);
system(cmd);
```

---

## ✅ 总结：你的最终结构体如下

```cpp
struct FilePacket {
    uint32_t cmd_id;             // CMD_SEND_FILE
    uint32_t file_size;
    uint32_t file_crc32;
    char     file_name[128];
    char     file_path[256];
    uint8_t  file_data[4 * 1024 * 1024];  // 最大4MB压缩包
};
```

非常适合你现在这种 **一发搞定、接收即解压** 的模式。

---

如果你需要，我可以：

* 帮你写出 C++ 的发送/接收代码模板
* 提供 CRC32 实现
* 添加压缩包校验 + 解压自动调用逻辑

是否需要我给你生成一套完整函数代码？



好的，我们来构建一套完整的**一次性发送压缩包文件的文件传输代码模板**，包括：

---

## ✅ 功能说明

* PC 端：发送 `.zip` 压缩包，读取 + CRC32 校验 + 封装发送
* 设备端：接收文件结构体 → CRC 校验 → 写入文件 → 自动解压

---

## ✅ 公共结构体定义（FilePacket）

```cpp
#define MAX_FILENAME_LEN 128
#define MAX_FILEPATH_LEN 256
#define MAX_FILEDATA_LEN (4 * 1024 * 1024)  // 最多 4MB，可根据内存调整

struct FilePacket {
    uint32_t cmd_id;            // CMD_SEND_FILE = 0x01
    uint32_t file_size;
    uint32_t file_crc32;
    char     file_name[MAX_FILENAME_LEN];
    char     file_path[MAX_FILEPATH_LEN];
    uint8_t  file_data[MAX_FILEDATA_LEN];
};
```

---

## ✅ CRC32 实现（简洁版本）

```cpp
uint32_t crc32(const uint8_t* data, size_t len) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int j = 0; j < 8; ++j)
            crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
    }
    return ~crc;
}
```

---

## ✅ 发送端函数（PC）

```cpp
bool send_file(int sock_fd, const char* src_path, const char* dst_path) {
    FILE* fp = fopen(src_path, "rb");
    if (!fp) {
        perror("open src file");
        return false;
    }

    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (file_size <= 0 || file_size > MAX_FILEDATA_LEN) {
        fprintf(stderr, "Invalid file size: %ld\n", file_size);
        fclose(fp);
        return false;
    }

    FilePacket pkt = {};
    pkt.cmd_id = 0x01;
    pkt.file_size = file_size;
    fread(pkt.file_data, 1, file_size, fp);
    fclose(fp);

    pkt.file_crc32 = crc32(pkt.file_data, file_size);
    strncpy(pkt.file_name, basename((char*)src_path), MAX_FILENAME_LEN);
    strncpy(pkt.file_path, dst_path, MAX_FILEPATH_LEN);

    if (send(sock_fd, &pkt, sizeof(pkt), 0) != sizeof(pkt)) {
        perror("send failed");
        return false;
    }

    printf("Send file OK: %s (%ld bytes)\n", src_path, file_size);
    return true;
}
```

---

## ✅ 接收端函数（设备）

```cpp
bool recv_file(int sock_fd) {
    FilePacket pkt;
    ssize_t n = recv(sock_fd, &pkt, sizeof(pkt), 0);
    if (n != sizeof(pkt)) {
        perror("recv failed or incomplete");
        return false;
    }

    uint32_t calc_crc = crc32(pkt.file_data, pkt.file_size);
    if (calc_crc != pkt.file_crc32) {
        fprintf(stderr, "CRC32 mismatch! recv=0x%08x, calc=0x%08x\n", pkt.file_crc32, calc_crc);
        return false;
    }

    FILE* fp = fopen(pkt.file_path, "wb");
    if (!fp) {
        perror("open write file");
        return false;
    }
    fwrite(pkt.file_data, 1, pkt.file_size, fp);
    fclose(fp);

    printf("File received: %s (%u bytes), saved to %s\n",
        pkt.file_name, pkt.file_size, pkt.file_path);

    // 自动解压
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "unzip -o %s -d /tmp/unpack_dir/", pkt.file_path);
    int ret = system(cmd);
    if (ret != 0) {
        fprintf(stderr, "unzip failed\n");
        return false;
    }

    printf("Unzip complete.\n");
    return true;
}
```

---

## ✅ 示例调用方式

### PC 端发送：

```cpp
send_file(sock_fd, "./update.zip", "/tmp/recv.zip");
```

### 设备端接收并解压：

```cpp
recv_file(sock_fd);
```

---

## 🧱 后续建议扩展

| 功能       | 可做法                  |
| -------- | -------------------- |
| ACK/NACK | 接收端校验成功后返回 ACK       |
| 分包大文件    | 适配 chunk 传输结构体       |
| 解压失败回滚   | 解压前备份旧文件             |
| 解压路径配置   | 结构体中加入 `unpack_path` |

---

是否需要我帮你打包成完整工程目录（带 Makefile/CMake）或对接到 FunctionFS 项目中？你也可以告诉我你当前的传输通道（USB FunctionFS / Socket / Pipe）我帮你定制集成。是否继续？
