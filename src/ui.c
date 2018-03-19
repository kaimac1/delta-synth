#include "ui.h"
#include "main.h"
#include "midi.h"
#include "notes.h"
#include "audio.h"
#include "board.h"
#include "synth.h"
#include "stm32f4xx_ll_tim.h"
#include <math.h>

#define COL_RED     0x00F8
#define COL_GREEN   0xE007
#define COL_BLUE    0x1F00
#define COL_WHITE   0x18C6
#define CRED rgb(192,10,10)
#define CGRN rgb(10,192,10)
#define CBLU rgb(10,10,192)
#define CWHT rgb(192,192,192)
#define CGREY 0x8210

#define TWOPI 6.2831853f
#define ADD_DELTA_CLAMPED(x, y) {(x) += (y); if ((x) > 127) (x) = 127; if ((x) < 0) (x) = 0;}

typedef enum {
    PAGE_OSC1,
    PAGE_ENV,
    PAGE_FILTER,
    PAGE_FX,
    NUM_PAGES
} UIPage;
UIPage page = PAGE_FX;

typedef struct {
    int attack;
    int decay;
    int release;
    int sustain;

    int cutoff;
    int resonance;
    int env_mod;

    int osc0_wave;
    int osc0_folding;
    int osc0_duty;
    int osc0_gain;
    int osc0_detune;

    int fx_ncombs;
    int fx_damping;
    int fx_amount;
} InputSettings;
InputSettings input;


bool redraw = true;
void draw_screen(void);


void ui_update(void) {

    bool evt = read_buttons();
    if (evt) {
        if (buttons[BUTTON_ENVELOPE] == BTN_PRESSED) {
            page++;
            page %= NUM_PAGES;
            redraw = true;
        }
        // if (buttons[BUTTON_FILTER] == BTN_PRESSED) {
        //     page = PAGE_FILTER;
        //     redraw = true;
        // }
        // if (buttons[BUTTON_OSC] == BTN_PRESSED) {
        //     page = PAGE_OSC;
        //     redraw = true;
        // }
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

        case PAGE_OSC1:
            // Waveform selector
            if (encoders[ENC_RED].delta) {
                ADD_DELTA_CLAMPED(input.osc0_wave, encoders[ENC_RED].delta);
                cfgnew.osc[0].waveform = input.osc0_wave > 42 ? (input.osc0_wave > 85 ? WAVE_TRI : WAVE_SQUARE) : WAVE_SAW;
            }

            if (encoders[ENC_GREEN].delta) {
                ADD_DELTA_CLAMPED(input.osc0_folding, encoders[ENC_GREEN].delta);
                cfgnew.osc[0].folding = 2.0f * (float)input.osc0_folding/127;
            }
            // Detune
            if (encoders[ENC_BLUE].delta) {
                ADD_DELTA_CLAMPED(input.osc0_detune, encoders[ENC_BLUE].delta);
                cfgnew.osc[0].detune = 1.0f + 0.1f * (float)input.osc0_detune/127;
            }
            // Gain
            if (encoders[ENC_WHITE].delta) {
                ADD_DELTA_CLAMPED(input.osc0_gain, encoders[ENC_WHITE].delta);
                cfgnew.osc[0].gain = (float)input.osc0_gain/127;
            }



            redraw = true;
            break;

        case PAGE_FX:
            if (encoders[ENC_RED].delta) {
                ADD_DELTA_CLAMPED(input.fx_ncombs, encoders[ENC_RED].delta);
                if (input.fx_ncombs > 8) input.fx_ncombs = 8;
                cfgnew.ncombs = input.fx_ncombs;
            }

            if (encoders[ENC_GREEN].delta) {
                ADD_DELTA_CLAMPED(input.fx_damping, encoders[ENC_GREEN].delta);
                cfgnew.fx_damping = (float)input.fx_damping/127;
            }

            if (encoders[ENC_BLUE].delta) {
                ADD_DELTA_CLAMPED(input.fx_amount, encoders[ENC_BLUE].delta);
                float a = -1.0f / logf(1.0f - 0.3f);
                float b = 127.0f / (logf(1.0f - 0.98f) * a + 1.0f);
                float fb = 1.0f - expf(((float)input.fx_amount - b) / (a*b));
                cfgnew.fx_combg = fb;
            }            
            redraw = true;
            break;

        default:
            break;
        }
    }

    //if (midi_event) {
        //redraw = true;
        //midi_event = false;
    //}

    // Redraw if required
    if (redraw) {
        draw_screen();
        if (display_draw()) redraw = false;
    }

    HAL_Delay(10);

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
        uint16_t col = (a < angle) ? colour : CGREY;
        draw_line(xf + rad1*sina, yf + rad1*cosa, xf + rad2*sina, yf + rad2*cosa, col);
    }    

}

