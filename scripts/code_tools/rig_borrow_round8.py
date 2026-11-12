"""Round-eight production actor-power, off Share, menu lease and notice regressions."""
from rig_shadow_presets_check import function


def extend_fixture(root, source):
    page=(root/'indra/newview/alkitpage.cpp').read_text(encoding='utf8')
    panel=(root/'indra/newview/alpanelcinelightrig.cpp').read_text(encoding='utf8')
    old='LLUUID actorTarget()const{return mWhoUnavailableTarget.notNull()?mWhoUnavailableTarget:mActorTarget.notNull()?mActorTarget:LLUUID{"new-body"};}'
    assert old in source
    source=source.replace(old,'LLUUID actorTarget()const;')
    source=source.replace('int main(){',function(page,'LLUUID ALKitPage::actorTarget() const').replace('ALKitPage::','KitPage::')+'\nint main(){')
    source=source.replace('LLUICtrl* whoChip(){return &chip;}', 'LLUICtrl* whoChip(){return mWhoChip;}')
    source=source.replace('struct ALPanelCineLightRig:LLView {','''struct ALPanelCineLightRig:LLView {
    bool visible=true;bool isInVisibleChain()const{return visible;}
    struct Clock{bool ready=true;int polls=0;bool due(){++polls;bool r=ready;ready=false;return r;}}mRigNoticeClock;
    bool mRigNoticeTimerRunning=false;
''')
    source=source.replace('void doOnIdleOneTime(std::function<void()> f){idle.push_back(f);}', '''void doOnIdleOneTime(std::function<void()> f){idle.push_back(f);}
std::vector<std::function<bool()>> repeating;
void doOnIdleRepeating(std::function<bool()> f){repeating.push_back(f);}
void tickNotices(){for(auto i=repeating.begin();i!=repeating.end();)if((*i)())i=repeating.erase(i);else ++i;}
''')
    # Execute the production null-filtered status loop with a name-call spy.
    start=panel.index('for (const auto& id : rig.lastResolvedStoredMembers())', panel.index('void ALPanelCineLightRig::syncGroupControls()'))
    from rig_borrow_check import block
    loop=block(panel[start:], 'for (const auto& id : rig.lastResolvedStoredMembers())')
    loop='for (const auto& id : rig.lastResolvedStoredMembers())'+loop[loop.index('{'):]
    extra='''std::string statusNames(const Rig& rig){std::string names;auto actorName=[](const LLUUID& id,const char*){assert(id.notNull());return id.asString();};'''+loop+'''return names;}\n'''
    assert 'else refreshRigNotices(true);' in function(panel,'void ALPanelCineLightRig::onVisibilityChange(')
    assert 'if (panel->mRigNoticeClock.due())' in function(panel,'void ALPanelCineLightRig::showRigNotice(')
    assert 'mRigDisplayedLabels["notice"]' not in function(panel,'void ALPanelCineLightRig::showRigNotice(')
    source=source.replace('int main(){',extra+'\nint main(){')
    tests=r'''
    // Hidden page built with only You; a Subject assignment occurs meanwhile.
    m=ALCineLightRigManager();c.members.clear();c.members[{"Alice"}];m.subject={};
    m.setSelectedSlot(Slot::SELF);p.page.mWhoPinned=false;p.page.mActorTarget={};p.page.visible=false;
    p.page.mWhoCombo=&p.anchor;p.mCastListInitialized=false;p.updateAnchorList();
    assert(p.anchor.items.size()==1);m.subject={"Alice"};
    p.page.connectWho(*shell);shell->mActorChoose=&actorChoose;shell->installActorChoose();
    actorChoose.onCommit();ALAvatarSubjectPicker::selected({"Alice"});
    assert(m.selectedSlot()==Slot::A && p.page.actorTarget()==LLUUID{"Alice"});
    p.page.visible=true;p.updateAnchorList();p.page.installMaster();
    p.page.master.setValue(true);p.page.master.onCommit();
    assert(m.at(Slot::A).enabled && !m.at(Slot::SELF).enabled);
    // Selection failure with an empty label must still intercept old-rig On.
    m.setSelectedSlot(Slot::SELF);auto* combo=p.mAnchorCombo;p.mAnchorCombo=nullptr;
    p.installGiveLabel();p.page.applyCastSelection({{"Alice"}});
    assert(p.page.mWhoUnavailableTarget==LLUUID{"Alice"});
    p.page.master.setValue(true);p.page.master.onCommit();assert(!m.at(Slot::SELF).enabled);
    p.mAnchorCombo=combo;p.page.refreshWhoUnavailable();
    assert(m.selectedSlot()==Slot::A && p.page.mWhoUnavailableTarget.isNull());
    // Independent final guard refuses a stale manager selection and re-resolves.
    m.setSelectedSlot(Slot::SELF);p.page.master.setValue(true);p.page.master.onCommit();
    assert(!m.at(Slot::SELF).enabled && m.selectedSlot()==Slot::A);
    p.page.mRigActorConnection.disconnect();
    // Off -> Share recovers authored legacy membership, not the owning Subject.
    m=ALCineLightRigManager();c.members.clear();c.groups.clear();c.members[{"Bob"}];c.members[{"Carol"}];c.members[{"Alice"}];
    m.subject={"Carol"};m.at(Slot::A).anchor={"Bob"};m.at(Slot::A).groupEnabled=true;m.at(Slot::A).slots=(1u<<0)|(1u<<2);
    m.at(Slot::A).members.clear();m.at(Slot::A).stored.clear();m.at(Slot::A).enabled=false;
    p.shareRig({"Alice"},1);auto recipients=c.membersInGroup(m.at(Slot::A).group);
    assert(std::find(recipients.begin(),recipients.end(),gAgentID)!=recipients.end());
    assert(std::find(recipients.begin(),recipients.end(),LLUUID{"Carol"})!=recipients.end());
    assert(std::find(recipients.begin(),recipients.end(),LLUUID{"Bob"})==recipients.end());
    assert(std::find(recipients.begin(),recipients.end(),LLUUID{"Alice"})!=recipients.end());
    // Off named groups keep authored membership as well, without adding Bob.
    m.at(Slot::A).group="authored";m.at(Slot::A).groupEnabled=false;
    m.at(Slot::A).members.clear();m.at(Slot::A).stored.clear();c.groups["authored"]={gAgentID,LLUUID{"Carol"}};
    p.shareRig({"Alice"},1);auto named=c.membersInGroup("authored");
    assert(std::find(named.begin(),named.end(),gAgentID)!=named.end() && std::find(named.begin(),named.end(),LLUUID{"Carol"})!=named.end());
    assert(std::find(named.begin(),named.end(),LLUUID{"Bob"})==named.end());
    // Captured Remove menus own a lease through rekey and compare canonical IDs.
    for(bool groupMenu:{false,true}){
        m=ALCineLightRigManager();c.members.clear();c.members[{"clone-old"}];m.giveActorRig(Slot::A,{"clone-old"});
        if(groupMenu)p.showRigGroupMenu(&p.page.chip,false);else p.showBorrowMenu({"clone-old"},true);
        LLMenuItemCallGL* remove=nullptr;for(auto* child:p.mRigGroupMenu.get()->children)
            if(auto* item=dynamic_cast<LLMenuItemCallGL*>(child);item && (item->params.name=="remove_rig" || item->params.name=="remove_borrowed_rig"))remove=item;
        assert(remove);auto removeAction=remove->params.on_click.function;
        // Keep only the chosen callback alive. A sibling Cast Give item also
        // owns this identity, and must not mask a missing Remove lease.
        std::function<void(LLContextMenu*)> releaseOthers=[&](LLContextMenu* menu){for(auto* child:menu->children){
            if(auto* item=dynamic_cast<LLMenuItemCallGL*>(child))item->params.on_click.function={};
            if(auto* sub=dynamic_cast<LLContextMenu*>(child))releaseOthers(sub);
        }};releaseOthers(p.mRigGroupMenu.get());
        c.members.erase({"clone-old"});c.members[{"clone-new"}];m.rewriteActorReferences({"clone-old"},{"clone-new"});
        assert(m.at(Slot::A).anchor==LLUUID{"clone-new"} && m.at(Slot::A).enabled);
        auto lease=m.mBorrowPicks.find({"clone-old"});assert(lease!=m.mBorrowPicks.end() && !lease->second.expired());m.refreshBorrowAnchors();
        removeAction(nullptr,{});assert(m.at(Slot::A).anchor.isNull() && !m.at(Slot::A).enabled);
    }
    // The Borrow menu requires a live chip anchor.
    auto* savedChip=p.page.mWhoChip;p.page.mWhoChip=nullptr;auto* oldMenu=p.mRigGroupMenu.get();
    p.showBorrowMenu({"Alice"});assert(p.mRigGroupMenu.get()==oldMenu);p.page.mWhoChip=savedChip;
    // Retired Cast Give menu item produces the neutral notice.
    m=ALCineLightRigManager();c.members.clear();c.members[{"Alice"}];p.showRigGroupMenu(&p.page.chip,true);
    auto* give=static_cast<LLMenuItemCallGL*>(p.mRigGroupMenu.get()->children.front());c.members.clear();
    const auto noticeCount=LLNotificationsUtil::notices.size();give->params.on_click.function(nullptr,{});
    assert(LLNotificationsUtil::notices.size()==noticeCount+1 && LLNotificationsUtil::find(p.mRigNotices.back())->data["MESSAGE"].asString()=="Actor unavailable");
    // Idle checks run without draw, at most once per clock tick. Only the
    // notice containing the actor whose visibility changed is canceled.
    c.members[{"Alice"}];c.members[{"Carol"}];RlvActions::visible=true;p.visible=true;
    p.clearRigLabels("notice");p.rigActorName({"Alice"},"notice");p.showRigNotice("Alice notice");auto aliceNotice=LLNotificationsUtil::find(p.mRigNotices.back());
    p.rigActorName({"Carol"},"notice");p.showRigNotice("Carol notice");auto carolNotice=LLNotificationsUtil::find(p.mRigNotices.back());
    p.showRigNotice("Neutral notice");auto neutralNotice=LLNotificationsUtil::find(p.mRigNotices.back());
    assert(!p.mRigDisplayedLabels.count("notice"));
    RlvActions::visible=false;RlvActions::exceptions={gAgentID,LLUUID{"Carol"}};
    p.mRigNoticeClock.ready=false;tickNotices();assert(!aliceNotice->isCancelled());
    p.mRigNoticeClock.ready=true;tickNotices();assert(aliceNotice->isCancelled() && !carolNotice->isCancelled() && !neutralNotice->isCancelled());
    p.refreshRigPrivacy();assert(!carolNotice->isCancelled() && !neutralNotice->isCancelled());
    p.visible=false;p.mRigNoticeClock.ready=true;tickNotices();assert(carolNotice->isCancelled() && !neutralNotice->isCancelled());
    RlvActions::visible=true;RlvActions::exceptions.clear();p.visible=true;
    Rig nullRecipients;nullRecipients.stored={{},{"Alice"},{},{"Carol"},{}};assert(statusNames(nullRecipients)=="Alice, Carol");
    std::cout<<"PASS: round 8 hidden Subject assignment -> actual Actor bar -> On; failed resolution interception; final power guard; Off Share mask; both rekeyed Remove leases; retired Cast Give; idle per-notice privacy and hide; null status IDs\n";
'''
    return source.replace('    // Retire from within a click:',tests+'\n    // Retire from within a click:')




