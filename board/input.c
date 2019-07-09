#include "board.h"
#include "stm32f4xx_hal.h"
#include "stm32f4xx_ll_system.h"
#include "stm32f4xx_ll_i2c.h"
#include "stm32f4xx_ll_rcc.h"
#include "stm32f4xx_ll_exti.h"
#include "stm32f4xx_ll_adc.h"
#include "stm32f4xx_ll_dma.h"
#include <string.h>

void gpio_init(void);
void encoder_init(void);
void adc_init(void);

// Initialise input hardware.
void input_init(void) {

    gpio_init();
    encoder_init();
    adc_init();

}


/******************************************************************************/
// GPIO - buttons/LEDs

ButtonState buttons[NUM_BUTTONS];
bool leds[16];
static bool leds_current[16];

#define GPIO_I2C_SPEED 100000
#define GPIO_I2C    I2C3
#define GPIO_I2C_CLK_EN __HAL_RCC_I2C3_CLK_ENABLE
#define GPIO_SDA_PORT GPIOC
#define GPIO_SDA_PIN  LL_GPIO_PIN_9
#define GPIO_SCL_PORT GPIOA
#define GPIO_SCL_PIN  LL_GPIO_PIN_8

#define GPIO_BANK1 0x48 // U7
#define GPIO_BANK2 0x40 // U6

// Extra buttons wired directly to MCU
#define BTN_ENC_PORT GPIOC
#define BTN_ENC_PIN  LL_GPIO_PIN_6
#define BTN_MS1_PORT GPIOC
#define BTN_MS1_PIN  LL_GPIO_PIN_0
#define BTN_MS2_PORT GPIOC
#define BTN_MS2_PIN  LL_GPIO_PIN_15
#define BTN_MS3_PORT GPIOC
#define BTN_MS3_PIN  LL_GPIO_PIN_14
#define BTN_MS4_PORT GPIOC
#define BTN_MS4_PIN  LL_GPIO_PIN_13
#define BTN_MS5_PORT GPIOB
#define BTN_MS5_PIN  LL_GPIO_PIN_7

uint8_t read_reg(uint8_t addr, uint8_t reg) {

    uint8_t value = 0;

    while(LL_I2C_IsActiveFlag_BUSY(GPIO_I2C));

    // Start
    LL_I2C_GenerateStartCondition(GPIO_I2C);
    while(!LL_I2C_IsActiveFlag_SB(GPIO_I2C));

    // Send address (tx)
    LL_I2C_TransmitData8(GPIO_I2C, addr);
    while(!LL_I2C_IsActiveFlag_ADDR(GPIO_I2C));
    LL_I2C_ClearFlag_ADDR(GPIO_I2C);
    
    // Send register
    while(!LL_I2C_IsActiveFlag_TXE(GPIO_I2C));
    LL_I2C_TransmitData8(GPIO_I2C, reg);

    // RX
    LL_I2C_GenerateStartCondition(GPIO_I2C);
    while(!LL_I2C_IsActiveFlag_SB(GPIO_I2C));

    // Send address (rx)
    LL_I2C_TransmitData8(GPIO_I2C, addr+1);
    while(!LL_I2C_IsActiveFlag_ADDR(GPIO_I2C));
    LL_I2C_ClearFlag_ADDR(GPIO_I2C);

    // Read value
    while(!LL_I2C_IsActiveFlag_RXNE(GPIO_I2C));
    value = LL_I2C_ReceiveData8(GPIO_I2C);

    LL_I2C_GenerateStopCondition(GPIO_I2C);

    return value;

}

void write_reg(uint8_t addr, uint8_t reg, uint8_t value) {

    while(LL_I2C_IsActiveFlag_BUSY(GPIO_I2C));

    LL_I2C_GenerateStartCondition(GPIO_I2C);
    while(!LL_I2C_IsActiveFlag_SB(GPIO_I2C));

    LL_I2C_TransmitData8(GPIO_I2C, addr);
    while(!LL_I2C_IsActiveFlag_ADDR(GPIO_I2C));
    LL_I2C_ClearFlag_ADDR(GPIO_I2C);
    while(!LL_I2C_IsActiveFlag_TXE(GPIO_I2C));

    LL_I2C_TransmitData8(GPIO_I2C, reg);
    while(!LL_I2C_IsActiveFlag_TXE(GPIO_I2C));        

    LL_I2C_TransmitData8(GPIO_I2C, value);
    while(!LL_I2C_IsActiveFlag_TXE(GPIO_I2C));

    LL_I2C_GenerateStopCondition(GPIO_I2C);

}

