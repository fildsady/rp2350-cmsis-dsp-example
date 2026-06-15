# rp2350-cmsis-dsp-example

ตัวอย่างการใช้งาน CMSIS-DSP บน **Raspberry Pi Pico 2 (RP2350)** — สาธิตการใช้ไลบรารี DSP อย่างเป็นทางการของ ARM ที่ผ่านการปรับแต่งแล้ว บน Cortex-M33 พร้อมตัวขยาย DSP ระดับฮาร์ดแวร์ (SIMD และ MAC)

ตัวอย่างแต่ละชุดเป็นอิสระในตัวเอง เลือกใช้งานได้ด้วย `#define` เพียงบรรทัดเดียว และแสดงผลข้อมูลในรูปแบบ CSV ผ่าน USB CDC เพื่อให้วิเคราะห์ได้สะดวก

---

## ตัวอย่างการใช้งาน

| # | ชื่อ | CMSIS-DSP API | คำอธิบาย |
|---|------|---------------|----------|
| 1 | **SINE_GEN** | `arm_sin_f32()`, `arm_cos_f32()` | สร้างคลื่นไซน์/โคไซน์ที่ 440 Hz — ใช้การประมาณค่าพหุนามของ ARM เร็วกว่า `sinf()` |
| 2 | **MOVING_AVG** | `arm_mean_f32()`, `arm_copy_f32()`, `arm_fill_f32()` | ค่าเฉลี่ยเคลื่อนที่แบบ sliding window 16 จุด เร่งความเร็วด้วย SIMD |
| 3 | **FIR_FILTER** | `arm_fir_init_f32()`, `arm_fir_f32()` | ฟิลเตอร์ FIR ผ่านต่ำ windowed sinc 31 tap (ความถี่ตัด 1 kHz ที่อัตราสุ่ม 8 kHz) เร็วกว่า pure C ประมาณ ~4× |
| 4 | **IIR_BIQUAD** | `arm_biquad_cascade_df1_init_f32()`, `arm_biquad_cascade_df1_f32()` | ฟิลเตอร์ผ่านต่ำ Butterworth อันดับ 4 (fc = 1 kHz) แบบ biquad cascade 2 stage |
| 5 | **FFT** | `arm_rfft_fast_f32()`, `arm_cmplx_mag_f32()`, `arm_max_f32()` | การแปลงฟูริเยร์แบบเร็ว (FFT) สำหรับสัญญาณจริง 256 จุด พร้อม Hann window — หา peak ของสเปกตรัมจากสัญญาณ 1 kHz + 2 kHz |
| 6 | **GOERTZEL** | `arm_dot_prod_f32()` | ตัวถอดรหัสโทน DTMF (Dual-Tone Multi-Frequency) ด้วยขั้นตอนวิธี Goertzel ผ่าน SIMD dot product |

---

## ข้อกำหนดฮาร์ดแวร์

- **Raspberry Pi Pico 2** (RP2350, Cortex-M33) — หรือบอร์ด Pico 2 รุ่นอื่นๆ
- สาย USB สำหรับจ่ายไฟและรับส่งข้อมูลแบบอนุกรม

> [!NOTE]
> โปรเจกต์นี้รองรับเฉพาะ **RP2350** เท่านั้น คอร์ Cortex-M33 มีตัวขยาย DSP ระดับฮาร์ดแวร์ (SIMD และ FPU) ซึ่ง CMSIS-DSP ใช้ประโยชน์เพื่อประสิทธิภาพสูงสุด **ไม่รองรับ** Pico รุ่นเดิม (RP2040 / Cortex-M0+)

---

## ไลบรารีที่ต้องใช้

