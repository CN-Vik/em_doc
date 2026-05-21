# 汇编代码

```c
;******************** (C) COPYRIGHT 2011 STMicroelectronics ********************
; 文件名: startup_stm32f10x_md.s
; 作用  : STM32F10x 中等容量（MD）器件启动文件（Keil MDK-ARM）
; 功能  :
;   1. 定义初始堆栈（SP）
;   2. 定义中断向量表（Vector Table）
;   3. 提供默认的中断/异常处理函数（弱定义）
;   4. 复位后调用 SystemInit() 完成系统时钟初始化
;   5. 跳转到 C 运行库 __main，最终进入 main()
;*******************************************************************************

;------------------------------------------------------------------------------
; 栈大小配置（单位：字节）
;------------------------------------------------------------------------------
Stack_Size      EQU     0x00000800     ; 栈大小 = 2KB，可根据应用调整

                AREA    STACK, NOINIT, READWRITE, ALIGN=3
Stack_Mem       SPACE   Stack_Size     ; 在 RAM 中为栈分配空间
__initial_sp                        ; 栈顶地址（MSP 初始值）


;------------------------------------------------------------------------------
; 堆大小配置（单位：字节）
;------------------------------------------------------------------------------
Heap_Size       EQU     0x00000400     ; 堆大小 = 1KB

                AREA    HEAP, NOINIT, READWRITE, ALIGN=3
__heap_base                         ; 堆起始地址
Heap_Mem        SPACE   Heap_Size      ; 在 RAM 中为堆分配空间
__heap_limit                        ; 堆结束地址

                PRESERVE8              ; 保证 8 字节对齐（ARM ABI 要求）
                THUMB                  ; 使用 Thumb 指令集


;------------------------------------------------------------------------------
; 中断向量表（复位后映射到地址 0x00000000）
;------------------------------------------------------------------------------
                AREA    RESET, DATA, READONLY
                EXPORT  __Vectors
                EXPORT  __Vectors_End
                EXPORT  __Vectors_Size

__Vectors
                DCD     __initial_sp           ; 0x00: 初始主栈指针 MSP
                DCD     Reset_Handler          ; 0x04: 复位中断入口
                DCD     NMI_Handler            ; 0x08: NMI
                DCD     HardFault_Handler      ; 0x0C: 硬件错误
                DCD     MemManage_Handler      ; 0x10: MPU 错误
                DCD     BusFault_Handler       ; 0x14: 总线错误
                DCD     UsageFault_Handler     ; 0x18: 用法错误
                DCD     0                      ; 保留
                DCD     0                      ; 保留
                DCD     0                      ; 保留
                DCD     0                      ; 保留
                DCD     SVC_Handler            ; SVCall（系统调用）
                DCD     DebugMon_Handler       ; 调试监控
                DCD     0                      ; 保留
                DCD     PendSV_Handler         ; PendSV（RTOS 上下文切换）
                DCD     SysTick_Handler        ; SysTick 定时器

                ;------------------ 外部中断向量 ------------------
                DCD     WWDG_IRQHandler
                DCD     PVD_IRQHandler
                DCD     TAMPER_IRQHandler
                DCD     RTC_IRQHandler
                DCD     FLASH_IRQHandler
                DCD     RCC_IRQHandler
                DCD     EXTI0_IRQHandler
                DCD     EXTI1_IRQHandler
                DCD     EXTI2_IRQHandler
                DCD     EXTI3_IRQHandler
                DCD     EXTI4_IRQHandler
                DCD     DMA1_Channel1_IRQHandler
                DCD     DMA1_Channel2_IRQHandler
                DCD     DMA1_Channel3_IRQHandler
                DCD     DMA1_Channel4_IRQHandler
                DCD     DMA1_Channel5_IRQHandler
                DCD     DMA1_Channel6_IRQHandler
                DCD     DMA1_Channel7_IRQHandler
                DCD     ADC1_2_IRQHandler
                DCD     USB_HP_CAN1_TX_IRQHandler
                DCD     USB_LP_CAN1_RX0_IRQHandler
                DCD     CAN1_RX1_IRQHandler
                DCD     CAN1_SCE_IRQHandler
                DCD     EXTI9_5_IRQHandler
                DCD     TIM1_BRK_IRQHandler
                DCD     TIM1_UP_IRQHandler
                DCD     TIM1_TRG_COM_IRQHandler
                DCD     TIM1_CC_IRQHandler
                DCD     TIM2_IRQHandler
                DCD     TIM3_IRQHandler
                DCD     TIM4_IRQHandler
                DCD     I2C1_EV_IRQHandler
                DCD     I2C1_ER_IRQHandler
                DCD     I2C2_EV_IRQHandler
                DCD     I2C2_ER_IRQHandler
                DCD     SPI1_IRQHandler
                DCD     SPI2_IRQHandler
                DCD     USART1_IRQHandler
                DCD     USART2_IRQHandler
                DCD     USART3_IRQHandler
                DCD     EXTI15_10_IRQHandler
                DCD     RTCAlarm_IRQHandler
                DCD     USBWakeUp_IRQHandler

__Vectors_End

__Vectors_Size  EQU  __Vectors_End - __Vectors   ; 向量表大小


;------------------------------------------------------------------------------
; 代码段
;------------------------------------------------------------------------------
                AREA    |.text|, CODE, READONLY

;------------------------------------------------------------------------------
; 复位中断处理函数
;------------------------------------------------------------------------------
Reset_Handler   PROC
                EXPORT  Reset_Handler [WEAK]
                IMPORT  __main
                IMPORT  SystemInit

                ; 调用系统初始化（时钟、PLL、Flash 等）
                LDR     R0, =SystemInit
                BLX     R0

                ; 跳转到 C 运行库入口 __main
                LDR     R0, =__main
                BX      R0
                ENDP


;------------------------------------------------------------------------------
; Cortex-M 内核异常默认处理函数（死循环）
;------------------------------------------------------------------------------
NMI_Handler     PROC
                EXPORT  NMI_Handler [WEAK]
                B       .
                ENDP

HardFault_Handler PROC
                EXPORT  HardFault_Handler [WEAK]
                B       .
                ENDP

MemManage_Handler PROC
                EXPORT  MemManage_Handler [WEAK]
                B       .
                ENDP

BusFault_Handler PROC
                EXPORT  BusFault_Handler [WEAK]
                B       .
                ENDP

UsageFault_Handler PROC
                EXPORT  UsageFault_Handler [WEAK]
                B       .
                ENDP

SVC_Handler     PROC
                EXPORT  SVC_Handler [WEAK]
                B       .
                ENDP

DebugMon_Handler PROC
                EXPORT  DebugMon_Handler [WEAK]
                B       .
                ENDP

PendSV_Handler  PROC
                EXPORT  PendSV_Handler [WEAK]
                B       .
                ENDP

SysTick_Handler PROC
                EXPORT  SysTick_Handler [WEAK]
                B       .
                ENDP


;------------------------------------------------------------------------------
; 外设中断默认处理函数（统一跳转到死循环）
;------------------------------------------------------------------------------
Default_Handler PROC

                EXPORT  WWDG_IRQHandler [WEAK]
                EXPORT  PVD_IRQHandler [WEAK]
                EXPORT  TAMPER_IRQHandler [WEAK]
                EXPORT  RTC_IRQHandler [WEAK]
                EXPORT  FLASH_IRQHandler [WEAK]
                EXPORT  RCC_IRQHandler [WEAK]
                EXPORT  EXTI0_IRQHandler [WEAK]
                EXPORT  EXTI1_IRQHandler [WEAK]
                EXPORT  EXTI2_IRQHandler [WEAK]
                EXPORT  EXTI3_IRQHandler [WEAK]
                EXPORT  EXTI4_IRQHandler [WEAK]
                EXPORT  DMA1_Channel1_IRQHandler [WEAK]
                EXPORT  DMA1_Channel2_IRQHandler [WEAK]
                EXPORT  DMA1_Channel3_IRQHandler [WEAK]
                EXPORT  DMA1_Channel4_IRQHandler [WEAK]
                EXPORT  DMA1_Channel5_IRQHandler [WEAK]
                EXPORT  DMA1_Channel6_IRQHandler [WEAK]
                EXPORT  DMA1_Channel7_IRQHandler [WEAK]
                EXPORT  ADC1_2_IRQHandler [WEAK]
                EXPORT  USB_HP_CAN1_TX_IRQHandler [WEAK]
                EXPORT  USB_LP_CAN1_RX0_IRQHandler [WEAK]
                EXPORT  CAN1_RX1_IRQHandler [WEAK]
                EXPORT  CAN1_SCE_IRQHandler [WEAK]
                EXPORT  EXTI9_5_IRQHandler [WEAK]
                EXPORT  TIM1_BRK_IRQHandler [WEAK]
                EXPORT  TIM1_UP_IRQHandler [WEAK]
                EXPORT  TIM1_TRG_COM_IRQHandler [WEAK]
                EXPORT  TIM1_CC_IRQHandler [WEAK]
                EXPORT  TIM2_IRQHandler [WEAK]
                EXPORT  TIM3_IRQHandler [WEAK]
                EXPORT  TIM4_IRQHandler [WEAK]
                EXPORT  I2C1_EV_IRQHandler [WEAK]
                EXPORT  I2C1_ER_IRQHandler [WEAK]
                EXPORT  I2C2_EV_IRQHandler [WEAK]
                EXPORT  I2C2_ER_IRQHandler [WEAK]
                EXPORT  SPI1_IRQHandler [WEAK]
                EXPORT  SPI2_IRQHandler [WEAK]
                EXPORT  USART1_IRQHandler [WEAK]
                EXPORT  USART2_IRQHandler [WEAK]
                EXPORT  USART3_IRQHandler [WEAK]
                EXPORT  EXTI15_10_IRQHandler [WEAK]
                EXPORT  RTCAlarm_IRQHandler [WEAK]
                EXPORT  USBWakeUp_IRQHandler [WEAK]

WWDG_IRQHandler
PVD_IRQHandler
TAMPER_IRQHandler
RTC_IRQHandler
FLASH_IRQHandler
RCC_IRQHandler
EXTI0_IRQHandler
EXTI1_IRQHandler
EXTI2_IRQHandler
EXTI3_IRQHandler
EXTI4_IRQHandler
DMA1_Channel1_IRQHandler
DMA1_Channel2_IRQHandler
DMA1_Channel3_IRQHandler
DMA1_Channel4_IRQHandler
DMA1_Channel5_IRQHandler
DMA1_Channel6_IRQHandler
DMA1_Channel7_IRQHandler
ADC1_2_IRQHandler
USB_HP_CAN1_TX_IRQHandler
USB_LP_CAN1_RX0_IRQHandler
CAN1_RX1_IRQHandler
CAN1_SCE_IRQHandler
EXTI9_5_IRQHandler
TIM1_BRK_IRQHandler
TIM1_UP_IRQHandler
TIM1_TRG_COM_IRQHandler
TIM1_CC_IRQHandler
TIM2_IRQHandler
TIM3_IRQHandler
TIM4_IRQHandler
I2C1_EV_IRQHandler
I2C1_ER_IRQHandler
I2C2_EV_IRQHandler
I2C2_ER_IRQHandler
SPI1_IRQHandler
SPI2_IRQHandler
USART1_IRQHandler
USART2_IRQHandler
USART3_IRQHandler
EXTI15_10_IRQHandler
RTCAlarm_IRQHandler
USBWakeUp_IRQHandler

                B       .           ; 所有未实现中断进入死循环
                ENDP

                ALIGN


;------------------------------------------------------------------------------
; 用户栈和堆初始化（供 C 运行库使用）
;------------------------------------------------------------------------------
                IF :DEF:__MICROLIB

                EXPORT  __initial_sp
                EXPORT  __heap_base
                EXPORT  __heap_limit

                ELSE

                IMPORT  __use_two_region_memory
                EXPORT  __user_initial_stackheap

__user_initial_stackheap
                ; R0 = 堆起始地址
                ; R1 = 栈结束地址
                ; R2 = 堆结束地址
                ; R3 = 栈起始地址
                LDR     R0, =Heap_Mem
                LDR     R1, =(Stack_Mem + Stack_Size)
                LDR     R2, =(Heap_Mem + Heap_Size)
                LDR     R3, =Stack_Mem
                BX      LR

                ALIGN
                ENDIF

                END

```
------
-----
----
这是 **STM32F10x（Cortex-M3）在 Keil MDK-ARM 下的启动文件（startup）**，它的作用可以一句话概括：

