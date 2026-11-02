#ifndef MACRO_MENU_H
#define MACRO_MENU_H

#include "../core/i_game_module.h"
#include <stdio.h>

// Mini menus de macros compartilhados entre os jogos (tela 480x320).
// Tela inicial = grade 3x2 com ate 5 categorias + PARAR TUDO; cada categoria = grade 3x2 de macros
// com cabecalho [< Voltar] titulo [PARAR]. Um ponto vermelho/verde mostra o estado.
class MacroMenu {
public:
    static constexpr uint8_t MAX_SLOTS = MACRO_COUNT;   // 25
    static constexpr uint8_t MAX_CATS  = 5;
    static constexpr uint8_t TOTAL     = MAX_SLOTS + UM_COUNT;   // + macros do usuario (slots 100..105)
    static uint8_t ix(uint8_t slot) { return slot >= USER_MACRO_BASE ? (uint8_t)(MAX_SLOTS + slot - USER_MACRO_BASE) : slot; }

    struct Cat  { const char * name; const char * icon; uint8_t n; uint8_t slots[6]; };
    struct Slot { const char * name; const char * icon; bool toggle; };

private:
    struct SlotCtx { MacroMenu * self; uint8_t slot; };
    struct NavCtx  { MacroMenu * self; int8_t view; };

    const Cat *  cats = nullptr;  uint8_t n_cats = 0;
    const Slot * slots = nullptr; uint8_t n_slots = 0;

    lv_obj_t * root = nullptr;
    lv_obj_t * view_menu = nullptr;
    lv_obj_t * view_cat[MAX_CATS] = {};
    lv_obj_t * dot_cat[MAX_CATS]  = {};
    lv_obj_t * btn[TOTAL] = {};
    lv_obj_t * dot[TOTAL] = {};
    lv_obj_t * ulbl[UM_COUNT] = {};
    SlotCtx    ctx[TOTAL] = {};
    const UserMacroStore * um = nullptr;
    int16_t    um_ver = -1;
    NavCtx     nav_menu[MAX_CATS] = {};
    NavCtx     nav_back = {};
    SlotCtx    stop_ctx = {};
    MacroHandler handler = nullptr;
    void * handler_ctx = nullptr;
    int8_t cur_view = -1, last_active = -2;

    static void onClick(lv_event_t * e) {
        SlotCtx * c = static_cast<SlotCtx *>(lv_event_get_user_data(e));
        if (c && c->self && c->self->handler) c->self->handler(c->self->handler_ctx, c->slot);
    }
    static void onNav(lv_event_t * e) {
        NavCtx * n = static_cast<NavCtx *>(lv_event_get_user_data(e));
        if (n && n->self) n->self->showView(n->view);
    }

    void paintSlot(uint8_t i, bool active) {
        if (!btn[i] || !dot[i]) return;
        lv_obj_set_style_bg_color(btn[i], lv_color_hex(active ? 0x133E35 : 0x0F3460), LV_PART_MAIN);
        lv_obj_set_style_border_color(btn[i], lv_color_hex(active ? 0x00E676 : 0x1F4E8C), LV_PART_MAIN);
        lv_obj_set_style_bg_color(dot[i], lv_color_hex(active ? 0x00E676 : 0xFF1744), LV_PART_MAIN);
    }

