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
#define NUM_BUTTONS 17
typedef enum {
    BTN_OSC_SEL = 0,
    BTN_OSC_WAVE,
    BTN_OSC_TUNE,
    BTN_ENV_SEL,
    BTN_ENV_MENU,
    BTN_EDIT = 15,
    BTN_ENCODER = 16
} ButtonName;

typedef enum {
    BTN_OFF,
    BTN_DOWN,
    BTN_HELD,
    BTN_UP
} ButtonState;
extern ButtonState buttons[NUM_BUTTONS];

typedef struct {
    int value;
    int last_value;
    int delta;
    int half_delta;
} EncoderState;
extern EncoderState encoder;

#define NUM_POTS 12
extern uint16_t pots[NUM_POTS];

void input_init(void);
bool read_buttons(void);
bool read_encoder(void);

// display
void display_init(void);
bool display_draw(void);
void draw_pixel(uint16_t x, uint16_t y, bool col);
void draw_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, bool fillcolor);
void draw_box(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
void draw_line(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, bool col);
void draw_text(uint16_t x, uint16_t y, char* text, bool inv);
void draw_text_rj(uint16_t x, uint16_t y, char* text, bool inv);
void draw_text_cen(uint16_t x, uint16_t y, char* text, bool inv);
void draw_image(uint16_t x, uint16_t y, uint8_t *array, uint16_t w, uint16_t h, bool invert);
void build_font_index(void);
extern volatile bool display_busy;

// uart
void uart_init(void);

