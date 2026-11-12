#ifndef POKEMON_LEGENDS_ZA_MODULE_H
#define POKEMON_LEGENDS_ZA_MODULE_H

#include "../core/i_game_module.h"
#include "../core/asset_manager.h"
#include "../ui/macro_menu.h"
#include <stdio.h>
#include <string.h>

// Layout Z-A para a tela de 3.5" (480x320), dentro do tabview do UIManager.
//
// Aba "Status": cabecalho (zona + coordenadas + estado do SD) + grade 2x3 de cards dos 6 Pokemon
//               com sprite 40x40 vindo do MicroSD (PSRAM), nivel, barra de HP, valor e condicao.
// Aba "Macros": mini menus. Tela inicial = 6 botoes (5 categorias + PARAR TUDO); dentro de cada
//               categoria, grade 3x2 de macros (90 px) + cabecalho [< Voltar] titulo [PARAR].
class PokemonLegendsZAModule : public IGameModule {
public:
    typedef void (*PokeEditHandler)(void * ctx, uint8_t slot, PokeEditAction action, uint16_t item_id);

private:
    static constexpr uint8_t SLOTS     = 17;   // macros do Z-A (o FireRed usa ate MACRO_COUNT)
    static constexpr uint8_t CATS      = 5;
    static constexpr uint8_t PARTY     = 6;

    PokeEditHandler edit_cb            = nullptr;
    void          * edit_ctx           = nullptr;

    // ---- Aba Status (Grade 2x3 de Cards + Sprites em PSRAM) ----
    lv_obj_t * container             = nullptr;
    lv_obj_t * lbl_zone              = nullptr;
    lv_obj_t * lbl_sd                = nullptr;
    lv_obj_t * card[PARTY]           = {};
    lv_obj_t * accent[PARTY]         = {};
    lv_obj_t * icon_bg[PARTY]        = {};
    lv_obj_t * img_poke[PARTY]       = {};
    lv_obj_t * lbl_fallback[PARTY]   = {};
    lv_obj_t * lbl_name[PARTY]       = {};
    lv_obj_t * lbl_lv[PARTY]         = {};
    lv_obj_t * bar_hp[PARTY]         = {};
    lv_obj_t * lbl_hp[PARTY]         = {};
    lv_obj_t * lbl_status[PARTY]     = {};

    // ---- Mini Menu do Pokemon (abre ao clicar num card) ----
    lv_obj_t * modal_panel           = nullptr;
    lv_obj_t * modal_img             = nullptr;
    lv_obj_t * modal_lbl_title       = nullptr;
    lv_obj_t * modal_lbl_meta        = nullptr;
    lv_obj_t * modal_lbl_feedback    = nullptr;
    int8_t     modal_slot            = -1;

    CachedAsset sprite_cache[PARTY]  = {};
    uint16_t    loaded_species[PARTY] = {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF};
    PokemonSlot last_party[PARTY]    = {};
    bool        party_drawn          = false;
    bool        last_sd_ready        = false;
    float       last_mx = -99999.0f, last_my = -99999.0f, last_mz = -99999.0f;
    char        last_zone[28]        = {};

    // ---- Aba Macros (componente compartilhado) ----
    MacroMenu menu;
    // Sem acentos: a fonte padrao do LVGL nao os tem
    const MacroMenu::Cat cats[5] = {
        { "Batalha/Cura", LV_SYMBOL_SHUFFLE, 6, { 1, 7, 6, 8, 15, 16 } },  // Combo, Esquiva, Auto-Catch, Spam Y, Curar HP, Curar Status
        { "Movimento", LV_SYMBOL_RIGHT,    3, { 10, 11, 14 } },        // Andar reto, Correr, Reset Cam
        { "Farm",      LV_SYMBOL_LOOP,     3, { 3, 2, 9 } },           // AFK Circulo, Turbo A, Spam B
        { "Sistema",   LV_SYMBOL_SETTINGS, 5, { 12, 13, 4, 5, 0 } },   // Menu, Home, Mapa, Cancelar, Parear
        { "Meus macros", LV_SYMBOL_EDIT,   6, { 100, 101, 102, 103, 104, 105 } }   // criados na pagina (aba Criar)
    };
    const MacroMenu::Slot slotdefs[SLOTS] = {
        {"Parear L+R", LV_SYMBOL_USB, false}, {"Combo Esq+Y", LV_SYMBOL_SHUFFLE, false},
        {"Turbo A", LV_SYMBOL_CHARGE, true}, {"AFK Circulo", LV_SYMBOL_LOOP, true},
        {"Mapa (+)", LV_SYMBOL_GPS, false}, {"Cancelar 2xB", LV_SYMBOL_LEFT, false},
        {"Auto-Catch", LV_SYMBOL_PLAY, false}, {"Esquiva", LV_SYMBOL_RIGHT, false},
        {"Spam Y", LV_SYMBOL_REFRESH, true}, {"Spam B", LV_SYMBOL_REFRESH, true},
        {"Andar reto", LV_SYMBOL_UP, true}, {"Correr", LV_SYMBOL_NEXT, true},
        {"Menu (X)", LV_SYMBOL_LIST, false}, {"Home", LV_SYMBOL_HOME, false},
        {"Reset Cam", LV_SYMBOL_EYE_OPEN, false}, {"Curar HP", LV_SYMBOL_PLUS, false},
        {"Curar Status", LV_SYMBOL_OK, false}
    };

