//
// Created by Lenovo on 2026/2/20.
//

#ifndef IIT6_OSCILLISCOPE_ADC_H
#define IIT6_OSCILLISCOPE_ADC_H

#include "FREQ.h"
#include "main.h"

#define CH1 1
#define CH2 2
#define LEN 1024

extern float CH1_DATA[LEN], CH2_DATA[LEN];
extern uint32_t Sample_Rate_CH1, Sample_Rate_CH2;

#endif //IIT6_OSCILLISCOPE_ADC_H