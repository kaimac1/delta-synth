#include "main.h"

__IO uint8_t UserPressButton = 0;

static void SystemClock_Config(void);

// uart

UART_HandleTypeDef h_uart;

void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{  
  GPIO_InitTypeDef  GPIO_InitStruct;
  
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_USART2_CLK_ENABLE();
  
  GPIO_InitStruct.Pin       = GPIO_PIN_2;
  GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull      = GPIO_NOPULL;
  GPIO_InitStruct.Speed     = GPIO_SPEED_FAST;
  GPIO_InitStruct.Alternate = GPIO_AF7_USART2;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    
  // /* UART RX GPIO pin configuration  */
  // GPIO_InitStruct.Pin = USARTx_RX_PIN;
  // GPIO_InitStruct.Alternate = USARTx_RX_AF;
    
  // HAL_GPIO_Init(USARTx_RX_GPIO_PORT, &GPIO_InitStruct);
    
  //HAL_NVIC_SetPriority(USART1_IRQn, 0, 1);
  //HAL_NVIC_EnableIRQ(USART1_IRQn);
}


void uart_init(void) {

    h_uart.Instance          = USART2;
    h_uart.Init.BaudRate     = 9600;
    h_uart.Init.WordLength   = UART_WORDLENGTH_8B;
    h_uart.Init.StopBits     = UART_STOPBITS_1;
    h_uart.Init.Parity       = UART_PARITY_NONE;
    h_uart.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    h_uart.Init.Mode         = UART_MODE_TX_RX;
    h_uart.Init.OverSampling = UART_OVERSAMPLING_16;
    
    HAL_UART_MspInit(&h_uart);
    if(HAL_UART_Init(&h_uart) != HAL_OK) {
        Error_Handler();
    }

}

// retarget newlib

int _write(int file, char * ptr, int len) {
    HAL_UART_Transmit(&h_uart, (uint8_t*)ptr, len, 1000);
    return len;
}


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

    while (1) {
        AudioPlay_Test();
    }
  
}

static void SystemClock_Config(void) {

    RCC_ClkInitTypeDef RCC_ClkInitStruct;
    RCC_OscInitTypeDef RCC_OscInitStruct;

    __HAL_RCC_PWR_CLK_ENABLE();

    /* The voltage scaling allows optimizing the power consumption when the device is 
     clocked below the maximum system frequency, to update the voltage scaling value 
     regarding system frequency refer to product datasheet.  */
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

    /* Select PLL as system clock source and configure the HCLK, PCLK1 and PCLK2 
     clocks dividers */
    RCC_ClkInitStruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2);
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;  
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;  
    HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2);
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (KEY_BUTTON_PIN == GPIO_Pin) {
        while (BSP_PB_GetState(BUTTON_KEY) != RESET);
        UserPressButton = 1;
    }
}

void Toggle_Leds(void) {
    BSP_LED_Toggle(LED3);
    HAL_Delay(100);
    BSP_LED_Toggle(LED4);
    HAL_Delay(100);
    BSP_LED_Toggle(LED5);
    HAL_Delay(100);
    BSP_LED_Toggle(LED6);
    HAL_Delay(100);
}

void Error_Handler(void) {
    BSP_LED_On(LED5);
    while(1);
}

