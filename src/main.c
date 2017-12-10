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

/******************************************************************************/
int main(void) { 

    HAL_Init();
    hardware_init();
     
    printf("Running.\r\n");
    gen_note_table();

    display_fillrect(0, 0, 128, 128, 0x0000);
    //display_write(0, 0, 128, 128, (uint16_t*)oled_test);

    synth_start();

    cfgnew.freq = note[69];
    cfgnew.key = true;
    cfgnew.osc_wave = WAVE_SAW;

    while (1) {
        read_buttons();

        if (buttons[0] == BTN_PRESSED) {
            printf("btn down\r\n");            
        } else if (buttons[0] == BTN_RELEASED) {
            printf("btn up\r\n");
        }

        if (ctrlcfg == CTRL_ENVELOPE && control_updated) {
            control_updated = false;
            printf("ADSR: %.3f %.3f %.3f %.3f\r\n", cfgnew.attack_time, cfgnew.decay_time, cfgnew.sustain_level, cfgnew.release_time);
            draw_adsr();
        }

        // if (HAL_UART_Receive(&h_uart_debug, (uint8_t*)&inchar, 1, 100) == HAL_OK) {
        //     switch (inchar) {
        //         // Controller pages
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

        //     }
        // }

        // x++;
        // if (x == 32) x = 0;
        // uint32_t start_time = LL_TIM_GetCounter(TIM2);
        // display_fillrect(x, x, 96, 96, 0xFF00);
        // uint32_t disp_time = LL_TIM_GetCounter(TIM2) - start_time;
        uint32_t disp_time = 0;
        HAL_Delay(30);

        //printf("***\r\n");
        // for (int i=0; i<MAX_ARP; i++) {
        //     printf("arp[%d] = %lu\r\n", i, cfgnew.arp_freqs[i]);
        // }

        //printf("%lu\t%lu\r\n", disp_time, loop_time);
        //printf("%d\r\n", pin_read(GPIOA, 1));

    }
  
}

void draw_adsr(void) {

    display_fillrect(0, 0, 128, 128, 0x0000);

    uint8_t y, yold=0;
    uint8_t height = 64;

    float dr = cfgnew.env_curve / cfgnew.decay_time;
    float rr = cfgnew.env_curve / cfgnew.release_time;
    float decay_time_actual   = log(ENV_OVERSHOOT / (1.0f - (cfgnew.sustain_level - ENV_OVERSHOOT))) / dr;
    float release_time_actual = log(ENV_OVERSHOOT / (cfgnew.sustain_level + ENV_OVERSHOOT)) / rr;

    float total = cfgnew.attack_time + decay_time_actual + release_time_actual;
    float time_step = total / 128;

    uint8_t attack_px = cfgnew.attack_time / time_step;
    uint8_t decay_px  = decay_time_actual / time_step;
    uint8_t release_px = release_time_actual / time_step;

    draw_line(0, height, attack_px, 0, 0xFF00);

    // Decay
    uint8_t offs = attack_px;
    float b = cfgnew.sustain_level - ENV_OVERSHOOT;
    for (uint8_t x=0; x<decay_px; x++) {
        float v = b + (1.0f-b)*exp(dr * x * time_step);
        y = height * (1.0f-v);
        if (x>0) draw_line(offs + x-1, yold, offs + x, y, 0x0FF0);
        yold = y;
    }
    offs += decay_px;

    // Release
    b = -ENV_OVERSHOOT;
    for (uint8_t x=0; x<release_px; x++) {
        float v = b + (cfg.sustain_level-b)*exp(rr * x * time_step);
        y = height * (1.0f-v);
        if (x>0) draw_line(offs + x-1, yold, offs + x, y, 0x00FF);
        yold = y;
    }

    // float total = 
    // printf("total = %f\r\n", total);



    // env += cfg.decay_rate * (cfg.sustain_level - ENV_OVERSHOOT - env);
    // if (env < cfg.sustain_level) {
    //     env = cfg.sustain_level;
    //     env_state = ENV_SUSTAIN;
    // }

    // } else if (env_state == ENV_SUSTAIN) {
    //     env = cfg.sustain_level;
    // }

    // } else {
    //     env_state = ENV_RELEASE;
    //     env += cfg.release_rate * (-ENV_OVERSHOOT - env);
    //     if (env < 0.0f) env = 0.0f;
    // }


    // uint8_t size = 96;
    // uint8_t sustain_w = 31;

    // sustain_y = (1.0f - cfg.sustain_level) * size;

    // if (cfg.sustain_level > 0.0f) {
    //     total_adr = cfg.attack_time + cfg.decay_time + cfg.release_time;
    //     attack_w  = cfg.attack_time/total_adr * 96;
    //     decay_w   = cfg.decay_time/total_adr * 96;
    // } else {
    //     total_adr = cfg.attack_time + cfg.decay_time;
    //     attack_w  = cfg.attack_time/total_adr * 128;
    //     decay_w   = cfg.decay_time/total_adr * 128;       
    // }

    // draw_line(0, size, attack_w, 0, 0x00FF);
    // draw_line(attack_w, 0, attack_w+decay_w, sustain_y, 0xFF00);
    // if (cfg.sustain_level > 0.0f) {
    //     draw_line(attack_w+decay_w, sustain_y, attack_w+decay_w+sustain_w, sustain_y, 0x0FF0);
    //     draw_line(attack_w+decay_w+sustain_w, sustain_y, 127, size, 0xF0F0);
    // }




}

void hardware_init(void) {

    // Enable all GPIO clocks
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();

    SystemClock_Config();

    BSP_LED_Init(LED3);
    BSP_LED_Init(LED4); 
    BSP_LED_Init(LED5);
    BSP_LED_Init(LED6); 
    
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

