#!/usr/bin/env python3
"""Mostra o conteudo de um objeto da memoria do Legends Z-A, campo por campo (so leitura).

Segue uma cadeia de ponteiros a partir da base do save e imprime cada campo de 8 bytes nao
nulo, marcando os que parecem enderecos. Serve para enxergar a estrutura em vez de adivinhar.

Uso:  python3 tools/za_dump.py --ip 192.168.15.9                    (objetos dos elos 1 e 2 do time)
      python3 tools/za_dump.py --ip 192.168.15.9 --cadeia "0x18"    (um objeto especifico)
"""
import argparse
import socket
import struct
import sys

from za_scan_pos import comando


def parece_ponteiro(v):
    return (1 << 32) <= v < (1 << 39) and v % 8 == 0


def dump(sock, base, cadeia, tamanho):
    alvo = int(comando(sock, f"pointerAll 0x{base:X} {cadeia} 0x0".replace("  ", " ")), 16)
    print(f"\n=== [0x{base:X} {cadeia}] -> objeto em 0x{alvo:X} ===")
    if not parece_ponteiro(alvo):
        print("  (nao resolve)")
        return
    dados = bytes.fromhex(comando(sock, f"peekAbsolute 0x{alvo:X} {tamanho}")).ljust(tamanho, b"\0")
    for off in range(0, tamanho, 8):
        v = struct.unpack_from("<Q", dados, off)[0]
        if v == 0:
            continue
        if parece_ponteiro(v):
            nota = f"ponteiro ({v - alvo:+#x} do objeto)" if abs(v - alvo) < 0x100000 else "ponteiro"
        else:
            baixo, alto = v & 0xFFFFFFFF, v >> 32
            f1 = struct.unpack("<f", struct.pack("<I", baixo))[0]
            nota = f"u32 {baixo} / {alto}" + (f", float {f1:.3f}" if 1e-3 < abs(f1) < 1e6 else "")
        print(f"  +0x{off:03X}: {v:016X}  {nota}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ip", required=True)
    ap.add_argument("--porta", type=int, default=6000)
    ap.add_argument("--base", type=lambda t: int(t, 0), default=0x5F0B250)
    ap.add_argument("--cadeia", action="append",
                    help='saltos ate o objeto, ex.: "0x18 0x1B0" (pode repetir)')
    ap.add_argument("--tam", type=lambda t: int(t, 0), default=0x300)
    args = ap.parse_args()

    sock = socket.create_connection((args.ip, args.porta), timeout=10)
    print("Titulo:", comando(sock, "getTitleID"), "| Build:", comando(sock, "getBuildID"))
    for cadeia in (args.cadeia or ["0x18", "0x18 0x1B0"]):
        dump(sock, args.base, cadeia, args.tam)
    return 0


if __name__ == "__main__":
    sys.exit(main())
