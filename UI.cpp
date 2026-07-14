#include "UI.h"
#include "Fonts.h"
#include "Theme.h"
#include <cmath>

namespace ui {

Color lerp(Color a, Color b, float t) {
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    return Color{
        static_cast<uint8_t>(a.r + (b.r - a.r) * t),
        static_cast<uint8_t>(a.g + (b.g - a.g) * t),
        static_cast<uint8_t>(a.b + (b.b - a.b) * t)
    };
}

float pulse(double t, double speed) {
    return 0.5f + 0.5f * static_cast<float>(std::sin(t * speed));
}

Color dim(Color c, float k) {
    if (k < 0) k = 0;
    if (k > 1) k = 1;
    return Color{static_cast<uint8_t>(c.r * k),
                 static_cast<uint8_t>(c.g * k),
                 static_cast<uint8_t>(c.b * k)};
}

void panel(Display& d, int x, int y, int w, int h, Color fill, Color border) {
    d.fillRoundRect(x, y, w, h, 4, fill);
    d.drawRoundRect(x, y, w, h, 4, border);
    if (w > 10) d.drawHLine(x + 5, y + 1, w - 10, dim(border, 0.72f));
}

void statusDot(Display& d, int cx, int cy, int r, Color c, bool hollow) {
    if (hollow) d.drawCircle(cx, cy, r, c);
    else {
        d.fillCircle(cx, cy, r, c);
        d.drawCircle(cx, cy, r, lerp(c, Theme::TEXT, 0.18f));
    }
}

void wifiBars(Display& d, int x, int y, int bars, Color on, Color off) {
    for (int i = 0; i < 4; ++i) {
        const int bh = 3 + i * 3;
        d.fillRoundRect(x + i * 4, y + (12 - bh), 3, bh, 1,
                        i < bars ? on : off);
    }
}

void battery(Display& d, int x, int y, int pct, Color c) {
    const int w = 22, h = 11;
    d.drawRoundRect(x, y, w, h, 2, Theme::TEXT2);
    d.fillRect(x + w, y + 3, 2, h - 6, Theme::TEXT2);
    int fillw = (w - 4) * pct / 100;
    if (fillw < 0) fillw = 0;
    if (fillw > w - 4) fillw = w - 4;
    if (fillw > 0) d.fillRoundRect(x + 2, y + 2, fillw, h - 4, 1, c);
}

void dpad(Display& d, int cx, int cy, SlewDir active, float p) {
    const int reach = 23;
    const int halfBase = 5;
    const int depth = 7;
    auto arm = [&](SlewDir dir, int bx, int by, int dx, int dy) {
        const bool on = active == dir;
        const Color c = on
            ? lerp(Theme::BLUE, Theme::TEXT, 0.08f + 0.18f * p)
            : Theme::OFF;
        const int tx = bx + dx * depth;
        const int ty = by + dy * depth;
        if (dx != 0) d.fillTriangle(bx, by - halfBase, bx, by + halfBase, tx, ty, c);
        else d.fillTriangle(bx - halfBase, by, bx + halfBase, by, tx, ty, c);
    };

    arm(SlewDir::North, cx, cy - reach + depth, 0, -1);
    arm(SlewDir::South, cx, cy + reach - depth, 0, 1);
    arm(SlewDir::East, cx + reach - depth, cy, 1, 0);
    arm(SlewDir::West, cx - reach + depth, cy, -1, 0);

    const Color ring = active == SlewDir::None
        ? Theme::BORDER
        : lerp(Theme::BLUE, Theme::TEXT, 0.08f + 0.18f * p);
    d.fillCircle(cx, cy, 9, Theme::PANEL2);
    d.drawCircle(cx, cy, 9, ring);
    d.drawCircle(cx, cy, 8, dim(ring, 0.72f));
    d.fillCircle(cx, cy, 2, active == SlewDir::None ? Theme::OFF : ring);
}

void progressBar(Display& d, int x, int y, int w, int h, float frac,
                 Color fg, Color bg, Color border, float tipPulse) {
    if (frac < 0) frac = 0;
    if (frac > 1) frac = 1;
    d.fillRoundRect(x, y, w, h, 3, bg);
    const int fw = static_cast<int>((w - 2) * frac);
    if (fw > 0) {
        d.fillRoundRect(x + 1, y + 1, fw, h - 2, 2, fg);
        const Color tip = lerp(fg, Theme::TEXT, 0.28f + 0.34f * tipPulse);
        d.fillRect(x + fw - 1, y + 2, 2, h - 4, tip);
    }
    d.drawRoundRect(x, y, w, h, 3, border);
}

int buttonHint(Display& d, int x, int y, const char* key, const char* label,
               bool pressed) {
    const int kw = d.textWidth(key, Fonts::Small);
    const int lw = label && *label ? d.textWidth(label, Fonts::Small) + 3 : 0;
    const int w = 6 + kw + lw + 6;
    actionCell(d, x, y, w, key, label, pressed, false);
    return w;
}

void actionCell(Display& d, int x, int y, int w, const char* key,
                const char* label, bool pressed, bool danger) {
    const Color accent = danger ? Theme::RED : Theme::BLUE;
    const Color fill = pressed ? accent : Theme::PANEL2;
    const Color edge = danger ? dim(Theme::RED, 0.72f) : Theme::BORDER;
    const Color keyColor = pressed ? Theme::BG : (danger ? Theme::RED : Theme::TEXT);
    const Color labelColor = pressed ? Theme::BG : Theme::DIM;

    d.fillRoundRect(x, y, w, 15, 3, fill);
    d.drawRoundRect(x, y, w, 15, 3, pressed ? accent : edge);

    const int keyW = d.textWidth(key, Fonts::Small);
    const int labelW = label && *label ? d.textWidth(label, Fonts::Small) : 0;
    const int total = keyW + (labelW ? 4 + labelW : 0);
    const int tx = x + (w - total) / 2;
    d.drawText(tx, y + 1, key, Fonts::Small, keyColor, fill);
    if (labelW)
        d.drawText(tx + keyW + 4, y + 1, label, Fonts::Small, labelColor, fill);
}

void warningBanner(Display& d, int x, int y, int w, const char* text,
                   Color c, float blink) {
    const Color bg = lerp(Theme::REDDARK, c, 0.10f + 0.18f * blink);
    d.fillRoundRect(x, y, w, 16, 3, bg);
    d.drawRoundRect(x, y, w, 16, 3, c);
    const int ix = x + 8, iy = y + 3;
    d.fillTriangle(ix, iy + 9, ix + 9, iy + 9, ix + 4, iy, c);
    d.fillRect(ix + 4, iy + 3, 1, 4, Theme::BG);
    d.fillRect(ix + 4, iy + 8, 1, 1, Theme::BG);
    d.drawText(x + 24, y + 2, text, Fonts::Small, Theme::TEXT, bg);
}

void hazardFrame(Display& d, int x, int y, int w, int h, Color c) {
    d.drawRoundRect(x, y, w, h, 5, c);
    d.drawRoundRect(x + 2, y + 2, w - 4, h - 4, 4, dim(c, 0.45f));
    for (int px = x + 8; px < x + w - 8; px += 14) {
        d.drawLine(px, y + 1, px + 5, y + 1, c);
        d.drawLine(px, y + h - 2, px + 5, y + h - 2, c);
    }
}

void stopOctagon(Display& d, int cx, int cy, int r, Color c, Color text, Color background) {
    const int q = r / 2;
    const int x0 = cx - r, x1 = cx - q, x2 = cx + q, x3 = cx + r;
    const int y0 = cy - r, y1 = cy - q, y2 = cy + q, y3 = cy + r;
    d.drawLine(x1, y0, x2, y0, c);
    d.drawLine(x2, y0, x3, y1, c);
    d.drawLine(x3, y1, x3, y2, c);
    d.drawLine(x3, y2, x2, y3, c);
    d.drawLine(x2, y3, x1, y3, c);
    d.drawLine(x1, y3, x0, y2, c);
    d.drawLine(x0, y2, x0, y1, c);
    d.drawLine(x0, y1, x1, y0, c);
    d.drawTextCentered(cx, cy - Fonts::Small.lineHeight / 2,
                       "STOP", Fonts::Small, text, background);
}

} // namespace ui
