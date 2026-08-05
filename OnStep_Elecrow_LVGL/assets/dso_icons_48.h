#pragma once
#include "lvgl.h"

extern const lv_img_dsc_t dso_gal_spiral_48;
extern const lv_img_dsc_t dso_gal_barred_48;
extern const lv_img_dsc_t dso_gal_elliptical_48;
extern const lv_img_dsc_t dso_gal_lenticular_48;
extern const lv_img_dsc_t dso_gal_irregular_48;
extern const lv_img_dsc_t dso_gal_interacting_48;
extern const lv_img_dsc_t dso_gal_cluster_48;
extern const lv_img_dsc_t dso_neb_emission_48;
extern const lv_img_dsc_t dso_neb_reflection_48;
extern const lv_img_dsc_t dso_neb_dark_48;
extern const lv_img_dsc_t dso_neb_planetary_48;
extern const lv_img_dsc_t dso_neb_supernova_48;
extern const lv_img_dsc_t dso_neb_hii_48;
extern const lv_img_dsc_t dso_neb_bright_48;
extern const lv_img_dsc_t dso_cl_open_48;
extern const lv_img_dsc_t dso_cl_globular_48;
extern const lv_img_dsc_t dso_cl_open_neb_48;
extern const lv_img_dsc_t dso_asterism_48;
extern const lv_img_dsc_t dso_star_double_48;
extern const lv_img_dsc_t dso_star_multiple_48;
extern const lv_img_dsc_t dso_star_variable_48;
extern const lv_img_dsc_t dso_quasar_48;
extern const lv_img_dsc_t dso_unknown_48;

#define DSO_COUNT 23

// Index-addressable table, so a catalog row can do
//   lv_img_set_src(img, dso_table_48[obj.type]);
extern const lv_img_dsc_t* const dso_table_48[23];
