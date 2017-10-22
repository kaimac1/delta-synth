#pragma once
#include "main.h"

#define SAMPLE_RATE 44100

typedef enum {
    WAVE_SINE,
    WAVE_SQUARE,
    WAVE_SAW
} WaveformType;


typedef struct {
    uint32_t freq;
    WaveformType osc_wave;
} SynthConfig;

extern SynthConfig cfg;
extern SynthConfig cfgnew;


void AudioPlay_Test(void);