void gpio_init(void) {

    // Pin init
    pin_cfg_i2c(GPIO_SCL_PORT, GPIO_SCL_PIN);
    pin_cfg_i2c(GPIO_SDA_PORT, GPIO_SDA_PIN);
    pin_cfg_input(BTN_ENC_PORT, BTN_ENC_PIN, LL_GPIO_PULL_UP);
    pin_cfg_input(BTN_MS1_PORT, BTN_MS1_PIN, LL_GPIO_PULL_UP);
    pin_cfg_input(BTN_MS2_PORT, BTN_MS2_PIN, LL_GPIO_PULL_UP);
    pin_cfg_input(BTN_MS3_PORT, BTN_MS3_PIN, LL_GPIO_PULL_UP);
    pin_cfg_input(BTN_MS4_PORT, BTN_MS4_PIN, LL_GPIO_PULL_UP);
    pin_cfg_input(BTN_MS5_PORT, BTN_MS5_PIN, LL_GPIO_PULL_UP);

    // I2C init
    GPIO_I2C_CLK_EN();
    LL_I2C_Disable(GPIO_I2C);
    LL_RCC_ClocksTypeDef rcc_clocks;
    LL_RCC_GetSystemClocksFreq(&rcc_clocks);
    LL_I2C_ConfigSpeed(GPIO_I2C, rcc_clocks.PCLK1_Frequency, GPIO_I2C_SPEED, LL_I2C_DUTYCYCLE_2);
    LL_I2C_Enable(GPIO_I2C);

    // Set pull-ups
    write_reg(GPIO_BANK1, 0x0C, 0xFF);
    write_reg(GPIO_BANK1, 0x0D, 0xFF);
    write_reg(GPIO_BANK2, 0x0C, 0xFF);
    write_reg(GPIO_BANK2, 0x0D, 0xFF);

    // Set output direction
    write_reg(GPIO_BANK1, 0x00, 0xAA);
    write_reg(GPIO_BANK1, 0x01, 0x55);
    write_reg(GPIO_BANK2, 0x00, 0xAA);
    write_reg(GPIO_BANK2, 0x01, 0x55);

}

// Update the state of a single button.
static bool update_button(int b, bool pressed) {

    bool changed = false;

    switch (buttons[b]) {
        case BTN_OFF:
            if (pressed) {
                buttons[b] = BTN_DOWN;
                changed = true;
            }
            break;
        case BTN_DOWN:
            buttons[b] = pressed ? BTN_HELD : BTN_UP;
            changed = true;
            break;
        case BTN_HELD:
            if (!pressed) {
                buttons[b] = BTN_UP;
                changed = true;
            }
            break;
        case BTN_UP:
            buttons[b] = BTN_OFF;
            changed = true;
            break;
    }    

    return changed;

}

