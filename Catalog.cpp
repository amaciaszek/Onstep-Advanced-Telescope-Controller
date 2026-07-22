#include "Catalog.h"
#include <cmath>
#include <cstring>
#include <cmath>

namespace catalog {

static constexpr double DEG = 3.14159265358979323846 / 180.0;

// ---------------------------------------------------------------- strings
const char* designation(const Record& r) {
    return &kNameBlob[r.nameOff];
}

const char* popularName(const Record& r) {
    if (!(r.flags & FLAG_HAS_NAME)) return "";
    const char* p = &kNameBlob[r.nameOff];
    while (*p) ++p;
    return p + 1;
}

const char* constellation(const Record& r) {
    return r.con < kConstCount ? kConstNames[r.con] : "---";
}

const char* typeName(ObjType t) {
    switch (t) {
        case TYPE_GAL_E:       return "ELLIPTICAL GALAXY";
        case TYPE_GAL_ES0:     return "ELLIPTICAL/LENTICULAR";
        case TYPE_GAL_S0:      return "LENTICULAR GALAXY";
        case TYPE_GAL_S0A:     return "LENTICULAR (S0-Aa)";
        case TYPE_GAL_SA:      return "SPIRAL GALAXY";
        case TYPE_GAL_SB:      return "BARRED SPIRAL";
        case TYPE_GAL_SAB:     return "INTERMEDIATE SPIRAL";
        case TYPE_GAL_IRR:     return "IRREGULAR GALAXY";
        case TYPE_GAL_GENERIC: return "GALAXY";
        case TYPE_GAL_PAIR:    return "GALAXY PAIR";
        case TYPE_GAL_TRIPLET: return "GALAXY TRIPLET";
        case TYPE_GAL_GROUP:   return "GROUP OF GALAXIES";
        case TYPE_EMISSION:    return "EMISSION NEBULA";
        case TYPE_HII:         return "HII IONIZED REGION";
        case TYPE_REFLECTION:  return "REFLECTION NEBULA";
        case TYPE_DARK:        return "DARK NEBULA";
        case TYPE_PLANETARY:   return "PLANETARY NEBULA";
        case TYPE_REMNANT:     return "SUPERNOVA REMNANT";
        case TYPE_CLUSTERNEB:  return "CLUSTER + NEBULA";
        case TYPE_GLOBULAR:    return "GLOBULAR CLUSTER";
        case TYPE_OPENCLUSTER: return "OPEN CLUSTER";
        default:               return "NEBULA";
    }
}

const char* typeShort(ObjType t) {
    switch (t) {
        case TYPE_GAL_E:       return "E";
        case TYPE_GAL_ES0:     return "E-S0";
        case TYPE_GAL_S0:      return "S0";
        case TYPE_GAL_S0A:     return "S0a";
        case TYPE_GAL_SA:      return "SA";
        case TYPE_GAL_SB:      return "SB";
        case TYPE_GAL_SAB:     return "SAB";
        case TYPE_GAL_IRR:     return "Irr";
        case TYPE_GAL_GENERIC: return "GAL";
        case TYPE_GAL_PAIR:    return "PAIR";
        case TYPE_GAL_TRIPLET: return "TRIP";
        case TYPE_GAL_GROUP:   return "GRP";
        case TYPE_EMISSION:    return "EN";
        case TYPE_HII:         return "HII";
        case TYPE_REFLECTION:  return "RN";
        case TYPE_DARK:        return "DN";
        case TYPE_PLANETARY:   return "PN";
        case TYPE_REMNANT:     return "SNR";
        case TYPE_CLUSTERNEB:  return "C+N";
        case TYPE_GLOBULAR:    return "GC";
        case TYPE_OPENCLUSTER: return "OC";
        default:               return "NEB";
    }
}

const char* groupName(TypeGroup g) {
    switch (g) {
        case GRP_GALAXY:    return "GALAXY";
        case GRP_PLANETARY: return "PLANETARY";
        case GRP_GLOBULAR:  return "GLOBULAR";
        case GRP_NEBULA:    return "NEBULA";
        case GRP_CLUSTER:   return "CLUSTER";
        default:            return "REMNANT";
    }
}

const char* sortName(SortMode m) {
    switch (m) {
        case SORT_ALTITUDE:   return "ALTITUDE";
        case SORT_TRANSIT:    return "TRANSIT";
        case SORT_MAGNITUDE:  return "BRIGHTEST";
        case SORT_SURFBRIGHT: return "SURF BRIGHT";
        case SORT_FOVFIT:     return "FOV FIT";
        default:              return "SETTING";
    }
}

// ---------------------------------------------------------------- geometry
double hourAngleOf(double raHours, double lstHours) {
    double ha = lstHours - raHours;
    while (ha < -12.0) ha += 24.0;
    while (ha >= 12.0) ha -= 24.0;
    return ha;
}

void altAzOf(double raHours, double decDeg, double lstHours, double latDeg,
             double& altDeg, double& azDeg) {
    const double ha = hourAngleOf(raHours, lstHours) * 15.0 * DEG;
    const double d  = decDeg * DEG;
    const double p  = latDeg * DEG;
    const double sd = std::sin(d), cd = std::cos(d);
    const double sp = std::sin(p), cp = std::cos(p);
    const double ch = std::cos(ha), sh = std::sin(ha);

    double sinAlt = sd * sp + cd * cp * ch;
    if (sinAlt > 1.0) sinAlt = 1.0;
    if (sinAlt < -1.0) sinAlt = -1.0;
    const double alt = std::asin(sinAlt);
    double az = std::atan2(-cd * sh, sd * cp - cd * sp * ch);
    az /= DEG;
    if (az < 0) az += 360.0;
    altDeg = alt / DEG;
    azDeg  = az;
}

// Single-precision hot path. The ESP32-S3 FPU is single-precision ONLY, so
// every double operation above is soft-float. Inside rebuild() we run over
// several thousand records, where 0.1 degree accuracy is plenty and float
// math is roughly 5x cheaper. Measured, not assumed -- see catalog_test.
static void altAzFast(float raH, float decD, float lstH, float sinLat,
                      float cosLat, float& altOut, float& azOut,
                      float& haOut) {
    float ha = lstH - raH;
    while (ha < -12.0f) ha += 24.0f;
    while (ha >= 12.0f) ha -= 24.0f;
    haOut = ha;
    const float har = ha * 15.0f * 0.017453292f;
    const float d = decD * 0.017453292f;
    const float sd = sinf(d), cd = cosf(d);
    const float ch = cosf(har), sh = sinf(har);

    float sinAlt = sd * sinLat + cd * cosLat * ch;
    if (sinAlt > 1.0f) sinAlt = 1.0f;
    if (sinAlt < -1.0f) sinAlt = -1.0f;
    altOut = asinf(sinAlt) * 57.29578f;
    float az = atan2f(-cd * sh, sd * cosLat - cd * sinLat * ch) * 57.29578f;
    if (az < 0) az += 360.0f;
    azOut = az;
}

double transitAltitude(double decDeg, double latDeg) {
    double a = 90.0 - std::fabs(latDeg - decDeg);
    // Below the pole the object culminates on the other side of the meridian.
    if (a > 90.0) a = 180.0 - a;
    return a;
}

double airmass(double altDeg) {
    if (altDeg <= 3.0) return 99.0;
    // Pickering (2002). Accurate to well under 1% down to the horizon and
    // cheap enough to evaluate per object on the S3.
    const double h = altDeg;
    return 1.0 / std::sin((h + 244.0 / (165.0 + 47.0 * std::pow(h, 1.1)))
                          * DEG);
}

// ---------------------------------------------------------------- sorting
namespace {

struct SortCtx {
    const View* view;
    double      fovArcmin;
    SortMode    mode;
};

// Keys are computed ONCE per object into a scratch array, then the index is
// sorted against that array. Evaluating sortKey inside the comparator instead
// costs O(n log n) transcendentals and measured ~25x slower on hardware.
float sortKey(const SortCtx& c, uint16_t rec) {
    const Record& r = kRecords[rec];
    const Sky s = c.view->skyOfRecord(rec);
    switch (c.mode) {
        case SORT_ALTITUDE:
            return float(-s.altDeg);                  // highest first
        case SORT_TRANSIT: {
            // hours until the next meridian crossing; already-transited
            // objects wrap to the back of the list
            double t = -s.hourAngle;
            if (t < 0) t += 24.0;
            return float(t);
        }
        case SORT_MAGNITUDE:
            return hasMag(r) ? float(magnitude(r)) : 90.0f;
        case SORT_SURFBRIGHT:
            return hasSB(r) ? float(surfBright(r)) : 90.0f;
        case SORT_FOVFIT: {
            // best fill without overflowing: aim for ~60% of the field
            const double frac = majorArcmin(r) / (c.fovArcmin > 1.0
                                                  ? c.fovArcmin : 1.0);
            return float(std::fabs(frac - 0.60) + (frac > 1.0 ? 10.0 : 0.0));
        }
        case SORT_SETTING:
            // most negative altitude rate first -- catch it before it goes
            return s.altRate;
        default:
            return 0.0f;
    }
}

// In-place heapsort against a precomputed key array. No allocation, no
// recursion, O(n log n), stable enough for a display list.
void heapSort(uint16_t* idx, float* key, uint16_t n) {
    auto swap = [&](uint16_t a, uint16_t b) {
        const uint16_t ti = idx[a]; idx[a] = idx[b]; idx[b] = ti;
        const float tk = key[a]; key[a] = key[b]; key[b] = tk;
    };
    auto sift = [&](uint16_t root, uint16_t end) {
        while (true) {
            uint16_t child = uint16_t(2 * root + 1);
            if (child >= end) break;
            if (child + 1 < end && key[child] < key[child + 1]) ++child;
            if (key[root] < key[child]) { swap(root, child); root = child; }
            else break;
        }
    };
    if (n < 2) return;
    for (int i = n / 2 - 1; i >= 0; --i) sift(uint16_t(i), n);
    for (uint16_t end = uint16_t(n - 1); end > 0; --end) {
        swap(0, end);
        sift(0, end);
    }
}

} // namespace

// ---------------------------------------------------------------- rebuild
Sky View::unpack(uint16_t rec) const {
    Sky s;
    s.altDeg    = sky_[rec].alt10 * 0.1f;
    s.azDeg     = sky_[rec].az10 * 0.1f;
    s.altRate   = sky_[rec].rate10 * 0.1f;
    s.hourAngle = float(hourAngleOf(raHours(kRecords[rec]), lst_));
    return s;
}

void View::rebuild(const Filter& f, double lstHours, double latDeg,
                   double fovArcmin) {
    count_ = 0;
    typeMatch_ = 0;
    lst_ = lstHours;
    const uint16_t n = kRecordCount < kMax ? kRecordCount : kMax;
    const float lstF   = float(lstHours);
    const float sinLat = sinf(float(latDeg) * 0.017453292f);
    const float cosLat = cosf(float(latDeg) * 0.017453292f);

    for (uint16_t i = 0; i < n; ++i) {
        const Record& r = kRecords[i];
        if (!f.group(groupOf(r))) continue;
        ++typeMatch_;

        float alt, az, ha;
        const float dec = float(decDeg(r));
        altAzFast(float(raHours(r)), dec, lstF, sinLat, cosLat, alt, az, ha);

        // d(alt)/dHA in degrees per sidereal hour; negative once past
        // the meridian. Reuses the cosines already in registers.
        const float ca = cosf(alt * 0.017453292f);
        float rate = 0.0f;
        if (fabsf(ca) > 1e-4f)
            rate = -cosLat * cosf(dec * 0.017453292f) *
                   sinf(ha * 15.0f * 0.017453292f) / ca * 15.0f;

        sky_[i].alt10  = int16_t(alt * 10.0f);
        sky_[i].az10   = uint16_t(az < 0 ? 0 : az * 10.0f);
        sky_[i].rate10 = int16_t(rate * 10.0f < -32000.0f ? -32000.0f
                        : (rate * 10.0f > 32000.0f ? 32000.0f : rate * 10.0f));

        const bool zenith = alt >= f.zenithExempt;
        if (!zenith) {
            if (alt < f.altFloor) continue;
            const int sector = int(az / 45.0) & 7;
            if (!f.sector(sector)) continue;
        }
        index_[count_++] = i;
        if (count_ >= kMax) break;
    }

    SortCtx c{this, fovArcmin, f.sort};
    for (uint16_t i = 0; i < count_; ++i) key_[i] = sortKey(c, index_[i]);
    heapSort(index_, key_, count_);
}

} // namespace catalog
