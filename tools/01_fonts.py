#!/usr/bin/env python3
"""
Build the OnStep 320x480 type ladder from InterVariable.

Inter's default figures are PROPORTIONAL (digit '1' is 833 units against '0' at
1292). On a live RA/Dec readout that makes the whole string shuffle sideways
every time a digit changes. We therefore bake the `tnum` substitution into the
cmap permanently, so the rasteriser never has to know about OpenType features.
`zero` (slashed zero) is baked too - it is unambiguous at 9 px, and this is an
instrument.

Optical size is a real axis in Inter and it matters here: opsz 14 thickens
stems and opens spacing for small text, opsz 32 tightens for the hero readout.
"""
import os
from fontTools.ttLib import TTFont
from fontTools.varLib import instancer

SRC = '/home/claude/fonts/InterVariable.ttf'
OUT = '/home/claude/gen/out/fonts'
os.makedirs(OUT, exist_ok=True)

# role, weight, optical size, target px, what it is for
LADDER = [
    ('Label',   500, 14,  9, 'tracked uppercase labels, footer hints'),
    ('Body',    400, 14, 12, 'list rows, data tables, secondary text'),
    ('Value',   500, 14, 15, 'alt/az, magnitudes, inline values'),
    ('Head',    400, 18, 20, 'object names, screen titles'),
    ('Hero',    300, 32, 34, 'RA/Dec readout, big numerics'),
    ('Strong',  600, 14, 12, 'selected rows, emphasis'),
]

def feature_map(font, tag):
    """Collect single substitutions belonging to one GSUB feature tag."""
    out = {}
    if 'GSUB' not in font:
        return out
    g = font['GSUB'].table
    idxs = set()
    for rec in g.FeatureList.FeatureRecord:
        if rec.FeatureTag == tag:
            idxs.update(rec.Feature.LookupListIndex)
    for i in idxs:
        lk = g.LookupList.Lookup[i]
        for st in lk.SubTable:
            if getattr(st, 'LookupType', lk.LookupType) == 1 or lk.LookupType == 1:
                m = getattr(st, 'mapping', None)
                if m:
                    out.update(m)
    return out

def bake(weight, opsz, path):
    f = TTFont(SRC)
    tnum = feature_map(f, 'tnum')
    zero = feature_map(f, 'zero')
    inst = instancer.instantiateVariableFont(f, {'wght': weight, 'opsz': opsz},
                                             inplace=False, updateFontNames=False)
    # remap digits (and comparison/sign glyphs tnum also covers) in every cmap
    remap = {}
    for src, dst in tnum.items():
        remap[src] = dst
    for src, dst in zero.items():
        remap[remap.get(src, src)] = dst
    changed = 0
    for table in inst['cmap'].tables:
        for cp, gname in list(table.cmap.items()):
            g2 = gname
            if g2 in tnum:
                g2 = tnum[g2]
            if g2 in zero:
                g2 = zero[g2]
            if g2 != gname and g2 in inst.getGlyphOrder():
                table.cmap[cp] = g2
                changed += 1
    inst.save(path)
    return changed, inst

print(f'{"file":26s} {"wght":>5s} {"opsz":>5s} {"px":>4s}  digits tabular  purpose')
for role, w, o, px, why in LADDER:
    p = f'{OUT}/OnStep-{role}.ttf'
    n, inst = bake(w, o, p)
    hm, cm = inst['hmtx'], inst.getBestCmap()
    adv = [hm[cm[ord(c)]][0] for c in '0123456789']
    ok = len(set(adv)) == 1
    print(f'OnStep-{role}.ttf{"":<{max(0,10-len(role))}} {w:5d} {o:5d} {px:4d}  '
          f'{"YES" if ok else "NO ":>14s}  {why}')
