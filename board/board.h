#pragma once
#include "main.h"
#include "stm32f4xx_hal.h"
#include "stm32f4xx_ll_gpio.h"
#include "stm32f4xx_ll_tim.h"

extern UART_HandleTypeDef h_uart_debug;
extern UART_HandleTypeDef h_uart_midi;

// IRQ priorities
#define PRIORITY_MIDI       0
#define PRIORITY_ENCODER    1
#define PRIORITY_AUDIO      2
#define PRIORITY_DISPLAY    3
#define PRIORITY_DEBUG      3


// timer
#define NOW_US() LL_TIM_GetCounter(TIM2)
void timer_init();
void delay_us(uint32_t microseconds);

// gpio
void pin_cfg_output(GPIO_TypeDef *port, uint32_t pin);
void pin_cfg_input(GPIO_TypeDef *port, uint32_t pin, uint32_t pull);
void pin_cfg_exti(GPIO_TypeDef *port, uint32_t pin, uint32_t pull, uint32_t edge);
void pin_cfg_af(GPIO_TypeDef *port, uint32_t pin, uint32_t af);
void pin_cfg_i2c(GPIO_TypeDef *port, uint32_t pin);
void pin_cfg_an(GPIO_TypeDef *port, uint32_t pin);
void pin_set(GPIO_TypeDef *port, uint32_t pin, bool state);
bool pin_read(GPIO_TypeDef *port, uint32_t pin);

// input
typedef enum {
    BUTTON_OSC,
    BUTTON_FILTER,
    BUTTON_ENVELOPE,
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
bool display_draw(void);
void draw_pixel(uint16_t x, uint16_t y, bool col);
void draw_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, bool fillcolor);
void draw_line(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, bool col);
void draw_text(uint16_t x, uint16_t y, char* text, int size);
void draw_text_rj(uint16_t x, uint16_t y, char* text, int size);
void draw_text_cen(uint16_t x, uint16_t y, char* text, int size);
void build_font_index(void);
extern volatile bool display_busy;

// uart
void uart_init(void);

