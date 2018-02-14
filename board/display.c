#include "board.h"
#include "display.h"
#include "stm32f4xx_ll_gpio.h"
#include "stm32f4xx_ll_spi.h"

#include "font.h"
#include <string.h>

#define _swap_int16_t(a, b) { int16_t t = a; a = b; b = t; }
#define abs(x) (((x) > 0) ? (x) : (-(x)))

void display_reset(void);

#define DISPLAY_SPI SPI2
#define SPI_CLK_EN  __HAL_RCC_SPI2_CLK_ENABLE

#define RESET_PORT  GPIOB
#define RESET_PIN   LL_GPIO_PIN_10
#define CS_PORT     GPIOB
#define CS_PIN      LL_GPIO_PIN_12
#define DC_PORT     GPIOB
#define DC_PIN      LL_GPIO_PIN_14
#define SCK_PORT    GPIOB
#define SCK_PIN     LL_GPIO_PIN_13
#define MOSI_PORT   GPIOB
#define MOSI_PIN    LL_GPIO_PIN_15

#define NOPS 32 // was 16

uint16_t dbuffer[128][128];

void display_init(void) {

    __HAL_RCC_GPIOB_CLK_ENABLE();
    SPI_CLK_EN();

    pin_cfg_output(RESET_PORT, RESET_PIN);
    pin_cfg_output(CS_PORT, CS_PIN);
    pin_cfg_output(DC_PORT, DC_PIN);
    pin_cfg_af(SCK_PORT, SCK_PIN, 5);
    pin_cfg_af(MOSI_PORT, MOSI_PIN, 5);

    pin_set(RESET_PORT, RESET_PIN, 1);
    pin_set(CS_PORT, CS_PIN, 1);
    pin_set(DC_PORT, DC_PIN, 1);


    LL_SPI_InitTypeDef spi;

    spi.TransferDirection = LL_SPI_FULL_DUPLEX;
    spi.Mode            = LL_SPI_MODE_MASTER;
    spi.DataWidth       = LL_SPI_DATAWIDTH_8BIT;
    spi.ClockPolarity   = LL_SPI_POLARITY_LOW;
    spi.ClockPhase      = LL_SPI_PHASE_1EDGE;
    spi.NSS             = LL_SPI_NSS_SOFT;
    spi.BaudRate        = LL_SPI_BAUDRATEPRESCALER_DIV8;
    spi.BitOrder        = LL_SPI_MSB_FIRST; 
    spi.CRCCalculation  = LL_SPI_CRCCALCULATION_DISABLE;
    spi.CRCPoly         = 0;

    LL_SPI_Init(DISPLAY_SPI, &spi);
    LL_SPI_Enable(DISPLAY_SPI);

    display_reset();

}

void writeCommand(uint8_t c) {
    LL_GPIO_ResetOutputPin(DC_PORT, DC_PIN);
    DISPLAY_SPI->DR = c;
    for (int i=0; i<NOPS; i++) asm("nop");

}

void writeData(uint8_t c) {
    LL_GPIO_SetOutputPin(DC_PORT, DC_PIN);
    DISPLAY_SPI->DR = c;
    for (int i=0; i<NOPS; i++) asm("nop");
} 


void set_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {

    writeCommand(SSD1351_CMD_SETCOLUMN);
    writeData(x);
    writeData(x+w-1);
    writeCommand(SSD1351_CMD_SETROW);
    writeData(y);
    writeData(y+h-1);
    writeCommand(SSD1351_CMD_WRITERAM);

}

// uint16_t Adafruit_SSD1351::Color565(uint8_t r, uint8_t g, uint8_t b) {
//   uint16_t c;
//   c = r >> 3;
//   c <<= 6;
//   c |= g >> 2;
//   c <<= 5;
//   c |= b >> 3;

//   return c;
// }

void draw_pixel(uint16_t x, uint16_t y, uint16_t col) {
    dbuffer[y][x] = col;
}


uint16_t font_index[FONT_CHARS];

void build_font_index(void) {

    uint16_t idx = 0;

    for (int i=0; i<FONT_CHARS; i++) {
        font_index[i] = idx;
        idx += font_widths[i]*2;
    }

}

void draw_text_rj(uint16_t x, uint16_t y, char* text, int size, uint16_t colour) {

    uint16_t xlen = 0;
    int len = strlen(text);

    for (int i=0; i<len; i++) {
        uint8_t c = text[i] - FONT_FIRST_CHAR;
        xlen += (font_widths[c] + 1)*size;
    }

    draw_text(x-xlen, y, text, size, colour);

}

