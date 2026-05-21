
ESP32 的 HTTP OTA 升级，通俗说就是：

```txt id="7g4z56"
ESP32 正在运行旧程序
        ↓
通过 Wi-Fi 去服务器下载新的 app.bin
        ↓
把新 app 写到 Flash 的另一个 APP 分区
        ↓
写完后告诉 bootloader：下次启动用新 APP
        ↓
重启
        ↓
bootloader 启动新 APP
```

注意一个关键点：**不是 bootloader 去联网下载固件，而是“当前正在运行的 APP”负责联网下载新固件。**
bootloader 只负责开机时选择启动哪个 APP 分区。

---

# 1. OTA 是解决什么问题？

正常烧录程序是这样：

```txt id="0bitqq"
电脑 USB 线 → ESP32 → 烧录新固件
```

但是产品卖出去以后，你不可能让用户拆机接 USB，所以就需要 OTA：

```txt id="m3j6qw"
服务器上放新固件
ESP32 通过 Wi-Fi 下载
自己写入 Flash
自己重启升级
```

所以 OTA 的本质就是：

```txt id="lelagy"
远程更新固件
```

比如你的设备已经卖出去了，后来你发现：

```txt id="0rh4q8"
有 bug
需要加新功能
需要换 UI
需要优化通信协议
需要升级算法
```

这时候就可以通过 OTA 远程升级。

---

# 2. ESP32 为什么需要多个 APP 分区？

如果 ESP32 只有一个 APP 分区，升级时会很危险。

比如当前程序就在这个分区运行：

```txt id="nes05z"
app 分区：正在运行的旧固件
```

你 OTA 的时候直接把新固件写进去：

```txt id="k62x4h"
旧固件正在运行
同时又把这个分区擦掉写新固件
```

这就很危险。一旦中途断电，旧固件也没了，新固件也没写完整，设备可能直接变砖。

所以 ESP32 OTA 通常使用“双 APP 分区”：

```txt id="sqo0b4"
factory / ota_0 / ota_1
```

常见分区结构：

```txt id="02cxxe"
┌──────────────────────┐
│ bootloader            │  开机引导程序
├──────────────────────┤
│ partition table       │  分区表
├──────────────────────┤
│ nvs                   │  保存 Wi-Fi 信息、配置等
├──────────────────────┤
│ otadata               │  记录下次启动哪个 APP
├──────────────────────┤
│ factory               │  出厂固件，可选
├──────────────────────┤
│ ota_0                 │  APP 槽位 0
├──────────────────────┤
│ ota_1                 │  APP 槽位 1
└──────────────────────┘
```

ESP-IDF 官方文档里也说明，OTA 安全更新应用程序时，需要至少两个 OTA APP slot，比如 `ota_0` 和 `ota_1`，还需要一个 OTA Data 分区；新固件会写到当前没有被选中启动的 OTA 分区，验证通过后再更新 OTA Data 分区，让 bootloader 下次启动新分区。([Espressif Systems][1])

---

# 3. 可以把它理解成“双系统”

你可以这样理解：

```txt id="5g19vi"
ota_0 = 系统 A
ota_1 = 系统 B
```

当前运行的是 `ota_0`：

```txt id="42iirr"
当前运行：ota_0 旧版本
空闲分区：ota_1
```

OTA 升级时，不会擦当前正在运行的 `ota_0`，而是把新固件写入 `ota_1`：

```txt id="fja01n"
ota_0：旧固件，正在运行
ota_1：写入新固件
```

写完以后，ESP32 修改 `otadata`：

```txt id="1iktby"
下次启动 ota_1
```

然后重启：

```txt id="muukv8"
bootloader 读取 otadata
发现下次应该启动 ota_1
于是运行 ota_1 新固件
```

如果新固件没问题，以后就运行新固件。

下一次再升级时，反过来：

```txt id="jdt4av"
当前运行：ota_1
新固件写入：ota_0
```

所以 OTA 是两个 APP 分区来回切换。

---

# 4. HTTP OTA 的完整工作流程

整体流程如下：

```txt id="6mn4cm"
1. ESP32 正常启动旧 APP
2. 旧 APP 连接 Wi-Fi
3. 旧 APP 访问升级服务器
4. 判断服务器是否有新版本
5. 如果有新版本，开始下载 app.bin
6. 找到当前没有运行的 OTA 分区
7. 边下载，边写入这个空闲 OTA 分区
8. 下载完成后校验固件是否合法
9. 校验通过，修改 otadata
10. 重启 ESP32
11. bootloader 启动新 APP
12. 新 APP 自检成功后，确认升级成功
```

ESP-IDF 官方 OTA 示例也是这个流程：设备通过 Wi-Fi 或以太网下载新镜像，写入 OTA 分区，然后指示 bootloader 下次 reset 后从这个镜像启动。([GitHub][2])

---

# 5. bootloader 在 OTA 里干什么？

很多新手容易误解，以为 OTA 是 bootloader 下载固件。

其实不是。

## 当前 APP 负责：

```txt id="39nscx"
联网
HTTP/HTTPS 下载
写 Flash
校验固件
设置下次启动分区
调用 esp_restart()
```

## bootloader 负责：

```txt id="hkbj5q"
开机
读取 otadata
判断应该启动 ota_0 还是 ota_1
校验 APP 是否能启动
跳转到对应 APP
```

bootloader 不关心你是从 HTTP 下载的，还是蓝牙下载的，还是串口下载的。

它只看：

```txt id="1q0lei"
otadata 里写的是哪个分区
```

比如：

```txt id="db715m"
otadata：下次启动 ota_1
```

那 bootloader 就启动 `ota_1`。

---

# 6. HTTP 下载的数据是什么？

OTA 下载的不是源码，不是 `.elf`，而是编译后的固件：

```txt id="ux779q"
xxx.bin
```

比如你执行：

```bash id="vczv6h"
idf.py build
```

编译后会生成类似：

```txt id="mo3viu"
build/your_project.bin
```

这个 `.bin` 文件就是 OTA 下载的固件。

你的服务器上可能放：

```txt id="zd5wqj"
https://example.com/firmware/v1.2.0/app.bin
```

ESP32 就请求这个文件：

```http id="2gvvba"
GET /firmware/v1.2.0/app.bin HTTP/1.1
Host: example.com
```

服务器返回的是二进制数据：

```txt id="qxkffh"
固件二进制内容
```

ESP32 一边接收，一边写入 Flash。

---

# 7. OTA 写 Flash 的底层过程

如果不用封装好的 `esp_https_ota()`，底层大概是这几个步骤：

```c id="v44ia7"
esp_ota_get_next_update_partition()
esp_ota_begin()
esp_ota_write()
esp_ota_end()
esp_ota_set_boot_partition()
esp_restart()
```

对应意思是：

## 1）找到空闲 OTA 分区

```c id="hipz48"
esp_ota_get_next_update_partition(NULL);
```

比如当前运行 `ota_0`，那它会返回 `ota_1`。

## 2）开始 OTA

```c id="pg78b3"
esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &update_handle);
```

这一步准备写入新固件。

## 3）边下载边写 Flash

```c id="x4lyom"
esp_ota_write(update_handle, data, data_len);
```

