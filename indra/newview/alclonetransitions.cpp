/**
 * @file alclonetransitions.cpp
 * @brief Session-only clone transitions manager (engine side): the record + state machine host, the
 *        per-frame clock, the render-facing queries and the cancellation table hooks.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 *
 * The lifecycle itself is ALCloneTransition::Machine (alclonetransition.h), the same code the native
 * fixture runs. Nothing here is ever written to a scene: every record, look and flag is session only.
 */
#include "llviewerprecompiledheaders.h"
#include "alclonetransitions.h"

#include "alghoststudio.h"
#include "llagent.h"
#include "llghostavatar.h"
#include "llpresentationtime.h"
#include "llviewerregion.h"
#include "llavatarappearancedefines.h"
#include "llwearabletype.h"

#include <cmath>
#include <map>

int32_t ALCloneTransitions::sRunning = 0;

namespace
{
const ALCloneTransition::RenderParams& inactive_params()
{
    static const ALCloneTransition::RenderParams none;
    return none;
}

// M6 (system-avatar entity clones). The Studio ENTITY branch draws only harvested attachment batches and static
// faces; it has NO system body / eyes / hair draw (the Studio overlay looks 0-100 are not compiled into the
// system-avatar programs, and the overlay of any other clone has never drawn them either). An entity whose
// system parts are VISIBLE therefore cannot become an overlay look: its native body would be hidden at commit
// and those parts would vanish. Same tests the native system passes use (LLVOAvatar::renderSkinned /
// renderTransparent / renderRigid): a baked texture that is IMG_INVISIBLE (a fully alpha-masked part) does not
// count. Animesh (control avatars) have no system body.
bool system_parts_visible(const LLVOAvatar* avatar)
{
    using namespace LLAvatarAppearanceDefines;
    if (!avatar || avatar->isControlAvatar()) return false;
    return avatar->isTextureVisible(TEX_HEAD_BAKED) || avatar->isTextureVisible(TEX_UPPER_BAKED)
        || avatar->isTextureVisible(TEX_LOWER_BAKED) || avatar->isTextureVisible(TEX_EYES_BAKED)
        || avatar->isTextureVisible(TEX_HAIR_BAKED)
        || (avatar->isWearingWearableType(LLWearableType::WT_SKIRT) && avatar->isTextureVisible(TEX_SKIRT_BAKED));
}
const char* const SYSTEM_PARTS_REFUSAL =
    "Overlay looks cannot draw this clone's standard avatar body or eyes yet (an alpha-masked mesh body works)";
}

struct ALCloneTransitions::Impl
{
    typedef ALCloneTransition::Record<LLUUID> Rec;

    struct Extra
    {
        ALCloneTransition::RenderParams params; // p, style, seed, direction, frame: snapshotted once per frame
        LLUUID region;                          // region at start (teleport test, as updateFades)
    };

    // The Machine host: validity, and the three outcomes. Every callback runs at most once per record.
    struct Host
    {
        Impl* mImpl = nullptr;

