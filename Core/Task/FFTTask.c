//
// Created by Lenovo on 2026/2/22.
//

#include "FFTTask.h"
#define ARM_MATH_CM7  // 必须在包含头文件前定义
#include "arm_math.h"

float VPP_CH1=0,VPP_CH2=0;
float CH1_OUT[LEN],CH2_OUT[LEN];
float CH1_MAG[LEN/2],CH2_MAG[LEN/2];


void StartFFTTask(void *argument) {
    arm_rfft_fast_instance_f32 S;
    arm_rfft_fast_init_f32(&S, 1024);
    osSemaphoreAcquire(FFTSEMHandle,osWaitForever);

    while (1) {
        arm_rfft_fast_f32(&S,CH1_DATA, CH1_OUT, 0);
        arm_rfft_fast_f32(&S,CH2_DATA, CH2_OUT, 0);

        arm_cmplx_mag_f32(CH1_OUT, CH1_MAG, LEN / 2);
        arm_cmplx_mag_f32(CH2_OUT, CH2_MAG, LEN / 2);


    }

}


void Get_VPP(void) {
    float32_t maxVal, minVal;
    uint32_t maxIdx, minIdx;
    arm_max_f32(CH1_DATA, LEN, &maxVal, &maxIdx);
    arm_min_f32(CH1_DATA, LEN, &minVal, &minIdx);
    VPP_CH1 = maxVal - minVal;
    arm_max_f32(CH2_DATA, LEN, &maxVal, &maxIdx);
    arm_min_f32(CH2_DATA, LEN, &minVal, &minIdx);
    VPP_CH2 = maxVal - minVal;
}