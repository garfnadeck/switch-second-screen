#ifndef ZELDA_TOTK_MODULE_H
#define ZELDA_TOTK_MODULE_H

#include "../core/i_game_module.h"
#include <stdio.h>

class ZeldaTotkModule : public IGameModule {
private:
    lv_obj_t * container = nullptr;
    lv_obj_t * hearts_lbl = nullptr;
    lv_obj_t * stamina_bar = nullptr;
    lv_obj_t * weapon_lbl = nullptr;
    lv_obj_t * weapon_bar = nullptr;
    lv_obj_t * bow_lbl = nullptr;
    lv_obj_t * bow_bar = nullptr;

    const char* macros[6] = {
        "Full Hearts", "Repair Gear", "Hoverbike",
        "Dup Item", "Skip BloodM", "Quick Save"
    };

public:
    const char* getGameName() const override { return "Zelda TOTK"; }

    void buildStatusTab(lv_obj_t * parent) override {
        container = lv_obj_create(parent);
        lv_obj_set_size(container, 465, 238);
        lv_obj_set_style_bg_color(container, lv_color_hex(0x1A2421), LV_PART_MAIN);
        lv_obj_set_style_border_color(container, lv_color_hex(0x00E676), LV_PART_MAIN);
        lv_obj_set_style_pad_all(container, 10, LV_PART_MAIN);
        lv_obj_clear_flag(container, LV_OBJ_FLAG_SCROLLABLE);

        hearts_lbl = lv_label_create(container);
        lv_obj_set_style_text_color(hearts_lbl, lv_color_hex(0xFF5252), LV_PART_MAIN);
        lv_obj_align(hearts_lbl, LV_ALIGN_TOP_LEFT, 2, 2);

        lv_obj_t * stam_title = lv_label_create(container);
        lv_label_set_text(stam_title, "Stamina Wheel:");
        lv_obj_set_style_text_color(stam_title, lv_color_hex(0x00E676), LV_PART_MAIN);
        lv_obj_align(stam_title, LV_ALIGN_TOP_LEFT, 2, 32);

        stamina_bar = lv_bar_create(container);
        lv_obj_set_size(stamina_bar, 440, 12);
        lv_obj_align(stamina_bar, LV_ALIGN_TOP_LEFT, 2, 52);
        lv_obj_set_style_bg_color(stamina_bar, lv_color_hex(0x00E676), LV_PART_INDICATOR);

        weapon_lbl = lv_label_create(container);
        lv_obj_set_style_text_color(weapon_lbl, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        lv_obj_align(weapon_lbl, LV_ALIGN_TOP_LEFT, 2, 78);

        weapon_bar = lv_bar_create(container);
        lv_obj_set_size(weapon_bar, 440, 10);
        lv_obj_align(weapon_bar, LV_ALIGN_TOP_LEFT, 2, 98);
        lv_obj_set_style_bg_color(weapon_bar, lv_color_hex(0x00B0FF), LV_PART_INDICATOR);

        bow_lbl = lv_label_create(container);
        lv_obj_set_style_text_color(bow_lbl, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        lv_obj_align(bow_lbl, LV_ALIGN_TOP_LEFT, 2, 124);

        bow_bar = lv_bar_create(container);
        lv_obj_set_size(bow_bar, 440, 10);
        lv_obj_align(bow_bar, LV_ALIGN_TOP_LEFT, 2, 144);
        lv_obj_set_style_bg_color(bow_bar, lv_color_hex(0xFFEA00), LV_PART_INDICATOR);
    }

    void updateStatusTab(const GameState &state) override {
        lv_label_set_text_fmt(hearts_lbl, "Coracoes: %d / %d   |   Rupees: %d",
                              state.zelda.hearts_current, state.zelda.hearts_max, state.zelda.rupees);
        lv_bar_set_value(stamina_bar, state.zelda.stamina_pct, LV_ANIM_ON);

        lv_label_set_text_fmt(weapon_lbl, "Arma: %s (%d%%)",
                              state.zelda.equipped_weapon, state.zelda.weapon_durability_pct);
        lv_bar_set_value(weapon_bar, state.zelda.weapon_durability_pct, LV_ANIM_ON);

        lv_label_set_text_fmt(bow_lbl, "Arco: %s (%d%%)",
                              state.zelda.equipped_bow, state.zelda.bow_durability_pct);
        lv_bar_set_value(bow_bar, state.zelda.bow_durability_pct, LV_ANIM_ON);
    }

    void formatMapCoordText(char * buf, size_t buf_size, const GameState &state) override {
        snprintf(buf, buf_size, "%s | X:%.0f Y:%.0f Z:%.0f",
                 state.map.zone_name, state.map.x, state.map.y, state.map.z);
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

#endif // ZELDA_TOTK_MODULE_H
