/**
 * @file alvcampagemodel.cpp
 */

#include "llviewerprecompiledheaders.h"

#include "alvcampagemodel.h"

#include "llcallbacklist.h"
#include "llselectmgr.h"
#include "llviewercamera.h"
#include "llviewerobjectlist.h"
#include "llviewercontrol.h"
#include "llprismpreview.h"

namespace
{
bool sameCapture(const LLPrismLens::CaptureHandle& left,
                 const LLPrismLens::CaptureHandle& right)
{
    return left.mId == right.mId && left.mGeneration == right.mGeneration;
}

bool sameDisplay(const LLPrismLens::DisplayHandle& left,
                 const LLPrismLens::DisplayHandle& right)
{
    return left.mId == right.mId && left.mGeneration == right.mGeneration;
}

}

void ALVCamPageModel::currentViewTransform(LLVector3& position,
                                           LLQuaternion& rotation)
{
    const LLViewerCamera& camera = LLViewerCamera::instance();
    position = camera.getOrigin();
    LLVector3 forward = camera.getAtAxis();
    LLVector3 up = camera.getUpAxis();
    LLVector3 right = forward % up;
    up = right % forward;
    forward.normVec();
    up.normVec();
    right.normVec();
    rotation = LLQuaternion(right, up, -forward);
}

ALVCamPageModel::ALVCamPageModel()
    : mAlive(std::make_shared<bool>(true))
{
    mConfigurationConnection = LLPrismLens::connectConfigurationChanged(
        [this](U64) { markDirty(DIRTY_CONFIGURATION); });
    mRuntimeConnection = LLPrismLens::connectRuntimeChanged(
        [this](U64) { markDirty(DIRTY_RUNTIME); });
    mPerformanceConnection = LLPrismLens::connectPerformanceChanged(
        [this](U64) { markDirty(DIRTY_PERFORMANCE); });
    mSelectionConnection = LLSelectMgr::getInstance()->mUpdateSignal.connect(
        [this]() { markDirty(DIRTY_SELECTION); });
    mPreviewConnection = LLPrismPreview::connectChanged(
        [this](U64) { markDirty(DIRTY_PREVIEW); });
}

ALVCamPageModel::~ALVCamPageModel()
{
    *mAlive = false;
}

boost::signals2::connection ALVCamPageModel::connectChanged(
    const changed_signal_t::slot_type& slot)
{
    return mChanged.connect(slot);
}

void ALVCamPageModel::markDirty(U32 dirty)
{
    mDirty |= dirty;
    if (!mPageVisible) return;
    if (mIdleQueued) return;
    mIdleQueued = true;
    const std::weak_ptr<bool> alive(mAlive);
    doOnIdleOneTime([this, alive]()
    {
        const std::shared_ptr<bool> token = alive.lock();
        if (!token || !*token) return;
        onIdleRefresh(alive);
    });
}

void ALVCamPageModel::setPageVisible(bool visible)
{
    if (mPageVisible == visible) return;
    mPageVisible = visible;
    if (visible)
        refreshNow(mDirty ? mDirty : DIRTY_ALL);
}

void ALVCamPageModel::onIdleRefresh(const std::weak_ptr<bool>& alive)
{
    const std::shared_ptr<bool> token = alive.lock();
    if (!token || !*token) return;
    mIdleQueued = false;
    if (!mPageVisible) return;
    refreshNow(mDirty);
}

