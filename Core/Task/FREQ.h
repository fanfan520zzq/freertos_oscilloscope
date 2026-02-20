//
// Created by Lenovo on 2026/2/17.
//

#ifndef ITVM_DDS_ADC_H
#define ITVM_DDS_ADC_H
#include <main.h>


void IC_ON();
void CalcFx(void);


extern uint8_t State_CH1,State_CH2;
extern uint32_t TIM2_Over_Cnt,TIM3_Over_Cnt;
extern double Fx_CH1 , Fx_CH2;


#endif //ITVM_DDS_ADC_H