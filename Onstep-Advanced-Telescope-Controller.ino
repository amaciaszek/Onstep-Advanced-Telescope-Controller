// OnStepRemote LVGL v5 for LilyGO T-Display-S3 (320x170).
// Install: TFT_eSPI, LVGL 8.3.x, Adafruit seesaw (optional controller).
//
// Serial console @115200 (newline): type `help`. See SerialConsole.h.
// Gamepad: START = cycle screens (HOME <-> DIAG). SELECT = emergency stop :Q#.

#include <Arduino.h>
#include <WiFi.h>
#include "LvglDisplayPort.h"
#include "LvglDashboard.h"
#include "OnStepClient.h"
#include "OnStepBackends.h"
#include "InputSeesaw.h"
#include "SerialConsole.h"
#include <Preferences.h>
#include "SessionLogger.h"
#include "NetworkConfigPortal.h"

LvglDisplayPort display;
LvglDashboard ui;
OnStepClient client;
OnStepStatusSource status(client);
HandheldDeviceInfo device;
InputSeesaw input;
SerialConsole console(client, status, input);
SessionLogger slog(client, status);
NetworkConfigPortal networkSetup;

static uint32_t lastMs = 0, lastUi = 0, lastDiag = 0, lastHeartbeat = 0;
static uint32_t lastCycleSeen = 0, lastCycleChangeMs = 0;

static void nextScreenHandler() { ui.nextScreen(); }
static void networkSetupHandler() { networkSetup.start(); }
Preferences prefs;
static void saveUiPrefs() {
  prefs.putInt("widget", ui.widget());
  prefs.putBool("night", ui.night());
}
static void nightHandler(bool on) { ui.setNight(on); saveUiPrefs(); }
static void widgetHandler(int w) { ui.setWidget(w); saveUiPrefs(); }
static void logHandler(int v) {
  if (v == -1)     slog.setEnabled(false);
  else if (v == 0) slog.probe();
  else if (v == 1) slog.setEnabled(true);
  else             slog.setSnapshotSecs((uint32_t)v);
}