void draw_text(uint16_t x, uint16_t y, char* text, int size, uint16_t colour) {

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
                col = col ? colour : 0;
                if (size == 1) {
                    draw_pixel(xoffs+px, y+py, col);
                } else {
                    for (int sx=0; sx<size; sx++) {
                        for (int sy=0; sy<size; sy++) {
                            draw_pixel(xoffs+px*size+sx, y+py*size+sy, col);
                        }
                    }
                }
            }

            // bottom 4
            data = font_data[font_index[c] + char_width + px];
            for (int py=0; py<4; py++) {
                uint16_t col = data & (0x10<<py);
                col = col ? colour : 0;
                if (size == 1) {
                    draw_pixel(xoffs+px, y+py+8, col);
                } else {
                    for (int sx=0; sx<size; sx++) {
                        for (int sy=0; sy<size; sy++) {
                            draw_pixel(xoffs+px*size+sx, y+(py+8)*size+sy, col);
                        }
                    }
                }
            }
        }

        xoffs += (char_width + 1)*size; // 1px space between characters

    }
}


void draw_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t fillcolor) {

    if ((x >= SSD1351WIDTH) || (y >= SSD1351HEIGHT)) return;
    if (y+h > SSD1351HEIGHT) {
        h = SSD1351HEIGHT - y - 1;
    }
    if (x+w > SSD1351WIDTH) {
        w = SSD1351WIDTH - x - 1;
    }

    for (int xp=x; xp<x+w; xp++) {
        for (int yp=y; yp<y+h; yp++) {
            dbuffer[yp][xp] = fillcolor;
        }
    }
}


// From Adafruit GFX library
void draw_line(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t col) {

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


void display_draw(void) {
    display_write(0, 0, 128, 128, (uint16_t*)dbuffer);
}


void display_write(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t *buffer) {

    set_rect(x, y, w, h);
    LL_GPIO_SetOutputPin(DC_PORT, DC_PIN);

    for (uint16_t i=0; i < w*h; i++) {
        uint16_t col = buffer[i];
        DISPLAY_SPI->DR = col >> 8;
        for (int i=0; i<NOPS; i++) asm("nop");
        DISPLAY_SPI->DR = col & 0xFF;
        for (int i=0; i<NOPS; i++) asm("nop");
    }

}


void display_reset(void) {

    pin_set(CS_PORT, CS_PIN, 0);
    
    pin_set(RESET_PORT, RESET_PIN, 1);
    HAL_Delay(200);
    pin_set(RESET_PORT, RESET_PIN, 0);
    HAL_Delay(200);
    pin_set(RESET_PORT, RESET_PIN, 1);
    HAL_Delay(200);

    // Initialization Sequence
    writeCommand(SSD1351_CMD_COMMANDLOCK);  // set command lock
    writeData(0x12);  
    writeCommand(SSD1351_CMD_COMMANDLOCK);  // set command lock
    writeData(0xB1);

    writeCommand(SSD1351_CMD_DISPLAYOFF);  		// 0xAE

    writeCommand(SSD1351_CMD_CLOCKDIV);  		// 0xB3
    writeCommand(0xF1);  						// 7:4 = Oscillator Frequency, 3:0 = CLK Div Ratio (A[3:0]+1 = 1..16)
    
    writeCommand(SSD1351_CMD_MUXRATIO);
    writeData(127);
    
    writeCommand(SSD1351_CMD_SETREMAP);
    writeData(0x74);
  
    writeCommand(SSD1351_CMD_SETCOLUMN);
    writeData(0x00);
    writeData(0x7F);
    writeCommand(SSD1351_CMD_SETROW);
    writeData(0x00);
    writeData(0x7F);

    writeCommand(SSD1351_CMD_STARTLINE); 		// 0xA1
    if (SSD1351HEIGHT == 96) {
      writeData(96);
    } else {
      writeData(0);
    }


    writeCommand(SSD1351_CMD_DISPLAYOFFSET); 	// 0xA2
    writeData(0x0);

    writeCommand(SSD1351_CMD_SETGPIO);
    writeData(0x00);
    
    writeCommand(SSD1351_CMD_FUNCTIONSELECT);
    writeData(0x01); // internal (diode drop)
    //writeData(0x01); // external bias

   // writeCommand(SSSD1351_CMD_SETPHASELENGTH);
   // writeData(0x32);

    writeCommand(SSD1351_CMD_PRECHARGE);  		// 0xB1
    writeCommand(0x32); // 32
 
    writeCommand(SSD1351_CMD_VCOMH);  			// 0xBE
    writeCommand(0x05);

    writeCommand(SSD1351_CMD_NORMALDISPLAY);  	// 0xA6

    writeCommand(SSD1351_CMD_CONTRASTABC);
    writeData(0xC8);
    writeData(0x80);
    writeData(0xC8);

    writeCommand(SSD1351_CMD_CONTRASTMASTER);
    writeData(0x0F);

    writeCommand(SSD1351_CMD_SETVSL );
    writeData(0xA0);
    writeData(0xB5);
    writeData(0x55);
    
    writeCommand(SSD1351_CMD_PRECHARGE2);
    writeData(0x01);
    
    writeCommand(SSD1351_CMD_DISPLAYON);		//--turn on oled panel    
}
