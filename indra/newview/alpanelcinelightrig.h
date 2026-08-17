/**
 * @file alpanelcinelightrig.h
 * @brief Shared Director/standalone cinematic light-rig panel.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#ifndef AL_PANEL_CINE_LIGHT_RIG_H
#define AL_PANEL_CINE_LIGHT_RIG_H

#include "llpanel.h"
#include "lluicolor.h"
#include "lluuid.h"

#include <string>
#include <vector>

class LLComboBox;
class LLButton;
class LLCheckBoxCtrl;
class LLLineEditor;
class LLSpinCtrl;
class LLTextBox;

class ALPanelCineLightRig final : public LLPanel
{
public:
    ALPanelCineLightRig();
    ~ALPanelCineLightRig() override;

    bool postBuild() override;
    void draw() override;
    void onVisibilityChange(bool new_visibility) override;

private:
    static const std::vector<std::string>& settings();

    void populateStaticCombos();
    void updateAnchorList();
    void syncAnchorSelection();
    void syncGroupControls();
    void syncSeedEditor(bool force = false);
    void updateDerivedStatus();
    S32 computeRequestedShadowSlots() const;
    void refreshSetupList(const std::string& select_name = std::string(),
                          bool allow_empty = false, bool force = false);
    static void refreshAllSetupLists(ALPanelCineLightRig* acting_panel,
        const std::string& acting_selection,
        const std::string& deleted_name = std::string());

    void onAnchorSelected();
    void onGroupEnabledCommit();
    void onGroupSlotsCommit();
    void adjustAim(const std::string& setting, F32 delta);
    void resetAim();
    void onSetupSelected();
    void saveSetup();
    void deleteSetup();
    bool deleteSetupCallback(const LLSD& notification, const LLSD& response,
                             const std::string name);
    void resetAll();
    bool resetAllCallback(const LLSD& notification, const LLSD& response);
    void commitSeed();
    void randomizeSeed();
    void onClickShadowFixIt();

    LLComboBox* mAnchorCombo = nullptr;
    LLCheckBoxCtrl* mGroupEnable = nullptr;
    LLCheckBoxCtrl* mGroupSlotChecks[5] = {};
    LLTextBox* mGroupStatus = nullptr;
    LLComboBox* mSetupCombo = nullptr;
    LLComboBox* mFXCombo = nullptr;
    LLLineEditor* mSeedEditor = nullptr;
    LLSpinCtrl* mFillEV = nullptr;
    LLTextBox* mClipStatus[4] = {};
    LLCheckBoxCtrl* mShaftControls[4] = {};
    LLCheckBoxCtrl* mHeroControls[4] = {};
    LLTextBox* mShadowHint = nullptr;
    LLButton* mShadowFixIt = nullptr;
    LLTextBox* mRadiusLabel = nullptr;
    LLButton* mSetupDelete = nullptr;
    std::vector<LLUUID> mCastIds;
    std::vector<std::string> mCastNames;
    S32 mDisplayedSlot = -1;
    bool mDisplayedGroupEnabled = false;
    U32 mDisplayedGroupSlots = ~0u;
    U32 mDisplayedResolvedSlots = ~0u;
    U32 mDisplayedSeed = 0;
    S32 mShadowHintState = -1;
    S32 mShadowHintRequested = -1;
    S32 mShadowHintSlots = -1;
    LLUIColor mRadiusDefaultColor;
    std::string mPendingSetupSelection;
    bool mAnchorSelectionInitialized = false;
    bool mCastListInitialized = false;
    bool mSetupRefreshPending = false;
    bool mPendingSetupAllowEmpty = false;
    bool mSeedInitialized = false;
    bool mRadiusCueInitialized = false;
    bool mRadiusOverCeiling = false;
};

#endif // AL_PANEL_CINE_LIGHT_RIG_H
