#include "App.h"
#include "Theme.h"
#include "Fonts.h"
#include "UI.h"
#include <cmath>
#include <cstdio>
#include <string>

// ---------- small helpers ----------
static std::string raToStr(double hours) {
    hours = std::fmod(hours, 24.0);
    if (hours < 0) hours += 24.0;
    long totalSeconds = std::lround(hours * 3600.0) % (24L * 3600L);
    const int hh = static_cast<int>(totalSeconds / 3600L);
    const int mm = static_cast<int>((totalSeconds / 60L) % 60L);
    const int ss = static_cast<int>(totalSeconds % 60L);
    char buf[16];
    std::snprintf(buf, sizeof buf, "%02d:%02d:%02d", hh, mm, ss);
    return buf;
}

static std::string decToStr(double degrees) {
    const char sign = degrees < 0 ? '-' : '+';
    long totalArcsec = std::lround(std::fabs(degrees) * 3600.0);
    const int dd = static_cast<int>(totalArcsec / 3600L);
    const int mm = static_cast<int>((totalArcsec / 60L) % 60L);
    const int ss = static_cast<int>(totalArcsec % 60L);
    char buf[16];
    std::snprintf(buf, sizeof buf, "%c%02d:%02d:%02d", sign, dd, mm, ss);
    return buf;
}

struct MenuItem { const char* label; ConfirmAction action; bool isMode; bool isClose; bool resumeGuide; };
static const MenuItem kMenu[] = {
    {"RESUME GUIDE", ConfirmAction::None,    false, false, true },
    {"PARK",         ConfirmAction::Park,    false, false, false},
    {"UNPARK",       ConfirmAction::Unpark,  false, false, false},
    {"SET HOME",     ConfirmAction::SetHome, false, false, false},
    {"GO HOME",      ConfirmAction::GoHome,  false, false, false},
    {"TOGGLE MODE",  ConfirmAction::None,    true,  false, false},
    {"CLOSE",        ConfirmAction::None,    false, true,  false},
};
static const int kMenuCount = static_cast<int>(sizeof(kMenu) / sizeof(kMenu[0]));

App::App(Display& d, Input& in, StatusSource& status, CommandSink& cmd,
         GuideController& guide, DeviceInfo& dev)
    : d_(d), in_(in), status_(status), cmd_(cmd), guide_(guide), dev_(dev) {}

constexpr double App::kRenderPeriod;

// =====================================================================
//  UPDATE
// =====================================================================
void App::update(double dt) {
    phase_ += dt;
    renderAccumulator_ += dt;
    if (renderAccumulator_ >= kRenderPeriod) {
        renderAccumulator_ = std::fmod(renderAccumulator_, kRenderPeriod);
        renderDue_ = true;
    }
    if (flash_ > 0) flash_ -= dt;
    if (btnFlash_ > 0) btnFlash_ -= dt;

    in_.update();
    handleButtons();
    handleJoystick();

    status_.update(dt);
    guide_.update(dt);
    dev_.update(dt);

    // leave the GoTo screen automatically once the slew finishes
    if (screen_ == Screen::GoTo && !status_.status().goingTo)
        gotoScreen(Screen::Home);
}

void App::handleButtons() {
    ButtonEvent e;
    while (in_.nextButton(e)) onButton(e);
}

void App::onButton(const ButtonEvent& e) {
    renderDue_ = true;
    lastBtn_ = e.button; btnFlash_ = 0.18;   // brief capsule highlight
    // Select is a GLOBAL emergency stop from every screen.
    if (e.button == Button::Select) { triggerEStop(); return; }

    switch (screen_) {
    case Screen::Home:
        switch (e.button) {
        case Button::A: { int r = status_.status().slewRate - 1; if (r < 1) r = 1; cmd_.setRate(r); } break;
        case Button::B: { int r = status_.status().slewRate + 1; if (r > 9) r = 9; cmd_.setRate(r); } break;
        case Button::X: gotoScreen(Screen::Position); break;
        case Button::Y: cmd_.stopSlew(); activeDir_ = SlewDir::None; break;
        case Button::Start: gotoScreen(Screen::Menu); break;
        default: break;
        }
        break;

    case Screen::Position:
        if (e.button == Button::X || e.button == Button::Start) gotoScreen(Screen::Home);
        break;

    case Screen::Menu:
        if (e.button == Button::A || e.button == Button::Start) {
            const MenuItem& it = kMenu[menuSel_];
            if (it.isClose)           gotoScreen(Screen::Home);
            else if (it.isMode)       mode_ = (mode_ == Mode::Imaging) ? Mode::Standalone : Mode::Imaging;
            else if (it.resumeGuide){ guide_.resume(); gotoScreen(Screen::Home); }
            else                      openConfirm(it.action);
        } else if (e.button == Button::X) {
            gotoScreen(Screen::Home);
        }
        break;

    case Screen::Confirm:
        if (e.button == Button::A)      executeConfirm(e.longPress);
        else if (e.button == Button::B || e.button == Button::X) gotoScreen(prevScreen_);
        break;

    case Screen::GoTo:
        // any of stop/back leaves; Select already e-stopped above
        if (e.button == Button::X || e.button == Button::Y) gotoScreen(Screen::Home);
        break;
    }
}

