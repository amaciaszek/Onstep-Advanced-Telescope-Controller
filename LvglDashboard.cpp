#include "LvglDashboard.h"
#include <Arduino.h>
#include <cmath>
#include <cstdarg>
#include <cstdio>

// ---------------------------------------------------------------- themes ---
static const Theme DAY = {
    /*bg*/lv_color_hex(0x020609), /*panel*/lv_color_hex(0x061018), /*edge*/lv_color_hex(0x17303E),
    /*accent*/lv_color_hex(0x17B8FF), /*good*/lv_color_hex(0x63F23A), /*warn*/lv_color_hex(0xFFD84D),
    /*alert*/lv_color_hex(0xFF4040), /*text*/lv_color_hex(0xF2F5F7), /*dim*/lv_color_hex(0x70808C),
    /*sweep*/lv_color_hex(0x1187B8), lv_color_hex(0x0A3A50), lv_color_hex(0x06222E), /*trail*/lv_color_hex(0x2E7D22),
    {&ui_img_icon_wifi,&ui_img_icon_wifi},{&ui_img_icon_link,&ui_img_icon_link},
    {&ui_img_icon_guide,&ui_img_icon_guide},{&ui_img_icon_slew,&ui_img_icon_slew},
    {&ui_img_icon_park_locked,&ui_img_icon_park_locked},{&ui_img_icon_park_unlocked,&ui_img_icon_park_unlocked},
    &ui_img_icon_battery,&ui_img_icon_temperature,&ui_img_panel_metric,&ui_img_sky_radar_frame,
    {{&ui_img_nav_home,&ui_img_nav_home},{&ui_img_nav_goto,&ui_img_nav_goto},
     {&ui_img_nav_align,&ui_img_nav_align},{&ui_img_nav_menu,&ui_img_nav_menu}},
    /*nightRule*/false
};
static const Theme NIGHT = {
    /*bg*/lv_color_hex(0x000000), /*panel*/lv_color_hex(0x100302), /*edge*/lv_color_hex(0x3A0C05),
    /*accent*/lv_color_hex(0xFF2814), /*good*/lv_color_hex(0xFF2814), /*warn*/lv_color_hex(0xFF6A30),
    /*alert*/lv_color_hex(0xFF2814), /*text*/lv_color_hex(0xFF4628), /*dim*/lv_color_hex(0x781208),
    /*sweep*/lv_color_hex(0xB01E0C), lv_color_hex(0x5E1006), lv_color_hex(0x300803), /*trail*/lv_color_hex(0x781208),
    {&ui_img_n_icon_wifi,&ui_img_n_icon_wifi_f},{&ui_img_n_icon_link,&ui_img_n_icon_link_f},
    {&ui_img_n_icon_guide,&ui_img_n_icon_guide_f},{&ui_img_n_icon_slew,&ui_img_n_icon_slew_f},
    {&ui_img_n_icon_park_locked,&ui_img_n_icon_park_locked_f},{&ui_img_n_icon_park_unlocked,&ui_img_n_icon_park_unlocked_f},
    &ui_img_n_icon_battery,&ui_img_n_icon_temperature,&ui_img_n_panel_metric,&ui_img_n_sky_radar_frame,
    {{&ui_img_n_nav_home,&ui_img_n_nav_home_f},{&ui_img_n_nav_goto,&ui_img_n_nav_goto_f},
     {&ui_img_n_nav_align,&ui_img_n_nav_align_f},{&ui_img_n_nav_menu,&ui_img_n_nav_menu_f}},
    /*nightRule*/true
};

static const int BAR_X0=20, BAR_X1=176, BAR_Y=133, BAR_CX=(BAR_X0+BAR_X1)/2;
static const int RCX=280, RCY=65, RR=28;   // radar center + usable radius

// ---------------------------------------------------------------- helpers --
lv_obj_t* LvglDashboard::panel(lv_obj_t* p,int x,int y,int w,int h,int r){
    lv_obj_t* o=lv_obj_create(p); lv_obj_set_pos(o,x,y); lv_obj_set_size(o,w,h);
    lv_obj_clear_flag(o,LV_OBJ_FLAG_SCROLLABLE); lv_obj_set_style_radius(o,r,0);
    lv_obj_set_style_bg_color(o,T_->panel,0); lv_obj_set_style_bg_opa(o,LV_OPA_70,0);
    lv_obj_set_style_border_color(o,T_->edge,0); lv_obj_set_style_border_width(o,1,0);
    lv_obj_set_style_pad_all(o,0,0); return o;
}
lv_obj_t* LvglDashboard::label(lv_obj_t* p,int x,int y,int w,int h,const char* t,const lv_font_t* f,lv_color_t c,lv_text_align_t a){
    lv_obj_t* o=lv_label_create(p); lv_obj_set_pos(o,x,y);
    if(w>0) lv_obj_set_size(o,w,h);
    lv_label_set_text(o,t);
    lv_obj_set_style_text_font(o,f,0); lv_obj_set_style_text_color(o,c,0);
    lv_obj_set_style_text_align(o,a,0); return o;
}
lv_obj_t* LvglDashboard::image(lv_obj_t* p,int x,int y,const lv_img_dsc_t* src,lv_opa_t opa){
    lv_obj_t* o=lv_img_create(p); lv_img_set_src(o,src); lv_obj_set_pos(o,x,y);
    lv_obj_set_style_img_opa(o,opa,0);
    lv_obj_clear_flag(o,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE); return o;
}
void LvglDashboard::setText(lv_obj_t* o,const char* fmt,...){ char b[96]; va_list ap; va_start(ap,fmt); vsnprintf(b,sizeof b,fmt,ap); va_end(ap); lv_label_set_text(o,b); }

// Night rule: swap outline<->filled. Day rule: dim with opacity.
void LvglDashboard::setIconState(lv_obj_t* icon,const lv_img_dsc_t* const pair[2],bool active){
    if(T_->nightRule){
        lv_img_set_src(icon,active?pair[1]:pair[0]);
        lv_obj_set_style_img_opa(icon,LV_OPA_COVER,0);
    } else {
        lv_img_set_src(icon,pair[1]);
        lv_obj_set_style_img_opa(icon,active?LV_OPA_COVER:LV_OPA_20,0);
    }
}

