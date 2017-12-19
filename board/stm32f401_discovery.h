#pragma once                                              
#include "stm32f4xx_hal.h"
   
#if !defined (USE_STM32F401_DISCO)
 #define USE_STM32F401_DISCO
#endif

/*############################### I2Cx #######################################*/
#define DISCOVERY_I2Cx                          I2C1
#define DISCOVERY_I2Cx_CLOCK_ENABLE()           __HAL_RCC_I2C1_CLK_ENABLE()
#define DISCOVERY_I2Cx_GPIO_PORT                GPIOB                       /* GPIOB */
#define DISCOVERY_I2Cx_SCL_PIN                  GPIO_PIN_6                  /* PB.06 */
#define DISCOVERY_I2Cx_SDA_PIN                  GPIO_PIN_9                  /* PB.09 */
#define DISCOVERY_I2Cx_GPIO_CLK_ENABLE()        __HAL_RCC_GPIOB_CLK_ENABLE() 
#define DISCOVERY_I2Cx_GPIO_CLK_DISABLE()       __HAL_RCC_GPIOB_CLK_DISABLE() 
#define DISCOVERY_I2Cx_AF                       GPIO_AF4_I2C1

#define DISCOVERY_I2Cx_FORCE_RESET()            __HAL_RCC_I2C1_FORCE_RESET()
#define DISCOVERY_I2Cx_RELEASE_RESET()          __HAL_RCC_I2C1_RELEASE_RESET()

/* I2C interrupt requests */
#define DISCOVERY_I2Cx_EV_IRQn                  I2C1_EV_IRQn
#define DISCOVERY_I2Cx_ER_IRQn                  I2C1_ER_IRQn

/* I2C speed and timeout max */
#define I2Cx_TIMEOUT_MAX                        0xA000 /*<! The value of the maximal timeout for I2C waiting loops */
#define I2Cx_MAX_COMMUNICATION_FREQ             ((uint32_t) 100000)

/*################################### AUDIO ##################################*/
/**
  * @brief  AUDIO I2C Interface pins
  */
/* Device I2C address */
#define AUDIO_I2C_ADDRESS                       0x94

/* Audio codec power on/off macro definition */
#define CODEC_AUDIO_POWER_OFF()      HAL_GPIO_WritePin(AUDIO_RESET_GPIO, AUDIO_RESET_PIN, GPIO_PIN_RESET)
#define CODEC_AUDIO_POWER_ON()       HAL_GPIO_WritePin(AUDIO_RESET_GPIO, AUDIO_RESET_PIN, GPIO_PIN_SET)

/* Audio Reset Pin definition */
#define AUDIO_RESET_GPIO_CLK_ENABLE()           __HAL_RCC_GPIOD_CLK_ENABLE()
#define AUDIO_RESET_PIN                         GPIO_PIN_4
#define AUDIO_RESET_GPIO                        GPIOD


