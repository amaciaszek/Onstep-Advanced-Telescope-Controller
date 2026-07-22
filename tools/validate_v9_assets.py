"""Fast consistency checks for the generated morphology catalog and art."""
from pathlib import Path
import re
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
CAT = (ROOT / "Catalog_long.cpp").read_text(encoding="utf-8")
PNG = ROOT / "assets" / "screen_png"

blob_src = re.search(r"const char kNameBlob\[\] = \{(.*?)\n\};", CAT, re.S)
records_src = re.search(r"const Record kRecords\[\] = \{(.*?)\n\};", CAT, re.S)
assert blob_src and records_src, "generated catalog sections missing"
blob = bytes(int(v) for v in re.findall(r"\d+", blob_src.group(1)))
records = [tuple(map(int, m)) for m in re.findall(
    r"\{\s*(-?\d+)\s*,\s*(-?\d+)\s*,\s*(-?\d+)\s*,\s*(-?\d+)\s*,"
    r"\s*(-?\d+)\s*,\s*(-?\d+)\s*,\s*(-?\d+)\s*,\s*(-?\d+)\s*,"
    r"\s*(-?\d+)\s*,\s*(-?\d+)\s*,\s*(-?\d+)\s*\}", records_src.group(1))]
assert len(records) == 4271, f"expected 4271 long records, got {len(records)}"
assert set(r[7] for r in records) == set(range(22)), "not all 22 subtypes occur"

def cstr(offset):
    end = blob.index(0, offset)
    return blob[offset:end].decode("ascii")

by_name = {cstr(r[10]): r for r in records}
for designation, subtype in {"M87": 0, "M51": 6, "M104": 4}.items():
    assert designation in by_name, f"{designation} missing"
    assert by_name[designation][7] == subtype, (
        f"{designation}: subtype {by_name[designation][7]}, expected {subtype}")

names = ["gal_e", "gal_es0", "gal_s0", "gal_s0a", "gal_sa", "gal_sb",
         "gal_sab", "gal_irr", "gal_generic", "gal_pair", "gal_triplet",
         "gal_group", "emission", "hii", "reflection", "dark", "planetary",
         "remnant", "clusterneb", "globular", "opencluster", "nebula"]
for name in names:
    for prefix in ("obj_", "n_obj_"):
        for suffix in ("", "_f"):
            path = PNG / f"{prefix}{name}{suffix}.png"
            assert path.exists(), f"missing {path.name}"
            with Image.open(path) as im:
                assert im.size == (18, 18), f"wrong size: {path.name} {im.size}"
                assert im.mode == "RGBA", f"wrong mode: {path.name} {im.mode}"

for i in range(8):
    for prefix in ("", "n_"):
        path = PNG / f"{prefix}moon_{i}.png"
        with Image.open(path) as im:
            assert im.size == (30, 30), f"wrong moon size: {path.name}"

print("OK: 4,271 records, all 22 subtypes, 88 glyphs, 16 moon sprites")
