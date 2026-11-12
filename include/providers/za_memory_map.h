#ifndef ZA_MEMORY_MAP_H
#define ZA_MEMORY_MAP_H

#include <stdint.h>

// =====================================================================================
//  MAPA DE MEMORIA DO POKEMON LEGENDS Z-A  (Atmosphere + sys-botbase)
//
//  Os enderecos dependem da VERSAO do jogo (build id). Os valores abaixo foram medidos e
//  validados ao vivo no console do usuario em 12/11/2026 na versao 2.0.2 (build b1f12fd919eae86a).
//  Pesquisa, detalhes do formato PA9 e historico: docs/ponteiros_za.md
//
//  - Time (6 slots): PREENCHIDO e validado ao vivo (Chikorita, Fletchling, Bunnelby, Scatterbug...).
//    Em [0x610A710, 0x18, 0x1B0], os 6 slots ficam em +0xF0, +0xF8, +0x100, +0x108, +0x110, +0x118.
//    Dentro de cada slot (+0x50), o bloco PA9 cifrado (0x148 = 328 bytes) fica em +0x30 e a
//    extensao de time cifrada (0x10 = 16 bytes: nivel e HP maximo) fica em +0x50.
//  - Posicao: PREENCHIDA (0x47D71A0, 0x248, 0x0, 0x138, +0x90) e validada ao vivo na 2.0.2.
//
//  Cadeia de ponteiros = argumentos do comando "pointerPeek" do sys-botbase:
//     pointerPeek <tamanho> <salto1> <salto2> ... <saltoN>
//  Ex.: { 3, {0x4A1B2C0, 0x18, 0x70} }  ->  pointerPeek N 0x4A1B2C0 0x18 0x70
//  Os campos abaixo sao deslocamentos (bytes) dentro do bloco decifrado (PA9 + extensao).
// =====================================================================================

struct MemChain {
    uint8_t  count;       // 0 = nao configurado
    uint32_t jumps[8];
};

struct ZaMemoryMap {
    // ---- Time (party) ----
    MemChain party;               // cadeia do 1o slot: [BASE, 0x18, 0x1B0, 0xF0, 0x50, 0x30, 0x0]
    uint16_t party_stride;        // tamanho do bloco principal PA9 (0x148 = 328 bytes)
    uint8_t  party_count;         // 6 slots (saltos em jumps[3] = 0xF0 + slot * 8)
    uint16_t off_species;         // u16 no bloco PA9 decifrado (0x08)
    uint16_t off_level;           // u8 na extensao decifrada (0x148 -> extra+0x00)
    uint16_t off_hp_cur;          // u16 no bloco PA9 decifrado (0x8A)
    uint16_t off_hp_max;          // u16 na extensao decifrada (0x14A -> extra+0x02)
    uint16_t off_status;          // u8 no bloco PA9 decifrado (0x90)
    // ---- Posicao do jogador ----
    MemChain pos;                 // aponta para 3 floats consecutivos (x,y,z)
    uint16_t off_x, off_y, off_z; // float32 dentro do bloco (ex.: 0, 4, 8)
    // ---- Conversao para o mapa (pixels dos tiles). Calibrar andando ate pontos conhecidos ----
    float world_min_x, world_max_x;   // coordenadas do jogo nas bordas do mapa
    float world_min_z, world_max_z;
};

static const ZaMemoryMap ZA_MAP = {
    // Slot 0: pointerPeek 328 0x610A710 0x18 0x1B0 0xF0 0x50 0x30 0x0 (bloco 0x148)
    //         pointerPeek 16  0x610A710 0x18 0x1B0 0xF0 0x50 0x50 0x0 (extensao 0x10)
    // Para os slots 0..5, jumps[3] varia de 0xF0 ate 0x118 (passo 8).
    /* party        */ { 7, { 0x610A710, 0x18, 0x1B0, 0xF0, 0x50, 0x30, 0x0 } },
    /* party_stride */ 0x148, /* party_count */ 6,
    /* offsets      */ 0x08, 0x148, 0x8A, 0x14A, 0x90,
    // [[[[main+0x47D71A0]+0x248]+0x0]+0x138]+0x90: QUATRO saltos seguidos e so entao o +0x90.
    // (No pointerPeek o ultimo numero e somado sem ser seguido, por isso o 0x90 vai separado.)
    /* pos          */ { 5, { 0x47D71A0, 0x248, 0x0, 0x138, 0x90 } },
    /* off x,y,z    */ 0, 4, 8,
    // Calibrado ao vivo em Lumiose (mapa_lumiose.jpg):
    // Centro (Campo 20 / Torre Prisma): X = -830.0, Z = -500.0 | Raio da cidade: R = 490.0
    // (+X = Leste/Direita, +Z = Norte/Cima, -Z = Sul/Baixo onde fica o Campo 1 em X~=-758, Z~=-885)
    /* world bounds */ -1320.f, -340.f, -990.f, -10.f
};

#endif // ZA_MEMORY_MAP_H