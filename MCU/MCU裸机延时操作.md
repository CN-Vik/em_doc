可以，就按你这个**最终架构不改**，把它整理成一个嵌入式 C 语言“非阻塞延时”学习案例。

你的核心写法是：

```c
static u32 peel_time_t0 = 0, peel_time_t1 = 0;

if (sensor.complete_weight_peel == ((u8)0x66))
{
    peel_time_t0 = mode.sys_time_ms;

    if ((peel_time_t0 - peel_time_t1) >= 1800)  /* 延时1800ms */
    {
        sensor.complete_weight_peel = 0;
        Beep(0, 1);
    }
}
else
{
    peel_time_t1 = mode.sys_time_ms;
    peel_time_t0 = mode.sys_time_ms;
}
```

---

# 嵌入式非阻塞延时学习案例

## 1. 案例目标

在嵌入式开发中，经常会遇到这种需求：

> 当某个条件成立后，不要马上执行动作，而是延时一段时间后再执行。

比如你的场景：

```c
sensor.complete_weight_peel == 0x66
```

表示“去皮完成”。

但是你不想立刻蜂鸣，而是想延时大约 `1800ms` 后执行：

```c
sensor.complete_weight_peel = 0;
Beep(0, 1);
```

---

## 2. 不推荐的写法：阻塞延时

新手可能会这样写：

```c
if (sensor.complete_weight_peel == ((u8)0x66))
{
    delay_ms(1800);

    sensor.complete_weight_peel = 0;
    Beep(0, 1);
}
```

这个写法虽然简单，但是在嵌入式里有问题。

因为 `delay_ms(1800)` 会让程序卡在这里 1.8 秒。

这 1.8 秒里面，其他任务可能都无法及时执行，比如：

```c
按键扫描
电机控制
重量检测
串口通信
WiFi处理
状态机运行
```

所以在主循环、状态机、周期任务里面，一般不推荐用这种阻塞式延时。

---

# 3. 推荐思想：非阻塞延时

非阻塞延时的核心思想是：

> 不让程序停下来等待，而是每次进来都看一下“时间到了没有”。

也就是不用：

```c
delay_ms(1800);
```

而是用：

```c
if ((当前时间 - 起始时间) >= 1800)
{
    // 时间到了，执行动作
}
```

你的代码就是这个思想。

---

# 4. 完整学习版代码

保持你的架构不变，只加详细中文注释：

```c
/*
 * 非阻塞延时案例：
 *
 * 功能：
 * 当 sensor.complete_weight_peel == 0x66 时，
 * 表示去皮完成。
 *
 * 代码不会立刻蜂鸣，而是等待一段时间。
 * 当等待时间 >= 1800ms 后：
 * 1. 清除 complete_weight_peel 标志
 * 2. 执行蜂鸣 Beep(0, 1)
 *
 * 注意：
 * 这个代码不会阻塞主循环。
 */

static u32 peel_time_t0 = 0;
static u32 peel_time_t1 = 0;

/*
 * 判断去皮完成标志是否置位
 */
if (sensor.complete_weight_peel == ((u8)0x66))
{
    /*
     * peel_time_t0 表示当前时间
     *
     * mode.sys_time_ms 一般是系统毫秒计数，
     * 比如系统每 1ms 加 1。
     */
    peel_time_t0 = mode.sys_time_ms;

    /*
     * peel_time_t0 - peel_time_t1 表示已经过去的时间
     *
     * 当 complete_weight_peel 不等于 0x66 时，
     * peel_time_t1 会一直被刷新为当前时间。
     *
     * 一旦 complete_weight_peel 变成 0x66，
     * 程序就不会再进入下面的 else，
     * 所以 peel_time_t1 停留在“刚进入 0x66 前的时间点”。
     *
     * 之后 peel_time_t0 不断更新为当前时间。
     *
     * 所以：
     *
     * peel_time_t0 - peel_time_t1
     *
     * 就表示 complete_weight_peel 变成 0x66 后，
     * 已经经过了多少毫秒。
     */
    if ((peel_time_t0 - peel_time_t1) >= 1800)  /* 延时1800ms */
    {
        /*
         * 延时时间到，清除去皮完成标志
         */
        sensor.complete_weight_peel = 0;

        /*
         * 蜂鸣一次
         */
        Beep(0, 1);
    }
}
else
{
    /*
     * 当 complete_weight_peel 不是 0x66 时，
     * 说明当前还没有触发去皮完成事件。
     *
     * 这里一直刷新 peel_time_t1 和 peel_time_t0。
     *
     * 这样做的效果是：
     * 一旦 complete_weight_peel 变成 0x66，
     * peel_time_t1 就会停在“事件刚发生前”的时间。
     */
    peel_time_t1 = mode.sys_time_ms;
    peel_time_t0 = mode.sys_time_ms;
}
```

---

# 5. 这个代码是怎么延时的？

假设你的系统时间 `mode.sys_time_ms` 一直增加：

