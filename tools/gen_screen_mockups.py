"""Generate 320x170 reference mockups for the controller user guide.

These are visual targets, not screenshots from LVGL.  They deliberately use
the same dimensions, content hierarchy, palette, and generated icon assets as
the firmware so photographs of the physical display are easy to compare.
"""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
import math

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "docs" / "screen_mockups"
ASSETS = ROOT / "assets" / "screen_png"
OUT.mkdir(parents=True, exist_ok=True)

W, H = 320, 170
BG = "#02070b"
PANEL = "#07121a"
EDGE = "#173143"
CYAN = "#27d7ef"
GREEN = "#5ee58c"
AMBER = "#ffba4a"
RED = "#ff4a45"
TEXT = "#e9f4f5"
DIM = "#78909b"


def font(size, bold=False):
    candidates = [
        Path("C:/Windows/Fonts/DejaVuSansCondensed-Bold.ttf" if bold else "C:/Windows/Fonts/DejaVuSansCondensed.ttf"),
        Path("C:/Windows/Fonts/arialbd.ttf" if bold else "C:/Windows/Fonts/arial.ttf"),
    ]
    for p in candidates:
        if p.exists():
            return ImageFont.truetype(str(p), size)
    return ImageFont.load_default()


F8, F9, F10, F12, F14, F18 = [font(n) for n in (8, 9, 10, 12, 14, 18)]
B10, B12, B14, B18 = [font(n, True) for n in (10, 12, 14, 18)]


def base(title, right=""):
    im = Image.new("RGB", (W, H), BG)
    d = ImageDraw.Draw(im)
    box(d, (3, 3, 316, 25), 6)
    d.text((10, 7), title, font=B12, fill=CYAN)
    if right:
        right_text(d, 310, 8, right, F9, DIM)
    return im, d


def box(d, xy, radius=5, fill=PANEL, outline=EDGE, width=1):
    d.rounded_rectangle(xy, radius, fill=fill, outline=outline, width=width)


def right_text(d, x, y, text, f, fill):
    b = d.textbbox((0, 0), text, font=f)
    d.text((x - (b[2] - b[0]), y), text, font=f, fill=fill)


def center_text(d, x, y, text, f, fill):
    b = d.textbbox((0, 0), text, font=f)
    d.text((x - (b[2] - b[0]) / 2, y), text, font=f, fill=fill)


def footer(d, text):
    box(d, (4, 138, 316, 165), 5)
    center_text(d, 160, 147, text, F9, DIM)


def glyph(im, name, x, y, size=None):
    p = ASSETS / name
    if not p.exists():
        return
    icon = Image.open(p).convert("RGBA")
    if size:
        icon = icon.resize((size, size), Image.Resampling.LANCZOS)
    im.alpha_composite(icon, (x, y)) if im.mode == "RGBA" else im.paste(icon, (x, y), icon)


