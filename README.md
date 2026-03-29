




## 统一帧格式


|0xAA|0x0A|OP|FREQ|VPP|WAVE|CRC16|
|---|---|---|---|---|---|---|
|1byte|1byte|1byte|2bytes|2bytes|1byte|2bytes|

## 操作码

    typedef enum {
        ADC_ON = 0xAD,
        ADC_OFF = 0xAE,
        DAC1_UPDATE = 0xD1,
        DAC2_UPDATE = 0xD2,
        DAC1_RELEASE = 0xDA,
        DAC2_RELEASE = 0xDB,
    }OP;



## 波形

    1-DC
    2-SINE
    3-SQUARE
    4-TRIANGLE





## LCD-RX
只有adc情况下会向lcd发信，直接发命令


## UPDATE:统一消息结构体+共用体
在op段里面已经写好要操作哪个通道了，传输参数即可


    typedef enum {
        ADC_ON,
        ADC_OFF,
        DAC1_UPDATE,
        DAC2_UPDATE,
    }OP;

    typedef struct {
        OP op;
        uint16_t VPP;          
        uint8_t WaveType;      
        uint16_t Freq;         
    }


## 等精度测量法总结

测频率：固定闸门时间数上升沿 或者 固定期望上升沿数数时间


等精度测量法：预设闸门时间,上升沿触发开始闸门，整数周期个上升沿后停止

 `误差：从脉冲数误差变为时间维度的无法对齐结束时间的良性误差`


#### 具体实现

双通道测频：一个闸门定时器，两个对信号进行计数的定时器，一个基准定时器


配置：闸门定时器设置TRGO enable(CNT_EN);剩下三个计时器设为 gate mode，ITR0，统一由闸门定时器`同时`管理


## ADC测量

预计分三档：

||low|medium|high|
|---|---|---|---|
|信号频率|1khz-12khz|12khz-110khz|110khz-1mhz
|采样频率|100khz|1mhz|等效采样|
|硬件配置|psc:23 arr:99|psc:23 arr:9|公式|
|期望点数|10-100|10-100|16|


串口屏宽度：240


## 定时器使用情况

TIM1 测频软件闸门


TIM2 测频通道 PA0


TIM3 TIM4 预计用于ADC时钟


TIM5 测频通道 PH8


TIM6 TIM7 双通道DDS


TIM8 FreeRTOS时钟


TIM12 测试信号 PH6


TIM15 测频时基时钟


### 定时器连接矩阵
![alt text](fbed2776-9476-4bb7-8964-c9ab28f9f703.png)


![alt text](image.png)


## 引脚说明

PA4 - DDS CH1


PA5 - DDS CH2


PA0 - FREQ CH1


PH8 - FREQ CH2


PC4 - ADC CH1


PB1 - ADC CH2


PB14 - USART_RX


PB15 _ USART_TX

## 3.12日更新

定时器的等精度测频必须要求信号的一定幅值才可以，本次ADC采集有可能为小幅值小偏置，所以舍弃定时器方案，我决定减少采样的频率范围，用高频ADC采样率扫描，做*软件比较器*与*软件输入捕获*

## UPDATE: DDS 1VDC Bias & 1Vpp Limits
- 增加了针对 DAC 输出的基准电压计算：固定 **1V DC 偏置 (`DAC_BIAS_1V`)**，并基于 3.3V 满量程对波形查找表（LUT）映射中心。
- API 能够响应入参 `vpp(0-1000)` 直接生成 `±0.5V (1Vpp)` 的可调幅度波形，波形始终以 1V 为中心稳压。

## UPDATE: SPWM Sweep Support (TIM1 & DMA Ping-Pong)
- 新增 `spwm_sweep.h/c` 模块用于实现连续低频扫频，采用 Ping-Pong 机制和 D-Cache 清洗策略：
- 使用 2000 个采样点的缓冲并一分为二（`HT` / `TC` 回调控制），缓冲区配置为 `__attribute__((section(".dma_buffer"))) ALIGN_32BYTES()`。
- 基于 `100kHz` 更新率，支持 `10Hz ~ 1kHz` 低频及 SPWM 平滑改变占空比的无极扫频输出。

## UPDATE: Agents Documentation
- 添加了 `agents_zh.md` (`AGENTS.md` 的中文双语扩展版) 以便代码智能体(Agents)可以随时调取项目的链路上下文。

## UPDATE 3.29: SPWM 定频输出 + 发波触发测量链路

### SPWM 定频输出
- SPWM_Set_Frequency(freq_hz): 10Hz~1kHz 自定义频率输出, 1Hz 分辨率, 支持运行中动态切频
- SPWM_Set_Frequency_Restart(freq_hz): 强制重启定频输出, 相位归零
- 增加 spwm_mode 标志位区分扫频/定频模式, DMA 回调中根据模式分支

### 发波触发测量链路 (adc_process)
- 新增 adc_process.h/c 模块, 封装扫频场景下的 ADC 单次采集 (2048点) + vTaskNotifyGiveFromISR 通知 DSP 任务
- 扫频模式 DMA 回调中植入三阶段状态机:
  1. 滤波器稳定期: 只发波, 等待系统稳定
  2. 单次触发采样: 在驻留时间中点触发 ADC_Sweep_Start_DMA()
  3. 等待切频: 驻留耗尽后切换下一频点
- spwm_sweep.c 不 include adc.h, 保持发波与底层 ADC 硬件的解耦
- DSP 信号处理任务接口已预留, 待后续实现

