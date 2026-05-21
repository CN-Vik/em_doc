**一句话：**

STM32 单 Bootloader + 双 App 区域时，做**中断向量表偏移**，是为了让 CPU 在 App 运行后，发生中断/异常时去 App 自己的中断函数，而不是继续去 Bootloader 的中断函数。

不做的话，App 可能能跳起来，但一开中断就容易跑飞、进 HardFault、进 Bootloader 的 `Default_Handler`、FreeRTOS 不调度、串口/定时器/DMA 中断全异常。

---

假设 Flash 布局是这样：

```text
0x08000000  ┌────────────────────┐
            │ Bootloader         │
            │ Bootloader向量表   │
0x08010000  ├────────────────────┤
            │ App1               │
            │ App1向量表         │
0x08050000  ├────────────────────┤
            │ App2               │
            │ App2向量表         │
            └────────────────────┘
```

STM32 上电后，默认会从 `0x08000000` 取中断向量表。

也就是说，默认情况下 CPU 认为：

```c
中断向量表地址 = 0x08000000
```

这正好是 Bootloader 的地址。

所以 Bootloader 能正常启动。

---

## 1. 中断向量表到底是什么？

每个 STM32 程序最开头都有一张表，叫**中断向量表**。

比如 App 的起始地址是 `0x08010000`，那么 App 的前面一般是：

```text
0x08010000: 初始 MSP 栈顶地址
0x08010004: Reset_Handler 地址
0x08010008: NMI_Handler 地址
0x0801000C: HardFault_Handler 地址
0x08010010: MemManage_Handler 地址
...
0x0801003C: SysTick_Handler 地址
...
后面还有 USART、TIM、DMA、EXTI 等外设中断入口
```

CPU 发生中断时，不是靠 C 函数名去找 ISR，而是硬件直接查这张表。

比如发生 SysTick 中断时，CPU 会去当前向量表里找：

```text
SysTick_Handler 的函数地址
```

发生 USART 中断时，CPU 会去当前向量表里找：

```text
USARTx_IRQHandler 的函数地址
```

所以关键点是：

**当前 CPU 认为向量表在哪里，它就从哪里取中断函数入口。**

---

## 2. Bootloader 跳 App 时，为什么要改 VTOR？

Cortex-M 里面有个寄存器叫：

```c
SCB->VTOR
```

它的作用就是告诉 CPU：

```text
当前中断向量表从哪个地址开始
```

Bootloader 默认运行时：

```c
SCB->VTOR = 0x08000000;
```

如果 Bootloader 要跳到 App1：

```c
SCB->VTOR = 0x08010000;
```

如果 Bootloader 要跳到 App2：

```c
SCB->VTOR = 0x08050000;
```

这就叫**中断向量表偏移**。

注意，“偏移”不是把代码搬走，而是告诉 CPU：

```text
你以后找中断入口，不要去 Bootloader 那张表了，要去 App 自己那张表。
```

---

## 3. 不做中断向量表偏移会怎么样？

假设 Bootloader 跳到了 App1，但是你没有执行：

```c
SCB->VTOR = APP1_ADDR;
```

那么 CPU 还会认为：

```c
当前向量表 = 0x08000000
```

也就是 Bootloader 的向量表。

这时候 App1 运行后，一旦发生中断，问题就来了。

比如 App1 初始化了定时器：

```c
TIM3_IRQHandler()
{
    // App自己的定时器处理
}
```

但是 TIM3 中断来了以后，CPU 不会去 App1 的 `TIM3_IRQHandler`。

它会去 Bootloader 的向量表里找 TIM3 的入口。

可能结果是：

```text
情况1：Bootloader 没有 TIM3_IRQHandler
      -> 进 Default_Handler 死循环

情况2：Bootloader 有 TIM3_IRQHandler
      -> 执行 Bootloader 的中断逻辑，和 App 外设状态完全不匹配

情况3：向量表里地址无效
      -> HardFault

情况4：FreeRTOS 的 SysTick/PendSV/SVC 没跑到 App 的处理函数
      -> RTOS 任务调度异常，系统卡死
```

所以你会看到一些很怪的现象：

```text
App main() 能进
外设初始化也能跑
但是一开中断就死
HAL_Delay 卡住
FreeRTOS 不调度
串口中断没反应
DMA 中断跑飞
定时器中断进 Default_Handler
莫名其妙 HardFault
```

这就是典型的**向量表没切到 App**导致的问题。

---

## 4. 为什么 main 能跑，但中断不行？

因为 Bootloader 跳 App 的时候，通常是手动跳到 App 的复位入口：

```c
uint32_t app_stack = *(uint32_t *)APP_ADDR;
uint32_t app_reset = *(uint32_t *)(APP_ADDR + 4);

__set_MSP(app_stack);

void (*app_entry)(void) = (void (*)(void))app_reset;
app_entry();
```

这段代码直接从 App 向量表里取了：

```text
APP_ADDR + 0：App 的 MSP
APP_ADDR + 4：App 的 Reset_Handler
```

所以 App 的 `Reset_Handler`、`SystemInit()`、`main()` 可以正常进入。

但是后面的中断不是 Bootloader 手动跳的，而是 CPU 硬件自动查 `SCB->VTOR`。

所以会出现：

```text
main() 能进
但是中断一来就炸
```

原因就是：

```text
Reset_Handler 是你手动跳对的；
中断入口是 CPU 根据 VTOR 自动找的。
```

---

## 5. 正确跳转 App 的典型流程

