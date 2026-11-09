#pragma once
// Busca de redes Wi-Fi ao redor da placa (assincrona).
// Uso: begin() na rota POST, poll() a cada volta do loop, toJson() na rota GET.
// A varredura leva alguns segundos; a pagina consulta o resultado em seguida.

#include <Arduino.h>

class WifiScanner {
public:
    enum class State : uint8_t { IDLE, RUNNING, DONE, FAILED };

    static constexpr uint8_t MAX_NETS = 20;

    struct Net {
        char ssid[33];
        int16_t rssi;
        bool secure;
    };

    // Inicia a varredura. false se ja estiver rodando.
    bool begin();

    // Fecha a varredura quando o radio termina (ou por tempo limite).
    void poll();

    State state() const { return _state; }

    // {"state":"...","count":N,"nets":[{"ssid":"...","rssi":-60,"secure":true}, ...]}
    String toJson() const;

private:
    State _state = State::IDLE;
    uint32_t _startedAt = 0;
    uint8_t _count = 0;
    Net _nets[MAX_NETS];

    void collect(int found);
};

extern WifiScanner wifiScanner;
