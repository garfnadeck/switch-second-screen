#ifndef UI_MANAGER_H
#define UI_MANAGER_H

#include <lvgl.h>
#include "../core/game_state.h"
#include "../core/asset_manager.h"
#include "../providers/provider_router.h"
#include "../games/pokemon_sv_module.h"
#include "../games/pokemon_legends_za_module.h"
#include "../games/pokemon_legends_arceus_module.h"
#include "../games/zelda_totk_module.h"
#include "../games/zelda_botw_module.h"
#include "../games/mariokart_8_module.h"
#include "../games/mariokart_world_module.h"
#include "../games/generic_module.h"
#include "../games/pokemon_firered_module.h"
#include <math.h>
#include <string.h>

class UIManager {
private:
    static constexpr uint8_t NUM_MODULES = static_cast<uint8_t>(GameProfile::COUNT);
    static constexpr uint8_t TRAIL_LEN   = 10;

    GameState &state;
    ProviderRouter &router;

    PokemonSVModule pokemon_sv_mod;
    PokemonLegendsZAModule pokemon_za_mod;
    PokemonLegendsArceusModule pokemon_arceus_mod;
    ZeldaTotkModule zelda_totk_mod;
    ZeldaBotwModule zelda_botw_mod;
    MarioKart8Module mk8_mod;
    MarioKartWorldModule mkw_mod;
    GenericModule generic_mod;
    PokemonFireRedModule firered_mod;

    IGameModule* modules[NUM_MODULES];

    // ---- Aba Mapa em Tempo Real ----
    static constexpr lv_coord_t MAP_VP_W = 248;
    static constexpr lv_coord_t MAP_VP_H = 212;
    lv_obj_t * map_viewport = nullptr;
    lv_obj_t * map_img = nullptr;
    lv_obj_t * player_ring = nullptr;
    lv_obj_t * player_dot = nullptr;
    lv_obj_t * trail_dots[TRAIL_LEN] = {};
    lv_obj_t * waypoint_dots[3] = {nullptr};
    lv_obj_t * waypoint_lbls[3] = {nullptr};
    lv_obj_t * lbl_map_zone = nullptr;
    lv_obj_t * coord_label = nullptr;
    lv_obj_t * lbl_map_meta = nullptr;
    lv_obj_t * lbl_btn_follow = nullptr;
    lv_obj_t * lbl_btn_layer = nullptr;
    lv_obj_t * lbl_btn_center = nullptr;

    CachedAsset map_zoom_asset;      // 768x768 RGB565 na PSRAM (Zoom 3x Rua)
    CachedAsset map_radar_asset;     // 480x480 RGB565 na PSRAM (Zoom 2x Radar)
    CachedAsset map_full_asset;      // 208x208 RGB565 na PSRAM (Zoom 1x Cidade Inteira / Submapas)
    uint8_t  map_zoom_level = 1;     // 0 = Cidade 208x208 (1x), 1 = Radar 480x480 (2x), 2 = Rua 768x768 (3x)
    uint8_t  map_layer = 0;          // 0 = Lumiose, 1 = Lysandre Lab, 2 = Esgoto Bleu, 3 = Esgoto Saison
    // Calibracao oficial do mapa_lumiose.jpg (medida ao vivo no Switch):
    // Campo 20 (Centro / Torre Prisma) = (-830.0, -500.0) | Raio da cidade = 490.0m
    // Campo 1 (Sul) = (-758..-677, -878..-941) -> cai exatamente sobre o escudo "1" no sul do mapa.
    static constexpr float WORLD_CENTER_X = -830.0f;
    static constexpr float WORLD_CENTER_Z = -500.0f;
    static constexpr float WORLD_RADIUS   =  490.0f;
    float    trail_x[TRAIL_LEN] = {};
    float    trail_y[TRAIL_LEN] = {};
    uint8_t  trail_count = 0;
    float    last_trail_x = -99999.0f, last_trail_y = -99999.0f;

    lv_obj_t * macro_generic = nullptr;   // pagina com os 6 botoes genericos
    lv_obj_t * macro_btn_lbls[6] = {nullptr};
    lv_obj_t * status_footer = nullptr;
    lv_obj_t * ip_textarea = nullptr;
    lv_obj_t * teclado = nullptr;         // teclado numerico para o IP do Switch
    lv_obj_t * lbl_diag = nullptr;        // Config: USB / macro ativa / execucoes
    lv_obj_t * lbl_board_ip = nullptr;    // Config: IP da placa
    lv_obj_t * lbl_scan = nullptr;        // Config: progresso/resultado da busca do Switch
    char scan_shown[48] = "";
    char sw_ip_shown[16] = "";
    char board_ip_shown[24] = "";
    int8_t   diag_conn = -1, diag_active = -2;
    uint32_t diag_exec = 0xFFFFFFFF;
    char     diag_link[40] = {};

    IGameModule* getActiveModule() {
        uint8_t idx = static_cast<uint8_t>(state.profile);
        return (idx < NUM_MODULES) ? modules[idx] : modules[0];
    }

    const char* getModeName() const {
        if (state.mode == SwitchMode::LOCKED_USB_MACRO) return "USB-HID";
        if (state.mode == SwitchMode::UNLOCKED_SYSBOTBASE) return "SYS-BOT";
        return "MOCK";
    }

    static const char* layerFile(uint8_t layer) {
        switch (layer) {
            case 1:  return "/assets/za/maps/lysandre_full.bin";
            case 2:  return "/assets/za/maps/sewers_bleu.bin";
            case 3:  return "/assets/za/maps/sewers_saison.bin";
            default: return "/assets/za/maps/lumiose_full.bin";
        }
    }

    static const char* layerLabel(uint8_t layer) {
        switch (layer) {
            case 1:  return "Mapa: Labs";
            case 2:  return "Mapa: Bleu";
            case 3:  return "Mapa: Saison";
            default: return "Mapa: Lumiose";
        }
    }

    // Converte coordenada do mundo (x, z_map) para [0.0 .. 1.0] no mapa_lumiose.jpg
    void worldToNorm(float wx, float wy, float &nx, float &ny) const {
        if (state.mode == SwitchMode::MOCK_SIMULATOR) {
            nx = wx / 260.0f;
            ny = wy / 230.0f;
        } else if (wx == 0.0f && wy == 0.0f) {
            nx = 0.50f;
            ny = 0.50f;
        } else {
            // +X = Leste (direita -> +nx), +Z = Norte (cima -> -ny), -Z = Sul (baixo -> +ny)
            nx = 0.50f + (wx - WORLD_CENTER_X) / (2.0f * WORLD_RADIUS);
            ny = 0.50f - (wy - WORLD_CENTER_Z) / (2.0f * WORLD_RADIUS);
        }
        if (nx < 0.03f) nx = 0.03f;
        if (nx > 0.97f) nx = 0.97f;
        if (ny < 0.03f) ny = 0.03f;
        if (ny > 0.97f) ny = 0.97f;
    }

