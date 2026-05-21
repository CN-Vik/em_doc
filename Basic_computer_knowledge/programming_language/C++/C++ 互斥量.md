在 C++ 中，互斥量（`std::mutex`）是一种用于多线程编程中**保护共享资源**的机制，防止多个线程同时访问某个资源，从而避免数据竞争（data race）和不一致的问题。

---

## 🔒 一、基础用法：`std::mutex`

头文件：`#include <mutex>`

```cpp
#include <iostream>
#include <thread>
#include <mutex>

std::mutex mtx; // 创建一个互斥量
int counter = 0;

void increment(int id) {
    for (int i = 0; i < 10000; ++i) {
        mtx.lock();       // 加锁
        ++counter;        // 访问共享资源
        mtx.unlock();     // 解锁
    }
}
```

你可以通过多个线程调用 `increment()`，互斥量保证了线程安全。

---

## ✅ 更推荐的方式：使用 `std::lock_guard`

它是一种 **RAII（资源获取即初始化）** 的封装类，作用域结束时会自动解锁，非常安全，防止漏解锁。

```cpp
void increment(int id) {
    for (int i = 0; i < 10000; ++i) {
        std::lock_guard<std::mutex> lock(mtx);
        ++counter;
    } // 作用域结束时自动释放锁
}
```

---

## ⛔ 错误示例（不要这样做）：

```cpp
mtx.lock();
++counter;
// 如果中间抛异常，mutex 可能永远不会 unlock
```

---

## 🧠 其他互斥量类型

| 类型                     | 说明                  |
| ---------------------- | ------------------- |
| `std::mutex`           | 最基本的互斥量             |
| `std::recursive_mutex` | 支持同一线程多次加锁          |
| `std::timed_mutex`     | 支持带超时的锁操作           |
| `std::shared_mutex`    | C++17，引入读写锁（多个读一个写） |

---

## 🧪 示例：多个线程安全地写输出

```cpp
#include <iostream>
#include <thread>
#include <mutex>

std::mutex print_mtx;

void safe_print(const std::string& msg) {
    std::lock_guard<std::mutex> lock(print_mtx);
    std::cout << msg << std::endl;
}

int main() {
    std::thread t1(safe_print, "hello from thread 1");
    std::thread t2(safe_print, "hello from thread 2");

    t1.join();
    t2.join();
    return 0;
}
```

---

## ⏳ `try_lock()` 与 `timed_mutex`

```cpp
if (mtx.try_lock()) {
    // 成功获得锁
    mtx.unlock();
} else {
    // 没拿到锁，干别的事
}
```

```cpp
std::timed_mutex tmtx;
if (tmtx.try_lock_for(std::chrono::milliseconds(100))) {
    // 拿到锁
    tmtx.unlock();
}
```

---

## 总结：

| 方法                       | 是否推荐               | 用法说明 |
| ------------------------ | ------------------ | ---- |
| `mutex.lock()/unlock()`  | ❌ 不推荐（容易忘记 unlock） |      |
| `std::lock_guard`        | ✅ 推荐，简单安全          |      |
| `std::unique_lock`       | ✅ 推荐，需要更灵活控制时      |      |
| `try_lock / timed_mutex` | ✅ 用于尝试加锁或有时间要求     |      |

---

如你需要我帮你写一个完整的多线程例子（比如生产者-消费者模型、线程池、文件读写等），可以直接告诉我场景！
