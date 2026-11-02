#ifndef POKEMON_LEGENDS_ZA_MODULE_H
#define POKEMON_LEGENDS_ZA_MODULE_H

#include "../core/i_game_module.h"
#include "../ui/macro_menu.h"
#include <stdio.h>
#include <string.h>

// Layout Z-A para a tela de 3.5" (480x320), dentro do tabview do UIManager.
//
// Aba "Status": zona + barras de vida dos 6 Pokemon (nome/nivel, barra, HP, condicao).
//               (USB / macro ativa / execucoes ficam na aba Config.)
// Aba "Macros": mini menus. Tela inicial = 6 botoes (5 categorias + PARAR TUDO); dentro de cada
//               categoria, grade 3x2 de macros (90 px) + cabecalho [< Voltar] titulo [PARAR].
//
//   Menu                                         Categoria "Batalha"
//   [Batalha ][Cura    ][Movimento]              [< Voltar]  Batalha   [PARAR]
//   [Farm    ][Sistema ][PARAR TUDO]             [Combo Esq+Y][Esquiva][Auto-Catch]
//                                                [Spam Y]
//   o = ponto de estado (vermelho = parado, verde = ativo; no menu = ha macro ativa na categoria)
class PokemonLegendsZAModule : public IGameModule {
private:
    static constexpr uint8_t SLOTS     = 17;   // macros do Z-A (o FireRed usa ate MACRO_COUNT)
    static constexpr uint8_t CATS      = 5;
    static constexpr uint8_t PARTY     = 6;

    // ---- Aba Status ----
    lv_obj_t * container = nullptr;
    lv_obj_t * lbl_zone  = nullptr;
    lv_obj_t * lbl_name[PARTY]   = {};
    lv_obj_t * bar_hp[PARTY]     = {};
    lv_obj_t * lbl_hp[PARTY]     = {};
    lv_obj_t * lbl_status[PARTY] = {};
    PokemonSlot last_party[PARTY] = {};
    bool        party_drawn = false;
    float       last_mx = -1, last_my = -1;
    char        last_zone[28] = {};

    // ---- Aba Macros (componente compartilhado) ----
    MacroMenu menu;
    // Sem acentos: a fonte padrao do LVGL nao os tem
    const MacroMenu::Cat cats[5] = {
        { "Batalha/Cura", LV_SYMBOL_SHUFFLE, 6, { 1, 7, 6, 8, 15, 16 } },  // Combo, Esquiva, Auto-Catch, Spam Y, Curar HP, Curar Status
        { "Movimento", LV_SYMBOL_RIGHT,    3, { 10, 11, 14 } },        // Andar reto, Correr, Reset Cam
        { "Farm",      LV_SYMBOL_LOOP,     3, { 3, 2, 9 } },           // AFK Circulo, Turbo A, Spam B
        { "Sistema",   LV_SYMBOL_SETTINGS, 5, { 12, 13, 4, 5, 0 } },   // Menu, Home, Mapa, Cancelar, Parear
        { "Meus macros", LV_SYMBOL_EDIT,   6, { 100, 101, 102, 103, 104, 105 } }   // criados na pagina (aba Criar)
    };
    const MacroMenu::Slot slotdefs[SLOTS] = {
        {"Parear L+R", LV_SYMBOL_USB, false}, {"Combo Esq+Y", LV_SYMBOL_SHUFFLE, false},
        {"Turbo A", LV_SYMBOL_CHARGE, true}, {"AFK Circulo", LV_SYMBOL_LOOP, true},
        {"Mapa (+)", LV_SYMBOL_GPS, false}, {"Cancelar 2xB", LV_SYMBOL_LEFT, false},
        {"Auto-Catch", LV_SYMBOL_PLAY, false}, {"Esquiva", LV_SYMBOL_RIGHT, false},
        {"Spam Y", LV_SYMBOL_REFRESH, true}, {"Spam B", LV_SYMBOL_REFRESH, true},
        {"Andar reto", LV_SYMBOL_UP, true}, {"Correr", LV_SYMBOL_NEXT, true},
        {"Menu (X)", LV_SYMBOL_LIST, false}, {"Home", LV_SYMBOL_HOME, false},
        {"Reset Cam", LV_SYMBOL_EYE_OPEN, false}, {"Curar HP", LV_SYMBOL_PLUS, false},
        {"Curar Status", LV_SYMBOL_OK, false}
    };

