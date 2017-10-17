#include "audio_play.h"
#include <math.h>
#define PI 3.1415926

extern __IO uint8_t UserPressButton;

#define OUT_BUFFER_SIZE 1024
uint16_t out_buffer_1[OUT_BUFFER_SIZE];
uint16_t out_buffer_2[OUT_BUFFER_SIZE];

uint16_t *out_buffer = out_buffer_1;

// Phase to amplitude conversion table
#define SINE_TABLE_WIDTH 12 // bits
#define SINE_TABLE_SIZE (1<<SINE_TABLE_WIDTH)
uint16_t sine_table[SINE_TABLE_SIZE];

uint32_t nco_phase;
volatile uint32_t freq = 0.02553125 * UINT32_MAX;

void swap_buffers(void) {

    if (out_buffer == out_buffer_1) {
        out_buffer = out_buffer_2;
    } else {
        out_buffer = out_buffer_1;
    }
}

void AudioPlay_Test(void) {  

    __IO uint8_t volume = 50; // 0 - 100

    // fill sin table
    for (int i=0; i<SINE_TABLE_SIZE; i++) {
        float arg = 2*PI*i / SINE_TABLE_SIZE;
        sine_table[i] = (int16_t)(sin(arg) * 32768);
    }

    // fill buffer
    for (int i=0; i<OUT_BUFFER_SIZE; i += 2) {
        nco_phase += freq;
        int index = nco_phase >> (32 - SINE_TABLE_WIDTH);
        uint16_t sl = sine_table[index]; 
        out_buffer[i] = sl;   // left
        out_buffer[i+1] = sl; // right
    }

    if (BSP_AUDIO_OUT_Init(OUTPUT_DEVICE_AUTO, volume, SAMPLE_RATE) != 0) {
        printf("init failed\r\n");
        return;
    }

    BSP_AUDIO_OUT_Play(out_buffer, OUT_BUFFER_SIZE * 2);

}

void BSP_AUDIO_OUT_TransferComplete_CallBack() {

     
    BSP_AUDIO_OUT_ChangeBuffer(out_buffer, OUT_BUFFER_SIZE);
 
    swap_buffers();

    // fill buffer
    for (int i=0; i<OUT_BUFFER_SIZE; i += 2) {
        nco_phase += freq;
        int index = nco_phase >> (32 - SINE_TABLE_WIDTH);
        uint16_t sl = sine_table[index]; 
        out_buffer[i] = sl;   // left
        out_buffer[i+1] = sl; // right
    }

    printf("done\r\n");



}

void BSP_AUDIO_OUT_Error_CallBack(void) {
    // printf("outerr");
    // Error_Handler();
}
