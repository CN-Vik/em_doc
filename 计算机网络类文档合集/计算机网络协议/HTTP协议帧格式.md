![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/88969280057a4d04aee3161cbd3a85c2.png)

----
只看 **HTTP 层**，可以先记住一句话：

> HTTP 层没有像 UART 那种固定“帧头/帧尾/校验”的二进制帧格式。
> HTTP/1.1 主要是 **文本协议**，格式是：
> **起始行 + 头部字段 + 空行 + 可选正文 Body**。

天气案例里，ESP32 发的是 **HTTP Request 请求报文**，服务器回的是 **HTTP Response 响应报文**。

---

# 1. HTTP 请求报文格式

HTTP 请求格式：

```text
请求行\r\n
请求头1\r\n
请求头2\r\n
请求头3\r\n
...\r\n
\r\n
请求体 Body
```

也就是：

```text
┌──────────────────────────────┐
│ Request Line 请求行           │
├──────────────────────────────┤
│ Headers 请求头                │
├──────────────────────────────┤
│ 空行 \r\n                     │
├──────────────────────────────┤
│ Body 请求体，可选              │
└──────────────────────────────┘
```

天气 GET 请求通常没有 Body。

---

# 2. 天气案例 HTTP GET 请求

比如 ESP32 请求大阪天气：

```http
GET /data/2.5/weather?q=Osaka&appid=YOUR_API_KEY&units=metric&lang=zh_cn HTTP/1.1
Host: api.openweathermap.org
User-Agent: esp32
Connection: close

```

注意：最后必须有一个空行。

在 C 字符串里要写成：

```c
const char *http_request =
    "GET /data/2.5/weather?q=Osaka&appid=YOUR_API_KEY&units=metric&lang=zh_cn HTTP/1.1\r\n"
    "Host: api.openweathermap.org\r\n"
    "User-Agent: esp32\r\n"
    "Connection: close\r\n"
    "\r\n";
```

---

# 3. 请求行格式

请求行格式：

```text
Method SP Request-URI SP HTTP-Version CRLF
```

对应天气请求：

```http
GET /data/2.5/weather?q=Osaka&appid=YOUR_API_KEY&units=metric&lang=zh_cn HTTP/1.1
```

拆开看：

```text
GET
/data/2.5/weather?q=Osaka&appid=YOUR_API_KEY&units=metric&lang=zh_cn
HTTP/1.1
```

含义：

| 字段                      | 含义          |
| ----------------------- | ----------- |
| `GET`                   | 请求方法，表示获取数据 |
| `/data/2.5/weather?...` | 请求资源路径和参数   |
| `HTTP/1.1`              | HTTP 协议版本   |

其中 `SP` 就是空格，`CRLF` 就是 `\r\n`。

---

# 4. 请求头格式

每个请求头格式都是：

```text
Header-Name: Header-Value\r\n
```

比如：

```http
Host: api.openweathermap.org
User-Agent: esp32
Connection: close
```

这些头部字段的作用：

| 请求头                 | 作用             |
| ------------------- | -------------- |
| `Host`              | 告诉服务器你访问的是哪个域名 |
| `User-Agent`        | 告诉服务器客户端是谁     |
| `Connection: close` | 请求完成后关闭连接      |

HTTP/1.1 里 `Host` 基本是必须的。

---

# 5. 空行很重要

请求头结束后，要有一个空行：

```text
\r\n
```

完整请求末尾实际是：

```text
Connection: close\r\n
\r\n
```

这个空行的意思是：

> 请求头结束了，后面如果还有内容，就是 Body。

天气 GET 请求没有 Body，所以空行后面就没东西了。

---

# 6. HTTP 响应报文格式

服务器返回格式：

```text
状态行\r\n
响应头1\r\n
响应头2\r\n
响应头3\r\n
...\r\n
\r\n
响应体 Body
```

结构是：

```text
┌──────────────────────────────┐
│ Status Line 状态行            │
├──────────────────────────────┤
│ Headers 响应头                │
├──────────────────────────────┤
│ 空行 \r\n                     │
├──────────────────────────────┤
│ Body 响应体                   │
└──────────────────────────────┘
```

