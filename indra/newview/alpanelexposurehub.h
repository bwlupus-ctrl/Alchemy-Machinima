/**
 * @file alpanelexposurehub.h
 * @brief Shared Exposure controls from panel_exposure_hub.xml.
 */

#ifndef AL_ALPANELEXPOSUREHUB_H
#define AL_ALPANELEXPOSUREHUB_H

#include "alscrollfit.h"
#include "llpanel.h"

#include <boost/signals2.hpp>

class LLButton;
class LLTabContainer;
class LLUICtrl;

class ALPanelExposureHub final : public LLPanel
{
public:
    ALPanelExposureHub();
    ~ALPanelExposureHub() override = default;

    bool postBuild() override;
    void onVisibilityChange(bool new_visibility) override;

private:
    void updateTonemapGating();
    void onNightMaskDarknessCommit(LLUICtrl* source);
    void syncNightMaskDarkness();
    void updateHistoryButtons();

    ALScrollFit::Controller mScrollFit;
    LLTabContainer* mTabs = nullptr;
    LLButton* mUndo = nullptr;
    LLButton* mRedo = nullptr;
    LLUICtrl* mAMDExposure = nullptr;
    LLUICtrl* mAMDExposureReset = nullptr;
    LLUICtrl* mSLogEV = nullptr;
    LLUICtrl* mSLogEVReset = nullptr;
    LLUICtrl* mNightMaskDarkness = nullptr;

    boost::signals2::scoped_connection mTonemapConnection;
    boost::signals2::scoped_connection mNightMaskConnection;
    boost::signals2::scoped_connection mHistoryConnection;
};

#endif // AL_ALPANELEXPOSUREHUB_H
