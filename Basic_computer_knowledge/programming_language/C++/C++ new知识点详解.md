好的，我们以“教科书”的形式，系统、详细、通俗易懂地讲解 C++ 中的 `new` 关键字。讲解将从最基础的概念出发，一步步深入，覆盖从基本使用到高级特性，并配有示例和小结，方便你系统学习与理解。

---

# 📘 C++ 教程：new 关键字详解

## 🧠 第1章 什么是 `new`？

### 1.1 背景知识

在 C++ 中，内存分为多种区域，如：

* **栈区（stack）**：函数调用时自动分配内存，函数返回后自动释放。速度快，但大小有限。
* **堆区（heap）**：程序运行时动态分配内存，由程序员手动申请和释放。灵活但易出错。

C++ 提供了 `new` 关键字用于在**堆区**动态分配内存。

---

## 🧩 第2章 `new` 的基本用法

### 2.1 分配单个变量

```cpp
int* p = new int;     // 分配一个 int，未初始化
*p = 42;              // 手动赋值
```

也可以直接初始化：

```cpp
int* p = new int(42); // 分配一个 int 并初始化为 42
```

释放内存：

```cpp
delete p;
p = nullptr;          // 避免野指针
```

### ✅ 小结：

| 操作    | 语法          | 说明         |
| ----- | ----------- | ---------- |
| 分配内存  | `new 类型`    | 返回指向该类型的指针 |
| 初始化变量 | `new 类型(值)` | 直接给变量赋初值   |
| 释放内存  | `delete 指针` | 必须手动释放堆内存  |

---

## 🧱 第3章 分配数组

### 3.1 动态数组

```cpp
int* arr = new int[5];      // 分配 5 个 int 元素
arr[0] = 1; arr[4] = 5;
```

释放数组：

```cpp
delete[] arr;               // 必须使用 delete[] 释放数组
arr = nullptr;
```

### 🚨 注意：

使用 `delete` 而不是 `delete[]` 会导致 **未定义行为（UB）**，特别是当数组中有类对象时。

---

## 🧩 第4章 分配类对象

### 4.1 分配一个对象

```cpp
class Person {
public:
    Person(const std::string& name) {
        std::cout << "Constructor called for " << name << "\n";
    }
    ~Person() {
        std::cout << "Destructor called\n";
    }
};

Person* p = new Person("Alice");   // 自动调用构造函数
delete p;                          // 自动调用析构函数
```

### 4.2 分配对象数组

```cpp
Person* arr = new Person[2] { {"Tom"}, {"Jerry"} };
delete[] arr;  // 自动调用每个对象的析构函数
```

---

## 🔍 第5章 new 背后的原理（简要）

### 5.1 new 是什么？

```cpp
void* operator new(size_t size);
```

* 实际调用的是 `operator new` 函数，它返回一块足够大的堆内存。
* `new` 后还会自动调用构造函数。
* 对应的 `delete` 调用的是 `operator delete` 并自动调用析构函数。

---

## 🔧 第6章 自定义 new 和 delete

### 6.1 重载 new/delete（类级别）

```cpp
class MyClass {
public:
    void* operator new(size_t size) {
        std::cout << "Custom new\n";
        return ::operator new(size); // 调用全局 new
    }

    void operator delete(void* ptr) {
        std::cout << "Custom delete\n";
        ::operator delete(ptr);      // 调用全局 delete
    }
};
```

### 6.2 使用示例

```cpp
MyClass* obj = new MyClass;  // 输出：Custom new
delete obj;                  // 输出：Custom delete
```

---

## 🧼 第7章 注意事项和陷阱

| 问题                   | 描述                                           |
| -------------------- | -------------------------------------------- |
| 内存泄漏                 | 没有调用 `delete`，导致堆空间被占用无法释放。                  |
| 野指针                  | 删除对象后未将指针设为 `nullptr`，再访问会出错。                |
| 重复释放                 | 连续调用 `delete` 可能导致崩溃。                        |
| `new` / `delete` 不匹配 | `new[]` 必须配对 `delete[]`，`new` 必须配对 `delete`。 |

---

## 🚀 第8章 C++11/17 中的新特性（现代写法）

现代 C++ 推荐使用智能指针，自动管理内存，避免手动 `new/delete`：

```cpp
#include <memory>

std::unique_ptr<int> ptr = std::make_unique<int>(42); // 自动释放，无需 delete
```

智能指针更安全、更简洁，是现代 C++ 的推荐做法。

---

## 📚 第9章 总结回顾

| 内容     | 示例                          | 说明         |
| ------ | --------------------------- | ---------- |
| 分配单个变量 | `int* p = new int(5);`      | 动态分配并初始化   |
| 分配数组   | `int* arr = new int[10];`   | 使用 `new[]` |
| 类对象    | `Person* p = new Person();` | 自动调用构造和析构  |
| 自定义行为  | 重载 `operator new`           | 用于调试或性能优化  |
| 智能指针   | `std::make_unique<T>()`     | 推荐的现代方式    |

---

如你有兴趣，我还可以继续写出进阶专题，例如：

* `new` 分配失败时的处理（nothrow 版本）
* 对比 `malloc/free` 与 `new/delete`
* 与 placement new（定位 new）有关的高级应用
* 对 STL 中 allocator 的底层 `new` 调用原理分析

是否需要我继续扩展？你可以指定你想深入哪一块内容 😊