void ALVCamPageModel::refreshNow(U32 dirty)
{
    if (dirty & (DIRTY_CONFIGURATION | DIRTY_RUNTIME))
    {
        mRegistry = LLPrismLens::registrySnapshot();
        mGate = LLPrismLens::gateSnapshot();
    }
    if (dirty & DIRTY_PERFORMANCE)
        mPerformance = LLPrismLens::performanceSnapshot();

    if (mSelectedCapture.mId.notNull() && !findCapture(mSelectedCapture))
    {
        mSelectedCapture = LLPrismLens::CaptureHandle();
        mSelectedDisplay = LLPrismLens::DisplayHandle();
    }
    if (mSelectedDisplay.mId.notNull())
    {
        const LLPrismLens::DisplayDefinition* display =
            findDisplay(mSelectedDisplay);
        if (!display || (mSelectedCapture.mId.notNull() &&
                         !sameCapture(display->mCapture, mSelectedCapture)))
            mSelectedDisplay = LLPrismLens::DisplayHandle();
    }
    for (U32 slot = 0; slot < LLPrismLens::MAX_CAPTURES; ++slot)
    {
        if (mMonitorDisplay[slot].mId.isNull()) continue;
        const LLPrismLens::DisplayDefinition* display =
            findDisplay(mMonitorDisplay[slot]);
        const LLPrismLens::CaptureDefinition* capture = nullptr;
        for (U32 index = 0; index < mRegistry.mCaptureCount; ++index)
            if (mRegistry.mCaptures[index].mSlot == slot)
            {
                capture = &mRegistry.mCaptures[index];
                break;
            }
        if (!display || !capture || !sameCapture(display->mCapture, capture->mHandle))
            mMonitorDisplay[slot] = LLPrismLens::DisplayHandle();
    }
    const U32 delivered = dirty;
    mDirty &= ~dirty;
    if (delivered) mChanged(delivered);
}

const LLPrismLens::CaptureDefinition* ALVCamPageModel::findCapture(
    const LLPrismLens::CaptureHandle& handle) const
{
    for (U32 index = 0; index < mRegistry.mCaptureCount; ++index)
        if (sameCapture(mRegistry.mCaptures[index].mHandle, handle))
            return &mRegistry.mCaptures[index];
    return nullptr;
}

const LLPrismLens::DisplayDefinition* ALVCamPageModel::findDisplay(
    const LLPrismLens::DisplayHandle& handle) const
{
    for (U32 index = 0; index < mRegistry.mDisplayCount; ++index)
        if (sameDisplay(mRegistry.mDisplays[index].mHandle, handle))
            return &mRegistry.mDisplays[index];
    return nullptr;
}

void ALVCamPageModel::setSelectedCapture(
    const LLPrismLens::CaptureHandle& capture)
{
    if (sameCapture(mSelectedCapture, capture)) return;
    mSelectedCapture = capture;
    mSelectedDisplay = LLPrismLens::DisplayHandle();
    markDirty(DIRTY_SELECTION);
}

void ALVCamPageModel::setSelectedDisplay(
    const LLPrismLens::DisplayHandle& display)
{
    if (sameDisplay(mSelectedDisplay, display)) return;
    mSelectedDisplay = display;
    markDirty(DIRTY_SELECTION);
}

LLPrismLens::DisplayHandle ALVCamPageModel::monitorDisplay(U32 capture_slot) const
{
    return capture_slot < LLPrismLens::MAX_CAPTURES
        ? mMonitorDisplay[capture_slot] : LLPrismLens::DisplayHandle();
}

bool ALVCamPageModel::setMonitorDisplay(
    const LLPrismLens::CaptureHandle& capture,
    const LLPrismLens::DisplayHandle& display)
{
    const LLPrismLens::CaptureDefinition* capture_definition =
        findCapture(capture);
    if (!capture_definition) return false;
    if (display.mId.notNull())
    {
        const LLPrismLens::DisplayDefinition* display_definition =
            findDisplay(display);
        if (!display_definition ||
            !sameCapture(display_definition->mCapture, capture))
            return false;
    }
    mMonitorDisplay[capture_definition->mSlot] = display;
    markDirty(DIRTY_SELECTION);
    return true;
}

void ALVCamPageModel::selectAfterAdd(
    const LLPrismLens::CaptureHandle& capture)
{
    mSelectedCapture = capture;
    mSelectedDisplay = LLPrismLens::DisplayHandle();
    refreshNow(DIRTY_CONFIGURATION | DIRTY_RUNTIME | DIRTY_SELECTION);
}

LLPrismLens::ERegistryResult ALVCamPageModel::addFromSelectedObject(
    std::string* reason)
{
    LLPrismLens::CaptureHandle capture;
    const LLPrismLens::ERegistryResult result =
        LLPrismLens::addCameraCaptureFromSelectedObject(&capture, reason);
    if (result != LLPrismLens::ERegistryResult::OK) return result;
    selectAfterAdd(capture);
    return result;
}

