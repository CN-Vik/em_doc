/**
 * @file drv_timer_io_dma.c
 * @author wls (ufo281@outlook.com) 
 * @brief dshot600 bit收发定时周期 or 频率，分别是 1.67us,1.8MHZ
 * @version 1.0
 * @date 2025-10-13
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#include "gd32f4xx.h"
#include "drv_timer_io_dma.h"
#include "dshot_bitbang.h"
#include "debug.h"


#define ARRAYNUM(arr_name)      (uint32_t)(sizeof(arr_name) / sizeof(*(arr_name)))

#define ARRAYNUMD(arr_name)      (uint32_t)(sizeof(arr_name) / sizeof(arr_name[0][0]))



#define SET_BIT(var, pos)      ((var) |=  (1U << (pos)))
#define CLEAR_BIT(var, pos)    ((var) &= ~(1U << (pos)))
#define TOGGLE_BIT(var, pos)   ((var) ^=  (1U << (pos)))
#define READ_BIT(var, pos)     (((var) >> (pos)) & 0x1U)



/************************** DSHOT 帧结构体 **************************/
// 用联合体实现位操作：既可以整体操作16位帧，也能单独访问每一位
typedef union __attribute__((packed))
{
    uint16_t raw;  // 完整16位帧数据
    
    /*MSB*/
    struct {
        uint16_t crc               : 4; // Bit0-3：crc bit
        uint16_t telemetry_request : 1; // Bit4：遥测请求（1=请求）
        uint16_t throttle          : 11;// Bit5-Bit15：油门值（0~1023）
    } bits; 

 // 帧位域分解
} dshot600_frame_t;



// 单Bit协议包结构体（3个uint16_t）
typedef struct 
{
    uint16_t data[3];

} tx_packet_t;


/**
 * @brief USE IO PC 6 7 8 9 
 * 
 * @param bit_value 
 * @return tx_packet_t 
 */
static inline uint16_t write_IO_value(bool bit_value) 
{
    uint16_t port_data = gpio_output_port_get(ESC_PWM_GPIO_PORT);/*读取当前输出IO寄存器的值*/

    if(bit_value)
    {
        SET_BIT(port_data,6);
        SET_BIT(port_data,7);
        SET_BIT(port_data,8);
        SET_BIT(port_data,9);
    }
    else
    {
        CLEAR_BIT(port_data,6);
        CLEAR_BIT(port_data,7);
        CLEAR_BIT(port_data,8);
        CLEAR_BIT(port_data,9);
    }

    return port_data;
}



/**
 * @brief dhsot 协议规定，发送的二进制数据周期为1667ns，此处把一个dshot一个二进制数据周期分为3段电平
 *          1. 连续的高高低电平代表着dshot二进制数据1（也就是IO连续着输出两个高电平，一个低电平，高电平占比67%）
 *          2. 连续的高低低电平代表着dshot二进制数据0（也就是IO连续着输出一个高电平，两个低电平，高电平占比33%）
 * 
 *              
 * 
 * @param bit_value 
 * @return tx_packet_t 
 */
static inline tx_packet_t write_dashot_bit_value(bool bit_value) 
{
    tx_packet_t pack; 
    
    if (bit_value) 
    {
        pack.data[0] = write_IO_value(1);
        pack.data[1] = write_IO_value(1);
        pack.data[2] = write_IO_value(0);
    }
    else
    {
        pack.data[0] = write_IO_value(1);
        pack.data[1] = write_IO_value(0);
        pack.data[2] = write_IO_value(0);
    }

    return pack;
}



/**
 * @brief Get the dshot buf data object
 * 
 * @param data 一帧的dshot数据
 * @param buf_data 
 */
void get_dshot_buf_data(uint16_t data , tx_packet_t *buf_data )
{
    // tx_packet_t buffer_data[16];

    for (uint16_t i = 0; i < 16; i++)
    {
        buf_data[i] = write_dashot_bit_value( data & (1<<i) );
    }

}


typedef struct
{
    tx_packet_t start;
    
    tx_packet_t frameio_data[16];

    tx_packet_t end;

}dshot_io_buf_frame_t;


