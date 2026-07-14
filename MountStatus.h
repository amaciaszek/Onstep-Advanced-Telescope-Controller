#pragma once
#include <string>

// ---- Domain types shared by simulator and (later) ESP32 firmware ----

enum class SlewDir { None, North, South, East, West };

enum class LinkState { Disconnected, Connecting, Connected };

enum class GuideState {
    Idle,        // guider not guiding
    Settling,
    Guiding,     // locked on star, correcting
    StarLost,
    Paused,
    Calibrating
};

// Mount state as reported BY THE LAPTOP (NINA/ASCOM) in the real system.
// The handheld never queries the mount directly for this.
struct MountStatus {
    double siteLatDeg = 91.0;   // >90 = unknown (not yet fetched)
    LinkState link   = LinkState::Disconnected;
    bool tracking    = false;
    bool parked      = false;
    bool homeSet     = false;
    bool slewing     = false;
    SlewDir slewDir  = SlewDir::None;
    int  slewRate    = 5;        // 1..9  (maps to OnStep :R1#..:R9#)

    double raHours   = 0.0;      // 0..24
    double decDeg    = 0.0;      // -90..90
    double altDeg    = 0.0;
    double azDeg     = 0.0;
    int    pierSide  = 0;        // 0 unknown, 1 east, 2 west
    bool   guidePulseActive = false; // OnStep is presently applying a guide pulse
    double localSiderealHours = -1.0; // :GS# when supported, otherwise unknown

    bool        goingTo = false; // a GoTo is in progress
    std::string targetName;
    double      distanceRemainingDeg = 0.0;
    int         etaSeconds = 0;
};

struct GuideStatus {
    GuideState state = GuideState::Idle;
    double rmsArcsec = 0.0;
};

inline const char* slewDirName(SlewDir d) {
    switch (d) {
        case SlewDir::North: return "N";
        case SlewDir::South: return "S";
        case SlewDir::East:  return "E";
        case SlewDir::West:  return "W";
        default:             return "-";
    }
}
