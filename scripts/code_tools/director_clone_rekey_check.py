"""Execute production clone-reference transaction methods with small native stores.

No viewer build. The existing actor-bar harness separately executes migrateActor
and the real Studio consumer; this fixture exercises the cross-reference pass,
including the collision branch that intentionally skips migrateActor.
"""
from pathlib import Path
import os
import subprocess
from relight_area_policy_check import method as source_method
from native_check_tools import check_directory, vcvars64


def method(path, signature):
    # Keep this historical transaction fixture on its exact reviewed baseline.
    if path.name == "lldirectorcast.cpp" and path.parent.name == "newview":
        from director_actor_bar_check import reverse_director_groups_cast, reverse_rig_no_group_cast
        source = reverse_director_groups_cast(path.read_text(encoding="utf-8"), path.name)
        source = reverse_rig_no_group_cast(source, path.name)
        baseline = check_directory("clone_rekey_baseline") / path.name
        baseline.write_text(source, encoding="utf-8")
        return source_method(baseline, signature)
    return source_method(path, signature)


def check():
    nv = Path("indra/newview")
    # The executed timeout helper is the one the real frame loop calls, and
    # the offset spinner uses the real getFollow result exercised below.
    assert "if (!holdMissingSubject(id, s, now)) continue;" in method(nv / "alprojectortracker.cpp", "void ALProjectorTracker::update(")
    assert "setEnabledIfChanged(mFollowOffset, following);" in (nv / "alpanelpatheditor.cpp").read_text()
    source_rewrite = method(nv / "alpanelghoststudio.cpp", "void ALPanelGhostStudio::rewriteActorReferences(")
    assert "mSourceCombo->setSelectedByValue(LLUUID::null, true)" in source_rewrite
    assert "mSourceSelectionRemoved = false" in source_rewrite and "mAddBtn->setEnabled(true)" in source_rewrite
    removal = method(nv / "alghoststudio.cpp", "void ALGhostStudio::removeInstance(")
    assert "replaceActorId(retiring_id, LLUUID::null, retiring_id)" in removal
    assert "const LLUUID retiring_id = id" in removal
    swap = method(nv / "alghoststudio.cpp", "bool ALGhostStudio::swapSource(")
    assert "onEntityRuntimeReplaced(*inst, replacement->getID(), replacement)" in swap
    assert "cast.remove(" not in swap and "migrateActor(" not in swap
    assert "candidate.mAnimationSource = before.mSource" in swap
    assert "inst->mSource =" not in swap # appearance never redirects motion/path/light provenance
    assert "inst->mAppearanceSource = candidate.mAppearanceSource" in swap
    # The exact same production runtime transaction executes R -> S -> N below;
    # swap uses its live-body R -> N branch, with outside reference checks shared.
    fixture = r'''
#include <algorithm>
#include <array>
#include <cassert>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>
using S32=int; using U32=unsigned; using U64=unsigned long long; using F32=float; using F64=double;
template<class T>T llclamp(T v,T a,T b){return std::clamp(v,a,b);}
template<class T>T llmax(T a,T b){return std::max(a,b);}
struct LLUUID {
 int value=0; bool isNull() const{return !value;} bool notNull() const{return value;}
 void setNull(){value=0;} static const LLUUID null;
 std::string asString() const{return std::to_string(value);}
 bool operator==(LLUUID b) const{return value==b.value;}
 bool operator!=(LLUUID b) const{return !(*this==b);}
 bool operator<(LLUUID b) const{return value<b.value;}
};
const LLUUID LLUUID::null{};
std::ostream& operator<<(std::ostream& s, LLUUID id){return s<<id.value;}
#define LL_WARNS(x) std::cerr
#define LL_INFOS(x) std::cerr
#define LL_ENDL std::endl
struct LLVOAvatar;
struct LLViewerObject {
 virtual ~LLViewerObject()=default;virtual LLVOAvatar* asAvatar(){return nullptr;}
 LLViewerObject* getRootEdit(){return this;}LLVOAvatar* getControlAvatar(){return nullptr;}
};
struct LLVOAvatar:LLViewerObject {
 LLUUID id;bool dead=false;bool ghost=true;
 LLVOAvatar* asAvatar() override{return this;}bool isDead(){return dead;}
 LLUUID getID() const{return id;} bool isControlAvatar(){return false;} bool mIsDummy=false;
 bool isGhostAvatar(){return ghost;}std::string getFullname(){return "clone";}
};
struct ObjectList{std::map<LLUUID,LLViewerObject*> objects;
 LLViewerObject* findObject(LLUUID id){auto it=objects.find(id);return it==objects.end()?nullptr:it->second;}
} gObjectList;
LLVOAvatar self;LLVOAvatar* gAgentAvatarp=&self;
bool isAgentAvatarValid(){return true;}
LLUUID gAgentID{100};
using uuid_vec_t=std::vector<LLUUID>;
struct LLSD {LLUUID id; double number=0; LLSD(LLUUID v={}):id(v){} LLUUID asUUID()const{return id;} double asReal()const{return number;}};
struct Spinner {LLSD value; LLSD getValue(){return value;}};
struct ALPanelPathEditor {LLUUID mActor; Spinner spinner; Spinner* mFollowOffset=&spinner; void onFollowOffsetCommit();};
struct LLPresentationTime{struct Frame{double presentation_time=50;};static Frame& currentFrame(){static Frame f;return f;}};
struct LLActorMover {
 enum {GAZE_TANGENT=0,GAZE_CAST=2,GAZE_OBJECT=4};
 struct GazeTarget {
  enum EMode{MOTION=0,CAMERA=1,CAST_MEMBER=2,OBJECT=4};
  EMode mMode=MOTION; LLUUID mCastRef,mObjectRef;
 };
 struct Path {LLUUID mArrivalTarget; int mArrivalFacingMode=0;};
 struct EditHistory{std::vector<Path> mUndo,mRedo;};
 struct Gaze {LLUUID mCastTarget,mObjectTarget; int mTarget=0;int mGazeMotor=0;U32 mHotGeneration=0;
  struct {GazeTarget mAuthored;} mParamTransition;
  bool invalidated=false;
 };
 struct DirectorGaze{bool mRekeyMotorPending=false;Gaze mGaze; GazeTarget mLastTarget,mCueFromBaseTarget,mCueFromTarget;};
 struct Follow{LLUUID mLeader;S32 mMode=0;F32 mOffset=0;};
 struct Move{bool mPathPoseEvaluated=true,mIsPath=true;};std::map<LLUUID,Move> mMoves;
 int stops=0;void stop(LLUUID id){++stops;mMoves.erase(id);}
 bool hasWalkablePath(LLUUID id){return mPaths.count(id)!=0;}
 bool wouldFollowCycle(const LLUUID&,const LLUUID&)const;
 bool setFollow(const LLUUID&,const LLUUID&,S32,F32);
 bool getFollow(const LLUUID&,LLUUID&,S32&,F32&)const;
 void clearFollow(const LLUUID&);
 std::map<LLUUID,Path> mPaths;
 std::map<LLUUID,EditHistory> mHistory;
 std::map<LLUUID,Gaze> mGazes;
 std::map<LLUUID,DirectorGaze> mDirectorGazes;
 std::map<LLUUID,Follow> mFollows;
 U64 mGazeRevision=0;
 static LLActorMover& instance(){static LLActorMover m;return m;}
 static bool rewriteGazeTarget(GazeTarget&,const LLUUID&,const LLUUID&);
 struct HotGaze{struct State{double mRelease=-1;void release(double t){mRelease=t;}}mState;DirectorGaze mPaint;GazeTarget mTarget;};
 std::map<LLUUID,HotGaze> mHotGazes;
 struct{void migrate(const LLUUID&,const LLUUID&){} template<class F>void forEach(F&&){}} mParkedHotGazes; // [ClearAnim] store stand-in; its rules are covered by alghostclearanim_test
 void restoreDirectorLookAtPose(LLVOAvatar*,DirectorGaze&){}
 void rewriteHotGazeReferences(const LLUUID&,const LLUUID&);
 bool rewriteActorReferences(const LLUUID&,const LLUUID&);
 static void invalidateGazeCache(Gaze& g,bool){g.invalidated=true;}
 void clearDirectorLookAtRuntime(LLUUID){}
 void dropActor(LLUUID id){mPaths.erase(id);mHistory.erase(id);mGazes.erase(id);mFollows.erase(id);}
 // Ownership migration itself is executed from production in actor_bar_check.
 void migrateActor(LLUUID old,LLUUID next){
  if(mPaths.count(old)){mPaths[next]=mPaths.at(old);mPaths.erase(old);}
  if(mHistory.count(old)){mHistory[next]=mHistory.at(old);mHistory.erase(old);}
 }
};
namespace ALGazeHot {
 struct Slot{LLActorMover::GazeTarget mTarget;LLUUID mWho;bool mHasTarget=true;std::string mTargetName;};
 using Slots=std::array<Slot,4>;
 bool rewrite(Slots&,const LLUUID&,const LLUUID&);
}
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
  bool enabled=true,restorePending=false,subjectPending=false;double missingSince=0.;std::string status;};
 std::map<LLUUID,State> mStates;std::set<LLUUID> mActive;U64 mRevision=0;
 bool restoreSucceeds=true;
 bool restore(LLViewerObject*,State& s){s.restorePending=!restoreSucceeds;return restoreSucceeds;}
 bool holdMissingSubject(const LLUUID&,State&,F64);
 static ALProjectorTracker& instance(){static ALProjectorTracker s;return s;}
 void rewriteActorReferences(const LLUUID&,const LLUUID&);
 void stop(const LLUUID&,const std::string&);
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
 bool isPrivateOverlayRuntime(LLUUID)const{return false;}
 enum ELookTarget{LOOK_TARGET_CAMERA,LOOK_TARGET_ME,LOOK_TARGET_ACTOR,LOOK_TARGET_GHOST,LOOK_TARGET_POINT};
 enum{TURN_MODE_OFF,TURN_MODE_ONCE};
 struct Instance{LLUUID mId,mSource,mEntityId,mAnimationSource,mAppearanceSource;bool mSourceRemoved=false,mHasAnimationSource=false,mAnimationSourceRemoved=false,mAppearanceSourceRemoved=false;
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

struct ALPanelLensGaze{static void rewriteActorReferences(LLUUID,LLUUID){}};
struct ALClientHeight {static ALClientHeight& instance(){static ALClientHeight s;return s;} void reset(LLUUID){}};
struct ALAvatarLightResponse {
 std::map<LLUUID,int> mEntries,mPersistExtras;std::set<LLUUID> mPersistSuppressed;int changes=0;
 void markChanged(LLUUID){++changes;}
 static bool instanceExists(){return true;}
 static ALAvatarLightResponse& instance(){static ALAvatarLightResponse s;return s;}
 void retireClone(const LLUUID&);
};
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
 struct CastMember{LLUUID mId;std::string mLastName;GazeTarget mGazeTarget,mEyeGazeTarget;GazeCueList mGazeCues;bool mCloneFading=false;};
 std::vector<CastMember> mCast;std::vector<LLUUID> mIds;
 ALGazeHot::Slots mHotGazeSlots;U64 mHotGazeRevision=0;
 GazeTarget mSelfGazeTarget,mSelfEyeGazeTarget;GazeCueList mSelfGazeCues;
 struct Start{LLUUID mId;};std::vector<Start> mPendingStarts;
 LLUUID mSubjectA,mSubjectB,mSubjectC,mSubjectD,mActiveActorId;
 std::set<LLUUID> mLookAtCameraIds;U64 mCastRevision=0,mGazeCueRevision=0,mSubjectRevision=0;
 static LLDirectorCast& instance(){static LLDirectorCast c;return c;}
 void setGazeCues(LLUUID,const GazeCueList&){}
 bool rewriteActorReferences(const LLUUID&,const LLUUID&);
 void replaceActorId(const LLUUID&,const LLUUID&,const LLUUID& = LLUUID::null);
 static bool isSelf(LLUUID id){return id.isNull();}
 static LLUUID canonicalActorId(LLUUID id){return id;}
 LLVOAvatar* resolve(const LLUUID&);
 CastMember* getMember(LLUUID id){for(auto& m:mCast)if(m.mId==id)return &m;return nullptr;}
 bool contains(LLUUID id) const{return std::any_of(mCast.begin(),mCast.end(),[&](auto& m){return m.mId==id;});}
 void remove(LLUUID id){mCast.erase(std::remove_if(mCast.begin(),mCast.end(),[&](auto& m){return m.mId==id;}),mCast.end());}
 std::function<void()> mActiveActorChanged=[]{};
 LLUUID getActiveActorId()const{return mActiveActorId;}
 static LLUUID runtimeActorId(LLUUID id){return id;}
 LLUUID getSteppedActorId()const{return mActiveActorId;}
 void setActiveActorId(const LLUUID&);
 void cancelPendingStart(LLUUID id){mPendingStarts.erase(std::remove_if(mPendingStarts.begin(),mPendingStarts.end(),[&](auto& s){return s.mId==id;}),mPendingStarts.end());}
 int sceneTouches=0; void rebuildMemberIndex(){} void touchScene(){++sceneTouches;}
};
'''
    fixture += method(nv / 'llactormover.cpp', 'void LLActorMover::rewriteHotGazeReferences(') + '\n'
    fixture += 'namespace ALGazeHot {\n' + method(nv / 'algazehotslots.h', 'inline bool rewrite(') + '\n}\n'
    fixture += r'''
namespace ALAvatarSubjectPicker {
std::vector<std::weak_ptr<LLUUID>> pendingSubjects;
'''
    fixture += method(nv / 'alavatarsubjectpicker.cpp', 'void prunePendingSubjects(') + '\n'
    fixture += method(nv / 'alavatarsubjectpicker.cpp', 'void rewriteActorReferences(') + '\n}\n'
    fixture += r'''
struct LLScrollListItem {LLSD value;LLSD getValue()const{return value;}};
struct Combo {
 std::vector<LLScrollListItem> rows; LLUUID selected;
 std::vector<LLScrollListItem*> getAllData(){std::vector<LLScrollListItem*> result;for(auto& r:rows)result.push_back(&r);return result;}
 void remove(S32 i){rows.erase(rows.begin()+i);}
};
struct CastList {
 uuid_vec_t selected;void selectMultiple(const uuid_vec_t& ids){selected=ids;}
 void deselectAllItems(){selected.clear();}void selectByID(LLUUID id){selected={id};}
};
struct ALKitShell {
 uuid_vec_t mCurrentCastSelection; int whoCommits=0; LLUUID lightRigWho,gazeWho,steppedWho,styleWho;
 void mCastSelection(const uuid_vec_t& ids){++whoCommits;lightRigWho=gazeWho=steppedWho=styleWho=ids.empty()?LLUUID{}:ids.front();}
'''
    fixture += method(nv / 'alkitshell.h', 'void                        setCastSelection(') + '\n'
    fixture += method(nv / 'alkitshell.h', 'const uuid_vec_t&           castSelection(') + '\n'
    fixture += method(nv / 'alkitshell.h', 'void                        cacheCastSelection(') + '\n};\n'
    fixture += r'''
struct LLFloaterDirector {
 static LLFloaterDirector* current;
 LLUUID mAnimSpeedSelection,mPrimaryCastActor,mCastRightClickId,mGroupShownFor,
  mActorStyleShownFor,mActorStyleLastCastSelection,mLocomotionEditorMember,mAnimAvatarId;
 std::string mAnimSpeedContextLabel;struct Clock{void forceDue(){}}mAnimSpeedClock;
 Combo speed;Combo* mAnimSpeedAvatar=&speed;CastList list;CastList* mCastList=&list;
 ALKitShell shell;ALKitShell* mShell=&shell;int refreshes=0;
 uuid_vec_t selectedCastIds(){return list.selected;}
 // Model the widget boundary: roster rebuild removes retired selected rows.
 void refreshCastList(){++refreshes;auto& cast=LLDirectorCast::instance();
  list.selected.erase(std::remove_if(list.selected.begin(),list.selected.end(),
   [&](LLUUID id){return !cast.contains(id);}),list.selected.end());}
 bool mSyncingActiveActor=false,mActorStyleHadTarget=false,mActorStyleTabDirty=false,mAnimateTabDirty=false;
 LLUUID firstSelectedCastId()const{return list.selected.empty()?LLUUID{}:list.selected.front();}
 void retargetPathFromCastSelection(){}void refreshGazeSelection(const uuid_vec_t&){}
 void syncActiveActor();
 void rebuildAnimSpeedAvatars(){speed.rows.push_back({mAnimSpeedSelection});speed.selected=mAnimSpeedSelection;}
 static void rewriteActorReferences(const LLUUID&,const LLUUID&);
};
LLFloaterDirector* LLFloaterDirector::current=nullptr;
struct LLFloaterReg{template<class T>static T* findTypedInstance(const char*){return T::current;}};
'''
    for file, signature in (
        ("llactormover.cpp", "bool LLActorMover::rewriteGazeTarget("),
        ("llactormover.cpp", "bool LLActorMover::rewriteActorReferences("),
        ("llactormover.cpp", "LLUUID path_key("),
        ("llactormover.cpp", "bool LLActorMover::wouldFollowCycle("),
        ("llactormover.cpp", "bool LLActorMover::setFollow("),
        ("llactormover.cpp", "void LLActorMover::clearFollow("),
        ("llactormover.cpp", "bool LLActorMover::getFollow("),
        ("alpanelpatheditor.cpp", "void ALPanelPathEditor::onFollowOffsetCommit("),
        ("llfloaterdirector.cpp", "void LLFloaterDirector::rewriteActorReferences("),
        ("llfloaterdirector.cpp", "void LLFloaterDirector::syncActiveActor("),
        ("lldirectorcast.cpp", "void LLDirectorCast::setActiveActorId("),
        ("alkithistory.cpp", "void ALKitHistory::rewriteActorReferences("),
        ("alprojectortracker.cpp", "void ALProjectorTracker::rewriteActorReferences("),
        ("alprojectortracker.cpp", "void ALProjectorTracker::stop("),
        ("alavatarlightresponse.cpp", "void ALAvatarLightResponse::retireClone("),
        ("alprojectortracker.cpp", "bool ALProjectorTracker::holdMissingSubject("),
        ("llcinematiccamera.cpp", "void LLCinematicCamera::onRuntimeTargetReplaced("),
        ("llghostavatar.cpp", "void LLGhostAvatar::releaseSourceReference("),
        ("llghostavatar.cpp", "void LLGhostAvatar::rewriteAnimationSourceReferences("),
        ("alghoststudio.cpp", "void ALGhostStudio::rewriteActorReferences("),
        ("alavatarstaging.cpp", "void ALAvatarStaging::removeActorReferences("),
        ("aldanceball.cpp", "void ALDanceBall::removeActorReferences("),
        ("alcinelightrigmanager.cpp", "void ALCineLightRigManager::rewriteActorReferences("),
        ("lldirectorcast.cpp", "bool LLDirectorCast::rewriteActorReferences("),
        ("lldirectorcast.cpp", "void LLDirectorCast::replaceActorId("),
        ("lldirectorcast.cpp", "LLVOAvatar* LLDirectorCast::resolve("),
    ):
        fixture += method(nv / file, signature) + "\n"
    # Execute the exact production custom-history callback, not a model of it.
    callback = method(nv / "alpanelgazecues.cpp", "[snapshot](const LLUUID& old_id")
    validate = method(nv / "alpanelgazecues.cpp", "[snapshot](bool, std::string& reason)")
    replay = method(nv / "alpanelgazecues.cpp", "[snapshot, validate](bool redo")
    choose_callback = method(nv / "alavatarsubjectpicker.cpp", "[handle, picked, subject]()")
    fixture += r'''
int main(){
 const LLUUID A{1},R{2},S{3},N{4},other{9},projector{20};
 LLVOAvatar original,newClone;original.id=R;newClone.id=N;
 gObjectList.objects[R]=&original;
 using T=LLActorMover::GazeTarget;
 auto target=[](LLUUID id){T t;t.mMode=T::CAST_MEMBER;t.mCastRef=id;return t;};
 self.id=gAgentID;
 LLDirectorCast& cast=LLDirectorCast::instance();
 LLFloaterDirector director;LLFloaterDirector::current=&director;
 cast.mActiveActorChanged=[&]{director.syncActiveActor();};
 director.list.selected={A};director.shell.mCurrentCastSelection={A};
 director.shell.lightRigWho=director.shell.gazeWho=director.shell.steppedWho=director.shell.styleWho=gAgentID;
 director.mAnimSpeedSelection=A;director.speed.selected=A;director.speed.rows={{{A}},{{R}}};
 auto pendingSubject=std::make_shared<LLUUID>(R);
 ALAvatarSubjectPicker::pendingSubjects.push_back(pendingSubject);
 auto subject=pendingSubject;struct Handle{bool get()const{return true;}}handle;
 LLUUID chosen;int choiceCommits=0;auto picked=[&](LLUUID id){chosen=id;++choiceCommits;};
 auto invokeChoice=''' + choose_callback + r''';
 ALPanelPathEditor pathEditor;pathEditor.mActor=A;pathEditor.spinner.value.number=7.;
 LLDirectorCast::CastMember a;a.mId=A;a.mGazeTarget=target(R);a.mEyeGazeTarget=target(R);
 a.mGazeCues={{target(R)}};
 LLDirectorCast::CastMember clone;clone.mId=R;clone.mGazeTarget=target(A);
 cast.mCast={a,clone};cast.mSelfGazeTarget=target(R);cast.mSelfEyeGazeTarget=target(R);
 cast.mSelfGazeCues={{target(R)}};
 cast.mSubjectA=R;cast.mSubjectB=R;cast.mSubjectC=R;cast.mSubjectD=R;
 cast.mActiveActorId=A;cast.mLookAtCameraIds={R};cast.mPendingStarts={{R}};
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
 studio.mInstances.front().mHasAnimationSource=true;studio.mInstances.front().mAnimationSource=R;studio.mInstances.front().mAppearanceSource=R;
 ALGhostStudio::Instance source;source.mId=S;studio.mInstances.push_back(source);
 studio.mCrowdDraft.mActive=true;studio.mCrowdDraft.mSource.mSourceId=R;
 studio.mCrowdDraft.mSource.mEntityId=R;
 LLGhostAvatar dependent;dependent.mAnimationSourceId=R;LLCharacter::sInstances={&dependent};
 LLGhostAvatar::sLiveSourceCounts[R]=1;
 struct CueHistory{LLUUID actor;LLDirectorCast::GazeCueList before,after;};
 auto snapshot=std::make_shared<CueHistory>(CueHistory{A,{{target(R)}},{{target(R)}}});
 auto validate=''' + validate + r''';
 auto replay=''' + replay + r''';
 auto rewrite_snapshot=''' + callback + r''';
 ALKitHistory history;actorReferenceHistories().insert(&history);
 history.mUndo={{{{rewrite_snapshot}}}};history.mRedo=history.mUndo;
 history.mOpen=std::make_unique<ALKitHistory::Step>(history.mUndo.front());
 auto& liveHot=LLActorMover::instance().mHotGazes[R];liveHot.mTarget=target(R);
 liveHot.mPaint.mGaze.mGazeMotor=73;liveHot.mPaint.mGaze.mHotGeneration=8;
 cast.mHotGazeSlots[0].mWho=R;
 cast.mHotGazeSlots[0].mTarget=target(R);
 auto verify=[&](LLUUID id){
  auto& live=LLActorMover::instance().mHotGazes;
  if(id.notNull()){assert(live.count(id)==1);assert(live.at(id).mTarget.mCastRef==id);
    assert(live.at(id).mPaint.mGaze.mGazeMotor==73 && live.at(id).mPaint.mGaze.mHotGeneration==8);}
  else assert(live.empty());
  assert(cast.mHotGazeSlots[0].mWho==id && cast.mHotGazeSlots[0].mTarget.mCastRef==id);
  if(id.isNull()) assert(!cast.mHotGazeSlots[0].mHasTarget);
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
  assert(studio.mInstances.front().mAnimationSource==id && studio.mInstances.front().mAppearanceSource==id);
  assert(studio.mInstances.front().mLookTargetId==id && studio.mInstances.front().mTurnTargetId==id);
  assert(snapshot->before.front().mTarget.mCastRef==id && snapshot->after.front().mTarget.mCastRef==id);
  if(id.notNull()) assert(mover.mFollows.at(A).mLeader==id);
  else assert(mover.mFollows.count(A)==0);
  assert(*pendingSubject==id);
  const int beforeChoice=choiceCommits;invokeChoice();
  if(id.notNull()) assert(chosen==id && choiceCommits==beforeChoice+1);
  else assert(choiceCommits==beforeChoice);
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
 // Swap-source uses the live R -> N branch of this same production transaction.
 // Prove it directly before also exercising parking/recovery/deletion.
 gObjectList.objects[N]=&newClone;studio.mInstances[1].mEntityId=N;
 cast.replaceActorId(R,N,S);verify(N);
 cast.replaceActorId(N,R,S);verify(R);
 studio.mInstances[1].mEntityId.setNull();
 gObjectList.objects.erase(N);
 gObjectList.objects.erase(R);original.dead=true;
 cast.replaceActorId(R,S,S);verify(S);
 // An unrelated parked clone must never publish the shell signal which commits
 // the independent Light Rig/Gaze/Stepped/Actor Style Who adapters.
 assert(director.shell.whoCommits==0 && director.refreshes==0);
 assert(director.shell.lightRigWho==gAgentID && director.shell.gazeWho==gAgentID);
 assert(director.shell.steppedWho==gAgentID && director.shell.styleWho==gAgentID);
 assert(director.shell.mCurrentCastSelection==uuid_vec_t{A});
 assert(director.mAnimSpeedSelection==A && director.speed.selected==A);
 for(auto* row:director.speed.getAllData())assert(row->getValue().asUUID()!=R);
 // Repeat parking with the clone active/selected. The production setter emits
 // synchronously, so syncActiveActor has selected S before the rewrite runs.
 cast.replaceActorId(S,R,S);verify(R);
 cast.setActiveActorId(R);director.shell.cacheCastSelection({R});
 const int beforePark=director.refreshes;
 cast.replaceActorId(R,S,S);verify(S);
 assert(director.refreshes>beforePark && director.list.selected==uuid_vec_t{S});
 assert(director.shell.castSelection()==uuid_vec_t{S});
 assert(director.shell.whoCommits==0 && director.shell.lightRigWho==gAgentID);
 auto& track=tracker.mStates.at(projector);tracker.mActive.insert(projector);
 assert(track.subjectPending && track.enabled);
 assert(tracker.holdMissingSubject(projector,track,10.));
 assert(tracker.holdMissingSubject(projector,track,100.));
 assert(track.enabled && tracker.mActive.count(projector));
 // Recovery also rewrites a selected clone without firing any Who adapter.
 assert(studio.mInstances.front().mSource==S && studio.mCrowdDraft.mSource.mEntityId==R);
 assert(cast.mActiveActorId==S && cast.mSubjectA==S && cast.mSubjectD==S);
 gObjectList.objects[N]=&newClone;studio.mInstances[1].mEntityId=N;
 cast.replaceActorId(S,N,S);verify(N);
 assert(director.list.selected==uuid_vec_t{N} && director.shell.mCurrentCastSelection==uuid_vec_t{N});
 assert(director.shell.whoCommits==0 && director.shell.lightRigWho==gAgentID);
 // Lazy connectWho / unpin both apply shell.castSelection(). Model that
 // boundary with the current cine_rig_slot Subject A lookup: retired IDs fail.
 auto applyLightRigWho=[&](const uuid_vec_t& ids){
  assert(ids.size()==1 && ids.front()!=R && ids.front()!=S);
  assert(ids.front()==cast.mSubjectA && cast.resolve(ids.front())==&newClone);
  director.shell.lightRigWho=ids.front();
 };
 applyLightRigWho(director.shell.castSelection()); // lazy page connectWho
 director.shell.lightRigWho=gAgentID; // pinned to another slot
 applyLightRigWho(director.shell.castSelection()); // Follow Cast / unpin
 assert(director.shell.lightRigWho==N && director.shell.whoCommits==0);
 assert(!track.subjectPending && track.enabled && tracker.mActive.count(projector));
 pathEditor.onFollowOffsetCommit();assert(mover.mFollows.at(A).mLeader==N);
 // With no own path, deletion must take clearFollow's actual stop path.
 mover.mFollows[{77}].mLeader=N;mover.mMoves[{77}]={};
 assert(studio.mInstances.front().mSource==N && studio.mCrowdDraft.mSource.mEntityId==R);
 assert(studio.mCrowdDraft.mSource.mSourceId==N);
 assert(cast.mActiveActorId==N && cast.mSubjectA==N && cast.mLookAtCameraIds.count(N));
 // Stable authored references must retire too, even when deletion is live.
 auto& staging=ALAvatarStaging::instance();staging.mEntries[A].mTrackTarget=S;
 auto& dance=ALDanceBall::instance();dance.mRoster={S};dance.mExcluded={S};dance.mRuntime[S]=1;
 studio.mInstances.push_back(look);studio.mInstances.back().mLookTarget=ALGhostStudio::LOOK_TARGET_GHOST;
 studio.mInstances.back().mLookTargetId=S;
 gObjectList.objects.erase(N);newClone.dead=true;
 tracker.restoreSucceeds=false;
 cast.replaceActorId(N,LLUUID::null,S);verify(LLUUID::null);
 assert(director.list.selected.empty() && director.shell.castSelection().empty());
 assert(director.shell.whoCommits==0);
 LLUUID leader;S32 mode=0;F32 offset=0;
 assert(!mover.getFollow(A,leader,mode,offset));
 pathEditor.onFollowOffsetCommit();assert(!mover.getFollow(A,leader,mode,offset));
 assert(mover.stops==1 && !mover.mMoves.count({77}));
 assert(!track.enabled && tracker.mActive.count(projector) && track.restorePending);
 // A normal missing resident still hits the same timeout and real stop().
 auto& absent=tracker.mStates[{21}];absent.subject={88};tracker.mActive.insert({21});
 tracker.restoreSucceeds=true;
 assert(tracker.holdMissingSubject({21},absent,10.));
 assert(!tracker.holdMissingSubject({21},absent,13.));
 assert(!absent.enabled && !tracker.mActive.count({21}) && absent.status=="Subject unavailable");
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
 std::string reason;assert(!validate(false,reason) && !replay(false,reason));
 // A removed non-clone owner remains valid during group preparation and
 // fails only when its custom entry is actually replayed, as before.
 snapshot->actor={88};assert(validate(false,reason));assert(!replay(false,reason));
 snapshot->actor=A;assert(validate(false,reason) && replay(false,reason));
 // Loaded destination wins orphaned OWN state but incoming refs still migrate.
 cast.mCast.front().mGazeTarget=target(R);cast.replaceActorId(R,A,S);
 assert(cast.mCast.front().mGazeTarget.mCastRef==A);
 // No old member at all: deletion still clears independent subjects/targets.
 tracker.mStates[projector].subject=R;tracker.mStates[projector].hasSubject=true;
 cast.mSubjectC=R;cast.replaceActorId(R,{},S);
 assert(cast.mSubjectC.isNull() && !tracker.mStates[projector].hasSubject);
 // An unenrolled and unreferenced deletion must leave scene cleanliness alone.
 const int touches=cast.sceneTouches;
 cast.replaceActorId({1234},{},{1235});assert(cast.sceneTouches==touches);
 auto& response=ALAvatarLightResponse::instance();
 for(int i=2000;i<2100;++i){response.mEntries[{i}]=1;response.mPersistSuppressed.insert({i});cast.replaceActorId({i},{},{i});}
 assert(response.mEntries.empty() && response.mPersistSuppressed.empty());
 assert(cast.sceneTouches==touches);
 actorReferenceHistories().erase(&history);
 std::cout<<"PASS: production clone reference transaction R -> parked S -> N; Head/Eyes/self/cues/runtime/Hot-slot Who+Target; path undo+redo; tracker/camera/Studio targets; custom history; deletion; collision and unenrolled branches\n";
}
'''
    directory = check_directory("clone_rekey")
    directory.mkdir(parents=True, exist_ok=True)
    (directory / "rekey.cpp").write_text(fixture, encoding="utf-8")
    vcvars = vcvars64()
    command = directory / "compile.cmd"
    command.write_text(
        f'@echo off\ncall "{vcvars}" >nul\n'
        f'cl /nologo /std:c++17 /EHsc /Fo"{directory / "rekey.obj"}" '
        f'/Fe"{directory / "rekey.exe"}" "{directory / "rekey.cpp"}"\nexit /b %ERRORLEVEL%\n', encoding="utf-8")
    subprocess.run(["cmd", "/d", "/c", str(command)], check=True)
    subprocess.run([str(directory / "rekey.exe")], check=True)


if __name__ == "__main__":
    check()
