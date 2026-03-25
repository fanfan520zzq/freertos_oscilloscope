//
// Created by Agent on 2026/03/25.
//

#ifndef SPWM_SWEEP_H
#define SPWM_SWEEP_H

#include "main.h"

// 缓冲区大小定义：Ping-Pong 缓冲，总大小 2000，半区 1000
#define SPWM_BUF_SIZE       2000
#define SPWM_HALF_BUF_SIZE  1000

// 采用1024点的正弦波表
#define SINE_LUT_SIZE       1024

// SPWM 的 ARR 最大值，假设 TIM1 频率配置使载波达到 100kHz，这里设置为占空比计数的最大范围
// 请根据实际 TIM1 的 ARR 值修改 (例如如果 APB 是 200MHz, 分频0, 则为 2000)
#define SPWM_ARR_MAX        1000

typedef struct {
    uint16_t freq;           // 频率 Hz
    uint32_t ftw;            // 预计算的频率控制字 (Frequency Tuning Word)
    uint32_t target_pulse;   // 该频点持续的脉冲数量
} SPWM_SweepPoint_t;

// API
void SPWM_Sweep_Init(void);
void SPWM_Sweep_Start(void);
void SPWM_Sweep_Stop(void);

#endif // SPWM_SWEEP_H
