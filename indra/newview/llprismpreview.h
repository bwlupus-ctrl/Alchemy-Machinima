/**
 * @file llprismpreview.h
 * @brief Demand and scoped render policy for Director VCam previews.
 */

#ifndef LL_LLPRISMPREVIEW_H
#define LL_LLPRISMPREVIEW_H

#include "llprismlens.h"
#include "llrect.h"
#include "llrendertarget.h"
#include "lltimer.h"

#include <array>
#include <cmath>
#include <limits>

namespace LLPrismPreviewPolicy
{
constexpr U32 WIDTH = 192;
constexpr U32 HEIGHT = 108;
constexpr F32 MAX_FAR_CLIP = 64.f;
constexpr F32 ENTITLEMENT_EPSILON = 0.001f;
constexpr F64 BUDGET_MS_PER_FRAME = 0.300;
constexpr F64 MAX_OPERATION_MS = 4.0;

inline std::array<F32, 4> neutralAO()
{
    return { 1.f, 1.f, 1.f, 1.f };
}

// This is the production AO seed decision as a pure operation so the renderer
// and regression test exercise the same stale-attachment replacement path.
inline void seedPreviewAO(bool preview_render, std::array<F32, 4>& attachment)
{
    if (preview_render)
    {
        attachment = neutralAO();
    }
}

inline F32 requestedHz(F32 shared_hz, LLPrismLens::EOutputRateMode mode,
                       F32 target_hz)
{
    const F32 bounded_shared = llclamp(shared_hz, 0.5f, 5.f);
    return mode == LLPrismLens::EOutputRateMode::TARGET_FPS
        ? llmin(bounded_shared, llmax(0.01f, target_hz))
        : bounded_shared;
}

inline F64 advanceDeadline(F64 now, F32 entitlement_hz)
{
    return now + 1.0 / static_cast<F64>(llmax(0.01f, entitlement_hz));
}

// Fit a producer aspect into the lifetime-fixed 16:9 preview pack. The
// retained publication exposes only this logical rectangle, avoiding an
// anamorphic stretch for non-16:9 cameras and Surface Lenses.
inline void letterboxExtent(F32 aspect, U32& width, U32& height)
{
    width = WIDTH;
    height = HEIGHT;
    if (!std::isfinite(aspect) || aspect <= 0.f) return;
    const F32 pack_aspect = static_cast<F32>(WIDTH) / static_cast<F32>(HEIGHT);
    if (aspect > pack_aspect)
    {
        height = llmax(1u, static_cast<U32>(ll_round(static_cast<F32>(WIDTH) / aspect)));
    }
    else if (aspect < pack_aspect)
    {
        width = llmax(1u, static_cast<U32>(ll_round(static_cast<F32>(HEIGHT) * aspect)));
    }
}

// Pure bounded max-min tier. Production invokes this once for visible/Gate
// producers and again with the returned capacity for preview-only producers.
inline F32 waterFillTier(const std::array<bool, LLPrismLens::MAX_CAPTURES>& active,
                         const std::array<F32, LLPrismLens::MAX_CAPTURES>& demand,
                         F32 capacity,
                         std::array<F32, LLPrismLens::MAX_CAPTURES>& entitlement)
{
    std::array<bool, LLPrismLens::MAX_CAPTURES> assigned{};
    U32 remaining_count = 0;
    for (U32 slot = 0; slot < LLPrismLens::MAX_CAPTURES; ++slot)
    {
        if (active[slot]) ++remaining_count;
    }

    F32 remaining = llmax(0.f, capacity);
    while (remaining_count > 0 && remaining > ENTITLEMENT_EPSILON)
    {
        const F32 share = remaining / static_cast<F32>(remaining_count);
        bool fixed_one = false;
        for (U32 slot = 0; slot < LLPrismLens::MAX_CAPTURES; ++slot)
        {
            if (!active[slot] || assigned[slot] || demand[slot] > share) continue;
            entitlement[slot] = demand[slot];
            remaining = llmax(0.f, remaining - entitlement[slot]);
            assigned[slot] = true;
            --remaining_count;
            fixed_one = true;
        }
        if (!fixed_one)
        {
            for (U32 slot = 0; slot < LLPrismLens::MAX_CAPTURES; ++slot)
            {
                if (active[slot] && !assigned[slot]) entitlement[slot] = share;
            }
            remaining = 0.f;
        }
    }
    return remaining > ENTITLEMENT_EPSILON ? remaining : 0.f;
}
} // namespace LLPrismPreviewPolicy

