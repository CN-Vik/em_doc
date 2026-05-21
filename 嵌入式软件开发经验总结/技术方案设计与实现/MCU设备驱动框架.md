
# 前言
MCU开发分5层
1. APP层 (文件名尾为app)
2. Mid层 (文件名尾为mid)
3. device driver层 （IIC ,SPI 等设备驱动，文件名尾为drv）
4. peri_driver层（外设驱动配置层，GPIO类，IIC类都在此层，文件名尾为drv）
5. hal 层（芯片外设驱动库）

# 1. GPIO类驱动框架

## 1.1 GPIO头文件
```c
/**
 * @file drv_gpio.h
 * @author wls (ufo281@outlook.com) 
 * @brief 
 * @version 1.0
 * @date 2025-09-25
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#ifndef _GPIO_H_
#define _GPIO_H_
#include <stdbool.h>
#include <stdint.h>
#include "gd32f4xx_gpio.h"


#define SC8815_A_EN_IO_PORT    GPIOE
#define SC8815_A_EN_IO_PIN     GPIO_PIN_2

#define SC8815_B_EN_IO_PORT    GPIOE
#define SC8815_B_EN_IO_PIN     GPIO_PIN_3

#define SC8815_A_PSTOP_IO_PORT    GPIOE
#define SC8815_A_PSTOP_IO_PIN     GPIO_PIN_2

#define SC8815_B_PSTOP_IO_PORT    GPIOE
#define SC8815_B_PSTOP_IO_PIN     GPIO_PIN_2


// gpio input define
#define SW_SNS_PORT           (GPIOA)
#define SW_SNS_PIN            (GPIO_PIN_0)
#define LEFT_BUTTON_PORT      (GPIOC)
#define LEFT_BUTTON_PIN       (GPIO_PIN_14)
#define RIGHT_BUTTON_PORT     (GPIOC)
#define RIGHT_BUTTON_PIN      (GPIO_PIN_15)
#define USB_DET_PORT          (GPIOB)
#define USB_DET_PIN           (GPIO_PIN_0)


#define GPIO_DEV_NUMS   32



typedef enum  {
    ON = 0,
    OFF = 1
}Output_Staus_e_t;



typedef enum {

    /*Output Groups*/
    HOLD_EN = 0,
    PHONE_ON,
    ESC_EN,
    SEL_USB,
    VBUS_CTRL,
    MB_PWR_EN,
    GPS_PWR_EN,
    GB_PWR_EN,
    SC8815_A_PSTOP,
    SC8815_A_EN,
    CHG_EN,
    ESC_SYNC,

    /*INput Groups*/
    KEY_ON_OFF,
    KEY_LEFT,
    KEY_RIGHT,
    USB_DET,
    KEY_END,

}Use_IO_e_t;


/**
 * @brief 具体IO的属性配置
 * 
 */
typedef struct 
{
    uint32_t gpio_port; /*eg: GPIOA, GPIOB CDE...*/

    uint32_t gpio_pin; 
    
}GPIO_dev_s_t;




typedef struct GPIO_Class_s
{
    GPIO_dev_s_t gpio_devs[GPIO_DEV_NUMS];

    uint8_t (*init)( struct GPIO_Class_s *dev , uint8_t device_state);
    uint8_t (*output_init)( struct GPIO_Class_s *dev , uint8_t device_state);
    uint8_t (*input_init)( struct GPIO_Class_s *dev );
    void (*set_output_mode)(uint32_t gpio_port, uint32_t gpio_pin);
    void (*set_input_mode)(uint32_t gpio_port, uint32_t gpio_pin);

    void (*write_bit)(uint32_t pin_port, uint32_t pin, bit_status bit_v);
    bit_status (*read_bit)(uint32_t pin_port, uint32_t pin);
    void (*ISR)(void);

} GPIO_Class_t;


uint8_t gpio_init( struct GPIO_Class_s *dev , uint8_t device_state);
void gpio_set_output_mode(uint32_t gpio_port,uint32_t gpio_pin);
void gpio_set_input_mode(uint32_t gpio_port,uint32_t gpio_pin);
void gpio_write_bit(uint32_t pin_port, uint32_t pin, bit_status bit_v);
bit_status gpio_read_bit(uint32_t pin_port, uint32_t pin);
uint8_t gpio_output_init(struct GPIO_Class_s *dev , uint8_t device_state);
uint8_t gpio_input_init( struct GPIO_Class_s *dev );
static void GPIO_INT_ISR(void);



extern GPIO_Class_t g_gpio;



#endif /* _GPIO_H_ */


```

 ## 1.2 GPIO源文件
 

```c
/*!
    \file  drv_gpio.c
    \brief gpio configuration file
    \version 2025-09-08, V1.0.0, firmware for GD32F4xx
*/

#include "drv_gpio.h"
#include "app_iap.h"
#include "power_manager.h"


GPIO_Class_t g_gpio = 
{
    .gpio_devs = {

        [SC8815_A_EN] = {

            .gpio_port = SC8815_A_EN_IO_PORT,
            .gpio_pin = SC8815_A_EN_IO_PIN,
        },
        [SC8815_A_PSTOP] = {

            .gpio_port = SC8815_A_PSTOP_IO_PORT,
            .gpio_pin = SC8815_A_PSTOP_IO_PIN,
        },
        [USB_DET] = {

            .gpio_port = USB_DET_PORT,
            .gpio_pin = USB_DET_PIN,
        },

    },
    .init = gpio_init,
    .output_init = gpio_output_init,
    .input_init = gpio_input_init,
    .set_output_mode = gpio_set_output_mode,
    .set_input_mode = gpio_set_input_mode,
    .write_bit = gpio_write_bit,
    .read_bit = gpio_read_bit,
    .ISR = GPIO_INT_ISR,
};


uint8_t gpio_init( struct GPIO_Class_s *dev , uint8_t device_state)
{

    if (dev==NULL ||  dev->output_init == NULL ||dev->input_init == NULL )
    {
        return 1;
    }
 
    /*----------CLOCK Init-----------*/
    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_GPIOB);
    rcu_periph_clock_enable(RCU_GPIOC);
    rcu_periph_clock_enable(RCU_GPIOD);
    rcu_periph_clock_enable(RCU_GPIOF);
    /*----------CLOCK Init-----------*/

    dev->output_init(dev,device_state);

    dev->input_init(dev);

}



/**
 * @brief set GPIO is output mode and set 0 value
 * 
 * @param gpio_port 
 * @param gpio_pin 
 * @return void 
 */
void gpio_set_output_mode(uint32_t gpio_port,uint32_t gpio_pin)
{
    gpio_mode_set(gpio_port, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, gpio_pin);
    gpio_output_options_set(gpio_port, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, gpio_pin);
}



/**
 * @brief SET GPIO is INPUT MODE
 * 
 * @param gpio_port 
 * @param gpio_pin 
 */
void gpio_set_input_mode(uint32_t gpio_port,uint32_t gpio_pin)
{
    gpio_mode_set(gpio_port, GPIO_MODE_INPUT, GPIO_PUPD_NONE, gpio_pin);
}



/**
 * @brief set gpio value
 * 
 * @param pin_port 
 * @param pin 
 * @param bit_v SET:1 or RESET:0
 */
void gpio_write_bit(uint32_t pin_port, uint32_t pin, bit_status bit_v)
{
    gpio_bit_write(pin_port, pin, bit_v);
}



/**
 * @brief read gpio status
 * 
 * @param pin_port 
 * @param pin 
 * @return bit_status SET:1 or RESET: 0
 */
bit_status gpio_read_bit(uint32_t pin_port, uint32_t pin)
{
    return gpio_input_bit_get(pin_port, pin);
}



uint8_t gpio_output_init(struct GPIO_Class_s *dev , uint8_t device_state)
{
    
    if ( dev==NULL || dev->set_output_mode ==NULL )
    {
        return 1;
    }

    dev->set_output_mode( dev->gpio_devs[SC8815_A_EN].gpio_port, dev->gpio_devs[SC8815_A_EN].gpio_pin);
    dev->write_bit( dev->gpio_devs[SC8815_A_EN].gpio_port , dev->gpio_devs[SC8815_A_EN].gpio_pin ,RESET);

    dev->set_output_mode( dev->gpio_devs[SC8815_A_PSTOP].gpio_port, dev->gpio_devs[SC8815_A_PSTOP].gpio_pin);
    dev->write_bit( dev->gpio_devs[SC8815_A_PSTOP].gpio_port , dev->gpio_devs[SC8815_A_PSTOP].gpio_pin ,RESET);


    gpio_bit_reset(OLED_RES_PORT, OLED_RES_PIN);
    gpio_mode_set(OLED_RES_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, OLED_RES_PIN);
    gpio_output_options_set(OLED_RES_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, OLED_RES_PIN);

    gpio_bit_set(OLED_D_C_PORT, OLED_D_C_PIN);
    gpio_mode_set(OLED_D_C_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, OLED_D_C_PIN);
    gpio_output_options_set(OLED_D_C_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, OLED_D_C_PIN);

    /* Start from initial state or shutdown state */
    if (device_state == INIT_STATUS || device_state == OFF_STATUS) 
    {
        // Initialize GPIOs to reset state
        gpio_bit_reset(ESC_EN_PORT, ESC_EN_PIN);
        gpio_mode_set(ESC_EN_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, ESC_EN_PIN);
        gpio_output_options_set(ESC_EN_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, ESC_EN_PIN);

        gpio_bit_reset(SEL_USB_PORT, SEL_USB_PIN);
        gpio_mode_set(SEL_USB_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, SEL_USB_PIN);
        gpio_output_options_set(SEL_USB_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, SEL_USB_PIN);

        gpio_bit_reset(GB_PWR_EN_PORT, GB_PWR_EN_PIN);
        gpio_mode_set(GB_PWR_EN_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GB_PWR_EN_PIN);
        gpio_output_options_set(GB_PWR_EN_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GB_PWR_EN_PIN);

        gpio_bit_reset(MB_PWR_EN_PORT, MB_PWR_EN_PIN);
        gpio_mode_set(MB_PWR_EN_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, MB_PWR_EN_PIN);
        gpio_output_options_set(MB_PWR_EN_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, MB_PWR_EN_PIN);

        gpio_bit_reset(PHONE_ON_PORT, PHONE_ON_PIN);
        gpio_mode_set(PHONE_ON_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, PHONE_ON_PIN);
        gpio_output_options_set(PHONE_ON_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, PHONE_ON_PIN);

        gpio_bit_reset(GPS_PWR_EN_PORT, GPS_PWR_EN_PIN);
        gpio_mode_set(GPS_PWR_EN_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPS_PWR_EN_PIN);
        gpio_output_options_set(GPS_PWR_EN_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPS_PWR_EN_PIN);

        gpio_bit_reset(CHG_EN_PORT, CHG_EN_PIN);
        gpio_mode_set(CHG_EN_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, CHG_EN_PIN);
        gpio_output_options_set(CHG_EN_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, CHG_EN_PIN);

        gpio_bit_reset(ESC_SYNC_PORT, ESC_SYNC_PIN);
        gpio_mode_set(ESC_SYNC_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, ESC_SYNC_PIN);
        gpio_output_options_set(ESC_SYNC_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, ESC_SYNC_PIN);

        gpio_bit_set(HOLD_EN_PORT, HOLD_EN_PIN);  // HOLD_EN high level valid
        gpio_mode_set(HOLD_EN_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, HOLD_EN_PIN);
        gpio_output_options_set(HOLD_EN_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, HOLD_EN_PIN);

        sysFlag.IoState.hold_en_sta_ = 1;
        sysFlag.IoState.phone_on_sta_ = 0;
        sysFlag.IoState.esc_en_sta_ = 0;
        sysFlag.IoState.sel_usb_sta_ = 0;
        sysFlag.IoState.mb_pwr_en_sta_ = 0;
        sysFlag.IoState.gps_pwr_en_sta_ = 0;
        sysFlag.IoState.gb_pwr_en_sta_ = 0;
        sysFlag.IoState.chg_en_sta_ = 0;
        sysFlag.IoState.esc_sync_sta_ = 0;
    } else if (device_state == ON_STATUS || device_state == SLEEP_STATUS ||
               device_state == BOOT_ON_STATUS) { /* Maintain GPIO state */
        if (sysFlag.IoState.hold_en_sta_ == 1) {
            gpio_bit_set(HOLD_EN_PORT, HOLD_EN_PIN);  // HOLD_EN high level valid
            gpio_mode_set(HOLD_EN_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, HOLD_EN_PIN);
            gpio_output_options_set(HOLD_EN_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, HOLD_EN_PIN);
        } else {
            gpio_bit_reset(HOLD_EN_PORT, HOLD_EN_PIN);
            gpio_mode_set(HOLD_EN_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, HOLD_EN_PIN);
            gpio_output_options_set(HOLD_EN_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, HOLD_EN_PIN);
        }

        if (sysFlag.IoState.phone_on_sta_ == 1) {
            gpio_bit_set(PHONE_ON_PORT, PHONE_ON_PIN);
            gpio_mode_set(PHONE_ON_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, PHONE_ON_PIN);
            gpio_output_options_set(PHONE_ON_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, PHONE_ON_PIN);
        } else {
            gpio_bit_reset(PHONE_ON_PORT, PHONE_ON_PIN);
            gpio_mode_set(PHONE_ON_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, PHONE_ON_PIN);
            gpio_output_options_set(PHONE_ON_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, PHONE_ON_PIN);
        }

        if (sysFlag.IoState.esc_en_sta_ == 1) {
            gpio_bit_set(ESC_EN_PORT, ESC_EN_PIN);
            gpio_mode_set(ESC_EN_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, ESC_EN_PIN);
            gpio_output_options_set(ESC_EN_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, ESC_EN_PIN);
        } else {
            gpio_bit_reset(ESC_EN_PORT, ESC_EN_PIN);
            gpio_mode_set(ESC_EN_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, ESC_EN_PIN);
            gpio_output_options_set(ESC_EN_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, ESC_EN_PIN);
        }

        if (sysFlag.IoState.sel_usb_sta_ == 1) {
            gpio_bit_set(SEL_USB_PORT, SEL_USB_PIN);
            gpio_mode_set(SEL_USB_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, SEL_USB_PIN);
            gpio_output_options_set(SEL_USB_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, SEL_USB_PIN);
        } else {
            gpio_bit_reset(SEL_USB_PORT, SEL_USB_PIN);
            gpio_mode_set(SEL_USB_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, SEL_USB_PIN);
            gpio_output_options_set(SEL_USB_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, SEL_USB_PIN);
        }

        /* In the case of reboot, pull the mainboard power control GPIO low during initialization */
        if (sysFlag.DeviceState.reboot == true) {
            sysFlag.IoState.mb_pwr_en_sta_ = 0;
        }
        if (sysFlag.IoState.mb_pwr_en_sta_ == 1) {
            gpio_bit_set(MB_PWR_EN_PORT, MB_PWR_EN_PIN);
            gpio_mode_set(MB_PWR_EN_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, MB_PWR_EN_PIN);
            gpio_output_options_set(MB_PWR_EN_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, MB_PWR_EN_PIN);
        } else {
            gpio_bit_reset(MB_PWR_EN_PORT, MB_PWR_EN_PIN);
            gpio_mode_set(MB_PWR_EN_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, MB_PWR_EN_PIN);
            gpio_output_options_set(MB_PWR_EN_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, MB_PWR_EN_PIN);
        }

        if (sysFlag.IoState.gps_pwr_en_sta_ == 1) {
            gpio_bit_set(GPS_PWR_EN_PORT, GPS_PWR_EN_PIN);
            gpio_mode_set(GPS_PWR_EN_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPS_PWR_EN_PIN);
            gpio_output_options_set(GPS_PWR_EN_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPS_PWR_EN_PIN);
        } else {
            gpio_bit_reset(GPS_PWR_EN_PORT, GPS_PWR_EN_PIN);
            gpio_mode_set(GPS_PWR_EN_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPS_PWR_EN_PIN);
            gpio_output_options_set(GPS_PWR_EN_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPS_PWR_EN_PIN);
        }

        if (sysFlag.IoState.gb_pwr_en_sta_ == 1) {
            gpio_bit_set(GB_PWR_EN_PORT, GB_PWR_EN_PIN);
            gpio_mode_set(GB_PWR_EN_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GB_PWR_EN_PIN);
            gpio_output_options_set(GB_PWR_EN_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GB_PWR_EN_PIN);
        } else {
            gpio_bit_reset(GB_PWR_EN_PORT, GB_PWR_EN_PIN);
            gpio_mode_set(GB_PWR_EN_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GB_PWR_EN_PIN);
            gpio_output_options_set(GB_PWR_EN_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GB_PWR_EN_PIN);
        }

        if (sysFlag.IoState.chg_en_sta_ == 1) {
            gpio_bit_set(CHG_EN_PORT, CHG_EN_PIN);
            gpio_mode_set(CHG_EN_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, CHG_EN_PIN);
            gpio_output_options_set(CHG_EN_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, CHG_EN_PIN);
        } else {
            gpio_bit_reset(CHG_EN_PORT, CHG_EN_PIN);
            gpio_mode_set(CHG_EN_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, CHG_EN_PIN);
            gpio_output_options_set(CHG_EN_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, CHG_EN_PIN);
        }

        if (sysFlag.IoState.esc_sync_sta_ == 1) {
            gpio_bit_set(ESC_SYNC_PORT, ESC_SYNC_PIN);
            gpio_mode_set(ESC_SYNC_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, ESC_SYNC_PIN);
            gpio_output_options_set(ESC_SYNC_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, ESC_SYNC_PIN);
        } else {
            gpio_bit_reset(ESC_SYNC_PORT, ESC_SYNC_PIN);
            gpio_mode_set(ESC_SYNC_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, ESC_SYNC_PIN);
            gpio_output_options_set(ESC_SYNC_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, ESC_SYNC_PIN);
        }
    }
}



uint8_t gpio_input_init( struct GPIO_Class_s *dev )
{
    if ( dev==NULL || dev->set_input_mode ==NULL )
    {
        return 1;
    }

    dev->set_input_mode( dev->gpio_devs[USB_DET].gpio_port ,dev->gpio_devs[USB_DET].gpio_pin );

    // // SW_SNS initialization
    // gpio_mode_set(SW_SNS_PORT, GPIO_MODE_INPUT, GPIO_PUPD_NONE, SW_SNS_PIN);

}



/**
 * @brief GPIO 中断服务函数
 * 
 */
static void GPIO_INT_ISR(void)
{

}


```

