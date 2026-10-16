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
    mActorPart = root->findChild<LLComboBox>("sa_actor_part", true);
    mActorPartRate = root->findChild<LLComboBox>("sa_actor_part_rate", true);
    mPartFace = root->findChild<LLComboBox>("sa_part_face", true);
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
    if (mActorPart) mActorPart->setCommitCallback([this](LLUICtrl*, const LLSD&) { onActorPartPick(); });
    if (mActorPartRate) mActorPartRate->setCommitCallback([this](LLUICtrl*, const LLSD&) { onActorPartRateCommit(); });
    if (mPartFace) mPartFace->setCommitCallback([this](LLUICtrl*, const LLSD&) { onPartFaceCommit(); });
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
        // An actor that only carries a rate offset or part overrides is not "Inherit": show them.
        const LLDirectorCast::ActorStyle::SteppedOverride& ov = style.mStepped;
        const bool has_parts = std::any_of(ov.mParts.begin(), ov.mParts.end(), [](S32 part) { return part >= 0; });
        const std::string mode = ov.mMode == 2 ? "Own" : (ov.mMode == 1 ? "Never"
            : (ov.mRateOffset >= 0 ? std::string("Rate: ") + offsetLabel(ov.mRateOffset)
                                   : (has_parts ? std::string("Part rates") : std::string("Inherit"))));
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
    // Inherit (-1) is a real, selectable choice: select by value so the combo always shows the
    // selected actor's own setting and never keeps the previous actor's.
    if (mActorSteps) mActorSteps->setValue(LLSD(style.mStepped.mWhat));
    if (mActorRole) mActorRole->setCurrentByIndex(style.mStepped.mRole);
    if (mActorOffset) mActorOffset->setValue(LLSD(style.mStepped.mRateOffset));
    // The Part rate row shows the picked part's own override (Inherit is a real choice, by value).
    if (mActorPartRate) mActorPartRate->setValue(LLSD(style.mStepped.mParts[selectedPart()]));
    mRefreshing = false;
    refreshConditionalRows();
}

// The part picked in the per-actor Part combo (EPartGroup value, never the torso).
S32 ALPanelSteppedAnim::selectedPart() const
{
    const S32 part = mActorPart ? mActorPart->getSelectedValue().asInteger() : ALSteppedAnim::PART_HEAD;
    return llclamp(part, static_cast<S32>(ALSteppedAnim::PART_HEAD), static_cast<S32>(ALSteppedAnim::kPartGroups) - 1);
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
    if (mActorSteps) style.mStepped.mWhat = llclamp(mActorSteps->getSelectedValue().asInteger(), -1, 1);
    if (mActorRole) style.mStepped.mRole = llclamp(mActorRole->getCurrentIndex(), 0, 2);
    if (mActorOffset) style.mStepped.mRateOffset = llclamp(mActorOffset->getSelectedValue().asInteger(), -1, 2);
    // Only the picked part is written; the other parts keep what the scene carries.
    if (mActorPartRate)
        style.mStepped.mParts[selectedPart()] = llclamp(mActorPartRate->getSelectedValue().asInteger(), -1, 3);
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
// Picking another part only re-reads that part's override; it writes nothing.
void ALPanelSteppedAnim::onActorPartPick() { refreshActor(); }
void ALPanelSteppedAnim::onActorPartRateCommit() { writeActor(); }

// The Face part combo is the only Face control: Smooth writes StepAnimFace off (the switch presets
// and scenes carry); Same / +1 / +2 write StepAnimFace on plus StepAnimPartFace.
void ALPanelSteppedAnim::onPartFaceCommit()
{
    if (mRefreshing || !mPartFace) return;
    bool face_on = gSavedSettings.getBOOL("StepAnimFace");
    S32 part_face = gSavedSettings.getS32("StepAnimPartFace");
    ALSteppedAnim::applyFaceControl(mPartFace->getSelectedValue().asInteger(), face_on, part_face);
    gSavedSettings.setS32("StepAnimPartFace", part_face);
    gSavedSettings.setBOOL("StepAnimFace", face_on);
    mLastPartFace = -1; // re-read on the next draw
}

// ui-draw-lint: allow-function(raw-write) selects only when the cached Face settings changed
void ALPanelSteppedAnim::refreshPartFace()
{
    if (!mPartFace) return;
    static LLCachedControl<bool> face_setting(gSavedSettings, "StepAnimFace");
    static LLCachedControl<S32> part_face_setting(gSavedSettings, "StepAnimPartFace");
    // Smooth whenever the engine treats the face as Smooth: StepAnimFace off OR the part is Smooth.
    const S32 shown = ALSteppedAnim::faceControlShown(face_setting, part_face_setting);
    if (shown == mLastPartFace) return;
    mLastPartFace = shown;
    mRefreshing = true;
    mPartFace->setValue(LLSD(shown));
    mRefreshing = false;
}

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
    if (name.empty()) return; // "No cast groups" placeholder: nothing to rate
    // Group rates are cast data (saved with the scene), keyed by the group tag.
    LLDirectorCast::instance().setGroupRate(name, llclamp(mGroupOffset->getCurrentIndex(), 0, 3));
    syncGroups(true);
}