// Dead-zone + dominant-axis live HERE (shared), so keyboard and seesaw
// behave identically. Joystick only slews from Home/Position.
void App::handleJoystick() {
    float x = in_.joyX();
    float y = in_.joyY();
    float mag = std::sqrt(x * x + y * y);
    const float DEAD = 0.35f;

    if (screen_ == Screen::Menu) {
        // repurpose vertical flicks for list navigation
        if (mag < DEAD) { menuNavLatch_ = false; return; }
        if (!menuNavLatch_) {
            if (y > 0.5f) {
                menuSel_ = (menuSel_ + kMenuCount - 1) % kMenuCount;
                menuNavLatch_ = true;
                renderDue_ = true;
            } else if (y < -0.5f) {
                menuSel_ = (menuSel_ + 1) % kMenuCount;
                menuNavLatch_ = true;
                renderDue_ = true;
            }
        }
        return;
    }

    if (screen_ != Screen::Home && screen_ != Screen::Position) {
        if (activeDir_ != SlewDir::None) { cmd_.stopSlew(); activeDir_ = SlewDir::None; }
        return;
    }

    SlewDir desired = SlewDir::None;
    if (mag >= DEAD) {
        if (std::fabs(x) >= std::fabs(y)) desired = (x > 0) ? SlewDir::East  : SlewDir::West;
        else                              desired = (y > 0) ? SlewDir::North : SlewDir::South;
    }

    if (desired != activeDir_) {
        if (activeDir_ != SlewDir::None) cmd_.stopSlew();
        if (desired != SlewDir::None) {
            // THE HANDOFF: in Imaging mode, pause the guider before a
            // manual slew so PHD2 doesn't fight the move.
            if (mode_ == Mode::Imaging && guide_.online()) {
                GuideState gs = guide_.status().state;
                if (gs == GuideState::Guiding || gs == GuideState::Settling)
                    guide_.pause();
            }
            cmd_.startSlew(desired);
        }
        activeDir_ = desired;
        renderDue_ = true;
    }
}

void App::triggerEStop() {
    cmd_.emergencyStop();
    if (guide_.online()) guide_.pause();  // don't let the guider nudge after stop
    activeDir_ = SlewDir::None;
    flash_ = 0.6;
    renderDue_ = true;
}

void App::openConfirm(ConfirmAction a) {
    pending_ = a;
    prevScreen_ = Screen::Menu;
    screen_ = Screen::Confirm;
}

void App::executeConfirm(bool longPress) {
    // Set Home is "very bad" -> require a long press even on the confirm
    // screen. Everything else confirms on a normal press.
    bool requiresLong = (pending_ == ConfirmAction::SetHome);
    if (requiresLong && !longPress) return;

    switch (pending_) {
        case ConfirmAction::Park:    cmd_.park();    break;
        case ConfirmAction::Unpark:  cmd_.unpark();  break;
        case ConfirmAction::SetHome: cmd_.setHome(); break;
        case ConfirmAction::GoHome:  cmd_.goHome();  break;
        default: break;
    }
    ConfirmAction done = pending_;
    pending_ = ConfirmAction::None;
    if (done == ConfirmAction::GoHome) {
        gotoTotal_ = status_.status().distanceRemainingDeg;
        if (gotoTotal_ < 0.1) gotoTotal_ = 1.0;
        gotoScreen(Screen::GoTo);
    } else {
        gotoScreen(Screen::Home);
    }
}