// Read each button and update its state.
// Returns true if any buttons have changed state.
bool read_buttons(void) {

    // HACK:
    // Disable ADC readings while reading the GPIOs.
    // Otherwise the readings are noisy (to be fixed with proper PCB layout)
    //LL_ADC_Disable(ADC1);
    //LL_DMA_DisableStream(DMA2, LL_DMA_STREAM_0); 
    //LL_DMA_ClearFlag_HT0(DMA2);
    //LL_DMA_ClearFlag_TC0(DMA2);
    //LL_DMA_ClearFlag_TE0(DMA2);

    bool changed = false;

    // Read keys 1-16 via I2C
    uint8_t b1a = ~read_reg(GPIO_BANK1, 0x12);
    uint8_t b1b = ~read_reg(GPIO_BANK1, 0x13);
    uint8_t b2a = ~read_reg(GPIO_BANK2, 0x12);
    uint8_t b2b = ~read_reg(GPIO_BANK2, 0x13);

    changed |= update_button(0,  b1b & 0x01);
    changed |= update_button(1,  b1b & 0x04);
    changed |= update_button(2,  b1b & 0x10);
    changed |= update_button(3,  b1b & 0x40);
    changed |= update_button(4,  b1a & 0x80);
    changed |= update_button(5,  b1a & 0x20);
    changed |= update_button(6,  b1a & 0x08);
    changed |= update_button(7,  b1a & 0x02);
    changed |= update_button(8,  b2b & 0x01);
    changed |= update_button(9,  b2b & 0x04);
    changed |= update_button(10, b2b & 0x10);
    changed |= update_button(11, b2b & 0x40);
    changed |= update_button(12, b2a & 0x80);
    changed |= update_button(13, b2a & 0x20);
    changed |= update_button(14, b2a & 0x08);
    changed |= update_button(15, b2a & 0x02);

    // Function keys
    changed |= update_button(BTN_SHIFT,     !pin_read(BTN_MS5_PORT, BTN_MS5_PIN));
    changed |= update_button(BTN_REC_SAVE,  !pin_read(BTN_MS4_PORT, BTN_MS4_PIN));
    changed |= update_button(BTN_PLAY_LOAD, !pin_read(BTN_MS3_PORT, BTN_MS3_PIN));
    changed |= update_button(BTN_SEQ_EDIT,  !pin_read(BTN_MS2_PORT, BTN_MS2_PIN));
    changed |= update_button(BTN_SYNTH_MENU,!pin_read(BTN_MS1_PORT, BTN_MS1_PIN));
    changed |= update_button(BTN_ENCODER,   !pin_read(BTN_ENC_PORT, BTN_ENC_PIN));

    // Re-enable ADCs
    //LL_DMA_EnableStream(DMA2, LL_DMA_STREAM_0);  
    //LL_ADC_Enable(ADC1);
    //LL_ADC_REG_StartConversionSWStart(ADC1);   

    return changed;

}

void update_leds(void) {

    uint8_t porta, portb;

    // Update LEDs over I2C if any have changed
    if (memcmp(leds, leds_current, sizeof(leds))) {
        porta = leds[15] | (leds[14] << 2) | (leds[13] << 4) | (leds[12] << 6);
        portb = (leds[8] << 1) | (leds[9] << 3) | (leds[10] << 5) | (leds[11] << 7);
        write_reg(GPIO_BANK2, 0x12, porta);
        write_reg(GPIO_BANK2, 0x13, portb);

        porta = leds[7] | (leds[6] << 2) | (leds[5] << 4) | (leds[4] << 6);
        portb = (leds[0] << 1) | (leds[1] << 3) | (leds[2] << 5) | (leds[3] << 7);
        write_reg(GPIO_BANK1, 0x12, porta);
        write_reg(GPIO_BANK1, 0x13, portb);

        memcpy(leds_current, leds, sizeof(leds));

    }

}




/******************************************************************************/
// Encoder

#define ENCODER_PORT    GPIOA
#define ENCODER_PIN_A   LL_GPIO_PIN_12
#define ENCODER_PIN_B   LL_GPIO_PIN_11


int enc_states[16] = {0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};
uint8_t enc_history;
EncoderState encoder;


void encoder_init(void) {

    pin_cfg_exti(ENCODER_PORT, ENCODER_PIN_A,  LL_GPIO_PULL_UP, LL_EXTI_TRIGGER_RISING_FALLING);
    pin_cfg_exti(ENCODER_PORT, ENCODER_PIN_B,  LL_GPIO_PULL_UP, LL_EXTI_TRIGGER_RISING_FALLING);
    enc_history = (pin_read(ENCODER_PORT, ENCODER_PIN_B) << 1) | pin_read(ENCODER_PORT, ENCODER_PIN_A);

    LL_SYSCFG_SetEXTISource(LL_SYSCFG_EXTI_PORTA, LL_SYSCFG_EXTI_LINE11);
    LL_SYSCFG_SetEXTISource(LL_SYSCFG_EXTI_PORTA, LL_SYSCFG_EXTI_LINE12);
    HAL_NVIC_SetPriority(EXTI15_10_IRQn, PRIORITY_ENCODER, 0);
    HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

}

// Update the delta for the encoder.
// Returns true if it has moved.
bool read_encoder(void) {

    bool changed = false;

    int ev = encoder.value;

    encoder.delta = ev - encoder.last_value;
    encoder.half_delta = (ev/2) - (encoder.last_value/2);
    if (encoder.delta != 0) changed = true;
    encoder.last_value = ev;

    return changed;

}