---

# 7. 天气案例 HTTP 响应

服务器可能返回：

```http
HTTP/1.1 200 OK
Content-Type: application/json; charset=utf-8
Content-Length: 168
Connection: close

{
  "weather": [
    {
      "main": "Clouds",
      "description": "多云"
    }
  ],
  "main": {
    "temp": 23.5,
    "humidity": 60
  },
  "name": "Osaka"
}
```

---

# 8. 状态行格式

状态行格式：

```text
HTTP-Version SP Status-Code SP Reason-Phrase CRLF
```

比如：

```http
HTTP/1.1 200 OK
```

拆开：

| 字段         | 含义        |
| ---------- | --------- |
| `HTTP/1.1` | HTTP 协议版本 |
| `200`      | 状态码       |
| `OK`       | 状态描述      |

常见状态码：

| 状态码                         | 含义             |
| --------------------------- | -------------- |
| `200 OK`                    | 请求成功           |
| `301/302`                   | 重定向            |
| `400 Bad Request`           | 请求格式或参数错误      |
| `401 Unauthorized`          | API Key 错误或无权限 |
| `403 Forbidden`             | 禁止访问           |
| `404 Not Found`             | API 路径错误       |
| `500 Internal Server Error` | 服务器内部错误        |

---

# 9. 响应头格式

响应头也是：

```text
Header-Name: Header-Value\r\n
```

比如：

```http
Content-Type: application/json; charset=utf-8
Content-Length: 168
Connection: close
```

含义：

| 响应头              | 含义                  |
| ---------------- | ------------------- |
| `Content-Type`   | Body 的数据类型，这里是 JSON |
| `Content-Length` | Body 的长度，单位字节       |
| `Connection`     | 连接控制                |

---

# 10. 响应体 Body

空行后面的内容就是 Body：

```json
{
  "weather": [
    {
      "main": "Clouds",
      "description": "多云"
    }
  ],
  "main": {
    "temp": 23.5,
    "humidity": 60
  },
  "name": "Osaka"
}
```

ESP32 真正要解析的是这一部分。

也就是：

```text
HTTP 响应头：告诉你返回了什么
HTTP 响应体：真正的天气数据
```

---

# 11. HTTP GET 请求完整字节格式

天气 GET 请求在 HTTP 层实际就是一串 ASCII 字符：

```text
GET /data/2.5/weather?q=Osaka&appid=YOUR_API_KEY&units=metric&lang=zh_cn HTTP/1.1\r\n
Host: api.openweathermap.org\r\n
User-Agent: esp32\r\n
Connection: close\r\n
\r\n
```

如果把换行展开看：

```text
GET /data/2.5/weather?q=Osaka&appid=YOUR_API_KEY&units=metric&lang=zh_cn HTTP/1.1[CRLF]
Host: api.openweathermap.org[CRLF]
User-Agent: esp32[CRLF]
Connection: close[CRLF]
[CRLF]
```

`[CRLF]` 就是：

```text
\r\n
```

对应十六进制：

```text
0D 0A
```

所以 HTTP 头部每行结尾都是：

```text
0D 0A
```

头部结束是两个连续的 CRLF：

```text
0D 0A 0D 0A
```

---

# 12. HTTP 响应完整格式

服务器响应在 HTTP 层也是文本加 Body：

```text
HTTP/1.1 200 OK\r\n
Content-Type: application/json; charset=utf-8\r\n
Content-Length: 168\r\n
Connection: close\r\n
\r\n
{"weather":[{"main":"Clouds","description":"多云"}],"main":{"temp":23.5,"humidity":60},"name":"Osaka"}
```

可以理解为：

```text
响应头部分：
HTTP/1.1 200 OK\r\n
Content-Type: application/json; charset=utf-8\r\n
Content-Length: 168\r\n
Connection: close\r\n
\r\n

响应体部分：
{"weather":[...],"main":{...},"name":"Osaka"}
```

---

# 13. GET 请求为什么没有 Body？

天气查询是获取数据，一般参数都放在 URL 后面：

```text
/data/2.5/weather?q=Osaka&appid=YOUR_API_KEY&units=metric
```

