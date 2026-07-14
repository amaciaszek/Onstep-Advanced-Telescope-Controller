#pragma once
// ESP32 / Arduino only. Wraps the PROVEN networking from the diagnostic
// sketch (connect-per-command, three reply kinds) behind a small class the
// backends can call. Behavior is intentionally unchanged from what already
// works on the hardware.
//
// BRING-UP ADDITIONS: verbose wire logging (toggle with `verbose on` in the
// serial console), per-call timing, and failure counters so a dead link is
// diagnosable from the serial monitor alone.
#include <Arduino.h>
#include <WiFi.h>
#include <string>
#include <cstdlib>

class OnStepClient {
public:
    enum ReplyKind { REPLY_HASH, REPLY_NUMERIC, REPLY_NONE };

    // Failure reason of the most recent send(), for diagnostics.
    enum FailReason { FAIL_NONE, FAIL_WIFI, FAIL_TCP_CONNECT, FAIL_TIMEOUT, FAIL_PEER_CLOSED };

    void begin(const char* ssid, const char* pass, IPAddress ip, uint16_t port) {
        ssid_ = ssid ? ssid : ""; pass_ = pass ? pass : ""; ip_ = ip; port_ = port;
    }

    void setVerbose(bool v) { verbose_ = v; }
    void setPort(uint16_t p) { port_ = p; }
    uint16_t port() const { return port_; }
    bool verbose() const { return verbose_; }

    bool ensureWiFi(uint32_t timeoutMs = 250) {
        if (WiFi.status() == WL_CONNECTED) return true;
        if (lastWiFiAttemptMs_ && millis() - lastWiFiAttemptMs_ < 5000) return false;
        lastWiFiAttemptMs_ = millis();
        Serial.printf("[wifi] connecting to '%s'...\n", ssid_.c_str());
        WiFi.mode(WIFI_STA);
        WiFi.begin(ssid_.c_str(), pass_.c_str());
        uint32_t start = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs) delay(200);
        linked_ = (WiFi.status() == WL_CONNECTED);
        if (linked_) {
            Serial.printf("[wifi] connected, IP=%s RSSI=%d dBm (%lu ms)\n",
                          WiFi.localIP().toString().c_str(), WiFi.RSSI(),
                          (unsigned long)(millis() - start));
        } else {
            Serial.printf("[wifi] FAILED after %lu ms, status=%d "
                          "(0=idle 1=no-ssid 4=conn-fail 5=lost 6=disconnected)\n",
                          (unsigned long)(millis() - start), (int)WiFi.status());
        }
        return linked_;
    }

    // Hash-terminated query -> stripped payload, or "" on failure.
    std::string query(const std::string& cmd, uint32_t timeoutMs = 2500) {
        String resp;
        if (!send(cmd.c_str(), REPLY_HASH, resp, timeoutMs)) { linked_ = false; return ""; }
        linked_ = true;
        resp.trim();
        if (resp.endsWith("#")) resp.remove(resp.length() - 1);
        return std::string(resp.c_str());
    }

    // Numeric command -> true iff OnStep replied '1'.
    bool numeric(const std::string& cmd, uint32_t timeoutMs = 2500) {
        String resp;
        bool ok = send(cmd.c_str(), REPLY_NUMERIC, resp, timeoutMs);
        linked_ = ok || linked_;
        return ok && resp == "1";
    }

    // Fire-and-forget (e.g. :Me# :Qe# :RC# :Q#). Optionally read :GE# after.
    void noReply(const std::string& cmd) {
        String resp;
        send(cmd.c_str(), REPLY_NONE, resp, 300);
    }

    int lastError() {
        std::string e = query(":GE#");
        if (e.size() >= 1) return std::atoi(e.c_str());
        return -1;
    }

    bool online() const { return WiFi.status() == WL_CONNECTED && linked_; }

    // ---- diagnostics ----
    FailReason lastFail() const { return lastFail_; }
    uint32_t okCount() const { return okCount_; }
    uint32_t failCount() const { return failCount_; }
    uint32_t lastMs() const { return lastCallMs_; }   // duration of last send()
    void printStats() {
        Serial.printf("[client] ok=%lu fail=%lu lastCall=%lums lastFail=%s linked=%d\n",
                      (unsigned long)okCount_, (unsigned long)failCount_,
                      (unsigned long)lastCallMs_, failName(lastFail_), (int)linked_);
    }

