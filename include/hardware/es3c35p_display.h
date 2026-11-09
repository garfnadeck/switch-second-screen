#pragma once

// Tela ST77922 (QSPI, 320x480) da placa ES3C35P.
// Init com a sequencia do fabricante e clock de 80 MHz (validado na bancada).
// Todas as funcoes sao no-op fora do ESP32 (o Pico W nao tem esp_lcd).

#include <stdint.h>

namespace es3c35p_display {

// Liga o backlight, sobe o barramento QSPI e inicializa o painel. false se falhar.
bool init();

// Desenha um texto centralizado (fonte 5x7 ampliada: T, E, S), preto sobre branco.
// Serve so para teste de bancada; a HUD vira o LVGL na fase F3.
bool drawTestText(const char * texto);

// Chamado quando o SPI termina de enviar um retangulo (contexto ISR: so sinalizar, nada pesado).
typedef void (*DrawDoneFn)(void * ctx);

// Envia pixels RGB565 (byte trocado) para a area nativa do painel 320x480. x2/y2 sao exclusivos.
// O buffer deve ficar valido ate done() ser chamado. Retorna false se o painel nao estiver pronto.
bool drawRect(int x1, int y1, int x2, int y2, const void * pixels, DrawDoneFn done, void * ctx);

}  // namespace es3c35p_display