这个函数可以调用很多次。你每收到一段 HTTP 数据，就写一段到 Flash。ESP-IDF 文档也说明，`esp_ota_write()` 可以在接收 OTA 数据时多次调用，并且会按顺序写入分区。([Espressif Systems][1])

## 4）结束并校验固件

```c id="8rnsix"
esp_ota_end(update_handle);
```

这一步会检查新写入的 APP 镜像是否合法。官方文档里说明，`esp_ota_end()` 会结束 OTA 更新并验证新写入的 APP image。([Espressif Systems][1])

## 5）设置下次启动分区

```c id="ktjs60"
esp_ota_set_boot_partition(update_partition);
```

这一步修改 `otadata`，告诉 bootloader：

```txt id="7mgj0f"
下次启动这个新分区
```

官方文档说明，`esp_ota_set_boot_partition()` 成功后，调用 `esp_restart()` 就会从新配置的 APP 分区启动。([Espressif Systems][1])

## 6）重启

```c id="jgmn5s"
esp_restart();
```

然后 bootloader 启动新固件。

---

# 8. ESP-IDF 推荐用哪个接口？

ESP-IDF 有两种常见 OTA 写法。

## 方式一：`esp_https_ota`

这是高级封装，简单好用。

你只需要给它一个固件 URL，它帮你完成：

```txt id="q5u7a7"
连接服务器
下载固件
写入 OTA 分区
校验固件
设置启动分区
```

ESP-IDF 官方文档说明，`esp_https_ota` 是在已有 OTA API 上封装出来的简化接口，用于通过 HTTPS 进行固件升级。([Espressif Systems][3])

代码大概是这样：

```c id="5ka0j9"
#include "esp_https_ota.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_system.h"

static const char *TAG = "OTA";

extern const char server_cert_pem_start[] asm("_binary_ca_cert_pem_start");

void ota_task(void *pvParameter)
{
    esp_http_client_config_t http_config = {
        .url = "https://example.com/firmware/app.bin",
        .cert_pem = server_cert_pem_start,
        .timeout_ms = 10000,
    };

    esp_https_ota_config_t ota_config = {
        .http_config = &http_config,
    };

    ESP_LOGI(TAG, "Starting OTA");

    esp_err_t ret = esp_https_ota(&ota_config);

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "OTA success, restarting...");
        esp_restart();
    } else {
        ESP_LOGE(TAG, "OTA failed");
    }

    vTaskDelete(NULL);
}
```

这个最适合新手。

官方文档也明确说，`esp_https_ota()` 会建立 HTTPS 连接、从 HTTP stream 读取镜像数据、写入 OTA 分区并完成升级；如果成功返回，需要调用 `esp_restart()` 启动新固件。([Espressif Systems][3])

---

## 方式二：原生 OTA API

这种更底层，更灵活。

你自己负责：

```txt id="5pttly"
HTTP 连接
数据接收
版本判断
写 Flash
断点续传
进度显示
错误处理
```

底层 API 类似：

```c id="ws71ro"
esp_ota_begin();
esp_ota_write();
esp_ota_end();
esp_ota_set_boot_partition();
```

适合你想自己掌控流程，比如：

```txt id="0ld44x"
显示下载进度
自己做断点续传
自己加密固件
自己做升级包协议
从非 HTTP 通道升级，比如 BLE、串口、SD 卡
```

---

# 9. HTTP OTA 和 HTTPS OTA 有啥区别？

## HTTP OTA

```txt id="qo69m9"
http://example.com/app.bin
```

优点：

```txt id="lfxs0f"
简单
调试方便
不需要证书
```

缺点：

```txt id="u9gev7"
明文传输
中间人可能篡改固件
不适合正式产品
```

## HTTPS OTA

```txt id="3bx4n1"
https://example.com/app.bin
```

优点：

```txt id="xb517b"
加密传输
能校验服务器身份
安全性更好
```

缺点：

```txt id="xqw39y"
需要证书
占用 RAM 更多
调试稍微麻烦
```

正式产品建议：

```txt id="4y7cma"
HTTPS OTA + 固件签名校验
```

ESP-IDF 的 `esp_https_ota` 需要配置服务器证书，官方文档说明需要提供 PEM 格式的 root certificate，也可以使用 ESP x509 certificate bundle。([Espressif Systems][3])

---

# 10. 断电会不会变砖？

正常 APP OTA 不容易变砖，因为它是安全更新模式。

比如当前运行 `ota_0`：

```txt id="9t8j5h"
ota_0：旧固件，正在运行
ota_1：正在写新固件
```

如果写 `ota_1` 的过程中断电：

```txt id="xhurdd"
ota_1：没写完整
ota_0：仍然完整
```

下次重启还是可以启动旧固件。

因为只有当新固件完整下载、校验通过以后，ESP32 才会修改 `otadata`，让 bootloader 下次启动新固件。官方文档也说明，安全更新模式的设计目标就是更新被中断时，芯片仍能继续启动当前应用程序。([Espressif Systems][1])

但是注意：

```txt id="nac5pg"
APP OTA 相对安全
bootloader OTA 风险高
partition table OTA 风险高
```

新手阶段不要升级 bootloader 和分区表，先只做 APP OTA。

---

# 11. 如果新固件有 bug 怎么办？

ESP-IDF 支持 OTA rollback，也就是回滚。

假设：

```txt id="a83ewb"
旧固件 ota_0：稳定
新固件 ota_1：刚升级完成
```

重启后先运行 `ota_1`。

如果 `ota_1` 启动后发现自己有严重问题，比如：

```txt id="4v2g1z"
Wi-Fi 连不上
关键外设初始化失败
程序一直崩溃
无法继续 OTA
```

那可以回滚到旧固件 `ota_0`。

官方文档说明，OTA rollback 的目的就是让设备在升级后仍能保持工作；如果新应用有严重错误，可以标记为 invalid，然后重启回到之前可工作的应用。([Espressif Systems][1])

新固件第一次启动后，通常要做自检：

```txt id="yoolno"
NVS 正常？
Wi-Fi 正常？
关键任务启动成功？
外设初始化成功？
能连接服务器？
```

如果自检成功，就调用：

```c id="8ckep7"
esp_ota_mark_app_valid_cancel_rollback();
```

意思是：

```txt id="uehw9u"
这个新固件没问题，确认升级成功
```

如果自检失败，就调用：

```c id="s2hwk9"
esp_ota_mark_app_invalid_rollback_and_reboot();
```

意思是：

```txt id="ldl7lv"
这个新固件不行，回滚旧固件
```

---

# 12. OTA 的版本判断怎么做？

OTA 不一定每次开机都直接下载完整固件。

更合理的流程是：

```txt id="jtvltv"
ESP32 当前版本：1.0.0
服务器最新版本：1.0.1

如果服务器版本 > 当前版本：
    下载新固件
否则：
    不升级
```

服务器可以放一个 JSON 文件：

```json id="nekrel"
{
  "version": "1.0.1",
  "url": "https://example.com/firmware/app_v1.0.1.bin",
  "size": 1048576,
  "sha256": "xxxxxx"
}
```

ESP32 先请求：

```txt id="1d3xku"
https://example.com/firmware/version.json
```

然后判断是否需要升级。

实际产品常见流程：

```txt id="25lopi"
1. 开机连接 Wi-Fi
2. 请求 version.json
3. 比较版本号
4. 如果有新版本，下载 app.bin
5. 校验 SHA256
6. 设置启动分区
7. 重启
```

