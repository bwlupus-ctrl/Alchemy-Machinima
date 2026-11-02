"""Run production hot runtime methods with real gaze math and small world adapters.

The world/joint store and the ordinary gazePaint shell are adapters. The actual
hot dispatcher, live resolver, layer capture/restore, rekey, motor and eye solver
are executed. Mutation controls are compiled and must fail at runtime.
"""
from pathlib import Path
from relight_area_policy_check import method
from native_check_tools import ROOT, check_directory, compile_run, viewer_native_flags

NV = ROOT / 'indra/newview'

def fixture():
    source = r'''
#include "linden_common.h"
#include "v3dmath.h"
#include "llsdutil_math.h"
#include "algazehot.h"
#include <cassert>
#include <map>
#include <iostream>
constexpr int GAZE_PRIORITY_BLEND=0, GAZE_PRIORITY_UPPER_BODY=2, GAZE_PRIORITY_PLANTED_SPINE=3;
constexpr int GAZE_POINT=3, GAZE_COMPOSE_REPLACE=0;
constexpr float GAZE_MIN_DIST=.25f;
struct LLJoint {
 LLQuaternion rotation; LLVector3 position; LLJoint* parent=nullptr;
 LLJoint* getParent(){return parent;}
 LLQuaternion getRotation()const{return rotation;}
 LLQuaternion getWorldRotation()const{return parent ? rotation * parent->getWorldRotation() : rotation;}
 LLVector3 getWorldPosition()const{return parent ? position * parent->getWorldRotation() + parent->getWorldPosition() : position;}
 void setRotation(LLQuaternion q){rotation=q;} void updateWorldMatrixChildren(){}
};
struct LLVisualParam{float weight=.25f;};
struct Region{bool pointInRegionGlobal(const LLVector3d& p){return p.mdV[0]>=0 && p.mdV[0]<256 && p.mdV[1]>=0 && p.mdV[1]<256;}} region;
struct LLVOAvatar;
struct LLViewerObject {
 LLUUID id=LLUUID::generateNewID(); bool dead=false; Region* regionPtr=&region;
 LLVOAvatar *avatar=nullptr,*ancestor=nullptr,*control=nullptr; LLViewerObject* root=nullptr;
 LLVector3 position; LLQuaternion rotation;
 bool isDead()const{return dead;} LLUUID getID()const{return id;}
 Region* getRegion(){return regionPtr;} LLVOAvatar* asAvatar(){return avatar;}
 LLVOAvatar* getAvatarAncestor(){return ancestor;} LLVOAvatar* getControlAvatar(){return control;}
 LLViewerObject* getRootEdit(){return root?root:this;}
 LLVector3 getPositionAgent(){return position;} LLVector3 getRenderPosition(){return position;} LLQuaternion getRenderRotation(){return rotation;}
};
struct LLVOAvatar:LLViewerObject {
 float uniformScale=1;float getUniformScale(){return uniformScale;}float getPelvisToFoot(){return 1;}
 std::map<std::string,LLJoint> joints;std::map<std::string,LLVisualParam> params;
 LLVOAvatar(){avatar=this;for(auto n:{"mRoot","mHead","mNeck","mTorso","mChest","mEyeLeft","mEyeRight"})joints[n]={};
 joints["mTorso"].parent=&joints["mRoot"];joints["mNeck"].parent=&joints["mTorso"];joints["mHead"].parent=&joints["mNeck"];
 joints["mEyeLeft"].parent=joints["mEyeRight"].parent=&joints["mHead"];
 joints["mRoot"].position={30,30,1};params["Blink_Left"]={};}
 LLJoint* getJoint(std::string n){auto i=joints.find(n);return i==joints.end()?nullptr:&i->second;}
 LLJoint* getRootJoint(){return getJoint("mRoot");}
 bool isGhostAvatar(){return false;} int getSkeletonSerialNum(){return 1;}int getAppearanceSerialNum(){return 1;}
 LLVisualParam* getVisualParam(const char* n){auto i=params.find(n);return i==params.end()?nullptr:&i->second;}
 float getVisualParamWeight(LLVisualParam* p){return p->weight;}bool setVisualParamWeight(LLVisualParam* p,float v){p->weight=v;return true;}
 void updateVisualParams(){}void markStepAnimPoseChanged(){}void syncStepAnimAttachmentMatrices(){}
 void invalidateSkinPalettesForOverlay(){}void invalidateSkinPalettesAfterOverlay(){}
};
''' + method(NV/'llactormover.cpp','LLVector3 gazeRenderedJointPosition(') + r'''
struct Agent {Region* getRegion(){return &region;} LLVector3 getPosAgentFromGlobal(LLVector3d p){return LLVector3(p);}
 LLVector3d getPosGlobalFromAgent(LLVector3 p){return LLVector3d(p);}} gAgent;
struct ObjectList {std::map<LLUUID,LLViewerObject*> objects; LLViewerObject* findObject(LLUUID id){auto i=objects.find(id);return i==objects.end()?nullptr:i->second;}}gObjectList;
struct LLViewerCamera {LLVector3 origin{40,30,1};static LLViewerCamera* getInstance(){static LLViewerCamera c;return &c;}LLVector3 getOrigin(){return origin;}};
struct LLDirectorCast {std::map<LLUUID,LLVOAvatar*> bodies;static LLDirectorCast& instance(){static LLDirectorCast c;return c;}
 LLVOAvatar* resolve(LLUUID id){auto i=bodies.find(id);return i==bodies.end()?nullptr:i->second;}};
struct LLPresentationTime {struct Frame{double presentation_time=0;};static Frame& currentFrame(){static Frame f;return f;}static float presentationDelta(){return 1.f/60;}};
struct LLFrameTimer {static U32 getFrameCount(){return U32(LLPresentationTime::currentFrame().presentation_time*60);}};
int currentGazePriority(){return 0;}
bool gazeCameraSafe(LLVOAvatar*,bool){return true;}
struct LLActorMover {
'''
    source += method(NV/'llactormover.h', '    struct GazeTarget\n') + ';\n'
    source += r'''
 struct Gaze {
 ALGazeMotor::GazeMotorState mGazeMotor;bool mChainYawValid=false,mBehindSweeping=false;
 F32 mChainYaw=0,mBehindShoulderSign=0;U32 mHotGeneration=0,mLastFrame=0;bool mHotFixated=false;
 F32 mHotSnap=-1,mTorsoAmount=0,mEnv=0,mIntensity=1,mHeadEyeBlend=1,mSmoothing=0;
 bool mEnabled=false,mDirValid=false,mBodyAimValid=false,mAppliedValid=false;
 S32 mTarget=0,mGazePriorityOverride=0,mCompositionOverride=0,mAnimPriorityOverride=-1;
 LLUUID mCastTarget,mObjectTarget;LLVector3d mPoint;
 F32 mMicroLifeOverride=0,mBreakFrequencyOverride=0,mVariationOverride=0,mDeadZoneDegOverride=0;S32 mBlinksOverride=0;
 struct Clock{double mTime=0;}mAnimationClock,mLifeClock;
 int mHeldSkeletonSerial=1,mHeldAppearanceSerial=1,mWrittenGaze=0;
'''
    source += method(NV/'llactormover.h','        void resetHotAim()') + '\n};\n'
    source += r'''
 struct DirectorJointPose {bool mValid=false;LLQuaternion mRotation;};
 struct DirectorVisualParamPose {bool mValid=false;float mWeight=0;};
 struct DirectorGaze {int mMode=0;Gaze mGaze;bool mCueOverride=false;GazeTarget mCueFromTarget;
 F32 mCueTargetBlend=1,mCueEyeWeight=1,mCueHeadWeight=1,mCueBodyWeight=1;
 DirectorJointPose mRoot,mPelvis,mTorso,mChest,mNeck,mHead,mEyeLeft,mEyeRight,mAltEyeLeft,mAltEyeRight;
 DirectorVisualParamPose mBlinkLeft,mBlinkRight;};
'''
    source += method(NV/'llactormover.h','    struct HotGaze\n') + ';\n'
    source += r'''
 std::map<LLUUID,HotGaze> mHotGazes;
 static bool ensureDirectorOwner(LLVOAvatar*,DirectorGaze&){return false;} static void releaseDirectorTurnAO(DirectorGaze&){}
 void captureDirectorLookAtPose(LLVOAvatar*,DirectorGaze&,bool);void restoreDirectorLookAtPose(LLVOAvatar*,DirectorGaze&);
 bool resolveHotGazeTarget(LLVOAvatar*,const GazeTarget&,LLVector3d&);
 bool resolveGazeObjectCenter(const LLUUID& id,LLVector3& p){auto* o=gObjectList.findObject(id);if(!o||o->dead)return false;p=o->position;return true;}
 static bool rewriteGazeTarget(GazeTarget&,const LLUUID&,const LLUUID&);
 void fireHotGaze(const LLUUID&,S32,const GazeTarget&,F32,F32,F32,bool,bool,F32,F32,F32);
 void hotGazeInput(S32,bool);void releaseHotGazeSlot(S32);void restoreHotGazePose(LLVOAvatar*);void applyHotGaze(LLVOAvatar*);
 void clearHotGazes();void restoreHotGazes();void applyHotGazes();void rewriteHotGazeReferences(const LLUUID&,const LLUUID&);
 void gazePaint(LLVOAvatar*,Gaze&,void*,F32,bool,bool,bool,DirectorGaze*);
};
#define LL_LLACTORMOVER_H
#include "algazehotslots.h"
#define AL_HOT_GAZE_STANDALONE
'''
    source += (NV/'tests/algazehot_test.cpp').read_text(encoding='utf-8')
    for sig in ('bool LLActorMover::rewriteGazeTarget(', 'void LLActorMover::captureDirectorLookAtPose(',
                'void LLActorMover::restoreDirectorLookAtPose(LLVOAvatar* av, DirectorGaze& runtime)',
                'bool LLActorMover::resolveHotGazeTarget(', 'void LLActorMover::releaseHotGazeSlot(',
                'void LLActorMover::restoreHotGazes(', 'void LLActorMover::applyHotGazes(',
                'void LLActorMover::fireHotGaze(', 'void LLActorMover::hotGazeInput(',
                'void LLActorMover::restoreHotGazePose(', 'void LLActorMover::applyHotGaze(',
                'void LLActorMover::clearHotGazes(', 'void LLActorMover::rewriteHotGazeReferences('):
        source += method(NV/'llactormover.cpp',sig) + '\n'
    source += r'''
// Adapter for the normal gaze shell: run the same hot motor and post-head
// world eye solve, with real quaternions, limits and actual envelope weights.
void LLActorMover::gazePaint(LLVOAvatar* av,Gaze& g,void*,float,bool,bool,bool,DirectorGaze* paint){
 LLVector3 dir=gAgent.getPosAgentFromGlobal(g.mPoint)-av->getJoint("mHead")->getWorldPosition();dir.normalize();
 const LLQuaternion root=av->getRootJoint()->getWorldRotation();const LLVector3 local=dir*~root;
 ALGazeMotor::GazeMotorInput in;in.mTargetYaw=atan2f(local.mV[1],local.mV[0]);
 in.mTargetPitch=-atan2f(local.mV[2],sqrtf(local.mV[0]*local.mV[0]+local.mV[1]*local.mV[1]));
 in.mTimeSeconds=g.mAnimationClock.mTime;in.mDeltaTime=1.f/60;in.mRefWorldRot=root;
 in.mHeadWorldRot=av->getJoint("mHead")->getWorldRotation();in.mSettings.mMasterGate=true;
 in.mSettings.mHeadDurationBaseMs=g.mHotSnap*1000;in.mSettings.mHeadDurationMaxMs=g.mHotSnap*1000;
 in.mSettings.mHeadLatencyBaseMs=0;in.mSettings.mEyeDurationBaseMs=g.mHotSnap*500;
 in.mSettings.mEyeDriftAmpRad=in.mSettings.mHeadDriftAmpRad=in.mSettings.mMicrosaccadeAmpRad=0;
 ALGazeMotor::GazeMotorPose pose;ALGazeHot::stepMotor(g.mGazeMotor,in,g.mHotFixated,dir,pose);
 ALGazeMotor::GazeMotorPose face;ALGazeHot::recruitFace(g.mGazeMotor,in,av->getJoint("mTorso")->getWorldRotation(),face);
 const F32 weight=g.mEnv*paint->mCueHeadWeight;
 LLQuaternion head,neck;head.setEulerAngles(0,face.mHeadPitch,face.mHeadYaw);neck.setEulerAngles(0,face.mNeckPitch,face.mNeckYaw);
 av->getJoint("mNeck")->setRotation(nlerp(weight,av->getJoint("mNeck")->getRotation(),neck));
 av->getJoint("mHead")->setRotation(nlerp(weight,av->getJoint("mHead")->getRotation(),head));
 if(g.mHotFixated){pose.mDesiredWorldGaze=gAgent.getPosAgentFromGlobal(g.mPoint)-av->getJoint("mHead")->getWorldPosition();pose.mDesiredWorldGaze.normalize();}
 F32 yaw,pitch;
 if(g.mHotFixated) ALGazeHot::fixatedEyeInHead(pose.mDesiredWorldGaze,av->getJoint("mHead")->getWorldRotation(),
 in.mSettings.mComfortYawDeg,in.mSettings.mComfortPitchDeg,yaw,pitch);
 else ALGazePolicy::eyeInHeadFromWorldGaze(pose.mDesiredWorldGaze,av->getJoint("mHead")->getWorldRotation(),
 in.mSettings.mComfortYawDeg,in.mSettings.mComfortPitchDeg,yaw,pitch);
 LLQuaternion eyes;eyes.setEulerAngles(0,pitch,yaw);
 for(auto n:{"mEyeLeft","mEyeRight"})av->getJoint(n)->setRotation(nlerp(g.mEnv*paint->mCueEyeWeight,av->getJoint(n)->getRotation(),eyes));
}
float angular(LLVector3 a,LLVector3 b){a.normalize();b.normalize();return acosf(llclamp(a*b,-1.f,1.f))*RAD_TO_DEG;}
void require(bool p,const char* message){if(!p)throw std::runtime_error(message);}
int main(){try{
 hotGazeContract();
 LLActorMover mover;LLVOAvatar actor,targetActor;auto& cast=LLDirectorCast::instance();
 cast.bodies[actor.id]=&actor;cast.bodies[targetActor.id]=&targetActor;
 LLViewerObject object;object.position={40,30,1};gObjectList.objects[object.id]=&object;
 auto& now=LLPresentationTime::currentFrame().presentation_time;
 LLActorMover::GazeTarget target;target.mMode=LLActorMover::GazeTarget::OBJECT;target.mObjectRef=object.id;
 target.mPickedSpot=true;target.mObjectOffset={1,0,0};
 LLVector3d point;
 require(mover.resolveHotGazeTarget(&actor,target,point) && (LLVector3(point)-LLVector3(41,30,1)).length()<.0001f,"spot translation");
 object.rotation.setEulerAngles(0,0,F_PI_BY_TWO);
 require(mover.resolveHotGazeTarget(&actor,target,point) && (LLVector3(point)-LLVector3(40,31,1)).length()<.0001f,"spot rotation");
 // Head/eye target frame follows joint translation and rotation.
 gObjectList.objects[targetActor.id]=&targetActor;target.mObjectRef=targetActor.id;target.mObjectJoint="mHead";
 targetActor.getRootJoint()->position={45,30,1};targetActor.getJoint("mHead")->rotation=object.rotation;
 require(mover.resolveHotGazeTarget(&actor,target,point) && (LLVector3(point)-LLVector3(45,31,1)).length()<.0001f,"avatar joint frame");
 targetActor.uniformScale=2;
 require(mover.resolveHotGazeTarget(&actor,target,point) && (LLVector3(point)-LLVector3(45,32,2)).length()<.0001f,"scaled avatar joint frame");
 targetActor.uniformScale=1;
 target.mObjectRef=object.id;target.mObjectJoint.clear();
 for(bool stay:{true,false})for(float snap:{0.f,.5f}){
 now=10;object.position={40,30,1};actor.getRootJoint()->rotation={};
 mover.fireHotGaze(actor.id,0,target,snap,4,1,false,stay,1,1,.2f);
 for(int frame=0;frame<121;++frame){
 now=10+frame/60.;mover.restoreHotGazes();
 // Two lower layers: animation/Director followed by a stepped held base.
 LLQuaternion lower;lower.setEulerAngles(0,0,.7f);actor.getJoint("mHead")->rotation=lower;
 actor.getJoint("mTorso")->rotation.setEulerAngles(0,0,-.7f);
 actor.getRootJoint()->rotation.setEulerAngles(0,0,.1f*frame/120);
 actor.getRootJoint()->position.mV[0]=30+.5f*frame/120;
 const float travel=llclamp(float((now-11)/.5),0.f,1.f);
 object.position.mV[1]=29+10*tanf((30+10*travel)*DEG_TO_RAD); // ten degrees in half a second
 mover.applyHotGazes();
 if(frame>40){
 require(mover.resolveHotGazeTarget(&actor,target,point),"live target");
 const LLVector3 desired=LLVector3(point)-actor.getJoint("mHead")->getWorldPosition();
 const LLVector3 actual=LLVector3::x_axis*actor.getJoint("mEyeLeft")->getWorldRotation();
 if(angular(actual,desired)>=.05f)std::cerr<<"snap "<<snap<<" frame "<<frame<<" error "<<angular(actual,desired)<<" actual "<<actual<<" desired "<<desired<<'\n';
 require(angular(actual,desired)<.05f,"moving target + turning actor fixation error");
 }
 mover.restoreHotGazes();require(actor.getJoint("mHead")->rotation==lower,"restore hot to stepped/Director base");
 require(actor.params["Blink_Left"].weight==.25f,"hot must not capture blink");
 }
 mover.releaseHotGazeSlot(0);now+=1.1;mover.applyHotGazes();require(mover.mHotGazes.empty(),"return completion");
 }
 actor.getJoint("mTorso")->rotation={};
 // An overlay must actually change a joint even on a held frame between grid captures.
 actor.getRootJoint()->rotation={};actor.getJoint("mHead")->rotation={};now=30;
 mover.fireHotGaze(actor.id,1,target,0,.1f,0,false,false,1,1,.2f);mover.applyHotGazes();
 require(actor.getJoint("mHead")->rotation!=LLQuaternion(),"disabled overlay");
 mover.restoreHotGazes();require(actor.getJoint("mHead")->rotation==LLQuaternion(),"missing restore");
 now=31;mover.applyHotGazes();require(mover.mHotGazes.empty(),"short reaction gone between captures");
 // Refuse unresolved, foreign-region, self avatar, attachment and animesh targets.
 auto fire=[&]{mover.fireHotGaze(actor.id,2,target,0,2,1,true,true,1,1,.2f);};
 target.mObjectRef=LLUUID::generateNewID();fire();require(mover.mHotGazes.empty(),"unresolved must not react");
 target.mObjectRef=object.id;object.regionPtr=nullptr;fire();require(mover.mHotGazes.empty(),"foreign region");object.regionPtr=&region;
 target.mMode=LLActorMover::GazeTarget::CAST_MEMBER;target.mCastRef=actor.id;fire();require(mover.mHotGazes.empty(),"self cast");
 target.mMode=LLActorMover::GazeTarget::OBJECT;object.ancestor=&actor;fire();require(mover.mHotGazes.empty(),"self attachment");object.ancestor=nullptr;
 object.control=&actor;fire();require(mover.mHotGazes.empty(),"self animesh");object.control=nullptr;
 target.mObjectRef=object.id;fire();mover.applyHotGazes();mover.restoreHotGazes();object.dead=true;now+=.1;mover.applyHotGazes();
 require(mover.mHotGazes.at(actor.id).mState.mRelease==now,"lost target must return");object.dead=false;mover.clearHotGazes();
 // Live runtime park/recover keeps motor state and envelope; target deletion releases.
 now=40;fire();auto& live=mover.mHotGazes.at(actor.id);live.mPaint.mGaze.mGazeMotor.mCommittedYaw=.123f;
 const auto parked=LLUUID::generateNewID();mover.rewriteHotGazeReferences(actor.id,parked);
 require(mover.mHotGazes.count(parked)==1,"park retains runtime");
 mover.rewriteHotGazeReferences(parked,targetActor.id);
 require(mover.mHotGazes.at(targetActor.id).mPaint.mGaze.mGazeMotor.mCommittedYaw==.123f,"rekey motor continuity");
 mover.rewriteHotGazeReferences(object.id,LLUUID::null);
 require(mover.mHotGazes.at(targetActor.id).mState.mRelease==now,"deleted live target releases");mover.clearHotGazes();
 std::cout<<"PASS: native algazehot_test + production apply/restore/resolve/rekey; moving/rotating surface and joint frames; world fixation Snap 0/.5; held/capture bases; no self/axis fallback\n";
 return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}}
'''
    return source

