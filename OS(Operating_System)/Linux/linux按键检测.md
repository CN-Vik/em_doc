按键检测在嵌入式 Linux 里，推荐路线是：

```text
GPIO 按键硬件
  ↓
设备树描述按键接在哪个 GPIO
  ↓
Linux gpio-keys 驱动
  ↓
Linux input 子系统
  ↓
/dev/input/eventX
  ↓
你的应用程序读取按键事件
```

也就是说，**不是应用程序直接读 GPIO 寄存器**，而是让 Linux 内核把按键抽象成标准输入设备。

---

# 1. MCU 里按键检测是怎么做的？

你以前做 MCU，可能这样写：

```c
if (gpio_input_bit_get(GPIOA, GPIO_PIN_0) == RESET) {
    delay_ms(20);
    if (gpio_input_bit_get(GPIOA, GPIO_PIN_0) == RESET) {
        key_pressed = 1;
    }
}
```

或者用外部中断：

```text
GPIO 下降沿中断
  ↓
进入 EXTI_IRQHandler
  ↓
消抖
  ↓
设置按键标志
```

MCU 里你自己负责：

```text
GPIO 初始化
上拉/下拉
中断配置
消抖
短按/长按/双击
```

---

# 2. Linux 里推荐怎么做？

Linux 里推荐用：

```text
gpio-keys 驱动
```

它是内核自带的通用 GPIO 按键驱动。

它帮你处理：

```text
GPIO 输入
中断触发
按下/松开事件
基础消抖
注册 input 设备
生成 /dev/input/eventX
```

然后你的应用程序只需要读：

```text
/dev/input/event0
/dev/input/event1
...
```

按键事件会变成标准 Linux input event。

---

# 3. 按键驱动整体结构

完整链路是：

```text
实体按键
  ↓
GPIO 引脚电平变化
  ↓
X2600 GPIO 控制器
  ↓
Linux GPIO controller driver
  ↓
gpio-keys 驱动
  ↓
input 子系统
  ↓
/dev/input/eventX
  ↓
应用程序 read()
```

所以你需要做的主要是：

```text
1. 确认按键硬件接到哪个 GPIO
2. 在设备树里添加 gpio-keys 节点
3. 打开内核 CONFIG_KEYBOARD_GPIO
4. 编译烧录 kernel/dtb/rootfs
5. 在板子上测试 /dev/input/eventX
6. 应用程序读取 event 事件
```

---

# 4. 设备树怎么写？

假设你的按键接法是：

```text
按键一端接 GPIO
另一端接 GND
GPIO 内部或外部上拉
按下时 GPIO = 0
松开时 GPIO = 1
```

这就是最常见的：

```text
低电平有效按键
```

设备树可以类似这样写：

```dts
#include <dt-bindings/input/input.h>
#include <dt-bindings/gpio/gpio.h>

gpio_keys {
    compatible = "gpio-keys";
    status = "okay";

    key_power {
        label = "power_key";
        gpios = <&gpioa 5 GPIO_ACTIVE_LOW>;
        linux,code = <KEY_POWER>;
        debounce-interval = <20>;
        wakeup-source;
    };

    key_menu {
        label = "menu_key";
        gpios = <&gpiob 3 GPIO_ACTIVE_LOW>;
        linux,code = <KEY_MENU>;
        debounce-interval = <20>;
    };

    key_back {
        label = "back_key";
        gpios = <&gpioc 7 GPIO_ACTIVE_LOW>;
        linux,code = <KEY_BACK>;
        debounce-interval = <20>;
    };
};
```

这里面重点是：

| 字段                         | 作用                    |
| -------------------------- | --------------------- |
| `compatible = "gpio-keys"` | 使用 Linux 通用 GPIO 按键驱动 |
| `label`                    | 按键名字                  |
| `gpios`                    | 按键接在哪个 GPIO           |
| `GPIO_ACTIVE_LOW`          | 低电平表示按下               |
| `linux,code`               | 上报给 Linux input 系统的键值 |
| `debounce-interval`        | 消抖时间，单位 ms            |
| `wakeup-source`            | 支持系统休眠后用这个按键唤醒        |

---

# 5. `linux,code` 是什么？

`linux,code` 是 Linux input 子系统里的按键码。

比如：

```text
KEY_POWER
KEY_ENTER
KEY_UP
KEY_DOWN
KEY_LEFT
KEY_RIGHT
KEY_MENU
KEY_BACK
KEY_HOME
KEY_VOLUMEUP
KEY_VOLUMEDOWN
```

