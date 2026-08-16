/**
 * @file alpanellensgaze.cpp
 * @brief Shared Lens Gaze controls -- see alpanellensgaze.h.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "alpanellensgaze.h"

#include "llactormover.h"
#include "llagent.h"
#include "llavatarnamecache.h"
#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "llcombobox.h"
#include "lldirectorcast.h"
#include "llsliderctrl.h"
#include "llspinctrl.h"
#include "lltextbox.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llvoavatarself.h"

static LLPanelInjector<ALPanelLensGaze> t_panel_lens_gaze("panel_lens_gaze");

namespace
{
std::string actorName(const LLUUID& id)
{
    if (const LLDirectorCast::CastMember* member =
            LLDirectorCast::instance().getMember(id))
    {
        if (!member->mLastName.empty())
        {
            return member->mLastName;
        }
    }
    if (LLAvatarName name; LLAvatarNameCache::get(id, &name))
    {
        return name.getCompleteName();
    }
    return id.asString().substr(0, 8);
}

bool isEditing(LLUICtrl* control)
{
    return control && (control->hasFocus() || control->hasMouseCapture());
}
} // anonymous namespace

bool ALPanelLensGaze::postBuild()
{
    mStatus = getChild<LLTextBox>("gaze_status");
    mEnable = getChild<LLCheckBoxCtrl>("gaze_enable_check");
    mTarget = getChild<LLComboBox>("gaze_target_combo");
    mCast = getChild<LLComboBox>("gaze_cast_combo");
    mSetPoint = getChild<LLButton>("btn_gaze_setpoint");
    mBlend = getChild<LLSliderCtrl>("gaze_blend_slider");
    mTorso = getChild<LLSliderCtrl>("gaze_torso_slider");
    mIntensity = getChild<LLSliderCtrl>("gaze_intensity_slider");
    mSmoothing = getChild<LLSliderCtrl>("gaze_smoothing_slider");
    mDeadZone = getChild<LLSpinCtrl>("gaze_deadzone_spinner");
    mBreakoff = getChild<LLCheckBoxCtrl>("gaze_breakoff_check");
    mBreakoffAngle = getChild<LLSpinCtrl>("gaze_breakoff_spinner");
    mEyelineYaw = getChild<LLSliderCtrl>("gaze_eyeline_yaw_slider");
    mEyelinePitch = getChild<LLSliderCtrl>("gaze_eyeline_pitch_slider");

    mEnable->setCommitCallback([this](LLUICtrl*, const LLSD&) { onEnableCommit(); });
    mTarget->setCommitCallback([this](LLUICtrl*, const LLSD&) { onTargetCommit(); });
    mCast->setCommitCallback([this](LLUICtrl*, const LLSD&) { onCastCommit(); });
    mSetPoint->setCommitCallback([this](LLUICtrl*, const LLSD&) { onSetPoint(); });
    mBlend->setCommitCallback([this](LLUICtrl*, const LLSD&) { onBlendCommit(); });
    mTorso->setCommitCallback([this](LLUICtrl*, const LLSD&) { onTorsoCommit(); });
    mIntensity->setCommitCallback([this](LLUICtrl*, const LLSD&) { onIntensityCommit(); });
    mSmoothing->setCommitCallback([this](LLUICtrl*, const LLSD&) { onSmoothingCommit(); });
    mBreakoff->setCommitCallback([this](LLUICtrl*, const LLSD&) { onBreakoffCommit(); });
    mEyelineYaw->setCommitCallback([this](LLUICtrl*, const LLSD&) { onEyelineCommit(); });
    mEyelinePitch->setCommitCallback([this](LLUICtrl*, const LLSD&) { onEyelineCommit(); });
    return true;
}

LLUUID ALPanelLensGaze::displayActor() const
{
    return mSelected.empty() ? LLUUID::null : mSelected.front();
}

uuid_vec_t ALPanelLensGaze::commitActors() const
{
    return mSelected.empty() ? uuid_vec_t(1, LLUUID::null) : mSelected;
}

void ALPanelLensGaze::draw()
{
    refreshCastCombo();
    refreshControls();
    LLPanel::draw();
}

void ALPanelLensGaze::refreshCastCombo()
{
    std::string signature;
    for (const LLUUID& selected : mSelected)
    {
        signature += selected.asString();
    }
    for (const LLUUID& id : LLDirectorCast::instance().getIds())
    {
        signature += id.asString();
    }
    if (signature != mCastSignature && !isEditing(mCast))
    {
        mCastSignature = signature;
        mCast->removeall();
        const LLUUID displayed = displayActor();
        for (const LLUUID& id : LLDirectorCast::instance().getIds())
        {
            const bool is_target_actor = displayed.notNull()
                ? id == displayed
                : isAgentAvatarValid() && id == gAgentAvatarp->getID();
            if (!is_target_actor)
            {
                mCast->add(actorName(id), LLSD(id.asString()));
            }
        }
        if (mCast->getItemCount() == 0)
        {
            mCast->add("(add another cast member)", LLSD(std::string()));
        }
    }

    if (!isEditing(mCast))
    {
        const LLUUID current = LLActorMover::instance().getGazeCastTarget(displayActor());
        if (current.isNull() ||
            !mCast->setSelectedByValue(LLSD(current.asString()), true))
        {
            mCast->selectFirstItem();
        }
    }
}

void ALPanelLensGaze::refreshControls()
{
    LLActorMover& mover = LLActorMover::instance();
    const LLUUID actor = displayActor();
    const bool have_actor = actor.notNull() || isAgentAvatarValid();
    const bool enabled = have_actor && mover.isGazeEnabled(actor);
    const S32 mode = have_actor ? mover.getGazeTargetMode(actor)
                                : (S32)LLActorMover::GAZE_TANGENT;

    mEnable->setEnabled(have_actor);
    if (!isEditing(mEnable) && mEnable->getValue().asBoolean() != enabled)
    {
        mEnable->set(enabled);
    }
    mTarget->setEnabled(have_actor && enabled);
    if (!isEditing(mTarget) && mTarget->getValue().asInteger() != mode)
    {
        mTarget->setValue(mode);
    }

    const bool cast_mode = mode == LLActorMover::GAZE_CAST;
    const bool point_mode = mode == LLActorMover::GAZE_POINT;
    mCast->setVisible(cast_mode);
    mCast->setEnabled(have_actor && enabled && cast_mode && mCast->getItemCount() > 0);
    mSetPoint->setVisible(point_mode);
    mSetPoint->setEnabled(have_actor && enabled && point_mode);

    LLUICtrl* authored[] = {
        mBlend, mTorso, mIntensity, mSmoothing, mEyelineYaw, mEyelinePitch
    };
    for (LLUICtrl* control : authored)
    {
        control->setEnabled(have_actor && enabled);
    }

    auto sync_slider = [](LLSliderCtrl* control, F32 value)
    {
        if (!isEditing(control) &&
            fabsf((F32)control->getValue().asReal() - value) > 0.001f)
        {
            control->setValue(value);
        }
    };
    sync_slider(mBlend, have_actor ? mover.getGazeHeadEyeBlend(actor) : 0.7f);
    sync_slider(mTorso, have_actor ? mover.getGazeTorsoAmount(actor) : 0.25f);
    sync_slider(mIntensity, have_actor ? mover.getGazeIntensity(actor) : 1.f);
    sync_slider(mSmoothing, have_actor ? mover.getGazeSmoothing(actor) : 0.5f);
    sync_slider(mEyelineYaw, have_actor ? mover.getGazeEyelineYaw(actor) : 0.f);
    sync_slider(mEyelinePitch, have_actor ? mover.getGazeEyelinePitch(actor) : 0.f);

    const bool breakoff = gSavedSettings.getS32("BDMergeGazeBehindPolicy") == 1;
    if (!isEditing(mBreakoff) && mBreakoff->getValue().asBoolean() != breakoff)
    {
        mBreakoff->set(breakoff);
    }
    mBreakoffAngle->setEnabled(breakoff);

    std::string status;
    mover.getGazeStatus(actor, status);
    if (mStatus->getValue().asString() != status)
    {
        mStatus->setText(status);
    }
}

void ALPanelLensGaze::onEnableCommit()
{
    for (const LLUUID& actor : commitActors())
    {
        LLActorMover::instance().setGazeEnabled(actor, mEnable->get());
    }
}

void ALPanelLensGaze::onTargetCommit()
{
    for (const LLUUID& actor : commitActors())
    {
        LLActorMover::instance().setGazeTargetMode(actor, mTarget->getValue().asInteger());
    }
}

void ALPanelLensGaze::onCastCommit()
{
    const std::string value = mCast->getSelectedValue().asString();
    const LLUUID target = value.empty() ? LLUUID::null : LLUUID(value);
    for (const LLUUID& actor : commitActors())
    {
        LLActorMover::instance().setGazeCastTarget(actor, target);
    }
}

void ALPanelLensGaze::onSetPoint()
{
    const LLVector3d point = gAgent.getPosGlobalFromAgent(
        LLViewerCamera::getInstance()->getOrigin());
    for (const LLUUID& actor : commitActors())
    {
        LLActorMover::instance().setGazePointGlobal(actor, point);
    }
}

void ALPanelLensGaze::onBlendCommit()
{
    for (const LLUUID& actor : commitActors())
    {
        LLActorMover::instance().setGazeHeadEyeBlend(
            actor, (F32)mBlend->getValue().asReal());
    }
}

void ALPanelLensGaze::onTorsoCommit()
{
    for (const LLUUID& actor : commitActors())
    {
        LLActorMover::instance().setGazeTorsoAmount(
            actor, (F32)mTorso->getValue().asReal());
    }
}

void ALPanelLensGaze::onIntensityCommit()
{
    for (const LLUUID& actor : commitActors())
    {
        LLActorMover::instance().setGazeIntensity(
            actor, (F32)mIntensity->getValue().asReal());
    }
}

void ALPanelLensGaze::onSmoothingCommit()
{
    for (const LLUUID& actor : commitActors())
    {
        LLActorMover::instance().setGazeSmoothing(
            actor, (F32)mSmoothing->getValue().asReal());
    }
}

void ALPanelLensGaze::onBreakoffCommit()
{
    gSavedSettings.setS32("BDMergeGazeBehindPolicy", mBreakoff->get() ? 1 : 0);
}

void ALPanelLensGaze::onEyelineCommit()
{
    const F32 yaw = (F32)mEyelineYaw->getValue().asReal();
    const F32 pitch = (F32)mEyelinePitch->getValue().asReal();
    for (const LLUUID& actor : commitActors())
    {
        LLActorMover::instance().setGazeEyelineOffset(actor, yaw, pitch);
    }
}
