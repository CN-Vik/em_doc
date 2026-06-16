/**
 * @file ringbuffer.c
 * @author vik (ufo281@outlook.com)
 * @brief 
 * @version 0.1
 * @date 2026-06-15
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#include "ringbuffer.h"

/**
 * @brief 环形缓冲区初始化
 * 
 * @param ring_buf 
 */
void ring_buf_init(ringbuffer_t *ring_buf)
{
    ring_buf->head = 0;
    ring_buf->tail = 0;
}



/**
 * @brief 获取环形缓冲区当前数据长度
 * 
 * @param ring_buf 
 * @return uint32_t 
 */
uint32_t ring_buf_len(ringbuffer_t *ring_buf)
{
    return (ring_buf->head - ring_buf->tail);
}



/**
 * @brief 获取环形缓冲区的剩余长度（配置好的环形缓冲区长度-当前环形缓冲区长度）
 * 
 * @param ring_buf 
 * @return uint32_t 
 */
uint32_t ring_buf_free(ringbuffer_t *ring_buf)
{
    return (RB_SIZE - ring_buf_len(ring_buf));
}



/**
 * @brief 将数据存入环形缓冲区
 * 
 * @param ring_buf 环形缓冲区的地址
 * @param data 数据值
 * @return int 
 */
int ring_buf_push(ringbuffer_t *ring_buf, uint8_t data)
{
    if (ring_buf_free(ring_buf) == 0) return -1;

    ring_buf->buf[ring_buf->head % RB_SIZE] = data;
    ring_buf->head++;
    return 0;
}



/**
 * @brief 将1Byte数据从环形缓冲区取出来
 * 
 * @param ring_buf 环形缓冲区的地址
 * @param data 数据地址
 * @return int 0:ok, -1:failed！
 */
int ring_buf_pop(ringbuffer_t *ring_buf, uint8_t *data)
{
    if (ring_buf_len(ring_buf) == 0) return -1;

    *data = ring_buf->buf[ring_buf->tail % RB_SIZE];
    ring_buf->tail++;
    return 0;
}



/**
 * @brief 将n个Byte数据从环形缓冲区取出来
 * 
 * @param ring_buf 环形缓冲区地址
 * @param data 数据地址
 * @param len 数据长度(单位:Byte)
 * @return int 
 */
int ring_buf_pop_n(ringbuffer_t *ring_buf, uint8_t *data, uint32_t len)
{
    if (ring_buf_len(ring_buf) < len) return -1;

    for (uint32_t i = 0; i < len; i++)
    {
        data[i] = ring_buf->buf[ring_buf->tail % RB_SIZE];
        ring_buf->tail++;
    }
    return 0;
}

