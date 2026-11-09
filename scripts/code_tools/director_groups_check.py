"""Run pure group helpers and production cast group/scene methods without a viewer build.

Only world stores, UI primitives and unrelated scene codecs are adapters. The
history queue, page/keyboard dispatch, menu and confirmation handlers, mutations,
and complete sceneData/applySceneData methods come from this worktree.
Run with ordinary Python (not -I).
"""
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
from native_check_tools import ROOT, check_directory, compile_run, viewer_native_flags
from relight_area_policy_check import method

NV = ROOT / "indra/newview"


def fixture():
    header = (NV / "aldirectorgroups.h").read_text(encoding="utf-8")
    source = r'''
#include "linden_common.h"
#include "llsd.h"
#include "llsdutil_math.h"
#include "llsdutil.h"
#include "indra_constants.h"
#include "v3math.h"
#include "v3dmath.h"
#include <cassert>
#include <array>
#include <set>
#include <iostream>
#include <memory>
#include <boost/signals2.hpp>
''' + header + r'''
struct LLActorMover {
 struct GazeTarget { enum EMode { MOTION, CAMERA, OBJECT }; EMode mMode=MOTION; LLUUID mCastRef,mObjectRef; LLVector3d mFixedPoint; };
 static LLActorMover& instance(){static LLActorMover value;return value;}
 void releaseHotGazeSlot(S32) { ++releases; } S32 releases=0;
 LLSD pathSceneData(const LLUUID&) const {return LLSD();}
 void clearAllDirectorLookAtRuntime(){} void clearPath(const LLUUID&){}
 void clearDirectorLookAtRuntime(const LLUUID&){} void dropActor(const LLUUID&){}
 void setGazeTargetConfig(const LLUUID&,const GazeTarget&){}
 bool applyPathSceneData(const LLUUID&,const LLSD&){return true;}
 void clearHotGazes(){} void pruneEditActor(){} void onRosterMemberRemoved(const LLUUID&){}
};
namespace ALGazeHot {
 struct Slot { std::string mGroup; LLUUID mWho; LLActorMover::GazeTarget mTarget; F32 mDwell=2.f, mReturn=1.2f; };
 using Slots=std::array<Slot,4>;
 Slots readSlots(const LLSD&){return Slots();}
 template<class F> LLSD writeSlots(const Slots&,F){return LLSD::emptyArray();}
}
namespace ALSteppedAnim {
 std::map<std::string,S32> readGroupRates(const LLSD& sd,const std::vector<std::string>& names){std::map<std::string,S32> out;for(const auto& n:names)if(sd.has(n))out[n]=sd[n].asInteger();return out;}
 S32 groupRate(const std::map<std::string,S32>& rates,const std::string& name){auto it=rates.find(name);return it==rates.end()?0:it->second;}
 void setGroupRate(std::map<std::string,S32>& rates,const std::string& name,S32 offset){if(offset)rates[name]=offset;else rates.erase(name);}
 void renameGroupRate(std::map<std::string,S32>& rates,const std::string& old,const std::string& name){auto it=rates.find(old);if(it!=rates.end()){if(!rates.count(name))rates[name]=it->second;rates.erase(it);}}
 void pruneGroupRate(std::map<std::string,S32>& rates,const std::string& name,bool){rates.erase(name);}
 LLSD writeGroupRates(const std::map<std::string,S32>& rates,const std::vector<std::string>& names){LLSD result=LLSD::emptyMap();for(const auto& name:names)if(rates.count(name))result[name]=rates.at(name);return result;}
}
struct ALGhostStudio {
 enum {BACKING_ENTITY_CLONE=1, BACKING_OTHER=2};
 struct Instance {LLUUID mId,mEntityId; int mKind=BACKING_ENTITY_CLONE;};
 std::vector<Instance> instances;
 static ALGhostStudio& instance(){static ALGhostStudio value;return value;}
 const Instance* getInstance(const LLUUID& id) const {for(const auto& i:instances)if(i.mId==id)return &i;return nullptr;}
 const Instance* findInstanceByRuntime(const LLUUID& id) const {for(const auto& i:instances)if(i.mEntityId==id)return &i;return nullptr;}
};
struct ALAvatarLightResponse {
 static ALAvatarLightResponse& instance(){static ALAvatarLightResponse value;return value;}
 bool importMerge(const LLSD&){return false;} void flushIfDirty(){}
 LLSD exportFor(const uuid_vec_t& ids){LLSD result=LLSD::emptyArray();for(const auto& id:ids)result.append(id);return result;}
};
struct LocomotionRoleEntry {LLUUID mAnim;F32 mNominalSpeed=0;bool mAllowAO=false;};
struct LocomotionSet {std::map<int,LocomotionRoleEntry>mRoles;LocomotionRoleEntry& edit(int i){return mRoles[i];}const LocomotionRoleEntry* get(int)const{return nullptr;}};
enum { LOCO_WALK_FORWARD };
LLSD writeLocomotionSet(const LocomotionSet&){return LLSD();}
LLSD writeActorStyle(int){return LLSD::emptyMap();}
void writeGazeExpression(LLSD&,const LLActorMover::GazeTarget&){}
LLSD writeGazeAimTarget(const LLActorMover::GazeTarget&){return LLSD();}
LLSD writeGazeCues(const std::vector<int>&){return LLSD();}
LLSD writeGazeInfluenceKeys(const std::vector<int>&){return LLSD();}
using SceneActorIdRemap=std::map<LLUUID,LLUUID>;
LLUUID recoverLegacySceneActorId(const LLUUID& id,const std::string&){return id;}
LocomotionSet readLocomotionSet(const LLSD&){return {};}
int readActorStyle(const LLSD&){return 0;}
void readGazeExpression(const LLSD&,LLActorMover::GazeTarget&){}
bool readEyeGazeAimTarget(const LLSD&,LLActorMover::GazeTarget&,const SceneActorIdRemap&){return false;}
std::vector<int>readGazeCues(const LLSD&,const SceneActorIdRemap&){return {};}
std::vector<int>readGazeInfluenceKeys(const LLSD&){return {};}
LLUUID gAgentID("00000000-0000-0000-0000-000000000099");
struct LLDirectorCast {
 static LLDirectorCast* active;
 static LLDirectorCast& instance(){return *active;}
 struct CastMember {
  LLUUID mId,mLocoAnim;std::string mGroup,mLastName;bool mHasMark=false,mEyeGazeTargetEnabled=false;
  LLVector3 mMark;LocomotionSet mLocomotionSet;int mActorStyle=0;
  LLActorMover::GazeTarget mGazeTarget,mEyeGazeTarget;std::vector<int> mGazeCues,mGazeInfluenceKeys;
 };
 using GroupSnapshot=ALDirectorGroups::GroupSnapshot;
 struct GroupEvent {enum Type{MEMBERS,RENAMED,DISSOLVED}type=MEMBERS;std::string oldName,newName;};
 boost::signals2::signal<void(const GroupEvent&)> groupEvent;
 std::vector<CastMember> mCast;
 std::map<std::string,F32> mGroupDelays;std::map<std::string,S32> mGroupRates;
 U64 mGroupRevision=1,mCastRevision=1,mSceneRevision=1,mHotGazeRevision=1,mGazeCueRevision=1,mGazeConfigRevision=1;
 using ActorStyle=int; using GazeCueList=std::vector<int>;
 uuid_vec_t mIds;std::map<LLUUID,size_t>mMemberIndex;std::set<LLUUID>mLookAtCameraIds;
 bool isRunning()const{return false;}bool isCountingDown()const{return false;}void cut(){}
 void cancelPendingStarts(){} void cancelPendingStart(const LLUUID&){} void resolve(const LLUUID&){}
 bool isSelf(const LLUUID& id)const{return id==gAgentID || id.isNull();}
 void setActiveActorId(const LLUUID& id){mActiveActorId=id;}LLUUID getActiveActorId()const{return mActiveActorId;}
 void setGazeCues(const LLUUID&,const std::vector<int>&){}
 void rebuildMemberIndex(){}
 U64 getGroupRevision()const{return mGroupRevision;}
 void applySceneData(const LLSD&);void remove(const LLUUID&);
 ALGazeHot::Slots mHotGazeSlots;
 LLUUID mSubjectA,mSubjectB,mSubjectC,mSubjectD,mActiveActorId;
 LLActorMover::GazeTarget mSelfGazeTarget,mSelfEyeGazeTarget;
 bool mSelfLookAtCamera=false,mSelfEyeGazeTargetEnabled=false;
 std::vector<int>mSelfGazeCues,mSelfGazeInfluenceKeys;int mSelfActorStyle=0;
 static LLUUID canonicalActorId(const LLUUID& id){return id;}
 CastMember* getMember(const LLUUID& id){for(auto& m:mCast)if(m.mId==id)return &m;return nullptr;}
 const CastMember* getMember(const LLUUID& id)const{for(auto& m:mCast)if(m.mId==id)return &m;return nullptr;}
 bool contains(const LLUUID& id)const{return getMember(id);}
 void touchScene(){++mSceneRevision;}
 bool isLookAtCamera(const LLUUID&)const{return false;}
 LLUUID getSteppedActorId()const{return mActiveActorId;}
 void changedGroups(const GroupEvent&);
 std::string makeGroup(const uuid_vec_t&,const std::string& = {});
 void addToGroup(const uuid_vec_t&,const std::string&);
 void removeFromGroup(const uuid_vec_t&);
 void setGroup(const LLUUID&,const std::string&);
 std::string getGroup(const LLUUID&)const;
 std::vector<std::string>getGroupNames()const;
 uuid_vec_t membersInGroup(const std::string&)const;
 bool renameGroup(const std::string&,const std::string&);
 bool dissolveGroup(const std::string&);
 void setGroupDelay(const std::string&,F32);F32 getGroupDelay(const std::string&)const;
 void setGroupRate(const std::string&,S32);S32 getGroupRate(const std::string&)const;
 void pruneGroupData(const std::string&);
 GroupSnapshot captureGroups()const;void restoreGroups(const GroupSnapshot&);
 LLSD sceneData()const;
};
LLDirectorCast* LLDirectorCast::active=nullptr;
''' + history_ui_adapters() + r'''
'''
    signatures = [
        "LLUUID canonicalSceneActorId(", "bool isSceneClone(", "LLUUID loadedSceneMemberId(",
        "void LLDirectorCast::changedGroups(", "std::string LLDirectorCast::makeGroup(",
        "void LLDirectorCast::addToGroup(", "void LLDirectorCast::removeFromGroup(",
        "void LLDirectorCast::setGroup(", "std::string LLDirectorCast::getGroup(",
        "std::vector<std::string> LLDirectorCast::getGroupNames(",
        "uuid_vec_t LLDirectorCast::membersInGroup(",
        "LLDirectorCast::GroupSnapshot LLDirectorCast::captureGroups(",
        "void LLDirectorCast::restoreGroups(", "bool LLDirectorCast::renameGroup(",
        "bool LLDirectorCast::dissolveGroup(", "void LLDirectorCast::setGroupDelay(",
        "F32 LLDirectorCast::getGroupDelay(", "void LLDirectorCast::setGroupRate(",
        "S32 LLDirectorCast::getGroupRate(", "void LLDirectorCast::pruneGroupData(",
        "LLSD LLDirectorCast::sceneData() const", "void LLDirectorCast::applySceneData(", "void LLDirectorCast::remove(",
    ]
    source += "\n".join(method(NV / "lldirectorcast.cpp", signature) for signature in signatures)
    source += history_ui_methods()
    source += "\n".join(method(NV / "llfloaterdirector.cpp", signature) for signature in (
        "void LLFloaterDirector::recordGroupEdit(", "void LLFloaterDirector::editGroupMembers(", "void LLFloaterDirector::updateCastGroupMenu(",
        "void LLFloaterDirector::clearGroupMergeConfirmation(", "void LLFloaterDirector::onClickGroupRename(",
        "void LLFloaterDirector::onGroupsListSelect(", "bool LLFloaterDirector::handleKeyHere("))
    history = method(NV / "alkithistory.cpp", "void ALKitHistory::recordCustom(")
    assert 'dynamic_cast<ALKitShell*>(owner) ? "chrome"' in history
    floater = (NV / "llfloaterdirector.cpp").read_text(encoding="utf-8")
    assert floater.count('[this](LLLineEditor*, void*) { clearGroupMergeConfirmation(); }') == 2
    assert 'clearGroupMergeConfirmation();\n    if (mGroupRenameEditor) ::setTextIfChanged(mGroupRenameEditor, "");' in floater
    return source + r'''
int main() {
 using namespace ALDirectorGroups;
 assert(normalizeGroupName("  Guards  ")=="Guards");
 assert(normalizeGroupName(" (NoNe) ").empty() && normalizeGroupName(" \t ").empty());
 assert(normalizeGroupName(std::string(40,'x'))==std::string(32,'x'));
 assert(normalizeGroupName(std::string(40,'x')+" end").size()==32);
 assert(normalizeGroupName("\xc3\x89quipe")=="\xc3\x89quipe");
 std::string unicode;for(int i=0;i<40;++i)unicode+="\xc3\xa9";
 assert(normalizeGroupName(unicode).size()==64);
 assert(uniqueGroupName({"gRoUp 1","GROUP 2"})=="Group 3");
 assert(uniqueGroupName({"GUARDS"},"guards")!="guards");
 const LLUUID a("00000000-0000-0000-0000-000000000001"), b("00000000-0000-0000-0000-000000000002"), c("00000000-0000-0000-0000-000000000003");
 LLDirectorCast cast;LLDirectorCast::active=&cast;
 for(const auto& id:{a,b,c}){LLDirectorCast::CastMember member;member.mId=id;cast.mCast.push_back(member);}
 int events=0;LLDirectorCast::GroupEvent last;
 cast.groupEvent.connect([&](const auto& event){++events;last=event;});
 assert(cast.makeGroup({}).empty() && events==0);
 assert(cast.makeGroup({a,b},"  Guards  ")=="Guards" && events==1);
 const auto revision=cast.mGroupRevision;
 cast.addToGroup({a,b},"GUARDS");assert(events==1 && cast.mGroupRevision==revision);
 assert(cast.makeGroup({c},"guards")!="guards");
 assert(cast.getGroupNames().size()==2);
 cast.setGroupDelay("Guards",2.5f);cast.setGroupRate("Guards",2);
 const auto original=cast.captureGroups();
 const int before_move=events;
 cast.addToGroup({a,b},cast.getGroup(c));assert(events==before_move+1);
 assert(cast.getGroupDelay("Guards")==0 && cast.getGroupRate("Guards")==0);
 cast.restoreGroups(original);assert(cast.captureGroups()==original);
 const int before_noop=events;cast.restoreGroups(original);assert(events==before_noop);
 cast.mHotGazeSlots[0].mGroup="Guards";
 assert(cast.renameGroup("Guards","  Sentinels  "));
 assert(last.type==LLDirectorCast::GroupEvent::RENAMED && last.oldName=="Guards" && last.newName=="Sentinels");
 assert(cast.mHotGazeSlots[0].mGroup=="Sentinels" && cast.getGroupDelay("Sentinels")==2.5f);
 cast.restoreGroups(original);assert(cast.captureGroups()==original);
 cast.mCast.erase(cast.mCast.begin()+1);cast.restoreGroups(original);assert(!cast.contains(b));
 assert(cast.getGroup(a)=="Guards");
 auto rekey=original;rekey.rewriteActor(a,b);assert(!rekey.tags.count(a) && rekey.tags.at(b)=="Guards");
 rekey.rewriteActor(b,LLUUID::null);assert(!rekey.tags.count(b));
 const int before_remove=events;cast.removeFromGroup({a,c});assert(events==before_remove+1 && cast.getGroupNames().empty());
 assert(cast.mGroupDelays.empty() && cast.mGroupRates.empty());
 cast.restoreGroups(original);assert(cast.dissolveGroup("Guards"));
 assert(last.type==LLDirectorCast::GroupEvent::DISSOLVED && last.oldName=="Guards");
 assert(cast.contains(a) && cast.getGroup(a).empty());

 // Complete production sceneData: live + parked clone ids, real members,
 // mixed groups, clone-only delays/rates, subjects, stepped choice and response ids.
 const LLUUID stable("00000000-0000-0000-0000-000000000010"), runtime("00000000-0000-0000-0000-000000000011");
 ALGhostStudio::instance().instances.push_back({stable,runtime,ALGhostStudio::BACKING_ENTITY_CLONE});
 LLDirectorCast saved;
 for(const auto& id:{a,stable,runtime}){LLDirectorCast::CastMember m;m.mId=id;m.mGroup=id==a?"Real":"Clone only";saved.mCast.push_back(m);}
 saved.mGroupDelays={{"Real",1.f},{"Clone only",4.f}};saved.mGroupRates={{"Real",1},{"Clone only",2}};
 saved.mSubjectA=runtime;saved.mSubjectB=stable;saved.mSubjectC=a;saved.mActiveActorId=runtime;
 const auto scene=saved.sceneData();
 assert(scene["cast"].size()==1 && scene["cast"][0]["id"].asUUID()==a);
 assert(scene["subject_a"].asUUID().isNull() && scene["subject_b"].asUUID().isNull() && scene["subject_c"].asUUID()==a);
 assert(scene["stepped_actor"].asUUID().isNull());
 assert(scene["group_delays"].size()==1 && scene["group_rates"].size()==1 && scene["light_response"].size()==1);
 saved.mCast[1].mGroup="Real";assert(saved.sceneData()["group_delays"].size()==1);
 assert(loadedSceneMemberId(stable,{}).isNull() && loadedSceneMemberId(runtime,{}).isNull());
 assert(loadedSceneMemberId(a,{{a,stable}}).isNull() && loadedSceneMemberId(a,{})==a);
''' + history_ui_tests() + r'''
}
'''


