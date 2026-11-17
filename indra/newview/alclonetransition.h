/** Session-only clone transitions: pure maths and render parameters. No viewer dependencies.
 *
 * CONTRACT (lead, 11/16): the render side (native mask, Studio overlay, effect shell) and the
 * engine side (state machine, UI, lifecycle) meet here. Do not change the meaning of an existing
 * field; add fields at the end and note it in the worktree REPORT.
 *
 * One transition = one field value f(x) in [0,1) per surface point, one progress p in [0,1].
 *   outgoing keeps a point  when f >= p   (it leaves as p rises)
 *   incoming keeps a point  when f <  p   (it arrives as p rises)
 * Same style, seed, p and coordinate on both halves => every point is kept by exactly one half
 * (for matched geometry; see PLAN.md 3.5). p == 0: only outgoing; p == 1: only incoming.
 * The GLSL field (render side) must match fieldValue() below to within 1e-5 (gate).
 */
#ifndef AL_CLONETRANSITION_H
#define AL_CLONETRANSITION_H
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace ALCloneTransition
{
enum Style : int32_t { STYLE_NONE = 0, STYLE_DISSOLVE = 1, STYLE_WIPE = 2, STYLE_CROSSFADE = 3 };
enum Side : int32_t { SIDE_OUTGOING = 0, SIDE_INCOMING = 1 };
enum Ease : int32_t { EASE_SMOOTH = 0, EASE_LINEAR = 1 };
enum Direction : int32_t { DIR_UP = 0, DIR_DOWN = 1 };

// Body-space transition coordinate (PLAN.md 3.1): final skinned agent position minus the body
// root, rotated into the body heading (+X forward, +Y left, +Z up), divided by the body scale.
// Metres for a scale-1 body. Feet near z = 0.
struct BodyFrame
{
    float rootAgent[3] = {0.f, 0.f, 0.f}; // presented root (agent space), recomputed each frame
    float heading = 0.f;                  // radians about +Z
    float scale = 1.f;
};

// Everything a renderer needs for one half of one transition this frame.
// active == false => renderers must behave byte-identically to "no transition".
struct RenderParams
{
    bool active = false;
    int32_t style = STYLE_NONE;
    int32_t side = SIDE_OUTGOING;
    int32_t direction = DIR_UP;
    float p = 0.f;               // eased progress, snapshotted ONCE per frame by the clock tick
    uint32_t seed = 0;           // same for both halves
    float edgeWidth = 0.06f;     // field units; additive edge band just on the kept side of the cut
    float edgeColor[3] = {1.0f, 0.55f, 0.15f};
    float edgeIntensity = 2.0f;  // linear radiance multiplier for the shell edge
    float bodyHeight = 1.8f;     // metres (scale 1); Wipe normalises z by this
    BodyFrame frame;
};

inline float ease(int32_t mode, float t)
{
    t = std::clamp(t, 0.f, 1.f);
    return mode == EASE_LINEAR ? t : t * t * (3.f - 2.f * t);
}

// Deterministic per-seed hash -> [0,1).
inline float hash3(int32_t x, int32_t y, int32_t z, uint32_t seed)
{
    uint32_t h = seed * 747796405u + 2891336453u;
    h ^= uint32_t(x) * 0x8da6b343u; h ^= uint32_t(y) * 0xd8163841u; h ^= uint32_t(z) * 0xcb1ab31fu;
    h = (h ^ (h >> 16)) * 0x7feb352du;
    h = (h ^ (h >> 15)) * 0x846ca68bu;
    h ^= h >> 16;
    return float(h >> 8) * (1.f / 16777216.f);
}

// Smooth value noise in body space (cell size in metres). Shared reference for the GLSL port.
inline float valueNoise(const float pos[3], float cell, uint32_t seed)
{
    const float x = pos[0] / cell, y = pos[1] / cell, z = pos[2] / cell;
    const float fx = std::floor(x), fy = std::floor(y), fz = std::floor(z);
    const int32_t ix = int32_t(fx), iy = int32_t(fy), iz = int32_t(fz);
    auto s = [](float t) { return t * t * (3.f - 2.f * t); };
    const float ux = s(x - fx), uy = s(y - fy), uz = s(z - fz);
    float c[2][2][2];
    for (int a = 0; a < 2; ++a) for (int b = 0; b < 2; ++b) for (int d = 0; d < 2; ++d)
        c[a][b][d] = hash3(ix + a, iy + b, iz + d, seed);
    auto lerp = [](float a, float b, float t) { return a + (b - a) * t; };
    const float x00 = lerp(c[0][0][0], c[1][0][0], ux), x10 = lerp(c[0][1][0], c[1][1][0], ux);
    const float x01 = lerp(c[0][0][1], c[1][0][1], ux), x11 = lerp(c[0][1][1], c[1][1][1], ux);
    return lerp(lerp(x00, x10, uy), lerp(x01, x11, uy), uz);
}

// Field value in [0,1) at a body-space position. The renderers port THIS function to GLSL.
inline float fieldValue(const RenderParams& r, const float pos[3])
{
    switch (r.style)
    {
    case STYLE_DISSOLVE:
    {
        // two octaves, renormalised so the histogram covers [0,1) well
        const float n = 0.65f * valueNoise(pos, 0.11f, r.seed) + 0.35f * valueNoise(pos, 0.037f, r.seed ^ 0x5bd1e995u);
        return std::clamp((n - 0.15f) / 0.70f, 0.f, 0.99999f);
    }
    case STYLE_WIPE:
    {
        const float h = std::max(r.bodyHeight, 0.1f);
        const float z = std::clamp(pos[2] / h, 0.f, 0.99999f);
        return r.direction == DIR_DOWN ? 0.99999f - z : z;
    }
    case STYLE_CROSSFADE:
    {
        // object-space dither: 1.5 cm cells (shadow-stable, unlike the screen Bayer)
        const float cell = 0.015f;
        return std::min(hash3(int32_t(std::floor(pos[0] / cell)), int32_t(std::floor(pos[1] / cell)),
                              int32_t(std::floor(pos[2] / cell)), r.seed), 0.99999f);
    }
    default:
        return 0.f;
    }
}

// Coverage test. Inactive params keep everything.
inline bool keeps(const RenderParams& r, float field)
{
    if (!r.active || r.style == STYLE_NONE) return true;
    return r.side == SIDE_OUTGOING ? field >= r.p : field < r.p;
}

// Additive shell edge weight in [0,1] for a kept point (0 = no edge). Crossfade has no edge.
inline float edgeWeight(const RenderParams& r, float field)
{
    if (!r.active || r.style == STYLE_CROSSFADE || r.style == STYLE_NONE || r.p <= 0.f || r.p >= 1.f) return 0.f;
    const float d = std::fabs(field - r.p);
    return d < r.edgeWidth ? 1.f - d / r.edgeWidth : 0.f;
}
}