    static lv_obj_t * makeDot(lv_obj_t * parent) {
        lv_obj_t * d = lv_obj_create(parent);
        lv_obj_set_size(d, 12, 12);
        lv_obj_align(d, LV_ALIGN_TOP_RIGHT, 0, 0);
        lv_obj_set_style_radius(d, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        lv_obj_set_style_border_width(d, 0, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(d, 0, LV_PART_MAIN);
        lv_obj_set_style_bg_color(d, lv_color_hex(0xFF1744), LV_PART_MAIN);
        lv_obj_clear_flag(d, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(d, LV_OBJ_FLAG_SCROLLABLE);
        return d;
    }

    void userText(uint8_t k, char * out, size_t n) const {
        if (um && um->m[k].n) snprintf(out, n, "#00F5D4 %s#\n%s\n#8888AA %u passos#", LV_SYMBOL_EDIT, um->label(k), (unsigned)um->m[k].n);
        else                  snprintf(out, n, "#8888AA %s#\n(vazio)", LV_SYMBOL_EDIT);
    }

    void buildSlot(uint8_t slot, lv_obj_t * parent) {
        uint8_t i = ix(slot);
        bool user = slot >= USER_MACRO_BASE;
        btn[i] = lv_btn_create(parent);
        lv_obj_set_size(btn[i], LV_PCT(32), 84);
        lv_obj_set_style_radius(btn[i], 8, LV_PART_MAIN);
        lv_obj_set_style_border_width(btn[i], 2, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(btn[i], 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(btn[i], 4, LV_PART_MAIN);
        lv_obj_set_style_bg_color(btn[i], lv_color_hex(0x1F4E8C), LV_STATE_PRESSED);
        lv_obj_clear_flag(btn[i], LV_OBJ_FLAG_SCROLLABLE);
        ctx[i] = { this, slot };
        lv_obj_add_event_cb(btn[i], onClick, LV_EVENT_CLICKED, &ctx[i]);

        lv_obj_t * lbl = lv_label_create(btn[i]);
        lv_label_set_recolor(lbl, true);
        lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(lbl, LV_PCT(96));
        lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, LV_PART_MAIN);
        char text[96];
        if (user) { userText((uint8_t)(slot - USER_MACRO_BASE), text, sizeof(text)); ulbl[slot - USER_MACRO_BASE] = lbl; }
        else snprintf(text, sizeof(text), "#00F5D4 %s#\n%s%s", slots[slot].icon, slots[slot].name, slots[slot].toggle ? " #8888AA ~#" : "");
        lv_label_set_text(lbl, text);
        lv_obj_center(lbl);
        dot[i] = makeDot(btn[i]);
        paintSlot(i, false);
    }

    static lv_obj_t * makeBtn(lv_obj_t * parent, lv_coord_t w, lv_coord_t h, uint32_t bg, uint32_t border,
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

    static lv_obj_t * makeGrid(lv_obj_t * parent, lv_coord_t y, lv_coord_t h) {
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

public:
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

    void showView(int8_t v) {
        cur_view = v;
        if (view_menu) { if (v < 0) lv_obj_clear_flag(view_menu, LV_OBJ_FLAG_HIDDEN); else lv_obj_add_flag(view_menu, LV_OBJ_FLAG_HIDDEN); }
        for (uint8_t i = 0; i < n_cats; i++) {
            if (!view_cat[i]) continue;
            if ((int8_t)i == v) lv_obj_clear_flag(view_cat[i], LV_OBJ_FLAG_HIDDEN);
            else                lv_obj_add_flag(view_cat[i], LV_OBJ_FLAG_HIDDEN);
        }
    }

    // cats/slots precisam permanecer vivos (arrays do modulo)
    void build(lv_obj_t * parent, const Cat * c, uint8_t nc, const Slot * s, uint8_t ns,
               MacroHandler cb, void * hctx) {
        cats = c; n_cats = nc > MAX_CATS ? MAX_CATS : nc;
        slots = s; n_slots = ns > MAX_SLOTS ? MAX_SLOTS : ns;
        handler = cb; handler_ctx = hctx;
        stop_ctx = { this, MACRO_STOP };
        root = makeTransparentPage(parent);

        view_menu = makeGrid(root, 0, 224);
        for (uint8_t i = 0; i < n_cats; i++) {
            char t[80];
            snprintf(t, sizeof(t), "#00F5D4 %s#\n%s\n#8888AA %u macros#", cats[i].icon, cats[i].name, (unsigned)cats[i].n);
            nav_menu[i] = { this, (int8_t)i };
            lv_obj_t * b = makeBtn(view_menu, LV_PCT(32), 90, 0x0F3460, 0x1F4E8C, t, onNav, &nav_menu[i]);
            dot_cat[i] = makeDot(b);
        }
        {
            char t[48];
            snprintf(t, sizeof(t), "#FF6E88 %s#\nPARAR TUDO", LV_SYMBOL_STOP);
            makeBtn(view_menu, LV_PCT(32), 90, 0x5A1020, 0xFF1744, t, onClick, &stop_ctx);
        }

        nav_back = { this, -1 };
        for (uint8_t i = 0; i < n_cats; i++) {
            view_cat[i] = makeTransparentPage(root);
            char left[32]; snprintf(left, sizeof(left), "%s Voltar", LV_SYMBOL_LEFT);
            lv_obj_t * bb = makeBtn(view_cat[i], 92, 28, 0x1F1F38, 0x2D3E66, left, onNav, &nav_back);
            lv_obj_set_pos(bb, 3, 2);
            lv_obj_t * title = lv_label_create(view_cat[i]);
            lv_obj_set_style_text_font(title, &lv_font_montserrat_14, LV_PART_MAIN);
            lv_obj_set_style_text_color(title, lv_color_hex(0x00F5D4), LV_PART_MAIN);
            lv_label_set_text(title, cats[i].name);
            lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 8);
            char right[32]; snprintf(right, sizeof(right), "#FF6E88 %s# PARAR", LV_SYMBOL_STOP);
            lv_obj_t * sb = makeBtn(view_cat[i], 92, 28, 0x5A1020, 0xFF1744, right, onClick, &stop_ctx);
            lv_obj_align(sb, LV_ALIGN_TOP_RIGHT, -3, 2);
            lv_obj_t * grid = makeGrid(view_cat[i], 34, 188);
            for (uint8_t k = 0; k < cats[i].n; k++) buildSlot(cats[i].slots[k], grid);
        }
        showView(-1);
    }

    void update(int8_t active, const UserMacroStore * store = nullptr) {
        um = store;
        if (um && (int16_t)um->version != um_ver) {       // nomes dos macros do usuario mudaram
            um_ver = (int16_t)um->version;
            for (uint8_t k = 0; k < UM_COUNT; k++) if (ulbl[k]) { char t[96]; userText(k, t, sizeof(t)); lv_label_set_text(ulbl[k], t); }
        }
        if (active == last_active) return;
        last_active = active;
        for (uint8_t i = 0; i < n_slots; i++) paintSlot(i, (int8_t)i == active);
        for (uint8_t k = 0; k < UM_COUNT; k++) paintSlot((uint8_t)(MAX_SLOTS + k), (int8_t)(USER_MACRO_BASE + k) == active);
        for (uint8_t c = 0; c < n_cats; c++) {
            bool any = false;
            for (uint8_t k = 0; k < cats[c].n; k++) if ((int8_t)cats[c].slots[k] == active) any = true;
            if (dot_cat[c]) lv_obj_set_style_bg_color(dot_cat[c], lv_color_hex(any ? 0x00E676 : 0xFF1744), LV_PART_MAIN);
        }
    }

    const char * label(uint8_t s) const {
        if (s >= USER_MACRO_BASE) return (um && s - USER_MACRO_BASE < UM_COUNT) ? um->label((uint8_t)(s - USER_MACRO_BASE)) : "";
        return (slots && s < n_slots) ? slots[s].name : "";
    }

    void setVisible(bool v) {
        if (!root) return;
        if (v) lv_obj_clear_flag(root, LV_OBJ_FLAG_HIDDEN); else lv_obj_add_flag(root, LV_OBJ_FLAG_HIDDEN);
    }
};

#endif // MACRO_MENU_H