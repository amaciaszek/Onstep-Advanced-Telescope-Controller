#include "LvglDisplayPort.h"
LvglDisplayPort* LvglDisplayPort::self_ = nullptr;
lv_disp_draw_buf_t LvglDisplayPort::drawBuf_;
lv_color_t LvglDisplayPort::buf1_[320 * 20];
lv_color_t LvglDisplayPort::buf2_[320 * 20];

bool LvglDisplayPort::begin() {
    self_ = this;
    pinMode(15, OUTPUT);
    digitalWrite(15, HIGH);
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
    return true;
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
