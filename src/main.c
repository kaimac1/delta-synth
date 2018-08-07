#include "main.h"
#include "midi.h"
#include "notes.h"
#include "audio.h"
#include "board.h"
#include "synth.h"
#include "ui.h"
#include "stm32f4xx_ll_tim.h"

void hardware_init(void);
void SystemClock_Config(void);

/******************************************************************************/
int main(void) { 

    HAL_Init();
    hardware_init();
    printf("Running.\r\n");
    build_font_index();
    gen_note_table();
    create_wave_tables();
    //synth_start();

    cfgnew.volume = 70;
    
    pin_cfg_output(GPIOA, 1<<5);    // Nucleo LED
    pin_set(GPIOA, 1<<5, 1);

    char buf[32];
    uint32_t ctr=0;

    while (1) {

        uint32_t sw = read_buttons();

        draw_rect(0, 0, 128, 64, 0);
        sprintf(buf, "ctr=%lu, sw=%lu", ctr, sw);
        draw_text(0, (ctr/64)%64, buf, 1);
        
        display_draw();
        HAL_Delay(25);
        ctr++;
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