Bootloader 跳 App 前一般要这样做：

```c
#define APP1_ADDR  0x08010000U

typedef void (*pFunction)(void);

void jump_to_app(uint32_t app_addr)
{
    uint32_t app_stack;
    uint32_t app_reset;
    pFunction app_entry;

    // 1. 关闭全局中断，防止跳转过程中来中断
    __disable_irq();

    // 2. 关闭 SysTick，避免 Bootloader 的 SysTick 干扰 App
    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL  = 0;

    // 3. 可选：反初始化 Bootloader 用过的外设
    // HAL_RCC_DeInit();
    // HAL_DeInit();

    // 4. 设置 App 的中断向量表地址
    SCB->VTOR = app_addr;

    // 5. 取 App 栈顶地址
    app_stack = *(uint32_t *)app_addr;

    // 6. 取 App Reset_Handler 地址
    app_reset = *(uint32_t *)(app_addr + 4);

    // 7. 设置主栈指针 MSP
    __set_MSP(app_stack);

    // 8. 跳转到 App 的 Reset_Handler
    app_entry = (pFunction)app_reset;
    app_entry();
}
```

如果是 App2：

```c
jump_to_app(0x08050000);
```

核心就是这句：

```c
SCB->VTOR = app_addr;
```

---

## 6. 双 App 区域时更要注意

单 App 时，Bootloader 只跳一个 App，比如：

```text
Bootloader -> App1
```

但双 App 时可能是：

```text
Bootloader -> App1
Bootloader -> App2
```

所以 Bootloader 要根据当前有效 App 设置不同的向量表：

```c
if (active_app == APP1)
{
    SCB->VTOR = APP1_ADDR;
    jump_to_app(APP1_ADDR);
}
else if (active_app == APP2)
{
    SCB->VTOR = APP2_ADDR;
    jump_to_app(APP2_ADDR);
}
```

否则有一种更危险的情况：

```text
CPU 正在运行 App2
但是 VTOR 还指向 App1
```

结果就是：

```text
App2 的代码在跑，
但是所有中断跑到 App1 的中断函数里。
```

这会非常混乱。

比如 App2 初始化的是 USART1，App1 的中断函数里处理的是旧版本协议，那就可能直接乱套。

---

## 7. App 工程本身也要配对地址

只在 Bootloader 里设置 `SCB->VTOR` 还不一定够。

因为很多 STM32 工程在 `system_stm32xxx.c` 里面有类似代码：

```c
#define VECT_TAB_OFFSET  0x00000000U

SCB->VTOR = FLASH_BASE | VECT_TAB_OFFSET;
```

默认情况下：

```c
FLASH_BASE = 0x08000000
VECT_TAB_OFFSET = 0x00000000
```

所以它会设置成：

```c
SCB->VTOR = 0x08000000;
```

这对 Bootloader 是对的，但是对 App 是错的。

如果 App1 放在 `0x08010000`，那 App1 里面应该是：

```c
#define VECT_TAB_OFFSET  0x00010000U
```

最终：

```c
SCB->VTOR = 0x08010000;
```

如果 App2 放在 `0x08050000`，那就是：

```c
#define VECT_TAB_OFFSET  0x00050000U
```

最终：

```c
SCB->VTOR = 0x08050000;
```

所以有两个地方要一致：

```text
1. App 的链接地址
2. App 的中断向量表地址
```

例如 App1：

```text
链接地址：0x08010000
VTOR地址：0x08010000
```

App2：

```text
链接地址：0x08050000
VTOR地址：0x08050000
```

---

## 8. 链接地址和向量表偏移是两回事

很多人容易混。

### 链接地址

链接地址决定代码里面的函数、变量、常量最终认为自己在哪里。

比如 App1 链接地址是：

```text
0x08010000
```

那么 App1 的 `Reset_Handler`、`main()`、各种函数地址都会按 `0x08010000` 往后排。

### 向量表偏移

向量表偏移是告诉 CPU：

```text
中断入口表在哪里
```

也就是：

```c
SCB->VTOR = 0x08010000;
```

所以：

```text
链接地址不对：程序本身可能跳不起来，函数地址错。
VTOR 不对：程序可能能起来，但中断一来就错。
```

---

## 9. 升级过程中为什么尤其重要？

App 升级一般流程是：

```text
当前运行 App1
下载新固件到 App2
校验 App2
设置 App2 有效标志
重启
Bootloader 启动
Bootloader 检查 App2 有效
跳转 App2
```

这时 Bootloader 必须做：

```c
SCB->VTOR = APP2_ADDR;
```

否则虽然跳到了 App2，但是中断还可能找 Bootloader 或 App1。

还有一种情况更危险：

```text
App1 正在升级 App2
App2 区域正在被擦除/写入
如果 VTOR 错误地指向 App2
这时候来了中断
CPU 从正在擦写的 Flash 区域取中断入口
```

可能取到：

```text
0xFFFFFFFF
```

然后直接 HardFault 或跑飞。

所以升级/擦写 Flash 时通常还要注意：

```text
1. 不要执行正在擦写区域的代码
2. Flash 擦写期间谨慎处理中断
3. Bootloader 跳 App 前重新设置 VTOR
4. App 启动后也最好设置自己的 VTOR
```

---

## 10. 总结成一句工程经验

你的结构是：

```text
Bootloader + App1 + App2
```

那么每次 Bootloader 决定运行哪个 App，就必须让 CPU 的中断向量表指向哪个 App：

