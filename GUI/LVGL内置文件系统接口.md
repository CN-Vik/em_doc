有，但要注意一句话：

**LVGL 没有“真正的内置文件系统”，它有的是“文件系统抽象层”。**

也就是说，LVGL 自己不负责管理 SD 卡、Flash、FATFS、LittleFS 这些东西。它只是提供一套统一接口，比如：

```c
lv_fs_open()
lv_fs_read()
lv_fs_close()
lv_fs_seek()
```

真正的数据来源还是你底层的：

```text
SD卡 + FATFS
SPI Flash + LittleFS
Linux 文件系统
ESP32 SPIFFS / LittleFS / SDMMC
自定义 Flash 资源区
```

LVGL 官方文档也说了，它有一个 File system abstraction module，可以把任意文件系统挂到 LVGL 上，并通过类似 `"S:/path/to/file.txt"` 的路径访问文件。([LVGL][1])

---

## 1. LVGL 图片有两种常见用法

### 方式一：图片转成 C 数组，编译进固件

这是 MCU 上最常用、最稳的方式。

```c
LV_IMG_DECLARE(my_logo);

lv_obj_t *img = lv_img_create(lv_scr_act());
lv_img_set_src(img, &my_logo);
```

LVGL v9 写法大概是：

```c
LV_IMAGE_DECLARE(my_logo);

lv_obj_t *img = lv_image_create(lv_screen_active());
lv_image_set_src(img, &my_logo);
```

这种方式**不需要文件系统**，图片数据直接在 Flash/ROM 里。

适合：

```text
logo
图标
按钮图片
小背景图
固定 UI 资源
```

---

### 方式二：图片放在 SD 卡 / Flash 文件系统里读取

例如：

```c
lv_obj_t *img = lv_img_create(lv_scr_act());
lv_img_set_src(img, "S:/image/logo.bin");
```

或者 LVGL v9：

```c
lv_obj_t *img = lv_image_create(lv_screen_active());
lv_image_set_src(img, "S:/image/logo.bin");
```

LVGL 文档里也明确说，图片 source 可以是 C 数组、外部文件，或者 symbol 文本。([LVGL][2])

但是这里有几个前提：

```text
1. 你的底层文件系统已经能正常读文件
2. 你要给 LVGL 注册文件系统驱动
3. 路径前缀，比如 S:/，要和你注册的盘符一致
4. 图片格式要是 LVGL 能解码的格式
```

---

## 2. LVGL 读文件的核心逻辑

可以理解成这样：

```text
lv_img_set_src(img, "S:/logo.bin")
        |
        v
LVGL 看到 S:
        |
        v
查找你注册的 S 盘文件系统驱动
        |
        v
调用 open/read/seek/close
        |
        v
底层 FATFS / LittleFS / POSIX 去真正读文件
        |
        v
LVGL 解码图片并显示
```

所以 **LVGL 不是直接读 SD 卡**，而是通过你适配的文件系统接口间接读取。

---

## 3. lv_conf.h 里面常见配置

比如用 FATFS：

```c
#define LV_USE_FS_FATFS 1

#if LV_USE_FS_FATFS
    #define LV_FS_FATFS_LETTER 'S'
    #define LV_FS_FATFS_CACHE_SIZE 0
#endif
```

官方文档列出的现成驱动包括 FATFS、STDIO、POSIX、WIN32、MEMFS、LittleFS、Arduino SD 等。([LVGL][1])

如果是 Linux / 嵌入式 Linux，可以用 POSIX 或 STDIO；如果是 MCU + SD 卡，常见就是 FATFS；如果是 MCU + SPI Flash，常见是 LittleFS 或自己写 lv_fs_drv_t 回调。

---

## 4. 图片格式不是随便放 PNG/JPG 就一定能显示

这个也很关键。

LVGL 最稳的是用官方图片转换器，把图片转成：

```text
C array：编译进代码
.bin：放进文件系统
```