void App::gotoScreen(Screen s) {
    if ((screen_ == Screen::Home || screen_ == Screen::Position) &&
        s != Screen::Home && s != Screen::Position && activeDir_ != SlewDir::None) {
        cmd_.stopSlew();
        activeDir_ = SlewDir::None;
    }
    prevScreen_ = screen_;
    screen_ = s;
    menuNavLatch_ = false;
    renderDue_ = true;
}

// =====================================================================
//  RENDER — information hierarchy first
//
//  Priority order on the home screen:
//    1. faults / current motion
//    2. guider health and RMS
//    3. coordinates
//    4. quiet mount health indicators
//    5. rate / joystick feedback
//
//  Tracking and slew rate are intentionally shown only once each.
// =====================================================================
static Color battColor(int pct) {
    return pct > 40 ? Theme::GREEN : (pct > 15 ? Theme::YELLOW : Theme::RED);
}

const char* App::stateLabel(const MountStatus& s, Color& c) {
    if (!status_.online())            { c = Theme::RED;    return "OFFLINE"; }
    if (activeDir_ == SlewDir::North) { c = Theme::CYAN;   return "SLEW N"; }
    if (activeDir_ == SlewDir::South) { c = Theme::CYAN;   return "SLEW S"; }
    if (activeDir_ == SlewDir::East)  { c = Theme::CYAN;   return "SLEW E"; }
    if (activeDir_ == SlewDir::West)  { c = Theme::CYAN;   return "SLEW W"; }
    if (s.goingTo)                    { c = Theme::YELLOW; return "GOTO"; }
    if (s.parked)                     { c = Theme::TEXT2;  return "PARKED"; }
    if (s.tracking)                   { c = Theme::GREEN;  return "TRACKING"; }
    c = Theme::TEXT2;
    return "IDLE";
}

void App::render() {
    if (!renderDue_) return;
    renderDue_ = false;

    const MountStatus s = status_.status();
    const GuideStatus g = guide_.status();
    const bool alarm = (g.state == GuideState::StarLost) ||
                       (dev_.wifiBars() <= 0) || !status_.online();

    d_.clear(Theme::BG);
    drawTopBar(s, g, alarm);
    switch (screen_) {
        case Screen::Home:     drawHome(s, g);  break;
        case Screen::Position: drawPosition(s); break;
        case Screen::Menu:     drawMenu();      break;
        case Screen::Confirm:  drawConfirm();   break;
        case Screen::GoTo:     drawGoto(s, g);  break;
    }
    if (flash_ > 0) drawStopOverlay();
    d_.present();
}

void App::drawTopBar(const MountStatus& s, const GuideStatus& g, bool alarm) {
    (void)g;
    const int W = d_.width();
    const Color bg = alarm ? Theme::REDDARK : Theme::PANEL2;
    d_.fillRect(0, 0, W, 20, bg);
    d_.drawHLine(0, 19, W, alarm ? Theme::RED : Theme::HAIRLINE);

    // Left: local link plus a compact product mark.
    if (dev_.wifiBars() <= 0) {
        d_.drawText(6, 4, "NO LINK", Fonts::Small, Theme::RED, bg);
    } else {
        ui::wifiBars(d_, 6, 3, dev_.wifiBars(), Theme::GREEN, Theme::OFF);
        d_.drawText(26, 2, "ONSTEP", Fonts::Body, Theme::TEXT, bg);
    }

    // Center: mode and target. This uses the header space without turning it
    // into a decorative title banner.
    const char* modeText = mode_ == Mode::Imaging ? "IMG" : "SETUP";
    const Color modeColor = mode_ == Mode::Imaging ? Theme::BLUE : Theme::YELLOW;
    const int modeW = d_.textWidth(modeText, Fonts::Small) + 10;
    const int modeX = 104;
    const Color modeBg = ui::dim(modeColor, 0.34f);
    d_.fillRoundRect(modeX, 3, modeW, 14, 4, modeBg);
    d_.drawTextCentered(modeX + modeW / 2, 3, modeText,
                        Fonts::Small, modeColor, modeBg);

    std::string centerText;
    if (!s.targetName.empty()) centerText = "TARGET " + s.targetName;
    else centerText = status_.online() ? "MOUNT READY" : "MOUNT OFFLINE";
    const Color centerColor = status_.online() ? Theme::TEXT2 : Theme::RED;
    d_.drawText(modeX + modeW + 8, 4, centerText,
                Fonts::Small, centerColor, bg);

    // Right: battery, compact and quiet unless it becomes a warning.
    const int pct = dev_.batteryPercent();
    const Color bc = battColor(pct);
    char percent[10];
    std::snprintf(percent, sizeof percent, "%d%%", pct);
    const int batteryX = W - 28;
    ui::battery(d_, batteryX, 4, pct, bc);
    d_.drawTextRight(batteryX - 5, 4, percent, Fonts::Small, bc, bg);
}

