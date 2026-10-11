/**
 * @file alvcammonitorstrip.h
 * @brief Drag source/drop adapters shared by Director VCam monitor tiles.
 *
 * VC6 owns the tile presentation.  This class deliberately contains only the
 * gesture boundary so tiles, world faces, and face rows use one cargo format
 * and one page-model validation path.
 */

#ifndef AL_VCAM_MONITOR_STRIP_H
#define AL_VCAM_MONITOR_STRIP_H

#include "alvcampagemodel.h"
#include "llui.h"

class ALVCamMonitorStrip
{
public:
    static bool beginDragFromTile(
        const LLPrismLens::CaptureHandle& capture,
        LLPrismLens::EFitMode new_face_fit = LLPrismLens::EFitMode::FIT,
        std::string* reason = nullptr);
    static bool beginDragFromTile(
        const LLUUID& capture_id,
        LLPrismLens::EFitMode new_face_fit = LLPrismLens::EFitMode::FIT,
        std::string* reason = nullptr);

    // Used by LLToolDragAndDrop for an unbound world face.  The value is
    // gesture-local UI session state and is never persisted in the registry.
    static LLPrismLens::EFitMode activeDragFitMode();

    // LLUI face rows delegate their handleDragAndDrop implementation here.
    static bool handleDisplayRowDrop(
        const LLPrismLens::DisplayHandle& display, bool drop,
        EDragAndDropType cargo_type, void* cargo_data, EAcceptance* accept,
        std::string& tooltip_msg);
};

#endif
