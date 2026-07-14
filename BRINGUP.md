# Bring-up notes (v2, debugged)

## Fixed since the ChatGPT handoff
1. **LvglDashboard.cpp would not compile** — two hard errors:
   - `lv_obj_set_size(7,7)` in makeMeridian() was missing the object argument.
   - `guideLine` (no underscore) used in three style calls in makeGuideActivity().
2. **Empty `:GU#` decoded as "tracking + GoTo active"** — `!has(gu,'n')` on an
   empty string is true. decode() now returns safe defaults when the mount
   never answered.
3. **Poller ran at raw loop speed** — OnStepStatusSource now paces itself:
   one command every 150 ms (~1 full 7-command refresh/sec). Tunable live
   with the `interval` console command.

## New: serial debug console (SerialConsole.h)
Serial Monitor @ 115200, line ending = Newline. Type `help`. Highlights:
- `verbose on` — every wire command/reply with timing, control bytes escaped
  (`:D#` shows `<7F>` when slewing).
- `raw` — undecoded reply strings from the last poll cycle. Use this to
  verify `:GU#` flags against the real OnStepX 10.25i in each mount state.
- `status` — the decoded MountStatus the UI is drawing, incl. hour angle.
- `send :GR#` / `sendn :hP#` / `sendx :Q#` — manual commands, three reply kinds.
- `fake on` — synthetic moving data drives every widget with no mount/WiFi.
  Use this FIRST to validate the display before touching the network.
- `pad` — live seesaw joystick/button monitor.
- `poll off`, `interval <ms>`, `stats`, `heap`, `wifi`, `estop`, `reboot`.

## Suggested first-flash sequence
1. Flash, open Serial Monitor. Boot log prints reset reason, heap, display
   init result.
2. `fake on` — confirm all widgets animate (RA/DEC, radar dot orbits,
   meridian dot flips sides, park icon toggles, guide graph wiggles).
3. `fake off`, `verbose on` — watch real polling. Any timeout prints the
   partial buffer it received.
4. `raw` in each mount state (parked/unparked/tracking/jog/goto/guiding)
   and save the `:GU#` strings — this is the pending hardware verification.

## Arduino IDE setup gotchas (will bite before any of the above)
- **lv_conf.h placement**: the copy in the sketch folder is NOT seen by the
  LVGL library sources. Copy `lv_conf.h` to your Arduino `libraries/` folder
  (as a sibling of the `lvgl` library folder, i.e. `libraries/lv_conf.h`).
  If the library compiles with a different conf than the sketch sees, you get
  silent ABI mismatches (garbage rendering) — keep the two files identical,
  or delete the sketch copy after moving it.
- **TFT_eSPI**: enable `Setup206_LilyGo_T_Display_S3.h` in
  `TFT_eSPI/User_Setup_Select.h`. The FATAL display message at boot means
  this wasn't done (width/height won't be 320x170).
- **Board settings**: ESP32S3 Dev Module, OPI PSRAM, USB CDC On Boot =
  Enabled (otherwise Serial goes nowhere on the T-Display-S3).
- The legacy framebuffer files (App.cpp, Display.cpp, UI.cpp, Fonts.cpp,
  FramebufferDisplay.cpp, TDisplayS3Display.cpp) all compile alongside the
  sketch. Verified they build; the linker strips them (--gc-sections). They
  slow compilation but are harmless. `desktop_only/` is ignored by Arduino.
