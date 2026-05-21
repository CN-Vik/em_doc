非常棒 👍！
你现在已经拿到了 **FreeRTOS 的三驾马车核心实现：**
✅ `vPortSVCHandler()`
✅ `xPortPendSVHandler()`
✅ `xPortSysTickHandler()`

下面我们就结合 **GD32F4xx (Cortex-M4)** 实际应用案例，彻底讲透它们在系统启动、任务调度、上下文切换中的作用。
我会带你走一遍 **从 main() → 第一个任务运行 → 多任务切换** 的全过程，让你清楚每条汇编指令到底干了什么。

---

## 🧭 一、系统启动阶段（main → 启动第一个任务）

假设你的主函数是这样的：

```c
int main(void)
{
    hardware_init();

    xTaskCreate(taskA, "TaskA", 256, NULL, 2, NULL);
    xTaskCreate(taskB, "TaskB", 256, NULL, 2, NULL);

    vTaskStartScheduler();   // ⬅️ 从这里开始 FreeRTOS 核心逻辑
    for(;;);
}
```

### Step 1️⃣：进入 `vTaskStartScheduler()`

* 初始化任务控制块（TCB）；
* 设置 **SysTick 定时器** 周期中断（tick 时间，比如 1ms）；
* 设置 **PendSV 和 SysTick 的中断优先级** 为最低；
* 最关键：调用 `vPortStartFirstTask()`（触发一次 **SVC**）。

---

## 🚀 二、启动第一个任务（SVC_Handler 的作用）

FreeRTOS 在启动第一个任务时不会直接跳过去执行任务，而是：

> 通过触发一次 **SVC (Supervisor Call)**，让 CPU 从 Handler 模式切换回 Thread 模式，并设置 PSP。

### 触发 SVC 的伪代码：

```c
__asm volatile( "svc 0" );   // 触发异常
```

### 然后进入你提供的：

```c
void vPortSVCHandler(void)
```

---

### 🧩 `vPortSVCHandler()` 实际做的事

这段汇编的目的是：

> 从当前任务（`pxCurrentTCB`）中恢复任务栈内容，切换堆栈到 PSP，并跳转到任务入口点。

来看汇编逻辑：

```asm
ldr r3, pxCurrentTCBConst2   ; 加载当前任务控制块指针变量地址
ldr r1, [r3]                 ; r1 = pxCurrentTCB
ldr r0, [r1]                 ; r0 = 当前任务的栈顶地址
ldmia r0!, {r4-r11, r14}     ; 从任务栈中恢复寄存器 r4-r11, r14
msr psp, r0                  ; 设置 PSP（Process Stack Pointer）
isb
mov r0, #0
msr basepri, r0              ; 清空中断屏蔽
bx r14                       ; 从异常返回 → 跳入任务
```

**执行完这段后：**

* PSP 指向当前任务的堆栈；
* CPU 从 Handler 模式返回到 Thread 模式；
* 当前任务正式开始执行。

✅ **第一个任务启动成功！**

---

## 🕒 三、系统运行阶段（SysTick 中断节拍）

每隔 1ms（或指定时间），SysTick 会进入：

```c
void xPortSysTickHandler(void)
```

### SysTickHandler 的作用：

1️⃣ 进入中断（tick 到来）
2️⃣ 调用 `xTaskIncrementTick()` 增加系统节拍计数；
3️⃣ 判断是否有任务延时到期、优先级更高的任务 ready；
4️⃣ 如果需要任务切换，则 **设置 PendSV 异常挂起**：

```c
portNVIC_INT_CTRL_REG = portNVIC_PENDSVSET_BIT;
```

📌 注意：这里只是“挂起”，不立即切换任务，
因为真正的上下文切换要放到 **PendSV_Handler** 中执行（优先级最低、可延后）。

---

## ⚙️ 四、任务切换阶段（PendSV_Handler）

当 `xPortSysTickHandler()` 设置 `PendSV` 挂起位后，
NVIC 会在所有高优先级中断都处理完后，执行：

```c
void xPortPendSVHandler(void)
```

---

### 🧩 `xPortPendSVHandler()` 做的核心工作：

> 1️⃣ 保存当前任务上下文（r4-r11, FPU寄存器）；
> 2️⃣ 调用 `vTaskSwitchContext()` 选择下一个任务；
> 3️⃣ 恢复下一个任务上下文；
> 4️⃣ 更新 PSP，返回到新任务。

