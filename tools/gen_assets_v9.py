#!/usr/bin/env python3
"""
gen_assets_v9.py -- object-type glyphs, moon phases and the SKY nav icon.

v9.1: the type set matches the reference art sheet. Galaxies carry their
morphology (elliptical, lenticular, the spiral families, irregular) plus the
pair/triplet/group systems, and the nebulae split into emission, HII,
reflection, dark, planetary and generic. 22 types, drawn to read at 16-32 px.

House rules (unchanged):
  * procedural, 4x supersample, LANCZOS down
  * day set = full colour; night set = single red channel, prefix n_
  * OUTLINE = inactive, FILLED (_f) = active
  * night types separate by SILHOUETTE + INTERIOR TEXTURE, never by hue

The morphology has to survive being shrunk to 18 px, so each glyph leans on
its single most recognisable feature:
    E        smooth featureless oval, bright core
    E-S0     oval with one faint lens ring
    S0       edge-on lens: thin bright spindle
    S0-Aa    lens with a dust lane
    SA       two-arm open spiral
    SB       bar with arms springing from its ends
    SAB      bar-ish core, tighter arms
    Irr      scattered asymmetric clumps
    generic  plain tilted oval (morphology unknown)
    pair/triplet/group   2 / 3 / 5 small ovals
    emission smooth filled cloud with filaments
    HII      cloud with embedded bright knots
    reflection  cloud with one dominant illuminating star
    dark     a dark lane occluding a faint star field
    planetary  ring / annulus
    remnant  torn open shell
    cluster+neb  star scatter inside a soft cloud
    globular  dense, central-condensed stipple
    open cluster  sparse discrete stars
    nebula   generic soft blob
"""
from PIL import Image, ImageDraw, ImageFilter
import math, os

S   = 4
OUT = "assets/screen_png"
os.makedirs(OUT, exist_ok=True)

BLUE    = (150, 190, 255)
GOLD    = (255, 205, 120)
CYAN    = (120, 230, 245)
CYAN_HI = (200, 250, 255)
RED_EM  = (255, 90, 90)
PINK    = (255, 130, 200)
VIOLET  = (190, 150, 255)
DIM     = (120, 130, 150)

NRED    = (235, 40, 22)
NRED_HI = (255, 90, 60)
NRED_LO = (110, 16, 8)

GLYPH, MOON, NAV = 18, 30, 26

DAY_ACCENT = {
    0: GOLD, 1: GOLD, 2: GOLD, 3: GOLD,
    4: BLUE, 5: BLUE, 6: BLUE, 7: BLUE,
    8: BLUE, 9: BLUE, 10: BLUE, 11: BLUE,
    12: RED_EM, 13: PINK, 14: BLUE, 15: DIM,
    16: CYAN, 17: VIOLET, 18: VIOLET,
    19: GOLD, 20: CYAN_HI, 21: PINK,
}


def canvas(w, h):
    return Image.new("RGBA", (w * S, h * S), (0, 0, 0, 0))


def L(v):
    return int(round(v * S))


def glowize(img, color, blur=4, strength=1.2):
    a = img.split()[3]
    glow = Image.new("RGBA", img.size, color + (0,))
    glow.putalpha(a)
    glow = glow.filter(ImageFilter.GaussianBlur(blur))
    r, g, b, ga = glow.split()
    ga = ga.point(lambda v: min(255, int(v * strength)))
    glow = Image.merge("RGBA", (r, g, b, ga))
    out = Image.new("RGBA", img.size, (0, 0, 0, 0))
    out.alpha_composite(glow)
    out.alpha_composite(img)
    return out


def finish(img, w, h, name):
    img.resize((w, h), Image.LANCZOS).save(f"{OUT}/{name}.png")
    return name


def clip_to(img, mask):
    out = Image.new("RGBA", img.size, (0, 0, 0, 0))
    out.paste(img, (0, 0), mask.split()[3])
    return out


