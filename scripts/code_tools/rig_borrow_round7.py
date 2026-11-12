"""Round-seven regressions execute the real actor callback, banner and history."""
from rig_borrow_check import block


def extend_fixture(root, source):
    page=(root/'indra/newview/alkitpage.cpp').read_text(encoding='utf8')
    banner=block(page[page.index('turn->setCommitCallback('):], '[this](LLUICtrl*, const LLSD&)')
    source=source.replace('struct KitPage:LLView {', 'struct KitPage:LLView {bool visible=true;void installBanner(LLUICtrl*);')
    # A spy makes even an unbound write observable; all power commits still use
    # the actual production master callback and settings writer.
    source=source.replace('struct LLUICtrl : LLView {', 'struct LLUICtrl : LLView {static inline int boundWrites=0;void writeBoundValue(bool){++boundWrites;}')
    source=source.replace('int main(){', 'void KitPage::installBanner(LLUICtrl* turn){turn->setCommitCallback('+banner+');}\nint main(){')
    tests=r'''
    // Preserve both unrelated undo and redo through the actual Actor-bar callback
    // while the built rig page is hidden and unpinned. Record using real history.
    m=ALCineLightRigManager();c.members.clear();c.members[{"Alice"}];c.members[{"Bob"}];
    m.at(Slot::A).anchor={"Alice"};m.at(Slot::B).anchor={"Bob"};
    p.page.mWhoPinned=false;p.page.mWhoUnavailableTarget={};p.page.visible=false;
    p.page.mWhoCombo=&p.anchor;p.anchor.open=false;p.mCastListInitialized=false;p.updateAnchorList();
    p.page.connectWho(*shell);shell->mActorChoose=&actorChoose;shell->installActorChoose();
    auto& history=shell->history();history.barrier("fixture reset");
    ALKitPage elsewhere;elsewhere.key="other/page";fixtureInput=&elsewhere;sKitInputSourceSet=true;
    int otherReplay=0;
    for(int i=0;i<2;++i)history.recordCustom(&elsewhere,"elsewhere "+std::to_string(i),"Other edit",[&](bool,std::string&){++otherReplay;return true;},false);
    history.undo(elsewhere.key);assert(history.mUndo.size()==1 && history.mRedo.size()==1);
    const auto undoSequence=history.mUndo.back().sequence,redoSequence=history.mRedo.back().sequence;
    fixtureInput=nullptr;sKitInputSourceSet=false;
    actorChoose.onCommit();ALAvatarSubjectPicker::selected({"Alice"});
    assert(m.selectedSlot()==Slot::A && !p.page.visible && !p.page.mWhoPinned);
    assert(history.mUndo.size()==1 && history.mRedo.size()==1 && history.mUndo.back().sequence==undoSequence && history.mRedo.back().sequence==redoSequence);
    actorChoose.onCommit();ALAvatarSubjectPicker::selected({"Bob"});
    assert(m.selectedSlot()==Slot::B && history.mUndo.size()==1 && history.mRedo.size()==1);
    // An open gesture is finished against its old context before a follow swap.
    LLUICtrl oldEdit;oldEdit.name="cine_master_ev";history.mContexts[p.page.mPage]=p.context();
    history.beginGesture(ALKitHistory::MOUSE,p.page.mPage,"Old slot");history.recordWidget(&oldEdit,1.,2.,"Old slot");
    actorChoose.onCommit();ALAvatarSubjectPicker::selected({"Alice"});assert(!history.mOpen);
    assert(history.mUndo.back().entries.front().context.find("B rig #")==0);
    p.page.mRigActorConnection.disconnect();p.page.visible=true;
    // A genuine combo pick still retains its structural barrier.
    p.anchor.setValue(LLSD(0));p.anchor.onCommit();assert(history.mUndo.empty() && history.mRedo.empty());
    // Both On paths act on the selected borrowed rig without invoking the
    // chip's available-action menu, and the banner never writes an unbound value.
    m=ALCineLightRigManager();m.at(Slot::A).anchor={"Alice"};m.setSelectedSlot(Slot::A);p.syncAnchorSelection();
    p.page.mWhoUnavailableTarget={};p.page.installMaster();p.page.mWhoAvailableAction=p.available();
    int menus=0;auto available=p.page.mWhoAvailableAction;p.page.mWhoAvailableAction=[&](){++menus;return available();};
    LLUICtrl turn;p.page.installBanner(&turn);const int bound=LLUICtrl::boundWrites;
    for(bool banner:{false,true}){
        gSavedSettings.setBOOL("CineLightRigEnabled",false);p.page.master.setValue(false);
        if(banner)turn.onCommit();else {p.page.master.setValue(true);p.page.master.onCommit();}
        assert(m.at(Slot::A).enabled && !m.at(Slot::SELF).enabled && menus==0 && LLUICtrl::boundWrites==bound);
    }
    assert(p.page.activateWhoUnavailable() && menus==1); // chip keeps the menu
    p.page.mWhoAvailableAction={};
    // Evaluate a pending-label callback only once, including its lease effects.
    int labels=0;p.page.mWhoUnavailableTarget={"pending"};p.page.mWhoUnavailableLabel=[&](const LLUUID&){++labels;return "Give Actor a rig";};
    p.page.refreshWhoUnavailable();assert(labels==1);p.page.cancelWhoUnavailable();
    // Cast selection must clear interception even when the page is pinned.
    p.page.mWhoPinned=true;p.page.mWhoUnavailableTarget={"pending"};p.mPendingBorrowPick=m.retainBorrowActor({"pending"});
    p.page.master.setValue(false);p.page.applyCastSelection({{"Bob"}});
    assert(p.page.mWhoUnavailableTarget.isNull() && !p.mPendingBorrowPick && p.page.master.isOn() && m.selectedSlot()==Slot::A);
    p.page.mWhoPinned=false;
    // Rebuilding/clearing the ledger leaves only areas with rendered identities.
    m=ALCineLightRigManager();m.subject={};p.mRigDisplayedLabels.clear();p.mCastListInitialized=false;p.updateAnchorList();
    for(const auto& area:p.mRigDisplayedLabels)assert(!area.second.empty());
    // Both captured Remove callbacks report retirement without changing rigs.
    for(bool groupMenu:{false,true})for(bool stillNearby:{false,true}){
        m=ALCineLightRigManager();c.members.clear();c.members[{"Alice"}];m.giveActorRig(Slot::A,{"Alice"});
        if(groupMenu)p.showRigGroupMenu(&p.page.chip,false);else p.showBorrowMenu({"Alice"},true);
        auto* menu=p.mRigGroupMenu.get();LLMenuItemCallGL* remove=nullptr;
        for(auto* child:menu->children)if(auto* item=dynamic_cast<LLMenuItemCallGL*>(child);item && (item->params.name=="remove_rig" || item->params.name=="remove_borrowed_rig"))remove=item;
        assert(remove);c.members.erase({"Alice"});
        if(stillNearby){c.live.insert({"Alice"});m.pruneRemovedAnchors();}
        else m.rewriteActorReferences({"Alice"},{});
        const int writes=m.mutations;auto notices=LLNotificationsUtil::notices.size();remove->params.on_click.function(nullptr,{});
        assert(m.mutations==writes && LLNotificationsUtil::notices.size()==notices+1 && LLNotificationsUtil::find(p.mRigNotices.back())->data["MESSAGE"].asString()=="Actor unavailable");
    }
    std::cout<<"PASS: round 7 real hidden Actor-bar follow preserves undo AND redo; resolved actor power; banner/master/chip routes; single label callback; pinned cancellation; sparse ledger; retired Remove notices\n";
'''
    return source.replace('    // Retire from within a click:',tests+'\n    // Retire from within a click:')