这些定义一般在内核头文件：

```c
include/uapi/linux/input-event-codes.h
```

例如：

```dts
linux,code = <KEY_ENTER>;
```

表示这个 GPIO 按键按下时，Linux 上报一个：

```text
ENTER 键事件
```

应用层收到的是：

```text
type = EV_KEY
code = KEY_ENTER
value = 1 / 0
```

---

# 6. `GPIO_ACTIVE_LOW` 是什么意思？

这个特别重要。

如果你的按键电路是：

```text
GPIO ---- 按键 ---- GND
GPIO 上拉
```

那么：

```text
松开：GPIO = 1
按下：GPIO = 0
```

这就是低电平有效：

```dts
GPIO_ACTIVE_LOW
```

如果你的按键是：

```text
GPIO ---- 按键 ---- VCC
GPIO 下拉
```

那么：

```text
松开：GPIO = 0
按下：GPIO = 1
```

这就是高电平有效：

```dts
GPIO_ACTIVE_HIGH
```

如果这个写反了，你会看到现象：

```text
没按下却显示按下
按下反而显示松开
```

---

# 7. 内核配置要开哪些？

一般需要这些：

```text
CONFIG_INPUT
CONFIG_INPUT_EVDEV
CONFIG_KEYBOARD_GPIO
CONFIG_GPIOLIB
```

如果要在用户态出现 `/dev/input/eventX`，`CONFIG_INPUT_EVDEV` 很重要。

你可以在 SDK 里查：

```bash
grep -R "CONFIG_KEYBOARD_GPIO" -n kernel/
grep -R "CONFIG_INPUT_EVDEV" -n kernel/
```

如果没开，进入 kernel 配置：

```bash
make kernel-menuconfig
```

或者 SDK 自己有类似配置命令，具体看 X2600 的 build 系统。

内核菜单里大概路径是：

```text
Device Drivers
  Input device support
    Event interface
    Keyboards
      GPIO Buttons
```

---

# 8. 编译烧录后怎么测试？

烧录新 kernel/dtb 后，板子启动。

先看日志：

```bash
dmesg | grep -i gpio
dmesg | grep -i key
dmesg | grep -i input
```

然后看 input 设备：

```bash
cat /proc/bus/input/devices
```

你可能看到类似：

```text
I: Bus=0019 Vendor=0001 Product=0001 Version=0100
N: Name="gpio_keys"
P: Phys=gpio-keys/input0
H: Handlers=kbd event0
B: EV=3
B: KEY=...
```

这里重点看：

```text
Name="gpio_keys"
Handlers=... event0
```

说明按键设备对应的是：

```text
/dev/input/event0
```

---

# 9. 用 evtest 测试按键

如果 rootfs 里有 `evtest`，直接：

```bash
evtest /dev/input/event0
```

然后按键，应该看到类似：

```text
Event: time 123.456, type 1 (EV_KEY), code 116 (KEY_POWER), value 1
Event: time 123.456, type 0 (EV_SYN), code 0 (SYN_REPORT), value 0

Event: time 124.000, type 1 (EV_KEY), code 116 (KEY_POWER), value 0
Event: time 124.000, type 0 (EV_SYN), code 0 (SYN_REPORT), value 0
```

解释一下：

```text
type 1 / EV_KEY：按键事件
code 116 / KEY_POWER：具体哪个按键
value 1：按下
value 0：松开
value 2：长按自动重复，前提是启用了 repeat
```

---

# 10. 没有 evtest 怎么办？

可以用：

```bash
hexdump /dev/input/event0
```

然后按键，能看到数据变化。

或者：

```bash
cat /dev/input/event0
```

按键时如果终端有乱码输出，说明 event 设备有数据。

但是 `cat` 看不懂内容，所以最好还是加 `evtest` 到 rootfs。

---

# 11. 应用程序怎么读取按键？

应用层 C 程序可以这样读：

