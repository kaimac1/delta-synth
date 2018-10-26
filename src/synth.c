#include "synth.h"
#include "board.h"
#include "audio.h"
#include <math.h>
#include <string.h>
#include "stm32f4xx_ll_tim.h"
#include "notes.h"
#define PI 3.1415926f

SynthConfig synth;
SynthConfig cfg;
SeqConfig seq;

typedef struct {
    float z1;
    float z2;
    float z3;
    float z4;
} Filter;

typedef struct {
    float level;
    EnvState state;
} Envelope;

Filter filter[NUM_VOICE];
Envelope env[NUM_VOICE][NUM_ENV];
float nco[NUM_VOICE][NUM_OSCILLATOR];
float lfo_nco[NUM_LFO];

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

    // Copy new settings: synth -> cfg
    if (!synth.busy) {
        memcpy(&cfg, &synth, sizeof(SynthConfig));
        // if (cfg.env_retrigger) {
        //     cfgnew.env_retrigger = false;
        //     cfg.env_retrigger = false;
        //     env_state = ENV_RELEASE;
        // }
    }

    //sequencer_update();
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
            //env[0].state = ENV_RELEASE;
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
// SYNTHESIS BLOCKS
/******************************************************************************/

/******************************************************************************/
// 4-pole low-pass filter.
// cutoff is in Hz.
// peak : 0-3.99 (resonance)
inline float ladder_filter(Filter * f, float in, float cutoff, float peak) {

    float a = PI * cutoff/SAMPLE_RATE;
    float ria = 1.0f / (1.0f + a);
    float g = a * ria;
    float gg = g * g;
    float bigS = f->z4 + g*(f->z3 + g*(f->z2 + g*f->z1));
    bigS = bigS * ria;

    float v;
    // zero-delay resonance feedback
    in = (in - peak*bigS)/(1.0f + peak*gg*gg);
    
    // 4x 1-pole filters
    v = (in - f->z1) * g;
    in = v + f->z1;
    f->z1 = in + v;

    v = (in - f->z2) * g;
    in = v + f->z2;
    f->z2 = in + v;

    v = (in - f->z3) * g;
    in = v + f->z3;
    f->z3 = in + v;

    v = (in - f->z4) * g;
    in = v + f->z4;
    f->z4 = in + v;

    return in;

}


// Basic bandpass filter (unused, was used for drums)
inline float bandpass(Filter *f, float in, float cutoff) {

    float a = PI * cutoff/SAMPLE_RATE;
    float ria = 1.0f / (1.0f + a);
    float g = a*ria;

    float hpout = (in - f->z1) * g;       
    float out = hpout + f->z1;
    f->z1 = out + hpout;

    float v = (hpout - f->z2) * g;
    out = v + f->z2;
    f->z2 = out + v;

    return out;
}


