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

#include "alcompassdial.h"          // ALCompassDial custom widget (heading dial)
#include "llactormover.h"
#include "llbutton.h"
#include "llradiogroup.h"
#include "llviewercontrol.h"        // gSavedSettings, LLCachedControl

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
    mScopeRadio = getChild<LLRadioGroup>("scope_radio");
    mScopeRadio->setCommitCallback([this](LLUICtrl*, const LLSD&) { onScopeCommit(); });
    // initial selection (draw()'s diff-sync only reacts to changes)
    mScopeRadio->setValue(gSavedSettings.getBOOL("ActorMoverSync") ? 1 : 0);

    mHeadingDial = getChild<ALCompassDial>("heading_dial");
    mHeadingDial->setCommitCallback([this](LLUICtrl*, const LLSD&) { onDialCommit(); });

    mWalkBtn = getChild<LLButton>("btn_walk");
    mStopBtn = getChild<LLButton>("btn_stop");
    mWalkBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickWalk(); });
    mStopBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickStop(); });

    return true;
}

void ALPanelActorMover::draw()
{
    static LLCachedControl<bool> sync(gSavedSettings, "ActorMoverSync", true);
    static LLCachedControl<F32>  heading(gSavedSettings, "ActorMoverHeading", 0.f);

    // two-way: reflect external changes (the settings-bound spinner, debug
    // settings, or the other live host) without fighting our own live commits
    const S32 want = sync ? 1 : 0;
    if (mScopeRadio->getValue().asInteger() != want)
    {
        mScopeRadio->setValue(want);
    }
    if (fabsf((F32)mHeadingDial->getValue().asReal() - (F32)heading) > 0.01f)
    {
        mHeadingDial->setValue((F32)heading);
    }

    // Sync (Everyone) needs no selection; Selected needs a host list selection
    const bool have_sel = !mSelected.empty();
    const bool enabled = sync || have_sel;
    mWalkBtn->setEnabled(enabled);
    mStopBtn->setEnabled(enabled);
    const std::string walk_tip = enabled
        ? std::string(sync ? "Start every actor (you, if none are listed) with these parameters"
                           : "Start the selected actor(s) with these parameters")
        : std::string("Select an actor first (or switch to Everyone)");
    const std::string stop_tip = enabled
        ? std::string(sync ? "Stop every actor and release their rendered bodies"
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
    gSavedSettings.setBOOL("ActorMoverSync", mScopeRadio->getValue().asInteger() == 1);
}

void ALPanelActorMover::onDialCommit()
{
    // live while dragging, so the in-world heading ray tracks the needle
    gSavedSettings.setF32("ActorMoverHeading", (F32)mHeadingDial->getValue().asReal());
}

void ALPanelActorMover::onClickWalk()
{
    static LLCachedControl<bool> sync(gSavedSettings, "ActorMoverSync", true);
    if (sync)
    {
        LLActorMover::instance().startAll();
    }
    else
    {
        // act on exactly the host-provided selection (one roster row from the
        // standalone, one or more cast rows from the console) -- identical to
        // each host's former hand-copied Walk handler
        for (const LLUUID& id : mSelected)
        {
            LLActorMover::instance().start(id);
        }
    }
}

void ALPanelActorMover::onClickStop()
{
    static LLCachedControl<bool> sync(gSavedSettings, "ActorMoverSync", true);
    if (sync)
    {
        LLActorMover::instance().stopAll();
    }
    else
    {
        for (const LLUUID& id : mSelected)
        {
            LLActorMover::instance().stop(id);
        }
    }
}
