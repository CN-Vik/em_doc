
# 1. 设备树配置

```c
&i2c2 {
	status = "okay";
	pinctrl-names = "default";
	pinctrl-0 = <&i2c2m4_xfer>;

    aw2013_led: aw2013_led@45 {
		compatible = "awinic,aw2013_i2c";
        reg = <0x45>;
		status = "okay";
        red_led-gpios = <&gpio3 RK_PD5 GPIO_ACTIVE_HIGH>;
	};
    
```

# 2. 驱动代码（随linux内核启动）
## 2.1 前期准备
1.  在kernel/drivers/leds/leds-aw2013.c 是AW2013类内核驱动文件 存放位置
2. kernel/drivers/leds/Kconfig 里边加入 以下的代码
```c
config LEDS_AW2013
	tristate "LED support for Awinic AW2013"
	depends on LEDS_CLASS && I2C && OF
	select REGMAP_I2C
	help
	  This option enables support for the AW2013 3-channel
	  LED driver.

	  To compile this driver as a module, choose M here: the module
	  will be called leds-aw2013.
```
3. kernel/drivers/leds/Makefile 里边加入obj-$(CONFIG_LEDS_AW2013)		+= leds-aw2013.o


## 2.2 aw2013.c 驱动文件（存放位置： kernel/drivers/leds/leds-aw2013.c）

