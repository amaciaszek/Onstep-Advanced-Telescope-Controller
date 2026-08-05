# Sky Rendering Brief — OnStep Advanced Telescope Controller

**Target hardware:** Elecrow 3.5" ESP32-S3 board. ESP32-S3 N16R8 (240 MHz dual Xtensa LX7,
512 KB internal SRAM, 8 MB octal PSRAM, 16 MB flash). ST77922 panel, 320×480, QSPI, RGB565.
MicroSD on 4-bit SDIO (IO4/5, IO2/3/6/7). Battery powered, ~250–320 mA at 25% brightness.

**Goal:** render a photographic Milky Way, a real star field, and constellation figures inside the
alt-az "radar" dome — at 208 px diameter on Home and ~300 px full-screen — without meaningful CPU
time or battery cost.

**Constraint the brief must respect:** storage is effectively free (multi-GB SD card), compute and
energy are not. Prefer precomputation and raw formats over runtime decode. Uncompressed is
*preferred* where it removes a decode step.

---

## 1. The geometric insight that makes this cheap

The horizon coordinates of a point depend only on hour angle, declination and latitude:

```
sin(alt) = sin(δ)sin(φ) + cos(δ)cos(φ)cos(H)
```

Nothing in that expression is a function of time. Local sidereal time enters only through
`H = LST − RA`.

Therefore, **for a fixed latitude and a fixed dome radius, the mapping from screen pixel to
(hour angle, declination) is constant.** It does not change as the night progresses. It changes only
when the observer moves.

If the sky texture is stored as an equirectangular map indexed by (RA, Dec), then advancing time is
a *single scalar offset applied to the RA axis* — the same offset for every pixel. Per-pixel runtime
trigonometry is entirely unnecessary.

This is the difference between ~40 ms and ~3 ms per redraw, and it is the thing most implementations
miss.

---

## 2. Options

### Option A — Static screen→(HA, Dec) LUT + equirectangular texture ★ recommended

Precompute, once per (latitude, dome radius), two arrays covering the pixels inside the dome:

```c
uint16_t u0[N];   // RA-axis texel index at LST = 0, scaled to texture width
uint16_t v [N];   // Dec-axis texel row, constant per pixel
```

Per redraw:

```c
uint16_t off = (uint16_t)(lst_fraction * TEX_W);
for (i = 0; i < N; i++) {
    uint32_t u = (u0[i] + off) & (TEX_W - 1);
    uint8_t  s = tex[(v[i] << TEX_W_SHIFT) | u];
    fb[px[i]] = palette[s];
}
```

Six operations per pixel, no trig, no branches, sequential LUT access.

- **Texture:** 512×256 or 1024×512, 8-bit palette-indexed. 128 KB / 512 KB. Keep in internal SRAM if
  it fits — this is the only semi-random access in the loop and SRAM residency matters more than
  resolution. The Milky Way is low spatial frequency; 512×256 (0.7°/texel) is visually sufficient
  because sharp detail comes from the vector star layer, not the texture.
- **LUT size:** 34,000 px (208 dome) × 4 B = 136 KB. 70,700 px (300 dome) × 4 B = 283 KB. PSRAM.
- **Latitude change:** regenerate on device. ~34k inverse projections ≈ 40 ms *(estimate)*. Do it at
  boot and on site change. No need to ship LUTs on the card.
- **Night mode:** swap a 256-entry palette. Zero cost, zero extra storage.

**Pros** — Fastest realistic option. Photographic quality (the texture is a real processed
panorama). Exact at any LST, no temporal quantisation. Works at any latitude including travel.
Trivially supports supersampling. Small storage. Same code path for both dome sizes.

**Cons** — LUT per (latitude, radius) pair costs PSRAM. Continuous zoom/pan would need LUT
regeneration per zoom level (~40 ms), so free-look panning is not smooth without Option E.

---

### Option B — Precomputed frame cache on SD

Render the entire dome background offline in Python for every LST step, store as raw frames, blit
the nearest one.

- 1440 frames (1-minute LST steps) × 208×208 × 2 B RGB565 = **124 MB per latitude**
- Same at 8-bit indexed = 62 MB per latitude
- Full-screen 300×300 set = 259 MB per latitude RGB565

