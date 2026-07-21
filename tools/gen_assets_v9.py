#!/usr/bin/env python3
"""
gen_assets_v9.py -- new artwork for the SKY and CATALOG screens.

Follows the house rules already established by gen_assets.py / gen_night.py:
  * everything drawn procedurally at 4x supersampling, then LANCZOS down
  * day set  = full colour, neon glow under sharp artwork
  * night set = single red channel, prefix n_, suffix _f for the FILLED state
  * OUTLINE = inactive/negative, FILLED = active/positive

The night set is not a red repaint of the day set. Hue is unavailable, so the
information that hue carried in day mode has to move somewhere else. It moves
into INTERIOR TEXTURE: each object type gets a structurally different fill so
the six types stay separable with one colour.

    galaxy      solid core, smooth  -- the only smoothly-filled ellipse
    planetary   ring, hollow centre -- reads as an annulus
    globular    dense stipple       -- granular
    nebula      diagonal hatch      -- linear texture, unmistakable vs stipple
    cluster     discrete star points -- sparse, high contrast
    remnant     torn double filament -- open, asymmetric

Run from the sketch folder:  python3 tools/gen_assets_v9.py
"""
from PIL import Image, ImageDraw, ImageFilter
import math, os

S   = 4                       # supersample
OUT = "assets/screen_png"
os.makedirs(OUT, exist_ok=True)

# ---- day palette (matches gen_assets.py) ---------------------------------
CYAN    = (23, 184, 255)
CYAN_HI = (140, 228, 255)
GREEN   = (99, 242, 58)
AMBER   = (255, 157, 32)
WHITE   = (242, 245, 247)
MAGENTA = (255, 120, 190)
VIOLET  = (190, 140, 255)
GOLD    = (255, 216, 77)
MOONLIT = (236, 232, 214)

# ---- night palette (matches gen_night.py) --------------------------------
RED     = (255, 40, 20)
RED_HI  = (255, 70, 40)
RED_LO  = (120, 18, 8)

# Per-type accent in day mode. Night mode uses RED_HI for all six and relies
# entirely on texture, which is the whole point of the exercise.
TYPE_COLOR = {
    "galaxy":    CYAN,
    "planetary": CYAN_HI,
    "globular":  GOLD,
    "nebula":    MAGENTA,
    "cluster":   GREEN,
    "remnant":   VIOLET,
}
TYPES = list(TYPE_COLOR.keys())

GLYPH = 18        # type glyph box, px
MOON  = 30        # moon phase disc, px
NAV   = 26        # nav glyph, px


def canvas(w, h):
    return Image.new("RGBA", (w * S, h * S), (0, 0, 0, 0))


def L(v):
    return int(round(v * S))


def glowize(img, color, blur=6, strength=1.4):
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


# =========================================================================
#  Texture helpers -- these carry the type identity in night mode
# =========================================================================
def clip_to(img, mask_img):
    """Keep only the parts of img that fall inside mask_img's alpha."""
    out = Image.new("RGBA", img.size, (0, 0, 0, 0))
    out.paste(img, (0, 0), mask_img.split()[3])
    return out


