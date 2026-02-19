//
// Created by Lenovo on 2026/2/11.
//
#include "cmsis_os2.h"
#include <MSG.h>
#include <FreeRTOS.h>


#include "usart.h"



void StartUARTTask(void *argument) {
    UART1_Receive_Start();
    uint8_t receive;
    uint8_t command[10] = {0};
    uint8_t cmdIndex = 0;
    uint8_t cmdLen = 0;
    for (;;) {
        osMessageQueueGet(UARTQueueHandle,&receive,0,osWaitForever);
        if (cmdIndex == 0) {
            if (receive == 0xAA)  command[cmdIndex++] = receive;
        }else if (cmdIndex == 1) {
            if (receive<2||receive > sizeof(command)) {
                cmdIndex = 0;
                continue;
            }
            cmdLen = receive;
            command[cmdIndex++] = receive;
        }else {
            command[cmdIndex++] = receive;
            if (cmdIndex == cmdLen) {
                uint16_t checksum = 0;
                uint16_t Evpp=command[4]|(command[5]<<8),
                         Efreq=command[3],
                         Examine=(command[8]<<8) | command[7],
                         Ewave=command[6];
                uint8_t Eop = command[2];
                checksum += command[0]+command[1]+Eop+Evpp+Efreq+Ewave;
                if (checksum == Examine ) {
                    APP_Text* msg = pvPortMalloc(sizeof(APP_Text));
                    msg->op = Eop;
                    msg->Freq = Efreq;
                    msg->VPP = Evpp;
                    msg->WaveType = command[6];
                    osMessageQueuePut(MSGQueueHandle,&msg,0,osWaitForever);
                    //osDelay(2);
                }
                cmdIndex = 0;
                cmdLen = 0;

            }

        }
    }
}


//TEST :AA 09 D2 32 E8 03 02 A1 05