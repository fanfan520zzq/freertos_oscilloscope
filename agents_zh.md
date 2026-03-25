# AGENTS.md

## 1. 项目主旨 (Purpose)
本项目为 IIT6 STM32 示波器/DDS（直接数字合成）固件。开发与扩展应当遵循现有架构，避免破坏稳定的软硬件链路。
系统依赖于 FreeRTOS，核心业务逻辑均位于 `Core/Task` 目录中。

## 2. 硬件与定时器拓扑 (System Topology)
- **TIM3 / TIM4**: 用于同步触发 ADC1 和 ADC2 采样。
- **TIM6 / TIM7**: 用于驱动双通道 DDS (DAC输出)。
- **TIM8**: FreeRTOS 系统时基 (Tick)。
- **TIM1/5/12/13**: 预留用于频率测量/门控信号。
- **DMA 内存**: 大容量 DMA 缓冲区（ADC 缓存、DDS 波形缓存、FFT 暂存区）必须指定链接到 AXI SRAM 的 `.dma_buffer` 段：
  `__attribute__((section(".dma_buffer"), aligned(32)))`，以避免 Cache 一致性问题。
- **构建系统**: CMake + CMakePresets (基于 gcc-arm-none-eabi 和 Ninja)。

## 3. FreeRTOS 任务与通信机制 (Task & Queue Model)
核心对象均在 `Core/Src/freertos.c` 中初始化：
- **队列 (Queues)**:
  - `UARTQueue` (80 x uint8_t): 接收来自 `UART1` 的原始字节流。
  - `MSGQueue` (16 x `APP_Text*`): 传递经过解码和堆分配的命令结构体。
- **信号量 (Semaphores)**:
  - `ADCSEM`: 触发单次/连续 ADC 采样。
  - `FFTSEM`: 当双通道 ADC DMA 传输完成后，释放此信号量来唤醒 FFT 计算任务。
  - `ADCFinishedSem`: 采样结束标志，可用于未来门控扩展。
- **任务 (Tasks) / 链路流程**:
  1. `UARTTask` (`UARTTX.c`, Realtime): 解析 `UARTQueue` 字节流，校验帧尾，然后调用 `pvPortMalloc` 生成 `APP_Text` 并推入 `MSGQueue`。
  2. `CMDTask` (`CMD.c`, Normal): 阻塞读取 `MSGQueue`，执行设备控制（如 `DDS_Init`、更新 DAC 波形、切换 `g_is_adc_continuous`），并通过释放 `ADCSEM` 来启动采样。任务需记得 `vPortFree` 回收内存。
  3. `ADCTask` (`ADCTask.c`, Normal6): 等待 `ADCSEM`，开始双路 DMA 转换（如 `CH1_Buffer`），转换完毕立刻停止 DMA，并释放 `FFTSEM`。
  4. `FFTTask` (`FFTTask.c`, Normal5): 等待 `FFTSEM`，计算信号频率（软过零点）、加窗、运行 `arm_rfft_fast_f32` 和波形识别，最后调用 LCD 更新。若 `g_is_adc_continuous == 1`，将重新释放 `ADCSEM` 发起下一轮。

## 4. 信号处理与显示 (Signal Chain & Display)
- **ADC 处理**: 采样率固定为 `2000000.0f`，改动频域或时域计算需参照此基准。
- **LCD 交互**: 采用串口屏（指令以 `0xFF 0xFF 0xFF` 结尾），UI 刷新统一复用 `LCD.c` 中的函数。
- **DDS 输出**: DMA 传输 LUT（如正弦波 `SinBuffer`），必须在更新完缓冲数据后调用 `SCB_CleanDCache_by_Addr` 刷 Cache，再调用 `HAL_DAC_Start_DMA` 启动。

## 5. 编码规范 (Coding Conventions)
- **代码语言**: 使用标准 C 语言，注释使用简易英文 (Simple English)。
- **任务命名**: 遵循 `Start<Name>Task` 的无限循环模式。
- **总线与 Cache**: 操作受 DMA 管理的 AXI 内存前，必须停止外设并在操作后清除/无效化 Data Cache。
- **操作码同步**: `MSG.h` 内置的 Opcode 需与上位机协议保持绝对一致。

