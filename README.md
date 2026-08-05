# OnStep Advanced Telescope Controller — Elecrow 3.5″ (LVGL)

Portrait 320×480 telescope controller for an OnStep-driven mount, running on the
Elecrow 3.5″ ESP32-S3 (ES3C35P). Touch-driven, LVGL 8.x, built directly on
Elecrow's own vendor display and touch libraries.

## Layout

```
OnStep_Elecrow_LVGL/
  OnStep_Elecrow_LVGL.ino     the application
  src/assets/                 Arduino-compiled generated assets
    onstep_assets.h           umbrella header — include this, get everything
    onstep_theme.{c,h}        day + night palettes, pre-snapped to RGB565
    onstep_{label,body,strong,value,head,hero}.c    Inter, 6 roles, 4bpp
    dso_icons_{24,48}.{c,h}   23 DSO type icons, LV_IMG_CF_ALPHA_4BIT
    ui_glyphs_{16,24}.{c,h}   35 UI glyphs, same format
sdcard/                       copy the CONTENTS of this to the microSD root
  sky/   mw_*.bin star_*.bin *.pal const_512.bin figures.bin
  moon/  moon_256.bin moon_512.bin
tools/                        regenerate any of the above
```

## Dependencies

Install from Elecrow's official resource pack (`1-Demo/Arduino/Install libraries/`):

- **ST77922** — display driver
- **ST77922_TOUCH** — touch driver
- **lvgl** 8.x

Nothing else. No TFT_eSPI: the vendor demos use it only as a software renderer
into a PSRAM sprite, and LVGL fills that role here.

## Arduino IDE settings

These are not optional.

| Setting | Value |
|---|---|
| Board | ESP32S3 Dev Module |
| **USB CDC On Boot** | **Enabled** — else `Serial` prints nothing over USB-C |
| Flash Size | 16MB (128Mb) |
| **PSRAM** | **OPI PSRAM** — the LVGL buffers and dome canvas need it |
| Partition Scheme | 16M Flash (3MB APP / 9.9MB FATFS) |

Arduino-ESP32 **3.x**.

## Current verified hardware baseline

The current application intentionally follows Elecrow's supplied LVGL/touch
demo without wrapper libraries: `ST77922`, `ST77922_TOUCH`, one full PSRAM
frame buffer, `mylcd.Fill_Colors()` in the LVGL flush callback, and
`my_touch.Get_Touch()` in the input callback. `full_refresh = 1` is enabled.

Open `OnStep_Elecrow_LVGL/OnStep_Elecrow_LVGL.ino`; do not use old sketches
that include `Elecrow35Display.h` or `Elecrow35Touch.h`, because those files
are not vendor libraries and are not part of this project.

## Two deliberate departures from the vendor demo

**Rotation 0 (portrait), not 1 (landscape).** In the vendor library's
`Fill_Colors()`, rotations 1 and 3 do a `ps_malloc` of the whole frame, a
per-pixel transpose, and a `free` — on *every call*. That is ~300 KB of heap
churn and 5–8 ms of copying per frame before anything reaches the wire.
Rotation 0 assigns `tx_buf = color` and pushes directly.

**Partial refresh, not `full_refresh = 1`.** The whole-frame restriction belongs
to the *rotated* path only. At rotation 0, `Fill_Colors(sx, sy, w, h, buf)`
takes a contiguous `w*h` buffer and windows it — exactly the shape LVGL's
partial flush provides. Full screen is 30.7 ms on the wire at 80 MHz; a single
catalog row is 1.8 ms.

The two are linked: rotation 0 is what makes partial refresh possible at all.

To go landscape anyway: set `PANEL_ROTATION 1`, `LVGL_PARTIAL_REFRESH 0`, and
swap `SCR_W`/`SCR_H`. It works; you pay the transpose every frame.

## Assets

**Fonts** are Inter, instantiated per role with the optical-size axis pinned,
and with `tnum` + `zero` baked into the cmap. Inter's default figures are
*proportional* — digit `1` is 833 units against `0` at 1292 — which would make
a live RA/Dec readout shuffle sideways every second. Verified after LVGL
conversion: all ten digits are 351 units in Hero.

**Icons and glyphs** are 4-bit alpha. That is already exactly
`LV_IMG_CF_ALPHA_4BIT`, so the conversion is a relabelling with no resampling.
LVGL tints them from `img_recolor` at draw time, which is why one asset serves
both the day and night palettes.

**Colours** are pre-snapped to the RGB565 grid. Changing a red or blue channel
by less than 8, or green by less than 4, changes nothing on glass. Minimum
pairwise separation after quantisation is 22.6 in both palettes — the first
pass failed this, with `Surface2` and `RuleFaint` collapsing to the same code.

**Flash cost:** ~47 KB icons + glyphs, ~120 KB fonts.

## SD card

Copy the contents of `sdcard/` to the card root. 1.6 MB total.

`figures.bin` holds 742 constellation polylines / 1445 segments in 14.3 KB,
traced from the NASA raster by skeletonisation (99.93% / 98.46% agreement both
ways). Note the artwork's lines are **not** great circles — assuming geodesics
silently drops about a fifth of them.

Sky textures are 512×256, which is the matched size for a 208 px dome
(0.70°/texel against 0.87°/px). 1024 is 2.5× oversampled and nearest-neighbour
sampling of an oversampled texture aliases *worse*.

## Not yet wired

The sketch runs standalone on mock data. To finish it:

1. **`OnStepClient`** — replace `g_mount` mock updates. Put the poll on a
   FreeRTOS task pinned to **core 0** so a blocking socket never stalls the
   renderer: `xTaskCreatePinnedToCore(pollerTask, "poll", 6144, nullptr, 4, nullptr, 0)`.
2. **Sky texture** — load `sky/mw_512.bin` + palette into PSRAM and replace the
   procedural star field in `drawDome()`. The projection around it is already
   correct, so only the inner loop changes.
3. **Static screen→(HA,Dec) LUT** — alt/az depends only on hour angle,
   declination and latitude, none of which change during a night. Time enters
   only as `H = LST − RA`, so the mapping is constant and advancing time is one
   integer offset. ~3 ms per dome, no runtime trigonometry.
4. **DSO imagery** — three tiers as raw RGB565 in fixed-size records, so the
   offset of object N is `N × record_size`.

## Regenerating

```
python tools/01_fonts.py         # Inter -> per-role TTFs, tabular baked in
python tools/02_theme.py         # palettes, with RGB565 collision checks
python tools/03_dso_icons.py     # 23 icons, verified pairwise-distinct
python tools/04_ui_glyphs.py     # 35 glyphs
python tools/05_moon.py          # albedo + Lommel-Seeliger renderer
python tools/13_lvgl_assets.py   # -> LVGL C descriptors
npm i -g lv_font_conv            # then see tools/make_fonts.sh
```