LLPrismLens::ERegistryResult ALVCamPageModel::addVirtualAtView(
    std::string* reason)
{
    LLVector3 position;
    LLQuaternion rotation;
    currentViewTransform(position, rotation);
    LLPrismLens::CaptureHandle capture;
    const LLPrismLens::ERegistryResult result = LLPrismLens::addVirtualCamera(
        &capture, position, rotation, reason);
    if (result != LLPrismLens::ERegistryResult::OK) return result;
    selectAfterAdd(capture);
    return result;
}

bool ALVCamPageModel::addOtsPair(std::string* reason)
{
    LLPrismLens::CaptureHandle first;
    LLPrismLens::CaptureHandle second;
    if (!LLPrismLens::buildOtsPairFromSubjects(&first, &second, reason))
        return false;
    selectAfterAdd(first);
    return true;
}

LLPrismLens::ERegistryResult ALVCamPageModel::addSurfaceLens(
    std::string* reason)
{
    LLPrismLens::CaptureHandle capture;
    const LLPrismLens::ERegistryResult result =
        LLPrismLens::addSurfaceLensFromSelectedFace(&capture, reason);
    if (result != LLPrismLens::ERegistryResult::OK) return result;
    selectAfterAdd(capture);
    return result;
}

bool ALVCamPageModel::removeCapture(
    const LLPrismLens::CaptureHandle& capture, std::string* reason)
{
    U32 removed_slot = LLPrismLens::MAX_CAPTURES;
    if (const LLPrismLens::CaptureDefinition* definition = findCapture(capture))
        removed_slot = definition->mSlot;
    if (!LLPrismLens::removeCapture(capture))
    {
        if (reason && reason->empty()) *reason = "That capture changed before removal.";
        return false;
    }
    mSelectedCapture = LLPrismLens::CaptureHandle();
    mSelectedDisplay = LLPrismLens::DisplayHandle();
    refreshNow(DIRTY_CONFIGURATION | DIRTY_RUNTIME | DIRTY_SELECTION);
    const LLPrismLens::CaptureDefinition* next = nullptr;
    for (U32 index = 0; index < mRegistry.mCaptureCount; ++index)
    {
        const LLPrismLens::CaptureDefinition& candidate = mRegistry.mCaptures[index];
        const bool next_after = next &&
            next->mSlot >= removed_slot;
        const bool candidate_after =
            candidate.mSlot >= removed_slot;
        if (!next || (!next_after && candidate_after) ||
            (next_after && candidate_after && candidate.mSlot < next->mSlot) ||
            (!next_after && !candidate_after && candidate.mSlot > next->mSlot))
            next = &candidate;
    }
    if (next) mSelectedCapture = next->mHandle;
    markDirty(DIRTY_SELECTION);
    return true;
}

LLPrismLens::ERegistryResult ALVCamPageModel::bindSelectedFace(
    const LLPrismLens::CaptureHandle& capture, LLPrismLens::EFitMode fit,
    std::string* reason)
{
    LLPrismLens::DisplayHandle display;
    const LLPrismLens::ERegistryResult result =
        LLPrismLens::addSelectedDisplay(capture, fit, &display, reason);
    if (result != LLPrismLens::ERegistryResult::OK) return result;
    return result;
}

LLPrismLens::ActionStatus ALVCamPageModel::bindSelectedFaceStatus(
    const LLPrismLens::CaptureHandle& capture) const
{
    return LLPrismLens::addDisplaySelectionStatus(capture);
}

LLPrismLens::ERegistryResult ALVCamPageModel::bindFaceTarget(
    const LLPrismLens::CaptureHandle& capture, LLViewerObject* object, S32 te,
    LLPrismLens::EFitMode fit, std::string* reason)
{
    LLPrismLens::DisplayHandle display;
    const LLPrismLens::ERegistryResult result = LLPrismLens::bindDisplayTarget(
        capture, object, te, fit, &display, reason);
    if (result != LLPrismLens::ERegistryResult::OK) return result;
    return result;
}

