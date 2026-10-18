/**
 * @file llprismgate.cpp
 * @brief Pure Vcam Gate router and persistence helpers.
 */

#include "linden_common.h"

#include "llprismgate.h"
#include "llprismpreview.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <limits>

namespace
{
constexpr U32 MAX_GATE_ARMS = LLPrismLens::MAX_CAPTURES;
constexpr U32 MAX_GATE_PREWARM_FRAMES = 5;
constexpr F64 MIN_GATE_INTERVAL_SECONDS = 0.5;
constexpr F64 MAX_GATE_INTERVAL_SECONDS = 120.0;

bool isNumeric(const LLSD& value)
{
    return value.isInteger() || value.isReal();
}

bool containsCapture(const std::vector<LLUUID>& captures, const LLUUID& id)
{
    return id.notNull() &&
        std::find(captures.begin(), captures.end(), id) != captures.end();
}
} // anonymous namespace

namespace LLPrismLens
{
const char* gateOnCutName(EMediaOnCut policy)
{
    switch (policy)
    {
        case EMediaOnCut::RESTART: return "restart";
        case EMediaOnCut::RESUME: return "resume";
        case EMediaOnCut::LIVE: return "live";
    }
    return "";
}
bool gateOnCutFromLLSD(const LLSD& slot, EMediaOnCut& policy)
{
    policy = EMediaOnCut::LIVE;
    if (!slot.has("source_on_cut")) return true;
    if (!slot["source_on_cut"].isString()) return false;
    const auto value = slot["source_on_cut"].asString();
    if (value == "restart") policy = EMediaOnCut::RESTART;
    else if (value == "resume") policy = EMediaOnCut::RESUME;
    else if (value != "live") return false;
    return true;
}
bool gateOutputSourceFromLLSD(const LLSD& output, bool& subscribed)
{
    subscribed = false;
    if (!output.has("gate_source")) return true;
    if (!output["gate_source"].isBoolean()) return false;
    subscribed = output["gate_source"].asBoolean();
    return true;
}

namespace
{
bool sameFeed(const CaptureHandle& a, const CaptureHandle& b)
{ return a.mId == b.mId && a.mGeneration == b.mGeneration; }
bool privateFeed(const GateSourceEvent* s)
{ return s && s->kind == ESourceSlotKind::MEDIA_URL; }
}

GatePosition gatePosition(EMediaOnCut mode, bool bookmarked, F64 position,
                          F64 off_time, F64 commit_time, F64 duration,
                          bool loop, F64 frame_period)
{
    GatePosition out;
    if (!std::isfinite(position) || !std::isfinite(off_time) ||
        !std::isfinite(commit_time) || !std::isfinite(duration) ||
        !std::isfinite(frame_period) || position < 0.) return out;
    out.valid = true;
    if (mode == EMediaOnCut::LIVE && duration <= 0.)
    { out.restart = true; out.reason = "Live needs a known length - restarting"; return out; }
    if (!bookmarked || mode == EMediaOnCut::RESTART) return out;
    if (mode == EMediaOnCut::RESUME) { out.seconds = position; return out; }
    if (duration <= 0.)
    {
        out.restart = true;
        out.reason = "Live needs a known length - restarting";
        return out;
    }
    const F64 q = position + std::max(0., commit_time - off_time);
    out.seconds = loop ? std::fmod(q, duration) :
        std::min(q, std::max(0., duration - (frame_period > 0. ? frame_period : .1)));
    out.terminal = !loop && q >= duration;
    return out;
}

F64 gatePositionDistance(F64 a, F64 b, F64 duration, bool loop)
{
    if (!std::isfinite(a) || !std::isfinite(b) || !std::isfinite(duration))
        return std::numeric_limits<F64>::infinity();
    F64 distance = std::abs(a - b);
    if (loop && duration > 0.)
    { distance = std::fmod(distance, duration); distance = std::min(distance, duration - distance); }
    return distance;
}

bool gateImageDemand(U32 frames, U32 watch_width, U32 watch_height,
                     U32& width, U32& height, F64& area)
{
    if (!frames || !watch_width || !watch_height) return false;
    width = std::max(width, watch_width); height = std::max(height, watch_height);
    area = std::max(area, F64(width) * height);
    return true;
}
// HEAD's visible-first water fill is the normal allocation in every Gate mode.
// Reservations are separate extra attempts; they never change these shares.
void gateCameraCadence(const std::array<bool, MAX_CAPTURES>& visible,
                       const std::array<bool, MAX_CAPTURES>& preview,
                       const std::array<F32, MAX_CAPTURES>& demand, F32 capacity,
                       S32, S32, bool,
                       std::array<F32, MAX_CAPTURES>& entitlement, S32)
{
    entitlement.fill(0.f);
    const F32 remaining = LLPrismPreviewPolicy::waterFillTier(visible, demand, capacity, entitlement);
    LLPrismPreviewPolicy::waterFillTier(preview, demand, remaining, entitlement);
}

void gateMediaCadence(const std::array<bool, MAX_CAPTURES>& visible,
                      const std::array<bool, MAX_CAPTURES>& preview,
                      const std::array<F32, MAX_CAPTURES>& demand, F32 capacity,
                      S32 warming, F32 warming_request,
                      std::array<F32, MAX_CAPTURES>& entitlement)
{
    gateCameraCadence(visible, preview, demand, capacity, -1, -1, false, entitlement);
    if (warming < 0 || warming >= static_cast<S32>(MAX_CAPTURES) || !preview[warming]) return;
    auto fair = visible;
    fair[warming] = true;
    auto fair_demand = demand;
    fair_demand[warming] = warming_request;
    std::array<F32, MAX_CAPTURES> shares{};
    LLPrismPreviewPolicy::waterFillTier(fair, fair_demand, capacity, shares);
    entitlement[warming] = std::max(entitlement[warming], shares[warming]);
}

F64 gateLeadSeconds(U32 frames, F64 frame_dt, bool video)
{
    return std::max(video ? .3 : 0., std::max(1u, frames) * llclamp(frame_dt, 1. / 240., .1));
}

bool prismFullTargetScissor(bool virtual_display, bool main_view,
                            U32 width, U32 height, S32* scissor)
{
    if (!virtual_display && main_view) return false;
    scissor[0] = scissor[1] = 0;
    scissor[2] = static_cast<S32>(width);
    scissor[3] = static_cast<S32>(height);
    return true;
}

bool gatePositionQualified(F64 drift, F64 uncertainty, F64 sample_age,
                           F64 frame_age, F64 frame_period)
{
    return std::isfinite(drift) && std::isfinite(uncertainty) &&
        std::isfinite(sample_age) && std::isfinite(frame_age) &&
        drift >= 0. && uncertainty >= 0. && uncertainty < .15 &&
        drift + uncertainty <= .150001 && sample_age >= 0. && sample_age <= .5 &&
        frame_age >= 0. && frame_age <= std::max(.15, frame_period);
}

F64 GateLatency::sample(F64 estimate, F64 seconds)
{
    if (!std::isfinite(seconds) || seconds < 0.) return estimate;
    // React promptly to a fast player after the conservative startup seed;
    // otherwise an early RESUME can consume the first-contribution allowance.
    return llclamp(.25 * estimate + .75 * llclamp(seconds, .05, .6), .05, .6);
}

void GateFadeDemand::add(U32 width, U32 height, bool cookie, bool variant)
{
    ++passes;
    pixels += static_cast<U64>(width) * height;
    qualified = qualified && variant && width && height &&
        width <= (cookie ? 1024u : 1920u) && height <= (cookie ? 1024u : 1080u);
}
bool GateFadeDemand::fits() const
{ return qualified && passes > 0 && passes <= 8 && pixels <= 8ULL * 1024 * 1024; }

GateReducer::Feed& GateReducer::feed(const CaptureHandle& h)
{
    for (auto& f : mFeeds) if (sameFeed(f.handle, h)) return f;
    Feed f; f.handle = h; mFeeds.push_back(f); return mFeeds.back();
}
const GateSourceEvent* GateReducer::source(const GateInput& in, const CaptureHandle& h) const
{
    const GateSourceEvent* result = nullptr;
    for (const auto& s : in.sources) if (sameFeed(s.handle, h))
    { if (s.retired || s.exited) return &s; if (!result) result = &s; }
    return result;
}
bool GateReducer::granted(const CaptureHandle& h) const
{ for (const auto& f : mFeeds) if (sameFeed(f.handle, h)) return f.grant; return false; }
bool GateReducer::charged(const CaptureHandle& h) const
{ for (const auto& f : mFeeds) if (sameFeed(f.handle, h)) return f.charged; return false; }
U32 GateReducer::charges() const
{ U32 n = 0; for (const auto& f : mFeeds) if (f.charged) ++n; return n; }
void GateReducer::grant(const CaptureHandle& h) { feed(h).grant = true; ++mRevision; }
void GateReducer::revoke(const CaptureHandle& h)
{
    auto& f = feed(h);
    f.grant = false; f.bookmark = false; f.contributing = false;
    // Public Disarm revokes before removing the arm. Ownership must survive
    // that call until update emits UNCLAIM after the retirement receipt.
    ++mRevision;
}
std::string GateReducer::status(const CaptureHandle& h) const
{
    if (mEnabled && sameFeed(h, mProgram)) return "On air";
    for (const auto& f : mFeeds) if (sameFeed(f.handle, h)) return f.status;
    return {};
}
void GateReducer::reset()
{
    // Runtime reset invalidates requests, but retirement charges survive it.
    ++mToken; ++mRevision; mProgram = {}; mCandidate = {};
    mProposal = 0; mFinishing = false; mPhase = EGatePhase::INACTIVE; mEnabled = false;
    mPending = false; mVisited.clear(); mOverrun = -1.; mAlpha = 0.; mResetPending = true;
    for (auto& f : mFeeds) { f.grant = false; f.bookmark = false; f.contributing = false; f.owned = false; }
}
void GateReducer::restoreProgram(const CaptureHandle& h, F64 now)
{
    reset(); mProgram = h; mDwellStart = now;
}
void GateReducer::retire(const CaptureHandle& h, F64 now, std::vector<GateAction>& out)
{
    if (h.mId.isNull()) return;
    auto& f = feed(h);
    if (!f.charged || f.retiring) return;
    f.retiring = true; f.closeRequested = false; f.retirement = now; f.token = ++mToken;
    out.push_back({EGateAction::RETIRE, h, f.token});
}
void GateReducer::skip(const GateInput& in, const std::string& reason, std::vector<GateAction>& out)
{
    if (mCandidate.mId.notNull())
    {
        feed(mCandidate).status = "Skipped: " + reason;
        mVisited.push_back(mCandidate.mId);
        retire(mCandidate, in.now, out);
    }
    if (mPhase == EGatePhase::CROSSFADING && privateFeed(source(in, mProgram)) && granted(mProgram))
        out.push_back({EGateAction::ENVELOPE, mProgram, ++mToken, 0., 1., 0.});
    mAlpha = 0.; mFinishing = false; mHoldingBudget = false;
    mCandidate = {}; mProposal = 0; mReason = reason; ++mRevision;
    mPhase = EGatePhase::ON_AIR;
}

bool GateReducer::keepsParked(const CaptureHandle& h) const
{
    for (const auto& f : mFeeds) if (sameFeed(f.handle, h)) return f.parked && !f.released;
    return false;
}
bool GateReducer::wantsImage(const GateInput& in) const
{
    const auto* s = source(in, mCandidate);
    if (!s || !in.settings.mActive) return false;
    if (mPhase == EGatePhase::CROSSFADING || mProgram.mId.isNull() || mPending) return true;
    // A video must complete both the parked image and a post-RESUME image.
    // Keep ordinary fair copy demand throughout warming, including READY.
    if (isMediaKind(s->kind)) return true;
    const bool needs_image = !freshImage(*s) || (privateFeed(s) && mResuming && s->frame <= mParkFrame);
    if (in.settings.mMode == EGateMode::MANUAL) return needs_image;
    return needs_image && (in.now >= mBoundary || gateWarmActive(in.now, mBoundary,
        std::max(1u, in.settings.mPrewarmFrames), llclamp(in.frameDt > 0. ? in.frameDt : in.dt, 1. / 240., .1)));
}
bool GateReducer::freshImage(const GateSourceEvent& s) const
{ return s.ready && s.frame > mWarmFrame; }
bool GateReducer::reservesImage(const GateInput& in) const
{
    const auto* s = source(in, mCandidate);
    if (!s || !wantsImage(in) || mPhase == EGatePhase::CROSSFADING) return false;
    // Fixed window: pre-roll plus the existing two-second overrun. Only an
    // admitted transport already seeking/resuming may use the overrun lane.
    const F64 end = mPending || mProgram.mId.isNull() ? mReservationEnd :
        in.settings.mMode == EGateMode::AUTO_CYCLE ? mBoundary : 0.;
    const F64 lead = gateLeadSeconds(in.settings.mPrewarmFrames,
        in.frameDt > 0. ? in.frameDt : in.dt, isMediaKind(s->kind));
    const bool transport = privateFeed(s) && s->admitted && s->metadataReady && mSeeking && !mResumeImage;
    if (end <= 0. || in.now < end - lead - 1e-9 ||
        in.now > end + (transport ? 2. : 0.) + 1e-9) return false;
    if (transport) return !mParked || (mResuming && !mResumeImage && s->frame <= mParkFrame);
    return !s->ready || s->frame <= (privateFeed(s) && mSeeking ? mParkFrame : mWarmFrame);
}
bool GateReducer::contributionQualified(const GateInput& in) const
{
    const auto* s = source(in, mCandidate);
    if (!s || !s->available || !s->permitted || !s->refusal.empty() || !freshImage(*s)) return false;
    if (!privateFeed(s)) return true;
    if (!granted(mCandidate) || !s->admitted || !s->metadataReady ||
        s->onCut != mPreparedOnCut || s->loop != mPreparedLoop ||
        !mResuming || s->token != mCommand || s->frame <= mParkFrame ||
        (!s->resumed && !s->eof)) return false;
    F64 target = mTarget;
    // Keep the predicted seek target bound, and independently require Live to
    // match its clock at the actual contribution/publication check time.
    if (s->onCut == EMediaOnCut::LIVE && !mRestart)
        for (const auto& f : mFeeds) if (sameFeed(f.handle, mCandidate))
        {
            const auto position = gatePosition(s->onCut, f.bookmark, f.position,
                f.off, mTargetCommit, s->duration, s->loop, s->framePeriod);
            if (!position.valid) return false;
            target = position.seconds;
            const auto live = gatePosition(s->onCut, f.bookmark, f.position,
                f.off, in.now, s->duration, s->loop, s->framePeriod);
            if (!live.valid || !gatePositionQualified(
                gatePositionDistance(s->position, live.seconds, s->duration, s->loop),
                s->error, s->sampleAge, s->frameAge, s->framePeriod)) return false;
        }
    // A now-based prediction must also land within the Live tolerance. Freeze
    // the first copied image's instant; later publication checks cannot move it.
    const F64 prediction_error = s->onCut == EMediaOnCut::LIVE && !mRestart && mTargetNowBased
        ? std::abs(mContributionTime - mTargetCommit) : 0.;
    return gatePositionQualified(gatePositionDistance(s->position, target, s->duration, s->loop) + prediction_error,
        s->error, s->sampleAge, s->frameAge, s->framePeriod);
}
bool GateReducer::validatePublication(U64 token, const GateInput& in)
{
    if (!token || token != mProposal) return false;
    if (!in.settings.mActive || (!in.mediaEnabled && privateFeed(source(in, mCandidate))) ||
        (mPhase != EGatePhase::CROSSFADING && !contributionQualified(in)))
    { mProposal = 0; ++mRevision; return false; }
    return true;
}
std::vector<GateAction> GateReducer::acknowledgeOutputs(U64 token, const GateInput& in)
{
    if (!validatePublication(token, in)) return {};
    // Publication only commits its prepared decision. It must never replay
    // receipts, claims, commands, or resident flags from the proposal snapshot.
    std::vector<GateAction> out;
    const auto* incoming = source(in, mCandidate);
    const auto* program = source(in, mProgram);
    mProposal = 0;
    if (mPhase == EGatePhase::CROSSFADING && mFinishing)
    {
        if (mProgram.mId.isNull() && privateFeed(incoming)) out.push_back({EGateAction::ENVELOPE, mCandidate, ++mToken, 0., 1., 0.});
        mFinishing = false;
        retire(mProgram, in.now, out); mProgram = mCandidate; mCandidate = {};
        mDwellStart = mFadeStart; mPhase = EGatePhase::ON_AIR;
        mOverrun = -1.; mPending = false; mVisited.clear(); mReason.clear();
    }
    else
    {
        const F64 duration = mEffectiveFade && mProgram.mId.notNull() ? in.settings.mCrossfadeSeconds : 0.;
        if (privateFeed(incoming)) out.push_back({EGateAction::ENVELOPE, mCandidate, ++mToken, 0., 1., duration});
        if (privateFeed(program)) out.push_back({EGateAction::ENVELOPE, mProgram, ++mToken, 0., 0., duration});
        feed(mCandidate).contributing = true;
        if (duration > 0.)
        { mPhase = EGatePhase::CROSSFADING; mFadeStart = in.now; mFadeSeconds = duration; mAlpha = 0.; }
        else
        {
            retire(mProgram, in.now, out); mProgram = mCandidate; mCandidate = {};
            mPhase = EGatePhase::ON_AIR; mDwellStart = in.now; mAlpha = 0.;
            mVisited.clear(); mOverrun = -1.; mPending = false;
            mReason = in.mediaEnabled && in.settings.mTransition == EGateTransition::CROSSFADE &&
                !in.destinations.fits() && in.settings.mFadeOverBudget == EGateFadeOverBudget::CUT_INSTEAD
                ? "Crossfade over budget - cut" : "";
        }
    }
    ++mRevision;
    return out;
}

std::vector<GateAction> GateReducer::update(const GateInput& in)
{
    std::vector<GateAction> out;
    if (!std::isfinite(in.now) || !std::isfinite(in.dt)) return out;
    const auto& settings = in.settings;
    if (mResetPending)
    {
        for (const auto& f : mFeeds) if (f.charged && !f.retiring) retire(f.handle, in.now, out);
        mResetPending = false;
    }
    // Receipt processing is independent of logical cancellation or feature state.
    for (auto& f : mFeeds)
    {
        const auto* s = source(in, f.handle);
        if (f.retiring && s && (s->exited || s->retired || (s->paused && s->token == f.token)))
        {
            f.charged = false; f.retiring = false; f.parked = true;
            if (f.contributing && !s->retired && !s->exited)
            {
                f.position = std::max(0., s->position); f.off = s->pauseTime > 0. && s->pauseTime <= in.now ? s->pauseTime : in.now;
                f.bookmark = true;
            }
            if (!f.contributing) f.off = in.now;
            f.contributing = false; ++mRevision;
        }
        else if (f.retiring && !f.closeRequested && in.now - f.retirement >= 2.)
        {
            f.closeRequested = true;
            out.push_back({EGateAction::CLOSE, f.handle, f.token});
            mReason = "Waiting for player retirement"; ++mRevision;
        }
        if (f.parked && !f.released && in.now - f.off >= 60.)
        {
            f.released = true; out.push_back({EGateAction::RELEASE, f.handle, f.token});
            f.status = "Idle video released - bookmark kept"; ++mRevision;
        }
    }
    for (auto& f : mFeeds)
    {
        const bool armed = std::any_of(settings.mArmed.begin(), settings.mArmed.end(),
            [&](const auto& arm) { return arm.mCaptureId == f.handle.mId; });
        if (f.owned && (!settings.mActive || !in.mediaEnabled || !armed))
        {
            retire(f.handle, in.now, out);
            if (!f.charged && !f.retiring)
            {
                f.owned = f.parked = f.released = f.grant = false;
                out.push_back({EGateAction::UNCLAIM, f.handle, ++mToken});
            }
        }
    }
    if (mProposal) validatePublication(mProposal, in);
    if (in.playVideos && in.mediaEnabled && !settings.mActive)
    {
        for (const auto& arm : settings.mArmed)
            for (const auto& s : in.sources)
                if (arm.mCaptureId == s.handle.mId && privateFeed(&s)) grant(s.handle);
        mReason = "Videos authorized - activate Gate to begin";
    }
    if (!settings.mActive)
    {
        if (mEnabled)
        {
            retire(mProgram, in.now, out); retire(mCandidate, in.now, out);
            for (auto& f : mFeeds) { f.grant = false; }
            mCandidate = {}; mProposal = 0; mPending = false; mAlpha = 0.;
            mPhase = EGatePhase::INACTIVE; mEnabled = false; ++mToken; ++mRevision;
        }
        return out;
    }
    // Claim every armed private owner before selecting a successor. Existing
    // standalone playback is muted and retired through the same ACK barrier.
    if (in.mediaEnabled)
        for (const auto& arm : settings.mArmed)
            for (const auto& s : in.sources) if (s.handle.mId == arm.mCaptureId && privateFeed(&s))
            {
                auto& f = feed(s.handle);
                if (!f.owned)
                {
                    f.owned = true;
                    out.push_back({EGateAction::CLAIM, s.handle, ++mToken});
                    if (s.resident && !sameFeed(s.handle, mProgram))
                    { f.charged = true; retire(s.handle, in.now, out); }
                }
            }
    if (in.activate || in.playVideos)
    {
        for (const auto& arm : settings.mArmed)
            for (const auto& s : in.sources)
                if (arm.mCaptureId == s.handle.mId && privateFeed(&s)) grant(s.handle);
        mVisited.clear();
    }
    if (!in.mediaEnabled && mMediaWasEnabled)
    {
        retire(mCandidate, in.now, out); mCandidate = {}; mProposal = 0;
        mPending = mFinishing = false; mAlpha = 0.; mPhase = EGatePhase::ON_AIR;
        ++mToken; ++mRevision;
    }
    mMediaWasEnabled = in.mediaEnabled;
    if (!in.mediaEnabled)
    {
        for (auto& f : mFeeds) { f.grant = false; }
        if (privateFeed(source(in, mProgram))) { retire(mProgram, in.now, out); mProgram = {}; }
        if (privateFeed(source(in, mCandidate))) { retire(mCandidate, in.now, out); mCandidate = {}; mProposal = 0; }
    }
    if (!mEnabled) { if (privateFeed(source(in, mProgram))) mProgram = {}; mEnabled = true; mPhase = EGatePhase::ON_AIR; mDwellStart = in.now; ++mRevision; }
    if (in.changed || in.playVideos || in.activate) { mVisited.clear(); mOverrun = -1.; }
    if (in.take && !mPending && settings.mMode == EGateMode::MANUAL && mCandidate.mId.notNull() && privateFeed(source(in, mCandidate)))
    {
        mSeeking = mParked = mResuming = mCorrected = mConverged = false;
        mStage = in.now; mDecodeElapsed = 0.; mPhase = EGatePhase::WARMING; mProposal = 0;
    }
    if (in.take && !mPending)
    {
        const auto* target = source(in, mCandidate);
        if (!target && settings.mPreviewArmIndex >= 0 && settings.mPreviewArmIndex < static_cast<S32>(settings.mArmed.size()))
            for (const auto& candidate : in.sources)
                if (candidate.handle.mId == settings.mArmed[settings.mPreviewArmIndex].mCaptureId) target = &candidate;
        mReservationEnd = in.now + gateLeadSeconds(settings.mPrewarmFrames,
            in.frameDt > 0. ? in.frameDt : in.dt, target && isMediaKind(target->kind));
    }
    if (in.take) { mPending = true; if (mOverrun < 0.) mOverrun = in.now; ++mRevision; }
    const auto* program = source(in, mProgram);
    if (mProgram.mId.notNull() && (!program || !program->available || !program->permitted ||
        (privateFeed(program) && (!granted(mProgram) || !program->refusal.empty()))))
    { retire(mProgram, in.now, out); mProgram = {}; if (mPhase != EGatePhase::CROSSFADING) mPhase = EGatePhase::HOLDING; ++mRevision; }
    if (mPhase == EGatePhase::CROSSFADING)
    {
        const auto* incoming = source(in, mCandidate);
        if (!incoming || !incoming->available || !incoming->permitted ||
            (privateFeed(incoming) && (!granted(mCandidate) || !incoming->refusal.empty())))
        { skip(in, "Incoming feed unavailable", out); mAlpha = 0.; return out; }
        mAlpha = mProgram.mId.isNull() ? 1. : llclamp((in.now - mFadeStart) / mFadeSeconds, 0., 1.);
        if (mAlpha < 1.) return out;
        if (!mFinishing)
        {
            mFinishing = true; mStage = in.now; mProposal = ++mToken;
            out.push_back({EGateAction::PROPOSE, mCandidate, mProposal, 0., 1., mFadeSeconds});
            return out;
        }
        if (in.accepted != mProposal)
        {
            if (in.now - mStage >= 2.) skip(in, "Output publication timed out", out);
            return out;
        }
        auto committed = acknowledgeOutputs(in.accepted, in);
        out.insert(out.end(), committed.begin(), committed.end());
        return out;
    }
    const bool auto_cycle = settings.mMode == EGateMode::AUTO_CYCLE;
    mEffectiveFade = mProgram.mId.notNull() && in.mediaEnabled && settings.mTransition == EGateTransition::CROSSFADE;
    if (mEffectiveFade && !in.destinations.fits() && settings.mFadeOverBudget == EGateFadeOverBudget::CUT_INSTEAD)
    { mEffectiveFade = false; mReason = "Crossfade over budget - cut"; }
    mBoundary = mDwellStart + settings.mIntervalSeconds;
    if (!sameFeed(mEndProgram, mProgram) || settings.mVideoDwell != EGateVideoDwell::PLAY_TO_END)
    { mEndProgram = mProgram; mKnownEndTime = mEndBoundary = std::numeric_limits<F64>::infinity(); }
    if (privateFeed(program) && settings.mVideoDwell == EGateVideoDwell::PLAY_TO_END)
    {
        if (program->duration > 0.)
        {
            if ((!program->eof || !std::isfinite(mKnownEndTime)) && program->sampleAge <= .5 &&
                std::isfinite(program->position) && std::isfinite(program->error) && program->error >= 0. && program->error < .15)
                mKnownEndTime = in.now + program->duration - program->position - program->error;
            mBoundary = std::isfinite(mKnownEndTime) ? std::max(mDwellStart, mKnownEndTime -
                (mEffectiveFade ? settings.mCrossfadeSeconds : 0.)) : program->eof ? in.now : std::numeric_limits<F64>::infinity();
        }
        else mBoundary = program->eof ? in.now : std::numeric_limits<F64>::infinity();
        if (mBoundary <= in.now) mEndBoundary = std::min(mEndBoundary, mBoundary);
        mBoundary = std::min(mBoundary, mEndBoundary); // EOF or a reached end never moves a deadline forward
    }
    bool due = mProgram.mId.isNull() || mPending || (auto_cycle && in.now >= mBoundary);
    if (due && mProgram.mId.notNull() && mOverrun < 0.) mOverrun = in.now;
    if (mPhase == EGatePhase::HOLDING && auto_cycle && in.now >= mBoundary)
    { mVisited.clear(); mPhase = EGatePhase::ON_AIR; }
    if (in.changed && mCandidate.mId.notNull() && !auto_cycle && settings.mPreviewArmIndex >= 0 &&
        settings.mPreviewArmIndex < static_cast<S32>(settings.mArmed.size()) &&
        settings.mArmed[settings.mPreviewArmIndex].mCaptureId != mCandidate.mId)
    {
        retire(mCandidate, in.now, out); mCandidate = {}; mProposal = 0;
        mPending = in.take; mPhase = EGatePhase::ON_AIR; ++mRevision;
    }
    // No catch-up loop: a pass visits at most the authored eight arms.
    if (mCandidate.mId.isNull() && (mPhase != EGatePhase::HOLDING || in.changed || in.playVideos || in.take ||
        (auto_cycle && in.now >= mDwellStart + settings.mIntervalSeconds)))
    {
        S32 start = mProgram.mId.isNull() ? llclamp(settings.mProgramArmIndex, 0, static_cast<S32>(settings.mArmed.size()) - 1) - 1 : -1;
        for (S32 i = 0; i < static_cast<S32>(settings.mArmed.size()); ++i)
            if (settings.mArmed[i].mCaptureId == mProgram.mId) start = i;
        for (S32 n = 0; n < static_cast<S32>(settings.mArmed.size()); ++n)
        {
            const S32 index = !auto_cycle && mProgram.mId.notNull() ? settings.mPreviewArmIndex :
                (start + 1 + n) % static_cast<S32>(settings.mArmed.size());
            if (index < 0 || index >= static_cast<S32>(settings.mArmed.size())) break;
            const auto& arm = settings.mArmed[index];
            if ((!arm.mEnabled && auto_cycle) || arm.mCaptureId == mProgram.mId ||
                std::find(mVisited.begin(), mVisited.end(), arm.mCaptureId) != mVisited.end()) continue;
            const GateSourceEvent* s = nullptr;
            for (const auto& candidate : in.sources) if (candidate.handle.mId == arm.mCaptureId) s = &candidate;
            if (!s) { mVisited.push_back(arm.mCaptureId); continue; }
            auto& f = feed(s->handle);
            if (privateFeed(s) && (!in.mediaEnabled || !f.grant))
            { f.status = "Waiting for Play videos"; mVisited.push_back(arm.mCaptureId); continue; }
            if (!s->available || !s->permitted || !s->refusal.empty())
            { f.status = "Skipped: " + (s->refusal.empty() ? "Feed unavailable" : s->refusal); mVisited.push_back(arm.mCaptureId); continue; }
            if (privateFeed(s) && (charges() >= 2 || f.retiring))
            { mReason = "Waiting for player retirement"; continue; }
            if (mProgram.mId.isNull())
                mReservationEnd = in.now + gateLeadSeconds(settings.mPrewarmFrames,
                    in.frameDt > 0. ? in.frameDt : in.dt, isMediaKind(s->kind));
            mWarmFrame = s->frame;
            mCandidate = s->handle; mCommand = ++mToken; f.token = mCommand;
            f.charged = privateFeed(s); f.parked = false; f.released = false; f.closeRequested = false;
            f.status = "Warming..."; mPhase = EGatePhase::WARMING; mStage = in.now;
            mPreparedOnCut = s->onCut; mPreparedLoop = s->loop; mPreparedDwell = settings.mVideoDwell;
            mDecodeElapsed = 0.; mHoldingBudget = false; mSeeking = mParked = mResuming = mCorrected = mRestart = mConverged = false;
            out.push_back({EGateAction::WARM, mCandidate, mCommand, 0., 0., 0.,
                s->loop && settings.mVideoDwell != EGateVideoDwell::PLAY_TO_END});
            ++mRevision; break;
        }
    }
    if (mCandidate.mId.isNull())
    {
        if (due && mPhase != EGatePhase::HOLDING) { mPhase = EGatePhase::HOLDING; mPending = false; mReason = "No usable successor"; mDwellStart = in.now; ++mRevision; }
        return out;
    }
    const auto* s = source(in, mCandidate);
    const auto candidate_arm = std::find_if(settings.mArmed.begin(), settings.mArmed.end(),
        [this](const auto& arm) { return arm.mCaptureId == mCandidate.mId; });
    if (auto_cycle && (candidate_arm == settings.mArmed.end() || !candidate_arm->mEnabled))
    { skip(in, "Auto arm disabled", out); return out; }
    if (!s || !s->available || !s->permitted || !s->refusal.empty())
    { skip(in, s && !s->refusal.empty() ? s->refusal : "Feed unavailable", out); return out; }
    if (privateFeed(s) && !granted(mCandidate))
    { skip(in, "Waiting for Play videos", out); return out; }
    if (privateFeed(s) && (mPreparedOnCut != s->onCut || mPreparedLoop != s->loop || mPreparedDwell != settings.mVideoDwell))
    { skip(in, "Playback settings changed", out); mVisited.clear(); return out; }
    if (mProposal && in.accepted == mProposal)
    {
        auto committed = acknowledgeOutputs(in.accepted, in);
        out.insert(out.end(), committed.begin(), committed.end());
        return out;
    }
    // A later arm in this bounded pass needs its first render opportunity;
    // selecting it must not reject its retained (necessarily stale) frame.
    if (mOverrun >= 0. && in.now > mStage && in.now - mOverrun >= 2. &&
        (privateFeed(s) || !freshImage(*s) || (mEffectiveFade && !in.destinations.fits() && settings.mFadeOverBudget == EGateFadeOverBudget::HOLD)))
    {
        skip(in, mEffectiveFade && !in.destinations.fits() && settings.mFadeOverBudget == EGateFadeOverBudget::HOLD ?
            "Crossfade over budget - hold timed out" : "Take timed out", out);
        mPending = false; return out;
    }
    if (mPhase != EGatePhase::READY && (privateFeed(s) || wantsImage(in)) &&
        (in.now - mStage >= 1800. || (s->admitted && s->opportunity && (mDecodeElapsed += std::max(0., in.dt)) > 10.)))
    { skip(in, "Seek/frame deadline", out); return out; }
    // A READY interval Cut inside the outgoing wrap window wins over any
    // outgoing pre-end seek. Native-repeat-only does not schedule such a seek.
    if (auto_cycle && !mEffectiveFade && privateFeed(program) && program->loop && program->duration > 0. &&
        std::isfinite(program->position) && std::isfinite(program->error) && program->error >= 0. && program->error < .15 &&
        program->sampleAge >= 0. && program->sampleAge <= .5)
    {
        const F64 end = in.now + program->duration - program->position;
        if (mBoundary >= end - program->error - .15 && mBoundary <= end + .05)
            mBoundary = s->ready ? std::max(mDwellStart, end - program->error) : end + .05;
        due = mPending || in.now >= mBoundary;
    }
    if (privateFeed(s))
    {
        const bool budget_hold = mEffectiveFade && settings.mFadeOverBudget == EGateFadeOverBudget::HOLD && !in.destinations.fits();
        if (budget_hold && mParked)
        {
            mHoldingBudget = true; mPhase = EGatePhase::READY;
            mReason = "Crossfade over budget - holding (up to 2 s)";
            if (!s->paused && mResuming)
            {
                mResuming = false; mCommand = ++mToken;
                out.push_back({EGateAction::PARK, mCandidate, mCommand, mTarget});
            }
            return out; // preserve the parked beginning, including the pre-roll lead
        }
        if (mHoldingBudget && !budget_hold)
        {
            mHoldingBudget = false; mSeeking = mParked = mResuming = mCorrected = mConverged = false;
            mStage = in.now; mDecodeElapsed = 0.; mProposal = 0; // original mOverrun still owns the 2 s cap
        }
        auto& f = feed(mCandidate);
        if (!s->admitted || !s->metadataReady) return out;
        // Observe each completed image exactly once, never a command receipt.
        if (mResuming && !mResumeImage && s->token == mCommand && s->frame > mParkFrame &&
            (s->resumed || s->eof))
        {
            mContributionTime = in.now - std::max(0., s->frameAge);
            f.latency.resume = GateLatency::sample(f.latency.resume, mContributionTime - mCommandStart);
            mResumeImage = true;
        }
        const bool now_based = mPending || mProgram.mId.isNull() ||
            (auto_cycle && (!std::isfinite(mBoundary) || in.now >= mBoundary));
        const F64 predicted = in.now + f.latency.seek + f.latency.resume +
            llclamp(in.frameDt > 0. ? in.frameDt : in.dt, 1. / 240., .1);
        const F64 commit = mSeeking ? mTargetCommit : now_based ? predicted :
            auto_cycle ? mBoundary : mStage;
        auto target = gatePosition(s->onCut, f.bookmark, f.position, f.off, commit,
            mRestart ? 0. : s->duration, s->loop, s->framePeriod);
        if (!target.valid) { skip(in, "Invalid media clock", out); return out; }
        if (target.restart) { mRestart = true; f.status = target.reason; }
        const bool terminal = target.terminal;
        const F64 advance = auto_cycle && !now_based && std::isfinite(mBoundary)
            ? std::max(0., mBoundary - in.now) : 0.;
        // One correction also handles a seed that resumed a scheduled source
        // too early: re-seek now, then use its observed resume latency.
        const bool early = mResuming && mResumeImage && advance > 0. &&
            gatePositionDistance(s->position + advance, mTarget, s->duration, s->loop) + s->error > .15;
        const bool correct = mSeeking && !mCorrected && mResuming && mResumeImage &&
            ((s->onCut == EMediaOnCut::LIVE && !mRestart) ||
             auto_cycle) && (early || !contributionQualified(in));
        if (!mSeeking || correct)
        {
            const F64 correction_commit = now_based ? predicted : auto_cycle ? mBoundary : predicted;
            if (correct) target = gatePosition(s->onCut, f.bookmark, f.position, f.off,
                correction_commit, s->duration, s->loop, s->framePeriod);
            if (target.seconds > 0. && !s->seekable)
            { skip(in, s->onCut == EMediaOnCut::LIVE ? "Live needs seek support" : "Bookmark needs seek support", out); return out; }
            mCorrected = mSeeking; mSeeking = true; mParked = false; mResuming = false; mConverged = false;
            mResumeImage = false; mTargetNowBased = f.bookmark && now_based;
            f.status = "Warming... Seeking";
            mTarget = target.seconds; mTargetCommit = correct ? correction_commit : commit;
            mCommandStart = in.now; mParkFrame = s->frame; mCommand = ++mToken;
            out.push_back({EGateAction::SEEK, mCandidate, mCommand, mTarget}); return out;
        }
        const F64 drift = gatePositionDistance(s->position, mTarget, s->duration, s->loop);
        if (terminal && s->eof && s->token == mCommand && s->frame > mParkFrame &&
            gatePositionQualified(drift, s->error, s->sampleAge, s->frameAge, s->framePeriod))
        {
            mParked = mResuming = mResumeImage = true;
            mContributionTime = mTargetCommit; // terminal image has no advancing Live clock
        }
        // A paused seek publishes one decoded image; it need not manufacture
        // a second playing frame merely to prove that its held image is ready.
        if (!mParked && s->paused && s->token == mCommand && s->frame > mParkFrame &&
            gatePositionQualified(drift, s->error, s->sampleAge, 0., s->framePeriod))
        {
            f.latency.seek = GateLatency::sample(f.latency.seek,
                in.now - std::max(0., s->frameAge) - mCommandStart);
            mParked = true; f.status = "Ready"; mCommand = ++mToken;
            out.push_back({EGateAction::PARK, mCandidate, mCommand, mTarget}); return out;
        }
        if (!mParked || (!mResuming && !s->paused)) return out;
        if (!mResuming && (due || (auto_cycle && commit - in.now <= f.latency.resume)))
        {
            mResuming = true; mCommand = ++mToken; mParkFrame = s->frame;
            mResumeImage = false; mCommandStart = in.now;
            out.push_back({EGateAction::RESUME, mCandidate, mCommand, mTarget}); return out;
        }
        if (mResuming && !contributionQualified(in)) return out;
    }
    if (!freshImage(*s)) return out;
    if (mPhase != EGatePhase::READY) { mPhase = EGatePhase::READY; feed(mCandidate).status = "Ready"; ++mRevision; }
    if (!due || in.destinations.passes == 0) return out;
    if (mEffectiveFade && !in.destinations.fits() && settings.mFadeOverBudget == EGateFadeOverBudget::HOLD)
    {
        mReason = "Crossfade over budget - holding (up to 2 s)";
        return out;
    }
    if (!mProposal)
    {
        mProposal = ++mToken;
        out.push_back({EGateAction::PROPOSE, mCandidate, mProposal, 0., mEffectiveFade ? 0. : 1.,
            mEffectiveFade && mProgram.mId.notNull() ? settings.mCrossfadeSeconds : 0.});
        return out;
    }
    if (in.accepted != mProposal) return out;
    auto committed = acknowledgeOutputs(in.accepted, in);
    out.insert(out.end(), committed.begin(), committed.end());
    return out;
}

S32 gateNextEnabledIndex(const std::vector<GateArmedCamera>& armed,
                         S32 current_index)
{
    if (armed.empty()) return -1;
    const S32 count = static_cast<S32>(armed.size());
    for (S32 offset = 1; offset <= count; ++offset)
    {
        S32 candidate = (current_index + offset) % count;
        if (candidate < 0) candidate += count;
        if (armed[static_cast<std::size_t>(candidate)].mEnabled)
            return candidate;
    }
    return -1;
}

S32 gateClampEnabledIndex(const std::vector<GateArmedCamera>& armed,
                          S32 preferred_index)
{
    if (armed.empty()) return -1;
    const S32 count = static_cast<S32>(armed.size());
    const S32 preferred = llclamp(preferred_index, 0, count - 1);
    if (armed[static_cast<std::size_t>(preferred)].mEnabled) return preferred;
    return gateNextEnabledIndex(armed, preferred - 1);
}

S32 gateResolveArmIndex(const std::vector<GateArmedCamera>& armed,
                        S32 preferred_index,
                        const std::vector<LLUUID>& live_capture_ids)
{
    if (armed.empty()) return -1;
    const S32 count = static_cast<S32>(armed.size());
    const bool preferred_valid = preferred_index >= 0 && preferred_index < count;
    if (preferred_valid && containsCapture(
            live_capture_ids,
            armed[static_cast<std::size_t>(preferred_index)].mCaptureId))
    {
        return preferred_index;
    }

    const S32 start = preferred_valid ? preferred_index : -1;
    for (S32 offset = 1; offset <= count; ++offset)
    {
        const S32 candidate = (start + offset) % count;
        const GateArmedCamera& arm =
            armed[static_cast<std::size_t>(candidate)];
        if (arm.mEnabled && containsCapture(live_capture_ids, arm.mCaptureId))
            return candidate;
    }
    return -1;
}

void gateFilterResolvableArms(GateSettings& settings,
                              const std::vector<LLUUID>& live_capture_ids)
{
    const std::vector<GateArmedCamera> stored = settings.mArmed;
    std::vector<S32> index_map(stored.size(), -1);
    settings.mArmed.clear();
    for (std::size_t index = 0; index < stored.size(); ++index)
    {
        if (!containsCapture(live_capture_ids, stored[index].mCaptureId)) continue;
        index_map[index] = static_cast<S32>(settings.mArmed.size());
        settings.mArmed.push_back(stored[index]);
    }
    const auto remap = [&index_map](S32 stored_index)
    {
        return stored_index >= 0 &&
                stored_index < static_cast<S32>(index_map.size())
            ? index_map[static_cast<std::size_t>(stored_index)] : -1;
    };
    settings.mProgramArmIndex = remap(settings.mProgramArmIndex);
    settings.mPreviewArmIndex = remap(settings.mPreviewArmIndex);
    if (settings.mProgramArmIndex < 0)
    {
        settings.mProgramArmIndex = gateClampEnabledIndex(settings.mArmed, 0);
        if (settings.mProgramArmIndex < 0 && !settings.mArmed.empty())
            settings.mProgramArmIndex = 0;
    }
    if (settings.mArmed.empty()) settings.mActive = false;
}

F64 gateNextCutTime(F64 past_boundary, F64 interval_seconds)
{
    if (!std::isfinite(past_boundary) || !std::isfinite(interval_seconds) ||
        interval_seconds <= 0.0)
    {
        return 0.0;
    }
    return past_boundary + interval_seconds;
}

bool gateWarmActive(F64 now, F64 next_cut_time, U32 prewarm_frames,
                    F64 frame_dt)
{
    return prewarm_frames > 0 && std::isfinite(now) &&
        std::isfinite(next_cut_time) && next_cut_time > 0.0 &&
        std::isfinite(frame_dt) && frame_dt > 0.0 &&
        now >= next_cut_time - static_cast<F64>(prewarm_frames) * frame_dt;
}

bool gateWarmSeedValid(U32 width, U32 height)
{
    return width > 0 && height > 0;
}

bool gateConsumePreviewWarmFrame(U32& frames_remaining)
{
    if (frames_remaining == 0) return false;
    --frames_remaining;
    return true;
}

bool gateIsRealCut(S32 current_index, S32 candidate_index)
{
    return current_index >= 0 && candidate_index >= 0 &&
        current_index != candidate_index;
}

ERegistryResult gateFeedbackResult(const LLUUID& camera_source_object_id,
                                   const LLUUID& display_object_id,
                                   bool display_gate_subscribed)
{
    return display_gate_subscribed && camera_source_object_id.notNull() &&
            camera_source_object_id == display_object_id
        ? ERegistryResult::INVALID_CONFIGURATION
        : ERegistryResult::OK;
}

bool gateDisplayCaptureReferenceAccepted(bool gate_source,
                                         bool capture_id_resolves)
{
    return gate_source || capture_id_resolves;
}

void gateRouteDisplayBinding(S32 capture_slot, U64 capture_generation,
                             U32& display_slot, U64& display_generation)
{
    if (capture_slot < 0 ||
        capture_slot >= static_cast<S32>(MAX_CAPTURES))
    {
        display_slot = MAX_CAPTURES;
        display_generation = 0;
        return;
    }
    display_slot = static_cast<U32>(capture_slot);
    display_generation = capture_generation;
}

LLSD gateSettingsToLLSD(const GateSettings& settings)
{
    LLSD gate = LLSD::emptyMap();
    gate["gate_id"] = settings.mGateId;
    gate["active"] = settings.mActive;
    gate["mode"] = settings.mMode == EGateMode::AUTO_CYCLE
        ? "auto_cycle" : "manual";
    gate["interval_seconds"] = settings.mIntervalSeconds;
    gate["prewarm_frames"] = static_cast<S32>(settings.mPrewarmFrames);
    gate["transition"] = settings.mTransition == EGateTransition::CUT ? "cut" : "crossfade";
    gate["crossfade_seconds"] = settings.mCrossfadeSeconds;
    gate["fade_over_budget"] = settings.mFadeOverBudget == EGateFadeOverBudget::CUT_INSTEAD ? "cut_instead" :
        settings.mFadeOverBudget == EGateFadeOverBudget::FADE_ANYWAY ? "fade_anyway" : "hold";
    gate["video_dwell"] = settings.mVideoDwell == EGateVideoDwell::INTERVAL ? "interval" : "play_to_end";
    gate["program_arm_index"] = settings.mProgramArmIndex;
    gate["preview_arm_index"] = settings.mPreviewArmIndex;
    gate["armed"] = LLSD::emptyArray();
    for (const GateArmedCamera& armed : settings.mArmed)
    {
        LLSD item = LLSD::emptyMap();
        item["arm_id"] = armed.mArmId;
        item["label"] = armed.mLabel;
        item["enabled"] = armed.mEnabled;
        item["capture_id"] = armed.mCaptureId;
        gate["armed"].append(item);
    }
    return gate;
}

bool gateSettingsFromLLSD(const LLSD& data, GateSettings& settings,
                          std::string* reason)
{
    const auto fail = [reason](const std::string& message)
    {
        if (reason) *reason = message;
        return false;
    };
    if (!data.isMap() || !data.has("gate_id") || !data.has("active") ||
        !data.has("mode") || !data.has("interval_seconds") ||
        !data.has("prewarm_frames") || !data.has("program_arm_index") ||
        !data.has("armed") || !data["armed"].isArray() ||
        !isNumeric(data["interval_seconds"]) ||
        !data["prewarm_frames"].isInteger() ||
        !data["program_arm_index"].isInteger())
    {
        return fail("A Prism Gate is missing required fields.");
    }

    GateSettings parsed;
    if (data.has("transition"))
    {
        if (!data["transition"].isString()) return fail("Invalid Gate transition.");
        const auto value = data["transition"].asString();
        if (value == "crossfade") parsed.mTransition = EGateTransition::CROSSFADE;
        else if (value != "cut") return fail("Unknown Gate transition.");
    }
    if (data.has("crossfade_seconds"))
    {
        if (!isNumeric(data["crossfade_seconds"])) return fail("Invalid Gate fade length.");
        parsed.mCrossfadeSeconds = data["crossfade_seconds"].asReal();
        if (!std::isfinite(parsed.mCrossfadeSeconds) || parsed.mCrossfadeSeconds < .1 || parsed.mCrossfadeSeconds > 3.)
            return fail("Gate fade length must be between 0.1 and 3 seconds.");
    }
    if (data.has("fade_over_budget"))
    {
        if (!data["fade_over_budget"].isString()) return fail("Invalid Gate fade budget policy.");
        const auto value = data["fade_over_budget"].asString();
        if (value == "fade_anyway") parsed.mFadeOverBudget = EGateFadeOverBudget::FADE_ANYWAY;
        else if (value == "hold") parsed.mFadeOverBudget = EGateFadeOverBudget::HOLD;
        else if (value != "cut_instead") return fail("Unknown Gate fade budget policy.");
    }
    if (data.has("video_dwell"))
    {
        if (!data["video_dwell"].isString()) return fail("Invalid Gate video dwell.");
        const auto value = data["video_dwell"].asString();
        if (value == "play_to_end") parsed.mVideoDwell = EGateVideoDwell::PLAY_TO_END;
        else if (value != "interval") return fail("Unknown Gate video dwell.");
    }
    parsed.mGateId = data["gate_id"].asUUID();
    if (parsed.mGateId.isNull()) return fail("A Prism Gate ID must be nonnull.");
    parsed.mActive = data["active"].asBoolean();
    const std::string mode = data["mode"].asString();
    if (mode == "manual") parsed.mMode = EGateMode::MANUAL;
    else if (mode == "auto_cycle") parsed.mMode = EGateMode::AUTO_CYCLE;
    else return fail("Unknown Prism Gate mode.");
    parsed.mIntervalSeconds = data["interval_seconds"].asReal();
    if (!std::isfinite(parsed.mIntervalSeconds) ||
        parsed.mIntervalSeconds < MIN_GATE_INTERVAL_SECONDS ||
        parsed.mIntervalSeconds > MAX_GATE_INTERVAL_SECONDS)
    {
        return fail("Prism Gate interval must be between 0.5 and 120 seconds.");
    }
    const S32 prewarm = data["prewarm_frames"].asInteger();
    if (prewarm < 0 || prewarm > static_cast<S32>(MAX_GATE_PREWARM_FRAMES))
        return fail("Prism Gate pre-warm must be between 0 and 5 frames.");
    parsed.mPrewarmFrames = static_cast<U32>(prewarm);
    parsed.mProgramArmIndex = data["program_arm_index"].asInteger();
    parsed.mPreviewArmIndex = data.has("preview_arm_index") &&
            data["preview_arm_index"].isInteger()
        ? data["preview_arm_index"].asInteger() : -1;

    std::set<LLUUID> arm_ids;
    std::set<LLUUID> capture_ids;
    std::vector<S32> index_map;
    const LLSD& armed_data = data["armed"];
    index_map.reserve(static_cast<std::size_t>(armed_data.size()));
    for (LLSD::array_const_iterator it = armed_data.beginArray();
         it != armed_data.endArray(); ++it)
    {
        const LLSD& item = *it;
        index_map.push_back(-1);
        if (parsed.mArmed.size() >= MAX_GATE_ARMS || !item.isMap() ||
            !item.has("arm_id") || !item.has("label") ||
            !item.has("enabled") || !item.has("capture_id") ||
            !item["label"].isString())
        {
            continue;
        }
        GateArmedCamera armed;
        armed.mArmId = item["arm_id"].asUUID();
        armed.mLabel = item["label"].asString();
        armed.mEnabled = item["enabled"].asBoolean();
        armed.mCaptureId = item["capture_id"].asUUID();
        if (armed.mArmId.isNull() || armed.mCaptureId.isNull() ||
            armed.mLabel.size() > 128 ||
            !arm_ids.insert(armed.mArmId).second ||
            !capture_ids.insert(armed.mCaptureId).second)
        {
            continue;
        }
        index_map.back() = static_cast<S32>(parsed.mArmed.size());
        parsed.mArmed.push_back(armed);
    }

    const auto remapIndex = [&index_map](S32 stored)
    {
        return stored >= 0 && stored < static_cast<S32>(index_map.size())
            ? index_map[static_cast<std::size_t>(stored)] : -1;
    };
    parsed.mProgramArmIndex = remapIndex(parsed.mProgramArmIndex);
    parsed.mPreviewArmIndex = remapIndex(parsed.mPreviewArmIndex);
    if (parsed.mProgramArmIndex < 0)
    {
        parsed.mProgramArmIndex = gateClampEnabledIndex(parsed.mArmed, 0);
        if (parsed.mProgramArmIndex < 0 && !parsed.mArmed.empty())
            parsed.mProgramArmIndex = 0;
    }
    if (parsed.mArmed.empty()) parsed.mActive = false;
    settings = parsed;
    if (reason) reason->clear();
    return true;
}

bool gateSettingsFromScene(const LLSD& scene, GateSettings& settings,
                           bool& present, std::string* reason)
{
    settings = GateSettings();
    present = false;
    if (!scene.isMap() || !scene.has("prism_gates"))
    {
        if (reason) reason->clear();
        return true;
    }
    const LLSD& gates = scene["prism_gates"];
    if (!gates.isArray())
    {
        if (reason) *reason =
            "Ignored unusable Prism Gate metadata; displays keep fixed feeds.";
        return true;
    }
    for (LLSD::array_const_iterator it = gates.beginArray();
         it != gates.endArray(); ++it)
    {
        GateSettings candidate;
        if (gateSettingsFromLLSD(*it, candidate, nullptr))
        {
            settings = candidate;
            present = true;
            break;
        }
    }
    if (reason) reason->clear();
    return true;
}
} // namespace LLPrismLens
