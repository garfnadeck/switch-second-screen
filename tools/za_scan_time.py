#!/usr/bin/env python3
"""Acha a cadeia do time no Legends Z-A a partir da base do save ja conhecida (so leitura).

Cadeia publicada (1.0.2): [BASE, 0x18, 0x1B0, 0xF0, 0x50, 0x30, 0x0]. Em outra versao, algum salto
pode ter mudado. O script varia um salto por vez (depois dois) em volta do valor publicado e
aceita o bloco quando a soma de verificacao do Pokemon confere.

Uso:  python3 tools/za_scan_time.py --ip 192.168.15.9
"""
import argparse
import itertools
import socket
import struct
import sys

from za_pa9 import decifrar, especie
from za_scan_pos import comando

SALTOS = [0x18, 0x1B0, 0xF0, 0x50, 0x30]
TAM_TIME = 0x158                    # bloco de caixa (0x148) + extensao de time (nivel, HP maximo)


def testar(sock, base, saltos):
    cadeia = " ".join(f"0x{s:X}" for s in saltos) + " 0x0"
    resp = comando(sock, f"pointerPeek {TAM_TIME} 0x{base:X} {cadeia}")
    try:
        bruto = bytes.fromhex(resp)
    except ValueError:
        return None
    if len(bruto) != TAM_TIME or bruto[:8] == b"\0" * 8:
        return None
    claro, soma_ok = decifrar(bruto)
    if not soma_ok or especie(claro) == 0:
        return None
    return {
        "cadeia": cadeia,
        "especie": especie(claro),
        "hp": struct.unpack_from("<H", claro, 0x8A)[0],
        "nivel": claro[0x148],
        "hp_max": struct.unpack_from("<H", claro, 0x14A)[0],
    }


def variacoes(niveis, alcance):
    """Gera cadeias variando os saltos dos 'niveis' indicados dentro de +-alcance (passo 8)."""
    deltas = range(-alcance, alcance + 8, 8)
    for combinacao in itertools.product(deltas, repeat=len(niveis)):
        if all(d == 0 for d in combinacao):
            continue
        saltos = list(SALTOS)
        for nivel, d in zip(niveis, combinacao):
            saltos[nivel] += d
        if min(saltos) >= 0:
            yield saltos


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ip", required=True)
    ap.add_argument("--porta", type=int, default=6000)
    ap.add_argument("--base", type=lambda t: int(t, 0), default=0x5F0B250)
    args = ap.parse_args()

    sock = socket.create_connection((args.ip, args.porta), timeout=10)
    print("Titulo:", comando(sock, "getTitleID"), "| Build:", comando(sock, "getBuildID"))

    achados = []

    def rodar(rotulo, gerador):
        n = 0
        for saltos in gerador:
            n += 1
            r = testar(sock, args.base, saltos)
            if r:
                achados.append(r)
                print(f"  ACHEI: [0x{args.base:X} {r['cadeia']}] especie interna {r['especie']}, "
                      f"nivel {r['nivel']}, HP {r['hp']}/{r['hp_max']}")
        print(f"{rotulo}: {n} cadeias testadas, {len(achados)} validas ate aqui")

    r = testar(sock, args.base, SALTOS)
    print("Cadeia publicada:", "VALIDA" if r else "nao resolve")

    for nivel in range(len(SALTOS)):
        rodar(f"Variando o salto {nivel + 1}", variacoes([nivel], 0x100))
    if not achados:
        for par in itertools.combinations(range(len(SALTOS)), 2):
            rodar(f"Variando os saltos {par[0] + 1} e {par[1] + 1}", variacoes(list(par), 0x40))

    if not achados:
        print("Nenhuma cadeia valida. A estrutura do time e outra nesta versao.")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