def history_ui_adapters():
    # UI/world adapters supply storage and rendering boundaries only. The queue,
    # selection, validation, replay, command handlers and scene loader are real.
    return r'''
#include "alkithistory.h"
#include "alundosequence.h"
#include "lltimer.h"
#include <unordered_set>
struct LLView : LLHandleProvider<LLView> {
 virtual ~LLView()=default; LLView* getParent(){return nullptr;}
};
struct LLUICtrl : LLView {void setValue(const LLSD&){} void onCommit(){}};
struct ALKitRow : LLUICtrl {
 struct Meta{std::string setting;} value;const Meta& meta(){return value;}
 void replayBoundValue(const LLSD&){} bool resettable(){return false;}void reset(){}
};
struct LLControlVariable{void set(const LLSD&){}};
struct Settings {std::shared_ptr<LLControlVariable>getControl(const std::string&){return {};}}gSavedSettings;
struct ALPanelWeatherSettings{struct SettingsBatch{void finish(){}};};
struct LLNotificationsUtil{inline static std::string message;static void add(const std::string&,const LLSD& args) {message=args["MESSAGE"].asString();}};
struct LLLineEditor : LLUICtrl {
 std::string text,name;std::string getText()const{return text;}void setText(const std::string& s){text=s;}
 const std::string& getName(){return name;} void setFocus(bool){}
};
struct LLButton : LLUICtrl {std::string label;void setLabel(const std::string& s){label=s;}};
void setLabelIfChanged(LLButton* b,const std::string& s){if(b)b->setLabel(s);}
struct LLComboBox : LLUICtrl {
 LLSD selected;std::vector<std::string>items;
 LLSD getValue()const{return selected;}void removeall(){items.clear();selected=LLSD();}
 void add(const std::string& s,const LLSD&){items.push_back(s);}
 bool setSelectedByValue(const LLSD& s,bool){if(std::find(items.begin(),items.end(),s.asString())==items.end())return false;selected=s;return true;}
 void clear(){selected=LLSD();}void setSimple(const std::string& s){selected=s;}
};
void setVisibleIfChanged(LLView*,bool){}
struct LLSpinCtrl : LLUICtrl {void setValue(F32){}};
struct LLScrollListCtrl : LLUICtrl {
 bool isInVisibleChain()const{return true;}
 std::string selected;bool setSelectedByValue(const LLSD& s,bool){selected=s.asString();return true;}
};
struct ALPanelLensGaze {
 LLComboBox* mApplyTo=nullptr;LLComboBox* mApplyGroup=nullptr;std::vector<std::string>mLastApplyGroups;
 std::string selectedApplyGroup()const;void restoreApplyGroup(const std::string&);void refreshApplyGroupCombo();
};
struct ALKitShell;
struct ALKitPage : LLView {
 ALKitShell* shell=nullptr;std::string mPage="groups_tab";
 std::function<U64(bool)>mOwnedGroupSequence,mOwnedSequence;
 std::function<bool(bool,U64)>mOwnedReady;
 std::function<void(bool,U64)>mOwnedDiscardStep;std::function<void(bool)>mOwnedReplay;
 const std::string& pageKey()const{return mPage;}void undoOnPage(bool);bool canUndoOnPage(bool);
};
struct ALExposureSettingsHistory {
 static void finishPage(ALKitPage*){} static U64 pageStepSequence(ALKitPage*,bool){return 0;}
 static bool pageReplayReady(ALKitPage*,bool){return true;}static void discardPageStep(ALKitPage*,bool){}
 static void replayPage(ALKitPage*,bool){}
};
struct ALKitShell : LLView {
 ALKitHistory store;ALKitPage* current=nullptr;ALKitHistory& history(){return store;}
 static ALKitShell* find(LLView* v){return dynamic_cast<ALKitPage*>(v)->shell;}
 bool navigateTo(const std::string&){return true;}std::string currentPageKey(){return current->mPage;}
 ALKitPage* findBuiltPage(const std::string&){return current;}
 bool closePopover(){return false;}struct Search{std::string queryText(){return {};}};
 Search search(){return {};}void clearSearch(){}bool cycleGroup(S32){return false;}
 bool cyclePage(S32){return false;}bool cycleChip(S32){return false;}
};
struct LLContextMenu;
struct LLContextMenuBranch : LLView {LLContextMenu* branch=nullptr;LLContextMenu* getBranch(){return branch;}};
struct LLMenuItemCallGL : LLView {
 struct Params{std::string name,label;struct{std::function<void(LLUICtrl*,const LLSD&)>function;}on_click;};
 Params params;explicit LLMenuItemCallGL(const Params& p):params(p){}
};
struct LLMenuItemSeparatorGL : LLView {struct Params{};explicit LLMenuItemSeparatorGL(const Params&){} };
struct LLUICtrlFactory{template<class T>static T* create(const typename T::Params& p){return new T(p);}};
struct LLContextMenu : LLView {
 LLContextMenuBranch* branch=nullptr;std::vector<std::unique_ptr<LLView>>items;
 std::map<std::string,std::string>labels;void setItemLabel(const std::string& k,const std::string& s){labels[k]=s;}
 void setItemEnabled(const std::string&,bool){}void empty(){items.clear();}
 void addChild(LLView* v){items.emplace_back(v);}
 template<class T>T* getChild(const std::string&){
  // As in the UI, the parent owns a branch; its submenu lives elsewhere.
  if(auto* value=dynamic_cast<T*>(branch))return value;
  static T dummy;return &dummy;
 }
};
struct ALKitPopover : LLView{void toggle(){}};
struct Focus{LLView* getKeyboardFocus(){return nullptr;}}gFocusMgr;
struct ALAvatarSubjectPicker{static bool handleKey(KEY,MASK){return false;}};
struct LLFloater : LLView {virtual bool handleKeyHere(KEY,MASK){return false;}};
struct LLFloaterDirector : LLFloater {
 ALKitShell* mShell=nullptr;ALPanelLensGaze* mGazePanel=nullptr;bool mIsV2=true;LLView* mHotkeysPanel=nullptr;
 uuid_vec_t selection;LLUUID mCastRightClickId;
 LLScrollListCtrl* mGroupsList=nullptr;LLSpinCtrl* mGroupDelaySpinner=nullptr;LLComboBox* mGroupRunCombo=nullptr;
 LLLineEditor* mGroupRenameEditor=nullptr;LLButton* mGroupRenameBtn=nullptr;
 std::string mGroupMergeFrom,mGroupMergeTo,mGroupMergeText,mGroupDelayShownFor;
 U64 mGroupMergeRevision=0;std::vector<std::string>mGroupsListSnapshot;
 uuid_vec_t selectedCastIds()const{return selection;}std::string selectedManagedGroup()const{return mGroupsList->selected;}
 void selectGroupMembers(const std::string&){} void refreshGroupControls(){}void recoveryToast(const std::string&){}
 bool isTextEntryFocus(){return false;}
 void recordGroupEdit(const ALDirectorGroups::GroupSnapshot&,const std::string&,
                      const std::string& = {},const std::string& = {});
 void editGroupMembers(const uuid_vec_t&,const std::string&,bool=false);
 void updateCastGroupMenu(LLContextMenu*);void clearGroupMergeConfirmation();void onClickGroupRename();
 void onGroupsListSelect();bool handleKeyHere(KEY,MASK)override;
};
class ALKitHistory::GestureTimer {public:explicit GestureTimer(ALKitHistory*){}};
std::unordered_set<ALKitHistory*>& actorReferenceHistories(){static std::unordered_set<ALKitHistory*> histories;return histories;}
static S32 sKitHistorySuspend=0,sKitHistoryReplay=0;static bool sKitInputSourceSet=false;
std::string rowPage(LLView* v){auto* page=dynamic_cast<ALKitPage*>(v);return page?page->mPage:"";}
LLView* ALKitHistory::inputPage(){return nullptr;}
std::string ALKitHistory::currentContext(const std::string&,const std::string&)const{return {};}
bool ALKitHistory::entryMatches(Entry&,bool){return true;}
LLUICtrl* ALKitHistory::resolveWidget(Entry&){return nullptr;}
'''


