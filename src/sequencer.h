#pragma once

void update_sequencer(void);
void draw_sequencer(void);

// Sequencer:

typedef struct {
    float freq;
    float gate_length;
} SeqStep;

#define MAX_STEPS 64
typedef struct {
    int length;
    SeqStep step[MAX_STEPS];
} SeqConfig;