    const char* zoomLabel() const {
        if (map_layer != 0 || map_zoom_level == 0) return "Cam: Cidade";
        if (map_zoom_level == 2 && map_zoom_asset.loaded) return "Cam: Rua 3x";
        return "Cam: Radar 2x";
    }

    void loadMapTextures() {
        AssetManager::preloadToPSRAM("/assets/za/maps/lumiose_zoom.bin",  map_zoom_asset,  768, 768, LV_IMG_CF_TRUE_COLOR);
        AssetManager::preloadToPSRAM("/assets/za/maps/lumiose_radar.bin", map_radar_asset, 480, 480, LV_IMG_CF_TRUE_COLOR);
        AssetManager::preloadToPSRAM(layerFile(map_layer),                map_full_asset,  208, 208, LV_IMG_CF_TRUE_COLOR);
        refreshMapSource();
    }

    void refreshMapSource() {
        if (!map_img) return;
        if (map_layer == 0 && map_zoom_level == 2 && map_zoom_asset.loaded) {
            lv_img_set_src(map_img, &map_zoom_asset.descriptor);
            lv_obj_clear_flag(map_img, LV_OBJ_FLAG_HIDDEN);
        } else if (map_layer == 0 && map_zoom_level >= 1 && map_radar_asset.loaded) {
            lv_img_set_src(map_img, &map_radar_asset.descriptor);
            lv_obj_clear_flag(map_img, LV_OBJ_FLAG_HIDDEN);
        } else if (map_full_asset.loaded) {
            lv_img_set_src(map_img, &map_full_asset.descriptor);
            lv_obj_set_pos(map_img, (MAP_VP_W - 208) / 2, (MAP_VP_H - 208) / 2);
            lv_obj_clear_flag(map_img, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(map_img, LV_OBJ_FLAG_HIDDEN);
        }
        if (lbl_btn_follow) {
            lv_label_set_text(lbl_btn_follow, zoomLabel());
        }
    }

    void updateMapRealtime() {
        if (!map_viewport || !player_dot) return;

        // Registra trilha se o jogador andou >= 6 unidades no mundo
        float dx_t = state.map.x - last_trail_x;
        float dy_t = state.map.y - last_trail_y;
        if (dx_t * dx_t + dy_t * dy_t >= 36.0f && (state.map.x != 0.0f || state.map.y != 0.0f)) {
            last_trail_x = state.map.x;
            last_trail_y = state.map.y;
            if (trail_count < TRAIL_LEN) {
                trail_x[trail_count] = state.map.x;
                trail_y[trail_count] = state.map.y;
                trail_count++;
            } else {
                for (uint8_t i = 0; i + 1 < TRAIL_LEN; i++) {
                    trail_x[i] = trail_x[i + 1];
                    trail_y[i] = trail_y[i + 1];
                }
                trail_x[TRAIL_LEN - 1] = state.map.x;
                trail_y[TRAIL_LEN - 1] = state.map.y;
            }
        }

        float nx = 0.5f, ny = 0.5f;
        worldToNorm(state.map.x, state.map.y, nx, ny);

        int16_t scroll_side = 0;
        if (map_layer == 0 && map_zoom_level == 2 && map_zoom_asset.loaded) {
            scroll_side = 768;
        } else if (map_layer == 0 && map_zoom_level >= 1 && map_radar_asset.loaded) {
            scroll_side = 480;
        }

        if (scroll_side > 0) {
            // Textura (480x480 ou 768x768) rola para manter (nx, ny) no centro do viewport (248x212)
            lv_coord_t px = (lv_coord_t)(nx * (float)scroll_side);
            lv_coord_t py = (lv_coord_t)(ny * (float)scroll_side);
            lv_coord_t img_x = (MAP_VP_W / 2) - px;
            lv_coord_t img_y = (MAP_VP_H / 2) - py;
            if (img_x > 0) img_x = 0;
            if (img_y > 0) img_y = 0;
            if (img_x < MAP_VP_W - scroll_side) img_x = MAP_VP_W - scroll_side;
            if (img_y < MAP_VP_H - scroll_side) img_y = MAP_VP_H - scroll_side;
            lv_obj_set_pos(map_img, img_x, img_y);

            lv_coord_t dot_x = img_x + px;
            lv_coord_t dot_y = img_y + py;
            lv_obj_set_pos(player_ring, dot_x - 13, dot_y - 13);
            lv_obj_set_pos(player_dot,  dot_x - 6,  dot_y - 6);

            for (uint8_t i = 0; i < TRAIL_LEN; i++) {
                if (i < trail_count) {
                    float tnx, tny;
                    worldToNorm(trail_x[i], trail_y[i], tnx, tny);
                    lv_coord_t tx = img_x + (lv_coord_t)(tnx * (float)scroll_side) - 3;
                    lv_coord_t ty = img_y + (lv_coord_t)(tny * (float)scroll_side) - 3;
                    lv_obj_set_pos(trail_dots[i], tx, ty);
                    lv_obj_clear_flag(trail_dots[i], LV_OBJ_FLAG_HIDDEN);
                } else {
                    lv_obj_add_flag(trail_dots[i], LV_OBJ_FLAG_HIDDEN);
                }
            }
        } else {
            // Mapa inteiro 208x208 centralizado no viewport (ox = 20, oy = 2)
            lv_coord_t ox = (MAP_VP_W - 208) / 2;
            lv_coord_t oy = (MAP_VP_H - 208) / 2;
            lv_coord_t dot_x = ox + (lv_coord_t)(nx * 208.0f);
            lv_coord_t dot_y = oy + (lv_coord_t)(ny * 208.0f);
            lv_obj_set_pos(player_ring, dot_x - 13, dot_y - 13);
            lv_obj_set_pos(player_dot,  dot_x - 6,  dot_y - 6);

            for (uint8_t i = 0; i < TRAIL_LEN; i++) {
                if (i < trail_count) {
                    float tnx, tny;
                    worldToNorm(trail_x[i], trail_y[i], tnx, tny);
                    lv_coord_t tx = ox + (lv_coord_t)(tnx * 208.0f) - 3;
                    lv_coord_t ty = oy + (lv_coord_t)(tny * 208.0f) - 3;
                    lv_obj_set_pos(trail_dots[i], tx, ty);
                    lv_obj_clear_flag(trail_dots[i], LV_OBJ_FLAG_HIDDEN);
                } else {
                    lv_obj_add_flag(trail_dots[i], LV_OBJ_FLAG_HIDDEN);
                }
            }
        }

        // Painel lateral de telemetria do mapa
        if (lbl_map_zone) {
            const char * zn = state.map.zone_name[0] ? state.map.zone_name : "Lumiose City";
            lv_label_set_text_fmt(lbl_map_zone, LV_SYMBOL_GPS " %s", zn);
        }
        if (coord_label) {
            char buf[96];
            snprintf(buf, sizeof(buf), "X: %7.1f\nZ: %7.1f\nAlt: %5.1f", state.map.x, state.map.y, state.map.z);
            lv_label_set_text(coord_label, buf);
        }
        if (lbl_map_meta) {
            float dx = state.map.x - WORLD_CENTER_X;
            float dz = state.map.y - WORLD_CENTER_Z;
            float dist = (state.map.x == 0.0f && state.map.y == 0.0f) ? 0.0f : sqrtf(dx * dx + dz * dz);
            const char * zstr = (map_zoom_level == 0) ? "1x" : (map_zoom_level == 1) ? "2x" : "3x";
            char mb[64];
            snprintf(mb, sizeof(mb), "Dist C20: %.0fm | Zoom: %s", dist, zstr);
            lv_label_set_text(lbl_map_meta, mb);
        }
    }

    // "a.b.c.d" com cada parte entre 0 e 255
    static bool ipValido(const char * t) {
        int partes = 0, valor = -1;
        for (const char * c = t; ; c++) {
            if (*c >= '0' && *c <= '9') {
                valor = (valor < 0 ? 0 : valor) * 10 + (*c - '0');
                if (valor > 255) return false;
            } else if (*c == '.' || *c == '\0') {
                if (valor < 0) return false;
                partes++; valor = -1;
                if (*c == '\0') break;
            } else return false;
        }
        return partes == 4;
    }

    static void macroThunk(void * ctx, uint8_t slot) {
        static_cast<UIManager*>(ctx)->onMacroClicked(slot);
    }

public:
    UIManager(GameState &s, ProviderRouter &r) : state(s), router(r) {
        modules[0] = &pokemon_sv_mod;
        modules[1] = &pokemon_za_mod;
        modules[2] = &pokemon_arceus_mod;
        modules[3] = &zelda_totk_mod;
        modules[4] = &zelda_botw_mod;
        modules[5] = &mk8_mod;
        modules[6] = &mkw_mod;
        modules[7] = &generic_mod;
        modules[8] = &firered_mod;
    }

    void reloadAssets() {
        loadMapTextures();
        pokemon_za_mod.reloadAssets();
    }

    void refreshActiveProfile() {
        uint8_t active_idx = static_cast<uint8_t>(state.profile);
        for (uint8_t i = 0; i < NUM_MODULES; i++) {
            modules[i]->setVisible(i == active_idx);
        }

        IGameModule * mod = getActiveModule();

        // Botoes genericos so aparecem se o jogo nao tem layout proprio de macros
        if (mod->hasCustomMacroTab()) lv_obj_add_flag(macro_generic, LV_OBJ_FLAG_HIDDEN);
        else                          lv_obj_clear_flag(macro_generic, LV_OBJ_FLAG_HIDDEN);

        for (int i = 0; i < 6; i++) {
            lv_label_set_text(macro_btn_lbls[i], mod->getMacroLabel(i));
        }

        for (int i = 0; i < 3; i++) {
            if (!waypoint_dots[i] || !waypoint_lbls[i]) continue;
            MapWaypoint &wp = state.map.waypoints[i];
            lv_obj_set_pos(waypoint_dots[i], (lv_coord_t)(wp.x * 0.9f), (lv_coord_t)(wp.y * 0.8f));
            lv_obj_set_style_bg_color(waypoint_dots[i], lv_color_hex(wp.color_hex), LV_PART_MAIN);
            lv_label_set_text(waypoint_lbls[i], wp.label);
            lv_obj_set_pos(waypoint_lbls[i], (lv_coord_t)(wp.x * 0.9f) + 12, (lv_coord_t)(wp.y * 0.8f) - 2);
        }
    }

    void tickUpdate() {
        router.update(state);
        IGameModule * mod = getActiveModule();
        mod->updateStatusTab(state);
        updateMapRealtime();

        // Config > IP do Switch: segue o estado (pode mudar pela pagina web ou pela busca)
        if (ip_textarea && strncmp(sw_ip_shown, state.switch_ip, sizeof(sw_ip_shown)) != 0) {
            snprintf(sw_ip_shown, sizeof(sw_ip_shown), "%s", state.switch_ip);
            if (lv_obj_get_state(ip_textarea) & LV_STATE_FOCUSED) { /* nao sobrescreve enquanto digita */ }
            else lv_textarea_set_text(ip_textarea, state.switch_ip);
        }
        // Config > busca do Switch (progresso e resultado)
        if (lbl_scan) {
            char txt[64];
            uint8_t st = router.scanState();
            if (st == 1)      snprintf(txt, sizeof(txt), "Procurando o Switch (%d%%)...", router.scanProgress());
            else if (st == 2) snprintf(txt, sizeof(txt), "Switch encontrado: %s", state.switch_ip);
            else if (st == 3) snprintf(txt, sizeof(txt), "Switch nao encontrado na rede.");
            else              snprintf(txt, sizeof(txt), "Detectar: procura o Switch na rede da placa");
            if (strncmp(scan_shown, txt, sizeof(scan_shown)) != 0) {
                snprintf(scan_shown, sizeof(scan_shown), "%s", txt);
                lv_label_set_text(lbl_scan, txt);
            }
        }

        // Config > IP da placa (so redesenha quando muda)
        if (lbl_board_ip && strncmp(board_ip_shown, state.board_ip, sizeof(board_ip_shown)) != 0) {
            snprintf(board_ip_shown, sizeof(board_ip_shown), "%s", state.board_ip);
            lv_label_set_text_fmt(lbl_board_ip, "IP da placa:\n%s", state.board_ip);
        }

        // Config > diagnostico (so redesenha quando muda)
        int8_t conn = state.usb_online ? 1 : 0;
        if (lbl_diag && (conn != diag_conn || state.active_macro_slot != diag_active ||
                         state.generic.macro_executions != diag_exec ||
                         strncmp(state.link_status, diag_link, sizeof(diag_link)) != 0)) {
            snprintf(diag_link, sizeof(diag_link), "%s", state.link_status);
            diag_conn = conn; diag_active = state.active_macro_slot; diag_exec = state.generic.macro_executions;
            const char * act = (diag_active >= 0) ? mod->getMacroLabel((uint8_t)diag_active) : "nenhuma";
            lv_label_set_text_fmt(lbl_diag, "USB: %s\nMacro ativa: %s\nExecucoes: %lu\nLeitura RAM: %s",
                                  conn ? "#00E676 ON#" : "#FF1744 OFF#", act, (unsigned long)diag_exec, state.link_status);
        }
    }

    void onProfileSelected(uint16_t sel) {
        router.setProfile(state, static_cast<GameProfile>(sel));
        refreshActiveProfile();
        lv_label_set_text_fmt(status_footer, "[%s] Jogo: %s",
                              getModeName(), getActiveModule()->getGameName());
    }

    void onModeSelected(uint16_t sel) {
        if (ip_textarea) {
            snprintf(state.switch_ip, sizeof(state.switch_ip), "%s", lv_textarea_get_text(ip_textarea));
        }
        router.switchMode(state, static_cast<SwitchMode>(sel));
        lv_label_set_text_fmt(status_footer, "Modo: %s (%s)", getModeName(), state.switch_ip);
    }

    // Botoes do painel FireRed (contador e insignias)
    void frAction(uint8_t code) {
        FireRedStats & f = state.fr;
        if (code >= FR_CODE_BADGE0 && code < FR_CODE_BADGE0 + 8) f.badges ^= (1u << (code - FR_CODE_BADGE0));
        else if (code == FR_CODE_ENC_INC) f.encounters++;
        else if (code == FR_CODE_ENC_DEC) { if (f.encounters) f.encounters--; }
        else if (code == FR_CODE_ENC_RST) f.encounters = 0;
        f.dirty = true; f.urgent = true;
        f.evaluate();
    }

    void onMacroClicked(uint8_t slot) {
        if (slot == MACRO_STOP) {
            router.stopAll(state);
            lv_label_set_text_fmt(status_footer, "[%s] PARAR TUDO", getModeName());
            return;
        }
        if (slot >= FR_CODE_BADGE0 && slot <= FR_CODE_ENC_INC) { frAction(slot); return; }
        router.triggerAction(static_cast<ActionMacro>(slot), state);
        lv_label_set_text_fmt(status_footer, "[%s] Macro: %s",
                              getModeName(),
                              getActiveModule()->getMacroLabel(slot));
    }

    void buildUI() {
        router.init(state);   // idempotente: nao reinicializa se o main ja chamou

        lv_obj_t * scr = lv_scr_act();
        lv_obj_set_style_bg_color(scr, lv_color_hex(0x12121F), LV_PART_MAIN);

        // Barra Superior (480x38)
        lv_obj_t * top_bar = lv_obj_create(scr);
        lv_obj_set_size(top_bar, 480, 38);
        lv_obj_align(top_bar, LV_ALIGN_TOP_MID, 0, 0);
        lv_obj_set_style_bg_color(top_bar, lv_color_hex(0x1F1F38), LV_PART_MAIN);
        lv_obj_set_style_border_width(top_bar, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(top_bar, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(top_bar, 4, LV_PART_MAIN);
        lv_obj_clear_flag(top_bar, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t * title = lv_label_create(top_bar);
        lv_label_set_text(title, "SWITCH HUD");
        lv_obj_set_style_text_color(title, lv_color_hex(0x00F5D4), LV_PART_MAIN);
        lv_obj_align(title, LV_ALIGN_LEFT_MID, 8, 0);

        lv_obj_t * dd = lv_dropdown_create(top_bar);
        lv_dropdown_set_options(dd,
            "Pokemon SV\n"
            "Pokemon Legends ZA\n"
            "Pokemon Arceus\n"
            "Zelda TOTK\n"
            "Zelda BOTW\n"
            "Mario Kart 8D\n"
            "Mario Kart World\n"
            "Generic Hub\n"
            "Pokemon FireRed"
        );
        // O dropdown precisa comecar no jogo que o provider realmente ativou
        if (static_cast<uint8_t>(state.profile) < NUM_MODULES) {
            lv_dropdown_set_selected(dd, static_cast<uint16_t>(state.profile));
        }
        lv_obj_set_size(dd, 185, 30);
        lv_obj_align(dd, LV_ALIGN_RIGHT_MID, -4, 0);
        lv_obj_set_style_bg_color(dd, lv_color_hex(0x0F3460), LV_PART_MAIN);
        lv_obj_set_style_text_color(dd, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        lv_obj_set_style_text_font(dd, &lv_font_montserrat_12, LV_PART_MAIN);
        lv_obj_add_event_cb(dd, [](lv_event_t * e) {
            auto * mgr = static_cast<UIManager*>(lv_event_get_user_data(e));
            mgr->onProfileSelected(lv_dropdown_get_selected(lv_event_get_target(e)));
        }, LV_EVENT_VALUE_CHANGED, this);

        // Tabview Principal (480x256)
        lv_obj_t * tabview = lv_tabview_create(scr, LV_DIR_TOP, 32);
        lv_obj_set_size(tabview, 480, 256);
        lv_obj_align(tabview, LV_ALIGN_TOP_MID, 0, 38);
        lv_obj_set_style_bg_color(tabview, lv_color_hex(0x12121F), LV_PART_MAIN);

        lv_obj_t * tab_btns = lv_tabview_get_tab_btns(tabview);
        lv_obj_set_style_bg_color(tab_btns, lv_color_hex(0x18182C), LV_PART_MAIN);
        lv_obj_set_style_text_color(tab_btns, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        lv_obj_set_style_text_font(tab_btns, &lv_font_montserrat_12, LV_PART_MAIN);

        lv_obj_t * tab_status = lv_tabview_add_tab(tabview, "Status");
        lv_obj_t * tab_map    = lv_tabview_add_tab(tabview, "Mapa");
        lv_obj_t * tab_macros = lv_tabview_add_tab(tabview, "Macros");
        lv_obj_t * tab_config = lv_tabview_add_tab(tabview, "Config");

        // Aba 5: Botoes — toque envia o bit ao Switch pelo USB HID (mapeamento medido no NS2)
        lv_obj_t * tab_botoes = lv_tabview_add_tab(tabview, "Botoes");
        lv_obj_set_style_pad_all(tab_botoes, 4, LV_PART_MAIN);
        static const char * nomesBotoes[16] = {
            "Y", "B", "A", "X", "L", "R", "ZL", "ZR",
            "-", "+", "L3", "R3", "Home", "Captura", "-", "-"
        };
        for (int i = 0; i < 16; i++) {
            lv_obj_t * btn = lv_btn_create(tab_botoes);
            lv_obj_set_size(btn, 110, 48);
            lv_obj_set_pos(btn, (i % 4) * 116, (i / 4) * 52);
            lv_obj_set_style_bg_color(btn, lv_color_hex(0x2A2A44), LV_PART_MAIN);
            lv_obj_set_user_data(btn, (void *)(intptr_t)i);
            lv_obj_add_event_cb(btn, [](lv_event_t * e) {
                auto * mgr = static_cast<UIManager*>(lv_event_get_user_data(e));
                int bit = (int)(intptr_t)lv_obj_get_user_data(lv_event_get_target(e));
                mgr->router.usb().livePad((uint16_t)(1u << bit), 0x08, mgr->state);
            }, LV_EVENT_PRESSED, this);
            lv_obj_add_event_cb(btn, [](lv_event_t * e) {
                auto * mgr = static_cast<UIManager*>(lv_event_get_user_data(e));
                mgr->router.usb().livePad(0, 0x08, mgr->state);
            }, LV_EVENT_RELEASED, this);
            lv_obj_t * txt = lv_label_create(btn);
            lv_label_set_text_fmt(txt, "%s (%d)", nomesBotoes[i], i);
            lv_obj_set_style_text_color(txt, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
            lv_obj_center(txt);
        }
        lv_obj_set_style_pad_all(tab_status, 5, LV_PART_MAIN);
        lv_obj_set_style_pad_all(tab_map, 4, LV_PART_MAIN);
        lv_obj_set_style_pad_all(tab_macros, 0, LV_PART_MAIN);   // grade 4x3 + pager usam a aba inteira (480x224)

        // Aba 1: Status
        for (uint8_t i = 0; i < NUM_MODULES; i++) {
            modules[i]->buildStatusTab(tab_status);
        }

        // Aba 2: Mapa em Tempo Real (Viewport 248x212 a esquerda + Painel Tatico 218x212 a direita)
        map_viewport = lv_obj_create(tab_map);
        lv_obj_set_size(map_viewport, MAP_VP_W, MAP_VP_H);
        lv_obj_set_pos(map_viewport, 0, 0);
        lv_obj_set_style_bg_color(map_viewport, lv_color_hex(0x0B192C), LV_PART_MAIN);
        lv_obj_set_style_border_color(map_viewport, lv_color_hex(0x00F5D4), LV_PART_MAIN);
        lv_obj_set_style_border_width(map_viewport, 1, LV_PART_MAIN);
        lv_obj_set_style_radius(map_viewport, 8, LV_PART_MAIN);
        lv_obj_set_style_pad_all(map_viewport, 0, LV_PART_MAIN);
        lv_obj_clear_flag(map_viewport, LV_OBJ_FLAG_SCROLLABLE);

        // Grade tatica de fundo (visivel nas bordas ou se o SD ainda nao tiver o mapa)
        for (int g = 1; g <= 3; g++) {
            lv_obj_t * hline = lv_obj_create(map_viewport);
            lv_obj_set_size(hline, MAP_VP_W - 4, 1);
            lv_obj_set_pos(hline, 2, g * 52);
            lv_obj_set_style_bg_color(hline, lv_color_hex(0x1E3E62), LV_PART_MAIN);
            lv_obj_set_style_border_width(hline, 0, LV_PART_MAIN);
        }
        for (int g = 1; g <= 3; g++) {
            lv_obj_t * vline = lv_obj_create(map_viewport);
            lv_obj_set_size(vline, 1, MAP_VP_H - 4);
            lv_obj_set_pos(vline, g * 62, 2);
            lv_obj_set_style_bg_color(vline, lv_color_hex(0x1E3E62), LV_PART_MAIN);
            lv_obj_set_style_border_width(vline, 0, LV_PART_MAIN);
        }

        // Imagem do mapa carregada do MicroSD para a PSRAM
        map_img = lv_img_create(map_viewport);
        lv_obj_set_pos(map_img, 0, 0);

        // Trilha dos ultimos passos do jogador
        for (uint8_t i = 0; i < TRAIL_LEN; i++) {
            trail_dots[i] = lv_obj_create(map_viewport);
            lv_obj_set_size(trail_dots[i], 6, 6);
            lv_obj_set_style_radius(trail_dots[i], LV_RADIUS_CIRCLE, LV_PART_MAIN);
            lv_obj_set_style_bg_color(trail_dots[i], lv_color_hex(0x00F5D4), LV_PART_MAIN);
            lv_obj_set_style_bg_opa(trail_dots[i], (lv_opa_t)(80 + i * 15), LV_PART_MAIN);
            lv_obj_set_style_border_width(trail_dots[i], 0, LV_PART_MAIN);
            lv_obj_add_flag(trail_dots[i], LV_OBJ_FLAG_HIDDEN);
        }

        // Anel de radar ao redor do jogador
        player_ring = lv_obj_create(map_viewport);
        lv_obj_set_size(player_ring, 26, 26);
        lv_obj_set_style_radius(player_ring, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(player_ring, LV_OPA_20, LV_PART_MAIN);
        lv_obj_set_style_bg_color(player_ring, lv_color_hex(0x00F5D4), LV_PART_MAIN);
        lv_obj_set_style_border_color(player_ring, lv_color_hex(0x00F5D4), LV_PART_MAIN);
        lv_obj_set_style_border_width(player_ring, 1, LV_PART_MAIN);

        // Pino principal do jogador
        player_dot = lv_obj_create(map_viewport);
        lv_obj_set_size(player_dot, 12, 12);
        lv_obj_set_style_radius(player_dot, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        lv_obj_set_style_bg_color(player_dot, lv_color_hex(0xFF1744), LV_PART_MAIN);
        lv_obj_set_style_border_color(player_dot, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        lv_obj_set_style_border_width(player_dot, 2, LV_PART_MAIN);

        // Painel lateral direito da aba Mapa (218x212)
        lv_obj_t * map_panel = lv_obj_create(tab_map);
        lv_obj_set_size(map_panel, 218, MAP_VP_H);
        lv_obj_set_pos(map_panel, 254, 0);
        lv_obj_set_style_bg_color(map_panel, lv_color_hex(0x181C32), LV_PART_MAIN);
        lv_obj_set_style_border_color(map_panel, lv_color_hex(0x2A3558), LV_PART_MAIN);
        lv_obj_set_style_border_width(map_panel, 1, LV_PART_MAIN);
        lv_obj_set_style_radius(map_panel, 8, LV_PART_MAIN);
        lv_obj_set_style_pad_all(map_panel, 6, LV_PART_MAIN);
        lv_obj_clear_flag(map_panel, LV_OBJ_FLAG_SCROLLABLE);

        lbl_map_zone = lv_label_create(map_panel);
        lv_obj_set_style_text_color(lbl_map_zone, lv_color_hex(0x00F5D4), LV_PART_MAIN);
        lv_obj_set_style_text_font(lbl_map_zone, &lv_font_montserrat_12, LV_PART_MAIN);
        lv_obj_set_width(lbl_map_zone, 204);
        lv_label_set_long_mode(lbl_map_zone, LV_LABEL_LONG_CLIP);
        lv_obj_set_pos(lbl_map_zone, 0, 0);
        lv_label_set_text(lbl_map_zone, LV_SYMBOL_GPS " Lumiose City");

        lv_obj_t * coord_box = lv_obj_create(map_panel);
        lv_obj_set_size(coord_box, 204, 62);
        lv_obj_set_pos(coord_box, 0, 20);
        lv_obj_set_style_bg_color(coord_box, lv_color_hex(0x101324), LV_PART_MAIN);
        lv_obj_set_style_border_color(coord_box, lv_color_hex(0x262E4E), LV_PART_MAIN);
        lv_obj_set_style_border_width(coord_box, 1, LV_PART_MAIN);
        lv_obj_set_style_radius(coord_box, 6, LV_PART_MAIN);
        lv_obj_set_style_pad_all(coord_box, 5, LV_PART_MAIN);
        lv_obj_clear_flag(coord_box, LV_OBJ_FLAG_SCROLLABLE);

        coord_label = lv_label_create(coord_box);
        lv_obj_set_style_text_color(coord_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        lv_obj_set_style_text_font(coord_label, &lv_font_montserrat_12, LV_PART_MAIN);
        lv_obj_set_style_text_line_space(coord_label, 2, LV_PART_MAIN);
        lv_obj_set_pos(coord_label, 2, 0);
        lv_label_set_text(coord_label, "X:     0.0\nZ:     0.0\nAlt:   0.0");

        lbl_map_meta = lv_label_create(map_panel);
        lv_obj_set_style_text_color(lbl_map_meta, lv_color_hex(0x99A2C8), LV_PART_MAIN);
        lv_obj_set_style_text_font(lbl_map_meta, &lv_font_montserrat_12, LV_PART_MAIN);
        lv_obj_set_pos(lbl_map_meta, 0, 86);
        lv_label_set_text(lbl_map_meta, "Dist C20: 0m | Zoom: 2x");

        // Linha 1 de botoes: [Cam: Radar 2x / Rua 3x / Cidade]  [Mapa: Lumiose/Labs/Esgoto]
        lv_obj_t * btn_follow = lv_btn_create(map_panel);
        lv_obj_set_size(btn_follow, 99, 42);
        lv_obj_set_pos(btn_follow, 0, 108);
        lv_obj_set_style_bg_color(btn_follow, lv_color_hex(0x0F3460), LV_PART_MAIN);
        lv_obj_set_style_radius(btn_follow, 6, LV_PART_MAIN);
        lv_obj_add_event_cb(btn_follow, [](lv_event_t * e) {
            auto * mgr = static_cast<UIManager*>(lv_event_get_user_data(e));
            mgr->map_zoom_level = (mgr->map_zoom_level == 0) ? 1 : 0;
            mgr->refreshMapSource();
            mgr->updateMapRealtime();
        }, LV_EVENT_CLICKED, this);
        lbl_btn_follow = lv_label_create(btn_follow);
        lv_obj_set_style_text_font(lbl_btn_follow, &lv_font_montserrat_12, LV_PART_MAIN);
        lv_label_set_text(lbl_btn_follow, "Cam: Radar 2x");
        lv_obj_center(lbl_btn_follow);

        lv_obj_t * btn_layer = lv_btn_create(map_panel);
        lv_obj_set_size(btn_layer, 99, 42);
        lv_obj_set_pos(btn_layer, 105, 108);
        lv_obj_set_style_bg_color(btn_layer, lv_color_hex(0x23294E), LV_PART_MAIN);
        lv_obj_set_style_radius(btn_layer, 6, LV_PART_MAIN);
        lv_obj_add_event_cb(btn_layer, [](lv_event_t * e) {
            auto * mgr = static_cast<UIManager*>(lv_event_get_user_data(e));
            mgr->map_layer = (uint8_t)((mgr->map_layer + 1) % 4);
            AssetManager::preloadToPSRAM(layerFile(mgr->map_layer), mgr->map_full_asset, 208, 208, LV_IMG_CF_TRUE_COLOR);
            if (mgr->lbl_btn_layer) {
                lv_label_set_text(mgr->lbl_btn_layer, layerLabel(mgr->map_layer));
            }
            mgr->refreshMapSource();
            mgr->updateMapRealtime();
        }, LV_EVENT_CLICKED, this);
        lbl_btn_layer = lv_label_create(btn_layer);
        lv_obj_set_style_text_font(lbl_btn_layer, &lv_font_montserrat_12, LV_PART_MAIN);
        lv_label_set_text(lbl_btn_layer, "Mapa: Lumiose");
        lv_obj_center(lbl_btn_layer);

        // Linha 2 de botoes: [Zoom +] [Zoom -] [Limpar]
        lv_obj_t * btn_zin = lv_btn_create(map_panel);
        lv_obj_set_size(btn_zin, 58, 42);
        lv_obj_set_pos(btn_zin, 0, 156);
        lv_obj_set_style_bg_color(btn_zin, lv_color_hex(0x23294E), LV_PART_MAIN);
        lv_obj_set_style_radius(btn_zin, 6, LV_PART_MAIN);
        lv_obj_add_event_cb(btn_zin, [](lv_event_t * e) {
            auto * mgr = static_cast<UIManager*>(lv_event_get_user_data(e));
            if (mgr->map_zoom_level < 2) mgr->map_zoom_level++;
            mgr->refreshMapSource();
            mgr->updateMapRealtime();
        }, LV_EVENT_CLICKED, this);
        lv_obj_t * lbl_zin = lv_label_create(btn_zin);
        lv_obj_set_style_text_font(lbl_zin, &lv_font_montserrat_12, LV_PART_MAIN);
        lv_label_set_text(lbl_zin, "Zoom +");
        lv_obj_center(lbl_zin);

        lv_obj_t * btn_zout = lv_btn_create(map_panel);
        lv_obj_set_size(btn_zout, 58, 42);
        lv_obj_set_pos(btn_zout, 63, 156);
        lv_obj_set_style_bg_color(btn_zout, lv_color_hex(0x23294E), LV_PART_MAIN);
        lv_obj_set_style_radius(btn_zout, 6, LV_PART_MAIN);
        lv_obj_add_event_cb(btn_zout, [](lv_event_t * e) {
            auto * mgr = static_cast<UIManager*>(lv_event_get_user_data(e));
            if (mgr->map_zoom_level > 0) mgr->map_zoom_level--;
            mgr->refreshMapSource();
            mgr->updateMapRealtime();
        }, LV_EVENT_CLICKED, this);
        lv_obj_t * lbl_zout = lv_label_create(btn_zout);
        lv_obj_set_style_text_font(lbl_zout, &lv_font_montserrat_12, LV_PART_MAIN);
        lv_label_set_text(lbl_zout, "Zoom -");
        lv_obj_center(lbl_zout);

        lv_obj_t * btn_center = lv_btn_create(map_panel);
        lv_obj_set_size(btn_center, 78, 42);
        lv_obj_set_pos(btn_center, 126, 156);
        lv_obj_set_style_bg_color(btn_center, lv_color_hex(0x1E3A5F), LV_PART_MAIN);
        lv_obj_set_style_radius(btn_center, 6, LV_PART_MAIN);
        lv_obj_add_event_cb(btn_center, [](lv_event_t * e) {
            auto * mgr = static_cast<UIManager*>(lv_event_get_user_data(e));
            mgr->trail_count = 0;
            mgr->last_trail_x = -99999.0f;
            mgr->last_trail_y = -99999.0f;
            if (mgr->map_zoom_level == 0) mgr->map_zoom_level = 1;
            mgr->refreshMapSource();
            mgr->updateMapRealtime();
        }, LV_EVENT_CLICKED, this);
        lbl_btn_center = lv_label_create(btn_center);
        lv_obj_set_style_text_font(lbl_btn_center, &lv_font_montserrat_12, LV_PART_MAIN);
        lv_label_set_text(lbl_btn_center, "Focar");
        lv_obj_center(lbl_btn_center);

        loadMapTextures();

        // Aba 3: Macros — botoes genericos (jogos sem layout proprio) ...
        macro_generic = lv_obj_create(tab_macros);
        lv_obj_set_size(macro_generic, LV_PCT(100), LV_PCT(100));
        lv_obj_set_pos(macro_generic, 0, 0);
        lv_obj_set_style_pad_all(macro_generic, 0, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(macro_generic, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_border_width(macro_generic, 0, LV_PART_MAIN);
        lv_obj_clear_flag(macro_generic, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_flex_flow(macro_generic, LV_FLEX_FLOW_ROW_WRAP);
        lv_obj_set_flex_align(macro_generic, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        lv_obj_set_style_pad_column(macro_generic, 6, LV_PART_MAIN);
        lv_obj_set_style_pad_row(macro_generic, 8, LV_PART_MAIN);
        lv_obj_set_style_pad_left(macro_generic, 3, LV_PART_MAIN);
        lv_obj_set_style_pad_top(macro_generic, 4, LV_PART_MAIN);

        for (int i = 0; i < 6; i++) {
            lv_obj_t * btn = lv_btn_create(macro_generic);
            lv_obj_set_size(btn, LV_PCT(32), 90);
            lv_obj_set_style_bg_color(btn, lv_color_hex(0x0F3460), LV_PART_MAIN);
            lv_obj_set_user_data(btn, (void*)(intptr_t)i);
            lv_obj_add_event_cb(btn, [](lv_event_t * e) {
                auto * mgr = static_cast<UIManager*>(lv_event_get_user_data(e));
                uint8_t slot = (uint8_t)(intptr_t)lv_obj_get_user_data(lv_event_get_target(e));
                mgr->onMacroClicked(slot);
            }, LV_EVENT_CLICKED, this);

            macro_btn_lbls[i] = lv_label_create(btn);
            lv_obj_center(macro_btn_lbls[i]);
        }

        // ... e paginas proprias dos modulos que tem layout dedicado (ex.: Z-A)
        for (uint8_t i = 0; i < NUM_MODULES; i++) {
            if (modules[i]->hasCustomMacroTab()) {
                modules[i]->buildMacroTab(tab_macros, &UIManager::macroThunk, this);
            }
        }

        // Aba 4: Config
        lv_obj_t * lbl_mode = lv_label_create(tab_config);
        lv_label_set_text(lbl_mode, "Modo:");
        lv_obj_set_style_text_color(lbl_mode, lv_color_hex(0x00F5D4), LV_PART_MAIN);
        lv_obj_align(lbl_mode, LV_ALIGN_TOP_LEFT, 5, 5);

        lv_obj_t * dd_mode = lv_dropdown_create(tab_config);
        lv_dropdown_set_options(dd_mode, "Simulador (Mock)\nUSB HID\nSys-Botbase");
        lv_dropdown_set_selected(dd_mode, static_cast<uint16_t>(state.mode));
        lv_obj_set_size(dd_mode, 210, 32);
        lv_obj_align(dd_mode, LV_ALIGN_TOP_LEFT, 5, 24);
        lv_obj_set_style_bg_color(dd_mode, lv_color_hex(0x1E1E36), LV_PART_MAIN);
        lv_obj_set_style_text_color(dd_mode, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        lv_obj_add_event_cb(dd_mode, [](lv_event_t * e) {
            auto * mgr = static_cast<UIManager*>(lv_event_get_user_data(e));
            mgr->onModeSelected(lv_dropdown_get_selected(lv_event_get_target(e)));
        }, LV_EVENT_VALUE_CHANGED, this);

        lv_obj_t * lbl_ip = lv_label_create(tab_config);
        lv_label_set_text(lbl_ip, "Switch IP:");
        lv_obj_set_style_text_color(lbl_ip, lv_color_hex(0x00F5D4), LV_PART_MAIN);
        lv_obj_align(lbl_ip, LV_ALIGN_TOP_LEFT, 235, 5);

        ip_textarea = lv_textarea_create(tab_config);
        lv_textarea_set_one_line(ip_textarea, true);
        lv_textarea_set_max_length(ip_textarea, 15);
        lv_textarea_set_text(ip_textarea, state.switch_ip);
        lv_obj_set_size(ip_textarea, 230, 32);
        lv_obj_align(ip_textarea, LV_ALIGN_TOP_LEFT, 235, 24);

        // Teclado numerico na tela: aparece ao tocar no campo; OK aplica e grava, o X cancela
        teclado = lv_keyboard_create(lv_layer_top());
        lv_keyboard_set_mode(teclado, LV_KEYBOARD_MODE_NUMBER);
        lv_obj_set_size(teclado, 480, 170);
        lv_obj_align(teclado, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_add_flag(teclado, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_event_cb(ip_textarea, [](lv_event_t * e) {
            auto * mgr = static_cast<UIManager*>(lv_event_get_user_data(e));
            lv_keyboard_set_textarea(mgr->teclado, mgr->ip_textarea);
            lv_obj_clear_flag(mgr->teclado, LV_OBJ_FLAG_HIDDEN);
        }, LV_EVENT_CLICKED, this);
        lv_obj_add_event_cb(teclado, [](lv_event_t * e) {
            auto * mgr = static_cast<UIManager*>(lv_event_get_user_data(e));
            lv_event_code_t code = lv_event_get_code(e);
            if (code != LV_EVENT_READY && code != LV_EVENT_CANCEL) return;
            const char * txt = lv_textarea_get_text(mgr->ip_textarea);
            if (code == LV_EVENT_READY && ipValido(txt)) {
                mgr->router.setSwitchTarget(mgr->state, txt, mgr->state.switch_port);   // reconecta na hora
                mgr->state.switch_ip_changed = true;                                    // o main grava na flash
            } else {
                lv_textarea_set_text(mgr->ip_textarea, mgr->state.switch_ip);           // cancelado ou invalido
            }
            lv_obj_add_flag(mgr->teclado, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_state(mgr->ip_textarea, LV_STATE_FOCUSED);
        }, LV_EVENT_ALL, this);
        lv_obj_set_style_bg_color(ip_textarea, lv_color_hex(0x1E1E36), LV_PART_MAIN);
        lv_obj_set_style_text_color(ip_textarea, lv_color_hex(0xFFFFFF), LV_PART_MAIN);

        lbl_diag = lv_label_create(tab_config);
        lv_label_set_recolor(lbl_diag, true);
        lv_obj_set_style_text_color(lbl_diag, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        lv_obj_set_style_text_font(lbl_diag, &lv_font_montserrat_12, LV_PART_MAIN);
        lv_obj_set_style_text_line_space(lbl_diag, 4, LV_PART_MAIN);
        lv_obj_align(lbl_diag, LV_ALIGN_TOP_LEFT, 5, 70);
        lv_label_set_text(lbl_diag, "USB: -");

        lbl_board_ip = lv_label_create(tab_config);
        lv_obj_set_style_text_color(lbl_board_ip, lv_color_hex(0x00F5D4), LV_PART_MAIN);
        lv_obj_set_style_text_font(lbl_board_ip, &lv_font_montserrat_12, LV_PART_MAIN);
        lv_obj_align(lbl_board_ip, LV_ALIGN_TOP_LEFT, 235, 62);
        lv_label_set_text(lbl_board_ip, "IP da placa: -");

        lv_obj_t * btn_detectar = lv_btn_create(tab_config);
        lv_obj_set_size(btn_detectar, 110, 30);
        lv_obj_align(btn_detectar, LV_ALIGN_TOP_LEFT, 235, 100);
        lv_obj_set_style_bg_color(btn_detectar, lv_color_hex(0x2A2A44), LV_PART_MAIN);
        lv_obj_add_event_cb(btn_detectar, [](lv_event_t * e) {
            auto * mgr = static_cast<UIManager*>(lv_event_get_user_data(e));
            const char * ip = mgr->state.board_ip;
            if (ip[0] < '0' || ip[0] > '9') return;          // placa sem IP ainda
            char prefixo[20];
            size_t n = 0; int pontos = 0;
            for (; ip[n] && pontos < 3 && n < sizeof(prefixo) - 1; n++) {
                prefixo[n] = ip[n];
                if (ip[n] == '.') pontos++;
            }
            prefixo[n] = '\0';
            mgr->router.scanForSwitch(mgr->state, prefixo);
        }, LV_EVENT_CLICKED, this);
        lv_obj_t * lbl_btn = lv_label_create(btn_detectar);
        lv_label_set_text(lbl_btn, "Detectar");
        lv_obj_center(lbl_btn);

        lbl_scan = lv_label_create(tab_config);
        lv_obj_set_width(lbl_scan, 240);
        lv_label_set_long_mode(lbl_scan, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_color(lbl_scan, lv_color_hex(0xAAAACC), LV_PART_MAIN);
        lv_obj_set_style_text_font(lbl_scan, &lv_font_montserrat_12, LV_PART_MAIN);
        lv_obj_align(lbl_scan, LV_ALIGN_TOP_LEFT, 235, 136);
        lv_label_set_text(lbl_scan, "Detectar: procura o Switch na rede da placa");

        // Rodapé
        status_footer = lv_label_create(scr);
        lv_label_set_text_fmt(status_footer, "[%s] IP: %s", getModeName(), state.switch_ip);
        lv_obj_set_style_text_color(status_footer, lv_color_hex(0x8888AA), LV_PART_MAIN);
        lv_obj_set_style_text_font(status_footer, &lv_font_montserrat_12, LV_PART_MAIN);
        lv_obj_align(status_footer, LV_ALIGN_BOTTOM_LEFT, 8, -4);

        refreshActiveProfile();

        lv_timer_create([](lv_timer_t * t) {
            auto * mgr = static_cast<UIManager*>(t->user_data);
            mgr->tickUpdate();
        }, 100, this);
    }
};

#endif // UI_MANAGER_H
