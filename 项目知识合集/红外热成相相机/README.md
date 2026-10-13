# 项目知识介绍

>我感觉我这个不就是个图像放大缩小的问题吗？？直接把OpenCV里边的图像处理算法给抄过来参考参考不就行了吗？ MLX90640获取到矩阵温度值之后，进行数据滤波，进行RGB颜色值映射，这一步只是32*24像素大小的图片，然后经过放大，放大到240*\*280 ，我这个思路对吗？？？ 用LVGL库也可以放大图像吧？？

你的思路基本完全正确！而且你这个项目根本不需要为了图像放大引入整套 OpenCV。 你用 ESP32-S3，直接实现 OpenCV 中的图像缩放算法就行。更重要的是，我现在找到了你现有代码中一个明确的几何问题，比之前一味调滤波更值得优先处理。

我查看了你给的 RP2040 项目页面，它同样使用 MLX90640、240×280 的 ST7789 屏幕，并且包含独立的热成像程序目录 `thermal_app`。另一个 ESP32-S3 热成像开源项目也采用双线性插值、温度滤波和伪彩色显示这条路线。

[image](https://www.google.com/s2/favicons?domain=https://github.com\&sz=32)

GitHub

+1



## 一、标准的热成像显示流程是什么？

你的想法没有问题，不过有一个细节：最好先插值温度或温度归一化值，再做 RGB 颜色映射。

MLX90640 获取温度

## 32 × 24 = 768 个像素

温度处理

坏点修正 + 适度滤波 + 温度范围映射

图像缩放 / 插值

32 × 24 → 目标显示分辨率

伪彩色 LUT

温度值 → RGB565 颜色

LVGL + ST7789

显示最终热像

为什么不直接先转换成 RGB，再插值？

因为温度本身是有物理意义的标量。先把温度插值，再查颜色表，可以避免直接对不同颜色进行 RGB 混合所产生的不必要色彩过渡。

你当前代码实际上已经比较接近这条路线：先由 `temperature_to_norm_u8()` 把温度归一化，再在 `bilinear_scale_fullscreen_1to1()` 中插值，最后查找 Ironbow LUT。所以问题不在于整个处理思路错了，而是具体缩放几何、滤波参数和显示策略需要核实。

## 二、OpenCV 的哪些算法适合你？

OpenCV 的 `resize()` 提供最近邻、双线性、双三次和 Lanczos 等插值方式。

[image](https://www.google.com/s2/favicons?domain=https://docs.opencv.org\&sz=32)

OpenCV 文档



[PPT - Image Sampling PowerPoint Presentation, free download - ID:2857904](https://images.openai.com/static-rsc-4/XbD9Q2aZwFQjca-fJwgMt8AvgHxoSMyBOh9h4G8ZbcndjBdB9ShjDSh3_QZkFTy4mkzUVKEfqvWRv8PlQwz0Y49dz44yOvTY9c6PwsahgBwz-QFwWS-sXW3xgBJbFBYw7gFzoOqyu3so-ihO0qwnuzIXoGq33Q_9GjyFDLTUo9E?purpose=inline)

1\. 最近邻插值（INTER_NEAREST）

直接复制最近的温度像素，不引入额外的中间温度。图像有方块感，但是特别适合用来排查：原始手指轮廓到底有没有被算法抹掉。

优先用于调试

[Checking your browser - reCAPTCHA](https://images.openai.com/static-rsc-4/wsne5qVr8ODdXUz0TZYHW-Zp0_DmhJDgul8Z_k5SjHidc3Nc6v67MNfbH_0Z4Ai2E6pt-_7znITpSrN-IdhYdMKr5YbW9rMGbI0M936_Rs11tvUwKRom4bA2OPc3B1gP7emDeY-AeiSnau3JedozOQ3-pgtSxSDbHYzTPmXVM70?purpose=inline)

2\. 双线性插值（INTER_LINEAR）

根据周围四个点进行加权平均，画面比较自然，计算量也很低。适合作为热成像的基础方案，但窄小的手指间冷区有可能被平滑。

适合作为默认方案

[Remastering Colin McRae Rally 3 with SilentPatch | Silent’s Blog](https://images.openai.com/static-rsc-4/iR9KxOfcB33LQ1TAjCHuDCP_2kL8a69bjZ_EMVzotsWaNNP8-EbnpyvQpQ140aJHV96hj7c1bGEhO24QQZgLc-YghcUI2ENxcIXW4MekFiZwx6fCIWAGOXSpmZBbZqGV4nF8Qr5Cr1JN0lM9Fg6KTOSancNd9rH4f4I3sftL2D4?purpose=inline)

3\. 双三次插值（INTER_CUBIC）

使用更多邻域像素计算中间值，通常可以形成更平滑的过渡，但也不保证轮廓更清楚，甚至可能出现过冲。应当与双线性对比后再决定是否使用。

我的建议是：先用最近邻确认轮廓，再对比双线性。暂时不要上 Lanczos、复杂锐化或者多层滤波。

你可以在电脑端用 OpenCV 对同一帧温度矩阵进行三种缩放对照，但没有必要把整个 OpenCV 库搬进 ESP-IDF。ESP32-S3 只需要处理 768 个源像素，自己实现缩放算法完全够用。

## 三、我发现了你代码里一个真正值得检查的问题

你的传感器输入是：

```
#define MLX_RAW_W  32
#define MLX_RAW_H  24
```

而目标图像是：

```
#define MLX_INTERP_W  240
#define MLX_INTERP_H  280
```

原始矩阵比例为 4:3，LCD 则是 240:280 的竖屏比例。

正常情况下，有三种选择：

方案 A：保留完整视场和原始比例

240 × 180

输出 240×180 的热图，上下留给状态栏或黑色留白。不拉伸、不裁剪。

方案 B：铺满整个屏幕

240 × 280

使用独立的横向、纵向缩放比例。可以保留全部视场，但物体会发生比例失真。

方案 C：保持比例并铺满高度

裁切

保持像素比例，但左右裁剪视场。你目前的缩放函数就属于这一类。

这里是关键：你当前的 `s_scale_fp` 按目标高度 280 计算，随后 X 方向也沿用相同的比例，再通过 `s_x_start_fp` 取中间区域。结果是，原始 32 列中大约只使用中间 20 列，左右各裁掉约 6 列。

这来自你的实际代码，不是猜测。它会显著缩小可见视场，让手在画面中显得更大、更挤。至于指缝究竟被抹掉多少，还需要对照原始温度矩阵才能确定。

我的首选是先采用方案 A，或者根据你希望的握持方向将图像旋转后保持完整视场。这样能先排除裁切对手部显示的干扰。

## 四、LVGL 本身能放大图像吗？

可以。你现在的工程使用的是 LVGL 8 风格的图像接口，支持 `lv_img_set_zoom()` 和 `lv_img_set_antialias()`。 官方文档说明，缩放系数 256 表示 1 倍，512 表示 2 倍；可以启用抗锯齿改善变换的视觉平滑度，但计算会更慢。

[image](https://www.google.com/s2/favicons?domain=https://lvgl.io\&sz=32)

LVGL documentation

+1



例如，若把图像源改成 32×24 的 RGB565 图像：

```
lv_img_set_src(img_thermal, &thermal_img_desc);

/* 7.5 倍缩放：32×24 -> 240×180 */
lv_img_set_zoom(img_thermal, 1920);

/* 先关闭抗锯齿，观察原始轮廓 */
lv_img_set_antialias(img_thermal, false);
```

这里 `1920 = 256 × 7.5`。实际显示位置还需要按旋转中心和界面布局调整。

不过，你要注意两个限制：

- LVGL 的缩放只负责显示变换，不会让 32×24 变成真正的高分辨率温度图。
- LVGL 8 的 `lv_img_set_zoom()` 使用统一缩放比例，不能单独设置 X、Y 两个不同的缩放比例。因此它适合保持比例放大，而不是直接把 32×24 填满 240×280。

你当前的图像描述符本来就定义为 240×280，`lv_img_set_zoom(img_thermal, 256)` 代表不额外放大，真正的放大工作是你自己的 `bilinear_scale_fullscreen_1to1()` 在做。

## 我的最终建议

先不要再堆滤波算法，也不要改 LCD 初始化。

第一步，只做一个纯显示对照：保留原来的传感器读取和温度处理，分别用最近邻和双线性插值显示同一帧、同一温度范围、同一完整视场。

如果最近邻下就能看到手指间的冷区，而双线性下冷区消失，才有明确证据说明插值是重要原因；如果两种方式都糊成一团，就应该继续查看原始温度矩阵，而不是继续换插值算法。

这样才能用一次最小实验把问题定位清楚，而不是再改一大堆代码碰运气。


--------------
-------------
--------------
# 看这个热成像相机还有人结合RGB图像传感器来做的，这个RGB图像传感器加进来有啥用吗？？
有用，而且如果你的目标是让 MLX90640 热成像相机的物体轮廓更清晰，增加一个 RGB 可见光摄像头，确实是一条值得考虑的路线。 不过，它解决的是“看清物体轮廓”的问题，并不能直接提高红外传感器的真实分辨率。

FLIR 的热成像设备就采用了类似思路，称为 MSX（多光谱动态成像）：从可见光图像中提取物体边缘、文字和线条，再叠加到热图上。

[image](https://www.google.com/s2/favicons?domain=https://www.flir.cn\&sz=32)

Teledyne FLIR

+1



## 一、RGB 摄像头加进来，究竟有什么用？

[FLIR TG165-X Thermal Imaging Camera with Bullseye Laser: Commercial Grade Infrared Camera for Building Inspection, HVAC and Electrical - Walmart.ca](https://images.openai.com/static-rsc-4/wxbJuzqvkdrvYv-MXmhyRa1SLVaK6a1f7rfv5sYAYmZCqualbFW5O05QewDx6fKVC65BBcFz2xp3bD1FF22OcgoHjEhn8rwSrcMLyqpQF_Gm7oM0sJIvRLu3Kj4qLDMa5GRUfaxh9BLpKk3dAHSshxcLcPk44O5dLY1d0WiPINg?purpose=inline)

[FLIR E5 Thermal Imaging Camera - Camax UK Ltd](https://images.openai.com/static-rsc-4/eSDrXlaP2xQIdseSCHHr7muKuEe4d1OHzzGwccLXYsklc9qU-H3zbprNATyrolGVD_octSeESeHDfD0649UJgbFyM1gAgkrmADEX-GABmENE6A9TDfUnafOyLL2fdXFk3x5CIiK7ykHft_uZMnjyQQ6briazBJHdGno9zhwlpjM?purpose=inline)

[Что такое FLIR MSX и как работает технология мультиспектральной съёмки](https://images.openai.com/static-rsc-4/rh9iRI6mBHfLw8DuizDWk0pN_ndpTQ1TmBgJ-5gCX_O3tTjSzOZ4m7wDw-DaLXQNF1861ojj9sbBt7iV0vc5FGnKdG519Jum8aVRbE5UD56uye9AKKruIWP4_w_P3Sw3132kQcu8rN7rDUVpQOZhett6jRdeBUzspeZNapamfO0?purpose=inline)

6

你的现有设备有两个不同的能力：

- MLX90640： 告诉你每个采样点的温度是多少。
- RGB 摄像头： 告诉你物体的可见形状、边缘、纹理和颜色。

两者结合，可以实现下面三种效果。

[Modified Flir E4 Thermal Imaging Camera “Before” and “After” Image Comparison](https://images.openai.com/static-rsc-4/OpUOZeZ4o6nyYfXI-13Ab1JVodS-FdjmNEa6XdfsMWRck54sIPZ2Yi32rIXPEHc012kFSMH7R51irIBDOB2dEbBeV4TkWRM2kWpxX9TPj9cor3ytVcZ4iVnpP4ap2X8mISyKts5jA7JyA1QdKi1QFCia9on-6MLWe9puSqEi2O0?purpose=inline)

1\. 纯热成像模式

只显示 MLX90640 的温度分布。优点是画面里的颜色与温度相对应；缺点是分辨率只有 32×24，手指、细线、小物体容易融合。

[GhostStop Ghost Hunting Equipment - FLIR TG267 Thermal Imaging Camera](https://images.openai.com/static-rsc-4/mKiaxQZ0IQ8OttR8peNx5ormUuhXBpahbFd-YPl3EInkG3wKRmVBUgMshRrjAQcEIhYbQHuOk3hVY40hIh1JfLc0O9RrAQ_W-kMBh56KmUf55ppmfg5IALQljfgPBerd_6wQ2uNa2I5VFHdYi9DAUr7MKd1uOguJ_e0PoWJ1rOU?purpose=inline)

2\. RGB 轮廓增强模式（最值得你研究）

先计算 RGB 图像中的物体边缘，再把这些边缘叠加到热图上。手掌外轮廓、指缝和物体边界可能变得清楚很多，同时保留原来的温度颜色。

[Handheld Thermal Camera | Thermal Scanner | Thermal Master](https://images.openai.com/static-rsc-4/y2vu2c31_oKuj0-rlj5ZexKnp1bVRReLCVyAPJY2kH19xTreO3Ul7u49ccIYQovpNrzd2hgIJfxIDdAnchwr5DagKeonPs4dv0J8mwIsh0AoBYubnHkDleQ_0WG3eV2GC96my6iyU1jZgp2jCn6AelznvyKxqo30ZbNzBOl0mlg?purpose=inline)

3\. 双光融合模式

将 RGB 画面和热成像画面按照一定透明度混合，或者采用画中画。这样方便辨认热源对应的真实物体，但如果混合比例不合适，热区的颜色和对比度会受到影响。

## 二、它能解决你现在的“手掌成一团”吗？

可以改善视觉轮廓，但不能保证手指的真实热像细节全部恢复。

举个例子：

假设 MLX90640 某一区域采集到的温度已经是：

```
35  35  34  35
35  35  34  35
35  35  35  35
```

即使你换成更好的插值算法，也无法确定这些像素之间真实的手指间隙在哪里。

但是，RGB 摄像头可能拍到清楚的手指边缘。对可见光图像进行边缘检测后，可以在热图上叠加相应的线条。

最后的画面就可能变成：

- 热成像底色仍然表示温度。
- 手指轮廓由 RGB 图像提供。
- 物体边缘看起来更清楚。

这正是 MSX 类技术的价值。FLIR 的资料也明确说明，这种做法可以为低分辨率热图增加可见细节，同时保留底层的热像温度数据。

[image](https://www.google.com/s2/favicons?domain=https://www.flir.cn\&sz=32)

Teledyne FLIR

+1



需要特别注意：RGB 边缘不等于温度边缘。 在热成像画面上叠加白色轮廓，只是增强视觉辨识度，并不代表 MLX90640 测到了这条边缘上的独立温度。

## 三、你的 ESP32-S3 项目适合怎样实现？

我建议不要一开始就做复杂的双光融合，而是先做一个轻量的 MSX 风格功能。

MLX90640

32×24 温度矩阵

温度处理、插值、伪彩色映射

同时采集另一条图像通道

RGB 摄像头

可见光图像

边缘提取

灰度 + Sobel 等

图像对齐 + 轮廓叠加

只把 RGB 边缘画到热图上，不修改温度矩阵

ST7789 240×280 显示屏

实现时有三个关键点：

第一，摄像头视场和图像对齐。 RGB 摄像头和 MLX90640 的镜头位置不同，同一个物体在两张图里的位置也可能不同。需要匹配视场，并根据实际距离进行标定或校正；否则会出现轮廓偏移、双重边缘。

[image](https://www.google.com/s2/favicons?domain=https://support.flir.com\&sz=32)

FLIR支持

+1



第二，不能只把两张图片直接叠起来。 RGB 图像建议先转灰度，再用 Sobel 等简单算法提取边缘，最后叠加细线。这样比整张 RGB 与伪彩色热图混合更适合保留温度信息。

第三，ESP32-S3 的资源要考虑。 需要额外的摄像头接口、GPIO、图像缓冲区和采集任务。第一版可以把 RGB 图像缩小到适合屏幕的尺寸，再做灰度与边缘处理，不必在 MCU 上运行完整 OpenCV。

## 四、到底值不值得加？

值得加：如果你的目标是做成完整的手持式热成像仪。

可以获得更容易辨认的轮廓、文字和物体细节，也方便观察热点对应哪个部件。

暂时不必加：如果你只是想把现有纯热图算法调好。

应该先解决当前的视场裁切、温度映射和插值显示问题。否则，即使增加 RGB 摄像头，也只是用可见光轮廓掩盖原有热图的问题。

我的建议：先把纯热成像调到稳定，再考虑加入 RGB 边缘增强。 两种模式都保留，平时看温度分布用纯热图，需要清晰辨认物体时切换到轮廓增强模式。这样最符合你目前这台 ESP32-S3 热成像相机的开发方向。