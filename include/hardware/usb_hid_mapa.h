#pragma once

// Mapeamento dos bits do relatorio HID do controle HORI, medido no NS2 (10/11).
// bit = posicao no campo de 16 botoes do descritor. Os bits 14 e 15 nao tem botao.
// Ordem diferente da do Pro Controller: use estes nomes, nao os do codigo antigo.

#include <stdint.h>

#define BIT_Y          0
#define BIT_B          1
#define BIT_A          2
#define BIT_X          3
#define BIT_L          4
#define BIT_R          5
#define BIT_ZL         6
#define BIT_ZR         7
#define BIT_MINUS      8
#define BIT_PLUS       9
#define BIT_STICK_L   10
#define BIT_STICK_R   11
#define BIT_HOME      12
#define BIT_CAPTURE   13

#define BOTAO(bit)    ((uint16_t)(1u << (bit)))
