/**
 * @file alpanelghoststudio.h
 * @brief Shared Ghost Studio panel (instance list + full per-ghost controls).
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 *
 * A reusable LLPanel (class "panel_ghost_studio") over the ALGhostStudio data
 * model: the instance list, source picker + Add/Duplicate/Delete, per-instance
 * placement (numeric X/Y/Z/yaw/scale, one-click in-world Place, snap-to-actor/
 * snap-to-me), look (style / tint / alpha / shimmer / pixelation / glitch),
 * the LIVE|FROZEN pose controls, and the line/ring array helper. Embedded by
 * BOTH the standalone Ghost Studio floater and the Director Console's Ghosts
 * tab, exactly like ALPanelPathEditor -- fully self-contained (it carries its
 * own cast-source picker), self-refreshing in draw(), and change-diffed so
 * the per-frame cost is trivial.
 */

#ifndef AL_ALPANELGHOSTSTUDIO_H
#define AL_ALPANELGHOSTSTUDIO_H

#include "llpanel.h"

#include "lluuid.h"

#include <string>
#include <vector>

class LLButton;
class LLCheckBoxCtrl;
class LLComboBox;
class LLLineEditor;
class LLFlyoutButton;
class LLScrollListCtrl;
class LLSliderCtrl;
class LLSpinCtrl;
class LLTextBox;
class ALCompassDial;

class ALPanelGhostStudio final : public LLPanel
{
public:
    ALPanelGhostStudio();
    ~ALPanelGhostStudio() override;

    bool postBuild() override;
    void draw() override;
    void onVisibilityChange(bool new_visibility) override;

private:
    // ---- refreshers (draw-rate, all change-diffed) ----
    void refreshSourceCombo();      // "You" + cast members (rebuilt on cast change)
    void refreshList();             // instance rows (rebuilt on composed-sig change)
    void refreshDetail();           // selected-instance widgets + enables
    void refreshStatus();           // bottom status line

    LLUUID selectedInstance() const;    // list selection -> instance id (null = none)

    // ---- list + CRUD ----
    void onListSelect();
    void onListDoubleClick();       // toggles the row's enable
    void onClickAdd();
    void onNameCommit();
    void onClickDuplicate();
    void onClickDelete();
    void onClickRefresh();

    // ---- placement ----
    void onPosCommit();             // X/Y/Z spinners -> foot position
    void onHeadingDialCommit();     // live dial -> yaw + exact-entry spinner
    void onYawCommit();
    void onScaleCommit();
    void onChaosCommit();
    void onDriveModeCommit();
    void onDirectedAnimCommit();
    void onClickPlace();            // arm the one-shot in-world placement tool
    void onClickToActor();          // snap to the source's current feet
    void onClickToMe();             // snap to my avatar's current feet
    void exitPlaceMode();           // drop the transient tool if it is ours

    // ---- [R2-3] in-world edit mode (persistent ALToolGhostEdit) ----
    void onToggleEditMode();        // checkbox -> arm / disarm the edit tool
    void exitEditMode();            // drop the tool if it is ours + uncheck

    // ---- look ----
    void onStyleCommit();
    void onActorTintToggle();
    void onHueCommit();
    void onAlphaCommit();
    void onShimmerSpeedCommit();
    void onShimmerAmountCommit();
    void onPixelCommit();
    void onGlitchCommit();
    void onBrightnessCommit();      // [R2-1] night-scene output dimmer
    void onEntityLookCommit();

    // ---- pose ----
    void onClickFreeze();           // "Grab pose now" -> FROZEN snapshot
    void onClickLive();             // "Follow live" -> drop the snapshot

    // ---- array helper ----
    void onClickArray(bool ring);

    // ---- master toggle ----
    void onShowAllToggle();

    // cached display name for a source id ("You" for null/self)
    static std::string sourceName(const LLUUID& id);

    // ---- change-diffing state ----
    std::string mListSig;           // composed instance-list signature last built
    std::string mSourceSig;         // cast signature the source combo was built from
    LLUUID      mShownFor;          // instance the detail widgets were last loaded for
    bool        mEditMode = false;  // [R2-3] our transient edit tool is armed
    bool        mHintEdit = false;  // which hint string the header line shows

    // ---- widgets ----
    LLCheckBoxCtrl*   mShowAllCheck = nullptr;
    LLCheckBoxCtrl*   mEditModeCheck = nullptr;     // [R2-3]
    LLTextBox*        mHint = nullptr;              // [R2-3] swaps in edit mode
    LLScrollListCtrl* mList = nullptr;
    LLComboBox*       mSourceCombo = nullptr;
    LLFlyoutButton*   mAddBtn = nullptr;
    LLButton*         mDupBtn = nullptr;
    LLButton*         mDelBtn = nullptr;
    LLButton*         mRefreshBtn = nullptr;
    LLTextBox*        mTypeText = nullptr;
    LLLineEditor*     mNameEdit = nullptr;

    LLSpinCtrl*       mPosX = nullptr;
    LLSpinCtrl*       mPosY = nullptr;
    LLSpinCtrl*       mPosZ = nullptr;
    ALCompassDial*    mHeadingDial = nullptr;
    LLSpinCtrl*       mYawSpin = nullptr;
    LLSpinCtrl*       mScaleSpin = nullptr;
    LLCheckBoxCtrl*   mChaosCheck = nullptr;
    LLSliderCtrl*     mChaosSlider = nullptr;
    LLComboBox*       mDriveModeCombo = nullptr;
    LLLineEditor*     mDirectedAnimEdit = nullptr;
    LLButton*         mPlaceBtn = nullptr;
    LLButton*         mToActorBtn = nullptr;
    LLButton*         mToMeBtn = nullptr;

    LLComboBox*       mStyleCombo = nullptr;
    LLCheckBoxCtrl*   mActorTintCheck = nullptr;
    LLSliderCtrl*     mHueSlider = nullptr;
    LLSliderCtrl*     mAlphaSlider = nullptr;
    LLSliderCtrl*     mShimmerSpeedSlider = nullptr;
    LLSliderCtrl*     mShimmerAmountSlider = nullptr;
    LLSliderCtrl*     mPixelSlider = nullptr;
    LLSliderCtrl*     mGlitchSlider = nullptr;
    LLSliderCtrl*     mBrightnessSlider = nullptr;    // [R2-1]
    LLComboBox*       mEntityLookCombo = nullptr;

    LLButton*         mFreezeBtn = nullptr;
    LLButton*         mLiveBtn = nullptr;
    LLTextBox*        mPoseStatus = nullptr;

    LLSpinCtrl*       mArrayCount = nullptr;
    LLSpinCtrl*       mArraySpacing = nullptr;
    LLButton*         mArrayLineBtn = nullptr;
    LLButton*         mArrayRingBtn = nullptr;

    LLTextBox*        mStatusText = nullptr;
};

#endif // AL_ALPANELGHOSTSTUDIO_H
