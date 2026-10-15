/**
 * @file alpanelsteppedanim.cpp
 * @brief Code-bound controls for Director > Look > Stepped Animation.
 */
#include "llviewerprecompiledheaders.h"

#include "alpanelsteppedanim.h"

#include "alkitbinding.h"
#include "alsteppedanim.h"
#include "alsteppedanimruntime.h"
#include "llagent.h"
#include "llbutton.h"
#include "llcombobox.h"
#include "lldirectorcast.h"
#include "llradiogroup.h"
#include "llrand.h"
#include "llscrolllistctrl.h"
#include "llsliderctrl.h"
#include "lltextbox.h"
#include "llviewercontrol.h"

#include <algorithm>
#include <cmath>

namespace
{
// Cached read for the draw path; the setting is only written from this panel.
const LLSD& groupRates()
{
    static LLCachedControl<LLSD> rates(gSavedSettings, "StepAnimGroupRates");
    return rates;
}

const char* offsetLabel(S32 offset)
{
    switch (offset)
    {
        case 1: return "+1 step";
        case 2: return "+2 steps";
        case 3: return "Smooth";
        default: return "Same";
    }
}
}

static LLPanelInjector<ALPanelSteppedAnim> t_panel_stepped_anim("panel_stepped_anim");

bool ALPanelSteppedAnim::postBuild()
{
    if (!LLPanel::postBuild())
    {
        return false;
    }
    bindSteppedPage(this);
    return true;
}

void ALPanelSteppedAnim::bindSteppedPage(LLView* root)
{
    if (!root || mActorTarget)
    {
        return;
    }
    mActorTarget = root->findChild<LLComboBox>("sa_actor_target", true);
    mOverrideList = root->findChild<LLScrollListCtrl>("sa_override_list", true);
    mActorOverride = root->findChild<LLComboBox>("sa_actor_override", true);
    mActorRate = root->findChild<LLSliderCtrl>("sa_actor_rate", true);
    mActorSteps = root->findChild<LLComboBox>("sa_actor_steps", true);
    mActorRole = root->findChild<LLComboBox>("sa_actor_role", true);
    mActorOffset = root->findChild<LLComboBox>("sa_actor_offset", true);
    mActorEffective = root->findChild<LLTextBox>("sa_actor_effective", true);
    mSeedReroll = root->findChild<LLButton>("sa_seed_reroll", true);
    mGroupPick = root->findChild<LLComboBox>("sa_group_pick", true);
    mGroupOffset = root->findChild<LLComboBox>("sa_group_offset", true);
    mGroupList = root->findChild<LLScrollListCtrl>("sa_group_list", true);
    mOverrideClear = root->findChild<LLButton>("sa_override_clear", true);
    mRateChips = root->findChild<LLRadioGroup>("sa_rate_chips", true);
    mStatus = root->findChild<LLTextBox>("sa_status", true);
    mRateReadout = root->findChild<LLTextBox>("sa_rate_readout", true);
    mSpeedMeter = root->findChild<LLTextBox>("sa_speed_meter", true);

    auto row = [root](const char* name) -> LLView*
    {
        LLView* control = root->findChild<LLView>(name, true);
        return control ? control->getParent() : nullptr;
    };
    mAdaptiveRows[0] = row("sa_fast_rate");
    mAdaptiveRows[1] = row("sa_adapt_threshold");
    mAdaptiveRows[2] = row("sa_adapt_hysteresis");
    mAdaptiveRows[3] = row("sa_adapt_dwell");
    mAdaptiveRows[4] = row("sa_speed_meter");
    mMixedRows[0] = row("sa_background_rate");
    mMixedRows[1] = row("sa_actor_role");
    mManualPhaseRow = row("sa_phase");
    mRadiusRow = row("sa_nearby_radius");
    mActorOwnRows[0] = row("sa_actor_rate");
    mActorOwnRows[1] = row("sa_actor_steps");

    if (mActorTarget) mActorTarget->setCommitCallback([this](LLUICtrl*, const LLSD&) { onActorTargetCommit(); });
    if (mOverrideList) mOverrideList->setCommitCallback([this](LLUICtrl*, const LLSD&) { onOverrideListSelect(); });
    if (mActorOverride) mActorOverride->setCommitCallback([this](LLUICtrl*, const LLSD&) { onActorOverrideCommit(); });
    if (mActorRate) mActorRate->setCommitCallback([this](LLUICtrl*, const LLSD&) { onActorRateCommit(); });
    if (mActorSteps) mActorSteps->setCommitCallback([this](LLUICtrl*, const LLSD&) { onActorStepsCommit(); });
    if (mActorRole) mActorRole->setCommitCallback([this](LLUICtrl*, const LLSD&) { onActorRoleCommit(); });
    if (mOverrideClear) mOverrideClear->setCommitCallback([this](LLUICtrl*, const LLSD&) { onOverrideClear(); });
    if (mActorOffset) mActorOffset->setCommitCallback([this](LLUICtrl*, const LLSD&) { onActorOffsetCommit(); });
    if (mSeedReroll) mSeedReroll->setCommitCallback([this](LLUICtrl*, const LLSD&) { onSeedReroll(); });
    if (mGroupPick) mGroupPick->setCommitCallback([this](LLUICtrl*, const LLSD&) { onGroupPick(); });
    if (mGroupOffset) mGroupOffset->setCommitCallback([this](LLUICtrl*, const LLSD&) { onGroupOffsetCommit(); });
    if (mGroupList) mGroupList->setCommitCallback([this](LLUICtrl*, const LLSD&) { onGroupListSelect(); });
    if (mRateChips) mRateChips->setCommitCallback([this](LLUICtrl*, const LLSD&) { onRateChip(); });

    ALKitWhoAdapters::add("stepped_anim", [](const LLUUID& id)
    {
        const LLUUID actor = LLDirectorCast::canonicalActorId(id);
        if (actor.isNull() || actor == gAgentID)
        {
            return LLSD(LLUUID::null.asString());
        }
        return LLDirectorCast::instance().contains(actor) ? LLSD(actor.asString()) : LLSD();
    });
    rebuildActors();
    refreshActor();
    syncGroups(true);
}

