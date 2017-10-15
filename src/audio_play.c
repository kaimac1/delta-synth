#include "audio_play.h"

extern __IO uint8_t UserPressButton;

/* Variables used in norma mode to manage audio file during DMA transfer*/
uint32_t AudioTotalSize           = 0xFFFF; /* This variable holds the total size of the audio file */
uint32_t AudioRemSize             = 0xFFFF; /* This variable holds the remaining data in audio file */
uint16_t *CurrentPos;             /* This variable holds the current position of audio pointer */


/* Size of the recorder buffer (Multiple of 4096, RAM_BUFFER_SIZE used in BSP)*/
#define WR_BUFFER_SIZE           0x7000

uint16_t WrBuffer[WR_BUFFER_SIZE];



void AudioPlay_Test(void) {  

  __IO uint8_t volume = 35; // 0 - 100
  uint32_t sample_rate = 22050;


  // fill buffer
  for (int i=0; i<WR_BUFFER_SIZE; i += 2) {
    uint16_t sl = i * 500;
    uint16_t sr = i * 800;
    WrBuffer[i] = sl;
    WrBuffer[i+1] = sr;
  }

  if (BSP_AUDIO_OUT_Init(OUTPUT_DEVICE_AUTO, volume, sample_rate) != 0) {
    printf("init failed\r\n");
    return;
  }

  printf("audioplay test\r\n");
  
  
  AudioTotalSize = WR_BUFFER_SIZE * 2;
  CurrentPos = WrBuffer;
  
  BSP_AUDIO_OUT_Play(CurrentPos, AudioTotalSize);

  AudioRemSize = AudioTotalSize - AUDIODATA_SIZE * DMA_MAX(AudioTotalSize);   
  CurrentPos += DMA_MAX(AudioTotalSize);

  UserPressButton = 0;  
  while(!UserPressButton);
  
  /* Stop Player before close Test */
  if (BSP_AUDIO_OUT_Stop(CODEC_PDWN_SW) != AUDIO_OK) {
    printf("stop error\r\n");
  }

  printf("stopped\r\n");
}

/*--------------------------------
Callbacks implementation:
The callbacks prototypes are defined in the stm32f401_discovery_audio.h file
and their implementation should be done in the user code if they are needed.
Below some examples of callback implementations.
--------------------------------------------------------*/
/**
* @brief  Calculates the remaining file size and new position of the pointer.
* @param  None
* @retval None
*/
void BSP_AUDIO_OUT_TransferComplete_CallBack()
{
  uint32_t replay = 1;
  
  // if (AudioRemSize > 0)
  // {
  //   /* Replay from the current position */
  //   BSP_AUDIO_OUT_ChangeBuffer((uint16_t*)CurrentPos, DMA_MAX(AudioRemSize/AUDIODATA_SIZE));
    
  //   /* Update the current pointer position */
  //   CurrentPos += DMA_MAX(AudioRemSize);        
    
  //   /* Update the remaining number of data to be played */
  //   AudioRemSize -= AUDIODATA_SIZE * DMA_MAX(AudioRemSize/AUDIODATA_SIZE);  
  // }
  // else
  // {
  //   /* Request to replay audio file from beginning */
  //   replay = 1;
  // }

  // /* Audio sample used for play */
  // if(replay == 1)
  // {
  //   /* Replay from the beginning */
  //   /* Set the current audio pointer position */
  //   CurrentPos = WrBuffer;
  //   /* Replay from the beginning */
  //   BSP_AUDIO_OUT_Play(CurrentPos, AudioTotalSize);
  //   /* Update the remaining number of data to be played */
  //   AudioRemSize = AudioTotalSize - AUDIODATA_SIZE * DMA_MAX(AudioTotalSize);  
  //   /* Update the current audio pointer position */
  //   CurrentPos += DMA_MAX(AudioTotalSize);
  // }
  
  /* Audio sample saved during record */
  if(replay == 1)
  {
    /* Replay from the beginning */
    BSP_AUDIO_OUT_Play(WrBuffer, AudioTotalSize);
  }
}

/**
  * @brief  Manages the DMA FIFO error interrupt.
  * @param  None
  * @retval None
  */
void BSP_AUDIO_OUT_Error_CallBack(void)
{
  /* Stop the program with an infinite loop */
  Error_Handler();
}
