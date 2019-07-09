#include "ui.h"
#include "sequencer.h"
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
    UI_KBD_TRACK
} UIPage;

typedef enum {
    UIMODE_SYNTH,
    UIMODE_SEQUENCER
} UIMode;

// UI state
int part;
int this_osc;
int this_env;
UIPage page;
UIMode uimode = UIMODE_SYNTH;
PlaybackMode mode;


uint16_t filtered_pot[NUM_POTS];
uint16_t saved_pot[NUM_POTS];
bool pot_sync[NUM_POTS];
int encoder_start = 0;
int pot_moved = -1;
int pot_show_timer;

bool tune_semitones = true;

typedef struct {
    int reverb_amount;
    int wet_level;
    ModDest env_dest[NUM_ENV];
    int env_amount[NUM_ENV];
    ModDest lfo_dest;
    int lfo_amount;
    int keyboard_track;
} InputSettings;
InputSettings input;

Menu menu_fx;
bool redraw = true;


void draw_screen(void);
void menu_fx_draw(Menu *menu, int i);
void menu_lfo_draw(Menu *menu, int i);

#define SAVE_POT(i) {saved_pot[(i)] = filtered_pot[(i)]; pot_sync[(i)] = 0;}

#define POT_UPDATE_THRESHOLD 50
#define POTMAX 4095.0f
#define DECAY_MIN 0.060f
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

                case POT_VOL:
                    synth.volume = amount;
                    break;                

                // Enevelope
                case POT_ATTACK:
                    temp = amount + MIN_ATTACK;
                    synth.part[part].env[this_env].attack = 1.0f/(temp * SAMPLE_RATE);
                    break;
                case POT_DECAY:
                    temp = amount + DECAY_MIN;
                    synth.part[part].env[this_env].decay = LEAD_DECAY_CONST / (temp*temp*temp);
                    break;
                case POT_SUSTAIN:
                    synth.part[part].env[this_env].sustain = amount;
                    break;
                case POT_RELEASE:
                    temp = amount + DECAY_MIN;
                    synth.part[part].env[this_env].release = LEAD_DECAY_CONST / (temp*temp*temp);
                    break;                    

                // Oscillator
                case POT_OSC1:
                    synth.part[part].osc[0].gain = amount;
                    break;
                case POT_OSC2:
                    synth.part[part].osc[1].gain = amount;
                    break;

                case POT_MOD:
                    synth.part[part].osc[this_osc].modifier = amount;
                    break;
                case POT_TUNE:
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

                // Filter
                case POT_CUTOFF:
                    synth.part[part].cutoff = amount;
                    break;
                case POT_RESONANCE:
                    synth.part[part].resonance = 4.00f * amount;
                    break;
                case POT_ENVMOD:
                    synth.part[part].env_mod = amount;
                    break;

                // LFO
                case POT_LFORATE:
                    synth.part[part].lfo.rate = 0.002f * amount;
                    break;


            }
        }
    }

    synth.part[part].env_dest[0] = input.env_dest[0];
    synth.part[part].env_dest[1] = input.env_dest[1];
    synth.part[part].env_amount[0] = input.env_amount[0] / 127.0f;
    synth.part[part].env_amount[1] = input.env_amount[1] / 127.0f;

    synth.part[part].lfo.dest = input.lfo_dest;
    synth.part[part].lfo.amount = input.lfo_amount / 127.0f;

    synth.part[part].keyboard_track = input.keyboard_track / 127.0f;

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

