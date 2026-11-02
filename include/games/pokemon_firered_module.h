#ifndef POKEMON_FIRERED_MODULE_H
#define POKEMON_FIRERED_MODULE_H

#include "../core/i_game_module.h"
#include "../ui/macro_menu.h"
#include <stdio.h>
#include <string.h>

// Pokemon FireRed (Switch) - sem leitura de memoria: o foco e diversao e macros.
//
// Status (480x224):  contador de encontros [-1][+1][0 segurar] | chance de shiny (barra)
//                    estatisticas da sessao | 8 insignias de Kanto (toque para marcar)
//                    conquistas (n/13 + ultima desbloqueada)
// Macros: Caca, Dialogo, Menu, Cura, Movimento, Batalha (mini menus 3x2)
class PokemonFireRedModule : public IGameModule {
private:
    static constexpr uint8_t SLOTS = 25;
    static constexpr uint8_t CATS  = 5;

    struct BtnCtx { PokemonFireRedModule * self; uint8_t code; };

    MacroMenu menu;
    MacroHandler handler = nullptr;
    void *       handler_ctx = nullptr;

    // ---- Macros ----
    const MacroMenu::Cat cats[CATS] = {
        { "Caca",      LV_SYMBOL_EYE_OPEN, 6, { 0, 24, 1, 2, 3, 5 } },    // Reset, Testar 1 ciclo, CacaReset, Grama, SpamA, Texto
        { "Menu/Cura", LV_SYMBOL_LIST,     6, { 6, 7, 8, 9, 10, 11 } },   // Menu, Bolsa, Salvar, Fechar, Pocao, Centro
        { "Movimento", LV_SYMBOL_NEXT,     6, { 12, 13, 14, 15, 16, 17 } },// correr 4 direcoes + patrulhas
        { "Batalha",   LV_SYMBOL_SHUFFLE,  6, { 18, 19, 20, 21, 22, 23 } },// golpes 1-4, trocar, fugir
        { "Meus macros", LV_SYMBOL_EDIT,   6, { 100, 101, 102, 103, 104, 105 } }   // criados na pagina (aba Criar)
    };
    const MacroMenu::Slot slotdefs[SLOTS] = {
        {"Soft Reset", LV_SYMBOL_POWER, false}, {"Caca Reset", LV_SYMBOL_REFRESH, true},
        {"Grama <->", LV_SYMBOL_SHUFFLE, true}, {"Spam A", LV_SYMBOL_CHARGE, true},
        {"Spam B", LV_SYMBOL_REFRESH, true}, {"Avancar texto", LV_SYMBOL_PLAY, false},
        {"Menu (+)", LV_SYMBOL_LIST, false}, {"Bolsa", LV_SYMBOL_DIRECTORY, false},
        {"Salvar", LV_SYMBOL_SAVE, false}, {"Fechar menus", LV_SYMBOL_CLOSE, false},
        {"Pocao", LV_SYMBOL_PLUS, false}, {"Centro Pokemon", LV_SYMBOL_OK, false},
        {"Correr cima", LV_SYMBOL_UP, true}, {"Correr baixo", LV_SYMBOL_DOWN, true},
        {"Correr esq", LV_SYMBOL_LEFT, true}, {"Correr dir", LV_SYMBOL_RIGHT, true},
        {"Patrulha <->", LV_SYMBOL_LOOP, true}, {"Patrulha ^v", LV_SYMBOL_LOOP, true},
        {"Golpe 1", LV_SYMBOL_PLAY, false}, {"Golpe 2", LV_SYMBOL_PLAY, false},
        {"Golpe 3", LV_SYMBOL_PLAY, false}, {"Golpe 4", LV_SYMBOL_PLAY, false},
        {"Trocar", LV_SYMBOL_SHUFFLE, false}, {"Fugir", LV_SYMBOL_RIGHT, false},
        {"Testar 1 ciclo", LV_SYMBOL_EYE_OPEN, false}
    };

    // ---- Status ----
    lv_obj_t * container = nullptr;
    lv_obj_t * lbl_count = nullptr, * lbl_shiny = nullptr, * lbl_stats = nullptr, * lbl_ach = nullptr;
    lv_obj_t * bar_shiny = nullptr;
    lv_obj_t * badge_btn[8] = {};
    BtnCtx     bctx[11] = {};
    FireRedStats last = {};
    bool       drawn = false;

    static constexpr const char * BADGES[8] = { "Pedra", "Cascata", "Trovao", "Arco-iris", "Alma", "Pantano", "Vulcao", "Terra" };

    static void onCode(lv_event_t * e) {
        BtnCtx * c = static_cast<BtnCtx *>(lv_event_get_user_data(e));
        if (c && c->self && c->self->handler) c->self->handler(c->self->handler_ctx, c->code);
    }