// alt/az for the tracked dec at a given hour angle; returns radar px + alt.
bool LvglDashboard::projectHA(double haH,double decDeg,double latDeg,int& px,int& py,double& altOut){
    double H=haH*15.0*M_PI/180.0, d=decDeg*M_PI/180.0, phi=latDeg*M_PI/180.0;
    double sa=sin(phi)*sin(d)+cos(phi)*cos(d)*cos(H);
    double alt=asin(fmax(-1.0,fmin(1.0,sa)));
    double y=-sin(H)*cos(d), x=sin(d)*cos(phi)-cos(d)*sin(phi)*cos(H);
    double az=atan2(y,x); if(az<0) az+=2*M_PI;           // 0=N, pi/2=E
    altOut=alt*180.0/M_PI;
    double r=RR*(1.0-fmax(0.0,altOut)/90.0);
    double screenA=az-M_PI/2.0;                           // N up on the frame
    px=RCX+int(cos(screenA)*r); py=RCY+int(sin(screenA)*r);
    return altOut>0.0;
}

// ---------------------------------------------------------------- lifecycle
const char* LvglDashboard::widgetName(int i){
    switch(i){case 0:return "SKY TRACK";case 1:return "MERIDIAN CLOCK";
              case 2:return "GUIDE SCOPE";default:return "GEM MOUNT";}
}
void LvglDashboard::begin(){ T_=&DAY; rebuild(); }

void LvglDashboard::setWidget(int idx){
    widgetIdx_=((idx%4)+4)%4;
    rebuild();
}
void LvglDashboard::buttonA(){
    if(screenIdx_!=2) return;
    if(settingsSel_==0) setWidget(widgetIdx_+1);
    else if(settingsSel_==1) setNight(!night_);
    else if(settingsSel_==2 && networkSetupHandler_) networkSetupHandler_();
}
void LvglDashboard::settingsMove(int delta){
    if(screenIdx_!=2 || !delta) return;
    settingsSel_=(settingsSel_+delta+3)%3;
    rebuild();
}

void LvglDashboard::setNight(bool on){
    if(on==night_) return;
    night_=on; T_= on? &NIGHT : &DAY;
    rebuild();
}
void LvglDashboard::rebuild(){
    lv_obj_t* oldH=home_, *oldD=diag_, *oldS=settings_;
    home_=lv_obj_create(nullptr); diag_=lv_obj_create(nullptr); settings_=lv_obj_create(nullptr);
    for(lv_obj_t* s:{home_,diag_,settings_}){
        lv_obj_set_style_bg_color(s,T_->bg,0); lv_obj_set_style_bg_opa(s,LV_OPA_COVER,0);
        lv_obj_clear_flag(s,LV_OBJ_FLAG_SCROLLABLE);
    }
    makeHome(); makeDiag(); makeSettings();
    lv_label_set_text(diagText_,diagCache_);
    lv_obj_t* scr = screenIdx_==0?home_ : screenIdx_==1?diag_ : settings_;
    lv_scr_load(scr);
    if(oldH) lv_obj_del(oldH);
    if(oldD) lv_obj_del(oldD);
    if(oldS) lv_obj_del(oldS);
}
void LvglDashboard::nextScreen(){
    screenIdx_=(screenIdx_+1)%3;
    lv_obj_t* scr = screenIdx_==0?home_ : screenIdx_==1?diag_ : settings_;
    // Guard: starting a screen-load animation while one is still running
    // crashes LVGL 8. Rapid presses fall back to an instant switch.
    uint32_t now=millis();
    if(now-lastSwitchMs_>250) lv_scr_load_anim(scr,LV_SCR_LOAD_ANIM_MOVE_LEFT,180,0,false);
    else                      lv_scr_load(scr);
    lastSwitchMs_=now;
}
void LvglDashboard::setDiagText(const char* t){
    snprintf(diagCache_,sizeof diagCache_,"%s",t);
    lv_label_set_text(diagText_,diagCache_);
}