```c
运行 Bootloader：SCB->VTOR = 0x08000000

运行 App1：      SCB->VTOR = 0x08010000

运行 App2：      SCB->VTOR = 0x08050000
```

否则就是：

```text
代码跑在 App，
中断却跑到 Bootloader 或另一个 App。
```

这就是为什么必须做中断向量表偏移。

-----
-----
----

**一句话：**

STM32 单 Bootloader + 双 App 区域时，做**中断向量表偏移**，是为了让 CPU 在 App 运行后，发生中断/异常时去 App 自己的中断函数，而不是继续去 Bootloader 的中断函数。

不做的话，App 可能能跳起来，但一开中断就容易跑飞、进 HardFault、进 Bootloader 的 `Default_Handler`、FreeRTOS 不调度、串口/定时器/DMA 中断全异常。

---

假设 Flash 布局是这样：

```text
0x08000000  ┌────────────────────┐
            │ Bootloader         │
            │ Bootloader向量表   │
0x08010000  ├────────────────────┤
            │ App1               │
            │ App1向量表         │
0x08050000  ├────────────────────┤
            │ App2               │
            │ App2向量表         │
            └────────────────────┘
```

STM32 上电后，默认会从 `0x08000000` 取中断向量表。

也就是说，默认情况下 CPU 认为：

```c
中断向量表地址 = 0x08000000
```

这正好是 Bootloader 的地址。

所以 Bootloader 能正常启动。

---

## 1. 中断向量表到底是什么？

每个 STM32 程序最开头都有一张表，叫**中断向量表**。

比如 App 的起始地址是 `0x08010000`，那么 App 的前面一般是：

```text
0x08010000: 初始 MSP 栈顶地址
0x08010004: Reset_Handler 地址
0x08010008: NMI_Handler 地址
0x0801000C: HardFault_Handler 地址
0x08010010: MemManage_Handler 地址
...
0x0801003C: SysTick_Handler 地址
...
后面还有 USART、TIM、DMA、EXTI 等外设中断入口
```

CPU 发生中断时，不是靠 C 函数名去找 ISR，而是硬件直接查这张表。

比如发生 SysTick 中断时，CPU 会去当前向量表里找：

```text
SysTick_Handler 的函数地址
```

发生 USART 中断时，CPU 会去当前向量表里找：

```text
USARTx_IRQHandler 的函数地址
```

所以关键点是：

**当前 CPU 认为向量表在哪里，它就从哪里取中断函数入口。**

---

## 2. Bootloader 跳 App 时，为什么要改 VTOR？

Cortex-M 里面有个寄存器叫：

```c
SCB->VTOR
```

它的作用就是告诉 CPU：

```text
当前中断向量表从哪个地址开始
```

Bootloader 默认运行时：

```c
SCB->VTOR = 0x08000000;
```

如果 Bootloader 要跳到 App1：

```c
SCB->VTOR = 0x08010000;
```

如果 Bootloader 要跳到 App2：

```c
SCB->VTOR = 0x08050000;
```

这就叫**中断向量表偏移**。

注意，“偏移”不是把代码搬走，而是告诉 CPU：

```text
你以后找中断入口，不要去 Bootloader 那张表了，要去 App 自己那张表。
```

---

## 3. 不做中断向量表偏移会怎么样？

假设 Bootloader 跳到了 App1，但是你没有执行：

```c
SCB->VTOR = APP1_ADDR;
```

那么 CPU 还会认为：

```c
当前向量表 = 0x08000000
```

也就是 Bootloader 的向量表。

这时候 App1 运行后，一旦发生中断，问题就来了。

比如 App1 初始化了定时器：

```c
TIM3_IRQHandler()
{
    // App自己的定时器处理
}
```

但是 TIM3 中断来了以后，CPU 不会去 App1 的 `TIM3_IRQHandler`。

它会去 Bootloader 的向量表里找 TIM3 的入口。

可能结果是：

```text
情况1：Bootloader 没有 TIM3_IRQHandler
      -> 进 Default_Handler 死循环

情况2：Bootloader 有 TIM3_IRQHandler
      -> 执行 Bootloader 的中断逻辑，和 App 外设状态完全不匹配

情况3：向量表里地址无效
      -> HardFault

情况4：FreeRTOS 的 SysTick/PendSV/SVC 没跑到 App 的处理函数
      -> RTOS 任务调度异常，系统卡死
```

所以你会看到一些很怪的现象：

```text
App main() 能进
外设初始化也能跑
但是一开中断就死
HAL_Delay 卡住
FreeRTOS 不调度
串口中断没反应
DMA 中断跑飞
定时器中断进 Default_Handler
莫名其妙 HardFault
```

这就是典型的**向量表没切到 App**导致的问题。

---

## 4. 为什么 main 能跑，但中断不行？

因为 Bootloader 跳 App 的时候，通常是手动跳到 App 的复位入口：

```c
uint32_t app_stack = *(uint32_t *)APP_ADDR;
uint32_t app_reset = *(uint32_t *)(APP_ADDR + 4);

__set_MSP(app_stack);

void (*app_entry)(void) = (void (*)(void))app_reset;
app_entry();
```

这段代码直接从 App 向量表里取了：

```text
APP_ADDR + 0：App 的 MSP
APP_ADDR + 4：App 的 Reset_Handler
```

所以 App 的 `Reset_Handler`、`SystemInit()`、`main()` 可以正常进入。

