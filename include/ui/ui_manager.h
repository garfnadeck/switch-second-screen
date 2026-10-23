#ifndef UI_MANAGER_H
#define UI_MANAGER_H

#include <lvgl.h>
#include "../core/game_state.h"
#include "../providers/provider_router.h"
#include "../games/pokemon_sv_module.h"
#include "../games/pokemon_legends_za_module.h"
#include "../games/pokemon_legends_arceus_module.h"
#include "../games/zelda_totk_module.h"
#include "../games/zelda_botw_module.h"
#include "../games/mariokart_8_module.h"
#include "../games/mariokart_world_module.h"
#include "../games/generic_module.h"
#include <string.h>

class UIManager {
private:
    static constexpr uint8_t NUM_MODULES = static_cast<uint8_t>(GameProfile::COUNT);

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

    IGameModule* modules[NUM_MODULES];

    lv_obj_t * player_dot = nullptr;
    lv_obj_t * waypoint_dots[3] = {nullptr};
    lv_obj_t * waypoint_lbls[3] = {nullptr};
    lv_obj_t * coord_label = nullptr;
    lv_obj_t * macro_generic = nullptr;   // pagina com os 6 botoes genericos
    lv_obj_t * macro_btn_lbls[6] = {nullptr};
    lv_obj_t * status_footer = nullptr;
    lv_obj_t * ip_textarea = nullptr;
    lv_obj_t * lbl_diag = nullptr;        // Config: USB / macro ativa / execucoes
    int8_t   diag_conn = -1, diag_active = -2;
    uint32_t diag_exec = 0xFFFFFFFF;
    char     diag_link[24] = {};

    IGameModule* getActiveModule() {
        uint8_t idx = static_cast<uint8_t>(state.profile);
        return (idx < NUM_MODULES) ? modules[idx] : modules[0];
    }

    const char* getModeName() const {
        if (state.mode == SwitchMode::LOCKED_USB_MACRO) return "USB-HID";
        if (state.mode == SwitchMode::UNLOCKED_SYSBOTBASE) return "SYS-BOT";
        return "MOCK";
    }

