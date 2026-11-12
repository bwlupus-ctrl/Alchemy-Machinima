"""Round-six regressions: Actor-bar wiring, pending power, opt-in custom contexts."""
from collections import Counter
from rig_shadow_presets_check import function
from rig_borrow_check import block


def check_history_callers(root):
    import re
    callers = Counter()
    providers = Counter()
    for path in (root/'indra/newview').rglob('*.cpp'):
        # Inventory all production calls, excluding method definitions.
        text = path.read_text(encoding='utf8')
        callers[path.name] += len(re.findall(r'(?:->|\.)recordCustom\s*\(', text))
        providers[path.name] += len(re.findall(r'(?:->|\.)registerContext\s*\(', text))
    callers = +callers; providers = +providers
    assert callers == Counter({'alghostclonemenu.cpp':4, 'alkitpresets.cpp':1,
        'alpanelcinelightrig.cpp':2, 'alpanelgazecues.cpp':1, 'alpanelghoststudio.cpp':2,
        'alpanellightcues.cpp':1, 'llfloaterdirector.cpp':1}), callers
    assert providers == Counter({'alpanelcinelightrig.cpp':1}), providers
    panel = (root/'indra/newview/alpanelcinelightrig.cpp').read_text(encoding='utf8')
    start = panel.index('shell->history().registerContext(')
    cb = block(panel[start:], '[handle](const std::string& widget)')
    assert panel[start+panel[start:].index(cb)+len(cb):].lstrip().startswith(', true);')
    header = (root/'indra/newview/alkithistory.h').read_text(encoding='utf8')
    assert 'bool custom_entries = false' in header
    print('PASS: production history inventory: 12 recordCustom calls in 7 files; rig is the sole context provider/opt-in')


