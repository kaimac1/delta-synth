#include "synth.h"
#include "board.h"
#include "audio.h"
#include <math.h>
#include <string.h>
#include "stm32f4xx_ll_tim.h"
#include "notes.h"
#define PI 3.1415926f

SynthConfig cfgnew;
SynthConfig cfg;

uint32_t nco[NUM_OSC];
float    env[NUM_OSC] = {0.0f};
EnvState env_state[NUM_OSC] = {ENV_RELEASE};

float    z1, z2, z3, z4; // filter state
uint32_t next_beat = 0;
int      arp_note = 0;
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

inline void sequencer_update(void);
inline void fill_buffer(void);
inline float sample_synth(void);
inline float sample_drums(void);

/******************************************************************************/
// This ISR is called when the DMA transfer of one buffer (A) completes.
// We need to immediately swap the DMA over to the other buffer (B), and start
// refilling the buffer that was just sent (A).
void BSP_AUDIO_OUT_TransferComplete_CallBack(void) {
 
    BSP_AUDIO_OUT_ChangeBuffer(out_buffer, OUT_BUFFER_SAMPLES);
    out_buffer = (out_buffer == out_buffer_1) ? out_buffer_2 : out_buffer_1;

    start_time = LL_TIM_GetCounter(TIM2);

    // Copy new settings: cfgnew -> cfg
    if (!cfgnew.busy) {

        // Volume level changed
        if (cfgnew.volume != cfg.volume) {
            //BSP_AUDIO_OUT_SetVolume(cfgnew.volume);
        }

        memcpy(&cfg, &cfgnew, sizeof(SynthConfig));
        // if (cfg.env_retrigger) {
        //     cfgnew.env_retrigger = false;
        //     cfg.env_retrigger = false;
        //     env_state = ENV_RELEASE;
        // }
    }

    sequencer_update();
    fill_buffer();
    loop_time = LL_TIM_GetCounter(TIM2) - start_time;

    //BSP_LED_Off(LED3);

}

// many things broken
inline void sequencer_update(void) {

    if (start_time > next_beat) {
        //BSP_LED_On(LED3);
        next_beat += 15000000/cfg.tempo;


        if (cfg.arp) {
            //env_state = ENV_RELEASE;
            arp_note++;
            if ((arp_note == MAX_ARP) || (cfg.arp_freqs[arp_note] == 0)) arp_note = 0;           
        }
    }

    if (cfg.arp) {
        //cfg.key = true;
        //cfg.freq = cfg.arp_freqs[arp_note];
    }


}

inline void fill_buffer(void) {

    float s;

    for (int i=0; i<OUT_BUFFER_SAMPLES; i += 2) {
        
        s = sample_synth();
        //s = 0.0f;
        //s += sample_drums();

        int16_t s16 = s * 1000;
        out_buffer[i] = s16;   // left
        out_buffer[i+1] = s16; // right
    }

}


/******************************************************************************/
// PolyBLEP (polynomial band-limited step)
// t: phase, [0,1]
// f: frequency, normalised
inline float polyblep(float t, uint32_t f) {

    float dt = (float)f / UINT32_MAX; // 0-1

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


inline float sample_synth(void) {

    float s = 0.0f;

    // Oscillators / envelopes
    for (int i=0; i<NUM_OSC; i++) {
        nco[i] += cfg.osc_freq[i];
        float phase = (float)nco[i] / UINT32_MAX;
        float s1 = 2.0f * phase - 1.0f;
        s1 -= polyblep(phase, cfg.osc_freq[i]);

        if (cfg.key[i]) {
            if (env_state[i] == ENV_RELEASE) {
                env_state[i] = ENV_ATTACK;
                env[i] = 0.0f;
            }

            if (env_state[i] == ENV_ATTACK) {
                env[i] += cfg.attack_rate;
                if (env[i] > 1.0f) {
                    env[i] = 1.0f;
                    env_state[i] = ENV_DECAY;
                }
            } else if (env_state[i] == ENV_DECAY) {
                env[i] += cfg.decay_rate * (cfg.sustain_level - ENV_OVERSHOOT - env[i]);
                if (env[i] < cfg.sustain_level) {
                    env[i] = cfg.sustain_level;
                    env_state[i] = ENV_SUSTAIN;
                }
            } else if (env_state[i] == ENV_SUSTAIN) {
                env[i] = cfg.sustain_level;
            }

        } else {
            env_state[i] = ENV_RELEASE;
            env[i] += cfg.release_rate * (-ENV_OVERSHOOT - env[i]);
            if (env[i] < 0.0f) env[i] = 0.0f;
        }

        s += s1 * env[i];

    }
    s /= (float)NUM_OSC;

    // Oscillator
    // uint32_t old_nco1 = nco1;
    // nco1 += cfg.freq;
    // if (cfg.sync && nco1 < old_nco1) nco2 = 0;
    // phase = (float)nco1 / UINT32_MAX;
    // s1 = 2.0f * phase - 1.0f;
    // s1 -= polyblep(phase, cfg.freq);

    // uint32_t freq2 = (uint32_t)(cfg.detune * (float)(cfg.freq));

    // nco2 += freq2;
    // phase = (float)nco2 / UINT32_MAX;
    // s2 = 2.0f * phase - 1.0f;
    // s2 -= polyblep(phase, freq2);

    // s = (s1 + s2) / 2.0f;


    // Filter
    float fc = cfg.cutoff;// + cfg.env_mod * env;
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

    return s;

}

inline float sample_drums(void) {

    float s;

    s = 0.0f;
    return s;

}


void create_wave_tables(void) {

    // Sine
    for (int i=0; i<SINE_TABLE_SIZE; i++) {
        float arg = 2*PI*i / SINE_TABLE_SIZE;
        sine_table[i] = (int16_t)(sinf(arg) * 32768);
    }
}


void synth_start(void) {

    // Initial config
    cfg.busy    = false;
    cfg.volume  = 0;
    cfg.legato  = false;
    cfg.arp     = ARP_OFF;
    cfg.arp_freqs[0] = 0;
    cfg.arp_freqs[1] = 0;
    cfg.arp_freqs[2] = 0;
    cfg.arp_freqs[3] = 0;
    cfg.arp_freqs[4] = 0;

    cfg.tempo = 120;
    cfg.detune = 1.0f;
    cfg.sync = false;

    cfg.attack_rate  = 0.0005;
    cfg.decay_rate   = 0.005;
    cfg.sustain_level = 1.0;
    cfg.release_rate = 0.0005;
    
    cfg.cutoff  = 10000.0;
    cfg.resonance = 0.0;
    cfg.env_mod = 0.0;
    cfg.env_curve = -log((1 + ENV_OVERSHOOT) / ENV_OVERSHOOT);
    memcpy(&cfgnew, &cfg, sizeof(SynthConfig));

    create_wave_tables();
    fill_buffer();

    if (BSP_AUDIO_OUT_Init(SAMPLE_RATE) != 0) {
        printf("init failed\r\n");
        return;
    }

    BSP_AUDIO_OUT_Play(out_buffer, OUT_BUFFER_SAMPLES * 2);
}




