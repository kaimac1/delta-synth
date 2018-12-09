#include "ui.h"
#include "bitmaps.h"
#include "main.h"
#include "midi.h"
#include "notes.h"
#include "audio.h"
#include "board.h"
#include "synth.h"
#include <math.h>
#include <string.h>

#define PERFDEBUG

typedef enum {
    UI_DEFAULT,
    UI_OSC_TUNE,
    UI_FX,
    UI_ENV_MENU,
    UI_LFO_MENU,
} UIPage;

// UI state
int part;
int this_osc;
int this_env;
UIPage page;


uint16_t filtered_pot[NUM_POTS];
uint16_t saved_pot[NUM_POTS];
bool pot_sync[NUM_POTS];
int encoder_start = 0;
int pot_moved = -1;
int pot_show_timer;

bool tune_semitones;

typedef struct {
    int reverb_amount;
    int wet_level;
    ModDest env_dest[NUM_ENV];
    int env_amount[NUM_ENV];
    ModDest lfo_dest;
    int lfo_amount;
} InputSettings;
InputSettings input;

unsigned int seq_idx;
float seq_note_input;

Menu menu_fx;
Menu menu_env;
Menu menu_lfo;

bool redraw = true;

void draw_screen(void);
void menu_fx_draw(Menu *menu, int i);
void menu_env_draw(Menu *menu, int i);
void menu_lfo_draw(Menu *menu, int i);

#define ADD_CLAMP(x, y, max) {(x) += (y); if ((x) > (max)) (x) = (max); if ((x) < 0) (x) = 0;}
#define ADD_CLAMP_MINMAX(x, y, min, max) {(x) += (y); if ((x) > (max)) (x) = (max); if ((x) < (min)) (x) = (min);}
#define ABS(a,b) ((a) > (b) ? ((a)-(b)) : ((b)-(a)))
#define SAVE_POT(i) {saved_pot[(i)] = filtered_pot[(i)]; pot_sync[(i)] = 0;}

#define POT_UPDATE_THRESHOLD 50
#define POTMAX 4095.0f
#define DECAY_MIN 0.02f
#define LEAD_DECAY_CONST 0.000014f
const float halfstep = 1.059463f;


void set_reverb(void) {

    float a = -1.0f / logf(1.0f - 0.3f);
    float b = 127.0f / (logf(1.0f - 0.98f) * a + 1.0f);
    float fb = 1.0f - expf((input.reverb_amount - b) / (a*b));
    synth.fx_combg = fb;
    synth.fx_wet = input.wet_level / 127.0f;

}



