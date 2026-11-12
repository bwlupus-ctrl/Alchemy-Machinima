"""Round-nine membership, legacy SELF, selection and refresh regressions."""
from rig_shadow_presets_check import function
from rig_borrow_check import block


def extend_fixture(root, source):
    panel=(root/'indra/newview/alpanelcinelightrig.cpp').read_text(encoding='utf8')
    page=(root/'indra/newview/alkitpage.cpp').read_text(encoding='utf8')
    manager=(root/'indra/newview/alcinelightrigmanager.cpp').read_text(encoding='utf8')
    assert '\u00e2\u20ac' not in panel
    source=source.replace('namespace ALGazeNearby {bool allowed=true;', 'namespace ALGazeNearby {std::string nearbyName="Nearby name";bool allowed=true;')
    source=source.replace('return allowed?"Nearby name":"";', 'return allowed?nearbyName:"";')
    source=source.replace('struct KitPage:LLView {','struct KitPage:LLView {void syncRigTarget();')
    source=source.replace('void syncAnchorSelection(){}','void syncAnchorSelection();')
    source=source.replace('void clearPendingBorrowPick(){mPendingBorrowPick.reset();}',
        'void clearPendingBorrowPick(){mPendingBorrowPick.reset();mPendingBorrowLabelValid=false;}')
    source=source.replace('struct ALPanelCineLightRig:LLView {','''struct ALPanelCineLightRig:LLView {
    std::string pendingBorrowLabel(const LLUUID&);LLUUID mPendingBorrowLabelActor;
    std::string mPendingBorrowLabel,mPendingBorrowLabelNearbyName;U64 mPendingBorrowLabelRevision=0;S32 mPendingBorrowLabelSlot=-1;
    bool mPendingBorrowLabelValid=false,mPendingBorrowLabelVisible=false,mPendingBorrowLabelNearby=false;
''')
    source=source.replace('void giveActorRig(Slot,const LLUUID&);','void restoreSelfAnchor();void giveActorRig(Slot,const LLUUID&);')
    source=source.replace('std::shared_ptr<const LLUUID> ALCineLightRigManager::retainBorrowActor(const LLUUID& actor)\n{',
        'int leaseCalls=0,chipClears=0;\nstd::shared_ptr<const LLUUID> ALCineLightRigManager::retainBorrowActor(const LLUUID& actor)\n{\n++leaseCalls;')
    source=source.replace('void ALPanelCineLightRig::clearRigLabels(const char* area)\n{',
        'extern int chipClears;\nvoid ALPanelCineLightRig::clearRigLabels(const char* area)\n{\nif(std::string(area)=="chip")++chipClears;')
    source=source.replace('std::map<std::string,uuid_vec_t> groups;', 'std::map<std::string,uuid_vec_t> groups;uuid_vec_t lastRigGroupRecipients;')
    source=source.replace('void addToGroup(const uuid_vec_t& ids,const std::string& n){', 'void addToGroup(const uuid_vec_t& ids,const std::string& n){lastRigGroupRecipients=ids;')
    # A phase spy rejects selection while a rebuild still has unfinished work.
    # All production resolution sites are at a completed return/end boundary.
    signature='void ALPanelCineLightRig::updateAnchorList(bool refresh_who)\n{'
    source=source.replace(signature, 'bool unfinishedAnchorBuild=false;\n'+signature+'\n    unfinishedAnchorBuild=true;',1)
    source=source.replace('if (refresh_who && mRigKitPage) mRigKitPage->refreshWhoUnavailable();',
        'unfinishedAnchorBuild=false; if (refresh_who && mRigKitPage) mRigKitPage->refreshWhoUnavailable();')
    signature='bool ALPanelCineLightRig::selectActorRig(const LLUUID& id)\n{'
    source=source.replace(signature, 'extern bool unfinishedAnchorBuild;\n'+signature+'\n    assert(!unfinishedAnchorBuild);',1)
    definitions='void setValueIfChanged(LLUICtrl* c,const LLSD& value,bool){if(c)c->setValue(value); }\n'+function(page,'void ALKitPage::syncRigTarget()').replace('ALKitPage::','KitPage::')+'\n'
    definitions+='\n'.join(function(panel,sig) for sig in ('std::string ALPanelCineLightRig::pendingBorrowLabel(', 'void ALPanelCineLightRig::syncAnchorSelection()'))
    definitions+='\n'+function(manager,'void ALCineLightRigManager::restoreSelfAnchor()')
    # Execute the actual checkbox caption loop, with only its widget write doubled.
    controls=function(panel,'void ALPanelCineLightRig::syncGroupControls()')
    at=controls.index('    for (S32 i = 0; i < 4; ++i)',controls.index('    auto label ='))
    loop=block(controls[at:],'for (S32 i = 0; i < 4; ++i)')
    definitions+='''\nstd::vector<std::string> checkboxCaptions(const LLUUID* subjects){
    std::vector<std::string> captions(5,"stale Carol");
    auto label=[&](S32 i,const std::string& s){captions[i]=s;};
    auto rigActorName=[](const LLUUID& id,const char*){return id.asString();};
    auto rigLabelBase=[](S32 i){return "Rig "+std::to_string(i+1);};
    for (S32 i = 0; i < 4; ++i)'''+loop[loop.index('{'):]+'\nreturn captions;}\n'
    source=source.replace('int main(){',definitions+'\nint main(){')
    tests=r'''
    // Partially available legacy group: Carol is authored but away; You resolves.
    m=ALCineLightRigManager();c.members.clear();c.groups.clear();c.canonical.clear();
    c.members[{"Alice"}];c.members[{"Carol"}];c.members[{"Bob"}];m.subject={"Carol"};
    auto& shared=m.at(Slot::A);shared.anchor={"Bob"};shared.groupEnabled=true;shared.slots=(1u<<0)|(1u<<2);
    shared.members={gAgentID};shared.stored={gAgentID};shared.enabled=true;
    p.shareRig({"Alice"},1);auto legacy=c.membersInGroup(shared.group);
    assert(std::count(legacy.begin(),legacy.end(),LLUUID{"Carol"})==1);
    assert(std::count(legacy.begin(),legacy.end(),gAgentID)==1);
    assert(std::find(legacy.begin(),legacy.end(),LLUUID{"Bob"})==legacy.end());
    // Named authored membership and effective fallback are also a union.
    c.groups.clear();c.groups["authored"]={{"Carol"}};shared.group="authored";shared.groupEnabled=false;
    shared.members={gAgentID};shared.stored={gAgentID};p.shareRig({"Alice"},1);
    auto namedUnion=c.membersInGroup("authored");
    assert(std::find(c.lastRigGroupRecipients.begin(),c.lastRigGroupRecipients.end(),LLUUID{"Carol"})!=c.lastRigGroupRecipients.end());
    assert(std::find(namedUnion.begin(),namedUnion.end(),LLUUID{"Carol"})!=namedUnion.end());
    assert(std::find(namedUnion.begin(),namedUnion.end(),gAgentID)!=namedUnion.end());
    assert(std::find(namedUnion.begin(),namedUnion.end(),LLUUID{"Bob"})==namedUnion.end());
    // Legacy anchored SELF -> actual Actor-bar You -> master and banner On.
    m=ALCineLightRigManager();c.members.clear();c.members[{"Alice"}];m.subject={};
    m.freeSlot=Slot::A;m.at(Slot::SELF).anchor={"Alice"};m.mBlobs[0].mAnchor={"Alice"};m.setSelectedSlot(Slot::A);
    p.page.mWhoPinned=false;p.page.mWhoUnavailableTarget={};p.page.mActorTarget={};p.page.mWhoCombo=&p.anchor;
    p.anchor.open=false;p.anchor.focus=false;p.mCastListInitialized=false;p.updateAnchorList();
    p.installGive();p.installGiveLabel();p.page.installMaster();p.page.connectWho(*shell);
    shell->mActorChoose=&actorChoose;shell->installActorChoose();
    actorChoose.onCommit();ALAvatarSubjectPicker::selected(gAgentID);
    assert(m.selectedSlot()==Slot::SELF && m.at(Slot::SELF).anchor.isNull() && m.mBlobs[0].mAnchor.isNull());
    assert(p.page.actorTarget()==gAgentID);
    LLUICtrl selfBanner;p.page.installBanner(&selfBanner);
    for(bool banner:{false,true}){
        gSavedSettings.setBOOL("CineLightRigEnabled",false);p.page.master.setValue(false);
        if(banner)selfBanner.onCommit();else {p.page.master.setValue(true);p.page.master.onCommit();}
        assert(m.at(Slot::SELF).enabled && m.slotActor(m.selectedSlot())==gAgentID);
    }
    actorChoose.onCommit();ALAvatarSubjectPicker::selected({"Alice"});
    p.page.master.setValue(true);p.page.master.onCommit();
    assert(m.selectedSlot()==Slot::A && m.at(Slot::A).anchor==LLUUID{"Alice"} && m.at(Slot::A).enabled);
    p.page.mRigActorConnection.disconnect();
    // Direct combo edits and passive scene/model selection both refresh target.
    m.at(Slot::B).anchor={"Bob"};p.mCastListInitialized=false;p.updateAnchorList();
    p.anchor.setValue(LLSD(2));p.anchor.onCommit();assert(p.page.actorTarget()==LLUUID{"Bob"});
    p.page.master.setValue(true);p.page.master.onCommit();assert(m.at(Slot::B).enabled);
    m.at(Slot::A).anchor={"Carol"};m.setSelectedSlot(Slot::A);p.updateAnchorList();
    assert(p.page.actorTarget()==LLUUID{"Carol"});p.page.master.setValue(true);p.page.master.onCommit();assert(m.at(Slot::A).enabled);
    // A pinned page must use the synchronized target even while keyboard focus
    // keeps the presentation combo on the previous slot.
    p.page.mWhoPinned=true;p.anchor.focus=true;m.setSelectedSlot(Slot::B);p.updateAnchorList();
    assert(p.page.actorTarget()==LLUUID{"Bob"});gSavedSettings.setBOOL("CineLightRigEnabled",false);p.page.master.setValue(true);p.page.master.onCommit();assert(m.at(Slot::B).enabled);
    p.anchor.focus=false;p.page.mWhoPinned=false;
    // A pending actor gains a rig during refresh. The completed rows/caches
    // and model selection must agree, without resuming an outer stale rebuild.
    c.members[{"Later"}];p.page.applyCastSelection({{"Later"}});assert(p.page.mWhoUnavailableTarget==LLUUID{"Later"});
    m.at(Slot::B).anchor={"Later"};p.mCastListInitialized=false;p.updateAnchorList();
    assert(m.selectedSlot()==Slot::B && p.page.mWhoUnavailableTarget.isNull());
    assert(p.mDisplayedSlot==2 && p.anchor.getValue().asInteger()==2 && (p.mAnchorVisibilityState>>10)==2);
    assert(p.page.actorTarget()==LLUUID{"Later"});p.page.master.setValue(true);p.page.master.onCommit();assert(m.at(Slot::B).enabled);
    // Unchanged pending Give performs no further lease/ledger work per tick.
    c.members[{"Waiting"}].mLastName="Waiting";p.page.applyCastSelection({{"Waiting"}});
    const auto leases=leaseCalls,clears=chipClears;auto waitingLease=p.mPendingBorrowPick;
    for(int tick=0;tick<20;++tick){p.updateAnchorList();p.page.refreshWhoUnavailable();}
    assert(leaseCalls==leases && chipClears==clears && p.mPendingBorrowPick==waitingLease);
    ++c.revision;c.members[{"Waiting"}].mLastName="Renamed";p.page.refreshWhoUnavailable();assert(p.page.label=="Give Renamed a rig");
    const auto visibleClears=chipClears;RlvActions::visible=false;p.page.refreshWhoUnavailable();assert(chipClears>visibleClears && p.page.label.find("Renamed")==std::string::npos);RlvActions::visible=true;
    p.page.cancelWhoUnavailable();
    // Nearby display-name arrival/rename changes no Cast revision.
    c.live.insert({"nearby-waiting"});p.page.applyCastSelection({{"nearby-waiting"}});
    assert(p.page.label=="Give Nearby name a rig");const auto nearbyLeases=leaseCalls,nearbyClears=chipClears;
    ALGazeNearby::nearbyName="Fresh nearby name";p.page.refreshWhoUnavailable();
    assert(p.page.label=="Give Fresh nearby name a rig" && leaseCalls==nearbyLeases && chipClears==nearbyClears+1);
    p.page.refreshWhoUnavailable();assert(leaseCalls==nearbyLeases && chipClears==nearbyClears+1);
    p.page.cancelWhoUnavailable();ALGazeNearby::nearbyName="Nearby name";
    LLUUID captionSubjects[4]={{"Carol"},{},{},{}};auto captions=checkboxCaptions(captionSubjects);assert(captions[1].find("Carol")!=std::string::npos);
    captionSubjects[0]={};captions=checkboxCaptions(captionSubjects);assert(captions[1]=="Subject A");
    std::cout<<"PASS: round 9 partial Share union; legacy SELF Actor-bar master/banner; direct and scene selection target; deferred resolution; pending label cache; neutral cleared Subject caption\n";
'''
    return source.replace('    // Retire from within a click:',tests+'\n    // Retire from within a click:')


