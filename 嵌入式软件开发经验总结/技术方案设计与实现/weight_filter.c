/**
 * @file weight_filter.c
 * @author vik (ufo281@outlook.com)
 * @brief 中值滤波和一阶低通滤波
 * @version 0.1
 * @date 2026-07-30
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#include "weight_filter.h"
#include <string.h>


weight_filter_t weight_filter = {0};


/*
 *中值滤波
 */
static int32_t median_filter(int32_t *buf)
{
    int32_t temp[MEDIAN_LEN];

    memcpy(temp, buf, sizeof(temp));


    //冒泡排序
    for(int i=0;i<MEDIAN_LEN;i++)
    {
        for(int j=i+1;j<MEDIAN_LEN;j++)
        {
            if(temp[i] > temp[j])
            {
                int32_t t=temp[i];
                temp[i]=temp[j];
                temp[j]=t;
            }
        }
    }


    return temp[2];
}



/*
 * 初始化
 */
void weight_filter_init(weight_filter_t *filter, float alpha)
{
    memset(filter,0,sizeof(weight_filter_t));

    filter->alpha = alpha;

}



/*
 * 更新一次重量数据
 */
int32_t weight_filter_update(weight_filter_t *filter, int32_t adc_value)
{

    /*
     * 第一次进入
     * 防止启动慢慢爬升
     */
    if(filter->init_flag == 0)
    {
        for(int i=0;i<MEDIAN_LEN;i++)
        {
            filter->median_buf[i]=adc_value;
        }

        filter->lpf_value = adc_value;

        filter->init_flag=1;
    }


    /*
     * ===========
     * 1. 中值滤波
     * ===========
     */

    filter->median_buf[filter->index]=adc_value;


    filter->index++;

    if(filter->index>=MEDIAN_LEN)
    {
        filter->index=0;
    }


    int32_t median_value;


    median_value = median_filter(filter->median_buf);



    /*
     * ===========
     * 2. 一阶低通
     * ===========
     *
     * y(k)=y(k-1)+a(x-y(k-1))
     *
     */


    filter->lpf_value += 
        filter->alpha *
        ((float)median_value - filter->lpf_value);



    return (int32_t)filter->lpf_value;

}

