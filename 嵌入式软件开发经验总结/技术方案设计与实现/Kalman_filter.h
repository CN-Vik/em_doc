/**
 * @file Kalman_filter.h
 * @author vik (ufo281@outlook.com)
 * @brief 
 * 
 *  use example:
 * 
 * KalmanFilter1D my_filter;

void setup() {
    // 假设第一次读取到的传感器值为 50.0
    // Q = 0.01 (认为真实温度变化很慢)
    // R = 0.5 (认为传感器读数容易上下跳动 0.5 的幅度)
    Kalman1D_Init(&my_filter, 50.0f, 0.01f, 0.5f); 
}

void loop() {
    float raw_data = Read_Sensor(); // 读取传感器原始毛刺数据
    
    // 送入卡尔曼滤波，得到平滑结果
    float filtered_data = Kalman1D_Update(&my_filter, raw_data); 
    
    // 使用 filtered_data 进行后续控制...
}
 * @version 0.1
 * @date 2026-07-28
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#ifndef KALMAN_FILTER_H
#define KALMAN_FILTER_H




#endif

