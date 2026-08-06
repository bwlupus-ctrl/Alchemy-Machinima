/**
 * @file alpaneldirectoranimswitcher.cpp
 * @brief Shared Director Animation Switchboard panel -- see the header.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "alpaneldirectoranimswitcher.h"

#include "animationexplorer.h"      // AnimationExplorer::getSelectedAnimId()
#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "llcombobox.h"
#include "llfloaterreg.h"
#include "llinventory.h"            // LLInventoryItem (drag-and-drop cargo)
#include "lllineeditor.h"
#include "llspinctrl.h"
#include "llstring.h"
#include "lltextbox.h"
#include "lluuid.h"
#include "llviewercontrol.h"

// The injector name must match class="panel_director_anim_switcher" in every
// host. Without the Director Console embed nothing instantiates this panel.
static LLPanelInjector<ALPanelDirectorAnimSwitcher>
    t_panel_director_anim_switcher("panel_director_anim_switcher");

namespace
{
constexpr S32 INVALID_SLOT = -1;

bool valid_slot(S32 slot)
{
    return slot >= 0 && slot < ALDirectorAnimSwitcher::SLOT_COUNT;
}
} // anonymous namespace

ALPanelDirectorAnimSwitcher::ALPanelDirectorAnimSwitcher() = default;
ALPanelDirectorAnimSwitcher::~ALPanelDirectorAnimSwitcher() = default;

bool ALPanelDirectorAnimSwitcher::postBuild()
{
    for (S32 slot = 0; slot < ALDirectorAnimSwitcher::SLOT_COUNT; ++slot)
    {
        const std::string name = llformat("anim_slot_%02d", slot + 1);
        mSlotButtons[slot] = getChild<LLButton>(name);
        mSlotButtons[slot]->setCommitCallback(
            [this, slot](LLUICtrl*, const LLSD&) { onSlotButton(slot); });
    }

    mAnimUUID = getChild<LLLineEditor>("anim_slot_uuid");
    mFromExplorer = getChild<LLButton>("anim_from_explorer");
    mSlotLabel = getChild<LLLineEditor>("anim_slot_label");
    mTargetCombo = getChild<LLComboBox>("anim_slot_target");
    mPriorityCombo = getChild<LLComboBox>("anim_slot_priority");
    mSpeedSpin = getChild<LLSpinCtrl>("anim_slot_speed");
    mLoopCheck = getChild<LLCheckBoxCtrl>("anim_slot_loop");
    mSnapCheck = getChild<LLCheckBoxCtrl>("anim_slot_snap");
    mSlotEnabled = getChild<LLCheckBoxCtrl>("anim_slot_enabled");
    mResetSlot = getChild<LLButton>("anim_slot_reset");
    mSelectedSlotText = getChild<LLTextBox>("anim_selected_slot");
    mProgramText = getChild<LLTextBox>("anim_program_status");
    mImportStatus = getChild<LLTextBox>("anim_import_status");

    mAnimUUID->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onAnimUUID(); });
    mFromExplorer->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onFromExplorer(); });
    mSlotLabel->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onSlotLabel(); });
    mTargetCombo->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onSlotTarget(); });
    mPriorityCombo->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onSlotPriority(); });
    mSpeedSpin->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onSlotSpeed(); });
    mLoopCheck->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onSlotLoop(); });
    mSnapCheck->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onSlotSnap(); });
    mSlotEnabled->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onSlotEnabled(); });
    mResetSlot->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onResetSlot(); });

    refreshBank(true);
    refreshProgramState();
    return true;
}

void ALPanelDirectorAnimSwitcher::draw()
{
    // A mirrored panel, scene load, or auto cut can replace the LLSD bank or
    // the live program without touching this instance. Twelve small records are
    // cheap to compare while visible.
    refreshBank(false);
    refreshProgramState();
    LLPanel::draw();
}

//static
ALPanelDirectorAnimSwitcher::Bank
ALPanelDirectorAnimSwitcher::loadNormalizedBank()
{
    Bank bank = ALDirectorAnimSwitcher::loadBank();
    if (bank.size() > (size_t)ALDirectorAnimSwitcher::SLOT_COUNT)
    {
        bank.resize(ALDirectorAnimSwitcher::SLOT_COUNT);
    }
    while (bank.size() < (size_t)ALDirectorAnimSwitcher::SLOT_COUNT)
    {
        Slot fallback;
        fallback.mEnabled = true;
        fallback.mLabel = llformat("Anim %d", (S32)bank.size() + 1);
        bank.push_back(std::move(fallback));
    }
    return bank;
}

//static
bool ALPanelDirectorAnimSwitcher::banksEqual(const Bank& lhs, const Bank& rhs)
{
    if (lhs.size() != rhs.size())
    {
        return false;
    }
    for (size_t i = 0; i < lhs.size(); ++i)
    {
        if (lhs[i].mEnabled != rhs[i].mEnabled ||
            lhs[i].mAnimID != rhs[i].mAnimID ||
            lhs[i].mLabel != rhs[i].mLabel ||
            lhs[i].mTarget != rhs[i].mTarget ||
            lhs[i].mPriority != rhs[i].mPriority ||
            lhs[i].mSpeed != rhs[i].mSpeed ||
            lhs[i].mLoop != rhs[i].mLoop ||
            lhs[i].mSnapOnCut != rhs[i].mSnapOnCut)
        {
            return false;
        }
    }
    return true;
}

//static
std::string ALPanelDirectorAnimSwitcher::displayLabel(const Slot& slot)
{
    if (!slot.mLabel.empty())
    {
        return slot.mLabel;
    }
    if (slot.mAnimID.notNull())
    {
        return slot.mAnimID.asString().substr(0, 8);
    }
    return "(empty)";
}

void ALPanelDirectorAnimSwitcher::onSlotButton(S32 slot)
{
    if (!valid_slot(slot))
    {
        return;
    }

    mSelectedSlot = slot;
    mImportStatus->setText(LLStringExplicit(""));
    refreshEditor();

    // Disarmed buttons remain useful for bank editing, but cannot punch. Slot
    // enabled is intentionally not checked here: it only filters auto.
    if (gSavedSettings.getBOOL("DirectorAnimSwitcherArmed"))
    {
        ALDirectorAnimSwitcher::instance().punch(slot);
    }
    // Toggle buttons mutate their visual state before invoking the callback.
    // Force a full program mirror even when the attempted punch was a no-op.
    mHaveProgramSnapshot = false;
    refreshProgramState();
}

void ALPanelDirectorAnimSwitcher::onSlotEnabled()
{
    if (mRefreshing || !valid_slot(mSelectedSlot) ||
        mBank.size() != (size_t)ALDirectorAnimSwitcher::SLOT_COUNT)
    {
        return;
    }
    mBank[mSelectedSlot].mEnabled = mSlotEnabled->getValue().asBoolean();
    saveBank();
}

void ALPanelDirectorAnimSwitcher::onAnimUUID()
{
    if (mRefreshing || !valid_slot(mSelectedSlot) ||
        mBank.size() != (size_t)ALDirectorAnimSwitcher::SLOT_COUNT)
    {
        return;
    }
    std::string text = mAnimUUID->getText();
    LLStringUtil::trim(text);
    LLUUID id;
    if (!text.empty() && LLUUID::validate(text))
    {
        id.set(text, false);
        mImportStatus->setText(LLStringExplicit("Animation UUID set"));
    }
    else if (text.empty())
    {
        mImportStatus->setText(LLStringExplicit("Cleared"));
    }
    else
    {
        mImportStatus->setText(LLStringExplicit("Not a valid UUID"));
        return; // leave the stored id untouched on a typo
    }
    mBank[mSelectedSlot].mAnimID = id;
    saveBank();
}

void ALPanelDirectorAnimSwitcher::onSlotLabel()
{
    if (mRefreshing || !valid_slot(mSelectedSlot) ||
        mBank.size() != (size_t)ALDirectorAnimSwitcher::SLOT_COUNT)
    {
        return;
    }
    mBank[mSelectedSlot].mLabel = mSlotLabel->getText();
    saveBank();
}

void ALPanelDirectorAnimSwitcher::onSlotTarget()
{
    if (mRefreshing || !valid_slot(mSelectedSlot) ||
        mBank.size() != (size_t)ALDirectorAnimSwitcher::SLOT_COUNT)
    {
        return;
    }
    mBank[mSelectedSlot].mTarget = mTargetCombo->getValue().asInteger();
    saveBank();
}

void ALPanelDirectorAnimSwitcher::onSlotPriority()
{
    if (mRefreshing || !valid_slot(mSelectedSlot) ||
        mBank.size() != (size_t)ALDirectorAnimSwitcher::SLOT_COUNT)
    {
        return;
    }
    mBank[mSelectedSlot].mPriority = mPriorityCombo->getValue().asInteger();
    saveBank();
}

void ALPanelDirectorAnimSwitcher::onSlotSpeed()
{
    if (mRefreshing || !valid_slot(mSelectedSlot) ||
        mBank.size() != (size_t)ALDirectorAnimSwitcher::SLOT_COUNT)
    {
        return;
    }
    mBank[mSelectedSlot].mSpeed =
        static_cast<F32>(mSpeedSpin->getValue().asReal());
    saveBank();
}

void ALPanelDirectorAnimSwitcher::onSlotLoop()
{
    if (mRefreshing || !valid_slot(mSelectedSlot) ||
        mBank.size() != (size_t)ALDirectorAnimSwitcher::SLOT_COUNT)
    {
        return;
    }
    mBank[mSelectedSlot].mLoop = mLoopCheck->getValue().asBoolean();
    saveBank();
}

void ALPanelDirectorAnimSwitcher::onSlotSnap()
{
    if (mRefreshing || !valid_slot(mSelectedSlot) ||
        mBank.size() != (size_t)ALDirectorAnimSwitcher::SLOT_COUNT)
    {
        return;
    }
    mBank[mSelectedSlot].mSnapOnCut = mSnapCheck->getValue().asBoolean();
    saveBank();
}

void ALPanelDirectorAnimSwitcher::onFromExplorer()
{
    if (!valid_slot(mSelectedSlot) ||
        mBank.size() != (size_t)ALDirectorAnimSwitcher::SLOT_COUNT)
    {
        return;
    }
    AnimationExplorer* explorer =
        LLFloaterReg::findTypedInstance<AnimationExplorer>(
            "animation_explorer");
    if (!explorer)
    {
        mImportStatus->setText(
            LLStringExplicit("Open the Animation Explorer first"));
        return;
    }
    const LLUUID id = explorer->getSelectedAnimId();
    if (id.isNull())
    {
        mImportStatus->setText(
            LLStringExplicit("No animation selected in the Explorer"));
        return;
    }
    mBank[mSelectedSlot].mAnimID = id;
    mImportStatus->setText(
        LLStringExplicit("Imported from Animation Explorer"));
    saveBank();
}

void ALPanelDirectorAnimSwitcher::onResetSlot()
{
    if (!valid_slot(mSelectedSlot) ||
        mBank.size() != (size_t)ALDirectorAnimSwitcher::SLOT_COUNT)
    {
        return;
    }
    Slot fresh;
    fresh.mLabel = llformat("Anim %d", mSelectedSlot + 1);
    mBank[mSelectedSlot] = fresh;
    mImportStatus->setText(LLStringExplicit("Slot cleared to defaults"));
    saveBank();
}

void ALPanelDirectorAnimSwitcher::saveBank()
{
    ALDirectorAnimSwitcher::saveBank(mBank);
    // Reload the canonical clamped representation before reflecting it.
    mBank = loadNormalizedBank();
    refreshButtons();
    refreshEditor();
    // The active program may be this slot, so its displayed label can change
    // even though the engine's active-slot index did not.
    mHaveProgramSnapshot = false;
}

void ALPanelDirectorAnimSwitcher::refreshBank(bool force)
{
    Bank latest = loadNormalizedBank();
    if (!force && banksEqual(latest, mBank))
    {
        return;
    }
    mBank = std::move(latest);
    refreshButtons();
    refreshEditor();
    mHaveProgramSnapshot = false;
}

void ALPanelDirectorAnimSwitcher::refreshButtons()
{
    if (mBank.size() != (size_t)ALDirectorAnimSwitcher::SLOT_COUNT)
    {
        return;
    }
    for (S32 slot = 0; slot < ALDirectorAnimSwitcher::SLOT_COUNT; ++slot)
    {
        LLButton* button = mSlotButtons[slot];
        if (!button)
        {
            continue;
        }
        const Slot& entry = mBank[slot];
        const std::string label = displayLabel(entry);
        button->setLabel(LLStringExplicit(
            llformat("%d %s", slot + 1, label.c_str())));
        button->setToolTip(LLStringExplicit(llformat(
            "Slot %d: %s. Click to select it; while armed, also punch it. "
            "Use in auto only controls automatic selection.",
            slot + 1, label.c_str())));
    }
}

void ALPanelDirectorAnimSwitcher::refreshEditor()
{
    if (!valid_slot(mSelectedSlot) ||
        mBank.size() != (size_t)ALDirectorAnimSwitcher::SLOT_COUNT)
    {
        return;
    }

    const Slot& slot = mBank[mSelectedSlot];
    mRefreshing = true;
    mSelectedSlotText->setText(
        LLStringExplicit(llformat("Slot %d setup", mSelectedSlot + 1)));
    mAnimUUID->setText(LLStringExplicit(
        slot.mAnimID.notNull() ? slot.mAnimID.asString() : std::string()));
    mSlotLabel->setText(LLStringExplicit(slot.mLabel));
    mTargetCombo->setValue(LLSD(slot.mTarget));
    mPriorityCombo->setValue(LLSD(slot.mPriority));
    mSpeedSpin->setValue(LLSD((F64)slot.mSpeed));
    mLoopCheck->setValue(slot.mLoop);
    mSnapCheck->setValue(slot.mSnapOnCut);
    mSlotEnabled->setValue(slot.mEnabled);
    // Priority and Snap apply to the self path; Speed and Loop apply to ghost
    // clones. Both stay editable regardless of target so a slot can be authored
    // before its subject is cast.
    mRefreshing = false;
}

void ALPanelDirectorAnimSwitcher::refreshProgramState()
{
    ALDirectorAnimSwitcher& switcher = ALDirectorAnimSwitcher::instance();
    const bool armed = gSavedSettings.getBOOL("DirectorAnimSwitcherArmed");
    const S32 active = armed ? switcher.activeSlot() : INVALID_SLOT;

    if (mHaveProgramSnapshot && armed == mLastArmed && active == mLastActiveSlot)
    {
        return;
    }

    for (S32 slot = 0; slot < ALDirectorAnimSwitcher::SLOT_COUNT; ++slot)
    {
        if (mSlotButtons[slot])
        {
            mSlotButtons[slot]->setToggleState(active == slot);
        }
    }

    if (armed && valid_slot(active) &&
        mBank.size() == (size_t)ALDirectorAnimSwitcher::SLOT_COUNT)
    {
        mProgramText->setText(LLStringExplicit(llformat(
            "PROGRAM  %d  %s", active + 1,
            displayLabel(mBank[active]).c_str())));
    }
    else if (armed)
    {
        mProgramText->setText(
            LLStringExplicit("Armed - click a slot to punch it"));
    }
    else
    {
        mProgramText->setText(
            LLStringExplicit("Disarmed - slot buttons select for editing only"));
    }

    mLastArmed = armed;
    mLastActiveSlot = active;
    mHaveProgramSnapshot = true;
}

bool ALPanelDirectorAnimSwitcher::handleDragAndDrop(
    S32 x, S32 y, MASK mask, bool drop, EDragAndDropType cargo_type,
    void* cargo_data, EAcceptance* accept, std::string& tooltip_msg)
{
    if (cargo_type != DAD_ANIMATION)
    {
        return LLPanel::handleDragAndDrop(
            x, y, mask, drop, cargo_type, cargo_data, accept, tooltip_msg);
    }

    if (!valid_slot(mSelectedSlot) ||
        mBank.size() != (size_t)ALDirectorAnimSwitcher::SLOT_COUNT)
    {
        *accept = ACCEPT_NO;
        return true;
    }

    *accept = ACCEPT_YES_SINGLE;
    if (drop && cargo_data)
    {
        LLInventoryItem* item = static_cast<LLInventoryItem*>(cargo_data);
        mBank[mSelectedSlot].mAnimID = item->getAssetUUID();
        if (mBank[mSelectedSlot].mLabel.empty())
        {
            mBank[mSelectedSlot].mLabel = item->getName();
        }
        mImportStatus->setText(
            LLStringExplicit("Dropped animation into the slot"));
        saveBank();
    }
    return true;
}
