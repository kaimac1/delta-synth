#include "board.h"
#include "stm32f4xx_hal.h"

int enc_states[16] = {0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};
uint8_t enc_history;

int enc_value;

void input_init(void) {

    GPIO_InitTypeDef gpio;

    // encoders
    __HAL_RCC_GPIOD_CLK_ENABLE();

    gpio.Pin = GPIO_PIN_1;
    gpio.Pull = GPIO_PULLUP;
    gpio.Mode = GPIO_MODE_IT_RISING_FALLING; 
    HAL_GPIO_Init(GPIOD, &gpio);

    gpio.Pin = GPIO_PIN_2;
    HAL_GPIO_Init(GPIOD, &gpio);
    
    HAL_NVIC_SetPriority(EXTI1_IRQn, 0x0F, 0x00);
    HAL_NVIC_EnableIRQ(EXTI1_IRQn);
    HAL_NVIC_SetPriority(EXTI2_IRQn, 0x0F, 0x00);
    HAL_NVIC_EnableIRQ(EXTI2_IRQn);

    enc_history = HAL_GPIO_ReadPin(GPIOD, GPIO_PIN_1);
    enc_history <<= 1;
    enc_history |= HAL_GPIO_ReadPin(GPIOD, GPIO_PIN_2);

    // button
    __HAL_RCC_GPIOA_CLK_ENABLE();    

    gpio.Pin  = GPIO_PIN_0;
    gpio.Pull = GPIO_NOPULL;
    gpio.Mode = GPIO_MODE_IT_RISING; 
    HAL_GPIO_Init(GPIOA, &gpio);
    HAL_NVIC_SetPriority(EXTI0_IRQn, 0x0F, 0x00);
    HAL_NVIC_EnableIRQ(EXTI0_IRQn);

}

void EXTI0_IRQHandler(void) {
  HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_0);
}

void EXTI1_IRQHandler(void) {
    HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_1);
}

void EXTI2_IRQHandler(void) {
    HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_2);
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {

    if ((GPIO_Pin == GPIO_PIN_1) || (GPIO_Pin == GPIO_PIN_2)) {

        int inca = HAL_GPIO_ReadPin(GPIOD, GPIO_PIN_1);
        int incb = HAL_GPIO_ReadPin(GPIOD, GPIO_PIN_2);

        enc_history <<= 2;
        enc_history |= ((inca << 1) | incb);

        enc_value += enc_states[enc_history & 0x0F];
        if (enc_value < 0) enc_value = 0;
        if (enc_value > 100) enc_value = 100;

        printf("irq %d\r\n", enc_value);
    }

    else if (GPIO_PIN_0 == GPIO_Pin) {
        //while (BSP_P != RESET);
        printf("button\r\n");
    }


}

// uint32_t BSP_PB_GetState(Button_TypeDef Button)
// {
//   return HAL_GPIO_ReadPin(BUTTON_PORT[Button], BUTTON_PIN[Button]);
// }