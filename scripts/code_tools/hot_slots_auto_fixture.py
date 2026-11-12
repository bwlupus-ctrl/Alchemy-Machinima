"""Exercise production Auto latches against owned mover state, original dispatch and planted negatives."""
import subprocess
from hot_combo_refresh_fixture import block
from native_check_tools import ROOT, check_directory, compile_run, viewer_native_flags


def fixture():
    def read(name): return (ROOT / 'indra/newview' / name).read_text(encoding='utf-8')
    header = read('algazehotslots.h').replace('#include "llactormover.h"', '')
    mover, cast, ui = read('llactormover.cpp'), read('lldirectorcast.cpp'), read('alpanellensgaze.cpp')
    assert 'groupEvent.connect(&hotGroupChanged)' in block(ui, 'void al_gaze_register_random_perf_idle()')
    source = r'''
#include "linden_common.h"
#include "v3math.h"
#include "v3dmath.h"
#include "llsdutil_math.h"
#include "algazehot.h"
#include "alghostclearanim.h"
#include <algorithm>
#include <array>
#include <iostream>
#include <functional>
#include <set>
#include <map>
F64 hotNow=0;std::string scenario;
struct LLPresentationTime {struct Frame{F64 presentation_time;};static Frame currentFrame(){return {hotNow};}};
struct LLVOAvatar {LLUUID id;const LLUUID& getID()const{return id;}void invalidateSkinPalettesAfterOverlay(){}};
struct LLActorMover {
 struct GazeTarget {enum EMode{MOTION,CAMERA,CAST_MEMBER,FIXED_POINT,OBJECT};EMode mMode=MOTION;
 LLUUID mCastRef,mObjectRef;LLVector3d mFixedPoint;bool mPickedSpot=false;LLVector3 mObjectOffset,mPointNudge;std::string mObjectJoint;F32 mIntensityOverride=-1.f;};
 enum class HotGazeSlotState {NONE,ACTIVE,RETURNING,FINISHED};
 struct HotGazeSlotInfo {HotGazeSlotState state=HotGazeSlotState::NONE;F64 start=-1,release=-1;U32 serial=0;};
 static inline bool exists=true;static inline int accesses=0;
 static bool instanceExists(){return exists;}static LLActorMover& instance(){++accesses;static LLActorMover m;return m;}
 struct HotGaze {ALGazeHot::State mState;GazeTarget mTarget;struct{bool mRekeyMotorPending=false;struct{U32 mHotGeneration=0;int mGazeMotor=0;}mGaze;}mPaint;};std::map<LLUUID,HotGaze> mHotGazes;
 ALGhostClearAnim::ParkedStore<LLUUID,HotGaze> mParkedHotGazes;
 std::set<LLUUID> unresolved,suppressed,copies;bool ready=true;int resolves=0,fires=0,releases=0,toggles=0;mutable int queries=0;
 bool resolveHotGazeTarget(LLVOAvatar* body,const GazeTarget& target,LLVector3d&){++resolves;return body&&ready&&!(target.mMode==GazeTarget::CAST_MEMBER&&unresolved.count(target.mCastRef));}
 HotGazeSlotInfo hotGazeSlotState(const LLUUID&,S32)const;
 void releaseHotGazeSlot(S32);
 void hotGazeInput(S32,bool);
 U32 parkHotGaze(const LLUUID&);void unparkHotGaze(U32,const LLUUID&);void discardParkedHotGaze(U32);
 void rewriteHotGazeReferences(const LLUUID&,const LLUUID&);
 static bool rewriteGazeTarget(GazeTarget& t,const LLUUID& old_id,const LLUUID& new_id){bool changed=t.mCastRef==old_id||t.mObjectRef==old_id;if(t.mCastRef==old_id)t.mCastRef=new_id;if(t.mObjectRef==old_id)t.mObjectRef=new_id;return changed;}
 template<class Paint>void restoreDirectorLookAtPose(LLVOAvatar*,Paint&){}
 void clearHotGazes(){mHotGazes.clear();mParkedHotGazes.clear();copies.clear();}
 void dropActor(const LLUUID& id){mHotGazes.erase(id);}
 void onAvatarRemoved(const LLUUID& id){mHotGazes.erase(id);}
 void fireHotGaze(const LLUUID& id,S32 slot,const GazeTarget& target,F32 snap,F32 dwell,F32 back,bool hold,bool stay,F32,F32,F32){
  LLVector3d point;LLVOAvatar body{id};if(!resolveHotGazeTarget(unresolved.count(id)?nullptr:&body,target,point)||suppressed.count(id))return;
  auto& state=mHotGazes[id].mState;
  if(!hold&&state.mSlot==slot&&!state.finished(hotNow)&&!state.returning(hotNow)){++toggles;state.release(hotNow);return;}
  ++fires;state.fire(slot,hotNow,snap,dwell,back,hold,stay);++mHotGazes[id].mPaint.mGaze.mHotGeneration;mHotGazes[id].mTarget=target;
 }
};
''' + block(mover, 'LLActorMover::HotGazeSlotInfo LLActorMover::hotGazeSlotState(').replace('{\n', '{\n    ++queries; // fixture-only read census\n', 1) + '\n'
    source += block(mover, 'void LLActorMover::releaseHotGazeSlot(').replace('{\n', '{\n    ++releases; // fixture-only release census\n', 1) + '\n' + block(mover, 'void LLActorMover::hotGazeInput(') + '\n' + header
    source += r'''
struct LLFrameTimer {static inline float now=0;float start=now;float getElapsedTimeF32(){return now-start;}void reset(){start=now;}};
''' + block(read('aluirefresh.h'), 'struct ALLiveReadoutClock\n') + ';\n' + r'''
struct LLApp {static inline bool exiting=false;static bool isExiting(){return exiting;}};
namespace ALDirectorGroups {std::string resolveGroupName(const std::vector<std::string>&,const std::string& s){return s;}}
namespace ALSteppedAnim {void renameGroupRate(std::map<std::string,S32>&,const std::string&,const std::string&){}}
struct LLDirectorCast {
 static inline bool exists=true;static inline int accesses=0;static bool instanceExists(){return exists;}
 static LLDirectorCast& instance(){++accesses;static LLDirectorCast c;return c;}
 struct CastMember{LLUUID mId;std::string mGroup;};std::vector<CastMember> mCast;
 std::map<std::string,F32> mGroupDelays;std::map<std::string,S32> mGroupRates;
 CastMember* getStoredMember(const LLUUID& id){for(auto& m:mCast)if(m.mId==id)return &m;return nullptr;}
 std::vector<std::string> getGroupNames(){return {"Group","Other","Renamed"};}
 void pruneGroupData(const std::string&){}
 struct GroupEvent {enum Type{MEMBERS,RENAMED,DISSOLVED} type;std::string oldName,newName;};
 std::function<void(const GroupEvent&)> onChanged;void changedGroups(const GroupEvent& event){if(onChanged)onChanged(event);}
 void addToGroup(const uuid_vec_t&,const std::string&);bool renameGroup(const std::string&,const std::string&);
 ALGazeHot::Slots mHotGazeSlots;U64 mHotGazeRevision=0;int whoCalls=0;std::map<LLUUID,LLVOAvatar> bodies;
 const auto& getHotGazeSlots(){return mHotGazeSlots;}
 LLVOAvatar* resolve(const LLUUID& id,bool=false){if(id.isNull()||LLActorMover::instance().unresolved.count(id))return nullptr;bodies[id].id=id;return &bodies[id];}
 uuid_vec_t membersInGroup(const std::string& name){++whoCalls;uuid_vec_t ids;for(const auto& m:mCast)if(m.mGroup==name)ids.push_back(m.mId);return ids;}
 LLUUID canonicalActorId(const LLUUID& id){return id;}void touchScene(){}
 void setHotGazeSlot(S32,const ALGazeHot::Slot&);void hotGazeInput(S32,bool);void setHotGazeTargetName(S32,const std::string&);
};
'''
    for text, signature in [(mover,'U32 LLActorMover::parkHotGaze('),(mover,'void LLActorMover::unparkHotGaze('),(mover,'void LLActorMover::discardParkedHotGaze('),(mover,'void LLActorMover::rewriteHotGazeReferences('),
                            (cast,'void LLDirectorCast::setHotGazeTargetName('),(cast,'void LLDirectorCast::addToGroup('),(cast,'bool LLDirectorCast::renameGroup('),(cast,'void LLDirectorCast::setHotGazeSlot('),(cast,'void LLDirectorCast::hotGazeInput('),
                            (ui,'ALGazeHot::AutoSlots& hotAutoSlots()'),(ui,'void updateHotAuto('),
                            (ui,'void hotGroupChanged('),(ui,'void setHotSlot('),(ui,'void al_gaze_hot_note_manual(')]:
        source += block(text,signature) + '\n'
    source += r'''
struct ALObjectPropertiesCache {
 struct Properties{std::string mName;};std::map<LLUUID,Properties> entries;std::function<void(const LLUUID&)> changed;
 static ALObjectPropertiesCache& instance(){static ALObjectPropertiesCache cache;return cache;}
 const Properties* get(const LLUUID& id){auto it=entries.find(id);return it==entries.end()?nullptr:&it->second;}
 auto setChangeCallback(std::function<void(const LLUUID&)> fn){changed=fn;return fn;}
 void publish(const LLUUID& id,const std::string& name){entries[id].mName=name;changed(id);}
};
void installPropertiesCallback(){std::function<void(const LLUUID&)> mHotPropertiesConnection;
''' + block(ui, 'mHotPropertiesConnection = ALObjectPropertiesCache::instance().setChangeCallback(') + ');\n}\n'
    source += r'''
void require(bool ok,const char* why){if(!ok){std::cerr<<scenario<<": "<<why<<"\n";std::exit(2);}}
int main(){
 using namespace ALGazeHot;using State=LLActorMover::HotGazeSlotState;
 const LLUUID a("00000000-0000-0000-0000-000000000001"),b("00000000-0000-0000-0000-000000000002"),n("00000000-0000-0000-0000-000000000003");
 Slots blank;require(blank[0].mAuto&&blank[0].mStay&&!blank[0].mHold,"new defaults");
 LLSD legacy=LLSD::emptyArray(),old;old["mode"]=1;old["hold"]=false;legacy.append(old);old["hold"]=true;legacy.append(old);
 auto loaded=readSlots(legacy);require(!loaded[0].mAuto&&!loaded[0].mHold&&!loaded[1].mAuto&&loaded[1].mHold&&loaded[2].mAuto,"legacy persistence");
 blank[0].mHold=true;auto saved=writeSlots(blank,[](const LLUUID&){return true;});
 require(saved[0]["auto"].asBoolean()&&!saved[0]["hold"].asBoolean()&&saved[0]["stay"].asBoolean()&&!saved[0]["return_after"].asBoolean(),"Auto compatibility");
 U64 gen=loadGeneration();readSlots(saved);require(loadGeneration()!=gen,"load generation");
 scenario="invalid_load";gen=loadGeneration();LLSD invalidMap;invalidMap["slots"]="bad";
 for(const auto& invalid:std::array<LLSD,4>{LLSD(),LLSD("bad"),LLSD(3),invalidMap}){readSlots(invalid);require(loadGeneration()==gen,"invalid data advanced load generation");}
 readSlots(LLSD::emptyArray());require(loadGeneration()==gen+1,"empty slot array not accepted");
 auto& c=LLDirectorCast::instance();auto& m=LLActorMover::instance();c.onChanged=&hotGroupChanged;
 auto poll=[&]{LLFrameTimer::now+=.26f;hotNow+=.26;updateHotAuto(false);};
 auto press=[&](S32 slot){al_gaze_hot_note_manual(slot);c.hotGazeInput(slot,true);};
 auto state=[&](const LLUUID& id,int slot=0){auto phase=m.hotGazeSlotState(id,slot).state;return phase==State::FINISHED?State::NONE:phase;};
 Slot setup;setup.mWho=a;setup.mHasTarget=true;setup.mTarget.mMode=LLActorMover::GazeTarget::CAMERA;
 scenario="startup";updateHotAuto(false);require(m.releases==0,"fresh idle releases empty slots");
 scenario="readiness";m.unresolved.insert(a);setHotSlot(0,setup);int before=m.fires;poll();require(m.fires==before,"unresolved Who fired");m.unresolved.clear();m.ready=false;poll();require(m.fires==before,"invalid target fired");m.ready=true;int initialResolves=m.resolves;poll();require(m.resolves==initialResolves+1,"Auto resolved twice per fire");require(m.fires==before+1&&state(a)==State::ACTIVE,"readiness did not fire");
 scenario="unchanged";before=m.fires;poll();poll();require(m.fires==before&&state(a)==State::ACTIVE&&m.toggles==0,"live actor toggled");
 scenario="edit";before=m.fires;setHotSlot(0,setup);require(m.fires==before+1&&state(a)==State::ACTIVE,"unchanged edit not synchronous");
 setup.mTarget.mObjectOffset.mV[0]=1;before=m.fires;setHotSlot(0,setup);require(m.fires==before+1,"offset edit not synchronous");
 scenario="manual_release";press(0);require(state(a)==State::RETURNING,"manual release missing");poll();hotNow+=5;before=m.fires;poll();m.ready=false;poll();m.ready=true;poll();require(m.fires==before&&state(a)==State::NONE,"manual release undone");
 scenario="manual_recovery";press(0);before=m.fires;poll();require(m.fires==before&&state(a)==State::ACTIVE,"manual Fire before poll toggled");
 scenario="group_release";setHotSlot(0,setup);m.releaseHotGazeSlot(0);poll();require(state(a)==State::RETURNING,"Auto interrupted return");hotNow+=5;before=m.fires;poll();require(m.fires==before+1&&state(a)==State::ACTIVE,"group release stranded recipient");
 scenario="avatar_removed";m.onAvatarRemoved(a);before=m.fires;poll();require(m.fires==before+1&&state(a)==State::ACTIVE,"silent avatar removal stranded Auto");
 scenario="alias_collision";c.mCast={{a,"Group"},{b,"Other"}};setup.mGroup="Group";setHotSlot(0,setup);
 // Pinned replaceActorId's alias collision drops the destination even when source has no hot state.
 m.dropActor(a);hotAutoSlots().rewriteActorReferences(b,a);c.mCast[1].mId=a;before=m.fires;poll();require(m.fires==before+1&&state(a)==State::ACTIVE,"alias collision silent drop stranded Auto");
 scenario="global_clear";m.clearHotGazes();before=m.fires;poll();require(m.fires==before+1&&state(a)==State::ACTIVE,"global clear stranded Auto");
 // Each non-user disappearance must recover with dwell enabled before any RETURNING.
 for(int drop=0;drop<4;++drop){
  scenario="dwell_drop_"+std::to_string(drop);Slot early=setup;early.mGroup.clear();early.mWho=a;early.mStay=false;early.mDwell=60;early.mReturn=0;
  setHotSlot(0,early);poll();before=m.fires;
  if(drop==0)m.clearHotGazes();if(drop==1)m.onAvatarRemoved(a);if(drop==2)m.dropActor(a);if(drop==3)m.releaseHotGazeSlot(0);
  poll();require(m.fires==before+1&&state(a)==State::ACTIVE,"early dwell drop stranded Auto");
 }
 scenario="dwell_return_stop";Slot complete=setup;complete.mGroup.clear();complete.mWho=a;complete.mStay=false;complete.mDwell=.3f;complete.mReturn=1;
 setHotSlot(0,complete);hotNow+=.5;poll();require(state(a)==State::RETURNING,"dwell return missing");before=m.fires;hotNow+=2;poll();poll();require(m.fires==before&&state(a)==State::NONE,"completed dwell fired again");
 scenario="zero_fire";complete.mSnap=0;complete.mDwell=0;setHotSlot(0,complete);require(state(a)==State::RETURNING,"zero fire not returning");before=m.fires;hotNow+=2;poll();poll();require(m.fires==before&&state(a)==State::NONE,"zero fire repeated");
 setHotSlot(0,setup);

 // Cadence alone, with mover entries both retained and erased by paint after completion.
 for(int duration=0;duration<2;++duration)for(int erased=0;erased<2;++erased){
  scenario="cadence_return_"+std::to_string(duration)+"_"+std::to_string(erased);
  m.clearHotGazes();Slot brief=setup;brief.mGroup.clear();brief.mWho=a;brief.mStay=false;brief.mSnap=.12f;brief.mDwell=.2f;brief.mReturn=duration?.1f:0.f;
  setHotSlot(0,brief);poll();require(state(a)==State::ACTIVE,"brief not active at first cadence");before=m.fires;
  if(erased){hotNow+=.26;m.dropActor(a);LLFrameTimer::now+=.26f;updateHotAuto(false);}else poll();
  require(m.fires==before&&state(a)==State::NONE,"brief natural completion repeated without RETURNING sample");poll();poll();require(m.fires==before,"brief repeated later");
 }
 scenario="all_zero";m.clearHotGazes();Slot instant=setup;instant.mGroup.clear();instant.mWho=a;instant.mStay=false;instant.mSnap=instant.mDwell=instant.mReturn=0;
 setHotSlot(0,instant);require(m.hotGazeSlotState(a,0).state==State::FINISHED,"finished entry hidden");before=m.fires;poll();m.dropActor(a);poll();require(m.fires==before,"all-zero fire repeated");
 // Observation is deliberately later than the planned end (.42). The envelope's
 // release time decides whether it was cut short, even without a RETURNING poll.
 for(int early=0;early<2;++early){
  scenario=early?"delayed_finished_early":"delayed_finished_natural";m.clearHotGazes();Slot brief=instant;brief.mSnap=.12f;brief.mDwell=.2f;brief.mReturn=.1f;
  setHotSlot(0,brief);auto envelope=m.hotGazeSlotState(a,0);hotNow=envelope.start+(early?.2:.32);m.releaseHotGazeSlot(0);
  hotNow=envelope.start+.26;before=m.fires;poll();
  require(m.fires==before+(early?1:0),"delayed FINISHED misclassified explicit release");
  require(early?state(a)==State::ACTIVE:state(a)==State::NONE,"delayed FINISHED phase wrong");
 }
 // A live edit must not consume the retiring identity when the new target is unavailable.
 for(int failure=0;failure<2;++failure){
  scenario=failure?"edit_suppressed":"edit_unresolved";m.clearHotGazes();Slot replacement=setup;replacement.mGroup.clear();replacement.mWho=a;replacement.mStay=false;replacement.mDwell=.2f;replacement.mReturn=1.2f;
  setHotSlot(0,replacement);auto retired=m.hotGazeSlotState(a,0);before=m.fires;
  replacement.mTarget.mMode=LLActorMover::GazeTarget::CAST_MEMBER;replacement.mTarget.mCastRef=n;
  if(failure)m.suppressed.insert(a);else m.unresolved.insert(n);
  setHotSlot(0,replacement);require(m.fires==before&&m.hotGazeSlotState(a,0).serial==retired.serial,"failed replacement changed envelope");
  poll();hotNow+=2;poll();poll();require(m.fires==before,"unavailable replacement fired");
  m.suppressed.clear();m.unresolved.clear();poll();require(m.fires==before+1&&state(a)==State::ACTIVE,"retiring envelope consumed pending edit");
  before=m.fires;hotNow+=3;poll();require(m.fires==before,"replacement completion repeated");
  // Readiness can also return before the retired envelope finishes. A falsely
  // accepted old identity would wait for its return instead of retrying now.
  m.clearHotGazes();Slot retiring=replacement;retiring.mTarget.mMode=LLActorMover::GazeTarget::CAMERA;setHotSlot(0,retiring);
  if(failure)m.suppressed.insert(a);else m.unresolved.insert(n);
  setHotSlot(0,replacement);poll();before=m.fires;
  m.suppressed.clear();m.unresolved.clear();poll();require(m.fires==before+1&&state(a)==State::ACTIVE,"pending edit waited for retiring envelope");
 }
 scenario="same_frame_identity";m.clearHotGazes();Slot sameFrame=setup;sameFrame.mGroup.clear();sameFrame.mWho=a;
 setHotSlot(0,sameFrame);auto first=m.hotGazeSlotState(a,0);before=m.fires;setHotSlot(0,sameFrame);auto second=m.hotGazeSlotState(a,0);
 require(first.start==second.start&&first.serial!=second.serial&&m.fires==before+1,"same-frame replacement identity missing");before=m.fires;poll();require(m.fires==before,"same-frame replacement left pending");
 scenario="target_name_setup";Slot named=sameFrame;named.mTargetName="New point label";require(sameSetup(named,sameFrame),"display name rearmed setup");
 scenario="intensity_setup";Slot intensity=sameFrame;intensity.mTarget.mIntensityOverride=.5f;require(!sameSetup(intensity,sameFrame),"target intensity omitted from setup");
 scenario="query_identity";auto own=m.hotGazeSlotState(a,0);require(own.start==m.mHotGazes[a].mState.mStart&&own.serial==m.mHotGazes[a].mPaint.mGaze.mHotGeneration,"query identity wrong");
 require(m.hotGazeSlotState(a,1).state==State::NONE,"query wrong slot");m.releaseHotGazeSlot(0);require(m.hotGazeSlotState(a,0).release==hotNow,"query release timing wrong");
 scenario="async_object_name";m.clearHotGazes();Slot object=instant;object.mSnap=.12f;object.mDwell=.2f;object.mReturn=.1f;object.mTarget.mMode=LLActorMover::GazeTarget::OBJECT;object.mTarget.mObjectRef=n;object.mTargetName="Object (name pending)";
 setHotSlot(0,object);poll();poll();before=m.fires;int priorReleases=m.releases;U64 priorRevision=c.mHotGazeRevision;
 require(state(a)==State::NONE,"object look did not complete");installPropertiesCallback();ALObjectPropertiesCache::instance().publish(n,"Named object");
 require(c.mHotGazeSlots[0].mTargetName=="Named object"&&c.mHotGazeRevision==priorRevision+1,"async name did not refresh model/UI revision");
 auto namedSaved=writeSlots(c.mHotGazeSlots,[](const LLUUID&){return true;});require(namedSaved[0]["target_name"].asString()=="Named object","async name not persisted");
 poll();poll();require(m.fires==before&&m.releases==priorReleases&&state(a)==State::NONE,"async name repeated completed look");
 ALObjectPropertiesCache::instance().publish(n,"Named object");require(c.mHotGazeRevision==priorRevision+1,"unchanged name dirtied UI");
 scenario="parked_own";m.clearHotGazes();Slot parked=setup;parked.mGroup.clear();parked.mWho=a;setHotSlot(0,parked);auto running=m.hotGazeSlotState(a,0);U32 token=m.parkHotGaze(a);before=m.fires;
 const auto& constMover=m;auto held=constMover.hotGazeSlotState(a,0);
 require(token&&m.mHotGazes.empty()&&held.state==State::ACTIVE&&held.start==running.start&&held.serial==running.serial,"parked own identity/phase hidden");
 require(state(a,-1)==State::ACTIVE&&state(a,1)==State::NONE&&state(b,-1)==State::NONE,"parked query leaked slot/owner");poll();poll();require(m.fires==before,"Auto fired over parked own look");
 scenario="parked_other_slot";setHotSlot(1,parked);poll();require(m.fires==before&&state(a,1)==State::NONE,"Auto stole parked other slot");setHotSlot(1,Slot{});
 scenario="parked_return";m.releaseHotGazeSlot(0);held=m.hotGazeSlotState(a,0);require(held.state==State::RETURNING&&held.release==hotNow,"parked release hidden");poll();require(m.fires==before,"Auto interrupted parked return");
 m.unparkHotGaze(token,a);require(m.mParkedHotGazes.empty()&&state(a)==State::RETURNING,"unpark lost returning envelope");poll();require(m.fires==before,"unpark triggered duplicate fire");
 scenario="parked_rekey";setHotSlot(0,parked);running=m.hotGazeSlotState(a,0);token=m.parkHotGaze(a);m.rewriteHotGazeReferences(a,n);hotAutoSlots().rewriteActorReferences(a,n);rewrite(c.mHotGazeSlots,a,n);before=m.fires;
 poll();held=m.hotGazeSlotState(n,0);require(m.fires==before&&state(a,-1)==State::NONE&&held.state==State::ACTIVE&&held.start==running.start&&held.serial==running.serial,"parked rekey lost own state");
 m.unparkHotGaze(token,n);poll();require(m.fires==before&&state(n)==State::ACTIVE,"respawn unpark toggled envelope");
 scenario="parked_snapshots";m.clearHotGazes();setHotSlot(0,object);token=m.parkHotGaze(a);hotNow+=1;setHotSlot(0,parked);U32 liveToken=m.parkHotGaze(a);before=m.fires;poll();
 require(m.fires==before&&state(a)==State::ACTIVE,"finished snapshot hid live parked envelope");m.discardParkedHotGaze(token);require(state(a)==State::ACTIVE,"discard removed another snapshot");m.unparkHotGaze(liveToken,a);require(m.mParkedHotGazes.empty()&&state(a)==State::ACTIVE,"snapshot unpark lost live envelope");
 // A retained Clear snapshot must never replace the current look's completion.
 setHotSlot(1,Slot{});
 for(int erased=0;erased<3;++erased){
  scenario=erased==0?"finished_running_parked":erased==1?"erased_running_parked_end":"erased_running_parked_early";
  m.clearHotGazes();Slot brief=object;brief.mSnap=.12f;brief.mDwell=.2f;brief.mReturn=.1f;
  setHotSlot(0,brief);auto older=m.hotGazeSlotState(a,0);token=m.parkHotGaze(a);hotNow+=1;
  setHotSlot(0,brief);auto current=m.hotGazeSlotState(a,0);require(current.start>older.start,"new running identity not latched");
  hotNow=current.start+(erased==2?.15:.5);if(erased)m.dropActor(a);
  auto observed=m.hotGazeSlotState(a,0);
  before=m.fires;poll();require(m.fires==before+(erased==2?1:0),"stale snapshot misclassified latched completion/drop");
  require(observed.start==(erased?older.start:current.start)&&observed.state==State::FINISHED,"finished query lost authoritative identity");
  before=m.fires;poll();poll();require(m.fires==before,"completed look repeated later with parked snapshot");
 }
 // Running wins in every phase, even over a live parked snapshot. Parked-only
 // selection follows phase priority, then start, independent of token order.
 scenario="query_precedence";
 for(int phase=0;phase<3;++phase)for(int parkedPhase=0;parkedPhase<3;++parkedPhase){
  m.clearHotGazes();LLActorMover::HotGaze runningHot,parkedHot;
  runningHot.mState.fire(0,hotNow-2,0,0,1,false,true);parkedHot.mState.fire(0,hotNow-1,0,0,1,false,true);
  if(phase)runningHot.mState.release(hotNow-(phase==2?2:.2));
  if(parkedPhase)parkedHot.mState.release(hotNow-(parkedPhase==2?2:.2));
  m.mHotGazes[a]=runningHot;m.mParkedHotGazes.park(a,parkedHot);auto picked=m.hotGazeSlotState(a,0);
  require(picked.start==runningHot.mState.mStart&&picked.state==(phase==0?State::ACTIVE:phase==1?State::RETURNING:State::FINISHED),"parked snapshot overrode running phase");
 }
 for(int finished=0;finished<2;++finished)for(int reverse=0;reverse<2;++reverse){
  m.clearHotGazes();LLActorMover::HotGaze older,newer;
  older.mState.fire(0,hotNow-2,0,0,0,false,!finished);newer.mState.fire(0,hotNow-1,0,0,0,false,!finished);
  m.mParkedHotGazes.park(a,reverse?newer:older);m.mParkedHotGazes.park(a,reverse?older:newer);
  require(m.hotGazeSlotState(a,0).start==newer.mState.mStart,"parked selection ignored newest start");
 }
 m.clearHotGazes();LLActorMover::HotGaze liveOld,finishedNew;
 liveOld.mState.fire(0,hotNow-2,0,0,0,false,true);finishedNew.mState.fire(0,hotNow-1,0,0,0,false,false);
 m.mParkedHotGazes.park(a,liveOld);m.mParkedHotGazes.park(a,finishedNew);
 require(m.hotGazeSlotState(a,0).start==liveOld.mState.mStart,"new finished snapshot hid older live snapshot");
 // Clear slot 0, resume firing, manually start slot 1, then edit slot 0.
 // Its own parked return/completion must not let Auto steal slot 1.
 scenario="coexist_returning";m.clearHotGazes();setHotSlot(0,parked);token=m.parkHotGaze(a);hotNow+=.1;
 Slot tapOther=parked;tapOther.mAuto=false;setHotSlot(1,tapOther);press(1);auto owner=m.hotGazeSlotState(a,1);before=m.fires;
 setHotSlot(0,parked);require(m.fires==before&&state(a,0)==State::RETURNING&&state(a,1)==State::ACTIVE,"edit stole running other slot during parked return");
 poll();require(m.fires==before&&m.hotGazeSlotState(a,1).start==owner.start,"poll stole other slot during parked return");
 scenario="coexist_finished";hotNow+=2;poll();require(m.fires==before&&state(a,1)==State::ACTIVE,"Auto stole other slot after parked completion");
 press(1);poll();require(m.fires==before&&state(a,1)==State::RETURNING,"Auto stole other slot while owner returning");
 hotNow+=2;poll();require(m.fires==before+1&&state(a,0)==State::ACTIVE,"blocked Auto did not remain pending");
 scenario="coexist_erased_pending";m.clearHotGazes();Slot shortLook=object;
 setHotSlot(0,shortLook);hotNow+=.05;press(1);before=m.fires;poll();hotNow+=1;poll();
 require(m.fires==before&&state(a,1)==State::ACTIVE,"erased own look stole other slot");
 press(1);hotNow+=2;poll();require(m.fires==before+1&&state(a,0)==State::ACTIVE,"ownership block lost pending intent past expected end");
 setHotSlot(1,Slot{});m.clearHotGazes();
 setHotSlot(0,setup);
 scenario="dwell_group_leave";c.mCast={{a,"Group"},{b,"Group"}};Slot earlyGroup=setup;earlyGroup.mGroup="Group";earlyGroup.mStay=false;earlyGroup.mDwell=60;earlyGroup.mReturn=0;setHotSlot(0,earlyGroup);before=m.fires;c.addToGroup({a},"Other");poll();require(m.fires==before+1&&state(b)==State::ACTIVE,"dwell group survivor stranded");c.mCast={{a,"Group"},{b,"Other"}};setHotSlot(0,setup);m.clearHotGazes();poll();
 scenario="suppressed";setup.mStay=false;setup.mDwell=60;setup.mGroup.clear();setup.mWho=b;m.suppressed.insert(b);setHotSlot(1,setup);before=m.fires;poll();require(m.fires==before&&state(b,1)==State::NONE,"cleared clone fired");m.suppressed.clear();poll();require(m.fires==before+1&&state(b,1)==State::ACTIVE,"suppressed fire consumed readiness");
 scenario="shared_slots";setup.mStay=true;Slot overlap=setup;overlap.mWho=a;before=m.fires;setHotSlot(1,overlap);poll();require(m.fires==before&&state(a)==State::ACTIVE&&state(a,1)==State::NONE,"Auto stole other slot");
 // Once the owner is explicitly deleted and finishes returning, the pending second slot can fire.
 setHotSlot(0,Slot{});poll();require(state(a)==State::RETURNING,"pending slot stole returning actor");hotNow+=5;before=m.fires;poll();require(m.fires==before+1&&state(a,1)==State::ACTIVE,"second slot not pending");
 // Takeover applies even to recipients never latched, and to presses entirely between polls.
 scenario="manual_between_polls";setHotSlot(1,Slot{});hotNow+=5;m.clearHotGazes();Slot pending=setup;pending.mWho=b;pending.mReturn=0;m.unresolved.insert(b);setHotSlot(0,pending);m.unresolved.clear();press(0);press(0);before=m.fires;poll();poll();require(m.fires==before&&state(b)==State::NONE&&manualMode(0),"between-polls manual release undone");
 m.clearHotGazes();poll();require(m.fires==before,"manual mode recovered a drop");
 scenario="invalid_load_manual";gen=loadGeneration();readSlots(LLSD("bad"));poll();
 require(loadGeneration()==gen&&manualMode(0)&&m.fires==before,"invalid load rearmed manual slot");
 setHotSlot(1,pending);require(state(b,1)==State::ACTIVE,"manual mode leaked to another slot");setHotSlot(1,Slot{});
 setHotSlot(0,pending);require(state(b)==State::ACTIVE&&!manualMode(0),"edit did not clear manual mode");
 press(0);m.clearHotGazes();c.mHotGazeSlots=readSlots(writeSlots(c.mHotGazeSlots,[](const LLUUID&){return true;}));before=m.fires;poll();require(m.fires==before+1&&state(b)==State::ACTIVE&&!manualMode(0),"load did not clear manual mode");
 scenario="reload_double_press";m.clearHotGazes();c.mHotGazeSlots=readSlots(writeSlots(c.mHotGazeSlots,[](const LLUUID&){return true;}));press(0);press(0);before=m.fires;poll();require(m.fires==before&&state(b)==State::NONE&&manualMode(0),"load poll lost manual takeover");
 scenario="reload_partial_manual";c.mCast={{a,"Group"},{b,"Group"}};Slots scene;scene[0]=setup;scene[0].mGroup="Group";
 m.clearHotGazes();c.mHotGazeSlots=readSlots(writeSlots(scene,[](const LLUUID&){return true;}));m.unresolved.insert(b);press(0);before=m.fires;poll();require(m.fires==before&&state(a)==State::ACTIVE,"load poll toggled partial manual Fire");m.unresolved.clear();poll();require(m.fires==before&&state(a)==State::ACTIVE&&state(b)==State::NONE,"manual takeover fired unresolved member");setHotSlot(0,c.mHotGazeSlots[0]);require(state(b)==State::ACTIVE,"edit did not rearm manual slot");
 scenario="members";before=m.fires;c.mCast.push_back({a,"Group"});hotGroupChanged({LLDirectorCast::GroupEvent::MEMBERS,{},{}});int memberQueries=m.queries;poll();require(m.queries==memberQueries+2,"members not deduplicated");require(m.fires==before&&state(a)==State::ACTIVE&&state(b)==State::ACTIVE,"MEMBERS/duplicates toggled live actor");
 scenario="merge";c.mCast.pop_back();c.mCast.push_back({n,"Renamed"});before=m.fires;c.renameGroup("Group","Renamed");poll();require(m.fires==before+1&&state(a)==State::ACTIVE&&state(n)==State::ACTIVE,"merge re-fired live actor");
 scenario="rekey";m.mHotGazes[n]=m.mHotGazes[b];m.mHotGazes.erase(b);hotAutoSlots().rewriteActorReferences(b,n);for(auto& member:c.mCast)if(member.mId==b)member.mId=n;before=m.fires;poll();require(m.fires==before&&state(n)==State::ACTIVE,"live rekey toggled");c.mCast={{a,"Renamed"},{n,"Renamed"}};
 scenario="group_leave";c.addToGroup({a},"Other");m.unresolved.insert(n);hotNow+=5;before=m.fires;poll();require(m.fires==before&&state(n)==State::NONE,"unresolved group survivor fired");m.unresolved.clear();poll();require(m.fires==before+1&&state(n)==State::ACTIVE,"public group leave stranded survivor");
 scenario="group_manual_race";m.releaseHotGazeSlot(0);hotNow+=5;m.unresolved.insert(n);poll();m.unresolved.clear();press(0);before=m.fires;poll();require(m.fires==before&&state(n)==State::ACTIVE,"manual Fire after recovery toggled recipient");
 scenario="dwell";Slot dwell=setup;dwell.mWho=n;dwell.mStay=false;setHotSlot(0,dwell);hotNow+=dwell.mSnap+dwell.mDwell;poll();require(state(n)==State::RETURNING,"dwell return not observed");hotNow+=10;before=m.fires;poll();m.ready=false;poll();m.ready=true;poll();require(m.fires==before&&state(n)==State::NONE,"dwell re-fired unchanged setup");
 scenario="dwell_rekey";hotAutoSlots().rewriteActorReferences(n,b);c.mHotGazeSlots[0].mWho=b;before=m.fires;poll();require(m.fires==before,"completed rekey lost latch");
 scenario="manual_again";press(0);before=m.fires;poll();require(m.fires==before&&state(b)==State::ACTIVE,"manual press did not resume dwell");
 scenario="tap_hold";Slot tap=setup;tap.mAuto=false;tap.mWho=a;setHotSlot(0,tap);hotNow+=5;before=m.fires;poll();require(m.fires==before,"Tap fired automatically");press(0);require(state(a)==State::ACTIVE,"original Tap dispatch missing");press(0);require(state(a)==State::RETURNING,"original Tap toggle missing");tap.mHold=true;setHotSlot(0,tap);press(0);c.hotGazeInput(0,false);require(state(a)==State::RETURNING,"original Hold up missing");
 scenario="cadence";setup.mWho=b;m.unresolved.insert(b);setHotSlot(0,setup);LLFrameTimer::now+=.20f;setHotSlot(1,Slot{});m.unresolved.clear();before=m.fires;LLFrameTimer::now+=.06f;updateHotAuto(false);require(m.fires==before+1&&state(b)==State::ACTIVE,"forced update advanced poll clock");
 int enumerations=c.whoCalls,resolves=m.resolves,queries=m.queries;for(int i=0;i<40;++i)hotGroupChanged({LLDirectorCast::GroupEvent::MEMBERS,{},{}});require(c.whoCalls==enumerations&&m.resolves==resolves&&m.queries==queries,"revision burst bypassed cadence");
 scenario="own_query";m.clearHotGazes();m.copies.insert(a);require(state(a)==State::NONE&&state(a,-1)==State::NONE,"inherited copy counted as own state");
 scenario="shutdown";int accesses=c.accesses+m.accesses;resolves=m.resolves;LLApp::exiting=true;updateHotAuto(true);LLApp::exiting=false;LLDirectorCast::exists=false;updateHotAuto(true);LLDirectorCast::exists=true;LLActorMover::exists=false;updateHotAuto(true);LLActorMover::exists=true;require(accesses==c.accesses+m.accesses&&resolves==m.resolves,"shutdown accessed singleton");
 std::cout<<"PASS: production query/controller/codec/setter/dispatch, silent drops/clear/suppression, overlapping slots, load/manual race, group/member/rekey/dwell/edit, cadence and shutdown\n";
}
'''
    return source.replace('LLApp', 'FixtureApp').replace('LLFrameTimer', 'FixtureTimer')


