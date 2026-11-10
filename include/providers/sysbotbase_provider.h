#ifndef SYSBOTBASE_PROVIDER_H
#define SYSBOTBASE_PROVIDER_H

#include "../core/i_game_provider.h"
#include "za_memory_map.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef ARDUINO
    #include <Arduino.h>
    #include <WiFi.h>
#else
    #include <sys/socket.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <errno.h>
    extern "C" uint32_t sim_millis();
    static inline uint32_t millis() { return sim_millis(); }
#endif

// Cliente do sys-botbase (Atmosphere, TCP porta 6000).
// Protocolo: comandos texto terminados em "\r\n"; a resposta de peek/pointerPeek e uma linha
// de hexadecimal terminada em "\n". Um pedido por vez (pede -> espera a linha -> proximo).
//
// Dois usos:
//  * modo UNLOCKED_SYSBOTBASE  : provider ativo (como antes)
//  * telemetria paralela       : o USB HID segue mandando as macros e este provider so le a RAM
//                                (router.startTelemetry) -> nao mexe em state.mode
class SysBotbaseProvider : public IGameProvider {
private:
#ifdef ARDUINO
    WiFiClient client;
#else
    int sock_fd = -1;
#endif
    static constexpr uint16_t RX_MAX = 3072;            // 6 Pokemon x ate ~250 bytes em hex cabe folgado
    char     rx[RX_MAX];
    uint16_t rx_len = 0;
    bool     awaiting = false;                           // ha um pedido sem resposta
    uint8_t  next_req = 0;                               // 0 = party, 1 = posicao
    uint32_t sent_ms = 0, last_poll_ms = 0, last_try_ms = 0;
    bool     linked = false;
    bool     telemetry_only = false;
    ZaMemoryMap map = ZA_MAP;

    // ---- Deteccao automatica do Switch (varre prefixo.1 .. prefixo.254 procurando a porta do sys-botbase) ----
    uint8_t scan_state = 0;               // 0 ocioso, 1 varrendo, 2 achou, 3 nao achou
    char    scan_prefix[20] = {};         // ex.: "192.168.15."
    int     scan_host = 1, scan_last = 254;

    void setLink(GameState &s, const char * t) { snprintf(s.link_status, sizeof(s.link_status), "%s", t); }

    bool isConnected() {
#ifdef ARDUINO
        return client.connected();
#else
        return sock_fd >= 0;
#endif
    }

    bool connectToSwitch(const char* ip, uint16_t port) {
        disconnect();
#ifdef ARDUINO
  #if defined(ARDUINO_ARCH_ESP32)
        return client.connect(ip, port, 150);
  #else
        client.setTimeout(150);
        return client.connect(ip, port);
  #endif
#else
        sock_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (sock_fd < 0) return false;
        struct sockaddr_in a; memset(&a, 0, sizeof(a));
        a.sin_family = AF_INET; a.sin_port = htons(port);
        inet_pton(AF_INET, ip, &a.sin_addr);
        if (connect(sock_fd, (struct sockaddr*)&a, sizeof(a)) != 0) { close(sock_fd); sock_fd = -1; return false; }
        fcntl(sock_fd, F_SETFL, O_NONBLOCK);
        return true;
#endif
    }

    void disconnect() {
        rx_len = 0; awaiting = false;
#ifdef ARDUINO
        client.stop();
#else
        if (sock_fd >= 0) { close(sock_fd); sock_fd = -1; }
#endif
    }

    void sendRaw(const char* cmd) {
#ifdef ARDUINO
        if (client.connected()) client.print(cmd);
#else
        if (sock_fd >= 0) send(sock_fd, cmd, strlen(cmd), 0);
#endif
    }

    // Acumula bytes ate achar '\n'. Retorna true quando ha uma linha completa em rx (sem o \n).
    bool readLine() {
        for (;;) {
            char c;
#ifdef ARDUINO
            if (!client.available()) return false;
            c = (char)client.read();
#else
            if (sock_fd < 0) return false;
            ssize_t n = recv(sock_fd, &c, 1, 0);
            if (n == 0) { disconnect(); return false; }
            if (n < 0) return false;
#endif
            if (c == '\n') { rx[rx_len] = 0; return true; }
            if (c != '\r' && rx_len < RX_MAX - 1) rx[rx_len++] = c;
        }
    }