    // ---- Status: helpers ----
    static uint32_t hpColor(uint16_t cur, uint16_t max) {
        if (max == 0) return 0x333856;
        if (cur == 0) return 0xFF1744;
        uint32_t pct = (uint32_t)cur * 100 / max;
        return pct > 50 ? 0x00E676 : (pct > 20 ? 0xFFC400 : 0xFF1744);
    }

    static uint32_t statusColor(const char * st) {
        if (!st || !st[0]) return 0x8888AA;
        if (strcmp(st, "OK") == 0)  return 0x00E676;
        if (strcmp(st, "FNT") == 0) return 0xFF1744;
        if (strcmp(st, "PSN") == 0) return 0xCE93D8;
        if (strcmp(st, "BRN") == 0) return 0xFF7043;
        if (strcmp(st, "FRZ") == 0) return 0x4FC3F7;
        return 0xFFC400; // PAR, SLP
    }

    void updateSprite(uint8_t i, uint16_t species, bool has_pokemon) {
        uint16_t target = has_pokemon ? species : 0;
        if (loaded_species[i] == target && sprite_cache[i].loaded) return;
        loaded_species[i] = target;

        char path[48];
        bool ok = false;
        if (target > 0) {
            snprintf(path, sizeof(path), "/assets/za/sprites/%u.bin", (unsigned)target);
            ok = AssetManager::preloadToPSRAM(path, sprite_cache[i], 40, 40, LV_IMG_CF_TRUE_COLOR_ALPHA);
        }
        if (!ok) {
            ok = AssetManager::preloadToPSRAM("/assets/za/sprites/0.bin", sprite_cache[i], 40, 40, LV_IMG_CF_TRUE_COLOR_ALPHA);
        }

        if (ok && img_poke[i]) {
            lv_img_set_src(img_poke[i], &sprite_cache[i].descriptor);
            lv_obj_clear_flag(img_poke[i], LV_OBJ_FLAG_HIDDEN);
            if (lbl_fallback[i]) lv_obj_add_flag(lbl_fallback[i], LV_OBJ_FLAG_HIDDEN);
        } else {
            if (img_poke[i]) lv_obj_add_flag(img_poke[i], LV_OBJ_FLAG_HIDDEN);
            if (lbl_fallback[i]) {
                lv_obj_clear_flag(lbl_fallback[i], LV_OBJ_FLAG_HIDDEN);
                if (has_pokemon && species > 0) {
                    char sp_txt[12];
                    snprintf(sp_txt, sizeof(sp_txt), "#%u", (unsigned)species);
                    lv_label_set_text(lbl_fallback[i], sp_txt);
                } else {
                    lv_label_set_text(lbl_fallback[i], "-");
                }
            }
        }
    }

    static const char * itemName(uint16_t item_id) {
        switch (item_id) {
            case 0:   return "Nenhum";
            case 158: return "Sitrus Berry";
            case 231: return "Lucky Egg";
            case 234: return "Leftovers";
            case 270: return "Life Orb";
            default:  return "Equipado";
        }
    }