dshot_io_buf_frame_t ds_buf_io={0};


void dshot_main(void)
{
    dshot600_frame_t dshot_data1 =
    {
        .bits={
            .crc =0xF,
            .telemetry_request =0x0,
            .throttle =0x600
        }
    };

    tx_packet_t buffer_data[16]={0};

    get_dshot_buf_data( (uint16_t)dshot_data1.raw, buffer_data );

    ds_buf_io.start = write_dashot_bit_value(1);
    memcpy( ds_buf_io.frameio_data, buffer_data, sizeof(buffer_data));
    ds_buf_io.end = write_dashot_bit_value(1);
    
}



uint16_t dshoot_dma_receice_buffer[RECEIVE_BUFFER_NUMBER] = {0};  // 32
uint16_t send_buffer_dshoot[SEND_BUFFER_NUMBER] = {0};



uint16_t tx_buf[][3]={

    { 0X3C0, 0X3C0, 0x0}, /* data_bit:1 */
    
    { 0X3C0, 0x0,  0x0}, /* data_bit:0 */
    { 0X3C0, 0x0,  0x0}, /* data_bit:0 */
    { 0X3C0, 0X3C0, 0x0}, /* data_bit:1 */
    {0X3C0, 0X3C0, 0x0} , /* data_bit:1 */

    {0X3C0, 0X3C0, 0x0} , /* data_bit:1 */
    {0X3C0, 0X3C0, 0x0} , /* data_bit:1 */
    { 0X3C0, 0x0,  0x0}, /* data_bit:0 */
    { 0X3C0, 0x0,  0x0}, /* data_bit:0 */
    {0X3C0, 0X3C0, 0x0} , /* data_bit:1 */


};


/**
    \brief      configure the GPIO ports
    \param[in]  none
    \param[out] none
    \retval     none
  */
void dshot_gpio_configuration_output(void)
{
    /* enable the led clock */
    rcu_periph_clock_enable(ESC_PWM_GPIO_CLK);

    /* configure led GPIO port */ 
    gpio_mode_set(  ESC_PWM_GPIO_PORT, 
                    GPIO_MODE_OUTPUT, 
                    GPIO_PUPD_PULLUP,
                    ESC_PWM_A_PIN | ESC_PWM_B_PIN | ESC_PWM_C_PIN | ESC_PWM_D_PIN
    
    );

    gpio_output_options_set(ESC_PWM_GPIO_PORT,
                            GPIO_OTYPE_PP, 
                            GPIO_OSPEED_50MHZ,
                            ESC_PWM_A_PIN | ESC_PWM_B_PIN | ESC_PWM_C_PIN | ESC_PWM_D_PIN
    );

}

void dshot_gpio_configuration_input(void)
{    
    /* enable the led clock */
    rcu_periph_clock_enable(ESC_PWM_GPIO_CLK);

    gpio_mode_set(  ESC_PWM_GPIO_PORT, 
                    GPIO_MODE_INPUT, 
                    GPIO_PUPD_PULLUP, 
                    ESC_PWM_A_PIN | ESC_PWM_B_PIN | ESC_PWM_C_PIN | ESC_PWM_D_PIN
    
    );


}

/**
    \brief      configure the TIMER 5 peripheral
    \param[in]  none
    \param[out] none
    \retval     none
  */
