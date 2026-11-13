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
    uint8_t  next_req = 0;                               // 0 = bloco PA9 do slot, 1 = extensao do slot, 2 = posicao
    uint8_t  party_idx = 0;                              // slot atual do time (0..5)
    uint32_t pending_ec = 0;                             // chave PA9 do slot atual (para decifrar a extensao 0x10)
    uint16_t pending_hp_cur = 0;
    uint16_t pending_species = 0;
    uint8_t  pending_status = 0;
    uint16_t pending_item = 0;
    uint8_t  pending_pp[4] = {};
    uint8_t  pending_nature = 0;
    uint16_t pending_ev_total = 0;
    bool     pending_shiny = false;
    bool     pending_max_iv = false;
    bool     pending_alpha = false;
    char     pending_name[16] = {};
    uint8_t  slot_claro[6][328] = {};                    // cache do bloco PA9 decifrado de cada slot (para edicao instantanea)
    uint8_t  slot_extra[6][16] = {};                     // cache da extensao 0x10 decifrada de cada slot (nivel / HP max)
    uint32_t slot_ec[6] = {};                            // ultimo EC visto em cada slot
    uint8_t  slot_ext_age[6] = {};                       // contadores para pular releitura redundante da extensao +0x50
    bool     slot_valid[6] = {};
    uint8_t  stash_poll_cnt = 0;                         // a cada 12 leituras checa o ShinyStashPointer
    uint32_t sent_ms = 0, last_poll_ms = 0, last_try_ms = 0, last_auto_scan_ms = 0;
    uint8_t  title_poll_cnt = 0;
    bool     awaiting_title = false;
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

    // O Wi-Fi do Switch pode levar algumas centenas de ms para aceitar a conexao:
    // com 150 ms ele era pulado na busca e a conexao direta falhava.
    // Valores de 1 s ou mais de proposito: ha versoes da biblioteca de rede do ESP32 que
    // arredondam limites abaixo de 1 s para zero, e a conexao falha na hora.
    static constexpr int TIMEOUT_CONEXAO_MS = 2000;     // conexao direta no IP configurado
    static constexpr int TIMEOUT_BUSCA_MS   = 1000;     // por endereco, durante a busca

    bool connectToSwitch(const char* ip, uint16_t port, int timeout_ms = TIMEOUT_CONEXAO_MS) {
        disconnect();
#ifdef ARDUINO
  #if defined(ARDUINO_ARCH_ESP32)
        bool ok = client.connect(ip, port, timeout_ms);
  #else
        client.setTimeout(timeout_ms);
        bool ok = client.connect(ip, port);
  #endif
        if (ok) client.setNoDelay(true);
        return ok;
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
        rx_len = 0; awaiting = false; next_req = 0;
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
    static uint32_t rd32(const uint8_t * p) {
        return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
    }
    static void wr16(uint8_t * p, uint16_t v) { p[0] = (uint8_t)(v & 0xFF); p[1] = (uint8_t)(v >> 8); }
    static void wr32(uint8_t * p, uint32_t v) {
        p[0] = (uint8_t)(v & 0xFF); p[1] = (uint8_t)((v >> 8) & 0xFF);
        p[2] = (uint8_t)((v >> 16) & 0xFF); p[3] = (uint8_t)((v >> 24) & 0xFF);
    }
    static float    rdf (const uint8_t * p) { float f; memcpy(&f, p, 4); return f; }

    static const char * statusName(uint8_t s) {
        switch (s) { case 1: return "SLP"; case 2: return "PSN"; case 3: return "BRN";
                     case 4: return "FRZ"; case 5: return "PAR"; case 6: return "CNF"; default: return ""; }
    }

    static const uint8_t * pa9OrderTable() {
        static const uint8_t ORDER[128] = {
            0,1,2,3, 0,1,3,2, 0,2,1,3, 0,3,1,2, 0,2,3,1, 0,3,2,1,
            1,0,2,3, 1,0,3,2, 2,0,1,3, 3,0,1,2, 2,0,3,1, 3,0,2,1,
            1,2,0,3, 1,3,0,2, 2,1,0,3, 3,1,0,2, 2,3,0,1, 3,2,0,1,
            1,2,3,0, 1,3,2,0, 2,1,3,0, 3,1,2,0, 2,3,1,0, 3,2,1,0,
            0,1,2,3, 0,1,3,2, 0,2,1,3, 0,3,1,2, 0,2,3,1, 0,3,2,1,
            1,0,2,3, 1,0,3,2
        };
        return ORDER;
    }

    // ---- Decifragem e Cifragem PA9 (PKHeX: LCG 0x41C64E6D + 0x6073 e 4 blocos de 80 bytes) ----
    static void pa9Xor(uint8_t * data, uint16_t len, uint32_t seed) {
        for (uint16_t i = 0; i + 1 < len; i += 2) {
            seed = seed * 0x41C64E6Du + 0x6073u;
            uint16_t k = (uint16_t)(seed >> 16);
            data[i]     ^= (uint8_t)(k & 0xFF);
            data[i + 1] ^= (uint8_t)(k >> 8);
        }
    }

    static bool pa9DecryptBlock(const uint8_t * raw, uint8_t * out, uint32_t &out_ec) {
        uint32_t ec = rd32(raw);
        uint16_t sanity = rd16(raw + 4);
        uint16_t chk_expect = rd16(raw + 6);
        if ((ec == 0 && chk_expect == 0) || sanity != 0) return false;

        uint8_t body[320];
        memcpy(body, raw + 8, 320);
        pa9Xor(body, 320, ec);

        uint16_t sum = 0;
        for (uint16_t i = 0; i < 320; i += 2) sum = (uint16_t)(sum + rd16(body + i));
        if (sum != chk_expect) return false;

        const uint8_t * ORDER = pa9OrderTable();
        uint8_t sv = (uint8_t)((ec >> 13) & 31u);
        memcpy(out, raw, 8);
        for (uint8_t b = 0; b < 4; b++) {
            uint8_t src_blk = ORDER[sv * 4 + b];
            memcpy(out + 8 + b * 80, body + src_blk * 80, 80);
        }
        out_ec = ec;
        return rd16(out + 0x08) != 0;
    }

    static void pa9EncryptBlock(uint8_t * claro, uint8_t * raw_out) {
        uint16_t sum = 0;
        for (uint16_t i = 8; i < 328; i += 2) sum = (uint16_t)(sum + rd16(claro + i));
        wr16(claro + 4, 0);
        wr16(claro + 6, sum);

        uint32_t ec = rd32(claro);
        const uint8_t * ORDER = pa9OrderTable();
        uint8_t sv = (uint8_t)((ec >> 13) & 31u);
        uint8_t body[320];
        for (uint8_t b = 0; b < 4; b++) {
            uint8_t dst_blk = ORDER[sv * 4 + b];
            memcpy(body + dst_blk * 80, claro + 8 + b * 80, 80);
        }
        pa9Xor(body, 320, ec);
        memcpy(raw_out, claro, 8);
        memcpy(raw_out + 8, body, 320);
    }

    static void pa9ExtractMeta(const uint8_t * claro, uint16_t &item, uint8_t pp[4],
                               uint8_t &nature, uint16_t &ev_tot,
                               bool &shiny, bool &max_iv, bool &alpha) {
        item = rd16(claro + 0x0A);
        for (int m = 0; m < 4; m++) pp[m] = claro[0x7A + m];
        nature = claro[0x21];
        ev_tot = 0;
        for (int e = 0; e < 6; e++) ev_tot = (uint16_t)(ev_tot + claro[0x26 + e]);
        uint16_t tid = rd16(claro + 0x0C);
        uint16_t sid = rd16(claro + 0x0E);
        uint32_t pid = rd32(claro + 0x1C);
        uint16_t p_lo = (uint16_t)(pid & 0xFFFFu);
        uint16_t p_hi = (uint16_t)(pid >> 16);
        shiny = ((tid ^ sid ^ p_lo ^ p_hi) < 16u);
        uint32_t iv32 = rd32(claro + 0x8C);
        max_iv = ((iv32 & 0x3FFFFFFFu) == 0x3FFFFFFFu);
        alpha = (claro[0x23] != 0 || claro[0x4A] >= 250);
    }

    static void pa9ExtractName(const uint8_t * claro, uint16_t species, char * out, size_t out_sz) {
        size_t j = 0;
        for (uint8_t i = 0; i < 13 && j + 1 < out_sz; i++) {
            uint16_t ch = rd16(claro + 0x58 + i * 2);
            if (ch == 0) break;
            if (ch >= 0x20 && ch <= 0x7E) out[j++] = (char)ch;
        }
        out[j] = 0;
        if (j == 0) snprintf(out, out_sz, "#%u", (unsigned)species);
    }

    static void clearSlot(PokemonSlot &p) {
        p.species = 0; p.max_hp = 0; p.current_hp = 0; p.level = 0;
        p.held_item = 0; p.pp[0] = p.pp[1] = p.pp[2] = p.pp[3] = 0;
        p.nature = 0; p.ev_total = 0;
        p.is_shiny = false; p.is_max_iv = false; p.is_alpha = false;
        p.name[0] = 0; p.status[0] = 0; p.active = false;
    }

    static const char * lumioseZoneName(float x, float z_map, float alt_y) {
        if (alt_y < -8.0f) return "Lumiose (Subsolo)";

        // Coordenadas normalizadas [0..1] no mapa_lumiose.jpg:
        // Centro (Campo 20 / Torre Prisma) = (-830.0, -500.0), Raio = 490.0
        float nx = 0.50f + (x - (-830.0f)) / 980.0f;
        float ny = 0.50f - (z_map - (-500.0f)) / 980.0f;

        struct WildZoneRef { uint8_t id; float nx; float ny; float r; const char * label; };
        static const WildZoneRef ZONES[20] = {
            {  1, 0.585f, 0.905f, 0.085f, "Campo 1 (Zona Selvagem 1)"   },
            {  2, 0.620f, 0.660f, 0.075f, "Campo 2 (Zona Selvagem 2)"   },
            {  3, 0.500f, 0.280f, 0.070f, "Campo 3 (Zona Selvagem 3)"   },
            {  4, 0.570f, 0.150f, 0.075f, "Campo 4 (Zona Selvagem 4)"   },
            {  5, 0.410f, 0.600f, 0.075f, "Campo 5 (Zona Selvagem 5)"   },
            {  6, 0.900f, 0.530f, 0.075f, "Campo 6 (Zona Selvagem 6)"   },
            {  7, 0.290f, 0.430f, 0.070f, "Campo 7 (Zona Selvagem 7)"   },
            {  8, 0.710f, 0.420f, 0.070f, "Campo 8 (Zona Selvagem 8)"   },
            {  9, 0.130f, 0.560f, 0.075f, "Campo 9 (Zona Selvagem 9)"   },
            { 10, 0.160f, 0.660f, 0.075f, "Campo 10 (Zona Selvagem 10)" },
            { 11, 0.820f, 0.590f, 0.075f, "Campo 11 (Zona Selvagem 11)" },
            { 12, 0.420f, 0.820f, 0.075f, "Campo 12 (Zona Selvagem 12)" },
            { 13, 0.400f, 0.075f, 0.075f, "Campo 13 (Zona Selvagem 13)" },
            { 14, 0.110f, 0.430f, 0.075f, "Campo 14 (Zona Selvagem 14)" },
            { 15, 0.810f, 0.180f, 0.075f, "Campo 15 (Zona Selvagem 15)" },
            { 16, 0.360f, 0.680f, 0.070f, "Campo 16 (Zona Selvagem 16)" },
            { 17, 0.760f, 0.770f, 0.070f, "Campo 17 (Zona Selvagem 17)" },
            { 18, 0.200f, 0.180f, 0.075f, "Campo 18 (Zona Selvagem 18)" },
            { 19, 0.800f, 0.310f, 0.075f, "Campo 19 (Zona Selvagem 19)" },
            { 20, 0.500f, 0.500f, 0.085f, "Campo 20 (Centro / Prisma)"  },
        };

        int best_idx = -1;
        float best_d2 = 999.0f;
        for (int i = 0; i < 20; i++) {
            float dx = nx - ZONES[i].nx;
            float dy = ny - ZONES[i].ny;
            float d2 = dx * dx + dy * dy;
            if (d2 < best_d2) {
                best_d2 = d2;
                best_idx = i;
            }
        }
        if (best_idx >= 0 && best_d2 <= ZONES[best_idx].r * ZONES[best_idx].r) {
            return ZONES[best_idx].label;
        }

        if (alt_y > 14.0f) return "Lumiose (Telhados)";
        if (ny < 0.35f) {
            if (nx < 0.38f) return "Distrito Bleu / NO";
            if (nx > 0.62f) return "Distrito Jaune / NE";
            return "Distrito Rouge / Norte";
        }
        if (ny > 0.65f) {
            if (nx < 0.38f) return "Distrito Magenta / SO";
            if (nx > 0.62f) return "Bulevar Sul / SE";
            return "Distrito Vert / Sul";
        }
        return (nx < 0.50f) ? "Lumiose Oeste" : "Lumiose Leste";
    }

    void advancePartySlot() {
        uint8_t cnt = (map.party_count > 0 && map.party_count <= 6) ? map.party_count : 6;
        party_idx = (uint8_t)((party_idx + 1) % cnt);
        if (++stash_poll_cnt >= 12) {
            stash_poll_cnt = 0;
            next_req = 3; // checa ShinyStashPointer
        } else {
            next_req = map.pos.count ? 2 : 0;
        }
    }

    void sendChain(const char * name, uint16_t size, const MemChain &c) {
        char cmd[160];
        int n = snprintf(cmd, sizeof(cmd), "%s %u", name, (unsigned)size);
        for (uint8_t i = 0; i < c.count && n < (int)sizeof(cmd) - 16; i++)
            n += snprintf(cmd + n, sizeof(cmd) - n, " 0x%X", (unsigned)c.jumps[i]);
        snprintf(cmd + n, sizeof(cmd) - n, "\r\n");
        sendRaw(cmd);
    }

    void sendPartySlotChain(uint8_t slot, bool is_extra) {
        MemChain c = map.party;
        if (c.count >= 6) {
            c.jumps[3] = map.party.jumps[3] + (uint32_t)slot * 8u;   // 0xF0, 0xF8, 0x100, 0x108, 0x110, 0x118
            c.jumps[5] = is_extra ? 0x50u : 0x30u;                   // +0x30 = bloco 0x148, +0x50 = extensao 0x10
        }
        sendChain("pointerPeek", is_extra ? 16 : map.party_stride, c);
    }

    void sendPartySlotPoke(uint8_t slot, const uint8_t * raw_bytes, uint16_t len, bool is_extra = false) {
        if (!isConnected() || slot >= 6 || map.party.count < 6) return;
        static char cmd[760];
        int n = snprintf(cmd, sizeof(cmd), "pointerPoke 0x");
        static const char HEX_DIGITS[] = "0123456789ABCDEF";
        for (uint16_t i = 0; i < len && n + 2 < (int)sizeof(cmd); i++) {
            cmd[n++] = HEX_DIGITS[(raw_bytes[i] >> 4) & 0x0F];
            cmd[n++] = HEX_DIGITS[raw_bytes[i] & 0x0F];
        }
        MemChain c = map.party;
        c.jumps[3] = map.party.jumps[3] + (uint32_t)slot * 8u;
        c.jumps[5] = is_extra ? 0x50u : 0x30u;
        for (uint8_t i = 0; i < c.count && n < (int)sizeof(cmd) - 16; i++)
            n += snprintf(cmd + n, sizeof(cmd) - n, " 0x%X", (unsigned)c.jumps[i]);
        snprintf(cmd + n, sizeof(cmd) - n, "\r\n");
        sendRaw(cmd);
    }

    void sendPositionPoke(float wx, float wy_alt, float wz_map) {
        if (!isConnected() || map.pos.count == 0) return;
        uint8_t raw12[12];
        memcpy(raw12 + 0, &wx, 4);
        memcpy(raw12 + 4, &wy_alt, 4);
        memcpy(raw12 + 8, &wz_map, 4);
        char cmd[180];
        int n = snprintf(cmd, sizeof(cmd), "pointerPoke 0x");
        static const char HEX_DIGITS[] = "0123456789ABCDEF";
        for (int i = 0; i < 12; i++) {
            cmd[n++] = HEX_DIGITS[(raw12[i] >> 4) & 0x0F];
            cmd[n++] = HEX_DIGITS[raw12[i] & 0x0F];
        }
        for (uint8_t i = 0; i < map.pos.count && n < (int)sizeof(cmd) - 16; i++)
            n += snprintf(cmd + n, sizeof(cmd) - n, " 0x%X", (unsigned)map.pos.jumps[i]);
        snprintf(cmd + n, sizeof(cmd) - n, "\r\n");
        sendRaw(cmd);
    }

    void applyAutoTitle(GameState &state, const char * tid) {
        if (!tid || strlen(tid) < 16) return;
        if (strncasecmp(tid, "01007EF00011E000", 16) == 0) {
            state.profile = GameProfile::ZELDA_BOTW;
        } else if (strncasecmp(tid, "0100F43008C44000", 16) == 0) {
            state.profile = GameProfile::POKEMON_LEGENDS_ZA;
        } else if (strncasecmp(tid, "0100554023408000", 16) == 0 ||
                   strncasecmp(tid, "010034D02340E000", 16) == 0) {
            state.profile = GameProfile::POKEMON_FIRERED;
        } else if (strncasecmp(tid, "0100F2C0115B6000", 16) == 0) {
            state.profile = GameProfile::ZELDA_TOTK;
        }
    }

    void requestNext(GameState &state) {
        // Checa o TitleID periodicamente para trocar de jogo automaticamente
        bool za = (state.profile == GameProfile::POKEMON_LEGENDS_ZA);
        if (!za || (map.party.count == 0 && map.pos.count == 0) || (++title_poll_cnt % 60 == 1)) {
            sendRaw("getTitleID\r\n"); awaiting_title = true; awaiting = true; sent_ms = millis(); return;
        }
        awaiting_title = false;
        if ((next_req == 0 || next_req == 1) && map.party.count) {
            sendPartySlotChain(party_idx, next_req == 1);
        } else if (next_req == 3 && map.party.count) {
            // ShinyStashPointer: [0x610A710, 0x120, 0x168, 0x0] (328 bytes)
            MemChain stash = { 4, { map.party.jumps[0], 0x120, 0x168, 0x0 } };
            sendChain("pointerPeek", 328, stash);
        } else if (map.pos.count) {
            next_req = 2;
            sendChain("pointerPeek", 12, map.pos);
        } else {
            next_req = 0;
            sendPartySlotChain(party_idx, false);
        }
        awaiting = true; sent_ms = millis();
    }

    void handleReply(GameState &state) {
        if (awaiting_title) {
            awaiting_title = false;
            applyAutoTitle(state, rx);
            if (!telemetry_only) state.connected = true;
            if (state.profile != GameProfile::POKEMON_LEGENDS_ZA) setLink(state, "ok (auto)");
            return;
        }
        bool za = (state.profile == GameProfile::POKEMON_LEGENDS_ZA);
        if (!za || (map.party.count == 0 && map.pos.count == 0)) {
            if (!telemetry_only) state.connected = true;
            setLink(state, "ok (sem offsets)");
            return;
        }
        static uint8_t buf[RX_MAX / 2];
        uint16_t n = hexToBytes(rx, buf, sizeof(buf));
        if (n == 0) {
            if ((next_req == 0 || next_req == 1) && map.party.count && party_idx < 6) {
                slot_valid[party_idx] = false;
                clearSlot(state.party[party_idx]);
                advancePartySlot();
            }
            setLink(state, "resposta invalida");
            return;
        }

        if (next_req == 0 && map.party.count) {
            static uint8_t claro[328];
            uint32_t ec = 0;
            if (n >= 328 && party_idx < 6 && pa9DecryptBlock(buf, claro, ec)) {
                memcpy(slot_claro[party_idx], claro, 328);
                slot_valid[party_idx] = true;
                pending_ec = ec;
                pending_species = rd16(claro + map.off_species);
                pending_hp_cur  = rd16(claro + map.off_hp_cur);
                pending_status  = claro[map.off_status];
                pa9ExtractMeta(claro, pending_item, pending_pp, pending_nature, pending_ev_total,
                               pending_shiny, pending_max_iv, pending_alpha);
                pa9ExtractName(claro, pending_species, pending_name, sizeof(pending_name));

                PokemonSlot &p = state.party[party_idx];
                bool same_poke = (slot_ec[party_idx] == ec && p.max_hp > 0 && p.level > 0);
                slot_ec[party_idx] = ec;

                if (same_poke && ++slot_ext_age[party_idx] < 6) {
                    // Otimizacao de velocidade: mesmo Pokemon no slot -> atualiza HP/Status/Item/PP/Shiny na hora
                    // sem precisar fazer um 2o pointerPeek para nivel/HP_max em todo ciclo!
                    p.species = pending_species;
                    p.current_hp = pending_hp_cur > p.max_hp ? p.max_hp : pending_hp_cur;
                    p.held_item = pending_item;
                    for (int m = 0; m < 4; m++) p.pp[m] = pending_pp[m];
                    p.nature = pending_nature;
                    p.ev_total = pending_ev_total;
                    p.is_shiny = pending_shiny;
                    p.is_max_iv = pending_max_iv;
                    p.is_alpha = pending_alpha;
                    p.active = true;
                    snprintf(p.name, sizeof(p.name), "%s", pending_name);
                    snprintf(p.status, sizeof(p.status), "%s", statusName(pending_status));
                    advancePartySlot();
                } else {
                    slot_ext_age[party_idx] = 0;
                    next_req = 1;   // pede a extensao 0x10 do mesmo slot (nivel e HP maximo)
                }
            } else {
                if (party_idx < 6) {
                    slot_valid[party_idx] = false;
                    slot_ec[party_idx] = 0;
                    clearSlot(state.party[party_idx]);
                }
                advancePartySlot();
            }
        } else if (next_req == 1 && map.party.count) {
            if (n >= 16 && party_idx < 6) {
                pa9Xor(buf, 16, pending_ec);
                memcpy(slot_extra[party_idx], buf, 16);
                uint16_t off_lv = map.off_level >= 0x148 ? (uint16_t)(map.off_level - 0x148) : 0;
                uint16_t off_hm = map.off_hp_max >= 0x148 ? (uint16_t)(map.off_hp_max - 0x148) : 2;
                uint8_t  lv    = (off_lv < 16) ? buf[off_lv] : 0;
                uint16_t hpmax = (off_hm + 1 < 16) ? rd16(buf + off_hm) : 0;
                PokemonSlot &p = state.party[party_idx];
                if (hpmax == 0 || hpmax > 9999 || lv == 0 || lv > 100) {
                    slot_valid[party_idx] = false;
                    clearSlot(p);
                } else {
                    p.species = pending_species;
                    p.max_hp = hpmax;
                    p.current_hp = pending_hp_cur > hpmax ? hpmax : pending_hp_cur;
                    p.level = lv;
                    p.held_item = pending_item;
                    for (int m = 0; m < 4; m++) p.pp[m] = pending_pp[m];
                    p.nature = pending_nature;
                    p.ev_total = pending_ev_total;
                    p.is_shiny = pending_shiny;
                    p.is_max_iv = pending_max_iv;
                    p.is_alpha = pending_alpha;
                    p.active = true;
                    snprintf(p.name, sizeof(p.name), "%s", pending_name);
                    snprintf(p.status, sizeof(p.status), "%s", statusName(pending_status));
                }
            } else if (party_idx < 6) {
                slot_valid[party_idx] = false;
                clearSlot(state.party[party_idx]);
            }
            advancePartySlot();
        } else if (next_req == 3) {
            static uint8_t claro_st[328];
            uint32_t ec_st = 0;
            if (n >= 328 && pa9DecryptBlock(buf, claro_st, ec_st)) {
                uint16_t sp = rd16(claro_st + map.off_species);
                state.shiny_stash_found = (sp > 0);
                state.shiny_stash_species = sp;
                pa9ExtractName(claro_st, sp, state.shiny_stash_name, sizeof(state.shiny_stash_name));
            } else {
                state.shiny_stash_found = false;
                state.shiny_stash_species = 0;
                state.shiny_stash_name[0] = 0;
            }
            next_req = map.pos.count ? 2 : 0;
        } else {
            if (n >= 12) {
                state.map.x = rdf(buf + map.off_x);
                state.map.y = rdf(buf + map.off_z);   // no mapa 2D: horizontal = x, vertical = z do jogo
                state.map.z = rdf(buf + map.off_y);
                snprintf(state.map.zone_name, sizeof(state.map.zone_name), "%s",
                         lumioseZoneName(state.map.x, state.map.y, state.map.z));
            }
            next_req = map.party.count ? 0 : 2;
        }
        if (!telemetry_only) state.connected = true;   // em telemetria, 'connected' = USB HID
        setLink(state, "ok (lendo RAM)");
    }

    void scanStep(GameState &state) {
        char ip[16];
        snprintf(ip, sizeof(ip), "%.12s%d", scan_prefix, scan_host);
        uint32_t t0 = millis();
        bool ok = connectToSwitch(ip, state.switch_port, TIMEOUT_BUSCA_MS);
        if (ok) {
            snprintf(state.switch_ip, sizeof(state.switch_ip), "%s", ip);
            scan_state = 2;
            setLink(state, "switch encontrado");
            last_poll_ms = millis();
            return;
        }
        // Diagnostico na tela: qual endereco falhou e em quanto tempo (0 ms = falhou sem esperar)
        char txt[40];
        snprintf(txt, sizeof(txt), "busca .%d falhou em %lums", scan_host, (unsigned long)(millis() - t0));
        setLink(state, txt);
        if (++scan_host > scan_last) { scan_state = 3; setLink(state, "switch nao encontrado"); }
    }

    // Um passo da maquina de estados (chamar sempre; nunca bloqueia mais que o connect)
    void pump(GameState &state) {
        uint32_t now = millis();

        if (scan_state == 1) { scanStep(state); return; }   // 1 endereco por chamada (ate TIMEOUT_BUSCA_MS)

        if (!isConnected()) {
            if (now - last_try_ms >= 5000) {             // tenta reconectar a cada 5 s (connect bloqueia ate 150 ms)
                last_try_ms = now;
                uint32_t t0 = millis();
                linked = connectToSwitch(state.switch_ip, state.switch_port);
                if (linked) setLink(state, "conectado");
                else {
                    char txt[40];
                    snprintf(txt, sizeof(txt), "sem conexao (%lums)", (unsigned long)(millis() - t0));
                    setLink(state, txt);
                }
                if (!linked && !telemetry_only) state.connected = false;
#ifdef ARDUINO
                if (!linked && WiFi.status() == WL_CONNECTED &&
                    (last_auto_scan_ms == 0 || now - last_auto_scan_ms >= 30000)) {
                    last_auto_scan_ms = now;
                    IPAddress lip = WiFi.localIP();
                    char prefix[20];
                    snprintf(prefix, sizeof(prefix), "%d.%d.%d.", lip[0], lip[1], lip[2]);
                    startScan(state, prefix, 1, 254);
                }
#endif
            }
            return;
        }

        if (awaiting) {
            if (readLine()) { awaiting = false; handleReply(state); rx_len = 0; last_poll_ms = now; }
            else if (now - sent_ms > 800) { awaiting = false; rx_len = 0; next_req = 0; setLink(state, "timeout"); }
            return;
        }
        if (now - last_poll_ms >= 20) requestNext(state);    // ~35-40 leituras/s (posicao ~20Hz + time sem lag)
    }

    void applySlotEdit(GameState &state, uint8_t s, PokeEditAction action, uint16_t item_id) {
        if (s >= 6) return;
        PokemonSlot &p = state.party[s];
        if (p.max_hp == 0) return;

        if (action == PokeEditAction::HEAL_HP || action == PokeEditAction::FULL_RESTORE) {
            p.current_hp = p.max_hp;
        }
        if (action == PokeEditAction::HEAL_STATUS || action == PokeEditAction::FULL_RESTORE) {
            p.status[0] = 0;
        }
        if (action == PokeEditAction::RESTORE_PP || action == PokeEditAction::FULL_RESTORE) {
            for (int m = 0; m < 4; m++) p.pp[m] = 40;
        }
        if (action == PokeEditAction::TOGGLE_SHINY) {
            p.is_shiny = !p.is_shiny;
        }
        if (action == PokeEditAction::MAX_IVS) {
            p.is_max_iv = true;
        }
        if (action == PokeEditAction::SET_ITEM) {
            p.held_item = item_id;
        }
        if (action == PokeEditAction::LEVEL_PLUS_1 || action == PokeEditAction::LEVEL_PLUS_10 || action == PokeEditAction::LEVEL_100) {
            uint8_t old_lv = p.level ? p.level : 1;
            uint8_t new_lv = (action == PokeEditAction::LEVEL_PLUS_1)  ? (old_lv < 100 ? old_lv + 1 : 100) :
                             (action == PokeEditAction::LEVEL_PLUS_10) ? (old_lv <= 90 ? old_lv + 10 : 100) : 100;
            uint16_t new_max = (uint16_t)((uint32_t)p.max_hp * new_lv / old_lv);
            if (new_max <= p.max_hp && new_lv > old_lv) new_max = (uint16_t)(p.max_hp + (new_lv - old_lv) * 2u);
            p.level = new_lv;
            p.max_hp = new_max;
            p.current_hp = new_max;
        }
        if (action == PokeEditAction::EV_ATK_SPE || action == PokeEditAction::EV_SPA_SPE || action == PokeEditAction::EV_HP_TANK) {
            p.ev_total = 510;
        }
        if (action == PokeEditAction::EV_RESET) {
            p.ev_total = 0;
        }
        if (action == PokeEditAction::SET_NATURE) {
            p.nature = (uint8_t)(item_id & 0xFF);
        }
        if (action == PokeEditAction::TOGGLE_ALPHA) {
            p.is_alpha = !p.is_alpha;
        }

        if (!slot_valid[s] || !isConnected()) return;

        uint8_t * claro = slot_claro[s];
        bool poke_extra = false;
        if (action == PokeEditAction::HEAL_HP || action == PokeEditAction::FULL_RESTORE) {
            wr16(claro + map.off_hp_cur, p.max_hp);
        }
        if (action == PokeEditAction::HEAL_STATUS || action == PokeEditAction::FULL_RESTORE) {
            wr32(claro + map.off_status, 0);
        }
        if (action == PokeEditAction::RESTORE_PP || action == PokeEditAction::FULL_RESTORE) {
            for (int m = 0; m < 4; m++) {
                uint16_t mv = rd16(claro + 0x72 + m * 2);
                if (mv != 0) claro[0x7A + m] = 40;
            }
        }
        if (action == PokeEditAction::TOGGLE_SHINY) {
            uint16_t tid = rd16(claro + 0x0C);
            uint16_t sid = rd16(claro + 0x0E);
            uint32_t pid = rd32(claro + 0x1C);
            uint16_t p_lo = (uint16_t)(pid & 0xFFFFu);
            uint16_t p_hi = p.is_shiny ? (uint16_t)(tid ^ sid ^ p_lo) : (uint16_t)(tid ^ sid ^ p_lo ^ 0x0100u);
            wr32(claro + 0x1C, ((uint32_t)p_hi << 16) | p_lo);
        }
        if (action == PokeEditAction::MAX_IVS) {
            uint32_t iv32 = rd32(claro + 0x8C);
            wr32(claro + 0x8C, (iv32 & 0xC0000000u) | 0x3FFFFFFFu);
        }
        if (action == PokeEditAction::SET_ITEM) {
            wr16(claro + 0x0A, item_id);
        }
        if (action == PokeEditAction::LEVEL_PLUS_1 || action == PokeEditAction::LEVEL_PLUS_10 || action == PokeEditAction::LEVEL_100) {
            uint32_t lv3 = (uint32_t)p.level * (uint32_t)p.level * (uint32_t)p.level;
            wr32(claro + 0x10, lv3);
            wr16(claro + map.off_hp_cur, p.max_hp);
            uint16_t off_lv = map.off_level >= 0x148 ? (uint16_t)(map.off_level - 0x148) : 0;
            uint16_t off_hm = map.off_hp_max >= 0x148 ? (uint16_t)(map.off_hp_max - 0x148) : 2;
            if (off_lv < 16) slot_extra[s][off_lv] = p.level;
            if (off_hm + 1 < 16) wr16(slot_extra[s] + off_hm, p.max_hp);
            poke_extra = true;
        }
        if (action == PokeEditAction::EV_ATK_SPE) {
            const uint8_t evs[6] = {6, 252, 0, 252, 0, 0}; // HP, ATK, DEF, SPE, SPA, SPD
            memcpy(claro + 0x26, evs, 6);
        }
        if (action == PokeEditAction::EV_SPA_SPE) {
            const uint8_t evs[6] = {6, 0, 0, 252, 252, 0};
            memcpy(claro + 0x26, evs, 6);
        }
        if (action == PokeEditAction::EV_HP_TANK) {
            const uint8_t evs[6] = {252, 0, 128, 0, 0, 128};
            memcpy(claro + 0x26, evs, 6);
        }
        if (action == PokeEditAction::EV_RESET) {
            memset(claro + 0x26, 0, 6);
        }
        if (action == PokeEditAction::SET_NATURE) {
            claro[0x20] = (uint8_t)(item_id & 0xFF);
            claro[0x21] = (uint8_t)(item_id & 0xFF);
        }
        if (action == PokeEditAction::TOGGLE_ALPHA) {
            claro[0x23] = p.is_alpha ? 1 : 0;
            claro[0x4A] = p.is_alpha ? 255 : 128;
            claro[0x4B] = p.is_alpha ? 255 : 128;
        }

        static uint8_t raw_out[328];
        pa9EncryptBlock(claro, raw_out);
        sendPartySlotPoke(s, raw_out, 328, false);
        if (poke_extra) {
            uint8_t raw_ext[16];
            memcpy(raw_ext, slot_extra[s], 16);
            pa9Xor(raw_ext, 16, slot_ec[s]);
            sendPartySlotPoke(s, raw_ext, 16, true);
        }
    }

public:
    ~SysBotbaseProvider() override { disconnect(); }

    void editPokemon(GameState &state, uint8_t slot, PokeEditAction action, uint16_t item_id = 0) {
        if (action == PokeEditAction::HEAL_PARTY_ALL) {
            for (uint8_t i = 0; i < 6; i++) {
                applySlotEdit(state, i, PokeEditAction::FULL_RESTORE, 0);
            }
            setLink(state, "time curado (RAM)");
            return;
        }
        applySlotEdit(state, slot, action, item_id);
        setLink(state, "pokemon atualizado (RAM)");
    }

    void teleportPlayer(GameState &state, float wx, float wy_alt, float wz_map) {
        state.map.x = wx;
        state.map.y = wz_map;
        state.map.z = wy_alt;
        snprintf(state.map.zone_name, sizeof(state.map.zone_name), "%s",
                 lumioseZoneName(wx, wz_map, wy_alt));
        sendPositionPoke(wx, wy_alt, wz_map);
        setLink(state, "teleporte enviado!");
    }

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
        party_idx = 0;
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