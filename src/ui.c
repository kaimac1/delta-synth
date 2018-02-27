#include "ui.h"
#include "main.h"
#include "midi.h"
#include "notes.h"
#include "audio.h"
#include "board.h"
#include "synth.h"
#include "stm32f4xx_ll_tim.h"
#include <math.h>

#define COL_RED     0xF800
#define COL_GREEN   0x07E0
#define COL_BLUE    0x001F
#define COL_WHITE   0xC618
#define CRED rgb(192,10,10)
#define CGRN rgb(10,192,10)
#define CBLU rgb(10,10,192)
#define CWHT rgb(192,192,192)

#define TWOPI 6.2831853f
#define ADD_DELTA_CLAMPED(x, y) {(x) += (y); if ((x) > 127) (x) = 127; if ((x) < 0) (x) = 0;}

typedef enum {
    PAGE_OSC,
    PAGE_ENV,
    PAGE_FILTER
} UIPage;
UIPage page = PAGE_ENV;

typedef struct {
    int attack;
    int decay;
    int release;
    int sustain;
    int cutoff;
    int resonance;
    int env_mod;
} InputSettings;
InputSettings input;

bool redraw = true;


void ui_update(void) {

    bool evt = read_buttons();
    if (evt) {
        if (buttons[BUTTON_ENVELOPE] == BTN_PRESSED) {
            page = PAGE_ENV;
            redraw = true;
        }
        if (buttons[BUTTON_FILTER] == BTN_PRESSED) {
            page = PAGE_FILTER;
            redraw = true;
        }
        if (buttons[BUTTON_OSC] == BTN_PRESSED) {
            page = PAGE_OSC;
            redraw = true;
        }
    }

    evt = read_encoders();
    if (evt) {
        switch (page) {
        case PAGE_ENV:

            // Attack
            if (encoders[ENC_RED].delta) {
                ADD_DELTA_CLAMPED(input.attack, encoders[ENC_RED].delta);
                cfgnew.attack_time = (float)input.attack/127 + 0.001f;
                cfgnew.attack_rate = 1.0f/(cfgnew.attack_time * SAMPLE_RATE);                    
            }
            // Decay
            if (encoders[ENC_GREEN].delta) {
                ADD_DELTA_CLAMPED(input.decay, encoders[ENC_GREEN].delta);
                float value = (float)input.decay/127;
                value = value*value*value;
                cfgnew.decay_time = value * 5;
                cfgnew.decay_rate = 1.0 - exp(cfgnew.env_curve / (cfgnew.decay_time * SAMPLE_RATE));
            }
            // Sustain
            if (encoders[ENC_BLUE].delta) {
                ADD_DELTA_CLAMPED(input.sustain, encoders[ENC_BLUE].delta);
                float value = (float)input.sustain/127;
                cfgnew.sustain_level = value;
            }
            // Release
            if (encoders[ENC_WHITE].delta) {
                ADD_DELTA_CLAMPED(input.release, encoders[ENC_WHITE].delta);
                float value = (float)input.release/127;
                value = value*value*value;
                cfgnew.release_time = value * 5;
                cfgnew.release_rate = 1.0 - exp(cfgnew.env_curve / (cfgnew.release_time * SAMPLE_RATE));
            }

            redraw = true;
            break;

        case PAGE_FILTER:

            // Cutoff
            if (encoders[ENC_RED].delta) {
                ADD_DELTA_CLAMPED(input.cutoff, encoders[ENC_RED].delta);
                cfgnew.cutoff = 10000.0f * (float)input.cutoff/127;
            }
            // Resonance
            if (encoders[ENC_GREEN].delta) {
                ADD_DELTA_CLAMPED(input.resonance, encoders[ENC_GREEN].delta);
                cfgnew.resonance = 3.99f * (float)input.resonance/127;
            }
            // Envelope modulation
            if (encoders[ENC_BLUE].delta) {
                ADD_DELTA_CLAMPED(input.env_mod, encoders[ENC_BLUE].delta);
                cfgnew.env_mod = 5000.0f * (float)input.env_mod/127;
            }

            redraw = true;
            break;

        case PAGE_OSC:

            // 

            redraw = true;
            break;
        }
    }

    //if (midi_event) {
        redraw = true;
//        midi_event = false;
    //}

    // Redraw if required
    if (redraw) {
        redraw = false;
        if (page == PAGE_ENV) {
            draw_adsr();
        } else if (page == PAGE_FILTER) {
            draw_filter();
        }
    }

}

