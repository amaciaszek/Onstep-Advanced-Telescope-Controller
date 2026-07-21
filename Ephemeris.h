#pragma once
#include <cstdint>

// =====================================================================
//  Sun and Moon ephemeris.
//
//  Everything in Catalog.h runs on sidereal time alone and needs no clock.
//  THIS file is the exception: sun and moon positions require an actual
//  calendar date, which the user enters on first boot (Settings::Clock).
//
//  Accuracy targets, deliberately modest and honest about it:
//    Sun   ~0.01 deg  -- twilight boundaries good to well under a minute
//    Moon  ~0.2 deg   -- phase, altitude and target separation, not occultations
//
//  Low-precision series after Meeus, Astronomical Algorithms ch. 25 and 47.
//  No Arduino dependency; unit-tested on the desktop.
// =====================================================================
namespace ephem {

// Wall-clock the user typed in, held as UTC. The handheld has no RTC by
// default, so this is seeded once and advanced by the main loop's dt.
// Drift of a few seconds an hour is irrelevant for twilight and moonrise.
struct Clock {
    int    year  = 2026;
    int    month = 7;
    int    day   = 21;
    double utcHours = 0.0;   // 0..24
    bool   valid = false;    // false until the user has entered a date

    void advance(double seconds);
    double julianDay() const;
};

double julianDay(int year, int month, int day, double utcHours);

// Greenwich mean sidereal time, hours 0..24.
double gmst(double jd);

// Longitude the site sits at, derived from LST the mount reports and GMST.
// Saves asking the user for a longitude they probably do not know offhand.
double longitudeFromLst(double lstHours, double jd);

struct Body {
    double raHours;
    double decDeg;
    double distanceAu;      // sun: AU. moon: earth radii.
};

Body sun(double jd);
Body moon(double jd);

// Illuminated fraction 0..1 and the sign of the phase angle
// (waxing > 0, waning < 0), from the geocentric elongation.
double moonIllumination(double jd, double& waxingSign);

enum Twilight : uint8_t {
    TW_DAY = 0,        // sun above 0 deg
    TW_CIVIL,          // 0 .. -6
    TW_NAUTICAL,       // -6 .. -12
    TW_ASTRONOMICAL,   // -12 .. -18
    TW_NIGHT,          // below -18: true astronomical dark
    TW_COUNT
};

const char* twilightName(Twilight t);
const char* twilightShort(Twilight t);
Twilight    twilightOf(double sunAltDeg);

// The whole SKY screen in one struct, recomputed once a second.
struct SkyState {
    bool     valid = false;      // false when the clock has not been set

    double   sunAltDeg = 0;
    double   sunAzDeg = 0;
    Twilight regime = TW_DAY;

    // Seconds until the sun next crosses each boundary going down, and the
    // matching boundary going up. Negative when the event will not occur
    // tonight (e.g. no astronomical dark at high summer latitudes).
    double   toNextTransition = -1;   // seconds to the next regime change
    Twilight nextRegime = TW_DAY;
    double   toDarkStart = -1;        // seconds to sun < -18 (or -1)
    double   toDarkEnd = -1;          // seconds to sun > -18 (or -1)

    double   moonAltDeg = 0;
    double   moonAzDeg = 0;
    double   moonRaHours = 0;
    double   moonDecDeg = 0;
    double   moonIllum = 0;           // 0..1
    double   moonWaxing = 1;          // +1 waxing, -1 waning
    double   toMoonRise = -1;         // seconds, -1 if none in 24 h
    double   toMoonSet = -1;
    bool     moonUp = false;
};

// lstHours and latDeg come from the mount (:GS# and :Gt#). The clock supplies
// the date. Sampling is coarse (10-minute steps, then bisection) because the
// sun and moon move slowly; total cost is well under a millisecond.
SkyState computeSky(const Clock& c, double lstHours, double latDeg);

// Angular separation between two equatorial positions, degrees.
double separation(double ra1Hours, double dec1Deg,
                  double ra2Hours, double dec2Deg);

} // namespace ephem
