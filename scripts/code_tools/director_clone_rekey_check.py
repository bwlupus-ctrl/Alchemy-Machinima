"""Execute production clone-reference transaction methods with small native stores.

No viewer build. The existing actor-bar harness separately executes migrateActor
and the real Studio consumer; this fixture exercises the cross-reference pass,
including the collision branch that intentionally skips migrateActor.
"""
from pathlib import Path
import os
import subprocess
from relight_area_policy_check import method


def check():
    nv = Path("indra/newview")
    fixture = r'''
#include <algorithm>
#include <cassert>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>
using S32=int; using U32=unsigned; using U64=unsigned long long;
struct LLUUID {
 int value=0; bool isNull() const{return !value;} bool notNull() const{return value;}
 void setNull(){value=0;} static const LLUUID null;
 bool operator==(LLUUID b) const{return value==b.value;}
 bool operator!=(LLUUID b) const{return !(*this==b);}
 bool operator<(LLUUID b) const{return value<b.value;}
};
const LLUUID LLUUID::null{};
std::ostream& operator<<(std::ostream& s, LLUUID id){return s<<id.value;}
#define LL_WARNS(x) std::cerr
#define LL_ENDL std::endl
struct LLVOAvatar;
struct LLViewerObject {
 virtual ~LLViewerObject()=default;virtual LLVOAvatar* asAvatar(){return nullptr;}
 LLViewerObject* getRootEdit(){return this;}LLVOAvatar* getControlAvatar(){return nullptr;}
};
struct LLVOAvatar:LLViewerObject {
 LLUUID id;bool dead=false;bool ghost=true;
 LLVOAvatar* asAvatar() override{return this;}bool isDead(){return dead;}
 bool isGhostAvatar(){return ghost;}std::string getFullname(){return "clone";}
};
struct ObjectList{std::map<LLUUID,LLViewerObject*> objects;
 LLViewerObject* findObject(LLUUID id){auto it=objects.find(id);return it==objects.end()?nullptr:it->second;}
} gObjectList;
LLVOAvatar self;LLVOAvatar* gAgentAvatarp=&self;
bool isAgentAvatarValid(){return true;}
struct LLActorMover {
 enum {GAZE_TANGENT=0,GAZE_CAST=2,GAZE_OBJECT=4};
 struct GazeTarget {
  enum EMode{MOTION=0,CAMERA=1,CAST_MEMBER=2,OBJECT=4};
  EMode mMode=MOTION; LLUUID mCastRef,mObjectRef;
 };
 struct Path {LLUUID mArrivalTarget; int mArrivalFacingMode=0;};
 struct EditHistory{std::vector<Path> mUndo,mRedo;};
 struct Gaze {LLUUID mCastTarget,mObjectTarget; int mTarget=0;
  struct {GazeTarget mAuthored;} mParamTransition;
  bool invalidated=false;
 };
 struct DirectorGaze{Gaze mGaze; GazeTarget mLastTarget,mCueFromBaseTarget,mCueFromTarget;};
 struct Follow{LLUUID mLeader;};
 std::map<LLUUID,Path> mPaths;
 std::map<LLUUID,EditHistory> mHistory;
 std::map<LLUUID,Gaze> mGazes;
 std::map<LLUUID,DirectorGaze> mDirectorGazes;
 std::map<LLUUID,Follow> mFollows;
 U64 mGazeRevision=0;
 static LLActorMover& instance(){static LLActorMover m;return m;}
 static bool rewriteGazeTarget(GazeTarget&,const LLUUID&,const LLUUID&);
 void rewriteActorReferences(const LLUUID&,const LLUUID&);
 static void invalidateGazeCache(Gaze& g,bool){g.invalidated=true;}
 void clearDirectorLookAtRuntime(LLUUID){}
 void dropActor(LLUUID id){mPaths.erase(id);mHistory.erase(id);mGazes.erase(id);mFollows.erase(id);}
 // Ownership migration itself is executed from production in actor_bar_check.
 void migrateActor(LLUUID old,LLUUID next){
  if(mPaths.count(old)){mPaths[next]=mPaths.at(old);mPaths.erase(old);}
  if(mHistory.count(old)){mHistory[next]=mHistory.at(old);mHistory.erase(old);}
 }
};
struct ALKitHistory {
 struct CustomEntry{std::function<void(const LLUUID&,const LLUUID&)> rewrite_actor;};
 struct Step{std::vector<CustomEntry> custom;};
 std::vector<Step> mUndo,mRedo;std::unique_ptr<Step> mOpen;U64 mRevision=0;
 static void rewriteActorReferences(const LLUUID&,const LLUUID&);
};
std::set<ALKitHistory*>& actorReferenceHistories(){static std::set<ALKitHistory*> s;return s;}
struct ALProjectorTracker {
 enum Mode{AIM,FOLLOW};
 struct State{LLUUID subject;bool hasSubject=true,captured=true,capturePending=true;
  bool enabled=true,restorePending=false;double missingSince=4.;};
 std::map<LLUUID,State> mStates;U64 mRevision=0;
 static ALProjectorTracker& instance(){static ALProjectorTracker s;return s;}
 void rewriteActorReferences(const LLUUID&,const LLUUID&);
 void stop(LLUUID id,const char*){auto& s=mStates.at(id);s.enabled=false;s.restorePending=true;}
};
struct ALDirectorAnimSwitcher {
 std::set<LLUUID> mPosedAvatars;
 static ALDirectorAnimSwitcher& instance(){static ALDirectorAnimSwitcher s;return s;}
 void rewriteActorReferences(LLUUID id,LLUUID){mPosedAvatars.erase(id);}
};
static LLUUID sCinematicFollowTarget;
struct LLCinematicCamera{static void onRuntimeTargetReplaced(const LLUUID&,const LLUUID&);};
struct LLCharacter{virtual ~LLCharacter()=default;static std::vector<LLCharacter*> sInstances;};
std::vector<LLCharacter*> LLCharacter::sInstances;
struct LLGhostAvatar:LLCharacter {
 LLUUID mAnimationSourceId;static std::map<LLUUID,U32> sLiveSourceCounts;
 void releaseSourceReference();
 static void rewriteAnimationSourceReferences(const LLUUID&,const LLUUID&);
};
std::map<LLUUID,U32> LLGhostAvatar::sLiveSourceCounts;
struct ALGhostStudio {
 enum ELookTarget{LOOK_TARGET_CAMERA,LOOK_TARGET_ME,LOOK_TARGET_ACTOR,LOOK_TARGET_GHOST,LOOK_TARGET_POINT};
 enum{TURN_MODE_OFF,TURN_MODE_ONCE};
 struct Instance{LLUUID mId,mSource,mEntityId;bool mSourceRemoved=false;
  ELookTarget mLookTarget=LOOK_TARGET_ACTOR,mTurnTarget=LOOK_TARGET_ACTOR;
  LLUUID mLookTargetId,mTurnTargetId;int mFootGlobal=3,mLookPointGlobal=0,mTurnMode=TURN_MODE_ONCE;
  bool mKeepFacing=true;};
 std::vector<Instance> mInstances;
 struct CrowdPlacementDraft{bool mActive=false;struct {LLUUID mSourceId,mEntityId;} mSource;} mCrowdDraft;
 U64 mInstanceRevision=0;
 Instance* getInstance(LLUUID id){for(auto& i:mInstances)if(i.mId==id)return &i;return nullptr;}
 LLVOAvatar* resolveEntityClone(LLUUID id){auto* i=getInstance(id);auto* o=i?gObjectList.findObject(i->mEntityId):nullptr;return o?o->asAvatar():nullptr;}
 static ALGhostStudio& instance(){static ALGhostStudio s;return s;}
 void rewriteActorReferences(const LLUUID&,const LLUUID&);
};
struct ALCineLightRigManager {
 static constexpr int SLOT_COUNT=5;
 struct ParamBlob{LLUUID mAnchor;bool mEnabled=true;};
 struct Rig{LLUUID anchor;LLUUID getAnchor(){return anchor;}void setAnchor(LLUUID id){anchor=id;}};
 ParamBlob mBlobs[SLOT_COUNT];Rig mInstances[SLOT_COUNT];int mSelected=0;U64 mConfigurationRevision=0;
 std::set<int> shutdown;int writes=0,saves=0;
 int slotIndex(int slot){return slot;}ParamBlob captureSelected(){return mBlobs[mSelected];}
 void shutdownSlot(int i){shutdown.insert(i);}void removeMirror(LLUUID){}
 void writeSelectedSettings(){++writes;}void persist(){++saves;}
 static bool instanceExists(){return true;}
 static ALCineLightRigManager& instance(){static ALCineLightRigManager s;return s;}
 void rewriteActorReferences(const LLUUID&,const LLUUID&);
};
struct ALPanelGhostStudio {static void rewriteActorReferences(LLUUID,LLUUID){}};
struct ALFloaterGazeCues{static void rewriteActorReferences(LLUUID,LLUUID){}};
struct LLFloaterDirector{static void rewriteActorReferences(LLUUID,LLUUID){}};
struct ALPanelLensGaze{static void rewriteActorReferences(LLUUID,LLUUID){}};
struct ALClientHeight {static ALClientHeight& instance(){static ALClientHeight s;return s;} void reset(LLUUID){}};
struct ALAvatarLightResponse {static bool instanceExists(){return false;} static ALAvatarLightResponse& instance(){static ALAvatarLightResponse s;return s;} void inherit(LLUUID,bool){}};
struct ALAvatarStaging {
 enum class FacingMode{TRACK,LOCK};enum class TrackSource{ACTOR,POINT};
 struct Entry{LLUUID mTrackTarget;float mLockYaw=0;FacingMode mMode=FacingMode::TRACK;
  TrackSource mTrackSource=TrackSource::ACTOR;bool mTrackPointValid=true;};
 std::map<LLUUID,Entry> mEntries;std::set<LLUUID> mRestorePending,mBallCentreDancers;
 static ALAvatarStaging& instance(){static ALAvatarStaging s;return s;}
 void removeActorReferences(const LLUUID&);
 void remove(LLUUID id){mEntries.erase(id);}
 float currentWorldYaw(LLUUID,const Entry&){return 1.5f;}
 void changed(const char*,LLUUID,bool){}
};
struct ALDanceBall {
 std::set<LLUUID> mRoster,mExcluded;std::map<LLUUID,int> mRuntime;U64 mRosterRevision=0;
 static ALDanceBall& instance(){static ALDanceBall s;return s;}
 void removeDancer(LLUUID id){mRoster.erase(id);}
 void removeActorReferences(const LLUUID&);
};
struct LLDirectorCast {
 using GazeTarget=LLActorMover::GazeTarget;
 struct GazeCue{GazeTarget mTarget;};using GazeCueList=std::vector<GazeCue>;
 struct CastMember{LLUUID mId;std::string mLastName;GazeTarget mGazeTarget,mEyeGazeTarget;GazeCueList mGazeCues;};
 std::vector<CastMember> mCast;std::vector<LLUUID> mIds;
 GazeTarget mSelfGazeTarget,mSelfEyeGazeTarget;GazeCueList mSelfGazeCues;
 struct Start{LLUUID mId;};std::vector<Start> mPendingStarts;
 LLUUID mSubjectA,mSubjectB,mSubjectC,mSubjectD,mActiveActorId;
 std::set<LLUUID> mLookAtCameraIds;U64 mCastRevision=0,mGazeCueRevision=0,mSubjectRevision=0;
 void rewriteActorReferences(const LLUUID&,const LLUUID&);
 void replaceActorId(const LLUUID&,const LLUUID&,const LLUUID& = LLUUID::null);
 static bool isSelf(LLUUID id){return id.isNull();}
 static LLUUID canonicalActorId(LLUUID id){return id;}
 LLVOAvatar* resolve(const LLUUID&);
 CastMember* getMember(LLUUID id){for(auto& m:mCast)if(m.mId==id)return &m;return nullptr;}
 bool contains(LLUUID id) const{return std::any_of(mCast.begin(),mCast.end(),[&](auto& m){return m.mId==id;});}
 void remove(LLUUID id){mCast.erase(std::remove_if(mCast.begin(),mCast.end(),[&](auto& m){return m.mId==id;}),mCast.end());}
 void setActiveActorId(LLUUID id){mActiveActorId=id;}
 void cancelPendingStart(LLUUID id){mPendingStarts.erase(std::remove_if(mPendingStarts.begin(),mPendingStarts.end(),[&](auto& s){return s.mId==id;}),mPendingStarts.end());}
 void rebuildMemberIndex(){} void touchScene(){}
};
'''
    for file, signature in (
        ("llactormover.cpp", "bool LLActorMover::rewriteGazeTarget("),
        ("llactormover.cpp", "void LLActorMover::rewriteActorReferences("),
        ("alkithistory.cpp", "void ALKitHistory::rewriteActorReferences("),
        ("alprojectortracker.cpp", "void ALProjectorTracker::rewriteActorReferences("),
        ("llcinematiccamera.cpp", "void LLCinematicCamera::onRuntimeTargetReplaced("),
        ("llghostavatar.cpp", "void LLGhostAvatar::releaseSourceReference("),
        ("llghostavatar.cpp", "void LLGhostAvatar::rewriteAnimationSourceReferences("),
        ("alghoststudio.cpp", "void ALGhostStudio::rewriteActorReferences("),
        ("alavatarstaging.cpp", "void ALAvatarStaging::removeActorReferences("),
        ("aldanceball.cpp", "void ALDanceBall::removeActorReferences("),
        ("alcinelightrigmanager.cpp", "void ALCineLightRigManager::rewriteActorReferences("),
        ("lldirectorcast.cpp", "void LLDirectorCast::rewriteActorReferences("),
        ("lldirectorcast.cpp", "void LLDirectorCast::replaceActorId("),
        ("lldirectorcast.cpp", "LLVOAvatar* LLDirectorCast::resolve("),
    ):
        fixture += method(nv / file, signature) + "\n"
    # Execute the exact production custom-history callback, not a model of it.
    callback = method(nv / "alpanelgazecues.cpp", "[snapshot](const LLUUID& old_id")
    fixture += r'''
int main(){
 const LLUUID A{1},R{2},S{3},N{4},other{9},projector{20};
 LLVOAvatar original,newClone;original.id=R;newClone.id=N;
 gObjectList.objects[R]=&original;
 using T=LLActorMover::GazeTarget;
 auto target=[](LLUUID id){T t;t.mMode=T::CAST_MEMBER;t.mCastRef=id;return t;};
 LLDirectorCast cast;
 LLDirectorCast::CastMember a;a.mId=A;a.mGazeTarget=target(R);a.mEyeGazeTarget=target(R);
 a.mGazeCues={{target(R)}};
 LLDirectorCast::CastMember clone;clone.mId=R;clone.mGazeTarget=target(A);
 cast.mCast={a,clone};cast.mSelfGazeTarget=target(R);cast.mSelfEyeGazeTarget=target(R);
 cast.mSelfGazeCues={{target(R)}};
 cast.mSubjectA=R;cast.mSubjectB=R;cast.mSubjectC=R;cast.mSubjectD=R;
 cast.mActiveActorId=R;cast.mLookAtCameraIds={R};cast.mPendingStarts={{R}};
 auto& mover=LLActorMover::instance();
 mover.mPaths[A]={R,2};mover.mHistory[A].mUndo={{R,2}};mover.mHistory[A].mRedo={{R,2}};
 mover.mPaths[other]={other,2};
 auto& g=mover.mGazes[A];g.mCastTarget=R;g.mTarget=LLActorMover::GAZE_CAST;
 g.mParamTransition.mAuthored=target(R);
 auto& d=mover.mDirectorGazes[A];d.mGaze=g;
 d.mLastTarget=d.mCueFromBaseTarget=d.mCueFromTarget=target(R);
 mover.mFollows[A].mLeader=R;
 auto& tracker=ALProjectorTracker::instance();tracker.mStates[projector].subject=R;
 sCinematicFollowTarget=R;
 auto& rig=ALCineLightRigManager::instance();rig.mBlobs[0].mAnchor=R;rig.mInstances[0].anchor=R;
 auto& studio=ALGhostStudio::instance();
 ALGhostStudio::Instance look;look.mLookTargetId=look.mTurnTargetId=R;studio.mInstances={look};
 studio.mInstances.front().mSource=R;
 ALGhostStudio::Instance source;source.mId=S;studio.mInstances.push_back(source);
 studio.mCrowdDraft.mActive=true;studio.mCrowdDraft.mSource.mSourceId=R;
 studio.mCrowdDraft.mSource.mEntityId=R;
 LLGhostAvatar dependent;dependent.mAnimationSourceId=R;LLCharacter::sInstances={&dependent};
 LLGhostAvatar::sLiveSourceCounts[R]=1;
 struct CueHistory{LLUUID actor;LLDirectorCast::GazeCueList before,after;};
 auto snapshot=std::make_shared<CueHistory>(CueHistory{A,{{target(R)}},{{target(R)}}});
 auto rewrite_snapshot=''' + callback + r''';
 ALKitHistory history;actorReferenceHistories().insert(&history);
 history.mUndo={{{{rewrite_snapshot}}}};history.mRedo=history.mUndo;
 history.mOpen=std::make_unique<ALKitHistory::Step>(history.mUndo.front());
 auto verify=[&](LLUUID id){
  assert(cast.mCast.front().mGazeTarget.mCastRef==id);
  assert(cast.mCast.front().mEyeGazeTarget.mCastRef==id);
  assert(cast.mCast.front().mGazeCues.front().mTarget.mCastRef==id);
  assert(cast.mSelfGazeTarget.mCastRef==id && cast.mSelfEyeGazeTarget.mCastRef==id);
  assert(cast.mSelfGazeCues.front().mTarget.mCastRef==id);
  assert(mover.mPaths[A].mArrivalTarget==id);
  // Replay both history copies into the live path, as undo/redo applyState does.
  mover.mPaths[A]=mover.mHistory[A].mUndo.back();assert(mover.mPaths[A].mArrivalTarget==id);
  mover.mPaths[A]=mover.mHistory[A].mRedo.back();assert(mover.mPaths[A].mArrivalTarget==id);
  assert(g.mCastTarget==id && g.mParamTransition.mAuthored.mCastRef==id);
  assert(d.mGaze.mCastTarget==id && d.mLastTarget.mCastRef==id);
  assert(d.mCueFromBaseTarget.mCastRef==id && d.mCueFromTarget.mCastRef==id);
  assert(tracker.mStates[projector].subject==id && sCinematicFollowTarget==id);
  assert(rig.mBlobs[0].mAnchor==id && rig.mInstances[0].anchor==id);
  assert(dependent.mAnimationSourceId==id);
  if(id.notNull()) assert(LLGhostAvatar::sLiveSourceCounts.at(id)==1);
  else assert(LLGhostAvatar::sLiveSourceCounts.empty());
  assert(studio.mInstances.front().mLookTargetId==id && studio.mInstances.front().mTurnTargetId==id);
  assert(snapshot->before.front().mTarget.mCastRef==id && snapshot->after.front().mTarget.mCastRef==id);
  assert(mover.mFollows[A].mLeader==id);
  assert(mover.mPaths[other].mArrivalTarget==other);
  if(id==N){
   // Execute the production resolver used by both Head and Eyes gaze.
   assert(cast.resolve(cast.mCast.front().mGazeTarget.mCastRef)==&newClone);
   assert(cast.resolve(cast.mCast.front().mEyeGazeTarget.mCastRef)==&newClone);
   assert(cast.resolve(mover.mPaths[A].mArrivalTarget)==&newClone);
   assert(cast.resolve(tracker.mStates[projector].subject)==&newClone);
  }
  if(id==S) assert(cast.resolve(cast.mCast.front().mGazeTarget.mCastRef)==nullptr);
 };
 gObjectList.objects.erase(R);original.dead=true;
 cast.replaceActorId(R,S,S);verify(S);
 assert(studio.mInstances.front().mSource==S && studio.mCrowdDraft.mSource.mEntityId==S);
 assert(cast.mActiveActorId==S && cast.mSubjectA==S && cast.mSubjectD==S);
 gObjectList.objects[N]=&newClone;studio.mInstances[1].mEntityId=N;
 cast.replaceActorId(S,N,S);verify(N);
 assert(studio.mInstances.front().mSource==N && studio.mCrowdDraft.mSource.mEntityId==N);
 assert(studio.mCrowdDraft.mSource.mSourceId==N);
 assert(cast.mActiveActorId==N && cast.mSubjectA==N && cast.mLookAtCameraIds.count(N));
 // Stable authored references must retire too, even when deletion is live.
 auto& staging=ALAvatarStaging::instance();staging.mEntries[A].mTrackTarget=S;
 auto& dance=ALDanceBall::instance();dance.mRoster={S};dance.mExcluded={S};dance.mRuntime[S]=1;
 studio.mInstances.push_back(look);studio.mInstances.back().mLookTarget=ALGhostStudio::LOOK_TARGET_GHOST;
 studio.mInstances.back().mLookTargetId=S;
 gObjectList.objects.erase(N);newClone.dead=true;
 cast.replaceActorId(N,LLUUID::null,S);verify(LLUUID::null);
 assert(studio.mInstances.front().mSource.isNull() && studio.mInstances.front().mSourceRemoved);
 assert(!studio.mCrowdDraft.mActive);
 assert(cast.mCast.front().mGazeTarget.mMode==T::MOTION);
 assert(cast.mCast.front().mEyeGazeTarget.mMode==T::MOTION);
 assert(snapshot->before.front().mTarget.mMode==T::MOTION);
 assert(mover.mPaths[A].mArrivalFacingMode==1);
 assert(!tracker.mStates[projector].hasSubject && !tracker.mStates[projector].enabled);
 assert(!rig.mBlobs[0].mEnabled && rig.shutdown.count(0));
 assert(dance.mRoster.empty() && dance.mExcluded.empty() && dance.mRuntime.empty());
 assert(tracker.mStates[projector].restorePending);
 assert(staging.mEntries[A].mTrackTarget.isNull() && staging.mEntries[A].mMode==ALAvatarStaging::FacingMode::LOCK);
 assert(!studio.mInstances.back().mKeepFacing && studio.mInstances.back().mLookTargetId.isNull());
 assert(cast.mSubjectA.isNull() && cast.mSubjectD.isNull() && cast.mActiveActorId.isNull());
 // Owner snapshots follow the same clone; deleted owner must become invalid.
 snapshot->actor=R;ALKitHistory::rewriteActorReferences(R,S);assert(snapshot->actor==S);
 ALKitHistory::rewriteActorReferences(S,N);assert(snapshot->actor==N);
 ALKitHistory::rewriteActorReferences(N,{});assert(snapshot->actor.isNull());
 // Loaded destination wins orphaned OWN state but incoming refs still migrate.
 cast.mCast.front().mGazeTarget=target(R);cast.replaceActorId(R,A,S);
 assert(cast.mCast.front().mGazeTarget.mCastRef==A);
 // No old member at all: deletion still clears independent subjects/targets.
 tracker.mStates[projector].subject=R;tracker.mStates[projector].hasSubject=true;
 cast.mSubjectC=R;cast.replaceActorId(R,{},S);
 assert(cast.mSubjectC.isNull() && !tracker.mStates[projector].hasSubject);
 actorReferenceHistories().erase(&history);
 std::cout<<"PASS: production clone reference transaction R -> parked S -> N; Head/Eyes/self/cues/runtime; path undo+redo; tracker/camera/Studio targets; custom history; deletion; collision and unenrolled branches\n";
}
'''
    directory = Path(".tmp/rekey_20261102/contract").resolve()
    directory.mkdir(parents=True, exist_ok=True)
    (directory / "rekey.cpp").write_text(fixture, encoding="utf-8")
    vcvars = next((Path(os.environ["ProgramFiles"]) / "Microsoft Visual Studio").glob(
        "*/Community/VC/Auxiliary/Build/vcvars64.bat"))
    command = directory / "compile.cmd"
    command.write_text(
        f'@echo off\ncall "{vcvars}" >nul\n'
        f'cl /nologo /std:c++17 /EHsc /Fo"{directory / "rekey.obj"}" '
        f'/Fe"{directory / "rekey.exe"}" "{directory / "rekey.cpp"}\nexit /b %ERRORLEVEL%\n', encoding="utf-8")
    subprocess.run(["cmd", "/d", "/c", str(command)], check=True)
    subprocess.run([str(directory / "rekey.exe")], check=True)


if __name__ == "__main__":
    check()
