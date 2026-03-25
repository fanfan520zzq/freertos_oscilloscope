//
// Created by Agent on 2026/03/25.
//

#include "spwm_sweep.h"
#include "tim.h" /* 包含 TIM1 对象 extern TIM_HandleTypeDef htim1; */
#include <math.h>

// ========== DDS 和 DMA 结构定义 ==========
// 必须配置 D-Cache 一致性: 放置到 .dma_buffer 中，对齐到 32 字节
ALIGN_32BYTES(uint16_t spwm_dma_buf[SPWM_BUF_SIZE]) __attribute__((section(".dma_buffer")));

// 正弦映射表LUT
static uint16_t SineLUT[SINE_LUT_SIZE];

// 全局 DDS 状态参数
static uint32_t Target_FTW = 0;
static uint32_t Current_FTW = 0;
static uint32_t Target_Pulse_Count = 0;
static uint32_t current_pulse_counter = 0;

static uint32_t phase_acc = 0; // 相位累加器
static uint16_t sweep_index = 0;

// ========== 宏计算 ==========
// FTW = f_out * (2^32 / f_clk)
// f_clk = 100,000 Hz (载波更新速率)
#define CALC_FTW(f) (uint32_t)(((float)(f) * 4294967296.0f) / 100000.0f)

// 扫频表配置，(freq, pulse_count)
// 数据基于 doc/sweep_for_gemini.md 中的列表
static SPWM_SweepPoint_t SweepTable[] = {
    {10,  CALC_FTW(10),  20000}, {13,  CALC_FTW(13),  20000},
    {16,  CALC_FTW(16),  20000}, {20,  CALC_FTW(20),  20000},
    {26,  CALC_FTW(26),  20000}, {33,  CALC_FTW(33),  20000},
    {42,  CALC_FTW(42),  10000}, {53,  CALC_FTW(53),  10000},
    {68,  CALC_FTW(68),  10000}, {80,  CALC_FTW(80),  10000},
    {82,  CALC_FTW(82),  10000}, {84,  CALC_FTW(84),  10000},
    {86,  CALC_FTW(86),  10000}, {88,  CALC_FTW(88),  10000},
    {90,  CALC_FTW(90),  10000}, {92,  CALC_FTW(92),  10000},
    {94,  CALC_FTW(94),  10000}, {96,  CALC_FTW(96),  10000},
    {98,  CALC_FTW(98),  10000}, {100, CALC_FTW(100), 10000},
    {102, CALC_FTW(102), 10000}, {104, CALC_FTW(104), 10000},
    {106, CALC_FTW(106), 10000}, {108, CALC_FTW(108), 10000},
    {110, CALC_FTW(110), 10000}, {112, CALC_FTW(112), 10000},
    {114, CALC_FTW(114), 10000}, {116, CALC_FTW(116), 10000},
    {118, CALC_FTW(118), 10000}, {120, CALC_FTW(120), 10000},
    {125, CALC_FTW(125),  4000}, {160, CALC_FTW(160),  4000},
    {204, CALC_FTW(204),  4000}, {260, CALC_FTW(260),  4000},
    {332, CALC_FTW(332),  4000}, {424, CALC_FTW(424),  4000},
    {541, CALC_FTW(541),  4000}, {691, CALC_FTW(691),  4000},
    {882, CALC_FTW(882),  4000}, {1000, CALC_FTW(1000), 4000}
};

static const uint16_t SweepTable_Size = sizeof(SweepTable) / sizeof(SweepTable[0]);


// ========== 函数实现 ==========

/**
 * @brief 计算填充半区缓冲区 (1000 个采样点)
 */
static void Fill_Buffer_Half(uint16_t *buf_ptr)
{
    for (uint16_t i = 0; i < SPWM_HALF_BUF_SIZE; i++) {
        phase_acc += Current_FTW;
        // 取高位(32 - 10 = 22)做 1024 映射表索引
        uint32_t lut_index = phase_acc >> 22;
        buf_ptr[i] = SineLUT[lut_index];
    }
}

/**
 * @brief 初始化 SPWM 和 Sweep 模型。应在 Task 或者主函数初始化时调用
 */