// Allocation-free policy core. Tests inject CPU/GPU/scratch costs here; the
// runtime feeds it delayed timestamp results through the same entry points.
class PrismPreviewBudgetController
{
public:
    enum class EOperation : U8 { AUXILIARY, ATLAS };
    struct Stats
    {
        F64 mCpuMs = 0.0;
        F64 mGpuMs = 0.0;
        F64 mChargedMs = 0.0;
        F64 mCpuP95Ms = 0.0;
        F64 mGpuP95Ms = 0.0;
        F64 mFrameDeltaP95Ms = 0.0;
        F64 mMaxOperationMs = 0.0;
        U32 mOperations = 0;
        U32 mGpuSamples = 0;
        U32 mDeferrals = 0;
        U32 mScratchReacquisitions = 0;
        U32 mPresentedFrames = 0;
    };

    void reset();
    void noteFrame(U32 frame, F64 now, F64 frame_ms, bool preview_work);
    bool reserve(EOperation operation, F64& reservation_ms);
    void complete(EOperation operation, F64 reservation_ms,
                  F64 cpu_ms, F64 gpu_ms, bool gpu_valid);
    void noteScratchReacquisition()
    {
        ++mStats.mScratchReacquisitions;
        tailViolation();
    }
    F32 effectiveRate(F32 requested) const;
    bool throttled() const { return mRateStep > 0 || mSuspended; }
    bool suspended() const { return mSuspended; }
    const Stats& stats() const { return mStats; }

private:
    static constexpr U32 SAMPLE_COUNT = 128;
    static constexpr U32 FRAME_COUNT = 512;
    F64 percentile95(const std::array<F64, SAMPLE_COUNT>& values,
                     U32 count) const;
    void tailViolation();
    std::array<F64, SAMPLE_COUNT> mCpuSamples{};
    std::array<F64, SAMPLE_COUNT> mGpuSamples{};
    std::array<F64, SAMPLE_COUNT> mChargedSamples{};
    std::array<F64, SAMPLE_COUNT> mBaselineFrames{};
    std::array<F64, SAMPLE_COUNT> mPreviewFrames{};
    std::array<F64, FRAME_COUNT> mFrameTimes{};
    std::array<F64, FRAME_COUNT> mFrameCharges{};
    std::array<U32, FRAME_COUNT> mFrameIds{};
    U32 mSampleCursor = 0;
    U32 mSampleCount = 0;
    U32 mBaselineCursor = 0;
    U32 mBaselineCount = 0;
    U32 mPreviewCursor = 0;
    U32 mPreviewCount = 0;
    U32 mCurrentFrame = 0;
    F64 mNow = 0.0;
    F64 mReservedMs = 0.0;
    std::array<F64, 2> mPredictionMs{ 1.5, 0.05 };
    U32 mRateStep = 0;
    U32 mCleanSamples = 0;
    bool mSuspended = false;
    Stats mStats;
};

class LLPrismPreviewService
{
public:
    using VisibilityOwner = U64;

    static LLPrismPreviewService& instance();

    enum class ETileState : U8
    {
        LIVE,
        PREVIEW,
        PAUSED,
        OFFLINE,
        THROTTLED
    };

    struct AtlasHandle
    {
        U64 mGeneration = 0;
    };

    struct TilePublication
    {
        AtlasHandle mAtlas;
        LLRectf mCellUV;
        ETileState mState = ETileState::OFFLINE;
        F64 mTimestamp = 0.0;
        U64 mPublicationSerial = 0;
    };

    // Future VC6 page contract. Visibility is owner-scoped so the inline page
    // and advanced manager cannot hide one another's previews.
    void setDemand(const LLPrismLens::CaptureHandle& capture, bool demanded);
    void clearDemand();
    void setPageVisible(bool visible);
    void setConsumerVisible(VisibilityOwner owner, bool visible);
    void releaseConsumer(VisibilityOwner owner);
    void setMonitorSelection(U32 capture_slot,
                             const LLPrismLens::DisplayHandle& display);
    AtlasHandle atlasHandle() const { return AtlasHandle{ mAtlasGeneration }; }
    bool bindAtlas(const AtlasHandle& handle, S32 texture_channel = 0);
    TilePublication tileState(const LLPrismLens::CaptureHandle& capture) const;
    U64 revision() const { return mRevision; }
    bool hasPendingAtlasUpdate() const;

