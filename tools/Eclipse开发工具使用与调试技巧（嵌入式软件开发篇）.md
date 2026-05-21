@[toc]
# 1. Eclipse 简介
>Eclipse 是一款集成开发工具（就是集成了 编辑器，编译器，调试功能于一体的集成工具 简称IDE，嵌入式领域常用的IDE有大名鼎鼎的Keil,IAR,CSS，没接触过keil CSS主要是用来TI DSP开发使用的用的也不多，就拿和Keil比较 绝对比keil更牛逼！下边再详细介绍下他的牛逼之处，做嵌入式开发我感觉keil的调试功能绝对宇宙NO.1）
![Eclipse使用界面](https://i-blog.csdnimg.cn/direct/24af1ae7c5ad4384872e0601db5f7018.png)
---
# 2. Eclipse使用教程（只讲常用的地方）
## 2.1 outline 功能
>说白了就是下图这个功能，可以概览当前.c .h文件包含的哪些结构体，变量 函数啥的 
![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/e4a7c3e9506c46ca8521cc76d31d2449.png)
>**操作步骤如下图**
![打开outline教程](https://i-blog.csdnimg.cn/direct/f24a937c1964404c9c6c949edc556cc9.gif)
![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/a169f4f73db5431f8e46d962db762048.png)
## 2.2 open call hierarchy 功能(查看函数，变量结构体成员等的调用关系)
![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/b7d575776bc54b8a95443474eccbd79f.png)
>**操作步骤如下图**
![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/e03f188be4a94854b3fc08adbee5a925.gif)

>**Eclispe 查看调用关系还有个很牛逼的地方就是可以查看变量，和结构体成员的调用关系**
![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/86b941fcc273471eadecd419dc33b157.gif)

## 2.3 搜索功能
###  2.3.1 全局字符搜索 快捷键（Ctrl+H）
>下图，最下边蓝色地方就是全局搜索出来的位置，whole word是全词匹配
![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/af8399782e9a4746bd4f23b3692ce51f.png)

> 实际操作演示如下图
> 点击变量名，按快捷键 Ctrl + H
![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/3d8ce7da2e6f4a438ca8764426200475.gif)
###  2.3.1 文件搜索功能- 快捷键（Ctrl+Shift+R）
![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/1668101f6cfb462786c4553be66f4bd6.gif)
# 3. Eclipse调试功能（最牛逼的特点！）
## 3.1 看API调用关系，从而理解程序运行流程
> 有些时候不知道函数执行流程，或者代码运行逻辑，就在想知道的那个API 那里打断点，一运行到那就停了，然后debug 框栏里就显示这个API的调用关系了
![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/54668527b7a44540a6cdf3ccfa911711.gif)
![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/fc075ac604024ad1bb456a06f463ca9f.png)
## 3.2 实战（看产品的程序运行流程）
>例如你有些时候不知道产品的某个功能的程序执行逻辑流程，你只知道最底层调用哪个API，然后就直接可以在那个API上打断点，debug 运行起来 你操作产品就会自动停在断点处，并在debug框内显示程序运行调用流程逻辑，便于你更快理解产品的功能代码逻辑
![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/985d1a4afa574bde9db486d31299e1f0.gif)
![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/caa459e2c6fd41e38eca2acbdecac0b7.png)

# 4. 对比Keil&&总结
>**相较于常用的keil 这些功能都是吊炸天的存在！**
>不过eclipse 也有个缺点就是UI主题不够好，代码注释功能不太行，还有自动补全功能，git功能 都比较差