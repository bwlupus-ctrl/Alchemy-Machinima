"""Round-three regressions execute production label rebuilds, menus, Share and history."""
from rig_shadow_presets_check import function


def combo_fixture(root):
    src=(root/'indra/newview/aldirectorcastui.cpp').read_text(encoding='utf8')
    hdr=(root/'indra/newview/aldirectorcastui.h').read_text(encoding='utf8')
    declarations=hdr[hdr.index('using ValueProvider'):hdr.index('// Merge a static self/actor picker')]
    return ('namespace ALDirectorCastUI {\n'+declarations+'\n'+
            src[src.index('namespace\n{'):src.index('void mergeActorCombo(')]+'\n}\n')


def classification_oracle(root):
    import re,json,xml.etree.ElementTree as ET
    h=(root/'indra/newview/alcinelightrigmanager.h').read_text(encoding='utf8')
    store=function(h,'void toSettingsStore(')
    expected=set(re.findall(r'settings\.set\w+\(\s*"([^"]+)"',store))
    for role in ('Key','Fill','Rim','Bg'):
        for tail in re.findall(r'settings\.set\w+\(\s*prefix \+ "([^"]+)"',store):
            if tail!='GelSlot':expected.add('CineLightRig'+role+tail)
        for i in range(3):expected.add('CineLightRig'+role+'GelSlot'+str(i))
        for tail in re.findall(r'settings\.set\w+\(\s*rim_prefix \+ "([^"]+)"',store):expected.add('CineRigRim'+role+tail)
    code=('    const std::set<std::string> slotSettings={'+','.join(json.dumps(s) for s in sorted(expected))+'};\n'+
          '    for(const auto& [name,control]:gSavedSettings.controls)\n        if(name.find("CineLightRig")==0 || name.find("CineRigRim")==0)assert(!p.context()(name).empty()==bool(slotSettings.count(name)));\n')
    tree=ET.parse(root/'indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml')
    for row in tree.getroot().iter('kit_row'):
        if row.get('setting'):
            code+='    assert(!p.context()('+json.dumps(row.get('widget'))+').empty()=='+('true' if row.get('setting') in expected else 'false')+');\n'
    return code


