/**
 * @file alpanelgazecues.h
 * @brief Director > Performance > Gaze Cues page logic: the cue list with Add, Remove, Play and Stop.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 *
 * The page root of the generated kit page (class "panel_gaze_cues"). It edits the same per-actor
 * gaze cue list as the Gaze Cues floater (alfloatergazecues.cpp) through LLDirectorCast: Add
 * appends a cue built from the actor's current gaze defaults, Remove deletes the selected cue (one
 * undo step each), Play previews the selected cue on the actor and Stop ends the preview
 * (LLActorMover::setGazeCuePreview / clearGazeCuePreview). Selecting a row opens the floater on
 * that actor at that cue, where targets, macros, transitions and influence keys are edited.
 *
 * The actor is the first cast selection, or the agent when nothing is selected, exactly as the
 * retired launcher card chose it.
 */

#ifndef AL_ALPANELGAZECUES_H
#define AL_ALPANELGAZECUES_H

#include "lldirectorcast.h"
#include "llpanel.h"

class LLButton;
class LLScrollListCtrl;

class ALPanelGazeCues final : public LLPanel
{
public:
    ALPanelGazeCues() = default;
    ~ALPanelGazeCues() override = default;

    bool postBuild() override;
    void draw() override;

private:
    LLUUID actorId() const;
    S32    selectedCueIndex() const;
    void   refreshRows(bool force);
    void   refreshButtons();
    void   onSelection();
    void   onAdd();
    void   onRemove();
    void   onPlay();
    void   onStop();
    // Replace the actor's cue list and record one undo step for the change.
    void commitListEdit(const LLUUID& actor, const LLDirectorCast::GazeCueList& after, const std::string& label);

    LLScrollListCtrl* mCueList = nullptr;
    LLButton*         mAddBtn = nullptr;
    LLButton*         mRemoveBtn = nullptr;
    LLButton*         mPlayBtn = nullptr;
    LLButton*         mStopBtn = nullptr;
    U64               mSeenRevision = U64(-1);
    LLUUID            mSeenActor;
};

#endif // AL_ALPANELGAZECUES_H