## 1.3 GPIO初始化&&应用

```c
    if (g_gpio.init!=NULL)
    {
        g_gpio.init(&g_gpio,device_status.device);
    }
    else
    {
        debug(" g_gpio.init is NULL \n");
    }

bool sc8815_gpio_write(uint32_t pin_port, uint32_t pin, bit_status value)
{
    if ( g_gpio.write_bit!=NULL )
    {
        g_gpio.write_bit(pin_port, pin, value);
    }
    
}    

```


# 2. IIC类驱动框架
## 2.1 头文件

```c
/**
 * @file drv_i2c.h
 * @author wls (ufo281@outlook.com) 
 * @brief 
 * @version 1.0
 * @date 2025-09-25
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#ifndef _DRV_I2C_H_
#define _DRV_I2C_H_

#include "gd32f4xx.h"
#include <stdio.h>
#include <stdint.h>
#include "debug.h"


#define PRINT   debug   

#define I2C_NUMS    3

/* 1:enable , 0:disable */
#define USE_I2C_DMA     1 /*DMA模式*/
#define USE_I2C_POLL    0 /*轮询模式*/

/* for i2c device config */
// #define USE_I2C0    
#define USE_I2C1



#define I2C_SPEED              400000      /*400KHZ*/
#define I2C0_SLAVE_ADDRESS7     0x08    

#define I2C0_PORT           GPIOB
#define I2C0_SCL_IO         GPIO_PIN_6
#define I2C0_SDA_IO         GPIO_PIN_7


#define I2C1_PORT           GPIOB
#define I2C1_SCL_IO         GPIO_PIN_10
#define I2C1_SDA_IO         GPIO_PIN_11


#define I2C0_DMA                DMA0
#define I2C0_DMA_TX_CH          DMA_CH6
#define I2C0_DMA_RX_CH          DMA_CH0


#define I2C1_DMA                DMA0
#define I2C1_DMA_TX_CH          DMA_CH7
#define I2C1_DMA_RX_CH          DMA_CH2


#define I2C_TIME_OUT    (uint16_t)(5000)
#define DMA_TIME_OUT    (uint16_t)(50000) /*随着传输数据量而改变*/

#define I2C_OK          1
#define I2C_FAIL        0
#define I2C_END         1


typedef enum 
{
    I2C_START = 0,
    I2C_SEND_ADDRESS,
    I2C_CLEAR_ADDRESS_FLAG,
    I2C_TRANSMIT_DATA,
    I2C_STOP

} i2c_process_e_t;


typedef enum {

    IIC_0 = 0,
    IIC_1 = 1,
    IIC_2 = 2

}I2C_em_t;



typedef struct i2c_class_s
{
    uint32_t i2c_speed; /*通讯速率（如100kHz, 400kHz）*/
    uint32_t i2c_clk_src; /* 时钟源 */
    uint32_t gpio_clk_src; /* 时钟源 */
    
    uint32_t i2c_dma_clk_src;   /* dma的时钟 */
    uint32_t i2c_dma;   /* 使用哪个dmax */
    uint32_t i2c_dma_tx_ch;   /* dma 发送通道 */
    uint32_t i2c_dma_rx_ch;   /* dma 接收通道 */
    uint8_t  dma_tx_ch_periph;/*dma tx通道绑定的具体外设，例如IIC or SPI*/
    uint8_t  dma_rx_ch_periph;/*dma tx通道绑定的具体外设，例如IIC or SPI*/

    uint32_t i2c_periph;
    uint32_t i2c_pin_port;   /* i2c io 的port */
    uint32_t i2c_sda_pin;   /*sda的具体IO号*/
    uint32_t i2c_scl_pin;   /*sda的具体IO号*/

    uint8_t (*init)( struct i2c_class_s *dev );
    uint8_t (*reset)( struct i2c_class_s *dev );

    uint8_t (*gpio_i2c_mode_config)(struct i2c_class_s *dev );

    uint8_t (*write_regs)(  struct i2c_class_s *dev,
                            uint8_t i2c_addr, // IIC device address(8bit)
                            uint8_t reg_addr,     // reg address
                            uint32_t len,          // bytes of length
                            uint8_t *buf          // data address
    );

    uint8_t (*read_regs)( struct i2c_class_s *dev,
                          uint8_t i2c_addr, // IIC device address(8bit)
                          uint8_t reg_addr,     // reg address
                          uint32_t len,          // bytes of length
                          uint8_t *buf          // data address
    );


} i2c_class_s_t;




/**
 * @brief 
 * 
 * @param i2cx 
 * @param flag I2C外设状态标志
 * @param condition 判断标志条件，SET OR RESET
 * @return uint8_t 0：OK!
 *                 1：failed!
 */
static inline uint8_t i2c_wait_flag(uint32_t i2cx, i2c_flag_enum flag ,FlagStatus condition)
{
    uint32_t timeout = I2C_TIME_OUT;
    while ( (i2c_flag_get(i2cx, flag) != condition) && (--timeout) );/*timeout 减小到0为停止*/
    return (timeout >= 0) ? 0 : 1;
}

uint8_t i2c_reset(struct i2c_class_s *dev );
uint8_t gpio_i2c_mode_cfg(struct i2c_class_s *dev );
uint8_t i2c_init( struct i2c_class_s *dev );
uint8_t write_regs( struct i2c_class_s *dev,
                    uint8_t i2c_addr, 
                    uint8_t reg_addr,     
                    uint32_t len,          
                    uint8_t *buf);
                        
uint8_t read_regs(  struct i2c_class_s *dev,
                    uint8_t i2c_addr,
                    uint8_t reg_addr,     
                    uint32_t len,          
                    uint8_t *buf);

extern i2c_class_s_t g_i2c[I2C_NUMS];



#endif /* _DRV_I2C_H_ */
```



-------
## 2.2 源文件