LVGL v8 文档里说，转换器可以把 BMP/PNG/JPG 转成 C 文件或 `.bin` 文件；`.bin` 文件需要通过文件系统读取。([LVGL][3])

但是如果你想直接这样：

```c
lv_img_set_src(img, "S:/logo.png");
lv_img_set_src(img, "S:/photo.jpg");
```

那就要启用对应解码器。

例如：

```c
#define LV_USE_PNG   1
#define LV_USE_BMP   1
#define LV_USE_TJPGD 1
```

不同 LVGL 版本配置名字可能有差异。核心意思是：**PNG/JPG/BMP 文件本身需要对应 decoder**。LVGL 文档也说明，LVGL 不直接支持通用 PNG/JPG 这类格式，需要通过外部库或 image decoder 接口支持。([LVGL][4])

---

## 5. 对你这种嵌入式开发，推荐怎么用？

我建议你按这个优先级：

```text
小图标 / logo / 固定 UI 资源：
    转 C 数组，编译进固件，最稳定。

大量图片 / 可替换图片 / 相册 / 皮肤：
    放 SD 卡或 Flash 文件系统，用 LVGL fs 读取。

MCU 资源紧张：
    尽量用 LVGL 转换后的 .bin 或 RGB565 原始资源，少直接解 PNG/JPG。

ESP32-S3 这种资源稍微多一点：
    可以尝试 PNG/JPG，但要注意 RAM、解码速度、Flash/PSRAM。
```

---

## 6. 一个最小理解例子

假设你有 SD 卡，里面放：

```text
/logo.bin
```

LVGL 配置成盘符 `S`：

```c
#define LV_USE_FS_FATFS 1
#define LV_FS_FATFS_LETTER 'S'
```

显示图片：

```c
lv_obj_t *img = lv_img_create(lv_scr_act());
lv_img_set_src(img, "S:/logo.bin");
lv_obj_center(img);
```

如果是 LVGL 9：

```c
lv_obj_t *img = lv_image_create(lv_screen_active());
lv_image_set_src(img, "S:/logo.bin");
lv_obj_center(img);
```

---

一句话总结：

**LVGL 有文件系统抽象层，但没有真正替你实现 SD/FATFS/Flash 文件系统。读取图片可以，但你要先接好底层文件系统，再注册 LVGL fs driver，最后用 `lv_img_set_src()` / `lv_image_set_src()` 指向 `"S:/xxx.bin"` 或 `"S:/xxx.png"`。**

[1]: https://lvgl.io/docs/open/9.4/details/main-modules/fs "File System (lv_fs_drv) - LVGL 9.4 documentation"
[2]: https://lvgl.io/docs/open/9.0/widgets/image "Image (lv_image) — LVGL  documentation"
[3]: https://lvgl.io/docs/open/8.3/overview/image?utm_source=chatgpt.com "Images — LVGL documentation"
[4]: https://lvgl.io/docs/open/9.4/details/main-modules/image?utm_source=chatgpt.com "Images (lv_image) - LVGL 9.4 documentation"



------
------
----

可以。**LVGL 文件系统抽象层的使用，本质就是：给 LVGL 注册一个“盘符 + 一组文件操作函数”。**比如你注册 `'S'` 代表 SD 卡，那么 LVGL 里面就可以用：

```c
"S:/logo.bin"
"S:/image/test.png"
```

LVGL 会根据 `S:` 找到你注册的文件系统驱动，然后调用你的 `open/read/seek/tell/close`。官方文档也说明，LVGL 的文件系统通过一个标识字母区分不同文件系统，例如 SD 卡注册为 `S` 后，可以用 `"S:/path/to/file.txt"` 访问。([LVGL][1])

---

## 1. 整体流程

你可以这样理解：

```text
lv_image_set_src(img, "S:/logo.bin")
        |
        v
LVGL 看到路径前缀 S:
        |
        v
查找你注册的 S 盘驱动
        |
        v
调用 open_cb()
        |
        v
调用 read_cb()
        |
        v
调用 seek_cb() / tell_cb()
        |
        v
调用 close_cb()
        |
        v
图片显示出来
```

