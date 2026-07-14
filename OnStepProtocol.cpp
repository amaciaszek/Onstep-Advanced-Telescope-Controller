#include "OnStepProtocol.h"
#include <cctype>
#include <cstdint>
#include <cstdlib>

namespace onstep {

static bool has(const std::string& s, char c) { return s.find(c) != std::string::npos; }

double parseSexagesimal(const std::string& s) {
    // Extract sign, then up to three integer groups separated by any
    // non-digit (':' '*' ' ' etc). Fractional seconds tolerated.
    int sign = 1;
    double parts[3] = {0, 0, 0};
    int np = 0;
    size_t i = 0;
    while (i < s.size() && (s[i] == ' ')) ++i;
    if (i < s.size() && (s[i] == '+' || s[i] == '-')) { if (s[i] == '-') sign = -1; ++i; }
    std::string num;
    auto flush = [&]() {
        if (!num.empty() && np < 3) { parts[np++] = std::atof(num.c_str()); num.clear(); }
        else num.clear();
    };
    for (; i < s.size(); ++i) {
        char c = s[i];
        if (std::isdigit((unsigned char)c) || c == '.') num += c;
        else flush();
    }
    flush();
    double v = parts[0] + parts[1] / 60.0 + parts[2] / 3600.0;
    return sign * v;
}

double parseRA (const std::string& hms) { return parseSexagesimal(hms); }
double parseDec(const std::string& dms) { return parseSexagesimal(dms); }

bool dMoving(const std::string& d) {
    return !d.empty() && (uint8_t)d[0] == 0x7F;
}

State decode(const Raw& r) {
    State st;
    const std::string& gu = r.gu;

    // Empty :GU# means the mount never answered. Leave the safe defaults
    // (not tracking, park unknown, idle) instead of decoding absence-of-flags
    // as "tracking + goto active".
    if (gu.empty()) {
        if(!r.gtLat.empty()) st.siteLatDeg = parseDec(r.gtLat);
    st.raHours = parseRA(r.gr);
        st.decDeg  = parseDec(r.gd);
        st.altDeg  = parseDec(r.ga);
        st.azDeg   = parseDec(r.gz);
        if (!r.gs.empty()) st.localSiderealHours = parseRA(r.gs);
        return st;
    }

    // --- tracking: lowercase 'n' means NOT tracking ---
    st.tracking = !has(gu, 'n');

    // --- park state (GU is authoritative; order matters) ---
    if      (has(gu, 'F')) st.park = Park::Failed;
    else if (has(gu, 'I')) st.park = Park::Parking;
    else if (has(gu, 'P')) st.park = Park::Parked;
    else if (has(gu, 'p')) st.park = Park::Unparked;
    else                   st.park = Park::Unknown;

    st.atHome     = has(gu, 'H');
    st.gotoActive = !has(gu, 'N');   // 'N' == no goto active

    // --- pier side (GU: T=east, W=west, o=none) ---
    if      (has(gu, 'W')) st.pierSide = 2;
    else if (has(gu, 'T')) st.pierSide = 1;
    else                   st.pierSide = 0;

    // --- dominant activity, in strict precedence order ---
    // Manual jog and parking/homing must win over the raw :D# slew flag, and
    // :D# alone is NEVER treated as motion (it stays idle during manual jog).
    if      (has(gu, 'g'))                    st.move = Move::Manual;     // manual directional
    else if (has(gu, 'I'))                    st.move = Move::Parking;
    else if (has(gu, 'h'))                    st.move = Move::Homing;
    else if (dMoving(r.d) && st.gotoActive)   st.move = Move::GotoSlew;   // real goto
    else if (has(gu, 'G'))                    st.move = Move::PulseGuide;
    else                                      st.move = Move::Idle;

    // --- trailing chars: pulse-rate, guide-rate selection, error code ---
    if (gu.size() >= 1) {
        char last = gu[gu.size() - 1];
        if (last >= '0' && last <= '9') st.errorCode = last - '0';
    }
    if (gu.size() >= 2) {
        char sel = gu[gu.size() - 2];
        if (sel >= '0' && sel <= '9') st.guideRateSel = sel;
    }

    if(!r.gtLat.empty()) st.siteLatDeg = parseDec(r.gtLat);
    st.raHours = parseRA(r.gr);
    st.decDeg  = parseDec(r.gd);
    st.altDeg  = parseDec(r.ga);
    st.azDeg   = parseDec(r.gz);
    if (!r.gs.empty()) st.localSiderealHours = parseRA(r.gs);
    return st;
}

MountStatus toMountStatus(const State& s, bool linked) {
    MountStatus m;
    m.link     = linked ? LinkState::Connected : LinkState::Disconnected;
    m.tracking = s.tracking;
    m.parked   = (s.park == Park::Parked);
    m.homeSet  = s.atHome;                 // NOTE: "at home", not "home defined"
    m.slewing  = s.moving();
    m.slewDir  = SlewDir::None;            // direction comes from the command layer
    m.siteLatDeg = s.siteLatDeg;
    m.raHours  = s.raHours;
    m.decDeg   = s.decDeg;
    m.altDeg   = s.altDeg;
    m.azDeg    = s.azDeg;
    m.pierSide = s.pierSide;
    m.guidePulseActive = (s.move == Move::PulseGuide);
    m.localSiderealHours = s.localSiderealHours;
    m.goingTo  = (s.move == Move::GotoSlew);
    if (s.move == Move::Parking) m.targetName = "PARK";
    else if (s.move == Move::Homing) m.targetName = "HOME";
    if (s.guideRateSel >= '1' && s.guideRateSel <= '9')
        m.slewRate = s.guideRateSel - '0';
    return m;
}

const char* slewCmd(SlewDir d) {
    switch (d) {
        case SlewDir::North: return ":Mn#";
        case SlewDir::South: return ":Ms#";
        case SlewDir::East:  return ":Me#";
        case SlewDir::West:  return ":Mw#";
        default:             return "";
    }
}
const char* stopCmd(SlewDir d) {
    switch (d) {
        case SlewDir::North: return ":Qn#";
        case SlewDir::South: return ":Qs#";
        case SlewDir::East:  return ":Qe#";
        case SlewDir::West:  return ":Qw#";
        default:             return ":Q#";
    }
}
const char* stopAllCmd() { return ":Q#"; }

std::string rateCmd(int r) {
    if (r < 0) r = 0;
    if (r > 9) r = 9;
    std::string s = ":R";
    s += (char)('0' + r);
    s += '#';
    return s;
}
const char* parkCmd()   { return ":hP#"; }
const char* unparkCmd() { return ":hR#"; }
const char* goHomeCmd() { return ":hC#"; }

} // namespace onstep
