/**
 * @file uart_dma.c
 * @author vik (ufo281@outlook.com)
 * @brief 
 * @version 0.1
 * @date 2026-06-15
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#include "uart_dma.h"
#include "ringbuffer.h"

static volatile uint8_t dma_tx_busy = 0;
static uint8_t dma_buf[UART_DMA_TX_BUF_SIZE];

extern ringbuffer_t uart_ring_buf;


void uart3_cfg(uint32_t bound)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;

    // RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3, ENABLE);
    // RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10; /*uart3_tx*/
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11; /*uart3_rx*/
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    USART_InitStructure.USART_BaudRate = bound;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;

    USART_Init(DEBUG_UART, &USART_InitStructure);
    USART_Cmd(DEBUG_UART, ENABLE);
}


void uart_dma_init(void)
{
    NVIC_InitTypeDef NVIC_InitStructure;
    DMA_InitTypeDef DMA_InitStructure;

    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);

    // USART1_TX DMA Config
    DMA_DeInit(UART3_TX_DMA_TX_CH);
    DMA_Cmd(UART3_TX_DMA_TX_CH, DISABLE);/*DMA配置期间必须关闭DMA通道*/


    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&(DEBUG_UART->DR);
    DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t)dma_buf;                  /*DMA 传输的源地址 = 内存数组 Buff*/
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralDST;                      /*外设作为目标端 ,内存 → 外设（内存数据发给串口）*/
    DMA_InitStructure.DMA_BufferSize = 0;                            /*设置本次 DMA 一共要传输BUFFSIZE 个字节，发完这个数量后停止（普通模式下）*/
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;        /*串口永远只往 USART_DR 这一个寄存器写数据，地址固定，不需要变*/
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;                 /*数据存在数组Buff[0]、Buff[1]、Buff[2]...，取完一个字节要往后移一位。*/
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte; /*外设数据位宽，外设端按 8 位 (1 字节) 传输，串口收发都是单字节，固定配 Byte。*/
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;         /*内存数据位宽，内存也按 8 位 (1 字节) 读取，和数组uint8_t类型匹配。*/
    /*Normal：普通模式（单次传输）
    传输完BUFFSIZE个数据后，DMA 自动停止，不再重复传输。
    对比：DMA_Mode_Circular 循环模式，发完从头再来（常用在不间断发送）。*/
    DMA_InitStructure.DMA_Mode = DMA_Mode_Normal;
    DMA_InitStructure.DMA_Priority = DMA_Priority_VeryHigh;
    DMA_InitStructure.DMA_M2M = DMA_M2M_Disable; /*关闭内存到内存模式，因为这里是内存 -> USART 外设*/
    DMA_Init(UART3_TX_DMA_TX_CH, &DMA_InitStructure);

    // 【关键修改】初始化时不要开启DMA通道，发送的时候再开启
    // DMA_Cmd(UART3_TX_DMA_TX_CH, ENABLE);
    USART_DMACmd(DEBUG_UART, USART_DMAReq_Tx, ENABLE);

    NVIC_InitStructure.NVIC_IRQChannel = DMA1_Channel2_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 3;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
    DMA_ITConfig(UART3_TX_DMA_TX_CH, DMA_IT_TC, ENABLE);
    
    NVIC_EnableIRQ(DMA1_Channel2_IRQn);
}

void uart_dma_send(uint8_t *data, uint16_t len)
{
    if (len == 0) return;

    // 1. 先关闭DMA通道，才能修改寄存器
    DMA_Cmd(UART3_TX_DMA_TX_CH, DISABLE);

    // 等待DMA空闲（双重保险）
    while (dma_tx_busy);

    // 2. 拷贝数据（用memcpy比循环更快）
    memcpy(dma_buf, data, len);

    // 3. 设置传输长度（通道关闭时写入才有效）
    UART3_TX_DMA_TX_CH->CNDTR = len;

    // 4. 标记忙，再开启DMA启动传输
    dma_tx_busy = 1;
    DMA_Cmd(UART3_TX_DMA_TX_CH, ENABLE);
}

void uart_dma_start(void)
{
    // DMA忙的时候直接返回，不重复启动
    if (dma_tx_busy) return;

    uint16_t len = ring_buf_len(&uart_ring_buf);
    if (len == 0 || dma_tx_busy) return;

    if (len > sizeof(dma_buf)) len = sizeof(dma_buf);

    ring_buf_pop_n(&uart_ring_buf, dma_buf, len);

    uart_dma_send(dma_buf, len);
}

void DMA1_Channel2_IRQHandler(void)
{
    if (DMA_GetITStatus(DMA1_IT_TC2))
    {
        // 先清中断标志，防止重复进入
        DMA_ClearITPendingBit(DMA1_IT_TC2);

        // 关闭DMA通道
        DMA_ClearFlag(DMA1_IT_TC2);

        // 清除忙标志
        dma_tx_busy = 0;

        uart_dma_start();
    }
}
