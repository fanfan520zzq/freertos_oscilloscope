//
// Created by Lenovo on 2026/2/22.
//

#include "FFTTask.h"
#define ARM_MATH_CM7  // 必须在包含头文件前定义
#include "arm_math.h"

#include "stdio.h"
#include "string.h"
#include "usart.h"

#define RATE 2000000.0f

int __io_putchar(int ch) {
    HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
    return ch;
}

float CH1_FLT[LEN],CH2_FLT[LEN] __attribute__((section(".dma_buffer")));
float CH1_OUT[LEN],CH2_OUT[LEN] __attribute__((section(".dma_buffer")));
float CH1_IN[LEN],CH2_IN[LEN] __attribute__((section(".dma_buffer")));
float hanning_window[LEN] __attribute__((section(".dma_buffer")));
float CH1_MAG[LEN/2+1],CH2_MAG[LEN/2+1] __attribute__((section(".dma_buffer")));

typedef struct {
    uint16_t max;
    uint16_t min;
    uint16_t mid;
}ADC_Stat_t;

float CH1_FREQ=0,CH2_FREQ=0;


ADC_Stat_t Find_Stats(uint16_t *data, uint16_t len);
ADC_Stat_t CHANNEL_1,CHANNEL_2;
void Hanning_Init(void);
void P2P_Analysis(void);
float Measure_Frequency_Software(uint16_t* buffer, uint32_t len, float sample_rate, uint16_t v_mid) ;

void StartFFTTask(void *argument) {
    Hanning_Init();
    arm_rfft_fast_instance_f32 S;
    arm_rfft_fast_init_f32(&S, 4096);

    memset(&CHANNEL_1,0,sizeof(ADC_Stat_t));
    memset(&CHANNEL_2,0,sizeof(ADC_Stat_t));

    const float32_t ADC_TO_V = 3.3f / 65535.0f; // 预乘系数

    while (1) {
        osSemaphoreAcquire(FFTSEMHandle,osWaitForever);


        P2P_Analysis();

        CH1_FREQ = Measure_Frequency_Software(CH1_Buffer, LEN , RATE , CHANNEL_1.mid) ;
        CH2_FREQ = Measure_Frequency_Software(CH2_Buffer, LEN , RATE , CHANNEL_2.mid) ;

        for (uint16_t i = 0 ; i< LEN ;i++) {
            CH1_FLT[i] = (float)CH1_Buffer[i] * ADC_TO_V;
            CH2_FLT[i] = (float)CH2_Buffer[i] * ADC_TO_V;
        }
        arm_mult_f32(CH1_FLT, hanning_window, CH1_IN, LEN);
        arm_mult_f32(CH2_FLT, hanning_window, CH2_IN, LEN);
        arm_rfft_fast_f32(&S,CH1_IN, CH1_OUT, 0);
        arm_rfft_fast_f32(&S,CH2_IN, CH2_OUT, 0);
        CH1_MAG[0]=fabsf(CH1_OUT[0]);
        CH2_MAG[0]=fabsf(CH2_OUT[0]);

        arm_cmplx_mag_f32(&CH1_OUT[2], &CH1_MAG[1], (LEN / 2)-1);
        arm_cmplx_mag_f32(&CH2_OUT[2], &CH2_MAG[1], (LEN / 2)-1);

        CH1_MAG[LEN/2]=fabsf(CH1_OUT[1]);
        CH2_MAG[LEN/2]=fabsf(CH2_OUT[1]);


        for (uint16_t i = 0; i < LEN/2; i++) {
            printf("%.3f \n",CH1_MAG[i]);
        }


    }
}

void P2P_Analysis() {

    CHANNEL_1 = Find_Stats(CH1_Buffer,LEN);
    CHANNEL_2 = Find_Stats(CH2_Buffer,LEN);
}

