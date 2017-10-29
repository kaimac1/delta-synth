#pragma once
#include "main.h"

extern UART_HandleTypeDef h_uart_debug;
extern UART_HandleTypeDef h_uart_midi;



void uart_init(void);
void input_init(void);

extern int enc_value;


#define MIDI_PORT	GPIOA
#define MIDI_TX_PIN	GPIO_PIN_9
#define MIDI_RX_PIN GPIO_PIN_10
#define MIDI_TX_AF	GPIO_AF7_USART1
#define MIDI_RX_AF	GPIO_AF7_USART1