# Scoped mutations are applied after all extracted production paths are built.
MUTATIONS = {
    'r8_cached_rig_rows': ('void KitPage::applyCastSelection(', 'if (mRigPanel && mRigPanel->selectActorRig(ids.front()))', 'if (selectWhoValue(ALKitWhoAdapters::get(mWhoAdapter)(ids.front())) && (mWhoCombo->onCommit(), true))'),
    'r8_failed_intercept': ('void KitPage::applyCastSelection(', 'mWhoUnavailableTarget = ids.front();', '/* omitted failed-selection interception */'),
    'r8_power_guard': ('void KitPage::installMaster()', 'if (LLDirectorCast::canonicalActorId(rigs.slotActor(rigs.selectedSlot())) != target)', 'if (false)'),
    'r8_legacy_mask': ('void ALPanelCineLightRig::shareRig(', 'if (rig.getGroupSlots() & (1u << i))', 'if (false)'),
    'r8_notice_timer': ('void ALPanelCineLightRig::showRigNotice(', 'if (panel->mRigNoticeClock.due()) panel->refreshRigNotices(!panel->isInVisibleChain());', '(void)panel->mRigNoticeClock.due();'),
    'r8_notice_blanket': ('void ALPanelCineLightRig::refreshRigNotices(', 'bool changed = false;', 'bool changed = true;'),
    'r8_notice_hidden': ('void ALPanelCineLightRig::showRigNotice(', 'refreshRigNotices(!panel->isInVisibleChain())', 'refreshRigNotices(false)'),
    'r8_chip_lease': ('void ALPanelCineLightRig::showBorrowMenu(', '[handle, captured, slot, was_cast, picked]()', '[handle, captured, slot, was_cast]()'),
    'r8_group_lease': ('void ALPanelCineLightRig::showRigGroupMenu(', ', pick = manager.retainBorrowActor(expected)', ''),
    'r8_chip_identity': ('void ALPanelCineLightRig::showBorrowMenu(', 'canonicalActorId(rigs.at(slot).getAnchor()) != actor', 'canonicalActorId(rigs.at(slot).getAnchor()) != captured'),
    'r8_group_identity': ('void ALPanelCineLightRig::showRigGroupMenu(', 'canonicalActorId(rigs.at(slot).getAnchor()) != actor', 'canonicalActorId(rigs.at(slot).getAnchor()) != expected'),
    'r8_cast_give_notice': ('void ALPanelCineLightRig::showRigGroupMenu(', 'if (actor.isNull()) { showRigNotice("Actor unavailable"); return; }', 'if (actor.isNull()) return;'),
    'r8_null_status': ('std::string statusNames(', 'if (id.isNull()) continue;', ''),
    'r8_null_menu_anchor': ('void ALPanelCineLightRig::showBorrowMenu(', '!mRigKitPage->whoChip() || ', ''),
}


def mutate(source, name):
    signature, old, new = MUTATIONS[name]
    start=source.index(signature)
    assert old in source[start:], (name, old)
    return source[:start]+source[start:].replace(old,new,1)