---

# 13. OTA 和你之前 HTTP 获取天气很像吗？

底层通信思路像，但数据用途不一样。

获取天气：

```txt id="bq5naq"
HTTP GET → 返回 JSON → cJSON 解析 → 显示天气
```

OTA 升级：

```txt id="g0fffl"
HTTP GET → 返回 app.bin 二进制固件 → 写入 Flash → 重启运行
```

天气数据是文本 JSON：

```json id="5xlcyb"
{"temp": 26.5}
```

OTA 数据是二进制固件：

```txt id="e7uztn"
0xE9 0x06 0x02 0x20 ...
```

所以 OTA 不能用 `cJSON` 解析，而是直接写 Flash。

---

# 14. OTA 需要准备哪些东西？

## ESP32 端需要：

```txt id="d72rlz"
Wi-Fi 连接功能
HTTP/HTTPS 客户端
OTA 分区表
OTA 下载任务
错误处理
重启逻辑
可选：回滚机制
```

## 服务器端需要：

```txt id="45ro2y"
固件 app.bin
版本描述文件 version.json
HTTP/HTTPS 文件下载服务
可选：设备鉴权
可选：灰度升级
```

## 编译端需要：

```txt id="sfi62j"
生成 app.bin
管理版本号
上传固件到服务器
```

---

# 15. 分区表示例

新手可以先用 ESP-IDF 默认的 OTA 分区配置。

也可以自定义 `partitions.csv`，例如：

```csv id="m4oufz"
# Name,   Type, SubType, Offset,  Size, Flags
nvs,      data, nvs,     0x9000,  0x6000,
otadata,  data, ota,     0xf000,  0x2000,
phy_init, data, phy,     0x11000, 0x1000,
factory,  app,  factory, 0x10000, 1M,
ota_0,    app,  ota_0,          , 1M,
ota_1,    app,  ota_1,          , 1M,
```

如果你的固件比较大，比如用了 LVGL、图片、字体、蓝牙、Wi-Fi、HTTPS，很可能 1MB 不够，要改成：

```csv id="95fbg1"
factory,  app,  factory, 0x10000, 2M,
ota_0,    app,  ota_0,          , 2M,
ota_1,    app,  ota_1,          , 2M,
```

但是 Flash 总大小也要够，比如 8MB / 16MB 更舒服。

---

# 16. 新手最容易踩的坑

## 坑 1：分区太小

编译出来的固件是 1.3MB，但是你的 OTA 分区只有 1MB。

结果：

```txt id="vy1269"
OTA 写入失败
```

解决：

```txt id="zuvx9w"
增大 ota_0 / ota_1 分区
使用更大 Flash
优化固件大小
```

---

## 坑 2：下载的不是 `.bin`

有些人把 GitHub 页面地址、网盘预览地址、HTML 页面地址填进去。

ESP32 下载到的不是固件，而是网页 HTML。

结果：

```txt id="kr4p8i"
esp_ota_end() 校验失败
```

你需要保证 URL 直接返回：

```txt id="3nmyx1"
app.bin 的二进制内容
```

---

## 坑 3：HTTPS 证书不对

如果你用 HTTPS，需要配置正确的 CA 证书。

否则会报：

```txt id="z6uhs6"
certificate verify failed
```

开发阶段可以先用 HTTP 或临时关闭校验测试，但正式产品不要这么做。

---

## 坑 4：新固件启动后没有确认 valid

如果启用了 rollback，新固件第一次启动后要调用：

```c id="1wyvso"
esp_ota_mark_app_valid_cancel_rollback();
```

否则可能下次被认为没有验证成功，然后回滚。

---

## 坑 5：OTA 任务栈太小

HTTPS、TLS、HTTP、Flash 写入都需要一定内存。

OTA 任务不要只给 4096，可以先给：

```c id="3a6j0t"
xTaskCreate(ota_task, "ota_task", 8192, NULL, 5, NULL);
```

甚至更大，具体看日志。

---

# 17. OTA 最简理解图

```txt id="9ae4d3"
第一次运行：

┌────────────┐
│ bootloader │
└─────┬──────┘
      ↓
┌────────────┐
│ factory    │  当前运行
└─────┬──────┘
      ↓
连接 Wi-Fi，下载新固件
      ↓
┌────────────┐
│ ota_0      │  写入新固件
└────────────┘
      ↓
修改 otadata：下次启动 ota_0
      ↓
重启


第二次运行：

┌────────────┐
│ bootloader │
└─────┬──────┘
      ↓
读取 otadata
      ↓
┌────────────┐
│ ota_0      │  运行新固件
└────────────┘
```

下一次升级：

```txt id="a7c0en"
当前运行 ota_0
新固件写入 ota_1
重启后运行 ota_1
```

再下一次：

```txt id="6rl65y"
当前运行 ota_1
新固件写入 ota_0
重启后运行 ota_0
```

就是两个 OTA 分区来回切换。

---

# 18. 你作为新手应该怎么入门？

建议你不要一上来自己写底层 `esp_ota_write()`。

按这个路线：

```txt id="bk22ln"
第一步：跑通 ESP-IDF simple_ota_example
第二步：把 URL 改成你自己服务器上的 app.bin
第三步：确认能下载、写入、重启
第四步：加入版本检测 version.json
第五步：加入进度显示
第六步：加入 rollback
第七步：正式产品改 HTTPS + 证书 + 固件签名
```

ESP-IDF 官方 OTA example 也说明，ESP-IDF 提供两类 OTA 方法：一种是 `app_update` 组件的 native APIs，另一种是 `esp_https_ota` 的简化 API；官方示例里 `simple_ota_example`、`advanced_https_ota` 等就是基于这个组件做的。([GitHub][2])

---

# 19. 一句话总结

ESP32 HTTP OTA 的核心原理就是：

```txt id="x1wm9a"
当前 APP 通过 HTTP/HTTPS 下载新固件，
写入另一个 OTA APP 分区，
校验成功后修改 otadata，
重启后 bootloader 启动新固件。
```

它最重要的三个东西是：

```txt id="q4wszs"
1. 双 APP 分区：ota_0 / ota_1
2. otadata：记录下次启动哪个 APP
3. bootloader：开机时根据 otadata 选择 APP
```

新手记住这句话就够了：

```txt id="9412ow"
OTA 不是把当前运行的程序覆盖掉，而是把新程序写到另一个分区，确认没问题后再切过去。
```

[1]: https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/ota.html "Over The Air Updates (OTA) - ESP32 -  — ESP-IDF Programming Guide v6.0.1 documentation"
[2]: https://github.com/espressif/esp-idf/blob/master/examples/system/ota/README.md "esp-idf/examples/system/ota/README.md at master · espressif/esp-idf · GitHub"
[3]: https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/esp_https_ota.html "ESP HTTPS OTA - ESP32 -  — ESP-IDF Programming Guide v6.0.1 documentation"


---------
------------
-----------

ESP32 的 **HTTP OTA 升级**，本质上就是：

> ESP32 正在运行老固件时，通过 Wi-Fi 从服务器下载一个新的 `app.bin`，写到 Flash 里另一个备用 APP 分区，然后重启。重启后 Bootloader 不再启动老 APP，而是启动新 APP。

