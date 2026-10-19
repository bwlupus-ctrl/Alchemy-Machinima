/** @file llprismgate.h @brief Simple Gate state, proposals and scene migration. */
#ifndef LL_LLPRISMGATE_H
#define LL_LLPRISMGATE_H
#include "llprismlens.h"
#include "llmediaframestate.h"
#include <map>
#include <set>

namespace LLPrismLens
{
struct GateCopyKey
{
    LLUUID id;
    U64 generation = 0, serial = 0;
    bool operator==(const GateCopyKey&) const = default;
};
struct GateState
{
    LLUUID program, pending, from, held, cursor, queued;
    F64 pendingStart = 0., fadeStart = 0., nextAuto = 0.;
    F64 lastTick = -1., unseenSince = -1., demandedSeconds = 0.;
    F64 fadeSeconds = .5, publishedAt = -1.;
    U64 baselinePublication = 0, baselineDecoder = 0, cutSerial = 0;
    LLMediaFrame::Identity baselineIdentity;
    bool awaitingCopy = false, fadeEndWaiting = false, emptyPublished = false;
    F32 alpha = 0.f;
    std::string text = "No sources ticked";
    GateCopyKey copyKey;
};
struct GateSourceInput
{
    LLUUID id;
    std::string name, reason;
    ESourceSlotKind kind = ESourceSlotKind::CAMERA;
    bool eligible = true, hasOutput = false, fullQuality = false;
    bool paused = false, frozen = false, downloading = false;
    bool desiredPlaying = false, playRefused = false, stopped = false;
    bool currentImage = false;
    U64 publication = 0, copiedDecoder = 0, decoder = 0;
    LLMediaFrame::Identity identity;
};
struct GateInput
{
    F64 now = 0.;
    GateSource settings;
    std::vector<GateSourceInput> sources;
    LLUUID take;
    bool demanded = false;
    bool hasGatePicture = false; // Both the retained texture and mHasOutput are valid.
};
struct GateProposal
{
    GateState next;
    LLUUID copy;
    bool needsPixels = false, rollback = false, publish = false;
    F64 demandedSeconds = 0.;
    std::vector<LLUUID> play, pause, feed;
};
GateProposal gateStep(const GateState& state, const GateInput& input);
void gateApply(GateState& state, const GateProposal& proposal, bool ok);
inline void gateOutputReleased(GateState& state)
{ state.copyKey = {}; state.emptyPublished = false; state.publishedAt = -1.; }
// Constant empty presentation never depends on retained texture allocation.
inline bool gateBlackComposite(bool empty, U32 slot, CompositeState& state)
{
    if (!empty) return false;
    state.mCaptureSlot = slot; state.mBlack = true; state.mGateAlpha = 0.f;
    state.mLetterbox = 1;
    for (U32 axis = 0; axis < 2; ++axis)
    {
        state.mDisplayToCaptureScale[axis] = 0.f;
        state.mDisplayToCaptureOffset[axis] = -2.f;
        state.mTextureRegionScale[axis] = 1.f;
        state.mTextureRegionOffset[axis] = 0.f;
    }
    std::fill(std::begin(state.mBarColorLinear), std::end(state.mBarColorLinear), 0.f);
    return true;
}
// Production two-pass seams: preparation keeps intent, composition owns commits.
GateProposal gatePrepareStep(GateState& state, const GateInput& input);
template<class SourceKey>
GateCookieKey gateCookieContentKey(const GateState& state, GateCookieKey key, SourceKey sourceKey)
{
    key.program = {}; key.from = {}; key.retained = {}; key.alpha = 0.f;
    key.black = state.held.isNull();
    if (key.black) { key.phase = 0; key.effectHz = 0.f; return key; }
    key.program = sourceKey(state.program);
    key.from = sourceKey(state.from);
    key.retained = {state.copyKey.id, state.copyKey.generation, state.copyKey.serial, true};
    key.alpha = state.alpha;
    return key;
}
template<class Publish>
bool gateComposeApply(GateState& state, const GateProposal& proposal, bool ok,
                      bool copied, const GateCopyKey& key, bool& rollback, Publish publish)
{
    gateApply(state, proposal, ok);
    if (copied) state.copyKey = key;
    rollback |= proposal.rollback;
    // Even a rejected end copy can present the complete incoming picture at
    // alpha one while the retained-texture swap waits for a retry.
    const bool present = ok || (state.fadeEndWaiting && state.alpha == 1.f);
    if (copied || (proposal.publish && present && state.publishedAt != proposal.next.lastTick) || rollback)
    {
        // Intent (including black/rollback state and audio) is already applied.
        // A failed picture publication remains pending until a later retry.
        if (!publish(rollback))
        {
            state.emptyPublished = false;
            rollback = true;
            return false;
        }
        state.publishedAt = proposal.next.lastTick;
        rollback = false;
    }
    return ok;
}
// Live ticks are the only implicit Play edge; scene restoration never calls this.
template<class Grant, class Mute, class Play>
void gateLiveTick(EGateOffAir policy, bool stopped, Grant grant, Mute mute, Play play)
{
    grant();
    if (policy == EGateOffAir::KEEP_PLAYING && stopped) { mute(); play(); }
}
F32 gateGain(const GateState& state, const LLUUID& id);
std::map<LLUUID, F32> gateAudioGains(const std::vector<std::pair<LLUUID, F32>>& videos,
                                    const std::set<LLUUID>& previous);
F64 gatePlayerDemand(const GateState& state, const LLUUID& id, F64 own);
GateStatus gateStatus(const GateState& state);
LLSD gateSourceToLLSD(const GateSource& source);
bool gateSourceFromLLSD(const LLSD& item, GateSource& source, std::string* reason = nullptr);
void gateResolveSources(GateSource& source, const LLUUID& self,
                        const std::vector<LLUUID>& eligible, std::vector<std::string>& notes);
void gateMigrateScene(LLSD& data, std::vector<SkippedBinding>& notes);
// Native registry assertions always run; only the optional media fixture needs a clip.
bool gateRegistryRegression(std::string& reason, std::string& media_status);
bool prismFullTargetScissor(bool virtual_display, bool main_view,
                            U32 width, U32 height, S32* scissor);
}
#endif
