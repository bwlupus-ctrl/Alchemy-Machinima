/**
 * @file llfloateractormover.h
 * @brief Transport floater for the Actor Mover (local ghost locomotion).
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#ifndef LL_LLFLOATERACTORMOVER_H
#define LL_LLFLOATERACTORMOVER_H

#include "llfloater.h"

class ALPanelPathEditor;
class LLButton;
class LLScrollListCtrl;

class LLFloaterActorMover final : public LLFloater
{
public:
    LLFloaterActorMover(const LLSD& key);
    bool postBuild() override;
    void draw() override;

private:
    void onClickWalk();
    void onClickStop();
    void refreshRoster();
    LLUUID selectedActor() const;   // null when nothing selected

    LLScrollListCtrl*  mRosterList = nullptr;
    LLButton*          mWalkBtn = nullptr;
    LLButton*          mStopBtn = nullptr;
    ALPanelPathEditor* mPathPanel = nullptr;    // shared waypoint editor
};

#endif // LL_LLFLOATERACTORMOVER_H
