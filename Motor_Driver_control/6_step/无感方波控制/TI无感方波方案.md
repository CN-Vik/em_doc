# 无感BLDC为啥需要检测零点换向呢？？
> **六步换向核心知识点：电机励磁磁场每换一相，电角度旋转60度，机械转子角度旋转60/7极对数=8.5度**

对，你这个问题其实正好抓住了**六步 BLDC 的本质**。

你说的：

> “我直接按照 AC → AB → BC → BA……不断切换，不就等于让定子磁场旋转了吗？”

**理论上完全可以。**

而且你之前“上电抖一下，然后不转”，并不是因为这个思路错了，而是因为你缺了一个非常关键的东西：

> **“什么时候切到下一步？”**

如果你只是按照固定时间切换，你做的是**开环六步控制**；如果根据电机实际转子位置决定什么时候切换，你做的是**闭环六步控制**。

---

# 1. 先回答你的核心问题：能不能直接循环六个状态？

**能。**

比如：

```text
状态0：A+ B-
状态1：A+ C-
状态2：B+ C-
状态3：B+ A-
状态4：C+ A-
状态5：C+ B-
```

然后：

```text
0
↓
1
↓
2
↓
3
↓
4
↓
5
↓
0
↓
1
↓
...
```

如果每一步停留：

```text
100 us
```

那就是：

```text
A+B-
   ↓ 100us
A+C-
   ↓ 100us
B+C-
   ↓ 100us
B+A-
   ↓ 100us
C+A-
   ↓ 100us
C+B-
   ↓ 100us
```

**电机确实有可能转起来。**

这就是最简单的：

> **开环六步 BLDC。**

所以你的思路没毛病。

---

# 2. 那为什么实际电机会“抖一下，然后不转”？

这里才是关键。

因为你以为：

```text
State0
 ↓
State1
 ↓
State2
```

代表：

> “定子磁场从位置 A 转到了位置 B。”

但实际上不是这么简单。

真正发生的是：

```text
定子磁场
       ↓
试图吸引/排斥
       ↓
转子永磁体
       ↓
产生电磁转矩
       ↓
转子开始运动
```

问题在于：

> **转子需要时间跟上你的定子磁场。**

---

# 3. 一个特别重要的概念：同步

假设你的定子磁场：

```text
位置：
0°
 ↓
60°
 ↓
120°
 ↓
180°
 ↓
240°
 ↓
300°
```

你以为转子：

```text
0°
 ↓
60°
 ↓
120°
 ↓
180°
```

会完美跟着走。

但真实情况可能是：

```text
定子磁场：

0°
   ↓
60°
   ↓
120°
   ↓
180°
```

转子：

```text
0°
 ↓
20°
 ↓
45°
 ↓
70°
```

它**根本没跟上**。

---

# 4. 为什么转子跟不上？

因为转子有：

* 转动惯量
* 摩擦
* 负载
* 电机电感
* 绕组电流建立时间

尤其是：

$$
T=J\frac{d\omega}{dt}
$$

电机产生的转矩是有限的。

你突然把磁场：

```text
0°
```

跳到：

```text
60°
```

转子不会瞬间：

```text
0° → 60°
```

它需要加速。

---

# 5. 更要命的是：启动的时候你根本不知道转子在哪里

比如你上电的时候：

```text
定子磁场：
        ↑
        │
        │
       N
```

而转子：

```text
      S
      │
      │
      N
```

假设刚好方向不合适。

你下一瞬间直接：

```text
State0 → State1
```

转子可能还没来得及过去。

你又：

```text
State1 → State2
```

然后：

```text
State2 → State3
```

---

# 6. 于是就会出现这种情况

你以为：

```text
定子磁场：

A
 \
  B
   \
    C
     \
      A
```

在旋转。

但实际上：

```text
定子磁场：

→ → → → →

转子：

↑
↑
↗
↑
↖
```

转子根本没有跟上。

最终：

```text
失步
```

于是你看到：

```text
咔
抖一下
嗡——
不转
```

这就是你之前实验时非常可能遇到的情况。

---

# 7. 这其实和步进电机很像

你可以把**开环六步 BLDC**想成一个“非常简陋的步进电机”。

MCU：

> “我不管你在哪里，我每 100us 就换一步。”

电机：

> “等等，你换太快了，我跟不上！”

MCU：

> “不管，我继续换。”

电机：

> “那我不转了。”

😂

---

# 8. 所以真正的问题不是“能不能让磁场旋转”

而是：

> **你旋转得多快？**

这就是整个问题的核心。

假设：

```text
每一步 = 60°电角度
```

如果：

```text
100us换一步
```

那么电频率：

$$
f_e=\frac{1}{6\times100us}
$$

也就是：

$$
f_e=1666Hz
$$

假设电机：

```text
7极对
```

那么机械转速：

$$
n=\frac{1666}{7}\times60
$$

大约：

$$
\boxed{14280RPM}
$$

---

# 9. 你刚上电就让一个 7 极对 BLDC 去追 14000 RPM？

它当然可能直接：

```text
抖一下
然后失步
```

🤣

这就是为什么**启动不能一上来就高速切六步**。

---

# 10. 正确的开环启动应该是什么样？

比如：

```text
第一步
↓
等待 5ms

第二步
↓
等待 4.5ms

第三步
↓
等待 4ms

第四步
↓
等待 3.5ms

第五步
↓
等待 3ms
```

也就是：

> **一开始慢慢转，然后逐渐加速。**

波形大概：

```text
换相周期

5ms
 │
 │\
 │ \
 │  \
 │   \
 │    \
 │     \
 └────────→ 时间

      越来越快
```

这叫：

> **开环加速斜坡 / Open-loop ramp**

---

# 11. 这时候你的想法就真的可以跑起来了

例如：

```c
while(1)
{
    BLDC_Commutate();

    delay_us(commutation_period);

    commutation_period -= acceleration_step;
}
```

大概就是：

```text
            六步换相
               │
               ↓
        ┌─────────────┐
        │ State 0     │
        └──────┬──────┘
               ↓
        ┌─────────────┐
        │ State 1     │
        └──────┬──────┘
               ↓
        ┌─────────────┐
        │ State 2     │
        └──────┬──────┘
               ↓
              ...
               ↓
        ┌─────────────┐
        │ State 5     │
        └──────┬──────┘
               │
               └──────→ State 0
```

**这确实能够让定子磁场旋转。**

---

# 12. 那为什么还要 BEMF？

因为开环的时候你实际上是在：

> **猜。**

你说：

```text
100us以后换相
```

凭什么？

你认为：

> “电机应该已经转到这里了。”

但电机实际可能：

```text
负载变大
↓
转速下降
↓
转子跟不上
```

这时候你仍然：

```text
100us
↓
换相
100us
↓
换相
100us
↓
换相
```

于是越来越严重。

---

# 13. BEMF的作用就是：别猜了，看电机自己

开环：

```text
MCU：

我认为你应该到这里了
↓
换相
```

无感闭环：

```text
MCU：

你现在实际转到哪里了？
↓
看BEMF
↓
哦，你已经到这里了
↓
换相
```

这就是两者最大的区别。

---

# 14. 所以你可以把两种控制方式放一起看

## 开环六步

```text
MCU
 │
 │ 固定时间
 ↓
换相
 │
 ↓
电机
```

没有反馈。

所以：

```text
MCU：100us换相
MCU：100us换相
MCU：100us换相
```

---

## 无感闭环六步

```text
             ┌─────────────┐
             │             ↓
MCU → 换相 → 电机 → BEMF → ADC
             ↑             │
             └─────────────┘
```

变成：

```text
电机实际状态
     ↓
    BEMF
     ↓
    ADC
     ↓
判断转子位置
     ↓
决定换相
```

这才是：

> **Sensorless BLDC**

---

# 15. 但是你可能会问：既然我可以开环，为什么还非要闭环？

因为开环有一个致命问题：

### 负载变化

假设：

```text
空载：

目标 5000RPM
实际 5000RPM
```

突然：

```text
负载 ↑
```

电机实际：

```text
5000 → 4500 → 4000RPM
```

但你的 MCU 还是：

```text
按照5000RPM对应的换相速度
```

继续换相。

于是：

```text
定子磁场：越来越快
转子：越来越慢
```

最后：

```text
失步
```

---

# 16. BEMF就是在解决这个问题

如果转子慢下来：

$$
E=K_e\omega
$$

所以：

```text
转速下降
 ↓
BEMF下降
 ↓
BEMF变化时间改变
 ↓
检测到换相时刻变化
 ↓
MCU跟着实际转子调整
```

这就是闭环。

---

# 17. 还有一个非常容易误解的地方

你说：

> “我直接让定子励磁磁场旋转起来不就行了？”

**对 FOC 来说，你这个理解几乎完全正确。**

FOC就是：

```text
给定一个连续旋转的磁场
          ↓
        转子
          ↓
      跟着旋转
```

比如：

$$
\theta_e=1000t
$$

然后：

```text
Ud/Uq
 ↓
反Park
 ↓
Uα/Uβ
 ↓
SVPWM
 ↓
三相电压
```

磁场可以非常平滑地：

```text
0°
1°
2°
3°
4°
5°
...
```

---

# 18. 六步 BLDC 不一样

它不是连续旋转磁场。

而是：

```text
0°
 ↓
60°
 ↓
120°
 ↓
180°
 ↓
240°
 ↓
300°
```

一次跳：

$$
60^\circ
$$

所以它实际上是：

> **用六个离散的定子磁场方向，模拟旋转磁场。**

---

# 19. 所以“换相”到底是什么？

你之前可能把“换相”理解得太神秘了。

其实就是：

> **改变哪两相通电，从而改变定子合成磁场的方向。**

例如：

```text
A+ B-
```

产生一个方向的磁场。

换成：

```text
A+ C-
```

磁场方向就转了约：

$$
60^\circ
$$

再换：

```text
B+ C-
```

又转：

$$
60^\circ
$$

所以：

```text
六个开关组合
       ↓
六个磁场方向
       ↓
组合起来
       ↓
旋转磁场
```

---

# 20. 你之前实验为什么“抖一下”特别有代表性？

因为你很可能做了类似：

```c
while(1)
{
    AC();
    AB();
    BC();
    BA();
    CB();
    CA();
}
```

但是**没有给每个状态一个合适的驻留时间和加速曲线**。

甚至可能：

```c
AC();
AB();
BC();
...
```

切换得太快。

那么 MCU 相当于：

```text
磁场：

A → B → C → A → B → C
```

而转子：

```text
？？？
```

刚通电时可能产生一次转矩：

```text
啪！
```

所以：

> **你看到“抖一下”。**

随后因为转子没有建立同步：

> **失步。**

于是就不转了。

---

# 21. 你现在可以做一个非常简单的实验

甚至**完全不用 BEMF**。

先做最原始的开环六步。

例如你的电机是：

```text
7极对
```

先：

```text
duty = 10%
```

然后：

```text
State 0
等待 10ms

State 1
等待 10ms

State 2
等待 10ms

State 3
等待 10ms

State 4
等待 10ms

State 5
等待 10ms
```

如果方向正确：

> 电机应该慢慢转。

然后逐渐：

```text
10ms
9ms
8ms
7ms
6ms
...
```

你会发现：

```text
换相越来越快
 ↓
电机越来越快
```

直到某个速度开始：

```text
嗡嗡响
抖动
失步
```

这个实验非常有价值。

因为它能让你**亲眼看到“开环六步”的极限在哪里。**

---

# 22. 然后你就会突然理解为什么需要 BEMF

开环：

```text
           MCU自己规定
               ↓
       ┌───固定换相周期───┐
       ↓                  │
     换相 → 电机 → 转子     │
       ↑                  │
       └──────────────────┘
```

无感：

```text
                 BEMF
                  ↑
                  │
MCU → 换相 → 电机 → 转子
 ↑                │
 └────实际位置────┘
```

**核心区别就一句：**

> 开环是“我觉得你应该转到这里了，所以我换相”；
> 无感闭环是“我检测到你真的转到这里了，所以我换相”。**

---

# 23. 最后再纠正一个特别关键的概念

**“检测过零点”并不是无感 BLDC 为了让磁场旋转才存在的。**

不是：

```text
检测过零
 ↓
才能产生旋转磁场
```

而是：

```text
六步换相本身就能产生旋转磁场

       ↓

但MCU不知道什么时候换下一步

       ↓

所以利用BEMF判断转子实际位置

       ↓

决定什么时候换下一步
```

所以：

$$
\boxed{
\text{换相}=\text{让定子磁场转起来}
}
$$

而：

$$
\boxed{
\text{BEMF检测}=\text{告诉MCU什么时候该换相}
}
$$

这两个概念一定要分开。

**你现在真正卡住的点，其实就是把“产生旋转磁场”和“决定什么时候改变磁场方向”混成了一件事。** 一旦把这两个东西拆开，整个无感六步就一下子清楚了。


---------
--------
# 为啥需要转子磁铁的位置？？因为励磁磁场转的太快转子磁铁跟不上

