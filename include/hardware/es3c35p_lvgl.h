#pragma once

// Liga o LVGL (480x320, paisagem) ao painel ST77922 da ES3C35P (320x480, retrato).
// A rotacao de 90 graus e feita no flush. O toque ainda nao esta ligado (entra depois).
// Fora do ESP32 nao faz nada.

void es3c35p_lvgl_init();
void es3c35p_lvgl_set_flip(bool flip);
bool es3c35p_lvgl_get_flip();
