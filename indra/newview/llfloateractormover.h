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

class ALPanelActorMover;
class ALPanelPathEditor;
class LLScrollListCtrl;
class LLTextBox;

class LLFloaterActorMover final : public LLFloater
{
public:
    LLFloaterActorMover(const LLSD& key);
    bool postBuild() override;
    void draw() override;
    void onOpen(const LLSD& key) override; // [DirFix] D14 reveal feed + claim

    uuid_vec_t currentSelection() const; // [DirFix] Actor Gaze synchronous pull

private:
    void refreshRoster();
    void revealPathTarget();        // [DirFix] D14: event-path feed + claim of the Path host
    LLUUID selectedActor() const;   // null when nothing selected
    uuid_vec_t selectedActors() const;
    void onOpenActorGaze();

    LLScrollListCtrl*   mRosterList = nullptr;
    ALPanelActorMover*  mMoverPanel = nullptr;  // shared transport (scope, heading, params, Walk/Stop)
    ALPanelPathEditor*  mPathPanel = nullptr;   // shared waypoint editor
    LLTextBox*          mStatusText = nullptr;  // [UIPerf] resolved once on first draw
    uuid_vec_t          mLastGazePush;          // [DirFix] push only on selection change
};

#endif // LL_LLFLOATERACTORMOVER_H
