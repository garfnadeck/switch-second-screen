import json
from pathlib import Path
from PIL import Image

CATALOGO = {
    "pkmn_za": [
        {
            "id": "lumiose",
            "name": "🏙️ Lumiose Surface",
            "zone": "Lumiose City (Surface)",
            "poi": "Prism Tower",
            "z": 0,
            "grid": 4, # 4x4 (16 tiles)
            "img": "mapa_lumiose",
            "theme": "#1B2838"
        },
        {
            "id": "lysandre",
            "name": "🔬 Lysandre Labs (B1F/B2F)",
            "zone": "Lysandre Secret Labs (B1F & B2F)",
            "poi": "Warp Panel / Lab Core",
            "z": -50,
            "grid": 3, # 3x3 (9 tiles de alta nitidez para ver as salas e corredores)
            "img": "za_lysandre",
            "theme": "#14141E"
        },
        {
            "id": "sewers_bleu",
            "name": "🕳️ Sewers (Bleu Sec. 3)",
            "zone": "Lumiose Sewers - Bleu Sector 3",
            "poi": "Bleu Floodgate",
            "z": -30,
            "grid": 2, # 2x2 (4 tiles)
            "img": "za_sewers_bleu",
            "theme": "#14262A"
        },
        {
            "id": "sewers_saison",
            "name": "💧 Sewers (Saison Canal)",
            "zone": "Lumiose Sewers - Saison Canal",
            "poi": "Canal Waterway",
            "z": -35,
            "grid": 2,
            "img": "za_sewers_saison",
            "theme": "#122B32"
        }
    ],
    "pkmn_sv": [
        {"id": "paldea",   "name": "🗺️ Paldea",        "zone": "Paldea",            "poi": "Tera Raid 6★", "z": 0, "grid": 4, "img": "mapa_paldea",   "theme": "#2A1E3A"},
        {"id": "kitakami", "name": "🎋 Kitakami",      "zone": "Kitakami",          "poi": "Ogerpon Den",  "z": 0, "grid": 3, "img": "mapa_kitakami", "theme": "#1C3026"},
        {"id": "terarium", "name": "🌐 Terarium (BB)", "zone": "Blueberry Academy", "poi": "Core Plaza",   "z": 0, "grid": 3, "img": "mapa_terarium", "theme": "#182A3E"}
    ],
    "zelda_totk": [
        {"id": "sky",     "name": "☁️ Céu (Sky)",        "zone": "Great Sky Island", "poi": "Flux Construct", "z": 1200, "grid": 3, "img": "mapa_totk_sky",     "theme": "#1C3444"},
        {"id": "surface", "name": "🌍 Superfície",       "zone": "Central Hyrule",   "poi": "Lookout Landing","z": 0,    "grid": 4, "img": "mapa_totk_surface", "theme": "#1B2D22"},
        {"id": "depths",  "name": "🕳️ Subsolo (Depths)", "zone": "Hyrule Depths",    "poi": "Lightroot",      "z": -650, "grid": 4, "img": "mapa_totk_depths",  "theme": "#161026"}
    ],
    "zelda_botw": [
        {"id": "surface", "name": "🌍 Hyrule Surface", "zone": "Hyrule Castle", "poi": "Ancient Shrine", "z": 0, "grid": 4, "img": "mapa_botw", "theme": "#1F291E"}
    ],
    "mk8": [
        {"id": "track",   "name": "🏁 Pista Ativa",    "zone": "Rainbow Road",  "poi": "Double Item Box","z": 0, "grid": 2, "img": "mapa_mk8",  "theme": "#1A1C2E"}
    ]
}

def encontrar_imagem(base_name):
    if not base_name:
        return None
    for ext in [".jpg", ".jpeg", ".png", ""]:
        p = Path(f"{base_name}{ext}")
        if p.exists() and p.is_file():
            return p
    return None

def padronizar_quadrado(im):
    w, h = im.size
    if w == h:
        return im
    lado = max(w, h)
    quadrada = Image.new("RGB", (lado, lado), (11, 19, 32))
    quadrada.paste(im, ((lado - w) // 2, (lado - h) // 2))
    return quadrada

def gerar_flash():
    manifest = {"games": {}}
    total_tiles = 0

    for game, layers in CATALOGO.items():
        manifest["games"][game] = []
        for layer in layers:
            lid = layer["id"]
            gsize = layer.get("grid", 4)
            img_path = encontrar_imagem(layer["img"])
            has_real_tiles = False
            base_path = f"/m/{game}/{lid}"

            if img_path:
                out_dir = Path(f"data/m/{game}/{lid}")
                out_dir.mkdir(parents=True, exist_ok=True)
                
                im = Image.open(img_path).convert("RGB")
                im = padronizar_quadrado(im)
                
                w, h = im.size
                tw, th = w // gsize, h // gsize
                tile_px = 160 if gsize >= 3 else 180

                for r in range(gsize):
                    for c in range(gsize):
                        tile = im.crop((c*tw, r*th, (c+1)*tw, (r+1)*th)).resize((tile_px, tile_px), Image.Resampling.LANCZOS)
                        tile.save(out_dir / f"{c}_{r}.jpg", "JPEG", quality=78, optimize=True)
                        total_tiles += 1
                has_real_tiles = True
                print(f"[MAPA REAL] {game} -> {lid} ('{img_path.name}') fatiado em {gsize}x{gsize} ({gsize*gsize} tiles)!")
            else:
                print(f"[PENDENTE]  {game} -> {lid} (Aguardando '{layer['img']}.jpg/.png')")

            manifest["games"][game].append({
                "id": lid,
                "name": layer["name"],
                "zone": layer["zone"],
                "poi": layer["poi"],
                "z": layer["z"],
                "grid": gsize,
                "has_tiles": has_real_tiles,
                "base_path": base_path,
                "theme": layer["theme"]
            })

    Path("data").mkdir(exist_ok=True)
    with open("data/maps_config.json", "w", encoding="utf-8") as f:
        json.dump(manifest, f)

    print(f"\n[CONCLUÍDO] Total de ficheiros JPG na Flash: {total_tiles}")

if __name__ == "__main__":
    gerar_flash()
