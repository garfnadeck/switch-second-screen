#!/usr/bin/env python3
"""Gera os assets binarios (RGB565 / RGB565A para LVGL 8.4) do MicroSD de 16 GB do ESP32-S3:
  - Mapas do Pokemon Legends: Z-A em sdcard_staging/assets/za/maps/
      * lumiose_radar.bin  (480x480, LV_IMG_CF_TRUE_COLOR = 460800 bytes)
      * lumiose_full.bin   (208x208, LV_IMG_CF_TRUE_COLOR = 86528 bytes)
      * lysandre_full.bin  (208x208, LV_IMG_CF_TRUE_COLOR = 86528 bytes)
      * sewers_bleu.bin    (208x208, LV_IMG_CF_TRUE_COLOR = 86528 bytes)
      * sewers_saison.bin  (208x208, LV_IMG_CF_TRUE_COLOR = 86528 bytes)
  - Sprites 40x40 dos Pokemon em sdcard_staging/assets/za/sprites/<species>.bin
      (LV_IMG_CF_TRUE_COLOR_ALPHA = 4800 bytes cada)
"""
import io
import os
import ssl
import struct
import urllib.request
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parent.parent
OUT_MAPS = ROOT / "sdcard_staging" / "assets" / "za" / "maps"
OUT_SPRITES = ROOT / "sdcard_staging" / "assets" / "za" / "sprites"

# Especies do time atual + linhas evolutivas + iniciais de Z-A + favoritos de Kalos/Mega
ZA_SPECIES = [
    # Time atual do usuario + evolucoes
    152, 153, 154,  # Chikorita, Bayleef, Meganium
    661, 662, 663,  # Fletchling, Fletchinder, Talonflame
    659, 660,       # Bunnelby, Diggersby
    664, 665, 666,  # Scatterbug, Spewpa, Vivillon
    # Outros iniciais de Z-A e Kalos
    158, 159, 160,  # Totodile, Croconaw, Feraligatr
    498, 499, 500,  # Tepig, Pignite, Emboar
    650, 651, 652,  # Chespin, Quilladin, Chesnaught
    653, 654, 655,  # Fennekin, Braixen, Delphox
    656, 657, 658,  # Froakie, Frogadier, Greninja
    1, 4, 7, 25, 26, 94, 130, 133, 134, 135, 136, 196, 197, 470, 471, 700,
    # Comuns em Lumiose / Wild Zones + Megas classicos
    13, 14, 15,     # Weedle, Kakuna, Beedrill
    16, 17, 18,     # Pidgey, Pidgeotto, Pidgeot
    63, 64, 65,     # Abra, Kadabra, Alakazam
    179, 180, 181,  # Mareep, Flaaffy, Ampharos
    212, 214, 228, 229, 248, 280, 281, 282, 475,
    302, 303, 304, 305, 306, 309, 310, 359,
    443, 444, 445,  # Gible, Gabite, Garchomp
    447, 448,       # Riolu, Lucario
    459, 460,       # Snover, Abomasnow
    667, 668,       # Litleo, Pyroar
    669, 670, 671,  # Flabebe, Floette, Florges
    672, 673,       # Skiddo, Gogoat
    674, 675,       # Pancham, Pangoro
    676,            # Furfrou
    677, 678,       # Espurr, Meowstic
    679, 680, 681,  # Honedge, Doublade, Aegislash
    701, 702, 704, 705, 706, 707, 714, 715,
    716, 717, 718,  # Xerneas, Yveltal, Zygarde
]


def img_to_rgb565(img: Image.Image) -> bytes:
    """LV_IMG_CF_TRUE_COLOR (RGB565 little-endian, 2 bytes/pixel)."""
    rgb = img.convert("RGB")
    buf = bytearray(rgb.width * rgb.height * 2)
    idx = 0
    for r, g, b in rgb.getdata():
        c = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
        buf[idx] = c & 0xFF
        buf[idx + 1] = (c >> 8) & 0xFF
        idx += 2
    return bytes(buf)


def img_to_rgb565a(img: Image.Image) -> bytes:
    """LV_IMG_CF_TRUE_COLOR_ALPHA (RGB565 little-endian + Alpha 8-bit = 3 bytes/pixel)."""
    rgba = img.convert("RGBA")
    buf = bytearray(rgba.width * rgba.height * 3)
    idx = 0
    for r, g, b, a in rgba.getdata():
        c = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
        buf[idx] = c & 0xFF
        buf[idx + 1] = (c >> 8) & 0xFF
        buf[idx + 2] = a
        idx += 3
    return bytes(buf)


