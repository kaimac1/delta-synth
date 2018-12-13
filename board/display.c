#include "board.h"
#include "display.h"
#include "stm32f4xx_ll_i2c.h"
#include "stm32f4xx_ll_dma.h"
#include "stm32f4xx_ll_rcc.h"
#include "stm32f4xx_ll_adc.h"

#include "font.h"
#include <string.h>

#define WIDTH   SSD1306_LCDWIDTH
#define HEIGHT  SSD1306_LCDHEIGHT

#define _swap_int16_t(a, b) { int16_t t = a; a = b; b = t; }
#define abs(x) (((x) > 0) ? (x) : (-(x)))

void ssd1306_command(uint8_t byte);
void display_reset(void);

#define DISPLAY_I2C I2C1
#define I2C_CLK_EN  __HAL_RCC_I2C1_CLK_ENABLE
#define DISPLAY_ADDRESS 0x78
#define SDA_PORT    GPIOB
#define SDA_PIN     LL_GPIO_PIN_9
#define SCL_PORT    GPIOB
#define SCL_PIN     LL_GPIO_PIN_8

uint8_t dbuf[1024];
volatile bool display_busy;
uint16_t font_index[FONT_CHARS];
int dma_page = 0;


// Initialise DMA to transfer the given page.
// A page is a 128x8 row of pixels - there are 8 pages.
void dma_setup(int page) {

    const int page_size = 128;
    uint32_t addr = (uint32_t)&dbuf + page_size*page;

    LL_DMA_InitTypeDef ddma;
    ddma.PeriphOrM2MSrcAddress  = (uint32_t)&(DISPLAY_I2C->DR);
    ddma.MemoryOrM2MDstAddress  = addr;
    ddma.Direction              = LL_DMA_DIRECTION_MEMORY_TO_PERIPH;
    ddma.Mode                   = LL_DMA_MODE_NORMAL;
    ddma.PeriphOrM2MSrcIncMode  = LL_DMA_PERIPH_NOINCREMENT;
    ddma.MemoryOrM2MDstIncMode  = LL_DMA_MEMORY_INCREMENT;
    ddma.PeriphOrM2MSrcDataSize = LL_DMA_PDATAALIGN_BYTE;
    ddma.MemoryOrM2MDstDataSize = LL_DMA_MDATAALIGN_BYTE;
    ddma.NbData                 = page_size;
    ddma.Channel                = LL_DMA_CHANNEL_1;
    ddma.Priority               = LL_DMA_PRIORITY_LOW;
    ddma.FIFOMode               = LL_DMA_FIFOMODE_DISABLE;
    ddma.FIFOThreshold          = LL_DMA_FIFOTHRESHOLD_1_4;
    ddma.MemBurst               = LL_DMA_MBURST_SINGLE;
    ddma.PeriphBurst            = LL_DMA_PBURST_SINGLE;
    LL_DMA_Init(DMA1, LL_DMA_STREAM_6, &ddma);

}

// Start a DMA transfer of the given page.
void start_page_write(int page) {

    dma_setup(page);

    // Set page
    ssd1306_command(0xB0 | page);
    ssd1306_command(0x02);
    ssd1306_command(0x10);

    // I2C page data preamble
    while(LL_I2C_IsActiveFlag_BUSY(DISPLAY_I2C));

    LL_I2C_GenerateStartCondition(DISPLAY_I2C);
    while(!LL_I2C_IsActiveFlag_SB(DISPLAY_I2C));

    LL_I2C_TransmitData8(DISPLAY_I2C, DISPLAY_ADDRESS);
    while(!LL_I2C_IsActiveFlag_ADDR(DISPLAY_I2C));

    LL_I2C_ClearFlag_ADDR(DISPLAY_I2C);
    while(!LL_I2C_IsActiveFlag_TXE(DISPLAY_I2C));

    LL_I2C_TransmitData8(DISPLAY_I2C, 0x40);
    while(!LL_I2C_IsActiveFlag_TXE(DISPLAY_I2C));  

    // Start
    LL_DMA_EnableStream(DMA1, LL_DMA_STREAM_6);

}

