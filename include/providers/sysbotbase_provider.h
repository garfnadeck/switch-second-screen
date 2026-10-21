#ifndef SYSBOTBASE_PROVIDER_H
#define SYSBOTBASE_PROVIDER_H

#include "../core/i_game_provider.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef ARDUINO
    #include <WiFi.h>
#else
    #include <sys/socket.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <errno.h>
#endif

class SysBotbaseProvider : public IGameProvider {
private:
    #ifdef ARDUINO
        WiFiClient client;
    #else
        int sock_fd = -1;
    #endif
    uint32_t poll_divider = 0;
    char rx_buf[256];

    bool connectToSwitch(const char* ip, uint16_t port = 6000) {
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

            fcntl(sock_fd, F_SETFL, O_NONBLOCK);

            struct sockaddr_in serv_addr;
            memset(&serv_addr, 0, sizeof(serv_addr));
            serv_addr.sin_family = AF_INET;
            serv_addr.sin_port = htons(port);
            inet_pton(AF_INET, ip, &serv_addr.sin_addr);

            connect(sock_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
            printf("[SysBotbase] Conectando ao Switch em %s:%d...\n", ip, port);
            return true;
        #endif
    }

    void disconnect() {
        #ifdef ARDUINO
            client.stop();
        #else
            if (sock_fd >= 0) {
                close(sock_fd);
                sock_fd = -1;
            }
        #endif
    }

    void sendCommand(const char* cmd) {
        #ifdef ARDUINO
            if (client.connected()) client.print(cmd);
        #else
            if (sock_fd >= 0) send(sock_fd, cmd, strlen(cmd), 0);
        #endif
    }

    void readIncomingData(GameState &state) {
        int bytes_read = 0;
        #ifdef ARDUINO
            if (client.connected() && client.available()) {
                bytes_read = client.readBytesUntil('\n', rx_buf, sizeof(rx_buf) - 1);
            }
        #else
            if (sock_fd >= 0) {
                bytes_read = (int)recv(sock_fd, rx_buf, sizeof(rx_buf) - 1, 0);
            }
        #endif

        if (bytes_read > 0) {
            rx_buf[bytes_read] = '\0';
            state.connected = true;

            // Protocolo de resposta simulado/decodificado da RAM:
            // Formato: "TELEM:x,y,z,val1,val2,val3"
            float x = 0, y = 0, z = 0;
            int v1 = 0, v2 = 0, v3 = 0;
            if (sscanf(rx_buf, "TELEM:%f,%f,%f,%d,%d,%d", &x, &y, &z, &v1, &v2, &v3) == 6) {
                state.map.x = x;
                state.map.y = y;
                state.map.z = z;

                if (state.profile == GameProfile::POKEMON_SV) {
                    state.party[0].current_hp = (uint16_t)v1;
                    state.party[1].current_hp = (uint16_t)v2;
                } else if (state.profile == GameProfile::ZELDA_TOTK) {
                    state.zelda.hearts_current = (uint8_t)v1;
                    state.zelda.stamina_pct = (uint8_t)v2;
                    state.zelda.weapon_durability_pct = (uint8_t)v3;
                }
            }
        }
    }

public:
    ~SysBotbaseProvider() override {
        disconnect();
    }

    void init(GameState &state) override {
        state.mode = SwitchMode::UNLOCKED_SYSBOTBASE;
        state.connected = connectToSwitch(state.switch_ip);
    }

    void setProfile(GameState &state, GameProfile new_profile) override {
        state.profile = new_profile;
        sendCommand("getTitleId\r\n");
    }

    void update(GameState &state) override {
        state.mode = SwitchMode::UNLOCKED_SYSBOTBASE;
        poll_divider++;

        // 1. Lê qualquer resposta que tenha chegado do Switch via Wi-Fi/TCP
        readIncomingData(state);

        // 2. A cada 300ms (3 ticks), solicita nova leitura de RAM ao Switch
        if (poll_divider % 3 == 0) {
            if (state.profile == GameProfile::POKEMON_SV) {
                sendCommand("pointerPeek 24 0x42F30E8 0x18 0x70\r\n");
            } else if (state.profile == GameProfile::ZELDA_TOTK) {
                sendCommand("peekMain 0x0471A2B0 16\r\n");
            } else {
                sendCommand("getTitleId\r\n");
            }
        }
    }

    void triggerAction(ActionMacro action, GameState &state) override {
        char cmd[64];
        snprintf(cmd, sizeof(cmd), "MACRO:%d:%d\r\n",
                 static_cast<int>(state.profile), static_cast<int>(action));
        sendCommand(cmd);
    }
};

#endif // SYSBOTBASE_PROVIDER_H