def home():
    im = Image.new("RGB", (W, H), BG); d = ImageDraw.Draw(im)
    box(d, (3, 2, 316, 25), 7, BG)
    for name, x in [("icon_wifi.png", 7), ("icon_link.png", 38), ("icon_park_unlocked.png", 71), ("icon_guide.png", 104), ("icon_battery.png", 143)]:
        glyph(im, name, x, 3)
    d.text((165, 5), "62%", font=F12, fill=TEXT)
    glyph(im, "icon_slew.png", 270, 3); d.text((294, 5), "4x", font=F12, fill=TEXT)
    d.rectangle((3, 27, 316, 28), fill=GREEN)
    box(d, (3, 31, 237, 87), 8)
    d.text((10, 38), "RA", font=F10, fill=CYAN); right_text(d, 166, 34, "05h 35m", F18, TEXT); d.text((172, 40), "17.3s", font=F12, fill=TEXT)
    d.line((10, 59, 230, 59), fill=EDGE)
    d.text((10, 66), "DEC", font=F10, fill=CYAN); right_text(d, 166, 62, "-05° 23'", F18, TEXT); d.text((172, 68), "28\"", font=F12, fill=TEXT)
    box(d, (244, 29, 316, 102), 8)
    d.ellipse((251, 36, 309, 94), outline=EDGE, width=1); d.line((280, 36, 280, 94), fill=EDGE); d.line((251, 65, 309, 65), fill=EDGE)
    pts = [(259, 81), (266, 72), (275, 64), (285, 55), (300, 48)]
    d.line(pts, fill=CYAN, width=2); d.ellipse((272, 61, 280, 69), fill=GREEN, outline=TEXT)
    for x, title, val in [(3, "ALT", "48.2°"), (82, "AZ", "127.6°"), (161, "PIER", "E")]:
        box(d, (x, 92, x + 76, 114), 5); d.text((x + 6, 98), title, font=F9, fill=CYAN); right_text(d, x + 70, 97, val, F10, TEXT)
    box(d, (3, 119, 237, 146), 8); d.text((8, 127), "E", font=F10, fill=CYAN); center_text(d, 98, 121, "M", F8, DIM); d.text((180, 127), "W", font=F10, fill=CYAN)
    d.line((20, 133, 176, 133), fill=EDGE, width=2); d.ellipse((89, 129, 97, 137), fill=CYAN); right_text(d, 232, 121, "T-MER", F8, DIM); right_text(d, 232, 132, "+01:14", F10, AMBER)
    box(d, (244, 106, 316, 146), 6); d.text((250, 110), "NET", font=F8, fill=CYAN); d.text((250, 120), "-54 dBm", font=F9, fill=TEXT); d.text((250, 132), "38ms 99%", font=F9, fill=GREEN)
    for i, label in enumerate(("HOME", "SKY", "CAT", "SET")):
        x = 3 + i * 79; box(d, (x, 151, x + 76, 167), 5); center_text(d, x + 38, 154, label, F8, CYAN if i == 0 else DIM)
    return im


def sky():
    im, d = base("SKY", "LST 22:41")
    d.text((10, 31), "-12H", font=F8, fill=DIM); center_text(d, 160, 31, "NOW", F8, CYAN); right_text(d, 310, 31, "+12H", F8, DIM)
    colors = ["#121d3b"] * 8 + ["#263e78"] * 6 + ["#b05d31"] * 4 + ["#dba444"] * 4 + ["#eee0ad"] * 4 + ["#dba444"] * 4 + ["#b05d31"] * 4 + ["#263e78"] * 6 + ["#121d3b"] * 8
    for i, c in enumerate(colors[:48]): d.rectangle((10 + i * 6.25, 42, 16 + i * 6.25, 54), fill=c)
    box(d, (10, 62, 206, 112), 6); d.text((18, 66), "ASTRONOMICAL NIGHT", font=B12, fill=GREEN); d.text((18, 82), "DAWN IN 04:18:32", font=B14, fill=TEXT); d.text((18, 100), "SUN -23.4°   DARK ENDS 04:07", font=F8, fill=DIM)
    box(d, (212, 62, 310, 112), 6); glyph(im, "moon_2.png", 218, 71); d.text((252, 66), "36%", font=B12, fill=TEXT); d.text((252, 84), "WAX", font=F8, fill=DIM); d.text((252, 98), "SET 02:14", font=F8, fill=AMBER)
    box(d, (10, 116, 310, 133), 3); d.text((16, 119), "MERIDIAN FLIP", font=F8, fill=DIM); right_text(d, 304, 119, "01:12:45", F9, TEXT)
    footer(d, "A SET DATE")
    return im


