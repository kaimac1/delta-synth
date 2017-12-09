#include "board.h"
#include "display.h"
#include "stm32f4xx_ll_gpio.h"
#include "stm32f4xx_ll_spi.h"

void display_reset(void);

#define DISPLAY_SPI SPI1
#define SPI_CLK_EN  __HAL_RCC_SPI1_CLK_ENABLE

#define RESET_PORT  GPIOD
#define RESET_PIN   LL_GPIO_PIN_0
#define CS_PORT     GPIOD
#define CS_PIN      LL_GPIO_PIN_1
#define DC_PORT     GPIOD
#define DC_PIN      LL_GPIO_PIN_2
#define SCK_PORT    GPIOB
#define SCK_PIN     LL_GPIO_PIN_3
#define MOSI_PORT   GPIOB
#define MOSI_PIN    LL_GPIO_PIN_5

void display_init(void) {

    __HAL_RCC_GPIOD_CLK_ENABLE();
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
    for (int i=0; i<16; i++) asm("nop");

}

void writeData(uint8_t c) {
    LL_GPIO_SetOutputPin(DC_PORT, DC_PIN);
    DISPLAY_SPI->DR = c;
    for (int i=0; i<16; i++) asm("nop");
} 

void goTo(int x, int y) {
  if ((x >= SSD1351WIDTH) || (y >= SSD1351HEIGHT)) return;
  
  // set x and y coordinate
  writeCommand(SSD1351_CMD_SETCOLUMN);
  writeData(x);
  writeData(SSD1351WIDTH-1);
  writeCommand(SSD1351_CMD_SETROW);
  writeData(y);
  writeData(SSD1351HEIGHT-1);
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

// void Adafruit_SSD1351::fillScreen(uint16_t fillcolor) {
//   fillRect(0, 0, SSD1351WIDTH, SSD1351HEIGHT, fillcolor);
// }

// Draw a filled rectangle with no rotation.
void display_fillrect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t fillcolor) {
    // Bounds check
    if ((x >= SSD1351WIDTH) || (y >= SSD1351HEIGHT))
        return;

    // Y bounds check
    if (y+h > SSD1351HEIGHT)
    {
        h = SSD1351HEIGHT - y - 1;
    }

    // X bounds check
    if (x+w > SSD1351WIDTH)
    {
        w = SSD1351WIDTH - x - 1;
    }
  
    writeCommand(SSD1351_CMD_SETCOLUMN);
    writeData(x);
    writeData(x+w-1);
    writeCommand(SSD1351_CMD_SETROW);
    writeData(y);
    writeData(y+h-1);
    writeCommand(SSD1351_CMD_WRITERAM);  

    LL_GPIO_SetOutputPin(DC_PORT, DC_PIN);

    for (uint16_t i=0; i < w*h; i++) {
        //writeData(fillcolor >> 8);
        //writeData(fillcolor);
        
        DISPLAY_SPI->DR = fillcolor >> 8;
        for (int i=0; i<16; i++) asm("nop");
        DISPLAY_SPI->DR = fillcolor;
        for (int i=0; i<16; i++) asm("nop");
    }
}

void display_write(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t *buffer) {

    writeCommand(SSD1351_CMD_SETCOLUMN);
    writeData(x);
    writeData(x+w-1);
    writeCommand(SSD1351_CMD_SETROW);
    writeData(y);
    writeData(y+h-1);
    writeCommand(SSD1351_CMD_WRITERAM);  

    LL_GPIO_SetOutputPin(DC_PORT, DC_PIN);

    for (uint16_t i=0; i < w*h; i++) {
        uint16_t col = buffer[i];
        DISPLAY_SPI->DR = col;
        for (int i=0; i<16; i++) asm("nop");
        DISPLAY_SPI->DR = col >> 8;
        for (int i=0; i<16; i++) asm("nop");
    }



}

// // Draw a horizontal line ignoring any screen rotation.
// void Adafruit_SSD1351::rawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) {
//   // Bounds check
//   if ((x >= SSD1351WIDTH) || (y >= SSD1351HEIGHT))
//     return;

//   // X bounds check
//   if (x+w > SSD1351WIDTH)
//   {
//     w = SSD1351WIDTH - x - 1;
//   }

