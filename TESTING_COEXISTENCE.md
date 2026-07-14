# Coexistence testing: handheld + laptop on OnStep simultaneously

## What the research says (OnStep wiki)
The command channels are ports 9999/9998/9997/9996, **one client per
channel**. Port 9999 can be shared by multiple programs *except when the
web server runs on the ESP32* — which is your Juwei's configuration. So
plan for strict 1-client-per-port, and verify which of 9998/9997/9996 your
built-in-WiFi OnStepX build actually opens (the wiki text is written for
the external SWS; the ESP32 Website Plugin may differ — this is
finding #1 to establish).

## What the simulations established (run in this session, tools/ included)

**A. Same port, laptop persistent + handheld per-command:**
laptop keeps its slot and is completely unaffected (median latency 10.6 ms,
zero drops across all runs). The handheld is always the loser — connections
are accepted-then-closed or stalled. **The laptop/guiding side never breaks.
Contention only degrades the handheld.**

**B. Separate ports:** both perfectly clean. Handheld per-command overhead
~30 ms/query; laptop latency unchanged.

**C. Pacing sweep (0/50/150 ms):** all clean even unpaced, but 150 ms holds
command-processor load to ~7 cmd/s — leave it there.

**D. Guiding-latency proxy:** laptop command latency with handheld polling
a separate channel: p95 rose 10.7→11.4 ms, max 10.8→15.1 ms. A few ms of
worst-case added serialization delay vs guide pulses of hundreds of ms and
guide exposures of seconds: **not measurable in RMS.** Pulse guide commands
also cannot be cancelled or corrupted by another connection's reads —
OnStep executes each command atomically.

**Stall mode (accepted-but-never-serviced):** each handheld command burned
the full 2.5 s timeout → 17.5 s frozen poll cycles. This motivated the
firmware fix below.

## Firmware changes made for this
1. **Fast-fail on peer close** (OnStepClient.h): read loop now exits the
   moment the peer closes with no reply — new `FAIL_PEER_CLOSED` reason,
   logged with a hint to try another port. Busy-channel poll cycle measured
   at **0.14 s (7/7 fast-fails) instead of ~17.5 s**. UI shows link-down but
   stays responsive.
2. **`port <n>` console command** — switch channels live, no reflash.
3. **`bench <n>` console command** — n timed :GR# queries with
   min/med/p95/max. This is your contention meter on the real mount.

## Field protocol (in risk order — mount can stay parked through step 5)
1. **No mount:** run `tools/mock_onstepx.py` on the laptop, point the
   handheld's ONSTEP_IP at the laptop (join same network, `port 9999`).
   Full firmware validation against fake data. Try `--busy-mode stall`
   and `--max-clients 2` variants.
2. **Real mount, parked, laptop disconnected:** handheld alone on 9999.
   `verbose on`, `raw`, confirm decode. `bench 50` → baseline latency.
3. **Discovery:** `port 9998` then `send :GR#` (repeat 9997/9996). Which
   channels answer? (Finding #1.)
4. **Contention, parked:** connect NINA/ASCOM as usual. On the handheld
   (still on 9999): does polling now fast-fail with `peer-closed`? Or do
   both work (meaning your build allows >1 client)? Does the ASCOM driver
   stay connected either way? Then `port 9998`, confirm clean coexistence.
   `bench 50` with laptop connected vs step-2 baseline.
5. **Tracking, unparked, safe pointing:** repeat step 4. Run
   `tools/contention_test.py --host <mount-ip>` from the laptop
   (read-only queries) for the scripted version.
6. **Guiding:** start PHD2 on a real star, note baseline RMS for 5 min
   with handheld `poll off`. Then `poll on` (on the handheld's dedicated
   port) for 5 min. Compare RMS and PHD2's guide log for dropped/late
   pulses. Given result D, expect no visible difference; the guide log is
   the arbiter.

## Decision rule
- If a spare channel (9998/9997/9996) answers: hardcode the handheld there,
  laptop keeps 9999. Done — cleanest possible outcome.
- If ONLY 9999 exists on your build: options are (a) OnStepX config change
  to enable more command-server clients (`COMMAND_SERVER` /
  standard-command-channel settings in Config.h — you compile your own
  OnStepX, so this is one line), or (b) accept alternation: the fast-fail
  client means the handheld just shows link-down while the laptop owns the
  channel, and recovers the instant the laptop disconnects. Nothing breaks.
