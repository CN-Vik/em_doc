写的虽然有点混乱，可以先去看韦东山嵌入式linux 第四期里边的makefile教程 那个知识点不会，在从我这里边找例子解释


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
gcc -o test main.o -Lmath_lib/library -lmath0 -lmath1 #math0是动态库0 math1是动态库1

#设置下程序中动态库的路径 因为程序运行时候需要调用 但是静态库就无需这样
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/home/wls/emmd_-linux/3.0_dymatic/math_lib/library
./test #运行程序 



# 头文件知识介绍 #include <stdio.h>  #include "stdio.h" 
#      < >的意思是去工具链指定目录查找头文件 ""的意思是去当前目录中去查找头文件
#  "math_lib/include/ar_opreat1.h"  除非你是像这样 在包含的时候直接指定头文件路径   
#  在编译的时候也可以 使用gcc命令进行 -I dir -I dir 指定单个或者多个头文件路径
#                                   dir 就是头文件路径 可以是绝对路径 也可以是相对路径
```


```powershell

include $(FW_PROJ_ROOT)/build/makerule/app/makefile.ex 
将 makefile.ex 文件的内容合并到当前的Makefile中。这个makefile.ex 头文件类似于其他,c文件 里边也是写的函数 
啥的和main.c文件中的内容相似，不过这么做就是把他模块化封装了，方便其他调用

include $(FW_PROJ_ROOT)/build/makeconfig 
$(FW_PROJ_ROOT) 是一个变量，表示指定的项目根目录路径，它可能是一个绝对路径或相对于当前Makefile的路径

这条语句可以在Makefile文件中直接调用 makeconfig 文件中定义的变量和规则。
通过包含 makeconfig 文件，Make工具会将 makeconfig 文件的内容合并到当前的Makefile中，
相当于把 makeconfig 中的内容复制粘贴到 include 语句所在位置

test : main.o
# 动态库使用教程 -Ldir dir是动态库的文件目录 ,-lname 假如 动态库是 libmath0.a 则这个name 就是math0
	gcc -o test main.o -Lmath_lib/library -lmath  # 链接程序
# math是动态库 math1是动态库1 
#设置下程序中动态库的路径 因为程序运行时候需要调用 但是静态库就无需这样
# 	export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/home/wls/emmd_-linux/4.0_makefile/math_lib/library
# # ./test #运行程序 

# make -f my_makefile 当目录下有多个Makefile的时候 而且名字不是标准的Makefile而是其他名字 \
这个时候 就可以用-f选项编译指定的makefile了

# make -f my_makefile clean 编译指定makefile的目标

main.o : main.c
# makefile 语法规则 开头必须是tab键
# 	make -C math_lib/ clean #进入 math_lib下清除下编译生成的文件
# 上述命令的含义就是 进入到math_lib目录下执行 make clean 命令
# 	make -C math_lib/  #进入到math_lib目录下 执行make命令 编译makefile脚本
# make -C dir 意思是进到dir路径下执行 make命令 也就是进到dir目录下运行 makefile文件 
# 上述命令的含义就是 进入到math_lib目录下 执行make 编译命令 然后再退回来

# 包含头文件 -Imath_lib/include 后边是跟的头文件目录  ./意思白哦是当前路径下
	gcc -c -o main.o main.c -I./math_lib/include -I./math_lib/library #编译成目标文件main.o


# 下边起一个声明作用 如果执行目标 make clean时候 文件夹内有和clean同名的文件那就得出问题 \
但是 使用下边这个声明下 目标就没问题
.PHONY:clean 