所以你需要做 4 件事：

```text
1. 底层文件系统先跑通
   比如 FatFS 的 f_open/f_read 能正常读取 SD 卡文件。

2. 给 LVGL 注册文件系统驱动
   设置盘符，比如 S。

3. 实现 open/read/seek/tell/close 回调

4. 用 lv_img_set_src 或 lv_image_set_src 读取图片
```

---

## 2. 最小注册代码框架

LVGL 的注册方式大概是这样。注意：`lv_fs_drv_t` 不能是局部临时变量，要用 `static` 或全局变量，因为 LVGL 后面还要继续用它。官方文档也特别强调这个点。([LVGL][1])

```c
#include "lvgl.h"

static void * my_fs_open(lv_fs_drv_t * drv, const char * path, lv_fs_mode_t mode);
static lv_fs_res_t my_fs_close(lv_fs_drv_t * drv, void * file_p);
static lv_fs_res_t my_fs_read(lv_fs_drv_t * drv, void * file_p, void * buf, uint32_t btr, uint32_t * br);
static lv_fs_res_t my_fs_seek(lv_fs_drv_t * drv, void * file_p, uint32_t pos, lv_fs_whence_t whence);
static lv_fs_res_t my_fs_tell(lv_fs_drv_t * drv, void * file_p, uint32_t * pos_p);

void lv_port_fs_init(void)
{
    static lv_fs_drv_t drv;        // 必须 static 或全局
    lv_fs_drv_init(&drv);          // 初始化默认值

    drv.letter = 'S';              // 注册成 S 盘，比如 SD 卡
    drv.cache_size = 0;            // 先关缓存，调通后再考虑开

    drv.open_cb  = my_fs_open;
    drv.close_cb = my_fs_close;
    drv.read_cb  = my_fs_read;
    drv.seek_cb  = my_fs_seek;
    drv.tell_cb  = my_fs_tell;

    lv_fs_drv_register(&drv);      // 注册到 LVGL
}
```

然后在初始化 LVGL 的地方调用：

```c
int main(void)
{
    lv_init();

    // 你的 LCD 初始化
    // 你的触摸初始化
    // 你的 SD 卡 / Flash / 文件系统初始化

    lv_port_fs_init();   // 注册 LVGL 文件系统

    // 创建 UI
}
```

---

## 3. 以 FatFS 为例：把 `S:` 接到 SD 卡

假设你底层用的是 FatFS，常见接口是：

```c
f_open()
f_read()
f_close()
f_lseek()
f_tell()
```

那么 LVGL 回调可以这样包一层：