void SPWM_Sweep_Init(void)
{
    // 1. 初始化正弦LUT，范围映射到 TIM1 ARR_MAX (对应调制100kHz时的计数值)
    for (int i = 0; i < SINE_LUT_SIZE; i++) {
        // [0, SPWM_ARR_MAX] 中心化正弦
        float rad = (2.0f * 3.14159265f * i) / SINE_LUT_SIZE;
        float val = (sinf(rad) + 1.0f) / 2.0f;
        SineLUT[i] = (uint16_t)(val * SPWM_ARR_MAX);
    }

    // 2. 初始化初始工作状态
    sweep_index = 0;
    Target_FTW = SweepTable[sweep_index].ftw;
    Target_Pulse_Count = SweepTable[sweep_index].target_pulse;
    Current_FTW = Target_FTW;
    current_pulse_counter = 0;
    phase_acc = 0;

    // 先用数据填充满 2000 个点的完整 Buffer，以便首轮 DMA 启动正常
    Fill_Buffer_Half(&spwm_dma_buf[0]);
    Fill_Buffer_Half(&spwm_dma_buf[SPWM_HALF_BUF_SIZE]);

    // DCache一致性清理
    SCB_CleanDCache_by_Addr((uint32_t*)spwm_dma_buf, sizeof(spwm_dma_buf));
}

/**
 * @brief 启动扫频和 TIM1 DMA
 */
void SPWM_Sweep_Start(void)
{
    // 调用 HAL 启动包含 DMA 的 PWM 生成（请匹配你的 TIM1 使用通道，例如 Channel 1）
    HAL_TIM_PWM_Start_DMA(&htim1, TIM_CHANNEL_1, (uint32_t*)spwm_dma_buf, SPWM_BUF_SIZE);
}

/**
 * @brief 停止 TIM1 DMA 输出
 */
void SPWM_Sweep_Stop(void)
{
    HAL_TIM_PWM_Stop_DMA(&htim1, TIM_CHANNEL_1);
}

// ========== DMA 中断回调 ==========

/**
 * @brief DMA 传输半完成回调 (HT)
 * 更新左半部分 (下标 0 ~ 999) 的占空比数据
 */
void HAL_TIM_PWM_PulseFinishedHalfCpltCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM1)
    {
        // 1. 无缝切换影子变量
        if (Current_FTW != Target_FTW) {
            Current_FTW = Target_FTW;
        }

        // 2. 计算并填充前半区 Buffer
        Fill_Buffer_Half(&spwm_dma_buf[0]);

        // 3. Cache 一致性: 必须清理该一半的缓存以便 DMA 搬走
        // 注: SPWM_HALF_BUF_SIZE * sizeof(uint16_t) 即 1000 * 2 = 2000 字节
        SCB_CleanDCache_by_Addr((uint32_t*)&spwm_dma_buf[0], SPWM_HALF_BUF_SIZE * sizeof(uint16_t));

        // 4. 驻留机制处理
        current_pulse_counter += SPWM_HALF_BUF_SIZE;
        if (current_pulse_counter >= Target_Pulse_Count) {
            current_pulse_counter = 0; // 重置计数
            sweep_index++;             // 下一个频点
            if (sweep_index >= SweepTable_Size) {
                sweep_index = 0;       // 循环扫频
            }
            // 更新 Target
            Target_FTW = SweepTable[sweep_index].ftw;
            Target_Pulse_Count = SweepTable[sweep_index].target_pulse;
        }
    }
}

/**
 * @brief DMA 传输完成回调 (TC)
 * 更新右半部分 (下标 1000 ~ 1999) 的占空比数据
 */
void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM1)
    {
        // 1. 无缝切换影子变量
        if (Current_FTW != Target_FTW) {
            Current_FTW = Target_FTW;
        }

        // 2. 计算并填充后半区 Buffer
        Fill_Buffer_Half(&spwm_dma_buf[SPWM_HALF_BUF_SIZE]);

        // 3. Cache 一致性 (注意缓存块地址偏移)
        SCB_CleanDCache_by_Addr((uint32_t*)&spwm_dma_buf[SPWM_HALF_BUF_SIZE], SPWM_HALF_BUF_SIZE * sizeof(uint16_t));

        // 4. 驻留机制处理
        current_pulse_counter += SPWM_HALF_BUF_SIZE;
        if (current_pulse_counter >= Target_Pulse_Count) {
            current_pulse_counter = 0;
            sweep_index++;
            if (sweep_index >= SweepTable_Size) {
                sweep_index = 0;
            }
            Target_FTW = SweepTable[sweep_index].ftw;
            Target_Pulse_Count = SweepTable[sweep_index].target_pulse;
        }
    }
}
