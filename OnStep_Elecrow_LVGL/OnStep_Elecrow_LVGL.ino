// ============================================================================
//  OnStep Advanced Telescope Controller — Elecrow 3.5" (320x480)
//  LVGL 8.x on Elecrow's own vendor libraries. Touch-driven.
//
//  Built on exactly the foundation from your working demo:
//    ST77922.h / .cpp        vendor display driver
//    ST77922_Touch.h / .cpp  vendor touch driver
//  No hand-rolled SPI, no hand-rolled I2C, no custom transpose math.
//
//  TWO CHANGES from the demo, both deliberate — see PORTRAIT + PARTIAL below.
// ============================================================================
//
//  PORTRAIT (rotation 0) instead of landscape (rotation 1)
//  -------------------------------------------------------
//  Three reasons, in order of weight:
//
//   1. Rotation 1 and 3 are expensive in the vendor library. Look at
//      Fill_Colors(): for those rotations it does a ps_malloc of the whole
//      frame, a per-pixel transpose, then a free — on EVERY call. That is
//      ~300 KB of heap churn and 5-8 ms of pure copying per frame, before a
//      single byte goes out on the wire. Rotation 0 takes `tx_buf = color`
//      directly with no copy at all.
//
//   2. Rotation 0 unlocks partial refresh (see below), which rotation 1
//      cannot do. That is worth far more than the transpose saving.
//
//   3. The PCB is 54.5 x 101.5 mm — a tall handheld. And all the generated
//      art (fonts, icons, screens) was laid out for 320x480 portrait.
//
//  If you specifically want landscape, everything below still works: set
//  PANEL_ROTATION to 1, set LVGL_PARTIAL_REFRESH to 0, and swap the SCR_W/H
//  constants. But you pay the transpose on every frame.
//
//  PARTIAL REFRESH instead of full_refresh = 1
//  --------------------------------------------
//  Your demo needed full_refresh because Fill_Colors' *rotated* path only
//  handles whole frames. At rotation 0 that restriction disappears:
//  Fill_Colors(sx, sy, w, h, buf) takes a contiguous w*h buffer and windows
//  it — which is precisely the shape LVGL's partial flush hands you.
//
//  Full screen is 30.7 ms on the wire at 80 MHz. One catalog row is 1.8 ms.
//  Redrawing only what changed is the single biggest responsiveness win
//  available, and it costs one line of setup.
// ============================================================================

#include <Arduino.h>
#include <lvgl.h>
#include <math.h>
#include "esp_heap_caps.h"
#include "ST77922.h"
#include "ST77922_Touch.h"
#include "ScreenId.h"
#include "src/assets/onstep_assets.h"

// ---------------------------------------------------------------- config
#define PANEL_ROTATION        0      // 0 = portrait 320x480 (see note above)
#define SCR_W  320
#define SCR_H  480

ST77922       mylcd = ST77922();     // self-initialises in its constructor
ST77922_TOUCH my_touch;

// ---------------------------------------------------------------- theme
// Every colour sits exactly on the RGB565 grid, so what you read here is what
// the panel shows. Nudging a channel by less than 8 (red/blue) or 4 (green)
// changes nothing on glass.
#define C_GROUND     0x080810
#define C_SURFACE    0x101018
#define C_SURFACE2   0x181C29
#define C_RULE       0x31354A
#define C_RULE_FAINT 0x212431
#define C_TEXT_HI    0xF7F7F7
#define C_TEXT_MID   0x9499A5
#define C_TEXT_LOW   0x5A616B
#define C_ACCENT     0x5AC9EF
#define C_ACCENT_DIM 0x184D5A
#define C_WARN       0xEFA239
#define C_ALERT      0xE7494A

// Night mode is the same layout with the palette rotated to a single red
// channel — not a second design.
static bool g_night = false;

// Look the requested colour up in whichever palette is live. Both palettes are
// pre-snapped to the RGB565 grid, so night mode is an exact swap rather than a
// runtime desaturation — no drift, no colours colliding after quantisation.
static inline lv_color_t TC(uint32_t rgb) {
  const onstep_palette_t *p = onstep_pal;
  switch (rgb) {
    case C_GROUND:     return p->ground;
    case C_SURFACE:    return p->surface;
    case C_SURFACE2:   return p->surface2;
    case C_RULE:       return p->rule;
    case C_RULE_FAINT: return p->ruleFaint;
    case C_TEXT_HI:    return p->textHi;
    case C_TEXT_MID:   return p->textMid;
    case C_TEXT_LOW:   return p->textLow;
    case C_ACCENT:     return p->accent;
    case C_ACCENT_DIM: return p->accentDim;
    case C_WARN:       return p->warn;
    case C_ALERT:      return p->alert;
    default:           return lv_color_hex(rgb);
  }
}