```c
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>

int main(int argc, char *argv[])
{
    int fd;
    struct input_event ev;

    if (argc < 2) {
        printf("Usage: %s /dev/input/eventX\n", argv[0]);
        return -1;
    }

    fd = open(argv[1], O_RDONLY);
    if (fd < 0) {
        perror("open input device failed");
        return -1;
    }

    printf("Start reading key events from %s...\n", argv[1]);

    while (1) {
        int ret = read(fd, &ev, sizeof(ev));
        if (ret != sizeof(ev)) {
            perror("read failed");
            break;
        }

        if (ev.type == EV_KEY) {
            if (ev.value == 1) {
                printf("key code %d pressed\n", ev.code);
            } else if (ev.value == 0) {
                printf("key code %d released\n", ev.code);
            } else if (ev.value == 2) {
                printf("key code %d repeat\n", ev.code);
            }
        }
    }

    close(fd);
    return 0;
}
```

交叉编译：

```bash
mips-linux-gnu-gcc key_test.c -o key_test
```

板子上运行：

```bash
./key_test /dev/input/event0
```

---

# 12. 长按、短按、双击在哪里做？

重点来了。

`gpio-keys` 只负责上报基础事件：

```text
按下
松开
重复
```

比如：

```text
按下：value = 1
松开：value = 0
重复：value = 2
```

但是：

```text
短按
长按
双击
三击
组合键
```

这些一般放在**应用层**做。

例如：

```text
应用收到 KEY_POWER value=1
记录按下时间 press_time

应用收到 KEY_POWER value=0
记录松开时间 release_time

release_time - press_time < 500ms
  短按

release_time - press_time > 2000ms
  长按
```

不要把复杂业务逻辑塞进内核驱动。

内核驱动只负责：

```text
稳定、标准地上报按键事件
```

应用层负责：

```text
业务判断
短按/长按/双击
菜单跳转
开关机逻辑
```

---

# 13. 如果不用 gpio-keys，能不能直接读 GPIO？

可以，但不推荐作为正式方案。

Linux 新一点的方式是用：

```text
libgpiod
```

工具有：

```bash
gpiodetect
gpioinfo
gpioget
gpiomon
```

比如监听 GPIO 边沿：

```bash
gpiomon gpiochip0 5
```

这种适合：

```text
临时调试
简单测试 GPIO 电平
验证硬件
```

但是正式产品里的按键，还是建议走：

```text
gpio-keys + input event
```

原因：

```text
标准
稳定
支持中断
支持消抖
支持唤醒
支持 Linux input 生态
应用代码通用
```

旧系统里还有 `/sys/class/gpio`：

```bash
echo 5 > /sys/class/gpio/export
echo in > /sys/class/gpio/gpio5/direction
cat /sys/class/gpio/gpio5/value
```

但这个接口已经偏旧，适合临时调试，不推荐新项目依赖它做正式应用。

---

# 14. 如果是矩阵键盘呢？

如果不是几个独立 GPIO 按键，而是矩阵键盘，比如：

```text
3 行 × 3 列
4 行 × 4 列
```

那就不要用 `gpio-keys`，应该用：

```text
gpio-matrix-keypad
```

结构是：

```text
行 GPIO
列 GPIO
扫描矩阵
上报 input event
```

设备树会不一样。

---

# 15. 如果是 ADC 按键呢？

有些产品为了省 GPIO，会把多个按键接到一个 ADC：

```text
不同按键接不同电阻
按下后 ADC 电压不同
根据电压判断哪个键
```

这种叫：

```text
ADC key
```

Linux 里通常走：

```text
adc-keys
```

或者厂家自己的 ADC key 驱动。

它不是 `gpio-keys`。

---

# 16. 如果是电源键呢？

电源键有时不是普通 GPIO，而是接到：

```text
PMIC
RTC
PMU
专用 PWRKEY 引脚
```

这种可能要用：

```text
PMIC pwrkey 驱动
```

或者 X2600 平台专门的 power key 驱动。

所以你要先看原理图：

```text
按键到底接 GPIO？
还是接 PMIC？
还是接 ADC？
还是接专用 PWRKEY？
```

不同接法，驱动方案不同。

---

# 17. 按键开发完整流程

你可以按这个顺序做：

```text
第一步：看原理图
确认按键接到哪个引脚
确认是上拉还是下拉
确认按下是高电平还是低电平

第二步：确认 X2600 GPIO 编号
找到这个引脚属于哪个 gpio controller
比如 gpioa/gpiob/gpioc，具体看 X2600 SDK 的 DTS 和 pinctrl

第三步：修改设备树
添加 gpio-keys 节点
写 gpios、linux,code、debounce-interval

第四步：打开内核配置
CONFIG_INPUT
CONFIG_INPUT_EVDEV
CONFIG_KEYBOARD_GPIO

第五步：重新编译 kernel/dtb
生成新的镜像

第六步：烧录启动
看 dmesg 是否注册 gpio_keys

第七步：测试 input event
cat /proc/bus/input/devices
evtest /dev/input/eventX

第八步：应用层读取
read struct input_event
做短按、长按、双击
```

