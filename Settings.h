#pragma once
#include <cstdint>
#include <cmath>
#include <cstdio>
#include <string>
#include "Catalog.h"
#include "Ephemeris.h"

// =====================================================================
//  User settings that survive across screens. Persisting these to NVS is
//  a platform concern; the shared App only reads and mutates this struct.
// =====================================================================

// Which generated catalog is linked in. Both generated .cpp files can remain
// in the Arduino sketch folder; compile-time guards select exactly one.
//   long  -- galaxies down to 1.5', open clusters <= 60' and mag <= 9
//   wide  -- galaxies down to 5.0', open clusters <= 180' and mag <= 11
#define CATALOG_PROFILE_LONG 1
#define CATALOG_PROFILE_WIDE 2
#ifndef CATALOG_PROFILE
#define CATALOG_PROFILE CATALOG_PROFILE_LONG
#endif

struct Settings {
    ephem::Clock clock;

    // ---- field of view -------------------------------------------------
    // Entered directly as an angle rather than derived from focal length and
    // sensor size: the user knows their framing, and this keeps the settings
    // screen to a single value. Stored in arcminutes throughout; the display
    // switches to degrees past kFovDegreeThreshold so you are never reading
    // "1440 ARCMIN".
    static constexpr double kFovDegreeThreshold = 400.0;   // arcmin
    static constexpr double kFovMin = 5.0;
    static constexpr double kFovMax = 6000.0;              // 100 degrees
    double fovArcmin = 90.0;

    catalog::Filter filter;

    // Increment scales with magnitude so a single joystick sweep can cross
    // the whole range, but fine control survives at the small end.
    static double fovStep(double v) {
        if (v < 30.0)   return 1.0;
        if (v < 120.0)  return 5.0;
        if (v < 400.0)  return 10.0;
        if (v < 1200.0) return 30.0;
        return 60.0;
    }

    void nudgeFov(int dir) {
        const double s = fovStep(fovArcmin + (dir > 0 ? 0.001 : -0.001));
        double v = fovArcmin + dir * s;
        // snap to the step grid so repeated nudges land on round numbers
        v = std::round(v / s) * s;
        if (v < kFovMin) v = kFovMin;
        if (v > kFovMax) v = kFovMax;
        fovArcmin = v;
    }

    bool fovInDegrees() const { return fovArcmin >= kFovDegreeThreshold; }

    std::string fovText() const {
        char buf[24];
        if (fovInDegrees())
            std::snprintf(buf, sizeof buf, "%.2f\xC2\xB0", fovArcmin / 60.0);
        else
            std::snprintf(buf, sizeof buf, "%.0f'", fovArcmin);
        return buf;
    }

    std::string fovUnitLabel() const {
        return fovInDegrees() ? "DEGREES" : "ARCMIN";
    }
};
