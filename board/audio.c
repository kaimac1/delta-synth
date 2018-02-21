#include "audio.h"

/* These PLL parameters are valid when the f(VCO clock) = 1Mhz */
const uint32_t I2SFreq[8] = {8000, 11025, 16000, 22050, 32000, 44100, 48000, 96000};
const uint32_t I2SPLLN[8] = {256, 429, 213, 429, 426, 271, 258, 344};
const uint32_t I2SPLLR[8] = {5, 4, 4, 4, 4, 6, 3, 1};

I2S_HandleTypeDef i2s;

static uint8_t I2S3_Init(uint32_t AudioFreq);

void I2S3_IRQHandler(void) { 
    HAL_DMA_IRQHandler(i2s.hdmatx);
}


void audio_init(uint32_t sample_rate) {
  
    BSP_AUDIO_OUT_ClockConfig(&i2s, sample_rate, NULL);
  
    i2s.Instance = I2S3;
    if (HAL_I2S_GetState(&i2s) == HAL_I2S_STATE_RESET) {
        BSP_AUDIO_OUT_MspInit(&i2s, NULL);
    }
  
    I2S3_Init(sample_rate);
    
}

uint8_t BSP_AUDIO_OUT_Play(uint16_t* pBuffer, uint32_t Size) {
    HAL_I2S_Transmit_DMA(&i2s, pBuffer, DMA_MAX(Size/AUDIODATA_SIZE)); 
    return AUDIO_OK;    
}


void audio_change_buffer(uint16_t *pData, uint16_t Size) {

    i2s.pTxBuffPtr = pData;
    i2s.TxXferSize  = Size;
    i2s.TxXferCount = Size;

    uint32_t *tmp = (uint32_t*)&pData;
    HAL_DMA_Start_IT(i2s.hdmatx, *(uint32_t*)tmp, (uint32_t)&i2s.Instance->DR, i2s.TxXferSize);
    SET_BIT(i2s.Instance->CR2, SPI_CR2_TXDMAEN);

}



uint8_t BSP_AUDIO_OUT_Pause(void) {    
    HAL_I2S_DMAPause(&i2s);
    return AUDIO_OK;
}

uint8_t BSP_AUDIO_OUT_Resume(void) {    
    HAL_I2S_DMAResume(&i2s);
    return AUDIO_OK;
}

uint8_t BSP_AUDIO_OUT_Stop(uint32_t Option) {
    HAL_I2S_DMAStop(&i2s);
    return AUDIO_OK;
}

void BSP_AUDIO_OUT_SetFrequency(uint32_t AudioFreq) { 
    BSP_AUDIO_OUT_ClockConfig(&i2s, AudioFreq, NULL);
    I2S3_Init(AudioFreq);
}



/**
  * @brief  Clock Config.
  * @param  hi2s: might be required to set audio peripheral predivider if any.
  * @param  AudioFreq: Audio frequency used to play the audio stream.
  * @note   This API is called by BSP_AUDIO_OUT_Init() and BSP_AUDIO_OUT_SetFrequency()
  *         Being __weak it can be overwritten by the application     
  * @param  Params : pointer on additional configuration parameters, can be NULL.
  */
__weak void BSP_AUDIO_OUT_ClockConfig(I2S_HandleTypeDef *hi2s, uint32_t AudioFreq, void *Params)
{ 
  RCC_PeriphCLKInitTypeDef rccclkinit;
  uint8_t index = 0, freqindex = 0xFF;
  
  for(index = 0; index < 8; index++)
  {
    if(I2SFreq[index] == AudioFreq)
    {
      freqindex = index;
    }
  }
  /* Enable PLLI2S clock */
  HAL_RCCEx_GetPeriphCLKConfig(&rccclkinit);
  /* PLLI2S_VCO Input = HSE_VALUE/PLL_M = 1 Mhz */
  if ((freqindex & 0x7) == 0)
  {
    /* I2S clock config 
    PLLI2S_VCO = f(VCO clock) = f(PLLI2S clock input) × (PLLI2SN/PLLM)
    I2SCLK = f(PLLI2S clock output) = f(VCO clock) / PLLI2SR */
    rccclkinit.PeriphClockSelection = RCC_PERIPHCLK_I2S;
    rccclkinit.PLLI2S.PLLI2SN = I2SPLLN[freqindex];
    rccclkinit.PLLI2S.PLLI2SR = I2SPLLR[freqindex];
    HAL_RCCEx_PeriphCLKConfig(&rccclkinit);
  }
  else 
  {
    /* I2S clock config 
    PLLI2S_VCO = f(VCO clock) = f(PLLI2S clock input) × (PLLI2SN/PLLM)
    I2SCLK = f(PLLI2S clock output) = f(VCO clock) / PLLI2SR */
    rccclkinit.PeriphClockSelection = RCC_PERIPHCLK_I2S;
    rccclkinit.PLLI2S.PLLI2SN = 258;
    rccclkinit.PLLI2S.PLLI2SR = 3;
    HAL_RCCEx_PeriphCLKConfig(&rccclkinit);
  }
}

/**
  * @brief  AUDIO OUT I2S MSP Init.
  * @param  hi2s: might be required to set audio peripheral predivider if any.
  * @param  Params : pointer on additional configuration parameters, can be NULL.
  */