void display_init(void) {

    __HAL_RCC_DMA1_CLK_ENABLE();

    // Pin init
    __HAL_RCC_GPIOB_CLK_ENABLE();
    pin_cfg_i2c(SDA_PORT, SDA_PIN);
    pin_cfg_i2c(SCL_PORT, SCL_PIN);        

    // I2C init
    I2C_CLK_EN();
    LL_I2C_Disable(DISPLAY_I2C);
    LL_RCC_ClocksTypeDef rcc_clocks;
    LL_RCC_GetSystemClocksFreq(&rcc_clocks);
    LL_I2C_ConfigSpeed(I2C1, rcc_clocks.PCLK1_Frequency, 1000000, LL_I2C_DUTYCYCLE_2);    
    LL_I2C_Enable(DISPLAY_I2C);

    // DMA init
    HAL_NVIC_SetPriority(DMA1_Stream6_IRQn, PRIORITY_DISPLAY, 0);
    HAL_NVIC_EnableIRQ(DMA1_Stream6_IRQn);
    dma_setup(0);
    LL_DMA_EnableIT_TC(DMA1, LL_DMA_STREAM_6);
    LL_I2C_EnableDMAReq_TX(DISPLAY_I2C);

    display_reset();

}

void ssd1306_command(uint8_t byte) {

    while(LL_I2C_IsActiveFlag_BUSY(DISPLAY_I2C));

    LL_I2C_GenerateStartCondition(DISPLAY_I2C);
    while(!LL_I2C_IsActiveFlag_SB(DISPLAY_I2C));

    LL_I2C_TransmitData8(DISPLAY_I2C, DISPLAY_ADDRESS);
    while(!LL_I2C_IsActiveFlag_ADDR(DISPLAY_I2C));

    LL_I2C_ClearFlag_ADDR(DISPLAY_I2C);
    while(!LL_I2C_IsActiveFlag_TXE(DISPLAY_I2C));

    LL_I2C_TransmitData8(DISPLAY_I2C, 0x00);
    while(!LL_I2C_IsActiveFlag_TXE(DISPLAY_I2C));        

    LL_I2C_TransmitData8(DISPLAY_I2C, byte);
    while(!LL_I2C_IsActiveFlag_TXE(DISPLAY_I2C));

    LL_I2C_GenerateStopCondition(DISPLAY_I2C);

}

// Start a DMA transfer of the buffer to the display.
// Returns false if a transfer is already in progress
bool display_draw(void) {

    if (display_busy) return false;
    display_busy = true;

    dma_page = 0;
    start_page_write(dma_page);

    return true;
}

// IRQ on transfer completion
void DMA1_Stream6_IRQHandler(void) {

    LL_DMA_DisableStream(DMA1, LL_DMA_STREAM_6);
    LL_DMA_ClearFlag_HT6(DMA1);
    LL_DMA_ClearFlag_TC6(DMA1);
    LL_DMA_ClearFlag_TE6(DMA1);
    
    LL_I2C_GenerateStopCondition(DISPLAY_I2C);

    dma_page++;
    if (dma_page == 8) {
        // Last page - finished
        dma_page = 0;
        display_busy = false;
    } else {
        // Start the next page
        start_page_write(dma_page);
    }

}





void draw_pixel(uint16_t x, uint16_t y, bool col) {

    int idx = WIDTH * (y/8) + x;
    if (col) {
        dbuf[idx] |= (1 << (y % 8));
    } else {
        dbuf[idx] &= ~(1 << (y % 8));
    }

}


void build_font_index(void) {

    uint16_t idx = 0;

    for (int i=0; i<FONT_CHARS; i++) {
        font_index[i] = idx;
        idx += font_widths[i]*2;
    }

}

void draw_text_cen(uint16_t x, uint16_t y, char* text, bool inv) {

    uint16_t xlen = 0;
    int len = strlen(text);

    for (int i=0; i<len; i++) {
        uint8_t c = text[i] - FONT_FIRST_CHAR;
        xlen += font_widths[c] + 1;
    }

    draw_text(x-xlen/2, y, text, inv);

}

void draw_text_rj(uint16_t x, uint16_t y, char* text, bool inv) {

    uint16_t xlen = 0;
    int len = strlen(text);

    for (int i=0; i<len; i++) {
        uint8_t c = text[i] - FONT_FIRST_CHAR;
        xlen += font_widths[c] + 1;
    }

    draw_text(x-xlen, y, text, inv);

}

void draw_text(uint16_t x, uint16_t y, char* text, bool inv) {

    uint16_t xoffs = x;
    int len = strlen(text);

    for (int i=0; i<len; i++) {

        uint8_t c = text[i] - FONT_FIRST_CHAR;
        uint8_t char_width = font_widths[c];

        for (int px=0; px<char_width; px++) {

            // top 8 pixels
            uint8_t data = font_data[font_index[c] + px];
            for (int py=0; py<FONT_HEIGHT; py++) {
                uint16_t col = data & (1<<py);
                if (inv) col = !col;
                draw_pixel(xoffs+px, y+py, col);
            }

            // bottom 4
            data = font_data[font_index[c] + char_width + px];
            for (int py=0; py<4; py++) {
                uint16_t col = data & (0x10<<py);
                if (inv) col = !col;
                draw_pixel(xoffs+px, y+py+8, col);
            }
        }

        xoffs += char_width + 1; // 1px space between characters

    }
}

