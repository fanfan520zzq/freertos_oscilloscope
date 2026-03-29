/**
 * @file    adc_process.h
 * @brief   扫频测量链路 ADC 接收端接口
 *
 * 提供扫频场景下的 ADC 采集触发与完成通知机制。
 * spwm_sweep.c 通过 ADC_Sweep_Start_DMA() 发起单次采集,
 * 采集完成后通过 FreeRTOS TaskNotify 唤醒后台 DSP 处理任务。
 *
 * 注意: 本模块与示波器常规采集 (ADCTask) 共用 ADC1 硬件,
 * 运行时二者互斥, 由上层调度保证。
 */

#ifndef ADC_PROCESS_H
#define ADC_PROCESS_H

#include "main.h"

/* -----------------------------------------------------------------------
 * 数据规模
 * --------------------------------------------------------------------- */
#define ADC_SWEEP_SAMPLE_LENGTH  2048u

/* -----------------------------------------------------------------------
 * DMA 缓冲区 (D-Cache 32 字节对齐, 放置在 .dma_buffer 段)
 * --------------------------------------------------------------------- */
extern ALIGN_32BYTES(uint16_t adc_sweep_buffer[ADC_SWEEP_SAMPLE_LENGTH]);

/* -----------------------------------------------------------------------
 * 公共接口
 * --------------------------------------------------------------------- */

/**
 * @brief  初始化扫频 ADC 采集模块
 *
 * 注册 DSP 处理任务句柄, 以便采集完成中断中发送 TaskNotify。
 * 必须在 RTOS 调度器启动后、扫频开始前调用。
 *
 * @param  dsp_task_handle  后续 DSP 处理任务的 osThreadId_t,
 *                          传 NULL 表示暂不绑定 (后续可通过
 *                          ADC_Sweep_Set_DSP_Task 设置)。
 */
void ADC_Sweep_Init(osThreadId_t dsp_task_handle);

/**
 * @brief  绑定/更换 DSP 处理任务句柄
 * @param  dsp_task_handle  目标任务句柄
 */
void ADC_Sweep_Set_DSP_Task(osThreadId_t dsp_task_handle);

/**
 * @brief  启动一次 ADC DMA 采集 (ADC_SWEEP_SAMPLE_LENGTH 个点)
 *
 * 由 spwm_sweep.c 的 DMA 回调在"阶段 2"时调用。
 * 内部封装 HAL_ADC_Start_DMA, 隐藏 hadc1 句柄。
 *
 * @note   本函数会在中断上下文中被调用, 实现必须极简、非阻塞。
 */
void ADC_Sweep_Start_DMA(void);

/**
 * @brief  扫频 ADC 采集完成回调 (由 HAL_ADC_ConvCpltCallback 中调用)
 *
 * 判断当前采集是否为扫频模式触发, 若是则:
 *   1. 停止 ADC DMA
 *   2. 使 D-Cache 无效化 (Invalidate)
 *   3. 通过 vTaskNotifyGiveFromISR 唤醒 DSP 任务
 *
 * @param  hadc  HAL ADC 句柄指针
 * @retval 1     本次采集属于扫频模式, 已处理
 * @retval 0     本次采集不属于扫频模式, 调用者应继续处理
 */
uint8_t ADC_Sweep_ConvCpltHandler(ADC_HandleTypeDef *hadc);

#endif /* ADC_PROCESS_H */
