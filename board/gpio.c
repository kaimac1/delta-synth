#include "board.h"

// Configure pin as output
void pin_cfg_output(GPIO_TypeDef *port, uint32_t pin) {

    LL_GPIO_InitTypeDef gpio;
    gpio.Mode       = LL_GPIO_MODE_OUTPUT;
    gpio.Speed      = LL_GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
    gpio.Alternate  = 0;
    gpio.Pull       = 0;
    gpio.Pin        = pin;
    LL_GPIO_Init(port, &gpio);

}

// Configure pin as input
void pin_cfg_input(GPIO_TypeDef *port, uint32_t pin, uint32_t pull) {

    LL_GPIO_InitTypeDef gpio;
    gpio.Mode       = LL_GPIO_MODE_INPUT;
    gpio.Speed      = LL_GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
    gpio.Alternate  = 0;
    gpio.Pull       = pull;
    gpio.Pin        = pin;
    LL_GPIO_Init(port, &gpio);

}

// Configure pin as alternate function
void pin_cfg_af(GPIO_TypeDef *port, uint32_t pin, uint32_t af) {

    LL_GPIO_InitTypeDef gpio;
    gpio.Mode       = LL_GPIO_MODE_ALTERNATE;
    gpio.Speed      = LL_GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
    gpio.Alternate  = af;
    gpio.Pull       = 0;
    gpio.Pin        = pin;
    LL_GPIO_Init(port, &gpio);

}

// Set state of output pin
void pin_set(GPIO_TypeDef *port, uint32_t pin, bool state) {

    if (state) {
        LL_GPIO_SetOutputPin(port, pin);
    } else {
        LL_GPIO_ResetOutputPin(port, pin);
    }

}

// Read state of pin
bool pin_read(GPIO_TypeDef *port, uint32_t pin) {
    return LL_GPIO_IsInputPinSet(port, pin);

}