void setup() {
  Serial.begin(115200);
  delay(250);
  Serial.println("\n===== OnStepRemote LVGL v5 =====");
  Serial.printf("[boot] reset reason: %d  heap=%u psram=%u\n",
                (int)esp_reset_reason(), (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getPsramSize());
  if (!display.begin()) {
    Serial.println("FATAL: display init failed (TFT_eSPI Setup206 + 320x170 expected).");
    while (true) delay(1000);
  }
  ui.begin();
  ui.setNetworkSetupHandler(networkSetupHandler);
  display.task();
  console.setScreenHandler(nextScreenHandler);
  console.setNightHandler(nightHandler);
  console.setWidgetHandler(widgetHandler);
  console.setLogHandler(logHandler);
  prefs.begin("onstepui", false);
  networkSetup.begin(prefs);
  ui.setWidget(prefs.getInt("widget", 0));
  if (prefs.getBool("night", false)) ui.setNight(true);
  Serial.printf("[boot] ui prefs: widget=%d (%s) night=%d\n",
                ui.widget(), LvglDashboard::widgetName(ui.widget()), (int)ui.night());
  if (input.begin()) Serial.printf("[boot] seesaw found at 0x50 on SDA=%d SCL=%d\n", input.sda(), input.scl());
  else               Serial.println("[boot] seesaw absent (optional)");
  const NetworkConfig& net = networkSetup.config();
  client.begin(net.ssid.c_str(), net.password.c_str(), net.mountIp, net.port);
  if (net.ssid.isEmpty()) { status.setEnabled(false); networkSetup.start(); }
  else                    client.ensureWiFi(500);
  Serial.println("[boot] ready. Type 'help'.");
  lastMs = millis();
  lastCycleChangeMs = millis();
}

void loop() {
  uint32_t now = millis();
  uint32_t elapsed = now - lastMs;
  lastMs = now;

  display.tick(elapsed);
  console.update();
  networkSetup.update();
  status.update(elapsed / 1000.0);
  device.update(elapsed / 1000.0);
  slog.update();                    // event-driven imaging-night logger

  // Gamepad: START cycles screens; SELECT is the immediate emergency stop.
  input.update();
  ButtonEvent ev;
  while (input.nextButton(ev)) {
    if (ev.button == Button::Select) {
      client.noReply(":Q#");
      Serial.println("[input] SELECT -> :Q# emergency stop sent");
    } else if (ev.button == Button::A && !ev.longPress) {
      ui.buttonA();                       // on SETTINGS: cycle radar widget
      saveUiPrefs();
    } else if (ev.button == Button::Start) {
      if (ev.longPress) { ui.setNight(!ui.night()); saveUiPrefs();
                          Serial.printf("[input] START long -> %s mode\n", ui.night()?"NIGHT":"day"); }
      else ui.nextScreen();
    }
  }

  // Settings navigation is entirely local and remains responsive with no
  // Wi-Fi or OnStep mount. A latch gives one move per stick deflection.
  {
    static bool navLatch = false;
    float y = input.joyY();
    if (fabsf(y) < 0.35f) navLatch = false;
    else if (!navLatch) { ui.settingsMove(y > 0 ? -1 : 1); navLatch = true; }
  }

  // Manual slew on HOME only. The dominant joystick axis starts one OnStep
  // jog command; returning to center or changing direction stops it first.
  {
    static SlewDir active = SlewDir::None;
    SlewDir wanted = SlewDir::None;
    float x = input.joyX(), y = input.joyY();
    if (ui.screenIndex() == 0) {
      if (fabsf(x) >= 0.35f || fabsf(y) >= 0.35f) {
        if (fabsf(x) > fabsf(y)) wanted = x > 0 ? SlewDir::East : SlewDir::West;
        else                     wanted = y > 0 ? SlewDir::North : SlewDir::South;
      }
    }
    if (wanted != active) {
      if (active != SlewDir::None) client.noReply(onstep::stopCmd(active));
      active = wanted;
      if (active != SlewDir::None) client.noReply(onstep::slewCmd(active));
      status.setSlewing(active != SlewDir::None);
    }
  }

  // Guide-burst polling: any pulse arms a 20 s window with a GU-heavy
  // rotation (~300 ms pulse sampling, coordinates still refreshing) so
  // calibration and dithers are captured with good coverage.
  {
    static uint32_t lastPulseMs = 0;
    if (status.status().guidePulseActive) lastPulseMs = now;
    status.setGuideBurst(lastPulseMs && now - lastPulseMs < 20000);
  }

  // Data staleness: track when the poller last completed a cycle.
  if (status.cycleCount() != lastCycleSeen) {
    lastCycleSeen = status.cycleCount();
    lastCycleChangeMs = now;
  }

  if (now - lastUi >= 100) {
    lastUi = now;
    UiExtras ex;
    ex.wifiBars   = device.wifiBars();
    ex.batteryPct = device.batteryPercent();
    ex.tempC      = temperatureRead();     // ESP32 internal, bring-up only
    ex.online     = status.online();
    ex.dataAgeMs  = now - lastCycleChangeMs;
    ex.rssi       = WiFi.RSSI();
    ex.latMs      = (uint16_t)client.lastMs();
    uint32_t tot  = client.okCount() + client.failCount();
    ex.okPct      = tot ? (uint8_t)((client.okCount() * 100) / tot) : 100;

    if (console.fakeActive()) { ex.online = true; ex.dataAgeMs = 0; ui.update(console.fakeStatus(), ex); }
    else                      ui.update(status.status(), ex);
  }

  // Diagnostics screen text, once per second.
  if (now - lastDiag >= 1000) {
    lastDiag = now;
    char buf[360];
    snprintf(buf, sizeof buf,
             "SSID  %s\nIP    %s -> %s:%u\nRSSI  %d dBm   wifi %s\n"
             "POLL  %lu ms   cycle %.2f s   cycles %lu\n"
             "CMDS  ok %lu   fail %lu   last %lu ms\nHEAP  %u free   up %lus",
             WiFi.SSID().c_str(), WiFi.localIP().toString().c_str(),
             "192.168.0.1", (unsigned)client.port(),
             WiFi.RSSI(), WiFi.status() == WL_CONNECTED ? "up" : "DOWN",
             (unsigned long)status.intervalMs(), status.cycleMs() / 1000.0f,
             (unsigned long)status.cycleCount(),
             (unsigned long)client.okCount(), (unsigned long)client.failCount(),
             (unsigned long)client.lastMs(),
             (unsigned)ESP.getFreeHeap(), (unsigned long)(now / 1000));
    ui.setDiagText(buf);
  }

  if (now - lastHeartbeat >= 60000) {
    lastHeartbeat = now;
    Serial.printf("[hb] up=%lus heap=%u wifi=%d mount=%d cycles=%lu\n",
                  (unsigned long)(now / 1000), (unsigned)ESP.getFreeHeap(),
                  WiFi.status() == WL_CONNECTED, (int)status.online(),
                  (unsigned long)status.cycleCount());
  }

  display.task();
  delay(3);
}