---

### 详细执行流程：

```asm
mrs r0, psp              ; 取当前任务的 PSP
isb
ldr r3, pxCurrentTCBConst
ldr r2, [r3]             ; r2 = pxCurrentTCB

; 检查是否使用 FPU，若使用则先保存 s16-s31
tst r14, #0x10
it eq
vstmdbeq r0!, {s16-s31}

stmdb r0!, {r4-r11, r14} ; 保存核心寄存器
str r0, [r2]             ; 把当前任务栈顶保存进 TCB

; --- 调用调度器选下一个任务 ---
stmdb sp!, {r0, r3}
mov r0, %0
msr basepri, r0
bl vTaskSwitchContext    ; 内核选择下一个 ready 任务
mov r0, #0
msr basepri, r0
ldmia sp!, {r0, r3}

; --- 恢复新任务上下文 ---
ldr r1, [r3]
ldr r0, [r1]
ldmia r0!, {r4-r11, r14}
tst r14, #0x10
it eq
vldmiaeq r0!, {s16-s31}
msr psp, r0
isb
bx r14                   ; 返回 → 新任务运行
```

---

### 🎯 结果

当这段代码结束：

* CPU 的寄存器和 PSP 已经全部切换；
* 下一任务继续执行；
* **整个上下文切换过程对用户是“透明”的**。

---

## 🪄 五、整个 FreeRTOS 调度链回顾

```
┌─────────────────────────────┐
│ main()                      │
│   ├─ 初始化硬件             │
│   ├─ 创建任务A/B            │
│   └─ vTaskStartScheduler()  │
└────────────┬────────────────┘
             │
             ▼
     +------------------+
     | vPortSVCHandler  | ← 启动第一个任务
     +------------------+
             │
             ▼
   +-------------------+
   | 任务A/B运行中...  |
   +-------------------+
             │
             ▼
     +------------------+
     | xPortSysTickHandler |
     | （1ms节拍）         |
     +------------------+
             │
             ▼
     +------------------+
     | xPortPendSVHandler |
     | （上下文切换）     |
     +------------------+
             │
             ▼
   +-------------------+
   | 下一个任务继续执行 |
   +-------------------+
```

---

## 🧩 六、实际应用场景举例：无人机飞控任务调度

比如你的 GD32F4xx 做无人机控制：

| 任务           | 优先级 | 功能          | 周期   |
| ------------ | --- | ----------- | ---- |
| `sensorTask` | 3   | 读取IMU/I2C数据 | 1ms  |
| `ctrlTask`   | 2   | PID运算/姿态控制  | 5ms  |
| `motorTask`  | 2   | PWM输出控制电机   | 5ms  |
| `uiTask`     | 1   | OLED SPI刷新  | 50ms |

运行时序：

1️⃣ SysTick 每 1ms 触发 `xPortSysTickHandler()`；
2️⃣ `xTaskIncrementTick()` 检查 1ms 周期任务（sensorTask）ready；
3️⃣ 若 sensorTask 比当前任务优先级高 → 触发 PendSV；
4️⃣ `xPortPendSVHandler()` 保存 ctrlTask 上下文，恢复 sensorTask 上下文；
5️⃣ `sensorTask` 运行完 → `vTaskDelay(1)` 挂起；
6️⃣ 再次调度 PendSV → 切回 ctrlTask；
7️⃣ 整个飞控任务系统稳定循环运行。

---

## 🔍 七、常见问题调试建议

| 现象                 | 可能原因                                  |
| ------------------ | ------------------------------------- |
| 系统卡在第一个任务          | `SVC_Handler` 未正确触发（未配置向量表 / PSP 未切换） |
| SysTick 打印正常但任务不切换 | `PendSV` 优先级错误（未设为最低）                 |
| 一切都不运行             | 向量表未指向正确的 FreeRTOS Handler            |
| FPU 异常             | 没有在 PendSV 保存/恢复 FPU 寄存器              |

---

## ✅ 八、小结：你应该牢牢记住的三句话

| Handler             | 关键功能    | 作用时机       |
| ------------------- | ------- | ---------- |
| **SVC_Handler**     | 启动第一个任务 | 仅启动时执行一次   |
| **SysTick_Handler** | 系统节拍心跳  | 周期性执行（1ms） |
| **PendSV_Handler**  | 实现上下文切换 | 每次需要切任务时执行 |

