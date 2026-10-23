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
#include <atomic>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
namespace ALWorldShadow
{
// The viewer supplies its native attachment visibility update (apply(mode)). The scope applies `shadow_mode` on entry and, on EVERY exit (also
// an early return), `restore_mode`, which is CAPTURED BY VALUE AT CONSTRUCTION: a camera mode that changes while the cube passes run must not
// change what is restored. OFF frames and disabled scopes never apply anything.
template<class Mode, class Apply> struct AttachmentVisibilityScope
{
    bool active;
    Mode restore;
    Apply apply;
    AttachmentVisibilityScope(bool enabled, Mode restore_mode, Mode shadow_mode, Apply update) : active(enabled), restore(restore_mode), apply(update)
    { if (active) apply(shadow_mode); }
    ~AttachmentVisibilityScope() { if (active) apply(restore); }
    AttachmentVisibilityScope(const AttachmentVisibilityScope&) = delete;
    AttachmentVisibilityScope& operator=(const AttachmentVisibilityScope&) = delete;
};
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
// Per-light near plane (10/23 lead decision, round 5): the floor is the RenderAreaShadowWorldNear setting; the light sits at its object's centre, so a closed
// emitter (a bulb) encloses it and fills every face when double-sided or inward-wound. The six faces clip by AXIAL depth, so the region left out is the world-axis
// CUBE |p|inf < near (not a sphere): the near plane clears the emitter's REACH = the Chebyshev distance max_i max(hi_i - p_i, p_i - lo_i) from the light to the farthest
// extent of the emitter's world AABB, plus 1 cm. Clamped to 1 m and to 0.9 x the light radius. Separate shade prims further out still cast.
// The raster AND the resolve use this same value (the resolve linearises depth with it).
inline double chebyshevReach(const double lo[3], const double hi[3], const double p[3])
{
    double r = 0.0;
    for (int i = 0; i < 3; ++i) r = std::max(r, std::max(hi[i] - p[i], p[i] - lo[i]));
    return r;
}
// Agent-space AABB of a local-space AABB (round 6 P0). The extents of an ACTIVE drawable (moving or selected-in-edit object, attachment, linkset being edited) are kept in
// its spatial bridge's LOCAL frame; LLDrawable::getRenderMatrix() is that frame's local -> agent matrix (LLMatrix4: row vectors, v' = v * M, translation in row 3, `m` is
// [row][col]). The 8 corners go through it and the min/max is taken. Inactive drawables already hold agent-space extents and skip this.
template<class T> inline void transformedAabb(const T m[4][4], const double lo[3], const double hi[3], double outLo[3], double outHi[3])
{
    for (int i = 0; i < 3; ++i) { outLo[i] = std::numeric_limits<double>::infinity(); outHi[i] = -std::numeric_limits<double>::infinity(); }
    for (int c = 0; c < 8; ++c)
    {
        const double v[3] = {(c & 1) ? hi[0] : lo[0], (c & 2) ? hi[1] : lo[1], (c & 4) ? hi[2] : lo[2]};
        for (int j = 0; j < 3; ++j)
        {
            const double w = v[0]*static_cast<double>(m[0][j]) + v[1]*static_cast<double>(m[1][j]) + v[2]*static_cast<double>(m[2][j]) + static_cast<double>(m[3][j]);
            outLo[j] = std::min(outLo[j], w);
            outHi[j] = std::max(outHi[j], w);
        }
    }
}
// Two resident-light entries. A linkset revision invalidates the cached near;
// unchanged static linksets need no sibling walk, changed ones are walked in full.
template<class Key> struct NearCache
{
    struct Entry { Key key{}; std::uint64_t revision = 0; double radius = 0, floor = 0, value = 0; bool valid = false; };
    std::array<Entry,kMaxLights> entries{};
    unsigned next = 0;
    const double* find(const Key& key, std::uint64_t revision, double radius, double floor) const
    {
        for (const auto& e : entries)
            if (e.valid && e.key == key && e.revision == revision && e.radius == radius && e.floor == floor) return &e.value;
        return nullptr;
    }
    void store(const Key& key, std::uint64_t revision, double radius, double floor, double value)
    {
        Entry* slot = nullptr;
        for (auto& e : entries) if (e.valid && e.key == key) { slot = &e; break; }
        if (!slot) slot = &entries[next++ % kMaxLights];
        *slot = {key,revision,radius,floor,value,true};
    }
};
// Process-unique bounds-revision tags. Every LLViewerObject takes one at construction (so a freshly rezzed static linkset is cacheable at once and a recreated
// object with a recycled UUID never matches a stale entry) and a new one on every linkset / transform / extents event (stampBounds). Tag 0 means "no root".
// The counter is atomic and relaxed: the writers are viewer-thread events, but a tag must stay unique even if one is ever raised elsewhere.
inline std::uint64_t nextBoundsRevision()
{
    static std::atomic<std::uint64_t> serial{1};
    return serial.fetch_add(1, std::memory_order_relaxed) + 1;
}
// O(1) invalidation: one new tag on the object and on its edit root (null when the object is its own root). No recursion, no allocation, no lock.
inline void stampBounds(std::uint64_t& own, std::uint64_t* root)
{
    const std::uint64_t tag = nextBoundsRevision();
    if (root) *root = tag;
    own = tag;
}
// A sibling link of the light's prim counts for the near plane when its world AABB contains the light and its largest extent is under 1 m (a bulb or glass around a
// tiny/invisible light link); a big sibling (a wall, a floor) never does.
inline bool siblingEnclosesLight(const double lo[3], const double hi[3], const double p[3])
{
    double largest = 0.0;
    for (int i = 0; i < 3; ++i)
    {
        if (!(p[i] >= lo[i] && p[i] <= hi[i])) return false;
        largest = std::max(largest, hi[i] - lo[i]);
    }
    return largest < 1.0;
}
// The reach a sibling link adds: its Chebyshev reach when it encloses the light, else nothing.
inline double siblingReach(const double lo[3], const double hi[3], const double p[3])
{ return siblingEnclosesLight(lo, hi, p) ? chebyshevReach(lo, hi, p) : 0.0; }
// The reach the siblings of a linkset add, over EVERY link (a 255-link build is covered; there is no cap). visit(f) calls f(link) for the root and each child;
// box(link, lo, hi) fills the link's agent-space AABB and returns false when it has none (or is the light's own link).
template<class Visit, class Box> inline double linksetSiblingReach(Visit&& visit, Box&& box, const double p[3], unsigned* visited = nullptr)
{
    double reach = 0.0;
    unsigned count = 0;
    visit([&](auto&& link)
    {
        ++count;
        double lo[3], hi[3];
        if (box(link, lo, hi)) reach = std::max(reach, siblingReach(lo, hi, p));
    });
    if (visited) *visited = count;
    return reach;
}
// The memoised near of one light: `revision` is the edit root's current tag (0 = no root: never cached). Any other tag is a valid key, because every object
// is stamped at construction and re-stamped by every event that can change the answer. walk() is the full, uncapped sibling walk.
template<class Key, class Walk> inline double cachedNear(NearCache<Key>& cache, const Key& key, std::uint64_t revision, double radius, double floor, Walk&& walk)
{
    if (revision != 0)
        if (const double* hit = cache.find(key, revision, radius, floor)) return *hit;
    const double value = walk();
    if (revision != 0) cache.store(key, revision, radius, floor, value);
    return value;
}
inline double emitterNearPlane(double setting, double emitterReach, double lightRadius)
{
    double n = nearPlane(setting);
    if (std::isfinite(emitterReach) && emitterReach > 0.0) n = std::max(n, emitterReach + 0.01);
    n = std::min(n, 1.0);
    if (std::isfinite(lightRadius) && lightRadius > 0.0) n = std::min(n, 0.9 * lightRadius);
    return n;
}
// View -> agent rotation for the resolve: out = three vec3 ROWS of the inverse (= the three COLUMNS of the view matrix's 3x3), packed tight as
// out[3*column + row] = view[column][row]. `Mat` is indexed [column][row] (glm::mat4). Do NOT memcpy a glm::mat3: with the aligned gentypes the mat3
// columns are 16 bytes wide, so a 36 byte copy scrambles the rows (round 5 root cause: 51 % of floor pixels dark instead of 9 %).
template<class Mat> inline void packViewRotation(const Mat& view, float out[9])
{
    for (int column = 0; column < 3; ++column)
        for (int row = 0; row < 3; ++row)
            out[3*column + row] = static_cast<float>(view[column][row]);
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
    On,            // `lights` lights rendered a cube THIS frame (round 5: rendered, not merely pinned)
    Pinned,        // lights are pinned but none rendered a cube this frame (native map, out of view, not found...): see SlotReason
};
// Why a pinned slot did (not) render a cube this frame; the readout and the throttled log print it.
enum class SlotReason : unsigned char
{
    None,           // the slot holds no light
    Rendered,       // its six faces were rendered this frame
    NotFound,       // the pinned UUID is not among the lights this frame
    NotEligible,    // found, but not world-eligible now (attachment, actor-only, mirror copy, suppressed, no range)
    NativeMap,      // a projector that currently samples a native SL shadow map keeps that path
    OutOfView,      // outside the camera frustum: its six passes are skipped
    BadRange,       // far plane not beyond the near plane
};
inline const char* slotReasonText(SlotReason r)
{
    switch (r)
    {
    case SlotReason::None: return "empty";
    case SlotReason::Rendered: return "rendering";
    case SlotReason::NotFound: return "light not found";
    case SlotReason::NotEligible: return "not eligible";
    case SlotReason::NativeMap: return "native map";
    case SlotReason::OutOfView: return "out of view";
    case SlotReason::BadRange: return "range inside the near plane";
    }
    return "?";
}
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
    case State::Pinned: return "pinned but not rendering";
    case State::On:
    {
        const unsigned n = std::clamp(lights, 1u, kMaxLights);
        return "on " + std::to_string(n) + (n == 1 ? " light (" : " lights (") + std::to_string(facePasses(n)) +
            " faces at " + std::to_string(kUsable) + ")";
    }
    }
    return "off";
}
// Round 5 readout: counts the lights that RENDERED a cube this frame (`rendered`), names the pinned lights (labels[k] for slot k, empty slots have reason None)
// and says why a pinned light is not rendering. Pure so the wording is testable.
inline std::string worldShadowReport(State state, unsigned rendered, const std::string* labels, const SlotReason* reasons, unsigned slots)
{
    if (state != State::On && state != State::Pinned) return worldShadowText(state, rendered);
    std::string out;
    if (rendered > 0)
    {
        out = worldShadowText(State::On, rendered) + ":";
        for (unsigned k = 0; k < slots; ++k)
            if (reasons[k] == SlotReason::Rendered) out += " " + labels[k] + ";";
        out.back() = '.';
    }
    for (unsigned k = 0; k < slots; ++k)
    {
        if (reasons[k] == SlotReason::None || reasons[k] == SlotReason::Rendered) continue;
        out += (out.empty() ? "" : " ") + std::string("light ") + std::to_string(k + 1) + "/" + std::to_string(slots) + " pinned but not rendering: " + labels[k] +
            " (" + slotReasonText(reasons[k]) + ").";
    }
    if (out.empty()) out = worldShadowText(State::Pinned);
    return out;
}
}
#endif
