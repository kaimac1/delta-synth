#pragma once
#include "main.h"
#include "stm32f401_discovery.h"
#include "stm32f4xx_ll_gpio.h"

extern UART_HandleTypeDef h_uart_debug;
extern UART_HandleTypeDef h_uart_midi;

// gpio
void pin_cfg_output(GPIO_TypeDef *port, uint32_t pin);
void pin_cfg_input(GPIO_TypeDef *port, uint32_t pin, uint32_t pull);
void pin_cfg_exti(GPIO_TypeDef *port, uint32_t pin, uint32_t pull, uint32_t edge);
void pin_cfg_af(GPIO_TypeDef *port, uint32_t pin, uint32_t af);
void pin_set(GPIO_TypeDef *port, uint32_t pin, bool state);
bool pin_read(GPIO_TypeDef *port, uint32_t pin);


// input
typedef enum {
	BUTTON_ENVELOPE,
	BUTTON_FILTER,
	BUTTON_OSC,
	NUM_BUTTONS
} ButtonName;

typedef enum {
    BTN_OFF,
    BTN_PRESSED,
    BTN_HELD,
    BTN_RELEASED
} ButtonState;
extern ButtonState buttons[NUM_BUTTONS];

typedef enum {
	ENC_GREEN,
	ENC_RED,
	ENC_BLUE,
	ENC_WHITE,
	NUM_ENCODERS
} EncoderName;

typedef struct {
    int value;
    int last_value;
    int delta;
} EncoderState;
extern EncoderState encoders[NUM_ENCODERS];

void input_init(void);
bool read_buttons(void);
bool read_encoders(void);


// display
void display_init(void);
void display_write(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t *buffer);
void draw_pixel(uint16_t x, uint16_t y, uint16_t col);
void draw_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t fillcolor);
void draw_line(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t col);
void draw_text(uint16_t x, uint16_t y, char* text, int size, uint16_t colour);
void draw_text_rj(uint16_t x, uint16_t y, char* text, int size, uint16_t colour);
void display_draw(void);
void build_font_index(void);

// uart
void uart_init(void);

#define MIDI_PORT	GPIOA
#define MIDI_TX_PIN	GPIO_PIN_9
#define MIDI_RX_PIN GPIO_PIN_10
#define MIDI_TX_AF	GPIO_AF7_USART1
#define MIDI_RX_AF	GPIO_AF7_USART1