对，你这个直觉**其实是对的**。

你说：

> **“我根本不需要知道转子在哪里，我只要让定子磁场一直旋转，转子自己跟着跑不就行了吗？”**

**完全可以。**

这就是**开环启动**的基本思想。

真正需要纠正的是：**不是“必须知道转子位置才能启动”，而是“如果你不知道转子位置，就不能随便让旋转磁场一开始就转得太快”。**

---

## 关键就在这里

假设你制造一个旋转磁场：

```text
定子磁场：

     ↑
     │
     │
     N
```

转子：

```text
      S
      │
      │
      N
```

现在你把定子磁场转一点：

```text
        ↗
       /
      /
```

转子会受到电磁转矩：

```text
磁场旋转
   ↓
产生转矩
   ↓
转子跟随
```

所以你说的：

> **“让定子磁场转起来，转子自然跟着转”**

完全成立。

---

# 但是有一个隐藏条件：转子必须“跟得上”

这个才是核心。

想象你手里拿一个磁铁：

```text
转子 = 小磁铁

      N────S
```

然后你拿另外一个磁铁在它前面慢慢转：

```text
        N
       ↗
      ↗
     ↗

   N────S
```

小磁铁会跟着转。

但是如果你突然：

```text
磁场：

0°
 ↓
180°
```

你让前面的磁场瞬间跳了 180°。

小磁铁不会瞬移过去。

于是：

```text
定子磁场：180°
转子：     20°
```

它们之间的相对角度太大，就可能出现：

> **跟丢了。**

---

# 所以真正的问题不是“知道不知道位置”

而是：

> **你给定子磁场的旋转速度是多少？**

---

## 情况一：慢慢旋转

例如：

```text
0°
10°
20°
30°
40°
50°
60°
...
```

转子：

```text
0°
8°
18°
28°
38°
48°
58°
...
```

很好。

```text
定子磁场 → → → → →
转子     → → → → →
```

转子基本跟着走。

---

# 情况二：转得太快

你：

```text
0°
 ↓
60°
 ↓
120°
 ↓
180°
 ↓
240°
```

转子：

```text
0°
 ↓
15°
 ↓
30°
 ↓
45°
 ↓
60°
```

于是：

```text
定子磁场：240°
转子：      60°
```

差了：

$$
180^\circ
$$

这时候就可能：

```text
失步
↓
抖动
↓
停转
```

---

# 所以你之前那个实验为什么会失败？

你之前做：

```text
AC
AB
BC
BA
CB
CA
AC
AB
...
```

**这个方向完全没问题。**

真正的问题很可能是：

```text
AC
↓
马上 AB
↓
马上 BC
↓
马上 BA
```

你实际上是在告诉电机：

> “赶紧跟着我！”

但是电机：

> “我还没转过去呢……”

😂

---

# 这就产生了一个非常重要的区别

### 开环六步

你完全**不需要知道转子位置**。

你可以：

```text
State 0
 ↓
等待
 ↓
State 1
 ↓
等待
 ↓
State 2
 ↓
等待
 ↓
State 3
...
```

这就是：

$$
\boxed{\text{Open-loop 6-step}}
$$

---

### 无感六步

则是：

```text
State 0
 ↓
等电机实际运动
 ↓
检测BEMF
 ↓
发现该换相了
 ↓
State 1
 ↓
检测BEMF
 ↓
发现该换相了
 ↓
State 2
```

这就是：

$$
\boxed{\text{Sensorless 6-step}}
$$

---

# 那为什么实际产品更喜欢后者？

因为开环最大的问题是：

## 你不知道电机实际转速

比如你规定：

```text
每1ms换一次
```

那是你的**命令速度**。

但是电机实际可能：

```text
负载小：

命令 → 5000 RPM
实际 → 4900 RPM
```

还可以。

突然：

```text
负载增加
```

实际变成：

```text
5000 → 4000 RPM
```

可是你还在：

```text
每1ms换相
```

于是：

```text
定子磁场
5000RPM
   ↓
   ↓
   ↓

转子
4000RPM
   ↓
   ↓
```

慢慢就跟不上了。

---

# BEMF就是用来解决这个问题

BEMF本质上提供了一个非常有价值的信息：

$$
\boxed{E \propto \omega}
$$

也就是：

> **反电动势和转速有关。**

而且BEMF的波形变化，还能告诉你转子处于哪个换相区间。

于是：

```text
          电机
           │
           ↓
         BEMF
           │
           ↓
          ADC
           │
           ↓
      判断换相时机
           │
           ↓
        下一状态
```

这样就不用猜。

---

# 但是！这里还有一个特别重要的点

你现在实际上已经碰到：

> **BLDC 六步控制和 FOC 的根本区别。**

---

## 六步 BLDC

定子磁场不是连续旋转的。

而是：

```text
     ①
    ↗
② ←     → ⑥

③         ⑤
    ↘
     ④
```

大概只有：

$$
6
$$

个固定方向。

所以：

```text
A+B-
 ↓
A+C-
 ↓
B+C-
 ↓
B+A-
 ↓
C+A-
 ↓
C+B-
```

---

## FOC

则是真的可以做到：

```text
0°
1°
2°
3°
4°
5°
6°
...
359°
```

连续旋转。

所以你说：

> **“我直接让定子励磁磁场旋转起来不就行了吗？”**

这其实就是**FOC的思路**。

FOC：

```text
我要一个 θ

↓
sin/cos

↓
SVPWM

↓
产生连续旋转磁场
```

---

# 所以你可以这样理解

### 六步 BLDC

```text
我要让磁场转

↓

我只有6个磁场方向

↓

0 → 60 → 120 → 180 → 240 → 300

↓

转子跟着走
```

### FOC

```text
我要让磁场转

↓

我可以产生任意角度的磁场

↓

0 → 1 → 2 → 3 → 4 → 5 → ...

↓

转子跟着走
```

---

# 最关键的一句话

你现在的理解可以改成：

> **“启动时确实可以完全不知道转子位置，直接用开环的方法让定子磁场旋转，转子靠电磁转矩跟随它。”**

但是：

> **“如果磁场旋转得太快，转子跟不上，就失步。”**

所以实际无感 BLDC 一般是：

```text
                启动阶段
                   │
                   ↓
          不知道转子位置
                   │
                   ↓
          开环强制旋转磁场
                   │
                   ↓
             电机转起来
                   │
                   ↓
          BEMF变得足够明显
                   │
                   ↓
            开始检测BEMF
                   │
                   ↓
          得到实际换相时机
                   │
                   ↓
            切入无感闭环
                   │
                   ↓
              正常运行
```

所以**BEMF不是为了“让电机旋转”而存在的**。

而是：

> **电机已经能旋转以后，拿 BEMF 来判断“下一次什么时候换相”。**

这下你就应该能把你贴的 TI `ISBLDC` 代码对应上了：**它压根不是在“产生旋转磁场”，它主要干的事情就是——检测当前悬空相的电压 → 积分 BEMF → 判断该换相 → 发出 `commTrigFlag` → 上层六步状态机切换到下一组 MOSFET。**



-----------
-------------
-----------


你这个懵其实**非常正常**。因为“无感六步 BLDC”最容易被各种“过零点、反电动势、悬空相、换相提前角、积分”这些词绕进去。

你先**完全忘掉代码**，甚至先忘掉 TI 的 `ISBLDC`。

我用一个最核心的问题给你讲：

> **没有霍尔、没有编码器，我怎么知道转子现在转到哪了？**

答案就是：

> **转子在转 → 电机自己产生反电动势 BEMF → 从悬空的那一相电压里观察这个 BEMF → 根据 BEMF 的变化推断转子位置 → 到时间就换相。**

这就是无感方波控制的灵魂。

---

# 1. 先理解有感六步控制

假设一个 BLDC 有：

```text
A相
B相
C相
```

转子在里面：

```text
        N   S
      ┌───────┐
      │ 转  子 │
      └───────┘

    A       B       C
```

如果你有霍尔传感器：

```text
转子位置
   ↓
Hall
   ↓
000 / 001 / 010 / ...
   ↓
告诉MCU：
“现在应该State 3”
   ↓
六步换相
```

所以有感控制非常简单：

> **霍尔告诉你什么时候换相。**

---

# 2. 无感控制的问题来了

现在把霍尔拆掉。

MCU突然变成：

```text
MCU：

？？？？？

转子到底在哪？
什么时候换相？
```

但是电机有一个非常牛逼的特性：

> **只要转子在转，电机就是一个发电机。**

也就是：

$$
\boxed{\text{转子运动}\rightarrow\text{产生反电动势}}
$$

这个反电动势就是：

$$
BEMF
$$

---

# 3. BEMF到底是什么？

这是理解整个无感控制最关键的一步。

假设转子磁铁：

```text
      N       S
    ┌───────────┐
    │   转子    │
    └───────────┘
```

它在旋转。

定子线圈：

```text
A线圈
B线圈
C线圈
```

磁铁不断扫过线圈。

根据电磁感应：

$$
e=-\frac{d\lambda}{dt}
$$

也就是：

> **磁通变化 → 线圈产生电压。**

所以：

```text
转子旋转
   ↓
磁通变化
   ↓
线圈产生电压
   ↓
BEMF
```

---

# 4. 最关键的一点：六步控制永远有一相“悬空”

比如：

```text
State 0

A：+
B：-
C：悬空
```

也就是：

```text
        VBUS
         │
       MOS
         │
         A
         │
       电机
         │
         B
         │
       MOS
         │
        GND

C ───────────── MCU ADC
```

这时候：

> A、B 两相在驱动电机。

而：

> **C 相没有驱动。**

但是 C 相线圈里面仍然有转子磁场经过。

所以 C 相自己会“发电”。

于是：

```text
C相悬空
   ↓
转子旋转
   ↓
C线圈产生BEMF
   ↓
ADC测C相电压
```

这就是无感检测的入口。

---

# 5. 所以“无感”其实并不是完全不测东西

很多人第一次学会误解成：

> “无感 = 什么都不测？”

不是。

无感 BLDC 仍然需要：

```text
ADC
 ↓
测三相电压
```

只不过它：

```text
没有霍尔
没有编码器
没有位置传感器
```

而是：

```text
利用电机自身产生的BEMF
```

来“间接”获得位置。

---

# 6. 为什么偏偏测悬空相？

因为驱动相上的电压：

```text
A = MCU主动给的
B = MCU主动给的
```

你根本分不清：

```text
这是PWM造成的？
还是电机BEMF造成的？
```

但是：

```text
C = 没有驱动
```

所以 C 相上的变化主要反映：

> **转子磁场产生的反电动势。**

因此六步 BLDC 天然适合无感检测。

---

# 7. 现在进入最重要的“过零点”

假设：

```text
A+
B-
C悬空
```

C 相 BEMF 随转子转动变化：

```text
BEMF

 +          /\
 |         /  \
 |        /    \
0|-------/------\--------
 |      /        \
 |     /          \
 -
 
       转子位置 →
```

它会从：

```text
负
 ↓
0
 ↓
正
```

或者：

```text
正
 ↓
0
 ↓
负
```

这里的：

```text
0
```

就是：

> **BEMF过零点。**

---

# 8. 为什么过零点能告诉我们转子位置？

因为 BEMF 本质上是：

$$
e \propto \omega \cdot \Phi(\theta)
$$

它跟：

```text
转子位置 θ
```

直接相关。

所以：

```text
BEMF波形
    ↓
找到某个特征点
    ↓
知道转子到了某个特定位置
```

其中最常用的就是：

> **BEMF Zero Crossing**

也就是过零点。

---

# 9. 但这里有个特别重要的坑

**过零点 ≠ 立刻换相。**

这是很多教程最容易讲得模糊的地方。

假设：

```text
State 0
A+
B-
C悬空
```

C 相 BEMF：

```text
        /
       /
------/----------  ← 过零点
     /
    /
```

检测到：

```text
BEMF = 0
```

通常：

> **再等 30°电角度**

然后才换相。

因为：

```text
一个六步区间 = 60°
```

而：

```text
BEMF过零点
```

通常位于这个区间的中间附近。

所以：

```text
换相
 ↓
30°
 ↓
BEMF过零
 ↓
30°
 ↓
下一次换相
```

这才是经典无感六步的核心节奏。

---

# 10. 为什么 TI 没有直接检测过零点？

这就是你贴的代码看起来特别奇怪的地方。

你可能发现它根本没写：

```c
if(Vbemf > 0)
```

然后：

```c
if(Vbemf < 0)
```

直接判断过零。

而是：

```c
bemfInt += VintPhase;
```

然后：

```c
if(bemfInt > threshold)
{
    commTrigger = true;
}
```

为什么？

因为 TI 用的是：

> **BEMF 积分法。**

---

# 11. 先理解“过零检测法”

传统方法：

```text
Vbemf
  │
  │       /
  │      /
0 ├-----/---------
  │    /
  │   /
  │
  └──────────────→
```

MCU不断检测：

