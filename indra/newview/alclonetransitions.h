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

#include <memory>
#include <string>

class LLVOAvatar;

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

    // Steady-state overlay look of an ENTITY clone (Instance::mOverlayLook), -1 = none (draw the native
    // body). A running transition is reported by studioLayersFor() instead; ask that first.
    int32_t overlayLookFor(const LLUUID& instance_id) const;

    // --- engine-facing (alclonetransitions.cpp) ----------------------------------------------
    // Start one transition on a Studio instance. The kind is decided by the CALLER (the shared action
    // ALGhostCloneActions::transitionTo). Returns false with `why` set on refusal (max 2 running, one per
    // clone, bad duration, clone or body not available).
    bool start(const LLUUID& instance_id, const ALCloneTransition::Request& request, std::string& why);
    // The one finalize (idempotent). generation 0 = whichever record the clone has now.
    bool finalize(const LLUUID& instance_id, ALCloneTransition::Reason reason, ALCloneTransition::Outcome outcome,
                  U32 generation = 0);
    // Cancel button / menu: ROLLBACK. True when something was running.
    bool cancel(const LLUUID& instance_id);
    bool isRunning(const LLUUID& instance_id) const;
    // Status text for the page and menu ("Running 0.8 / 1.5 s", the last outcome, or empty).
    std::string statusFor(const LLUUID& instance_id) const;

    // Cancellation table hooks (PLAN 2.6). All are cheap no-ops unless a transition runs, and all are
    // idempotent. `Edit` rolls back the clone's own transition first, then the caller applies its edit (D18).
    static void cancelFor(const LLUUID& instance_id, ALCloneTransition::Reason reason);
    static void cancelAllRunning(ALCloneTransition::Reason reason);
    static void onSceneReplaced();                      // scene load / cast clear (call site: lldirectorcast.cpp, R-owned)
    static void onAllOff(bool clones_hidden);           // ALL OFF: ROLLBACK when it hides clones, else snap to the end (D9)
    static void onDisconnect();                         // logout / disconnect: DISCARD, restore
    static void shutdown();                             // before disconnectViewer(): DISCARD, restore

    // Per-frame (llappviewer.cpp): the clock tick (p snapshotted ONCE per frame, validity, teleport test, end)
    // and, after every pose writer including Stay in place, the body frame refresh.
    // M6 / T2: true when the entity body has system parts (body / eyes / hair) that native avatar rendering draws
    // and the Studio entity branch cannot (see ALCloneTransition::systemPartsDrawn for the rule).
    static bool systemPartsVisible(const LLVOAvatar* avatar);
    // A short note shown as the clone's last outcome (N3: overlay look dropped for a replacement body).
    static void setNote(const LLUUID& instance_id, const std::string& text);
    // True while a running OVERLAY_LOOK_TO_ENTITY transition needs the native body drawn (Studio body flags).
    static bool keepsBodyShown(const LLUUID& instance_id);
    static void tick();
    static void refreshBodyFrames();

    // One running transition as the UI sees it.
    struct Snapshot { bool running = false; U32 generation = 0; int32_t kind = 0; F32 elapsed = 0.f, duration = 0.f; };
    Snapshot snapshotFor(const LLUUID& instance_id) const;

private:
    ~ALCloneTransitions();
    struct Impl;
    std::unique_ptr<Impl> mImpl;
    static int32_t sRunning;
};
#endif