    // ---- Status: helpers ----
    static uint32_t hpColor(uint16_t cur, uint16_t max) {
        if (max == 0) return 0x444466;
        uint32_t pct = (uint32_t)cur * 100 / max;
        return pct > 50 ? 0x00E676 : (pct > 20 ? 0xFFC400 : 0xFF1744);
    }

    void paintParty(uint8_t i, const PokemonSlot &p) {
        bool has = (p.max_hp > 0);
        if (!has) {
            lv_label_set_text(lbl_name[i], "---");
            lv_label_set_text(lbl_hp[i], "sem dados");
            lv_label_set_text(lbl_status[i], "");
            lv_bar_set_range(bar_hp[i], 0, 1);
            lv_bar_set_value(bar_hp[i], 0, LV_ANIM_OFF);
        } else {
            char nm[40];
            snprintf(nm, sizeof(nm), "%.12s Lv%u", p.name, (unsigned)p.level);
            lv_label_set_text(lbl_name[i], nm);
            lv_label_set_text_fmt(lbl_hp[i], "%u/%u", (unsigned)p.current_hp, (unsigned)p.max_hp);
            lv_label_set_text(lbl_status[i], p.status);
            lv_bar_set_range(bar_hp[i], 0, p.max_hp);
            lv_bar_set_value(bar_hp[i], p.current_hp, LV_ANIM_OFF);
        }
        lv_obj_set_style_bg_color(bar_hp[i], lv_color_hex(hpColor(p.current_hp, p.max_hp)), LV_PART_INDICATOR);
    }

    static bool sameSlot(const PokemonSlot &a, const PokemonSlot &b) {
        return a.level == b.level && a.current_hp == b.current_hp && a.max_hp == b.max_hp &&
               strncmp(a.name, b.name, sizeof(a.name)) == 0 && strncmp(a.status, b.status, sizeof(a.status)) == 0;
    }

    static lv_obj_t * makeTransparentPage(lv_obj_t * p) { return MacroMenu::makeTransparentPage(p); }

public:
    const char* getGameName() const override { return "Pokemon ZA"; }

