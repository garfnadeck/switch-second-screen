#include "hardware/es3c35p_lvgl.h"

#if defined(ARDUINO_ARCH_ESP32)

#include <lvgl.h>
#include "esp_cache.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "hardware/es3c35p_display.h"
#include "hardware/es3c35p_touch.h"

static const char * TAG = "es3c35p-hud";

#define HUD_W       480     // LVGL: paisagem
#define HUD_H       320
#define PANEL_W     320     // painel nativo: retrato
#define PANEL_H     480
#define BANDA       40      // linhas por envio: 320 * 40 * 2 = 25600 bytes = max_transfer_sz do barramento

// Quadros na PSRAM (8 MB). A tela e enviada inteira de uma vez, como no teste de bancada
// (que funcionou com faixas de largura total). O LVGL redesenha a tela inteira a cada quadro.
static lv_color_t * s_lvgl = NULL;      // 480x320, o que o LVGL desenha
static uint16_t   * s_nativo = NULL;    // 320x480, girado e com byte trocado, o que vai para o SPI
static lv_disp_draw_buf_t s_draw;
static lv_disp_drv_t      s_disp;
static lv_indev_drv_t     s_indev;
static SemaphoreHandle_t  s_sem = NULL;   // sinaliza o fim de cada faixa enviada pelo SPI

// Roda quando o SPI termina uma faixa (contexto de interrupcao): so sinaliza
static void bandaPronta(void * ctx)
{
    (void)ctx;
    BaseType_t acordar = pdFALSE;
    xSemaphoreGiveFromISR(s_sem, &acordar);
    if (acordar == pdTRUE) portYIELD_FROM_ISR();
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

    // Faixas de 40 linhas, uma de cada vez: espera o SPI terminar cada uma
    for (int y = 0; y < PANEL_H; y += BANDA) {
        if (!es3c35p_display::drawRect(0, y, PANEL_W, y + BANDA, s_nativo + y * PANEL_W, bandaPronta, NULL)) {
            ESP_LOGE(TAG, "envio da faixa %d falhou", y);
            break;
        }
        if (xSemaphoreTake(s_sem, pdMS_TO_TICKS(200)) != pdTRUE) {
            ESP_LOGE(TAG, "faixa %d nao terminou a tempo", y);
            break;
        }
    }
    lv_disp_flush_ready(drv);           // libera o LVGL mesmo se algo falhou (nao trava)
}

// Toque: nativo (x, y) -> logico (lx = y, ly = 319 - x), inverso da rotacao do envio
static void toqueLer(lv_indev_drv_t * drv, lv_indev_data_t * data)
{
    (void)drv;
    uint16_t tx = 0, ty = 0;
    if (es3c35p_touch::read(&tx, &ty)) {
        int lx = ty;
        int ly = (PANEL_W - 1) - tx;
        if (lx < 0) lx = 0;
        if (lx >= HUD_W) lx = HUD_W - 1;
        if (ly < 0) ly = 0;
        if (ly >= HUD_H) ly = HUD_H - 1;
        data->point.x = lx;
        data->point.y = ly;
        data->state = LV_INDEV_STATE_PR;
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
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

    s_sem = xSemaphoreCreateBinary();
    if (!s_sem) {
        ESP_LOGE(TAG, "sem memoria para o semaforo: HUD desligada");
        return;
    }

    if (!es3c35p_touch::init()) {
        ESP_LOGW(TAG, "touch nao iniciado: HUD sem toque");
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
