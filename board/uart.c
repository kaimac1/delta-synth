#include "board.h"
#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_usart.h"
#include "stm32f4xx_ll_usart.h"
#include "midi.h"

UART_HandleTypeDef h_uart_debug;
UART_HandleTypeDef h_uart_midi;

void HAL_UART_MspInit(UART_HandleTypeDef *huart) {  

    GPIO_InitTypeDef  GPIO_InitStruct;

    __HAL_RCC_GPIOA_CLK_ENABLE();

    if (huart == &h_uart_debug) {
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

        HAL_NVIC_SetPriority(USART2_IRQn, 3, 0);
        HAL_NVIC_EnableIRQ(USART2_IRQn);

    } else if (huart == &h_uart_midi) {
        __HAL_RCC_USART1_CLK_ENABLE();

        // TX
        GPIO_InitStruct.Pin       = MIDI_TX_PIN;
        GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull      = GPIO_NOPULL;
        GPIO_InitStruct.Speed     = GPIO_SPEED_FAST;
        GPIO_InitStruct.Alternate = MIDI_TX_AF;
        HAL_GPIO_Init(MIDI_PORT, &GPIO_InitStruct);
        // RX
        GPIO_InitStruct.Pin = MIDI_RX_PIN;
        GPIO_InitStruct.Alternate = MIDI_RX_AF;
        HAL_GPIO_Init(MIDI_PORT, &GPIO_InitStruct);

        HAL_NVIC_SetPriority(USART1_IRQn, 1, 0);
        HAL_NVIC_EnableIRQ(USART1_IRQn);        
    }
}

void uart_init(void) {

    // debug
    h_uart_debug.Instance          = USART2;
    h_uart_debug.Init.BaudRate     = 115200;
    h_uart_debug.Init.WordLength   = UART_WORDLENGTH_8B;
    h_uart_debug.Init.StopBits     = UART_STOPBITS_1;
    h_uart_debug.Init.Parity       = UART_PARITY_NONE;
    h_uart_debug.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    h_uart_debug.Init.Mode         = UART_MODE_TX_RX;
    h_uart_debug.Init.OverSampling = UART_OVERSAMPLING_16;
    
    HAL_UART_MspInit(&h_uart_debug);
    HAL_UART_Init(&h_uart_debug);

    // midi
    h_uart_midi.Instance          = USART1;
    h_uart_midi.Init.BaudRate     = 31250;
    h_uart_midi.Init.WordLength   = UART_WORDLENGTH_8B;
    h_uart_midi.Init.StopBits     = UART_STOPBITS_1;
    h_uart_midi.Init.Parity       = UART_PARITY_NONE;
    h_uart_midi.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    h_uart_midi.Init.Mode         = UART_MODE_TX_RX;
    h_uart_midi.Init.OverSampling = UART_OVERSAMPLING_16;
    
    HAL_UART_MspInit(&h_uart_midi);
    HAL_UART_Init(&h_uart_midi);
    LL_USART_EnableIT_RXNE(USART1);

}

void USART2_IRQHandler(void) {
    HAL_UART_IRQHandler(&h_uart_debug);
}

void uart_send(uint8_t *ptr, int len) {
    for (int i=0; i<len; i++) {
        while (!LL_USART_IsActiveFlag_TXE(USART2));
        LL_USART_TransmitData8(USART2, *ptr++);
    }
}

// retarget newlib
int _write(int file, char * ptr, int len) {
    uart_send((uint8_t*)ptr, len);
    return len;
}


// MIDI in
void USART1_IRQHandler(void) {
    if (LL_USART_IsActiveFlag_RXNE(USART1)) {
        LL_USART_ClearFlag_RXNE(USART1);
        uint8_t byte = LL_USART_ReceiveData8(USART1);
        midi_process_byte(byte);
    }
}


