#include "board.h"
#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_usart.h"
#include "stm32f4xx_ll_usart.h"
#include "midi.h"

UART_HandleTypeDef h_uart_debug;
UART_HandleTypeDef h_uart_midi;

#define DEBUG_PORT   GPIOA
#define DEBUG_TX_PIN LL_GPIO_PIN_9
#define DEBUG_RX_PIN LL_GPIO_PIN_10
#define DEBUG_AF 7

#define MIDI_PORT   GPIOC
#define MIDI_TX_PIN LL_GPIO_PIN_6
#define MIDI_RX_PIN LL_GPIO_PIN_7
#define MIDI_AF 8

void HAL_UART_MspInit(UART_HandleTypeDef *huart) {  

    if (huart == &h_uart_debug) {
        __HAL_RCC_USART1_CLK_ENABLE();

        pin_cfg_af(DEBUG_PORT, DEBUG_TX_PIN, DEBUG_AF);
        pin_cfg_af(DEBUG_PORT, DEBUG_RX_PIN, DEBUG_AF);
        HAL_NVIC_SetPriority(USART1_IRQn, 3, 0);
        HAL_NVIC_EnableIRQ(USART1_IRQn);

    } else if (huart == &h_uart_midi) {
        __HAL_RCC_USART6_CLK_ENABLE();

        pin_cfg_af(MIDI_PORT, MIDI_TX_PIN, MIDI_AF);
        pin_cfg_af(MIDI_PORT, MIDI_RX_PIN, MIDI_AF);        
        HAL_NVIC_SetPriority(USART6_IRQn, 0, 0);
        HAL_NVIC_EnableIRQ(USART6_IRQn);        
    }
}

void uart_init(void) {

    // debug
    h_uart_debug.Instance          = USART1;
    h_uart_debug.Init.BaudRate     = 1000000;
    h_uart_debug.Init.WordLength   = UART_WORDLENGTH_8B;
    h_uart_debug.Init.StopBits     = UART_STOPBITS_1;
    h_uart_debug.Init.Parity       = UART_PARITY_NONE;
    h_uart_debug.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    h_uart_debug.Init.Mode         = UART_MODE_TX_RX;
    h_uart_debug.Init.OverSampling = UART_OVERSAMPLING_16;
    
    HAL_UART_MspInit(&h_uart_debug);
    HAL_UART_Init(&h_uart_debug);

    // midi
    h_uart_midi.Instance          = USART6;
    h_uart_midi.Init.BaudRate     = 31250;
    h_uart_midi.Init.WordLength   = UART_WORDLENGTH_8B;
    h_uart_midi.Init.StopBits     = UART_STOPBITS_1;
    h_uart_midi.Init.Parity       = UART_PARITY_NONE;
    h_uart_midi.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    h_uart_midi.Init.Mode         = UART_MODE_TX_RX;
    h_uart_midi.Init.OverSampling = UART_OVERSAMPLING_16;
    
    HAL_UART_MspInit(&h_uart_midi);
    HAL_UART_Init(&h_uart_midi);
    LL_USART_EnableIT_RXNE(USART6);

}

void USART1_IRQHandler(void) {
    HAL_UART_IRQHandler(&h_uart_debug);
}

void uart_send(uint8_t *ptr, int len) {
    for (int i=0; i<len; i++) {
        while (!LL_USART_IsActiveFlag_TXE(USART1));
        LL_USART_TransmitData8(USART1, *ptr++);
    }
}

// retarget newlib
int _write(int file, char * ptr, int len) {
    uart_send((uint8_t*)ptr, len);
    return len;
}


// MIDI in
void USART6_IRQHandler(void) {
    if (LL_USART_IsActiveFlag_RXNE(USART6)) {
        LL_USART_ClearFlag_RXNE(USART6);
        uint8_t byte = LL_USART_ReceiveData8(USART6);
        midi_process_byte(byte);
    }
}


