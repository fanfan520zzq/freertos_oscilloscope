#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define BUFFER_SIZE 500
#define PERIOD 80
#define CH1 1
#define CH2 2

// 预计算一个周期的正弦表 (0-255归一化幅度)，节省运行时计算量
static const uint8_t SinTable[PERIOD] = {
    128,138,148,158,167,177,186,194,203,211,218,225,232,237,242,246,249,252,254,255,
    255,254,252,249,246,242,237,232,225,218,211,203,194,186,177,167,158,148,138,128,
    118,108,98,89,79,70,62,53,45,38,31,24,19,14,10,7,4,2,1,0,
    0,1,2,4,7,10,14,19,24,31,38,45,53,62,70,79,89,98,108,118
};
void LCD_Output(char* Type, uint16_t Amplitude, uint8_t CH, float freq) {
    uint8_t wave_buffer[BUFFER_SIZE];

    // 计算偏移量缩放：0-65535 映射到 0-160
    uint32_t scale_160 = (uint32_t)(Amplitude * 160) >> 16;
    uint8_t top_limit = 32; // 顶值基准

    for (int i = 0; i < BUFFER_SIZE; i++) {
        uint8_t phase = i % PERIOD;
        uint32_t temp_offset = 0;

        if (Type[0] == 's' && Type[1] == 'i') {      // Sine
            // 将 SinTable(0-255) 映射到 0-scale_160
            temp_offset = (SinTable[phase] * scale_160) >> 8;
        }
        else if (Type[0] == 's') {                   // Square
            temp_offset = (phase < 40) ? 0 : scale_160; // 这里的0或scale_160取决于你希望高电平在上还是在下
        }
        else if (Type[0] == 't') {                   // Triangle
            uint8_t tri = (phase < 40) ? phase : (80 - phase); // 0~40
            temp_offset = (tri * scale_160) / 40;
        }
        else if (Type[0] == 'c') {                   // Current
            temp_offset = 96; // 32 + 96 = 128 (固定输出128)
        }

        // 核心修改：从顶值 32 开始向下加
        // 结果范围：32 + (0~160) = 32 ~ 192
        wave_buffer[i] = top_limit + (uint8_t)temp_offset;
    }

    // --- 以下发送逻辑 ---
    float vpp = (Amplitude * 3.3f) / 65535.0f;
    int t_type = (CH == CH1) ? 4 : 7;
    int t_freq = (CH == CH1) ? 2 : 5;
    int t_vpp  = (CH == CH1) ? 3 : 6;
    int s_id   = (CH == CH1) ? 0 : 1;

    const char* name = "unknown";
    if (Type[0] == 's' && Type[1] == 'i') name = "sine";
    else if (Type[0] == 's') name = "square";
    else if (Type[0] == 't') name = "triangle";
    else if (Type[0] == 'c') name = "current";

    printf("t%d.txt=\"%s\"\xff\xff\xff", t_type, name);
    printf("t%d.txt=\"%.3f\"\xff\xff\xff", t_freq, freq);
    printf("t%d.txt=\"%.3f\"\xff\xff\xff", t_vpp, vpp);

    for (int i = 0; i < BUFFER_SIZE; i++) {
        // 注意：如果波形看起来是倒过来的（例如电平越高点越靠下），
        // 那是因为屏幕坐标系的 Y0 在顶端。
        printf("add 1,%d,%d\xff\xff\xff", s_id, wave_buffer[i]);
    }
}