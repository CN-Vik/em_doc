/**
 * @file ringbuffer.h
 * @author vik (ufo281@outlook.com)
 * @brief 
 * @version 0.1
 * @date 2026-06-15
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#ifndef RINGBUFFER_H
#define RINGBUFFER_H

#include <stdint.h>
#include <stddef.h>

#define RB_SIZE 1024   // 必须2的幂更快（这里先普通实现）

typedef struct {
    uint8_t buf[RB_SIZE];
    volatile uint32_t head;
    volatile uint32_t tail;
} ringbuffer_t;

void ring_buf_init(ringbuffer_t *ring_buf);
uint32_t ring_buf_len(ringbuffer_t *ring_buf);
uint32_t ring_buf_free(ringbuffer_t *ring_buf);

int ring_buf_push(ringbuffer_t *ring_buf, uint8_t data);
int ring_buf_pop(ringbuffer_t *ring_buf, uint8_t *data);
int ring_buf_pop_n(ringbuffer_t *ring_buf, uint8_t *data, uint32_t len);

#endif

