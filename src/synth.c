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
SeqConfig seq;

typedef struct {
    float z1;
    float z2;
    float z3;
    float z4;
} Filter;

Filter filter[NUM_VOICE];

float nco[NUM_VOICE][NUM_OSCILLATOR];
float    env[NUM_VOICE] = {0.0f};
EnvState env_state[NUM_VOICE] = {ENV_RELEASE};
float lfo_nco;

//float    z1, z2, z3, z4; // filter state
uint32_t next_beat = 0;
int      arp_note = 0;
int seq_note = 0;
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
inline float sample_drums(void);

int bn = 1;
float bnoise(void) {
    bn *= 16807;
    return (float)bn * 4.6566129e-010f;
}




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

    pin_set(GPIOA, 1<<5, 0);

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

    if (i++ > 50) {
        ltime = NOW_US() - start_time;
        loop_time = ltime;
        transfer_time = txtime;
        i = 0;
    }

}

// many things broken
inline void sequencer_update(void) {

    if (start_time > next_beat) {
        next_beat += 15000000/cfg.tempo;

        if (cfg.seq_play) {
            env_state[0] = ENV_RELEASE;
            pin_set(GPIOA, 1<<5, 1);
            seq_note++;
            if (seq_note == NUM_SEQ_NOTES) seq_note = 0;
        }

        // if (cfg.arp) {
        //     //env_state = ENV_RELEASE;
        //     arp_note++;
        //     if ((arp_note == MAX_ARP) || (cfg.arp_freqs[arp_note] == 0)) arp_note = 0;           
        // }
    }

    if (cfg.seq_play) {
        if (seq.note[seq_note] != 0.0f) {
            cfg.key[0] = true;
            cfg.freq[0] = seq.note[seq_note];
        }
    }

    // if (cfg.arp) {
    //     //cfg.key = true;
    //     //cfg.freq = cfg.arp_freqs[arp_note];
    // }


}