void timer_configuration_send_dshot(void)
{
    timer_parameter_struct timer_initpara;               /* timer_initpara用于配置定时器的参数 */

    /* 使能RCU时钟 */ 
    rcu_periph_clock_enable(RCU_TIMER5);                 /* 使能TIMER5的时钟 */
    rcu_timer_clock_prescaler_config(RCU_TIMER_PSC_MUL2); /*TIMER_CLK = 240MHZ */

    /* 复位TIMER5 */
    timer_deinit(TIMER5);                                /* 复位TIMER5 */
    timer_struct_para_init(&timer_initpara);             /* 初始化timer_initpara为默认值 */

    /* 配置TIMER5, timer_T(中断周期：566 ns ) */
    timer_initpara.prescaler         = 0;              /* 设置预分频值 ,max=65536*/
    timer_initpara.counterdirection  = TIMER_COUNTER_UP; /* 设置向上计数模式 */
    timer_initpara.period            = 66; /* 设置自动重装载值 ,max = 65536*/
    timer_initpara.clockdivision     = TIMER_CKDIV_DIV1; /* 设置时钟分频因子 */
    timer_init(TIMER5, &timer_initpara);                 /* 根据参数初始化定时器 */

    /* 使能定时器及中断功能*/
    timer_interrupt_enable(TIMER5, TIMER_INT_UP);        /* 使能定时器的更新中断 */
    // nvic_irq_enable(TIMER5_IRQn, 0, 2);                  /* 配置NVIC优先级，抢占优先级0，响应优先级3 */

    /* auto-reload preload enable */
    timer_auto_reload_shadow_enable(TIMER5);

    timer_enable(TIMER5);                                /* 使能定时器TIMER5 */
}

void dma_configuration_send_dshot(void)
{
    dshot_main();
    dma_single_data_parameter_struct dma_init_struct;

    /* enable DMA clock */
    rcu_periph_clock_enable(RCU_DMA0);

    /* initialize DMA channel5 */
    dma_deinit(TIM5_TX_DMAx,TIM5_TX_DMAx_CH);

    /* DMA channel5 initialize */
    // dma_init_struct.periph_addr = (uint32_t)&GPIO_TG(ESC_PWM_GPIO_PORT); /*  */
    dma_init_struct.periph_addr = (uint32_t)&GPIO_OCTL(ESC_PWM_GPIO_PORT); /* 临时测试 */
    dma_init_struct.periph_inc = DMA_PERIPH_INCREASE_DISABLE;
    // dma_init_struct.memory0_addr = (uint32_t)send_buffer_dshoot;
    dma_init_struct.memory0_addr = (uint32_t)&ds_buf_io; /*临时测试*/
    dma_init_struct.memory_inc = DMA_MEMORY_INCREASE_ENABLE;
    dma_init_struct.periph_memory_width = DMA_PERIPH_WIDTH_16BIT;
    dma_init_struct.circular_mode = DMA_CIRCULAR_MODE_ENABLE;
    dma_init_struct.direction = DMA_MEMORY_TO_PERIPH;
    dma_init_struct.number = sizeof(ds_buf_io) / sizeof(uint16_t); /* 临时测试 */
    // dma_init_struct.number = ARRAYNUM(send_buffer_dshoot); /* 正式使用的 */
    dma_init_struct.priority = DMA_PRIORITY_ULTRA_HIGH;
    dma_single_data_mode_init(TIM5_TX_DMAx,TIM5_TX_DMAx_CH,&dma_init_struct);

    /* configure DMA mode */
    dma_circulation_disable(TIM5_TX_DMAx, TIM5_TX_DMAx_CH);
    
    dma_channel_subperipheral_select(TIM5_TX_DMAx,TIM5_TX_DMAx_CH,DMA_SUBPERI7);
    
    /* enable TIM5_TX_DMAx transfer complete interrupt */
    dma_interrupt_enable(TIM5_TX_DMAx, TIM5_TX_DMAx_CH, DMA_CHXCTL_FTFIE);

    // nvic_irq_enable(DMA0_Channel1_IRQn, 0, 0);
    
    /* enable DMA channel5 */
    dma_channel_enable(TIM5_TX_DMAx,TIM5_TX_DMAx_CH);
    
    /* TIMER5 update DMA request enable */
    timer_dma_enable(TIMER5,TIMER_DMA_UPD);

}


/* -------------------------- 接收定时器配置 -------------------------- */
/**
 * @brief  配置接收定时器（控制DShot信号的采样时序）
 * @param  none
 * @retval none
 * @note   配置TMRA单元作为接收定时器，产生固定频率的采样时钟，定时读取GPIO状态。
         核心配置类似发送定时器，但**周期值不同**（适配接收采样率）。
 */
