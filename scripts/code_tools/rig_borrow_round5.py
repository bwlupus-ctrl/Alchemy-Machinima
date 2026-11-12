"""Round-five regressions exercise custom history and unchanged production refreshes."""
import json
import xml.etree.ElementTree as ET
from rig_shadow_presets_check import function


def extend_fixture(root, source):
    page = (root/'indra/newview/alkitpage.cpp').read_text(encoding='utf8')
    panel = (root/'indra/newview/alpanelcinelightrig.cpp').read_text(encoding='utf8')
    for signature in ('void ALKitPage::refreshWhoHint()', 'void ALKitPage::refreshSubjectCombos()'):
        body = function(page, signature)
        assert 'dynamic_cast<ALPanelCineLightRig*>' not in body and 'mRigPanel' in body
    assert 'mRigKitPage->setRigPanel(this);' in panel
    # The fixture's anchor and page combo now both commit through production
    # onAnchorSelected, including the same selection barrier as the real panel.
    source = source.replace('p.page.mRigPanel=&p;', '''p.page.mRigPanel=&p;
    p.anchor.commit=[&](){p.onAnchorSelected();};
    p.page.combo.commit=[&](){p.anchor.setValue(p.page.combo.getValue());p.onAnchorSelected();};''')
    source = source.replace('struct Rig {', '''struct Rig {
    static constexpr S32 LIGHT_COUNT=4;std::array<bool,LIGHT_COUNT> shafts{};
    bool isShaftEnabled(S32 i)const{return shafts[i];}void setShaftEnabled(S32 i,bool b){shafts[i]=b;}
    void applyShaftPreset();''')
    # Execute the production preset custom recorder and the complete shaft
    # snapshot/replay/validation block. Settings are unchanged in this fixture.
    presets = (root/'indra/newview/alkitpresets.cpp').read_text(encoding='utf8')
    rig = (root/'indra/newview/alcinelightrig.cpp').read_text(encoding='utf8')
    record = function(presets, 'void Load::recordState(')
    shafts = rig[rig.index('if (old_shafts != new_shafts)'):rig.index('// Record the loaded name')]
    declarations = '''
namespace ALPresetLook {
ALKitHistory* sLoadHistory=nullptr;
bool replaying(){return ALKitHistory::replaying();}
struct Load {void recordState(const std::string&,std::function<bool(bool,std::string&)>,std::function<bool(bool,std::string&)>);};
'''+record+'''
}
bool ALKitHistory::hasInputSource(){return true;}
void Rig::applyShaftPreset(){
    ALPresetLook::Load load;const auto old_shafts=shafts;
    for(S32 i=0;i<LIGHT_COUNT;++i)setShaftEnabled(i,false);
    setShaftEnabled(0,true);const auto new_shafts=shafts;
'''+shafts+'''
}
'''
    source = source.replace('int main(){', declarations+'\nint main(){')
    # Cross-check every fixture-editing row in the actual regenerated XUI.
    tree = ET.parse(root/'indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml')
    fixture_widgets = [r.get('widget') for r in tree.getroot().iter('kit_row')
                       if r.get('undo_scope') == 'page' and
                       (r.get('widget', '').startswith('cine_fixture_') or
                        r.get('widget', '').startswith('cine_gobo_'))]
    assert 'cine_gobo_preview' in fixture_widgets
    checks = ''.join('assert(rigSlotHistoryKind('+json.dumps(w)+')==2);\n' for w in fixture_widgets)
    tests = r'''
    // An unchanged production refresh, including a forced list rebuild and
    // the real hint/Subject refreshers, must preserve the actual shell picker.
    m=ALCineLightRigManager();m.mSelected=Slot::SELF;c.members.clear();c.members[{"Bob"}];
    m.at(Slot::A).anchor={"Bob"};p.page.mWhoAvailableAction={};p.page.mWhoUnavailableTarget={};
    p.page.mWhoCombo=&p.anchor;p.anchor.open=false;p.mCastListInitialized=false;p.updateAnchorList();
    LLButton stablePicker;installPicker(&p.page,&stablePicker);stablePicker.onCommit();
    auto* stable=shell->mOpenPopover;assert(stable && stable->visible);
    for(int i=0;i<10;++i){p.updateAnchorList();p.page.refreshWhoHint();p.page.refreshSubjectCombos();p.syncGroupControls();}
    p.mCastListInitialized=false;++c.revision;p.updateAnchorList();assert(stable->visible);
    c.members[{"Bob"}].mLastName="Renamed Bob";++c.revision;p.updateAnchorList();assert(!stable->visible);
    stablePicker.onCommit();stable=shell->mOpenPopover;assert(stable && stable->visible);
    m.at(Slot::B).anchor={"Alice"};p.updateAnchorList();assert(!stable->visible);
    // A privacy/visibility change still retires the snapshot when its authored
    // visible name happens to equal the neutral hidden label.
    m.at(Slot::B).anchor={};c.members[{"Bob"}].mLastName="Actor";++c.revision;p.updateAnchorList();
    stablePicker.onCommit();stable=shell->mOpenPopover;assert(stable && stable->visible);
    RlvActions::visible=false;++c.revision;p.updateAnchorList();assert(!stable->visible);
    RlvActions::visible=true;p.refreshRigPrivacy();
    p.page.mWhoCombo=&p.page.combo;
    // Actual Remove menu item -> finishRigAction -> production anchor commit.
    // An unfinished global edit survives; selecting SELF must precede the pin.
    m=ALCineLightRigManager();c.members.clear();c.members[{"Bob"}];m.giveActorRig(Slot::A,{"Bob"});
    p.page.mWhoCombo=&p.anchor;p.mCastListInitialized=false;p.anchor.open=false;p.updateAnchorList();
    shell->history().barrier("reset");auto& rh=shell->history();rh.registerContext(p.page.mPage,p.context(),true);
    LLUICtrl global;global.name="cine_live_probe_radius";global.value=2.;int globalReplays=0;global.commit=[&](){++globalReplays;};
    rh.beginGesture(ALKitHistory::MOUSE,p.page.mPage,"Radius");rh.recordWidget(&global,1.,2.,"Radius");
    p.showRigGroupMenu(&p.page.chip,false);auto* removeMenu=p.mRigGroupMenu.get();bool removed=false;
    for(auto* child:removeMenu->children)if(auto* item=dynamic_cast<LLMenuItemCallGL*>(child);item && item->params.name=="remove_borrowed_rig"){
        item->params.on_click.function(nullptr,{});removed=true;break;}
    assert(removed && m.selectedSlot()==Slot::SELF && m.at(Slot::A).anchor.isNull());
    rh.undo(p.page.mPage);assert(globalReplays==1);
    // Programmatic selection is history-preserving even when the slot changes.
    rh.barrier("reset");global.value=2.;rh.beginGesture(ALKitHistory::MOUSE,p.page.mPage,"Radius");rh.recordWidget(&global,1.,2.,"Radius");rh.finishGesture();
    {ALKitHistory::ScopedSuspend suspend;p.anchor.value=1;p.onAnchorSelected();}
    rh.undo(p.page.mPage);assert(globalReplays==2);p.page.mWhoCombo=&p.page.combo;
    // An unchanged setting entry is pruned, leaving only the production
    // preset shaft custom entry. The actual Replace callback must reject it.
    m=ALCineLightRigManager();c.members.clear();c.members[{"Bob"}];c.members[{"Alice"}];m.giveActorRig(Slot::A,{"Bob"});
    rh.barrier("reset");rh.registerContext(p.page.mPage,p.context(),true);ALPresetLook::sLoadHistory=&rh;
    LLUICtrl unchanged;unchanged.name="cine_master_ev";unchanged.value=1.;
    rh.beginGesture(ALKitHistory::MOUSE,p.page.mPage,"Preset");rh.recordWidget(&unchanged,1.,1.,"Preset");
    m.selected().applyShaftPreset();rh.finishGesture();assert(rh.mUndo.size()==1 && rh.mUndo.back().entries.empty() && rh.mUndo.back().custom.size()==1);
    assert(!rh.mUndo.back().custom.front().context.empty());
    p.showBorrowMenu({"Alice"});static_cast<LLMenuItemCallGL*>(p.mRigGroupMenu.get()->children.front())->params.on_click.function(nullptr,{});
    rh.undo(p.page.mPage);assert(m.at(Slot::A).shafts[0] && rh.mRedo.empty());
    // Search/Quick preset proxies own the action, while the custom snapshot
    // must retain the original rig row's context, just like setting entries.
    m=ALCineLightRigManager();m.subject={"Bob"};rh.barrier("proxy reset");
    ALKitPage proxy;proxy.key="quick_group/quick_tab";fixtureInput=&proxy;sKitInputSourceSet=true;
    ALKitRow proxyRow,originalRow;originalRow.mMeta.page_key=p.page.mPage;proxyRow.original=&originalRow;sKitBindingSource={&proxyRow};
    rh.beginGesture(ALKitHistory::MOUSE,proxy.key,"Preset");m.selected().applyShaftPreset();rh.finishGesture();
    assert(rh.mUndo.back().page==proxy.key && !rh.mUndo.back().custom.front().context.empty());
    m.subject={"Alice"};rh.undo(proxy.key);assert(m.selected().shafts[0] && rh.mRedo.empty());
    fixtureInput=nullptr;sKitInputSourceSet=false;sKitBindingSource={};
    // Reusing the same canonical actor after Remove must still reject stale
    // slot history through the epoch guard, while global history survives.
    for(bool slotEntry:{false,true}){
        m=ALCineLightRigManager();c.members.clear();c.members[{"Bob"}];m.giveActorRig(Slot::A,{"Bob"});
        ALKitHistory h;h.registerContext(p.page.mPage,p.context(),true);LLUICtrl edit;
        edit.name=slotEntry?"cine_master_ev":"cine_live_probe_radius";edit.value=2.;int replays=0;edit.commit=[&](){++replays;};
        h.beginGesture(ALKitHistory::MOUSE,p.page.mPage,"Edit");h.recordWidget(&edit,1.,2.,"Edit");h.finishGesture();
        m.removeBorrowedRig(Slot::A);m.giveActorRig(Slot::A,{"Bob"});h.undo(p.page.mPage);
        assert(replays==(slotEntry?0:1));
    }
    // The Cast-group patch in Make group edits global membership. Its paired
    // rig-selector widget has its own slot guard; the global custom patch does
    // not acquire actor context merely because it originated on the rig page.
    assert(p.context()("rig_make_group").empty());
    assert(!p.context()("Light rig shafts").empty() && !p.context()("cine_group_state").empty());
    {
        ALKitHistory h;h.registerContext(p.page.mPage,p.context(),true);int replays=0;
        h.recordCustom(&p,"rig_make_group","Groups",[&](bool,std::string&){++replays;return true;});
        assert(h.mUndo.back().custom.front().context.empty());m.subject={"another actor"};
        h.undo(p.page.mPage);assert(replays==1);
    }
    // Canonical actor changes without an epoch bump: Cast-side Subject
    // reassignment, scene replacement and clone identity rewrite. Both widget
    // and setting entries use the registered context; globals remain usable.
    for(int boundary=0;boundary<3;++boundary)for(bool setting:{false,true})for(bool slotEntry:{false,true}){
        m=ALCineLightRigManager();m.subject={"Bob"};c.members.clear();c.members[{"Bob"}];
        ALKitHistory h;h.registerContext(p.page.mPage,p.context(),true);LLUICtrl edit;
        edit.name=slotEntry?"cine_master_ev":"cine_live_probe_radius";edit.value=2.;int replays=0;edit.commit=[&](){++replays;};
        h.beginGesture(ALKitHistory::MOUSE,p.page.mPage,"Edit");h.recordWidget(&edit,1.,2.,"Edit");h.finishGesture();
        if(setting){auto& e=h.mUndo.back().entries.front();e.widget=false;e.key=slotEntry?"CineLightRigMasterEV":"CineLightRigLiveProbeRadius";e.context=p.context()(e.key);}
        const auto epoch=m.historyEpoch(Slot::A);
        if(boundary==0)m.subject={"Alice"};
        else if(boundary==1){m=ALCineLightRigManager();m.subject={"scene-Alice"};}
        else {m.at(Slot::A).anchor={"new-body"};}
        assert(m.historyEpoch(Slot::A)==epoch);
        std::string reason;assert(h.prepareReplay(p.page.mPage,false,reason)==!slotEntry);
        if(!setting){h.undo(p.page.mPage);assert(replays==(slotEntry?0:1));}
    }
    // Cancel and an explicit rig choice release the pending lease. Once its
    // actor is rekeyed, no abandoned alias remains after lifecycle pruning.
    for(bool cancel:{false,true}){
        m=ALCineLightRigManager();c.members.clear();c.members[{"pending-body"}];
        p.mPendingBorrowPick=m.retainBorrowActor({"pending-body"});p.page.mWhoUnavailableTarget={"pending-body"};
        c.members.erase({"pending-body"});c.members[{"new-body"}];++c.revision;m.rewriteActorReferences({"pending-body"},{"new-body"});
        assert(!m.mBorrowRekeys.empty());
        if(cancel)p.page.cancelWhoUnavailable();else assert(p.page.selectWhoValue(LLSD(0)));
        assert(!p.mPendingBorrowPick);++c.revision;m.refreshBorrowAnchors();assert(m.mBorrowRekeys.empty());
    }
    std::cout<<"PASS: unchanged real picker refresh, changed choices close, actual Remove/anchor commit preserves global undo, custom-only preset Replace rejected, canonical actor contexts, abandoned leases and XUI fixture classification\n";
'''
    return source.replace('    // Retire from within a click:', checks+tests+'\n    // Retire from within a click:')
