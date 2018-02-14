#pragma once
#include "stm32f4xx_hal.h"


#define RCC_PERIPHCLK_I2S RCC_PERIPHCLK_I2S_APB1

/* I2S peripheral configuration defines */
#define I2S3                            SPI3
#define I2S3_CLK_ENABLE()               __HAL_RCC_SPI3_CLK_ENABLE()
#define I2S3_CLK_DISABLE()              __HAL_RCC_SPI3_CLK_DISABLE()
#define I2S3_SCK_SD_WS_AF               GPIO_AF6_SPI3
#define I2S3_SCK_SD_CLK_ENABLE()        __HAL_RCC_GPIOC_CLK_ENABLE()
#define I2S3_WS_CLK_ENABLE()            __HAL_RCC_GPIOA_CLK_ENABLE()
#define I2S3_WS_PIN                     GPIO_PIN_4
#define I2S3_SCK_PIN                    GPIO_PIN_10
#define I2S3_SD_PIN                     GPIO_PIN_12
#define I2S3_SCK_SD_GPIO_PORT           GPIOC
#define I2S3_WS_GPIO_PORT               GPIOA

/* I2S DMA Stream definitions */
#define I2S3_DMAx_CLK_ENABLE()          __HAL_RCC_DMA1_CLK_ENABLE()
#define I2S3_DMAx_CLK_DISABLE()         __HAL_RCC_DMA1_CLK_DISABLE()
#define I2S3_DMAx_STREAM                DMA1_Stream7
#define I2S3_DMAx_CHANNEL               DMA_CHANNEL_0
#define I2S3_DMAx_IRQ                   DMA1_Stream7_IRQn
#define I2S3_DMAx_PERIPH_DATA_SIZE      DMA_PDATAALIGN_HALFWORD
#define I2S3_DMAx_MEM_DATA_SIZE         DMA_MDATAALIGN_HALFWORD
#define DMA_MAX_SZE                     0xFFFF

#define I2S3_IRQHandler                 DMA1_Stream7_IRQHandler

/* Select the interrupt preemption priority and subpriority for the DMA interrupt */
#define AUDIO_OUT_IRQ_PREPRIO           0x03   /* Select the preemption priority level(0 is the highest) */
#define AUDIODATA_SIZE                  2   /* 16-bits audio data size */

/* Audio status definition */     
#define AUDIO_OK                        0
#define AUDIO_ERROR                     1
#define AUDIO_TIMEOUT                   2

/* AudioFreq * DataSize (2 bytes) * NumChannels (Stereo: 2) */
#define DEFAULT_AUDIO_IN_FREQ                 I2S_AUDIOFREQ_16K
#define DEFAULT_AUDIO_IN_BIT_RESOLUTION       16
#define DEFAULT_AUDIO_IN_CHANNEL_NBR          1 /* Mono = 1, Stereo = 2 */
#define DEFAULT_AUDIO_IN_VOLUME               64

/* PDM buffer input size */
#define INTERNAL_BUFF_SIZE                    128*DEFAULT_AUDIO_IN_FREQ/16000*DEFAULT_AUDIO_IN_CHANNEL_NBR
/* PCM buffer output size */
#define PCM_OUT_SIZE                          DEFAULT_AUDIO_IN_FREQ/1000
#define CHANNEL_DEMUX_MASK                    0x55
   
#define DMA_MAX(_X_)                (((_X_) <= DMA_MAX_SZE)? (_X_):DMA_MAX_SZE)


uint8_t BSP_AUDIO_OUT_Init(uint32_t AudioFreq);
uint8_t BSP_AUDIO_OUT_Play(uint16_t* pBuffer, uint32_t Size);
void    BSP_AUDIO_OUT_ChangeBuffer(uint16_t *pData, uint16_t Size);
uint8_t BSP_AUDIO_OUT_Pause(void);
uint8_t BSP_AUDIO_OUT_Resume(void);
uint8_t BSP_AUDIO_OUT_Stop(uint32_t Option);
void    BSP_AUDIO_OUT_SetFrequency(uint32_t AudioFreq);

void    BSP_AUDIO_OUT_TransferComplete_CallBack(void);
void    BSP_AUDIO_OUT_HalfTransfer_CallBack(void);
void    BSP_AUDIO_OUT_Error_CallBack(void);

void  BSP_AUDIO_OUT_ClockConfig(I2S_HandleTypeDef *hi2s, uint32_t AudioFreq, void *Params);
void  BSP_AUDIO_OUT_MspInit(I2S_HandleTypeDef *hi2s, void *Params);
void  BSP_AUDIO_OUT_MspDeInit(I2S_HandleTypeDef *hi2s, void *Params);