void update_lead(void) {

    for (int i=0; i<NUM_POTS; i++) {

        // Filter
        filtered_pot[i] += 0.2f * (pots[i] - filtered_pot[i]);

        // Resync pots that have moved
        if (!pot_sync[i] && (ABS(filtered_pot[i], saved_pot[i]) > 100)) {
            pot_sync[i] = 1;
        }

        if (pot_sync[i]) {
            // Update 
            if (ABS(filtered_pot[i], saved_pot[i]) > POT_UPDATE_THRESHOLD) {
                saved_pot[i] = filtered_pot[i];
                pot_moved = i;
                redraw = true;
            }
        
            float amount = filtered_pot[i] / POTMAX;
            float temp;
            int temp_int;

            switch (i) {
                // Oscillator
                case 0: // Osc1/2 mix
                    synth.part[part].osc[0].gain = 1.0f - amount;
                    synth.part[part].osc[1].gain = amount;
                    break;
                case 1: // Modifier
                    synth.part[part].osc[this_osc].modifier = amount;
                    break;
                case 2: // Tune
                    if (tune_semitones) {
                        // +/- 1 octave, semitone steps
                        temp = 24 * (amount - 0.5f);
                        if (temp > 0.0f) temp += 0.5f;
                        if (temp < 0.0f) temp -= 0.5f;
                        temp = (int)temp;
                        temp = pow(halfstep, temp);
                    } else {
                        // +/- 1 semitone, free
                        temp = 2 * (amount - 0.5f);
                        temp = pow(halfstep, temp);
                    }
                    synth.part[part].osc[this_osc].detune = temp;
                    break;

                // Enevelope
                case 3: // Attack
                    temp = amount + MIN_ATTACK;
                    synth.part[part].env[this_env].attack = 1.0f/(temp * SAMPLE_RATE);
                    break;
                case 4: // Decay
                    temp = amount + DECAY_MIN;
                    synth.part[part].env[this_env].decay = LEAD_DECAY_CONST / (temp*temp*temp);
                    break;
                case 5: // Sustain
                    synth.part[part].env[this_env].sustain = amount;
                    break;
                case 6: // Release
                    temp = amount + DECAY_MIN;
                    synth.part[part].env[this_env].release = LEAD_DECAY_CONST / (temp*temp*temp);
                    break;

                // Filter
                case 9: // Cutoff
                    synth.part[part].cutoff = amount;
                    break;
                case 10: // Resonance
                    synth.part[part].resonance = 3.99f * amount;
                    break;
                case 11: // Envelope cutoff modulation
                    synth.part[part].env_mod = amount;
                    break;

                // LFO
                case 7: // Rate
                    synth.part[part].lfo.rate = 0.006f * amount;
                    break;

                case 8:
                    synth.volume = amount;
                    break;
            }
        }
    }

    synth.part[part].noise_gain = 0.0f;//pots[2] / POTMAX;

    synth.part[part].env_dest[0] = input.env_dest[0];
    synth.part[part].env_dest[1] = input.env_dest[1];
    synth.part[part].env_amount[0] = input.env_amount[0] / 127.0f;
    synth.part[part].env_amount[1] = input.env_amount[1] / 127.0f;

    synth.part[part].lfo.dest = input.lfo_dest;
    synth.part[part].lfo.amount = input.lfo_amount / 127.0f;

}



void ui_init(void) {

    for (int i=0; i<NUM_POTS; i++) {
        pot_sync[i] = true;
        filtered_pot[i] = pots[i];
        saved_pot[i] = filtered_pot[i];
    }

    menu_fx.draw = (void*)menu_fx_draw;
    menu_fx.num_items = 2;
    strcpy(menu_fx.items[0].name, "Reverb amt");
    strcpy(menu_fx.items[1].name, "Wet level");
    
    menu_env.draw = (void*)menu_env_draw;
    menu_env.num_items = 4;
    strcpy(menu_env.items[0].name, "Env 1 dest");
    strcpy(menu_env.items[1].name, "Env 1 amt");
    strcpy(menu_env.items[2].name, "Env 2 dest");
    strcpy(menu_env.items[3].name, "Env 2 amt");

    menu_lfo.draw = (void*)menu_lfo_draw;
    menu_lfo.num_items = 2;
    strcpy(menu_lfo.items[0].name, "LFO dest");
    strcpy(menu_lfo.items[1].name, "LFO amt");

    input.reverb_amount = 64;
    input.wet_level = 0;
    input.env_dest[0] = DEST_AMP;
    input.env_amount[0] = 127;
    input.env_dest[1] = DEST_AMP;
    input.env_amount[1] = 0;
    input.lfo_dest = DEST_AMP;
    input.lfo_amount = 0;
    set_reverb();

    //synth.seq_play = true;

}

// Switch in and out of the page if the button is pressed.
// If the button is held and the encoder is moved before the button is released, switch out of the page
// (go back to DEFAULT) when the button is released.
void check_button_for_page(int btn, int new_page) {

    if (buttons[btn] == BTN_DOWN) {
        if (page != new_page) {
            encoder_start = encoder.value;
            page = new_page;
        } else {
            page = UI_DEFAULT;
        }
        redraw = true;
    }
    if (buttons[btn] == BTN_UP) {
        if (page == new_page) {
            if (encoder.value != encoder_start) {
                page = UI_DEFAULT;
            }
        }
        redraw = true;
    }

}

