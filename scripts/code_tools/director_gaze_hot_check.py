"""Execute production Hot Gaze code and the native test without building the viewer.

Only viewer/world storage is stubbed. Envelopes, slots, serialization, pose
capture/restore and moving-object centre resolution are copied verbatim.
"""
from pathlib import Path
import os
import re
import subprocess
from relight_area_policy_check import method


def check():
    nv = Path("indra/newview")
    fixture = r'''
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <iostream>
#include <map>
#include <string>
#include <vector>
using S32=int; using U32=unsigned; using U64=unsigned long long; using F32=float; using F64=double;
template<class T>T llclamp(T v,T a,T b){return std::clamp(v,a,b);}
template<class T>T llmax(T a,T b){return std::max(a,b);}
template<class T>T llmin(T a,T b){return std::min(a,b);}
struct LLUUID {
 int value=0; bool isNull()const{return !value;}bool notNull()const{return value;}
 void setNull(){value=0;}static const LLUUID null;
 static LLUUID generateNewID(){static int n=1;return {n++};}
 bool operator==(LLUUID b)const{return value==b.value;}
 bool operator!=(LLUUID b)const{return value!=b.value;}
 bool operator<(LLUUID b)const{return value<b.value;}
}; const LLUUID LLUUID::null{};
struct LLVector3d {
 double mdV[3]{};LLVector3d(double x=0,double y=0,double z=0):mdV{x,y,z}{}
 bool operator==(const LLVector3d& b)const{return std::equal(mdV,mdV+3,b.mdV);}
};
struct LLVector3 {
 float x=0;bool isFinite()const{return std::isfinite(x);}
 bool operator==(LLVector3 b)const{return x==b.x;}
};
struct LLSD {
 enum Type{NONE,MAP,ARRAY,NUMBER,STRING,UUID};Type type=NONE;
 double num=0;std::string str;LLUUID id;std::map<std::string,LLSD> map;std::vector<LLSD> arr;
 LLSD()=default;LLSD(double v):type(NUMBER),num(v){}LLSD(int v):LLSD(double(v)){}
 LLSD(bool v):LLSD(int(v)){}LLSD(const std::string& v):type(STRING),str(v){}
 LLSD(const char* v):LLSD(std::string(v)){}LLSD(LLUUID v):type(UUID),id(v){}
 static LLSD emptyArray(){LLSD s;s.type=ARRAY;return s;}
 bool isArray()const{return type==ARRAY;}bool isMap()const{return type==MAP;}
 bool has(const char* k)const{return map.count(k)!=0;}
 int size()const{return int(arr.size());}void append(const LLSD& v){type=ARRAY;arr.push_back(v);}
 LLSD& operator[](const char* k){type=MAP;return map[k];}
 const LLSD& operator[](const char* k)const{static LLSD none;auto i=map.find(k);return i==map.end()?none:i->second;}
 LLSD& operator[](int k){return arr.at(k);}const LLSD& operator[](int k)const{return arr.at(k);}
 double asReal()const{return num;}int asInteger()const{return int(num);}
 bool asBoolean()const{return num!=0;}std::string asString()const{return str;}LLUUID asUUID()const{return id;}
};
LLSD ll_sd_from_vector3d(const LLVector3d& v){LLSD a=LLSD::emptyArray();for(double n:v.mdV)a.append(n);return a;}
LLVector3d ll_vector3d_from_sd(const LLSD& s){return s.isArray()&&s.size()==3?LLVector3d(s[0].asReal(),s[1].asReal(),s[2].asReal()):LLVector3d();}
struct LLQuaternion {int v=0;bool operator==(LLQuaternion b)const{return v==b.v;}};
struct LLJoint {
 LLQuaternion rotation;LLJoint* getParent(){return this;}
 LLQuaternion getRotation()const{return rotation;}void setRotation(LLQuaternion q){rotation=q;}
};
struct LLVisualParam {float weight=0;};
struct LLVOAvatar {
 LLUUID id{100};LLUUID getID()const{return id;}
 std::map<std::string,LLJoint> joints;std::map<std::string,LLVisualParam> params;
 bool isDead()const{return false;}bool isGhostAvatar()const{return false;}
 int getSkeletonSerialNum()const{return 1;}int getAppearanceSerialNum()const{return 1;}
 LLJoint* getJoint(const char* n){auto i=joints.find(n);return i==joints.end()?nullptr:&i->second;}
 LLVisualParam* getVisualParam(const char* n){auto i=params.find(n);return i==params.end()?nullptr:&i->second;}
 float getVisualParamWeight(LLVisualParam* p){return p->weight;}
 bool setVisualParamWeight(LLVisualParam* p,float v){bool changed=p->weight!=v;p->weight=v;return changed;}
 void updateVisualParams(){}
};
struct LLBBox {
 LLVector3 centre;LLBBox(LLVector3 p={},LLQuaternion q={},LLVector3 a={},LLVector3 b={}):centre(p){}
 void addPointLocal(LLVector3){}void addBBoxAgent(const LLBBox& b){centre=b.centre;}
 LLVector3 getCenterAgent()const{return centre;}
};
struct LLViewerObject {
 LLUUID id;float position=0;bool dead=false;std::vector<LLViewerObject*> children;
 bool isDead()const{return dead;}LLViewerObject* getRootEdit(){return this;}
 LLUUID getID()const{return id;}LLVector3 getPositionAgent()const{return {position};}
 LLBBox getBoundingBoxAgent()const{return LLBBox({position});}
 const auto& getChildren()const{return children;}
};
struct ObjectList {std::map<LLUUID,LLViewerObject*> objects;
 LLViewerObject* findObject(LLUUID id){auto i=objects.find(id);return i==objects.end()?nullptr:i->second;}
} gObjectList;
U32 gFrameCount=1;
struct LLActorMover {
 enum {GAZE_PRIORITY_BLEND=0,GAZE_PRIORITY_UPPER_BODY=2,GAZE_PRIORITY_PLANTED_SPINE=3};
 struct GazeTarget {enum EMode{MOTION=0,CAMERA=1,CAST_MEMBER=2,FIXED_POINT=3,OBJECT=4};
 EMode mMode=MOTION;LLUUID mCastRef,mObjectRef;LLVector3d mFixedPoint;};
 struct Gaze{bool mChainYawValid=false,mBehindSweeping=false;float mChainYaw=0,mBehindShoulderSign=0;/*HOT_RESET*/
 int mGazeMotor=0;U32 mHotGeneration=0;int mGazePriorityOverride=0;float mTorsoAmount=0;int mHeldSkeletonSerial=1,mHeldAppearanceSerial=1,mWrittenGaze=0;};
 struct DirectorJointPose{bool mValid=false;LLQuaternion mRotation;};
 struct DirectorVisualParamPose{bool mValid=false;float mWeight=0;};
 struct DirectorGaze {
 int mMode=0;Gaze mGaze;
 DirectorJointPose mRoot,mPelvis,mTorso,mChest,mNeck,mHead,mEyeLeft,mEyeRight,mAltEyeLeft,mAltEyeRight;
 DirectorVisualParamPose mBlinkLeft,mBlinkRight;
 };
 struct GazeObjectCenterCache{U32 mFrame=0;bool mValid=false;LLVector3 mCenter;};
 std::map<LLUUID,GazeObjectCenterCache> mGazeObjectCenters;
 static void releaseDirectorTurnAO(DirectorGaze&){}
 static bool ensureDirectorOwner(LLVOAvatar*,DirectorGaze&){return false;}
 void captureDirectorLookAtPose(LLVOAvatar*,DirectorGaze&,bool);
 void restoreDirectorLookAtPose(LLVOAvatar*,DirectorGaze&);
 bool resolveGazeObjectCenter(const LLUUID&,LLVector3&);
};
int currentGazePriority(){return 0;}
'''
    fixture = fixture.replace("/*HOT_RESET*/", method(nv / "llactormover.h", "        void resetHotAim()"))
    math = (nv / "algazemath.h").read_text(encoding="utf-8")
    fixture += "constexpr float F_PI=3.14159265358979323846f, DEG_TO_RAD=F_PI/180.f;\nnamespace ALGazeMath {\n" + math[math.index("constexpr U64 DEFAULT_GAZE_SEED"):math.index("inline U64 castSeedFromUUID")] + method(nv / "algazemath.h", "inline F32 unitHash(") + "\n" + math[math.index("enum EGazeCueMacro"):math.index("} // namespace ALGazeMath")] + "}\n"
    fixture += method(nv / "llactormover.cpp", "static F32 resolveBehindShoulderChainYaw(") + "\n"
    for header in ("algazehot.h", "algazehotslots.h"):
        fixture += re.sub(r'^#include[^\n]*\n', '', (nv / header).read_text(encoding="utf-8"), flags=re.M)
    fixture += r'''
struct LLPresentationTime {struct Frame{F64 presentation_time=0;};static Frame& currentFrame(){static Frame f;return f;}};
'''
    fixture += r'''
struct LLDirectorCast {LLVOAvatar body;static LLDirectorCast& instance(){static LLDirectorCast c;return c;}LLVOAvatar* resolve(LLUUID){return &body;}};
'''
    fixture += 'struct HotDispatchFixture : LLActorMover {using DirectorGaze=LLActorMover::DirectorGaze;using GazeTarget=LLActorMover::GazeTarget;\n'
    fixture += method(nv / 'llactormover.h', '    struct HotGaze\n') + ';\n'
    fixture += r'''
    std::map<LLUUID,HotGaze> mHotGazes;
    void fireHotGaze(const LLUUID&,S32,const GazeTarget&,F32,F32,F32,bool,bool,F32,F32,F32);
    void hotGazeInput(S32,bool);
    };
'''
    for signature in ('void LLActorMover::fireHotGaze(', 'void LLActorMover::hotGazeInput('):
        fixture += method(nv / 'llactormover.cpp', signature).replace('LLActorMover::', 'HotDispatchFixture::') + '\n'
    for signature in ("void LLActorMover::captureDirectorLookAtPose(",
                      "void LLActorMover::restoreDirectorLookAtPose(LLVOAvatar* av, DirectorGaze& runtime)",
                      "bool LLActorMover::resolveGazeObjectCenter("):
        fixture += method(nv / "llactormover.cpp", signature) + "\n"
    fixture += '#define AL_HOT_GAZE_STANDALONE\n' + (nv / 'tests/algazehot_test.cpp').read_text(encoding="utf-8")
    fixture += r'''
int main(){
 hotGazeContract();
 HotDispatchFixture dispatch;LLActorMover::GazeTarget aim;
 auto& now=LLPresentationTime::currentFrame().presentation_time;
 auto fire=[&](int slot,bool hold,float head=1,float eyes=1,float torso=.2f){dispatch.fireHotGaze({9000},slot,aim,.2f,2,1,hold,false,head,eyes,torso);};
 now=10;fire(0,false);
 auto& active=dispatch.mHotGazes.at({100});
 now=10.1;const float before=active.mState.weight(now);fire(0,false);
 assert(active.mState.mRelease==now && hotNear(active.mState.weight(now),before)); // second Tap releases
 now=20;fire(0,false);now=22.5;assert(active.mState.returning(now));
 const float returning=active.mState.weight(now);fire(0,false);
 assert(!active.mState.returning(now) && hotNear(active.mState.weight(now),returning)); // Tap during automatic return re-fires
 now=30;fire(1,true);now=31;fire(2,true,0,0,0);
 assert(active.mFromHead==1 && active.mFromEyes==1 && hotNear(active.mFromTorso,.2f));
 assert(active.mState.mSlot==2 && active.mState.acquireFraction(now)==0);
 now=31.1;dispatch.hotGazeInput(1,false);assert(active.mState.mRelease<0); // takeover ignores old key up
 dispatch.hotGazeInput(2,false);assert(active.mState.mRelease==now);
 now=40;fire(3,true);const auto generation=active.mPaint.mGaze.mHotGeneration;
 now=40.05;fire(3,true);assert(active.mPaint.mGaze.mHotGeneration==generation+1);
 auto& aimState=active.mPaint.mGaze;
 aimState.mChainYawValid=true;aimState.mChainYaw=2.9f;aimState.mBehindShoulderSign=1;aimState.mBehindSweeping=true;
 now=50;dispatch.fireHotGaze({9000},0,aim,0,2,1,true,false,1,1,.2f);
 assert(!aimState.mChainYawValid && !aimState.mBehindSweeping);
 const float targetYaw=-80*DEG_TO_RAD;
 const float instant=resolveBehindShoulderChainYaw(targetYaw,2.5f,.001f,true,
     aimState.mChainYawValid,aimState.mChainYaw,aimState.mBehindShoulderSign,aimState.mBehindSweeping);
 assert(instant==targetYaw); // zero Snap takes over from a committed opposite shoulder immediately
 LLActorMover mover;LLVOAvatar av;LLActorMover::DirectorGaze paint;
 av.joints["mHead"].rotation={71};av.joints["mChest"].rotation={28};
 av.joints["mEyeLeft"].rotation={19};av.params["Blink_Left"].weight=.25f;
 paint.mGaze.mTorsoAmount=.2f;
 mover.captureDirectorLookAtPose(&av,paint,true);
 av.joints["mHead"].rotation={100};av.joints["mChest"].rotation={101};
 av.joints["mEyeLeft"].rotation={102};av.params["Blink_Left"].weight=1;
 mover.restoreDirectorLookAtPose(&av,paint);
 assert(av.joints["mHead"].rotation.v==71 && av.joints["mChest"].rotation.v==28);
 assert(av.joints["mEyeLeft"].rotation.v==19 && av.params["Blink_Left"].weight==.25f);
 // No lingering write after release: a new normal gaze is not overwritten by a second restore.
 av.joints["mHead"].rotation={82};mover.restoreDirectorLookAtPose(&av,paint);
 assert(av.joints["mHead"].rotation.v==82);
 LLViewerObject object;object.id={900};object.position=3;gObjectList.objects[object.id]=&object;
 LLVector3 target;assert(mover.resolveGazeObjectCenter(object.id,target) && target.x==3);
 object.position=17;++gFrameCount;
 assert(mover.resolveGazeObjectCenter(object.id,target) && target.x==17);
 object.dead=true;++gFrameCount;assert(!mover.resolveGazeObjectCenter(object.id,target));
 std::cout<<"PASS: production hot gaze tap/hold/stay/return, early release, retrigger, takeover, scene round-trip, clone filtering/rekey, exact pose restoration, live moving-object target\n";
}
'''
    avatar = (nv / "llvoavatar.cpp").read_text()
    assert avatar.index("actor_mover.restoreHotGazePose(this)") < avatar.index("actor_mover.restoreDirectorLookAtPose(this)")
    assert avatar.count("actor_mover.applyHotGaze(this)") == 2
    paint = method(nv / "llactormover.cpp", "void LLActorMover::applyHotGaze(")
    assert "captureDirectorLookAtPose(av, paint, false)" in paint
    assert "gazePaint(av, g" in paint and "getGazeTarget" not in paint
    inputs = (nv / "llviewerinput.cpp").read_text()
    assert "sHotGazeKeys.find(translated_key)" in inputs and "!gKeyboard->getKeyDown(it->first)" in inputs
    directory = Path(".tmp/hotgaze_20261102/contract").resolve()
    directory.mkdir(parents=True, exist_ok=True)
    (directory / "hot.cpp").write_text(fixture, encoding="utf-8")
    vcvars = next((Path(os.environ["ProgramFiles"]) / "Microsoft Visual Studio").glob("*/Community/VC/Auxiliary/Build/vcvars64.bat"))
    command = directory / "compile.cmd"
    command.write_text(f'@echo off\ncall "{vcvars}" >nul\ncl /nologo /std:c++17 /EHsc /Fo"{directory / "hot.obj"}" /Fe"{directory / "hot.exe"}" "{directory / "hot.cpp"}"\nexit /b %ERRORLEVEL%\n')
    subprocess.run(["cmd", "/d", "/c", str(command)], check=True)
    subprocess.run([str(directory / "hot.exe")], check=True)


if __name__ == "__main__":
    check()
