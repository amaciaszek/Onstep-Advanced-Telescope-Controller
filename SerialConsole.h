#pragma once
// ESP32 / Arduino only. Bring-up serial console: type commands into the
// Arduino IDE Serial Monitor (115200, newline line ending) while the device
// runs. Everything here is diagnostic; nothing is required for normal use.
//
// Commands:
//   help              this list
//   wifi              WiFi state, IP, RSSI, mount reachability
//   heap              free heap / min free heap / PSRAM
//   stats             OnStepClient call counters + poller cycle timing
//   status            decoded MountStatus snapshot the UI is displaying
//   raw               raw (undecoded) reply strings from the last poll cycle
//   verbose on|off    log every command/reply on the wire with timing
//   poll on|off       pause/resume the status poller (quiet the wire)
//   interval <ms>     ms between individual status commands (default 150)
//   send <cmd>        send hash-terminated query, print reply  e.g. send :GR#
//   sendn <cmd>       send numeric command, print 0/1          e.g. sendn :hP#
//   sendx <cmd>       send fire-and-forget command             e.g. sendx :Q#
//   estop             shortcut for sendx :Q#
//   pad               live seesaw joystick/button readout (any key to stop)
//   fake on|off       drive the UI with synthetic moving data (no mount needed)
//   port <n>          switch OnStep command port live (9999/9998/9997/9996)
//   bench <n>         send n :GR# queries, report latency stats -- run this
//                     WHILE the laptop is connected/guiding to measure contention
//   reboot            ESP.restart()
#include <Arduino.h>
#include <WiFi.h>
#include <cmath>
#include "OnStepClient.h"
#include "OnStepBackends.h"
#include "InputSeesaw.h"
#include "MountStatus.h"

class SerialConsole {
public:
    SerialConsole(OnStepClient& c, OnStepStatusSource& s, InputSeesaw& in)
        : c_(c), s_(s), in_(in) {}

    void setScreenHandler(void (*fn)()) { screenFn_ = fn; }
    void setNightHandler(void (*fn)(bool)) { nightFn_ = fn; }
    void setWidgetHandler(void (*fn)(int)) { widgetFn_ = fn; }
    void setLogHandler(void (*fn)(int)) { logFn_ = fn; }   // -1 off, 0 probe, 1 on, >=2 interval secs

    // ---- fake data mode: exercises every dashboard widget without a mount --
    bool fakeActive() const { return fake_; }
    MountStatus fakeStatus() {
        double t = millis() / 1000.0;
        MountStatus m;
        m.link = LinkState::Connected;
        m.tracking = true;
        m.parked = fmod(t, 30.0) > 25.0;             // parks for 5s every 30s
        m.raHours  = fmod(t / 20.0, 24.0);
        m.decDeg   = 41.0 + 20.0 * sin(t / 9.0);
        m.altDeg   = 45.0 + 40.0 * sin(t / 7.0);
        m.azDeg    = fmod(t * 12.0, 360.0);
        m.pierSide = (fmod(t, 40.0) < 20.0) ? 1 : 2;
        m.guidePulseActive = fmod(t, 4.0) < 2.0;
        m.localSiderealHours = fmod(m.raHours + 1.5, 24.0);
        m.slewRate = 1 + (int)fmod(t / 5.0, 9.0);
        return m;
    }

