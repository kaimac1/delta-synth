#include "uart.h"
#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_usart.h"

UART_HandleTypeDef h_uart;

void HAL_UART_MspInit(UART_HandleTypeDef *huart) {  

    GPIO_InitTypeDef  GPIO_InitStruct;

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_USART2_CLK_ENABLE();

    // TX
    GPIO_InitStruct.Pin       = GPIO_PIN_2;
    GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull      = GPIO_NOPULL;
    GPIO_InitStruct.Speed     = GPIO_SPEED_FAST;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART2;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    // RX
    GPIO_InitStruct.Pin = GPIO_PIN_3;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART2;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    HAL_NVIC_SetPriority(USART2_IRQn, 0, 1);
    HAL_NVIC_EnableIRQ(USART2_IRQn);
}

void uart_init(void) {

    h_uart.Instance          = USART2;
    h_uart.Init.BaudRate     = 115200;
    h_uart.Init.WordLength   = UART_WORDLENGTH_8B;
    h_uart.Init.StopBits     = UART_STOPBITS_1;
    h_uart.Init.Parity       = UART_PARITY_NONE;
    h_uart.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    h_uart.Init.Mode         = UART_MODE_TX_RX;
    h_uart.Init.OverSampling = UART_OVERSAMPLING_16;
    
    HAL_UART_MspInit(&h_uart);
    HAL_UART_Init(&h_uart);

}

void USART2_IRQHandler(void) {
    HAL_UART_IRQHandler(&h_uart);
}

// retarget newlib

int _write(int file, char * ptr, int len) {
    HAL_UART_Transmit(&h_uart, (uint8_t*)ptr, len, 1000);
    return len;
}


