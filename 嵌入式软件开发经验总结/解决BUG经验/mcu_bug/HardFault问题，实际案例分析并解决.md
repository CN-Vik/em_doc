@[toc]
# 1. HardFault 简介
> 简单来讲就是硬件异常， 详细介绍如下为AI回答，还算详细

**HardFault** 是 ARM Cortex-M 系列处理器中的一种异常（Exception），属于处理器内核的故障处理机制之一。它是处理器在检测到严重错误时触发的最高优先级异常之一，通常用于处理无法通过常规手段恢复的系统错误。

---

## 1.1 HardFault 的触发原因
HardFault 通常由以下原因触发：
1. **非法内存访问**：
   - 访问未映射的内存区域（如空指针解引用）。
   - 访问受保护的内存区域（如内核空间或只读区域）。
   - 访问未对齐的内存地址（在某些 Cortex-M 处理器中，未对齐访问会触发 HardFault）。

2. **堆栈溢出**：
   - 堆栈指针（SP）超出分配的堆栈空间，导致内存损坏。
   - 堆栈指针指向无效的内存区域。

3. **未定义指令**：
   - 处理器尝试执行一条无效或未定义的指令。

4. **总线错误**：
   - 访问外设或内存时，总线返回错误（如访问未初始化的外设寄存器）。

5. **异常处理失败**：
   - 在异常处理过程中发生错误（如嵌套异常或优先级配置错误）。

6. **浮点运算错误**（仅限支持浮点单元的 Cortex-M4/M7）：
   - 浮点运算异常（如除以零、溢出等）。

---

## 1.2 HardFault 的特点
1. **高优先级**：
   - HardFault 的优先级高于大多数其他异常（如 SVCall、PendSV 等），但低于 NMI（不可屏蔽中断）。

2. **不可屏蔽**：
   - HardFault 不能被屏蔽或禁用，一旦触发，处理器必须处理。

3. **调试友好**：
   - ARM Cortex-M 处理器提供了丰富的调试信息（如寄存器状态、堆栈内容等），便于定位问题。

---

## 1.3 HardFault 的处理流程
当 HardFault 发生时，处理器的行为如下：
1. **保存上下文**：
   - 处理器将当前的寄存器状态（如 PC、LR、PSR 等）压入堆栈。
   - 更新链接寄存器（LR）为特殊值（如 `0xFFFFFFF9`），表示从异常返回。

2. **跳转到 HardFault 处理程序**：
   - 处理器跳转到 HardFault 异常向量表指定的处理程序。

3. **执行 HardFault 处理程序**：
   - 在 HardFault 处理程序中，开发者可以捕获异常信息并进行分析。

4. **系统恢复或复位**：
   - 根据问题的严重性，系统可以选择复位或尝试恢复。

---

## 1.4 HardFault 的调试方法
1. **捕获异常信息**：
   - 使用调试工具（如 J-Link、ST-Link）捕获 HardFault 发生时的寄存器状态。
   - 重点关注以下寄存器：
     - **PC**（程序计数器）：指示触发异常的指令地址。
     - **LR**（链接寄存器）：指示异常发生时的返回地址。
     - **SP**（堆栈指针）：检查堆栈是否溢出或损坏。
     - **CFSR**（配置故障状态寄存器）：提供具体的故障原因（如内存管理故障、总线故障等）。

2. **回溯调用栈**：
   - 通过堆栈内容回溯函数调用链，定位问题代码。

3. **检查内存和堆栈**：
   - 确认堆栈是否溢出。
   - 检查指针是否指向有效内存。

4. **使用调试工具**：
   - 使用调试工具的断点、单步执行、内存监视等功能，逐步排查问题。

---

## 1.5 如何避免 HardFault
1. **初始化指针和变量**：
   - 确保所有指针在使用前被正确初始化。

2. **检查内存访问**：
   - 避免访问未映射或受保护的内存区域。

3. **优化堆栈使用**：
   - 合理分配堆栈大小，避免堆栈溢出。

4. **启用内存保护单元（MPU）**：
   - 使用 MPU 限制对关键内存区域的访问。

5. **增加错误检查**：
   - 在关键代码段中添加断言（assert）和错误处理逻辑。

