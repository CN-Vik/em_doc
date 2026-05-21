C++ 中，不同线程之间实现 **数据收发** 和 **通知机制**，最常见的做法是通过以下几个组合来完成：

---

## 🧰 常用手段组合

| 目的     | 推荐工具                                                  | 说明             |
| ------ | ----------------------------------------------------- | -------------- |
| 数据共享   | `std::queue<T>` / `std::vector<T>` / 自定义缓冲区           | 多线程共享数据        |
| 互斥访问   | `std::mutex` / `std::lock_guard` / `std::unique_lock` | 防止数据竞争         |
| 条件通知   | `std::condition_variable`                             | 控制线程等待/唤醒      |
| 原子状态控制 | `std::atomic<bool/int>`                               | 控制标志位，不加锁也线程安全 |

---

## ✅ 示例：线程 A 发送数据，线程 B 接收并处理

```cpp
#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <string>
#include <chrono>

std::mutex mtx;
std::condition_variable cv;
std::queue<std::string> message_queue;
bool done = false;

// 线程 A：发送数据
void sender() {
    for (int i = 0; i < 5; ++i) {
        {
            std::lock_guard<std::mutex> lock(mtx);
            message_queue.push("Message " + std::to_string(i));
            std::cout << "[Sender] Sent: Message " << i << std::endl;
        }
        cv.notify_one();  // 通知接收线程
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }

    // 通知完成
    {
        std::lock_guard<std::mutex> lock(mtx);
        done = true;
    }
    cv.notify_all();
}

// 线程 B：接收数据
void receiver() {
    while (true) {
        std::unique_lock<std::mutex> lock(mtx);
        // 等待数据或结束信号
        cv.wait(lock, [] { return !message_queue.empty() || done; });

        while (!message_queue.empty()) {
            std::string msg = message_queue.front();
            message_queue.pop();
            lock.unlock();

            std::cout << "       [Receiver] Got: " << msg << std::endl;
            lock.lock();
        }

        if (done && message_queue.empty()) {
            break;
        }
    }
}

int main() {
    std::thread send_thread(sender);
    std::thread recv_thread(receiver);

    send_thread.join();
    recv_thread.join();

    std::cout << "Finished all communication." << std::endl;
    return 0;
}
```

---

## 🔄 数据收发机制详解

1. **共享队列**：

   * 使用 `std::queue<std::string>` 来传递数据；
   * 可以替换成任何类型（如结构体、二进制 buffer 等）；
2. **互斥锁保护共享数据**：

   * 保证一个时间点只有一个线程能访问 `queue`；
3. **条件变量实现等待/唤醒**：

   * `cv.wait()` 让接收线程阻塞直到有数据；
   * `cv.notify_one()` 唤醒一个等待的接收线程；
4. **done 标志**：

   * 用于通知接收方：发送线程已结束，不会再发新数据；
   * 否则接收方可能一直在等。

---

## ✍️ 可扩展方向（告诉我想看哪部分）：

* 多个发送者/接收者；
* 数据结构支持（结构体、多字段打包）；
* 网络数据模拟；
* 多条件同步（多个条件变量）；
* 超时机制 + 重发机制。

要不要我扩展成一个「多线程收发 + CRC 校验 + ACK/NACK + 重发」的完整小工程结构？或者你有具体收发场景？我可以按你需求定制。