所以 GET 请求通常没有 Body。

```text
GET 请求：
参数在 URL 里
Body 一般为空
```

---

# 14. 如果是 POST，HTTP 层格式会不一样

比如 ESP32 上传传感器数据：

```http
POST /api/weather/upload HTTP/1.1
Host: example.com
Content-Type: application/json
Content-Length: 31
Connection: close

{"temp":23.5,"humidity":60}
```

POST 报文结构：

```text
请求行
请求头
空行
请求体
```

其中请求体是：

```json
{"temp":23.5,"humidity":60}
```

POST 必须告诉服务器 Body 多长，所以经常有：

```http
Content-Length: 31
```

---

# 15. Content-Length 是干啥的？

比如响应：

```http
Content-Length: 168
```

意思是：

> 空行后面的 Body 有 168 字节。

ESP32 接收时，可以根据这个判断 Body 有没有收完整。

如果没有 `Content-Length`，也可能用：

```http
Transfer-Encoding: chunked
```

---

# 16. Chunked 格式是什么？

有些服务器不知道一次返回多大，就会分块返回：

```http
HTTP/1.1 200 OK
Content-Type: application/json
Transfer-Encoding: chunked

A
0123456789
5
abcde
0

```

格式是：

```text
块大小，十六进制
块内容
块大小，十六进制
块内容
0
结束
```

对新手来说你先知道：

```text
Content-Length：提前告诉你 Body 总长度
chunked：一块一块发，最后用 0 表示结束
```

如果你用 `esp_http_client`，很多细节它会帮你处理。

如果你自己 socket 手写 HTTP，就要注意这个坑。

---

# 17. 只看 HTTP 层，请求和响应总结

## HTTP 请求

```text
┌────────────────────────────────────────────┐
│ 请求行                                      │
│ GET /data/2.5/weather?... HTTP/1.1          │
├────────────────────────────────────────────┤
│ 请求头                                      │
│ Host: api.openweathermap.org                │
│ User-Agent: esp32                           │
│ Connection: close                           │
├────────────────────────────────────────────┤
│ 空行                                        │
│ \r\n                                        │
├────────────────────────────────────────────┤
│ 请求体                                      │
│ GET 一般为空                                │
└────────────────────────────────────────────┘
```

## HTTP 响应

```text
┌────────────────────────────────────────────┐
│ 状态行                                      │
│ HTTP/1.1 200 OK                             │
├────────────────────────────────────────────┤
│ 响应头                                      │
│ Content-Type: application/json              │
│ Content-Length: xxx                         │
│ Connection: close                           │
├────────────────────────────────────────────┤
│ 空行                                        │
│ \r\n                                        │
├────────────────────────────────────────────┤
│ 响应体                                      │
│ {"main":{"temp":23.5},"weather":[...]}      │
└────────────────────────────────────────────┘
```

---

# 18. 你作为嵌入式开发要重点记住

HTTP 层最重要的是这几个：

```text
1. 请求行 / 状态行
2. Header 头部
3. 空行 \r\n
4. Body
5. GET 通常没有 Body
6. POST 通常有 Body
7. Body 长度由 Content-Length 或 chunked 决定
8. 天气 JSON 在响应 Body 里面
```

一句话总结：

```text
HTTP 请求 = 你问服务器要什么
HTTP 响应 = 服务器告诉你结果，并把数据放在 Body 里
```

天气案例中：

```text
ESP32 HTTP Request：
GET /data/2.5/weather?... HTTP/1.1

服务器 HTTP Response：
HTTP/1.1 200 OK
...
天气 JSON 数据
```


---------
---------
----------

# 更详细的底层原理
天气案例里，**HTTP 请求和应答不是直接裸奔在 Wi-Fi 上的**，它是一层一层封装的。

你可以这样理解：

```text
天气 JSON
  ↑
HTTP 协议
  ↑
TCP 协议
  ↑
IP 协议
  ↑
Wi-Fi / 以太网
  ↑
无线电波 / 网线
```

对 ESP32 来说，获取天气时真正发出去的数据大概是：

