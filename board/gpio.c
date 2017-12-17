#include "board.h"
#include "stm32f4xx_ll_exti.h"

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

// Configure pin as input + EXTI
void pin_cfg_exti(GPIO_TypeDef *port, uint32_t pin, uint32_t pull, uint32_t edge) {

    pin_cfg_input(port, pin, pull);

    LL_EXTI_InitTypeDef exti;
    exti.Line_0_31 = pin;
    exti.LineCommand = ENABLE;
    exti.Mode = LL_EXTI_MODE_IT;
    exti.Trigger = edge;
    LL_EXTI_Init(&exti);    

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