void update_synth(void) {

    bool enc = read_encoder();

    // Keys:

    // Oscillator select
    if (buttons[BTN_OSC_SEL] == BTN_DOWN) {
        this_osc++;
        this_osc %= NUM_OSCILLATOR;
        SAVE_POT(POT_MOD);
        SAVE_POT(POT_TUNE);
        redraw = true;
    }

    // Oscillator waveform
    if (buttons[BTN_OSC_WAVE] == BTN_DOWN) {
        Wave new = (synth.part[part].osc[this_osc].waveform + 1) % NUM_WAVE;
        synth.part[part].osc[this_osc].waveform = new;
        redraw = true;
    }

    // Tune mode
    if (buttons[BTN_OSC_TUNE] == BTN_DOWN) {
        tune_semitones = !tune_semitones;
        SAVE_POT(POT_TUNE);
    }

    // Envelope select
    if (buttons[BTN_ENV_SEL] == BTN_DOWN) {
        this_env++;
        this_env %= NUM_ENV;
        SAVE_POT(POT_ATTACK);
        SAVE_POT(POT_DECAY);
        SAVE_POT(POT_SUSTAIN);
        SAVE_POT(POT_RELEASE);
        redraw = true;
    }

    check_button_for_page(BTN_ENV_DEST, UI_ENV_MENU);
    check_button_for_page(BTN_LFO_DEST, UI_LFO_MENU);        
    check_button_for_page(BTN_KBD_TRACK, UI_KBD_TRACK);
    check_button_for_page(10, UI_FX);

    // Encoder movement
    if (enc) {
        switch (page) {
            case UI_DEFAULT:
                break;

            case UI_OSC_TUNE:
                redraw = true;
                break;

            case UI_FX:
                if (buttons[BTN_SEQ_EDIT] == BTN_HELD) {
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
                ADD_CLAMP_MINMAX(input.env_amount[this_env], encoder.delta, -127, 127);
                redraw = true;
                break;

            case UI_LFO_MENU:
                ADD_CLAMP_MINMAX(input.lfo_amount, encoder.delta, -127, 127);
                redraw = true;
                break;

            case UI_KBD_TRACK:
                ADD_CLAMP(input.keyboard_track, encoder.delta, 127);
                redraw = true;
                break;

            default:
                break;
        }

    }    

}



void ui_update(void) {

    static int ctr;

    bool btn = read_buttons();

    if (btn) {
        // Switch between SYNTH/SEQ modes
        if (buttons[BTN_SYNTH_MENU] == BTN_DOWN) {
            uimode = UIMODE_SYNTH;
            redraw = true;
        } else if (buttons[BTN_SEQ_EDIT] == BTN_DOWN) {
            uimode = UIMODE_SEQUENCER;
            redraw = true;
        }
    }

    switch (uimode) {
        case UIMODE_SYNTH:
            update_synth();
            break;

        case UIMODE_SEQUENCER:
            update_sequencer();
            break;
    }



    update_lead();

    // Handle pot movements.
    // In the ENV and LFO dest modes, moving a pot changes the modulation dest.
    // Outside of those modes we want a notification on the screen showing the value of 
    // the updated pot.
    if (pot_moved >= 0) {
        if (page == UI_ENV_MENU) {
            // Change destination
            if (pot_moved == POT_MOD) {
                if (buttons[BTN_SEQ_EDIT] == BTN_HELD) {
                    input.env_dest[this_env] = DEST_MOD1;
                } else {
                    input.env_dest[this_env] = DEST_MOD;
                }
            }
            else if (pot_moved == POT_VOL) input.env_dest[this_env] = DEST_AMP;
            else if (pot_moved == POT_TUNE) input.env_dest[this_env] = DEST_FREQ;
            else if (pot_moved == POT_RESONANCE) input.env_dest[this_env] = DEST_RES;
            else if (pot_moved == POT_OSC1) input.env_dest[this_env] = DEST_OSC1;
            pot_moved = -1;

        } else if (page == UI_LFO_MENU) {
            // Change destination
            if (pot_moved == POT_MOD) {
                if (buttons[BTN_SEQ_EDIT] == BTN_HELD) {
                    input.lfo_dest = DEST_MOD1;
                } else {
                    input.lfo_dest = DEST_MOD;
                }
            }
            else if (pot_moved == POT_VOL) input.lfo_dest = DEST_AMP;
            else if (pot_moved == POT_TUNE) input.lfo_dest = DEST_FREQ;
            else if (pot_moved == POT_RESONANCE) input.lfo_dest = DEST_RES;
            else if (pot_moved == POT_CUTOFF) input.lfo_dest = DEST_CUTOFF;
            else if (pot_moved == POT_OSC1) input.lfo_dest = DEST_OSC1;
            pot_moved = -1;            

        } else {
            // Show control update
            pot_show_timer++;
            if (pot_show_timer == 100) {
                pot_show_timer = 0;
                pot_moved = -1;
                redraw = true;
            }
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


void draw_envdest(void);
void draw_lfo(void);
void draw_kbd_track(void);

// Names for pot moved box
const char *pot_names[NUM_POTS] = {
    "Volume", "Osc 1", "Modifier", "Attack", "Osc 2", "Osc tune", "Decay",
    "Cutoff", "LFO rate", "Sustain", "Peak", "User A", "Release", "Env mod", "User B"
};

void draw_screen(void) {

    if (display_busy) return;
    draw_rect(0, 0, 128, 64, 0);

    if (uimode == UIMODE_SEQUENCER) {
        draw_sequencer();
        return;
    }

    // Menus
    if (page == UI_FX) {
        draw_menu(menu_fx);
        return;
    }
    if (page == UI_ENV_MENU) {
        draw_envdest();
        return;
    }
    if (page == UI_LFO_MENU) {
        draw_lfo();
        return;
    }
    if (page == UI_KBD_TRACK) {
        draw_kbd_track();
        return;
    }

    char buf[32];

    // Show pot name/value on move
    if (pot_moved != -1) {
        draw_box(16,16,96,32);
        int amount = filtered_pot[pot_moved] / 32;
        sprintf(buf, "%s", pot_names[pot_moved]);
        draw_text_cen(64, 20, buf, 0);
        sprintf(buf, "%d", amount);
        draw_text_cen(64, 34, buf, 0);
    }

    // Osc
    uint8_t *osc_img[2] = {NULL};
    for (int i=0; i<2; i++) {
        switch(synth.part[part].osc[i].waveform) {
            case WAVE_SAW: osc_img[i] = saw; break;
            case WAVE_SQUARE: osc_img[i] = square; break;
            case WAVE_TRI: osc_img[i] = tri; break;
            case WAVE_NOISE: osc_img[i] = (uint8_t*)&synth; break; // TODO
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

char *dest_str[] = {"Amp", "Osc freq", "Osc mod", "Osc1 mod", "Peak", "Cutoff", "Osc1 gain"};

void draw_envdest(void) {

    char buf[32];

    sprintf(buf, "Env%d", this_env+1);
    draw_text_cen(64, 16, buf, 0);

    sprintf(buf, "%s", dest_str[input.env_dest[this_env]]);
    draw_text_cen(64, 32, buf, 0);

    sprintf(buf, "%d", input.env_amount[this_env]);
    draw_text_cen(64, 48, buf, 0);

}

void draw_lfo(void) {

    char buf[32];

    draw_text_cen(64, 16, "LFO", 0);

    sprintf(buf, "%s", dest_str[input.lfo_dest]);
    draw_text_cen(64, 32, buf, 0);

    sprintf(buf, "%d", input.lfo_amount);
    draw_text_cen(64, 48, buf, 0);

}

void draw_kbd_track(void) {

    char buf[32];

    draw_text_cen(64, 16, "Keyboard track", 0);

    sprintf(buf, "%d", input.keyboard_track);
    draw_text_cen(64, 32, buf, 0);

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