    static int hexNib(char c) {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    }
    // "0A1B..." -> bytes. Retorna quantidade de bytes (0 se invalido)
    static uint16_t hexToBytes(const char * hex, uint8_t * out, uint16_t max) {
        uint16_t n = 0;
        while (hex[0] && hex[1] && n < max) {
            int hi = hexNib(hex[0]), lo = hexNib(hex[1]);
            if (hi < 0 || lo < 0) break;
            out[n++] = (uint8_t)((hi << 4) | lo);
            hex += 2;
        }
        return n;
    }
    static uint16_t rd16(const uint8_t * p) { return (uint16_t)(p[0] | (p[1] << 8)); }
    static float    rdf (const uint8_t * p) { float f; memcpy(&f, p, 4); return f; }

    static const char * statusName(uint8_t s) {
        switch (s) { case 1: return "SLP"; case 2: return "PSN"; case 3: return "BRN";
                     case 4: return "FRZ"; case 5: return "PAR"; case 6: return "CNF"; default: return ""; }
    }

    void sendChain(const char * name, uint16_t size, const MemChain &c) {
        char cmd[160];
        int n = snprintf(cmd, sizeof(cmd), "%s %u", name, (unsigned)size);
        for (uint8_t i = 0; i < c.count && n < (int)sizeof(cmd) - 16; i++)
            n += snprintf(cmd + n, sizeof(cmd) - n, " 0x%X", (unsigned)c.jumps[i]);
        snprintf(cmd + n, sizeof(cmd) - n, "\r\n");
        sendRaw(cmd);
    }

    void requestNext(GameState &state) {
        // Sem enderecos configurados (ou jogo diferente do Z-A): so confirma o link com getTitleID
        bool za = (state.profile == GameProfile::POKEMON_LEGENDS_ZA);
        if (!za || (map.party.count == 0 && map.pos.count == 0)) {
            sendRaw("getTitleID\r\n"); next_req = 0; awaiting = true; sent_ms = millis(); return;
        }
        if (next_req == 0 && map.party.count) {
            sendChain("pointerPeek", (uint16_t)(map.party_stride * map.party_count), map.party);
        } else if (map.pos.count) {
            next_req = 1;
            sendChain("pointerPeek", 12, map.pos);
        } else {
            sendChain("pointerPeek", (uint16_t)(map.party_stride * map.party_count), map.party);
        }
        awaiting = true; sent_ms = millis();
    }

    void handleReply(GameState &state) {
        bool za = (state.profile == GameProfile::POKEMON_LEGENDS_ZA);
        if (!za || (map.party.count == 0 && map.pos.count == 0)) {
            if (!telemetry_only) state.connected = true;
            setLink(state, "ok (sem offsets)");
            return;
        }
        static uint8_t buf[RX_MAX / 2];
        uint16_t n = hexToBytes(rx, buf, sizeof(buf));
        if (n == 0) { setLink(state, "resposta invalida"); return; }

        if (next_req == 0 && map.party.count) {
            for (uint8_t i = 0; i < map.party_count && i < 6; i++) {
                uint32_t b = (uint32_t)i * map.party_stride;
                if (b + map.party_stride > n) break;
                PokemonSlot &p = state.party[i];
                uint16_t hpmax = rd16(buf + b + map.off_hp_max);
                if (hpmax == 0 || hpmax > 9999) { p.max_hp = 0; p.current_hp = 0; p.name[0] = 0; p.status[0] = 0; continue; }
                p.max_hp = hpmax;
                p.current_hp = rd16(buf + b + map.off_hp_cur);
                p.level = buf[b + map.off_level];
                snprintf(p.name, sizeof(p.name), "#%u", (unsigned)rd16(buf + b + map.off_species)); // TODO: tabela de nomes
                snprintf(p.status, sizeof(p.status), "%s", statusName(buf[b + map.off_status]));
            }
            next_req = map.pos.count ? 1 : 0;
        } else {
            if (n >= 12) {
                state.map.x = rdf(buf + map.off_x);
                state.map.y = rdf(buf + map.off_z);   // no mapa 2D: horizontal = x, vertical = z do jogo
                state.map.z = rdf(buf + map.off_y);
            }
            next_req = map.party.count ? 0 : 1;
        }
        if (!telemetry_only) state.connected = true;   // em telemetria, 'connected' = USB HID
        setLink(state, "ok (lendo RAM)");
    }

