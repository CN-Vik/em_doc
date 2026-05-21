`std::condition_variable` 是 C++11 提供的线程同步原语，**用于线程之间的等待和通知机制**。它通常和 `std::mutex` 搭配使用，能够让一个线程在某个条件满足之前等待（阻塞），而另一个线程在条件满足时通知它继续执行。

---

### 📌 核心作用

* 一个或多个线程 **等待某个条件成立**；
* 另一个线程在条件成立时 **发出通知唤醒等待线程**。

---

### 🧠 工作原理（简化流程）

1. 等待线程调用 `wait()`，并释放互斥锁；
2. 通知线程调用 `notify_one()` 或 `notify_all()`；
3. 被唤醒的线程自动重新尝试获取互斥锁，并继续执行。

---

### 🔧 常见用法

```cpp
#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>

std::mutex mtx;
std::condition_variable cv;
bool ready = false;

void worker() {
    std::unique_lock<std::mutex> lock(mtx);
    // 等待 ready 为 true
    cv.wait(lock, [] { return ready; });
    std::cout << "Worker thread proceeding\n";
}

void setter() {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    {
        std::lock_guard<std::mutex> lock(mtx);
        ready = true;
    }
    cv.notify_one();  // 唤醒一个等待线程
}

int main() {
    std::thread t1(worker);
    std::thread t2(setter);

    t1.join();
    t2.join();
}
```

---

### ✨ 方法说明

#### `wait`

```cpp
cv.wait(lock);
cv.wait(lock, predicate);
```

* 会释放 `lock` 并阻塞当前线程，直到 `notify_*()` 被调用；
* 第二种形式使用 **lambda 表达式作为条件**，更安全，可以避免虚假唤醒（spurious wakeup）。

#### `notify_one()`

* 唤醒一个等待线程。

#### `notify_all()`

* 唤醒所有等待线程。

---

### ⚠️ 注意事项

* 必须在持有 `std::mutex` 的情况下调用 `wait()`；
* `wait()` 可能 **虚假唤醒**（条件没满足也醒了），所以必须用循环/谓词保护；
* 调用 `notify_one` 和 `notify_all` 时，不需要持有锁，但如果你在修改共享变量同时通知，**建议加锁**，避免竞态。

---

### ✅ 应用场景举例

* 生产者-消费者模型；
* 有限资源池控制；
* 多线程任务依赖调度；
* 单次事件同步（如等初始化完成后再开始运行）。

---

如你需要我写一个生产者-消费者完整例子，我可以继续提供。需要吗？


下面是一个**完整的生产者-消费者模型示例**，基于 `std::mutex` + `std::condition_variable`，支持多线程安全的数据共享和同步。

---

## ✅ 示例：生产者-消费者（一个生产者 + 一个消费者）

```cpp
#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <chrono>

std::mutex mtx;
std::condition_variable cv;
std::queue<int> buffer;

const size_t MAX_BUFFER_SIZE = 5;
bool done = false;

void producer() {
    for (int i = 1; i <= 10; ++i) {
        std::unique_lock<std::mutex> lock(mtx);
        // 如果缓冲区满了，就等待
        cv.wait(lock, [] { return buffer.size() < MAX_BUFFER_SIZE; });

        buffer.push(i);
        std::cout << "[Producer] Produced: " << i << std::endl;

        lock.unlock();
        cv.notify_one();  // 通知消费者
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    // 告诉消费者生产结束
    {
        std::lock_guard<std::mutex> lock(mtx);
        done = true;
    }
    cv.notify_all();
}

void consumer() {
    while (true) {
        std::unique_lock<std::mutex> lock(mtx);
        // 如果缓冲区空，就等待
        cv.wait(lock, [] { return !buffer.empty() || done; });

        // 如果数据为空且生产已完成，退出
        if (buffer.empty() && done) {
            break;
        }

        int item = buffer.front();
        buffer.pop();
        std::cout << "       [Consumer] Consumed: " << item << std::endl;

        lock.unlock();
        cv.notify_one();  // 通知生产者
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }
}

int main() {
    std::thread t1(producer);
    std::thread t2(consumer);

    t1.join();
    t2.join();

    std::cout << "All done!" << std::endl;
    return 0;
}
```

---

## 🧠 程序说明

* **生产者线程**：不断生产数据，若队列满就等待；
* **消费者线程**：不断消费数据，若队列空就等待；
* 使用 `std::queue` 存储数据；
* 使用 `cv.wait(lock, predicate)` 来避免虚假唤醒；
* `done` 表示生产者是否已完成，避免消费者死等。

---

如果你需要支持多个生产者、多个消费者、支持文件写入、图形可视化、网络收发等，我可以继续扩展这个例子，告诉我你想要的方向就行。
