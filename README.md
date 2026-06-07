# SignalScope-F407

Low-voltage signal measurement terminal based on STM32F407, FreeRTOS, LVGL,
ADC, TIM and DMA.

This project runs on an ALIENTEK STM32F407 Explorer board and focuses on a
board-side acquisition and visualization workflow for safe 0-3.3 V signals. It
is not a calibrated oscilloscope; it is an embedded signal measurement
prototype for validating timer-triggered ADC sampling, DMA buffering, RTOS task
separation and local LVGL display.

## Features

- External input path: `PA5 / ADC1_IN5 / STM_ADC`.
- TIM3 TRGO triggers ADC1 sampling; DMA2 circular buffer transfers samples.
- DMA half/full callbacks notify the acquisition task with
  `vTaskNotifyGiveFromISR`.
- FreeRTOS tasks separate acquisition, key/control logic and LVGL UI refresh.
- Triple-buffered UI snapshots reduce blocking between waveform processing and
  display refresh.
- LVGL dashboard displays waveform, sample rate, frequency, Vpp, average,
  range, duty cycle and trigger state.
- Runtime diagnostics show DMA event count, overrun count, task stack headroom
  and task heartbeat.
- F103 external source validation: 1 kHz, 50% duty, 0-3.3 V square wave.

## Architecture

```text
External 0-3.3 V signal
  -> PA5 / ADC1_IN5
  -> TIM3 trigger
  -> ADC1 conversion
  -> DMA2 circular buffer
  -> DMA half/full interrupt
  -> FreeRTOS acquisition task
  -> triple-buffered snapshot
  -> LVGL UI task
```

Core project files:

| Path | Purpose |
| --- | --- |
| `Core/BSP/bsp_scope_adc.c` | ADC/TIM/DMA acquisition driver |
| `Core/APP/app_scope.c` | Sampling state, metrics and diagnostics |
| `Core/APP/app_scope_ui.c` | LVGL dashboard and key actions |
| `Core/Src/freertos.c` | FreeRTOS task creation |
| `cmake/stm32cubemx/CMakeLists.txt` | STM32CubeMX generated source list |

## Build

Requirements:

- CMake 3.22 or later
- Ninja
- `arm-none-eabi-gcc`

Build:

```powershell
cmake --preset Debug
cmake --build --preset Debug
```

Expected artifact:

```text
build/Debug/SignalScope_F407.elf
```

## Hardware Notes

- Connect the test signal to `PA5 / ADC1_IN5 / STM_ADC`.
- Keep the input in the 0-3.3 V range.
- Share ground between the source board and the F407 board.
- Use a 1 kOhm to 4.7 kOhm series resistor for simple board-to-board tests.
- Do not connect negative voltage, unknown high voltage or mains voltage.
- On the Explorer V3 board, `PA1` is connected to the Ethernet PHY reference
  clock path and is not used as the external ADC input.

## Validation

See [VALIDATION.md](VALIDATION.md) for screen checklist, key test and runtime
diagnostics.

## Third-party Components

The repository keeps STM32 HAL/CMSIS, FreeRTOS and LVGL sources needed for a
reproducible firmware build. Third-party components retain their original
licenses.
