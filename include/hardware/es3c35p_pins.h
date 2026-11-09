#ifndef ES3C35P_PINS_H
#define ES3C35P_PINS_H

/*
 * Pinagem da placa ES3C35P (ESP32-S3 N16R8, tela 3.5" 320x480).
 * Validada na bancada (teste-tela-st77922). Detalhes: docs/pinagem_es3c35p.html
 *
 * Ainda não incluído pelo firmware: o perfil ativo de hardware_hal.h
 * continua sendo o antigo até a fase F3.
 */

/* Tela ST77922 (QSPI). RST da tela = EN do ESP32. */
#define ES3C35P_LCD_CS      10
#define ES3C35P_LCD_CLK     12
#define ES3C35P_LCD_D0      11
#define ES3C35P_LCD_D1      13
#define ES3C35P_LCD_D2      14
#define ES3C35P_LCD_D3      9
#define ES3C35P_LCD_BL      41   /* alto liga */

/* Touch capacitivo, I2C0. Compartilhado com o codec de áudio ES8311. */
#define ES3C35P_TOUCH_SDA   38
#define ES3C35P_TOUCH_SCL   39
#define ES3C35P_TOUCH_INT   47   /* não confiável: leitura por polling */
#define ES3C35P_TOUCH_RST   48   /* ativo em nível baixo */
#define ES3C35P_TOUCH_ADDR  0x55

/* Cartão microSD, SDIO 4 bits */
#define ES3C35P_SD_CLK      5
#define ES3C35P_SD_CMD      4
#define ES3C35P_SD_D0       6
#define ES3C35P_SD_D1       7
#define ES3C35P_SD_D2       2
#define ES3C35P_SD_D3       3

/* Áudio I2S (ainda não testado) */
#define ES3C35P_AUDIO_EN    1    /* ativo em nível baixo */
#define ES3C35P_I2S_MCLK    17
#define ES3C35P_I2S_BCLK    18
#define ES3C35P_I2S_LRCK    21
#define ES3C35P_I2S_DOUT    15
#define ES3C35P_I2S_DIN     16

/* Outros */
#define ES3C35P_RGB_LED     40
#define ES3C35P_BOOT        0
#define ES3C35P_BATT_ADC    8

#endif
