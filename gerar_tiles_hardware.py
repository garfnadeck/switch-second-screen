import os
from pathlib import Path
from PIL import Image
import json

def fatiar_para_sdcard():
    img_path = "mapa_lumiose.jpg"
    if not Path(img_path).exists():
        print(f"[Erro] Imagem {img_path} não encontrada na raiz.")
        return

    im = Image.open(img_path)
    width, height = im.size

    zoom = 12
    cols, rows = 4, 4
    tile_w = width // cols
    tile_h = height // rows

    # Caminho oficial de staging para o MicroSD do Pico W / ESP32
    base_out = Path("sdcard_staging/assets/pokemon_za/maps/pokemon-lumiose-city/tiles") / str(zoom)
    base_out.mkdir(parents=True, exist_ok=True)

    # Espelho para o mock web local
    mock_out = Path("hud_mock/pokemon-lumiose-city/tiles") / str(zoom)
    mock_out.mkdir(parents=True, exist_ok=True)

    manifest = []

    for r in range(rows):
        y = 2034 + r
        for c in range(cols):
            x = 2039 + c

            box = (c * tile_w, r * tile_h, (c + 1) * tile_w, (r + 1) * tile_h)
            tile_img = im.crop(box)

            # Grava no staging do MicroSD
            x_dir_sd = base_out / str(x)
            x_dir_sd.mkdir(parents=True, exist_ok=True)
            tile_file_sd = x_dir_sd / f"{y}.jpg"
            tile_img.save(tile_file_sd, "JPEG", quality=92)

            # Grava no mock web
            x_dir_mock = mock_out / str(x)
            x_dir_mock.mkdir(parents=True, exist_ok=True)
            tile_file_mock = x_dir_mock / f"{y}.jpg"
            tile_img.save(tile_file_mock, "JPEG", quality=92)

            rel_path = f"pokemon-lumiose-city/tiles/{zoom}/{x}/{y}.jpg"
            manifest.append({
                "z": str(zoom),
                "x": str(x),
                "y": str(y),
                "col": c,
                "row": r,
                "path": rel_path
            })

    output = {
        "cols": cols,
        "rows": rows,
        "zoom": zoom,
        "tiles": manifest
    }

    # Salva o manifesto em ambos os locais
    with open("hud_mock/tiles_map.json", "w", encoding="utf-8") as f:
        json.dump(output, f, indent=2)

    sd_manifest = Path("sdcard_staging/assets/pokemon_za/maps/pokemon-lumiose-city/tiles_map.json")
    with open(sd_manifest, "w", encoding="utf-8") as f:
        json.dump(output, f, indent=2)

    print(f"[OK] {len(manifest)} tiles gerados para o MicroSD e mock web com sucesso!")

if __name__ == "__main__":
    fatiar_para_sdcard()