    // Call once per loop(); reads serial without blocking.
    void update() {
        while (Serial.available()) {
            char ch = Serial.read();
            if (ch == '\r') continue;
            if (ch == '\n') { handle(line_); line_ = ""; }
            else if (line_.length() < 96) line_ += ch;
        }
    }

private:
    void handle(String cmd) {
        cmd.trim();
        if (cmd.length() == 0) return;
        Serial.printf("> %s\n", cmd.c_str());

        if (cmd == "help") {
            Serial.println(
                "help wifi heap stats status raw | verbose on|off | poll on|off\n"
                "interval <ms> | send <c> sendn <c> sendx <c> | estop | pad\n"
                "port <n> | bench <n> | screen | night on|off | widget <0-3>\n"
                "log on|off | log interval <s> | probe | fake on|off | reboot");
        }
        else if (cmd == "wifi") {
            Serial.printf("[wifi] status=%d connected=%d SSID='%s' IP=%s RSSI=%d dBm\n",
                          (int)WiFi.status(), WiFi.status() == WL_CONNECTED,
                          WiFi.SSID().c_str(), WiFi.localIP().toString().c_str(),
                          WiFi.RSSI());
            Serial.printf("[wifi] mount link flag (last cmd ok): %d\n", (int)c_.online());
        }
        else if (cmd == "heap") {
            Serial.printf("[heap] free=%u minFree=%u largest=%u psramFree=%u\n",
                          (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMinFreeHeap(),
                          (unsigned)ESP.getMaxAllocHeap(), (unsigned)ESP.getFreePsram());
        }
        else if (cmd == "stats")  { c_.printStats(); }
        else if (cmd == "status") { printStatus(s_.status()); }
        else if (cmd == "raw")    { s_.printRaw(); }
        else if (cmd == "verbose on")  { c_.setVerbose(true);  Serial.println("[console] wire logging ON"); }
        else if (cmd == "verbose off") { c_.setVerbose(false); Serial.println("[console] wire logging OFF"); }
        else if (cmd == "poll on")  { s_.setEnabled(true);  Serial.println("[console] poller ON"); }
        else if (cmd == "poll off") { s_.setEnabled(false); Serial.println("[console] poller OFF (wire quiet)"); }
        else if (cmd.startsWith("interval ")) {
            long ms = cmd.substring(9).toInt();
            if (ms < 20) ms = 20;
            s_.setIntervalMs((uint32_t)ms);
            Serial.printf("[console] poll interval = %ld ms (~%.1f full refresh/s)\n",
                          ms, 1000.0 / (ms * 7.0));
        }
        else if (cmd.startsWith("send "))  { doSend(cmd.substring(5), 0); }
        else if (cmd.startsWith("sendn ")) { doSend(cmd.substring(6), 1); }
        else if (cmd.startsWith("sendx ")) { doSend(cmd.substring(6), 2); }
        else if (cmd == "estop") {
            c_.noReply(":Q#");
            Serial.println("[console] :Q# sent (emergency stop)");
        }
        else if (cmd.startsWith("port ")) {
            long p = cmd.substring(5).toInt();
            if (p < 1 || p > 65535) { Serial.println("[console] bad port"); return; }
            c_.setPort((uint16_t)p);
            Serial.printf("[console] OnStep port -> %ld (channels: 9999/9998/9997/9996)\n", p);
        }
        else if (cmd.startsWith("bench ")) { bench(cmd.substring(6).toInt()); }
        else if (cmd == "night on")  { if (nightFn_) { nightFn_(true);  Serial.println("[console] NIGHT mode (red)"); } }
        else if (cmd == "night off") { if (nightFn_) { nightFn_(false); Serial.println("[console] day mode"); } }
        else if (cmd == "log on")   { if (logFn_) logFn_(1); }
        else if (cmd == "log off")  { if (logFn_) logFn_(-1); }
        else if (cmd.startsWith("log interval ")) {
            long sec = cmd.substring(13).toInt();
            if (logFn_ && sec >= 2) { logFn_((int)sec); Serial.printf("[console] snapshot every %ld s\n", sec); }
        }
        else if (cmd == "probe")    { if (logFn_) logFn_(0); }
        else if (cmd.startsWith("widget ")) {
            long w = cmd.substring(7).toInt();
            if (widgetFn_) { widgetFn_((int)w);
                Serial.printf("[console] radar widget -> %ld\n", w); }
        }
        else if (cmd == "screen") {
            if (screenFn_) { screenFn_(); Serial.println("[console] next screen"); }
        }
        else if (cmd == "pad") { padMonitor(); }
        else if (cmd == "fake on")  { fake_ = true;  Serial.println("[console] FAKE data driving UI"); }
        else if (cmd == "fake off") { fake_ = false; Serial.println("[console] fake data OFF"); }
        else if (cmd == "reboot") { Serial.println("[console] restarting..."); Serial.flush(); ESP.restart(); }
        else Serial.println("[console] unknown command; type 'help'");
    }

    void doSend(String c, int kind) {
        c.trim();
        if (c.length() == 0) { Serial.println("[console] usage: send :GR#"); return; }
        bool wasVerbose = c_.verbose();
        c_.setVerbose(true);                 // always show wire detail for manual sends
        if (kind == 0) {
            std::string r = c_.query(std::string(c.c_str()));
            Serial.printf("[console] payload='%s' (%u chars)\n", r.c_str(), (unsigned)r.size());
        } else if (kind == 1) {
            bool ok = c_.numeric(std::string(c.c_str()));
            Serial.printf("[console] numeric reply: %s\n", ok ? "1 (success)" : "0/none (failure)");
        } else {
            c_.noReply(std::string(c.c_str()));
            Serial.println("[console] sent, no reply expected");
        }
        c_.setVerbose(wasVerbose);
    }

    void printStatus(const MountStatus& m) {
        Serial.printf("  link=%s tracking=%d parked=%d atHome=%d slewing=%d goingTo=%d\n",
                      m.link == LinkState::Connected ? "connected" : "down",
                      (int)m.tracking, (int)m.parked, (int)m.homeSet,
                      (int)m.slewing, (int)m.goingTo);
        Serial.printf("  RA=%.5f h  DEC=%+.4f deg\n", m.raHours, m.decDeg);
        Serial.printf("  ALT=%.3f deg  AZ=%.3f deg  pier=%d (0?,1E,2W)\n",
                      m.altDeg, m.azDeg, m.pierSide);
        Serial.printf("  LST=%.5f h  guidePulse=%d  rate=%dx\n",
                      m.localSiderealHours, (int)m.guidePulseActive, m.slewRate);
        if (m.localSiderealHours >= 0) {
            double ha = m.localSiderealHours - m.raHours;
            while (ha > 12) ha -= 24;
            while (ha < -12) ha += 24;
            Serial.printf("  hourAngle=%+.3f h (%s of meridian)\n",
                          ha, ha < 0 ? "east" : "west");
        }
    }

    // Latency benchmark: the on-device contention probe. Run it with the
    // laptop idle, then again while NINA/PHD2 are connected and guiding, and
    // compare. Rising p95/max or peer-closed failures = channel contention.
    void bench(long n) {
        if (n < 1) n = 20;
        if (n > 500) n = 500;
        bool wasEnabled = s_.enabled();
        s_.setEnabled(false);                       // quiet the poller
        Serial.printf("[bench] %ld x :GR# on port %u...\n", n, (unsigned)c_.port());
        uint32_t lat[500];
        long ok = 0, fail = 0;
        for (long i = 0; i < n; ++i) {
            std::string r = c_.query(":GR#");
            if (r.empty()) fail++;
            else lat[ok++] = c_.lastMs();
            delay(30);
        }
        if (ok) {
            // insertion sort (n small)
            for (long i = 1; i < ok; ++i) {
                uint32_t k = lat[i]; long j = i - 1;
                while (j >= 0 && lat[j] > k) { lat[j+1] = lat[j]; --j; }
                lat[j+1] = k;
            }
            Serial.printf("[bench] ok=%ld fail=%ld  lat ms: min=%lu med=%lu p95=%lu max=%lu\n",
                          ok, fail, (unsigned long)lat[0], (unsigned long)lat[ok/2],
                          (unsigned long)lat[(ok*95)/100 < ok ? (ok*95)/100 : ok-1],
                          (unsigned long)lat[ok-1]);
        } else {
            Serial.printf("[bench] ALL %ld FAILED -- ", n);
            c_.printStats();
        }
        s_.setEnabled(wasEnabled);
    }

    void padMonitor() {
        if (!in_.available()) { Serial.println("[pad] seesaw not detected at 0x50"); return; }
        Serial.println("[pad] live readout; send any character to stop");
        while (Serial.available()) Serial.read();
        while (!Serial.available()) {
            in_.update();
            ButtonEvent ev;
            String btns = "";
            while (in_.nextButton(ev)) {
                const char* names[] = {"A", "B", "X", "Y", "Start", "Select"};
                btns += names[(int)ev.button];
                if (ev.longPress) btns += "(long)";
                btns += " ";
            }
            Serial.printf("[pad] joy=(%+.2f,%+.2f) %s\n",
                          in_.joyX(), in_.joyY(),
                          btns.length() ? btns.c_str() : "");
            delay(150);
        }
        while (Serial.available()) Serial.read();
        Serial.println("[pad] stopped");
    }

    OnStepClient&       c_;
    OnStepStatusSource& s_;
    InputSeesaw&        in_;
    String line_;
    bool fake_ = false;
    void (*screenFn_)() = nullptr;
    void (*nightFn_)(bool) = nullptr;
    void (*widgetFn_)(int) = nullptr;
    void (*logFn_)(int) = nullptr;
};
