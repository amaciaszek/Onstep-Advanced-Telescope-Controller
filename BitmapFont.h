#pragma once
#include <cstdint>

// A single pre-rasterized glyph. Bitmap data is a row-major 4-bit alpha mask,
// packed two pixels per byte (high nibble first). Metrics are measured in
// screen pixels. yOffset is relative to the top of the font's line box.
struct BitmapGlyph {
    uint32_t codepoint;
    uint32_t bitmapOffset;
    uint8_t  width;
    uint8_t  height;
    int8_t   xOffset;
    int8_t   yOffset;
    uint8_t  advance;
};

struct BitmapFont {
    const uint8_t*      bitmap;
    const BitmapGlyph*  glyphs;      // sorted by codepoint
    uint16_t            glyphCount;
    uint8_t             lineHeight;
    uint8_t             ascent;

    const BitmapGlyph* find(uint32_t codepoint) const {
        int lo = 0;
        int hi = static_cast<int>(glyphCount) - 1;
        while (lo <= hi) {
            const int mid = lo + (hi - lo) / 2;
            const uint32_t value = glyphs[mid].codepoint;
            if (value == codepoint) return &glyphs[mid];
            if (value < codepoint) lo = mid + 1;
            else hi = mid - 1;
        }
        return nullptr;
    }
};
