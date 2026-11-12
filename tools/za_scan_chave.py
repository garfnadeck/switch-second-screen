#!/usr/bin/env python3
"""Procura a tabela de blocos do save do Legends Z-A pelas chaves dos blocos (so leitura).

Nos jogos recentes os blocos do save ficam na memoria numa tabela {chave, ponteiro} e os dados
sao cifrados com um xorshift semeado pela chave (PKHeX: SCXorShift32). Chaves do Z-A (PKHeX):
  time 0x3AA1A9AD, caixa 0x0D66012C, treinador 0xE3E89BD1.
O script acha cada chave na memoria, mostra o contexto e, para o time, segue os ponteiros
vizinhos e tenta abrir o bloco (puro e com o xorshift).

Uso:  python3 tools/za_scan_chave.py --ip 192.168.15.9
"""
import argparse
import socket
import struct
import sys
import time

from za_pa9 import especie, validar
from za_scan_heap import ler_abs, parece_ponteiro, ponteiro, unir
from za_scan_pos import comando

CHAVES = {0x3AA1A9AD: "time", 0x0D66012C: "caixa", 0xE3E89BD1: "treinador"}
TAM_TIME = 0x158


def xorshift(chave, dados):
    """Decifra 'dados' com o SCXorShift32 do PKHeX semeado por 'chave'."""
    estado = chave
    for _ in range(bin(chave).count("1")):
        estado ^= (estado << 2) & 0xFFFFFFFF
        estado ^= estado >> 15
        estado ^= (estado << 13) & 0xFFFFFFFF
    saida = bytearray(len(dados))
    cont = 0
    for i, b in enumerate(dados):
        saida[i] = b ^ ((estado >> (cont << 3)) & 0xFF)
        if cont == 3:
            estado ^= (estado << 2) & 0xFFFFFFFF
            estado ^= estado >> 15
            estado ^= (estado << 13) & 0xFFFFFFFF
            cont = 0
        else:
            cont += 1
    return bytes(saida)


def resumo(claro, modo):
    return (f"especie interna {especie(claro)}, nivel {claro[0x148]}, "
            f"HP {struct.unpack_from('<H', claro, 0x8A)[0]}/{struct.unpack_from('<H', claro, 0x14A)[0]} ({modo})")


def tentar_abrir(sock, alvo, chave):
    """Le o bloco apontado e procura um Pokemon nos primeiros bytes, puro ou com xorshift."""
    bruto = ler_abs(sock, alvo, 0x200)
    for rotulo, dados in (("puro", bruto), ("xorshift", xorshift(chave, bruto))):
        for off in range(0, 0x20):
            claro, modo = validar(dados[off:off + TAM_TIME])
            if claro is not None and claro[0x148] <= 100:
                print(f"        >>> POKEMON em 0x{alvo:X}+0x{off:X} ({rotulo}): {resumo(claro, modo)}")
                return True
    print(f"        0x{alvo:X}: puro {bruto[:12].hex()} | xorshift {xorshift(chave, bruto)[:12].hex()}")
    return False


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ip", required=True)
    ap.add_argument("--porta", type=int, default=6000)
    ap.add_argument("--base", type=lambda t: int(t, 0), default=0x5F0B250)
    ap.add_argument("--raio", type=lambda t: int(t, 0), default=0x400000)
    args = ap.parse_args()

    sock = socket.create_connection((args.ip, args.porta), timeout=15)
    print("Titulo:", comando(sock, "getTitleID"), "| Build:", comando(sock, "getBuildID"))

    b = f"0x{args.base:X}"
    ancoras = [ponteiro(sock, f"{b} 0x0"), ponteiro(sock, f"{b} 0xA0 0x0"),
               ponteiro(sock, f"{b} 0xB0 0x0"), ponteiro(sock, f"{b} 0x18 0x1B0 0x0")]
    ancoras = [a for a in ancoras if parece_ponteiro(a)]
    faixas = unir([((a - args.raio) & ~0xFFF, (a + args.raio) & ~0xFFF) for a in ancoras])
    total = sum(f - i for i, f in faixas)

    # leitura maior por pedido, se o sys-botbase aceitar
    bloco = 0x8000
    try:
        ok = len(bytes.fromhex(comando(sock, f"peekAbsolute 0x{ancoras[0]:X} {bloco}"))) == bloco
    except ValueError:
        ok = False
    if not ok:
        bloco = 0x2000
    print(f"Lendo {total / 1048576:.0f} MB em {len(faixas)} faixa(s), {bloco:#x} bytes por pedido...")

    padroes = {struct.pack("<I", k): (k, nome) for k, nome in CHAVES.items()}
    contagem = {nome: 0 for nome in CHAVES.values()}
    lido = 0
    t0 = time.time()
    for ini, fim in faixas:
        for end in range(ini, fim, bloco):
            dados = ler_abs(sock, end, bloco + 0x40)          # folga para o contexto
            for padrao, (chave, nome) in padroes.items():
                pos = dados.find(padrao, 0, bloco)
                while pos != -1:
                    if pos % 4 == 0:
                        contagem[nome] += 1
                        onde = end + pos
                        if contagem[nome] <= 12:
                            print(f"  chave '{nome}' em 0x{onde:X}: {dados[pos:pos + 0x30].hex()}")
                            if nome == "time":
                                for o in range(8, 0x30, 8):
                                    alvo = struct.unpack_from("<Q", dados, pos + o)[0]
                                    if parece_ponteiro(alvo):
                                        print(f"      ponteiro em +0x{o:X}:")
                                        tentar_abrir(sock, alvo, chave)
                    pos = dados.find(padrao, pos + 1, bloco)
            lido += bloco
            if lido % 0x400000 == 0:
                print(f"  {lido / 1048576:.0f}/{total / 1048576:.0f} MB, {time.time() - t0:.0f} s, {contagem}")

    print("Total de ocorrencias:", contagem)
    return 0 if contagem["time"] else 1


if __name__ == "__main__":
    sys.exit(main())
