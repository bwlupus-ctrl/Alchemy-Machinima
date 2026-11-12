"""Round-one regressions: extracted menu callbacks, privacy, retirement and replay."""
from rig_shadow_presets_check import function, SHIM
from rig_sway_recovery import extended_shim
from rig_group_history_check import history_fixture
from rig_borrow_check import block


def fixture(root, mutation=''):
    panel = (root/'indra/newview/alpanelcinelightrig.cpp').read_text(encoding='utf8')
    manager = (root/'indra/newview/alcinelightrigmanager.cpp').read_text(encoding='utf8')
    page = (root/'indra/newview/alkitpage.cpp').read_text(encoding='utf8')
    shell = (root/'indra/newview/alkitshell.cpp').read_text(encoding='utf8')
    # Prove the real chip handler calls activation BEFORE opening the generic picker.
    chip = shell[shell.index('if (LLButton* who = page->whoChip();'):]
    assert chip.index('if (page->activateWhoUnavailable())') < chip.index('if (ALKitPopover* old')
    assert 'mWhoUnavailableTarget.notNull()' not in chip[:chip.index('if (ALKitPopover* old')]
    privacy = function(panel, 'void ALPanelCineLightRig::refreshRigPrivacy()')
    assert 'setBehaviourCallback' not in panel
    for required in ('mCastListInitialized = false', 'mLitTargetForceRefresh = true',
                     'mDisplayedGroupSlots = ~0u', 'retireRigMenu(', 'updateAnchorList()',
                     'syncGroupControls()', 'refreshWhoUnavailable()', 'hidePopover()'):
        assert required in privacy, 'missing restriction invalidation: '+required
    menu_body = function(panel, 'void ALPanelCineLightRig::showBorrowMenu(')
    assert menu_body.index('if (slot ==') < menu_body.index('create<LLContextMenu>')
    assert 'markDead' not in panel
    assert panel.count('mLastName') == 2, 'cached rig names must only be used by rigActorName'
    for signature in ('ALPanelCineLightRig::~ALPanelCineLightRig()',
                      'void ALPanelCineLightRig::showRigGroupMenu(',
                      'void ALPanelCineLightRig::showBorrowMenu('):
        body = function(panel, signature)
        assert 'delete ' not in body and 'retireRigMenu(' in body
    assert 'data["anchor"] = legacyAnchorForSlot(mSelected);' in function(manager, 'LLSD ALCineLightRigManager::sceneData()')
    assert '(value == 0 || rig.getAnchor().isNull())' in function(panel, 'void ALPanelCineLightRig::updateAnchorList(')
    assert 'expected.isNull() ? llformat("Rig %d"' in function(panel, 'void ALPanelCineLightRig::showBorrowMenu(')
    from rig_borrow_round2 import ownership_fixture, history_services
    lines = [extended_shim(SHIM), '#include <boost/signals2.hpp>\n#include <set>\n#include <unordered_set>\n#include <cstdio>\nenum class ALCineLightRigSlot {SELF,A,B,C,D,COUNT};', history_services(root, history_fixture(root)), ownership_fixture(root), r'''
template<class... A> std::string llformat(const char* fmt,A... args){char s[256];std::snprintf(s,sizeof(s),fmt,args...);return s;}
const LLUUID gAgentID{"self"};
namespace RlvActions {enum {SNC_DEFAULT};bool visible=true;std::set<LLUUID> exceptions;bool canShowName(int,const LLUUID& id){return visible || exceptions.count(id);}}
namespace ALGazeNearby {bool allowed=true;bool nearbyAllowed(){return allowed;}std::string nonCastTargetName(const LLUUID&){return allowed?"Nearby name":"";}}
struct LLCharacter {static inline std::vector<LLCharacter*> sInstances;virtual ~LLCharacter()=default;};
struct LLVOAvatar:LLCharacter {LLUUID id;bool mIsDummy=false;bool isDead()const{return false;}bool isControlAvatar()const{return false;}bool isGhostAvatar()const{return false;}LLUUID getID()const{return id;}};
struct LLDirectorCast {
    struct Member{std::string mLastName,mGroup;LLUUID mId;};using CastMember=Member; std::map<LLUUID,Member> members;
    struct ActorChoice {LLUUID mId;std::string mLabel;S32 slot=0;bool isSubject(S32 s)const{return slot==s;}};
    std::vector<ActorChoice> getActorChoices()const{return {{gAgentID,"You",0},{ALCSubject(),"UNTRUSTED CHOICE",1}};}
    LLUUID ALCSubject()const;
    U64 revision=1;U64 getCastRevision()const{return revision;}U64 getActorChoiceRevision()const{return revision;}U64 getGroupRevision()const{return 1;}
    std::map<std::string,uuid_vec_t> groups;
    struct Snapshot{std::map<LLUUID,std::string> tags;};Snapshot captureGroups()const{Snapshot s;for(auto& [id,m]:members)s.tags[id]=m.mGroup;return s;}
    std::vector<Member> getCast()const{std::vector<Member> v;for(auto [id,m]:members){m.mId=id;v.push_back(m);}return v;}
    void add(const LLUUID& id){members[id];}
    uuid_vec_t membersInGroup(const std::string& n)const{auto i=groups.find(n);return i==groups.end()?uuid_vec_t():i->second;}
    std::string makeGroup(const uuid_vec_t& ids){groups["Group 1"].clear();for(auto& id:ids)if(members.count(id)){groups["Group 1"].push_back(id);members[id].mGroup="Group 1";}return "Group 1";}
    void addToGroup(const uuid_vec_t& ids,const std::string& n){for(auto& id:ids)if(members.count(id)){groups[n].push_back(id);members[id].mGroup=n;}}
    std::set<LLUUID> live; static inline std::map<LLUUID,LLUUID> canonical;
    static LLDirectorCast& instance(){static LLDirectorCast c;return c;}
    static bool isSelf(const LLUUID& id){return id==gAgentID;}
    static LLUUID canonicalActorId(const LLUUID& id){auto i=canonical.find(id);return i==canonical.end()?id:i->second;}
    const uuid_vec_t& getIds()const{static uuid_vec_t ids;ids.clear();for(const auto& m:members)ids.push_back(m.first);return ids;}
    LLUUID getSubjectA()const{return {}; }LLUUID getSubjectB()const{return {}; }LLUUID getSubjectC()const{return {}; }LLUUID getSubjectD()const{return {}; }
    bool contains(const LLUUID& id)const{return members.count(canonicalActorId(id));}
    const Member* getMember(const LLUUID& id)const{auto i=members.find(canonicalActorId(id));return i==members.end()?nullptr:&i->second;}
    LLVOAvatar* resolve(const LLUUID& id,bool){static LLVOAvatar a;return live.count(id)?&a:nullptr;}
};
struct ALGhostStudio {std::set<LLUUID> deleting;static ALGhostStudio& instance(){static ALGhostStudio s;return s;}bool isDeleting(const LLUUID& id)const{return deleting.count(id);}};
struct Rig {
    LLUUID anchor;bool enabled=false;std::string group;uuid_vec_t members,stored;
    LLUUID object;bool groupEnabled=false;U32 slots=0;
    const std::string& getLitGroup()const{return group;}U32 getGroupSlots()const{return slots;}U32 lastResolvedGroupSlots()const{return slots;}
    const uuid_vec_t& lastResolvedMembers()const{return members;}const uuid_vec_t& lastResolvedStoredMembers()const{return stored;}bool isGroupEnabled()const{return groupEnabled;}LLUUID getObjectTarget()const{return object;}
    const LLUUID& getAnchor()const{return anchor;}void setAnchor(const LLUUID& id){anchor=id;}
    void setObjectTarget(const LLUUID& id){object=id;}void setGroupEnabled(bool b){groupEnabled=b;}void setLitGroup(const std::string& s){group=s;}
};
struct Blob {LLUUID mAnchor;bool mEnabled=false;void captureRigState(const Rig& rig){mAnchor=rig.anchor;}};
using Slot=ALCineLightRigSlot;
S32 slotIndex(Slot s){return static_cast<S32>(s);}
struct ALCineLightRigManager {
    using Slot=ALCineLightRigSlot;static constexpr Slot SLOT_SELF=Slot::SELF;static constexpr S32 SLOT_COUNT=5;
    Rig mInstances[5];Blob mBlobs[5];Slot mSelected=Slot::A;std::array<LLUUID,4> mEnrolledAnchors{};
    std::map<LLUUID,std::weak_ptr<const LLUUID>> mBorrowPicks;U64 mBorrowCastRevision=~U64(0);bool mBorrowAnchorsDirty=true;std::shared_ptr<const LLUUID> retainBorrowActor(const LLUUID&);void refreshBorrowAnchors();
    std::array<U64,5> mHistoryEpoch{};std::map<LLUUID,LLUUID> mBorrowRekeys;U64 mConfigurationRevision=0;
    LLUUID subject{"Bob"};int mutations=0,cleanupWrites=0;
    static ALCineLightRigManager& instance(){static ALCineLightRigManager m;return m;}
    LLUUID resolveRigActorIdentity(const LLUUID&)const;
    LLUUID resolveBorrowActor(const LLUUID&,bool=false)const;
    void rewriteActorReferences(const LLUUID&,const LLUUID&);void removeBorrowedRig(Slot);
    void giveActorRig(Slot,const LLUUID&);void pruneRemovedAnchors();
    bool validSlot(Slot s)const{return s>=Slot::SELF && s<Slot::COUNT;}
    Blob captureSelected()const{Blob b;b.captureRigState(mInstances[slotIndex(mSelected)]);b.mEnabled=mInstances[slotIndex(mSelected)].enabled;return b;}
    Rig& at(Slot s){return mInstances[slotIndex(s)];}const Rig& at(Slot s)const{return mInstances[slotIndex(s)];}
    Rig& selected(){return at(mSelected);}const Rig& selected()const{return at(mSelected);}Slot selectedSlot()const{return mSelected;}
    bool isSlotEnabled(Slot s)const{return at(s).enabled;}
    LLUUID groupSlotActor(Slot s)const;Slot actorSlot(const LLUUID&)const;
    LLUUID slotActor(Slot s)const{return at(s).anchor.notNull()?at(s).anchor:s==Slot::SELF?gAgentID:subject;}
    Slot borrowSlot(const LLUUID&)const{return Slot::COUNT;}Slot leastRecentlyUsedSlot()const{return Slot::A;}
    void setSelectedSlot(Slot s){mSelected=s;}void touchSlot(Slot){}
    int enabledCount()const{int n=0;for(auto& r:mInstances)n+=r.enabled;return n;}
    void setLitGroup(Slot s,const std::string& n){at(s).group=n;++mutations;}
    void writeSelectedSettings(){at(mSelected).enabled=mBlobs[slotIndex(mSelected)].mEnabled;++mutations;if(ALKitHistory::suspended())++cleanupWrites;}
    void shutdownSlot(S32 i){mInstances[i].enabled=false;}void persist(){}void removeMirror(const LLUUID&){}
    U64 historyEpoch(Slot s)const{return mHistoryEpoch[slotIndex(s)];}
};
LLUUID LLDirectorCast::ALCSubject()const{return ALCineLightRigManager::instance().subject;}
const char* ROLE_NAMES[]={"Key","Fill","Rim","Bg"};
struct LLScrollListItem {LLSD value;std::string label;bool getEnabled()const{return true;}const LLSD& getValue()const{return value;}
struct Column{LLSD v;LLSD getValue()const{return v;}};Column column;Column* getColumn(S32){column.v=label;return &column;}};
struct LLComboBox:LLUICtrl {
    std::vector<LLScrollListItem> items;bool open=false,focus=false;
    void hideList(){open=false;}void setFocus(bool b){focus=b;}
    bool hasFocus()const{return focus;}bool isListVisible()const{return open;}
    LLControlVariable* getControlVariable(){return nullptr;}
    void clearRows(){items.clear();}void add(const std::string& label,const LLSD& value,S32=0,bool=true){items.push_back({value,label});}
    std::vector<LLScrollListItem*> getAllData(){std::vector<LLScrollListItem*> v;for(auto& i:items)v.push_back(&i);return v;}
    int getCurrentIndex()const{for(int i=0;i<items.size();++i)if(items[i].value==value)return i;return -1;}
    LLSD getSelectedValue()const{return value;}
};
std::string whoChipLabel(const std::string& label,LLComboBox*,bool){return label+" current";}
namespace ALKitWhoAdapters {using Adapter=std::function<LLSD(const LLUUID&)>;Adapter get(const std::string&){return [](const LLUUID& id){auto s=ALCineLightRigManager::instance().actorSlot(id);return s==Slot::COUNT?LLSD():LLSD(static_cast<S32>(s));};}}
struct ALPanelCineLightRig;
struct KitPage:LLView {
    ALPanelCineLightRig* mRigPanel=nullptr;
    LLUUID mWhoUnavailableTarget,mActorTarget;std::function<bool()> mWhoAvailableAction;
    std::function<std::string(const LLUUID&)> mWhoUnavailableLabel;std::function<void(const LLUUID&)> mWhoUnavailableAction;
    bool mWhoPinned=false,mWhoError=false,mActorTargetPage=false;LLSD mSavedWhoPin;
    std::string mWhoAdapter="cine_rig_slot",mWhoLabel="Editing rig",mPage="light_group/cine_light_rig_tab",mMasterSetting="CineLightRigEnabled";
    LLComboBox combo;LLUICtrl chip,master;LLUICtrl* mWhoChip=&chip;LLUICtrl* mMaster=&master;LLComboBox* mWhoCombo=&combo;
    std::string label="original label",hint="original hint";bool mWhoHint=true;
    KitPage(){for(int i=0;i<5;++i)combo.items.push_back({LLSD(i)});combo.value=0;combo.commit=[this](){ALCineLightRigManager::instance().setSelectedSlot(static_cast<Slot>(combo.value.asInteger()));};}
    LLComboBox* whoCombo(){return mWhoCombo;}ALKitRow* findRow(const std::string&){return nullptr;}bool activateWhoUnavailable();bool whoPinned()const{return mWhoPinned;}LLUUID actorTarget()const{return mActorTarget.notNull()?mActorTarget:LLUUID{"new-body"};}
    bool setWhoPin(const LLSD&);bool selectWhoValue(const LLSD&);void applyCastSelection(const uuid_vec_t&);
    void refreshWhoUnavailable();void cancelWhoUnavailable();void refreshWhoHint();bool canFollowCast()const{return true;}
    LLUICtrl* whoChip(){return &chip;}void setMasterContext(const std::string&,const std::string&){}
    void setWhoChipLabel(const std::string& s){label=s;}void setWhoHint(const std::string& s){hint=s;}
    void refreshActorTargetAvailability(){}void saveState(){}
};
struct LLContextMenu;
struct ALPanelCineLightRig:LLView {
    int mFixtureRoleIndex=0,retries=0;KitPage page;KitPage* mRigKitPage=&page;
    template<class T>LLHandle<T> getDerivedHandle(){return {static_cast<T*>(this)};}
    void ensureRigActorInCast(const LLUUID& id);
    bool mCastListInitialized=true,mLitTargetForceRefresh=false;U32 mDisplayedGroupSlots=0;U64 mSubjectChoiceRevision=0;
    std::map<std::string,std::map<LLUUID,bool>> mRigDisplayedLabels;bool mRigNearbyAllowed=true;LLHandle<LLContextMenu> mRigGroupMenu;
    LLComboBox anchor,lit,subjectCombo;LLComboBox* mAnchorCombo=&anchor;LLComboBox* mLitTarget=&lit;
    LLComboBox* mSubjectTargetCombos[3]={&subjectCombo,nullptr,nullptr};
    std::string cachedName;
    void updateAnchorList(bool refresh_who=true);bool selectActorRig(const LLUUID&);void syncGroupControls();void syncObjectTargetControls(bool){}void syncAnchorSelection(){}
    U64 mAnchorCastGroupRevision=0;std::array<std::string,5> mAnchorLitGroups;std::array<uuid_vec_t,5> mAnchorLitMembers;
    U32 mAnchorVisibilityState=~0u;std::array<U32,5> mAnchorGroupStates{};std::array<LLUUID,5> mAnchorObjectTargets{},mAnchorAssignments{};
    LLUICtrl allOff,remove,status,power,roster[5];LLUICtrl *mAllRigsOff=&allOff,*mRemoveBorrowedRig=&remove,*mRigGroupStatus=&status,*mRigEnabled=&power;
    LLUICtrl* mRigRoster[5]={&roster[0],&roster[1],&roster[2],&roster[3],&roster[4]};
    std::vector<LLUUID> mCastIds;std::vector<std::string> mCastNames;std::vector<S32> mCastValues;bool mAnchorSelectionInitialized=false;
    void finishRigAction(S32,bool=false);void refreshRigNotices(bool=false);void refreshRigPrivacy();void showRigNotice(const std::string&);std::vector<LLUUID> mRigNotices;
    static std::string rigLabelBase(S32);std::string rigTargetDescription(S32,const char*="anchors");std::string rigLabel(S32,const char*="anchors");
    std::string rigActorName(const LLUUID&,const char*);void clearRigLabels(const char* area);std::shared_ptr<const LLUUID> mPendingBorrowPick;
    bool addAvatarToRigGroup(const LLUUID&);
    void shareRig(const LLUUID&,S32);void showRigGroupMenu(LLUICtrl*,bool);void onRigGroupAction(S32){}
    void showBorrowMenu(const LLUUID&,bool=false);
    void clearPendingBorrowPick(){mPendingBorrowPick.reset();}
    void onAnchorSelected();
    S32 mDisplayedSlot=-1,mDisplayedObjectTargetSlot=-1,mShadowHintState=-1;
    bool mDisplayedGroupEnabled=false,mSeedInitialized=false,mRadiusCueInitialized=false;U32 mDisplayedResolvedSlots=0;
    std::function<void()> replace(LLUUID,bool,Slot,LLUUID);std::function<void()> share(LLUUID,bool,Slot,LLUUID);
    std::function<std::string(const std::string&)> context();std::function<bool()> available();
};
struct LLContextMenu:LLUICtrl {
    struct Params{std::string name,label;bool visible=false;};bool hidden=false;static inline int deleted=0;
    std::vector<LLUICtrl*> children;void addChild(LLUICtrl* p){children.push_back(p);}void appendContextSubMenu(LLContextMenu* p){addChild(p);}
    void hide(){hidden=true;}bool getVisible()const{return !hidden;}void buildDrawLabels(){}LLHandle<LLContextMenu> getHandle(){return {this};}
    ~LLContextMenu(){++deleted;}
};
struct LLMenuItemCallGL:LLUICtrl {struct Params{std::string name,label,tool_tip;bool enabled=true;struct Click{std::function<void(LLUICtrl*,const LLSD&)> function;}on_click;};Params params;};
struct LLUICtrlFactory{template<class T>static T* create(const typename T::Params& p){auto* v=new T;if constexpr(std::is_same_v<T,LLMenuItemCallGL>)v->params=p;return v;}};
LLContextMenu holder;LLContextMenu* gMenuHolder=&holder;
namespace ALKitMenu {void showContextMenu(LLUICtrl*,LLContextMenu*,S32,S32){}}
struct FocusMgr {bool childHasKeyboardFocus(LLComboBox*)const{return false;}}gFocusMgr;
void setTextIfChanged(LLUICtrl* c,const std::string& s){if(c)c->value=s;}
void setToolTipIfChanged(LLUICtrl*,const std::string&){}void setLabelIfChanged(LLUICtrl*,const std::string&){}void setEnabledIfChanged(LLUICtrl*,bool){}
enum {ADD_BOTTOM};
#define LL_PROFILE_ZONE_NAMED_CATEGORY_UI(...)
#define LL_WARNS(...) std::cerr
#define LL_ENDL std::endl
std::vector<std::function<void()>> idle;
void doOnIdleOneTime(std::function<void()> f){idle.push_back(f);}
''']
    lines.append(function(panel, 'void retireRigMenu('))
    lines.append(function(panel, 'bool rigNameVisible('))
    lines.append(function(panel, 'S32 rigSlotHistoryKind('))
    for sig in ('bool ALPanelCineLightRig::selectActorRig(', 'void ALPanelCineLightRig::clearRigLabels(', 'bool ALPanelCineLightRig::addAvatarToRigGroup(', 'std::string ALPanelCineLightRig::rigActorName(', 'std::string ALPanelCineLightRig::rigLabel(', 'std::string ALPanelCineLightRig::rigLabelBase(', 'std::string ALPanelCineLightRig::rigTargetDescription(', 'void ALPanelCineLightRig::ensureRigActorInCast(', 'void ALPanelCineLightRig::showRigNotice(', 'void ALPanelCineLightRig::refreshRigNotices(', 'void ALPanelCineLightRig::updateAnchorList(', 'void ALPanelCineLightRig::showRigGroupMenu(', 'void ALPanelCineLightRig::showBorrowMenu(', 'void ALPanelCineLightRig::shareRig('):
        lines.append(function(panel,sig))
    lines.append('void ALPanelCineLightRig::syncGroupControls(){cachedName=rigActorName(LLUUID{"hidden"},"lights_status");}')
    from rig_borrow_round3 import combo_fixture
    # Definitions are needed before updateAnchorList uses the helper.
    ix=next(i for i,s in enumerate(lines) if s.startswith('void ALPanelCineLightRig::updateAnchorList'))
    lines.insert(ix, combo_fixture(root))
    lines.append(function(shell, 'bool ALKitShell::hidePopover()'))
    history = (root/'indra/newview/alkithistory.cpp').read_text(encoding='utf8')
    lines.append(function(history, 'void ALKitHistory::barrier('))
    lines.append(function(panel, 'void ALPanelCineLightRig::refreshRigPrivacy()'))
    lines.append(function(panel, 'void ALPanelCineLightRig::finishRigAction('))
    lines.append(function(panel, 'void ALPanelCineLightRig::onAnchorSelected()'))
    for sig in ('LLUUID ALCineLightRigManager::groupSlotActor(',
                'ALCineLightRigManager::Slot ALCineLightRigManager::actorSlot(',
                'LLUUID ALCineLightRigManager::resolveRigActorIdentity(',
                'LLUUID ALCineLightRigManager::resolveBorrowActor(',
                'void ALCineLightRigManager::rewriteActorReferences(',
                'void ALCineLightRigManager::giveActorRig(',
                'void ALCineLightRigManager::removeBorrowedRig(',
                'std::shared_ptr<const LLUUID> ALCineLightRigManager::retainBorrowActor(', 'void ALCineLightRigManager::refreshBorrowAnchors(', 'void ALCineLightRigManager::pruneRemovedAnchors('):
        lines.append(function(manager,sig).replace('rig.mLastResolvedMembers', 'rig.members')
                     .replace('rig.mLastResolvedStoredMembers', 'rig.stored'))
    menu = function(panel,'void ALPanelCineLightRig::showBorrowMenu(')
    for name,item in (('replace','replace_rig'),('share','share_rig')):
        part=menu[menu.index('add("'+item+'"'):]
        cb=block(part,'[handle, captured, was_cast, slot, expected, expected_was_cast, picked, expected_pick]')
        lines.append('std::function<void()> ALPanelCineLightRig::'+name+'(LLUUID captured,bool was_cast,Slot slot,LLUUID expected){auto handle=getDerivedHandle<ALPanelCineLightRig>();bool expected_was_cast=LLDirectorCast::instance().contains(expected);auto picked=ALCineLightRigManager::instance().retainBorrowActor(captured);auto expected_pick=ALCineLightRigManager::instance().retainBorrowActor(expected);return '+cb+';}')
    context=block(panel[panel.index('shell->history().registerContext('):],'[handle](const std::string& widget)')
    lines.append('std::function<std::string(const std::string&)> ALPanelCineLightRig::context(){auto handle=getDerivedHandle<ALPanelCineLightRig>();return '+context+';}')
    available=block(panel[panel.index('mRigKitPage->setWhoAvailableAction('):],'[handle]()')
    lines.append('std::function<bool()> ALPanelCineLightRig::available(){auto handle=getDerivedHandle<ALPanelCineLightRig>();return '+available+';}')
    for sig in ('bool ALKitPage::activateWhoUnavailable()', 'bool ALKitPage::setWhoPin(', 'bool ALKitPage::selectWhoValue(', 'void ALKitPage::applyCastSelection(', 'void ALKitPage::refreshWhoUnavailable()', 'void ALKitPage::cancelWhoUnavailable()', 'void ALKitPage::refreshWhoHint()'):
        lines.append(function(page,sig).replace('ALKitPage::','KitPage::'))
    lines.append(r'''
int main(){
    auto& m=ALCineLightRigManager::instance();auto& c=LLDirectorCast::instance();ALPanelCineLightRig p;p.page.mRigPanel=&p;
    const auto& owned=ALPresetLook::owned(ALPresetLook::LIGHT_RIG);
    assert(std::find(owned.begin(),owned.end(),"CineLightRigEnabled")==owned.end());
    assert(!p.context()("CineLightRigObjectTarget").empty() && !p.context()("cine_enabled").empty());
    c.members[{"hidden"}].mLastName="PLANTED SECRET";RlvActions::visible=false;
    assert(p.rigActorName({"hidden"},"test")=="Actor");RlvActions::visible=true;assert(p.rigActorName({"hidden"},"test")=="PLANTED SECRET");
    // Both real commit callbacks must follow a re-key and must do nothing after retirement.
    for(bool share:{false,true})for(int removal=0;removal<3;++removal){
        m=ALCineLightRigManager();c.members.clear();c.live.clear();
        c.members[{"Bob"}];c.members[{"clone-root"}];LLDirectorCast::canonical[LLUUID{"control-avatar"}]={"clone-root"};
        const auto captured=LLDirectorCast::canonicalActorId({"control-avatar"});
        auto action=share?p.share(captured,true,Slot::A,{"Bob"}):p.replace(captured,true,Slot::A,{"Bob"});
        c.members.erase({"clone-root"});c.members[{"new-body"}];m.rewriteActorReferences(captured,{"new-body"});
        if(removal==1){c.members.erase({"new-body"});m.rewriteActorReferences({"new-body"},LLUUID::null);}
        if(removal==2){c.members.erase({"new-body"});c.live.insert({"new-body"});} // removed Cast actor still nearby
        m.pruneRemovedAnchors(); // menu remains open over the manager cleanup cadence
        int writes=m.mutations,retries=p.retries;auto notices=LLNotificationsUtil::notices.size();action();
        if(removal){assert(m.mutations==writes && LLNotificationsUtil::notices.size()==notices+1 && LLNotificationsUtil::find(p.mRigNotices.back())->data["MESSAGE"].asString()=="Actor unavailable" && p.retries==retries && m.at(Slot::A).group.empty());}
        else if(share)assert(c.contains({"new-body"}) && !m.at(Slot::A).group.empty());
        else assert(m.at(Slot::A).anchor==LLUUID{"new-body"} && m.at(Slot::A).enabled);
        assert(!c.members.count({"clone-root"}) && !c.members.count({"control-avatar"}));
    }
    // Re-key enrollment must survive removal before the next manager tick.
    m=ALCineLightRigManager();c.members.clear();c.members[{"old-body"}];m.giveActorRig(Slot::A,{"old-body"});
    auto parkedPick=m.retainBorrowActor({"parked"});
    c.members.erase({"old-body"});c.members[{"parked"}];m.rewriteActorReferences({"old-body"},{"parked"});
    c.members.erase({"parked"});c.members[{"old-body"}];m.rewriteActorReferences({"parked"},{"old-body"});
    c.members.erase({"old-body"});c.members[{"fresh-body"}];m.rewriteActorReferences({"old-body"},{"fresh-body"});
    assert(m.resolveBorrowActor({"parked"},true)==LLUUID{"fresh-body"});
    c.members.erase({"fresh-body"});m.pruneRemovedAnchors();
    assert(!m.at(Slot::A).enabled && m.at(Slot::A).anchor.isNull());
    // Bob's Subject rig was replaced by Alice, then Off, then Alice retired.
    for(bool despawn:{false,true})for(const char* widget:{"CineLightRigEnabled","cine_enabled"}){
        m=ALCineLightRigManager();c.members.clear();c.members[{"Alice"}];m.giveActorRig(Slot::A,{"Alice"});
        ALKitHistory h;auto ctx=p.context();h.mContexts["light_group/cine_light_rig_tab"]=ctx;
        assert(p.page.setWhoPin(LLSD(1)));
        LLUICtrl power;power.name=widget;power.value=false;
        power.commit=[&](){m.at(Slot::A).enabled=power.value.asBoolean();};
        m.at(Slot::A).enabled=false;
        h.beginGesture(ALKitHistory::MOUSE,"light_group/cine_light_rig_tab","Off");
        h.recordWidget(&power,true,false,"Off");h.finishGesture();assert(h.mUndo.size()==1);
        const int writes=m.cleanupWrites;
        const auto epoch=m.historyEpoch(Slot::A);
        if(despawn)m.rewriteActorReferences({"Alice"},LLUUID::null);
        else {c.members.erase({"Alice"});m.pruneRemovedAnchors();}
        assert(m.historyEpoch(Slot::A)>epoch);
        assert(m.cleanupWrites>writes && !ALKitHistory::suspended());
        assert(m.slotActor(Slot::A)==LLUUID{"Bob"});h.undo("light_group/cine_light_rig_tab");
        assert(!m.at(Slot::A).enabled && h.mRedo.empty());
    }
    // Non-Cast self and away Subject recipients are valid for both actions.
    for(bool share:{false,true})for(LLUUID recipient:{gAgentID,LLUUID{"away-subject"}}){
        m=ALCineLightRigManager();c.members.clear();c.live.clear();m.subject=recipient;
        c.members[{"new-body"}];int retries=p.retries;
        auto action=share?p.share({"new-body"},true,Slot::A,recipient):p.replace({"new-body"},true,Slot::A,recipient);
        action();assert(p.retries==retries);
        if(share)assert(c.contains({"new-body"}) && !m.at(Slot::A).group.empty());else assert(m.at(Slot::A).anchor==LLUUID{"new-body"});
    }
    // Prune dead aliases/tombstones while preserving a rig's live re-key chain.
    m=ALCineLightRigManager();c.members.clear();c.members[{"live"}];m.giveActorRig(Slot::A,{"live"});
    auto oldPick=m.retainBorrowActor({"old"});m.rewriteActorReferences({"old"},{"live"});m.mBorrowRekeys[{"dead"}]={};m.mBorrowRekeys[{"unused"}]={"gone"};
    m.pruneRemovedAnchors();assert(m.mBorrowRekeys.size()==1 && m.resolveBorrowActor({"old"},true)==LLUUID{"live"});
    oldPick.reset();m.removeBorrowedRig(Slot::A);c.members.clear();m.pruneRemovedAnchors();assert(m.mBorrowRekeys.empty());
    m.mBorrowRekeys[{"blob-alias"}]={"blob-only"};m.mBlobs[4].mAnchor={"blob-alias"};
    m.mBorrowRekeys[{"anchor-alias"}]={"anchor-only"};m.at(Slot::C).anchor={"anchor-alias"};
    m.mBorrowRekeys[{"enrolled-alias"}]={"enrolled-only"};m.mEnrolledAnchors[1]={"enrolled-alias"};
    m.pruneRemovedAnchors();assert(m.mBorrowRekeys.size()==3);
    m.mBlobs[4].mAnchor.setNull();m.at(Slot::C).anchor.setNull();m.mEnrolledAnchors[1].setNull();
    m.pruneRemovedAnchors();assert(m.mBorrowRekeys.empty());
    // A successful give/borrow traverses real finishRigAction -> setWhoPin ->
    // selectWhoValue, clears the Give intercept, and leaves the rig selected.
    for(bool follow:{false,true}){
        m=ALCineLightRigManager();c.members[{"new-body"}];m.giveActorRig(Slot::A,{"new-body"});
        p.page.mWhoUnavailableTarget={"new-body"};p.finishRigAction(1,follow);
        assert(p.page.mWhoUnavailableTarget.isNull() && m.selectedSlot()==Slot::A);
    }
    // Gaze-style pages keep their custom chip and hint during rig refreshes.
    KitPage gaze;gaze.mWhoAdapter="gaze_slot";gaze.mPage="actor_group/gaze_tab";
    gaze.refreshWhoUnavailable();assert(gaze.label=="original label" && gaze.hint=="original hint");
    gaze.mWhoUnavailableTarget={"pending"};gaze.mWhoUnavailableLabel=[](const LLUUID&){return "new pending label";};
    gaze.refreshWhoUnavailable();assert(gaze.label=="original label" && gaze.hint=="original hint");
    gaze.refreshWhoHint();assert(gaze.hint=="Follows the Cast selection; pick one to lock.");
    // Name-command notification can precede or follow the completed cache write.
    for(bool early:{false,true}){
        c.members[{"hidden"}].mLastName="PLANTED SECRET";RlvActions::visible=true;RlvActions::exceptions.clear();
        p.refreshRigPrivacy();p.anchor.open=p.lit.open=p.subjectCombo.open=true;
        auto* shell=ALKitShell::find(&p);Popover pop;pop.visible=true;pop.snapshot="Rig 2: PLANTED SECRET";shell->mOpenPopover=&pop;
        if(early)p.refreshRigPrivacy(); // old RLV notification ordering
        RlvActions::visible=false;p.refreshRigPrivacy();
        assert(!pop.visible && !p.anchor.open && !p.lit.open && !p.subjectCombo.open && p.cachedName=="Actor");
        RlvActions::exceptions.insert({"hidden"});p.refreshRigPrivacy();assert(p.cachedName=="PLANTED SECRET");
        pop.visible=true;shell->mOpenPopover=&pop;RlvActions::exceptions.clear();p.refreshRigPrivacy();
        assert(!pop.visible && p.cachedName=="Actor");auto recorded=p.mRigDisplayedLabels;p.refreshRigPrivacy();assert(p.mRigDisplayedLabels==recorded);
        shell->mOpenPopover=nullptr;
    }
    RlvActions::visible=true;
    // The existing shell chip route reaches the real available-action lambda.
    m=ALCineLightRigManager();m.at(Slot::A).anchor={"Alice"};p.page.mWhoAvailableAction=p.available();
    assert(p.page.mWhoUnavailableTarget.isNull() && p.page.activateWhoUnavailable() && p.mRigGroupMenu.get());
    // Retire from within a click: the menu remains alive until the next idle tick.
    for(auto& f:idle)f();idle.clear();LLContextMenu::deleted=0;
    auto* menu=new LLContextMenu;LLHandle<LLContextMenu> handle{menu};retireRigMenu(handle);
    assert(menu->hidden && !handle.get() && LLContextMenu::deleted==0);retireRigMenu(handle);for(auto& f:idle)f();assert(LLContextMenu::deleted==1);
    std::cout<<"PASS: production ownership/power epoch and pinned undo, registry SELF, non-Cast Replace/Share, privacy command ordering/exception removal/open popover, Gaze isolation, real pin completion, alias pruning, callback lifetime and deferred menu deletion\n";
}
''')
    from rig_borrow_round3 import extend_fixture
    source=extend_fixture(root,'\n'.join(lines)).replace('struct LLSD {', 'struct LLSD {bool isDefined()const{return type!=NIL;}').replace('std::string asString()const{return text;}', 'std::string asString()const{return type==INTEGER?std::to_string(int(number)):text;}')
    from rig_borrow_round4 import extend_fixture as round4
    source=round4(root,source)
    from rig_borrow_round5 import extend_fixture as round5
    source=round5(root,source)
    from rig_borrow_round6 import extend_fixture as round6
    source=round6(root,source)
    from rig_borrow_round7 import extend_fixture as round7
    source=round7(root,source)
    from rig_borrow_round8 import extend_fixture as round8
    source=round8(root,source)
    from rig_borrow_round9 import extend_fixture as round9
    source=round9(root,source)
    from rig_borrow_round10 import extend_fixture as round10
    source=round10(root,source)
    from rig_borrow_round11 import extend_fixture as round11
    source=round11(root,source)
    mutations={
        'follow_history': ('    ALKitHistory::ScopedSuspend suspend;', ''),
        'resolved_actor_selection': ('(mRigPanel && mRigPanel->selectActorRig(mWhoUnavailableTarget))', 'true'),
        'banner_unbound_write': ('if (mWhoAdapter != "cine_rig_slot") mMaster->writeBoundValue(true);', 'mMaster->writeBoundValue(true);'),
        'banner_available_menu': ('if (mWhoUnavailableTarget.notNull() && activateWhoUnavailable()) return;', 'if (activateWhoUnavailable()) return;'),
        'pinned_pending': ('void KitPage::applyCastSelection(const uuid_vec_t& ids)\n{', 'void KitPage::applyCastSelection(const uuid_vec_t& ids)\n{\n    if (mWhoPinned || !canFollowCast()) return;'),
        'label_repeat': ('const std::string label = mWhoUnavailableLabel(mWhoUnavailableTarget);', 'const std::string label = mWhoUnavailableLabel(mWhoUnavailableTarget);mWhoUnavailableLabel(mWhoUnavailableTarget);'),
        'empty_ledger': ('mRigDisplayedLabels.erase(area);', 'mRigDisplayedLabels[area].clear();'),
        'retired_remove_notice': ('panel->showRigNotice("Actor unavailable");', ''),
        'actor_bar_subscription': ('[this]() { applyActiveRigActor(); }', '[this]() {}'),
        'pending_power_empty': ('    mWhoUnavailableTarget.setNull();\n    restoreRigMaster();', '    mWhoUnavailableTarget.setNull();'),
        'pending_power_resolved': ('            restoreRigMaster();\n            setWhoChipLabel', '            setWhoChipLabel'),
        'custom_context_opt_in': ('mCustomContextPages.count(mOpen->custom_binding_page)', 'true'),
        'custom_source_walk': ('if (!mOpen->custom_source_resolved || mOpen->custom_source.get() != source)', 'if (true)'),
        'give_history_suspend': ('    ALKitHistory::ScopedSuspend suspend;', ''),
        'anchored_target_description': ('rig.isGroupEnabled() || !rig.getLitGroup().empty() || rig.getObjectTarget().notNull())\n            label +=', 'false)\n            label +='),
        'edit_your_rig': ('i == 0 ? "Edit your rig" : "Edit " + rigLabel(i, "menu")', '"Edit " + rigLabel(i, "menu")'),
        'custom_source_change': (' || mOpen->custom_source.get() != source', ''),
        'master_null_control': ('if (LLControlVariable* control = gSavedSettings.getControl(mMasterSetting).get())', 'if (LLControlVariable* control = gSavedSettings.getControl(mMasterSetting).get(); true)'),
        'privacy_cache': ('if (!visible) return "Actor";', ''),
        'retired_replace': ('const LLUUID id = rigs.resolveBorrowActor(captured, was_cast);', 'const LLUUID id = captured;'),
        'retired_share': ('const LLUUID id = rigs.resolveBorrowActor(captured, was_cast);', 'const LLUUID id = captured;'),
        'history_retirement': ('++mHistoryEpoch[slotIndex(slot)];', ''),
        'history_despawn': ('++mHistoryEpoch[i];', ''),
        'cleanup_capture': ('ALKitHistory::ScopedSuspend suspend;', ''),
        'power_epoch': ('"CineLightRigEnabled",', '"PLANTED OMITTED POWER",'),
        'popover_close': ('shell->hidePopover();', '(void)shell;'),
        'privacy_signature': ('if (!changed) return;', 'return;'),
        'self_candidate': ('(expected_was_cast ? rigs.resolveBorrowActor(expected, true) : expected)', 'rigs.resolveBorrowActor(expected, true)'),
        'pending_pin': ('            mWhoUnavailableTarget.setNull();', ''),
        'gaze_pending': ('if (mWhoAdapter == "cine_rig_slot") setWhoChipLabel(label);', 'setWhoChipLabel(label);'),
        'gaze_refresh': ('mWhoAdapter == "cine_rig_slot" && mWhoUnavailableTarget.isNull()', 'mWhoUnavailableTarget.isNull()'),
        'alias_pruning': ('it = mBorrowRekeys.erase(it);', '++it;'),
        'synchronous_delete': ('doOnIdleOneTime([handle]() { if (auto* menu = handle.get()) delete menu; });', 'delete handle.get();'),
        'subject_label_bypass': ('label += actor_label(subject_choice->mId);', 'label += subject_choice->mLabel;'),
        'nearby_not_recorded': ('if (id.notNull()) mRigDisplayedLabels[area][id] = visible;', 'if (id.notNull() && LLDirectorCast::instance().contains(id)) mRigDisplayedLabels[area][id] = visible;'),
        'shownearby_ignored': ('bool changed = mRigNearbyAllowed != ALGazeNearby::nearbyAllowed();', 'bool changed = false;'),
        'replace_epoch': ('if (slotActor(slot) != next) ++mHistoryEpoch[slotIndex(slot)];', ''),
        'global_epoch': ('const S32 kind = rigSlotHistoryKind(widget);', 'const S32 kind = widget.find("CineLightRig")==0?1:rigSlotHistoryKind(widget);'),
        'share_drops_recipient': ('for (const auto& recipient : recipients) ensureRigActorInCast(recipient);', ''),
        'cancel_intercept': ('mRigKitPage->cancelWhoUnavailable();', ''),
        'action_clears_history': ('shell->history().finishGesture();', 'shell->history().barrier("planted clear");'),
        'share_loses_fallback': ('cast.addToGroup(recipients, group);', 'cast.addToGroup(uuid_vec_t{id}, group);'),
        'open_list_freezes_status': ('clearRigLabels("roster");', 'if (mAnchorCombo->isListVisible()) return;clearRigLabels("roster");'),
        'unavailable_intercept': ('panel->mRigKitPage->cancelWhoUnavailable();', ''),
        'remove_keeps_targets': ('at(slot).setObjectTarget(LLUUID::null);', ''),
        'per_frame_prune': ('if (!mBorrowAnchorsDirty && mBorrowCastRevision == revision) return;', ''),
        'historical_aliases': ('!referenced(it->first) && !mBorrowPicks.count(it->first)', '!referenced(it->first) && !referenced(it->second)'),
        'share_runtime_identity': ('const LLUUID actor = manager.resolveRigActorIdentity(member);', 'const LLUUID actor = member;'),
        'share_ignores_stored': ('const auto& effective = rig.lastResolvedStoredMembers().empty()\n        ? rig.lastResolvedMembers() : rig.lastResolvedStoredMembers();', 'const auto& effective = rig.lastResolvedMembers();'),
        'custom_context': ('if (!entry.context.empty() && entry.context != currentContext(entry.binding_page, entry.key))', 'if (false)'),
        'history_actor': ('" actor " + LLDirectorCast::canonicalActorId(ALCineLightRigManager::instance().slotActor(\n                    static_cast<ALCineLightRigManager::Slot>(slot))).asString()', '" actor omitted"'),
        'picker_poll_closes': ('mRigDisplayedLabels.erase(area);', 'if(auto* shell=ALKitShell::find(this))shell->hidePopover();mRigDisplayedLabels.erase(area);'),
        'pending_lease': ('if (mRigPanel) mRigPanel->clearPendingBorrowPick();', ''),
        'gobo_context': ('"gobo_preview",', '"omitted_gobo_preview",'),
        'remove_selection': ('ALCineLightRigManager::instance().setSelectedSlot(static_cast<ALCineLightRigManager::Slot>(slot));', ''),
    }
    from rig_borrow_round11 import MUTATIONS as R11, mutate as mutate11
    if mutation in R11: return mutate11(source,mutation)
    from rig_borrow_round10 import MUTATIONS as R10, mutate as mutate10
    if mutation in R10: return mutate10(source,mutation)
    from rig_borrow_round9 import MUTATIONS as R9, mutate as mutate9
    if mutation in R9: return mutate9(source,mutation)
    from rig_borrow_round8 import MUTATIONS, mutate
    if mutation in MUTATIONS:
        return mutate(source, mutation)
    if mutation:
        old,new=mutations[mutation]
        if mutation in ('retired_share','retired_replace'):
            pos=source.index('ALPanelCineLightRig::'+('share(' if mutation=='retired_share' else 'replace('))
            source=source[:pos]+source[pos:].replace(old,new,1)
            if mutation=='retired_share':
                # Real shareRig now provides its own second lifetime check.
                source=source.replace('const LLUUID id = manager.resolveBorrowActor(captured);','const LLUUID id = captured;')
        elif mutation in ('cleanup_capture','history_retirement'):
            pos=source.index('void ALCineLightRigManager::removeBorrowedRig(')
            source=source[:pos]+source[pos:].replace(old,new,1)
        elif mutation in ('give_history_suspend','follow_history','pinned_pending','action_clears_history','retired_remove_notice'):
            signature={'give_history_suspend':'void ALCineLightRigManager::giveActorRig(',
                'follow_history':'void KitPage::applyCastSelection(',
                'pinned_pending':'void KitPage::applyCastSelection(',
                'action_clears_history':'void ALPanelCineLightRig::finishRigAction(',
                'retired_remove_notice':'void ALPanelCineLightRig::showRigGroupMenu('}[mutation]
            pos=source.index(signature)
            assert old in source[pos:]
            source=source[:pos]+source[pos:].replace(old,new,1)
            if mutation == 'follow_history':
                # Rig follow now delegates to a resolver with its own guard.
                # Plant the original unsuspended chain, not just removal of
                # one redundant guard: the unchanged oracle must still reject it.
                resolver=source.index('bool ALPanelCineLightRig::selectActorRig(')
                source=source[:resolver]+source[resolver:].replace(old,new,1)
        elif mutation=='self_candidate':
            source=source.replace(old,new)
        elif mutation=='remove_selection':
            # Restore both parts of the original Remove failure: no preselection
            # and a clearing barrier on its programmatic anchor commit.
            source=source.replace(old,new,1).replace('if(shell && !ALKitHistory::suspended())','if(shell)')
        elif mutation=='pending_lease':
            # Other lifecycle hooks use the same spelling; remove cancellation's
            # hook specifically so this control tests the abandoned-Give path.
            pos=source.index('void KitPage::cancelWhoUnavailable()')
            assert old in source[pos:]
            source=source[:pos]+source[pos:].replace(old,new,1)
        else:
            assert old in source
            source=source.replace(old,new,1)
    return source