LLUUID ALPanelSteppedAnim::selectedActor() const
{
    return mActorTarget ? LLUUID(mActorTarget->getSelectedValue().asString())
                        : LLDirectorCast::instance().getSteppedActorId();
}

// ui-draw-lint: allow-function(raw-write) draw() calls this only when the cast revision changed
void ALPanelSteppedAnim::rebuildActors()
{
    if (!mActorTarget || !mOverrideList)
    {
        return;
    }
    LLDirectorCast& cast = LLDirectorCast::instance();
    const LLUUID selected = cast.getSteppedActorId();
    mActorTarget->removeall();
    mOverrideList->deleteAllItems();
    const auto add_actor = [this](const LLUUID& id, const std::string& name, const LLDirectorCast::ActorStyle& style)
    {
        mActorTarget->add(name, id.asString());
        const char* mode = style.mStepped.mMode == 2 ? "Own" : (style.mStepped.mMode == 1 ? "Never" : "Inherit");
        LLSD row;
        row["id"] = id;
        row["columns"][0]["column"] = "name";
        row["columns"][0]["value"] = name;
        row["columns"][1]["column"] = "value";
        row["columns"][1]["value"] = mode;
        mOverrideList->addElement(row, ADD_BOTTOM);
    };
    for (const LLDirectorCast::ActorChoice& choice : cast.getActorChoices())
    {
        const LLUUID value = LLDirectorCast::isSelf(choice.mId)
            ? LLUUID::null : choice.mId;
        add_actor(value, choice.mLabel, cast.getActorStyle(value));
    }
    mActorTarget->setValue(selected.asString());
    mCastRevision = cast.getActorChoiceRevision();
}

// ui-draw-lint: allow-function(raw-write) draw() calls this only when the cast revision changed
void ALPanelSteppedAnim::refreshActor()
{
    const LLDirectorCast::ActorStyle& style = LLDirectorCast::instance().getActorStyle(selectedActor());
    mRefreshing = true;
    if (mActorOverride) mActorOverride->setCurrentByIndex(style.mStepped.mMode);
    if (mActorRate) mActorRate->setValue(style.mStepped.mRate);
    if (mActorSteps) mActorSteps->setCurrentByIndex(style.mStepped.mWhat + 1);
    if (mActorRole) mActorRole->setCurrentByIndex(style.mStepped.mRole);
    if (mActorOffset) mActorOffset->setCurrentByIndex(style.mStepped.mRateOffset + 1);
    mRefreshing = false;
    refreshConditionalRows();
}