    // Botoes desenhados pelos modulos chamam isto
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
            MapWaypoint &wp = state.map.waypoints[i];
            lv_obj_set_pos(waypoint_dots[i], (lv_coord_t)(wp.x * 1.4f), (lv_coord_t)(wp.y * 0.65f));
            lv_obj_set_style_bg_color(waypoint_dots[i], lv_color_hex(wp.color_hex), LV_PART_MAIN);
            lv_label_set_text(waypoint_lbls[i], wp.label);
            lv_obj_set_pos(waypoint_lbls[i], (lv_coord_t)(wp.x * 1.4f) + 12, (lv_coord_t)(wp.y * 0.65f) - 2);
        }
    }

    void tickUpdate() {
        router.update(state);
        IGameModule * mod = getActiveModule();
        mod->updateStatusTab(state);

        lv_obj_set_pos(player_dot, (lv_coord_t)(state.map.x * 1.4f), (lv_coord_t)(state.map.y * 0.65f));
        char buf[64];
        mod->formatMapCoordText(buf, sizeof(buf), state);
        lv_label_set_text(coord_label, buf);

        // Config > diagnostico (so redesenha quando muda)
        int8_t conn = state.connected ? 1 : 0;
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

    void onMacroClicked(uint8_t slot) {
        if (slot == MACRO_STOP) {
            router.stopAll(state);
            lv_label_set_text_fmt(status_footer, "[%s] PARAR TUDO", getModeName());
            return;
        }
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
            "Generic Hub"
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
        lv_obj_set_style_pad_all(tab_status, 6, LV_PART_MAIN);
        lv_obj_set_style_pad_all(tab_macros, 0, LV_PART_MAIN);   // grade 4x3 + pager usam a aba inteira (480x224)

        // Aba 1: Status
        for (uint8_t i = 0; i < NUM_MODULES; i++) {
            modules[i]->buildStatusTab(tab_status);
        }

        // Aba 2: Mapa Tático Compacto
        coord_label = lv_label_create(tab_map);
        lv_obj_set_style_text_color(coord_label, lv_color_hex(0x00F5D4), LV_PART_MAIN);
        lv_obj_set_style_text_font(coord_label, &lv_font_montserrat_12, LV_PART_MAIN);
        lv_obj_align(coord_label, LV_ALIGN_TOP_MID, 0, -4);

        lv_obj_t * map_viewport = lv_obj_create(tab_map);
        lv_obj_set_size(map_viewport, 465, 178);
        lv_obj_align(map_viewport, LV_ALIGN_TOP_MID, 0, 16);
        lv_obj_set_style_bg_color(map_viewport, lv_color_hex(0x0B192C), LV_PART_MAIN);
        lv_obj_set_style_border_color(map_viewport, lv_color_hex(0x00F5D4), LV_PART_MAIN);
        lv_obj_set_style_pad_all(map_viewport, 0, LV_PART_MAIN);
        lv_obj_clear_flag(map_viewport, LV_OBJ_FLAG_SCROLLABLE);

        for (int g = 1; g <= 3; g++) {
            lv_obj_t * hline = lv_obj_create(map_viewport);
            lv_obj_set_size(hline, 460, 1);
            lv_obj_set_pos(hline, 0, g * 44);
            lv_obj_set_style_bg_color(hline, lv_color_hex(0x1E3E62), LV_PART_MAIN);
            lv_obj_set_style_border_width(hline, 0, LV_PART_MAIN);
        }
        for (int g = 1; g <= 5; g++) {
            lv_obj_t * vline = lv_obj_create(map_viewport);
            lv_obj_set_size(vline, 1, 175);
            lv_obj_set_pos(vline, g * 76, 0);
            lv_obj_set_style_bg_color(vline, lv_color_hex(0x1E3E62), LV_PART_MAIN);
            lv_obj_set_style_border_width(vline, 0, LV_PART_MAIN);
        }

        for (int i = 0; i < 3; i++) {
            waypoint_dots[i] = lv_obj_create(map_viewport);
            lv_obj_set_size(waypoint_dots[i], 10, 10);
            lv_obj_set_style_radius(waypoint_dots[i], 2, LV_PART_MAIN);
            lv_obj_set_style_border_width(waypoint_dots[i], 0, LV_PART_MAIN);

            waypoint_lbls[i] = lv_label_create(map_viewport);
            lv_obj_set_style_text_color(waypoint_lbls[i], lv_color_hex(0xCCCCCC), LV_PART_MAIN);
            lv_obj_set_style_text_font(waypoint_lbls[i], &lv_font_montserrat_12, LV_PART_MAIN);
        }

        player_dot = lv_obj_create(map_viewport);
        lv_obj_set_size(player_dot, 14, 14);
        lv_obj_set_style_radius(player_dot, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        lv_obj_set_style_bg_color(player_dot, lv_color_hex(0xFF1744), LV_PART_MAIN);
        lv_obj_set_style_border_color(player_dot, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        lv_obj_set_style_border_width(player_dot, 2, LV_PART_MAIN);

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
        lv_obj_set_style_bg_color(ip_textarea, lv_color_hex(0x1E1E36), LV_PART_MAIN);
        lv_obj_set_style_text_color(ip_textarea, lv_color_hex(0xFFFFFF), LV_PART_MAIN);

        lbl_diag = lv_label_create(tab_config);
        lv_label_set_recolor(lbl_diag, true);
        lv_obj_set_style_text_color(lbl_diag, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        lv_obj_set_style_text_font(lbl_diag, &lv_font_montserrat_12, LV_PART_MAIN);
        lv_obj_set_style_text_line_space(lbl_diag, 4, LV_PART_MAIN);
        lv_obj_align(lbl_diag, LV_ALIGN_TOP_LEFT, 5, 70);
        lv_label_set_text(lbl_diag, "USB: -");

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