    lv_obj_t * smallBtn(lv_obj_t * parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h,
                        const char * text, uint32_t bg, uint32_t border, BtnCtx * ctx, lv_event_code_t ev) {
        lv_obj_t * b = lv_btn_create(parent);
        lv_obj_set_size(b, w, h);
        lv_obj_set_pos(b, x, y);
        lv_obj_set_style_radius(b, 8, LV_PART_MAIN);
        lv_obj_set_style_border_width(b, 2, LV_PART_MAIN);
        lv_obj_set_style_border_color(b, lv_color_hex(border), LV_PART_MAIN);
        lv_obj_set_style_bg_color(b, lv_color_hex(bg), LV_PART_MAIN);
        lv_obj_set_style_shadow_width(b, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(b, 1, LV_PART_MAIN);
        lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_event_cb(b, onCode, ev, ctx);
        lv_obj_t * l = lv_label_create(b);
        lv_label_set_recolor(l, true);
        lv_obj_set_style_text_font(l, &lv_font_montserrat_12, LV_PART_MAIN);
        lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_label_set_text(l, text);
        lv_obj_center(l);
        return b;
    }

    static void fmtTime(char * out, size_t n, uint32_t s) {
        snprintf(out, n, "%02lu:%02lu:%02lu", (unsigned long)(s / 3600), (unsigned long)((s / 60) % 60), (unsigned long)(s % 60));
    }

    void paintBadge(uint8_t i, bool on) {
        if (!badge_btn[i]) return;
        lv_obj_set_style_bg_color(badge_btn[i], lv_color_hex(on ? 0x6B4E00 : 0x1F1F38), LV_PART_MAIN);
        lv_obj_set_style_border_color(badge_btn[i], lv_color_hex(on ? 0xFFC400 : 0x2D3E66), LV_PART_MAIN);
        lv_obj_set_style_text_color(lv_obj_get_child(badge_btn[i], 0), lv_color_hex(on ? 0xFFE27A : 0x777799), LV_PART_MAIN);
    }

    void redraw(const FireRedStats & f) {
        char b[96];
        snprintf(b, sizeof(b), "%lu", (unsigned long)f.encounters);
        lv_label_set_text(lbl_count, b);

        float pct = f.shinyChance() * 100.0f;     // snprintf do libc (o do LVGL nao formata float)
        snprintf(b, sizeof(b), "Chance de shiny acumulada: %.1f%%  (1/8192 por encontro)", pct);
        lv_label_set_text(lbl_shiny, b);
        lv_bar_set_value(bar_shiny, (int32_t)(pct * 10.0f), LV_ANIM_OFF);
        uint32_t col = pct >= 63.0f ? 0xFFC400 : (pct >= 30.0f ? 0x00F5D4 : 0x4C8DFF);
        lv_obj_set_style_bg_color(bar_shiny, lv_color_hex(col), LV_PART_INDICATOR);

        char t[16]; fmtTime(t, sizeof(t), f.hunt_s);
        snprintf(b, sizeof(b), "Resets %lu  |  Passos ~%lu  |  Caca %s  |  Macros %lu",
                 (unsigned long)f.soft_resets, (unsigned long)f.steps_est, t, (unsigned long)f.runs);
        lv_label_set_text(lbl_stats, b);

        for (uint8_t i = 0; i < 8; i++) paintBadge(i, f.badges & (1u << i));

        if (f.last_ach >= 0)
            snprintf(b, sizeof(b), "#FFC400 %s %u/%u#  Ultima: %s", LV_SYMBOL_OK, (unsigned)f.achCount(),
                     (unsigned)FireRedStats::ACH_COUNT, FireRedStats::achName((uint8_t)f.last_ach));
        else
            snprintf(b, sizeof(b), "#8888AA Conquistas 0/%u - comece a jogar!#", (unsigned)FireRedStats::ACH_COUNT);
        lv_label_set_text(lbl_ach, b);
    }

    static bool same(const FireRedStats & a, const FireRedStats & b) {
        return a.encounters == b.encounters && a.soft_resets == b.soft_resets && a.steps_est == b.steps_est &&
               a.hunt_s == b.hunt_s && a.runs == b.runs && a.badges == b.badges && a.ach_mask == b.ach_mask &&
               a.last_ach == b.last_ach;
    }

public:
    const char* getGameName() const override { return "Pokemon FireRed"; }

    void buildStatusTab(lv_obj_t * parent) override {
        container = MacroMenu::makeTransparentPage(parent);
        // contexts: [0..7] insignias, 8 = -1, 9 = +1, 10 = zerar
        for (uint8_t i = 0; i < 8; i++) bctx[i] = { this, (uint8_t)(FR_CODE_BADGE0 + i) };
        bctx[8] = { this, FR_CODE_ENC_DEC }; bctx[9] = { this, FR_CODE_ENC_INC }; bctx[10] = { this, FR_CODE_ENC_RST };

        lv_obj_t * cap = lv_label_create(container);
        lv_obj_set_style_text_font(cap, &lv_font_montserrat_12, LV_PART_MAIN);
        lv_obj_set_style_text_color(cap, lv_color_hex(0xFF8A3D), LV_PART_MAIN);
        lv_label_set_text(cap, "ENCONTROS");
        lv_obj_set_pos(cap, 8, 2);

        lbl_count = lv_label_create(container);
        lv_obj_set_style_text_font(lbl_count, &lv_font_montserrat_28, LV_PART_MAIN);
        lv_obj_set_style_text_color(lbl_count, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        lv_obj_set_width(lbl_count, 190);
        lv_label_set_long_mode(lbl_count, LV_LABEL_LONG_CLIP);
        lv_obj_set_pos(lbl_count, 8, 16);
        lv_label_set_text(lbl_count, "0");

        smallBtn(container, 226, 4, 70, 44, "-1", 0x1F1F38, 0x2D3E66, &bctx[8], LV_EVENT_CLICKED);
        smallBtn(container, 302, 4, 90, 44, "#00F5D4 +1#", 0x0F3460, 0x1F4E8C, &bctx[9], LV_EVENT_CLICKED);
        smallBtn(container, 398, 4, 74, 44, "Zerar\n#8888AA segure#", 0x3A1424, 0x8C2B3F, &bctx[10], LV_EVENT_LONG_PRESSED);

        lbl_shiny = lv_label_create(container);
        lv_obj_set_style_text_font(lbl_shiny, &lv_font_montserrat_12, LV_PART_MAIN);
        lv_obj_set_style_text_color(lbl_shiny, lv_color_hex(0xCCCCDD), LV_PART_MAIN);
        lv_obj_set_pos(lbl_shiny, 8, 54);
        lv_label_set_text(lbl_shiny, "Chance de shiny acumulada: 0.0%");

        bar_shiny = lv_bar_create(container);
        lv_obj_set_size(bar_shiny, 464, 10);
        lv_obj_set_pos(bar_shiny, 8, 72);
        lv_bar_set_range(bar_shiny, 0, 1000);
        lv_bar_set_value(bar_shiny, 0, LV_ANIM_OFF);
        lv_obj_set_style_bg_color(bar_shiny, lv_color_hex(0x1F1F38), LV_PART_MAIN);
        lv_obj_set_style_radius(bar_shiny, 4, LV_PART_MAIN);
        lv_obj_set_style_radius(bar_shiny, 4, LV_PART_INDICATOR);

        lbl_stats = lv_label_create(container);
        lv_obj_set_style_text_font(lbl_stats, &lv_font_montserrat_12, LV_PART_MAIN);
        lv_obj_set_style_text_color(lbl_stats, lv_color_hex(0x9AA3C7), LV_PART_MAIN);
        lv_obj_set_width(lbl_stats, 464);
        lv_label_set_long_mode(lbl_stats, LV_LABEL_LONG_CLIP);
        lv_obj_set_pos(lbl_stats, 8, 90);

        lv_obj_t * cap2 = lv_label_create(container);
        lv_obj_set_style_text_font(cap2, &lv_font_montserrat_12, LV_PART_MAIN);
        lv_obj_set_style_text_color(cap2, lv_color_hex(0xFF8A3D), LV_PART_MAIN);
        lv_label_set_text(cap2, "INSIGNIAS DE KANTO (toque para marcar)");
        lv_obj_set_pos(cap2, 8, 112);

        for (uint8_t i = 0; i < 8; i++) {
            badge_btn[i] = smallBtn(container, 6 + i * 58, 130, 55, 34, BADGES[i], 0x1F1F38, 0x2D3E66, &bctx[i], LV_EVENT_CLICKED);
            paintBadge(i, false);
        }

        lbl_ach = lv_label_create(container);
        lv_label_set_recolor(lbl_ach, true);
        lv_obj_set_style_text_font(lbl_ach, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_set_style_text_color(lbl_ach, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        lv_obj_set_width(lbl_ach, 464);
        lv_label_set_long_mode(lbl_ach, LV_LABEL_LONG_CLIP);
        lv_obj_set_pos(lbl_ach, 8, 176);

        last = FireRedStats();
        last.encounters = 0xFFFFFFFF;   // forca o primeiro redraw
        drawn = true;
    }

    bool hasCustomMacroTab() const override { return true; }

    void buildMacroTab(lv_obj_t * parent, MacroHandler cb, void * hctx) override {
        handler = cb; handler_ctx = hctx;
        menu.build(parent, cats, CATS, slotdefs, SLOTS, cb, hctx);
    }

    void updateStatusTab(const GameState &state) override {
        menu.update(state.active_macro_slot, &state.um);
        if (drawn && !same(state.fr, last)) {
            last = state.fr;
            redraw(last);
        }
    }

    void formatMapCoordText(char * buf, size_t n, const GameState &) override {
        snprintf(buf, n, "FireRed | sem mapa");
    }

    const char* getMacroLabel(uint8_t slot) const override { return menu.label(slot); }

    void setVisible(bool visible) override {
        if (container) { if (visible) lv_obj_clear_flag(container, LV_OBJ_FLAG_HIDDEN); else lv_obj_add_flag(container, LV_OBJ_FLAG_HIDDEN); }
        menu.setVisible(visible);
    }
};

#endif // POKEMON_FIRERED_MODULE_H