void ALPanelSteppedAnim::writeActor()
{
    if (mRefreshing)
    {
        return;
    }
    LLDirectorCast& cast = LLDirectorCast::instance();
    const LLUUID id = selectedActor();
    LLDirectorCast::ActorStyle style = cast.getActorStyle(id);
    if (mActorOverride) style.mStepped.mMode = llclamp(mActorOverride->getCurrentIndex(), 0, 2);
    if (mActorRate) style.mStepped.mRate = llclamp((F32)mActorRate->getValue().asReal(), 2.f, 30.f);
    if (mActorSteps) style.mStepped.mWhat = llclamp(mActorSteps->getCurrentIndex() - 1, -1, 1);
    if (mActorRole) style.mStepped.mRole = llclamp(mActorRole->getCurrentIndex(), 0, 2);
    if (mActorOffset) style.mStepped.mRateOffset = llclamp(mActorOffset->getCurrentIndex() - 1, -1, 2);
    cast.setActorStyle(id, style);
    rebuildActors();
    refreshActor();
}

void ALPanelSteppedAnim::onActorTargetCommit()
{
    LLDirectorCast::instance().setSteppedActorId(selectedActor());
    refreshActor();
}

void ALPanelSteppedAnim::onOverrideListSelect()
{
    if (!mOverrideList || !mActorTarget)
    {
        return;
    }
    const LLUUID id = mOverrideList->getSelectedValue().asUUID();
    mActorTarget->setValue(id.asString());
    onActorTargetCommit();
}

void ALPanelSteppedAnim::onActorOverrideCommit() { writeActor(); }
void ALPanelSteppedAnim::onActorRateCommit() { writeActor(); }
void ALPanelSteppedAnim::onActorStepsCommit() { writeActor(); }
void ALPanelSteppedAnim::onActorRoleCommit() { writeActor(); }
void ALPanelSteppedAnim::onActorOffsetCommit() { writeActor(); }

// A new random seed re-rolls every actor's phase, boil, rate variation and timing. Kept in
// 1..99999 so the Seed spinner shows it exactly; viewer-local like every Stepped setting.
void ALPanelSteppedAnim::onSeedReroll()
{
    gSavedSettings.setU32("StepAnimSeed", static_cast<U32>(1 + ll_rand(99999)));
}

std::string ALPanelSteppedAnim::selectedGroup() const
{
    return mGroupPick ? mGroupPick->getSelectedValue().asString() : std::string();
}

void ALPanelSteppedAnim::onGroupPick() { refreshGroupOffset(); }

void ALPanelSteppedAnim::onGroupListSelect()
{
    if (!mGroupList || !mGroupPick) return;
    const std::string name = mGroupList->getSelectedValue().asString();
    if (name.empty()) return;
    mGroupPick->setValue(name);
    refreshGroupOffset();
}

void ALPanelSteppedAnim::onGroupOffsetCommit()
{
    if (mRefreshing || !mGroupOffset) return;
    const std::string name = selectedGroup();
    if (name.empty()) return;
    LLSD rates = gSavedSettings.getLLSD("StepAnimGroupRates");
    if (!rates.isMap()) rates = LLSD::emptyMap();
    const S32 offset = llclamp(mGroupOffset->getCurrentIndex(), 0, 3);
    if (offset == 0) rates.erase(name); else rates[name] = offset;
    gSavedSettings.setLLSD("StepAnimGroupRates", rates);
    syncGroups(true);
}

// ui-draw-lint: allow-function(raw-write) runs on a click, a group change or the 2 Hz poll only
void ALPanelSteppedAnim::refreshGroupOffset()
{
    if (!mGroupOffset) return;
    const LLSD& rates = groupRates();
    const std::string name = selectedGroup();
    mRefreshing = true;
    mGroupOffset->setCurrentByIndex(name.empty() || !rates.has(name) ? 0 : llclamp(rates[name].asInteger(), 0, 3));
    mRefreshing = false;
}

