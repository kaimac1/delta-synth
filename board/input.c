#include "board.h"
#include "stm32f4xx_hal.h"
#include "stm32f4xx_ll_system.h"
#include "stm32f4xx_ll_spi.h"
#include "stm32f4xx_ll_exti.h"
#include "stm32f4xx_ll_adc.h"
#include "stm32f4xx_ll_dma.h"



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
// GPIO expander

#define SWITCH_SPI  SPI2
#define SPI_CLK_EN  __HAL_RCC_SPI2_CLK_ENABLE
#define CS_PORT     GPIOB
#define CS_PIN      LL_GPIO_PIN_12
#define SCK_PORT    GPIOB
#define SCK_PIN     LL_GPIO_PIN_13
#define MISO_PORT   GPIOB
#define MISO_PIN    LL_GPIO_PIN_14
#define MOSI_PORT   GPIOB
#define MOSI_PIN    LL_GPIO_PIN_15

ButtonState buttons[NUM_BUTTONS];

uint8_t spi_tx(uint8_t byte) {
    while (!LL_SPI_IsActiveFlag_TXE(SWITCH_SPI));
    SWITCH_SPI->DR = byte;
    while (!LL_I2S_IsActiveFlag_RXNE(SWITCH_SPI));
    return SWITCH_SPI->DR;
}


uint8_t read_reg(uint8_t reg) {

    uint8_t value;

    pin_set(CS_PORT, CS_PIN, 0);
    delay_us(100);
    spi_tx(0x41); // Read
    spi_tx(reg);
    value = spi_tx(0x00);
    pin_set(CS_PORT, CS_PIN, 1);

    return value;

}

void write_reg(uint8_t reg, uint8_t value) {

    pin_set(CS_PORT, CS_PIN, 0);
    spi_tx(0x40); // Write
    spi_tx(reg);
    spi_tx(value);
    pin_set(CS_PORT, CS_PIN, 1);    

}

void gpio_init(void) {

    SPI_CLK_EN();

    // Pin init
    pin_cfg_output(CS_PORT, CS_PIN);
    pin_set(CS_PORT, CS_PIN, 1);
    pin_cfg_af(SCK_PORT, SCK_PIN, 5);
    pin_cfg_af(MOSI_PORT, MOSI_PIN, 5);
    pin_cfg_af(MISO_PORT, MISO_PIN, 5);    

    // SPI init
    LL_SPI_InitTypeDef spi;
    spi.TransferDirection = LL_SPI_FULL_DUPLEX;
    spi.Mode            = LL_SPI_MODE_MASTER;
    spi.DataWidth       = LL_SPI_DATAWIDTH_8BIT;
    spi.ClockPolarity   = LL_SPI_POLARITY_LOW;
    spi.ClockPhase      = LL_SPI_PHASE_1EDGE;
    spi.NSS             = LL_SPI_NSS_SOFT;
    spi.BaudRate        = LL_SPI_BAUDRATEPRESCALER_DIV16;
    spi.BitOrder        = LL_SPI_MSB_FIRST; 
    spi.CRCCalculation  = LL_SPI_CRCCALCULATION_DISABLE;
    spi.CRCPoly         = 0;
    LL_SPI_Init(SWITCH_SPI, &spi);
    LL_SPI_Enable(SWITCH_SPI);    

    // Set pull-ups
    write_reg(0x0C, 0xFF);
    write_reg(0x0D, 0xFF);

}

// Read each button and update its state.
// Returns true if any buttons have changed state.
bool read_buttons(void) {

    uint16_t switches = ~((read_reg(0x13) << 8) | read_reg(0x12));
    bool changed = false;

    for (int i=0; i<NUM_BUTTONS; i++) {
        bool pressed = switches & (1 << i);

        switch (buttons[i]) {
            case BTN_OFF:
                if (pressed) {
                    buttons[i] = BTN_DOWN;
                    changed = true;
                }
                break;
            case BTN_DOWN:
                buttons[i] = pressed ? BTN_HELD : BTN_UP;
                changed = true;
                break;
            case BTN_HELD:
                if (!pressed) {
                    buttons[i] = BTN_UP;
                    changed = true;
                }
                break;
            case BTN_UP:
                buttons[i] = BTN_OFF;
                changed = true;
                break;
        }
    }

    return changed;

}




/******************************************************************************/
// Encoder

#define ENCODER_PORT    GPIOA
#define ENCODER_PIN_A   LL_GPIO_PIN_11
#define ENCODER_PIN_B   LL_GPIO_PIN_12

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

    encoder.delta = encoder.value - encoder.last_value;
    if (encoder.delta != 0) changed = true;
    encoder.last_value = encoder.value;

    return changed;

}

void EXTI15_10_IRQHandler(void) {

    LL_EXTI_ClearFlag_0_31(LL_EXTI_LINE_ALL);
    uint32_t port = ENCODER_PORT->IDR;
    int inca = !!(port & ENCODER_PIN_A);
    int incb = !!(port & ENCODER_PIN_B);
    enc_history <<= 2;
    enc_history |= ((incb << 1) | inca);
    encoder.value += enc_states[enc_history & 0x0F];

}