官方 ESP-IDF 的 OTA 机制要求 Flash 分区表里通常要有至少两个 OTA APP 分区，比如 `ota_0`、`ota_1`，还要有一个 `otadata` 分区，用来记录下次该启动哪个 APP 分区。OTA 时，新固件会写入“当前没在运行”的那个 APP 分区，校验成功后再更新 `otadata`，让下次启动切到新固件。([Espressif Systems][1])

---

## 1. 先用大白话理解 OTA

你可以把 ESP32 的 Flash 想象成有几个房间：

```text
Flash
├── bootloader          启动程序
├── partition table     分区表
├── nvs                 存 Wi-Fi 配置、参数
├── otadata             记录现在/下次启动哪个 APP
├── factory             出厂固件，可选
├── ota_0               APP 房间 A
└── ota_1               APP 房间 B
```

假设现在 ESP32 正在运行 `ota_0` 里的老程序：

```text
当前运行：ota_0
准备升级：把新固件下载到 ota_1
```

升级成功后，ESP32 不会立刻在运行中把自己变成新程序，而是：

```text
1. 老 APP 正在 ota_0 运行
2. 下载 new_app.bin
3. 写入 ota_1
4. 校验 ota_1 里的固件是否完整、合法
5. 修改 otadata：告诉 Bootloader 下次启动 ota_1
6. esp_restart()
7. Bootloader 读 otadata
8. 启动 ota_1 的新 APP
```

所以 OTA 不是“边运行边替换当前代码”，而是 **运行 A，更新 B；重启后切到 B**。

---

## 2. Bootloader 在 OTA 里负责什么？

很多新手容易误会，以为 OTA 是 Bootloader 去联网下载固件。

正常 ESP-IDF OTA 流程里：

```text
联网下载固件：由当前 APP 完成
写入 Flash：由当前 APP 调 OTA API 完成
选择下次启动哪个 APP：当前 APP 修改 otadata
真正切换 APP：重启后 Bootloader 负责
```

Bootloader 主要干这几件事：

```text
上电/重启
  ↓
读取 partition table
  ↓
读取 otadata
  ↓
判断应该启动 factory / ota_0 / ota_1
  ↓
校验 APP 镜像
  ↓
跳转到对应 APP
```

`otadata` 分区就是 OTA 的“启动选择记录本”。官方文档也说明，首次启动时如果 `otadata` 为空，Bootloader 会优先启动 factory 分区；第一次 OTA 后，`otadata` 会记录下一次应该启动哪个 OTA APP 分区。([Espressif Systems][1])

---

## 3. HTTP OTA 的完整工作流程

假设你的服务器上有一个新固件：

```text
http://example.com/firmware/app_v2.bin
```

ESP32 当前运行的是旧固件，它收到升级指令后开始 OTA。

### 第一步：ESP32 先联网

比如先连接 Wi-Fi：

```text
nvs_flash_init()
esp_netif_init()
esp_event_loop_create_default()
example_connect()
```

这一步和你之前学的 Wi-Fi Station TCP/HTTP 示例类似，必须先让 ESP32 有 IP 地址。

---

### 第二步：APP 发起 HTTP GET 请求

ESP32 访问服务器：

```http
GET /firmware/app_v2.bin HTTP/1.1
Host: example.com
```

服务器返回：

```text
HTTP/1.1 200 OK
Content-Type: application/octet-stream
Content-Length: xxxx

这里开始就是 app.bin 的二进制数据
```

ESP32 不是一次性把整个固件读进 RAM，因为固件可能几百 KB 到几 MB，RAM 不够。

它会一小块一小块读，比如每次读 1024、2048、4096 字节。

```text
HTTP 收到 4KB → 写入 Flash
HTTP 再收 4KB → 继续写入 Flash
HTTP 再收 4KB → 继续写入 Flash
...
直到整个 app.bin 下载完
```

---

### 第三步：选择要写入的 OTA 分区

如果当前运行的是 `ota_0`，那新固件一般写到 `ota_1`。

如果当前运行的是 `ota_1`，那新固件一般写到 `ota_0`。

ESP-IDF 里通常用：

```c
const esp_partition_t *update_partition =
    esp_ota_get_next_update_partition(NULL);
```

官方文档里说明，`esp_ota_get_next_update_partition()` 会返回下一个适合写入新固件的 OTA APP 分区，并且不会返回当前正在运行的分区。([Espressif Systems][1])

---

### 第四步：开始 OTA 写入

典型底层流程是：

```c
esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &update_handle);
```

这一步可以理解为：

```text
我要开始升级了
目标分区是 ota_1
请准备好这个分区
该擦除的地方先擦除
给我一个 OTA 写入句柄 update_handle
```

官方文档说明，`esp_ota_begin()` 会开始向指定分区写 OTA，并根据镜像大小擦除对应 Flash 区域；如果传 `OTA_SIZE_UNKNOWN`，则可能擦除整个目标分区。([Espressif Systems][1])

---

### 第五步：边下载，边写 Flash

每收到一块数据，就调用：

```c
esp_ota_write(update_handle, data, data_len);
```

逻辑类似：

```text
while (还没下载完) {
    data = http_read();
    esp_ota_write(update_handle, data, data_len);
}
```

官方文档说明，`esp_ota_write()` 可以在 OTA 过程中被多次调用，每次把收到的数据顺序写入目标分区。([Espressif Systems][1])

---

### 第六步：结束 OTA 并校验固件

下载完后调用：

```c
esp_ota_end(update_handle);
```

这一步非常关键，它不是简单地“关闭文件”，而是会检查新写进去的 APP 镜像是否合法。

比如检查：

```text
固件头是否正确
镜像格式是否正确
写入是否完整
安全启动情况下签名是否有效
```

官方文档说明，`esp_ota_end()` 会完成 OTA 并校验新写入的 APP 镜像，如果镜像非法会返回 `ESP_ERR_OTA_VALIDATE_FAILED`。([Espressif Systems][1])

---

### 第七步：设置下次启动新分区

如果新固件校验通过，就调用：

```c
esp_ota_set_boot_partition(update_partition);
```

这一步会修改 `otadata`：

```text
下次启动 ota_1
```

官方文档说明，如果 `esp_ota_set_boot_partition()` 返回 `ESP_OK`，调用 `esp_restart()` 后就会启动新配置的 APP 分区。([Espressif Systems][1])

然后：

```c
esp_restart();
```

---

### 第八步：重启后 Bootloader 启动新 APP

重启之后：

```text
Bootloader 启动
  ↓
读取 otadata
  ↓
发现下次应该启动 ota_1
  ↓
校验 ota_1
  ↓
跳转到 ota_1
  ↓
新固件开始运行
```

---

## 4. 整体流程图

```text
老 APP 正在运行
      │
      ▼
收到升级指令
      │
      ▼
连接 Wi-Fi / Ethernet
      │
      ▼
HTTP / HTTPS 请求服务器上的 app.bin
      │
      ▼
找到空闲 OTA 分区
比如当前运行 ota_0，就选择 ota_1
      │
      ▼
esp_ota_begin()
擦除目标 OTA 分区
      │
      ▼
循环下载固件数据
HTTP read 一块
      │
      ▼
esp_ota_write()
写入 Flash
      │
      ▼
固件下载完成
      │
      ▼
esp_ota_end()
校验新固件
      │
      ▼
esp_ota_set_boot_partition()
修改 otadata
      │
      ▼
esp_restart()
      │
      ▼
Bootloader 读取 otadata
      │
      ▼
启动新 APP
```

