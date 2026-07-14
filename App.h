#pragma once
#include "Display.h"
#include "Input.h"
#include "Interfaces.h"

// Imaging  : status comes from the laptop, disruptive commands first
//            pause the guider (the handoff). Default at the telescope.
// Standalone: no laptop (visual / polar align / setup). Commands go
//            straight to the mount; no guider to coordinate with.
enum class Mode { Imaging, Standalone };

enum class Screen { Home, Position, Menu, Confirm, GoTo };

enum class ConfirmAction { None, Park, Unpark, SetHome, GoHome };

// The whole device behaviour lives here and is platform-independent.
class App {
public:
    App(Display& d, Input& in,
        StatusSource& status, CommandSink& cmd,
        GuideController& guide, DeviceInfo& dev);

    void update(double dt);
    void render();

    void setMode(Mode m) { mode_ = m; renderDue_ = true; }
    Mode mode() const    { return mode_; }

    // Exposed for headless tests:
    Screen  screen() const  { return screen_; }
    SlewDir slewDir() const { return activeDir_; }
    bool    stopFlashing() const { return flash_ > 0.0; }

private:
    // input handling
    void handleButtons();
    void handleJoystick();
    void onButton(const ButtonEvent& e);
    void triggerEStop();
    void openConfirm(ConfirmAction a);
    void executeConfirm(bool longPress);
    void gotoScreen(Screen s);

    // rendering
    void drawTopBar(const MountStatus& s, const GuideStatus& g, bool alarm);
    void drawHome(const MountStatus& s, const GuideStatus& g);
    void drawPosition(const MountStatus& s);
    void drawMenu();
    void drawConfirm();
    void drawGoto(const MountStatus& s, const GuideStatus& g);
    void drawBottom(const char* hintsScreen);
    void drawStopOverlay();
    const char* stateLabel(const MountStatus& s, Color& outColor);

    Display&         d_;
    Input&           in_;
    StatusSource&    status_;
    CommandSink&     cmd_;
    GuideController& guide_;
    DeviceInfo&      dev_;

    Mode    mode_     = Mode::Imaging;
    Screen  screen_   = Screen::Home;
    Screen  prevScreen_ = Screen::Home;

    SlewDir activeDir_ = SlewDir::None;   // what we've told the mount to do
    int     menuSel_   = 0;
    ConfirmAction pending_ = ConfirmAction::None;

    double  phase_ = 0.0;   // animation clock
    double  flash_ = 0.0;   // e-stop red-flash timer (seconds remaining)
    bool    menuNavLatch_ = false; // debounce joystick menu nav

    Button  lastBtn_  = Button::A; // for bottom-strip press highlight
    double  btnFlash_ = 0.0;
    double  gotoTotal_ = 1.0;      // captured distance at GoTo start

    // UI rendering is intentionally decoupled from the fast input/control
    // loop. Twenty frames per second is more than enough for readable mount
    // coordinates and subtle equipment-style animation.
    double  renderAccumulator_ = 0.0;
    bool    renderDue_ = true;
    static constexpr double kRenderPeriod = 1.0 / 20.0;
};
