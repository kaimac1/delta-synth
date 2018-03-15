#pragma once
#include "main.h"

#define SAMPLE_RATE 44100
#define ENV_OVERSHOOT 0.05f

#define SINE_TABLE_WIDTH 11 // bits
#define SINE_TABLE_SIZE (1<<SINE_TABLE_WIDTH)
extern int16_t sine_table[SINE_TABLE_SIZE];


typedef enum {
    ENV_ATTACK,
    ENV_DECAY,
    ENV_SUSTAIN,
    ENV_RELEASE
} EnvState;

typedef enum {
    WAVE_TRI,
    WAVE_SQUARE,
    WAVE_SAW
} Wave;

typedef enum {
    ARP_OFF,
    ARP_UP,
    ARP_DOWN,
    ARP_UP_DOWN,
} ArpMode;


#define MAX_ARP 5
#define NUM_VOICE 4
#define NUM_OSCILLATOR 3

typedef struct {
    Wave waveform;
    float folding;
    float duty;
    float detune;
    float gain;
} Oscillator;

typedef struct {
    bool busy;          // config being updated, don't copy

    uint8_t volume;     // 0-100
    bool legato;

    // Sequencer
    ArpMode arp;        // arpeggio mode
    uint32_t arp_freqs[MAX_ARP]; // arp notes, normalised
    int tempo;          // bpm

    // Oscillators
    float freq[NUM_VOICE];
    Oscillator osc[NUM_OSCILLATOR];

    // ADSR 
    bool key[NUM_VOICE];           // key down?
    float attack_rate;
    float decay_rate;
    float sustain_level;
    float release_rate;
    float env_curve;    // linearity
    bool env_retrigger; // retrigger now?

    float attack_time;
    float decay_time;
    float release_time;

    // Filter
    float cutoff;       // Hz
    float resonance;    // 0-4
    float env_mod;      // Hz at max env

    int ncombs;
    float fx_damping;
    float fx_combg;

} SynthConfig;

extern SynthConfig cfg;
extern SynthConfig cfgnew;

extern uint32_t loop_time;
extern uint32_t transfer_time;

void create_wave_tables(void);
void synth_start(void);