LLPrismLens::ERegistryResult ALVCamPageModel::reassignDisplay(
    const LLPrismLens::DisplayHandle& display,
    const LLPrismLens::CaptureHandle& capture, std::string* reason)
{
    return LLPrismLens::reassignDisplay(display, capture, reason);
}

LLPrismLens::ActionStatus ALVCamPageModel::reassignDisplayStatus(
    const LLPrismLens::DisplayHandle& display,
    const LLPrismLens::CaptureHandle& capture)
{
    LLPrismLens::ActionStatus status;
    const LLPrismLens::RegistrySnapshot snapshot =
        LLPrismLens::registrySnapshot();
    const LLPrismLens::DisplayDefinition* display_definition = nullptr;
    const LLPrismLens::CaptureDefinition* old_capture = nullptr;
    const LLPrismLens::CaptureDefinition* destination_capture = nullptr;
    for (U32 index = 0; index < snapshot.mCaptureCount; ++index)
    {
        const LLPrismLens::CaptureDefinition& candidate =
            snapshot.mCaptures[index];
        if (sameCapture(candidate.mHandle, capture))
            destination_capture = &candidate;
    }
    for (U32 index = 0; index < snapshot.mDisplayCount; ++index)
    {
        const LLPrismLens::DisplayDefinition& candidate =
            snapshot.mDisplays[index];
        if (sameDisplay(candidate.mHandle, display))
        {
            display_definition = &candidate;
            break;
        }
    }
    if (!destination_capture || !display_definition)
    {
        status.mResult = LLPrismLens::ERegistryResult::STALE_HANDLE;
        status.mReason = display_definition
            ? "That capture no longer exists." : "That display handle is stale.";
        return status;
    }
    if (destination_capture->mMode ==
        LLPrismLens::ECaptureMode::SURFACE_LENS)
    {
        status.mResult = LLPrismLens::ERegistryResult::INVALID_CONFIGURATION;
        status.mReason = "A surface lens owns exactly one display face.";
        return status;
    }
    if (sameCapture(display_definition->mCapture, capture))
    {
        status.mResult = LLPrismLens::ERegistryResult::DUPLICATE;
        status.mReason = "That display already uses this capture.";
        return status;
    }
    for (U32 index = 0; index < snapshot.mCaptureCount; ++index)
    {
        const LLPrismLens::CaptureDefinition& candidate =
            snapshot.mCaptures[index];
        if (sameCapture(candidate.mHandle, display_definition->mCapture))
        {
            old_capture = &candidate;
            break;
        }
    }
    if (!old_capture)
    {
        status.mResult = LLPrismLens::ERegistryResult::STALE_HANDLE;
        status.mReason = "The display's capture is no longer valid.";
        return status;
    }
    if (old_capture->mMode == LLPrismLens::ECaptureMode::SURFACE_LENS)
    {
        status.mResult = LLPrismLens::ERegistryResult::INVALID_CONFIGURATION;
        status.mReason =
            "A Surface Lens aperture cannot be moved to another capture.";
        return status;
    }
    if (display_definition->mGateSubscribed)
    {
        status.mResult = LLPrismLens::ERegistryResult::INVALID_CONFIGURATION;
        status.mReason =
            "A Gate-subscribed display must be changed to a fixed feed before reassignment.";
        return status;
    }
    if (display_definition->mSettings.mVirtual)
    {
        status.mResult = LLPrismLens::ERegistryResult::OK;
        return status;
    }
    LLViewerObject* object = gObjectList.findObject(
        display_definition->mDisplayObjectId);
    return LLPrismLens::displayTargetStatus(
        capture, object, display_definition->mDisplayTextureEntry, &display);
}

