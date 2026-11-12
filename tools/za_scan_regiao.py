#!/usr/bin/env python3
"""Procura Pokemon em uma regiao de memoria do Legends Z-A (so leitura).

Le a regiao inteira para o Mac e procura: (1) blocos de Pokemon validos, em alinhamento de 4;
(2) objetos com a assinatura de Pokemon, decifrando cada um pela rede.
Sem --ini/--fim, usa a regiao dos objetos "vivos": os 32 MB em volta do ponteiro [BASE, 0x240].

Uso:  python3 tools/za_scan_regiao.py --ip 192.168.15.11
      python3 tools/za_scan_regiao.py --ip 192.168.15.11 --ini 0x5CA7000000 --fim 0x5CB1000000
"""
import argparse
import socket
import struct
import sys
import time

from za_bfs_pokemon import descrever
from za_cadeias import listar_blocos
from za_scan_heap import ler_abs, parece_ponteiro, ponteiro
from za_scan_pos import comando

BASE_SAVE = 0x5F0B250


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ip", required=True)
    ap.add_argument("--porta", type=int, default=6000)
    ap.add_argument("--ini", type=lambda t: int(t, 0))
    ap.add_argument("--fim", type=lambda t: int(t, 0))
    ap.add_argument("--nome", default="Chikorita")
    args = ap.parse_args()

    sock = socket.create_connection((args.ip, args.porta), timeout=20)
    print("Titulo:", comando(sock, "getTitleID"), "| Build:", comando(sock, "getBuildID"))
    b = f"0x{BASE_SAVE:X}"
    modelo = ponteiro(sock, f"{b} 0x18 0x1B0 0x0")
    assinatura = struct.unpack("<Q", ler_abs(sock, modelo, 8))[0]

    if args.ini is None or args.fim is None:
        ref = ponteiro(sock, f"{b} 0x240 0x0")
        if not parece_ponteiro(ref & ~0x7):
            print("Nao consegui achar a regiao padrao; passe --ini e --fim.")
            return 1
        args.ini = ref & ~0xFFFFFF
        args.fim = args.ini + 0x2000000
        print(f"referencia [{b} 0x240] = 0x{ref:X}")
    total = args.fim - args.ini
    print(f"Lendo 0x{args.ini:X} .. 0x{args.fim:X} ({total / 1048576:.0f} MB)...")

    bloco = 0x10000
    t0 = time.time()
    partes = []
    for end in range(args.ini, args.fim, bloco):
        partes.append(ler_abs(sock, end, min(bloco, args.fim - end)))
        feito = end + bloco - args.ini
        if feito % 0x800000 == 0:
            print(f"  {feito / 1048576:.0f}/{total / 1048576:.0f} MB, {time.time() - t0:.0f} s")
    dados = b"".join(partes)
    zeradas = sum(1 for i in range(0, len(dados), 0x1000) if dados[i:i + 0x1000].count(0) == 0x1000)
    print(f"  lido em {time.time() - t0:.0f} s; {100 * zeradas // max(len(dados) // 0x1000, 1)}% das paginas zeradas")

    print("Blocos de Pokemon validos:")
    blocos = listar_blocos({"heap_ini": args.ini}, dados)
    for end, nome, esp, hp, nivel, modo in blocos:
        marca = "   <== " + args.nome.upper() if nome.lower() == args.nome.lower() else ""
        print(f"  bloco 0x{end:X} ({modo}): '{nome}' especie {esp}, HP {hp}{marca}")
    if not blocos:
        print("  nenhum")

    padrao = struct.pack("<Q", assinatura)
    print("Objetos com a assinatura de Pokemon:")
    com_dados = vazios = 0
    pos = dados.find(padrao)
    while pos != -1:
        if pos % 8 == 0:
            texto, vazio = descrever(sock, args.ini + pos)
            if vazio:
                vazios += 1
            else:
                com_dados += 1
                print(f"  objeto 0x{args.ini + pos:X}: {texto}")
        pos = dados.find(padrao, pos + 1)
    print(f"  {com_dados} com dados, {vazios} em branco")
    return 0 if (blocos or com_dados) else 1


if __name__ == "__main__":
    sys.exit(main())