```c
/**
 * @file drv_i2c.c
 * @author wls (ufo281@outlook.com) 
 * @brief 
 * @version 1.0
 * @date 2025-09-25
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#include "drv_i2c.h"




i2c_class_s_t g_i2c[I2C_NUMS] =
{
    [IIC_0] = {//[0]
        
        .i2c_speed = I2C_SPEED,/*400KHZ*/
        .i2c_periph = I2C0,

        .i2c_dma_clk_src = RCU_DMA0,
        .i2c_dma = I2C0_DMA,
        .i2c_dma_tx_ch = I2C0_DMA_TX_CH,
        .i2c_dma_rx_ch = I2C0_DMA_RX_CH,
        .dma_tx_ch_periph = DMA_SUBPERI1,
        .dma_rx_ch_periph = DMA_SUBPERI1,

        .i2c_clk_src = RCU_I2C0,
        .gpio_clk_src = RCU_GPIOB,
        .i2c_pin_port = I2C0_PORT,
        .i2c_scl_pin = I2C0_SCL_IO,
        .i2c_sda_pin = I2C0_SDA_IO,
        .init = i2c_init,
        .reset = i2c_reset,
        .gpio_i2c_mode_config = gpio_i2c_mode_cfg,
        .read_regs  = read_regs,
        .write_regs = write_regs
    },
    [IIC_1] = {//[1]

        .i2c_speed = I2C_SPEED,/*400KHZ*/
        .i2c_periph = I2C1,

        .i2c_dma_clk_src = RCU_DMA0,
        .i2c_dma = I2C1_DMA,
        .i2c_dma_tx_ch = I2C1_DMA_TX_CH,
        .i2c_dma_rx_ch = I2C1_DMA_RX_CH,
        .dma_tx_ch_periph = DMA_SUBPERI7,
        .dma_rx_ch_periph = DMA_SUBPERI7,

        .gpio_clk_src = RCU_GPIOB,
        .i2c_clk_src = RCU_I2C1,
        .i2c_pin_port = I2C1_PORT,
        .i2c_scl_pin = I2C1_SCL_IO,
        .i2c_sda_pin = I2C1_SDA_IO,
        .init = i2c_init,
        .reset = i2c_reset,
        .gpio_i2c_mode_config = gpio_i2c_mode_cfg,
        .read_regs  = read_regs,
        .write_regs = write_regs,

    },
    // {//[2]
    //     .i2c_speed = I2C_SPEED,/*400KHZ*/
    //     .i2c_periph = I2C2,
    //     .i2c_scl_pin = I2C0_SCL_IO,
    //     .i2c_sda_pin = I2C0_SDA_IO,
    //     .i2c_init = i2c0_init,
    //     .read_regs  = read_regs,
    //     .write_regs = write_regs
    // }
};



uint8_t gpio_i2c_mode_cfg(struct i2c_class_s *dev )
{
    if (dev==NULL)
    {
        return 1;
    }
    /* enable GPIO PORT clock */
    rcu_periph_clock_enable(dev->gpio_clk_src);

    gpio_af_set(dev->i2c_pin_port, GPIO_AF_4, dev->i2c_scl_pin);

    gpio_af_set(dev->i2c_pin_port, GPIO_AF_4, dev->i2c_sda_pin);

    gpio_mode_set(dev->i2c_pin_port, GPIO_MODE_AF, GPIO_PUPD_PULLUP,dev->i2c_sda_pin);
    gpio_output_options_set(dev->i2c_pin_port, GPIO_OTYPE_OD, GPIO_OSPEED_50MHZ,dev->i2c_sda_pin);

    gpio_mode_set(dev->i2c_pin_port, GPIO_MODE_AF, GPIO_PUPD_PULLUP, dev->i2c_scl_pin);
    gpio_output_options_set(dev->i2c_pin_port, GPIO_OTYPE_OD, GPIO_OSPEED_50MHZ, dev->i2c_scl_pin);
    
    return 0;
}


uint8_t i2c_reset(struct i2c_class_s *dev )
{

    if (dev==NULL)
    {
        return 1;
    }

    i2c_deinit(dev->i2c_periph);

    /* configure SDA/SCL for GPIO */
    GPIO_BC(dev->i2c_pin_port) |= dev->i2c_scl_pin;
    GPIO_BC(dev->i2c_pin_port) |= dev->i2c_sda_pin;
    gpio_output_options_set(dev->i2c_pin_port, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, dev->i2c_scl_pin);
    gpio_output_options_set(dev->i2c_pin_port, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, dev->i2c_sda_pin);
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    /* stop signal scl pin*/
    GPIO_BOP(dev->i2c_pin_port) |= dev->i2c_scl_pin;
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    __NOP();
    GPIO_BOP(dev->i2c_pin_port) |= dev->i2c_sda_pin;

    /* connect I2C_SCL_PIN to I2C_SCL */
    /* connect I2C_SDA_PIN to I2C_SDA */
    gpio_output_options_set(dev->i2c_pin_port, GPIO_OTYPE_OD, GPIO_OSPEED_50MHZ, dev->i2c_scl_pin);
    gpio_output_options_set(dev->i2c_pin_port, GPIO_OTYPE_OD, GPIO_OSPEED_50MHZ, dev->i2c_sda_pin);
    /* configure the  dev->i2c_periph interface */
    
    dev->init(dev);

    return 0;
}



uint8_t i2c_init( struct i2c_class_s *dev )
{

    if (dev==NULL || dev->gpio_i2c_mode_config == NULL )
    {
        return 1;
    }

    

    /*-----------------GPIO- CONFIG-----------------------------*/
    dev->gpio_i2c_mode_config(dev);
    
    /*-----------------GPIO- CONFIG-----------------------------*/

#if ( 1 == USE_I2C_DMA)

    /* enable DMA0 clock */
    rcu_periph_clock_enable(dev->i2c_dma);

#endif

    /*-----------------I2C0- CONFIG-----------------------------*/
    /*I2C CLOCK Enable*/
    rcu_periph_clock_enable(dev->i2c_clk_src);


    /* configure I2C clock */
    i2c_clock_config( dev->i2c_periph, dev->i2c_speed,I2C_DTCY_2);
    /* configure I2C address */
    i2c_mode_addr_config(dev->i2c_periph,
                        I2C_I2CMODE_ENABLE,
                        I2C_ADDFORMAT_7BITS,
                        I2C0_SLAVE_ADDRESS7);
    /* enable  dev->i2c_periph */
    i2c_enable(dev->i2c_periph);
    /* enable acknowledge */
    i2c_ack_config(dev->i2c_periph,I2C_ACK_ENABLE);

    /*----------------- dev->i2c_periph- CONFIG-----------------------------*/

    return 0;
}



/**
 * @brief i2cx write regs 
 * 
 * @param dev 
 * @param i2c_addr IIC device address(8bit)
 * @param reg_addr 
 * @param len 
 * @param buf 
 * @return uint8_t 
 */
uint8_t write_regs( struct i2c_class_s *dev,
                    uint8_t i2c_addr, 
                    uint8_t reg_addr,     
                    uint32_t len,          
                    uint8_t *buf          
                 )
{
    if ( dev==NULL )
    {
        return 1;
    }

#if ( 1 == USE_I2C_POLL)

    if ( i2c_wait_flag(  dev->i2c_periph, I2C_FLAG_I2CBSY, RESET) != 0 )
    {
        return 1;
    }
    
    /* wait until I2C bus is idle */
    // while(i2c_flag_get( dev->i2c_periph, I2C_FLAG_I2CBSY));
    
    /* send a start condition to I2C bus */
    i2c_start_on_bus( dev->i2c_periph);
    
    if ( i2c_wait_flag(  dev->i2c_periph, I2C_FLAG_SBSEND, SET) != 0 )
    {
        return 2;
    }
    /* wait until SBSEND bit is set */
    // while(!i2c_flag_get( dev->i2c_periph, I2C_FLAG_SBSEND));
    
    /* send slave address to I2C bus */
    i2c_master_addressing( dev->i2c_periph, i2c_addr, I2C_TRANSMITTER);
    
    if ( i2c_wait_flag(  dev->i2c_periph, I2C_FLAG_ADDSEND, SET) != 0 )
    {
        return 3;
    }
    /* wait until ADDSEND bit is set */
    // while(!i2c_flag_get( dev->i2c_periph, I2C_FLAG_ADDSEND));
    
    /* clear the ADDSEND bit */
    i2c_flag_clear( dev->i2c_periph,I2C_FLAG_ADDSEND);
    
    if ( i2c_wait_flag(  dev->i2c_periph, I2C_FLAG_TBE, SET) != 0 )
    {
        return 4;
    }
    /* wait until the transmit data buffer is empty */
    // while( SET != i2c_flag_get( dev->i2c_periph, I2C_FLAG_TBE));
    
    /* send the I2C_DEV's internal address to write to : only one byte address */
    i2c_data_transmit( dev->i2c_periph, reg_addr);
    
    if ( i2c_wait_flag(  dev->i2c_periph, I2C_FLAG_BTC, SET) != 0 )
    {
        return 5;
    }
    /* wait until BTC bit is set */
    // while(!i2c_flag_get( dev->i2c_periph, I2C_FLAG_BTC));
    
    /* while there is data to be written */
    while(len--)
    {  
        i2c_data_transmit( dev->i2c_periph, *buf);
        /* point to the next byte to be written */
        buf++; 

        if ( i2c_wait_flag(  dev->i2c_periph, I2C_FLAG_BTC, SET) != 0 )
        {
            return 6;
        }
        /* wait until BTC bit is set */
        // while(!i2c_flag_get( dev->i2c_periph, I2C_FLAG_BTC));
    }

    /* send a stop condition to I2C bus */
    i2c_stop_on_bus( dev->i2c_periph);
    

    uint32_t time_out=0;
    /* wait until the stop condition is finished */
    while( I2C_CTL0( dev->i2c_periph)&I2C_CTL0_STOP )
    {
        time_out++;
        if (time_out >=1000)
        {
            time_out = 0;
            return 7;
        }
        
    }


    return 0;




#elif (( 1 == USE_I2C_DMA))   /* use i2c dma */


    dma_single_data_parameter_struct dma_init_parameter = {0};

    uint8_t i2c_state = I2C_START;
    uint16_t timeout = 0;
    uint8_t i2c_timeout_flag = 0;


    /* enable acknowledge */    
    i2c_ack_config( dev->i2c_periph, I2C_ACK_ENABLE);

    while (!(i2c_timeout_flag))
    {
        switch (i2c_state)
        {
            case I2C_START:
                /* i2c master sends start signal only when the bus is idle */
                while (i2c_flag_get( dev->i2c_periph, I2C_FLAG_I2CBSY) && (timeout < I2C_TIME_OUT))
                {
                    timeout++;
                }
                if (timeout < I2C_TIME_OUT)
                {
                    i2c_start_on_bus( dev->i2c_periph);
                    timeout = 0;
                    i2c_state = I2C_SEND_ADDRESS;
                }
                else
                {
                    if ( dev->reset!= NULL)
                    {
                        dev->reset(dev);
                    }
                    else{
                        PRINT("write: i2c reset is null!\n");
                    }
                    
                    timeout = 0;
                    i2c_state = I2C_START;
                    PRINT("i2c bus is busy in WRITE!\n");
                }
                break;
            case I2C_SEND_ADDRESS:
                /* i2c master sends START signal successfully */
                while ((!i2c_flag_get( dev->i2c_periph, I2C_FLAG_SBSEND)) && (timeout < I2C_TIME_OUT))
                {
                    timeout++;
                }
                if (timeout < I2C_TIME_OUT)
                {
                    i2c_master_addressing( dev->i2c_periph, i2c_addr, I2C_TRANSMITTER);
                    timeout = 0;
                    i2c_state = I2C_CLEAR_ADDRESS_FLAG;
                }
                else
                {
                    timeout = 0;
                    i2c_state = I2C_START;
                    PRINT("i2c master sends start signal timeout in WRITE!\n");
                }
                break;
            case I2C_CLEAR_ADDRESS_FLAG:
                /* address flag set means i2c slave sends ACK */
                while ((!i2c_flag_get( dev->i2c_periph, I2C_FLAG_ADDSEND)) && (timeout < I2C_TIME_OUT))
                {
                    timeout++;
                }
                if (timeout < I2C_TIME_OUT)
                {
                    i2c_flag_clear( dev->i2c_periph, I2C_FLAG_ADDSEND);
                    timeout = 0;
                    i2c_state = I2C_TRANSMIT_DATA;
                }
                else
                {
                    timeout = 0;
                    i2c_state = I2C_START;
                    PRINT("i2c master clears address flag timeout in WRITE!\n");
                }
                break;
            case I2C_TRANSMIT_DATA:
                /* wait until the transmit data buffer is empty */
                while ((!i2c_flag_get( dev->i2c_periph, I2C_FLAG_TBE)) && (timeout < I2C_TIME_OUT))
                {
                    timeout++;
                }
                if (timeout < I2C_TIME_OUT)
                {
                    /* send the I2C_DEV's internal address to write to : only one byte address */
                    i2c_data_transmit( dev->i2c_periph, reg_addr);
                    timeout = 0;
                }
                else
                {
                    timeout = 0;
                    i2c_state = I2C_START;
                    PRINT("i2c master sends I2C_DEV's internal address timeout in WRITE!\n");
                }

                /* wait until BTC bit is set */
                while ( (i2c_flag_get( dev->i2c_periph, I2C_FLAG_BTC)==RESET)  && (++timeout < I2C_TIME_OUT));
                if (timeout < I2C_TIME_OUT )
                {
                    timeout = 0;
                    dma_deinit(dev->i2c_dma, dev->i2c_dma_tx_ch);
                    dma_single_data_para_struct_init(&dma_init_parameter);
                    dma_init_parameter.direction = DMA_MEMORY_TO_PERIPH;
                    dma_init_parameter.memory0_addr = (uint32_t)buf;
                    dma_init_parameter.memory_inc = DMA_MEMORY_INCREASE_ENABLE;
                    dma_init_parameter.periph_memory_width = DMA_MEMORY_WIDTH_8BIT;
                    dma_init_parameter.number = len;
                    dma_init_parameter.periph_addr = (uint32_t)&I2C_DATA( dev->i2c_periph);
                    dma_init_parameter.periph_inc = DMA_PERIPH_INCREASE_DISABLE;
                    dma_init_parameter.priority = DMA_PRIORITY_ULTRA_HIGH;
                    dma_single_data_mode_init(dev->i2c_dma, dev->i2c_dma_tx_ch, &dma_init_parameter);

                    /* configure DMA mode */
                    dma_circulation_disable(dev->i2c_dma, dev->i2c_dma_tx_ch);
                    dma_channel_subperipheral_select(dev->i2c_dma, dev->i2c_dma_tx_ch, dev->dma_tx_ch_periph);
                    /* enable I2C DMA */
                    i2c_dma_config( dev->i2c_periph, I2C_DMA_ON);
                    /* enable DMA TX channel */
                    dma_channel_enable(dev->i2c_dma, dev->i2c_dma_tx_ch);
                    /* wait until FTF bit is set */
                    while ( !dma_flag_get(dev->i2c_dma, dev->i2c_dma_tx_ch, DMA_FLAG_FTF) && (++timeout < DMA_TIME_OUT) );
                    if (timeout < DMA_TIME_OUT)
                    {
                        timeout = 0;
                    }
                    else
                    {
                        PRINT("write:I2C DMA wait until FTF bit is set failed! is reset \n");
                        timeout = 0;
                        i2c_state = I2C_START;
                        break;
                    }

                    /* wait until BTC bit is set */
                    while ((!i2c_flag_get( dev->i2c_periph, I2C_FLAG_BTC)) && (timeout < I2C_TIME_OUT) )
                    {
                        timeout++;
                    }
                    if (timeout < I2C_TIME_OUT)
                    {
                        timeout = 0;
                        i2c_state = I2C_STOP;
                    }
                    else
                    {
                        timeout = 0;
                        i2c_state = I2C_START;
                        PRINT("i2c master sends data timeout in WRITE!\n");
                    }

                }
                else
                {
                    timeout = 0;
                    PRINT("i2c tx reg_addr failed !\n");
                    i2c_state = I2C_START;
                }
                break;

            case I2C_STOP:
                /* send a stop condition to I2C bus */
                i2c_stop_on_bus( dev->i2c_periph);
                /* i2c master sends STOP signal successfully */
                while ((I2C_CTL0( dev->i2c_periph) & I2C_CTL0_STOP) && (timeout < I2C_TIME_OUT))
                {
                    timeout++;
                }
                if (timeout < I2C_TIME_OUT)
                {
                    timeout = 0;
                    i2c_state = I2C_END;
                    i2c_timeout_flag = I2C_OK;
                }
                else
                {
                    timeout = 0;
                    i2c_state = I2C_START;
                    PRINT("i2c master sends stop signal timeout in WRITE!\n");
                }
                break;
            default:
                i2c_state = I2C_START;
                i2c_timeout_flag = I2C_OK;
                timeout = 0;
                PRINT("i2c master sends start signal in WRITE.\n");
                break;
        }
    }
    return I2C_END;

#endif


}



/**
 * @brief i2cx read regs
 * 
 * @param dev 
 * @param i2c_addr IIC device address(8bit)
 * @param reg_addr 
 * @param len 
 * @param buf 
 * @return uint8_t 
 */
uint8_t read_regs(  struct i2c_class_s *dev,
                    uint8_t i2c_addr,
                    uint8_t reg_addr,     
                    uint32_t len,          
                    uint8_t *buf          
                )
{

    if ( dev==NULL )
    {
        return 1;
    }

#if ( 1 == USE_I2C_POLL)


    if ( i2c_wait_flag(  dev->i2c_periph, I2C_FLAG_I2CBSY, RESET) != 0 )
    {
        return 1;
    }
    /* wait until I2C bus is idle */
    // while(i2c_flag_get( dev->i2c_periph, I2C_FLAG_I2CBSY));

    if(2 == len){
        i2c_ackpos_config( dev->i2c_periph,I2C_ACKPOS_NEXT);
    }
    
    /* send a start condition to I2C bus */
    i2c_start_on_bus( dev->i2c_periph);
    
    if ( i2c_wait_flag(  dev->i2c_periph, I2C_FLAG_SBSEND, SET) != 0 )
    {
        return 2;
    }
    /* wait until SBSEND bit is set */
    // while(!i2c_flag_get( dev->i2c_periph, I2C_FLAG_SBSEND));
    
    /* send slave address to I2C bus */

    i2c_master_addressing( dev->i2c_periph, i2c_addr, I2C_TRANSMITTER);
    
    if ( i2c_wait_flag(  dev->i2c_periph, I2C_FLAG_ADDSEND, SET) != 0 )
    {
        return 3;
    }
    /* wait until ADDSEND bit is set */
    // while(!i2c_flag_get( dev->i2c_periph, I2C_FLAG_ADDSEND));
    
    /* clear the ADDSEND bit */
    i2c_flag_clear( dev->i2c_periph,I2C_FLAG_ADDSEND);
    
    if ( i2c_wait_flag(  dev->i2c_periph, I2C_FLAG_TBE, SET) != 0 )
    {
        return 4;
    }
    /* wait until the transmit data buffer is empty */
    // while(SET != i2c_flag_get(  dev->i2c_periph , I2C_FLAG_TBE));

    /* enable  dev->i2c_periph*/
    i2c_enable( dev->i2c_periph);
    
    /* send the I2C_DEV's internal address to write to */
    i2c_data_transmit( dev->i2c_periph, reg_addr);  
    
    if ( i2c_wait_flag(  dev->i2c_periph, I2C_FLAG_BTC, SET) != 0 )
    {
        return 5;
    }
    /* wait until BTC bit is set */
    // while(!i2c_flag_get( dev->i2c_periph, I2C_FLAG_BTC));
    
    /* send a start condition to I2C bus */
    i2c_start_on_bus( dev->i2c_periph);
    
    if ( i2c_wait_flag(  dev->i2c_periph, I2C_FLAG_SBSEND, SET) != 0 )
    {
        return 6;
    }
    /* wait until SBSEND bit is set */
    // while(!i2c_flag_get( dev->i2c_periph, I2C_FLAG_SBSEND));
    
    /* send slave address to I2C bus */
    i2c_master_addressing( dev->i2c_periph, i2c_addr, I2C_RECEIVER);

    if(len < 3){
        /* disable acknowledge */
        i2c_ack_config( dev->i2c_periph,I2C_ACK_DISABLE);
    }
    
    if ( i2c_wait_flag(  dev->i2c_periph, I2C_FLAG_ADDSEND, SET) != 0 )
    {
        return 7;
    }
    /* wait until ADDSEND bit is set */
    // while(!i2c_flag_get( dev->i2c_periph, I2C_FLAG_ADDSEND));
    
    /* clear the ADDSEND bit */
    i2c_flag_clear( dev->i2c_periph,I2C_FLAG_ADDSEND);
    
    if(1 == len){
        /* send a stop condition to I2C bus */
        i2c_stop_on_bus( dev->i2c_periph);
    }
    
    /* while there is data to be read */
    while(len){
        if(3 == len)
        {
            if ( i2c_wait_flag(  dev->i2c_periph, I2C_FLAG_BTC, SET) != 0 )
            {
                return 8;
            }
            /* wait until BTC bit is set */
            // while(!i2c_flag_get( dev->i2c_periph, I2C_FLAG_BTC));

            /* disable acknowledge */
            i2c_ack_config( dev->i2c_periph,I2C_ACK_DISABLE);
        }
        if(2 == len)
        {
            if ( i2c_wait_flag(  dev->i2c_periph, I2C_FLAG_BTC, SET) != 0 )
            {
                return 9;
            }
            /* wait until BTC bit is set */
            // while(!i2c_flag_get( dev->i2c_periph, I2C_FLAG_BTC));
            
            /* send a stop condition to I2C bus */
            i2c_stop_on_bus( dev->i2c_periph);
        }
        
        /* wait until the RBNE bit is set and clear it */
        if(i2c_flag_get( dev->i2c_periph, I2C_FLAG_RBNE))
        {
            /* read a byte from the I2C_DEV */
            *buf = i2c_data_receive( dev->i2c_periph);
            
            /* point to the next location where the byte read will be saved */
            buf++; 
            
            /* decrement the read bytes counter */
            len--;
        } 
    }
    
    uint32_t time_out=0;
    /* wait until the stop condition is finished */
    while(I2C_CTL0( dev->i2c_periph)&I2C_CTL0_STOP)
    {
        time_out++;
        if (time_out >=1000)
        {
            time_out = 0;
            return 10;
        }
    }
    
    /* enable acknowledge */
    i2c_ack_config( dev->i2c_periph,I2C_ACK_ENABLE);

    i2c_ackpos_config( dev->i2c_periph,I2C_ACKPOS_CURRENT);

    return 0;

#elif (( 1 == USE_I2C_DMA))   /* use i2c dma */

    dma_single_data_parameter_struct dma_init_parameter;

    uint8_t i2c_state = I2C_START;
    uint8_t read_cycle = RESET;
    uint16_t timeout = 0;
    uint8_t i2c_timeout_flag = 0;

    /* enable acknowledge */
    i2c_ack_config( dev->i2c_periph, I2C_ACK_ENABLE);

    while (!(i2c_timeout_flag))
    {
        switch (i2c_state)
        {
            case I2C_START:
                if (RESET == read_cycle)
                {
                    /* disable  dev->i2c_periph */
                    i2c_disable( dev->i2c_periph);
                    /* enable  dev->i2c_periph */
                    i2c_enable( dev->i2c_periph);

                    /* enable acknowledge */
                    i2c_ack_config( dev->i2c_periph, I2C_ACK_ENABLE);
                    /* i2c master sends start signal only when the bus is idle */
                    while (i2c_flag_get( dev->i2c_periph, I2C_FLAG_I2CBSY) && (timeout < I2C_TIME_OUT))
                    {
                        timeout++;
                    }
                    if (timeout < I2C_TIME_OUT)
                    {
                        /* send the start signal */
                        i2c_start_on_bus( dev->i2c_periph);
                        timeout = 0;
                        i2c_state = I2C_SEND_ADDRESS;
                    }
                    else
                    {
                        if ( dev->reset!= NULL)
                        {
                            dev->reset(dev);
                        }
                        else{
                            PRINT("read: i2c reset is null!\n");
                        }
                            
                        timeout = 0;
                        i2c_state = I2C_START;
                        printf("i2c bus is busy in READ!\n");
                    }
                }
                else
                {
                    i2c_start_on_bus( dev->i2c_periph);
                    timeout = 0;
                    i2c_state = I2C_SEND_ADDRESS;
                }
                break;
            case I2C_SEND_ADDRESS:
                /* i2c master sends START signal successfully */
                while ((!i2c_flag_get( dev->i2c_periph, I2C_FLAG_SBSEND)) && (timeout < I2C_TIME_OUT))
                {
                    timeout++;
                }
                if (timeout < I2C_TIME_OUT)
                {
                    if (RESET == read_cycle)
                    {
                        i2c_master_addressing( dev->i2c_periph, i2c_addr, I2C_TRANSMITTER);
                        i2c_state = I2C_CLEAR_ADDRESS_FLAG;
                    }
                    else
                    {
                        i2c_master_addressing( dev->i2c_periph, i2c_addr, I2C_RECEIVER);
                        i2c_state = I2C_CLEAR_ADDRESS_FLAG;
                    }
                    timeout = 0;
                }
                else
                {
                    timeout = 0;
                    i2c_state = I2C_START;
                    read_cycle = 0;
                    printf("i2c master sends start signal timeout in READ!\n");
                }
                break;
            case I2C_CLEAR_ADDRESS_FLAG:
                /* address flag set means i2c slave sends ACK */
                while ((!i2c_flag_get( dev->i2c_periph, I2C_FLAG_ADDSEND)) && (timeout < I2C_TIME_OUT))
                {
                    timeout++;
                }
                if (timeout < I2C_TIME_OUT)
                {
                    i2c_flag_clear( dev->i2c_periph, I2C_FLAG_ADDSEND);
                    timeout = 0;
                    i2c_state = I2C_TRANSMIT_DATA;
                }
                else
                {
                    timeout = 0;
                    i2c_state = I2C_START;
                    read_cycle = 0;
                    printf("i2c master clears address flag timeout in READ!\n");
                }
                break;
            case I2C_TRANSMIT_DATA:
                if (RESET == read_cycle)
                {
                    /* wait until the transmit data buffer is empty */
                    while ((!i2c_flag_get( dev->i2c_periph, I2C_FLAG_TBE)) && (timeout < I2C_TIME_OUT))
                    {
                        timeout++;
                    }
                    if (timeout < I2C_TIME_OUT)
                    {
                        /* send the I2C_DEV's internal address to write to : only one byte address */
                        i2c_data_transmit( dev->i2c_periph, reg_addr);
                        timeout = 0;
                    }
                    else
                    {
                        timeout = 0;
                        i2c_state = I2C_START;
                        read_cycle = 0;
                        printf("i2c master wait data buffer is empty timeout in READ!\n");
                    }
                    /* wait until BTC bit is set */
                    while ((!i2c_flag_get( dev->i2c_periph, I2C_FLAG_BTC)) && (timeout < I2C_TIME_OUT))
                    {
                        timeout++;
                    }
                    if (timeout < I2C_TIME_OUT)
                    {
                        timeout = 0;
                        i2c_state = I2C_START;
                        read_cycle++;
                    }
                    else
                    {
                        timeout = 0;
                        i2c_state = I2C_START;
                        read_cycle = 0;
                        printf("i2c master sends I2C_DEV's internal address timeout in READ!\n");
                    }
                }
                else
                {
                    /* one byte master reception procedure (polling) */
                    if (len < 2)
                    {
                        /* disable acknowledge */
                        i2c_ack_config( dev->i2c_periph, I2C_ACK_DISABLE);
                        /* clear ADDSEND register by reading I2C_STAT0 then I2C_STAT1 register (I2C_STAT0 has already been read) */
                        i2c_flag_get( dev->i2c_periph, I2C_FLAG_ADDSEND);
                        /* send a stop condition to I2C bus*/
                        i2c_stop_on_bus( dev->i2c_periph);

                        timeout = 0;
                        /* wait for the byte to be received */
                        while (!i2c_flag_get( dev->i2c_periph, I2C_FLAG_RBNE) && (++timeout < I2C_TIME_OUT));
                        if( timeout<I2C_TIME_OUT )
                        {
                            /* read the byte received from the I2C_DEV */
                            *buf = i2c_data_receive( dev->i2c_periph);
                            /* decrement the read bytes counter */
                            len--;
                            timeout = 0;
                            i2c_state = I2C_STOP;
                        }
                        else
                        {
                            timeout = 0;
                            i2c_state = I2C_START;
                        }
                        
                    }
                    else
                    {
                        /* more than one byte master reception procedure (DMA) */
                        dma_deinit(dev->i2c_dma, dev->i2c_dma_rx_ch);
                        dma_single_data_para_struct_init(&dma_init_parameter);
                        dma_init_parameter.direction = DMA_PERIPH_TO_MEMORY;
                        dma_init_parameter.memory0_addr = (uint32_t)buf;
                        dma_init_parameter.memory_inc = DMA_MEMORY_INCREASE_ENABLE;
                        dma_init_parameter.periph_memory_width = DMA_MEMORY_WIDTH_8BIT;
                        dma_init_parameter.number = len;
                        dma_init_parameter.periph_addr = (uint32_t)&I2C_DATA( dev->i2c_periph);
                        dma_init_parameter.periph_inc = DMA_PERIPH_INCREASE_DISABLE;
                        dma_init_parameter.priority = DMA_PRIORITY_ULTRA_HIGH;
                        dma_single_data_mode_init(dev->i2c_dma, dev->i2c_dma_rx_ch, &dma_init_parameter);
                        /* configure DMA mode */
                        dma_circulation_disable(dev->i2c_dma, dev->i2c_dma_rx_ch);
                        dma_channel_subperipheral_select(dev->i2c_dma, dev->i2c_dma_rx_ch, dev->dma_rx_ch_periph);
                        i2c_dma_last_transfer_config( dev->i2c_periph, I2C_DMALST_ON);
                        /* enable I2C DMA */
                        i2c_dma_config( dev->i2c_periph, I2C_DMA_ON);
                        /* enable DMA RX channel */
                        dma_channel_enable(dev->i2c_dma, dev->i2c_dma_rx_ch);
                        /* wait until FTF bit is set */
                        timeout = 0;
                        while ( !dma_flag_get(dev->i2c_dma, dev->i2c_dma_rx_ch, DMA_FLAG_FTF) && (++timeout < DMA_TIME_OUT) );
                        if (timeout < DMA_TIME_OUT)
                        {
                            // PRINT(" read: I2C DMA wait until FTF bit is ok!666 \n");
                            timeout = 0;
                        }
                        else
                        {
                            PRINT(" read: I2C DMA wait until FTF bit is timeout! 777 \n");
                            timeout = 0;
                            i2c_state = I2C_START;
                            break;
                        }
                        i2c_state = I2C_STOP;
                    }
                }
                break;
            case I2C_STOP:
                /* send a stop condition to I2C bus */
                i2c_stop_on_bus( dev->i2c_periph);
                /* i2c master sends STOP signal successfully */
                while ((I2C_CTL0( dev->i2c_periph) & I2C_CTL0_STOP) && (timeout < I2C_TIME_OUT))
                {
                    timeout++;
                }
                if (timeout < I2C_TIME_OUT)
                {
                    timeout = 0;
                    i2c_state = I2C_END;
                    i2c_timeout_flag = I2C_OK;
                }
                else
                {
                    timeout = 0;
                    i2c_state = I2C_START;
                    read_cycle = 0;
                    printf("i2c master sends stop signal timeout in READ!\n");
                }
                break;
            default:
                i2c_state = I2C_START;
                read_cycle = 0;
                i2c_timeout_flag = I2C_OK;
                timeout = 0;
                printf("i2c master sends start signal in READ.\n");
                break;
        }
    }
    return I2C_END;

#endif

}





```

