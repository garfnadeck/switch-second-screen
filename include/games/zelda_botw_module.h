#ifndef ZELDA_BOTW_MODULE_H
#define ZELDA_BOTW_MODULE_H

#include "../core/i_game_module.h"
#include <stdio.h>

class ZeldaBotwModule : public IGameModule {
private:
    lv_obj_t * container = nullptr;
    lv_obj_t * hdr_lbl = nullptr;
    lv_obj_t * hearts_bar = nullptr;
    lv_obj_t * hearts_lbl = nullptr;
    lv_obj_t * stamina_bar = nullptr;
    lv_obj_t * stamina_lbl = nullptr;
    lv_obj_t * rupees_lbl = nullptr;
    lv_obj_t * gear_lbl = nullptr;

    const char* macros[6] = {
        "Runa Amiibo (L)", "Amiibo Loop", "Whistle Sprint",
        "Spam A (Loot)", "Quick Save", "Mapa (-)"
    };

public:
    const char* getGameName() const override { return "Zelda BOTW"; }

    void buildStatusTab(lv_obj_t * parent) override {
        container = lv_obj_create(parent);
        lv_obj_set_size(container, 468, 214);
        lv_obj_set_style_bg_opa(container, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_border_width(container, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(container, 4, LV_PART_MAIN);
        lv_obj_clear_flag(container, LV_OBJ_FLAG_SCROLLABLE);

        hdr_lbl = lv_label_create(container);
        lv_obj_set_style_text_color(hdr_lbl, lv_color_hex(0x00F5D4), LV_PART_MAIN);
        lv_obj_set_style_text_font(hdr_lbl, &lv_font_montserrat_12, LV_PART_MAIN);
        lv_label_set_text(hdr_lbl, "Hyrule HUD - Breath of the Wild");
        lv_obj_align(hdr_lbl, LV_ALIGN_TOP_LEFT, 4, 2);

        rupees_lbl = lv_label_create(container);
        lv_obj_set_style_text_color(rupees_lbl, lv_color_hex(0x00E676), LV_PART_MAIN);
        lv_obj_set_style_text_font(rupees_lbl, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_label_set_text(rupees_lbl, "Rupees: 0");
        lv_obj_align(rupees_lbl, LV_ALIGN_TOP_RIGHT, -6, 2);

        // Barra de Coracoes
        hearts_lbl = lv_label_create(container);
        lv_obj_set_style_text_color(hearts_lbl, lv_color_hex(0xFF6E88), LV_PART_MAIN);
        lv_obj_set_style_text_font(hearts_lbl, &lv_font_montserrat_12, LV_PART_MAIN);
        lv_label_set_text(hearts_lbl, "Coracoes: 3 / 3");
        lv_obj_set_pos(hearts_lbl, 6, 30);

        hearts_bar = lv_bar_create(container);
        lv_obj_set_size(hearts_bar, 448, 16);
        lv_obj_set_pos(hearts_bar, 6, 48);
        lv_bar_set_range(hearts_bar, 0, 120);
        lv_bar_set_value(hearts_bar, 12, LV_ANIM_OFF);
        lv_obj_set_style_bg_color(hearts_bar, lv_color_hex(0x2A1420), LV_PART_MAIN);
        lv_obj_set_style_bg_color(hearts_bar, lv_color_hex(0xFF1744), LV_PART_INDICATOR);
        lv_obj_set_style_radius(hearts_bar, 6, LV_PART_MAIN);
        lv_obj_set_style_radius(hearts_bar, 6, LV_PART_INDICATOR);

        // Barra de Stamina
        stamina_lbl = lv_label_create(container);
        lv_obj_set_style_text_color(stamina_lbl, lv_color_hex(0x00E676), LV_PART_MAIN);
        lv_obj_set_style_text_font(stamina_lbl, &lv_font_montserrat_12, LV_PART_MAIN);
        lv_label_set_text(stamina_lbl, "Stamina: 100%");
        lv_obj_set_pos(stamina_lbl, 6, 76);

        stamina_bar = lv_bar_create(container);
        lv_obj_set_size(stamina_bar, 448, 16);
        lv_obj_set_pos(stamina_bar, 6, 94);
        lv_bar_set_range(stamina_bar, 0, 100);
        lv_bar_set_value(stamina_bar, 100, LV_ANIM_OFF);
        lv_obj_set_style_bg_color(stamina_bar, lv_color_hex(0x10241A), LV_PART_MAIN);
        lv_obj_set_style_bg_color(stamina_bar, lv_color_hex(0x00E676), LV_PART_INDICATOR);
        lv_obj_set_style_radius(stamina_bar, 6, LV_PART_MAIN);
        lv_obj_set_style_radius(stamina_bar, 6, LV_PART_INDICATOR);

        gear_lbl = lv_label_create(container);
        lv_obj_set_style_text_color(gear_lbl, lv_color_hex(0xB8C4E0), LV_PART_MAIN);
        lv_obj_set_style_text_font(gear_lbl, &lv_font_montserrat_12, LV_PART_MAIN);
        lv_label_set_text(gear_lbl, "Arma: -  |  Arco: -");
        lv_obj_set_pos(gear_lbl, 6, 126);
    }

    void updateStatusTab(const GameState &state) override {
        if (!container) return;
        uint8_t h_cur = state.zelda.hearts_current ? state.zelda.hearts_current : 12;
        uint8_t h_max = state.zelda.hearts_max ? state.zelda.hearts_max : 12;
        uint8_t stam  = state.zelda.stamina_pct ? state.zelda.stamina_pct : 100;
        lv_label_set_text_fmt(hearts_lbl, "Coracoes: %u / %u", (unsigned)(h_cur / 4), (unsigned)(h_max / 4));
        lv_bar_set_range(hearts_bar, 0, h_max);
        lv_bar_set_value(hearts_bar, h_cur, LV_ANIM_OFF);

        lv_label_set_text_fmt(stamina_lbl, "Stamina: %u%%", (unsigned)stam);
        lv_bar_set_value(stamina_bar, stam, LV_ANIM_OFF);

        lv_label_set_text_fmt(rupees_lbl, "Rupees: %lu", (unsigned long)state.zelda.rupees);
        lv_label_set_text_fmt(gear_lbl, "Arma: %s (%u%%)  |  Arco: %s (%u%%)",
                              state.zelda.equipped_weapon[0] ? state.zelda.equipped_weapon : "-",
                              (unsigned)state.zelda.weapon_durability_pct,
                              state.zelda.equipped_bow[0] ? state.zelda.equipped_bow : "-",
                              (unsigned)state.zelda.bow_durability_pct);
    }

    void formatMapCoordText(char * buf, size_t buf_size, const GameState &state) override {
        snprintf(buf, buf_size, "Hyrule (BOTW) | X: %.0f  Y: %.0f", state.map.x, state.map.y);
    }

    const char* getMacroLabel(uint8_t slot) const override {
        return (slot < 6) ? macros[slot] : "";
    }

    void setVisible(bool visible) override {
        if (!container) return;
        if (visible) lv_obj_clear_flag(container, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(container, LV_OBJ_FLAG_HIDDEN);
    }
};

#endif // ZELDA_BOTW_MODULE_H