    void scanStep(GameState &state) {
        char ip[16];
        snprintf(ip, sizeof(ip), "%.12s%d", scan_prefix, scan_host);
        if (connectToSwitch(ip, state.switch_port)) {
            snprintf(state.switch_ip, sizeof(state.switch_ip), "%s", ip);
            scan_state = 2;
            setLink(state, "switch encontrado");
            last_poll_ms = millis();
            return;
        }
        if (++scan_host > scan_last) { scan_state = 3; setLink(state, "switch nao encontrado"); }
    }

    // Um passo da maquina de estados (chamar sempre; nunca bloqueia mais que o connect)
    void pump(GameState &state) {
        uint32_t now = millis();

        if (scan_state == 1) { scanStep(state); return; }   // 1 host por chamada (<=150 ms)

        if (!isConnected()) {
            if (now - last_try_ms >= 5000) {             // tenta reconectar a cada 5 s (connect bloqueia ate 150 ms)
                last_try_ms = now;
                linked = connectToSwitch(state.switch_ip, state.switch_port);
                setLink(state, linked ? "conectado" : "sem conexao");
                if (!linked && !telemetry_only) state.connected = false;
            }
            return;
        }

        if (awaiting) {
            if (readLine()) { awaiting = false; handleReply(state); rx_len = 0; last_poll_ms = now; }
            else if (now - sent_ms > 800) { awaiting = false; rx_len = 0; setLink(state, "timeout"); }
            return;
        }
        if (now - last_poll_ms >= 250) requestNext(state);   // ~4 leituras/s no total
    }

public:
    ~SysBotbaseProvider() override { disconnect(); }

    // ---- telemetria paralela (USB HID continua sendo o provider ativo) ----
    void startTelemetry(GameState &state) { telemetry_only = true; last_try_ms = 0; setLink(state, "iniciando"); }
    void pollTelemetry(GameState &state)  { pump(state); }

    // Troca o alvo (IP/porta ja gravados em state) e reconecta
    void retarget(GameState &state) { disconnect(); last_try_ms = 0; scan_state = 0; setLink(state, "reconectando"); }

    // Varre a sub-rede. first/last permitem testar com faixas pequenas.
    void startScan(GameState &state, const char * prefix, int first = 1, int last = 254) {
        telemetry_only = true;
        disconnect();
        snprintf(scan_prefix, sizeof(scan_prefix), "%s", prefix);
        scan_host = first; scan_last = last; scan_state = 1;
        setLink(state, "procurando switch");
    }
    uint8_t scanState() const { return scan_state; }
    int scanProgress() const { return scan_state == 1 ? (scan_host * 100) / (scan_last ? scan_last : 1) : (scan_state ? 100 : 0); }

    // ---- provider ativo (modo Sys-Botbase) ----
    void init(GameState &state) override {
        telemetry_only = false;
        state.mode = SwitchMode::UNLOCKED_SYSBOTBASE;
        last_try_ms = 0;
        setLink(state, "iniciando");
    }
    void setProfile(GameState &state, GameProfile new_profile) override {
        state.profile = new_profile;
        next_req = 0;
    }
    void update(GameState &state) override {
        state.mode = SwitchMode::UNLOCKED_SYSBOTBASE;
        pump(state);
    }
    void triggerAction(ActionMacro, GameState &) override {
        // Botoes pelo sys-botbase (click/press/setStick) ficam para uma etapa futura;
        // as macros continuam saindo pelo USB HID.
    }
};

#endif // SYSBOTBASE_PROVIDER_H