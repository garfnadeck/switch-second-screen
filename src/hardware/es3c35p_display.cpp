#include "hardware/es3c35p_display.h"

#if defined(ARDUINO_ARCH_ESP32)

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_cache.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_st77922.h"

#include "hardware/es3c35p_pins.h"
#include "hardware/st77922_init_table.h"

using es3c35p_display::DrawDoneFn;

static const char * TAG = "es3c35p-lcd";

#define LCD_HOST        SPI2_HOST
#define LCD_H_RES       320
#define LCD_V_RES       480
#define LCD_PCLK_HZ     (80 * 1000 * 1000)  // 80 MHz, igual ao exemplo do fabricante
#define BAND_ROWS       40                  // linhas por transferencia: 320 * 40 * 2 = 25 KB

#define COR_FUNDO       0xFFFF              // branco
#define COR_TEXTO       0x0000              // preto
#define TEXTO_ESCALA    10                  // cada pixel da fonte vira 10x10

static esp_lcd_panel_handle_t s_panel = NULL;
static uint16_t * s_quadro = NULL;          // quadro inteiro na PSRAM, nunca liberado (DMA assincrono)
static DrawDoneFn s_done = NULL;            // quem espera o fim do envio (ex.: flush do LVGL)
static void * s_done_ctx = NULL;

// Roda quando um envio de cor termina no SPI (contexto de interrupcao)
static bool onColorTransDone(esp_lcd_panel_io_handle_t io, esp_lcd_panel_io_event_data_t * edata, void * user_ctx)
{
    (void)io; (void)edata; (void)user_ctx;
    if (s_done) {
        DrawDoneFn f = s_done;
        s_done = NULL;
        f(s_done_ctx);
    }
    return false;
}

// Glifo 5x7: uma linha por byte, bit 4 = pixel da esquerda.
static uint8_t glifoLinha(char c, int linha)
{
    static const uint8_t T[7] = { 0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04 };
    static const uint8_t E[7] = { 0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F };
    static const uint8_t S[7] = { 0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E };
    switch (c) {
        case 'T': return T[linha];
        case 'E': return E[linha];
        case 'S': return S[linha];
        default:  return 0;
    }
}

// Retorna 1 se o pixel (x, y) faz parte do texto centralizado.
static int pixelDoTexto(const char * texto, int x, int y)
{
    const int n = (int)strlen(texto);
    const int avanco = 6 * TEXTO_ESCALA;
    const int largura = n * avanco - TEXTO_ESCALA;
    const int altura = 7 * TEXTO_ESCALA;
    const int x0 = (LCD_H_RES - largura) / 2;
    const int y0 = (LCD_V_RES - altura) / 2;

    if (n == 0 || x < x0 || y < y0 || x >= x0 + largura || y >= y0 + altura) return 0;

    int dx = x - x0;
    int dy = y - y0;
    int cx = dx % avanco;
    if (cx >= 5 * TEXTO_ESCALA) return 0;

    int letra = dx / avanco;
    int coluna = cx / TEXTO_ESCALA;
    int linha = dy / TEXTO_ESCALA;
    return (glifoLinha(texto[letra], linha) >> (4 - coluna)) & 1;
}

namespace es3c35p_display {

bool init()
{
    if (s_panel) return true;

    gpio_config_t bl = { .pin_bit_mask = 1ULL << ES3C35P_LCD_BL, .mode = GPIO_MODE_OUTPUT };
    gpio_config(&bl);
    gpio_set_level((gpio_num_t)ES3C35P_LCD_BL, 1);
    ESP_LOGI(TAG, "Backlight ligado (IO%d)", ES3C35P_LCD_BL);

    spi_bus_config_t bus = {};
    bus.sclk_io_num = ES3C35P_LCD_CLK;
    bus.data0_io_num = ES3C35P_LCD_D0;
    bus.data1_io_num = ES3C35P_LCD_D1;
    bus.data2_io_num = ES3C35P_LCD_D2;
    bus.data3_io_num = ES3C35P_LCD_D3;
    bus.max_transfer_sz = LCD_H_RES * BAND_ROWS * 2;
    if (spi_bus_initialize(LCD_HOST, &bus, SPI_DMA_CH_AUTO) != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao iniciar o barramento SPI");
        return false;
    }

    esp_lcd_panel_io_handle_t io = NULL;
    esp_lcd_panel_io_spi_config_t io_cfg = {};
    io_cfg.cs_gpio_num = ES3C35P_LCD_CS;
    io_cfg.dc_gpio_num = -1;                // QSPI nao usa DC
    io_cfg.spi_mode = 0;
    io_cfg.pclk_hz = LCD_PCLK_HZ;
    io_cfg.trans_queue_depth = 10;
    io_cfg.lcd_cmd_bits = 32;
    io_cfg.lcd_param_bits = 8;
    io_cfg.flags.quad_mode = 1;
    io_cfg.on_color_trans_done = onColorTransDone;
    if (esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_cfg, &io) != ESP_OK) return false;

