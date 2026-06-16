/**
 * @file uart_log.c
 * @author vik (ufo281@outlook.com)
 * @brief 
 * @version 0.1
 * @date 2026-06-15
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#include "uart_log.h"
#include "ringbuffer.h"
#include "uart_dma.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>


ringbuffer_t uart_ring_buf;

static char log_buf[256];

void uart_log_init(void)
{
    ring_buf_init(&uart_ring_buf);

    uart_dma_init();

    uart_dma_start();
}

void log_putc(char c)
{
    ring_buf_push(&uart_ring_buf, (uint8_t)c);
}

void log_puts(const char *s)
{
    while (*s)
    {
        ring_buf_push(&uart_ring_buf, (uint8_t)(*s++));
    }

    uart_dma_start();
}

void LOGI(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);

    vsnprintf(log_buf, sizeof(log_buf), fmt, ap);

    va_end(ap);

    log_puts(log_buf);
}