## 2.3 IIC类设备驱动框架（SC8815 充电控制芯片）
### 2.3.1 IIC头文件

```c
/**
 * @file drv_i2c.h
 * @author wls (ufo281@outlook.com) 
 * @brief 
 * @version 1.0
 * @date 2025-09-25
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#ifndef _DRV_I2C_H_
#define _DRV_I2C_H_

#include "gd32f4xx.h"
#include <stdio.h>
#include <stdint.h>


#define I2C_NUMS    3


#define I2C_SPEED              400000      /*400KHZ*/
#define I2C0_SLAVE_ADDRESS7     0x08    

#define I2C0_PORT           GPIOB
#define I2C0_SDA_IO         GPIO_PIN_6
#define I2C0_SCL_IO         GPIO_PIN_7


#define I2C1_PORT           GPIOB
#define I2C1_SCL_IO         GPIO_PIN_10
#define I2C1_SDA_IO         GPIO_PIN_11


typedef enum {

    IIC_0 = 0,
    IIC_1 = 1,
    IIC_2 = 2

}I2C_em_t;



typedef struct i2c_class_s
{
    uint32_t i2c_speed; /*通讯速率（如100kHz, 400kHz）*/
    uint32_t i2c_clk_src; /* 时钟源 */
    uint32_t gpio_clk_src; /* 时钟源 */
    uint32_t i2c_periph;
    uint32_t i2c_pin_port;   /* i2c io 的port */
    uint32_t i2c_sda_pin;   /*sda的具体IO号*/
    uint32_t i2c_scl_pin;   /*sda的具体IO号*/

    uint8_t (*init)( struct i2c_class_s *dev );
    uint8_t (*gpio_i2c_mode_config)(struct i2c_class_s *dev );

    uint8_t (*write_regs)(  uint32_t i2cx,
                            uint8_t i2c_addr, // IIC device address(8bit)
                            uint8_t reg_addr,     // reg address
                            uint32_t len,          // bytes of length
                            uint8_t *buf          // data address
    );

    uint8_t (*read_regs)( uint32_t i2cx,
                          uint8_t i2c_addr, // IIC device address(8bit)
                          uint8_t reg_addr,     // reg address
                          uint32_t len,          // bytes of length
                          uint8_t *buf          // data address
    );



} i2c_class_s_t;

uint8_t gpio_i2c_mode_cfg(struct i2c_class_s *dev );
uint8_t i2c_init( struct i2c_class_s *dev );
uint8_t write_regs( uint32_t i2cx,
                    uint8_t i2c_addr, 
                    uint8_t reg_addr,     
                    uint32_t len,          
                    uint8_t *buf);
                        
uint8_t read_regs(  uint32_t i2cx,
                    uint8_t i2c_addr,
                    uint8_t reg_addr,     
                    uint32_t len,          
                    uint8_t *buf);

extern i2c_class_s_t g_i2c[I2C_NUMS];



#endif /* _DRV_I2C_H_ */

```

