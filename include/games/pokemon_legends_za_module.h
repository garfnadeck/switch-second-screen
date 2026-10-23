#ifndef POKEMON_LEGENDS_ZA_MODULE_H
#define POKEMON_LEGENDS_ZA_MODULE_H

#include "../core/i_game_module.h"
#include <stdio.h>
#include <string.h>

// Layout Z-A para a tela de 3.5" (480x320), dentro do tabview do UIManager.
//
// Aba "Status": zona + barras de vida dos 6 Pokemon (nome/nivel, barra, HP, condicao).
//               (USB / macro ativa / execucoes ficam na aba Config.)
// Aba "Macros": 17 macros em 2 paginas de 4x3 botoes (~107x58 px) + pager "< 1/2 >".
//
//   Pagina 1                                   Pagina 2
//   [Parear][Combo ][Turbo A][AFK   ]          [Menu X][Home  ][ResetCam][CurarHP]
//   [Mapa  ][Cancel][AutoCtch][Esq. ]          [CurarSt]
//   [SpamY ][SpamB ][Andar  ][Correr]
//   o = ponto de estado (vermelho = parado, verde = ativo)
class PokemonLegendsZAModule : public IGameModule {
private:
    static constexpr uint8_t SLOTS     = MACRO_COUNT;   // 17
    static constexpr uint8_t PER_PAGE  = 12;            // 4 colunas x 3 linhas
    static constexpr uint8_t PAGES     = (SLOTS + PER_PAGE - 1) / PER_PAGE;
    static constexpr uint8_t PARTY     = 6;

    struct SlotCtx { PokemonLegendsZAModule * self; uint8_t slot; };

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

    // ---- Aba Macros ----
    lv_obj_t * macro_container = nullptr;
    lv_obj_t * page_cont[PAGES] = {};
    lv_obj_t * lbl_pager = nullptr;
    lv_obj_t * btn[SLOTS] = {};
    lv_obj_t * dot[SLOTS] = {};
    SlotCtx    ctx[SLOTS] = {};
    MacroHandler handler     = nullptr;
    void *       handler_ctx = nullptr;
    uint8_t    cur_page = 0;
    int8_t     last_active = -2;

    // Sem acentos: a fonte padrao do LVGL nao os tem
    const char* macros[SLOTS] = {
        "Parear L+R", "Combo Esq+Y", "Turbo A", "AFK Circulo",
        "Mapa (+)", "Cancelar 2xB", "Auto-Catch", "Esquiva",
        "Spam Y", "Spam B", "Andar reto", "Correr",
        "Menu (X)", "Home", "Reset Cam", "Curar HP", "Curar Status"
    };
    const char* icons[SLOTS] = {
        LV_SYMBOL_USB, LV_SYMBOL_SHUFFLE, LV_SYMBOL_CHARGE, LV_SYMBOL_LOOP,
        LV_SYMBOL_GPS, LV_SYMBOL_LEFT, LV_SYMBOL_PLAY, LV_SYMBOL_RIGHT,
        LV_SYMBOL_REFRESH, LV_SYMBOL_REFRESH, LV_SYMBOL_UP, LV_SYMBOL_NEXT,
        LV_SYMBOL_LIST, LV_SYMBOL_HOME, LV_SYMBOL_EYE_OPEN, LV_SYMBOL_PLUS, LV_SYMBOL_OK
    };
    // Toggles (ficam ligados ate tocar de novo): Turbo A, AFK, Spam Y, Spam B, Andar, Correr
    static bool isToggle(uint8_t s) { return s == 2 || s == 3 || (s >= 8 && s <= 11); }

    static void onClick(lv_event_t * e) {
        SlotCtx * c = static_cast<SlotCtx *>(lv_event_get_user_data(e));
        if (c && c->self && c->self->handler) c->self->handler(c->self->handler_ctx, c->slot);
    }

