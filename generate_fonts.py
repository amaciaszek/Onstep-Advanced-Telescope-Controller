#!/usr/bin/env python3
"""Generate 4-bit antialiased bitmap fonts for the OnStep remote.

The output is self-contained C++: no TTF parsing or font library is needed on
ESP32. Each glyph stores a packed 4-bit alpha mask plus placement metrics.
"""
from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import Iterable
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]

# These are build-time inputs only. The generated .cpp contains the rasterized
# glyphs, so the font files are not deployed to the controller.
FONT_SPECS = [
    ("Small", "/usr/share/fonts/opentype/inter/InterDisplay-Medium.otf", 9),
    ("Body",  "/usr/share/fonts/opentype/inter/InterDisplay-Medium.otf", 11),
    ("Value", "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf", 13),
    ("State", "/usr/share/fonts/opentype/inter/InterDisplay-SemiBold.otf", 19),
]

# Printable ASCII plus degree, middle dot, and left/right arrows. The arrows are
# useful for future menus; most directional UI is still drawn as vector shapes.
CODEPOINTS = list(range(0x20, 0x7F)) + [0x00B0, 0x00B7, 0x2190, 0x2192]

@dataclass
class Glyph:
    codepoint: int
    offset: int
    width: int
    height: int
    x_offset: int
    y_offset: int
    advance: int


def quantize_4bpp(alpha: bytes) -> bytes:
    nibbles = [(v * 15 + 127) // 255 for v in alpha]
    out = bytearray()
    for i in range(0, len(nibbles), 2):
        hi = nibbles[i]
        lo = nibbles[i + 1] if i + 1 < len(nibbles) else 0
        out.append((hi << 4) | lo)
    return bytes(out)


def render_font(path: str, px: int) -> tuple[list[Glyph], bytes, int, int]:
    font = ImageFont.truetype(path, px)
    ascent, descent = font.getmetrics()
    line_height = ascent + descent
    glyphs: list[Glyph] = []
    bitmap = bytearray()

    for cp in CODEPOINTS:
        ch = chr(cp)
        # Anchor at the baseline. Convert that to a y offset measured from the
        # top of a line box whose baseline is at `ascent`.
        left, top, right, bottom = font.getbbox(ch, anchor="ls")
        width = max(0, right - left)
        height = max(0, bottom - top)
        advance = max(1, int(round(font.getlength(ch))))
        y_offset = ascent + top

        offset = len(bitmap)
        if width and height:
            img = Image.new("L", (width, height), 0)
            draw = ImageDraw.Draw(img)
            # Draw so the glyph bounding box starts at image origin.
            draw.text((-left, -top), ch, font=font, fill=255, anchor="ls")
            bitmap.extend(quantize_4bpp(img.tobytes()))

        glyphs.append(Glyph(cp, offset, width, height, left, y_offset, advance))

    return glyphs, bytes(bitmap), line_height, ascent


def emit_bytes(data: bytes, indent: str = "    ") -> str:
    lines = []
    for i in range(0, len(data), 16):
        chunk = data[i:i + 16]
        lines.append(indent + ", ".join(f"0x{b:02X}" for b in chunk) + ",")
    return "\n".join(lines)


def emit_glyphs(glyphs: Iterable[Glyph], indent: str = "    ") -> str:
    lines = []
    for g in glyphs:
        lines.append(
            f"{indent}{{0x{g.codepoint:04X}, {g.offset}, {g.width}, {g.height}, "
            f"{g.x_offset}, {g.y_offset}, {g.advance}}},"
        )
    return "\n".join(lines)


def main() -> None:
    header = '''#pragma once
#include "BitmapFont.h"

namespace Fonts {
extern const BitmapFont Small;  // compact labels and button hints
extern const BitmapFont Body;   // normal UI labels
extern const BitmapFont Value;  // tabular/monospaced coordinates and numbers
extern const BitmapFont State;  // large mount state and warnings
}
'''
    (ROOT / "Fonts.h").write_text(header)

    cpp = ['#include "Fonts.h"', '', 'namespace {']
    declarations = []
    total = 0
    for name, path, px in FONT_SPECS:
        glyphs, bitmap, line_height, ascent = render_font(path, px)
        total += len(bitmap) + len(glyphs) * 16
        ident = name.lower()
        cpp.append(f'alignas(4) const uint8_t {ident}Bitmap[] = {{')
        cpp.append(emit_bytes(bitmap))
        cpp.append('};')
        cpp.append(f'const BitmapGlyph {ident}Glyphs[] = {{')
        cpp.append(emit_glyphs(glyphs))
        cpp.append('};')
        cpp.append('')
        declarations.append(
            f'const BitmapFont {name}{{{ident}Bitmap, {ident}Glyphs, '
            f'static_cast<uint16_t>(sizeof({ident}Glyphs) / sizeof({ident}Glyphs[0])), '
            f'{line_height}, {ascent}}};'
        )
    cpp.append('} // namespace')
    cpp.append('')
    cpp.append('namespace Fonts {')
    cpp.extend(declarations)
    cpp.append('} // namespace Fonts')
    cpp.append('')
    (ROOT / "Fonts.cpp").write_text("\n".join(cpp))
    print(f"Generated Fonts.cpp: approx {total / 1024:.1f} KiB including metadata")


if __name__ == "__main__":
    main()