def padronizar_quadrado(im: Image.Image, bg=(11, 25, 44)) -> Image.Image:
    w, h = im.size
    if w == h:
        return im
    lado = max(w, h)
    q = Image.new("RGB", (lado, lado), bg)
    q.paste(im, ((lado - w) // 2, (lado - h) // 2))
    return q


def gerar_mapas():
    OUT_MAPS.mkdir(parents=True, exist_ok=True)
    fontes = [
        ("mapa_lumiose.jpg", "lumiose_radar.bin", 480),
        ("mapa_lumiose.jpg", "lumiose_full.bin", 208),
        ("za_lysandre.png", "lysandre_full.bin", 208),
        ("za_sewers_bleu.png", "sewers_bleu.bin", 208),
        ("za_sewers_saison.png", "sewers_saison.bin", 208),
    ]
    for src_name, dst_name, side in fontes:
        src = ROOT / src_name
        if not src.exists():
            print(f"[AVISO] Mapa fonte ausente: {src}")
            continue
        im = Image.open(src).convert("RGB")
        im = padronizar_quadrado(im)
        im_resized = im.resize((side, side), Image.Resampling.LANCZOS)
        raw = img_to_rgb565(im_resized)
        dst = OUT_MAPS / dst_name
        dst.write_bytes(raw)
        print(f"[MAPA OK] {dst.name:20s} ({side}x{side}, {len(raw):7d} bytes)")


def desenhar_pokebola_40() -> Image.Image:
    im = Image.new("RGBA", (40, 40), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    # Metade superior vermelha, inferior branca
    d.pieslice([3, 3, 36, 36], 180, 360, fill=(238, 52, 72, 255), outline=(25, 28, 42, 255), width=2)
    d.pieslice([3, 3, 36, 36], 0, 180, fill=(240, 244, 255, 255), outline=(25, 28, 42, 255), width=2)
    d.line([(3, 20), (36, 20)], fill=(25, 28, 42, 255), width=3)
    d.ellipse([14, 14, 25, 25], fill=(240, 244, 255, 255), outline=(25, 28, 42, 255), width=2)
    d.ellipse([17, 17, 22, 22], fill=(0, 245, 212, 255))
    return im


def baixar_sprite(sp: int) -> bool:
    dst_bin = OUT_SPRITES / f"{sp}.bin"
    dst_png = OUT_SPRITES / f"{sp}.png"
    if dst_bin.exists() and dst_bin.stat().st_size == 40 * 40 * 3:
        return True
    url = f"https://raw.githubusercontent.com/PokeAPI/sprites/master/sprites/pokemon/{sp}.png"
    try:
        ctx = ssl.create_default_context(cafile="/etc/ssl/cert.pem") if Path("/etc/ssl/cert.pem").exists() else None
        req = urllib.request.Request(url, headers={"User-Agent": "SwitchSecondScreen/1.0"})
        with urllib.request.urlopen(req, timeout=8, context=ctx) as r:
            data = r.read()
        im = Image.open(io.BytesIO(data)).convert("RGBA")
        # Recorta bordas transparentes para aproveitar os 40x40 px da tela 3.5"
        bbox = im.getbbox()
        if bbox:
            im = im.crop(bbox)
        canvas = Image.new("RGBA", (40, 40), (0, 0, 0, 0))
        im.thumbnail((38, 38), Image.Resampling.LANCZOS)
        ox = (40 - im.width) // 2
        oy = (40 - im.height) // 2
        canvas.paste(im, (ox, oy), im)
        canvas.save(dst_png)
        dst_bin.write_bytes(img_to_rgb565a(canvas))
        return True
    except Exception as e:
        print(f"  [falha #{sp}] {e}")
        return False


def gerar_sprites():
    OUT_SPRITES.mkdir(parents=True, exist_ok=True)
    # 0.bin = Pokebola padrao (fallback)
    pb = desenhar_pokebola_40()
    pb.save(OUT_SPRITES / "0.png")
    (OUT_SPRITES / "0.bin").write_bytes(img_to_rgb565a(pb))
    print("[SPRITE OK] 0.bin (Pokebola padrao 40x40)")

    unicos = sorted(set(ZA_SPECIES))
    ok = 0
    with ThreadPoolExecutor(max_workers=8) as ex:
        for sp, res in zip(unicos, ex.map(baixar_sprite, unicos)):
            if res:
                ok += 1
    print(f"[SPRITES CONCLUIDO] {ok}/{len(unicos)} especies geradas em {OUT_SPRITES}")


if __name__ == "__main__":
    gerar_mapas()
    gerar_sprites()
