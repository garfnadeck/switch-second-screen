#!/usr/bin/env python3
"""Procura a base do ponteiro de posicao do jogador no Legends Z-A (so leitura).

ATENCAO: este script parte de uma cadeia ERRADA (junta o ultimo salto com o +0x90 em 0x1C8, pulando
uma etapa). O que ele achou em 10/11/2026 (0x41E9D20) NAO e a posicao do jogador: e um valor da
interface que so muda ao abrir o menu. A cadeia certa e "0x41EC340 0x248 0x0 0x138 0x90"
(ver docs/ponteiros_za.md e tools/za_pos_ao_vivo.py). Mantido so pelas funcoes de leitura.

Ideia: entre versoes do jogo a forma da cadeia se mantem ([[[main+BASE]+0x248]+0x0]+0x138, +0x90)
e so a BASE muda. O script le um trecho do 'main' em volta da base publicada, testa cada
ponteiro encontrado como BASE e guarda os que devolvem tres floats plausiveis. Depois pede
para andar com o personagem e mostra quais mudaram.

Uso:  python3 tools/za_scan_pos.py --ip 192.168.15.9
"""
import argparse
import math
import socket
import struct
import sys

SALTOS = "0x248 0x0 0x1C8"      # 0x1C8 = 0x138 + 0x90 (x, y, z em float32)


def comando(sock, texto):
    """Envia um comando do sys-botbase e devolve a linha de resposta."""
    sock.sendall((texto + "\r\n").encode())
    buf = b""
    while not buf.endswith(b"\n"):
        parte = sock.recv(65536)
        if not parte:
            break
        buf += parte
    return buf.strip().decode(errors="replace")


def ler_main(sock, inicio, tamanho, bloco=0x1000):
    """Le 'tamanho' bytes a partir de main+inicio, em blocos."""
    dados = b""
    for off in range(inicio, inicio + tamanho, bloco):
        n = min(bloco, inicio + tamanho - off)
        resp = comando(sock, f"peekMain 0x{off:X} {n}")
        try:
            pedaco = bytes.fromhex(resp)
        except ValueError:
            pedaco = b""
        dados += pedaco.ljust(n, b"\0")
    return dados


def ler_posicao(sock, base):
    resp = comando(sock, f"pointerPeek 12 0x{base:X} {SALTOS}")
    try:
        bruto = bytes.fromhex(resp)
    except ValueError:
        return None
    if len(bruto) != 12 or bruto == b"\0" * 12:
        return None
    return struct.unpack("<fff", bruto)


def plausivel(pos):
    """Tres floats finitos, de tamanho razoavel, sem lixo de numero minusculo."""
    if pos is None:
        return False
    for v in pos:
        if not math.isfinite(v) or abs(v) > 100000:
            return False
        if v != 0 and abs(v) < 1e-4:
            return False
    return sum(1 for v in pos if abs(v) > 0.01) >= 2


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ip", required=True)
    ap.add_argument("--porta", type=int, default=6000)
    ap.add_argument("--centro", type=lambda t: int(t, 0), default=0x41EC340,
                    help="base publicada (padrao: versao de outubro/2025)")
    ap.add_argument("--raio", type=lambda t: int(t, 0), default=0x20000,
                    help="quanto procurar para cada lado")
    args = ap.parse_args()

    sock = socket.create_connection((args.ip, args.porta), timeout=10)
    print("Titulo:", comando(sock, "getTitleID"), "| Build:", comando(sock, "getBuildID"))

    inicio = (args.centro - args.raio) & ~0x7
    tamanho = args.raio * 2
    print(f"Lendo main+0x{inicio:X} .. main+0x{inicio + tamanho:X} ...")
    dados = ler_main(sock, inicio, tamanho)

    # Candidatos: valores de 8 bytes com cara de endereco (alinhado, entre 2^32 e 2^39)
    bases = []
    for i in range(0, len(dados) - 7, 8):
        v = struct.unpack_from("<Q", dados, i)[0]
        if (1 << 32) <= v < (1 << 39) and v % 8 == 0:
            bases.append(inicio + i)
    print(f"{len(bases)} ponteiros para testar...")

    achados = {}
    for n, base in enumerate(bases, 1):
        pos = ler_posicao(sock, base)
        if plausivel(pos):
            achados[base] = pos
        if n % 500 == 0:
            print(f"  {n}/{len(bases)} testados, {len(achados)} plausiveis")

    if not achados:
        print("Nenhum candidato. Tente um raio maior: --raio 0x80000")
        return 1

    print(f"\n{len(achados)} candidatos. Agora ANDE com o personagem alguns passos.")
    input("Depois de andar, aperte Enter aqui... ")

    print("\nBase          antes (x, y, z)                    depois (x, y, z)")
    mudou = 0
    for base, antes in sorted(achados.items()):
        depois = ler_posicao(sock, base)
        if depois is None:
            continue
        delta = max(abs(a - d) for a, d in zip(antes, depois))
        if 0.05 < delta < 1000:
            mudou += 1
            print(f"0x{base:07X}   {antes[0]:9.2f} {antes[1]:9.2f} {antes[2]:9.2f}"
                  f"    {depois[0]:9.2f} {depois[1]:9.2f} {depois[2]:9.2f}   <- MUDOU")
    if mudou == 0:
        print("Nenhum candidato mudou. Liste todos para conferir:")
        for base, antes in sorted(achados.items())[:40]:
            print(f"0x{base:07X}   {antes[0]:9.2f} {antes[1]:9.2f} {antes[2]:9.2f}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
