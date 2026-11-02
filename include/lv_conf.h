#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/* Color settings */
#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 0

/* Memory settings - Pool interno seguro para Mac, ESP32-S3 e RP2040 (Pico W) */
#if defined(ARDUINO) && defined(BOARD_HAS_PSRAM)
    /* ESP32-S3 N16R8: o pool do LVGL vai para a PSRAM (8 MB) -> sem limite apertado de RAM interna.
       9 jogos (Z-A + FireRed com 41 botoes de macro) usam ~140 KB. */
    #define LV_MEM_CUSTOM 1
    #define LV_MEM_CUSTOM_INCLUDE <esp_heap_caps.h>
    #define LV_MEM_CUSTOM_ALLOC(size)       heap_caps_malloc((size), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
    #define LV_MEM_CUSTOM_FREE(p)           heap_caps_free(p)
    #define LV_MEM_CUSTOM_REALLOC(p, size)  heap_caps_realloc((p), (size), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
#else
    #define LV_MEM_CUSTOM 0
    #define LV_MEM_SIZE (256U * 1024U) /* simulador / boards sem PSRAM: 9 jogos usam ~140 KB */
#endif

/* HAL settings */
#define LV_TICK_CUSTOM 1
#if defined(ARDUINO)
    #define LV_TICK_CUSTOM_INCLUDE "Arduino.h"
    #define LV_TICK_CUSTOM_SYS_TIME_EXPR (millis())
#else
    #define LV_TICK_CUSTOM_INCLUDE <SDL2/SDL.h>
    #define LV_TICK_CUSTOM_SYS_TIME_EXPR (SDL_GetTicks())
#endif

/* Feature configuration */
#define LV_USE_PERF_MONITOR 1
#define LV_USE_MEM_MONITOR 0

/* Fonts */
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_28 1
#define LV_FONT_DEFAULT &lv_font_montserrat_14

/* Widgets */
#define LV_USE_ARC        1
#define LV_USE_BAR        1
#define LV_USE_BTN        1
#define LV_USE_BTNMATRIX  1
#define LV_USE_CANVAS     1
#define LV_USE_CHECKBOX   1
#define LV_USE_DROPDOWN   1
#define LV_USE_IMG        1
#define LV_USE_LABEL      1
#define LV_USE_LINE       1
#define LV_USE_ROLLER     1
#define LV_USE_SLIDER     1
#define LV_USE_SWITCH     1
#define LV_USE_TEXTAREA   1
#define LV_USE_TABLE      1
#define LV_USE_TABVIEW    1

#endif /* LV_CONF_H */