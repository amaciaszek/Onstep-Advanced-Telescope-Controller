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
private:
    static void flush(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* color);
    TFT_eSPI tft_;
    static LvglDisplayPort* self_;
    static lv_disp_draw_buf_t drawBuf_;
    static lv_color_t buf1_[320 * 20];
    static lv_color_t buf2_[320 * 20];
};