但是后面的中断不是 Bootloader 手动跳的，而是 CPU 硬件自动查 `SCB->VTOR`。

所以会出现：

```text
main() 能进
但是中断一来就炸
```

原因就是：

```text
Reset_Handler 是你手动跳对的；
中断入口是 CPU 根据 VTOR 自动找的。
```

---

## 5. 正确跳转 App 的典型流程

Bootloader 跳 App 前一般要这样做：

```c
#define APP1_ADDR  0x08010000U

typedef void (*pFunction)(void);

void jump_to_app(uint32_t app_addr)
{
    uint32_t app_stack;
    uint32_t app_reset;
    pFunction app_entry;

    // 1. 关闭全局中断，防止跳转过程中来中断
    __disable_irq();

    // 2. 关闭 SysTick，避免 Bootloader 的 SysTick 干扰 App
    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL  = 0;

    // 3. 可选：反初始化 Bootloader 用过的外设
    // HAL_RCC_DeInit();
    // HAL_DeInit();

    // 4. 设置 App 的中断向量表地址
    SCB->VTOR = app_addr;

    // 5. 取 App 栈顶地址
    app_stack = *(uint32_t *)app_addr;

    // 6. 取 App Reset_Handler 地址
    app_reset = *(uint32_t *)(app_addr + 4);

    // 7. 设置主栈指针 MSP
    __set_MSP(app_stack);

    // 8. 跳转到 App 的 Reset_Handler
    app_entry = (pFunction)app_reset;
    app_entry();
}
```

如果是 App2：

```c
jump_to_app(0x08050000);
```

核心就是这句：

```c
SCB->VTOR = app_addr;
```

---

## 6. 双 App 区域时更要注意

单 App 时，Bootloader 只跳一个 App，比如：

```text
Bootloader -> App1
```

但双 App 时可能是：

```text
Bootloader -> App1
Bootloader -> App2
```

所以 Bootloader 要根据当前有效 App 设置不同的向量表：

```c
if (active_app == APP1)
{
    SCB->VTOR = APP1_ADDR;
    jump_to_app(APP1_ADDR);
}
else if (active_app == APP2)
{
    SCB->VTOR = APP2_ADDR;
    jump_to_app(APP2_ADDR);
}
```

否则有一种更危险的情况：

```text
CPU 正在运行 App2
但是 VTOR 还指向 App1
```

结果就是：

```text
App2 的代码在跑，
但是所有中断跑到 App1 的中断函数里。
```

这会非常混乱。

比如 App2 初始化的是 USART1，App1 的中断函数里处理的是旧版本协议，那就可能直接乱套。

---

## 7. App 工程本身也要配对地址

只在 Bootloader 里设置 `SCB->VTOR` 还不一定够。

因为很多 STM32 工程在 `system_stm32xxx.c` 里面有类似代码：

```c
#define VECT_TAB_OFFSET  0x00000000U

SCB->VTOR = FLASH_BASE | VECT_TAB_OFFSET;
```

默认情况下：

```c
FLASH_BASE = 0x08000000
VECT_TAB_OFFSET = 0x00000000
```

所以它会设置成：

```c
SCB->VTOR = 0x08000000;
```

这对 Bootloader 是对的，但是对 App 是错的。

如果 App1 放在 `0x08010000`，那 App1 里面应该是：

```c
#define VECT_TAB_OFFSET  0x00010000U
```

最终：

```c
SCB->VTOR = 0x08010000;
```

如果 App2 放在 `0x08050000`，那就是：

```c
#define VECT_TAB_OFFSET  0x00050000U
```

最终：

```c
SCB->VTOR = 0x08050000;
```

所以有两个地方要一致：

```text
1. App 的链接地址
2. App 的中断向量表地址
```

例如 App1：

```text
链接地址：0x08010000
VTOR地址：0x08010000
```

App2：

```text
链接地址：0x08050000
VTOR地址：0x08050000
```

---

## 8. 链接地址和向量表偏移是两回事

很多人容易混。

### 链接地址

链接地址决定代码里面的函数、变量、常量最终认为自己在哪里。

比如 App1 链接地址是：

```text
0x08010000
```

那么 App1 的 `Reset_Handler`、`main()`、各种函数地址都会按 `0x08010000` 往后排。

### 向量表偏移

向量表偏移是告诉 CPU：

```text
中断入口表在哪里
```

也就是：

```c
SCB->VTOR = 0x08010000;
```

所以：

```text
链接地址不对：程序本身可能跳不起来，函数地址错。
VTOR 不对：程序可能能起来，但中断一来就错。
```

---

## 9. 升级过程中为什么尤其重要？

App 升级一般流程是：

```text
当前运行 App1
下载新固件到 App2
校验 App2
设置 App2 有效标志
重启
Bootloader 启动
Bootloader 检查 App2 有效
跳转 App2
```

这时 Bootloader 必须做：

```c
SCB->VTOR = APP2_ADDR;
```

否则虽然跳到了 App2，但是中断还可能找 Bootloader 或 App1。

还有一种情况更危险：

```text
App1 正在升级 App2
App2 区域正在被擦除/写入
如果 VTOR 错误地指向 App2
这时候来了中断
CPU 从正在擦写的 Flash 区域取中断入口
```

可能取到：

```text
0xFFFFFFFF
```

然后直接 HardFault 或跑飞。

所以升级/擦写 Flash 时通常还要注意：

```text
1. 不要执行正在擦写区域的代码
2. Flash 擦写期间谨慎处理中断
3. Bootloader 跳 App 前重新设置 VTOR
4. App 启动后也最好设置自己的 VTOR
```

