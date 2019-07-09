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

Filter filter[NUM_PART];
Envelope env[NUM_PART][NUM_ENV];
float nco[NUM_PART][NUM_OSCILLATOR];
float lfo_nco[NUM_PART];

// profiling
uint32_t start_time;
uint32_t loop_time;
uint32_t transfer_time;

// Sequencer
SeqConfig seq;
uint32_t next_beat = 0;
int seq_step = 0;

// Output buffer
#define OUT_BUFFER_SAMPLES 512
uint16_t out_buffer_1[OUT_BUFFER_SAMPLES];
uint16_t out_buffer_2[OUT_BUFFER_SAMPLES];
uint16_t *out_buffer = out_buffer_1;

// Lookup tables
#define EXP_TABLE_SIZE 1024
float exp_table[EXP_TABLE_SIZE];

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
            env[0][0].state = ENV_RELEASE;
            env[0][1].state = ENV_RELEASE;
            seq_step++;
            if (seq_step == seq.length) seq_step = 0;
        }
    }

    if (cfg.seq_play) {
        if (seq.step[seq_step].freq != 0.0f) {
            cfg.part[0].gate = true;
            cfg.part[0].freq = seq.step[seq_step].freq;
        }
    }

}


/******************************************************************************/
// SYNTHESIS BLOCKS
/******************************************************************************/