    void paintModal() {
        if (!modal_panel || modal_slot < 0 || modal_slot >= PARTY) return;
        const PokemonSlot &p = last_party[modal_slot];
        if (p.max_hp == 0) {
            lv_label_set_text_fmt(modal_lbl_title, "Slot %d: Vazio", (int)modal_slot + 1);
            lv_label_set_text(modal_lbl_meta, "Nenhum Pokemon neste slot");
            if (modal_img) lv_obj_add_flag(modal_img, LV_OBJ_FLAG_HIDDEN);
            return;
        }
        if (modal_img) {
            if (sprite_cache[modal_slot].loaded) {
                lv_img_set_src(modal_img, &sprite_cache[modal_slot].descriptor);
                lv_obj_clear_flag(modal_img, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_add_flag(modal_img, LV_OBJ_FLAG_HIDDEN);
            }
        }
        char title[96];
        snprintf(title, sizeof(title), "#00F5D4 #%d# %s  #AAAACC Lv%u#%s%s",
                 (int)modal_slot + 1, p.name, (unsigned)p.level,
                 p.is_shiny ? " #FFD700 *SHINY#" : "",
                 p.is_max_iv ? " #00E676 6IV#" : "");
        lv_label_set_text(modal_lbl_title, title);

        char meta[128];
        snprintf(meta, sizeof(meta), "HP: %u/%u (%s) | PP: %u/%u/%u/%u | Item: %s",
                 (unsigned)p.current_hp, (unsigned)p.max_hp,
                 p.status[0] ? p.status : "OK",
                 (unsigned)p.pp[0], (unsigned)p.pp[1], (unsigned)p.pp[2], (unsigned)p.pp[3],
                 itemName(p.held_item));
        lv_label_set_text(modal_lbl_meta, meta);
    }

    void openModal(uint8_t slot) {
        if (slot >= PARTY || !modal_panel) return;
        modal_slot = (int8_t)slot;
        if (modal_lbl_feedback) lv_label_set_text(modal_lbl_feedback, "Toque numa acao para aplicar na RAM");
        paintModal();
        lv_obj_clear_flag(modal_panel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(modal_panel);
    }

    void closeModal() {
        modal_slot = -1;
        if (modal_panel) lv_obj_add_flag(modal_panel, LV_OBJ_FLAG_HIDDEN);
    }

    void triggerPokeEdit(PokeEditAction action, uint16_t item_id, const char * msg) {
        if (modal_slot < 0 || modal_slot >= PARTY) return;
        if (edit_cb) edit_cb(edit_ctx, (uint8_t)modal_slot, action, item_id);
        if (modal_lbl_feedback && msg) lv_label_set_text(modal_lbl_feedback, msg);
    }

    void paintParty(uint8_t i, const PokemonSlot &p) {
        bool has = (p.max_hp > 0);
        uint32_t col = hpColor(p.current_hp, p.max_hp);
        updateSprite(i, p.species, has);

        lv_obj_set_style_bg_color(accent[i], lv_color_hex(col), LV_PART_MAIN);
        lv_obj_set_style_border_color(card[i], lv_color_hex(has ? (p.is_shiny ? 0xFFD700 : 0x2A3558) : 0x1E2238), LV_PART_MAIN);
        lv_obj_set_style_border_color(icon_bg[i], lv_color_hex(has ? col : 0x262A40), LV_PART_MAIN);

        if (!has) {
            lv_label_set_text(lbl_name[i], "---");
            lv_obj_set_style_text_color(lbl_name[i], lv_color_hex(0x666A88), LV_PART_MAIN);
            lv_label_set_text(lbl_lv[i], "");
            lv_label_set_text(lbl_hp[i], "aguardando RAM...");
            lv_obj_set_style_text_color(lbl_hp[i], lv_color_hex(0x666A88), LV_PART_MAIN);
            lv_label_set_text(lbl_status[i], "");
            lv_bar_set_range(bar_hp[i], 0, 1);
            lv_bar_set_value(bar_hp[i], 0, LV_ANIM_OFF);
        } else {
            lv_label_set_text(lbl_name[i], p.name);
            lv_obj_set_style_text_color(lbl_name[i], lv_color_hex(p.is_shiny ? 0xFFD700 : 0xFFFFFF), LV_PART_MAIN);

            char lv_txt[20];
            snprintf(lv_txt, sizeof(lv_txt), "%sLv%u", p.is_shiny ? "*" : "", (unsigned)p.level);
            lv_label_set_text(lbl_lv[i], lv_txt);

            uint32_t pct = (uint32_t)p.current_hp * 100 / p.max_hp;
            char hp_txt[36];
            snprintf(hp_txt, sizeof(hp_txt), "%u/%u (%lu%%)%s",
                     (unsigned)p.current_hp, (unsigned)p.max_hp, (unsigned long)pct,
                     p.held_item ? " [I]" : "");
            lv_label_set_text(lbl_hp[i], hp_txt);
            lv_obj_set_style_text_color(lbl_hp[i], lv_color_hex(0xD6DCFF), LV_PART_MAIN);

            lv_label_set_text(lbl_status[i], p.status[0] ? p.status : "OK");
            lv_obj_set_style_text_color(lbl_status[i], lv_color_hex(statusColor(p.status[0] ? p.status : "OK")), LV_PART_MAIN);

            lv_bar_set_range(bar_hp[i], 0, p.max_hp);
            lv_bar_set_value(bar_hp[i], p.current_hp, LV_ANIM_OFF);
        }
        lv_obj_set_style_bg_color(bar_hp[i], lv_color_hex(col), LV_PART_INDICATOR);
        if (modal_slot == (int8_t)i) paintModal();
    }

    static bool sameSlot(const PokemonSlot &a, const PokemonSlot &b) {
        return a.species == b.species && a.level == b.level &&
               a.current_hp == b.current_hp && a.max_hp == b.max_hp &&
               a.held_item == b.held_item && a.is_shiny == b.is_shiny && a.is_max_iv == b.is_max_iv &&
               memcmp(a.pp, b.pp, 4) == 0 &&
               strncmp(a.name, b.name, sizeof(a.name)) == 0 &&
               strncmp(a.status, b.status, sizeof(a.status)) == 0;
    }

    static lv_obj_t * makeTransparentPage(lv_obj_t * p) { return MacroMenu::makeTransparentPage(p); }

public:
    const char* getGameName() const override { return "Pokemon ZA"; }

    void setPokeEditHandler(PokeEditHandler cb, void * ctx) {
        edit_cb = cb;
        edit_ctx = ctx;
    }

    // Forca recarregamento dos sprites do MicroSD para a PSRAM (ex.: apos upload Wi-Fi)
    void reloadAssets() {
        for (uint8_t i = 0; i < PARTY; i++) {
            loaded_species[i] = 0xFFFF;
            AssetManager::freeAsset(sprite_cache[i]);
            paintParty(i, last_party[i]);
        }
    }

    // ---------------- Aba Status ----------------
    void buildStatusTab(lv_obj_t * parent) override {
        container = makeTransparentPage(parent);

        lbl_zone = lv_label_create(container);
        lv_label_set_recolor(lbl_zone, true);
        lv_obj_set_style_text_font(lbl_zone, &lv_font_montserrat_12, LV_PART_MAIN);
        lv_obj_set_style_text_color(lbl_zone, lv_color_hex(0x00F5D4), LV_PART_MAIN);
        lv_obj_align(lbl_zone, LV_ALIGN_TOP_LEFT, 2, -2);
        lv_label_set_text(lbl_zone, LV_SYMBOL_GPS " Zona: -");

        lbl_sd = lv_label_create(container);
        lv_obj_set_style_text_font(lbl_sd, &lv_font_montserrat_12, LV_PART_MAIN);
        lv_obj_set_style_text_color(lbl_sd, lv_color_hex(AssetManager::isSDReady() ? 0x00E676 : 0x8888AA), LV_PART_MAIN);
        lv_obj_align(lbl_sd, LV_ALIGN_TOP_RIGHT, -2, -2);
        lv_label_set_text(lbl_sd, AssetManager::isSDReady() ? LV_SYMBOL_SD_CARD " SD 16GB" : LV_SYMBOL_SD_CARD " Flash");
        last_sd_ready = AssetManager::isSDReady();

        // Grade 2x3 de cards dos 6 Pokemon (cada card: 231 x 60 px, clicavel para abrir Mini Menu)
        for (uint8_t i = 0; i < PARTY; i++) {
            lv_coord_t cx = (i % 2) * 236;
            lv_coord_t cy = 16 + (i / 2) * 64;

            card[i] = lv_obj_create(container);
            lv_obj_set_size(card[i], 231, 60);
            lv_obj_set_pos(card[i], cx, cy);
            lv_obj_set_style_bg_color(card[i], lv_color_hex(0x181C32), LV_PART_MAIN);
            lv_obj_set_style_bg_color(card[i], lv_color_hex(0x242B4A), LV_STATE_PRESSED);
            lv_obj_set_style_radius(card[i], 8, LV_PART_MAIN);
            lv_obj_set_style_border_width(card[i], 1, LV_PART_MAIN);
            lv_obj_set_style_border_color(card[i], lv_color_hex(0x2A3558), LV_PART_MAIN);
            lv_obj_set_style_pad_all(card[i], 0, LV_PART_MAIN);
            lv_obj_clear_flag(card[i], LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_add_flag(card[i], LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_user_data(card[i], (void*)(uintptr_t)i);
            lv_obj_add_event_cb(card[i], [](lv_event_t * e) {
                auto * mod = static_cast<PokemonLegendsZAModule*>(lv_event_get_user_data(e));
                uint8_t idx = (uint8_t)(uintptr_t)lv_obj_get_user_data(lv_event_get_current_target(e));
                mod->openModal(idx);
            }, LV_EVENT_CLICKED, this);

            accent[i] = lv_obj_create(card[i]);
            lv_obj_set_size(accent[i], 4, 46);
            lv_obj_set_pos(accent[i], 0, 7);
            lv_obj_set_style_bg_color(accent[i], lv_color_hex(0x333856), LV_PART_MAIN);
            lv_obj_set_style_border_width(accent[i], 0, LV_PART_MAIN);
            lv_obj_set_style_radius(accent[i], 2, LV_PART_MAIN);
            lv_obj_add_flag(accent[i], LV_OBJ_FLAG_EVENT_BUBBLE);

            icon_bg[i] = lv_obj_create(card[i]);
            lv_obj_set_size(icon_bg[i], 44, 44);
            lv_obj_set_pos(icon_bg[i], 7, 8);
            lv_obj_set_style_bg_color(icon_bg[i], lv_color_hex(0x101324), LV_PART_MAIN);
            lv_obj_set_style_radius(icon_bg[i], 22, LV_PART_MAIN);
            lv_obj_set_style_border_width(icon_bg[i], 1, LV_PART_MAIN);
            lv_obj_set_style_border_color(icon_bg[i], lv_color_hex(0x262A40), LV_PART_MAIN);
            lv_obj_set_style_pad_all(icon_bg[i], 0, LV_PART_MAIN);
            lv_obj_clear_flag(icon_bg[i], LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_add_flag(icon_bg[i], LV_OBJ_FLAG_EVENT_BUBBLE);

            lbl_fallback[i] = lv_label_create(icon_bg[i]);
            lv_obj_set_style_text_font(lbl_fallback[i], &lv_font_montserrat_12, LV_PART_MAIN);
            lv_obj_set_style_text_color(lbl_fallback[i], lv_color_hex(0x00F5D4), LV_PART_MAIN);
            lv_obj_center(lbl_fallback[i]);
            lv_label_set_text(lbl_fallback[i], "-");
            lv_obj_add_flag(lbl_fallback[i], LV_OBJ_FLAG_EVENT_BUBBLE);

            img_poke[i] = lv_img_create(card[i]);
            lv_obj_set_pos(img_poke[i], 9, 10);
            lv_obj_add_flag(img_poke[i], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(img_poke[i], LV_OBJ_FLAG_EVENT_BUBBLE);

            lbl_name[i] = lv_label_create(card[i]);
            lv_obj_set_style_text_font(lbl_name[i], &lv_font_montserrat_14, LV_PART_MAIN);
            lv_obj_set_style_text_color(lbl_name[i], lv_color_hex(0xFFFFFF), LV_PART_MAIN);
            lv_obj_set_width(lbl_name[i], 122);
            lv_label_set_long_mode(lbl_name[i], LV_LABEL_LONG_CLIP);
            lv_obj_set_pos(lbl_name[i], 56, 4);
            lv_obj_add_flag(lbl_name[i], LV_OBJ_FLAG_EVENT_BUBBLE);

            lbl_lv[i] = lv_label_create(card[i]);
            lv_obj_set_style_text_font(lbl_lv[i], &lv_font_montserrat_12, LV_PART_MAIN);
            lv_obj_set_style_text_color(lbl_lv[i], lv_color_hex(0x00F5D4), LV_PART_MAIN);
            lv_obj_set_pos(lbl_lv[i], 178, 5);
            lv_obj_add_flag(lbl_lv[i], LV_OBJ_FLAG_EVENT_BUBBLE);

            bar_hp[i] = lv_bar_create(card[i]);
            lv_obj_set_size(bar_hp[i], 166, 9);
            lv_obj_set_pos(bar_hp[i], 56, 25);
            lv_obj_set_style_bg_color(bar_hp[i], lv_color_hex(0x0D1020), LV_PART_MAIN);
            lv_obj_set_style_radius(bar_hp[i], 4, LV_PART_MAIN);
            lv_obj_set_style_radius(bar_hp[i], 4, LV_PART_INDICATOR);
            lv_bar_set_range(bar_hp[i], 0, 1);
            lv_bar_set_value(bar_hp[i], 0, LV_ANIM_OFF);
            lv_obj_add_flag(bar_hp[i], LV_OBJ_FLAG_EVENT_BUBBLE);

            lbl_hp[i] = lv_label_create(card[i]);
            lv_obj_set_style_text_font(lbl_hp[i], &lv_font_montserrat_12, LV_PART_MAIN);
            lv_obj_set_style_text_color(lbl_hp[i], lv_color_hex(0xD6DCFF), LV_PART_MAIN);
            lv_obj_set_width(lbl_hp[i], 134);
            lv_label_set_long_mode(lbl_hp[i], LV_LABEL_LONG_CLIP);
            lv_obj_set_pos(lbl_hp[i], 56, 39);
            lv_obj_add_flag(lbl_hp[i], LV_OBJ_FLAG_EVENT_BUBBLE);

            lbl_status[i] = lv_label_create(card[i]);
            lv_obj_set_style_text_font(lbl_status[i], &lv_font_montserrat_12, LV_PART_MAIN);
            lv_obj_set_style_text_color(lbl_status[i], lv_color_hex(0x00E676), LV_PART_MAIN);
            lv_obj_set_pos(lbl_status[i], 192, 39);
            lv_obj_add_flag(lbl_status[i], LV_OBJ_FLAG_EVENT_BUBBLE);

            paintParty(i, last_party[i]);
        }

        // ---- Mini Menu Modal (abre ao clicar num Pokemon) ----
        modal_panel = lv_obj_create(container);
        lv_obj_set_size(modal_panel, 468, 206);
        lv_obj_set_pos(modal_panel, 0, 0);
        lv_obj_set_style_bg_color(modal_panel, lv_color_hex(0x101426), LV_PART_MAIN);
        lv_obj_set_style_border_width(modal_panel, 2, LV_PART_MAIN);
        lv_obj_set_style_border_color(modal_panel, lv_color_hex(0x00F5D4), LV_PART_MAIN);
        lv_obj_set_style_radius(modal_panel, 8, LV_PART_MAIN);
        lv_obj_set_style_pad_all(modal_panel, 4, LV_PART_MAIN);
        lv_obj_clear_flag(modal_panel, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(modal_panel, LV_OBJ_FLAG_HIDDEN);

        modal_img = lv_img_create(modal_panel);
        lv_obj_set_pos(modal_img, 4, 2);

        modal_lbl_title = lv_label_create(modal_panel);
        lv_label_set_recolor(modal_lbl_title, true);
        lv_obj_set_style_text_font(modal_lbl_title, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_set_style_text_color(modal_lbl_title, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        lv_obj_set_pos(modal_lbl_title, 48, 2);
        lv_label_set_text(modal_lbl_title, "Pokemon");

        modal_lbl_meta = lv_label_create(modal_panel);
        lv_obj_set_style_text_font(modal_lbl_meta, &lv_font_montserrat_12, LV_PART_MAIN);
        lv_obj_set_style_text_color(modal_lbl_meta, lv_color_hex(0xD6DCFF), LV_PART_MAIN);
        lv_obj_set_pos(modal_lbl_meta, 48, 22);
        lv_label_set_text(modal_lbl_meta, "HP: - | PP: - | Item: -");

        // Botao Fechar [X]
        lv_obj_t * btn_close = lv_btn_create(modal_panel);
        lv_obj_set_size(btn_close, 64, 34);
        lv_obj_set_pos(btn_close, 392, 2);
        lv_obj_set_style_bg_color(btn_close, lv_color_hex(0xFF1744), LV_PART_MAIN);
        lv_obj_set_style_radius(btn_close, 6, LV_PART_MAIN);
        lv_obj_add_event_cb(btn_close, [](lv_event_t * e) {
            static_cast<PokemonLegendsZAModule*>(lv_event_get_user_data(e))->closeModal();
        }, LV_EVENT_CLICKED, this);
        lv_obj_t * lbl_close = lv_label_create(btn_close);
        lv_obj_set_style_text_font(lbl_close, &lv_font_montserrat_12, LV_PART_MAIN);
        lv_label_set_text(lbl_close, LV_SYMBOL_CLOSE " Voltar");
        lv_obj_center(lbl_close);

        auto makeModalBtn = [&](lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h,
                                uint32_t bg_hex, const char * txt, lv_event_cb_t cb) {
            lv_obj_t * b = lv_btn_create(modal_panel);
            lv_obj_set_size(b, w, h);
            lv_obj_set_pos(b, x, y);
            lv_obj_set_style_bg_color(b, lv_color_hex(bg_hex), LV_PART_MAIN);
            lv_obj_set_style_radius(b, 6, LV_PART_MAIN);
            lv_obj_set_style_pad_all(b, 2, LV_PART_MAIN);
            lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, this);
            lv_obj_t * l = lv_label_create(b);
            lv_obj_set_style_text_font(l, &lv_font_montserrat_12, LV_PART_MAIN);
            lv_obj_set_style_text_color(l, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
            lv_label_set_text(l, txt);
            lv_obj_center(l);
        };

        // Linha 1 (y=44): Curar HP | Curar Status | Restaurar PP | Full Restore
        makeModalBtn(2, 44, 110, 38, 0x1B5E20, "+ Curar HP", [](lv_event_t * e) {
            static_cast<PokemonLegendsZAModule*>(lv_event_get_user_data(e))
                ->triggerPokeEdit(PokeEditAction::HEAL_HP, 0, "HP restaurado em 100%!");
        });
        makeModalBtn(116, 44, 110, 38, 0x006064, "Curar Status", [](lv_event_t * e) {
            static_cast<PokemonLegendsZAModule*>(lv_event_get_user_data(e))
                ->triggerPokeEdit(PokeEditAction::HEAL_STATUS, 0, "Status negativo removido!");
        });
        makeModalBtn(230, 44, 110, 38, 0x4A148C, "Restaurar PP", [](lv_event_t * e) {
            static_cast<PokemonLegendsZAModule*>(lv_event_get_user_data(e))
                ->triggerPokeEdit(PokeEditAction::RESTORE_PP, 0, "PP dos 4 golpes restaurado!");
        });
        makeModalBtn(344, 44, 112, 38, 0x00897B, "Full Restore", [](lv_event_t * e) {
            static_cast<PokemonLegendsZAModule*>(lv_event_get_user_data(e))
                ->triggerPokeEdit(PokeEditAction::FULL_RESTORE, 0, "HP + Status + PP 100%!");
        });

        // Linha 2 (y=88): Shiny ON/OFF | Max 6 IVs (31) | Curar Time Todo
        makeModalBtn(2, 88, 146, 38, 0x8D6E00, "* Shiny ON/OFF", [](lv_event_t * e) {
            static_cast<PokemonLegendsZAModule*>(lv_event_get_user_data(e))
                ->triggerPokeEdit(PokeEditAction::TOGGLE_SHINY, 0, "Shiny alternado no PID!");
        });
        makeModalBtn(154, 88, 146, 38, 0x1A237E, "Max 6 IVs (31)", [](lv_event_t * e) {
            static_cast<PokemonLegendsZAModule*>(lv_event_get_user_data(e))
                ->triggerPokeEdit(PokeEditAction::MAX_IVS, 0, "6 IVs definidos em 31!");
        });
        makeModalBtn(306, 88, 150, 38, 0xAD1457, "+ Curar Time Todo", [](lv_event_t * e) {
            static_cast<PokemonLegendsZAModule*>(lv_event_get_user_data(e))
                ->triggerPokeEdit(PokeEditAction::HEAL_PARTY_ALL, 0, "Time inteiro 100% curado!");
        });

        // Linha 3 (y=132): Equipar Item Rapido (Leftovers, Sitrus, Lucky Egg, Life Orb, Sem Item)
        makeModalBtn(2, 132, 88, 36, 0x263238, "Leftovers", [](lv_event_t * e) {
            static_cast<PokemonLegendsZAModule*>(lv_event_get_user_data(e))
                ->triggerPokeEdit(PokeEditAction::SET_ITEM, 234, "Item equipado: Leftovers!");
        });
        makeModalBtn(93, 132, 88, 36, 0x263238, "Sitrus B.", [](lv_event_t * e) {
            static_cast<PokemonLegendsZAModule*>(lv_event_get_user_data(e))
                ->triggerPokeEdit(PokeEditAction::SET_ITEM, 158, "Item equipado: Sitrus Berry!");
        });
        makeModalBtn(184, 132, 88, 36, 0x263238, "Lucky Egg", [](lv_event_t * e) {
            static_cast<PokemonLegendsZAModule*>(lv_event_get_user_data(e))
                ->triggerPokeEdit(PokeEditAction::SET_ITEM, 231, "Item equipado: Lucky Egg!");
        });
        makeModalBtn(275, 132, 88, 36, 0x263238, "Life Orb", [](lv_event_t * e) {
            static_cast<PokemonLegendsZAModule*>(lv_event_get_user_data(e))
                ->triggerPokeEdit(PokeEditAction::SET_ITEM, 270, "Item equipado: Life Orb!");
        });
        makeModalBtn(366, 132, 90, 36, 0x37474F, "Sem Item", [](lv_event_t * e) {
            static_cast<PokemonLegendsZAModule*>(lv_event_get_user_data(e))
                ->triggerPokeEdit(PokeEditAction::SET_ITEM, 0, "Item removido do Pokemon!");
        });

        modal_lbl_feedback = lv_label_create(modal_panel);
        lv_obj_set_style_text_font(modal_lbl_feedback, &lv_font_montserrat_12, LV_PART_MAIN);
        lv_obj_set_style_text_color(modal_lbl_feedback, lv_color_hex(0x00E676), LV_PART_MAIN);
        lv_obj_set_pos(modal_lbl_feedback, 4, 174);
        lv_label_set_text(modal_lbl_feedback, "Toque numa acao para aplicar na RAM");

        party_drawn = true;
    }

    // ---------------- Aba Macros ----------------
    bool hasCustomMacroTab() const override { return true; }

    void buildMacroTab(lv_obj_t * parent, MacroHandler cb, void * hctx) override {
        menu.build(parent, cats, 5, slotdefs, SLOTS, cb, hctx);
    }

    // Chamado a cada 100 ms pelo UIManager (so para o jogo ativo). So mexe no LVGL quando algo muda.
    void updateStatusTab(const GameState &state) override {
        menu.update(state.active_macro_slot, &state.um);

        if (AssetManager::isSDReady() != last_sd_ready && lbl_sd) {
            last_sd_ready = AssetManager::isSDReady();
            lv_obj_set_style_text_color(lbl_sd, lv_color_hex(last_sd_ready ? 0x00E676 : 0x8888AA), LV_PART_MAIN);
            lv_label_set_text(lbl_sd, last_sd_ready ? LV_SYMBOL_SD_CARD " SD 16GB" : LV_SYMBOL_SD_CARD " Flash");
        }

        for (uint8_t i = 0; i < PARTY; i++) {
            if (!sameSlot(state.party[i], last_party[i])) {
                last_party[i] = state.party[i];
                paintParty(i, last_party[i]);
            }
        }

        if (state.map.x != last_mx || state.map.y != last_my || state.map.z != last_mz ||
            strncmp(state.map.zone_name, last_zone, sizeof(last_zone)) != 0) {
            last_mx = state.map.x; last_my = state.map.y; last_mz = state.map.z;
            snprintf(last_zone, sizeof(last_zone), "%s", state.map.zone_name);
            char zb[96];
            if (last_zone[0]) {
                snprintf(zb, sizeof(zb), LV_SYMBOL_GPS " %s   #AAAACC X:%.0f  Z:%.0f  Alt:%.0f#",
                         last_zone, state.map.x, state.map.y, state.map.z);
            } else {
                snprintf(zb, sizeof(zb), LV_SYMBOL_GPS " Lumiose   #AAAACC X:%.0f  Z:%.0f  Alt:%.0f#",
                         state.map.x, state.map.y, state.map.z);
            }
            lv_label_set_text(lbl_zone, zb);
        }
    }

    void formatMapCoordText(char * buf, size_t buf_size, const GameState &state) override {
        snprintf(buf, buf_size, "X: %.0f  Z: %.0f  Alt: %.0f", state.map.x, state.map.y, state.map.z);
    }

    const char* getMacroLabel(uint8_t slot) const override {
        return menu.label(slot);
    }

    void setVisible(bool visible) override {
        if (container) { if (visible) lv_obj_clear_flag(container, LV_OBJ_FLAG_HIDDEN); else lv_obj_add_flag(container, LV_OBJ_FLAG_HIDDEN); }
        menu.setVisible(visible);
    }
};

#endif // POKEMON_LEGENDS_ZA_MODULE_H
