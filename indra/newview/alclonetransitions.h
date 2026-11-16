/** Session-only clone transitions manager (engine side). Render-facing queries are the CONTRACT
 *  between the engine and the renderers (lead, 11/16). See alclonetransition.h and
 *  .tmp/clonetrans/PLAN.md (rev 3) sections 2.2, 2.3, 2.6, 3.
 *
 *  Phase 1 kinds:
 *   - LOOK_TO_LOOK (b): an overlay clone; the Studio draws it twice this frame: look A as
 *     SIDE_OUTGOING and look B as SIDE_INCOMING, same body, same placement.
 *   - ENTITY_TO_OVERLAY_LOOK (a1): an entity clone; outgoing = its native lit body (native mask +
 *     effect shell), incoming = the Studio overlay of the SAME body with the target look (attached
 *     placement). On COMMIT the instance gets mOverlayLook = look and its body is render-only hidden.
 *   - OVERLAY_LOOK_TO_ENTITY (a1 reverse): outgoing = Studio overlay look, incoming = native body.
 *     On COMMIT mOverlayLook = -1 and the body is shown again.
 */
#ifndef AL_CLONETRANSITIONS_H
#define AL_CLONETRANSITIONS_H
#include "alclonetransition.h"
#include "lluuid.h"
#include "llsingleton.h"

class ALCloneTransitions : public LLSingleton<ALCloneTransitions>
{
    LLSINGLETON(ALCloneTransitions);
public:
    enum Kind : int32_t { KIND_LOOK_TO_LOOK = 0, KIND_ENTITY_TO_OVERLAY_LOOK = 1, KIND_OVERLAY_LOOK_TO_ENTITY = 2 };

    // --- render-facing (cheap; called per draw) -------------------------------------------
    // True when at least one transition is RUNNING. Renderers early-out on false.
    static bool anyRunning() { return sRunning > 0; }

    // Native body mask (entity runtime avatar id, or an attachment's owning avatar id).
    // Returns params with active == false when that body is not in a running transition.
    const ALCloneTransition::RenderParams& nativeParamsFor(const LLUUID& body_id) const;

    // Effect shell (additive edge in the world pass) for a native body; active == false => no shell.
    const ALCloneTransition::RenderParams& shellParamsFor(const LLUUID& body_id) const;

    // Studio overlay layers for a Ghost Studio instance id (ALGhostStudio::Instance::mId).
    // count 0 = draw normally; 1 or 2 = draw each layer with its look and params.
    struct StudioLayer { int32_t look = -1; ALCloneTransition::RenderParams params; };
    struct StudioLayers { int32_t count = 0; bool attachedPlacement = false; StudioLayer layer[2]; };
    void studioLayersFor(const LLUUID& instance_id, StudioLayers& out) const;

    // --- engine-facing (implemented in alclonetransitions.cpp by the engine work) -------------
    // ... start / cancel / finalize / clock tick / shutdown: defined by the engine work.

private:
    static int32_t sRunning;
};
#endif
