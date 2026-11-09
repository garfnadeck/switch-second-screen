#include "hardware/es3c35p_lvgl.h"

#if defined(ARDUINO_ARCH_ESP32)

#include <lvgl.h>
#include "esp_log.h"
#include "hardware/es3c35p_display.h"

static const char * TAG = "es3c35p-hud";

#define HUD_W       480     // LVGL: paisagem
#define HUD_H       320
#define PANEL_W     320     // painel nativo: retrato
#define PANEL_H     480
#define BUF_LINHAS  25      // linhas por flush: 480 * 25 * 2 = 24 KB em RAM interna (DMA)

static lv_color_t s_buf[HUD_W * BUF_LINHAS];
static uint16_t   s_girado[HUD_W * BUF_LINHAS];     // area girada e com byte trocado, pronta para o SPI
static lv_disp_draw_buf_t s_draw;
static lv_disp_drv_t      s_disp;
static lv_indev_drv_t     s_indev;

// Roda quando o SPI termina o envio: libera o LVGL para a proxima area
static void flushPronto(void * ctx)
{
    lv_disp_flush_ready((lv_disp_drv_t *)ctx);
}

// Area logica (lx, ly) -> nativa (x = 319 - ly, y = lx). Com i = coluna nativa e j = linha nativa:
// lx = x1 + j, ly = y2 - i. Ver tests/rot_test.py.
static void flush(lv_disp_drv_t * drv, const lv_area_t * a, lv_color_t * cor)
{
    const int lw = a->x2 - a->x1 + 1;
    const int lh = a->y2 - a->y1 + 1;
    const int nx1 = (PANEL_W - 1) - a->y2;
    const int nx2 = (PANEL_W - 1) - a->y1;
    const int ny1 = a->x1;
    const int ny2 = a->x2;
    const int nw = nx2 - nx1 + 1;       // == lh
    const int nh = ny2 - ny1 + 1;       // == lw

    const uint16_t * origem = (const uint16_t *)cor;
    for (int j = 0; j < nh; j++) {
        for (int i = 0; i < nw; i++) {
            uint16_t c = origem[(lh - 1 - i) * lw + j];
            s_girado[j * nw + i] = (uint16_t)((c >> 8) | (c << 8));   // painel quer o byte alto primeiro
        }
    }
    if (!es3c35p_display::drawRect(nx1, ny1, nx2 + 1, ny2 + 1, s_girado, flushPronto, drv)) {
        ESP_LOGE(TAG, "envio da area falhou");
        lv_disp_flush_ready(drv);     // nao trava o LVGL
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

    lv_init();
    lv_disp_draw_buf_init(&s_draw, s_buf, NULL, HUD_W * BUF_LINHAS);

    lv_disp_drv_init(&s_disp);
    s_disp.hor_res = HUD_W;
    s_disp.ver_res = HUD_H;
    s_disp.flush_cb = flush;
    s_disp.draw_buf = &s_draw;
    lv_disp_drv_register(&s_disp);

    lv_indev_drv_init(&s_indev);
    s_indev.type = LV_INDEV_TYPE_POINTER;
    s_indev.read_cb = toqueLer;
    lv_indev_drv_register(&s_indev);

    ESP_LOGI(TAG, "LVGL ligado: %dx%d (girado no envio)", HUD_W, HUD_H);
}

#endif  // ARDUINO_ARCH_ESP32
