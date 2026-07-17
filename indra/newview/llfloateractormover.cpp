/**
 * @file llfloateractormover.cpp
 * @brief Transport floater for the Actor Mover (local ghost locomotion).
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "llfloateractormover.h"

#include "llactormover.h"
#include "llbutton.h"
#include "lltextbox.h"

LLFloaterActorMover::LLFloaterActorMover(const LLSD& key)
:   LLFloater(key)
{
}

bool LLFloaterActorMover::postBuild()
{
    getChild<LLButton>("walk_btn")->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickWalk(); });
    getChild<LLButton>("stop_btn")->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickStop(); });
    return true;
}

void LLFloaterActorMover::draw()
{
    // live transport state readout
    LLTextBox* status = getChild<LLTextBox>("status_text");
    if (status)
    {
        status->setText(LLActorMover::instance().anyMoving()
                            ? std::string("Moving (local only)")
                            : std::string("Idle"));
    }
    LLFloater::draw();
}

void LLFloaterActorMover::onClickWalk()
{
    LLActorMover::instance().start();
}

void LLFloaterActorMover::onClickStop()
{
    LLActorMover::instance().stop();
}
