#include "hardware/es3c35p_lvgl.h"

#if defined(ARDUINO_ARCH_ESP32)

#include <lvgl.h>
#include "esp_cache.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <string.h>
#include <stdio.h>
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

// Diagnostico do toque na propria tela (o serial desta placa nao mostra os logs).
// Rotulo fixo no canto inferior esquerdo, sobre a HUD.
static lv_obj_t * s_dbg = NULL;
static bool       s_estavaPressionado = false;
static bool       s_touchOk = false;
static char       s_toqueTxt[32] = "";      // estado do toque mostrado no rotulo
static char       s_msgMostrada[120] = "";  // ultimo texto colocado no rotulo (evita redesenhar a toa)
// Diagnostico do flush: quantas vezes foi chamado e qual area logica foi enviada por ultimo
static unsigned long s_nFlush = 0;
static unsigned long s_msGira = 0, s_msEnvio = 0;   // tempo (ms) da rotacao e do envio SPI no ultimo flush
static int        s_fx1 = 0, s_fx2 = 0, s_fy1 = 0, s_fy2 = 0;

// Roda quando o SPI termina uma faixa (contexto de interrupcao): so sinaliza
static void bandaPronta(void * ctx)
{
    (void)ctx;
    BaseType_t acordar = pdFALSE;
    xSemaphoreGiveFromISR(s_sem, &acordar);
    if (acordar == pdTRUE) portYIELD_FROM_ISR();
}

#if defined(ES3C35P_ROT_HW)
// Modo de rotacao pelo painel: a area logica vai direto, em faixas de linhas, so trocando a ordem dos bytes
static void flush(lv_disp_drv_t * drv, const lv_area_t * a, lv_color_t * cor)
{
    (void)cor;
    const uint16_t * origem = (const uint16_t *)s_lvgl;
    int w = a->x2 - a->x1 + 1;
    int linhas = 25600 / (w * 2);           // faixas de no maximo 25600 bytes
    if (linhas < 1) linhas = 1;
    if (w <= 0) { lv_disp_flush_ready(drv); return; }

    for (int y0 = a->y1; y0 <= a->y2; y0 += linhas) {
        int n = (y0 + linhas - 1 <= a->y2) ? linhas : (a->y2 - y0 + 1);
        for (int r = 0; r < n; r++) {
            const uint16_t * linha = origem + (y0 + r) * HUD_W + a->x1;
            for (int x = 0; x < w; x++) {
                uint16_t c = linha[x];
                s_nativo[r * w + x] = (uint16_t)((c >> 8) | (c << 8));
            }
        }
        esp_cache_msync(s_nativo, w * n * sizeof(uint16_t), ESP_CACHE_MSYNC_FLAG_DIR_C2M);
        if (!es3c35p_display::drawRect(a->x1, y0, a->x2 + 1, y0 + n, s_nativo, bandaPronta, NULL)) {
            ESP_LOGE(TAG, "envio da faixa falhou");
            break;
        }
        if (xSemaphoreTake(s_sem, pdMS_TO_TICKS(200)) != pdTRUE) {
            ESP_LOGE(TAG, "faixa nao terminou a tempo");
            break;
        }
    }
    lv_disp_flush_ready(drv);
}
#else
// Software: o LVGL entrega so os pixels da area (no modo parcial, com largura igual a da area).
// Gira essa area para o buffer nativo e envia em blocos.
static void flush(lv_disp_drv_t * drv, const lv_area_t * a, lv_color_t * cor)
{
    const uint16_t * src = (const uint16_t *)cor;
    int ax1 = a->x1, ay1 = a->y1, ax2 = a->x2, ay2 = a->y2;
    int aw = ax2 - ax1 + 1;                 // largura da area no buffer do LVGL
    s_nFlush++;
    s_fx1 = ax1; s_fx2 = ax2; s_fy1 = ay1; s_fy2 = ay2;

    // Area logica -> area nativa (inclusiva): nativo x = 319 - ly, nativo y = lx
    int nx1 = (PANEL_W - 1) - ay2, nx2 = (PANEL_W - 1) - ay1;
    int ny1 = ax1,                 ny2 = ax2;
    int w = nx2 - nx1 + 1;
    int h = ny2 - ny1 + 1;
    if (w <= 0 || h <= 0 || w * h > PANEL_W * PANEL_H) { lv_disp_flush_ready(drv); return; }

    // Rotacao em blocos de 16x16 (ordem amigavel ao cache da PSRAM)
    int64_t t0 = esp_timer_get_time();
    const int T = 16;
    for (int by = ny1; by <= ny2; by += T) {
        int yFim = (by + T - 1 < ny2) ? (by + T - 1) : ny2;
        for (int bx = nx1; bx <= nx2; bx += T) {
            int xFim = (bx + T - 1 < nx2) ? (bx + T - 1) : nx2;
            for (int y = by; y <= yFim; y++) {
                for (int x = bx; x <= xFim; x++) {
                    int lx = y;
                    int ly = (PANEL_W - 1) - x;
                    uint16_t c = src[(ly - ay1) * aw + (lx - ax1)];
                    s_nativo[(y - ny1) * w + (x - nx1)] = (uint16_t)((c >> 8) | (c << 8));   // painel quer o byte alto primeiro
                }
            }
        }
    }
    esp_cache_msync(s_nativo, w * h * sizeof(uint16_t), ESP_CACHE_MSYNC_FLAG_DIR_C2M);
    int64_t t1 = esp_timer_get_time();

    // Blocos de no maximo 25600 bytes (limite do barramento SPI), um de cada vez
    int linhasPorBloco = 12800 / w;
    if (linhasPorBloco < 1) linhasPorBloco = 1;
    for (int r = 0; r < h; r += linhasPorBloco) {
        int n = (r + linhasPorBloco > h) ? (h - r) : linhasPorBloco;
        if (!es3c35p_display::drawRect(nx1, ny1 + r, nx2 + 1, ny1 + r + n, s_nativo + r * w, bandaPronta, NULL)) {
            ESP_LOGE(TAG, "envio do bloco falhou");
            break;
        }
        if (xSemaphoreTake(s_sem, pdMS_TO_TICKS(200)) != pdTRUE) {
            ESP_LOGE(TAG, "bloco nao terminou a tempo");
            break;
        }
    }
    s_msGira = (unsigned long)((t1 - t0) / 1000);
    s_msEnvio = (unsigned long)((esp_timer_get_time() - t1) / 1000);
    lv_disp_flush_ready(drv);
}
#endif  // ES3C35P_ROT_HW

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
        snprintf(s_toqueTxt, sizeof(s_toqueTxt), "toque %d,%d", lx, ly);
        s_estavaPressionado = true;
    } else {
        data->state = LV_INDEV_STATE_REL;
        if (s_estavaPressionado) snprintf(s_toqueTxt, sizeof(s_toqueTxt), "solto");
        s_estavaPressionado = false;
    }

    // Rotulo de diagnostico: erro do I2C, toque e o ultimo flush (so redesenha se o texto mudou)
    if (s_dbg) {
        char msg[120];
        snprintf(msg, sizeof(msg), "i2c=%d st=0x%02X %s | fl#%lu gira%lums env%lums",
                 es3c35p_touch::ultimoErro(), es3c35p_touch::ultimoStatus(), s_toqueTxt,
                 s_nFlush, s_msGira, s_msEnvio);
        if (strcmp(msg, s_msgMostrada) != 0) {
            snprintf(s_msgMostrada, sizeof(s_msgMostrada), "%s", msg);
            lv_label_set_text(s_dbg, msg);
        }
    }
}

