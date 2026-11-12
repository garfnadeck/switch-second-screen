#!/usr/bin/env python3
"""Segue a cadeia publicada do time elo por elo e mostra onde ela quebra (so leitura).

Serve para saber se a cadeia depende da tela: rode andando pela cidade e de novo com o
menu de Pokemon aberto, e compare.

Uso:  python3 tools/za_cadeia.py --ip 192.168.15.9
"""
import argparse
import socket
import struct
import sys

from za_pa9 import TAM_CAIXA, especie, validar
from za_scan_pos import comando

SALTOS = [0x18, 0x1B0, 0xF0, 0x50, 0x30]
SALTO_EXTRA = 0x50


def ler_abs(sock, endereco, tamanho):
    try:
        return bytes.fromhex(comando(sock, f"peekAbsolute 0x{endereco:X} {tamanho}"))
    except ValueError:
        return b""


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ip", required=True)
    ap.add_argument("--porta", type=int, default=6000)
    ap.add_argument("--base", type=lambda t: int(t, 0), default=0x610A710)
    args = ap.parse_args()

    sock = socket.create_connection((args.ip, args.porta), timeout=10)
    atual = int(comando(sock, f"pointerAll 0x{args.base:X} 0x0"), 16)
    print(f"objeto do save: 0x{atual:X}")

    registro = 0
    for n, salto in enumerate(SALTOS, 1):
        if n == len(SALTOS):
            registro = atual
        bruto = ler_abs(sock, atual + salto, 8)
        proximo = struct.unpack("<Q", bruto)[0] if len(bruto) == 8 else 0
        print(f"elo {n}: [0x{atual:X} + 0x{salto:X}] = 0x{proximo:X}")
        if not ((1 << 32) <= proximo < (1 << 39)):
            print(f"  -> a cadeia quebra aqui (elo {n}): o valor nao e um endereco.")
            return 1
        atual = proximo

    bruto_extra = ler_abs(sock, registro + SALTO_EXTRA, 8)
    end_extra = struct.unpack("<Q", bruto_extra)[0] if len(bruto_extra) == 8 else 0
    print(f"extensao: [0x{registro:X} + 0x{SALTO_EXTRA:X}] = 0x{end_extra:X}")

    bloco = ler_abs(sock, atual, TAM_CAIXA)
    extra = ler_abs(sock, end_extra, 0x10) if (1 << 32) <= end_extra < (1 << 39) else b"\0" * 0x10
    claro, modo = validar(bloco + extra) if len(bloco) == TAM_CAIXA else (None, None)
    if claro is None:
        print("cadeia resolve, mas o bloco final nao e um Pokemon valido. Inicio:", bloco[:16].hex())
        return 1
    nome = claro[0x58:0x58 + 26].decode("utf-16-le", errors="replace").split("\0")[0]
    print(f"POKEMON VALIDO ({modo}): '{nome}' especie interna {especie(claro)}, nivel {claro[0x148]}, "
          f"HP {struct.unpack_from('<H', claro, 0x8A)[0]}/{struct.unpack_from('<H', claro, 0x14A)[0]}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
