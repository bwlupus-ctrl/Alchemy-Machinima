/**
 * @file alpaneldirectorvcam.h
 * @brief Director v2 Camera > VCam kit-page adapter.
 */

#ifndef AL_PANEL_DIRECTOR_VCAM_H
#define AL_PANEL_DIRECTOR_VCAM_H

#include "alvcampagemodel.h"
#include "llpanel.h"
#include "llframetimer.h"

#include <array>
#include <boost/signals2.hpp>
#include <unordered_map>
#include <vector>

class ALKitCard;
class ALKitPage;
class ALKitRow;
class ALVCamMonitorStrip;
class ALVCamMonitorHeader;
class ALVCamMonitorPager;
class ALVCamCameraSelector;
class ALVCamFaceList;
class ALVCamProjectorList;
class LLContextMenu;
class LLLineEditor;
class LLUICtrl;

class ALPanelDirectorVCam final : public LLPanel
{
public:
    ALPanelDirectorVCam();
    ~ALPanelDirectorVCam() override;
    bool postBuild() override;
    void draw() override;
    void onVisibilityChange(bool visible) override;

private:
    enum EProjectorControl : U32
    {
        PROJ_LOOK, PROJ_FIT, PROJ_ANCHOR_X, PROJ_ANCHOR_Y, PROJ_FLIP_H,
        PROJ_FLIP_V, PROJ_ROTATE, PROJ_KEYSTONE_H, PROJ_KEYSTONE_V,
        PROJ_THROW, PROJ_BRIGHTNESS, PROJ_LAMP_POWER, PROJ_FOLLOW_EXPOSURE,
        PROJ_LAMP_KELVIN,
        PROJ_GEL_TINT, PROJ_HOTSPOT, PROJ_SHUTTER_FLICKER, PROJ_SHUTTER_RATE,
        PROJ_GATE_WEAVE, PROJ_GRAIN, PROJ_DUST, PROJ_SCRATCHES, PROJ_BURN,
        PROJ_HALATION, PROJ_RESOLUTION, PROJ_NO_SIGNAL, PROJ_KEEP_GOBO,
        PROJ_TV_PRESET, PROJ_TV_SCAN, PROJ_TV_GRAY, PROJ_TV_STATIC,
        PROJ_TV_VIGNETTE, PROJ_TV_BRIGHTNESS, PROJ_TV_LINES, PROJ_TV_DARK,
        PROJ_TV_SHARP, PROJ_TV_THICK, PROJ_TV_PIXEL, PROJ_TV_SEPIA,
        PROJ_TV_CHROMA, PROJ_TV_VROLL, PROJ_TV_ROLL_SPEED, PROJ_TV_TRACKING,
        PROJ_TV_FLICKER, PROJ_TV_INTERLACE, PROJ_TV_DROPOUT, PROJ_TV_SHEEN,
        PROJ_CONTROL_COUNT
    };

    void collectControls(LLView* root);
    void onModelChanged(U32 dirty);
    void refresh(U32 dirty = ALVCamPageModel::DIRTY_ALL);
    void refreshPreviewDemand();
    void onControl(LLUICtrl* control);
    void onMonitorMenu(const std::string& action, const LLSD& parameter = LLSD());
    void showMonitorMenu(U32 slot, S32 x, S32 y);
    void setStatus(const std::string& text, bool warning = true);
    void clearStatus();
    void setAspectValue(const std::string& name, F32 ratio);
    void updateCardState(bool have_capture, bool have_display,
                         const std::string& camera_label);
    LLUICtrl* control(const std::string& name) const;
    LLUICtrl* projectorControl(EProjectorControl control) const
    {
        return mProjectorControls[control];
    }
    ALKitRow* row(const std::string& name) const;
    const LLPrismLens::CaptureDefinition* selectedCapture() const;
    const LLPrismLens::DisplayDefinition* selectedDisplay() const;
    const LLPrismLens::ProjectorDefinition* selectedProjector() const;
    void commitCamera(const std::string& name, const LLSD& value);
    void commitDisplay(const std::string& name, const LLSD& value);
    void commitProjector(const std::string& name, const LLSD& value);

    ALVCamPageModel mModel;
    ALKitPage* mKitPage = nullptr;
    ALVCamMonitorStrip* mMonitorStrip = nullptr;
    ALVCamMonitorHeader* mMonitorHeader = nullptr;
    ALVCamMonitorPager* mMonitorPager = nullptr;
    ALVCamCameraSelector* mCameraSelector = nullptr;
    ALVCamFaceList* mFaceList = nullptr;
    ALVCamProjectorList* mProjectorList = nullptr;
    LLLineEditor* mCameraName = nullptr;
    std::unordered_map<std::string, LLUICtrl*> mControls;
    std::vector<ALKitCard*> mCards;
    std::array<LLUICtrl*, PROJ_CONTROL_COUNT> mProjectorControls{};
    std::unordered_map<std::string, ALKitRow*> mRows;
    std::unordered_map<LLUUID, std::string> mProjectorNames;
    std::array<std::string, LLPrismLens::MAX_CAPTURES> mCameraLabels;
    std::vector<boost::signals2::scoped_connection> mConnections;
    LLHandle<LLContextMenu> mMonitorMenu;
    LLPrismLens::CaptureHandle mMenuCapture;
    LLPrismLens::CaptureHandle mMoveTargetCapture;
    std::vector<LLPrismLens::DisplayHandle> mMenuDisplays;
    std::array<LLPrismLens::CaptureHandle, LLPrismLens::MAX_CAPTURES> mPreviewDemand;
    LLPrismLens::EFitMode mNewFaceFit = LLPrismLens::EFitMode::FIT;
    bool mCompact = false;
    U32 mLargePage = 0;
    U64 mCameraLabelRevision = 0;
    bool mCustomAspect = false;
    bool mCustomScreenAspect = false;
    bool mRefreshing = false;
    std::string mStatus;
    LLFrameTimer mStatusTimer;
    bool mStatusWarning = false;
};

#endif