clean: #删除 library文件夹下所有后缀为.a .so .o的文件
# rm -rf $(PWD)/*.a $(PWD)/*.so $(PWD)/*.o    ./表示当前文件夹下的文件  
	rm -rf ./*.a ./*.so ./*.o ./test
# ./math_lib/library/*.a 		\
# ./math_lib/library/*.so   \
# ./math_lib/library/*.o
	@echo currentpwd: $(PWD)    
#在shell 打印 当前路径 curpwd: PWD是个变量这变量里边存储的是当前路径 \
  $(PWD)意思就是调用PWD变量  

# := 等于第一次赋值内的变量的内容 就算c变量内容以后变了 a的值还是等于c变量的初始值内容
a := $(c) #直接赋值 而且而且c变量值改变后 a变量的值也不改变
b = $(c) # 实时赋值  c里边儿的值改变后 b的值也立刻变成c里边的值
c = abc
c = 123
d = 111
d ?= 996 #d变量 在前边没定义 赋值的话 就直接= 996 
# 但是在前边赋值的话 就等于前边赋的值111
d += 211 #在原来d基础上 +个211 d=111 211

all:
	@echo a = $(a) 
	@echo b = $(b) 
	@echo d = $(d)


name :=	a b c 
# foreach 类似于查找替换函数 将name变量中的 a b c替换成 a.o b.o c.o n是个临时变量 
# 其实流程就是 在name中遍历查找 分别赋值给 n 然后再替换成 $(n).o 再赋值给name
files := $(foreach n,$(name),$(n).o)

sources := f.s b.c v.c u.h b.h math_lib/ library/
aim_sources := $(filter b.h %.c %s %/,$(sources)) 
# filter 类似于查找函数 上边在source变量中 找出 所有.c  .s 和文件路径 还有b.h文件 并赋值给aim_sources

taraim_sorce := $(filter-out %.s %/,$(sources))
# filter-out 函数 就是在sources 变量中找出 除了.s %/文件路径 其他都打印出来

wil_sourc := $(wildcard math_lib/library/*.o math_lib/source/*.c *.c *.o) #后边*.c *.o的意思是找出当前路径下 .c .o文件 也可以加路径 ./*.c 也表示当前路径下
# 上述 wildcard函数 找出 math_lib/library/下所有.o文件 和math_lib/source/所有.c文件 

# export 关键字的作用是 类似于c中的extern关键字作用 声明变量让其他文件中也可以调用
export past_var = a.c b.c start.s kk.h
# unexport past_var = a.c b.c start.s kk.h  关键字的作用是让这个past_var变量为局部变量 类似于 static 全局变量，外部文件不可调用这个变量

past = $(patsubst %.c, %.o , $(past_var) )
# patsubst函数的作用是 查找替换 把past_var变量找出所有的.c文件 然后替换成.o文件 然后赋值给past变量

function: 
	@echo files = $(files) # 输出 files= a.o b.o c.o 
	@echo source = $(aim_sources)
	@echo taraim_sorce = $(taraim_sorce)
	@echo wil_source = $(wil_sourc)
	@echo past_var = $(past_var)	
	@echo past = $(past)

```


```powershell
#makefile 注意！！点 1.如果有目标或者依赖使用的是绝对路径 \
那所有的依赖和目标都得使用绝对路径 \
 使用相对路径就会出错    \
	PWD是个变量这变量里边存储的是当前路径 \
  $(PWD)意思就是调用PWD变量  \
 使用通配符 $@  $^ $< 的注意事项  目标名字和依赖名字必须一致  \
 例如 依赖是  ar_opreat0.c  那目标的名字 也必须是 ar_opreat0.o 如果是其他名字像 math0.o那用通配符就会出问题！！

mathlib_target = library/libmath.so

mathlib_relyon= library/ar_opreat0.o library/ar_opreat1.o


 # 左侧 library/libmath.so 是要生成的目标 :右侧 是生成目标所需要的依赖 \
    当依赖文件修改后 makefile工具会自动检测并执行命令 更新目标 没有的话就不会执行
 mathlib_target : $(mathlib_relyon)
	gcc -shared -o $(mathlib_target)  $^
# gcc -shared -o library/libmath.so  library/ar_opreat0.o library/ar_opreat1.o
# $@ 目标的集合 == library/libmath.so ，依赖文件的集合 $^ == library/ar_opreat0.o library/ar_opreat1.o
# 使用 $@ $^ 时候 左侧是目标 右侧是依赖 依赖是 a.c b.c 生成的目标只会是 a.o b.o 不能是其他
# $^ 表示的依赖文件的集合 内部用空格隔开 如果有重复的则会自动从删除重复的 只保留一份


# 下边的意思是通配符 在这里代表的意思是 所有的.o目标文件 包括 math0.o math1.o
library/%.o : source/%.c
	gcc -c -o $@  $<  -I./include -I./library
# $@ 表示目标的集合 $<表示所有的依赖文件集合中的 第一个依赖文件
# -Ixx/dir  xx/dir 是.c文件中的头文件目录 一个-I后跟一个头文件的目录 

# library/math0.o : source/ar_opreat0.c  
# 	gcc -c -o library/math0.o source/ar_opreat0.c  -I ./include

# library/math1.o : source/ar_opreat1.c  
# 	gcc -c -o library/math1.o source/ar_opreat1.c  -I ./include


# 下边起一个声明作用 如果执行目标 make clean时候 文件夹内有和clean同名的文件那就得出问题 \
但是 使用下边这个声明下 目标就没问题
.PHONY:clean 

clean: #删除 library文件夹下所有后缀为.a .so .o的文件
	rm -rf library/*.a library/*.so library/*.o    
	@echo currentpwd: $(PWD)    
#在shell 打印 当前路径 curpwd: PWD是个变量这变量里边存储的是当前路径 \
  $(PWD)意思就是调用PWD变量  




```
