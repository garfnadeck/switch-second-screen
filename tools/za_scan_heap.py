#!/usr/bin/env python3
"""Procura Pokemon validos na memoria, perto dos dados do save ja conhecidos (so leitura).

Nao depende de cadeia de ponteiros: le a memoria em volta do objeto do save, do bloco do
treinador e do bloco da caixa, e testa cada posicao com a soma de verificacao do Pokemon
(aceita bloco cifrado ou aberto). Mostra o endereco de cada um e a distancia ate as ancoras.

Uso:  python3 tools/za_scan_heap.py --ip 192.168.15.9
"""
import argparse
import socket
import struct
import sys
import time

from za_pa9 import especie, validar
from za_scan_pos import comando

TAM_TIME = 0x158
BLOCO = 0x2000


def parece_ponteiro(v):
    return (1 << 32) <= v < (1 << 39) and v % 8 == 0


def ler_abs(sock, endereco, tamanho):
    try:
        dados = bytes.fromhex(comando(sock, f"peekAbsolute 0x{endereco:X} {tamanho}"))
    except ValueError:
        dados = b""
    return dados.ljust(tamanho, b"\0")[:tamanho]


def ponteiro(sock, cadeia):
    try:
        return int(comando(sock, f"pointerAll {cadeia}"), 16)
    except ValueError:
        return 0


def unir(faixas):
    """Junta faixas (inicio, fim) que se sobrepoem."""
    saida = []
    for ini, fim in sorted(faixas):
        if saida and ini <= saida[-1][1]:
            saida[-1] = (saida[-1][0], max(saida[-1][1], fim))
        else:
            saida.append((ini, fim))
    return saida


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ip", required=True)
    ap.add_argument("--porta", type=int, default=6000)
    ap.add_argument("--base", type=lambda t: int(t, 0), default=0x5F0B250)
    ap.add_argument("--raio", type=lambda t: int(t, 0), default=0x400000,
                    help="quanto ler para cada lado de cada ancora (padrao 4 MB)")
    args = ap.parse_args()

    sock = socket.create_connection((args.ip, args.porta), timeout=15)
    print("Titulo:", comando(sock, "getTitleID"), "| Build:", comando(sock, "getBuildID"))

    b = f"0x{args.base:X}"
    ancoras = {
        "objeto do save": ponteiro(sock, f"{b} 0x0"),
        "treinador":      ponteiro(sock, f"{b} 0xA0 0x0"),
        "caixa (objeto)": ponteiro(sock, f"{b} 0xB0 0x0"),
        "caixa (dados)":  ponteiro(sock, f"{b} 0xB0 0x978 0x0"),
        "elo 2 do time":  ponteiro(sock, f"{b} 0x18 0x1B0 0x0"),
    }
    for nome, end in ancoras.items():
        print(f"  ancora {nome}: 0x{end:X}")
    validas = {n: e for n, e in ancoras.items() if parece_ponteiro(e)}
    if not validas:
        print("Nenhuma ancora valida.")
        return 1

    faixas = unir([((e - args.raio) & ~0xFFF, (e + args.raio) & ~0xFFF) for e in validas.values()])
    total = sum(f - i for i, f in faixas)
    print(f"Lendo {total / 1048576:.0f} MB em {len(faixas)} faixa(s)...")

    achados = 0
    lido = 0
    inicio_t = time.time()
    sobra = b""
    for ini, fim in faixas:
        sobra = b""
        for end in range(ini, fim, BLOCO):
            dados = sobra + ler_abs(sock, end, BLOCO)
            origem = end - len(sobra)
            limite = len(dados) - TAM_TIME
            for off in range(0, max(limite, 0) + 1, 8):
                # filtro rapido: chave nao nula e os 2 bytes de sanidade zerados
                if dados[off + 4] or dados[off + 5] or dados[off:off + 4] == b"\0\0\0\0":
                    continue
                claro, modo = validar(dados[off:off + TAM_TIME])
                if claro is None:
                    continue
                achados += 1
                onde = origem + off
                perto = min(validas.items(), key=lambda kv: abs(kv[1] - onde))
                print(f"  ACHEI em 0x{onde:X} ({modo}): especie interna {especie(claro)}, "
                      f"nivel {claro[0x148]}, HP {struct.unpack_from('<H', claro, 0x8A)[0]}"
                      f"/{struct.unpack_from('<H', claro, 0x14A)[0]}"
                      f" | {onde - perto[1]:+#x} de '{perto[0]}'")
            sobra = dados[-(TAM_TIME - 8):]
            lido += BLOCO
            if lido % 0x200000 == 0:
                print(f"  {lido / 1048576:.0f}/{total / 1048576:.0f} MB, {achados} achados, "
                      f"{time.time() - inicio_t:.0f} s")

    if achados == 0:
        print("Nenhum Pokemon valido nessa regiao. Tente um raio maior: --raio 0x1000000")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
