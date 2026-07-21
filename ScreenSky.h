#pragma once
#include "ScreenModule.h"

// =====================================================================
//  SKY -- sun regime, twilight countdowns, moon, meridian flip.
//
//  The twilight ribbon is 48 fixed segment objects rather than a canvas.
//  Segments are created once and only ever recoloured, so the whole strip
//  costs 48 style writes per refresh and nothing per frame. It is updated
//  on a 30 s tick because the sun does not move faster than that matters.
// =====================================================================
class ScreenSky : public ScreenModule {
public:
    const char* name() const override { return "SKY"; }
    const char* hint() const override { return "A SET DATE"; }

    void build(lv_obj_t* root, const Theme& T) override;
    void update(const MountStatus& m, const UiExtras& ex,
                SkyContext& ctx) override;
    bool onButton(char which, SkyContext& ctx) override;
    bool onMove(int dx, int dy, SkyContext& ctx) override;

    // The date editor is a sub-state of this screen rather than a separate
    // screen: it is only ever reached from here and only ever returns here.
    bool editingDate() const { return editing_; }

private:
    void buildRibbon(lv_obj_t* parent, const Theme& T);
    void updateRibbon(const SkyContext& ctx);
    void updateDateEditor(const SkyContext& ctx);
    lv_color_t regimeColor(int regime) const;

    const Theme* T_ = nullptr;

    static constexpr int kSeg = 48;
    lv_obj_t* seg_[kSeg] = {};
    lv_obj_t* nowMark_ = nullptr;

    lv_obj_t* hdrRight_ = nullptr;
    lv_obj_t* regimeLbl_ = nullptr;
    lv_obj_t* countLbl_ = nullptr;
    lv_obj_t* sunLbl_ = nullptr;
    lv_obj_t* moonImg_ = nullptr;
    lv_obj_t* moonPct_ = nullptr;
    lv_obj_t* moonPhase_ = nullptr;
    lv_obj_t* moonRise_ = nullptr;
    lv_obj_t* flipLbl_ = nullptr;
    lv_obj_t* flipVal_ = nullptr;
    lv_obj_t* noDate_ = nullptr;
    lv_obj_t* body_ = nullptr;

    // date editor
    lv_obj_t* editPanel_ = nullptr;
    lv_obj_t* editField_[5] = {};
    lv_obj_t* editBox_[5] = {};
    bool editing_ = false;
    int  editSel_ = 0;
    int  lastMoonIdx_ = -1;
    uint32_t lastRibbonMs_ = 0;
};
