#include "main.h"
#include "midi.h"
#include "notes.h"
#include "stm32f401_discovery.h"
#include "stm32f401_discovery_audio.h"
#include "board.h"
#include "synth.h"
#include "stm32f4xx_ll_tim.h"
#include "oled.h"

#include <math.h>

void hardware_init(void);
void SystemClock_Config(void);

void draw_adsr(void);

ControllerConfig ctrlcfg = CTRL_ENVELOPE;

typedef struct {
    int attack;
    int decay;
    int release;
    int sustain;
} InputSettings;
InputSettings input;

#define ADD_DELTA_CLAMPED(x, y) {(x) += (y); if ((x) > 127) (x) = 127; if ((x) < 0) (x) = 0;}



/******************************************************************************/
int main(void) { 

    HAL_Init();
    hardware_init();
     
    printf("Running.\r\n");
    gen_note_table();

    //display_fillrect(0, 0, 128, 128, 0x0000);
    //display_write(0, 0, 128, 128, (uint16_t*)oled_test);

    // for (int i=0; i<8; i++) {
    //     draw_text(0, i*16, "The quick brown fox ju", (3<<(i*2)));
    //     //HAL_Delay(1000);
    // }

    synth_start();

    cfgnew.freq = note[69];
    cfgnew.key = true;
    cfgnew.osc_wave = WAVE_SAW;
    control_updated = true;

    while (1) {
        bool evt = read_buttons();
        if (evt) {
            if (buttons[BUTTON_ENVELOPE] == BTN_PRESSED) {
                printf("btn down\r\n");            
            } else if (buttons[BUTTON_ENVELOPE] == BTN_RELEASED) {
                printf("btn up\r\n");
            }

            if (buttons[BUTTON_FILTER] == BTN_PRESSED) {
                printf("filt\r\n");
            }
        }

        evt = read_encoders();
        if (evt) {
            switch (ctrlcfg) {
            case CTRL_ENVELOPE:

                // Attack
                if (encoders[ENC_RED].delta) {
                    ADD_DELTA_CLAMPED(input.attack, encoders[ENC_RED].delta);
                    cfgnew.attack_time = (float)input.attack/127 + 0.001f;
                    cfgnew.attack_rate = 1.0f/(cfg.attack_time * SAMPLE_RATE);                    
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


                control_updated = true;
                break;
            }
        }

        if (ctrlcfg == CTRL_ENVELOPE && control_updated) {
            control_updated = false;
            draw_adsr();
        }

        //printf("%d\t%d\t%d\r\n", encoders[0].value, encoders[0].last_value, encoders[0].delta);

        //         case '1': ctrlcfg = CTRL_MAIN; printf("MAIN\r\n"); break;
        //         case '2': ctrlcfg = CTRL_ENVELOPE; printf("ENVELOPE\r\n"); break;
        //         case '3': ctrlcfg = CTRL_FILTER; printf("FILTER\r\n"); break;
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

        HAL_Delay(2);

        //printf("***\r\n");
        // for (int i=0; i<MAX_ARP; i++) {
        //     printf("arp[%d] = %lu\r\n", i, cfgnew.arp_freqs[i]);
        // }
        //printf("%lu\t%lu\r\n", disp_time, loop_time);
        //printf("%d\r\n", pin_read(GPIOA, 1));

    }
  
}

void draw_adsr(void) {

    const uint8_t height = 64;
    const uint16_t attack_col  = 0xF800;
    const uint16_t decay_col   = 0x07E0;
    const uint16_t release_col = 0xFFFF;

    draw_rect(0, 0, 128, 128, 0x0000);

    char buf[32];
    sprintf(buf, "Attack   %.3f", cfgnew.attack_time);
    draw_text(0, 64, buf, attack_col);
    sprintf(buf, "Decay    %.3f", cfgnew.decay_time);
    draw_text(0, 80, buf, decay_col);
    sprintf(buf, "Sustain  %.3f", cfgnew.sustain_level);
    draw_text(0, 96, buf, 0x001F);
    sprintf(buf, "Release  %.3f", cfgnew.release_time);
    draw_text(0, 112, buf, release_col);


    uint8_t y, yold = 0;
    
    uint8_t offs;

    // Calculate the actual durations for decay and release.
    // (.decay_time and .release_time are 'nominal' values:
    // .decay_time assumes sustain=0, .release_time assumes sustain=1)
    float dr = cfgnew.env_curve / cfgnew.decay_time;
    float rr = cfgnew.env_curve / cfgnew.release_time;
    float decay_time_actual   = (float)log(ENV_OVERSHOOT / (1.0f - (cfgnew.sustain_level - ENV_OVERSHOOT))) / dr;
    float release_time_actual = (float)log(ENV_OVERSHOOT / (cfgnew.sustain_level + ENV_OVERSHOOT)) / rr;

    // Calculate time step per pixel, and divide the display into
    // attack/decay/release sections appropriately based on the total time
    float total = cfgnew.attack_time + decay_time_actual + release_time_actual;
    float time_step = total / 127;
    uint8_t attack_px = cfgnew.attack_time / time_step;
    uint8_t decay_px  = decay_time_actual / time_step;
    uint8_t release_px = release_time_actual / time_step;

    if (decay_px == 0) {
        decay_px++;
    }

    // Attack
    draw_line(0, height, attack_px, 0, attack_col);

    // Decay
    offs = attack_px;
    float b = cfgnew.sustain_level - ENV_OVERSHOOT;
    for (uint8_t x=0; x<decay_px; x++) {
        float v = b + (1.0f-b) * (float)exp(dr * x * time_step);
        y = height * (1.0f-v);
        if (x>0) draw_line(offs + x-1, yold, offs + x, y, decay_col);
        yold = y;
    }
    offs += decay_px;

    // Release
    b = -ENV_OVERSHOOT;
    for (uint8_t x=0; x<release_px; x++) {
        float v = b + (cfg.sustain_level-b)* (float)exp(rr * x * time_step);
        y = height * (1.0f-v);
        draw_line(offs + x-1, yold, offs + x, y, release_col);
        yold = y;
    }

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

