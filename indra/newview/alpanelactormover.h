/**
 * @file alpanelactormover.h
 * @brief Actor Mover transport panel: the Selected|Everyone scope, the heading
 *        compass dial + spinner, Speed/Distance/Cadence, End behavior, the
 *        custom walk-anim UUID row, and the Walk / Stop buttons.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 *
 * Extracted from floater_actor_mover.xml AND the Director Console's Move tab
 * (which hand-copied the same transport with a second copy of the wiring) so the
 * standalone floater and the console embed the SAME panel (panel_actor_mover.xml)
 * instead of duplicating the transport, which is how they drift.
 *
 * All parameter controls are settings-backed (ActorMoverSpeed/Distance/
 * WalkNominal/Heading/EndMode/UseCustomAnim/CustomAnim), so multiple live
 * instances (floater + console tab) stay in lockstep through gSavedSettings with
 * no fork -- exactly like ALPanelFlycamOrbit / ALPanelPathEditor. The scope radio
 * and the compass dial two-way sync their settings (ActorMoverSync /
 * ActorMoverHeading) in draw() without fighting their own live commits.
 *
 * The only host-specific input is "which actor(s) are selected": the roster /
 * cast LIST stays with the host (the standalone's own list, the console's
 * persistent cast column). Each host feeds its current selection to the panel via
 * setSelectedActors() each draw; Walk / Stop then honor ActorMoverSync -- Sync
 * (Everyone) drives LLActorMover::startAll()/stopAll(), otherwise the panel acts
 * on exactly that host-provided set. So Walk / Stop behave identically from both
 * hosts, and the panel owns no selection state that could fork.
 */

#ifndef AL_ALPANELACTORMOVER_H
#define AL_ALPANELACTORMOVER_H

#include "llpanel.h"
#include "lluuid.h"     // uuid_vec_t

class ALCompassDial;
class LLButton;
class LLRadioGroup;

class ALPanelActorMover final : public LLPanel
{
public:
    ALPanelActorMover() = default;
    ~ALPanelActorMover() override = default;

    bool postBuild() override;
    void draw() override;

    // The host tells the panel which roster / cast actors are currently
    // selected; when scope = Selected (ActorMoverSync off) Walk / Stop act on
    // exactly this set. Sync (Everyone) ignores it and drives startAll/stopAll.
    // Fed each draw before LLFloater::draw() paints the panel, so the enable
    // state and the button actions always see the current selection.
    void setSelectedActors(const uuid_vec_t& ids) { mSelected = ids; }

private:
    void onScopeCommit();
    void onDialCommit();
    void onClickWalk();
    void onClickStop();

    // set a tooltip only when it changed (draw()-rate friendly)
    static void setToolTipIfChanged(LLUICtrl* ctrl, const std::string& tip);

    LLRadioGroup*  mScopeRadio = nullptr;
    ALCompassDial* mHeadingDial = nullptr;
    LLButton*      mWalkBtn = nullptr;
    LLButton*      mStopBtn = nullptr;

    // host-provided selection (the Selected-scope target set); no fork-prone
    // state beyond this transient mirror of the host list selection
    uuid_vec_t mSelected;
};

#endif // AL_ALPANELACTORMOVER_H
