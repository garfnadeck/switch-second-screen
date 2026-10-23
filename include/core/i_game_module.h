#ifndef I_GAME_MODULE_H
#define I_GAME_MODULE_H

#include <lvgl.h>
#include "game_state.h"

// Chamado quando o usuario toca num botao de macro desenhado pelo modulo (slot 0..MACRO_COUNT-1)
typedef void (*MacroHandler)(void * ctx, uint8_t slot);

class IGameModule {
public:
    virtual ~IGameModule() = default;

    // Nome que aparece no seletor Dropdown
    virtual const char* getGameName() const = 0;

    // Constrói os elementos visuais na Aba Status
    virtual void buildStatusTab(lv_obj_t * parent) = 0;

    // Atualiza os elementos visuais a cada ciclo de 100ms
    virtual void updateStatusTab(const GameState &state) = 0;

    // Formata o texto de coordenadas do mapa (2D ou 3D com eixo Z)
    virtual void formatMapCoordText(char * buf, size_t buf_size, const GameState &state) = 0;

    // Retorna o nome de cada um dos 6 botões de Macro daquele jogo
    virtual const char* getMacroLabel(uint8_t slot) const = 0;

    // Controle de visibilidade ao trocar de jogo
    virtual void setVisible(bool visible) = 0;

    // ---- Opcional: layout proprio da Aba Macros (com ponto de estado por slot) ----
    // Modulos que nao sobrescrevem usam os 6 botoes genericos do UIManager.
    virtual bool hasCustomMacroTab() const { return false; }
    virtual void buildMacroTab(lv_obj_t * parent, MacroHandler cb, void * ctx) {
        (void)parent; (void)cb; (void)ctx;
    }
};

#endif // I_GAME_MODULE_H