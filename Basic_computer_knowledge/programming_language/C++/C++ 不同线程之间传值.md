不同线程之间传值，常见有以下**五种安全可靠**的方式，避免用**全局变量裸暴露**：

---

## ① **通过线程函数参数传值（最基本）**

```cpp
#include <iostream>
#include <thread>

void threadFunc(int val) {
    std::cout << "Thread received: " << val << std::endl;
}

int main() {
    int value = 42;
    std::thread t(threadFunc, value);
    t.join();
}
```

⚠️注意：传入的是拷贝，若想共享变量，需用引用或指针。

---

## ② **通过 `std::ref` 传引用**

```cpp
#include <iostream>
#include <thread>

void threadFunc(int& val) {
    val += 10;
}

int main() {
    int value = 42;
    std::thread t(threadFunc, std::ref(value));
    t.join();
    std::cout << value << std::endl;  // 输出 52
}
```

⚠️多线程修改同一变量，要加锁。

---

## ③ **共享数据 + 互斥锁 `std::mutex` 保护**

```cpp
#include <iostream>
#include <thread>
#include <mutex>

int sharedData = 0;
std::mutex mtx;

void threadFunc() {
    std::lock_guard<std::mutex> lock(mtx);
    sharedData += 1;
}

int main() {
    std::thread t1(threadFunc);
    std::thread t2(threadFunc);
    t1.join();
    t2.join();
    std::cout << sharedData << std::endl;
}
```

---

## ④ **使用 `std::condition_variable` 通知传值**

线程A生产数据，线程B等待并消费：

```cpp
#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>

int data = 0;
bool ready = false;
std::mutex mtx;
std::condition_variable cv;

void producer() {
    {
        std::lock_guard<std::mutex> lock(mtx);
        data = 42;
        ready = true;
    }
    cv.notify_one();
}

void consumer() {
    std::unique_lock<std::mutex> lock(mtx);
    cv.wait(lock, [] { return ready; });
    std::cout << "Received data: " << data << std::endl;
}

int main() {
    std::thread t1(consumer);
    std::thread t2(producer);
    t1.join();
    t2.join();
}
```

---

## ⑤ **线程安全队列传值（推荐，解耦好）**

比如：

```cpp
#include <iostream>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>

std::queue<int> msgQueue;
std::mutex mtx;
std::condition_variable cv;

void producer() {
    for (int i = 0; i < 5; ++i) {
        {
            std::lock_guard<std::mutex> lock(mtx);
            msgQueue.push(i);
        }
        cv.notify_one();
    }
}

void consumer() {
    for (int i = 0; i < 5; ++i) {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [] { return !msgQueue.empty(); });
        int val = msgQueue.front();
        msgQueue.pop();
        std::cout << "Consumed: " << val << std::endl;
    }
}

int main() {
    std::thread t1(consumer);
    std::thread t2(producer);
    t1.join();
    t2.join();
}
```

---

## ✅ **总结建议**

| 需求         | 推荐方式                      |
| ---------- | ------------------------- |
| 简单只传一次值    | 线程参数或 `std::ref`          |
| 多次共享或修改数据  | 共享变量 + `std::mutex`       |
| 需要通知、等待    | `std::condition_variable` |
| 大量数据流、解耦需求 | 线程安全队列（配合锁或封装类）           |

---

你如果告诉我具体场景（比如：线程A接收串口数据，线程B处理，线程C保存文件），我可以帮你出针对性的设计方案。你要举例吗？