def catalog_filter():
    im, d = base("CATALOG", "FILTER")
    cx, cy, r = 52, 83, 42
    for i in range(8):
        a0 = math.radians(i * 45 - 90); a1 = math.radians((i + 1) * 45 - 90)
        poly = [(cx, cy), (cx + r * math.cos(a0), cy + r * math.sin(a0)), (cx + r * math.cos(a1), cy + r * math.sin(a1))]
        d.polygon(poly, fill="#0d2938" if i not in (2, 3) else "#116276", outline=EDGE)
    d.ellipse((43, 74, 61, 92), fill="#153d36", outline=GREEN); center_text(d, cx, 35, "N", F8, TEXT); center_text(d, cx, 126, "S", F8, TEXT); d.text((98, 79), "E", font=F8, fill=TEXT); d.text((1, 79), "W", font=F8, fill=TEXT)
    chips = [("GALAXY", "obj_gal_sa.png"), ("PLANETARY", "obj_planetary.png"), ("GLOBULAR", "obj_globular.png"), ("NEBULA", "obj_emission.png"), ("CLUSTER", "obj_opencluster.png"), ("REMNANT", "obj_remnant.png")]
    for i, (label, icon) in enumerate(chips):
        x = 116 + (i % 2) * 99; y = 39 + (i // 2) * 20; box(d, (x, y, x + 96, y + 17), 3); glyph(im, icon, x + 1, y, 16); d.text((x + 20, y + 3), label, font=F8, fill=TEXT if i < 4 else DIM)
    box(d, (116, 100, 311, 132), 4); d.text((124, 101), "842", font=B18, fill=CYAN); d.text((124, 120), "OBJECTS", font=F8, fill=DIM); right_text(d, 304, 103, "ALT 25°", F9, TEXT); right_text(d, 304, 118, "ZEN 70°", F8, GREEN)
    footer(d, "A TOGGLE   Y PRESET   B LIST")
    return im


def catalog_list():
    im, d = base("CATALOG", "842  ALTITUDE")
    rows = [("M31", "ANDROMEDA", "3.4", "190'", "obj_gal_es0.png"), ("M42", "ORION NEBULA", "4.0", "85'", "obj_emission.png"), ("M13", "HERCULES", "5.8", "20'", "obj_globular.png"), ("NGC 7000", "N AMERICA", "4.0", "120'", "obj_hii.png"), ("M57", "RING NEBULA", "8.8", "1.4'", "obj_planetary.png")]
    for i, row in enumerate(rows):
        y = 38 + i * 19; box(d, (4, y, 312, y + 17), 2, "#0c1c25" if i == 0 else PANEL, CYAN if i == 0 else EDGE, 2 if i == 0 else 1); glyph(im, row[4], 7, y, 16); d.text((28, y + 2), row[0], font=F9, fill=TEXT); d.text((102, y + 3), row[1], font=F8, fill=DIM); right_text(d, 248, y + 3, row[2], F8, TEXT); right_text(d, 295, y + 3, row[3], F8, DIM); d.rectangle((302, y + 4, 305, y + 14), fill=GREEN)
    footer(d, "A DETAIL   Y SORT   B FILTER")
    return im


def catalog_detail():
    im, d = base("CATALOG", "M31")
    box(d, (10, 39, 114, 115), 0); d.rectangle((27, 55, 96, 99), outline=DIM); d.ellipse((38, 67, 86, 87), outline=CYAN, width=2); d.line((62, 55, 62, 99), fill=EDGE); d.line((27, 77, 96, 77), fill=EDGE); center_text(d, 62, 118, "FOV 2.0° × 1.3°", F8, DIM)
    box(d, (118, 39, 310, 115), 6); glyph(im, "obj_gal_es0_f.png", 123, 43, 20); d.text((148, 43), "M31", font=B14, fill=TEXT); d.text((148, 60), "ELLIPTICAL / SPIRAL", font=F8, fill=DIM); d.text((126, 76), "ANDROMEDA GALAXY", font=B10, fill=CYAN); d.text((126, 91), "MAG 3.4   SB 13.5", font=F9, fill=TEXT); d.text((126, 104), "190' × 60'   ALT 47°", font=F9, fill=TEXT)
    box(d, (64, 119, 256, 134), 3, "#2b1b08", AMBER); center_text(d, 160, 121, "PRESS A AGAIN TO CONFIRM GOTO", F8, AMBER)
    footer(d, "A CONFIRM GOTO   B CANCEL")
    return im


def settings():
    im, d = base("SETTINGS", "A/SELECT CHANGE")
    rows = [("RADAR WIDGET", "PULSE GRAPH"), ("NIGHT MODE", "OFF"), ("BRIGHTNESS", "25%"), ("TEMPERATURE", "HIDDEN"), ("WI-FI SETUP", "PROFILES >")]
    for i, (name, val) in enumerate(rows):
        y = 30 + i * 22; selected = i == 2; box(d, (10, y, 310, y + 19), 4, "#0d2938" if selected else PANEL, CYAN if selected else EDGE, 2 if selected else 1); d.text((18, y + 4), name, font=F8, fill=CYAN if selected else DIM); right_text(d, 300, y + 3, val, F10, TEXT)
    footer(d, "STICK SELECT   A/SELECT CHANGE")
    return im


def profiles():
    im, d = base("ONSTEP PROFILES", "A EDIT  SELECT USE")
    rows = [("PROFILE 1  ACTIVE", "OBSERVATORY"), ("PROFILE 2", "TRAVEL-MOUNT"), ("PROFILE 3", "(empty)")]
    for i, (name, ssid) in enumerate(rows):
        y = 36 + i * 33; box(d, (10, y, 310, y + 27), 5, "#0d2938" if i == 0 else PANEL, CYAN if i == 0 else EDGE, 2 if i == 0 else 1); d.text((18, y + 5), name, font=F8, fill=CYAN if i == 0 else DIM); right_text(d, 300, y + 5, ssid, F10, TEXT if ssid != "(empty)" else DIM)
    footer(d, "STICK SELECT   A EDIT   SELECT ACTIVATE")
    return im


def keyboard():
    im, d = base("WI-FI NAME", "OBSERVATORY_")
    chars = "abcdefghijklmnopqrstuvwxyz0123456789.-_@"
    for i in range(40):
        x = 5 + (i % 10) * 31; y = 29 + (i // 10) * 25; selected = i == 5; box(d, (x, y, x + 29, y + 23), 3, "#0d3846" if selected else PANEL, CYAN if selected else EDGE, 2 if selected else 1); center_text(d, x + 14.5, y + 4, chars[i], F12, TEXT)
    for i, cmd in enumerate(("CASE", "SPACE", "<", "NEXT", "EXIT")):
        x = 5 + i * 63; box(d, (x, 132, x + 59, 164), 4); center_text(d, x + 29.5, 142, cmd, F8, TEXT)
    return im


def diagnostics():
    im, d = base("DIAGNOSTICS", "START NEXT")
    box(d, (3, 28, 316, 166), 8)
    lines = [("SSID", "OBSERVATORY"), ("IP", "192.168.0.42 → 192.168.0.1:9999"), ("RSSI", "-54 dBm   Wi-Fi UP"), ("POLL", "420 ms   cycle 2.10 s"), ("CMDS", "ok 1842   fail 0   last 38 ms"), ("HEAP", "89432 free   up 03:17:41")]
    for i, (k, v) in enumerate(lines):
        y = 36 + i * 19; d.text((12, y), k, font=F9, fill=CYAN); d.text((55, y), v, font=F9, fill=TEXT if i != 4 else GREEN)
    return im


SCREENS = {
    "01_home": home,
    "02_sky": sky,
    "03_catalog_filter": catalog_filter,
    "04_catalog_list": catalog_list,
    "05_catalog_detail": catalog_detail,
    "06_settings": settings,
    "07_profiles": profiles,
    "08_keyboard": keyboard,
    "09_diagnostics": diagnostics,
}


def main():
    made = []
    for name, fn in SCREENS.items():
        image = fn()
        path = OUT / f"{name}.png"
        image.save(path)
        made.append((name.replace("_", " ").upper(), image))

    sheet = Image.new("RGB", (1000, 650), "#081015")
    d = ImageDraw.Draw(sheet)
    d.text((20, 12), "ONSTEP HAND CONTROLLER — IDEAL SCREEN REFERENCE", font=font(20, True), fill=TEXT)
    d.text((20, 39), "Each panel below is an exact 320 × 170 render target", font=font(12), fill=DIM)
    for i, (label, image) in enumerate(made):
        col, row = i % 3, i // 3
        x, y = 10 + col * 330, 70 + row * 190
        sheet.paste(image, (x, y))
        center_text(d, x + 160, y + 173, label, F10, DIM)
    sheet.save(OUT / "screen_reference_sheet.png")
    print(f"Generated {len(made)} screens and contact sheet in {OUT}")


if __name__ == "__main__":
    main()