    // ---------------- Aba Status ----------------
    void buildStatusTab(lv_obj_t * parent) override {
        container = makeTransparentPage(parent);

        lbl_zone = lv_label_create(container);
        lv_label_set_recolor(lbl_zone, true);
        lv_obj_set_style_text_font(lbl_zone, &lv_font_montserrat_12, LV_PART_MAIN);
        lv_obj_set_style_text_color(lbl_zone, lv_color_hex(0x00F5D4), LV_PART_MAIN);
        lv_obj_align(lbl_zone, LV_ALIGN_TOP_LEFT, 4, 0);
        lv_label_set_text(lbl_zone, "Zona: -");

        for (uint8_t i = 0; i < PARTY; i++) {
            lv_coord_t y = 20 + i * 30;

            lbl_name[i] = lv_label_create(container);
            lv_obj_set_style_text_font(lbl_name[i], &lv_font_montserrat_12, LV_PART_MAIN);
            lv_obj_set_style_text_color(lbl_name[i], lv_color_hex(0xFFFFFF), LV_PART_MAIN);
            lv_obj_set_width(lbl_name[i], 118);
            lv_label_set_long_mode(lbl_name[i], LV_LABEL_LONG_CLIP);
            lv_obj_set_pos(lbl_name[i], 4, y + 5);

            bar_hp[i] = lv_bar_create(container);
            lv_obj_set_size(bar_hp[i], 190, 12);
            lv_obj_set_pos(bar_hp[i], 126, y + 6);
            lv_obj_set_style_bg_color(bar_hp[i], lv_color_hex(0x1F1F38), LV_PART_MAIN);
            lv_obj_set_style_radius(bar_hp[i], 4, LV_PART_MAIN);
            lv_obj_set_style_radius(bar_hp[i], 4, LV_PART_INDICATOR);
            lv_bar_set_range(bar_hp[i], 0, 1);
            lv_bar_set_value(bar_hp[i], 0, LV_ANIM_OFF);

            lbl_hp[i] = lv_label_create(container);
            lv_obj_set_style_text_font(lbl_hp[i], &lv_font_montserrat_12, LV_PART_MAIN);
            lv_obj_set_style_text_color(lbl_hp[i], lv_color_hex(0xCCCCDD), LV_PART_MAIN);
            lv_obj_set_width(lbl_hp[i], 80);
            lv_label_set_long_mode(lbl_hp[i], LV_LABEL_LONG_CLIP);
            lv_obj_set_pos(lbl_hp[i], 322, y + 5);

            lbl_status[i] = lv_label_create(container);
            lv_obj_set_style_text_font(lbl_status[i], &lv_font_montserrat_12, LV_PART_MAIN);
            lv_obj_set_style_text_color(lbl_status[i], lv_color_hex(0xFFC400), LV_PART_MAIN);
            lv_obj_set_width(lbl_status[i], 52);
            lv_label_set_long_mode(lbl_status[i], LV_LABEL_LONG_CLIP);
            lv_obj_set_pos(lbl_status[i], 406, y + 5);

            paintParty(i, last_party[i]);   // estado vazio inicial
        }
        party_drawn = true;
    }

    // ---------------- Aba Macros ----------------
    bool hasCustomMacroTab() const override { return true; }

    void buildMacroTab(lv_obj_t * parent, MacroHandler cb, void * hctx) override {
        menu.build(parent, cats, 5, slotdefs, SLOTS, cb, hctx);
    }

    // Chamado a cada 100 ms pelo UIManager (so para o jogo ativo). So mexe no LVGL quando algo muda.
    void updateStatusTab(const GameState &state) override {
        menu.update(state.active_macro_slot, &state.um);

        for (uint8_t i = 0; i < PARTY; i++) {
            if (!sameSlot(state.party[i], last_party[i])) {
                last_party[i] = state.party[i];
                paintParty(i, last_party[i]);
            }
        }

        if (state.map.x != last_mx || state.map.y != last_my ||
            strncmp(state.map.zone_name, last_zone, sizeof(last_zone)) != 0) {
            last_mx = state.map.x; last_my = state.map.y;
            snprintf(last_zone, sizeof(last_zone), "%s", state.map.zone_name);
            if (last_zone[0]) {
                char zb[64];   // snprintf do libc (o do LVGL nao formata float)
                snprintf(zb, sizeof(zb), "Zona: %s  X:%.0f Y:%.0f", last_zone, state.map.x, state.map.y);
                lv_label_set_text(lbl_zone, zb);
            } else lv_label_set_text(lbl_zone, "Zona: -");
        }
    }

    void formatMapCoordText(char * buf, size_t buf_size, const GameState &state) override {
        snprintf(buf, buf_size, "Lumiose | X: %.0f  Y: %.0f", state.map.x, state.map.y);
    }

    const char* getMacroLabel(uint8_t slot) const override {
        return menu.label(slot);
    }

    void setVisible(bool visible) override {
        if (container) { if (visible) lv_obj_clear_flag(container, LV_OBJ_FLAG_HIDDEN); else lv_obj_add_flag(container, LV_OBJ_FLAG_HIDDEN); }
        menu.setVisible(visible);
    }
};

#endif // POKEMON_LEGENDS_ZA_MODULE_H