/******************************************************************************/
// 4-pole low-pass filter.
// cutoff: 0-1 (normalised freq)
// peak : 0-4 (resonance)
inline float ladder_filter(Filter * f, float in, float cutoff, float peak) {

    float a = PI * cutoff;
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

inline float squ(float x) {
    return x*x;
}

float polyblep(float t, float dt) {

    if (t < dt) {
        return -squ(t / dt - 1.0f);
        //t /= dt;
        //return t + t - t*t - 1.0f;
    } else if (t > 1.0f - dt) {
        return squ((t-1.0f) / dt + 1.0f);
        //t = (t - 1.0f)/dt;
        //return t*t + t + t + 1.0f;
    } else {
        return 0.0f;
    }
}


/******************************************************************************/
// Oscillators

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


/******************************************************************************/
// SAMPLE GENERATION
/******************************************************************************/

float dest_amp;
float dest_freq;
float dest_mod;
float dest_mod1;
float dest_res;
float dest_cutoff;
float dest_osc1;
float *dests[] = {&dest_amp, &dest_freq, &dest_mod, &dest_mod1, &dest_res, &dest_cutoff, &dest_osc1};

inline void fill_buffer(void) {

    // Envelope retrigger
    for (int p=0; p<NUM_PART; p++) {
        if (cfg.part[p].gate && cfg.part[p].trig) {
            synth.part[p].trig = false;
            cfg.part[p].trig = false;
            env[p][0].state = ENV_ATTACK;
            env[p][1].state = ENV_ATTACK;
        }
    }

    // Modulation routing
    float *deste0 = dests[cfg.part[0].env_dest[0]];
    float *deste1 = dests[cfg.part[0].env_dest[1]];
    float *destl0 = dests[cfg.part[0].lfo.dest];

    // If neither envelope goes to the amp, the amp is gated
    bool gate_mode = ((deste0 != &dest_amp) && (deste1 != &dest_amp));

    for (int j=0; j<OUT_BUFFER_SAMPLES; j += 2) {
        
        float s = 0.0f;

        for (int p=0; p<NUM_PART; p++) {
            float mix = 0.0f;
            dest_freq = 1.0f;
            dest_mod = 0.0f;
            dest_mod1 = 0.0f;
            dest_amp = 0.0f;
            dest_res = 0.0f;
            dest_cutoff = 0.0f;
            dest_osc1 = 0.0f;

            // Update envelopes.
            // Route envelopes to their destinations.
            envelope(&env[p][0], cfg.part[p].gate, &cfg.part[p].env[0]);
            *deste0 += env[p][0].level * cfg.part[p].env_amount[0];
            envelope(&env[p][1], cfg.part[p].gate, &cfg.part[p].env[1]);
            *deste1 += env[p][1].level * cfg.part[p].env_amount[1];

            if (gate_mode) dest_amp = 1.0f * cfg.part[p].gate;

            // LFO
            lfo_nco[p] += cfg.part[p].lfo.rate;
            if (lfo_nco[p] > 1.0f) lfo_nco[p] -= 1.0f;
            float lfo_out = (lfo_nco[p] < 0.5f) ? lfo_nco[p] : 1.0f - lfo_nco[p];
            lfo_out = cfg.part[p].lfo.amount*(4.0f * lfo_out - 1.0f);
            if (destl0 == &dest_amp) {
                *destl0 *= (1.0f + lfo_out);
            } else {
                *destl0 += lfo_out;
            }

            // Oscillators
            for (int osc=0; osc<NUM_OSCILLATOR; osc++) {
                // Advance oscillator phase according to (detuned) frequency
                float freq = dest_freq * cfg.part[p].osc[osc].detune * cfg.part[p].freq;
                nco[p][osc] += freq;
                if (nco[p][osc] > 1.0f) nco[p][osc] -= 1.0f;
                float phase = nco[p][osc];
                float s1 = 0.0f;

                float mod = dest_mod + cfg.part[p].osc[osc].modifier;
                if (osc == 0) mod += dest_mod1;
                if (mod > 1.0f) mod = 1.0f;
                if (mod < 0.0f) mod = 0.0f;

                switch (cfg.part[p].osc[osc].waveform) {
                    case WAVE_SAW:
                        s1 = oscillator_saw(phase, freq, mod);
                        break;
                    case WAVE_SQUARE:
                        s1 = oscillator_square(phase, freq, mod);
                        break;
                    case WAVE_TRI:
                        s1 = oscillator_tri(phase, freq, mod);
                        break;
                    case WAVE_NOISE:
                        s1 = whitenoise();
                    default:
                        break;
                }
                
                // Sum oscillators
                float gain = cfg.part[p].osc[osc].gain;
                if (osc == 0) gain += dest_osc1;
                mix += gain * s1;
            }

            // Filter
            float fc = cfg.part[p].cutoff + cfg.part[p].env_mod * env[p][1].level + dest_cutoff;
            fc = 0.15f * fc * exp_lookup(fc);
            //fc = 2.0f * cfg.part[p].freq * cfg.part[p].keyboard_track;
            float res = cfg.part[p].resonance + 4.0f * dest_res;
            if (res > 4.0f) res = 4.0f;
            else if (res < 0.0f) res = 0.0f;
            mix = ladder_filter(&filter[p], mix, fc, res);

            // Amp
            mix *= dest_amp;
            s += mix;

        }

        s /= (float)NUM_PART;

        // Dither
        s += whitenoise() * 0.001f;

        // Effects
        //s = reverb(s);

        // Delay
        // float fx = DELAY_LINE_GET(delay1);
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
        int16_t s16 = y * cfg.volume * 1000.0f;

        out_buffer[j] = s16;   // left
        out_buffer[j+1] = s16; // right        

    }

}


/******************************************************************************/
// SETUP
/******************************************************************************/

// Fast exp() using a lookup table.
// Domain is 0-1.
// RETURNS ZERO FOR NEGATIVE ARGS!
float exp_lookup(float arg) {

    int idx = arg * EXP_TABLE_SIZE;
    if (idx > EXP_TABLE_SIZE-1) {
        idx = EXP_TABLE_SIZE-1;
    } else if (idx < 0) {
        return 0.0f;
    }

    return exp_table[idx];
}

// Create lookup tables.
void create_tables(void) {

    // Exp map
    for (int i=0; i<EXP_TABLE_SIZE; i++) {
        float arg = (float)i/EXP_TABLE_SIZE;
        exp_table[i] = expf(arg);
    }

}

void synth_start(void) {

    create_tables();

    // Initial config
    cfg.busy    = false;
    cfg.volume  = 0.1f;
    cfg.legato  = false;
    cfg.tempo   = 90;

    for (int p=0; p<NUM_PART; p++) {
        for (int o=0; o<NUM_OSCILLATOR; o++) {
            cfg.part[p].osc[o].waveform = WAVE_SQUARE;
            cfg.part[p].osc[o].modifier = 0.0f;
            cfg.part[p].osc[o].detune = 1.0f;
            cfg.part[p].osc[o].gain = 0.5f;
        }
        for (int e=0; e<NUM_ENV; e++) {
            cfg.part[p].env[e].attack  = 0.0005;
            cfg.part[p].env[e].decay   = 0.005;
            cfg.part[p].env[e].sustain = 1.0;
            cfg.part[p].env[e].release = 0.0005;
            cfg.part[p].env_amount[e] = 1.0f;
        }
        cfg.part[p].lfo.rate = 0.0f;
        cfg.part[p].lfo.dest = DEST_AMP;
        cfg.part[p].lfo.amount = 0.0f;
        cfg.part[p].cutoff  = 1.0f;
        cfg.part[p].resonance = 0.0f;
        cfg.part[p].env_mod = 0.0f;    
        cfg.part[p].keyboard_track = 0.0f;
    }
    
    cfg.fx_damping = 0.3f;
    cfg.fx_combg = 0.881678f;
    cfg.fx_wet = 0.5f;

    memcpy(&synth, &cfg, sizeof(SynthConfig));
    fill_buffer();

    audio_init(SAMPLE_RATE);
    BSP_AUDIO_OUT_Play(out_buffer, OUT_BUFFER_SAMPLES * 2);

}




