/**
 * @file alpaneldirectorvcam.h
 * @brief Director v2 Camera > VCam kit-page adapter.
 */

#ifndef AL_PANEL_DIRECTOR_VCAM_H
#define AL_PANEL_DIRECTOR_VCAM_H

#include "alvcampagemodel.h"
#include "llpanel.h"

#include <boost/signals2.hpp>
#include <unordered_map>
#include <vector>

class ALKitPage;
class ALKitRow;
class ALVCamMonitorStrip;
class ALVCamMonitorPager;
class ALVCamCameraSelector;
class ALVCamFaceList;
class LLContextMenu;
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
    void collectControls(LLView* root);
    void onModelChanged(U32 dirty);
    void refresh(U32 dirty = ALVCamPageModel::DIRTY_ALL);
    void refreshPreviewDemand();
    void onControl(LLUICtrl* control);
    void onMonitorMenu(const std::string& action);
    void showMonitorMenu(U32 slot);
    void setStatus(const std::string& text);
    LLUICtrl* control(const std::string& name) const;
    ALKitRow* row(const std::string& name) const;
    const LLPrismLens::CaptureDefinition* selectedCapture() const;
    const LLPrismLens::DisplayDefinition* selectedDisplay() const;
    void commitCamera(const std::string& name, const LLSD& value);
    void commitDisplay(const std::string& name, const LLSD& value);

    ALVCamPageModel mModel;
    ALKitPage* mKitPage = nullptr;
    ALVCamMonitorStrip* mMonitorStrip = nullptr;
    ALVCamMonitorPager* mMonitorPager = nullptr;
    ALVCamCameraSelector* mCameraSelector = nullptr;
    ALVCamFaceList* mFaceList = nullptr;
    std::unordered_map<std::string, LLUICtrl*> mControls;
    std::unordered_map<std::string, ALKitRow*> mRows;
    std::vector<boost::signals2::scoped_connection> mConnections;
    LLHandle<LLContextMenu> mMonitorMenu;
    LLPrismLens::CaptureHandle mMenuCapture;
    LLPrismLens::EFitMode mNewFaceFit = LLPrismLens::EFitMode::FIT;
    bool mCompact = false;
    U32 mLargePage = 0;
    bool mCustomAspect = false;
    bool mRefreshing = false;
    std::string mStatus;
};

#endif
