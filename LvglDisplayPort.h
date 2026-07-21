#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>
#define LV_CONF_INCLUDE_SIMPLE
#include <lvgl.h>

class LvglDisplayPort {
public:
    bool begin();
    void tick(uint32_t elapsedMs);
    void task();
    void setBrightness(uint8_t percent);
    void sleep();
private:
    static void flush(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* color);
    TFT_eSPI tft_;
    uint8_t brightness_=100;
    static LvglDisplayPort* self_;
    static lv_disp_draw_buf_t drawBuf_;
    static lv_color_t buf1_[320 * 20];
    static lv_color_t buf2_[320 * 20];
};
