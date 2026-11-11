#!/usr/bin/env python3
"""Acha cadeias de ponteiros fixas ate um Pokemon, usando a copia feita por za_dump_mem.py (sem rede).

1. Lista os objetos de Pokemon da copia (pela assinatura) e decifra cada um.
2. Para o Pokemon escolhido (--nome), procura de tras para frente quem aponta para ele, ate chegar
   em um ponteiro guardado nos dados estaticos do jogo (main). Cada caminho completo e uma cadeia
   que pode ir para o firmware.

Uso:  python3 tools/za_cadeias.py --nome Chikorita
"""
import argparse
import bisect
import json
import os
import struct
import sys
import time

from za_pa9 import TAM_CAIXA, especie, validar


def carregar(pasta):
    with open(os.path.join(pasta, "meta.json")) as f:
        meta = json.load(f)
    with open(os.path.join(pasta, "heap.bin"), "rb") as f:
        heap = f.read()
    with open(os.path.join(pasta, "main.bin"), "rb") as f:
        principal = f.read()
    return meta, heap, principal


def listar_pokemon(meta, heap):
    """Devolve [(endereco do objeto, apelido, especie, nivel, hp, hpmax)] dos objetos com dados."""
    ini, fim = meta["heap_ini"], meta["heap_fim"]
    padrao = struct.pack("<Q", meta["assinatura"])
    saida = []
    pos = heap.find(padrao)
    while pos != -1:
        if pos % 8 == 0 and pos + 24 <= len(heap):
            extra_end, bloco_end = struct.unpack_from("<QQ", heap, pos + 8)
            if ini <= bloco_end < fim - TAM_CAIXA and ini <= extra_end < fim - 0x10:
                bloco = heap[bloco_end - ini:bloco_end - ini + TAM_CAIXA]
                extra = heap[extra_end - ini:extra_end - ini + 0x10]
                if bloco[:4] != b"\0\0\0\0":
                    claro, _ = validar(bloco + extra)
                    if claro is not None:
                        nome = claro[0x58:0x58 + 26].decode("utf-16-le", errors="replace").split("\0")[0]
                        saida.append((ini + pos, nome, especie(claro), claro[0x148],
                                      struct.unpack_from("<H", claro, 0x8A)[0],
                                      struct.unpack_from("<H", claro, 0x14A)[0]))
        pos = heap.find(padrao, pos + 1)
    return saida


def indexar(meta, heap, principal):
    """Lista ordenada de (valor, endereco onde o valor esta guardado) para valores que apontam para o heap."""
    lo, hi = meta["heap_ini"], meta["heap_fim"]
    pares = []
    for dados, origem in ((heap, lo), (principal, meta["main_ini"])):
        visao = memoryview(dados)[:len(dados) & ~7].cast("Q")
        for i, v in enumerate(visao):
            if lo <= v < hi:
                pares.append((v, origem + i * 8))
    pares.sort()
    return pares


def buscar_cadeias(meta, pares, alvo, max_off, niveis, max_estados, max_cadeias):
    """Busca de tras para frente. Estado = (endereco a alcancar, saltos ja fixados depois dele)."""
    valores = [p[0] for p in pares]
    main_ini, main_fim, base_main = meta["main_ini"], meta["main_fim"], meta["base_main"]
    cadeias = []
    estados = [(alvo, ())]
    visitados = {alvo}
    for nivel in range(1, niveis + 1):
        novos = []
        for endereco, saltos in estados:
            a = bisect.bisect_left(valores, endereco - max_off)
            z = bisect.bisect_right(valores, endereco)
            for valor, onde in pares[a:z]:
                cadeia = (endereco - valor,) + saltos
                if main_ini <= onde < main_fim:
                    cadeias.append((onde - base_main,) + cadeia)
                elif onde not in visitados:
                    visitados.add(onde)
                    novos.append((onde, cadeia))
        print(f"  nivel {nivel}: {len(novos)} caminhos em aberto, {len(cadeias)} cadeias completas")
        if len(cadeias) >= max_cadeias or not novos:
            break
        novos.sort(key=lambda e: sum(e[1]))               # prefere saltos pequenos
        estados = novos[:max_estados]
    return cadeias


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--pasta", default="dump_za")
    ap.add_argument("--nome", default="Chikorita")
    ap.add_argument("--max-off", type=lambda t: int(t, 0), default=0x400)
    ap.add_argument("--niveis", type=int, default=7)
    ap.add_argument("--estados", type=int, default=20000)
    ap.add_argument("--cadeias", type=int, default=40)
    args = ap.parse_args()

    meta, heap, principal = carregar(args.pasta)
    print(f"Copia de {meta['quando']}, build {meta['build']}")
    pokes = listar_pokemon(meta, heap)
    print(f"{len(pokes)} Pokemon com dados na copia:")
    for end, nome, esp, nivel, hp, hpmax in pokes:
        print(f"  0x{end:X}: '{nome}' especie {esp}, nivel {nivel}, HP {hp}/{hpmax}")
    alvos = [p for p in pokes if p[1].lower() == args.nome.lower()]
    if not alvos:
        print(f"Nenhum Pokemon chamado '{args.nome}' na copia.")
        return 1

    t0 = time.time()
    print("Indexando ponteiros (pode levar um minuto)...")
    pares = indexar(meta, heap, principal)
    print(f"  {len(pares)} ponteiros para o heap, {time.time() - t0:.0f} s")

    achou = False
    for end, nome, esp, nivel, hp, hpmax in alvos:
        print(f"\nCadeias ate '{nome}' (objeto em 0x{end:X}):")
        cadeias = buscar_cadeias(meta, pares, end, args.max_off, args.niveis, args.estados, args.cadeias)
        cadeias.sort(key=lambda c: (len(c), sum(c[1:])))
        for c in cadeias[:args.cadeias]:
            achou = True
            # c = (deslocamento no main, saltos..., ultimo salto somado): chega ao OBJETO do Pokemon
            objeto = " ".join(f"0x{x:X}" for x in c)
            bloco = " ".join(f"0x{x:X}" for x in c[:-1] + (c[-1] + 0x10, 0))
            extra = " ".join(f"0x{x:X}" for x in c[:-1] + (c[-1] + 0x8, 0))
            print(f"  objeto: pointerAll {objeto}")
            print(f"     bloco (0x148):   pointerPeek 328 {bloco}")
            print(f"     extensao (0x10): pointerPeek 16 {extra}")
        if not cadeias:
            print("  nenhuma cadeia completa. Tente --max-off 0x1000 ou --niveis 9.")
    return 0 if achou else 1


if __name__ == "__main__":
    sys.exit(main())
