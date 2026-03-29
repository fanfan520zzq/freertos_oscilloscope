# 任务名称：建立发波触发测量链路

## 1. 架构目标
在原有的 SPWM 扫频驱动 (`spwm_sweep.c`) 中引入“稳定期丢弃”与“采样触发”状态机。
同时，搭建接收端 (`adc_process.h/.c`) 的骨架并声明对外接口，为后续的 FreeRTOS 信号处理任务（DSP）铺平道路。
核心设计原则：**发波为主，采样为辅，高频中断必须极简。**

## 2. 核心修改：spwm_sweep.c (状态机植入)
在现有的 `SPWM_DMA_HalfCpltCallback` 和 `SPWM_DMA_CpltCallback` 中，利用 `current_pulse_counter` 实现三阶段流水线逻辑：

* **阶段 1 (滤波器稳定期)：**
  当 `0 <= current_pulse_counter < sample_start_point` 时，只进行 DDS 查表发波，不触发 ADC。
  *约束：* `sample_start_point` 动态设为当前频点 `Target_Pulse_Count / 2`。
* **阶段 2 (单次触发采样)：**
  当且仅当 `current_pulse_counter == sample_start_point` 成立的那**一次**中断里，调用外部接口 `ADC_Sweep_Start_DMA()`。
  *约束：* 必须保证每个频点只触发一次 ADC，严禁在后续中断中重复触发。
* **阶段 3 (等待切频)：**
  当 `current_pulse_counter >= Target_Pulse_Count` 时，代表当前频点驻留时间耗尽。切换 `Target_FTW` 和 `Target_Pulse_Count` 进入下一个频点，并重置 `current_pulse_counter = 0`。

## 3. 新增模块：adc_process.h (接收端接口声明)
新建 `adc_process.h` 和 `adc_process.c`，对外暴露以下接口，供发波链路和 RTOS 调度：

* **数据规模宏定义：** `#define ADC_SAMPLE_LENGTH 2048` (预留给后续的 FFT 或 RMS 算法)。
* **内存对齐的缓冲区：**
    ```c
    // 必须声明 D-Cache 对齐，防止 DMA 搬运与 CPU 读取产生一致性错误
    extern ALIGN_32BYTES(uint16_t adc_buffer[ADC_SAMPLE_LENGTH]);
    ```
* **底层触发接口：** `void ADC_Sweep_Start_DMA(void);`
  *说明：* 此函数内部封装 `HAL_ADC_Start_DMA`，隐藏底层硬件句柄（如 `hadc1`），供 `spwm_sweep.c` 跨文件调用。
* **中断回调与任务通知（在 .c 中实现）：**
  重写 `HAL_ADC_ConvCpltCallback`，在 2048 个点采满后，使用 `vTaskNotifyGiveFromISR()` 唤醒后台的 DSP 计算任务。

## 4. 工程底线警告
* **中断超时危险：** `SPWM_DMA_HalfCpltCallback` 是发生频率极高（例如每 10ms 一次）的底层中断。内部**绝不允许**加入任何阻塞逻辑（如 `while` 等待、系统级 Delay 或复杂的浮点运算）。
* **解耦：** `spwm_sweep.c` 不包含任何 `<adc.h>` 或具体的 ADC 句柄，只 `include "adc_process.h"`。