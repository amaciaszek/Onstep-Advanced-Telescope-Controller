#pragma once
#include "lvgl.h"

extern const lv_img_dsc_t dso_gal_spiral_24;
extern const lv_img_dsc_t dso_gal_barred_24;
extern const lv_img_dsc_t dso_gal_elliptical_24;
extern const lv_img_dsc_t dso_gal_lenticular_24;
extern const lv_img_dsc_t dso_gal_irregular_24;
extern const lv_img_dsc_t dso_gal_interacting_24;
extern const lv_img_dsc_t dso_gal_cluster_24;
extern const lv_img_dsc_t dso_neb_emission_24;
extern const lv_img_dsc_t dso_neb_reflection_24;
extern const lv_img_dsc_t dso_neb_dark_24;
extern const lv_img_dsc_t dso_neb_planetary_24;
extern const lv_img_dsc_t dso_neb_supernova_24;
extern const lv_img_dsc_t dso_neb_hii_24;
extern const lv_img_dsc_t dso_neb_bright_24;
extern const lv_img_dsc_t dso_cl_open_24;
extern const lv_img_dsc_t dso_cl_globular_24;
extern const lv_img_dsc_t dso_cl_open_neb_24;
extern const lv_img_dsc_t dso_asterism_24;
extern const lv_img_dsc_t dso_star_double_24;
extern const lv_img_dsc_t dso_star_multiple_24;
extern const lv_img_dsc_t dso_star_variable_24;
extern const lv_img_dsc_t dso_quasar_24;
extern const lv_img_dsc_t dso_unknown_24;

#define DSO_COUNT 23

// Index-addressable table, so a catalog row can do
//   lv_img_set_src(img, dso_table_24[obj.type]);
extern const lv_img_dsc_t* const dso_table_24[23];