> **在 CPU 上电 / 复位后，用最少的汇编代码把系统“拉起来”，再把控制权交给 C 运行库，最终进入 `main()`。**

下面我 **按模块 + 按时间顺序** 给你系统性解读一遍，偏“嵌入式软件工程师视角”。

---

# 一、这个文件在系统里的“地位”

在 STM32 工程中：

```
上电 / 复位
   ↓
startup_stm32f10x_md.s   ←【你贴的这个】
   ↓
SystemInit()             ← system_stm32f10x.c
   ↓
C 运行库 (__main)
   ↓
main()
```

👉 **这是 MCU 启动链路的第 1 个软件节点**

---

# 二、整体工作流程（先给你一张“脑图”）

**完整启动流程：**

1. **CPU 复位**
2. 从 **Flash 0x08000000** 取：

   * MSP（主堆栈指针）
   * Reset_Handler 地址
3. 跳转执行 `Reset_Handler`
4. `Reset_Handler`：

   * 调用 `SystemInit()`（时钟 / Flash / PLL）
   * 跳转到 `__main`
5. `__main`：

   * 初始化 `.data / .bss`
   * 初始化堆 / 栈
   * 调用 `main()`
6. 进入用户程序

---

# 三、Stack / Heap 定义（内存布局基础）

## 1️⃣ 栈（Stack）

