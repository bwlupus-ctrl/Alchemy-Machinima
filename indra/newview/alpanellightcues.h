/**
 * @file alpanellightcues.h
 * @brief Director > Light > Light Cues page logic: the cue list with Add, Remove, Play and Stop.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 *
 * The page root of the generated kit page (class "panel_light_cues"). It drives the same
 * ALCineLightRig cue list and transport as the Lighting Cue Stack floater
 * (alfloatercinelightcues.cpp): Add appends a complete live-rig snapshot, Remove deletes the
 * selected cue, Play runs it through cueGoto(), Stop calls cueRelease(). Selecting a row opens
 * the floater at that cue, where the timing, FX and list files are edited.
 */

#ifndef AL_ALPANELLIGHTCUES_H
#define AL_ALPANELLIGHTCUES_H

#include "alcinelightrigmodel.h"
#include "llpanel.h"

class LLButton;
class LLScrollListCtrl;

class ALPanelLightCues final : public LLPanel
{
public:
    ALPanelLightCues() = default;
    ~ALPanelLightCues() override = default;

    bool postBuild() override;
    void draw() override;

private:
    S32  selectedCueIndex() const;
    void refreshRows(bool force);
    void refreshButtons();
    void onSelection();
    void onAdd();
    void onRemove();
    void onPlay();
    void onStop();
    // Replace the selected rig's cue list and record one undo step for the change.
    void commitListEdit(const ALCineLightRigModel::CueList& after, const std::string& label);

    LLScrollListCtrl* mCueRows = nullptr;
    LLButton*         mAddBtn = nullptr;
    LLButton*         mRemoveBtn = nullptr;
    LLButton*         mPlayBtn = nullptr;
    LLButton*         mStopBtn = nullptr;
    U64               mSeenRevision = U64(-1);
    S32               mSeenSlot = -1;
};

#endif // AL_ALPANELLIGHTCUES_H
