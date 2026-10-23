#ifndef POKEMON_LEGENDS_ZA_MODULE_H
#define POKEMON_LEGENDS_ZA_MODULE_H

#include "../core/i_game_module.h"
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
    static constexpr uint8_t SLOTS     = MACRO_COUNT;   // 17
    static constexpr uint8_t CATS      = 5;
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
    struct Cat { const char * name; const char * icon; uint8_t n; uint8_t slots[6]; };
    const Cat cats[CATS] = {
        { "Batalha",   LV_SYMBOL_SHUFFLE,  4, { 1, 7, 6, 8 } },        // Combo, Esquiva, Auto-Catch, Spam Y
        { "Cura",      LV_SYMBOL_PLUS,     2, { 15, 16 } },            // Curar HP, Curar Status
        { "Movimento", LV_SYMBOL_RIGHT,    3, { 10, 11, 14 } },        // Andar reto, Correr, Reset Cam
        { "Farm",      LV_SYMBOL_LOOP,     3, { 3, 2, 9 } },           // AFK Circulo, Turbo A, Spam B
        { "Sistema",   LV_SYMBOL_SETTINGS, 5, { 12, 13, 4, 5, 0 } }    // Menu, Home, Mapa, Cancelar, Parear
    };
    struct NavCtx { PokemonLegendsZAModule * self; int8_t view; };   // view: -1 = menu, 0..CATS-1 = categoria
    lv_obj_t * macro_container = nullptr;
    lv_obj_t * view_menu = nullptr;
    lv_obj_t * view_cat[CATS] = {};
    lv_obj_t * dot_cat[CATS] = {};
    NavCtx     nav_menu[CATS]  = {};
    NavCtx     nav_back  = {};
    SlotCtx    stop_ctx  = {};
    lv_obj_t * btn[SLOTS] = {};
    lv_obj_t * dot[SLOTS] = {};
    SlotCtx    ctx[SLOTS] = {};
    MacroHandler handler     = nullptr;
    void *       handler_ctx = nullptr;
    int8_t     cur_view = -1;
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
        lv_obj_set_size(btn[i], LV_PCT(32), 84);   // 3 colunas x 2 linhas (flex da categoria)
        lv_obj_set_style_radius(btn[i], 8, LV_PART_MAIN);
        lv_obj_set_style_border_width(btn[i], 2, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(btn[i], 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(btn[i], 4, LV_PART_MAIN);
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
        lv_obj_set_size(dot[i], 12, 12);
        lv_obj_align(dot[i], LV_ALIGN_TOP_RIGHT, 0, 0);
        lv_obj_set_style_radius(dot[i], LV_RADIUS_CIRCLE, LV_PART_MAIN);
        lv_obj_set_style_border_width(dot[i], 0, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(dot[i], 0, LV_PART_MAIN);   // sem sombra: poupa RAM/CPU
        lv_obj_clear_flag(dot[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(dot[i], LV_OBJ_FLAG_SCROLLABLE);

        paintSlot(i, false);
    }

    void showView(int8_t v) {
        cur_view = v;
        if (view_menu) { if (v < 0) lv_obj_clear_flag(view_menu, LV_OBJ_FLAG_HIDDEN); else lv_obj_add_flag(view_menu, LV_OBJ_FLAG_HIDDEN); }
        for (uint8_t i = 0; i < CATS; i++) {
            if (!view_cat[i]) continue;
            if ((int8_t)i == v) lv_obj_clear_flag(view_cat[i], LV_OBJ_FLAG_HIDDEN);
            else                lv_obj_add_flag(view_cat[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
    static void onNav(lv_event_t * e) {
        NavCtx * n = static_cast<NavCtx *>(lv_event_get_user_data(e));
        if (n && n->self) n->self->showView(n->view);
    }

    // Botao pequeno/medio com texto (menu, voltar, parar)
    lv_obj_t * makeBtn(lv_obj_t * parent, lv_coord_t w, lv_coord_t h, uint32_t bg, uint32_t border,
                       const char * text, lv_event_cb_t cb, void * user) {
        lv_obj_t * b = lv_btn_create(parent);
        lv_obj_set_size(b, w, h);
        lv_obj_set_style_radius(b, 8, LV_PART_MAIN);
        lv_obj_set_style_border_width(b, 2, LV_PART_MAIN);
        lv_obj_set_style_border_color(b, lv_color_hex(border), LV_PART_MAIN);
        lv_obj_set_style_shadow_width(b, 0, LV_PART_MAIN);
        lv_obj_set_style_bg_color(b, lv_color_hex(bg), LV_PART_MAIN);
        lv_obj_set_style_pad_all(b, 2, LV_PART_MAIN);
        lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, user);
        lv_obj_t * l = lv_label_create(b);
        lv_label_set_recolor(l, true);
        lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_set_style_text_font(l, &lv_font_montserrat_12, LV_PART_MAIN);
        lv_label_set_text(l, text);
        lv_obj_center(l);
        return b;
    }

    lv_obj_t * makeGrid(lv_obj_t * parent, lv_coord_t y, lv_coord_t h) {
        lv_obj_t * g = lv_obj_create(parent);
        lv_obj_set_size(g, LV_PCT(100), h);
        lv_obj_set_pos(g, 0, y);
        lv_obj_set_style_pad_all(g, 0, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(g, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_border_width(g, 0, LV_PART_MAIN);
        lv_obj_clear_flag(g, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_flex_flow(g, LV_FLEX_FLOW_ROW_WRAP);
        lv_obj_set_flex_align(g, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        lv_obj_set_style_pad_column(g, 6, LV_PART_MAIN);
        lv_obj_set_style_pad_row(g, 8, LV_PART_MAIN);
        lv_obj_set_style_pad_left(g, 3, LV_PART_MAIN);
        lv_obj_set_style_pad_top(g, 4, LV_PART_MAIN);
        return g;
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
        stop_ctx = { this, MACRO_STOP };
        macro_container = makeTransparentPage(parent);

        // ---- Menu: 5 categorias + PARAR TUDO (grade 3x2) ----
        view_menu = makeGrid(macro_container, 0, 224);
        for (uint8_t c = 0; c < CATS; c++) {
            char t[80];
            snprintf(t, sizeof(t), "#00F5D4 %s#\n%s\n#8888AA %u macros#", cats[c].icon, cats[c].name, (unsigned)cats[c].n);
            nav_menu[c] = { this, (int8_t)c };
            lv_obj_t * b = makeBtn(view_menu, LV_PCT(32), 90, 0x0F3460, 0x1F4E8C, t, onNav, &nav_menu[c]);
            dot_cat[c] = lv_obj_create(b);
            lv_obj_set_size(dot_cat[c], 12, 12);
            lv_obj_align(dot_cat[c], LV_ALIGN_TOP_RIGHT, 0, 0);
            lv_obj_set_style_radius(dot_cat[c], LV_RADIUS_CIRCLE, LV_PART_MAIN);
            lv_obj_set_style_border_width(dot_cat[c], 0, LV_PART_MAIN);
            lv_obj_set_style_shadow_width(dot_cat[c], 0, LV_PART_MAIN);
            lv_obj_set_style_bg_color(dot_cat[c], lv_color_hex(0xFF1744), LV_PART_MAIN);
            lv_obj_clear_flag(dot_cat[c], LV_OBJ_FLAG_CLICKABLE);
            lv_obj_clear_flag(dot_cat[c], LV_OBJ_FLAG_SCROLLABLE);
        }
        {
            char t[48];
            snprintf(t, sizeof(t), "#FF6E88 %s#\nPARAR TUDO", LV_SYMBOL_STOP);
            makeBtn(view_menu, LV_PCT(32), 90, 0x5A1020, 0xFF1744, t, onClick, &stop_ctx);
        }

        // ---- Categorias: [< Voltar]  titulo  [PARAR] + grade 3x2 ----
        nav_back = { this, -1 };
        for (uint8_t c = 0; c < CATS; c++) {
            view_cat[c] = makeTransparentPage(macro_container);
            char left[32]; snprintf(left, sizeof(left), "%s Voltar", LV_SYMBOL_LEFT);
            lv_obj_t * bb = makeBtn(view_cat[c], 92, 28, 0x1F1F38, 0x2D3E66, left, onNav, &nav_back);
            lv_obj_set_pos(bb, 3, 2);
            lv_obj_t * title = lv_label_create(view_cat[c]);
            lv_obj_set_style_text_font(title, &lv_font_montserrat_14, LV_PART_MAIN);
            lv_obj_set_style_text_color(title, lv_color_hex(0x00F5D4), LV_PART_MAIN);
            lv_label_set_text(title, cats[c].name);
            lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 8);
            char right[32]; snprintf(right, sizeof(right), "#FF6E88 %s# PARAR", LV_SYMBOL_STOP);
            lv_obj_t * sb = makeBtn(view_cat[c], 92, 28, 0x5A1020, 0xFF1744, right, onClick, &stop_ctx);
            lv_obj_align(sb, LV_ALIGN_TOP_RIGHT, -3, 2);

            lv_obj_t * grid = makeGrid(view_cat[c], 34, 188);
            for (uint8_t k = 0; k < cats[c].n; k++) buildSlot(cats[c].slots[k], grid);
        }
        showView(-1);
    }

    // Chamado a cada 100 ms pelo UIManager (so para o jogo ativo). So mexe no LVGL quando algo muda.
    void updateStatusTab(const GameState &state) override {
        if (state.active_macro_slot != last_active) {
            last_active = state.active_macro_slot;
            for (uint8_t i = 0; i < SLOTS; i++) paintSlot(i, (int8_t)i == last_active);
            for (uint8_t c = 0; c < CATS; c++) {
                bool any = false;
                for (uint8_t k = 0; k < cats[c].n; k++) if ((int8_t)cats[c].slots[k] == last_active) any = true;
                if (dot_cat[c]) lv_obj_set_style_bg_color(dot_cat[c], lv_color_hex(any ? 0x00E676 : 0xFF1744), LV_PART_MAIN);
            }
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