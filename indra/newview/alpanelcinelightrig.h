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
#include "alrigrimcontroller.h"
#include "aluirefresh.h"
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
    // [TronT1] Applies a named Rig Rim preset row (RIG_RIM_PRESETS, defined
    // in alpanelcinelightrig.cpp) by label match -- lets ALTron::applyPreset
    // (the Tron look presets) and the Lightbox "Tron" tab's Integration
    // group ("Apply Tron Rig Rim preset" button) stamp the same look without
    // duplicating its values. Returns false (no-op) when no row matches.
    // CineRigRimEnabled/DebugRimOnly/IncludeProbes are never touched, same
    // as onRigRimPresetSelected().
    static bool applyRigRimPresetByName(const std::string& name);
    // Keeps this wrapper's own rect in sync with the flow_grid card layout
    // it hosts. flow_grid self-sizes to its packed card content whenever it
    // reshapes (see ALFlowGrid::reshape), but that only updates flow_grid's
    // own rect -- our containing LLScrollContainer reads *our* rect (we are
    // its scroll document) to compute the scrollable range, so we mirror the
    // grid's natural height onto ourselves after every reshape.
    void reshape(S32 width, S32 height, bool called_from_parent = true) override;

private:
    enum class EasyModeOrigin
    {
        USER,
        PASSIVE,
        SEARCH,
        RESTORE
    };

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
    // explicit_entry: the user clicked the Easy toggle, asking for the
    // simplified vocabulary - Rim/Bg EVs may be snapped to their nearest Easy
    // presence bucket. Passive re-syncs (first show, slot switch, scene load)
    // pass false and never mutate near-bucket authored EVs.
    bool normalizeSelectedForEasy(bool explicit_entry);
    bool applyEasyMode(bool desired, EasyModeOrigin origin, bool force = false);
    bool syncEasyModeState(bool desired, EasyModeOrigin origin, bool force = false);
    void onEasyModeCommit();
    void onEasyBrightnessCommit();
    void onEasyDramaCommit();
    void onEasyRimCommit();
    void onEasyBgCommit();
    void onEasyWarmthCommit();
    void onEasyConeWidthCommit();
    void onEasyConeFeatherCommit();
    void onManualConeWidthCommit(S32 light);
    std::string fixtureSettingPrefix() const;
    void syncFixtureControls(bool force = false);
    void onFixtureRoleCommit();
    void onFixtureModeCommit();
    void onFixturePresetCommit();
    void onFixtureValuesCommit();
    void syncGoboLibrary(bool force = false);
    void syncProjectorFeeds(bool force = false);
    void onProjectorFeedCommit(S32 light);
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
    void adjustSelectedLightAim(const char* axis, F32 delta);
    void resetAim();
    void onSetupSelected();
    void onFlarePresetSelected(LLComboBox* source);
    void onNightMaskPresetSelected(LLComboBox* source);
    // [NightMaskPercent] The Night Mask Darkness sliders show
    // round((1 - floor) * 100) while CineLightRigNightMaskDarkness stays the
    // 0..1 brightness floor. Commit maps percent -> floor; the setting-signal
    // listener maps floor -> percent onto BOTH sliders (setValue never commits,
    // so there is no feedback loop).
    void onNightMaskDarknessCommit(LLUICtrl* source);
    void syncNightMaskDarknessSliders();
    // [RigRimPreset] Applies a full Rig Rim look (master/soften/back-softness/
    // tint/shadow/alpha/bounce + every per-light gain/sharpness/wrap/back-bias)
    // and snaps the combo back to the sentinel. Never touches
    // CineRigRimEnabled, CineRigRimDebugRimOnly or CineRigRimIncludeProbes.
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
    LLComboBox* mEasyFlarePreset = nullptr;
    LLComboBox* mNightMaskPreset = nullptr;
    LLComboBox* mEasyNightMaskPreset = nullptr;
    LLUICtrl* mNightMaskDarkness = nullptr; // [NightMaskPercent]
    LLUICtrl* mEasyNightMaskDarkness = nullptr; // [NightMaskPercent]
    boost::signals2::scoped_connection mNightMaskDarknessConnection; // [NightMaskPercent]
    boost::signals2::scoped_connection mEasyModeConnection;
    LLTextBox* mNightMaskStatus = nullptr;
    LLComboBox* mSubjectTargetCombos[3] = {};
    ALRigRimController mRim;
    LLComboBox* mFXCombo = nullptr;
    LLLineEditor* mSeedEditor = nullptr;
    LLSpinCtrl* mFillEV = nullptr;
    LLUICtrl* mEasyBrightness = nullptr;
    LLUICtrl* mEasyDrama = nullptr;
    LLComboBox* mEasyRim = nullptr;
    LLComboBox* mEasyBg = nullptr;
    LLUICtrl* mEasyWarmth = nullptr;
    LLUICtrl* mEasyConeWidth = nullptr;
    LLUICtrl* mEasyConeFeather = nullptr;
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
    LLComboBox* mProjectorFeeds[4] = {};
    U64 mProjectorFeedRevision = 0;
    S32 mProjectorFeedRigSlot = -1;
    std::vector<LLUICtrl*> mAdvancedDrivenControls;
    LLTextBox* mClipStatus[4] = {};
    LLCheckBoxCtrl* mShaftControls[4] = {};
    LLCheckBoxCtrl* mHeroControls[4] = {};
    // [ActorMask] Actor only checkboxes: 4 per-light, then Bounce lights the
    // set, Catchlight actor only, then the rig-level master "Actor only (all
    // lights)". Each has a reset button. All are disabled while the selected rig
    // has an object target (Actor only is avatar-only in v1); the four per-light
    // boxes and the catchlight box are also disabled while the master is on.
    static constexpr S32 ACTOR_ONLY_CONTROL_COUNT = 7;
    static constexpr S32 ACTOR_ONLY_MASTER_INDEX = 6;
    LLUICtrl* mActorOnlyControls[ACTOR_ONLY_CONTROL_COUNT] = {};
    LLUICtrl* mActorOnlyResets[ACTOR_ONLY_CONTROL_COUNT] = {};
    std::string mActorOnlyTips[ACTOR_ONLY_CONTROL_COUNT];
    // [ActorMask] -1 unset; else bit 0 = object-target lock, bit 1 = master on.
    S32 mActorOnlyLockState = -1;
    void syncActorOnlyControls(bool object_targeted, bool actor_only_all); // [ActorMask]
    // [ActorMask] Master click: ticks (or unticks) every per-light + catchlight
    // Actor only box -- the proven per-light path.
    void onActorOnlyAllChanged(bool enabled);
    LLUICtrl* mManualConeWidths[4] = {};
    LLTextBox* mShadowHint = nullptr;
    LLTextBox* mLiveProbeStatus = nullptr;
    LLTextBox* mLiveProbeLiveStatus = nullptr;
    LLButton* mLiveProbeRefreshNow = nullptr; // [LiveProbeRefresh]
    // [ProbeManualRate] Manual-mode controls (enabled only when Refresh = Manual)
    LLUICtrl* mLiveProbeManualFaces = nullptr;
    LLUICtrl* mLiveProbeManualFrames = nullptr;
    LLUICtrl* mLiveProbeManualFacesReset = nullptr;
    LLUICtrl* mLiveProbeManualFramesReset = nullptr;
    LLButton* mShadowFixIt = nullptr;
    LLTextBox* mRadiusLabel = nullptr;
    LLButton* mSetupSave = nullptr;
    LLButton* mSetupDelete = nullptr;
    // [UIPerf] Widgets that used to be findChild'd by name every frame
    // (syncEasyControls / applyManualLock / updateDerivedStatus). Resolved once
    // in postBuild(); null-checked at use exactly like the old findChild result.
    LLUICtrl* mFXStop = nullptr;
    LLUICtrl* mEasyGatedControls[8] = {};   // shaft, flare, probe x3, night x3
    LLUICtrl* mEasyResetButtons[8] = {};
    LLView* mFlowGrid = nullptr;
    // [UIPerf] last text pushed per frame, so unchanged status lines skip the
    // LLTextBox re-parse / reflow.
    std::string mLastLiveProbeStatus;
    std::string mLastNightMaskStatus;
    std::string mLastGoboSoftness;
    bool mLiveProbeStatusSet = false;
    S32 mLastLiveProbeState = -1;
    bool mNightMaskStatusSet = false;
    bool mGoboSoftnessSet = false;
    S32 mDisplayedGoboIndex = -1;
    ALLiveReadoutClock mPanelRefreshClock{10.f};
    ALLiveReadoutClock mLiveProbeClock{2.f};
    std::vector<LLUUID> mCastIds;
    std::vector<std::string> mCastNames;
    std::vector<S32> mCastValues;
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
    U64 mSubjectChoiceRevision = ~U64(0);
    bool mCastListInitialized = false;
    bool mSetupRefreshPending = false;
    bool mPendingSetupAllowEmpty = false;
    bool mSeedInitialized = false;
    bool mRadiusCueInitialized = false;
    bool mRadiusOverCeiling = false;
    S32 mEasyModeSlot = -1;
    S32 mEasyModeOverride = -1;
    bool mEasyModeActive = false;
    bool mSyncingEasyControls = false;
    bool mApplyingEasyMode = false;
    S32 mFixtureRoleIndex = 0;
    S32 mDisplayedFixtureSlot = -1;
};

#endif // AL_PANEL_CINE_LIGHT_RIG_H
