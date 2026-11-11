#!/usr/bin/env python3
"""Copia para o Mac a memoria do Legends Z-A onde ficam os Pokemon, para analisar sem a rede (so leitura).

Grava em uma pasta: heap.bin (a area do alocador dos Pokemon e arredores), main.bin (os dados
estaticos do jogo, de onde partem os ponteiros fixos) e meta.json (enderecos de cada trecho).
Depois rode tools/za_cadeias.py na mesma sessao do jogo: os enderecos mudam a cada vez que o
jogo e aberto.

Uso:  python3 tools/za_dump_mem.py --ip 192.168.15.9
"""
import argparse
import json
import os
import socket
import struct
import sys
import time

from za_scan_heap import ler_abs, ponteiro
from za_scan_pos import comando

BASE_SAVE = 0x5F0B250
MAIN_INI, MAIN_FIM = 0x3C00000, 0x6300000      # trecho de dados do executavel (inclui 0x41EC340 e 0x5F0B250)


def copiar(sock, ini, fim, caminho, bloco, rotulo):
    total = fim - ini
    t0 = time.time()
    with open(caminho, "wb") as f:
        for end in range(ini, fim, bloco):
            f.write(ler_abs(sock, end, min(bloco, fim - end)))
            feito = end + bloco - ini
            if feito % 0x1000000 == 0 or feito >= total:
                seg = time.time() - t0
                resta = seg * max(total - feito, 0) / max(feito, 1)
                print(f"  {rotulo}: {min(feito, total) / 1048576:.0f}/{total / 1048576:.0f} MB, "
                      f"{seg:.0f} s, faltam ~{resta / 60:.0f} min")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ip", required=True)
    ap.add_argument("--porta", type=int, default=6000)
    ap.add_argument("--saida", default="dump_za")
    ap.add_argument("--antes", type=lambda t: int(t, 0), default=0x2C00000,
                    help="quanto copiar antes do alocador (padrao 44 MB)")
    ap.add_argument("--depois", type=lambda t: int(t, 0), default=0x4C00000,
                    help="quanto copiar depois do Pokemon em branco conhecido (padrao 76 MB)")
    args = ap.parse_args()

    sock = socket.create_connection((args.ip, args.porta), timeout=20)
    build = comando(sock, "getBuildID")
    print("Titulo:", comando(sock, "getTitleID"), "| Build:", build)

    b = f"0x{BASE_SAVE:X}"
    alocador = ponteiro(sock, f"{b} 0x18 0x8 0x0")
    modelo = ponteiro(sock, f"{b} 0x18 0x1B0 0x0")
    assinatura = struct.unpack("<Q", ler_abs(sock, modelo, 8))[0]
    base_main = int(comando(sock, "getMainNsoBase"), 16)
    heap_ini = (alocador - args.antes) & ~0xFFFFF
    heap_fim = (modelo + args.depois) & ~0xFFFFF

    bloco = 0x10000
    try:
        if len(bytes.fromhex(comando(sock, f"peekAbsolute 0x{alocador & ~0xFFF:X} {bloco}"))) != bloco:
            bloco = 0x8000
    except ValueError:
        bloco = 0x8000

    os.makedirs(args.saida, exist_ok=True)
    meta = {"build": build, "base_main": base_main, "main_ini": base_main + MAIN_INI,
            "main_fim": base_main + MAIN_FIM, "heap_ini": heap_ini, "heap_fim": heap_fim,
            "assinatura": assinatura, "base_save": BASE_SAVE, "quando": time.strftime("%Y-%m-%d %H:%M:%S")}
    print(f"main 0x{meta['main_ini']:X}..0x{meta['main_fim']:X} ({(MAIN_FIM - MAIN_INI) / 1048576:.0f} MB) | "
          f"heap 0x{heap_ini:X}..0x{heap_fim:X} ({(heap_fim - heap_ini) / 1048576:.0f} MB)")

    copiar(sock, meta["main_ini"], meta["main_fim"], os.path.join(args.saida, "main.bin"), bloco, "main")
    copiar(sock, heap_ini, heap_fim, os.path.join(args.saida, "heap.bin"), bloco, "heap")
    with open(os.path.join(args.saida, "meta.json"), "w") as f:
        json.dump(meta, f, indent=1)
    print(f"Pronto: pasta '{args.saida}'. Agora, SEM fechar o jogo: python3 tools/za_cadeias.py")
    return 0


if __name__ == "__main__":
    sys.exit(main())
