# Making the Card Earn Its Keep

The Milky Way worked because it was real data doing something the class of device
shouldn't be able to do. Everything below is judged against that test. Ordered by
impact, not by size — the two best items on the list are the smallest.

**Total for all ten: 5.5 GB. On a 64 GB card that is 9% used.**

---

## 1 · Deep zoom — the sky as a mip pyramid · 340 MB

Built. `build_pyramid.py` produces levels 512 → 8192 from the 16k source you already
have, with the top level tiled at 256×256.

| Level | arcmin/texel | Size | Residency |
|---|---|---|---|
| 512 | 42.19′ | 0.13 MB | whole |
| 1024 | 21.09′ | 0.52 MB | whole |
| 2048 | 10.55′ | 2.10 MB | whole |
| 4096 | 5.27′ | 8.39 MB | whole |
| 8192 | 2.64′ | 33.55 MB | 512 tiles |

The point isn't the pyramid, it's that **only visible tiles are ever resident**. A 320 px
view needs about nine 256×256 tiles — 0.6 MB — no matter how deep it goes. You could add
16384 (1.32′/texel, 128 MB) and the runtime memory cost would not change by a byte.

What it buys: the sky view stops being a status widget and becomes a planetarium. Zoom
from the whole hemisphere down to a 7° field with real dust structure, on a handset.

---

## 2 · Your horizon · 3 MB

The cheapest premium feature on this list and the one nobody else has.

Photograph a 360° panorama from your regular site. Derive an altitude-per-azimuth profile
(360 uint16, **720 bytes**) and mask the dome with it. Now the sky map shows *your* treeline
and *your* neighbour's roof, and "clears the oak at 22:40" is a real answer rather than a
guess. Store the panorama too and you can draw the actual silhouette instead of a line.

Multiple sites stored by name. This is the feature that makes the device feel like it
belongs to you rather than to a product category.

---

## 3 · Precomputed visibility tables · 30 MB per latitude

For every catalog object × every day of the year: transit time, maximum altitude, hours
above your altitude floor. 14,000 × 365 × 6 bytes.

"Best tonight" then becomes a **table read and a sort**, not an ephemeris run over 14,000
objects. The list appears instantly, filterable by type and by how long the object stays
up. Regenerate on the desktop when you change site — the same trip that regenerates the
sky LUT.

Instant is the whole feature. A list that takes two seconds to compute feels like a
computer; a list that is simply *there* feels like an instrument.

---

## 4 · Classical constellation art · 23 MB

The engraved figures from Bayer's *Uranometria* or Hevelius, as alpha overlays keyed to
the same coordinates as the stick figures. Drawn at 10–15% opacity under the vector lines.

Antique engraving over a real photographic sky is a striking combination, and it costs
nothing at runtime — it is one more alpha layer through the LUT you already have.

---

## 5 · Lunar feature database · 8 MB

4k albedo (32 MB if you want it, 8 at 2k) plus ~2,000 named features with selenographic
coordinates — 48 KB of text.

Then: tap the Moon and it labels craters **along the terminator**, which is exactly where
lunar detail lives and exactly where an observer is looking. The renderer already computes
the terminator, so the selection criterion is free. No handheld controller does this.

---

## 6 · Flight recorder · 3.7 MB per night

Log every poll — 4 Hz × 8 h × 32 bytes. A full season is 450 MB.

Replay any past night: where the mount actually went, pointing error over time, when the
flip happened, what the temperature did. This is the feature that reads as *professional*
rather than futuristic, and it costs one file append per poll.

---

## 7 · Object descriptions · 3 MB

14,000 objects × ~200 characters. What you are actually looking at, on the detail screen,
under the image. Distance, what kind of thing it is, why it is interesting.

Sounds like filler; isn't. It is the difference between a coordinate database and a guide.

---

## 8 · Pre-rendered boot sequence · 9 MB

Thirty frames of 320×480 RGB565. A one-second reveal — the sky dome resolving out of black
as the instrument comes up.

This is pure polish with no functional value whatsoever, which is exactly why it belongs
on a list about feeling premium. It is also **the single best argument for having a large
card**: nobody pre-renders video for a boot animation on an embedded device, because
nobody has the room. You do.

---

## 9 · Moon ephemeris table · 2 MB

Hourly positions for 20 years, interpolated at runtime. The Moon is the hardest body to
compute accurately and the one you care most about. Ship the answer instead of the
algorithm, and delete the code.

---

## 10 · Font atlases at every size · ~4 MB

With room to spare there is no reason to compromise the ladder. Ship every size and weight
rasterised at 4bpp, including sizes you only use in one place.

---

## What this adds up to

| | |
|---|---|
| DSO imagery, full NGC+IC | 4810 MB |
| Everything above | 868 MB |
| **Total** | **5.5 GB** |
| Free on a 64 GB card | 58 GB |

---

## The principle

Every item on this list follows the same shape: **move work off the device and ship the
answer.** The pyramid ships resolution instead of computing it. The visibility tables ship
sorted results instead of running ephemeris. The Moon table ships positions instead of
orbital mechanics. The boot sequence ships frames instead of rendering them.

That is what a large card is actually for on a 240 MHz microcontroller — not storing more
of the same, but changing which side of the desktop/device line the expensive work happens
on. The Milky Way was the first instance of it. None of the rest is harder.