def query_allowance_checks():
    from director_actor_bar_check import reverse_hot_gaze_query_mover
    for name in ('llactormover.cpp','llactormover.h'):
        source=(ROOT/'indra/newview'/name).read_text(encoding='utf-8')
        base=subprocess.check_output(['git','show','f2b73c3d416:indra/newview/'+name],cwd=ROOT).decode('utf-8').replace('\r\n','\n')
        peeled=reverse_hot_gaze_query_mover(source,name)
        assert peeled==base and reverse_hot_gaze_query_mover(peeled,name)==base,(name,'query reversal/idempotence')
        for broken in (source+'\n// unrelated edit\n',source.replace('hotGazeSlotState(', 'changedQuery(',1)):
            try:
                result=reverse_hot_gaze_query_mover(broken,name)
                assert result==base,(name,'outside allowance')
            except AssertionError:print('PASS: query allowance negative rejected:',name,flush=True)
            else:raise AssertionError('query allowance accepted unrelated edit: '+name)


def input_wiring_checks():
    sources = {n: (ROOT / 'indra/newview' / n).read_text(encoding='utf-8') for n in ('llviewerinput.cpp','llviewerjoystick.cpp')}
    bases = {n: subprocess.check_output(['git','show','f2b73c3d416:indra/newview/'+n],cwd=ROOT).decode('utf-8').replace('\r\n','\n') for n in sources}
    def check(texts):
        for name,text in texts.items():
            lines = text.splitlines(keepends=True)
            notes = [line for line in lines if 'al_gaze_hot_note_manual(' in line]
            expected = [f'    if (s == KEYSTATE_DOWN) al_gaze_hot_note_manual({i});\n' for i in range(4)] if name=='llviewerinput.cpp' else ['        if (down != was && down && (mXIEdges & masks[i])) al_gaze_hot_note_manual(i);\n']
            assert notes==expected,(name,'press notification changed')
            assert ''.join(line for line in lines if line not in notes)==bases[name],(name,'original dispatch changed')
    check(sources)
    for name in sources:
        notes=[line for line in sources[name].splitlines() if 'al_gaze_hot_note_manual(' in line]
        calls=[line for line in sources[name].splitlines() if 'LLDirectorCast::instance().hotGazeInput(' in line]
        for call in notes+calls:
            broken=dict(sources);broken[name]=broken[name].replace(call,call.replace('al_gaze_hot_note_manual','missing_note').replace('LLDirectorCast::instance().hotGazeInput','replacement_dispatch'),1)
            try:check(broken)
            except AssertionError:print('PASS: wiring negative rejected:',name,call.strip(),flush=True)
            else:raise AssertionError('input negative survived')
    ui=(ROOT/'indra/newview/alpanellensgaze.cpp').read_text(encoding='utf-8')
    assert 'al_gaze_hot_note_manual(mHotButtonSlot);\n        LLDirectorCast::instance().hotGazeInput(mHotButtonSlot, true);' in ui
    assert 'al_gaze_hot_input(' not in ui
    envelope=(ROOT/'indra/newview/algazehot.h').read_text(encoding='utf-8')
    assert envelope==subprocess.check_output(['git','show','f2b73c3d416:indra/newview/algazehot.h'],cwd=ROOT).decode('utf-8').replace('\r\n','\n')