// ===========================================================================================
// ENGINE CORE (workstream E, 11/16): the lifecycle state machine, pure and host-templated so the
// production code runs unchanged in the native fixture (tests/alclonetransition_test.cpp,
// scripts/code_tools/clone_transition_engine_check.py). No viewer types: ids are any ordered type.
// PLAN.md rev 3 2.2 / 2.6 / D18 / D20.
// ===========================================================================================
#include <vector>

namespace ALCloneTransition
{
// Transition kinds (the ALCloneTransitions::Kind values).
enum KindId : int32_t { KIND_LOOK_TO_LOOK = 0, KIND_ENTITY_TO_OVERLAY_LOOK = 1, KIND_OVERLAY_LOOK_TO_ENTITY = 2 };
// Phase 1 uses RUNNING -> DONE / ROLLED_BACK / DISCARDED; PREPARING and READY are trivial (a single body
// needs no preparation) but stay in the enum for the two-body kinds of Phase 2.
enum State : int32_t { ST_PREPARING = 0, ST_READY = 1, ST_RUNNING = 2, ST_FINALIZING = 3, ST_DONE = 4, ST_ROLLED_BACK = 5, ST_DISCARDED = 6 };
enum Outcome : int32_t { OUT_COMMIT = 0, OUT_ROLLBACK = 1, OUT_DISCARD = 2 };
// Every row of the cancellation table (PLAN 2.6) plus the natural end.
enum Reason : int32_t
{
    R_COMPLETE = 0, R_CANCEL, R_SWAP_SOURCE, R_FADE, R_DELETE, R_GROUP_DELETE, R_RECOVERY, R_SCENE_LOAD,
    R_SHOW_CLONES_OFF, R_SOLO, R_TELEPORT, R_DISCONNECT, R_SHUTDOWN, R_ALL_OFF, R_EDIT_LOCK, R_BODY_INVALID, R_COUNT
};
inline const char* reasonName(int32_t r)
{
    static const char* const names[R_COUNT] = {
        "complete", "cancel", "swap source", "fade", "delete", "group delete", "recovery", "scene load",
        "show clones off", "solo", "teleport", "disconnect", "shutdown", "ALL OFF", "edit lock", "body invalid"};
    return r >= 0 && r < R_COUNT ? names[r] : "unknown";
}
inline constexpr int MAX_RUNNING = 2; // D20

inline uint32_t seedFromBytes(const unsigned char* bytes, size_t count)
{
    uint32_t h = 2166136261u; // FNV-1a over the clone id bytes: the same clone always gets the same seed
    for (size_t i = 0; i < count; ++i) { h ^= bytes[i]; h *= 16777619u; }
    return h;
}

struct Request
{
    int32_t kind = KIND_LOOK_TO_LOOK;
    int32_t fromLook = -1, toLook = -1;
    int32_t style = STYLE_DISSOLVE, direction = DIR_UP, ease = EASE_SMOOTH;
    double duration = 1.5;
};
inline bool validStyle(int32_t s) { return s == STYLE_DISSOLVE || s == STYLE_WIPE || s == STYLE_CROSSFADE; }

template <class Id>
struct Record
{
    Id clone{};                // the Studio instance id (stable)
    uint32_t generation = 0;   // unique per started transition
    int32_t kind = KIND_LOOK_TO_LOOK;
    Id body{};                 // entity runtime id (null for overlay-only kinds)
    uint64_t bodyGeneration = 0;
    int32_t fromLook = -1, toLook = -1, style = STYLE_DISSOLVE, direction = DIR_UP, ease = EASE_SMOOTH;
    uint32_t seed = 0;
    double duration = 1.5, elapsed = 0.0;
    float p = 0.f;             // eased progress snapshot (the clock tick writes it once per frame)
    int32_t state = ST_RUNNING;
};

// Host interface (the manager supplies the real one, the fixture a double):
//   bool cloneValid(const Id&) / bool bodyValid(const Record<Id>&)
//   void onStarted(const Record<Id>&)
//   void onCommit(const Record<Id>&) / onRollback(...) / onDiscard(...)   -- exactly one, once per record
template <class Id, class Host>
class Machine
{
public:
    explicit Machine(Host& host) : mHost(host) {}