/******************************************************************************/
// White noise
int wn = 1;
inline float whitenoise(void) {
    wn *= 16807;
    return (float)wn * 4.6566129e-010f;
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


/******************************************************************************/
// ADSR envelope generator
inline float envelope(Envelope *e, bool gate, ADSR *adsr) {

    if (gate) {
        if (e->state == ENV_RELEASE) {
            e->state = ENV_ATTACK;
        }

        if (e->state == ENV_ATTACK) {
            e->level += adsr->attack;
            if (e->level > 1.0f) {
                e->level = 1.0f;
                e->state = ENV_DECAY;
            }
        } else if (e->state == ENV_DECAY) {
            e->level += adsr->decay * (adsr->sustain - ENV_OVERSHOOT - e->level);
            if (e->level < adsr->sustain) {
                e->level = adsr->sustain;
                e->state = ENV_SUSTAIN;
            }
        } else if (e->state == ENV_SUSTAIN) {
            e->level = adsr->sustain;
        }

    } else {
        e->state = ENV_RELEASE;
        if (e->level > 0.0f) {
            e->level -= adsr->release * (ENV_OVERSHOOT + e->level);
        } else {
            e->level = 0.0f;
        }
    }

    return e->level;

}

/******************************************************************************/
// Reverb and delay effects

#define ALLPASS_G 0.5f
#define DELAY_LINE(name, length) struct { int idx; float s; float dl[length]; } name; int name##_len = length;
#define DELAY_LINE_PUT(name, value) name.dl[name.idx--] = value; if (name.idx < 0) name.idx += name##_len;
#define DELAY_LINE_GET(name) name.dl[name.idx]
#define COMB_PUT(name, value) {comb_result = DELAY_LINE_GET(name); name.s = comb_result + (name.s - comb_result)*cfg.fx_damping; DELAY_LINE_PUT(name, value + cfg.fx_combg * name.s);}
#define ALLPASS_PUT(name, value) {allpass_result = DELAY_LINE_GET(name) + ALLPASS_G * value; DELAY_LINE_PUT(name, value - ALLPASS_G * allpass_result);}

DELAY_LINE(allpass1, 225);
DELAY_LINE(allpass2, 556);
DELAY_LINE(allpass3, 441);
DELAY_LINE(allpass4, 341);
DELAY_LINE(comb1, 1116);
DELAY_LINE(comb2, 1188);
DELAY_LINE(comb3, 1277);
DELAY_LINE(comb4, 1356);
DELAY_LINE(comb5, 1422);
DELAY_LINE(comb6, 1491);
DELAY_LINE(comb7, 1557);
DELAY_LINE(comb8, 1617);
DELAY_LINE(delay1, 5000);

inline float reverb(float in) {

    float out = 0.0f;
    float comb_result = 0.0f;
    float allpass_result = 0.0f;    

    // Parallel comb filter bank
    COMB_PUT(comb1, in);
    out += comb_result;
    COMB_PUT(comb2, in);
    out += comb_result;
    COMB_PUT(comb3, in);
    out += comb_result;
    COMB_PUT(comb4, in);
    out += comb_result;
    COMB_PUT(comb5, in);
    out += comb_result;
    COMB_PUT(comb6, in);
    out += comb_result;
    COMB_PUT(comb7, in);
    out += comb_result;
    COMB_PUT(comb8, in);
    out += comb_result;

    // Serial allpass filters
    ALLPASS_PUT(allpass1, out);
    ALLPASS_PUT(allpass2, allpass_result);
    ALLPASS_PUT(allpass3, allpass_result);
    ALLPASS_PUT(allpass4, allpass_result);

    // Wet/dry mix    
    out = cfg.fx_wet * allpass_result + in;
    return out;

}

float xn1 = 0.0f;
float yn1 = 0.0f;

inline float oscillator_square(float phase, float freq, float mod) {

    float out;

    float width = 0.5f + 0.5f*mod;
    float phase2;
    if (phase < width) {
        out = -1.0f;
        phase2 = phase + (1.0f-width);
    } else {
        out = 1.0f;
        phase2 = phase - width;
    }
    out -= polyblep(phase, freq);
    out += polyblep(phase2, freq);
    return out;

}

inline float oscillator_tri(float phase, float freq, float mod) {

    float out;
    out = (phase < 0.5f) ? phase : 1.0f - phase;
    out = 8.0f * out - 2.0f;
    float fold = 2.0f * mod;
    if (out > fold) out = fold - (out-fold);
    else if (out < -fold) out = -fold - (out-fold);
    return out;

}

inline float oscillator_saw(float phase, float freq, float mod) {

    float out = 2.0f * phase - 1.0f;
    out -= polyblep(phase, freq);
    return out;

}


/******************************************************************************/
// SAMPLE GENERATION
/******************************************************************************/

inline void fill_buffer(void) {

    float pitch_both = 1.0f;
    float mod_both = 0.0f;
    float dummy = 0.0f;
    float *deste1 = &dummy;

    switch (cfg.env_dest[1]) {
        case ENVDEST_PITCH_BOTH:
            deste1 = &pitch_both;
            break;
        case ENVDEST_MOD_BOTH:
            deste1 = &mod_both;
            break;
        default:
            break;
    }

    if (cfg.env_dest[1] == ENVDEST_PITCH_BOTH) {
        deste1 = &pitch_both;
    }

    for (int j=0; j<OUT_BUFFER_SAMPLES; j += 2) {
        
        float s = 0.0f;
        // float lfo_out = 0.0f;

        // // LFO
        // for (int id=0; id<NUM_LFO; id++) {
        //     lfo_nco[id] += cfg.lfo[id].rate;
        //     if (lfo_nco[id] > 1.0f) lfo_nco[id] -= 1.0f;
        //     lfo_out = (lfo_nco[id] < 0.5f) ? lfo_nco[id] : 1.0f - lfo_nco[id];
        //     //lfo_out = cfg.lfo_amount*(4.0f * lfo_out - 1.0f);
        //     lfo_out = 1.0f + cfg.lfo[id].amount*(2.0f * lfo_out - 0.5f);
        // }

        for (int voice=0; voice<NUM_VOICE; voice++) {
            float mix = 0.0f;
            pitch_both = 1.0f;
            mod_both = 0.0f;

            // Envelopes
            for (int e=0; e<NUM_ENV; e++) {
                if (cfg.key[voice] && cfg.key_retrigger[voice]) {
                    synth.key_retrigger[voice] = false;
                    cfg.key_retrigger[voice] = false;
                    env[voice][e].state = ENV_ATTACK;
                }
                envelope(&env[voice][e], cfg.key[voice], &cfg.env[e]);
            }

            *deste1 += env[voice][1].level * cfg.env_amount[1];

            // Oscillators
            for (int osc=0; osc<NUM_OSCILLATOR; osc++) {

                float mod = mod_both + cfg.osc[osc].modifier;
                if (mod > 1.0f) mod = 1.0f;
                if (mod < 0.0f) mod = 0.0f;                

                // Advance oscillator phase according to (detuned) frequency
                float freq = pitch_both * cfg.osc[osc].detune * cfg.freq[voice];
                nco[voice][osc] += freq;
                if (nco[voice][osc] > 1.0f) nco[voice][osc] -= 1.0f;
                float phase = nco[voice][osc];
                float s1;

                switch (cfg.osc[osc].waveform) {
                    case WAVE_SAW:
                        s1 = oscillator_saw(phase, freq, mod);
                        break;
                    case WAVE_SQUARE:
                        s1 = oscillator_square(phase, freq, mod);
                        break;
                    case WAVE_TRI:
                        s1 = oscillator_tri(phase, freq, mod);
                        break;
                    default:
                        break;
                }
                
                // Sum oscillators
                mix += (osc == 0) ? ((1.0f - cfg.osc_balance) * s1) : (cfg.osc_balance * s1);
            }

            // Noise
            float noise = whitenoise();
            mix += noise * cfg.noise_gain;

            // Filter
            float fc = cfg.cutoff + cfg.env_mod * env[voice][1].level;
            mix = ladder_filter(&filter[voice], mix, fc, cfg.resonance);

            // Amp
            mix *= env[voice][0].level;
            s += mix;

        }

        s /= (float)NUM_VOICE;

        // Dither
        s += whitenoise() * 0.001f;

        // Effects
        s = reverb(s);

        // Delay
        // fx = DELAY_LINE_GET(delay1);
        // DELAY_LINE_PUT(delay1, s + fx * 0.75f);
        // s += fx;

        // DC filter
        const float r = 0.997f;
        float y = s - xn1 + r * yn1;
        xn1 = s;
        yn1 = y;

        // Ear protection :)
        if (y > 30.0f) y = 30.0f + (y-30.0f)*0.1f;
        if (y < -30.0f) y = -30.0f - (-30.0f-y)*0.1f;
        int16_t s16 = y * 500;

        out_buffer[j] = s16;   // left
        out_buffer[j+1] = s16; // right        

    }

}

/*bool trig_bass;
bool trig_snare;
bool trig_clap;
bool trig_hat_cl;
bool trig_hat_op;

float nco_bass = 0.0f;
bool bass_attack = false;
float env_bass = 0.0f;
float bass_gain = 3.0f;
float bass_freq = 0.0f;

float nco_snare_lo = 0.0f;
float nco_snare_hi = 0.0f;
float env_snare_tone = 0.0f;
float env_snare_noise = 0.0f;

float env_clap = 0.0f;
int clap_num = 0;
int clap_ctr = 0;
Filter fclap;

float env_hat = 0.0f;
bool hat_open;
Filter fhat;

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
        env_snare_tone = 1.0f;
        env_snare_noise = 1.0f;
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
    env_snare_tone -= 2.0f * cfg.snare_decay * (ENV_OVERSHOOT + env_snare_tone);
    if (env_snare_tone < 0.0f) env_snare_tone = 0.0f;        
    s += part * env_snare_tone;

    // Noise
    env_snare_noise -= cfg.snare_decay * (ENV_OVERSHOOT + env_snare_noise);
    if (env_snare_noise < 0.0f) env_snare_noise = 0.0f;            
    s += whitenoise() * env_snare_noise;

    // Clap

    if (trig_clap) {
        trig_clap = false;
        env_clap = 1.0f;
        clap_num = 0;
        clap_ctr = 0;
    }

    env_clap -= cfg.clap_decay * (ENV_OVERSHOOT + env_clap);
    float thresh = 0.4f / (cfg.clap_decay/0.0005f);
    if (env_clap < 0.0f) env_clap = 0.0f;
    if (clap_num < 3) {
        clap_ctr++;
        if (clap_ctr > thresh * SAMPLE_RATE * 0.05f) {
            clap_ctr = 0;
            clap_num++;
            env_clap = 1.0f;
        }
    }

    float c = 60.0f * bandpass(&fclap, whitenoise(), cfg.clap_filt);
    s += c * env_clap;


    // Hi hat (closed)

    if (trig_hat_cl) {
        trig_hat_cl = false;
        hat_open = false;
        env_hat = 1.0f;
    }
    else if (trig_hat_op) {
        trig_hat_op = false;
        hat_open = true;
        env_hat = 1.0f;
    }

    float dec = hat_open ? 0.0004f : 0.002f;
    env_hat -= dec * (ENV_OVERSHOOT + env_hat);
    if (env_hat < 0.0f) env_hat = 0.0f;
    //s += 0.5f * whitenoise() * env_hat;
    s += 2.0f * bandpass(&fhat, whitenoise(), 15000.0f) * env_hat;

    return s;

}*/


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
    cfg.osc[0].modifier = 0.0f;
    cfg.osc[0].detune = 1.0f;
    cfg.osc[1].waveform = WAVE_SQUARE;
    cfg.osc[1].modifier = 0.0f;
    cfg.osc[1].detune = 1.0f;    
    cfg.osc_balance = 0.5f;
    cfg.noise_gain = 0.0f;

    cfg.env[0].attack  = 0.0005;
    cfg.env[0].decay   = 0.005;
    cfg.env[0].sustain = 1.0;
    cfg.env[0].release = 0.0005;
    
    cfg.env[1].attack  = 0.0005;
    cfg.env[1].decay   = 0.005;
    cfg.env[1].sustain = 1.0;
    cfg.env[1].release = 0.0005;

    cfg.env_dest[0] = ENVDEST_AMP;
    cfg.env_amount[0] = 1.0f;
    cfg.env_dest[1] = ENVDEST_PITCH_BOTH;
    cfg.env_amount[1] = 1.0f;

    cfg.cutoff  = 10000.0;
    cfg.resonance = 0.0;
    cfg.env_mod = 0.0;
    
    cfg.fx_damping = 0.3f;
    cfg.fx_combg = 0.881678f;
    cfg.fx_wet = 0.5f;

    cfg.lfo[0].rate = 0.0f;
    cfg.lfo[0].amount = 0.0f;

    memcpy(&synth, &cfg, sizeof(SynthConfig));

    
    fill_buffer();

    audio_init(SAMPLE_RATE);
    BSP_AUDIO_OUT_Play(out_buffer, OUT_BUFFER_SAMPLES * 2);

}




