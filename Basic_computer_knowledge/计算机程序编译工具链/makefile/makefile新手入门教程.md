@[toc]
# 前言
> 学习makfile的原因，cmake生成的makefile 太臃肿了
> [参考文档-程序编译流程和原理](https://blog.csdn.net/m0_57249200/article/details/138384362?fromshare=blogdetail&sharetype=blogdetail&sharerId=138384362&sharerefer=PC&sharesource=m0_57249200&sharefrom=from_link)
![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/4c5c088bee3744358928b476dfb212be.png)
![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/16a202417cbc46ca9a209304dec48539.jpeg)
# 1.GCC等前提基础知识
## 1.1 计算机程序编译知识

------
![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/8d8ec5e78c5146d9ad9cc8a89e3a83db.png)---------------------------
![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/7bc07734a44b4543a07298bec000b702.png)

---
## 1.2 静态链接和动态链接介绍
动态链接和静态链接是程序在**编译和运行**时处理库（如 `.so`、`.dll` 或 `.a`、`.lib`）的两种方式。  
### **1.2.1 静态链接（Static Linking）**
**👉 编译时直接把库的代码拷贝到可执行文件中**。  
#### **特点**：
- 生成的可执行文件**体积大**（因为包含所有库代码）。
- 运行时**不依赖外部库**，可以独立运行。
- 适用于**嵌入式系统**或**发布时不想依赖额外库**的场景。  

#### **示例：**
假设你有一个 `math.a`（静态库）：
```sh
gcc main.c -o main -L. -lmath
```
编译后，`main` 已经包含了 `math.a` 的所有必要代码，**不需要额外的 `.a` 文件**。

---

### **1.2.2 动态链接（Dynamic Linking）**
**👉 编译时不复制库代码，运行时再加载**。  
#### **特点**：
- 生成的可执行文件**体积小**，但运行时**依赖动态库**（如 `.so`、`.dll`）。
- 可以**多个程序共享库**，减少内存占用。
- 适用于**大项目**，库更新后**无需重新编译**整个程序。  

#### **示例：**
假设你有 `math.so`（动态库）：
```sh
gcc main.c -o main -L. -lmath
```
运行时需要 `math.so`，否则会报错：
```sh
./main: error while loading shared libraries: libmath.so: cannot open shared object file
```
解决办法：
```sh
export LD_LIBRARY_PATH=.:$LD_LIBRARY_PATH
./main
```
这样 `main` 就能找到 `math.so` 并正常运行。

---

### **1.2.3 主要区别**
|  | **静态链接** | **动态链接** |
|---|---|---|
| 依赖库 | 不需要（已包含） | 运行时需要 |
| 生成文件 | 体积大 | 体积小 |
| 运行效率 | 略快 | 可能稍慢（取决于系统缓存） |
| 适用场景 | 独立发布、嵌入式 | 共享库、系统程序 |

---

### **1.2.4 现实生活中的例子**
- **静态链接**：类似买一个蛋糕，所有配料（库）都已经混合好了，可以直接吃（运行）。
- **动态链接**：类似做蛋糕时调用超市的原料（库），如果超市没开（库丢失），就做不出来（运行失败）。
这样理解是不是更直观了？😃


### 1.2.5 静态库动态库 学习案例
当然！我来写一个**简单易懂**的**静态链接**和**动态链接**示例，适合新手学习。  

---

#### **1.2.5.1 准备一个库（math_lib）**
我们要创建一个数学库 `math_lib`，提供**加法**和**减法**函数，分别演示**静态链接**和**动态链接**的用法。

#### **创建 `math_lib.c`（数学库实现）**
```c
// math_lib.c
#include <stdio.h>

void add(int a, int b) {
    printf("Add: %d + %d = %d\n", a, b, a + b);
}

void subtract(int a, int b) {
    printf("Subtract: %d - %d = %d\n", a, b, a - b);
}
```
#### **创建 `math_lib.h`（头文件）**
```c
// math_lib.h
#ifndef MATH_LIB_H
#define MATH_LIB_H

void add(int a, int b);
void subtract(int a, int b);

#endif
```

---

#### **1.2.5.2 静态链接示例**
##### **(1) 生成静态库**
```sh
gcc -c math_lib.c -o math_lib.o    # 编译成目标文件
ar rcs libmath.a math_lib.o        # 生成静态库 libmath.a
```
这会创建 `libmath.a`，以后我们可以把它链接到程序里。

---

##### **(2) 创建 `main_static.c`**
```c
// main_static.c
#include <stdio.h>
#include "math_lib.h"

int main() {
    add(10, 5);
    subtract(10, 5);
    return 0;
}
```

---

##### **(3) 编译 & 运行**
```sh
gcc main_static.c -o main_static -L. -lmath
./main_static
```
**输出：**
```
Add: 10 + 5 = 15
Subtract: 10 - 5 = 5
```
这里 `main_static` **已经包含了 `libmath.a` 的代码**，所以不需要额外的库文件就能运行。

---

#### **1.2.5.3 动态链接示例**
##### **(1) 生成动态库**
```sh
gcc -shared -fPIC math_lib.c -o libmath.so
```
这会创建 `libmath.so`，用于动态链接。

---

##### **(2) 创建 `main_dynamic.c`**
```c
// main_dynamic.c
#include <stdio.h>
#include "math_lib.h"

int main() {
    add(20, 10);
    subtract(20, 10);
    return 0;
}
```

---

##### **(3) 编译 & 运行**
```sh
gcc main_dynamic.c -o main_dynamic -L. -lmath
./main_dynamic
```
⚠️ 运行时可能会报错：
```
./main_dynamic: error while loading shared libraries: libmath.so: cannot open shared object file
```
**因为系统找不到 `libmath.so`，需要告诉它库的位置**：
```sh
export LD_LIBRARY_PATH=.:$LD_LIBRARY_PATH
./main_dynamic
```
**输出：**
```
Add: 20 + 10 = 30
Subtract: 20 - 10 = 10
```

---

##### **4. 总结**
|  | **静态链接** | **动态链接** |
|---|---|---|
| 编译方式 | `ar rcs libmath.a math_lib.o` | `gcc -shared -fPIC math_lib.c -o libmath.so` |
| 运行依赖 | **不需要外部库**（直接包含代码） | **需要 `libmath.so`**（否则运行时报错） |
| 可执行文件大小 | **更大**（包含库代码） | **更小**（库分开存放） |
| 适用场景 | 嵌入式、独立软件 | 共享库、插件、系统库 |

这个例子适合新手学习，手把手体验**静态链接**和**动态链接**的区别！🔥 🚀



---



## 1.3 gcc 实战介绍

>下图为目录结构
![目录结构](https://i-blog.csdnimg.cn/direct/8d211a0411e54891ad5b6e00b7d25a3a.png)--- 需要编译main.c文件，依赖的一个mat.h文件在其他目录，
![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/56d2abbd12454404bfdc7acde7c235b2.png)

>需要先将main.c 编译成main.o，依赖mat.h头文件的 mat.c 编译成mat.o 然后将两个.o文件链接成个可执行文件，（如果多个文件也类似 先编译mian.c 成目标文件.o 然后再链接其他需要的.o）
![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/58f04b34c42846fcbfe8ace318065f80.png)
-----
---
# 2. makefile 知识
## 2.1 makefile基础教程
### 2.1.1 V1.0版本教程
```bash
# 最基础版本的makefile
# test是目标 ，main.o sub.o是依赖，执行make 指令时候先执行这条指令，
# 发现依赖main.o 没有就 先去执行
# main.o : main.c
# 	gcc -c -o main.o main.c 
# sub.o没有就先去执行最下边的sub.o : sub.c
test : main.o sub.o
	gcc -o test main.o sub.o

main.o : main.c
	gcc -c -o main.o main.c

sub.o : sub.c
	gcc -c -o sub.o sub.c

clean:
	rm *.o test -f
```
### 2.1.2 V2.0版本教程
```bash
# 目标 test 依赖于 main.o 和 sub.o
# 这意味着：首先需要先编译 main.o 和 sub.o，然后才能生成可执行文件 test
test : main.o sub.o
	# 链接 main.o 和 sub.o 生成可执行文件 test
	# gcc 是编译器，-o test 指定输出的可执行文件名
	gcc -o test main.o sub.o

# 规则：如何从 .c 文件编译成 .o 文件
# %.o : %.c 表示：所有的 .c 文件都会通过此规则生成对应的 .o 文件
%.o : %.c
	# 使用 gcc 编译器将 .c 文件编译成 .o 文件
	# -o $@ 让编译器将输出文件名设置为目标文件名（即 .o 文件）
	# $< 代表当前规则中的依赖文件（即 .c 文件）
	gcc -c -o $@ $<

# clean 目标用于清理编译过程中产生的临时文件和可执行文件
# 这里的 *.o 是指所有的 .o 文件，test 是指可执行文件
clean:
	# 删除所有的 .o 文件和 test 可执行文件
	# -f 表示强制删除，不会报错如果文件不存在
	rm *.o test -f
```

#### **详细解释：**

1. **`test : main.o sub.o`**
   - `test` 目标依赖 `main.o` 和 `sub.o` 文件，意思是要生成 `test` 可执行文件，必须先生成 `main.o` 和 `sub.o`。

2. **`gcc -o test main.o sub.o`**
   - 使用 `gcc` 编译器将 `main.o` 和 `sub.o` 链接成一个名为 `test` 的可执行文件。

3. **`%.o : %.c`**
   - 这是一个**模式规则**，表示如何从 `.c` 文件生成 `.o` 文件。
   - `%` 是通配符，它表示任意文件名。
   - 例如，`main.c` 会被编译成 `main.o`，`sub.c` 会被编译成 `sub.o`，依此类推。

4. **`gcc -c -o $@ $<`**
   - `gcc -c` 是编译命令，表示编译源代码但不进行链接。
   - `$@` 是自动变量，表示规则的目标文件（即 `.o` 文件）。
   - `$<` 是自动变量，表示规则的第一个依赖文件（即 `.c` 文件）。
   - 所以，`gcc -c -o $@ $<` 就是将 `.c` 文件编译成 `.o` 文件。

5. **`clean` 目标**
   - `clean` 目标用于清理编译过程中产生的临时文件，比如 `.o` 文件和生成的可执行文件 `test`。
   - `rm *.o test -f` 表示强制删除所有的 `.o` 文件和 `test` 文件。
   - 使用 `clean` 目标可以让你清理工作目录，准备重新编译。

#### **总结：**
- **`Makefile` 通过规则定义了如何编译和链接文件**，例如如何从 `.c` 文件生成 `.o` 文件，如何将 `.o` 文件链接成最终的可执行文件。
- `clean` 目标用于清理编译产生的中间文件，使得每次构建都能在干净的环境下开始。

---
## 2.2 makefile 常用函数介绍
### 2.2.1**📌 `foreach` 函数简介**
`foreach` 是 `Makefile` 的**循环函数**，用于**遍历列表中的每个元素**，然后执行某个操作。  

📌 **基本语法：**
```make
$(foreach 变量, 列表, 表达式)
```
- `变量`：循环变量，每次迭代时存储当前元素。
- `列表`：需要遍历的列表，多个元素用空格分隔。
- `表达式`：对每个元素执行的操作，通常是字符串替换、变量计算等。

---

#### **🚀 示例 1：遍历文件列表**
```make
SRC = main.c sub.c utils.c
OBJ = $(foreach file, $(SRC), $(file:.c=.o))

all:
	@echo "源文件: $(SRC)"
	@echo "目标文件: $(OBJ)"
```
#### **🔍 解析**
- `SRC` 里有 `main.c sub.c utils.c`
- `$(foreach file, $(SRC), $(file:.c=.o))`
  - 遍历 `SRC` 里的每个 `.c` 文件
  - 替换 `.c` 为 `.o`
  - 结果：`main.o sub.o utils.o`

✅ **运行 `make`**
```sh
$ make
源文件: main.c sub.c utils.c
目标文件: main.o sub.o utils.o
```

---

#### **🚀 示例 2：创建多个目录**
```make
DIRS = build build/app build/src

all:
	@echo "创建目录: $(DIRS)"
	$(foreach dir, $(DIRS), mkdir -p $(dir);)
```
#### **🔍 解析**
- `DIRS` 包含 `build`、`build/app`、`build/src`
- `foreach` 遍历 `DIRS`，对每个目录执行 `mkdir -p $(dir);`

✅ **运行 `make`**
```sh
$ make
创建目录: build build/app build/src
```
✔ **保证所有目录都被创建！**

---

#### **🚀 示例 3：批量添加前缀**
```make
FILES = a.txt b.txt c.txt
PREFIXED = $(foreach f, $(FILES), dir/$(f))

all:
	@echo "加前缀后的文件列表: $(PREFIXED)"
```
#### **🔍 解析**
- `FILES = a.txt b.txt c.txt`
- `$(foreach f, $(FILES), dir/$(f))`  
  - 遍历 `FILES`，给每个文件加上 `dir/`
  - 结果：`dir/a.txt dir/b.txt dir/c.txt`

✅ **运行 `make`**
```sh
$ make
加前缀后的文件列表: dir/a.txt dir/b.txt dir/c.txt
```

---

#### **🚀 示例 4：给 `CFLAGS` 添加 `-I` 目录**
```make
INC_DIRS = include src headers
CFLAGS = $(foreach dir, $(INC_DIRS), -I$(dir))

all:
	@echo "编译选项: $(CFLAGS)"
```
#### **🔍 解析**
- `INC_DIRS = include src headers`
- `$(foreach dir, $(INC_DIRS), -I$(dir))`
  - 遍历 `INC_DIRS`
  - 每个目录前加 `-I`
  - 结果：`-Iinclude -Isrc -Iheaders`

✅ **运行 `make`**
```sh
$ make
编译选项: -Iinclude -Isrc -Iheaders
```
✔ **适用于自动添加 `gcc` 头文件路径！**

---

**🔹 总结**
| 用途 | 示例 |
|------|------|
| **批量替换** | `$(foreach f, $(SRC), $(f:.c=.o))` |
| **创建多个目录** | `$(foreach d, $(DIRS), mkdir -p $(d);)` |
| **给路径加前缀** | `$(foreach f, $(FILES), dir/$(f))` |
| **批量构造编译选项** | `$(foreach dir, $(INC_DIRS), -I$(dir))` |

🔥 **`foreach` 让 `Makefile` 更强大，适合处理文件列表、目录创建、编译参数等任务！** 🚀

---

#### 实战演示： 把所有目录中的所有.c文件找出来赋值给变量
 1. 使用 foreach 函数遍历 SRC_DIRS 中的每个目录，使用 wildcard 函数查找目录中的所有 .c 文件
```bash
# 源代码目录
SRC_DIRS = app Driver/src

# 找到所有目录下的.c文件，赋值给SRC_FILES 变量
# 使用 foreach 函数遍历 SRC_DIRS 中的每个目录，使用 wildcard 函数查找目录中的所有 .c 文件
SRC_FILES = $(foreach dir, $(SRC_DIRS), $(wildcard $(dir)/*.c))
```
>**#SRC_FILES 实际打印出来的效果为：【SRC_FILES=  app/main.c  Driver/src/mul.c Driver/src/mat.c Driver/src/add.c Driver/src/sub.c 】**
![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/92901a881b4c4c02bb1dd3f1b361629a.png)
### 2.2.2 **模式转换 `OBJ = $(SRC:.c=.o)`**
这一行 `Makefile` 代码的作用是**把 `SRC` 变量中的 `.c` 文件转换成 `.o` 文件**，生成 `OBJ` 变量。

---
#### **📌 代码拆解**
```make
SRC = main.c sub.c
OBJ = $(SRC:.c=.o)  #OBJ = main.o sub.o
```
- `SRC = main.c sub.c`  
  👉 `SRC` 变量存储所有的源文件 `.c`。
- `$(SRC:.c=.o)`  
  👉 `Makefile` **模式替换**，把 `SRC` 里面的 `.c` **替换成** `.o`，相当于：
  ```make
  OBJ = main.o sub.o
  ```

---

#### 实战案例：如下
```bash
# 获取当前目录下所有的 .c 文件，赋值给变量 SRCS
SRCS = $(wildcard *.c)

# 将 当前目录下 所有.c 文件替换为 .o 文件放到 build 目录下
OBJS = $(SRCS:%.c= $(BUILD_DIR)/%.o)
```
#### **🔍 `$(VAR:旧模式=新模式)` 语法**
`Makefile` 允许用 `$(VAR:旧模式=新模式)` **批量替换**字符串：
- `$(SRC:.c=.o)`  
  👉 **找到 `SRC` 里的 `.c`，替换成 `.o`**
- **示例**
  ```make
  FILES = a.txt b.txt c.txt
  NEW_FILES = $(FILES:.txt=.md)
  ```
  - `NEW_FILES` 的值变成：`a.md b.md c.md`

---

#### **🚀 示例**
#### **`Makefile`**
```make
CC = gcc
CFLAGS = -Wall -O2 -g -MMD

SRC = main.c sub.c   # 定义源文件
OBJ = $(SRC:.c=.o)   # 自动把 .c 转成 .o

all: test

test: $(OBJ)         # 依赖所有 .o 文件
	$(CC) -o $@ $^

%.o: %.c             # 规则：编译 .c -> .o
	$(CC) -c $(CFLAGS) -o $@ $<

clean:
	rm -f $(OBJ) test *.d

.PHONY: all clean
```

---

#### **🛠 编译过程**
```sh
$ make
gcc -c -Wall -O2 -g -MMD -o main.o main.c
gcc -c -Wall -O2 -g -MMD -o sub.o sub.c
gcc -o test main.o sub.o
```
✅ `make` **自动生成 `main.o sub.o`，并链接 `test`！**

---

**💡 结论**
- `OBJ = $(SRC:.c=.o)` **自动转换** `.c -> .o`，省去手写 `main.o sub.o`。
- 适合 **多个 `.c` 文件的项目**，维护起来更方便！
- 和 `%.o : %.c` 规则配合，`make` **智能编译变化的文件**，提高效率！ 🚀
 
 -----
 
### 2.2.3**`wildcard` 函数简介**

`wildcard` 是 `Makefile` 中的一个内置函数，用于**匹配指定路径下符合条件的文件**。通常，它用于批量匹配文件，比如查找所有的 `.c` 文件、`.h` 文件等。

📌 **基本语法：**
```make
$(wildcard pattern)
```
- `pattern`：指定匹配模式，可以是通配符形式（例如 `*.c`、`dir/*.h` 等）。

---

#### **📚 使用场景**

#### **1. 查找当前目录下的 `.c` 文件**
```make
SRC = $(wildcard *.c)
```
- `$(wildcard *.c)` 会匹配当前目录下的所有 `.c` 文件，并返回一个文件列表（例如 `main.c sub.c`）。
  
#### **2. 查找某个子目录下的 `.h` 文件**
```make
INCLUDES = $(wildcard include/*.h)
```
- `$(wildcard include/*.h)` 会匹配 `include` 目录下的所有 `.h` 文件。

---

#### **🚀 示例 1：批量处理 `.c` 文件**
假设我们有以下项目结构：
```
/my_project
│── Makefile
│── main.c
│── sub.c
│── utils.c
```

#### **Makefile：**
```bash
# 自动找到所有 .c 文件
SRC = $(wildcard *.c)

# 生成 .o 文件
OBJ = $(SRC:.c=.o)

# 默认目标
all: $(OBJ)
	@echo "All object files: $(OBJ)"

# 编译规则：.c 文件编译成 .o 文件
%.o: %.c
	gcc -c -o $@ $<

# 清理目标
clean:
	rm -f $(OBJ)
```

#### **运行过程：**
1. `$(wildcard *.c)` 会查找当前目录下的所有 `.c` 文件，得到 `main.c sub.c utils.c`。
2. `$(SRC:.c=.o)` 会将每个 `.c` 文件转换为 `.o` 文件，即 `main.o sub.o utils.o`。
3. `make` 执行时，会编译这些 `.c` 文件并生成 `.o` 文件。

---

#### **🚀 示例 2：匹配子目录中的文件**

假设我们有以下目录结构：
```
/my_project
│── Makefile
│── include
│   └── header1.h
│   └── header2.h
│── src
│   └── main.c
│   └── sub.c
```

#### **Makefile：**
```bash
# 自动获取 include 目录下的所有 .h 文件
HEADERS = $(wildcard include/*.h)

# 自动获取 src 目录下的所有 .c 文件
SRC = $(wildcard src/*.c)

# 生成的目标文件 .o
OBJ = $(SRC:.c=.o)

# 默认目标
all: $(OBJ)
	@echo "All object files: $(OBJ)"

# 编译规则：.c 文件编译成 .o 文件
%.o: %.c
	gcc -c -o $@ $<

# 清理目标
clean:
	rm -f $(OBJ)
```

#### **运行过程：**
1. `$(wildcard include/*.h)` 会获取 `include` 目录下的所有 `.h` 文件，结果是 `header1.h header2.h`。
2. `$(wildcard src/*.c)` 会获取 `src` 目录下的所有 `.c` 文件，结果是 `main.c sub.c`。
3. `$(SRC:.c=.o)` 会将 `.c` 文件转换成 `.o` 文件，得到 `main.o sub.o`。

---

#### **💡 使用 `wildcard` 的好处**
- **自动化**：不需要手动列出文件，`wildcard` 函数自动匹配所有符合条件的文件。
- **灵活性**：可以匹配文件类型（如 `*.c`、`*.h`）、目录下的文件等，支持通配符。
- **动态更新**：当你添加或删除文件时，`wildcard` 会自动更新文件列表，避免手动维护文件名。

---

#### **🌟 总结**
- `wildcard` 是 `Makefile` 中非常有用的一个函数，它能**自动匹配符合指定模式的文件**。
- 适用于 **自动处理文件列表**，避免手动添加文件路径和名称。

---
### 2.2.4**`addprefix` 函数简介**

`addprefix` 是 `makefile` 中的一个字符串处理函数，作用是**给一组字符串（通常是文件名）统一加上前缀**。
#### **📌 语法**
```makefile
$(addprefix 前缀, 单词列表)
```
- `前缀`：要添加的字符串（通常是路径）。
- `单词列表`：多个单词（通常是文件名）用空格分隔。

---

#### **🔹 示例 1：给文件名添加路径**
```makefile
SRC_FILES = main.c utils.c driver.c
SRC_PATH = src/

FULL_SRC_FILES = $(addprefix $(SRC_PATH), $(SRC_FILES))

# 结果：
# FULL_SRC_FILES = src/main.c src/utils.c src/driver.c
```
**📖 解释**：  
- `SRC_FILES` 里有 3 个 `.c` 文件。  
- `addprefix` 给每个文件名前面加上 `src/`，形成完整路径。

---

#### **🔹 示例 2：给变量加上 `./bin/` 目录**
```makefile
EXECUTABLES = program1 program2 program3
BIN_DIR = ./bin/

BIN_FILES = $(addprefix $(BIN_DIR), $(EXECUTABLES))

# 结果：
# BIN_FILES = ./bin/program1 ./bin/program2 ./bin/program3
```
这里 `program1`、`program2`、`program3` 都变成了 `./bin/` 目录下的可执行文件。

---

#### **🔹 示例 3：配合 `addsuffix`（添加后缀）**
```makefile
SRC_FILES = main utils driver
OBJ_FILES = $(addsuffix .o, $(SRC_FILES))

# 结果：
# OBJ_FILES = main.o utils.o driver.o
```
然后再用 `addprefix` 加上路径：
```makefile
OBJ_DIR = build/
FULL_OBJ_FILES = $(addprefix $(OBJ_DIR), $(OBJ_FILES))

# 结果：
# FULL_OBJ_FILES = build/main.o build/utils.o build/driver.o
```
这样就得到了完整的目标文件路径。

---

#### **📌 总结**
| 函数 | 作用 | 示例 |
|---|---|---|
| `$(addprefix prefix, list)` | 给 `list` 里的每个元素加上 `prefix` | `$(addprefix src/, main.c utils.c)` → `src/main.c src/utils.c` |
| `$(addsuffix suffix, list)` | 给 `list` 里的每个元素加上 `suffix` | `$(addsuffix .o, main utils)` → `main.o utils.o` |

这个 `addprefix` 很常用于 `makefile`，主要是**自动管理文件路径**，避免手写路径出错，提高可维护性！😃





