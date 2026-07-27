"""Export DSO glyph masters and labeled design-reference sheets.

The firmware assets are 18x18. gen_assets_v9.py draws them at 4x resolution
before LANCZOS downsampling, so this tool preserves those real 72x72 procedural
masters and presents both resolutions without inventing new artwork.
"""
from pathlib import Path
import importlib.util
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
FINAL = ROOT / "assets" / "screen_png"
MASTER = ROOT / "assets" / "master_png"
SHEETS = ROOT / "assets" / "glyph_sheets"
MASTER.mkdir(parents=True, exist_ok=True)
SHEETS.mkdir(parents=True, exist_ok=True)

spec = importlib.util.spec_from_file_location("gen_assets_v9", ROOT / "tools" / "gen_assets_v9.py")
gen = importlib.util.module_from_spec(spec)
spec.loader.exec_module(gen)

TYPES = [
    (0, "Elliptical galaxy", "E"),
    (1, "Elliptical / spiral transition", "E–S0"),
    (2, "Lenticular galaxy", "S0"),
    (3, "Lenticular / early spiral", "S0–Aa"),
    (4, "Unbarred spiral galaxy", "SA"),
    (5, "Barred spiral galaxy", "SB"),
    (6, "Intermediate spiral galaxy", "SAB"),
    (7, "Irregular galaxy", "Irr"),
    (8, "Galaxy — morphology unknown", "GAL"),
    (9, "Galaxy pair", "PAIR"),
    (10, "Galaxy triplet", "TRIPLET"),
    (11, "Galaxy group", "GROUP"),
    (12, "Emission nebula", "EM"),
    (13, "H II ionized region", "HII"),
    (14, "Reflection nebula", "REFL"),
    (15, "Dark nebula", "DARK"),
    (16, "Planetary nebula", "PN"),
    (17, "Supernova remnant", "SNR"),
    (18, "Star cluster + nebula", "CL+N"),
    (19, "Globular cluster", "GC"),
    (20, "Open cluster", "OC"),
    (21, "Generic / unclassified nebula", "NEB"),
]

BG = "#03070a"
PANEL = "#08131a"
EDGE = "#20323d"
TEXT = "#edf4f4"
DIM = "#8da0a8"
CYAN = "#32d6ea"
RED = "#ff4c38"


def font(size, bold=False):
    choices = [
        Path("C:/Windows/Fonts/arialbd.ttf" if bold else "C:/Windows/Fonts/arial.ttf"),
        Path("C:/Windows/Fonts/DejaVuSans-Bold.ttf" if bold else "C:/Windows/Fonts/DejaVuSans.ttf"),
    ]
    for path in choices:
        if path.exists():
            return ImageFont.truetype(str(path), size)
    return ImageFont.load_default()


F12, F15, F19, F24, B16, B22, B28 = (
    font(12), font(15), font(19), font(24), font(16, True), font(22, True), font(28, True)
)


def center(d, x, y, text, f, color):
    box = d.textbbox((0, 0), text, font=f)
    d.text((x - (box[2] - box[0]) / 2, y), text, font=f, fill=color)


def export_masters():
    masters = {}
    for idx, _, _ in TYPES:
        fn = gen.DRAW[idx]
        name = gen.NAMES[idx]
        accent = gen.DAY_ACCENT[idx]
        hi = tuple(min(255, int(v * 1.25 + 45)) for v in accent)
        for night in (False, True):
            for filled in (False, True):
                prefix = "n_" if night else ""
                suffix = "_f" if filled else ""
                if night:
                    raw = fn(filled, gen.NRED, gen.NRED_HI)
                    image = gen.glowize(raw, gen.NRED, 3, 0.58 if filled else 0.28)
                else:
                    raw = fn(filled, accent, hi)
                    image = gen.glowize(raw, accent, 3, 0.62 if filled else 0.30)
                path = MASTER / f"{prefix}obj_{name}{suffix}_72.png"
                image.save(path)
                masters[(idx, night, filled)] = image
    return masters


def composite_on_bg(canvas, image, xy, size=None, resample=Image.Resampling.LANCZOS):
    icon = image
    if size is not None:
        icon = icon.resize((size, size), resample)
    canvas.paste(icon, xy, icon)


def final_sheet(night):
    """Runtime sheet: literal 18x18 plus nearest-neighbor pixel inspection."""
    title_color = RED if night else CYAN
    width, height = 1500, 1670
    sheet = Image.new("RGB", (width, height), BG)
    d = ImageDraw.Draw(sheet)
    palette = "RED NIGHT" if night else "FULL COLOR DAY"
    d.text((32, 24), f"DSO GLYPHS — FINAL FIRMWARE RESOLUTION — {palette}", font=B28, fill=TEXT)
    d.text((32, 62), "Every source glyph is exactly 18 × 18 pixels. Pixel zoom uses nearest-neighbor only.", font=F19, fill=DIM)
    d.text((900, 64), "OUTLINE = inactive     FILLED = active/selected", font=F15, fill=title_color)
    for n, (idx, label, code) in enumerate(TYPES):
        col, row = n % 2, n // 2
        x, y = 24 + col * 738, 105 + row * 140
        d.rounded_rectangle((x, y, x + 714, y + 124), 9, fill=PANEL, outline=EDGE, width=2)
        d.text((x + 18, y + 14), code, font=B22, fill=title_color)
        d.text((x + 18, y + 48), label, font=F15, fill=TEXT)
        d.text((x + 18, y + 77), gen.NAMES[idx], font=F12, fill=DIM)
        for filled, ox, cap in [(False, 365, "OUTLINE"), (True, 535, "FILLED")]:
            prefix = "n_" if night else ""
            suffix = "_f" if filled else ""
            icon = Image.open(FINAL / f"{prefix}obj_{gen.NAMES[idx]}{suffix}.png").convert("RGBA")
            composite_on_bg(sheet, icon, (x + ox, y + 18), 90, Image.Resampling.NEAREST)
            composite_on_bg(sheet, icon, (x + ox + 112, y + 50))
            center(d, x + ox + 45, y + 106, f"{cap}  5× PIXEL VIEW", F12, DIM)
            center(d, x + ox + 121, y + 76, "1:1", F12, DIM)
    path = SHEETS / ("glyphs_final_red_18px.png" if night else "glyphs_final_color_18px.png")
    sheet.save(path)
    return path


