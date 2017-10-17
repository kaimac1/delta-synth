#include "main.h"
#include "uart.h"

__IO uint8_t UserPressButton = 0;

static void SystemClock_Config(void);

extern UART_HandleTypeDef h_uart;

#define RXBUFFERSIZE 8

uint32_t note[10];


/******************************************************************************/
int main(void) { 
 
    HAL_Init();
    BSP_LED_Init(LED3);
    BSP_LED_Init(LED4); 
    BSP_LED_Init(LED5);
    BSP_LED_Init(LED6); 
    SystemClock_Config();
    BSP_PB_Init(BUTTON_KEY, BUTTON_MODE_EXTI);
    uart_init();
    printf("Running.\r\n");


    note[0] = 261.13 / SAMPLE_RATE * UINT32_MAX; // C
    note[1] = 293.66 / SAMPLE_RATE * UINT32_MAX; // D
    note[2] = 329.63 / SAMPLE_RATE * UINT32_MAX; // E
    note[3] = 349.23 / SAMPLE_RATE * UINT32_MAX; // F
    note[4] = 392.00 / SAMPLE_RATE * UINT32_MAX; // G
    note[5] = 440.00 / SAMPLE_RATE * UINT32_MAX; // A 
    note[6] = 493.88 / SAMPLE_RATE * UINT32_MAX; // B
    note[7] = 523.25 / SAMPLE_RATE * UINT32_MAX; // C


    AudioPlay_Test();

    char inchar;

    while (1) {        
        if (HAL_UART_Receive(&h_uart, (uint8_t*)&inchar, 1, 1000) == HAL_OK) {
             printf("char = %c\r\n", inchar);
             switch (inchar) {
                case 'z': freq = note[0]; break;
                case 'x': freq = note[1]; break;
                case 'c': freq = note[2]; break;
                case 'v': freq = note[3]; break;
                case 'b': freq = note[4]; break;
                case 'n': freq = note[5]; break;
                case 'm': freq = note[6]; break;
                case ',': freq = note[7]; break;
             }
        }
    }
  
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *UartHandle)
{
  BSP_LED_Toggle(LED4);
}

static void SystemClock_Config(void) {

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

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
    if (KEY_BUTTON_PIN == GPIO_Pin) {
        while (BSP_PB_GetState(BUTTON_KEY) != RESET);
        UserPressButton = 1;
    }
}


