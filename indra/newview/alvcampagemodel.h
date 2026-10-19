/**
 * @file alvcampagemodel.h
 * @brief Reusable command/snapshot model for Director VCam surfaces.
 */

#ifndef AL_VCAM_PAGE_MODEL_H
#define AL_VCAM_PAGE_MODEL_H

#include "llprismlens.h"

#include <array>
#include <boost/signals2.hpp>
#include <memory>

class LLViewerObject;

class ALVCamPageModel
{
public:
    enum class EProjectorLookPreset : S32
    {
        CUSTOM = -1,
        CLEAN_DIGITAL,
        CINEMA_XENON,
        HOME_MOVIE_16MM,
        OLD_SLIDE_PROJECTOR,
        WORN_FILM_REEL,
        COUNT
    };

    struct ProjectorLookPreset
    {
        const char* mName;
        LLPrismLens::ProjectorLook mLook;
    };

    struct FaceDropStatus
    {
        LLPrismLens::ActionStatus mAction;
        LLPrismLens::CaptureHandle mCapture;
        LLPrismLens::DisplayHandle mExistingDisplay;
        bool mReassignment = false;
    };
    struct ProjectorDropStatus
    {
        LLPrismLens::ActionStatus mAction;
        LLPrismLens::CaptureHandle mCapture;
        LLPrismLens::ProjectorHandle mExistingProjector;
        bool mReassignment = false;
        bool mAlreadyBound = false;
    };

    enum DirtyBits : U32
    {
        DIRTY_NONE = 0,
        DIRTY_CONFIGURATION = 1 << 0,
        DIRTY_RUNTIME = 1 << 1,
        DIRTY_PERFORMANCE = 1 << 2,
        DIRTY_SELECTION = 1 << 3,
        DIRTY_PREVIEW = 1 << 4,
        DIRTY_ALL = DIRTY_CONFIGURATION | DIRTY_RUNTIME |
                    DIRTY_PERFORMANCE | DIRTY_SELECTION | DIRTY_PREVIEW
    };
    using changed_signal_t = boost::signals2::signal<void(U32)>;

    ALVCamPageModel();
    ~ALVCamPageModel();

    const LLPrismLens::RegistrySnapshot& registry() const { return mRegistry; }
    const LLPrismLens::PerformanceSnapshot& performance() const { return mPerformance; }
    const LLPrismLens::CaptureHandle& selectedCapture() const { return mSelectedCapture; }
    const LLPrismLens::DisplayHandle& selectedDisplay() const { return mSelectedDisplay; }
    const LLPrismLens::ProjectorHandle& selectedProjector() const { return mSelectedProjector; }
    LLPrismLens::DisplayHandle monitorDisplay(U32 capture_slot) const;

    void setSelectedCapture(const LLPrismLens::CaptureHandle& capture);
    void setSelectedDisplay(const LLPrismLens::DisplayHandle& display);
    void setSelectedProjector(const LLPrismLens::ProjectorHandle& projector);
    bool setMonitorDisplay(const LLPrismLens::CaptureHandle& capture,
                           const LLPrismLens::DisplayHandle& display);
    boost::signals2::connection connectChanged(const changed_signal_t::slot_type& slot);
    void setPageVisible(bool visible);
    bool pageVisible() const { return mPageVisible; }
    void refreshNow(U32 dirty = DIRTY_ALL);
    void markDirty(U32 dirty);
    static void currentViewTransform(LLVector3& position,
                                     LLQuaternion& rotation);

