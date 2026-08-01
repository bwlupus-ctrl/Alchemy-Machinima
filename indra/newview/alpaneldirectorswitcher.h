/**
 * @file alpaneldirectorswitcher.h
 * @brief Shared Director camera-switcher panel.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 *
 * The Director Console embeds this panel through panel_director_switcher.xml.
 * It is deliberately self-contained: settings-backed controls bind directly
 * to gSavedSettings, slot edits round-trip the switcher's LLSD bank, and draw()
 * mirrors the singleton's live program state. A future standalone shell can
 * therefore embed the same panel without duplicating any wiring.
 */

#ifndef AL_ALPANELDIRECTORSWITCHER_H
#define AL_ALPANELDIRECTORSWITCHER_H

#include "aldirectorswitcher.h"
#include "llpanel.h"

#include <array>
#include <vector>

class LLButton;
class LLCheckBoxCtrl;
class LLComboBox;
class LLLineEditor;
class LLSpinCtrl;
class LLTextBox;

class ALPanelDirectorSwitcher final : public LLPanel
{
public:
    ALPanelDirectorSwitcher();
    ~ALPanelDirectorSwitcher() override;

    bool postBuild() override;
    void draw() override;

private:
    using Slot = ALDirectorSwitcher::Slot;
    using Bank = std::vector<Slot>;

    static Bank loadNormalizedBank();
    static bool banksEqual(const Bank& lhs, const Bank& rhs);
    static std::string displayLabel(const Slot& slot);

    void onSlotButton(S32 slot);
    void onSlotEnabled();
    void onSlotMode();
    void onSlotLabel();
    void onSlotSubject();
    void onCustomEnabled();
    void onCustomValue();
    void onCustomReset(S32 field);
    void onCaptureCurrentView();
    void onResetSlots();
    void saveBank(bool retake_custom = false);

    void refreshBank(bool force);
    void refreshButtons();
    void refreshEditor();
    void refreshProgramState();
    void updateCurveControlsEnabled();

    std::array<LLButton*, ALDirectorSwitcher::SLOT_COUNT> mSlotButtons{};
    LLCheckBoxCtrl* mSlotEnabled = nullptr;
    LLComboBox*     mSlotMode = nullptr;
    LLComboBox*     mPrimarySubject = nullptr;
    LLComboBox*     mSecondarySubject = nullptr;
    LLLineEditor*   mSlotLabel = nullptr;
    LLCheckBoxCtrl* mCustomEnabled = nullptr;
    LLSpinCtrl*     mCustomYaw = nullptr;
    LLSpinCtrl*     mCustomPitch = nullptr;
    LLSpinCtrl*     mCustomDistance = nullptr;
    LLSpinCtrl*     mCustomHeight = nullptr;
    LLSpinCtrl*     mCustomFov = nullptr;
    LLButton*       mCaptureCurrent = nullptr;
    LLTextBox*      mCaptureStatus = nullptr;
    LLTextBox*      mSelectedSlotText = nullptr;
    LLTextBox*      mProgramText = nullptr;

    Bank mBank;
    S32  mSelectedSlot = 0;

    // draw()-rate diffing: auto-director can change the program without a UI
    // callback, while the slot bank can change through another panel instance.
    S32  mLastActiveSlot = -2;
    bool mLastDriving = false;
    bool mLastArmed = false;
    bool mHaveProgramSnapshot = false;
    bool mRefreshing = false;
    boost::signals2::scoped_connection mEaseCurveConnection;
    boost::signals2::scoped_connection mFreezeCurveConnection;
};

#endif // AL_ALPANELDIRECTORSWITCHER_H
