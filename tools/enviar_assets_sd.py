#!/usr/bin/env python3
"""Envia os mapas e sprites de sdcard_staging/assets/za/ via Wi-Fi para o MicroSD de 16 GB
do ESP32-S3 (endpoints /api/sd/status, /api/sd/upload e /api/sd/reload).

Uso:
  python3 tools/enviar_assets_sd.py --ip 192.168.15.X
  python3 tools/enviar_assets_sd.py --ip 192.168.15.X --so-time   # envia mapas + sprites do time atual
"""
import argparse
import json
import sys
import time
import urllib.parse
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
ZA_DIR = ROOT / "sdcard_staging" / "assets" / "za"

# Especies prioritarias (Pokebola + time atual + iniciais + evolucoes) enviadas primeiro
PRIORIDADE = [0, 152, 661, 659, 664, 153, 154, 662, 663, 660, 665, 666, 25, 133, 158, 498, 448, 445, 282, 658]


def http_get_json(url: str, timeout: float = 5.0) -> dict:
    with urllib.request.urlopen(url, timeout=timeout) as r:
        return json.loads(r.read().decode("utf-8", errors="replace"))


def upload_arquivo(ip: str, sd_path: str, local_file: Path, timeout: float = 20.0) -> bool:
    boundary = "----SwitchHudBoundary7MA4YWxkTrZu0gW"
    data = local_file.read_bytes()
    head = (
        f"--{boundary}\r\n"
        f'Content-Disposition: form-data; name="file"; filename="{local_file.name}"\r\n'
        f"Content-Type: application/octet-stream\r\n\r\n"
    ).encode("utf-8")
    tail = f"\r\n--{boundary}--\r\n".encode("utf-8")
    body = head + data + tail

    q = urllib.parse.urlencode({"path": sd_path})
    url = f"http://{ip}/api/sd/upload?{q}"
    req = urllib.request.Request(
        url,
        data=body,
        method="POST",
        headers={
            "Content-Type": f"multipart/form-data; boundary={boundary}",
            "Content-Length": str(len(body)),
        },
    )
    with urllib.request.urlopen(req, timeout=timeout) as r:
        resp = json.loads(r.read().decode("utf-8", errors="replace"))
        return bool(resp.get("ok"))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ip", required=True, help="IP da placa ESP32-S3 na rede Wi-Fi")
    ap.add_argument("--so-time", action="store_true", help="Envia os mapas + sprites principais primeiro")
    args = ap.parse_args()

    print(f"Consultando MicroSD na placa http://{args.ip}/api/sd/status ...")
    st = http_get_json(f"http://{args.ip}/api/sd/status")
    print(f"  sd_ready={st.get('sd_ready')} | total={st.get('total_mb')} MB | usado={st.get('used_mb')} MB\n")

    arquivos = []
    # 1. Sprites prioritarios primeiro (rapidos: 4.8 KB cada)
    sprites_dir = ZA_DIR / "sprites"
    enviados_sp = set()
    for sp in PRIORIDADE:
        f = sprites_dir / f"{sp}.bin"
        if f.exists():
            arquivos.append((f"/assets/za/sprites/{f.name}", f))
            enviados_sp.add(f.name)

    # 2. Mapas (208x208 e 480x480)
    maps_dir = ZA_DIR / "maps"
    for nome in ["lumiose_full.bin", "lumiose_radar.bin", "lumiose_zoom.bin", "lysandre_full.bin", "sewers_bleu.bin", "sewers_saison.bin"]:
        f = maps_dir / nome
        if f.exists():
            arquivos.append((f"/assets/za/maps/{f.name}", f))

    # 3. Demais sprites (se nao pediu --so-time)
    if not args.so_time and sprites_dir.exists():
        for f in sorted(sprites_dir.glob("*.bin")):
            if f.name not in enviados_sp:
                arquivos.append((f"/assets/za/sprites/{f.name}", f))

    t0 = time.time()
    ok_count = 0
    for idx, (sd_path, local_f) in enumerate(arquivos, 1):
        kb = local_f.stat().st_size / 1024.0
        print(f"[{idx:3d}/{len(arquivos):3d}] {sd_path:34s} ({kb:6.1f} KB) ... ", end="", flush=True)
        try:
            ok = upload_arquivo(args.ip, sd_path, local_f)
            print("OK" if ok else "FALHOU")
            if ok:
                ok_count += 1
        except Exception as e:
            print(f"ERRO: {e}")

    # Recarrega na PSRAM da placa imediatamente
    try:
        req = urllib.request.Request(f"http://{args.ip}/api/sd/reload", data=b"", method="POST")
        urllib.request.urlopen(req, timeout=5).read()
        print(f"\n[RELOAD OK] {ok_count}/{len(arquivos)} arquivos gravados no MicroSD e carregados na PSRAM em {time.time() - t0:.1f}s!")
    except Exception as e:
        print(f"\n[AVISO] Arquivos enviados ({ok_count}/{len(arquivos)}), falha no reload: {e}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
