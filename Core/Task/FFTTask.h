//
// Created by Lenovo on 2026/2/22.
//

#ifndef IIT6_OSCILLISCOPE_FFTTASK_H
#define IIT6_OSCILLISCOPE_FFTTASK_H


#include "ADCTask.h"
#include "MSG.h"

#include "LCD.h"






uint32_t Find_Rising_Edge(const float *buf, uint32_t len,
                           float v_mid, float vpp);

#endif //IIT6_OSCILLISCOPE_FFTTASK_H