void App::drawHome(const MountStatus& s, const GuideStatus& g) {
    const float slowPulse = ui::pulse(phase_, 3.0);
    const float fastPulse = ui::pulse(phase_, 8.0);

    // Geometry: a narrow health rail, a dominant information card, and a
    // compact physical-control card. The dashboard stays wide rather than
    // becoming a vertical list of equally loud rows.
    const int y = 24;
    const int h = 109;
    const int healthX = 5,   healthW = 68;
    const int heroX   = 77,  heroW   = 160;
    const int ctrlX   = 241, ctrlW   = 74;

    ui::panel(d_, healthX, y, healthW, h, Theme::PANEL, Theme::BORDER);
    ui::panel(d_, heroX,   y, heroW,   h, Theme::PANEL, Theme::BORDER);
    ui::panel(d_, ctrlX,   y, ctrlW,   h, Theme::PANEL, Theme::BORDER);

    // -----------------------------------------------------------------
    // QUIET HEALTH RAIL — baseline facts, never the visual headline.
    // Guiding is deliberately absent here because it owns the hero area.
    // -----------------------------------------------------------------
    d_.drawText(healthX + 9, y + 5, "SYSTEM",
                Fonts::Small, Theme::DIM, Theme::PANEL);
    d_.drawHLine(healthX + 8, y + 20, healthW - 16, Theme::HAIRLINE);

    auto railRow = [&](int rowY, const char* label, Color color,
                       bool active, bool breathe) {
        Color shown = active ? color : Theme::OFF;
        if (active && breathe) {
            shown = ui::lerp(ui::dim(color, 0.55f), color,
                             0.52f + 0.48f * slowPulse);
        }
        ui::statusDot(d_, healthX + 12, rowY + 5, 3, shown, !active);
        d_.drawText(healthX + 22, rowY, label, Fonts::Small,
                    active ? Theme::TEXT2 : Theme::DIM, Theme::PANEL);
    };

    railRow(y + 28, "TRACK", Theme::GREEN, s.tracking, s.tracking);
    railRow(y + 48, "HOME",  Theme::GREEN, s.homeSet, false);
    railRow(y + 68, "PARK",  Theme::YELLOW, s.parked, false);
    railRow(y + 88, "MOUNT", Theme::GREEN, status_.online(), false);

    // -----------------------------------------------------------------
    // HERO — whichever fact needs attention now.
    // Guiding wins during normal imaging. Motion and faults override it.
    // -----------------------------------------------------------------
    const bool linkLost = dev_.wifiBars() <= 0 || !status_.online();
    const bool starLost = g.state == GuideState::StarLost;

    const char* heroLabel = "IDLE";
    const char* heroSub = "READY";
    Color heroColor = Theme::TEXT2;
    char heroMetric[16] = "";

    if (linkLost) {
        heroLabel = "LINK LOST";
        heroSub = status_.online() ? "HANDHELD WIFI" : "MOUNT OFFLINE";
        heroColor = Theme::RED;
    } else if (starLost) {
        heroLabel = "STAR LOST";
        heroSub = "CHECK GUIDER";
        heroColor = Theme::RED;
    } else if (activeDir_ != SlewDir::None) {
        heroLabel = activeDir_ == SlewDir::North ? "SLEW N" :
                    activeDir_ == SlewDir::South ? "SLEW S" :
                    activeDir_ == SlewDir::East  ? "SLEW E" : "SLEW W";
        heroSub = (mode_ == Mode::Imaging && guide_.online())
                    ? "GUIDER PAUSED" : "MANUAL MOTION";
        heroColor = Theme::CYAN;
    } else if (s.goingTo) {
        heroLabel = "GOTO";
        heroSub = s.targetName.empty() ? "MOUNT SLEW" : s.targetName.c_str();
        heroColor = Theme::YELLOW;
    } else if (s.parked) {
        heroLabel = "PARKED";
        heroSub = "MOUNT SAFE";
        heroColor = Theme::TEXT2;
    } else {
        switch (g.state) {
            case GuideState::Guiding:
                heroLabel = "GUIDING";
                heroSub = "RMS";
                std::snprintf(heroMetric, sizeof heroMetric, "%.2f\"", g.rmsArcsec);
                heroColor = Theme::GREEN;
                break;
            case GuideState::Settling:
                heroLabel = "SETTLING";
                heroSub = "GUIDER WAIT";
                heroColor = Theme::YELLOW;
                break;
            case GuideState::Paused:
                heroLabel = "GUIDE PAUSED";
                heroSub = "READY TO RESUME";
                heroColor = Theme::YELLOW;
                break;
            case GuideState::Calibrating:
                heroLabel = "CALIBRATING";
                heroSub = "PHD2";
                heroColor = Theme::YELLOW;
                break;
            case GuideState::Idle:
            default:
                heroLabel = s.tracking ? "TRACKING" : "IDLE";
                heroSub = guide_.online() ? "GUIDER IDLE" : "PHD2 OFFLINE";
                heroColor = s.tracking ? Theme::GREEN : Theme::TEXT2;
                break;
        }
    }

    Color heroBg = Theme::PANEL;
    if (linkLost || starLost) {
        heroBg = Theme::REDDARK;
        d_.fillRoundRect(heroX + 1, y + 1, heroW - 2, 35, 3, heroBg);
    } else {
        d_.fillRoundRect(heroX + 1, y + 1, heroW - 2, 35, 3, Theme::PANEL3);
        heroBg = Theme::PANEL3;
    }

    Color shownHero = heroColor;
    if (linkLost || starLost || activeDir_ != SlewDir::None) {
        shownHero = ui::lerp(ui::dim(heroColor, 0.60f), heroColor,
                             0.58f + 0.42f * fastPulse);
    }

    d_.drawText(heroX + 10, y + 5, heroLabel,
                Fonts::State, shownHero, heroBg);

    if (heroMetric[0]) {
        d_.drawTextRight(heroX + heroW - 9, y + 5, heroMetric,
                         Fonts::Value, Theme::TEXT, heroBg);
        d_.drawTextRight(heroX + heroW - 9, y + 23, heroSub,
                         Fonts::Small, Theme::DIM, heroBg);
    } else {
        d_.drawTextRight(heroX + heroW - 9, y + 23, heroSub,
                         Fonts::Small,
                         (linkLost || starLost) ? Theme::RED : Theme::DIM,
                         heroBg);
    }

    // Coordinates are prominent but secondary to the current operational
    // state. Tabular digits prevent horizontal jitter as values change.
    d_.drawText(heroX + 9, y + 43, "RA",
                Fonts::Small, Theme::DIM, Theme::PANEL);
    d_.drawText(heroX + 31, y + 40, raToStr(s.raHours),
                Fonts::Value, Theme::BLUE, Theme::PANEL);
    d_.drawText(heroX + 9, y + 63, "DEC",
                Fonts::Small, Theme::DIM, Theme::PANEL);
    d_.drawText(heroX + 31, y + 60, decToStr(s.decDeg),
                Fonts::Value, Theme::BLUE, Theme::PANEL);

    d_.drawHLine(heroX + 8, y + 80, heroW - 16, Theme::HAIRLINE);

    char alt[14], az[14];
    std::snprintf(alt, sizeof alt, "ALT %.0f°", s.altDeg);
    std::snprintf(az, sizeof az, "AZ %.0f°", s.azDeg);
    const char* pier = s.pierSide == 1 ? "PIER E" :
                       s.pierSide == 2 ? "PIER W" : "PIER --";
    d_.drawText(heroX + 9, y + 87, alt,
                Fonts::Small, Theme::TEXT2, Theme::PANEL);
    d_.drawTextCentered(heroX + heroW / 2, y + 87, az,
                        Fonts::Small, Theme::TEXT2, Theme::PANEL);
    d_.drawTextRight(heroX + heroW - 8, y + 87, pier,
                     Fonts::Small, Theme::TEXT2, Theme::PANEL);

    // -----------------------------------------------------------------
    // CONTROL CARD — rate appears once, small and near the control it
    // affects. Direction feedback gets the physical space instead.
    // -----------------------------------------------------------------
    char rateText[12];
    std::snprintf(rateText, sizeof rateText, "RATE %d", s.slewRate);
    const int rateW = d_.textWidth(rateText, Fonts::Small) + 12;
    const int rateX = ctrlX + (ctrlW - rateW) / 2;
    d_.fillRoundRect(rateX, y + 6, rateW, 15, 4, Theme::PANEL3);
    d_.drawTextCentered(ctrlX + ctrlW / 2, y + 6, rateText,
                        Fonts::Small, Theme::BLUE, Theme::PANEL3);

    const int dpadX = ctrlX + ctrlW / 2;
    const int dpadY = y + 62;
    ui::dpad(d_, dpadX, dpadY, activeDir_, fastPulse);

    auto dirColor = [&](SlewDir dir) {
        return activeDir_ == dir ? Theme::CYAN : Theme::TEXT2;
    };
    d_.drawTextCentered(dpadX, y + 27, "N", Fonts::Small,
                        dirColor(SlewDir::North), Theme::PANEL);
    d_.drawTextCentered(dpadX, y + 84, "S", Fonts::Small,
                        dirColor(SlewDir::South), Theme::PANEL);
    d_.drawText(ctrlX + 7, y + 58, "W", Fonts::Small,
                dirColor(SlewDir::West), Theme::PANEL);
    d_.drawTextRight(ctrlX + ctrlW - 7, y + 58, "E", Fonts::Small,
                     dirColor(SlewDir::East), Theme::PANEL);

    const char* joyText = activeDir_ == SlewDir::None ? "JOY READY" : "RELEASE = STOP";
    d_.drawTextCentered(ctrlX + ctrlW / 2, y + 96, joyText,
                        Fonts::Small,
                        activeDir_ == SlewDir::None ? Theme::DIM : Theme::YELLOW,
                        Theme::PANEL);

    drawBottom("home");
}

