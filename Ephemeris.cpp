#include "Ephemeris.h"
#include "Catalog.h"
#include <cmath>

namespace ephem {

static constexpr double PI  = 3.14159265358979323846;
static constexpr double DEG = PI / 180.0;

static double wrap360(double d) {
    d = std::fmod(d, 360.0);
    return d < 0 ? d + 360.0 : d;
}
static double wrap24(double h) {
    h = std::fmod(h, 24.0);
    return h < 0 ? h + 24.0 : h;
}

// ---------------------------------------------------------------- clock
void Clock::advance(double seconds) {
    if (!valid) return;
    utcHours += seconds / 3600.0;
    while (utcHours >= 24.0) {
        utcHours -= 24.0;
        static const int len[] = {31,28,31,30,31,30,31,31,30,31,30,31};
        int dim = len[month - 1];
        if (month == 2 && ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0))
            dim = 29;
        if (++day > dim) { day = 1; if (++month > 12) { month = 1; ++year; } }
    }
}

double Clock::julianDay() const {
    return ephem::julianDay(year, month, day, utcHours);
}

double julianDay(int y, int m, int d, double utcHours) {
    if (m <= 2) { y -= 1; m += 12; }
    const int A = y / 100;
    const int B = 2 - A + A / 4;
    const double jd = std::floor(365.25 * (y + 4716)) +
                      std::floor(30.6001 * (m + 1)) + d + B - 1524.5;
    return jd + utcHours / 24.0;
}

double gmst(double jd) {
    const double T = (jd - 2451545.0) / 36525.0;
    double theta = 280.46061837 + 360.98564736629 * (jd - 2451545.0) +
                   T * T * (0.000387933 - T / 38710000.0);
    return wrap360(theta) / 15.0;
}

double longitudeFromLst(double lstHours, double jd) {
    double lon = (lstHours - gmst(jd)) * 15.0;
    while (lon > 180.0) lon -= 360.0;
    while (lon < -180.0) lon += 360.0;
    return lon;
}

// ---------------------------------------------------------------- sun
Body sun(double jd) {
    const double n = jd - 2451545.0;
    const double L = wrap360(280.460 + 0.9856474 * n);          // mean long
    const double g = wrap360(357.528 + 0.9856003 * n) * DEG;    // mean anomaly
    const double lambda = (L + 1.915 * std::sin(g) +
                           0.020 * std::sin(2 * g)) * DEG;      // ecliptic long
    const double eps = (23.439 - 0.0000004 * n) * DEG;
    const double R = 1.00014 - 0.01671 * std::cos(g) - 0.00014 * std::cos(2 * g);

    Body b;
    b.raHours = wrap24(std::atan2(std::cos(eps) * std::sin(lambda),
                                  std::cos(lambda)) / DEG / 15.0);
    b.decDeg = std::asin(std::sin(eps) * std::sin(lambda)) / DEG;
    b.distanceAu = R;
    return b;
}

