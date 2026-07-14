#pragma once
// Font resilience: if the LVGL library was built without our lv_conf.h
// (only montserrat_14 enabled by default), alias the missing sizes to the
// nearest enabled one so the project still compiles and runs everywhere.
// For the intended look, copy this project's lv_conf.h to your Arduino
// libraries folder (as a SIBLING of the lvgl library folder):
//   Documents/Arduino/libraries/lv_conf.h
// then delete the app and rebuild (Arduino caches library objects).
#define LV_CONF_INCLUDE_SIMPLE
#include <lvgl.h>

#if !LV_FONT_MONTSERRAT_14
#error "montserrat_14 must be enabled (it is LVGL's default; check lv_conf.h)"
#endif

#if !LV_FONT_MONTSERRAT_10
#define lv_font_montserrat_10 lv_font_montserrat_14
#endif
#if !LV_FONT_MONTSERRAT_12
#define lv_font_montserrat_12 lv_font_montserrat_14
#endif
#if !LV_FONT_MONTSERRAT_16
#define lv_font_montserrat_16 lv_font_montserrat_14
#endif
#if !LV_FONT_MONTSERRAT_18
#define lv_font_montserrat_18 lv_font_montserrat_14
#endif
#if !LV_FONT_MONTSERRAT_20
#define lv_font_montserrat_20 lv_font_montserrat_14
#endif
#if !LV_FONT_MONTSERRAT_22
#define lv_font_montserrat_22 lv_font_montserrat_14
#endif
#if !LV_FONT_MONTSERRAT_24
#define lv_font_montserrat_24 lv_font_montserrat_14
#endif