/******************************************************************************/
// ADC

uint16_t pots[NUM_POTS];

void adc_init(void) {

    pin_cfg_an(GPIOA, LL_GPIO_PIN_0);   // ch0  Osc1
    pin_cfg_an(GPIOA, LL_GPIO_PIN_1);   // ch1  Osc2
    pin_cfg_an(GPIOC, LL_GPIO_PIN_0);   // ch10 Noise
    pin_cfg_an(GPIOC, LL_GPIO_PIN_1);   // ch11 Env1 attack
    pin_cfg_an(GPIOC, LL_GPIO_PIN_2);   // ch12 Env1 
    pin_cfg_an(GPIOC, LL_GPIO_PIN_3);   // ch13 Env1
    pin_cfg_an(GPIOB, LL_GPIO_PIN_0);   // ch8
    pin_cfg_an(GPIOA, LL_GPIO_PIN_4);   // ch4
    pin_cfg_an(GPIOB, LL_GPIO_PIN_1);   // ch9
    pin_cfg_an(GPIOC, LL_GPIO_PIN_4);   // ch14
    pin_cfg_an(GPIOA, LL_GPIO_PIN_2);   // ch2
    pin_cfg_an(GPIOA, LL_GPIO_PIN_3);   // ch3



    __HAL_RCC_ADC1_CLK_ENABLE();
    __HAL_RCC_DMA2_CLK_ENABLE();

    LL_ADC_CommonInitTypeDef adc_common;
    adc_common.CommonClock = LL_ADC_CLOCK_SYNC_PCLK_DIV2;
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
    reg.SequencerLength = LL_ADC_REG_SEQ_SCAN_ENABLE_12RANKS;
    reg.SequencerDiscont = LL_ADC_REG_SEQ_DISCONT_DISABLE;
    reg.ContinuousMode = LL_ADC_REG_CONV_CONTINUOUS;
    reg.DMATransfer = LL_ADC_REG_DMA_TRANSFER_UNLIMITED;
    LL_ADC_REG_Init(ADC1, &reg);

    LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_1, LL_ADC_CHANNEL_0);
    LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_2, LL_ADC_CHANNEL_1);
    LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_3, LL_ADC_CHANNEL_10);
    LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_4, LL_ADC_CHANNEL_11);
    LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_5, LL_ADC_CHANNEL_12);
    LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_6, LL_ADC_CHANNEL_13);
    LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_7, LL_ADC_CHANNEL_8);
    LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_8, LL_ADC_CHANNEL_4);
    LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_9, LL_ADC_CHANNEL_9);
    LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_10, LL_ADC_CHANNEL_14);
    LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_11, LL_ADC_CHANNEL_2);
    LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_12, LL_ADC_CHANNEL_3);    

    LL_ADC_SetChannelSamplingTime(ADC1, LL_ADC_CHANNEL_0, LL_ADC_SAMPLINGTIME_480CYCLES);
    LL_ADC_SetChannelSamplingTime(ADC1, LL_ADC_CHANNEL_1, LL_ADC_SAMPLINGTIME_480CYCLES);
    LL_ADC_SetChannelSamplingTime(ADC1, LL_ADC_CHANNEL_10, LL_ADC_SAMPLINGTIME_480CYCLES);
    LL_ADC_SetChannelSamplingTime(ADC1, LL_ADC_CHANNEL_11, LL_ADC_SAMPLINGTIME_480CYCLES);
    LL_ADC_SetChannelSamplingTime(ADC1, LL_ADC_CHANNEL_12, LL_ADC_SAMPLINGTIME_480CYCLES);
    LL_ADC_SetChannelSamplingTime(ADC1, LL_ADC_CHANNEL_13, LL_ADC_SAMPLINGTIME_480CYCLES);
    LL_ADC_SetChannelSamplingTime(ADC1, LL_ADC_CHANNEL_8, LL_ADC_SAMPLINGTIME_480CYCLES);
    LL_ADC_SetChannelSamplingTime(ADC1, LL_ADC_CHANNEL_4, LL_ADC_SAMPLINGTIME_480CYCLES);
    LL_ADC_SetChannelSamplingTime(ADC1, LL_ADC_CHANNEL_9, LL_ADC_SAMPLINGTIME_480CYCLES);
    LL_ADC_SetChannelSamplingTime(ADC1, LL_ADC_CHANNEL_14, LL_ADC_SAMPLINGTIME_480CYCLES);
    LL_ADC_SetChannelSamplingTime(ADC1, LL_ADC_CHANNEL_2, LL_ADC_SAMPLINGTIME_480CYCLES);
    LL_ADC_SetChannelSamplingTime(ADC1, LL_ADC_CHANNEL_3, LL_ADC_SAMPLINGTIME_480CYCLES);

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

