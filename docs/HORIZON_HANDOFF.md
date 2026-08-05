# Horizon Profile — Full Rundown and Project Handoff

Written as a self-contained brief. Part 1 is project context for a fresh conversation;
Part 2 is the horizon feature itself.

---

# PART 1 — PROJECT CONTEXT

## 1.1 What this is

**OnStep Advanced Telescope Controller** — a handheld controller for an OnStep-driven
GEM (CGEM II with a C8 SCT), talking LX200 protocol over Wi-Fi. Repo:
`github.com/amaciaszek/Onstep-Advanced-Telescope-Controller`.

Currently running on a **LilyGO T-Display-S3** (1.9″, 320×170 landscape). Being ported to
an **Elecrow 3.5″ ESP32-S3** (320×480 portrait), which is the reason for everything below.

## 1.2 Target hardware — Elecrow 3.5″ ESP32-S3

| | |
|---|---|
| MCU | ESP32-S3 N16R8, dual Xtensa LX7 @ 240 MHz, 512 KB SRAM, 8 MB OPI PSRAM, 16 MB flash |
| Panel | ST77922, 320×480, **QSPI** (not SPI), RGB565, IPS, 300 cd/m² |
| Active area | 48.96 × 73.44 mm, **0.153 mm pitch** (T-Display was ~0.133) |
| PCB | 101.50 × 54.50 × 10 mm, mounting holes 94.50 apart |
| Touch | capacitive, I²C |
| Storage | microSD on 4-bit SDIO |
| Power | 1.45 W typical, 120 mA backlight; ~190 mA at the cell at 25% brightness |

**Pin map** (all verified against the Elecrow datasheet):

```
LCD    CS 10 · SCK 12 · D0 11 · D1 13 · D2 14 · D3 9 · RST on EN (-1) · BL 41 (PWM)
Touch  I2C SDA 38 / SCL 39 (shared with audio codec and the expansion socket)
       RST 48 · IRQ 47          <- both OUTSIDE GPIO0-21, so touch cannot wake from sleep
SD     CLK 5 · CMD 4 · D0-3 6/7/2/3
Audio  EN 1 · MCLK 17 · BCLK 18 · DOUT 16 · LRCK 21 · DIN 15
RGB    IO40 (addressable, single-wire)
Batt   IO8 (ADC1 — usable with Wi-Fi active; ADC2 would not be)
Expand IO45, IO46 (strapping pins, NOT RTC-capable)
```

**Constraints that keep biting:**
- **IO14 is QSPI D2** — the old enclosure button 2 has no home.
- RTC-capable GPIOs are 0–21 and this board spends nearly all of them. **Dropping the
  speaker frees IO1/15/16/17/18/21**, all RTC-capable, for buttons and deep-sleep wake.
- The board is one PCB, so nothing can be power-gated from inside the S3.

## 1.3 Codebase architecture

Rendering is a clean three-layer stack and the port seam is tiny:

```
Display            abstract: width/height/clear/drawPixel/present + software primitives
  └ FramebufferDisplay   RGB565 double buffer + dirty-tile diffing
      └ TDisplayS3Display     TFT_eSPI backend        (existing)
      └ Elecrow35Display      esp_lcd + ST77922 QSPI  (written, untested on hardware)
```

`FramebufferDisplay` has **exactly one** pure virtual for the backend:

```cpp
virtual void flushRect(int x, int y, int w, int h,
                       const uint16_t* pixels, int stridePixels) = 0;
```

`Elecrow35Display.{h,cpp}` exists and implements it via `esp_lcd_st77922` (ESP component
registry). Key differences from the TFT_eSPI version, all documented in the file:
esp_lcd needs a contiguous staging buffer in DMA-capable **internal** RAM (so every flush
is a gather-copy), `draw_bitmap` is async and needs a completion semaphore, QSPI uses
`dc_gpio_num = -1` and `lcd_cmd_bits = 32`, and byte-swapping replaces `setSwapBytes(true)`.

**Three open items, in priority order:**

1. **Write-time dirty marking.** `present()` currently compares the whole backbuffer
   against a front buffer — 600 KB of PSRAM read per present at 320×480, roughly 10–15 ms
   of pure overhead. Mark tiles dirty in `drawPixel`/`fillRect` instead; the front buffer
   disappears and 300 KB comes back. **Prerequisite for any animation.**
