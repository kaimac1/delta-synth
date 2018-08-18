#include "ui.h"
#include "main.h"
#include "midi.h"
#include "notes.h"
#include "audio.h"
#include "board.h"
#include "synth.h"
#include "stm32f4xx_ll_tim.h"
#include "stm32f4xx_ll_adc.h"
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
int selected_osc;
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
    int folding;
    int duty;
    int gain;
    int detune;
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

#define MAP_ENCODER_CLAMP(x) {(x) += encoder.delta; if ((x) > 127) (x) = 127; if ((x) < 0) (x) = 0;}
//#define ADD_DELTA_WRAPPED(x, y) {(x) += (y); if ((x) > 127) (x) -= 127; if ((x) < 0) (x) += 127;}
#define POTMAX 4095.0f
#define ABS(a,b) ((a) > (b) ? ((a)-(b)) : ((b)-(a)))

#define POT_IS_SYNCED(i) (ABS(pots[(i)], saved_pots[part][(i)]) < 50)

void update_lead(void) {

    // Source
    cfgnew.osc[0].gain = saved_pots[part][0] / POTMAX;
    cfgnew.osc[1].gain = saved_pots[part][1] / POTMAX;
    cfgnew.noise_gain = saved_pots[part][2] / POTMAX;

    // Env1
    cfgnew.attack_time = saved_pots[part][3]/POTMAX + MIN_ATTACK;
    cfgnew.attack_rate = 1.0f/(cfgnew.attack_time * SAMPLE_RATE);

    float decay = saved_pots[part][4]/POTMAX;
    decay = decay*decay*decay;
    cfgnew.decay_time = decay * 5;
    cfgnew.decay_rate = 1.0 - exp(cfgnew.env_curve / (cfgnew.decay_time * SAMPLE_RATE));
    cfgnew.release_time = cfgnew.decay_time;
    cfgnew.release_rate = cfgnew.decay_rate;

    cfgnew.sustain_level = saved_pots[part][5]/POTMAX;
    
    // Filter
    cfgnew.cutoff = 10000.0f * (saved_pots[part][9] / POTMAX);
    cfgnew.resonance = 3.99f * (saved_pots[part][10] / POTMAX);
    cfgnew.env_mod = 5000.0f * (saved_pots[part][11]/ POTMAX);

}

void update_drums(void) {

    cfgnew.bass_pitch = 0.002f * (saved_pots[PART_DRUMS][0] / POTMAX);
    cfgnew.bass_click = 0.02f + 0.5f * (saved_pots[PART_DRUMS][1] / POTMAX);
    cfgnew.bass_punch = 0.0005f * (saved_pots[PART_DRUMS][2] / POTMAX);

}

void ui_init(void) {

    for (int i=0; i<NUM_POTS; i++) {
        pot_sync[i] = true;
    }

    // Initial drums settings
    saved_pots[PART_DRUMS][0] = 2048;
    saved_pots[PART_DRUMS][1] = 2048;
    saved_pots[PART_DRUMS][2] = 2048;
    update_drums();


}


