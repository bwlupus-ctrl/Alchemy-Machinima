/** @file alworldshadowmath.h
 * Pure, allocation-free maths of the world-space cube shadow maps (Relight P1-lite): the face table, the widened
 * field of view, the atlas grid, the receiver -> tile mapping, the depth linearisation, the contact hand-off band,
 * the resolve ledger, the pinned residency rule and the Director readout wording. No GL, no globals, no time input.
 * The GLSL copy of the face table (areaShadowFilterF.glsl) is text-pinned by relight_shader_check.py.
 *
 * Public references: PCF (Reeves 1987), PCSS (Fernando 2005), normal-offset shadows (Holbert 2011), slope-scaled
 * depth bias, cube shadow maps with guard bands.
 */
#ifndef AL_WORLD_SHADOW_MATH_H
#define AL_WORLD_SHADOW_MATH_H
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
namespace ALWorldShadow
{
// ---------------------------------------------------------------------------------------------------- geometry
constexpr unsigned kFaces = 6;          // faces per light
constexpr unsigned kMaxLights = 2;      // resident lights (setting 1-2)
constexpr unsigned kUsable = 512;       // usable texels per face edge
constexpr unsigned kGutter = 16;        // guard texels per side: holds REAL depth of the face's own projection
constexpr unsigned kTile = kUsable + 2 * kGutter; // 544
constexpr unsigned kColumns = 3;        // tile columns of the atlas
constexpr unsigned kRowsPerLight = 2;   // tile rows per light (3 x 2 faces)
constexpr unsigned kAtlasWidth = kColumns * kTile;                 // 1632
constexpr unsigned kAtlasHeightPerLight = kRowsPerLight * kTile;   // 1088
constexpr double kTanHalfFov = double(kTile) / double(kUsable);    // tan(half the widened FOV) = 1.0625
constexpr double kPi = 3.14159265358979323846;
// Widened FOV 2*atan(544/512) = 93.47 degrees: the usable 512 interior maps to exactly 90 degrees.
inline double widenedFovRadians() { return 2.0 * std::atan(kTanHalfFov); }
inline double widenedFovDegrees() { return widenedFovRadians() * 180.0 / kPi; }

struct Vec3 { double x, y, z; };
// One cube face: look axis, and the camera up vector the probe code uses (LLViewerWindow::cubeSnapshot look_dirs /
// look_upvecs, LLCubeMapArray order +X -X +Y -Y +Z -Z). right = normalize(cross(axis, up)) and
// up' = cross(right, axis), exactly the rows glm::lookAt builds: the view matrix rows are (right, up', -axis).
struct Face { Vec3 axis, up; };
constexpr std::array<Face, kFaces> kFaceTable = {{
    {{ 1, 0, 0}, {0,-1, 0}},
    {{-1, 0, 0}, {0,-1, 0}},
    {{ 0, 1, 0}, {0, 0, 1}},
    {{ 0,-1, 0}, {0, 0,-1}},
    {{ 0, 0, 1}, {0,-1, 0}},
    {{ 0, 0,-1}, {0,-1, 0}},
}};
inline Vec3 cross(const Vec3& a, const Vec3& b)
{ return {a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x}; }
inline double dot(const Vec3& a, const Vec3& b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
inline Vec3 normalized(const Vec3& v)
{
    const double l = std::sqrt(dot(v, v));
    return l > 0.0 ? Vec3{v.x/l, v.y/l, v.z/l} : Vec3{0, 0, 0};
}
inline Vec3 faceRight(unsigned face) { return normalized(cross(kFaceTable[face % kFaces].axis, kFaceTable[face % kFaces].up)); }
inline Vec3 faceUp(unsigned face)
{ return normalized(cross(faceRight(face), kFaceTable[face % kFaces].axis)); }
inline Vec3 faceAxis(unsigned face) { return kFaceTable[face % kFaces].axis; }

// Atlas tile of face f of resident light k: column f % 3, row 2k + f / 3 (pixels).
struct TileSlot { unsigned x, y; };
inline TileSlot tileSlot(unsigned light, unsigned face)
{ return {(face % kColumns) * kTile, (kRowsPerLight * light + face / kColumns) * kTile}; }
inline unsigned atlasHeight(unsigned lights) { return kAtlasHeightPerLight * std::clamp(lights, 1u, kMaxLights); }
inline std::uint64_t atlasBytes(unsigned lights)
{ return std::uint64_t(kAtlasWidth) * atlasHeight(lights) * 4u; }          // D32F
inline std::uint64_t scratchBytes() { return std::uint64_t(kTile) * kTile * 4u; }

// The face a direction (receiver - light centre, in the AGENT axes) belongs to: the major axis, ties to the lower face
// index. Returns the axial depth z (> 0) and the in-tile texel coordinates (floating point, texel centres at +0.5).
struct FaceSample { unsigned face; double z, x, y; };
inline FaceSample faceSample(const Vec3& d)
{
    const double ax = std::abs(d.x), ay = std::abs(d.y), az = std::abs(d.z);
    unsigned face;
    if (ax >= ay && ax >= az) face = d.x >= 0 ? 0u : 1u;
    else if (ay >= az) face = d.y >= 0 ? 2u : 3u;
    else face = d.z >= 0 ? 4u : 5u;
    const double z = dot(d, faceAxis(face));
    const double sx = dot(d, faceRight(face)) / z, sy = dot(d, faceUp(face)) / z;
    // texel = tile/2 * (1 + (x/z) / tan(half FOV)) = tile/2 + usable/2 * (x/z)
    return {face, z, 0.5 * kTile + 0.5 * kUsable * sx, 0.5 * kTile + 0.5 * kUsable * sy};
}
// Texel of a unit-slope (45 degree) ray on a face edge: the usable interior ends at tile/2 + usable/2.
inline double usableEdgeTexel() { return 0.5 * kTile + 0.5 * kUsable; }

// ---------------------------------------------------------------------------------------------------- depth
// Window depth d in [0,1] of an axial distance z for the face projection (near n, far f), and its inverse
// z = 2fn / ((f+n) - (2d-1)(f-n)).
inline double depthFromLinear(double z, double n, double f)
{ return 0.5 * ((f + n) / (f - n) - 2.0*f*n / ((f - n) * z)) + 0.5; }
inline double linearize(double d, double n, double f)
{ return 2.0*f*n / ((f + n) - (2.0*d - 1.0) * (f - n)); }
// Face near plane: a FIXED 5 cm (user decision 10/23), decoupled from the Light bias setting (which keeps governing the screen trace only).
// The region left out is a CUBE of half-size 5 cm around the light, not a sphere. A 25 cm plane (the earlier max(5 cm, Light bias) rule) erased a lamp
// shade or housing 9-11 cm from the light (scene W: wall above the lamp lit 1.0 against a ground truth of 0.0); 5 cm restores it. Far = 1.5 x the light radius.
constexpr double kNearPlane = 0.05;
inline double nearPlane() { return kNearPlane; }
// Runtime near plane from RenderAreaShadowWorldNear (metres): clamped to 2 cm..1 m, non-finite = the default. The no-argument form is the default (harness, tests).
inline double nearPlane(double setting)
{ return std::isfinite(setting) ? std::min(std::max(setting, 0.02), 1.0) : kNearPlane; }
// Per-light near plane (10/23 lead decision): the floor is the RenderAreaShadowWorldNear setting; the light sits at its object's centre, so a closed
// emitter (a bulb) encloses it and fills every face when double-sided or inward-wound. The near plane therefore clears the emitter's inscribed sphere
// (half of the smallest scale axis) plus 1 cm; clamped to 1 m and to 0.9 x the light radius. Separate shade prims further out still cast.
// The raster AND the resolve use this same value (the resolve linearises depth with it).
inline double emitterNearPlane(double setting, double emitterMinScale, double lightRadius)
{
    double n = nearPlane(setting);
    if (std::isfinite(emitterMinScale) && emitterMinScale > 0.0) n = std::max(n, 0.5 * emitterMinScale + 0.01);
    n = std::min(n, 1.0);
    if (std::isfinite(lightRadius) && lightRadius > 0.0) n = std::min(n, 0.9 * lightRadius);
    return n;
}
inline double farPlane(double lightRadius) { return 1.5 * std::max(lightRadius, 0.0); }

// ---------------------------------------------------------------------------------------------------- bias
// One constant set for every scene (tuned on the 3D harness, never per scene). KEEP IN SYNC with the constants of
// areaShadowFilterF.glsl (relight_shader_check.py pins them). texel_w = 2 z / 512 is the world size of one texel at
// axial depth z.
constexpr double kNormalOffsetTexels = 1.0, kNormalOffsetCap = 0.05;   // receiver offset along the geometric normal
constexpr double kConstantBias = 0.003;                                // metres
constexpr double kSlopeTexels = 1.0, kTanCap = 8.0;                    // slope-scaled compare bias
constexpr double kContactTanCap = 2.0;     // the contact band covers receivers up to ~63 degrees from the light
inline double texelWorld(double z) { return 2.0 * z / double(kUsable); }
inline double normalOffset(double z) { return std::min(kNormalOffsetTexels * texelWorld(z), kNormalOffsetCap); }
inline double compareBias(double z, double tanTheta)
{ return kConstantBias + kSlopeTexels * texelWorld(z) * std::min(std::max(tanTheta, 0.0), kTanCap); }
// The map's blind band: a blocker closer than this to the receiver along the light ray is not seen by the map (the
// normal offset and the compare bias eat it). B_max is its worst case over the light's range at the contact tan cap,
// used as a CONSTANT contact distance by the CPU (a per-pixel form comes with P3).
inline double blindBandMax(double farM)
{ return normalOffset(farM) + kConstantBias + kSlopeTexels * texelWorld(farM) * kContactTanCap; }
// The contact distance a world-owned channel uploads: the user's "mapped contact" setting, never below the blind band,
// clamped to [2 cm, 0.5 m].
inline double contactDistance(double userContact, double farM)
{
    const double user = std::isfinite(userContact) ? userContact : 0.15;
    return std::clamp(std::max(user, blindBandMax(farM)), 0.02, 0.5);
}

// ---------------------------------------------------------------------------------------------------- budget
// World resolve ledger (pure, integer): full-resolution pixels x resident lights x 48 gathers (16 blocker search taps +
// 32 filter taps). Ceiling 1.0 G; above it the light count falls to 1 and then to 0 (off) at plan time, never per frame.
constexpr std::uint64_t kResolveGathers = 48;
constexpr std::uint64_t kResolveCeiling = 1000000000ull;
inline std::uint64_t resolveCost(unsigned width, unsigned height, unsigned lights)
{ return std::uint64_t(width) * height * std::min(lights, kMaxLights) * kResolveGathers; }
inline unsigned plannedLights(unsigned width, unsigned height, unsigned requested)
{
    unsigned n = std::clamp(requested, 1u, kMaxLights);
    while (n > 0 && resolveCost(width, height, n) > kResolveCeiling) --n;
    return n;
}
// Raster passes per frame: six faces per resident light, never fewer, never demoted per frame.
inline unsigned facePasses(unsigned lights) { return kFaces * std::min(lights, kMaxLights); }

// ---------------------------------------------------------------------------------------------------- residency
// Pinned residency (user decision 10/22, U8; projectors included by decision 10/23): two session slots. A slot is FILLED only while empty, from the frame's
// ranked candidates (best first) with the best one not already resident; it is CLEARED only when its id is no longer an
// eligible light, or when it lies beyond the planned light count. No timing, cost or rank input once held, so a shot
// never swaps lights. Returns the number of slots that changed.
template<class ID> struct Residency
{
    std::array<ID, kMaxLights> id{};
    std::array<bool, kMaxLights> held{};
    unsigned count() const { unsigned n = 0; for (bool h : held) n += h ? 1u : 0u; return n; }
    bool contains(const ID& x) const { for (unsigned i = 0; i < kMaxLights; ++i) if (held[i] && id[i] == x) return true; return false; }
};
template<class ID, class Eligible, class Ranked>
inline unsigned updateResidency(Residency<ID>& r, unsigned lights, Eligible stillEligible, const Ranked* ranked, unsigned rankedCount)
{
    unsigned changed = 0;
    lights = std::min(lights, kMaxLights);
    for (unsigned i = 0; i < kMaxLights; ++i)
        if (r.held[i] && (i >= lights || !stillEligible(r.id[i]))) { r.held[i] = false; r.id[i] = ID{}; ++changed; }
    for (unsigned i = 0; i < lights; ++i)
    {
        if (r.held[i]) continue;
        for (unsigned c = 0; c < rankedCount; ++c)
            if (!r.contains(ranked[c])) { r.id[i] = ranked[c]; r.held[i] = true; ++changed; break; }
    }
    return changed;
}

// ---------------------------------------------------------------------------------------------------- readout
enum class State
{
    Off,           // the setting is off
    Choosing,      // on, no light chosen yet
    NoLight,       // on, nothing to choose (no eligible light, point or projector, in view)
    LowMemory,     // the atlas could not be allocated
    OverBudget,    // the resolve ledger rejected every light at this resolution
    On,            // `lights` lights resident and rendered
};
// Director readout under World shadows. Pure so the wording is testable.
inline std::string worldShadowText(State state, unsigned lights = 0)
{
    switch (state)
    {
    case State::Off: return "off";
    case State::Choosing: return "choosing a light";
    case State::NoLight: return "no light in view";
    case State::LowMemory: return "not enough video memory";
    case State::OverBudget: return "over the work budget at this size";
    case State::On:
    {
        const unsigned n = std::clamp(lights, 1u, kMaxLights);
        return "on " + std::to_string(n) + (n == 1 ? " light (" : " lights (") + std::to_string(facePasses(n)) +
            " faces at " + std::to_string(kUsable) + ")";
    }
    }
    return "off";
}
}
#endif
