#!/usr/bin/env python3
"""Procura o time nos blocos de dados do save do Legends Z-A, em qualquer alinhamento (so leitura).

Os blocos do save (caixas, treinador...) ficam em areas vizinhas na memoria. O script le 4 MB em
volta dos dados das caixas e testa TODAS as posicoes (a busca antiga so testava multiplos de 8).
Para cada Pokemon achado, mostra a distancia ate cada ponteiro fixo do save, que vira a cadeia.

Uso:  python3 tools/za_scan_save.py --ip 192.168.15.9
"""
import argparse
import socket
import struct
import sys
import time

from za_cadeias import listar_blocos
from za_scan_heap import ler_abs, parece_ponteiro, ponteiro
from za_scan_pos import comando

BASE_SAVE = 0x5F0B250
# ponteiros do save para areas de dados (vistos no mapa de regioes): cadeia ate o ponteiro
RAIZES = {
    "0x0": "save+0x000",
    "0x70": "save+0x070",
    "0xB0 0x978": "caixas",
    "0xC8 0x40": "save+0x0C8,+0x40",
    "0xD0": "save+0x0D0",
    "0x100": "save+0x100",
    "0x38 0x1C0": "save+0x038,+0x1C0",
    "0x38 0x2C0": "save+0x038,+0x2C0",
    "0x110 0xD0": "save+0x110,+0xD0",
}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ip", required=True)
    ap.add_argument("--porta", type=int, default=6000)
    ap.add_argument("--raio", type=lambda t: int(t, 0), default=0x200000)
    args = ap.parse_args()

    sock = socket.create_connection((args.ip, args.porta), timeout=15)
    print("Titulo:", comando(sock, "getTitleID"), "| Build:", comando(sock, "getBuildID"))

    b = f"0x{BASE_SAVE:X}"
    raizes = {}
    for cadeia, rotulo in RAIZES.items():
        v = ponteiro(sock, f"{b} {cadeia} 0x0")
        if parece_ponteiro(v & ~0x7):
            raizes[cadeia] = v
            print(f"  [{b} {cadeia}] ({rotulo}) -> 0x{v:X}")
    caixas = raizes.get("0xB0 0x978")
    if not caixas:
        print("Nao achei os dados das caixas; o jogo esta aberto?")
        return 1

    ini = (caixas - args.raio) & ~0xFFF
    fim = (caixas + args.raio) & ~0xFFF
    print(f"Lendo 0x{ini:X} .. 0x{fim:X} ({(fim - ini) / 1048576:.0f} MB)...")
    t0 = time.time()
    dados = b"".join(ler_abs(sock, e, 0x8000) for e in range(ini, fim, 0x8000))
    print(f"  lido em {time.time() - t0:.0f} s. Procurando Pokemon em todas as posicoes...")

    blocos = listar_blocos({"heap_ini": ini}, dados, alinhamento=1)
    if not blocos:
        print("Nenhum bloco de Pokemon valido nessa regiao.")
        return 1
    for end, nome, esp, hp, nivel, modo in blocos:
        print(f"\n  POKEMON em 0x{end:X} ({modo}): '{nome}' especie {esp}, HP {hp}, "
              f"nivel {nivel} (se a extensao for contigua); alinhamento {end % 8}")
        for cadeia, raiz in raizes.items():
            dist = end - raiz
            if 0 <= dist < 0x100000:
                print(f"      = [{b} {cadeia}] + 0x{dist:X}   ->   pointerPeek 344 {b} {cadeia} 0x{dist:X}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