float Measure_Frequency_Software(uint16_t* buffer, uint32_t len, float sample_rate, uint16_t v_mid) {
    double first_cross_index = -1.0;
    double last_cross_index = -1.0;
    uint32_t cross_count = 0;

    // 遍历数组，寻找上升沿过零点
    // 注意：从 0 到 len-1 遍历，对比 i 和 i+1
    for (uint32_t i = 0; i < len - 1; i++) {
        // 判断上升沿：当前点在中值以下，下一点在中值以上
        if (buffer[i] < v_mid && buffer[i+1] >= v_mid) {

            // 执行线性插值计算精确下标位置
            // formula: exact_index = i + (Vmid - V[i]) / (V[i+1] - V[i])
            double exact_index = (double)i + (double)(v_mid - buffer[i]) / (buffer[i+1] - buffer[i]);

            if (first_cross_index < 0) {
                // 记录第一个过零点
                first_cross_index = exact_index;
            }
            // 持续更新最后一个过零点
            last_cross_index = exact_index;
            cross_count++;
        }
    }

    // 如果检测到的周期数太少（少于2个完整周期），无法计算频率
    if (cross_count < 2) {
        return 0.0f;
    }

    // 计算总点数（含小数部分）和总周期数
    // 注意：n个点之间有 n-1 个周期
    double total_samples = last_cross_index - first_cross_index;
    uint32_t total_cycles = cross_count - 1;

    // 频率 = 采样率 / 平均每个周期的采样点数
    float freq = (float)(sample_rate * total_cycles / total_samples);

    return freq;
}
void Hanning_Init(void) {
    for (int i = 0; i < LEN; i++) {
        // 汉宁窗公式: 0.5 * (1 - cos(2*PI*i / (LEN-1)))
        hanning_window[i] = 0.5f * (1.0f - arm_cos_f32(2.0f * PI * i / (float32_t)(LEN - 1)));
    }
}

// void Type_recognition(void) {
//     memset(CH1_TYPE, 0, sizeof(CH1_TYPE));
//     memset(CH2_TYPE, 0, sizeof(CH2_TYPE));
//
//     float maxVal;
//     uint32_t Fundmental_idx;
//     arm_max_f32(&CH1_MAG[1], (LEN/2)-1, &maxVal, &Fundmental_idx);
//     Fundmental_idx +=1;
//     uint32_t harmonic3_idx = Fundmental_idx * 3 ;
//     float harmonic3_val=findMaxInRange(CH1_MAG, harmonic3_idx-2, harmonic3_idx+2);
//     float CH1_ratio = harmonic3_val / maxVal;
//     if (CH1_ratio<0.0f) {
//         if (Fx_CH1<10) strcpy(CH1_TYPE, "current");
//         else strcpy(CH1_TYPE, "sine");
//     }
//     else if (CH1_ratio<0.25f)  strcpy(CH1_TYPE, "triangle");
//     else if (CH1_ratio>0.25f)  strcpy(CH1_TYPE, "square");
//
//     arm_max_f32(&CH2_MAG[1], (LEN/2)-1, &maxVal, &Fundmental_idx);
//     Fundmental_idx +=1;
//     harmonic3_idx = Fundmental_idx * 3 ;
//     harmonic3_val=findMaxInRange(CH2_MAG, harmonic3_idx-2, harmonic3_idx+2);
//     float CH2_ratio = harmonic3_val / maxVal;
//     if (CH2_ratio<0.0f) {
//         if (Fx_CH2<10) strcpy(CH2_TYPE, "current");
//         else strcpy(CH2_TYPE, "sine");
//     }
//     else if (CH2_ratio<0.25f)  strcpy(CH2_TYPE, "triangle");
//     else if (CH2_ratio>0.25f)  strcpy(CH2_TYPE, "square");
// }



ADC_Stat_t Find_Stats(uint16_t *data, uint16_t len) {
    ADC_Stat_t res = {0, 4095, 0.0f};
    uint32_t sum = 0;

    for (uint16_t i = 0; i < len; i++) {
        uint16_t val = data[i];

        if (val > res.max) res.max = val;
        if (val < res.min) res.min = val;
    }

    res.mid = (res.max+res.min)/2;
    return res;
}
