#!/usr/bin/env python3
"""Varre a area de memoria onde o jogo aloca os Pokemon, procurando objetos de Pokemon (so leitura).

Cada Pokemon na memoria e um objeto [assinatura][ponteiro da extensao][ponteiro do bloco]. Os dois
Pokemon em branco conhecidos, seus blocos e suas extensoes saem todos do mesmo alocador, que cobre
de ~0x...8C00000 ate ~0x...F34B0000 (cerca de 180 MB). O script le essa faixa inteira procurando a
assinatura e mostra cada Pokemon com dados. Demora (dezenas de MB por minuto): deixe rodando.

Uso:  python3 tools/za_scan_assinatura.py --ip 192.168.15.9
"""
import argparse
import socket
import struct
import sys
import time

from za_bfs_pokemon import descrever
from za_scan_heap import ler_abs, ponteiro
from za_scan_pos import comando

BASE_SAVE = 0x5F0B250


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ip", required=True)
    ap.add_argument("--porta", type=int, default=6000)
    ap.add_argument("--antes", type=lambda t: int(t, 0), default=0x400000,
                    help="quanto ler antes do inicio do alocador")
    ap.add_argument("--depois", type=lambda t: int(t, 0), default=0x1000000,
                    help="quanto ler depois do ultimo objeto conhecido")
    ap.add_argument("--bloco", type=lambda t: int(t, 0), default=0x10000)
    args = ap.parse_args()

    sock = socket.create_connection((args.ip, args.porta), timeout=20)
    print("Titulo:", comando(sock, "getTitleID"), "| Build:", comando(sock, "getBuildID"))

    b = f"0x{BASE_SAVE:X}"
    alocador = ponteiro(sock, f"{b} 0x18 0x8 0x0")
    modelo = ponteiro(sock, f"{b} 0x18 0x1B0 0x0")
    assinatura = struct.unpack("<Q", ler_abs(sock, modelo, 8))[0]
    try:
        base_main = int(comando(sock, "getMainNsoBase"), 16)
        print(f"assinatura 0x{assinatura:X} (main+0x{assinatura - base_main:X})")
    except ValueError:
        print(f"assinatura 0x{assinatura:X}")

    ini = (alocador & ~0xFFFFF) - args.antes
    fim = ((modelo + args.depois) | 0xFFFF) + 1
    total = fim - ini
    print(f"alocador 0x{alocador:X}; varrendo 0x{ini:X} .. 0x{fim:X} ({total / 1048576:.0f} MB)")

    # o sys-botbase aceita pedidos grandes? se nao, cai para um tamanho menor
    bloco = args.bloco
    try:
        if len(bytes.fromhex(comando(sock, f"peekAbsolute 0x{alocador & ~0xFFF:X} {bloco}"))) != bloco:
            bloco = 0x8000
    except ValueError:
        bloco = 0x8000
    padrao = struct.pack("<Q", assinatura)

    com_dados = vazios = 0
    t0 = time.time()
    for end in range(ini, fim, bloco):
        dados = ler_abs(sock, end, bloco)
        pos = dados.find(padrao)
        while pos != -1:
            if pos % 8 == 0:
                objeto = end + pos
                texto, vazio = descrever(sock, objeto)
                if vazio:
                    vazios += 1
                else:
                    com_dados += 1
                    print(f"  POKEMON em 0x{objeto:X} ({objeto - modelo:+#x} do modelo): {texto}")
            pos = dados.find(padrao, pos + 1)
        feito = end + bloco - ini
        if feito % 0x800000 == 0:
            seg = time.time() - t0
            resta = seg * (total - feito) / feito
            print(f"    {feito / 1048576:.0f}/{total / 1048576:.0f} MB, {seg:.0f} s, faltam ~{resta / 60:.0f} min"
                  f" | com dados {com_dados}, em branco {vazios}")

    print(f"Fim: {com_dados} Pokemon com dados, {vazios} em branco, em {time.time() - t0:.0f} s.")
    return 0 if com_dados else 1


if __name__ == "__main__":
    sys.exit(main())
