/**
 * @file alvcampagemodel.cpp
 */

#include "llviewerprecompiledheaders.h"

#include "alvcampagemodel.h"

#include "alkithistory.h"
#include "llcallbacklist.h"
#include "llselectmgr.h"
#include "llviewercamera.h"

#include <algorithm>

namespace
{
constexpr const char* VCAM_PAGE = "camera_group/vcam_tab";

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

LLPrismLens::RemovedDisplayRecord displayRecord(
    const LLPrismLens::DisplayDefinition& definition)
{
    LLPrismLens::RemovedDisplayRecord result;
    result.mDefinition = definition;
    return result;
}

bool captureLive(const LLPrismLens::CaptureHandle& handle)
{
    const LLPrismLens::RegistrySnapshot snapshot =
        LLPrismLens::registrySnapshot();
    for (U32 index = 0; index < snapshot.mCaptureCount; ++index)
        if (sameCapture(snapshot.mCaptures[index].mHandle, handle)) return true;
    return false;
}

bool displayLive(const LLPrismLens::DisplayHandle& handle)
{
    const LLPrismLens::RegistrySnapshot snapshot =
        LLPrismLens::registrySnapshot();
    for (U32 index = 0; index < snapshot.mDisplayCount; ++index)
        if (sameDisplay(snapshot.mDisplays[index].mHandle, handle)) return true;
    return false;
}

bool allCapturesLive(const std::vector<LLPrismLens::RemovedCaptureRecord>& records)
{
    for (const LLPrismLens::RemovedCaptureRecord& record : records)
        if (!captureLive(record.mCapture.mHandle)) return false;
    return true;
}

bool restoreCaptureSet(std::vector<LLPrismLens::RemovedCaptureRecord> records,
                       std::string& reason)
{
    std::sort(records.begin(), records.end(),
        [](const LLPrismLens::RemovedCaptureRecord& left,
           const LLPrismLens::RemovedCaptureRecord& right)
        {
            if (left.mHadGateArm != right.mHadGateArm)
                return left.mHadGateArm;
            return left.mGateArmIndex < right.mGateArmIndex;
        });
    std::vector<LLPrismLens::CaptureHandle> restored;
    for (const LLPrismLens::RemovedCaptureRecord& record : records)
    {
        if (LLPrismLens::restoreCaptureIdentity(record, &reason))
        {
            restored.push_back(record.mCapture.mHandle);
            continue;
        }
        for (auto it = restored.rbegin(); it != restored.rend(); ++it)
            LLPrismLens::removeCapture(*it);
        return false;
    }
    return true;
}

bool swapCaptureSets(const std::vector<LLPrismLens::RemovedCaptureRecord>& expected,
                     const std::vector<LLPrismLens::RemovedCaptureRecord>& desired,
                     std::string& reason)
{
    if (!allCapturesLive(expected))
    {
        reason = "the camera set changed";
        return false;
    }
    for (const LLPrismLens::RemovedCaptureRecord& record : desired)
    {
        if (captureLive(record.mCapture.mHandle))
        {
            reason = "a reserved camera identity is already live";
            return false;
        }
    }
    for (auto it = expected.rbegin(); it != expected.rend(); ++it)
    {
        if (!LLPrismLens::removeCapture(it->mCapture.mHandle))
        {
            reason = "the camera set changed during removal";
            return false;
        }
    }
    if (restoreCaptureSet(desired, reason)) return true;

    std::string rollback_reason;
    if (!restoreCaptureSet(expected, rollback_reason))
        reason += "; rollback failed: " + rollback_reason;
    return false;
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

ALVCamPageModel::ALVCamPageModel(ALKitHistory& history)
    : mHistory(history), mAlive(std::make_shared<bool>(true))
{
    mConfigurationConnection = LLPrismLens::connectConfigurationChanged(
        [this](U64) { markDirty(DIRTY_CONFIGURATION); });
    mRuntimeConnection = LLPrismLens::connectRuntimeChanged(
        [this](U64) { markDirty(DIRTY_RUNTIME); });
    mPerformanceConnection = LLPrismLens::connectPerformanceChanged(
        [this](U64) { markDirty(DIRTY_PERFORMANCE); });
    mSelectionConnection = LLSelectMgr::getInstance()->mUpdateSignal.connect(
        [this]() { markDirty(DIRTY_SELECTION); });
    refreshNow();
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

void ALVCamPageModel::onIdleRefresh(const std::weak_ptr<bool>& alive)
{
    const std::shared_ptr<bool> token = alive.lock();
    if (!token || !*token) return;
    mIdleQueued = false;
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

void ALVCamPageModel::recordAddedCapture(
    const std::string& label, const LLPrismLens::RemovedCaptureRecord& record)
{
    const std::weak_ptr<bool> alive(mAlive);
    const LLPrismLens::CaptureHandle handle = record.mCapture.mHandle;
    mHistory.beginGesture(ALKitHistory::MOUSE, VCAM_PAGE, label);
    mHistory.recordCustom("vcam.capture", label,
        [handle](bool redo, std::string& reason)
        {
            const bool live = captureLive(handle);
            if ((!redo && live) || (redo && !live)) return true;
            reason = redo ? "the capture identity is already live"
                          : "the capture no longer exists";
            return false;
        },
        [this, alive, record](bool redo, std::string& reason)
        {
            const bool ok = redo
                ? LLPrismLens::restoreCaptureIdentity(record, &reason)
                : LLPrismLens::removeCapture(record.mCapture.mHandle);
            const std::shared_ptr<bool> token = alive.lock();
            if (ok && token && *token)
            {
                mSelectedCapture = redo ? record.mCapture.mHandle
                                        : LLPrismLens::CaptureHandle();
                markDirty(DIRTY_CONFIGURATION | DIRTY_RUNTIME | DIRTY_SELECTION);
            }
            if (!ok && reason.empty()) reason = "the registry changed";
            return ok;
        }, {});
    mHistory.commitGesture();
}

void ALVCamPageModel::recordAddedDisplay(
    const std::string& label, const LLPrismLens::RemovedDisplayRecord& record)
{
    const LLPrismLens::DisplayHandle handle = record.mDefinition.mHandle;
    mHistory.beginGesture(ALKitHistory::MOUSE, VCAM_PAGE, label);
    mHistory.recordCustom("vcam.display", label,
        [handle](bool redo, std::string& reason)
        {
            const bool live = displayLive(handle);
            if ((!redo && live) || (redo && !live)) return true;
            reason = redo ? "the display identity is already live"
                          : "the display no longer exists";
            return false;
        },
        [record](bool redo, std::string& reason)
        {
            const bool ok = redo
                ? LLPrismLens::restoreDisplayIdentity(record, &reason)
                : LLPrismLens::removeDisplay(record.mDefinition.mHandle, &reason);
            return ok;
        }, {});
    mHistory.commitGesture();
}

LLPrismLens::ERegistryResult ALVCamPageModel::addFromSelectedObject(
    std::string* reason)
{
    LLPrismLens::CaptureHandle capture;
    const LLPrismLens::ERegistryResult result =
        LLPrismLens::addCameraCaptureFromSelectedObject(&capture, reason);
    if (result != LLPrismLens::ERegistryResult::OK) return result;
    LLPrismLens::RemovedCaptureRecord record;
    LLPrismLens::captureRemovalRecord(capture, record, nullptr);
    recordAddedCapture("Add camera", record);
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
    LLPrismLens::RemovedCaptureRecord record;
    LLPrismLens::captureRemovalRecord(capture, record, nullptr);
    recordAddedCapture("Add virtual camera", record);
    selectAfterAdd(capture);
    return result;
}

bool ALVCamPageModel::addOtsPair(std::string* reason)
{
    std::vector<LLPrismLens::RemovedCaptureRecord> old_records;
    const LLPrismLens::RegistrySnapshot before_registry =
        LLPrismLens::registrySnapshot();
    const LLPrismLens::GateSnapshot before_gate = LLPrismLens::gateSnapshot();
    for (const LLPrismLens::GateArmedCamera& arm : before_gate.mSettings.mArmed)
    {
        if (!LLPrismLens::isOtsPairArmLabel(arm.mLabel)) continue;
        for (U32 index = 0; index < before_registry.mCaptureCount; ++index)
        {
            const LLPrismLens::CaptureDefinition& capture =
                before_registry.mCaptures[index];
            if (capture.mHandle.mId != arm.mCaptureId) continue;
            LLPrismLens::RemovedCaptureRecord record;
            if (LLPrismLens::captureRemovalRecord(
                    capture.mHandle, record, reason))
                old_records.push_back(record);
            break;
        }
    }
    LLPrismLens::CaptureHandle first;
    LLPrismLens::CaptureHandle second;
    if (!LLPrismLens::buildOtsPairFromSubjects(&first, &second, reason))
        return false;
    LLPrismLens::RemovedCaptureRecord first_record;
    LLPrismLens::RemovedCaptureRecord second_record;
    LLPrismLens::captureRemovalRecord(first, first_record, nullptr);
    LLPrismLens::captureRemovalRecord(second, second_record, nullptr);
    const std::vector<LLPrismLens::RemovedCaptureRecord> new_records =
        { first_record, second_record };
    mHistory.beginGesture(ALKitHistory::MOUSE, VCAM_PAGE, "Add OTS pair");
    mHistory.recordCustom("vcam.ots_pair", "Add OTS pair",
        [old_records, new_records](bool redo, std::string& failure)
        {
            const auto& expected = redo ? old_records : new_records;
            const auto& desired = redo ? new_records : old_records;
            if (!allCapturesLive(expected))
            {
                failure = "the OTS pair changed";
                return false;
            }
            for (const LLPrismLens::RemovedCaptureRecord& record : desired)
                if (captureLive(record.mCapture.mHandle))
                {
                    failure = "a reserved OTS identity is already live";
                    return false;
                }
            return true;
        },
        [old_records, new_records](bool redo, std::string& failure)
        {
            return redo
                ? swapCaptureSets(old_records, new_records, failure)
                : swapCaptureSets(new_records, old_records, failure);
        }, {});
    mHistory.commitGesture();
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
    LLPrismLens::RemovedCaptureRecord record;
    LLPrismLens::captureRemovalRecord(capture, record, nullptr);
    recordAddedCapture("Add surface lens", record);
    selectAfterAdd(capture);
    return result;
}

bool ALVCamPageModel::removeCapture(
    const LLPrismLens::CaptureHandle& capture, std::string* reason)
{
    LLPrismLens::RemovedCaptureRecord record;
    if (!LLPrismLens::captureRemovalRecord(capture, record, reason) ||
        !LLPrismLens::removeCapture(capture))
    {
        if (reason && reason->empty()) *reason = "That capture changed before removal.";
        return false;
    }
    const std::weak_ptr<bool> alive(mAlive);
    mHistory.beginGesture(ALKitHistory::MOUSE, VCAM_PAGE, "Remove camera");
    mHistory.recordCustom("vcam.remove_capture", "Remove camera",
        [capture](bool redo, std::string& failure)
        {
            const bool live = captureLive(capture);
            if ((!redo && !live) || (redo && live)) return true;
            failure = redo ? "the restored capture no longer exists"
                           : "the removed identity is already live";
            return false;
        },
        [this, alive, record](bool redo, std::string& failure)
        {
            const bool ok = redo
                ? LLPrismLens::removeCapture(record.mCapture.mHandle)
                : LLPrismLens::restoreCaptureIdentity(record, &failure);
            const std::shared_ptr<bool> token = alive.lock();
            if (ok && token && *token)
            {
                mSelectedCapture = redo ? LLPrismLens::CaptureHandle()
                                        : record.mCapture.mHandle;
                markDirty(DIRTY_CONFIGURATION | DIRTY_RUNTIME | DIRTY_SELECTION);
            }
            if (!ok && failure.empty()) failure = "the registry changed";
            return ok;
        }, {});
    mHistory.commitGesture();
    mSelectedCapture = LLPrismLens::CaptureHandle();
    mSelectedDisplay = LLPrismLens::DisplayHandle();
    refreshNow(DIRTY_CONFIGURATION | DIRTY_RUNTIME | DIRTY_SELECTION);
    const LLPrismLens::CaptureDefinition* next = nullptr;
    for (U32 index = 0; index < mRegistry.mCaptureCount; ++index)
    {
        const LLPrismLens::CaptureDefinition& candidate = mRegistry.mCaptures[index];
        const bool next_after = next &&
            next->mSlot >= record.mCapture.mSlot;
        const bool candidate_after =
            candidate.mSlot >= record.mCapture.mSlot;
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
    refreshNow(DIRTY_CONFIGURATION);
    const LLPrismLens::DisplayDefinition* definition = findDisplay(display);
    if (definition) recordAddedDisplay("Bind display face", displayRecord(*definition));
    return result;
}

LLPrismLens::ERegistryResult ALVCamPageModel::bindFaceTarget(
    const LLPrismLens::CaptureHandle& capture, LLViewerObject* object, S32 te,
    LLPrismLens::EFitMode fit, std::string* reason)
{
    LLPrismLens::DisplayHandle display;
    const LLPrismLens::ERegistryResult result = LLPrismLens::bindDisplayTarget(
        capture, object, te, fit, &display, reason);
    if (result != LLPrismLens::ERegistryResult::OK) return result;
    refreshNow(DIRTY_CONFIGURATION);
    const LLPrismLens::DisplayDefinition* definition = findDisplay(display);
    if (definition) recordAddedDisplay("Bind display face", displayRecord(*definition));
    return result;
}

LLPrismLens::ERegistryResult ALVCamPageModel::reassignDisplay(
    const LLPrismLens::DisplayHandle& display,
    const LLPrismLens::CaptureHandle& capture, std::string* reason)
{
    refreshNow(DIRTY_CONFIGURATION);
    const LLPrismLens::DisplayDefinition* before = findDisplay(display);
    if (!before)
    {
        if (reason) *reason = "That display handle is stale.";
        return LLPrismLens::ERegistryResult::STALE_HANDLE;
    }
    const LLPrismLens::CaptureHandle old_capture = before->mCapture;
    const LLPrismLens::ERegistryResult result =
        LLPrismLens::reassignDisplay(display, capture, reason);
    if (result != LLPrismLens::ERegistryResult::OK) return result;
    mHistory.beginGesture(ALKitHistory::MOUSE, VCAM_PAGE, "Reassign display");
    mHistory.recordCustom("vcam.reassign", "Reassign display",
        [display](bool, std::string& failure)
        {
            if (displayLive(display)) return true;
            failure = "the display no longer exists";
            return false;
        },
        [display, old_capture, capture](bool redo, std::string& failure)
        {
            return LLPrismLens::reassignDisplay(
                       display, redo ? capture : old_capture, &failure) ==
                   LLPrismLens::ERegistryResult::OK;
        }, {});
    mHistory.commitGesture();
    return result;
}

LLPrismLens::ERegistryResult ALVCamPageModel::addVirtualScreen(
    const LLPrismLens::CaptureHandle& capture, const LLVector3& pos,
    const LLQuaternion& rot, F32 width, F32 height, std::string* reason)
{
    LLPrismLens::DisplayHandle display;
    const LLPrismLens::ERegistryResult result = LLPrismLens::addVirtualDisplay(
        capture, pos, rot, width, height, &display, reason);
    if (result != LLPrismLens::ERegistryResult::OK) return result;
    refreshNow(DIRTY_CONFIGURATION);
    const LLPrismLens::DisplayDefinition* definition = findDisplay(display);
    if (definition) recordAddedDisplay("Add virtual screen", displayRecord(*definition));
    return result;
}

bool ALVCamPageModel::setCamera(
    const LLPrismLens::CaptureHandle& capture,
    const LLPrismLens::CameraSettings& settings, std::string* reason)
{
    refreshNow(DIRTY_CONFIGURATION);
    const LLPrismLens::CaptureDefinition* before = findCapture(capture);
    if (!before) return false;
    const LLPrismLens::CameraSettings old_value = before->mCamera;
    if (!LLPrismLens::setCameraSettings(capture, settings, reason)) return false;
    mHistory.beginGesture(ALKitHistory::KEY, VCAM_PAGE, "Edit camera");
    mHistory.recordCustom("vcam.camera", "Edit camera",
        [capture, old_value, settings](bool redo, std::string& failure)
        { return LLPrismLens::setCameraSettings(capture, redo ? settings : old_value, &failure); });
    return true;
}

bool ALVCamPageModel::setRate(
    const LLPrismLens::CaptureHandle& capture,
    const LLPrismLens::CaptureRateSettings& settings, std::string* reason)
{
    refreshNow(DIRTY_CONFIGURATION);
    const LLPrismLens::CaptureDefinition* before = findCapture(capture);
    if (!before) return false;
    const LLPrismLens::CaptureRateSettings old_value = before->mRate;
    if (!LLPrismLens::setCaptureRateSettings(capture, settings, reason)) return false;
    mHistory.beginGesture(ALKitHistory::KEY, VCAM_PAGE, "Edit picture rate");
    mHistory.recordCustom("vcam.rate", "Edit picture rate",
        [capture, old_value, settings](bool redo, std::string& failure)
        { return LLPrismLens::setCaptureRateSettings(capture, redo ? settings : old_value, &failure); });
    return true;
}

bool ALVCamPageModel::setDisplay(
    const LLPrismLens::DisplayHandle& display,
    const LLPrismLens::DisplaySettings& settings, std::string* reason)
{
    refreshNow(DIRTY_CONFIGURATION);
    const LLPrismLens::DisplayDefinition* before = findDisplay(display);
    if (!before) return false;
    const LLPrismLens::DisplaySettings old_value = before->mSettings;
    if (!LLPrismLens::setDisplaySettings(display, settings, reason)) return false;
    mHistory.beginGesture(ALKitHistory::KEY, VCAM_PAGE, "Edit display");
    mHistory.recordCustom("vcam.display_settings", "Edit display",
        [display, old_value, settings](bool redo, std::string& failure)
        { return LLPrismLens::setDisplaySettings(display, redo ? settings : old_value, &failure); });
    return true;
}

bool ALVCamPageModel::setLabel(const LLPrismLens::CaptureHandle& capture,
                               const std::string& label, std::string* reason)
{
    refreshNow(DIRTY_CONFIGURATION);
    const LLPrismLens::CaptureDefinition* before = findCapture(capture);
    if (!before) return false;
    const std::string old_value = before->mLabel;
    if (!LLPrismLens::setCaptureLabel(capture, label, reason)) return false;
    refreshNow(DIRTY_CONFIGURATION);
    const LLPrismLens::CaptureDefinition* after = findCapture(capture);
    const std::string new_value = after ? after->mLabel : label;
    mHistory.beginGesture(ALKitHistory::KEY, VCAM_PAGE, "Rename camera");
    mHistory.recordCustom("vcam.label", "Rename camera",
        [capture, old_value, new_value](bool redo, std::string& failure)
        { return LLPrismLens::setCaptureLabel(capture, redo ? new_value : old_value, &failure); });
    return true;
}
