//
// Created by Lenovo on 2026/2/17.
//

#ifndef ITVM_DDS_ADC_H
#define ITVM_DDS_ADC_H
#include <main.h>


void IC_ON();
void CalcFx(void);


extern double Fx_CH1 , Fx_CH2;
extern uint8_t over_cnt;


#endif //ITVM_DDS_ADC_H