---

## 10. 总结成一句工程经验

你的结构是：

```text
Bootloader + App1 + App2
```

那么每次 Bootloader 决定运行哪个 App，就必须让 CPU 的中断向量表指向哪个 App：

```c
运行 Bootloader：SCB->VTOR = 0x08000000

运行 App1：      SCB->VTOR = 0x08010000

运行 App2：      SCB->VTOR = 0x08050000
```

否则就是：

```text
代码跑在 App，
中断却跑到 Bootloader 或另一个 App。
```

这就是为什么必须做中断向量表偏移。


---------
-----------
---------
对，你这个理解方向是对的。

**如果 App 正在跑，但 `SCB->VTOR` 还指向 Bootloader，那么 TIM2 中断来了以后，CPU 就会去 Bootloader 的向量表里找 TIM2 的 ISR 入口。**

重点是：

```c
TIM2中断入口地址 = *(uint32_t *)(SCB->VTOR + TIM2_IRQn对应的偏移)
```

如果：

```c
SCB->VTOR = 0x08000000;   // Bootloader
```

那 CPU 查的就是：

```text
Bootloader 的 TIM2 中断入口
```

如果：

```c
SCB->VTOR = 0x08010000;   // App
```

那 CPU 查的才是：

```text
App 的 TIM2 中断入口
```

---

# 1. Bootloader 里也配置了 TIM2，会发生什么？

假设 Bootloader 里面真的写了：

```c
void TIM2_IRQHandler(void)
{
    // Bootloader自己的TIM2中断处理
}
```

而 App 里面也写了：

```c
void TIM2_IRQHandler(void)
{
    // App自己的TIM2中断处理
}
```

但是你没有设置：

```c
SCB->VTOR = APP_ADDR;
```

那么 App 跑起来后，TIM2 中断来了，CPU 会执行：

```c
Bootloader 的 TIM2_IRQHandler()
```

而不是 App 的。

这时候就会出现很诡异的情况：

```text
主程序：App 的 main()
中断函数：Bootloader 的 TIM2_IRQHandler()
```

也就是：

```text
代码身体在 App，
中断灵魂还在 Bootloader。
```

---

# 2. Bootloader 没配置 TIM2，会发生什么？

这个更常见。

你可能会想：

> Bootloader 没配置 TIM2，那是不是 TIM2 中断就不会有？

不是。

因为现在 TIM2 是 App 配置的。

比如 App 里执行了：

```c
HAL_TIM_Base_Start_IT(&htim2);
```

或者底层做了这些事：

```c
TIM2->DIER |= TIM_DIER_UIE;       // 允许 TIM2 更新中断
NVIC_EnableIRQ(TIM2_IRQn);        // 允许 NVIC 响应 TIM2 中断
TIM2->CR1 |= TIM_CR1_CEN;         // 启动 TIM2
```

那么 TIM2 到时间后，仍然会产生中断。

**TIM2 中断是否产生，取决于外设 TIM2 和 NVIC 是否被 App 配置了。**

但是：

**TIM2 中断发生后跳到哪里执行，取决于 `SCB->VTOR`。**

这两个是两回事。

---

# 3. Bootloader 没写 TIM2_IRQHandler，向量表里是什么？

STM32 工程里的启动文件通常有类似这种弱定义：

```c
void TIM2_IRQHandler(void) __attribute__((weak, alias("Default_Handler")));
```

意思是：

```text
如果你自己写了 TIM2_IRQHandler，就用你写的。
如果你没写 TIM2_IRQHandler，就默认指向 Default_Handler。
```

所以 Bootloader 如果没用 TIM2，一般就没有自己实现 `TIM2_IRQHandler()`。

那么 Bootloader 的向量表里，TIM2 那一项通常就是：

```text
TIM2_IRQHandler -> Default_Handler
```

而 `Default_Handler` 通常长这样：

```c
void Default_Handler(void)
{
    while (1)
    {
    }
}
```

所以结果就是：

```text
App 配置 TIM2
TIM2 中断来了
CPU 去 Bootloader 向量表找 TIM2_IRQHandler
发现 Bootloader 没有 TIM2_IRQHandler
于是进入 Bootloader 的 Default_Handler
系统卡死
```

这就是你调试时可能看到的现象：

```text
App 明明在跑
一进 TIM2 中断
结果跑到 0x0800xxxx 的 Default_Handler 里死循环
```

---

# 4. 注意：Bootloader 没配置 TIM2，不代表 TIM2 中断不会发生

这个点很关键。

比如当前运行状态是：

```text
PC  正在 App 代码区
VTOR 还指向 Bootloader
```

然后 App 初始化了 TIM2：

```c
void app_timer_init(void)
{
    TIM2->PSC = 1000 - 1;
    TIM2->ARR = 1000 - 1;

    TIM2->DIER |= TIM_DIER_UIE;
    NVIC_EnableIRQ(TIM2_IRQn);

    TIM2->CR1 |= TIM_CR1_CEN;
}
```

这个时候 TIM2 外设真的被 App 启动了。

时间到了以后：

```text
TIM2->SR 里的 UIF 置 1
NVIC 发现 TIM2_IRQn pending
CPU 响应 TIM2 中断
```

然后 CPU 查中断入口：

```c
handler = *(uint32_t *)(SCB->VTOR + TIM2向量偏移);
```

如果 `SCB->VTOR` 还是 Bootloader：