def run(root):
    from native_check_tools import check_directory, compile_run, viewer_native_flags
    out=check_directory('rig_borrow_round1')
    (out/'stdtypes.h').write_text('#pragma once\n#include <cstdint>\nusing F32=float;using F64=double;using S32=int;using U32=uint32_t;using U64=uint64_t;using S64=int64_t;using U8=uint8_t;\n')
    native_flags,_=viewer_native_flags()
    flags=[f'/I"{out}"',f'/I"{root/"indra/newview"}"']+native_flags+['/UNDEBUG']
    good=compile_run(out,fixture(root),'round1',flags=flags)
    assert good.returncode==0,good.stdout+good.stderr
    print(good.stdout.strip())
    controls = ('privacy_cache','retired_replace','retired_share','history_retirement',
                 'history_despawn','cleanup_capture','synchronous_delete','power_epoch',
        'popover_close','privacy_signature','self_candidate','pending_pin','gaze_refresh','gaze_pending','alias_pruning',
        'subject_label_bypass','nearby_not_recorded','shownearby_ignored','replace_epoch','global_epoch','share_drops_recipient','cancel_intercept',
        'action_clears_history','share_loses_fallback','open_list_freezes_status','unavailable_intercept','remove_keeps_targets','per_frame_prune','historical_aliases','share_runtime_identity','share_ignores_stored',
        'custom_context','history_actor','picker_poll_closes','pending_lease','gobo_context','remove_selection',
        'actor_bar_subscription','pending_power_empty','pending_power_resolved','custom_context_opt_in','custom_source_walk',
        'give_history_suspend','anchored_target_description','edit_your_rig','master_null_control','custom_source_change',
        'follow_history','resolved_actor_selection','banner_unbound_write','banner_available_menu',
        'pinned_pending','label_repeat','empty_ledger','retired_remove_notice',
        *(__import__('rig_borrow_round8').MUTATIONS), *(__import__('rig_borrow_round9').MUTATIONS), *(__import__('rig_borrow_round10').MUTATIONS), *(__import__('rig_borrow_round11').MUTATIONS))
    def check_control(name):
        # Every executable gets an isolated startup/source/output directory.
        # All variants retain the same Release flags and assertion oracles.
        directory=out/name;directory.mkdir(parents=True,exist_ok=True)
        bad=compile_run(directory,fixture(root,name),name,flags=flags)
        assert bad.returncode!=0,'planted '+name+' escaped'
        if name.startswith(('r8_', 'r9_', 'r10_', 'r11_')): assert 'Assertion failed' in bad.stderr,(name,bad.returncode,bad.stderr)
        print('PASS: rejected planted '+name,flush=True)
    from concurrent.futures import ThreadPoolExecutor
    with ThreadPoolExecutor(max_workers=4) as pool:
        list(pool.map(check_control, controls))


if __name__=='__main__':
    from pathlib import Path
    run(Path(__file__).resolve().parents[2])
