#pragma once
// ESP32 / Arduino only. Concrete implementations of the four shared
// interfaces, built on OnStepClient (proven networking) and the tested
// onstep:: protocol decoder. The shared App/UI never changes.
#include <Arduino.h>
#include "Interfaces.h"
#include "OnStepProtocol.h"
#include "OnStepClient.h"

// ---- Status plane: poll OnStep directly, ONE query per update() tick -------
// A full status refresh is spread across several ticks so the control loop
// (buttons/joystick, handled earlier in App::update) stays responsive despite
// blocking connect-per-command networking.
class OnStepStatusSource : public StatusSource {
public:
    explicit OnStepStatusSource(OnStepClient& c) : c_(c) {}

    // Optionally quiet the poller during an active manual slew so the stick
    // stays crisp; the App can call this when it starts/stops jogging.
    void setSlewing(bool s) { slewing_ = s; }
    void setGuideBurst(bool b) { if (b != guideBurst_) idx_ = 0; guideBurst_ = b; }

    // ---- bring-up controls (driven from the serial console) ----
    void setEnabled(bool e)          { enabled_ = e; }
    bool enabled() const             { return enabled_; }
    void setIntervalMs(uint32_t ms)  { intervalMs_ = ms; }
    uint32_t intervalMs() const      { return intervalMs_; }
    const onstep::Raw& raw() const   { return raw_; }
    uint32_t cycleMs() const         { return lastCycleMs_; }  // last full refresh
    uint32_t cycleCount() const      { return cycles_; }

    void update(double) override {
        if (!enabled_) return;
        // Deliberate pacing: ONE blocking query every intervalMs_, not at
        // raw loop speed. 150 ms * 7 commands ~= 1 full refresh per second.
        uint32_t now = millis();
        if (now - lastPollMs_ < intervalMs_) return;
        lastPollMs_ = now;

        // Poll rotations: full status; jog (manual slew: GU+D only); guide
        // burst (GU every other slot at ~300 ms, coords still refreshing).
        static const char* full[]  = {":GU#", ":GR#", ":GD#", ":GA#", ":GZ#", ":GS#", ":D#"};
        static const char* jog[]   = {":GU#", ":D#"};
        static const char* burst[] = {":GU#", ":GR#", ":GU#", ":GD#", ":GU#", ":D#"};
        const char** list = slewing_ ? jog  : (guideBurst_ ? burst : full);
        int n              = slewing_ ? 2    : (guideBurst_ ? 6     : 7);

        if (idx_ == 0) cycleStartMs_ = now;
        // One-shot: site latitude (static) once the link is alive.
        if (!latFetched_ && c_.online()) {
            std::string lat = c_.query(":Gt#");
            if (!lat.empty()) { raw_.gtLat = lat; latFetched_ = true;
                Serial.printf("[poll] site latitude: %s\n", lat.c_str()); }
        }
        std::string val = c_.query(list[idx_ % n]);
        store(list[idx_ % n], val);
        // Publish partial updates too so classic OnStep's :GU# pulse flag is
        // visible immediately instead of waiting for a whole polling cycle.
        cached_ = onstep::toMountStatus(onstep::decode(raw_), c_.online());
        if (++idx_ >= n) {                 // completed a cycle -> publish
            idx_ = 0;
            lastCycleMs_ = millis() - cycleStartMs_;
            cycles_++;
        }
    }
    MountStatus status() const override { return cached_; }
    bool        online() const override { return c_.online(); }