```text
handler = Bootloader向量表里的TIM2入口
```

所以不管 Bootloader 有没有配置 TIM2，只要 App 把 TIM2 和 NVIC 打开了，中断就可能来。

问题只是：

```text
中断来了以后，CPU 去错地方执行。
```

---

# 5. 分几种情况看结果

## 情况 1：Bootloader 没写 TIM2_IRQHandler

Bootloader 启动文件里默认：

```c
TIM2_IRQHandler -> Default_Handler
```

结果：

```text
App 启动 TIM2
TIM2 中断来了
CPU 进 Bootloader 的 Default_Handler
系统卡死
```

这是最常见的。

---

## 情况 2：Bootloader 写了 TIM2_IRQHandler

比如 Bootloader 里面有：

```c
void TIM2_IRQHandler(void)
{
    TIM2->SR &= ~TIM_SR_UIF;
    bootloader_tick++;
}
```

结果：

```text
App 启动 TIM2
TIM2 中断来了
CPU 进 Bootloader 的 TIM2_IRQHandler
```

这时候可能不会马上死，但是逻辑错了。

因为它执行的是 Bootloader 的中断逻辑，不是 App 的。

可能出现：

```text
App 的定时任务不执行
App 的计数变量不增加
App 的电机控制周期不跑
Bootloader 的变量被错误修改
Bootloader 的中断清标志方式和 App 不一致
系统行为很诡异
```

---

## 情况 3：Bootloader 的 TIM2_IRQHandler 没有正确清 TIM2 中断标志

比如 Bootloader 中断函数没有清除 App 配置产生的 TIM2 更新标志：

```c
void TIM2_IRQHandler(void)
{
    // 没有清 TIM2->SR 的 UIF
}
```

那么会出现：

```text
进入 TIM2_IRQHandler
退出中断
发现 TIM2 中断标志还在
马上再次进入 TIM2_IRQHandler
再退出
又马上进入
...
```

结果就是：

```text
CPU 一直卡在 TIM2 中断里
主循环几乎不运行
看起来像死机
```

---

## 情况 4：Bootloader 的 TIM2_IRQHandler 用了 Bootloader 自己的全局变量/句柄

比如 Bootloader 用 HAL：

```c
TIM_HandleTypeDef htim2;

void TIM2_IRQHandler(void)
{
    HAL_TIM_IRQHandler(&htim2);
}
```

但是现在实际运行的是 App。

App 里面也有自己的：

```c
TIM_HandleTypeDef htim2;
```

注意，这两个 `htim2` 不是一个东西。

Bootloader 的 `htim2` 在 Bootloader 的 RAM 区域，App 的 `htim2` 在 App 的 RAM 区域。

如果 CPU 跑到了 Bootloader 的中断函数，它用的是 Bootloader 的 `htim2`。

这个 `htim2` 可能已经不是有效状态了。

可能导致：

```text
HAL 状态错误
回调不执行
访问异常
清标志异常
进入 HardFault
```

所以这类问题非常隐蔽。

---

# 6. 可以把它理解成“中断函数地址拿错了”

App 里面本来应该是：

```text
TIM2中断 -> App_TIM2_IRQHandler
```

但因为 VTOR 没改，实际变成：

```text
TIM2中断 -> Bootloader_TIM2_IRQHandler
```

或者：

```text
TIM2中断 -> Bootloader_Default_Handler
```

所以问题不在于 TIM2 是谁配置的。

问题在于：

```text
中断来了以后，CPU 去哪张表里取函数地址。
```

---

# 7. 你可以用这个流程记住

正确情况：

```text
App 配置 TIM2
    ↓
TIM2 到时间产生中断
    ↓
NVIC 通知 CPU
    ↓
CPU 查 App 向量表
    ↓
执行 App 的 TIM2_IRQHandler
```

错误情况：

```text
App 配置 TIM2
    ↓
TIM2 到时间产生中断
    ↓
NVIC 通知 CPU
    ↓
CPU 还在查 Bootloader 向量表
    ↓
执行 Bootloader 的 TIM2_IRQHandler
    ↓
如果 Bootloader 没写，就进 Default_Handler
```

---

# 8. 所以 Bootloader 跳 App 时必须做这件事

```c
SCB->VTOR = APP_ADDR;
```

例如：

```c
#define APP1_ADDR  0x08010000U

SCB->VTOR = APP1_ADDR;
```

这样 TIM2 中断来了以后，CPU 查的是：

```text
0x08010000 + TIM2向量偏移
```

也就是 App 自己的向量表。

---

# 9. 最直接的调试现象

如果你遇到这种情况：

```text
App main 能跑
一开定时器中断就死
调试器停在 Default_Handler
Default_Handler 地址在 Bootloader 区域
```

那基本可以重点怀疑：

```text
SCB->VTOR 没有切到 App
```

可以在 App 运行后看一下：

```c
SCB->VTOR
```

如果它还是：

```text
0x08000000
```

那就是错的。

App1 应该是：

```text
0x08010000
```

App2 应该是：

```text
0x08050000
```

具体看你的 Flash 分区。

---

# 10. 一句话总结

**Bootloader 没配置 TIM2，也不代表没事。**

因为：

```text
App 可以配置 TIM2，让 TIM2 中断产生。
但是 CPU 响应中断时，如果 VTOR 还指向 Bootloader，
它就会去 Bootloader 的向量表找 TIM2_IRQHandler。
```