---

## 5. ESP-IDF 里有两种 OTA 写法

ESP-IDF 官方 OTA 示例说明有两种方式：一种是直接使用 `app_update` 组件的原生 OTA API；另一种是使用 `esp_https_ota` 组件提供的简化 API。([GitHub][2])

### 方式一：底层原生 API

你自己控制 HTTP 下载流程：

```c
esp_ota_begin()
esp_ota_write()
esp_ota_end()
esp_ota_set_boot_partition()
esp_restart()
```

优点：

```text
灵活
适合你自己封装 HTTP、断点续传、版本判断、加密协议
```

缺点：

```text
代码多
新手容易漏掉校验、错误处理、证书处理
```

---

### 方式二：esp_https_ota 简化 API

官方推荐 HTTPS OTA 可以这样：

```c
esp_http_client_config_t config = {
    .url = CONFIG_FIRMWARE_UPGRADE_URL,
    .cert_pem = (char *)server_cert_pem_start,
};

esp_https_ota_config_t ota_config = {
    .http_config = &config,
};

esp_err_t ret = esp_https_ota(&ota_config);
if (ret == ESP_OK) {
    esp_restart();
}
```

官方文档说明，`esp_https_ota` 是对现有 OTA API 的一层封装，用来简化 HTTPS 固件升级。([Espressif Systems][3])

对新手来说，建议先跑：

```text
esp-idf/examples/system/ota/simple_ota_example
esp-idf/examples/system/ota/advanced_https_ota
```

然后再把你的 LCD、LVGL、传感器、业务逻辑慢慢迁进去。

---

## 6. HTTP 和 HTTPS 的区别

你问的是 HTTP OTA，但实际产品里更建议 HTTPS OTA。

### HTTP OTA

```text
优点：
简单
测试方便
局域网服务器容易搭

缺点：
不加密
容易被中间人攻击
别人可能替换你的固件
不适合正式产品
```

### HTTPS OTA

```text
优点：
传输加密
可以校验证书
更适合产品发布

缺点：
证书配置麻烦一点
占用 RAM 稍高
服务器要配置 HTTPS
```

官方 `esp_https_ota` 文档也强调了服务器证书校验，通常需要给 `esp_http_client_config_t::cert_pem` 提供根证书，或者使用 ESP x509 Certificate Bundle。([Espressif Systems][3])

---

## 7. 为什么 OTA 至少需要两个 APP 分区？

因为 ESP32 不能一边运行当前 APP，一边把当前 APP 所在的 Flash 区域擦掉。

比如：

```text
当前程序正在 ota_0 运行
你不能把 ota_0 擦掉再写新程序
否则程序自己把自己干掉了
```

所以必须这样：

```text
当前运行 ota_0
新固件写 ota_1

当前运行 ota_1
新固件写 ota_0
```

这就是 A/B 分区升级。

官方文档也说，APP OTA 是 safe update 模式，新固件写入当前未被选择启动的 OTA APP slot；验证后再更新 OTA Data 分区，指定下一次启动新镜像。([Espressif Systems][1])

---

## 8. 分区表示例

常见 OTA 分区表大概长这样：

```csv
# Name,   Type, SubType, Offset,   Size
nvs,      data, nvs,     0x9000,   0x6000
otadata,  data, ota,     0xf000,   0x2000
phy_init, data, phy,     0x11000,  0x1000
factory,  app,  factory, 0x10000,  1M
ota_0,    app,  ota_0,   0x110000, 1M
ota_1,    app,  ota_1,   0x210000, 1M
```

这里重点是：

```text
factory：出厂 APP，可选
ota_0：OTA APP 分区 0
ota_1：OTA APP 分区 1
otadata：记录当前/下次启动哪个 OTA 分区
```

`otadata` 分区在 ESP-IDF 里通常是 `data, ota` 类型，官方文档说明它存储当前选中的 OTA APP slot 信息，推荐大小是 `0x2000` 字节。([Espressif Systems][4])

---

## 9. OTA 升级失败会不会变砖？

正常 APP OTA 设计得比较安全。

因为升级过程中：

```text
老 APP 还在 ota_0
新 APP 写到 ota_1
```

如果写到一半断电：

```text
ota_1 里的新 APP 是坏的
但 ota_0 老 APP 还在
otadata 还没切过去
下次仍然启动老 APP
```

所以一般不会变砖。

但是有几个危险情况：

```text
1. 分区表设计错了
2. ota_0 / ota_1 太小，新固件写不下
3. 强行 OTA 更新 bootloader 或 partition table
4. 没做镜像校验
5. 使用 HTTP，被人替换恶意固件
6. 新固件启动后立刻崩溃，又没开 rollback
```

官方 OTA 示例也区分了 safe update 和 unsafe update：APP 分区 OTA 属于安全更新；Bootloader、分区表、某些数据分区更新属于不安全更新，更新中断可能导致设备不可恢复。([GitHub][2])

---

## 10. Rollback 回滚机制是干啥的？

假设新固件下载成功，也能启动，但启动后马上崩溃。

这时候如果没有回滚机制，设备可能会一直启动坏 APP，反复重启。

ESP-IDF 支持 OTA rollback。大概流程是：

```text
1. 新 APP 下载完成
2. 设置新 APP 为下次启动
3. 重启进入新 APP
4. 新 APP 第一次启动时状态是待验证
5. 新 APP 自检成功后，调用 esp_ota_mark_app_valid_cancel_rollback()
6. 如果新 APP 自检失败或崩溃，Bootloader 可回滚到旧 APP
```

官方文档说明，启用 `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE` 后，如果新 APP 第一次启动没有确认有效，发生重置就会回滚；新 APP 应该尽快完成自检，然后调用 `esp_ota_mark_app_valid_cancel_rollback()` 确认可用。([Espressif Systems][1])

你可以理解为：

```text
新固件启动后要对 Bootloader 说一句：
“我没问题，可以正式用我了。”
```

如果它没来得及说这句话就死了，Bootloader 下次就可以换回老固件。

---

## 11. 最小伪代码理解

底层原生 OTA 大概是这样：

```c
void ota_task(void *arg)
{
    // 1. 找到下一个 OTA 分区
    const esp_partition_t *update_partition =
        esp_ota_get_next_update_partition(NULL);

    // 2. 开始 OTA
    esp_ota_handle_t ota_handle;
    esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &ota_handle);

    // 3. HTTP 连接服务器，下载 app.bin
    while (1) {
        int len = http_read(buffer, sizeof(buffer));

        if (len < 0) {
            // 下载出错
            esp_ota_abort(ota_handle);
            return;
        }

        if (len == 0) {
            // 下载完成
            break;
        }

        // 4. 写入 Flash
        esp_ota_write(ota_handle, buffer, len);
    }

    // 5. 结束 OTA，校验固件
    if (esp_ota_end(ota_handle) != ESP_OK) {
        // 固件非法
        return;
    }

    // 6. 设置下次启动新固件
    if (esp_ota_set_boot_partition(update_partition) != ESP_OK) {
        return;
    }

    // 7. 重启
    esp_restart();
}
```

---

## 12. 对你做 ESP32 + LCD + LVGL 项目的建议

