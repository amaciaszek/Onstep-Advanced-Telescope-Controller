#include "LvglDisplayPort.h"
#include <WiFi.h>
#include <esp_sleep.h>
#include <esp_arduino_version.h>
LvglDisplayPort* LvglDisplayPort::self_ = nullptr;
lv_disp_draw_buf_t LvglDisplayPort::drawBuf_;
lv_color_t LvglDisplayPort::buf1_[320 * 20];
lv_color_t LvglDisplayPort::buf2_[320 * 20];

bool LvglDisplayPort::begin() {
    self_ = this;
    pinMode(15, OUTPUT);
    digitalWrite(15, HIGH);
    pinMode(38, OUTPUT);
    tft_.init();
    tft_.setRotation(3);
    tft_.setSwapBytes(true);
    tft_.fillScreen(TFT_BLACK);
    if (tft_.width() != 320 || tft_.height() != 170) return false;
    lv_init();
    lv_disp_draw_buf_init(&drawBuf_, buf1_, buf2_, 320 * 20);
    static lv_disp_drv_t drv;
    lv_disp_drv_init(&drv);
    drv.hor_res = 320;
    drv.ver_res = 170;
    drv.flush_cb = flush;
    drv.draw_buf = &drawBuf_;
    lv_disp_drv_register(&drv);
    setBrightness(25);
    return true;
}

void LvglDisplayPort::setBrightness(uint8_t percent) {
    brightness_=percent>100?100:percent;
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcAttach(38,5000,8);
    ledcWrite(38,(uint32_t)brightness_*255/100);
#else
    static bool attached=false;
    if(!attached){ ledcSetup(7,5000,8); ledcAttachPin(38,7); attached=true; }
    ledcWrite(7,(uint32_t)brightness_*255/100);
#endif
}

void LvglDisplayPort::sleep() {
    setBrightness(0);
    tft_.writecommand(0x10); // ST7789 sleep-in
    delay(120);
    digitalWrite(15,LOW);
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    // Wait for the shutdown press to be released, otherwise LOW would wake
    // the ESP32 immediately. GPIO14 or RESET can then wake/restart it.
    while(digitalRead(14)==LOW) delay(10);
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_14,0);
    esp_deep_sleep_start();
}

void LvglDisplayPort::flush(lv_disp_drv_t* drv, const lv_area_t* a, lv_color_t* c) {
    const uint32_t w = a->x2 - a->x1 + 1;
    const uint32_t h = a->y2 - a->y1 + 1;
    self_->tft_.startWrite();
    self_->tft_.setAddrWindow(a->x1, a->y1, w, h);
    self_->tft_.pushColors(reinterpret_cast<uint16_t*>(c), w * h, true);
    self_->tft_.endWrite();
    lv_disp_flush_ready(drv);
}
void LvglDisplayPort::tick(uint32_t ms) { lv_tick_inc(ms); }
void LvglDisplayPort::task() { lv_timer_handler(); }
