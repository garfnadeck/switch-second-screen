#ifndef GAME_STATE_H
#define GAME_STATE_H

#include <stdint.h>
#include "user_macros.h"

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
    POKEMON_FIRERED = 8,
    COUNT = 9
};

// Quantidade total de macros (slots 0..MACRO_COUNT-1). Slots 0..5 = os 6 originais.
// Maximo de macros de qualquer jogo (Z-A usa 17, FireRed usa 25). O significado do slot depende do perfil.
static constexpr uint8_t MACRO_COUNT = 25;
// Valor especial enviado pelos botoes "PARAR TUDO" (cancela fila e toggles, solta os botoes)
static constexpr uint8_t MACRO_STOP = 255;
// Codigos especiais dos botoes do painel FireRed (contador / insignias), tratados pelo UIManager
static constexpr uint8_t FR_CODE_BADGE0  = 240;   // 240..247 = alterna a insignia 0..7
static constexpr uint8_t FR_CODE_ENC_RST = 248;   // zera encontros
static constexpr uint8_t FR_CODE_ENC_DEC = 249;   // -1 encontro
static constexpr uint8_t FR_CODE_ENC_INC = 250;   // +1 encontro

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
    // Slots 6..16 (Z-A) sao enviados como numero: static_cast<ActionMacro>(slot)
};

// ---------- Pokemon FireRed: contadores e conquistas (a parte divertida) ----------
struct FireRedStats {
    uint32_t encounters = 0;     // contador manual de encontros (toque +1/-1)
    uint32_t soft_resets = 0;    // vezes que a macro Soft Reset foi usada
    uint32_t steps_est = 0;      // passos estimados pelas macros de andar/correr
    uint32_t hunt_s = 0;         // segundos com alguma macro ativa
    uint32_t runs = 0;           // macros executadas
    uint8_t  badges = 0;         // bitmask das 8 insignias de Kanto
    uint32_t ach_mask = 0;       // conquistas desbloqueadas
    int8_t   last_ach = -1;      // ultima conquista (para o aviso na tela)
    bool     dirty = false;      // mudou: salvar na placa (com folga de tempo)
    bool     urgent = false;     // mudou por toque: salvar ja

    static constexpr uint8_t ACH_COUNT = 13;
    static const char * achName(uint8_t i) {
        static const char * const n[ACH_COUNT] = {
            "Primeiros Passos", "Mochileiro", "Maratonista", "Cacador Paciente", "Insone",
            "Primeiro Encontro", "Cem Encontros", "Mil Encontros", "Sorte ou Persistencia",
            "Reiniciador", "Primeira Insignia", "Meio Caminho", "Mestre de Kanto" };
        return i < ACH_COUNT ? n[i] : "";
    }
    // Chance acumulada de ter visto ao menos um shiny (Gen 3: 1/8192 por encontro)
    float shinyChance() const {
        float miss = 1.0f, base = 1.0f - 1.0f / 8192.0f;
        uint32_t n = encounters > 40000 ? 40000 : encounters;
        // potencia por quadrados sucessivos (evita depender de powf no firmware)
        while (n) { if (n & 1) miss *= base; base *= base; n >>= 1; }
        return 1.0f - miss;
    }
    uint8_t badgeCount() const { uint8_t c = 0; for (uint8_t i = 0; i < 8; i++) if (badges & (1u << i)) c++; return c; }
    uint8_t achCount() const { uint8_t c = 0; for (uint8_t i = 0; i < ACH_COUNT; i++) if (ach_mask & (1u << i)) c++; return c; }

    // Avalia as conquistas; devolve quantas foram desbloqueadas agora
    uint8_t evaluate() {
        bool ok[ACH_COUNT] = {
            runs >= 1, steps_est >= 1000, steps_est >= 10000, hunt_s >= 3600, hunt_s >= 18000,
            encounters >= 1, encounters >= 100, encounters >= 1000, shinyChance() >= 0.5f,
            soft_resets >= 1, badgeCount() >= 1, badgeCount() >= 4, badgeCount() >= 8 };
        uint8_t fresh = 0;
        for (uint8_t i = 0; i < ACH_COUNT; i++) {
            if (ok[i] && !(ach_mask & (1u << i))) { ach_mask |= (1u << i); last_ach = (int8_t)i; fresh++; dirty = true; urgent = true; }
        }
        return fresh;
    }
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
    int8_t active_macro_slot = -1; // -1 = nenhuma macro em execucao
    char link_status[24] = "desativado";   // estado da leitura de RAM (sys-botbase)
    char switch_ip[16] = "192.168.1.50";
    char board_ip[24] = "-";               // IP da propria placa (Wi-Fi ou AP), para mostrar na Config
    uint16_t switch_port = 6000;

    FireRedStats fr;
    UserMacroStore um;           // macros criados pelo usuario (6 slots)
    PokemonSlot party[6];
    ZeldaStats zelda;
    GenericStats generic;
    MapRadar map;
};

#endif // GAME_STATE_H