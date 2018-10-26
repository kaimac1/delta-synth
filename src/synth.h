#pragma once
#include "main.h"

#define SAMPLE_RATE 44100
#define ENV_OVERSHOOT 0.05f
#define MIN_ATTACK 0.005f;

#define SINE_TABLE_WIDTH 11 // bits
#define SINE_TABLE_SIZE (1<<SINE_TABLE_WIDTH)
extern int16_t sine_table[SINE_TABLE_SIZE];


#define MAX_ARP 5

#define NUM_VOICE 6
#define NUM_OSCILLATOR 2
#define NUM_ENV 2
#define NUM_LFO 1

// Envelope generator state
typedef enum {
    ENV_ATTACK,
    ENV_DECAY,
    ENV_SUSTAIN,
    ENV_RELEASE
} EnvState;

// Envelope destination
typedef enum {
    ENVDEST_DUMMY = -1, // So that the enum is signed.
    ENVDEST_AMP = 0,
    ENVDEST_PITCH_OSC1,
    ENVDEST_PITCH_BOTH,
    ENVDEST_MOD_OSC1,
    ENVDEST_MOD_BOTH,
    NUM_ENVDEST
} EnvDest;

// Oscillator waveform
typedef enum {
    WAVE_TRI,
    WAVE_SQUARE,
    WAVE_SAW,
    NUM_WAVE
} Wave;

typedef enum {
    ARP_OFF,
    ARP_UP,
    ARP_DOWN,
    ARP_UP_DOWN,
} ArpMode;

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
} LFO;



typedef struct {
    bool busy;          // config being updated, don't copy

    uint8_t volume;     // 0-100
    bool legato;

    // Sequencer
    ArpMode arp;        // arpeggio mode
    uint32_t arp_freqs[MAX_ARP]; // arp notes, normalised
    int tempo;          // bpm
    bool seq_play;

    // Voices
    float freq[NUM_VOICE];
    bool key[NUM_VOICE];           // key down?
    bool key_retrigger[NUM_VOICE];

    // Oscillators
    Oscillator osc[NUM_OSCILLATOR];
    float osc_balance;
    float noise_gain;

    // Envelope settings
    ADSR env[NUM_ENV];
    EnvDest env_dest[NUM_ENV];
    float env_amount[NUM_ENV];
    
    // Filter
    float cutoff;       // fs
    float resonance;    // 0-4
    float env_mod;      // Hz at max env

    // LFO
    LFO lfo[NUM_LFO];

    // FX
    float fx_damping;
    float fx_combg;
    float fx_wet;

    // Drums
    float bass_pitch;
    float bass_click;
    float bass_punch;
    float bass_decay;
    float snare_decay;
    float snare_tone;

    float clap_decay;
    float clap_filt;

} SynthConfig;

#define NUM_SEQ_NOTES 2
typedef struct {
    float note[NUM_SEQ_NOTES];

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

void create_wave_tables(void);
void synth_start(void);


