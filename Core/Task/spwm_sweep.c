//
// Created by Agent on 2026/03/25.
//

#include "spwm_sweep.h"
#include "tim.h" /* 包含 TIM8 对象 extern TIM_HandleTypeDef htim8; */
#include "adc_process.h"
#include <math.h>

extern DMA_HandleTypeDef hdma_tim8_up; // 声明外部定义的 DMA 句柄

// ========== DDS 和 DMA 结构定义 ==========
// 必须配置 D-Cache 一致性: 放置到 .dma_buffer 中，对齐到 32 字节
ALIGN_32BYTES(uint16_t spwm_dma_buf[SPWM_BUF_SIZE]) __attribute__((section(".dma_buffer")));

// 正弦映射表LUT
static uint16_t SineLUT[SINE_LUT_SIZE];

// 工作模式: 0=扫频, 1=定频
static volatile uint8_t spwm_mode = 0;

// 全局 DDS 状态参数
static volatile uint32_t Target_FTW = 0;
static volatile uint32_t Current_FTW = 0;
static uint32_t Target_Pulse_Count = 0;
static uint32_t current_pulse_counter = 0;

static uint32_t phase_acc = 0; // 相位累加器
static uint16_t sweep_index = 0;

// 扫频采样触发控制: sample_start_point = Target_Pulse_Count / 2
static uint32_t sample_start_point = 0;
static uint8_t  sample_triggered = 0; // 当前频点是否已触发 ADC

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
 * @brief 计算填充半区缓冲区 (1008 个采样点)
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
 * @brief 扫频模式三阶段状态机 (在 DMA HT/TC 回调中调用)
 *
 * 阶段 1: 滤波器稳定期 (pulse_counter < sample_start_point), 只发波
 * 阶段 2: 单次触发 ADC 采集 (pulse_counter 刚跨越 sample_start_point)
 * 阶段 3: 等待切频 (pulse_counter >= Target_Pulse_Count)
 */
static void Sweep_StateMachine(void)
{
    current_pulse_counter += SPWM_HALF_BUF_SIZE;

    /* 阶段 2: 单次 ADC 触发 (每频点仅一次) */
    if (!sample_triggered &&
        current_pulse_counter >= sample_start_point) {
        sample_triggered = 1;
        ADC_Sweep_Start_DMA();
    }

    /* 阶段 3: 驻留时间耗尽, 切到下一个频点 */
    if (current_pulse_counter >= Target_Pulse_Count) {
        current_pulse_counter = 0;
        sample_triggered = 0;
        sweep_index++;
        if (sweep_index >= SweepTable_Size) {
            sweep_index = 0;
        }
        Target_FTW = SweepTable[sweep_index].ftw;
        Target_Pulse_Count = SweepTable[sweep_index].target_pulse;
        sample_start_point = Target_Pulse_Count / 2;
    }
}

// 提前声明回调函数以避免编译错误
void SPWM_DMA_HalfCpltCallback(DMA_HandleTypeDef *hdma);
void SPWM_DMA_CpltCallback(DMA_HandleTypeDef *hdma);

/**
 * @brief 初始化 SPWM 和 Sweep 模型。应在 Task 或者主函数初始化时调用
 */
void SPWM_Sweep_Init(void)
{
    // 1. 初始化正弦LUT，范围映射到 TIM8 ARR_MAX (对应调制100kHz时的计数值)
    for (int i = 0; i < SINE_LUT_SIZE; i++) {
        // [0, SPWM_ARR_MAX] 中心化正弦
        float rad = (2.0f * 3.14159265f * i) / SINE_LUT_SIZE;
        float val = (sinf(rad) + 1.0f) / 2.0f;
        SineLUT[i] = (uint16_t)(val * SPWM_ARR_MAX);
    }

    // 2. 初始化初始工作状态 (默认扫频模式)
    spwm_mode = 0;
    sweep_index = 0;
    Target_FTW = SweepTable[sweep_index].ftw;
    Target_Pulse_Count = SweepTable[sweep_index].target_pulse;
    sample_start_point = Target_Pulse_Count / 2;
    sample_triggered = 0;
    Current_FTW = Target_FTW;
    current_pulse_counter = 0;
    phase_acc = 0;

    // 先用数据填充满完整 Buffer，以便首轮 DMA 启动正常
    Fill_Buffer_Half(&spwm_dma_buf[0]);
    Fill_Buffer_Half(&spwm_dma_buf[SPWM_HALF_BUF_SIZE]);

    // DCache一致性清理
    // SCB_CleanDCache_by_Addr((uint32_t*)spwm_dma_buf, sizeof(spwm_dma_buf));
}

/**
 * @brief 停止 TIM8 DMA 输出
 */
void SPWM_Sweep_Stop(void)
{
    HAL_TIM_PWM_Stop_DMA(&htim8, TIM_CHANNEL_1);
}

/**
 * @brief 启动指定频率输出 (10Hz-1kHz, 1Hz分辨率)
 * @param freq_hz 目标频率 (10-1000 Hz)
 *
 * 可在输出运行中调用以动态切频，新频率在下一个 DMA 半区回调中无缝生效。
 * 首次调用会启动 DMA 输出。
 */
