"""Native world adapters for exact production frame/copy/Stepped/particle extraction.

No painter, ownership validation, source-copy, held capture or repair is replaced.
UI/configuration, drawable and unrelated Studio features use small stores/no-ops.
"""
from relight_area_policy_check import method


def integration(nv):
    source = r'''
#include "alsteppedanim.h"
#include <functional>
#include <limits>
#include <deque>
using LLControlAvatar=LLVOAvatar;
#undef LL_PROFILE_ZONE_NAMED
#define LL_PROFILE_ZONE_NAMED(...)
#undef LL_PROFILE_ZONE_SCOPED_CATEGORY_UI
#define LL_PROFILE_ZONE_SCOPED_CATEGORY_UI
#undef LL_PROFILE_PLOT
#define LL_PROFILE_PLOT(...)
void assert_main_thread(){}
struct LLGhostAvatar;
namespace ALGhostClearAnim{inline bool mayStampMirror(bool cleared){return !cleared;}} // [ClearAnim] stand-in for the pure rule
struct ALGhostStudio {
 enum{BACKING_OVERLAY=0,BACKING_ENTITY_CLONE=1,DRIVE_TRUE_MIRROR=3,DRIVE_REFLECT=4,DRIVE_FROZEN=2,DRIVE_DIRECTED=1,POSE_FROZEN=1};
 struct Instance {
 LLUUID mId,mEntityId,mOverlayAvatar;int mKind=1,mDriveMode=3,mDirectedMirrorFreezeDrive=0,mReflectPlaneMode=0,mMotion=0,mPose=0;
 float mStartOffset=0;bool mDeleting=false,mVisible=true;
 bool mReflectWasDriven=false,mReflectFloor=false,mReflectPlaneValid=true,mReflectHasLastPlane=true,
 mReflectPlaneDegenerate=false,mReflectMirrorPosition=false,mChaosHasBase=false,mDirectedMirror=false,mMotionHasBase=false;
 LLVector3d mReflectPlanePoint,mFootGlobal;LLVector3 mReflectPlaneNormal{0,1,0};};
 int resolveCount=0;std::vector<Instance>mInstances;std::map<LLUUID,LLGhostAvatar*>clones;
 static ALGhostStudio& instance(){static ALGhostStudio s;return s;}
 LLGhostAvatar* resolveEntityClone(const LLUUID&) const;
 const Instance* getInstance(LLUUID id)const{for(auto& i:mInstances)if(i.mId==id)return &i;return nullptr;}
 const Instance* findInstanceByRuntime(LLUUID id)const{for(auto& i:mInstances)if(i.mEntityId==id)return &i;return nullptr;}
 std::map<LLUUID,LLUUID>owners;
 LLUUID actorOwner(LLUUID id)const{auto i=owners.find(id);return i==owners.end()?id:i->second;}
 bool isVisible(const Instance& i)const{return i.mVisible;}
 LLVOAvatar* resolveOverlayAvatar(const Instance& i){auto* o=gObjectList.findObject(i.mOverlayAvatar);return o?o->asAvatar():nullptr;}
 void rememberOverlayDomain(LLVOAvatar*){}
 bool reflectOwnsPosition(const Instance&){return false;}
 LLVector3d reflectSourceFoot(LLVOAvatar* a,bool){return LLVector3d(a->getRootJoint()->getWorldPosition());}
 bool motionOwnsTilt(int){return false;}bool canApplyMotionTilt(const Instance&,LLGhostAvatar*){return false;}
 void updatePostObjectList(bool=false);
 template<class F>void applyStepAnimNameplateOffsets(F){} void restoreStepAnimNameplateOffsets(){}
};
using avatar_joint_list_t=std::vector<LLJoint*>;
struct LLGhostAvatar:LLVOAvatar {
 LLGhostAvatar(){ghost=true;}
 ~LLGhostAvatar(){sOffsetHotGazes.erase(this);}
 bool mEntityAnimCleared=false; // [ClearAnim] fixture clones are never cleared
 LLJoint* mRoot=getRootJoint();LLUUID mAnimationSourceId;int mEntityDriveMode=3;
 U32 mTrueMirrorLastStampFrame=0xFFFFFFFF,mReflectLastFrame=0xFFFFFFFF;
 bool mTrueMirrorSourceValid=false,mReflectPreviousWritten=false,mTrueMirrorParamSetsBuilt=true,mEntityPhysicsEnabled=false,mGhostFloorPosition=false;
 double mReflectNextVerify=1.e30;float mEntityScale=1;
 std::vector<std::pair<size_t,size_t>>mReflectPartners;std::vector<LLQuaternion>mReflectLastRotations;
 std::vector<int>mTrueMirrorFaceParamIds,mTrueMirrorPhysicsParamIds;
 struct FloorHold{void capture(LLVector3d,LLQuaternion){}}mReflectFloorRootHold;
 bool isBuilt(){return true;}int getEntityDriveMode(){return mEntityDriveMode;}
 const LLUUID& getAnimationSourceId(){return mAnimationSourceId;}
 LLVOAvatar* reflectSource(){auto* o=gObjectList.findObject(mAnimationSourceId);return o?o->asAvatar():nullptr;}
 void captureReflectPositions(){}void buildReflectPartners(LLVOAvatar*);
 void buildTrueMirrorParamSets(){}void stampTrueMirrorFace(LLVOAvatar*){}
 void moveStampedAttachments(){} // Production does not sync animated attachments here.

 bool directedMirrorCacheMatches();void clearDirectedMirrorJoints();void buildDirectedMirrorJoints();
 void restoreDirectedMirrorPose(bool=true);void applyDirectedMirrorStamp(S32,S32,bool=false);

 void setGhostFloorPosition(LLVector3,LLVector3){}void setGhostFootPosition(LLVector3){}
 void reassertTiltRotation(){}void refreshVisualBounds(){}
 void applyOffsetHistory(double,bool=false);void restoreOffsetPose();
 bool hasOffsetHotGaze()const{return sOffsetHotGazes.count(this)!=0;}static bool hasOffsetHotGazes(){return !sOffsetHotGazes.empty();}
 struct ClonedLinkset{LLUUID mRoot;};std::vector<ClonedLinkset>mClonedLinksets;
 void stampTrueMirrorSkeleton(LLVOAvatar*,bool,bool=false);void applyTrueMirrorStamp(bool=false);void applyReflectStamp(const LLVector3&,bool=false);
};
'''
    source += method(nv/'llactormover.cpp','bool LLActorMover::hasHotGazePose(') + '\n'
    header = (nv/'llghostavatar.h').read_text(encoding='utf-8')
    fields = header[header.index('    bool mDirectedMirror ='):header.index('    bool mDirectedMirrorNeedsReskin =')]
    fields += '    bool mDirectedMirrorNeedsReskin = false;\n'
    fields += header[header.index('    struct OffsetFrame'):header.index('    void setEntityLook(')]
    source = source.replace(' bool directedMirrorCacheMatches();', fields + ' bool directedMirrorCacheMatches();')
    source += 'std::set<const LLGhostAvatar*> LLGhostAvatar::sOffsetHotGazes;\n'
    source += 'struct HotHistoryClock { static double getTotalSeconds(){ return LLPresentationTime::currentFrame().presentation_time; } };\n'
    source += '#define LLTimer HotHistoryClock\n'
    for sig in ('void LLGhostAvatar::applyOffsetHistory(', 'void LLGhostAvatar::restoreOffsetPose(', 'void LLGhostAvatar::prepareOffsetDrive('):
        source += method(nv/'llghostavatar.cpp', sig) + '\n'
    for sig in ('bool LLGhostAvatar::directedMirrorCacheMatches(', 'void LLGhostAvatar::clearDirectedMirrorJoints(',
                'void LLGhostAvatar::buildDirectedMirrorJoints(', 'void LLGhostAvatar::restoreDirectedMirrorPose(',
                'void LLGhostAvatar::applyDirectedMirrorStamp(', 'void LLGhostAvatar::buildReflectPartners(', 'void LLGhostAvatar::stampTrueMirrorSkeleton(', 'void LLGhostAvatar::applyTrueMirrorStamp(',
                'void LLGhostAvatar::applyReflectStamp('):
        source += method(nv/'llghostavatar.cpp', sig) + '\n'
    source += method(nv/'alghoststudio.cpp','LLGhostAvatar* ALGhostStudio::resolveEntityClone(') + '\n'
    source += method(nv/'alghoststudio.cpp','void ALGhostStudio::updatePostObjectList(') + '\n'
    source += r'''
struct ALSteppedAnimRuntime {
 inline static bool sActive=false,sHasOverlay=false;
 static ALSteppedAnimRuntime& instance(){static ALSteppedAnimRuntime s;return s;}
'''
    source += method(nv/'alsteppedanimruntime.h','    struct Entry\n') + ';\n'
    source += r'''
 ALSteppedAnim::Config mConfig;double mTime=0;std::map<LLUUID,Entry>mEntries;std::vector<LLUUID>mPlanned;
 U64 mAgentRegionHandle=0,mPaletteRebuilds=0;LLVector3d mAgentOriginGlobal;bool mHaveAgentOrigin=false;
 int mActiveActors=0,mSkippedActors=0;
 static bool eligible(LLVOAvatar* a){return a&&!a->isDead();}
 static U32 poseJointCount(LLVOAvatar* a){return U32(a->getSkeleton().size());}
 static LLJoint* poseJoint(LLVOAvatar* a,const Entry&,U32 i){return i<a->getSkeleton().size()?a->getSkeleton()[i]:nullptr;}
 void buildClasses(LLVOAvatar* a,Entry& e){e.mJointCount=poseJointCount(a);e.mClasses.assign(e.mJointCount,ALSteppedAnim::JC_GAZE|ALSteppedAnim::groupBit(ALSteppedAnim::PART_HEAD));e.mParents.assign(e.mJointCount,-1);}
 void resetEntryHistory(Entry& e){e.mHaveHeld=false;}
 int activeActorCount(){return mActiveActors;}
 const ALSteppedAnim::FrameDecision* decisionFor(LLVOAvatar* a){auto i=a?mEntries.find(a->id):mEntries.end();return i==mEntries.end()?nullptr:&i->second.mDecision;}
 void verifyRestored(const char*){}
 void readPose(LLVOAvatar*,Entry&,ALSteppedAnim::PoseBuffer&);
 ALSteppedAnim::LimbFrame limbFrame(LLVOAvatar*,const Entry&,F64)const;
 void updateEaseCandidate(Entry&,const ALSteppedAnim::LimbFrame&,F64);
 void capturePose(LLVOAvatar*,Entry&,const ALSteppedAnim::LimbFrame&,F64,const Entry*,U16,U16);
 void applyEffects(LLVOAvatar*,Entry&,ALSteppedAnim::PoseBuffer&);
 void writeOverlay(LLVOAvatar*,Entry&,const ALSteppedAnim::PoseBuffer&);
 void applyHeld();void restoreOverlay(LLVOAvatar*,Entry&);void endFrameRestore();
};
constexpr U8 WRITE_ROT=1,WRITE_POS=2,WRITE_SCALE=4;
U32 gFrameCount=0;
'''
    # Pure production implementation; omit only its viewer PCH include.
    pure = (nv/'alsteppedanim.cpp').read_text(encoding='utf-8')
    source += pure.replace('#include "llviewerprecompiledheaders.h"','') + '\n'
    for sig in ('bool rotEqual(', 'bool vecEqual(', 'void ALSteppedAnimRuntime::readPose(',
                'ALSteppedAnim::LimbFrame ALSteppedAnimRuntime::limbFrame(',
                'void ALSteppedAnimRuntime::updateEaseCandidate(', 'void ALSteppedAnimRuntime::capturePose(',
                'void ALSteppedAnimRuntime::applyEffects(', 'void ALSteppedAnimRuntime::writeOverlay(',
                'void ALSteppedAnimRuntime::applyHeld(', 'void ALSteppedAnimRuntime::restoreOverlay(',
                'void ALSteppedAnimRuntime::endFrameRestore('):
        source += method(nv/'alsteppedanimruntime.cpp',sig) + '\n'
    source += r'''
// The pointer adapter preserves the null/notNull API of the particle lists.
template<class T>struct Ptr {T* p=nullptr;Ptr(){}Ptr(T* v):p(v){}T* operator->(){return p;}bool notNull(){return p;}bool isNull(){return !p;}};
struct LLViewerPartSource {LLViewerObject* mSourceObjectp;LLVector3 mPosAgent;int emissions=0;bool isDead(){return false;}void update(float){++emissions;mPosAgent=mSourceObjectp->getRenderPosition();}};
struct LLPartData{enum{LL_PART_FOLLOW_SRC_MASK=1};};
struct LLViewerPart{int mFlags=1;Ptr<LLViewerPartSource>mPartSourcep;LLVector3 mPosAgent,mPosOffset;};
struct LLViewerPartGroup{std::vector<LLViewerPart*>mParticles;};
struct LLViewerPartSim {
 struct StepAnimDeferredSource{Ptr<LLViewerPartSource>mSource;float mDelta=0;};
 std::vector<StepAnimDeferredSource>mStepAnimDeferredSources;
 std::vector<Ptr<LLViewerPartSource>>mViewerPartSources;std::vector<LLViewerPartGroup*>mViewerPartGroups;
 static LLViewerPartSim& instance(){static LLViewerPartSim s;return s;}
 void refreshStepAnimAttachmentSourcePositions(bool=false);
};
#define LLPointer Ptr
'''
    source += method(nv/'llviewerpartsim.cpp','void LLViewerPartSim::refreshStepAnimAttachmentSourcePositions(') + '\n'
    app = (nv/'llappviewer.cpp').read_text(encoding='utf-8')
    start = app.rindex('    if (ALSteppedAnimRuntime::sActive)',0,app.index('// HOT_GAZE_PRESENT_BEGIN'))
    end = app.index('    // HOT_GAZE_PRESENT_END',start)
    source += 'void presentFrame(){\n' + app[start:end] + '}\n'
    start = app.index('        LLActorMover::instance().restoreHotGazes();')
    end = app.index('\n\n',start)
    ordinary = next(line for line in app.splitlines() if line.strip() == 'ALGhostStudio::instance().updatePostObjectList();')
    source += 'void copyOrdinaryStudio(){\n' + ordinary + '\n}\n'
    source += 'void finishFrame(){\n' + app[start:end] + '}\n'
    avatar = (nv/'llvoavatar.cpp').read_text(encoding='utf-8')
    line = next(line for line in avatar.splitlines() if 'restoreHotGazePose(this)' in line)
    source += 'void LLVOAvatar::frameStartRestore(){\n LLActorMover& actor_mover=LLActorMover::instance();\n' + line + '\n}\n'
    return source


