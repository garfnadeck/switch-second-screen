#pragma once

/* Sequencia de init do ST77922 da ES3C35P (definida em src/hardware/st77922_init_table.c). */
#include <stddef.h>
#include "esp_lcd_st77922.h"

#ifdef __cplusplus
extern "C" {
#endif
extern const st77922_lcd_init_cmd_t ES3C35P_INIT_CMDS[];
extern const size_t ES3C35P_INIT_CMDS_COUNT;
#ifdef __cplusplus
}
#endif