void timer_configuration_receive_dshoot(void)
{

    timer_parameter_struct timer_initpara;               /* timer_initpara用于配置定时器的参数 */

    /* 使能RCU时钟 */ 
    rcu_periph_clock_enable(RCU_TIMER0);                 /* 使能TIMER0的时钟 */
    rcu_timer_clock_prescaler_config(RCU_TIMER_PSC_MUL2); /*TIMER_CLK = 240MHZ */

    /* 复位TIMER0 */
    timer_deinit(TIMER0);                                /* 复位TIMER0 */
    timer_struct_para_init(&timer_initpara);             /* 初始化timer_initpara为默认值 */

    /* 配置TIMER0，定时周期 400ns */
    timer_initpara.prescaler         = 0;              /* 设置预分频值 ,max=65536*/
    timer_initpara.counterdirection  = TIMER_COUNTER_UP; /* 设置向上计数模式 */
    timer_initpara.period            = 48; /* 设置自动重装载值 ,max = 65536*/
    timer_initpara.clockdivision     = TIMER_CKDIV_DIV1; /* 设置时钟分频因子 */
    timer_init(TIMER0, &timer_initpara);                 /* 根据参数初始化定时器 */

    /* 使能定时器及中断功能*/
    timer_interrupt_enable(TIMER0, TIMER_INT_UP);        /* 使能定时器的更新中断 */
    // nvic_irq_enable(TIMER5_IRQn, 0, 2);                  /* 配置NVIC优先级，抢占优先级0，响应优先级3 */

    /* auto-reload preload enable */
    timer_auto_reload_shadow_enable(TIMER0);

    timer_enable(TIMER0);                                /* 使能定时器TIMER0 */

}


/* -------------------------- 接收DMA配置 -------------------------- */
/**
 * @brief  配置接收DMA（自动搬运GPIO数据到接收缓冲区）
 * @param  none
 * @retval none
 * @note   配置DMA通道将GPIO**输入数据寄存器（PIDRA）**的数据搬运到接收缓冲区：
         - 每次定时器触发DMA，读取GPIO当前状态并存入缓冲区
         - 核心配置：
           中断使能（采样完成触发回调）、传输次数（缓冲区大小）、数据宽度（16位）
           目标地址：接收缓冲区（递增存放）、源地址：GPIO输入寄存器（固定读取）
           地址策略：源地址固定（始终读PIDRA）、目标地址递增（存缓冲区下一字节）
 */
void dma_configuration_receive_dshoot(void)
{
    dma_single_data_parameter_struct dma_init_struct;

    /* enable DMA clock */
    rcu_periph_clock_enable(RCU_DMA0);

    /* initialize DMA channel5 */
    dma_deinit(TIM0_RX_DMAx,TIM0_RX_DMAx_CH);

    /* DMA channel5 initialize */
    dma_init_struct.periph_addr = (uint32_t)&GPIO_ISTAT(ESC_PWM_GPIO_PORT);
    dma_init_struct.periph_inc = DMA_PERIPH_INCREASE_DISABLE;
    dma_init_struct.memory0_addr = (uint32_t)dshoot_dma_receice_buffer;
    dma_init_struct.memory_inc = DMA_MEMORY_INCREASE_ENABLE;
    dma_init_struct.periph_memory_width = DMA_PERIPH_WIDTH_16BIT;
    dma_init_struct.circular_mode = DMA_CIRCULAR_MODE_ENABLE;
    dma_init_struct.direction = DMA_PERIPH_TO_MEMORY;
    dma_init_struct.number = ARRAYNUM(dshoot_dma_receice_buffer);
    dma_init_struct.priority = DMA_PRIORITY_ULTRA_HIGH;
    dma_single_data_mode_init(TIM0_RX_DMAx,TIM0_RX_DMAx_CH,&dma_init_struct);

    /* configure DMA mode */
    dma_circulation_disable(TIM0_RX_DMAx, TIM0_RX_DMAx_CH);
    
    dma_channel_subperipheral_select(TIM0_RX_DMAx,TIM0_RX_DMAx_CH,DMA_SUBPERI6);
    
    /* enable TIM5_TX_DMAx transfer complete interrupt */
    dma_interrupt_enable(TIM0_RX_DMAx, TIM0_RX_DMAx_CH, DMA_CHXCTL_FTFIE);

    // nvic_irq_enable(DMA0_Channel1_IRQn, 0, 0);
    
    /* enable DMA channel5 */
    dma_channel_enable(TIM0_RX_DMAx,TIM0_RX_DMAx_CH);
    
    /* TIMER5 update DMA request enable */
    timer_dma_enable(TIMER0,TIMER_DMA_UPD);

}

