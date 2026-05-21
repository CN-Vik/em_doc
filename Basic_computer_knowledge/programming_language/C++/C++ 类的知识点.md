好的，下面我将以“教科书”风格，**系统全面、详细、通俗易懂**地为你讲解 **C++ 中的类（class）**。讲解内容将涵盖从基础到进阶、从语法到原理、从设计思想到实际应用。适合作为系统学习 C++ 类的教程。

---

# 📘 C++ 教程：类（Class）详解

---

## 🧠 第1章 什么是类？

### 1.1 类的定义

类（Class）是 C++ 面向对象编程的核心，是一种**用户自定义的数据类型**，它将数据（成员变量）和行为（成员函数）封装在一起。

你可以把类看作“模具”，用它来创建多个“对象”。

### 1.2 类和对象的关系

| 类（Class） | 对象（Object） |
| -------- | ---------- |
| 模板 / 模型  | 模型制造出的实例   |
| 只定义结构    | 实际分配内存     |
| 类似“蓝图”   | 类似“房子”     |

---

## 🧩 第2章 类的基本语法

### 2.1 定义类

```cpp
class Person {
public:              // 公共访问权限
    std::string name;
    int age;

    void sayHello() {
        std::cout << "Hello, I am " << name << std::endl;
    }
};
```

### 2.2 创建对象

```cpp
Person p;            // 栈上创建
p.name = "Alice";
p.sayHello();
```

---

## 🔒 第3章 访问权限

C++ 类中有 3 种访问权限：

| 访问权限 | 关键字         | 意义            |
| ---- | ----------- | ------------- |
| 公有   | `public`    | 所有人都可以访问      |
| 私有   | `private`   | 只有类内部可以访问（默认） |
| 受保护  | `protected` | 只有类和子类可以访问    |

```cpp
class MyClass {
private:
    int secret;

public:
    void set(int s) { secret = s; }
    int get() { return secret; }
};
```

---

## 🧱 第4章 构造函数和析构函数

### 4.1 构造函数（Constructor）

构造函数在对象创建时自动调用，用于初始化。

```cpp
class Dog {
public:
    Dog(std::string n) { name = n; }
    std::string name;
};
```

### 4.2 析构函数（Destructor）

析构函数在对象销毁时自动调用，释放资源。

```cpp
class Dog {
public:
    ~Dog() {
        std::cout << "Dog destroyed\n";
    }
};
```

---

## ⚙️ 第5章 成员函数、this 指针

### 5.1 定义与调用

成员函数定义在类内或类外均可：

```cpp
class Car {
public:
    void drive();     // 声明
};

void Car::drive() {   // 类外定义
    std::cout << "Driving\n";
}
```

### 5.2 this 指针

成员函数内部，`this` 指针指向当前对象。

```cpp
class Person {
public:
    void setName(std::string name) {
        this->name = name;  // 用 this 区分成员变量和形参
    }
private:
    std::string name;
};
```

---

## 🧬 第6章 构造函数重载与默认参数

```cpp
class Point {
public:
    Point() { x = 0; y = 0; }
    Point(int x, int y = 0) {
        this->x = x; this->y = y;
    }
private:
    int x, y;
};
```

支持多个构造函数（重载），可使用默认参数简化调用。

---

## 📦 第7章 类的封装

封装是面向对象三大特性之一（封装、继承、多态）。

**封装目的：**

1. 隐藏内部实现（数据私有）
2. 提供公开接口（成员函数）
3. 提高安全性和可维护性

```cpp
class Account {
private:
    double balance;

public:
    void deposit(double amount);
    void withdraw(double amount);
    double getBalance();
};
```

---

## 👨‍👩‍👧‍👦 第8章 类的继承

### 8.1 基本继承语法

```cpp
class Animal {
public:
    void eat() { std::cout << "Eating\n"; }
};

class Dog : public Animal {
public:
    void bark() { std::cout << "Bark\n"; }
};
```

### 8.2 继承类型

