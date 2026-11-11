#!/usr/bin/env python3
"""Procura o time do Legends Z-A a partir de dois pontos proximos (so leitura).

A) O bloco grande vizinho dos Pokemon em branco ([BASE, 0x18, 0x220], ~240 KB): le inteiro e
   procura blocos de Pokemon, a assinatura dos objetos de Pokemon e o nome em texto puro.
B) O objeto do personagem ([[[main+0x41EC340]+0x248]+0x0], o mesmo da posicao): anda pelos
   objetos ligados a ele, poucos niveis, procurando objetos de Pokemon pela assinatura.

Uso:  python3 tools/za_busca_time.py --ip 192.168.15.9 --nome Chikorita
"""
import argparse
import socket
import struct
import sys
import time
from collections import deque

from za_bfs_pokemon import descrever
from za_pa9 import TAM_CAIXA, especie, validar
from za_scan_heap import ler_abs, ponteiro
from za_scan_pos import comando

BASE_SAVE = 0x5F0B250
RAIZ_PERSONAGEM = "0x41EC340 0x248 0x0"


def parte_a(sock, assinatura, nome):
    ini, _, _, _, _, _, _, _, fim = struct.unpack("<9Q", ler_abs(sock, ponteiro(sock, f"0x{BASE_SAVE:X} 0x18 0x0") + 0x220, 72))
    tamanho = fim - ini
    print(f"\nA) bloco em 0x{ini:X}, {tamanho} bytes")
    if not (0 < tamanho <= 0x200000):
        print("   tamanho fora do esperado; pulando.")
        return
    dados = b"".join(ler_abs(sock, ini + o, min(0x8000, tamanho - o)) for o in range(0, tamanho, 0x8000))
    naozero = sum(1 for i in range(0, len(dados), 0x1000) if dados[i:i + 0x1000].strip(b"\0"))
    print(f"   {naozero} de {len(dados) // 0x1000 + 1} paginas com dados. Inicio: {dados[:32].hex()}")

    achou = 0
    for off in range(0, len(dados) - TAM_CAIXA, 8):
        if dados[off + 4] or dados[off + 5] or dados[off:off + 4] == b"\0\0\0\0":
            continue
        claro, modo = validar(dados[off:off + 0x158])
        if claro is None or struct.unpack_from("<H", claro, 0x8A)[0] > 999:
            continue
        achou += 1
        apel = claro[0x58:0x58 + 26].decode("utf-16-le", errors="replace").split("\0")[0]
        print(f"   BLOCO em +0x{off:X} ({modo}): '{apel}' especie {especie(claro)}, "
              f"HP {struct.unpack_from('<H', claro, 0x8A)[0]}")
    padrao_ass = struct.pack("<Q", assinatura)
    n_ass = dados.count(padrao_ass)
    pos = dados.find(nome.encode("utf-16-le"))
    print(f"   blocos validos: {achou} | objetos com a assinatura: {n_ass} | "
          f"nome '{nome}' em texto: {'+0x%X' % pos if pos >= 0 else 'nao'}")
    p = dados.find(padrao_ass)
    while p != -1 and p % 8 == 0:
        texto, vazio = descrever(sock, ini + p)
        print(f"   objeto de Pokemon em +0x{p:X}: {texto}")
        p = dados.find(padrao_ass, p + 8)


def parte_b(sock, assinatura, niveis, maximo):
    raiz = ponteiro(sock, f"{RAIZ_PERSONAGEM} 0x0")
    print(f"\nB) objeto do personagem em 0x{raiz:X}")
    if raiz == 0:
        print("   nao resolve.")
        return
    faixa = raiz >> 36
    fila = deque([(raiz, [])])
    vistos = {raiz}
    visitados = achados = 0
    t0 = time.time()
    while fila and visitados < maximo:
        end, caminho = fila.popleft()
        janela = 0x1000 if not caminho else 0x400          # o personagem e um objeto grande
        dados = ler_abs(sock, end, janela)
        visitados += 1
        if struct.unpack_from("<Q", dados, 0)[0] == assinatura and caminho:
            texto, vazio = descrever(sock, end)
            cadeia = " ".join(f"0x{o:X}" for o in caminho)
            achados += 0 if vazio else 1
            print(f"   {'POKEMON' if not vazio else 'vazio  '} [{RAIZ_PERSONAGEM} {cadeia}] -> {texto}")
            continue
        if len(caminho) >= niveis:
            continue
        for off in range(0, janela, 8):
            v = struct.unpack_from("<Q", dados, off)[0]
            if v and v % 8 == 0 and (v >> 36) == faixa and v not in vistos:
                vistos.add(v)
                fila.append((v, caminho + [off]))
        if visitados % 500 == 0:
            print(f"     {visitados} objetos, fila {len(fila)}, nivel {len(caminho)}, {time.time() - t0:.0f} s")
    print(f"   fim: {visitados} objetos visitados, {achados} Pokemon com dados.")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ip", required=True)
    ap.add_argument("--porta", type=int, default=6000)
    ap.add_argument("--nome", default="Chikorita")
    ap.add_argument("--niveis", type=int, default=3)
    ap.add_argument("--max", type=int, default=3000)
    args = ap.parse_args()

    sock = socket.create_connection((args.ip, args.porta), timeout=15)
    print("Titulo:", comando(sock, "getTitleID"), "| Build:", comando(sock, "getBuildID"))
    modelo = ponteiro(sock, f"0x{BASE_SAVE:X} 0x18 0x1B0 0x0")
    assinatura = struct.unpack("<Q", ler_abs(sock, modelo, 8))[0]
    print(f"assinatura de Pokemon 0x{assinatura:X}")
    parte_a(sock, assinatura, args.nome)
    parte_b(sock, assinatura, args.niveis, args.max)
    return 0


if __name__ == "__main__":
    sys.exit(main())