```c
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/ide.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/gpio.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/of_gpio.h>
#include <linux/semaphore.h>
#include <linux/timer.h>
#include <linux/i2c.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include "../app/aw2013.h"
#include <linux/fs.h>
#include <linux/mutex.h>


#define aw2013_CNT      1
#define aw2013_NAME     "aw2013"


#define AW2013_IOC_MAGIC     'W'
#define AW2013_IOC_READ_REG  _IOWR(AW2013_IOC_MAGIC, 0x01, struct aw2013_reg_op)
#define AW2013_IOC_WRITE_REG _IOW(AW2013_IOC_MAGIC, 0x02, struct aw2013_reg_op)

#define AW2013_IOC_RED_LEDIO_SET _IOW(AW2013_IOC_MAGIC, 0x03, int)  // 0:关, 1:开
#define AW2013_IOC_RED_LEDIO_GET _IOR(AW2013_IOC_MAGIC, 0x04, int)  // 读当前GPIO电平


struct aw2013_reg_op {
    uint8_t reg;
    uint8_t val;
};


struct aw2013_dev {

    struct i2c_client *client;
    dev_t devid;            /* 设备号 	 */
    struct cdev cdev;       /* cdev 	*/
    struct class *class;    /* 类 		*/
    struct device *device;  /* 设备 	 */
    struct device_node *nd; /* 设备节点 */
    int major;              /* 主设备号 */
    void *private_data;     /* 私有数据 */
    struct mutex lock;
    struct gpio_desc *red_led_gpio;
};

static struct aw2013_dev aw2013dev;


/**
 * @brief 从aw2013读取一个寄存器数据
 *
 * @param dev aw2013设备
 * @param reg_addr 要读取的寄存器地址
 * @param reg_data 读取到的数据
 * @return int 操作结果
 */
static int aw2013_read_reg(struct i2c_client *client, unsigned char reg_addr, unsigned char *reg_data)
{
    int ret;


    ret = i2c_smbus_read_byte_data(client, reg_addr);
    if (ret < 0)
    {
        printk("[aw2013_read_reg]: failed=%d i2c_addr=0x%x reg=0x%x len=%d\n", ret, client->addr, reg_addr, 1);
        return ret;
    }
    *reg_data = ret & 0xFF;
    printk("[aw2013_read_reg]: success i2c_addr=0x%x reg=0x%x reg_data=0x%x\n", client->addr, reg_addr, *reg_data);

    return 0;
}


/**
 * @brief 向寄存器地址
 *
 * @param dev aw2013设备
 * @param reg 要写入的寄存器首地址
 * @param reg_data 向寄存器地址写入的数据
 * @return int 操作结果
 */
static int aw2013_write_reg(struct i2c_client *client, unsigned char reg_addr, unsigned char reg_data)
{
    int ret = 0;

    ret = i2c_smbus_write_byte_data(client, reg_addr, reg_data);
    if (ret < 0)
    {
        printk("[aw2013_write_reg]: failed=%d i2c_addr=0x%x reg=0x%x data=0x%x\n", ret, client->addr, reg_addr, reg_data);
        return ret;
    } else
    {
        printk("[aw2013_write_reg]: success i2c_addr=0x%x reg=0x%x data=0x%x\n", client->addr, reg_addr, reg_data);
    }

    return 0;
}


/**
 * @brief 打开设备
 *
 * @param inode 传递给驱动的inode
 * @param filp 设备文件，file结构体有个叫做private_data的成员变量
 * 一般在open的时候将private_data指向设备结构体。
 * @return int 0 成功;其他 失败
 */
static int aw2013_open(struct inode *inode, struct file *filp)
{
    struct aw2013_dev *dev = &aw2013dev;

    filp->private_data = dev;
    printk("aw2013_open - start 11111 \n");

    return 0;
}

/**
 * @brief 向设备写入数据
 *
 * @param file 要打开的设备文件(文件描述符)
 * @param buf 用户空间的数据缓冲区
 * @param count 要写入的数据长度
 * @param pos 相对于文件首地址的偏移
 * @return ssize_t 写入的字节数，如果为负值，表示写入失败
 */
static ssize_t aw2013_write(struct file *file, const char __user *buf, size_t count, loff_t *pos)
{
    // int ret = 0;
    // unsigned char databuf[2];
    // struct aw2013_dev *dev = (struct aw2013_dev *)file->private_data;

    printk("aw2013 write start! \r\n");

    // /*向内核空间写数据 buf -> databuf */
    // ret = copy_from_user(databuf, buf, count);
    // if (ret < 0)
    // {
    //     printk("kernel write data failed! \r\n");
    //     return -EFAULT;
    // }

    // aw2013_write_reg(dev, databuf[0], databuf[1]);
    return 0;
}

/**
 * @brief 从设备读取数据
 *
 * @param filp 要打开的设备文件(文件描述符)
 * @param buf 返回给用户空间的数据缓冲区
 * @param cnt 要读取的数据长度
 * @param off 相对于文件首地址的偏移
 * @return int 读取的字节数，如果为负值，表示读取失败
 */
static ssize_t aw2013_read(struct file *filp, char __user *buf, size_t cnt, loff_t *off)
{
    // short data[3];
    // long err = 0;

    // struct aw2013_dev *dev = (struct aw2013_dev *)filp->private_data;

    // aw2013_readdata(dev);

    // // data[0] = dev->ir;
    // // data[1] = dev->als;
    // // data[2] = dev->ps;
    // err = copy_to_user(buf, data, sizeof(data));
    return 0;
}

/**
 * @brief 关闭/释放设备
 *
 * @param filp 要关闭的设备文件(文件描述符)
 * @return int 0 成功;其他 失败
 */
static int aw2013_release(struct inode *inode, struct file *filp)
{
    return 0;
}


static long aw2013_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    struct aw2013_reg_op op;
    struct aw2013_dev *dev = &aw2013dev;

    /* 从用户空间复制数据到内核空间 arg->op */
    if (copy_from_user(&op, (void __user *)arg, sizeof(op)))
        return -EFAULT;

    mutex_lock(&dev->lock);

    switch (cmd)
    {
        case AW2013_IOC_READ_REG:
            // printk("[aw2013_ioctl]: AW2013_IOC_READ_REGS 1111\n");
            if (aw2013_read_reg(dev->client, op.reg, &op.val))
            {
                mutex_unlock(&dev->lock);
                return -EIO;
            }
            // else
            // {
            //     // printk("[aw2013_ioctl]: AW2013_IOC_READ_REGS ok 22222\n");
            // }

            // 将内核空间的 op 结构体数据拷贝到用户空间
            // 如果拷贝失败（返回值非0），说明用户空间指针非法或不可访问
            if (copy_to_user((void __user *)arg, &op, sizeof(op)))
            {
                // 发生错误时，先解锁互斥锁，防止死锁
                mutex_unlock(&dev->lock);
                // 返回 -EFAULT，表示拷贝到用户空间失败
                return -EFAULT;
            }
            // else
            // {
            //     // printk("[aw2013_ioctl]: AW2013_IOC_READ_REG ok 33333\n");
            // }
            break;

        case AW2013_IOC_WRITE_REG:
            // printk("[aw2013_ioctl]: AW2013_IOC_WRITE_REG 1111\n");
            if (aw2013_write_reg(dev->client, op.reg, op.val))
            {
                mutex_unlock(&dev->lock);
                return -EIO;
            }
            // else
            // {
            //     // printk("[aw2013_ioctl]: AW2013_IOC_WRITE_REG ok 22222\n");
            // }
            break;
        case AW2013_IOC_RED_LEDIO_SET:
            if (aw2013dev.red_led_gpio)
            {
                int level;
                if (copy_from_user(&level, (void __user *)arg, sizeof(level)))
                {
                    mutex_unlock(&dev->lock);
                    return -EFAULT;
                }
                gpiod_set_value(aw2013dev.red_led_gpio, level ? 1 : 0);
            }
            break;

        case AW2013_IOC_RED_LEDIO_GET:
            if (aw2013dev.red_led_gpio)
            {
                int level = gpiod_get_value(aw2013dev.red_led_gpio);
                if (copy_to_user((void __user *)arg, &level, sizeof(level)))
                {
                    mutex_unlock(&dev->lock);
                    return -EFAULT;
                }
            }
            break;

        default:
            mutex_unlock(&dev->lock);
            return -EINVAL;
    }

    mutex_unlock(&dev->lock);
    return 0;
}


/* aw2013文件操作函数 */
static const struct file_operations aw2013_file_ops = {
    .owner          = THIS_MODULE,
    .open           = aw2013_open,
    .read           = aw2013_read,
    .write          = aw2013_write,
    .release        = aw2013_release,
    .unlocked_ioctl = aw2013_ioctl,
};

/**
 * @brief i2c驱动的probe函数，当驱动与设备匹配以后此函数就会执行
 *
 * @param client i2c设备
 * @param id i2c设备ID
 * @return int 0，成功;其他负值,失败
 */
static int aw2013_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    unsigned char dev_id = 0;

    dev_info(&client->dev, "[aw2013_probe]:START V1.0.9\n");

    mutex_init(&aw2013dev.lock);
    aw2013dev.client = client;

    /* 1、构建设备号 */
    if (aw2013dev.major)
    {
        aw2013dev.devid = MKDEV(aw2013dev.major, 0);
        register_chrdev_region(aw2013dev.devid, aw2013_CNT, aw2013_NAME);
    } else
    {
        alloc_chrdev_region(&aw2013dev.devid, 0, aw2013_CNT, aw2013_NAME);
        aw2013dev.major = MAJOR(aw2013dev.devid);
    }

    /* 2、注册设备 */
    cdev_init(&aw2013dev.cdev, &aw2013_file_ops);
    aw2013dev.cdev.owner = THIS_MODULE;
    cdev_add(&aw2013dev.cdev, aw2013dev.devid, aw2013_CNT);

    /* 3、创建类 */
    aw2013dev.class = class_create(THIS_MODULE, aw2013_NAME);
    if (IS_ERR(aw2013dev.class))
    {
        dev_info(&client->dev, "[aw2013_probe]: class fialed! \n");
        return PTR_ERR(aw2013dev.class);
    }

    /* 4、创建设备 */
    aw2013dev.device = device_create(aw2013dev.class, NULL, aw2013dev.devid, NULL, aw2013_NAME);
    if (IS_ERR(aw2013dev.device))
    {
        dev_info(&client->dev, "[aw2013_probe]: device_create fialed! \n");
        return PTR_ERR(aw2013dev.device);
    }


    /*5. config red led GPIO*/
    // 申请 red_led GPIO 控制句柄，默认输出低电平（GPIOD_OUT_LOW）
    // "red_led" 是设备树或板级文件中定义的 GPIO 名称
    aw2013dev.red_led_gpio = devm_gpiod_get_optional(&client->dev, "red_led", GPIOD_OUT_LOW);
    // 检查 GPIO 申请是否出错
    if (IS_ERR(aw2013dev.red_led_gpio))
    {
        // 申请失败，打印错误信息并返回错误码
        dev_err(&client->dev, "Failed to get red_led gpio\n");
        return PTR_ERR(aw2013dev.red_led_gpio);
    }
    // 如果成功获取到 GPIO 控制句柄
    if (aw2013dev.red_led_gpio)
    {
        // 设置 GPIO 输出为高电平，关闭红色 LED 
        gpiod_set_value(aw2013dev.red_led_gpio, 1);
        // 延时 10 毫秒，等待芯片上电稳定
        msleep(10);
    }


    aw2013dev.private_data = client;

    dev_info(&client->dev, "[aw2013_probe]: 22222222\n");

    // 1. 发送复位命令
    aw2013_write_reg(client, AW_REG_RESET, 0x55);

    // 2. 等待复位完成（至少10ms）
    msleep(20);

    // 3. 尝试读取设备ID/状态寄存器验证
    aw2013_read_reg(client, AW_REG_RESET, &dev_id);

    // 预期值：AW2013芯片ID应为0x33（依据数据手册）
    if (dev_id != 0x33)
    {
        dev_info(&client->dev, "[aw2013_probe]: Invalid AW2013 CHIP_ID! Expected 0x33, got 0x%02X\n", dev_id);
        return -ENODEV;
    } else
    {
        dev_info(&client->dev, "[aw2013_probe]: AW2013 CHIP_ID read ok! is 0x%x\n", dev_id);
    }


    dev_info(&client->dev, "[aw2013_probe]: initialization complete\n");

    return 0;
}

/**
 * @brief i2c驱动的remove函数，移除i2c驱动的时候此函数会执行
 *
 * @param client i2c设备
 * @return int 0，成功;其他负值,失败
 */
static int aw2013_remove(struct i2c_client *client)
{
    /* 注销掉类和设备 */
    device_destroy(aw2013dev.class, aw2013dev.devid);
    class_destroy(aw2013dev.class);
    mutex_destroy(&aw2013dev.lock);

    /* 删除设备 */
    cdev_del(&aw2013dev.cdev);
    unregister_chrdev_region(aw2013dev.devid, aw2013_CNT);

    return 0;
}

/* 传统匹配方式ID列表 */
static const struct i2c_device_id aw2013_id[] = {
    {"awinic,aw2013_i2c", 0},
    {}};

/* 设备树匹配列表 */
static const struct of_device_id aw2013_of_match[] = {
    {.compatible = "awinic,aw2013_i2c"},
    {/* Sentinel */}};

MODULE_DEVICE_TABLE(of, aw2013_of_match);

/* i2c驱动结构体 */
static struct i2c_driver aw2013_driver = {
    .probe  = aw2013_probe,
    .remove = aw2013_remove,
    .driver = {
        .owner          = THIS_MODULE,
        .name           = aw2013_NAME,
        .of_match_table = aw2013_of_match,
    },
    .id_table = aw2013_id,
};


module_i2c_driver(aw2013_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("WLS_996");
MODULE_DESCRIPTION("AW2013 LED Driver");

```


