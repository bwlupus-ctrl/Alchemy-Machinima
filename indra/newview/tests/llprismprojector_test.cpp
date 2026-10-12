/**
 * @file llprismprojector_test.cpp
 * @brief Pure VP1/VP2 projector contract regressions.
 */

#include "linden_common.h"

#include "../test/lltut.h"
#include "../llprismlens.h"

namespace tut
{
struct prism_projector_data {};
typedef test_group<prism_projector_data> prism_projector_group;
typedef prism_projector_group::object prism_projector_object;
prism_projector_group prism_projector_tests("LLPrismProjector");

template<> template<>
void prism_projector_object::test<1>()
{
    set_test_name("projector schema defaults are pipeline-neutral");
    LLPrismLens::ProjectorDefinition projector;
    ensure("object is the VP1 target kind",
           projector.mTarget == LLPrismLens::EProjectorTarget::OBJECT);
    ensure("default fit is FIT",
           projector.mSettings.mFitMode == LLPrismLens::EFitMode::FIT);
    ensure_equals("default prep resolution", projector.mSettings.mResolution,
                  static_cast<U16>(512));
    ensure("default no-signal preserves the object's cookie",
           projector.mSettings.mNoSignal ==
               LLPrismLens::ENoSignal::OWN_COOKIE);
    ensure("unresolved targets read OFFLINE",
           projector.mRuntime.mHealth == LLPrismLens::EDisplayHealth::OFFLINE);
}

template<> template<>
void prism_projector_object::test<2>()
{
    set_test_name("bootstrap demand expires after thirty frames");
    using LLPrismLens::ProjectorDemandPolicy::active;
    ensure("bind frame is immediately demanded", active(100, 0, 100, true));
    ensure("thirtieth bootstrap frame is demanded", active(129, 0, 100, true));
    ensure("bootstrap has no thirty-first-frame credit",
           !active(130, 0, 100, true));
    ensure("offline targets never bootstrap", !active(100, 0, 100, false));
}

template<> template<>
void prism_projector_object::test<3>()
{
    set_test_name("consumption demand stops within two frames");
    using LLPrismLens::ProjectorDemandPolicy::active;
    ensure("consumption frame is demanded", active(200, 200, 0, true));
    ensure("two-frame grace remains demanded", active(202, 200, 0, true));
    ensure("third frame is idle", !active(203, 200, 0, true));
}

template<> template<>
void prism_projector_object::test<4>()
{
    set_test_name("capacity and handle domains remain separate from displays");
    ensure_equals("projector capacity", LLPrismLens::MAX_PROJECTOR_BINDINGS, 8u);
    ensure("display capacity is untouched",
           LLPrismLens::MAX_DISPLAY_BINDINGS == 16u);
    LLPrismLens::ProjectorHandle projector;
    LLPrismLens::DisplayHandle display;
    ensure("new projector handle is stale", projector.mId.isNull() &&
           projector.mGeneration == 0);
    ensure("new display handle remains independent", display.mId.isNull() &&
           display.mGeneration == 0);
}

template<> template<>
void prism_projector_object::test<5>()
{
    set_test_name("empty registry snapshot hides projector state");
    LLPrismLens::RegistrySnapshot snapshot;
    ensure_equals("no projector rows", snapshot.mProjectorCount, 0u);
    ensure_equals("fixed projector storage", snapshot.mProjectors.size(),
                  static_cast<std::size_t>(LLPrismLens::MAX_PROJECTOR_BINDINGS));
}

template<> template<>
void prism_projector_object::test<6>()
{
    set_test_name("projector look LLSD round trip preserves every field");
    LLPrismLens::ProjectorLook source;
    source.mKeystoneH = -0.25f;
    source.mKeystoneV = 0.5f;
    source.mShutterFlicker = 0.35f;
    source.mShutterHz = 18.f;
    source.mGateWeave = 0.4f;
    source.mHotspot = 0.6f;
    source.mLampKelvin = 3400.f;
    source.mGelTint = LLColor3(0.8f, 0.7f, 0.6f);
    source.mDust = 0.1f;
    source.mScratches = 0.2f;
    source.mBurn = 0.3f;
    source.mGrain = 0.4f;
    source.mHalation = 0.5f;
    source.mThrowMode = LLPrismLens::EProjectorThrow::CAMERA_FOV;
    LLPrismLens::ProjectorLook result;
    ensure("look parses", LLPrismLens::projectorLookFromLLSD(
        LLPrismLens::projectorLookToLLSD(source), result));
    ensure_equals("keystone H", result.mKeystoneH, source.mKeystoneH);
    ensure_equals("keystone V", result.mKeystoneV, source.mKeystoneV);
    ensure_equals("shutter", result.mShutterFlicker, source.mShutterFlicker);
    ensure_equals("shutter Hz", result.mShutterHz, source.mShutterHz);
    ensure_equals("weave", result.mGateWeave, source.mGateWeave);
    ensure_equals("hotspot", result.mHotspot, source.mHotspot);
    ensure_equals("Kelvin", result.mLampKelvin, source.mLampKelvin);
    ensure("gel", result.mGelTint == source.mGelTint);
    ensure_equals("dust", result.mDust, source.mDust);
    ensure_equals("scratches", result.mScratches, source.mScratches);
    ensure_equals("burn", result.mBurn, source.mBurn);
    ensure_equals("grain", result.mGrain, source.mGrain);
    ensure_equals("halation", result.mHalation, source.mHalation);
    ensure("throw", result.mThrowMode == source.mThrowMode);
}

template<> template<>
void prism_projector_object::test<7>()
{
    set_test_name("default look is a strict feed and bind no-op");
    LLPrismLens::ProjectorLook look;
    ensure("all contract defaults", look.isDefault());
    ensure("default selects legacy feed shader", !look.hasFeedEffects());
    ensure("throw defaults off",
           look.mThrowMode == LLPrismLens::EProjectorThrow::OFF);
}

template<> template<>
void prism_projector_object::test<8>()
{
    set_test_name("6500 Kelvin is exact identity");
    ensure("D65 identity",
           LLPrismLens::projectorKelvinRgb(6500.f) == LLColor3(1.f, 1.f, 1.f));
}

template<> template<>
void prism_projector_object::test<9>()
{
    set_test_name("zero keystone is exact identity");
    F32 u = 0.f;
    F32 v = 0.f;
    bool inside = false;
    LLPrismLens::projectorKeystoneUv(0.173f, 0.827f, 0.f, 0.f,
                                     u, v, inside);
    ensure("inside", inside);
    ensure_approximately_equals("U identity", u, 0.173f, 20);
    ensure_approximately_equals("V identity", v, 0.827f, 20);
}

template<> template<>
void prism_projector_object::test<10>()
{
    set_test_name("look persistence clamps values and defaults malformed gel");
    LLSD data = LLSD::emptyMap();
    data["keystone_h"] = 9.f;
    data["shutter_hz"] = 1.f;
    data["dust"] = -2.f;
    data["gel_tint"] = "not a colour";
    LLPrismLens::ProjectorLook look;
    ensure("look parses", LLPrismLens::projectorLookFromLLSD(data, look));
    ensure_equals("keystone clamps", look.mKeystoneH, 1.f);
    ensure_equals("shutter rate clamps", look.mShutterHz, 12.f);
    ensure_equals("dust clamps", look.mDust, 0.f);
    ensure("malformed gel defaults white",
           look.mGelTint == LLColor3(1.f, 1.f, 1.f));
}

template<> template<>
void prism_projector_object::test<11>()
{
    set_test_name("rig projector resolves again after emitter rebuild");
    LLPrismLens::ProjectorDefinition binding;
    binding.mTarget = LLPrismLens::EProjectorTarget::RIG_LIGHT;
    binding.mRigSlot = 2;
    binding.mRigLight = 0;
    ensure("missing rebuilt emitter quarantines",
           !LLPrismLens::resolveRigProjectorState(binding, 5, 4, LLUUID::null));
    ensure("binding is quarantined while emitter is absent", binding.mQuarantined);
    LLUUID rebuilt_emitter;
    rebuilt_emitter.generate();
    ensure("stable rig identity resolves the rebuilt emitter",
           LLPrismLens::resolveRigProjectorState(binding, 5, 4, rebuilt_emitter));
    ensure("rebuilt object replaces the stale resolution",
           binding.mObjectId == rebuilt_emitter && !binding.mQuarantined);
}

template<> template<>
void prism_projector_object::test<12>()
{
    set_test_name("stale rig slot is quarantined");
    LLPrismLens::ProjectorDefinition binding;
    binding.mTarget = LLPrismLens::EProjectorTarget::RIG_LIGHT;
    binding.mRigSlot = 5;
    binding.mRigLight = 0;
    LLUUID emitter;
    emitter.generate();
    ensure("stale slot cannot resolve",
           !LLPrismLens::resolveRigProjectorState(binding, 5, 4, emitter));
    ensure("stale target is retained but quarantined",
           binding.mQuarantined && binding.mObjectId.isNull() &&
           !binding.mQuarantineReason.empty());
}

template<> template<>
void prism_projector_object::test<13>()
{
    set_test_name("unknown loaded projector target kind is skipped");
    std::vector<LLPrismLens::ProjectorDefinition> loaded;
    LLPrismLens::ProjectorDefinition binding;
    ensure("future target kind reports a non-fatal skip",
           !LLPrismLens::appendLoadedProjector(loaded, "future_fixture", binding));
    ensure_equals("unknown target was not installed", loaded.size(), std::size_t(0));
    ensure("known object target installs",
           LLPrismLens::appendLoadedProjector(loaded, "object", binding));
    ensure_equals("known target was installed", loaded.size(), std::size_t(1));
}

template<> template<>
void prism_projector_object::test<14>()
{
    set_test_name("rig projector reassignment requires a Camera Feed capture");
    LLPrismLens::ProjectorDefinition binding;
    binding.mTarget = LLPrismLens::EProjectorTarget::RIG_LIGHT;
    ensure("surface lens is rejected for rig projector",
           LLPrismLens::validateProjectorReassignment(
               binding, LLPrismLens::ECaptureMode::SURFACE_LENS) ==
               LLPrismLens::ERegistryResult::INVALID_CONFIGURATION);
    ensure("camera feed is accepted for rig projector",
           LLPrismLens::validateProjectorReassignment(
               binding, LLPrismLens::ECaptureMode::CAMERA_FEED) ==
               LLPrismLens::ERegistryResult::OK);
}

template<> template<>
void prism_projector_object::test<15>()
{
    set_test_name("loaded point light transitions into quarantine");
    LLPrismLens::ProjectorDefinition binding;
    LLPrismLens::updateLoadedProjectorQuarantine(
        binding, true, true, false, true, false);
    ensure("point light binding is retained in quarantine",
           binding.mQuarantined && !binding.mQuarantineReason.empty());
    LLPrismLens::updateLoadedProjectorQuarantine(
        binding, true, true, false, true, true);
    ensure("restored spotlight leaves quarantine",
           !binding.mQuarantined && binding.mQuarantineReason.empty());
}
}
