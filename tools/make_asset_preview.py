"""Build nearest-neighbour QA sheets for the tiny production sprites."""
from pathlib import Path
import sys
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "assets" / "screen_png"
OUT = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "assets" / "previews"

GLYPHS = [
    ("E", "gal_e"), ("E-S0", "gal_es0"), ("S0", "gal_s0"),
    ("S0-AA", "gal_s0a"), ("SA", "gal_sa"), ("SB", "gal_sb"),
    ("SAB", "gal_sab"), ("IRR", "gal_irr"), ("GAL", "gal_generic"),
    ("PAIR", "gal_pair"), ("TRIPLET", "gal_triplet"),
    ("GROUP", "gal_group"), ("EMISSION", "emission"), ("HII", "hii"),
    ("REFLECT", "reflection"), ("DARK", "dark"),
    ("PLANETARY", "planetary"), ("REMNANT", "remnant"),
    ("CL+NEB", "clusterneb"), ("GLOBULAR", "globular"),
    ("OPEN CL", "opencluster"), ("NEBULA", "nebula"),
]

scale, cell_w, cell_h, cols = 4, 86, 92, 6
groups = [("DAY OUTLINE", "obj_", ""), ("DAY FILLED", "obj_", "_f"),
          ("NIGHT OUTLINE", "n_obj_", ""),
          ("NIGHT FILLED", "n_obj_", "_f")]
block_h = 28 + cell_h * 4
rows = (len(GLYPHS) + cols - 1) // cols
sheet = Image.new("RGB", (cell_w * cols, block_h * rows), "#020609")
d = ImageDraw.Draw(sheet)
for block in range(rows):
    y0 = block * block_h
    for col in range(cols):
        idx = block * cols + col
        if idx >= len(GLYPHS):
            continue
        label, _ = GLYPHS[idx]
        d.text((col * cell_w + 4, y0 + 6), label, fill="#a8b9c4")
    for row, (caption, prefix, suffix) in enumerate(groups):
        y = y0 + 28 + row * cell_h
        d.text((4, y + 3), caption.replace(" ", "\n"), fill="#52636e")
        for col in range(cols):
            idx = block * cols + col
            if idx >= len(GLYPHS):
                continue
            _, name = GLYPHS[idx]
            im = Image.open(SRC / f"{prefix}{name}{suffix}.png").convert("RGBA")
            im = im.resize((im.width * scale, im.height * scale),
                           Image.Resampling.NEAREST)
            x = col * cell_w + (cell_w - im.width) // 2
            sheet.paste(im, (x, y + 17), im)

moon_cell = 128
moon = Image.new("RGB", (8 * moon_cell, 2 * moon_cell + 22), "#020609")
md = ImageDraw.Draw(moon)
for i in range(8):
    md.text((i * moon_cell + 40, 5), str(i), fill="#a8b9c4")
    for row, prefix in enumerate(("", "n_")):
        im = Image.open(SRC / f"{prefix}moon_{i}.png").convert("RGBA")
        im = im.resize((im.width * scale, im.height * scale),
                       Image.Resampling.NEAREST)
        moon.paste(im, (i * moon_cell + (moon_cell-im.width)//2,
                        22 + row*moon_cell + (moon_cell-im.height)//2), im)

OUT.mkdir(parents=True, exist_ok=True)
sheet.save(OUT / "dso_glyphs_preview.png")
moon.save(OUT / "moon_phases_preview.png")
print(OUT)