    // Returns the new generation (>0) or 0 with `why` set. One per clone; at most MAX_RUNNING at once.
    uint32_t start(const Id& clone, const Id& body, const Request& rq, uint32_t seed, const char*& why)
    {
        why = nullptr;
        if (rq.kind < KIND_LOOK_TO_LOOK || rq.kind > KIND_OVERLAY_LOOK_TO_ENTITY) { why = "Unknown transition kind"; return 0; }
        if (!validStyle(rq.style)) { why = "Unknown transition style"; return 0; }
        if (!(rq.duration >= 0.1 && rq.duration <= 30.0)) { why = "Duration must be 0.1 to 30 seconds"; return 0; }
        if (find(clone)) { why = "This clone already has a transition running; Cancel it first"; return 0; }
        if (int(mRecords.size()) >= MAX_RUNNING) { why = "Two transitions are already running; wait or Cancel one"; return 0; }
        if (!mHost.cloneValid(clone)) { why = "Clone is not available"; return 0; }
        Record<Id> r;
        r.clone = clone; r.body = body; r.generation = ++mNextGeneration; r.bodyGeneration = r.generation;
        r.kind = rq.kind; r.fromLook = rq.fromLook; r.toLook = rq.toLook; r.style = rq.style;
        r.direction = rq.direction; r.ease = rq.ease; r.seed = seed; r.duration = rq.duration;
        r.state = ST_RUNNING;
        mRecords.push_back(r);
        mHost.onStarted(mRecords.back());
        return r.generation;
    }

