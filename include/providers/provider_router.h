#ifndef PROVIDER_ROUTER_H
#define PROVIDER_ROUTER_H

#include "../core/i_game_provider.h"
#include "mock_provider.h"
#include "usb_hid_provider.h"
#include "sysbotbase_provider.h"

class ProviderRouter : public IGameProvider {
private:
    MockProvider mock_prov;
    UsbHidProvider usb_prov;
    SysBotbaseProvider tcp_prov;
    IGameProvider * active_prov = &mock_prov;
    SwitchMode boot_mode = SwitchMode::MOCK_SIMULATOR;
    bool initialized = false;
    bool telemetry = false;   // le RAM via sys-botbase em paralelo ao provider ativo

public:
    // Acesso ao provider USB (a API web le o slot ativo daqui; evita uma 2a instancia)
    UsbHidProvider & usb() { return usb_prov; }

    // Modo em que o router sobe (ex.: LOCKED_USB_MACRO no Pico W real)
    void setBootMode(SwitchMode m) { boot_mode = m; }

    // Liga a leitura de RAM (party/posicao) sem trocar o provider ativo: o USB HID segue com as macros
    void startTelemetry(GameState &state) {
        if (active_prov == &tcp_prov) return;   // ja e o provider ativo
        telemetry = true;
        tcp_prov.startTelemetry(state);
    }

    // IP/porta do Switch definidos pela pagina web (sem reiniciar). Liga a telemetria se ainda nao estava.
    void setSwitchTarget(GameState &state, const char * ip, uint16_t port) {
        snprintf(state.switch_ip, sizeof(state.switch_ip), "%s", ip);
        state.switch_port = port;
        if (active_prov != &tcp_prov && !telemetry) { telemetry = true; tcp_prov.startTelemetry(state); }
        tcp_prov.retarget(state);
    }
    // Procura o Switch na rede ("192.168.15." -> .1 ate .254)
    void scanForSwitch(GameState &state, const char * prefix) {
        telemetry = true;
        tcp_prov.startScan(state, prefix);
    }
    uint8_t scanState() const { return tcp_prov.scanState(); }
    int     scanProgress() const { return tcp_prov.scanProgress(); }

    // Parar tudo (macros USB). O sys-botbase nao envia botoes, entao nao ha o que cancelar nele.
    void stopAll(GameState &state) { usb_prov.stopAll(state); }

    // Acoes do Mini Menu do Pokemon (Curar HP, Status, PP, Item, Shiny, 6IVs, Curar Time, Level, EVs, Nature, Alpha)
    void editPokemon(GameState &state, uint8_t slot, PokeEditAction action, uint16_t item_id = 0) {
        tcp_prov.editPokemon(state, slot, action, item_id);
    }

    // Teleporte rapido no Mapa (escreve X, Y_alt, Z_map via pointerPoke no Switch)
    void teleportPlayer(GameState &state, float wx, float wy_alt, float wz_map) {
        tcp_prov.teleportPlayer(state, wx, wy_alt, wz_map);
    }

    void switchMode(GameState &state, SwitchMode new_mode) {
        state.mode = new_mode;
        if (new_mode == SwitchMode::MOCK_SIMULATOR) {
            active_prov = &mock_prov;
        } else {
            // Limpa os Pokemon falsos do Mock para exibir "--- / sem dados" ate a leitura real da RAM
            for (int i = 0; i < 6; i++) {
                state.party[i] = PokemonSlot{};
                state.party[i].active = false;
            }
            if (new_mode == SwitchMode::LOCKED_USB_MACRO) {
                active_prov = &usb_prov;
            } else {
                active_prov = &tcp_prov;
            }
        }
        active_prov->init(state);
    }

    void init(GameState &state) override {
        if (initialized) return;   // idempotente: main e UIManager podem chamar
        initialized = true;
        mock_prov.init(state); // Inicializa dados base
        switchMode(state, boot_mode);
    }

    void setProfile(GameState &state, GameProfile new_profile) override {
        // Atualiza os waypoints no mock base e avisa o provedor ativo
        mock_prov.setProfile(state, new_profile);
        if (active_prov != &mock_prov) {
            active_prov->setProfile(state, new_profile);
        }
    }

    unsigned long last_eval_ms = 0;

    void update(GameState &state) override {
        active_prov->update(state);
        if (telemetry && active_prov != &tcp_prov) tcp_prov.pollTelemetry(state);
        // FireRed: avalia conquistas ~1x por segundo
        if (state.profile == GameProfile::POKEMON_FIRERED) {
            unsigned long now = millis();
            if (now - last_eval_ms >= 1000) { last_eval_ms = now; state.fr.evaluate(); }
        }
    }

    void triggerAction(ActionMacro action, GameState &state) override {
        active_prov->triggerAction(action, state);
    }
};

#endif // PROVIDER_ROUTER_H