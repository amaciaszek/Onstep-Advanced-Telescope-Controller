// OnStepRemote LVGL v5 for LilyGO T-Display-S3 (320x170).
// Install: TFT_eSPI, LVGL 8.3.x, Adafruit seesaw (optional controller).
//
// Serial console @115200 (newline): type `help`. See SerialConsole.h.
// Gamepad: START = cycle screens. SELECT = settings/activate; hold = e-stop.

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
Preferences prefs;

static uint32_t lastMs = 0, lastUi = 0, lastDiag = 0, lastHeartbeat = 0;
static uint32_t lastCycleSeen = 0, lastCycleChangeMs = 0;

static void nextScreenHandler() { ui.nextScreen(); }
static void networkSaveHandler(int slot,const char* ssid,const char* password) {
  networkSetup.saveProfile(slot,ssid,password);
}
static void profileActivateHandler(int slot) { networkSetup.activateProfile(slot); }
static void brightnessHandler(int percent) {
  display.setBrightness((uint8_t)percent);
  prefs.putInt("bright",percent);
}
static bool gotoHandler(double raHours,double decDeg,const char* designation) {
  if (!status.online() || status.status().parked) {
    Serial.printf("[goto] rejected for %s: %s\n", designation,
                  status.status().parked ? "mount parked" : "mount offline");
    return false;
  }

  long raSec=lround(fmod(raHours+24.0,24.0)*3600.0) % 86400L;
  const int rh=raSec/3600, rm=(raSec/60)%60, rs=raSec%60;
  if(decDeg>90.0) decDeg=90.0;
  if(decDeg< -90.0) decDeg=-90.0;
  const char sign=decDeg<0?'-':'+';
  long decSec=lround(fabs(decDeg)*3600.0);
  if(decSec>324000L) decSec=324000L;
  const int dd=decSec/3600, dm=(decSec/60)%60, ds=decSec%60;

  char cmd[24];
  snprintf(cmd,sizeof cmd,":Sr%02d:%02d:%02d#",rh,rm,rs);
  if(!client.numeric(cmd)) { Serial.printf("[goto] %s RA rejected\n",designation); return false; }
  snprintf(cmd,sizeof cmd,":Sd%c%02d*%02d:%02d#",sign,dd,dm,ds);
  if(!client.numeric(cmd)) { Serial.printf("[goto] %s Dec rejected\n",designation); return false; }
  const int result=client.commandDigit(":MS#",5000);
  Serial.printf("[goto] %s RA %.5fh Dec %+.4f -> :MS result %d\n",
                designation,raHours,decDeg,result);
  return result==0;
}
static void saveUiPrefs() {
  prefs.putInt("widget", ui.widget());
  prefs.putBool("night", ui.night());
  prefs.putBool("show_temp",ui.showTemperature());
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
  ui.setNetworkSaveHandler(networkSaveHandler);
  ui.setProfileActivateHandler(profileActivateHandler);
  ui.setBrightnessHandler(brightnessHandler);
  ui.setGotoHandler(gotoHandler);
  display.task();
  console.setScreenHandler(nextScreenHandler);
  console.setNightHandler(nightHandler);
  console.setWidgetHandler(widgetHandler);
  console.setLogHandler(logHandler);
  prefs.begin("onstepui", false);
  networkSetup.begin(prefs);
  String profileNames[3],profilePasswords[3];
  for(int i=0;i<3;i++){
    profileNames[i]=networkSetup.profile(i).ssid;
    profilePasswords[i]=networkSetup.profile(i).password;
  }
  ui.setNetworkProfiles(profileNames,profilePasswords,networkSetup.activeProfile());
  ui.setWidget(prefs.getInt("widget", 0));
  ui.setBrightness(prefs.getInt("bright",25));
  ui.setShowTemperature(prefs.getBool("show_temp",false));
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

  // Gamepad: START cycles screens; short SELECT opens/activates settings.
  input.update();
  ButtonEvent ev;
  while (input.nextButton(ev)) {
    if (ev.button == Button::Select) {
      if (ev.longPress) {
        client.noReply(":Q#");
        Serial.println("[input] SELECT long -> :Q# emergency stop sent");
      } else if (ui.screenIndex() >= 2) {
        ui.buttonSelect();
        saveUiPrefs();
      } else {
        ui.showSettings();
      }
    } else if (ev.button == Button::A && !ev.longPress) {
      ui.buttonA();                       // on SETTINGS: cycle radar widget
      saveUiPrefs();
    } else if (ev.button == Button::B && !ev.longPress) {
      ui.buttonB();                       // v9 screens: back / drill down
    } else if (ev.button == Button::X && !ev.longPress) {
      ui.buttonX();
    } else if (ev.button == Button::Y && !ev.longPress) {
      ui.buttonY();                       // v9 screens: sort / preset cycle
    } else if (ev.button == Button::Start) {
      if (ev.longPress) { ui.setNight(!ui.night()); saveUiPrefs();
                          Serial.printf("[input] START long -> %s mode\n", ui.night()?"NIGHT":"day"); }
      else ui.nextScreen();
    } else if (ev.button == Button::Power) {
      if (ev.longPress) {
        Serial.println("[power] deep sleep; GPIO14 or RESET wakes");
        display.sleep();
      } else ui.nextScreen();
    }
  }

  // A quick flick moves once. Holding starts a controlled repeat after
  // 450 ms, then advances every 150 ms until the stick is released.
  {
    static int heldDir=0;
    static uint32_t heldSince=0,lastRepeat=0;
    float x=input.joyX(),y=input.joyY();
    int dir=0;
    if(fabsf(x)>=0.35f||fabsf(y)>=0.35f)
      dir=fabsf(x)>fabsf(y)?(x>0?1:2):(y>0?3:4);
    auto move=[&](int d){
      if(d==1)ui.inputMove(1,0); else if(d==2)ui.inputMove(-1,0);
      else if(d==3)ui.inputMove(0,-1); else if(d==4)ui.inputMove(0,1);
    };
    if(!dir){heldDir=0;heldSince=lastRepeat=0;}
    else if(dir!=heldDir){heldDir=dir;heldSince=lastRepeat=now;move(dir);}
    else if(now-heldSince>=450&&now-lastRepeat>=150){lastRepeat=now;move(dir);}
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
