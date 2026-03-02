//
// Created by Lenovo on 2026/2/14.
//

#include <stdbool.h>

#include "DDS.h"
#include "MSG.h"
#include "FREQ.h"



uint8_t g_is_adc_continuous = 1;


void StartCMDTask(void *argument) {   //PA4 PA5
    APP_Text* MSG;
    DDS_Init();  //初始化ROM
    g_is_adc_continuous = 0;

    HAL_TIM_PWM_Start(&htim12, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim13, TIM_CHANNEL_1);

    while (1) {
        if (osMessageQueueGet(MSGQueueHandle,&MSG,0,osWaitForever)==osOK) {
            switch (MSG->op) {
                case DAC1_UPDATE:{
                    DDS1_Update_DATA(MSG->Freq,MSG->VPP,MSG->WaveType);  break;
                }
                case DAC2_UPDATE: {
                    DDS2_Update_DATA(MSG->Freq,MSG->VPP,MSG->WaveType);  break;
                }
                case DAC1_RELEASE: {
                    HAL_DAC_Stop_DMA(&hdac1, DAC_CHANNEL_1);
                    break;
                }
                case DAC2_RELEASE: {
                    HAL_DAC_Stop_DMA(&hdac1, DAC_CHANNEL_2);
                    break;
                }
                case ADC_ON: {
                    if (g_is_adc_continuous != 1) {
                        g_is_adc_continuous=1;
                        osSemaphoreRelease(FreqSEMHandle);
                    }

                    break;
                }
                case ADC_OFF: {
                    g_is_adc_continuous = 0;
                    break;
                }
                    //default: DDS_Stop();
            }
        }
    }
}