### 2.3.2 IIC源文件

```c
/**
 * @file drv_i2c.c
 * @author wls (ufo281@outlook.com) 
 * @brief 
 * @version 1.0
 * @date 2025-09-25
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#include "drv_i2c.h"




i2c_class_s_t g_i2c[I2C_NUMS] =
{
    [IIC_0] = {//[0]
        
        .i2c_speed = I2C_SPEED,/*400KHZ*/
        .i2c_periph = I2C0,
        .i2c_clk_src = RCU_I2C0,
        .gpio_clk_src = RCU_GPIOB,
        .i2c_pin_port = I2C0_PORT,
        .i2c_scl_pin = I2C0_SCL_IO,
        .i2c_sda_pin = I2C0_SDA_IO,
        .init = i2c_init,
        .gpio_i2c_mode_config = gpio_i2c_mode_cfg,
        .read_regs  = read_regs,
        .write_regs = write_regs
    },
    [IIC_1] = {//[1]

        .i2c_speed = I2C_SPEED,/*400KHZ*/
        .i2c_periph = I2C1,
        .gpio_clk_src = RCU_GPIOB,
        .i2c_clk_src = RCU_I2C1,
        .i2c_pin_port = I2C1_PORT,
        .i2c_scl_pin = I2C1_SCL_IO,
        .i2c_sda_pin = I2C1_SDA_IO,
        .init = i2c_init,
        .gpio_i2c_mode_config = gpio_i2c_mode_cfg,
        .read_regs  = read_regs,
        .write_regs = write_regs,

    },
    // {//[2]
    //     .i2c_speed = I2C_SPEED,/*400KHZ*/
    //     .i2c_periph = I2C2,
    //     .i2c_scl_pin = I2C0_SCL_IO,
    //     .i2c_sda_pin = I2C0_SDA_IO,
    //     .i2c_init = i2c0_init,
    //     .read_regs  = read_regs,
    //     .write_regs = write_regs
    // }
} ;



uint8_t gpio_i2c_mode_cfg(struct i2c_class_s *dev )
{
    if (dev==NULL)
    {
        return 1;
    }
    /* enable GPIO PORT clock */
    rcu_periph_clock_enable(dev->gpio_clk_src);

    gpio_af_set(dev->i2c_pin_port, GPIO_AF_4, dev->i2c_scl_pin);

    gpio_af_set(dev->i2c_pin_port, GPIO_AF_4, dev->i2c_sda_pin);

    gpio_mode_set(dev->i2c_pin_port, GPIO_MODE_AF, GPIO_PUPD_PULLUP,dev->i2c_sda_pin);
    gpio_output_options_set(dev->i2c_pin_port, GPIO_OTYPE_OD, GPIO_OSPEED_50MHZ,dev->i2c_sda_pin);

    gpio_mode_set(dev->i2c_pin_port, GPIO_MODE_AF, GPIO_PUPD_PULLUP, dev->i2c_scl_pin);
    gpio_output_options_set(dev->i2c_pin_port, GPIO_OTYPE_OD, GPIO_OSPEED_50MHZ, dev->i2c_scl_pin);
    
    return 0;
}


uint8_t i2c_init( struct i2c_class_s *dev )
{

    if (dev==NULL || dev->gpio_i2c_mode_config == NULL )
    {
        return 1;
    }

    

    /*-----------------GPIO- CONFIG-----------------------------*/
    dev->gpio_i2c_mode_config(dev);
    
    /*-----------------GPIO- CONFIG-----------------------------*/



    /*-----------------I2C0- CONFIG-----------------------------*/
    /*I2C CLOCK Enable*/
    rcu_periph_clock_enable(dev->i2c_clk_src);


    /* configure I2C clock */
    i2c_clock_config( dev->i2c_periph, dev->i2c_speed,I2C_DTCY_2);
    /* configure I2C address */
    i2c_mode_addr_config(dev->i2c_periph,
                        I2C_I2CMODE_ENABLE,
                        I2C_ADDFORMAT_7BITS,
                        I2C0_SLAVE_ADDRESS7);
    /* enable i2cx */
    i2c_enable(dev->i2c_periph);
    /* enable acknowledge */
    i2c_ack_config(dev->i2c_periph,I2C_ACK_ENABLE);

    /*-----------------i2cx- CONFIG-----------------------------*/

    return 0;
}



/**
 * @brief i2cx write regs 
 * 
 * @param i2c_addr IIC device address(8bit)
 * @param reg_addr reg address
 * @param len bytes of length
 * @param buf data address
 * @return uint8_t 
 */
uint8_t write_regs( uint32_t i2cx,
                    uint8_t i2c_addr, 
                    uint8_t reg_addr,     
                    uint32_t len,          
                    uint8_t *buf          
                 )
{

    /* wait until I2C bus is idle */
    while(i2c_flag_get(i2cx, I2C_FLAG_I2CBSY));
    
    /* send a start condition to I2C bus */
    i2c_start_on_bus(i2cx);
    
    /* wait until SBSEND bit is set */
    while(!i2c_flag_get(i2cx, I2C_FLAG_SBSEND));
    
    /* send slave address to I2C bus */
    i2c_master_addressing(i2cx, i2c_addr, I2C_TRANSMITTER);
    
    /* wait until ADDSEND bit is set */
    while(!i2c_flag_get(i2cx, I2C_FLAG_ADDSEND));
    
    /* clear the ADDSEND bit */
    i2c_flag_clear(i2cx,I2C_FLAG_ADDSEND);
    
    /* wait until the transmit data buffer is empty */
    while( SET != i2c_flag_get(i2cx, I2C_FLAG_TBE));
    
    /* send the EEPROM's internal address to write to : only one byte address */
    i2c_data_transmit(i2cx, reg_addr);
    
    /* wait until BTC bit is set */
    while(!i2c_flag_get(i2cx, I2C_FLAG_BTC));
    
    /* while there is data to be written */
    while(len--)
    {  
        i2c_data_transmit(i2cx, *buf);
        /* point to the next byte to be written */
        buf++; 
        /* wait until BTC bit is set */
        while(!i2c_flag_get(i2cx, I2C_FLAG_BTC));
    }

    /* send a stop condition to I2C bus */
    i2c_stop_on_bus(i2cx);
    
    /* wait until the stop condition is finished */
    while(I2C_CTL0(i2cx)&I2C_CTL0_STOP);


    return 0;
}


/**
 * @brief i2cx read regs
 * 
 * @param i2c_addr IIC device address(8bit)
 * @param reg_addr reg address
 * @param len bytes of length
 * @param buf data address
 * @return uint8_t 
 */
uint8_t read_regs(  uint32_t i2cx,
                    uint8_t i2c_addr,
                    uint8_t reg_addr,     
                    uint32_t len,          
                    uint8_t *buf          
                )
{

    /* wait until I2C bus is idle */
    while(i2c_flag_get(i2cx, I2C_FLAG_I2CBSY));

    if(2 == len){
        i2c_ackpos_config(i2cx,I2C_ACKPOS_NEXT);
    }
    
    /* send a start condition to I2C bus */
    i2c_start_on_bus(i2cx);
    
    /* wait until SBSEND bit is set */
    while(!i2c_flag_get(i2cx, I2C_FLAG_SBSEND));
    
    /* send slave address to I2C bus */

    i2c_master_addressing(i2cx, i2c_addr, I2C_TRANSMITTER);
    
    /* wait until ADDSEND bit is set */
    while(!i2c_flag_get(i2cx, I2C_FLAG_ADDSEND));
    
    /* clear the ADDSEND bit */
    i2c_flag_clear(i2cx,I2C_FLAG_ADDSEND);
    
    /* wait until the transmit data buffer is empty */
    while(SET != i2c_flag_get( i2cx , I2C_FLAG_TBE));

    /* enable i2cx*/
    i2c_enable(i2cx);
    
    /* send the EEPROM's internal address to write to */
    i2c_data_transmit(i2cx, reg_addr);  
    
    /* wait until BTC bit is set */
    while(!i2c_flag_get(i2cx, I2C_FLAG_BTC));
    
    /* send a start condition to I2C bus */
    i2c_start_on_bus(i2cx);
    
    /* wait until SBSEND bit is set */
    while(!i2c_flag_get(i2cx, I2C_FLAG_SBSEND));
    
    /* send slave address to I2C bus */
    i2c_master_addressing(i2cx, i2c_addr, I2C_RECEIVER);

    if(len < 3){
        /* disable acknowledge */
        i2c_ack_config(i2cx,I2C_ACK_DISABLE);
    }
    
    /* wait until ADDSEND bit is set */
    while(!i2c_flag_get(i2cx, I2C_FLAG_ADDSEND));
    
    /* clear the ADDSEND bit */
    i2c_flag_clear(i2cx,I2C_FLAG_ADDSEND);
    
    if(1 == len){
        /* send a stop condition to I2C bus */
        i2c_stop_on_bus(i2cx);
    }
    
    /* while there is data to be read */
    while(len){
        if(3 == len){
            /* wait until BTC bit is set */
            while(!i2c_flag_get(i2cx, I2C_FLAG_BTC));

            /* disable acknowledge */
            i2c_ack_config(i2cx,I2C_ACK_DISABLE);
        }
        if(2 == len){
            /* wait until BTC bit is set */
            while(!i2c_flag_get(i2cx, I2C_FLAG_BTC));
            
            /* send a stop condition to I2C bus */
            i2c_stop_on_bus(i2cx);
        }
        
        /* wait until the RBNE bit is set and clear it */
        if(i2c_flag_get(i2cx, I2C_FLAG_RBNE)){
            /* read a byte from the EEPROM */
            *buf = i2c_data_receive(i2cx);
            
            /* point to the next location where the byte read will be saved */
            buf++; 
            
            /* decrement the read bytes counter */
            len--;
        } 
    }
    
    /* wait until the stop condition is finished */
    while(I2C_CTL0(i2cx)&I2C_CTL0_STOP);
    /* enable acknowledge */
    i2c_ack_config(i2cx,I2C_ACK_ENABLE);

    i2c_ackpos_config(i2cx,I2C_ACKPOS_CURRENT);

    return 0;
}





```