ALVCamPageModel::FaceDropStatus ALVCamPageModel::faceDropStatus(
    const LLUUID& capture_id, LLViewerObject* object, S32 te)
{
    FaceDropStatus result;
    static LLCachedControl<bool> prism_enabled(
        gSavedSettings, "PrismLensEnabled", false);
    if (!prism_enabled)
    {
        result.mAction.mResult =
            LLPrismLens::ERegistryResult::INVALID_CONFIGURATION;
        result.mAction.mReason =
            "Turn on Virtual Cam before assigning a monitor to a surface.";
        return result;
    }
    const LLPrismLens::RegistrySnapshot snapshot =
        LLPrismLens::registrySnapshot();
    for (U32 index = 0; index < snapshot.mCaptureCount; ++index)
    {
        if (snapshot.mCaptures[index].mHandle.mId == capture_id)
        {
            result.mCapture = snapshot.mCaptures[index].mHandle;
            break;
        }
    }
    if (result.mCapture.mId.isNull())
    {
        result.mAction.mResult = LLPrismLens::ERegistryResult::STALE_HANDLE;
        result.mAction.mReason = "That capture no longer exists.";
        return result;
    }
    if (object)
    {
        for (U32 index = 0; index < snapshot.mDisplayCount; ++index)
        {
            const LLPrismLens::DisplayDefinition& candidate =
                snapshot.mDisplays[index];
            if (!candidate.mSettings.mVirtual &&
                candidate.mDisplayObjectId == object->getID() &&
                candidate.mDisplayTextureEntry == te)
            {
                result.mExistingDisplay = candidate.mHandle;
                result.mReassignment = true;
                result.mAction = reassignDisplayStatus(
                    candidate.mHandle, result.mCapture);
                return result;
            }
        }
    }
    result.mAction = LLPrismLens::displayTargetStatus(
        result.mCapture, object, te, nullptr);
    return result;
}

LLPrismLens::ERegistryResult ALVCamPageModel::dropOnFace(
    const LLUUID& capture_id, LLViewerObject* object, S32 te,
    LLPrismLens::EFitMode fit, bool* reassigned, std::string* reason)
{
    if (reassigned) *reassigned = false;
    const FaceDropStatus status = faceDropStatus(capture_id, object, te);
    if (!status.mAction.allowed())
    {
        if (reason) *reason = status.mAction.mReason;
        return status.mAction.mResult;
    }
    if (status.mReassignment)
    {
        const LLPrismLens::ERegistryResult result = LLPrismLens::reassignDisplay(
            status.mExistingDisplay, status.mCapture, reason);
        if (result == LLPrismLens::ERegistryResult::OK && reassigned)
            *reassigned = true;
        return result;
    }
    LLPrismLens::DisplayHandle display;
    return LLPrismLens::bindDisplayTarget(
        status.mCapture, object, te, fit, &display, reason);
}

LLPrismLens::ERegistryResult ALVCamPageModel::addVirtualScreen(
    const LLPrismLens::CaptureHandle& capture, const LLVector3& pos,
    const LLQuaternion& rot, F32 width, F32 height, std::string* reason)
{
    LLPrismLens::DisplayHandle display;
    const LLPrismLens::ERegistryResult result = LLPrismLens::addVirtualDisplay(
        capture, pos, rot, width, height, &display, reason);
    if (result != LLPrismLens::ERegistryResult::OK) return result;
    return result;
}

bool ALVCamPageModel::setCamera(
    const LLPrismLens::CaptureHandle& capture,
    const LLPrismLens::CameraSettings& settings, std::string* reason)
{
    return LLPrismLens::setCameraSettings(capture, settings, reason);
}

bool ALVCamPageModel::setRate(
    const LLPrismLens::CaptureHandle& capture,
    const LLPrismLens::CaptureRateSettings& settings, std::string* reason)
{
    return LLPrismLens::setCaptureRateSettings(capture, settings, reason);
}

bool ALVCamPageModel::setDisplay(
    const LLPrismLens::DisplayHandle& display,
    const LLPrismLens::DisplaySettings& settings, std::string* reason)
{
    return LLPrismLens::setDisplaySettings(display, settings, reason);
}

bool ALVCamPageModel::setLabel(const LLPrismLens::CaptureHandle& capture,
                               const std::string& label, std::string* reason)
{
    return LLPrismLens::setCaptureLabel(capture, label, reason);
}