```text
[Wi-Fi帧头] [IP头] [TCP头] [HTTP请求内容]
```

服务器返回时也是：

```text
[Wi-Fi帧头] [IP头] [TCP头] [HTTP响应内容]
```

---

# 1. 天气 HTTP 请求完整流程

假设 ESP32 要访问：

```text
http://api.openweathermap.org/data/2.5/weather?q=Osaka&appid=API_KEY&units=metric&lang=zh_cn
```

完整过程是：

```text
ESP32 连接 Wi-Fi
  ↓
DNS 解析 api.openweathermap.org 得到服务器 IP
  ↓
TCP 三次握手连接服务器 80 端口
  ↓
发送 HTTP GET 请求
  ↓
服务器返回 HTTP 响应
  ↓
ESP32 解析 JSON
  ↓
TCP 连接关闭
```

---

# 2. 协议栈分层格式

从上到下看：

```text
应用层：HTTP
传输层：TCP
网络层：IP
链路层：Wi-Fi / Ethernet
物理层：无线信号
```

天气请求封装后大概是这样：

```text
┌────────────────────────────────────┐
│ Wi-Fi / Ethernet 帧头              │ ← 局域网传输用
├────────────────────────────────────┤
│ IP Header                          │ ← 源 IP、目标 IP
├────────────────────────────────────┤
│ TCP Header                         │ ← 源端口、目标端口、序号
├────────────────────────────────────┤
│ HTTP Request                       │ ← GET 天气请求
└────────────────────────────────────┘
```

HTTP 响应也是类似：

```text
┌────────────────────────────────────┐
│ Wi-Fi / Ethernet 帧头              │
├────────────────────────────────────┤
│ IP Header                          │
├────────────────────────────────────┤
│ TCP Header                         │
├────────────────────────────────────┤
│ HTTP Response                      │ ← 天气 JSON 数据
└────────────────────────────────────┘
```

---

# 3. HTTP 请求报文格式

HTTP 请求本身长这样：

```http
GET /data/2.5/weather?q=Osaka&appid=API_KEY&units=metric&lang=zh_cn HTTP/1.1
Host: api.openweathermap.org
User-Agent: esp32
Connection: close

```

注意最后有一个空行。

标准格式是：

```text
请求行
请求头1
请求头2
请求头3
空行
请求体
```

对于 GET 请求，一般没有请求体。

所以天气 HTTP GET 请求可以拆成：

```text
请求行：
GET /data/2.5/weather?q=Osaka&appid=API_KEY&units=metric&lang=zh_cn HTTP/1.1

请求头：
Host: api.openweathermap.org
User-Agent: esp32
Connection: close

空行：
\r\n

请求体：
GET 请求通常为空
```

---

# 4. HTTP 请求行格式

这一行：

```http
GET /data/2.5/weather?q=Osaka&appid=API_KEY&units=metric&lang=zh_cn HTTP/1.1
```

拆开是：

```text
GET                         请求方法
/data/2.5/weather?...       请求路径和参数
HTTP/1.1                    HTTP 协议版本
```

也就是：

```text
方法 + 空格 + URL路径 + 空格 + HTTP版本 + \r\n
```

真正格式：

```text
GET /xxx/xxx?参数1=值1&参数2=值2 HTTP/1.1\r\n
```

---

# 5. HTTP 请求头格式

请求头每一行格式是：

```text
字段名: 字段值\r\n
```

比如：

```http
Host: api.openweathermap.org
User-Agent: esp32
Connection: close
```

对应含义：

| 请求头                 | 含义              |
| ------------------- | --------------- |
| `Host`              | 要访问哪个网站/服务器     |
| `User-Agent`        | 客户端身份，这里是 ESP32 |
| `Connection: close` | 服务器返回后关闭连接      |

HTTP 请求头结束后，必须有一个空行：

```text
\r\n
```

所以完整 HTTP 请求在 C 语言里一般写成这样：

```c
const char *request =
    "GET /data/2.5/weather?q=Osaka&appid=API_KEY&units=metric&lang=zh_cn HTTP/1.1\r\n"
    "Host: api.openweathermap.org\r\n"
    "User-Agent: esp32\r\n"
    "Connection: close\r\n"
    "\r\n";
```

