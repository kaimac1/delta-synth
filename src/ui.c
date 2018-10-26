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
    PART_LEAD,
    PART_DRUMS,
    NUM_PARTS
} UIPart;

typedef enum {
    UI_DEFAULT,
    UI_OSC_TUNE,
    UI_FX,
    UI_ENV_MENU
} UIPage;

// UI state
int this_osc;
int this_env;
UIPage page;
UIPart part;

uint16_t filtered_pot[NUM_POTS];
uint16_t saved_pot[NUM_POTS];
bool pot_sync[NUM_POTS];
int encoder_start = 0;
int pot_moved = -1;
int pot_show_timer;

bool detune_mode;

typedef struct {
    int reverb_amount;
    int wet_level;
    EnvDest env_dest[NUM_ENV];
    int env_amount[NUM_ENV];
} InputSettings;
InputSettings input;

unsigned int seq_idx;
float seq_note_input;

Menu menu_fx;
Menu menu_env;

bool redraw = true;

void draw_screen(void);
void menu_fx_draw(Menu *menu, int i);
void menu_env_draw(Menu *menu, int i);

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

            switch (i) {
                // Oscillators
                case 0:
                    synth.osc_balance = amount;
                    break;
                case 1:
                    synth.osc[this_osc].modifier = amount;
                    break;
                case 2:
                    temp = 2 * (amount - 0.5f);
                    temp = pow(halfstep, temp);
                    synth.osc[this_osc].detune = temp;
                    break;

                // Enevelope
                case 3:
                    temp = amount + MIN_ATTACK;
                    synth.env[this_env].attack = 1.0f/(temp * SAMPLE_RATE);
                    break;
                case 4:
                    temp = amount + DECAY_MIN;
                    synth.env[this_env].decay = LEAD_DECAY_CONST / (temp*temp*temp);
                    break;
                case 5:
                    synth.env[this_env].sustain = amount;
                    break;
                case 6:
                    temp = amount + DECAY_MIN;
                    synth.env[this_env].release = LEAD_DECAY_CONST / (temp*temp*temp);
                    break;

                // Filter
                case 9:
                    synth.cutoff = 10000.0f * amount;
                    break;
                case 10:
                    synth.resonance = 3.99f * amount;
                    break;
                case 11:
                    synth.env_mod = 5000.0f * amount;
                    break;

                // LFO
                case 7:
                    synth.lfo[0].rate = 0.001f * amount;
                    break;
            }
        }
    }

    synth.noise_gain = 0.0f;//pots[2] / POTMAX;

    synth.env_dest[0] = input.env_dest[0];
    synth.env_dest[1] = input.env_dest[1];
    synth.env_amount[0] = input.env_amount[0] / 127.0f;
    synth.env_amount[1] = input.env_amount[1] / 127.0f;

}



void ui_init(void) {

    for (int i=0; i<NUM_POTS; i++) {
        pot_sync[i] = true;
        filtered_pot[i] = pots[i];
        saved_pot[i] = filtered_pot[i];
    }

    menu_fx.draw = (void*)menu_fx_draw;
    menu_fx.num_items = 2;
    menu_fx.items[0].name = "Reverb amt";
    menu_fx.items[1].name = "Wet level";
    
    menu_env.draw = (void*)menu_env_draw;
    menu_env.num_items = 2;
    menu_env.items[0].name = "Dest 1";
    menu_env.items[1].name = "Dest 1 amount";

    input.reverb_amount = 64;
    input.wet_level = 64;
    set_reverb();

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
            Wave new = (synth.osc[this_osc].waveform + 1) % NUM_WAVE;
            synth.osc[this_osc].waveform = new;
            redraw = true;
        }

        // Tune
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
        check_button_for_page(4, UI_ENV_MENU);

        // FX menu
        check_button_for_page(5, UI_FX);

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
                    if (menu_env.selected_item == 0) {
                        ADD_CLAMP(input.env_dest[this_env], encoder.half_delta, NUM_ENVDEST-1);
                    } else if (menu_env.selected_item == 1) {
                        ADD_CLAMP_MINMAX(input.env_amount[this_env], encoder.delta, -127, 127);
                    }
                } else {
                    menu_scroll(&menu_env);
                }
                redraw = true;
                break;

            default:
                break;
        }

    }

    update_lead();

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

    char buf[32];

    if (pot_moved != -1) {
        draw_box(16,16,96,32);
        sprintf(buf, "%d: %d", pot_moved, filtered_pot[pot_moved]);
        draw_text_cen(64, 34, buf, 0);
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

    char *dests[] = {"Amp", "Osc1 pitch", "Osc1+2 pitch", "Osc1 mod", "Osc1+2 mod"};

    switch (i) {
        case 0:
            sprintf(menu->value, "%s", dests[input.env_dest[this_env]]);
            break;
        case 1:
            sprintf(menu->value, "%d", input.env_amount[this_env]);
            break;
        default:
            strcpy(menu->value, "");
            break;            
    }

}
