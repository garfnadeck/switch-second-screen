#ifndef I_GAME_PROVIDER_H
#define I_GAME_PROVIDER_H

#include "game_state.h"

class IGameProvider {
public:
    virtual ~IGameProvider() = default;
    virtual void init(GameState &state) = 0;
    virtual void setProfile(GameState &state, GameProfile new_profile) = 0;
    virtual void update(GameState &state) = 0;
    virtual void triggerAction(ActionMacro action, GameState &state) = 0;
};

#endif // I_GAME_PROVIDER_H