`\r\n` 是 HTTP 协议要求的换行符。

---

# 6. 这个 HTTP 请求在 TCP 里长什么样？

HTTP 本身只是 TCP 负载。

TCP 包大概这样：

```text
┌────────────────────────────────────┐
│ TCP Header                         │
│ - 源端口：随机端口，例如 54321     │
│ - 目标端口：80                    │
│ - 序号 seq                        │
│ - 确认号 ack                      │
│ - 标志位 PSH ACK                  │
├────────────────────────────────────┤
│ TCP Payload                        │
│ GET /data/2.5/weather?... HTTP/1.1 │
│ Host: api.openweathermap.org       │
│ User-Agent: esp32                  │
│ Connection: close                  │
│                                    │
└────────────────────────────────────┘
```

也就是说：

```text
TCP 不知道你这是天气请求。
TCP 只知道：我要把这一串字节可靠地送到服务器 80 端口。
```

HTTP 的内容在 TCP 的数据区里。

---

# 7. 这个 TCP 包在 IP 里长什么样？

IP 包大概这样：

```text
┌────────────────────────────────────┐
│ IP Header                          │
│ - 源 IP：ESP32 的 IP，比如 192.168.1.20
│ - 目标 IP：天气服务器 IP
│ - 协议号：6，表示 TCP
│ - TTL
│ - 总长度
├────────────────────────────────────┤
│ TCP Segment                        │
│ ┌────────────────────────────────┐ │
│ │ TCP Header                     │ │
│ ├────────────────────────────────┤ │
│ │ HTTP GET 请求                  │ │
│ └────────────────────────────────┘ │
└────────────────────────────────────┘
```

IP 层关心的是：

```text
从哪个 IP 来？
要发到哪个 IP 去？
上层协议是什么？
```

对 HTTP 来说，上层协议是 TCP，所以 IP Header 里：

```text
Protocol = 6
```

---

# 8. 这个 IP 包在 Wi-Fi 里长什么样？

ESP32 用 Wi-Fi 发出去时，还会再加 Wi-Fi 帧头。

简化后：

```text
┌────────────────────────────────────┐
│ Wi-Fi MAC Header                   │
│ - 源 MAC：ESP32 的 MAC             │
│ - 目标 MAC：路由器/AP 的 MAC       │
├────────────────────────────────────┤
│ LLC/SNAP                           │
├────────────────────────────────────┤
│ IP Packet                          │
│ ┌────────────────────────────────┐ │
│ │ IP Header                      │ │
│ ├────────────────────────────────┤ │
│ │ TCP Header                     │ │
│ ├────────────────────────────────┤ │
│ │ HTTP GET 请求                  │ │
│ └────────────────────────────────┘ │
├────────────────────────────────────┤
│ FCS 校验                           │
└────────────────────────────────────┘
```

你可以先不用纠结 Wi-Fi 帧头细节。

嵌入式软件开发时重点先掌握：

```text
HTTP 在 TCP 里面
TCP 在 IP 里面
IP 在 Wi-Fi / Ethernet 里面
```

---

# 9. 服务器返回的 HTTP 响应格式

服务器返回大概是：

```http
HTTP/1.1 200 OK
Content-Type: application/json; charset=utf-8
Content-Length: 180
Connection: close

{
  "weather": [
    {
      "main": "Clouds",
      "description": "多云"
    }
  ],
  "main": {
    "temp": 23.5,
    "humidity": 60
  },
  "name": "Osaka"
}
```

HTTP 响应格式是：

```text
状态行
响应头1
响应头2
响应头3
空行
响应体
```

---

# 10. HTTP 响应状态行

这一行：

```http
HTTP/1.1 200 OK
```

拆开是：

```text
HTTP/1.1    协议版本
200         状态码
OK          状态说明
```

常见状态码：

| 状态码       | 含义             |
| --------- | -------------- |
| `200 OK`  | 请求成功           |
| `301/302` | 重定向            |
| `400`     | 请求参数错误         |
| `401`     | API Key 错误或无权限 |
| `404`     | 路径不存在          |
| `500`     | 服务器内部错误        |

