#include "ui.h"
#include "main.h"
#include "midi.h"
#include "notes.h"
#include "audio.h"
#include "board.h"
#include "synth.h"
#include <math.h>
#include <string.h>

typedef enum {
    PART_LEAD,
    PART_DRUMS,
    NUM_PARTS
} UIPart;

typedef enum {
    UI_DEFAULT,
    UI_OSC_MIX,
    UI_OSC_MOD,
    UI_OSC_TUNE
} UIPage;

// UI state
int this_osc;
UIPage page;
UIPart part;

uint16_t saved_pot[NUM_POTS];
bool pot_sync[NUM_POTS];
int encoder_start = 0;
int pot_moved = -1;
int pot_show_timer;

bool detune_mode;

// char *pot_names[NUM_PARTS][NUM_POTS] = {
//     {"Osc 1", "Osc 2", "Noise",
//     "Attack", "Decay", "Sustain",
//     "??", "??", "??",
//     "Cutoff", "Peak", "Env. mod"},
//     {"Bass pitch", "Bass click", "Bass punch"}};

typedef struct {
    int wave;
    int detune;
    int detune_cents;
    int gain;
} OscSettings;

typedef struct {
    OscSettings osc[NUM_OSCILLATOR];
    int lfo_rate;
    int lfo_amount;
    int fx_damping;
    int fx_amount;
} InputSettings;
InputSettings input;

unsigned int seq_idx;
float seq_note_input;

bool redraw = true;
void draw_screen(void);

#define OSC_BOTH NUM_OSCILLATOR
//#define CHANGE_OSC_ATTR(attr, value) synth.osc[this_osc].attr = (value)

#define MAP_ENCODER_CLAMP(x) {(x) += encoder.delta; if ((x) > 127) (x) = 127; if ((x) < 0) (x) = 0;}
//#define ADD_DELTA_WRAPPED(x, y) {(x) += (y); if ((x) > 127) (x) -= 127; if ((x) < 0) (x) += 127;}
#define POTMAX 4095.0f
#define ABS(a,b) ((a) > (b) ? ((a)-(b)) : ((b)-(a)))

#define SAVE_POT(i) {saved_pot[(i)] = pots[(i)]; pot_sync[(i)] = 0;}
#define POT_MOVED(i) (ABS(pots[(i)], saved_pot[(i)]) > 100)

#define DECAY_MIN 0.02f
#define LEAD_DECAY_CONST 0.000014f

void update_lead(void) {

    // Source
    synth.osc_balance = pots[0] / POTMAX;
    if (pot_sync[1]) synth.osc[this_osc].modifier = pots[1] / POTMAX;
    synth.noise_gain = 0.0f;//pots[2] / POTMAX;

    // Env1
    float attack = pots[3]/POTMAX + MIN_ATTACK;
    synth.env[0].attack = 1.0f/(attack * SAMPLE_RATE);
    float decay = pots[4]/POTMAX + DECAY_MIN;
    decay = LEAD_DECAY_CONST / (decay * decay * decay);
    synth.env[0].decay = decay;
    synth.env[0].release = decay;
    synth.env[0].sustain = pots[5]/POTMAX;

    // Env2
    // attack = pots[6]/POTMAX + MIN_ATTACK;
    // synth.env[1].attack = 1.0f/(attack * SAMPLE_RATE);
    // decay = pots[7]/POTMAX + DECAY_MIN;
    // synth.env[1].decay = LEAD_DECAY_CONST / (decay * decay * decay);
    // synth.env[1].release = synth.env[1].decay;
    // synth.env[1].sustain = pots[8]/POTMAX;

    synth.lfo[0].rate = 0.001f * pots[6] / POTMAX;
    synth.lfo[0].amount = pots[7] / POTMAX;

    float fx_amount = pots[2]/POTMAX * 127.0f;
    float a = -1.0f / logf(1.0f - 0.3f);
    float b = 127.0f / (logf(1.0f - 0.98f) * a + 1.0f);
    float fb = 1.0f - expf((fx_amount - b) / (a*b));
    synth.fx_combg = fb;


    
    // Filter
    synth.cutoff = 10000.0f * (pots[9] / POTMAX);
    synth.resonance = 3.99f * (pots[10] / POTMAX);
    synth.env_mod = 5000.0f * (pots[11]/ POTMAX);

}

