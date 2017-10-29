#pragma once
#include "main.h"

#define SAMPLE_RATE 44100

typedef enum {
    WAVE_SINE,
    WAVE_SQUARE,
    WAVE_SAW
} WaveformType;


typedef struct {
	uint8_t volume;
    uint32_t freq;
    bool key;
    WaveformType osc_wave;
    float attack;
    float release;

} SynthConfig;

extern SynthConfig cfg;
extern SynthConfig cfgnew;


void AudioPlay_Test(void);


