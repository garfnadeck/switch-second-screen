#!/usr/bin/env python3
"""Le os 6 Pokemon do time no Legends Z-A 2.0.2 (so leitura).

Na versao 2.0.2 (base 0x610A710), o objeto [BASE, 0x18, 0x1B0] guarda os 6 ponteiros dos slots
do time em +0xF0, +0xF8, +0x100, +0x108, +0x110 e +0x118. Em cada slot (+0x50), o ponteiro do
bloco principal cifrado PA9 (0x148 bytes) fica em +0x30 e o da extensao de time (0x10 bytes:
nivel e HP maximo) fica em +0x50.

Uso:  python3 tools/za_ler_pokemon.py --ip 192.168.15.5
"""
import argparse
import socket
import struct
import sys

from za_pa9 import TAM_CAIXA, especie, validar
from za_scan_pos import comando

SLOTS = [0xF0 + i * 8 for i in range(6)]
OFF_REGISTRO = 0x50
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
    ap.add_argument("--base", type=lambda t: int(t, 0), default=0x610A710)
    args = ap.parse_args()

    sock = socket.create_connection((args.ip, args.porta), timeout=10)
    print("Titulo:", comando(sock, "getTitleID"), "| Build:", comando(sock, "getBuildID"))

    achou = False
    for idx, off_slot in enumerate(SLOTS, 1):
        cad_bloco = f"0x18 0x1B0 0x{off_slot:X} 0x{OFF_REGISTRO:X} 0x{OFF_BLOCO:X} 0x0"
        cad_extra = f"0x18 0x1B0 0x{off_slot:X} 0x{OFF_REGISTRO:X} 0x{OFF_EXTRA:X} 0x0"
        bloco = peek(sock, args.base, cad_bloco, TAM_CAIXA)
        extra = peek(sock, args.base, cad_extra, 0x10)
        print(f"\nSlot {idx} (+0x{off_slot:X}): bloco [{cad_bloco}], extensao [{cad_extra}]")
        if bloco is None or extra is None:
            print("  nao resolve (slot vazio)")
            continue
        print(f"  bloco comeca com {bloco[:16].hex()} | extensao {extra.hex()}")
        claro, modo = validar(bloco + extra)
        if claro is None:
            print("  vazio ou soma nao bate")
            continue
        achou = True
        apelido = claro[0x58:0x58 + 26].decode("utf-16-le", errors="replace").split("\0")[0]
        ot = claro[0xF8:0xF8 + 26].decode("utf-16-le", errors="replace").split("\0")[0]
        print(f"  POKEMON VALIDO ({modo}): especie interna {especie(claro)}, apelido '{apelido}', OT '{ot}'")
        print(f"  nivel {claro[0x148]}, HP {struct.unpack_from('<H', claro, 0x8A)[0]}/"
              f"{struct.unpack_from('<H', claro, 0x14A)[0]}, "
              f"status {struct.unpack_from('<I', claro, 0x90)[0]}")
    return 0 if achou else 1


if __name__ == "__main__":
    sys.exit(main())