如果 Bootloader 有 TIM2_IRQHandler：

```text
执行 Bootloader 的 TIM2 中断函数。
```

如果 Bootloader 没有 TIM2_IRQHandler：

```text
通常进入 Bootloader 的 Default_Handler，然后卡死。
```

所以中断向量表偏移不是可选项，而是 Bootloader 跳 App 的必要步骤。


----
----
----
对，你这个理解方向是对的。

**如果 App 正在跑，但 `SCB->VTOR` 还指向 Bootloader，那么 TIM2 中断来了以后，CPU 就会去 Bootloader 的向量表里找 TIM2 的 ISR 入口。**

重点是：

```c
TIM2中断入口地址 = *(uint32_t *)(SCB->VTOR + TIM2_IRQn对应的偏移)
```

如果：

```c
SCB->VTOR = 0x08000000;   // Bootloader
```

那 CPU 查的就是：

```text
Bootloader 的 TIM2 中断入口
```

如果：

```c
SCB->VTOR = 0x08010000;   // App
```

那 CPU 查的才是：

```text
App 的 TIM2 中断入口
```

---

# 1. Bootloader 里也配置了 TIM2，会发生什么？

假设 Bootloader 里面真的写了：

```c
void TIM2_IRQHandler(void)
{
    // Bootloader自己的TIM2中断处理
}
```

而 App 里面也写了：

```c
void TIM2_IRQHandler(void)
{
    // App自己的TIM2中断处理
}
```

但是你没有设置：

```c
SCB->VTOR = APP_ADDR;
```

那么 App 跑起来后，TIM2 中断来了，CPU 会执行：

```c
Bootloader 的 TIM2_IRQHandler()
```

而不是 App 的。

这时候就会出现很诡异的情况：

```text
主程序：App 的 main()
中断函数：Bootloader 的 TIM2_IRQHandler()
```

也就是：

```text
代码身体在 App，
中断灵魂还在 Bootloader。
```

---

# 2. Bootloader 没配置 TIM2，会发生什么？

这个更常见。

你可能会想：

> Bootloader 没配置 TIM2，那是不是 TIM2 中断就不会有？

不是。

因为现在 TIM2 是 App 配置的。

比如 App 里执行了：

```c
HAL_TIM_Base_Start_IT(&htim2);
```

或者底层做了这些事：

```c
TIM2->DIER |= TIM_DIER_UIE;       // 允许 TIM2 更新中断
NVIC_EnableIRQ(TIM2_IRQn);        // 允许 NVIC 响应 TIM2 中断
TIM2->CR1 |= TIM_CR1_CEN;         // 启动 TIM2
```

那么 TIM2 到时间后，仍然会产生中断。

**TIM2 中断是否产生，取决于外设 TIM2 和 NVIC 是否被 App 配置了。**

但是：

**TIM2 中断发生后跳到哪里执行，取决于 `SCB->VTOR`。**

这两个是两回事。

---

# 3. Bootloader 没写 TIM2_IRQHandler，向量表里是什么？

STM32 工程里的启动文件通常有类似这种弱定义：

```c
void TIM2_IRQHandler(void) __attribute__((weak, alias("Default_Handler")));
```

意思是：

```text
如果你自己写了 TIM2_IRQHandler，就用你写的。
如果你没写 TIM2_IRQHandler，就默认指向 Default_Handler。
```

所以 Bootloader 如果没用 TIM2，一般就没有自己实现 `TIM2_IRQHandler()`。

那么 Bootloader 的向量表里，TIM2 那一项通常就是：

```text
TIM2_IRQHandler -> Default_Handler
```

而 `Default_Handler` 通常长这样：

```c
void Default_Handler(void)
{
    while (1)
    {
    }
}
```

所以结果就是：

```text
App 配置 TIM2
TIM2 中断来了
CPU 去 Bootloader 向量表找 TIM2_IRQHandler
发现 Bootloader 没有 TIM2_IRQHandler
于是进入 Bootloader 的 Default_Handler
系统卡死
```

这就是你调试时可能看到的现象：

```text
App 明明在跑
一进 TIM2 中断
结果跑到 0x0800xxxx 的 Default_Handler 里死循环
```

---

# 4. 注意：Bootloader 没配置 TIM2，不代表 TIM2 中断不会发生

这个点很关键。

比如当前运行状态是：

```text
PC  正在 App 代码区
VTOR 还指向 Bootloader
```

然后 App 初始化了 TIM2：

```c
void app_timer_init(void)
{
    TIM2->PSC = 1000 - 1;
    TIM2->ARR = 1000 - 1;

    TIM2->DIER |= TIM_DIER_UIE;
    NVIC_EnableIRQ(TIM2_IRQn);

    TIM2->CR1 |= TIM_CR1_CEN;
}
```

这个时候 TIM2 外设真的被 App 启动了。

时间到了以后：

```text
TIM2->SR 里的 UIF 置 1
NVIC 发现 TIM2_IRQn pending
CPU 响应 TIM2 中断
```

然后 CPU 查中断入口：

```c
handler = *(uint32_t *)(SCB->VTOR + TIM2向量偏移);
```

如果 `SCB->VTOR` 还是 Bootloader：

```text
handler = Bootloader向量表里的TIM2入口
```

所以不管 Bootloader 有没有配置 TIM2，只要 App 把 TIM2 和 NVIC 打开了，中断就可能来。

问题只是：

```text
中断来了以后，CPU 去错地方执行。
```

---

# 5. 分几种情况看结果