```asm
Stack_Size      EQU     0x00000800  ; 2KB

AREA    STACK, NOINIT, READWRITE, ALIGN=3
Stack_Mem       SPACE   Stack_Size
__initial_sp
```

### 关键点

* **栈向下生长**
* `__initial_sp` = 栈顶地址
* 上电后 **MSP = __initial_sp**
* 中断 / 异常 / main 都用它（除非你切 PSP）

👉 这是 **中断安全 + RTOS 能跑** 的基础

---

## 2️⃣ 堆（Heap）

```asm
Heap_Size       EQU     0x00000400  ; 1KB

AREA    HEAP, NOINIT, READWRITE, ALIGN=3
__heap_base
Heap_Mem        SPACE   Heap_Size
__heap_limit
```

### 作用

* 给 `malloc / free / new` 用
* 不用 C 动态内存时可以设为 0

---

# 四、向量表（Vector Table）——**最核心的部分**

```asm
AREA    RESET, DATA, READONLY
EXPORT  __Vectors

__Vectors
    DCD __initial_sp
    DCD Reset_Handler
    DCD NMI_Handler
    DCD HardFault_Handler
    ...
```

## 1️⃣ 向量表本质

| 表项    | 含义            |
| ----- | ------------- |
| 第 0 项 | MSP 初始值       |
| 第 1 项 | Reset_Handler |
| 后面    | 各种异常 / 中断 ISR |

