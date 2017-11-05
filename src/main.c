#include "main.h"
#include "notes.h"

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

    AudioPlay_Test();

    char inchar;

    cfgnew.freq = note[69];
    cfgnew.key = true;
    cfgnew.osc_wave = WAVE_SQUARE;

    while (1) {
        if (HAL_UART_Receive(&h_uart_debug, (uint8_t*)&inchar, 1, 100) == HAL_OK) {
            switch (inchar) {
                case '1': ctrlcfg = CTRL_MAIN; printf("MAIN\r\n"); break;
                case '2': ctrlcfg = CTRL_ENVELOPE; printf("ENVELOPE\r\n"); break;
            }
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