def check():
    paint = method(NV/'llactormover.cpp','void LLActorMover::gazePaint(')
    assert 'ALGazeHot::stepMotor(g.mGazeMotor, in, g.mHotFixated, g.mSmoothDir, pose)' in paint
    assert 'eye_world_gaze, headWorld, comfort_yaw_deg' in paint
    assert 'if (motor_acquire || g.mHotSnap >= 0.f)' in paint
    assert 'ALGazeHot::fixatedEyeInHead(' in paint
    assert 'ALGazeHot::recruitFace(g.mGazeMotor, in,' in paint
    assert 'gAgent.getPosAgentFromGlobal(g.mPoint) - gazeRenderedJointPosition(av, head)' in paint
    assert 'gazePaint(av, g, nullptr, dt, advance, false, false, &paint)' in method(NV/'llactormover.cpp','void LLActorMover::applyHotGaze(')
    assert 'const F32 env_eased = g.mHotSnap >= 0.f ? g.mEnv :' in paint
    avatar=(NV/'llvoavatar.cpp').read_text(encoding='utf-8');app=(NV/'llappviewer.cpp').read_text(encoding='utf-8')
    assert 'applyHotGaze(this)' not in avatar
    assert app.index('ALSteppedAnimRuntime::instance().applyHeld()') < app.index('LLActorMover::instance().applyHotGazes()')
    end=method(NV/'alsteppedanimruntime.cpp','void ALSteppedAnimRuntime::endFrameRestore(')
    assert end.index('restoreHotGazes()') < end.index('restoreOverlay(avatar, entry)')
    clear=method(NV/'llactormover.cpp','void LLActorMover::clearAllDirectorLookAtRuntime(')
    assert clear.index('clearHotGazes()') < clear.index('restoreDirectorLookAtPose(')
    out=check_directory('hot_gaze');flags,libs=viewer_native_flags();source=fixture()
    native = '#define AL_HOT_GAZE_NATIVE_MAIN\n#include "' + (NV/'tests/algazehot_test.cpp').as_posix() + '"\n'
    result=compile_run(out,native,'algazehot_native',flags,libs)
    assert result.returncode==0,result.stdout+result.stderr
    print('PASS: linked and ran algazehot_test.cpp with its real llactormover.h and native LLSD/math dependencies', flush=True)
    result=compile_run(out,source,'algazehot_test',flags,libs)
    assert result.returncode==0,result.stdout+result.stderr
    print(result.stdout.strip())
    controls={
        'overlay_disabled':('    auto& hot = it->second;','    return;\n    auto& hot = it->second;'),
        'restore_emptied':(method(NV/'llactormover.cpp','void LLActorMover::restoreHotGazePose('),'void LLActorMover::restoreHotGazePose(LLVOAvatar*) {}'),
        'fixation_lock_removed':('g.mHotFixated = now >= hot.mState.mStart + hot.mState.mAcquire;','g.mHotFixated = false;'),
        'self_axis_fallback':('if (!body || body == actor || body->isDead() || body->getRegion() != gAgent.getRegion()) return false;', 'if (body == actor) { point = gAgent.getPosGlobalFromAgent(actor->getJoint("mHead")->getWorldPosition() + LLVector3(10,0,0)); return true; }\n        if (!body || body->isDead() || body->getRegion() != gAgent.getRegion()) return false;'),
        'unresolved_axis_fallback':('if (!body || !resolveHotGazeTarget(body, target, point)) return;','if (!body) return;\n    if (!resolveHotGazeTarget(body, target, point)) point = LLVector3d(100,30,1);'),
    }
    for name,(old,new) in controls.items():
        assert source.count(old)==1,(name,source.count(old))
        result=compile_run(out,source.replace(old,new),name,flags,libs)
        assert result.returncode==2,(name,result.stdout,result.stderr)
        print('PASS: negative control '+name+' failed behaviorally: '+result.stderr.strip())

if __name__=='__main__':check()
