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


typedef struct {
	uint8_t volume; // 0-100
    uint32_t freq;
    bool key;
    WaveformType osc_wave;

    // ADSR 
    float attack;	// seconds
    float decay;	// seconds
    float sustain;	// 0-1
    float release;	// seconds
    bool env_retrigger;

    // Filter
    float cutoff;
    float resonance;
    float env_mod;

} SynthConfig;

extern SynthConfig cfg;
extern SynthConfig cfgnew;


void synth_start(void);


