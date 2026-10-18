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

bool sameProjector(const LLPrismLens::ProjectorHandle& left,
                   const LLPrismLens::ProjectorHandle& right)
{
    return left.mId == right.mId && left.mGeneration == right.mGeneration;
}

bool sameLook(const LLPrismLens::ProjectorLook& left,
              const LLPrismLens::ProjectorLook& right)
{
    return left.mKeystoneH == right.mKeystoneH &&
           left.mKeystoneV == right.mKeystoneV &&
           left.mShutterFlicker == right.mShutterFlicker &&
           left.mShutterHz == right.mShutterHz &&
           left.mGateWeave == right.mGateWeave &&
           left.mHotspot == right.mHotspot &&
           left.mLampKelvin == right.mLampKelvin &&
           left.mGelTint == right.mGelTint &&
           left.mDust == right.mDust &&
           left.mScratches == right.mScratches &&
           left.mBurn == right.mBurn &&
           left.mGrain == right.mGrain &&
           left.mHalation == right.mHalation &&
           left.mThrowMode == right.mThrowMode;
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
    doOnIdleRepeating([this, alive]()
    {
        const std::shared_ptr<bool> token = alive.lock();
        if (!token || !*token) return true;
        return onIdleRefresh(alive);
    });
}

void ALVCamPageModel::setPageVisible(bool visible)
{
    if (mPageVisible == visible) return;
    mPageVisible = visible;
    if (visible)
        refreshNow(mDirty ? mDirty : DIRTY_ALL);
}

bool ALVCamPageModel::onIdleRefresh(const std::weak_ptr<bool>& alive)
{
    const std::shared_ptr<bool> token = alive.lock();
    if (!token || !*token) return true;
    if (!mPageVisible || !mDirty) { mIdleQueued = false; return true; }
    const U32 volatile_dirty = DIRTY_RUNTIME | DIRTY_PERFORMANCE | DIRTY_PREVIEW;
    if (!(mDirty & ~volatile_dirty) && LLTimer::getTotalSeconds() < mNextRuntimeRefresh)
        return false; // same registered callback, once per idle; no requeue/spin
    mIdleQueued = false;
    refreshNow(mDirty);
    return true;
}

