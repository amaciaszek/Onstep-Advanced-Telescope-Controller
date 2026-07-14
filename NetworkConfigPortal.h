#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include <WebServer.h>
#include <WiFi.h>

struct NetworkConfig {
    String ssid = "TerransOnStep";
    String password = "password";
    IPAddress mountIp = IPAddress(192, 168, 0, 1);
    uint16_t port = 9999;
};

class NetworkConfigPortal {
public:
    void begin(Preferences& prefs) {
        prefs_ = &prefs;
        config_.ssid = prefs.getString("wifi_ssid", config_.ssid);
        config_.password = prefs.getString("wifi_pass", config_.password);
        IPAddress parsed;
        if (parsed.fromString(prefs.getString("mount_ip", "192.168.0.1"))) config_.mountIp = parsed;
        config_.port = prefs.getUShort("mount_port", 9999);
    }
    const NetworkConfig& config() const { return config_; }
    bool active() const { return active_; }

    void start() {
        if (active_) return;
        WiFi.mode(WIFI_AP_STA);
        WiFi.softAP("OnStep-Remote-Setup");
        server_.on("/", HTTP_GET, [this]() { page(); });
        server_.on("/save", HTTP_POST, [this]() { save(); });
        server_.begin();
        active_ = true;
        Serial.printf("[setup] connect to OnStep-Remote-Setup and open http://%s/\n",
                      WiFi.softAPIP().toString().c_str());
    }
    void update() { if (active_) server_.handleClient(); }

private:
    void page(const char* notice = "") {
        String h = F("<!doctype html><meta name=viewport content='width=device-width'><title>OnStep Remote</title>"
                     "<style>body{font:18px sans-serif;max-width:32em;margin:2em auto;padding:0 1em}input{width:100%;padding:.6em;margin:.3em 0 1em;box-sizing:border-box}button{padding:.7em 1.4em}</style>"
                     "<h1>OnStep Remote network</h1>");
        h += notice;
        h += F("<form method=post action=/save><label>Wi-Fi name (SSID)<input name=s required value='");
        h += config_.ssid;
        h += F("'></label><label>Password<input name=p type=password value='");
        h += config_.password;
        h += F("'></label><label>OnStep IP<input name=i required value='");
        h += config_.mountIp.toString();
        h += F("'></label><label>OnStep port<input name=o type=number min=1 max=65535 required value='");
        h += config_.port;
        h += F("'></label><button>Save and reboot</button></form>");
        server_.send(200, "text/html", h);
    }
    void save() {
        IPAddress ip;
        long port = server_.arg("o").toInt();
        if (!ip.fromString(server_.arg("i")) || port < 1 || port > 65535 || server_.arg("s").isEmpty()) {
            page("<p><b>Invalid IP, port, or SSID.</b></p>"); return;
        }
        config_.ssid = server_.arg("s"); config_.password = server_.arg("p");
        config_.mountIp = ip; config_.port = (uint16_t)port;
        prefs_->putString("wifi_ssid", config_.ssid);
        prefs_->putString("wifi_pass", config_.password);
        prefs_->putString("mount_ip", config_.mountIp.toString());
        prefs_->putUShort("mount_port", config_.port);
        server_.send(200, "text/html", "<h1>Saved</h1><p>The controller is restarting.</p>");
        delay(500); ESP.restart();
    }
    Preferences* prefs_ = nullptr;
    NetworkConfig config_;
    WebServer server_{80};
    bool active_ = false;
};
