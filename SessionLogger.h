#pragma once
// ESP32/Arduino. First-imaging-night data capture: compact, parseable,
// event-driven serial logging. Console: `log on`, `log off`,
// `log interval <s>` (snapshot period, default 10 s), `probe` (one-shot
// command survey). Copy/paste the serial output for later analysis.
//
// Format (comma-separated, one record per line, ms = uptime):
//   LOG,START,<ms>,v1                    logging enabled
//   EV,<ms>,STATE,<old>,<new>            movement state transition
//   EV,<ms>,PULSE,START                  guide pulse flag rose
//   EV,<ms>,PULSE,END,<dur_ms>           guide pulse flag fell (+duration)
//   EV,<ms>,TARGET,<raH>,<decD>          GoTo target changed (from :Gr#/:Gd#)
//   EV,<ms>,PARK,<0|1>                   park state change
//   EV,<ms>,LINK,<0|1>                   mount link up/down
//   SNAP,<ms>,<raH>,<decD>,<alt>,<az>,<pier>,<haH>,<trk>,<park>,<move>,<latMs>,<ok%>
//   PROBE,<cmd>,<raw reply, control bytes as <XX>>
//
// Pulse metrics are edge-timed from :GU#'s pulse flag at the poll rate
// (~1 Hz full refresh), so durations are approximate (+/- one poll cycle).
// Guide ERROR magnitude is NOT available from OnStep: PHD2 computes the
// error and sends only the resulting pulse; RMS lives in PHD2 (future:
// PHD2 event-server bridge on the laptop, port 4400).
#include <Arduino.h>
#include "OnStepClient.h"
#include "OnStepBackends.h"
#include "MountStatus.h"

class SessionLogger {
public:
    SessionLogger(OnStepClient& c, OnStepStatusSource& s) : c_(c), s_(s) {}

    void setEnabled(bool e) {
        if (e && !enabled_) Serial.printf("LOG,START,%lu,v1\n", (unsigned long)millis());
        if (!e && enabled_) Serial.printf("LOG,STOP,%lu\n", (unsigned long)millis());
        enabled_ = e;
    }
    bool enabled() const { return enabled_; }
    void setSnapshotSecs(uint32_t s) { snapMs_ = s < 2 ? 2000 : s * 1000; }

    // Call every loop; cheap unless something changed or a snapshot is due.
    void update() {
        if (!enabled_) return;
        uint32_t now = millis();
        MountStatus m = s_.status();

        // --- state transitions ---
        const char* mv = moveName(m);
        if (strcmp(mv, lastMove_) != 0) {
            Serial.printf("EV,%lu,STATE,%s,%s\n", (unsigned long)now, lastMove_, mv);
            strncpy(lastMove_, mv, sizeof lastMove_ - 1);
        }
        if (m.parked != lastParked_) {
            Serial.printf("EV,%lu,PARK,%d\n", (unsigned long)now, (int)m.parked);
            lastParked_ = m.parked;
        }
        bool link = s_.online();
        if (link != lastLink_) {
            Serial.printf("EV,%lu,LINK,%d\n", (unsigned long)now, (int)link);
            lastLink_ = link;
        }

        // --- pulse edges (approximate: sampled at poll rate) ---
        if (m.guidePulseActive && !lastPulse_) {
            pulseT0_ = now;
            Serial.printf("EV,%lu,PULSE,START\n", (unsigned long)now);
        }
        if (!m.guidePulseActive && lastPulse_) {
            Serial.printf("EV,%lu,PULSE,END,%lu\n", (unsigned long)now,
                          (unsigned long)(now - pulseT0_));
            pulseCount_++;
        }
        lastPulse_ = m.guidePulseActive;

        // --- GoTo target: poll during slews, sparsely otherwise ---
        uint32_t targetEvery = (m.goingTo || m.slewing) ? 3000 : 30000;
        if (link && now - lastTargetMs_ > targetEvery) {
            lastTargetMs_ = now;
            std::string tr = c_.query(":Gr#"), td = c_.query(":Gd#");
            if (!tr.empty() && !td.empty() &&
                (tr != lastTr_ || td != lastTd_)) {
                lastTr_ = tr; lastTd_ = td;
                Serial.printf("EV,%lu,TARGET,%s,%s\n", (unsigned long)now,
                              tr.c_str(), td.c_str());
            }
        }

        // --- periodic snapshot ---
        if (now - lastSnapMs_ >= snapMs_) {
            lastSnapMs_ = now;
            double ha = -99;
            if (m.localSiderealHours >= 0) {
                ha = m.localSiderealHours - m.raHours;
                while (ha > 12) ha -= 24; while (ha < -12) ha += 24;
            }
            uint32_t tot = c_.okCount() + c_.failCount();
            Serial.printf("SNAP,%lu,%.5f,%.4f,%.2f,%.2f,%d,%.3f,%d,%d,%s,%lu,%lu\n",
                          (unsigned long)now, m.raHours, m.decDeg, m.altDeg, m.azDeg,
                          m.pierSide, ha, (int)m.tracking, (int)m.parked, mv,
                          (unsigned long)c_.lastMs(),
                          (unsigned long)(tot ? c_.okCount() * 100 / tot : 100));
        }
    }