```c
#include "lvgl.h"
#include "ff.h"       // FatFS 头文件

static void * my_fs_open(lv_fs_drv_t * drv, const char * path, lv_fs_mode_t mode)
{
    LV_UNUSED(drv);

    FIL * fp = lv_malloc(sizeof(FIL));
    if(fp == NULL) {
        return NULL;
    }

    BYTE flags = 0;

    if(mode == LV_FS_MODE_RD) {
        flags = FA_READ;
    }
    else if(mode == LV_FS_MODE_WR) {
        flags = FA_WRITE | FA_CREATE_ALWAYS;
    }
    else if(mode == (LV_FS_MODE_RD | LV_FS_MODE_WR)) {
        flags = FA_READ | FA_WRITE;
    }

    /*
     * 注意：
     * 如果你传入 "S:/logo.bin"
     * LVGL 传给这里的 path 通常是 "/logo.bin"
     *
     * 有些 FatFS 工程可以直接 f_open(fp, "/logo.bin", ...)
     * 有些工程需要 "0:/logo.bin"
     * 这个要看你 FatFS 挂载路径。
     */
    FRESULT res = f_open(fp, path, flags);
    if(res != FR_OK) {
        lv_free(fp);
        return NULL;
    }

    return fp;
}

static lv_fs_res_t my_fs_close(lv_fs_drv_t * drv, void * file_p)
{
    LV_UNUSED(drv);

    FIL * fp = (FIL *)file_p;

    FRESULT res = f_close(fp);
    lv_free(fp);

    return res == FR_OK ? LV_FS_RES_OK : LV_FS_RES_FS_ERR;
}

static lv_fs_res_t my_fs_read(lv_fs_drv_t * drv,
                              void * file_p,
                              void * buf,
                              uint32_t btr,
                              uint32_t * br)
{
    LV_UNUSED(drv);

    FIL * fp = (FIL *)file_p;
    UINT read_cnt = 0;

    FRESULT res = f_read(fp, buf, btr, &read_cnt);
    *br = read_cnt;

    return res == FR_OK ? LV_FS_RES_OK : LV_FS_RES_FS_ERR;
}

static lv_fs_res_t my_fs_seek(lv_fs_drv_t * drv,
                              void * file_p,
                              uint32_t pos,
                              lv_fs_whence_t whence)
{
    LV_UNUSED(drv);

    FIL * fp = (FIL *)file_p;
    FSIZE_t target_pos = pos;

    if(whence == LV_FS_SEEK_CUR) {
        target_pos = f_tell(fp) + pos;
    }
    else if(whence == LV_FS_SEEK_END) {
        target_pos = f_size(fp) + pos;
    }
    else {
        target_pos = pos;
    }

    FRESULT res = f_lseek(fp, target_pos);

    return res == FR_OK ? LV_FS_RES_OK : LV_FS_RES_FS_ERR;
}

static lv_fs_res_t my_fs_tell(lv_fs_drv_t * drv,
                              void * file_p,
                              uint32_t * pos_p)
{
    LV_UNUSED(drv);

    FIL * fp = (FIL *)file_p;
    *pos_p = f_tell(fp);

    return LV_FS_RES_OK;
}
```

这个代码的作用就是：

```text
LVGL 的 open_cb  ->  FatFS 的 f_open
LVGL 的 read_cb  ->  FatFS 的 f_read
LVGL 的 close_cb ->  FatFS 的 f_close
LVGL 的 seek_cb  ->  FatFS 的 f_lseek
LVGL 的 tell_cb  ->  FatFS 的 f_tell
```

官方模板里也是类似结构：先初始化存储设备和文件系统，然后设置 `fs_drv.letter`、`open_cb`、`close_cb`、`read_cb`、`write_cb`、`seek_cb`、`tell_cb`，最后 `lv_fs_drv_register()`。([GitHub][2])

---

## 4. 先测试 LVGL 能不能读文件

不要一上来就显示图片。先用 `lv_fs_open()` 测试。

```c
void lv_fs_test(void)
{
    lv_fs_file_t f;
    lv_fs_res_t res;

    res = lv_fs_open(&f, "S:/test.txt", LV_FS_MODE_RD);
    if(res != LV_FS_RES_OK) {
        LV_LOG_ERROR("lv_fs_open failed");
        return;
    }

    uint8_t buf[32];
    uint32_t read_num = 0;

    res = lv_fs_read(&f, buf, sizeof(buf) - 1, &read_num);
    if(res != LV_FS_RES_OK) {
        LV_LOG_ERROR("lv_fs_read failed");
        lv_fs_close(&f);
        return;
    }

    buf[read_num] = '\0';
    LV_LOG_USER("read: %s", buf);

    lv_fs_close(&f);
}
```

官方文档的测试例子也是用 `lv_fs_open()`、`lv_fs_read()`、`lv_fs_close()` 这一套流程。([LVGL][1])

---

## 5. 读图片怎么写？

### LVGL 8 写法

```c
lv_obj_t * img = lv_img_create(lv_scr_act());
lv_img_set_src(img, "S:/logo.bin");
lv_obj_center(img);
```

### LVGL 9 写法

```c
lv_obj_t * img = lv_image_create(lv_screen_active());
lv_image_set_src(img, "S:/logo.bin");
lv_obj_center(img);
```