void SPWM_Set_Frequency(uint16_t freq_hz)
{
    if (freq_hz < 10) freq_hz = 10;
    if (freq_hz > 1000) freq_hz = 1000;

    // 切换到定频模式
    spwm_mode = 1;
    Target_FTW = CALC_FTW(freq_hz);

    // 如果 DMA 尚未运行 (Current_FTW == 0 表示首次)，则完整启动
    if (Current_FTW == 0) {
        phase_acc = 0;
        Current_FTW = Target_FTW;

        Fill_Buffer_Half(&spwm_dma_buf[0]);
        Fill_Buffer_Half(&spwm_dma_buf[SPWM_HALF_BUF_SIZE]);

        HAL_TIM_PWM_Start(&htim8, TIM_CHANNEL_1);

        hdma_tim8_up.XferCpltCallback = SPWM_DMA_CpltCallback;
        hdma_tim8_up.XferHalfCpltCallback = SPWM_DMA_HalfCpltCallback;

        HAL_DMA_Start_IT(&hdma_tim8_up, (uint32_t)spwm_dma_buf, (uint32_t)&htim8.Instance->CCR1, SPWM_BUF_SIZE);
        __HAL_TIM_ENABLE_DMA(&htim8, TIM_DMA_UPDATE);
    }
    // 如果 DMA 已在运行，只更新 Target_FTW 即可，回调中会自动切换
}

/**
 * @brief 强制重启定频输出 (停止后重新启动 DMA)
 * @param freq_hz 目标频率 (10-1000 Hz)
 */
void SPWM_Set_Frequency_Restart(uint16_t freq_hz)
{
    if (freq_hz < 10) freq_hz = 10;
    if (freq_hz > 1000) freq_hz = 1000;

    HAL_TIM_PWM_Stop_DMA(&htim8, TIM_CHANNEL_1);

    spwm_mode = 1;
    phase_acc = 0;
    Target_FTW = CALC_FTW(freq_hz);
    Current_FTW = Target_FTW;
    Target_Pulse_Count = 0xFFFFFFFF;
    current_pulse_counter = 0;

    Fill_Buffer_Half(&spwm_dma_buf[0]);
    Fill_Buffer_Half(&spwm_dma_buf[SPWM_HALF_BUF_SIZE]);

    HAL_TIM_PWM_Start(&htim8, TIM_CHANNEL_1);

    hdma_tim8_up.XferCpltCallback = SPWM_DMA_CpltCallback;
    hdma_tim8_up.XferHalfCpltCallback = SPWM_DMA_HalfCpltCallback;

    HAL_DMA_Start_IT(&hdma_tim8_up, (uint32_t)spwm_dma_buf, (uint32_t)&htim8.Instance->CCR1, SPWM_BUF_SIZE);
    __HAL_TIM_ENABLE_DMA(&htim8, TIM_DMA_UPDATE);
}

// ========== DMA 中断回调 ==========

/**
 * @brief DMA 传输半完成回调 (HT)
 * 更新左半部分 (下标 0 ~ SPWM_HALF_BUF_SIZE-1) 的占空比数据
 */
void SPWM_DMA_HalfCpltCallback(DMA_HandleTypeDef *hdma)
{
    (void)hdma;

    // 无缝切换影子变量
    if (Current_FTW != Target_FTW) {
        Current_FTW = Target_FTW;
    }

    Fill_Buffer_Half(&spwm_dma_buf[0]);

    // SCB_CleanDCache_by_Addr((uint32_t*)&spwm_dma_buf[0], SPWM_HALF_BUF_SIZE * sizeof(uint16_t));

    if (spwm_mode == 0) {
        Sweep_StateMachine();
    }
}

/**
 * @brief DMA 传输完成回调 (TC)
 * 更新右半部分 (下标 SPWM_HALF_BUF_SIZE ~ SPWM_BUF_SIZE-1) 的占空比数据
 */
void SPWM_DMA_CpltCallback(DMA_HandleTypeDef *hdma)
{
    (void)hdma;

    if (Current_FTW != Target_FTW) {
        Current_FTW = Target_FTW;
    }

    Fill_Buffer_Half(&spwm_dma_buf[SPWM_HALF_BUF_SIZE]);

    // SCB_CleanDCache_by_Addr((uint32_t*)&spwm_dma_buf[SPWM_HALF_BUF_SIZE], SPWM_HALF_BUF_SIZE * sizeof(uint16_t));

    if (spwm_mode == 0) {
        Sweep_StateMachine();
    }
}

/**
 * @brief 启动扫频和 TIM8 DMA
 */
void SPWM_Sweep_Start(void)
{
    spwm_mode = 0; // 扫频模式
    sample_triggered = 0;
    sample_start_point = SweepTable[sweep_index].target_pulse / 2;

    HAL_TIM_PWM_Start(&htim8, TIM_CHANNEL_1);

    hdma_tim8_up.XferCpltCallback = SPWM_DMA_CpltCallback;
    hdma_tim8_up.XferHalfCpltCallback = SPWM_DMA_HalfCpltCallback;

    HAL_DMA_Start_IT(&hdma_tim8_up, (uint32_t)spwm_dma_buf, (uint32_t)&htim8.Instance->CCR1, SPWM_BUF_SIZE);
    __HAL_TIM_ENABLE_DMA(&htim8, TIM_DMA_UPDATE);
}