📌 **Cortex-M 硬件强制规定的格式**

---

## 2️⃣ 外设中断向量

```asm
DCD USART1_IRQHandler
DCD USART2_IRQHandler
DCD USART3_IRQHandler
```

这些名字 **必须和 ST 标准库 / HAL / LL 对应**
否则 NVIC 触发了也找不到函数

---

## 3️⃣ `__Vectors_Size`

```asm
__Vectors_Size  EQU  __Vectors_End - __Vectors
```

供 **调试器 / 链接器** 使用，没业务逻辑

---

# 五、Reset_Handler —— 启动核心

```asm
Reset_Handler PROC
    IMPORT  __main
    IMPORT  SystemInit

    LDR R0, =SystemInit
    BLX R0

    LDR R0, =__main
    BX  R0
ENDP
```

## 1️⃣ 这段代码“刻意很短”

原因：

* **此时堆栈刚建立**
* 不适合复杂逻辑
* 所有初始化交给 C 层

---

## 2️⃣ SystemInit() 干啥？

在 `system_stm32f10x.c`：

* 时钟源切换（HSI / HSE）
* PLL 配置
* Flash 等待周期
* 向量表偏移（SCB->VTOR）

👉 **“裸机能不能跑、跑多快”全在这**

---

## 3️⃣ `__main` 是谁？