// ---------------------------------------------------------------- moon
// Meeus low-precision lunar theory: the largest handful of periodic terms.
// Good to roughly 0.2 deg, which is fine for phase, altitude and separation.
Body moon(double jd) {
    const double T = (jd - 2451545.0) / 36525.0;

    const double Lp = wrap360(218.316 + 481267.8813 * T);        // mean long
    const double M  = wrap360(357.529 + 35999.0503 * T) * DEG;   // sun anomaly
    const double Mp = wrap360(134.963 + 477198.8676 * T) * DEG;  // moon anomaly
    const double D  = wrap360(297.850 + 445267.1115 * T) * DEG;  // elongation
    const double F  = wrap360(93.272 + 483202.0175 * T) * DEG;   // arg latitude

    const double lon = Lp
        + 6.289 * std::sin(Mp)
        - 1.274 * std::sin(2 * D - Mp)
        + 0.658 * std::sin(2 * D)
        - 0.214 * std::sin(2 * Mp)
        - 0.186 * std::sin(M)
        - 0.114 * std::sin(2 * F)
        + 0.059 * std::sin(2 * D - 2 * Mp)
        + 0.057 * std::sin(2 * D - M - Mp)
        + 0.053 * std::sin(2 * D + Mp)
        + 0.046 * std::sin(2 * D - M)
        - 0.041 * std::sin(M - Mp);

    const double lat =
          5.128 * std::sin(F)
        + 0.281 * std::sin(Mp + F)
        - 0.278 * std::sin(F - Mp)
        - 0.173 * std::sin(2 * D - F)
        + 0.055 * std::sin(2 * D - Mp + F)
        - 0.046 * std::sin(2 * D - Mp - F)
        + 0.033 * std::sin(2 * D + F)
        + 0.017 * std::sin(2 * Mp + F);

    const double dist = 385000.56
        - 20905.0 * std::cos(Mp)
        -  3699.0 * std::cos(2 * D - Mp)
        -  2956.0 * std::cos(2 * D)
        -   570.0 * std::cos(2 * Mp)
        +   246.0 * std::cos(2 * D - 2 * Mp);

    const double l = lon * DEG, b = lat * DEG;
    const double eps = (23.4393 - 0.0000004 * (jd - 2451545.0)) * DEG;
    const double sl = std::sin(l), cl = std::cos(l);
    const double sb = std::sin(b), cb = std::cos(b);
    const double se = std::sin(eps), ce = std::cos(eps);

    Body out;
    out.raHours = wrap24(std::atan2(sl * ce - (sb / cb) * se, cl) / DEG / 15.0);
    out.decDeg = std::asin(sb * ce + cb * se * sl) / DEG;
    out.distanceAu = dist;     // km, despite the field name
    return out;
}

double moonIllumination(double jd, double& waxingSign) {
    const Body s = sun(jd);
    const Body m = moon(jd);
    const double elong = separation(s.raHours, s.decDeg, m.raHours, m.decDeg);
    // Phase angle of the moon as seen from earth, small-parallax approximation.
    const double phase = 180.0 - elong;
    const double k = (1.0 + std::cos(phase * DEG)) / 2.0;
    // Waxing when the moon leads the sun in ecliptic longitude, which for our
    // purposes is well approximated by the RA difference.
    double dra = m.raHours - s.raHours;
    while (dra < 0) dra += 24.0;
    while (dra >= 24.0) dra -= 24.0;
    waxingSign = (dra < 12.0) ? 1.0 : -1.0;
    return k;
}

double separation(double ra1H, double dec1, double ra2H, double dec2) {
    const double d1 = dec1 * DEG, d2 = dec2 * DEG;
    const double dra = (ra1H - ra2H) * 15.0 * DEG;
    double c = std::sin(d1) * std::sin(d2) +
               std::cos(d1) * std::cos(d2) * std::cos(dra);
    if (c > 1.0) c = 1.0;
    if (c < -1.0) c = -1.0;
    return std::acos(c) / DEG;
}

// ---------------------------------------------------------------- twilight
const char* twilightName(Twilight t) {
    switch (t) {
        case TW_DAY:           return "DAYLIGHT";
        case TW_CIVIL:         return "CIVIL TWILIGHT";
        case TW_NAUTICAL:      return "NAUTICAL TWILIGHT";
        case TW_ASTRONOMICAL:  return "ASTRO TWILIGHT";
        default:               return "ASTRO DARK";
    }
}
const char* twilightShort(Twilight t) {
    switch (t) {
        case TW_DAY:           return "DAY";
        case TW_CIVIL:         return "CIVIL";
        case TW_NAUTICAL:      return "NAUT";
        case TW_ASTRONOMICAL:  return "ASTRO";
        default:               return "DARK";
    }
}
Twilight twilightOf(double alt) {
    if (alt > 0.0)   return TW_DAY;
    if (alt > -6.0)  return TW_CIVIL;
    if (alt > -12.0) return TW_NAUTICAL;
    if (alt > -18.0) return TW_ASTRONOMICAL;
    return TW_NIGHT;
}

