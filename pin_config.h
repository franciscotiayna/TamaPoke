#pragma once

// Pines oficiales de la Waveshare ESP32-S3-Touch-AMOLED-1.75C
// Fuente: github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.75C
// (examples/arduino/libraries/board_config/pin_config.h + esquematico).
// Diferencias con la 1.75 original: LCD_RESET 39->1, TP_RESET 40->2, MCLK 42->16,
// sin ranura microSD (los sprites van en la particion FAT de la flash interna)
// y sin RTC PCF85063 (ver rtcbat.cpp). GPIO3 es SYS_OUT (tecla PWR): no usar.

#define XPOWERS_CHIP_AXP2101

// Pantalla AMOLED 466x466, driver CO5300 por QSPI
#define LCD_SDIO0 4
#define LCD_SDIO1 5
#define LCD_SDIO2 6
#define LCD_SDIO3 7
#define LCD_SCLK 38
#define LCD_CS 12
#define LCD_RESET 1
#define LCD_WIDTH 466
#define LCD_HEIGHT 466

// Táctil capacitivo CST9217 por I2C
#define IIC_SDA 15
#define IIC_SCL 14
#define TP_INT 11
#define TP_RESET 2

// Audio ES8311 (DAC) + ES7210 (micros, no usado). Bus I2S compartido.
#define I2S_MCK_IO 16
#define I2S_BCK_IO 9
#define I2S_DI_IO 10
#define I2S_WS_IO 45
#define I2S_DO_IO 8
#define PA 46

// Tecla PWR vista por el ESP32 (SYS_OUT, via el transistor T1): despierta de la
// suspension profunda. Pulsada = 1 con pull-up interno: R9 no viene montada (ver suspend()).
#define PWR_KEY_IO 3
