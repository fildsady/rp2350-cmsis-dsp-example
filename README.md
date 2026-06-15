# rp2350-cmsis-dsp-example

CMSIS-DSP examples for the **Raspberry Pi Pico 2 (RP2350)** — demonstrating ARM's official optimized DSP library running on Cortex-M33 with hardware DSP extensions (SIMD, MAC instructions).

Each example is self-contained and selectable via a single `#define`, outputting CSV data over USB CDC for easy analysis.

---

## Examples

| # | Name | CMSIS-DSP API | Description |
|---|------|---------------|-------------|
| 1 | **SINE_GEN** | `arm_sin_f32()`, `arm_cos_f32()` | Sine/cosine wave generation at 440 Hz — ARM polynomial approximation, faster than `sinf()` |
| 2 | **MOVING_AVG** | `arm_mean_f32()`, `arm_copy_f32()`, `arm_fill_f32()` | 16-point sliding window moving average, SIMD-accelerated |
| 3 | **FIR_FILTER** | `arm_fir_init_f32()`, `arm_fir_f32()` | 31-tap windowed sinc low-pass FIR (cutoff 1 kHz @ 8 kHz SR), ~4× faster than pure C |
| 4 | **IIR_BIQUAD** | `arm_biquad_cascade_df1_init_f32()`, `arm_biquad_cascade_df1_f32()` | Butterworth 4th-order low-pass (fc = 1 kHz), 2-stage biquad cascade |
| 5 | **FFT** | `arm_rfft_fast_f32()`, `arm_cmplx_mag_f32()`, `arm_max_f32()` | 256-point real FFT with Hann window — finds spectral peaks of 1 kHz + 2 kHz signal |
| 6 | **GOERTZEL** | `arm_dot_prod_f32()` | DTMF tone decoder using Goertzel algorithm via SIMD dot-product |

---

## Hardware Requirements

- **Raspberry Pi Pico 2** (RP2350, Cortex-M33) — or any Pico 2 variant
- USB cable for power and serial output

> [!NOTE]
> This project targets the **RP2350** specifically. The Cortex-M33 core provides hardware DSP extensions (SIMD, FPU) that CMSIS-DSP exploits for maximum performance. It will **not** build for the original Pico (RP2040 / Cortex-M0+).

---

## Dependencies

| Dependency | Source | Version |
|---|---|---|
| [Pico SDK](https://github.com/raspberrypi/pico-sdk) | Auto-configured by VS Code extension | 2.2.0 |
| [CMSIS-DSP](https://github.com/ARM-software/CMSIS-DSP) | Fetched via CMake `FetchContent` | v1.15.0 |
| [FreeRTOS-Kernel](https://github.com/FreeRTOS/FreeRTOS-Kernel) | Git submodule (`lib/FreeRTOS-Kernel`) | latest |

---

## Getting Started

### 1. Clone with submodules

```bash
git clone --recurse-submodules https://github.com/fildsady/rp2350-cmsis-dsp-example.git
cd rp2350-cmsis-dsp-example
```

If you already cloned without `--recurse-submodules`:

```bash
git submodule update --init --recursive
```

### 2. Select an example

Open [`src/main.c`](src/main.c) and change the `ACTIVE_EXAMPLE` define:

```c
#define ACTIVE_EXAMPLE  EXAMPLE_FFT   /* ← change this */
```

Available options:

```c
#define ACTIVE_EXAMPLE  EXAMPLE_SINE_GEN    // 1
#define ACTIVE_EXAMPLE  EXAMPLE_MOVING_AVG  // 2
#define ACTIVE_EXAMPLE  EXAMPLE_FIR_FILTER  // 3
#define ACTIVE_EXAMPLE  EXAMPLE_IIR_BIQUAD  // 4
#define ACTIVE_EXAMPLE  EXAMPLE_FFT         // 5
#define ACTIVE_EXAMPLE  EXAMPLE_GOERTZEL    // 6
```

### 3. Build

#### Using VS Code (Recommended)

Open the project with the **Raspberry Pi Pico** VS Code extension. It will auto-detect the SDK and toolchain.

#### Using CMake directly

```bash
mkdir build && cd build
cmake .. -DPICO_BOARD=pico2
make -j$(nproc)
```

> [!IMPORTANT]
> First build will take a few minutes — CMake will automatically download CMSIS-DSP v1.15.0 via `FetchContent`. Subsequent builds are fast.

### 4. Flash

Drag and drop the generated `dsp_examples.uf2` onto the Pico 2 in BOOTSEL mode, or use `picotool`:

```bash
picotool load build/dsp_examples.uf2 --force
```

### 5. View output

Open a serial monitor at the Pico's USB CDC port (any baud rate). The VS Code Serial Monitor extension works directly.

All examples output **CSV data** that can be pasted into Excel, Python, or any plotting tool.

---

## Example Output

### FFT (Example 5)

```
=== RFFT (arm_rfft_fast_f32, N=256, SR=8000Hz, bin=31.2Hz) ===
input: sin(1kHz) + sin(2kHz) + Hann window

arm_rfft_fast_f32: 142 us
arm_cmplx_mag_f32: 18 us
Peak: bin 32 → 1000.0 Hz (mag=0.9821)

bin,freq_hz,magnitude
0,0.0,0.00012
...
32,1000.0,0.98213
...
64,2000.0,0.97951
...
```

### GOERTZEL / DTMF (Example 6)

```
=== GOERTZEL DTMF (arm_dot_prod_f32, N=128, SR=8000Hz) ===
key='1' → detected='1' [OK]
key='2' → detected='2' [OK]
...
key='#' → detected='#' [OK]

DTMF decode (7 freqs, N=128): 87 us
```

---

## Project Structure

```
rp2350-cmsis-dsp-example/
├── src/
│   └── main.c              # All 6 examples (selected by #define)
├── inc/
│   └── FreeRTOSConfig.h    # FreeRTOS configuration
├── lib/
│   └── FreeRTOS-Kernel/    # Git submodule
├── CMakeLists.txt          # Build config — fetches CMSIS-DSP automatically
├── pico_sdk_import.cmake
└── .vscode/                # VS Code + Pico extension settings
```

---

## Why CMSIS-DSP?

The Cortex-M33 in the RP2350 has hardware DSP extensions — SIMD instructions that operate on **2× 16-bit** or **4× 8-bit** values in a single cycle, plus a dedicated MAC (multiply-accumulate) pipeline. CMSIS-DSP compiles to use these instructions directly.

| Operation | Pure C | CMSIS-DSP | Speedup |
|---|---|---|---|
| FIR filter (31 tap, 32 samples) | ~320 µs | ~80 µs | ~4× |
| 256-point real FFT | ~900 µs | ~142 µs | ~6× |
| DTMF decode (7 freqs, N=128) | ~320 µs | ~87 µs | ~3.7× |

> Numbers measured on RP2350 @ 125 MHz.

---

## License

This example code is released under the **MIT License**.  
CMSIS-DSP is © ARM Ltd, licensed under Apache 2.0.  
FreeRTOS Kernel is © Amazon, licensed under MIT.