        bool cloneValid(const LLUUID& clone) const
        {
            const ALGhostStudio::Instance* inst = ALGhostStudio::instance().getInstance(clone);
            return inst && !inst->mDeleting && inst->mState != ALGhostStudio::STATE_TEARING_DOWN;
        }
        bool bodyValid(const Rec& r) const
        {
            if (r.kind == ALCloneTransition::KIND_LOOK_TO_LOOK) return true; // one overlay body, nothing to resolve
            const ALGhostStudio::Instance* inst = ALGhostStudio::instance().getInstance(r.clone);
            if (!inst || inst->mEntityId != r.body) return false;
            LLGhostAvatar* ghost = ALGhostStudio::instance().resolveEntityClone(r.clone);
            if (!ghost || ghost->isDead() || ghost->getID() != r.body) return false;
            // M6: re-checked every tick and at commit; a COMMIT onto an invalid half becomes a ROLLBACK, so a body
            // whose system parts became visible can never be hidden behind an overlay look that cannot draw them.
            if (r.kind == ALCloneTransition::KIND_ENTITY_TO_OVERLAY_LOOK && system_parts_visible(ghost)) return false;
            return true;
        }
        void onStarted(const Rec& r)
        {
            // Publish the running state FIRST: the record is already in the machine, and keepsBodyShown() (read by
            // reapplyEntityBodyFlags below) is gated on sRunning. Without this the first reverse transition left the
            // native body hidden until finalize.
            sRunning = mImpl->mMachine.running();
            // The reverse kind needs the native body drawn (it is the incoming half): lift the overlay hide.
            if (r.kind == ALCloneTransition::KIND_OVERLAY_LOOK_TO_ENTITY)
                ALGhostStudio::instance().reapplyEntityBodyFlags(r.clone);
        }
        void onCommit(const Rec& r)
        {
            ALGhostStudio& studio = ALGhostStudio::instance();
            mImpl->setStatus(r.clone, "Done");
            mImpl->release(r.clone);
            if (r.kind == ALCloneTransition::KIND_LOOK_TO_LOOK)
            {
                if (ALGhostStudio::Instance* inst = studio.getInstance(r.clone))
                {
                    inst->mStyle = r.toLook;
                    studio.touchInstances(); // Style column
                }
            }
            else if (r.kind == ALCloneTransition::KIND_ENTITY_TO_OVERLAY_LOOK)
                studio.setEntityOverlayLook(r.clone, r.toLook);
            else
                studio.setEntityOverlayLook(r.clone, -1);
        }
        void onRollback(const Rec& r)
        {
            mImpl->setStatus(r.clone, std::string("Cancelled (") + ALCloneTransition::reasonName(mImpl->mReason) + ")");
            mImpl->release(r.clone);
            ALGhostStudio::instance().reapplyEntityBodyFlags(r.clone); // the reverse kind lifted the overlay hide
        }
        void onDiscard(const Rec& r)
        {
            mImpl->setStatus(r.clone, std::string());
            mImpl->release(r.clone);
            ALGhostStudio::instance().reapplyEntityBodyFlags(r.clone); // null-safe: restore whatever still exists
        }
    };

    Host mHost;
    ALCloneTransition::Machine<LLUUID, Host> mMachine;
    std::map<LLUUID, Extra> mExtras;
    std::map<LLUUID, std::string> mStatus; // last outcome per clone (session only; capped)
    ALCloneTransition::Reason mReason = ALCloneTransition::R_CANCEL;
    U64 mClockGeneration = ~0ull;

    Impl() : mMachine(mHost) { mHost.mImpl = this; }

    void release(const LLUUID& clone)
    {
        mExtras.erase(clone);
        sRunning = mMachine.running();
    }
    void setStatus(const LLUUID& clone, const std::string& text)
    {
        if (text.empty()) { mStatus.erase(clone); return; }
        if (mStatus.size() >= 32 && !mStatus.count(clone)) mStatus.erase(mStatus.begin());
        mStatus[clone] = text;
    }

    static float headingOf(const LLQuaternion& rotation)
    {
        const LLVector3 forward = LLVector3::x_axis * rotation;
        return std::atan2(forward.mV[1], forward.mV[0]);
    }