2. **`drawTextBlended()`.** AA text currently requires a known solid background colour
   (it precomputes 16 blended shades per string). That blocks all text over imagery —
   constellation labels on the dome, object names over photos. Fix is ~10 lines: read the
   framebuffer pixel, unpack 565, blend against the glyph's 4-bit coverage, repack.
3. **Persistent socket.** `OnStepClient` is connect-per-command — a fresh TCP handshake
   per query, ~5–7 packets where 2 would do. Dominant term in the radio power budget.
   Also: move the poller to a **FreeRTOS task pinned to core 0** so blocking network calls
   stop stalling the renderer.

## 1.4 Design language

Neutral graphite ground, **single cyan accent**, imagery is the only other saturated thing
on screen. Night mode rotates everything — photographs included — to a single red channel.
Hairlines instead of bordered cards; hierarchy by type size and weight.

`Theme565.h` is generated with every colour snapped to the RGB565 grid (zero drift,
minimum pairwise separation 22.6). This matters more than it sounds: the first pass had
`Surface2` and `RuleFaint` quantising to the *identical* code.

**RGB565 is the dominant image-quality constraint**, not resolution. A sky pixel at 8-bit
level 16 has only three distinct red/blue codes below it. Fix is free: bake 16
Bayer-dithered palettes (16 × 256 × 2 B = **8 KB**) and index with
`pal[(y&3)<<2 | (x&3)][value]`.

## 1.5 The sky renderer — and the insight it rests on

Horizon coordinates depend only on hour angle, declination and latitude — **none of which
change during a night**. Time enters only through `H = LST − RA`. So for a fixed latitude
and dome radius, the screen→(HA, Dec) mapping is **constant**, and advancing time is a
single integer offset applied identically to every pixel.

```c
uint16_t off = (uint16_t)(lst_fraction * TEX_W);
for (i = 0; i < N; i++) {
    uint32_t u = (u0[i] + off) & (TEX_W - 1);
    fb[px[i]] = palette[tex[(v[i] << TEX_W_SHIFT) | u]];
}
```

No trigonometry at runtime. ~3 ms per dome. Two further reductions: dropping the
framebuffer-index array (row spans make it implicit) halves the LUT, and **mirror symmetry
about the meridian** halves it again — reflecting azimuth leaves declination untouched and
flips hour angle, so the left half of the dome is the right half with `u0` negated.
265 KB → **66 KB**.

**512 × 256 is the correct texture size** for a 208 px dome (0.703°/texel against
0.865°/px). 1024 is 2.5× oversampled and nearest-neighbour sampling of an oversampled
texture *aliases worse*.

**Redraw cadence beats optimisation.** The rim of a 208 px dome moves one pixel in ~3.5
minutes. Redraw on pixel-motion, not on a timer.

### Source raster convention (hard-won — do not re-derive)

NASA SVS Deep Star Maps 2020 are laid out as **inside-of-sphere** textures:

```
u_src = ((180 - RA_deg) / 360) mod 1      RA increases RIGHT to LEFT, RA 0h at CENTRE
v_src = (90 - Dec_deg) / 180              Dec +90 at top
```

This took four attempts. The galactic bulge test, band-flatness test and pole test all
gave *contradictory* answers because the galactic centre and anticentre are antipodal, so
a doubly-mirrored map still produces a plausible band. FFT cross-correlation against a
synthetic catalog map settled it; the winner then predicted Polaris at u = 0.3946 against
0.3947 measured independently, and all 12 verification stars land on the exact pixel.

The pipeline remaps to `u_out = RA/360` as a pure index permutation (no resampling).

## 1.6 Assets already built (`onstep_art_pack.zip`, `sky_pyramid.zip`)

