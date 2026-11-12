
#!/usr/bin/env python3
"""Varredura segura de um objeto na memoria do Switch via sys-botbase.

Ordem dos argumentos (confirmada em commands.md do olliz0r/sys-botbase):
  peekAbsolute <endereco> <tamanho>   -> endereco primeiro, tamanho depois

Uso:
  python3 tools/za_scan_objeto.py snap --ip 192.168.15.5 --base 0x4B4A861C40 --size 512 --out parado.json
  python3 tools/za_scan_objeto.py snap --ip 192.168.15.5 --base 0x4B4A861C40 --size 512 --out andou.json
  python3 tools/za_scan_objeto.py comparar parado.json andou.json

Seguranca:
  - Cada leitura tem tamanho fixo de 64 bytes.
  - Timeout curto por leitura e pausa entre leituras.
  - Para depois de 3 falhas seguidas.
  - O endereco base e um ponteiro de heap: muda quando o jogo reinicia.
"""
import argparse
import json
import math
import socket
import struct
import sys
import time

BLOCO = 64
TIMEOUT = 2.0
PAUSA = 0.05
MAX_FALHAS = 3


def ler_bloco(ip, porta, endereco, tamanho):
    """Le 'tamanho' bytes em 'endereco' e devolve os bytes, ou None se falhar."""
    s = socket.create_connection((ip, porta), timeout=TIMEOUT)
    try:
        s.sendall(f"peekAbsolute 0x{endereco:X} {tamanho}\n".encode())
        alvo = tamanho * 2
        recebido = b""
        while len(recebido) < alvo:
            try:
                parte = s.recv(256)
            except socket.timeout:
                break
            if not parte:
                break
            recebido += parte
        texto = "".join(ch for ch in recebido.decode(errors="ignore") if ch in "0123456789abcdefABCDEF")
        if len(texto) < alvo:
            return None
        return bytes.fromhex(texto[:alvo])
    except (socket.timeout, OSError, ValueError):
        return None
    finally:
        s.close()


def cmd_snap(args):
    base = int(args.base, 0)
    dados = {}
    falhas = 0
    for off in range(0, args.size, BLOCO):
        bloco = ler_bloco(args.ip, args.porta, base + off, BLOCO)
        if bloco is None:
            falhas += 1
            print(f"  falha em base+0x{off:X} ({falhas}/{MAX_FALHAS})", file=sys.stderr)
            if falhas >= MAX_FALHAS:
                print("Parando: falhas seguidas. Verifique o jogo e o sys-botbase.", file=sys.stderr)
                break
            continue
        falhas = 0
        dados[off] = bloco.hex()
        time.sleep(PAUSA)
    with open(args.out, "w", encoding="utf-8") as f:
        json.dump({"base": base, "tamanho": args.size, "blocos": dados,
                   "quando": time.strftime("%Y-%m-%d %H:%M:%S")}, f, indent=1)
    print(f"Salvo em {args.out}: {len(dados)} blocos de {BLOCO} bytes.")


def carregar(caminho):
    with open(caminho, encoding="utf-8") as f:
        j = json.load(f)
    mem = bytearray()
    for off, hx in sorted((int(k), v) for k, v in j["blocos"].items()):
        if len(mem) < off:
            mem.extend(b"\x00" * (off - len(mem)))
        mem[off:off + BLOCO] = bytes.fromhex(hx)
    return bytes(mem), j


def cmd_comparar(args):
    a, ja = carregar(args.parado)
    b, jb = carregar(args.andou)
    if ja["base"] != jb["base"]:
        print("AVISO: os snapshots tem bases diferentes. Refaca os dois com a mesma base.")
    tam = min(len(a), len(b))
    candidatos = []
    for off in range(0, tam - 3, 4):
        if a[off:off + 4] == b[off:off + 4]:
            continue
        fa = struct.unpack("<f", a[off:off + 4])[0]
        fb = struct.unpack("<f", b[off:off + 4])[0]
        if not (math.isfinite(fa) and math.isfinite(fb)):
            continue
        if max(abs(fa), abs(fb)) > 1e5:
            continue
        candidatos.append((off, fa, fb))
    print(f"Floats que mudaram (plausiveis, |v| <= 1e5): {len(candidatos)}")
    print(f"{'offset':>10}  {'parado':>14}  {'andou':>14}  {'delta':>12}")
    for off, fa, fb in candidatos:
        print(f"0x{off:08X}  {fa:14.4f}  {fb:14.4f}  {fb - fa:12.4f}")
    if not candidatos:
        print("Nada mudou. Confira se o personagem andou de fato e se a base esta certa.")


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = p.add_subparsers(dest="cmd", required=True)

    s = sub.add_parser("snap", help="le o objeto e salva em JSON")
    s.add_argument("--ip", required=True)
    s.add_argument("--porta", type=int, default=6000)
    s.add_argument("--base", required=True, help="endereco absoluto do objeto, ex: 0x4B4A861C40")
    s.add_argument("--size", type=int, default=512, help="bytes a ler a partir da base (padrao 512)")
    s.add_argument("--out", required=True)
    s.set_defaults(func=cmd_snap)

    c = sub.add_parser("comparar", help="compara dois snapshots e lista floats que mudaram")
    c.add_argument("parado")
    c.add_argument("andou")
    c.set_defaults(func=cmd_comparar)

    args = p.parse_args()
    if args.cmd == "snap" and args.size > 4096:
        p.error("--size muito grande; use ate 4096 bytes")
    args.func(args)


if __name__ == "__main__":
    main()