    // The presented body root (agent space), heading and scale this frame (PLAN 3.1).
    void fillFrame(const Rec& r, ALCloneTransition::RenderParams& params) const
    {
        ALGhostStudio& studio = ALGhostStudio::instance();
        const ALGhostStudio::Instance* inst = studio.getInstance(r.clone);
        if (!inst) return;
        LLVector3 foot = gAgent.getPosAgentFromGlobal(inst->mFootGlobal);
        LLQuaternion rotation = inst->mRotation;
        F32 scale = inst->mScale;
        const LLVOAvatar* body_avatar = nullptr; // the body whose shape height drives Wipe (S2)
        if (r.kind != ALCloneTransition::KIND_LOOK_TO_LOOK || inst->mKind == ALGhostStudio::BACKING_ENTITY_CLONE)
        {
            if (LLGhostAvatar* ghost = studio.resolveEntityClone(r.clone))
            {
                foot += ghost->getStayInPlaceRenderedOffset(); // the DRAWN body, Stay in place included
                rotation = ghost->getRotation();
                scale = ghost->getUniformScale();
                body_avatar = ghost;
            }
        }
        else
        {
            body_avatar = ALGhostStudio::resolveOverlayAvatar(*inst); // the overlay's source body
        }
        // S2: per-record body height in BODY units (the field divides by scale): the avatar's own shape height,
        // 1.8 m when it is unavailable or implausible.
        float height = 1.8f;
        if (body_avatar)
        {
            const float h = body_avatar->mBodySize.mV[VZ];
            if (std::isfinite(h) && h >= 0.3f && h <= 8.f) height = h;
        }
        params.bodyHeight = height;
        ALCloneTransition::BodyFrame& frame = params.frame;
        for (int i = 0; i < 3; ++i) frame.rootAgent[i] = foot.mV[i];
        frame.heading = headingOf(rotation);
        frame.scale = scale > 0.f ? scale : 1.f;
    }

    void refreshFrames()
    {
        for (const Rec& r : mMachine.records())
        {
            auto it = mExtras.find(r.clone);
            if (it != mExtras.end()) fillFrame(r, it->second.params);
        }
    }

    void tick()
    {
        const LLTemporalFrameContext& frame = LLPresentationTime::currentFrame();
        if (frame.generation == mClockGeneration) return; // one tick per presentation frame
        mClockGeneration = frame.generation;
        // Region change or teleport (the updateFades test): Phase 1 single-body kinds roll back.
        const LLViewerRegion* region = gAgent.getRegion();
        bool interrupted = !region || gAgent.getTeleportState() != LLAgent::TELEPORT_NONE;
        if (!interrupted)
            for (const auto& entry : mExtras)
                interrupted = interrupted || entry.second.region != region->getRegionID();
        if (interrupted)
        {
            mReason = ALCloneTransition::R_TELEPORT;
            mMachine.finalizeAll(ALCloneTransition::R_TELEPORT, ALCloneTransition::OUT_ROLLBACK);
            sRunning = mMachine.running();
            return;
        }
        // Presentation time: world time scale aware, and stopped by Freeze (same rule as the Studio motion
        // clock); the 0.25 s cap keeps a stalled frame from skipping the whole effect.
        const F64 raw = LLPresentationTime::drives(LLTemporalFeature::ANIMATION) ? frame.presentation_delta : frame.wall_delta;
        const F64 delta = (std::isfinite(raw) && raw > 0.0) ? std::min(raw, 0.25) : 0.0;
        mReason = ALCloneTransition::R_BODY_INVALID; // overwritten by the machine for a natural end
        mMachine.tick(delta);
        sRunning = mMachine.running();
        // Snapshot p ONCE per frame into the params every renderer reads this frame.
        for (const Rec& r : mMachine.records())
        {
            auto it = mExtras.find(r.clone);
            if (it != mExtras.end()) it->second.params.p = r.p;
        }
    }
};

ALCloneTransitions::ALCloneTransitions() : mImpl(new Impl) {}
ALCloneTransitions::~ALCloneTransitions() = default;

// --- render-facing --------------------------------------------------------------------------
const ALCloneTransition::RenderParams& ALCloneTransitions::nativeParamsFor(const LLUUID& body_id) const
{
    if (sRunning <= 0 || body_id.isNull()) return inactive_params();
    for (const Impl::Rec& r : mImpl->mMachine.records())
    {
        if (r.kind == ALCloneTransition::KIND_LOOK_TO_LOOK || r.body != body_id) continue;
        auto it = mImpl->mExtras.find(r.clone);
        if (it == mImpl->mExtras.end()) continue;
        // Native half: the side was fixed at start() (outgoing when the lit body leaves for an overlay look, else
        // incoming). This getter never mutates the params.
        return it->second.params;
    }
    return inactive_params();
}