| Asset | Detail |
|---|---|
| **Fonts** | Inter, six roles (Label 9 / Body 12 / Value 15 / Strong 12 / Head 20 / Hero 34), optical size pinned per role, **`tnum` and `zero` baked into the cmap** — Inter's default figures are proportional and would make a live RA/Dec readout shuffle |
| **Theme565.h** | day + night, every value on the 565 grid |
| **DSO icons** | 23 types, 4-bit alpha, 24 px and 48 px, all 253 pairs ≥ 0.13 RMS apart |
| **UI glyphs** | 35, 4-bit alpha, 16 px and 24 px |
| **Milky Way + stars** | separate 8-bit layers, luminance-ordered palette, star map pre-blurred before downsample to kill sampling twinkle |
| **Constellation figures** | 742 polylines / 1445 segments / **14.3 KB**, traced from the NASA raster by skeletonisation; agrees 99.93% / 98.46% both ways. NOTE: the artwork's lines are **not** great circles — assuming geodesics silently drops ~20% of them |
| **Moon** | real albedo, Lommel–Seeliger shading, libration as a normal rotation before texture fetch; illuminated fraction tracks (1−cos φ)/2 to 0.02 |
| **Sky mip pyramid** | 512 → 8192, tiled 256×256 above 4096; ~9 tiles (0.6 MB) resident regardless of depth |

## 1.7 Other systems specified

- **DSO imagery**: three tiers (thumb 40×40, portrait 320×214, field 320×320) as raw
  RGB565 **big-endian**, packed into fixed-size records so offset = `N × record_size`.
  Full NGC+IC = 4.81 GB. Field is square and ≥ sensor diagonal so the **sensor rectangle
  rotates over it** — rotating an outline is free, rotating a bitmap is not.
- **Power**: backlight 21%, radio 37%, CPU 19%. Animation costs ~0.07% of a session;
  auto-dim + idle poll rate + CPU scaling + modem sleep return **37%**.
- **I²C add-ons on the expansion socket**: DS3231 RTC (0x68), BME280 (0x76), DRV2605L
  haptic (0x5A). Bus is IO38/39, shared with touch and the seesaw gamepad at 0x50.

---

# PART 2 — THE HORIZON PROFILE FEATURE

## 2.1 What it is

A per-site function **altitude(azimuth)** describing the real skyline — trees, roofs,
ridge. Used to:

1. Mask the sky dome so it shows *your* horizon, not a mathematical circle.
2. Replace "rises above 30°" with "**clears your treeline at 22:41**".
3. Rank "best tonight" by hours above the *real* horizon.

## 2.2 Data format — 764 bytes

```c
struct HorizonProfile {          // "HZN1"
  char     magic[4];
  uint16_t samples;              // 720 (0.5° steps)
  int16_t  az_offset_x10;        // user calibration, applied at load
  float    lat, lon;             // capture site
  uint32_t epoch;                // capture time
  char     name[24];             // "Home — back yard"
  uint8_t  alt[720];             // altitude in 0.5° steps, 0..127.5°
};
```

Sampling guidance: **0.5–1° captures individual trees**, 2° captures a treeline, 5°+ only
captures a ridge. Go with 720 samples; it costs nothing.

**Transfer is a non-problem at this size.** The hard parts are capture and azimuth.

## 2.3 The three approaches, decided

### Option 1 — camera on the device: **dead, and not marginally**

The ESP32-S3 has a DVP camera interface, but it needs ~11 pins and this board has **two
free** (IO45/46, both strapping pins). Even dropping the speaker gets you to eight. There
is no path to a camera on this hardware without a different board.

Even if there were, you'd still need absolute azimuth, which means a magnetometer, which
sits next to a steel tripod and stepper motors and is ~5° accurate at best. You'd have
added cost and complexity to arrive at the same azimuth error the phone already has.

### Option 2 — point the mount: **viable, tedious, but keep it**

OnStep reports alt/az directly (`:GA#`, `:GZ#`) regardless of mount type, so the GEM/alt-az
distinction doesn't matter for *reading*. Slewing a GEM to horizon points is the hassle —
and genuinely risky near the horizon.

But you don't need to slew. **Jog with the D-pad, press a button to record a point.** And
you're right that it yields few vectors: 20–40 points with interpolation, which is a ridge,
not a treeline.

**Do not discard this.** It becomes the calibration step in §2.5, which is what makes the
phone data trustworthy.

### Option 3 — phone: **the answer, with one correction**

Not an Android app — **a mobile web app**. You already build HTML5 front-ends (GuestGuard),
`getUserMedia` and `DeviceOrientationEvent` give you camera and fused orientation, and it
works on iOS too. No app store, no Android Studio, no second toolchain to maintain.

## 2.4 Error budget — why this works

| Axis | Source | Accuracy |
|---|---|---|
| **Altitude** | accelerometer (gravity vector) | **0.5–1°** |
| Azimuth | magnetometer | 5–10°, worse near steel |

