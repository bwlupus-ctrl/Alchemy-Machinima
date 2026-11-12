/** Pure shaft admission decisions, shared by the renderer and native tests. */
#ifndef AL_SHAFT_DECISION_H
#define AL_SHAFT_DECISION_H
namespace ALShaftDecision
{
inline bool observe(bool visible, bool testing) { return visible || testing; }
// Timer owns destruction; cancellation only retires. Nested render/timer callbacks
// hold a lease until they leave, even if an event cancels or restarts the probe.
struct ProbeLifetime
{
    bool done = false;
    unsigned callbacks = 0;
    bool canDestroy() const { return done && callbacks == 0; }
    struct Lease
    {
        ProbeLifetime& state;
        explicit Lease(ProbeLifetime& s) : state(s) { ++state.callbacks; }
        ~Lease() { --state.callbacks; }
        Lease(const Lease&) = delete;
        Lease& operator=(const Lease&) = delete;
    };
};
template<class Light, class Color> inline void configureProbeLight(Light& light, const Color& white)
{
    light.setIsLight(true); // parameter block must exist before any setters
    light.setLightSRGBColor(white);
    light.setLightIntensity(1.f);
    light.setLightRadius(12.f);
    light.setLightFalloff(1.f);
}
enum Tier { ActorRig = 0, World = 1, FlaggedWorld = 2, Rig = 3 };
// Rendering resolves the first flagged ID; an auto-enabled child shadows a user root.
inline bool promoted(bool childFlag, bool childAuto, bool rootFlag, bool rootAuto, bool suppressed)
{
    return !suppressed && (childFlag ? !childAuto : rootFlag && !rootAuto);
}
inline int tier(bool rig, bool actorOnly, bool userShaft)
{
    return rig ? (actorOnly ? ActorRig : Rig) : (userShaft ? FlaggedWorld : World);
}
inline bool outranks(bool occupied, int candidateTier, bool rig, double priority,
                     int ownerTier, double ownerPriority)
{
    return !occupied ? (candidateTier == ActorRig || rig || priority > 0.0)
        : candidateTier != ownerTier ? candidateTier > ownerTier : priority > ownerPriority;
}
inline bool shaderReady(bool complete, int level, bool failed = false)
{
    return complete && level == 3 && !failed;
}
enum class Reason { NotProjector, NotFlagged, MasterOff, DeferredOff,
    ShadowsOff, ShaderUnavailable, ActorOnly, Suppressed, ZeroLight, ZeroShaft,
    Froxel, Drawing, Waiting, Occupied, Filtered };
struct Status
{
    bool projector = false, flagged = false, master = false, deferred = false;
    bool shadows = false, shader = false, actorOnly = false, suppressed = false;
    bool light = true, shaft = true, froxel = false, drawn = false, waiting = false, occupied = false;
};
inline Reason reason(const Status& s)
{
    if (!s.projector) return Reason::NotProjector;
    if (!s.flagged) return Reason::NotFlagged;
    if (!s.deferred) return Reason::DeferredOff;
    if (!s.shadows) return Reason::ShadowsOff;
    if (s.actorOnly) return Reason::ActorOnly;
    if (s.suppressed) return Reason::Suppressed;
    if (!s.light) return Reason::ZeroLight;
    if (s.froxel) return Reason::Froxel;
    if (!s.master) return Reason::MasterOff;
    if (!s.shader) return Reason::ShaderUnavailable;
    if (!s.shaft) return Reason::ZeroShaft;
    if (s.drawn) return Reason::Drawing;
    if (s.waiting) return Reason::Waiting;
    if (s.occupied) return Reason::Occupied;
    return Reason::Filtered;
}
inline const char* text(Reason r)
{
    switch (r)
    {
    case Reason::NotProjector: return "Not a projector";
    case Reason::NotFlagged: return "Not flagged - right-click the light > Volumetric Shaft";
    case Reason::MasterOff: return "Shafts are off";
    case Reason::DeferredOff: return "Deferred rendering is unavailable";
    case Reason::ShadowsOff: return "Projector shadows are off - enable Sun/Moon + Projectors in Graphics";
    case Reason::ShaderUnavailable: return "Shaft shader unavailable";
    case Reason::ActorOnly: return "Actor-only rig light - no shaft";
    case Reason::Suppressed: return "Cast Shadows is off for this light";
    case Reason::ZeroLight: return "Light brightness or range is too low";
    case Reason::ZeroShaft: return "Shaft brightness or haziness is zero";
    case Reason::Froxel: return "Lit through the froxel grid (soft shaft)";
    case Reason::Drawing: return "Drawing";
    case Reason::Waiting: return "Waiting for a shadow slot or shaft draw";
    case Reason::Occupied: return "No shadow slot - other lights have priority; raise Projector shadow slots";
    case Reason::Filtered: return "No shaft draw - light is off-screen, out of range or filtered";
    }
    return "Shaft status unavailable";
}
}
#endif