```c
if(Vbemf > 0)
```

和：

```c
if(Vbemf < 0)
```

一旦：

```text
正 → 负
```

或者：

```text
负 → 正
```

就知道：

> BEMF过零了。

然后：

```text
延迟30°
```

再换相。

---

# 12. 但是实际电机没这么干净

真实世界：

```text
理想BEMF：

       /\
      /  \
-----/----\-----

实际ADC：

  ~~/\_/\/\~~
     ↑
   PWM噪声
```

你会遇到：

```text
PWM开关噪声
MOSFET尖峰
二极管反向恢复
采样噪声
母线纹波
死区
寄生参数
```

于是你可能检测到：

```text
+0.02V
-0.01V
+0.03V
-0.02V
```

MCU：

```text
卧槽？

过零了？

又过零了？

又过零了？
```

换相直接乱掉。

---

# 13. TI 的思路非常有意思

它不问：

> “BEMF有没有刚好过零？”

而是问：

> **“这一段 BEMF 积累了多少电压-时间面积？”**

数学上：

$$
\boxed{
I=\int V_{bemf}dt
}
$$

也就是：

```text
电压
 ×
 时间
```

---

# 14. 举个非常简单的例子

假设：

```text
BEMF = 1V
```

持续：

```text
1ms
```

那么：

$$
\int Vdt
=
1\times0.001
=
0.001Vs
$$

如果：

```text
BEMF = 2V
```

持续：

```text
1ms
```

那么：

$$
0.002Vs
$$

所以：

> **BEMF越大，积分增长越快。**

电机越快：

```text
BEMF越大
 ↓
积分越快
 ↓
更快达到threshold
 ↓
更快换相
```

这就是 TI 这套东西非常核心的思想。

---

# 15. 把 TI 的代码翻译成人话

例如：

```c
obj->bemfInt += obj->VintPhase;
```

翻译：

> **把当前悬空相的BEMF一点一点累积起来。**

然后：

```c
if(obj->bemfInt > obj->thresholdInt)
```

翻译：

> **累计到足够多了。**

然后：

```c
obj->commTrigFlag = true;
```

翻译：

> **好了，该换相了。**

---

# 16. 为什么还要判断正负？

你代码里面有：

```c
if(obj->bemfDirect == DIR_POS)
```

或者：

```c
DIR_NEG
```

这其实是在说：

> **当前这个六步状态，我只接受某一个方向的BEMF。**

例如：

```text
State 1

B相悬空

BEMF应该：
正向变化
```

那么：

```c
if(Vbemf > 0)
{
    bemfInt += Vbemf;
}
else
{
    bemfInt = 0;
}
```

如果当前应该检测负方向：

```c
if(Vbemf < 0)
{
    bemfInt += Vbemf;
}
else
{
    bemfInt = 0;
}
```

所以：

> **`bemfDirect` 本质就是告诉积分器：这一步应该看正BEMF还是负BEMF。**

---

# 17. 那 `bemfLockFlag` 又是什么鬼？

这个也很简单。

假设：

```text
BEMF已经达到threshold
       ↓
触发换相
```

如果不锁：

```text
下一次ADC
 ↓
BEMF仍然是正的
 ↓
继续积分
 ↓
又达到threshold
 ↓
又换相
```

就可能连续误触发。

所以 TI：

```c
bemfLockFlag = true;
```

意思就是：

> **我刚刚换过相了，先别急着再次触发。**

然后等待 BEMF 方向发生变化：

```c
if(Vbemf < 0)
{
    bemfLockFlag = false;
}
```

重新允许积分。

所以：

```text
换相
 ↓
LOCK
 ↓
等待BEMF反向
 ↓
UNLOCK
 ↓
重新积分
 ↓
达到threshold
 ↓
换相
```

---

# 18. 到这里，你应该能看懂 TI 的整个 `ISBLDC_run()`

它实际上就是：

```text
① 当前六步状态
       ↓
② 知道应该检测 A/B/C 哪一相
       ↓
③ 知道应该检测正BEMF还是负BEMF
       ↓
④ 从ADC得到三相电压
       ↓
⑤ 计算相对中性点电压
       ↓
⑥ 拿出悬空相
       ↓
⑦ 对BEMF进行积分
       ↓
⑧ 积分达到阈值
       ↓
⑨ commTrigFlag = true
       ↓
⑩ 外部执行下一步换相
```

---

# 19. 你可以把它想成“盲人摸路”

这个比喻我觉得特别适合理解无感 BLDC。

有感：

```text
转子
 ↓
霍尔
 ↓
直接告诉你：
“现在在这里”
```

无感：

```text
转子
 ↓
产生BEMF
 ↓
MCU观察BEMF
 ↓
推断：
“哦，转子应该到这里了”
```

所以它不是：

> **直接测转子位置。**

而是：

> **通过电机自身产生的电压变化，间接推断转子位置。**

---

# 20. 那为什么一定要“先启动”？

这又是无感 BLDC 的另一个关键。

电机没转：

```text
ω = 0
```

BEMF：

$$
E=K_e\omega
$$

所以：

$$
\boxed{E=0}
$$

也就是说：

```text
电机静止
 ↓
没有BEMF
 ↓
ADC什么都检测不到
 ↓
不知道转子在哪里
```

这就是：

> **无感 BLDC 的先天缺陷。**

所以必须：

```text
先强行转起来
       ↓
转子开始旋转
       ↓
产生BEMF
       ↓
BEMF足够大
       ↓
切换无感闭环
```

---

# 21. 所以完整过程其实只有三个阶段

### 第一阶段：定位

```text
固定某一个六步状态
       ↓
给一点电流
       ↓
把转子吸到已知位置
```

---

### 第二阶段：强拖

```text
MCU不知道转子在哪

所以：

State0
 ↓
等一段时间
 ↓
State1
 ↓
等一段时间
 ↓
State2
 ↓
等一段时间
 ↓
State3
 ↓
...
```

这叫：

> **开环强制换相 / Open-loop ramp**

电机被 MCU “拖着”转。

---

### 第三阶段：BEMF闭环

转起来以后：

```text
BEMF出现
 ↓
ADC检测
 ↓
判断悬空相
 ↓
BEMF积分
 ↓
达到threshold
 ↓
换相
 ↓
继续检测
```

此时 MCU 不再自己决定：

```text
“100us后换相”
```

而变成：

```text
“BEMF告诉我该换相了”
```

这就是**无感闭环**。

---

# 22. 最后把整个东西压缩成一句话

你现在只要记住这一句话：

> **无感六步 BLDC = 用两相驱动电机、留一相悬空，然后通过悬空相产生的反电动势 BEMF 来判断转子转到了哪里，再根据这个位置不断进行六步换相。**

而 TI 的增强版：

> **不是简单检测 BEMF 过零，而是对悬空相 BEMF 做“电压-时间积分”，积分达到阈值就产生换相触发，同时通过连续换相之间的时间计算转速。**

所以你贴的 TI 代码，本质上根本没有那么复杂：

```text
                 六步状态
                    │
                    ▼
              哪一相悬空？
                    │
                    ▼
              ADC测悬空相
                    │
                    ▼
                 BEMF
                    │
                    ▼
             正/负方向判断
                    │
                    ▼
                BEMF积分
                    │
                    ▼
           积分达到 Threshold？
                 │       │
                NO      YES
                 │       │
                 │       ▼
                 │      换相
                 │       │
                 │       ▼
                 └──←下一状态
```

**你现在如果把这个框架真正搞明白，TI 这份 `ISBLDC.c` 基本就已经读懂 80% 了。**

而且这和你前面一直研究的 **FOC + SMO + PLL** 是完全不同的思路：

```text
无感六步：
BEMF → 换相时刻 → 六步

无感FOC：
BEMF/电流 → SMO → 估算角度 → PLL → 连续电角度 → Park/FOC
```

前者其实要简单很多。



-----------
-------------

你这份 TI Motor SDK 里的 `ISBLDC`，**本质上不是 FOC 的 SMO/PLL**，而是一套专门给 **无感六步 BLDC** 用的 **反电动势积分换相检测器（BEMF Integrator）**。

它和你最近研究的 **SMO + PLL 无感 FOC** 属于两个不同路线。

我先不急着逐行解释代码，先把它背后的物理原理讲透，然后再把代码对应上。这样你会非常容易看懂。

---

# 一、先搞清楚：这套东西到底在干什么？

一句话：

> **它不计算连续电角度，而是在判断“现在是不是该进行下一次六步换相”。**

所以它最终输出的核心东西不是：

```c
theta_e
```

而是：

```c
commTrigFlag
```

也就是：

```text
        电机转起来
             ↓
       检测反电动势
             ↓
       判断 BEMF 是否
       积累到指定程度
             ↓
      commTrigFlag = true
             ↓
        六步换相
             ↓
        继续检测
```

所以它更准确地说是：

> **无感六步换相位置/时序估计器**

而不是 FOC 里面那种连续位置观测器。

---

# 二、它需要哪些基础知识？

理解这份代码，实际上只需要搞懂 5 个东西：

1. BLDC 六步换相
2. 反电动势 BEMF
3. 悬空相
4. 反电动势过零点
5. 为什么 TI 不直接检测过零，而是做“积分”

我们一个个来看。

---

# 三、BLDC 六步换相到底是什么？

假设三相：

```text
A
B
C
```

六步 BLDC 的典型驱动方式：

| Comm State | 导通    | 悬空 |
| ---------- | ----- | -- |
| 0          | A+ B- | C  |
| 1          | A+ C- | B  |
| 2          | B+ C- | A  |
| 3          | B+ A- | C  |
| 4          | C+ A- | B  |
| 5          | C+ B- | A  |

然后：

```text
0 → 1 → 2 → 3 → 4 → 5 → 0
```

不断循环。

这里最重要的是：

> **每一个状态都有一相是悬空的。**

比如：

```text
State 0

A → PWM
B → LOW
C → 悬空
```

这时候 C 相没有被 MOSFET 强制驱动。

所以 C 相电压里面就能观察到：

> **转子磁场产生的反电动势。**

这就是无感检测的关键。

---

# 四、什么叫反电动势 BEMF？

BLDC 电机转起来以后：

```text
转子磁铁
   ↓
切割定子磁场
   ↓
线圈产生感应电压
   ↓
反电动势 BEMF
```

最简单可以理解成：

$$
E=K_e\omega
$$

也就是：

> **转得越快，反电动势越大。**

例如：

```text
100 RPM
    ↓
BEMF 很小

1000 RPM
    ↓
BEMF 变大

5000 RPM
    ↓
BEMF 更大
```

所以：

> **没有位置传感器，也可以通过 BEMF 推测转子位置。**

---

# 五、为什么可以通过悬空相检测位置？

这是无感六步 BLDC 最经典的东西。

假设：

```text
A+ 
B-
C 悬空
```

C 相没有主动驱动。

那么 C 相电压主要体现：

$$
V_C \approx E_C
$$

当然真实系统里还会受到：

```text
PWM
MOSFET
二极管
寄生参数
电阻压降
母线电压
采样偏置
```

影响。

但总体上：

> **悬空相电压包含转子产生的反电动势信息。**

---

# 六、最传统的方法：BEMF 过零检测

传统无感 BLDC 很喜欢这么干：

```text
悬空相 BEMF
      │
      ▼
    过零点
      │
      ▼
   延迟 30°
      │
      ▼
    换相
```

为什么要延迟 30°？

因为：

> **反电动势过零点通常对应当前 60°换相区间的中点附近。**

所以：

```text
过零点
  ↓
再等半个 60°区间
  ↓
30°
  ↓
换相
```

这就是经典：

> **BEMF Zero Crossing + 30° Delay**

---

# 七、但是 TI 这份代码没有直接找过零

这是你这份代码最值得研究的地方。

它没有：

```c
if(Vbemf > 0)
```

然后直接判断。

它干的是：

$$
\boxed{
\int E\,dt
}
$$

也就是：

> **反电动势积分。**

---

# 八、为什么积分反电动势？

假设反电动势是：

```text
        /\
       /  \
------/----\------
     /      \
```

传统过零检测非常依赖：

```text
     ↓
-----0-----
     ↑
   过零
```

但是实际 MCU ADC 看到的波形可能是：

```text
噪声
PWM尖峰
采样误差
母线波动
电压偏置
```

导致过零点抖动：

```text
理论：

--------0--------

实际：

-----\_/\_0/\/\------
```

那么直接过零检测容易抖。

---

# 九、积分相当于干了什么？

假设：

$$
E(t)>0
$$

那么：

$$
\int E(t)dt
$$

会越来越大。

例如：

```text
BEMF：

  2V
  │
  │       ┌──────
  │      /
  │     /
  │____/
  │
  └────────────── t

积分：

  │
  │          /
  │        /
  │      /
  │    /
  │___/
  └────────────── t
```

于是 TI 不问：

> “BEMF 有没有刚好过零？”

而是问：

> **“BEMF 的正向伏秒面积有没有积累到足够大？”**

也就是：