/*void update_drums(void) {

    synth.bass_pitch = 0.0005f + 0.0015f * (pots[PART_DRUMS][0] / POTMAX);
    synth.bass_click = 0.02f + 0.5f * (pots[PART_DRUMS][1] / POTMAX);
    synth.bass_punch = 0.0002f * (pots[PART_DRUMS][2] / POTMAX);

    float decay = pots[PART_DRUMS][3]/POTMAX + DECAY_MIN;
    synth.bass_decay = LEAD_DECAY_CONST / (decay * decay * decay);

    decay = 0.3f * pots[PART_DRUMS][4]/POTMAX + 0.1f;
    synth.snare_decay = LEAD_DECAY_CONST / (decay * decay * decay); // FIXME: RANGE

    synth.snare_tone = pots[PART_DRUMS][5]/POTMAX;

    synth.clap_decay = 0.0004f + 0.0016f * pots[PART_DRUMS][6]/POTMAX;
    synth.clap_filt = 3000.0f * pots[PART_DRUMS][8]/POTMAX;


}*/

void ui_init(void) {

    for (int i=0; i<NUM_POTS; i++) {
        pot_sync[i] = true;
    }

    // Initial drums settings
    //pots[PART_DRUMS][0] = 1000;
    //pots[PART_DRUMS][1] = 0;
    //pots[PART_DRUMS][2] = 2048;
    //pots[PART_DRUMS][3] = 2048;
    //pots[PART_DRUMS][4] = 2048;
    //update_drums();


}