const ALCloneTransition::RenderParams& ALCloneTransitions::shellParamsFor(const LLUUID& body_id) const
{
    const ALCloneTransition::RenderParams& native = nativeParamsFor(body_id);
    // Crossfade has no edge (edgeWeight() is 0): no shell for it.
    return native.active && native.style != ALCloneTransition::STYLE_CROSSFADE ? native : inactive_params();
}

void ALCloneTransitions::studioLayersFor(const LLUUID& instance_id, StudioLayers& out) const
{
    out = StudioLayers();
    if (sRunning <= 0) return;
    const Impl::Rec* rec = mImpl->mMachine.find(instance_id);
    if (!rec) return;
    auto it = mImpl->mExtras.find(instance_id);
    if (it == mImpl->mExtras.end()) return;
    ALCloneTransition::RenderParams params = it->second.params;
    switch (rec->kind)
    {
    case ALCloneTransition::KIND_LOOK_TO_LOOK:
        out.count = 2;
        out.attachedPlacement = false;
        out.layer[0].look = rec->fromLook;
        out.layer[0].params = params; out.layer[0].params.side = ALCloneTransition::SIDE_OUTGOING;
        out.layer[1].look = rec->toLook;
        out.layer[1].params = params; out.layer[1].params.side = ALCloneTransition::SIDE_INCOMING;
        break;
    case ALCloneTransition::KIND_ENTITY_TO_OVERLAY_LOOK:
        out.count = 1;
        out.attachedPlacement = true;
        out.layer[0].look = rec->toLook;
        out.layer[0].params = params; out.layer[0].params.side = ALCloneTransition::SIDE_INCOMING;
        break;
    case ALCloneTransition::KIND_OVERLAY_LOOK_TO_ENTITY:
        out.count = 1;
        out.attachedPlacement = true;
        out.layer[0].look = rec->fromLook;
        out.layer[0].params = params; out.layer[0].params.side = ALCloneTransition::SIDE_OUTGOING;
        break;
    default:
        out = StudioLayers();
        break;
    }
}

int32_t ALCloneTransitions::overlayLookFor(const LLUUID& instance_id) const
{
    const ALGhostStudio::Instance* inst = ALGhostStudio::instance().getInstance(instance_id);
    return inst && inst->mKind == ALGhostStudio::BACKING_ENTITY_CLONE ? int32_t(inst->mOverlayLook) : -1;
}

