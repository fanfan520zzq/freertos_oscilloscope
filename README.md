




## 统一帧格式


|0xAA|0x0A|OP|FREQ|VPP|WAVE|CRC16|
|---|---|---|---|---|---|---|
|1byte|1byte|1byte|2bytes|2bytes|1byte|2bytes|


## 波形

0-DC
1-SINE
2-SQUARE
3-TRIANGLE



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