---

# 11. HTTP 响应头

比如：

```http
Content-Type: application/json; charset=utf-8
Content-Length: 180
Connection: close
```

含义是：

| 响应头              | 含义               |
| ---------------- | ---------------- |
| `Content-Type`   | 返回的数据类型，这里是 JSON |
| `Content-Length` | 响应体长度            |
| `Connection`     | 连接是否关闭           |

---

# 12. HTTP 响应体

空行后面的才是真正的天气数据：

```json
{
  "weather": [
    {
      "main": "Clouds",
      "description": "多云"
    }
  ],
  "main": {
    "temp": 23.5,
    "humidity": 60
  },
  "name": "Osaka"
}
```

ESP32 最后用 `cJSON` 解析的是这部分。

你关心的是：

```text
main.temp                 温度
main.humidity             湿度
weather[0].description    天气描述
name                      城市名
```

---

# 13. 响应在协议栈里怎么封装？

服务器返回时，方向反过来。

```text
服务器应用层生成 HTTP 响应
  ↓
TCP 封装
  ↓
IP 封装
  ↓
链路层封装
  ↓
互联网传输
  ↓
路由器
  ↓
Wi-Fi 发给 ESP32
```

ESP32 收到后逐层拆包：

```text
Wi-Fi 驱动收到无线帧
  ↓
取出 IP 包
  ↓
IP 层发现这是发给自己的
  ↓
取出 TCP 段
  ↓
TCP 层重组数据流
  ↓
交给 HTTP client
  ↓
HTTP client 得到响应头和响应体
  ↓
你的代码拿到 JSON
```

---

# 14. 请求和应答的完整协议栈格式对比

## ESP32 发请求

```text
Wi-Fi Frame
└── IP Packet
    └── TCP Segment
        └── HTTP Request
            ├── Request Line
            │   └── GET /data/2.5/weather?... HTTP/1.1
            ├── Headers
            │   ├── Host: api.openweathermap.org
            │   ├── User-Agent: esp32
            │   └── Connection: close
            ├── Empty Line
            └── Body: 空
```

## 服务器回响应

```text
Wi-Fi Frame
└── IP Packet
    └── TCP Segment
        └── HTTP Response
            ├── Status Line
            │   └── HTTP/1.1 200 OK
            ├── Headers
            │   ├── Content-Type: application/json
            │   ├── Content-Length: xxx
            │   └── Connection: close
            ├── Empty Line
            └── Body
                └── {"main":{"temp":23.5},"weather":[...]}
```

---

# 15. 加上 TCP 三次握手，完整通信长这样

HTTP 请求不是上来就发的，先要建立 TCP 连接。

```text
ESP32                                      天气服务器
  │                                             │
  │  SYN                                        │
  │────────────────────────────────────────────>│
  │                                             │
  │  SYN + ACK                                  │
  │<────────────────────────────────────────────│
  │                                             │
  │  ACK                                        │
  │────────────────────────────────────────────>│
  │                                             │
  │  HTTP GET /data/2.5/weather?...             │
  │────────────────────────────────────────────>│
  │                                             │
  │  HTTP/1.1 200 OK + JSON天气数据             │
  │<────────────────────────────────────────────│
  │                                             │
  │  FIN / ACK 关闭连接                         │
  │<───────────────────────────────────────────>│
```

所以真正完整顺序是：

```text
DNS 解析
TCP 三次握手
HTTP 请求
HTTP 响应
TCP 四次挥手/关闭连接
```

---

# 16. DNS 解析也是协议栈的一部分

你写的是域名：

```text
api.openweathermap.org
```

但 TCP/IP 真正连接的是 IP 地址。

所以 ESP32 要先问 DNS 服务器：

```text
api.openweathermap.org 的 IP 是多少？
```

DNS 查询通常是：

```text
应用层：DNS
传输层：UDP
网络层：IP
链路层：Wi-Fi
```

格式大概是：

```text
Wi-Fi Frame
└── IP Packet
    └── UDP Datagram
        └── DNS Query
            └── 查询 api.openweathermap.org
```