//   if (w < 0) return;

//   // set location
//   writeCommand(SSD1351_CMD_SETCOLUMN);
//   writeData(x);
//   writeData(x+w-1);
//   writeCommand(SSD1351_CMD_SETROW);
//   writeData(y);
//   writeData(y);
//   // fill!
//   writeCommand(SSD1351_CMD_WRITERAM);  

//   for (uint16_t i=0; i < w; i++) {
//     writeData(color >> 8);
//     writeData(color);
//   }
// }

// // Draw a vertical line ignoring any screen rotation.
// void Adafruit_SSD1351::rawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) {
//   // Bounds check
//   if ((x >= SSD1351WIDTH) || (y >= SSD1351HEIGHT))
//   return;

//   // X bounds check
//   if (y+h > SSD1351HEIGHT)
//   {
//     h = SSD1351HEIGHT - y - 1;
//   }

//   if (h < 0) return;

//   // set location
//   writeCommand(SSD1351_CMD_SETCOLUMN);
//   writeData(x);
//   writeData(x);
//   writeCommand(SSD1351_CMD_SETROW);
//   writeData(y);
//   writeData(y+h-1);
//   // fill!
//   writeCommand(SSD1351_CMD_WRITERAM);  

//   for (uint16_t i=0; i < h; i++) {
//     writeData(color >> 8);
//     writeData(color);
//   }
// }

// void Adafruit_SSD1351::drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) {
//   // Transform x and y based on current rotation.
//   switch (getRotation()) {
//   case 0:  // No rotation
//     rawFastVLine(x, y, h, color);
//     break;
//   case 1:  // Rotated 90 degrees clockwise.
//     swap(x, y);
//     x = WIDTH - x - h;
//     rawFastHLine(x, y, h, color);
//     break;
//   case 2:  // Rotated 180 degrees clockwise.
//     x = WIDTH - x - 1;
//     y = HEIGHT - y - h;
//     rawFastVLine(x, y, h, color);
//     break;
//   case 3:  // Rotated 270 degrees clockwise.
//     swap(x, y);
//     y = HEIGHT - y - 1;
//     rawFastHLine(x, y, h, color);
//     break;
//   }
// }

// void Adafruit_SSD1351::drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) {
//   // Transform x and y based on current rotation.
//   switch (getRotation()) {
//   case 0:  // No rotation.
//     rawFastHLine(x, y, w, color);
//     break;
//   case 1:  // Rotated 90 degrees clockwise.
//     swap(x, y);
//     x = WIDTH - x - 1;
//     rawFastVLine(x, y, w, color);
//     break;
//   case 2:  // Rotated 180 degrees clockwise.
//     x = WIDTH - x - w;
//     y = HEIGHT - y - 1;
//     rawFastHLine(x, y, w, color);
//     break;
//   case 3:  // Rotated 270 degrees clockwise.
//     swap(x, y);
//     y = HEIGHT - y - w;
//     rawFastVLine(x, y, w, color);
//     break;
//   }
// }

// void Adafruit_SSD1351::drawPixel(int16_t x, int16_t y, uint16_t color)
// {
//   // Transform x and y based on current rotation.
//   switch (getRotation()) {
//   // Case 0: No rotation
//   case 1:  // Rotated 90 degrees clockwise.
//     swap(x, y);
//     x = WIDTH - x - 1;
//     break;
//   case 2:  // Rotated 180 degrees clockwise.
//     x = WIDTH - x - 1;
//     y = HEIGHT - y - 1;
//     break;
//   case 3:  // Rotated 270 degrees clockwise.
//     swap(x, y);
//     y = HEIGHT - y - 1;
//     break;
//   }

//   // Bounds check.
//   if ((x >= SSD1351WIDTH) || (y >= SSD1351HEIGHT)) return;
//   if ((x < 0) || (y < 0)) return;

//   goTo(x, y);
  
//   // setup for data
//   *rsport |= rspinmask;
//   *csport &= ~ cspinmask;
  
//   spiwrite(color >> 8);    
//   spiwrite(color);
  
//   *csport |= cspinmask;
// }

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
