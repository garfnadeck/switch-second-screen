#pragma once

// Teste isolado do toque (so para diagnostico): sem LVGL e sem Wi-Fi, o mesmo caminho da bancada.
// Desenha na tela: barra no topo (azul = toque lido sem erro, vermelho = erro do I2C) e um quadrado
// verde em cada ponto tocado. Ativo so com -DTOUCH_ISOLADO. Nunca retorna.

void touchIsoladoRun();
