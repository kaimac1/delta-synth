#pragma once
#include "main.h"

#define SAMPLE_RATE 44100

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
} WaveformType;

typedef enum {
    ARP_OFF,
    ARP_UP,
    ARP_DOWN,
    ARP_UP_DOWN,
} ArpMode;

#define MAX_ARP 5

typedef struct {
    bool busy;
	uint8_t volume; // 0-100
    uint32_t freq;
    bool key;
    WaveformType osc_wave;
    bool legato;

    uint32_t arp_freqs[MAX_ARP];
    ArpMode arp;
    float arp_rate;

    int tempo;
    float detune;
    bool sync;

    // ADSR 
    float attack;	// seconds
    float decay;	// seconds
    float sustain;	// 0-1
    float release;	// seconds
    float env_curve;
    bool env_retrigger;

    // Filter
    float cutoff;
    float resonance;
    float env_mod;

} SynthConfig;

extern SynthConfig cfg;
extern SynthConfig cfgnew;

extern uint32_t loop_time;

void synth_start(void);


