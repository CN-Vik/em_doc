# **1. 前提基础知识（ARM M核异常 压栈流程）**
>M核栈增长方向是地址逐渐减小的（TIPS：有的架构的处理器是增大的例如8051内核，而有的像ARM A核心是可设置的 可以增大也可以减小）
>ARM Coretex-M核心常用的有M0 M3 M4，下图第一个为M3的压栈情况，M4带FPU和浮点寄存器的并不一样
>M3的假设hardfault ISR里边就有个while(1);语句，那看SP的栈顶值，从栈顶开始数算第一个向上高地址方向数到第六个就是LR的值，把或者值读出来赋值给PC（定义个函数指针），就可以跳转到问题发生处了，然后结合hardfault状态寄存器（0XE000ED2C,SCB->HSFR）的值 大概能分析出产生hardfault的原因
![图1](https://i-blog.csdnimg.cn/direct/bee35196ab4d4ad2ad50df79ce7c7e77.png)

![图2](https://i-blog.csdnimg.cn/direct/5d256631e658425b892dd13cc7fdc01e.png)
---
![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/f744e1f81cdb4c96aca429cd9a774aad.png)



**下图为M4核的压栈情况，因为带有浮点寄存器所以压栈的内容也多 **
![M4核心](https://i-blog.csdnimg.cn/direct/73c139f7777146dc81444f84be71a215.png)
>ARM CM4核心带浮点处理器FPU的，压栈的东西还不一样
---
进入hardfult后看MSP或者SP的值，看下边第二章图如果hardfult里边啥都没有，就只有个while(1){} 可以用第二张图判断SP+20里边存储的就是LR寄存器的值，也就是产生hardfault前导致的问题的地方，把这个值像第一张图一样写给PC就能定位到因为哪里产生的hardfault，

> 现在还在推出来个类似的公式解决方法，发现函数里边写的东西越多函数栈指针会变化，也就不能直接取*(SP+24)|*(SP+23)|*(SP+22)|*(SP+21)，拼凑出来的值就是LR的值可以直接赋值给PC，定义个函数指针给赋值了，在调用在hardfault里边直接就可以跳过去

![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/e9d9af098d364b6f8356a35490627e6f.png)
![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/fbd3a05d20874b4094c0536be1baa3ae.png)
---
![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/a13f32bdf1304aca8139996baeb4b0ee.png)


**公式化的方法（待在M3内核上验证 理论上来说MSP是+0x14的），把下列代码替换掉原来的HardFault_Handler ISR**

```c
/*----------------hardfault 调查原因方案，需要debug 单步执行--------------------*/
void(*f1)(void);
uint32_t result;

void HardFault_Handler(void)
{
	result= __get_MSP()+0x1C; //实际操作Arm Coretex-M0核心的+0x1c，M3核的+0X14
	f1=(void (*)(void))(*(unsigned int *)result);
	f1();

	while (1);
}
/*----------------hardfault 调查原因方案，需要debug 单步执行--------------------*/
```


## ARM官方给出的hardfault 原因分析方法
![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/63a2618e93bc4366860dde7ff42cc2f3.png)
![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/4711d992cc3745f992c64da7d0558b1b.png)

> **总结下来就是利用处理异常时候会进行压栈处理，也会把LR的值压进去，然后分析栈中的LR的值，设置PC跳到导致产生hardfault的地方，然后结合上边的hardfault状态寄存器进行分析问题的具体原因**
![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/c2c844fdebf442b9be98661ac94a7f41.png)


> #  **<引流>**
> ##  **Github:** [HardFault问题定位与分析](https://github.com/UFO281/HardFault.git)
> ## **知乎：**[ARM Coretex-M核单片机，例如STM32单片机遇到HardFault问题应该如何解决？](https://www.zhihu.com/question/13536848639/answer/111838910932)