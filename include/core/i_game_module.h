#ifndef I_GAME_MODULE_H
#define I_GAME_MODULE_H

#include <lvgl.h>
#include "game_state.h"

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
};

#endif // I_GAME_MODULE_H