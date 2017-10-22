#include "main.h"
#include "stm32f4xx_it.h"

extern I2S_HandleTypeDef       hAudioOutI2s;
extern I2S_HandleTypeDef       hAudioInI2s;

void NMI_Handler(void) {}

void HardFault_Handler(void) {
  while (1);
}

void MemManage_Handler(void) {
  while (1);
}

void BusFault_Handler(void) {
  while (1);
}

void UsageFault_Handler(void) {
  while (1);
}

void SVC_Handler(void) {
}

void DebugMon_Handler(void) {
}

void PendSV_Handler(void) {
}

void SysTick_Handler(void) {
  HAL_IncTick();
}





void I2S3_IRQHandler(void) { 
  HAL_DMA_IRQHandler(hAudioOutI2s.hdmatx);
}

void I2S2_IRQHandler(void) {
  HAL_DMA_IRQHandler(hAudioInI2s.hdmarx);
}