| 类型        | 说明   | 子类继承后的权限                   |
| --------- | ---- | -------------------------- |
| public    | 公有继承 | 保持不变                       |
| protected | 保护继承 | public → protected         |
| private   | 私有继承 | public/protected → private |

---

## 🎭 第9章 多态（预览）

### 9.1 虚函数（virtual）

```cpp
class Animal {
public:
    virtual void speak() {
        std::cout << "Animal speaks\n";
    }
};

class Cat : public Animal {
public:
    void speak() override {
        std::cout << "Meow\n";
    }
};

Animal* a = new Cat();
a->speak();  // 输出 Meow（动态绑定）
```

> 多态是指：父类指针（或引用）调用子类重写的函数。

---

## 🧰 第10章 类的高级特性

### 10.1 静态成员（static）

属于类本身而非对象：

```cpp
class A {
public:
    static int count;
    static void printCount();
};

int A::count = 0;

void A::printCount() {
    std::cout << "Count = " << count << std::endl;
}
```

### 10.2 常量成员（const）

```cpp
class B {
public:
    void print() const {
        // 成员函数不能修改成员变量
    }
};
```

### 10.3 友元函数（friend）

允许外部函数访问私有成员：

```cpp
class Secret {
private:
    int value;
    friend void reveal(Secret s);
};

void reveal(Secret s) {
    std::cout << s.value;
}
```

---

## 🧪 第11章 类的特殊成员函数（六大函数）

当你不定义时，编译器可能自动生成：

| 函数             | 用途          |
| -------------- | ----------- |
| 默认构造函数         | 无参构造        |
| 析构函数           | 对象销毁        |
| 拷贝构造函数         | 用对象拷贝初始化另一个 |
| 拷贝赋值运算符        | 对象赋值        |
| 移动构造函数（C++11）  | 资源“偷”走      |
| 移动赋值运算符（C++11） | 高效转移资源      |

---

## 🧹 第12章 类与内存管理

### 12.1 栈 vs 堆对象

```cpp
Person p;           // 栈上对象，自动析构
Person* p2 = new Person();  // 堆上对象，需要手动 delete
```

### 12.2 Rule of Three / Five / Zero

* **Rule of Three**：拷贝构造、赋值运算符、析构函数
* **Rule of Five**：再加移动构造和移动赋值
* **Rule of Zero**：用 RAII 和智能指针，避免自己写这些函数

---

## 📚 第13章 小结与建议

### 类的核心要点：

✅ 类是用户定义的数据类型
✅ 成员变量和函数共同封装
✅ 支持访问控制、构造析构、继承、多态等特性
✅ 推荐使用智能指针、const、封装和接口设计

---

## ✏️ 附加：练习建议

1. 定义一个 `Student` 类，包含姓名、学号、分数，写构造函数和打印函数。
2. 定义一个 `Shape` 基类和 `Circle` / `Rectangle` 子类，实现面积计算，理解多态。
3. 练习自定义拷贝构造、赋值函数、析构函数，理解资源管理。

---

如你有兴趣，我还可以为你写进阶专题，如：

* 类模板（template class）
* 虚函数表（vtable）与运行时类型识别
* 接口设计和抽象类
* RAII 与资源管理类设计
* PIMPL 和大项目的封装技巧

你想继续学习哪个部分？我可以继续以教科书方式深入讲解。




-----
----
>下边是小案例

# 📘 C++ 教程——类 (Class) 详解教程

---

## 🧐 第1章 什么是类？

### 概念

类 (class) 是 C++ 中面向对象编程的核心概念，是用户定义的数据类型，将数据和操作这两者结合起来。

### 学习案例：简单的人类

```cpp
#include <iostream>
#include <string>

class Person {
public:
    std::string name;
    int age;

    void introduce() {
        std::cout << "Hi, I am " << name << ", and I am " << age << " years old." << std::endl;
    }
};

int main() {
    Person p;
    p.name = "Alice";
    p.age = 25;
    p.introduce();
    return 0;
}
```