    // One-shot survey: everything the firmware might answer, raw and escaped.
    // Run this once tonight (mount unparked, tracking) and paste the block.
    void probe() {
        static const char* cmds[] = {
            ":GVP#", ":GVN#", ":GVD#",          // product / firmware / date
            ":GR#", ":GD#",                     // current RA/DEC
            ":Gr#", ":Gd#",                     // TARGET RA/DEC (GoTo destination)
            ":GA#", ":GZ#",                     // alt / az
            ":GS#", ":GL#", ":GC#",             // sidereal / local time / date
            ":Gt#", ":Gg#",                     // site latitude / longitude
            ":GT#",                             // tracking rate (Hz)
            ":GU#", ":D#", ":GW#", ":Gm#",      // status / moving / align+track / pier side
            ":GE#",                             // last error
            ":GX9A#", ":GX9B#", ":GX9C#",       // ambient T(C)/P(mb)/H(%) -- 10/1010/70 = OnStep defaults, no sensor
            ":GXFA#",                           // OnStepX extended: axis1 (if supported)
        };
        bool wasEnabled = s_.enabled();
        s_.setEnabled(false);
        Serial.println("PROBE,BEGIN");
        for (auto cmd : cmds) {
            std::string r = c_.query(cmd, 1200);
            Serial.printf("PROBE,%s,", cmd);
            if (r.empty()) Serial.println(
                c_.lastFail()==OnStepClient::FAIL_NONE ? "<bare-#-idle>" : "<timeout>");
            else {
                for (char ch : r) {
                    if (ch >= 32 && ch < 127) Serial.print(ch);
                    else Serial.printf("<%02X>", (uint8_t)ch);
                }
                Serial.println();
            }
            delay(60);
        }
        Serial.printf("PROBE,END,pulses_seen=%lu\n", (unsigned long)pulseCount_);
        s_.setEnabled(wasEnabled);
    }

private:
    static const char* moveName(const MountStatus& m) {
        if (m.parked)  return "PARKED";
        // Classic OnStep (4.x) drops the 'N' not-slewing flag during pulse
        // guides, so pulses also look like slews; check GUIDE first to keep
        // guiding-night logs quiet (no TRACK->SLEW spam per pulse).
        if (m.guidePulseActive) return "GUIDE";
        if (m.goingTo) return "GOTO";
        if (m.slewing) return "SLEW";
        if (m.tracking) return "TRACK";
        return "IDLE";
    }
    OnStepClient&       c_;
    OnStepStatusSource& s_;
    bool     enabled_ = false;
    char     lastMove_[8] = "IDLE";
    bool     lastParked_ = false, lastLink_ = false, lastPulse_ = false;
    uint32_t pulseT0_ = 0, pulseCount_ = 0;
    uint32_t lastTargetMs_ = 0, lastSnapMs_ = 0, snapMs_ = 10000;
    std::string lastTr_, lastTd_;
};
