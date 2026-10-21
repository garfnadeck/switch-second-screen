#ifndef MOCK_PROVIDER_H
#define MOCK_PROVIDER_H

#include "../core/i_game_provider.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

class MockProvider : public IGameProvider {
private:
    uint32_t tick_counter = 0;
    float angle = 0.0f;

public:
    void init(GameState &state) override {
        tick_counter = 0;
        state.mode = SwitchMode::MOCK_SIMULATOR;
        state.connected = true;
        snprintf(state.switch_ip, sizeof(state.switch_ip), "192.168.1.50");

        const char* names[6] = {"Garchomp", "Gholdengo", "Flutter Mane", "Incineroar", "Urshifu", "Ogerpon"};
        uint8_t levels[6]    = {78, 75, 76, 72, 74, 77};
        uint16_t max_hps[6]  = {280, 240, 210, 260, 275, 250};
        const char* stats[6] = {"OK", "OK", "PAR", "OK", "BRN", "OK"};

        for (int i = 0; i < 6; i++) {
            snprintf(state.party[i].name, sizeof(state.party[i].name), "%s", names[i]);
            state.party[i].level = levels[i];
            state.party[i].max_hp = max_hps[i];
            state.party[i].current_hp = max_hps[i];
            snprintf(state.party[i].status, sizeof(state.party[i].status), "%s", stats[i]);
            state.party[i].active = true;
        }

        state.zelda.hearts_max = 20;
        state.zelda.hearts_current = 16;
        state.zelda.stamina_pct = 100;
        snprintf(state.zelda.equipped_weapon, sizeof(state.zelda.equipped_weapon), "Master Sword +45");
        state.zelda.weapon_durability_pct = 85;
        snprintf(state.zelda.equipped_bow, sizeof(state.zelda.equipped_bow), "Savage Lynel Bow x5");
        state.zelda.bow_durability_pct = 62;
        state.zelda.rupees = 4850;

        setProfile(state, GameProfile::POKEMON_SV);
    }

    void setProfile(GameState &state, GameProfile new_profile) override {
        state.profile = new_profile;
        if (new_profile == GameProfile::POKEMON_SV) {
            snprintf(state.map.zone_name, sizeof(state.map.zone_name), "Paldea - Area Zero");
            state.map.waypoints[0] = {45.0f,  50.0f,  "Tera Raid 6*", 0x00F5D4};
            state.map.waypoints[1] = {210.0f, 70.0f,  "Shiny Outbreak", 0xFFEA00};
            state.map.waypoints[2] = {150.0f, 190.0f, "Station #3", 0x00E676};
        } else if (new_profile == GameProfile::ZELDA_TOTK) {
            snprintf(state.map.zone_name, sizeof(state.map.zone_name), "Hyrule - Central");
            state.map.waypoints[0] = {60.0f,  175.0f, "Shrine Kyononis", 0x00B0FF};
            state.map.waypoints[1] = {200.0f, 55.0f,  "Korok Seed #42", 0x00E676};
            state.map.waypoints[2] = {120.0f, 110.0f, "Gleeok Boss", 0xFF1744};
        } else {
            snprintf(state.map.zone_name, sizeof(state.map.zone_name), "Switch System Mode");
            state.map.waypoints[0] = {80.0f,  80.0f,  "WP Alpha", 0x00F5D4};
            state.map.waypoints[1] = {180.0f, 120.0f, "WP Beta", 0xFFEA00};
            state.map.waypoints[2] = {130.0f, 180.0f, "WP Gamma", 0x00E676};
        }
    }

    void update(GameState &state) override {
        tick_counter++;
        angle += 0.035f;
        state.map.x = 130.0f + cosf(angle) * 85.0f;
        state.map.y = 115.0f + sinf(angle * 2.0f) * 55.0f;
        state.map.z = (state.profile == GameProfile::ZELDA_TOTK) ? (120.0f + sinf(angle) * 45.0f) : 0.0f;

        if (tick_counter % 12 == 0) {
            if (state.profile == GameProfile::POKEMON_SV) {
                if (state.party[0].current_hp > 30) state.party[0].current_hp -= 10;
                else state.party[0].current_hp = state.party[0].max_hp;
            } else if (state.profile == GameProfile::ZELDA_TOTK) {
                if (state.zelda.weapon_durability_pct > 15) state.zelda.weapon_durability_pct -= 3;
                else state.zelda.weapon_durability_pct = 100;
                if (state.zelda.stamina_pct > 20) state.zelda.stamina_pct -= 5;
                else state.zelda.stamina_pct = 100;
            }
        }
    }

    void triggerAction(ActionMacro action, GameState &state) override {
        if (state.profile == GameProfile::POKEMON_SV && action == ActionMacro::MACRO_SLOT_1) {
            for (int i = 0; i < 6; i++) {
                state.party[i].current_hp = state.party[i].max_hp;
                snprintf(state.party[i].status, sizeof(state.party[i].status), "OK");
            }
        } else if (state.profile == GameProfile::ZELDA_TOTK) {
            if (action == ActionMacro::MACRO_SLOT_1) {
                state.zelda.hearts_current = state.zelda.hearts_max;
                state.zelda.stamina_pct = 100;
            } else if (action == ActionMacro::MACRO_SLOT_2) {
                state.zelda.weapon_durability_pct = 100;
                state.zelda.bow_durability_pct = 100;
            }
        }
    }
};

#endif // MOCK_PROVIDER_H