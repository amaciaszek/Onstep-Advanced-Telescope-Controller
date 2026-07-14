#!/usr/bin/env python3
"""
gen_assets.py v2 (day + night-vision sets) — procedurally generates every UI PNG at 4x supersampling with
neon-glow styling, then downsamples for crisp anti-aliased 22px icons.
Output: assets/screen_png/*.png  (same names/dims as the originals)
Rerun any time; tweak the STYLE block to restyle the whole UI at once.
"""
from PIL import Image, ImageDraw, ImageFilter, ImageFont
import math, os

S = 4  # supersample factor
OUT = "assets/screen_png"
os.makedirs(OUT, exist_ok=True)

# ---- STYLE ----------------------------------------------------------------
CYAN    = (23, 184, 255)
CYAN_HI = (140, 228, 255)
GREEN   = (99, 242, 58)
GREEN_HI= (190, 255, 160)
AMBER   = (255, 157, 32)
AMBER_HI= (255, 210, 130)
WHITE   = (242, 245, 247)
EDGE    = (23, 48, 62)
PANEL   = (4, 12, 18)
FONT_B  = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"

def canvas(w, h):
    return Image.new("RGBA", (w * S, h * S), (0, 0, 0, 0))

def glowize(img, color, blur=7, strength=1.6):
    """Neon: soft colored glow underneath the sharp artwork."""
    a = img.split()[3]
    glow = Image.new("RGBA", img.size, color + (0,))
    glow.putalpha(a)
    glow = glow.filter(ImageFilter.GaussianBlur(blur))
    # boost glow alpha
    r, g, b, ga = glow.split()
    ga = ga.point(lambda v: min(255, int(v * strength)))
    glow = Image.merge("RGBA", (r, g, b, ga))
    out = Image.new("RGBA", img.size, (0, 0, 0, 0))
    out.alpha_composite(glow)
    out.alpha_composite(img)
    return out

def finish(img, w, h, name):
    img = img.resize((w, h), Image.LANCZOS)
    img.save(f"{OUT}/{name}.png")
    print("wrote", name, f"{w}x{h}")

def L(d): return int(round(d * S))  # scale helper

# ---- 22x22 status icons ----------------------------------------------------
def icon_wifi():
    img = canvas(22, 22); d = ImageDraw.Draw(img)
    cx, cy = L(11), L(17)
    for i, r in enumerate([L(4), L(8), L(12)]):
        d.arc([cx-r, cy-r, cx+r, cy+r], 225, 315, fill=CYAN_HI, width=int(L(2)))
    d.ellipse([cx-L(1.6), cy-L(1.6), cx+L(1.6), cy+L(1.6)], fill=CYAN_HI)
    finish(glowize(img, CYAN), 22, 22, "icon_wifi")

def icon_link():
    img = canvas(22, 22); d = ImageDraw.Draw(img)
    # two interlocked rounded links, diagonal
    d.rounded_rectangle([L(2), L(7), L(12), L(15)], radius=L(4), outline=CYAN_HI, width=int(L(2)))
    d.rounded_rectangle([L(10), L(7), L(20), L(15)], radius=L(4), outline=GREEN_HI, width=int(L(2)))
    img = img.rotate(-30, resample=Image.BICUBIC, center=(L(11), L(11)))
    finish(glowize(img, CYAN), 22, 22, "icon_link")

def _padlock(open_):
    img = canvas(22, 22); d = ImageDraw.Draw(img)
    col_hi = GREEN_HI if open_ else AMBER_HI
    col    = GREEN if open_ else AMBER
    d.rounded_rectangle([L(5), L(10), L(17), L(19)], radius=L(2), outline=col_hi, width=int(L(2)))
    d.ellipse([L(10), L(13), L(12.6), L(15.6)], fill=col_hi)   # keyhole
    if open_:
        d.arc([L(9), L(1), L(19), L(11)], 270, 90, fill=col_hi, width=int(L(2)))   # shackle swung right
        d.line([L(9), L(6), L(9), L(10)], fill=col_hi, width=int(L(2)))
    else:
        d.arc([L(6.5), L(2), L(15.5), L(11)], 180, 360, fill=col_hi, width=int(L(2)))
        d.line([L(6.5), L(6.5), L(6.5), L(10)], fill=col_hi, width=int(L(2)))
        d.line([L(15.5), L(6.5), L(15.5), L(10)], fill=col_hi, width=int(L(2)))
    return glowize(img, col)

def icon_park():
    finish(_padlock(False), 22, 22, "icon_park_locked")
    finish(_padlock(True), 22, 22, "icon_park_unlocked")

def icon_guide():
    img = canvas(22, 22); d = ImageDraw.Draw(img)
    cx = cy = L(11); r = L(7)
    d.ellipse([cx-r, cy-r, cx+r, cy+r], outline=GREEN_HI, width=int(L(1.6)))
    for a in range(4):  # crosshair ticks outside+inside
        dx, dy = [(0,-1),(1,0),(0,1),(-1,0)][a]
        d.line([cx+dx*L(4.5), cy+dy*L(4.5), cx+dx*L(9.5), cy+dy*L(9.5)], fill=GREEN_HI, width=int(L(1.8)))
    d.ellipse([cx-L(1.4), cy-L(1.4), cx+L(1.4), cy+L(1.4)], fill=GREEN_HI)
    finish(glowize(img, GREEN), 22, 22, "icon_guide")