但是要注意：**显示图片至少需要 `open/read/seek/tell/close` 这些回调。**LVGL 8 文档里明确写了，图片对象从文件打开时需要 `open`、`close`、`read`、`seek`、`tell` 回调。([LVGL][3])

---

## 6. 图片格式要注意

建议新手先用 LVGL 图片转换器转成 `.bin`，然后放进 SD 卡：

```text
SD 卡：
/logo.bin
```

然后：

```c
lv_img_set_src(img, "S:/logo.bin");        // LVGL 8
lv_image_set_src(img, "S:/logo.bin");      // LVGL 9
```

如果你直接放：

```c
lv_img_set_src(img, "S:/logo.png");
```

那还需要启用 PNG 解码器，比如：

```c
#define LV_USE_PNG 1
```

如果是 JPG，也要启用对应 JPG/TJPGD 解码器。否则 LVGL 虽然能读到文件，但**不会解码图片格式**。

---

## 7. 如果你用的是 LVGL 自带文件系统适配

有些 LVGL 版本已经带了现成的 FS 适配，比如：

```text
FATFS
STDIO
POSIX
WIN32
MEMFS
LITTLEFS
Arduino SD
```

LVGL 9 文档列出的现成驱动包括 FATFS、STDIO、POSIX、WIN32、MEMFS、LITTLEFS、Arduino SD 等。([LVGL][1])

这种情况下可以在 `lv_conf.h` 里开：

```c
#define LV_USE_FS_FATFS 1
#define LV_FS_FATFS_LETTER 'S'
#define LV_FS_FATFS_CACHE_SIZE 0
```

然后用：

```c
lv_fs_open(&f, "S:/test.txt", LV_FS_MODE_RD);
```

不过这里要注意：**LVGL 只是桥接层，你工程里仍然要有 FatFS 本体和 SD 卡驱动。**LVGL 文档也说明，文件系统扩展只是 LVGL 与 FATFS/STDIO/POSIX 等之间的桥，底层驱动和库仍然需要你提供。([LVGL][4])

---

## 8. 如果你是 ESP32 / Linux，用 POSIX 会更简单

比如 ESP32 挂载 SPIFFS 到：

```text
/spiffs
```

或者 Linux 上资源目录是：

```text
/home/vik/lvgl_assets/
```

可以用 POSIX/STDIO 方式。

概念上类似这样：

```c
#define LV_USE_FS_POSIX 1
#define LV_FS_POSIX_LETTER 'A'
#define LV_FS_POSIX_PATH "/spiffs/"
```

然后 LVGL 里写：

```c
lv_image_set_src(img, "A:/logo.bin");
```

实际访问的可能就是：

```text
/spiffs/logo.bin
```

这个方式在 ESP32-S3 + LVGL 上挺常见。

---

## 9. 最容易踩的坑

### 坑 1：底层 FatFS 没先测试

先单独测试：

```c
FIL f;
f_open(&f, "/test.txt", FA_READ);
f_read(&f, buf, len, &br);
f_close(&f);
```

这个不通，LVGL 肯定也不通。

---

### 坑 2：路径写错

有些版本示例写：

```c
"S:folder/file.txt"
```

有些写：

```c
"S:/folder/file.txt"
```

LVGL 9 文档推荐的是 `"S:/path/to/file.txt"` 这种形式。([LVGL][1])
如果你的 `open_cb` 里面拿到的是：

```c
"/logo.bin"
```

而 FatFS 需要：

```c
"0:/logo.bin"
```

那你就要在 `my_fs_open()` 里拼接真实路径。

---

### 坑 3：只实现了 open/read/close，没有实现 seek/tell

读普通文本可能没问题，但读图片可能失败。图片解码器经常需要跳转文件位置，所以 `seek_cb` 和 `tell_cb` 很重要。

---

### 坑 4：图片格式没开解码器

`.bin` 最稳。
`.png`、`.jpg` 需要额外 decoder。

