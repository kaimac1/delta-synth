#pragma once
#include "main.h"

#define SAMPLE_RATE 44100
#define ENV_OVERSHOOT 0.05f
#define MIN_ATTACK 0.005f;

#define NUM_PART 1
#define NUM_OSCILLATOR 2
#define NUM_ENV 2

// Envelope generator state
typedef enum {
    ENV_ATTACK,
    ENV_DECAY,
    ENV_SUSTAIN,
    ENV_RELEASE
} EnvState;

// Modulation destination
typedef enum {
    //ENVDEST_DUMMY = -1, // So that the enum is signed.
    DEST_AMP = 0,
    DEST_FREQ,
    DEST_MOD,
    DEST_NOISE,
    NUM_DEST
} ModDest;

// Oscillator waveform
typedef enum {
    WAVE_TRI,
    WAVE_SQUARE,
    WAVE_SAW,
    NUM_WAVE
} Wave;

// Oscillator settings
typedef struct {
    Wave waveform;
    float modifier;
    float detune;
} Oscillator;

// Envelope generator settings
typedef struct {
    float attack;
    float decay;
    float sustain;
    float release;
} ADSR;

// LFO settings
typedef struct {
    float rate;
    float amount;
    ModDest dest;
} LFO;

// Single voice
typedef struct {

    float freq;
    bool gate;
    bool trig;

    Oscillator osc[NUM_OSCILLATOR];
    float osc_balance;
    float noise_gain;
    ADSR env[NUM_ENV];

    ModDest env_dest[NUM_ENV];
    float env_amount[NUM_ENV];

    LFO lfo;

    // Filter
    float cutoff;       // fs
    float resonance;    // 0-4
    float env_mod;      // Hz

} MonoSynth;



typedef struct {
    bool busy;          // config being updated, don't copy

    float volume;
    bool legato;

    // Sequencer
    int tempo;          // bpm
    bool seq_play;

    MonoSynth part[NUM_PART];
    
    // FX
    float fx_damping;
    float fx_combg;
    float fx_wet;

} SynthConfig;

typedef struct {
    float freq;
    float gate_length;
} SeqStep;

#define NUM_SEQ_STEPS 16
typedef struct {
    SeqStep step[NUM_SEQ_STEPS];
} SeqConfig;

extern SynthConfig cfg;
extern SynthConfig synth;
extern SeqConfig seq;

extern uint32_t loop_time;
extern uint32_t transfer_time;

extern bool trig_bass;
extern bool trig_snare;
extern bool trig_clap;
extern bool trig_hat_cl;
extern bool trig_hat_op;

void synth_start(void);
float exp_lookup(float arg);
