/**
 * @file weight_filter.h
 * @author vik (ufo281@outlook.com)
 * @brief 中值滤波和一阶低通滤波
 * @version 0.1
 * @date 2026-07-30
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#ifndef WEIGHT_FILTER_H
#define WEIGHT_FILTER_H

#include <stdint.h>


#define MEDIAN_LEN      30

typedef struct
{
    int32_t median_buf[MEDIAN_LEN];
    uint8_t index;

    float lpf_value;     //低通输出值
    float alpha;         //低通系数

    uint8_t init_flag;

}weight_filter_t;



extern weight_filter_t weight_filter;



void weight_filter_init(weight_filter_t *filter, float alpha);

int32_t weight_filter_update(weight_filter_t *filter, int32_t adc_value);




#endif