__weak void BSP_AUDIO_OUT_MspInit(I2S_HandleTypeDef *hi2s, void *Params)
{
  static DMA_HandleTypeDef hdma_i2sTx;
  GPIO_InitTypeDef  GPIO_InitStruct;
  
  /* Enable I2S3 clock */
  I2S3_CLK_ENABLE();
  
  /*** Configure the GPIOs ***/  
  /* Enable I2S GPIO clocks */
  I2S3_SCK_SD_CLK_ENABLE();
  I2S3_WS_CLK_ENABLE();
  
  /* I2S3 pins configuration: WS, SCK and SD pins ----------------------------*/
  GPIO_InitStruct.Pin         = I2S3_SCK_PIN | I2S3_SD_PIN; 
  GPIO_InitStruct.Mode        = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull        = GPIO_NOPULL;
  GPIO_InitStruct.Speed       = GPIO_SPEED_FAST;
  GPIO_InitStruct.Alternate   = I2S3_SCK_SD_WS_AF;
  HAL_GPIO_Init(I2S3_SCK_SD_GPIO_PORT, &GPIO_InitStruct);
  
  GPIO_InitStruct.Pin         = I2S3_WS_PIN ;
  HAL_GPIO_Init(I2S3_WS_GPIO_PORT, &GPIO_InitStruct); 
   
  /* Enable the I2S DMA clock */
  I2S3_DMAx_CLK_ENABLE(); 
  
  if(hi2s->Instance == I2S3)
  {
    /* Configure the hdma_i2sTx handle parameters */   
    hdma_i2sTx.Init.Channel             = I2S3_DMAx_CHANNEL;  
    hdma_i2sTx.Init.Direction           = DMA_MEMORY_TO_PERIPH;
    hdma_i2sTx.Init.PeriphInc           = DMA_PINC_DISABLE;
    hdma_i2sTx.Init.MemInc              = DMA_MINC_ENABLE;
    hdma_i2sTx.Init.PeriphDataAlignment = I2S3_DMAx_PERIPH_DATA_SIZE;
    hdma_i2sTx.Init.MemDataAlignment    = I2S3_DMAx_MEM_DATA_SIZE;
    hdma_i2sTx.Init.Mode                = DMA_NORMAL;
    hdma_i2sTx.Init.Priority            = DMA_PRIORITY_HIGH;
    hdma_i2sTx.Init.FIFOMode            = DMA_FIFOMODE_ENABLE;         
    hdma_i2sTx.Init.FIFOThreshold       = DMA_FIFO_THRESHOLD_FULL;
    hdma_i2sTx.Init.MemBurst            = DMA_MBURST_SINGLE;
    hdma_i2sTx.Init.PeriphBurst         = DMA_PBURST_SINGLE; 
    
    hdma_i2sTx.Instance                 = I2S3_DMAx_STREAM;
    
    /* Associate the DMA handle */
    __HAL_LINKDMA(hi2s, hdmatx, hdma_i2sTx);
    
    /* Deinitialize the Stream for new transfer */
    HAL_DMA_DeInit(&hdma_i2sTx);
    
    /* Configure the DMA Stream */
    HAL_DMA_Init(&hdma_i2sTx);
  }
  
  /* I2S DMA IRQ Channel configuration */
  HAL_NVIC_SetPriority(I2S3_DMAx_IRQ, AUDIO_OUT_IRQ_PREPRIO, 0);
  HAL_NVIC_EnableIRQ(I2S3_DMAx_IRQ); 
}

/**
  * @brief  De-Initializes BSP_AUDIO_OUT MSP.
  * @param  hi2s: might be required to set audio peripheral predivider if any.
  * @param  Params : pointer on additional configuration parameters, can be NULL.
  */
__weak void BSP_AUDIO_OUT_MspDeInit(I2S_HandleTypeDef *hi2s, void *Params)
{  
  GPIO_InitTypeDef  GPIO_InitStruct;

  /* I2S DMA IRQ Channel deactivation */
  HAL_NVIC_DisableIRQ(I2S3_DMAx_IRQ); 
  
  if(hi2s->Instance == I2S3)
  {
    /* Deinitialize the Stream for new transfer */
    HAL_DMA_DeInit(hi2s->hdmatx);
  }

 /* Disable I2S block */
  __HAL_I2S_DISABLE(hi2s);

  /* CODEC_I2S pins configuration: SCK and SD pins */
  GPIO_InitStruct.Pin = I2S3_SCK_PIN | I2S3_SD_PIN;
  HAL_GPIO_DeInit(I2S3_SCK_SD_GPIO_PORT, GPIO_InitStruct.Pin);
  
  /* CODEC_I2S pins configuration: WS pin */
  GPIO_InitStruct.Pin = I2S3_WS_PIN;
  HAL_GPIO_DeInit(I2S3_WS_GPIO_PORT, GPIO_InitStruct.Pin);
  
  /* Disable I2S clock */
  I2S3_CLK_DISABLE();

  /* GPIO pins clock and DMA clock can be shut down in the applic 
     by surcgarging this __weak function */   
}


static uint8_t I2S3_Init(uint32_t AudioFreq)
{
  /* Initialize the hAudioOutI2s Instance parameter */
  i2s.Instance         = I2S3;
 
 /* Disable I2S block */
  __HAL_I2S_DISABLE(&i2s);
   
  /* I2S3 peripheral configuration */
  i2s.Init.AudioFreq   = AudioFreq;
  i2s.Init.ClockSource = I2S_CLOCK_PLL;
  i2s.Init.CPOL        = I2S_CPOL_LOW;
  i2s.Init.DataFormat  = I2S_DATAFORMAT_16B;
  i2s.Init.MCLKOutput  = I2S_MCLKOUTPUT_DISABLE;
  i2s.Init.Mode        = I2S_MODE_MASTER_TX;
  i2s.Init.Standard    = I2S_STANDARD_PHILIPS;
  /* Initialize the I2S peripheral with the structure above */  
  if(HAL_I2S_Init(&i2s) != HAL_OK)
  {
    return AUDIO_ERROR;
  }
  else
  {
    return AUDIO_OK;
  }
 }