### 2.2.1 驱动makeifle 

```bash
# RK3588专用交叉编译工具链
TOOLCHAIN_PATH := /home/wls/manifoldtech_file/project/RK3588/rk3588_linux_241112/prebuilts/gcc/linux-x86/aarch64/gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu
CROSS_COMPILE := $(TOOLCHAIN_PATH)/bin/aarch64-none-linux-gnu-
CC := $(CROSS_COMPILE)gcc
SYSROOT := $(TOOLCHAIN_PATH)/aarch64-none-linux-gnu/libc

# RK3588专用编译选项 (解决usleep问题)
CFLAGS := -Wall -O2 -std=c99
CFLAGS += --sysroot=$(SYSROOT)
CFLAGS += -I.
CFLAGS += -march=armv8-a+crypto+crc
CFLAGS += -mtune=cortex-a76.cortex-a55

# 关键修复：添加posix实时扩展库 (解决usleep问题)
LDFLAGS := -lrt -pthread

# 目标设置
TARGET := aw2013_app
SRCS := aw2013_app.c

# RK3588构建规则
$(TARGET): $(SRCS)
	@echo "正在为RK3588编译 $(TARGET)..."
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)
	@echo "$(TARGET) 编译完成 (RK3588 AArch64)"

clean:
	@rm -f $(TARGET)
	@echo "清理完成: $(TARGET) 已删除"

.PHONY: clean
```


