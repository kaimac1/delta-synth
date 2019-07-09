#include "sequencer.h"
#include "board.h"
#include "ui.h"
#include "synth.h"

SeqConfig seq;

// Currently selected step
int this_step;
// Counter for selected step LED blink
int step_led_ctr;
const int blink = 20;

int step_play_ctr;
const int tick = 50;


void next_step(void) {
    leds[this_step] = 0;
    this_step++;
    if (this_step >= seq.length) this_step = 0;
    leds[this_step] = 1;
    step_led_ctr = 0;
}


// MIDI note on/off ISRs
void seq_note_on(float freq) {
    seq.step[this_step].freq = freq;
}
void seq_note_off(float freq) {
    next_step();
}


void update_sequencer(void) {

    bool enc = read_encoder();

    
    if (mode == MODE_REC) {
        // LED blink
        step_led_ctr++;
        if (step_led_ctr > blink) {
            step_led_ctr = 0;
            leds[this_step] ^= 1;
            update_leds();
        }
    }

    if (buttons[BTN_SHIFT] == BTN_DOWN) {
        next_step();
    }

    if (buttons[BTN_PLAY_LOAD] == BTN_DOWN) {
        mode = MODE_PLAY;
        //synth.seq_play = true;
        redraw = true;
    }
    else if (buttons[BTN_REC_SAVE] == BTN_DOWN) {
        mode = MODE_REC;
        //synth.seq_play = false;
        redraw = true;
    }

    // for (int i=0; i<16; i++) {
    //     if (buttons[i] == BTN_DOWN) {
    //         leds[i] = 1;
    //         update_leds();
    //     } else if (buttons[i] == BTN_UP) {
    //         leds[i] = 0;
    //         update_leds();
    //     }
    // }

    if (enc) {
        // Change sequence length
        ADD_CLAMP(seq.length, encoder.delta, MAX_STEPS);
        redraw = true;
    }


    if (mode == MODE_PLAY) {
        step_play_ctr++;
        if (step_play_ctr > tick) {
            step_play_ctr = 0;
            next_step();
            update_leds();
            synth.part[0].freq = seq.step[this_step].freq;
            synth.part[0].gate = true;
            synth.part[0].trig = true;
        }
    }


}


void draw_sequencer(void) {

    char buf[16];

    draw_text(0,16, mode == MODE_REC ? "Rec" : "Play", 0);

    // sequence length debug
    sprintf(buf, "len:%d", seq.length);
    draw_text_cen(64,34,buf,0);

}