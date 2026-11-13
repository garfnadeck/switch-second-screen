#pragma once

#include <stdint.h>
#include <math.h>
#ifdef ARDUINO
    #include <Arduino.h>
#endif
#if defined(ARDUINO_ARCH_ESP32)
    #include "driver/i2s_std.h"
#endif
#include "hardware/es3c35p_pins.h"
#include "hardware/es3c35p_touch.h"

namespace es3c35p_extra {

static bool s_sound_enabled = true;
static bool s_audio_ready   = false;
static uint8_t s_last_r = 255, s_last_g = 255, s_last_b = 255;

#if defined(ARDUINO_ARCH_ESP32)
static i2s_chan_handle_t s_tx_chan = nullptr;
static volatile bool s_playing = false;

inline bool es8311Write(uint8_t reg, uint8_t val) {
    return es3c35p_touch::i2cWriteReg(0x18, reg, val);
}

inline void initAudio() {
    pinMode(ES3C35P_AUDIO_EN, OUTPUT);
    digitalWrite(ES3C35P_AUDIO_EN, HIGH); // amplificador desligado ate confirmar codec

    if (!es3c35p_touch::i2cProbe(0x18)) {
        s_audio_ready = false;
        return;
    }

    // Configura ES8311 para 16 kHz / 16-bit I2S Slave (MCLK fornecido pelo ESP32)
    es8311Write(0x00, 0x1F); // Reset suave
    delay(10);
    es8311Write(0x00, 0x80); // CSM_ON = 1 (liga a maquina de estados do chip!)
    es8311Write(0x01, 0x3F); // Clocks ativos
    es8311Write(0x02, 0x00); // MCLK div = 1
    es8311Write(0x03, 0x10);
    es8311Write(0x04, 0x10);
    es8311Write(0x05, 0x00);
    es8311Write(0x06, 0x03);
    es8311Write(0x07, 0x00);
    es8311Write(0x08, 0xFF);
    es8311Write(0x09, 0x00); // SDP IN: I2S 16-bit
    es8311Write(0x0A, 0x00); // SDP OUT: I2S 16-bit
    es8311Write(0x0D, 0x01); // Analog power up
    es8311Write(0x0E, 0x02);
    es8311Write(0x12, 0x00); // DAC power up
    es8311Write(0x13, 0x10); // HP/Speaker output enable
    es8311Write(0x14, 0x1A); // PGA gain
    es8311Write(0x31, 0x00); // Unmute DAC
    es8311Write(0x32, 0x96); // Volume moderado
    es8311Write(0x37, 0x08);

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    if (i2s_new_channel(&chan_cfg, &s_tx_chan, nullptr) != ESP_OK) {
        s_audio_ready = false;
        return;
    }
    i2s_std_config_t std_cfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(16000),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = (gpio_num_t)ES3C35P_I2S_MCLK,
            .bclk = (gpio_num_t)ES3C35P_I2S_BCLK,
            .ws   = (gpio_num_t)ES3C35P_I2S_LRCK,
            .dout = (gpio_num_t)ES3C35P_I2S_DOUT,
            .din  = I2S_GPIO_UNUSED,
            .invert_flags = { .mclk_inv = false, .bclk_inv = false, .ws_inv = false },
        },
    };
    if (i2s_channel_init_std_mode(s_tx_chan, &std_cfg) != ESP_OK ||
        i2s_channel_enable(s_tx_chan) != ESP_OK) {
        s_audio_ready = false;
        return;
    }

    // Mantem amplificador desligado ate tocar (evita chiado/zumbido em repouso)
    digitalWrite(ES3C35P_AUDIO_EN, HIGH);
    s_audio_ready = true;
}

inline void audioTask(void * arg) {
    uint8_t mode = (uint8_t)(uintptr_t)arg;
    // mode 1 = Shiny arpeggio (3 notas), mode 0 = bip curto
    static const uint16_t freqs_shiny[3] = { 1046, 1318, 1568 }; // C6, E6, G6
    const uint8_t notes = (mode == 1) ? 3 : 1;
    int16_t buf[256]; // 128 frames estereo (8 ms @ 16 kHz)
    size_t written = 0;

    digitalWrite(ES3C35P_AUDIO_EN, LOW); // Liga amplificador apenas durante o som
    vTaskDelay(pdMS_TO_TICKS(10));

    for (uint8_t n = 0; n < notes; n++) {
        uint16_t f = (mode == 1) ? freqs_shiny[n] : 880;
        const int frames = 16000 / 8; // ~125 ms por nota
        float phase = 0.0f;
        float step = 2.0f * 3.14159265f * (float)f / 16000.0f;
        int sent = 0;
        while (sent < frames) {
            int chunk = (frames - sent > 128) ? 128 : (frames - sent);
            for (int i = 0; i < chunk; i++) {
                int16_t s = (int16_t)(sinf(phase) * 2800.0f);
                buf[i * 2]     = s;
                buf[i * 2 + 1] = s;
                phase += step;
            }
            i2s_channel_write(s_tx_chan, buf, (size_t)chunk * 4u, &written, pdMS_TO_TICKS(50));
            sent += chunk;
        }
    }
    memset(buf, 0, sizeof(buf));
    for (int z = 0; z < 12; z++) {
        i2s_channel_write(s_tx_chan, buf, sizeof(buf), &written, pdMS_TO_TICKS(50));
    }
    digitalWrite(ES3C35P_AUDIO_EN, HIGH); // Desliga amplificador ao terminar
    s_playing = false;
    vTaskDelete(nullptr);
}
#endif

inline void init() {
#if defined(ARDUINO_ARCH_ESP32)
    analogReadResolution(12);
    pinMode(ES3C35P_BATT_ADC, INPUT);
    neopixelWrite(ES3C35P_RGB_LED, 0, 0, 12);
#endif
}

inline void setSoundEnabled(bool en) { s_sound_enabled = en; }
inline bool isSoundEnabled() { return s_sound_enabled; }
inline bool isAudioReady() { return s_audio_ready; }

inline void setRgb(uint8_t r, uint8_t g, uint8_t b) {
#if defined(ARDUINO_ARCH_ESP32)
    if (r == s_last_r && g == s_last_g && b == s_last_b) return;
    s_last_r = r; s_last_g = g; s_last_b = b;
    neopixelWrite(ES3C35P_RGB_LED, r, g, b);
#else
    (void)r; (void)g; (void)b;
#endif
}

inline void playBeep(bool shiny_arpeggio = false) {
#if defined(ARDUINO_ARCH_ESP32)
    if (!s_sound_enabled || !s_audio_ready || s_playing || !s_tx_chan) return;
    s_playing = true;
    xTaskCreate(audioTask, "aud", 2048, (void *)(uintptr_t)(shiny_arpeggio ? 1 : 0), 1, nullptr);
#else
    (void)shiny_arpeggio;
#endif
}

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