void es3c35p_lvgl_init()
{
    if (!es3c35p_display::init()) {
        ESP_LOGE(TAG, "tela nao inicializada: HUD desligada");
        return;
    }

    // Toque depois da tela, na mesma ordem do ambiente isolado que funcionou
    s_touchOk = es3c35p_touch::init();
    if (!s_touchOk) {
        ESP_LOGW(TAG, "touch nao iniciado: HUD sem toque");
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


    lv_init();
    lv_disp_draw_buf_init(&s_draw, s_lvgl, NULL, HUD_W * HUD_H);

    lv_disp_drv_init(&s_disp);
    s_disp.hor_res = HUD_W;
    s_disp.ver_res = HUD_H;
    s_disp.flush_cb = flush;
    s_disp.draw_buf = &s_draw;
    s_disp.full_refresh = 0;            // so as areas que mudaram sao redesenhadas e enviadas
    lv_disp_drv_register(&s_disp);

    lv_indev_drv_init(&s_indev);
    s_indev.type = LV_INDEV_TYPE_POINTER;
    s_indev.read_cb = toqueLer;
    lv_indev_drv_register(&s_indev);

    // Rotulo de diagnostico do toque (fica na camada de cima, sobre a HUD)
    s_dbg = lv_label_create(lv_layer_top());
    lv_obj_set_style_text_color(s_dbg, lv_color_hex(0xFFFF00), 0);
    lv_obj_align(s_dbg, LV_ALIGN_BOTTOM_LEFT, 4, -4);
    lv_label_set_text(s_dbg, s_touchOk ? "touch: pronto, aguardando toque" : "touch: NAO iniciou");

    ESP_LOGI(TAG, "LVGL ligado: %dx%d, quadro inteiro na PSRAM", HUD_W, HUD_H);
}

#endif  // ARDUINO_ARCH_ESP32
