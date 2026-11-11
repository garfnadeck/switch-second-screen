#!/usr/bin/env python3
"""Procura o bloco do time dentro do objeto do save do Legends Z-A (so leitura).

O treinador ([BASE, 0xA0, 0x40]) e a caixa ([BASE, 0xB0, 0x978, 0x0]) saem do mesmo objeto.
O time e outro bloco do save (PKHeX: Party9a, 6 posicoes de 0x1E0 bytes), entao deve estar em
outra posicao desse objeto. Para cada ponteiro do objeto, o script procura um Pokemon valido:
  - direto dentro do bloco apontado            -> cadeia [BASE, a, b]
  - um nivel adiante (ponteiro dentro do bloco) -> cadeia [BASE, a, b, 0x0]

Uso:  python3 tools/za_scan_bloco.py --ip 192.168.15.9
"""
import argparse
import socket
import struct
import sys

from za_pa9 import especie, validar
from za_scan_pos import comando

TAM_TIME = 0x158
JANELA_OBJETO = 0x400       # quanto do objeto do save percorrer
JANELA_BLOCO = 0x1400       # quanto de cada bloco ler


def parece_ponteiro(v):
    return (1 << 32) <= v < (1 << 39) and v % 8 == 0


def ler_abs(sock, endereco, tamanho, bloco=0x1000):
    dados = b""
    for off in range(0, tamanho, bloco):
        n = min(bloco, tamanho - off)
        resp = comando(sock, f"peekAbsolute 0x{endereco + off:X} {n}")
        try:
            pedaco = bytes.fromhex(resp)
        except ValueError:
            pedaco = b""
        dados += pedaco.ljust(n, b"\0")
    return dados


def pokemon(bruto):
    """Devolve um resumo se 'bruto' (0x158 bytes) for um Pokemon valido, senao None."""
    if len(bruto) < TAM_TIME or bruto[:8] == b"\0" * 8:
        return None
    claro, modo = validar(bruto[:TAM_TIME])
    if claro is None:
        return None
    return (f"especie interna {especie(claro)}, nivel {claro[0x148]}, "
            f"HP {struct.unpack_from('<H', claro, 0x8A)[0]}/{struct.unpack_from('<H', claro, 0x14A)[0]} ({modo})")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ip", required=True)
    ap.add_argument("--porta", type=int, default=6000)
    ap.add_argument("--base", type=lambda t: int(t, 0), default=0x5F0B250)
    args = ap.parse_args()

    sock = socket.create_connection((args.ip, args.porta), timeout=10)
    print("Titulo:", comando(sock, "getTitleID"), "| Build:", comando(sock, "getBuildID"))

    resp = comando(sock, f"pointerAll 0x{args.base:X} 0x0")
    objeto = int(resp, 16)
    if not parece_ponteiro(objeto):                       # alguns builds devolvem os bytes invertidos
        objeto = int.from_bytes(bytes.fromhex(resp), "little")
    print(f"Objeto do save em 0x{objeto:X}")
    if not parece_ponteiro(objeto):
        print("Endereco do objeto nao parece valido. Resposta bruta:", resp)
        return 1

    tabela = ler_abs(sock, objeto, JANELA_OBJETO)
    achados = 0
    vistos = set()
    for a in range(0, JANELA_OBJETO, 8):
        bloco_end = struct.unpack_from("<Q", tabela, a)[0]
        if not parece_ponteiro(bloco_end):
            continue
        bloco = ler_abs(sock, bloco_end, JANELA_BLOCO)

        for b in range(0, JANELA_BLOCO - TAM_TIME, 8):    # Pokemon direto dentro do bloco
            r = pokemon(bloco[b:b + TAM_TIME])
            if r:
                achados += 1
                print(f"  ACHEI [0x{args.base:X} 0x{a:X} 0x{b:X}]: {r}")

        for b in range(0, 0x1000, 8):                     # ou um ponteiro adiante
            alvo = struct.unpack_from("<Q", bloco, b)[0]
            if not parece_ponteiro(alvo) or alvo in vistos:
                continue
            vistos.add(alvo)
            r = pokemon(ler_abs(sock, alvo, TAM_TIME))
            if r:
                achados += 1
                print(f"  ACHEI [0x{args.base:X} 0x{a:X} 0x{b:X} 0x0]: {r}")
        print(f"posicao 0x{a:03X} do objeto verificada ({achados} achados ate aqui)")

    if achados == 0:
        print("Nenhum Pokemon valido a ate 2 niveis do objeto do save.")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