void App::drawPosition(const MountStatus& s) {
    ui::panel(d_, 5, 24, 310, 109, Theme::PANEL, Theme::BORDER);
    d_.drawText(14, 28, "POSITION", Fonts::Body, Theme::TEXT, Theme::PANEL);
    d_.drawHLine(14, 46, 292, Theme::HAIRLINE);

    auto numericRow = [&](int x, int rowY, const char* key,
                          const std::string& value, Color color) {
        d_.drawText(x, rowY + 3, key, Fonts::Small, Theme::DIM, Theme::PANEL);
        d_.drawText(x + 39, rowY, value, Fonts::Value, color, Theme::PANEL);
    };
    auto textRow = [&](int x, int rowY, const char* key,
                       const std::string& value, Color color) {
        d_.drawText(x, rowY + 1, key, Fonts::Small, Theme::DIM, Theme::PANEL);
        d_.drawText(x + 45, rowY, value, Fonts::Body, color, Theme::PANEL);
    };

    numericRow(15, 51, "RA", raToStr(s.raHours), Theme::BLUE);
    numericRow(15, 74, "DEC", decToStr(s.decDeg), Theme::BLUE);

    char alt[18], az[18];
    std::snprintf(alt, sizeof alt, "%.1f°", s.altDeg);
    std::snprintf(az, sizeof az, "%.1f°", s.azDeg);
    numericRow(15, 98, "ALT", alt, Theme::TEXT);
    numericRow(164, 98, "AZ", az, Theme::TEXT);

    const char* pier = s.pierSide == 1 ? "EAST" :
                       s.pierSide == 2 ? "WEST" : "UNKNOWN";
    textRow(164, 52, "PIER", pier, Theme::TEXT);
    textRow(164, 75, "TRACK", s.tracking ? "SIDEREAL" : "OFF",
            s.tracking ? Theme::GREEN : Theme::DIM);

    drawBottom("pos");
}

