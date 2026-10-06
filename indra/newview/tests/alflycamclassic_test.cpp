/**
 * @file alflycamclassic_test.cpp
 * @brief Independent golden checks for the pre-change Classic flycam math.
 *
 * Derived from git show 7a6d4e5e5e3:indra/newview/llviewerjoystick.cpp.
 * Expectations are closed-form per-axis calculations, not a second integrator.
 */
#include "linden_common.h"
#include "../test/lltut.h"
#include "../alflycamclassic.h"
#include "m3math.h"
#include <array>
#include <cmath>

namespace tut { namespace {
constexpr U32 CAM_X=3,CAM_Y=4,CAM_Z=5,CAM_W=6;
struct Storage {
 std::array<F32,7> last{{.03f,-.02f,.01f,.04f,-.03f,.02f,-.01f}};
 std::array<F32,7> delta{{.01f,-.015f,.02f,-.01f,.005f,.012f,-.02f}};
 LLVector3 position{1.f,2.f,3.f}; LLQuaternion rotation; F32 zoom=1.1f;
 Storage(){rotation.setQuat(.12f,-.18f,.23f);}
 ALFlycamClassic::State state(){return {last,delta,position,rotation,zoom};}
};
ALFlycamClassic::Input input(bool bd,F32 dt){
 ALFlycamClassic::Input in{};in.axis={{.32f,-.41f,.18f,.27f,-.22f,.35f,-.16f}};
 in.axisDeadZone={{.02f,.04f,.06f,.08f,.10f,.12f,.14f}};in.axisScale={{1.1f,-.9f,.7f,1.2f,-.8f,.6f,1.4f}};
 in.frameIntervalSeconds=dt;in.flycamFeathering=5.25f;in.buildModeScale=.23f;in.minView=.2f;in.maxView=2.1f;
 in.cameraAngle=1.047f;in.triggerRoll=.31f;in.triggerThrottle=1.f;in.blackDragon=bd;return in;
}
F32 signed_deadzone(F32 x,F32 dz,bool normalise){
 if(normalise&&std::fabs(x)<=dz)return x;
 x=x>0.f?std::max(x-dz,0.f):std::min(x+dz,0.f);return normalise?x/(1.f-dz):x;
}
F32 raw_axis(const ALFlycamClassic::Input& in,U32 a){
 F32 x=-in.axis[a];if(in.blackDragon){if(a==CAM_W)x+=(F32)in.zoomIn-(F32)in.zoomOut;
 if(a==0)x+=(F32)in.jump-(F32)in.crouch;if(a==CAM_X)x+=(F32)in.rollRight-(F32)in.rollLeft;}
 else if(!in.invertPitch&&a==CAM_Y)x=-x;return x;
}
struct Expected{std::array<F32,7> last,delta;F32 time,feather;bool zero;};
Expected closed_form(const ALFlycamClassic::Input& in,const Storage& before){
 Expected out{before.last,before.delta,in.blackDragon?std::max(.016f,std::min(.033f,in.frameIntervalSeconds)):std::min(.2f,in.frameIntervalSeconds),in.flycamFeathering,true};
 if(in.blackDragon&&(before.zoom<=in.minView||before.zoom>=in.maxView))out.feather=3.f;
 for(U32 a=0;a<7;++a){const F32 raw=raw_axis(in,a);F32 cmd=raw;
  if((in.blackDragon&&(in.cursor3D||std::fabs(raw)<in.axisDeadZone[a]))||(!in.blackDragon&&in.cursor3D))cmd-=before.last[a];
  out.last[a]=raw;cmd=signed_deadzone(cmd,in.axisDeadZone[a],in.blackDragon);
  if(in.xiTakeover&&in.xiMode==0&&a==CAM_X)cmd=std::max(-1.f,std::min(1.f,cmd+in.triggerRoll));
  if(in.xiTakeover&&a<=CAM_Z&&in.triggerThrottle!=1.f)cmd*=in.triggerThrottle;
  if(in.buildMode&&a<=2)cmd*=in.buildModeScale;cmd*=in.axisScale[a];if(!in.cursor3D)cmd*=out.time;
  out.delta[a]=before.delta[a]+(cmd-before.delta[a])*out.time*out.feather;out.zero=out.zero&&cmd==0.f;}
 return out;
}
void verify_case(const char* label,ALFlycamClassic::Input in){
 Storage actual,before=actual;if(in.blackDragon&&in.zoomDefault==1){before.zoom=in.cameraAngle;before.delta[CAM_W]=0.f;}
 Expected e=closed_form(in,before);auto state=actual.state();auto mid=ALFlycamClassic::integrateDeltas(in,state);
 ensure_equals("branch",mid.blackDragon,in.blackDragon);ensure_equals("zero",mid.isZero,e.zero);
 ensure("time",std::fabs(mid.time-e.time)<=1.e-6f);ensure("feather",std::fabs(mid.feather-e.feather)<=1.e-6f);
 for(U32 a=0;a<7;++a){ensure(label,std::fabs(actual.last[a]-e.last[a])<=1.e-6f);ensure(label,std::fabs(actual.delta[a]-e.delta[a])<=1.e-6f);}
 LLVector3 pos=before.position+LLVector3(e.delta.data())*before.rotation;
 LLQuaternion rot=LLQuaternion(LLMatrix3(e.delta[CAM_X],e.delta[CAM_Y],e.delta[CAM_Z]))*before.rotation;
 F32 zoom=in.zoomDirect?e.last[CAM_W]*in.axisScale[CAM_W]+(in.blackDragon?in.axisScale[CAM_W]:in.axisDeadZone[CAM_W])
                       :std::max(in.minView,std::min(in.maxView,before.zoom+e.delta[CAM_W]));
 ALFlycamClassic::finishIntegration(in,mid,state);ensure(label,(actual.position-pos).length()<=1.e-6f);
 if(!in.levelHorizon&&!in.autoLeveling&&!(in.blackDragon&&in.rollDefault==1))for(U32 i=0;i<4;++i)ensure(label,std::fabs(actual.rotation.mQ[i]-rot.mQ[i])<=1.e-6f);
 if(in.levelHorizon||(in.blackDragon&&in.rollDefault==1)){LLMatrix3 level(actual.rotation);ensure("exact horizon has zero vertical left",std::fabs(level.mMatrix[1][2])<=1.e-6f);}
 ensure(label,std::fabs(actual.zoom-zoom)<=1.e-6f);if(in.levelHorizon){ensure_equals("level delta",actual.delta[CAM_X],0.f);ensure_equals("level reseed",actual.last[CAM_X],-in.axis[CAM_X]);}
}
}}
namespace tut {
struct flycam_classic_data{};typedef test_group<flycam_classic_data> flycam_classic_group;typedef flycam_classic_group::object flycam_classic_object;
flycam_classic_group gFlycamClassicGroup("alflycamclassic");
template<> template<> void flycam_classic_object::test<1>(){
 for(bool bd:{false,true})for(S32 mode=0;mode<3;++mode)for(bool build:{false,true})for(F32 dt:{1.f/30.f,1.f/60.f,1.f/144.f,.5f}){
  auto in=input(bd,dt);in.buildMode=build;in.xiTakeover=mode!=0;in.xiMode=mode==1?0:1;if(mode==1)in.triggerRoll=.37f;if(mode==2)in.triggerThrottle=.31f;verify_case("classic matrix",in);}}
template<> template<> void flycam_classic_object::test<2>(){
 for(bool direct:{false,true})for(U32 b=0;b<7;++b){auto in=input(true,1.f/60);in.zoomDirect=direct;
  if(b==0)in.zoomDefault=1;if(b==1)in.zoomOut=1;if(b==2)in.zoomIn=1;if(b==3)in.jump=1;if(b==4)in.crouch=1;if(b==5)in.rollLeft=1;if(b==6)in.rollRight=1;verify_case("BD buttons",in);}}
template<> template<> void flycam_classic_object::test<3>(){
 for(bool bd:{false,true})for(bool automatic:{false,true})for(bool horizon:{false,true}){auto in=input(bd,1.f/60);in.autoLeveling=automatic;in.levelHorizon=horizon;in.rollDefault=bd&&automatic;verify_case("level matrix",in);}}
template<> template<> void flycam_classic_object::test<4>(){
 for(bool bd:{false,true})for(bool invert:{false,true})for(bool cursor:{false,true})for(U32 a=0;a<7;++a){auto in=input(bd,1.f/144);in.invertPitch=invert;in.cursor3D=cursor;in.axis.fill(0.f);in.axis[a]=(a&1)?-.11f:.11f;in.axisScale[a]=(a&1)?-1.3f:1.3f;verify_case("axis/dead-zone",in);}}
}
