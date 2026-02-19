//
// Created by Lenovo on 2026/2/17.
//

#include "ADC.h"
#include <tim.h>
#include "stdio.h"
#define IC_BUF_SIZE 32     // 采样深度，越大越稳定但刷新越慢
#define IC_PRESCALER 8      // 对应你截图中的 Division by 8

//TIM15 - 1S timer
//TIM2  - Input Capturer

#define Fs 240000000
uint32_t State = 0;
uint32_t T1, T2;
uint32_t Nx = 0;
uint32_t TIM2_Over_Cnt = 0;
double Fx=0;

uint8_t IC_flag=0;

// HAL_TIM_Base_Start_IT(&htim15);
// HAL_TIM_Base_Start_IT(&htim2);
// HAL_TIM_IC_Start_IT(&htim2, TIM_CHANNEL_1);

void StartADCTask(void *argument) {


    while (1) {
        osSemaphoreAcquire(FreqSEMHandle,osWaitForever);

        if (IC_flag == 0) {
            HAL_TIM_Base_Start_IT(&htim2);
            HAL_TIM_IC_Start_IT(&htim2, TIM_CHANNEL_1);
            IC_flag = 1;
        }


        osDelay(10);

    }



}

void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim) {
    if (htim->Instance == TIM2) {
        if (State == 0) {
            // 状态0：捕获到第一个上升沿，开启测量
            T1 = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_1);
            TIM2_Over_Cnt = 0;
            Nx = 0;
            State = 1;
            HAL_TIM_Base_Start_IT(&htim15); // 开启1s定时闸门
        }
        else if (State == 1) {
            Nx++; // 闸门期间，只管累加被测信号个数
        }
        else if (State == 2) {
            // 状态2：1s时间已过，捕获最后一个上升沿，结束测量
            T2 = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_1);
            Nx++;


            double Ns = (double)TIM2_Over_Cnt * 65535.0f + T2 - T1;
            Fx = (double)Nx * Fs / Ns;

            HAL_TIM_Base_Stop_IT(&htim15);
            State = 0; // 重置状态，等待下一次任务启动
        }
    }
}




// float freq1=0,freq2=0;
// uint32_t IC_Cnt1=0,IC_Cnt2=0;
// uint32_t IC_Buffer1[IC_BUF_SIZE] __attribute__((section(".dma_buffer")));
// uint32_t IC_Buffer2[IC_BUF_SIZE] __attribute__((section(".dma_buffer")));
// uint8_t ic_flag1=0,ic_flag2=0;
//
//
// void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim){
//     if(htim->Channel==HAL_TIM_ACTIVE_CHANNEL_1){
//         IC_Cnt1 = IC_Buffer1[IC_BUF_SIZE-1]-IC_Buffer1[0];
//         freq1 =  (float)(IC_BUF_SIZE-1)*8*240000000/IC_Cnt1;
//         HAL_TIM_IC_Stop_DMA(&htim2,TIM_CHANNEL_1);
//         // HAL_TIM_IC_Start_DMA(&htim2,TIM_CHANNEL_1,IC_Buffer1,IC_BUF_SIZE);
//         // ic_flag1=1;
//     }
//     else if(htim->Channel==HAL_TIM_ACTIVE_CHANNEL_3){
//         //...
//     }
// }
//
//
// void IC_ON(){
//     //ic_flag1=0;
//     HAL_TIM_IC_Start_DMA(&htim2,TIM_CHANNEL_1,IC_Buffer1,IC_BUF_SIZE);
// }