def ellipse_mask(size, box):
    img = Image.new("RGBA", size, (0, 0, 0, 0))
    ImageDraw.Draw(img).ellipse(box, fill=(255, 255, 255, 255))
    return img


def stipple(size, color, count, radius, seed=7, cx=None, cy=None, conc=0.0):
    img = Image.new("RGBA", size, (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    w, h = size
    if cx is None:
        cx, cy = w / 2, h / 2
    x, y = seed * 2 + 1, seed * 5 + 3
    for _ in range(count):
        x = (x * 1103515245 + 12345) & 0x7FFFFFFF
        y = (y * 1103515245 + 12345) & 0x7FFFFFFF
        px = (x % 1000) / 1000.0 * w
        py = (y % 1000) / 1000.0 * h
        if conc > 0:
            px = px * (1 - conc) + cx * conc
            py = py * (1 - conc) + cy * conc
        r = L(radius)
        d.ellipse([px - r, py - r, px + r, py + r], fill=color)
    return img


def star(d, x, y, r, color, spike=False):
    d.ellipse([x - r, y - r, x + r, y + r], fill=color)
    if spike:
        d.line([x - r * 2.4, y, x + r * 2.4, y], fill=color, width=L(0.4))
        d.line([x, y - r * 2.4, x, y + r * 2.4], fill=color, width=L(0.4))


def blob_shape(size, cx, cy, lobes):
    shape = Image.new("RGBA", size, (0, 0, 0, 0))
    sd = ImageDraw.Draw(shape)
    for ox, oy, rx, ry in lobes:
        sd.ellipse([cx + L(ox - rx), cy + L(oy - ry),
                    cx + L(ox + rx), cy + L(oy + ry)], fill=(255,) * 4)
    return shape


# ---- galaxies -------------------------------------------------------------
def g_elliptical(filled, c, hi, lens=False):
    img = canvas(GLYPH, GLYPH)
    d = ImageDraw.Draw(img)
    cx = cy = L(GLYPH / 2)
    rx, ry = L(6.4), L(4.6)
    box = [cx - rx, cy - ry, cx + rx, cy + ry]
    if filled:
        for k in range(6, 0, -1):
            f = k / 6.0
            col = tuple(min(255, int(hi[i] * (1.15 - f * 0.5))) for i in range(3))
            d.ellipse([cx - rx * f, cy - ry * f, cx + rx * f, cy + ry * f],
                      fill=col + (255,))
    else:
        d.ellipse(box, outline=hi, width=L(1.0))
    if lens:
        d.ellipse([cx - rx + L(1), cy - ry + L(0.6),
                   cx + rx - L(1), cy + ry - L(0.6)], outline=c, width=L(0.6))
    star(d, cx, cy, L(1.4 if filled else 1.0), hi)
    return img.rotate(-22, resample=Image.BICUBIC, center=(cx, cy))


def g_lenticular(filled, c, hi, dust=False):
    img = canvas(GLYPH, GLYPH)
    d = ImageDraw.Draw(img)
    cx = cy = L(GLYPH / 2)
    rx, ry = L(7.6), L(1.7)
    box = [cx - rx, cy - ry, cx + rx, cy + ry]
    if filled:
        d.ellipse(box, fill=c + (255,))
        d.ellipse([cx - L(3.2), cy - L(2.6), cx + L(3.2), cy + L(2.6)],
                  fill=hi + (255,))
    else:
        d.ellipse(box, outline=hi, width=L(1.0))
        d.ellipse([cx - L(3.2), cy - L(2.6), cx + L(3.2), cy + L(2.6)],
                  outline=c, width=L(0.7))
    if dust:
        d.line([cx - rx + L(1), cy, cx + rx - L(1), cy],
               fill=(20, 20, 30, 220), width=L(0.7))
    star(d, cx, cy, L(1.3 if filled else 1.0), hi)
    return img.rotate(-24, resample=Image.BICUBIC, center=(cx, cy))


def spiral_arm(d, cx, cy, a0, turns, r0, r1, color, w, n=40):
    pts = []
    for i in range(n + 1):
        t = i / n
        ang = a0 + turns * 2 * math.pi * t
        r = r0 + (r1 - r0) * t
        pts.append((cx + math.cos(ang) * r, cy + math.sin(ang) * r))
    d.line(pts, fill=color, width=w, joint="curve")


def g_spiral(filled, c, hi, barred=False, intermediate=False):
    img = canvas(GLYPH, GLYPH)
    d = ImageDraw.Draw(img)
    cx = cy = L(GLYPH / 2)
    R = L(6.8)
    w = L(1.3 if filled else 0.9)
    turns = 0.55 if intermediate else 0.62
    r0 = L(2.6) if barred else L(1.4)
    arm = hi if filled else c
    if barred or intermediate:
        # SB gets the unmistakable long bar; SAB gets a shorter, softer bar
        # so it reads as intermediate rather than as another unbarred spiral.
        bar = L(3.0 if barred else 1.8)
        d.line([cx - bar, cy, cx + bar, cy], fill=hi, width=L(1.6))
        spiral_arm(d, cx + bar, cy, 0.0, turns, r0 * 0.4, R, arm, w)
        spiral_arm(d, cx - bar, cy, math.pi, turns, r0 * 0.4, R, arm, w)
    else:
        spiral_arm(d, cx, cy, 0.2, turns, r0, R, arm, w)
        spiral_arm(d, cx, cy, 0.2 + math.pi, turns, r0, R, arm, w)
    if filled:
        for sx, sy in [(R * 0.7, 0), (0, R * 0.7), (-R * 0.6, R * 0.3)]:
            star(d, int(cx + sx), int(cy + sy), L(0.7), hi)
    core = L(1.8 if filled else 1.2)
    d.ellipse([cx - core, cy - core, cx + core, cy + core], fill=hi)
    return img.rotate(-18, resample=Image.BICUBIC, center=(cx, cy))


def g_irregular(filled, c, hi):
    img = canvas(GLYPH, GLYPH)
    d = ImageDraw.Draw(img)
    cx = cy = L(GLYPH / 2)
    clumps = [(-3.4, -1.6, 3.0), (0.6, -2.8, 2.0), (2.8, 0.8, 2.6),
              (-1.2, 2.6, 2.2), (1.0, 1.0, 1.6)]
    if filled:
        shape = blob_shape(img.size, cx, cy,
                           [(ox, oy, r, r) for ox, oy, r in clumps])
        img.alpha_composite(clip_to(stipple(img.size, hi, 90, 0.7), shape))
        img.alpha_composite(clip_to(
            Image.new("RGBA", img.size, c + (90,)), shape))
    for ox, oy, r in clumps:
        star(d, int(cx + L(ox)), int(cy + L(oy)), L(0.8 if filled else 0.6), hi)
    return img


def g_generic(filled, c, hi):
    img = canvas(GLYPH, GLYPH)
    d = ImageDraw.Draw(img)
    cx = cy = L(GLYPH / 2)
    rx, ry = L(6.6), L(3.4)
    box = [cx - rx, cy - ry, cx + rx, cy + ry]
    if filled:
        d.ellipse(box, fill=c + (200,))
        d.ellipse([cx - L(2.4), cy - L(1.6), cx + L(2.4), cy + L(1.6)],
                  fill=hi + (255,))
    else:
        d.ellipse(box, outline=hi, width=L(1.0))
    star(d, cx, cy, L(1.2 if filled else 0.9), hi)
    return img.rotate(-25, resample=Image.BICUBIC, center=(cx, cy))


def g_multi(filled, c, hi, n):
    img = canvas(GLYPH, GLYPH)
    cx = cy = L(GLYPH / 2)
    if n == 2:
        pos = [(-3.4, -1.2), (3.4, 1.2)]
    elif n == 3:
        pos = [(-3.6, -2.6), (3.2, -1.0), (0.4, 3.4)]
    else:
        pos = [(-4.0, -3.0), (2.0, -3.4), (4.0, 1.2), (-2.4, 2.8), (0.6, 0.4)]
    for i, (ox, oy) in enumerate(pos):
        px, py = cx + L(ox), cy + L(oy)
        rx = L(2.2 - i * 0.15)
        ry = L(1.3 - i * 0.08)
        sub = Image.new("RGBA", img.size, (0, 0, 0, 0))
        sd = ImageDraw.Draw(sub)
        if filled:
            sd.ellipse([px - rx, py - ry, px + rx, py + ry], fill=c + (255,))
            sd.ellipse([px - L(0.9), py - L(0.9), px + L(0.9), py + L(0.9)],
                       fill=hi + (255,))
        else:
            sd.ellipse([px - rx, py - ry, px + rx, py + ry],
                       outline=hi, width=L(0.8))
        img.alpha_composite(sub.rotate(-30 + i * 40, resample=Image.BICUBIC,
                                       center=(px, py)))
    return img


# ---- nebulae, clusters, remnants -----------------------------------------
def n_emission(filled, c, hi):
    img = canvas(GLYPH, GLYPH)
    d = ImageDraw.Draw(img)
    cx = cy = L(GLYPH / 2)
    shape = blob_shape(img.size, cx, cy,
                       [(-3.2, 0.5, 3.8, 3.3), (-0.8, -2.5, 3.8, 3.1),
                        (2.7, -1.2, 3.2, 2.8), (2.4, 2.1, 3.5, 2.7),
                        (-1.2, 2.7, 3.7, 2.5)])
    if filled:
        img.alpha_composite(clip_to(
            Image.new("RGBA", img.size, c + (150,)), shape))
        for a in (25, 78, 132):
            ar = math.radians(a)
            d.line([cx - math.cos(ar) * L(5), cy - math.sin(ar) * L(5),
                    cx + math.cos(ar) * L(5), cy + math.sin(ar) * L(5)],
                   fill=hi + (200,), width=L(0.6))
    else:
        d.line([(cx-L(4.0),cy+L(1.2)),(cx-L(1.2),cy-L(1.0)),
                (cx+L(1.2),cy+L(1.0)),(cx+L(4.2),cy-L(1.4))],
               fill=c+(190,),width=L(0.55),joint="curve")
    edge = shape.filter(ImageFilter.FIND_EDGES)
    tint = Image.new("RGBA", img.size, hi + (255,))
    tint.putalpha(edge.split()[3].point(lambda v: min(255, v * 3)))
    img.alpha_composite(tint)
    return img


def n_hii(filled, c, hi):
    img = n_emission(filled, c, hi)
    d = ImageDraw.Draw(img)
    cx = cy = L(GLYPH / 2)
    for ox, oy in [(-2.0, -1.0), (1.6, 0.4), (0.2, 2.0), (2.6, -1.8)]:
        star(d, int(cx + L(ox)), int(cy + L(oy)), L(0.9 if filled else 0.7),
             hi, spike=filled)
    return img


def n_reflection(filled, c, hi):
    img = canvas(GLYPH, GLYPH)
    d = ImageDraw.Draw(img)
    cx = cy = L(GLYPH / 2)
    shape = blob_shape(img.size, cx, cy,
                       [(0, 0, 5.6, 4.4), (-2.0, 1.4, 3.0, 2.4)])
    if filled:
        img.alpha_composite(clip_to(
            Image.new("RGBA", img.size, c + (110,)), shape))
    edge = shape.filter(ImageFilter.FIND_EDGES)
    tint = Image.new("RGBA", img.size, c + (255,))
    tint.putalpha(edge.split()[3].point(lambda v: min(255, v * 2)))
    img.alpha_composite(tint)
    star(d, cx + L(1.4), cy - L(0.8), L(2.0 if filled else 1.4), hi, spike=True)
    return img


def n_dark(filled, c, hi):
    # A dark nebula is defined by absence, so it is drawn as a dashed frame
    # (so the glyph has real extent) enclosing a star field with an opaque
    # winding lane blotting part of it out.
    img = canvas(GLYPH, GLYPH)
    d = ImageDraw.Draw(img)
    cx = cy = L(GLYPH / 2)
    m = L(2.0)
    # A restrained broken oval establishes the cloud boundary. Seven stars
    # are enough to communicate occlusion without turning into visual noise.
    d.arc([m,m,GLYPH*S-m,GLYPH*S-m],190,345,fill=c+(210,),width=L(0.8))
    d.arc([m,m,GLYPH*S-m,GLYPH*S-m],15,155,fill=c+(210,),width=L(0.8))
    for ox,oy,r0 in [(-5,-4,.55),(-1,-5,.7),(4,-4,.55),(-5,3,.65),
                     (0,4,.5),(5,3,.7),(4,0,.45)]:
        star(d,cx+L(ox),cy+L(oy),L(r0),hi,spike=filled and r0>.65)
    pts = [(cx + L(i), cy + L(math.sin(i * 0.5) * 2.2)) for i in range(-6, 7)]
    if filled:
        d.line(pts, fill=(6, 8, 14, 255), width=L(4.4), joint="curve")
        d.line(pts, fill=c + (160,), width=L(0.6), joint="curve")
    else:
        d.line(pts, fill=c + (220,), width=L(1.2), joint="curve")
    return img


def n_planetary(filled, c, hi):
    img = canvas(GLYPH, GLYPH)
    d = ImageDraw.Draw(img)
    cx = cy = L(GLYPH / 2)
    r = L(6.2)
    d.ellipse([cx - r, cy - r, cx + r, cy + r], outline=hi,
              width=L(2.4) if filled else L(1.2))
    ri = L(3.4)
    d.ellipse([cx - ri, cy - ri, cx + ri, cy + ri], outline=c, width=L(0.8))
    star(d, cx, cy, L(1.1), hi, spike=filled)
    return img


def n_remnant(filled, c, hi):
    img = canvas(GLYPH, GLYPH)
    d = ImageDraw.Draw(img)
    cx = cy = L(GLYPH / 2)
    r = L(6.4)
    w = L(1.5) if filled else L(0.9)
    d.arc([cx - r, cy - r, cx + r, cy + r], 140, 380, fill=hi, width=w)
    r2 = L(4.6)
    d.arc([cx - r2, cy - L(5.6), cx + r2, cy + L(3.2)], 320, 150, fill=c, width=w)
    for a in (200, 255, 330):
        ar = math.radians(a)
        d.line([cx + math.cos(ar) * r * 0.72, cy + math.sin(ar) * r * 0.72,
                cx + math.cos(ar) * r * 1.04, cy + math.sin(ar) * r * 1.04],
               fill=hi, width=L(0.7))
    return img


def n_clusterneb(filled, c, hi):
    img = canvas(GLYPH, GLYPH)
    cx = cy = L(GLYPH / 2)
    shape = blob_shape(img.size, cx, cy,
                       [(-2.2,0.4,4.2,3.5),(2.2,-1.4,3.4,2.8),
                        (1.5,2.2,3.8,2.5)])
    if filled:
        img.alpha_composite(clip_to(
            Image.new("RGBA", img.size, c + (90,)), shape))
    edge = shape.filter(ImageFilter.FIND_EDGES)
    tint = Image.new("RGBA", img.size, c + (200,))
    tint.putalpha(edge.split()[3].point(lambda v: min(255, v * 2)))
    img.alpha_composite(tint)
    d = ImageDraw.Draw(img)
    for ox, oy in [(-3.0, -1.2), (0.4, -2.5), (2.8, 0.0),
                   (-1.2, 2.1), (1.8, 1.8), (0.0, 0.0)]:
        star(d, int(cx + L(ox)), int(cy + L(oy)), L(0.9 if filled else 0.7),
             hi, spike=filled and (ox,oy) in [(0.4,-2.5),(0.0,0.0)])
    return img


def c_globular(filled, c, hi):
    img = canvas(GLYPH, GLYPH)
    d = ImageDraw.Draw(img)
    cx = cy = L(GLYPH / 2)
    r = L(6.6)
    box = [cx - r, cy - r, cx + r, cy + r]
    mask = ellipse_mask(img.size, box)
    img.alpha_composite(clip_to(
        stipple(img.size, hi, 115 if filled else 72, 0.42,
                cx=cx, cy=cy, conc=0.38), mask))
    img.alpha_composite(clip_to(
        stipple(img.size, hi, 76 if filled else 44, 0.38,
                seed=23, cx=cx, cy=cy, conc=0.72), mask))
    star(d,cx,cy,L(1.0 if filled else 0.7),hi,spike=filled)
    d.ellipse(box, outline=hi if filled else c, width=L(1.0 if filled else 0.7))
    return img


def c_open(filled, c, hi):
    img = canvas(GLYPH, GLYPH)
    d = ImageDraw.Draw(img)
    cx = cy = L(GLYPH / 2)
    stars = [(-5.0, -2.6, 1.1), (-1.4, -5.0, 0.85), (2.8, -3.8, 1.0),
             (5.0, 0.6, 0.9), (-3.4, 2.6, 0.95), (1.2, 4.4, 1.15),
             (4.0, 3.6, 0.8), (-0.6, 0.2, 1.3), (2.0, -0.8, 0.7)]
    for ox, oy, r in stars:
        star(d, int(cx + L(ox)), int(cy + L(oy)),
             L(r * (1.25 if filled else 1.0)), hi,
             spike=filled and r >= 1.1)
    return img


def n_generic(filled, c, hi):
    img = canvas(GLYPH, GLYPH)
    cx = cy = L(GLYPH / 2)
    shape = blob_shape(img.size, cx, cy,
                       [(0, 0, 5.8, 4.6), (1.6, -1.2, 3.4, 2.6)])
    if filled:
        img.alpha_composite(clip_to(
            Image.new("RGBA", img.size, c + (120,)), shape))
    edge = shape.filter(ImageFilter.FIND_EDGES)
    tint = Image.new("RGBA", img.size, hi + (255,))
    tint.putalpha(edge.split()[3].point(lambda v: min(255, v * 2)))
    img.alpha_composite(tint)
    return img


DRAW = {
    0: lambda f, c, hi: g_elliptical(f, c, hi),
    1: lambda f, c, hi: g_elliptical(f, c, hi, lens=True),
    2: lambda f, c, hi: g_lenticular(f, c, hi),
    3: lambda f, c, hi: g_lenticular(f, c, hi, dust=True),
    4: lambda f, c, hi: g_spiral(f, c, hi),
    5: lambda f, c, hi: g_spiral(f, c, hi, barred=True),
    6: lambda f, c, hi: g_spiral(f, c, hi, intermediate=True),
    7: g_irregular,
    8: g_generic,
    9: lambda f, c, hi: g_multi(f, c, hi, 2),
    10: lambda f, c, hi: g_multi(f, c, hi, 3),
    11: lambda f, c, hi: g_multi(f, c, hi, 5),
    12: n_emission, 13: n_hii, 14: n_reflection, 15: n_dark,
    16: n_planetary, 17: n_remnant, 18: n_clusterneb,
    19: c_globular, 20: c_open, 21: n_generic,
}
NAMES = {
    0: "gal_e", 1: "gal_es0", 2: "gal_s0", 3: "gal_s0a", 4: "gal_sa",
    5: "gal_sb", 6: "gal_sab", 7: "gal_irr", 8: "gal_generic",
    9: "gal_pair", 10: "gal_triplet", 11: "gal_group",
    12: "emission", 13: "hii", 14: "reflection", 15: "dark",
    16: "planetary", 17: "remnant", 18: "clusterneb", 19: "globular",
    20: "opencluster", 21: "nebula",
}


def moon_phase(index, night):
    """Projected illuminated sphere: waxing right, waning left."""
    img = canvas(MOON, MOON)
    px = img.load()
    w, h = img.size
    cx = (w - 1) / 2.0
    cy = (h - 1) / 2.0
    r = L(MOON / 2 - 1.6)
    phi = 2.0 * math.pi * (index % 8) / 8.0
    sx, sz = math.sin(phi), -math.cos(phi)
    lit = NRED_HI if night else (236, 232, 214)
    dark = (48, 5, 2) if night else (25, 29, 31)
    rim = NRED_LO if night else (115, 122, 120)
    for yy in range(h):
        yn = (yy - cy) / r
        for xx in range(w):
            xn = (xx - cx) / r
            q = xn * xn + yn * yn
            if q > 1.0:
                continue
            z = math.sqrt(max(0.0, 1.0 - q))
            color = lit if xn * sx + z * sz > 0.0 else dark
            if q > 0.90:
                color = rim
            px[xx, yy] = color + (255,)
    return img


def nav_sky(filled, night):
    img = canvas(NAV, NAV)
    d = ImageDraw.Draw(img)
    hi = NRED_HI if night else CYAN_HI
    c = NRED if night else CYAN
    cx = L(NAV / 2)
    d.line([L(3), L(19), L(23), L(19)], fill=hi, width=L(1.4))
    disc = Image.new("RGBA", img.size, (0, 0, 0, 0))
    dd = ImageDraw.Draw(disc)
    r = L(6.2)
    dd.ellipse([cx - r - L(2), L(4), cx + r - L(2), L(4) + 2 * r],
               fill=hi + (255,) if filled else (0, 0, 0, 0),
               outline=hi + (255,), width=L(1.2))
    cut = Image.new("L", img.size, 0)
    ImageDraw.Draw(cut).ellipse(
        [cx - r + L(2), L(3), cx + r + L(2), L(3) + 2 * r], fill=255)
    disc.putalpha(Image.composite(disc.split()[3], Image.new("L", img.size, 0),
                                  cut.point(lambda v: 255 if v < 128 else 0)))
    img.alpha_composite(disc)
    for sx, sy, sr in [(6.5, 6.0, 1.0), (19.5, 8.5, 0.8), (17.0, 4.0, 0.6)]:
        star(d, L(sx), L(sy), L(sr * (1.3 if filled else 1.0)), c)
    return img


def main():
    made = []
    for idx, fn in DRAW.items():
        nm = NAMES[idx]
        acc = DAY_ACCENT[idx]
        hi = tuple(min(255, int(v * 1.25 + 45)) for v in acc)
        for filled in (False, True):
            sfx = "_f" if filled else ""
            img = glowize(fn(filled, acc, hi), acc, 3,
                          0.62 if filled else 0.30)
            made.append(finish(img, GLYPH, GLYPH, f"obj_{nm}{sfx}"))
            img = glowize(fn(filled, NRED, NRED_HI), NRED, 3,
                          0.58 if filled else 0.28)
            made.append(finish(img, GLYPH, GLYPH, f"n_obj_{nm}{sfx}"))

    for i in range(8):
        made.append(finish(glowize(moon_phase(i, False), (200, 200, 190), 2, 0.20),
                           MOON, MOON, f"moon_{i}"))
        made.append(finish(glowize(moon_phase(i, True), NRED, 2, 0.25),
                           MOON, MOON, f"n_moon_{i}"))

    for filled in (False, True):
        sfx = "_f" if filled else ""
        made.append(finish(glowize(nav_sky(filled, False), CYAN, 5,
                                   1.4 if filled else 0.7),
                           NAV, NAV, f"nav_sky{sfx}"))
        made.append(finish(glowize(nav_sky(filled, True), NRED, 5,
                                   1.3 if filled else 0.6),
                           NAV, NAV, f"n_nav_sky{sfx}"))

    print(f"wrote {len(made)} assets to {OUT}")


if __name__ == "__main__":
    main()
