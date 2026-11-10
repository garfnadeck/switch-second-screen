#include "hardware/touch_isolado.h"

#if defined(ARDUINO_ARCH_ESP32) && defined(TOUCH_ISOLADO)

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "hardware/es3c35p_display.h"
#include "hardware/es3c35p_touch.h"
#ifdef TOUCH_ISOLADO_WIFI
    #include <WiFi.h>
#endif
#ifdef TOUCH_ISOLADO_LVGL
    #include <lvgl.h>
    #include "hardware/es3c35p_lvgl.h"
#endif

// Cores em RGB565 com os bytes trocados (o driver envia assim)
static uint16_t cor(uint16_t rgb565) { return (uint16_t)((rgb565 >> 8) | (rgb565 << 8)); }
#define COR_PRETO   cor(0x0000)
#define COR_AZUL    cor(0x001F)
#define COR_VERMELHO cor(0xF800)
#define COR_VERDE   cor(0x07E0)

static SemaphoreHandle_t s_fim = NULL;
static volatile bool     s_terminou = true;

static void fimDoEnvio(void * ctx)
{
    (void)ctx;
    BaseType_t acordar = pdFALSE;
    xSemaphoreGiveFromISR(s_fim, &acordar);
    if (acordar == pdTRUE) portYIELD_FROM_ISR();
}

// Envia um retangulo nativo e espera o SPI terminar (o buffer precisa ficar valido ate la)
static void enviar(int x1, int y1, int x2, int y2, const uint16_t * px)
{
    if (!es3c35p_display::drawRect(x1, y1, x2, y2, px, fimDoEnvio, NULL)) return;
    xSemaphoreTake(s_fim, pdMS_TO_TICKS(200));
}

static uint16_t s_faixa[320 * 40];   // para limpar a tela em faixas de 40 linhas
static uint16_t s_barra[320 * 12];   // barra de status no topo
static uint16_t s_quad[16 * 16];     // marca do toque

static void limpar()
{
    for (int i = 0; i < 320 * 40; i++) s_faixa[i] = COR_PRETO;
    for (int y = 0; y < 480; y += 40) enviar(0, y, 320, y + 40, s_faixa);
}

static void preencher(uint16_t * buf, int n, uint16_t c)
{
    for (int i = 0; i < n; i++) buf[i] = c;
}

void touchIsoladoRun()
{
#ifdef TOUCH_ISOLADO_LVGL
    // Teste de bisseccao: o caminho exato da HUD (LVGL + toque), sem interface e sem Wi-Fi.
    // O rotulo amarelo do canto inferior mostra o estado do toque.
    es3c35p_lvgl_init();
    while (1) {
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(5));
    }
#endif
    s_fim = xSemaphoreCreateBinary();
    if (!es3c35p_display::init()) {
        while (1) vTaskDelay(pdMS_TO_TICKS(1000));   // sem tela nao ha o que mostrar
    }
    limpar();

#ifdef TOUCH_ISOLADO_WIFI
    // Teste de bisseccao: liga o radio Wi-Fi (como o firmware principal faz) antes do toque
    WiFi.mode(WIFI_AP);
    WiFi.softAP("SwitchHUD", "switchhud1");
    vTaskDelay(pdMS_TO_TICKS(500));
#endif

    bool toqueOk = es3c35p_touch::init();
    preencher(s_quad, 16 * 16, COR_VERDE);

    while (1) {
        uint16_t x = 0, y = 0;
        bool tocou = toqueOk && es3c35p_touch::read(&x, &y);

        // Barra no topo: vermelho se o I2C reportou erro, azul se a leitura esta sem erro
        int erro = es3c35p_touch::ultimoErro();
        preencher(s_barra, 320 * 12, erro == 0 ? COR_AZUL : COR_VERMELHO);
        enviar(0, 0, 320, 12, s_barra);

        if (tocou) {
            int x1 = x - 8 < 0 ? 0 : x - 8;
            int y1 = y - 8 < 0 ? 0 : y - 8;
            int x2 = x1 + 16 > 320 ? 320 : x1 + 16;
            int y2 = y1 + 16 > 480 ? 480 : y1 + 16;
            enviar(x1, y1, x2, y2, s_quad);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

#else

void touchIsoladoRun() {}

#endif
