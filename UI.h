#pragma once
#include "Display.h"
#include "MountStatus.h"

// Reusable dashboard components. Pure drawing on top of Display, so the
// same components render on desktop and on the ESP32 TFT unchanged.
namespace ui {

Color  lerp(Color a, Color b, float t);
float  pulse(double t, double speed = 6.0);       // 0..1 sine
Color  dim(Color c, float k);                      // scale brightness

void panel(Display& d, int x, int y, int w, int h, Color fill, Color border);
void statusDot(Display& d, int cx, int cy, int r, Color c, bool hollow);
void wifiBars(Display& d, int x, int y, int bars, Color on, Color off);
void battery(Display& d, int x, int y, int pct, Color c);
void dpad(Display& d, int cx, int cy, SlewDir active, float pulse);
void progressBar(Display& d, int x, int y, int w, int h, float frac,
                 Color fg, Color bg, Color border, float tipPulse);
int  buttonHint(Display& d, int x, int y, const char* key, const char* label,
                bool pressed);                     // returns width drawn
void actionCell(Display& d, int x, int y, int w, const char* key,
                const char* label, bool pressed, bool danger = false);
void warningBanner(Display& d, int x, int y, int w, const char* text,
                   Color c, float blink);
void hazardFrame(Display& d, int x, int y, int w, int h, Color c);
void stopOctagon(Display& d, int cx, int cy, int r, Color c, Color text, Color background);

} // namespace ui
