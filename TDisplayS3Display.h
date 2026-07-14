#pragma once

#if defined(ARDUINO)
#include <TFT_eSPI.h>
#include "FramebufferDisplay.h"

#ifndef ONSTEP_TDISPLAY_POWER_PIN
#define ONSTEP_TDISPLAY_POWER_PIN 15
#endif

// TFT_eSPI must be configured for Setup206_LilyGo_T_Display_S3.
// The board's native panel is 170x320; rotation 1 exposes 320x170.
class TDisplayS3Display final : public FramebufferDisplay {
public:
    TDisplayS3Display();

    bool begin();
    void setBacklight(bool on);
    TFT_eSPI& rawTft() { return tft_; }

protected:
    void flushRect(int x, int y, int w, int h,
                   const uint16_t* pixels, int stridePixels) override;

private:
    TFT_eSPI tft_;
};
#endif
