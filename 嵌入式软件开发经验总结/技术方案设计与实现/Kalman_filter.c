/**
 * @file Kalman_filter.c
 * @author vik (ufo281@outlook.com)
 * @brief 
 * @version 0.1
 * @date 2026-07-28
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#include "Kalman_filter.h"



// 定义卡尔曼滤波器的结构体
typedef struct {
    float x;  // 系统的状态量 (最优估计值，也就是最终的滤波输出)
    float p;  // 估计误差协方差 (系统对当前预测的“信心”大小)
    float q;  // 过程噪声 (系统真实状态发生改变的波动程度)
    float r;  // 测量噪声 (传感器自身的硬件底噪)
} KalmanFilter1D;

// 初始化函数
// 初始值 init_x 可以设为第一次读到的传感器值
// q 和 r 是需要你根据实际情况调整的参数
void Kalman1D_Init(KalmanFilter1D *kf, float init_x, float q, float r) {
    kf->x = init_x;
    kf->p = 1.0f;   // 初始误差可以随便设一个非零值，算法跑几次后会自动收敛
    kf->q = q;      // 例如: 0.01
    kf->r = r;      // 例如: 0.1
}

// 核心计算函数 (每次采集到新数据时调用)
float Kalman1D_Update(KalmanFilter1D *kf, float measurement) {
    // 1. 预测阶段
    // 对于温度、ADC等简单信号，我们假设它静止不变，所以预测值就是上一次的值 x。
    // 但我们的不确定性 p 增加了，因为时间推移，现实可能发生了变化 (加上过程噪声 q)
    kf->p = kf->p + kf->q;

    // 2. 计算卡尔曼增益 K (核心！)
    // 如果测量噪声 r 很大，K 就会变小；如果预测误差 p 很大，K 就会变大趋近于 1
    float k = kf->p / (kf->p + kf->r);
    
    // 3. 更新阶段：融合预测值与测量值
    // 新的最优估计 = 旧预测值 + 增益 * (测量值 - 旧预测值)
    kf->x = kf->x + k * (measurement - kf->x);
    
    // 4. 更新误差协方差，供下一次循环使用
    kf->p = (1.0f - k) * kf->p;
    
    return kf->x;
}
