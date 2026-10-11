/**
 * @file alvcammonitorstrip.cpp
 */

#include "llviewerprecompiledheaders.h"

#include "alvcammonitorstrip.h"

#include "lltooldraganddrop.h"

namespace
{
LLPrismLens::EFitMode sActiveDragFit = LLPrismLens::EFitMode::FIT;

bool sameCapture(const LLPrismLens::CaptureHandle& left,
                 const LLPrismLens::CaptureHandle& right)
{
    return left.mId == right.mId && left.mGeneration == right.mGeneration;
}

bool currentCapture(const LLUUID& id, LLPrismLens::CaptureHandle& capture,
                    LLPrismLens::ECaptureMode* mode = nullptr)
{
    const LLPrismLens::RegistrySnapshot snapshot =
        LLPrismLens::registrySnapshot();
    for (U32 index = 0; index < snapshot.mCaptureCount; ++index)
    {
        const LLPrismLens::CaptureDefinition& candidate =
            snapshot.mCaptures[index];
        if (candidate.mHandle.mId == id)
        {
            capture = candidate.mHandle;
            if (mode) *mode = candidate.mMode;
            return true;
        }
    }
    return false;
}
}

bool ALVCamMonitorStrip::beginDragFromTile(
    const LLPrismLens::CaptureHandle& capture,
    LLPrismLens::EFitMode new_face_fit, std::string* reason)
{
    LLPrismLens::CaptureHandle current;
    LLPrismLens::ECaptureMode mode = LLPrismLens::ECaptureMode::SURFACE_LENS;
    if (!currentCapture(capture.mId, current, &mode) ||
        !sameCapture(current, capture))
    {
        if (reason) *reason = "That capture no longer exists.";
        return false;
    }
    if (mode == LLPrismLens::ECaptureMode::SURFACE_LENS)
    {
        if (reason)
            *reason = "A Surface Lens owns exactly one immutable aperture display.";
        return false;
    }
    if (new_face_fit != LLPrismLens::EFitMode::FIT &&
        new_face_fit != LLPrismLens::EFitMode::FILL &&
        new_face_fit != LLPrismLens::EFitMode::STRETCH)
    {
        if (reason) *reason = "Unknown display fit mode.";
        return false;
    }
    sActiveDragFit = new_face_fit;
    LLToolDragAndDrop::instance().beginDrag(
        DAD_VCAM_FEED, current.mId, LLToolDragAndDrop::SOURCE_VCAM);
    if (reason) reason->clear();
    return true;
}

bool ALVCamMonitorStrip::beginDragFromTile(
    const LLUUID& capture_id, LLPrismLens::EFitMode new_face_fit,
    std::string* reason)
{
    LLPrismLens::CaptureHandle capture;
    if (!currentCapture(capture_id, capture))
    {
        if (reason) *reason = "That capture no longer exists.";
        return false;
    }
    return beginDragFromTile(capture, new_face_fit, reason);
}

LLPrismLens::EFitMode ALVCamMonitorStrip::activeDragFitMode()
{
    return sActiveDragFit;
}

bool ALVCamMonitorStrip::handleDisplayRowDrop(
    const LLPrismLens::DisplayHandle& display, bool drop,
    EDragAndDropType cargo_type, void* cargo_data, EAcceptance* accept,
    std::string& tooltip_msg)
{
    if (cargo_type != DAD_VCAM_FEED || !cargo_data ||
        LLToolDragAndDrop::instance().getSource() !=
            LLToolDragAndDrop::SOURCE_VCAM)
        return false;

    LLPrismLens::CaptureHandle capture;
    const LLUUID& capture_id = *static_cast<const LLUUID*>(cargo_data);
    if (!currentCapture(capture_id, capture))
    {
        *accept = ACCEPT_NO_CUSTOM;
        tooltip_msg = "That capture no longer exists.";
        return true;
    }
    const LLPrismLens::ActionStatus status =
        ALVCamPageModel::reassignDisplayStatus(display, capture);
    if (!status.allowed())
    {
        *accept = ACCEPT_NO_CUSTOM;
        tooltip_msg = status.mReason;
        return true;
    }
    *accept = ACCEPT_YES_SINGLE;
    tooltip_msg.clear();
    if (drop)
    {
        std::string reason;
        if (LLPrismLens::reassignDisplay(display, capture, &reason) !=
            LLPrismLens::ERegistryResult::OK)
        {
            *accept = ACCEPT_NO_CUSTOM;
            tooltip_msg = reason;
        }
    }
    return true;
}
