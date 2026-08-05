#pragma once
#include "lvgl.h"

extern const lv_img_dsc_t ui_info_24;
extern const lv_img_dsc_t ui_center_24;
extern const lv_img_dsc_t ui_goto_24;
extern const lv_img_dsc_t ui_image_24;
extern const lv_img_dsc_t ui_notes_24;
extern const lv_img_dsc_t ui_back_24;
extern const lv_img_dsc_t ui_align_24;
extern const lv_img_dsc_t ui_guide_24;
extern const lv_img_dsc_t ui_tools_24;
extern const lv_img_dsc_t ui_wifi0_24;
extern const lv_img_dsc_t ui_wifi1_24;
extern const lv_img_dsc_t ui_wifi2_24;
extern const lv_img_dsc_t ui_wifi3_24;
extern const lv_img_dsc_t ui_link_24;
extern const lv_img_dsc_t ui_tracking_24;
extern const lv_img_dsc_t ui_battery_24;
extern const lv_img_dsc_t ui_sdcard_24;
extern const lv_img_dsc_t ui_warning_24;
extern const lv_img_dsc_t ui_park_24;
extern const lv_img_dsc_t ui_filter_24;
extern const lv_img_dsc_t ui_sort_24;
extern const lv_img_dsc_t ui_moon_24;
extern const lv_img_dsc_t ui_clock_24;
extern const lv_img_dsc_t ui_check_24;
extern const lv_img_dsc_t ui_close_24;
extern const lv_img_dsc_t ui_chev_left_24;
extern const lv_img_dsc_t ui_chev_right_24;
extern const lv_img_dsc_t ui_chev_up_24;
extern const lv_img_dsc_t ui_chev_down_24;
extern const lv_img_dsc_t ui_stick_24;
extern const lv_img_dsc_t ui_dpad_24;
extern const lv_img_dsc_t ui_btn_a_24;
extern const lv_img_dsc_t ui_btn_b_24;
extern const lv_img_dsc_t ui_btn_x_24;
extern const lv_img_dsc_t ui_btn_y_24;

#define UI_COUNT 35

// Index-addressable table, so a catalog row can do
//   lv_img_set_src(img, ui_table_24[obj.type]);
extern const lv_img_dsc_t* const ui_table_24[35];
