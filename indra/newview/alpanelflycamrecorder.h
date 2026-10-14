/**
 * @file alpanelflycamrecorder.h
 * @brief Flycam Recorder transport panel: record/play/stop/clear, scrub,
 *        time + status readouts, Save/Load, and the settings-backed playback
 *        (speed, loop, smooth, anchor, follow, operator) and sample-rate rows.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 *
 * Extracted from LLFloaterFlycamRecorder AND the Director Console's Takes tab
 * (which hand-copied the same controls with a second copy of the wiring) so the
 * standalone floater and the console embed the SAME panel
 * (panel_flycam_recorder.xml) instead of duplicating the transport wiring, which
 * is how they drift. All state lives in the one LLFlycamRecorder singleton and
 * gSavedSettings: the transport buttons + scrub drive the singleton, draw()
 * refreshes the readouts and enable state from it, and the FlycamRec* controls
 * are settings-backed -- so two live instances (floater + console tab) stay in
 * lockstep with no fork, exactly like ALPanelCineCamParams / ALPanelPathEditor.
 * The Save/Load pickers route through the singleton, so a host closing mid-pick
 * can't dangle.
 */

#ifndef AL_ALPANELFLYCAMRECORDER_H
#define AL_ALPANELFLYCAMRECORDER_H

#include "llpanel.h"
#include "aluirefresh.h"

#include <boost/signals2/connection.hpp>

class LLButton;
class LLComboBox;
class LLSliderCtrl;
class LLTextBox;
class LLUICtrl;

class ALPanelFlycamRecorder final : public LLPanel
{
public:
    ALPanelFlycamRecorder() = default;
    ~ALPanelFlycamRecorder() override = default;

    bool postBuild() override;
    void draw() override;

private:
    void onRecord();
    void onPlayPause();
    void onStop();
    void onClear();
    void onSave();
    void onLoad();
    void onScrub();
    void updateFlightModelVisibility();

    // set a tooltip only when it changed (draw()-rate friendly)
    static void setToolTipIfChanged(LLUICtrl* ctrl, const std::string& tip);

    LLButton*     mRecordBtn = nullptr;
    LLButton*     mPlayBtn = nullptr;
    LLButton*     mStopBtn = nullptr;
    LLButton*     mClearBtn = nullptr;
    LLButton*     mSaveBtn = nullptr;
    LLButton*     mLoadBtn = nullptr;
    LLSliderCtrl* mScrub = nullptr;
    LLTextBox*    mTimeText = nullptr;
    LLTextBox*    mStatusText = nullptr;
    LLComboBox*   mDronePreset = nullptr;
    LLComboBox*   mDroneRelease = nullptr;
    LLComboBox*   mWeightedPreset = nullptr;
    LLUICtrl*     mWeightedWeight = nullptr;
    U64           mDronePresetRevision = 0;
    ALLiveReadoutClock mScrubClock{10.f};
    ALLiveReadoutClock mReadoutClock{4.f};
    boost::signals2::scoped_connection mFlightModelConnection;
};

#endif // AL_ALPANELFLYCAMRECORDER_H