void ui_update(void) {

    bool btn = read_buttons();
    bool enc = read_encoder();

    // Sync pots that have moved
    for (int i=0; i<NUM_POTS; i++) {
        if (!pot_sync[i] && POT_MOVED(i)) {
            pot_sync[i] = 1;
            pot_moved = i;
            redraw = true;
        }
    }

    // Briefly show that pot is synced
    if (pot_moved != -1) {
        pot_show_timer++;
        if (pot_show_timer == 100) {
            pot_show_timer = 0;
            pot_moved = -1;
            redraw = true;
        }
    }


    if (btn) {


        if (part == PART_LEAD) {

            // Oscillator select
            if (buttons[BTN_OSC_SEL] == BTN_DOWN) {
                this_osc++;
                this_osc %= NUM_OSCILLATOR;// + 1;
                SAVE_POT(1);
                SAVE_POT(2);
                redraw = true;
            }

            // Oscillator waveform
            if (buttons[BTN_OSC_WAVE] == BTN_DOWN) {
                if (this_osc == OSC_BOTH) {
                    Wave new = (synth.osc[0].waveform + 1) % NUM_WAVE;
                    synth.osc[0].waveform = new;
                    synth.osc[1].waveform = new;
                } else {
                    Wave new = (synth.osc[this_osc].waveform + 1) % NUM_WAVE;
                    synth.osc[this_osc].waveform = new;
                }
                    redraw = true;
            }

            // Oscillator mod
            // Pressing and releasing the button without moving the encoder will keep the UI in OSCMOD mode,
            // until the button is pressed again.
            // If the encoder is moved while the button is held, the UI will leave OSCMOD mode when the button is released.
            if (buttons[BTN_OSC_MOD] == BTN_DOWN) {
                if (page != UI_OSC_MOD) {
                    encoder_start = encoder.value;
                    page = UI_OSC_MOD;
                } else {
                    page = UI_DEFAULT;
                }
                redraw = true;
            }
            if (buttons[BTN_OSC_MOD] == BTN_UP) {
                if (page == UI_OSC_MOD) {
                    if (encoder.value != encoder_start) {
                        page = UI_DEFAULT;        
                    }
                }
                redraw = true;
            }

            // Oscillator tune
            if (buttons[BTN_OSC_TUNE] == BTN_DOWN) {
                if (page != UI_OSC_TUNE) {
                    encoder_start = encoder.value;
                    page = UI_OSC_TUNE;
                } else {
                    page = UI_DEFAULT;
                }
                redraw = true;
            }
            if (buttons[BTN_OSC_TUNE] == BTN_UP) {
                if (page == UI_OSC_TUNE) {
                    if (encoder.value != encoder_start) {
                        page = UI_DEFAULT;        
                    }
                }
                redraw = true;            
            }
        }


        // Drum testing
        if (buttons[8] == BTN_DOWN) trig_bass = true;

        // Part select
        if (buttons[15] == BTN_DOWN) {
            // Next part
            part++;
            part %= NUM_PARTS;
            redraw = true;

            // Reset pot sync
            for (int i=0; i<NUM_POTS; i++) {
                pot_sync[i] = 0;
            }
        }



    }




    if (part == PART_LEAD) {

        if (enc) {

            switch (page) {
                case UI_DEFAULT:
                    break;

                case UI_OSC_MOD:
                    if (this_osc == OSC_BOTH) {
                        //MAP_ENCODER_CLAMP(input.osc[0].modifier);
                        //input.osc[1].modifier = input.osc[0].modifier;
                        //synth.osc[0].modifier = (float)input.osc[0].modifier/127;
                        //synth.osc[1].modifier = (float)input.osc[1].modifier/127;
                    } else {
                        //MAP_ENCODER_CLAMP(input.osc[this_osc].modifier);
                        //synth.osc[this_osc].modifier = (float)input.osc[this_osc].modifier/127;
                    }
                    redraw = true;
                    break;

                case UI_OSC_TUNE:
                    if (buttons[BTN_ENCODER]) {
                        input.osc[this_osc].detune += encoder.half_delta;
                        if ((input.osc[this_osc].detune) > 36) (input.osc[this_osc].detune) =36;
                        if ((input.osc[this_osc].detune) < -36) (input.osc[this_osc].detune) = -36;
                    } else {
                        input.osc[this_osc].detune_cents += encoder.delta;
                    }
                    float det = input.osc[this_osc].detune + 0.01f * input.osc[this_osc].detune_cents;
                    const float halfstep = 1.059463f;
                    float detune = pow(halfstep, det);
                    synth.osc[this_osc].detune = detune;
                    redraw = true;
                    break;
            }

        }

        update_lead();

    } else if (part == PART_DRUMS) {
        //update_drums();
    }

    // synth.seq_play = true;

    // if (synth.seq_play && seq_note_input != 0.0f) {
    //     seq.note[seq_idx] = seq_note_input;
    // }

    
    // Redraw display if required
    //redraw = true;
    if (redraw) {
        draw_screen();
        if (display_draw()) redraw = false;
    }

    // ctr++;
    // if (ctr == 18) {
    //     ctr = 0;
    //     beat++;
    //     if (beat == 16) beat = 0;

    //     if (beat == 14) trig_hat_op = true;
    //     else if (beat % 2 == 0) trig_hat_cl = true;

    //     switch (beat) {
    //         // case 4: 
    //         // case 7:
    //         // // case 9:
    //         // // case 12:
    //         // // case 15:
    //         //     trig_snare = true;
    //         //     break;
    //         case 0:                
    //         case 3:
    //         case 11:
    //             trig_bass = true;
    //             break;
    //         // case 8:
    //         //     trig_clap = true;
    //         //     break;

    //     }
    // }

    

    HAL_Delay(5);

    //         if (encoders[ENC_GREEN].delta) {
    //             ADD_DELTA_CLAMPED(input.fx_damping, encoders[ENC_GREEN].delta);
    //             synth.fx_damping = (float)input.fx_damping/127;
    //         // if (encoders[ENC_BLUE].delta) {
    //         //     ADD_DELTA_CLAMPED(input.fx_amount, encoders[ENC_BLUE].delta);
    //         //     float a = -1.0f / logf(1.0f - 0.3f);
    //         //     float b = 127.0f / (logf(1.0f - 0.98f) * a + 1.0f);
    //         //     float fb = 1.0f - expf(((float)input.fx_amount - b) / (a*b));
    //         //     synth.fx_combg = fb;

}

extern float    env[NUM_VOICE];

