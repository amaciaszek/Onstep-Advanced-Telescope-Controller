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

- The gamepad is probed at address `0x50` on SDA/SCL `43/44`, then `8/9`.
  Define `ONSTEP_I2C_SDA` and `ONSTEP_I2C_SCL` before including
  `InputSeesaw.h` if your wiring uses another pair. The selected pins are
  printed at boot.
- START cycles HOME, DIAGNOSTICS, and SETTINGS even when Wi-Fi or OnStep is
  offline. On SETTINGS, move the stick up/down and press A to change a value.
- Choose **WI-FI / ONSTEP SETUP**, press A, then join the open
  `OnStep-Remote-Setup` access point with a phone or laptop and open
  `http://192.168.4.1/`. SSID, password, mount IP, and TCP port are saved in
  ESP32 NVS and survive power-off. These few strings use only a tiny fraction
  of flash storage.

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