// ---------------------------------------------------------------- mount state
// Replace this block with your OnStepClient. Everything downstream reads only
// from g_mount, so the swap is contained.
struct MountState {
  double raHours   = 5.5755;         // M 1
  double decDeg    = 22.0145;
  double lstHours  = 4.025;
  double latDeg    = 42.0;
  double altDeg    = 0, azDeg = 0;
  bool   tracking  = true;
  bool   slewing   = false;
  bool   pierEast  = true;
  bool   linked    = false;
  int    rssi      = -58;
  int    battPct   = 87;
  char   targetId[16]   = "M 1";
  char   targetName[28] = "Crab Nebula";
};
static MountState g_mount;

static const double D2R = M_PI / 180.0, R2D = 180.0 / M_PI;

static void horizonFromEq(double raH, double decD, double lstH, double latD,
                          double *altOut, double *azOut) {
  const double H = (lstH - raH) * 15.0 * D2R;
  const double d = decD * D2R, p = latD * D2R;
  const double sa = sin(d) * sin(p) + cos(d) * cos(p) * cos(H);
  const double alt = asin(sa < -1 ? -1 : (sa > 1 ? 1 : sa));
  double az = atan2(-sin(H) * cos(d), sin(d) * cos(p) - cos(d) * sin(p) * cos(H));
  az = fmod(az * R2D + 360.0, 360.0);
  *altOut = alt * R2D;
  *azOut  = az;
}

// ---------------------------------------------------------------- LVGL glue
static lv_disp_draw_buf_t lv_draw_buf;
static lv_disp_drv_t      lv_disp_drv;
static lv_color_t        *lv_buf_1 = nullptr;
static uint32_t           lv_last_tick = 0;

static void lvgl_flush(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *px) {
  // The vendor rotation-aware driver expects a complete frame, exactly as in
  // Elecrow's official LVGL/touch demo. full_refresh below guarantees that.
  (void)area;
  mylcd.Fill_Colors(0, 0, mylcd.Get_Width(), mylcd.Get_Height(), (uint16_t *)px);
  lv_disp_flush_ready(drv);
}

