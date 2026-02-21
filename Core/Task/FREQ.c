//
// Created by Lenovo on 2026/2/17.
//

#include "FREQ.h"
#include <tim.h>
#include "stdio.h"

//TIM15 - 1S timer
//TIM2,3  - Input Capturer

#define Fs 240000000
uint8_t State_CH1 =0, State_CH2 = 0;
uint32_t T1_CH1, T2_CH1 , T1_CH2, T2_CH2 ;
uint32_t Nx_CH1, Nx_CH2 = 0;
uint32_t TIM2_Over_Cnt = 0,TIM3_Over_Cnt = 0;
extern double Fx_CH1=0 , Fx_CH2=0;

uint8_t IC_CH1_flag=0 , IC_CH2_flag=0;

void ADC_Init(void);
void Trigger_Frequency_Measurement(void);
void process_frequency_logic(TIM_HandleTypeDef *htim,
                             volatile uint8_t *pState,
                             uint32_t *pT1,
                             uint32_t *pT2,
                             uint32_t *pNx,
                             volatile uint32_t *pOverCnt,
                             double *pFx
                             );
void StartFREQTask(void *argument) {
    uint32_t nx_start, ns_start;
    uint32_t nx_end, ns_end;
    uint32_t delta_nx, delta_ns;

    HAL_TIM_Base_Start(&htim3); // 测量通道1
    HAL_TIM_Base_Start(&htim4); // 测量通道2
    HAL_TIM_Base_Start(&htim5); // 基准时钟
    while (1) {
        osSemaphoreAcquire(FreqSEMHandle,osWaitForever);

        nx_start = __HAL_TIM_GET_COUNTER(&htim3);
        ns_start = __HAL_TIM_GET_COUNTER(&htim5);

        osDelay(1000);


        nx_end = __HAL_TIM_GET_COUNTER(&htim3);
        ns_end = __HAL_TIM_GET_COUNTER(&htim5);

        delta_nx = nx_end - nx_start;
        delta_ns = ns_end - ns_start;

        if (delta_ns > 0) {
            Fx_CH1 = (double)delta_nx * Fs / (double)delta_ns;
        }

        osSemaphoreRelease(ADCSEMHandle);
        osDelay(10);

    }

} //test  freq:AA 09 AD 00 00 00 00 60 01

// void Trigger_Frequency_Measurement(void) {
//     // 1. 初始化/重置所有状态变量
//     State_CH1 = 0;
//     State_CH2 = 0;
//     TIM2_Over_Cnt = 0;
//     TIM3_Over_Cnt = 0;
//
//     // 2. 启动输入捕获定时器（它们现在处于状态0，等待第一个上升沿）
//     HAL_TIM_IC_Start_IT(&htim2, TIM_CHANNEL_1);
//     HAL_TIM_IC_Start_IT(&htim3, TIM_CHANNEL_1);
//
//     // 3. 启动各自的基准时钟（用于记录 Ns）
//     HAL_TIM_Base_Start_IT(&htim2);
//     HAL_TIM_Base_Start_IT(&htim3);
//
//
//
//     // 4. 开启 1s 闸门定时器
//     // 当 TIM15 溢出时，会在回调中把 State 改为 2，通知捕获逻辑准备关闸
//     __HAL_TIM_SET_COUNTER(&htim15, 0);
//     HAL_TIM_Base_Start_IT(&htim15);
// }


// void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim) {
//     if (htim->Instance == TIM2) process_frequency_logic(&htim2,&State_CH1,&T1_CH1,&T2_CH1,&Nx_CH1,&TIM2_Over_Cnt,&Fx_CH1);
//     if (htim->Instance == TIM3) process_frequency_logic(&htim3,&State_CH2,&T1_CH2,&T2_CH2,&Nx_CH2,&TIM3_Over_Cnt,&Fx_CH2);
// }



// void process_frequency_logic(TIM_HandleTypeDef *htim,
//                              volatile uint8_t *pState,
//                              uint32_t *pT1,
//                              uint32_t *pT2,
//                              uint32_t *pNx,
//                              volatile uint32_t *pOverCnt,
//                              double *pFx
//                              )
// {
//     if (*pState == 0) {
//         // --- 状态 0: 捕获到第一个边沿，测量开始 ---
//         *pT1 = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_1);
//         *pOverCnt = 0;
//         *pNx = 0;
//         *pState = 1;
//     }
//     else if (*pState == 1) {
//         // --- 状态 1: 闸门开启期间，仅累加被测信号脉冲数 ---
//         (*pNx)++;
//     }
//     else if (*pState == 2) {
//         // --- 状态 2: 1s 时间已过，捕获最后一个边沿，结束测量 ---
//         *pT2 = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_1);
//         (*pNx)++; // 加上最后这一个脉冲
//         double Ns = (double)(*pOverCnt) * 65536.0f + (double)(*pT2) - (double)(*pT1);
//
//         // 核心公式: Fx = (Nx * Fs) / Ns
//         if (Ns > 0) {
//             *pFx = (double)(*pNx) * Fs / Ns;
//         }
//     }
// }





