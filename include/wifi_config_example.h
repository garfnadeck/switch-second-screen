#ifndef WIFI_CONFIG_H
#define WIFI_CONFIG_H

const char* const WIFI_SSID = "SEU_WIFI";
const char* const WIFI_PASSWORD = "SUA_SENHA";

// IP do Nintendo Switch na sua rede (ou IP do Mac rodando fake_switch_server.py)
const char* const SWITCH_IP = "192.168.15.XX"; // <-- Coloque aqui o IP do Switch
const uint16_t SWITCH_PORT = 6000;             // Porta padrão do sys-botbase

#endif // WIFI_CONFIG_H