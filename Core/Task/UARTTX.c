// UARTTX.c
#include "cmsis_os2.h"
#include "FreeRTOS.h"
#include "MSG.h"
#include "usart.h"
#include "protocol.h"



void StartUARTTask(void *argument) {
    UART1_Receive_Start();

    uint8_t test[] = {0xAA, 0x0A, 0xAE, 0x00, 0x00, 0x00, 0x00, 0x00};
    uint16_t crc = Proto_CRC16(test, 8);


    uint8_t raw[PROTO_LEN];
    uint8_t byte;
    uint8_t idx = 0;
    for (;;) {
         if (osMessageQueueGet(UARTQueueHandle, &byte, 0, osWaitForever) != osOK)
             continue;

         // 未同步时只等帧头
         if (idx == 0 && byte != PROTO_HEADER)
             continue;

         raw[idx++] = byte;

         // 第二字节收到后立即验长度，不对就重新同步
         if (idx == 2 && byte != PROTO_LEN) {
             idx = 0;
             continue;
         }

         // 帧未收完
         if (idx < PROTO_LEN)
             continue;

         // 帧收齐，重置索引
         idx = 0;

         // 解包
         ProtoFrame frame;
         if (!Proto_Decode(raw, &frame))
             continue;   // CRC错或格式错，丢帧

         // 业务范围校验
         if (!Proto_Validate(&frame))
             continue;   // 参数越界，丢帧

         // 分配消息并入队
         APP_Text *msg = pvPortMalloc(sizeof(APP_Text));
         if (msg == NULL)
             continue;   // heap耗尽保护

         msg->op       = frame.op;
         msg->Freq     = frame.freq_hz;
         msg->VPP      = frame.vpp_mv;
         msg->WaveType = frame.wave_type;

         if (osMessageQueuePut(MSGQueueHandle, &msg, 0, 10) != osOK)
             vPortFree(msg);   // 入队失败立即释放，不能泄漏
    }
}



//
// void StartUARTTask(void *argument) {
//     UART1_Receive_Start();
//
//     uint8_t raw[PROTO_LEN];
//     uint8_t byte;
//     uint8_t idx = 0;
//
//     for (;;) {
//         if (osMessageQueueGet(UARTQueueHandle, &byte, 0, osWaitForever) != osOK)
//             continue;
//
//         // 未同步时只等帧头
//         if (idx == 0 && byte != PROTO_HEADER)
//             continue;
//
//         raw[idx++] = byte;
//
//         // 第二字节收到后立即验长度，不对就重新同步
//         if (idx == 2 && byte != PROTO_LEN) {
//             idx = 0;
//             continue;
//         }
//
//         // 帧未收完
//         if (idx < PROTO_LEN)
//             continue;
//
//         // 帧收齐，重置索引
//         idx = 0;
//
//         // 解包
//         ProtoFrame frame;
//         if (!Proto_Decode(raw, &frame))
//             continue;   // CRC错或格式错，丢帧
//
//         // 业务范围校验
//         if (!Proto_Validate(&frame))
//             continue;   // 参数越界，丢帧
//
//         // 分配消息并入队
//         APP_Text *msg = pvPortMalloc(sizeof(APP_Text));
//         if (msg == NULL)
//             continue;   // heap耗尽保护
//
//         msg->op       = frame.op;
//         msg->Freq     = frame.freq_hz;
//         msg->VPP      = frame.vpp_mv;
//         msg->WaveType = frame.wave_type;
//
//         if (osMessageQueuePut(MSGQueueHandle, &msg, 0, 10) != osOK)
//             vPortFree(msg);   // 入队失败立即释放，不能泄漏
//     }
// }