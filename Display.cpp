#include "Display.h"
#include "Font5x7.h"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <utility>

namespace {

void circleOctants(Display& d, int cx, int cy, int x, int y, Color c) {
    d.drawPixel(cx + x, cy + y, c);
    d.drawPixel(cx - x, cy + y, c);
    d.drawPixel(cx + x, cy - y, c);
    d.drawPixel(cx - x, cy - y, c);
    d.drawPixel(cx + y, cy + x, c);
    d.drawPixel(cx - y, cy + x, c);
    d.drawPixel(cx + y, cy - x, c);
    d.drawPixel(cx - y, cy - x, c);
}

void roundedCornerOctants(Display& d,
                          int tlx, int tly, int trx, int try_,
                          int blx, int bly, int brx, int bry,
                          int x, int y, Color c) {
    d.drawPixel(tlx - x, tly - y, c);
    d.drawPixel(tlx - y, tly - x, c);
    d.drawPixel(trx + x, try_ - y, c);
    d.drawPixel(trx + y, try_ - x, c);
    d.drawPixel(blx - x, bly + y, c);
    d.drawPixel(blx - y, bly + x, c);
    d.drawPixel(brx + x, bry + y, c);
    d.drawPixel(brx + y, bry + x, c);
}

Color blend15(Color bg, Color fg, uint8_t alpha) {
    const uint16_t inv = static_cast<uint16_t>(15 - alpha);
    return Color{
        static_cast<uint8_t>((bg.r * inv + fg.r * alpha + 7) / 15),
        static_cast<uint8_t>((bg.g * inv + fg.g * alpha + 7) / 15),
        static_cast<uint8_t>((bg.b * inv + fg.b * alpha + 7) / 15)
    };
}

// Small UTF-8 decoder sufficient for the generated font's ASCII, degree sign,
// middle dot, and arrow codepoints. Invalid sequences become '?'.
uint32_t nextCodepoint(const std::string& text, size_t& index) {
    const uint8_t c0 = static_cast<uint8_t>(text[index++]);
    if (c0 < 0x80) return c0;

    auto continuation = [&](uint8_t& out) -> bool {
        if (index >= text.size()) return false;
        out = static_cast<uint8_t>(text[index++]);
        return (out & 0xC0) == 0x80;
    };

    uint8_t c1 = 0, c2 = 0, c3 = 0;
    if ((c0 & 0xE0) == 0xC0 && continuation(c1))
        return ((c0 & 0x1F) << 6) | (c1 & 0x3F);
    if ((c0 & 0xF0) == 0xE0 && continuation(c1) && continuation(c2))
        return ((c0 & 0x0F) << 12) | ((c1 & 0x3F) << 6) | (c2 & 0x3F);
    if ((c0 & 0xF8) == 0xF0 && continuation(c1) && continuation(c2) && continuation(c3))
        return ((c0 & 0x07) << 18) | ((c1 & 0x3F) << 12) |
               ((c2 & 0x3F) << 6) | (c3 & 0x3F);
    return '?';
}

uint8_t alphaNibble(const uint8_t* data, uint32_t pixelIndex) {
    const uint8_t packed = data[pixelIndex >> 1];
    return (pixelIndex & 1U) ? (packed & 0x0F) : (packed >> 4);
}

} // namespace

void Display::fillRect(int x, int y, int w, int h, Color c) {
    if (w <= 0 || h <= 0) return;
    for (int j = 0; j < h; ++j)
        for (int i = 0; i < w; ++i)
            drawPixel(x + i, y + j, c);
}

void Display::drawRect(int x, int y, int w, int h, Color c) {
    if (w <= 0 || h <= 0) return;
    drawHLine(x, y, w, c);
    drawHLine(x, y + h - 1, w, c);
    drawVLine(x, y, h, c);
    drawVLine(x + w - 1, y, h, c);
}

void Display::fillCircle(int cx, int cy, int r, Color c) {
    if (r < 0) return;
    for (int y = -r; y <= r; ++y) {
        int x = 0;
        while ((x + 1) * (x + 1) + y * y <= r * r) ++x;
        drawHLine(cx - x, cy + y, x * 2 + 1, c);
    }
}

void Display::drawCircle(int cx, int cy, int r, Color c) {
    if (r < 0) return;
    int x = r;
    int y = 0;
    int err = 1 - r;
    while (x >= y) {
        circleOctants(*this, cx, cy, x, y, c);
        ++y;
        if (err < 0) err += 2 * y + 1;
        else { --x; err += 2 * (y - x) + 1; }
    }
}

void Display::drawHLine(int x, int y, int w, Color c) { fillRect(x, y, w, 1, c); }
void Display::drawVLine(int x, int y, int h, Color c) { fillRect(x, y, 1, h, c); }