def main():
    query_allowance_checks()
    input_wiring_checks()
    flags,libraries=viewer_native_flags();directory=check_directory('hot_slots_auto');source=fixture()
    result=compile_run(directory,source,'hot_slots_auto',flags=flags,libraries=libraries)
    assert result.returncode==0,result.stdout+result.stderr
    print(result.stdout,end='',flush=True)
    negatives={
      'new_defaults':('bool mAuto = true','bool mAuto = false'),
      'legacy':('s.has("auto") && s["auto"].asBoolean()','!s.has("auto") || s["auto"].asBoolean()'),
      'hold_compatibility':('s["hold"] = !slot.mAuto && slot.mHold','s["hold"] = slot.mHold'),
      'load_generation':('++loadGeneration();','(void)loadGeneration();'),
      'invalid_load_generation':('if (!data.isArray()) return slots;', 'if (!data.isArray()) { ++loadGeneration(); return slots; }'),
      'suppression_latch':('else\n                    latch = {info.start, info.serial, 0.0, true};','else\n                    latch = {info.start, info.serial, 0.0, false};'),
      'cross_slot':('if (owned) { latch.pending = true; continue; }',''),
      'cross_slot_pending':('if (owned) { latch.pending = true; continue; }','if (owned) continue;'),
      'manual_release':('if (manualMode(i)) continue;',''),
      'dwell':('if (!latch.pending && !slot.mStay &&','if (false &&'),
      'retired_identity':('if (replacement)\n                    latch', 'if (fired.state != State::NONE)\n                    latch'),
      'zero_fire':('const bool replacement = fired.state != State::NONE &&','const bool replacement = fired.state == State::ACTIVE &&'),
      'manual_edit':('manualControls()[index].active = false;',''),
      'manual_load':('control.active && control.generation == loadGeneration()','control.active'),
      'returning_guard':('const bool live = state == State::ACTIVE || state == State::RETURNING;', 'const bool live = state == State::ACTIVE;'),
      'edited_wrapper':('hotAutoSlots().edited(index);',''),
      'synchronous':('    updateHotAuto(true);',''),
      'forced_clock':('if (!force && !clock.due()) return;','if (!force && !clock.due()) return; clock.forceDue(); clock.due();'),
      'per_frame':('if (!force && !clock.due()) return;',''),
      'shutdown':('if (FixtureApp::isExiting() || !LLDirectorCast::instanceExists() || !LLActorMover::instanceExists()) return;',''),
      'duplicate_members':('members.erase(std::unique(members.begin(), members.end()), members.end());',''),
      'query_slot':('(slot != -1 && state.mSlot != slot)','false'),
      'query_returning':('(state.returning(now) ? HotGazeSlotState::RETURNING : HotGazeSlotState::ACTIVE)', 'HotGazeSlotState::ACTIVE'),
      'inherited_copy':('HotGazeSlotInfo result;', 'if(copies.count(actor))return {HotGazeSlotState::ACTIVE,0,-1,1}; HotGazeSlotInfo result;'),
      'query_finished':('state.finished(now) ? HotGazeSlotState::FINISHED','false ? HotGazeSlotState::FINISHED'),
      'rekey_latches':('if (new_id.notNull()) latches[new_id] = it->second;',''),
      'premature_dwell_stop':('((state == State::NONE || stale) && now + tolerance >= latch.end)', '(state == State::NONE || stale)'),
      'pending_retirement':('&& (!latch.pending || !same)', ''),
      'erased_expected_end':('((state == State::NONE || stale) && now + tolerance >= latch.end)', '((state == State::NONE || stale) && false)'),
      'finished_success':('const bool replacement = fired.state != State::NONE &&', 'const bool replacement = fired.state != State::NONE && fired.state != State::FINISHED &&'),
      'early_finished':('(info.release < 0.0 || info.release + slot.mReturn + tolerance >= latch.end)', 'true'),
      'delayed_finished':('info.release + slot.mReturn + tolerance >= latch.end', 'now + tolerance >= latch.end'),
      'target_name_setup':('return a.mAuto == b.mAuto &&', 'return a.mTargetName == b.mTargetName && a.mAuto == b.mAuto &&'),
      'intensity_setup':(' && x.mIntensityOverride == y.mIntensityOverride', ''),
      'parked_query':('mParkedHotGazes.forOwner(actor,', 'mParkedHotGazes.forOwner(LLUUID(),'),
      'running_authority':('if (result.state != HotGazeSlotState::NONE) return result;', 'if (live(result)) return result;'),
      'parked_recent':('info.start > result.start', 'info.start < result.start'),
      'stale_snapshot':('state == State::NONE || stale', 'state == State::NONE'),
      'query_start':('return {phase, state.mStart, state.mRelease,', 'return {phase, -1.0, state.mRelease,'),
      'query_serial':('hot.mPaint.mGaze.mHotGeneration};','0};'),
      'query_release':('state.mStart, state.mRelease,', 'state.mStart, -1.0,'),
      'manual_note':('ALGazeHot::noteManual(slot);','(void)slot;'),
      'double_resolve':('            mover.fireHotGaze(actor, index, slot.mTarget, slot.mSnap, slot.mDwell,','            LLVector3d point; mover.resolveHotGazeTarget(cast.resolve(actor), slot.mTarget, point);\n            mover.fireHotGaze(actor, index, slot.mTarget, slot.mSnap, slot.mDwell,'),
    }
    for name,(old,new) in negatives.items():
        assert source.count(old)==1,(name,source.count(old))
        result=compile_run(directory,source.replace(old,new),name,flags=flags,libraries=libraries)
        assert result.returncode==2,(name,result.stdout,result.stderr)
        print('PASS: planted negative rejected:',name,result.stderr.strip(),flush=True)
    # Require each new end-to-end case to catch its own regression, independently
    # of the direct query/codec assertions and earlier ownership cases.
    for case in ('finished_running_parked','erased_running_parked_end','erased_running_parked_early','coexist_returning','coexist_finished','coexist_erased_pending','invalid_load_manual'):
        if case=='finished_running_parked':
            old='if (result.state != HotGazeSlotState::NONE) return result;'
            new=f'if (result.state != HotGazeSlotState::NONE && scenario!="{case}") return result; if(scenario=="{case}") result={{}};'
        elif case.startswith('erased_'):
            old='state == State::NONE || stale'
            new=f'state == State::NONE || (stale && scenario!="{case}")'
            if case.endswith('_early'):
                old='((state == State::NONE || stale) && now + tolerance >= latch.end)'
                new=f'((state == State::NONE || stale) && (scenario=="{case}" || now + tolerance >= latch.end))'
        elif case.startswith('coexist_'):
            old='if (owned) { latch.pending = true; continue; }'
            new=f'if (owned && scenario!="{case}") {{ latch.pending = true; continue; }}'
            if case=='coexist_erased_pending':
                new=f'if (owned) {{ if(scenario!="{case}") latch.pending = true; continue; }}'
        else:
            old='if (!data.isArray()) return slots;'
            new=f'if (!data.isArray()) {{ if(scenario=="{case}") ++loadGeneration(); return slots; }}'
        assert source.count(old)==1,(case,source.count(old))
        result=compile_run(directory,source.replace(old,new),'r7_'+case,flags=flags,libraries=libraries)
        assert result.returncode==2 and case in result.stderr,(case,result.stdout,result.stderr)
        print('PASS: round-7 scenario negative rejected:',case,flush=True)
    # Reintroduce the display-name trigger only for the real async callback case,
    # so rejection must come from a repeated completed look, not sameSetup alone.
    old='return a.mAuto == b.mAuto &&'
    broken=source.replace(old,'return (scenario!="async_object_name" || a.mTargetName == b.mTargetName) && a.mAuto == b.mAuto &&')
    result=compile_run(directory,broken,'async_object_name',flags=flags,libraries=libraries)
    assert result.returncode==2 and 'async_object_name: async name repeated completed look' in result.stderr,(result.stdout,result.stderr)
    print('PASS: async object-name callback negative rejected',flush=True)
    for case in ('cadence_return_0_0','cadence_return_0_1','cadence_return_1_0','cadence_return_1_1','all_zero','edit_unresolved','edit_suppressed'):
        if case.startswith('edit_'):
            old='if (replacement)\n                    latch'
            new=f'if (replacement || (scenario=="{case}" && fired.state!=State::NONE))\n                    latch'
        else:
            old='if (!latch.pending && !slot.mStay &&'
            new=f'if (scenario!="{case}" && !latch.pending && !slot.mStay &&'
        assert source.count(old)==1,(case,source.count(old))
        result=compile_run(directory,source.replace(old,new),'r5_'+case,flags=flags,libraries=libraries)
        assert result.returncode==2 and case in result.stderr,(case,result.stdout,result.stderr)
        print('PASS: cadence/replacement negative rejected:',case,flush=True)
    # Reintroduce trusting a prior fired latch at each absent/finished early release path.
    for case in ('alias_collision','avatar_removed','global_clear','group_release','group_leave','dwell_drop_0','dwell_drop_1','dwell_drop_2','dwell_drop_3','dwell_group_leave'):
        old='const auto state = info.state;'
        broken=source.replace(old,old+f' if(scenario=="{case}" && (state==State::NONE || state==State::FINISHED) && latches.count(actor)) continue;')
        result=compile_run(directory,broken,'stale_'+case,flags=flags,libraries=libraries)
        assert result.returncode==2 and case in result.stderr,(case,result.stdout,result.stderr)
        print('PASS: silent-release negative rejected:',case,flush=True)
    for case in ('manual_between_polls','reload_double_press','reload_partial_manual'):
        old='if (manualMode(i)) continue;'
        broken=source.replace(old,f'if (scenario!="{case}" && manualMode(i)) continue;')
        result=compile_run(directory,broken,'takeover_'+case,flags=flags,libraries=libraries)
        assert result.returncode==2 and case in result.stderr,(case,result.stdout,result.stderr)
        print('PASS: manual-takeover negative rejected:',case,flush=True)
    for case in ('reload_partial_manual','members' ,'merge','manual_recovery','group_manual_race'):
        old='if (live && !(edited && state == State::RETURNING) && (!latch.pending || !same))'
        broken=source.replace(old,f'if (scenario!="{case}" && live && !(edited && state == State::RETURNING) && (!latch.pending || !same))')
        broken=broken.replace('if (state == State::ACTIVE) continue;', f'if (scenario!="{case}" && state == State::ACTIVE) continue;')
        if case in ('reload_partial_manual','manual_recovery','group_manual_race'):
            # Slot manual mode independently prevents this duplicate fire.
            broken=broken.replace('if (manualMode(i)) continue;', f'if (scenario!="{case}" && manualMode(i)) continue;')
        result=compile_run(directory,broken,'toggle_'+case,flags=flags,libraries=libraries)
        assert result.returncode==2 and case in result.stderr,(case,result.stdout,result.stderr)
        print('PASS: live-toggle negative rejected:',case,flush=True)

if __name__=='__main__':main()
