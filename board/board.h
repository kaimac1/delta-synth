#pragma once
#include "main.h"
#include "stm32f401_discovery.h"
#include "stm32f4xx_ll_gpio.h"

extern UART_HandleTypeDef h_uart_debug;
extern UART_HandleTypeDef h_uart_midi;

// gpio
void pin_cfg_output(GPIO_TypeDef *port, uint32_t pin);
void pin_cfg_input(GPIO_TypeDef *port, uint32_t pin, uint32_t pull);
void pin_cfg_af(GPIO_TypeDef *port, uint32_t pin, uint32_t af);
void pin_set(GPIO_TypeDef *port, uint32_t pin, bool state);
bool pin_read(GPIO_TypeDef *port, uint32_t pin);


// input
typedef enum {
    BTN_OFF,
    BTN_PRESSED,
    BTN_HELD,
    BTN_RELEASED
} ButtonState;
extern ButtonState buttons[1];

void input_init(void);
void read_buttons(void);


// display
void display_init(void);
void display_fillrect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t fillcolor);
void display_write(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t *buffer);
void draw_pixel(uint16_t x, uint16_t y, uint16_t col);
void draw_line(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t col);



void uart_init(void);


extern int enc_value;


#define MIDI_PORT	GPIOA
#define MIDI_TX_PIN	GPIO_PIN_9
#define MIDI_RX_PIN GPIO_PIN_10
#define MIDI_TX_AF	GPIO_AF7_USART1
#define MIDI_RX_AF	GPIO_AF7_USART1