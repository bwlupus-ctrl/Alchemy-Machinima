"""Run production hot runtime methods with real gaze math and small world adapters.

Only the world/joint store and settings are adapters. The complete production gazePaint and actual
hot dispatcher, live resolver, layer capture/restore, rekey, motor and eye solver
are executed. Mutation controls are compiled and must fail at runtime.
"""
from pathlib import Path
from director_gaze_hot_integration import integration, assertions
from relight_area_policy_check import method
from native_check_tools import ROOT, check_directory, compile_run, viewer_native_flags

NV = ROOT / 'indra/newview'

def fixture():
    source = r'''
#include "linden_common.h"
#include "v3dmath.h"
#include "llsdutil_math.h"
#include "algazehot.h"
#include "alghostanimmirrormath.h"
#define LL_LLMOTIONCONTROLLER_H
struct LLMotionController {static float getGlobalTimeFactor(){return 1;} float getTimeFactor(){return 1;} bool getLastAppliedRegularRotationPriority(void*,S32&){return false;}};
#include "alavataranimationtime.h"
#include <cassert>
#include <map>
#include <iostream>
constexpr int GAZE_PRIORITY_BLEND=0, GAZE_PRIORITY_HEAD_EYES=1, GAZE_PRIORITY_UPPER_BODY=2, GAZE_PRIORITY_PLANTED_SPINE=3;
constexpr int GAZE_TANGENT=0, GAZE_CAMERA=1, GAZE_CAST=2, GAZE_POINT=3, GAZE_OBJECT=4;
constexpr int GAZE_COMPOSE_REPLACE=0, GAZE_COMPOSE_ADDITIVE=1, GAZE_COMPOSE_BLEND=2, GAZE_LEAN_ANGLE_EASE=1;
constexpr float GAZE_MIN_DIST=.25f, GAZE_EASE_TIME=.35f, GAZE_TAU_MIN=.04f, GAZE_TAU_MAX=.5f, GAZE_LOOKAHEAD=6.f, GAZE_CAMERA_ROLL_NECK_SHARE=.35f, GAZE_DIRECTOR_EYE_ROT_MAX=F_PI_BY_TWO*.22f;
struct LLJoint {
 LLVector3 scale{1,1,1};LLVector3 getPosition(){return position;}LLVector3 getScale(){return scale;}void setScale(LLVector3 v){scale=v;}void setPosition(LLVector3 p,bool=false){position=p;}
 std::string name; LLQuaternion rotation; LLVector3 position; LLJoint* parent=nullptr;
 std::string getName(){return name;} void setWorldRotation(LLQuaternion q){rotation=parent?q*~parent->getWorldRotation():q;}
 LLJoint* getParent(){return parent;}
 LLQuaternion getRotation()const{return rotation;}
 LLQuaternion getWorldRotation()const{return parent ? rotation * parent->getWorldRotation() : rotation;}
 LLVector3 getWorldPosition()const{return parent ? position * parent->getWorldRotation() + parent->getWorldPosition() : position;}
 void setRotation(LLQuaternion q){rotation=q;} void updateWorldMatrixChildren(){}
};
struct LLVisualParam{float weight=.25f;float getWeight(){return weight;}bool isAnimating(){return false;}};
struct Region{U64 getHandle(){return 1;}bool pointInRegionGlobal(const LLVector3d& p){return p.mdV[0]>=0 && p.mdV[0]<256 && p.mdV[1]>=0 && p.mdV[1]<256;}} region;
struct LLVOAvatar;
struct LLViewerObject {
 virtual ~LLViewerObject()=default;
 LLUUID id=LLUUID::generateNewID(); bool dead=false; Region* regionPtr=&region;
 LLVOAvatar *avatar=nullptr,*ancestor=nullptr,*control=nullptr; LLViewerObject* root=nullptr;
 LLJoint* attachmentJoint=nullptr;bool isAttachment(){return ancestor;}LLVOAvatar* getAvatar(){return ancestor;}
 LLVector3 position; LLQuaternion rotation;
 bool isDead()const{return dead;} LLUUID getID()const{return id;}
 Region* getRegion(){return regionPtr;} LLVOAvatar* asAvatar(){return avatar;}
 LLVOAvatar* getAvatarAncestor(){return ancestor;} LLVOAvatar* getControlAvatar(){return control;}
 LLViewerObject* getRootEdit(){return root?root:this;}
 LLVector3 getPositionAgent(){return position;} LLVector3 getRenderPosition(){return attachmentJoint?attachmentJoint->getWorldPosition()+LLVector3(1,0,0)*attachmentJoint->getWorldRotation():position;} LLQuaternion getRenderRotation(){return rotation;}
};
struct LLVOAvatar:LLViewerObject {
 bool ghost=false;float uniformScale=1;float getUniformScale(){return uniformScale;}float getPelvisToFoot(){return 1;}
 std::vector<LLJoint*>skeleton;
 std::vector<LLJoint*>& getSkeleton(){if(skeleton.empty())for(auto name:{"mTorso","mChest","mNeck","mHead","mEyeLeft","mEyeRight","mWristLeft","mWristRight"}){auto& j=joints[name];j.name=name;skeleton.push_back(&j);}return skeleton;}
 int getSkeletonJointCount(){return int(getSkeleton().size());}LLJoint* getSkeletonJoint(int i){return getSkeleton()[i];}
 U32 getSteppedSkeletonSerial(){return 1;}U32 getJointOverrideEpoch(){return 1;}
 struct StepAnimLabelState{};void restoreStepAnimLabels(StepAnimLabelState){}void shiftStepAnimLabels(LLVector3,StepAnimLabelState&){}
 void captureStepAnimRenderState(){}void verifyStepAnimRenderState(U32){}void frameStartRestore();
 LLVisualParam* getVisualParam(int){return nullptr;}
 std::map<std::string,LLJoint> joints;std::map<std::string,LLVisualParam> params;
 LLVOAvatar(){avatar=this;for(auto n:{"mRoot","mHead","mNeck","mTorso","mChest","mEyeLeft","mEyeRight","mWristLeft","mWristRight"})joints[n]={};
 joints["mTorso"].parent=&joints["mRoot"];joints["mNeck"].parent=&joints["mTorso"];joints["mHead"].parent=&joints["mNeck"];
 joints["mEyeLeft"].parent=joints["mEyeRight"].parent=&joints["mHead"];
 joints["mRoot"].position={30,30,1};params["Blink_Left"]={};}
 LLJoint* getJoint(std::string n){auto i=joints.find(n);return i==joints.end()?nullptr:&i->second;}
 LLJoint* getRootJoint(){return getJoint("mRoot");}
 U64 getCreationSerial(){return reinterpret_cast<U64>(this);} LLMotionController motion; LLMotionController& getMotionController(){return motion;}
 bool isSitting(){return false;} bool isBuilt(){return true;}bool isGhostAvatar(){return ghost;} int getSkeletonSerialNum(){return 1;}int getAppearanceSerialNum(){return 1;}
 LLVisualParam* getVisualParam(const char* n){auto i=params.find(n);return i==params.end()?nullptr:&i->second;}
 float getVisualParamWeight(LLVisualParam* p){return p->weight;}bool setVisualParamWeight(LLVisualParam* p,float v){p->weight=v;return true;}
 void updateVisualParams(){}void markStepAnimPoseChanged(){}void syncStepAnimAttachmentMatrices(){}
 int invalidateSkinPalettesForOverlay(){return 0;}void invalidateSkinPalettesAfterOverlay(){}
};
''' + method(NV/'llactormover.cpp','LLVector3 gazeRenderedJointPosition(') + r'''
struct Agent {Region* getRegion(){return &region;} LLVector3 getPosAgentFromGlobal(LLVector3d p){return LLVector3(p);}
 LLVector3d getPosGlobalFromAgent(LLVector3 p){return LLVector3d(p);}} gAgent;
struct ObjectList {int queries=0;std::map<LLUUID,LLViewerObject*> objects; LLViewerObject* findObject(LLUUID id){++queries;auto i=objects.find(id);return i==objects.end()?nullptr:i->second;}}gObjectList;
struct LLViewerCamera {LLVector3 origin{40,30,1};static LLViewerCamera* getInstance(){static LLViewerCamera c;return &c;}LLVector3 getOrigin(){return origin;} LLVector3 getAtAxis(){return LLVector3::x_axis;} LLVector3 getUpAxis(){return LLVector3::z_axis;}};
struct LLDirectorCast {std::map<LLUUID,LLVOAvatar*> bodies;static LLDirectorCast& instance(){static LLDirectorCast c;return c;}
 LLVOAvatar* resolve(LLUUID id){auto i=bodies.find(id);return i==bodies.end()?nullptr:i->second;}};
struct LLPresentationTime {struct Frame{double presentation_time=0;};static Frame& currentFrame(){static Frame f;return f;}static float presentationDelta(){return 1.f/60;}};

int gSavedSettings=0;
template<class T>struct LLCachedControl {T value;LLCachedControl(int,const char*,T v={}):value(v){} operator T()const{return value;}};
struct FrameInterval{float value(){return 1.f/60;}}gFrameIntervalSeconds;
struct LLPrismLens{static bool gateOnAirCameraEye(LLVector3&,void* =nullptr,LLQuaternion* =nullptr){return false;}};
int currentGazePriority(){return 0;}
bool gazeCameraSafe(LLVOAvatar*,bool){return true;}
struct LLActorMover {
'''
    source += method(NV/'llactormover.h', '    struct GazeTarget\n') + ';\n'
    for signature in ('    struct Gaze\n', '    struct DirectorJointPose\n',
                      '    struct DirectorVisualParamPose\n', '    struct DirectorGaze\n'):
        source += method(NV/'llactormover.h', signature) + ';\n'
    source += method(NV/'llactormover.h','    struct HotGaze\n') + ';\n'
    source += r'''
 std::map<LLUUID,Gaze> mGazes;std::map<LLUUID,DirectorGaze> mDirectorGazes;
 ALGhostAnimMirrorMath::GazeWeights getGazeWrittenWeights(const LLUUID&)const;
 std::map<LLUUID,HotGaze> mHotGazes;std::map<LLUUID,DirectorGaze> mHotGazeCopies;
 static LLActorMover& instance(){static LLActorMover m;return m;}
 bool hasHotGazes()const{return !mHotGazes.empty();}
 static bool hotGazeCopiesJoint(const std::string&);bool hasHotGazePose(const LLVOAvatar*)const;void captureHotGazeCopy(LLVOAvatar*,LLVOAvatar*);
 struct Move{bool mIsPath=false;float mDist=0,mDir=1;LLQuaternion mCurRot;};
 struct Path{std::vector<int>mNodes;bool mDirty=false;float mTotalLength=0;void evalAtDistance(float,LLVector3d&,LLVector3d&)const{}};
 const Path* getPath(LLUUID){return nullptr;}
 bool applyDirectorBodyTurn(LLVOAvatar*,DirectorGaze&,const LLVector3&){return false;}
 static void invalidateGazeCache(Gaze&,bool);static bool ensureGazeOwner(LLVOAvatar*,Gaze&);
 static void invalidateDirectorCache(DirectorGaze&);
 static bool ensureDirectorOwner(LLVOAvatar*,DirectorGaze&); static void releaseDirectorTurnAO(DirectorGaze&){}
 void captureDirectorLookAtPose(LLVOAvatar*,DirectorGaze&,bool);void restoreDirectorLookAtPose(LLVOAvatar*,DirectorGaze&);
 bool resolveHotGazeTarget(LLVOAvatar*,const GazeTarget&,LLVector3d&);
 bool resolveGazeObjectCenter(const LLUUID& id,LLVector3& p){auto* o=gObjectList.findObject(id);if(!o||o->dead)return false;p=o->position;return true;}
 static bool rewriteGazeTarget(GazeTarget&,const LLUUID&,const LLUUID&);
 void fireHotGaze(const LLUUID&,S32,const GazeTarget&,F32,F32,F32,bool,bool,F32,F32,F32);
 void hotGazeInput(S32,bool);void releaseHotGazeSlot(S32);void restoreHotGazePose(LLVOAvatar*);void applyHotGaze(LLVOAvatar*);
 void clearHotGazes();void restoreHotGazes();void applyHotGazes();void rewriteHotGazeReferences(const LLUUID&,const LLUUID&);
 void gazePaint(LLVOAvatar*,Gaze&,const Move*,F32,bool,bool,bool,DirectorGaze* =nullptr,F32=90,const GazeTarget* =nullptr,F32=1);
};
#define LL_LLACTORMOVER_H
#include "algazehotslots.h"
#define AL_HOT_GAZE_STANDALONE
'''
    source += (NV/'tests/algazehot_test.cpp').read_text(encoding='utf-8')
    for sig in ('ALGhostAnimMirrorMath::GazeWeights LLActorMover::getGazeWrittenWeights(', 'void LLActorMover::invalidateGazeCache(', 'bool LLActorMover::ensureGazeOwner(',
                'void LLActorMover::invalidateDirectorCache(', 'bool LLActorMover::ensureDirectorOwner(',
                'bool LLActorMover::hotGazeCopiesJoint(', 'bool LLActorMover::hasHotGazePose(', 'void LLActorMover::captureHotGazeCopy(',
                'bool LLActorMover::rewriteGazeTarget(', 'void LLActorMover::captureDirectorLookAtPose(',
                'void LLActorMover::restoreDirectorLookAtPose(LLVOAvatar* av, DirectorGaze& runtime)',
                'bool LLActorMover::resolveHotGazeTarget(', 'void LLActorMover::releaseHotGazeSlot(',
                'void LLActorMover::restoreHotGazes(', 'void LLActorMover::applyHotGazes(',
                'void LLActorMover::fireHotGaze(', 'void LLActorMover::hotGazeInput(',
                'void LLActorMover::restoreHotGazePose(', 'void LLActorMover::applyHotGaze(',
                'void LLActorMover::clearHotGazes(', 'void LLActorMover::rewriteHotGazeReferences('):
        source += method(NV/'llactormover.cpp',sig) + '\n'
    for sig in ('static F32 gazeAllowedAlpha(', 'void applyGazeAimRoll(',
                'bool currentAnimatedGazeDirection(', 'static LLVector3 nearLensOffsetDir(', 'static F32 resolveBehindShoulderChainYaw(',
                'void LLActorMover::gazePaint('):
        source += method(NV/'llactormover.cpp', sig) + '\n'
    source += integration(NV)
    source += r'''
float angular(LLVector3 a,LLVector3 b){a.normalize();b.normalize();return acosf(llclamp(a*b,-1.f,1.f))*RAD_TO_DEG;}
void require(bool p,const char* message){if(!p)throw std::runtime_error(message);}
int main(){try{
 hotGazeContract();
 LLActorMover& mover=LLActorMover::instance();LLVOAvatar actor,targetActor;auto& cast=LLDirectorCast::instance();
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
 now=10+frame/60.;LLFrameTimer::updateFrameCount();mover.restoreHotGazes();
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
 const LLVector3 actual=(LLVector3::x_axis*actor.getJoint("mEyeLeft")->getWorldRotation()+
     LLVector3::x_axis*actor.getJoint("mEyeRight")->getWorldRotation())*.5f;
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
'''
    source += assertions()
    source += r'''
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
    flags = ['/Od' if flag == '/O2' else flag for flag in flags] # native behavior, no optimizer time
    native = '#define AL_HOT_GAZE_NATIVE_MAIN\n#include "' + (NV/'tests/algazehot_test.cpp').as_posix() + '"\n'
    result=compile_run(out,native,'algazehot_native',flags,libs)
    assert result.returncode==0,result.stdout+result.stderr
    print('PASS: linked and ran algazehot_test.cpp with its real llactormover.h and native LLSD/math dependencies', flush=True)
    assert source.count(paint) == 1 # exact complete production body, never a renderer substitute
    result=compile_run(out,source,'algazehot_test',flags,libs)
    assert result.returncode==0,result.stdout+result.stderr
    print(result.stdout.strip())
    controls={
        'production_paint_return':(paint,paint[:paint.index('{')+1]+' return;'+paint[paint.index('{')+1:]),
        'flip_composition_missing':('                    ghost->applyDirectedMirrorStamp(inst->mDriveMode, inst->mDirectedMirrorFreezeDrive, true);','                    (void)mode; // mutant: omit late flip composition'),
        'clone_copy_missing':('        ALGhostStudio::instance().updatePostObjectList(true);','        /* mutant: no source presentation copy */'),
        'clone_runtime_alias_missing':('                if (inst.mEntityId.notNull()) instances[inst.mEntityId] = &inst;','                /* mutant: stable instance keys only */'),
        'copy_nonhot_limbs':(method(NV/'llactormover.cpp','bool LLActorMover::hotGazeCopiesJoint('),'bool LLActorMover::hotGazeCopiesJoint(const std::string&) { return true; }'),
        'recovered_restore_clears_motor':('    if (preserve_motor) runtime.mGaze.mGazeMotor = motor;','    (void)preserve_motor; (void)motor;'),
        'hot_particle_repair_missing':('        LLViewerPartSim::instance().refreshStepAnimAttachmentSourcePositions(true);','        /* mutant: no final hot particle repair */'),
        'hot_repair_emits':('    if (!hot_gaze)\n    {\n        for (StepAnimDeferredSource&','    if (true)\n    {\n        for (StepAnimDeferredSource&'),
        'inherited_restore_missing':('        restoreDirectorLookAtPose(body, entry.second);','        /* mutant: inherited clone pose not restored */'),
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
