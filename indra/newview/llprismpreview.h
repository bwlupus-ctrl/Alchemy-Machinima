/**
 * @file llprismpreview.h
 * @brief Demand and scoped render policy for Director VCam previews.
 */

#ifndef LL_LLPRISMPREVIEW_H
#define LL_LLPRISMPREVIEW_H

#include "llprismlens.h"

#include <array>

namespace LLPrismPreviewPolicy
{
constexpr U32 WIDTH = 192;
constexpr U32 HEIGHT = 108;
constexpr F32 MAX_FAR_CLIP = 64.f;

inline std::array<F32, 4> neutralAO()
{
    return { 1.f, 1.f, 1.f, 1.f };
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
    while (remaining_count > 0 && remaining > 0.f)
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
    return remaining;
}
} // namespace LLPrismPreviewPolicy

class LLPrismPreviewService
{
public:
    using VisibilityOwner = U64;

    static LLPrismPreviewService& instance();

    // Future VC6 page contract. Visibility is owner-scoped so the inline page
    // and advanced manager cannot hide one another's previews.
    void setDemand(const LLPrismLens::CaptureHandle& capture, bool demanded);
    void clearDemand();
    void setPageVisible(bool visible);
    void setConsumerVisible(VisibilityOwner owner, bool visible);
    void releaseConsumer(VisibilityOwner owner);

    bool prepareFrame();
    bool isDemanded(const LLPrismLens::CaptureHandle& capture) const;
    F32 requestedHz(const LLPrismLens::CaptureRateSettings& rate) const;
    void notePublication(const LLPrismLens::CaptureHandle& capture, U64 serial);
    void onRenderTargetsReleased();

private:
    LLPrismPreviewService() = default;

    struct DemandEntry
    {
        LLPrismLens::CaptureHandle mCapture;
        bool mDemanded = false;
        U64 mPublicationSerial = 0;
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
};

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
