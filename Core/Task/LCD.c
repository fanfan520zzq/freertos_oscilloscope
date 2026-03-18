#include "LCD.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#define TX_BUF_SIZE  480
static uint8_t tx_buf_a[TX_BUF_SIZE];
static uint8_t tx_buf_b[TX_BUF_SIZE];
static uint8_t *tx_front = tx_buf_a;   // 正在发送
static uint8_t *tx_back  = tx_buf_b;   // 正在填充

static inline void lcd_wait_dma(void) {
    // 简单轮询；电赛中可改为 osSemaphoreAcquire(lcd_dma_sem, 10)
    while (HAL_UART_GetState(&huart1) == HAL_UART_STATE_BUSY_TX) {
        osDelay(1);
    }
}


static void lcd_send_raw(const uint8_t *data, uint16_t len) {
    lcd_wait_dma();
    memcpy(tx_front, data, len);
    HAL_UART_Transmit_DMA(&huart1, tx_front, len);
    // 交换缓冲（下次填充另一块，不影响本次 DMA）
    uint8_t *tmp = tx_front;
    tx_front = tx_back;
    tx_back = tmp;
}


static void lcd_cmd(const char *fmt, ...) {
    static uint8_t cmd_buf[128];
    va_list args;
    va_start(args, fmt);
    int n = vsnprintf((char*)cmd_buf, sizeof(cmd_buf) - 3, fmt, args);
    va_end(args);
    cmd_buf[n]   = 0xFF;
    cmd_buf[n+1] = 0xFF;
    cmd_buf[n+2] = 0xFF;
    lcd_send_raw(cmd_buf, n + 3);
}


void LCD_Update_Stats(float ch1_freq, float ch1_vpp, uint8_t ch1_type,
                      float ch2_freq, float ch2_vpp, uint8_t ch2_type) {
    // 频率用 0.1Hz 精度存成整数，避免浮点格式化
    lcd_cmd("va0.val=%d", (int)(ch1_freq * 10));
    lcd_cmd("va1.val=%d", (int)(ch1_vpp * 1000));   // mV
    lcd_cmd("va2.val=%d", ch1_type);
    lcd_cmd("va3.val=%d", (int)(ch2_freq * 10));
    lcd_cmd("va4.val=%d", (int)(ch2_vpp * 1000));
    lcd_cmd("va5.val=%d", ch2_type);
}

void LCD_Send_Waveform(uint8_t ch, const float *samples, uint32_t in_len,
                       float vpp_v, float dummy) {
    (void)dummy;

    // 1. 降采样到 LCD_WAVE_POINTS 个点
    static uint8_t wave[LCD_WAVE_POINTS];
    float step = (float)in_len / LCD_WAVE_POINTS;
    float half = (float)(LCD_WAVE_HEIGHT / 2);

    for (int i = 0; i < LCD_WAVE_POINTS; i++) {
        uint32_t idx = (uint32_t)(i * step);
        if (idx >= in_len) idx = in_len - 1;

        // 归一化：samples 单位是 V（已减均值），vpp_v 是峰峰
        float norm = (vpp_v > 0.001f) ? (samples[idx] / (vpp_v * 0.5f)) : 0.0f;
        // 限幅 [-1, 1] → 映射到 [0, LCD_WAVE_HEIGHT]
        norm = (norm > 1.0f) ? 1.0f : (norm < -1.0f ? -1.0f : norm);
        // 屏幕 Y0 在顶部，波形向下为正：用 (half - norm*half)
        wave[i] = (uint8_t)(half - norm * (half - 2.0f));
    }

    // 2. 发 addt 头部
    lcd_cmd("addt %d,%d,%d", LCD_COMP_WAVE, ch, LCD_WAVE_POINTS);

    // 3. 发原始字节（无 \xFF 分隔符）
    lcd_wait_dma();
    memcpy(tx_front, wave, LCD_WAVE_POINTS);
    HAL_UART_Transmit_DMA(&huart1, tx_front, LCD_WAVE_POINTS);
    uint8_t *tmp = tx_front; tx_front = tx_back; tx_back = tmp;
}

// ─── 文本更新 ─────────────────────────────────────────────────────
void LCD_Send_Text_Str(uint8_t comp_id, const char *str) {
    lcd_cmd("t%d.txt=\"%s\"", comp_id, str);
}