def history_ui_methods():
    result = "\n".join(method(NV / "alkithistory.cpp", signature) for signature in (
        "ALKitHistory::ALKitHistory()", "ALKitHistory::~ALKitHistory()",
        "void ALKitHistory::rewriteActorReferences(",
        "ALKitHistory::ScopedSuspend::ScopedSuspend()", "ALKitHistory::ScopedSuspend::~ScopedSuspend()",
        "bool ALKitHistory::suspended()", "ALKitHistory::ScopedReplay::ScopedReplay()",
        "ALKitHistory::ScopedReplay::~ScopedReplay()", "void ALKitHistory::beginGesture(",
        "void ALKitHistory::recordCustom(", "void ALKitHistory::finishGesture()",
        "void ALKitHistory::barrier(", "bool ALKitHistory::prepareReplay(",
        "void ALKitHistory::discardStep(", "bool ALKitHistory::replay(",
        "bool ALKitHistory::canUndo(", "U64 ALKitHistory::stepSequence(",
        "bool ALKitHistory::canRedo(", "void ALKitHistory::undo(", "void ALKitHistory::redo("))
    result += "\n".join(method(NV / "alkitpage.cpp", signature) for signature in (
        "void ALKitPage::undoOnPage(", "bool ALKitPage::canUndoOnPage("))
    result += "\n".join(method(NV / "alpanellensgaze.cpp", signature) for signature in (
        "std::string ALPanelLensGaze::selectedApplyGroup() const",
        "void ALPanelLensGaze::restoreApplyGroup(", "void ALPanelLensGaze::refreshApplyGroupCombo()"))
    return result