// ---------------------------------------------------------------- search
namespace {

// Altitude of a body at jd, for the site defined by longitude and latitude.
double altAt(bool isMoon, double jd, double lonDeg, double latDeg,
             double* raOut = nullptr, double* decOut = nullptr,
             double* azOut = nullptr) {
    const Body b = isMoon ? moon(jd) : sun(jd);
    const double lst = wrap24(gmst(jd) + lonDeg / 15.0);
    double alt, az;
    catalog::altAzOf(b.raHours, b.decDeg, lst, latDeg, alt, az);
    if (raOut)  *raOut = b.raHours;
    if (decOut) *decOut = b.decDeg;
    if (azOut)  *azOut = az;
    return alt;
}

// Scan forward up to `hours` looking for the first crossing of `target`
// altitude in the given direction. Returns seconds, or -1 if none.
// Ten-minute coarse steps then five rounds of bisection: worst case about
// 150 evaluations, still microseconds.
double findCrossing(bool isMoon, double jd0, double lonDeg, double latDeg,
                    double targetAlt, int direction, double hours) {
    const double step = 10.0 / 60.0 / 24.0;      // 10 minutes in days
    double t0 = jd0;
    double a0 = altAt(isMoon, t0, lonDeg, latDeg);
    const double limit = jd0 + hours / 24.0;

    for (double t1 = jd0 + step; t1 <= limit; t1 += step) {
        const double a1 = altAt(isMoon, t1, lonDeg, latDeg);
        const bool crossed = direction > 0 ? (a0 <= targetAlt && a1 > targetAlt)
                                           : (a0 >= targetAlt && a1 < targetAlt);
        if (crossed) {
            double lo = t0, hi = t1;
            for (int i = 0; i < 20; ++i) {
                const double mid = 0.5 * (lo + hi);
                const double am = altAt(isMoon, mid, lonDeg, latDeg);
                const bool aboveTarget = am > targetAlt;
                if ((direction > 0) == aboveTarget) hi = mid; else lo = mid;
            }
            return (0.5 * (lo + hi) - jd0) * 86400.0;
        }
        t0 = t1; a0 = a1;
    }
    return -1.0;
}

} // namespace

SkyState computeSky(const Clock& c, double lstHours, double latDeg) {
    SkyState s;
    if (!c.valid || latDeg > 90.0 || lstHours < 0.0) return s;
    s.valid = true;

    const double jd = c.julianDay();
    const double lon = longitudeFromLst(lstHours, jd);

    // ---- sun ----
    double ra, dec, az;
    s.sunAltDeg = altAt(false, jd, lon, latDeg, &ra, &dec, &az);
    s.sunAzDeg = az;
    s.regime = twilightOf(s.sunAltDeg);

    // Next regime change: the sun is either descending (look for the next
    // lower boundary) or ascending (the next higher one).
    const double ahead = altAt(false, jd + 1.0 / 1440.0, lon, latDeg);
    const bool descending = ahead < s.sunAltDeg;
    static const double kBounds[] = {0.0, -6.0, -12.0, -18.0};

    double best = -1.0;
    Twilight bestRegime = s.regime;
    for (int i = 0; i < 4; ++i) {
        const int dir = descending ? -1 : +1;
        const double t = findCrossing(false, jd, lon, latDeg, kBounds[i],
                                      dir, 24.0);
        if (t >= 0 && (best < 0 || t < best)) {
            best = t;
            bestRegime = descending ? Twilight(i + 1) : Twilight(i);
        }
    }
    s.toNextTransition = best;
    s.nextRegime = bestRegime;

    s.toDarkStart = findCrossing(false, jd, lon, latDeg, -18.0, -1, 24.0);
    s.toDarkEnd   = findCrossing(false, jd, lon, latDeg, -18.0, +1, 24.0);

    // ---- moon ----
    s.moonAltDeg = altAt(true, jd, lon, latDeg, &s.moonRaHours, &s.moonDecDeg,
                         &s.moonAzDeg);
    s.moonUp = s.moonAltDeg > 0.0;
    s.moonIllum = moonIllumination(jd, s.moonWaxing);
    // Standard refraction/semidiameter allowance for lunar rise and set.
    s.toMoonRise = findCrossing(true, jd, lon, latDeg, -0.583, +1, 24.0);
    s.toMoonSet  = findCrossing(true, jd, lon, latDeg, -0.583, -1, 24.0);

    return s;
}

} // namespace ephem
