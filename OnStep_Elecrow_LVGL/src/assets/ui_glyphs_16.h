#pragma once
#include "lvgl.h"

extern const lv_img_dsc_t ui_info_16;
extern const lv_img_dsc_t ui_center_16;
extern const lv_img_dsc_t ui_goto_16;
extern const lv_img_dsc_t ui_image_16;
extern const lv_img_dsc_t ui_notes_16;
extern const lv_img_dsc_t ui_back_16;
extern const lv_img_dsc_t ui_align_16;
extern const lv_img_dsc_t ui_guide_16;
extern const lv_img_dsc_t ui_tools_16;
extern const lv_img_dsc_t ui_wifi0_16;
extern const lv_img_dsc_t ui_wifi1_16;
extern const lv_img_dsc_t ui_wifi2_16;
extern const lv_img_dsc_t ui_wifi3_16;
extern const lv_img_dsc_t ui_link_16;
extern const lv_img_dsc_t ui_tracking_16;
extern const lv_img_dsc_t ui_battery_16;
extern const lv_img_dsc_t ui_sdcard_16;
extern const lv_img_dsc_t ui_warning_16;
extern const lv_img_dsc_t ui_park_16;
extern const lv_img_dsc_t ui_filter_16;
extern const lv_img_dsc_t ui_sort_16;
extern const lv_img_dsc_t ui_moon_16;
extern const lv_img_dsc_t ui_clock_16;
extern const lv_img_dsc_t ui_check_16;
extern const lv_img_dsc_t ui_close_16;
extern const lv_img_dsc_t ui_chev_left_16;
extern const lv_img_dsc_t ui_chev_right_16;
extern const lv_img_dsc_t ui_chev_up_16;
extern const lv_img_dsc_t ui_chev_down_16;
extern const lv_img_dsc_t ui_stick_16;
extern const lv_img_dsc_t ui_dpad_16;
extern const lv_img_dsc_t ui_btn_a_16;
extern const lv_img_dsc_t ui_btn_b_16;
extern const lv_img_dsc_t ui_btn_x_16;
extern const lv_img_dsc_t ui_btn_y_16;

#define UI_COUNT 35

// Index-addressable table, so a catalog row can do
//   lv_img_set_src(img, ui_table_16[obj.type]);
extern const lv_img_dsc_t* const ui_table_16[35];
