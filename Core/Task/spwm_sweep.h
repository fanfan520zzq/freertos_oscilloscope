//
// Created by Agent on 2026/03/25.
//

#ifndef SPWM_SWEEP_H
#define SPWM_SWEEP_H

#include "main.h"

// 缓冲区大小定义：Ping-Pong 缓冲，总大小 2016，半区 1008
// 注意：必须保证 size * sizeof(uint16_t) 为 32 字节的倍数以满足 D-Cache 对齐要求
// 2016 * 2 = 4032 bytes = 126 * 32, 1008 * 2 = 2016 bytes = 63 * 32
#define SPWM_BUF_SIZE       2016
#define SPWM_HALF_BUF_SIZE  1008

// 采用1024点的正弦波表
#define SINE_LUT_SIZE       1024

// SPWM 的 ARR 最大值，根据 TIM8 设置 Period=2399，最大占空比应为 2400
#define SPWM_ARR_MAX        2400

typedef struct {
    uint16_t freq;           // 频率 Hz
    uint32_t ftw;            // 预计算的频率控制字 (Frequency Tuning Word)
    uint32_t target_pulse;   // 该频点持续的脉冲数量
} SPWM_SweepPoint_t;

// API

/**
 * @brief 初始化 SPWM 模块 (正弦LUT + 默认状态)
 */
void SPWM_Sweep_Init(void);

/**
 * @brief 启动扫频模式
 */
void SPWM_Sweep_Start(void);

/**
 * @brief 设置定频输出 (10Hz-1kHz, 1Hz分辨率)
 * @param freq_hz 目标频率 (10-1000 Hz)
 * @note  DMA运行中调用可动态切频，无需先Stop
 */
void SPWM_Set_Frequency(uint16_t freq_hz);

/**
 * @brief 强制重启定频输出 (先Stop再Start，相位归零)
 * @param freq_hz 目标频率 (10-1000 Hz)
 */
void SPWM_Set_Frequency_Restart(uint16_t freq_hz);

/**
 * @brief 停止 SPWM 输出
 */
void SPWM_Sweep_Stop(void);

#endif // SPWM_SWEEP_H
