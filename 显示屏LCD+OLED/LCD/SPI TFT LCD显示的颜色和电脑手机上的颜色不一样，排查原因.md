这是**非常正常**的现象，不是你代码写错了 👍
一句话结论：

👉 **不同屏幕 ≠ 同一套颜色标准，所以显示出来一定有偏差**

但这里面其实有好几个“坑点”，我给你从**硬件 → 接口 → 软件 → 人眼**一层一层讲清楚。

---

# 🎨 一、最核心原因：屏幕不是一个“级别”的东西

## 💻 笔记本屏幕（高端选手）

![Image](https://i-blog.csdnimg.cn/img_convert/2afce3e1519743819a9e256ca640dbfe.jpeg)

![Image](https://i-blog.csdnimg.cn/img_convert/f05a4ddb477339f7c657133304c64311.jpeg)

![Image](https://i-blog.csdnimg.cn/img_convert/5114b2dee4d0d895f3b134c2076d8ab7.jpeg)

一般特点：

* IPS / OLED
* 支持 **sRGB / AdobeRGB**
* 出厂校色
* 颜色准确（偏“真实”）

---

## 📺 你的 TFT LCD（嵌入式）

![Image](https://i-blog.csdnimg.cn/img_convert/5dd14f0a47a158667468ef050233164f.jpeg)

![Image](https://i-blog.csdnimg.cn/img_convert/b78074fca51597a7fd2042de89a98256.jpeg)

![Image](https://i-blog.csdnimg.cn/img_convert/95cbe2f94d4faab201994e4ee0a3f9a9.jpeg)

特点：

* 低成本面板
* 无校色
* 色域小
* 偏色严重（常见：偏蓝 / 偏绿）

---

👉 所以第一层结论：

```text
不是你LCD错了，是它“本来就不准”
```

---

# ⚠️ 二、你这个项目还有一个关键原因：RGB565

你用的是：

👉 SPI + ST7789 → **RGB565**

---

## 📉 RGB565 vs 电脑屏幕（RGB888）

![Image](https://i-blog.csdnimg.cn/img_convert/abcd573d4cf761f56d8c59b46d24b047.jpeg)

![Image](https://i-blog.csdnimg.cn/img_convert/cde96005f29988a9cba42ddeeb708ef3.jpeg)

![Image](https://i-blog.csdnimg.cn/img_convert/5ca853f2f9183d67fd5fdb58b9fe2d83.jpeg)

| 类型     | 位数  | 颜色数量  |
| ------ | --- | ----- |
| RGB888 | 24位 | 1677万 |
| RGB565 | 16位 | 65536 |

---

👉 直接影响：

* 渐变变“断层”（banding）
* 颜色不细腻
* 某些颜色明显偏差

---

# 🔧 三、Gamma（伽马）差异

👉 不同屏幕有不同 gamma 曲线

```text
电脑：标准 gamma 2.2
TFT：厂家随便调的 😅
```

---

👉 表现：

* 同一个灰色

  * 电脑：正常灰
  * LCD：可能发白 or 发黑

---

# 🔌 四、驱动初始化参数（你这个很可能有影响）

ST7789 初始化里通常有：

```c
MADCTL
COLMOD
GAMMA
```

---

## 常见坑：

### ❌ 颜色顺序错（最常见）

```c
RGB ↔ BGR
```

👉 表现：

* 红色变蓝色
* 整体偏色

👉 解决：

```c
st7789_set_color_order(ST7789_BGR); // 或 RGB
```

---

### ❌ Gamma参数没调

很多驱动直接用默认值：

👉 显示效果 = 偏色 + 发灰

---

# 🔍 五、背光 & 亮度

👉 你的 LCD：

* LED 背光
* 没有亮度曲线校准

👉 笔记本：

* 自动亮度 / 色温调节

---

👉 表现：

```text
LCD：刺眼 / 偏冷
电脑：柔和 / 偏暖
```

---

# 👁 六、人眼错觉（这个也很真实）

👉 同一个颜色：

* 小屏：更鲜艳
* 大屏：更柔和

👉 环境光也影响：

* 白天 vs 晚上
* 暖光 vs 冷光

---

# 🧩 七、总结（按影响排序）

| 原因       | 影响程度  |
| -------- | ----- |
| 屏幕硬件差异   | ⭐⭐⭐⭐⭐ |
| RGB565色深 | ⭐⭐⭐⭐  |
| Gamma    | ⭐⭐⭐   |
| 驱动配置     | ⭐⭐⭐   |
| 背光       | ⭐⭐    |

---

# 🔧 八、你可以怎么优化？

## ✔ 1. 确认颜色顺序（必须做）

测试纯色：

```c
lv_color_hex(0xFF0000); // 红
```

👉 如果不红 → RGB/BGR错了

---

## ✔ 2. 做“颜色补偿”（工程常用）

比如：

```c
// 偏蓝 → 降蓝
// 偏暗 → 提亮
```

---

## ✔ 3. 用更高色深（如果可能）

👉 SPI基本没救（就是565）

👉 RGB屏（并口）可以上：

* RGB666
* RGB888

---

## ✔ 4. 调整 gamma（进阶）

👉 改 ST7789 寄存器（高级玩法）

---

# 🧠 最后一句人话总结

```text
你现在看到的“颜色不一样”

不是 bug
不是代码错
是因为：

👉 你在用“玩具屏”和“专业屏”对比
```

---

如果你下一步想搞：

👉 “让这个 LCD 看起来更像手机屏”
👉 “UI颜色调优（LVGL配色方案）”

我可以帮你做一套：

🔥 嵌入式屏幕专用 UI 调色方案（非常实战）

-----
------
# TFT LCD显示纯黑色变成灰色
这个现象**非常典型**，而且你这个描述很精准：

> 👉 “黑色看起来像灰色”

结论先说清楚：

```text
在 ST7789 这种 TFT 屏上 —— 这是正常现象（物理限制）
```

但我们可以把原因拆开讲清楚 + 给你优化办法 👇

---

# 🧠 一、核心原因：TFT 根本做不到“真黑”

## 📺 TFT LCD 的工作原理

![Image](https://i-blog.csdnimg.cn/img_convert/b6a3828146658a1ab1a0e69ad88f9abc.jpeg)

![Image](https://i-blog.csdnimg.cn/img_convert/2ce79e04447fb04f2dcf0640d3ce423d.jpeg)

![Image](https://i-blog.csdnimg.cn/img_convert/06736e27fdd16caa011e8431d8b8b364.jpeg)

![Image](https://i-blog.csdnimg.cn/img_convert/f369fb53f399c412bcb9b6c26f16a49c.jpeg)

![Image](https://i-blog.csdnimg.cn/img_convert/be7d04b5d58b3608f66697a188b60a66.jpeg)

![Image](https://i-blog.csdnimg.cn/img_convert/691d624e6dbaf2e73071ea33e9b70fde.jpeg)

👉 TFT LCD 是这样工作的：

1. 背后有一个 **LED背光（一直亮）**
2. 前面的液晶只是“挡光”

---

## ⚠️ 关键点来了：

```text
液晶 ≠ 完全挡光
```

👉 所以：

* 黑色 = “尽量挡住光”
* 但总有光漏出来

👉 看起来就是：

👉 **深灰色**

---

# 🔥 二、对比一下你笔记本（差距来源）

## 💻 如果是 OLED 屏

```text
黑色 = 像素直接关闭
👉 真·纯黑
```

---

## 💻 如果是 IPS 屏

👉 也有背光，但：

* 面板质量高
* 偏振片更好
* 漏光更少

👉 所以更接近黑

---

# ⚠️ 三、你这个项目的额外“减分项”

你用的是：

👉 SPI + ST7789（入门级 LCD）

---

## ❌ 问题叠加：

### 1️⃣ 面板质量低

* 漏光严重
* 对比度低

---

### 2️⃣ 背光太亮（很关键！）

👉 很多模块默认：

```text
背光 = 100%
```

👉 黑色直接被“照灰了”

---

### 3️⃣ Gamma 没调

👉 黑色区间不够压

---

# 👁 四、你看到“灰黑”的真实原因组合

```text
背光漏光
+ 面板对比度低
+ Gamma不准
+ 背光太亮
= 灰色黑
```

---

# 🔧 五、怎么优化（实用！）

## ✔ 方法1：调低背光（效果最明显）

如果你背光是PWM控制：

```c
// 降低占空比
```

👉 效果：

* 黑色更黑
* 对比度提升

👉 这是最有效的一招 ⭐⭐⭐⭐⭐

---

## ✔ 方法2：别用纯黑，用“伪黑”

```c
lv_color_hex(0x000000); ❌ 太死黑
```

👉 改成：

```c
lv_color_hex(0x0a0a0a); // 或 0x101010
```

👉 好处：

* 看起来更均匀
* 不会发灰发脏

👉 UI设计常用技巧

---

## ✔ 方法3：调 Gamma（进阶）

ST7789 支持 gamma 曲线：

👉 可以让暗部更“压黑”

但这个要改寄存器（稍复杂）

---

## ✔ 方法4：减少大面积纯黑

👉 工程经验：

```text
大面积纯黑 = 最容易暴露LCD缺陷
```

👉 改成：

* 深灰背景 + 卡片式UI

---

# 🎨 六、UI设计建议（很关键）

你现在做 LVGL，建议：

👉 别用这种：

```text
纯黑背景 + 白字 ❌
```

👉 改成：

```text
深灰背景 + 柔和白字 ✅
```

---

## 📱 类似这种风格

![Image](https://i-blog.csdnimg.cn/img_convert/15e006ac5a0e2436e1415bdf92d8217d.jpeg)

![Image](https://i-blog.csdnimg.cn/img_convert/f4c02927a6b04eb8e85e64bf535af70a.jpeg)

![Image](https://i-blog.csdnimg.cn/img_convert/ee4d1c0f9cd143dc1e983b304c2b9bd6.jpeg)

---

# 🧠 七、工程师总结（说人话）

```text
你这个“黑不黑”的问题：

不是代码问题
不是LVGL问题
不是SPI问题

👉 是屏幕物理极限
```

---

# 🚀 给你一个实战建议（最有用）

如果你这个项目是产品级：

👉 直接记住：

```text
TFT LCD 不适合做“纯黑UI”
```

---

# 👍 最后一句

👉 你现在已经开始注意“显示效果”了

这其实说明你已经从：

```text
能跑 → 好用 → 好看（UI/体验阶段）
```

在进阶了 😄

---

如果你想，我可以帮你：

👉 做一套 **LVGL暗黑主题（专门适配垃圾LCD也好看）**
👉 包括颜色、对比度、字体、布局

让你这个屏幕看起来直接提升一个档次 👍

-------
-------
-----

# IPS简介
## 🖥️ IPS 是啥意思？

**IPS = In-Plane Switching（平面内切换）**

👉 是一种 **LCD 液晶屏的显示技术类型**（就像一种“屏幕做法”）

---

# 📺 直观理解一下

![Image](https://i-blog.csdnimg.cn/img_convert/f836eadac95d965ea80dfb68879903b5.jpeg)

![Image](https://i-blog.csdnimg.cn/img_convert/ad56552b2b65244c54b733f77f557873.jpeg)

![Image](https://i-blog.csdnimg.cn/img_convert/0da4319ada76089077b12ed246bfaaff.jpeg)

👉 简单说区别在这：

* **IPS**：液晶分子“平着转”
* **传统TN屏**：液晶“扭着转”

---

# 🧠 用大白话解释

👉 你可以这么理解：

| 类型   | 类比           |
| ---- | ------------ |
| TN屏  | 门帘（角度一变就变形）  |
| IPS屏 | 百叶窗（怎么看都差不多） |

---

# 🔥 IPS 的核心特点

## ✔ 1. 可视角度大（最重要）

👉 从侧面看也不变色、不发白

---

## ✔ 2. 颜色更准

👉 更接近真实颜色
👉 做 UI / 看图片更舒服

---

## ✔ 3. 对比度比低端屏好

👉 黑色更“接近黑”（但仍不是真黑）

---

# ❌ 缺点也有

| 缺点   | 说明        |
| ---- | --------- |
| 成本高  | 比 TN 贵    |
| 响应稍慢 | 打游戏极限不如TN |
| 仍有漏光 | 黑色不如 OLED |

---

# 📊 和其他屏对比一下

| 类型   | 特点      |
| ---- | ------- |
| TN   | 便宜、差    |
| VA   | 对比度高    |
| IPS  | 综合最均衡 ⭐ |
| OLED | 真黑、最强   |

---

# 🔧 回到你这个项目（重点）

你用的 **ST7789 TFT**：

👉 有可能是：

* IPS（好一点的）
* 或“假IPS / 低端IPS”

---

## 所以你现在看到：

👉 黑色发灰
👉 颜色有点偏

👉 即使是 IPS 也会这样，因为：

```text
IPS ≠ 高端屏
IPS 只是“类型”，不是“品质保证”
```

---

# 🧠 一句话总结

```text
IPS = 一种让屏幕“角度更好、颜色更准”的LCD技术
```

但：

```text
你这个小屏 IPS ≈ 入门级 IPS 😄
```

---

# 👍 给你一个工程师判断方法

以后看到屏幕参数：

👉 重点看这几个，而不是只看 IPS：

* 色深（RGB565 / 666 / 888）
* 亮度（cd/m²）
* 对比度
* 是否校色

---

如果你想，我可以帮你：

👉 教你 **怎么判断你这块ST7789到底是不是“真IPS”**
👉 或者 **推荐几款显示效果明显更好的屏（适合ESP32）**


-----
------
-----
# LVGL优化显示问题
可以优化，而且你这段代码其实已经“接近正确”，但还有几个典型的 **LVGL v9 工程级优化点**，我给你直接改成一个更规范、更稳、更适合后续扩展 UI 的版本。

---

# 🧠 先说你这段代码的问题（重点）

### ❌ 1. 没必要再创建一层 father 容器

```c
father_lcd_ob = lv_obj_create(lv_screen_active());
```

👉 你现在是：

```text
screen（屏幕）
  └── father（你创建的）
       └── 子控件
```

👉 实际上可以直接用 screen：

```text
screen
  └── 子控件
```

---

### ❌ 2. 用 width/height 获取屏幕尺寸（不优雅）

```c
scr_act_lcd_width()
```

👉 v9 推荐：

```c
LV_PCT(100)
```

---

### ❌ 3. 样式是“散着写的”（后期难维护）

👉 工程里应该用 **style统一管理**

---

# ✅ 优化目标

👉 我给你做的是：

* 更干净结构
* 可扩展 UI
* 适合做产品级界面
* 顺便解决你“黑色发灰”的观感问题（UI层优化）

---

# 🚀 优化版本（推荐直接用）

```c
#include "lvgl.h"

LV_IMG_DECLARE(wls);

static const char *TAG = "LVGL_DEMO";

lv_obj_t *father_lcd_ob;

/* 🔥 建议：统一样式 */
static lv_style_t style_screen;

/* 初始化样式 */
static void style_init(void)
{
    lv_style_init(&style_screen);

    lv_style_set_bg_color(&style_screen, lv_color_hex(0x0f1113)); // ❗不是纯黑
    lv_style_set_bg_opa(&style_screen, LV_OPA_COVER);

    lv_style_set_border_width(&style_screen, 0);
    lv_style_set_pad_all(&style_screen, 0);
}

/* 创建基础界面 */
void creat_father_object(void)
{
    /* 🔥 获取当前屏幕 */
    lv_obj_t *scr = lv_screen_active();

    /* 🔥 应用样式到屏幕（推荐） */
    lv_obj_add_style(scr, &style_screen, LV_PART_MAIN);

    /* ❗ 如果你确实需要父容器（比如分层UI） */
    father_lcd_ob = lv_obj_create(scr);

    lv_obj_set_size(father_lcd_ob, LV_PCT(100), LV_PCT(100));
    lv_obj_center(father_lcd_ob);

    /* 去掉默认样式影响 */
    lv_obj_set_style_bg_opa(father_lcd_ob, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(father_lcd_ob, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(father_lcd_ob, 0, LV_PART_MAIN);
}
```

---

# 🔥 关键优化点解释（很重要）

---

## 🟢 1. 不用纯黑（解决你“灰黑”问题）

```c
lv_color_hex(0x0f1113);
```

👉 比 `0x000000` 更好：

* 不发灰
* 更均匀
* 更像手机UI

---

## 🟢 2. 样式集中管理（工程必备）

```c
static lv_style_t style_screen;
```

👉 好处：

* UI统一风格
* 后期一键换主题
* 不会到处写 style

---

## 🟢 3. 屏幕直接做背景（性能更好）

```c
lv_obj_add_style(scr, &style_screen, LV_PART_MAIN);
```

👉 少一层对象：

* 更省 RAM
* 更少重绘
* 更快

---

## 🟢 4. father 变“透明层”

```c
lv_obj_set_style_bg_opa(... LV_OPA_TRANSP);
```

👉 这样：

* father 只负责布局
* 背景由 screen 控制

---

# ⚠️ 你这个项目再给你几个“性能建议”（很关键）

---

## 🚨 1. SPI屏 + LVGL → 动画要克制

```c
lv_bar_set_value(..., LV_ANIM_ON);
```

👉 会：

* 刷屏频繁
* SPI带宽吃紧
* 卡顿

👉 建议：

```c
LV_ANIM_OFF（能不用就不用）
```

---

## 🚨 2. 尽量减少全屏刷新

👉 比如：

* 少用透明
* 少用阴影
* 少用渐变

---

## 🚨 3. 字体 / 图片占 RAM

```c
LV_IMG_DECLARE(wls);
```

👉 注意：

* 图片尽量压缩
* 用 indexed / alpha 小图

---

# 🧠 最后给你一个“工程级思路”

你现在已经进入这个阶段：

```text
能显示 → 做界面 → 开始优化
```

下一步应该是：

```text
UI结构设计 + 性能优化 + 视觉优化
```

---

# 🚀 如果你要继续往上走（强烈建议）

我可以帮你做一套：

👉 **ESP32 + ST7789 + LVGL 最优架构（实战版）**

包括：

* 双缓冲/单缓冲怎么选
* SPI刷新极限优化
* UI不卡顿方案
* 类似“手机界面”的UI结构

直接说一句：

👉 “给我整一套LVGL架构”

我给你上点真正工程级的东西 😎

好，这个我直接给你一套**能落地的工程级 LVGL 架构（ESP32 + ST7789 + SPI）**。不是讲概念，是你可以直接照着改项目的那种。

---

# 🧠 一句话目标

```text
不卡顿 / 易扩展 / 易维护 / 适合做产品
```

---

# 🏗️ 一、整体架构（核心思路）

👉 推荐结构：

```text
app/
 ├── main.c
 ├── lvgl_port/        ← LVGL移植层（驱动、tick、flush）
 ├── ui/               ← UI界面层
 │    ├── screen_main.c
 │    ├── screen_setting.c
 │    ├── ui_style.c
 │    └── ui.h
 ├── widget/           ← 可复用组件
 │    ├── ui_bar.c
 │    ├── ui_card.c
 │    └── ...
 └── bsp/
      ├── lcd.c
      ├── touch.c（如果有）
      └── backlight.c
```

---

# 🔧 二、LVGL 移植层（最关键）

## ✔ 1. 双缓冲（强烈推荐）

```c
#define LVGL_BUF_SIZE (240 * 40)   // 一行一行刷（推荐）

static lv_color_t buf1[LVGL_BUF_SIZE];
static lv_color_t buf2[LVGL_BUF_SIZE];

lv_display_set_buffers(disp, buf1, buf2,
                       LVGL_BUF_SIZE,
                       LV_DISPLAY_RENDER_MODE_PARTIAL);
```

---

## ✔ 2. flush_cb（核心性能点）

```c
void disp_flush(lv_display_t *disp,
                const lv_area_t *area,
                uint8_t *px_map)
{
    int w = area->x2 - area->x1 + 1;
    int h = area->y2 - area->y1 + 1;

    lcd_set_window(area->x1, area->y1, w, h);
    lcd_write_data(px_map, w * h * 2); // RGB565

    lv_display_flush_ready(disp);
}
```

👉 ⚠️ 重点：

* 必须 DMA（SPI）
* 不能阻塞太久

---

## ✔ 3. tick（系统心跳）

```c
esp_timer_start_periodic(timer, 2000); // 2ms

void lv_tick_task(void *arg)
{
    lv_tick_inc(2);
}
```

---

## ✔ 4. LVGL任务（单独线程）

```c
void lvgl_task(void *arg)
{
    while (1) {
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}
```

---

# 🎨 三、UI 层设计（重点）

## ✔ 1. 不要直接在 main 写 UI ❌

👉 用“屏幕模块化”

---

## ✔ screen_main.c

```c
#include "ui.h"

static lv_obj_t *scr;

void screen_main_create(void)
{
    scr = lv_obj_create(NULL);

    lv_obj_set_style_bg_color(scr, lv_color_hex(0x0f1113), 0);

    lv_obj_t *label = lv_label_create(scr);
    lv_label_set_text(label, "Hello LVGL");
    lv_obj_center(label);

    lv_screen_load(scr);
}
```

---

## ✔ screen 切换

```c
lv_screen_load_anim(new_scr,
                    LV_SCR_LOAD_ANIM_MOVE_LEFT,
                    200,
                    0,
                    false);
```

---

# 🧩 四、组件化（高级）

👉 比如你温度条：

---

## widget/ui_bar.c

```c
lv_obj_t *ui_temp_bar_create(lv_obj_t *parent)
{
    lv_obj_t *bar = lv_bar_create(parent);

    lv_obj_set_size(bar, 200, 20);
    lv_bar_set_range(bar, 0, 100);

    return bar;
}
```

---

👉 用的时候：

```c
lv_obj_t *bar = ui_temp_bar_create(scr);
```

---

# ⚡ 五、性能优化（你这个项目重点）

---

## 🚨 1. SPI 带宽限制（最大瓶颈）

👉 你是 ST7789：

```text
240x320 × 16bit ≈ 150KB / 帧
```

👉 SPI 40MHz：

```text
≈ 20~30 FPS 极限
```

---

## ✔ 优化策略：

### ✅ 局部刷新（必须）

```c
LV_DISPLAY_RENDER_MODE_PARTIAL
```

---

### ✅ 减少动画

```c
lv_bar_set_value(bar, val, LV_ANIM_OFF);
```

---

### ✅ 禁用阴影 / 渐变

```c
lv_obj_set_style_shadow_width(obj, 0, 0);
```

---

### ✅ 图片用 RGB565

不要用：

```text
ARGB8888 ❌（炸内存 + 慢）
```

---

# 🧠 六、UI设计建议（你刚好遇到的坑）

---

## ❌ 别用纯黑

```c
0x000000 ❌
```

👉 用：

```c
0x0f1113 ✅
```

---

## ✔ 卡片式UI（推荐）

```text
背景：深灰
卡片：稍亮灰
```

---

## ✔ 减少大面积刷新

👉 比如：

* 不要全屏动画
* 用局部组件更新

---

# 🔌 七、背光控制（强烈建议）

```c
ledc_set_duty(..., duty);
```

👉 降低亮度：

* 黑更黑
* UI更舒服

---

# 📊 八、最终结构总结

```text
LVGL架构核心：

驱动层（SPI DMA）
   ↓
LVGL移植层（flush/tick/task）
   ↓
UI层（screen）
   ↓
组件层（widget）
```

---

# 🚀 九、你现在这个水平，我给你一个升级路线

你现在已经：

```text
会用LVGL ✅
会写界面 ✅
```

下一步建议：

```text
1️⃣ UI结构化（你刚开始做）
2️⃣ 性能优化（正在踩坑）
3️⃣ 产品级UI（下一阶段）
```

---

# 🔥 最后一句（很关键）

```text
LVGL难的不是“会用API”
而是：

👉 怎么在SPI带宽限制下做“流畅UI”
```

---

如果你下一步想更狠一点，我可以帮你：

👉 **做一套“类似小米手环/智能屏 UI”的完整模板（ESP32可跑）**

包括：

* 页面切换
* 动画控制
* 状态栏
* 卡片UI

直接可以当产品原型用的那种 😎