void dshot_config_init(void)
{
    /* enable DMA clock */
    rcu_periph_clock_enable(RCU_DMA0);
    rcu_periph_clock_enable(RCU_DMA1);

}


/* -------------------------- DShot驱动反初始化 -------------------------- */
/**
 * @brief  DShot驱动反初始化（资源释放）
 * @param  none
 * @retval none
 * @note   停止定时器、禁用中断、去初始化外设、关闭时钟，释放所有DShot资源。
 */
void dshot_config_deinit(void)
{
    /* disable TIM5_TX_DMAx channel7 */
    dma_channel_disable(TIM5_TX_DMAx, TIM5_TX_DMAx_CH);
    timer_dma_disable(TIMER5,TIMER_DMA_UPD);

    /* clear DMA HTF and FTF flag */
    dma_flag_clear(TIM5_TX_DMAx, TIM5_TX_DMAx_CH,DMA_FLAG_FTF);
    dma_flag_clear(TIM5_TX_DMAx, TIM5_TX_DMAx_CH,DMA_FLAG_HTF);  // 停止定时器

    dma_deinit(TIM0_RX_DMAx,TIM0_RX_DMAx_CH);
    dma_deinit(TIM5_TX_DMAx,TIM5_TX_DMAx_CH);
    timer_deinit(TIMER0);   /* 复位TIMER0 */
    timer_deinit(TIMER5);   /* 复位TIMER5 */

    /* disable TIM5_TX_DMAx channel7 */
    dma_channel_disable(TIM0_RX_DMAx, TIM0_RX_DMAx_CH);
    timer_dma_disable(TIMER0,TIMER_DMA_UPD);

    /* clear DMA HTF and FTF flag */
    dma_flag_clear(TIM0_RX_DMAx, TIM0_RX_DMAx_CH,DMA_FLAG_FTF);
    dma_flag_clear(TIM0_RX_DMAx, TIM0_RX_DMAx_CH,DMA_FLAG_HTF);

    
    nvic_irq_disable(DMA0_Channel1_IRQn);
    nvic_irq_disable(DMA1_Channel5_IRQn);

    rcu_periph_clock_disable(RCU_DMA0);
    rcu_periph_clock_disable(RCU_DMA1);
    rcu_periph_clock_disable(RCU_TIMER5);
    rcu_periph_clock_disable(RCU_TIMER0);

}





/* -------------------------- 发送DMA完成中断回调 -------------------------- */
/**
 * @brief  发送DMA传输完成中断回调函数
 * @param  none
 * @retval none
 * @note   当发送DMA完成设定次数（帧长度）后触发：
 *         1. 停止发送定时器（不再产生PWM时钟）
 *         2. 禁用发送DMA通道（停止数据搬运）
 *         3. 清除DMA传输完成标志（避免重复触发）
 *         4. 切换GPIO为输入模式（准备接收反馈数据）
 *         5. 配置接收定时器与DMA（启动接收流程）
 */