## 2.4 具体的设备驱动实现

### 2.4.1 头文件

```c
/**
 * @file sc8815.h
 * @author wls (ufo281@outlook.com) 
 * @brief 
 * @version 1.0
 * @date 2025-09-25
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#ifndef _SC8815_H_
#define _SC8815_H_

#include "charge_ctrl_drv.h"
#include "drv_gpio.h"




#define SC8815_I2C_ADDR_8b_W        0xE8    /*8bit i2c write addr*/
#define SC8815_I2C_ADDR_8b_R        0xE9    /*8bit i2c read addr*/
#define SC8815_I2C_ADDR_7b          0x74    /*7bit i2c addr*/



bool sc8815_gpio_write(uint32_t pin_port, uint32_t pin, bit_status value);
uint8_t sc8815_a_init(struct Chg_Ctrl_Dev_s *dev);
uint8_t sc8815_en( struct Chg_Ctrl_Dev_s *dev, uint8_t en_flag);
uint8_t sc8815_pstop_en(struct Chg_Ctrl_Dev_s *dev,uint8_t en_flag);
uint8_t sc8815_reads_regs(  uint8_t i2c_addr,
                            uint8_t reg_addr,
                            uint8_t *data_buffer,
                            uint16_t number_of_byte);
uint8_t sc8815_write_regs(  uint8_t i2c_addr,
                            uint8_t reg_addr,
                            uint8_t *data_buffer,
                            uint8_t number_of_byte);
int8_t sc8815_adc_get_voltage_current(struct Chg_Ctrl_Dev_s *dev);
bool sc8815_get_pstop_flag(struct Chg_Ctrl_Dev_s *dev);
uint8_t sc8815_set_current(struct Chg_Ctrl_Dev_s *dev, uint16_t ibus, uint16_t ibat);
uint8_t sc8815_charge_disable(struct Chg_Ctrl_Dev_s *dev);
uint8_t sc8815_charge_enable(struct Chg_Ctrl_Dev_s *dev, bool enable);
int8_t sc8815_activate_battery(struct Chg_Ctrl_Dev_s *dev);


#endif 

```

### 2.4.2 源文件

