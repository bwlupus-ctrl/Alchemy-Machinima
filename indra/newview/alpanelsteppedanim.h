/**
 * @file alpanelsteppedanim.h
 * @brief Code-bound controls for Director > Look > Stepped Animation.
 */
#ifndef AL_PANEL_STEPPED_ANIM_H
#define AL_PANEL_STEPPED_ANIM_H

#include "llpanel.h"
#include "lltimer.h"
#include "lluuid.h"

class LLButton;
class LLComboBox;
class LLRadioGroup;
class LLScrollListCtrl;
class LLSliderCtrl;
class LLTextBox;
class LLView;

class ALPanelSteppedAnim final : public LLPanel
{
public:
    ALPanelSteppedAnim() = default;
    bool postBuild() override;
    void draw() override;
    void bindSteppedPage(LLView* root);

private:
    void onActorTargetCommit();
    void onOverrideListSelect();
    void onActorOverrideCommit();
    void onActorRateCommit();
    void onActorStepsCommit();
    void onActorRoleCommit();
    void onOverrideClear();
    void onRateChip();
    void rebuildActors();
    void refreshActor();
    void refreshConditionalRows();
    void refreshReadouts();
    void refreshStatus();
    void refreshRateReadout();
    void refreshSpeedMeter();
    void writeActor();
    LLUUID selectedActor() const;

    LLComboBox*       mActorTarget = nullptr;
    LLScrollListCtrl* mOverrideList = nullptr;
    LLComboBox*       mActorOverride = nullptr;
    LLSliderCtrl*     mActorRate = nullptr;
    LLComboBox*       mActorSteps = nullptr;
    LLComboBox*       mActorRole = nullptr;
    LLButton*         mOverrideClear = nullptr;
    LLRadioGroup*     mRateChips = nullptr;
    LLTextBox*        mStatus = nullptr;
    LLTextBox*        mRateReadout = nullptr;
    LLTextBox*        mSpeedMeter = nullptr;
    LLView*           mAdaptiveRows[5]{};
    LLView*           mMixedRows[2]{};
    LLView*           mManualPhaseRow = nullptr;
    LLView*           mActorOwnRows[2]{};
    U64               mCastRevision = 0;
    S32               mLastActive = -1;
    S32               mLastSkipped = -1;
    F32               mLastRate = -1.f;
    F32               mLastSpeed = -1.f;
    LLFrameTimer      mSpeedMeterTimer; // throttles the live speed readout to 4 Hz
    bool              mRefreshing = false;
};

#endif