你的项目里有 LCD、LVGL、按键、温湿度、音频等模块，建议 OTA 模块独立出来：

```text
main/
├── app_main.c
├── wifi/
│   ├── wifi_app.c
│   └── wifi_app.h
├── ota/
│   ├── app_ota.c
│   └── app_ota.h
├── lcd/
├── lvgl_ui/
├── sensor/
└── audio/
```

正常运行时：

```text
app_main
  ↓
初始化 Wi-Fi
  ↓
初始化 LCD / LVGL
  ↓
初始化业务任务
  ↓
等待升级指令
  ↓
创建 ota_task
  ↓
下载固件并升级
```

OTA 最好单独一个任务，不要放在 LVGL 定时器回调、按键回调、HTTP 回调里直接干。

---

## 13. 新手最容易踩的坑

### 坑 1：OTA 分区太小

你的 `app.bin` 如果是 1.3MB，但 `ota_0` / `ota_1` 只有 1MB，肯定失败。

所以要看：

```text
build/xxx.bin 大小
partition.csv 里 ota_0 / ota_1 大小
```

---

### 坑 2：以为 OTA 会更新 bootloader

普通 APP OTA 只更新 APP 分区，不更新 bootloader、partition table、NVS。

也就是说：

```text
OTA 更新的是你的应用程序 app.bin
不是整个 flash
```

---

### 坑 3：HTTP URL 下载的不是原始 bin 文件

有时候你把固件放 GitHub、网盘、对象存储上，URL 返回的可能不是 `app.bin`，而是 HTML 页面、跳转页面、鉴权失败页面。

ESP32 写进去后校验就会失败。

你要确保 HTTP 返回的就是：

```text
纯 app.bin 二进制数据
```

---

### 坑 4：忘了 HTTPS 证书

HTTPS OTA 时，证书不对会连接失败。

正式产品建议用：

```text
服务器根证书 cert_pem
或者 ESP x509 Certificate Bundle
```

---

### 坑 5：新固件启动后没确认有效

如果你开启了 rollback，新固件第一次启动后要在自检成功后调用：

```c
esp_ota_mark_app_valid_cancel_rollback();
```

否则可能被认为没有确认成功，后续发生异常重启时会回滚。

---

## 14. 一句话总结

ESP32 HTTP OTA 的核心就是：

```text
当前 APP 从服务器下载 new_app.bin
        ↓
写到另一个 OTA 分区
        ↓
校验新固件
        ↓
修改 otadata
        ↓
重启
        ↓
Bootloader 启动新分区
```

更形象地说：

```text
不要拆正在住的房子。
你现在住 ota_0，就把新房子 ota_1 装修好。
确认 ota_1 没问题后，重启搬过去住。
下次升级时，再反过来把 ota_0 装修成新版本。
```

[1]: https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/ota.html "Over The Air Updates (OTA) - ESP32 -  — ESP-IDF Programming Guide v6.0.1 documentation"
[2]: https://github.com/espressif/esp-idf/blob/master/examples/system/ota/README.md "esp-idf/examples/system/ota/README.md at master · espressif/esp-idf · GitHub"
[3]: https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/esp_https_ota.html "ESP HTTPS OTA - ESP32 -  — ESP-IDF Programming Guide v6.0.1 documentation"
[4]: https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/partition-tables.html "Partition Tables - ESP32 -  — ESP-IDF Programming Guide v6.0.1 documentation"


----------
----------
----------

# 附录
## ESP factory介绍
`factory, app, factory, 0x10000, 1M` 这一行的意思是：

```csv
# 名字      类型   子类型     起始地址    大小
factory,   app,   factory,   0x10000,   1M
```

也就是：

> 在 Flash 的 `0x10000` 地址处，放一个 **出厂默认 APP 固件**，大小是 `1MB`。

官方 ESP-IDF 文档里也说，`factory` 是一个默认 APP 分区，Bootloader 默认会运行这个分区里的程序；如果用了 OTA，则通常还会有 `otadata` 来决定启动哪个 OTA 分区。([Espressif Systems][1])

---

## 1. `factory` 是什么？

你可以把它理解成：

```text
factory = 出厂固件 / 保底固件 / 初始固件
```

比如你的产品第一次烧录出厂时，把程序烧到 `factory` 里面。

```text
ESP32 Flash
├── bootloader
├── partition table
├── nvs
├── otadata
├── factory   ← 出厂默认程序
├── ota_0     ← OTA 升级槽位 0
└── ota_1     ← OTA 升级槽位 1
```

第一次启动时，如果还没有 OTA 记录，Bootloader 一般会先启动 `factory`。

---

## 2. 那 `factory` 和 `ota_0 / ota_1` 有啥区别？

核心区别是：

```text
factory：出厂默认 APP，通常不被 OTA 更新
ota_0：OTA 升级用的 APP 槽位
ota_1：OTA 升级用的 APP 槽位
```

官方文档里明确提到，OTA 需要至少两个 OTA APP slot，也就是 `ota_0` 和 `ota_1`，再加一个 OTA Data 分区；OTA 时，新固件会写入当前没有被选中启动的 OTA slot。([Espressif Systems][2])

---

## 3. 用房子比喻

你可以这样理解：

```text
factory = 出厂老房子
ota_0   = 新房子 A
ota_1   = 新房子 B
```

刚买来设备时：

```text
住在 factory
```

第一次 OTA 升级：

```text
把新固件写到 ota_0
重启后住到 ota_0
```

第二次 OTA 升级：

```text
当前住 ota_0
把新固件写到 ota_1
重启后住到 ota_1
```

第三次 OTA 升级：

```text
当前住 ota_1
把新固件写到 ota_0
重启后住到 ota_0
```

所以 OTA 主要是在 `ota_0` 和 `ota_1` 之间来回切换。

---

## 4. `factory` 会不会被 OTA 覆盖？

一般不会。

正常 OTA 流程是：

```text
当前运行 ota_0 → 新固件写 ota_1
当前运行 ota_1 → 新固件写 ota_0
```

`factory` 一般作为出厂保底固件，不参与普通 OTA 轮换。ESP-IDF 文档也提到，OTA 不会更新 `factory` 分区；如果想节省 Flash，可以移除 `factory`，直接用 `ota_0`。([Espressif Systems][3])

---

## 5. `factory` 有啥用？

它主要有几个作用。

### 作用 1：出厂第一次启动

设备刚烧录时，可能还没有 OTA 信息。

这时 Bootloader 可以启动：

```text
factory
```

也就是出厂程序。

---

### 作用 2：恢复出厂固件

如果你的 OTA 程序坏了，或者用户想恢复出厂状态，可以设计成回到 `factory`。

比如：

```text
长按某个按键 10 秒
  ↓
清除 otadata
  ↓
重启
  ↓
Bootloader 回到 factory
```

官方 Bootloader 文档也提到，可以把一个基础可工作的固件放在 factory 分区，OTA 后固件会进入 OTA app slot；如果想回到出厂固件，可以配置 factory reset 机制。([Espressif Systems][4])

---

### 作用 3：做一个“保底救砖程序”

比如你把 `factory` 做得很简单：

```text
factory 程序只负责：
1. 连接 Wi-Fi
2. 重新 OTA 下载正常固件
3. 恢复设备
```

