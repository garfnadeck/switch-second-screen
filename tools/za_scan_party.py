#!/usr/bin/env python3
"""Procura a base do ponteiro do time no Legends Z-A (so leitura).

Cadeia publicada: [BASE, 0x18, 0x1B0, 0xF0, 0x50, 0x30, 0x0] -> bloco de 0x148 bytes do 1o Pokemon.
Testa cada ponteiro em volta da base publicada e aceita so os blocos cuja soma de verificacao bate.

Uso:  python3 tools/za_scan_party.py --ip 192.168.15.9
"""
import argparse
import socket
import struct
import sys

from za_pa9 import TAM_CAIXA, decifrar, especie
from za_scan_pos import comando, ler_main

SALTOS = "0x18 0x1B0 0xF0 0x50 0x30 0x0"


def ler_bloco(sock, base, tamanho):
    resp = comando(sock, f"pointerPeek {tamanho} 0x{base:X} {SALTOS}")
    try:
        bruto = bytes.fromhex(resp)
    except ValueError:
        return None
    return bruto if len(bruto) == tamanho else None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ip", required=True)
    ap.add_argument("--porta", type=int, default=6000)
    ap.add_argument("--centro", type=lambda t: int(t, 0), default=0x5F0B250 - 0x2620,
                    help="base esperada (publicada menos o deslocamento achado na posicao)")
    ap.add_argument("--raio", type=lambda t: int(t, 0), default=0x10000)
    args = ap.parse_args()

    sock = socket.create_connection((args.ip, args.porta), timeout=10)
    print("Titulo:", comando(sock, "getTitleID"), "| Build:", comando(sock, "getBuildID"))

    inicio = (args.centro - args.raio) & ~0x7
    tamanho = args.raio * 2
    print(f"Lendo main+0x{inicio:X} .. main+0x{inicio + tamanho:X} ...")
    dados = ler_main(sock, inicio, tamanho)

    bases = []
    for i in range(0, len(dados) - 7, 8):
        v = struct.unpack_from("<Q", dados, i)[0]
        if (1 << 32) <= v < (1 << 39) and v % 8 == 0:
            bases.append(inicio + i)
    print(f"{len(bases)} ponteiros para testar...")

    achados = 0
    for n, base in enumerate(bases, 1):
        cabeca = ler_bloco(sock, base, 8)                 # leitura curta primeiro: descarta rapido
        if cabeca and cabeca != b"\0" * 8:
            bruto = ler_bloco(sock, base, TAM_CAIXA)
            if bruto:
                claro, soma_ok = decifrar(bruto)
                if soma_ok and especie(claro) != 0:
                    achados += 1
                    print(f"  ACHEI base 0x{base:07X}: especie interna {especie(claro)}, "
                          f"chave 0x{struct.unpack_from('<I', bruto, 0)[0]:08X}")
        if n % 500 == 0:
            print(f"  {n}/{len(bases)} testados, {achados} validos")

    if achados == 0:
        print("Nenhum bloco valido. Tente: --raio 0x40000, ou confira se ha Pokemon no time.")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
