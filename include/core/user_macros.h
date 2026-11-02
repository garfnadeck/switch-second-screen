#ifndef USER_MACROS_H
#define USER_MACROS_H

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// Macros criados pelo usuario (pagina web: editor + gravador). Valem para qualquer jogo.
// Cada passo = "segurar estes botoes + direcional por X ms". Soltar tudo = passo com botoes 0 (espera).
// Texto de passos: "botoes:direcional:ms;botoes:direcional:ms;..." (botoes = mascara SwitchButton em decimal,
// direcional 0..7 ou 8 = neutro).
static constexpr uint8_t  UM_COUNT      = 6;     // 6 slots (grade 3x2 da categoria "Meus macros")
static constexpr uint8_t  UM_MAX_STEPS  = 64;
static constexpr uint8_t  USER_MACRO_BASE = 100; // slots 100..105 no resto do firmware
static constexpr uint8_t  UM_NAME_LEN   = 17;

struct UMStep { uint16_t btns; uint8_t hat; uint16_t ms; };

struct UserMacro {
    char     name[UM_NAME_LEN] = "";
    uint16_t repeat = 1;        // 0 = loop infinito
    uint8_t  n = 0;
    UMStep   s[UM_MAX_STEPS];
};

struct UserMacroStore {
    UserMacro m[UM_COUNT];
    uint16_t  version = 0;      // sobe a cada mudanca (a UI redesenha os nomes)

    // Nome so com caracteres seguros (a fonte do LVGL nao tem acentos e o '|' e separador do arquivo)
    static void cleanName(char * dst, const char * src) {
        uint8_t k = 0;
        for (const char * c = src; *c && k < UM_NAME_LEN - 1; c++) {
            char ch = *c;
            if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') ||
                ch == ' ' || ch == '-' || ch == '_' || ch == '.' || ch == '+') dst[k++] = ch;
        }
        dst[k] = 0;
    }

    // Interpreta o texto de passos. Devolve false se algo estiver invalido (nada e alterado).
    static bool parseSteps(const char * txt, UMStep * out, uint8_t & count) {
        uint8_t n = 0;
        const char * p = txt;
        while (*p) {
            char * e;
            unsigned long b = strtoul(p, &e, 10);       if (e == p || *e != ':') return false; p = e + 1;
            unsigned long h = strtoul(p, &e, 10);       if (e == p || *e != ':') return false; p = e + 1;
            unsigned long ms = strtoul(p, &e, 10);      if (e == p) return false; p = e;
            if (b > 0x3FFF || h > 8 || ms > 60000 || n >= UM_MAX_STEPS) return false;
            out[n++] = { (uint16_t)b, (uint8_t)h, (uint16_t)ms };
            if (*p == ';') p++;
            else if (*p) return false;
        }
        count = n;
        return true;
    }

    bool set(uint8_t i, const char * name, uint16_t repeat, const char * steps) {
        if (i >= UM_COUNT) return false;
        UMStep tmp[UM_MAX_STEPS]; uint8_t n = 0;
        if (!parseSteps(steps, tmp, n)) return false;
        UserMacro & u = m[i];
        cleanName(u.name, name);
        u.repeat = repeat;
        u.n = n;
        for (uint8_t k = 0; k < n; k++) u.s[k] = tmp[k];
        version++;
        return true;
    }

    void clear(uint8_t i) { if (i < UM_COUNT) { m[i].name[0] = 0; m[i].n = 0; m[i].repeat = 1; version++; } }

    // Passos -> texto (para a pagina e para o arquivo). Devolve o tamanho escrito.
    size_t stepsText(uint8_t i, char * out, size_t cap) const {
        size_t len = 0;
        if (cap) out[0] = 0;
        if (i >= UM_COUNT) return 0;
        for (uint8_t k = 0; k < m[i].n; k++) {
            char b[28];
            int w = snprintf(b, sizeof(b), "%s%u:%u:%u", k ? ";" : "", (unsigned)m[i].s[k].btns, (unsigned)m[i].s[k].hat, (unsigned)m[i].s[k].ms);
            if (w < 0 || len + (size_t)w + 1 > cap) break;
            memcpy(out + len, b, (size_t)w); len += (size_t)w; out[len] = 0;
        }
        return len;
    }

    const char * label(uint8_t i) const { return (i < UM_COUNT && m[i].name[0]) ? m[i].name : "(vazio)"; }
};

#endif // USER_MACROS_H