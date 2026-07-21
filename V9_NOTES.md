# v9 — Sky, Twilight and Catalog

Adds a deep-sky catalog browser and a sun/moon/twilight screen to the
OnStep controller, in native LVGL, with full day and night-vision art sets.

## What's new

| File | Purpose |
|---|---|
| `Catalog.h/.cpp` | 16-byte packed records, sky-window filter, six sort orders |
| `Catalog_long.cpp` / `Catalog_wide.cpp` | generated tables selected in `Settings.h` |
| `Ephemeris.h/.cpp` | sun, moon, twilight regimes, rise/set search |
| `SkyContext.h` | shared state + refresh cadence, no LVGL dependency |
| `ScreenModule.h/.cpp` | screen framework and shared construction helpers |
| `ScreenSky.h/.cpp` | twilight ribbon, moon, meridian flip, date entry |
| `ScreenCatalog.h/.cpp` | filter / list / detail |
| `tools/build_catalog.py` | TSV → packed C arrays |
| `tools/gen_assets_v9.py` | type glyphs, moon phases, nav_sky (day + night) |

## Build

Choose the linked catalog with `CATALOG_PROFILE` in `Settings.h`. Both source
files can remain in the Arduino sketch folder; compile guards ensure only the
selected table defines catalog storage:

* `Catalog_long.cpp` — 4,271 objects, galaxies down to 1.5′, open clusters
  ≤60′ and mag ≤9. For 1000 mm and up. ~101 KB flash.
* `Catalog_wide.cpp` — 1,168 objects, galaxies down to 5′, clusters ≤180′
  and mag ≤11. For short refractors. ~29 KB flash.

Regenerate either from the TheSkyLive TSV:

```
python3 tools/build_catalog.py theskylive.tsv
```

Thresholds are constants at the top of that script. It also writes
`tools/survivors_*.csv` so you can audit the cut before flashing.

Regenerate art (both palettes) and recompile it into C:

```
python3 tools/gen_assets_v9.py
python3 tools/png2c.py
```

Total image data is now ~220 KB of flash.

## Navigation

START cycles: **HOME → SKY → CATALOG → SETTINGS → DIAG → HOME**.
START long-press still toggles night mode. SELECT long-press is still
`:Q#` emergency stop.

On SKY: `A` opens the UTC date editor, `A` again accepts, `B` cancels.
Stick left/right picks a field, up/down changes it.

On CATALOG the three states drill down and back:

```
FILTER --B--> LIST --A--> DETAIL
FILTER <--B-- LIST <--B-- DETAIL
```

* **FILTER** — stick moves focus around the eight compass wedges, the
  altitude ring, then the six type chips. `A` toggles. `Y` cycles the
  azimuth presets ALL → EAST → WEST → SOUTH.
* **LIST** — stick up/down scrolls, left/right pages. `A` opens detail,
  `Y` cycles sort order.
* **DETAIL** — stick left/right adjusts field of view live, so framing can
  be judged against the object on screen.

## The sky window

An object is shown when

```
(azimuth in a selected sector AND alt >= altFloor)  OR  (alt >= zenithExempt)
```

The second clause is the point. Near the zenith azimuth stops meaning
anything, so a target 88° up in the north is still visible when only the
western sectors are selected. Defaults are 25° floor and 70° exemption;
set the exemption above 90 to disable it. The filled cap at the centre of
the compass rose is the always-included region.

## Night mode

Hue carries type identity in day mode. In night mode there is only red, so
that information moves into **interior texture**:

| Type | Texture |
|---|---|
| Galaxy | smooth solid ellipse |
| Planetary | annulus, hollow centre |
| Globular | dense stipple |
| Nebula | diagonal hatch |
| Open cluster | discrete points |
| Remnant | torn open arc |

Two other night-specific substitutions, following the existing `nightRule`
convention that OUTLINE = inactive and FILLED = active:

* List selection is an accent bar plus a **filled** glyph, not a colour change.
* An imminent meridian flip switches the countdown to a **larger font**
  rather than turning red, since red is already everything.
* The twilight ribbon encodes its four regimes as four luminance steps of
  red rather than four hues.

## Adding a screen

1. Subclass `ScreenModule` (`build` / `update` / `onButton` / `onMove`).
2. Add an instance to `kModules[]` in `LvglDashboard.cpp`.
3. Bump `kModuleCount` in `LvglDashboard.h`.

`nextScreen()` picks it up automatically. The contract that keeps this
efficient: `build()` allocates LVGL objects once, `update()` only mutates
them. Screens are rebuilt exactly twice per session — at boot and on a
night-mode toggle.

## Verified vs unverified

**Verified numerically** on the desktop (`desktop_only/catalog_test.cpp`,
build with the command in its header comment):

* record pack/unpack round-trip against M31's catalogued values
* zenith geometry, transit altitude, equator rising due east, airmass,
  hour-angle wraparound
* JD and GMST against J2000.0 reference values
* zenith-exemption behaviour, and that no object leaks outside the window
* type filter exactness, altitude sort monotonicity
* moon sprite illumination fraction against the cosine law
* timing: full 4,271-object rebuild + sort 0.27 ms, `computeSky` 0.22 ms
  on desktop

**Not verified** — I had no LVGL or Arduino toolchain available:

* none of `ScreenSky.cpp`, `ScreenCatalog.cpp`, `ScreenModule.cpp` or the
  `LvglDashboard` integration has been compiled. Expect to fix compile
  errors on first build.
* every `ui_img_*` symbol referenced by the new code was checked to exist
  in `UiAssets.h` (82 referenced, 0 missing), but that is a symbol check,
  not a compile.
* LVGL 8.3 API usage — particularly `lv_arc` angle conventions and
  `lv_line` point lifetimes — is written from the documented behaviour and
  should be sanity-checked on hardware. `fovPts_` is a member array
  deliberately, because `lv_line_set_points` does not copy.
* on-screen layout has never been rendered; pixel positions are calculated,
  not eyeballed.

## GoTo command path

The detail action uses a two-press confirmation, sets target right ascension
with `:Sr`, target declination with `:Sd`, then starts the slew with `:MS`.
A zero `:MS` result is success; other result digits are displayed as a failed
GoTo and logged to Serial. Offline and parked mounts are rejected. Bench-test
this sequence on the exact classic OnStep firmware build before field use.
