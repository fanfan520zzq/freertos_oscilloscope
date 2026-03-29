/**
 * @file    adc_process.c
 * @brief   扫频测量链路 ADC 接收端实现
 */

#include "adc_process.h"
#include "adc.h"
#include "tim.h"
#include "FreeRTOS.h"
#include "task.h"

/* -----------------------------------------------------------------------
 * DMA 缓冲区定义
 * --------------------------------------------------------------------- */
ALIGN_32BYTES(uint16_t adc_sweep_buffer[ADC_SWEEP_SAMPLE_LENGTH])
    __attribute__((section(".dma_buffer")));

/* -----------------------------------------------------------------------
 * 私有状态
 * --------------------------------------------------------------------- */

/* 扫频采集活跃标志: 1 = ADC_Sweep_Start_DMA 已触发, 等待完成 */
static volatile uint8_t s_sweep_active = 0;

/* 需要被通知的 DSP 任务句柄 (原生 FreeRTOS TaskHandle_t) */
static TaskHandle_t s_dsp_task = NULL;

/* -----------------------------------------------------------------------
 * 公共接口实现
 * --------------------------------------------------------------------- */

void ADC_Sweep_Init(osThreadId_t dsp_task_handle)
{
    s_sweep_active = 0;
    s_dsp_task = (TaskHandle_t)dsp_task_handle;
}

void ADC_Sweep_Set_DSP_Task(osThreadId_t dsp_task_handle)
{
    s_dsp_task = (TaskHandle_t)dsp_task_handle;
}

void ADC_Sweep_Start_DMA(void)
{
    s_sweep_active = 1;

    /* 启动 TIM3 作为 ADC1 的外部触发源 */
    HAL_TIM_Base_Start(&htim3);

    HAL_ADC_Start_DMA(&hadc1,
                       (uint32_t *)adc_sweep_buffer,
                       ADC_SWEEP_SAMPLE_LENGTH);
}

uint8_t ADC_Sweep_ConvCpltHandler(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance != ADC1 || !s_sweep_active)
        return 0;

    s_sweep_active = 0;

    HAL_ADC_Stop_DMA(&hadc1);

    /* D-Cache 无效化, 确保 CPU 读到 DMA 搬入的最新数据 */
    SCB_InvalidateDCache_by_Addr((uint32_t *)adc_sweep_buffer,
                                 sizeof(adc_sweep_buffer));

    /* 唤醒 DSP 处理任务 */
    if (s_dsp_task != NULL) {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        vTaskNotifyGiveFromISR(s_dsp_task, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }

    return 1;
}