    LLPrismLens::ERegistryResult addFromSelectedObject(std::string* reason = nullptr);
    LLPrismLens::ERegistryResult addVirtualAtView(std::string* reason = nullptr);
    LLPrismLens::ERegistryResult addMediaSlot(std::string* reason = nullptr);
    bool addOtsPair(std::string* reason = nullptr);
    LLPrismLens::ERegistryResult addSurfaceLens(std::string* reason = nullptr);
    bool removeCapture(const LLPrismLens::CaptureHandle& capture,
                       std::string* reason = nullptr);
    LLPrismLens::ERegistryResult bindSelectedFace(
        const LLPrismLens::CaptureHandle& capture, LLPrismLens::EFitMode fit,
        std::string* reason = nullptr);
    LLPrismLens::ActionStatus bindSelectedFaceStatus(
        const LLPrismLens::CaptureHandle& capture) const;
    LLPrismLens::ERegistryResult bindFaceTarget(
        const LLPrismLens::CaptureHandle& capture, LLViewerObject* object,
        S32 te, LLPrismLens::EFitMode fit, std::string* reason = nullptr);
    LLPrismLens::ERegistryResult reassignDisplay(
        const LLPrismLens::DisplayHandle& display,
        const LLPrismLens::CaptureHandle& capture,
        std::string* reason = nullptr);
    static LLPrismLens::ActionStatus reassignDisplayStatus(
        const LLPrismLens::DisplayHandle& display,
        const LLPrismLens::CaptureHandle& capture);
    static FaceDropStatus faceDropStatus(const LLUUID& capture_id,
                                         LLViewerObject* object, S32 te);
    static LLPrismLens::ERegistryResult dropOnFace(
        const LLUUID& capture_id, LLViewerObject* object, S32 te,
        LLPrismLens::EFitMode fit, bool* reassigned = nullptr,
        std::string* reason = nullptr);
    static LLPrismLens::ERegistryResult applyToSelectedFaces(
        const LLUUID& capture_id, LLPrismLens::EFitMode fit,
        std::string* reason = nullptr);
    static bool clearSelectedFaces(std::string* reason = nullptr);
    static ProjectorDropStatus projectorDropStatus(
        const LLUUID& capture_id, LLViewerObject* object);
    static LLPrismLens::ERegistryResult dropOnProjector(
        const LLUUID& capture_id, LLViewerObject* object,
        bool* reassigned = nullptr, std::string* reason = nullptr);
    LLPrismLens::ERegistryResult addVirtualScreen(
        const LLPrismLens::CaptureHandle& capture, const LLVector3& pos,
        const LLQuaternion& rot, F32 width, F32 height,
        std::string* reason = nullptr);
    bool setCamera(const LLPrismLens::CaptureHandle& capture,
                   const LLPrismLens::CameraSettings& settings,
                   std::string* reason = nullptr);
    bool setRate(const LLPrismLens::CaptureHandle& capture,
                 const LLPrismLens::CaptureRateSettings& settings,
                 std::string* reason = nullptr);
    bool setDisplay(const LLPrismLens::DisplayHandle& display,
                    const LLPrismLens::DisplaySettings& settings,
                    std::string* reason = nullptr);
    LLPrismLens::ActionStatus projectorStatus(
        const LLPrismLens::CaptureHandle& capture, LLViewerObject* object,
        const LLPrismLens::ProjectorHandle* reassigning = nullptr) const;
    LLPrismLens::ERegistryResult bindProjector(
        const LLPrismLens::CaptureHandle& capture, LLViewerObject* object,
        const LLPrismLens::ProjectorSettings& settings,
        LLPrismLens::ProjectorHandle* binding = nullptr,
        std::string* reason = nullptr);
    LLPrismLens::ERegistryResult reassignProjector(
        const LLPrismLens::ProjectorHandle& binding,
        const LLPrismLens::CaptureHandle& capture,
        std::string* reason = nullptr);
    bool setProjectorSettings(
        const LLPrismLens::ProjectorHandle& binding,
        const LLPrismLens::ProjectorSettings& settings,
        std::string* reason = nullptr);
    bool setProjectorLook(
        const LLPrismLens::ProjectorHandle& binding,
        EProjectorLookPreset preset, std::string* reason = nullptr);
    static const std::array<ProjectorLookPreset,
        static_cast<size_t>(EProjectorLookPreset::COUNT)>& projectorLookPresets();
    static EProjectorLookPreset projectorLookPreset(
        const LLPrismLens::ProjectorLook& look);

    bool removeProjector(const LLPrismLens::ProjectorHandle& binding,
                         std::string* reason = nullptr);
    bool setLabel(const LLPrismLens::CaptureHandle& capture,
                  const std::string& label, std::string* reason = nullptr);

private:
    bool onIdleRefresh(const std::weak_ptr<bool>& alive);
    void selectAfterAdd(const LLPrismLens::CaptureHandle& capture);
    const LLPrismLens::CaptureDefinition* findCapture(
        const LLPrismLens::CaptureHandle& handle) const;
    const LLPrismLens::DisplayDefinition* findDisplay(
        const LLPrismLens::DisplayHandle& handle) const;
    const LLPrismLens::ProjectorDefinition* findProjector(
        const LLPrismLens::ProjectorHandle& handle) const;
    LLPrismLens::RegistrySnapshot mRegistry;
    LLPrismLens::PerformanceSnapshot mPerformance;
    LLPrismLens::CaptureHandle mSelectedCapture;
    LLPrismLens::DisplayHandle mSelectedDisplay;
    LLPrismLens::ProjectorHandle mSelectedProjector;
    std::array<LLPrismLens::DisplayHandle, LLPrismLens::MAX_CAPTURES>
        mMonitorDisplay;
    U32 mDirty = DIRTY_ALL;
    bool mIdleQueued = false;
    F64 mNextRuntimeRefresh = 0.;
    bool mPageVisible = false;
    std::shared_ptr<bool> mAlive;
    boost::signals2::scoped_connection mConfigurationConnection;
    boost::signals2::scoped_connection mRuntimeConnection;
    boost::signals2::scoped_connection mPerformanceConnection;
    boost::signals2::scoped_connection mSelectionConnection;
    boost::signals2::scoped_connection mPreviewConnection;
    changed_signal_t mChanged;
};

#endif