def assertions():
    return r'''
 // R2-3: acquire, park/recover, then the actual updateCharacter restore call
 // followed by the real painter. Compare with an uninterrupted control motor.
 mover.clearHotGazes();target.mMode=LLActorMover::GazeTarget::OBJECT;target.mObjectRef=object.id;
 target.mPickedSpot=false;object.position={40,36,1};
 LLVOAvatar control,recovered;control.id=actor.id; // identical deterministic motor seed
 auto runAcquire=[&](bool rekey){
   mover.clearHotGazes();now=50;actor.getRootJoint()->position=recovered.getRootJoint()->position={30,30,1};actor.getRootJoint()->rotation={};
   for(auto& j:actor.joints)j.second.rotation={};for(auto& j:recovered.joints)j.second.rotation={};
   cast.bodies[actor.id]=&actor;
   mover.fireHotGaze(actor.id,0,target,.8f,4,1,true,true,1,1,.2f);
   std::vector<LLQuaternion>trajectory;
   LLVOAvatar* body=&actor;double originalEnd=0;
   for(int frame=0;frame<=50;++frame){
     now=50+frame/60.;LLFrameTimer::updateFrameCount();body->frameStartRestore();
     if(rekey&&frame==17){
       const auto& before=mover.mHotGazes.at(body->id).mPaint.mGaze.mGazeMotor;
       originalEnd=ALTrajectory::programEndTime(before.mChannels[ALGazeMotor::CH_HEAD_YAW].mProgram);
       auto park=LLUUID::generateNewID();mover.rewriteHotGazeReferences(body->id,park);
       mover.rewriteHotGazeReferences(park,recovered.id);cast.bodies[recovered.id]=&recovered;body=&recovered;
       body->frameStartRestore(); // the formerly destructive first owner bind
       const auto& bound=mover.mHotGazes.at(body->id).mPaint.mGaze.mGazeMotor;
       require(bound.mInitialized,"recovered restore erased acquisition motor");
       require(ALTrajectory::programEndTime(bound.mChannels[ALGazeMotor::CH_HEAD_YAW].mProgram)==originalEnd,"recovered restore shortened trajectory");
     }
     mover.applyHotGazes();trajectory.push_back(body->getJoint("mHead")->getRotation());
     if(rekey&&frame>=17&&frame<48)
       require(ALTrajectory::programEndTime(mover.mHotGazes.at(body->id).mPaint.mGaze.mGazeMotor.mChannels[ALGazeMotor::CH_HEAD_YAW].mProgram)==originalEnd,"recovered paint restarted trajectory");
   }
   mover.clearHotGazes();return trajectory;
 };
 const auto uninterrupted=runAcquire(false),resumed=runAcquire(true);
 for(size_t i=0;i<resumed.size();++i)
   require(angular(LLVector3::x_axis*uninterrupted[i],LLVector3::x_axis*resumed[i])<.03f,"second acquisition whip after recovery");
 // R2-1/R2-4: production Studio deliberately receives reverse instance order.
 // Source-only hot must reach True Mirror, Reflect and mixed chains, while
 // real Stepped capture/held/write/restore never retains any hot rotations.
 auto& studio=ALGhostStudio::instance();auto& step=ALSteppedAnimRuntime::instance();auto& particles=LLViewerPartSim::instance();
 for(bool stepped:{false,true})for(bool reflected:{false,true})for(bool solo:{false,true})for(int finalKind:{0,1,2}){
   mover.clearHotGazes();step.mEntries.clear();step.mPlanned.clear();studio.mInstances.clear();studio.clones.clear();
   LLGhostAvatar clone,chain;clone.mAnimationSourceId=actor.id;chain.mAnimationSourceId=clone.id;
   clone.mEntityDriveMode=reflected?4:3;chain.mEntityDriveMode=finalKind==1?4:3;
   for(auto* body:{static_cast<LLVOAvatar*>(&actor),static_cast<LLVOAvatar*>(&clone),static_cast<LLVOAvatar*>(&chain)}){
     cast.bodies[body->id]=body;gObjectList.objects[body->id]=body;
     for(auto& j:body->joints)j.second.rotation={};
     step.mPlanned.push_back(body->id);
   }
   studio.clones[clone.id]=&clone;studio.clones[chain.id]=&chain;
   ALGhostStudio::Instance first,last;first.mId=LLUUID::generateNewID();first.mEntityId=clone.id;first.mDriveMode=clone.mEntityDriveMode;last.mId=LLUUID::generateNewID();last.mEntityId=chain.id;
   first.mVisible=!solo;
   if(finalKind==2){last.mKind=0;last.mOverlayAvatar=chain.id;last.mEntityId.setNull();}
   studio.mInstances={last,first}; // verifies the real dependency sort
   step.sActive=stepped;step.mActiveActors=stepped?3:0;
   if(stepped)for(auto id:step.mPlanned){auto& e=step.mEntries[id];
     e.mAssignment.stepped=true;e.mAssignment.heldClasses=ALSteppedAnim::JC_GAZE|ALSteppedAnim::groupBit(ALSteppedAnim::PART_HEAD);
     e.mDecision.capture=true;e.mDecision.captureGroups=ALSteppedAnim::kGroupBits;
   }
   // An emitter on the source head and one on the inherited chain attachment.
   LLViewerObject emitter,chainEmitter;emitter.ancestor=&actor;chainEmitter.ancestor=&chain;
   emitter.attachmentJoint=actor.getJoint("mHead");chainEmitter.attachmentJoint=chain.getJoint("mHead");
   LLViewerPartSource src{&emitter},chainSrc{&chainEmitter};LLViewerPart follower,chainFollower;
   follower.mPartSourcep=&src;chainFollower.mPartSourcep=&chainSrc;follower.mPosOffset={.2f,0,0};chainFollower.mPosOffset=follower.mPosOffset;
   LLViewerPartGroup group;group.mParticles={&follower,&chainFollower};particles.mViewerPartSources={&src,&chainSrc};particles.mViewerPartGroups={&group};
   now=70;mover.fireHotGaze(actor.id,0,target,0,4,1,true,true,1,1,.2f);
   for(int frame=0;frame<4;++frame){
     now=70+frame/60.;LLFrameTimer::updateFrameCount();actor.frameStartRestore();clone.frameStartRestore();chain.frameStartRestore();
     actor.getJoint("mWristLeft")->rotation.setEulerAngles(0,0,.1f*frame);
     if(solo&&frame==2)studio.mInstances[1].mVisible=true; // unsolo without re-driving
     copyOrdinaryStudio();
     std::map<LLUUID,LLQuaternion> baseHead;
     for(auto* body:{static_cast<LLVOAvatar*>(&actor),static_cast<LLVOAvatar*>(&clone),static_cast<LLVOAvatar*>(&chain)})baseHead[body->id]=body->getJoint("mHead")->getRotation();
     // World particle update emits once. Stepped's existing pass consumes its
     // deferred source once, before the position-only final hot repair.
     if(stepped){particles.mStepAnimDeferredSources={{&src,.016f},{&chainSrc,.016f}};}
     else{src.update(.016f);chainSrc.update(.016f);}
     presentFrame();
     const auto sourceHead=actor.getJoint("mHead")->getRotation();
     require(sourceHead!=LLQuaternion(),"source hot absent at Studio copy");
     ALReflect::LocalPose expected;ALReflect::reflectLocalPose({sourceHead,LLVector3::zero},expected);
     require(angular(LLVector3::x_axis*clone.getJoint("mHead")->getRotation(),LLVector3::x_axis*(reflected?expected.rotation:sourceHead))<.03f,"source-only hot missing on clone");
     const auto expectedChain=finalKind==1?ALReflect::rotation(clone.getJoint("mHead")->getRotation()):clone.getJoint("mHead")->getRotation();
     require(angular(LLVector3::x_axis*chain.getJoint("mHead")->getRotation(),LLVector3::x_axis*expectedChain)<.03f,"source-only hot missing through solo-hidden intermediate");
     require(studio.mInstances[1].mVisible==(!solo||frame>=2),"hot propagation changed render-only solo visibility");
     for(const char* eye:{"mEyeLeft","mEyeRight"}){
       const std::string sourceName=reflected?ALReflect::partner(eye):std::string(eye);
       const auto sourceEye=actor.getJoint(sourceName)->getRotation();
       ALReflect::LocalPose reflectedEye;ALReflect::reflectLocalPose({sourceEye,LLVector3::zero},reflectedEye);
       require(angular(LLVector3::x_axis*clone.getJoint(eye)->getRotation(),LLVector3::x_axis*(reflected?reflectedEye.rotation:sourceEye))<.03f,"source hot eye missing on clone");
       const auto chainEye=finalKind==1?ALReflect::rotation(clone.getJoint(ALReflect::partner(eye))->getRotation()):clone.getJoint(eye)->getRotation();
       require(angular(LLVector3::x_axis*chain.getJoint(eye)->getRotation(),LLVector3::x_axis*chainEye)<.03f,"source hot eye missing on clone chain");
     }
     for(auto* p:{&follower,&chainFollower}){
       auto& s=*p->mPartSourcep.p;
       require((s.mPosAgent-s.mSourceObjectp->getRenderPosition()).length()<.0001f,"hot attachment particle source stale");
       require((p->mPosAgent-s.mPosAgent-p->mPosOffset).length()<.0001f,"hot follow-source particle stale");
       require(s.emissions==frame+1,"hot repair emitted twice");
     }
     // A late hot refresh also must not drain still-deferred simulation work.
     particles.mStepAnimDeferredSources={{&src,.016f}};
     particles.refreshStepAnimAttachmentSourcePositions(true);
     require(src.emissions==frame+1,"hot repair consumed deferred emission");
     require(particles.mStepAnimDeferredSources.size()==1,"hot repair discarded deferred source");
     particles.mStepAnimDeferredSources.clear();
     if(stepped)for(auto& pair:step.mEntries){
       auto* body=cast.resolve(pair.first);auto& bones=body->getSkeleton();
       for(size_t i=0;i<bones.size();++i){
         if(bones[i]!=body->getJoint("mWristLeft")&&bones[i]!=body->getJoint("mWristRight"))require(angular(LLVector3::x_axis*pair.second.mHeld.joints[i].rot,LLVector3::x_axis)<.03f,"Stepped captured hot pose");
         if(bones[i]->getName()=="mWristLeft")require(bones[i]->getRotation()==pair.second.mWritten.joints[i].rot,"hot copy overwrote Stepped limb hold");
       }
     }
     finishFrame();
     for(auto* body:{static_cast<LLVOAvatar*>(&actor),static_cast<LLVOAvatar*>(&clone),static_cast<LLVOAvatar*>(&chain)})
       require(body->getJoint("mHead")->getRotation()==baseHead[body->id],"inherited hot contaminated next frame");
     if(stepped)for(auto& pair:step.mEntries){const bool capture=pair.first==actor.id||frame%2==0;pair.second.mDecision.capture=capture;pair.second.mDecision.captureGroups=capture?ALSteppedAnim::kGroupBits:0;}
   }
   mover.clearHotGazes();step.sHasOverlay=false;step.mEntries.clear();step.mPlanned.clear();studio.mInstances.clear();studio.clones.clear();
   cast.bodies.erase(clone.id);cast.bodies.erase(chain.id);gObjectList.objects.erase(clone.id);gObjectList.objects.erase(chain.id);
   particles.mViewerPartSources.clear();particles.mViewerPartGroups.clear();
 }

 // Integration: render-only visibility preserves inheritance; delay, held helper
 // poses and deletion, while live private helpers inherit and unwind normally.
 for(int policy=0;policy<6;++policy){
   mover.clearHotGazes();step.sActive=false;studio.mInstances.clear();
   LLGhostAvatar clone;clone.mAnimationSourceId=actor.id;
   cast.bodies[clone.id]=&clone;gObjectList.objects[clone.id]=&clone;
   ALGhostStudio::Instance inst;inst.mId=LLUUID::generateNewID();inst.mEntityId=clone.id;
   if(policy==0)inst.mVisible=false;
   if(policy==1){inst.mStartOffset=.5f;clone.mStartOffset=.5f;}
   if(policy==2){inst.mKind=0;inst.mOverlayAvatar=clone.id;inst.mPose=1;}
   if(policy==3)inst.mDeleting=true;
   if(policy==4){inst.mKind=0;inst.mOverlayAvatar=clone.id;}
   studio.mInstances={inst};
   now=80;mover.fireHotGaze(actor.id,0,target,0,4,1,true,true,1,1,.2f);
   const auto base=clone.getJoint("mHead")->getRotation();
   copyOrdinaryStudio();presentFrame();
   require((mover.mHotGazeCopies.count(clone.id)!=0)==(policy==0||policy==1||policy>=4),"QoL late-copy eligibility lost");
   if(policy>=1&&policy<4)require(clone.getJoint("mHead")->getRotation()==base,"late copy overwrote delayed/held/retiring pose");
   else require(clone.getJoint("mHead")->getRotation()==actor.getJoint("mHead")->getRotation(),"private helper hot pose absent");
   LLVOAvatar attached;studio.owners[attached.id]=actor.id;
   require(mover.hasHotGazePose(&attached),"hot affected filter lost wearer ownership");
   studio.owners.erase(attached.id);
   require(!mover.hasHotGazePose(&attached),"hot affected filter leaked across independent bodies");
   finishFrame();require(clone.getJoint("mHead")->getRotation()==base,"helper restore lost delayed base");
   mover.clearHotGazes();studio.mInstances.clear();
   cast.bodies.erase(clone.id);gObjectList.objects.erase(clone.id);
 }


 // P2 offset: run real 30 Hz history over 60 Hz frames, including a short
 // source reaction that expires before the clone first presents it.
 for(float offset:{0.f,.5f})for(bool reflected:{false,true})for(bool stepped:{false,true})for(bool fresh:{false,true}){
   mover.clearHotGazes();step.sActive=stepped;step.mEntries.clear();step.mPlanned.clear();studio.mInstances.clear();studio.clones.clear();
   LLGhostAvatar delayed,tail;delayed.mAnimationSourceId=actor.id;tail.mAnimationSourceId=delayed.id;
   delayed.mEntityDriveMode=reflected?4:3;delayed.mStartOffset=offset;
   ALGhostStudio::Instance first,last;first.mId=LLUUID::generateNewID();first.mEntityId=delayed.id;first.mStartOffset=offset;first.mDriveMode=delayed.mEntityDriveMode;
   last.mId=LLUUID::generateNewID();last.mEntityId=tail.id;studio.mInstances={last,first};
   studio.clones[delayed.id]=&delayed;studio.clones[tail.id]=&tail;
   for(auto* body:{static_cast<LLVOAvatar*>(&actor),static_cast<LLVOAvatar*>(&delayed),static_cast<LLVOAvatar*>(&tail)}){
     cast.bodies[body->id]=body;gObjectList.objects[body->id]=body;for(auto& j:body->joints)j.second.rotation={};
     if(stepped){step.mPlanned.push_back(body->id);auto& e=step.mEntries[body->id];e.mAssignment.stepped=true;e.mAssignment.heldClasses=ALSteppedAnim::JC_GAZE|ALSteppedAnim::groupBit(ALSteppedAnim::PART_HEAD);e.mDecision.capture=true;e.mDecision.captureGroups=ALSteppedAnim::kGroupBits;}
   }
   step.mActiveActors=stepped?3:0;
   const int start=fresh?0:48;bool sawDelayed=false,sawTailAfterSource=false,froze=false;double paused=0;LLQuaternion expectedReaction;
   for(int frame=0;frame<start+100;++frame){
     now=110+frame/60.+paused;LLFrameTimer::updateFrameCount();
     actor.frameStartRestore();delayed.frameStartRestore();tail.frameStartRestore();delayed.restoreOffsetPose();
     if(frame==start)mover.fireHotGaze(actor.id,0,target,0,4,.1f,true,true,1,1,.2f);
     if(frame==start+12)mover.releaseHotGazeSlot(0);
     actor.getJoint("mWristLeft")->rotation.setEulerAngles(0,0,.003f*frame);
     copyOrdinaryStudio();const auto base=delayed.getJoint("mHead")->getRotation();const auto wrist=delayed.getJoint("mWristLeft")->getRotation();
     const int poseBefore=delayed.poseChanges,skinBefore=delayed.skinChanges,syncBefore=delayed.attachmentSyncs;
     const bool captured=offset>0&&delayed.mOffsetHistory.back().time==now;
     const auto priorInput=offset>0?delayed.mOffsetHotRotations.find(delayed.mOffsetHistory.back().time):delayed.mOffsetHotRotations.end();
     const auto priorHot=priorInput!=delayed.mOffsetHotRotations.end()?priorInput->second:std::map<size_t,LLQuaternion>();
     presentFrame();const auto shown=delayed.getJoint("mHead")->getRotation();
     const auto afterInput=offset>0?delayed.mOffsetHotRotations.find(delayed.mOffsetHistory.back().time):delayed.mOffsetHotRotations.end();
     const auto afterHot=afterInput!=delayed.mOffsetHotRotations.end()?afterInput->second:std::map<size_t,LLQuaternion>();
     if(offset>0&&!captured)require(afterHot==priorHot,"late hot rewrote an older 30 Hz sample");
     require(delayed.mOffsetHotRotations.size()<=delayed.mOffsetHistory.size(),"hot side history exceeded ordinary history bound");
     const double elapsed=(frame-start)/60.;
     if(frame==start+6)expectedReaction=reflected?ALReflect::rotation(actor.getJoint("mHead")->getRotation()):actor.getJoint("mHead")->getRotation();
     if(offset>0&&frame==start+36)require(angular(LLVector3::x_axis*shown,LLVector3::x_axis*expectedReaction)<.05f,"delayed hot lost source/Reflect pose");
     if(offset==0){
       const auto expected=reflected?ALReflect::rotation(actor.getJoint("mHead")->getRotation()):actor.getJoint("mHead")->getRotation();
       require(angular(LLVector3::x_axis*shown,LLVector3::x_axis*expected)<.03f,"offset zero changed hot inheritance");
       require(delayed.mOffsetHistory.empty()&&delayed.mOffsetHotRotations.empty()&&!delayed.hasOffsetHotGaze(),"zero offset allocated hot history");
     }else{
       if(elapsed<offset-1.e-6)require(angular(LLVector3::x_axis*shown,LLVector3::x_axis*base)<.03f,"inherited hot appeared before offset");
       if(elapsed>offset+.06&&elapsed<offset+.17){require(angular(LLVector3::x_axis*shown,LLVector3::x_axis*base)>1,"delayed inherited reaction never appeared");sawDelayed=true;}
       if(elapsed>offset+.5)require(angular(LLVector3::x_axis*shown,LLVector3::x_axis*base)<.03f,"delayed reaction failed to return");
       require(angular(LLVector3::x_axis*delayed.mOffsetRestore.rotations[3],LLVector3::x_axis)<.03f,"hot contaminated animation input restore");
       for(const auto& input:delayed.mOffsetHistory)require(angular(LLVector3::x_axis*input.rotations[3],LLVector3::x_axis)<.03f,"hot contaminated ordinary history");
       if(sawDelayed&&!mover.hasHotGazes())sawTailAfterSource=true;
     }
     if(shown!=base)require(delayed.poseChanges>poseBefore&&delayed.skinChanges>skinBefore&&delayed.attachmentSyncs>syncBefore&&delayed.attachmentHead==shown,"final delayed hot pose was not published to skin/attachment consumers");
     require(shown==tail.getJoint("mHead")->getRotation(),"delayed source tail missing on immediate descendant");
     require(delayed.getJoint("mWristLeft")->getRotation()==wrist,"delayed hot changed non-gaze held pose");
     // Freeze while the delayed reaction is visible. It holds the ordinary
     // delayed image; transient hot cannot become the frozen input snapshot.
     if(offset>0&&!froze&&elapsed>offset+.1){
       require(shown!=base,"Freeze fixture missed delayed hot");
       delayed.prepareOffsetDrive(2);delayed.mEntityDriveMode=2;
       require(delayed.getJoint("mHead")->getRotation()==base,"Freeze latched transient delayed hot");
       const auto count=delayed.mOffsetHistory.size();finishFrame();
       now+=.2;LLFrameTimer::updateFrameCount();copyOrdinaryStudio();presentFrame();
       require(delayed.getJoint("mHead")->getRotation()==base&&delayed.mOffsetHistory.size()==count,"frozen delayed history or pose changed");finishFrame();
       delayed.prepareOffsetDrive(reflected?4:3);delayed.mEntityDriveMode=reflected?4:3;paused+=.2;froze=true;
     }else finishFrame();
     require(delayed.getJoint("mHead")->getRotation()==base,"delayed hot unwind lost presented base");
   }
   if(offset>0)require(sawDelayed&&sawTailAfterSource&&froze&&!delayed.hasOffsetHotGaze(),"delayed lifecycle did not complete");
   mover.clearHotGazes();step.sHasOverlay=false;step.mEntries.clear();step.mPlanned.clear();studio.mInstances.clear();studio.clones.clear();
   for(auto* body:{&delayed,&tail}){cast.bodies.erase(body->id);gObjectList.objects.erase(body->id);}
 }

 // R3-1: real pose-flip publication and restoration, source-only and chain.
 // Nonzero animated bases expose even the zero-weight acquisition copy.
 for(bool chained:{false,true}){
   mover.clearHotGazes();step.sActive=false;studio.mInstances.clear();studio.clones.clear();
   LLGhostAvatar clone,chain;clone.mAnimationSourceId=actor.id;chain.mAnimationSourceId=clone.id;
   clone.mDirectedMirror=chain.mDirectedMirror=true;
   ALGhostStudio::Instance first,last;first.mId=LLUUID::generateNewID();first.mEntityId=clone.id;first.mDirectedMirror=true;
   last.mId=LLUUID::generateNewID();last.mEntityId=chain.id;last.mDirectedMirror=true;
   studio.clones[clone.id]=&clone;studio.clones[chain.id]=&chain;
   studio.mInstances={first};if(chained)studio.mInstances.push_back(last);
   for(auto* body:{static_cast<LLVOAvatar*>(&actor),static_cast<LLVOAvatar*>(&clone),static_cast<LLVOAvatar*>(&chain)}){
     cast.bodies[body->id]=body;gObjectList.objects[body->id]=body;
     for(auto& j:body->joints)j.second.rotation={};
   }
   now=90;mover.fireHotGaze(actor.id,0,target,.5f,4,.5f,true,true,1,1,.2f);
   for(double elapsed:{0.,.25,.6,.8,1.05,1.3,1.31}){
     now=90+elapsed;LLFrameTimer::updateFrameCount();
     actor.frameStartRestore();clone.frameStartRestore();chain.frameStartRestore();
     clone.restoreDirectedMirrorPose(false);chain.restoreDirectedMirrorPose(false);
     actor.getJoint("mHead")->rotation.setEulerAngles(0,0,30*DEG_TO_RAD);
     actor.getJoint("mWristLeft")->rotation.setEulerAngles(0,0,.4f);
     actor.getJoint("mWristRight")->rotation.setEulerAngles(0,0,-.2f);
     if(elapsed==.8)mover.releaseHotGazeSlot(0);
     copyOrdinaryStudio();
     const auto base=clone.getJoint("mHead")->getRotation(),chainBase=chain.getJoint("mHead")->getRotation();
     const auto wrist=clone.getJoint("mWristLeft")->getRotation();
     require(angular(LLVector3::x_axis*base,LLVector3::x_axis*actor.getJoint("mHead")->getRotation())>50,"flip fixture did not flip animated base");
     presentFrame();
     if(elapsed==0.)require(mover.mHotGazes.at(actor.id).mPaint.mGaze.mEnv==0,"acquire boundary not zero weight");
     const auto expected=ALReflect::rotation(actor.getJoint("mHead")->getRotation());
     require(angular(LLVector3::x_axis*clone.getJoint("mHead")->getRotation(),LLVector3::x_axis*expected)<.03f,"enabled flip lost inherited hot composition");
     for(const char* eye:{"mEyeLeft","mEyeRight"}){
       const auto expectedEye=ALReflect::rotation(actor.getJoint(ALReflect::partner(eye))->getRotation());
       require(angular(LLVector3::x_axis*clone.getJoint(eye)->getRotation(),LLVector3::x_axis*expectedEye)<.03f,"enabled flip lost inherited eye composition");
     }
     if(chained)require(angular(LLVector3::x_axis*chain.getJoint("mHead")->getRotation(),LLVector3::x_axis*actor.getJoint("mHead")->getRotation())<.03f,"enabled flip chain lost composition");
     require(clone.getJoint("mWristLeft")->getRotation()==wrist,"late flip changed non-gaze pose");
     if(elapsed>=1.3){
       require(mover.mHotGazes.empty(),"return boundary not zero weight");
       require(clone.getJoint("mHead")->getRotation()==base,"zero-weight return changed flipped base");
     }
     finishFrame();
     require(clone.getJoint("mHead")->getRotation()==base,"flip inherited unwind lost ordinary base");
     if(chained)require(chain.getJoint("mHead")->getRotation()==chainBase,"flip chain unwind lost ordinary base");
   }
   mover.clearHotGazes();studio.mInstances.clear();studio.clones.clear();
   for(auto* body:{&clone,&chain}){cast.bodies.erase(body->id);gObjectList.objects.erase(body->id);}
 }
 step.sActive=false;mover.clearHotGazes();
 ALGhostStudio::Instance idle;idle.mId=LLUUID::generateNewID();studio.mInstances={idle};
 const int resolves=gObjectList.queries;
 particles.mViewerPartSources={nullptr}; // any inactive particle scan would dereference this
 presentFrame();
 require(gObjectList.queries==resolves,"empty hot frame scanned Studio clones");
 particles.mViewerPartSources.clear();studio.mInstances.clear();
 std::cout<<"PASS: complete production gazePaint, ownership, frame ordering, Studio True Mirror/Reflect chains, Stepped capture/restore, recovered acquire, single-emission particle followers (Stepped ON/OFF), real enabled pose-flip source/chain acquire/return boundaries; solo-hidden sources + unsolo + Reflect/private descendants; real offset history timing/return/Freeze/expired-source tails/zero offset @60Hz/30Hz\n";
'''
