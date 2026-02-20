//
// Created by Lenovo on 2026/2/14.
//

#include "DDS.h"
#include "MSG.h"
#include "FREQ.h"





void StartCMDTask(void *argument) {   //PA4 PA5
    APP_Text* MSG;
    DDS_Init();  //初始化ROM

    HAL_TIM_PWM_Start(&htim14, TIM_CHANNEL_1);


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
                    osSemaphoreRelease(FreqSEMHandle);
                    break;
                }
                case ADC_OFF: {
                    int k=1;
                    break;
                }
                    //default: DDS_Stop();
            }
        }
    }
}
