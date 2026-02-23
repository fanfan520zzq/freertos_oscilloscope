//
// Created by Lenovo on 2026/2/22.
//

#include "LCD.h"

#include <stdio.h>

#include "usart.h"


int __io_putchar(int ch) {
    HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
    return ch;
}


void StartLCDTask(void *argument) {


    setvbuf(stdout, NULL, _IONBF, 0);
    while (1) {
        osSemaphoreAcquire(LCDSEMHandle,osWaitForever);

        if (g_is_adc_continuous==1) {
            fflush(stdout);
            printf("t2.txt=\"%.*s\"\xff\xff\xff",9,CH1_FREQ);
            fflush(stdout);
            printf("t5.txt=\"%.*s\"\xff\xff\xff",9,CH2_FREQ);
            fflush(stdout);
            printf("t3.txt=\"%.*s\"\xff\xff\xff",4,CH1_VPP);
            fflush(stdout);
            printf("t6.txt=\"%.*s\"\xff\xff\xff",4,CH2_VPP);
            fflush(stdout);
            printf("t4.txt=\"%.*s\"\xff\xff\xff",7,CH1_TYPE);
            fflush(stdout);
            printf("t7.txt=\"%.*s\"\xff\xff\xff",7,CH2_TYPE);
            fflush(stdout);
            for (int i=0;i<LEN;i++) {
                printf("add 1,0,%d\xff\xff\xff",CH1_LCD[i]);
                printf("add 1,1,%d\xff\xff\xff",CH2_LCD[i]);
            }
            fflush(stdout);

            osDelay(5000);

            // 链路闭环：再次释放第一个信号量，开启新一轮采样过程
            osSemaphoreRelease(FreqSEMHandle);
        }
        int k=1;

    }
}
