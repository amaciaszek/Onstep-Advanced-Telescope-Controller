#pragma once
#include <cstdint>
#include <cstddef>

// =====================================================================
//  Deep-sky catalog, packed to 16 bytes per object in flash.
//
//  The generated tables (Catalog_long.cpp / Catalog_wide.cpp) are built
//  by tools/build_catalog.py from the TheSkyLive TSV. Exactly one of them
//  is compiled in; see CATALOG_BUILD in Settings.h.
//
//  Nothing here allocates. The browser works on a uint16 index array that
//  is rebuilt whenever the filter or the sidereal time changes.
// =====================================================================
namespace catalog {

// Fine-grained object type -- this is the SUBTYPE, and it drives which glyph
// is drawn. It carries the galaxy morphology from the TheSkyLive
// Classification column so an elliptical does not look like a barred spiral.
//
// Values are stable and generated code depends on them; append new ones at
// the end rather than renumbering. gen_assets_v9.py and the glyph table both
// index by this.
enum ObjType : uint8_t {
    // galaxies, by morphology (matches the reference art sheet)
    TYPE_GAL_E       = 0,    // elliptical
    TYPE_GAL_ES0     = 1,    // elliptical / lenticular transition
    TYPE_GAL_S0      = 2,    // lenticular
    TYPE_GAL_S0A     = 3,    // lenticular S0-Aa
    TYPE_GAL_SA      = 4,    // unbarred spiral
    TYPE_GAL_SB      = 5,    // barred spiral
    TYPE_GAL_SAB     = 6,    // intermediate spiral
    TYPE_GAL_IRR     = 7,    // irregular
    TYPE_GAL_GENERIC = 8,    // galaxy, morphology unknown/blank
    // multi-galaxy systems
    TYPE_GAL_PAIR    = 9,
    TYPE_GAL_TRIPLET = 10,
    TYPE_GAL_GROUP   = 11,
    // everything else
    TYPE_EMISSION    = 12,   // emission nebula
    TYPE_HII         = 13,   // HII ionized region
    TYPE_REFLECTION  = 14,   // reflection nebula
    TYPE_DARK        = 15,   // dark nebula
    TYPE_PLANETARY   = 16,
    TYPE_REMNANT     = 17,   // supernova remnant
    TYPE_CLUSTERNEB  = 18,   // star cluster + nebula
    TYPE_GLOBULAR    = 19,
    TYPE_OPENCLUSTER = 20,
    TYPE_NEBULA      = 21,   // generic / unclassified nebula
    TYPE_COUNT       = 22
};

// Filter chips presented in the UI. Several subtypes map to one group; the
// filter and the six top-level chips work at this coarser level, while the
// glyphs and the detail screen use the fine ObjType above.
enum TypeGroup : uint8_t {
    GRP_GALAXY    = 0,   // all TYPE_GAL_*
    GRP_PLANETARY = 1,
    GRP_GLOBULAR  = 2,
    GRP_NEBULA    = 3,   // emission/HII/reflection/dark/generic + cluster+neb
    GRP_CLUSTER   = 4,   // open clusters
    GRP_REMNANT   = 5,
    GRP_COUNT     = 6
};

// One outline+filled glyph pair per ObjType. gen_assets_v9.py emits these.
static constexpr uint8_t GLYPH_COUNT = TYPE_COUNT;

static constexpr uint8_t FLAG_MAG_IS_B  = 0x01;  // magnitude is B, not V
static constexpr uint8_t FLAG_HAS_NAME  = 0x02;  // popular name follows desig

static constexpr uint8_t MAG_NONE = 255;
static constexpr uint8_t SB_NONE  = 255;

struct Record {
    uint16_t ra;        // J2000 hours * (65535/24)
    int16_t  dec;       // J2000 arcminutes
    uint16_t major;     // arcmin * 10
    uint16_t minor;     // arcmin * 10
    uint8_t  pa;        // position angle / 2  (0..179 -> 0..358 deg)
    uint8_t  mag;       // (mag + 3.0) * 10, 255 = unknown
    uint8_t  sb;        // (sb - 15.0) * 10, 255 = unknown (galaxies only)
    uint8_t  type;      // ObjType (subtype). Group is derived from it.
    uint8_t  flags;     // FLAG_MAG_IS_B | FLAG_HAS_NAME
    uint8_t  con;       // index into kConstNames
    uint16_t nameOff;   // byte offset into kNameBlob
};                      // 16 bytes: subtype consumes the former alignment pad
static_assert(sizeof(Record) == 16, "Catalog Record layout changed");

// ---- generated tables ----------------------------------------------------
extern const Record   kRecords[];
extern const uint16_t kRecordCount;
extern const char     kNameBlob[];
extern const uint16_t kNameBlobLen;
extern const char     kConstNames[][4];
extern const uint8_t  kConstCount;

// ---- unpacking -----------------------------------------------------------
inline ObjType typeOf(const Record& r) {
    return static_cast<ObjType>(r.type);
}
inline TypeGroup groupOf(const Record& r);
inline double raHours(const Record& r)  { return r.ra * (24.0 / 65535.0); }
inline double decDeg (const Record& r)  { return r.dec / 60.0; }
inline double majorArcmin(const Record& r) { return r.major * 0.1; }
inline double minorArcmin(const Record& r) { return r.minor * 0.1; }
inline double posAngleDeg(const Record& r) { return r.pa * 2.0; }
inline bool   hasMag(const Record& r)   { return r.mag != MAG_NONE; }
inline double magnitude(const Record& r){ return r.mag * 0.1 - 3.0; }
inline bool   magIsBlue(const Record& r){ return (r.flags & FLAG_MAG_IS_B) != 0; }
inline bool   hasSB(const Record& r)    { return r.sb != SB_NONE; }
inline double surfBright(const Record& r){ return r.sb * 0.1 + 15.0; }
inline bool   isGalaxy(ObjType t) { return t <= TYPE_GAL_GROUP; }

const char* designation(const Record& r);   // always present
const char* popularName(const Record& r);   // "" when FLAG_HAS_NAME is clear
const char* constellation(const Record& r);
const char* typeName(ObjType t);            // full label, e.g. "BARRED SPIRAL"
const char* typeShort(ObjType t);           // 3-4 char list-column label
const char* groupName(TypeGroup g);

// Which of the 6 filter chips a subtype belongs to.
inline TypeGroup groupOfType(ObjType t) {
    if (t <= TYPE_GAL_GROUP)      return GRP_GALAXY;
    switch (t) {
        case TYPE_PLANETARY:      return GRP_PLANETARY;
        case TYPE_GLOBULAR:       return GRP_GLOBULAR;
        case TYPE_OPENCLUSTER:    return GRP_CLUSTER;
        case TYPE_REMNANT:        return GRP_REMNANT;
        default:                  return GRP_NEBULA;  // emission/HII/refl/dark/
                                                      // cluster+neb/generic
    }
}
inline TypeGroup groupOf(const Record& r) { return groupOfType(typeOf(r)); }

// =====================================================================
//  Filter model
//
//  An object is shown when
//
//      (azimuth in a selected sector  AND  alt >= altFloor)
//                          OR
//      (alt >= zenithExempt)
//
//  The second clause is the point: near the zenith azimuth stops meaning
//  anything, so a target 88 degrees up in the north is perfectly visible
//  even when only the western sectors are selected.
// =====================================================================
enum SortMode : uint8_t {
    SORT_ALTITUDE = 0,   // highest first
    SORT_TRANSIT,        // soonest meridian crossing first, past-transit last
    SORT_MAGNITUDE,      // brightest first
    SORT_SURFBRIGHT,     // best surface brightness first (galaxies)
    SORT_FOVFIT,         // fills the configured field best
    SORT_SETTING,        // dropping fastest -- catch it before it's gone
    SORT_COUNT
};

const char* sortName(SortMode m);

struct Filter {
    uint8_t  azMask       = 0xFF;   // bit n = 45-degree sector starting at n*45
    uint8_t  typeMask     = 0x3F;   // bit n = TypeGroup n
    float    altFloor     = 25.0f;  // degrees
    float    zenithExempt = 70.0f;  // degrees; >90 disables the exemption
    SortMode sort         = SORT_ALTITUDE;