def extend_fixture(root, source):
    check_history_callers(root)
    page = (root/'indra/newview/alkitpage.cpp').read_text(encoding='utf8')
    shell = (root/'indra/newview/alkitshell.cpp').read_text(encoding='utf8')
    manager = (root/'indra/newview/alcinelightrigmanager.cpp').read_text(encoding='utf8')
    cast = (root/'indra/newview/lldirectorcast.cpp').read_text(encoding='utf8')
    # Services: active-actor signal and picker transport. The actual Actor-bar
    # chooser callback, Cast setter, subscription and page methods are extracted.
    source = source.replace('struct LLDirectorCast {', '''struct LLDirectorCast {
    LLUUID mActiveActorId;boost::signals2::signal<void()> mActiveActorChanged;
    void setActiveActorId(const LLUUID&);const LLUUID& getActiveActorId()const{return mActiveActorId;}
    boost::signals2::connection addActiveActorChangedCallback(const std::function<void()>& cb){return mActiveActorChanged.connect(cb);}
    static LLUUID runtimeActorId(const LLUUID& id){return id;}LLUUID getSteppedActorId()const{return mActiveActorId;}void touchScene(){}
''', 1)
    source = source.replace('struct ALGhostStudio {', 'struct ALGhostStudio {bool isPrivateOverlayRuntime(const LLUUID&)const{return false;}')
    source = source.replace('struct Rig {', function(cast,'void LLDirectorCast::setActiveActorId(')+'\nstruct Rig {', 1)
    source = source.replace('struct KitPage:LLView {', '''struct KitPage:LLView {
    boost::signals2::scoped_connection mWhoConnection,mRigActorConnection;
    void applyActiveRigActor();void connectWho(ALKitShell&);void restoreRigMaster();void updateMaster();
    struct Doc{bool dimmed=false;void setDimmed(bool b){dimmed=b;}} doc;Doc* mDoc=&doc;
    LLUICtrl banner,state;LLUICtrl *mBanner=&banner,*mMasterState=&state;
    std::string pageKey()const{return mPage;}
    void installMasterWatch();std::vector<boost::signals2::scoped_connection> mConnections;
''', 1)
    source = source.replace('struct ALKitShell : LLView {', '''struct LLContextMenu;struct LLButton;
struct ALKitShell : LLView {
    LLButton* mActorChoose=nullptr;LLHandle<LLContextMenu> mActorMenu;void installActorChoose();
    boost::signals2::signal<void(const uuid_vec_t&)> castSignal;
    boost::signals2::connection setCastSelectionCallback(const std::function<void(const uuid_vec_t&)>& cb){return castSignal.connect(cb);}
''', 1)
    source = source.replace('struct LLControlVariable {', '''struct LLControlVariable {
    static inline bool missingControl=false;
    static inline boost::signals2::signal<void(LLControlVariable*,const LLSD&,const LLSD&)> signal;
    static auto* getSignal(){assert(!missingControl && "Signal requested for missing setting control");return &signal;}
''', 1)
    # A missing-control signal request fails an explicit spy assertion, rather
    # than invoking undefined behavior or waiting in Windows crash handling.
    source = source.replace('auto it=controls.find(n);return it==controls.end()?LLHandle<LLControlVariable>():LLHandle<LLControlVariable>(&it->second);',
        'auto it=controls.find(n);LLControlVariable::missingControl=it==controls.end();return it==controls.end()?LLHandle<LLControlVariable>():LLHandle<LLControlVariable>(&it->second);')
    # Count provenance walks, and model UI state for the actual updateMaster body.
    source = source.replace('LLView* getParent() const { return parent; }',
                            'static inline int parentReads=0;LLView* getParent() const { ++parentReads;return parent; }')
    source = source.replace('void setToolTipIfChanged(LLUICtrl*,const std::string&){}',
        'void setVisibleIfChanged(LLUICtrl* c,bool b){if(c)c->visible=b;}void setToolTipIfChanged(LLUICtrl*,const std::string&){}')
    # Selected settings are a model/settings buffer swap in production too.
    # Execute the actual manager selector with its session/log services doubled.
    source = source.replace('void setSelectedSlot(Slot s){mSelected=s;}', '''
    struct Session{bool canSelect(Slot s)const{return s>=Slot::SELF && s<Slot::COUNT;}}mRigSession;
    void logSelectedTarget(){}void setSelectedSlot(Slot);
''')
    source = source.replace('++mutations;if(ALKitHistory::suspended())++cleanupWrites;',
        'gSavedSettings.writes["CineLightRigEnabled"]=at(mSelected).enabled;++mutations;if(ALKitHistory::suspended())++cleanupWrites;')
    source = source.replace('Slot borrowSlot(const LLUUID&)const{return Slot::COUNT;}',
        'Slot freeSlot=Slot::COUNT;Slot borrowSlot(const LLUUID& id)const{auto own=actorSlot(id);return own==Slot::COUNT?freeSlot:own;}')
    source = source.replace('LLUUID actorTarget()const{return mActorTarget.notNull()?mActorTarget:LLUUID{"new-body"};}',
        'LLUUID actorTarget()const{return mWhoUnavailableTarget.notNull()?mWhoUnavailableTarget:mActorTarget.notNull()?mActorTarget:LLUUID{"new-body"};}')
    branch = block(page[page.index('if (mWhoAdapter == "cine_rig_slot")'):], 'if (mWhoAdapter == "cine_rig_slot")')
    watch = branch[branch.index('if (LLControlVariable* control'):branch.index('mMaster->setCommitCallback')]
    definitions = '\n'.join(function(page, sig).replace('ALKitPage::','KitPage::') for sig in (
        'void ALKitPage::restoreRigMaster()', 'void ALKitPage::updateMaster()',
        'void ALKitPage::applyActiveRigActor()', 'void ALKitPage::connectWho('))
    definitions += '\n'+function(manager, 'void ALCineLightRigManager::setSelectedSlot(')
    definitions += '\nvoid KitPage::installMasterWatch(){'+watch+'}\n'
    choose = block(shell[shell.index('mActorChoose->setCommitCallback('):], '[this](LLUICtrl*, const LLSD&)')
    definitions += '''
struct ALAvatarSubjectPicker {
    static inline std::function<void(const LLUUID&)> selected;
    static void choose(LLButton*,ALKitShell*,LLHandle<LLContextMenu>&,bool,std::function<void(const LLUUID&)> cb,bool){selected=std::move(cb);}
};
void ALKitShell::installActorChoose(){mActorChoose->setCommitCallback('''+choose+''');}
'''
    # Install the actual Give-label callback too, so the actor-name/chip path is real.
    panel = (root/'indra/newview/alpanelcinelightrig.cpp').read_text(encoding='utf8')
    label = block(panel[panel.index('mRigKitPage->setWhoUnavailable('):], '[handle](const LLUUID& id)')
    source = source.replace('void installGive();', 'void installGiveLabel();void installGive();')
    definitions += '\nvoid ALPanelCineLightRig::installGiveLabel(){auto handle=getDerivedHandle<ALPanelCineLightRig>();page.mWhoUnavailableLabel='+label+';}\n'
    source = source.replace('int main(){', definitions+'\nint main(){')
    tests = r'''
    // Actor-bar chooser -> production Cast setter/signal -> production rig
    // subscription -> real Give chip -> actual master commit. No Cast injection.
    m=ALCineLightRigManager();m.mSelected=Slot::SELF;m.subject={};m.freeSlot=Slot::A;
    c.members.clear();c.live.clear();c.members[{"Alice"}].mLastName="Alice";
    p.page.mWhoPinned=false;p.page.mWhoUnavailableTarget={};p.page.mWhoCombo=&p.page.combo;
    p.page.mWhoAvailableAction={};p.page.combo.value=0;gSavedSettings.setBOOL("CineLightRigEnabled",false);
    p.installGive();p.installGiveLabel();p.page.installMaster();c.setActiveActorId(gAgentID);
    p.page.connectWho(*shell);
    LLButton actorChoose;shell->mActorChoose=&actorChoose;shell->installActorChoose();actorChoose.onCommit();
    assert(ALAvatarSubjectPicker::selected);ALAvatarSubjectPicker::selected({"Alice"});
    assert(p.page.label=="Give Alice a rig" && p.page.mWhoUnavailableTarget==LLUUID{"Alice"});
    assert(m.selectedSlot()==Slot::SELF && !m.at(Slot::SELF).enabled);
    p.page.master.setValue(true);p.page.master.onCommit();
    assert(m.selectedSlot()==Slot::A && m.at(Slot::A).anchor==LLUUID{"Alice"} && m.at(Slot::A).enabled);
    assert(!m.at(Slot::SELF).enabled && p.page.mWhoUnavailableTarget.isNull() && !p.page.mWhoPinned);
    // Explicit rig pin ignores Actor picks. Only rig pages subscribe; connect a
    // different adapter with a live Who chip and require no extra subscriber.
    assert(p.page.setWhoPin(LLSD(1)));c.members[{"Bob"}].mLastName="Bob";
    actorChoose.onCommit();ALAvatarSubjectPicker::selected({"Bob"});assert(p.page.mWhoUnavailableTarget.isNull() && m.selectedSlot()==Slot::A);
    KitPage other;other.mWhoAdapter="gaze_slot";auto subscriptions=c.mActiveActorChanged.num_slots();other.connectWho(*shell);
    assert(c.mActiveActorChanged.num_slots()==subscriptions);other.mWhoConnection.disconnect();
    assert(p.page.setWhoPin(LLSD()));assert(p.page.label=="Give Bob a rig");
    // Every termination path restores ACTUAL power and un-dims controls.
    for(int end=0;end<5;++end){
        c.setActiveActorId(gAgentID);m.mSelected=Slot::SELF;gSavedSettings.setBOOL("CineLightRigEnabled",true);
        c.setActiveActorId({"Bob"});assert(!p.page.master.isOn() && m.selected().enabled);
        p.page.updateMaster();assert(p.page.doc.dimmed);
        if(end==0)c.setActiveActorId({});
        if(end==1)p.page.cancelWhoUnavailable();
        if(end==2)assert(p.page.selectWhoValue(LLSD(0)));
        if(end==3){m.subject={"Bob"};m.at(Slot::B).enabled=true;m.mBlobs[2].mEnabled=true;p.page.refreshWhoUnavailable();
            assert(m.selectedSlot()==Slot::B && m.slotActor(m.selectedSlot())==LLUUID{"Bob"});
            assert(std::find(p.mCastValues.begin(),p.mCastValues.end(),2)!=p.mCastValues.end());}
        if(end==4)c.setActiveActorId(gAgentID);
        assert(p.page.mWhoUnavailableTarget.isNull() && p.page.master.isOn() && !p.page.doc.dimmed);
        p.page.master.setValue(!p.page.master.isOn());p.page.master.onCommit();assert(!m.selected().enabled);
        if(end==3){m.at(Slot::SELF).enabled=false;p.page.master.setValue(true);p.page.master.onCommit();assert(m.at(Slot::B).enabled && !m.at(Slot::SELF).enabled);}
        m.at(Slot::B).anchor={};m.subject={};c.setActiveActorId(gAgentID);
    }
    // Production master observer tolerates a missing setting control.
    gSavedSettings.controls.erase("CineLightRigEnabled");p.page.installMasterWatch();
    gSavedSettings.controls["CineLightRigEnabled"];p.page.installMasterWatch();
    // giveActorRig itself must suspend setting-buffer writes, even with no
    // caller guard, for both an already-selected and a different slot.
    p.page.mRigActorConnection.disconnect();
    for(bool change:{false,true}){
        m=ALCineLightRigManager();m.mSelected=change?Slot::SELF:Slot::A;const int captured=m.cleanupWrites;
        assert(!ALKitHistory::suspended());m.giveActorRig(Slot::A,{"Alice"});
        assert(m.cleanupWrites>captured && !ALKitHistory::suspended());
    }
    // Anchored labels omit only a duplicate actor target. Named/legacy groups
    // and objects retain their useful target descriptions in the copied picker.
    for(int target=0;target<4;++target){
        m=ALCineLightRigManager();m.subject={};m.at(Slot::A).anchor={"Alice"};
        if(target==1)m.at(Slot::A).group="Extras";
        if(target==2){m.at(Slot::A).groupEnabled=true;m.at(Slot::A).slots=2;}
        if(target==3)m.at(Slot::A).object={"prop"};
        p.mCastListInitialized=false;p.anchor.open=false;p.updateAnchorList();
        const auto found=std::find(p.mCastValues.begin(),p.mCastValues.end(),1);assert(found!=p.mCastValues.end());
        const auto label=p.mCastNames[found-p.mCastValues.begin()];
        if(target==0)assert(label=="Rig 2: Alice");
        if(target==1)assert(label=="Rig 2: Alice: group Extras");
        if(target==2)assert(label=="Rig 2: Alice: group: Alice");
        if(target==3)assert(label=="Rig 2: Alice: prop (object)");
    }
    p.showBorrowMenu({"Alice"},true);auto* editMenu=p.mRigGroupMenu.get();bool editYour=false;
    for(auto* child:editMenu->children)if(auto* item=dynamic_cast<LLMenuItemCallGL*>(child);item && item->params.name=="edit_rig_0"){
        assert(item->params.label=="Edit your rig");editYour=true;}
    assert(editYour);
    // Non-rig context provider changes between record and replay. Its original
    // callbacks, key, page, validation and rewrite semantics remain intact.
    {
        ALKitHistory h;ALKitPage input;input.key="other/page";fixtureInput=&input;sKitInputSourceSet=true;
        ALKitRow original,proxy;original.mMeta.page_key=input.key;proxy.original=&original;sKitBindingSource={&proxy};
        int context=1,queried=0,replayed=0,validated=0;h.registerContext(input.key,[&](const std::string&){++queried;return std::to_string(context);});
        h.recordCustom(&input,"legacy key","Original label",[&](bool redo,std::string&){replayed+=redo?10:1;return true;},false,
            [&](bool,std::string&){++validated;return true;});
        const auto& step=h.mUndo.back();assert(step.label=="Original label" && step.page==input.key && step.custom.front().key=="legacy key");
        assert(step.custom.front().context.empty() && step.custom.front().binding_page.empty() && queried==0);
        context=2;h.undo(input.key);h.redo(input.key);assert(replayed==11 && validated>=2 && queried==0);
        fixtureInput=nullptr;sKitInputSourceSet=false;sKitBindingSource={};
    }
    // Resolve the original proxy row once per recording gesture, including
    // many custom-only snapshots. The next gesture picks up its new source.
    {
        ALKitHistory h;h.registerContext(p.page.mPage,p.context(),true);
        ALKitPage input;input.key="quick_group/quick_tab";fixtureInput=&input;sKitInputSourceSet=true;
        ALKitRow proxy,original;LLView child;child.parent=&proxy;proxy.original=&original;original.mMeta.page_key=p.page.mPage;sKitBindingSource={&child};
        h.beginGesture(ALKitHistory::MOUSE,input.key,"Preset");const int walks=LLView::parentReads;
        for(int i=0;i<20;++i)h.recordCustom(&input,"Light rig shafts","Preset",[](bool,std::string&){return true;},true);
        assert(LLView::parentReads-walks==1 && h.mOpen->custom.size()==20);
        for(const auto& entry:h.mOpen->custom)assert(!entry.context.empty() && entry.binding_page==p.page.mPage);
        // The same Quick gesture can cross source pages. Neither direction may
        // reuse the previous row's opt-in status, even with a non-rig provider.
        ALKitRow nonRig;nonRig.mMeta.page_key="other/page";int nonRigQueries=0;
        h.registerContext(nonRig.mMeta.page_key,[&](const std::string&){++nonRigQueries;return "non-rig context";});
        sKitBindingSource={&nonRig};h.recordCustom(&input,"Light rig shafts","Other",[](bool,std::string&){return true;},true);
        assert(h.mOpen->custom.back().context.empty() && h.mOpen->custom.back().binding_page.empty() && nonRigQueries==0);
        sKitBindingSource={&child};h.recordCustom(&input,"Light rig shafts","Rig",[](bool,std::string&){return true;},true);
        assert(!h.mOpen->custom.back().context.empty());
        h.finishGesture();original.mMeta.page_key="other/page";h.beginGesture(ALKitHistory::MOUSE,input.key,"Other preset");
        h.recordCustom(&input,"Light rig shafts","Preset",[](bool,std::string&){return true;},true);
        assert(h.mOpen->custom.front().context.empty());h.finishGesture();
        fixtureInput=nullptr;sKitInputSourceSet=false;sKitBindingSource={};
    }
    std::cout<<"PASS: real Actor-bar callback -> active signal -> Give chip -> master borrows actor without lighting You; pins/other pages; every pending termination -> one-click Off; opt-in custom history and cached proxy source; labels and suspension\n";
'''
    return source.replace('    // Retire from within a click:', tests+'\n    // Retire from within a click:')

