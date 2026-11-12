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
#include "llhandle.h"
struct LLView;
''' + header + r'''
struct ALCineLightRigManager {
 enum Slot { SELF=0, A=1, B, C, D };static constexpr S32 SLOT_COUNT=5;
 struct Rig {std::string name;S32 snaps=0;const std::string& getLitGroup()const{return name;}void setLitGroup(const std::string& n){name=n;++snaps;}};
 struct Blob {std::string mLitGroup;};Rig mInstances[5];Blob mBlobs[5];
 static bool validSlot(Slot s){return int(s)>=0 && int(s)<5;}
 LLSD captureLitGroups()const;void restoreLitGroups(const LLSD&);void restoreLitGroupDelta(const LLSD&,const LLSD&,const LLSD&);void setLitGroup(Slot,const std::string&);
 boost::signals2::connection mLitGroupConnection;
 boost::signals2::shared_connection_block blockLitGroupFollower(){return boost::signals2::shared_connection_block(mLitGroupConnection);}
 static bool instanceExists(){return true;}
 static ALCineLightRigManager& instance(){static ALCineLightRigManager value;return value;}
 void subjectRigOff(Slot slot){retired.push_back(slot);}
 std::vector<Slot> retired;
};
struct LLActorMover {
 struct GazeTarget { enum EMode { MOTION, CAMERA, OBJECT }; EMode mMode=MOTION; LLUUID mCastRef,mObjectRef; LLVector3d mFixedPoint;bool mPersonaOverride=false; };
 static LLActorMover& instance(){static LLActorMover value;return value;}
 void releaseHotGazeSlot(S32) { ++releases; } S32 releases=0;
 struct Waypoint {LLVector3d mPosGlobal,mCamPosGlobal;LLQuaternion mCamRot;LLUUID mAnim;
 F32 mRootAbove=0,mDwell=0,mSpeedOverride=0,mGroundOffset=0,mCornerRadius=0,mYawOffset=0,mCamFov=0,mCamTransition=0;bool mHasCam=false;};
 struct Path {std::vector<Waypoint>mNodes;LLUUID mArrivalTarget;F32 mSpeed=1,mCadence=1,mTension=0,mEaseIn=0,mEaseOut=0,mArrivalDir=0,mSyncLeadTrail=0;
 S32 mEndMode=0,mArrivalFacingMode=0,mShape=0;bool mGroundFollow=false,mPitchToSlope=false,mAirborne=false,mSyncToTake=false;
 LLVector3d mPrimitiveCenterGlobal;F32 mPrimitiveRadiusX=0,mPrimitiveRadiusY=0,mPrimitiveStartDeg=0,mPrimitiveSweepDeg=0,mPrimitiveYawDeg=0,mPrimitivePitchDeg=0,mPrimitiveRollDeg=0,mPrimitiveRise=0;
 bool isPrimitive()const{return false;}void markDirty(){}void rebuild(){}};
 struct PathState : Path {LLHandle<LLView> mUndoOwner;U64 mUndoSequence=0;};
 struct EditHistory {std::vector<PathState>mUndo,mRedo;};
 std::map<LLUUID,EditHistory>mHistory;static constexpr S32 UNDO_DEPTH=50;
 std::map<LLUUID,Path>paths;std::map<LLUUID,Path>& mPaths=paths;
 struct Move{bool mIsPath=false;};std::map<LLUUID,Move>mMoves;
 U64 mPathRevision=0;LLUUID mEditActor;S32 mEditNode=-1;
 void stop(const LLUUID&){}bool isPathWalking(const LLUUID&)const{return false;}
 void appendWaypointHere(const LLUUID& id){paths[id].mNodes.emplace_back();}
 void setEditNode(S32 n){mEditNode=n;}
 PathState captureState(const LLUUID&)const;void applyState(const LLUUID&,const PathState&);
 void snapshotForUndo(const LLUUID&,LLView*);U64 pagePathSequence(LLView*,bool,bool=false)const;
 bool pagePathReplayReady(LLView*,bool,U64)const;void replayPagePath(LLView*,bool);
 bool undoPath(const LLUUID&);bool redoPath(const LLUUID&);void discardPagePathRedo(LLView*);
 const Path* getPath(const LLUUID& id)const{auto it=paths.find(id);return it==paths.end()?nullptr:&it->second;}
 LLSD pathSceneData(const LLUUID&) const;
 void clearAllDirectorLookAtRuntime(){} void clearPath(const LLUUID&){}
 void clearDirectorLookAtRuntime(const LLUUID&){} void dropActor(const LLUUID&){}
 void setGazeTargetConfig(const LLUUID&,const GazeTarget&){}
 bool applyPathSceneData(const LLUUID&,const LLSD&){return true;}
 void clearHotGazes(){} void pruneEditActor(){} void onRosterMemberRemoved(const LLUUID&){}
};
namespace ALGazeHot {
 struct Slot { std::string mGroup; LLUUID mWho; LLActorMover::GazeTarget mTarget; F32 mDwell=2.f, mReturn=1.2f; };
 using Slots=std::array<Slot,4>;
 Slots readSlots(const LLSD& data){Slots slots;for(S32 i=0;i<4 && i<data.size();++i){slots[i].mGroup=data[i]["group"].asString();slots[i].mWho=data[i]["who"].asUUID();}return slots;}
 template<class F> LLSD writeSlots(const Slots& slots,F saved){LLSD data=LLSD::emptyArray();for(const auto& slot:slots)data.append(LLSD().with("group",slot.mGroup).with("who",saved(slot.mWho)?slot.mWho:LLUUID::null));return data;}
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
void writeGazeExpression(LLSD&,const LLActorMover::GazeTarget&,bool=false){}
LLSD writeGazeAimTarget(const LLActorMover::GazeTarget&);
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
bool isSceneClone(const LLUUID&);
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
 void cancelPendingStarts(){} void cancelPendingStart(const LLUUID&){}
 std::set<LLUUID> away;int resolves=0;void* resolve(const LLUUID& id){++resolves;return away.count(id)?nullptr:this;}
 bool isSelf(const LLUUID& id)const{return id==gAgentID || id.isNull();}
 void setActiveActorId(const LLUUID& id){mActiveActorId=id;}LLUUID getActiveActorId()const{return mActiveActorId;}
 void setGazeCues(const LLUUID&,const std::vector<int>&){}
 void rebuildMemberIndex(){}
 U64 getGroupRevision()const{return mGroupRevision;}
 void applySceneData(const LLSD&);void remove(const LLUUID&);
 U64 mSubjectRevision=1;U32 mSubjectAssignmentDepth=0;LLUUID mSubjectAssignmentBefore[4];
 void beginSubjectAssignment();void endSubjectAssignment();
 void setSubjectA(const LLUUID&);void setSubjectB(const LLUUID&);
 void setSubjectC(const LLUUID&);void setSubjectD(const LLUUID&);
 ALGazeHot::Slots mHotGazeSlots;
 LLUUID mSubjectA,mSubjectB,mSubjectC,mSubjectD,mActiveActorId;
 LLActorMover::GazeTarget mSelfGazeTarget,mSelfEyeGazeTarget;
 bool mSelfLookAtCamera=false,mSelfEyeGazeTargetEnabled=false;
 std::vector<int>mSelfGazeCues,mSelfGazeInfluenceKeys;int mSelfActorStyle=0;
 inline static std::map<LLUUID,LLUUID> aliases;
 static LLUUID canonicalActorId(const LLUUID& id){auto it=aliases.find(id);return it==aliases.end()?id:it->second;}
 static LLUUID savedSceneActorId(const LLUUID&);
 CastMember* getStoredMember(const LLUUID& id){for(auto& m:mCast)if(m.mId==id)return &m;return nullptr;}
 const CastMember* getStoredMember(const LLUUID& id)const{for(const auto& m:mCast)if(m.mId==id)return &m;return nullptr;}
 CastMember* getMember(const LLUUID& id){return getStoredMember(canonicalActorId(id));}
 const CastMember* getMember(const LLUUID& id)const{return getStoredMember(canonicalActorId(id));}
 void add(const LLUUID& id){const auto stored=canonicalActorId(id);if(!containsStored(stored)){CastMember m;m.mId=stored;mCast.push_back(m);mIds.push_back(stored);}}
 bool contains(const LLUUID& id)const;
 bool containsStored(const LLUUID& id)const{return getStoredMember(id);}
 U64 getCastRevision()const{return mCastRevision;}
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
 GroupSnapshot captureGroups()const;void restoreGroups(const GroupSnapshot&,const std::string& = {},const std::string& = {});
 const ALGazeHot::Slots& getHotGazeSlots()const{return mHotGazeSlots;}
 LLSD sceneData()const;
};
LLDirectorCast* LLDirectorCast::active=nullptr;
''' + '\nS32 slotIndex(ALCineLightRigManager::Slot s){return int(s); }\n' + '\n'.join(method(NV / 'alcinelightrigmanager.cpp', signature) for signature in ('LLSD ALCineLightRigManager::captureLitGroups()', 'void ALCineLightRigManager::restoreLitGroups(', 'void ALCineLightRigManager::restoreLitGroupDelta(', 'void ALCineLightRigManager::setLitGroup(')) + history_ui_adapters() + r'''
'''
    signatures = [
        "bool LLDirectorCast::contains(", "LLUUID LLDirectorCast::savedSceneActorId(",
        "LLSD writeGazeAimTarget(", "LLSD writeCueTarget(",
        "void LLDirectorCast::beginSubjectAssignment(", "void LLDirectorCast::endSubjectAssignment(",
        "void LLDirectorCast::setSubjectA(", "void LLDirectorCast::setSubjectB(",
        "void LLDirectorCast::setSubjectC(", "void LLDirectorCast::setSubjectD(",
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
    source += method(NV / "llactormover.cpp", "LLSD LLActorMover::pathSceneData(")
    source += history_ui_methods()
    source += "\nLLUUID path_key(const LLUUID& id){return id;}\n"
    source += "\n".join(method(NV / "llactormover.cpp", signature) for signature in (
        "LLActorMover::PathState LLActorMover::captureState(", "void LLActorMover::applyState(",
        "void LLActorMover::snapshotForUndo(const LLUUID& actor_id, LLView* owner)",
        "U64 LLActorMover::pagePathSequence(", "bool LLActorMover::pagePathReplayReady(",
        "void LLActorMover::replayPagePath(", "bool LLActorMover::undoPath(", "bool LLActorMover::redoPath(",
        "void LLActorMover::discardPagePathRedo("))
    # The production Path Add handler and history route run unchanged. Only the
    # world-position/ground-snap append and path render rebuild are adapters.
    source += "\nstruct ALPanelPathEditor {LLUUID mActor;LLView* mKitPage;void claimEdit(){}void onClickAdd();};\n"
    source += method(NV / "alpanelpatheditor.cpp", "void ALPanelPathEditor::onClickAdd(")
    # Compile the actual picker completion lambda, including stored-ID selection.
    callback = method(NV / "llfloaterdirector.cpp", "[this, group](const LLUUID& id)")
    source += "\nstd::function<void(const LLUUID&)> LLFloaterDirector::pickGroupMember(const std::string& group){return " + callback + ";}\n"
    source += "\n".join(method(NV / "llfloaterdirector.cpp", signature) for signature in (
        "void LLFloaterDirector::recordGroupEdit(", "void LLFloaterDirector::editGroupMembers(", "void LLFloaterDirector::updateCastGroupMenu(",
        "void LLFloaterDirector::clearGroupMergeConfirmation(", "void LLFloaterDirector::onClickGroupRename(",
        "void LLFloaterDirector::onGroupsListSelect(", "void LLFloaterDirector::onCommitGroupDelay(",
        "void LLFloaterDirector::refreshGroupControls(", "bool LLFloaterDirector::handleKeyHere("))
    history = method(NV / "alkithistory.cpp", "void ALKitHistory::recordCustom(")
    assert 'dynamic_cast<ALKitShell*>(owner) ? "chrome"' in history
    floater = (NV / "llfloaterdirector.cpp").read_text(encoding="utf-8")
    assert floater.count('[this](LLLineEditor*, void*) { clearGroupMergeConfirmation(); }') == 2
    assert 'mGroupDelayShownFor.clear();' in floater[floater.index("bool LLFloaterDirector::loadScene("):]
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
 // Merged remove() must execute the real A-D setters and retire each rig.
 LLDirectorCast subjectCast;
 LLDirectorCast::CastMember subjectMember;subjectMember.mId=a;subjectCast.mCast.push_back(subjectMember);subjectCast.mIds.push_back(a);
 subjectCast.mSubjectA=subjectCast.mSubjectB=subjectCast.mSubjectC=subjectCast.mSubjectD=a;
 auto& rig=ALCineLightRigManager::instance();rig.retired.clear();
 subjectCast.remove(a);
 assert(subjectCast.mSubjectA.isNull() && subjectCast.mSubjectB.isNull() && subjectCast.mSubjectC.isNull() && subjectCast.mSubjectD.isNull());
 assert(subjectCast.mSubjectRevision==5 && subjectCast.mSubjectAssignmentDepth==0);
 assert((rig.retired==std::vector<ALCineLightRigManager::Slot>{ALCineLightRigManager::A,ALCineLightRigManager::B,ALCineLightRigManager::C,ALCineLightRigManager::D}));
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
 saved.mHotGazeSlots[0].mGroup="Clone only";saved.mHotGazeSlots[0].mWho=stable;
 saved.mHotGazeSlots[1].mGroup="Real";
 saved.mCast[0].mGazeTarget.mCastRef=stable;saved.mSelfGazeTarget.mCastRef=runtime;
 saved.mCast[0].mEyeGazeTargetEnabled=true;saved.mCast[0].mEyeGazeTarget.mCastRef=runtime;
 saved.mSelfEyeGazeTargetEnabled=true;saved.mSelfEyeGazeTarget.mCastRef=stable;
 LLActorMover::GazeTarget clone_target;clone_target.mCastRef=runtime;clone_target.mObjectRef=stable;
 assert(writeCueTarget(clone_target)["cast_ref"].asUUID().isNull());
 assert(writeCueTarget(clone_target)["object_ref"].asUUID().isNull());
 auto& path=LLActorMover::instance().paths[a];path.mNodes.resize(1);path.mArrivalTarget=stable;
 const auto scene=saved.sceneData();
 assert(scene["cast"].size()==1 && scene["cast"][0]["id"].asUUID()==a);
 assert(scene["subject_a"].asUUID().isNull() && scene["subject_b"].asUUID().isNull() && scene["subject_c"].asUUID()==a);
 assert(scene["stepped_actor"].asUUID().isNull());
 assert(scene["group_delays"].size()==1 && scene["group_rates"].size()==1 && scene["light_response"].size()==1);
 assert(scene["hot_gaze_slots"][0]["group"].asString().empty() && scene["hot_gaze_slots"][0]["who"].asUUID().isNull());
 assert(scene["hot_gaze_slots"][1]["group"].asString()=="Real");
 assert(saved.mHotGazeSlots[0].mGroup=="Clone only");
 assert(scene["cast"][0]["gaze_target"]["cast_ref"].asUUID().isNull());
 assert(scene["self_gaze_target"]["cast_ref"].asUUID().isNull());
 assert(scene["cast"][0]["eye_gaze_target"]["cast_ref"].asUUID().isNull());
 assert(scene["self_eye_gaze_target"]["cast_ref"].asUUID().isNull());
 assert(scene["cast"][0]["actor_path"]["arrival_target"].asUUID().isNull());
 saved.mCast[1].mGroup="Real";assert(saved.sceneData()["group_delays"].size()==1);
 LLSD stale_scene=scene;stale_scene["hot_gaze_slots"][0]["group"]="Missing";stale_scene["hot_gaze_slots"][0]["who"]=a;
 LLDirectorCast stale_loaded;stale_loaded.applySceneData(stale_scene);
 assert(stale_loaded.mHotGazeSlots[0].mGroup.empty() && stale_loaded.mHotGazeSlots[0].mWho.isNull());
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
struct LLUICtrl : LLView {bool enabled=true,focused=false;void setValue(const LLSD&){} void onCommit(){} void setFocus(bool v){focused=v;}bool hasFocus()const{return focused;}};
struct ALKitRow : LLUICtrl {
 struct Meta{std::string setting,page_key;} value;const Meta& meta(){return value;}
 ALKitRow* sourceRow(){return nullptr;}
 void replayBoundValue(const LLSD&){} bool resettable(){return false;}void reset(){}
};
struct LLControlVariable{void set(const LLSD&){}};
struct Settings {std::shared_ptr<LLControlVariable>getControl(const std::string&){return {};}}gSavedSettings;
struct ALPanelWeatherSettings{struct SettingsBatch{void finish(){}};};
struct LLNotificationsUtil{inline static std::string message;static void add(const std::string&,const LLSD& args) {message=args["MESSAGE"].asString();}};
struct LLLineEditor : LLUICtrl {
 std::string text,name;std::string getText()const{return text;}void setText(const std::string& s){text=s;}
 const std::string& getName(){return name;} bool focused=false;void setFocus(bool v){focused=v;}bool hasFocus(){return focused;}void selectAll(){}
};
struct LLButton : LLUICtrl {std::string label;void setLabel(const std::string& s){label=s;}};
void setLabelIfChanged(LLButton* b,const std::string& s){if(b)b->setLabel(s);}
struct LLComboBox : LLUICtrl {
 LLSD selected;std::vector<std::string>items;
 LLSD getValue()const{return selected;}void removeall(){items.clear();selected=LLSD();}
 void add(const std::string& s,const LLSD&){items.push_back(s);}
 bool setSelectedByValue(const LLSD& s,bool){if(std::find(items.begin(),items.end(),s.asString())==items.end())return false;selected=s;return true;}
 void clear(){selected=LLSD();}bool setSimple(const std::string& s){selected=s;return true;}
 void clearRows(){items.clear();}void add(const std::string& s){items.push_back(s);}std::string getSelectedItemLabel()const{return selected.asString();}void setLabel(const std::string&){}
};
void setVisibleIfChanged(LLView*,bool){}
struct LLSpinCtrl : LLUICtrl {LLSD value;void setValue(F32 v){value=v;}LLSD getValue(){return value;}};
struct LLScrollListCell {LLSD value;LLSD getValue(){return value;}void setValue(const LLSD& v){value=v;}};
struct LLScrollListItem {LLSD value;std::vector<LLScrollListCell>columns;bool selected=false;
 LLSD getValue(){return value;}LLScrollListCell* getColumn(S32 i){return i<(S32)columns.size()?&columns[i]:nullptr;}};
enum {ADD_BOTTOM};
struct LLScrollListCtrl : LLUICtrl {
 bool isInVisibleChain()const{return true;}int deletes=0;
 std::string selected;std::vector<std::unique_ptr<LLScrollListItem>> rows;
 bool setSelectedByValue(const LLSD& s,bool){selected=s.asString();return true;}
 void deleteAllItems(){++deletes;rows.clear();selected.clear();}
 LLScrollListItem* getItem(const LLSD& id){for(auto& r:rows)if(r->value==id)return r.get();return nullptr;}
 void addElement(const LLSD& sd,int){auto row=std::make_unique<LLScrollListItem>();row->value=sd["value"];for(const auto& col:llsd::inArray(sd["columns"]))row->columns.push_back({col["value"]});rows.push_back(std::move(row));}
 std::vector<LLScrollListItem*>getAllSelected(){std::vector<LLScrollListItem*> out;for(auto& r:rows)if(r->selected)out.push_back(r.get());return out;}
 void selectMultiple(const uuid_vec_t& ids){for(auto& r:rows)r->selected=std::find(ids.begin(),ids.end(),r->value.asUUID())!=ids.end();}
};
struct LLTextBox:LLUICtrl{std::string text;};
void setTextIfChanged(LLTextBox* c,const std::string& s){if(c)c->text=s;}
void setTextIfChanged(LLLineEditor* c,const std::string& s){if(c)c->text=s;}
template<class T>void setEnabledIfChanged(T* c,bool v){assert(c);c->enabled=v;}
template<class T>void setToolTipIfChanged(T* c,const std::string&){assert(c);}
void setValueIfChanged(LLSpinCtrl* c,F32 v){c->value=v;}
struct ALLiveReadoutClock{bool first=true;explicit ALLiveReadoutClock(F32=4.f){}bool due(){bool v=first;first=false;return v;}void forceDue(){first=true;}};
struct ALPanelLensGaze {
 LLComboBox* mApplyTo=nullptr;LLComboBox* mApplyGroup=nullptr;std::vector<std::string>mLastApplyGroups;
 std::string appliedGroup()const{return selectedApplyGroup();}std::string selectedApplyGroup()const;void restoreApplyGroup(const std::string&);void refreshApplyGroupCombo();
};
struct ALKitShell;
struct ALKitPage : LLView {
 ALKitShell* shell=nullptr;std::string mPage="groups_tab";
 std::function<U64(bool)>mOwnedGroupSequence,mOwnedSequence;
 std::function<bool(bool,U64)>mOwnedReady;
 std::function<void(bool,U64)>mOwnedDiscardStep;std::function<void(bool)>mOwnedReplay;
 std::function<void()>mOwnedDiscardRedo;
 const std::string& pageKey()const{return mPage;}void historyEdited(bool);void undoOnPage(bool);bool canUndoOnPage(bool);
};
struct ALExposureSettingsHistory {
 static void discardPageRedo(ALKitPage*){}
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
 std::string mGroupMergeFrom,mGroupMergeTo,mGroupMergeText,mGroupDelayShownFor,mGroupMembersShownFor;
 LLScrollListCtrl* mGroupMembersList=nullptr;LLButton *mGroupNewBtn=nullptr,*mGroupAddSelectedBtn=nullptr,*mGroupAddBtn=nullptr,*mGroupRemoveBtn=nullptr,*mGroupDissolveBtn=nullptr,*mGroupStartBtn=nullptr,*mGroupStopBtn=nullptr;
 LLTextBox* mGroupUsedBy=nullptr;std::vector<std::string>mLastGroupNames;
 bool mGroupsTabDirty=true;U64 mGroupListRevision=0,mGroupCastRevision=0;ALLiveReadoutClock mGroupPresenceClock;
 struct GroupEdit {ALDirectorGroups::GroupSnapshot before,after;std::string gazeBefore,gazeAfter,renamedFrom,renamedTo;LLSD rigsBefore,rigsAfter;};
 std::shared_ptr<GroupEdit>mLastGroupDelayEdit;std::string mLastGroupDelayName;U64 mGroupDelayHistoryRevision=0,mGroupDelayHistorySequence=0;
 U64 mGroupMergeRevision=0;std::vector<std::string>mGroupsListSnapshot;
 uuid_vec_t selectedCastIds()const{return selection;}std::string selectedManagedGroup()const{return mGroupsList->selected;}
 void selectGroupMembers(const std::string&){selection.clear();} void refreshGroupControls();std::string toast;void recoveryToast(const std::string& s){toast=s;}
 bool selectLeafTab(const std::string&){return true;}
 bool isTextEntryFocus(){return false;}
 void recordGroupEdit(const ALDirectorGroups::GroupSnapshot&,const LLSD&,const std::string&,
                      const std::string& = {},const std::string& = {},const std::string& = {});
 void editGroupMembers(const uuid_vec_t&,const std::string&,bool=false);
 std::function<void(const LLUUID&)>pickGroupMember(const std::string& group);
 void updateCastGroupMenu(LLContextMenu*);void clearGroupMergeConfirmation();void onClickGroupRename();
 void onGroupsListSelect();void onCommitGroupDelay();bool handleKeyHere(KEY,MASK)override;
};
class ALKitHistory::GestureTimer {public:explicit GestureTimer(ALKitHistory*){}};
std::unordered_set<ALKitHistory*>& actorReferenceHistories(){static std::unordered_set<ALKitHistory*> histories;return histories;}
static S32 sKitHistorySuspend=0,sKitHistoryReplay=0;static bool sKitInputSourceSet=false;
static LLHandle<LLView> sKitBindingSource;
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
        "void ALKitHistory::discardStep(", "void ALKitHistory::discardRedo(", "bool ALKitHistory::replay(",
        "bool ALKitHistory::canUndo(", "U64 ALKitHistory::stepSequence(",
        "bool ALKitHistory::canRedo(", "void ALKitHistory::undo(", "void ALKitHistory::redo("))
    result += "\n".join(method(NV / "alkitpage.cpp", signature) for signature in (
        "void ALKitPage::historyEdited(", "void ALKitPage::undoOnPage(", "bool ALKitPage::canUndoOnPage("))
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
 director.recordGroupEdit(before_edit,ALCineLightRigManager::instance().captureLitGroups(),"Add to group");
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

 // Rename and merge recover five independent instance/blob names on MEMBERS or RENAMED replay.
 auto& rigs=ALCineLightRigManager::instance();
 rigs.setLitGroup(ALCineLightRigManager::A,"unchanged");const S32 snaps=rigs.mInstances[1].snaps;
 rigs.mBlobs[1].mLitGroup="different";rigs.setLitGroup(ALCineLightRigManager::A,"unchanged");
 assert(rigs.mBlobs[1].mLitGroup=="unchanged" && rigs.mInstances[1].snaps==snaps);
 int rig_follow_events=0;
 rigs.mLitGroupConnection=edit_cast.groupEvent.connect([&](const LLDirectorCast::GroupEvent& e){
  ++rig_follow_events;
  if(e.type==LLDirectorCast::GroupEvent::RENAMED)for(S32 i=0;i<5;++i){
   if(rigs.mInstances[i].name==e.oldName)rigs.mInstances[i].name=e.newName;
   if(rigs.mBlobs[i].mLitGroup==e.oldName)rigs.mBlobs[i].mLitGroup=e.newName;}
 });
 std::string replay_from,replay_to;
 auto alternate_events=edit_cast.groupEvent.connect([&](const LLDirectorCast::GroupEvent& e){
  if(e.type==LLDirectorCast::GroupEvent::MEMBERS && !replay_from.empty())
   edit_cast.groupEvent({LLDirectorCast::GroupEvent::RENAMED,replay_from,replay_to});
 });
 for(bool renamed_replay:{false,true})for(bool merge:{false,true}){
  history.barrier("rig rename");edit_cast.addToGroup({a},"Guards");edit_cast.addToGroup({b},"Extras");
  for(S32 i=0;i<5;++i){rigs.mInstances[i].name=i%2?"Extras":"Guards";rigs.mBlobs[i].mLitGroup=i%2?"Guards":"Extras";}
  const auto bindings=rigs.captureLitGroups();const auto groups=edit_cast.captureGroups();
  const std::string destination=merge?"Extras":"Crew";
  edit_cast.renameGroup("Guards",destination);auto renamed=rigs.captureLitGroups();
  director.recordGroupEdit(groups,bindings,"Rename group","Guards",destination);
  const int follows_before_replay=rig_follow_events;
  if(renamed_replay){replay_from=destination;replay_to="Guards";}
  page.undoOnPage(false);assert(rigs.captureLitGroups()==bindings && edit_cast.getGroup(a)=="Guards");
  if(renamed_replay){replay_from="Guards";replay_to=destination;}
  page.undoOnPage(true);assert(rigs.captureLitGroups()==renamed && edit_cast.getGroup(a)==destination);
  if(renamed_replay){replay_from=destination;replay_to="Guards";}
  page.undoOnPage(false);
  assert(rig_follow_events==follows_before_replay);
  replay_from.clear();replay_to.clear();
 }
 history.barrier("membership scenarios");
 for(S32 action=0;action<4;++action){
  edit_cast.addToGroup({a},"Guards");edit_cast.addToGroup({b},"Extras");
  const auto groups=edit_cast.captureGroups();const auto bindings=rigs.captureLitGroups();
  if(action==0)edit_cast.makeGroup({a,b});
  if(action==1)edit_cast.addToGroup({a},"Extras");
  if(action==2)edit_cast.removeFromGroup({a});
  if(action==3)edit_cast.dissolveGroup("Guards");
  const auto after=edit_cast.captureGroups();
  director.recordGroupEdit(groups,bindings,"Membership edit");
  page.undoOnPage(false);assert(edit_cast.captureGroups()==groups && rigs.captureLitGroups()==bindings);
  page.undoOnPage(true);assert(edit_cast.captureGroups()==after && rigs.captureLitGroups()==bindings);
  page.undoOnPage(false);history.barrier("next membership scenario");
 }
 // No-binding group edits never revert later Lights choices, in either direction.
 for(S32 action=0;action<5;++action){
  history.barrier("independent Lights");edit_cast.addToGroup({a},"Guards");edit_cast.addToGroup({b},"Extras");
  const auto groups=edit_cast.captureGroups();const auto bindings=rigs.captureLitGroups();
  if(action==0)edit_cast.setGroupDelay("Guards",.75f);
  if(action==1)edit_cast.makeGroup({a,b});
  if(action==2)edit_cast.addToGroup({a},"Extras");
  if(action==3)edit_cast.removeFromGroup({a});
  if(action==4)edit_cast.dissolveGroup("Guards");
  director.recordGroupEdit(groups,bindings,"Independent edit");
  rigs.setLitGroup(ALCineLightRigManager::A,"Later Lights");auto later=rigs.captureLitGroups();
  page.undoOnPage(false);assert(rigs.captureLitGroups()==later && edit_cast.captureGroups()==groups);
  rigs.setLitGroup(ALCineLightRigManager::B,"Even later");later=rigs.captureLitGroups();
  page.undoOnPage(true);assert(rigs.captureLitGroups()==later);
 }
 // A changed binding is restored only while it still matches the edit's value.
 // Check fields independently, including divergent instance/blob pairs and rename signals.
 for(bool signal:{false,true}){
  history.barrier("Lights delta conflicts");edit_cast.addToGroup({a},"Guards");edit_cast.addToGroup({b},"Extras");
  for(S32 i=0;i<5;++i){rigs.mInstances[i].name="Guards";rigs.mBlobs[i].mLitGroup="Guards";}
  rigs.setLitGroup(ALCineLightRigManager::D,"Extras");
  auto before=edit_cast.captureGroups();auto bindings=rigs.captureLitGroups();
  edit_cast.renameGroup("Guards","Crew");director.recordGroupEdit(before,bindings,"Rename","Guards","Crew");
  rigs.setLitGroup(ALCineLightRigManager::A,"User choice");
  rigs.mBlobs[2].mLitGroup="Blob choice";
  if(signal){replay_from="Crew";replay_to="Guards";}
  page.undoOnPage(false);
  assert(rigs.mInstances[0].name=="Guards" && rigs.mInstances[1].name=="User choice");
  assert(rigs.mInstances[2].name=="Guards" && rigs.mBlobs[2].mLitGroup=="Blob choice");
  assert(rigs.mInstances[4].name=="Extras");
  rigs.mInstances[3].name="Redo choice";
  if(signal){replay_from="Guards";replay_to="Crew";}
  page.undoOnPage(true);
  assert(rigs.mInstances[0].name=="Crew" && rigs.mInstances[1].name=="User choice");
  assert(rigs.mInstances[2].name=="Crew" && rigs.mBlobs[2].mLitGroup=="Blob choice");
  assert(rigs.mInstances[3].name=="Redo choice" && rigs.mBlobs[3].mLitGroup=="Crew");
  assert(rigs.mInstances[4].name=="Extras");
  replay_from.clear();replay_to.clear();
 }
 history.barrier("after delta scenarios");
 // Merge, dissolve and last-member removal recover Hot bindings and Gaze selection.
 edit_cast.addToGroup({a},"Guards");edit_cast.mHotGazeSlots[0].mGroup="Guards";gaze.restoreApplyGroup("Guards");
 edit_cast.mHotGazeSlots[1].mGroup="Extras";edit_cast.mHotGazeSlots[1].mWho=b;
 std::vector<LLDirectorCast::GroupEvent> merge_events;
 auto merge_connection=edit_cast.groupEvent.connect([&](const auto& e){merge_events.push_back(e);});
 auto merge_before=edit_cast.captureGroups();edit_cast.renameGroup("Guards","Extras");
 director.recordGroupEdit(merge_before,ALCineLightRigManager::instance().captureLitGroups(),"Merge group","Guards","Extras");
 assert(gaze.selectedApplyGroup()=="Extras" && edit_cast.mHotGazeSlots[0].mGroup=="Extras");
 page.undoOnPage(false);assert(edit_cast.getGroup(a)=="Guards" && edit_cast.mHotGazeSlots[0].mGroup=="Guards");
 assert(gaze.selectedApplyGroup()=="Guards");
 assert(merge_events.back().type==LLDirectorCast::GroupEvent::MEMBERS);
 assert(edit_cast.getGroup(b)=="Extras" && edit_cast.mHotGazeSlots[1].mGroup=="Extras" && edit_cast.mHotGazeSlots[1].mWho==b);
 page.undoOnPage(true);assert(edit_cast.getGroup(a)=="Extras" && edit_cast.mHotGazeSlots[0].mGroup=="Extras");
 assert(merge_events.back().type==LLDirectorCast::GroupEvent::RENAMED && merge_events.back().oldName=="Guards" && merge_events.back().newName=="Extras");
 assert(edit_cast.mHotGazeSlots[1].mGroup=="Extras");
 page.undoOnPage(false);history.barrier("dissolve");
 auto dissolve_before=edit_cast.captureGroups();edit_cast.dissolveGroup("Guards");
 director.recordGroupEdit(dissolve_before,ALCineLightRigManager::instance().captureLitGroups(),"Dissolve group");
 assert(edit_cast.mHotGazeSlots[0].mGroup.empty() && gaze.selectedApplyGroup().empty());
 page.undoOnPage(false);assert(edit_cast.mHotGazeSlots[0].mGroup=="Guards" && gaze.selectedApplyGroup()=="Guards");
 history.barrier("remove");auto remove_before=edit_cast.captureGroups();edit_cast.removeFromGroup({a});
 director.recordGroupEdit(remove_before,ALCineLightRigManager::instance().captureLitGroups(),"Remove from group");page.undoOnPage(false);
 assert(edit_cast.mHotGazeSlots[0].mGroup=="Guards" && gaze.selectedApplyGroup()=="Guards");
 // Rekey visits the real queue. Missing participants and changed touched values reject replay.
 ALKitHistory::rewriteActorReferences(a,c);edit_cast.mCast[0].mId=c;edit_cast.mIds[0]=c;
 page.undoOnPage(true);assert(edit_cast.getGroup(c).empty());
 edit_cast.setGroup(c,"Changed elsewhere");page.undoOnPage(false);assert(edit_cast.getGroup(c)=="Changed elsewhere");
 assert(!history.canUndo("chrome"));
 auto missing_before=edit_cast.captureGroups();edit_cast.addToGroup({c},"Extras");
 director.recordGroupEdit(missing_before,ALCineLightRigManager::instance().captureLitGroups(),"Add");edit_cast.remove(c);page.undoOnPage(false);
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
  director.recordGroupEdit(before,ALCineLightRigManager::instance().captureLitGroups(),"Dissolve group");
  if(redo)page.undoOnPage(false);
  if(actor)slot.mWho=b;else slot.mGroup="Extras";
  const auto rebound=edit_cast.captureGroups();LLNotificationsUtil::message.clear();
  // Direct real queue replay retains the rejection reason; page dispatch uses this queue too.
  if(redo)history.redo("chrome");else history.undo("chrome");
  auto expected_binding=rebound.hotGroups.at(0);
  if(!expected_binding.first.empty() && edit_cast.membersInGroup(expected_binding.first).empty())expected_binding={"",LLUUID::null};
  assert(edit_cast.mHotGazeSlots[0].mGroup==expected_binding.first && edit_cast.mHotGazeSlots[0].mWho==expected_binding.second);
  assert(edit_cast.getGroup(b)==(redo?"":"Guards") && !director.toast.empty());
  history.barrier("next binding conflict");
 }
 // Target and timing edits are independent of the binding pair and survive both directions.
 edit_cast.setGroup(b,"Guards");auto& independent=edit_cast.mHotGazeSlots[0];
 independent.mGroup="Guards";independent.mWho=a;auto independent_before=edit_cast.captureGroups();
 edit_cast.dissolveGroup("Guards");director.recordGroupEdit(independent_before,ALCineLightRigManager::instance().captureLitGroups(),"Dissolve");
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

 // Row selection preserves selected cast and scene/actor state. Refresh stays cheap.
 LLDirectorCast ui_cast;LLDirectorCast::active=&ui_cast;
 for(const auto& id:{a,b,c}){LLDirectorCast::CastMember m;m.mId=id;m.mLastName=id==a?"Rider":"Horse";ui_cast.mCast.push_back(m);}
 ui_cast.makeGroup({b},"Mount");ui_cast.makeGroup({c},"Other");
 // Simulate an old stored rider ID whose live canonical identity became the mount.
 LLDirectorCast::aliases[a]=b;
 assert(ui_cast.contains(a));ui_cast.addToGroup({a},"Riders");
 assert(ui_cast.getStoredMember(a)->mGroup=="Riders" && ui_cast.getStoredMember(b)->mGroup=="Mount");
 assert(ui_cast.makeGroup({a},"Rider again")=="Rider again");
 assert(ui_cast.getStoredMember(b)->mGroup=="Mount");
 LLScrollListCtrl group_rows,member_rows;LLLineEditor rename_editor;LLTextBox used_by;
 LLFloaterDirector ui;ui.mShell=&shell;ui.mGroupsList=&group_rows;ui.mGroupMembersList=&member_rows;
 ui.mGroupRenameEditor=&rename_editor;ui.mGroupUsedBy=&used_by;ui.mGroupDelaySpinner=&delay;
 ui.selection={b,c};group_rows.selected="Rider again";
 const U64 scene_revision=ui_cast.mSceneRevision;const LLUUID active_actor=ui_cast.mActiveActorId;
 ui.onGroupsListSelect();assert((ui.selection==uuid_vec_t{b,c}));
 ui.refreshGroupControls();assert(member_rows.rows.size()==1 && member_rows.rows[0]->value.asUUID()==a);
 assert(member_rows.rows[0]->columns[0].value.asString()=="Rider");
 const int resolved=ui_cast.resolves,deleted=group_rows.deletes,member_deleted=member_rows.deletes;
 for(int i=0;i<1000;++i)ui.refreshGroupControls();
 assert(ui_cast.resolves==resolved && group_rows.deletes==deleted && member_rows.deletes==member_deleted);
 ui_cast.away.insert(a);ui.mGroupPresenceClock.forceDue();ui.refreshGroupControls();
 assert(group_rows.deletes==deleted && member_rows.deletes==member_deleted);
 assert(group_rows.getItem("Rider again")->getColumn(2)->getValue().asInteger()==0);
 assert(member_rows.rows[0]->columns[0].value.asString()=="Rider (away)");
 assert(ui_cast.mSceneRevision==scene_revision && ui_cast.mActiveActorId==active_actor);
 ui_cast.setGroupRate("Rider again",2);ui.refreshGroupControls();assert(used_by.text.find("Stepped rate: +2")!=std::string::npos);
 // Legacy host has none of the v2-only widgets.
 LLFloaterDirector v1;LLScrollListCtrl legacy_rows;v1.mGroupsList=&legacy_rows;v1.refreshGroupControls();
 // Picker keeps stored rider IDs, but converts transient control-avatar IDs
 // before both enrollment and the exact stored-ID membership edit.
 auto pick=ui.pickGroupMember("Other");pick(a);
 assert(ui_cast.getStoredMember(a)->mGroup=="Other" && ui_cast.getStoredMember(b)->mGroup=="Mount");
 ui_cast.setGroup(a,"Rider again");
 const LLUUID control("00000000-0000-0000-0000-000000000071"),root("00000000-0000-0000-0000-000000000072");
 LLDirectorCast::aliases[control]=root;pick(control);
 assert(ui_cast.containsStored(root) && !ui_cast.containsStored(control) && ui_cast.getStoredMember(root)->mGroup=="Other");
 pick(LLUUID::null);assert(ui_cast.getStoredMember(gAgentID)->mGroup=="Other");
 ui.pickGroupMember("Missing")(control);assert(ui.toast=="Group 'Missing' no longer exists");
 LLDirectorCast::aliases.clear();
 // A no-op Add reports the existing membership.
 ui.selection={a};ui.editGroupMembers({a},"Rider again");assert(ui.toast=="Already in 'Rider again'");
 // Consecutive delay commits share a step; any intervening history edit ends it.
 history.barrier("delay coalescing");group_rows.selected="Rider again";
 delay.setValue(1.f);ui.onCommitGroupDelay();
 auto& delay_rigs=ALCineLightRigManager::instance();
 delay_rigs.setLitGroup(ALCineLightRigManager::A,"Independent Lights during delays");
 const auto delay_bindings=delay_rigs.captureLitGroups();
 delay.setValue(2.f);ui.onCommitGroupDelay();delay.setValue(3.f);ui.onCommitGroupDelay();
 assert(group_rows.hasFocus());page.undoOnPage(false);assert(ui_cast.getGroupDelay("Rider again")==0 && !history.canUndo("chrome"));
 assert(delay_rigs.captureLitGroups()==delay_bindings);
 page.undoOnPage(true);assert(ui_cast.getGroupDelay("Rider again")==3);
 assert(delay_rigs.captureLitGroups()==delay_bindings);
 // Real Path Add -> historyEdited(true) -> owned sequence, with no kit entry.
 history.barrier("Path interleave");ui_cast.setGroupDelay("Rider again",0);
 auto& mover=LLActorMover::instance();mover.paths.clear();mover.mHistory.clear();
 ALPanelPathEditor path_editor;path_editor.mActor=a;path_editor.mKitPage=&page;
 page.mOwnedSequence=[&](bool redo){return mover.pagePathSequence(&page,redo);};
 page.mOwnedReplay=[&](bool redo){mover.replayPagePath(&page,redo);};
 page.mOwnedReady=[&](bool redo,U64 seq){return mover.pagePathReplayReady(&page,redo,seq);};
 page.mOwnedDiscardRedo=[&](){mover.discardPagePathRedo(&page);};
 delay.setValue(1.f);ui.onCommitGroupDelay();path_editor.onClickAdd();
 assert(mover.getPath(a)->mNodes.size()==1);
 delay.setValue(2.f);ui.onCommitGroupDelay();
 page.undoOnPage(false);assert(ui_cast.getGroupDelay("Rider again")==1 && mover.getPath(a)->mNodes.size()==1);
 page.undoOnPage(false);assert(!mover.getPath(a) && ui_cast.getGroupDelay("Rider again")==1);
 page.undoOnPage(false);assert(ui_cast.getGroupDelay("Rider again")==0);
 page.undoOnPage(true);assert(ui_cast.getGroupDelay("Rider again")==1 && !mover.getPath(a));
 page.undoOnPage(true);assert(mover.getPath(a)->mNodes.size()==1 && ui_cast.getGroupDelay("Rider again")==1);
 page.undoOnPage(true);assert(ui_cast.getGroupDelay("Rider again")==2);
 // Bare owned activity must break coalescing even before a sequence is assigned.
 history.barrier("owned notification");delay.setValue(3.f);ui.onCommitGroupDelay();
 page.historyEdited(true);delay.setValue(4.f);ui.onCommitGroupDelay();
 page.undoOnPage(false);assert(ui_cast.getGroupDelay("Rider again")==3);
 // A direct owned replay changes the shared sequence without touching kit revision.
 history.barrier("owned replay");delay.setValue(5.f);ui.onCommitGroupDelay();
 const auto kit_revision=history.revision();assert(mover.undoPath(a));assert(history.revision()==kit_revision);
 delay.setValue(6.f);ui.onCommitGroupDelay();page.undoOnPage(false);assert(ui_cast.getGroupDelay("Rider again")==5);
 // Ordinary kit mutations are still boundaries.
 history.barrier("kit interleave");delay.setValue(7.f);ui.onCommitGroupDelay();
 history.recordCustom(&page,"page","Intervening kit edit",[](bool,std::string&){return true;});
 delay.setValue(8.f);ui.onCommitGroupDelay();page.undoOnPage(false);assert(ui_cast.getGroupDelay("Rider again")==7);
 page.mOwnedSequence={};page.mOwnedReplay={};page.mOwnedReady={};page.mOwnedDiscardRedo={};
 // Rename undo/redo publishes name events, so name-following consumers can track it.
 history.barrier("rename events");rename_editor.text="Renamed";ui.onClickGroupRename();
 std::vector<LLDirectorCast::GroupEvent> rename_events;auto connection=ui_cast.groupEvent.connect([&](const auto& e){rename_events.push_back(e);});
 page.undoOnPage(false);assert(rename_events.size()==1 && rename_events.back().type==LLDirectorCast::GroupEvent::RENAMED);
 assert(rename_events.back().oldName=="Renamed" && rename_events.back().newName=="Rider again");
 page.undoOnPage(true);assert(rename_events.back().oldName=="Rider again" && rename_events.back().newName=="Renamed");
 // Changing Gaze apply selection skips only that ancillary part and keeps the step replayable.
 history.barrier("later Gaze apply");ui.mGazePanel=&gaze;gaze.restoreApplyGroup("Renamed");
 auto dissolve_ui=ui_cast.captureGroups();ui_cast.dissolveGroup("Renamed");ui.recordGroupEdit(dissolve_ui,ALCineLightRigManager::instance().captureLitGroups(),"Dissolve group");
 gaze.restoreApplyGroup("Other");page.undoOnPage(false);
 assert(ui_cast.getGroup(a)=="Renamed" && gaze.selectedApplyGroup()=="Other" && history.canRedo("chrome"));
 assert(ui.toast.find("later Gaze bindings were kept")!=std::string::npos);
 // Missing group bindings restore to an inert slot rather than a single actor.
 auto empty_binding=ui_cast.captureGroups();empty_binding.hotGroups[0]={"Missing",a};ui_cast.restoreGroups(empty_binding);
 assert(ui_cast.mHotGazeSlots[0].mGroup.empty() && ui_cast.mHotGazeSlots[0].mWho.isNull());
 std::cout<<"PASS: stored rider IDs, clone gaze/path/Hot filters, selection preservation, Gaze conflict replay, delay coalescing, rename events, revision/clock refresh and V1 null widgets\n";
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
 for(int i=1;i<=4;++i){auto result=tut::director_groups_tests.run_test(i);
  if(result.result!=tut::test_result::ok){std::cerr<<result.message;return 1;}}
 std::cout<<"PASS: 4 registered aldirectorgroups TUT cases\n";
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
        "lost_rig_bindings": ('restoreLitGroups(result);', '/* planted lost rig binding replay */'),
        "rig_full_snapshot": ('restoreLitGroups(result);', 'restoreLitGroups(desired);'),
        "rig_no_conflict_check": (' && current[i][field] == expected[i][field]', ''),
        "rig_double_rename": ('auto block_rig_follower = rigs.blockLitGroupFollower();', '/* planted follower left active */'),
        "delay_ignores_owned_notification": ('shell->history().noteOwnedEdit();', '/* planted: ignore owned activity */'),
        "delay_ignores_owned_sequence": ('mGroupDelayHistorySequence == alUndoSequenceClock() &&', ''),
        "picker_transient_uuid": ('const LLUUID actor = cast.containsStored(picked) ? picked :\n                    LLDirectorCast::canonicalActorId(picked);', 'const LLUUID actor = picked;'),
        "picker_canonicalizes_stored_rider": ('cast.containsStored(picked) ? picked :', ''),
        "merge_inverse_rename": ('const bool split = !redo && !edit->renamedFrom.empty() &&', 'const bool split = false && !redo && !edit->renamedFrom.empty() &&'),
        "stored_rider_lookup": ('if (CastMember* member = getStoredMember(id))', 'if (CastMember* member = getMember(id))'),
        "hot_group_saved": ('slot.mGroup.clear();\n            slot.mWho.setNull();\n        }\n    data["hot_gaze_slots"]', '/* planted: retain clone-only group */\n        }\n    data["hot_gaze_slots"]'),
        "gaze_clone_ref": ('gaze_sd["cast_ref"] = saved_id(m.mGazeTarget.mCastRef);', 'gaze_sd["cast_ref"] = m.mGazeTarget.mCastRef;'),
        "row_replaces_cast": ('    mGroupPresenceClock.forceDue();\n    if (mGroupRunCombo)', '    selectGroupMembers(name);\n    mGroupPresenceClock.forceDue();\n    if (mGroupRunCombo)'),
        "hot_rebind_discards_step": ('    return true;\n}\n} // namespace ALDirectorGroups', '    return patchDelta(current.hotGroups, expected.hotGroups, target.hotGroups, apply);\n}\n} // namespace ALDirectorGroups'),
        "rename_members_event": ('renamed_from.empty() ? GroupEvent::MEMBERS : GroupEvent::RENAMED', 'GroupEvent::MEMBERS'),
        "refresh_without_gate": ('mGroupCastRevision == cast.getCastRevision() && !presence_due) return;', 'mGroupCastRevision == cast.getCastRevision() && false) return;'),
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
        "whole_snapshot_replay": ('cast.restoreGroups(current, split ? std::string() : redo ? edit->renamedFrom : edit->renamedTo,',
                                 'cast.restoreGroups(redo ? edit->after : edit->before, split ? std::string() : redo ? edit->renamedFrom : edit->renamedTo,'),
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
                                '(void)0; /* planted regression: omit Gaze selection replay */'),
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
