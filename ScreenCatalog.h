#pragma once
#include "ScreenModule.h"

// =====================================================================
//  CATALOG -- filter, list and detail, as three states of one screen.
//
//  They are one module because they share all their state and the only
//  navigation between them is a linear drill-down. Splitting them into
//  three modules would mean three copies of the selection index and a
//  back-stack to keep them in sync.
//
//  State machine:
//      FILTER  --B-->  LIST  --A-->  DETAIL
//      FILTER  <--B--  LIST  <--B--  DETAIL
//
//  Only the objects for the active state are un-hidden; all three sets are
//  built once at boot. Hiding is O(1) and avoids any allocation on nav.
// =====================================================================
class ScreenCatalog : public ScreenModule {
public:
    const char* name() const override { return "CATALOG"; }
    const char* hint() const override;

    void build(lv_obj_t* root, const Theme& T) override;
    void update(const MountStatus& m, const UiExtras& ex,
                SkyContext& ctx) override;
    bool onButton(char which, SkyContext& ctx) override;
    bool onMove(int dx, int dy, SkyContext& ctx) override;

private:
    enum State : uint8_t { FILTER = 0, LIST, DETAIL };

    void buildFilter(lv_obj_t* root, const Theme& T);
    void buildList(lv_obj_t* root, const Theme& T);
    void buildDetail(lv_obj_t* root, const Theme& T);

    void updateFilter(SkyContext& ctx);
    void updateList(SkyContext& ctx);
    void updateDetail(const MountStatus& m, SkyContext& ctx);

    void showState();
    void setEllipse(const catalog::Record& r, double fovArcmin);

    const Theme* T_ = nullptr;
    State state_ = FILTER;

    lv_obj_t* hdrRight_ = nullptr;

    // ---- filter ----------------------------------------------------------
    // Eight lv_arc wedges around a common centre. lv_arc handles angular
    // spans natively, which is exactly the primitive this widget needs.
    static constexpr int kSectors = 8;
    lv_obj_t* filterRoot_ = nullptr;
    lv_obj_t* wedge_[kSectors] = {};
    lv_obj_t* floorRing_ = nullptr;
    lv_obj_t* zenithCap_ = nullptr;
    lv_obj_t* chip_[6] = {};
    lv_obj_t* chipLbl_[6] = {};
    lv_obj_t* chipIcon_[6] = {};
    lv_obj_t* countBig_ = nullptr;
    lv_obj_t* countSub_ = nullptr;
    lv_obj_t* floorLbl_ = nullptr;
    lv_obj_t* zenithLbl_ = nullptr;
    int filterFocus_ = 0;         // 0..7 wedges, 8 floor ring, 9..14 chips

    // ---- list ------------------------------------------------------------
    static constexpr int kRows = 5;
    lv_obj_t* listRoot_ = nullptr;
    lv_obj_t* row_[kRows] = {};
    lv_obj_t* rowIcon_[kRows] = {};
    lv_obj_t* rowDesig_[kRows] = {};
    lv_obj_t* rowName_[kRows] = {};
    lv_obj_t* rowMag_[kRows] = {};
    lv_obj_t* rowSize_[kRows] = {};
    lv_obj_t* rowAlt_[kRows] = {};
    lv_obj_t* scrollThumb_ = nullptr;
    lv_obj_t* emptyLbl_ = nullptr;
    uint16_t listSel_ = 0;
    uint16_t listTop_ = 0;
    uint32_t seenGeneration_ = 0xFFFFFFFF;

    // ---- detail ----------------------------------------------------------
    lv_obj_t* detailRoot_ = nullptr;
    lv_obj_t* fovBox_ = nullptr;
    lv_obj_t* fovLine_ = nullptr;
    lv_point_t fovPts_[25] = {};
    lv_obj_t* fovDot_ = nullptr;
    lv_obj_t* fovNote_ = nullptr;
    lv_obj_t* dTitle_ = nullptr;
    lv_obj_t* dType_ = nullptr;
    lv_obj_t* dIcon_ = nullptr;
    lv_obj_t* dVal_[6] = {};
    lv_obj_t* dLbl_[6] = {};
    lv_obj_t* dMoon_ = nullptr;
    lv_obj_t* dMax_ = nullptr;
    bool gotoConfirm_ = false;
    int8_t gotoResult_ = 0;       // 0 idle, 1 accepted, -1 rejected/failed
};
