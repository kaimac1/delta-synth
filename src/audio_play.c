#include "audio_play.h"
#include <math.h>
#include <string.h>
#define PI 3.1415926

extern __IO uint8_t UserPressButton;

SynthConfig cfgnew;
SynthConfig cfg;

// Output buffer
#define OUT_BUFFER_SAMPLES 512
uint16_t out_buffer_1[OUT_BUFFER_SAMPLES];
uint16_t out_buffer_2[OUT_BUFFER_SAMPLES];
uint16_t *out_buffer = out_buffer_1;

// Sine table
#define SINE_TABLE_WIDTH 10 // bits
#define SINE_TABLE_SIZE (1<<SINE_TABLE_WIDTH)
uint16_t sine_table[SINE_TABLE_SIZE];


void create_wave_tables(void) {

    // Sine
    for (int i=0; i<SINE_TABLE_SIZE; i++) {
        float arg = 2*PI*i / SINE_TABLE_SIZE;
        sine_table[i] = (int16_t)(sin(arg) * 32768);
    }
}


void swap_buffers(void) {

    if (out_buffer == out_buffer_1) {
        out_buffer = out_buffer_2;
    } else {
        out_buffer = out_buffer_1;
    }
}


uint32_t nco_phase;

void fill_buffer(void) {

    uint32_t start = HAL_GetTick();
    uint16_t s;
    int idx;

    BSP_LED_On(LED3);

    for (int i=0; i<OUT_BUFFER_SAMPLES; i += 2) {
        nco_phase += cfg.freq;
        idx = nco_phase >> (32 - SINE_TABLE_WIDTH);

        if (cfg.osc_wave == WAVE_SINE) {
            s = sine_table[idx]; 
        } else if (cfg.osc_wave == WAVE_SQUARE) {
            s = (nco_phase < UINT32_MAX/2) ? 10000 : 0;
        } else {
            s = 0;
        }

        out_buffer[i] = s;   // left
        out_buffer[i+1] = s; // right
    }

    BSP_LED_Off(LED3);

    //printf("%lu\r\n", HAL_GetTick() - start);
}


void AudioPlay_Test(void) {  

    __IO uint8_t volume = 70; // 0 - 100

    // Initial config
    cfg.freq = 0.02553125 * UINT32_MAX;
    cfg.osc_wave = WAVE_SINE;
    memcpy(&cfgnew, &cfg, sizeof(SynthConfig));

    create_wave_tables();
    fill_buffer();

    if (BSP_AUDIO_OUT_Init(OUTPUT_DEVICE_AUTO, volume, SAMPLE_RATE) != 0) {
        printf("init failed\r\n");
        return;
    }

    BSP_AUDIO_OUT_Play(out_buffer, OUT_BUFFER_SAMPLES * 2);
}


void BSP_AUDIO_OUT_TransferComplete_CallBack(void) {
 
    BSP_AUDIO_OUT_ChangeBuffer(out_buffer, OUT_BUFFER_SAMPLES);
    swap_buffers();

    // Copy new settings
    memcpy(&cfg, &cfgnew, sizeof(SynthConfig));
    fill_buffer();

}

