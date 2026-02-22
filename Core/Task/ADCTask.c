//
// Created by Lenovo on 2026/2/20.
//
#include <adc.h>
#include "ADCTask.h"
#include "tim.h"
#include "stdio.h"
#include "usart.h"


//osSemaphoreAcquire(ADCSEMHandle,osWaitForever);


uint32_t Sample_Rate_CH1 = 0 , Sample_Rate_CH2 = 0 ;
uint8_t psc_CH1=0, psc_CH2=0;
uint16_t arr_CH1=0, arr_CH2=0;
uint16_t CH1_Buffer[LEN] __attribute__((section(".dma_buffer"))) __attribute__((aligned(32)));
uint16_t CH2_Buffer[LEN] __attribute__((section(".dma_buffer"))) __attribute__((aligned(32)));
uint8_t flag_CH1=0,flag_CH2=0;

float CH1_DATA[LEN], CH2_DATA[LEN];


typedef struct {
    TIM_HandleTypeDef* htim;
    uint8_t* psc_val;
    uint16_t* arr_val;
    uint32_t* sample_rate;
} ADC_Channel_Cfg;

// 在函数外部获取配置
ADC_Channel_Cfg Get_Cfg(uint8_t channel) {
    if (channel == CH1) return (ADC_Channel_Cfg){&htim3, &psc_CH1, &arr_CH1, &Sample_Rate_CH1};
    return (ADC_Channel_Cfg){&htim4, &psc_CH2, &arr_CH2, &Sample_Rate_CH2};
}


int __io_putchar(int ch) {
    HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
    return ch;
}


void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc){
    if(hadc == &hadc1){
        //SCB_InvalidateDCache_by_Addr((uint32_t*)CH1_Buffer, sizeof(CH1_Buffer));
        HAL_ADC_Stop(&hadc1);
        flag_CH1=1;
    }
    if(hadc == &hadc2) {
        //SCB_InvalidateDCache_by_Addr((uint32_t*)CH2_Buffer, sizeof(CH2_Buffer));
        HAL_ADC_Stop(&hadc2);
        flag_CH2=1;
    }
}

void Start_Sample(void);
void Set_sample_rate_arr(double freq, uint8_t channel);

void StartADCTask(void *argument) {
    osSemaphoreAcquire(ADCSEMHandle,osWaitForever);
    Set_sample_rate_arr(Fx_CH1,CH1);
    Set_sample_rate_arr(Fx_CH2,CH2);
    Start_Sample();


    while (1) {
        if ((flag_CH2==1)&&(flag_CH1==1)) {
            for (uint16_t i = 0; i < LEN; i++) {
                CH1_DATA[i] = (float)CH1_Buffer[i]/65535.0f*3.3f;
                CH2_DATA[i] = (float)CH2_Buffer[i]/65535.0f*3.3f;
                printf("%.3f , %.3f \r\n",CH1_DATA[i],CH2_DATA[i]);
            }
            //osSemaphoreRelease(FFTSEMHandle);
        }
    }

}

void Start_Sample(void) {
    flag_CH1=0;  flag_CH2=0;
    HAL_ADC_Stop_DMA(&hadc1);
    HAL_ADC_Stop_DMA(&hadc2);

    HAL_TIM_Base_Start(&htim3);
    HAL_TIM_Base_Start(&htim4);
    // SCB_CleanDCache_by_Addr((uint32_t*)CH1_Buffer, sizeof(CH1_Buffer));
    // SCB_CleanDCache_by_Addr((uint32_t*)CH2_Buffer, sizeof(CH2_Buffer));
    HAL_ADC_Start_DMA(&hadc1,(uint32_t*)CH1_Buffer,LEN);
    HAL_ADC_Start_DMA(&hadc2,(uint32_t*)CH2_Buffer,LEN);
}

void Set_sample_rate_arr(double freq, uint8_t channel) {
    ADC_Channel_Cfg cfg = Get_Cfg(channel);
    uint32_t psc_tmp = 0, arr_tmp = 0, sr_tmp = 0;
    const uint32_t timer_clk = 240000000;

    if (freq < 1200) {
        psc_tmp = 24 - 1;
        arr_tmp = 100 - 1;////////
        sr_tmp  = 100000;
    }
    else if (freq <= 110000) {
        psc_tmp = 24 - 1;
        arr_tmp = 10 - 1;
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
    HAL_TIM_GenerateEvent(cfg.htim, TIM_EVENTSOURCE_UPDATE);
    __HAL_TIM_SET_COUNTER(cfg.htim, 0);
}