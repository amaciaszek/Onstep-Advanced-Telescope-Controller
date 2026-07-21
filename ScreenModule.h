#pragma once
#define LV_CONF_INCLUDE_SIMPLE
#include <lvgl.h>
#include "MountStatus.h"
#include "SkyContext.h"
#include "FontCompat.h"

// House font ladder. One place to retune type scale for every v9 screen.
#define FONT_TINY  (&lv_font_montserrat_10)
#define FONT_SMALL (&lv_font_montserrat_12)
#define FONT_BODY  (&lv_font_montserrat_14)
#define FONT_VALUE (&lv_font_montserrat_18)
#define FONT_BIG   (&lv_font_montserrat_24)

struct Theme;      // defined in LvglDashboard.h
struct UiExtras;   // defined in LvglDashboard.h

// =====================================================================
//  ScreenModule -- one screen, one file.
//
//  Adding a screen is: write a subclass, add one line to the registry in
//  LvglDashboard.cpp. Nothing else in the dashboard needs to change.
//
//  Lifecycle:
//    build(root, T)   create every LVGL object once, into `root`.
//                     Called again after a theme switch (the dashboard
//                     destroys and recreates all screens on setNight()),
//                     so cache object pointers as members and do NOT
//                     assume they survive.
//    update(...)      called ~10 Hz. Set text and styles on existing
//                     objects; never create or delete here.
//    onButton/onMove  input. Return true if consumed, false to let the
//                     dashboard handle it (e.g. START to change screen).
//
//  The contract that keeps this efficient: build() allocates, update()
//  only mutates. LVGL object creation is the expensive operation and it
//  happens exactly twice per screen per session (once at boot, once if
//  the user toggles night mode).
// =====================================================================
class ScreenModule {
public:
    virtual ~ScreenModule() = default;

    // Shown in the screen-cycle indicator and the serial console.
    virtual const char* name() const = 0;

    virtual void build(lv_obj_t* root, const Theme& T) = 0;
    virtual void update(const MountStatus& m, const UiExtras& ex,
                        SkyContext& ctx) = 0;

    // Buttons are the seesaw's A/B/X/Y. START and SELECT are reserved by
    // the dashboard for screen cycling and e-stop.
    virtual bool onButton(char which, SkyContext& ctx) {
        (void)which; (void)ctx; return false;
    }
    // dx/dy are -1, 0 or +1, already debounced and auto-repeated.
    virtual bool onMove(int dx, int dy, SkyContext& ctx) {
        (void)dx; (void)dy; (void)ctx; return false;
    }

    // Footer hint strip, drawn by the dashboard so every screen matches.
    virtual const char* hint() const { return ""; }

protected:
    // ---- shared construction helpers -------------------------------------
    // These exist so screens look consistent without every screen repeating
    // fifteen lines of lv_obj_set_style_* calls.
    static lv_obj_t* panel(lv_obj_t* p, const Theme& T,
                           int x, int y, int w, int h, int radius = 6);
    static lv_obj_t* label(lv_obj_t* p, int x, int y, int w,
                           const char* text, const lv_font_t* f,
                           lv_color_t c,
                           lv_text_align_t a = LV_TEXT_ALIGN_LEFT);
    static lv_obj_t* image(lv_obj_t* p, int x, int y,
                           const lv_img_dsc_t* src);
    // Section header bar with an accent tab on the left, used by every v9
    // screen so they read as one family.
    static lv_obj_t* header(lv_obj_t* p, const Theme& T, const char* title,
                            lv_color_t accent, lv_obj_t** rightLabelOut);
    static void setText(lv_obj_t* o, const char* fmt, ...);
};
