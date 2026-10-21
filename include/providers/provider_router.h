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

public:
    void switchMode(GameState &state, SwitchMode new_mode) {
        state.mode = new_mode;
        if (new_mode == SwitchMode::MOCK_SIMULATOR) {
            active_prov = &mock_prov;
        } else if (new_mode == SwitchMode::LOCKED_USB_MACRO) {
            active_prov = &usb_prov;
        } else {
            active_prov = &tcp_prov;
        }
        active_prov->init(state);
    }

    void init(GameState &state) override {
        mock_prov.init(state); // Inicializa dados base
        switchMode(state, SwitchMode::MOCK_SIMULATOR);
    }

    void setProfile(GameState &state, GameProfile new_profile) override {
        // Atualiza os waypoints no mock base e avisa o provedor ativo
        mock_prov.setProfile(state, new_profile);
        if (active_prov != &mock_prov) {
            active_prov->setProfile(state, new_profile);
        }
    }

    void update(GameState &state) override {
        active_prov->update(state);
    }

    void triggerAction(ActionMacro action, GameState &state) override {
        active_prov->triggerAction(action, state);
    }
};

#endif // PROVIDER_ROUTER_H