## 2.2 app代码

```c
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <stdint.h>
#include <string.h>
#include "aw2013.h"

/* LED Imax,0x00=omA,0x01=5mA,0x02=10mA,0x03=15mA */
#define Imax 0x02

#define AW2013_IOC_MAGIC     'W'
#define AW2013_IOC_READ_REG  _IOWR(AW2013_IOC_MAGIC, 0x01, aw2013_reg_op_t)
#define AW2013_IOC_WRITE_REG _IOW(AW2013_IOC_MAGIC, 0x02, aw2013_reg_op_t)
#define AW2013_IOC_RED_LEDIO_SET _IOW(AW2013_IOC_MAGIC, 0x03, int)  // 0:关, 1:开
#define AW2013_IOC_RED_LEDIO_GET _IOR(AW2013_IOC_MAGIC, 0x04, int)  // 读当前GPIO电平


typedef struct {
    uint8_t reg;
    uint8_t val;

} aw2013_reg_op_t;

int app_aw2013_write_reg(int fd, uint8_t reg, uint8_t val)
{
    aw2013_reg_op_t op = {.reg = reg, .val = val};
    return ioctl(fd, AW2013_IOC_WRITE_REG, &op);
}

int app_aw2013_read_reg(int fd, uint8_t reg, uint8_t *val)
{
    aw2013_reg_op_t op = {.reg = reg};
    int ret            = ioctl(fd, AW2013_IOC_READ_REG, &op);
    if (ret == 0)
        *val = op.val;
    return ret;
}

int app_aw2013_red_ledio_set(int fd, int level)
{
    return ioctl(fd, AW2013_IOC_RED_LEDIO_SET, &level);
}

int app_aw2013_red_ledio_get(int fd, int *level)
{
    return ioctl(fd, AW2013_IOC_RED_LEDIO_GET, level);
}


int main()
{
    uint8_t dev_id = 0;

    uint8_t val = 0;  // 读个寄存器验证

    printf("\n [APP_AW2013]:aw2013 open start V1.0.9! \n");

    int fd = open("/dev/aw2013", O_RDWR);
    if (fd < 0)
    {
        perror("open aw2013");
        return 1;
    } else
    {
        printf("[APP_AW2013]:aw2013 open success! 111111 \n");
    }

    app_aw2013_write_reg(fd, AW_REG_RESET, 0x55 & 0XFF);  // software reset
    sleep(1);

    printf("[APP_AW2013]:aw2013 open success! 111111-(1) \n");

    app_aw2013_read_reg(fd, AW_REG_RESET, &dev_id);
    // 预期值：AW2013芯片ID应为0x33（依据数据手册）
    if (dev_id != 0x33)
    {
        printf("[APP_AW2013]:Invalid AW2013 CHIP_ID! Expected 0x33, got 0x%02X\n", dev_id);
        return -1;
    } else
    {
        printf("[APP_AW2013]:AW2013 CHIP_ID read ok!222222 is 0x%x\n", dev_id);
    }

    // 举例：初始化 PWM 模式使能红灯
    sleep(1);

    printf("[APP_AW2013]:AW2013 reset ok 333333!\n");

    // 4. 应用配置
    /* enable LED */
    app_aw2013_write_reg(fd, 0x01, 0x01);
    /* config mode, IMAX = 5mA */
    app_aw2013_write_reg(fd, 0x31, Imax);
    /* config mode, IMAX = 5mA */
    app_aw2013_write_reg(fd, 0x32, Imax);
    /* config mode, IMAX = 5mA */
    app_aw2013_write_reg(fd, 0x33, Imax);
    /* LED0 level */
    app_aw2013_write_reg(fd, 0x34, 0xff);
    /* LED1 level */
    app_aw2013_write_reg(fd, 0x35, 0xff);
    /* LED2 level */
    app_aw2013_write_reg(fd, 0x36, 0xff);

    app_aw2013_write_reg(fd, 0x30, 0x1);

    printf("[APP_AW2013]:LED configured 444444 \n");

    app_aw2013_read_reg(fd, 0x30, &val);
    sleep(1);

    printf("[APP_AW2013]:Read 555555 AW_REG_LED_ENABLE back reg 0x30: 0x%02X\n", val);


    // 开启 GPIO 输出（拉高）
    printf("[APP_AW2013]: enable gpio output HIGH\n");
    app_aw2013_red_ledio_set(fd, 0);
    sleep(5);
    app_aw2013_write_reg(fd, 0x30, 0x0);


    close(fd);
    return 0;
}

```