void ALVCamPageModel::refreshNow(U32 dirty)
{
    mNextRuntimeRefresh = LLTimer::getTotalSeconds() + .25;
    LLPrismLens::ProjectorHandle added_projector;
    if (dirty & (DIRTY_CONFIGURATION | DIRTY_RUNTIME))
    {
        const LLPrismLens::RegistrySnapshot fresh = LLPrismLens::registrySnapshot();
        if (dirty & DIRTY_CONFIGURATION)
            for (U32 index = 0; index < fresh.mProjectorCount; ++index)
            {
                bool known = false;
                for (U32 old_index = 0; old_index < mRegistry.mProjectorCount;
                     ++old_index)
                    if (sameProjector(fresh.mProjectors[index].mHandle,
                                      mRegistry.mProjectors[old_index].mHandle))
                    {
                        known = true;
                        break;
                    }
                if (!known) added_projector = fresh.mProjectors[index].mHandle;
            }
        mRegistry = fresh;
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
        // A Gate-subscribed face follows the Director switcher and belongs to
        // no one camera; it stays selectable under every Camera Feed (not
        // under a Surface Lens, which owns exactly one face).
        const LLPrismLens::CaptureDefinition* selected_capture =
            mSelectedCapture.mId.notNull() ? findCapture(mSelectedCapture) : nullptr;
        const bool gate_listed = display && display->mGateSubscribed &&
            (!selected_capture ||
             selected_capture->isCameraFeed());
        if (!display || (mSelectedCapture.mId.notNull() && !gate_listed &&
                         !sameCapture(display->mCapture, mSelectedCapture)))
            mSelectedDisplay = LLPrismLens::DisplayHandle();
    }
    if (mSelectedProjector.mId.notNull())
    {
        const LLPrismLens::ProjectorDefinition* projector =
            findProjector(mSelectedProjector);
        if (!projector || (mSelectedCapture.mId.notNull() &&
                           findCapture(projector->mHomeCapture) &&
                           !sameCapture(projector->mHomeCapture, mSelectedCapture)))
            mSelectedProjector = LLPrismLens::ProjectorHandle();
    }
    if (added_projector.mId.notNull())
    {
        const LLPrismLens::ProjectorDefinition* added = findProjector(added_projector);
        if (added && mSelectedCapture.mId.notNull() &&
            sameCapture(added->mHomeCapture, mSelectedCapture))
            mSelectedProjector = added_projector;
    }
    if (mSelectedProjector.mId.isNull() && mSelectedCapture.mId.notNull())
    {
        U32 projector_count = 0;
        LLPrismLens::ProjectorHandle only_projector;
        for (U32 index = 0; index < mRegistry.mProjectorCount; ++index)
            if (sameCapture(mRegistry.mProjectors[index].mCapture,
                            mSelectedCapture))
            {
                only_projector = mRegistry.mProjectors[index].mHandle;
                ++projector_count;
            }
        if (projector_count == 1) mSelectedProjector = only_projector;
    }
    for (U32 slot = 0; slot < LLPrismLens::MAX_CAPTURES; ++slot)
    {
        if (mMonitorDisplay[slot].mId.isNull()) continue;
        const LLPrismLens::DisplayDefinition* display =
            findDisplay(mMonitorDisplay[slot]);
        const LLPrismLens::CaptureDefinition* capture = nullptr;
        for (U32 index = 0; index < mRegistry.mSlotCount; ++index)
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
    for (U32 index = 0; index < mRegistry.mSlotCount; ++index)
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

const LLPrismLens::ProjectorDefinition* ALVCamPageModel::findProjector(
    const LLPrismLens::ProjectorHandle& handle) const
{
    for (U32 index = 0; index < mRegistry.mProjectorCount; ++index)
        if (sameProjector(mRegistry.mProjectors[index].mHandle, handle))
            return &mRegistry.mProjectors[index];
    return nullptr;
}

void ALVCamPageModel::setSelectedCapture(
    const LLPrismLens::CaptureHandle& capture)
{
    if (sameCapture(mSelectedCapture, capture)) return;
    mSelectedCapture = capture;
    mSelectedDisplay = LLPrismLens::DisplayHandle();
    mSelectedProjector = LLPrismLens::ProjectorHandle();
    U32 projector_count = 0;
    LLPrismLens::ProjectorHandle only_projector;
    for (U32 index = 0; index < mRegistry.mProjectorCount; ++index)
        if (sameCapture(mRegistry.mProjectors[index].mCapture, capture))
        {
            only_projector = mRegistry.mProjectors[index].mHandle;
            ++projector_count;
        }
    if (projector_count == 1) mSelectedProjector = only_projector;
    markDirty(DIRTY_SELECTION);
}

void ALVCamPageModel::setSelectedProjector(
    const LLPrismLens::ProjectorHandle& projector)
{
    if (sameProjector(mSelectedProjector, projector)) return;
    mSelectedProjector = projector;
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
    mSelectedProjector = LLPrismLens::ProjectorHandle();
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

LLPrismLens::ERegistryResult ALVCamPageModel::addMediaSlot(std::string* reason)
{
    LLPrismLens::CaptureHandle handle;
    const auto result = LLPrismLens::addMediaSlot(&handle, reason);
    if (result == LLPrismLens::ERegistryResult::OK) setSelectedCapture(handle);
    refreshNow();
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
    mSelectedProjector = LLPrismLens::ProjectorHandle();
    refreshNow(DIRTY_CONFIGURATION | DIRTY_RUNTIME | DIRTY_SELECTION);
    const LLPrismLens::CaptureDefinition* next = nullptr;
    for (U32 index = 0; index < mRegistry.mSlotCount; ++index)
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
    for (U32 index = 0; index < snapshot.mSlotCount; ++index)
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
    if (!LLPrismLens::canRouteSource(*destination_capture))
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
    for (U32 index = 0; index < snapshot.mSlotCount; ++index)
    {
        const LLPrismLens::CaptureDefinition& candidate =
            snapshot.mCaptures[index];
        if (sameCapture(candidate.mHandle, display_definition->mCapture))
        {
            old_capture = &candidate;
            break;
        }
    }
    // An orphaned display (its capture was removed) may be re-pointed at any
    // valid capture; the registry re-attaches it.
    if (old_capture && old_capture->isSurfaceLens())
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
    for (U32 index = 0; index < snapshot.mSlotCount; ++index)
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

LLPrismLens::ERegistryResult ALVCamPageModel::applyToSelectedFaces(
    const LLUUID& capture_id, LLPrismLens::EFitMode fit, std::string* reason)
{
    struct Target { LLViewerObject* mObject; S32 mTE; };
    struct Collector final : LLSelectedTEFunctor
    {
        std::vector<Target>& mTargets;
        explicit Collector(std::vector<Target>& targets) : mTargets(targets) {}
        bool apply(LLViewerObject* object, S32 te) override
        {
            if (object) mTargets.push_back({object, te});
            return true;
        }
    };
    std::vector<Target> targets;
    Collector collector(targets);
    LLSelectMgr::getInstance()->getSelection()->applyToTEs(&collector);
    if (targets.empty())
    {
        if (reason) *reason = "Select at least one object face.";
        return LLPrismLens::ERegistryResult::INVALID_SELECTION;
    }

    const LLPrismLens::RegistrySnapshot snapshot = LLPrismLens::registrySnapshot();
    U32 additions = 0;
    for (const Target& target : targets)
    {
        bool already_bound = false;
        for (U32 index = 0; index < snapshot.mDisplayCount; ++index)
        {
            const LLPrismLens::DisplayDefinition& display = snapshot.mDisplays[index];
            if (!display.mSettings.mVirtual &&
                display.mDisplayObjectId == target.mObject->getID() &&
                display.mDisplayTextureEntry == target.mTE &&
                display.mCapture.mId == capture_id)
            {
                already_bound = true;
                break;
            }
        }
        if (already_bound) continue;
        const FaceDropStatus status = faceDropStatus(capture_id, target.mObject, target.mTE);
        if (!status.mAction.allowed())
        {
            if (reason) *reason = status.mAction.mReason;
            return status.mAction.mResult;
        }
        if (!status.mReassignment) ++additions;
    }
    if (snapshot.mDisplayCount + additions > LLPrismLens::MAX_DISPLAY_BINDINGS)
    {
        if (reason) *reason = llformat(
            "This selection needs %u new face bindings; VCam supports 16 total.", additions);
        return LLPrismLens::ERegistryResult::AT_CAPACITY;
    }
    LLPrismLens::ScopedRevisionSignals signals;
    for (const Target& target : targets)
    {
        bool already_bound = false;
        for (U32 index = 0; index < snapshot.mDisplayCount; ++index)
        {
            const LLPrismLens::DisplayDefinition& display = snapshot.mDisplays[index];
            if (!display.mSettings.mVirtual &&
                display.mDisplayObjectId == target.mObject->getID() &&
                display.mDisplayTextureEntry == target.mTE &&
                display.mCapture.mId == capture_id)
            {
                already_bound = true;
                break;
            }
        }
        if (already_bound) continue;
        const LLPrismLens::ERegistryResult result =
            dropOnFace(capture_id, target.mObject, target.mTE, fit, nullptr, reason);
        if (result != LLPrismLens::ERegistryResult::OK) return result;
    }
    return LLPrismLens::ERegistryResult::OK;
}

bool ALVCamPageModel::clearSelectedFaces(std::string* reason)
{
    struct Target { LLUUID mObject; S32 mTE; };
    struct Collector final : LLSelectedTEFunctor
    {
        std::vector<Target>& mTargets;
        explicit Collector(std::vector<Target>& targets) : mTargets(targets) {}
        bool apply(LLViewerObject* object, S32 te) override
        {
            if (object) mTargets.push_back({object->getID(), te});
            return true;
        }
    };
    std::vector<Target> targets;
    Collector collector(targets);
    LLSelectMgr::getInstance()->getSelection()->applyToTEs(&collector);
    const LLPrismLens::RegistrySnapshot snapshot = LLPrismLens::registrySnapshot();
    bool removed = false;
    LLPrismLens::ScopedRevisionSignals signals;
    for (U32 index = 0; index < snapshot.mDisplayCount; ++index)
    {
        const LLPrismLens::DisplayDefinition& display = snapshot.mDisplays[index];
        for (const Target& target : targets)
        {
            if (!display.mSettings.mVirtual && display.mDisplayObjectId == target.mObject &&
                display.mDisplayTextureEntry == target.mTE)
            {
                removed = LLPrismLens::removeDisplay(display.mHandle, reason) || removed;
                break;
            }
        }
    }
    if (!removed && reason) *reason = "The selected faces have no VCam feed.";
    return removed;
}

ALVCamPageModel::ProjectorDropStatus ALVCamPageModel::projectorDropStatus(
    const LLUUID& capture_id, LLViewerObject* object)
{
    ProjectorDropStatus result;
    const LLPrismLens::RegistrySnapshot snapshot = LLPrismLens::registrySnapshot();
    for (U32 index = 0; index < snapshot.mSlotCount; ++index)
        if (snapshot.mCaptures[index].mHandle.mId == capture_id)
            result.mCapture = snapshot.mCaptures[index].mHandle;
    if (result.mCapture.mId.isNull())
    {
        result.mAction.mResult = LLPrismLens::ERegistryResult::STALE_HANDLE;
        result.mAction.mReason = "That capture no longer exists.";
        return result;
    }
    if (object)
        for (U32 index = 0; index < snapshot.mProjectorCount; ++index)
        {
            const LLPrismLens::ProjectorDefinition& binding = snapshot.mProjectors[index];
            if (binding.mTarget == LLPrismLens::EProjectorTarget::OBJECT &&
                binding.mObjectId == object->getID())
            {
                result.mExistingProjector = binding.mHandle;
                result.mReassignment = true;
                if (binding.mCapture.mId == capture_id)
                {
                    result.mAlreadyBound = true;
                    result.mAction.mResult = LLPrismLens::ERegistryResult::OK;
                    return result;
                }
                result.mAction = LLPrismLens::projectorTargetStatus(
                    result.mCapture, object, &binding.mHandle);
                return result;
            }
        }
    result.mAction = LLPrismLens::projectorTargetStatus(result.mCapture, object);
    return result;
}

LLPrismLens::ERegistryResult ALVCamPageModel::dropOnProjector(
    const LLUUID& capture_id, LLViewerObject* object, bool* reassigned,
    std::string* reason)
{
    if (reassigned) *reassigned = false;
    const ProjectorDropStatus status = projectorDropStatus(capture_id, object);
    if (!status.mAction.allowed())
    {
        if (reason) *reason = status.mAction.mReason;
        return status.mAction.mResult;
    }
    if (status.mAlreadyBound)
    {
        if (reassigned) *reassigned = true;
        return LLPrismLens::ERegistryResult::OK;
    }
    if (status.mReassignment)
    {
        const LLPrismLens::ERegistryResult result = LLPrismLens::reassignProjector(
            status.mExistingProjector, status.mCapture, reason);
        if (result == LLPrismLens::ERegistryResult::OK && reassigned) *reassigned = true;
        return result;
    }
    LLPrismLens::ProjectorHandle binding;
    return LLPrismLens::bindProjector(status.mCapture, object,
                                      LLPrismLens::ProjectorSettings(),
                                      &binding, reason);
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

LLPrismLens::ActionStatus ALVCamPageModel::projectorStatus(
    const LLPrismLens::CaptureHandle& capture, LLViewerObject* object,
    const LLPrismLens::ProjectorHandle* reassigning) const
{
    return LLPrismLens::projectorTargetStatus(
        capture, object, reassigning);
}

LLPrismLens::ERegistryResult ALVCamPageModel::bindProjector(
    const LLPrismLens::CaptureHandle& capture, LLViewerObject* object,
    const LLPrismLens::ProjectorSettings& settings,
    LLPrismLens::ProjectorHandle* binding, std::string* reason)
{
    return LLPrismLens::bindProjector(
        capture, object, settings, binding, reason);
}

LLPrismLens::ERegistryResult ALVCamPageModel::reassignProjector(
    const LLPrismLens::ProjectorHandle& binding,
    const LLPrismLens::CaptureHandle& capture, std::string* reason)
{
    return LLPrismLens::reassignProjector(binding, capture, reason);
}

bool ALVCamPageModel::setProjectorSettings(
    const LLPrismLens::ProjectorHandle& binding,
    const LLPrismLens::ProjectorSettings& settings, std::string* reason)
{
    return LLPrismLens::setProjectorSettings(binding, settings, reason);
}

const std::array<ALVCamPageModel::ProjectorLookPreset,
    static_cast<size_t>(ALVCamPageModel::EProjectorLookPreset::COUNT)>&
ALVCamPageModel::projectorLookPresets()
{
    static const std::array<ProjectorLookPreset,
        static_cast<size_t>(EProjectorLookPreset::COUNT)> presets = []
    {
        std::array<ProjectorLookPreset,
            static_cast<size_t>(EProjectorLookPreset::COUNT)> result{};
        result[0] = { "Clean digital", LLPrismLens::ProjectorLook() };

        result[1] = { "Cinema xenon", LLPrismLens::ProjectorLook() };
        result[1].mLook.mLampKelvin = 6000.f;
        result[1].mLook.mHotspot = .25f;
        result[1].mLook.mShutterFlicker = .05f;
        result[1].mLook.mHalation = .15f;
        result[1].mLook.mGrain = .05f;

        result[2] = { "16mm home movie", LLPrismLens::ProjectorLook() };
        result[2].mLook.mLampKelvin = 3400.f;
        result[2].mLook.mHotspot = .45f;
        result[2].mLook.mShutterFlicker = .35f;
        result[2].mLook.mShutterHz = 18.f;
        result[2].mLook.mGateWeave = .35f;
        result[2].mLook.mDust = .3f;
        result[2].mLook.mScratches = .2f;
        result[2].mLook.mGrain = .35f;
        result[2].mLook.mHalation = .25f;

        result[3] = { "Old slide projector", LLPrismLens::ProjectorLook() };
        result[3].mLook.mLampKelvin = 3000.f;
        result[3].mLook.mHotspot = .6f;
        result[3].mLook.mKeystoneV = .15f;
        result[3].mLook.mDust = .15f;
        result[3].mLook.mBurn = .2f;
        result[3].mLook.mHalation = .2f;

        result[4] = { "Worn film reel", LLPrismLens::ProjectorLook() };
        result[4].mLook.mLampKelvin = 3800.f;
        result[4].mLook.mShutterFlicker = .25f;
        result[4].mLook.mGateWeave = .25f;
        result[4].mLook.mDust = .5f;
        result[4].mLook.mScratches = .5f;
        result[4].mLook.mBurn = .35f;
        result[4].mLook.mGrain = .45f;
        return result;
    }();
    return presets;
}

ALVCamPageModel::EProjectorLookPreset ALVCamPageModel::projectorLookPreset(
    const LLPrismLens::ProjectorLook& look)
{
    const auto& presets = projectorLookPresets();
    for (size_t index = 0; index < presets.size(); ++index)
        if (sameLook(look, presets[index].mLook))
            return static_cast<EProjectorLookPreset>(index);
    return EProjectorLookPreset::CUSTOM;
}

bool ALVCamPageModel::setProjectorLook(
    const LLPrismLens::ProjectorHandle& binding,
    EProjectorLookPreset preset, std::string* reason)
{
    const S32 index = static_cast<S32>(preset);
    const auto& presets = projectorLookPresets();
    const LLPrismLens::ProjectorDefinition* definition = findProjector(binding);
    if (!definition || index < 0 || index >= static_cast<S32>(presets.size()))
    {
        if (reason) *reason = definition
            ? "Choose a projector look preset." : "That projector no longer exists.";
        return false;
    }
    LLPrismLens::ProjectorSettings settings = definition->mSettings;
    settings.mLook = presets[index].mLook;
    return setProjectorSettings(binding, settings, reason);
}

LLPrismLens::ERegistryResult ALVCamPageModel::setProjectorGateSubscribed(
    const LLPrismLens::ProjectorHandle& binding, bool subscribed,
    std::string* reason)
{
    return LLPrismLens::setProjectorGateSubscribed(
        binding, subscribed, reason);
}

bool ALVCamPageModel::removeProjector(
    const LLPrismLens::ProjectorHandle& binding, std::string* reason)
{
    const bool removed = LLPrismLens::removeProjector(binding, reason);
    if (removed && sameProjector(mSelectedProjector, binding))
        mSelectedProjector = LLPrismLens::ProjectorHandle();
    return removed;
}

bool ALVCamPageModel::setLabel(const LLPrismLens::CaptureHandle& capture,
                               const std::string& label, std::string* reason)
{
    return LLPrismLens::setCaptureLabel(capture, label, reason);
}
