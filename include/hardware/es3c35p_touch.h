#pragma once

// Touch capacitivo da ES3C35P: I2C0 (IO38/IO39), chip em 0x55, RST IO48, INT IO47.
// Sequencia validada na bancada: 1 ponto, status 0x0010 e pontos 0x0014 (7 bytes).
// Workaround: se o INT ficar baixo por mais de 30 ms, pulsa o RST (causa raiz ainda aberta).
// Fora do ESP32 nao faz nada.

#include <stdint.h>

namespace es3c35p_touch {

// Sobe o I2C e o RST/INT. false se falhar (o HUD segue sem toque).
bool init();

// true se houver toque. x/y: coordenadas nativas do painel (x: 0..319, y: 0..479).
bool read(uint16_t * x, uint16_t * y);

// Diagnostico: ultimo erro do I2C (esp_err_t, 0 = ok) e ultimo byte de status lido.
int      ultimoErro();
uint8_t  ultimoStatus();
const char * infoChip0x28();

// Acesso ao mesmo barramento I2C0 (IO38/IO39) para o codec de audio ES8311 (0x18).
bool i2cProbe(uint8_t addr7);
bool i2cWriteReg(uint8_t addr7, uint8_t reg, uint8_t val);

}  // namespace es3c35p_touch