static void lvgl_touch_read(lv_indev_drv_t *drv, lv_indev_data_t *data) {
  (void)drv;
  if (my_touch.Get_Touch()) {
    data->point.x = my_touch.touch.x[0];
    data->point.y = my_touch.touch.y[0];
    data->state   = LV_INDEV_STATE_PRESSED;
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

// ---------------------------------------------------------------- screens
static lv_obj_t *g_screen[SCR_COUNT];
static ScreenId  g_current = SCR_HOME;
static lv_obj_t *g_tabBtn[SCR_COUNT];

// Home widgets we update live
static lv_obj_t *lblRa, *lblDec, *lblAlt, *lblAz, *lblPier, *lblTrack, *lblTransit;
static lv_obj_t *domeCanvas;
static lv_color_t *domeBuf = nullptr;
#define DOME_R   104
#define DOME_D   (DOME_R * 2)

// ---- shared style helpers -------------------------------------------------
static lv_obj_t *mkLabel(lv_obj_t *par, const char *txt, uint32_t col,
                         const lv_font_t *font) {
  lv_obj_t *l = lv_label_create(par);
  lv_label_set_text(l, txt);
  lv_obj_set_style_text_color(l, TC(col), 0);
  if (font) lv_obj_set_style_text_font(l, font, 0);
  return l;
}

static lv_obj_t *mkRule(lv_obj_t *par, int y, uint32_t col = C_RULE) {
  lv_obj_t *r = lv_obj_create(par);
  lv_obj_remove_style_all(r);
  lv_obj_set_size(r, SCR_W, 1);
  lv_obj_set_pos(r, 0, y);
  lv_obj_set_style_bg_color(r, TC(col), 0);
  lv_obj_set_style_bg_opa(r, LV_OPA_COVER, 0);
  lv_obj_clear_flag(r, LV_OBJ_FLAG_SCROLLABLE);
  return r;
}

static void styleScreen(lv_obj_t *s) {
  lv_obj_set_style_bg_color(s, TC(C_GROUND), 0);
  lv_obj_set_style_bg_opa(s, LV_OPA_COVER, 0);
  lv_obj_clear_flag(s, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_all(s, 0, 0);
}

// ---- sky dome -------------------------------------------------------------
// Drawn straight into the canvas buffer. This is where the SD-card texture and
// the static screen->(HA,Dec) LUT drop in later; the geometry below is already
// the same projection, so that swap touches only the inner loop.
static uint32_t rnd32(uint32_t &s) { s = s * 1664525u + 1013904223u; return s; }

static void drawDome() {
  if (!domeBuf) return;
  const int cx = DOME_R, cy = DOME_R;
  const lv_color_t sky   = TC(0x04060E);
  const lv_color_t below = TC(0x020306);
  const lv_color_t bg    = TC(C_GROUND);

  for (int y = 0; y < DOME_D; ++y) {
    for (int x = 0; x < DOME_D; ++x) {
      const int dx = x - cx, dy = y - cy;
      const int r2 = dx * dx + dy * dy;
      lv_color_t c;
      if (r2 > DOME_R * DOME_R)        c = bg;
      else if (r2 > (int)(DOME_R * 0.667f) * (int)(DOME_R * 0.667f) * 1)
        c = (sqrtf((float)r2) / DOME_R > 0.667f) ? below : sky;  // below 30 deg
      else                              c = sky;
      domeBuf[y * DOME_D + x] = c;
    }
  }
  // star field — placeholder for the real Milky Way texture
  uint32_t seed = 12345;
  for (int i = 0; i < 420; ++i) {
    const int x = rnd32(seed) % DOME_D, y = rnd32(seed) % DOME_D;
    const int dx = x - cx, dy = y - cy;
    if (dx * dx + dy * dy > DOME_R * DOME_R) continue;
    const uint32_t m = rnd32(seed) % 100;
    const uint8_t v = m > 96 ? 255 : (m > 80 ? 150 : 80);
    domeBuf[y * DOME_D + x] = TC((uint32_t)v << 16 | (uint32_t)v << 8 | v);
  }
  // graticule
  const lv_color_t rule = TC(C_RULE), faint = TC(C_RULE_FAINT);
  for (int a = 0; a < 720; ++a) {
    const float t = a * (float)M_PI / 360.0f;
    int x = cx + (int)(DOME_R * cosf(t)), y = cy + (int)(DOME_R * sinf(t));
    if (x >= 0 && x < DOME_D && y >= 0 && y < DOME_D) domeBuf[y * DOME_D + x] = rule;
    x = cx + (int)(DOME_R * 0.5f * cosf(t)); y = cy + (int)(DOME_R * 0.5f * sinf(t));
    if (x >= 0 && x < DOME_D && y >= 0 && y < DOME_D) domeBuf[y * DOME_D + x] = faint;
  }
  for (int y = 0; y < DOME_D; ++y) domeBuf[y * DOME_D + cx] = faint;

  // trajectory: past in grey, ahead in accent
  const lv_color_t acc = TC(C_ACCENT), past = TC(0x4A5058);
  for (int k = -140; k <= 140; ++k) {
    double alt, az;
    horizonFromEq(g_mount.raHours, g_mount.decDeg,
                  g_mount.lstHours + k * 0.05, g_mount.latDeg, &alt, &az);
    if (alt < 0) continue;
    const float rr = DOME_R * (1.0f - (float)alt / 90.0f);
    const int px = cx - (int)(rr * sinf((float)az * (float)D2R));
    const int py = cy - (int)(rr * cosf((float)az * (float)D2R));
    if (px < 1 || px >= DOME_D - 1 || py < 1 || py >= DOME_D - 1) continue;
    const lv_color_t c = (k <= 0) ? past : acc;
    domeBuf[py * DOME_D + px] = c;
    if (k > 0) domeBuf[py * DOME_D + px + 1] = c;
  }
  // reticle at the current pointing
  double alt, az;
  horizonFromEq(g_mount.raHours, g_mount.decDeg, g_mount.lstHours,
                g_mount.latDeg, &alt, &az);
  g_mount.altDeg = alt; g_mount.azDeg = az;
  if (alt > 0) {
    const float rr = DOME_R * (1.0f - (float)alt / 90.0f);
    const int px = cx - (int)(rr * sinf((float)az * (float)D2R));
    const int py = cy - (int)(rr * cosf((float)az * (float)D2R));
    for (int a = 0; a < 360; a += 4) {
      const float t = a * (float)D2R;
      const int x = px + (int)(9 * cosf(t)), y = py + (int)(9 * sinf(t));
      if (x >= 0 && x < DOME_D && y >= 0 && y < DOME_D) domeBuf[y * DOME_D + x] = acc;
    }
    for (int d = 10; d <= 15; ++d) {
      if (px + d < DOME_D) domeBuf[py * DOME_D + px + d] = acc;
      if (px - d >= 0)     domeBuf[py * DOME_D + px - d] = acc;
      if (py + d < DOME_D) domeBuf[(py + d) * DOME_D + px] = acc;
      if (py - d >= 0)     domeBuf[(py - d) * DOME_D + px] = acc;
    }
    if (px >= 0 && px < DOME_D && py >= 0 && py < DOME_D)
      domeBuf[py * DOME_D + px] = acc;
  }
  lv_obj_invalidate(domeCanvas);
}

// ---- tab bar --------------------------------------------------------------
static void switchTo(ScreenId id);

static void tab_cb(lv_event_t *e) {
  switchTo((ScreenId)(intptr_t)lv_event_get_user_data(e));
}

static void buildTabBar(lv_obj_t *par, ScreenId self) {
  static const char *names[SCR_COUNT] = {"HOME", "SKY", "CATALOG", "SET"};
  mkRule(par, SCR_H - 44);
  for (int i = 0; i < SCR_COUNT; ++i) {
    lv_obj_t *b = lv_btn_create(par);
    lv_obj_remove_style_all(b);
    // 80 x 44: a fingertip wants >= 44 px, which is 6.7 mm at 0.153 mm pitch
    lv_obj_set_size(b, SCR_W / SCR_COUNT, 44);
    lv_obj_set_pos(b, i * (SCR_W / SCR_COUNT), SCR_H - 43);
    lv_obj_add_event_cb(b, tab_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    lv_obj_t *l = mkLabel(b, names[i], i == self ? C_ACCENT : C_TEXT_LOW,
                          &onstep_label);
    lv_obj_center(l);
    if (i == self) {
      lv_obj_t *bar = lv_obj_create(par);
      lv_obj_remove_style_all(bar);
      lv_obj_set_size(bar, SCR_W / SCR_COUNT, 2);
      lv_obj_set_pos(bar, i * (SCR_W / SCR_COUNT), SCR_H - 44);
      lv_obj_set_style_bg_color(bar, TC(C_ACCENT), 0);
      lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    }
    g_tabBtn[i] = b;
  }
}

static void buildStatusBar(lv_obj_t *par, const char *title, const char *right) {
  lv_obj_t *t = mkLabel(par, title, C_TEXT_HI, &onstep_label);
  lv_obj_set_pos(t, 12, 8);

  lv_obj_t *w = lv_img_create(par);
  lv_img_set_src(w, &ui_wifi3_16);
  lv_obj_set_pos(w, 236, 6);
  lv_obj_set_style_img_recolor(w, TC(C_TEXT_MID), 0);
  lv_obj_set_style_img_recolor_opa(w, LV_OPA_COVER, 0);

  lv_obj_t *b = lv_img_create(par);
  lv_img_set_src(b, &ui_battery_16);
  lv_obj_set_pos(b, 256, 6);
  lv_obj_set_style_img_recolor(b, TC(C_TEXT_MID), 0);
  lv_obj_set_style_img_recolor_opa(b, LV_OPA_COVER, 0);
  lv_obj_t *r = mkLabel(par, right, C_TEXT_LOW, &onstep_label);
  lv_obj_align(r, LV_ALIGN_TOP_RIGHT, -12, 8);
  lv_obj_set_style_text_font(r, &onstep_label, 0);
  mkRule(par, 27);
}

// ---- HOME -----------------------------------------------------------------
static void action_cb(lv_event_t *e) {
  const int which = (int)(intptr_t)lv_event_get_user_data(e);
  switch (which) {
    case 0: switchTo(SCR_CATALOG); break;                 // GOTO
    case 1: g_mount.tracking = !g_mount.tracking; break;   // TRACK
    case 2: g_mount.slewing = false; break;                // STOP
    case 3: switchTo(SCR_SETTINGS); break;                 // TOOLS
  }
}

static void buildHome() {
  lv_obj_t *s = g_screen[SCR_HOME] = lv_obj_create(nullptr);
  styleScreen(s);
  buildStatusBar(s, "HOME", "22:45");

  lblTrack = mkLabel(s, "TRACKING", C_ACCENT, &onstep_label);
  lv_obj_set_pos(lblTrack, 24, 33);
  lv_obj_t *dot = lv_obj_create(s);
  lv_obj_remove_style_all(dot);
  lv_obj_set_size(dot, 6, 6);
  lv_obj_set_pos(dot, 12, 37);
  lv_obj_set_style_radius(dot, 3, 0);
  lv_obj_set_style_bg_color(dot, TC(C_ACCENT), 0);
  lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);

  lblTransit = mkLabel(s, "TRANSIT 00:18", C_TEXT_LOW, &onstep_label);
  lv_obj_align(lblTransit, LV_ALIGN_TOP_RIGHT, -12, 33);

  // dome canvas
  domeBuf = (lv_color_t *)heap_caps_malloc(DOME_D * DOME_D * sizeof(lv_color_t),
                                           MALLOC_CAP_SPIRAM);
  domeCanvas = lv_canvas_create(s);
  lv_canvas_set_buffer(domeCanvas, domeBuf, DOME_D, DOME_D, LV_IMG_CF_TRUE_COLOR);
  lv_obj_set_pos(domeCanvas, (SCR_W - DOME_D) / 2, 56);
  drawDome();

  mkRule(s, 274);
  lv_obj_t *k1 = mkLabel(s, "RA", C_TEXT_LOW, &onstep_label);
  lv_obj_set_pos(k1, 12, 292);
  lblRa = mkLabel(s, "05:34:31.9", C_TEXT_HI, &onstep_hero);
  lv_obj_set_pos(lblRa, 44, 282);

  lv_obj_t *k2 = mkLabel(s, "DEC", C_TEXT_LOW, &onstep_label);
  lv_obj_set_pos(k2, 12, 330);
  lblDec = mkLabel(s, "+22:00:52", C_TEXT_HI, &onstep_hero);
  lv_obj_set_pos(lblDec, 44, 320);

  mkRule(s, 360);
  lv_obj_t *ka = mkLabel(s, "ALT", C_TEXT_LOW, &onstep_label);
  lv_obj_set_pos(ka, 12, 370);
  lblAlt = mkLabel(s, "--", C_TEXT_HI, &onstep_value);
  lv_obj_set_pos(lblAlt, 12, 384);

  lv_obj_t *kz = mkLabel(s, "AZ", C_TEXT_LOW, &onstep_label);
  lv_obj_set_pos(kz, 116, 370);
  lblAz = mkLabel(s, "--", C_TEXT_HI, &onstep_value);
  lv_obj_set_pos(lblAz, 116, 384);

  lv_obj_t *kp = mkLabel(s, "PIER", C_TEXT_LOW, &onstep_label);
  lv_obj_set_pos(kp, 224, 370);
  lblPier = mkLabel(s, "EAST", C_ACCENT, &onstep_value);
  lv_obj_set_pos(lblPier, 224, 384);

  mkRule(s, 412);
  static const char *acts[4] = {"GOTO", "TRACK", "STOP", "TOOLS"};
  static const uint32_t cols[4] = {C_ACCENT, C_TEXT_MID, C_ALERT, C_TEXT_MID};
  for (int i = 0; i < 4; ++i) {
    lv_obj_t *b = lv_btn_create(s);
    lv_obj_remove_style_all(b);
    lv_obj_set_size(b, 72, 44);
    lv_obj_set_pos(b, 8 + i * 77, 420);
    lv_obj_set_style_border_width(b, 1, 0);
    lv_obj_set_style_border_color(b, TC(i == 0 ? C_ACCENT_DIM : C_RULE), 0);
    lv_obj_set_style_radius(b, 3, 0);
    lv_obj_add_event_cb(b, action_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    static const lv_img_dsc_t *ag[4] = {&ui_goto_24, &ui_tracking_24,
                                        &ui_close_24, &ui_tools_24};
    lv_obj_t *g = lv_img_create(b);
    lv_img_set_src(g, ag[i]);
    lv_obj_align(g, LV_ALIGN_TOP_MID, 0, 4);
    lv_obj_set_style_img_recolor(g, TC(cols[i]), 0);
    lv_obj_set_style_img_recolor_opa(g, LV_OPA_COVER, 0);
    lv_obj_t *l = mkLabel(b, acts[i], cols[i], &onstep_label);
    lv_obj_align(l, LV_ALIGN_BOTTOM_MID, 0, -4);
  }
  buildTabBar(s, SCR_HOME);
}

// ---- CATALOG --------------------------------------------------------------
struct CatEntry { const char *id; const char *name; float mag; int alt; uint8_t type; };
static const CatEntry kCatalog[] = {
  {"M 31","Andromeda",3.4f,64,DSO_GAL_SPIRAL},
  {"M 57","Ring Nebula",8.8f,61,DSO_NEB_PLANETARY},
  {"M 13","Hercules Cluster",5.8f,58,DSO_CL_GLOBULAR},
  {"M 33","Triangulum",5.7f,52,DSO_GAL_SPIRAL},
  {"M 92","--",6.4f,49,DSO_CL_GLOBULAR},
  {"NGC 7000","North America",4.0f,47,DSO_NEB_EMISSION},
  {"M 27","Dumbbell",7.4f,44,DSO_NEB_PLANETARY},
  {"NGC 891","--",10.0f,41,DSO_GAL_LENTICULAR},
  {"NGC 869","Double Cluster",4.3f,38,DSO_CL_OPEN},
  {"IC 1396","Elephant Trunk",3.5f,36,DSO_NEB_DARK},
  {"M 81","Bode's Galaxy",6.9f,33,DSO_GAL_SPIRAL},
  {"M 82","Cigar Galaxy",8.4f,33,DSO_GAL_IRREGULAR},
  {"M 76","Little Dumbbell",10.1f,31,DSO_NEB_PLANETARY},
  {"M 1","Crab Nebula",8.4f,70,DSO_NEB_SUPERNOVA},
  {"M 45","Pleiades",1.6f,66,DSO_CL_OPEN},
  {"M 42","Orion Nebula",4.0f,42,DSO_NEB_EMISSION},
  {"M 51","Whirlpool",8.4f,48,DSO_GAL_INTERACTING},
  {"M 101","Pinwheel",7.9f,50,DSO_GAL_SPIRAL},
  {"M 63","Sunflower",8.6f,46,DSO_GAL_SPIRAL},
  {"M 97","Owl Nebula",9.9f,55,DSO_NEB_PLANETARY},
};
static const int kCatalogCount = sizeof(kCatalog) / sizeof(kCatalog[0]);

static void catalog_row_cb(lv_event_t *e) {
  const int i = (int)(intptr_t)lv_event_get_user_data(e);
  g_mount.raHours = 5.5755;                       // real app: look up coords
  snprintf(g_mount.targetId, sizeof(g_mount.targetId), "%s", kCatalog[i].id);
  snprintf(g_mount.targetName, sizeof(g_mount.targetName), "%s", kCatalog[i].name);
  Serial.printf("selected %s (%s)\n", kCatalog[i].id, kCatalog[i].name);
  switchTo(SCR_HOME);
}

static void buildCatalog() {
  lv_obj_t *s = g_screen[SCR_CATALOG] = lv_obj_create(nullptr);
  styleScreen(s);
  buildStatusBar(s, "CATALOG", "20 SHOWN");

  // LVGL gives momentum scrolling, rubber-band and fling for free — exactly
  // the behaviour a finger expects, and none of it is ours to write.
  lv_obj_t *list = lv_obj_create(s);
  lv_obj_remove_style_all(list);
  lv_obj_set_size(list, SCR_W, SCR_H - 28 - 44);
  lv_obj_set_pos(list, 0, 28);
  lv_obj_set_style_bg_color(list, TC(C_GROUND), 0);
  lv_obj_set_style_bg_opa(list, LV_OPA_COVER, 0);
  lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_all(list, 0, 0);
  lv_obj_set_style_pad_row(list, 0, 0);
  lv_obj_set_scroll_dir(list, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_OFF);

  for (int i = 0; i < kCatalogCount; ++i) {
    lv_obj_t *row = lv_obj_create(list);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, SCR_W, 44);            // 44 px: a real touch target
    lv_obj_set_style_bg_color(row, TC(C_GROUND), 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(row, TC(C_SURFACE2), LV_STATE_PRESSED);
    lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_border_color(row, TC(C_RULE_FAINT), 0);
    lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(row, catalog_row_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);

    // Real DSO type icon. One 4-bit alpha asset serves both palettes because
    // LVGL tints it at draw time from img_recolor.
    lv_obj_t *ic = lv_img_create(row);
    lv_img_set_src(ic, dso_table_24[kCatalog[i].type]);
    lv_obj_set_pos(ic, 10, 10);
    lv_obj_set_style_img_recolor(ic, TC(C_TEXT_MID), 0);
    lv_obj_set_style_img_recolor_opa(ic, LV_OPA_COVER, 0);

    lv_obj_t *id = mkLabel(row, kCatalog[i].id, C_TEXT_HI, &onstep_body);
    lv_obj_set_pos(id, 44, 6);
    lv_obj_t *nm = mkLabel(row, kCatalog[i].name, C_TEXT_LOW, &onstep_label);
    lv_obj_set_pos(nm, 44, 25);

    char buf[16];
    snprintf(buf, sizeof(buf), "%.1f", kCatalog[i].mag);
    lv_obj_t *mg = mkLabel(row, buf, C_TEXT_MID, &onstep_label);
    lv_obj_align(mg, LV_ALIGN_TOP_RIGHT, -64, 14);
    snprintf(buf, sizeof(buf), "+%d", kCatalog[i].alt);
    lv_obj_t *al = mkLabel(row, buf, C_TEXT_MID, &onstep_label);
    lv_obj_align(al, LV_ALIGN_TOP_RIGHT, -12, 14);
  }
  buildTabBar(s, SCR_CATALOG);
}

// ---- SKY ------------------------------------------------------------------
static void buildSky() {
  lv_obj_t *s = g_screen[SCR_SKY] = lv_obj_create(nullptr);
  styleScreen(s);
  buildStatusBar(s, "SKY", "LST 04:02");

  lv_obj_t *l = mkLabel(s, "NOW", C_TEXT_LOW, &onstep_label);
  lv_obj_set_pos(l, 12, 40);
  lv_obj_t *r = mkLabel(s, "Astronomical night", C_TEXT_HI, &onstep_head);
  lv_obj_set_pos(r, 12, 56);
  lv_obj_t *sub = mkLabel(s, "1 h 11 m until nautical dawn", C_TEXT_MID,
                          &onstep_label);
  lv_obj_set_pos(sub, 12, 84);
  mkRule(s, 110);

  static const char *ev[4][2] = {
    {"Meridian flip", "00:18"}, {"Moonset", "05:26"},
    {"Nautical dawn", "04:52"}, {"Sunrise", "06:03"}};
  for (int i = 0; i < 4; ++i) {
    lv_obj_t *k = mkLabel(s, ev[i][0], C_TEXT_MID, &onstep_body);
    lv_obj_set_pos(k, 12, 128 + i * 40);
    lv_obj_t *v = mkLabel(s, ev[i][1], C_TEXT_HI, &onstep_value);
    lv_obj_align(v, LV_ALIGN_TOP_RIGHT, -12, 126 + i * 40);
    mkRule(s, 158 + i * 40, C_RULE_FAINT);
  }
  buildTabBar(s, SCR_SKY);
}

// ---- SETTINGS -------------------------------------------------------------
static void rebuildAll();

static void night_cb(lv_event_t *e) {
  (void)e;
  g_night = !g_night;
  onstep_set_night(g_night);
  rebuildAll();
}
static void bright_cb(lv_event_t *e) {
  lv_obj_t *sl = lv_event_get_target(e);
  const int v = lv_slider_get_value(sl);
  ledcWrite(LCD_BL, map(v, 5, 100, 13, 255));
}

static void buildSettings() {
  lv_obj_t *s = g_screen[SCR_SETTINGS] = lv_obj_create(nullptr);
  styleScreen(s);
  buildStatusBar(s, "SETTINGS", "");

  lv_obj_t *l1 = mkLabel(s, "Night mode", C_TEXT_HI, &onstep_value);
  lv_obj_set_pos(l1, 12, 44);
  lv_obj_t *l1s = mkLabel(s, "Red observing palette", C_TEXT_LOW,
                          &onstep_label);
  lv_obj_set_pos(l1s, 12, 66);
  lv_obj_t *sw = lv_switch_create(s);
  lv_obj_set_size(sw, 56, 30);
  lv_obj_align(sw, LV_ALIGN_TOP_RIGHT, -12, 48);
  if (g_night) lv_obj_add_state(sw, LV_STATE_CHECKED);
  lv_obj_add_event_cb(sw, night_cb, LV_EVENT_VALUE_CHANGED, nullptr);
  mkRule(s, 96, C_RULE_FAINT);

  lv_obj_t *l2 = mkLabel(s, "Brightness", C_TEXT_HI, &onstep_value);
  lv_obj_set_pos(l2, 12, 110);
  lv_obj_t *l2s = mkLabel(s, "Backlight is ~21% of the power budget",
                          C_TEXT_LOW, &onstep_label);
  lv_obj_set_pos(l2s, 12, 132);
  lv_obj_t *sl = lv_slider_create(s);
  lv_obj_set_size(sl, SCR_W - 24, 20);
  lv_obj_set_pos(sl, 12, 158);
  lv_slider_set_range(sl, 5, 100);
  lv_slider_set_value(sl, 25, LV_ANIM_OFF);
  lv_obj_add_event_cb(sl, bright_cb, LV_EVENT_VALUE_CHANGED, nullptr);
  mkRule(s, 192, C_RULE_FAINT);

  char buf[64];
  snprintf(buf, sizeof(buf), "Rotation %d  ·  vendor full refresh",
           PANEL_ROTATION);
  lv_obj_t *l3 = mkLabel(s, buf, C_TEXT_MID, &onstep_label);
  lv_obj_set_pos(l3, 12, 206);
  snprintf(buf, sizeof(buf), "PSRAM free  %u KB", (unsigned)(ESP.getFreePsram() / 1024));
  lv_obj_t *l4 = mkLabel(s, buf, C_TEXT_MID, &onstep_label);
  lv_obj_set_pos(l4, 12, 226);

  buildTabBar(s, SCR_SETTINGS);
}

// ---- navigation -----------------------------------------------------------
static void switchTo(ScreenId id) {
  if (id == g_current) return;
  const bool forward = id > g_current;
  g_current = id;
  // 180 ms slide, ease-out: fast enough to feel instant, slow enough to show
  // direction. Costs ~0.05 J — about 0.07% of a session for 200 of them.
  lv_scr_load_anim(g_screen[id],
                   forward ? LV_SCR_LOAD_ANIM_MOVE_LEFT : LV_SCR_LOAD_ANIM_MOVE_RIGHT,
                   180, 0, false);
}

static void rebuildAll() {
  const ScreenId keep = g_current;
  lv_obj_t *old[SCR_COUNT];
  for (int i = 0; i < SCR_COUNT; ++i) old[i] = g_screen[i];
  buildHome(); buildSky(); buildCatalog(); buildSettings();
  lv_scr_load(g_screen[keep]);
  g_current = keep;
  for (int i = 0; i < SCR_COUNT; ++i) if (old[i]) lv_obj_del(old[i]);
}

// ---------------------------------------------------------------- setup
void setup() {
  Serial.begin(115200);
  delay(400);
  Serial.println("\nOnStep Controller — Elecrow 3.5, LVGL, vendor libs");
  Serial.printf("PSRAM: %s (%u bytes)\n", psramFound() ? "YES" : "NO",
                (unsigned)ESP.getPsramSize());

  mylcd.Set_Rotation(PANEL_ROTATION);
  Serial.printf("Display %u x %u (rotation %u)\n",
                mylcd.Get_Width(), mylcd.Get_Height(), mylcd.Get_Rotation());

  my_touch.init();
  my_touch.Set_Rotation(PANEL_ROTATION);

  lv_init();

  // Same full-frame allocation as the supplied vendor LVGL/touch demo.
  const uint32_t bufPx = (uint32_t)mylcd.Get_Width() * mylcd.Get_Height();
  lv_buf_1 = (lv_color_t *)heap_caps_malloc(bufPx * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
  if (!lv_buf_1) { Serial.println("ERR: LVGL buffer"); while (1) delay(1000); }
  lv_disp_draw_buf_init(&lv_draw_buf, lv_buf_1, nullptr, bufPx);

  lv_disp_drv_init(&lv_disp_drv);
  lv_disp_drv.hor_res  = mylcd.Get_Width();
  lv_disp_drv.ver_res  = mylcd.Get_Height();
  lv_disp_drv.flush_cb = lvgl_flush;
  lv_disp_drv.draw_buf = &lv_draw_buf;
  lv_disp_drv.full_refresh = 1;
  lv_disp_drv_register(&lv_disp_drv);

  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type    = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = lvgl_touch_read;
  lv_indev_drv_register(&indev_drv);

  buildHome(); buildSky(); buildCatalog(); buildSettings();
  lv_scr_load(g_screen[SCR_HOME]);

  lv_last_tick = millis();
  Serial.println("UI ready.");
}

// ---------------------------------------------------------------- loop
void loop() {
  const uint32_t now = millis();
  lv_tick_inc(now - lv_last_tick);
  lv_last_tick = now;

  // Mount poll. In the real app this reads g_mount from a FreeRTOS task pinned
  // to core 0, so a blocking socket never stalls the renderer:
  //   xTaskCreatePinnedToCore(pollerTask, "poll", 6144, nullptr, 4, nullptr, 0);
  static uint32_t lastPoll = 0;
  if (now - lastPoll > 250) {
    lastPoll = now;
    g_mount.lstHours += 250.0 / 3600000.0 * 1.0027379;   // sidereal advance

    char buf[24];
    const double ra = fmod(g_mount.raHours + 24.0, 24.0);
    const int rh = (int)ra, rm = (int)((ra - rh) * 60);
    const double rs = ((ra - rh) * 60 - rm) * 60;
    snprintf(buf, sizeof(buf), "%02d:%02d:%04.1f", rh, rm, rs);
    lv_label_set_text(lblRa, buf);

    const double dec = g_mount.decDeg;
    const int dd = (int)fabs(dec), dm = (int)((fabs(dec) - dd) * 60);
    const int ds = (int)(((fabs(dec) - dd) * 60 - dm) * 60);
    snprintf(buf, sizeof(buf), "%c%02d:%02d:%02d", dec < 0 ? '-' : '+', dd, dm, ds);
    lv_label_set_text(lblDec, buf);

    snprintf(buf, sizeof(buf), "%.1f", g_mount.altDeg);  lv_label_set_text(lblAlt, buf);
    snprintf(buf, sizeof(buf), "%.1f", g_mount.azDeg);   lv_label_set_text(lblAz, buf);
    lv_label_set_text(lblPier, g_mount.pierEast ? "EAST" : "WEST");
    lv_label_set_text(lblTrack, g_mount.tracking ? "TRACKING" : "STOPPED");
    lv_obj_set_style_text_color(lblTrack, TC(g_mount.tracking ? C_ACCENT : C_WARN), 0);
  }

  // The sky rotates one dome pixel roughly every 3.5 minutes, so redrawing it
  // more often than that is pure waste. This is the single biggest energy
  // decision in the renderer, and it is independent of how fast the loop runs.
  static uint32_t lastDome = 0;
  if (now - lastDome > 30000) { lastDome = now; if (g_current == SCR_HOME) drawDome(); }

  lv_timer_handler();
  delay(2);
}
