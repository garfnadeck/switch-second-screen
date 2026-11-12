#!/usr/bin/env python3
"""Acha o HP ao vivo comparando a copia de memoria (valores antigos) com o jogo agora (valores novos).

1. Na copia (za_dump_mem.py), procura todo lugar com o HP atual e o maximo ANTIGOS lado a lado,
   em varios formatos (16 bits, 32 bits, float; nas duas ordens).
2. Le esses enderecos agora, pela rede, e mantem os que mostram os valores NOVOS.
3. Para os que sobram, procura na copia as cadeias de ponteiros fixas ate eles.
So funciona se o jogo NAO foi fechado desde a copia (os enderecos mudam ao reabrir).

Uso:  python3 tools/za_valor.py --ip 192.168.15.11 --antes 23 29 --agora 10 32
"""
import argparse
import json
import os
import re
import socket
import struct
import sys
import time

from za_cadeias import buscar_cadeias, indexar
from za_scan_pos import comando

FORMATOS = {"u16": "<HH", "u32": "<II", "float": "<ff"}


def padroes(atual, maximo):
    """{rotulo: bytes} para (atual, maximo) e (maximo, atual) em cada formato."""
    saida = {}
    for nome, fmt in FORMATOS.items():
        conv = float if nome == "float" else int
        saida[f"{nome} atual,max"] = struct.pack(fmt, conv(atual), conv(maximo))
        saida[f"{nome} max,atual"] = struct.pack(fmt, conv(maximo), conv(atual))
    return saida


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ip", required=True)
    ap.add_argument("--porta", type=int, default=6000)
    ap.add_argument("--pasta", default="dump_za")
    ap.add_argument("--antes", type=int, nargs=2, required=True, metavar=("HP", "MAX"),
                    help="HP atual e maximo na hora da copia")
    ap.add_argument("--agora", type=int, nargs=2, required=True, metavar=("HP", "MAX"),
                    help="HP atual e maximo neste momento")
    ap.add_argument("--max-cadeias", type=int, default=8)
    args = ap.parse_args()
    if tuple(args.antes) == tuple(args.agora):
        print("Os valores de antes e de agora precisam ser diferentes.")
        return 1

    with open(os.path.join(args.pasta, "meta.json")) as f:
        meta = json.load(f)
    with open(os.path.join(args.pasta, "heap.bin"), "rb") as f:
        heap = f.read()
    print(f"Copia de {meta['quando']}. Procurando {args.antes[0]}/{args.antes[1]}...")

    antigos = padroes(*args.antes)
    novos = padroes(*args.agora)
    candidatos = []                                        # (endereco, rotulo)
    for rotulo, padrao in antigos.items():
        passo = 2 if rotulo.startswith("u16") else 4
        n = 0
        for m in re.finditer(re.escape(padrao), heap):
            if m.start() % passo == 0:
                candidatos.append((meta["heap_ini"] + m.start(), rotulo))
                n += 1
        print(f"  {rotulo}: {n} lugares")
    if not candidatos:
        print("Nenhum lugar com esses valores na copia.")
        return 1

    sock = socket.create_connection((args.ip, args.porta), timeout=15)
    if comando(sock, "getBuildID").lower() != meta["build"].lower():
        print("O jogo aberto nao e o mesmo da copia.")
        return 1
    print(f"Lendo {len(candidatos)} enderecos agora, procurando {args.agora[0]}/{args.agora[1]}...")
    t0 = time.time()
    vivos, iguais = [], 0
    for end, rotulo in candidatos:
        try:
            lido = bytes.fromhex(comando(sock, f"peekAbsolute 0x{end:X} 8"))
        except ValueError:
            continue
        if lido == novos[rotulo]:
            vivos.append((end, rotulo))
        elif lido == antigos[rotulo]:
            iguais += 1
    print(f"  {len(vivos)} mudaram para o valor novo; {iguais} continuam com o valor antigo; "
          f"{len(candidatos) - len(vivos) - iguais} viraram outra coisa. ({time.time() - t0:.0f} s)")
    if not vivos:
        print("Nenhum endereco acompanhou a mudanca. O HP ao vivo nao esta na faixa da copia nesses formatos.")
        return 1

    for end, rotulo in vivos:
        try:
            contexto = bytes.fromhex(comando(sock, f"peekAbsolute 0x{end - 0x20:X} 96"))
        except ValueError:
            contexto = b""
        print(f"\n  HP AO VIVO em 0x{end:X} ({rotulo}). 0x20 antes ate 0x40 depois:\n    {contexto.hex()}")

    with open(os.path.join(args.pasta, "main.bin"), "rb") as f:
        principal = f.read()
    print("\nIndexando ponteiros da copia (pode levar um minuto)...")
    pares = indexar(meta, heap, principal)
    for end, rotulo in vivos[:6]:
        print(f"\nCadeias ate 0x{end:X} ({rotulo}):")
        cadeias = buscar_cadeias(meta, pares, end, 0x800, 7, 20000, args.max_cadeias)
        cadeias.sort(key=lambda c: (len(c), sum(c[1:])))
        for c in cadeias[:args.max_cadeias]:
            print("  pointerPeek 8 " + " ".join(f"0x{x:X}" for x in c))
        if not cadeias:
            print("  nenhuma cadeia completa dentro da copia.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