void ui_update(void) {

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

        // Oscillator select
        if (buttons[BTN_OSC_SEL] == BTN_DOWN) {
            selected_osc++;
            selected_osc %= NUM_OSCILLATOR;
            redraw = true;
        }

        // Oscillator waveform
        if (buttons[BTN_OSC_WAVE] == BTN_DOWN) {
            Wave new = cfgnew.osc[selected_osc].waveform;
            new++;
            new %= NUM_WAVE;
            cfgnew.osc[selected_osc].waveform = new;
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
                    MAP_ENCODER_CLAMP(input.osc[selected_osc].folding);
                    cfgnew.osc[selected_osc].folding = 2.0f * (float)input.osc[selected_osc].folding/127;
                    redraw = true;
                    break;

                case UI_OSC_TUNE:
                    break;


            }

        }

        update_lead();

    } else if (part == PART_DRUMS) {
        update_drums();
    }

    // cfgnew.seq_play = true;

    // if (cfgnew.seq_play && seq_note_input != 0.0f) {
    //     seq.note[seq_idx] = seq_note_input;
    // }

    
    // Redraw display if required
    if (redraw) {
        draw_screen();
        if (display_draw()) redraw = false;
    }

    HAL_Delay(5);




    //     case PAGE_OSC:
    //         // // Detune
    //         // if (encoders[ENC_BLUE].delta) {
    //         //     ADD_DELTA_CLAMPED(input.osc[ui.selected_osc].detune, encoders[ENC_BLUE].delta);
    //         //     cfgnew.osc[ui.selected_osc].detune = 1.0f + 0.1f * (float)input.osc[ui.selected_osc].detune/127;
    //         // }
    //     case PAGE_FX:
    //         if (encoders[ENC_GREEN].delta) {
    //             ADD_DELTA_CLAMPED(input.fx_damping, encoders[ENC_GREEN].delta);
    //             cfgnew.fx_damping = (float)input.fx_damping/127;
    //         }
    //         // if (encoders[ENC_BLUE].delta) {
    //         //     ADD_DELTA_CLAMPED(input.fx_amount, encoders[ENC_BLUE].delta);
    //         //     float a = -1.0f / logf(1.0f - 0.3f);
    //         //     float b = 127.0f / (logf(1.0f - 0.98f) * a + 1.0f);
    //         //     float fb = 1.0f - expf(((float)input.fx_amount - b) / (a*b));
    //         //     cfgnew.fx_combg = fb;
    //         // }            
    //     case PAGE_LFO:
    //         // if (encoders[ENC_RED].delta) {
    //         //     ADD_DELTA_CLAMPED(input.lfo_rate, encoders[ENC_RED].delta);
    //         //     cfgnew.lfo_rate = (float)(input.lfo_rate+1) / (6 * SAMPLE_RATE);
    //         // }
    //         if (encoders[ENC_GREEN].delta) {
    //             ADD_DELTA_CLAMPED(input.lfo_amount, encoders[ENC_GREEN].delta);
    //             cfgnew.lfo_amount = (float)input.lfo_amount/127;
    //         }            

}

extern float    env[NUM_VOICE];