这样即使 `ota_0 / ota_1` 出问题，也有一个基础恢复入口。

---

## 6. app1、app2 是啥？

你说的 `app1 / app2`，一般就是别人通俗叫法。

在 ESP-IDF 里更标准的名字是：

```text
ota_0
ota_1
```

有些人会叫：

```text
app0 / app1
app1 / app2
A 分区 / B 分区
OTA A / OTA B
```

本质都是：

```text
两个 OTA APP 槽位
```

比如：

```csv
ota_0, app, ota_0, 0x110000, 1M
ota_1, app, ota_1, 0x210000, 1M
```

这两个才是 OTA 升级来回切换的主力。

---

## 7. 有没有 `factory` 的 OTA 分区表

比如这种：

```csv
nvs,      data, nvs,     0x9000,   0x6000
otadata,  data, ota,     0xf000,   0x2000
phy_init, data, phy,     0x11000,  0x1000
factory,  app,  factory, 0x10000,  1M
ota_0,    app,  ota_0,   0x110000, 1M
ota_1,    app,  ota_1,   0x210000, 1M
```

这个结构是：

```text
factory + ota_0 + ota_1
```

优点：

```text
有出厂保底固件
更安全
方便恢复出厂
```

缺点：

```text
占用 Flash 多
每个 APP 分区可能只能分到 1MB
你的 LVGL / 音频 / OTA / AI 语音项目可能不够放
```

---

## 8. 没有 `factory` 的 OTA 分区表

也可以这样：

```csv
nvs,      data, nvs,     0x9000,   0x6000
otadata,  data, ota,     0xf000,   0x2000
phy_init, data, phy,     0x11000,  0x1000
ota_0,    app,  ota_0,   0x10000,  1800K
ota_1,    app,  ota_1,   ,         1800K
```

这时没有 `factory`。

第一次烧录时，程序可以直接烧到 `ota_0`。

ESP-IDF 配置里也有“两块大 OTA 分区”的方案，两个 APP 分区都更大，适合固件体积较大的项目。官方配置说明里提到，`Factory app, two OTA definitions` 是 `factory + 两个 OTA app`，而 `Two large size OTA partitions` 是两个更大的 OTA app 分区。([Espressif Systems][5])

---

## 9. 对你来说该怎么选？

如果你现在做的是：

```text
ESP32-S3
LCD
LVGL
Wi-Fi
HTTP OTA
音频
AI 语音助手
```

你的固件可能会比较大。

我建议你优先考虑：

```text
不要 factory
只保留 ota_0 + ota_1
```

因为 `factory` 会额外吃掉一块 APP 空间。

比如 4MB Flash，如果用：

```text
factory 1M
ota_0   1M
ota_1   1M
```

那每个 APP 只有 1MB，LVGL 项目可能很快不够。

如果不用 factory：

```text
ota_0   约 1.8MB
ota_1   约 1.8MB
```

你的 APP 空间会宽松很多。

---

## 10. 最直观对比

| 分区        | 标准名字         | 作用            | OTA 会不会写它 |
| --------- | ------------ | ------------- | --------- |
| `factory` | 出厂 APP       | 第一次启动/恢复出厂/救砖 | 通常不会      |
| `ota_0`   | OTA APP 槽位 0 | OTA 升级运行分区 A  | 会         |
| `ota_1`   | OTA APP 槽位 1 | OTA 升级运行分区 B  | 会         |
| `otadata` | OTA 数据分区     | 记录当前启动哪个 APP  | 会更新启动记录   |

---

## 11. 一句话总结

```text
factory 是出厂默认固件。
ota_0 / ota_1 是 OTA 升级来回切换的两个 APP 分区。
```

更通俗地说：

```text
factory = 原厂保底系统
ota_0   = 升级系统 A
ota_1   = 升级系统 B
```

实际产品里：

```text
想要恢复出厂/保底救砖：保留 factory
想要省 Flash、APP 更大：去掉 factory，只用 ota_0 + ota_1
```

[1]: https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/partition-tables.html?utm_source=chatgpt.com "Partition Tables - ESP32 - — ESP-IDF Programming Guide ..."
[2]: https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/ota.html?utm_source=chatgpt.com "Over The Air Updates (OTA) - ESP32"
[3]: https://docs.espressif.com/projects/esp-idf/en/v4.0.3/api-guides/partition-tables.html?utm_source=chatgpt.com "Partition Tables - - — ESP-IDF Programming Guide v4.0.3 ..."
[4]: https://docs.espressif.com/projects/esp-idf/en/v4.2.1/esp32/api-guides/bootloader.html?utm_source=chatgpt.com "Bootloader - ESP32 - — ESP-IDF Programming Guide v4. ..."
[5]: https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/kconfig-reference.html?utm_source=chatgpt.com "Configuration Options Reference - ESP32 - — ESP-IDF ..."

----------
------------
## ESP partition table介绍
`partition table` 就是 **Flash 分区表**。

大白话说：

> ESP32 的 Flash 是一整块存储空间，分区表就是告诉 Bootloader 和程序：
> 哪一段 Flash 放 bootloader，哪一段放 APP，哪一段放 NVS，哪一段放 OTA 固件。

你可以把 Flash 想象成一块硬盘，分区表就像硬盘分区说明书。

---

# 1. ESP32 Flash 里不是乱放东西的

比如一个 4MB Flash：

```text
ESP32 Flash 4MB
0x000000 ───────────────────────────────
         bootloader
         partition table
         nvs
         otadata
         app / ota_0 / ota_1
         spiffs / fatfs
0x400000 ───────────────────────────────
```

Bootloader 启动后，必须知道：

```text
APP 在哪里？
NVS 在哪里？
OTA 信息在哪里？
文件系统在哪里？
```

这些信息就来自 **partition table 分区表**。

---

# 2. 分区表本质是啥？

它本质上就是一个表格。

比如：

```csv
# Name,    Type, SubType, Offset,   Size
nvs,       data, nvs,     0x9000,   0x6000
otadata,   data, ota,     0xf000,   0x2000
phy_init,  data, phy,     0x11000,  0x1000
factory,   app,  factory, 0x10000,  1M
ota_0,     app,  ota_0,   0x110000, 1M
ota_1,     app,  ota_1,   0x210000, 1M
```

这个表告诉 ESP32：

```text
nvs      从 0x9000 开始，大小 0x6000
otadata  从 0xf000 开始，大小 0x2000
factory  从 0x10000 开始，大小 1MB
ota_0    从 0x110000 开始，大小 1MB
ota_1    从 0x210000 开始，大小 1MB
```

---

# 3. 每一列是什么意思？

看这一行：

```csv
factory, app, factory, 0x10000, 1M
```

含义是：

| 字段        | 含义                  |
| --------- | ------------------- |
| `factory` | 分区名字                |
| `app`     | 分区类型，表示这里放的是 APP 程序 |
| `factory` | 子类型，表示这是出厂 APP      |
| `0x10000` | 起始地址                |
| `1M`      | 分区大小                |

再看这个：

```csv
nvs, data, nvs, 0x9000, 0x6000
```

含义是：

| 字段       | 含义         |
| -------- | ---------- |
| `nvs`    | 分区名字       |
| `data`   | 数据分区       |
| `nvs`    | 用来存 NVS 参数 |
| `0x9000` | 起始地址       |
| `0x6000` | 大小         |