$$
\boxed{
\int E\,dt > Threshold
}
$$

满足以后：

```c
commTrigFlag = true;
```

然后换相。

---

# 十、这就是这套算法的核心灵魂

可以把它浓缩成一句话：

> **根据当前六步状态，选择正确的悬空相和正确的 BEMF 极性，只对正确方向的反电动势进行积分，当积分面积达到阈值时触发换相。**

这句话基本就是整个 `ISBLDC` 的灵魂。

---

# 十一、现在回来看你的代码

先看这个结构：

```c
typedef struct ISBLDC_Obj
{
    MATH_Vec3 Vabcg;
    MATH_Vec3 VabcOffset;
    MATH_Vec3 Vabcn;

    float32_t VintPhase;

    float32_t thresholdMin;
    float32_t thresholdMax;
    float32_t thresholdSF;
    float32_t thresholdInt;

    float32_t bemfInt;

    float32_t speedScaler;
    float32_t speedInt_Hz;

    uint32_t timeStamp;
    uint32_t timeStampBuf[6];
    uint32_t timePeriod;

    uint32_t intTimer;

    uint16_t commState;

    ISBLDC_PHS_e bemfPhase;
    ISBLDC_DIR_e bemfDirect;

    bool bemfLockFlag;
    bool commTrigFlag;

} ISBLDC_Obj;
```

这些变量基本可以分成四类。

---

# 十二、第一类：电压

```c
Vabcg
VabcOffset
Vabcn
VintPhase
```

对应：

```text
ADC/电压采样
     ↓
去 Offset
     ↓
得到三相对地电压
     ↓
转换成三相对中性点电压
     ↓
选择悬空相
```

---

# 十三、为什么代码要计算 `Vabcn`？

这里：

```c
obj->Vabcn.value[0] =
    obj->Vabcg.value[0]
    - ((obj->Vabcg.value[1] + obj->Vabcg.value[2]) * 0.5f);
```

其实就是：

$$
V_{AN}
=
V_{AG}
-\frac{V_{BG}+V_{CG}}{2}
$$

同理：

$$
V_{BN}
=
V_{BG}
-\frac{V_{AG}+V_{CG}}{2}
$$

$$
V_{CN}
=
V_{CG}
-\frac{V_{AG}+V_{BG}}{2}
$$

为什么这么干？

因为 MCU 测到的往往是：

```text
A 对 GND
B 对 GND
C 对 GND
```

但是我们真正想知道的是：

```text
A 对电机中性点
B 对电机中性点
C 对电机中性点
```

所以要做一次虚拟中性点计算。

---

# 十四、这个公式其实非常重要

例如：

```text
Va = 8V
Vb = 0V
Vc = 0V
```

那么：

$$
V_{AN}
=
8-\frac{0+0}{2}
=8V
$$

而：

$$
V_{BN}
=
0-\frac{8+0}{2}
=-4V
$$

$$
V_{CN}
=
0-\frac{8+0}{2}
=-4V
$$

所以代码就获得了：

```text
A：+8V
B：-4V
C：-4V
```

这样的三相对中性点电压。

---

# 十五、第二类：选择当前要检测哪一相

这一段是整个算法非常核心的部分：

```c
switch(obj->commState)
{
    case 0:
        obj->bemfPhase = PHS_C;
        obj->bemfDirect = DIR_NEG;
        break;

    case 1:
        obj->bemfPhase = PHS_B;
        obj->bemfDirect = DIR_POS;
        break;

    case 2:
        obj->bemfPhase = PHS_A;
        obj->bemfDirect = DIR_NEG;
        break;

    case 3:
        obj->bemfPhase = PHS_C;
        obj->bemfDirect = DIR_POS;
        break;

    case 4:
        obj->bemfPhase = PHS_B;
        obj->bemfDirect = DIR_NEG;
        break;

    case 5:
        obj->bemfPhase = PHS_A;
        obj->bemfDirect = DIR_POS;
        break;
}
```

你可以把它整理成表：

| CommState | 检测相 | BEMF方向 |
| --------: | --- | ------ |
|         0 | C   | 负      |
|         1 | B   | 正      |
|         2 | A   | 负      |
|         3 | C   | 正      |
|         4 | B   | 负      |
|         5 | A   | 正      |

这就是：

> **六步换相状态 → 指定悬空相 → 指定 BEMF 极性**

---

# 十六、然后真正选择电压

```c
switch(obj->bemfPhase)
{
    case PHS_A:
        obj->VintPhase = obj->Vabcn.value[0];
        break;

    case PHS_B:
        obj->VintPhase = obj->Vabcn.value[1];
        break;

    case PHS_C:
        obj->VintPhase = obj->Vabcn.value[2];
        break;
}
```

实际上就是：

```text
State 0
   ↓
检测 C 相

State 1
   ↓
检测 B 相

State 2
   ↓
检测 A 相

...
```

最终：

```c
VintPhase
```

就是：

> **当前状态下用于 BEMF 判断的那一相电压。**

---

# 十七、第三类：BEMF 积分

核心来了。

## 正向积分

```c
obj->bemfInt += obj->VintPhase;
```

然后：

```c
if(obj->bemfInt > obj->thresholdInt)
```

触发：

```c
obj->commTrigFlag = true;
```

数学上就是：

$$
I_E[k]
=
I_E[k-1]+V_{bemf}[k]
$$

严格来说如果采样周期为：

$$
T_s
$$

应该是：

$$
I_E[k]
=
I_E[k-1]
+
V_{bemf}[k]T_s
$$

不过 TI 这里把采样周期等比例吸收到阈值/参数体系里面去了。

---

# 十八、负向积分

```c
obj->bemfInt += obj->VintPhase;

if(obj->bemfInt < 0.0f)
{
    if(obj->bemfInt < -obj->thresholdInt)
    {
        ...
        obj->commTrigFlag = true;
    }
}
```

也就是：

$$
\boxed{
\int E\,dt < -Threshold
}
$$

才触发。

所以：

```text
DIR_POS

      BEMF
       ↓
 + + + + + + 
       ↓
    积分增加
       ↓
 threshold
       ↓
     换相
```

而：

```text
DIR_NEG

      BEMF
       ↓
 - - - - - -
       ↓
    积分减少
       ↓
 -threshold
       ↓
     换相
```

---

# 十九、为什么只允许正确方向积分？

看：

```c
if(obj->bemfInt > 0.0f)
```

否则：

```c
obj->bemfInt = 0.0f;
```

这意味着：

> 如果当前应该检测正 BEMF，但实际检测到负电压，那么积分器立即清零。

例如：

```text
期望：

+ + + + + + +

实际：

+ + + - - + +
      ↑
      清零
```

这相当于一种很简单的：

> **方向判别 + 积分门控**

---

# 二十、这其实非常像“面积触发器”

你可以把 `bemfInt` 想成一个桶：

```text
          Threshold
       ───────────────
              ↑
              │
          ┌───────┐
          │       │
          │ BEMF  │
          │ 积分  │
          │       │
          └───────┘
              ↑
           Vbemf
```

不断往里面装：

```text
BEMF × 时间
```

装满：

```text
commTrigFlag = true
```

然后：

```c
bemfInt = 0;
```

重新开始。

这就是这个算法非常形象的理解方式。

---

# 二十一、那么 `bemfLockFlag` 又是干什么的？

这个变量非常值得注意：

```c
bool bemfLockFlag;
```

它相当于：

> **换相触发之后的“锁定/消隐状态”。**

触发：

```c
commTrigFlag = true;
bemfLockFlag = true;
```

之后，并不会马上再次积分。

比如正向：

```c
if(obj->bemfLockFlag == true)
{
    if(obj->VintPhase < 0.0f)
    {
        obj->bemfLockFlag = false;
    }
}
```

意思是：

> 触发一次以后，必须等 BEMF 真正反向，再解除锁定。

---

# 二十二、为什么需要这个锁？

否则可能出现：

```text
BEMF
       /\
      /  \
─────/    \─────
       ↑
     threshold
       ↑
    触发换相

但这一帧之后
bemfInt 又快速超过 threshold

       ↑
   再触发一次
```

于是：

```text
一次 BEMF 波形
     ↓
换相
换相
换相
换相
```

电机直接乱套。

所以：

```text
commTrigFlag
      ↓
bemfLockFlag = true
      ↓
等待 BEMF 反向
      ↓
解锁
      ↓
允许下一次积分
```

本质就是一个：

> **边沿/周期锁存机制。**

---

# 二十三、还有一个非常有意思的东西：Flyback Lockout

代码注释：

```c
// Flyback voltage lockout flag
```

也就是说 TI 很清楚：

> **MOSFET 换相瞬间会产生飞返电压/续流尖峰。**

所以刚换相之后：

```text
MOSFET切换
   ↓
绕组电流变化
   ↓
寄生参数
   ↓
Flyback / ringing
   ↓
ADC看到巨大尖峰
```

如果马上拿这个电压判断 BEMF：

```text
误触发
```

所以 `bemfLockFlag` 也起到一种：

> **换相后消隐/抗假触发**

的作用。

---

# 二十四、然后就是速度计算

这部分非常简单。

它实际上没有通过：

```text
BEMF幅值
```

计算速度。

而是通过：

> **两次换相之间的时间**

计算速度。

---

# 二十五、`timeStamp` 是什么？

```c
obj->timeStamp++;
```

假设：

```text
ISBLDC_run()
```

每：

```text
50 us
```

执行一次。

那么：

```text
timeStamp = 1
```

就是：

$$
50us
$$

而：

```text
timeStamp = 100
```

就是：

$$
100\times50us=5ms
$$

---

# 二十六、换相发生的时候记录时间

```c
if(obj->commTrigFlag == true)
{
    obj->timeStampBuf[0] = obj->timeStamp;
```

所以：

```text
换相1
 ↓
记录时间

换相2
 ↓
记录时间

换相3
 ↓
记录时间
```

---

# 二十七、为什么不是只用一次换相周期？

TI 这里用了：

```c
timeStampBuf[0]
timeStampBuf[1]
timeStampBuf[2]
timeStampBuf[3]
timeStampBuf[4]
```

然后：

```c
timePeriod =
    timeStamp
    + buf0
    + buf1
    + buf2
    + buf3
    + buf4;
```

实际上是在做：

> **多次换相周期平均。**

这样速度不会因为一次换相时间抖动而剧烈变化。

---

# 二十八、速度公式

然后：

```c
obj->speedInt_Hz =
    obj->speedScaler /
    ((float32_t)obj->timePeriod);
```

其中：

```c
speedScaler = ctrlFreq_Hz;
```

假设：

```text
控制频率 = 20 kHz
```

那么：

```c
speedScaler = 20000
```

如果 6 个换相区间总共用了：

```text
1000 个控制周期
```

那么：

$$
f=\frac{20000}{1000}=20Hz
$$

但是这里的 `Hz` **到底代表机械转速还是电气频率，要结合 TI 后续的 commutation/极对数定义看**。从这段代码本身，它是在根据六步换相事件间隔计算周期频率，不能简单把它直接当 RPM。

---

# 二十九、`thresholdInt` 是这套算法最关键的参数

你看：

```c
if(obj->bemfInt > obj->thresholdInt)
```

所以：

```text
thresholdInt
```

决定：

> **什么时候认为“BEMF 积分够了，可以换相”。**

阈值太小：

```text
很容易达到
↓
提前换相
↓
噪声/尖峰容易误触发
```

阈值太大：

```text
需要积累更久
↓
换相延迟
↓
电机效率下降
↓
高速可能跟不上
```

所以它实际上就是：

> **换相时刻的核心参数。**

---

# 三十、为什么 TI 要让 threshold 随速度变化？

这里：

```c
void ISBLDC_updateThresholdInt(
    ISBLDC_Handle handle,
    const float32_t speedRef)
```

里面：

```c
thresholdInt =
    thresholdMax
    - thresholdSF * speedAbs;
```

也就是说：

$$
Threshold
=
Threshold_{max}
-
K\cdot|\omega|
$$

然后限制：

$$
Threshold_{min}
\le Threshold
\le Threshold_{max}
$$

这很有意思。

---

# 三十一、为什么高速需要更小的积分阈值？

因为：

$$
E=K_e\omega
$$

速度越高：

$$
E↑
$$

因此：

```text
低速：

BEMF 小
  ↓
积分慢
  ↓
阈值不能太大
```

高速：

```text
BEMF 大
  ↓
积分非常快
  ↓
如果阈值太大
  ↓
换相延迟
```

所以 TI 做：

```text
低速 → 较大/特定阈值
高速 → 较小阈值
```

这是一个：

> **速度相关自适应换相阈值。**

不过你贴的这个版本里：

```c
thresholdSF
```

初始化代码被注释掉了：

```c
// obj->thresholdSF = ...
```

所以**当前这份代码实际上没有启用这个线性速度补偿逻辑**，除非其他地方又设置了 `thresholdSF`。

---

# 三十二、`ISBLDC_setParams()` 在干什么？

