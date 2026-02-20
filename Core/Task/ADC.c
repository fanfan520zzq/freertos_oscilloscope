//
// Created by Lenovo on 2026/2/20.
//

#include "ADC.h"
#include "tim.h"

//osSemaphoreAcquire(ADCSEMHandle,osWaitForever);

//TIM4-CH1 TIM1-CH2

uint32_t Sample_Rate_CH1 = 0 , Sample_Rate_CH2 = 0 ;
uint8_t psc_CH1=0, psc_CH2=0;
uint16_t arr_CH1=0, arr_CH2=0;
uint16_t CH1_Buffer[1024] __attribute__((section(".dma_buffer")));
uint16_t CH2_Buffer[1024] __attribute__((section(".dma_buffer")));

typedef struct {
    TIM_HandleTypeDef* htim;
    uint8_t* psc_val;
    uint16_t* arr_val;
    uint32_t* sample_rate;
} ADC_Channel_Cfg;

// 在函数外部获取配置
ADC_Channel_Cfg Get_Cfg(uint8_t channel) {
    if (channel == CH1) return (ADC_Channel_Cfg){&htim4, &psc_CH1, &arr_CH1, &Sample_Rate_CH1};
    return (ADC_Channel_Cfg){&htim1, &psc_CH2, &arr_CH2, &Sample_Rate_CH2};
}

void Set_sample_rate_arr(double freq, uint8_t channel);
void StartADCTask(void *argument) {
    osSemaphoreAcquire(ADCSEMHandle,osWaitForever);

    int k=1;


}

void Set_sample_rate_arr(double freq, uint8_t channel) {
    ADC_Channel_Cfg cfg = Get_Cfg(channel);
    uint32_t psc_tmp = 0, arr_tmp = 0, sr_tmp = 0;
    const uint32_t timer_clk = 240000000;

    if (freq < 1200) {
        psc_tmp = 24 - 1;
        arr_tmp = 100 - 1;
        sr_tmp  = 100000;
    }
    else if (freq <= 110000) {
        psc_tmp = 24 - 1;
        arr_tmp = 1000 - 1;
        sr_tmp  = 1000000;
    }
    else {
        psc_tmp = 1 - 1;
        arr_tmp = (uint32_t)((float)timer_clk / freq * 17.0f / 16.0f) - 1;
        sr_tmp  = (uint32_t)(16.0f * freq / 17.0f); // 实际 ADC 触发频率
    }

    // 更新全局变量和寄存器
    *cfg.psc_val = (uint8_t)psc_tmp;
    *cfg.arr_val = (uint16_t)arr_tmp;
    *cfg.sample_rate = sr_tmp;
    __HAL_TIM_SET_PRESCALER(cfg.htim, psc_tmp);
    __HAL_TIM_SET_AUTORELOAD(cfg.htim, arr_tmp);
    //HAL_TIM_GenerateEvent(cfg.htim, TIM_EVENTSOURCE_UPDATE);
}