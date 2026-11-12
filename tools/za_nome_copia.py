#!/usr/bin/env python3
"""Procura um nome em texto puro na copia de memoria (sem rede) e interpreta o que ha em volta.

Um Pokemon mantido ABERTO na memoria tem a soma desatualizada e escapa das buscas pela soma, mas
o apelido fica em UTF-16 no deslocamento 0x58 do bloco. Para cada ocorrencia do nome, o script
olha os bytes como se o bloco comecasse 0x58 antes e mostra especie, HP e o nome do treinador.

Uso:  python3 tools/za_nome_copia.py --nome Chikorita --treinador Laila
"""
import argparse
import json
import os
import re
import struct
import sys


def texto16(dados, pos, tamanho=26):
    return dados[pos:pos + tamanho].decode("utf-16-le", errors="replace").split("\0")[0]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--pasta", default="dump_za")
    ap.add_argument("--nome", default="Chikorita")
    ap.add_argument("--treinador", default="Laila")
    args = ap.parse_args()

    with open(os.path.join(args.pasta, "meta.json")) as f:
        meta = json.load(f)
    achados = blocos = 0
    for arquivo, origem in (("heap.bin", meta["heap_ini"]), ("main.bin", meta["main_ini"])):
        with open(os.path.join(args.pasta, arquivo), "rb") as f:
            dados = f.read()
        ocorrencias = [m.start() for m in re.finditer(re.escape(args.nome.encode("utf-16-le")), dados)]
        treinador = len(re.findall(re.escape(args.treinador.encode("utf-16-le")), dados))
        print(f"{arquivo}: '{args.nome}' aparece {len(ocorrencias)} vez(es); '{args.treinador}' {treinador} vez(es)")
        for pos in ocorrencias:
            achados += 1
            ini = pos - 0x58
            if ini < 0 or ini + 0x158 > len(dados):
                continue
            chave, sanidade, soma = struct.unpack_from("<IHH", dados, ini)
            especie = struct.unpack_from("<H", dados, ini + 0x08)[0]
            hp = struct.unpack_from("<H", dados, ini + 0x8A)[0]
            dono = texto16(dados, ini + 0xF8)
            parece = sanidade == 0 and 0 < especie < 1200 and hp <= 999 and chave != 0
            if parece:
                blocos += 1
            if achados <= 40 or parece:
                print(f"  0x{origem + pos:X}: bloco em 0x{origem + ini:X}? chave {chave:08X}, sanidade {sanidade}, "
                      f"especie {especie}, HP {hp}, treinador '{dono[:12]}'"
                      + ("   <== PARECE UM POKEMON ABERTO" if parece else ""))
                if parece:
                    print(f"      nivel (extensao contigua) {dados[ini + 0x148]}, "
                          f"HP maximo {struct.unpack_from('<H', dados, ini + 0x14A)[0]}; "
                          f"inicio {dados[ini:ini + 16].hex()}")
    print(f"Total: {achados} ocorrencias, {blocos} com cara de Pokemon aberto.")
    return 0 if blocos else 1


if __name__ == "__main__":
    sys.exit(main())
