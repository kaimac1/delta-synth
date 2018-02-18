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

void draw_arc(uint16_t x, uint16_t y, uint16_t radius, float start_angle, float end_angle, uint16_t colour) {

    float step = 2.0f * 3.1415926f / 128;
    float xold = -1;
    float yold = -1;

    for (float a=start_angle; a<end_angle; a+=step) {
        float ynew = y + 0.5f + radius*cosf(a);
        float xnew = x + 0.5f - radius*sinf(a);
        if (xold != -1) draw_line(xold, yold, xnew, ynew, colour);
        xold = xnew;
        yold = ynew;
    }

}

void draw_adsr(void) {

    char buf[32];

    draw_rect(0, 0, 128, 128, 0x0000);
    draw_text(0,  0,  "Attack", 1, COL_RED);
    // draw_text(64, 80,  "D", 1, COL_GREEN);
    // draw_text(0,  112, "S", 1, COL_BLUE);
    // draw_text(64, 112, "R", 1, COL_WHITE);

    sprintf(buf, "%d", input.attack);
    draw_text(0+12, 74, buf, 2, COL_RED);
    
    // sprintf(buf, "%d", input.decay);
    // draw_text(64+12, 74, buf, 2, COL_GREEN);
    
    sprintf(buf, "%d", input.sustain);
    draw_text(0+12, 104, buf, 2, COL_BLUE);
    
    sprintf(buf, "%d", input.release);
    draw_text(64+12, 104, buf, 2, COL_WHITE);

    int cx = 32;
    int cy = 32;
    int cr = input.sustain;

    float attack = input.attack / 127.0f;
    float rad = attack * 3.1415926f * 2;

    int py = cy + cr*cosf(rad);
    int px = cx - cr*sinf(rad);

    for (int r=0; r<input.release; r++) {
        draw_arc(cx, cy, cr+r, 0.0f, 2*3.1415926f, 0x1111);
    }


    //draw_line(cx, cy+cr/2, cx, cy+cr, COL_RED);
    draw_line((cx+px)/2, (cy+py)/2, px, py, COL_RED);

    for (int r=0; r<input.release; r++) {
        draw_arc(cx, cy, cr+r, 0.0f, rad, COL_RED);
    }

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

    // Enable all GPIO clocks
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_SYSCFG_CLK_ENABLE();

    SystemClock_Config();

    uart_init();
    input_init();
    display_init();

    // Microsecond timer
    __HAL_RCC_TIM2_CLK_ENABLE();
    LL_TIM_InitTypeDef tim;
    tim.Prescaler = (SystemCoreClock / 1000000) - 1;
    tim.CounterMode = LL_TIM_COUNTERMODE_UP;
    tim.Autoreload = UINT32_MAX;
    tim.ClockDivision = 0;
    tim.RepetitionCounter = 0;
    LL_TIM_Init(TIM2, &tim);
    LL_TIM_EnableCounter(TIM2);

}


void SystemClock_Config(void) {

    RCC_ClkInitTypeDef RCC_ClkInitStruct;
    RCC_OscInitTypeDef RCC_OscInitStruct;

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

    // 84 MHz
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = 8;
    RCC_OscInitStruct.PLL.PLLN = 336;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
    RCC_OscInitStruct.PLL.PLLQ = 7;
    HAL_RCC_OscConfig(&RCC_OscInitStruct);

    RCC_ClkInitStruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2);
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;  
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;  
    HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2);
}

