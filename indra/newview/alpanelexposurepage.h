/**
 * @file alpanelexposurepage.h
 * @brief Director > Light > Exposure page logic: tone-mapper gating, undo/redo and the Light Intensity presets.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 *
 * The page root of the generated kit page (class "panel_exposure_page"). Every row but the buttons
 * is a plain setting binding; this class adds what the shared Exposure hub
 * (alpanelexposurehub.cpp) did around them:
 *   - AMD exposure and S-Log EV are enabled only for the tone mapper they belong to;
 *   - Undo and Redo drive ALExposureSettingsHistory, and slider/spinner drags are grouped into one
 *     step (installEditFinalizers);
 *   - the "Open window" button opens the Exposure hub floater.
 * It also registers the "env_intensity" preset provider (built-in and saved Light Intensity
 * presets) that the page's preset strip uses.
 */

#ifndef AL_ALPANELEXPOSUREPAGE_H
#define AL_ALPANELEXPOSUREPAGE_H

#include "llpanel.h"
#include "aluirefresh.h"

#include <boost/signals2.hpp>
#include <vector>

class LLButton;
class LLUICtrl;
class LLTextBox;
class LLComboBox;

class ALPanelExposurePage final : public LLPanel
{
public:
    ALPanelExposurePage();
    ~ALPanelExposurePage() override = default;

    bool postBuild() override;
    void onVisibilityChange(bool new_visibility) override;
    void draw() override;

private:
    void updateTonemapGating();
    void updateEnvOnlyAvailability();
    boost::signals2::scoped_connection mEnvOnlyHDRConnection;
    boost::signals2::scoped_connection mEnvOnlyConnection;
    void updateHistoryButtons();
    void updateSensor();
    void showSensorReadout();
    void updateSensorStatus();
    void updateSensorGating();
    void syncSensorISO();

    LLButton* mUndo = nullptr;
    LLButton* mRedo = nullptr;
    LLUICtrl* mAMDExposure = nullptr;
    LLUICtrl* mAMDExposureReset = nullptr;
    LLUICtrl* mSLogEV = nullptr;
    LLUICtrl* mSLogEVReset = nullptr;
    LLTextBox* mSensorReadout = nullptr;
    LLTextBox* mSensorStatus = nullptr;
    LLComboBox* mSensorISO = nullptr;
    LLUICtrl* mSensorSeed = nullptr;
    LLSD mSensorStoredISO;
    LLSD mSensorStoredSeed;
    LLView* mSensorSeedRow = nullptr;
    LLView* mCustomSensorCard = nullptr;
    bool mSensorDirty = true;
    bool mSensorEnabled = false;
    std::string mSensorReadoutText;
    ALLiveReadoutClock mSensorClock{10.f};
    ALLiveReadoutClock mSensorStatusClock{2.f};
    std::vector<boost::signals2::scoped_connection> mSensorConnections;

    boost::signals2::scoped_connection mTonemapConnection;
    boost::signals2::scoped_connection mHistoryConnection;
};

#endif // AL_ALPANELEXPOSUREPAGE_H