### 分析

* `class Person` 是类的定义
* `name` 和 `age` 是成员变量
* `introduce()` 是成员函数
* `main` 中创建对象，并调用成员函数

---

## 🏛️ 第2章 类的基本语法

### 类的定义和创建对象

```cpp
class Rectangle {
public:
    int width, height;
    int area() {
        return width * height;
    }
};

int main() {
    Rectangle r;
    r.width = 10;
    r.height = 5;
    std::cout << "Area = " << r.area() << std::endl;
    return 0;
}
```

### 分析

* 类中可以包含多个函数和变量
* 通过对象调用成员

---

## 🔒 第3章 访问权限

### 描述

* `public`: 全局可用
* `private`: 级联类可观看，外部不能直接操作
* `protected`: 子类可用

### 案例：银行账户

```cpp
class Account {
private:
    double balance;
public:
    void deposit(double amt) { balance += amt; }
    void withdraw(double amt) { balance -= amt; }
    double getBalance() const { return balance; }
};

int main() {
    Account acc;
    acc.deposit(1000);
    acc.withdraw(250);
    std::cout << acc.getBalance();
    return 0;
}
```

### 分析

* 通过接口方法操作 private 属性
* 有效实现了数据隐藏

---

## 🪟 第4章 构造和析构

### 构造函数

```cpp
class Car {
public:
    std::string brand;
    Car(std::string b) {
        brand = b;
    }
};

int main() {
    Car c("Toyota");
    std::cout << c.brand;
    return 0;
}
```

### 析构函数

```cpp
class Car {
public:
    ~Car() {
        std::cout << "Car destroyed" << std::endl;
    }
};
```

### 分析

* 构造函数用来初始化
* 析构函数用来释放资源

---

## ✨ 第5章 this 指针

```cpp
class Box {
private:
    int length;
public:
    void setLength(int length) {
        this->length = length;
    }
    int getLength() { return length; }
};
```

### 分析

* `this` 用来区分当前对象的成员

---

## 🏠 第6章 类的封装

### 实例：系统登录

```cpp
class Login {
private:
    std::string password;
public:
    void setPassword(std::string pwd) {
        password = pwd;
    }
    bool check(std::string input) {
        return input == password;
    }
};
```

### 分析

* 数据隐藏 + 官方接口

---

## 👨‍👩‍👧 第7章 继承

### 基本继承

```cpp
class Animal {
public:
    void eat() { std::cout << "Eat"; }
};

class Dog : public Animal {
public:
    void bark() { std::cout << "Bark"; }
};
```

### 分析

* `Dog` 继承了 `Animal`
* `Dog` 可以调用 `eat()`

---

## 😡 第8章 多态

### 虚函数

```cpp
class Animal {
public:
    virtual void speak() {
        std::cout << "Animal speak";
    }
};

class Cat : public Animal {
public:
    void speak() override {
        std::cout << "Meow";
    }
};

Animal* a = new Cat();
a->speak();  // Meow
```

---

## 🤜 第9章 静态、常量、友兄

### 静态成员

```cpp
class Counter {
public:
    static int count;
    Counter() { ++count; }
};
int Counter::count = 0;
```

### 常量函数

```cpp
class Demo {
public:
    void print() const { /* ... */ }
};
```

### 友兄函数

```cpp
class Secret {
private:
    int value;
    friend void show(Secret s);
};
```

---

## 🔧 第10章 特殊成员函数

* 拷贝构造函数
* 拷贝赋值运算符
* 移动构造和赋值 (C++11)
* 析构函数

---

## ✍️ 经典练习

1. 定义一个 Student 类，包括姓名/学号/分数，并写出打印函数
2. 定义 Shape 基类，实现 Circle 和 Rectangle 类，构造多态
3. 自定义拷贝运算符和析构函数，理解 Rule of 3/5/0

---

如果你想学习类模板，抽象类，RAII 资源管理，或虚函数表原理，我可以推出下一部分进阶系列。