```c
/**
 * @file sc8815.c
 * @author wls (ufo281@outlook.com) 
 * @brief 
 * @version 1.0
 * @date 2025-09-25
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#include "sc8815.h"


bool sc8815_gpio_write(uint32_t pin_port, uint32_t pin, bit_status value)
{
    if ( g_gpio.write_bit!=NULL )
    {
        g_gpio.write_bit(pin_port, pin, value);
    }
    
}



uint8_t sc8815_a_init(struct Chg_Ctrl_Dev_s *dev)
{

    if (dev==NULL)
    {
        return 1;
    }
    
    if ( (dev->chip_en ==NULL)        || 
         (dev->chip_pstop_en == NULL) ||
         (dev->write_regs == NULL)    ||
         (dev->read_regs == NULL) )  

    {
        return 2;
    }
    

    uint8_t val_wr, val_rd, ratio_reg = 0;

    
    // husb238 set sucess,then enable sc8815
    dev->chip_pstop_en( dev, 0);
    // gpio_output(dev->pstop, OFF);

    dev->chip_en( dev, 1);
    // gpio_output(dev->sc8815_en, ON);
    // gpio_output(dev->vbat_en, ON);      // 保持电池回路的电一直打开。TODO：放在电池插拔处进行管理？

    dev->read_regs(dev->i2c_addr,dev->regs_addr_list.vbat_set, &val_rd,1);
    // sc8815_reg_read(dev->iic, SS8815_REG_VBAT_SET, &val_rd);


    val_wr = val_rd;
    val_wr &= ~(0x03 << 6); // 不使用温度补偿值，如果不能充满，再尝试调整该值
    val_wr &= ~(0x01 << 5); // VBAT电压设置为内部设置
    val_wr &= ~(0x03 << 3);
#if (BATTERY_SERIES == BATTERY_1S)
    val_wr |=  (0x00 << 3);  // 1s
#elif (BATTERY_SERIES == BATTERY_2S)
    val_wr |=  (0x01 << 3);  // 2s
#elif (BATTERY_SERIES == BATTERY_3S)
    val_wr |=  (0x02 << 3);  // 3s
#elif (BATTERY_SERIES == BATTERY_4S)
    val_wr |=  (0x03 << 3);  // 4s
#else
    val_wr |=  (0x01 << 3);  // 2s
#endif
    val_wr &= ~(0x07 << 0);
#if (BATTERY_MAX_VOL == BATTERY_VOL_4_10)
    val_wr |= 0x0;  // 4.1V
#elif (BATTERY_MAX_VOL == BATTERY_VOL_4_20)
    val_wr |= 0x1;  // 4.2V
#elif (BATTERY_MAX_VOL == BATTERY_VOL_4_25)
    val_wr |= 0x2;  // 4.25V
#elif (BATTERY_MAX_VOL == BATTERY_VOL_4_30)
    val_wr |= 0x3;  // 4.3V
#elif (BATTERY_MAX_VOL == BATTERY_VOL_4_35)
    val_wr |= 0x4;  // 4.35V
#elif (BATTERY_MAX_VOL == BATTERY_VOL_4_40)
    val_wr |= 0x5;  // 4.4V
#elif (BATTERY_MAX_VOL == BATTERY_VOL_4_45)
    val_wr |= 0x6;  // 4.45V
#elif (BATTERY_MAX_VOL == BATTERY_VOL_4_50)
    val_wr |= 0x7;  // 4.5V
#else
    val_wr |= 0x1;  // 4.2V
#endif

    dev->write_regs(dev->i2c_addr ,dev->regs_addr_list.vbat_set, &val_wr,1);
    // sc8815_reg_write(dev->iic, SS8815_REG_VBAT_SET, val_wr);

    // 回读校验
    dev->read_regs(dev->i2c_addr,dev->regs_addr_list.vbat_set, &val_rd,1);
    dev->read_regs(dev->i2c_addr,dev->regs_addr_list.vbat_set, &dev->regs_value.vbat_set,1);

    // sc8815_reg_read(dev->iic, SS8815_REG_VBAT_SET, &val_rd);
    if (val_wr != val_rd) {
        PRINT("sc8815_init REG_VBAT_SET write error, wr=[%#X], rd=[%#X]", val_wr, val_rd);
    }

    // VINREG参考电压,VBUS最小电压阈值
    val_wr = 112;
    dev->write_regs(dev->i2c_addr ,dev->regs_addr_list.vinreg_set, &val_wr,1);
    // sc8815_reg_write(dev->iic, SS8815_REG_VINREG_SET, val_wr);  // 4.52V (x+1)*40 = VINREG

    // 回读校验
    dev->read_regs(dev->i2c_addr,dev->regs_addr_list.vinreg_set, &val_rd,1);
    // sc8815_reg_read(dev->iic, SS8815_REG_VINREG_SET, &val_rd);
    if (val_wr != val_rd) {
        PRINT("sc8815_init REG_VINREG_SET write error, wr=[%#X], rd=[%#X]", val_wr, val_rd);
    }

    // 电压/电流倍数
    dev->read_regs(dev->i2c_addr,dev->regs_addr_list.ratio, &val_rd,1);
    // sc8815_reg_read(dev->iic, SS8815_REG_RATIO, &val_rd);
    val_wr = val_rd;
    val_wr &= ~(0x01 << 4);
    val_wr |=  (0x00 << 4); // IBAT_R=6, 对于 65W 充电头 6A 的 ibat 足够了
    val_wr &= ~(0x03 << 2);
    val_wr |=  (0x01 << 2); // IBUS_R=6
    val_wr &= ~(0x01 << 1);
    val_wr |=  (SC8815_VBUS_RATIO << 1); // VBAT_R: 0->12.5 or 1->5
    val_wr &= ~(0x01 << 0); // VBUS_R=12.5
    
    dev->write_regs(dev->i2c_addr ,dev->regs_addr_list.ratio, &val_wr,1);
    // sc8815_reg_write(dev->iic, SS8815_REG_RATIO, val_wr);

    // 回读校验
    dev->read_regs(dev->i2c_addr,dev->regs_addr_list.ratio, &val_rd,1);
    dev->read_regs(dev->i2c_addr,dev->regs_addr_list.ratio, &dev->regs_value.ratio,1);

    // sc8815_reg_read(dev->iic, SS8815_REG_RATIO, &val_rd);
    if (val_wr != val_rd) {
        PRINT("sc8815_init REG_RATIO write error, wr=[%#X], rd=[%#X]", val_wr, val_rd);
    }
    // 回读记录
    ratio_reg = (val_rd >> 0) & 0x1;
    if (0 == ratio_reg)
        dev->adc_detec_bat_info.vbus_ratio_10 = 125;
    else
        dev->adc_detec_bat_info.vbus_ratio_10 = 50;
    ratio_reg = (val_rd >> 1) & 0x1;
    if (0 == ratio_reg)
        dev->adc_detec_bat_info.vbat_ratio_10 = 125;
    else
        dev->adc_detec_bat_info.vbat_ratio_10 = 50;
    ratio_reg = (val_rd >> 2) & 0x3;
    if (0x01 == ratio_reg)
        dev->adc_detec_bat_info.ibus_ratio = 6;
    else if (0x02 == ratio_reg)
        dev->adc_detec_bat_info.ibus_ratio = 3;
    else {
        dev->adc_detec_bat_info.ibus_ratio = 6;
        PRINT("dev_BUS RATIO read error!");
    }
    ratio_reg = (val_rd >> 4) & 0x1;
    if (0 == ratio_reg)
        dev->adc_detec_bat_info.ibat_ratio = 6;
    else
        dev->adc_detec_bat_info.ibat_ratio = 12;

    // 设置为充电模式
    dev->read_regs(dev->i2c_addr,dev->regs_addr_list.ctrl0_set, &val_rd,1);
    // sc8815_reg_read(dev->iic, SS8815_REG_CTRL0_SET, &val_rd);
    val_wr = val_rd;
    val_wr &= ~(0x01 << 7); // 充电模式
    val_wr &= ~(0x01 << 4);
    val_wr |=  (0x01 << 4); // VINREG 倍数,1:40x,0:100x
    val_wr &= ~(0x03 << 2);
    val_wr |=  (0x01 << 2); // 开关频率为 300kHz
    val_wr &= ~(0x03 << 0);
    val_wr |=  (0x03 << 0); // 死区时间为 80ns
    dev->write_regs(dev->i2c_addr ,dev->regs_addr_list.ctrl0_set, &val_wr,1);
    // sc8815_reg_write(dev->iic, SS8815_REG_CTRL0_SET, val_wr);

    // 回读校验
    dev->read_regs(dev->i2c_addr,dev->regs_addr_list.ctrl0_set, &val_rd,1);
    // sc8815_reg_read(dev->iic, SS8815_REG_CTRL0_SET, &val_rd);
    if (val_wr != val_rd) {
        PRINT("sc8815_init REG_CTRL0_SET write error, wr=[%#X], rd=[%#X]", val_wr, val_rd);
    }

    // 充电电流选择
    dev->read_regs(dev->i2c_addr,dev->regs_addr_list.ctrl1_set, &val_rd,1);
    // sc8815_reg_read(dev->iic, SS8815_REG_CTRL1_SET, &val_rd);
    val_wr = val_rd;
    val_wr &= ~(0x01 << 7);
    val_wr |=  (0x01 << 7); // 1: IBAT as charging current
    val_wr &= ~(0x01 << 6);
    //val_wr |=  (0x01 << 6); // disable trickle charge
    val_wr &= ~(0x01 << 5);
    dev->write_regs(dev->i2c_addr ,dev->regs_addr_list.ctrl1_set, &val_wr,1);
    // sc8815_reg_write(dev->iic, SS8815_REG_CTRL1_SET, val_wr);
    // 回读校验
    dev->read_regs(dev->i2c_addr,dev->regs_addr_list.ctrl1_set, &val_rd,1);
    // sc8815_reg_read(dev->iic, SS8815_REG_CTRL1_SET, &val_rd);
    if (val_wr != val_rd) {
        PRINT("sc8815_init REG_CTRL1_SET write error, wr=[%#X], rd=[%#X]", val_wr, val_rd);
    }

    // factory设置 
    dev->read_regs(dev->i2c_addr,dev->regs_addr_list.ctrl2_set, &val_rd,1);
    // sc8815_reg_read(dev->iic, SS8815_REG_CTRL2_SET, &val_rd);
    val_wr = val_rd;
    val_wr &= ~(0x01 << 5);
    val_wr |=  (0x01 << 3);
    dev->write_regs(dev->i2c_addr ,dev->regs_addr_list.ctrl2_set, &val_wr,1);
    // sc8815_reg_write(dev->iic, SS8815_REG_CTRL2_SET, val_wr);

    // 回读校验
    dev->read_regs(dev->i2c_addr,dev->regs_addr_list.ctrl2_set, &val_rd,1);
    // sc8815_reg_read(dev->iic, SS8815_REG_CTRL2_SET, &val_rd);
    if (val_wr != val_rd) {
        PRINT("sc8815_init REG_CTRL2_SET write error, wr=[%#X], rd=[%#X]", val_wr, val_rd);
    }

    // GPO设置,adc_detec_bat_info,EOC
    dev->read_regs(dev->i2c_addr,dev->regs_addr_list.ctrl3_set, &val_rd,1);
    // sc8815_reg_read(dev->iic, SS8815_REG_CTRL3_SET, &val_rd);
    val_wr = val_rd;
    /**
     * @brief 《应用注意事项》 P6：3.1 PSTOP 拉高时，设置 AD_START 0x0C[5] = 0 ，可以有效降低待机电流
     *        《应用注意事项》 P6：3.2 需要读 adc_detec_bat_info 时，将 AD_START 置 1 ，然后等待 750us （内部 adc_detec_bat_info 转换时间），再读 adc_detec_bat_info 。
     *                                在等待期间，不能有读写任何 adc_detec_bat_info 寄存器 的操作，否则会影响 adc_detec_bat_info 寄存器值更新。
     *        《应用注意事项》 P7：9.对于 3S 以上的应用，电池充满后，需要将 PSTOP 拉高， adc_detec_bat_info 功能关闭，如果需要使用 adc_detec_bat_info 功能，在使用完成后也要及时关闭
     */
    val_wr &= ~(0x01 << 5);
    val_wr |=  (0x01 << 5); // 使能 adc_detec_bat_info 转换
    val_wr &= ~(0x01 << 4); // 内部环路使用
    val_wr &= ~(0x01 << 3); // 内部环路使用
    val_wr &= ~(0x01 << 1); // 充电结束电流设置为充电限制电流的1/25
    dev->write_regs(dev->i2c_addr ,dev->regs_addr_list.ctrl3_set, &val_wr,1);
    // sc8815_reg_write(dev->iic, SS8815_REG_CTRL3_SET, val_wr);

    // 回读校验
    dev->read_regs(dev->i2c_addr,dev->regs_addr_list.ctrl3_set, &val_rd,1);
    // sc8815_reg_read(dev->iic, SS8815_REG_CTRL3_SET, &val_rd);
    if (val_wr != val_rd) {
        PRINT("sc8815_init REG_CTRL3_SET write error, wr=[%#X], rd=[%#X]", val_wr, val_rd);
    }

    // 关中断
    dev->read_regs(dev->i2c_addr,dev->regs_addr_list.mask, &val_rd,1);
    // sc8815_reg_read(dev->iic, SS8815_REG_MASK, &val_rd);
    val_wr = val_rd;
    val_wr &= ~(0x01 << 6);
    val_wr |=  (0x01 << 6); // disabled AC_OK_Mask
    val_wr &= ~(0x01 << 5);
    val_wr |=  (0x01 << 5); // disabled INDET_Mask
    val_wr &= ~(0x01 << 3);
    val_wr |=  (0x01 << 3); // disabled VBUS_SHORT_Mask
    val_wr &= ~(0x01 << 2);
    val_wr |=  (0x01 << 2); // disabled OTP_Mask
    val_wr &= ~(0x01 << 1);
    val_wr |=  (0x01 << 1); // disabled EOC_Mask
    val_wr &= ~(0x01 << 0);
    val_wr |=  (0x01 << 0); // Write this bit to 1 after power up
    dev->write_regs(dev->i2c_addr ,dev->regs_addr_list.mask, &val_wr,1);
    // sc8815_reg_write(dev->iic, SS8815_REG_MASK, val_wr);

    // 回读校验
    dev->read_regs(dev->i2c_addr,dev->regs_addr_list.mask, &val_rd,1);
    // sc8815_reg_read(dev->iic, SS8815_REG_MASK, &val_rd);
    if (val_wr != val_rd) {
        PRINT("sc8815_init REG_MASK write error, wr=[%#X], rd=[%#X]", val_wr, val_rd);
    }

    // delay_1ms(10);   // 《应用注意事项》 P9：2.Sleep 模式转充电模式设置方式: 维持 PSTOP 高 -> 设置充电电压 电流等参数 -> OTG=0, delay 10ms -> PSTOP 拉低

    // 读取所有寄存器
    dev->read_regs(dev->i2c_addr,dev->regs_addr_list.vbat_set, &dev->regs_value.vbat_set ,sizeof(dev->regs_value));

    // sc8815_reg_read_multi(dev->iic, SS8815_REG_VBAT_SET, dev->config.reg_buffer, sizeof(dev->config.reg_buffer));

    return 0;
}




/**
 * @brief 启用SC8815 SET Hight level, eg:en_flag == ON
 * 
 * @param dev 
 * @param en_flag SET:hight
 *                RESET:low
 * @return uint8_t 1:failed!
 *                 0:ok!
 */
uint8_t sc8815_en( struct Chg_Ctrl_Dev_s *dev, uint8_t en_flag)
{
    if (dev->gpio_write==NULL)
    {
        return 1;
    }
    else
    {
        dev->gpio_write(dev->en_pin_port, dev->en_pin, !en_flag);
        return 0;
    }

}



/**
 * @brief 开启关闭SC8815 电源模块
 * 
 * @param port 
 * @param pstop_io 
 * @param en_flag SET:hight ,enable
 *                RESET:low ,disable
 */
uint8_t sc8815_pstop_en(struct Chg_Ctrl_Dev_s *dev,uint8_t en_flag)
{

    if (dev->gpio_write==NULL)
    {
        return 1;
    }
    else
    {
        dev->gpio_write(dev->pstop_pin_port, dev->pstop_pin, !en_flag);
        return 0;
    }

        
}




/**
 * @brief 读取多个寄存器数据
 *
 * @param i2c_addr
 * @param reg_addr 寄存器地址
 * @param data_buffer 数据地址
 * @param number_of_byte 字节长度
 * @return uint8_t
 */
uint8_t sc8815_reads_regs(  uint8_t i2c_addr,
                            uint8_t reg_addr,
                            uint8_t *data_buffer,
                            uint16_t number_of_byte)
{

    if (g_i2c[IIC_0].read_regs != NULL)
    {
        g_i2c[IIC_0].read_regs( g_i2c[IIC_0].i2c_periph, 
                                i2c_addr,
                                reg_addr,
                                number_of_byte,
                                data_buffer);
    }

    return 0;

}




/**
 * @brief 写寄存器
 *
 * @param i2c_addr
 * @param reg_addr
 * @param data_buffer
 * @param number_of_byte
 * @return uint8_t
 */
uint8_t sc8815_write_regs(  uint8_t i2c_addr,
                            uint8_t reg_addr,
                            uint8_t *data_buffer,
                            uint8_t number_of_byte)
{

    if (g_i2c[IIC_0].write_regs!= NULL)
    {
        g_i2c[IIC_0].write_regs(g_i2c[IIC_0].i2c_periph,
                                i2c_addr,
                                reg_addr,
                                number_of_byte,
                                data_buffer);
    }

    return 0;
}



/**
 * @brief SC8815 获取充电电源电流，电压，
 *              以及充电电池的电流，电压
 * 
 * @param dev 
 * @return int8_t 0:ok
 */
int8_t sc8815_adc_get_voltage_current(struct Chg_Ctrl_Dev_s *dev)
{
    if (dev==NULL)
    {
        return 1;
    }

    if ( (dev->read_regs == NULL) )  
    {
        return 2;
    }
    
    uint8_t adc_buf[8] = {0};

    dev->read_regs( dev->i2c_addr, dev->regs_addr_list.vbus_fb_value, adc_buf, sizeof(adc_buf) );
    // sc8815_reg_read_multi(dev., SS8815_REG_VBUS_FB_VALUE, adc_buf, sizeof(adc_buf));

    uint16_t temp_reg = 0;

    // get vbus 充电电源的电压
    temp_reg = (((adc_buf[0] << 8) | adc_buf[1]) >> 6);
    dev->adc_detec_bat_info.vbus = (0 == temp_reg) ? 0 : (temp_reg+1) * dev->adc_detec_bat_info.vbus_ratio_10 * 2 /10;  // vbus_ratio/10*2，其中10是因为在读取的时候放大了十倍

    // get vbat 电池的电压
    temp_reg = (((adc_buf[2] << 8) | adc_buf[3]) >> 6);
    dev->adc_detec_bat_info.vbat = (0 == temp_reg) ? 0 : (temp_reg+1) * dev->adc_detec_bat_info.vbat_ratio_10 * 2 /10;  // vbat_ratio/10*2，其中10是因为在读取的时候放大了十倍

    // get ibus 充电电源的电流
    temp_reg = (((adc_buf[4] << 8) | adc_buf[5]) >> 6);
    dev->adc_detec_bat_info.ibus = (0 == temp_reg) ? 0 : (temp_reg+1) * dev->adc_detec_bat_info.ibus_ratio * 5 / 3;  // adc_detec_bat_info->ibus * ibus_ratio / 600 * 1000

    // get ibat 电池的电流
    temp_reg = (((adc_buf[6] << 8) | adc_buf[7]) >> 6);
    dev->adc_detec_bat_info.ibat = (0 == temp_reg) ? 0 : (temp_reg+1) * dev->adc_detec_bat_info.ibat_ratio * 5 / 3;

    return 0;
}


/**
 * @brief 返回pstop 标志
 * 
 * @param dev 
 * @return uint8_t false：failed! 
 *                 true: ok!
 */
bool sc8815_get_pstop_flag(struct Chg_Ctrl_Dev_s *dev)
{ 
    if (dev==NULL)
    {   
        return false;
    }
    
    return dev->pstop_flag;
}


uint8_t sc8815_set_current(struct Chg_Ctrl_Dev_s *dev, uint16_t ibus, uint16_t ibat)
{
    if (dev==NULL)
    {
        return 1;
    }
    if ( (dev->chip_en ==NULL) || 
         (dev->chip_pstop_en == NULL)   ||
         (dev->write_regs == NULL)      ||
         (dev->read_regs == NULL)       ||
         (dev->charge_enable == NULL)   ||
         (dev->get_pstop_flag == NULL) )  

    {
        return 2;
    }

    float ibus_tag = ((float)ibus / 1000.0f);
    float ibat_tag = ((float)ibat / 1000.0f);
    if (ibus_tag < SC8815_MIN_IBUS) {
        ibus_tag = SC8815_MIN_IBUS;
        PRINT("set_SC8815_current current too low, set ibus to 300mA \n");
    }
    if (ibat_tag < SC8815_MIN_IBAT) {
        ibat_tag = SC8815_MIN_IBAT;
        PRINT("set_SC8815_current current too low, set ibat to 300mA \n");
    }

    uint8_t current_limit_buf[2] = {0};
    dev->read_regs(dev->i2c_addr, dev->regs_addr_list.ibus_lim_set, current_limit_buf,sizeof(current_limit_buf) );
    // sc8815_reg_read_multi(dev->iic, SS8815_REG_IBUS_LIM_SET, current_limit_buf, sizeof(current_limit_buf));
    dev->config.ibus_reg = current_limit_buf[0];
    dev->config.ibat_reg = current_limit_buf[1];

    dev->config.ibus_limit = (float)(dev->config.ibus_reg + 1) * dev->adc_detec_bat_info.ibus_ratio / 256.0;
    dev->config.ibat_limit = (float)(dev->config.ibat_reg + 1) * dev->adc_detec_bat_info.ibat_ratio / 256.0;

    uint8_t ibus_wr = ((ibus_tag) * 256 / dev->adc_detec_bat_info.ibus_ratio) - 1;
    if (ibus_wr != dev->config.ibus_reg) 
    {
        dev->write_regs(dev->i2c_addr, dev->regs_addr_list.ibus_lim_set, &ibus_wr, sizeof(ibus_wr) );
        // sc8815_reg_write(dev->iic, SS8815_REG_IBUS_LIM_SET, ibus_wr);
        dev->config.ibus_reg = ibus_wr;
        dev->config.ibus_limit = ibus_tag;
        PRINT(" ibus: %.2fmA  reg:%d", ibus_tag, ibus_wr);
    }

    uint8_t ibat_wr = ((ibat_tag) * 256 / dev->adc_detec_bat_info.ibat_ratio) - 1;
    if (ibat_wr != dev->config.ibat_reg) 
    {
        dev->write_regs(dev->i2c_addr, dev->regs_addr_list.ibat_lim_set, &ibat_wr, sizeof(ibat_wr) );
        // sc8815_reg_write(dev->iic, SS8815_REG_IBAT_LIM_SET, ibat_wr);
        dev->config.ibat_reg = ibat_wr;
        dev->config.ibat_limit = ibat_tag;
        PRINT(" ibat: %.2fmA  reg:%d", ibat_tag, ibat_wr);
    }

    if ( dev->get_pstop_flag(dev) != false ) 
    {
        dev->charge_enable(dev,true);
        // charge_enable(dev, true);
        PRINT(" enable charge \n");
    }
    return 0;
}



uint8_t sc8815_charge_disable(struct Chg_Ctrl_Dev_s *dev)
{
    if (dev==NULL)
    {
        return 1;
    }

    if ( dev->chip_pstop_en !=NULL )
    {
        dev->chip_pstop_en( dev, 0);
    }
    else{
        PRINT(" sc8815_charge_disable! failed! 2 \n");
        return 2;
    }
    
    return 0;

}




/**
 * @brief 
 * 
 * @param dev 
 * @param enable 1:enable
 *               0:disable
 */
uint8_t sc8815_charge_enable(struct Chg_Ctrl_Dev_s *dev, bool enable)
{
    if (dev==NULL)
    {
        return 1;
    }

    if (dev->pstop_flag == enable) 
    {
        PRINT(" sc8815_charge_enable had is enable 2! \n");

        return 2;
    }

    if ( dev->chip_pstop_en !=NULL )
    {
        dev->chip_pstop_en( dev, enable);
    }
    else{
        PRINT(" sc8815_charge_enable! failed!3 \n");
        return 3;
    }

    dev->pstop_flag = enable; // 需要先置位，否则 ADC 无法进行读取

    if (true == enable) 
    {
        // gpio_output(dev->vbat_en, ON);       // 保持电池通路一直打开
        // delay_1ms(10);                    // 《应用注意事项》 P7：8.适配器插入后，需要把输入端的隔离管完全打开， delay 10ms ，再将 PSTOP 拉低使能充电，以防误触 VINREG 和 EOC
        
        if (dev->adc_get_voltage_current !=NULL)
        {
            dev->adc_get_voltage_current(dev);
            // sc8815_adc_read(dev);                   // 《应用注意事项》 P6：2.1.对实际电池电压进行检测。如果实际 VBAT 电压高于 VBAT 设置电压，不能开启充电，否则有反灌电流风险
        }
        else{
            PRINT(" sc8815_charge_enable faile!11 \n");
            return 4;
        }
        
        /* 电池电压满了 */
        if (dev->adc_detec_bat_info.vbat >= SOC_VOC_100_MV) 
        {  // 《应用注意事项》 P7：4.在开启充电前，应先检测电池实际电压，如果已高于设置电压值，不能开启充电。

            if ( dev->charge_disable != NULL )
            {
                dev->charge_disable(dev);
                // charge_disable(dev);
            }
            else{

                PRINT(" charge_disable is NULL faile! 22 \n");
                return 5;
            }
            PRINT(" voltage=[%dmV] too high! can not charge!", dev->adc_detec_bat_info.vbat);
            return 6;
        }

    }

    return 0;
}



int8_t sc8815_activate_battery(struct Chg_Ctrl_Dev_s *dev)
{
    uint8_t val_wr, val_rd = 0;

    if (dev == NULL)
    {
        return 1;
    }

    if ( (dev->chip_en ==NULL) || 
        (dev->chip_pstop_en == NULL) ||
        (dev->write_regs == NULL)   ||
        (dev->read_regs == NULL)   ||
        (dev->set_current == NULL) ||
        (dev->delay_ms == NULL)   ||
        (dev->charge_enable == NULL) ||
        (dev->init == NULL) )  
    {
        return 2;
    }

    // 关闭功率输出来配置寄存器
    if ( dev->charge_disable != NULL )
    {
        dev->charge_disable(dev);        
        // charge_disable(dev);
        PRINT(" charge disable ok! \n");
    }
    else
    {
        PRINT(" charge disable failed! \n");
    }
    // charge_disable(dev);

    // 配置成输出低电压激活
    dev->read_regs(dev->i2c_addr, dev->regs_addr_list.vbat_set, &val_rd, 1);
    // sc8815_reg_read(dev->iic, SS8815_REG_VBAT_SET, &val_rd);
    val_wr = val_rd;
    val_wr &= ~(0x01 << 5); // VBAT电压设置为内部设置
    val_wr &= ~(0x03 << 3);
    val_wr |=  (0x00 << 3);  // 1s
    val_wr &= ~(0x07 << 0);
    val_wr |=  (0x02 << 0);  // 4.25V

    dev->write_regs(dev->i2c_addr, dev->regs_addr_list.vbat_set, &val_wr, 1);
    // sc8815_reg_write(dev->iic, SS8815_REG_VBAT_SET, val_wr);

    // 回读校验
    dev->read_regs(dev->i2c_addr, dev->regs_addr_list.vbat_set, &val_rd, 1);
    // sc8815_reg_read(dev->iic, SS8815_REG_VBAT_SET, &val_rd);
    if (val_wr != val_rd) {
        PRINT("SS8815_%d REG_VBAT_SET write error, wr=[%#X], rd=[%#X] \n", 0, val_wr, val_rd);
    }

    // 激活
    dev->set_current(dev, 500, 301);
    // set_sc8815_current(dev, 500, 301);
    
    dev->delay_ms(500);
    // delay_1ms(500);

    dev->charge_enable(dev,false);
    // charge_enable(dev, false);

    dev->delay_ms(300);
    // delay_1ms(300);

    // 激活完成后恢复默认配置
    dev->init(dev);
    // dcdc_ctrl_dev_init(dev);

    PRINT("battery=[%d] activate success! \n", 0);

    return 0;
}

```