void DMA_TXEnd_IrqCallback(void)
{
    // debug(" TIM5_DMA_TX %x \n",(uint32_t)GPIO_OCTL(ESC_PWM_GPIO_PORT) );

    /* disable TIM5_TX_DMAx channel7 */
    dma_channel_disable(TIM5_TX_DMAx, TIM5_TX_DMAx_CH);

    timer_dma_disable(TIMER5,TIMER_DMA_UPD);

    /* clear DMA HTF and FTF flag */
    dma_flag_clear(TIM5_TX_DMAx, TIM5_TX_DMAx_CH,DMA_FLAG_FTF);
    dma_flag_clear(TIM5_TX_DMAx, TIM5_TX_DMAx_CH,DMA_FLAG_HTF);
    
#if 0 /* aim for test */
    /* reconfigure transfer number for the next DMA transfer */
    dma_transfer_number_config(TIM5_TX_DMAx, TIM5_TX_DMAx_CH, ARRAYNUM(send_buffer_dshoot));

    /* enable TIM5_TX_DMAx channel7 */
    dma_channel_enable(TIM5_TX_DMAx, TIM5_TX_DMAx_CH);

    /* TIMER5 update DMA request enable */
    timer_dma_enable(TIMER5,TIMER_DMA_UPD);
#endif

    dshot_gpio_configuration_input();                    // 切换GPIO为输入模式
    timer_configuration_receive_dshoot();                // 配置接收定时器
    dma_configuration_receive_dshoot();                  // 配置接收DMA
}


/* -------------------------- 接收DMA完成中断回调 -------------------------- */
/**
 * @brief  接收DMA传输完成中断回调函数
 * @param  none
 * @retval none
 * @note   当接收DMA完成设定次数（缓冲区大小）后触发：
 *         1. 停止接收定时器（不再采样GPIO状态）
 *         2. 禁用接收DMA通道（停止数据搬运）
 *         3. 清除DMA传输完成标志（避免重复触发）
 *         4. 调用DShot解码函数处理接收缓冲区数据
 */
void DMA_RX_End_IrqCallback(void)
{   

    debug(" TIM0_DMA_RX %x ,buf[0]:%x\n",(uint32_t)GPIO_ISTAT(ESC_PWM_GPIO_PORT) ,dshoot_dma_receice_buffer[0]);

    /* disable TIM5_TX_DMAx channel7 */
    dma_channel_disable(TIM0_RX_DMAx, TIM0_RX_DMAx_CH);

    timer_dma_disable(TIMER0,TIMER_DMA_UPD);

    /* clear DMA HTF and FTF flag */
    dma_flag_clear(TIM0_RX_DMAx, TIM0_RX_DMAx_CH,DMA_FLAG_FTF);
    dma_flag_clear(TIM0_RX_DMAx, TIM0_RX_DMAx_CH,DMA_FLAG_HTF);
    
#if 0 /* aim for test */
    /* reconfigure transfer number for the next DMA transfer */
    dma_transfer_number_config(TIM0_RX_DMAx, TIM0_RX_DMAx_CH, ARRAYNUM(dshoot_dma_receice_buffer));

    /* enable TIM0_RX_DMAx channel7 */
    dma_channel_enable(TIM0_RX_DMAx, TIM0_RX_DMAx_CH);

    /* TIMER0 update DMA request enable */
    timer_dma_enable(TIMER0,TIMER_DMA_UPD);
#endif

    // dshot_IRQHandler(dshoot_dma_receice_buffer); // 处理接收到的DShot原始数据

}




/*!
    \brief      this function handles DMA1_Channel7_IRQHandler interrupt
    \param[in]  none
    \param[out] none
    \retval     none
*/
void DMA0_Channel1_IRQHandler(void)
{
    if(dma_interrupt_flag_get(TIM5_TX_DMAx, TIM5_TX_DMAx_CH, DMA_INT_FLAG_FTF)) 
    {
        dma_interrupt_flag_clear(TIM5_TX_DMAx, TIM5_TX_DMAx_CH, DMA_INT_FLAG_FTF);

        DMA_TXEnd_IrqCallback();

    }
}



/*!
    \brief      this function handles DMA1_Channel5_IRQHandler interrupt
    \param[in]  none
    \param[out] none
    \retval     none
*/
void DMA1_Channel5_IRQHandler(void)
{
    if(dma_interrupt_flag_get(TIM0_RX_DMAx, TIM0_RX_DMAx_CH, DMA_INT_FLAG_FTF)) 
    {
        dma_interrupt_flag_clear(TIM0_RX_DMAx, TIM0_RX_DMAx_CH, DMA_INT_FLAG_FTF);

        DMA_RX_End_IrqCallback();

    }
}