void App::drawMenu() {
    ui::panel(d_, 5, 24, 310, 109, Theme::PANEL, Theme::BORDER);
    int y = 28;
    for (int i = 0; i < kMenuCount; ++i) {
        const bool selected = i == menuSel_;
        const Color rowBg = selected ? Theme::PANEL3 : Theme::PANEL;
        if (selected) {
            d_.fillRoundRect(9, y - 1, 302, 13, 2, rowBg);
            d_.fillRoundRect(9, y - 1, 3, 13, 1, Theme::BLUE);
        }
        std::string label = kMenu[i].label;
        if (kMenu[i].isMode)
            label += mode_ == Mode::Imaging ? "   IMG" : "   SETUP";
        d_.drawText(17, y, label, Fonts::Small,
                    selected ? Theme::TEXT : Theme::TEXT2, rowBg);
        if (selected)
            d_.drawTextRight(303, y, ">", Fonts::Small, Theme::BLUE, rowBg);
        y += 14;
    }
    drawBottom("menu");
}

void App::drawConfirm() {
    const char* verb = "?";
    const char* sub = "";
    bool danger = false;
    switch (pending_) {
        case ConfirmAction::Park:
            verb = "PARK MOUNT?";
            sub = "TRACKING WILL STOP";
            danger = true;
            break;
        case ConfirmAction::Unpark:
            verb = "UNPARK?";
            sub = "MOUNT MAY RESUME TRACKING";
            break;
        case ConfirmAction::SetHome:
            verb = "SET HOME?";
            sub = "CHANGES THE MOUNT REFERENCE";
            danger = true;
            break;
        case ConfirmAction::GoHome:
            verb = "GO HOME?";
            sub = "MOUNT WILL SLEW";
            danger = true;
            break;
        default:
            break;
    }

    const Color edge = danger ? Theme::RED : Theme::YELLOW;
    const Color panelBg = danger ? Theme::REDDARK : Theme::PANEL;
    ui::panel(d_, 18, 29, 284, 96, panelBg, edge);
    d_.drawText(29, 33, "CONFIRM ACTION", Fonts::Body, edge, panelBg);
    d_.drawHLine(29, 50, 262, ui::dim(edge, 0.42f));
    d_.drawTextCentered(d_.width() / 2, 56, verb,
                        Fonts::State, Theme::TEXT, panelBg);
    d_.drawTextCentered(d_.width() / 2, 86, sub,
                        Fonts::Body, Theme::TEXT2, panelBg);
    if (pending_ == ConfirmAction::SetHome)
        d_.drawTextCentered(d_.width() / 2, 105, "LONG PRESS REQUIRED",
                            Fonts::Small, Theme::YELLOW, panelBg);

    drawBottom("confirm");
}