    // THE one finalize. Idempotent: an unknown clone, a stale generation (0 = any) or a record that is
    // not RUNNING is a no-op returning false. The record is removed BEFORE the host runs, so a host
    // callback that re-enters finalize for the same clone finds nothing (no double release).
    bool finalize(const Id& clone, uint32_t generation, int32_t reason, int32_t outcome)
    {
        auto it = std::find_if(mRecords.begin(), mRecords.end(), [&](const Record<Id>& r) { return r.clone == clone; });
        if (it == mRecords.end()) return false;
        if (generation != 0 && it->generation != generation) return false;
        if (it->state != ST_RUNNING) return false;
        Record<Id> r = *it;
        r.state = ST_FINALIZING;
        mRecords.erase(it);
        const bool cloneOk = mHost.cloneValid(r.clone), bodyOk = mHost.bodyValid(r);
        int32_t o = outcome;
        if (!cloneOk && !bodyOk) o = OUT_DISCARD;                            // both invalid: touch nothing else
        else if (o == OUT_COMMIT && !(cloneOk && bodyOk)) o = OUT_ROLLBACK;   // never commit onto an invalid side
        if (o == OUT_COMMIT) { r.state = ST_DONE; mHost.onCommit(r); }
        else if (o == OUT_ROLLBACK) { r.state = ST_ROLLED_BACK; mHost.onRollback(r); }
        else { r.state = ST_DISCARDED; mHost.onDiscard(r); }
        mLastOutcome = o; mLastReason = reason; mLastClone = clone;
        return true;
    }

    void finalizeAll(int32_t reason, int32_t outcome)
    {
        std::vector<Id> ids;
        for (const auto& r : mRecords) ids.push_back(r.clone);
        for (const Id& id : ids) finalize(id, 0, reason, outcome);
    }

    // One clock step (the host passes the presentation delta once per frame).
    void tick(double delta)
    {
        std::vector<Id> ids;
        for (const auto& r : mRecords) ids.push_back(r.clone);
        for (const Id& id : ids)
        {
            Record<Id>* r = find(id);
            if (!r || r->state != ST_RUNNING) continue;
            if (!mHost.cloneValid(r->clone) || !mHost.bodyValid(*r))
            {
                finalize(id, r->generation, R_BODY_INVALID, OUT_ROLLBACK);
                continue;
            }
            if (delta > 0.0) r->elapsed += delta;
            r->p = ease(r->ease, float(r->elapsed / r->duration));
            if (r->elapsed >= r->duration) finalize(id, r->generation, R_COMPLETE, OUT_COMMIT);
        }
    }

    Record<Id>* find(const Id& clone)
    {
        for (auto& r : mRecords) if (r.clone == clone) return &r;
        return nullptr;
    }
    const Record<Id>* find(const Id& clone) const
    {
        for (const auto& r : mRecords) if (r.clone == clone) return &r;
        return nullptr;
    }
    std::vector<Record<Id>>& records() { return mRecords; }
    const std::vector<Record<Id>>& records() const { return mRecords; }
    int running() const { return int(mRecords.size()); }
    int32_t lastOutcome() const { return mLastOutcome; }
    int32_t lastReason() const { return mLastReason; }
    const Id& lastClone() const { return mLastClone; }

private:
    Host& mHost;
    std::vector<Record<Id>> mRecords;
    uint32_t mNextGeneration = 0;
    int32_t mLastOutcome = -1, mLastReason = -1;
    Id mLastClone{};
};
}

#endif