def master_sheet(masters, night):
    """Master sheet: true pre-downsample 72x72 images shown at 1:1."""
    title_color = RED if night else CYAN
    width, height = 1500, 1670
    sheet = Image.new("RGB", (width, height), BG)
    d = ImageDraw.Draw(sheet)
    palette = "RED NIGHT" if night else "FULL COLOR DAY"
    d.text((32, 24), f"DSO GLYPHS — PROCEDURAL MASTER RESOLUTION — {palette}", font=B28, fill=TEXT)
    d.text((32, 62), "True 72 × 72 pre-downsample artwork, displayed here at native 1:1 scale.", font=F19, fill=DIM)
    d.text((900, 64), "OUTLINE = inactive     FILLED = active/selected", font=F15, fill=title_color)
    for n, (idx, label, code) in enumerate(TYPES):
        col, row = n % 2, n // 2
        x, y = 24 + col * 738, 105 + row * 140
        d.rounded_rectangle((x, y, x + 714, y + 124), 9, fill=PANEL, outline=EDGE, width=2)
        d.text((x + 18, y + 14), code, font=B22, fill=title_color)
        d.text((x + 18, y + 48), label, font=F15, fill=TEXT)
        d.text((x + 18, y + 77), gen.NAMES[idx], font=F12, fill=DIM)
        for filled, ox, cap in [(False, 410, "OUTLINE"), (True, 565, "FILLED")]:
            composite_on_bg(sheet, masters[(idx, night, filled)], (x + ox, y + 18))
            center(d, x + ox + 36, y + 96, f"{cap}  72×72", F12, DIM)
    path = SHEETS / ("glyphs_master_red_72px.png" if night else "glyphs_master_color_72px.png")
    sheet.save(path)
    return path


def overview(masters):
    width, height = 1160, 1840
    sheet = Image.new("RGB", (width, height), BG)
    d = ImageDraw.Draw(sheet)
    d.text((28, 22), "ALL 88 DSO GLYPHS — MASTER OVERVIEW", font=B28, fill=TEXT)
    d.text((28, 60), "22 object types × outline/filled × color/red — 72×72 native masters", font=F19, fill=DIM)
    headers = [(580, "COLOR OUTLINE", CYAN), (720, "COLOR FILLED", CYAN), (860, "RED OUTLINE", RED), (1000, "RED FILLED", RED)]
    for x, text, color in headers:
        center(d, x, 95, text, F12, color)
    for row, (idx, label, code) in enumerate(TYPES):
        y = 122 + row * 77
        if row % 2 == 0:
            d.rectangle((18, y - 2, width - 18, y + 74), fill=PANEL)
        d.text((30, y + 12), code, font=B16, fill=CYAN if idx < 12 else TEXT)
        d.text((125, y + 12), label, font=F15, fill=TEXT)
        d.text((125, y + 39), gen.NAMES[idx], font=F12, fill=DIM)
        for col, key in enumerate(((False, False), (False, True), (True, False), (True, True))):
            composite_on_bg(sheet, masters[(idx, key[0], key[1])], (544 + col * 140, y))
    path = SHEETS / "glyphs_all_88_master_overview.png"
    sheet.save(path)
    return path


def write_readme(paths):
    text = """# DSO glyph design sheets

The controller has 22 deep-sky object types. Each has an outline (inactive)
and filled (active/selected) form in full-color day and red-only night modes:
88 runtime glyphs total.

- `glyphs_final_color_18px.png` and `glyphs_final_red_18px.png` show the exact
  18×18 firmware images both at literal 1:1 size and as a 5× nearest-neighbor
  pixel inspection. The zoom does not smooth or alter the pixels.
- `glyphs_master_color_72px.png` and `glyphs_master_red_72px.png` show the true
  72×72 procedural artwork before firmware downsampling.
- `glyphs_all_88_master_overview.png` puts all variants on one comparison sheet.
- Individual transparent 72×72 masters are in `../master_png/`.

When designing replacements, work at 72×72, keep important strokes at least
4 master pixels wide, and test the final LANCZOS reduction at 18×18. Night-mode
types must remain distinguishable by silhouette and texture, not by hue.
"""
    (SHEETS / "README.md").write_text(text, encoding="utf-8")


def main():
    masters = export_masters()
    paths = [final_sheet(False), final_sheet(True), master_sheet(masters, False), master_sheet(masters, True), overview(masters)]
    write_readme(paths)
    print(f"Exported {len(masters)} transparent masters and {len(paths)} sheets")
    for path in paths:
        print(path)


if __name__ == "__main__":
    main()