void App::drawGoto(const MountStatus& s, const GuideStatus& g) {
    ui::panel(d_, 5, 24, 310, 109, Theme::PANEL, Theme::BORDER);
    std::string title = "GOTO";
    if (!s.targetName.empty()) title += "  " + s.targetName;
    d_.drawText(15, 28, title, Fonts::Body, Theme::YELLOW, Theme::PANEL);

    const double total = gotoTotal_ > 0.1 ? gotoTotal_ : 1.0;
    const float fraction = 1.0f - static_cast<float>(s.distanceRemainingDeg / total);
    ui::progressBar(d_, 15, 52, 290, 15, fraction,
                    Theme::BLUE, Theme::PANEL2, Theme::BORDER,
                    ui::pulse(phase_, 8.0));
    char percent[8];
    std::snprintf(percent, sizeof percent, "%d%%",
                  static_cast<int>(fraction * 100 + 0.5f));
    d_.drawTextRight(303, 69, percent, Fonts::Small, Theme::TEXT, Theme::PANEL);

    int seconds = s.etaSeconds;
    if (seconds < 0) seconds = 0;
    char eta[18], distance[24];
    std::snprintf(eta, sizeof eta, "ETA %02d:%02d", seconds / 60, seconds % 60);
    std::snprintf(distance, sizeof distance, "DIST %.1f°", s.distanceRemainingDeg);
    d_.drawText(15, 86, eta, Fonts::Body, Theme::TEXT2, Theme::PANEL);
    d_.drawTextRight(303, 86, distance, Fonts::Body, Theme::TEXT2, Theme::PANEL);

    Color stateColor;
    const char* state = stateLabel(s, stateColor);
    d_.drawText(15, 108, state, Fonts::Small, stateColor, Theme::PANEL);
    if (g.state == GuideState::Paused)
        d_.drawTextRight(303, 108, "GUIDE PAUSED",
                         Fonts::Small, Theme::YELLOW, Theme::PANEL);

    drawBottom("goto");
}

