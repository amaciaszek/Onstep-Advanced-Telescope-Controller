#pragma once
#include "FontCompat.h"
#include "MountStatus.h"
#include "UiAssets.h"

struct UiExtras {
    int      wifiBars   = 0;
    int      batteryPct = 0;
    float    tempC      = 0;
    bool     online     = false;
    uint32_t dataAgeMs  = 0;
    int      rssi       = 0;
    uint16_t latMs      = 0;
    uint8_t  okPct      = 100;
};

// One palette + icon set per theme. Night theme: pure red channel only,
// state shown by OUTLINE (inactive) vs FILLED (active) instead of dimming.
struct Theme {
    lv_color_t bg, panel, edge, accent, good, warn, alert, text, dim, sweep1, sweep2, sweep3, trail;
    // icon pairs: [0]=inactive/outline, [1]=active/filled (day mode uses same img + opacity)
    const lv_img_dsc_t *wifi[2], *link[2], *guide[2], *slew[2];
    const lv_img_dsc_t *parkLocked[2], *parkUnlocked[2];
    const lv_img_dsc_t *battery, *temperature, *panelMetric, *radarFrame;
    const lv_img_dsc_t *nav[4][2]; // home,goto,align,menu x (outline,filled)
    bool nightRule;                // true = swap outline/filled instead of opacity dim
};

class LvglDashboard {
public:
    void begin();
    void update(const MountStatus& m, const UiExtras& ex);
    void nextScreen();               // HOME -> DIAG -> SETTINGS -> HOME
    void setNight(bool on);          // rebuilds screens in the other theme
    bool night() const { return night_; }
    void setWidget(int idx);         // 0..3, rebuilds the radar region
    int  widget() const { return widgetIdx_; }
    int  screenIndex() const { return screenIdx_; }
    static const char* widgetName(int idx);
    void buttonA();                  // on SETTINGS screen: cycle widget
    void settingsMove(int delta);    // joystick up/down on SETTINGS
    void setNetworkSetupHandler(void (*handler)()) { networkSetupHandler_ = handler; }
    void setDiagText(const char* txt);
private:
    lv_obj_t* label(lv_obj_t* p,int x,int y,int w,int h,const char* t,const lv_font_t* f,lv_color_t c,lv_text_align_t a=LV_TEXT_ALIGN_LEFT);
    lv_obj_t* panel(lv_obj_t* p,int x,int y,int w,int h,int radius=8);
    lv_obj_t* image(lv_obj_t* p,int x,int y,const lv_img_dsc_t* src,lv_opa_t opa=LV_OPA_COVER);
    void makeHome();
    void makeDiag();
    void makeSettings();
    void buildWidget(lv_obj_t* parent);   // active radar-region widget
    void updateWidget(const MountStatus& m,const UiExtras& ex);
    void rebuild();
    void setText(lv_obj_t* o,const char* fmt,...);
    void setIconState(lv_obj_t* icon,const lv_img_dsc_t* const pair[2],bool active);
    // sky-track math: alt/az of (current dec) at hour angle ha, project to radar px
    bool projectHA(double haHours,double decDeg,double latDeg,int& px,int& py,double& altOut);

    const Theme* T_=nullptr;
    bool night_=false;
    char diagCache_[360]="collecting...";

    lv_obj_t *home_=nullptr,*diag_=nullptr,*settings_=nullptr;
    int screenIdx_=0;
    int widgetIdx_=0;
    int settingsSel_=0;
    void (*networkSetupHandler_)()=nullptr;
    uint32_t lastSwitchMs_=0;
    lv_obj_t* setRows_[4]={};
    lv_obj_t* setNightRow_=nullptr;
    lv_obj_t *ribbon_;
    lv_obj_t *raMain_,*raSec_,*decMain_,*decSec_,*alt_,*az_,*pier_,*stale_;
    lv_obj_t *battery_,*temp_,*rate_;
    lv_obj_t *wifiIcon_,*linkIcon_,*parkIcon_,*guideIcon_,*slewImg_;
    // ---- radar-region widgets (only the active one is built) ----
    // W0 sky track
    lv_obj_t *radarDot_=nullptr,*comet_=nullptr,*comet2_=nullptr,*comet3_=nullptr;
    lv_obj_t *trackLine_,*trackLowLine_,*pastLine_,*merMark_;
    lv_point_t trackPts_[10],trackLowPts_[10],pastPts_[4];
    int trackN_=0;
    lv_obj_t *trail_[6]; int trailHead_=0; uint32_t lastTrailMs_=0;
    int lastDotX_=0,lastDotY_=0;
    float dotFx_=280,dotFy_=65,haFx_=98;
    // W1 meridian clock
    lv_obj_t *clkArc_=nullptr,*clkNeedle_,*clkLst_,*clkLst2_,*clkCap_,*clkText_;
    lv_point_t clkNeedlePts_[2];
    float clkAngF_=0;
    // W2 guide scope
    lv_obj_t *gsCrossV_=nullptr,*gsCrossH_,*gsStar_,*gsRipple_,*gsBarRA_,*gsBarDE_,*gsCap_;
    lv_point_t gsVPts_[2],gsHPts_[2];
    float gsX_=0,gsY_=0; uint32_t gsRippleT0_=0; bool gsPulsePrev_=false;
    // W3 GEM mount
    lv_obj_t *gmPier_=nullptr,*gmBar_,*gmTube_,*gmWeight_,*gmCap_,*gmDrive_,*gmShine_;
    lv_point_t gmPierPts_[5],gmBarPts_[2],gmTubePts_[2];
    float gmAngF_=0,gmTubeF_=0;
    lv_obj_t *haDot_,*tmer_;
    lv_obj_t *adTitle_,*adLine1_,*adLine2_,*guideLine_;
    lv_point_t guidePts_[20];
    double guideHistory_[20]{}; int guideHead_=0;
    lv_obj_t *diagText_;
};
