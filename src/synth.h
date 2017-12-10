#pragma once
#include "main.h"

#define SAMPLE_RATE 44100
#define ENV_OVERSHOOT 0.05f

typedef enum {
    ENV_ATTACK,
    ENV_DECAY,
    ENV_SUSTAIN,
    ENV_RELEASE
} EnvState;

typedef enum {
    WAVE_SINE,
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

typedef struct {
    bool busy;          // config being updated, don't copy

    uint8_t volume;     // 0-100
    bool legato;

    // Sequencer
    ArpMode arp;        // arpeggio mode
    uint32_t arp_freqs[MAX_ARP]; // arp notes, normalised
    int tempo;          // bpm

    // Oscillators
    uint32_t freq;      // normalised (max=fs/2)
    Wave osc_wave;      // waveform
    bool sync;          // osc2 sync to master?
    float detune;       // fraction of master freq.

    // ADSR 
    bool key;           // key down?
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

} SynthConfig;

extern SynthConfig cfg;
extern SynthConfig cfgnew;

extern uint32_t loop_time;

void synth_start(void);


