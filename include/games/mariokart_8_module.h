#ifndef MARIOKART_8_MODULE_H
#define MARIOKART_8_MODULE_H

#include "../core/i_game_module.h"
#include <stdio.h>

class MarioKart8Module : public IGameModule {
private:
    lv_obj_t * container = nullptr;
    lv_obj_t * info_label = nullptr;
    const char* macros[6] = {
        "Auto-Acc", "Item Hold", "Drift Boost",
        "Restart Race", "200cc Helper", "Menu Select"
    };

public:
    const char* getGameName() const override { return "Mario Kart 8D"; }

    void buildStatusTab(lv_obj_t * parent) override {
        container = lv_obj_create(parent);
        lv_obj_set_size(container, 465, 238);
        lv_obj_set_style_bg_opa(container, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_border_width(container, 0, LV_PART_MAIN);
        lv_obj_clear_flag(container, LV_OBJ_FLAG_SCROLLABLE);

        info_label = lv_label_create(container);
        lv_obj_set_style_text_color(info_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        lv_obj_set_style_text_font(info_label, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_label_set_text(info_label, "Mario Kart 8 Deluxe\nTelemetry & Position HUD");
        lv_obj_align(info_label, LV_ALIGN_CENTER, 0, 0);
    }

    void updateStatusTab(const GameState &state) override {
        (void)state;
    }

    void formatMapCoordText(char * buf, size_t buf_size, const GameState &state) override {
        snprintf(buf, buf_size, "Pista MK8D | Lap: 1/3");
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

#endif // MARIOKART_8_MODULE_H