**Pros** — Effectively zero CPU. Unlimited offline rendering quality: real photograph, proper
anti-aliasing, dithered gradients, hand-tuned constellation line weights. Lowest possible energy for
the compute half.

**Cons** — Trades CPU for SD I/O and its power draw; a 86 KB frame read is ~10–40 ms *(estimate,
depends on SDIO width and card)*. Per-latitude generation must be run ahead of any trip. No zoom,
no pan. Quantised to the frame step — at 1-minute steps the rim moves ~0.5 px per step, invisible;
at 4-minute steps it becomes a perceptible tick.

---

### Option C — Cached background + live vector overlay

Option B for the static sky, with trajectory arc, reticle, target marker and labels drawn live on
top each frame.

**Pros** — Offline beauty for the expensive layer, full interactivity for the cheap layer. The
dynamic elements are exactly the ones that must stay live.

**Cons** — Inherits B's storage and trip-planning burden. The overlay must be composited into a
scratch buffer or the background re-blitted each time the reticle moves.

---

### Option D — Vector-only Milky Way (isophote polygons)

Store the band as 2–4 nested filled contours in RA/Dec, project the vertices, scanline-fill.

**Pros** — A few KB. Resolution independent, sharp at any zoom. No LUT, no texture, no SD.

**Cons** — Stylised, not photographic; loses the mottling and dust structure that carry the beauty.
Polygon fill with correct nesting is more code than it appears. Best kept as a low-memory fallback
or a deliberate "chart mode".

---

### Option E — Per-pixel live trigonometry

Full transform per pixel each redraw.

**Pros** — No LUT memory. Arbitrary projection, free zoom and pan, any latitude instantly.

**Cons** — ~30–60 ms per dome *(estimate)*, roughly 10–20× Option A for identical output at fixed
latitude. Only justified if free-look panning is a requirement.

---

### Option F — Pole-centred planisphere

Store one pole-centred chart; time advance becomes image rotation about the centre; draw a computed
horizon mask.

**Pros** — Conceptually simple, one image for all times.

**Cons** — A rotate-blit costs about the same per pixel as Option A but with far worse cache
locality, and the zenith is no longer at the centre, which changes the widget's meaning. No
advantage over A. Listed for completeness; not recommended.

---

## 3. Layer split (applies to all options)

Do not try to make one mechanism do everything. Sharpness and cost differ per layer.

| Layer | Mechanism | Data | Runtime cost |
|---|---|---|---|
| Milky Way / nebulosity | Raster texture (A) or cached frame (B) | 128–512 KB | ~3 ms |
| Stars | Vector points from catalog | 45 KB | <1 ms |
| Constellation figures | Vector line segments | ~10 KB | negligible |
| Constellation labels | Text, culled to alt > 20° | — | negligible |
| Trajectory arc, reticle, markers | Vector, live | — | negligible |

**Stars must be vector, not baked into the texture.** Point-drawn stars stay sharp at any dome size
and are what makes the render read as crisp rather than blurry. Yale Bright Star Catalog is 9,110
entries; packed as `uint16 RA, int16 Dec, uint8 mag` that is 45 KB. A magnitude limit of ~5.5 leaves
roughly 800 above the horizon at any moment — under 1 ms to project even with full trigonometry.

**Constellation figures must be vector.** Standard stick-figure line sets (Stellarium's
`constellationship.fab`, or the HYG-derived line lists) are ~700 segments referencing bright stars.
Project the endpoints already computed for the star layer and Bresenham between them.

---

## 4. Recommendation

**Implement Option A with the layer split in §3.** Keep Option C in reserve if measurement shows the
per-pixel loop underperforming; the layer architecture makes them interchangeable.

Staged plan:

1. Build the Python asset pipeline: panorama → equatorial equirectangular → grayscale/palette →
   tone curve → raw binary. Verify against known reference points (the galactic centre must land at
   RA 266.405°, Dec −28.94°).
