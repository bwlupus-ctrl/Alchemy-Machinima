/**
 * @file llprismpreview.cpp
 * @brief Demand and scoped render policy for Director VCam previews.
 */

#include "llviewerprecompiledheaders.h"

#include "llprismpreview.h"

#include "llviewercontrol.h"
#include "pipeline.h"

namespace
{
constexpr LLPrismPreviewService::VisibilityOwner PAGE_VISIBILITY_OWNER = 1;
}

LLPrismPreviewService& LLPrismPreviewService::instance()
{
    static LLPrismPreviewService service;
    return service;
}

void LLPrismPreviewService::setDemand(const LLPrismLens::CaptureHandle& capture,
                                      bool demanded)
{
    DemandEntry* empty = nullptr;
    for (DemandEntry& entry : mDemand)
    {
        if ((entry.mCapture.mId == capture.mId &&
             entry.mCapture.mGeneration == capture.mGeneration) ||
            (!entry.mCapture.mId.isNull() && entry.mCapture.mId == capture.mId))
        {
            entry = demanded ? DemandEntry{ capture, true, entry.mPublicationSerial }
                             : DemandEntry{};
            return;
        }
        if (!empty && !entry.mDemanded) empty = &entry;
    }
    if (demanded && empty && !capture.mId.isNull())
    {
        *empty = DemandEntry{ capture, true, 0 };
    }
}

void LLPrismPreviewService::clearDemand()
{
    for (DemandEntry& entry : mDemand) entry = DemandEntry{};
}

void LLPrismPreviewService::setPageVisible(bool visible)
{
    setConsumerVisible(PAGE_VISIBILITY_OWNER, visible);
}

void LLPrismPreviewService::setConsumerVisible(VisibilityOwner owner, bool visible)
{
    if (owner == 0) return;
    VisibilityEntry* empty = nullptr;
    for (VisibilityEntry& entry : mVisibility)
    {
        if (entry.mOwner == owner)
        {
            entry.mVisible = visible;
            return;
        }
        if (!empty && entry.mOwner == 0) empty = &entry;
    }
    if (empty) *empty = VisibilityEntry{ owner, visible };
}

void LLPrismPreviewService::releaseConsumer(VisibilityOwner owner)
{
    for (VisibilityEntry& entry : mVisibility)
    {
        if (entry.mOwner == owner)
        {
            entry = VisibilityEntry{};
            return;
        }
    }
}

bool LLPrismPreviewService::anyVisibleConsumer() const
{
    for (const VisibilityEntry& entry : mVisibility)
    {
        if (entry.mOwner != 0 && entry.mVisible) return true;
    }
    return false;
}

bool LLPrismPreviewService::anyDemand() const
{
    for (const DemandEntry& entry : mDemand)
    {
        if (entry.mDemanded) return true;
    }
    return false;
}

bool LLPrismPreviewService::prepareFrame()
{
    static LLCachedControl<bool> previews_enabled(
        gSavedSettings, "PrismMonitorPreviewsEnabled", true);
    if (!previews_enabled || !anyVisibleConsumer() || !anyDemand())
    {
        return false;
    }
    LL_PROFILE_ZONE_NAMED_CATEGORY_DISPLAY("VCam preview scratch warmup");
    return gPipeline.allocatePrismPreviewScratch();
}

bool LLPrismPreviewService::isDemanded(
    const LLPrismLens::CaptureHandle& capture) const
{
    for (const DemandEntry& entry : mDemand)
    {
        if (entry.mDemanded && entry.mCapture.mId == capture.mId &&
            entry.mCapture.mGeneration == capture.mGeneration) return true;
    }
    return false;
}

F32 LLPrismPreviewService::requestedHz(
    const LLPrismLens::CaptureRateSettings& rate) const
{
    static LLCachedControl<F32> preview_fps(
        gSavedSettings, "PrismMonitorPreviewFPS", 1.f);
    return LLPrismPreviewPolicy::requestedHz(preview_fps, rate.mMode,
                                             rate.mTargetFps);
}

void LLPrismPreviewService::notePublication(
    const LLPrismLens::CaptureHandle& capture, U64 serial)
{
    for (DemandEntry& entry : mDemand)
    {
        if (entry.mDemanded && entry.mCapture.mId == capture.mId &&
            entry.mCapture.mGeneration == capture.mGeneration)
        {
            entry.mPublicationSerial = serial;
            return;
        }
    }
}

void LLPrismPreviewService::onRenderTargetsReleased()
{
    // No GL name or LLRenderTarget pointer escapes the pipeline. Publication
    // serials are invalid until their captures publish again in the new context.
    for (DemandEntry& entry : mDemand) entry.mPublicationSerial = 0;
}

ScopedPrismPreviewQuality::ScopedPrismPreviewQuality()
    : mShadowDetail(LLPipeline::RenderShadowDetail),
      mDeferredSSAO(LLPipeline::RenderDeferredSSAO),
      mVolumetricLighting(LLPipeline::RenderVolumetricLighting),
      mProjectorVolumetrics(LLPipeline::BDMergeProjectorVolumetrics),
      mFarClip(LLPipeline::RenderFarClip),
      mPreviewRender(LLPipeline::sPrismPreviewRender)
{
    LLPipeline::sPrismPreviewRender = true;
    LLPipeline::RenderShadowDetail = 0;
    LLPipeline::RenderDeferredSSAO = false;
    LLPipeline::RenderVolumetricLighting = false;
    LLPipeline::BDMergeProjectorVolumetrics = false;
    LLPipeline::RenderFarClip = llmin(mFarClip, LLPrismPreviewPolicy::MAX_FAR_CLIP);
}

ScopedPrismPreviewQuality::~ScopedPrismPreviewQuality()
{
    LLPipeline::RenderFarClip = mFarClip;
    LLPipeline::BDMergeProjectorVolumetrics = mProjectorVolumetrics;
    LLPipeline::RenderVolumetricLighting = mVolumetricLighting;
    LLPipeline::RenderDeferredSSAO = mDeferredSSAO;
    LLPipeline::RenderShadowDetail = mShadowDetail;
    LLPipeline::sPrismPreviewRender = mPreviewRender;
}
