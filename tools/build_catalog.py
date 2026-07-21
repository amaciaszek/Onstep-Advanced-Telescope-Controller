#!/usr/bin/env python3
"""
build_catalog.py -- TheSkyLive TSV  ->  packed 16-byte flash records.

Emits Catalog_long.cpp and Catalog_wide.cpp (pick one at build time via
CATALOG_BUILD in Settings.h) plus a survivor report so the thresholds can
be audited before flashing.

    python3 tools/build_catalog.py theskylive.tsv

KEY FACT that drives the whole cull: the 'Surface Brightness' column is
populated for galaxies ONLY (10190/10481 galaxies, 0/632 everything else).
A global SB filter silently deletes every nebula and globular in the file,
so every cut below is per-type.
"""
import csv, math, os, re, sys, collections

# ---------------------------------------------------------------- thresholds
# Two builds. LONG is for long focal lengths (1270mm class) where small
# objects are the point. WIDE is for short refractors where anything under
# ~5' is a smudge and big clusters/nebulae matter.
PROFILES = {
    "long": dict(
        galaxy_min_size=1.5,      # arcmin -- size-only cut, no SB/mag gate
        galaxy_max_mag=99.0,
        galaxy_max_sb=99.0,
        oc_max_size=60.0,
        oc_max_mag=9.0,
    ),
    "wide": dict(
        galaxy_min_size=5.0,
        galaxy_max_mag=99.0,
        galaxy_max_sb=99.0,
        oc_max_size=180.0,
        oc_max_mag=11.0,
    ),
}

# Types dropped unconditionally: not imaging targets or not real.
DROP_TYPES = {"Nonexistent Object", "Nova Star", "Association of Stars",
              "Other Classification"}

GALAXY_FAMILY = {"Galaxy"}
GALAXY_GROUP  = {"Galaxy Pair", "Galaxy Triplet", "Group of Galaxies"}

# ---------------------------------------------------------------- type enum
# Must match ObjType in Catalog.h
TYPE_MAP = {
    "Galaxy":               0,
    "Planetary Nebula":     1,
    "Globular Cluster":     2,
    "Nebula":               3,
    "Emission Nebula":      3,
    "Reflection Nebula":    3,
    "HII Ionized region":   3,
    "Dark Nebula":          3,
    "Open Cluster":         4,
    "Supernova Remnant":    5,
    "Galaxy Pair":          6,
    "Galaxy Triplet":       6,
    "Group of Galaxies":    6,
    "Star Cluster + Nebula": 7,
}

# ------------------------------------------------------- constellation table
IAU = {
 "Andromeda":"And","Antlia":"Ant","Apus":"Aps","Aquarius":"Aqr","Aquila":"Aql",
 "Ara":"Ara","Aries":"Ari","Auriga":"Aur","Bo\u00f6tes":"Boo","Bootes":"Boo",
 "Caelum":"Cae","Camelopardalis":"Cam","Cancer":"Cnc","Canes Venatici":"CVn",
 "Canis Major":"CMa","Canis Minor":"CMi","Capricornus":"Cap","Carina":"Car",
 "Cassiopeia":"Cas","Centaurus":"Cen","Cepheus":"Cep","Cetus":"Cet",
 "Chamaeleon":"Cha","Circinus":"Cir","Columba":"Col","Coma Berenices":"Com",
 "Corona Australis":"CrA","Corona Borealis":"CrB","Corvus":"Crv","Crater":"Crt",
 "Crux":"Cru","Cygnus":"Cyg","Delphinus":"Del","Dorado":"Dor","Draco":"Dra",
 "Equuleus":"Equ","Eridanus":"Eri","Fornax":"For","Gemini":"Gem","Grus":"Gru",
 "Hercules":"Her","Horologium":"Hor","Hydra":"Hya","Hydrus":"Hyi","Indus":"Ind",
 "Lacerta":"Lac","Leo":"Leo","Leo Minor":"LMi","Lepus":"Lep","Libra":"Lib",
 "Lupus":"Lup","Lynx":"Lyn","Lyra":"Lyr","Mensa":"Men","Microscopium":"Mic",
 "Monoceros":"Mon","Musca":"Mus","Norma":"Nor","Octans":"Oct","Ophiuchus":"Oph",
 "Orion":"Ori","Pavo":"Pav","Pegasus":"Peg","Perseus":"Per","Phoenix":"Phe",
 "Pictor":"Pic","Pisces":"Psc","Piscis Austrinus":"PsA","Puppis":"Pup",
 "Pyxis":"Pyx","Reticulum":"Ret","Sagitta":"Sge","Sagittarius":"Sgr",
 "Scorpius":"Sco","Sculptor":"Scl","Scutum":"Sct","Serpens":"Ser",
 "Sextans":"Sex","Taurus":"Tau","Telescopium":"Tel","Triangulum":"Tri",
 "Triangulum Australe":"TrA","Tucana":"Tuc","Ursa Major":"UMa",
 "Ursa Minor":"UMi","Vela":"Vel","Virgo":"Vir","Volans":"Vol","Vulpecula":"Vul",
}