DNS 返回 IP 后，ESP32 才能建立 TCP 连接。

---

# 17. 如果是 HTTPS，协议栈格式变成什么？

HTTP：

```text
Wi-Fi
└── IP
    └── TCP
        └── HTTP
```

HTTPS：

```text
Wi-Fi
└── IP
    └── TCP
        └── TLS
            └── HTTP
```

也就是中间多了一层 TLS。

HTTPS 请求真实格式不是：

```text
TCP 里面直接放 HTTP GET
```

而是：

```text
TCP 里面放 TLS 加密数据
TLS 解密后才是 HTTP GET
```

---

# 18. HTTPS 天气请求封装格式

```text
Wi-Fi Frame
└── IP Packet
    └── TCP Segment
        └── TLS Record
            └── Encrypted HTTP Request
                └── GET /data/2.5/weather?... HTTP/1.1
```

服务器响应：

```text
Wi-Fi Frame
└── IP Packet
    └── TCP Segment
        └── TLS Record
            └── Encrypted HTTP Response
                └── HTTP/1.1 200 OK + JSON
```

抓包时，HTTP 可以看到明文：

```http
GET /data/2.5/weather?q=Osaka&appid=API_KEY HTTP/1.1
Host: api.openweathermap.org
```

HTTPS 抓包时看到的是：

```text
TLS Application Data
加密数据
加密数据
加密数据
```

看不到具体天气请求和 API Key。

---

# 19. HTTP 和 HTTPS 协议栈对比

| 项目              | HTTP              | HTTPS                 |
| --------------- | ----------------- | --------------------- |
| 默认端口            | 80                | 443                   |
| 应用层内容           | 明文 HTTP           | HTTP 被 TLS 加密         |
| 协议栈             | Wi-Fi/IP/TCP/HTTP | Wi-Fi/IP/TCP/TLS/HTTP |
| 是否能抓包看到 API Key | 能                 | 正常看不到                 |
| ESP32 资源占用      | 低                 | 更高                    |
| 正式产品推荐          | 不推荐               | 推荐                    |

---

# 20. 最核心的一张图

天气 HTTP 请求：

```text
ESP32 应用代码
    ↓
HTTP GET 请求字符串
    ↓
TCP 头 + HTTP 数据
    ↓
IP 头 + TCP 数据
    ↓
Wi-Fi 头 + IP 数据
    ↓
无线发送出去
```

天气 HTTP 响应：

```text
无线收到数据
    ↓
去掉 Wi-Fi 头
    ↓
去掉 IP 头
    ↓
TCP 重组数据
    ↓
得到 HTTP 响应
    ↓
去掉 HTTP 响应头
    ↓
得到 JSON 天气数据
    ↓
cJSON 解析
```

---

# 21. 你在 ESP32 代码里看到的是哪一层？

如果你用 `esp_http_client`：

```c
esp_http_client_perform(client);
```

你主要操作的是：

```text
HTTP 层
```

底层的：

```text
DNS
TCP
IP
Wi-Fi
```

大部分 ESP-IDF 已经帮你处理了。

如果你用 socket 手写：

```c
socket()
connect()
send()
recv()
```

你操作的是：

```text
TCP 层
```

HTTP 请求字符串要你自己拼。

如果你用 Wi-Fi 驱动：

```c
esp_wifi_start()
```

你操作的是：

```text
Wi-Fi 链路层/网络接口
```

---

# 22. 一句话总结

天气案例中的 HTTP 协议栈格式就是：

```text
Wi-Fi 帧
  里面装 IP 包
    IP 包里面装 TCP 段
      TCP 段里面装 HTTP 请求/响应
        HTTP 响应体里面装 JSON 天气数据
```

最关键记住这句：

```text
HTTP 不是直接在 Wi-Fi 上跑的。
HTTP 是 TCP 的数据内容。
TCP 又被 IP 封装。
IP 最后才通过 Wi-Fi 发出去。
```

如果是 HTTPS，就是：

```text
Wi-Fi
  ↓
IP
  ↓
TCP
  ↓
TLS 加密层
  ↓
HTTP
  ↓
JSON 天气数据
```
