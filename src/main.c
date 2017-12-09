#include "main.h"
#include "notes.h"
#include "stm32f401_discovery.h"
#include "stm32f401_discovery_audio.h"
#include "board.h"
#include "synth.h"
#include "stm32f4xx_ll_tim.h"
#include "oled.h"

void hardware_init(void);
void SystemClock_Config(void);

ControllerConfig ctrlcfg;

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
            display_fillrect(0, 0, 32, 32, 0x0FF0);
        } else if (buttons[0] == BTN_RELEASED) {
            printf("btn up\r\n");
            display_fillrect(0, 0, 32, 32, 0x0000);
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