2. Implement on-device LUT generation and the inner loop. Measure real redraw time before optimising.
3. Add the star layer, then constellation lines, then labels.
4. Add 2× supersampling if headroom allows (see §6).
5. Only then consider a frame cache.

---

## 5. Redraw cadence and energy

The dome does not need per-frame redraw. Sidereal rotation is 15″/s; across a 208 px hemisphere
dome one pixel is ~52′, so the sky takes roughly **3.5 minutes to move one pixel** at the rim
(~2.4 minutes on the 300 px view).

Redraw on whichever comes first: LST advancing one pixel, target change, or user input. At 3 ms
every 2 minutes the duty cycle is ~0.0025% — the sky renderer's energy cost is not measurable
against a 120 mA backlight. **This is the single most important efficiency decision, and it is
independent of which option is chosen.** Getting the cadence right matters far more than getting
the inner loop fast.

---

## 6. Free quality wins

- **Supersampling.** Generate the LUT at 2× and box-downsample. LUT grows to 544 KB, cost to ~12 ms,
  still trivially within budget, and it removes all aliasing on the band edge and horizon circle.
- **Palette-indexed colour.** Quantise the panorama offline to 256 colours; ship two palettes
  (natural and red). Night mode becomes a pointer swap, and the photograph rotates to red along with
  the UI.
- **Dither at quantisation time.** Floyd–Steinberg in the Python pipeline costs nothing at runtime
  and removes banding in the faint band gradients.
- **Precompute the horizon mask** into the LUT as invalid entries rather than testing per pixel.

---

## 7. SD card budget

Store everything raw. The point of a large card is to delete decode steps.

| Asset | Format | Size |
|---|---|---|
| Object images, 3,000 objects | raw RGB565, 270×152 | 246 MB |
| Object thumbnails, 3,000 | raw RGB565, 24×24 | 3.5 MB |
| Sky texture, 1024×512 | 8-bit indexed + palettes | 512 KB |
| Star catalog (BSC) | packed binary | 45 KB |
| Constellation lines + labels | binary | ~15 KB |
| NGC/IC catalog, ~14,000 objects | fixed-record binary | ~1 MB |
| *Optional* frame cache, 1 latitude | RGB565, 1440 frames | 124 MB |
| *Optional* full-screen frame cache | RGB565, 1440 frames | 259 MB |
| **Total, everything including both caches** | | **~635 MB** |

A 32 GB card leaves ~50× headroom. Object image count can rise to the full NGC/IC set (14,000 ×
82 KB ≈ 1.15 GB) and still be comfortable.

**Store object images as raw RGB565 pre-scaled to display size.** This eliminates JPEG decode
(~30–60 ms) in favour of a straight DMA blit (~2 ms) and removes the decoder from the build
entirely. This is the specific case where "not super compressed" is exactly right.

---

## 8. Data sources

Licensing is not a constraint for this project (private use), so choose on quality.

- **Milky Way panorama:** NASA SVS *Deep Star Maps 2020* (equirectangular, celestial coordinates,
  up to 16k); Stellarium's own Milky Way texture (already tone-mapped for this exact purpose);
  ESO/Serge Brunier *GigaGalaxy Zoom* panorama (highest quality, needs the most hand-tuning).
- **Stars:** Yale Bright Star Catalog, or HYG database filtered by magnitude.
- **Constellation figures:** Stellarium `constellationship.fab`, or the IAU boundary/line datasets.

---

## 9. Verification and open questions

Formulae verified during design: the galactic→equatorial transform reproduces the galactic centre
(l=0, b=0) at RA 266.39°, Dec −28.96° against the accepted 266.405°, −28.94°.

Everything below is **estimate, not measurement** — the implementing agent should measure before
optimising:

- Inner loop time with the texture in SRAM versus PSRAM. This is the main uncertainty; PSRAM random
  access latency could plausibly triple the estimate.
- On-device LUT generation time at boot.
- SDIO read throughput on this board at 4-bit, which determines whether Option B/C is viable.
- Whether 512×256 texture resolution is visually sufficient at 300 px dome diameter, or whether
  1024×512 is needed.
- Deep sleep current with the SD card and touch controller present — unrelated to rendering, but it
  bounds the whole power budget.
