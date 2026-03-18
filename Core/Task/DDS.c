//
// Created by Lenovo on 2026/2/17.
//

#include "DDS.h"


static uint16_t SinBuffer[1024],SquBuffer[1024],TriBuffer[1024];
uint16_t Buffer2[1024] __attribute__((section(".dma_buffer")));
uint16_t Buffer1[1024] __attribute__((section(".dma_buffer")));

static uint32_t phase_index1=0,FTW1;
static uint32_t phase_index2=0,FTW2;
static uint16_t Buffer1_Len,Buffer2_Len;

void DDS_Init(void)
{
    const uint16_t MAX_VAL = 1309;    // 1V 对应的数字量
    const float MID_VAL = 654.5f;     // 中点值 (1241 / 2)
    for(uint16_t i = 0; i < 1024; i++)
    {
        SinBuffer[i] = (uint16_t)(MID_VAL + MID_VAL * sinf((2.0f * 3.1415926f * i) / 1024.0f));
        SquBuffer[i] = (i < 512) ? MAX_VAL : 0;
        if(i <= 512) TriBuffer[i] = (uint16_t)((i * (float)MAX_VAL) / 512.0f);
        else  TriBuffer[i] = (uint16_t)(((1024 - i) * (float)MAX_VAL) / 512.0f);
    }

    HAL_DAC_Stop_DMA(&hdac1, DAC_CHANNEL_1);
    HAL_DAC_Stop_DMA(&hdac1, DAC_CHANNEL_2);
    HAL_TIM_Base_Start(&htim6);
    HAL_TIM_Base_Start(&htim7);
}


void DDS1_Update_DATA(uint16_t freq,uint16_t vpp,uint8_t waveType){//vpp 0-1000
    HAL_DAC_Stop_DMA(&hdac1, DAC_CHANNEL_1);

    phase_index1=0;
    freq=freq*1000;
    FTW1=(uint32_t)(freq)*(4294967296.0f)/DDS_TIM; //2^32=4294967296
    Buffer1_Len=1000000/freq;
    for(int i=0;i<Buffer1_Len;i++){
        switch(waveType){
            case 0:
                Buffer1[i]=(uint16_t)(SinBuffer[phase_index1>>22]*(vpp)/1000.0f);
                break;
            case 1:
                Buffer1[i]=(uint16_t)(SquBuffer[phase_index1>>22]*(vpp)/1000.0f);
                break;
            case 2:
                Buffer1[i]=(uint16_t)(TriBuffer[phase_index1>>22]*(vpp)/1000.0f);
                break;
            default:
                Buffer1[i]=0;
                break;
        }
        phase_index1+=FTW1;
    }

    SCB_CleanDCache_by_Addr((uint32_t*)Buffer1, (uint32_t)Buffer1_Len);
    HAL_DAC_Start_DMA(&hdac1, DAC_CHANNEL_1, (uint32_t*)Buffer1, Buffer1_Len, DAC_ALIGN_12B_R);
}

void DDS2_Update_DATA(uint16_t freq,uint16_t vpp,uint8_t waveType){//vpp 0-1000
    HAL_DAC_Stop_DMA(&hdac1, DAC_CHANNEL_2);

    phase_index2=0;
    freq=freq*1000;
    FTW2=(uint32_t)(freq)*(4294967296.0f)/DDS_TIM; //2^32=4294967296
    Buffer2_Len=1000000/freq;

    for(int i=0;i<Buffer2_Len;i++){
        switch(waveType){
            case 0:
                Buffer2[i]=(uint16_t)(SinBuffer[phase_index2>>22]*(vpp)/1000.0f);
                break;
            case 1:
                Buffer2[i]=(uint16_t)(SquBuffer[phase_index2>>22]*(vpp)/1000.0f);
                break;
            case 2:
                Buffer2[i]=(uint16_t)(TriBuffer[phase_index2>>22]*(vpp)/1000.0f);
                break;
            default:
                Buffer2[i]=0;
                break;
        }
        phase_index2+=FTW2;


    }
    SCB_CleanDCache_by_Addr((uint32_t*)Buffer2, (uint32_t)Buffer2_Len-1);
    HAL_DAC_Start_DMA(&hdac1, DAC_CHANNEL_2, (uint32_t*)Buffer2, Buffer2_Len, DAC_ALIGN_12B_R);

}