// --- engine-facing ----------------------------------------------------------------------------
bool ALCloneTransitions::start(const LLUUID& instance_id, const ALCloneTransition::Request& request, std::string& why)
{
    ALGhostStudio& studio = ALGhostStudio::instance();
    const ALGhostStudio::Instance* inst = studio.getInstance(instance_id);
    if (!inst || inst->mDeleting) { why = "Clone is not available"; return false; }
    const bool entity = inst->mKind == ALGhostStudio::BACKING_ENTITY_CLONE;
    LLUUID body;
    switch (request.kind)
    {
    case ALCloneTransition::KIND_LOOK_TO_LOOK:
        if (entity) { why = "Choose Normal look first, then use Overlay look on an entity clone"; return false; }
        if (!ALGhostStudio::isOverlayStyle(request.fromLook) || !ALGhostStudio::isOverlayStyle(request.toLook))
        { why = "Pick an overlay look"; return false; }
        if (request.fromLook == request.toLook) { why = "The clone already has that look"; return false; }
        break;
    case ALCloneTransition::KIND_ENTITY_TO_OVERLAY_LOOK:
        if (!entity) { why = "Overlay look transitions apply to entity clones"; return false; }
        if (inst->mOverlayLook >= 0) { why = "This clone already shows an overlay look; go to Normal look first"; return false; }
        if (!ALGhostStudio::isOverlayStyle(request.toLook)) { why = "Pick an overlay look"; return false; }
        break;
    case ALCloneTransition::KIND_OVERLAY_LOOK_TO_ENTITY:
        if (!entity) { why = "Normal look transitions apply to entity clones"; return false; }
        if (inst->mOverlayLook < 0) { why = "This clone already has its normal look"; return false; }
        break;
    default:
        why = "Unknown transition kind";
        return false;
    }
    if (entity)
    {
        LLGhostAvatar* ghost = studio.resolveEntityClone(instance_id);
        if (!ghost || ghost->isDead() || !ghost->isBuilt()) { why = "The clone's body is not ready"; return false; }
        body = ghost->getID();
        if (inst->mEntityId != body) { why = "The clone's body is not ready"; return false; }
        if (request.kind == ALCloneTransition::KIND_ENTITY_TO_OVERLAY_LOOK && system_parts_visible(ghost))
        { why = SYSTEM_PARTS_REFUSAL; return false; }
    }
    // A refused request never modifies state: reject a duplicate BEFORE anything touches mExtras (the running
    // record's render params live there; the refusal branch below erases the entry).
    if (mImpl->mMachine.find(instance_id)) { why = "This clone already has a transition running; Cancel it first"; return false; }
    if (!studio.isVisible(*inst)) { why = "The clone is hidden; show it first"; return false; }
    if (ALCloneTransitions::anyRunning() && mImpl->mMachine.running() >= ALCloneTransition::MAX_RUNNING &&
        !mImpl->mMachine.find(instance_id))
    { why = "Two transitions are already running; wait or Cancel one"; return false; }
    LLViewerRegion* region = gAgent.getRegion();
    if (!region || gAgent.getTeleportState() != LLAgent::TELEPORT_NONE) { why = "Not available during a teleport"; return false; }

    ALCloneTransition::Request rq = request;
    if (rq.kind == ALCloneTransition::KIND_OVERLAY_LOOK_TO_ENTITY) rq.fromLook = inst->mOverlayLook;
    if (rq.kind == ALCloneTransition::KIND_ENTITY_TO_OVERLAY_LOOK) rq.fromLook = -1;
    const char* reason = nullptr;
    const uint32_t seed = ALCloneTransition::seedFromBytes(instance_id.mData, UUID_BYTES);
    Impl::Extra extra;
    extra.region = region->getRegionID();
    extra.params.active = true;
    extra.params.style = rq.style;
    extra.params.side = rq.kind == ALCloneTransition::KIND_ENTITY_TO_OVERLAY_LOOK
        ? ALCloneTransition::SIDE_OUTGOING : ALCloneTransition::SIDE_INCOMING; // the native half (see nativeParamsFor)
    extra.params.direction = rq.direction;
    extra.params.seed = seed;
    extra.params.p = 0.f;
    mImpl->mExtras[instance_id] = extra; // visible to onStarted(); removed again on refusal
    const uint32_t generation = mImpl->mMachine.start(instance_id, body, rq, seed, reason);
    if (!generation)
    {
        mImpl->mExtras.erase(instance_id);
        sRunning = mImpl->mMachine.running();
        why = reason ? reason : "Transition refused";
        return false;
    }
    sRunning = mImpl->mMachine.running();
    mImpl->setStatus(instance_id, std::string());
    if (const Impl::Rec* rec = mImpl->mMachine.find(instance_id)) mImpl->fillFrame(*rec, mImpl->mExtras[instance_id].params);
    why.clear();
    return true;
}

bool ALCloneTransitions::finalize(const LLUUID& instance_id, ALCloneTransition::Reason reason,
                                  ALCloneTransition::Outcome outcome, U32 generation)
{
    mImpl->mReason = reason;
    const bool done = mImpl->mMachine.finalize(instance_id, generation, reason, outcome);
    sRunning = mImpl->mMachine.running();
    return done;
}

bool ALCloneTransitions::cancel(const LLUUID& instance_id)
{
    return finalize(instance_id, ALCloneTransition::R_CANCEL, ALCloneTransition::OUT_ROLLBACK);
}

bool ALCloneTransitions::isRunning(const LLUUID& instance_id) const
{
    return mImpl->mMachine.find(instance_id) != nullptr;
}

