/**
 * @file uart_log.h
 * @author vik (ufo281@outlook.com)
 * @brief 
 *  use:  LOGI("hello world %d\r\n", 123);
 
 use example
 * 
 * 

#include "stm32f10x.h"
#include "uart_log.h"

volatile uint32_t cnt = 0;

void RCC_Config(void);
void GPIO_Config(void);
void USART3_Config(void);

int main(void)
{
    RCC_Config();
    GPIO_Config();
    USART3_Config();

    uart_log_init();

    while (1)
    {
        LOGI("USART3 DMA printf test, cnt:%d\r\n", cnt++);

        for (volatile int i = 0; i < 500000; i++);
    }
}
 * 
 * 
 * 
 * @version 0.1
 * @date 2026-06-15
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#ifndef UART_LOG_H
#define UART_LOG_H

#include <stdint.h>

void uart_log_init(void);

void log_putc(char c);
void log_puts(const char *s);

void LOGI(const char *fmt, ...);

#endif
