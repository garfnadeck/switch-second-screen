#!/usr/bin/env python3
"""Acha a base dos dados do save no Legends Z-A pelo nome do treinador (so leitura).

O bloco do treinador nao e cifrado: cadeia [BASE, 0xA0, 0x40], nome em UTF-16 no deslocamento 0x10.
Para cada BASE que devolver um nome legivel, testa tambem as cadeias do time e da caixa 1
e diz se o bloco e um Pokemon valido (soma de verificacao).

Uso:  python3 tools/za_scan_base.py --ip 192.168.15.9
"""
import argparse
import socket
import struct
import sys

from za_pa9 import TAM_CAIXA, decifrar, especie
from za_scan_pos import comando, ler_main

CADEIA_TREINADOR = "0xA0 0x40"
CADEIAS_POKEMON = {
    "time":    "0x18 0x1B0 0xF0 0x50 0x30 0x0",
    "caixa 1": "0xB0 0x978 0x0",
}


def peek(sock, base, cadeia, tamanho):
    resp = comando(sock, f"pointerPeek {tamanho} 0x{base:X} {cadeia}")
    try:
        bruto = bytes.fromhex(resp)
    except ValueError:
        return None
    return bruto if len(bruto) == tamanho else None


def nome_treinador(bloco):
    """Nome em UTF-16 (0x10, ate 13 letras). Devolve '' se nao parecer um nome."""
    if bloco is None or bloco[0x05] > 1 or not (1 <= bloco[0x07] <= 12):
        return ""
    letras = []
    for i in range(0x10, 0x10 + 0x1A, 2):
        c = struct.unpack_from("<H", bloco, i)[0]
        if c == 0:
            break
        if c < 0x20 or 0xD800 <= c <= 0xDFFF:
            return ""
        letras.append(chr(c))
    return "".join(letras) if len(letras) >= 2 else ""


def testar_pokemon(sock, base):
    for rotulo, cadeia in CADEIAS_POKEMON.items():
        bruto = peek(sock, base, cadeia, TAM_CAIXA)
        if bruto is None or bruto == b"\0" * TAM_CAIXA:
            print(f"      {rotulo}: cadeia nao resolve (zeros)")
            continue
        claro, soma_ok = decifrar(bruto)
        if soma_ok and especie(claro) != 0:
            hp = struct.unpack_from("<H", claro, 0x8A)[0]
            print(f"      {rotulo}: POKEMON VALIDO, especie interna {especie(claro)}, HP atual {hp}")
        elif soma_ok:
            print(f"      {rotulo}: espaco vazio valido (cadeia certa, sem Pokemon)")
        else:
            print(f"      {rotulo}: tem dados, mas a soma nao bate. Inicio: {bruto[:16].hex()}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ip", required=True)
    ap.add_argument("--porta", type=int, default=6000)
    ap.add_argument("--centro", type=lambda t: int(t, 0), default=0x5F0B250 - 0x2620)
    ap.add_argument("--raio", type=lambda t: int(t, 0), default=0x40000)
    args = ap.parse_args()

    sock = socket.create_connection((args.ip, args.porta), timeout=10)
    print("Titulo:", comando(sock, "getTitleID"), "| Build:", comando(sock, "getBuildID"))

    inicio = (args.centro - args.raio) & ~0x7
    tamanho = args.raio * 2
    print(f"Lendo main+0x{inicio:X} .. main+0x{inicio + tamanho:X} ...")
    dados = ler_main(sock, inicio, tamanho)

    bases = []
    for i in range(0, len(dados) - 7, 8):
        v = struct.unpack_from("<Q", dados, i)[0]
        if (1 << 32) <= v < (1 << 39) and v % 8 == 0:
            bases.append(inicio + i)
    print(f"{len(bases)} ponteiros para testar...")

    achados = 0
    for n, base in enumerate(bases, 1):
        nome = nome_treinador(peek(sock, base, CADEIA_TREINADOR, 0x30))
        if nome:
            achados += 1
            print(f"  BASE 0x{base:07X}: treinador '{nome}'")
            testar_pokemon(sock, base)
        if n % 1000 == 0:
            print(f"  {n}/{len(bases)} testados, {achados} com nome")

    if achados == 0:
        print("Nenhuma base com nome de treinador. A cadeia do treinador deve ser outra nesta versao.")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