def history_ui_tests():
    return r'''
 // Full applySceneData preserves exact legacy keys and their annotations.
 LLDirectorCast loaded;LLDirectorCast::active=&loaded;LLSD legacy=LLSD::emptyMap();
 const std::vector<std::string> legacy_names={"Guards","guards"," (none) ","  "+std::string(40,'x')+"  "};
 for(size_t i=0;i<legacy_names.size();++i){LLSD row;row["id"]=LLUUID(llformat("00000000-0000-0000-0000-%012d",int(i+20)));
  row["group"]=legacy_names[i];legacy["cast"].append(row);legacy["group_delays"][legacy_names[i]]=F32(i+1);legacy["group_rates"][legacy_names[i]]=int(i+1);}
 loaded.applySceneData(legacy);
 assert(loaded.getGroupNames()==legacy_names);
 for(size_t i=0;i<legacy_names.size();++i){assert(loaded.getGroupDelay(legacy_names[i])==F32(i+1));assert(loaded.getGroupRate(legacy_names[i])==S32(i+1));}

 LLDirectorCast edit_cast;LLDirectorCast::active=&edit_cast;
 for(const auto& id:{a,b,c}){LLDirectorCast::CastMember m;m.mId=id;edit_cast.mCast.push_back(m);edit_cast.mIds.push_back(id);}
 edit_cast.makeGroup({a},"Guards");edit_cast.makeGroup({b},"Extras");
 ALKitShell shell;ALKitPage page;page.shell=&shell;shell.current=&page;
 LLFloaterDirector director;director.mShell=&shell;auto& history=shell.history();
 LLComboBox apply_to,apply_group;apply_to.selected=1;ALPanelLensGaze gaze;
 gaze.mApplyTo=&apply_to;gaze.mApplyGroup=&apply_group;director.mGazePanel=&gaze;
 gaze.restoreApplyGroup("Guards");
 auto before_edit=edit_cast.captureGroups();edit_cast.addToGroup({a},"Extras");
 director.recordGroupEdit(before_edit,"Add to group");
 assert(history.canUndo("chrome") && !history.canUndo(page.mPage) && page.canUndoOnPage(false));
 // Real departure and unrelated annotation edits must survive keyboard replay.
 edit_cast.remove(c);edit_cast.setGroupRate("Extras",3);edit_cast.setGroupDelay("Extras",4);
 page.mPage="light_actors_tab";
 assert(director.handleKeyHere('Z',MASK_CONTROL));assert(edit_cast.getGroup(a)=="Guards");
 assert(!edit_cast.contains(c) && edit_cast.getGroupRate("Extras")==3 && edit_cast.getGroupDelay("Extras")==4);
 assert(page.canUndoOnPage(true));assert(director.handleKeyHere('Y',MASK_CONTROL));assert(edit_cast.getGroup(a)=="Extras");
 // Shell/header buttons use this same real dispatch. A newer page edit wins.
 int page_value=1;
 history.recordCustom(&page,"page","Page edit",[&](bool redo,std::string&){page_value=redo?1:0;return true;},false,{},{});
 page.undoOnPage(false);assert(page_value==0 && edit_cast.getGroup(a)=="Extras");
 page.undoOnPage(false);assert(edit_cast.getGroup(a)=="Guards");
 page.undoOnPage(true);assert(edit_cast.getGroup(a)=="Extras" && page_value==0);
 page.undoOnPage(true);assert(page_value==1);
 history.barrier("next scenario");

 // Merge, dissolve and last-member removal recover Hot bindings and Gaze selection.
 edit_cast.addToGroup({a},"Guards");edit_cast.mHotGazeSlots[0].mGroup="Guards";gaze.restoreApplyGroup("Guards");
 auto merge_before=edit_cast.captureGroups();edit_cast.renameGroup("Guards","Extras");
 director.recordGroupEdit(merge_before,"Merge group","Guards","Extras");
 assert(gaze.selectedApplyGroup()=="Extras" && edit_cast.mHotGazeSlots[0].mGroup=="Extras");
 page.undoOnPage(false);assert(edit_cast.getGroup(a)=="Guards" && edit_cast.mHotGazeSlots[0].mGroup=="Guards");
 assert(gaze.selectedApplyGroup()=="Guards");
 page.undoOnPage(true);assert(edit_cast.getGroup(a)=="Extras" && edit_cast.mHotGazeSlots[0].mGroup=="Extras");
 page.undoOnPage(false);history.barrier("dissolve");
 auto dissolve_before=edit_cast.captureGroups();edit_cast.dissolveGroup("Guards");
 director.recordGroupEdit(dissolve_before,"Dissolve group");
 assert(edit_cast.mHotGazeSlots[0].mGroup.empty() && gaze.selectedApplyGroup().empty());
 page.undoOnPage(false);assert(edit_cast.mHotGazeSlots[0].mGroup=="Guards" && gaze.selectedApplyGroup()=="Guards");
 history.barrier("remove");auto remove_before=edit_cast.captureGroups();edit_cast.removeFromGroup({a});
 director.recordGroupEdit(remove_before,"Remove from group");page.undoOnPage(false);
 assert(edit_cast.mHotGazeSlots[0].mGroup=="Guards" && gaze.selectedApplyGroup()=="Guards");
 // Rekey visits the real queue. Missing participants and changed touched values reject replay.
 ALKitHistory::rewriteActorReferences(a,c);edit_cast.mCast[0].mId=c;edit_cast.mIds[0]=c;
 page.undoOnPage(true);assert(edit_cast.getGroup(c).empty());
 edit_cast.setGroup(c,"Changed elsewhere");page.undoOnPage(false);assert(edit_cast.getGroup(c)=="Changed elsewhere");
 assert(!history.canUndo("chrome"));
 auto missing_before=edit_cast.captureGroups();edit_cast.addToGroup({c},"Extras");
 director.recordGroupEdit(missing_before,"Add");edit_cast.remove(c);page.undoOnPage(false);
 assert(!history.canUndo("chrome"));

 // Run the actual submenu population against a parent containing only a branch.
 director.selection={b};director.mCastRightClickId=b;
 LLContextMenu parent,submenu;LLContextMenuBranch branch;branch.branch=&submenu;parent.branch=&branch;
 director.updateCastGroupMenu(&parent);
 assert(submenu.items.size()==edit_cast.getGroupNames().size()+2);
 auto* choice=dynamic_cast<LLMenuItemCallGL*>(submenu.items.front().get());assert(choice);
 assert(choice->params.label.find("Extras")!=std::string::npos);
 choice->params.on_click.function(nullptr,LLSD());assert(edit_cast.getGroup(b)=="Extras");


 // Existing destinations must retain exact case and legacy shape in model/menu/page commands.
 LLDirectorCast::active=&loaded;director.selection={b};
 LLDirectorCast::CastMember extra;extra.mId=b;loaded.mCast.push_back(extra);loaded.mIds.push_back(b);
 for(const auto& name:legacy_names){
  loaded.addToGroup({b},name);assert(loaded.getGroup(b)==name);
  loaded.removeFromGroup({b});director.updateCastGroupMenu(&parent);
  bool clicked=false;
  for(auto& item:submenu.items)if(auto* entry=dynamic_cast<LLMenuItemCallGL*>(item.get()))
   if(entry->params.label.find(name+" (")==0){entry->params.on_click.function(nullptr,LLSD());clicked=true;break;}
  assert(clicked && loaded.getGroup(b)==name);
  loaded.removeFromGroup({b});director.editGroupMembers({b},name);assert(loaded.getGroup(b)==name);
 }
 // Exact legacy merge destinations use the same resolution in the model and Groups editor.
 LLScrollListCtrl legacy_groups;LLLineEditor legacy_editor;LLButton legacy_rename;
 LLSpinCtrl legacy_delay;LLComboBox legacy_run;
 director.mGroupsList=&legacy_groups;director.mGroupRenameEditor=&legacy_editor;director.mGroupRenameBtn=&legacy_rename;
 director.mGroupDelaySpinner=&legacy_delay;director.mGroupRunCombo=&legacy_run;
 for(const auto& name:legacy_names){
  loaded.setGroup(b,"Source");legacy_groups.selected="Source";legacy_editor.text=name;
  director.clearGroupMergeConfirmation();director.onClickGroupRename();assert(loaded.getGroup(b)=="Source");
  director.onClickGroupRename();assert(loaded.getGroup(b)==name);
 }
 director.mGroupsList=nullptr;LLDirectorCast::active=&edit_cast;
 history.barrier("binding conflict");
 // Changing either recipient field after a group edit invalidates undo and redo, with a reason.
 for(bool redo:{false,true})for(bool actor:{false,true}){
  edit_cast.setGroup(b,"Guards");auto& slot=edit_cast.mHotGazeSlots[0];slot.mGroup="Guards";slot.mWho=a;
  gaze.restoreApplyGroup("Guards");auto before=edit_cast.captureGroups();edit_cast.dissolveGroup("Guards");
  director.recordGroupEdit(before,"Dissolve group");
  if(redo)page.undoOnPage(false);
  if(actor)slot.mWho=b;else slot.mGroup="Extras";
  const auto rebound=edit_cast.captureGroups();LLNotificationsUtil::message.clear();
  // Direct real queue replay retains the rejection reason; page dispatch uses this queue too.
  if(redo)history.redo("chrome");else history.undo("chrome");
  assert(edit_cast.captureGroups()==rebound && !LLNotificationsUtil::message.empty());
  history.barrier("next binding conflict");
 }
 // Target and timing edits are independent of the binding pair and survive both directions.
 edit_cast.setGroup(b,"Guards");auto& independent=edit_cast.mHotGazeSlots[0];
 independent.mGroup="Guards";independent.mWho=a;auto independent_before=edit_cast.captureGroups();
 edit_cast.dissolveGroup("Guards");director.recordGroupEdit(independent_before,"Dissolve");
 independent.mTarget.mObjectRef=c;independent.mDwell=7.f;independent.mReturn=3.f;
 page.undoOnPage(false);assert(independent.mGroup=="Guards" && independent.mWho==a);
 page.undoOnPage(true);assert(independent.mGroup.empty() && independent.mWho.isNull());
 assert(independent.mTarget.mObjectRef==c && independent.mDwell==7.f && independent.mReturn==3.f);
 history.barrier("confirmation");

 // Actual confirmation handlers: text, selection and scene generations invalidate.
 LLScrollListCtrl groups;LLLineEditor editor;LLButton rename;LLSpinCtrl delay;LLComboBox run;
 director.mGroupsList=&groups;director.mGroupRenameEditor=&editor;director.mGroupRenameBtn=&rename;
 director.mGroupDelaySpinner=&delay;director.mGroupRunCombo=&run;
 LLSD confirmation=LLSD::emptyMap();
 for(const auto& id:{a,b}){LLSD row;row["id"]=id;row["group"]=id==a?"Guards":"Extras";confirmation["cast"].append(row);}
 auto arm=[&](){groups.selected="Guards";editor.text="Extras";director.onClickGroupRename();assert(edit_cast.getGroup(a)=="Guards");};
 edit_cast.applySceneData(confirmation);arm();
 editor.text="extras";director.onClickGroupRename();assert(edit_cast.getGroup(a)=="Guards");
 director.onClickGroupRename();assert(edit_cast.getGroup(a)=="Extras");
 edit_cast.applySceneData(confirmation);director.clearGroupMergeConfirmation();arm();
 groups.selected="";director.onGroupsListSelect();groups.selected="Guards";director.onClickGroupRename();assert(edit_cast.getGroup(a)=="Guards");
 edit_cast.applySceneData(confirmation);director.onClickGroupRename();assert(edit_cast.getGroup(a)=="Guards");
 director.onClickGroupRename();assert(edit_cast.getGroup(a)=="Extras");
 director.clearGroupMergeConfirmation();assert(director.mGroupMergeRevision==0 && director.mGroupMergeText.empty());
 std::cout<<"PASS: real chrome history queue + page/keyboard dispatch, sequence ordering, submenu branch, full applySceneData legacy keys, unrelated departure/rates/delays, Hot/Gaze undo, confirmation generations\n";
'''


