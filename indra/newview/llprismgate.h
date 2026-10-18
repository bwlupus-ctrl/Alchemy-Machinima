/**
 * @file llprismgate.h
 * @brief Pure Vcam Gate sequence and persistence helpers.
 */

#ifndef LL_LLPRISMGATE_H
#define LL_LLPRISMGATE_H

#include "llprismlens.h"
#include <limits>

namespace LLPrismLens
{
// Pointer-free Gate protocol. All times are viewer monotonic seconds. A command
// receipt is not a decoded-frame acknowledgement; adapters must provide both.
struct GatePosition
{
    bool valid = false, restart = false, terminal = false;
    F64 seconds = 0.;
    std::string reason;
};
GatePosition gatePosition(EMediaOnCut mode, bool bookmarked, F64 position,
                          F64 off_time, F64 commit_time, F64 duration,
                          bool loop, F64 frame_period);
F64 gatePositionDistance(F64 a, F64 b, F64 duration, bool loop);
// Shared image-demand adapter, used before media admission and by Gate watches.
bool gateImageDemand(U32 frames, U32 watch_width, U32 watch_height,
                     U32& width, U32& height, F64& area);
inline bool gateCadenceDue(F32 hz, F64 now, F64 deadline, bool reserved)
{ return reserved || (hz > 0.f && now >= deadline); }
struct GateFrameBudget
{
    U32 cameraFrame = ~0u, warmFrame = ~0u;
    bool cameraAvailable(U32 frame) const { return cameraFrame != frame; }
    void cameraAttempt(U32 frame) { cameraFrame = frame; }
    bool warmCameraAttempt(U32 frame, bool eligible)
    {
        if (!eligible || warmFrame == frame) return false;
        warmFrame = frame;
        return true;
    }
    bool mediaDue(F32 hz, F64 now, F64 deadline, bool reserved) const
    { return gateCadenceDue(hz, now, deadline, reserved); }
};
inline F32 gateProducerDemand(const CaptureRateSettings& rate, F32 capacity)
{
    return rate.mMode == EOutputRateMode::TARGET_FPS ? rate.mTargetFps : capacity;
}
void gateCameraCadence(const std::array<bool, MAX_CAPTURES>& visible,
                       const std::array<bool, MAX_CAPTURES>& preview,
                       const std::array<F32, MAX_CAPTURES>& demand, F32 capacity,
                       S32 program, S32 incoming, bool fading,
                       std::array<F32, MAX_CAPTURES>& entitlement, S32 reservation = -1);
// Media warming has an additive copy lane, independent of camera admission and
// HEAD's ordinary visible allocation. Its rate is the candidate's max-min share
// alongside the visible producers, never the preview tier's leftover capacity.
void gateMediaCadence(const std::array<bool, MAX_CAPTURES>& visible,
                      const std::array<bool, MAX_CAPTURES>& preview,
                      const std::array<F32, MAX_CAPTURES>& demand, F32 capacity,
                      S32 warming, F32 warming_request,
                      std::array<F32, MAX_CAPTURES>& entitlement);
F64 gateLeadSeconds(U32 frames, F64 frame_dt, bool video);
bool prismFullTargetScissor(bool virtual_display, bool main_view,
                            U32 width, U32 height, S32* scissor);
bool gatePositionQualified(F64 drift, F64 uncertainty, F64 sample_age,
                           F64 frame_age, F64 frame_period);
// Per-player command-to-copied-image timings, in monotonic seconds.
struct GateLatency
{
    F64 seek = .2, resume = .2;
    static F64 sample(F64 estimate, F64 seconds);
};
struct GateFadeDemand
{
    U32 passes = 0;
    U64 pixels = 0;
    bool qualified = true;
    void add(U32 width, U32 height, bool cookie, bool variant = true);
    bool fits() const;
};
struct GateSourceEvent
{
    CaptureHandle handle;
    ESourceSlotKind kind = ESourceSlotKind::EMPTY;
    bool metadataReady = true, resident = false;
    bool available = false, permitted = false, admitted = false;
    bool ready = false, resumed = false, eof = false, opportunity = true;
    bool retired = false, paused = false, exited = false, seekable = false;
    bool loop = true;
    EMediaOnCut onCut = EMediaOnCut::LIVE;
    U64 token = 0, frame = 0;
    F64 position = 0., duration = 0., sampleAge = 1., error = 1., pauseTime = 0.;
    F64 frameAge = 1., framePeriod = 1. / 30.;
    std::string refusal;
};
enum class EGateAction : U8 { CLAIM, UNCLAIM, WARM, SEEK, PARK, RESUME, RETIRE, CLOSE, RELEASE, PROPOSE, ENVELOPE };
struct GateAction
{
    EGateAction kind;
    CaptureHandle handle;
    U64 token = 0;
    F64 position = 0., gain = 0., seconds = 0.;
    bool loop = false;
};
struct GateInput
{
    F64 now = 0., dt = 0., frameDt = 0.;
    GateSettings settings;
    std::vector<GateSourceEvent> sources;
    GateFadeDemand destinations;
    bool mediaEnabled = true, take = false, playVideos = false;
    bool activate = false, changed = false;
    // Matching proposal token, after all participating destination draws/mips.
    U64 accepted = 0;
};
inline bool gateInterceptsPlay(bool active, bool media_cycle, bool armed)
{ return active && media_cycle && armed; }

class GateReducer
{
public:
    std::vector<GateAction> update(const GateInput& input);
    // Preparation and publication both validate current source qualification.
    // ACK never replays the reducer or transport; stale proposals are consumed.
    bool validatePublication(U64 token, const GateInput& input);
    std::vector<GateAction> acknowledgeOutputs(U64 token, const GateInput& input);
    U64 proposal() const { return mProposal; }
    bool wantsImage(const GateInput& input) const;
    bool reservesImage(const GateInput& input) const;
    bool keepsParked(const CaptureHandle& handle) const;
    void reset();
    EGatePhase phase() const { return mPhase; }
    const CaptureHandle& program() const { return mProgram; }
    const CaptureHandle& incoming() const { return mCandidate; }
    U64 token() const { return mToken; }
    U64 revision() const { return mRevision; }
    F64 alpha() const { return mAlpha; }
    F64 targetPosition() const { return mTarget; }
    bool pending() const { return mPending; }
    const std::string& reason() const { return mReason; }
    std::string status(const CaptureHandle& handle) const;
    bool granted(const CaptureHandle& handle) const;
    bool charged(const CaptureHandle& handle) const;
    U32 charges() const;
    void grant(const CaptureHandle& handle);
    void revoke(const CaptureHandle& handle);
    // Restores only a valid stable Program. Never restores tokens or grants.
    void restoreProgram(const CaptureHandle& handle, F64 now);
private:
    struct Feed
    {
        CaptureHandle handle;
        bool grant = false, bookmark = false, charged = false, retiring = false, contributing = false;
        bool closeRequested = false, parked = false, released = false, owned = false;
        F64 position = 0., off = 0., retirement = 0.;
        GateLatency latency;
        U64 token = 0;
        std::string status;
    };
    bool freshImage(const GateSourceEvent& source) const;
    bool contributionQualified(const GateInput& input) const;
    Feed& feed(const CaptureHandle& handle);
    const GateSourceEvent* source(const GateInput&, const CaptureHandle&) const;
    void retire(const CaptureHandle&, F64 now, std::vector<GateAction>&);
    void skip(const GateInput&, const std::string&, std::vector<GateAction>&);
    std::vector<Feed> mFeeds;
    std::vector<LLUUID> mVisited;
    CaptureHandle mProgram, mCandidate, mEndProgram;
    EGatePhase mPhase = EGatePhase::INACTIVE;
    U64 mToken = 1, mRevision = 1, mProposal = 0, mCommand = 0, mParkFrame = 0, mWarmFrame = 0;
    F64 mDwellStart = 0., mBoundary = 0., mOverrun = -1., mStage = 0.;
    F64 mKnownEndTime = std::numeric_limits<F64>::infinity();
    F64 mEndBoundary = std::numeric_limits<F64>::infinity();
    F64 mDecodeElapsed = 0., mTarget = 0., mFadeStart = 0., mFadeSeconds = .5, mAlpha = 0.;
    F64 mTargetCommit = 0.;
    F64 mCommandStart = 0.;
    F64 mContributionTime = 0.;
    F64 mReservationEnd = 0.;
    bool mPending = false, mSeeking = false, mParked = false, mResuming = false;
    bool mResumeImage = false, mTargetNowBased = false;
    bool mCorrected = false, mRestart = false, mEffectiveFade = false, mEnabled = false;
    bool mHoldingBudget = false;
    bool mConverged = false, mResetPending = false, mFinishing = false, mMediaWasEnabled = true;
    EMediaOnCut mPreparedOnCut = EMediaOnCut::LIVE;
    bool mPreparedLoop = true;
    EGateVideoDwell mPreparedDwell = EGateVideoDwell::INTERVAL;
    U64 mConvergenceFrame = 0;
    std::string mReason;
};

S32 gateNextEnabledIndex(const std::vector<GateArmedCamera>& armed,
                         S32 current_index);
S32 gateClampEnabledIndex(const std::vector<GateArmedCamera>& armed,
                          S32 preferred_index);
S32 gateResolveArmIndex(const std::vector<GateArmedCamera>& armed,
                        S32 preferred_index,
                        const std::vector<LLUUID>& live_capture_ids);
void gateFilterResolvableArms(GateSettings& settings,
                              const std::vector<LLUUID>& live_capture_ids);
F64 gateNextCutTime(F64 past_boundary, F64 interval_seconds);
bool gateWarmActive(F64 now, F64 next_cut_time, U32 prewarm_frames,
                    F64 frame_dt);
bool gateWarmSeedValid(U32 width, U32 height);
bool gateConsumePreviewWarmFrame(U32& frames_remaining);
bool gateIsRealCut(S32 current_index, S32 candidate_index);
ERegistryResult gateFeedbackResult(const LLUUID& camera_source_object_id,
                                   const LLUUID& display_object_id,
                                   bool display_gate_subscribed);
bool gateDisplayCaptureReferenceAccepted(bool gate_source,
                                         bool capture_id_resolves);
void gateRouteDisplayBinding(S32 capture_slot, U64 capture_generation,
                             U32& display_slot, U64& display_generation);

// Malformed armed-camera entries are dropped while a valid outer Gate record
// remains loadable, matching the scene best-effort contract.
LLSD gateSettingsToLLSD(const GateSettings& settings);
bool gateSettingsFromLLSD(const LLSD& data, GateSettings& settings,
                          std::string* reason = nullptr);
bool gateSettingsFromScene(const LLSD& scene, GateSettings& settings,
                           bool& present, std::string* reason = nullptr);
const char* gateOnCutName(EMediaOnCut policy);
bool gateOnCutFromLLSD(const LLSD& slot, EMediaOnCut& policy);
bool gateOutputSourceFromLLSD(const LLSD& output, bool& subscribed);
} // namespace LLPrismLens

#endif // LL_LLPRISMGATE_H
