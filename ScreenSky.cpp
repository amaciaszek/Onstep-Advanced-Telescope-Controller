#include "ScreenSky.h"
#include "LvglDashboard.h"
#include <stdio.h>
#include <math.h>

// Twilight band colours. Day mode uses real sky colours; night mode has to
// encode four regimes in one hue, so it uses four LUMINANCE steps of red
// instead. That works because the bands are adjacent and compared against
// each other, not identified in isolation.
lv_color_t ScreenSky::regimeColor(int regime) const {
    if (T_->nightRule) {
        switch (regime) {
            case ephem::TW_DAY:          return lv_color_hex(0xFF5A38);
            case ephem::TW_CIVIL:        return lv_color_hex(0xC03018);
            case ephem::TW_NAUTICAL:     return lv_color_hex(0x7A1C0A);
            case ephem::TW_ASTRONOMICAL: return lv_color_hex(0x3E0E05);
            default:                     return lv_color_hex(0x140302);
        }
    }
    switch (regime) {
        case ephem::TW_DAY:          return lv_color_hex(0xFFBA4A);
        case ephem::TW_CIVIL:        return lv_color_hex(0xE07634);
        case ephem::TW_NAUTICAL:     return lv_color_hex(0x3054A8);
        case ephem::TW_ASTRONOMICAL: return lv_color_hex(0x1A2656);
        default:                     return lv_color_hex(0x080B16);
    }
}