# ---------------------------------------------------------------- helpers
def fnum(s):
    try:
        v = float(s)
        return v if math.isfinite(v) else None
    except (TypeError, ValueError):
        return None


def designation(raw):
    """'Messier 45' -> 'M45'; 'NGC 7331' -> 'NGC7331'; 'IC 2391' -> 'IC2391'."""
    s = raw.strip()
    s = re.sub(r"^Messier\s*", "M", s, flags=re.I)
    s = re.sub(r"\s+", "", s)
    return s.upper() if s[:2].upper() in ("NG", "IC") else s


def popular_name(com, desig, objname):
    """Com_Name is lowercase 'designation tokens + popular words'."""
    if not com:
        return ""
    words = com.lower().split()
    # strip any leading token that is part of the designation
    strip = set()
    for tok in re.split(r"[\s]+", objname.lower()):
        strip.add(tok)
        strip.add(tok.replace(" ", ""))
    strip.add(desig.lower())
    strip.add(objname.lower().replace(" ", ""))
    out = []
    leading = True
    for w in words:
        if leading and (w in strip or w.rstrip("0123456789") + w.lstrip(
                "abcdefghijklmnopqrstuvwxyz") in strip):
            continue
        if leading and re.fullmatch(r"[a-z]*\d+", w) and not out:
            # unconsumed numeric fragment of the designation
            continue
        leading = False
        out.append(w)
    name = " ".join(out).strip()
    if not name:
        return ""
    small = {"of", "the", "and", "a", "in"}
    parts = []
    for i, w in enumerate(name.split()):
        parts.append(w if (i and w in small) else w.capitalize())
    return " ".join(parts)


# ---------------------------------------------------------------- packing
RA_SCALE = 65535.0 / 24.0          # hours -> uint16
MAG_OFFSET = 3.0                   # mag -3.0 .. +22.5 in uint8 tenths
SB_OFFSET = 15.0                   # SB 15.0 .. 40.4 in uint8 tenths, 255=none


def pack(rows, prof):
    kept = []
    for r in rows:
        t = r["Object Type"].strip()
        if t in DROP_TYPES or t not in TYPE_MAP:
            continue
        size = fnum(r["Major Angular Size"])
        minor = fnum(r["Minor Angular Size"])
        magv = fnum(r["Magnitude V (Visual 551nm)"])
        magb = fnum(r["Magnitude B (Blue 445nm)"])
        mag = magv if magv is not None else magb
        is_b = magv is None and magb is not None
        sb = fnum(r["Surface Brightness"])
        ra = fnum(r["Ra_decimal"])
        dec = fnum(r["Dec_decimal"])
        if ra is None or dec is None:
            continue

        if t in GALAXY_FAMILY or t in GALAXY_GROUP:
            if (size or 0.0) < prof["galaxy_min_size"]:
                continue
            if (mag if mag is not None else 99.0) > prof["galaxy_max_mag"]:
                continue
            if (sb if sb is not None else 99.0) > prof["galaxy_max_sb"]:
                continue
        elif t == "Open Cluster":
            if (size or 0.0) > prof["oc_max_size"]:
                continue
            if (mag if mag is not None else 99.0) > prof["oc_max_mag"]:
                continue
        # everything else: keep unconditionally

        kept.append(dict(
            ra=ra, dec=dec, size=size or 0.0, minor=minor or size or 0.0,
            pa=fnum(r["Position Angle"]) or 0.0,
            mag=mag, is_b=is_b, sb=sb, type=TYPE_MAP[t],
            con=IAU.get(r["Constellation"].strip(), "---"),
            desig=designation(r["Object Name"]),
            popular=popular_name(r["Com_Name"], designation(r["Object Name"]),
                                 r["Object Name"]),
        ))

    kept.sort(key=lambda o: o["ra"])

    # ---- name blob: "DESIG\0" or "DESIG\0Popular Name\0" (flag bit) --------
    blob = bytearray()
    offsets = {}
    for o in kept:
        key = (o["desig"], o["popular"])
        if key in offsets:
            o["nameOff"] = offsets[key]
            continue
        off = len(blob)
        offsets[key] = off
        o["nameOff"] = off
        blob += o["desig"].encode("ascii", "replace") + b"\0"
        if o["popular"]:
            blob += o["popular"].encode("ascii", "replace") + b"\0"
    if len(blob) > 65535:
        raise SystemExit("name blob overflowed uint16 offsets")

    recs = []
    for o in kept:
        ra = max(0, min(65535, int(round(o["ra"] * RA_SCALE))))
        dec = max(-5400, min(5400, int(round(o["dec"] * 60.0))))
        maj = max(0, min(65535, int(round(o["size"] * 10.0))))
        mnr = max(0, min(65535, int(round(o["minor"] * 10.0))))
        pa = int(round(o["pa"] / 2.0)) % 180
        if o["mag"] is None:
            mag = 255
        else:
            mag = max(0, min(254, int(round((o["mag"] + MAG_OFFSET) * 10.0))))
        if o["sb"] is None:
            sb = 255
        else:
            sb = max(0, min(254, int(round((o["sb"] - SB_OFFSET) * 10.0))))
        flags = o["type"] & 0x0F
        if o["is_b"]:
            flags |= 0x10
        if o["popular"]:
            flags |= 0x20
        recs.append((ra, dec, maj, mnr, pa, mag, sb, flags,
                     o["con"], o["nameOff"]))
    return kept, recs, bytes(blob)


