#ifndef POKEMON_SV_MODULE_H
#define POKEMON_SV_MODULE_H

#include "../core/i_game_module.h"
#include <stdio.h>

class PokemonSVModule : public IGameModule {
private:
    lv_obj_t * container = nullptr;
    lv_obj_t * hp_bars[6] = {nullptr};
    lv_obj_t * hp_labels[6] = {nullptr};

    const char* macros[6] = {
        "Heal Party", "Auto-Hatch", "Mash A",
        "Picnic Reset", "Raid Ready", "Quick Save"
    };

public:
    const char* getGameName() const override { return "Pokemon SV"; }

    void buildStatusTab(lv_obj_t * parent) override {
        container = lv_obj_create(parent);
        lv_obj_set_size(container, 465, 238);
        lv_obj_set_style_bg_opa(container, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_border_width(container, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(container, 0, LV_PART_MAIN);
        lv_obj_clear_flag(container, LV_OBJ_FLAG_SCROLLABLE);

        for (int i = 0; i < 6; i++) {
            int col = i / 3;
            int row = i % 3;

            lv_obj_t * card = lv_obj_create(container);
            lv_obj_set_size(card, 226, 72);
            lv_obj_set_pos(card, col * 236, row * 78);
            lv_obj_set_style_bg_color(card, lv_color_hex(0x1E1E36), LV_PART_MAIN);
            lv_obj_set_style_border_color(card, lv_color_hex(0x2E2E52), LV_PART_MAIN);
            lv_obj_set_style_pad_all(card, 5, LV_PART_MAIN);
            lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

            hp_labels[i] = lv_label_create(card);
            lv_obj_set_style_text_color(hp_labels[i], lv_color_hex(0xFFFFFF), LV_PART_MAIN);
            lv_obj_set_style_text_font(hp_labels[i], &lv_font_montserrat_12, LV_PART_MAIN);
            lv_obj_align(hp_labels[i], LV_ALIGN_TOP_LEFT, 2, 0);

            hp_bars[i] = lv_bar_create(card);
            lv_obj_set_size(hp_bars[i], 212, 8);
            lv_obj_align(hp_bars[i], LV_ALIGN_BOTTOM_MID, 0, -2);
            lv_bar_set_value(hp_bars[i], 100, LV_ANIM_OFF);
        }
    }

    void updateStatusTab(const GameState &state) override {
        for (int i = 0; i < 6; i++) {
            const PokemonSlot &p = state.party[i];
            uint8_t pct = (p.max_hp > 0) ? (uint8_t)((p.current_hp * 100) / p.max_hp) : 0;
            lv_bar_set_value(hp_bars[i], pct, LV_ANIM_ON);
            lv_label_set_text_fmt(hp_labels[i], "%s Lv.%d [%s]  HP:%d/%d",
                                  p.name, p.level, p.status, p.current_hp, p.max_hp);

            lv_color_t c = lv_color_hex(0x00E676);
            if (pct < 20) c = lv_color_hex(0xFF1744);
            else if (pct < 50) c = lv_color_hex(0xFFEA00);
            lv_obj_set_style_bg_color(hp_bars[i], c, LV_PART_INDICATOR);
        }
    }

    void formatMapCoordText(char * buf, size_t buf_size, const GameState &state) override {
        snprintf(buf, buf_size, "%s | X: %.0f  Y: %.0f",
                 state.map.zone_name, state.map.x, state.map.y);
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

#endif // POKEMON_SV_MODULE_H
