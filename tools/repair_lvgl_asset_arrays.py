"""Repair generated LVGL alpha-map arrays broken by character-based wrapping.

Older generator output split `0xNN` literals across physical lines. The data
itself is intact: each map body can be joined, parsed into byte tokens, and
re-emitted with line breaks only between whole byte literals.
"""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
ASSET_DIR = ROOT / "OnStep_Elecrow_LVGL" / "src" / "assets"
FILES = ("dso_icons_24.c", "dso_icons_48.c", "ui_glyphs_16.c", "ui_glyphs_24.c")
ARRAY = re.compile(
    r"(static const uint8_t \w+_map\[\] = \{\n)(.*?)(\n\};)", re.DOTALL
)


def repair_array(match: re.Match) -> str:
    compact = re.sub(r"\s+", "", match.group(2))
    residue = re.sub(r"0x[0-9A-Fa-f]{2}|,", "", compact)
    if residue:
        raise ValueError(f"Unexpected array content: {residue[:40]!r}")
    tokens = re.findall(r"0x[0-9A-Fa-f]{2}", compact)
    if not tokens:
        raise ValueError("Empty byte array")
    lines = ["  " + ",".join(tokens[i:i + 16]) + ","
             for i in range(0, len(tokens), 16)]
    return match.group(1) + "\n".join(lines) + match.group(3)


def main() -> None:
    total_arrays = total_bytes = 0
    for name in FILES:
        path = ASSET_DIR / name
        text = path.read_text(encoding="utf-8")
        count = 0

        def replace(match: re.Match) -> str:
            nonlocal count, total_bytes
            count += 1
            total_bytes += len(re.findall(r"0x[0-9A-Fa-f]{2}", re.sub(r"\s+", "", match.group(2))))
            return repair_array(match)

        repaired = ARRAY.sub(replace, text)
        if count == 0:
            raise RuntimeError(f"No byte arrays found in {path}")
        path.write_text(repaired, encoding="utf-8", newline="\n")
        total_arrays += count
        print(f"{name}: repaired {count} arrays")
    print(f"Repaired {total_arrays} arrays / {total_bytes} bytes.")


if __name__ == "__main__":
    main()