不是你写的，是 **ARM C Runtime**

负责：

* `.data` 拷贝（Flash → SRAM）
* `.bss` 清零
* 堆 / 栈初始化
* 调用 `main()`

---

# 六、异常 / 中断处理机制

## 1️⃣ WEAK 定义的意义

```asm
EXPORT  USART1_IRQHandler [WEAK]
```

### 含义：

* **你在 C 文件里写同名函数，会自动覆盖**
* 否则走默认死循环

👉 这就是 **“中断函数不用手动注册”** 的原理

---

## 2️⃣ 默认中断行为

```asm
USART1_IRQHandler
USART2_IRQHandler
...
    B .
```

* 无限死循环
* 防止“中断乱飞”
* 调试时一进来就卡住，方便定位

---

## 3️⃣ HardFault / UsageFault 等

```asm
HardFault_Handler
    B .
```

你以后如果做 **HardFault 调试**，
就是在这里改成 **C 函数 + 寄存器打印**

---

# 七、Microlib / 非 Microlib 的区别

## 1️⃣ Microlib（精简库）

```asm
IF :DEF:__MICROLIB
EXPORT __initial_sp
EXPORT __heap_base
EXPORT __heap_limit
```

* Keil 精简运行库
* 省空间
* 启动更简单

---

## 2️⃣ 非 Microlib（完整库）

```asm
__user_initial_stackheap
    LDR R0, =Heap_Mem
    LDR R1, =(Stack_Mem + Stack_Size)
    LDR R2, =(Heap_Mem + Heap_Size)
    LDR R3, =Stack_Mem
    BX  LR
```

👉 这是 **告诉 C 运行库：**

* 堆从哪到哪
* 栈从哪到哪

---

# 八、从“工程实践”角度的总结

## 这个 startup 文件做了什么？

✔ 定义 **栈 / 堆**
✔ 建立 **中断向量表**
✔ 提供 **默认中断实现（弱符号）**
✔ 调用 `SystemInit()`
✔ 跳转到 `__main → main()`

---

## 你可以改 / 常改的地方

| 位置                | 场景                  |
| ----------------- | ------------------- |
| Stack_Size        | RTOS / 中断多          |
| Heap_Size         | malloc / LVGL       |
| HardFault_Handler | 崩溃定位                |
| 向量表               | Bootloader / APP 跳转 |

---

## 一句话终极总结

> **startup 文件 = MCU 从“硅片”进入“C 世界”的桥梁**

如果你愿意，下一步我可以帮你：

* ✅ 画一张 **“复位到 main 的时序图”**
* ✅ 结合 **RT-Thread / FreeRTOS** 讲 PendSV / SVC
* ✅ 拆解 **Bootloader → APP 跳转时这个文件如何配合**

你想往哪个方向继续？
