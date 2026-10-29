/**
 * @file alpaneldirectoranimswitcher.h
 * @brief Shared Director Animation Switchboard panel.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 *
 * The Director Console embeds this panel through
 * panel_director_anim_switcher.xml. Like the camera switcher panel it is
 * self-contained: the arm/auto/scheduler controls bind directly to
 * gSavedSettings, slot edits round-trip the switchboard's LLSD bank, and draw()
 * mirrors the singleton's live program state so a mirrored panel, scene load, or
 * auto cut is always reflected.
 */

#ifndef AL_ALPANELDIRECTORANIMSWITCHER_H
#define AL_ALPANELDIRECTORANIMSWITCHER_H

#include "aldirectoranimswitcher.h"
#include "llpanel.h"

#include <array>
#include <vector>

class LLButton;
class LLCheckBoxCtrl;
class LLComboBox;
class LLLineEditor;
class LLSpinCtrl;
class LLTextBox;

class ALPanelDirectorAnimSwitcher final : public LLPanel
{
public:
    ALPanelDirectorAnimSwitcher();
    ~ALPanelDirectorAnimSwitcher() override;

    bool postBuild() override;
    void draw() override;

    // Inventory drag-and-drop of an animation asset into the selected slot.
    bool handleDragAndDrop(S32 x, S32 y, MASK mask, bool drop,
                           EDragAndDropType cargo_type, void* cargo_data,
                           EAcceptance* accept,
                           std::string& tooltip_msg) override;

private:
    using Slot = ALDirectorAnimSwitcher::Slot;
    using Bank = std::vector<Slot>;

    static Bank loadNormalizedBank();
    static bool banksEqual(const Bank& lhs, const Bank& rhs);
    static std::string displayLabel(const Slot& slot);

    void onSlotButton(S32 slot);
    void onSlotEnabled();
    void onSlotKind();
    void onAnimUUID();
    void onPoseName();
    void onPoseLoadMethod();
    void onSlotLabel();
    void onSlotTarget();
    void onSlotPriority();
    void onSlotSpeed();
    void onSlotLoop();
    void onSlotSnap();
    void onFromExplorer();
    void onResetSlot();
    void saveBank();

    void refreshBank(bool force);
    void refreshPoseChoices(const std::string& selected_pose);
    void refreshButtons();
    void refreshEditor();
    void refreshProgramState();

    std::array<LLButton*, ALDirectorAnimSwitcher::SLOT_COUNT> mSlotButtons{};
    LLComboBox*     mKindCombo = nullptr;
    LLLineEditor*   mAnimUUID = nullptr;
    LLTextBox*      mAnimUUIDText = nullptr;
    LLButton*       mFromExplorer = nullptr;
    LLComboBox*     mPoseNameCombo = nullptr;
    LLComboBox*     mPoseLoadMethodCombo = nullptr;
    LLTextBox*      mPoseNameText = nullptr;
    LLLineEditor*   mSlotLabel = nullptr;
    LLComboBox*     mTargetCombo = nullptr;
    LLComboBox*     mPriorityCombo = nullptr;
    LLTextBox*      mPriorityText = nullptr;
    LLSpinCtrl*     mSpeedSpin = nullptr;
    LLCheckBoxCtrl* mLoopCheck = nullptr;
    LLCheckBoxCtrl* mSnapCheck = nullptr;
    LLCheckBoxCtrl* mSlotEnabled = nullptr;
    LLButton*       mResetSlot = nullptr;
    LLTextBox*      mSelectedSlotText = nullptr;
    LLTextBox*      mProgramText = nullptr;
    LLTextBox*      mImportStatus = nullptr;

    Bank mBank;
    S32  mSelectedSlot = 0;

    // draw()-rate diffing: the auto scheduler can change the program without a
    // UI callback, and another panel instance can change the shared bank.
    S32  mLastActiveSlot = -2;
    bool mLastArmed = false;
    bool mHaveProgramSnapshot = false;
    bool mRefreshing = false;
};

#endif // AL_ALPANELDIRECTORANIMSWITCHER_H
