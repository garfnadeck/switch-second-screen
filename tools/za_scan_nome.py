#!/usr/bin/env python3
"""Procura um Pokemon na memoria pelo nome em texto puro (so leitura).

Um Pokemon em uso fica aberto na memoria e a soma de verificacao pode estar desatualizada,
entao a busca pela soma falha. O apelido fica em UTF-16 no deslocamento 0x58 do bloco; sem
apelido personalizado, o campo guarda o nome da especie.
Para cada ocorrencia do nome, mostra os campos do bloco que comecaria 0x58 bytes antes.

Uso:  python3 tools/za_scan_nome.py --ip 192.168.15.9 --hp 25
      python3 tools/za_scan_nome.py --ip 192.168.15.9 --nome Germignon --hp 25
"""
import argparse
import socket
import struct
import sys
import time

from za_scan_heap import ler_abs, parece_ponteiro, ponteiro, unir
from za_scan_pos import comando

BLOCO = 0x8000
OFF_NOME = 0x58
OFF_HP = 0x8A
# Chikorita nos idiomas do jogo
NOMES_PADRAO = ["Chikorita", "Germignon", "Endivie", "チコリータ", "치코리타", "菊草葉", "菊草叶"]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ip", required=True)
    ap.add_argument("--porta", type=int, default=6000)
    ap.add_argument("--base", type=lambda t: int(t, 0), default=0x5F0B250)
    ap.add_argument("--nome", action="append", help="nome a procurar (pode repetir)")
    ap.add_argument("--hp", type=int, default=None, help="HP atual esperado, para destacar o bloco certo")
    ap.add_argument("--raio", type=lambda t: int(t, 0), default=0x400000)
    args = ap.parse_args()

    sock = socket.create_connection((args.ip, args.porta), timeout=15)
    print("Titulo:", comando(sock, "getTitleID"), "| Build:", comando(sock, "getBuildID"))

    b = f"0x{args.base:X}"
    reservatorio = ponteiro(sock, f"{b} 0x18 0x8 0x0")
    ancoras = [ponteiro(sock, f"{b} 0x0"), ponteiro(sock, f"{b} 0xA0 0x0"),
               ponteiro(sock, f"{b} 0xB0 0x0"), ponteiro(sock, f"{b} 0x18 0x0"),
               ponteiro(sock, f"{b} 0x18 0x1B0 0x0")]
    faixas = [((reservatorio & ~0xFFF), (reservatorio & ~0xFFF) + 0x800000)]
    faixas += [((a - args.raio) & ~0xFFF, (a + args.raio) & ~0xFFF) for a in ancoras if parece_ponteiro(a)]
    faixas = unir(faixas)
    total = sum(f - i for i, f in faixas)

    padroes = {n.encode("utf-16-le"): n for n in (args.nome or NOMES_PADRAO)}
    print(f"Procurando {list(padroes.values())} em {total / 1048576:.0f} MB...")

    achados = 0
    destaque = 0
    lido = 0
    t0 = time.time()
    folga = 0x200
    for ini, fim in faixas:
        for end in range(ini, fim, BLOCO):
            # le um pouco antes e depois para poder mostrar o bloco inteiro em volta do nome
            dados = ler_abs(sock, end - folga, BLOCO + 2 * folga)
            for padrao, nome in padroes.items():
                pos = dados.find(padrao, folga, folga + BLOCO)
                while pos != -1:
                    if pos % 2 == 0:
                        achados += 1
                        inicio = pos - OFF_NOME                  # onde o bloco comecaria
                        onde = end - folga + inicio
                        chave, sanidade, soma = struct.unpack_from("<IHH", dados, inicio)
                        esp = struct.unpack_from("<H", dados, inicio + 0x08)[0]
                        hp = struct.unpack_from("<H", dados, inicio + OFF_HP)[0]
                        marca = ""
                        if sanidade == 0 and (args.hp is None or hp == args.hp):
                            destaque += 1
                            marca = "  <== PARECE UM BLOCO DE POKEMON"
                        if achados <= 60 or marca:
                            print(f"  '{nome}' em 0x{end - folga + pos:X}; bloco em 0x{onde:X}: "
                                  f"chave {chave:08X}, sanidade {sanidade}, soma {soma:04X}, "
                                  f"especie {esp}, HP {hp}{marca}")
                            if marca:
                                print(f"      inicio do bloco: {dados[inicio:inicio + 0x20].hex()}")
                    pos = dados.find(padrao, pos + 1, folga + BLOCO)
            lido += BLOCO
            if lido % 0x400000 == 0:
                print(f"    {lido / 1048576:.0f}/{total / 1048576:.0f} MB, {time.time() - t0:.0f} s, "
                      f"{achados} ocorrencias, {destaque} com cara de bloco")

    print(f"Total: {achados} ocorrencias do nome, {destaque} com cara de bloco de Pokemon.")
    return 0 if destaque else 1


if __name__ == "__main__":
    sys.exit(main())