private:
    static const char* failName(FailReason f) {
        switch (f) {
            case FAIL_WIFI:        return "wifi";
            case FAIL_TCP_CONNECT: return "tcp-connect";
            case FAIL_TIMEOUT:     return "timeout";
            case FAIL_PEER_CLOSED: return "peer-closed(channel busy?)";
            default:               return "none";
        }
    }
    // Print a reply with control bytes made visible (":D#" answers 0x7F).
    static void printEscaped(const String& s) {
        for (size_t i = 0; i < s.length(); ++i) {
            char c = s[i];
            if (c >= 32 && c < 127) Serial.print(c);
            else Serial.printf("<%02X>", (uint8_t)c);
        }
    }

    // Direct port of the working sendCommand(): new TCP connection per call,
    // reply handling switched on ReplyKind.
    bool send(const char* command, ReplyKind kind, String& out, uint32_t timeoutMs) {
        out = "";
        uint32_t t0 = millis();
        lastFail_ = FAIL_NONE;
        if (!ensureWiFi()) { fail(FAIL_WIFI, command, t0); return false; }
        client_.stop();
        delay(20);
        if (!client_.connect(ip_, port_)) { fail(FAIL_TCP_CONNECT, command, t0); return false; }
        while (client_.available()) client_.read();
        client_.print(command);

        if (kind == REPLY_NONE) {
            delay(150); client_.stop();
            lastCallMs_ = millis() - t0; okCount_++;
            if (verbose_) Serial.printf("[tx] %-8s (no-reply, %lums)\n", command,
                                        (unsigned long)lastCallMs_);
            return true;
        }

        uint32_t start = millis();
        while (millis() - start < timeoutMs) {
            // Peer closed with no (complete) reply: on a 1-client-per-channel
            // command server this is the signature of an occupied channel.
            // Bail immediately instead of burning the full timeout.
            if (!client_.connected() && !client_.available()) {
                client_.stop();
                lastCallMs_ = millis() - t0;
                lastFail_ = FAIL_PEER_CLOSED; failCount_++;
                Serial.printf("[tx] %-8s PEER CLOSED after %lums, partial='", command,
                              (unsigned long)lastCallMs_);
                printEscaped(out); Serial.println("' -> try another port (9998/9997/9996)?");
                return false;
            }
            while (client_.available()) {
                char c = client_.read();
                if (kind == REPLY_NUMERIC) {
                    if (c == '\r' || c == '\n' || c == ' ') continue;
                    out += c;
                    if (c == '0' || c == '1') { done(command, out, t0); return true; }
                } else { // REPLY_HASH
                    out += c;
                    if (c == '#') { done(command, out, t0); return true; }
                }
            }
            delay(1);
        }
        client_.stop();
        lastCallMs_ = millis() - t0;
        lastFail_ = FAIL_TIMEOUT; failCount_++;
        Serial.printf("[tx] %-8s TIMEOUT after %lums, partial='", command,
                      (unsigned long)lastCallMs_);
        printEscaped(out); Serial.println("'");
        return false; // timed out
    }

    void done(const char* cmd, const String& out, uint32_t t0) {
        client_.stop();
        lastCallMs_ = millis() - t0; okCount_++;
        if (verbose_) {
            Serial.printf("[tx] %-8s -> '", cmd);
            printEscaped(out);
            Serial.printf("' (%lums)\n", (unsigned long)lastCallMs_);
        }
    }
    void fail(FailReason r, const char* cmd, uint32_t t0) {
        lastFail_ = r; failCount_++;
        lastCallMs_ = millis() - t0;
        Serial.printf("[tx] %-8s FAILED (%s) after %lums\n", cmd, failName(r),
                      (unsigned long)lastCallMs_);
    }

    std::string ssid_;
    std::string pass_;
    IPAddress   ip_;
    uint16_t    port_ = 9999;
    WiFiClient  client_;
    bool        linked_ = false;
    bool        verbose_ = false;
    FailReason  lastFail_ = FAIL_NONE;
    uint32_t    okCount_ = 0, failCount_ = 0, lastCallMs_ = 0;
    uint32_t    lastWiFiAttemptMs_ = 0;
};
