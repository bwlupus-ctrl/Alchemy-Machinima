/**
 * @file alpanelactormover.cpp
 * @brief Actor Mover transport panel -- see alpanelactormover.h.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "alpanelactormover.h"

#include "alkitpage.h"
#include "alcompassdial.h"          // ALCompassDial custom widget (heading dial)
#include "fsposeranimator.h"
#include "llactormover.h"
#include "llagent.h"
#include "llagentcamera.h"
#include "llbutton.h"
#include "llradiogroup.h"
#include "llviewercontrol.h"        // gSavedSettings, LLCachedControl
#include "llvoavatarself.h"

// both the standalone Actor Mover floater and the Director Console Move tab
// instantiate this class via <panel class="panel_actor_mover" .../>. The injector
// string MUST match the class= string in the XML or the panel silently falls back
// to a plain LLPanel and none of the wiring below runs.
static LLPanelInjector<ALPanelActorMover> t_panel_actor_mover("panel_actor_mover");

//static
void ALPanelActorMover::setToolTipIfChanged(LLUICtrl* ctrl, const std::string& tip)
{
    if (ctrl && ctrl->getToolTip() != tip)
    {
        ctrl->setToolTip(tip);
    }
}

bool ALPanelActorMover::postBuild()
{
    mKitPage = findChild<ALKitPage>("move_kit");
    mScopeRadio = getChild<LLRadioGroup>("scope_radio");
    mScopeRadio->setCommitCallback([this](LLUICtrl*, const LLSD&) { onScopeCommit(); });
    // initial selection (draw()'s diff-sync only reacts to changes)
    mScopeRadio->setValue(gSavedSettings.getBOOL("ActorMoverSync") ? 1 : 0);

    // Both hosts retain the exact Heading spinner as well as the live drag/wheel compass.
    // Ghosts owns the kit-wide "heading_dial" name, so Move uses "move_heading_dial".
    mHeadingDial = findChild<ALCompassDial>("heading_dial");
    if (!mHeadingDial)
    {
        mHeadingDial = findChild<ALCompassDial>("move_heading_dial");
    }
    if (mHeadingDial)
    {
        mHeadingDial->setCommitCallback([this](LLUICtrl*, const LLSD&) { onDialCommit(); });
    }
    mMyHeadingDial = findChild<ALCompassDial>("my_heading_dial");
    if (mMyHeadingDial)
    {
        mMyHeadingDial->setCommitCallback([this](LLUICtrl*, const LLSD&) { onMyHeadingCommit(); });
        refreshMyHeading();
    }

    mWalkBtn = getChild<LLButton>("btn_walk");
    // Kit widget names are unique across Director pages and Camera > Takes owns "btn_stop", so
    // the generated Move page names its Stop row "move_btn_stop".
    mStopBtn = findChild<LLButton>("btn_stop");
    if (!mStopBtn)
    {
        mStopBtn = getChild<LLButton>("move_btn_stop");
    }
    mWalkBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickWalk(); });
    mStopBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickStop(); });

    return true;
}

void ALPanelActorMover::draw()
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_UI; // [DirFix] D9
    if (mMyHeadingDial && isWidgetOnScreen(mMyHeadingDial) && mMyHeadingClock.due())
    {
        refreshMyHeading();
    }
    static LLCachedControl<bool> sync(gSavedSettings, "ActorMoverSync", true);
    static LLCachedControl<F32>  heading(gSavedSettings, "ActorMoverHeading", 0.f);

    // two-way: reflect external changes (the settings-bound spinner, debug
    // settings, or the other live host) without fighting our own live commits
    const bool everyone = sync && !(mKitPage && mKitPage->whoPinned());
    const S32 want = everyone ? 1 : 0;
    if (mScopeRadio->getValue().asInteger() != want)
    {
        mScopeRadio->setValue(want);
    }
    if (mHeadingDial && fabsf((F32)mHeadingDial->getValue().asReal() - (F32)heading) > 0.01f)
    {
        mHeadingDial->setValue((F32)heading);
    }

    // Sync (Everyone) needs no selection; Selected needs a host list selection
    const bool have_sel = mKitPage && mKitPage->whoPinned()
        ? !mKitPage->actorTargetAway() && mKitPage->actorTarget().notNull()
        : !mSelected.empty();
    const bool enabled = everyone || have_sel;
    mWalkBtn->setEnabled(enabled);
    mStopBtn->setEnabled(enabled);
    const std::string walk_tip = enabled
        ? std::string(everyone ? "Start every actor (you, if none are listed) with these parameters"
                           : "Start the selected actor(s) with these parameters")
        : std::string("Select an actor first (or switch to Everyone)");
    const std::string stop_tip = enabled
        ? std::string(everyone ? "Stop every actor and release their rendered bodies"
                           : "Stop the selected actor(s)")
        : std::string("Select an actor first (or switch to Everyone)");
    setToolTipIfChanged(mWalkBtn, walk_tip);
    setToolTipIfChanged(mStopBtn, stop_tip);

    LLPanel::draw();
}

void ALPanelActorMover::onScopeCommit()
{
    // segmented Selected|Everyone drives the shared "sync all" setting
    // (1 = Everyone = sync on), same setting the standalone mover historically
    // exposed as a "Sync all" checkbox
    const bool everyone = mScopeRadio->getValue().asInteger() == 1;
    if (everyone && mKitPage && mKitPage->whoPinned())
        mKitPage->setWhoPin(LLSD());
    gSavedSettings.setBOOL("ActorMoverSync", everyone);
}

void ALPanelActorMover::onDialCommit()
{
    // live while dragging, so the in-world heading ray tracks the needle
    if (!mHeadingDial)
    {
        return;
    }
    gSavedSettings.setF32("ActorMoverHeading", (F32)mHeadingDial->getValue().asReal());
}

const char* ALPanelActorMover::myHeadingDisabledReason() const
{
    if (!isAgentAvatarValid()) return "My heading is unavailable until your avatar is ready.";
    if (gAgent.isSitting()) return "Not available while seated: the seat owns your avatar's rotation.";
    // Mouselook owns pitch as well as yaw; an upright absolute heading would erase its pitch.
    if (gAgentCamera.cameraMouselook()) return "Leave mouselook to use My heading.";
    if (LLActorMover::instance().isDriving(gAgent.getID()))
        return "Stop / Release Actor Mover before turning your avatar.";
    // The query uses the shared motion registry, including poses left playing with the UI hidden.
    static const FSPoserAnimator poser;
    if (poser.isPosingAvatar(gAgentAvatarp)) return "Stop posing your avatar before using My heading.";
    if (gAgent.rotateGrabbed()) return "Your turn controls are held by a script.";
    return nullptr;
}

void ALPanelActorMover::refreshMyHeading()
{
    const char* reason = myHeadingDisabledReason();
    setEnabledIfChanged(mMyHeadingDial, reason == nullptr);
    setToolTipIfChanged(mMyHeadingDial, reason ? reason :
        "Drag the needle to turn your avatar in-world, as if you turned with the keys. Others see it. "
        "The wheel trims 5 degrees per click. Not available while seated.");
    if (isAgentAvatarValid() && !mMyHeadingDial->hasMouseCapture())
    {
        LLVector3 at = gAgent.getFrameAgent().getAtAxis();
        if (gAgentAvatarp->getParent())
        {
            // Seated agent axes are parent-local; project only after returning to world space.
            at *= static_cast<LLViewerObject*>(gAgentAvatarp->getParent())->getRenderRotation();
        }
        const F32 degrees = atan2f(at.mV[VY], at.mV[VX]) * RAD_TO_DEG;
        // Keyboard focus on the dial must not freeze live readback; only a drag owns the needle.
        setValueIfChanged(mMyHeadingDial, LLSD(degrees), true);
    }
}

void ALPanelActorMover::onMyHeadingCommit()
{
    // Recheck at commit: seating or a tool taking ownership can race the 10 Hz UI refresh.
    if (!mMyHeadingDial || myHeadingDisabledReason()) return;
    const F32 radians = (F32)mMyHeadingDial->getValue().asReal() * DEG_TO_RAD;
    // Share keyboard camera preparation (including its preference and autopilot cancellation).
    // Zero yaw avoids adding a relative turn; refocus may reset axes, so set heading afterwards.
    gAgent.moveYaw(0.f);
    // Same frame as keyboard yaw / slamLookAt. A horizontal unit look vector removes tilt;
    // normal AgentUpdate publishes this frame as BodyRotation.
    gAgent.resetAxes(LLVector3(cosf(radians), sinf(radians), 0.f));
}

uuid_vec_t ALPanelActorMover::selectedTargets() const
{
    if (mKitPage && mKitPage->whoPinned())
    {
        const LLUUID actor = mKitPage->actorTarget();
        return !mKitPage->actorTargetAway() && actor.notNull() ? uuid_vec_t{actor} : uuid_vec_t{};
    }
    return mSelected;
}

void ALPanelActorMover::onClickWalk()
{
    static LLCachedControl<bool> sync(gSavedSettings, "ActorMoverSync", true);
    if (sync && !(mKitPage && mKitPage->whoPinned()))
    {
        LLActorMover::instance().startAll();
    }
    else
    {
        // act on exactly the host-provided selection (one roster row from the
        // standalone, one or more cast rows from the console) -- identical to
        // each host's former hand-copied Walk handler
        for (const LLUUID& id : selectedTargets())
        {
            LLActorMover::instance().start(id);
        }
    }
}

void ALPanelActorMover::onClickStop()
{
    static LLCachedControl<bool> sync(gSavedSettings, "ActorMoverSync", true);
    if (sync && !(mKitPage && mKitPage->whoPinned()))
    {
        LLActorMover::instance().stopAll();
    }
    else
    {
        for (const LLUUID& id : selectedTargets())
        {
            LLActorMover::instance().stop(id);
        }
    }
}