ALCloneTransitions::Snapshot ALCloneTransitions::snapshotFor(const LLUUID& instance_id) const
{
    Snapshot snap;
    if (const Impl::Rec* rec = mImpl->mMachine.find(instance_id))
    {
        snap.running = true;
        snap.generation = rec->generation;
        snap.kind = rec->kind;
        snap.elapsed = F32(rec->elapsed);
        snap.duration = F32(rec->duration);
    }
    return snap;
}

std::string ALCloneTransitions::statusFor(const LLUUID& instance_id) const
{
    if (const Impl::Rec* rec = mImpl->mMachine.find(instance_id))
        return llformat("Running %.1f / %.1f s", rec->elapsed, rec->duration);
    const auto it = mImpl->mStatus.find(instance_id);
    return it == mImpl->mStatus.end() ? std::string() : it->second;
}

// --- cancellation table hooks (all idempotent, all free when nothing runs) ---------------------------
void ALCloneTransitions::cancelFor(const LLUUID& instance_id, ALCloneTransition::Reason reason)
{
    if (sRunning <= 0 || !instanceExists()) return;
    instance().finalize(instance_id, reason, ALCloneTransition::OUT_ROLLBACK);
}

void ALCloneTransitions::cancelAllRunning(ALCloneTransition::Reason reason)
{
    if (sRunning <= 0 || !instanceExists()) return;
    ALCloneTransitions& self = instance();
    self.mImpl->mReason = reason;
    self.mImpl->mMachine.finalizeAll(reason, ALCloneTransition::OUT_ROLLBACK);
    sRunning = self.mImpl->mMachine.running();
}

void ALCloneTransitions::onSceneReplaced()
{
    cancelAllRunning(ALCloneTransition::R_SCENE_LOAD);
}

void ALCloneTransitions::onAllOff(bool clones_hidden)
{
    if (sRunning <= 0 || !instanceExists()) return;
    ALCloneTransitions& self = instance();
    self.mImpl->mReason = ALCloneTransition::R_ALL_OFF;
    // D9: snap to the end, unless ALL OFF hides the clones (then put everything back).
    self.mImpl->mMachine.finalizeAll(ALCloneTransition::R_ALL_OFF,
        clones_hidden ? ALCloneTransition::OUT_ROLLBACK : ALCloneTransition::OUT_COMMIT);
    sRunning = self.mImpl->mMachine.running();
}

void ALCloneTransitions::onDisconnect()
{
    if (sRunning <= 0 || !instanceExists()) return;
    ALCloneTransitions& self = instance();
    self.mImpl->mReason = ALCloneTransition::R_DISCONNECT;
    self.mImpl->mMachine.finalizeAll(ALCloneTransition::R_DISCONNECT, ALCloneTransition::OUT_DISCARD);
    sRunning = self.mImpl->mMachine.running();
}

void ALCloneTransitions::shutdown()
{
    if (!instanceExists()) return;
    ALCloneTransitions& self = instance();
    self.mImpl->mReason = ALCloneTransition::R_SHUTDOWN;
    self.mImpl->mMachine.finalizeAll(ALCloneTransition::R_SHUTDOWN, ALCloneTransition::OUT_DISCARD);
    self.mImpl->mExtras.clear();
    self.mImpl->mStatus.clear();
    sRunning = 0;
}

bool ALCloneTransitions::keepsBodyShown(const LLUUID& instance_id)
{
    if (sRunning <= 0 || !instanceExists()) return false;
    const Impl::Rec* rec = instance().mImpl->mMachine.find(instance_id);
    return rec && rec->kind == ALCloneTransition::KIND_OVERLAY_LOOK_TO_ENTITY;
}

void ALCloneTransitions::tick()
{
    if (sRunning <= 0 || !instanceExists()) return;
    instance().mImpl->tick();
}

void ALCloneTransitions::refreshBodyFrames()
{
    if (sRunning <= 0 || !instanceExists()) return;
    instance().mImpl->refreshFrames();
}