def extend_fixture(root, source):
    source=source.replace('const char*="anchors"','const char* area="anchors"')
    source=source.replace('static constexpr Slot SLOT_SELF=Slot::SELF;',
        'static constexpr Slot SLOT_SELF=Slot::SELF,SLOT_A=Slot::A,SLOT_B=Slot::B,SLOT_C=Slot::C,SLOT_D=Slot::D;')
    source=source.replace('LLSD get() const { return {}; }',
        'LLSD getValue()const{return 0;}LLSD get() const { return {}; }')
    source=source.replace('LLControlVariable* getControlVariable(){return nullptr;}',
        'LLControlVariable* getControlVariable(){static LLControlVariable control;return &control;}')
    source=source.replace('struct LLUICtrl : LLView {','struct LLUICtrl : LLView {bool visible=true;')
    source=source.replace('namespace LLNotificationsUtil {\ninline void add(const char*, const LLSD&) {}\n}', '''
struct Notice {LLUUID id;bool cancelled=false,expired=false;LLSD data;const LLUUID& getID()const{return id;}bool isCancelled()const{return cancelled;}bool isRespondedTo()const{return false;}bool isActive()const{return !cancelled && !expired;}};
namespace LLNotificationsUtil {
inline std::map<LLUUID,std::shared_ptr<Notice>> notices;
inline std::shared_ptr<Notice> add(const char*,const LLSD& data){static int seq=0;auto p=std::make_shared<Notice>();p->id={std::to_string(++seq)};p->data=data;notices[p->id]=p;return p;}
inline std::shared_ptr<Notice> find(const LLUUID& id){auto it=notices.find(id);return it==notices.end()?nullptr:it->second;}
inline void cancel(const std::shared_ptr<Notice>& p){p->cancelled=true;}
}
''')
    source=source.replace('bool getBOOL(const std::string&)const{return true;}void setBOOL(const std::string&,bool){}',
        'std::map<std::string,LLSD> writes;bool getBOOL(const std::string& n)const{auto it=writes.find(n);return it==writes.end()?true:it->second.asBoolean();}void setBOOL(const std::string& n,bool b){writes[n]=b;if(n=="CineLightRigEnabled")powerWrite(b);}void setString(const std::string& n,const std::string& v){writes[n]=v;}static void powerWrite(bool);')
    pos=source.index('const char* ROLE_NAMES')
    source=source[:pos]+'''void HistorySettings::powerWrite(bool b){auto& m=ALCineLightRigManager::instance();m.selected().enabled=b;}
'''+source[pos:]
    # Fixture page lives inside the production panel, as in the real widget tree.
    source=source.replace('    int mFixtureRoleIndex=0,', '    ALPanelCineLightRig(){page.parent=this;}\n    int mFixtureRoleIndex=0,')
    # The actual unbound master commit callback from ALKitPage::build() is used
    # after menu dismissal; no stand-in toggle policy.
    page=(root/'indra/newview/alkitpage.cpp').read_text(encoding='utf8')
    from rig_borrow_check import block
    branch=block(page[page.index('if (mWhoAdapter == "cine_rig_slot")'):], 'if (mWhoAdapter == "cine_rig_slot")')
    callback=block(branch,'[this](LLUICtrl*, const LLSD&)')
    source=source.replace('bool activateWhoUnavailable();', 'void installMaster();bool activateWhoUnavailable();')
    source=source.replace('struct LLUICtrl : LLView {', 'struct LLUICtrl : LLView {bool isOn()const{return value.asBoolean();}')
    source=source.replace('int main(){', 'void KitPage::installMaster(){mMaster->setCommitCallback('+callback+');}\nint main(){')
    source=source.replace('void installMaster();', '''
    enum class SubjectValueSource{CONTROL,DANCE_FOCUS,DANCE_BLOCK_CENTRE,DANCE_WAVE_FOCUS};
    struct SubjectCombo{LLComboBox* mCombo;S32 mSelfValue=0,mSubjectAValue=1,mSubjectCount=4;SubjectValueSource mValueSource=SubjectValueSource::CONTROL;};
    std::vector<SubjectCombo> mSubjectCombos;void refreshSubjectCombos();
    void installMaster();''')
    dance='''struct ALDanceBall {static ALDanceBall& instance(){static ALDanceBall b;return b;}S32 autoJoinFocus(){return 0;}S32 blockingCentreFocus(){return 0;}struct Wave{S32 mFocus=0;};Wave wave(){return {};}};\n'''
    source=source.replace('int main(){',dance+function(page,'void ALKitPage::refreshSubjectCombos()').replace('ALKitPage::','KitPage::')+'\nint main(){')
    test=classification_oracle(root)+r'''
    // Real updateAnchorList -> real shared mergeSubjectCombo: cached Subject
    // names and untrusted ActorChoice labels must both be hidden after polling.
    m=ALCineLightRigManager();c.members.clear();c.groups.clear();
    m.subject={"subject-secret"};c.members[m.subject].mLastName="SUBJECT SECRET";
    p.mSubjectChoiceRevision=~U64(0);p.mCastListInitialized=false;
    RlvActions::visible=true;ALGazeNearby::allowed=true;p.updateAnchorList();
    assert(p.subjectCombo.items[1].label.find("SUBJECT SECRET")!=std::string::npos);
    assert(p.mRigDisplayedLabels["subjects"].count(m.subject));
    RlvActions::visible=false;p.refreshRigPrivacy();
    assert(p.subjectCombo.items[1].label.find("SECRET")==std::string::npos);
    assert(p.subjectCombo.items[1].label.find("Actor")!=std::string::npos);
    p.page.mSubjectCombos={{&p.subjectCombo}};p.page.refreshSubjectCombos();
    assert(p.subjectCombo.items[1].label.find("SECRET")==std::string::npos); // page's later revision-driven rebuild too
    for(auto& row:p.anchor.items)assert(row.label.find("SECRET")==std::string::npos);
    // Exactly rendered identities: unrelated Cast churn/visibility does not
    // close the currently open picker. A non-Cast Nearby menu row does.
    m=ALCineLightRigManager();m.subject={};c.members.clear();p.mRigDisplayedLabels.clear();
    p.mCastListInitialized=false;++c.revision;RlvActions::visible=true;p.updateAnchorList();
    LLVOAvatar nearby;nearby.id={"nearby-secret"};LLCharacter::sInstances={&nearby};
    p.showRigGroupMenu(&p.page.chip,true);
    assert(p.mRigDisplayedLabels["menu"].count(nearby.id));
    auto* nearbyMenu=p.mRigGroupMenu.get();assert(nearbyMenu && !nearbyMenu->children.empty());
    assert(static_cast<LLMenuItemCallGL*>(nearbyMenu->children[0])->params.label=="Nearby name");
    p.anchor.open=true;c.members[{"unrelated"}].mLastName="UNRELATED";
    RlvActions::visible=false;RlvActions::exceptions={gAgentID,nearby.id};p.refreshRigPrivacy();
    assert(p.anchor.open && !nearbyMenu->hidden); // only unrelated Cast got hidden
    RlvActions::exceptions.clear();p.refreshRigPrivacy();assert(nearbyMenu->hidden && !p.anchor.open);
    RlvActions::visible=true;p.showRigGroupMenu(&p.page.chip,true);nearbyMenu=p.mRigGroupMenu.get();
    ALGazeNearby::allowed=false;p.refreshRigPrivacy();assert(nearbyMenu->hidden);
    p.showRigGroupMenu(&p.page.chip,true);
    assert(!p.mRigDisplayedLabels["menu"].count(nearby.id)); // @shownearby removes enumeration
    assert(p.rigActorName(nearby.id,"test")=="Actor");ALGazeNearby::allowed=true;
    p.mRigDisplayedLabels.clear();p.mRigNearbyAllowed=true;ALGazeNearby::allowed=false;
    p.anchor.open=true;p.refreshRigPrivacy();assert(!p.anchor.open); // shownearby itself is in the signature
    ALGazeNearby::allowed=true;p.refreshRigPrivacy();
    RlvActions::visible=true;c.members.erase(nearby.id);p.ensureRigActorInCast(nearby.id);
    assert(!p.mRigNotices.empty());auto notice=LLNotificationsUtil::find(p.mRigNotices.back());
    assert(notice->data["MESSAGE"].asString().find("Nearby name")!=std::string::npos);
    RlvActions::visible=false;p.refreshRigPrivacy();assert(notice->isCancelled());RlvActions::visible=true;
    p.rigActorName(nearby.id,"notice");p.showRigNotice("Expired tip");auto expiredId=p.mRigNotices.back();
    LLNotificationsUtil::find(expiredId)->expired=true;p.refreshRigPrivacy();
    assert(std::find(p.mRigNotices.begin(),p.mRigNotices.end(),expiredId)==p.mRigNotices.end());
    assert(!p.mRigDisplayedLabels.count("notice:"+expiredId.asString()));
    LLCharacter::sInstances.clear();c.members.clear();
    // Exercise REAL shareRig, not the earlier callback double. An away non-Cast
    // Subject remains a member and keeps receiving light after sharing.
    for(bool legacyShare:{false,true}){
        m=ALCineLightRigManager();c.members.clear();c.groups.clear();m.subject={"non-cast-Bob"};c.live.insert({"Alice"});
        if(legacyShare){m.at(Slot::B).anchor={"non-cast-secondary"};m.at(Slot::A).groupEnabled=true;m.at(Slot::A).slots=6;m.at(Slot::A).members={{"non-cast-Bob"},{"non-cast-secondary"}};}
        p.shareRig({"Alice"},1);auto recipients=c.membersInGroup(m.at(Slot::A).group);
        assert(c.contains({"non-cast-Bob"}));
        assert(std::find(recipients.begin(),recipients.end(),LLUUID{"non-cast-Bob"})!=recipients.end());
        assert(std::find(recipients.begin(),recipients.end(),LLUUID{"Alice"})!=recipients.end());
        if(legacyShare)assert(std::find(recipients.begin(),recipients.end(),LLUUID{"non-cast-secondary"})!=recipients.end());
    }
    // Real menu + alkitpage cancellation + real master commit. The menu captures
    // its pick, so Escape/outside-click/Cancel leave no interception to clear.
    for(bool cancel:{false,true}){
        m=ALCineLightRigManager();c.members.clear();c.members[{"pending"}];
        p.page.mWhoUnavailableTarget={"pending"};gSavedSettings.setBOOL("CineLightRigEnabled",true);
        p.page.master.setValue(false);p.showBorrowMenu({"pending"});
        auto* menu=p.mRigGroupMenu.get();assert(p.page.mWhoUnavailableTarget.isNull() && p.page.master.isOn());
        if(cancel){auto* item=static_cast<LLMenuItemCallGL*>(menu->children.back());item->params.on_click.function(nullptr,{});}
        menu->hide();p.page.installMaster();p.page.master.setValue(false);p.page.master.onCommit();
        assert(!m.selected().enabled);p.page.master.setValue(true);p.page.master.onCommit();assert(m.selected().enabled);
    }
    // Global edits survive either retirement or Replace; per-slot power and EV
    // edits recorded before either lifetime boundary are rejected by real undo.
    for(bool replace:{false,true})for(const char* widget:{"CineLightRigLiveProbeRadius","cine_live_probe_radius","CineLightRigNightMaskDarkness","cine_night_mask_darkness","CineLightRigRandomInterval","CineRigRimEnabled","CineLightRigEnabled","cine_enabled","CineLightRigMasterEV","cine_master_ev"}){
        m=ALCineLightRigManager();c.members.clear();c.members[{"Alice"}];c.members[{"Bob"}];m.giveActorRig(Slot::A,{"Alice"});
        ALKitHistory h;h.mContexts["light_group/cine_light_rig_tab"]=p.context();
        LLUICtrl w;w.name=widget;w.value=2.;int replayed=0;w.commit=[&](){++replayed;};
        h.beginGesture(ALKitHistory::MOUSE,"light_group/cine_light_rig_tab","Change");h.recordWidget(&w,1.,2.,"Change");h.finishGesture();
        bool global=rigSlotHistoryKind(widget)==0;
        if(replace)m.giveActorRig(Slot::A,{"Bob"});else m.removeBorrowedRig(Slot::A);
        h.undo("light_group/cine_light_rig_tab");assert(replayed==(global?1:0));
        assert(w.value.asReal()==(global?1.:2.));
    }
    // Reusing the same actor preserves epoch; replacing a Subject increments it.
    m=ALCineLightRigManager();m.subject={"Bob"};auto epoch=m.historyEpoch(Slot::A);
    m.giveActorRig(Slot::A,{"Alice"});assert(m.historyEpoch(Slot::A)>epoch);epoch=m.historyEpoch(Slot::A);
    m.giveActorRig(Slot::A,{"Alice"});assert(m.historyEpoch(Slot::A)==epoch);
    std::cout<<"PASS: real Subject/combo/nearby rebuild privacy, exact rendered set, shownearby, real Share retains non-Cast Bob, menu cancellation/master, global vs slot retirement/Replace undo\n";
'''
    source=source.replace('    // Retire from within a click:',test+'\n    // Retire from within a click:')
    return source
