好的，给你举个具体的例子，演示 Python 包和子模块的导入用法。

---

假设你有如下文件结构：

```
mypackage/
    __init__.py
    math_tools.py
```

文件内容：

* `mypackage/__init__.py`
  （这个文件可以是空的，表示这是一个包）

* `mypackage/math_tools.py`：

```python
def add(a, b):
    return a + b

def multiply(a, b):
    return a * b
```

---

### 1. 导入整个子模块

```python
from mypackage import math_tools

print(math_tools.add(3, 5))       # 输出 8
print(math_tools.multiply(2, 4))  # 输出 8
```

这里导入了 `mypackage` 里的子模块 `math_tools`，使用时需写 `math_tools.` 前缀。

---

### 2. 直接导入子模块中的某个函数

```python
from mypackage.math_tools import add

print(add(10, 20))   # 输出 30
```

这样只导入了 `add` 函数，调用时直接用 `add`，不需要模块名前缀。

---

### 3. 同时导入多个函数

```python
from mypackage.math_tools import add, multiply

print(add(1, 2))        # 输出 3
print(multiply(3, 4))   # 输出 12
```

---

### 总结

* `from mypackage import math_tools` 是导入子模块，使用时写 `math_tools.函数名`
* `from mypackage.math_tools import add` 是直接导入子模块里的函数，可以直接调用函数名

---

如果你想，我还可以帮你写一段演示代码，或者帮你搭建包结构的示例项目。需要的话告诉我\~



---------

当然可以！我帮你把上面关于 Python 包、子模块导入的内容做个整体总结，方便你理解和记忆。

---

# Python 包与子模块导入整体总结

---

## 1. 什么是包和子模块？

* **包（package）**：包含 `__init__.py` 文件的文件夹，是 Python 组织模块的单位。
* **模块（module）**：Python 脚本文件，比如 `math_tools.py` 就是一个模块。
* **子模块**：包内的模块，就是包的组成部分。

---

## 2. 目录结构示例

```
mypackage/           # 包
    __init__.py      # 标识包，可以是空文件
    math_tools.py    # 子模块，定义了一些函数
```

---

## 3. 导入方式及示例

### 方式一：导入整个子模块

```python
from mypackage import math_tools

print(math_tools.add(3, 5))
print(math_tools.multiply(2, 4))
```

* 作用：导入包里的子模块。
* 使用时：调用需要加模块名前缀 `math_tools.`。

---

### 方式二：直接导入子模块中的函数或类

```python
from mypackage.math_tools import add

print(add(10, 20))
```

* 作用：直接导入子模块中的某个具体成员（函数、类等）。
* 使用时：可以直接调用，无需模块名前缀。

---

### 方式三：同时导入多个成员

```python
from mypackage.math_tools import add, multiply
```

---

## 4. 总结表格

| 导入写法                                   | 说明               | 调用方式                  |
| -------------------------------------- | ---------------- | --------------------- |
| `from mypackage import math_tools`     | 导入整个子模块          | `math_tools.add(...)` |
| `from mypackage.math_tools import add` | 直接导入子模块中的函数      | `add(...)`            |
| `from mypackage.math_tools import *`   | 导入子模块所有公开成员（不推荐） | 直接调用，无需前缀             |

---

## 5. 使用建议

* **推荐**使用 `from package import submodule` 或 `from package.submodule import function`，写得清晰明确。
* **避免**使用 `from module import *`，避免命名冲突和代码可读性差。

---

如果你搭建了类似结构，运行导入语句时确保：

* 你的 Python 环境的当前工作目录包含 `mypackage` 所在路径，或者将 `mypackage` 所在目录添加到 `PYTHONPATH`。
* `__init__.py` 文件存在，表示这是一个包。

---

有了这个整体理解，你就可以灵活地根据需要导入包中的模块和函数啦！

需要，我还可以帮你写一个完整的演示代码示例和包结构，方便你实操理解。你想要吗？