inline float fast_sin(float arg) {
    size_t idx = (size_t)(SINE_TABLE_SIZE * arg/TWOPI) + 1;
    return sine_table[idx] / 32768.0f;
}
inline float fast_cos(float arg) {
    size_t idx = (size_t)(SINE_TABLE_SIZE * arg/TWOPI) + 1;
    idx += SINE_TABLE_SIZE >> 2;
    idx %= SINE_TABLE_SIZE;
    return sine_table[idx] / 32768.0f;
}

void draw_gauge(uint16_t x, uint16_t y, float amount, uint16_t colour) {

    int radius = 16;
    int thickness = 23-radius;

    float start = TWOPI * 0.1f;
    float end = TWOPI * 0.9f;

    float angle = start + (end-start) * amount;

    int segments = 284; // Tune this so there are no gaps
    float step = TWOPI / segments;
    float xf = x + 0.5f;
    float yf = y + 0.5f;
    
    for (float a=start; a<end; a+=step) {
        float sina = -fast_sin(a);
        float cosa = fast_cos(a);
        float rad1 = radius;
        float rad2 = radius + thickness;
        uint16_t col = (a < angle) ? colour : 0x1082;
        draw_line(xf + rad1*sina, yf + rad1*cosa, xf + rad2*sina, yf + rad2*cosa, col);
    }    

}

void draw_adsr(void) {

    char buf[32];

    uint32_t time = LL_TIM_GetCounter(TIM2);

    draw_rect(0, 0, 128, 128, 0x0000);
    draw_text(0,  0,   "Envelope",  1, COL_WHITE);

    draw_text_cen(32,  16,   "ATTACK",  1, CRED);
    draw_gauge(32, 52, input.attack / 127.0f, CRED);

    draw_text_cen(96, 16,   "DECAY",   1, CGRN);
    draw_gauge(96, 52, input.decay / 127.0f, CGRN);
    
    draw_text_cen(32,   116, "SUSTAIN", 1, CBLU);
    draw_gauge(32, 94, input.sustain / 127.0f, CBLU);
    
    draw_text_cen(96,  116, "RELEASE", 1, CWHT);
    draw_gauge(96, 94, input.release / 127.0f, CWHT);

    time = LL_TIM_GetCounter(TIM2) - time;

    int fps = 1000000 / display_write_time;
    float load = 100 * ((float)loop_time / transfer_time);
    sprintf(buf, "%d/%.1f", fps, load);
    draw_text(64,0, buf, 1, COL_WHITE);

    display_draw();

}

void draw_filter(void) {

    draw_rect(0, 0, 128, 128, 0x0000);
    draw_text(0,  0,   "Filter",  1, COL_WHITE);

    draw_text_cen(32,  16,   "CUTOFF",  1, CRED);
    draw_gauge(32, 52, input.cutoff / 127.0f, CRED);

    draw_text_cen(96, 16,   "RESONANCE",   1, CGRN);
    draw_gauge(96, 52, input.resonance / 127.0f, CGRN);

    draw_text_cen(32,   116, "ENV MOD", 1, CBLU);
    draw_gauge(32, 94, input.env_mod / 127.0f, CBLU);

    char buf[32];
    for (int i=0; i<NUM_OSC; i++) {
        if (cfg.key[i]) {
            sprintf(buf, "%d: %lu", i, cfg.osc_freq[i] / 1000);
        } else {
            sprintf(buf, "%d: --", i);
        }
        draw_text(64, 64+12*i, buf, 1, 0xFFFF);
    }
    
    display_draw();

}
