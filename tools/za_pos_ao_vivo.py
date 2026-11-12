#!/usr/bin/env python3
"""Mostra a posicao do jogador ao vivo no terminal (so leitura).

Le o mesmo ponteiro que a placa usa, varias vezes por segundo, e imprime uma linha a cada
mudanca. Serve para conferir se o valor acompanha o personagem. Ctrl+C para sair.

Uso:  python3 tools/za_pos_ao_vivo.py --ip 192.168.15.9
"""
import argparse
import socket
import struct
import sys
import time

from za_scan_pos import comando

CADEIA = "0x47D71A0 0x248 0x0 0x138 0x90"   # a mesma do include/providers/za_memory_map.h (build 2.0.2)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ip", required=True)
    ap.add_argument("--porta", type=int, default=6000)
    ap.add_argument("--cadeia", default=CADEIA)
    ap.add_argument("--intervalo", type=float, default=0.2)
    args = ap.parse_args()

    sock = socket.create_connection((args.ip, args.porta), timeout=5)
    print("Titulo:", comando(sock, "getTitleID"), "| Build:", comando(sock, "getBuildID"))
    print("Ande com o personagem. Uma linha por mudanca; Ctrl+C para sair.\n")

    anterior = None
    leituras = 0
    inicio = time.time()
    try:
        while True:
            bruto = bytes.fromhex(comando(sock, f"pointerPeek 12 {args.cadeia}"))
            leituras += 1
            if len(bruto) == 12:
                pos = struct.unpack("<fff", bruto)
                if anterior is None or max(abs(a - b) for a, b in zip(pos, anterior)) > 0.01:
                    print(f"{time.time() - inicio:7.1f} s  x {pos[0]:9.2f}  y {pos[1]:9.2f}  z {pos[2]:9.2f}"
                          f"   (leitura {leituras})")
                    anterior = pos
            elif leituras % 10 == 1:
                print(f"resposta inesperada: {bruto.hex()!r}")
            if leituras % 25 == 0 and anterior is not None:
                print(f"        ... {leituras} leituras, ultima mudanca acima", end="\r")
            time.sleep(args.intervalo)
    except KeyboardInterrupt:
        print(f"\n{leituras} leituras em {time.time() - inicio:.0f} s.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
