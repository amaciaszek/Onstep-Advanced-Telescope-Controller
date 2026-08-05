# OnStep 320×480 — UI, Motion and Power Specification

Target: Elecrow 3.5″ ESP32-S3, ST77922 QSPI, 320×480 RGB565, 0.153 mm pitch.

The governing decision in this document: **motion is not the thing to economise on.**
The power model says rendering is 19% of the budget and animation is a sliver of that,
while the radio is 37% and the backlight 21%. Every animation specified below costs
less than the auto-dim alone returns. Spend on beauty, claw it back from the radio.

---

## 1. Layout system

**Grid.** 12 px side margin, 8 px gutter. All vertical measurements from the top.

| Band | Height | Notes |
|---|---|---|
| Status bar | 28 | title left, glyphs right, 1 px rule beneath |
| Content | 426 | screen-specific |
| Footer prompts | 26 | physical-button hints, 1 px rule above |

**Type ladder** (from the asset pack, tabular figures baked in):

| Role | px | Weight | Optical size | Used for |
|---|---|---|---|---|
| Hero | 34 | 300 | 32 | RA/Dec readout |
| Head | 20 | 400 | 18 | object names, screen titles |
| Value | 15 | 500 | 14 | alt/az, magnitudes, inline values |
| Body | 12 | 400 | 14 | list rows, data tables |
| Strong | 12 | 600 | 14 | selected rows, emphasis |
| Label | 9 | 500 | 14 | tracked uppercase labels |

**Row pitch.** Catalog rows are 32 px — 13 visible against roughly 4 on the T-Display.
Settings rows are 58 px because each carries a second explanatory line. Icon rail
items are 68 px.

**Touch targets.** A finger wants ≥ 44 px (6.7 mm). Catalog rows at 32 px are fine for
the stick and marginal for a fingertip, which is deliberate: touch is a coarse
accelerator, not the primary input. The footer band is 26 px but its *hit* area should
extend to 44 px upward.

### Screen geometry

**Home** — 0 status, 28 tracking line, 64–272 dome (r = 104 at 160,168), 282 rule,
286–366 hero RA/Dec, 368 rule, 372–412 alt/az/pier triple, 414 rule, 418–470 four
action tiles at 70×44.

**Catalog** — 28 status, 34–54 filter summary, 58–454 list (13 rows at 32), 454 rule,
footer. The list is a **separate surface** taller than the viewport; see §2.

**Object detail** — 48 px icon rail full height, 50–183 image (270×152), 192 rule,
200–416 nine data rows at 24, 426 accent rule + armed-GoTo band.

**Sky** — 28 status, 40–100 regime block, 106 rule, 124–220 Moon disc at 96 px,
232 rule, 242–380 next-events list.

**Settings** — seven rows at 58, each a title, a value right-aligned, and a caption.

---

## 2. The one architectural rule for motion

**Animate by compositing pre-rendered surfaces. Never by re-running the draw code.**

`FramebufferDisplay` is immediate mode: a frame means re-executing every `drawText`,
`fillRect` and icon blit. That is fine at one frame per input, and hopeless at 30 fps.

Instead, allocate a second offscreen surface, render the outgoing and incoming screens
into it once, then move bytes:

- **Screen transition** — both screens exist as framebuffers; each frame is a
  column-offset copy. No drawing code runs during the slide.
- **List scrolling** — render the whole list once into a tall surface
  (320 × 1216 for 38 rows = 760 KB in PSRAM), then blit a moving 396 px window.
  Scrolling never re-renders a row.
- **Live elements** — the tracking pulse and reticle are tens of pixels. Redraw those
  regions only.

### Dirty tracking must move to write time

`present()` currently compares the whole backbuffer against a front buffer to find
changes. At 320×480 that is **600 KB of PSRAM read per present**, roughly 10–15 ms
before a single pixel ships — it was 218 KB and 4–5 ms on the T-Display.

Mark tiles dirty in `drawPixel`/`fillRect` instead. `present()` then walks only marked
tiles, the front buffer disappears entirely, and 300 KB of PSRAM comes back. This is
internal to `FramebufferDisplay` and changes no call sites. **It is the prerequisite
for everything in §3.**

