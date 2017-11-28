#include "synth.h"
#include "board.h"
#include "stm32f401_discovery.h"
#include "stm32f401_discovery_audio.h"
#include <math.h>
#include <string.h>
#include "stm32f4xx_ll_tim.h"
#define PI 3.1415926f

#define ENV_OVERSHOOT 0.05f

SynthConfig cfgnew;
SynthConfig cfg;

uint32_t nco_phase;
float env = 0.0;
EnvState env_state;

uint32_t start_time;
uint32_t loop_time;

// Output buffer
#define OUT_BUFFER_SAMPLES 512
uint16_t out_buffer_1[OUT_BUFFER_SAMPLES];
uint16_t out_buffer_2[OUT_BUFFER_SAMPLES];
uint16_t *out_buffer = out_buffer_1;

// Sine table
#define SINE_TABLE_WIDTH 11 // bits
#define SINE_TABLE_SIZE (1<<SINE_TABLE_WIDTH)
int16_t sine_table[SINE_TABLE_SIZE];

void fill_buffer(void);


// filter state
float z1, z2, z3, z4;

/******************************************************************************/
// This ISR is called when the DMA transfer of one buffer (A) completes.
// We need to immediately swap the DMA over to the other buffer (B), and start
// refilling the buffer that was just sent (A).

void BSP_AUDIO_OUT_TransferComplete_CallBack(void) {
 
    BSP_AUDIO_OUT_ChangeBuffer(out_buffer, OUT_BUFFER_SAMPLES);
    out_buffer = (out_buffer == out_buffer_1) ? out_buffer_2 : out_buffer_1;

    start_time = LL_TIM_GetCounter(TIM2);

    // Copy new settings: cfgnew -> cfg
    if (cfgnew.volume != cfg.volume) {
        BSP_AUDIO_OUT_SetVolume(cfgnew.volume);
    }
    memcpy(&cfg, &cfgnew, sizeof(SynthConfig));
    if (cfg.env_retrigger) {
        cfgnew.env_retrigger = false;
        cfg.env_retrigger = false;
        env_state = ENV_RELEASE;
    }

    fill_buffer();
    loop_time = LL_TIM_GetCounter(TIM2) - start_time;

}


void create_wave_tables(void) {

    // Sine
    for (int i=0; i<SINE_TABLE_SIZE; i++) {
        float arg = 2*PI*i / SINE_TABLE_SIZE;
        sine_table[i] = (int16_t)(sin(arg) * 32768);
    }
}

inline float polyblep(float t) {
    float dt = (float)cfg.freq / UINT32_MAX; // 0-1
    if (t < dt) {
        t /= dt;
        return t + t - t*t - 1.0f;
    } else if (t > 1.0f - dt) {
        t = (t - 1.0f)/dt;
        return t*t + t + t + 1.0f;
    } else {
        return 0.0f;
    }
}


void fill_buffer(void) {

    float s;

    for (int i=0; i<OUT_BUFFER_SAMPLES; i += 2) {
        
        // Oscillator
        nco_phase += cfg.freq;
        float phase = (float)nco_phase / UINT32_MAX;
        s = 2.0f * phase - 1.0f;
        s -= polyblep(phase);

        // Envelope
        if (cfg.key) {
            if (env_state == ENV_RELEASE) {
                env_state = ENV_ATTACK;
                env = 0.0f;
            }

            if (env_state == ENV_ATTACK) {
                env += 1.0f/(cfg.attack * SAMPLE_RATE);
                //env += cfg.attack * (1.0f + ENV_OVERSHOOT - env);
                if (env > 1.0f) {
                    env = 1.0f;
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
            if (env < 0.0f) env = 0.0f;
        }

        s = s * env;

        // Filter
        float fc = cfg.cutoff + cfg.env_mod * env;
        float a = PI * fc/SAMPLE_RATE;
        float ria = 1.0f / (1.0f + a);
        float g = a * ria;

        float bigG = g*g*g*g;
        float bigS = g*g*g*z1 + g*g*z2 + g*z3 + z4;
        bigS = bigS * ria;

        float v;
        // zero-delay resonance feedback
        s = (s - cfg.resonance*bigS)/(1.0f + cfg.resonance*bigG);
        
        // 4x 1-pole filters
        v = (s - z1) * g;
        s = v + z1;
        z1 = s + v;

        v = (s - z2) * g;
        s = v + z2;
        z2 = s + v;

        v = (s - z3) * g;
        s = v + z3;
        z3 = s + v;

        v = (s - z4) * g;
        s = v + z4;
        z4 = s + v;


        int16_t s16 = s * 10000;

        out_buffer[i] = s16;   // left
        out_buffer[i+1] = s16; // right
    }

}


void synth_start(void) {

    // Initial config
    cfg.volume = 55;
    cfg.freq = 261.13 / SAMPLE_RATE * UINT32_MAX;
    cfg.osc_wave = WAVE_SINE;
    cfg.attack = 0.0005;
    cfg.decay = 0.005;
    cfg.sustain = 1.0;
    cfg.release = 0.0005;
    cfg.cutoff = 10000.0;
    cfg.resonance = 0.0;
    cfg.env_mod = 0.0;
    cfg.env_curve = -log((1 + ENV_OVERSHOOT) / ENV_OVERSHOOT);
    env_state = ENV_RELEASE;
    memcpy(&cfgnew, &cfg, sizeof(SynthConfig));

    create_wave_tables();
    fill_buffer();

    if (BSP_AUDIO_OUT_Init(OUTPUT_DEVICE_AUTO, cfg.volume, SAMPLE_RATE) != 0) {
        printf("init failed\r\n");
        return;
    }

    BSP_AUDIO_OUT_Play(out_buffer, OUT_BUFFER_SAMPLES * 2);
}




