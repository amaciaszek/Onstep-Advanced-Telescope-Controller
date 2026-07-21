"""Build a nearest-neighbour QA sheet for the tiny production sprites."""
from pathlib import Path
import sys
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "assets" / "screen_png"
OUT = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "assets" / "previews"

names = ["galaxy", "planetary", "globular", "nebula", "cluster", "remnant"]
rows = [("DAY OUTLINE", "obj_", ""), ("DAY FILLED", "obj_", "_f"),
        ("NIGHT OUTLINE", "n_obj_", ""), ("NIGHT FILLED", "n_obj_", "_f")]
scale, cell_w, cell_h, left, top = 4, 82, 92, 105, 22
sheet = Image.new("RGB", (left + cell_w * 6, top + cell_h * 4), "#020609")
d = ImageDraw.Draw(sheet)
for col, name in enumerate(names):
    d.text((left + col * cell_w + 3, 5), name.upper(), fill="#a8b9c4")
for row, (caption, prefix, suffix) in enumerate(rows):
    y = top + row * cell_h
    d.text((4, y + 34), caption, fill="#70808c")
    for col, name in enumerate(names):
        im = Image.open(SRC / f"{prefix}{name}{suffix}.png").convert("RGBA")
        im = im.resize((im.width * scale, im.height * scale), Image.Resampling.NEAREST)
        sheet.paste(im, (left + col * cell_w + (cell_w-im.width)//2,
                         y + (cell_h-im.height)//2), im)

moon_cell = 128
moon = Image.new("RGB", (8 * moon_cell, 2 * moon_cell + 22), "#020609")
md = ImageDraw.Draw(moon)
for i in range(8):
    md.text((i * moon_cell + 40, 5), str(i), fill="#a8b9c4")
    for row, prefix in enumerate(("", "n_")):
        im = Image.open(SRC / f"{prefix}moon_{i}.png").convert("RGBA")
        im = im.resize((im.width * scale, im.height * scale), Image.Resampling.NEAREST)
        moon.paste(im, (i * moon_cell + (moon_cell-im.width)//2,
                        22 + row*moon_cell + (moon_cell-im.height)//2), im)

OUT.mkdir(parents=True, exist_ok=True)
sheet.save(OUT / "dso_glyphs_preview.png")
moon.save(OUT / "moon_phases_preview.png")
print(OUT)
