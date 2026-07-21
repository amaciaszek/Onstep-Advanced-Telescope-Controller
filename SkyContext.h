#pragma once
#include "Catalog.h"
#include "Ephemeris.h"
#include "Settings.h"
#include "MountStatus.h"

// =====================================================================
//  SkyContext -- everything the SKY and CATALOG screens read from.
//
//  Deliberately free of LVGL and Arduino so it stays unit-testable on the
//  desktop (see desktop_only/catalog_test.cpp). The screens are pure views
//  over this; they never compute anything themselves.
//
//  Refresh cadence lives here rather than in the screens, because the cost
//  is real: a full catalog rebuild is milliseconds, not microseconds, and
//  must never land inside an LVGL redraw.
// =====================================================================
class SkyContext {
public:
    using GotoHandler = bool (*)(double raHours, double decDeg,
                                 const char* designation);

    Settings          settings;
    catalog::View     view;
    ephem::SkyState   sky;

    // Sun altitude across a 24 h window centred on now, for the twilight
    // ribbon. 48 samples = one every 30 minutes, which is finer than the
    // ribbon's segment resolution.
    static constexpr int kSunSamples = 48;
    float  sunTrack[kSunSamples] = {0};
    bool   sunTrackValid = false;

    // Currently highlighted / opened object, as an index into kRecords.
    uint16_t selectedRecord = 0xFFFF;
    GotoHandler gotoHandler = nullptr;

    // Call once per main-loop tick with the elapsed milliseconds.
    void tick(uint32_t dtMs, const MountStatus& m) {
        settings.clock.advance(dtMs / 1000.0);
        catalogAgeMs_ += dtMs;
        skyAgeMs_ += dtMs;

        if (m.localSiderealHours < 0.0 || m.siteLatDeg > 90.0) return;

        if (dirty_ || catalogAgeMs_ >= kCatalogPeriodMs) {
            view.rebuild(settings.filter, m.localSiderealHours, m.siteLatDeg,
                         settings.fovArcmin);
            catalogAgeMs_ = 0;
            dirty_ = false;
            generation_++;
        }
        if (skyAgeMs_ >= kSkyPeriodMs) {
            recomputeSky(m);
            skyAgeMs_ = 0;
        }
    }

    // Anything that changes the filter, the sort or the field of view must
    // call this; the rebuild then happens on the next tick, off the UI path.
    void markDirty() { dirty_ = true; }

    // Screens compare this against their own copy to know whether the list
    // they are showing is stale, without diffing contents.
    uint32_t generation() const { return generation_; }

    bool haveSite(const MountStatus& m) const {
        return m.localSiderealHours >= 0.0 && m.siteLatDeg <= 90.0;
    }

    double meridianFlipSeconds(const MountStatus& m) const {
        if (m.localSiderealHours < 0.0) return -1.0;
        const double ha = catalog::hourAngleOf(m.raHours, m.localSiderealHours);
        if (ha >= 0.0) return -1.0;              // already past the meridian
        return -ha * 3600.0 * 0.9972696;         // sidereal hours -> seconds
    }

    // 0..7, matching the moon_N assets. 0 new, 2 first quarter, 4 full.
    int moonPhaseIndex() const {
        if (!sky.valid) return 0;
        // Recover the phase angle from the illuminated fraction, then pick a
        // bucket. The exact percentage is shown as text; this only selects
        // which of the eight sprites to draw.
        double k = sky.moonIllum;
        if (k < 0) k = 0;
        if (k > 1) k = 1;
        const double ang = acos(1.0 - 2.0 * k);        // 0 at new, pi at full
        double frac = ang / 3.14159265358979323846;    // 0..1
        if (sky.moonWaxing < 0) frac = 2.0 - frac;     // 1..2 waning
        int idx = int(frac * 4.0 + 0.5) & 7;
        return idx;
    }

private:
    void recomputeSky(const MountStatus& m) {
        if (!settings.clock.valid) {
            sky.valid = false;
            sunTrackValid = false;
            return;
        }
        sky = ephem::computeSky(settings.clock, m.localSiderealHours,
                                m.siteLatDeg);

        const double jd = settings.clock.julianDay();
        const double lon = ephem::longitudeFromLst(m.localSiderealHours, jd);
        const double jd0 = jd - 0.5;
        for (int i = 0; i < kSunSamples; ++i) {
            const double t = jd0 + double(i) / (kSunSamples - 1);
            const ephem::Body b = ephem::sun(t);
            double lst = ephem::gmst(t) + lon / 15.0;
            while (lst < 0) lst += 24.0;
            while (lst >= 24.0) lst -= 24.0;
            double alt, az;
            catalog::altAzOf(b.raHours, b.decDeg, lst, m.siteLatDeg, alt, az);
            sunTrack[i] = float(alt);
        }
        sunTrackValid = true;
    }

    static constexpr uint32_t kCatalogPeriodMs = 30000;
    static constexpr uint32_t kSkyPeriodMs = 2000;
    uint32_t catalogAgeMs_ = kCatalogPeriodMs;
    uint32_t skyAgeMs_ = kSkyPeriodMs;
    uint32_t generation_ = 0;
    bool     dirty_ = true;
};
