# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview
STM32H743-based dual-channel oscilloscope and DDS (Direct Digital Synthesis) signal generator with FreeRTOS task management, real-time FFT analysis, and UART control interface.

**Critical**: Read `AGENTS.md` for complete system architecture, task model, signal chain, and coding conventions. It contains essential tribal knowledge about DMA buffer placement, cache coherency, and FreeRTOS patterns.

## Build Commands

### Initial Configuration
```bash
cmake --preset Debug
```

### Build
```bash
cmake --build build/Debug
```

The output ELF is at `cmake-build-debug/IIT6_Oscilliscope.elf`.

### Flash
Use STM32CubeProgrammer or OpenOCD with your debug probe. The project uses STM32H743XX with the linker script `STM32H743XX_FLASH.ld`.

## Architecture Essentials

### Task Structure
All application logic lives in `Core/Task/*.c` as FreeRTOS tasks:
- `UARTTX.c` - UART frame parser (AA protocol from README.md)
- `CMD.c` - Command dispatcher, controls DDS and ADC modes
- `ADCTask.c` - Dual-channel ADC DMA capture (4096 samples @ 2MHz)
- `FFTTask.c` - Signal analysis: software zero-cross frequency measurement, Hanning window, arm_rfft_fast_f32, harmonic classification
- `LCD.c` - Nextion UART display driver
- `DDS.c` - Dual-channel waveform generation with 1V DC bias, 1Vpp range
- `spwm_sweep.c` - Low-frequency sweep (10Hz-1kHz) with ping-pong DMA

### Critical Memory Regions
Large DMA buffers MUST use:
```c
__attribute__((section(".dma_buffer"), aligned(32)))
```
This places them in AXI SRAM (`.dma_buffer` section in linker script) to avoid D-Cache coherency issues. Always call `SCB_CleanDCache_by_Addr` before DMA reads your buffer.

### Timer Allocation (from README.md)
- TIM1: Frequency measurement gate
- TIM2: Frequency channel PA0
- TIM3/TIM4: ADC sampling clocks (synchronized)
- TIM5: Frequency channel PH8
- TIM6/TIM7: Dual DDS DAC update triggers
- TIM8: FreeRTOS tick source
- TIM12: Test signal output PH6
- TIM15: Frequency measurement timebase

### Communication Queues
Defined in `Core/Src/freertos.c`:
- `UARTQueue` - Raw UART bytes
- `MSGQueue` - Parsed command frames (heap-allocated `APP_Text` from `MSG.h`)

Semaphores:
- `ADCSEM` - Triggers ADC capture
- `FFTSEM` - Signals ADC DMA complete
- `ADCFinishedSem` - Reserved for future use

## Key Constraints

1. **Sample Rate**: ADC runs at fixed `RATE=2000000.0f` (2MHz). Algorithms in FFTTask.c depend on this constant.

2. **Continuous Mode**: Controlled by `g_is_adc_continuous` flag. When enabled, FFTTask re-releases ADCSEM for streaming.

3. **Opcode Protocol**: All UART commands follow `README.md` frame format. New opcodes must be added to `MSG.h` enum.

4. **DDS Constraints**: 1V DC bias, ±0.5V (1Vpp max), 1024-sample LUTs. Waveform types: DC, SINE, SQUARE, TRIANGLE.

5. **Pin Assignments** (from README.md):
   - PA4: DDS CH1, PA5: DDS CH2
   - PA0: FREQ CH1, PH8: FREQ CH2
   - PC4: ADC CH1, PB1: ADC CH2
   - PB14/PB15: UART RX/TX

## Adding New Features

1. Create new task in `Core/Task/<name>.c` with `Start<Name>Task` function
2. Register task in `Core/Src/freertos.c`
3. Add source files to `CMakeLists.txt` target_sources
4. For new UART commands: add opcode to `MSG.h`, handle in `CMD.c`
5. Stop peripherals before modifying DMA buffers, restart after cache maintenance

## Common Pitfalls

- Forgetting `SCB_CleanDCache_by_Addr` on DMA buffers causes stale data
- Not stopping DMA (`HAL_ADC_Stop_DMA`, `HAL_DAC_Stop_DMA`) before buffer edits
- Heap-allocated UART messages (`pvPortMalloc` in UARTTX.c) must be freed
- LCD commands must end with `0xFF 0xFF 0xFF` (Nextion protocol)
