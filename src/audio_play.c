#include "audio_play.h"
#include <math.h>
#include <string.h>
#define PI 3.1415926

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
int16_t sine_table[SINE_TABLE_SIZE];


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
float env = 0.0;

void fill_buffer(void) {

    uint32_t start = HAL_GetTick();
    int16_t s;
    int idx;

    BSP_LED_On(LED3);

    for (int i=0; i<OUT_BUFFER_SAMPLES; i += 2) {
        
        // Oscillator
        nco_phase += cfg.freq;
        idx = nco_phase >> (32 - SINE_TABLE_WIDTH);

        if (cfg.osc_wave == WAVE_SINE) {
            s = sine_table[idx]; 
        } else if (cfg.osc_wave == WAVE_SQUARE) {
            s = (nco_phase < UINT32_MAX/2) ? 10000 : 0;
        } else {
            s = 0;
        }

        // Envelope
        if (cfg.key) {
            env += cfg.attack;
            if (env > 1.0) env = 1.0;
        } else {
            env -= cfg.release;
            if (env < 0.0) env = 0.0;
        }

        s = s * env;


        out_buffer[i] = s;   // left
        out_buffer[i+1] = s; // right
    }

    BSP_LED_Off(LED3);

    //printf("%lu\r\n", HAL_GetTick() - start);
}


void AudioPlay_Test(void) {  

    // Initial config
    cfg.volume = 50;
    cfg.freq = 261.13 / SAMPLE_RATE * UINT32_MAX;
    cfg.osc_wave = WAVE_SINE;
    cfg.attack = 0.0002;
    cfg.release = 0.0001;
    memcpy(&cfgnew, &cfg, sizeof(SynthConfig));

    create_wave_tables();
    fill_buffer();

    if (BSP_AUDIO_OUT_Init(OUTPUT_DEVICE_AUTO, cfg.volume, SAMPLE_RATE) != 0) {
        printf("init failed\r\n");
        return;
    }

    BSP_AUDIO_OUT_Play(out_buffer, OUT_BUFFER_SAMPLES * 2);
}


void BSP_AUDIO_OUT_TransferComplete_CallBack(void) {
 
    BSP_AUDIO_OUT_ChangeBuffer(out_buffer, OUT_BUFFER_SAMPLES);
    swap_buffers();

    // Copy new settings
    if (cfgnew.volume != cfg.volume) {
        BSP_AUDIO_OUT_SetVolume(cfgnew.volume);
    }

    memcpy(&cfg, &cfgnew, sizeof(SynthConfig));
    fill_buffer();

}

