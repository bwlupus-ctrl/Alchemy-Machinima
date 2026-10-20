/** Pure reflection rules shared by clones, inventory playback and TUT. */
#ifndef AL_ALREFLECTMATH_H
#define AL_ALREFLECTMATH_H

#include "llmath.h"
#include "llquaternion.h"
#include "v3math.h"
#include "v3dmath.h"
#include <cmath>
#include <string>
#include <utility>

namespace ALReflect
{
inline std::string partner(std::string name)
{
    for (const auto& pair : {std::pair<const char*, const char*>("Left", "Right"),
                             {"Right", "Left"}})
    {
        const auto pos = name.find(pair.first);
        if (pos != std::string::npos)
            return name.replace(pos, std::string(pair.first).size(), pair.second);
    }
    for (const auto& pair : {std::pair<const char*, const char*>("L_", "R_"),
                             {"R_", "L_"}, {"LEFT_", "RIGHT_"}, {"RIGHT_", "LEFT_"}})
        if (name.rfind(pair.first, 0) == 0)
            return name.replace(0, std::string(pair.first).size(), pair.second);
    return name;
}
inline LLQuaternion rotation(const LLQuaternion& q)
{ return LLQuaternion(-q.mQ[0], q.mQ[1], -q.mQ[2], q.mQ[3]); }
inline LLVector3 vector(const LLVector3& v)
{ return LLVector3(v.mV[0], -v.mV[1], v.mV[2]); }
inline LLVector3 constraintOffset(const LLVector3& v, bool ground)
{ return ground ? v : vector(v); }
inline S32 facePartner(S32 id) { return id == 58 ? 59 : id == 59 ? 58 : id; }
inline F32 faceWeight(S32 id, F32 weight)
{ return id == 1206 || id == 1207 ? -weight : weight; }
// LLHandMotion's stable serialized values; no motion/controller dependency.
inline S32 handPose(S32 pose)
{ return pose >= 4 && pose <= 6 ? pose + 3 : pose >= 7 && pose <= 9 ? pose - 3 : pose; }
inline LLQuaternion rootRotation(const LLQuaternion& source, const LLVector3& normal)
{ return LLQuaternion(F_PI, LLVector3::y_axis) * source * LLQuaternion(F_PI, normal); }
inline LLVector3 planeVector(const LLVector3& v, const LLVector3& n)
{ return v - n * (2.f * (v * n)); }
inline LLVector3d planeFoot(const LLVector3d& foot, const LLVector3d& point, const LLVector3& n)
{
    const F64 d = (foot.mdV[0] - point.mdV[0]) * n.mV[0] +
                  (foot.mdV[1] - point.mdV[1]) * n.mV[1];
    return foot - LLVector3d(2. * d * n.mV[0], 2. * d * n.mV[1], 0.);
}
struct Plane
{
    bool valid = false;
    bool degenerate = false;
    LLVector3d point;
    LLVector3 normal = LLVector3::x_axis;
};
// Returns the capture tier: 0 bisector, 1 forward, 2 last plane, 3 world X,
// -1 invalid input, 4 hysteresis hold. Never manufactures a non-finite plane.
inline S32 updatePlane(Plane& p, const LLVector3d& source, const LLVector3d& clone,
                       const LLVector3& forward, bool live)
{
    if (!source.isFinite() || !clone.isFinite() || !forward.isFinite()) return -1;
    LLVector3 delta((F32)(clone.mdV[0] - source.mdV[0]),
                    (F32)(clone.mdV[1] - source.mdV[1]), 0.f);
    if (!delta.isFinite()) return -1;
    const F32 distance = delta.length();
    if (!std::isfinite(distance)) return -1;
    if (live && p.valid)
    {
        if (distance < .1f) p.degenerate = true;
        if (distance <= .3f) return 4;
    }
    S32 tier = 0;
    if (distance < .1f)
    {
        delta = LLVector3(forward.mV[0], forward.mV[1], 0.f);
        if (delta.length() >= .2f) tier = 1;
        else if (p.valid) return 2;
        else { delta = LLVector3::x_axis; tier = 3; }
    }
    delta.normalize();
    const LLVector3d midpoint = source * .5 + clone * .5;
    if (!midpoint.isFinite() || !delta.isFinite() || fabsf(delta.length() - 1.f) > .001f) return -1;
    p.point = midpoint;
    p.normal = delta;
    p.valid = true;
    p.degenerate = false;
    return tier;
}

enum class Route { LOCAL, INWORLD };
// The user's last intent owns every echo for an original id. Only STOP expires.
enum class Intent { NONE, PLAY, STOP };
enum class Event
{
    USER_PLAY, USER_STOP, NATURAL_END, REGION_START, REGION_STOP,
    BUILD_READY, BUILD_FAILED_SIGNALED, BUILD_FAILED_UNSIGNALED, IDLE
};
enum class Action { NONE, STOCK, ENSURE_DERIVED, CONSUME, STOP_LOCAL, DEFER_STOP, PLAY_ORIGINAL };
struct Decision { Intent intent; Action action; };
inline Decision decide(Intent intent, F64 age, Event event)
{
    if (intent == Intent::STOP && age >= 60.) intent = Intent::NONE;
    switch (event)
    {
    case Event::USER_PLAY: return {Intent::PLAY, Action::ENSURE_DERIVED};
    case Event::USER_STOP: return {Intent::STOP, Action::STOP_LOCAL};
    case Event::NATURAL_END:
        return intent == Intent::PLAY ? Decision{Intent::STOP, Action::DEFER_STOP} : Decision{intent, Action::NONE};
    case Event::REGION_START:
        return {intent, intent == Intent::PLAY ? Action::ENSURE_DERIVED :
                        intent == Intent::STOP ? Action::CONSUME : Action::STOCK};
    case Event::REGION_STOP:
        return {Intent::NONE, intent == Intent::NONE ? Action::STOCK : Action::STOP_LOCAL};
    case Event::BUILD_READY:
        return {intent, intent == Intent::PLAY ? Action::ENSURE_DERIVED : Action::NONE};
    case Event::BUILD_FAILED_SIGNALED:
        return {intent, intent == Intent::PLAY ? Action::PLAY_ORIGINAL : Action::NONE};
    case Event::BUILD_FAILED_UNSIGNALED: return {intent, Action::NONE};
    case Event::IDLE:
        return {intent, intent == Intent::PLAY ? Action::ENSURE_DERIVED : Action::NONE};
    }
    return {intent, Action::NONE};
}
struct Playback
{
    Intent intent = Intent::PLAY;
    F64 since = 0.;
    Route route = Route::LOCAL;
    // H3 changes intent immediately, but controller and wire work wait for idle.
    bool deferredStop = false;
    bool requested = false; // startMotion called after READY for this PLAY.
    F64 requestedAt = 0.;
    bool wasLoading = false;
    bool started = false; // Observed active and not stopped after that request.
    bool completed = false;
};
inline Decision readyDecision(Playback& play, bool active, bool loading, F64 now)
{
    if (play.intent != Intent::PLAY || play.completed) return {play.intent, Action::NONE};
    if (!play.requested) return {play.intent, Action::ENSURE_DERIVED};
    if (loading) { play.wasLoading = true; return {play.intent, Action::NONE}; }
    if (play.wasLoading) { play.requestedAt = now; play.wasLoading = false; }
    if (active) { play.started = true; return {play.intent, Action::NONE}; }
    // Start the fallback window only after leaving the controller's loading set.
    // Bound the wait for motions stopped before running playback is observed.
    if (!play.started && now - play.requestedAt < 2.) return {play.intent, Action::NONE};
    play.completed = true;
    return decide(play.intent, 0., Event::NATURAL_END);
}
inline Decision failedDecision(Intent intent, Route route, bool signaled)
{
    if (intent == Intent::PLAY && route == Route::LOCAL)
        return decide(intent, 0., Event::USER_STOP);
    return decide(intent, 0., signaled ? Event::BUILD_FAILED_SIGNALED : Event::BUILD_FAILED_UNSIGNALED);
}
}
#endif
