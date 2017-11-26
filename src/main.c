#include "main.h"
#include "notes.h"
#include "stm32f401_discovery.h"
#include "stm32f401_discovery_audio.h"
#include "board.h"
#include "synth.h"

#include "stm32f4xx_ll_tim.h"

void SystemClock_Config(void);

ControllerConfig ctrlcfg;


/******************************************************************************/
int main(void) { 
 
    HAL_Init();
    BSP_LED_Init(LED3);
    BSP_LED_Init(LED4); 
    BSP_LED_Init(LED5);
    BSP_LED_Init(LED6); 
    SystemClock_Config();
    uart_init();
    input_init();
    printf("Running.\r\n");

    gen_note_table();

    __HAL_RCC_TIM2_CLK_ENABLE();
    LL_TIM_InitTypeDef tim;
    tim.Prescaler = (SystemCoreClock / 1000000) - 1;
    tim.CounterMode = LL_TIM_COUNTERMODE_UP;
    tim.Autoreload = UINT32_MAX;
    tim.ClockDivision = 0;
    tim.RepetitionCounter = 0;
    LL_TIM_Init(TIM2, &tim);
    LL_TIM_EnableCounter(TIM2);




    synth_start();

    char inchar;

    cfgnew.freq = note[69];
    cfgnew.key = true;
    cfgnew.osc_wave = WAVE_SAW;

    bool start_sweep = false;

    while (1) {
        if (HAL_UART_Receive(&h_uart_debug, (uint8_t*)&inchar, 1, 100) == HAL_OK) {
            switch (inchar) {
                case '1': ctrlcfg = CTRL_MAIN; printf("MAIN\r\n"); break;
                case '2': ctrlcfg = CTRL_ENVELOPE; printf("ENVELOPE\r\n"); break;
                case '3': ctrlcfg = CTRL_FILTER; printf("FILTER\r\n"); break;

                case 's': start_sweep = true; break;
            }
        }
        printf("%d\r\n", loop_time);

        if (start_sweep) {
            cfgnew.key = true;
            cfgnew.freq = 20.0f / SAMPLE_RATE * UINT32_MAX;
            while (cfgnew.freq < (15000.0f / SAMPLE_RATE * UINT32_MAX)) {
                cfgnew.freq *= 1.001f;
                HAL_Delay(5);
            }
            printf("done.\r\n");
            start_sweep = false;
        }
    }
  
}

void SystemClock_Config(void) {

    RCC_ClkInitTypeDef RCC_ClkInitStruct;
    RCC_OscInitTypeDef RCC_OscInitStruct;

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

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