---

如果你愿意，我可以帮你画出一张「**任务切换寄存器堆栈图（含MSP/PSP/FPU栈结构）**」，
展示 Cortex-M4 进出异常时栈帧布局 + FreeRTOS 手工保存部分，
让你彻底理解任务是怎么被“冻结、恢复”的。

要我帮你画这张栈图吗？


------
-----
----

非常好 👍 那我们就结合你现在的 **FreeRTOS + GD32F4XX** 实际情况，一步一步“串起来讲”，让你彻底搞清楚 **SysTick + PendSV + SVC** 是怎么联动起来实现任务切换的。

---

## 🧠 一、从全局看 FreeRTOS 的任务调度原理

FreeRTOS 在 Cortex-M（GD32F4XX 属于 M4）上靠 **三个关键中断/异常** 来实现多任务调度：

| 模块          | 异常名       | 作用                              |
| ----------- | --------- | ------------------------------- |
| **SysTick** | 系统滴答定时器中断 | 周期触发 → 系统节拍计数（Tick），检测任务延时、唤醒任务 |
| **PendSV**  | 可挂起系统服务异常 | 真正执行**任务切换（上下文切换）**             |
| **SVC**     | 系统调用异常    | 用来启动第一个任务（仅在调度器启动时使用一次）         |

---

## ⚙️ 二、任务调度流程的时序图

当你调用：

```c
vTaskStartScheduler();
```

整个启动和任务切换过程如下 👇

### 🟢 Step 1：启动调度器

```c
vTaskStartScheduler()
    └── xPortStartScheduler()
            ├── 设置 PendSV 和 SysTick 的优先级（最低）
            ├── 配置 SysTick 定时周期（比如 1ms）
            └── 调用 vPortSVCHandler() （通过 SVC 指令触发）
```

---

### 🟡 Step 2：SVC 异常（启动第一个任务）

`vPortSVCHandler()` 是汇编实现的：

```asm
ldr r3, pxCurrentTCBConst2
ldr r1, [r3]         ; 取出当前任务控制块指针
ldr r0, [r1]         ; 取出任务栈顶指针
ldmia r0!, {r4-r11, r14}
msr psp, r0          ; 设置任务栈指针（PSP）
bx r14               ; 跳转执行任务
```

📘 解释：

* 这一步将第一个任务的上下文恢复到 CPU 寄存器中；
* 然后通过 `bx r14` 从异常返回，**CPU 开始用 PSP 运行第一个任务**；
* 此时 FreeRTOS 正式“开始跑任务”。

---

### 🔵 Step 3：SysTick 周期触发中断

SysTick 每 1ms 进入中断，调用：

```c
xPortSysTickHandler()
```

流程：

1. 调用 `xTaskIncrementTick()` → 系统时钟 +1
2. 检查是否有任务需要切换（例如高优先级任务就绪）
3. 如果需要切换，则设置：

   ```c
   portNVIC_INT_CTRL_REG = portNVIC_PENDSVSET_BIT;
   ```

   👉 手动触发 PendSV 异常！

---

### 🔴 Step 4：PendSV 异常（执行任务切换）

`xPortPendSVHandler()` 做的事就是：

1. 保存当前任务上下文（寄存器 → 当前 TCB 的栈中）；
2. 调用 `vTaskSwitchContext()`，找到下一个要运行的任务；
3. 恢复下一个任务的上下文；
4. 从异常返回，CPU 继续执行新任务。

```asm
mrs r0, psp
stmdb r0!, {r4-r11, r14}   ; 保存现场
str r0, [r2]                ; 存回 TCB
bl vTaskSwitchContext       ; 找到下一个任务
ldr r1, [r3]                ; 新任务 TCB
ldr r0, [r1]
ldmia r0!, {r4-r11, r14}    ; 恢复现场
msr psp, r0
bx r14
```

---

## 🧩 三、整个流程总结（任务切换时序）

```
                SysTick定时器溢出
                        │
                        ▼
            xPortSysTickHandler()  （tick +1）
                        │
                        ▼
           判断是否有任务需要切换？
                        │
               ┌────────┴────────┐
               │                 │
           否：返回           是：触发PendSV
                                 │
                                 ▼
                    xPortPendSVHandler()
               （保存旧任务、切换TCB、恢复新任务）
                                 │
                                 ▼
                        新任务运行
```

