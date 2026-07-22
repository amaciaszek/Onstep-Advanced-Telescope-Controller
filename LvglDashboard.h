#pragma once
#include <Arduino.h>
#include "FontCompat.h"
#include "MountStatus.h"
#include "UiAssets.h"
#include "SkyContext.h"

class ScreenModule;

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
    // ---- v9 -------------------------------------------------------------
    // Object type glyphs, indexed by catalog::TypeGroup, x (outline,filled).
    // In DAY mode the six differ by hue. In NIGHT mode hue is unavailable,
    // so they differ by interior TEXTURE instead: smooth / annulus /
    // stipple / hatch / points / torn arc. See tools/gen_assets_v9.py.
    const lv_img_dsc_t *objGlyph[catalog::GLYPH_COUNT][2];
    const lv_img_dsc_t *moon[8];   // phase 0=new, 2=first qtr, 4=full
    const lv_img_dsc_t *navSky[2];
    bool nightRule;                // true = swap outline/filled instead of opacity dim
};

class LvglDashboard {
public:
    void begin();
    void update(const MountStatus& m, const UiExtras& ex);
    void nextScreen();               // HOME -> SKY -> CATALOG -> SETTINGS -> DIAG
    void showSettings();              // direct access from SELECT
    void setNight(bool on);          // rebuilds screens in the other theme
    bool night() const { return night_; }
    void setWidget(int idx);         // 0..3, rebuilds the radar region
    int  widget() const { return widgetIdx_; }
    int  screenIndex() const { return screenIdx_; }
    void setBrightness(int percent);
    int  brightness() const { return brightness_; }
    void setShowTemperature(bool show);
    bool showTemperature() const { return showTemperature_; }
    static const char* widgetName(int idx);
    void buttonA();                  // on SETTINGS screen: cycle widget
    void buttonSelect();
    void settingsMove(int delta);    // joystick up/down on SETTINGS
    void inputMove(int dx,int dy);    // settings or keyboard cursor
    void setNetworkProfiles(const String ssid[3],const String password[3],int active);
    void setNetworkSaveHandler(void (*handler)(int,const char*,const char*)) { networkSaveHandler_ = handler; }
    void setProfileActivateHandler(void (*handler)(int)) { profileActivateHandler_=handler; }
    void setBrightnessHandler(void (*handler)(int)) { brightnessHandler_ = handler; }
    void setGotoHandler(SkyContext::GotoHandler handler) { sky_.gotoHandler = handler; }
    void setDiagText(const char* txt);

    // ---- v9 --------------------------------------------------------------
    // The dashboard owns the sky/catalog state so the sketch only has to
    // feed it a tick; screens are pure views over it.
    SkyContext& sky() { return sky_; }
    void tickSky(uint32_t dtMs, const MountStatus& m) { sky_.tick(dtMs, m); }
    void buttonB();
    void buttonX();
    void buttonY();
private:
    lv_obj_t* label(lv_obj_t* p,int x,int y,int w,int h,const char* t,const lv_font_t* f,lv_color_t c,lv_text_align_t a=LV_TEXT_ALIGN_LEFT);
    lv_obj_t* panel(lv_obj_t* p,int x,int y,int w,int h,int radius=8);
    lv_obj_t* image(lv_obj_t* p,int x,int y,const lv_img_dsc_t* src,lv_opa_t opa=LV_OPA_COVER);
    void makeModuleScreens();
    bool moduleButton(char which);
    void makeHome();
    void makeDiag();
    void makeSettings();
    void makeKeyboard();
    void makeProfiles();
    void refreshKeyboard();
    void keyboardSelect();
    void openKeyboardForProfile(int slot);
    void buildWidget(lv_obj_t* parent);   // active radar-region widget
    void updateWidget(const MountStatus& m,const UiExtras& ex);
    void rebuild();
    // Registry of v9 screens. ADD A SCREEN: write a ScreenModule subclass,
    // add it here and to kModuleCount. Nothing else changes.
    static constexpr int kModuleCount = 2;
    static constexpr int kModuleBase  = 5;   // screenIdx_ 5.. are modules
    ScreenModule* module_[kModuleCount] = {};
    lv_obj_t*     moduleScreen_[kModuleCount] = {};
    lv_obj_t*     moduleHint_[kModuleCount] = {};
    SkyContext    sky_;
    uint32_t      lastSkyMs_ = 0;
    void setText(lv_obj_t* o,const char* fmt,...);
    void setIconState(lv_obj_t* icon,const lv_img_dsc_t* const pair[2],bool active);
    // sky-track math: alt/az of (current dec) at hour angle ha, project to radar px
    bool projectHA(double haHours,double decDeg,double latDeg,int& px,int& py,double& altOut);

    const Theme* T_=nullptr;
    bool night_=false;
    char diagCache_[360]="collecting...";

    lv_obj_t *home_=nullptr,*diag_=nullptr,*settings_=nullptr,*keyboard_=nullptr,*profilesScreen_=nullptr;
    int screenIdx_=0;
    int widgetIdx_=0;
    int settingsSel_=0;
    int brightness_=25;
    bool showTemperature_=false;
    void (*networkSaveHandler_)(int,const char*,const char*)=nullptr;
    void (*profileActivateHandler_)(int)=nullptr;
    void (*brightnessHandler_)(int)=nullptr;
    int keyboardSel_=0,keyboardMode_=0;
    int profileSel_=0,editProfile_=0,activeProfile_=0;
    String profileSsid_[3],profilePassword_[3];
    bool passwordStage_=false;
    String editSsid_,editPassword_;
    lv_obj_t *keyboardTitle_=nullptr,*keyboardValue_=nullptr,*keyboardKeys_[45]={};
    uint32_t keyboardBlinkMs_=0;
    bool keyboardBlinkOn_=true;
    uint32_t lastSwitchMs_=0;
    lv_obj_t* setRows_[4]={};
    lv_obj_t* setNightRow_=nullptr;
    lv_obj_t *ribbon_;
    lv_obj_t *raMain_,*raSec_,*decMain_,*decSec_,*alt_,*az_,*pier_,*stale_;
    lv_obj_t *battery_,*temp_,*tempIcon_,*rate_;
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
    lv_obj_t *pulseRaLine_=nullptr,*pulseDecLine_=nullptr,*pulseDir_=nullptr;
    lv_point_t pulseRaPts_[20],pulseDecPts_[20];
    float pulseRaHistory_[20]={},pulseDecHistory_[20]={};
    int pulseHead_=0; double pulseLastRa_=NAN,pulseLastDec_=NAN;
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
