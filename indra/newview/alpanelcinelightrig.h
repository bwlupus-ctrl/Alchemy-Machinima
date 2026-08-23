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
class LLIconCtrl;
class LLSpinCtrl;
class LLTextBox;
class LLUICtrl;

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
    void syncObjectTargetControls(bool force = false);
    void syncGroupControls();
    void syncSeedEditor(bool force = false);
    void syncEasyModeForSelected(bool force = false);
    void syncEasyControls();
    bool selectedIsEasyNative() const;
    bool normalizeSelectedForEasy();
    void onEasyModeCommit();
    void onEasyBrightnessCommit();
    void onEasyDramaCommit();
    void onEasyRimCommit();
    void onEasyBgCommit();
    void onEasyWarmthCommit();
    std::string fixtureSettingPrefix() const;
    void syncFixtureControls(bool force = false);
    void onFixtureRoleCommit();
    void onFixtureModeCommit();
    void onFixturePresetCommit();
    void onFixtureValuesCommit();
    void syncGoboLibrary(bool force = false);
    void onGoboLibraryCommit();
    void onManualCommit();
    void applyManualLock();
    void updateDerivedStatus();
    S32 computeRequestedShadowSlots() const;
    void refreshSetupList(const std::string& select_name = std::string(),
                          bool allow_empty = false, bool force = false);
    static void refreshAllSetupLists(ALPanelCineLightRig* acting_panel,
        const std::string& acting_selection,
        const std::string& deleted_name = std::string());

    void onAnchorSelected();
    void onTargetObject();
    void onClearObjectTarget();
    void onGroupEnabledCommit();
    void onGroupSlotsCommit();
    void adjustAim(const std::string& setting, F32 delta);
    void resetAim();
    void onSetupSelected();
    void onFlarePresetSelected();
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
    LLButton* mObjectTargetButton = nullptr;
    LLButton* mObjectTargetClear = nullptr;
    LLTextBox* mObjectTargetStatus = nullptr;
    LLCheckBoxCtrl* mGroupEnable = nullptr;
    LLCheckBoxCtrl* mGroupSlotChecks[5] = {};
    LLTextBox* mGroupStatus = nullptr;
    LLComboBox* mSetupCombo = nullptr;
    LLComboBox* mFlarePreset = nullptr;
    LLComboBox* mFXCombo = nullptr;
    LLLineEditor* mSeedEditor = nullptr;
    LLSpinCtrl* mFillEV = nullptr;
    LLUICtrl* mEasyBrightness = nullptr;
    LLUICtrl* mEasyDrama = nullptr;
    LLComboBox* mEasyRim = nullptr;
    LLComboBox* mEasyBg = nullptr;
    LLUICtrl* mEasyWarmth = nullptr;
    LLCheckBoxCtrl* mEasyModeToggle = nullptr;
    LLComboBox* mFixtureRole = nullptr;
    LLCheckBoxCtrl* mFixtureMode = nullptr;
    LLComboBox* mFixturePreset = nullptr;
    LLSpinCtrl* mFixtureKelvin = nullptr;
    LLSpinCtrl* mFixtureSourceSize = nullptr;
    LLComboBox* mFixtureGelSlots[3] = {};
    LLComboBox* mGoboLibrary = nullptr;
    LLIconCtrl* mGoboPreview = nullptr;
    LLTextBox* mGoboSoftness = nullptr;
    std::vector<LLUICtrl*> mAdvancedDrivenControls;
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
    LLUUID mDisplayedGroupObjectTarget;
    LLUUID mDisplayedObjectTarget;
    S32 mDisplayedObjectTargetSlot = -1;
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
    S32 mEasyModeSlot = -1;
    bool mEasyModeActive = false;
    bool mSyncingEasyControls = false;
    S32 mFixtureRoleIndex = 0;
    S32 mDisplayedFixtureSlot = -1;
};

#endif // AL_PANEL_CINE_LIGHT_RIG_H