---

## 💡 四、实际案例：两个任务切换演示

```c
void vTask1(void *pvParameters)
{
    for (;;)
    {
        printf("Task1 running\n");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void vTask2(void *pvParameters)
{
    for (;;)
    {
        printf("Task2 running\n");
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

int main(void)
{
    SystemInit();
    printf("System Init OK\n");

    xTaskCreate(vTask1, "T1", 256, NULL, 1, NULL);
    xTaskCreate(vTask2, "T2", 256, NULL, 2, NULL); // 高优先级

    vTaskStartScheduler();

    for (;;);
}
```

### 🔍 执行过程：

1. `vTaskStartScheduler()` 调用 SVC → 启动 Task2；
2. Task2 运行 500ms，`vTaskDelay()` 进入阻塞；
3. SysTick 中断触发，Tick +1；
4. FreeRTOS 检测 Task2 超时 → 唤醒；
5. SysTick 触发 PendSV → 切回 Task2；
6. 在串口上可看到：

   ```
   Task2 running
   Task1 running
   Task2 running
   Task1 running
   ```

---

## 🚦 五、总结要点（记牢）

| 异常名         | 调用时机      | 主要作用        |
| ----------- | --------- | ----------- |
| **SVC**     | 调度器启动     | 恢复第一个任务     |
| **SysTick** | 周期中断      | 更新系统时基、触发调度 |
| **PendSV**  | SysTick触发 | 真正执行任务切换    |

---

要不要我接下来再帮你画一个 “FreeRTOS 在 GD32F4XX 上的完整中断优先级图”
（包含 **SysTick、PendSV、SVC** 三个优先级关系 + NVIC 配置方式 + 常见错误原因）？
这个图对理解 FreeRTOS 在 Cortex-M4 的运行机制非常直观。

----
-----
----
非常好 👍
那我们就来画出一张 **FreeRTOS 在 GD32F4XX（Cortex-M4）上的中断优先级全景图**，
并结合源码解释 **SVC、PendSV、SysTick 三者的优先级关系、作用和常见坑点**。

---

## 🧠 一、FreeRTOS 的三大“核心异常”

| 异常名                                | 触发方式                                                   | 作用                    | 是否自动触发   |
| :--------------------------------- | :----------------------------------------------------- | :-------------------- | :------- |
| **SVC (Supervisor Call)**          | 软件触发（`SVC` 指令）                                         | 启动第一个任务               | ✅ 仅启动时一次 |
| **PendSV (Pendable Service Call)** | 软件触发（`portNVIC_INT_CTRL_REG = portNVIC_PENDSVSET_BIT`） | 执行任务切换（上下文保存/恢复）      | ✅ 每次任务切换 |
| **SysTick (System Tick Timer)**    | 定时器自动中断                                                | 系统时基（1ms Tick），触发任务切换 | ✅ 周期性    |

---

## ⚙️ 二、FreeRTOS 的中断优先级配置关系图（GD32F4XX）

Cortex-M 内核支持 4 bit 优先级（`__NVIC_PRIO_BITS = 4`），优先级范围是：

```
0x00（最高优先级） ～ 0x0F（最低优先级）
```

FreeRTOS 的关键优先级配置项如下：

```c
#define configKERNEL_INTERRUPT_PRIORITY         ( 15 << (8 - __NVIC_PRIO_BITS) ) // 最低
#define configMAX_SYSCALL_INTERRUPT_PRIORITY    ( 5  << (8 - __NVIC_PRIO_BITS) ) // 可调用RTOS API的最高中断优先级
```

---

### 🧩 中断优先级示意图

```
优先级数字越小 → 优先级越高
─────────────────────────────────────────────
| 优先级 | 异常 / 中断名            | 说明 |
|:-------:|---------------------------|:------|
|  0-4    | 高优先级硬件中断         | 不允许调用 FreeRTOS API |
|  5      | 可调用 RTOS API 的中断   | 例如 UART、DMA、I2C 等安全中断 |
|  6-14   | 用户其他中断（一般用途） |
|  **15** | **SysTick、PendSV、SVC** | FreeRTOS 核心中断（最低） |
─────────────────────────────────────────────
```