def main():
    directory = check_directory("director_groups")
    flags, libraries = viewer_native_flags()
    flags += ["/UNDEBUG"]  # assertions must execute even with Release dependency flags
    # Run the registered TUT cases with their actual test framework, using
    # existing llcommon/llmath libraries rather than any viewer target.
    unit = r'''
#include "linden_common.h"
#include "lltut.h"
namespace tut { test_runner_singleton runner; }
#include "tests/aldirectorgroups_test.cpp"
int main() {
 for(int i=1;i<=3;++i){auto result=tut::director_groups_tests.run_test(i);
  if(result.result!=tut::test_result::ok){std::cerr<<result.message;return 1;}}
 std::cout<<"PASS: 3 registered aldirectorgroups TUT cases\n";
}
'''
    unit_flags = flags + [f'/I"{ROOT / "indra/externals/tut"}"']
    unit_result = compile_run(directory, unit, "aldirectorgroups_unit", flags=unit_flags, libraries=libraries)
    if unit_result.returncode:
        raise RuntimeError(unit_result.stdout + unit_result.stderr)
    print(unit_result.stdout.strip(), flush=True)
    source = fixture()
    result = compile_run(directory, source, flags=flags, libraries=libraries)
    if result.returncode:
        raise RuntimeError(result.stdout + result.stderr)
    print(result.stdout.strip(), flush=True)
    negatives = {
        "duplicate_case_name": ("if (!name.empty() && existingGroupName(names, name).empty()) return name;",
                                "if (!name.empty()) return name;"),
        "restore_changes_names": ("member.mGroup = found->second;", "member.mGroup = utf8str_tolower(found->second);"),
        "clone_saved": ("if (!saved_actor(m.mId)) continue;", "/* planted regression: persist clones */"),
        "dummy_submenu": ('menu->getChild<LLContextMenuBranch>("add_to_group")->getBranch()',
                          'menu->getChild<LLContextMenu>("add_to_group")'),
        "unreachable_chrome": ('const U64 chrome = shell->history().stepSequence("chrome", redo);',
                               'const U64 chrome = 0;'),
        "normalized_scene_name": ('m.mGroup = e["group"].asString();',
                                  'm.mGroup = ALDirectorGroups::normalizeGroupName(e["group"].asString());'),
        "whole_snapshot_replay": ('cast.restoreGroups(current);',
                                 'cast.restoreGroups(redo ? edit->after : edit->before);'),
        "unrelated_departure_invalidates": ('auto current = LLDirectorCast::instance().captureGroups();',
            'for(const auto& tag:edit->before.tags) if(!LLDirectorCast::instance().contains(tag.first)) return false; '
            'auto current = LLDirectorCast::instance().captureGroups();'),
        "hot_actor_not_captured": ('{ mHotGazeSlots[i].mGroup, mHotGazeSlots[i].mWho }',
                                   '{ mHotGazeSlots[i].mGroup, LLUUID::null }'),
        "existing_name_normalized": ('if (std::find(names.begin(), names.end(), requested) != names.end()) return requested;',
                                     '/* planted: normalize existing destinations */'),
        "lost_hot_binding": ('for (S32 i = 0; i < 4; ++i) result.hotGroups[i] = { mHotGazeSlots[i].mGroup, mHotGazeSlots[i].mWho };',
                             '/* planted regression: omit Hot bindings */'),
        "stale_merge_generation": ('mGroupMergeText != mGroupRenameEditor->getText() || mGroupMergeRevision != cast.getGroupRevision()',
                                   'mGroupMergeText != mGroupRenameEditor->getText()'),
        "stale_merge_editor": ('mGroupMergeText != mGroupRenameEditor->getText() || mGroupMergeRevision != cast.getGroupRevision()',
                               'mGroupMergeRevision != cast.getGroupRevision()'),
        "lost_gaze_selection": ('mGazePanel->restoreApplyGroup(redo ? edit->gazeAfter : edit->gazeBefore);',
                                '/* planted regression: omit Gaze selection replay */'),
    }
    def negative(item):
        name, (before, after) = item
        assert source.count(before) == 1, name
        result = compile_run(directory, source.replace(before, after), name,
                             flags=flags, libraries=libraries)
        if result.returncode == 0:
            raise RuntimeError("planted negative unexpectedly passed: " + name)
        print("PASS: rejected planted " + name, flush=True)
    with ThreadPoolExecutor(max_workers=2) as pool:
        list(pool.map(negative, negatives.items()))


if __name__ == "__main__":
    main()