// ui-draw-lint: allow-function(raw-write) runs on a click, a group change or the 2 Hz poll only
void ALPanelSteppedAnim::refreshGroupOffset()
{
    if (!mGroupOffset) return;
    const std::string name = selectedGroup();
    const bool has_group = !name.empty();
    mRefreshing = true;
    mGroupOffset->setEnabled(has_group);
    if (mGroupPick) mGroupPick->setEnabled(has_group);
    mGroupOffset->setCurrentByIndex(has_group ? LLDirectorCast::instance().getGroupRate(name) : 0);
    mRefreshing = false;
}

// ui-draw-lint: allow-function(raw-write) rebuilds only when the tag set or a rate changed (2 Hz poll)
void ALPanelSteppedAnim::syncGroups(bool force)
{
    if (!mGroupPick || !mGroupList) return;
    if (!force && mGroupTimer.getElapsedTimeF32() < 0.5f) return;
    mGroupTimer.reset();
    const LLDirectorCast& cast = LLDirectorCast::instance();
    const std::vector<std::string> names = cast.getGroupNames();
    std::string signature;
    for (const std::string& name : names)
        signature += name + "=" + std::to_string(cast.getGroupRate(name)) + ";";
    if (!force && mGroupsSynced && signature == mGroupListSignature) return;
    // The first sync always rebuilds, so nothing baked into the layout survives.
    const bool same_names = mGroupsSynced && names == mGroupNames;
    mGroupsSynced = true;
    mGroupListSignature = signature;
    mGroupNames = names;
    mRefreshing = true;
    if (!same_names)
    {
        const std::string keep = selectedGroup();
        mGroupPick->removeall();
        if (names.empty())
        {
            // No cast groups (the common case): a disabled placeholder with an empty value.
            mGroupPick->add("No cast groups", LLSD(std::string()), ADD_BOTTOM, false);
            mGroupPick->setValue(LLSD(std::string()));
            mGroupPick->setLabel(std::string("No cast groups"));
        }
        else
        {
            for (const std::string& name : names) mGroupPick->add(name, name);
            mGroupPick->setValue(std::find(names.begin(), names.end(), keep) != names.end() ? keep : names.front());
        }
    }
    mGroupList->deleteAllItems();
    for (const std::string& name : names)
    {
        LLSD row;
        row["id"] = name;
        row["columns"][0]["column"] = "name";
        row["columns"][0]["value"] = name;
        row["columns"][1]["column"] = "value";
        row["columns"][1]["value"] = offsetLabel(cast.getGroupRate(name));
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
    LLDirectorCast& cast = LLDirectorCast::instance();
    const LLUUID id = selectedActor();
    const LLDirectorCast::ActorStyle::SteppedOverride& style = cast.getActorStyle(id).mStepped;
    ALSteppedAnim::ActorOverride ov;
    ov.mode = style.mMode; ov.rate = style.mRate; ov.what = style.mWhat; ov.role = style.mRole;
    ov.rateOffset = style.mRateOffset; ov.parts = style.mParts;
    const S32 group_offset = cast.getGroupRate(cast.getGroup(id.isNull() ? gAgentID : id));
    // The same pure function the planner uses (sanitised base fps, Mixed background, Adaptive fast
    // rate, group / actor offsets, Roto Ink cadence); the seed variation is shown as a range.
    const ALSteppedAnim::Config config = ALSteppedAnimRuntime::instance().configSnapshot();
    const ALSteppedAnim::RateChoice choice = ALSteppedAnim::resolveRate(config, ov, true, group_offset);
    const S32 base_fps = ALSteppedAnim::sanitize(config).baseFps;
    const std::array<F32, 9> key = {{choice.stepped ? 1.f : 0.f, choice.rate, choice.fastRate, (F32)choice.offset,
                                     choice.variation, choice.rotoSuppressed ? 1.f : 0.f,
                                     (F32)(ov.mode * 4 + (ov.rateOffset >= 0 ? 1 : 0) + (group_offset == 3 ? 2 : 0)),
                                     choice.adaptive ? 1.f : 0.f, (F32)base_fps}};
    if (key == mEffectiveKey) return;
    mEffectiveKey = key;
    std::string text;
    if (!choice.stepped)
        text = ov.mode == 1 ? "Smooth (never stepped)"
            : (group_offset == ALSteppedAnim::RATE_SMOOTH && ov.rateOffset < 0 && ov.mode != 2
                   ? "Smooth (group)" : "Smooth (no rate)");
    else
    {
        // Step rates the seed spread can actually reach after whole-step quantisation: the factor
        // lies in [1, 1 + variation) (unitHash is below 1), so the ends are varyRate's values at
        // factor 1 and at the largest factor a seed gives. A spread that quantisation removes
        // reads as no variation at this rate.
        const auto reachable = [base_fps](F32 rate, F32 variation, F32& lo, F32& hi)
        {
            const F32 top = 1.f + variation * (1.f - 1.f / 16777216.f);
            const F32 a = ALSteppedAnim::varyRateAtFactor(rate, 1.f, base_fps);
            const F32 b = ALSteppedAnim::varyRateAtFactor(rate, top, base_fps);
            lo = std::min(a, b);
            hi = std::max(a, b);
        };
        F32 lo = choice.rate, hi = choice.rate;
        F32 fast_lo = choice.fastRate, fast_hi = choice.fastRate;
        if (choice.variation > 0.f)
        {
            reachable(choice.rate, choice.variation, lo, hi);
            if (choice.fastRate > 0.f) reachable(choice.fastRate, choice.variation, fast_lo, fast_hi);
        }
        const bool spread = hi > lo + 1.e-3f;
        const bool fast_spread = fast_hi > fast_lo + 1.e-3f;
        text = spread ? llformat("steps on %.1f-%.1f fps", lo, hi) : llformat("%.1f fps", choice.rate);
        if (choice.adaptive && choice.fastRate > 0.f)
            text += fast_spread ? llformat(", fast %.1f-%.1f fps", fast_lo, fast_hi)
                                : llformat(", fast %.1f fps", choice.fastRate);
        if (choice.offset > 0)
            text += llformat(" (%s%s)", offsetLabel(choice.offset), ov.rateOffset >= 0 ? ", own" : ", group");
        if (choice.variation > 0.f && !spread && !fast_spread) text += " (no variation at this rate)";
        if (choice.rotoSuppressed) text += " (Roto Ink cadence: offsets and variation off)";
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
    refreshPartFace();
    syncGroups(false);
    refreshReadouts();
    LLPanel::draw();
}