void EXTI15_10_IRQHandler(void) {

    uint32_t port = ENCODER_PORT->IDR;

    if (LL_EXTI_IsActiveFlag_0_31(LL_EXTI_LINE_11)) LL_EXTI_ClearFlag_0_31(LL_EXTI_LINE_11);
    if (LL_EXTI_IsActiveFlag_0_31(LL_EXTI_LINE_12)) LL_EXTI_ClearFlag_0_31(LL_EXTI_LINE_12);

    int inca = !!(port & ENCODER_PIN_A);
    int incb = !!(port & ENCODER_PIN_B);
    enc_history <<= 2;
    enc_history |= ((incb << 1) | inca);
    encoder.value += enc_states[enc_history & 0x0F];

}



/******************************************************************************/
// ADC

uint16_t pots[NUM_POTS];
#define CYCLES LL_ADC_SAMPLINGTIME_480CYCLES

void adc_init(void) {

    pin_cfg_an(GPIOB, LL_GPIO_PIN_1);   // ch9
    pin_cfg_an(GPIOA, LL_GPIO_PIN_6);   // ch6
    pin_cfg_an(GPIOA, LL_GPIO_PIN_1);   // ch1
    pin_cfg_an(GPIOB, LL_GPIO_PIN_0);   // ch8
    pin_cfg_an(GPIOA, LL_GPIO_PIN_5);   // ch5
    pin_cfg_an(GPIOA, LL_GPIO_PIN_0);   // ch0
    pin_cfg_an(GPIOC, LL_GPIO_PIN_5);   // ch15
    pin_cfg_an(GPIOA, LL_GPIO_PIN_4);   // ch4
    pin_cfg_an(GPIOC, LL_GPIO_PIN_3);   // ch13
    pin_cfg_an(GPIOC, LL_GPIO_PIN_4);   // ch14
    pin_cfg_an(GPIOA, LL_GPIO_PIN_3);   // ch3
    pin_cfg_an(GPIOC, LL_GPIO_PIN_2);   // ch12
    pin_cfg_an(GPIOA, LL_GPIO_PIN_7);   // ch7
    pin_cfg_an(GPIOA, LL_GPIO_PIN_2);   // ch2
    pin_cfg_an(GPIOC, LL_GPIO_PIN_1);   // ch11

    __HAL_RCC_ADC1_CLK_ENABLE();
    __HAL_RCC_DMA2_CLK_ENABLE();

    LL_ADC_CommonInitTypeDef adc_common;
    adc_common.CommonClock = LL_ADC_CLOCK_SYNC_PCLK_DIV8;
    adc_common.Multimode = LL_ADC_MULTI_INDEPENDENT;
    adc_common.MultiDMATransfer = LL_ADC_MULTI_REG_DMA_EACH_ADC;
    adc_common.MultiTwoSamplingDelay = LL_ADC_MULTI_TWOSMP_DELAY_5CYCLES;
    LL_ADC_CommonInit(ADC123_COMMON, &adc_common);

    LL_ADC_InitTypeDef adc;
    adc.Resolution = LL_ADC_RESOLUTION_12B;
    adc.DataAlignment = LL_ADC_DATA_ALIGN_RIGHT;
    adc.SequencersScanMode = LL_ADC_SEQ_SCAN_ENABLE;
    LL_ADC_Init(ADC1, &adc);

    LL_ADC_REG_InitTypeDef reg;
    reg.TriggerSource = LL_ADC_REG_TRIG_SOFTWARE;
    reg.SequencerLength = LL_ADC_REG_SEQ_SCAN_ENABLE_15RANKS;
    reg.SequencerDiscont = LL_ADC_REG_SEQ_DISCONT_DISABLE;
    reg.ContinuousMode = LL_ADC_REG_CONV_CONTINUOUS;
    reg.DMATransfer = LL_ADC_REG_DMA_TRANSFER_UNLIMITED;
    LL_ADC_REG_Init(ADC1, &reg);

    LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_1,  LL_ADC_CHANNEL_9);
    LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_2,  LL_ADC_CHANNEL_6);
    LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_3,  LL_ADC_CHANNEL_1);
    LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_4,  LL_ADC_CHANNEL_8);
    LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_5,  LL_ADC_CHANNEL_5);
    LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_6,  LL_ADC_CHANNEL_0);
    LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_7,  LL_ADC_CHANNEL_15);
    LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_8,  LL_ADC_CHANNEL_4);
    LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_9,  LL_ADC_CHANNEL_13);
    LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_10, LL_ADC_CHANNEL_14);
    LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_11, LL_ADC_CHANNEL_3);
    LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_12, LL_ADC_CHANNEL_12);
    LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_13, LL_ADC_CHANNEL_7);    
    LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_14, LL_ADC_CHANNEL_2);    
    LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_15, LL_ADC_CHANNEL_11);

    LL_ADC_SetChannelSamplingTime(ADC1, LL_ADC_CHANNEL_0, CYCLES);
    LL_ADC_SetChannelSamplingTime(ADC1, LL_ADC_CHANNEL_1, CYCLES);
    LL_ADC_SetChannelSamplingTime(ADC1, LL_ADC_CHANNEL_2, CYCLES);
    LL_ADC_SetChannelSamplingTime(ADC1, LL_ADC_CHANNEL_3, CYCLES);
    LL_ADC_SetChannelSamplingTime(ADC1, LL_ADC_CHANNEL_4, CYCLES);
    LL_ADC_SetChannelSamplingTime(ADC1, LL_ADC_CHANNEL_5, CYCLES);
    LL_ADC_SetChannelSamplingTime(ADC1, LL_ADC_CHANNEL_6, CYCLES);
    LL_ADC_SetChannelSamplingTime(ADC1, LL_ADC_CHANNEL_7, CYCLES);
    LL_ADC_SetChannelSamplingTime(ADC1, LL_ADC_CHANNEL_8, CYCLES);
    LL_ADC_SetChannelSamplingTime(ADC1, LL_ADC_CHANNEL_9, CYCLES);
    LL_ADC_SetChannelSamplingTime(ADC1, LL_ADC_CHANNEL_11, CYCLES);
    LL_ADC_SetChannelSamplingTime(ADC1, LL_ADC_CHANNEL_12, CYCLES);
    LL_ADC_SetChannelSamplingTime(ADC1, LL_ADC_CHANNEL_13, CYCLES);
    LL_ADC_SetChannelSamplingTime(ADC1, LL_ADC_CHANNEL_14, CYCLES);
    LL_ADC_SetChannelSamplingTime(ADC1, LL_ADC_CHANNEL_15, CYCLES);
    
    

    LL_ADC_Enable(ADC1);

    // DMA init
    LL_DMA_InitTypeDef ddma;
    ddma.PeriphOrM2MSrcAddress  = (uint32_t)&(ADC1->DR);
    ddma.MemoryOrM2MDstAddress  = (uint32_t)&pots;
    ddma.Direction              = LL_DMA_DIRECTION_PERIPH_TO_MEMORY;
    ddma.Mode                   = LL_DMA_MODE_CIRCULAR;
    ddma.PeriphOrM2MSrcIncMode  = LL_DMA_PERIPH_NOINCREMENT;
    ddma.MemoryOrM2MDstIncMode  = LL_DMA_MEMORY_INCREMENT;
    ddma.PeriphOrM2MSrcDataSize = LL_DMA_PDATAALIGN_HALFWORD;
    ddma.MemoryOrM2MDstDataSize = LL_DMA_MDATAALIGN_HALFWORD;
    ddma.NbData                 = NUM_POTS;
    ddma.Channel                = LL_DMA_CHANNEL_0;
    ddma.Priority               = LL_DMA_PRIORITY_LOW;
    ddma.FIFOMode               = LL_DMA_FIFOMODE_DISABLE;
    ddma.FIFOThreshold          = LL_DMA_FIFOTHRESHOLD_1_4;
    ddma.MemBurst               = LL_DMA_MBURST_SINGLE;
    ddma.PeriphBurst            = LL_DMA_PBURST_SINGLE;
    LL_DMA_Init(DMA2, LL_DMA_STREAM_0, &ddma);
    LL_DMA_EnableStream(DMA2, LL_DMA_STREAM_0);  

    LL_ADC_REG_StartConversionSWStart(ADC1);

}

