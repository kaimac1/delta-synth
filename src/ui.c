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
    UI_OSC_MOD,
    UI_OSC_TUNE
} UIPage;

// UI state
int this_osc;
UIPage page;
UIPart part;

uint16_t saved_pots[NUM_PARTS][NUM_POTS];
bool pot_sync[NUM_POTS];
int encoder_start = 0;
int pot_moved = -1;
int pot_show_timer;

char *pot_names[NUM_PARTS][NUM_POTS] = {
    {"Osc 1", "Osc 2", "Noise",
    "Attack", "Decay", "Sustain",
    "??", "??", "??",
    "Cutoff", "Peak", "Env. mod"},
    {"Bass pitch", "Bass click", "Bass punch"}};





typedef struct {
    int wave;
    int modifier;
    int detune;
    int gain;
} OscSettings;

typedef struct {
    int attack;
    int decay;
    int release;
    int sustain;
    int cutoff;
    int resonance;
    int env_mod;
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

#define POT_IS_SYNCED(i) (ABS(pots[(i)], saved_pots[part][(i)]) < 50)

#define DECAY_MIN 0.001f
#define LEAD_DECAY_CONST 0.000014f

void update_lead(void) {

    // Source
    synth.osc[0].gain = saved_pots[part][0] / POTMAX;
    synth.osc[1].gain = saved_pots[part][1] / POTMAX;
    synth.noise_gain = 0.0f;//saved_pots[part][2] / POTMAX;

    // Env1
    float attack = saved_pots[part][3]/POTMAX + MIN_ATTACK;
    synth.env[0].attack = 1.0f/(attack * SAMPLE_RATE);
    float decay = saved_pots[part][4]/POTMAX + DECAY_MIN;
    synth.env[0].decay = LEAD_DECAY_CONST / (decay * decay * decay);
    synth.env[0].release = synth.env[0].decay;
    synth.env[0].sustain = saved_pots[part][5]/POTMAX;

    // Env2
    attack = saved_pots[part][6]/POTMAX + MIN_ATTACK;
    synth.env[1].attack = 1.0f/(attack * SAMPLE_RATE);
    decay = saved_pots[part][7]/POTMAX + DECAY_MIN;
    synth.env[1].decay = LEAD_DECAY_CONST / (decay * decay * decay);
    synth.env[1].release = synth.env[1].decay;
    synth.env[1].sustain = saved_pots[part][8]/POTMAX;

    float fx_amount = saved_pots[part][2]/POTMAX * 127.0f;
    float a = -1.0f / logf(1.0f - 0.3f);
    float b = 127.0f / (logf(1.0f - 0.98f) * a + 1.0f);
    float fb = 1.0f - expf((fx_amount - b) / (a*b));
    synth.fx_combg = fb;


    
    // Filter
    synth.cutoff = 10000.0f * (saved_pots[part][9] / POTMAX);
    synth.resonance = 3.99f * (saved_pots[part][10] / POTMAX);
    synth.env_mod = 5000.0f * (saved_pots[part][11]/ POTMAX);

}

void update_drums(void) {

    synth.bass_pitch = 0.0005f + 0.0015f * (saved_pots[PART_DRUMS][0] / POTMAX);
    synth.bass_click = 0.02f + 0.5f * (saved_pots[PART_DRUMS][1] / POTMAX);
    synth.bass_punch = 0.0002f * (saved_pots[PART_DRUMS][2] / POTMAX);

    float decay = saved_pots[PART_DRUMS][3]/POTMAX + DECAY_MIN;
    synth.bass_decay = LEAD_DECAY_CONST / (decay * decay * decay);

    decay = 0.3f * saved_pots[PART_DRUMS][4]/POTMAX + 0.1f;
    synth.snare_decay = LEAD_DECAY_CONST / (decay * decay * decay); // FIXME: RANGE

    synth.snare_tone = saved_pots[PART_DRUMS][5]/POTMAX;

    synth.clap_decay = 0.0004f + 0.0016f * saved_pots[PART_DRUMS][6]/POTMAX;
    synth.clap_filt = 3000.0f * saved_pots[PART_DRUMS][8]/POTMAX;


}

void ui_init(void) {

    for (int i=0; i<NUM_POTS; i++) {
        pot_sync[i] = true;
    }

    // Initial drums settings
    saved_pots[PART_DRUMS][0] = 1000;
    saved_pots[PART_DRUMS][1] = 0;
    saved_pots[PART_DRUMS][2] = 2048;
    saved_pots[PART_DRUMS][3] = 2048;
    saved_pots[PART_DRUMS][4] = 2048;
    update_drums();


}


void ui_update(void) {

    static int ctr = 0;
    static int beat = 0;

    bool btn = read_buttons();
    bool enc = read_encoder();

    // Work out if the pots are synced to the original values of the current part.
    // Update saved_pots with the current pot values, for pots that are synced.
    for (int i=0; i<NUM_POTS; i++) {
        if (POT_IS_SYNCED(i)) {
            if (!pot_sync[i]) {
                // Pot was just synced
                pot_moved = i;
                redraw = true;
            }
            pot_sync[i] = true;
            
        }
        if (pot_sync[i]) saved_pots[part][i] = pots[i];        
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
                this_osc %= NUM_OSCILLATOR + 1;
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
                pot_sync[i] = POT_IS_SYNCED(i);
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
                        MAP_ENCODER_CLAMP(input.osc[0].modifier);
                        input.osc[1].modifier = input.osc[0].modifier;
                        synth.osc[0].modifier = (float)input.osc[0].modifier/127;
                        synth.osc[1].modifier = (float)input.osc[1].modifier/127;
                    } else {
                        MAP_ENCODER_CLAMP(input.osc[this_osc].modifier);
                        synth.osc[this_osc].modifier = (float)input.osc[this_osc].modifier/127;
                    }
                    redraw = true;
                    break;

                case UI_OSC_TUNE:
                    input.osc[this_osc].detune += encoder.delta;
                    if ((input.osc[this_osc].detune) > 127) (input.osc[this_osc].detune) = 127;
                    if ((input.osc[this_osc].detune) < -127) (input.osc[this_osc].detune) = -127;
                    //synth.osc[this_osc].detune = 1.0f + 0.1f * (float)input.osc[this_osc].detune/127;
                    const float halfstep = 1.059463f;
                    float detune = pow(halfstep, input.osc[this_osc].detune);
                    synth.osc[this_osc].detune = detune;
                    redraw = true;
                    break;
            }

        }

        update_lead();

    } else if (part == PART_DRUMS) {
        update_drums();
    }

    // synth.seq_play = true;

    // if (synth.seq_play && seq_note_input != 0.0f) {
    //     seq.note[seq_idx] = seq_note_input;
    // }

    
    // Redraw display if required
    //if (enc) redraw = true;
    redraw = true;
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
    //         }
    //         // if (encoders[ENC_BLUE].delta) {
    //         //     ADD_DELTA_CLAMPED(input.fx_amount, encoders[ENC_BLUE].delta);
    //         //     float a = -1.0f / logf(1.0f - 0.3f);
    //         //     float b = 127.0f / (logf(1.0f - 0.98f) * a + 1.0f);
    //         //     float fb = 1.0f - expf(((float)input.fx_amount - b) / (a*b));
    //         //     synth.fx_combg = fb;
    //         // }            
    //     case PAGE_LFO:
    //         // if (encoders[ENC_RED].delta) {
    //         //     ADD_DELTA_CLAMPED(input.lfo_rate, encoders[ENC_RED].delta);
    //         //     synth.lfo_rate = (float)(input.lfo_rate+1) / (6 * SAMPLE_RATE);
    //         // }
    //         if (encoders[ENC_GREEN].delta) {
    //             ADD_DELTA_CLAMPED(input.lfo_amount, encoders[ENC_GREEN].delta);
    //             synth.lfo_amount = (float)input.lfo_amount/127;
    //         }            

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

    if (display_busy) return;
    draw_rect(0, 0, 128, 64, 0);

    char buf[32];

    if (pot_moved != -1) {
        draw_box(16,16,96,32);

        sprintf(buf, "%s", pot_names[part][pot_moved]);
        draw_text_cen(64, 18, buf, 1);       
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
    char *osc_mod[2];
    for (int i=0; i<2; i++) {
        switch(synth.osc[i].waveform) {
            case WAVE_SAW: osc_img[i] = saw; osc_mod[i] = "--"; break;
            case WAVE_SQUARE: osc_img[i] = square; osc_mod[i] = "Pulse"; break;
            case WAVE_TRI: osc_img[i] = tri; osc_mod[i] = "Fold"; break;
            default: break;        
        }
    }
    if (this_osc == 0 || this_osc == OSC_BOTH) draw_box(31, 0, 32, 17);
    if (this_osc == 1 || this_osc == OSC_BOTH) draw_box(64, 0, 32, 17);
    draw_image(32, 1, osc_img[0], 30, 15, false);
    draw_image(65,1, osc_img[1], 30, 15, false);


    switch (page) {
        case UI_DEFAULT:
            break;

        case UI_OSC_MOD:
            sprintf(buf, "%d", (int)(127 * synth.osc[0].modifier));
            draw_text_cen(38, 20, buf, 2);
            draw_text_cen(38, 50, osc_mod[0], 1);
            sprintf(buf, "%d", (int)(127 * synth.osc[1].modifier));
            draw_text_cen(90, 20, buf, 2);
            draw_text_cen(90, 50, osc_mod[1], 1);
            break;

        case UI_OSC_TUNE:
            sprintf(buf, "Tune: %.2f", (double)synth.osc[this_osc].detune);
            draw_text(0, 16, buf, 2);
            break;
    }

    sprintf(buf, "%d %d %d %d %d %d", synth.key[0], synth.key[1], synth.key[2], synth.key[3], synth.key[4], synth.key[5]);
    draw_text(0, 32, buf, 1);

    // float load = 100 * (float)(loop_time) / transfer_time;
    // sprintf(buf, "load %.1f", (double)load);
    // draw_text(0, 48, buf, 1);

}

