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

float nco[NUM_VOICE][NUM_OSCILLATOR];
float    env[NUM_VOICE] = {0.0f};
EnvState env_state[NUM_VOICE] = {ENV_RELEASE};

float    z1, z2, z3, z4; // filter state
uint32_t next_beat = 0;
int      arp_note = 0;
uint32_t start_time;
uint32_t loop_time;
uint32_t transfer_time;

// Output buffer
#define OUT_BUFFER_SAMPLES 512
uint16_t out_buffer_1[OUT_BUFFER_SAMPLES];
uint16_t out_buffer_2[OUT_BUFFER_SAMPLES];
uint16_t *out_buffer = out_buffer_1;

// Sine table
int16_t sine_table[SINE_TABLE_SIZE];

inline void sequencer_update(void);
inline void fill_buffer(void);
inline float sample_synth(void);
inline float sample_drums(void);

/******************************************************************************/
// This ISR is called when the DMA transfer of one buffer (A) completes.
// We need to immediately swap the DMA over to the other buffer (B), and start
// refilling the buffer that was just sent (A).
void HAL_I2S_TxCpltCallback(I2S_HandleTypeDef *hi2s) {

    static uint32_t txtime = 0;
    static uint32_t ltime = 0;
    static int i = 0;

    uint32_t now = NOW_US();
    txtime = now - start_time;
    start_time = now;

    audio_change_buffer(out_buffer, OUT_BUFFER_SAMPLES);
    out_buffer = (out_buffer == out_buffer_1) ? out_buffer_2 : out_buffer_1;

    // Copy new settings: cfgnew -> cfg
    if (!cfgnew.busy) {
        memcpy(&cfg, &cfgnew, sizeof(SynthConfig));
        // if (cfg.env_retrigger) {
        //     cfgnew.env_retrigger = false;
        //     cfg.env_retrigger = false;
        //     env_state = ENV_RELEASE;
        // }
    }

    sequencer_update();
    fill_buffer();

    if (i++ > 100) {
        ltime = NOW_US() - start_time;
        loop_time = ltime;
        transfer_time = txtime;
        i = 0;
    }

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
        //s += sample_drums();

        int16_t s16 = s * 1000;
        out_buffer[i] = s16;   // left
        out_buffer[i+1] = s16; // right
    }

}


/******************************************************************************/
// PolyBLEP (polynomial band-limited step)
// t: phase, [0,1]
// dt: frequency, normalised [0,1]
inline float polyblep(float t, float dt) {

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

    for (int i=0; i<NUM_VOICE; i++) {
        float osc_mix = 0.0f;

        // Oscillators
        for (int osc=0; osc<NUM_OSCILLATOR; osc++) {

            // Advance oscillator phase according to (detuned) frequency
            float freq = cfg.osc[osc].detune * cfg.freq[i];
            nco[i][osc] += freq;
            if (nco[i][osc] > 1.0f) nco[i][osc] -= 1.0f;
            float phase = nco[i][osc];
            float s1 = 0.0f;

            // Generate waveform from phase
            if (cfg.osc[osc].waveform == WAVE_SAW) {
                s1 = 2.0f * phase - 1.0f;
                s1 -= polyblep(phase, freq);

            } else if (cfg.osc[osc].waveform == WAVE_SQUARE) {
                s1 = (phase < 0.5f) ? -1.0f : 1.0f;
                s1 -= polyblep(phase, freq);
                float phase2 = (phase > 0.5f) ? phase-0.5f : phase+0.5f;
                s1 += polyblep(phase2, freq);

            } else if (cfg.osc[osc].waveform == WAVE_TRI) {
                s1 = (phase < 0.5f) ? phase : 1.0f - phase;
                s1 = 8.0f * s1 - 2.0f;
                if (s1 > cfg.osc[osc].folding) s1 = cfg.osc[osc].folding - (s1-cfg.osc[osc].folding);
                if (s1 < -cfg.osc[osc].folding) s1 = -cfg.osc[osc].folding - (s1-cfg.osc[osc].folding);
            }

            s1 *= cfg.osc[osc].gain;
            osc_mix += s1;
        }

        // Envelope
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

        s += osc_mix * env[i];

    }
    s /= (float)NUM_VOICE;


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
        sine_table[i] = (int16_t)(sinf(arg) * 32767);
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

    cfg.osc[0].waveform = WAVE_SQUARE;
    cfg.osc[0].folding = 2.0f;
    cfg.osc[0].gain = 1.0f;
    cfg.osc[0].detune = 1.0f;

    cfg.osc[1].waveform = WAVE_SQUARE;
    cfg.osc[1].folding = 2.0f;
    cfg.osc[1].gain = 0.5f;
    cfg.osc[1].detune = 1.0f;    

    cfg.osc[2].waveform = WAVE_SQUARE;
    cfg.osc[2].folding = 2.0f;
    cfg.osc[2].gain = 0.5f;
    cfg.osc[2].detune = 1.0f;        

    cfg.attack_rate  = 0.0005;
    cfg.decay_rate   = 0.005;
    cfg.sustain_level = 1.0;
    cfg.release_rate = 0.0005;
    
    cfg.cutoff  = 10000.0;
    cfg.resonance = 0.0;
    cfg.env_mod = 0.0;
    cfg.env_curve = -log((1 + ENV_OVERSHOOT) / ENV_OVERSHOOT);
    memcpy(&cfgnew, &cfg, sizeof(SynthConfig));

    
    fill_buffer();

    audio_init(SAMPLE_RATE);
    BSP_AUDIO_OUT_Play(out_buffer, OUT_BUFFER_SAMPLES * 2);

}




