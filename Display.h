#pragma once
#include <cstdint>
#include <string>
#include "BitmapFont.h"

struct Color {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    constexpr Color(uint8_t red = 0, uint8_t green = 0, uint8_t blue = 0)
        : r(red), g(green), b(blue) {}
};

inline bool operator==(Color a, Color b) {
    return a.r == b.r && a.g == b.g && a.b == b.b;
}
inline bool operator!=(Color a, Color b) { return !(a == b); }

// Abstract drawing target. The desktop simulator may implement drawPixel()
// directly. The ESP32 backend supplied with this package renders into an
// RGB565 framebuffer and pushes only changed tiles to the TFT.
class Display {
public:
    virtual ~Display() = default;

    virtual int  width()  const = 0;
    virtual int  height() const = 0;
    virtual void clear(Color c) = 0;
    virtual void drawPixel(int x, int y, Color c) = 0;
    virtual void present() = 0;

    // Software primitive defaults. Framebuffer backends override fillRect()
    // because bulk memory fills are much faster than repeated drawPixel calls.
    virtual void fillRect(int x, int y, int w, int h, Color c);
    virtual void drawRect(int x, int y, int w, int h, Color c);
    virtual void fillCircle(int cx, int cy, int r, Color c);
    virtual void drawCircle(int cx, int cy, int r, Color c);
    void drawHLine(int x, int y, int w, Color c);
    void drawVLine(int x, int y, int h, Color c);
    void drawLine(int x0, int y0, int x1, int y1, Color c);
    void fillTriangle(int x0, int y0, int x1, int y1, int x2, int y2, Color c);
    void fillRoundRect(int x, int y, int w, int h, int r, Color c);
    void drawRoundRect(int x, int y, int w, int h, int r, Color c);

    // Pre-rasterized 4-bit antialiased fonts. y is the top of the font line
    // box, not the glyph baseline. `background` must be the solid color under
    // the text; this lets the renderer precompute 16 blended shades once per
    // string instead of doing expensive per-pixel division.
    void drawText(int x, int y, const std::string& text,
                  const BitmapFont& font, Color foreground, Color background);
    void drawTextRight(int rightX, int y, const std::string& text,
                       const BitmapFont& font, Color foreground, Color background);
    void drawTextCentered(int centerX, int y, const std::string& text,
                          const BitmapFont& font, Color foreground, Color background);
    int  textWidth(const std::string& text, const BitmapFont& font) const;
    int  textHeight(const BitmapFont& font) const { return font.lineHeight; }

    // Legacy 5x7 overloads retained so platform/debug code that has not yet
    // migrated still compiles. The dashboard itself uses the AA overloads.
    void drawText(int x, int y, const std::string& text,
                  Color c, int scale = 1, bool bold = false);
    void drawTextRight(int rightX, int y, const std::string& text,
                       Color c, int scale = 1, bool bold = false);
    int  textWidth(const std::string& text, int scale = 1) const;
};
