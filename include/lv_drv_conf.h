#ifndef LV_DRV_CONF_H
#define LV_DRV_CONF_H

#include "lv_conf.h"

#ifndef ARDUINO
    #define USE_SDL 1
    #define SDL_HOR_RES     480
    #define SDL_VER_RES     320
    #define SDL_ZOOM        1
    #define SDL_INCLUDE_PATH <SDL2/SDL.h>
#endif

#endif /* LV_DRV_CONF_H */