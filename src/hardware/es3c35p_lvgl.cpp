#include "hardware/es3c35p_lvgl.h"

#if defined(ARDUINO_ARCH_ESP32)

#include <lvgl.h>
#include "esp_cache.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "hardware/es3c35p_display.h"

static const char * TAG = "es3c35p-hud";

#define HUD_W       480     // LVGL: paisagem
#define HUD_H       320
#define PANEL_W     320     // painel nativo: retrato
#define PANEL_H     480

// Quadros na PSRAM (8 MB). A tela e enviada inteira de uma vez, como no teste de bancada
// (que funcionou com faixas de largura total). O LVGL redesenha a tela inteira a cada quadro.
static lv_color_t * s_lvgl = NULL;      // 480x320, o que o LVGL desenha
static uint16_t   * s_nativo = NULL;    // 320x480, girado e com byte trocado, o que vai para o SPI
static lv_disp_draw_buf_t s_draw;
static lv_disp_drv_t      s_disp;
static lv_indev_drv_t     s_indev;

// Roda quando o SPI termina o envio: libera o LVGL para o proximo quadro
static void quadroPronto(void * ctx)
{
    lv_disp_flush_ready((lv_disp_drv_t *)ctx);
}

// Logico (lx, ly) -> nativo (x = 319 - ly, y = lx). Conferido em tests/rot_test.py.
static void flush(lv_disp_drv_t * drv, const lv_area_t * a, lv_color_t * cor)
{
    (void)a; (void)cor;                 // full_refresh: a area e a tela inteira, o buffer e s_lvgl
    const uint16_t * origem = (const uint16_t *)s_lvgl;

    for (int ly = 0; ly < HUD_H; ly++) {
        for (int lx = 0; lx < HUD_W; lx++) {
            uint16_t c = origem[ly * HUD_W + lx];
            int x = (PANEL_W - 1) - ly;
            int y = lx;
            s_nativo[y * PANEL_W + x] = (uint16_t)((c >> 8) | (c << 8));   // painel quer o byte alto primeiro
        }
    }
    esp_cache_msync(s_nativo, PANEL_W * PANEL_H * sizeof(uint16_t), ESP_CACHE_MSYNC_FLAG_DIR_C2M);

    if (!es3c35p_display::drawRect(0, 0, PANEL_W, PANEL_H, s_nativo, quadroPronto, drv)) {
        ESP_LOGE(TAG, "envio do quadro falhou");
        lv_disp_flush_ready(drv);       // nao trava o LVGL
    }
}

// Toque ainda nao ligado: sempre "solto"
static void toqueLer(lv_indev_drv_t * drv, lv_indev_data_t * data)
{
    (void)drv;
    data->state = LV_INDEV_STATE_REL;
}

void es3c35p_lvgl_init()
{
    if (!es3c35p_display::init()) {
        ESP_LOGE(TAG, "tela nao inicializada: HUD desligada");
        return;
    }

    s_lvgl = (lv_color_t *)heap_caps_aligned_alloc(64, HUD_W * HUD_H * sizeof(lv_color_t),
                                                   MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    s_nativo = (uint16_t *)heap_caps_aligned_alloc(64, PANEL_W * PANEL_H * sizeof(uint16_t),
                                                   MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!s_lvgl || !s_nativo) {
        ESP_LOGE(TAG, "sem PSRAM para os quadros: HUD desligada");
        return;
    }

    lv_init();
    lv_disp_draw_buf_init(&s_draw, s_lvgl, NULL, HUD_W * HUD_H);

    lv_disp_drv_init(&s_disp);
    s_disp.hor_res = HUD_W;
    s_disp.ver_res = HUD_H;
    s_disp.flush_cb = flush;
    s_disp.draw_buf = &s_draw;
    s_disp.full_refresh = 1;
    lv_disp_drv_register(&s_disp);

    lv_indev_drv_init(&s_indev);
    s_indev.type = LV_INDEV_TYPE_POINTER;
    s_indev.read_cb = toqueLer;
    lv_indev_drv_register(&s_indev);

    ESP_LOGI(TAG, "LVGL ligado: %dx%d, quadro inteiro na PSRAM", HUD_W, HUD_H);
}

#endif  // ARDUINO_ARCH_ESP32