**Altitude is the axis that answers the question, and altitude is the accurate one.**
Azimuth error simply *rotates the whole profile*, which one offset corrects. This is the
observation the entire design leans on.

## 2.5 Recommended method — phone for shape, mount for azimuth

1. **Capture in daylight.** Sky/ground segmentation needs light, and trees don't move.
   Overcast is ideal (flat, bright sky, high contrast against foliage).
2. **Eight photos at ~50° spacing** (a phone main camera is ~65° horizontal FOV, so this
   overlaps). The app records yaw/pitch/roll with each frame.
3. **Segment each frame per column** — find the sky/ground boundary. Sky is brighter and
   less saturated; a per-column threshold on `V − S` works well. Falls back to letting the
   user drag the line where it fails (power lines, thin branches).
4. **Project to (az, alt)** with a pinhole model and roll correction:
   ```
   f  = (W/2) / tan(hfov/2)
   Xc = (x - W/2)cos(roll) + (y - H/2)sin(roll)
   Yc = -(x - W/2)sin(roll) + (y - H/2)cos(roll)
   az  = yaw   + atan2(Xc, f)
   alt = pitch - atan2(Yc, f)
   ```
5. **Resample to 720 bins**, median per bin, interpolate gaps, light smoothing.
6. **Export `.hzn`.**
7. **Calibrate azimuth on the device.** Jog the mount to one distinctive horizon feature —
   a chimney, a lone tree — and press a button. The device compares to the profile and
   solves the single offset. Two features gives a sanity check.

That last step is where Option 2 earns its place: the phone gives you 720 points of shape
in 30 seconds, the mount gives you the one number the phone can't.

## 2.6 Transfer — the mixed-content trap

`getUserMedia` and `DeviceOrientationEvent` both require a **secure context**. A page
served over plain HTTP from the device on a LAN IP will not get camera or orientation
access. And an HTTPS page cannot POST to a plain-HTTP device — mixed content is blocked.

The way through is that these are **two separate page loads**, not one page talking to two
origins:

1. **Capture page**, hosted on GitHub Pages (HTTPS). Camera + orientation work. Exports
   `.hzn` to the phone's Downloads.
2. **Upload page**, served by the device over plain HTTP from its own AP. A plain
   `<form enctype="multipart/form-data">` — no camera needed, so no secure context needed.
   Fully offline in a field.

Fallback for v1 with zero device code: **put the `.hzn` on the SD card.** You do this once
per site.

## 2.7 On-device rendering — essentially free

The horizon test is a **pure function of screen position**: azimuth from `atan2(-dx,-dy)`,
altitude from `90(1 − r/R)`. Neither depends on time. So precompute a bitmask once when the
profile loads:

**33,979 dome pixels = 4,247 bytes.** The dome render then adds one bit test per pixel.

Rendering:
- Below-horizon pixels → near-black terrain fill.
- Boundary drawn in `TextLow` as a 1 px silhouette line.
- Optionally store the panorama itself (~3 MB) and texture the terrain — but the line
  alone reads well and costs nothing.

**Rise/set correction** reuses the trajectory arc code exactly: walk the object's track,
find where `alt(t) > horizon(az(t))`. One extra comparison inside a loop you already have.

## 2.8 Build order

1. Format + loader + bitmask + dome masking. **Test with a hand-written profile** — a flat
   20° wall, then a single 40° tree — before any capture code exists.
2. "Clears at HH:MM" on the object detail and Sky screens.
3. Web capture app: orientation only, user traces the line by dragging. No segmentation.
   Ugly but complete, and it proves the pipeline end to end.
4. Add automatic segmentation.
5. Add the mount-based azimuth calibration.
6. Multiple named sites; a picker in Settings.

Step 3 before step 4 matters — manual tracing is a working feature on its own, and it
stays as the fallback for the frames where segmentation fails.

## 2.9 Open questions

- Phone camera horizontal FOV: read from `getCapabilities()` where available, otherwise a
  one-time calibration (photograph two objects of known angular separation), otherwise
  assume 65° and let the user nudge.
- Whether to store the panorama for a textured silhouette or keep the line only.
- Whether the profile should be latitude-tagged and refuse to load at a different site, or
  just warn. (Travel case: Panama.)