// ui-draw-lint: allow-function(raw-write) rebuilds only when the tag set or a rate changed (2 Hz poll)
void ALPanelSteppedAnim::syncGroups(bool force)
{
    if (!mGroupPick || !mGroupList) return;
    if (!force && mGroupTimer.getElapsedTimeF32() < 0.5f) return;
    mGroupTimer.reset();
    const LLSD& rates = groupRates();
    const std::vector<std::string> names = LLDirectorCast::instance().getGroupNames();
    std::string signature;
    for (const std::string& name : names)
        signature += name + "=" + std::to_string(rates.has(name) ? rates[name].asInteger() : 0) + ";";
    if (!force && signature == mGroupListSignature) return;
    const bool same_names = names == mGroupNames;
    mGroupListSignature = signature;
    mGroupNames = names;
    mRefreshing = true;
    if (!same_names)
    {
        const std::string keep = selectedGroup();
        mGroupPick->removeall();
        for (const std::string& name : names) mGroupPick->add(name, name);
        if (!names.empty())
            mGroupPick->setValue(std::find(names.begin(), names.end(), keep) != names.end() ? keep : names.front());
    }
    mGroupList->deleteAllItems();
    for (const std::string& name : names)
    {
        LLSD row;
        row["id"] = name;
        row["columns"][0]["column"] = "name";
        row["columns"][0]["value"] = name;
        row["columns"][1]["column"] = "value";
        row["columns"][1]["value"] = offsetLabel(rates.has(name) ? llclamp(rates[name].asInteger(), 0, 3) : 0);
        mGroupList->addElement(row, ADD_BOTTOM);
    }
    mRefreshing = false;
    refreshGroupOffset();
}

void ALPanelSteppedAnim::onOverrideClear()
{
    LLDirectorCast& cast = LLDirectorCast::instance();
    LLDirectorCast::ActorStyle style = cast.getActorStyle(selectedActor());
    style.mStepped = LLDirectorCast::ActorStyle::SteppedOverride();
    cast.setActorStyle(selectedActor(), style);
    rebuildActors();
    refreshActor();
}

void ALPanelSteppedAnim::onRateChip()
{
    if (!mRateChips)
    {
        return;
    }
    const S32 divisor = mRateChips->getSelectedIndex() + 1;
    if (divisor > 0)
    {
        const S32 base = gSavedSettings.getS32("StepAnimBaseFPS");
        gSavedSettings.setF32("StepAnimRate", (F32)base / (F32)divisor);
    }
}

void ALPanelSteppedAnim::refreshConditionalRows()
{
    static LLCachedControl<S32> mode_setting(gSavedSettings, "StepAnimMode");
    static LLCachedControl<S32> offset_setting(gSavedSettings, "StepAnimOffsetMode");
    const S32 mode = llclamp((S32)mode_setting, 0, 2);
    for (LLView* row : mAdaptiveRows) if (row) row->setVisible(mode == 1);
    for (LLView* row : mMixedRows) if (row) row->setVisible(mode == 2);
    if (mManualPhaseRow) mManualPhaseRow->setVisible((S32)offset_setting == 2);
    static LLCachedControl<S32> scope_setting(gSavedSettings, "StepAnimScope");
    static LLCachedControl<bool> limit_setting(gSavedSettings, "StepAnimLimitDistance");
    if (mRadiusRow) mRadiusRow->setVisible((S32)scope_setting == ALSteppedAnim::SCOPE_NEARBY || limit_setting);
    const bool own = mActorOverride && mActorOverride->getCurrentIndex() == 2;
    for (LLView* row : mActorOwnRows) if (row) row->setVisible(own);
}

