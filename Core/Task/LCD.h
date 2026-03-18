// LCD.h — 新版接口
#ifndef LCD_H
#define LCD_H

#include "main.h"
#include "usart.h"

#define LCD_WAVE_HEIGHT  255
#define LCD_WAVE_POINTS  480


#define LCD_COMP_WAVE 1
#define CH1 0
#define CH2 1


void LCD_Update_Stats(float ch1_freq, float ch1_vpp, uint8_t ch1_type,
                      float ch2_freq, float ch2_vpp, uint8_t ch2_type);


#endif