void App::drawBottom(const char* screenName) {
    const int W = d_.width();
    const std::string screen = screenName;
    d_.fillRect(0, 137, W, 33, Theme::PANEL2);
    d_.drawHLine(0, 137, W, Theme::HAIRLINE);

    auto pressed = [&](Button button) {
        return btnFlash_ > 0 && lastBtn_ == button;
    };

    if (screen == "home") {
        const char* context = activeDir_ == SlewDir::None
            ? "JOYSTICK READY" : "RELEASE JOYSTICK TO STOP";
        d_.drawText(6, 139, context, Fonts::Small,
                    activeDir_ == SlewDir::None ? Theme::DIM : Theme::YELLOW,
                    Theme::PANEL2);
        if (mode_ == Mode::Imaging && activeDir_ != SlewDir::None)
            d_.drawTextRight(W - 6, 139, "GUIDER PAUSED",
                             Fonts::Small, Theme::YELLOW, Theme::PANEL2);

        int x = 2;
        ui::actionCell(d_, x, 153, 32, "A", "-", pressed(Button::A)); x += 35;
        ui::actionCell(d_, x, 153, 32, "B", "+", pressed(Button::B)); x += 35;
        ui::actionCell(d_, x, 153, 45, "X", "POS", pressed(Button::X)); x += 48;
        ui::actionCell(d_, x, 153, 50, "Y", "STOP", pressed(Button::Y)); x += 53;
        ui::actionCell(d_, x, 153, 72, "START", "MENU", pressed(Button::Start)); x += 75;
        ui::actionCell(d_, x, 153, 72, "SEL", "E-STOP",
                       pressed(Button::Select), true);
    } else if (screen == "pos") {
        d_.drawText(6, 139, "LIVE MOUNT COORDINATES",
                    Fonts::Small, Theme::DIM, Theme::PANEL2);
        ui::actionCell(d_, 5, 153, 90, "X", "BACK", pressed(Button::X));
        ui::actionCell(d_, 99, 153, 100, "START", "BACK", pressed(Button::Start));
        ui::actionCell(d_, 203, 153, 112, "SEL", "E-STOP",
                       pressed(Button::Select), true);
    } else if (screen == "menu") {
        d_.drawText(6, 139, "JOYSTICK NAVIGATION",
                    Fonts::Small, Theme::DIM, Theme::PANEL2);
        ui::actionCell(d_, 5, 153, 92, "JOY", "MOVE", false);
        ui::actionCell(d_, 101, 153, 72, "A", "OPEN", pressed(Button::A));
        ui::actionCell(d_, 177, 153, 72, "X", "BACK", pressed(Button::X));
        ui::actionCell(d_, 253, 153, 62, "SEL", "STOP",
                       pressed(Button::Select), true);
    } else if (screen == "confirm") {
        d_.drawText(6, 139, "VERIFY BEFORE COMMAND",
                    Fonts::Small, Theme::DIM, Theme::PANEL2);
        const char* accept = pending_ == ConfirmAction::SetHome ? "HOLD A" : "A";
        ui::actionCell(d_, 5, 153, 126, accept, "CONFIRM", pressed(Button::A));
        ui::actionCell(d_, 135, 153, 86, "B", "CANCEL", pressed(Button::B));
        ui::actionCell(d_, 225, 153, 90, "SEL", "E-STOP",
                       pressed(Button::Select), true);
    } else if (screen == "goto") {
        d_.drawText(6, 139, "GOTO IN PROGRESS",
                    Fonts::Small, Theme::YELLOW, Theme::PANEL2);
        ui::actionCell(d_, 5, 153, 142, "Y", "STOP GOTO",
                       pressed(Button::Y), true);
        ui::actionCell(d_, 151, 153, 164, "SEL", "EMERGENCY STOP",
                       pressed(Button::Select), true);
    }
}

void App::drawStopOverlay() {
    const float p = ui::pulse(phase_, 12.0);
    const Color bg = ui::lerp(Theme::REDDARK, Color{72, 8, 12},
                              0.15f + 0.18f * p);
    d_.fillRect(0, 0, d_.width(), d_.height(), bg);
    ui::hazardFrame(d_, 7, 7, d_.width() - 14, d_.height() - 14,
                    ui::lerp(ui::dim(Theme::RED, 0.72f), Theme::RED, p));
    ui::stopOctagon(d_, 58, 85, 23, Theme::RED, Theme::TEXT, bg);
    d_.drawText(99, 47, "EMERGENCY STOP",
                Fonts::State, Theme::RED, bg);
    d_.drawText(100, 82, "MOUNT HALTED",
                Fonts::Body, Theme::TEXT, bg);
    d_.drawText(100, 101, "STOP COMMAND SENT",
                Fonts::Small, Theme::TEXT2, bg);
}
