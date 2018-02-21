#include "board.h"
#include "stm32f4xx_ll_tim.h"

// Microsecond timer

void timer_init(void) {

    __HAL_RCC_TIM2_CLK_ENABLE();

    LL_TIM_InitTypeDef tim;
    tim.Prescaler = (SystemCoreClock / 2000000) - 1;
    tim.CounterMode = LL_TIM_COUNTERMODE_UP;
    tim.Autoreload = UINT32_MAX;
    tim.ClockDivision = 0;
    tim.RepetitionCounter = 0;
    LL_TIM_Init(TIM2, &tim);
    LL_TIM_EnableCounter(TIM2);

}


void delay_us(uint32_t microseconds) {

    // TODO: handle timer overflow
    uint32_t start = NOW_US();
    while (NOW_US() < start+microseconds);

}