void draw_screen(void) {

    char buf[32];
    uint32_t time;
    char *wave;

    if (display_busy) return;

    draw_rect(0, 0, 128, 128, 0x0000);

    switch (page) {
        case PAGE_ENV:
            time = LL_TIM_GetCounter(TIM2);
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

            float load = 100 * ((float)loop_time / transfer_time);
            sprintf(buf, "%.1f", load);
            draw_text(64,0, buf, 1, COL_WHITE);
            break;


        case PAGE_FILTER:
            draw_text(0,  0,   "Filter",  1, COL_WHITE);

            draw_text_cen(32,  16,   "CUTOFF",  1, CRED);
            draw_gauge(32, 52, input.cutoff / 127.0f, CRED);

            draw_text_cen(96, 16,   "RESONANCE",   1, CGRN);
            draw_gauge(96, 52, input.resonance / 127.0f, CGRN);

            draw_text_cen(32,   116, "ENV MOD", 1, CBLU);
            draw_gauge(32, 94, input.env_mod / 127.0f, CBLU);

            char buf[32];
            for (int i=0; i<NUM_VOICE; i++) {
                if (cfg.key[i]) {
                    //sprintf(buf, "%d: %lu", i, cfg.freq[i] / 1000);
                } else {
                    //sprintf(buf, "%d: --", i);
                }
                //draw_text(64, 64+12*i, buf, 1, 0xFFFF);
            }
            break;


        case PAGE_OSC1:
            draw_text(0,  0,   "Osc 1",  1, COL_WHITE);

            draw_text_cen(32,  16,   "WAVEFORM",  1, CRED);
            if (cfg.osc[0].waveform == WAVE_SAW) {
                wave = "Saw";
            } else if (cfg.osc[0].waveform == WAVE_SQUARE) {
                wave = "Square";
                draw_text_cen(96, 16, "SYMMETRY", 1, CGRN);
                draw_gauge(96, 52, input.osc0_duty / 127.0f, CGRN);
            } else {
                wave = "Tri";
                draw_text_cen(96, 16, "FOLDING", 1, CGRN);
                draw_gauge(96, 52, input.osc0_folding / 127.0f, CGRN);                
            }
            draw_text_cen(32,  40, wave,  1, CRED);

            draw_text_cen(32,   116, "DETUNE", 1, CBLU);
            draw_gauge(32, 94, input.osc0_detune / 127.0f, CBLU);
            
            draw_text_cen(96,  116, "GAIN", 1, CWHT);
            draw_gauge(96, 94, input.osc0_gain / 127.0f, CWHT);


            break;

        case PAGE_FX:
            draw_text_cen(32,  16,   "COMBS",  1, CRED);
            sprintf(buf, "%d", input.fx_ncombs);
            draw_text_cen(32,  40, buf,  1, CRED);

            draw_text_cen(96, 16, "DAMPING", 1, CGRN);
            draw_gauge(96, 52, input.fx_damping / 127.0f, CGRN);

            draw_text_cen(32,   116, "AMOUNT", 1, CBLU);
            draw_gauge(32, 94, input.fx_amount / 127.0f, CBLU);

            break;


        default:
            break;
    }

}