---

## 3. Animation catalogue

| Animation | Duration | Curve | Technique | Region |
|---|---|---|---|---|
| Screen change | 180 ms | ease-out cubic | offset blit of two surfaces | full |
| List momentum | ~700 ms | friction 0.94/frame | window blit on tall surface | 320×396 |
| Selection move | 120 ms | ease-out quad | redraw two rows | 320×64 |
| Backlight wake | 180 ms | linear | LEDC duty | — |
| Backlight dim | 1200 ms | ease-in-out | LEDC duty | — |
| Tracking pulse | 3000 ms loop | sine, 55–100% α | one dot | 6×6 |
| Reticle drift | continuous | sidereal rate | sub-pixel reticle | 24×24 |
| Slew gauge | continuous | value-driven | arc segment | 176×176 |
| GoTo arm | 240 ms | ease-out | band rises 8 px | 320×54 |
| Value settle | 200 ms | ease-out | brightness only | text run |
| Night crossfade | 400 ms | linear palette lerp | LUT interpolation | full |

**Curves.** Ease-out cubic is `1 − (1−t)³`. Use ease-out for anything the user
initiated — it starts fast, so the interface feels like it responded instantly and
then settled. Reserve ease-in-out for things the device initiated on its own, like the
dim ramp, where the slow start reads as considerate rather than sluggish.

**Do not animate the RA/Dec readout.** Rolling or fading digits are harder to read, not
easier, and the whole point of baking tabular figures was to stop the numbers moving.
Values snap; only things with real physical continuity are allowed to move.

**Motion levels.** Full / Reduced / Off in Settings. **Night mode should force Reduced**
— peripheral movement is far more distracting to a dark-adapted eye than to an indoor
one. Reduced keeps transitions at 110 ms and drops the pulse and drift entirely.

---

## 4. Power

Measured inputs are the datasheet's 1.45 W total and 120 mA backlight. The component
split below is **estimated** and should be checked with a meter.

| | Power | Share |
|---|---|---|
| Backlight @ 25% | 150 mW | 21.4% |
| Wi-Fi radio | 260 mW | 37.1% |
| CPU @ 240 MHz | 130 mW | 18.6% |
| Panel + PSRAM | 100 mW | 14.3% |
| Regulators | 60 mW | 8.6% |
| **Total** | **700 mW** | ≈ 217 mA at the cell |

### What animation costs

A 180 ms transition at +300 mW is **0.054 J**. Two hundred of them in a night is
10.8 J against a 15 120 J session — **0.07%**.

### What the levers return

| Lever | Saving |
|---|---|
| Auto-dim 25% → 8% after 30 s idle | 10.2% |
| Poll 4 Hz → 1 Hz when nothing is moving | 14.3% |
| CPU 240 → 160 MHz outside redraws | 5.7% |
| Wi-Fi modem sleep between polls | 6.4% |
| **Combined** | **36.6%** |

Runtime on a 2000 mAh cell goes from about 6 h to **9.5 h** — while gaining every
animation in §3.

**Auto-dim is also the best-looking feature here.** A 1200 ms ease-in-out ramp to 8%
after 30 s of stillness, snapping back in 180 ms on any input, reads as the instrument
resting rather than a battery-saving compromise. On a handheld you are looking at the
eyepiece most of the night, not the screen.

### Redraw cadence

The sky dome does not need per-frame redraw. Sidereal rotation moves the rim of a
208 px dome one pixel in about 3.5 minutes. Redraw on pixel-motion, target change or
input — not on a timer.

---

## 5. Open items

- `drawTextBlended()` — AA text currently needs a known solid background, which blocks
  constellation labels over the dome and any text over object imagery. This is why the
  Home render has no labels on the sky.
- Object imagery from your own C8 frames, as raw RGB565 at 270×152.
- Component power split needs a meter; only the totals are from the datasheet.
- Deep-sleep current with the SD card and touch controller present is unmeasured and
  bounds everything above.