---

### 坑 5：`lv_fs_drv_t drv` 写成局部变量

错误写法：

```c
void lv_port_fs_init(void)
{
    lv_fs_drv_t drv;     // 错误：函数退出后 drv 失效
    lv_fs_drv_init(&drv);
    ...
    lv_fs_drv_register(&drv);
}
```

正确写法：

```c
void lv_port_fs_init(void)
{
    static lv_fs_drv_t drv;   // 正确
    lv_fs_drv_init(&drv);
    ...
    lv_fs_drv_register(&drv);
}
```

---

## 10. 你可以按这个顺序调试

```text
第一步：底层 SD/FatFS 自己能读 test.txt

第二步：注册 LVGL fs driver，盘符用 S

第三步：用 lv_fs_open("S:/test.txt") 读文本

第四步：把 logo.bin 放 SD 卡根目录

第五步：lv_img_set_src(img, "S:/logo.bin")

第六步：如果要 PNG/JPG，再开对应图片解码器
```

一句话总结：

**LVGL 文件系统抽象层就是让你把底层的 `f_open/f_read/f_close` 包装成 LVGL 的 `open_cb/read_cb/close_cb`，注册一个盘符，比如 `S:`，之后 LVGL 读图片时就可以用 `"S:/logo.bin"` 这种路径。**

[1]: https://lvgl.io/docs/open/9.4/details/main-modules/fs "File System (lv_fs_drv) - LVGL 9.4 documentation"
[2]: https://github.com/lvgl/lvgl/blob/master/examples/porting/lv_port_fs_template.c "lvgl/examples/porting/lv_port_fs_template.c at master · lvgl/lvgl · GitHub"
[3]: https://lvgl.io/docs/open/8.3/overview/file-system "File system — LVGL  documentation"
[4]: https://lvgl.io/docs/open/9.1/libs/fs "File System Interfaces — LVGL  documentation"


------
-----
---

可以，**PNG 图片直接显示的核心流程是：**

```text id="chctlt"
底层文件系统能读 PNG 文件
        ↓
LVGL 注册文件系统盘符，比如 S:
        ↓
lv_conf.h 打开 PNG 解码器
        ↓
lv_img_set_src(img, "S:/xxx.png")
```

官方 LVGL 8 文档说明：打开 `LV_USE_PNG` 后，LVGL 会自动注册 PNG decoder，PNG 文件就可以像普通图片源一样使用；但如果 PNG 来自文件，还必须先注册文件系统驱动。PNG 解码时会把整张图解出来，RAM 大约需要 `宽 × 高 × 4` 字节。([LVGL][1])

---

# 1. LVGL 8 显示 PNG

## 第一步：打开 PNG 解码器

在 `lv_conf.h` 里打开：

```c id="a2i0og"
#define LV_USE_PNG 1
```

如果你的 PNG 是从 SD 卡 / Flash / Linux 文件系统读取，还要打开对应文件系统。

比如 FatFS：

```c id="9gy1xm"
#define LV_USE_FS_FATFS 1
#define LV_FS_FATFS_LETTER 'S'
#define LV_FS_FATFS_CACHE_SIZE 0
```

或者 Linux / POSIX：

```c id="yktf3p"
#define LV_USE_FS_POSIX 1
#define LV_FS_POSIX_LETTER 'A'
#define LV_FS_POSIX_PATH "/home/vik/lvgl_assets/"
#define LV_FS_POSIX_CACHE_SIZE 0
```

---

## 第二步：底层文件先能读

比如 SD 卡根目录放：

```text id="z5ernk"
logo.png
```

你要先确认 FatFS 自己能读：

```c id="y7uyy1"
FIL file;
FRESULT res;

res = f_open(&file, "0:/logo.png", FA_READ);
if(res == FR_OK) {
    printf("open png ok\r\n");
    f_close(&file);
} else {
    printf("open png failed: %d\r\n", res);
}
```

这个不通，LVGL 肯定也显示不了。

---

