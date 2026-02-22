//
// Created by Lenovo on 2026/2/17.
//

#include "FREQ.h"
#include <tim.h>
#include "stdio.h"

#define Fs 240000000
extern double Fx_CH1=0 , Fx_CH2=0;

uint8_t over_cnt=0;

void StartFREQTask(void *argument) {

    while (1) {
        osSemaphoreAcquire(FreqSEMHandle,osWaitForever);
        over_cnt=0;
        // 1. 准备：清零并开启 Slave（此时由于闸门关着，它们不会动）
        HAL_TIM_Base_Start(&htim2);
        HAL_TIM_Base_Start(&htim5);
        HAL_TIM_Base_Start_IT(&htim15);

        // 2. 踢一脚：开启 TIM15 (One Pulse Mode)
        HAL_TIM_Base_Start(&htim1);

        // 3. 等待：osDelay(1000) 即可，此时不要求准，只要等它跑完
        osDelay(1000);

        // 4. 读数：此时三个定时器已经由硬件同步锁死了
        uint32_t nx1 = __HAL_TIM_GET_COUNTER(&htim2);
        uint32_t nx2 = __HAL_TIM_GET_COUNTER(&htim5);
        uint16_t ns  = __HAL_TIM_GET_COUNTER(&htim15);
        uint32_t ns_full = (over_cnt*65536) + ns ;
        // 5. 计算

        double fx1 = (double)nx1 * 240000000.0 / ns_full;
        double fx2 = (double)nx2 * 240000000.0 / ns_full;
        osSemaphoreRelease(ADCSEMHandle);
        osDelay(10);

    }

} //test  freq:AA 09 AD 00 00 00 00 60 01