| ไลบรารี | แหล่งที่มา | เวอร์ชัน |
|---------|-----------|---------|
| [Pico SDK](https://github.com/raspberrypi/pico-sdk) | กำหนดค่าอัตโนมัติโดยส่วนขยาย VS Code | 2.2.0 |
| [CMSIS-DSP](https://github.com/ARM-software/CMSIS-DSP) | ดาวน์โหลดผ่าน CMake `FetchContent` | v1.15.0 |
| [FreeRTOS-Kernel](https://github.com/FreeRTOS/FreeRTOS-Kernel) | Git submodule (`lib/FreeRTOS-Kernel`) | latest |

---

## เริ่มต้นใช้งาน

### 1. Clone พร้อม submodule

```bash
git clone --recurse-submodules https://github.com/fildsady/rp2350-cmsis-dsp-example.git
cd rp2350-cmsis-dsp-example
```

หาก clone ไปแล้วโดยไม่ได้ใส่ `--recurse-submodules`:

```bash
git submodule update --init --recursive
```

### 2. เลือกตัวอย่างที่ต้องการ

เปิดไฟล์ [`src/main.c`](src/main.c) แล้วเปลี่ยนค่า `ACTIVE_EXAMPLE`:

```c
#define ACTIVE_EXAMPLE  EXAMPLE_FFT   /* ← เปลี่ยนตรงนี้ */
```

ตัวเลือกที่มี:

```c
#define ACTIVE_EXAMPLE  EXAMPLE_SINE_GEN    // 1
#define ACTIVE_EXAMPLE  EXAMPLE_MOVING_AVG  // 2
#define ACTIVE_EXAMPLE  EXAMPLE_FIR_FILTER  // 3
#define ACTIVE_EXAMPLE  EXAMPLE_IIR_BIQUAD  // 4
#define ACTIVE_EXAMPLE  EXAMPLE_FFT         // 5
#define ACTIVE_EXAMPLE  EXAMPLE_GOERTZEL    // 6
```

### 3. บิลด์โปรเจกต์

#### ใช้ VS Code (แนะนำ)

เปิดโปรเจกต์ด้วยส่วนขยาย **Raspberry Pi Pico** ใน VS Code ระบบจะตรวจจับ SDK และ toolchain โดยอัตโนมัติ

#### ใช้ CMake โดยตรง

```bash
mkdir build && cd build
cmake .. -DPICO_BOARD=pico2
make -j$(nproc)
```

> [!IMPORTANT]
> การบิลด์ครั้งแรกอาจใช้เวลาสักครู่ — CMake จะดาวน์โหลด CMSIS-DSP v1.15.0 ผ่าน `FetchContent` โดยอัตโนมัติ การบิลด์ครั้งถัดไปจะเร็วขึ้นมาก

### 4. โปรแกรมลงบอร์ด

ลากวางไฟล์ `dsp_examples.uf2` ที่ได้ไปยังบอร์ด Pico 2 ที่อยู่ในโหมด BOOTSEL หรือใช้ `picotool`:

```bash
picotool load build/dsp_examples.uf2 --force
```

### 5. ดูผลลัพธ์

เปิด serial monitor ที่พอร์ต USB CDC ของ Pico (ความเร็ว baud rate ใดก็ได้) ส่วนขยาย Serial Monitor ของ VS Code ใช้งานได้โดยตรง

ตัวอย่างทุกชุดแสดงผลเป็น **ข้อมูล CSV** ที่สามารถนำไปวางใน Excel, Python หรือโปรแกรมสร้างกราฟอื่นๆ ได้ทันที

---

## ตัวอย่างผลลัพธ์

### FFT (ตัวอย่างที่ 5)

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

### GOERTZEL / DTMF (ตัวอย่างที่ 6)

```
=== GOERTZEL DTMF (arm_dot_prod_f32, N=128, SR=8000Hz) ===
key='1' → detected='1' [OK]
key='2' → detected='2' [OK]
...
key='#' → detected='#' [OK]

DTMF decode (7 freqs, N=128): 87 us
```

---

## โครงสร้างโปรเจกต์

```
rp2350-cmsis-dsp-example/
├── src/
│   └── main.c              # ตัวอย่างทั้ง 6 ชุด (เลือกด้วย #define)
├── inc/
│   └── FreeRTOSConfig.h    # การกำหนดค่า FreeRTOS
├── lib/
│   └── FreeRTOS-Kernel/    # Git submodule
├── CMakeLists.txt          # การกำหนดค่าบิลด์ — ดาวน์โหลด CMSIS-DSP อัตโนมัติ
├── pico_sdk_import.cmake
└── .vscode/                # การตั้งค่า VS Code และส่วนขยาย Pico
```

---

## ทำไมต้องใช้ CMSIS-DSP?

Cortex-M33 ใน RP2350 มีตัวขยาย DSP ระดับฮาร์ดแวร์ — คำสั่ง SIMD ที่ประมวลผลค่า **2× 16 บิต** หรือ **4× 8 บิต** พร้อมกันในรอบสัญญาณนาฬิกาเดียว พร้อมกับ pipeline MAC (multiply-accumulate) เฉพาะทาง CMSIS-DSP คอมไพล์ให้ใช้คำสั่งเหล่านี้โดยตรง

| การดำเนินการ | Pure C | CMSIS-DSP | ความเร็วที่เพิ่มขึ้น |
|---|---|---|---|
| ฟิลเตอร์ FIR (31 tap, 32 samples) | ~320 µs | ~80 µs | ~4× |
| FFT สำหรับสัญญาณจริง 256 จุด | ~900 µs | ~142 µs | ~6× |
| ถอดรหัส DTMF (7 ความถี่, N=128) | ~320 µs | ~87 µs | ~3.7× |

> ผลการวัดบน RP2350 ที่ความถี่ 125 MHz

---

## สัญญาอนุญาต

โค้ดตัวอย่างในโปรเจกต์นี้เผยแพร่ภายใต้ **MIT License**
CMSIS-DSP เป็นลิขสิทธิ์ของ © ARM Ltd เผยแพร่ภายใต้ Apache 2.0
FreeRTOS Kernel เป็นลิขสิทธิ์ของ © Amazon เผยแพร่ภายใต้ MIT