def emit(profile, kept, recs, blob, path):
    cons = sorted({r[8] for r in recs})
    cidx = {c: i for i, c in enumerate(cons)}
    lines = []
    A = lines.append
    A('// GENERATED by tools/build_catalog.py -- do not edit by hand.')
    A('// profile: %s   objects: %d   blob: %d bytes   total: %d bytes'
      % (profile, len(recs), len(blob), len(recs) * 16 + len(blob)))
    A('#include "Catalog.h"')
    A('#include "Settings.h"')
    A('')
    macro = 'CATALOG_PROFILE_%s' % profile.upper()
    A('#if CATALOG_PROFILE == %s' % macro)
    A('')
    A('namespace catalog {')
    A('')
    A('const char kConstNames[][4] = {')
    for i in range(0, len(cons), 12):
        A('    ' + ', '.join('"%s"' % c for c in cons[i:i + 12]) + ',')
    A('};')
    A('const uint8_t kConstCount = %d;' % len(cons))
    A('')
    A('const char kNameBlob[] = {')
    for i in range(0, len(blob), 24):
        A('    ' + ', '.join('%d' % b for b in blob[i:i + 24]) + ',')
    A('};')
    A('const uint16_t kNameBlobLen = %d;' % len(blob))
    A('')
    A('const Record kRecords[] = {')
    for ra, dec, maj, mnr, pa, mag, sb, flags, con, off in recs:
        A('    {%5d,%6d,%5d,%5d,%3d,%3d,%3d,%3d,%3d,%5d},'
          % (ra, dec, maj, mnr, pa, mag, sb, flags, cidx[con], off))
    A('};')
    A('const uint16_t kRecordCount = %d;' % len(recs))
    A('')
    A('} // namespace catalog')
    A('')
    A('#endif')
    with open(path, 'w') as f:
        f.write('\n'.join(lines) + '\n')

    by = collections.Counter()
    names = {0: 'Galaxy', 1: 'Planetary', 2: 'Globular', 3: 'Nebula',
             4: 'OpenCluster', 5: 'Remnant', 6: 'GalaxyGroup',
             7: 'Cluster+Neb'}
    for o in kept:
        by[names[o['type']]] += 1
    print('[%s] %d objects, %d B records + %d B names = %.1f KB  -> %s'
          % (profile, len(recs), len(recs) * 16, len(blob),
             (len(recs) * 16 + len(blob)) / 1024.0, os.path.basename(path)))
    for k, v in by.most_common():
        print('        %-14s %5d' % (k, v))


def main():
    src = sys.argv[1] if len(sys.argv) > 1 else 'theskylive.tsv'
    out = os.path.dirname(os.path.abspath(__file__)) + '/..'
    with open(src, encoding='utf-8-sig', newline='') as f:
        rows = list(csv.DictReader(f, delimiter='\t'))
    print('read %d source rows' % len(rows))
    for name, prof in PROFILES.items():
        kept, recs, blob = pack(rows, prof)
        emit(name, kept, recs, blob, '%s/Catalog_%s.cpp' % (out, name))
        with open('%s/tools/survivors_%s.csv' % (out, name), 'w',
                  newline='') as f:
            w = csv.writer(f)
            w.writerow(['desig', 'popular', 'type', 'con', 'ra_h', 'dec_deg',
                        'size_arcmin', 'mag', 'magband', 'sb'])
            for o in sorted(kept, key=lambda o: -(o['size'] or 0)):
                w.writerow([o['desig'], o['popular'], o['type'], o['con'],
                            round(o['ra'], 5), round(o['dec'], 4),
                            o['size'], o['mag'], 'B' if o['is_b'] else 'V',
                            o['sb']])


if __name__ == '__main__':
    main()
