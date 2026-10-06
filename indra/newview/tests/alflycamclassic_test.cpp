/**
 * @file alflycamclassic_test.cpp
 * @brief Independent golden oracle for the pre-change Classic moveFlycam math.
 *
 * The reference below is a direct re-statement of the two integrators from
 * git show 7a6d4e5e5e3:indra/newview/llviewerjoystick.cpp.  It deliberately
 * does not call, include, or share helpers with alflycamclassic.cpp.
 */

#include "linden_common.h"
#include "../test/lltut.h"
#include "../alflycamclassic.h"
#include "m3math.h"

#include <array>
#include <cmath>

namespace tut
{
namespace
{
constexpr U32 X_I=1,Y_I=2,Z_I=0,CAM_X=3,CAM_Y=4,CAM_Z=5,CAM_W=6;

struct Storage
{
    std::array<F32,7> last{{.03f,-.02f,.01f,.04f,-.03f,.02f,-.01f}};
    std::array<F32,7> delta{{.01f,-.015f,.02f,-.01f,.005f,.012f,-.02f}};
    LLVector3 position{1.f,2.f,3.f};
    LLQuaternion rotation;
    F32 zoom=1.1f;
    Storage() { rotation.setQuat(.12f,-.18f,.23f); }
    ALFlycamClassic::State state() { return {last,delta,position,rotation,zoom}; }
};

ALFlycamClassic::Input base_input(bool bd,F32 dt)
{
    ALFlycamClassic::Input in{};
    in.axis={{.32f,-.41f,.18f,.27f,-.22f,.35f,-.16f}};
    in.axisDeadZone={{.02f,.04f,.06f,.08f,.10f,.12f,.14f}};
    in.axisScale={{1.1f,-.9f,.7f,1.2f,-.8f,.6f,1.4f}};
    in.frameIntervalSeconds=dt;in.flycamFeathering=5.25f;in.buildModeScale=.23f;
    in.minView=.2f;in.maxView=2.1f;in.cameraAngle=1.047f;
    in.triggerRoll=.31f;in.triggerThrottle=1.f;
    in.blackDragon=bd;in.zoomDirect=false;
    return in;
}

ALFlycamClassic::Intermediate reference_deltas(const ALFlycamClassic::Input& in,Storage& s)
{
    F32 time=in.frameIntervalSeconds;
    F32 cur[7]; bool zero=true;
    if(in.blackDragon)
    {
        time=llclamp(time,.016f,.033f);F32 feather=in.flycamFeathering;
        if(in.zoomDefault==1){s.zoom=in.cameraAngle;s.delta[CAM_W]=0.f;}
        if(s.zoom<=in.minView||s.zoom>=in.maxView)feather=3.f;
        for(U32 i=0;i<7;++i)
        {
            cur[i]=-in.axis[i];
            if(i==CAM_W){cur[i]-=(F32)in.zoomOut;cur[i]+=(F32)in.zoomIn;}
            if(i==Z_I){cur[i]+=(F32)in.jump;cur[i]-=(F32)in.crouch;}
            if(i==CAM_X){cur[i]-=(F32)in.rollLeft;cur[i]+=(F32)in.rollRight;}
            F32 tmp=cur[i],dz=in.axisDeadZone[i];
            if(in.cursor3D||llabs(cur[i])<dz)cur[i]-=s.last[i];
            s.last[i]=tmp;
            if(llabs(cur[i])>dz)
            {
                if(cur[i]>0)cur[i]=llclamp(cur[i]-dz,0.f,1.f-dz);
                else cur[i]=llclamp(cur[i]+dz,-1.f+dz,0.f);
                cur[i]/=(1.f-dz);
            }
            if(in.xiTakeover&&in.xiMode==0&&i==CAM_X)cur[i]=llclamp(cur[i]+in.triggerRoll,-1.f,1.f);
            if(in.xiTakeover&&i<=CAM_Z&&in.triggerThrottle!=1.f)cur[i]*=in.triggerThrottle;
            if(in.buildMode&&(i==X_I||i==Y_I||i==Z_I))cur[i]*=in.buildModeScale;
            cur[i]*=in.axisScale[i];
            if(!in.cursor3D)cur[i]*=time;
            s.delta[i]=s.delta[i]+(cur[i]-s.delta[i])*time*feather;
            zero=zero&&(cur[i]==0.f);
        }
        if(in.invertPitch)cur[CAM_Y]=-cur[CAM_Y];
        return {true,zero,time,feather};
    }
    if(time>.2f)time=.2f;
    F32 feather=in.flycamFeathering;bool absolute=in.cursor3D;
    for(U32 i=0;i<7;++i)
    {
        cur[i]=-in.axis[i];
        if(!in.invertPitch&&i==4)cur[i]=-cur[i];
        F32 tmp=cur[i];if(absolute)cur[i]-=s.last[i];s.last[i]=tmp;
        if(cur[i]>0)cur[i]=llmax(cur[i]-in.axisDeadZone[i],0.f);
        else cur[i]=llmin(cur[i]+in.axisDeadZone[i],0.f);
        if(in.xiTakeover&&in.xiMode==0&&i==CAM_X)cur[i]=llclamp(cur[i]+in.triggerRoll,-1.f,1.f);
        if(in.xiTakeover&&i<=CAM_Z&&in.triggerThrottle!=1.f)cur[i]*=in.triggerThrottle;
        if(in.buildMode&&(i==X_I||i==Y_I||i==Z_I))cur[i]*=in.buildModeScale;
        cur[i]*=in.axisScale[i];if(!absolute)cur[i]*=time;
        s.delta[i]=s.delta[i]+(cur[i]-s.delta[i])*time*feather;
        zero=zero&&(cur[i]==0.f);
    }
    return {false,zero,time,feather};
}

void reference_finish(const ALFlycamClassic::Input& in,const ALFlycamClassic::Intermediate& mid,Storage& s)
{
    const F32 time=mid.time,feather=mid.feather;
    s.position+=LLVector3(s.delta.data())*s.rotation;
    LLMatrix3 rm(s.delta[CAM_X],s.delta[CAM_Y],s.delta[CAM_Z]);s.rotation=LLQuaternion(rm)*s.rotation;
    if(in.blackDragon?(in.levelHorizon||in.autoLeveling||in.rollDefault==1):(in.levelHorizon||in.autoLeveling))
    {
        LLMatrix3 level(s.rotation);LLVector3 x(level.mMatrix[0]),y(level.mMatrix[1]),z(level.mMatrix[2]);
        y.mV[2]=0.f;y.normVec();level.setRows(x,y,z);level.orthogonalize();LLQuaternion q(level);
        if(in.blackDragon)
        {
            LLQuaternion lerp=nlerp(llmin(feather*time,1.f),s.rotation,q);
            s.rotation=in.levelHorizon||in.rollDefault==1?q:lerp;
        }
        else s.rotation=in.levelHorizon?q:nlerp(llmin(feather*time,1.f),s.rotation,q);
    }
    if(in.zoomDirect)
        s.zoom=in.blackDragon?s.last[CAM_W]*in.axisScale[6]+in.axisScale[6]
                             :s.last[CAM_W]*in.axisScale[6]+in.axisDeadZone[6];
    else s.zoom=llclamp(s.zoom+s.delta[CAM_W],in.minView,in.maxView);
    if(in.levelHorizon){s.delta[CAM_X]=0.f;s.last[CAM_X]=-in.axis[CAM_X];}
}

void compare(const char* label,const ALFlycamClassic::Input& in)
{
    Storage golden,actual;auto actual_state=actual.state();
    auto gm=reference_deltas(in,golden);auto am=ALFlycamClassic::integrateDeltas(in,actual_state);
    ensure_equals("golden branch",am.blackDragon,gm.blackDragon);ensure_equals("golden zero",am.isZero,gm.isZero);
    ensure("golden time",std::fabs(am.time-gm.time)<=1.e-6f);ensure("golden feather",std::fabs(am.feather-gm.feather)<=1.e-6f);
    reference_finish(in,gm,golden);ALFlycamClassic::finishIntegration(in,am,actual_state);
    for(U32 i=0;i<7;++i){ensure(label,std::fabs(actual.last[i]-golden.last[i])<=1.e-6f);ensure(label,std::fabs(actual.delta[i]-golden.delta[i])<=1.e-6f);}
    ensure(label,(actual.position-golden.position).length()<=1.e-6f);
    ensure(label,std::fabs(actual.zoom-golden.zoom)<=1.e-6f);
    for(U32 i=0;i<4;++i)ensure(label,std::fabs(actual.rotation.mQ[i]-golden.rotation.mQ[i])<=1.e-6f);
}
}

struct flycam_classic_data{};
typedef test_group<flycam_classic_data> flycam_classic_group;
typedef flycam_classic_group::object flycam_classic_object;
flycam_classic_group gFlycamClassicGroup("alflycamclassic");

// Golden matrix: BD/non-BD x operator(XInput) off/ROLL/THROTTLE x build x dt clamps.
template<> template<> void flycam_classic_object::test<1>()
{
    for(bool bd:{false,true})for(S32 xi=0;xi<3;++xi)for(bool build:{false,true})
    for(F32 dt:{1.f/30.f,1.f/60.f,1.f/144.f,.5f})
    {
        auto in=base_input(bd,dt);in.buildMode=build;in.xiTakeover=xi!=0;
        if(xi==1){in.xiMode=0;in.triggerRoll=.37f;}else if(xi==2){in.xiMode=1;in.triggerThrottle=.31f;}
        compare("classic matrix",in);
    }
}

// BD-only buttons and direct/limited zoom paths.
template<> template<> void flycam_classic_object::test<2>()
{
    for(bool direct:{false,true})for(U32 button=0;button<7;++button)
    {
        auto in=base_input(true,1.f/60);in.zoomDirect=direct;
        if(button==0)in.zoomDefault=1;if(button==1)in.zoomOut=1;if(button==2)in.zoomIn=1;
        if(button==3)in.jump=1;if(button==4)in.crouch=1;if(button==5)in.rollLeft=1;if(button==6)in.rollRight=1;
        compare("BD buttons",in);
    }
}

// ROLL_DEFAULT, auto-level and the moved level-horizon tail in both branches.
template<> template<> void flycam_classic_object::test<3>()
{
    for(bool bd:{false,true})for(bool automatic:{false,true})for(bool horizon:{false,true})
    {
        auto in=base_input(bd,1.f/60);in.autoLeveling=automatic;in.levelHorizon=horizon;in.rollDefault=bd&&automatic;
        compare("level matrix",in);
        if(horizon)
        {
            Storage actual;auto state=actual.state();auto mid=ALFlycamClassic::integrateDeltas(in,state);ALFlycamClassic::finishIntegration(in,mid,state);
            ensure_equals("level cleanup delta",actual.delta[CAM_X],0.f);
            ensure_equals("level cleanup raw reseed",actual.last[CAM_X],-in.axis[CAM_X]);
        }
    }
}

// Pitch inversion, per-axis sign/dead-zone coverage, and cursor/absolute path.
template<> template<> void flycam_classic_object::test<4>()
{
    for(bool bd:{false,true})for(bool invert:{false,true})for(bool cursor:{false,true})for(U32 axis=0;axis<7;++axis)
    {
        auto in=base_input(bd,1.f/144);in.invertPitch=invert;in.cursor3D=cursor;
        in.axis.fill(0.f);in.axis[axis]=(axis&1)?-.11f:.11f;in.axisScale[axis]=(axis&1)?-1.3f:1.3f;
        compare("axis parity",in);
    }
}
}