🟢 **FreeRTOS 设计原理：**

* 把 **SVC、PendSV、SysTick** 统一设置成最低优先级（`15`）。
* 这样可以保证：

  * 所有外设中断都可以“打断”RTOS tick；
  * 而任务切换永远在“安全的最后阶段”进行。

---

## 🔧 三、代码中优先级的设置位置

在 `xPortStartScheduler()` 或 `port.c` 里，你会看到：

```c
NVIC_SetPriority( PendSV_IRQn, configKERNEL_INTERRUPT_PRIORITY );
NVIC_SetPriority( SysTick_IRQn, configKERNEL_INTERRUPT_PRIORITY );
NVIC_SetPriority( SVCall_IRQn, configKERNEL_INTERRUPT_PRIORITY );
```

👉 **三者的优先级都被设置成最低（configKERNEL_INTERRUPT_PRIORITY）**。

---

## 🚦 四、任务切换时的优先级时序

下面是完整的 **时序图**：

```
  SysTick 定时器溢出中断
        │
        ▼
  SysTick_Handler()
        │
        │── 调用 xTaskIncrementTick()
        │       │
        │       ├─ 检查任务延时是否到期
        │       └─ 若需切换任务 → 触发 PendSV
        ▼
  PendSV_Handler()
        │
        ├─ 保存当前任务寄存器
        ├─ vTaskSwitchContext() 找新任务
        ├─ 恢复新任务寄存器
        ▼
  从异常返回 → CPU 执行新任务
```

---

## 🧩 五、三个异常的协同关系总结表

| 异常名         |   优先级  | 调用方式  | 使用阶段 | 核心作用        |
| :---------- | :----: | :---- | :--- | :---------- |
| **SVC**     | 最低（15） | 软件触发  | 启动时  | 恢复第一个任务上下文  |
| **SysTick** | 最低（15） | 定时器中断 | 运行中  | 周期计数 + 触发调度 |
| **PendSV**  | 最低（15） | 软件触发  | 运行中  | 真正执行任务切换    |

---

## ⚠️ 六、常见配置错误（在 GD32F4XX 移植时特别容易踩坑）

| 错误类型                | 表现                           | 原因分析                                                                  |
| :------------------ | :--------------------------- | :-------------------------------------------------------------------- |
| **SysTick 优先级设置太高** | FreeRTOS 一运行就“卡死”或“Tick 不增加” | SysTick 中断优先级高于 `configMAX_SYSCALL_INTERRUPT_PRIORITY`，违反 FreeRTOS 要求 |
| **PendSV 优先级错误**    | 任务切换异常 / HardFault           | PendSV 没设为最低，导致在切换过程中被打断                                              |
| **SVC 未正确链接**       | 调度器启动后无反应                    | 矢量表中 `SVC_Handler` 未正确指向 `vPortSVCHandler`                            |
| **SysTick 源未配置**    | 没 tick                       | 没有初始化 `SysTick_Config(SystemCoreClock / configTICK_RATE_HZ)`          |

---

## 🔍 七、矢量表绑定（重点检查）

在 `startup_gd32f4xx.s` 或者 `gd32f4xx_it.c` 中确保下面这三个异常指向 FreeRTOS 的版本：

```c
void SVC_Handler(void)          { vPortSVCHandler(); }
void PendSV_Handler(void)       { xPortPendSVHandler(); }
void SysTick_Handler(void)      { xPortSysTickHandler(); }
```

✅ 如果没有改，会出现：

* “任务一直不切换”
* “系统卡死在 SysTick_Handler”
* “SVC_Handler 进入 HardFault”

---

## 🧭 八、总结一句话

> FreeRTOS 通过 **SysTick → PendSV → SVC** 三个异常组合，
> 在 Cortex-M 的异常机制上 **实现了完整的任务调度框架**。
>
> 其中：
>
> * **SysTick** 定时触发；
> * **PendSV** 真正切换上下文；
> * **SVC** 启动第一个任务；
>
> 三者优先级都是最低，以保证任务切换的安全性和确定性。

---

我可以帮你画一张 **更直观的时序+优先级综合图（像芯片手册那种）**，
展示从 `vTaskStartScheduler()` 到第一个任务运行，再到周期性任务切换的全过程（带栈切换箭头）。

要不要我帮你画出来？
