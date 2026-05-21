```c

#gcc -o hello1 hello.c -v
# -o x y.c  生成可执行文件x ,依赖文件是y.c 此操作直接一步到位，把预处理，编译，汇编，链接操作全给做了直接一步到位生成可执行文件 

# -v 意思是打印编译时候显示的信息 
# -o 生成目标文件
# -I 指定头文件目录
# -L 指定链接时候的库文件目录
# -l 指定链接哪一库文件

#gcc hello.c #直接编译一步到位 生成个a.out可执行文件
#gcc -c main.c #直接 一步到位 包括预处理 编译 汇编 产生个mian.o 目标文件，如果文件名是ace.c 那就产生个ace.o目标文件

#cd source/

#gcc -E -o hello.i show_compile.c  # -E 预处理选项 hello.i产生预处理后的文件
#gcc -S -o hello.s hello.i # -S 编译程序 将预处理后的高级语言文件 编译成汇编文件
#gcc -c -o hello.o main.c  # -c选项 一步到位 把预处理 编译 汇编 全做了，直接生成个目标文件 hello.o
#gcc -o hello hello.o # -o 选项就是把目标文件进行个链接 将hello.o 以及其他的库文件.o文件 链接成个可执行文件hello

# gcc -c -o main.o main.c
# gcc -o test main.o -Lmath_lib/include/ -lmath 



# 静态库使用教程 -Ldir dir是静态库的文件目录 ,-lname 假如 静态库是 libmath.a 则这个name 就是math
# gcc -o test main.c -Lmath_lib/include/ -lmath 

# cd math_lib/
# ./compile.sh
# cd ..

./rm-rf.sh #把之前编译的文件全给删除了

# # -I math_lib/include 后边是跟的头文件目录  ./意思白哦是当前路径下
# gcc -c -o main.o main.c -I ./math_lib/include -I ./math_lib/library

# # 静态库使用教程 -Ldir dir是静态库的文件目录 ,-lname 假如 静态库是 libmath.a 则这个name 就是math
# gcc -o test main.o -Lmath_lib/library -lmath0 -lmath1 #math0是静态库0 math1是静态库1
# ./test #运行程序 


# 包含头文件 -I math_lib/include 后边是跟的头文件目录  ./意思白哦是当前路径下
gcc -c -o main.o main.c -I ./math_lib/include -I ./math_lib/library

# 静态库使用教程 -Ldir dir是动态库的文件目录 ,-lname 假如 动态库是 libmath0.a 则这个name 就是math0
gcc -o test main.o -Lmath_lib/library -lmath  #math是动态库 math1是动态库1

#设置下程序中动态库的路径 因为程序运行时候需要调用 但是静态库就无需这样
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/home/wls/emmd_-linux/4.0_makefile/math_lib/library
./test #运行程序 



# 头文件知识介绍 #include <stdio.h>  #include "stdio.h" 
#      < >的意思是去工具链指定目录查找头文件 ""的意思是去当前目录中去查找头文件
#  "math_lib/include/ar_opreat1.h"  除非你是像这样 在包含的时候直接指定头文件路径   
#  在编译的时候也可以 使用gcc命令进行 -I dir -I dir 指定单个或者多个头文件路径
#                                   dir 就是头文件路径 可以是绝对路径 也可以是相对路径
```


```c
#gcc -o hello1 hello.c -v
# -o x y.c  生成可执行文件x ,依赖文件是y.c 此操作直接一步到位，把预处理，编译，汇编，链接操作全给做了直接一步到位生成可执行文件 
# -v 意思是打印编译时候显示的信息 
# -o 生成目标文件
# -I 指定头文件目录
# -L 指定链接时候的库文件目录
# -l 指定链接哪一库文件

# --- 以下没有指定 编译的.c文件路径 和 生成的目标文件.o 或者.i 或者可执行文件test的路径
# 那就默认是 他们都在同一级别的目录

#gcc hello.c #直接编译一步到位 生成个a.out可执行文件
#gcc -c main.c #直接 一步到位 包括预处理 编译 汇编 产生个mian.o 目标文件，如果文件名是ace.c 那就产生个ace.o目标文件
#gcc -E -o hello.i show_compile.c  # -E 预处理选项 hello.i产生预处理后的文件
#gcc -S -o hello.s hello.i # -S 编译程序 将预处理后的高级语言文件 编译成汇编文件
#gcc -c -o hello.o main.c  # -c选项 一步到位 把预处理 编译 汇编 全做了，直接生成个目标文件 hello.o
#gcc -o hello hello.o # -o 选项就是把目标文件进行个链接 将hello.o 以及其他的库文件.o文件 链接成个可执行文件hello


#cd source/
#rm -rf *.o *.a #删除所有的目标文件.o文件 和静态库.a文件

# 默认将.o目标文件放在和.c文件的同一文件夹中
# gcc -c -o math0.o ar_opreat0.c #把这个.c文件 编译成目标.o文件
# gcc -c -o math1.o ar_opreat1.c #把这个.c文件 编译成目标.o文件

# ar crs  libmath0.a math0.o # 把 math0.o math1.o 制作成静态库文件 libmath.a 
# ar crs libmath1.a math1.o # 把 math0.o math1.o 制作成静态库文件 libmath.a 
# 静态库只是 多个目标文件的集合而已，本质还是属于 汇编语言编译后生成的 机器码：目标文件  ，还需要链接才能生成可执行文件
# 【静态库文件命名规范 lib为前缀，中间是静态库名，扩展名为.a】


# rm -rf library/*.a library/*.o  #删除library路径下 所有的 静态库.a文件 和目标文件.o

rm -rf library/*.a library/*.so  #删除library路径下 所有的 动态库.so文件 和目标文件.o

#---------在实际项目开发中 项目中几十个上百个.c .h头文件那都是常事儿 
# 所以不可能 上百个.c .h文件都放在同一个文件夹下进行编译运行
# 所以要进行 分类管理 同一类功能模块的文件放下同一个文件夹 源代码.c文件 和头文件.h 还有封装的库.a .so文件都要分开放的
# 所以 在编译项目的时候直接 指定编译某个文件夹里边的.c文件 编译生成到指定的文件夹下
# 例如下边：此时的根目录为：math_lib 然后把 source/下的ar_opreat0.c文件 指定编译生成的math0.o 放到library/文件夹下

# 此编译脚本当前的目录是math_lib -I后边跟的./include是ar_opreat0.c里边包含的头文件的目录
#    也可以写成绝对路径  ./意思是当前目录下的include的文件夹 也可以写成include                             
gcc -c -o library/math0.o source/ar_opreat0.c  -I ./include #把source目录下的.c文件编译了 并且把生成的目标文件 命名为math0.o放到library目录下
gcc -c -o library/math1.o source/ar_opreat1.c -I ./include     #不嫌麻烦 也可以写绝对路径 就是从根目录下开始算 eg:/home/wls/linux.......

# #生成静态库 .a文件
# ar crs library/libmath0.a library/math0.o # 把 此目录(library)下的目标文件 编译生成到 liblibrary目录下 并命名为libmath0.a
# ar crs library/libmath1.a library/math1.o 
gcc -shared -o library/libmath.so  library/math0.o library/math1.o
# gcc -shared -o library/libmath1.so  library/math1.o 

rm -rf library/*.o #既然目的要生成 静态库 那么过程中产生的.o目标文件 就没用了 所以删除了吧

```