    bool prepareFrame();
    // Called only after ScopedPrismRenderState is live. Failed cold allocation
    // is rate-limited so a low-memory viewer does not churn FBOs every frame.
    bool ensureScratchReady();
    bool isDemanded(const LLPrismLens::CaptureHandle& capture) const;
    F32 requestedHz(const LLPrismLens::CaptureRateSettings& rate) const;
    void notePublication(const LLPrismLens::CaptureHandle& capture, U64 serial,
                         bool preview_quality);
    // Called once from renderAuxiliaryView after its scene-render scope ended.
    // It performs at most one queued cell update.
    void updateAtlas();
    void onRenderTargetsReleased();
    bool beginOperation(PrismPreviewBudgetController::EOperation operation,
                        const LLPrismLens::CaptureHandle& capture,
                        F64& reservation_ms, S32& query_slot);
    void endOperation(PrismPreviewBudgetController::EOperation operation,
                      const LLPrismLens::CaptureHandle& capture,
                      F64 reservation_ms, S32 query_slot, F64 cpu_ms);
    bool budgetThrottled() const { return mBudget.throttled(); }
    bool budgetSuspended() const { return mBudget.suspended(); }
    void runSelfTest(F32 seconds = 20.f);

private:
    LLPrismPreviewService() = default;
    void pollQueries();
    void tickSelfTest();

    struct DemandEntry
    {
        LLPrismLens::CaptureHandle mCapture;
        bool mDemanded = false;
        U64 mPublicationSerial = 0;
        U64 mAtlasSerial = 0;
        bool mPreviewQuality = false;
    };
    struct VisibilityEntry
    {
        VisibilityOwner mOwner = 0;
        bool mVisible = false;
    };

    bool anyVisibleConsumer() const;
    bool anyDemand() const;

    std::array<DemandEntry, LLPrismLens::MAX_CAPTURES> mDemand;
    std::array<VisibilityEntry, 4> mVisibility;
    std::array<LLPrismLens::DisplayHandle, LLPrismLens::MAX_CAPTURES> mSelection;
    std::array<TilePublication, LLPrismLens::MAX_CAPTURES> mTiles;
    std::array<F64, LLPrismLens::MAX_CAPTURES> mNextAtlasDue{};
    LLRenderTarget mAtlas;
    U64 mAtlasGeneration = 1;
    U64 mRevision = 1;
    U32 mNextAtlasSlot = 0;
    U32 mLastAtlasFrame = std::numeric_limits<U32>::max();
    F64 mScratchRetryAfter = 0.0;
    bool mLastPreviewsEnabled = true;
    struct QueryEntry
    {
        U32 mStart = 0;
        U32 mEnd = 0;
        U32 mIssuedFrame = 0;
        F64 mReservationMs = 0.0;
        F64 mCpuMs = 0.0;
        PrismPreviewBudgetController::EOperation mOperation =
            PrismPreviewBudgetController::EOperation::ATLAS;
        bool mPending = false;
    };
    std::array<QueryEntry, 16> mQueries;
    PrismPreviewBudgetController mBudget;
    bool mScratchWarmed = false;
    enum class ESelfTestPhase : U8 { IDLE, BASELINE, WARMING, ENABLED };
    ESelfTestPhase mSelfTestPhase = ESelfTestPhase::IDLE;
    F64 mSelfTestPhaseStarted = 0.0;
    F32 mSelfTestSeconds = 20.f;
    PrismPreviewBudgetController::Stats mSelfTestStartStats;
    std::array<U32, LLPrismLens::MAX_CAPTURES> mSelfTestProduced{};
    F64 mSelfTestMaxOperationMs = 0.0;
    U32 mLastPreviewWorkFrame = std::numeric_limits<U32>::max();
};

class LLPrismPreviewOperation
{
public:
    LLPrismPreviewOperation(PrismPreviewBudgetController::EOperation operation,
                            const LLPrismLens::CaptureHandle& capture = {});
    ~LLPrismPreviewOperation();
    bool admitted() const { return mAdmitted; }
private:
    PrismPreviewBudgetController::EOperation mOperation;
    LLPrismLens::CaptureHandle mCapture;
    LLTimer mTimer;
    F64 mReservationMs = 0.0;
    S32 mQuerySlot = -1;
    bool mAdmitted = false;
};

namespace LLPrismPreview
{
void runSelfTest(F32 seconds = 20.f);
}

// Installed only for a preview-only scene attempt and nested inside the full
// Prism render-state scope. Destruction restores every cached pipeline flag on
// every return path before the outer scope restores camera/GL state.
class ScopedPrismPreviewQuality
{
public:
    ScopedPrismPreviewQuality();
    ~ScopedPrismPreviewQuality();
    ScopedPrismPreviewQuality(const ScopedPrismPreviewQuality&) = delete;
    ScopedPrismPreviewQuality& operator=(const ScopedPrismPreviewQuality&) = delete;

private:
    S32 mShadowDetail;
    bool mDeferredSSAO;
    bool mVolumetricLighting;
    bool mProjectorVolumetrics;
    F32 mFarClip;
    bool mPreviewRender;
};

#endif // LL_LLPRISMPREVIEW_H
