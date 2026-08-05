# DSO Image System — format, storage and display

Three images per object, because they do three different jobs. Raw RGB565 throughout:
storage is free and decode time is not.

---

## 1. The three tiers

| Tier | Size | Bytes | Framing | Job |
|---|---|---|---|---|
| **Thumb** | 40 × 40 | 3,200 | 1.6 × object size | catalog row recognition |
| **Portrait** | 320 × 214 | 136,960 | 1.6 × object size | the hero image — identification and desire |
| **Field** | 320 × 320 | 204,800 | ≥ sensor diagonal × 1.15 | framing and planning |

**337 KB per object.** Messier 0.04 GB · bright NGC 0.86 GB · full NGC+IC **4.81 GB**.

The Portrait and the Field are *not* the same picture at different sizes, and this is the
central design point. The Portrait is cropped tight — it answers "what is this." The Field
is deliberately wider than your sensor — it answers "will it fit, and how should I rotate."

---

## 2. Why the Field is square and oversized

A rectangle of diagonal *D*, rotated about its centre, always fits inside a square of
side *D*. So the stored field only needs to be as wide as your **sensor diagonal**, and
the rectangle can then swing to any angle without ever running off the edge.

**Rotate the outline, never the bitmap.** Rotating four corner points is free; rotating a
320 × 320 bitmap is 100k pixels of resampling per frame. It is also the more honest
interaction — the sky stays put and the sensor turns, which is exactly what happens when
you rotate the camera in the focuser.

Field width to request, for your rig:

| Camera | C8 @ f/10 | C8 + 0.63× |
|---|---|---|
| ASI2600MC (23.5 × 15.7) | 55′ | 87′ |
| ASI533MC (11.3 × 11.3) | 31′ | 49′ |
| ASI183MC (13.2 × 8.8) | 31′ | 49′ |

At 320 px, a 55′ field is 10.3″/px — comfortably finer than the object detail that matters
at this display size.

---

## 3. On-card layout

Do **not** write one file per image. A FAT32 directory with 40,000 entries is slow to scan
and the scan happens on every open. Pack into fixed-size records instead:

```
/dso/thumb.pak       ordinal * 3,200      bytes
/dso/portrait.pak    ordinal * 136,960    bytes
/dso/field.pak       ordinal * 204,800    bytes
/dso/images.meta     ordinal * 10         bytes   (RAM-resident, 136 KB)
/dso/images.present  1 bit per object             (RAM-resident, 1.7 KB)
```

The byte offset of object *N* is `N × record_size`. **One seek, one read, no index
lookup, no directory traversal.** The presence bitmap answers "does this object have
imagery" without touching the card at all — which is what lets the catalog fall back to
the procedural type icon instantly rather than after a failed read.

### Meta record — 10 bytes

```c
struct DsoImageMeta {          // little-endian
  uint8_t  flags;              // bit0 thumb, bit1 portrait, bit2 field
  uint8_t  reserved;
  uint16_t field_arcmin_x10;   // angular width of the stored field square
  uint16_t maj_arcmin_x10;     // object major axis
  uint16_t min_arcmin_x10;     // object minor axis
  int16_t  pa_deg_x10;         // position angle of the object's major axis
};
```

`field_arcmin` is what makes the overlay scale correct, and it is per-object because the
Portrait framing varies with object size. `pa_deg` drives the suggested camera angle.

**Store pixels big-endian.** The ST77922 wants MSB first, so writing `>u2` in the packer
means `flushRect()` can DMA straight from the read buffer with no byte swap.

---

## 4. Read cost

| | Bytes | At ~6 MB/s SDIO |
|---|---|---|
| One thumb | 3.2 KB | 0.5 ms |
| A page of 13 thumbs | 42 KB | 7 ms |
| Portrait | 137 KB | 23 ms |
| Field | 205 KB | 34 ms |

Estimates — measure `SDMMC` 4-bit throughput on the board before trusting them.

**Scrolling is the only place this bites.** Loading a thumb per row during a momentum
throw would stutter. It falls out of the architecture already specified: the catalog is
rendered once into a tall PSRAM surface when the filter changes, so thumbs load in one
batch (38 rows = 122 KB, ~20 ms) and scrolling never touches the card again.

---

## 5. The Frame screen

Reached from the icon rail on object detail. Layout:

```
  0–28    status: object ID · FRAME
 28–62    sensor field in arcmin, stored image scale in ″/px
 62–382   field image 320×320, sensor rectangle overlaid
382–430   camera angle · object fills · verdict
430–454   (spare)
454–480   ◀ ▶ ROTATE   A SUGGEST   X FLIP
```

**Overlay elements**

- Sensor rectangle at true scale, accent when it fits, amber when the object overflows.
- A 10 px tick on one edge marking sensor "up".
- The object's extent as a dashed ellipse, from `maj`/`min`/`pa`.
- A scale bar sized to the field (5′, 10′ or 30′).

**Verdict logic**

| Condition | Result |
|---|---|
| major ≤ 0.85 × long side and minor ≤ 0.85 × short side | FITS |
| within 1.0 × on both | TIGHT — little room to guide out |
| otherwise | MOSAIC *n*×*m*, panels at 15% overlap |

**Suggest angle** sets the camera angle so the sensor's long axis aligns with the object's
major axis — `pa` if the sensor is landscape, `pa + 90` if portrait.

**Meridian flip** adds 180°. Worth knowing: this does *not* change whether the object
fits, because a rectangle rotated 180° occupies the same area. What it changes is which
edge is up in your subs — which matters for mosaic panel ordering and for matching a
reference frame across the flip. That is why the tick exists.

---

## 6. Acquisition

`fetch_dso.ps1` in the pack drives `curl.exe` against CDS hips2fits, which serves colour
imagery at an arbitrary field and pixel size, TAN projected and north-up:

```
https://alasky.u-strasbg.fr/hips-image-services/hips2fits
  ?hips=CDS/P/DSS2/color&width=320&height=320&fov=0.917
  &projection=TAN&coordsys=icrs&ra=250.42&dec=36.46&format=png
```

`fov` is in **degrees**. North-up matters: the overlay assumes it, and the object PA in
the meta record is measured from north. If you swap to a survey that isn't north-up, the
overlay needs a per-object rotation offset added to the meta record.

PanSTARRS DR1 colour is sharper than DSS2 for northern objects; DSS2 covers the whole sky.
The script skips files that already exist, so it resumes cleanly across a 14,000-object run.

---

## 7. Open items

- Sensor dimensions and focal length belong in Settings, since `field_arcmin` must be
  chosen at download time to match. Changing cameras means re-fetching the Field tier
  (the Portrait and Thumb tiers are unaffected).
- Your own C8 frames should override the survey image where you have them. Same record
  format — the packer takes whatever PNG it finds, so a personal frame simply replaces
  the downloaded one in `src_dir`.
- `drawTextBlended()` is still required for the labels over the field image.