## 第三步：LVGL 显示 PNG

LVGL 8 写法：

```c id="obqgj9"
lv_obj_t * img = lv_img_create(lv_scr_act());

lv_img_set_src(img, "S:/logo.png");

lv_obj_center(img);
```

如果你用的是 Linux / POSIX，并且盘符是 `A`：

```c id="zkq5aj"
lv_obj_t * img = lv_img_create(lv_scr_act());

lv_img_set_src(img, "A:/logo.png");

lv_obj_center(img);
```

LVGL 官方 PNG 示例也是类似：创建 `lv_img`，然后 `lv_img_set_src(img, "A:.../xxx.png")`，前提是假设文件系统已经挂到 `A` 盘。([LVGL][1])

---

# 2. LVGL 9 显示 PNG

LVGL 9 里面图片对象 API 改名了：

```text id="rsc4as"
LVGL 8: lv_img_create()
LVGL 9: lv_image_create()

LVGL 8: lv_img_set_src()
LVGL 9: lv_image_set_src()
```

LVGL 9 的图片源可以是 C 数组、外部文件，或者符号文本；外部文件可以放在 SD 卡等存储里，然后用 `lv_image_set_src()` 设置路径。([LVGL][2])

---

## LVGL 9 配置

LVGL 9 里 PNG 常见有两种 decoder 选择：

```c id="f8i3td"
#define LV_USE_LODEPNG 1
```

或者：

```c id="fvvges"
#define LV_USE_LIBPNG 1
```

LVGL 9 的 decoder 文档里列出 PNG 可以用 `LV_USE_LODEPNG` 或 `LV_USE_LIBPNG`，其中 LodePNG 不需要外部库，libpng 需要外部库。([LVGL][3])

如果你用 `libpng`，LVGL 文档说明要在 `lv_conf.h` 中设置：

```c id="ug5z1y"
#define LV_USE_LIBPNG 1
```

并且每张 PNG 解码后也大约需要：

```text id="tbul66"
宽 × 高 × 4 字节 RAM
```

解码后的格式是 RGBA。([LVGL][4])

---

## LVGL 9 显示代码

```c id="lx3kx3"
lv_obj_t * img = lv_image_create(lv_screen_active());

lv_image_set_src(img, "S:/logo.png");

lv_obj_center(img);
```

---

# 3. 最小完整流程，按 SD 卡 FatFS 举例

假设你 SD 卡里有：

```text id="fv3o53"
0:/logo.png
```

你给 LVGL 注册成 `S:`，那么 LVGL 里路径写：

```text id="bxp3ge"
S:/logo.png
```

整体代码大概是：

```c id="2jdw4a"
void app_lvgl_test_png(void)
{
#if LVGL_VERSION_MAJOR >= 9
    lv_obj_t * img = lv_image_create(lv_screen_active());
    lv_image_set_src(img, "S:/logo.png");
    lv_obj_center(img);
#else
    lv_obj_t * img = lv_img_create(lv_scr_act());
    lv_img_set_src(img, "S:/logo.png");
    lv_obj_center(img);
#endif
}
```

---

# 4. 如果显示不出来，按这个顺序查

## 1. 先查 PNG decoder 有没有打开

LVGL 8：

```c id="eoe24j"
#define LV_USE_PNG 1
```

LVGL 9：

```c id="d1oi69"
#define LV_USE_LODEPNG 1
```

或者：

```c id="q4lv33"
#define LV_USE_LIBPNG 1
```

---

## 2. 再查 LVGL 文件系统能不能读

写个测试：

```c id="6d2qbu"
void lv_fs_png_test(void)
{
    lv_fs_file_t f;
    lv_fs_res_t res;

    res = lv_fs_open(&f, "S:/logo.png", LV_FS_MODE_RD);
    if(res != LV_FS_RES_OK) {
        LV_LOG_ERROR("open logo.png failed");
        return;
    }

    uint8_t buf[16];
    uint32_t br = 0;

    res = lv_fs_read(&f, buf, sizeof(buf), &br);
    if(res == LV_FS_RES_OK) {
        LV_LOG_USER("read png ok, read %lu bytes", (unsigned long)br);
    }

    lv_fs_close(&f);
}
```