6. **定期代码审查**：
   - 通过代码审查和静态分析工具，发现潜在的内存和指针问题。

---

## 1.6 总结
HardFault 是 ARM Cortex-M 处理器中用于处理严重错误的机制，通常由非法内存访问、堆栈溢出或未定义指令等原因触发。通过捕获异常信息、回溯调用栈和检查内存，可以快速定位问题。为了避免 HardFault，开发者应注重代码的健壮性，合理管理内存和堆栈，并充分利用调试工具进行分析。

---
---
# 2. HardFault 实际案例分析步骤
## 2.1 发现产生的HardFault
>**step 1. 这个其实很简单，在hardfault中断里边，加串口打印就OK了！
>**注意ISR内，只保留while死循环！！！**
![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/e6563c36040d4ff3bfba79359ee15b19.png)
---
>**step 2. 看串口助手界面，发现一直打印hardfault，说明程序崩溃了，进hardfault了**
>![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/92391006cf8d489485df01903ee02af6.png)
## 2.2 替换HardFault_Handler ISR
>**把你的hardfault中断程序替换成我写的这个,注意编译器版本**

+ ARMCC编译器版本的（也就是keil工程的）
```c
// Return the R13(Stack Pointer)
__ASM unsigned int __Get_SP_Value(void)
{
    mov r0, r13 //R13(SP)
    bx lr
}

void (*f1)(void);
unsigned int result;

void HardFault_Handler(void)
{
    //实际操作Arm Coretex-M0核心的是+0x1C，M3核的是+0X14
    result = __Get_SP_Value()+0x14;
    f1 = (void (*)(void))(*(unsigned int *)result);
    f1();
    
  	while (1)
	{
		pr_info("[HardFault_Handler]:=!! \r\n");
	}
}
```

+ GCC_ARM编译器版本的
```c
/*----------------hardfault 调查原因方案，需要debug 单步执行--------------------*/

__attribute__((always_inline)) static inline unsigned int __get_SP(void)
{
    register unsigned int result;

    __asm volatile ("MOV %0, sp\n" : "=r" (result));
    return result;
}

void(*f1)(void);
unsigned int result;

void HardFault_Handler(void)
{
	//实际操作Arm Coretex-M0核心的是+0x1C，M3核的是+0X14
    result= __get_SP()+0x1C;
    f1=(void (*)(void))(*(unsigned int *)result);
    f1();

    while (1)
    {
        pr_info("[HardFault_Handler]:=!! \r\n");
    }

}
/*----------------hardfault 调查原因方案，需要debug 单步执行--------------------*/

```
## 2.3 实战操作
>**步骤2.1，2.2 做好了之后，就进行debug，打断点，单步调试，看产生hardfault崩溃的位置在哪？**

>**例如我用的IDE为Eclipse, 产生hardfault了就停在了这，然后单步执行，就能找到了崩溃的位置**
>
![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/29f8ca8d488148d19aed1c27914a8c5f.gif)
## 2.4 原理分析
>下图AMR Coretex-M3核心，产生中断时候的压栈情况，根据中断压栈情况，进行栈回溯，就可以发现栈中有LR寄存器的值，然后就可以读出来栈中LR的值，跳转过去

>Tips：ARM M3核心 和M4核心压榨情况并不一样的，M4有带有浮点数寄存器的
![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/60ecd49d91474b1c847a850841c82497.png)

>下图为M4内核的栈帧，异常压栈情况
![图2](https://i-blog.csdnimg.cn/direct/96f69faaaa68468ca70d5176845f5b10.png)

----
---
## 3. HardFault 其他定位分析方法（手动）
>上述的属于半自动的定位方法，其实有时候也不管用的，比如arm系统崩溃了，会引起其他错误
>目前所遇到情况感觉纯手动对比反汇编文件是最管用的
>简单来说就是根据中断流程的压栈原理，来反推 栈中LR的值，取出来在手动去反汇编文件里对比去
![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/6be7002f2fc64ce4b3cdf248f28634d2.png)
![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/98b60b222aeb4f96b8c989bfccb84459.png)



## 4. 总结
此类属于访问非法程序地址，给PC赋值为0，属于最简单最好找的hardfault问题了，有些是外设配置出问题，导致系统奔溃了，会更麻烦！ 得看hardfault状态寄存器了