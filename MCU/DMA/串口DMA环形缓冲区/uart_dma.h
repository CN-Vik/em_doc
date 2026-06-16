#ifndef UART_DMA_H
#define UART_DMA_H

#include "mh210x.h"
#include <stdint.h>

#define DMA_CH2 DMA1_Channel2
#define UART3_TX_DMA_TX_CH DMA_CH2

#define DEBUG_UART USART3
#define UART_DMA_TX_BUF_SIZE 256

void uart3_cfg(uint32_t bound);

void uart_dma_init(void);
void uart_dma_start(void);
void uart_dma_send(uint8_t *data, uint32_t len);

#endif





