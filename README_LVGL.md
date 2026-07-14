# OnStepRemote LVGL visual prototype

This is the first live implementation of the chosen 320x170 dashboard concept.

## Required Arduino libraries
- TFT_eSPI (configured for LilyGO T-Display-S3 / Setup206)
- LVGL **8.3.x** (the code intentionally targets LVGL 8, matching the reference GitHub projects)
- Adafruit seesaw Library (optional gamepad; absence is non-fatal)

## Board settings
- ESP32S3 Dev Module / LilyGO T-Display-S3
- PSRAM: enabled
- USB CDC on boot: enabled if you use native USB serial

## Current live fields
- Wi-Fi and mount-link status
- Park/unpark glyph
- OnStep pulse-guide activity glyph (not the same as PHD2 lock state)
- Battery estimate
- ESP32 internal temperature (bring-up only, not ambient)
- Slew-rate selection
- RA, DEC, altitude, azimuth
- Pier-side/azimuth orientation graphic
- Experimental time-to-meridian from OnStep `:GS#` local sidereal time

## Deliberate first-pass limitations
- Bottom HOME/GOTO/ALIGN/MENU controls are visual placeholders.
- Guide trace shows OnStep pulse-guide activity history, not PHD2 RMS error.
- If `:GS#` is unsupported or empty, T-MER falls back to EAST/WEST/unknown.
- The radar uses Alt/Az rather than a star catalog or target-name database.

## Compiled image assets

The generated UI graphics are embedded directly in flash; no SD card or filesystem is required.

- Original generated PNGs: `assets/source_png/`
- Cleaned, screen-sized PNGs: `assets/screen_png/`
- LVGL declarations: `UiAssets.h`
- RGB565 + alpha image data: `UiAssets.cpp`
- Full filename/use mapping: `ASSET_MANIFEST.md`

The dashboard references every compiled asset from `LvglDashboard.cpp`, including both parked and unparked variants. Runtime state changes swap the park glyph and adjust status-icon opacity.
