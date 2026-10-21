#ifndef GAME_STATE_H
#define GAME_STATE_H

#include <stdint.h>

enum class SwitchMode : uint8_t {
    MOCK_SIMULATOR = 0,
    SIMULATOR_MOCK = 0, // Alias
    LOCKED_USB_MACRO = 1,
    UNLOCKED_SYSBOTBASE = 2
};

enum class GameProfile : uint8_t {
    POKEMON_SV = 0,
    POKEMON_LEGENDS_ZA = 1,
    POKEMON_LEGENDS_ARCEUS = 2,
    ZELDA_TOTK = 3,
    ZELDA_BOTW = 4,
    MARIOKART_8 = 5,
    MARIOKART_WORLD = 6,
    GENERIC_HUB = 7,
    COUNT = 8
};

enum class ActionMacro : uint8_t {
    MACRO_SLOT_1 = 0,
    MACRO_SLOT_2 = 1,
    MACRO_SLOT_3 = 2,
    MACRO_SLOT_4 = 3,
    MACRO_SLOT_5 = 4,
    MACRO_SLOT_6 = 5,
    MACRO_1 = 0, // Alias
    MACRO_2 = 1, // Alias
    MACRO_3 = 2, // Alias
    MACRO_4 = 3, // Alias
    MACRO_5 = 4, // Alias
    MACRO_6 = 5  // Alias
};

struct PokemonSlot {
    char name[16];
    uint8_t level;
    uint16_t current_hp;
    uint16_t max_hp;
    char status[8];
    bool active = true;
};

struct ZeldaStats {
    uint8_t hearts_current;
    uint8_t hearts_max;
    uint8_t stamina_pct;
    char equipped_weapon[24];
    uint8_t weapon_durability_pct;
    char equipped_bow[24];
    uint8_t bow_durability_pct;
    uint32_t rupees;
};

struct GenericStats {
    char active_profile_name[24];
    uint32_t macro_executions;
    uint32_t session_Minutes;
    bool turbo_enabled;
};

struct MapWaypoint {
    float x;
    float y;
    char label[20];
    uint32_t color_hex;
};

struct MapRadar {
    char zone_name[28];
    float x;
    float y;
    float z;
    MapWaypoint waypoints[3];
};

struct GameState {
    SwitchMode mode = SwitchMode::MOCK_SIMULATOR;
    GameProfile profile = GameProfile::POKEMON_SV;
    bool connected = false;
    char switch_ip[16] = "192.168.1.50";
    uint16_t switch_port = 6000;

    PokemonSlot party[6];
    ZeldaStats zelda;
    GenericStats generic;
    MapRadar map;
};

#endif // GAME_STATE_H
