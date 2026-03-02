//
// Created by Lenovo on 2026/2/22.
//

#include "FFTTask.h"
#define ARM_MATH_CM7  // 必须在包含头文件前定义
#include "arm_math.h"

#include "stdio.h"
#include "string.h"


float CH1_OUT[LEN],CH2_OUT[LEN];
float CH1_IN[LEN],CH2_IN[LEN];
float hanning_window[LEN];
float CH1_MAG[LEN/2+1],CH2_MAG[LEN/2+1];


char CH1_TYPE[10],CH2_TYPE[10];
char CH1_VPP[10],CH2_VPP[10];
char CH1_FREQ[10],CH2_FREQ[10];


void Get_VPP(void);
void Hanning_Init(void);
void Type_recognition(void);
void StartFFTTask(void *argument) {
    arm_rfft_fast_instance_f32 S;
    arm_rfft_fast_init_f32(&S, 1024);
    Hanning_Init();

    while (1) {
        osSemaphoreAcquire(FFTSEMHandle,osWaitForever);
        Get_VPP();
        arm_mult_f32(CH1_DATA, hanning_window, CH1_IN, LEN);
        arm_mult_f32(CH2_DATA, hanning_window, CH2_IN, LEN);
        arm_rfft_fast_f32(&S,CH1_IN, CH1_OUT, 0);
        arm_rfft_fast_f32(&S,CH2_IN, CH2_OUT, 0);

        CH1_MAG[0]=fabsf(CH1_OUT[0]);
        CH2_MAG[0]=fabsf(CH2_OUT[0]);

        arm_cmplx_mag_f32(&CH1_OUT[2], &CH1_MAG[1], (LEN / 2)-1);
        arm_cmplx_mag_f32(&CH2_OUT[2], &CH2_MAG[1], (LEN / 2)-1);

        CH1_MAG[LEN/2]=fabsf(CH1_OUT[1]);
        CH2_MAG[LEN/2]=fabsf(CH2_OUT[1]);

        // for (uint16_t i = 0; i < LEN/2; i++) {
        //     printf("%.3f \n",CH1_MAG[i]);
        // }
        Type_recognition();
        int a=1;

        osSemaphoreRelease(LCDSEMHandle);
    }
}

void Get_VPP(void) {
    memset(CH1_VPP, 0, sizeof(CH1_VPP));
    memset(CH2_VPP, 0, sizeof(CH2_VPP));
    memset(CH1_FREQ, 0, sizeof(CH1_FREQ));
    memset(CH2_FREQ, 0, sizeof(CH2_FREQ));
    float32_t maxVal, minVal;
    uint32_t maxIdx, minIdx;
    arm_max_f32(CH1_DATA, LEN, &maxVal, &maxIdx);
    arm_min_f32(CH1_DATA, LEN, &minVal, &minIdx);
    sprintf(CH1_VPP,"%f",maxVal-minVal);

    arm_max_f32(CH2_DATA, LEN, &maxVal, &maxIdx);
    arm_min_f32(CH2_DATA, LEN, &minVal, &minIdx);
    sprintf(CH2_VPP,"%f",maxVal-minVal);

    sprintf(CH1_FREQ,"%f",Fx_CH1);
    sprintf(CH2_FREQ,"%f",Fx_CH2);
}

void Hanning_Init(void) {
    for (int i = 0; i < LEN; i++) {
        // 汉宁窗公式: 0.5 * (1 - cos(2*PI*i / (LEN-1)))
        hanning_window[i] = 0.5f * (1.0f - arm_cos_f32(2.0f * PI * i / (float32_t)(LEN - 1)));
    }
}
float findMaxInRange(float arr[], uint32_t start, uint32_t end);


void Type_recognition(void) {
    memset(CH1_TYPE, 0, sizeof(CH1_TYPE));
    memset(CH2_TYPE, 0, sizeof(CH2_TYPE));

    float maxVal;
    uint32_t Fundmental_idx;
    arm_max_f32(&CH1_MAG[1], (LEN/2)-1, &maxVal, &Fundmental_idx);
    Fundmental_idx +=1;
    uint32_t harmonic3_idx = Fundmental_idx * 3 ;
    float harmonic3_val=findMaxInRange(CH1_MAG, harmonic3_idx-2, harmonic3_idx+2);
    float CH1_ratio = harmonic3_val / maxVal;
    if (CH1_ratio<0.0f) {
        if (Fx_CH1<10) strcpy(CH1_TYPE, "current");
        else strcpy(CH1_TYPE, "sine");
    }
    else if (CH1_ratio<0.25f)  strcpy(CH1_TYPE, "triangle");
    else if (CH1_ratio>0.25f)  strcpy(CH1_TYPE, "square");

    arm_max_f32(&CH2_MAG[1], (LEN/2)-1, &maxVal, &Fundmental_idx);
    Fundmental_idx +=1;
    harmonic3_idx = Fundmental_idx * 3 ;
    harmonic3_val=findMaxInRange(CH2_MAG, harmonic3_idx-2, harmonic3_idx+2);
    float CH2_ratio = harmonic3_val / maxVal;
    if (CH2_ratio<0.0f) {
        if (Fx_CH2<10) strcpy(CH2_TYPE, "current");
        else strcpy(CH2_TYPE, "sine");
    }
    else if (CH2_ratio<0.25f)  strcpy(CH2_TYPE, "triangle");
    else if (CH2_ratio>0.25f)  strcpy(CH2_TYPE, "square");
}


float findMaxInRange(float arr[], uint32_t start, uint32_t end) {
    float max = arr[start];  // 初始化为区间第一个元素
    for (uint16_t i = start + 1; i <= end; i++) {
        if (arr[i] > max) {
            max = arr[i];
        }
    }
    return max;
}
