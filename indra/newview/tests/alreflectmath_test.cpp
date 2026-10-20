#include "linden_common.h"
#include "../test/lltut.h"
#include "../alreflectmath.h"
#include <set>
#include <limits>

namespace tut
{
namespace
{
void closeVec(const char* label, const LLVector3& actual, const LLVector3& expected, F32 tolerance = 1e-5f)
{ ensure(label, (actual - expected).length() <= tolerance); }
struct Random
{
    U32 seed = 0x39483a1u;
    F32 number() { seed = seed * 1664525u + 1013904223u; return (F32)(seed >> 8) / 8388608.f - 1.f; }
    LLVector3 vec() { return LLVector3(number(), number(), number()); }
    LLQuaternion quat() { LLQuaternion q(number(), number(), number(), number()); q.normalize(); return q; }
};
// Literal inventory from avatar_skeleton.xml, pinned here rather than parsed at runtime.
const char* const BONES[] = {
    "mPelvis", "mSpine1", "mSpine2", "mTorso",
    "mSpine3", "mSpine4", "mChest", "mNeck",
    "mHead", "mSkull", "mEyeRight", "mEyeLeft",
    "mFaceRoot", "mFaceEyeAltRight", "mFaceEyeAltLeft", "mFaceForeheadLeft",
    "mFaceForeheadRight", "mFaceEyebrowOuterLeft", "mFaceEyebrowCenterLeft", "mFaceEyebrowInnerLeft",
    "mFaceEyebrowOuterRight", "mFaceEyebrowCenterRight", "mFaceEyebrowInnerRight", "mFaceEyeLidUpperLeft",
    "mFaceEyeLidLowerLeft", "mFaceEyeLidUpperRight", "mFaceEyeLidLowerRight", "mFaceEar1Left",
    "mFaceEar2Left", "mFaceEar1Right", "mFaceEar2Right", "mFaceNoseLeft",
    "mFaceNoseCenter", "mFaceNoseRight", "mFaceCheekLowerLeft", "mFaceCheekUpperLeft",
    "mFaceCheekLowerRight", "mFaceCheekUpperRight", "mFaceJaw", "mFaceChin",
    "mFaceTeethLower", "mFaceLipLowerLeft", "mFaceLipLowerRight", "mFaceLipLowerCenter",
    "mFaceTongueBase", "mFaceTongueTip", "mFaceJawShaper", "mFaceForeheadCenter",
    "mFaceNoseBase", "mFaceTeethUpper", "mFaceLipUpperLeft", "mFaceLipUpperRight",
    "mFaceLipCornerLeft", "mFaceLipCornerRight", "mFaceLipUpperCenter", "mFaceEyecornerInnerLeft",
    "mFaceEyecornerInnerRight", "mFaceNoseBridge", "mCollarLeft", "mShoulderLeft",
    "mElbowLeft", "mWristLeft", "mHandMiddle1Left", "mHandMiddle2Left",
    "mHandMiddle3Left", "mHandIndex1Left", "mHandIndex2Left", "mHandIndex3Left",
    "mHandRing1Left", "mHandRing2Left", "mHandRing3Left", "mHandPinky1Left",
    "mHandPinky2Left", "mHandPinky3Left", "mHandThumb1Left", "mHandThumb2Left",
    "mHandThumb3Left", "mCollarRight", "mShoulderRight", "mElbowRight",
    "mWristRight", "mHandMiddle1Right", "mHandMiddle2Right", "mHandMiddle3Right",
    "mHandIndex1Right", "mHandIndex2Right", "mHandIndex3Right", "mHandRing1Right",
    "mHandRing2Right", "mHandRing3Right", "mHandPinky1Right", "mHandPinky2Right",
    "mHandPinky3Right", "mHandThumb1Right", "mHandThumb2Right", "mHandThumb3Right",
    "mWingsRoot", "mWing1Left", "mWing2Left", "mWing3Left",
    "mWing4Left", "mWing4FanLeft", "mWing1Right", "mWing2Right",
    "mWing3Right", "mWing4Right", "mWing4FanRight", "mHipRight",
    "mKneeRight", "mAnkleRight", "mFootRight", "mToeRight",
    "mHipLeft", "mKneeLeft", "mAnkleLeft", "mFootLeft",
    "mToeLeft", "mTail1", "mTail2", "mTail3",
    "mTail4", "mTail5", "mTail6", "mGroin",
    "mHindLimbsRoot", "mHindLimb1Left", "mHindLimb2Left", "mHindLimb3Left",
    "mHindLimb4Left", "mHindLimb1Right", "mHindLimb2Right", "mHindLimb3Right",
    "mHindLimb4Right",
};
const char* const VOLUMES[] = {
    "PELVIS", "BUTT", "BELLY", "LEFT_HANDLE",
    "RIGHT_HANDLE", "LOWER_BACK", "CHEST", "LEFT_PEC",
    "RIGHT_PEC", "UPPER_BACK", "NECK", "HEAD",
    "L_CLAVICLE", "L_UPPER_ARM", "L_LOWER_ARM", "L_HAND",
    "R_CLAVICLE", "R_UPPER_ARM", "R_LOWER_ARM", "R_HAND",
    "R_UPPER_LEG", "R_LOWER_LEG", "R_FOOT", "L_UPPER_LEG",
    "L_LOWER_LEG", "L_FOOT",
};
}
struct reflect_data {};
typedef test_group<reflect_data> reflect_group;
typedef reflect_group::object reflect_object;
reflect_group reflect_tests("alreflectmath");

template<> template<> void reflect_object::test<1>()
{
    std::set<std::string> names(std::begin(BONES), std::end(BONES));
    for (S32 full = 0; full < 2; ++full)
    {
        if (full) names.insert(std::begin(VOLUMES), std::end(VOLUMES));
        S32 paired = 0, centre = 0;
        for (const auto& name : names)
        {
            const auto other = ALReflect::partner(name);
            ensure("partner exists: " + name, names.count(other) == 1);
            ensure_equals("involution " + name, ALReflect::partner(other), name);
            if (other == name) ++centre; else ++paired;
        }
        ensure_equals("population", (S32)names.size(), full ? 159 : 133);
        ensure_equals("pairs", paired / 2, full ? 59 : 50);
        ensure_equals("centre", centre, full ? 41 : 33);
    }
    ensure_equals(ALReflect::partner("mWristLeft"), std::string("mWristRight"));
    ensure_equals(ALReflect::partner("mWristRight"), std::string("mWristLeft"));
    ensure_equals(ALReflect::partner("L_CLAVICLE"), std::string("R_CLAVICLE"));
    ensure_equals(ALReflect::partner("LEFT_PEC"), std::string("RIGHT_PEC"));
    ensure_equals(ALReflect::partner("mFaceEyeAltLeft"), std::string("mFaceEyeAltRight"));
    ensure_equals(ALReflect::partner("mSpine1"), std::string("mSpine1"));
}
template<> template<> void reflect_object::test<2>()
{
    const LLQuaternion right_reach(1.57079632679f, LLVector3(0,0,1));
    closeVec("left reaches forward", LLVector3(0,1,0) * ALReflect::rotation(right_reach), LLVector3(1,0,0));
    closeVec("name swap alone reaches backward", LLVector3(0,1,0) * right_reach, LLVector3(-1,0,0));
}
template<> template<> void reflect_object::test<3>()
{
    Random random;
    for (S32 i=0; i<64; ++i)
    {
        const auto q = random.quat(); const auto v = random.vec();
        // Expected S is literal, independently of the header vector helper.
        const auto world = v * q;
        closeVec("S(qv) = q'(Sv)", LLVector3(v.mV[0],-v.mV[1],v.mV[2]) * ALReflect::rotation(q),
            LLVector3(world.mV[0],-world.mV[1],world.mV[2]));
    }
}
template<> template<> void reflect_object::test<4>()
{
    Random random;
    for (S32 i=0; i<32; ++i)
    {
        const auto a=random.quat(), b=random.quat(); const auto v=random.vec();
        closeVec("rotation involution", v * ALReflect::rotation(ALReflect::rotation(a)), v*a);
        closeVec("homomorphism", v * ALReflect::rotation(a*b), v*(ALReflect::rotation(a)*ALReflect::rotation(b)));
        closeVec("vector involution", ALReflect::vector(ALReflect::vector(v)), v);
    }
}
template<> template<> void reflect_object::test<5>()
{
    Random random; bool wrong_order=false, missing_s=false, yaw_only=false;
    for (F32 degrees : {0.f,37.f,90.f,179.f,-135.f})
    {
        const F32 az=degrees*0.01745329252f; const LLVector3 n(cosf(az),sinf(az),0);
        for (S32 i=0; i<32; ++i)
        {
            const auto source=random.quat(); const auto u=random.vec();
            const auto world=u*source;
            const LLVector3 expected=world-n*(2.f*(world*n));
            const LLVector3 su(u.mV[0],-u.mV[1],u.mV[2]);
            const auto clone=ALReflect::rootRotation(source,n);
            closeVec("full 3D M R S", su*clone,expected);
            const auto mr=su*source;
            missing_s |= (mr-n*(2.f*(mr*n))-expected).length()>.1f;
            const auto srm=LLQuaternion(3.14159265359f,n)*source*LLQuaternion(3.14159265359f,LLVector3(0,1,0));
            wrong_order |= (su*srm-expected).length()>.1f;
            const auto f=LLVector3(1,0,0)*source;
            const LLQuaternion yaw(2.f*az-atan2f(f.mV[1],f.mV[0])+3.14159265359f,LLVector3(0,0,1));
            yaw_only |= (su*yaw-expected).length()>.1f;
        }
    }
    ensure("distinguishes S R M",wrong_order); ensure("distinguishes M R",missing_s); ensure("distinguishes yaw only",yaw_only);
}
template<> template<> void reflect_object::test<6>()
{
    for (F32 az : {0.f,.645771823f,1.57079632679f,3.12413936f,-2.35619449f})
    {
        const LLVector3 n(cosf(az),sinf(az),0);
        const LLQuaternion source(az,LLVector3(0,0,1));
        closeVec("facing pair",LLVector3(1,0,0)*ALReflect::rootRotation(source,n),-n);
    }
}
template<> template<> void reflect_object::test<7>()
{
    ALReflect::Plane p;
    ensure_equals(ALReflect::updatePlane(p,LLVector3d(10,20,3),LLVector3d(14,20,9),LLVector3(1,0,0),false),0);
    ensure("midpoint",(p.point-LLVector3d(12,20,6)).length()<1e-8);
    closeVec("normal",p.normal,LLVector3(1,0,0));
    const auto foot=ALReflect::planeFoot(LLVector3d(11,22,7),p.point,p.normal);
    ensure("foot and height",(foot-LLVector3d(13,22,7)).length()<1e-8);
    p.valid=false;
    ensure_equals(ALReflect::updatePlane(p,LLVector3d(),LLVector3d(),LLVector3(0,1,0),false),1);
    closeVec("forward fallback",p.normal,LLVector3(0,1,0));
    ensure_equals(ALReflect::updatePlane(p,LLVector3d(),LLVector3d(),LLVector3(0,0,1),false),2);
    closeVec("last plane fallback",p.normal,LLVector3(0,1,0));
    p.valid=false;
    ensure_equals(ALReflect::updatePlane(p,LLVector3d(),LLVector3d(),LLVector3(0,0,1),false),3);
    closeVec("world X fallback",p.normal,LLVector3(1,0,0));
    ensure_equals("band holds even before degeneracy",ALReflect::updatePlane(p,LLVector3d(),LLVector3d(0,.2,0),LLVector3(0,1,0),true),4);
    for (F64 d : {.05,.1,.2,.3})
    {
        ensure_equals("hold hysteresis",ALReflect::updatePlane(p,LLVector3d(),LLVector3d(0,d,0),LLVector3(0,1,0),true),4);
        closeVec("held",p.normal,LLVector3(1,0,0));
    }
    ensure_equals(ALReflect::updatePlane(p,LLVector3d(),LLVector3d(0,.31,0),LLVector3(1,0,0),true),0);
    closeVec("recovered",p.normal,LLVector3(0,1,0));
    ensure_equals(ALReflect::updatePlane(p,LLVector3d(std::numeric_limits<F64>::quiet_NaN(),0,0),LLVector3d(),LLVector3(1,0,0),true),-1);
    ensure("NaN never propagated",p.point.isFinite() && p.normal.isFinite());
}
template<> template<> void reflect_object::test<8>()
{
    ensure_equals(ALReflect::facePartner(58),59); ensure_equals(ALReflect::facePartner(59),58);
    ensure_equals(ALReflect::facePartner(123),123);
    ensure_equals(ALReflect::faceWeight(1206,-.7f),.7f); ensure_equals(ALReflect::faceWeight(1207,1.3f),-1.3f);
    ensure_equals(ALReflect::faceWeight(1205,-.7f),-.7f);
}
template<> template<> void reflect_object::test<9>()
{
    for (const auto& pair : {std::pair<S32,S32>(4,7),{5,8},{6,9}})
    {
        ensure_equals(ALReflect::handPose(pair.first),pair.second);
        ensure_equals(ALReflect::handPose(pair.second),pair.first);
    }
    for (S32 pose : {0,1,2,3,10,11,12,13}) ensure_equals(ALReflect::handPose(pose),pose);
}
template<> template<> void reflect_object::test<10>()
{
    closeVec("source local",ALReflect::vector(LLVector3(1,2,3)),LLVector3(1,-2,3));
    closeVec("BODY local",ALReflect::constraintOffset(LLVector3(4,-5,6),false),LLVector3(4,5,6));
    closeVec("GROUND agent",ALReflect::constraintOffset(LLVector3(4,-5,6),true),LLVector3(4,-5,6));
}
// Part B: pure intent decisions and per-PLAY activation reconciliation.
template<> template<> void reflect_object::test<11>()
{
    using namespace ALReflect;
    for (F64 age : {0., .1, 9.9, 10., 11., 30., 59., 59.999})
    {
        const auto echo = decide(Intent::STOP, age, Event::REGION_START);
        ensure("late START after Stop is consumed throughout sixty seconds",echo.action == Action::CONSUME);
        ensure("echo does not renew Stop's timestamp or change intent",echo.intent == Intent::STOP);
        ensure("idle retains suppression",decide(Intent::STOP,age,Event::IDLE).intent == Intent::STOP);
    }
}
template<> template<> void reflect_object::test<12>()
{
    using namespace ALReflect;
    for (F64 age : {60., 60.001, 90.})
    {
        const auto echo = decide(Intent::STOP,age,Event::REGION_START);
        ensure("expired Stop uses stock in the same START call",echo.intent == Intent::NONE && echo.action == Action::STOCK);
        ensure("idle removes expired Stop",decide(Intent::STOP,age,Event::IDLE).intent == Intent::NONE);
    }
    for (Event event : {Event::REGION_START, Event::REGION_STOP})
        ensure("no intent leaves stock untouched",decide(Intent::NONE,0.,event).action == Action::STOCK);
}
template<> template<> void reflect_object::test<13>()
{
    using namespace ALReflect;
    for (F64 stopped_age : {0., .001, 11., 30., 59., 61.})
    {
        const auto replay = decide(Intent::STOP,stopped_age,Event::USER_PLAY);
        ensure("Play always overwrites Stop",replay.intent == Intent::PLAY && replay.action == Action::ENSURE_DERIVED);
        const auto echo = decide(replay.intent,0.,Event::REGION_START);
        ensure("first observed START always plays the replay",echo.intent == Intent::PLAY && echo.action == Action::ENSURE_DERIVED);
        ensure("READY may complete replay",decide(replay.intent,0.,Event::BUILD_READY).action == Action::ENSURE_DERIVED);
    }
}
template<> template<> void reflect_object::test<14>()
{
    using namespace ALReflect;
    const auto play = decide(Intent::STOP,30.,Event::USER_PLAY);
    // No REGION_START callback: the sim kept the original signaled and coalesced
    // the new START. Production idle reconciles that current signal immediately.
    for (F64 age : {0., 1., 11., 30., 61., 3600.})
    {
        ensure("coalesced START still reconciles without a deadline",
            decide(play.intent,age,Event::IDLE).action == Action::ENSURE_DERIVED);
        const auto ready = decide(play.intent,age,Event::BUILD_READY);
        ensure("READY needs no remembered echo",ready.intent == Intent::PLAY && ready.action == Action::ENSURE_DERIVED);
    }
}
template<> template<> void reflect_object::test<15>()
{
    using namespace ALReflect;
    const auto ended = decide(Intent::PLAY,123.,Event::NATURAL_END);
    ensure("natural end records Stop immediately and defers local/wire work",ended.intent == Intent::STOP && ended.action == Action::DEFER_STOP);
    ensure("late START cannot revive natural end",decide(ended.intent,11.,Event::REGION_START).action == Action::CONSUME);
    ensure("late READY cannot revive natural end",decide(ended.intent,1.,Event::BUILD_READY).action == Action::NONE);
    ensure("repeated natural end does not queue another stop",decide(ended.intent,1.,Event::NATURAL_END).action == Action::NONE);
    const auto replay = decide(ended.intent,0.,Event::USER_PLAY);
    ensure("Play supersedes even a deferred natural stop",decide(replay.intent,0.,Event::IDLE).action == Action::ENSURE_DERIVED);
}
template<> template<> void reflect_object::test<16>()
{
    using namespace ALReflect;
    const auto stopped = decide(Intent::PLAY,.1,Event::USER_STOP);
    ensure("user Stop during build records Stop and stops locally",stopped.intent == Intent::STOP && stopped.action == Action::STOP_LOCAL);
    for (F64 age : {0., 9., 11., 30., 59., 61.})
    {
        ensure("READY after Stop starts nothing",decide(stopped.intent,age,Event::BUILD_READY).action == Action::NONE);
        ensure("failure after Stop starts nothing even if signaled",decide(stopped.intent,age,Event::BUILD_FAILED_SIGNALED).action == Action::NONE);
        ensure("failure after Stop starts nothing without signal",decide(stopped.intent,age,Event::BUILD_FAILED_UNSIGNALED).action == Action::NONE);
    }
}
template<> template<> void reflect_object::test<17>()
{
    using namespace ALReflect;
    ensure("failure falls back locally while Play and region signal agree",
        decide(Intent::PLAY,1.,Event::BUILD_FAILED_SIGNALED).action == Action::PLAY_ORIGINAL);
    const auto unsignaled = decide(Intent::PLAY,1.,Event::BUILD_FAILED_UNSIGNALED);
    ensure("failure without a signal cannot start or resend anything",unsignaled.intent == Intent::PLAY && unsignaled.action == Action::NONE);
    ensure("a later signal can use failed-build fallback",decide(unsignaled.intent,30.,Event::BUILD_FAILED_SIGNALED).action == Action::PLAY_ORIGINAL);
    ensure("a missing intent cannot fall back",decide(Intent::NONE,0.,Event::BUILD_FAILED_SIGNALED).action == Action::NONE);
}
template<> template<> void reflect_object::test<18>()
{
    using namespace ALReflect;
    for (Intent intent : {Intent::PLAY, Intent::STOP})
    {
        const auto stopped = decide(intent,1.,Event::REGION_STOP);
        ensure("region STOP stops both ids and clears either intent",stopped.intent == Intent::NONE && stopped.action == Action::STOP_LOCAL);
        ensure("READY after region STOP starts nothing",decide(stopped.intent,0.,Event::BUILD_READY).action == Action::NONE);
        ensure("a later unrelated START is stock",decide(stopped.intent,0.,Event::REGION_START).action == Action::STOCK);
    }
    ensure("Play itself has no sixty-second expiry",decide(Intent::PLAY,3600.,Event::IDLE).intent == Intent::PLAY);
}

template<> template<> void reflect_object::test<19>()
{
    using namespace ALReflect;
    Playback play;
    ensure("READY requests once",readyDecision(play,false,false,10.).action == Action::ENSURE_DERIVED);
    play.requested = true; play.requestedAt = 10.;
    // Duration <= ease-out: activation can stop before ensure observes it.
    for (F64 now : {10., 10.5, 11., 11.999})
        ensure("instant stop waits for bounded activation window",readyDecision(play,false,false,now).action == Action::NONE);
    ensure("unobserved activation is not started",!play.started && !play.completed);
    const auto ended = readyDecision(play,false,false,12.);
    ensure("window completes once",ended.intent == Intent::STOP && ended.action == Action::DEFER_STOP && play.completed);
    ensure("completed request never queues a second stop",readyDecision(play,false,false,13.).action == Action::NONE);
    play.intent = ended.intent;
    ensure("late START cannot restart completion",decide(play.intent,0.,Event::REGION_START).action == Action::CONSUME);
}

template<> template<> void reflect_object::test<20>()
{
    using namespace ALReflect;
    Playback play;
    for (S32 replay = 0; replay < 2; ++replay)
    {
        play = {decide(play.intent,0.,Event::USER_PLAY).intent,0.,Route::LOCAL};
        ensure("Play resets all three facts",!play.requested && !play.wasLoading && !play.started && !play.completed);
        ensure("fresh Play requests once",readyDecision(play,false,false,10.).action == Action::ENSURE_DERIVED);
        play.requested = true; play.requestedAt = 10.;
        ensure("queued request does not start again",readyDecision(play,false,false,10.).action == Action::NONE);
        ensure("actual activation starts playback",readyDecision(play,true,false,10.1).action == Action::NONE && play.started);
        const auto ended = readyDecision(play,false,false,10.2);
        ensure("observed playback completes without waiting for window",ended.intent == Intent::STOP && ended.action == Action::DEFER_STOP && play.completed);
        play.intent = ended.intent;
        ensure("completed playback cannot restart",readyDecision(play,false,false,20.).action == Action::NONE);
    }
}

template<> template<> void reflect_object::test<21>()
{
    using namespace ALReflect;
    for (bool signaled : {false, true})
    {
        const auto failed = failedDecision(Intent::PLAY,Route::LOCAL,signaled);
        ensure("Local failure resolves PLAY and resets preview via local stop",
            failed.intent == Intent::STOP && failed.action == Action::STOP_LOCAL);
        ensure("Local failure cannot remain pending",decide(failed.intent,0.,Event::IDLE).action == Action::NONE);
        const auto world = failedDecision(Intent::PLAY,Route::INWORLD,signaled);
        ensure("Inworld fallback still requires current region signal",world.intent == Intent::PLAY &&
            world.action == (signaled ? Action::PLAY_ORIGINAL : Action::NONE));
    }
    ensure("cancelled Local failure stays cancelled",failedDecision(Intent::STOP,Route::LOCAL,true).action == Action::NONE);
}

template<> template<> void reflect_object::test<22>()
{
    using namespace ALReflect;
    Playback play;
    ensure("READY starts a loading derived",readyDecision(play,false,false,100.).action == Action::ENSURE_DERIVED);
    play.requested = true; play.requestedAt = 100.;
    // Controller success in the loading set is inactive even with stopped=false.
    for (F64 now : {100., 102., 110., 130.})
        ensure("loading never causes premature natural STOP",readyDecision(play,false,true,now).action == Action::NONE && !play.started && !play.completed && play.intent == Intent::PLAY);
    ensure("activation after long loading starts playback",readyDecision(play,true,false,131.).action == Action::NONE && play.started);
    const auto ended = readyDecision(play,false,false,132.);
    ensure("inactive after activation completes once",ended.action == Action::DEFER_STOP && ended.intent == Intent::STOP && play.completed);
    ensure("completion never restarts or repeats",readyDecision(play,false,false,133.).action == Action::NONE);
}

template<> template<> void reflect_object::test<23>()
{
    using namespace ALReflect;
    Playback play;
    play.requested = true; play.requestedAt = 1.;
    for (F64 now : {1., 3., 60., 3600., 86400.})
        ensure("long looping playback stays PLAY while active",readyDecision(play,true,false,now).action == Action::NONE && play.started && !play.completed && play.intent == Intent::PLAY);
}

template<> template<> void reflect_object::test<24>()
{
    using namespace ALReflect;
    Playback play;
    play.requested = true; play.requestedAt = 100.;
    for (F64 now : {100., 102., 109.})
        ensure("loading holds PLAY",readyDecision(play,false,true,now).action == Action::NONE && !play.completed && play.intent == Intent::PLAY);
    // Last loading observation was 109; the window starts when exit is observed.
    for (F64 now : {110., 111., 111.999})
        ensure("unobserved activation gets two seconds after loading",readyDecision(play,false,false,now).action == Action::NONE && !play.completed && play.intent == Intent::PLAY);
    const auto ended = readyDecision(play,false,false,112.);
    ensure("completes at exactly 112",ended.intent == Intent::STOP && ended.action == Action::DEFER_STOP && play.completed && !play.started);
    for (F64 now : {112., 113., 130.})
        ensure("post-loading completion fires exactly once",readyDecision(play,false,false,now).action == Action::NONE);
}

template<> template<> void reflect_object::test<25>()
{
    const LLVector3d point(10., 20., 3.);
    const LLVector3 normal = LLVector3::x_axis;
    const LLVector3d p = ALReflect::planePoint(LLVector3d(7., 22., 8.), point, normal);
    ensure("point reflected across offset plane", (p - LLVector3d(13., 22., 8.)).length() < 1e-6);
    const LLVector3 d = ALReflect::planeVector(LLVector3(2.f, -3.f, 4.f), normal);
    ensure("aim reflects without plane translation", (d - LLVector3(-2.f, -3.f, 4.f)).length() < 1e-6f);
    const LLVector3d tilted = ALReflect::planePoint(LLVector3d(1., 2., 5.), LLVector3d(0., 0., 3.), LLVector3::z_axis);
    ensure("point helper also handles tilted planes", (tilted - LLVector3d(1., 2., 1.)).length() < 1e-6);
}

template<> template<> void reflect_object::test<26>()
{
    // Actor looks +X, left +Y. Reflection looks -X, so its right is +Y.
    const LLVector3d key(-2., 1., 4.);
    const LLVector3d reflected = ALReflect::planePoint(key, LLVector3d::zero, LLVector3::x_axis);
    ensure("key on actor left becomes key on reflection right", (reflected - LLVector3d(2., 1., 4.)).length() < 1e-6);
    const LLVector3 reflected_forward = ALReflect::planeVector(LLVector3::x_axis, LLVector3::x_axis);
    const LLVector3 reflected_right = reflected_forward % LLVector3::z_axis;
    ensure("key lies on clone right", LLVector3(reflected) * reflected_right > 0.f);
}

template<> template<> void reflect_object::test<27>()
{
    LLVector3 normal(3.f, 4.f, 0.f); normal.normalize();
    const LLVector3d point(100., 200., -7.);
    const LLVector3d actor(97., 196., 2.), key(94., 198., 6.);
    const auto mirror_actor = ALReflect::planePoint(actor, point, normal);
    const auto mirror_key = ALReflect::planePoint(key, point, normal);
    ensure("vertical mirror preserves light height", fabs(mirror_key.mdV[2] - key.mdV[2]) < 1e-6);
    ensure("light to actor distance preserved", fabs((mirror_key - mirror_actor).length() - (key - actor).length()) < 1e-5);
    const LLVector3 direction(.2f, -.7f, .4f);
    ensure("direction length preserved", fabs(ALReflect::planeVector(direction, normal).length() - direction.length()) < 1e-6f);
    ensure("reflection is involutive", (ALReflect::planePoint(mirror_key, point, normal) - key).length() < 1e-5);
}

template<> template<> void reflect_object::test<28>()
{
    for (F32 speed : {-8.f, -1.f, 0.f, 1.f, 8.f})
    {
        ensure("rotating gobo reverses direction", ALReflect::mirrorGoboSpeed(1, speed) == -speed);
        ensure("rotation reflection is involutive",
            ALReflect::mirrorGoboSpeed(1, ALReflect::mirrorGoboSpeed(1, speed)) == speed);
        for (S32 mode : {0, 2, 3, 4, 5, 6})
            ensure("non-rotation gobo speed unchanged", ALReflect::mirrorGoboSpeed(mode, speed) == speed);
    }
}

}