// ---------------------------------------------------------------- build ----
void LvglDashboard::makeHome(){
    lv_obj_t* P=home_;
    lv_obj_t* bar=panel(P,3,2,314,23,7); lv_obj_set_style_bg_color(bar,T_->bg,0);
    wifiIcon_=image(P,7,3,T_->wifi[1]);
    linkIcon_=image(P,38,3,T_->link[1]);
    parkIcon_=image(P,71,3,T_->parkUnlocked[1]);
    guideIcon_=image(P,104,3,T_->guide[0],T_->nightRule?LV_OPA_COVER:LV_OPA_30);
    image(P,143,3,T_->battery); battery_=label(P,165,5,38,16,"--%",&lv_font_montserrat_14,T_->text);
    image(P,207,3,T_->temperature); temp_=label(P,229,5,42,16,"--C",&lv_font_montserrat_14,T_->text);
    slewImg_=image(P,270,3,T_->slew[1]); rate_=label(P,293,5,24,16,"-x",&lv_font_montserrat_14,T_->text);

    ribbon_=lv_obj_create(P); lv_obj_set_pos(ribbon_,3,26); lv_obj_set_size(ribbon_,314,2);
    lv_obj_set_style_border_width(ribbon_,0,0); lv_obj_set_style_radius(ribbon_,0,0);
    lv_obj_set_style_bg_color(ribbon_,T_->dim,0); lv_obj_set_style_bg_opa(ribbon_,LV_OPA_COVER,0);

    panel(P,3,31,234,56,8);
    label(P,10,38,-1,0,"RA",&lv_font_montserrat_12,T_->accent);
    raMain_=label(P,38,34,130,24,"--h --m",&lv_font_montserrat_20,T_->text,LV_TEXT_ALIGN_RIGHT);
    raSec_ =label(P,172,40,58,18,"--.-s",&lv_font_montserrat_14,T_->text);
    lv_obj_t* sep=lv_obj_create(P); lv_obj_set_pos(sep,10,58); lv_obj_set_size(sep,220,1);
    lv_obj_set_style_bg_color(sep,T_->edge,0); lv_obj_set_style_bg_opa(sep,LV_OPA_COVER,0); lv_obj_set_style_border_width(sep,0,0);
    label(P,10,66,-1,0,"DEC",&lv_font_montserrat_12,T_->accent);
    decMain_=label(P,38,62,130,24,"+--° --'",&lv_font_montserrat_20,T_->text,LV_TEXT_ALIGN_RIGHT);
    decSec_ =label(P,172,68,58,18,"--\"",&lv_font_montserrat_14,T_->text);
    stale_=label(P,214,36,-1,0,LV_SYMBOL_REFRESH,&lv_font_montserrat_12,T_->warn);
    lv_obj_add_flag(stale_,LV_OBJ_FLAG_HIDDEN);

    buildWidget(P);

    // metric cards
    image(P,3,92,T_->panelMetric); image(P,82,92,T_->panelMetric); image(P,161,92,T_->panelMetric);
    label(P,9,98,-1,0,"ALT",&lv_font_montserrat_12,T_->accent);  alt_=label(P,36,96,42,18,"--.-°",&lv_font_montserrat_14,T_->text);
    label(P,88,98,-1,0,"AZ",&lv_font_montserrat_12,T_->accent);  az_=label(P,108,96,48,18,"---.-°",&lv_font_montserrat_14,T_->text);
    label(P,167,98,-1,0,"PIER",&lv_font_montserrat_12,T_->accent); pier_=label(P,206,96,28,18,"--",&lv_font_montserrat_14,T_->text);

    // hour-angle bar
    panel(P,3,119,234,27,8);
    label(P,8,127,-1,0,"E",&lv_font_montserrat_12,T_->accent);
    label(P,BAR_CX-4,121,-1,0,"M",&lv_font_montserrat_10,T_->dim);
    label(P,180,127,-1,0,"W",&lv_font_montserrat_12,T_->accent);
    lv_obj_t* track=lv_obj_create(P); lv_obj_set_pos(track,BAR_X0,BAR_Y); lv_obj_set_size(track,BAR_X1-BAR_X0,2);
    lv_obj_set_style_bg_color(track,T_->edge,0); lv_obj_set_style_bg_opa(track,LV_OPA_COVER,0); lv_obj_set_style_border_width(track,0,0); lv_obj_set_style_radius(track,0,0);
    lv_obj_t* mtick=lv_obj_create(P); lv_obj_set_pos(mtick,BAR_CX,BAR_Y-4); lv_obj_set_size(mtick,1,10);
    lv_obj_set_style_bg_color(mtick,T_->dim,0); lv_obj_set_style_bg_opa(mtick,LV_OPA_COVER,0); lv_obj_set_style_border_width(mtick,0,0); lv_obj_set_style_radius(mtick,0,0);
    haDot_=lv_obj_create(P); lv_obj_set_size(haDot_,9,9);
    lv_obj_clear_flag(haDot_,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(haDot_,LV_RADIUS_CIRCLE,0);
    lv_obj_set_style_bg_color(haDot_,T_->accent,0); lv_obj_set_style_bg_opa(haDot_,LV_OPA_COVER,0); lv_obj_set_style_border_width(haDot_,0,0);
    label(P,196,121,40,10,"T-MER",&lv_font_montserrat_10,T_->dim,LV_TEXT_ALIGN_RIGHT);
    tmer_=label(P,192,131,44,14,"--:--",&lv_font_montserrat_14,T_->text,LV_TEXT_ALIGN_RIGHT);

    // adaptive panel
    panel(P,244,106,73,40,6);
    adTitle_=label(P,250,109,-1,0,"NET",&lv_font_montserrat_10,T_->accent);
    adLine1_=label(P,250,119,62,12,"---dBm",&lv_font_montserrat_10,T_->text);
    adLine2_=label(P,250,131,62,12,"--ms --%",&lv_font_montserrat_10,T_->text);
    for(int i=0;i<20;i++){guidePts_[i].x=250+i*3; guidePts_[i].y=132;}
    guideLine_=lv_line_create(P); lv_line_set_points(guideLine_,guidePts_,20);
    lv_obj_set_style_line_color(guideLine_,T_->good,0); lv_obj_set_style_line_width(guideLine_,1,0);
    lv_obj_add_flag(guideLine_,LV_OBJ_FLAG_HIDDEN);

    // nav
    const char* names[]={"HOME","GOTO","ALIGN","MENU"};
    for(int i=0;i<4;i++){
        int x=3+i*79;
        panel(P,x,151,76,16,5);
        if(i==0){ lv_obj_t* tab=lv_obj_create(P); lv_obj_set_pos(tab,x+6,151); lv_obj_set_size(tab,64,2);
                  lv_obj_set_style_bg_color(tab,T_->accent,0); lv_obj_set_style_bg_opa(tab,LV_OPA_COVER,0); lv_obj_set_style_border_width(tab,0,0); lv_obj_set_style_radius(tab,0,0); }
        image(P,x+6,152,T_->nav[i][i==0?1:0],(!T_->nightRule&&i!=0)?LV_OPA_60:LV_OPA_COVER);
        label(P,x+24,154,48,12,names[i],&lv_font_montserrat_10,i==0?T_->accent:T_->dim);
    }
}

// ================= radar-region widgets: build =================
void LvglDashboard::buildWidget(lv_obj_t* P){
    // reset pointers that update() checks
    radarDot_=nullptr; clkArc_=nullptr; gsCrossV_=nullptr; gmPier_=nullptr; comet_=nullptr;
    switch(widgetIdx_){
    case 0: { // SKY TRACK
        image(P,244,29,T_->radarFrame);
        pastLine_=lv_line_create(P); lv_obj_set_style_line_color(pastLine_,T_->dim,0);
        lv_obj_set_style_line_width(pastLine_,1,0); lv_obj_set_style_line_opa(pastLine_,LV_OPA_60,0);
        trackLine_=lv_line_create(P); lv_obj_set_style_line_color(trackLine_,T_->accent,0);
        lv_obj_set_style_line_width(trackLine_,2,0); lv_obj_set_style_line_rounded(trackLine_,true,0);
        trackLowLine_=lv_line_create(P); lv_obj_set_style_line_color(trackLowLine_,T_->warn,0);
        lv_obj_set_style_line_width(trackLowLine_,2,0); lv_obj_set_style_line_rounded(trackLowLine_,true,0);
        merMark_=lv_obj_create(P); lv_obj_set_size(merMark_,5,5);
        lv_obj_clear_flag(merMark_,LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_radius(merMark_,1,0); lv_obj_set_style_border_width(merMark_,1,0);
        lv_obj_set_style_border_color(merMark_,T_->text,0); lv_obj_set_style_bg_opa(merMark_,LV_OPA_0,0);
        lv_obj_add_flag(merMark_,LV_OBJ_FLAG_HIDDEN);
        for(int i=0;i<6;i++){
            trail_[i]=lv_obj_create(P); lv_obj_set_size(trail_[i],3,3);
            lv_obj_clear_flag(trail_[i],LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_radius(trail_[i],LV_RADIUS_CIRCLE,0);
            lv_obj_set_style_bg_color(trail_[i],T_->trail,0);
            lv_obj_set_style_border_width(trail_[i],0,0);
            lv_obj_set_style_bg_opa(trail_[i],(lv_opa_t)(40+i*30),0);
            lv_obj_add_flag(trail_[i],LV_OBJ_FLAG_HIDDEN);
        }
        auto mkcomet=[&](int sz,lv_color_t col){
            lv_obj_t* o=lv_obj_create(P); lv_obj_set_size(o,sz,sz);
            lv_obj_clear_flag(o,LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_radius(o,LV_RADIUS_CIRCLE,0);
            lv_obj_set_style_bg_color(o,col,0);
            lv_obj_set_style_bg_opa(o,LV_OPA_COVER,0);
            lv_obj_set_style_border_width(o,0,0);
            lv_obj_add_flag(o,LV_OBJ_FLAG_HIDDEN);
            return o;
        };
        comet3_=mkcomet(3,T_->sweep2);
        comet2_=mkcomet(4,T_->sweep1);
        comet_ =mkcomet(5,T_->text);
        radarDot_=lv_obj_create(P); lv_obj_set_size(radarDot_,9,9);
        lv_obj_clear_flag(radarDot_,LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_radius(radarDot_,LV_RADIUS_CIRCLE,0);
        lv_obj_set_style_bg_color(radarDot_,T_->good,0);
        lv_obj_set_style_bg_opa(radarDot_,LV_OPA_COVER,0);
        lv_obj_set_style_border_color(radarDot_,T_->text,0);
        lv_obj_set_style_border_width(radarDot_,1,0);
        break; }
    case 1: { // MERIDIAN CLOCK
        image(P,244,29,T_->radarFrame);
        clkArc_=lv_arc_create(P);
        lv_obj_set_size(clkArc_,60,60); lv_obj_set_pos(clkArc_,250,35);
        lv_arc_set_rotation(clkArc_,270);          // 0 at top = meridian
        lv_arc_set_bg_angles(clkArc_,0,360);
        lv_obj_remove_style(clkArc_,nullptr,LV_PART_KNOB);
        lv_obj_set_style_arc_opa(clkArc_,LV_OPA_0,LV_PART_MAIN);
        lv_obj_set_style_arc_color(clkArc_,T_->accent,LV_PART_INDICATOR);
        lv_obj_set_style_arc_width(clkArc_,3,LV_PART_INDICATOR);
        lv_obj_set_style_arc_rounded(clkArc_,true,LV_PART_INDICATOR);
        lv_obj_clear_flag(clkArc_,LV_OBJ_FLAG_CLICKABLE);
        clkNeedle_=lv_line_create(P);
        clkNeedlePts_[0]={280,65}; clkNeedlePts_[1]={280,40};
        lv_line_set_points(clkNeedle_,clkNeedlePts_,2);
        lv_obj_set_style_line_color(clkNeedle_,T_->text,0);
        lv_obj_set_style_line_width(clkNeedle_,2,0);
        lv_obj_set_style_line_rounded(clkNeedle_,true,0);
        auto mkdot=[&](int sz,lv_color_t col){
            lv_obj_t* o=lv_obj_create(P); lv_obj_set_size(o,sz,sz);
            lv_obj_clear_flag(o,LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_radius(o,LV_RADIUS_CIRCLE,0);
            lv_obj_set_style_bg_color(o,col,0);
            lv_obj_set_style_bg_opa(o,LV_OPA_COVER,0);
            lv_obj_set_style_border_width(o,0,0);
            return o;
        };
        clkLst2_=mkdot(3,T_->sweep2);
        clkLst_ =mkdot(4,T_->accent);
        clkCap_=label(P,258,52,44,10,"T-MER",&lv_font_montserrat_10,T_->dim,LV_TEXT_ALIGN_CENTER);
        clkText_=label(P,256,70,48,14,"--:--",&lv_font_montserrat_12,T_->text,LV_TEXT_ALIGN_CENTER);
        break; }
    case 2: { // GUIDE SCOPE
        image(P,244,29,T_->radarFrame);
        gsCrossV_=lv_line_create(P);
        gsVPts_[0]={280,40}; gsVPts_[1]={280,90};
        lv_line_set_points(gsCrossV_,gsVPts_,2);
        gsCrossH_=lv_line_create(P);
        gsHPts_[0]={255,65}; gsHPts_[1]={305,65};
        lv_line_set_points(gsCrossH_,gsHPts_,2);
        for(lv_obj_t* o:{gsCrossV_,gsCrossH_}){
            lv_obj_set_style_line_color(o,T_->edge,0); lv_obj_set_style_line_width(o,1,0);
        }
        gsRipple_=lv_obj_create(P); lv_obj_set_size(gsRipple_,8,8);
        lv_obj_clear_flag(gsRipple_,LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_radius(gsRipple_,LV_RADIUS_CIRCLE,0);
        lv_obj_set_style_bg_opa(gsRipple_,LV_OPA_0,0);
        lv_obj_set_style_border_color(gsRipple_,T_->good,0);
        lv_obj_set_style_border_width(gsRipple_,1,0);
        lv_obj_add_flag(gsRipple_,LV_OBJ_FLAG_HIDDEN);
        gsStar_=lv_obj_create(P); lv_obj_set_size(gsStar_,5,5);
        lv_obj_clear_flag(gsStar_,LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_radius(gsStar_,LV_RADIUS_CIRCLE,0);
        lv_obj_set_style_bg_color(gsStar_,T_->text,0);
        lv_obj_set_style_bg_opa(gsStar_,LV_OPA_COVER,0);
        lv_obj_set_style_border_width(gsStar_,0,0);
        // RA / DEC correction kick bars under the circle
        gsBarRA_=lv_obj_create(P); lv_obj_set_pos(gsBarRA_,250,99); lv_obj_set_size(gsBarRA_,2,2);
        gsBarDE_=lv_obj_create(P); lv_obj_set_pos(gsBarDE_,286,99); lv_obj_set_size(gsBarDE_,2,2);
        for(lv_obj_t* o:{gsBarRA_,gsBarDE_}){
            lv_obj_clear_flag(o,LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_radius(o,1,0); lv_obj_set_style_border_width(o,0,0);
            lv_obj_set_style_bg_color(o,T_->good,0); lv_obj_set_style_bg_opa(o,LV_OPA_COVER,0);
        }
        gsCap_=label(P,252,99,60,10,"RA      DE",&lv_font_montserrat_10,T_->dim);
        break; }
    default: { // GEM MOUNT
        panel(P,244,29,73,73,8);
        gmCap_=label(P,252,33,58,10,"MOUNT",&lv_font_montserrat_10,T_->dim);
        gmPier_=lv_line_create(P);
        lv_obj_set_style_line_color(gmPier_,T_->dim,0);
        lv_obj_set_style_line_width(gmPier_,2,0);
        lv_obj_set_style_line_rounded(gmPier_,true,0);
        gmBar_=lv_line_create(P);
        lv_obj_set_style_line_color(gmBar_,T_->accent,0);
        lv_obj_set_style_line_width(gmBar_,2,0);
        lv_obj_set_style_line_rounded(gmBar_,true,0);
        gmWeight_=lv_obj_create(P); lv_obj_set_size(gmWeight_,5,5);
        lv_obj_clear_flag(gmWeight_,LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_radius(gmWeight_,LV_RADIUS_CIRCLE,0);
        lv_obj_set_style_bg_color(gmWeight_,T_->accent,0);
        lv_obj_set_style_bg_opa(gmWeight_,LV_OPA_COVER,0);
        lv_obj_set_style_border_width(gmWeight_,0,0);
        gmTube_=lv_line_create(P);
        lv_obj_set_style_line_color(gmTube_,T_->text,0);
        lv_obj_set_style_line_width(gmTube_,4,0);
        lv_obj_set_style_line_rounded(gmTube_,true,0);
        gmShine_=lv_obj_create(P); lv_obj_set_size(gmShine_,3,3);
        lv_obj_clear_flag(gmShine_,LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_radius(gmShine_,LV_RADIUS_CIRCLE,0);
        lv_obj_set_style_bg_color(gmShine_,T_->text,0);
        lv_obj_set_style_bg_opa(gmShine_,LV_OPA_COVER,0);
        lv_obj_set_style_border_width(gmShine_,0,0);
        lv_obj_add_flag(gmShine_,LV_OBJ_FLAG_HIDDEN);
        gmDrive_=lv_obj_create(P); lv_obj_set_size(gmDrive_,4,4);
        lv_obj_clear_flag(gmDrive_,LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_radius(gmDrive_,LV_RADIUS_CIRCLE,0);
        lv_obj_set_style_bg_color(gmDrive_,T_->good,0);
        lv_obj_set_style_bg_opa(gmDrive_,LV_OPA_COVER,0);
        lv_obj_set_style_border_width(gmDrive_,0,0);
        lv_obj_add_flag(gmDrive_,LV_OBJ_FLAG_HIDDEN);
        break; }
    }
}

void LvglDashboard::makeDiag(){
    lv_obj_t* P=diag_;
    panel(P,3,3,314,20,6);
    label(P,10,7,-1,0,"DIAGNOSTICS",&lv_font_montserrat_12,T_->accent);
    label(P,200,7,114,14,"START = next",&lv_font_montserrat_10,T_->dim,LV_TEXT_ALIGN_RIGHT);
    panel(P,3,27,314,140,8);
    diagText_=label(P,12,34,296,126,"collecting...",&lv_font_montserrat_12,T_->text);
    lv_label_set_long_mode(diagText_,LV_LABEL_LONG_CLIP);
}

void LvglDashboard::makeSettings(){
    lv_obj_t* P=settings_;
    panel(P,3,3,314,20,6);
    label(P,10,7,-1,0,"SETTINGS",&lv_font_montserrat_12,T_->accent);
    label(P,170,7,144,14,"A = change   START = next",&lv_font_montserrat_10,T_->dim,LV_TEXT_ALIGN_RIGHT);
    panel(P,3,27,314,120,8);
    const char* names[3]={"RADAR WIDGET","NIGHT MODE","WI-FI / ONSTEP SETUP"};
    String values[3]={widgetName(widgetIdx_),night_?"ON (red)":"OFF (day)","A: open setup AP"};
    for(int i=0;i<3;i++){
        bool selected=i==settingsSel_;
        lv_obj_t* row=panel(P,10,34+i*35,300,29,5);
        if(selected){ lv_obj_set_style_border_color(row,T_->accent,0); lv_obj_set_style_bg_color(row,T_->edge,0); }
        label(P,18,38+i*35,150,12,names[i],&lv_font_montserrat_10,selected?T_->accent:T_->dim);
        label(P,18,50+i*35,282,13,values[i].c_str(),&lv_font_montserrat_12,T_->text);
    }
    label(P,3,152,314,14,"stick: select   A: change   START: next",&lv_font_montserrat_10,T_->dim,LV_TEXT_ALIGN_CENTER);
}

// ---------------------------------------------------------------- update ---
void LvglDashboard::update(const MountStatus& m,const UiExtras& ex){
    setText(rate_,"%dx",m.slewRate); setText(battery_,"%d%%",ex.batteryPct); setText(temp_,"%.0fC",ex.tempC);
    setIconState(wifiIcon_,T_->wifi,ex.wifiBars>0);
    setIconState(linkIcon_,T_->link,ex.online);
    if(m.parked){ if(T_->nightRule) lv_img_set_src(parkIcon_,T_->parkLocked[0]);
                  else { lv_img_set_src(parkIcon_,T_->parkLocked[1]); lv_obj_set_style_img_opa(parkIcon_,LV_OPA_COVER,0);} }
    else        { lv_img_set_src(parkIcon_,T_->parkUnlocked[1]); lv_obj_set_style_img_opa(parkIcon_,LV_OPA_COVER,0); }
    setIconState(guideIcon_,T_->guide,m.guidePulseActive);
    setIconState(slewImg_,T_->slew,m.goingTo||m.slewing||m.tracking);

    lv_color_t rc=T_->dim;
    if(!ex.online)                rc=T_->edge;
    else if(m.parked)             rc=T_->warn;
    else if(m.goingTo||m.slewing) rc=T_->warn;
    else if(m.tracking)           rc=T_->good;
    else                          rc=T_->accent;
    lv_obj_set_style_bg_color(ribbon_,rc,0);

    int total=int(lround(fmod((m.raHours+24.0),24.0)*3600.0)); int rh=total/3600,rm=(total/60)%60;
    double rs=fmod(m.raHours*3600.0,60.0); if(rs<0)rs+=60;
    setText(raMain_,"%02dh %02dm",rh,rm); setText(raSec_,"%04.1fs",rs);
    double ad=fabs(m.decDeg); int dd=int(ad),dm=int((ad-dd)*60.0),ds=int(lround((((ad-dd)*60.0)-dm)*60.0)); if(ds==60){ds=0;dm++;}
    setText(decMain_,"%c%02d° %02d'",m.decDeg<0?'-':'+',dd,dm); setText(decSec_,"%02d\"",ds);

    bool stale=ex.dataAgeMs>3000||!ex.online;
    lv_color_t vc=stale?T_->dim:T_->text;
    for(lv_obj_t* o:{raMain_,raSec_,decMain_,decSec_,alt_,az_}) lv_obj_set_style_text_color(o,vc,0);
    if(stale) lv_obj_clear_flag(stale_,LV_OBJ_FLAG_HIDDEN); else lv_obj_add_flag(stale_,LV_OBJ_FLAG_HIDDEN);

    setText(alt_,"%4.1f°",m.altDeg); setText(az_,"%5.1f°",m.azDeg);
    setText(pier_,m.pierSide==1?"E":m.pierSide==2?"W":"--");

    updateWidget(m,ex);

    // hour-angle bar (eased) + T-MER
    if(m.localSiderealHours>=0){
        double ha=m.localSiderealHours-m.raHours; while(ha>12)ha-=24; while(ha<-12)ha+=24;
        double clamped=fmax(-6.0,fmin(6.0,ha));
        float txp=BAR_CX+float(clamped/6.0*(BAR_X1-BAR_X0)/2.0);
        haFx_+=(txp-haFx_)*0.35f;
        lv_obj_set_pos(haDot_,int(haFx_)-4,BAR_Y-4);
        double hrsTo=-ha; bool past=hrsTo<0; double ab=fabs(hrsTo);
        int hh=int(ab),mm=int((ab-hh)*60);
        setText(tmer_,"%c%02d:%02d",past?'-':'+',hh,mm);
        lv_color_t wc=past?T_->alert:(ab<0.25?T_->warn:ab<1.0?T_->warn:T_->accent);
        lv_obj_set_style_bg_color(haDot_,wc,0);
        lv_obj_set_style_text_color(tmer_,wc,0);
        if(past||ab<0.25){
            lv_opa_t o=(lv_opa_t)(150+105.0*sin(millis()/250.0));
            lv_obj_set_style_bg_opa(haDot_,o,0); lv_obj_set_style_text_opa(tmer_,o,0);
        } else { lv_obj_set_style_bg_opa(haDot_,LV_OPA_COVER,0); lv_obj_set_style_text_opa(tmer_,LV_OPA_COVER,0); }
    } else { setText(tmer_,"--:--"); lv_obj_set_style_text_color(tmer_,T_->dim,0); }

    // adaptive panel
    if(m.guidePulseActive){
        lv_label_set_text(adTitle_,"PULSE");
        lv_obj_add_flag(adLine1_,LV_OBJ_FLAG_HIDDEN); lv_obj_add_flag(adLine2_,LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(guideLine_,LV_OBJ_FLAG_HIDDEN);
        double sample=sin(millis()/180.0)*1.5;
        guideHistory_[guideHead_]=sample; guideHead_=(guideHead_+1)%20;
        for(int i=0;i<20;i++){double v=guideHistory_[(guideHead_+i)%20]; guidePts_[i].y=(lv_coord_t)(132-int(v*3.0));}
        lv_line_set_points(guideLine_,guidePts_,20);
    } else {
        lv_label_set_text(adTitle_,"NET");
        lv_obj_add_flag(guideLine_,LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(adLine1_,LV_OBJ_FLAG_HIDDEN); lv_obj_clear_flag(adLine2_,LV_OBJ_FLAG_HIDDEN);
        setText(adLine1_,"%d dBm",ex.rssi);
        setText(adLine2_,"%ums %u%%",(unsigned)ex.latMs,(unsigned)ex.okPct);
        lv_obj_set_style_text_color(adLine2_,ex.okPct>=95?T_->text:ex.okPct>=80?T_->warn:T_->alert,0);
    }
}

// ================= radar-region widgets: update =================
void LvglDashboard::updateWidget(const MountStatus& m,const UiExtras& ex){
    switch(widgetIdx_){
    case 0: { // SKY TRACK: past trail, future arc, meridian mark, comet direction cue
        double a=(m.azDeg-90.0)*M_PI/180.0; double r=RR*(1.0-fmax(0.0,fmin(90.0,m.altDeg))/90.0);
        float tx=RCX+cos(a)*r, ty=RCY+sin(a)*r;
        dotFx_+=(tx-dotFx_)*0.35f; dotFy_+=(ty-dotFy_)*0.35f;
        lv_obj_set_pos(radarDot_,int(dotFx_)-4,int(dotFy_)-4);
        trackN_=0;
        if(m.siteLatDeg<=90.0 && m.localSiderealHours>=0){
            double ha0=m.localSiderealHours-m.raHours; while(ha0>12)ha0-=24; while(ha0<-12)ha0+=24;
            int nAll=0,nLow=0; bool merShown=false;
            for(int k=0;k<=8;k++){
                double ha=ha0+k*0.5; int px,py; double altv;
                bool up=projectHA(ha,m.decDeg,m.siteLatDeg,px,py,altv);
                if(!up) break;
                if(altv>=15.0&&nLow==0){ trackPts_[nAll].x=px; trackPts_[nAll].y=py; nAll++; }
                else { if(nLow==0&&nAll>0){trackLowPts_[0]=trackPts_[nAll-1]; nLow=1;}
                       trackLowPts_[nLow].x=px; trackLowPts_[nLow].y=py; nLow++;
                       if(nLow>=9)break; }
                if(!merShown&&ha0<0&&ha>=0){
                    int mx,my; double malt;
                    projectHA(0,m.decDeg,m.siteLatDeg,mx,my,malt);
                    lv_obj_set_pos(merMark_,mx-2,my-2);
                    lv_obj_clear_flag(merMark_,LV_OBJ_FLAG_HIDDEN);
                    merShown=true;
                }
            }
            if(!merShown) lv_obj_add_flag(merMark_,LV_OBJ_FLAG_HIDDEN);
            int nPast=0;
            for(int k=3;k>=0;k--){
                double ha=ha0-k*0.5; int px,py; double altv;
                if(projectHA(ha,m.decDeg,m.siteLatDeg,px,py,altv)){ pastPts_[nPast].x=px; pastPts_[nPast].y=py; nPast++; }
            }
            trackN_=nAll;
            if(nAll>=2){ lv_line_set_points(trackLine_,trackPts_,nAll); lv_obj_clear_flag(trackLine_,LV_OBJ_FLAG_HIDDEN); }
            else lv_obj_add_flag(trackLine_,LV_OBJ_FLAG_HIDDEN);
            if(nLow>=2){ lv_line_set_points(trackLowLine_,trackLowPts_,nLow); lv_obj_clear_flag(trackLowLine_,LV_OBJ_FLAG_HIDDEN); }
            else lv_obj_add_flag(trackLowLine_,LV_OBJ_FLAG_HIDDEN);
            if(nPast>=2){ lv_line_set_points(pastLine_,pastPts_,nPast); lv_obj_clear_flag(pastLine_,LV_OBJ_FLAG_HIDDEN); }
            else lv_obj_add_flag(pastLine_,LV_OBJ_FLAG_HIDDEN);
        } else {
            for(lv_obj_t* o:{trackLine_,trackLowLine_,pastLine_,merMark_}) lv_obj_add_flag(o,LV_OBJ_FLAG_HIDDEN);
        }
        // comet: a bright spark repeatedly travelling the future arc = direction of travel
        if(trackN_>=2 && ex.online){
            auto place=[&](lv_obj_t* o,float phase01,int half){
                float ph=phase01*(trackN_-1);
                int i0=int(ph); if(i0>=trackN_-1)i0=trackN_-2;
                float fr=ph-i0;
                float cxp=trackPts_[i0].x+(trackPts_[i0+1].x-trackPts_[i0].x)*fr;
                float cyp=trackPts_[i0].y+(trackPts_[i0+1].y-trackPts_[i0].y)*fr;
                lv_obj_set_pos(o,int(cxp)-half,int(cyp)-half);
                lv_obj_clear_flag(o,LV_OBJ_FLAG_HIDDEN);
            };
            float p=(millis()%2600)/2600.0f;
            place(comet_,p,2);
            place(comet2_,fmaxf(0.f,p-0.06f),2);
            place(comet3_,fmaxf(0.f,p-0.12f),1);
        } else for(lv_obj_t* o:{comet_,comet2_,comet3_}) lv_obj_add_flag(o,LV_OBJ_FLAG_HIDDEN);
        // breadcrumbs while moving
        int dx=int(dotFx_)-1,dy=int(dotFy_)-1;
        bool moving=(abs(dx-lastDotX_)+abs(dy-lastDotY_))>1;
        if(moving&&millis()-lastTrailMs_>250){
            lastTrailMs_=millis();
            lv_obj_set_pos(trail_[trailHead_],lastDotX_,lastDotY_);
            lv_obj_clear_flag(trail_[trailHead_],LV_OBJ_FLAG_HIDDEN);
            trailHead_=(trailHead_+1)%6;
        }
        if(!moving&&millis()-lastTrailMs_>1500)
            for(int i=0;i<6;i++) lv_obj_add_flag(trail_[i],LV_OBJ_FLAG_HIDDEN);
        lastDotX_=dx; lastDotY_=dy;
        break; }
    case 1: { // MERIDIAN CLOCK: needle=HA, draining arc to transit, LST orbiter
        if(m.localSiderealHours>=0){
            double ha=m.localSiderealHours-m.raHours; while(ha>12)ha-=24; while(ha<-12)ha+=24;
            double clamped=fmax(-6.0,fmin(6.0,ha));
            float target=float(clamped/6.0*135.0);          // -135..+135 deg from top
            clkAngF_+=(target-clkAngF_)*0.25f;
            double th=clkAngF_*M_PI/180.0;
            clkNeedlePts_[0]={RCX,RCY};
            clkNeedlePts_[1]={(lv_coord_t)(RCX+int(sin(th)*24)),(lv_coord_t)(RCY-int(cos(th)*24))};
            lv_line_set_points(clkNeedle_,clkNeedlePts_,2);
            // arc from needle to meridian (top): drains as transit nears
            if(clkAngF_<0){ lv_arc_set_angles(clkArc_,(uint16_t)(360+clkAngF_),360); }
            else          { lv_arc_set_angles(clkArc_,0,(uint16_t)clkAngF_); }
            double hrsTo=-ha; bool past=hrsTo<0; double ab=fabs(hrsTo);
            int hh=int(ab),mm=int((ab-hh)*60);
            setText(clkText_,"%c%01d:%02d",past?'-':'+',hh,mm);
            lv_color_t wc=past?T_->alert:(ab<1.0?T_->warn:T_->accent);
            lv_obj_set_style_arc_color(clkArc_,wc,LV_PART_INDICATOR);
            lv_obj_set_style_text_color(clkText_,wc,0);
            lv_obj_set_style_line_color(clkNeedle_,(past||ab<0.25)?wc:T_->text,0);
            if(past||ab<0.25){
                lv_opa_t o=(lv_opa_t)(150+105.0*sin(millis()/250.0));
                lv_obj_set_style_line_opa(clkNeedle_,o,0);
            } else lv_obj_set_style_line_opa(clkNeedle_,LV_OPA_COVER,0);
        }
        { // LST orbiter with tail: one lap / 12 s
            double oa=(millis()%12000)/12000.0*2*M_PI-M_PI/2;
            lv_obj_set_pos(clkLst_, RCX+int(cos(oa)*30)-2, RCY+int(sin(oa)*30)-2);
            double ob=oa-0.22;
            lv_obj_set_pos(clkLst2_,RCX+int(cos(ob)*30)-1, RCY+int(sin(ob)*30)-1);
            // arc breathes gently so the remaining-time band reads as live
            lv_opa_t bo=(lv_opa_t)(200+55.0*sin(millis()/900.0));
            lv_obj_set_style_arc_opa(clkArc_,bo,LV_PART_INDICATOR);
        }
        break; }
    case 2: { // GUIDE SCOPE: star wanders with pulse activity, ripple on pulse edge
        bool act=m.guidePulseActive&&ex.online;
        // pseudo-random wander driven by two incommensurate sines
        float amp=act?7.0f:1.2f;
        float wx=amp*sin(millis()/430.0f)*sin(millis()/171.0f);
        float wy=amp*sin(millis()/377.0f)*cos(millis()/233.0f);
        gsX_+=(wx-gsX_)*0.3f; gsY_+=(wy-gsY_)*0.3f;
        lv_obj_set_pos(gsStar_,RCX+int(gsX_)-2,RCY+int(gsY_)-2);
        lv_obj_set_style_bg_color(gsStar_,act?T_->good:T_->text,0);
        // ripple ring on pulse rising edge, expands + fades 600 ms
        if(act&&!gsPulsePrev_) gsRippleT0_=millis();
        gsPulsePrev_=act;
        uint32_t dt=millis()-gsRippleT0_;
        if(gsRippleT0_&&dt<600){
            int rr=4+int(dt*0.04);
            lv_obj_set_size(gsRipple_,rr*2,rr*2);
            lv_obj_set_pos(gsRipple_,RCX+int(gsX_)-rr,RCY+int(gsY_)-rr);
            lv_obj_set_style_border_opa(gsRipple_,(lv_opa_t)(255-dt*255/600),0);
            lv_obj_clear_flag(gsRipple_,LV_OBJ_FLAG_HIDDEN);
        } else lv_obj_add_flag(gsRipple_,LV_OBJ_FLAG_HIDDEN);
        // correction kick bars
        int hRA=act?2+abs(int(8*sin(millis()/301.0f))):2;
        int hDE=act?2+abs(int(8*sin(millis()/257.0f))):2;
        lv_obj_set_size(gsBarRA_,14,hRA); lv_obj_set_pos(gsBarRA_,254,110-hRA);
        lv_obj_set_size(gsBarDE_,14,hDE); lv_obj_set_pos(gsBarDE_,290,110-hDE);
        break; }
    default: { // GEM MOUNT: counterweight bar swings with HA, tube flips with pier
        double ha=0;
        if(m.localSiderealHours>=0){ ha=m.localSiderealHours-m.raHours; while(ha>12)ha-=24; while(ha<-12)ha+=24; }
        float tgt=m.parked? 0.0f : float(fmax(-6.0,fmin(6.0,ha))/6.0*75.0);   // deg from vertical
        gmAngF_+=(tgt-gmAngF_)*((m.goingTo||m.slewing)?0.5f:0.15f);
        int cx=280, base=95, hub_y=68;
        // pier: tripod + column
        gmPierPts_[0]={(lv_coord_t)(cx-14),(lv_coord_t)base};
        gmPierPts_[1]={(lv_coord_t)cx,(lv_coord_t)(base-12)};
        gmPierPts_[2]={(lv_coord_t)(cx+14),(lv_coord_t)base};
        gmPierPts_[3]={(lv_coord_t)cx,(lv_coord_t)(base-12)};
        gmPierPts_[4]={(lv_coord_t)cx,(lv_coord_t)hub_y};
        lv_line_set_points(gmPier_,gmPierPts_,5);
        // counterweight bar through hub at gmAngF_ from vertical
        double th=gmAngF_*M_PI/180.0;
        float bx=sin(th), by=-cos(th);
        gmBarPts_[0]={(lv_coord_t)(cx+int(bx*16)),(lv_coord_t)(hub_y+int(by*16))};
        gmBarPts_[1]={(lv_coord_t)(cx-int(bx*12)),(lv_coord_t)(hub_y-int(by*12))};
        lv_line_set_points(gmBar_,gmBarPts_,2);
        lv_obj_set_pos(gmWeight_,cx-int(bx*12)-2,hub_y-int(by*12)-2);
        // tube: perpendicular to bar at the top end; flips with pier side
        int dir=(m.pierSide==2)?-1:1;
        float px=-by*dir, py=bx*dir;
        int tx0=cx+int(bx*16), ty0=hub_y+int(by*16);
        if(m.parked){ px=0; py=-1; }                    // folded up when parked
        gmTubePts_[0]={(lv_coord_t)(tx0-int(px*9)),(lv_coord_t)(ty0-int(py*9))};
        gmTubePts_[1]={(lv_coord_t)(tx0+int(px*11)),(lv_coord_t)(ty0+int(py*11))};
        lv_line_set_points(gmTube_,gmTubePts_,2);
        lv_obj_set_style_line_color(gmTube_,m.parked?T_->dim:(m.tracking?T_->good:T_->text),0);
        setText(gmCap_,m.parked?"PARKED":(m.pierSide==2?"PIER W":"PIER E"));
        // RA drive indicator: small tick orbiting the hub while tracking
        // (exaggerated sidereal motion; communicates 'drive running')
        if(m.tracking&&!m.parked&&ex.online){
            double da=(millis()%8000)/8000.0*2*M_PI;
            lv_obj_set_pos(gmDrive_,cx+int(cos(da)*7)-2,hub_y+int(sin(da)*7)-2);
            lv_obj_clear_flag(gmDrive_,LV_OBJ_FLAG_HIDDEN);
            // shine slides along the tube: 'optics alive' cue
            float sp=(millis()%2500)/2500.0f;
            float sx=gmTubePts_[0].x+(gmTubePts_[1].x-gmTubePts_[0].x)*sp;
            float sy=gmTubePts_[0].y+(gmTubePts_[1].y-gmTubePts_[0].y)*sp;
            lv_obj_set_pos(gmShine_,int(sx)-1,int(sy)-1);
            lv_obj_clear_flag(gmShine_,LV_OBJ_FLAG_HIDDEN);
        } else { lv_obj_add_flag(gmDrive_,LV_OBJ_FLAG_HIDDEN); lv_obj_add_flag(gmShine_,LV_OBJ_FLAG_HIDDEN); }
        break; }
    }
}
