#include "ScreenCatalog.h"
#include "LvglDashboard.h"
#include <stdio.h>
#include <math.h>

static const char* kGroupLabel[6] = {
    "GALAXY", "PLANETARY", "GLOBULAR", "NEBULA", "CLUSTER", "REMNANT"
};

// Map an ObjType to the glyph slot (which is indexed by TypeGroup).
static int glyphSlot(const catalog::Record& r) {
    return int(catalog::groupOf(r));
}

const char* ScreenCatalog::hint() const {
    switch (state_) {
        case FILTER: return "A TOGGLE   Y PRESET   B LIST";
        case LIST:   return "A DETAIL   Y SORT   B FILTER";
        default:
            if (gotoConfirm_) return "A CONFIRM GOTO   B CANCEL";
            if (gotoResult_ > 0) return "GOTO SENT   B BACK";
            if (gotoResult_ < 0) return "GOTO FAILED / OFFLINE   B BACK";
            return "A GOTO   B BACK";
    }
}

// =====================================================================
//  build
// =====================================================================
void ScreenCatalog::build(lv_obj_t* root, const Theme& T) {
    T_ = &T;
    seenGeneration_ = 0xFFFFFFFF;
    header(root, T, "CATALOG", T.accent, &hdrRight_);
    buildFilter(root, T);
    buildList(root, T);
    buildDetail(root, T);
    showState();
}

