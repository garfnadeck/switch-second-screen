#ifndef GENERIC_MODULE_H
#define GENERIC_MODULE_H

#include "../core/i_game_module.h"
#include <stdio.h>

class GenericModule : public IGameModule {
private:
    lv_obj_t * container = nullptr;
    lv_obj_t * info_lbl = nullptr;

    const char* macros[6] = {
        "Turbo A", "Turbo B", "Combo 1",
        "Combo 2", "Home Wake", "Screenshot"
    };

public:
    const char* getGameName() const override { return "Generic Hub"; }

    void buildStatusTab(lv_obj_t * parent) override {
        container = lv_obj_create(parent);
        lv_obj_set_size(container, 298, 200);
        lv_obj_set_style_bg_color(container, lv_color_hex(0x1E1E36), LV_PART_MAIN);
        lv_obj_set_style_border_color(container, lv_color_hex(0x00F5D4), LV_PART_MAIN);

        info_lbl = lv_label_create(container);
        lv_obj_set_style_text_color(info_lbl, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        lv_obj_center(info_lbl);
    }

    void updateStatusTab(const GameState &state) override {
        lv_label_set_text_fmt(info_lbl,
            "UNIVERSAL USB / WIFI HUB\n\n"
            "Switch IP: %s\n"
            "Status: %s\n"
            "Use a aba Macros para Turbo.",
            state.switch_ip,
            state.connected ? "ONLINE" : "OFFLINE");
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

#endif // GENERIC_MODULE_H