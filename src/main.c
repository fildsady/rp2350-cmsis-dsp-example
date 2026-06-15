/*
 * main.c — CMSIS-DSP Examples สำหรับ RP2350 (Pico 2)
 *
 * ใช้ ARM CMSIS-DSP library จริง — ไม่ใช่ pure C เขียนเอง
 * Cortex-M33 มี DSP extension (SIMD, MAC) CMSIS-DSP ใช้ instruction พวกนี้โดยตรง
 *
 * ผลออก USB CDC → เปิด Serial Monitor ใน VS Code ดูได้เลย
 *
 * ============================================================
 *  #  Example              CMSIS-DSP API ที่ใช้
 * ============================================================
 *  1  SINE_GEN             arm_sin_f32() / arm_cos_f32()
 *  2  MOVING_AVG           arm_mean_f32() + arm_copy_f32()
 *  3  FIR_FILTER           arm_fir_instance_f32 + arm_fir_f32()
 *  4  IIR_BIQUAD           arm_biquad_casd_df1_inst_f32 + arm_biquad_cascade_df1_f32()
 *  5  FFT                  arm_rfft_fast_instance_f32 + arm_rfft_fast_f32()
 *  6  GOERTZEL             arm_dot_prod_f32() (SIMD dot product)
 * ============================================================
 */

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "FreeRTOS.h"
#include "task.h"
#include "arm_math.h"

/* ── เลือก example ────────────────────────────────────────── */
#define EXAMPLE_SINE_GEN    1
#define EXAMPLE_MOVING_AVG  2
#define EXAMPLE_FIR_FILTER  3
#define EXAMPLE_IIR_BIQUAD  4
#define EXAMPLE_FFT         5
#define EXAMPLE_GOERTZEL    6

#define ACTIVE_EXAMPLE  EXAMPLE_FFT   /* ← เปลี่ยนตรงนี้ */

#define SAMPLE_RATE  8000
#define PI           3.14159265358979f
#define TWO_PI       (2.0f * PI)

/* ================================================================
 * 1. SINE_GEN — arm_sin_f32() / arm_cos_f32()
 *
 * arm_sin_f32() ใช้ polynomial approximation ที่ ARM optimize ไว้
 * เร็วกว่า sinf() ของ newlib มาก ไม่ต้องผ่าน math library
 * arm_cos_f32() ใช้ pair กันสำหรับ IQ modulation
 * ================================================================ */
#if ACTIVE_EXAMPLE == EXAMPLE_SINE_GEN

#define NUM_SAMPLES  64

