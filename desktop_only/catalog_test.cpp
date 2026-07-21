// Numerical verification for Catalog.cpp and Ephemeris.cpp.
// Build: see tools/run_tests.sh
#include "../Catalog.h"
#include "../Ephemeris.h"
#include <cstdio>
#include <cmath>
#include <chrono>
#include <string>

static int failures = 0;
static void check(const char* what, double got, double want, double tol) {
    const bool ok = std::fabs(got - want) <= tol;
    if (!ok) ++failures;
    std::printf("  %-42s got %10.4f want %10.4f  %s\n",
                what, got, want, ok ? "ok" : "FAIL");
}

int main() {
    std::printf("catalog: %u records, %u name bytes, %u constellations\n",
                catalog::kRecordCount, catalog::kNameBlobLen,
                catalog::kConstCount);

    // ---- record round-trip on a known object -------------------------
    // M31 -- RA 00h42m44s = 0.71194 h, Dec +41.2689, 177.83'
    const catalog::Record* m31 = nullptr;
    for (uint16_t i = 0; i < catalog::kRecordCount; ++i) {
        if (std::string(catalog::designation(catalog::kRecords[i])) == "M31") {
            m31 = &catalog::kRecords[i];
            break;
        }
    }
    if (!m31) { std::printf("M31 not found -- FAIL\n"); return 1; }
    std::printf("\nrecord round-trip (M31 = %s, %s, %s)\n",
                catalog::popularName(*m31), catalog::constellation(*m31),
                catalog::typeName(catalog::typeOf(*m31)));
    check("RA hours", catalog::raHours(*m31), 0.71194, 0.0005);
    check("Dec deg", catalog::decDeg(*m31), 41.2689, 0.02);
    check("major arcmin", catalog::majorArcmin(*m31), 177.83, 0.06);
    check("magnitude", catalog::magnitude(*m31), 3.44, 0.06);
    check("surface brightness", catalog::surfBright(*m31), 23.63, 0.06);

    // ---- geometry ----------------------------------------------------
    std::printf("\ngeometry\n");
    // An object on the meridian (HA = 0) at dec = lat must be at the zenith.
    double alt, az;
    catalog::altAzOf(6.0, 41.0, 6.0, 41.0, alt, az);
    check("zenith altitude", alt, 90.0, 0.001);
    check("transit alt, dec=lat", catalog::transitAltitude(41.0, 41.0), 90.0, 1e-9);
    check("transit alt, dec=0 lat=41", catalog::transitAltitude(0.0, 41.0), 49.0, 1e-9);
    check("transit alt, circumpolar", catalog::transitAltitude(80.0, 41.0), 51.0, 1e-9);
    // Celestial equator rising due east.
    catalog::altAzOf(0.0, 0.0, 18.0, 41.0, alt, az);
    check("equator at HA -6h: altitude", alt, 0.0, 0.001);
    check("equator at HA -6h: azimuth", az, 90.0, 0.001);
    check("airmass at zenith", catalog::airmass(90.0), 1.000, 0.002);
    check("airmass at 30 deg", catalog::airmass(30.0), 1.995, 0.01);
    check("hour angle wrap", catalog::hourAngleOf(23.0, 1.0), 2.0, 1e-9);

    // ---- ephemeris ---------------------------------------------------
    std::printf("\nephemeris\n");
    // J2000.0 epoch: JD 2451545.0 = 2000 Jan 1, 12:00 UT
    check("JD of J2000.0", ephem::julianDay(2000, 1, 1, 12.0), 2451545.0, 1e-6);
    check("JD 2026-07-21 00:00", ephem::julianDay(2026, 7, 21, 0.0),
          2461242.5, 1e-6);
    // GMST at 2000 Jan 1 12h UT is 18h 41m 50.5s = 18.69736 h
    check("GMST at J2000.0", ephem::gmst(2451545.0), 18.69736, 0.0005);

    // Sun position, 2026-07-21 00:00 UT. Around this date the sun sits near
    // RA 8h, Dec +20 deg. Reference values from the low-precision series are
    // self-consistent to ~0.01 deg; this is a regression guard, not an
    // independent check against JPL.
    {
        const double jd = ephem::julianDay(2026, 7, 21, 0.0);
        const ephem::Body s = ephem::sun(jd);
        std::printf("  sun  RA %.4f h  Dec %+.3f deg  R %.5f AU\n",
                    s.raHours, s.decDeg, s.distanceAu);
        check("sun RA in expected range", s.raHours, 7.95, 0.35);
        check("sun Dec in expected range", s.decDeg, 20.4, 0.7);
        const ephem::Body m = ephem::moon(jd);
        std::printf("  moon RA %.4f h  Dec %+.3f deg  d %.0f km\n",
                    m.raHours, m.decDeg, m.distanceAu);
        check("moon distance plausible", m.distanceAu, 385000.0, 45000.0);
        double waxing = 0;
        const double k = ephem::moonIllumination(jd, waxing);
        std::printf("  moon illumination %.1f%%  %s\n", k * 100.0,
                    waxing > 0 ? "waxing" : "waning");
        check("illumination in range", k, 0.5, 0.5001);
    }

    // Twilight search at 41 N. Feed it an LST consistent with longitude 0
    // so the derived longitude comes out near zero.
    {
        ephem::Clock c;
        c.year = 2026; c.month = 7; c.day = 21; c.utcHours = 2.0;
        c.valid = true;
        const double jd = c.julianDay();
        const double lst = ephem::gmst(jd);      // longitude 0
        const ephem::SkyState s = ephem::computeSky(c, lst, 41.0);
        std::printf("  regime %s, sun alt %+.2f deg\n",
                    ephem::twilightName(s.regime), s.sunAltDeg);
        if (s.toNextTransition >= 0)
            std::printf("  next: %s in %.0f min\n",
                        ephem::twilightName(s.nextRegime),
                        s.toNextTransition / 60.0);
        std::printf("  dark start %.0f min, dark end %.0f min\n",
                    s.toDarkStart / 60.0, s.toDarkEnd / 60.0);
        std::printf("  moon alt %+.2f, illum %.0f%%, rise %.0f min, set %.0f min\n",
                    s.moonAltDeg, s.moonIllum * 100.0,
                    s.toMoonRise / 60.0, s.toMoonSet / 60.0);
        check("longitude recovered from LST",
              ephem::longitudeFromLst(lst, jd), 0.0, 0.01);
        check("sky state valid", s.valid ? 1.0 : 0.0, 1.0, 0.0);
    }

    // ---- filter model ------------------------------------------------
    std::printf("\nfilter model\n");
    static catalog::View view;
    catalog::Filter f;
    const double lst = 20.0, lat = 41.0;

    f.azMask = 0xFF;
    view.rebuild(f, lst, lat, 90.0);
    const uint16_t all = view.count();
    std::printf("  all sectors, alt>=25          %5u of %u\n",
                all, view.typeMatchCount());

    // West only = sectors 4,5 (180-270 deg). With the zenith exemption on,
    // the count must EXCEED a strict west-only cut.
    f.azMask = (1 << 4) | (1 << 5);
    f.zenithExempt = 70.0f;
    view.rebuild(f, lst, lat, 90.0);
    const uint16_t westExempt = view.count();
    f.zenithExempt = 91.0f;               // exemption disabled
    view.rebuild(f, lst, lat, 90.0);
    const uint16_t westStrict = view.count();
    std::printf("  west sectors, zenith exempt   %5u\n", westExempt);
    std::printf("  west sectors, strict          %5u\n", westStrict);
    check("exemption adds objects", westExempt > westStrict ? 1.0 : 0.0,
          1.0, 0.0);

    // Every extra object must genuinely be above the exemption altitude.
    f.zenithExempt = 70.0f;
    view.rebuild(f, lst, lat, 90.0);
    int badSector = 0;
    for (uint16_t i = 0; i < view.count(); ++i) {
        const catalog::Sky s = view.sky(i);
        const int sec = int(s.azDeg / 45.0) & 7;
        const bool inSector = (f.azMask >> sec) & 1;
        if (!inSector && s.altDeg < f.zenithExempt) ++badSector;
    }
    check("no out-of-window leakage", badSector, 0.0, 0.0);

    // Type filter.
    f.azMask = 0xFF;
    f.typeMask = 1 << catalog::GRP_PLANETARY;
    view.rebuild(f, lst, lat, 90.0);
    int wrongType = 0;
    for (uint16_t i = 0; i < view.count(); ++i)
        if (catalog::groupOf(view.at(i)) != catalog::GRP_PLANETARY) ++wrongType;
    std::printf("  planetary nebulae up          %5u of %u\n",
                view.count(), view.typeMatchCount());
    check("type filter is exact", wrongType, 0.0, 0.0);

    // ---- sort orders are monotonic ------------------------------------
    std::printf("\nsort orders\n");
    f.typeMask = 0x3F;
    const char* names[] = {"ALTITUDE", "TRANSIT", "MAGNITUDE",
                           "SURFBRIGHT", "FOVFIT", "SETTING"};
    for (int m = 0; m < catalog::SORT_COUNT; ++m) {
        f.sort = catalog::SortMode(m);
        view.rebuild(f, lst, lat, 90.0);
        std::printf("  %-11s top: ", names[m]);
        for (uint16_t i = 0; i < 4 && i < view.count(); ++i)
            std::printf("%-12s", catalog::designation(view.at(i)));
        std::printf("\n");
    }
    f.sort = catalog::SORT_ALTITUDE;
    view.rebuild(f, lst, lat, 90.0);
    bool descending = true;
    for (uint16_t i = 1; i < view.count(); ++i)
        if (view.sky(i).altDeg > view.sky(i - 1).altDeg + 0.11)
            descending = false;
    check("altitude sort is descending", descending ? 1.0 : 0.0, 1.0, 0.0);

    // ---- timing -------------------------------------------------------
    std::printf("\ntiming (desktop; ESP32-S3 at 240 MHz is roughly 20-30x slower)\n");
    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < 100; ++i) view.rebuild(f, lst + i * 0.01, lat, 90.0);
    auto t1 = std::chrono::steady_clock::now();
    const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    std::printf("  full rebuild + sort           %.3f ms/call\n", ms / 100.0);

    ephem::Clock c;
    c.year = 2026; c.month = 7; c.day = 21; c.utcHours = 2.0; c.valid = true;
    t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < 100; ++i) (void)ephem::computeSky(c, 20.0, 41.0);
    t1 = std::chrono::steady_clock::now();
    const double ms2 = std::chrono::duration<double, std::milli>(t1 - t0).count();
    std::printf("  computeSky (sun+moon+search)  %.3f ms/call\n", ms2 / 100.0);

    std::printf("\n%s (%d failures)\n", failures ? "FAILURES PRESENT" : "all checks passed",
                failures);
    return failures ? 1 : 0;
}