## 情况 1：Bootloader 没写 TIM2_IRQHandler

Bootloader 启动文件里默认：

```c
TIM2_IRQHandler -> Default_Handler
```

结果：

```text
App 启动 TIM2
TIM2 中断来了
CPU 进 Bootloader 的 Default_Handler
系统卡死
```

这是最常见的。

---

## 情况 2：Bootloader 写了 TIM2_IRQHandler

比如 Bootloader 里面有：

```c
void TIM2_IRQHandler(void)
{
    TIM2->SR &= ~TIM_SR_UIF;
    bootloader_tick++;
}
```

结果：

```text
App 启动 TIM2
TIM2 中断来了
CPU 进 Bootloader 的 TIM2_IRQHandler
```

这时候可能不会马上死，但是逻辑错了。

因为它执行的是 Bootloader 的中断逻辑，不是 App 的。

可能出现：

```text
App 的定时任务不执行
App 的计数变量不增加
App 的电机控制周期不跑
Bootloader 的变量被错误修改
Bootloader 的中断清标志方式和 App 不一致
系统行为很诡异
```

---

## 情况 3：Bootloader 的 TIM2_IRQHandler 没有正确清 TIM2 中断标志

比如 Bootloader 中断函数没有清除 App 配置产生的 TIM2 更新标志：

```c
void TIM2_IRQHandler(void)
{
    // 没有清 TIM2->SR 的 UIF
}
```

那么会出现：

```text
进入 TIM2_IRQHandler
退出中断
发现 TIM2 中断标志还在
马上再次进入 TIM2_IRQHandler
再退出
又马上进入
...
```

结果就是：

```text
CPU 一直卡在 TIM2 中断里
主循环几乎不运行
看起来像死机
```

---

## 情况 4：Bootloader 的 TIM2_IRQHandler 用了 Bootloader 自己的全局变量/句柄

比如 Bootloader 用 HAL：

```c
TIM_HandleTypeDef htim2;

void TIM2_IRQHandler(void)
{
    HAL_TIM_IRQHandler(&htim2);
}
```

但是现在实际运行的是 App。

App 里面也有自己的：

```c
TIM_HandleTypeDef htim2;
```

注意，这两个 `htim2` 不是一个东西。

Bootloader 的 `htim2` 在 Bootloader 的 RAM 区域，App 的 `htim2` 在 App 的 RAM 区域。

如果 CPU 跑到了 Bootloader 的中断函数，它用的是 Bootloader 的 `htim2`。

这个 `htim2` 可能已经不是有效状态了。

可能导致：

```text
HAL 状态错误
回调不执行
访问异常
清标志异常
进入 HardFault
```

所以这类问题非常隐蔽。

---

# 6. 可以把它理解成“中断函数地址拿错了”

App 里面本来应该是：

```text
TIM2中断 -> App_TIM2_IRQHandler
```

但因为 VTOR 没改，实际变成：

```text
TIM2中断 -> Bootloader_TIM2_IRQHandler
```

或者：

```text
TIM2中断 -> Bootloader_Default_Handler
```

所以问题不在于 TIM2 是谁配置的。

问题在于：

```text
中断来了以后，CPU 去哪张表里取函数地址。
```

---

# 7. 你可以用这个流程记住

正确情况：

```text
App 配置 TIM2
    ↓
TIM2 到时间产生中断
    ↓
NVIC 通知 CPU
    ↓
CPU 查 App 向量表
    ↓
执行 App 的 TIM2_IRQHandler
```

错误情况：

```text
App 配置 TIM2
    ↓
TIM2 到时间产生中断
    ↓
NVIC 通知 CPU
    ↓
CPU 还在查 Bootloader 向量表
    ↓
执行 Bootloader 的 TIM2_IRQHandler
    ↓
如果 Bootloader 没写，就进 Default_Handler
```

---

# 8. 所以 Bootloader 跳 App 时必须做这件事

```c
SCB->VTOR = APP_ADDR;
```

例如：

```c
#define APP1_ADDR  0x08010000U

SCB->VTOR = APP1_ADDR;
```

这样 TIM2 中断来了以后，CPU 查的是：

```text
0x08010000 + TIM2向量偏移
```

也就是 App 自己的向量表。

---

# 9. 最直接的调试现象

如果你遇到这种情况：

```text
App main 能跑
一开定时器中断就死
调试器停在 Default_Handler
Default_Handler 地址在 Bootloader 区域
```

那基本可以重点怀疑：

```text
SCB->VTOR 没有切到 App
```

可以在 App 运行后看一下：

```c
SCB->VTOR
```

如果它还是：

```text
0x08000000
```

那就是错的。

App1 应该是：

```text
0x08010000
```

App2 应该是：

```text
0x08050000
```

具体看你的 Flash 分区。

---

# 10. 一句话总结

**Bootloader 没配置 TIM2，也不代表没事。**

因为：

```text
App 可以配置 TIM2，让 TIM2 中断产生。
但是 CPU 响应中断时，如果 VTOR 还指向 Bootloader，
它就会去 Bootloader 的向量表找 TIM2_IRQHandler。
```

如果 Bootloader 有 TIM2_IRQHandler：

```text
执行 Bootloader 的 TIM2 中断函数。
```

如果 Bootloader 没有 TIM2_IRQHandler：

```text
通常进入 Bootloader 的 Default_Handler，然后卡死。
```

所以中断向量表偏移不是可选项，而是 Bootloader 跳 App 的必要步骤。