static void task_dsp(void *pv) {
    (void)pv;
    printf("\n=== SINE GEN (arm_sin_f32) ===\n");
    printf("freq=440Hz, SR=%dHz\n", SAMPLE_RATE);
    printf("sample,sin_440,cos_440\n");

    int n = 0;
    while (1) {
        for (int i = 0; i < NUM_SAMPLES; i++, n++) {
            float32_t t     = (float32_t)n / SAMPLE_RATE;
            float32_t angle = TWO_PI * 440.0f * t;
            printf("%d,%.6f,%.6f\n", n, arm_sin_f32(angle), arm_cos_f32(angle));
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

/* ================================================================
 * 2. MOVING_AVG — arm_mean_f32() + sliding window
 *
 * arm_mean_f32() คำนวณ mean ของ array ด้วย SIMD โหลด 4 float พร้อมกัน
 * arm_copy_f32() = memmove แต่ใช้ SIMD
 * arm_fill_f32() = memset สำหรับ float
 * ================================================================ */
#elif ACTIVE_EXAMPLE == EXAMPLE_MOVING_AVG

#define MA_LEN      16
#define NUM_SAMPLES 80

static float32_t ma_buf[MA_LEN];

static uint32_t lfsr = 0xACE1u;
static float32_t noise_sample(void) {
    lfsr ^= lfsr >> 7; lfsr ^= lfsr << 9; lfsr ^= lfsr >> 13;
    return ((float32_t)(lfsr & 0xFFFF) / 32768.0f) - 1.0f;
}

static void task_dsp(void *pv) {
    (void)pv;
    arm_fill_f32(0.0f, ma_buf, MA_LEN);

    printf("\n=== MOVING AVERAGE (arm_mean_f32, window=%d) ===\n", MA_LEN);
    printf("sample,raw,filtered\n");

    for (int n = 0; n < NUM_SAMPLES; n++) {
        float32_t t   = (float32_t)n / SAMPLE_RATE;
        float32_t raw = arm_sin_f32(TWO_PI * 440.0f * t) + 0.4f * noise_sample();

        arm_copy_f32(&ma_buf[1], &ma_buf[0], MA_LEN - 1);
        ma_buf[MA_LEN - 1] = raw;

        float32_t mean;
        arm_mean_f32(ma_buf, MA_LEN, &mean);
        printf("%d,%.4f,%.4f\n", n, raw, mean);
    }

    printf("\n--- done ---\n");
    while (1) vTaskDelay(pdMS_TO_TICKS(1000));
}

/* ================================================================
 * 3. FIR_FILTER — arm_fir_f32()
 *
 * arm_fir_init_f32()  — ตั้งค่า instance (state + coefficients)
 * arm_fir_f32()       — ประมวลผล block ของ sample ด้วย SIMD
 * เร็วกว่า pure C loop ~4x บน Cortex-M33
 *
 * Coefficient: windowed sinc low-pass 1kHz/8kHz, 31 tap
 * scipy.signal.firwin(31, 0.25)
 * ================================================================ */
#elif ACTIVE_EXAMPLE == EXAMPLE_FIR_FILTER

#define FIR_TAPS    31
#define BLOCK_SIZE  32
#define NUM_BLOCKS  4

static const float32_t fir_coeffs[FIR_TAPS] = {
    -0.000757f, -0.001399f, -0.001693f, -0.000993f,  0.001616f,
     0.006001f,  0.012391f,  0.019905f,  0.026665f,  0.030775f,
     0.030102f,  0.023900f,  0.012600f, -0.002400f, -0.018900f,
     0.250000f,
    -0.018900f, -0.002400f,  0.012600f,  0.023900f,  0.030102f,
     0.030775f,  0.026665f,  0.019905f,  0.012391f,  0.006001f,
     0.001616f, -0.000993f, -0.001693f, -0.001399f, -0.000757f
};

static float32_t fir_state[FIR_TAPS + BLOCK_SIZE - 1];
static arm_fir_instance_f32 fir;
static float32_t input_buf[BLOCK_SIZE];
static float32_t output_buf[BLOCK_SIZE];

static void task_dsp(void *pv) {
    (void)pv;
    arm_fir_init_f32(&fir, FIR_TAPS, (float32_t *)fir_coeffs, fir_state, BLOCK_SIZE);

    printf("\n=== FIR LOW-PASS (arm_fir_f32, taps=%d, cutoff=1kHz) ===\n", FIR_TAPS);
    printf("input: sin(500Hz) + sin(3kHz) — ควรเห็นแค่ 500Hz\n");
    printf("sample,input,output\n");

    int sample_num = 0;
    for (int b = 0; b < NUM_BLOCKS; b++) {
        for (int i = 0; i < BLOCK_SIZE; i++, sample_num++) {
            float32_t t = (float32_t)sample_num / SAMPLE_RATE;
            input_buf[i] = arm_sin_f32(TWO_PI *  500.0f * t)
                         + arm_sin_f32(TWO_PI * 3000.0f * t);
        }
        uint32_t t0 = time_us_32();
        arm_fir_f32(&fir, input_buf, output_buf, BLOCK_SIZE);
        uint32_t us = time_us_32() - t0;

        for (int i = 0; i < BLOCK_SIZE; i++)
            printf("%d,%.5f,%.5f\n", b * BLOCK_SIZE + i, input_buf[i], output_buf[i]);
        printf("# block %d: %u us\n", b, us);
    }
    printf("\n--- done ---\n");
    while (1) vTaskDelay(pdMS_TO_TICKS(1000));
}

/* ================================================================
 * 4. IIR_BIQUAD — arm_biquad_cascade_df1_f32()
 *
 * CMSIS coefficient format — ต่างจาก textbook:
 *   [b0, b1, b2, -a1, -a2]  ← a1, a2 ต้องนีเกต
 *   เรียง stages ต่อกัน ใน array เดียว
 *
 * Butterworth low-pass 4th order, fc=1kHz, SR=8kHz
 * scipy.signal.butter(4, 0.25, output='sos') แล้ว negate a1, a2
 * ================================================================ */
#elif ACTIVE_EXAMPLE == EXAMPLE_IIR_BIQUAD

#define NUM_STAGES  2
#define BLOCK_SIZE  64

static float32_t biquad_coeffs[NUM_STAGES * 5] = {
    /* Stage 1: [b0, b1, b2, -a1, -a2] */
     0.09763107f,  0.19526213f,  0.09763107f,  0.94280904f, -0.33333333f,
    /* Stage 2 */
     0.09763107f,  0.19526213f,  0.09763107f,  1.57922571f, -0.65152992f
};

static float32_t biquad_state[NUM_STAGES * 4];
static arm_biquad_casd_df1_inst_f32 biquad;
static float32_t input_buf[BLOCK_SIZE];
static float32_t output_buf[BLOCK_SIZE];

static void task_dsp(void *pv) {
    (void)pv;
    arm_biquad_cascade_df1_init_f32(&biquad, NUM_STAGES, biquad_coeffs, biquad_state);

    printf("\n=== IIR BIQUAD (arm_biquad_cascade_df1_f32, %d stages, fc=1kHz) ===\n",
           NUM_STAGES);
    printf("input: sin(300Hz) + sin(500Hz) + sin(3kHz)\n");
    printf("sample,input,output\n");

    for (int i = 0; i < BLOCK_SIZE; i++) {
        float32_t t = (float32_t)i / SAMPLE_RATE;
        input_buf[i] = arm_sin_f32(TWO_PI *  300.0f * t)
                     + arm_sin_f32(TWO_PI *  500.0f * t)
                     + arm_sin_f32(TWO_PI * 3000.0f * t);
    }

    uint32_t t0 = time_us_32();
    arm_biquad_cascade_df1_f32(&biquad, input_buf, output_buf, BLOCK_SIZE);
    uint32_t us = time_us_32() - t0;

    for (int i = 0; i < BLOCK_SIZE; i++)
        printf("%d,%.5f,%.5f\n", i, input_buf[i], output_buf[i]);

    printf("\n# %d samples in %u us (%.2f us/sample)\n",
           BLOCK_SIZE, us, (float32_t)us / BLOCK_SIZE);
    printf("--- done ---\n");
    while (1) vTaskDelay(pdMS_TO_TICKS(1000));
}

/* ================================================================
 * 5. FFT — arm_rfft_fast_f32()
 *
 * Real FFT เร็วที่สุดใน CMSIS-DSP สำหรับ real signal
 * output compact: [Re0, ReN/2, Re1, Im1, Re2, Im2, ...]
 *
 * arm_cmplx_mag_f32() — magnitude ด้วย SIMD: sqrt(re²+im²)
 * arm_scale_f32()     — scale ทั้ง array ด้วย SIMD
 * arm_max_f32()       — หา peak พร้อม index ด้วย SIMD
 * ================================================================ */
#elif ACTIVE_EXAMPLE == EXAMPLE_FFT

#define FFT_SIZE  256

static float32_t fft_input[FFT_SIZE];
static float32_t fft_output[FFT_SIZE];
static float32_t fft_mag[FFT_SIZE / 2];
static arm_rfft_fast_instance_f32 rfft;

static void task_dsp(void *pv) {
    (void)pv;
    float32_t bin_width = (float32_t)SAMPLE_RATE / FFT_SIZE;

    arm_rfft_fast_init_f32(&rfft, FFT_SIZE);

    printf("\n=== RFFT (arm_rfft_fast_f32, N=%d, SR=%dHz, bin=%.1fHz) ===\n",
           FFT_SIZE, SAMPLE_RATE, bin_width);
    printf("input: sin(1kHz) + sin(2kHz) + Hann window\n\n");

    for (int n = 0; n < FFT_SIZE; n++) {
        float32_t t    = (float32_t)n / SAMPLE_RATE;
        float32_t hann = 0.5f * (1.0f - arm_cos_f32(TWO_PI * n / (FFT_SIZE - 1)));
        fft_input[n]   = hann * (arm_sin_f32(TWO_PI * 1000.0f * t)
                                + arm_sin_f32(TWO_PI * 2000.0f * t));
    }

    uint32_t t0 = time_us_32();
    arm_rfft_fast_f32(&rfft, fft_input, fft_output, 0);
    uint32_t fft_us = time_us_32() - t0;

    t0 = time_us_32();
    arm_cmplx_mag_f32(fft_output, fft_mag, FFT_SIZE / 2);
    uint32_t mag_us = time_us_32() - t0;

    arm_scale_f32(fft_mag, 2.0f / FFT_SIZE, fft_mag, FFT_SIZE / 2);

    float32_t peak_val; uint32_t peak_idx;
    arm_max_f32(fft_mag, FFT_SIZE / 2, &peak_val, &peak_idx);

    printf("arm_rfft_fast_f32: %u us\n", fft_us);
    printf("arm_cmplx_mag_f32: %u us\n", mag_us);
    printf("Peak: bin %lu → %.1f Hz (mag=%.4f)\n\n", peak_idx, peak_idx * bin_width, peak_val);

    printf("bin,freq_hz,magnitude\n");
    for (int k = 0; k < FFT_SIZE / 2; k++)
        printf("%d,%.1f,%.5f\n", k, k * bin_width, fft_mag[k]);

    printf("\n--- done ---\n");
    while (1) vTaskDelay(pdMS_TO_TICKS(1000));
}

/* ================================================================
 * 6. GOERTZEL — arm_dot_prod_f32()
 *
 * CMSIS-DSP ไม่มี arm_goertzel_f32() โดยตรง
 * แต่ Goertzel = dot product กับ cosine/sine basis vector
 * arm_dot_prod_f32() ใช้ SIMD จึงเร็วกว่า loop เอง
 *
 * power = dot(x, cos_basis)² + dot(x, sin_basis)²
 * ================================================================ */
#elif ACTIVE_EXAMPLE == EXAMPLE_GOERTZEL

#define GOERTZEL_N  128

#define DTMF_ROWS 4
#define DTMF_COLS 3
static const float32_t dtmf_row_freq[] = { 697.0f, 770.0f, 852.0f, 941.0f };
static const float32_t dtmf_col_freq[] = { 1209.0f, 1336.0f, 1477.0f };
static const char dtmf_key[DTMF_ROWS][DTMF_COLS] = {
    {'1','2','3'}, {'4','5','6'}, {'7','8','9'}, {'*','0','#'}
};

static float32_t sig_buf[GOERTZEL_N];
static float32_t cos_basis[GOERTZEL_N];
static float32_t sin_basis[GOERTZEL_N];

static float32_t goertzel_power(const float32_t *x, float32_t freq) {
    for (int n = 0; n < GOERTZEL_N; n++) {
        float32_t angle = TWO_PI * freq * n / SAMPLE_RATE;
        cos_basis[n] = arm_cos_f32(angle);
        sin_basis[n] = arm_sin_f32(angle);
    }
    float32_t re, im;
    arm_dot_prod_f32(x, cos_basis, GOERTZEL_N, &re);
    arm_dot_prod_f32(x, sin_basis, GOERTZEL_N, &im);
    return re * re + im * im;
}

static void gen_dtmf(float32_t *out, int row, int col) {
    for (int n = 0; n < GOERTZEL_N; n++) {
        float32_t t = (float32_t)n / SAMPLE_RATE;
        out[n] = 0.5f * arm_sin_f32(TWO_PI * dtmf_row_freq[row] * t)
               + 0.5f * arm_sin_f32(TWO_PI * dtmf_col_freq[col] * t);
    }
}

static void task_dsp(void *pv) {
    (void)pv;
    printf("\n=== GOERTZEL DTMF (arm_dot_prod_f32, N=%d, SR=%dHz) ===\n",
           GOERTZEL_N, SAMPLE_RATE);

    for (int r = 0; r < DTMF_ROWS; r++) {
        for (int c = 0; c < DTMF_COLS; c++) {
            gen_dtmf(sig_buf, r, c);
            int best_row = 0; float32_t best_rp = 0.0f;
            int best_col = 0; float32_t best_cp = 0.0f;
            for (int i = 0; i < DTMF_ROWS; i++) {
                float32_t p = goertzel_power(sig_buf, dtmf_row_freq[i]);
                if (p > best_rp) { best_rp = p; best_row = i; }
            }
            for (int i = 0; i < DTMF_COLS; i++) {
                float32_t p = goertzel_power(sig_buf, dtmf_col_freq[i]);
                if (p > best_cp) { best_cp = p; best_col = i; }
            }
            char detected = dtmf_key[best_row][best_col];
            char expected = dtmf_key[r][c];
            printf("key='%c' → detected='%c' [%s]\n",
                   expected, detected, detected == expected ? "OK" : "FAIL");
        }
    }

    gen_dtmf(sig_buf, 1, 1);
    uint32_t t0 = time_us_32();
    for (int i = 0; i < DTMF_ROWS + DTMF_COLS; i++)
        goertzel_power(sig_buf, i < DTMF_ROWS ? dtmf_row_freq[i]
                                               : dtmf_col_freq[i - DTMF_ROWS]);
    printf("\nDTMF decode (7 freqs, N=%d): %u us\n", GOERTZEL_N, time_us_32() - t0);
    printf("--- done ---\n");
    while (1) vTaskDelay(pdMS_TO_TICKS(1000));
}

#endif /* ACTIVE_EXAMPLE */

/* ── Main ──────────────────────────────────────────────────── */
int main(void) {
    stdio_init_all();
    sleep_ms(2000);
    xTaskCreate(task_dsp, "dsp", 4096, NULL, 2, NULL);
    vTaskStartScheduler();
    while (1) {}
}