### 2.4.3 中间层实现（承上启下，对接应用层）

#### 2.4.3.1 头文件

```c
/**
 * @file charge_manarge.c
 * @author vik (ufo281@outlook.com)
 * @brief 中间层：负责SC8815充电管理类芯片的配置使用
 * @version 0.1
 * @date 2025-09-23
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#ifndef CHARGE_MANARGE_H 
#define CHARGE_MANARGE_H

#include "charge_ctrl_drv.h"





void charge_ctrl_dev_init(void);
int8_t charge_ctrl_adc_update(void);
int8_t set_charge_ctrl_current(uint8_t dev_id, uint16_t ibus, uint16_t ibat);
int8_t set_charge_ctrl_activate_battery(uint8_t dev_id);
int8_t set_charge_ctrl_status(uint8_t dev_id, bool en);
bool get_charge_ctrl_status(uint8_t dev_id);
const Chg_Ctrl_Dev_adc_info_s_t *get_charge_ctrl_adc(uint8_t dev_id);
const Chg_Ctrl_Dev_cur_cfg_s_t *get_charge_ctrl_cfg(uint8_t dev_id);
void printf_charge_ctrl_info(void);
void test_sc8815(void);





#endif

```

#### 2.4.3.2 源文件

```c
/**
 * @file charge_manarge.c
 * @author vik (ufo281@outlook.com)
 * @brief 充电控制设备 中间层：负责SC8815充电管理类芯片的配置使用
 * @version 0.1
 * @date 2025-09-23
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#include "charge_ctrl_mid.h"

#define ARRAY_SZ(x)                     ((sizeof(x)) / (sizeof((x)[0])))




/**
 * @brief 充电设备初始化
 * 
 */
void charge_ctrl_dev_init(void)
{
    for(int i=0; i<ARRAY_SZ(g_chg_ctrl_dev); i++) 
    {
        if ( g_chg_ctrl_dev[i].init != NULL )
        {
            if ( g_chg_ctrl_dev[i].init(&g_chg_ctrl_dev[i]) !=0 )
            {
                PRINT(" [int8_t charge_ctrl_adc_update]:dev-%d -init failed! \n",i);
            }
        }
        else
        {
            PRINT("g_chg_ctrl_dev[%d].init is NULL Failed!  \r\n",i);
        }
    }
}


int8_t charge_ctrl_adc_update(void)
{
    for(int i=0; i<ARRAY_SZ(g_chg_ctrl_dev); i++) 
    {
        if ( g_chg_ctrl_dev[i].adc_get_voltage_current )
        {
            if ( g_chg_ctrl_dev[i].adc_get_voltage_current(&g_chg_ctrl_dev[i]) !=0 )
            {
                PRINT(" [charge_ctrl_adc_update]:dev-%d -adc_get_voltage_current failed! \n",i);
            }
        }
        else{
            PRINT(" [charge_ctrl_adc_update]:dev-%d adc_get_voltage_current is NULL failed! \n",i);
        }
    }
    return 0;
}

int8_t set_charge_ctrl_current(uint8_t dev_id, uint16_t ibus, uint16_t ibat)
{
    if (dev_id >= ARRAY_SZ(g_chg_ctrl_dev)) 
    {
        PRINT("dev_id=[%d] out of range.", dev_id);
        return -1;
    }

    if ( g_chg_ctrl_dev[dev_id].set_current !=NULL )
    {

        if ( g_chg_ctrl_dev[dev_id].set_current(&g_chg_ctrl_dev[dev_id],ibus, ibat)!=0 )
        {
            PRINT(" [set_charge_ctrl_current]:dev-%d set_current failed! \n",dev_id);
        }

    }
    else{
        PRINT(" [set_charge_ctrl_current]:dev-%d set_charge_ctrl_current is NULL failed! \n",dev_id);
    }

    return 0;
}



/**
 * @brief 激活电池
 * 
 * @param dev_id 
 * @return int8_t 
 */
int8_t set_charge_ctrl_activate_battery(uint8_t dev_id)
{
    if (dev_id >= ARRAY_SZ(g_chg_ctrl_dev)) 
    {
        PRINT("dev_id=[%d] out of range.", dev_id);
        return -1;
    }

    if ( g_chg_ctrl_dev[dev_id].activate_battery !=NULL )
    {
        if ( g_chg_ctrl_dev[dev_id].activate_battery(&g_chg_ctrl_dev[dev_id])!=0 )
        {
            PRINT(" [activate_battery]:dev-%d activate_battery failed! \n",dev_id);
            return -2;
        }

    }
    else{
        PRINT(" [activate_battery]:dev-%d activate_battery is NULL failed! \n",dev_id);
        return -3;
    }

    return 0;
}


/**
 * @brief Get the charge ctrl status 
 * 
 * @param dev_id 
 * @return true 
 * @return false 
 */
bool get_charge_ctrl_status(uint8_t dev_id)
{
    if (dev_id >= ARRAY_SZ(g_chg_ctrl_dev)) 
    {
        PRINT("dev_id=[%d] out of range.", dev_id);
        return false;
    }


    if ( g_chg_ctrl_dev[dev_id].get_pstop_flag !=NULL )
    {
        return g_chg_ctrl_dev[dev_id].get_pstop_flag(&g_chg_ctrl_dev[dev_id]);
    }
    else{
        PRINT(" [get_charge_ctrl_status]:dev-%d get_pstop_flag is NULL failed! \n",dev_id);
        return false;
    }

}


int8_t set_charge_ctrl_status(uint8_t dev_id, bool en)
{
    if (dev_id >= ARRAY_SZ(g_chg_ctrl_dev)) 
    {
        PRINT("dev_id=[%d] out of range.", dev_id);
        return -1;
    }

    if ( g_chg_ctrl_dev[dev_id].charge_enable !=NULL )
    {
        if ( g_chg_ctrl_dev[dev_id].charge_enable(&g_chg_ctrl_dev[dev_id],en)!=0 )
        {
            PRINT(" [activate_battery]:dev-%d activate_battery failed! \n",dev_id);
            return -2;
        }

    }
    else{
        PRINT(" [activate_battery]:dev-%d activate_battery is NULL failed! \n",dev_id);
        return -3;
    }
    
    // charge_enable(&g_chg_ctrl_dev[dev_id].dev, en);
    return 0;
}


const Chg_Ctrl_Dev_adc_info_s_t *get_charge_ctrl_adc(uint8_t dev_id)
{
    if (dev_id >= ARRAY_SZ(g_chg_ctrl_dev)) 
    {
        PRINT("[get_charge_ctrl_adc]: dev_id=[%d] out of range.", dev_id);
        return NULL;
    }
    return &g_chg_ctrl_dev[dev_id].adc_detec_bat_info;
}

const Chg_Ctrl_Dev_cur_cfg_s_t *get_charge_ctrl_cfg(uint8_t dev_id)
{
    if (dev_id >= ARRAY_SZ(g_chg_ctrl_dev))
    {
        PRINT("[get_charge_ctrl_cfg]: dev_id=[%d] out of range.", dev_id);
        return NULL;
    }
    return &g_chg_ctrl_dev[dev_id].config;
}

void printf_charge_ctrl_info(void)
{
    PRINT("------------------------------------- charge_ctrl info -------------------------------------");
    for(int i=0; i<ARRAY_SZ(g_chg_ctrl_dev); i++) 
    {
        const Chg_Ctrl_Dev_adc_info_s_t *adc = get_charge_ctrl_adc(i);
        const Chg_Ctrl_Dev_cur_cfg_s_t  *cfg = get_charge_ctrl_cfg(i);
        PRINT("sc8815_%d: vbus = %d, ibus = %d, pbus = %0.1f, vbat = %d, ibat = %d, ibus_lim = %.2f, ibat_lim = %.2f",
                i,
                adc->vbus,
                adc->ibus,
                ((float)adc->vbus * adc->ibus / 1000000),
                adc->vbat,
                adc->ibat,
                cfg->ibus_limit,
                cfg->ibat_limit);
    }
}

void test_sc8815(void)
{

    if ( g_chg_ctrl_dev[SC8815_CHIP1].adc_get_voltage_current )
    {
        g_chg_ctrl_dev[SC8815_CHIP1].adc_get_voltage_current(&g_chg_ctrl_dev[SC8815_CHIP1]);
    }
    else{
        PRINT(" test sc8815! failed! \n");
    }

    if ( g_chg_ctrl_dev[SC8815_CHIP1].charge_enable )
    {
        g_chg_ctrl_dev[SC8815_CHIP1].charge_enable(&g_chg_ctrl_dev[SC8815_CHIP1],1);
    }
    else{
        PRINT(" test sc8815! failed! \n");
    }
    
    if ( g_chg_ctrl_dev[SC8815_CHIP1].activate_battery )
    {
        g_chg_ctrl_dev[SC8815_CHIP1].activate_battery( &g_chg_ctrl_dev[SC8815_CHIP1] );
    }
    else
    {
        PRINT(" test activate_battery! failed! \n");
    }

}


```
