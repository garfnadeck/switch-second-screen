#pragma once

#include <stdint.h>
#ifdef ARDUINO
    #include <Arduino.h>
#endif
#include "hardware/es3c35p_pins.h"

namespace es3c35p_extra {

static bool s_sound_enabled = true;

inline void init() {
#if defined(ARDUINO_ARCH_ESP32)
    analogReadResolution(12);
    pinMode(ES3C35P_BATT_ADC, INPUT);
#endif
}

inline void setSoundEnabled(bool en) { s_sound_enabled = en; }
inline bool isSoundEnabled() { return s_sound_enabled; }

// Le a tensao da bateria no GPIO 8 (divisor 100k/100k -> multiplica por 2).
// Retorna tensao em milivolts (ex.: 4150 = 4.15V) e preenche porcentagem estimada (0..100).
inline uint16_t readBatteryMv(uint8_t &out_pct) {
#if defined(ARDUINO_ARCH_ESP32)
    uint32_t mv_pin = analogReadMilliVolts(ES3C35P_BATT_ADC);
    uint32_t mv = mv_pin * 2u;
    if (mv < 2500u || mv > 4500u) {
        // Alimentado direto pelo USB sem bateria conectada no JST (ou leitura flutuante)
        out_pct = 100;
        return 5000;
    }
    if (mv >= 4150u) out_pct = 100;
    else if (mv <= 3300u) out_pct = 0;
    else out_pct = (uint8_t)(((mv - 3300u) * 100u) / 850u);
    return (uint16_t)mv;
#else
    out_pct = 100;
    return 4200;
#endif
}

}  // namespace es3c35p_extra
