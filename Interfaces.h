#pragma once
#include "MountStatus.h"

// =====================================================================
//  The architecture we converged on: status and commands are SEPARATE
//  planes with opposite risk profiles.
//
//   StatusSource   = involuntary, frequent. In the real system this is
//                    the LAPTOP (NINA Advanced API / PHD2), never a poll
//                    to the mount. Read-only.
//   CommandSink    = voluntary, rare. Direct to OnStep, only on a button
//                    press. Allowed to be disruptive.
//   GuideController = pause/resume PHD2 so a manual slew is a clean
//                    handoff, not a fight with the guider.
//
//  In the simulator all three are backed by one SimWorld. On hardware
//  they become WiFi/HTTP clients. The shared App never knows which.
// =====================================================================

// ---- Status plane: read mount state from the laptop -----------------
class StatusSource {
public:
    virtual ~StatusSource() = default;
    virtual void        update(double dt) = 0;   // impl controls cadence
    virtual MountStatus status() const = 0;
    virtual bool        online() const = 0;      // is the source reachable
};

// ---- Command plane: write to the mount, on user action --------------
class CommandSink {
public:
    virtual ~CommandSink() = default;
    virtual void startSlew(SlewDir d) = 0;
    virtual void stopSlew() = 0;
    virtual void emergencyStop() = 0;            // :Q# fastest path
    virtual void setRate(int r) = 0;             // 1..9
    virtual void park() = 0;
    virtual void unpark() = 0;
    virtual void setHome() = 0;
    virtual void goHome() = 0;
};

// ---- Guider plane: cooperate with PHD2 ------------------------------
class GuideController {
public:
    virtual ~GuideController() = default;
    virtual void        update(double dt) = 0;
    virtual GuideStatus status() const = 0;
    virtual bool        online() const = 0;
    virtual void        pause() = 0;
    virtual void        resume() = 0;
};

// ---- Local device info: handheld battery + link to the mount AP -----
// This is the HANDHELD's own hardware, distinct from mount status.
class DeviceInfo {
public:
    virtual ~DeviceInfo() = default;
    virtual void update(double dt) = 0;
    virtual int  batteryPercent() const = 0;   // 0..100
    virtual int  wifiBars() const = 0;         // 0..4 (link to mount)
};
