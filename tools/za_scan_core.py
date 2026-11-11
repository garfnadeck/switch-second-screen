#!/usr/bin/env python3
"""Procura blocos de Pokemon (0x148 bytes) no reservatorio de memoria do jogo (so leitura).

No Legends Z-A cada Pokemon vive em duas areas separadas: bloco (0x148) e extensao (0x10).
Os blocos saem todos do mesmo reservatorio, cujo inicio esta anotado no objeto [BASE, 0x18].
Fase 1: varre o reservatorio e lista os blocos validos (com apelido e HP).
Fase 2: para cada bloco achado, procura quem aponta para ele (para montar o caminho).

Uso:  python3 tools/za_scan_core.py --ip 192.168.15.9
"""
import argparse
import socket
import struct
import sys
import time

from za_pa9 import TAM_CAIXA, especie, validar
from za_scan_heap import ler_abs, parece_ponteiro, ponteiro, unir
from za_scan_pos import comando

BLOCO = 0x8000


def apelido(claro):
    texto = claro[0x58:0x58 + 26].decode("utf-16-le", errors="replace").split("\0")[0]
    return texto if texto.isprintable() else ""


def varrer(sock, ini, fim, visitar, folga):
    """Le [ini, fim) em pedacos e chama visitar(endereco_do_pedaco, dados) com 'folga' de sobreposicao."""
    t0 = time.time()
    for end in range(ini, fim, BLOCO):
        visitar(end, ler_abs(sock, end, BLOCO + folga))
        feito = end + BLOCO - ini
        if feito % 0x200000 == 0:
            print(f"    {feito / 1048576:.0f}/{(fim - ini) / 1048576:.0f} MB, {time.time() - t0:.0f} s")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ip", required=True)
    ap.add_argument("--porta", type=int, default=6000)
    ap.add_argument("--base", type=lambda t: int(t, 0), default=0x5F0B250)
    ap.add_argument("--tamanho", type=lambda t: int(t, 0), default=0x600000,
                    help="quanto do reservatorio varrer (padrao 6 MB)")
    args = ap.parse_args()

    sock = socket.create_connection((args.ip, args.porta), timeout=15)
    print("Titulo:", comando(sock, "getTitleID"), "| Build:", comando(sock, "getBuildID"))

    b = f"0x{args.base:X}"
    reservatorio = ponteiro(sock, f"{b} 0x18 0x8 0x0")
    vazio = ponteiro(sock, f"{b} 0x18 0xD0 0x0")
    objeto1 = ponteiro(sock, f"{b} 0x18 0x0")
    objeto2 = ponteiro(sock, f"{b} 0x18 0x1B0 0x0")
    print(f"reservatorio 0x{reservatorio:X} | bloco vazio conhecido 0x{vazio:X}")
    if not parece_ponteiro(reservatorio & ~0x7):
        print("Endereco do reservatorio invalido.")
        return 1

    # ---- Fase 1: blocos validos no reservatorio
    ini = reservatorio & ~0xFFF
    fim = ini + args.tamanho
    print(f"Fase 1: varrendo 0x{ini:X} .. 0x{fim:X}")
    achados = {}

    def fase1(end, dados):
        for off in range(0, BLOCO, 4):
            # filtro rapido: chave nao nula e os 2 bytes de sanidade zerados
            if dados[off + 4] or dados[off + 5] or dados[off:off + 4] == b"\0\0\0\0":
                continue
            claro, modo = validar(dados[off:off + TAM_CAIXA])
            if claro is None:
                continue
            hp = struct.unpack_from("<H", claro, 0x8A)[0]
            nome = apelido(claro)
            if hp > 999 or not nome:
                continue                                  # coincidencia de soma
            achados[end + off] = (especie(claro), nome, hp, modo)
            print(f"  BLOCO em 0x{end + off:X} ({modo}): especie interna {especie(claro)}, "
                  f"apelido '{nome}', HP atual {hp} | {end + off - vazio:+#x} do bloco vazio")

    varrer(sock, ini, fim, fase1, TAM_CAIXA)
    if not achados:
        print("Nenhum bloco valido no reservatorio. Tente: --tamanho 0x1000000")
        return 1

    # ---- Fase 2: quem aponta para cada bloco
    print(f"\nFase 2: procurando ponteiros para {len(achados)} bloco(s)...")
    padroes = {struct.pack("<Q", end): end for end in achados}
    faixas = unir([(ini, fim)] + [((a - 0x400000) & ~0xFFF, (a + 0x400000) & ~0xFFF)
                                  for a in (objeto1, objeto2) if parece_ponteiro(a)])

    def fase2(end, dados):
        for padrao, alvo in padroes.items():
            pos = dados.find(padrao, 0, BLOCO)
            while pos != -1:
                if pos % 8 == 0:
                    onde = end + pos
                    antes = dados[max(pos - 0x10, 0):pos].hex()
                    print(f"  ponteiro para 0x{alvo:X} ('{achados[alvo][1]}') em 0x{onde:X}"
                          f" | 0x10 bytes antes: {antes}")
                pos = dados.find(padrao, pos + 1, BLOCO)

    for f_ini, f_fim in faixas:
        print(f"  faixa 0x{f_ini:X} .. 0x{f_fim:X}")
        varrer(sock, f_ini, f_fim, fase2, 8)
    return 0


if __name__ == "__main__":
    sys.exit(main())