def hatch_layer(size, color, spacing, width, angle=45):
    """Diagonal hatching -- the nebula texture."""
    img = Image.new("RGBA", size, (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    w, h = size
    span = w + h
    step = L(spacing)
    for i in range(-span, span, step):
        if angle >= 0:
            d.line([(i, 0), (i + h, h)], fill=color, width=L(width))
        else:
            d.line([(i, h), (i + h, 0)], fill=color, width=L(width))
    return img


def stipple_layer(size, color, count, radius, seed=7):
    """Deterministic pseudo-random dots -- the globular texture."""
    img = Image.new("RGBA", size, (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    w, h = size
    x, y = seed, seed * 3 + 1
    for _ in range(count):
        # cheap LCG so the pattern is stable across runs and platforms
        x = (x * 1103515245 + 12345) & 0x7FFFFFFF
        y = (y * 1103515245 + 12345) & 0x7FFFFFFF
        px = x % w
        py = y % h
        r = L(radius)
        d.ellipse([px - r, py - r, px + r, py + r], fill=color)
    return img


def ellipse_mask(size, box):
    img = Image.new("RGBA", size, (0, 0, 0, 0))
    ImageDraw.Draw(img).ellipse(box, fill=(255, 255, 255, 255))
    return img


# =========================================================================
#  Object type glyphs
# =========================================================================
def glyph_galaxy(filled, c, hi):
    img = canvas(GLYPH, GLYPH)
    d = ImageDraw.Draw(img)
    cx = cy = L(GLYPH / 2)
    # Two logarithmic-ish arms create a unique S silhouette; this survives
    # reduction much better than an ellipse with decorative arcs.
    for phase in (0.0, math.pi):
        pts = []
        for i in range(28):
            t = i / 27.0
            a = phase + 0.4 + t * 1.8 * math.pi
            r = L(0.8 + t * 6.6)
            pts.append((cx + math.cos(a) * r, cy + math.sin(a) * r * 0.58))
        d.line(pts, fill=hi, width=L(1.45 if filled else 1.0), joint="curve")
    core = L(1.8 if filled else 1.25)
    d.ellipse([cx-core,cy-L(1.0),cx+core,cy+L(1.0)],fill=hi)
    return img.rotate(-24,resample=Image.BICUBIC,center=(cx,cy))


def glyph_planetary(filled, c, hi):
    img = canvas(GLYPH, GLYPH)
    d = ImageDraw.Draw(img)
    cx = cy = L(GLYPH / 2)
    r = L(5.5)
    # Annulus plus cardinal rays: compact, symmetric and unlike every other
    # category even in monochrome night mode.
    w = L(2.0) if filled else L(1.0)
    d.ellipse([cx - r, cy - r, cx + r, cy + r], outline=hi, width=w)
    ray0,ray1=L(6.2),L(8.0)
    d.line([cx-ray1,cy,cx-ray0,cy],fill=hi,width=L(0.8))
    d.line([cx+ray0,cy,cx+ray1,cy],fill=hi,width=L(0.8))
    d.line([cx,cy-ray1,cx,cy-ray0],fill=hi,width=L(0.8))
    d.line([cx,cy+ray0,cx,cy+ray1],fill=hi,width=L(0.8))
    d.ellipse([cx-L(0.8),cy-L(0.8),cx+L(0.8),cy+L(0.8)],fill=hi)
    return img


def glyph_globular(filled, c, hi):
    img = canvas(GLYPH, GLYPH)
    d = ImageDraw.Draw(img)
    cx = cy = L(GLYPH / 2)
    r = L(6.8)
    box = [cx - r, cy - r, cx + r, cy + r]
    mask = ellipse_mask(img.size, box)
    dots = stipple_layer(img.size, hi, 150 if filled else 90,
                         0.5 if filled else 0.4)
    img.alpha_composite(clip_to(dots, mask))
    # concentration toward the middle -- globulars are centrally condensed
    rc = L(2.6)
    inner = ellipse_mask(img.size, [cx - rc, cy - rc, cx + rc, cy + rc])
    dense = stipple_layer(img.size, hi, 190, 0.5, seed=19)
    img.alpha_composite(clip_to(dense, inner))
    # Bright central cross makes the concentration readable at 18 px.
    d.line([cx-L(3.2),cy,cx+L(3.2),cy],fill=hi,width=L(0.7))
    d.line([cx,cy-L(3.2),cx,cy+L(3.2)],fill=hi,width=L(0.7))
    return img


def glyph_nebula(filled, c, hi):
    img = canvas(GLYPH, GLYPH)
    d = ImageDraw.Draw(img)
    cx = cy = L(GLYPH / 2)
    # Irregular five-lobed cloud. Its scalloped outer silhouette remains
    # recognizable at 18 px and cannot be confused with a planetary ring.
    lobes = [(-4.0, 0.7, 3.2, 3.4), (-1.7, -2.8, 3.7, 3.3),
             (2.2, -2.4, 3.2, 3.0), (4.0, 1.3, 3.2, 3.5),
             (0.0, 3.1, 4.2, 3.1)]
    shape = Image.new("RGBA", img.size, (0, 0, 0, 0))
    sd = ImageDraw.Draw(shape)
    for ox, oy, rx, ry in lobes:
        sd.ellipse([cx + L(ox - rx), cy + L(oy - ry),
                    cx + L(ox + rx), cy + L(oy + ry)],
                   fill=(255, 255, 255, 255))
    fill=Image.new("RGBA",img.size,(c+(210 if filled else 75,)))
    fill.putalpha(shape.split()[3]);img.alpha_composite(fill)
    hatch=hatch_layer(img.size,hi,2.4,0.45)
    img.alpha_composite(clip_to(hatch,shape))
    # soft outline around the union
    edge = shape.filter(ImageFilter.FIND_EDGES)
    tint = Image.new("RGBA", img.size, hi + (255,))
    tint.putalpha(edge.split()[3].point(lambda v: min(255, v * 3)))
    img.alpha_composite(tint)
    return img


def glyph_cluster(filled, c, hi):
    img = canvas(GLYPH, GLYPH)
    d = ImageDraw.Draw(img)
    cx = cy = L(GLYPH / 2)
    stars = [(-6.0,-2.8,1.1),(-2.0,-5.5,0.7),(3.2,-4.0,0.9),
             (6.0,0.8,0.75),(-4.2,3.8,0.8),(0.8,5.4,1.1),
             (4.4,4.2,0.65),(-0.8,0.1,1.25)]
    for ox, oy, r in stars:
        rr = L(r * (1.35 if filled else 1.0))
        px, py = cx + L(ox), cy + L(oy)
        d.ellipse([px - rr, py - rr, px + rr, py + rr], fill=hi)
        if filled and r>=1.0:
            d.line([px-rr*1.8,py,px+rr*1.8,py],fill=c,width=L(0.4))
            d.line([px,py-rr*1.8,px,py+rr*1.8],fill=c,width=L(0.4))
    return img


def glyph_remnant(filled, c, hi):
    img = canvas(GLYPH, GLYPH)
    d = ImageDraw.Draw(img)
    cx = cy = L(GLYPH / 2)
    r = L(6.8)
    w = L(1.5) if filled else L(0.9)
    # Broken shell with deliberately uneven gaps and radial filaments.
    for a0,a1 in ((8,72),(96,158),(184,248),(278,338)):
        d.arc([cx-r,cy-r,cx+r,cy+r],a0,a1,fill=hi,width=w)
    r2=L(4.4)
    d.arc([cx-r2,cy-r2,cx+r2,cy+r2],205,330,fill=c,width=L(0.8))
    for a in (18,112,218,305):
        ar = math.radians(a)
        x0 = cx + math.cos(ar) * r * 0.72
        y0 = cy + math.sin(ar) * r * 0.72
        x1 = cx + math.cos(ar) * r * 1.02
        y1 = cy + math.sin(ar) * r * 1.02
        d.line([x0, y0, x1, y1], fill=hi, width=L(0.7))
    return img


GLYPH_FN = {
    "galaxy": glyph_galaxy, "planetary": glyph_planetary,
    "globular": glyph_globular, "nebula": glyph_nebula,
    "cluster": glyph_cluster, "remnant": glyph_remnant,
}


# =========================================================================
#  Moon phase discs
# =========================================================================
def moon_phase(index, night):
    """Physically coherent projected sphere.

    0=new, 2=first quarter, 4=full, 6=last quarter.  A surface normal is
    lit when it faces the Sun, which naturally produces curved crescent and
    gibbous terminators. Waxing is lit on the right; waning on the left.
    """
    img=canvas(MOON,MOON)
    px=img.load();w,h=img.size
    cx=(w-1)/2.0;cy=(h-1)/2.0;r=L(MOON/2-1.6)
    phi=2.0*math.pi*(index%8)/8.0
    sx,sz=math.sin(phi),-math.cos(phi)
    lit_col=RED_HI if night else MOONLIT
    dark_col=(45,6,3) if night else (25,29,31)
    rim_col=RED_LO if night else (115,122,120)
    for yy in range(h):
        yn=(yy-cy)/r
        for xx in range(w):
            xn=(xx-cx)/r
            q=xn*xn+yn*yn
            if q>1.0:continue
            z=math.sqrt(max(0.0,1.0-q))
            sun=xn*sx+z*sz
            edge=z
            if sun>0:
                # Gentle limb darkening keeps the disc dimensional without
                # muddy texture at the final 30 px size.
                f=0.78+0.22*edge
                col=tuple(int(v*f) for v in lit_col)
                px[xx,yy]=col+(255,)
            else:
                px[xx,yy]=dark_col+(150,)
    d=ImageDraw.Draw(img)
    box=[cx-r,cy-r,cx+r,cy+r]
    d.ellipse(box,outline=rim_col+(255,),width=L(0.8))
    return img


# =========================================================================
#  Nav glyph: SKY (replaces the unused ALIGN slot)
# =========================================================================
def nav_sky(filled, night):
    img = canvas(NAV, NAV)
    d = ImageDraw.Draw(img)
    hi = RED_HI if night else CYAN_HI
    c = RED if night else CYAN
    cx, cy = L(NAV / 2), L(NAV / 2)
    # horizon
    d.line([L(3), L(19), L(23), L(19)], fill=hi, width=L(1.4))
    # crescent: big disc minus offset disc
    disc = Image.new("RGBA", img.size, (0, 0, 0, 0))
    dd = ImageDraw.Draw(disc)
    r = L(6.2)
    dd.ellipse([cx - r - L(2), L(4), cx + r - L(2), L(4) + 2 * r],
               fill=hi + (255,) if filled else (0, 0, 0, 0),
               outline=hi + (255,), width=L(1.2))
    cut = Image.new("RGBA", img.size, (0, 0, 0, 0))
    ImageDraw.Draw(cut).ellipse(
        [cx - r + L(2), L(3), cx + r + L(2), L(3) + 2 * r],
        fill=(0, 0, 0, 255))
    disc.putalpha(Image.composite(disc.split()[3],
                                  Image.new("L", img.size, 0),
                                  cut.split()[3].point(
                                      lambda v: 255 if v < 128 else 0)))
    img.alpha_composite(disc)
    # stars
    for sx, sy, sr in [(6.5, 6.0, 1.0), (19.5, 8.5, 0.8), (17.0, 4.0, 0.6)]:
        rr = L(sr * (1.3 if filled else 1.0))
        d.ellipse([L(sx) - rr, L(sy) - rr, L(sx) + rr, L(sy) + rr], fill=c)
    return img


# =========================================================================
def main():
    made = []
    # ---- type glyphs -----------------------------------------------------
    for t in TYPES:
        col = TYPE_COLOR[t]
        hi = tuple(min(255, int(v * 1.35 + 40)) for v in col)
        for filled in (False, True):
            # day
            img = GLYPH_FN[t](filled, col, hi)
            img = glowize(img, col, blur=3, strength=0.55 if filled else 0.25)
            made.append(finish(img, GLYPH, GLYPH,
                               f"obj_{t}{'_f' if filled else ''}"))
            # night
            img = GLYPH_FN[t](filled, RED, RED_HI)
            img = glowize(img, RED, blur=3, strength=0.5 if filled else 0.22)
            made.append(finish(img, GLYPH, GLYPH,
                               f"n_obj_{t}{'_f' if filled else ''}"))

    # ---- moon phases -----------------------------------------------------
    for i in range(8):
        img = moon_phase(i, False)
        img = glowize(img, (200, 200, 190), blur=2, strength=0.2)
        made.append(finish(img, MOON, MOON, f"moon_{i}"))
        img = moon_phase(i, True)
        img = glowize(img, RED, blur=2, strength=0.25)
        made.append(finish(img, MOON, MOON, f"n_moon_{i}"))

    # ---- nav -------------------------------------------------------------
    for filled in (False, True):
        img = nav_sky(filled, False)
        img = glowize(img, CYAN, blur=5, strength=1.5 if filled else 0.8)
        made.append(finish(img, NAV, NAV, f"nav_sky{'_f' if filled else ''}"))
        img = nav_sky(filled, True)
        img = glowize(img, RED, blur=5, strength=1.4 if filled else 0.7)
        made.append(finish(img, NAV, NAV, f"n_nav_sky{'_f' if filled else ''}"))

    print(f"wrote {len(made)} assets to {OUT}")
    for n in made:
        print("   ", n)


if __name__ == "__main__":
    main()
