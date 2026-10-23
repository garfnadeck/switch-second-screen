#ifndef ZA_MEMORY_MAP_H
#define ZA_MEMORY_MAP_H

#include <stdint.h>

// =====================================================================================
//  MAPA DE MEMORIA DO POKEMON LEGENDS Z-A  (Atmosphere + sys-botbase)
//
//  >>> TUDO ABAIXO COMECA ZERADO DE PROPOSITO <<<
//  Eu nao tenho os enderecos reais do Z-A. Eles dependem da VERSAO do jogo (e do build
//  id) e normalmente vem de pesquisa da comunidade ou de busca na memoria com um debugger.
//  Preencha so este arquivo; o resto (leitura, parse, telas) ja esta pronto.
//
//  Cadeia de ponteiros = argumentos do comando "pointerPeek" do sys-botbase:
//     pointerPeek <tamanho> <salto1> <salto2> ... <saltoN>
//  Ex.: { 3, {0x4A1B2C0, 0x18, 0x70} }  ->  pointerPeek N 0x4A1B2C0 0x18 0x70
//  Os campos abaixo sao deslocamentos (bytes) dentro do bloco lido.
// =====================================================================================

struct MemChain {
    uint8_t  count;       // 0 = nao configurado
    uint32_t jumps[8];
};

struct ZaMemoryMap {
    // ---- Time (party) ----
    MemChain party;               // aponta para o INICIO do 1o Pokemon do time
    uint16_t party_stride;        // bytes entre um Pokemon e o proximo
    uint8_t  party_count;         // normalmente 6
    uint16_t off_species;         // u16
    uint16_t off_level;           // u8
    uint16_t off_hp_cur;          // u16
    uint16_t off_hp_max;          // u16
    uint16_t off_status;          // u8 (0 = nenhum; veja status_name())
    // ---- Posicao do jogador ----
    MemChain pos;                 // aponta para 3 floats consecutivos (x,y,z)
    uint16_t off_x, off_y, off_z; // float32 dentro do bloco (ex.: 0, 4, 8)
    // ---- Conversao para o mapa (pixels dos tiles). Calibrar andando ate pontos conhecidos ----
    float world_min_x, world_max_x;   // coordenadas do jogo nas bordas do mapa
    float world_min_z, world_max_z;
};

// PREENCHER:
static const ZaMemoryMap ZA_MAP = {
    /* party        */ { 0, { 0 } },
    /* party_stride */ 0, /* party_count */ 6,
    /* offsets      */ 0, 0, 0, 0, 0,
    /* pos          */ { 0, { 0 } },
    /* off x,y,z    */ 0, 4, 8,
    /* world bounds */ 0.f, 0.f, 0.f, 0.f
};

#endif // ZA_MEMORY_MAP_H