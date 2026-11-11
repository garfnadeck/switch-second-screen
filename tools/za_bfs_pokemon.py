#!/usr/bin/env python3
"""Acha os Pokemon na memoria do Legends Z-A andando pelos objetos a partir do save (so leitura).

Cada Pokemon e um objeto pequeno: [assinatura][ponteiro da extensao 0x10][ponteiro do bloco 0x148].
A assinatura e lida do Pokemon em branco ja conhecido ([BASE, 0x18, 0x1B0]). O script percorre os
objetos alcancaveis a partir do save, nivel por nivel, e para cada objeto com essa assinatura le
as duas partes e mostra o Pokemon e o caminho de ponteiros ate ele.

Uso:  python3 tools/za_bfs_pokemon.py --ip 192.168.15.9
"""
import argparse
import socket
import struct
import sys
import time
from collections import deque

from za_pa9 import TAM_CAIXA, especie, validar
from za_scan_heap import ler_abs, ponteiro
from za_scan_pos import comando

JANELA = 0x400


def descrever(sock, objeto):
    """Le as duas partes de um Pokemon e devolve (texto, vazio?)."""
    extra_end, bloco_end = struct.unpack("<QQ", ler_abs(sock, objeto + 8, 16))
    bloco = ler_abs(sock, bloco_end, TAM_CAIXA)
    extra = ler_abs(sock, extra_end, 0x10)
    if bloco[:4] == b"\0\0\0\0":
        return "em branco", True
    claro, modo = validar(bloco + extra)
    if claro is None:
        return f"bloco nao validou; inicio {bloco[:16].hex()}", False
    nome = claro[0x58:0x58 + 26].decode("utf-16-le", errors="replace").split("\0")[0]
    return (f"'{nome}' especie interna {especie(claro)}, nivel {claro[0x148]}, "
            f"HP {struct.unpack_from('<H', claro, 0x8A)[0]}/{struct.unpack_from('<H', claro, 0x14A)[0]}"
            f" ({modo})"), False


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ip", required=True)
    ap.add_argument("--porta", type=int, default=6000)
    ap.add_argument("--base", type=lambda t: int(t, 0), default=0x5F0B250)
    ap.add_argument("--niveis", type=int, default=5, help="profundidade maxima a partir do save")
    ap.add_argument("--max", type=int, default=6000, help="maximo de objetos a visitar")
    args = ap.parse_args()

    sock = socket.create_connection((args.ip, args.porta), timeout=15)
    print("Titulo:", comando(sock, "getTitleID"), "| Build:", comando(sock, "getBuildID"))

    b = f"0x{args.base:X}"
    raiz = ponteiro(sock, f"{b} 0x0")
    modelo = ponteiro(sock, f"{b} 0x18 0x1B0 0x0")
    assinatura = struct.unpack("<Q", ler_abs(sock, modelo, 8))[0]
    print(f"objeto do save 0x{raiz:X} | assinatura de Pokemon 0x{assinatura:X}")

    # Ponteiros de dados ficam na mesma faixa alta do objeto do save; a assinatura e codigo (outra faixa)
    faixa = raiz >> 36

    def e_dado(v):
        return v % 8 == 0 and (v >> 36) == faixa and v != 0

    fila = deque([(raiz, [])])
    vistos = {raiz}
    visitados = 0
    achados = 0
    t0 = time.time()
    while fila and visitados < args.max:
        end, caminho = fila.popleft()
        dados = ler_abs(sock, end, JANELA)
        visitados += 1
        if struct.unpack_from("<Q", dados, 0)[0] == assinatura and caminho:
            texto, vazio = descrever(sock, end)
            cadeia = " ".join(f"0x{o:X}" for o in caminho)
            if not vazio:
                achados += 1
            print(f"  {'POKEMON' if not vazio else 'vazio  '} [{b} {cadeia}] -> {texto}")
            continue                                   # nao expande dentro de um Pokemon
        if len(caminho) >= args.niveis:
            continue
        for off in range(0, JANELA, 8):
            v = struct.unpack_from("<Q", dados, off)[0]
            if e_dado(v) and v not in vistos:
                vistos.add(v)
                fila.append((v, caminho + [off]))
        if visitados % 500 == 0:
            print(f"    {visitados} objetos visitados, fila {len(fila)}, nivel {len(caminho)}, "
                  f"{achados} Pokemon, {time.time() - t0:.0f} s")

    print(f"Fim: {visitados} objetos visitados, {achados} Pokemon com dados.")
    if fila:
        print("(parou no limite; aumente com --max para ir mais fundo)")
    return 0 if achados else 1


if __name__ == "__main__":
    sys.exit(main())