// ui-draw-lint: allow-function(raw-write) writes only when the effective-rate inputs changed
void ALPanelSteppedAnim::refreshActorEffective()
{
    if (!mActorEffective) return;
    static LLCachedControl<F32> rate_setting(gSavedSettings, "StepAnimRate");
    static LLCachedControl<S32> base_setting(gSavedSettings, "StepAnimBaseFPS");
    LLDirectorCast& cast = LLDirectorCast::instance();
    const LLUUID id = selectedActor();
    const LLDirectorCast::ActorStyle::SteppedOverride& ov = cast.getActorStyle(id).mStepped;
    const std::string group = cast.getGroup(id.isNull() ? gAgentID : id);
    S32 group_offset = 0;
    if (!group.empty())
    {
        const LLSD& rates = groupRates();
        if (rates.has(group)) group_offset = llclamp(rates[group].asInteger(), 0, 3);
    }
    const F32 own_rate = ov.mMode == 2 ? ov.mRate : (F32)rate_setting;
    const std::array<F32, 6> key = {{own_rate, (F32)(S32)base_setting, (F32)ov.mRateOffset,
                                     (F32)group_offset, (F32)ov.mMode, 0.f}};
    if (key == mEffectiveKey) return;
    mEffectiveKey = key;
    const S32 offset = ov.mRateOffset >= 0 ? ov.mRateOffset : (ov.mMode == 2 ? 0 : group_offset);
    std::string text;
    if (ov.mMode == 1 || offset == ALSteppedAnim::RATE_SMOOTH)
        text = ov.mMode == 1 ? "Smooth (never stepped)" : "Smooth (group)";
    else
    {
        const F32 rate = ALSteppedAnim::offsetRate(ALSteppedAnim::effectiveRate(own_rate), offset, (S32)base_setting);
        text = llformat("%.1f fps", rate);
        if (offset > 0) text += llformat(" (%s%s)", offsetLabel(offset), ov.mRateOffset >= 0 ? ", own" : ", group");
    }
    mActorEffective->setText(text);
}

void ALPanelSteppedAnim::refreshReadouts()
{
    refreshStatus();
    refreshActorEffective();
    refreshRateReadout();
    refreshSpeedMeter();
}

// ui-draw-lint: allow-function(raw-write) writes only when the active/skipped counts changed
void ALPanelSteppedAnim::refreshStatus()
{
    ALSteppedAnimRuntime& runtime = ALSteppedAnimRuntime::instance();
    const S32 active = runtime.activeActorCount();
    const S32 skipped = runtime.skippedActorCount();
    if (mStatus && (active != mLastActive || skipped != mLastSkipped))
        mStatus->setText(llformat("%d stepped, %d skipped", active, skipped));
    mLastActive = active;
    mLastSkipped = skipped;
}

// ui-draw-lint: allow-function(raw-write) writes only when the cached rate setting changed
void ALPanelSteppedAnim::refreshRateReadout()
{
    static LLCachedControl<F32> rate_setting(gSavedSettings, "StepAnimRate");
    static LLCachedControl<S32> base_setting(gSavedSettings, "StepAnimBaseFPS");
    const F32 rate = rate_setting;
    if (mRateReadout && rate != mLastRate)
        mRateReadout->setText(llformat("%.1f fps", rate));
    if (mRateChips && rate != mLastRate)
    {
        const F32 base = (F32)(S32)base_setting;
        S32 selected = -1;
        for (S32 i = 0; i < 4; ++i)
            if (fabsf(rate - base / (F32)(i + 1)) <= .001f) selected = i;
        mRateChips->setSelectedIndex(selected);
    }
    mLastRate = rate;
}

// ui-draw-lint: allow-function(raw-write) live meter text, throttled to 4 Hz and only on change
void ALPanelSteppedAnim::refreshSpeedMeter()
{
    if (!mSpeedMeter || (mLastSpeed >= 0.f && mSpeedMeterTimer.getElapsedTimeF32() < 0.25f))
        return;
    mSpeedMeterTimer.reset();
    const F32 speed = ll_round(ALSteppedAnimRuntime::instance().selectedLimbSpeed(), 0.01f);
    if (speed != mLastSpeed)
        mSpeedMeter->setText(llformat("%.2f m/s", speed));
    mLastSpeed = speed;
}

void ALPanelSteppedAnim::draw()
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_UI;
    if (LLDirectorCast::instance().getActorChoiceRevision() != mCastRevision)
    {
        rebuildActors();
        refreshActor();
    }
    refreshConditionalRows();
    syncGroups(false);
    refreshReadouts();
    LLPanel::draw();
}
