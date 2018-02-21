#include "main.h"
#include "midi.h"
#include "notes.h"
#include "audio.h"
#include "board.h"
#include "synth.h"
#include "stm32f4xx_ll_tim.h"

#include <math.h>

void hardware_init(void);
void SystemClock_Config(void);
void draw_adsr(void);
void draw_filter(void);
void draw_osc(void);

ControllerConfig ctrlcfg = CTRL_ENVELOPE;
bool redraw;

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

#define COL_RED     0xF800
#define COL_GREEN   0x07E0
#define COL_BLUE    0x001F
#define COL_WHITE   0xC618

#define ADD_DELTA_CLAMPED(x, y) {(x) += (y); if ((x) > 127) (x) = 127; if ((x) < 0) (x) = 0;}

/******************************************************************************/
int main(void) { 

    HAL_Init();
    hardware_init();
    printf("Running.\r\n");
    build_font_index();
    gen_note_table();
    synth_start();

    cfgnew.volume = 60;
    redraw = true;

    pin_cfg_output(GPIOA, 1<<5);
    pin_set(GPIOA, 1<<5, 1);

    while (1) {
        bool evt = read_buttons();
    //     if (evt) {
    //         if (buttons[BUTTON_ENVELOPE] == BTN_PRESSED) {
    //             ctrlcfg = CTRL_ENVELOPE;
    //             redraw = true;
    //         }
    //         if (buttons[BUTTON_FILTER] == BTN_PRESSED) {
    //             ctrlcfg = CTRL_FILTER;
    //             redraw = true;
    //         }
    //         if (buttons[BUTTON_OSC] == BTN_PRESSED) {
    //             ctrlcfg = CTRL_OSC;
    //             redraw = true;
    //         }
    //     }

        evt = read_encoders();
        if (evt) {
            switch (ctrlcfg) {
            case CTRL_ENVELOPE:

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

            case CTRL_FILTER:

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

            case CTRL_OSC:

                // 

                redraw = true;
                break;
            }
        }

        // Redraw if required
        if (redraw) {
            redraw = false;
            if (ctrlcfg == CTRL_ENVELOPE) {
                draw_adsr();
            } else if (ctrlcfg == CTRL_FILTER) {
                draw_filter();
            }
        }

        //         // Legato
        //         case 'l': cfgnew.legato ^= 1; break;
        //         // arpeggio
        //         case 'a':
        //             cfgnew.arp++;
        //             cfgnew.arp %= 4;
        //             printf("arp=%d\r\n", cfgnew.arp);
        //             break;
        //         // sync
        //         case 's':
        //             cfgnew.sync ^= 1; break;
        //printf("***\r\n");
        // for (int i=0; i<MAX_ARP; i++) {
        //     printf("arp[%d] = %lu\r\n", i, cfgnew.arp_freqs[i]);
        // }
        //printf("%lu\r\n", loop_time);
    }
  
}

#define TWOPI 6.2831853f

void draw_arc(uint16_t x, uint16_t y, uint16_t radius, uint16_t width, float start_angle, float end_angle, uint16_t colour) {

    const int segments = 700;
    float step = TWOPI / segments;
    float xf = x + 0.5f;
    float yf = y + 0.5f;
    
    for (float a=start_angle; a<end_angle; a+=step) {
        float sina = -sinf(a);
        float cosa = cosf(a);
        float rad1 = radius;
        float rad2 = radius + width;
        draw_line(xf + rad1*sina, yf + rad1*cosa, xf + rad2*sina, yf + rad2*cosa, colour);
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
    int thickness = 7;

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
    draw_text(0,  0,   "Attack",  1, COL_RED);
    draw_text(64, 0,   "Decay",   1, COL_GREEN);
    draw_text(0,  64, "Sustain", 1, COL_BLUE);
    draw_text(64, 64, "Release", 1, COL_WHITE);

    draw_gauge(32, 40, input.attack / 127.0f, COL_RED);
    draw_gauge(96, 40, input.decay / 127.0f, COL_GREEN);
    draw_gauge(32, 104, input.sustain / 127.0f, COL_BLUE);
    draw_gauge(96, 104, input.release / 127.0f, COL_WHITE);

    time = LL_TIM_GetCounter(TIM2) - time;

    // sprintf(buf, "draw %lu", time);
    // draw_text(0,64, buf, 1, COL_WHITE);   
    // sprintf(buf, "disp %lu", display_write_time);
    // draw_text(0,80, buf, 1, COL_WHITE);
    // sprintf(buf, "loop %lu", loop_time);
    // draw_text(0,96, buf, 1, COL_WHITE);
    // sprintf(buf, "txfer %lu", transfer_time);
    // draw_text(0,112, buf, 1, COL_WHITE);    

    display_draw();

}

void draw_filter(void) {

    draw_rect(0, 0, 128, 128, 0x0000);

    char buf[32];
    
    draw_text(0, 0, "Cutoff", 1, COL_RED);
    sprintf(buf, "%d", input.cutoff);
    draw_text(0, 16, buf, 3, COL_RED);
    
    draw_text(64, 0, "Resonance", 1, COL_GREEN);
    sprintf(buf, "%d", input.resonance);
    draw_text_rj(128, 16, buf, 3, COL_GREEN);

    draw_text(0, 64, "Env. mod.", 1, COL_BLUE);
    sprintf(buf, "%d", input.env_mod);
    draw_text(0, 80, buf, 3, COL_BLUE);

    display_draw();

}

void hardware_init(void) {

    SystemClock_Config();

    // Enable all GPIO clocks
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_SYSCFG_CLK_ENABLE();

    uart_init();
    input_init();
    timer_init();
    display_init();
    


}


void SystemClock_Config(void) {

    RCC_ClkInitTypeDef RCC_ClkInitStruct;
    RCC_OscInitTypeDef RCC_OscInitStruct;

    __HAL_PWR_OVERDRIVE_ENABLE();
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    // 180 MHz
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = 8;             // 1 MHz PLL input
    RCC_OscInitStruct.PLL.PLLN = 360;           // 360 MHz fVCO
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2; // 180 MHz PLL output
    RCC_OscInitStruct.PLL.PLLQ = 7;             // USB clock - don't care
    HAL_RCC_OscConfig(&RCC_OscInitStruct);

    RCC_ClkInitStruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2);
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;          // CPU  = 180 MHz
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;           // APB1 =  45 MHz
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;           // APB2 =  90 MHz
    HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5);        


}

