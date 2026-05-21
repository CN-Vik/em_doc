好的！下面我给你写一个**完整、可编译运行的 C++ 示例**，演示 `std::mutex + std::condition_variable` 的经典用法：

---

## ✅ 场景描述：

我们有两个线程：

* 一个线程负责 **等待条件满足**（模拟你的 `device_info_capture`）
* 一个线程负责 **设置条件并唤醒等待者**

---

## ✅ 完整代码示例：

```cpp
#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>

std::mutex mtx;
std::condition_variable cond;
bool ready = false;

// 模拟：等待线程（比如你的 device_info_capture）
void wait_for_start()
{
    std::unique_lock<std::mutex> lock(mtx);
    std::cout << "[waiter] 等待条件...\n";

    // 如果 ready 为 false，则阻塞等待
    cond.wait(lock, [] { return ready; });

    std::cout << "[waiter] 条件满足，继续执行...\n";
}

// 模拟：控制线程，设置 ready = true 并唤醒
void set_ready()
{
    std::this_thread::sleep_for(std::chrono::seconds(2)); // 模拟延时
    {
        std::lock_guard<std::mutex> lock(mtx);
        ready = true;
        std::cout << "[notifier] 条件已满足，发送通知...\n";
    }
    cond.notify_one(); // 唤醒等待者
}

int main()
{
    std::thread t1(wait_for_start);  // 启动等待线程
    std::thread t2(set_ready);       // 启动控制线程

    t1.join();
    t2.join();

    return 0;
}
```

---

## ✅ 运行结果类似：

```text
[waiter] 等待条件...
[notifier] 条件已满足，发送通知...
[waiter] 条件满足，继续执行...
```

---

## ✅ 代码结构说明：

| 角色                        | 说明                                              |
| ------------------------- | ----------------------------------------------- |
| `wait_for_start()`        | 加锁并调用 `cond.wait(lock)` 阻塞等待条件                  |
| `set_ready()`             | 设置条件变量 `ready = true`，然后 `cond.notify_one()` 唤醒 |
| `std::mutex`              | 用来保护 `ready` 共享变量                               |
| `std::condition_variable` | 用来实现线程间的“通知与等待”机制                               |
| `wait(lock, predicate)`   | 推荐写法，内部自动循环检查 predicate（避免虚假唤醒）                 |

---

## ✅ 学到了什么？

* `std::unique_lock`：管理锁，自动加解锁。
* `condition_variable.wait(lock, condition)`：

  * 如果 `condition == false`，自动释放锁，进入阻塞；
  * 被唤醒后自动加锁，判断条件再继续；
* `notify_one()`：唤醒一个等待线程；
* 所有加锁解锁操作都自动完成，不需要手动 `unlock()`。

---

是否需要我再改成你项目结构中 `class Device_Info` 的样子来嵌入示例？也可以帮你写成多轮温度采集 + 唤醒控制的模拟版。你说一声我就写。