void ScreenSky::buildRibbon(lv_obj_t* p, const Theme& T) {
    // 48 segments across 300 px. Created once, recoloured thereafter.
    const int x0 = 10, y = 42, w = 300, h = 13;
    lv_obj_t* frame = lv_obj_create(p);
    lv_obj_set_pos(frame, x0 - 1, y - 1);
    lv_obj_set_size(frame, w + 2, h + 2);
    lv_obj_clear_flag(frame, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(frame, 0, 0);
    lv_obj_set_style_bg_opa(frame, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(frame, T.edge, 0);
    lv_obj_set_style_border_width(frame, 1, 0);
    lv_obj_set_style_pad_all(frame, 0, 0);

    const int segW = w / kSeg;
    for (int i = 0; i < kSeg; ++i) {
        lv_obj_t* s = lv_obj_create(p);
        lv_obj_set_pos(s, x0 + i * segW, y);
        lv_obj_set_size(s, segW + 1, h);
        lv_obj_clear_flag(s, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_radius(s, 0, 0);
        lv_obj_set_style_border_width(s, 0, 0);
        lv_obj_set_style_pad_all(s, 0, 0);
        lv_obj_set_style_bg_opa(s, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(s, T.panel, 0);
        seg_[i] = s;
    }

    // "Now" marker sits at the centre: the ribbon is always drawn as the
    // 24 h window centred on the present moment, so the marker never moves
    // and the SKY slides underneath it. Cheaper and easier to read than a
    // travelling caret.
    nowMark_ = lv_obj_create(p);
    lv_obj_set_pos(nowMark_, x0 + w / 2 - 1, y - 4);
    lv_obj_set_size(nowMark_, 2, h + 8);
    lv_obj_clear_flag(nowMark_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(nowMark_, 0, 0);
    lv_obj_set_style_border_width(nowMark_, 0, 0);
    lv_obj_set_style_bg_opa(nowMark_, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(nowMark_, T.text, 0);

    label(p, x0, 31, 0, "-12H", FONT_TINY, T.dim);
    label(p, x0 + w / 2 - 14, 31, 28, "NOW", FONT_TINY, T.accent,
          LV_TEXT_ALIGN_CENTER);
    label(p, x0 + w - 40, 31, 40, "+12H", FONT_TINY, T.dim,
          LV_TEXT_ALIGN_RIGHT);
}

void ScreenSky::build(lv_obj_t* root, const Theme& T) {
    T_ = &T;
    lastMoonIdx_ = -1;

    header(root, T, "SKY", T.accent, &hdrRight_);
    buildRibbon(root, T);

    // ---- regime block ----------------------------------------------------
    body_ = panel(root, T, 10, 62, 196, 50);
    regimeLbl_ = label(body_, 8, 4, 180, "--", FONT_BODY, T.text);
    countLbl_  = label(body_, 8, 20, 180, "--", FONT_VALUE, T.text);
    sunLbl_    = label(body_, 8, 40, 180, "", FONT_TINY, T.dim);

    // ---- moon ------------------------------------------------------------
    lv_obj_t* mp = panel(root, T, 212, 62, 98, 50);
    moonImg_   = image(mp, 4, 9, T.moon[0]);
    moonPct_   = label(mp, 40, 5, 54, "--", FONT_BODY, T.text);
    moonPhase_ = label(mp, 40, 21, 54, "", FONT_TINY, T.dim);
    moonRise_  = label(mp, 40, 33, 54, "", FONT_TINY, T.dim);

    // ---- meridian flip ---------------------------------------------------
    lv_obj_t* fp = panel(root, T, 10, 116, 300, 17, 3);
    flipLbl_ = label(fp, 6, 2, 150, "MERIDIAN FLIP", FONT_TINY, T.dim);
    flipVal_ = label(fp, 150, 2, 144, "--:--", FONT_TINY, T.text,
                     LV_TEXT_ALIGN_RIGHT);

    // ---- no-date overlay -------------------------------------------------
    noDate_ = panel(root, T, 10, 62, 300, 50);
    label(noDate_, 0, 6, 300, "DATE NOT SET", FONT_BODY, T.warn,
          LV_TEXT_ALIGN_CENTER);
    label(noDate_, 0, 24, 300, "SUN AND MOON NEED A CALENDAR DATE",
          FONT_TINY, T.dim, LV_TEXT_ALIGN_CENTER);
    label(noDate_, 0, 35, 300, "PRESS A TO ENTER IT", FONT_TINY, T.dim,
          LV_TEXT_ALIGN_CENTER);
    lv_obj_add_flag(noDate_, LV_OBJ_FLAG_HIDDEN);

    // ---- date editor -----------------------------------------------------
    editPanel_ = panel(root, T, 10, 38, 300, 95);
    label(editPanel_, 0, 4, 300, "UTC DATE AND TIME", FONT_SMALL, T.warn,
          LV_TEXT_ALIGN_CENTER);
    label(editPanel_, 0, 20, 300, "ENTER UTC, NOT LOCAL TIME", FONT_TINY,
          T.dim, LV_TEXT_ALIGN_CENTER);
    static const char* lbl[5] = {"YEAR", "MON", "DAY", "HH", "MM"};
    static const int wid[5] = {56, 40, 40, 40, 40};
    int x = 20;
    for (int i = 0; i < 5; ++i) {
        lv_obj_t* b = lv_obj_create(editPanel_);
        lv_obj_set_pos(b, x, 36);
        lv_obj_set_size(b, wid[i], 30);
        lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_radius(b, 3, 0);
        lv_obj_set_style_bg_color(b, T.panel, 0);
        lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(b, T.edge, 0);
        lv_obj_set_style_border_width(b, 1, 0);
        lv_obj_set_style_pad_all(b, 0, 0);
        editBox_[i] = b;
        editField_[i] = label(b, 0, 6, wid[i], "--", FONT_VALUE, T.text,
                              LV_TEXT_ALIGN_CENTER);
        label(editPanel_, x, 68, wid[i], lbl[i], FONT_TINY, T.dim,
              LV_TEXT_ALIGN_CENTER);
        x += wid[i] + 8;
        if (i == 2) x += 8;
    }
    lv_obj_add_flag(editPanel_, LV_OBJ_FLAG_HIDDEN);
}

void ScreenSky::updateRibbon(const SkyContext& ctx) {
    if (!ctx.sunTrackValid) {
        for (int i = 0; i < kSeg; ++i)
            lv_obj_set_style_bg_color(seg_[i], T_->panel, 0);
        return;
    }
    for (int i = 0; i < kSeg; ++i) {
        // kSeg segments map 1:1 onto kSunSamples by construction.
        const float alt = ctx.sunTrack[i * SkyContext::kSunSamples / kSeg];
        const int rg = ephem::twilightOf(alt);
        lv_obj_set_style_bg_color(seg_[i], regimeColor(rg), 0);
    }
}

void ScreenSky::updateDateEditor(const SkyContext& ctx) {
    const ephem::Clock& c = ctx.settings.clock;
    setText(editField_[0], "%04d", c.year);
    setText(editField_[1], "%02d", c.month);
    setText(editField_[2], "%02d", c.day);
    setText(editField_[3], "%02d", int(c.utcHours));
    setText(editField_[4], "%02d", int(fmod(c.utcHours * 60.0, 60.0)));
    for (int i = 0; i < 5; ++i) {
        const bool sel = (i == editSel_);
        lv_obj_set_style_border_color(editBox_[i], sel ? T_->warn : T_->edge, 0);
        lv_obj_set_style_border_width(editBox_[i], sel ? 2 : 1, 0);
        lv_obj_set_style_text_color(editField_[i], sel ? T_->warn : T_->text, 0);
    }
}

void ScreenSky::update(const MountStatus& m, const UiExtras& ex,
                       SkyContext& ctx) {
    (void)ex;

    if (m.localSiderealHours >= 0)
        setText(hdrRight_, "LST %02d:%02d", int(m.localSiderealHours),
                int(fmod(m.localSiderealHours * 60.0, 60.0)));
    else
        setText(hdrRight_, "LST --:--");

    if (editing_) {
        lv_obj_add_flag(body_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(noDate_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(editPanel_, LV_OBJ_FLAG_HIDDEN);
        updateDateEditor(ctx);
        return;
    }
    lv_obj_add_flag(editPanel_, LV_OBJ_FLAG_HIDDEN);

    updateRibbon(ctx);

    if (!ctx.sky.valid) {
        lv_obj_add_flag(body_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(noDate_, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_clear_flag(body_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(noDate_, LV_OBJ_FLAG_HIDDEN);

        const ephem::Twilight rg = ctx.sky.regime;
        lv_label_set_text(regimeLbl_, ephem::twilightName(rg));
        lv_obj_set_style_text_color(regimeLbl_,
            rg == ephem::TW_NIGHT ? T_->good : T_->text, 0);

        if (ctx.sky.toNextTransition >= 0) {
            const long s = long(ctx.sky.toNextTransition);
            setText(countLbl_, "%s IN %ld:%02ld:%02ld",
                    ephem::twilightShort(ctx.sky.nextRegime),
                    s / 3600, (s / 60) % 60, s % 60);
        } else {
            setText(countLbl_, "NO CHANGE 24H");
        }

        const long de = long(ctx.sky.toDarkEnd);
        if (ctx.sky.toDarkEnd >= 0)
            setText(sunLbl_, "SUN %+.1f  DARK ENDS %ld:%02ld",
                    ctx.sky.sunAltDeg, de / 3600, (de / 60) % 60);
        else
            setText(sunLbl_, "SUN %+.1f", ctx.sky.sunAltDeg);

        const int mi = ctx.moonPhaseIndex();
        if (mi != lastMoonIdx_) {
            lv_img_set_src(moonImg_, T_->moon[mi & 7]);
            lastMoonIdx_ = mi;
        }
        setText(moonPct_, "%.0f%%", ctx.sky.moonIllum * 100.0);
        lv_obj_set_style_text_color(moonPct_,
            ctx.sky.moonUp ? T_->text : T_->dim, 0);
        lv_label_set_text(moonPhase_, ctx.sky.moonWaxing > 0 ? "WAX" : "WANE");
        const double t = ctx.sky.moonUp ? ctx.sky.toMoonSet : ctx.sky.toMoonRise;
        if (t >= 0) {
            const long s = long(t);
            setText(moonRise_, "%s %ld:%02ld", ctx.sky.moonUp ? "SET" : "UP",
                    s / 3600, (s / 60) % 60);
        } else {
            lv_label_set_text(moonRise_, "--");
        }
        lv_obj_set_style_text_color(moonRise_,
            ctx.sky.moonUp ? T_->warn : T_->good, 0);
    }

    // ---- meridian flip ---------------------------------------------------
    const double flip = ctx.meridianFlipSeconds(m);
    if (flip >= 0) {
        const long s = long(flip);
        setText(flipVal_, "%ld:%02ld:%02ld", s / 3600, (s / 60) % 60, s % 60);
        const bool soon = flip < 900.0;
        lv_obj_set_style_text_color(flipVal_, soon ? T_->alert : T_->text, 0);
        // In night mode the alert colour is the same red as everything else,
        // so urgency is carried by WEIGHT: the label switches to the larger
        // font rather than changing hue.
        lv_obj_set_style_text_font(flipVal_,
            (soon && T_->nightRule) ? FONT_SMALL : FONT_TINY, 0);
        lv_label_set_text(flipLbl_, "MERIDIAN FLIP");
    } else {
        lv_label_set_text(flipVal_, "PAST / UNKNOWN");
        lv_obj_set_style_text_color(flipVal_, T_->dim, 0);
        lv_obj_set_style_text_font(flipVal_, FONT_TINY, 0);
        lv_label_set_text(flipLbl_, "MERIDIAN");
    }
}

bool ScreenSky::onButton(char which, SkyContext& ctx) {
    if (which == 'A') {
        if (editing_) {
            ctx.settings.clock.valid = true;
            editing_ = false;
        } else {
            editing_ = true;
            editSel_ = 0;
        }
        return true;
    }
    if (which == 'B' && editing_) { editing_ = false; return true; }
    return false;
}

bool ScreenSky::onMove(int dx, int dy, SkyContext& ctx) {
    if (!editing_) return false;
    if (dx) {
        editSel_ = (editSel_ + dx + 5) % 5;
        return true;
    }
    if (!dy) return false;
    ephem::Clock& c = ctx.settings.clock;
    switch (editSel_) {
        case 0:
            c.year += dy;
            if (c.year < 2024) c.year = 2024;
            if (c.year > 2099) c.year = 2099;
            break;
        case 1:
            c.month += dy;
            if (c.month < 1) c.month = 12;
            if (c.month > 12) c.month = 1;
            break;
        case 2:
            c.day += dy;
            if (c.day < 1) c.day = 31;
            if (c.day > 31) c.day = 1;
            break;
        case 3:
            c.utcHours += dy;
            break;
        case 4:
            c.utcHours += dy / 60.0;
            break;
    }
    while (c.utcHours < 0) c.utcHours += 24.0;
    while (c.utcHours >= 24.0) c.utcHours -= 24.0;
    return true;
}