void draw_screen(void) {

    char buf[32];
    char *wave = "";

    if (display_busy) return;

    draw_rect(0, 0, 128, 64, 0);


    if (pot_moved != -1) {
        draw_box(16,16,96,32);

        sprintf(buf, "%s", pot_names[part][pot_moved]);
        draw_text_cen(64, 18, buf, 1);       
        sprintf(buf, "Original value");//, pot_moved, pot_sync[pot_moved]);
        draw_text_cen(64, 34, buf, 1);
    }


    if (part == PART_DRUMS) {
        sprintf(buf, "Drums");
        draw_text(0, 1, buf, 1);
        return;
    }

    // Osc
    sprintf(buf, "Osc %d", selected_osc + 1);
    draw_text(0, 1, buf, 1);

    switch (cfgnew.osc[selected_osc].waveform) {
        case WAVE_SAW: wave = "Saw"; break;
        case WAVE_SQUARE: wave = "Square"; break;
        case WAVE_TRI: wave = "Tri"; break;
        default: break;
    }
    draw_text(64, 1, wave, 1);


    switch (page) {
        case UI_DEFAULT:
            break;

        case UI_OSC_MOD:
            sprintf(buf, "Mod: %f", cfgnew.osc[selected_osc].folding);
            draw_text(0, 16, buf, 1);
            break;

        case UI_OSC_TUNE:
            sprintf(buf, "Tune: %f", cfgnew.osc[selected_osc].detune);
            draw_text(0, 16, buf, 1);
            break;
    }

    // float load = 100 * (float)(loop_time) / transfer_time;
    // sprintf(buf, "load %.1f", load);
    // draw_text(0, 48, buf, 1);



    //float load = 100 * ((float)loop_time / transfer_time);
    //sprintf(buf, "%.3f", (double)load);
    //draw_text(64,0, buf, 1, COL_WHITE);
   

    // switch (ui.page) {
    //     case PAGE_ENV:
    //         draw_text(0,  0,   "Envelope",  1, COL_WHITE);

    //         draw_text_cen(32,  16,   "ATTACK",  1, CRED);
    //         draw_gauge(32, 52, input.attack / 127.0f, CRED);

    //         draw_text_cen(96, 16,   "DECAY",   1, CGRN);
    //         draw_gauge(96, 52, input.decay / 127.0f, CGRN);
            
    //         draw_text_cen(32,   116, "SUSTAIN", 1, CBLU);
    //         draw_gauge(32, 94, input.sustain / 127.0f, CBLU);
            
    //         draw_text_cen(96,  116, "RELEASE", 1, CWHT);
    //         draw_gauge(96, 94, input.release / 127.0f, CWHT);
    //         break;


    //     case PAGE_FILTER:
    //         draw_text(0,  0,   "Filter",  1, COL_WHITE);

    //         draw_text_cen(32,  16,   "CUTOFF",  1, CRED);
    //         draw_gauge(32, 52, input.cutoff / 127.0f, CRED);

    //         draw_text_cen(96, 16,   "RESONANCE",   1, CGRN);
    //         draw_gauge(96, 52, input.resonance / 127.0f, CGRN);

    //         draw_text_cen(32,   116, "ENV MOD", 1, CBLU);
    //         draw_gauge(32, 94, input.env_mod / 127.0f, CBLU);
    //         break;

    //     case PAGE_FILTER_ENV:
    //         draw_text(0,  0,   "Filter Env",  1, COL_WHITE);
    //         break;


    //     case PAGE_OSC:
    //         sprintf(buf, "Osc %d", ui.selected_osc+1);
    //         draw_text(0,  0,   buf,  1, COL_WHITE);

    //         draw_text_cen(32,  16,   "WAVEFORM",  1, CRED);
    //         if (cfg.osc[ui.selected_osc].waveform == WAVE_SAW) {
    //             wave = "Saw";
    //         } else if (cfg.osc[ui.selected_osc].waveform == WAVE_SQUARE) {
    //             wave = "Square";
    //             draw_text_cen(96, 16, "SYMMETRY", 1, CGRN);
    //             draw_gauge(96, 52, input.osc[ui.selected_osc].duty / 127.0f, CGRN);
    //         } else {
    //             wave = "Tri";
    //             draw_text_cen(96, 16, "FOLDING", 1, CGRN);
    //             draw_gauge(96, 52, input.osc[ui.selected_osc].folding / 127.0f, CGRN);                
    //         }
    //         draw_text_cen(32,  40, wave,  1, CRED);

    //         draw_text_cen(32,   116, "DETUNE", 1, CBLU);
    //         draw_gauge(32, 94, input.osc[ui.selected_osc].detune / 127.0f, CBLU);
            
    //         draw_text_cen(96,  116, "GAIN", 1, CWHT);
    //         draw_gauge(96, 94, input.osc[ui.selected_osc].gain / 127.0f, CWHT);
    //         break;


    //     case PAGE_FX:
    //         draw_text_cen(96, 16, "DAMPING", 1, CGRN);
    //         draw_gauge(96, 52, input.fx_damping / 127.0f, CGRN);

    //         draw_text_cen(32,   116, "AMOUNT", 1, CBLU);
    //         draw_gauge(32, 94, input.fx_amount / 127.0f, CBLU);
    //         break;


    //     case PAGE_LFO:
    //         draw_text(0, 0, "LFO", 1, COL_WHITE);

    //         draw_text_cen(32,  16,   "RATE",  1, CRED);
    //         draw_gauge(32, 52, input.lfo_rate / 127.0f, CRED);
    //         draw_text_cen(96, 16, "AMOUNT", 1, CGRN);
    //         draw_gauge(96, 52, input.lfo_amount / 127.0f, CGRN);
    //         break;

}

