#include "board.h"
#include "stm32f4xx_hal.h"
#include "stm32f4xx_ll_system.h"
#include "stm32f4xx_ll_exti.h"

int enc_states[16] = {0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};

GPIO_TypeDef* button_ports[NUM_BUTTONS];
uint32_t      button_pins[NUM_BUTTONS];
GPIO_TypeDef* encoder_ports[NUM_ENCODERS];
uint32_t      encoder_pin_a[NUM_ENCODERS];
uint32_t      encoder_pin_b[NUM_ENCODERS];

ButtonState buttons[NUM_BUTTONS];
Encoder encoders[NUM_ENCODERS];

void read_encoders(void);

void input_init(void) {

    // buttons
    button_ports[BUTTON_ENVELOPE] = GPIOA;
    button_pins[BUTTON_ENVELOPE]  = LL_GPIO_PIN_0;
    button_ports[BUTTON_FILTER] = GPIOB;
    button_pins[BUTTON_FILTER]  = LL_GPIO_PIN_0;

    for (int i=0; i<NUM_BUTTONS; i++) {
        pin_cfg_input(button_ports[i], button_pins[i], LL_GPIO_PULL_NO);
    }

    // encoders
    encoder_ports[0] = GPIOE;
    encoder_pin_a[0]  = LL_GPIO_PIN_7;
    encoder_pin_b[0]  = LL_GPIO_PIN_8;
    encoder_ports[1] = GPIOE;
    encoder_pin_a[1]  = LL_GPIO_PIN_9;
    encoder_pin_b[1]  = LL_GPIO_PIN_10;
    encoder_ports[2] = GPIOE;
    encoder_pin_a[2]  = LL_GPIO_PIN_11;
    encoder_pin_b[2]  = LL_GPIO_PIN_12;
    encoder_ports[3] = GPIOE;
    encoder_pin_a[3]  = LL_GPIO_PIN_13;
    encoder_pin_b[3]  = LL_GPIO_PIN_14;

    for (int i=0; i<NUM_ENCODERS; i++) {
        pin_cfg_exti(encoder_ports[i], encoder_pin_a[i],  LL_GPIO_PULL_UP, LL_EXTI_TRIGGER_RISING_FALLING);
        pin_cfg_exti(encoder_ports[i], encoder_pin_b[i],  LL_GPIO_PULL_UP, LL_EXTI_TRIGGER_RISING_FALLING);
        encoders[i].history = (pin_read(encoder_ports[i], encoder_pin_b[i]) << 1) | pin_read(encoder_ports[i], encoder_pin_a[i]);
    }

    LL_SYSCFG_SetEXTISource(LL_SYSCFG_EXTI_PORTE, LL_SYSCFG_EXTI_LINE7);
    LL_SYSCFG_SetEXTISource(LL_SYSCFG_EXTI_PORTE, LL_SYSCFG_EXTI_LINE8);
    LL_SYSCFG_SetEXTISource(LL_SYSCFG_EXTI_PORTE, LL_SYSCFG_EXTI_LINE9);
    LL_SYSCFG_SetEXTISource(LL_SYSCFG_EXTI_PORTE, LL_SYSCFG_EXTI_LINE10);
    LL_SYSCFG_SetEXTISource(LL_SYSCFG_EXTI_PORTE, LL_SYSCFG_EXTI_LINE11);
    LL_SYSCFG_SetEXTISource(LL_SYSCFG_EXTI_PORTE, LL_SYSCFG_EXTI_LINE12);
    LL_SYSCFG_SetEXTISource(LL_SYSCFG_EXTI_PORTE, LL_SYSCFG_EXTI_LINE13);
    LL_SYSCFG_SetEXTISource(LL_SYSCFG_EXTI_PORTE, LL_SYSCFG_EXTI_LINE14);
    HAL_NVIC_SetPriority(EXTI9_5_IRQn, 0x0F, 0x00);
    HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);
    HAL_NVIC_SetPriority(EXTI15_10_IRQn, 0x0F, 0x00);
    HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

}

void read_buttons(void) {

    for (int i=0; i<NUM_BUTTONS; i++) {

        bool pressed = pin_read(button_ports[i], button_pins[i]);

        switch (buttons[i]) {
            case BTN_OFF:
                if (pressed) buttons[i] = BTN_PRESSED;
                break;
            case BTN_PRESSED:
                buttons[i] = pressed ? BTN_HELD : BTN_RELEASED;
                break;
            case BTN_HELD:
                if (!pressed) buttons[i] = BTN_RELEASED;
                break;
            case BTN_RELEASED:
                buttons[i] = BTN_OFF;
                break;
        }
    }

}



void EXTI9_5_IRQHandler(void) {

    LL_EXTI_ClearFlag_0_31(LL_EXTI_LINE_ALL);
    read_encoders();

}

void EXTI15_10_IRQHandler(void) {

    LL_EXTI_ClearFlag_0_31(LL_EXTI_LINE_ALL);
    read_encoders();

}

void read_encoders(void) {

    for (int i=0; i<NUM_ENCODERS; i++) {

        int inca = pin_read(encoder_ports[i], encoder_pin_a[i]);
        int incb = pin_read(encoder_ports[i], encoder_pin_b[i]);

        encoders[i].history <<= 2;
        encoders[i].history |= ((incb << 1) | inca);
        encoders[i].value += enc_states[encoders[i].history & 0x0F];

    }
}