def icon_battery():
    img = canvas(22, 22); d = ImageDraw.Draw(img)
    d.rounded_rectangle([L(2), L(7), L(18), L(15)], radius=L(1.5), outline=GREEN_HI, width=int(L(1.6)))
    d.rounded_rectangle([L(18.5), L(9.5), L(20.5), L(12.5)], radius=L(0.8), fill=GREEN_HI)
    for i in range(3):
        x = L(4.2) + i * L(4.4)
        d.rounded_rectangle([x, L(8.8), x + L(3.2), L(13.2)], radius=L(0.6), fill=GREEN)
    finish(glowize(img, GREEN), 22, 22, "icon_battery")

def icon_temperature():
    img = canvas(22, 22); d = ImageDraw.Draw(img)
    d.rounded_rectangle([L(9), L(2), L(13), L(13)], radius=L(2), outline=WHITE, width=int(L(1.6)))
    d.ellipse([L(7), L(12), L(15), L(20)], outline=WHITE, width=int(L(1.6)))
    d.ellipse([L(9), L(14), L(13), L(18)], fill=(255, 90, 70))
    d.line([L(11), L(8), L(11), L(15)], fill=(255, 90, 70), width=int(L(2)))
    for y in (4.5, 7, 9.5):
        d.line([L(14), L(y), L(16), L(y)], fill=WHITE, width=int(L(1)))
    finish(glowize(img, (255, 120, 90), strength=1.1), 22, 22, "icon_temperature")

def icon_slew():
    img = canvas(22, 22); d = ImageDraw.Draw(img)
    cx, cy, r = L(11), L(13), L(8.5)
    d.arc([cx-r, cy-r, cx+r, cy+r], 150, 390, fill=CYAN_HI, width=int(L(2)))
    for a in (150, 210, 270, 330, 30):
        ar = math.radians(a)
        d.line([cx+math.cos(ar)*r*0.75, cy+math.sin(ar)*r*0.75,
                cx+math.cos(ar)*r*0.95, cy+math.sin(ar)*r*0.95], fill=CYAN_HI, width=int(L(1.2)))
    na = math.radians(310)
    d.line([cx, cy, cx+math.cos(na)*r*0.7, cy+math.sin(na)*r*0.7], fill=WHITE, width=int(L(1.8)))
    d.ellipse([cx-L(1.4), cy-L(1.4), cx+L(1.4), cy+L(1.4)], fill=WHITE)
    finish(glowize(img, CYAN), 22, 22, "icon_slew")

# ---- 15x15 nav icons --------------------------------------------------------
def nav_icons():
    def base(): return canvas(15, 15)
    # home
    img = base(); d = ImageDraw.Draw(img)
    d.polygon([L(7.5), L(1.5), L(13.5), L(7), L(12), L(7), L(12), L(13), L(3), L(13), L(3), L(7), L(1.5), L(7)],
              outline=CYAN_HI, width=int(L(1.4)))
    d.rectangle([L(6.2), L(9), L(8.8), L(13)], outline=CYAN_HI, width=int(L(1.2)))
    finish(glowize(img, CYAN, blur=5), 15, 15, "nav_home")
    # goto: target + arrow
    img = base(); d = ImageDraw.Draw(img)
    d.ellipse([L(3), L(3), L(12), L(12)], outline=CYAN_HI, width=int(L(1.4)))
    d.ellipse([L(6.4), L(6.4), L(8.6), L(8.6)], fill=CYAN_HI)
    d.line([L(11), L(4), L(14), L(1)], fill=WHITE, width=int(L(1.6)))
    d.polygon([L(14), L(1), L(11.4), L(1.6), L(13.4), L(3.6)], fill=WHITE)
    finish(glowize(img, CYAN, blur=5), 15, 15, "nav_goto")
    # align: star + crosshair
    img = base(); d = ImageDraw.Draw(img)
    cx = cy = L(7.5)
    for a in range(4):
        dx, dy = [(0,-1),(1,0),(0,1),(-1,0)][a]
        d.line([cx+dx*L(3), cy+dy*L(3), cx+dx*L(6.5), cy+dy*L(6.5)], fill=CYAN_HI, width=int(L(1.4)))
    pts = []
    for i in range(8):
        r = L(2.6) if i % 2 == 0 else L(1.1)
        a = math.pi/4*i - math.pi/2
        pts += [cx+math.cos(a)*r, cy+math.sin(a)*r]
    d.polygon(pts, fill=WHITE)
    finish(glowize(img, CYAN, blur=5), 15, 15, "nav_align")
    # menu
    img = base(); d = ImageDraw.Draw(img)
    for i, y in enumerate((4, 7.5, 11)):
        d.rounded_rectangle([L(2.5), L(y-0.9), L(12.5), L(y+0.9)], radius=L(0.9), fill=CYAN_HI)
    finish(glowize(img, CYAN, blur=5), 15, 15, "nav_menu")