    st77922_vendor_config_t vendor = {};
    vendor.init_cmds = ES3C35P_INIT_CMDS;
    vendor.init_cmds_size = ES3C35P_INIT_CMDS_COUNT;
    vendor.flags.use_qspi_interface = 1;
#if defined(ES3C35P_ROT_HW)
    vendor.flags.supports_madctl_mv = 1;   // rotacao pelo proprio painel (MADCTL.MV), sem girar no ESP32
#endif

    esp_lcd_panel_dev_config_t panel_cfg = {};
    panel_cfg.reset_gpio_num = -1;          // RST da tela esta no EN do ESP32
    // BGR: no teste de 09/11 o ciano do texto saiu amarelo-esverdeado (R e B trocados), entao o painel usa BGR
    panel_cfg.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR;
    panel_cfg.bits_per_pixel = 16;
    panel_cfg.vendor_config = &vendor;

    if (esp_lcd_new_panel_st77922(io, &panel_cfg, &s_panel) != ESP_OK) return false;
    esp_lcd_panel_reset(s_panel);
    esp_lcd_panel_init(s_panel);
    vTaskDelay(pdMS_TO_TICKS(200));
    esp_lcd_panel_disp_on_off(s_panel, true);
#if defined(ES3C35P_ROT_HW)
    // Painel em paisagem (480x320): troca X/Y e espelha conforme os defines (ajustar na placa se a imagem vier invertida)
    #ifndef ES3C35P_MIRROR_X
        #define ES3C35P_MIRROR_X 0
    #endif
    #ifndef ES3C35P_MIRROR_Y
        #define ES3C35P_MIRROR_Y 0
    #endif
    esp_lcd_panel_swap_xy(s_panel, true);
    esp_lcd_panel_mirror(s_panel, ES3C35P_MIRROR_X, ES3C35P_MIRROR_Y);
#endif

    // Diagnostico: RDDID zerado indica que o painel nao responde ao QSPI
    uint8_t rddid[3] = { 0 };
    esp_err_t err = esp_lcd_panel_io_rx_param(io, 0x04, rddid, sizeof(rddid));
    ESP_LOGI(TAG, "RDDID: err=%s  %02X %02X %02X", esp_err_to_name(err), rddid[0], rddid[1], rddid[2]);
    ESP_LOGI(TAG, "Tela inicializada (%dx%d)", LCD_H_RES, LCD_V_RES);
    return true;
}

bool drawRect(int x1, int y1, int x2, int y2, const void * pixels, DrawDoneFn done, void * ctx)
{
    if (!s_panel) return false;
    s_done = done;
    s_done_ctx = ctx;
    esp_err_t err = esp_lcd_panel_draw_bitmap(s_panel, x1, y1, x2, y2, pixels);
    if (err != ESP_OK) {
        s_done = NULL;
        ESP_LOGE(TAG, "draw_bitmap falhou: %s", esp_err_to_name(err));
        return false;
    }
    return true;
}

bool drawTestText(const char * texto)
{
    if (!s_panel) return false;
    const size_t total = LCD_H_RES * LCD_V_RES * sizeof(uint16_t);

    if (!s_quadro) {
        s_quadro = (uint16_t *)heap_caps_aligned_alloc(64, total, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!s_quadro) {
            ESP_LOGE(TAG, "Sem memoria (PSRAM) para o quadro");
            return false;
        }
    }

    for (int i = 0; i < LCD_H_RES * LCD_V_RES; i++) {
        int x = i % LCD_H_RES;
        int y = i / LCD_H_RES;
        uint16_t c = pixelDoTexto(texto, x, y) ? COR_TEXTO : COR_FUNDO;
        s_quadro[i] = (uint16_t)((c >> 8) | (c << 8));   // ESP32 little-endian; o painel quer o byte alto primeiro
    }
    esp_cache_msync(s_quadro, total, ESP_CACHE_MSYNC_FLAG_DIR_C2M);   // DMA le direto da PSRAM

    for (int y = 0; y < LCD_V_RES; y += BAND_ROWS) {
        esp_lcd_panel_draw_bitmap(s_panel, 0, y, LCD_H_RES, y + BAND_ROWS, s_quadro + y * LCD_H_RES);
    }
    ESP_LOGI(TAG, "Texto \"%s\" enviado", texto);
    return true;
}

}  // namespace es3c35p_display

#else  // fora do ESP32: sem tela

namespace es3c35p_display {
bool init() { return false; }
bool drawTestText(const char *) { return false; }
bool drawRect(int, int, int, int, const void *, DrawDoneFn, void *) { return false; }
}  // namespace es3c35p_display

#endif
