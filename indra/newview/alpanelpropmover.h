/**
 * @file alpanelpropmover.h
 * @brief Shared GUI for the client-side object path mover (props on splines).
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 *
 * The full-works GUI over ALObjectPathMover: enroll/remove props, author and
 * inspect their nodes, tune path speed / end mode / per-node skid and speed,
 * and drive/stop each prop or all of them -- replacing the /objpath* chat
 * commands as the primary interface (the commands remain as a scriptable
 * harness). Same shared-panel idiom as ALPanelGhostStudio: this ONE panel is
 * hosted by the standalone "Prop Mover" floater AND embedded as the Director
 * Console's Props tab, so the console stays a superset.
 *
 * All state lives in ALObjectPathMover + the LLActorMover path store (keyed by
 * the object's root UUID); this panel is a pure view that refreshes on a short
 * throttle from draw(), so in-world changes (chat commands, enrollment from
 * the right-click menu, a derezzed prop) show up without any explicit wiring.
 */

#ifndef AL_ALPANELPROPMOVER_H
#define AL_ALPANELPROPMOVER_H

#include "llpanel.h"
#include "llframetimer.h"
#include "lluuid.h"

class LLButton;
class LLComboBox;
class LLScrollListCtrl;
class LLSpinCtrl;
class LLTextBox;

class ALPanelPropMover final : public LLPanel
{
public:
    ALPanelPropMover();
    bool postBuild() override;
    void draw() override;

private:
    // ---- actions ----------------------------------------------------------
    void onEnrollSelected();    // enroll the in-world selection's linkset root
    void onRemoveProp();        // un-enroll the list-selected prop (drops its path)
    void onAddNode();           // append a node at the prop's CURRENT position
    void onDeleteNode();        // delete the selected node (last when none selected)
    void onClearNodes();        // stop + drop every node of the selected prop
    void onDrive();
    void onStop();
    void onDriveAll();
    void onStopAll();
    void onSelectProp();
    void onSelectNode();
    void onCommitPathSpeed();
    void onCommitEndMode();
    void onCommitNodeSkid();
    void onCommitNodeSpeed();

    // ---- refresh ----------------------------------------------------------
    void refresh();             // throttled from draw(); rebuilds both lists
    void refreshNodePane();     // node list + per-node/param controls for the selection
    void setStatus(const std::string& msg);     // one-line action feedback

    // the list-selected prop / node (LLUUID::null / -1 = none). The prop id is
    // kept across refreshes so background list rebuilds never lose the
    // operator's place.
    LLUUID selectedProp() const;
    S32    selectedNode() const;

    LLScrollListCtrl* mPropList = nullptr;
    LLScrollListCtrl* mNodeList = nullptr;
    LLSpinCtrl*       mPathSpeedSpin = nullptr;
    LLComboBox*       mEndModeCombo = nullptr;
    LLSpinCtrl*       mNodeSkidSpin = nullptr;
    LLSpinCtrl*       mNodeSpeedSpin = nullptr;
    LLTextBox*        mStatusText = nullptr;

    LLFrameTimer mRefreshTimer;     // throttle the draw()-driven refresh
    std::string  mStatusMsg;        // sticky last-action line (beats the summary)
    F32          mStatusUntil = 0.f;// frame-seconds until the sticky line expires
};

#endif // AL_ALPANELPROPMOVER_H