/******************************************************************************/
// PolyBLEP (polynomial band-limited step)
// t: phase, [0,1]
// dt: frequency, normalised [0,1]
float polyblep(float t, float dt) {

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

#define ALLPASS_G 0.5f


#define DELAY_LINE(name, length) struct { int idx; float s; float dl[length]; } name; int name##_len = length;
#define DELAY_LINE_PUT(name, value) name.dl[name.idx--] = value; if (name.idx < 0) name.idx += name##_len;
#define DELAY_LINE_GET(name) name.dl[name.idx]
#define COMB_PUT(name, value) {comb_result = DELAY_LINE_GET(name); name.s = comb_result + (name.s - comb_result)*cfg.fx_damping; DELAY_LINE_PUT(name, value + cfg.fx_combg * name.s);}

DELAY_LINE(comb1, 781);
DELAY_LINE(comb2, 831);
DELAY_LINE(comb3, 893);
DELAY_LINE(comb4, 949);
DELAY_LINE(comb5, 995);
DELAY_LINE(comb6, 1043);
DELAY_LINE(comb7, 1089);
DELAY_LINE(comb8, 1131);
//DELAY_LINE(comb8, 6000);

inline void fill_buffer(void) {

    for (int j=0; j<OUT_BUFFER_SAMPLES; j += 2) {
        
        float s = 0.0f;
        float comb_result = 0.0f;

        // // LFO
        // lfo_nco += cfg.lfo_rate;
        // if (lfo_nco > 1.0f) lfo_nco -= 1.0f;
        // float lfo_out = (lfo_nco < 0.5f) ? lfo_nco : 1.0f - lfo_nco;
        // //lfo_out = cfg.lfo_amount*(4.0f * lfo_out - 1.0f);
        // lfo_out = 1.0f + cfg.lfo_amount*(2.0f * lfo_out - 0.5f);


        for (int i=0; i<NUM_VOICE; i++) {
            float osc_mix = 0.0f;

            // Oscillators
            for (int osc=0; osc<NUM_OSCILLATOR; osc++) {

                // Advance oscillator phase according to (detuned) frequency
                float freq = (cfg.osc[osc].detune) * cfg.freq[i];
                //freq *= lfo_out;
                nco[i][osc] += freq;
                if (nco[i][osc] > 1.0f) nco[i][osc] -= 1.0f;
                float phase = nco[i][osc];
                float s1 = 0.0f;

                // Generate waveform from phase
                if (cfg.osc[osc].waveform == WAVE_SAW) {
                    s1 = 2.0f * phase - 1.0f;
                    s1 -= polyblep(phase, freq);

                } else if (cfg.osc[osc].waveform == WAVE_SQUARE) {
                    float phase2;
                    if (phase < 0.5f) {
                        s1 = -1.0f;
                        phase2 = phase + 0.5f;
                    } else {
                        s1 = 1.0f;
                        phase2 = phase - 0.5f;
                    }
                    s1 -= polyblep(phase, freq);
                    s1 += polyblep(phase2, freq);

                } else if (cfg.osc[osc].waveform == WAVE_TRI) {
                    s1 = (phase < 0.5f) ? phase : 1.0f - phase;
                    s1 = 8.0f * s1 - 2.0f;
                    if (s1 > cfg.osc[osc].folding) s1 = cfg.osc[osc].folding - (s1-cfg.osc[osc].folding);
                    if (s1 < -cfg.osc[osc].folding) s1 = -cfg.osc[osc].folding - (s1-cfg.osc[osc].folding);
                }

                osc_mix += s1 * cfg.osc[osc].gain;
            }

            // Noise
            float noise = bnoise();//(float)random_uint16() / 65535;
            osc_mix += noise * cfg.noise_gain;

            // Filter
            float fc = cfg.cutoff + cfg.env_mod * env[i];
            float a = PI * fc/SAMPLE_RATE;
            float ria = 1.0f / (1.0f + a);
            float g = a * ria;
            float gg = g * g;
            float bigS = filter[i].z4 + g*(filter[i].z3 + g*(filter[i].z2 + g*filter[i].z1));
            bigS = bigS * ria;

            float v;
            // zero-delay resonance feedback
            osc_mix = (osc_mix - cfg.resonance*bigS)/(1.0f + cfg.resonance*gg*gg);
            
            // 4x 1-pole filters
            v = (osc_mix - filter[i].z1) * g;
            osc_mix = v + filter[i].z1;
            filter[i].z1 = osc_mix + v;

            v = (osc_mix - filter[i].z2) * g;
            osc_mix = v + filter[i].z2;
            filter[i].z2 = osc_mix + v;

            v = (osc_mix - filter[i].z3) * g;
            osc_mix = v + filter[i].z3;
            filter[i].z3 = osc_mix + v;

            v = (osc_mix - filter[i].z4) * g;
            osc_mix = v + filter[i].z4;
            filter[i].z4 = osc_mix + v;

            // Envelope
            if (cfg.key[i]) {
                if (cfg.key_retrigger[i]) {
                    cfgnew.key_retrigger[i] = false;
                    cfg.key_retrigger[i] = false;
                    env_state[i] = ENV_ATTACK;
                }
                if (env_state[i] == ENV_RELEASE) {
                    env_state[i] = ENV_ATTACK;
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
                if (env[i] > 0.0f) {
                    env[i] -= cfg.release_rate * (ENV_OVERSHOOT + env[i]);
                } else {
                    env[i] = 0.0f;
                }
            }

            osc_mix *= env[i];
            s += osc_mix;

        }

        s /= (float)NUM_VOICE;




        // float fx = 0.0f;

        // COMB_PUT(comb1, s);
        // fx += comb_result;
        // COMB_PUT(comb2, s);
        // fx += comb_result;
        // COMB_PUT(comb3, s);
        // fx += comb_result;
        // COMB_PUT(comb4, s);
        // fx += comb_result;
        // COMB_PUT(comb5, s);
        // fx += comb_result;
        // COMB_PUT(comb6, s);
        // fx += comb_result;
        // COMB_PUT(comb7, s);
        // fx += comb_result;
        // COMB_PUT(comb8, s);
        // fx += comb_result;

        // // fx = DELAY_LINE_GET(comb8);
        // // DELAY_LINE_PUT(comb8, s + fx * 0.5f);

        // fx *= 0.1f;
        // s = fx + s;

        s += sample_drums();

        s += bnoise() * 0.001f; // Dither

        int16_t s16 = s * 700;

        out_buffer[j] = s16;   // left
        out_buffer[j+1] = s16; // right        

    }

}


bool trig_bass;
bool trig_snare;

float nco_bass = 0.0f;
bool bass_attack = false;
float env_bass = 0.0f;
float bass_gain = 3.0f;
float bass_freq = 0.0f;

float nco_snare_lo = 0.0f;
float nco_snare_hi = 0.0f;
float env_snare = 0.0f;
float env_snare2 = 0.0f;

inline float sample_drums(void) {

    float s = 0.0f;
    
    // Bass

    if (trig_bass) {
        nco_bass = 0.0f;
        env_bass = 0.0f;
        trig_bass = false;
        bass_attack = true;
        bass_freq = cfg.bass_pitch;
    }

    nco_bass += bass_freq;
    bass_freq *= (1.0f - cfg.bass_punch);
    if (nco_bass > 1.0f) nco_bass -= 1.0f;
    float part = (nco_bass < 0.5f) ? nco_bass : 1.0f - nco_bass;
    part = 8.0f * part - 2.0f;
    part *= bass_gain;

    if (bass_attack) {
        env_bass += cfg.bass_click;
        if (env_bass > 1.0f) {
            env_bass = 1.0f;
            bass_attack = false;
        }
    } else {
        env_bass -= cfg.bass_decay * (ENV_OVERSHOOT + env_bass);
        if (env_bass < 0.0f) env_bass = 0.0f;        
    }

    s = part * env_bass;

    // Snare

    if (trig_snare) {
        env_snare = 1.0f;
        env_snare2 = 1.0f;
        trig_snare = false;
    }

    // 180 Hz
    nco_snare_lo += 0.00408f;
    if (nco_snare_lo > 1.0f) nco_snare_lo -= 1.0f;
    float snare_lo = (nco_snare_lo < 0.5f) ? nco_snare_lo : 1.0f - nco_snare_lo;
    snare_lo = 8.0f * snare_lo - 2.0f;    

    // 330 Hz
    nco_snare_hi += 0.00748f;
    if (nco_snare_hi > 1.0f) nco_snare_hi -= 1.0f;
    float snare_hi = (nco_snare_hi < 0.5f) ? nco_snare_hi : 1.0f - nco_snare_hi;
    snare_hi = 8.0f * snare_hi - 2.0f;

    // Tone mix
    part = cfg.snare_tone * snare_hi + (1.0f - cfg.snare_tone) * snare_lo;
    env_snare -= 2.0f * cfg.snare_decay * (ENV_OVERSHOOT + env_snare);
    if (env_snare < 0.0f) env_snare = 0.0f;        
    s += part * env_snare;

    // Noise
    env_snare2 -= cfg.snare_decay * (ENV_OVERSHOOT + env_snare2);
    if (env_snare2 < 0.0f) env_snare2 = 0.0f;            
    s += bnoise() * env_snare2;




    
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

    cfg.tempo = 30;

    cfg.osc[0].waveform = WAVE_SQUARE;
    cfg.osc[0].folding = 2.0f;
    cfg.osc[0].gain = 1.0f;
    cfg.osc[0].detune = 1.0f;

    cfg.osc[1].waveform = WAVE_SQUARE;
    cfg.osc[1].folding = 2.0f;
    cfg.osc[1].gain = 0.5f;
    cfg.osc[1].detune = 1.0f;    

    cfg.noise_gain = 0.0f;

    cfg.attack_rate  = 0.0005;
    cfg.decay_rate   = 0.005;
    cfg.sustain_level = 1.0;
    cfg.release_rate = 0.0005;
    
    cfg.cutoff  = 10000.0;
    cfg.resonance = 0.0;
    cfg.env_mod = 0.0;
    cfg.env_curve = -log((1 + ENV_OVERSHOOT) / ENV_OVERSHOOT);

    cfg.fx_damping = 0.5;
    cfg.fx_combg = 0.881678f;

    cfg.lfo_rate = 1.0f;
    cfg.lfo_amount = 0.01f;

    memcpy(&cfgnew, &cfg, sizeof(SynthConfig));

    
    fill_buffer();

    audio_init(SAMPLE_RATE);
    BSP_AUDIO_OUT_Play(out_buffer, OUT_BUFFER_SAMPLES * 2);

}