---

# 4. 分区表放在哪里？

ESP32 默认情况下：

```text
bootloader        在 0x0000
partition table   在 0x8000
app               通常从 0x10000 开始
```

大概是这样：

```text
Flash 起始
│
├── 0x0000   bootloader
│
├── 0x8000   partition table
│
├── 0x9000   nvs
│
├── 0xF000   otadata
│
└── 0x10000  app / factory / ota_0
```

所以你经常会看到 ESP32 烧录地址：

```text
0x1000   bootloader.bin
0x8000   partition-table.bin
0x10000  app.bin
```

有些芯片/配置 bootloader 地址可能是 `0x1000`，你先记住核心逻辑就行：

> Bootloader 先启动，然后读取 `0x8000` 附近的分区表，再根据分区表找到 APP。

---

# 5. Bootloader 为什么需要分区表？

因为 Bootloader 自己不知道你的 APP 放在哪里。

启动流程大概是：

```text
ESP32 上电
  ↓
ROM Bootloader 启动
  ↓
加载二级 Bootloader
  ↓
二级 Bootloader 读取 partition table
  ↓
查看有哪些 APP 分区
  ↓
根据 otadata 判断启动 factory / ota_0 / ota_1
  ↓
跳转到对应 APP
```

如果没有分区表，Bootloader 就不知道：

```text
你的 APP 在 0x10000？
还是在 0x110000？
还是在 0x210000？
```

---

# 6. 常见分区类型

ESP32 常见分区有这些：

```text
app 类型：
factory     出厂 APP
ota_0       OTA APP 0
ota_1       OTA APP 1

data 类型：
nvs         存参数
otadata     存 OTA 启动信息
phy_init    Wi-Fi PHY 初始化数据
spiffs      文件系统
fat         FAT 文件系统
coredump    崩溃转储
```

---

# 7. `nvs` 是干啥的？

`nvs` 是非易失性存储，常用来存：

```text
Wi-Fi SSID
Wi-Fi 密码
设备配置
校准参数
用户设置
蓝牙配对信息
```

比如你的代码里经常有：

```c
nvs_flash_init();
```

它初始化的就是这个 `nvs` 分区。

---

# 8. `otadata` 是干啥的？

`otadata` 是 OTA 升级非常关键的分区。

它记录：

```text
当前应该启动 ota_0 还是 ota_1
新固件是否待验证
是否需要回滚
```

比如当前运行 `ota_0`，升级后写入 `ota_1`。

升级成功后：

```text
otadata 记录：下次启动 ota_1
```

重启后 Bootloader 读取 `otadata`，然后启动 `ota_1`。

---

# 9. `factory / ota_0 / ota_1` 是干啥的？

这几个都是放 APP 程序的。

```text
factory = 出厂默认程序
ota_0   = OTA 程序槽位 0
ota_1   = OTA 程序槽位 1
```

如果你用 OTA：

```text
当前运行 ota_0
新固件写 ota_1
重启后启动 ota_1
```

下一次：

```text
当前运行 ota_1
新固件写 ota_0
重启后启动 ota_0
```

---

# 10. 分区表和 OTA 的关系

OTA 必须依赖分区表。

因为 OTA 需要知道：

```text
当前 APP 在哪个分区？
下一个可写的 OTA 分区在哪里？
每个 OTA 分区有多大？
otadata 在哪里？
```

比如你的分区表是：

```csv
ota_0, app, ota_0, 0x10000,  1800K
ota_1, app, ota_1, 0x1D0000, 1800K
```

那么 OTA 时：

```text
如果当前运行 ota_0：
    新固件写入 ota_1

如果当前运行 ota_1：
    新固件写入 ota_0
```

如果分区表里没有 `ota_0 / ota_1`，那普通双分区 OTA 就没法做。

---

# 11. 分区大小为什么重要？

比如你分区表写：

```csv
ota_0, app, ota_0, 0x10000, 1M
ota_1, app, ota_1, 0x110000, 1M
```

那每个 APP 分区只有 `1MB`。

如果你的 `app.bin` 编译出来是 `1.3MB`，就会失败：

```text
固件太大
写不进 ota_0 / ota_1
```

所以你做：

```text
LVGL
LCD
Wi-Fi
音频
AI 语音助手
OTA
```

这种项目时，APP 很可能比较大，要特别注意 `ota_0 / ota_1` 的大小。

---

# 12. 一个适合 OTA 的简单分区表

如果你是 4MB Flash，又想做 OTA，可以类似这样：

```csv
# Name,    Type, SubType, Offset,   Size
nvs,       data, nvs,     0x9000,   0x6000
otadata,   data, ota,     0xf000,   0x2000
phy_init,  data, phy,     0x11000,  0x1000
ota_0,     app,  ota_0,   0x20000,  1856K
ota_1,     app,  ota_1,   ,         1856K
```

这里没有 `factory`，把空间尽量留给 `ota_0` 和 `ota_1`。

如果你想要出厂保底程序，可以用：

```csv
# Name,    Type, SubType, Offset,   Size
nvs,       data, nvs,     0x9000,   0x6000
otadata,   data, ota,     0xf000,   0x2000
phy_init,  data, phy,     0x11000,  0x1000
factory,   app,  factory, 0x10000,  1M
ota_0,     app,  ota_0,   0x110000, 1M
ota_1,     app,  ota_1,   0x210000, 1M
```

但这个每个 APP 只有 1MB，项目大了容易不够。

---

# 13. 分区表是谁生成的？

你在工程里一般会有一个 CSV 文件：

```text
partitions.csv
```

或者使用 ESP-IDF 默认分区表。

编译时，ESP-IDF 会把它转换成：

```text
partition-table.bin
```

烧录时会把它烧到 Flash 的分区表位置。

烧录日志里你可能会看到类似：

```text
bootloader.bin       → 0x1000
partition-table.bin  → 0x8000
app.bin              → 0x10000
```

---

# 14. 新手最容易误解的点

## 误解 1：分区表是代码吗？

不是。

分区表不是 C 代码，它是 Flash 空间布局配置。

它告诉系统：

```text
哪个地址放什么东西
每块区域多大
每块区域是什么用途
```

---

## 误解 2：APP 只能从 0x10000 启动吗？

不是绝对的。

普通 factory APP 常放 `0x10000`。

但 OTA APP 可以放在：

```text
0x110000
0x210000
```

或者其他地址。

具体看你的分区表。

---

## 误解 3：改了分区表，原来的数据还安全吗？

不一定。

如果你改了分区布局，可能导致：

```text
APP 覆盖 NVS
OTA 分区重叠
文件系统被覆盖
旧数据读不到
```

所以改分区表后，很多时候需要：

```bash
idf.py erase-flash
idf.py flash
```

尤其是开发阶段改分区布局时。

---

# 15. 一句话总结

`partition table` 就是 ESP32 Flash 的“地图”。

它告诉 Bootloader 和 APP：

```text
nvs 在哪里
otadata 在哪里
factory 在哪里
ota_0 在哪里
ota_1 在哪里
文件系统在哪里
每个区域有多大
```

更通俗地说：

```text
Flash = 一栋楼
partition table = 楼层房间分布图
factory / ota_0 / ota_1 / nvs = 不同房间
Bootloader = 保安/管理员，按分布图找到该进哪个房间启动程序
```