    void paintSlot(uint8_t i, bool active) {
        if (!btn[i] || !dot[i]) return;
        uint32_t dot_col = active ? 0x00E676 : 0xFF1744;
        lv_obj_set_style_bg_color(btn[i], lv_color_hex(active ? 0x133E35 : 0x0F3460), LV_PART_MAIN);
        lv_obj_set_style_border_color(btn[i], lv_color_hex(active ? 0x00E676 : 0x1F4E8C), LV_PART_MAIN);
        lv_obj_set_style_bg_color(dot[i], lv_color_hex(dot_col), LV_PART_MAIN);
    }

    void buildSlot(uint8_t i, lv_obj_t * parent) {
        btn[i] = lv_btn_create(parent);
        lv_obj_set_size(btn[i], LV_PCT(23), 58);   // 4 colunas x 3 linhas (flex da pagina)
        lv_obj_set_style_radius(btn[i], 8, LV_PART_MAIN);
        lv_obj_set_style_border_width(btn[i], 2, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(btn[i], 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(btn[i], 2, LV_PART_MAIN);
        lv_obj_set_style_bg_color(btn[i], lv_color_hex(0x1F4E8C), LV_STATE_PRESSED);
        lv_obj_clear_flag(btn[i], LV_OBJ_FLAG_SCROLLABLE);

        ctx[i] = { this, i };
        lv_obj_add_event_cb(btn[i], onClick, LV_EVENT_CLICKED, &ctx[i]);

        // Icone (ciano) + rotulo (+ tag LOOP nos toggles, na mesma linha do rotulo)
        lv_obj_t * lbl = lv_label_create(btn[i]);
        lv_label_set_recolor(lbl, true);
        lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(lbl, LV_PCT(96));
        lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, LV_PART_MAIN);
        char text[80];
        snprintf(text, sizeof(text), "#00F5D4 %s#\n%s%s", icons[i], macros[i],
                 isToggle(i) ? " #8888AA ~#" : "");
        lv_label_set_text(lbl, text);
        lv_obj_center(lbl);

        // Ponto de estado no canto superior direito
        dot[i] = lv_obj_create(btn[i]);
        lv_obj_set_size(dot[i], 10, 10);
        lv_obj_align(dot[i], LV_ALIGN_TOP_RIGHT, 2, -2);
        lv_obj_set_style_radius(dot[i], LV_RADIUS_CIRCLE, LV_PART_MAIN);
        lv_obj_set_style_border_width(dot[i], 0, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(dot[i], 0, LV_PART_MAIN);   // sem sombra: poupa RAM/CPU
        lv_obj_clear_flag(dot[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(dot[i], LV_OBJ_FLAG_SCROLLABLE);

        paintSlot(i, false);
    }

    void showPage(uint8_t p) {
        if (p >= PAGES) p = 0;
        cur_page = p;
        for (uint8_t i = 0; i < PAGES; i++) {
            if (!page_cont[i]) continue;
            if (i == p) lv_obj_clear_flag(page_cont[i], LV_OBJ_FLAG_HIDDEN);
            else        lv_obj_add_flag(page_cont[i], LV_OBJ_FLAG_HIDDEN);
        }
        if (lbl_pager) lv_label_set_text_fmt(lbl_pager, "%u/%u", (unsigned)(p + 1), (unsigned)PAGES);
    }

    static void onPrev(lv_event_t * e) {
        auto * self = static_cast<PokemonLegendsZAModule *>(lv_event_get_user_data(e));
        self->showPage((self->cur_page + PAGES - 1) % PAGES);
    }
    static void onNext(lv_event_t * e) {
        auto * self = static_cast<PokemonLegendsZAModule *>(lv_event_get_user_data(e));
        self->showPage((self->cur_page + 1) % PAGES);
    }

    lv_obj_t * makePagerBtn(lv_obj_t * parent, const char * symbol, lv_event_cb_t cb, lv_coord_t x) {
        lv_obj_t * b = lv_btn_create(parent);
        lv_obj_set_size(b, 56, 24);
        lv_obj_align(b, LV_ALIGN_BOTTOM_MID, x, 0);
        lv_obj_set_style_radius(b, 6, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(b, 0, LV_PART_MAIN);
        lv_obj_set_style_bg_color(b, lv_color_hex(0x1F1F38), LV_PART_MAIN);
        lv_obj_set_style_pad_all(b, 0, LV_PART_MAIN);
        lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, this);
        lv_obj_t * l = lv_label_create(b);
        lv_label_set_text(l, symbol);
        lv_obj_set_style_text_font(l, &lv_font_montserrat_12, LV_PART_MAIN);
        lv_obj_center(l);
        return b;
    }

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

    static lv_obj_t * makeTransparentPage(lv_obj_t * parent) {
        lv_obj_t * o = lv_obj_create(parent);
        lv_obj_set_size(o, LV_PCT(100), LV_PCT(100));
        lv_obj_set_pos(o, 0, 0);
        lv_obj_set_style_pad_all(o, 0, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(o, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_border_width(o, 0, LV_PART_MAIN);
        lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
        return o;
    }

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
        handler = cb;
        handler_ctx = hctx;
        macro_container = makeTransparentPage(parent);

        for (uint8_t p = 0; p < PAGES; p++) {
            lv_obj_t * pg = lv_obj_create(macro_container);
            lv_obj_set_size(pg, LV_PCT(100), 190);
            lv_obj_set_pos(pg, 0, 0);
            lv_obj_set_style_pad_all(pg, 0, LV_PART_MAIN);
            lv_obj_set_style_bg_opa(pg, LV_OPA_TRANSP, LV_PART_MAIN);
            lv_obj_set_style_border_width(pg, 0, LV_PART_MAIN);
            lv_obj_clear_flag(pg, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_flex_flow(pg, LV_FLEX_FLOW_ROW_WRAP);
            lv_obj_set_flex_align(pg, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
            lv_obj_set_style_pad_column(pg, 6, LV_PART_MAIN);
            lv_obj_set_style_pad_row(pg, 8, LV_PART_MAIN);
            lv_obj_set_style_pad_left(pg, 3, LV_PART_MAIN);
            lv_obj_set_style_pad_top(pg, 4, LV_PART_MAIN);
            page_cont[p] = pg;
            for (uint8_t i = p * PER_PAGE; i < SLOTS && i < (p + 1) * PER_PAGE; i++) buildSlot(i, pg);
        }

        // Pager "<  1/2  >" na base da aba
        makePagerBtn(macro_container, LV_SYMBOL_LEFT,  onPrev, -60);
        makePagerBtn(macro_container, LV_SYMBOL_RIGHT, onNext,  60);
        lbl_pager = lv_label_create(macro_container);
        lv_obj_set_style_text_font(lbl_pager, &lv_font_montserrat_12, LV_PART_MAIN);
        lv_obj_set_style_text_color(lbl_pager, lv_color_hex(0x8888AA), LV_PART_MAIN);
        lv_obj_align(lbl_pager, LV_ALIGN_BOTTOM_MID, 0, -5);
        showPage(0);
    }

    // Chamado a cada 100 ms pelo UIManager (so para o jogo ativo). So mexe no LVGL quando algo muda.
    void updateStatusTab(const GameState &state) override {
        if (state.active_macro_slot != last_active) {
            last_active = state.active_macro_slot;
            for (uint8_t i = 0; i < SLOTS; i++) paintSlot(i, (int8_t)i == last_active);
        }

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
        return (slot < SLOTS) ? macros[slot] : "";
    }

    void setVisible(bool visible) override {
        lv_obj_t * pages[2] = { container, macro_container };
        for (lv_obj_t * o : pages) {
            if (!o) continue;
            if (visible) lv_obj_clear_flag(o, LV_OBJ_FLAG_HIDDEN);
            else lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
        }
    }
};

#endif // POKEMON_LEGENDS_ZA_MODULE_H