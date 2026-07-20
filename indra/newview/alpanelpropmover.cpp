/**
 * @file alpanelpropmover.cpp
 * @brief Shared GUI for the client-side object path mover -- see the header.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "alpanelpropmover.h"

#include "alobjectpathmover.h"
#include "alobjectproperties.h"     // cached object names (populated by selection replies)
#include "llactormover.h"           // shared Path store + per-node accessors
#include "llbutton.h"
#include "llcombobox.h"
#include "llscrolllistctrl.h"
#include "llselectmgr.h"            // "Enroll selected" reads the in-world selection
#include "llspinctrl.h"
#include "lltextbox.h"
#include "llviewerobject.h"
#include "llviewerobjectlist.h"
#include "lluictrlfactory.h"

// the ONE shared panel: the standalone Prop Mover floater and the Director
// Console's Props tab both instantiate this class from panel_prop_mover.xml
static LLPanelInjector<ALPanelPropMover> t_panel_prop_mover("panel_prop_mover");

namespace
{
// list-friendly name for an enrolled prop: the cached ObjectProperties name
// when a reply has landed (enrolling selects the object, which triggers one),
// else a recognizable short-id placeholder.
std::string prop_display_name(const LLUUID& id)
{
    if (const auto* props = ALObjectPropertiesCache::instance().get(id))
    {
        if (!props->mName.empty())
        {
            return props->mName;
        }
    }
    return "Object " + id.asString().substr(0, 8);
}
} // anonymous namespace

ALPanelPropMover::ALPanelPropMover() = default;

// ---------------------------------------------------------------------------
bool ALPanelPropMover::postBuild()
{
    mPropList      = getChild<LLScrollListCtrl>("prop_list");
    mNodeList      = getChild<LLScrollListCtrl>("node_list");
    mPathSpeedSpin = getChild<LLSpinCtrl>("path_speed_spin");
    mEndModeCombo  = getChild<LLComboBox>("end_mode_combo");
    mNodeSkidSpin  = getChild<LLSpinCtrl>("node_skid_spin");
    mNodeSpeedSpin = getChild<LLSpinCtrl>("node_speed_spin");
    mStatusText    = getChild<LLTextBox>("status_text");

    mPropList->setCommitCallback(boost::bind(&ALPanelPropMover::onSelectProp, this));
    mNodeList->setCommitCallback(boost::bind(&ALPanelPropMover::onSelectNode, this));

    getChild<LLButton>("btn_enroll")->setClickedCallback(boost::bind(&ALPanelPropMover::onEnrollSelected, this));
    getChild<LLButton>("btn_remove")->setClickedCallback(boost::bind(&ALPanelPropMover::onRemoveProp, this));
    getChild<LLButton>("btn_node_add")->setClickedCallback(boost::bind(&ALPanelPropMover::onAddNode, this));
    getChild<LLButton>("btn_node_del")->setClickedCallback(boost::bind(&ALPanelPropMover::onDeleteNode, this));
    getChild<LLButton>("btn_node_clear")->setClickedCallback(boost::bind(&ALPanelPropMover::onClearNodes, this));
    getChild<LLButton>("btn_drive")->setClickedCallback(boost::bind(&ALPanelPropMover::onDrive, this));
    getChild<LLButton>("btn_stop")->setClickedCallback(boost::bind(&ALPanelPropMover::onStop, this));
    getChild<LLButton>("btn_drive_all")->setClickedCallback(boost::bind(&ALPanelPropMover::onDriveAll, this));
    getChild<LLButton>("btn_stop_all")->setClickedCallback(boost::bind(&ALPanelPropMover::onStopAll, this));

    mPathSpeedSpin->setCommitCallback(boost::bind(&ALPanelPropMover::onCommitPathSpeed, this));
    mEndModeCombo->setCommitCallback(boost::bind(&ALPanelPropMover::onCommitEndMode, this));
    mNodeSkidSpin->setCommitCallback(boost::bind(&ALPanelPropMover::onCommitNodeSkid, this));
    mNodeSpeedSpin->setCommitCallback(boost::bind(&ALPanelPropMover::onCommitNodeSpeed, this));

    refresh();
    return LLPanel::postBuild();
}

// ---------------------------------------------------------------------------
// A pure view over mover state that others can mutate (chat commands, the
// right-click enrollment, a derez), so refresh on a short throttle instead of
// wiring change signals through every mutation site.
void ALPanelPropMover::draw()
{
    if (mRefreshTimer.getElapsedTimeF32() > 0.5f)
    {
        mRefreshTimer.reset();
        refresh();
    }
    LLPanel::draw();
}

// ---------------------------------------------------------------------------
LLUUID ALPanelPropMover::selectedProp() const
{
    return mPropList->getSelectedValue().asUUID();
}

S32 ALPanelPropMover::selectedNode() const
{
    LLScrollListItem* item = mNodeList->getFirstSelected();
    return item ? item->getValue().asInteger() : -1;
}

void ALPanelPropMover::setStatus(const std::string& msg)
{
    mStatusMsg   = msg;
    mStatusUntil = (F32)LLFrameTimer::getElapsedSeconds() + 5.f;
    mRefreshTimer.reset();
    refresh();
}

// ---------------------------------------------------------------------------
void ALPanelPropMover::refresh()
{
    ALObjectPathMover& opm  = ALObjectPathMover::instance();
    LLActorMover&      mover = LLActorMover::instance();

    // ---- prop list (selection + scroll preserved across the rebuild) ------
    const LLUUID sel        = selectedProp();
    const S32    scroll_pos = mPropList->getScrollPos();
    mPropList->clearRows();

    S32 driving = 0;
    for (const LLUUID& id : opm.getRoster())
    {
        const LLActorMover::Path* path = mover.getPath(id);
        const S32  nodes    = path ? (S32)path->mNodes.size() : 0;
        const bool resolved = gObjectList.findObject(id) != nullptr;
        const bool is_driving = opm.isDriving(id);
        driving += is_driving ? 1 : 0;

        LLSD row;
        row["value"] = id;
        row["columns"][0]["column"] = "name";
        row["columns"][0]["value"]  = prop_display_name(id);
        row["columns"][1]["column"] = "nodes";
        row["columns"][1]["value"]  = nodes;
        row["columns"][2]["column"] = "state";
        row["columns"][2]["value"]  = !resolved ? "away"
                                    : is_driving ? "driving" : "-";
        mPropList->addElement(row, ADD_BOTTOM);
    }
    if (sel.notNull())
    {
        mPropList->selectByValue(sel);
    }
    else if (mPropList->getItemCount() == 1)
    {
        mPropList->selectFirstItem();    // a lone prop selects itself: fewer clicks
    }
    mPropList->setScrollPos(scroll_pos);

    refreshNodePane();

    // ---- status line: sticky action feedback, else a live summary ---------
    if (!mStatusMsg.empty() && (F32)LLFrameTimer::getElapsedSeconds() < mStatusUntil)
    {
        mStatusText->setText(mStatusMsg);
    }
    else
    {
        mStatusMsg.clear();
        mStatusText->setText(llformat("%d prop%s enrolled, %d driving",
                                      (S32)opm.getRoster().size(),
                                      opm.getRoster().size() == 1 ? "" : "s",
                                      driving));
    }
}

void ALPanelPropMover::refreshNodePane()
{
    LLActorMover& mover = LLActorMover::instance();
    const LLUUID  sel   = selectedProp();
    const LLActorMover::Path* path = sel.notNull() ? mover.getPath(sel) : nullptr;

    const S32 node_sel   = selectedNode();
    const S32 scroll_pos = mNodeList->getScrollPos();
    mNodeList->clearRows();
    if (path)
    {
        for (S32 i = 0; i < (S32)path->mNodes.size(); ++i)
        {
            const LLActorMover::Waypoint& wp = path->mNodes[i];
            LLSD row;
            row["value"] = i;
            row["columns"][0]["column"] = "index";
            row["columns"][0]["value"]  = i + 1;
            row["columns"][1]["column"] = "skid";
            row["columns"][1]["value"]  = llformat("%.0f", wp.mYawOffset * RAD_TO_DEG);
            row["columns"][2]["column"] = "speed";
            row["columns"][2]["value"]  = wp.mSpeedOverride > 0.f
                                        ? llformat("%.2f", wp.mSpeedOverride)
                                        : std::string("path");
            mNodeList->addElement(row, ADD_BOTTOM);
        }
        if (node_sel >= 0 && node_sel < (S32)path->mNodes.size())
        {
            mNodeList->selectByValue(node_sel);
        }
        mNodeList->setScrollPos(scroll_pos);
    }

    // path-wide params mirror the selected prop (guard the no-selection case
    // so a half-open panel never writes into a null path)
    const bool has_path = path != nullptr;
    mPathSpeedSpin->setEnabled(has_path);
    mEndModeCombo->setEnabled(has_path);
    if (has_path)
    {
        mPathSpeedSpin->setValue(path->mSpeed);
        mEndModeCombo->setCurrentByIndex(llclamp(path->mEndMode, 0, 2));
    }

    // per-node params mirror the selected node
    const S32  cur      = selectedNode();
    const bool has_node = path && cur >= 0 && cur < (S32)path->mNodes.size();
    mNodeSkidSpin->setEnabled(has_node);
    mNodeSpeedSpin->setEnabled(has_node);
    if (has_node)
    {
        mNodeSkidSpin->setValue(path->mNodes[cur].mYawOffset * RAD_TO_DEG);
        mNodeSpeedSpin->setValue(path->mNodes[cur].mSpeedOverride);
    }
}

// ---------------------------------------------------------------------------
// actions
// ---------------------------------------------------------------------------
void ALPanelPropMover::onEnrollSelected()
{
    LLViewerObject* obj = LLSelectMgr::getInstance()->getSelection()->getPrimaryObject();
    if (!obj || obj->isAvatar())
    {
        setStatus("Select an object in world first (right-click or edit it)");
        return;
    }
    LLViewerObject* root = obj->getRootEdit();
    if (!root)
    {
        setStatus("Could not resolve the selection's root prim");
        return;
    }
    if (ALObjectPathMover::isTarget(root->getID()))
    {
        setStatus("Already enrolled: " + prop_display_name(root->getID()));
        mPropList->selectByValue(root->getID());
        refreshNodePane();
        return;
    }
    ALObjectPathMover::toggleTarget(root->getID());
    setStatus("Enrolled " + prop_display_name(root->getID()));
    mPropList->selectByValue(root->getID());
    refreshNodePane();
}

void ALPanelPropMover::onRemoveProp()
{
    const LLUUID sel = selectedProp();
    if (sel.isNull())
    {
        setStatus("Select an enrolled prop first");
        return;
    }
    const std::string name = prop_display_name(sel);
    // toggle-off IS the removal: it stops the drive and drops the path too
    ALObjectPathMover::toggleTarget(sel);
    setStatus("Removed " + name + " (path dropped)");
}

void ALPanelPropMover::onAddNode()
{
    const LLUUID sel = selectedProp();
    if (sel.isNull())
    {
        setStatus("Select an enrolled prop first");
        return;
    }
    if (!ALObjectPathMover::instance().appendWaypointHere(sel))
    {
        setStatus("Prop not in view/resolvable - node not added");
        return;
    }
    const LLActorMover::Path* path = LLActorMover::instance().getPath(sel);
    const S32 n = path ? (S32)path->mNodes.size() : 0;
    setStatus(llformat("Node %d added at the prop's current spot (move it, add the next)", n));
}

void ALPanelPropMover::onDeleteNode()
{
    const LLUUID sel = selectedProp();
    const LLActorMover::Path* path = sel.notNull() ? LLActorMover::instance().getPath(sel) : nullptr;
    if (!path || path->mNodes.empty())
    {
        setStatus("No nodes to delete");
        return;
    }
    // the selected node, else the LAST one (matches the /objpathskid idiom)
    S32 idx = selectedNode();
    if (idx < 0 || idx >= (S32)path->mNodes.size())
    {
        idx = (S32)path->mNodes.size() - 1;
    }
    LLActorMover::instance().deleteWaypoint(sel, idx);
    setStatus(llformat("Node %d deleted", idx + 1));
}

void ALPanelPropMover::onClearNodes()
{
    const LLUUID sel = selectedProp();
    if (sel.isNull())
    {
        setStatus("Select an enrolled prop first");
        return;
    }
    ALObjectPathMover::instance().stop(sel);
    LLActorMover::instance().clearPath(sel);
    setStatus("Path cleared (prop stays enrolled)");
}

void ALPanelPropMover::onDrive()
{
    const LLUUID sel = selectedProp();
    if (sel.isNull())
    {
        setStatus("Select an enrolled prop first");
        return;
    }
    if (ALObjectPathMover::instance().start(sel))
    {
        setStatus("Driving " + prop_display_name(sel));
    }
    else
    {
        const LLActorMover::Path* path = LLActorMover::instance().getPath(sel);
        setStatus((path && path->mNodes.size() >= 2)
                  ? "Prop not in view/resolvable - cannot drive"
                  : "Needs at least 2 nodes - use Add node");
    }
}

void ALPanelPropMover::onStop()
{
    const LLUUID sel = selectedProp();
    if (sel.isNull())
    {
        setStatus("Select an enrolled prop first");
        return;
    }
    ALObjectPathMover::instance().stop(sel);
    setStatus("Stopped (prop stays where the drive left it)");
}

void ALPanelPropMover::onDriveAll()
{
    ALObjectPathMover::instance().startAll();
    refresh();
}

void ALPanelPropMover::onStopAll()
{
    ALObjectPathMover::instance().stopAll();
    setStatus("All drives stopped");
}

// ---------------------------------------------------------------------------
void ALPanelPropMover::onSelectProp()
{
    mNodeList->deselectAllItems();
    refreshNodePane();
}

void ALPanelPropMover::onSelectNode()
{
    refreshNodePane();
}

void ALPanelPropMover::onCommitPathSpeed()
{
    const LLUUID sel = selectedProp();
    if (sel.notNull() && LLActorMover::instance().getPath(sel))
    {
        LLActorMover::Path& path = LLActorMover::instance().editPath(sel);
        path.mSpeed = llclamp((F32)mPathSpeedSpin->getValue().asReal(), 0.05f, 50.f);
        path.markDirty();
    }
}

void ALPanelPropMover::onCommitEndMode()
{
    const LLUUID sel = selectedProp();
    if (sel.notNull() && LLActorMover::instance().getPath(sel))
    {
        LLActorMover::Path& path = LLActorMover::instance().editPath(sel);
        path.mEndMode = llclamp(mEndModeCombo->getCurrentIndex(), 0, 2);
        path.markDirty();
    }
}

void ALPanelPropMover::onCommitNodeSkid()
{
    const LLUUID sel = selectedProp();
    const S32    idx = selectedNode();
    if (sel.notNull() && idx >= 0)
    {
        LLActorMover::instance().setNodeYawOffset(
            sel, idx, (F32)mNodeSkidSpin->getValue().asReal() * DEG_TO_RAD);
        refreshNodePane();
    }
}

void ALPanelPropMover::onCommitNodeSpeed()
{
    const LLUUID sel = selectedProp();
    const S32    idx = selectedNode();
    if (sel.notNull() && idx >= 0)
    {
        LLActorMover::instance().setNodeSpeed(
            sel, idx, (F32)mNodeSpeedSpin->getValue().asReal());
        refreshNodePane();
    }
}
