#ifndef POKEMON_LEGENDS_ARCEUS_MODULE_H
#define POKEMON_LEGENDS_ARCEUS_MODULE_H

#include "../core/i_game_module.h"
#include <stdio.h>

class PokemonLegendsArceusModule : public IGameModule {
private:
    lv_obj_t * container = nullptr;
    lv_obj_t * info_label = nullptr;
    const char* macros[6] = {
        "Auto-Catch", "Fast Travel", "Farm Ore",
        "Shiny Reset", "Alpha Hunt", "Quick Save"
    };

public:
    const char* getGameName() const override { return "Pokemon Arceus"; }

    void buildStatusTab(lv_obj_t * parent) override {
        container = lv_obj_create(parent);
        lv_obj_set_size(container, 465, 238);
        lv_obj_set_style_bg_opa(container, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_border_width(container, 0, LV_PART_MAIN);
        lv_obj_clear_flag(container, LV_OBJ_FLAG_SCROLLABLE);

        info_label = lv_label_create(container);
        lv_obj_set_style_text_color(info_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        lv_obj_set_style_text_font(info_label, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_label_set_text(info_label, "Pokemon Legends: Arceus\nStatus e Pesquisa de Hisui [Aguardando RAM]");
        lv_obj_align(info_label, LV_ALIGN_CENTER, 0, 0);
    }

    void updateStatusTab(const GameState &state) override {
        (void)state;
    }

    void formatMapCoordText(char * buf, size_t buf_size, const GameState &state) override {
        snprintf(buf, buf_size, "Hisui | X: %.0f  Y: %.0f", state.map.x, state.map.y);
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

#endif // POKEMON_LEGENDS_ARCEUS_MODULE_H