// Scroll through menu based on encoder delta
void menu_scroll(Menu *menu) {

    menu->selected_item += encoder.half_delta;
    if (menu->selected_item >= menu->num_items) menu->selected_item = menu->num_items -1;
    if (menu->selected_item < 0) menu->selected_item = 0;

}


void ui_update(void) {

    static int ctr;

    bool btn = read_buttons();
    bool enc = read_encoder();

    // Handle button presses
    if (btn) {

        // Oscillator select
        if (buttons[BTN_OSC_SEL] == BTN_DOWN) {
            this_osc++;
            this_osc %= NUM_OSCILLATOR;
            SAVE_POT(1);
            SAVE_POT(2);
            redraw = true;
        }

        // Oscillator waveform
        if (buttons[BTN_OSC_WAVE] == BTN_DOWN) {
            Wave new = (synth.part[part].osc[this_osc].waveform + 1) % NUM_WAVE;
            synth.part[part].osc[this_osc].waveform = new;
            redraw = true;
        }

        // Tune
        if (buttons[BTN_OSC_TUNE] == BTN_DOWN) {
            tune_semitones = !tune_semitones;
            SAVE_POT(2);
            //seq_idx++;
            //if (seq_idx == NUM_SEQ_STEPS) seq_idx = 0;
        }
        //

        // Envelope select
        if (buttons[BTN_ENV_SEL] == BTN_DOWN) {
            this_env++;
            this_env %= NUM_ENV;
            SAVE_POT(3);
            SAVE_POT(4);
            SAVE_POT(5);
            SAVE_POT(6);
            redraw = true;
        }

        // Envelope menu
        check_button_for_page(BTN_ENV_MENU, UI_ENV_MENU);

        // FX menu
        check_button_for_page(5, UI_FX);

        // LFO menu
        check_button_for_page(6, UI_LFO_MENU);

    }

    // Handle encoder movements
    if (enc) {

        switch (page) {
            case UI_DEFAULT:
                break;

            case UI_OSC_TUNE:
                redraw = true;
                break;

            case UI_FX:
                if (buttons[BTN_EDIT] == BTN_HELD) {
                    if (menu_fx.selected_item == 0) {
                        ADD_CLAMP(input.reverb_amount, encoder.delta, 127);
                        set_reverb();
                    } else if (menu_fx.selected_item == 1) {
                        ADD_CLAMP(input.wet_level, encoder.delta, 127);
                        set_reverb();
                    }

                } else {
                    menu_scroll(&menu_fx);
                }
                redraw = true;
                break;

            case UI_ENV_MENU:
                if (buttons[BTN_EDIT] == BTN_HELD) {
                    switch (menu_env.selected_item) {
                        case 0:
                            ADD_CLAMP(input.env_dest[0], encoder.half_delta, NUM_DEST-1);
                            break;
                        case 1:
                            ADD_CLAMP_MINMAX(input.env_amount[0], encoder.delta, -127, 127);
                            break;
                        case 2:
                        ADD_CLAMP(input.env_dest[1], encoder.half_delta, NUM_DEST-1);
                            break;
                        case 3:
                            ADD_CLAMP_MINMAX(input.env_amount[1], encoder.delta, -127, 127);
                            break;
                    }
                } else {
                    menu_scroll(&menu_env);
                }
                redraw = true;
                break;

            case UI_LFO_MENU:
                if (buttons[BTN_EDIT] == BTN_HELD) {
                    switch (menu_lfo.selected_item) {
                        case 0:
                            ADD_CLAMP(input.lfo_dest, encoder.half_delta, NUM_DEST-1);
                            break;
                        case 1:
                            ADD_CLAMP_MINMAX(input.lfo_amount, encoder.delta, -127, 127);
                            break;
                    }
                } else {
                    menu_scroll(&menu_lfo);
                }
                redraw = true;
                break;

            default:
                break;
        }

    }

    update_lead();
    if (seq_note_input > 0.0f) {
        seq.step[seq_idx].freq = seq_note_input;
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
    
    // Redraw display if required
#ifdef PERFDEBUG
    //redraw = true;
#endif
    if (ctr++ == 100) {
        ctr = 0;
        redraw = true;
    }
    if (redraw) {
        draw_screen();
        if (display_draw()) redraw = false;
    }

    HAL_Delay(5);

}





void draw_screen(void) {

    if (display_busy) return;
    draw_rect(0, 0, 128, 64, 0);

    // Menus
    if (page == UI_FX) {
        draw_menu(menu_fx);
        return;
    }
    if (page == UI_ENV_MENU) {
        draw_menu(menu_env);
        return;
    }
    if (page == UI_LFO_MENU) {
        draw_menu(menu_lfo);
        return;
    }

    char buf[32];

    if (pot_moved != -1) {
        draw_box(16,16,96,32);
        int amount = filtered_pot[pot_moved] / 32;
        sprintf(buf, "%d: %d", pot_moved, amount);
        draw_text_cen(64, 34, buf, 0);
    }

    // Osc
    uint8_t *osc_img[2] = {NULL};
    for (int i=0; i<2; i++) {
        switch(synth.part[part].osc[i].waveform) {
            case WAVE_SAW: osc_img[i] = saw; break;
            case WAVE_SQUARE: osc_img[i] = square; break;
            case WAVE_TRI: osc_img[i] = tri; break;
            default: break;        
        }
    }
    if (this_osc == 0) draw_box(0, 0, 32, 17);
    if (this_osc == 1) draw_box(32, 0, 32, 17);
    draw_image(1, 1, osc_img[0], 30, 15, false);
    draw_image(33,1, osc_img[1], 30, 15, false);

    // Env
    sprintf(buf, "Env%d", this_env+1);
    draw_text(65, 3, buf, 0);

#ifdef PERFDEBUG
    float load = 100 * (float)(loop_time) / transfer_time;
    sprintf(buf, "load %.1f", (double)load);
    draw_text(0, 48, buf, 0);
#endif

}



void draw_menu(Menu menu) {

    for (int i=0; i<menu.num_items; i++) {
        if (menu.selected_item == i) draw_rect(0, 16*i, 128, 16, 1);
        draw_text(2, 2 + 16*i, menu.items[i].name, (menu.selected_item == i));

        menu.draw(&menu, i);
        draw_text_rj(126, 2+16*i, menu.value, (menu.selected_item == i));
    }

}

void menu_fx_draw(Menu *menu, int i) {

    switch (i) {
        case 0:
            sprintf(menu->value, "%d", input.reverb_amount);
            break;
        case 1:
            sprintf(menu->value, "%d", input.wet_level);
            break;
        default:
            strcpy(menu->value, "");
            break;
    }
}

void menu_env_draw(Menu *menu, int i) {

    char *dests[] = {"Amp", "Osc freq", "Osc mod", "Noise"};

    switch (i) {
        case 0:
            sprintf(menu->value, "%s", dests[input.env_dest[0]]);
            break;
        case 1:
            sprintf(menu->value, "%d", input.env_amount[0]);
            break;
        case 2:
            sprintf(menu->value, "%s", dests[input.env_dest[1]]);
            break;
        case 3:
            sprintf(menu->value, "%d", input.env_amount[1]);
            break;
        default:
            strcpy(menu->value, "");
            break;            
    }

}

void menu_lfo_draw(Menu *menu, int i) {

    char *dests[] = {"Amp", "Osc freq", "Osc mod", "Noise"};

    switch (i) {
        case 0:
            sprintf(menu->value, "%s", dests[input.lfo_dest]);
            break;
        case 1:
            sprintf(menu->value, "%d", input.lfo_amount);
            break;
        default:
            strcpy(menu->value, "");
            break;            
    }

}
