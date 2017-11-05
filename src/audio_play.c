#include "audio_play.h"
#include <math.h>
#include <string.h>
#define PI 3.1415926

#define ENV_OVERSHOOT 0.05

SynthConfig cfgnew;
SynthConfig cfg;

uint32_t nco_phase;
float env = 0.0;
EnvState env_state;
float env_curve;

// Output buffer
#define OUT_BUFFER_SAMPLES 512
uint16_t out_buffer_1[OUT_BUFFER_SAMPLES];
uint16_t out_buffer_2[OUT_BUFFER_SAMPLES];
uint16_t *out_buffer = out_buffer_1;

// Sine table
#define SINE_TABLE_WIDTH 11 // bits
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
            s = (nco_phase < UINT32_MAX/2) ? 10000 : -10000;
        } else {
            s = 0;
        }

        // Envelope
        if (cfg.key) {
            if (env_state == ENV_RELEASE) {
                env_state = ENV_ATTACK;
                env = 0.0;
            }

            if (env_state == ENV_ATTACK) {
                env += cfg.attack * (1 + ENV_OVERSHOOT - env);
                if (env > 1.0) {
                    env = 1.0;
                    env_state = ENV_DECAY;
                }
            } else if (env_state == ENV_DECAY) {
                env += cfg.decay * (cfg.sustain - ENV_OVERSHOOT - env);
                if (env < cfg.sustain) {
                    env = cfg.sustain;
                    env_state = ENV_SUSTAIN;
                }
            } else if (env_state == ENV_SUSTAIN) {
                env = cfg.sustain;
            }

        } else {
            env_state = ENV_RELEASE;
            env += cfg.release * (-ENV_OVERSHOOT - env);
            if (env < 0.0) env = 0.0;
        }

        // Filter
        

        s = s * env;

        out_buffer[i] = s;   // left
        out_buffer[i+1] = s; // right
    }

    BSP_LED_Off(LED3);

    printf("%lu\r\n", HAL_GetTick() - start);
}


void AudioPlay_Test(void) {  

    // Initial config
    cfg.volume = 50;
    cfg.freq = 261.13 / SAMPLE_RATE * UINT32_MAX;
    cfg.osc_wave = WAVE_SINE;
    cfg.attack = 0.0005;
    cfg.decay = 0.005;
    cfg.sustain = 1.0;
    cfg.release = 0.0005;
    env_state = ENV_RELEASE;
    memcpy(&cfgnew, &cfg, sizeof(SynthConfig));

    env_curve = -log((1 + ENV_OVERSHOOT) / ENV_OVERSHOOT);

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
    if (cfg.env_retrigger) {
        cfgnew.env_retrigger = false;
        cfg.env_retrigger = false;
        env_state = ENV_RELEASE;
    }

    // convert from time to rate
    cfg.attack  = 1.0 - exp(env_curve / (cfg.attack * SAMPLE_RATE));
    cfg.decay   = 1.0 - exp(env_curve / (cfg.decay * SAMPLE_RATE));
    cfg.release = 1.0 - exp(env_curve / (cfg.release * SAMPLE_RATE));
    


    fill_buffer();

}