    bool group(TypeGroup g) const { return (typeMask >> g) & 1; }
    void toggleGroup(TypeGroup g) { typeMask ^= uint8_t(1u << g); }
    bool sector(int s) const      { return (azMask >> (s & 7)) & 1; }
    void toggleSector(int s)      { azMask ^= uint8_t(1u << (s & 7)); }
};

// Per-object geometry for the current instant. Returned by value; the View
// stores it packed at 6 bytes/record so the whole working set fits in a
// few tens of KB of SRAM.
struct Sky {
    float altDeg;
    float azDeg;
    float hourAngle;     // hours, -12..+12 (negative = still rising)
    float altRate;       // degrees per sidereal hour (negative = setting)
};

// The working set. One instance lives in App; nothing else allocates.
class View {
public:
    // Recompute alt/az for every record and rebuild the filtered, sorted
    // index. Cost is ~4300 * (4 transcendentals) ~= 3 ms at 240 MHz, so
    // call it on filter change and roughly every 30 s -- not per frame.
    void rebuild(const Filter& f, double lstHours, double latDeg,
                 double fovArcmin);

    uint16_t count() const { return count_; }
    const Record& at(uint16_t i) const { return kRecords[index_[i]]; }
    uint16_t recordIndex(uint16_t i) const { return index_[i]; }
    Sky      sky(uint16_t i) const { return unpack(index_[i]); }
    Sky      skyOfRecord(uint16_t rec) const { return unpack(rec); }

    // Total that passed the type filter regardless of sky window -- lets the
    // UI say "412 of 1180 up right now".
    uint16_t typeMatchCount() const { return typeMatch_; }

private:
    // Packed per-record geometry: 6 bytes instead of 16.
    struct SkyPacked {
        int16_t  alt10;   // degrees * 10
        uint16_t az10;    // degrees * 10, 0..3600
        int16_t  rate10;  // degrees per sidereal hour * 10
    };
    Sky unpack(uint16_t rec) const;

    static constexpr uint16_t kMax = 5120;   // covers the 'long' build (~4300)
    uint16_t   index_[kMax];
    float      key_[kMax];      // scratch sort keys, computed once per rebuild
    SkyPacked  sky_[kMax];
    double     lst_ = 0.0;
    uint16_t   count_ = 0;
    uint16_t   typeMatch_ = 0;
};

// Standalone geometry helpers, exposed because the detail screen needs them.
void altAzOf(double raHours, double decDeg, double lstHours, double latDeg,
             double& altDeg, double& azDeg);
double transitAltitude(double decDeg, double latDeg);
double hourAngleOf(double raHours, double lstHours);      // hours, -12..+12
double airmass(double altDeg);                            // <=0 alt -> 99

} // namespace catalog
