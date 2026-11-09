#include "net/wifi_scanner.h"
#include <WiFi.h>

// Tempo maximo de uma varredura antes de desistir (ms)
static constexpr uint32_t SCAN_TIMEOUT_MS = 15000;

// Valor "rede aberta" varia entre os cores: arduino-pico usa ENC_TYPE_NONE, arduino-esp32 usa WIFI_AUTH_OPEN
#if defined(ARDUINO_ARCH_ESP32)
    #define SCAN_OPEN_TYPE WIFI_AUTH_OPEN
#else
    #define SCAN_OPEN_TYPE ENC_TYPE_NONE
#endif

WifiScanner wifiScanner;

static const char * stateName(WifiScanner::State s) {
    switch (s) {
        case WifiScanner::State::RUNNING: return "running";
        case WifiScanner::State::DONE:    return "done";
        case WifiScanner::State::FAILED:  return "failed";
        default:                          return "idle";
    }
}

// Escapa aspas, barra invertida e caracteres de controle para o JSON
static String jsonEscape(const char * in) {
    String o;
    for (const char * c = in; *c; c++) {
        if (*c == '"' || *c == '\\')      { o += '\\'; o += *c; }
        else if ((unsigned char)*c < 32)  { o += ' '; }
        else                              { o += *c; }
    }
    return o;
}

bool WifiScanner::begin() {
    if (_state == State::RUNNING) return false;
    // Varredura assincrona: o radio continua atendendo a pagina enquanto procura.
    // Nota: em modo SwitchHUD (AP) o radio do Pico W pode nao aceitar varredura;
    // isso precisa ser validado na placa antes de dar o recurso como pronto.
    WiFi.scanNetworks(true);
    _state = State::RUNNING;
    _startedAt = millis();
    return true;
}

void WifiScanner::poll() {
    if (_state != State::RUNNING) return;
    int found = WiFi.scanComplete();
    if (found == WIFI_SCAN_RUNNING) {
        if (millis() - _startedAt > SCAN_TIMEOUT_MS) {
            WiFi.scanDelete();
            _state = State::FAILED;
        }
        return;
    }
    if (found < 0) {                       // WIFI_SCAN_FAILED ou erro do radio
        WiFi.scanDelete();
        _state = State::FAILED;
        return;
    }
    collect(found);
    WiFi.scanDelete();
    _state = State::DONE;
}

// Guarda as redes encontradas: sem SSID vazio (ocultas), sem repeticao, mais fortes primeiro
void WifiScanner::collect(int found) {
    _count = 0;
    for (int i = 0; i < found; i++) {
        String ssid = WiFi.SSID(i);
        if (ssid.length() == 0 || ssid.length() > 32) continue;
        int16_t rssi = (int16_t)WiFi.RSSI(i);
        bool secure = WiFi.encryptionType(i) != SCAN_OPEN_TYPE;

        int dup = -1;
        for (uint8_t k = 0; k < _count; k++) {
            if (strcmp(_nets[k].ssid, ssid.c_str()) == 0) { dup = k; break; }
        }
        if (dup >= 0) {
            if (rssi > _nets[dup].rssi) _nets[dup].rssi = rssi;   // mantem o sinal mais forte
            continue;
        }
        if (_count >= MAX_NETS) continue;
        snprintf(_nets[_count].ssid, sizeof(_nets[_count].ssid), "%s", ssid.c_str());
        _nets[_count].rssi = rssi;
        _nets[_count].secure = secure;
        _count++;
    }
    // Ordenacao simples por sinal (no maximo 20 itens)
    for (uint8_t a = 0; a < _count; a++) {
        for (uint8_t b = a + 1; b < _count; b++) {
            if (_nets[b].rssi > _nets[a].rssi) {
                Net tmp = _nets[a]; _nets[a] = _nets[b]; _nets[b] = tmp;
            }
        }
    }
}

String WifiScanner::toJson() const {
    String j = "{\"state\":\"";
    j += stateName(_state);
    j += "\",\"count\":" + String(_count) + ",\"nets\":[";
    for (uint8_t i = 0; i < _count; i++) {
        if (i) j += ",";
        j += "{\"ssid\":\"" + jsonEscape(_nets[i].ssid) + "\",\"rssi\":" + String(_nets[i].rssi) +
             ",\"secure\":" + (_nets[i].secure ? "true" : "false") + "}";
    }
    j += "]}";
    return j;
}
