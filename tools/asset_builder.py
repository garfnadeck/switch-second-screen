import os
import struct
from PIL import Image, ImageDraw

# Caminhos de saída
STAGING_DIR = "sdcard_staging/assets"

def rgb888_to_rgb565_alpha(img: Image.Image) -> bytearray:
    """
    Converte uma imagem RGBA do Pillow para o formato LV_IMG_CF_TRUE_COLOR_ALPHA (RGB565 + 8-bit Alpha = 3 bytes/pixel)
    compatível com telas TFT 16-bit no LVGL 8.4.
    """
    img = img.convert("RGBA")
    raw_data = bytearray()
    for r, g, b, a in img.getdata():
        # Converte R(8) G(8) B(8) para RGB565 (16-bit Little Endian)
        rgb565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
        raw_data.extend(struct.pack("<HB", rgb565, a))
    return raw_data

def generate_zelda_hearts():
    """Gera os ícones de Coração (Cheio, Meio, Vazio) para Zelda TOTK/BOTW em PNG e .bin"""
    out_dir = os.path.join(STAGING_DIR, "zelda_totk/icons")
    os.makedirs(out_dir, exist_ok=True)
    
    size = (24, 24)
    states = ["full", "half", "empty"]
    
    for state in states:
        img = Image.new("RGBA", size, (0, 0, 0, 0))
        draw = ImageDraw.Draw(img)
        
        # Desenho geométrico estilizado de coração
        heart_polygon = [
            (12, 21), (3, 12), (2, 7), (5, 3), 
            (9, 3), (12, 6), (15, 3), (19, 3), 
            (22, 7), (21, 12)
        ]
        
        if state == "full":
            draw.polygon(heart_polygon, fill=(255, 23, 68, 255), outline=(255, 255, 255, 220))
        elif state == "empty":
            draw.polygon(heart_polygon, fill=(40, 40, 60, 200), outline=(120, 120, 150, 200))
        elif state == "half":
            draw.polygon(heart_polygon, fill=(40, 40, 60, 200), outline=(120, 120, 150, 200))
            # Pinta metade esquerda de vermelho
            left_half = [(12, 21), (3, 12), (2, 7), (5, 3), (9, 3), (12, 6)]
            draw.polygon(left_half, fill=(255, 23, 68, 255))

        # Salva PNG (para preview/Web Server) e .bin (para o LVGL no MicroSD)
        img.save(os.path.join(out_dir, f"heart_{state}.png"))
        with open(os.path.join(out_dir, f"heart_{state}.bin"), "wb") as f:
            f.write(rgb888_to_rgb565_alpha(img))
            
    print(f"[OK] Ícones de coração do Zelda gerados em: {out_dir}")

def slice_map_into_tiles(source_image_path: str, output_subdir: str, tile_size: int = 128):
    """
    Fatia uma imagem grande de mapa em blocos de 128x128px (PNG e .bin) para carregamento sob demanda na PSRAM.
    """
    if not os.path.exists(source_image_path):
        print(f"[Aviso] Imagem fonte não encontrada: {source_image_path}")
        return

    out_dir = os.path.join(STAGING_DIR, output_subdir)
    os.makedirs(out_dir, exist_ok=True)

    img = Image.open(source_image_path).convert("RGBA")
    width, height = img.size
    cols = width // tile_size
    rows = height // tile_size

    for r in range(rows):
        for c in range(cols):
            box = (c * tile_size, r * tile_size, (c + 1) * tile_size, (r + 1) * tile_size)
            tile = img.crop(box)
            
            base_name = f"tile_{r}_{c}"
            tile.save(os.path.join(out_dir, f"{base_name}.png"))
            with open(os.path.join(out_dir, f"{base_name}.bin"), "wb") as f:
                f.write(rgb888_to_rgb565_alpha(tile))

    print(f"[OK] Mapa fatiado em {rows * cols} tiles ({cols}x{rows}) em: {out_dir}")

def generate_sample_radar_map():
    """Gera um mapa estilizado de exemplo (512x512) e fatia em tiles de 128x128 para testes"""
    sample_path = "tools/sample_map_512.png"
    img = Image.new("RGBA", (512, 512), (11, 25, 44, 255))
    draw = ImageDraw.Draw(img)
    
    # Grade tática e contornos simulados de terreno
    for i in range(0, 512, 32):
        draw.line([(i, 0), (i, 512)], fill=(20, 50, 80, 255), width=1)
        draw.line([(0, i), (512, i)], fill=(20, 50, 80, 255), width=1)
        
    # Círculos topográficos estilizados
    draw.ellipse([100, 100, 400, 400], outline=(0, 245, 212, 120), width=2)
    draw.ellipse([180, 180, 320, 320], outline=(0, 245, 212, 180), width=2)
    
    img.save(sample_path)
    slice_map_into_tiles(sample_path, "pokemon_sv/maps/paldea", tile_size=128)

if __name__ == "__main__":
    generate_zelda_hearts()
    generate_sample_radar_map()