void ScreenCatalog::buildFilter(lv_obj_t* root, const Theme& T) {
    filterRoot_ = lv_obj_create(root);
    lv_obj_set_pos(filterRoot_, 0, 37);
    lv_obj_set_size(filterRoot_, 320, 96);
    lv_obj_clear_flag(filterRoot_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(filterRoot_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(filterRoot_, 0, 0);
    lv_obj_set_style_pad_all(filterRoot_, 0, 0);

    // ---- compass rose ----------------------------------------------------
    // Radius encodes altitude: horizon at the rim, zenith at the centre.
    const int cx = 52, cy = 46, rOuter = 44;
    for (int s = 0; s < kSectors; ++s) {
        lv_obj_t* a = lv_arc_create(filterRoot_);
        lv_obj_set_size(a, rOuter * 2, rOuter * 2);
        lv_obj_set_pos(a, cx - rOuter, cy - rOuter);
        lv_obj_remove_style(a, nullptr, LV_PART_KNOB);
        lv_obj_clear_flag(a, LV_OBJ_FLAG_CLICKABLE);
        lv_arc_set_bg_angles(a, 0, 360);
        // LVGL angles run clockwise from 3 o'clock; compass azimuth runs
        // clockwise from 12 o'clock. Hence the -90 offset.
        lv_arc_set_angles(a, s * 45 - 90 + 1, (s + 1) * 45 - 90 - 1);
        lv_obj_set_style_arc_width(a, 18, LV_PART_INDICATOR);
        lv_obj_set_style_arc_opa(a, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_arc_color(a, T.sweep2, LV_PART_INDICATOR);
        wedge_[s] = a;
    }
    floorRing_ = lv_arc_create(filterRoot_);
    lv_obj_set_size(floorRing_, 1, 1);
    lv_obj_remove_style(floorRing_, nullptr, LV_PART_KNOB);
    lv_obj_clear_flag(floorRing_, LV_OBJ_FLAG_CLICKABLE);
    lv_arc_set_bg_angles(floorRing_, 0, 360);
    lv_arc_set_angles(floorRing_, 0, 360);
    lv_obj_set_style_arc_width(floorRing_, 2, LV_PART_INDICATOR);
    lv_obj_set_style_arc_opa(floorRing_, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_arc_color(floorRing_, T.accent, LV_PART_INDICATOR);

    // Zenith exemption cap: the always-included region at the middle. This
    // is the visual argument for why a northern object at 88 degrees still
    // appears when only the western sectors are selected.
    zenithCap_ = lv_obj_create(filterRoot_);
    lv_obj_clear_flag(zenithCap_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(zenithCap_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(zenithCap_, T.good, 0);
    lv_obj_set_style_bg_opa(zenithCap_, LV_OPA_30, 0);
    lv_obj_set_style_border_color(zenithCap_, T.good, 0);
    lv_obj_set_style_border_width(zenithCap_, 1, 0);
    lv_obj_set_style_pad_all(zenithCap_, 0, 0);

    label(filterRoot_, cx - 6, cy - rOuter - 12, 12, "N", FONT_TINY, T.text,
          LV_TEXT_ALIGN_CENTER);
    label(filterRoot_, cx - 6, cy + rOuter + 2, 12, "S", FONT_TINY, T.text,
          LV_TEXT_ALIGN_CENTER);
    label(filterRoot_, cx + rOuter + 3, cy - 5, 10, "E", FONT_TINY, T.text);
    label(filterRoot_, cx - rOuter - 12, cy - 5, 10, "W", FONT_TINY, T.text,
          LV_TEXT_ALIGN_RIGHT);

    // ---- type chips ------------------------------------------------------
    const int x0 = 116, y0 = 2, cw = 96, ch = 17, gap = 3;
    for (int i = 0; i < 6; ++i) {
        const int col = i % 2, row = i / 2;
        lv_obj_t* c = lv_obj_create(filterRoot_);
        lv_obj_set_pos(c, x0 + col * (cw + gap), y0 + row * (ch + gap));
        lv_obj_set_size(c, cw, ch);
        lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_radius(c, 3, 0);
        lv_obj_set_style_bg_color(c, T.panel, 0);
        lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(c, T.edge, 0);
        lv_obj_set_style_border_width(c, 1, 0);
        lv_obj_set_style_pad_all(c, 0, 0);
        chip_[i] = c;
        chipIcon_[i] = image(c, 1, 0, T.objGlyph[i][0]);
        chipLbl_[i] = label(c, 20, 2, cw - 23, kGroupLabel[i], FONT_TINY,
                            T.dim);
    }

    // ---- live count ------------------------------------------------------
    lv_obj_t* cp = panel(filterRoot_, T, x0, y0 + 3 * (ch + gap) + 1,
                         cw * 2 + gap, 32);
    countBig_ = label(cp, 8, 3, 110, "0", FONT_BIG, T.accent);
    countSub_ = label(cp, 8, 20, 110, "OBJECTS", FONT_TINY, T.dim);
    floorLbl_ = label(cp, 110, 4, 80, "ALT 25", FONT_TINY, T.text,
                      LV_TEXT_ALIGN_RIGHT);
    zenithLbl_ = label(cp, 110, 18, 80, "ZEN 70", FONT_TINY, T.good,
                       LV_TEXT_ALIGN_RIGHT);
}

void ScreenCatalog::buildList(lv_obj_t* root, const Theme& T) {
    listRoot_ = lv_obj_create(root);
    lv_obj_set_pos(listRoot_, 0, 37);
    lv_obj_set_size(listRoot_, 320, 96);
    lv_obj_clear_flag(listRoot_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(listRoot_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(listRoot_, 0, 0);
    lv_obj_set_style_pad_all(listRoot_, 0, 0);

    for (int r = 0; r < kRows; ++r) {
        lv_obj_t* o = lv_obj_create(listRoot_);
        lv_obj_set_pos(o, 4, r * 19);
        lv_obj_set_size(o, 308, 18);
        lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_radius(o, 2, 0);
        lv_obj_set_style_bg_color(o, T.panel, 0);
        lv_obj_set_style_bg_opa(o, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_color(o, T.accent, 0);
        lv_obj_set_style_border_width(o, 0, 0);
        lv_obj_set_style_border_side(o, LV_BORDER_SIDE_LEFT, 0);
        lv_obj_set_style_pad_all(o, 0, 0);
        row_[r] = o;
        rowIcon_[r]  = image(o, 3, 0, T.objGlyph[0][0]);
        rowDesig_[r] = label(o, 24, 2, 74, "", FONT_SMALL, T.text);
        rowName_[r]  = label(o, 100, 3, 116, "", FONT_TINY, T.dim);
        rowMag_[r]   = label(o, 210, 3, 40, "", FONT_TINY, T.text,
                             LV_TEXT_ALIGN_RIGHT);
        rowSize_[r]  = label(o, 252, 3, 44, "", FONT_TINY, T.dim,
                             LV_TEXT_ALIGN_RIGHT);
        // Altitude as a short vertical bar: reads instantly and costs one
        // object instead of an arc per row.
        lv_obj_t* b = lv_obj_create(o);
        lv_obj_set_pos(b, 300, 2);
        lv_obj_set_size(b, 4, 14);
        lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_radius(b, 1, 0);
        lv_obj_set_style_border_width(b, 0, 0);
        lv_obj_set_style_pad_all(b, 0, 0);
        lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(b, T.good, 0);
        rowAlt_[r] = b;
    }

    scrollThumb_ = lv_obj_create(listRoot_);
    lv_obj_set_size(scrollThumb_, 2, 20);
    lv_obj_set_pos(scrollThumb_, 316, 0);
    lv_obj_clear_flag(scrollThumb_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(scrollThumb_, 1, 0);
    lv_obj_set_style_border_width(scrollThumb_, 0, 0);
    lv_obj_set_style_bg_opa(scrollThumb_, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(scrollThumb_, T.accent, 0);

    emptyLbl_ = label(listRoot_, 0, 34, 320, "NO OBJECTS IN THIS WINDOW",
                      FONT_BODY, T.warn, LV_TEXT_ALIGN_CENTER);
    lv_obj_add_flag(emptyLbl_, LV_OBJ_FLAG_HIDDEN);
}

void ScreenCatalog::buildDetail(lv_obj_t* root, const Theme& T) {
    detailRoot_ = lv_obj_create(root);
    lv_obj_set_pos(detailRoot_, 0, 37);
    lv_obj_set_size(detailRoot_, 320, 96);
    lv_obj_clear_flag(detailRoot_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(detailRoot_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(detailRoot_, 0, 0);
    lv_obj_set_style_pad_all(detailRoot_, 0, 0);

    // ---- framing widget --------------------------------------------------
    // The sensor field, with the object drawn to scale inside it and rotated
    // to its position angle. lv_line takes an arbitrary polygon, which is
    // the only native LVGL primitive that can express a rotated ellipse.
    fovBox_ = lv_obj_create(detailRoot_);
    lv_obj_set_pos(fovBox_, 10, 2);
    lv_obj_set_size(fovBox_, 104, 76);
    lv_obj_clear_flag(fovBox_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(fovBox_, 0, 0);
    lv_obj_set_style_bg_color(fovBox_, T.panel, 0);
    lv_obj_set_style_bg_opa(fovBox_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(fovBox_, T.edge, 0);
    lv_obj_set_style_border_width(fovBox_, 1, 0);
    lv_obj_set_style_pad_all(fovBox_, 0, 0);

    fovLine_ = lv_line_create(fovBox_);
    lv_obj_set_style_line_color(fovLine_, T.accent, 0);
    lv_obj_set_style_line_width(fovLine_, 2, 0);
    lv_obj_set_style_line_rounded(fovLine_, true, 0);

    fovDot_ = lv_obj_create(fovBox_);
    lv_obj_set_size(fovDot_, 3, 3);
    lv_obj_set_pos(fovDot_, 50, 36);
    lv_obj_clear_flag(fovDot_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(fovDot_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(fovDot_, 0, 0);
    lv_obj_set_style_bg_opa(fovDot_, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(fovDot_, T.accent, 0);
    lv_obj_add_flag(fovDot_, LV_OBJ_FLAG_HIDDEN);

    fovNote_ = label(detailRoot_, 10, 80, 104, "", FONT_TINY, T.dim);

    // ---- data panel ------------------------------------------------------
    lv_obj_t* dp = panel(detailRoot_, T, 118, 2, 192, 76);
    dIcon_ = image(dp, 5, 3, T.objGlyph[0][1]);
    dTitle_ = label(dp, 26, 3, 120, "--", FONT_BODY, T.text);
    dType_ = label(dp, 26, 18, 160, "", FONT_TINY, T.dim);

    static const char* fl[6] = {"SIZE", "MAG", "ALT", "AIRMASS",
                                "SURF BR", "TRANSIT"};
    for (int i = 0; i < 6; ++i) {
        const int col = i % 2, row = i / 2;
        const int fx = 6 + col * 94, fy = 32 + row * 15;
        dLbl_[i] = label(dp, fx, fy, 44, fl[i], FONT_TINY, T.dim);
        dVal_[i] = label(dp, fx + 42, fy, 48, "--", FONT_TINY, T.text);
    }

    lv_obj_t* fp = panel(detailRoot_, T, 10, 80, 300, 16, 3);
    dMax_ = label(fp, 5, 2, 140, "", FONT_TINY, T.dim);
    dMoon_ = label(fp, 150, 2, 145, "", FONT_TINY, T.dim,
                   LV_TEXT_ALIGN_RIGHT);
    lv_obj_set_pos(fovNote_, 10, 80);
    lv_obj_add_flag(fovNote_, LV_OBJ_FLAG_HIDDEN);
}

void ScreenCatalog::showState() {
    lv_obj_add_flag(filterRoot_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(listRoot_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(detailRoot_, LV_OBJ_FLAG_HIDDEN);
    switch (state_) {
        case FILTER: lv_obj_clear_flag(filterRoot_, LV_OBJ_FLAG_HIDDEN); break;
        case LIST:   lv_obj_clear_flag(listRoot_, LV_OBJ_FLAG_HIDDEN); break;
        default:     lv_obj_clear_flag(detailRoot_, LV_OBJ_FLAG_HIDDEN); break;
    }
}

// =====================================================================
//  update
// =====================================================================
void ScreenCatalog::updateFilter(SkyContext& ctx) {
    const catalog::Filter& f = ctx.settings.filter;
    const int cx = 52, cy = 46, rOuter = 44;

    // Radius that corresponds to an altitude, with the horizon at the rim.
    auto radiusOfAlt = [&](float alt) {
        float r = rOuter * (1.0f - alt / 90.0f);
        if (r < 3) r = 3;
        if (r > rOuter) r = rOuter;
        return int(r);
    };
    const int rFloor = radiusOfAlt(f.altFloor);

    for (int s = 0; s < kSectors; ++s) {
        const bool on = f.sector(s);
        const bool focus = (filterFocus_ == s);
        // Wedge thickness spans from the floor ring out to the rim.
        const int width = rOuter - rFloor;
        lv_obj_set_style_arc_width(wedge_[s], width > 2 ? width : 2,
                                   LV_PART_INDICATOR);
        lv_obj_set_size(wedge_[s], (rOuter + rFloor), (rOuter + rFloor));
        lv_obj_set_pos(wedge_[s], cx - (rOuter + rFloor) / 2,
                       cy - (rOuter + rFloor) / 2);
        lv_obj_set_style_arc_color(wedge_[s],
            on ? (focus ? T_->accent : T_->sweep1)
               : (focus ? T_->dim : T_->sweep3), LV_PART_INDICATOR);
        lv_obj_set_style_arc_opa(wedge_[s],
            on ? LV_OPA_COVER : LV_OPA_40, LV_PART_INDICATOR);
    }

    lv_obj_set_size(floorRing_, rFloor * 2, rFloor * 2);
    lv_obj_set_pos(floorRing_, cx - rFloor, cy - rFloor);
    lv_obj_set_style_arc_width(floorRing_, filterFocus_ == 8 ? 3 : 2,
                               LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(floorRing_,
        filterFocus_ == 8 ? T_->text : T_->accent, LV_PART_INDICATOR);

    if (f.zenithExempt <= 90.0f) {
        const int rz = radiusOfAlt(f.zenithExempt);
        lv_obj_clear_flag(zenithCap_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_size(zenithCap_, rz * 2, rz * 2);
        lv_obj_set_pos(zenithCap_, cx - rz, cy - rz);
    } else {
        lv_obj_add_flag(zenithCap_, LV_OBJ_FLAG_HIDDEN);
    }

    for (int i = 0; i < 6; ++i) {
        const bool on = f.group(catalog::TypeGroup(i));
        const bool focus = (filterFocus_ == 9 + i);
        lv_img_set_src(chipIcon_[i], T_->objGlyph[i][on ? 1 : 0]);
        lv_obj_set_style_img_opa(chipIcon_[i],
            on ? LV_OPA_COVER : LV_OPA_50, 0);
        lv_obj_set_style_bg_opa(chip_[i], on ? LV_OPA_COVER : LV_OPA_40, 0);
        lv_obj_set_style_border_color(chip_[i],
            focus ? T_->text : (on ? T_->accent : T_->edge), 0);
        lv_obj_set_style_border_width(chip_[i], focus ? 2 : 1, 0);
        lv_obj_set_style_text_color(chipLbl_[i], on ? T_->text : T_->dim, 0);
    }

    setText(countBig_, "%u", unsigned(ctx.view.count()));
    setText(countSub_, "OF %u SELECTED", unsigned(ctx.view.typeMatchCount()));
    setText(floorLbl_, "ALT %.0f", f.altFloor);
    if (f.zenithExempt > 90.0f) lv_label_set_text(zenithLbl_, "NO ZENITH");
    else setText(zenithLbl_, "ZEN %.0f", f.zenithExempt);
}

void ScreenCatalog::updateList(SkyContext& ctx) {
    const uint16_t n = ctx.view.count();
    if (!n) {
        lv_obj_clear_flag(emptyLbl_, LV_OBJ_FLAG_HIDDEN);
        for (int r = 0; r < kRows; ++r)
            lv_obj_add_flag(row_[r], LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(scrollThumb_, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_obj_add_flag(emptyLbl_, LV_OBJ_FLAG_HIDDEN);

    if (listSel_ >= n) listSel_ = n - 1;
    if (listSel_ < listTop_) listTop_ = listSel_;
    if (listSel_ >= listTop_ + kRows)
        listTop_ = uint16_t(listSel_ - kRows + 1);

    for (int r = 0; r < kRows; ++r) {
        const uint16_t i = uint16_t(listTop_ + r);
        if (i >= n) { lv_obj_add_flag(row_[r], LV_OBJ_FLAG_HIDDEN); continue; }
        lv_obj_clear_flag(row_[r], LV_OBJ_FLAG_HIDDEN);

        const catalog::Record& rec = ctx.view.at(i);
        const catalog::Sky sk = ctx.view.sky(i);
        const bool sel = (i == listSel_);

        // Selection is an accent bar plus a filled glyph, not a colour
        // change: in night mode the fill IS the signal.
        lv_obj_set_style_bg_opa(row_[r], sel ? LV_OPA_60 : LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(row_[r], sel ? 3 : 0, 0);
        lv_img_set_src(rowIcon_[r], T_->objGlyph[glyphSlot(rec)][sel ? 1 : 0]);

        lv_label_set_text(rowDesig_[r], catalog::designation(rec));
        lv_obj_set_style_text_color(rowDesig_[r], sel ? T_->text : T_->dim, 0);

        const char* pop = catalog::popularName(rec);
        lv_label_set_text(rowName_[r],
                          *pop ? pop : catalog::constellation(rec));
        lv_obj_set_style_text_color(rowName_[r],
                                    sel ? T_->accent : T_->dim, 0);

        if (catalog::hasMag(rec))
            setText(rowMag_[r], "%.1f%s", catalog::magnitude(rec),
                    catalog::magIsBlue(rec) ? "B" : "");
        else
            lv_label_set_text(rowMag_[r], "--");

        const double sz = catalog::majorArcmin(rec);
        if (sz >= 60.0) setText(rowSize_[r], "%.1f\xC2\xB0", sz / 60.0);
        else            setText(rowSize_[r], "%.1f'", sz);

        int h = int(14.0 * (sk.altDeg / 90.0));
        if (h < 1) h = 1;
        if (h > 14) h = 14;
        lv_obj_set_size(rowAlt_[r], 4, h);
        lv_obj_set_pos(rowAlt_[r], 300, 2 + (14 - h));
        lv_obj_set_style_bg_color(rowAlt_[r],
            sk.altDeg < 20 ? T_->alert : (sk.altDeg < 35 ? T_->warn : T_->good),
            0);
    }

    if (n > kRows) {
        lv_obj_clear_flag(scrollThumb_, LV_OBJ_FLAG_HIDDEN);
        const int trackH = kRows * 19 - 2;
        int th = trackH * kRows / n;
        if (th < 6) th = 6;
        const int ty = int((trackH - th) *
                       (double(listTop_) / double(n - kRows)));
        lv_obj_set_size(scrollThumb_, 2, th);
        lv_obj_set_pos(scrollThumb_, 316, ty);
    } else {
        lv_obj_add_flag(scrollThumb_, LV_OBJ_FLAG_HIDDEN);
    }
}

// Build a 24-point rotated ellipse for the framing widget.
void ScreenCatalog::setEllipse(const catalog::Record& r, double fovArcmin) {
    const int bw = 104, bh = 76;
    const int cx = bw / 2, cy = bh / 2;
    if (fovArcmin < 0.5) fovArcmin = 0.5;

    // The SHORT side of the box represents the configured field, so an
    // object exactly filling the field touches top and bottom.
    const double pxPerArcmin = double(bh) / fovArcmin;
    double rx = 0.5 * catalog::majorArcmin(r) * pxPerArcmin;
    double ry = 0.5 * catalog::minorArcmin(r) * pxPerArcmin;
    if (ry < 0.1) ry = rx;

    if (rx < 1.5 && ry < 1.5) {
        lv_obj_add_flag(fovLine_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(fovDot_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_pos(fovDot_, cx - 1, cy - 1);
        lv_obj_clear_flag(fovNote_, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(fovNote_, "SMALLER THAN 1 PX");
        return;
    }
    lv_obj_add_flag(fovDot_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(fovLine_, LV_OBJ_FLAG_HIDDEN);

    // Sky position angle runs from north through east. On screen with north
    // up and east to the left, that is a rotation of -PA.
    const double rot = -catalog::posAngleDeg(r) * 3.14159265358979 / 180.0;
    const double cr = cos(rot), sr = sin(rot);
    const bool overflow = (rx * 2 > bh) || (rx * 2 > bw);

    for (int i = 0; i < 25; ++i) {
        const double t = 2.0 * 3.14159265358979 * (i % 24) / 24.0;
        const double x = rx * cos(t), y = ry * sin(t);
        int px = int(cx + x * cr - y * sr + 0.5);
        int py = int(cy + x * sr + y * cr + 0.5);
        // Clamp so an overflowing object is clipped by the frame rather than
        // drawn outside it.
        if (px < 0) px = 0;
        if (px > bw - 1) px = bw - 1;
        if (py < 0) py = 0;
        if (py > bh - 1) py = bh - 1;
        fovPts_[i].x = lv_coord_t(px);
        fovPts_[i].y = lv_coord_t(py);
    }
    lv_line_set_points(fovLine_, fovPts_, 25);
    lv_obj_set_style_line_color(fovLine_,
        overflow ? T_->warn : T_->accent, 0);
    lv_obj_set_style_line_width(fovLine_, overflow ? 3 : 2, 0);

    if (overflow) {
        lv_obj_clear_flag(fovNote_, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(fovNote_, "OVERFLOWS FIELD");
    } else {
        lv_obj_add_flag(fovNote_, LV_OBJ_FLAG_HIDDEN);
    }
}

void ScreenCatalog::updateDetail(const MountStatus& m, SkyContext& ctx) {
    if (ctx.selectedRecord == 0xFFFF) return;
    const catalog::Record& r = catalog::kRecords[ctx.selectedRecord];

    double alt = 0, az = 0;
    if (ctx.haveSite(m))
        catalog::altAzOf(catalog::raHours(r), catalog::decDeg(r),
                         m.localSiderealHours, m.siteLatDeg, alt, az);

    lv_img_set_src(dIcon_, T_->objGlyph[glyphSlot(r)][1]);
    const char* pop = catalog::popularName(r);
    if (*pop) {
        lv_label_set_text(dTitle_, pop);
        setText(dType_, "%s  %s  %s", catalog::designation(r),
                catalog::constellation(r),
                catalog::typeName(catalog::typeOf(r)));
    } else {
        lv_label_set_text(dTitle_, catalog::designation(r));
        setText(dType_, "%s  %s", catalog::constellation(r),
                catalog::typeName(catalog::typeOf(r)));
    }

    const double maj = catalog::majorArcmin(r);
    if (maj >= 60.0) setText(dVal_[0], "%.2f\xC2\xB0", maj / 60.0);
    else             setText(dVal_[0], "%.1f'", maj);

    if (catalog::hasMag(r))
        setText(dVal_[1], "%.1f %s", catalog::magnitude(r),
                catalog::magIsBlue(r) ? "B" : "V");
    else
        lv_label_set_text(dVal_[1], "--");

    setText(dVal_[2], "%.0f\xC2\xB0", alt);
    lv_obj_set_style_text_color(dVal_[2],
        alt < 20 ? T_->alert : (alt < 35 ? T_->warn : T_->good), 0);

    const double am = catalog::airmass(alt);
    if (am > 9.9) lv_label_set_text(dVal_[3], "--");
    else          setText(dVal_[3], "%.2f", am);
    lv_obj_set_style_text_color(dVal_[3],
        am < 1.3 ? T_->good : (am < 2.0 ? T_->text : T_->warn), 0);

    if (catalog::hasSB(r)) setText(dVal_[4], "%.1f", catalog::surfBright(r));
    else                   lv_label_set_text(dVal_[4], "--");

    if (ctx.haveSite(m)) {
        const double ha = catalog::hourAngleOf(catalog::raHours(r),
                                               m.localSiderealHours);
        const double toT = -ha * 0.99727;
        const long s = long(fabs(toT) * 3600.0);
        if (toT >= 0) setText(dVal_[5], "%ld:%02ld", s / 3600, (s / 60) % 60);
        else          setText(dVal_[5], "-%ld:%02ld", s / 3600, (s / 60) % 60);
        lv_obj_set_style_text_color(dVal_[5], toT >= 0 ? T_->good : T_->dim, 0);
        setText(dMax_, "MAX %.0f\xC2\xB0  AZ %.0f\xC2\xB0",
                catalog::transitAltitude(catalog::decDeg(r), m.siteLatDeg), az);
    } else {
        lv_label_set_text(dVal_[5], "--");
        lv_label_set_text(dMax_, "NO SITE DATA");
    }

    if (ctx.sky.valid) {
        const double sep = ephem::separation(
            catalog::raHours(r), catalog::decDeg(r),
            ctx.sky.moonRaHours, ctx.sky.moonDecDeg);
        setText(dMoon_, "MOON %.0f\xC2\xB0 %.0f%% %s", sep,
                ctx.sky.moonIllum * 100.0, ctx.sky.moonUp ? "UP" : "DOWN");
        lv_obj_set_style_text_color(dMoon_,
            !ctx.sky.moonUp ? T_->dim
                            : (sep >= 90 ? T_->good
                                         : (sep >= 40 ? T_->warn : T_->alert)),
            0);
    } else {
        lv_label_set_text(dMoon_, "MOON  SET DATE");
        lv_obj_set_style_text_color(dMoon_, T_->dim, 0);
    }

    setEllipse(r, ctx.settings.fovArcmin);
}

void ScreenCatalog::update(const MountStatus& m, const UiExtras& ex,
                           SkyContext& ctx) {
    (void)ex;
    // A rebuild invalidates the index, so clamp the selection when the
    // generation counter moves rather than diffing the list.
    if (ctx.generation() != seenGeneration_) {
        seenGeneration_ = ctx.generation();
        if (ctx.view.count() && listSel_ >= ctx.view.count())
            listSel_ = uint16_t(ctx.view.count() - 1);
    }

    switch (state_) {
        case FILTER:
            setText(hdrRight_, "SKY WINDOW");
            updateFilter(ctx);
            break;
        case LIST:
            setText(hdrRight_, "%u/%u  %s",
                    ctx.view.count() ? unsigned(listSel_ + 1) : 0u,
                    unsigned(ctx.view.count()),
                    catalog::sortName(ctx.settings.filter.sort));
            updateList(ctx);
            break;
        default:
            if (gotoConfirm_)      lv_label_set_text(hdrRight_, "CONFIRM GOTO");
            else if (gotoResult_ > 0) lv_label_set_text(hdrRight_, "GOTO SENT");
            else if (gotoResult_ < 0) lv_label_set_text(hdrRight_, "GOTO FAILED");
            else setText(hdrRight_, "FOV %.0f'", ctx.settings.fovArcmin);
            updateDetail(m, ctx);
            break;
    }
}

// =====================================================================
//  input
// =====================================================================
bool ScreenCatalog::onButton(char which, SkyContext& ctx) {
    catalog::Filter& f = ctx.settings.filter;
    switch (state_) {
    case FILTER:
        if (which == 'A') {
            if (filterFocus_ < 8)      f.toggleSector(filterFocus_);
            else if (filterFocus_ == 8) f.altFloor = f.altFloor > 20 ? 10 : 30;
            else f.toggleGroup(catalog::TypeGroup(filterFocus_ - 9));
            ctx.markDirty();
            return true;
        }
        if (which == 'Y') {
            // ALL -> EAST -> WEST -> SOUTH -> ALL
            switch (f.azMask) {
                case 0xFF:       f.azMask = 0b00001110; break;
                case 0b00001110: f.azMask = 0b11100000; break;
                case 0b11100000: f.azMask = 0b00111000; break;
                default:         f.azMask = 0xFF; break;
            }
            ctx.markDirty();
            return true;
        }
        if (which == 'B') { state_ = LIST; showState(); return true; }
        return false;

    case LIST:
        if (which == 'A') {
            if (ctx.view.count()) {
                ctx.selectedRecord = ctx.view.recordIndex(listSel_);
                gotoConfirm_ = false;
                gotoResult_ = 0;
                state_ = DETAIL;
                showState();
            }
            return true;
        }
        if (which == 'Y') {
            int s = int(f.sort) + 1;
            if (s >= catalog::SORT_COUNT) s = 0;
            f.sort = catalog::SortMode(s);
            ctx.markDirty();
            listSel_ = 0;
            listTop_ = 0;
            return true;
        }
        if (which == 'B') { state_ = FILTER; showState(); return true; }
        return false;

    default:
        if (which == 'B') {
            if (gotoConfirm_) { gotoConfirm_ = false; return true; }
            gotoResult_ = 0; state_ = LIST; showState(); return true;
        }
        if (which == 'A') {
            if (gotoResult_ != 0) { gotoResult_ = 0; return true; }
            if (!gotoConfirm_) { gotoConfirm_ = true; return true; }
            gotoConfirm_ = false;
            if (ctx.selectedRecord == 0xFFFF || !ctx.gotoHandler) {
                gotoResult_ = -1;
                return true;
            }
            const catalog::Record& r = catalog::kRecords[ctx.selectedRecord];
            gotoResult_ = ctx.gotoHandler(catalog::raHours(r),
                                          catalog::decDeg(r),
                                          catalog::designation(r)) ? 1 : -1;
            return true;
        }
        return false;
    }
}

bool ScreenCatalog::onMove(int dx, int dy, SkyContext& ctx) {
    catalog::Filter& f = ctx.settings.filter;
    switch (state_) {
    case FILTER:
        if (dy && filterFocus_ == 8) {
            float v = f.altFloor + dy;
            if (v < 0) v = 0;
            if (v > 85) v = 85;
            f.altFloor = v;
            ctx.markDirty();
            return true;
        }
        if (dx || dy) {
            const int step = dx ? dx : (dy > 0 ? -1 : 1);
            filterFocus_ = (filterFocus_ + step + 15) % 15;
            return true;
        }
        return false;

    case LIST: {
        const uint16_t n = ctx.view.count();
        if (!n) return true;
        int sel = int(listSel_);
        if (dy) sel -= dy;              // stick up walks up the list
        if (dx) sel += dx * kRows;      // stick sideways pages
        if (sel < 0) sel = 0;
        if (sel >= int(n)) sel = int(n) - 1;
        listSel_ = uint16_t(sel);
        return true;
    }

    default:
        // Detail: left/right adjusts the field of view live, so framing can
        // be judged against the object on screen instead of from memory.
        if (dx) {
            gotoConfirm_ = false;
            gotoResult_ = 0;
            ctx.settings.nudgeFov(dx);
            ctx.markDirty();
            return true;
        }
        return false;
    }
}