### 2.2.1 app makefile

```c
# Kernel module name
obj-m := aw2013.o

# Kernel source directory
KDIR := /home/wls/manifoldtech_file/project/RK3588/rk3588_linux_241112/kernel

# Cross compiler and architecture
CROSS_COMPILE := /home/wls/manifoldtech_file/project/RK3588/rk3588_linux_241112/prebuilts/gcc/linux-x86/aarch64/gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu/bin/aarch64-none-linux-gnu-
ARCH := arm64

# Enable C99 standard for kernel module
EXTRA_CFLAGS += -std=gnu99

# Output directory for build artifacts
OUTPUT_DIR := $(CURDIR)/output

all: | $(OUTPUT_DIR)
	@echo "Building kernel module..."
	$(MAKE) -C $(KDIR) M=$(CURDIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) EXTRA_CFLAGS="$(EXTRA_CFLAGS)" modules
	@echo "Moving build artifacts to $(OUTPUT_DIR)..."
	@mv -f *.o *.ko *.mod* modules.order Module.symvers .*.cmd .tmp_versions $(OUTPUT_DIR)/ 2>/dev/null || true

$(OUTPUT_DIR):
	@mkdir -p $@
	@echo "Created output directory: $@"

clean:
	@echo "Cleaning build artifacts..."
	@if [ -d "$(OUTPUT_DIR)" ]; then \
		rm -rf $(OUTPUT_DIR); \
		echo "Removed output directory: $(OUTPUT_DIR)"; \
	fi
	@$(MAKE) -C $(KDIR) M=$(CURDIR) clean

.PHONY: all clean
```

# 3. 也可以做成.ko驱动模块，以上的makefile 就是为了.ko驱动模块使用的