```c
obj->speedScaler = pUserParams->ctrlFreq_Hz;
```

记录控制频率。

然后：

```c
obj->thresholdMax =
    pUserParams->dcBus_nominal_V * threshold_max;
```

也就是：

$$
Threshold_{max}
=
V_{BUS}\times threshold\_max
$$

所以阈值和母线电压挂钩。

这非常合理，因为：

```text
24V母线
```

和：

```text
48V母线
```

BEMF 电压量级完全不同。

如果使用固定绝对电压阈值：

```text
24V → 适合
48V → 可能不适合
```

因此 TI 使用：

> **母线电压 × 比例系数**

来获得归一化程度更好的阈值。

---

# 三十三、整个 `ISBLDC_run()` 可以压缩成这张流程图

```text
             commState
                 │
                 ▼
      ┌─────────────────────┐
      │ 判断当前悬空相       │
      │ A / B / C            │
      └──────────┬──────────┘
                 │
                 ▼
      ┌─────────────────────┐
      │ 判断BEMF方向         │
      │ POS / NEG            │
      └──────────┬──────────┘
                 │
                 ▼
      ┌─────────────────────┐
      │ ADC三相电压           │
      │ Va Vb Vc             │
      └──────────┬──────────┘
                 │
                 ▼
      ┌─────────────────────┐
      │ 去Offset             │
      └──────────┬──────────┘
                 │
                 ▼
      ┌─────────────────────┐
      │ 转换到电机中性点      │
      │ VaN VbN VcN          │
      └──────────┬──────────┘
                 │
                 ▼
      ┌─────────────────────┐
      │ 选择悬空相电压        │
      │ VintPhase            │
      └──────────┬──────────┘
                 │
                 ▼
        ┌────────────────┐
        │ BEMF Lock?     │
        └───────┬────────┘
                │
          No    │    Yes
          ↓     │     ↓
      积分BEMF   │   等待反向
          │     │
          ▼     │
    |积分| > 阈值?
          │
        Yes
          ↓
   commTrigFlag=true
          ↓
     bemfInt清零
          ↓
       换相
          ↓
    时间戳计算速度
```

---

# 三十四、那么它和你之前研究的 SMO + PLL 到底有什么区别？

这个非常重要。

| 项目      | TI ISBLDC | SMO + PLL |
| ------- | --------- | --------- |
| 电机      | BLDC      | PMSM/BLDC |
| 控制方式    | 六步        | FOC       |
| 位置输出    | 6个离散换相区间  | 连续电角度     |
| 核心信号    | BEMF      | BEMF/状态观测 |
| 核心算法    | BEMF积分    | 滑模观测器     |
| PLL     | ❌         | ✅         |
| 输出      | 换相触发      | θe、ωe     |
| 电流控制    | 通常无       | Id/Iq     |
| SVPWM   | ❌         | ✅         |
| 正弦电流    | ❌         | ✅         |
| 实现复杂度   | 低         | 高         |
| 低成本 MCU | 非常适合      | 也可以       |

所以你不要把：

```c
ISBLDC
```

理解成：

> “TI 的另一种 SMO。”

不是。

它其实更加接近：

> **高级版 BEMF 六步无感换相器。**

---

# 三十五、还有一个特别关键的问题：它没有 `atan2()`

你之前一直在研究：

```c
theta = atan2(Ebeta, Ealpha)
```

这里完全没有。

因为它根本不需要连续角度。

它只需要知道：

```text
现在属于哪个 60°区间？
```

所以：

```text
SMO：

Ea/Eb
 ↓
atan2
 ↓
连续 θe
 ↓
FOC
```

而：

```text
ISBLDC：

悬空相 BEMF
 ↓
积分
 ↓
达到阈值
 ↓
换相
 ↓
下一个60°
```

---

# 三十六、你可以把两种算法画成这样

### TI ISBLDC

```text
                 BEMF
                   │
                   ▼
              积分器 ∫
                   │
                   ▼
              Threshold
                   │
                   ▼
           commTrigFlag
                   │
                   ▼
             六步换相
                   │
                   └──────→ 下一状态
```

### 你的 SMO + PLL

```text
         Va,Vb,Vc + Ia,Ib
                │
                ▼
              SMO
                │
          Eα       Eβ
           \       /
            \     /
             atan2
                │
              θe
                │
                ▼
              PLL
                │
          θe + ωe
                │
                ▼
             FOC
                │
          SVPWM / PWM
```

---

# 三十七、所以你真正应该关注 TI 这份代码的“精髓”

如果你以后自己实现，实际上核心只有 6 步：

```c
1. 获取 Va/Vb/Vc
        ↓
2. 计算 VaN/VbN/VcN
        ↓
3. 根据 commState 选择悬空相
        ↓
4. 判断 BEMF 正负
        ↓
5. 对正确方向 BEMF 积分
        ↓
6. 积分达到 threshold → 换相
```

速度估计再额外：

```c
7. 测量换相周期
        ↓
8. 换算 speed
```

这就是整个算法。

---

# 三十八、而你这份代码里最值得注意的一个细节

你贴的 `isbldc.h` 和 `isbldc.c` 里面有一个明显的版本差异：

`h` 里面：

```c
timeStampBuf[6]
```

而 `c` 里面实际使用的是：

```c
timeStampBuf[0] ~ timeStampBuf[4]
```

而且你贴出来的两个版本在：

```c
timePeriod
```

的计算顺序也略有区别。

这说明你现在拿到的很可能是 **TI Motor SDK 不同版本/不同阶段的 `ISBLDC` 实现**，不要机械地把两个文件逐字对应起来。

不过**算法思想完全一致**：

> **六步状态机 + 悬空相 BEMF + 单极性积分 + 阈值触发换相 + 换相周期测速。**

---

如果你接下来是想把它**真正移植到你现在的 ESP32 FOC 项目里**，那么最有价值的下一步其实是：我可以直接按照你现在的 **20 kHz ADC/PWM + 三相电压采样** 架构，把这套 TI `ISBLDC` 拆成一个非常干净的 `bldc_bemf_observer.c/.h`，并且把 **“为什么积分阈值对应 30°换相延迟”** 的数学关系也推出来。



---------------
----------------


可以把 TI 这套 **无感 BLDC** 理解成一条非常清晰的链路：

> **先用六步驱动让电机转起来 → 利用悬空相反电动势判断转子位置 → 判断“该不该换相” → 触发下一次六步换相 → 根据换相周期算速度 → 速度环调 PWM 占空比。**

它和你之前研究的 **SMO + PLL + FOC** 最大区别是：**它不追求连续电角度，而只需要知道什么时候进行下一次 60°换相。**

---

# 1. 整体架构