```c
1000
1001
1002
1003
...
```

刚开始：

```c
sensor.complete_weight_peel != 0x66
```

所以程序一直执行：

```c
peel_time_t1 = mode.sys_time_ms;
peel_time_t0 = mode.sys_time_ms;
```

假设此时：

```c
peel_time_t1 = 5000;
peel_time_t0 = 5000;
```

---

某一刻，去皮完成：

```c
sensor.complete_weight_peel = 0x66;
```

程序开始进入：

```c
if (sensor.complete_weight_peel == ((u8)0x66))
```

这时 `peel_time_t1` 不再刷新。

假设：

```c
peel_time_t1 = 5000;
```

然后每次调用时，`peel_time_t0` 都会更新。

比如第一次：

```c
peel_time_t0 = 5010;
peel_time_t0 - peel_time_t1 = 5010 - 5000 = 10;
```

还没到 1800ms。

---

过了一会儿：

```c
peel_time_t0 = 6000;
peel_time_t1 = 5000;
peel_time_t0 - peel_time_t1 = 1000;
```

还是没到 1800ms。

---

再过一会儿：

```c
peel_time_t0 = 6800;
peel_time_t1 = 5000;
peel_time_t0 - peel_time_t1 = 1800;
```

满足条件：

```c
if ((peel_time_t0 - peel_time_t1) >= 1800)
```

于是执行：

```c
sensor.complete_weight_peel = 0;
Beep(0, 1);
```

---

# 6. 为什么执行完后不会一直蜂鸣？

因为时间到后，你执行了：

```c
sensor.complete_weight_peel = 0;
```

下一次再进入这个函数时：

```c
sensor.complete_weight_peel == ((u8)0x66)
```

这个条件就不成立了。

程序进入：

```c
else
{
    peel_time_t1 = mode.sys_time_ms;
    peel_time_t0 = mode.sys_time_ms;
}
```

所以它会重新刷新时间，不会一直执行 `Beep(0, 1)`。

这点很关键。

---

# 7. 这个代码属于哪种延时？

它属于：

```c
非阻塞延时
```

也可以叫：

```c
基于系统 Tick 的软件定时
```

或者：

```c
时间差判断延时
```

核心公式就是：

```c
当前时间 - 起始时间 >= 延时时间
```

对应到你的代码就是：

```c
peel_time_t0 - peel_time_t1 >= 1800
```

---

# 8. 这个写法适合用在哪里？

适合用在这些地方：

```c
主循环 while(1)
10ms周期任务
状态机函数
传感器处理函数
按键消抖
蜂鸣器延时
LED闪烁
通信超时判断
电机状态切换
```

比如：

```c
while (1)
{
    Key_Process();
    Sensor_Process();
    Weight_Process();
    Motor_Process();
    Wifi_Process();
}
```

你的延时逻辑放在 `Weight_Process()` 里面，就不会阻塞其他任务。

---

# 9. 这个写法的重点

你这个代码最重要的点是：

```c
else
{
    peel_time_t1 = mode.sys_time_ms;
    peel_time_t0 = mode.sys_time_ms;
}
```

这个 `else` 的作用不是多余的。

它是在事件没发生时，不断刷新起点时间。

一旦事件发生：

```c
sensor.complete_weight_peel == 0x66
```

就不再进入 `else`，所以 `peel_time_t1` 固定住。

然后：

```c
peel_time_t0
```

继续更新。

于是两个时间相减，就能得到事件发生后的经过时间。

---

# 10. 最终推荐保留版本

你的最终版本可以这样放到工程里：

```c
#define WEIGHT_PEEL_DONE_FLAG     ((u8)0x66)
#define WEIGHT_PEEL_DELAY_MS      1800u

static u32 peel_time_t0 = 0;
static u32 peel_time_t1 = 0;

if (sensor.complete_weight_peel == WEIGHT_PEEL_DONE_FLAG)
{
    peel_time_t0 = mode.sys_time_ms;

    if ((u32)(peel_time_t0 - peel_time_t1) >= WEIGHT_PEEL_DELAY_MS)
    {
        sensor.complete_weight_peel = 0;
        Beep(0, 1);
    }
}
else
{
    peel_time_t1 = mode.sys_time_ms;
    peel_time_t0 = mode.sys_time_ms;
}
```

这个没有改变你的架构，只是把魔法数字提成了宏：

```c
#define WEIGHT_PEEL_DONE_FLAG     ((u8)0x66)
#define WEIGHT_PEEL_DELAY_MS      1800u
```

这样以后你想改延时时间，就不用到代码里面找 `1800` 了，直接改宏即可。

---

# 11. 一句话总结

你这个代码的本质是：

```c
当 complete_weight_peel 不是 0x66 时，不断刷新起始时间；
当 complete_weight_peel 变成 0x66 后，停止刷新起始时间；
然后用当前时间减去起始时间；
超过 1800ms 后，清标志并蜂鸣。
```

这就是嵌入式里非常常见的**非阻塞延时写法**。