MUTATIONS={
    'r9_partial_legacy':('void ALPanelCineLightRig::shareRig(', 'else if (rig.isGroupEnabled())','else if (recipients.empty() && rig.isGroupEnabled())'),
    'r9_partial_named':('void ALPanelCineLightRig::shareRig(', 'if (!rig.getLitGroup().empty())','if (recipients.empty() && !rig.getLitGroup().empty())'),
    'r9_legacy_self':('bool ALPanelCineLightRig::selectActorRig(', 'manager.restoreSelfAnchor();','(void)manager;'),
    'r9_combo_target':('void ALPanelCineLightRig::onAnchorSelected()', 'if (mRigKitPage) mRigKitPage->syncRigTarget();','/* no target sync */'),
    'r9_pinned_target':('LLUUID KitPage::actorTarget() const', 'if (mActorTarget.notNull()) return mActorTarget;', 'if (!mWhoPinned && mActorTarget.notNull()) return mActorTarget;'),
    'r9_scene_target':('void ALPanelCineLightRig::syncAnchorSelection()', 'if (mRigKitPage) mRigKitPage->syncRigTarget();','/* no target sync */'),
    'r9_reentrant':('void ALPanelCineLightRig::updateAnchorList(', '    // Only the combo\'s rows', '    if (refresh_who && mRigKitPage) mRigKitPage->refreshWhoUnavailable();\n    // Only the combo\'s rows'),
    'r9_nearby_name':('std::string ALPanelCineLightRig::pendingBorrowLabel(', 'mPendingBorrowLabelNearbyName == nearby_name', 'true'),
    'r9_pending_cache':('std::string ALPanelCineLightRig::pendingBorrowLabel(', 'if (mPendingBorrowLabelValid &&', 'if (false &&'),
    'r9_neutral_caption':('std::vector<std::string> checkboxCaptions(', 'label(i + 1, llformat("Subject %c", \'A\' + i));','(void)i;'),
}

def mutate(source,name):
    signature,old,new=MUTATIONS[name];start=source.index(signature);end=source.index('\n}',start)+2
    body=source[start:end];assert old in body,(name,old)
    # Both direct-selection returns must lose their sync for the planted variant.
    body=body.replace(old,new)
    return source[:start]+body+source[end:]
