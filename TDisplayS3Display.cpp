#include "TDisplayS3Display.h"

#if defined(ARDUINO)
#include <Arduino.h>

TDisplayS3Display::TDisplayS3Display()
    : FramebufferDisplay(320, 170, 16, 10), tft_() {}

bool TDisplayS3Display::begin() {
    pinMode(ONSTEP_TDISPLAY_POWER_PIN, OUTPUT);
    digitalWrite(ONSTEP_TDISPLAY_POWER_PIN, HIGH);

    tft_.init();
    tft_.setRotation(3);
    tft_.setSwapBytes(true);
    tft_.fillScreen(TFT_BLACK);

    if (tft_.width() != width() || tft_.height() != height()) return false;
    return beginFramebuffer(true);
}

void TDisplayS3Display::setBacklight(bool on) {
    digitalWrite(ONSTEP_TDISPLAY_POWER_PIN, on ? HIGH : LOW);
}

void TDisplayS3Display::flushRect(int x, int y, int w, int h,
                                 const uint16_t* pixels, int stridePixels) {
    if (w <= 0 || h <= 0) return;

    tft_.startWrite();
    tft_.setAddrWindow(x, y, w, h);
    for (int row = 0; row < h; ++row) {
        const uint16_t* src = pixels + static_cast<size_t>(y + row) * stridePixels + x;
        tft_.pushPixels(src, static_cast<uint32_t>(w));
    }
    tft_.endWrite();
}
#endif
