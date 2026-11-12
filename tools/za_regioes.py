#!/usr/bin/env python3
"""Mostra quais regioes de memoria o jogo usa FORA da faixa copiada (sem rede).

Todo ponteiro guardado na copia que aponta para fora dela indica uma regiao em uso que ainda nao
foi lida. O script conta esses ponteiros por blocos de 16 MB e mostra tambem para onde apontam os
objetos ligados ao save (treinador, caixas...), que e onde o time deve estar.

Uso:  python3 tools/za_regioes.py
"""
import argparse
import collections
import json
import os
import struct
import sys

BALDE = 0x1000000        # 16 MB


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--pasta", default="dump_za")
    args = ap.parse_args()

    with open(os.path.join(args.pasta, "meta.json")) as f:
        meta = json.load(f)
    with open(os.path.join(args.pasta, "heap.bin"), "rb") as f:
        heap = f.read()
    with open(os.path.join(args.pasta, "main.bin"), "rb") as f:
        principal = f.read()
    lo, hi = meta["heap_ini"], meta["heap_fim"]
    print(f"Copia de {meta['quando']}: heap 0x{lo:X}..0x{hi:X}")

    # enderecos de dados do jogo ficam na mesma "familia" alta do heap (0x5x........)
    fam_lo, fam_hi = (lo >> 36) << 36, ((lo >> 36) + 1) << 36

    def fora(v):
        return fam_lo <= v < fam_hi and not (lo <= v < hi) and v % 8 == 0

    contagem = collections.Counter()
    for dados in (heap, principal):
        for v in memoryview(dados)[:len(dados) & ~7].cast("Q"):
            if fam_lo <= v < fam_hi and not (lo <= v < hi) and v % 8 == 0:
                contagem[v // BALDE] += 1

    total = sum(contagem.values())
    print(f"\n{total} ponteiros para fora da copia, em {len(contagem)} blocos de 16 MB.")
    print("Regioes mais apontadas (em ordem de endereco, so as com 50 ponteiros ou mais):")
    faixa_ini = None
    anterior = None
    soma = 0
    linhas = []
    for balde in sorted(b for b, n in contagem.items() if n >= 50):
        if anterior is not None and balde != anterior + 1:
            linhas.append((faixa_ini, anterior, soma))
            faixa_ini, soma = None, 0
        if faixa_ini is None:
            faixa_ini = balde
        soma += contagem[balde]
        anterior = balde
    if faixa_ini is not None:
        linhas.append((faixa_ini, anterior, soma))
    for a, z, n in linhas:
        print(f"  0x{a * BALDE:X} .. 0x{(z + 1) * BALDE:X}  ({(z - a + 1) * 16} MB)  {n} ponteiros")
    print(f"  total dessas regioes: {sum((z - a + 1) * 16 for a, z, _ in linhas)} MB")

    # objetos ligados ao save e para onde eles apontam fora da copia
    off_main = meta["base_main"] + meta["base_save"] - meta["main_ini"]
    save = struct.unpack_from("<Q", principal, off_main)[0]
    print(f"\nObjeto do save em 0x{save:X}. Ponteiros dos seus blocos para fora da copia:")
    if not (lo <= save < hi):
        print("  (o objeto do save esta fora da copia)")
        return 0
    for a in range(0, 0x400, 8):
        obj = struct.unpack_from("<Q", heap, save - lo + a)[0]
        if not (lo <= obj < hi - 0x1000):
            if fora(obj):
                print(f"  save+0x{a:03X} -> 0x{obj:X} (fora)")
            continue
        longe = []
        for b2 in range(0, 0x1000, 8):
            v = struct.unpack_from("<Q", heap, obj - lo + b2)[0]
            if fora(v):
                longe.append(f"+0x{b2:X}->0x{v:X}")
        if longe:
            print(f"  save+0x{a:03X} -> 0x{obj:X}: " + ", ".join(longe[:6]) + (" ..." if len(longe) > 6 else ""))
    return 0


if __name__ == "__main__":
    sys.exit(main())
