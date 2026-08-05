# Elecrow 3.5″ Port — Notes from the Official Resource Pack

Everything here is transcribed or derived from the vendor pack
(`3.5inch_ESP32-S3-Resource Pack`), not inferred. Where the vendor's own
documents contradict each other, that is called out explicitly.

---

## Corrections to earlier assumptions

Four things I had guessed wrong before the docs arrived. All are now fixed in
the code, but they are the ones that would have burned an evening:

| | I assumed | Actually |
|---|---|---|
| QSPI clock | 40 MHz | **80 MHz** — the pins are IOMUX-capable, so it holds |
| SPI mode | 3 | **0** |
| Touch controller | FT6236 @ 0x38 | **0x55, 16-bit register addresses, up to 10 points** |
| Driver stack | `esp_lcd_st77922` component | vendor's own bit-banged QSPI framing |

The touch one matters most: anything written against an FT6x36 register map will
simply not work. There is no FT6236 on this board despite what the marketing
datasheet implies.

---

## The QSPI framing

QSPI is not SPI with more wires. There is no DC line — the command travels
inside the transaction's cmd/addr fields:

**Register write (1-wire):** cmd `0x02`, addr = `reg << 8`, 8 command bits,
24 address bits.

**Pixel burst (4-wire QIO):** cmd `0x32`, addr = `0x3C << 8`, then subsequent
chunks are bare data continuations with `SPI_TRANS_VARIABLE_DUMMY` and zero
command/address bits. Max **0x4000 pixels** per transaction.

**CS is toggled manually** via `GPIO.out_w1tc` / `out_w1ts` even though
`spics_io_num` is set, because CS must stay low across a chunked burst. This
looks redundant and is not.

---

## The vendor's architecture is already yours

Their demos use **TFT_eSPI purely as a software renderer into a PSRAM sprite**,
then push the whole sprite with `Fill_Colors(0, 0, w, h, sprite.getPointer())`.

`FramebufferDisplay` already *is* that sprite. So TFT_eSPI drops out of the build
entirely — you keep your own renderer and your own dirty-tile logic, and
The application uses the vendor `ST77922` display driver directly as shown in
Elecrow's LVGL demo; there is no `Elecrow35Display` wrapper to install.

**Do not use rotation 1 or 3.** The vendor's `Fill_Colors` handles those with a
`ps_malloc` and a per-pixel transpose *on every call*. Rotation 0 is native
portrait 320×480 with MADCTL = 0, which is what you want anyway.

---

## Pin conflict — unresolved in the vendor docs

| Source | IO47 | IO48 |
|---|---|---|
| Official IO allocation table | touch **RESET** | touch **INTERRUPT** |
| Working vendor driver | touch **INT** | touch **RESET** |

The driver wins (it demonstrably runs). Touch uses the vendor
`ST77922_TOUCH` library. If touch fails to initialise, verify the vendor
library version before changing its pin definitions.

Neither pin is in GPIO0–21, so **neither is RTC-capable — touch cannot wake the
device from deep sleep.** That has to remain a physical button.

---

## Confirmed pin map

```
LCD    CS 10 · SCK 12 · D0 11 · D1 13 · D2 14 · D3 9 · BL 41 (PWM)
       reset bonded to EN — a chip reset is a panel reset, no GPIO
Touch  I2C 38/39 @ 0x55 · RST 48 · INT 47   (see conflict above)
SD     CLK 5 · CMD 4 · D0-3 = 6/7/2/3
Audio  EN 1 · DIN 15 · DOUT 16 · MCLK 17 · BCLK 18 · LRCK 21
RGB    IO40, WS2812B-V5-W, single wire
Batt   IO8 = ADC1_CH7, on-board 1:2 divider
Free   IO45, IO46 (expansion socket) · IO43/44 (UART, if unused)
```

Two notes the table adds:

- **IO42 is listed as "LCD command/data select"** but QSPI has no DC line, so it
  is unused. Treat it as free with caution — it may be physically routed.
- **IO8 is on ADC1**, which is the lucky outcome: ADC2 is unusable while Wi-Fi is
  active.

**IO14 is QSPI D2.** The old enclosure button 2 has no home. Dropping the speaker
frees IO1/15/16/17/18/21 — all RTC-capable, so any of them can be an `ext0`
deep-sleep wake source. IO21 is the natural pick.

---

## Arduino IDE settings

These are not optional:

| Setting | Value |
|---|---|
| Board | ESP32S3 Dev Module |
| **USB CDC On Boot** | **Enabled** — else `Serial` prints nothing over USB-C |
| Flash Size | 16MB (128Mb) |
| **PSRAM** | **OPI PSRAM** — the two 300 KB framebuffers need it |
| Partition Scheme | 16M Flash (3MB APP / 9.9MB FATFS) |

Arduino-ESP32 **3.x** required: the code uses `ledcAttachChannel()` and the
`esp_adc/adc_oneshot` API.

---

## Bring-up order

Run `Elecrow35_BringUp.ino` before touching the application. It answers the four
questions that are painful to debug blind:

1. **Colour order.** Three bars must read RED / GREEN / BLUE top to bottom. If
   they read blue/green/red, flip `ELECROW35_SWAP_BYTES` in the header. One
   define, one place.
2. **Origin and extent.** A white square must sit hard in the top-left and a 1 px
   frame must trace all four edges with no wrap and no missing row.
3. **Throughput.** Prints measured ms per full-screen present. Compare against
   the ~31 ms/frame ceiling the design assumes.
4. **I2C.** Scans and names everything on 38/39, so you know what is present
   before adding the DS3231 / BME280 / DRV2605L.

---

## What still needs doing in the app

Unchanged from the earlier plan, and now unblocked:

1. **Write-time dirty marking.** `present()` currently diffs the whole backbuffer
   against a front buffer — 600 KB of PSRAM read per present at this size,
   ~10–15 ms before a pixel ships. Mark tiles dirty in `drawPixel`/`fillRect`
   instead; the front buffer disappears and 300 KB comes back. Internal to
   `FramebufferDisplay`, no call sites change. **Prerequisite for animation.**
2. **`drawTextBlended()`.** AA text needs a known solid background today, which
   blocks all text over imagery. ~10 lines: read the framebuffer pixel, unpack
   565, blend against the glyph's 4-bit coverage, repack.
3. **Poller on core 0.** `xTaskCreatePinnedToCore(pollerTask, ..., 0)` so
   blocking network calls stop stalling the renderer, and drop the
   input-before-query ordering workaround.
4. **Persistent socket** in `OnStepClient`, replacing connect-per-command.

Tune `ELECROW35_TILE_W/H` once (1) is done — 32×24 is a starting guess, not a
measurement.
