/*
 * Teste de bancada: tela ST77922 320x480 por QSPI, com backlight e cores de teste.
 *
 * Pinagem (referência ES3C35P, docs/PINAGEM_TELA_ES3C35P.md):
 *   CS IO10 | CLK IO12 | D0 IO11 | D1 IO13 | D2 IO14 | D3 IO9
 *   RST: EN (sem GPIO, usa reset por software)
 *   BL:  IO41 (alto liga)
 *
 * Esperado no monitor serial: "PSRAM encontrada", "Tela inicializada" e
 * a tela alternando entre vermelho, verde, azul e branco a cada 2 s.
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_st77922.h"

#define TAG "bringup"

#define LCD_HOST      SPI2_HOST
#define PIN_CS        10
#define PIN_CLK       12
#define PIN_D0        11
#define PIN_D1        13
#define PIN_D2        14
#define PIN_D3        9
#define PIN_BL        41
#define LCD_H_RES     320
#define LCD_V_RES     480
#define LCD_PCLK_HZ   (2 * 1000 * 1000)    /* 2 MHz: diagnóstico de ruído na imagem; subir depois se estável */
#define BAND_ROWS     40   /* linhas por transferência: 320 * 40 * 2 bytes = 25 KB */

static void backlight_on(void)
{
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << PIN_BL,
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&io));
    gpio_set_level(PIN_BL, 1);
    ESP_LOGI(TAG, "Backlight ligado (IO%d)", PIN_BL);
}

static void check_psram(void)
{
    size_t total = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    if (total > 0) {
        ESP_LOGI(TAG, "PSRAM encontrada: %u KB", (unsigned)(total / 1024));
    } else {
        ESP_LOGE(TAG, "PSRAM NAO encontrada. Verificar sdkconfig (OCT x QUAD).");
    }
}

static esp_lcd_panel_handle_t lcd_init(void)
{
    spi_bus_config_t bus = {
        .sclk_io_num = PIN_CLK,
        .data0_io_num = PIN_D0,
        .data1_io_num = PIN_D1,
        .data2_io_num = PIN_D2,
        .data3_io_num = PIN_D3,
        .max_transfer_sz = LCD_H_RES * BAND_ROWS * 2,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &bus, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_handle_t io = NULL;
    esp_lcd_panel_io_spi_config_t io_cfg = {
        .cs_gpio_num = PIN_CS,
        .dc_gpio_num = -1,          /* QSPI não usa DC */
        .spi_mode = 0,
        .pclk_hz = LCD_PCLK_HZ,
        .trans_queue_depth = 10,
        .lcd_cmd_bits = 32,         /* comandos QSPI do ST77xxx usam 32 bits */
        .lcd_param_bits = 8,
        .flags = { .quad_mode = 1 },
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_cfg, &io));

    esp_lcd_panel_handle_t panel = NULL;
    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = -1,       /* RST está no EN */
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };
    /* Obrigatório para QSPI: o componente precisa saber que a interface é QSPI.
     * O init do ST77922 é o padrão do componente (sem init_cmds próprios). */
    st77922_vendor_config_t vendor_cfg = {
        .flags = { .use_qspi_interface = 1 },
    };
    panel_cfg.vendor_config = &vendor_cfg;
    ESP_ERROR_CHECK(esp_lcd_new_panel_st77922(io, &panel_cfg, &panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
    /* Pausa para o painel sair do modo de suspensão (o ST77922 precisa de ~120 ms). */
    vTaskDelay(pdMS_TO_TICKS(200));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel, true));

    ESP_LOGI(TAG, "Tela inicializada (%dx%d)", LCD_H_RES, LCD_V_RES);
    return panel;
}

static void fill_color(esp_lcd_panel_handle_t panel, uint16_t color)
{
    /* RAM interna com DMA (não PSRAM): diagnóstico de ruído na parte de baixo da tela. */
    uint16_t *band = heap_caps_malloc(LCD_H_RES * BAND_ROWS * sizeof(uint16_t),
                                      MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
    if (!band) {
        ESP_LOGE(TAG, "Sem memoria para a faixa de cor");
        return;
    }
    for (int i = 0; i < LCD_H_RES * BAND_ROWS; i++) band[i] = color;

    for (int y = 0; y < LCD_V_RES; y += BAND_ROWS) {
        esp_lcd_panel_draw_bitmap(panel, 0, y, LCD_H_RES, y + BAND_ROWS, band);
    }
    heap_caps_free(band);
}

void app_main(void)
{
    ESP_LOGI(TAG, "Iniciando teste da placa ESP32-S3 3.5\"");
    check_psram();
    backlight_on();

    esp_lcd_panel_handle_t panel = lcd_init();

    const uint16_t colors[] = { 0xF800 /* vermelho */, 0x07E0 /* verde */,
                                0x001F /* azul */,     0xFFFF /* branco */ };
    const char *names[]     = { "vermelho", "verde", "azul", "branco" };
    int i = 0;
    while (1) {
        ESP_LOGI(TAG, "Cor: %s", names[i]);
        fill_color(panel, colors[i]);
        i = (i + 1) % 4;
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