void Display::drawLine(int x0, int y0, int x1, int y1, Color c) {
    int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        drawPixel(x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        const int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void Display::fillTriangle(int x0, int y0, int x1, int y1, int x2, int y2, Color c) {
    if (y0 > y1) { std::swap(y0, y1); std::swap(x0, x1); }
    if (y1 > y2) { std::swap(y1, y2); std::swap(x1, x2); }
    if (y0 > y1) { std::swap(y0, y1); std::swap(x0, x1); }
    auto edge = [](int ya, int yb, int xa, int xb, int y) -> int {
        if (yb == ya) return xa;
        return xa + (xb - xa) * (y - ya) / (yb - ya);
    };
    for (int y = y0; y <= y2; ++y) {
        int xa = (y < y1) ? edge(y0, y1, x0, x1, y)
                          : edge(y1, y2, x1, x2, y);
        int xb = edge(y0, y2, x0, x2, y);
        if (xa > xb) std::swap(xa, xb);
        drawHLine(xa, y, xb - xa + 1, c);
    }
}

void Display::fillRoundRect(int x, int y, int w, int h, int r, Color c) {
    if (w <= 0 || h <= 0) return;
    r = std::max(0, std::min(r, std::min(w / 2, h / 2)));
    if (r == 0) { fillRect(x, y, w, h, c); return; }
    fillRect(x + r, y, w - 2 * r, h, c);
    fillRect(x, y + r, w, h - 2 * r, c);
    fillCircle(x + r, y + r, r, c);
    fillCircle(x + w - r - 1, y + r, r, c);
    fillCircle(x + r, y + h - r - 1, r, c);
    fillCircle(x + w - r - 1, y + h - r - 1, r, c);
}

void Display::drawRoundRect(int x, int y, int w, int h, int r, Color c) {
    if (w <= 0 || h <= 0) return;
    r = std::max(0, std::min(r, std::min(w / 2, h / 2)));
    if (r == 0) { drawRect(x, y, w, h, c); return; }

    drawHLine(x + r, y, w - 2 * r, c);
    drawHLine(x + r, y + h - 1, w - 2 * r, c);
    drawVLine(x, y + r, h - 2 * r, c);
    drawVLine(x + w - 1, y + r, h - 2 * r, c);

    const int tlx = x + r, tly = y + r;
    const int trx = x + w - r - 1, try_ = y + r;
    const int blx = x + r, bly = y + h - r - 1;
    const int brx = x + w - r - 1, bry = y + h - r - 1;
    int px = r, py = 0, err = 1 - r;
    while (px >= py) {
        roundedCornerOctants(*this, tlx, tly, trx, try_, blx, bly, brx, bry,
                             px, py, c);
        ++py;
        if (err < 0) err += 2 * py + 1;
        else { --px; err += 2 * (py - px) + 1; }
    }
}

void Display::drawText(int x, int y, const std::string& text,
                       const BitmapFont& font, Color foreground, Color background) {
    Color shades[16];
    for (uint8_t a = 0; a < 16; ++a) shades[a] = blend15(background, foreground, a);

    int cursorX = x;
    int cursorY = y;
    size_t index = 0;
    while (index < text.size()) {
        uint32_t cp = nextCodepoint(text, index);
        if (cp == '\n') {
            cursorX = x;
            cursorY += font.lineHeight;
            continue;
        }

        const BitmapGlyph* glyph = font.find(cp);
        if (!glyph) glyph = font.find('?');
        if (!glyph) continue;

        const uint8_t* pixels = font.bitmap + glyph->bitmapOffset;
        for (uint8_t gy = 0; gy < glyph->height; ++gy) {
            for (uint8_t gx = 0; gx < glyph->width; ++gx) {
                const uint32_t p = static_cast<uint32_t>(gy) * glyph->width + gx;
                const uint8_t alpha = alphaNibble(pixels, p);
                if (alpha == 0) continue;
                drawPixel(cursorX + glyph->xOffset + gx,
                          cursorY + glyph->yOffset + gy,
                          shades[alpha]);
            }
        }
        cursorX += glyph->advance;
    }
}

void Display::drawTextRight(int rightX, int y, const std::string& text,
                            const BitmapFont& font, Color foreground, Color background) {
    drawText(rightX - textWidth(text, font), y, text, font, foreground, background);
}

void Display::drawTextCentered(int centerX, int y, const std::string& text,
                               const BitmapFont& font, Color foreground, Color background) {
    drawText(centerX - textWidth(text, font) / 2, y, text, font, foreground, background);
}

int Display::textWidth(const std::string& text, const BitmapFont& font) const {
    int width = 0;
    int maxWidth = 0;
    size_t index = 0;
    while (index < text.size()) {
        const uint32_t cp = nextCodepoint(text, index);
        if (cp == '\n') {
            maxWidth = std::max(maxWidth, width);
            width = 0;
            continue;
        }
        const BitmapGlyph* glyph = font.find(cp);
        if (!glyph) glyph = font.find('?');
        if (glyph) width += glyph->advance;
    }
    return std::max(maxWidth, width);
}

// ---------------------------------------------------------------------
// Legacy 5x7 renderer
// ---------------------------------------------------------------------
void Display::drawText(int x, int y, const std::string& text,
                       Color c, int scale, bool bold) {
    if (scale < 1) scale = 1;
    int cursorX = x;
    for (char raw : text) {
        char ch = static_cast<char>(std::toupper(static_cast<unsigned char>(raw)));
        if (ch < FONT5x7_FIRST || ch > FONT5x7_LAST) ch = '?';
        const uint8_t* glyph = FONT5x7[ch - FONT5x7_FIRST];
        for (int col = 0; col < 5; ++col) {
            for (int row = 0; row < 7; ++row) {
                if (!(glyph[col] & (1U << row))) continue;
                fillRect(cursorX + col * scale, y + row * scale, scale, scale, c);
                if (bold) drawVLine(cursorX + col * scale + scale, y + row * scale, scale, c);
            }
        }
        cursorX += 6 * scale;
    }
}

void Display::drawTextRight(int rightX, int y, const std::string& text,
                            Color c, int scale, bool bold) {
    drawText(rightX - textWidth(text, scale), y, text, c, scale, bold);
}

int Display::textWidth(const std::string& text, int scale) const {
    if (text.empty()) return 0;
    if (scale < 1) scale = 1;
    return static_cast<int>(text.size()) * 6 * scale - scale;
}