    // Dump everything the poller knows, control bytes escaped.
    void printRaw() const {
        auto esc = [](const std::string& s) {
            for (char c : s) {
                if (c >= 32 && c < 127) Serial.print(c);
                else Serial.printf("<%02X>", (uint8_t)c);
            }
            Serial.println();
        };
        Serial.print("  :GU# = "); esc(raw_.gu);
        Serial.print("  :GR# = "); esc(raw_.gr);
        Serial.print("  :GD# = "); esc(raw_.gd);
        Serial.print("  :GA# = "); esc(raw_.ga);
        Serial.print("  :GZ# = "); esc(raw_.gz);
        Serial.print("  :GS# = "); esc(raw_.gs);
        Serial.print("  :D#  = "); esc(raw_.d);
        Serial.printf("  cycles=%lu lastCycle=%lums interval=%lums enabled=%d\n",
                      (unsigned long)cycles_, (unsigned long)lastCycleMs_,
                      (unsigned long)intervalMs_, (int)enabled_);
    }

private:
    void store(const std::string& cmd, const std::string& v) {
        if      (cmd == ":GU#") raw_.gu = v;
        else if (cmd == ":GR#") raw_.gr = v;
        else if (cmd == ":GD#") raw_.gd = v;
        else if (cmd == ":GA#") raw_.ga = v;
        else if (cmd == ":GZ#") raw_.gz = v;
        else if (cmd == ":D#")  raw_.d  = v;
        else if (cmd == ":GS#") raw_.gs = v;
    }
    OnStepClient& c_;
    onstep::Raw   raw_;
    MountStatus   cached_;
    int  idx_ = 0;
    bool slewing_ = false;
    bool guideBurst_ = false;
    bool enabled_ = true;
    uint32_t intervalMs_ = 150;
    uint32_t lastPollMs_ = 0, cycleStartMs_ = 0, lastCycleMs_ = 0, cycles_ = 0;
    bool latFetched_ = false;
};

// ---- Command plane: user actions straight to OnStep -----------------------
class OnStepCommandSink : public CommandSink {
public:
    explicit OnStepCommandSink(OnStepClient& c) : c_(c) {}
    void startSlew(SlewDir d) override { lastDir_ = d; c_.noReply(onstep::slewCmd(d)); }
    void stopSlew() override           { c_.noReply(onstep::stopCmd(lastDir_)); }
    void emergencyStop() override      { c_.noReply(onstep::stopAllCmd()); }
    void setRate(int r) override       { c_.noReply(onstep::rateCmd(r)); }
    void park() override               { c_.numeric(onstep::parkCmd()); }
    void unpark() override             { c_.numeric(onstep::unparkCmd()); }
    void goHome() override             { c_.numeric(onstep::goHomeCmd()); }
    void setHome() override {
        // Destructive and firmware-specific. Deliberately NOT firing a bare
        // code until verified against OnStepX 10.25i. Wire the confirmed
        // command here (e.g. reset-home) once checked on the bench.
        Serial.println("[setHome] guarded: verify OnStepX reset-home code before enabling");
    }
private:
    OnStepClient& c_;
    SlewDir       lastDir_ = SlewDir::None;
};

// ---- Guider plane: no PHD2 in the direct/standalone setup yet --------------
class NullGuideController : public GuideController {
public:
    void update(double) override {}
    GuideStatus status() const override { return GuideStatus{}; }  // Idle
    bool online() const override { return false; }
    void pause() override {}
    void resume() override {}
};

// ---- Handheld-local device info -------------------------------------------
class HandheldDeviceInfo : public DeviceInfo {
public:
    // batteryPin: T-Display-S3 battery sense (via 2:1 divider). Calibrate.
    explicit HandheldDeviceInfo(int batteryPin = 4) : pin_(batteryPin) {}
    void update(double) override {
        int rssi = WiFi.RSSI();                 // dBm, ~ -30 (strong) .. -90 (weak)
        if      (rssi >= -55) bars_ = 4;
        else if (rssi >= -65) bars_ = 3;
        else if (rssi >= -75) bars_ = 2;
        else if (rssi >= -85) bars_ = 1;
        else                  bars_ = 0;
        // Battery: 12-bit ADC, 3.3V ref, 2:1 divider -> volts, mapped 3.3..4.2V.
        float v = analogReadMilliVolts(pin_) / 1000.0f * 2.0f;
        int pct = (int)((v - 3.30f) / (4.20f - 3.30f) * 100.0f);
        battery_ = pct < 0 ? 0 : pct > 100 ? 100 : pct;
    }
    int batteryPercent() const override { return battery_; }
    int wifiBars() const override { return bars_; }
private:
    int pin_;
    int battery_ = 100;
    int bars_ = 0;
};
