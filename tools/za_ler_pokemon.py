#!/usr/bin/env python3
"""Le os Pokemon guardados no objeto [BASE, 0x18] do Legends Z-A (so leitura).

Nessa versao o jogo guarda cada Pokemon em duas areas: o bloco principal (0x148 bytes) e a
extensao de time (0x10 bytes: nivel, HP maximo...). O objeto [BASE, 0x18] tem registros de
0xA0 bytes; em cada um, o ponteiro do bloco fica em +0x30 e o da extensao em +0x50.

Uso:  python3 tools/za_ler_pokemon.py --ip 192.168.15.9
"""
import argparse
import socket
import struct
import sys

from za_pa9 import TAM_CAIXA, especie, validar
from za_scan_pos import comando

REGISTROS = [0x0A0, 0x140]      # inicio de cada registro dentro do objeto
OFF_BLOCO = 0x30                # ponteiro do bloco de 0x148 dentro do registro
OFF_EXTRA = 0x50                # ponteiro da extensao de 0x10 dentro do registro


def peek(sock, base, cadeia, tamanho):
    try:
        dados = bytes.fromhex(comando(sock, f"pointerPeek {tamanho} 0x{base:X} {cadeia}"))
    except ValueError:
        dados = b""
    return dados if len(dados) == tamanho else None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ip", required=True)
    ap.add_argument("--porta", type=int, default=6000)
    ap.add_argument("--base", type=lambda t: int(t, 0), default=0x5F0B250)
    args = ap.parse_args()

    sock = socket.create_connection((args.ip, args.porta), timeout=10)
    print("Titulo:", comando(sock, "getTitleID"), "| Build:", comando(sock, "getBuildID"))

    achou = False
    for reg in REGISTROS:
        cad_bloco = f"0x18 0x{reg + OFF_BLOCO:X} 0x0"
        cad_extra = f"0x18 0x{reg + OFF_EXTRA:X} 0x0"
        bloco = peek(sock, args.base, cad_bloco, TAM_CAIXA)
        extra = peek(sock, args.base, cad_extra, 0x10)
        print(f"\nRegistro +0x{reg:X}: bloco [{cad_bloco}], extensao [{cad_extra}]")
        if bloco is None or extra is None:
            print("  nao resolve")
            continue
        print(f"  bloco comeca com {bloco[:16].hex()} | extensao {extra.hex()}")
        claro, modo = validar(bloco + extra)
        if claro is None:
            print("  nao e um Pokemon valido (soma nao bate)")
            continue
        achou = True
        apelido = claro[0x58:0x58 + 26].decode("utf-16-le", errors="replace").split("\0")[0]
        print(f"  POKEMON VALIDO ({modo}): especie interna {especie(claro)}, apelido '{apelido}'")
        print(f"  HP atual {struct.unpack_from('<H', claro, 0x8A)[0]}, "
              f"status {struct.unpack_from('<I', claro, 0x90)[0]}")
        print(f"  extensao em claro: nivel {claro[0x148]}, HP maximo "
              f"{struct.unpack_from('<H', claro, 0x14A)[0]} | bruta: nivel {extra[0]}, "
              f"HP maximo {struct.unpack_from('<H', extra, 2)[0]}")
    return 0 if achou else 1


if __name__ == "__main__":
    sys.exit(main())
