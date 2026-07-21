#include "ScreenModule.h"
#include "LvglDashboard.h"
#include <stdarg.h>
#include <stdio.h>

lv_obj_t* ScreenModule::panel(lv_obj_t* p, const Theme& T,
                              int x, int y, int w, int h, int radius) {
    lv_obj_t* o = lv_obj_create(p);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(o, radius, 0);
    lv_obj_set_style_bg_color(o, T.panel, 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_70, 0);
    lv_obj_set_style_border_color(o, T.edge, 0);
    lv_obj_set_style_border_width(o, 1, 0);
    lv_obj_set_style_pad_all(o, 0, 0);
    return o;
}

lv_obj_t* ScreenModule::label(lv_obj_t* p, int x, int y, int w,
                              const char* text, const lv_font_t* f,
                              lv_color_t c, lv_text_align_t a) {
    lv_obj_t* o = lv_label_create(p);
    lv_obj_set_pos(o, x, y);
    if (w > 0) lv_obj_set_width(o, w);
    lv_label_set_text(o, text);
    lv_obj_set_style_text_font(o, f, 0);
    lv_obj_set_style_text_color(o, c, 0);
    lv_obj_set_style_text_align(o, a, 0);
    return o;
}

lv_obj_t* ScreenModule::image(lv_obj_t* p, int x, int y,
                              const lv_img_dsc_t* src) {
    lv_obj_t* o = lv_img_create(p);
    lv_img_set_src(o, src);
    lv_obj_set_pos(o, x, y);
    return o;
}

lv_obj_t* ScreenModule::header(lv_obj_t* p, const Theme& T, const char* title,
                               lv_color_t accent, lv_obj_t** rightLabelOut) {
    // Full-width bar at y=22, matching the dashboard's content origin.
    lv_obj_t* bar = lv_obj_create(p);
    lv_obj_set_pos(bar, 0, 21);
    lv_obj_set_size(bar, 320, 15);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(bar, 0, 0);
    lv_obj_set_style_bg_color(bar, T.panel, 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_pad_all(bar, 0, 0);

    // Accent tab. In night mode every accent is the same red, so the tab is
    // what distinguishes one screen from another -- its WIDTH is coded per
    // screen, not its colour.
    lv_obj_t* tab = lv_obj_create(bar);
    lv_obj_set_pos(tab, 0, 0);
    lv_obj_set_size(tab, 3, 15);
    lv_obj_set_style_radius(tab, 0, 0);
    lv_obj_set_style_bg_color(tab, accent, 0);
    lv_obj_set_style_bg_opa(tab, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(tab, 0, 0);

    label(bar, 9, 2, 0, title, FONT_SMALL, accent);
    if (rightLabelOut)
        *rightLabelOut = label(bar, 180, 2, 132, "", FONT_SMALL, T.dim,
                               LV_TEXT_ALIGN_RIGHT);
    return bar;
}

void ScreenModule::setText(lv_obj_t* o, const char* fmt, ...) {
    if (!o) return;
    char buf[96];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    lv_label_set_text(o, buf);
}
