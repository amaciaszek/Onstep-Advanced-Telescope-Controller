#pragma once
// Umbrella header — include this and you have every generated asset.
#include "lvgl.h"
#include "onstep_theme.h"
#include "dso_icons_24.h"
#include "dso_icons_48.h"
#include "ui_glyphs_16.h"
#include "ui_glyphs_24.h"

// Fonts. These are Inter, instantiated per role with the optical-size axis
// pinned, and with tabular figures baked into the cmap — Inter's default
// figures are proportional, which would make a live RA/Dec readout shuffle
// sideways every second. Verified: all ten digits are 351 units in Hero.
extern const lv_font_t onstep_label;    //  9px  tracked uppercase labels
extern const lv_font_t onstep_body;     // 12px  list rows, data tables
extern const lv_font_t onstep_strong;   // 12px  selected rows, emphasis
extern const lv_font_t onstep_value;    // 15px  alt/az, magnitudes
extern const lv_font_t onstep_head;     // 20px  object names, titles
extern const lv_font_t onstep_hero;     // 34px  RA/Dec readout

// DSO type indices — order matches dso_table_24[] / dso_table_48[].
enum onstep_dso_type {
  DSO_GAL_SPIRAL=0, DSO_GAL_BARRED, DSO_GAL_ELLIPTICAL, DSO_GAL_LENTICULAR,
  DSO_GAL_IRREGULAR, DSO_GAL_INTERACTING, DSO_GAL_CLUSTER, DSO_NEB_EMISSION,
  DSO_NEB_REFLECTION, DSO_NEB_DARK, DSO_NEB_PLANETARY, DSO_NEB_SUPERNOVA,
  DSO_NEB_HII, DSO_NEB_BRIGHT, DSO_CL_OPEN, DSO_CL_GLOBULAR, DSO_CL_OPEN_NEB,
  DSO_ASTERISM, DSO_STAR_DOUBLE, DSO_STAR_MULTIPLE, DSO_STAR_VARIABLE,
  DSO_QUASAR, DSO_UNKNOWN
};