![Image](https://images.openai.com/static-rsc-4/CnErh8gswo4m0ml_wB8_1oqUAzPtsS0eS0MFvdsGAF_K3CbzD2XEf5T2FYK2SXM4KEQ0-sy-Ux5gvv3-IK18n1wNLU8ASioVAKpBnhs2z64xF5FfErdeNvsSIOT73G9fer4kWKFkA0_BIbK0Xo3bcP9frl1C8kiRkVNRvz6aBT-JuSVUhl_QTeJ4-QgUA58h?purpose=fullsize)

![Image](https://images.openai.com/static-rsc-4/xbBtB47_USwsa5v-3r4KxgeGbIQNqEmxu9AaJ8zJPSDp2-tHGHTv6jhTea1AxPH029KzIWydLAqSRjhAIpfhiWFpQt4Zc6zWAcZbBMZYih3ao9gnqAP07s84Ix5KvFt71HcPmgNIlev2vrOgArvArr0Gj1jFfDWDBB4sdaCrtBW3TFkMgMbRZlkHFs99oFQJ?purpose=fullsize)

![Image](https://images.openai.com/static-rsc-4/UVtnRB9SNJrDTmJY8S-ZOYH_5y861UCnhWwmgLe7AHLkZ-HktsHNq-KfFNUkbw0Do5nRLPCCQuxLt-gqUdERfU_ENK6rFlR-0zdUCAYtDgV05OwxMN2NHEDEvQZIWMpV4UiSm9rhLb5OFql7uDTDbaBRuBECs5b2n3udHhVBRPHmb6v1Ia9HlZE0gMLgZzIc?purpose=fullsize)

![Image](https://images.openai.com/static-rsc-4/anEn8i03X_KmXD1NaiMMcsi46qUhXWt0aRYn3r0aRqYx3kv4vPaBDv8d5Wio8OUxt34rkDKzWeaNnSgVWYvoP2qL2Ny-a835-xt4ytVJ8F65gv0aiJ9oDuERf67A2-eBXh-D-4QgCD8BF_JGNyqaxse_Sq1PlF_gOJbiR2BfaC-Dc2dF4hiOtacf2vNcQuzr?purpose=fullsize)

![Image](https://images.openai.com/static-rsc-4/wnRLmtSMe-aCWz2w2XoP3YP_IV1MI4pZiIKgLd4ATLzbDSjZvB24wzRRy1Yqf42_UGTWmM2kax1oj0cYdFJijN8r5oekoYr81DPHOOVPczaAfIk7e-h-nSBuGWHoPsnQESKVuIEGGgXr8D5gfcRhRH5I6lCmQO2X9dMYWO0ArQ0GIdN3R3PKxo7RLLESn3bn?purpose=fullsize)

整个流程可以画成：

```text
                  目标转速
                     │
                     ▼
                 速度控制器
                  PI/PID
                     │
                     ▼
                PWM Duty
                     │
                     ▼
              六步换相状态机
                     │
          ┌──────────┴──────────┐
          │                     │
          ▼                     ▼
      MOSFET驱动              电机
          │                     │
          │                 转子旋转
          │                     │
          │                     ▼
          │                  BEMF
          │                     │
          │                     ▼
          │              悬空相电压检测
          │                     │
          │                     ▼
          │                BEMF处理
          │                     │
          │                     ▼
          │                积分 ∫BEMF
          │                     │
          │                     ▼
          │               Threshold判断
          │                     │
          │                     ▼
          └──────────── 换相触发
                                │
                                ▼
                           下一60°状态
```

这就是 TI 这套东西最核心的框架。

---

# 2. 第一阶段：启动

这是无感 BLDC 最特殊的地方。

因为：

> **电机静止时没有反电动势。**

$$
E=K_e\omega
$$

当：

$$
\omega=0
$$

那么：

$$
E=0
$$

所以你不可能：

```text
静止
 ↓
检测BEMF
 ↓
知道转子位置
```

这是一个死循环。

所以启动阶段必须：

> **先不要依赖 BEMF，而是人为控制换相。**

---

# 3. 启动一般分几个阶段

典型是：

```text
STOP
 ↓
ALIGN
 ↓
OPEN LOOP ACCELERATION
 ↓
BEMF DETECTABLE
 ↓
CLOSED LOOP SENSORLESS
```

---

## ① ALIGN：转子定位

比如强制：

```text
A+
B-
C悬空
```

持续一段时间。

电流产生定子磁场：

```text
      定子磁场
          ↓
      ┌──────┐
      │ N  S │ ← 转子磁铁
      └──────┘
```

转子会被拉到一个固定位置。

这样 MCU 就知道：

> “现在转子大概在这里。”

---

# 4. 第二阶段：开环强拖

定位完成之后：

```text
State 0
 ↓
State 1
 ↓
State 2
 ↓
State 3
 ↓
State 4
 ↓
State 5
 ↓
State 0
```

注意：

**这时候还没有用 BEMF。**

MCU 自己按照越来越快的节奏换相。

比如：

```text
低速：

|------|------|------|------|
  10ms    8ms    6ms    5ms

逐渐：

|----|----|---|---|--|--|
```

这就是：

> **Open-loop acceleration**

目的是把电机拖到一个足够高的速度。

---

# 5. 为什么要开环加速？

因为：

$$
E=K_e\omega
$$

低速：

```text
ω小
 ↓
BEMF小
 ↓
信噪比差
```

高速：

```text
ω大
 ↓
BEMF大
 ↓
容易检测
```

所以：

> **先自己把电机转起来，等 BEMF 足够明显以后，再切换到无感闭环。**

---

# 6. 第三阶段：进入 BEMF 无感闭环

此时：

```text
电机已经转起来
        ↓
产生BEMF
        ↓
ADC采样三相电压
```

例如：

```text
Va
Vb
Vc
```

---

# 7. 根据当前换相状态选择悬空相

这是 TI `ISBLDC` 的核心。

比如：

```text
State 0
```

当前：

```text
A+ 
B-
C悬空
```

所以：

```text
检测 C 相
```

代码就是：

```c
case 0:
    bemfPhase = PHS_C;
    bemfDirect = DIR_NEG;
    break;
```

State 1：

```text
A+
C-
B悬空
```

所以：

```text
检测 B
```

State 2：

```text
B+
C-
A悬空
```

所以：

```text
检测 A
```

于是形成：

```text
State     检测相

  0          C
  1          B
  2          A
  3          C
  4          B
  5          A
```

---

# 8. 然后把三相对地电压转换成对中性点电压

ADC一般测：

```text
Va-GND
Vb-GND
Vc-GND
```

但 BEMF 更关心：

```text
Va-N
Vb-N
Vc-N
```

所以代码计算：

$$
V_{AN}=V_{AG}-\frac{V_{BG}+V_{CG}}{2}
$$

同理：

$$
V_{BN}=V_{BG}-\frac{V_{AG}+V_{CG}}{2}
$$

$$
V_{CN}=V_{CG}-\frac{V_{AG}+V_{BG}}{2}
$$

然后选出当前悬空相：

```text
VintPhase
```

---

# 9. 接下来就是整个算法的灵魂：BEMF积分

传统 BLDC：

```text
BEMF
 ↓
过零检测
 ↓
延迟30°
 ↓
换相
```

TI 这里采用了一个更有意思的方法：

```text
BEMF
 ↓
只保留正确方向
 ↓
积分
 ↓
达到阈值
 ↓
换相
```

数学上：

$$
I_{bemf}=\int V_{bemf}dt
$$

然后：

$$
|I_{bemf}|>Threshold
$$

就触发：

```c
commTrigFlag = true;
```

---

# 10. 为什么要积分？

因为实际 BEMF 信号不是干净的正弦/梯形波。

实际可能是：

```text
理想：

      /\
     /  \
____/    \____

实际：

   /\_/\
__/     \__/\_
     ↑
    噪声
```

如果单纯：

```c
if(Vbemf > 0)
```

很容易被：

```text
PWM尖峰
ADC噪声
MOSFET开关
二极管续流
```

误触发。

积分相当于：

> **不看某一个瞬间，而看一段时间积累下来的“伏秒面积”。**

所以抗噪能力更好。

---

# 11. `thresholdInt` 就决定什么时候换相

例如：

```text
BEMF
 │
 │      █████
 │     ███████
 │    █████████
 │___██████████________
             │
             ▼
         积分面积
             │
             ▼
        Threshold
             │
             ▼
           换相
```

所以：

```text
Threshold 太小
    ↓
容易提前换相

Threshold 太大
    ↓
换相太晚
```

这个参数实际上直接影响：

> **换相提前角/延迟。**

---

# 12. 换相之后发生什么？

一旦：

```c
commTrigFlag = true;
```

外部控制逻辑就会：

```text
State 0
 ↓
State 1
```

或者：

```text
State 1
 ↓
State 2
```

然后重新开始：

```text
选择新的悬空相
 ↓
检测新的BEMF
 ↓
积分
 ↓
Threshold
 ↓
再次换相
```

所以整个系统形成闭环：

```text
电机
 ↓
BEMF
 ↓
检测
 ↓
换相
 ↓
电机
 ↓
BEMF
 ↓
检测
 ↓
换相
```

---

# 13. 这就是“无感闭环”的真正含义

很多人第一次接触无感 BLDC 会误解：

> “是不是 MCU 算出了一个电角度？”

不是。

它实际上是：

```text
我不知道：
θ = 137.24°
```

它只知道：

```text
现在：
State 2

过一会儿：
该进入 State 3
```

所以它只有：

```text
0°
60°
120°
180°
240°
300°
```

这种**离散位置区间**。

---

# 14. 同时还能计算速度

每次换相的时候：

```c
timeStamp
```

记录一下时间。

例如：

```text
换相1
   ↓
100us
200us
300us
...
   ↓
换相2
```

那么：

$$
T_{comm}
$$

就是两次换相之间的时间。

而电机每：

$$
60^\circ
$$

换一次相。

所以：

$$
f_e
=
\frac{1}{6T_{comm}}
$$

如果是多极对电机：

$$
f_m=\frac{f_e}{p}
$$

再：

$$
RPM=60f_m
$$

所以：

$$
\boxed{
RPM=\frac{60}{6pT_{comm}}
}
$$

即：

$$
\boxed{
RPM=\frac{10}{pT_{comm}}
}
$$

这里的 \(p\) 是极对数。

---

# 15. TI 为什么连续记录多个换相时间？

你代码：

```c
timeStampBuf[0]
timeStampBuf[1]
timeStampBuf[2]
timeStampBuf[3]
timeStampBuf[4]
```

然后：

```c
timePeriod =
    timeStamp
    + buf0
    + buf1
    + buf2
    + buf3
    + buf4;
```

相当于：

> **用多个换相区间一起算平均速度。**

这样：

```text
单次测速：

1000
980
1020
970
1030
```

会抖得比较厉害。

平均以后：

```text
≈ 1000
```

稳定很多。

---

# 16. 最后就是速度控制

于是完整闭环就是：

```text
                目标RPM
                   │
                   ▼
              ┌─────────┐
              │ Speed PI│
              └────┬────┘
                   │
                   ▼
                 Duty
                   │
                   ▼
              六步换相器
                   │
                   ▼
                MOSFET
                   │
                   ▼
                 BLDC
                   │
                   ▼
                 BEMF
                   │
                   ▼
              BEMF检测器
                   │
             ┌─────┴─────┐
             │           │
             ▼           ▼
          换相事件       换相周期
             │           │
             ▼           ▼
          下一状态      Speed
             │           │
             │           └──────→ Speed PI
             │
             └──────────────→ 六步换相器
```

这才是完整的 TI 无感 BLDC 控制闭环。

---

# 17. 你可以把它分成 4 个模块

如果你以后自己写，建议直接按照这个架构理解：

### ① 启动器

```text
ALIGN
 ↓
OPEN LOOP
 ↓
ACCELERATION
```

负责：

> **让静止电机转起来。**

---

### ② BEMF Observer

也就是你贴的：

```text
ISBLDC
```

负责：

> **根据悬空相 BEMF 判断换相时刻。**

---

### ③ Commutation

```text
State 0
→ State 1
→ State 2
→ ...
```

负责：

> **控制六个 MOSFET 怎么导通。**

---

### ④ Speed Controller

```text
RPM_ref
   ↓
PI
   ↓
PWM Duty
```

负责：

> **控制电机转速。**

---

# 18. 最终把整个 TI 方案压缩成一句话

你可以记住这一句话：

> **TI 无感 BLDC = 开环启动 + 悬空相反电动势检测 + BEMF 积分判定换相 + 六步换相 + 换相周期测速 + 速度 PI 调占空比。**

它不是：

```text
BEMF
 ↓
atan2
 ↓
θe
 ↓
PLL
 ↓
FOC
```

而是：

```text
BEMF
 ↓
“什么时候该换相？”
 ↓
commTrigFlag
 ↓
六步换相
```

---

## 和你现在研究的 SMO+PLL 放在一起看

```text
       TI 无感 BLDC                         无感 FOC

     三相电压采样                         三相电压/电流
          │                                   │
          ▼                                   ▼
       悬空相 BEMF                         SMO
          │                                   │
          ▼                              Eα / Eβ
      BEMF 积分                              │
          │                                  ▼
          ▼                                PLL
    换相触发信号                             │
          │                                  ▼
          ▼                                θe
     六步换相                               │
          │                                  ▼
          ▼                              Park/PI
        BLDC                                 │
                                             ▼
                                           SVPWM
```

**最大的本质区别：**

$$
\boxed{\text{六步 BLDC：估计“换相时刻”}}
$$

而：

$$
\boxed{\text{无感 FOC：估计“连续电角度”}}
$$

所以如果你现在的目标是**做一个低成本、几块钱 BOM 的无感 BLDC 电调**，TI 这套思路其实非常值得借鉴——它比 SMO+PLL+FOC 简单很多，MCU 运算量和外围要求也低很多。



------------
------------

需要，但要分清楚 **“ADC”和“比较器”分别干什么**。

对于你贴的这套 **TI ISBLDC 无感方案**：

### 结论先说

| 硬件          | 是否必须     | 作用                        |
| ----------- | -------- | ------------------------- |
| **ADC**     | ✅ 需要     | 采样三相电压，做 BEMF 积分          |
| **比较器 CMP** | ❌ 不必须    | 可以不用，TI 这份代码就是 ADC + 软件判断 |
| 三相电压采样      | ✅ 基本需要   | 获取悬空相 BEMF                |
| 电流 ADC      | ⚠️ 视控制需求 | 这套 BEMF 检测本身不依赖相电流        |
| 过零比较器       | ❌ 不需要    | 它不是传统硬件过零检测方案             |

---

# 1. 你这份代码明确就是 ADC 路线

看这里：

```c
void ISBLDC_run(
    ISBLDC_Handle handle,
    MATH_Vec3 pVabcVec)
```

输入：

```c
pVabcVec
```

就是：

```text
Va
Vb
Vc
```

然后：

```c
obj->Vabcg.value[0] =
    pVabcVec->value[0] -
    obj->VabcOffset.value[0];
```

说明它拿到的是**实际采样得到的三相电压**。

然后：

```c
Vabcg
 ↓
Vabcn
 ↓
VintPhase
 ↓
BEMF积分
```

整个过程都是 MCU 软件计算。

---

# 2. 它不需要比较器

传统无感 BLDC 经常是：

```text
悬空相
  ↓
BEMF
  ↓
比较器
  ↓
0V / 1
  ↓
过零检测
```

例如：

```text
        BEMF
          │
          ▼
     ┌─────────┐
     │Comparator│
     └────┬────┘
          │
          ▼
       GPIO/CMP
          │
          ▼
      Zero Cross
```

这种方案比较器直接告诉 MCU：

```text
BEMF > 0
```

还是：

```text
BEMF < 0
```

---

# 3. TI 这份 ISBLDC 不一样

它是：

```text
三相电压
   ↓
ADC
   ↓
MCU
   ↓
软件计算 Vabcn
   ↓
选择悬空相
   ↓
软件判断正负
   ↓
软件积分
   ↓
Threshold
   ↓
commTrigFlag
```

例如：

```c
obj->bemfInt += obj->VintPhase;

if(obj->bemfInt > obj->thresholdInt)
{
    obj->commTrigFlag = true;
}
```

所以这里根本没有：

```c
CMP
```

或者：

```c
Comparator
```

---

# 4. 为什么 ADC 更灵活？

因为 ADC 给你的是：

```text
2.35V
2.42V
2.51V
2.63V
...
```

而比较器只有：

```text
0
1
```

ADC 可以做：

```text
BEMF过零
+
BEMF积分
+
阈值调整
+
软件滤波
+
速度估计
+
故障检测
```

所以 TI 这套积分算法天然适合 ADC。

---

# 5. 三相电压到底怎么接 ADC？

典型硬件：

```text
              VBUS
                │
          ┌─────┴─────┐
          │ 三相逆变桥 │
          └─┬──┬──┬──┘
            │  │  │
            A  B  C
            │  │  │
           分压分压分压
            │  │  │
           ADC ADC ADC
```

例如：

```text
A ── R ──┬── R ── GND
         │
        ADC_A

B ── R ──┬── R ── GND
         │
        ADC_B

C ── R ──┬── R ── GND
         │
        ADC_C
```

因为 MCU ADC 一般只能接受：

```text
0 ~ 3.3V
```

所以必须分压。

---

# 6. 这里有个非常重要的问题：为什么要采三相？

因为当前状态下：

```text
两相导通
一相悬空
```

比如：

```text
State 0

A → PWM
B → LOW
C → Hi-Z
```

你需要知道：

```text
C 相电压
```

所以最简单就是：

```text
ADC_A
ADC_B
ADC_C
```

三个都采。

---

# 7. 但实际上能不能只采一个 ADC？

**可以。**

因为当前 `commState` 已经知道：

```text
现在 C 是悬空
```

那么理论上：

```text
只采 C
```

就够做 BEMF 检测。

下一状态：

```text
B 悬空
```

切换 ADC MUX：

```text
采 B
```

再下一状态：

```text
A 悬空
```

采 A。

所以可以设计成：

```text
       ┌────────────┐
A ────►│            │
B ────►│ ADC MUX    │──► ADC
C ────►│            │
       └────────────┘
```

但是工程上：

> **如果 MCU ADC 通道足够，我更建议直接采三相。**

因为软件简单，而且可以同时做：

* BEMF
* 电压监控
* 母线电压
* 故障判断
* 参数调试

---

# 8. 但是你可能会问：为什么不用比较器？

因为比较器方案其实也有优势。

例如：

```text
BEMF
 ↓
CMP
 ↓
硬件过零
 ↓
PWM同步/中断
 ↓
换相
```

优点：

* 响应快
* CPU负担低
* 不需要高速 ADC
* 硬件可以直接触发换相
* 很适合极低成本 BLDC

但是它比较难做：

> **BEMF 积分这种连续幅值算法。**

因为比较器只告诉你：

```text
正 / 负
```

不知道：

```text
到底是 +0.2V
还是 +2V
```

而 TI 的算法需要：

$$
\int V_{BEMF}dt
$$

所以必须知道 BEMF 的**幅值**。

---

# 9. 这也解释了为什么 TI 这里叫“积分”

如果只有比较器：

```text
BEMF：

+ + + + + + + + 
```

你最多知道：

```text
BEMF > 0
```

但是无法直接计算：

$$
\int V_{BEMF}dt
$$

因为你不知道每个采样点的：

$$
V_{BEMF}
$$

而 ADC：

```text
+0.1V
+0.3V
+0.5V
+0.8V
+1.2V
+1.5V
```

就可以：

$$
\sum V_{BEMF}[k]
$$

得到积分。

---

# 10. 还有一个关键点：它不一定需要“专门的 BEMF ADC”

你可能会以为：

```text
BEMF
 ↓
专门ADC
```

其实没必要。

只需要普通 MCU ADC：

```text
三相电压分压
      ↓
MCU ADC
      ↓
软件处理
```

就行。

这也是这种方案非常便宜的原因。

---

# 11. 如果你想做超低成本电调

比如你前面说的：

> **BOM 目标 4～5 元，不包含电机**

那么硬件上我会优先考虑：

```text
             MCU
              │
       ┌──────┼──────┐
       │      │      │
      ADC    ADC    ADC
       │      │      │
       A      B      C
       │      │      │
       └── 三相逆变桥 ──┘
```

而不是：

```text
三相电压
 ↓
三个外部比较器
 ↓
GPIO
```

如果 MCU 自带：

* ADC
* PWM
* 硬件触发 ADC
* 比较器（可选）

那么**全部集成在 MCU 里面**，外围成本最低。

---

# 12. 对你现在研究的 TI 方案，记住这个硬件链路就够了

```text
              ┌───────────────┐
              │      MCU      │
              │               │
 A电压 ──────►│ ADC_A         │
 B电压 ──────►│ ADC_B         │
 C电压 ──────►│ ADC_C         │
              │       │       │
              │       ▼       │
              │   ISBLDC算法  │
              │       │       │
              │       ▼       │
              │ commTrigFlag │
              └───────┬───────┘
                      │
                      ▼
                  六步换相
                      │
                      ▼
                  PWM/MOSFET
                      │
                      ▼
                    BLDC
```

**所以你贴的这套 `ISBLDC`，硬件上核心就是：**

$$
\boxed{\text{三相电压采样 ADC}}
$$

**不要求外部比较器。**

而如果做的是**传统 BEMF 过零检测**，才更常见：

$$
\boxed{\text{悬空相 BEMF + 比较器}}
$$

这两种方案不要混在一起。



------------
------------
# 代码方案

可以。下面我直接按照你贴的 **TI Motor SDK `ISBLDC` 思路**，给你整理一套完整的 **无感六步 BLDC C 语言伪代码**。

我会把它拆成：

```text
① 参数/数据结构
② 初始化
③ ADC三相电压处理
④ BEMF检测
⑤ BEMF积分换相
⑥ 六步换相
⑦ 启动/强拖
⑧ BEMF闭环切换
⑨ 换相周期测速
⑩ 速度PI
⑪ 主控制流程
```

这样你拿去移植到 STM32 / ESP32 / TI C2000 都比较容易。

---

# 1. 整体软件架构

```text
                     ┌──────────────┐
                     │   Speed Ref  │
                     └──────┬───────┘
                            ↓
                     ┌──────────────┐
                     │   Speed PI   │
                     └──────┬───────┘
                            ↓
                         PWM Duty
                            │
                            ▼
                  ┌──────────────────┐
                  │ Six-Step Driver  │
                  │ State 0~5        │
                  └────────┬─────────┘
                           │
                           ▼
                     ┌───────────┐
                     │  BLDC     │
                     └─────┬─────┘
                           │
                           ▼
                     三相 BEMF
                           │
                           ▼
                    ┌─────────────┐
                    │ ADC Voltage │
                    └──────┬──────┘
                           ↓
                    ┌─────────────┐
                    │ BEMF处理器  │
                    └──────┬──────┘
                           ↓
                      BEMF积分
                           ↓
                       Threshold
                           ↓
                    commTrigFlag
                           │
                           └─────────→ State + 1
```

---

# 2. 数据结构

先把 TI 那套结构简化一下：

```c
typedef enum
{
    BLDC_PHASE_A = 0,
    BLDC_PHASE_B,
    BLDC_PHASE_C
} BLDC_Phase_e;


typedef enum
{
    BEMF_NEG = 0,
    BEMF_POS
} BEMF_Dir_e;


typedef enum
{
    BLDC_STARTUP = 0,
    BLDC_OPEN_LOOP,
    BLDC_BEMF_CLOSED_LOOP
} BLDC_RunState_e;
```

控制器：

```c
typedef struct
{
    /* =========================
     * ADC三相电压
     * ========================= */
    float Va;
    float Vb;
    float Vc;

    /* ADC Offset */
    float VaOffset;
    float VbOffset;
    float VcOffset;

    /* 相对虚拟中性点 */
    float Van;
    float Vbn;
    float Vcn;


    /* =========================
     * 当前BEMF检测
     * ========================= */

    BLDC_Phase_e bemfPhase;
    BEMF_Dir_e   bemfDir;

    float Vbemf;

    /* BEMF积分 */
    float bemfIntegral;

    /* 积分阈值 */
    float threshold;

    float thresholdMax;
    float thresholdMin;


    /* =========================
     * 换相
     * ========================= */

    uint16_t commState;

    bool commTrigger;
    bool bemfLock;


    /* =========================
     * PWM
     * ========================= */

    float duty;


    /* =========================
     * 速度
     * ========================= */

    uint32_t timestamp;

    uint32_t commPeriod;

    uint32_t commTimeBuf[6];

    float speedHz;
    float speedRPM;


    /* =========================
     * 启动
     * ========================= */

    BLDC_RunState_e runState;

    uint32_t startupTimer;

    uint32_t openLoopTimer;

    float openLoopPeriod;

    float openLoopPeriodMin;


    /* =========================
     * 速度PI
     * ========================= */

    float speedRefRPM;

    float speedKp;
    float speedKi;

    float speedIntegral;

} BLDC_Controller;
```

---

# 3. 初始化

```c
void BLDC_Init(BLDC_Controller *bldc)
{
    memset(bldc, 0, sizeof(BLDC_Controller));

    bldc->commState = 0;

    bldc->duty = 0.05f;

    bldc->thresholdMax = 1.0f;
    bldc->thresholdMin = 0.1f;

    bldc->threshold = bldc->thresholdMax;

    bldc->runState = BLDC_STARTUP;

    bldc->startupTimer = 0;

    bldc->openLoopTimer = 0;

    bldc->speedKp = 0.001f;
    bldc->speedKi = 0.0001f;

    bldc->speedIntegral = 0.0f;

    bldc->bemfIntegral = 0.0f;

    bldc->bemfLock = false;
    bldc->commTrigger = false;
}
```

---

# 4. ADC读取三相电压

假设 ADC 返回已经转换成实际电压：

```c
void BLDC_ReadVoltage(BLDC_Controller *bldc)
{
    bldc->Va = ADC_ReadPhaseA();
    bldc->Vb = ADC_ReadPhaseB();
    bldc->Vc = ADC_ReadPhaseC();
}
```

如果 ADC 返回的是原始 ADC：

```c
bldc->Va =
    ADC_ReadPhaseA() *
    ADC_VREF /
    ADC_MAX *
    VOLTAGE_DIVIDER;
```

实际项目里就是这么换算。

---

# 5. 去 ADC Offset

对应 TI：

```c
Vabcg = Vabc - VabcOffset;
```

代码：

```c
void BLDC_RemoveVoltageOffset(BLDC_Controller *bldc)
{
    bldc->Va -= bldc->VaOffset;
    bldc->Vb -= bldc->VbOffset;
    bldc->Vc -= bldc->VcOffset;
}
```

---

# 6. 计算三相对虚拟中性点电压

这就是你贴的 TI 代码：

```c
void BLDC_CalcPhaseVoltage(BLDC_Controller *bldc)
{
    bldc->Van =
        bldc->Va -
        0.5f * (bldc->Vb + bldc->Vc);

    bldc->Vbn =
        bldc->Vb -
        0.5f * (bldc->Va + bldc->Vc);

    bldc->Vcn =
        bldc->Vc -
        0.5f * (bldc->Va + bldc->Vb);
}
```

这一步非常重要：

```text
ADC测到：

Va-GND
Vb-GND
Vc-GND

       ↓

计算：

Va-N
Vb-N
Vc-N
```

---

# 7. 根据六步状态选择 BEMF 相

这部分直接照着 TI：

```c
void BLDC_SelectBEMFPhase(BLDC_Controller *bldc)
{
    switch(bldc->commState)
    {
        case 0:

            bldc->bemfPhase = BLDC_PHASE_C;
            bldc->bemfDir   = BEMF_NEG;

            break;


        case 1:

            bldc->bemfPhase = BLDC_PHASE_B;
            bldc->bemfDir   = BEMF_POS;

            break;


        case 2:

            bldc->bemfPhase = BLDC_PHASE_A;
            bldc->bemfDir   = BEMF_NEG;

            break;


        case 3:

            bldc->bemfPhase = BLDC_PHASE_C;
            bldc->bemfDir   = BEMF_POS;

            break;


        case 4:

            bldc->bemfPhase = BLDC_PHASE_B;
            bldc->bemfDir   = BEMF_NEG;

            break;


        case 5:

            bldc->bemfPhase = BLDC_PHASE_A;
            bldc->bemfDir   = BEMF_POS;

            break;
    }
}
```

于是：

```text
State 0 → C负
State 1 → B正
State 2 → A负
State 3 → C正
State 4 → B负
State 5 → A正
```

---

# 8. 选择当前 BEMF 电压

```c
float BLDC_GetBEMFVoltage(BLDC_Controller *bldc)
{
    switch(bldc->bemfPhase)
    {
        case BLDC_PHASE_A:
            return bldc->Van;

        case BLDC_PHASE_B:
            return bldc->Vbn;

        case BLDC_PHASE_C:
            return bldc->Vcn;

        default:
            return 0.0f;
    }
}
```

---

# 9. BEMF积分算法

这是整个算法最核心的函数。

```c
void BLDC_BEMF_Integrator(BLDC_Controller *bldc)
{
    bldc->commTrigger = false;


    /* ==========================
     * 正向BEMF
     * ========================== */

    if(bldc->bemfDir == BEMF_POS)
    {
        /* 换相后的锁定状态 */
        if(bldc->bemfLock)
        {
            /*
             * 等待BEMF重新变成负
             */
            if(bldc->Vbemf < 0.0f)
            {
                bldc->bemfLock = false;
            }

            return;
        }


        /*
         * 只允许正电压积分
         */
        if(bldc->Vbemf > 0.0f)
        {
            bldc->bemfIntegral += bldc->Vbemf;

            /*
             * 达到积分阈值
             */
            if(bldc->bemfIntegral >= bldc->threshold)
            {
                bldc->commTrigger = true;

                bldc->bemfLock = true;

                bldc->bemfIntegral = 0.0f;
            }
        }
        else
        {
            /*
             * 方向错误
             */
            bldc->bemfIntegral = 0.0f;
        }
    }


    /* ==========================
     * 负向BEMF
     * ========================== */

    else
    {
        if(bldc->bemfLock)
        {
            /*
             * 等待BEMF重新变成正
             */
            if(bldc->Vbemf > 0.0f)
            {
                bldc->bemfLock = false;
            }

            return;
        }


        /*
         * 只允许负电压积分
         */
        if(bldc->Vbemf < 0.0f)
        {
            bldc->bemfIntegral += bldc->Vbemf;

            /*
             * 达到负阈值
             */
            if(bldc->bemfIntegral <=
               -bldc->threshold)
            {
                bldc->commTrigger = true;

                bldc->bemfLock = true;

                bldc->bemfIntegral = 0.0f;
            }
        }
        else
        {
            bldc->bemfIntegral = 0.0f;
        }
    }
}
```

---

# 10. 注意一个实际工程问题

严格数学积分应该：

```c
bemfIntegral += Vbemf * Ts;
```

也就是：

$$
I=\int Vdt
$$

离散化：

$$
I[k]=I[k-1]+V[k]T_s
$$

所以如果你自己实现，我更建议：

```c
bldc->bemfIntegral +=
    bldc->Vbemf *
    CONTROL_PERIOD;
```

然后阈值也按照：

```text
V × s
```

来定义。

TI 那份代码没有显式乘 `Ts`，因为它把采样频率相关的比例关系放到了阈值/缩放参数体系里。

**你自己写的时候最好显式乘 `Ts`，更容易理解和调参。**

---

# 11. 换相函数

检测器只负责：

```c
commTrigger = true;
```

真正的 MOSFET 换相最好独立出来。

```c
void BLDC_Commutate(BLDC_Controller *bldc)
{
    bldc->commState++;

    if(bldc->commState >= 6)
    {
        bldc->commState = 0;
    }


    switch(bldc->commState)
    {
        case 0:

            /*
             * A+
             * B-
             * C Hi-Z
             */

            PhaseA_HighPWM(bldc->duty);
            PhaseB_Low();
            PhaseC_HighZ();

            break;


        case 1:

            /*
             * A+
             * C-
             * B Hi-Z
             */

            PhaseA_HighPWM(bldc->duty);
            PhaseC_Low();
            PhaseB_HighZ();

            break;


        case 2:

            /*
             * B+
             * C-
             * A Hi-Z
             */

            PhaseB_HighPWM(bldc->duty);
            PhaseC_Low();
            PhaseA_HighZ();

            break;


        case 3:

            /*
             * B+
             * A-
             * C Hi-Z
             */

            PhaseB_HighPWM(bldc->duty);
            PhaseA_Low();
            PhaseC_HighZ();

            break;


        case 4:

            /*
             * C+
             * A-
             * B Hi-Z
             */

            PhaseC_HighPWM(bldc->duty);
            PhaseA_Low();
            PhaseB_HighZ();

            break;


        case 5:

            /*
             * C+
             * B-
             * A Hi-Z
             */

            PhaseC_HighPWM(bldc->duty);
            PhaseB_Low();
            PhaseA_HighZ();

            break;
    }
}
```

---

# 12. 这里千万注意一个问题

上面的：

```text
State 0 → A+ B- C悬空
State 1 → A+ C- B悬空
...
```

只是**典型六步顺序**。

你实际电机：

```text
ABC相序
霍尔方向
电机绕组定义
PWM高低侧方式
```

可能和 TI 定义不一致。

所以实际移植时可能需要：

```text
State顺序反过来
```

或者：

```text
BEMF正负反过来
```

这是正常的。

---

# 13. 换相周期测速

下面实现 TI 的思想。

```c
void BLDC_UpdateSpeed(BLDC_Controller *bldc)
{
    if(bldc->commTrigger)
    {
        /*
         * 当前换相间隔
         */
        uint32_t period =
            bldc->timestamp;


        /*
         * 保存历史值
         */
        bldc->commTimeBuf[5] =
            bldc->commTimeBuf[4];

        bldc->commTimeBuf[4] =
            bldc->commTimeBuf[3];

        bldc->commTimeBuf[3] =
            bldc->commTimeBuf[2];

        bldc->commTimeBuf[2] =
            bldc->commTimeBuf[1];

        bldc->commTimeBuf[1] =
            bldc->commTimeBuf[0];

        bldc->commTimeBuf[0] =
            period;


        /*
         * 多个换相周期求和
         */
        uint32_t total =
            bldc->commTimeBuf[0] +
            bldc->commTimeBuf[1] +
            bldc->commTimeBuf[2] +
            bldc->commTimeBuf[3] +
            bldc->commTimeBuf[4];


        /*
         * 转换成时间
         */
        float totalTime =
            total * CONTROL_PERIOD;


        /*
         * 5个换相区间
         *
         * 一个换相区间 = 60°电角度
         *
         * 这里根据你实际平均窗口
         * 计算电频率
         */
        float electricalFrequency =
            5.0f /
            (6.0f * totalTime);


        /*
         * 机械频率
         */
        float mechanicalFrequency =
            electricalFrequency /
            MOTOR_POLE_PAIRS;


        /*
         * RPM
         */
        bldc->speedRPM =
            mechanicalFrequency * 60.0f;


        /*
         * 清零当前计时器
         */
        bldc->timestamp = 0;
    }
}
```

不过这里有一个工程细节：

**第一次、第二次换相的时候历史 buffer 还没填满，不能直接拿 5 个值平均。**

实际代码应该增加：

```c
validCommCount
```

后面再计算。

---

# 14. 更简单的单次测速公式

如果不做平均：

```c
float BLDC_GetSpeedRPM(BLDC_Controller *bldc)
{
    float Tcomm =
        bldc->timestamp *
        CONTROL_PERIOD;

    /*
     * 每次换相 = 60°电角度
     */

    float electricalFreq =
        1.0f /
        (6.0f * Tcomm);


    float mechanicalFreq =
        electricalFreq /
        MOTOR_POLE_PAIRS;


    return mechanicalFreq * 60.0f;
}
```

最终：

$$
\boxed{
RPM=
\frac{10}
{pT_{comm}}
}
$$

其中：

* \(p\)：极对数
* \(T_{comm}\)：一次 60°电角换相所花时间

---

# 15. 启动阶段

这是完整系统里非常重要的一部分。

```c
void BLDC_Startup(BLDC_Controller *bldc)
{
    switch(bldc->runState)
    {
        case BLDC_STARTUP:

            /*
             * 1. 转子定位
             */

            bldc->duty = 0.10f;

            BLDC_Align();

            bldc->startupTimer++;

            if(bldc->startupTimer >
               ALIGN_TIME)
            {
                bldc->runState =
                    BLDC_OPEN_LOOP;

                bldc->openLoopPeriod =
                    OPEN_LOOP_PERIOD_START;

                bldc->openLoopTimer = 0;
            }

            break;


        case BLDC_OPEN_LOOP:

            /*
             * 开环强拖
             */

            bldc->openLoopTimer++;

            if(bldc->openLoopTimer >=
               bldc->openLoopPeriod)
            {
                bldc->openLoopTimer = 0;

                /*
                 * 强制换相
                 */
                BLDC_Commutate(bldc);


                /*
                 * 逐渐加速
                 */
                bldc->openLoopPeriod -=
                    OPEN_LOOP_ACCEL;


                if(bldc->openLoopPeriod <
                   bldc->openLoopPeriodMin)
                {
                    bldc->openLoopPeriod =
                        bldc->openLoopPeriodMin;
                }
            }


            /*
             * 检查BEMF是否已经足够
             */

            if(BLDC_BEMF_IsReliable(bldc))
            {
                bldc->runState =
                    BLDC_BEMF_CLOSED_LOOP;
            }

            break;


        case BLDC_BEMF_CLOSED_LOOP:

            /*
             * 已经进入无感闭环
             */

            break;
    }
}
```

---

# 16. BEMF是否可靠？

可以简单做：

```c
bool BLDC_BEMF_IsReliable(BLDC_Controller *bldc)
{
    if(fabsf(bldc->Vbemf) >
       BEMF_MIN_VOLTAGE)
    {
        return true;
    }

    return false;
}
```

实际工程可以更严格：

```text
BEMF幅值足够
+
连续几个周期检测成功
+
换相周期稳定
+
速度超过最低转速
```

才切闭环。

例如：

```c
if(speedRPM > SENSORLESS_MIN_RPM &&
   bemfDetectCount > 20)
{
    switchToSensorless = true;
}
```

---

# 17. 速度 PI

有了：

```c
speedRPM
```

就可以做速度环：

```c
void BLDC_SpeedPI(BLDC_Controller *bldc)
{
    float error =
        bldc->speedRefRPM -
        bldc->speedRPM;


    /*
     * 积分
     */
    bldc->speedIntegral +=
        error * SPEED_LOOP_TS;


    /*
     * 限幅
     */
    if(bldc->speedIntegral >
       DUTY_MAX)
    {
        bldc->speedIntegral =
            DUTY_MAX;
    }

    if(bldc->speedIntegral <
       DUTY_MIN)
    {
        bldc->speedIntegral =
            DUTY_MIN;
    }


    /*
     * PI
     */
    float duty =
        bldc->speedKp * error +
        bldc->speedKi *
        bldc->speedIntegral;


    /*
     * 限幅
     */
    if(duty > DUTY_MAX)
        duty = DUTY_MAX;

    if(duty < DUTY_MIN)
        duty = DUTY_MIN;


    bldc->duty = duty;
}
```

于是：

```text
目标RPM
   ↓
Speed PI
   ↓
Duty
   ↓
六步PWM
   ↓
BLDC
   ↓
BEMF
   ↓
测速
   ↓
Speed PI
```

形成完整闭环。

---

# 18. 最核心的 20 kHz ISR

假设你和现在的项目一样：

```text
PWM = 20kHz
ADC = 20kHz
```

那么每次 PWM/ADC 同步触发：

```c
void BLDC_ControlISR(void)
{
    /*
     * =========================
     * 1. ADC
     * =========================
     */

    BLDC_ReadVoltage(&bldc);


    /*
     * =========================
     * 2. 去Offset
     * =========================
     */

    BLDC_RemoveVoltageOffset(&bldc);


    /*
     * =========================
     * 3. 计算相电压
     * =========================
     */

    BLDC_CalcPhaseVoltage(&bldc);


    /*
     * =========================
     * 4. 根据状态选择BEMF
     * =========================
     */

    BLDC_SelectBEMFPhase(&bldc);


    /*
     * =========================
     * 5. 获取悬空相BEMF
     * =========================
     */

    bldc.Vbemf =
        BLDC_GetBEMFVoltage(&bldc);


    /*
     * =========================
     * 6. BEMF积分
     * =========================
     */

    if(bldc.runState ==
       BLDC_BEMF_CLOSED_LOOP)
    {
        BLDC_BEMF_Integrator(&bldc);
    }


    /*
     * =========================
     * 7. 换相
     * =========================
     */

    if(bldc.commTrigger)
    {
        BLDC_Commutate(&bldc);

        bldc.timestamp = 0;
    }


    /*
     * =========================
     * 8. 时间计数
     * =========================
     */

    bldc.timestamp++;


    /*
     * =========================
     * 9. 超时判断
     * =========================
     */

    if(bldc.timestamp >
       SENSORLESS_TIMEOUT)
    {
        /*
         * 电机可能堵转
         */

        bldc.speedRPM = 0;

        bldc.runState =
            BLDC_STARTUP;
    }
}
```

---

# 19. 实际项目最好把速度环降低频率

不要 20 kHz 每次都跑速度 PI。

例如：

```text
PWM/ADC
 20kHz
   │
   ├── BEMF检测 20kHz
   │
   ├── 换相
   │
   └── 分频
        ↓
       1kHz
        ↓
     Speed PI
```

所以：

```c
if(++speedLoopCounter >= 20)
{
    speedLoopCounter = 0;

    BLDC_SpeedPI(&bldc);
}
```

这样更合理。

---

# 20. 最终完整流程

整个代码运行起来实际上就是：

```text
                  上电
                   │
                   ▼
              BLDC_Init()
                   │
                   ▼
                ALIGN
                   │
                   ▼
            强制六步换相
                   │
                   ▼
             OPEN LOOP
                   │
                   ▼
             电机开始旋转
                   │
                   ▼
             BEMF逐渐增大
                   │
                   ▼
          BEMF是否足够可靠？
             │          │
            NO         YES
             │          │
             └──继续───→│
                        ▼
                SENSORLESS
                CLOSED LOOP
                        │
                        ▼
               ADC读取Va/Vb/Vc
                        │
                        ▼
                  虚拟中性点
                        │
                        ▼
                 选择悬空相
                        │
                        ▼
                    BEMF
                        │
                        ▼
                 单方向积分
                        │
                        ▼
              Integral > Threshold
                        │
                        ▼
                commTrigger=true
                        │
                        ▼
                   六步换相
                        │
                        ▼
                  测量换相周期
                        │
                        ▼
                     RPM
                        │
                        ▼
                   Speed PI
                        │
                        ▼
                    PWM Duty
                        │
                        └──────────┐
                                   │
                                   ▼
                                  电机
```

---

# 21. 你真正需要移植的核心代码其实就这么点

如果把启动、速度 PI、硬件驱动全部拿掉，**TI `ISBLDC` 最核心的算法甚至可以浓缩成：**

```c
void SensorlessBLDC_Run(void)
{
    /*
     * ① 读取三相电压
     */
    Read_Vabc();


    /*
     * ② 转换成相对中性点电压
     */
    Van = Va - (Vb + Vc) * 0.5f;
    Vbn = Vb - (Va + Vc) * 0.5f;
    Vcn = Vc - (Va + Vb) * 0.5f;


    /*
     * ③ 根据六步状态选择悬空相
     */
    Select_BEMF_Phase();


    /*
     * ④ 得到BEMF
     */
    Vbemf = Get_BEMF();


    /*
     * ⑤ 积分
     */
    if(BEMF方向正确)
    {
        bemfIntegral += Vbemf * Ts;
    }
    else
    {
        bemfIntegral = 0;
    }


    /*
     * ⑥ 达到阈值
     */
    if(fabsf(bemfIntegral) >
       threshold)
    {
        bemfIntegral = 0;

        /*
         * ⑦ 换相
         */
        commState++;

        if(commState >= 6)
            commState = 0;


        /*
         * ⑧ 测速
         */
        CalculateSpeed();
    }
}
```

**这就是 TI 这套算法的骨架。**

你现在如果是准备在 **ESP32 + 20 kHz PWM/ADC** 上实际做，我建议下一步重点不是继续堆代码，而是把 **“三相电压采样电路 → ADC采样时刻 → 悬空相BEMF到底应该是什么波形 → `threshold` 怎么根据你的 12V/2208-80T 电机算出来”** 这一整套接起来。否则代码写出来，最容易卡在“ADC看到的根本不像理论 BEMF”这一关。
