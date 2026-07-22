# platform_esp32 — LilyGO T-Display-S3 firmware

This is the hardware backend. It plugs your **proven OnStep networking** and the
**tested `onstep::` protocol decoder** into the *same* `shared/` App/UI that the
desktop simulator runs. Nothing in `shared/` changes between sim and hardware.

## What maps to what

| Shared interface | ESP32 backend | Source |
|---|---|---|
| `Display` | `TDisplayS3Display` | TFT_eSPI + RGB565 dirty-tile present() |
| `Input` | `InputSeesaw` | Adafruit I2C gamepad |
| `StatusSource` | `OnStepStatusSource` | polls `:GU# :GR# :GD# :GA# :GZ# :D#`, decodes |
| `CommandSink` | `OnStepCommandSink` | `:Me# :Qe# :Q# :R# :hP# :hR# :hC#` |
| `GuideController` | `NullGuideController` | no PHD2 in the direct setup (yet) |
| `DeviceInfo` | `HandheldDeviceInfo` | `WiFi.RSSI()` bars + battery ADC |

`OnStepClient` is a near-verbatim wrap of the diagnostic sketch's
`sendCommand()` — connect-per-command, three reply kinds. Behavior on the wire
is unchanged from what already works.

## The two design decisions baked in

**Status decode fidelity.** All the observed rules live in `shared/OnStepProtocol.cpp`
and are unit-tested (`tests/smoke.cpp`) against your real strings:
- park is `P` only — `H` (at home) during `I` reads as *parking*, not parked;
- motion combines `:D#` (0x7F) with `GU` `g`/`I`/`h`/`G`; `:D#` alone is never
  treated as motion, and a manual jog is detected from `g`;
- RA/DEC/ALT/AZ parsed tolerantly (`:` or `*` separators, optional sign).

**Responsiveness.** Networking blocks, so `OnStepStatusSource::update()` issues
**one query per tick** and cycles the set; `App::update()` handles input *before*
that query, so button/stick latency is ~one query, not a full refresh. During an
active jog the poller drops to `:GU#`+`:D#` only (`setSlewing(true)`), called from
`loop()`.

## Build (Arduino IDE or PlatformIO, ESP32-S3)

Libraries: `TFT_eSPI`, `Adafruit_seesaw`. Enable **PSRAM** in the board config.

## Controller and network setup

- The gamepad is probed at address `0x50` on SDA/SCL `44/43`, then `8/9`.
  Define `ONSTEP_I2C_SDA` and `ONSTEP_I2C_SCL` before including
  `InputSeesaw.h` if your wiring uses another pair. The selected pins are
  printed at boot.
- Enclosure button 1 (BOOT/GPIO0) acts as A/change. A short press of button 2
  (GPIO14) advances screens; hold it for at least 600 ms to shut the display,
  Wi-Fi, and ESP32 down into deep sleep. Press GPIO14 again or press RESET to
  wake/restart. Deep sleep is very low power, but only a physical battery
  switch can provide a true zero-current disconnect.
- START cycles HOME, SKY, CATALOG, SETTINGS, and DIAGNOSTICS even when Wi-Fi
  or OnStep is offline. On SETTINGS, move the stick up/down and press A to
  change a value.
- Catalog detail uses a two-press GoTo confirmation, then sends classic
  OnStep `:Sr`, `:Sd`, and `:MS`. GoTo is rejected while offline or parked.
- Catalog rows now preserve 22 object subtypes, including galaxy morphology
  and separate nebula classes, with matching day and red night-mode glyphs.
- A short SELECT opens SETTINGS directly; in SETTINGS it activates the selected
  row. Hold SELECT for at least 600 ms for the emergency stop command.
- Choose **WI-FI / ONSTEP SETUP** and press A to use the on-device keyboard.
  The CASE key cycles lowercase, uppercase, and number/symbol layouts. Enter
  the Wi-Fi name, choose NEXT, enter the password, then choose ENTER. EXIT
  returns without changing the saved network. Credentials are stored in ESP32
  NVS and survive power-off. The temporary `OnStep-Remote-Setup` web page is
  still available automatically on a completely unconfigured first boot.
- Wi-Fi setup first shows three OnStep mount profiles. Move the stick to view
  them, press A to create/edit one, or press SELECT to activate a configured
  profile. The password is shown while editing because this is a local
  handheld setup screen. Most users can simply use Profile 1.
- A quick joystick flick moves one keyboard character. Holding it for 450 ms
  starts a controlled repeat every 150 ms.
- BRIGHTNESS cycles through 5%, 10%, 15%, 20%, 25%, 35%, and 50%. It defaults
  to 25%, is capped at 50%, and is saved in NVS.
- TEMPERATURE defaults to hidden. It is the ESP32 chip temperature, not the
  outdoor air or telescope temperature, and can be shown again from Settings.
- PULSE GRAPH is designed for classic OnStep. Blue is signed RA-coordinate
  change and green is signed Dec-coordinate change while `:GU#` reports an
  active guide pulse. `RA+`, `RA-`, `DE+`, and `DE-` show the inferred motion
  direction. It deliberately does not claim RMS guide error, because that
  information lives in the PC guider and is not reported by classic OnStep.

Add all of `shared/*.cpp/.h` and `platform_esp32/*.h` plus
`TDisplayS3Display.cpp` to the sketch. TFT_eSPI must be configured for the board
(commonly `Setup206_LilyGo_T_Display_S3`).

`OnStepRemote.ino` is the entry point. It sets `Mode::Standalone` (direct to the
mount, no guider). When you later add a laptop for imaging, swap
`OnStepStatusSource` for a NINA-based `StatusSource` and set `Mode::Imaging` — the
UI is unchanged.

## Before you fly it — verify these on the bench

- **`setHome` is intentionally a guarded no-op.** Setting home/park is
  destructive and firmware-specific; confirm the OnStepX 10.25i code, then wire it
  in `OnStepCommandSink::setHome()`. The UI already long-press-gates it.
- **`goHome` uses `:hC#`** (move to home). Confirm that matches your intent vs.
  reset-home.
- **Rate mapping** sends `:R1#..:R9#`. Your diagnostic used `:RC#` (centering) and
  `:R0..9#` presets — adjust `onstep::rateCmd()` if you prefer named rates.
- **Stop on release** sends the axis-specific `:Qn#/:Qs#/:Qe#/:Qw#`; e-stop sends
  `:Q#` (aborts goto/park too).