uint8_t saw[] = {
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0
,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0
,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,0,0,0,0,0
,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,1,1,0,0,0,0,0
,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,1,1,0,0,0,0,0
,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,1,1,0,0,0,0,0
,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0
,0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0
,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0
,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0
,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0
,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};
uint8_t square[] = {
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0
,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0
,0,0,0,0,1,1,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0
,0,0,0,0,1,1,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0
,0,0,0,0,1,1,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0
,0,0,0,0,1,1,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0
,0,0,0,0,1,1,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0
,0,0,0,0,1,1,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0
,0,0,0,0,1,1,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0
,0,0,0,0,1,1,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0
,0,0,0,0,1,1,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0
,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};
uint8_t tri[] = {
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0
,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0
,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,0,0,0,0,0,0,0,0
,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,0,0,1,1,1,0,0,0,0,0,0,0
,0,0,0,0,1,1,0,0,0,0,0,0,0,0,1,1,1,0,0,0,0,1,1,1,0,0,0,0,0,0
,0,0,0,0,1,1,1,0,0,0,0,0,0,1,1,1,0,0,0,0,0,0,1,1,1,0,0,0,0,0
,0,0,0,0,0,1,1,1,0,0,0,0,1,1,1,0,0,0,0,0,0,0,0,1,1,1,0,0,0,0
,0,0,0,0,0,0,1,1,1,0,0,1,1,1,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0
,0,0,0,0,0,0,0,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};


void draw_screen(void) {

    static int potmin = 4095;
    static int potmax = 0;

    if (display_busy) return;
    draw_rect(0, 0, 128, 64, 0);

    char buf[32];

    if (pot_moved != -1) {
        draw_box(16,16,96,32);

        //sprintf(buf, "%s", pot_names[pot_moved]);
        //draw_text_cen(64, 18, buf, 1);       
        sprintf(buf, "Original value");
        draw_text_cen(64, 34, buf, 1);
    }


    if (part == PART_DRUMS) {
        sprintf(buf, "Drums");
        draw_text(0, 1, buf, 1);
        return;
    }

    // Osc
    uint8_t *osc_img[2] = {NULL};
    for (int i=0; i<2; i++) {
        switch(synth.osc[i].waveform) {
            case WAVE_SAW: osc_img[i] = saw; break;
            case WAVE_SQUARE: osc_img[i] = square; break;
            case WAVE_TRI: osc_img[i] = tri; break;
            default: break;        
        }
    }
    if (this_osc == 0 || this_osc == OSC_BOTH) draw_box(0, 0, 32, 17);
    if (this_osc == 1 || this_osc == OSC_BOTH) draw_box(32, 0, 32, 17);
    draw_image(1, 1, osc_img[0], 30, 15, false);
    draw_image(33,1, osc_img[1], 30, 15, false);


    switch (page) {
        case UI_DEFAULT:
            break;

        case UI_OSC_MIX:
            draw_box(16,16,96,32);
            sprintf(buf, "Osc mix: %d", (int)(127 * synth.osc_balance));
            draw_text_cen(64, 34, buf, 1);
            break;

        case UI_OSC_MOD:
            break;

        case UI_OSC_TUNE:
            sprintf(buf, "Tune: %d %d", input.osc[this_osc].detune, input.osc[this_osc].detune_cents);
            draw_text(0, 16, buf, 1);
            break;
    }

    int pot = pots[0];
    if (pot > potmax) potmax = pot;
    if (pot < potmin) potmin = pot;

    // sprintf(buf, "%d", pot);
    // draw_text(16, 24, buf, 1);
    // sprintf(buf, "%d", potmax);
    // draw_text(16, 34, buf, 1);
    // sprintf(buf, "%d", potmin);
    // draw_text(16, 44, buf, 1);
    sprintf(buf, "%f", synth.env[0].release);
    draw_text(16, 24, buf, 1);
    sprintf(buf, "%f", synth.env[0].decay);
    draw_text(16, 34, buf, 1);
    sprintf(buf, "%f", synth.fx_combg);
    draw_text(16, 44, buf, 1);


    // float load = 100 * (float)(loop_time) / transfer_time;
    // sprintf(buf, "load %.1f", (double)load);
    // draw_text(0, 48, buf, 1);

}