void draw_box(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {

    w -= 1;
    h -= 1;
    draw_line(x, y, x+w, y, 1);
    draw_line(x, y, x, y+h, 1);
    draw_line(x, y+h, x+w, y+h, 1);
    draw_line(x+w, y, x+w, y+h, 1);

}


void draw_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, bool fillcolor) {

    if ((x >= WIDTH) || (y >= HEIGHT)) return;
    if (y+h > HEIGHT) {
        h = HEIGHT - y - 1;
    }
    if (x+w > WIDTH) {
        w = WIDTH - x - 1;
    }

    for (int xp=x; xp<x+w; xp++) {
        for (int yp=y; yp<y+h; yp++) {
            draw_pixel(xp, yp, fillcolor);
        }
    }
}


// From Adafruit GFX library
void draw_line(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, bool col) {

    int16_t steep = abs(y1 - y0) > abs(x1 - x0);
    if (steep) {
        _swap_int16_t(x0, y0);
        _swap_int16_t(x1, y1);
    }

    if (x0 > x1) {
        _swap_int16_t(x0, x1);
        _swap_int16_t(y0, y1);
    }

    int16_t dx, dy;
    dx = x1 - x0;
    dy = abs(y1 - y0);

    int16_t err = dx / 2;
    int16_t ystep;

    if (y0 < y1) {
        ystep = 1;
    } else {
        ystep = -1;
    }

    for (; x0<=x1; x0++) {
        if (steep) {
            draw_pixel(y0, x0, col);
        } else {
            draw_pixel(x0, y0, col);
        }
        err -= dy;
        if (err < 0) {
            y0 += ystep;
            err += dx;
        }
    }
}

void draw_image(uint16_t x, uint16_t y, uint8_t *array, uint16_t w, uint16_t h, bool invert) {

    for (int px=0; px<w; px++) {
        for (int py=0; py<h; py++) {
            if (invert) {
                draw_pixel(x+px, y+py, !array[py*w+px]);
            } else {
                draw_pixel(x+px, y+py, array[py*w+px]);
            }
            
        }
    }
}


void display_reset(void) {

    // Required at power-on
    HAL_Delay(100);

    ssd1306_command(SSD1306_DISPLAYOFF);                    // 0xAE
    ssd1306_command(SSD1306_SETDISPLAYCLOCKDIV);            // 0xD5
    ssd1306_command(0x80);                                  // the suggested ratio 0x80
    ssd1306_command(SSD1306_SETMULTIPLEX);                  // 0xA8
    ssd1306_command(SSD1306_LCDHEIGHT - 1);
    ssd1306_command(SSD1306_SETDISPLAYOFFSET);              // 0xD3
    ssd1306_command(0x0);                                   // no offset
    ssd1306_command(SSD1306_SETSTARTLINE | 0x0);            // line #0
    ssd1306_command(SSD1306_CHARGEPUMP);                    // 0x8D
    ssd1306_command(0x14);
    ssd1306_command(SSD1306_MEMORYMODE);                    // 0x20
    //ssd1306_command(0x00);                                  // 0x0 act like ks0108
    ssd1306_command(0x10); // White display
    ssd1306_command(SSD1306_SEGREMAP | 0x1);
    ssd1306_command(SSD1306_COMSCANDEC);
    ssd1306_command(SSD1306_SETCOMPINS);                    // 0xDA
    ssd1306_command(0x12);
    ssd1306_command(SSD1306_SETCONTRAST);                   // 0x81
    ssd1306_command(0xCF);
    ssd1306_command(SSD1306_SETPRECHARGE);                  // 0xd9
    ssd1306_command(0xF1);
    ssd1306_command(SSD1306_SETVCOMDETECT);                 // 0xDB
    ssd1306_command(0x40);
    ssd1306_command(SSD1306_DISPLAYALLON_RESUME);           // 0xA4
    ssd1306_command(SSD1306_NORMALDISPLAY);                 // 0xA6
    ssd1306_command(SSD1306_DEACTIVATE_SCROLL);
    ssd1306_command(SSD1306_DISPLAYON);//--turn on oled panel   

}