# ---- radar frame 73x73: decluttered, bigger cardinals, corner brackets -----
def radar_frame():
    img = canvas(73, 73); d = ImageDraw.Draw(img)
    cx = cy = L(36.5)
    # dark disc
    d.ellipse([cx-L(33), cy-L(33), cx+L(33), cy+L(33)], fill=PANEL + (235,))
    # single bright outer ring
    d.ellipse([cx-L(33), cy-L(33), cx+L(33), cy+L(33)], outline=CYAN, width=int(L(1.6)))
    # one faint mid ring (45 deg altitude)
    d.ellipse([cx-L(16.5), cy-L(16.5), cx+L(16.5), cy+L(16.5)], outline=EDGE, width=int(L(1)))
    # faint cross
    d.line([cx-L(31), cy, cx+L(31), cy], fill=EDGE, width=int(L(1)))
    d.line([cx, cy-L(31), cx, cy+L(31)], fill=EDGE, width=int(L(1)))
    # zenith dot
    d.ellipse([cx-L(1), cy-L(1), cx+L(1), cy+L(1)], fill=EDGE)
    # 30-degree azimuth ticks on outer ring
    for a in range(0, 360, 30):
        if a % 90 == 0: continue
        ar = math.radians(a)
        d.line([cx+math.cos(ar)*L(30.5), cy+math.sin(ar)*L(30.5),
                cx+math.cos(ar)*L(33), cy+math.sin(ar)*L(33)], fill=CYAN, width=int(L(1)))
    img = glowize(img, CYAN, blur=6, strength=0.9)
    # cardinals: bigger, brighter, N highlighted
    d = ImageDraw.Draw(img)
    f = ImageFont.truetype(FONT_B, L(9))
    for txt, (x, y), col in [("N", (36.5, 5.5), WHITE), ("S", (36.5, 67.5), CYAN_HI),
                             ("E", (5.5, 36.5), CYAN_HI), ("W", (67.5, 36.5), CYAN_HI)]:
        bb = d.textbbox((0, 0), txt, font=f)
        d.text((L(x)-(bb[2]-bb[0])/2, L(y)-(bb[3]-bb[1])/2-bb[1]), txt, font=f, fill=col)
    finish(img, 73, 73, "sky_radar_frame")

# ---- panels -----------------------------------------------------------------
def tech_panel(w, h, name, corner=True):
    img = canvas(w, h); d = ImageDraw.Draw(img)
    r = L(6)
    # vertical subtle gradient fill
    top, bot = (7, 18, 26), (3, 8, 12)
    for y in range(L(h)):
        t = y / L(h)
        col = tuple(int(top[i]*(1-t) + bot[i]*t) for i in range(3))
        d.line([L(1), y, L(w-1), y], fill=col + (215,))
    mask = Image.new("L", img.size, 0)
    ImageDraw.Draw(mask).rounded_rectangle([0, 0, L(w)-1, L(h)-1], radius=r, fill=255)
    img.putalpha(mask)
    d = ImageDraw.Draw(img)
    d.rounded_rectangle([L(0.5), L(0.5), L(w-0.5), L(h-0.5)], radius=r, outline=EDGE, width=int(L(1)))
    if corner:  # bright corner accent brackets, HUD style
        cl = L(min(8, w // 6))
        for (x0, y0, dx, dy) in [(1.2, 1.2, 1, 1), (w-1.2, 1.2, -1, 1), (1.2, h-1.2, 1, -1), (w-1.2, h-1.2, -1, -1)]:
            d.line([L(x0), L(y0), L(x0)+cl*dx, L(y0)], fill=CYAN, width=int(L(1.2)))
            d.line([L(x0), L(y0), L(x0), L(y0)+cl*dy], fill=CYAN, width=int(L(1.2)))
    finish(glowize(img, CYAN, blur=4, strength=0.5), w, h, name)

def meridian_horizon():
    # kept for compatibility; simple horizon curve, unused by v6 home screen
    img = canvas(197, 20); d = ImageDraw.Draw(img)
    d.arc([L(-60), L(6), L(257), L(150)], 245, 295, fill=EDGE, width=int(L(2)))
    d.line([L(98.5), L(2), L(98.5), L(12)], fill=AMBER, width=int(L(1.4)))
    finish(glowize(img, CYAN, blur=4, strength=0.5), 197, 20, "meridian_horizon")

if __name__ == "__main__":
    icon_wifi(); icon_link(); icon_park(); icon_guide()
    icon_battery(); icon_temperature(); icon_slew()
    nav_icons(); radar_frame()
    tech_panel(75, 25, "panel_metric")
    tech_panel(76, 17, "panel_button")
    tech_panel(237, 67, "panel_long")
    meridian_horizon()
    print("all assets generated")
