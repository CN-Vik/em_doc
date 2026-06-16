/**
 * @file main.c
 * @author vik (ufo281@outlook.com)
 * @brief 
 * @version 0.1
 * @date 2026-06-15
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include "delay.h"
#include "mh210x.h"
#include "uart_log.h"
#include "uart_dma.h"

/**
 * @brief                
 *               printf()

                    │

                    ▼

            vsnprintf()

                    │

                    ▼

             RingBuffer

                    │
          DMA空闲？───No──────┐
              │               │
             Yes              │
              │               │
              ▼               │
        启动DMA发送            │
              │               │
              ▼               │
         DMA后台发送           │
              │               │
              ▼               │
       DMA完成中断IRQ─────────┘
              │
              ▼
      RingBuffer还有数据？
         │
      Yes│No
         ▼
     再启动DMA
 * 
 * @return int 
 */
int main(void)
{
    
    RCC_ClocksTypeDef clocks;

    RCC_GetClocksFreq(&clocks);
    Delay_Init();
    uart3_cfg(115200);/*初始化uart外设*/
    uart_log_init();/*初始化UART DMA和ring buf环形缓冲区*/

    LOGI("SYSCLK: %3.1fMhz, HCLK: %3.1fMhz, PCLK1: %3.1fMhz, PCLK2: %3.1fMhz, ADCCLK: %3.1fMhz\r\n",
               (float)clocks.SYSCLK_Frequency / 1000000, (float)clocks.HCLK_Frequency / 1000000,
               (float)clocks.PCLK1_Frequency / 1000000, (float)clocks.PCLK2_Frequency / 1000000, (float)clocks.ADCCLK_Frequency / 1000000);

    LOGI("USART Asyn DMA Test.\n");


    int cnt = 0;

    while (1)
    {

        LOGI("USART3_DMA_printf_test66, cnt:%d\r\n", cnt++);

        Delay_Ms(100);
    }
}
