/**
 * @file alfloateractorgaze.cpp
 * @brief Standalone host for the shared Actor Gaze panel.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "alfloateractorgaze.h"

#include "alpanellensgaze.h"
#include "llfloaterreg.h"

ALFloaterActorGaze::ALFloaterActorGaze(const LLSD& key)
    : LLFloater(key)
{
}

bool ALFloaterActorGaze::postBuild()
{
    mGazePanel = findChild<ALPanelLensGaze>("lens_gaze_panel");
    return LLFloater::postBuild();
}

void ALFloaterActorGaze::showForSelection(ESelectionSource source,
                                           const uuid_vec_t& actors)
{
    if (ALFloaterActorGaze* floater =
            LLFloaterReg::showTypedInstance<ALFloaterActorGaze>(
                "actor_gaze", LLSD(), true))
    {
        floater->takeSelection(source, actors);
    }
}

void ALFloaterActorGaze::updateSelection(ESelectionSource source,
                                          const uuid_vec_t& actors)
{
    if (ALFloaterActorGaze* floater =
            LLFloaterReg::findTypedInstance<ALFloaterActorGaze>("actor_gaze");
        floater && floater->mSelectionSource == source)
    {
        floater->takeSelection(source, actors);
    }
}

void ALFloaterActorGaze::takeSelection(ESelectionSource source,
                                        const uuid_vec_t& actors)
{
    mSelectionSource = source;
    if (mGazePanel)
    {
        mGazePanel->setSelectedActors(actors);
    }
}