---

# 18. 常见问题排查

## 问题 1：`/dev/input/eventX` 没有出现

查：

```bash
dmesg | grep -i key
dmesg | grep -i input
cat /proc/bus/input/devices
```

可能原因：

```text
设备树没生效
compatible 写错
CONFIG_KEYBOARD_GPIO 没开
CONFIG_INPUT_EVDEV 没开
GPIO 控制器没启用
gpios 写错
dtb 没有烧成新的
```

---

## 问题 2：按键没反应

查：

```bash
cat /proc/interrupts
```

按键前后看中断计数有没有变化。

如果没变化：

```text
GPIO 写错
pinctrl 没配置成 GPIO
中断没配好
按键硬件没接好
上拉/下拉有问题
```

也可以临时用 GPIO 工具看电平：

```bash
gpioinfo
gpioget gpiochip0 5
```

---

## 问题 3：按下/松开反了

大概率是：

```text
GPIO_ACTIVE_LOW / GPIO_ACTIVE_HIGH 写反
```

如果硬件是按下接地，就应该：

```dts
GPIO_ACTIVE_LOW
```

---

## 问题 4：按一次触发很多次

这是消抖问题。

可以加大：

```dts
debounce-interval = <20>;
```

如果还不行，可以试：

```dts
debounce-interval = <30>;
debounce-interval = <50>;
```

但是不要太大，否则按键响应会变慢。

正常机械按键消抖一般：

```text
10ms ~ 50ms
```

---

## 问题 5：休眠后按键不能唤醒

设备树里要加：

```dts
wakeup-source;
```

同时内核、电源管理、GPIO 控制器都要支持 wakeup。

还可以查：

```bash
cat /sys/kernel/debug/wakeup_sources
```

或者：

```bash
cat /proc/interrupts
```

看对应中断是否支持唤醒。

---

# 19. 一个完整的三按键例子

假设你有三个按键：

```text
KEY1：菜单键，接 GPIOA5，低电平有效
KEY2：返回键，接 GPIOA6，低电平有效
KEY3：确认键，接 GPIOA7，低电平有效
```

设备树：

```dts
#include <dt-bindings/input/input.h>
#include <dt-bindings/gpio/gpio.h>

gpio_keys {
    compatible = "gpio-keys";
    status = "okay";

    menu_key {
        label = "menu";
        gpios = <&gpioa 5 GPIO_ACTIVE_LOW>;
        linux,code = <KEY_MENU>;
        debounce-interval = <20>;
    };

    back_key {
        label = "back";
        gpios = <&gpioa 6 GPIO_ACTIVE_LOW>;
        linux,code = <KEY_BACK>;
        debounce-interval = <20>;
    };

    enter_key {
        label = "enter";
        gpios = <&gpioa 7 GPIO_ACTIVE_LOW>;
        linux,code = <KEY_ENTER>;
        debounce-interval = <20>;
    };
};
```

启动后测试：

```bash
cat /proc/bus/input/devices
evtest /dev/input/event0
```

应用层收到：

```text
KEY_MENU pressed
KEY_MENU released

KEY_BACK pressed
KEY_BACK released

KEY_ENTER pressed
KEY_ENTER released
```

---

# 20. 总结

按键检测在 X2600 Linux 上，最推荐方案是：

```text
设备树 gpio-keys
  ↓
内核 CONFIG_KEYBOARD_GPIO
  ↓
Linux input 子系统
  ↓
/dev/input/eventX
  ↓
应用层读取 struct input_event
```

你要记住这句话：

```text
普通 GPIO 按键：用 gpio-keys
矩阵键盘：用 gpio-matrix-keypad
ADC 按键：用 adc-keys
PMIC 电源键：用 PMIC pwrkey 驱动
```

对你现在的新手阶段，最该掌握的是：

```text
看原理图确认按键引脚
  ↓
写设备树 gpio-keys
  ↓
打开 CONFIG_KEYBOARD_GPIO / CONFIG_INPUT_EVDEV
  ↓
evtest 测试 /dev/input/eventX
  ↓
应用层做短按、长按、双击
```