PNG 文件开头一般是：

```text id="69khcz"
89 50 4E 47 0D 0A 1A 0A
```

其中 `50 4E 47` 就是 ASCII 的 `PNG`。

---

## 3. 查路径

这个最容易错。

你写：

```c id="3m9a0g"
lv_img_set_src(img, "S:/logo.png");
```

LVGL 的文件系统回调里拿到的 `path` 可能是：

```text id="9xyhac"
/logo.png
```

但是你的 FatFS 可能需要：

```text id="1uk6cu"
0:/logo.png
```

这时候你要在 `open_cb` 里面做路径转换：

```c id="s2n59i"
static void * my_fs_open(lv_fs_drv_t * drv, const char * path, lv_fs_mode_t mode)
{
    LV_UNUSED(drv);

    FIL * fp = lv_malloc(sizeof(FIL));
    if(fp == NULL) return NULL;

    char full_path[128];

    /*
     * LVGL 传进来的 path 可能是 "/logo.png"
     * FatFS 实际需要 "0:/logo.png"
     */
    snprintf(full_path, sizeof(full_path), "0:%s", path);

    BYTE flags = FA_READ;

    FRESULT res = f_open(fp, full_path, flags);
    if(res != FR_OK) {
        lv_free(fp);
        return NULL;
    }

    return fp;
}
```

---

# 5. PNG 在 MCU 上的注意点

PNG 好处是方便、支持透明，但在 MCU 上有几个缺点：

```text id="zcb34x"
1. 解码慢
2. 占 RAM 大
3. 大图容易显示失败
4. 首次显示可能卡顿
```

比如一张 `480 × 800` 的 PNG，解码 RAM 大约是：

```text id="34gpyl"
480 × 800 × 4 = 1,536,000 字节 ≈ 1.46 MB
```

如果你的 MCU 没有 PSRAM / SDRAM，这种大图基本很难直接 PNG 解码显示。

所以嵌入式里更推荐：

```text id="jii364"
小图标：PNG 可以
大背景图：建议转成 LVGL .bin 或 RGB565
资源固定：建议转成 C array
资源可替换：放文件系统，用 .bin 或 PNG
```

---

# 6. 最推荐你的做法

你作为嵌入式新手，建议先这样做：

```text id="hbezbu"
第一步：先用 SD/FatFS 读通 logo.png

第二步：用 lv_fs_open("S:/logo.png") 测试 LVGL 文件系统读通

第三步：打开 PNG decoder

第四步：用 lv_img_set_src(img, "S:/logo.png") 显示

第五步：如果 RAM 不够，再把 PNG 转成 LVGL .bin
```

最小显示代码就是：

### LVGL 8

```c id="uk5vry"
lv_obj_t * img = lv_img_create(lv_scr_act());
lv_img_set_src(img, "S:/logo.png");
lv_obj_center(img);
```

### LVGL 9

```c id="6sgai2"
lv_obj_t * img = lv_image_create(lv_screen_active());
lv_image_set_src(img, "S:/logo.png");
lv_obj_center(img);
```

一句话总结：**PNG 不是只靠 `lv_img_set_src()` 就行，必须同时满足：文件系统路径能读到 PNG + LVGL PNG decoder 已启用 + RAM 足够解码整张 PNG。**

[1]: https://lvgl.io/docs/open/8.3/libs/png "PNG decoder — LVGL  documentation"
[2]: https://lvgl.io/docs/open/9.0/widgets/image "Image (lv_image) — LVGL  documentation"
[3]: https://lvgl.io/docs/open/main-modules/images/decoders "Image Decoders | LVGL Open"
[4]: https://lvgl.io/docs/open/9.3/details/libs/libpng "libpng Decoder - LVGL 9.3 documentation"

