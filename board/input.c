#include "board.h"
#include "stm32f4xx_hal.h"
#include "stm32f4xx_ll_system.h"
#include "stm32f4xx_ll_exti.h"

int enc_states[16] = {0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};
uint8_t enc_history[NUM_ENCODERS];

GPIO_TypeDef* button_ports[NUM_BUTTONS];
uint32_t      button_pins[NUM_BUTTONS];
GPIO_TypeDef* encoder_ports[NUM_ENCODERS];
uint32_t      encoder_pin_a[NUM_ENCODERS];
uint32_t      encoder_pin_b[NUM_ENCODERS];

ButtonState buttons[NUM_BUTTONS];
EncoderState encoders[NUM_ENCODERS];


// Initialise input hardware.
void input_init(void) {

    // buttons
    button_ports[BUTTON_ENVELOPE] = GPIOC;
    button_pins[BUTTON_ENVELOPE]  = LL_GPIO_PIN_0;
    button_ports[BUTTON_FILTER]   = GPIOC;
    button_pins[BUTTON_FILTER]    = LL_GPIO_PIN_1;
    button_ports[BUTTON_OSC]      = GPIOC;
    button_pins[BUTTON_OSC]       = LL_GPIO_PIN_2;

    for (int i=0; i<NUM_BUTTONS; i++) {
        pin_cfg_input(button_ports[i], button_pins[i], LL_GPIO_PULL_DOWN);
    }

    // encoders
    encoder_ports[0] = GPIOB;
    encoder_pin_a[0] = LL_GPIO_PIN_5;
    encoder_pin_b[0] = LL_GPIO_PIN_14;
    encoder_ports[1] = GPIOA;
    encoder_pin_a[1] = LL_GPIO_PIN_11;
    encoder_pin_b[1] = LL_GPIO_PIN_12;
    encoder_ports[2] = GPIOB;
    encoder_pin_a[2] = LL_GPIO_PIN_8;
    encoder_pin_b[2] = LL_GPIO_PIN_9;
    encoder_ports[3] = GPIOB;
    encoder_pin_a[3] = LL_GPIO_PIN_6;
    encoder_pin_b[3] = LL_GPIO_PIN_7;

    for (int i=0; i<NUM_ENCODERS; i++) {
        pin_cfg_exti(encoder_ports[i], encoder_pin_a[i],  LL_GPIO_PULL_UP, LL_EXTI_TRIGGER_RISING_FALLING);
        pin_cfg_exti(encoder_ports[i], encoder_pin_b[i],  LL_GPIO_PULL_UP, LL_EXTI_TRIGGER_RISING_FALLING);
        enc_history[i] = (pin_read(encoder_ports[i], encoder_pin_b[i]) << 1) | pin_read(encoder_ports[i], encoder_pin_a[i]);
    }

    LL_SYSCFG_SetEXTISource(LL_SYSCFG_EXTI_PORTB, LL_SYSCFG_EXTI_LINE5);
    LL_SYSCFG_SetEXTISource(LL_SYSCFG_EXTI_PORTB, LL_SYSCFG_EXTI_LINE14);
    LL_SYSCFG_SetEXTISource(LL_SYSCFG_EXTI_PORTA, LL_SYSCFG_EXTI_LINE11);
    LL_SYSCFG_SetEXTISource(LL_SYSCFG_EXTI_PORTA, LL_SYSCFG_EXTI_LINE12);
    LL_SYSCFG_SetEXTISource(LL_SYSCFG_EXTI_PORTB, LL_SYSCFG_EXTI_LINE8);
    LL_SYSCFG_SetEXTISource(LL_SYSCFG_EXTI_PORTB, LL_SYSCFG_EXTI_LINE9);
    LL_SYSCFG_SetEXTISource(LL_SYSCFG_EXTI_PORTB, LL_SYSCFG_EXTI_LINE6);
    LL_SYSCFG_SetEXTISource(LL_SYSCFG_EXTI_PORTB, LL_SYSCFG_EXTI_LINE7);
    HAL_NVIC_SetPriority(EXTI9_5_IRQn,   PRIORITY_ENCODER, 0);
    HAL_NVIC_SetPriority(EXTI15_10_IRQn, PRIORITY_ENCODER, 0);
    HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);
    HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

}


// Read each button and update its state.
// Returns true if any buttons have changed state.
bool read_buttons(void) {

    bool changed = false;

    for (int i=0; i<NUM_BUTTONS; i++) {
        bool pressed = pin_read(button_ports[i], button_pins[i]);

        switch (buttons[i]) {
            case BTN_OFF:
                if (pressed) {
                    buttons[i] = BTN_PRESSED;
                    changed = true;
                }
                break;
            case BTN_PRESSED:
                buttons[i] = pressed ? BTN_HELD : BTN_RELEASED;
                changed = true;
                break;
            case BTN_HELD:
                if (!pressed) {
                    buttons[i] = BTN_RELEASED;
                    changed = true;
                }
                break;
            case BTN_RELEASED:
                buttons[i] = BTN_OFF;
                changed = true;
                break;
        }
    }

    return changed;

}


// Update the delta for each encoder.
// Returns true if any encoders have moved.
bool read_encoders(void) {

    bool changed = false;

    for (int i=0; i<NUM_ENCODERS; i++) {
        encoders[i].delta = encoders[i].value - encoders[i].last_value;
        if (encoders[i].delta != 0) changed = true;
        encoders[i].last_value = encoders[i].value;
    }

    return changed;

}

void encoder_irq(void) {

    uint32_t port;

    for (int i=0; i<NUM_ENCODERS; i++) {
        port = encoder_ports[i]->IDR;
        int inca = !!(port & encoder_pin_a[i]);
        int incb = !!(port & encoder_pin_b[i]);

        enc_history[i] <<= 2;
        enc_history[i] |= ((incb << 1) | inca);
        encoders[i].value += enc_states[enc_history[i] & 0x0F];
    }

}

void EXTI9_5_IRQHandler(void) {
    LL_EXTI_ClearFlag_0_31(LL_EXTI_LINE_ALL);
    encoder_irq();
}

void EXTI15_10_IRQHandler(void) {
    LL_EXTI_ClearFlag_0_31(LL_EXTI_LINE_ALL);
    encoder_irq();
}

