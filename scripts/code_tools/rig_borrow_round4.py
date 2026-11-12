"""Round-four regressions use the production shell picker and menu/history paths."""
from rig_shadow_presets_check import function
from rig_borrow_check import block


def extend_fixture(root, source):
    # Expose the existing unavailable-action lambda without reimplementing it.
    panel=(root/'indra/newview/alpanelcinelightrig.cpp').read_text(encoding='utf8')
    pos=panel.index('panel && !panel->addAvatarToRigGroup(id)')
    start=panel.rfind('[handle](const LLUUID& id)',0,pos)
    action=block(panel[start:],'[handle](const LLUUID& id)')
    source=source.replace('bool addAvatarToRigGroup(const LLUUID&);', 'void installGive();bool addAvatarToRigGroup(const LLUUID&);')
    source=source.replace('int main(){','void ALPanelCineLightRig::installGive(){auto handle=getDerivedHandle<ALPanelCineLightRig>();page.mWhoUnavailableAction='+action+';}\nint main(){')
    # The shell's production chip callback constructs independent buttons from
    # combo labels. Only UI services/geometry are doubled here.
    shell=(root/'indra/newview/alkitshell.cpp').read_text(encoding='utf8')
    callback=block(shell[shell.index('if (LLButton* who = page->whoChip();'):],
                   '[page_handle, shell_handle, anchor_handle, prefix,')
    callback=callback.replace('ALKitPage*','KitPage*')
    source=source.replace('struct Popover {', '''
struct LLRect {LLRect(int,int,int,int){}};
struct LLButton:LLUICtrl {struct Params{std::string name,label;LLRect rect{0,0,0,0};};
    Params params;template<class T>LLHandle<T> getDerivedHandle(){return {static_cast<T*>(this)};}
    void setOrigin(int,int){}void setEnabled(bool){} };
struct Popover {struct Params{std::string name;LLRect rect{0,0,0,0};bool visible=false;};
    std::vector<LLUICtrl*> children;void addChild(LLUICtrl* c){children.push_back(c);}
    template<class T>LLHandle<T> getDerivedHandle(){return {static_cast<T*>(this)};}
    void bindAnchor(LLButton*){}void show();
''')
    source=source.replace('    Popover* mOpenPopover=nullptr;', '''
    template<class T>LLHandle<T> getDerivedHandle(){return {static_cast<T*>(this)};}
    void addChild(Popover*){}bool consumeAnchorClose(Popover*){return false;}
    Popover* mOpenPopover=nullptr;''')
    source=source.replace('struct LLUICtrlFactory{', 'using ALKitPopover=Popover;namespace ALKitStyle{void styleTree(Popover*){}}\nstruct LLUICtrlFactory{')
    source=source.replace('std::is_same_v<T,LLMenuItemCallGL>', 'std::is_same_v<T,LLMenuItemCallGL> || std::is_same_v<T,LLButton>')
    source=source.replace('int main(){', '''
void Popover::show(){visible=true;ALKitShell::find(nullptr)->mOpenPopover=this;}
void installPicker(KitPage* page,LLButton* who){
    auto page_handle=LLHandle<KitPage>(page);auto shell_handle=LLHandle<ALKitShell>(ALKitShell::find(page));
    auto anchor_handle=LLHandle<LLButton>(who);std::string prefix="rig";
    who->setCommitCallback('''+callback+''');
}
int main(){''')
    tests=r'''
    // Real who-chip -> shell callback -> copied buttons. Hide names, then
    // rebuild on Cast churn BEFORE the next privacy poll, while editing You.
    m=ALCineLightRigManager();m.mSelected=Slot::SELF;c.members.clear();c.live.clear();
    m.at(Slot::A).anchor={"snapshot-secret"};c.members[{"snapshot-secret"}].mLastName="SNAPSHOT SECRET";
    RlvActions::visible=true;p.page.mWhoAvailableAction={};p.page.mWhoUnavailableTarget={};
    p.page.mWhoCombo=&p.anchor;p.mSubjectChoiceRevision=~U64(0);p.mCastListInitialized=false;p.anchor.open=false;p.updateAnchorList();
    LLButton picker;installPicker(&p.page,&picker);picker.onCommit();
    auto* shell=ALKitShell::find(&p);auto* snapshot=shell->mOpenPopover;assert(snapshot && snapshot->getVisible());
    bool secret=false;for(auto* child:snapshot->children)secret|=static_cast<LLButton*>(child)->params.label.find("SNAPSHOT SECRET")!=std::string::npos;
    assert(secret);RlvActions::visible=false;c.members[{"churn"}];++c.revision;p.updateAnchorList();
    assert(!snapshot->getVisible());p.refreshRigPrivacy();assert(!snapshot->getVisible());
    for(const auto& item:p.anchor.items)assert(item.label.find("SECRET")==std::string::npos);
    // Unrelated ledgers are refreshed independently of the picker snapshot.
    for(const char* area:{"subjects","anchors","roster","chip","hint","checks","lights_combo","lights_status","notice","menu"}){
        snapshot->visible=true;shell->mOpenPopover=snapshot;p.clearRigLabels(area);assert(snapshot->visible);
    }
    RlvActions::visible=true;p.page.mWhoCombo=&p.page.combo;
    // An open list defers only its own rows. Status, enables, roster and chip
    // keep tracking model changes; keyboard focus alone does not defer rows.
    m=ALCineLightRigManager();m.mSelected=Slot::A;c.members.clear();c.members[{"Bob"}];
    m.at(Slot::A).anchor={"Bob"};p.mCastListInitialized=false;p.anchor.open=false;p.updateAnchorList();
    auto oldRows=p.anchor.items.size();p.anchor.open=true;m.at(Slot::B).anchor={"Alice"};m.at(Slot::A).enabled=true;
    p.page.label="stale chip";p.updateAnchorList();assert(p.page.label!="stale chip");assert(p.anchor.items.size()==oldRows && !p.mCastListInitialized);
    assert(p.status.value.asString()=="1 rig on" && p.roster[1].value.asString().find(": on - ")!=std::string::npos);
    assert(p.allOff.enabled && p.remove.enabled);p.anchor.open=false;p.anchor.focus=true;p.updateAnchorList();
    assert(p.mCastListInitialized && std::find(p.mCastIds.begin(),p.mCastIds.end(),LLUUID{"Alice"})!=p.mCastIds.end());p.anchor.focus=false;
    // Actual Replace menu callback, and the shell's own real history. The
    // unfinished global gesture must be finalized, not erased, by completion.
    for(bool slotEdit:{false,true}){
        m=ALCineLightRigManager();c.members.clear();c.members[{"Alice"}];c.members[{"Bob"}];m.giveActorRig(Slot::A,{"Bob"});
        shell->history().barrier("fixture reset");auto& h=shell->history();h.mContexts[p.page.mPage]=p.context();
        LLUICtrl edit;edit.name=slotEdit?"cine_master_ev":"cine_live_probe_radius";edit.value=2.;int replays=0;edit.commit=[&](){++replays;};
        h.beginGesture(ALKitHistory::MOUSE,p.page.mPage,"Before Replace");h.recordWidget(&edit,1.,2.,"Before Replace");
        p.showBorrowMenu({"Alice"});auto* replace=static_cast<LLMenuItemCallGL*>(p.mRigGroupMenu.get()->children.front());
        assert(replace->params.name=="replace_rig");replace->params.on_click.function(nullptr,{});
        assert(m.at(Slot::A).anchor==LLUUID{"Alice"});h.undo(p.page.mPage);assert(replays==(slotEdit?0:1));
    }
    // A stale pending Give is consumed once, cancels interception and restores
    // the real switch. Subsequent commits toggle the real selected rig.
    m=ALCineLightRigManager();c.members.clear();c.live.clear();p.page.installMaster();p.installGive();
    p.page.mWhoUnavailableLabel=[](const LLUUID&){return "Give Actor a rig";};p.page.mWhoUnavailableTarget={"departed"};
    gSavedSettings.setBOOL("CineLightRigEnabled",false);p.page.master.setValue(true);p.page.master.onCommit();
    assert(p.page.mWhoUnavailableTarget.isNull() && !p.page.master.isOn());
    assert(LLNotificationsUtil::find(p.mRigNotices.back())->data["MESSAGE"].asString()=="Actor unavailable");
    p.page.master.setValue(true);p.page.master.onCommit();assert(m.selected().enabled);
    // Remove resets every targeting layer in both the rig and saved blob.
    m=ALCineLightRigManager();c.members[{"Bob"}];m.giveActorRig(Slot::A,{"Bob"});
    m.at(Slot::A).object={"prop"};m.at(Slot::A).groupEnabled=true;m.at(Slot::A).group="old";m.removeBorrowedRig(Slot::A);
    assert(m.at(Slot::A).object.isNull() && !m.at(Slot::A).groupEnabled && m.at(Slot::A).group.empty());
    assert(m.mBlobs[1].mObjectTarget.isNull() && !m.mBlobs[1].mGroupEnabled && m.mBlobs[1].mLitGroup.empty());
    // A leased menu identity follows arbitrarily many rekeys. Historical
    // unleased body aliases disappear, even while the destination stays Cast.
    m=ALCineLightRigManager();c.members.clear();LLUUID body{"body0"};c.members[body];auto lease=m.retainBorrowActor(body);
    for(int i=1;i<=100;++i){LLUUID next{"body"+std::to_string(i)};c.members.erase(body);c.members[next];++c.revision;m.rewriteActorReferences(body,next);body=next;assert(m.mBorrowRekeys.size()<=1);}
    assert(m.resolveBorrowActor(*lease,true)==body);lease.reset();m.mBorrowAnchorsDirty=true;m.refreshBorrowAnchors();assert(m.mBorrowRekeys.empty());
    // Unchanged ticks must not enter the pruning loop. A deliberately planted
    // dead alias is a sentinel: only a revision change allows its removal.
    m.mBorrowRekeys[{"sentinel"}]={"gone"};for(int i=0;i<1000;++i)m.refreshBorrowAnchors();assert(m.mBorrowRekeys.count({"sentinel"}));
    ++c.revision;m.refreshBorrowAnchors();assert(!m.mBorrowRekeys.count({"sentinel"}));
    // Both Share branches retain the actual recipients and exclude unrelated
    // slot actors. Existing away group membership remains authored too.
    for(bool existing:{false,true})for(int controlAvatar:{0,1,2}){
        m=ALCineLightRigManager();c.members.clear();c.groups.clear();c.members[{"Alice"}];c.members[{"Bob"}];
        m.at(Slot::A).anchor={"Bob"};m.at(Slot::B).anchor={"unrelated"};m.at(Slot::A).members={{"Bob"}};
        if(controlAvatar){m.at(Slot::A).members={{"control-avatar-Bob"}};if(controlAvatar==1)m.at(Slot::A).stored={{"Bob"}};
            else LLDirectorCast::canonical[LLUUID{"control-avatar-Bob"}]={"Bob"};}
        if(existing){c.groups["away"]={{"away-member"}};m.at(Slot::A).group="away";}
        p.shareRig({"Alice"},1);auto ids=c.membersInGroup(m.at(Slot::A).group);
        assert(std::find(ids.begin(),ids.end(),LLUUID{"Bob"})!=ids.end());assert(std::find(ids.begin(),ids.end(),LLUUID{"Alice"})!=ids.end());
        assert(std::find(ids.begin(),ids.end(),gAgentID)==ids.end());assert(std::find(ids.begin(),ids.end(),LLUUID{"unrelated"})==ids.end());
        assert(std::find(ids.begin(),ids.end(),LLUUID{"control-avatar-Bob"})==ids.end());
        LLDirectorCast::canonical.erase(LLUUID{"control-avatar-Bob"});
    }
    std::cout<<"PASS: real shell picker closes on pre-poll Cast churn; actual Replace preserves shell global undo/rejects stale slot; open-list status refresh, unavailable Give, complete Remove and bounded revision-gated aliases\n";
'''
    source=source.replace('    // Retire from within a click:', tests+'\n    // Retire from within a click:')
    # Service state needed by the stronger assertions, not behavior doubles.
    source=source.replace('void setEnabledIfChanged(LLUICtrl*,bool){}', 'void setEnabledIfChanged(LLUICtrl* c,bool b){if(c)c->enabled=b;}')
    source=source.replace('struct LLUICtrl : LLView {', 'struct LLUICtrl : LLView {bool enabled=false;')
    source=source.replace('struct Blob {LLUUID mAnchor;', 'struct Blob {LLUUID mObjectTarget;bool mGroupEnabled=false;std::string mLitGroup;LLUUID mAnchor;')
    source=source.replace('mAnchor=rig.anchor;', 'mAnchor=rig.anchor;mObjectTarget=rig.object;mGroupEnabled=rig.groupEnabled;mLitGroup=rig.group;')
    return source
