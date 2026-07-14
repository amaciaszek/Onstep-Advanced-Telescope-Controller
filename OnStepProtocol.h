#pragma once
#include <string>
#include "MountStatus.h"

// Platform-independent OnStep/OnStepX protocol layer. NO Arduino dependency
// so it compiles and is unit-tested on the desktop. The ESP32 status source
// fetches the raw strings over WiFi and hands them here; all the fragile
// decode rules (observed on real hardware) live in one tested place.
namespace onstep {

// Hash-stripped replies gathered from the mount.
struct Raw {
    std::string gtLat; // :Gt# site latitude, fetched once
    std::string gr;   // :GR#  current RA    "HH:MM:SS"
    std::string gd;   // :GD#  current Dec   "+DD*MM:SS"
    std::string ga;   // :GA#  altitude      "+DD*MM:SS"
    std::string gz;   // :GZ#  azimuth       "DDD*MM:SS"
    std::string gt;   // :GT#  tracking rate (Hz)
    std::string gw;   // :GW#  basic status  e.g. "GT0"
    std::string gu;   // :GU#  detailed      e.g. "NpeET160"
    std::string d;    // :D#   movement indicator (0x7F while slewing, else empty)
    std::string gs;   // :GS#  local sidereal time "HH:MM:SS"
};

enum class Park { Unknown, Unparked, Parking, Parked, Failed };

// A single dominant activity. Precedence matters: manual jog and parking must
// win over the raw :D# slew flag, and :D# alone must NOT be treated as motion.
enum class Move { Idle, Manual, PulseGuide, Parking, Homing, GotoSlew };

struct State {
    double siteLatDeg = 91.0;
    bool   tracking   = false;
    Park   park       = Park::Unknown;
    bool   atHome     = false;
    bool   gotoActive = false;   // GU lacks 'N'
    Move   move       = Move::Idle;
    int    pierSide   = 0;        // 0 none, 1 east, 2 west
    int    errorCode  = 0;
    char   guideRateSel = 0;
    double raHours = 0, decDeg = 0, altDeg = 0, azDeg = 0;
    double localSiderealHours = -1.0;

    bool moving()  const { return move != Move::Idle; }
};

// Core decode + projection into the UI's MountStatus.
State       decode(const Raw& r);
MountStatus toMountStatus(const State& s, bool linked);

// Parsers (tolerant of ':' and '*' separators, optional sign).
double parseSexagesimal(const std::string& s);   // -> value in its own unit
double parseRA (const std::string& hms);          // "HH:MM:SS" -> hours
double parseDec(const std::string& dms);          // "+DD*MM:SS" -> degrees
bool   dMoving(const std::string& d);             // true if starts with 0x7F

// LX200 command builders.
const char* slewCmd(SlewDir d);     // :Mn# :Ms# :Me# :Mw#
const char* stopCmd(SlewDir d);     // :Qn# :Qs# :Qe# :Qw#
const char* stopAllCmd();           // :Q#   (also aborts goto/park)
std::string rateCmd(int rate1to9);  // :R1# .. :R9#
const char* parkCmd();              // :hP#
const char* unparkCmd();            // :hR#
const char* goHomeCmd();            // :hC#   move to home
// NOTE: "set home / set park" codes are destructive and vary; verify against
// OnStepX 10.25i before wiring. Intentionally not provided as a bare builder.

} // namespace onstep
