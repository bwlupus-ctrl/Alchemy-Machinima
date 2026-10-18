/**
 * @file llprismprojector_test.cpp
 * @brief Pure VP1/VP2 projector contract regressions.
 */

#include "linden_common.h"

#include "../test/lltut.h"
#include "../llprismlens.h"
#include "../alvcamautovol.h"
#include "../alflareenergy.h"
#include "../alvcammediafilelifecycle.h"
#include <array>
#include "../../llplugin/llmediaframestate.h"
#include "../../llplugin/llpluginclassmedia.h"
#include "../../llplugin/llpluginloopseam.h"
#include "../../llplugin/llpluginmessage.h"
#include "../../llplugin/llpluginmessageclasses.h"
#include "llcontrol.h"
#include "llerrorcontrol.h"

// llpluginclassmedia.obj (now linked via the media slot paths) references the viewer global.
LLControlGroup gSavedSettings("PrismProjectorTest");

#include <limits>
#include <map>
#include <sstream>

namespace tut
{
template<class T> concept public_camera_fields =
    requires(T value) { value.mMode; } ||
    requires(T value) { value.mCamera; } ||
    requires(T value) { value.mCameraObjectId; };
static_assert(!public_camera_fields<LLPrismLens::SourceSlotData>);
static_assert(!public_camera_fields<LLPrismLens::CaptureDefinition>);

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
    set_test_name("projector reassignment uses source capability and explicit media kind");
    LLPrismLens::SourceSlotData source;
    ensure("empty slot has no default camera payload", !source.cameraData() && !source.isSurfaceLens());
    source.setCameraData(LLPrismLens::CameraPayload());
    LLPrismLens::ProjectorDefinition binding;
    binding.mTarget = LLPrismLens::EProjectorTarget::RIG_LIGHT;
    ensure("surface lens is rejected for rig projector",
           LLPrismLens::validateProjectorReassignment(
               binding, source) ==
               LLPrismLens::ERegistryResult::INVALID_CONFIGURATION);
    source.cameraData()->mMode = LLPrismLens::ECaptureMode::CAMERA_FEED;
    ensure("camera feed is accepted for rig projector",
           LLPrismLens::validateProjectorReassignment(
               binding, source) ==
               LLPrismLens::ERegistryResult::OK);
    source.setMediaData(LLPrismLens::MediaFaceSource());
    ensure("media capability is explicit", source.cameraData() == nullptr && !source.isSurfaceLens());
    ensure("media is eligible for shared routing", LLPrismLens::validateProjectorReassignment(binding, source) == LLPrismLens::ERegistryResult::OK);
    binding.mGateSubscribed = true;
    ensure("media cannot inherit a gate subscription", LLPrismLens::validateProjectorReassignment(binding, source) == LLPrismLens::ERegistryResult::INVALID_CONFIGURATION);
    binding.mGateSubscribed = false;
    source.setMediaUrlData(LLPrismLens::MediaUrlSource());
    ensure("private kind has no camera payload", source.cameraData() == nullptr && source.mediaData() == nullptr && source.isMedia());
    ensure("private route capability", LLPrismLens::validateProjectorReassignment(binding, source) == LLPrismLens::ERegistryResult::OK);
    ensure("both media output paths", LLPrismLens::isMediaKind(source.sourceKind()));
    ensure("private defaults", source.mediaUrlData()->mLoop && source.mediaUrlData()->mVolume == 0.f && source.mediaUrlData()->mValue.empty());
    binding.mGateSubscribed = true;
    ensure("private media cannot inherit a gate subscription", LLPrismLens::validateProjectorReassignment(binding, source) == LLPrismLens::ERegistryResult::INVALID_CONFIGURATION);

}

template<> template<>
void prism_projector_object::test<15>()
{
    set_test_name("loaded point light is a valid local projector target");
    LLPrismLens::ProjectorDefinition binding;
    LLPrismLens::updateLoadedProjectorQuarantine(
        binding, true, true, false, true, false);
    ensure("point light binding is active",
           !binding.mQuarantined && binding.mQuarantineReason.empty());
    LLPrismLens::updateLoadedProjectorQuarantine(
        binding, true, true, false, false, false);
    ensure("non-light target is quarantined with a reason",
           binding.mQuarantined && !binding.mQuarantineReason.empty());
}

template<> template<>
void prism_projector_object::test<16>()
{
    set_test_name("projector lamp power persistence and validation");
    LLPrismLens::ProjectorSettings source;
    source.mLampPowerEV = 7.5f;
    LLPrismLens::ProjectorSettings result;
    ensure("lamp power parses", LLPrismLens::projectorLampPowerFromLLSD(
        LLPrismLens::projectorLampPowerToLLSD(source), result));
    ensure_equals("lamp power round trips", result.mLampPowerEV,
                  source.mLampPowerEV);
    ensure("non-zero lamp power is not identity", !result.isLampPowerIdentity());

    LLSD invalid = LLSD::emptyMap();
    invalid["lamp_power_ev"] = 8.1f;
    ensure("out-of-range lamp power is rejected",
           !LLPrismLens::projectorLampPowerFromLLSD(invalid, result));
    invalid["lamp_power_ev"] = std::numeric_limits<F32>::infinity();
    ensure("non-finite lamp power is rejected",
           !LLPrismLens::projectorLampPowerFromLLSD(invalid, result));

    LLPrismLens::ProjectorSettings legacy;
    legacy.mLampPowerEV = 4.f;
    ensure("old scene without lamp power parses",
           LLPrismLens::projectorLampPowerFromLLSD(LLSD::emptyMap(), legacy));
    ensure_equals("old scene defaults lamp power", legacy.mLampPowerEV, 0.f);
}

template<> template<>
void prism_projector_object::test<17>()
{
    set_test_name("binding a point light creates and removes a local projector");
    LLPrismLens::ProjectorDefinition binding;
    ensure("point light accepts a binding",
           LLPrismLens::updateLocalProjectorBindingState(
               binding, true, true, false));
    ensure("bound point light has a local projector", binding.mLocalProjector);
    LLPrismLens::updateLocalProjectorBindingState(
        binding, false, true, false);
    ensure("unbind restores the point light", !binding.mLocalProjector);
    ensure("capture deletion removes even a Gate-subscribed binding",
           LLPrismLens::removeProjectorWithCapture(true, true));
    ensure("real spotlight needs no local override",
           !LLPrismLens::usesLocalProjectorOverride(true, true, true));
}

template<> template<>
void prism_projector_object::test<18>()
{
    set_test_name("non-light cannot become a local projector");
    LLPrismLens::ProjectorDefinition binding;
    ensure("non-light rejected",
           !LLPrismLens::updateLocalProjectorBindingState(
               binding, true, false, false));
    ensure("non-light has no override", !binding.mLocalProjector);
    ensure("loader rejects non-light with a reason",
           LLPrismLens::quarantineLoadedProjectorTarget(
               true, true, false, false, false));
}

template<> template<>
void prism_projector_object::test<19>()
{
    set_test_name("local projector flag LLSD round trip");
    const LLSD data = LLPrismLens::localProjectorToLLSD(true);
    ensure("local projector survives round trip",
           LLPrismLens::localProjectorFromLLSD(data));
    ensure("old scenes default to a normal projector",
           !LLPrismLens::localProjectorFromLLSD(LLSD::emptyMap()));
}

template<> template<>
void prism_projector_object::test<20>()
{
    set_test_name("camera label normalization supports scene-safe renames");
    ensure_equals("surrounding whitespace is trimmed",
        LLPrismLens::normalizeCaptureLabel("  Close Up  ", "Virtual 1"),
        std::string("Close Up"));
    ensure_equals("empty input restores the supplied scene default",
        LLPrismLens::normalizeCaptureLabel(" \t ", "Virtual 1"),
        std::string("Virtual 1"));
    ensure_equals("names are capped at 32 characters",
        LLPrismLens::normalizeCaptureLabel(std::string(40, 'x'), "Virtual 1"),
        std::string(32, 'x'));
}

template<> template<>
void prism_projector_object::test<21>()
{
    set_test_name("volumetric decline belongs to the stable light identity");
    LLPrismLens::ProjectorDefinition projector;
    projector.mObjectId.generate();
    const std::string light_a = LLPrismLens::projectorVolumetricIdentity(projector);
    std::map<std::string, bool> declined;
    declined[light_a] = true;
    projector.mObjectId.generate(); // retarget the same projector from A to B
    const std::string light_b = LLPrismLens::projectorVolumetricIdentity(projector);
    ALVCamAutoVol::EnableOverride emitter;
    ensure("retargeted light B starts un-declined", emitter.acquire(false, !declined[light_b]));
    projector = LLPrismLens::ProjectorDefinition(); // unassign
    ensure("unassigned projector carries no light A decline",
        !declined[LLPrismLens::projectorVolumetricIdentity(projector)]);
    ensure("light A retains its own decline", declined[light_a]);

    projector.mTarget = LLPrismLens::EProjectorTarget::RIG_LIGHT;
    projector.mRigSlot = 1;
    projector.mRigLight = 2;
    const std::string rig_light = LLPrismLens::projectorVolumetricIdentity(projector);
    declined[rig_light] = true;
    projector.mObjectId.generate(); // rebuilt emitter for the same slot and role
    ensure_equals("rig identity survives emitter replacement",
        LLPrismLens::projectorVolumetricIdentity(projector), rig_light);
    emitter = ALVCamAutoVol::EnableOverride();
    ensure("rebuilt light retains its decline", !emitter.acquire(false, !declined[rig_light]));
    ++projector.mRigLight;
    ensure("another rig role has its own choice",
        !declined[LLPrismLens::projectorVolumetricIdentity(projector)]);
    --projector.mRigLight;
    ++projector.mRigSlot;
    ensure("another rig slot has its own choice",
        !declined[LLPrismLens::projectorVolumetricIdentity(projector)]);
}

template<> template<>
void prism_projector_object::test<22>()
{
    set_test_name("VLC load tokens reject missing and older frame/geometry provenance");
    ensure("missing token", !LLMediaFrame::acceptsToken("B", ""));
    ensure("old callback", !LLMediaFrame::acceptsToken("B", "A"));
    ensure("rapid navigation", !LLMediaFrame::acceptsToken("C", "B"));
    ensure("only current load", LLMediaFrame::acceptsToken("C", "C"));
    LLMediaFrame::UploadState state;
    const LLMediaFrame::Identity old{1, 1, "A"}, current{2, 1, "B"};
    ensure("stale success cannot publish", !state.finish(old, current, true));
    ensure_equals("no completed serial", state.serial, U64(0));
}

template<> template<>
void prism_projector_object::test<23>()
{
    set_test_name("paused frame retry and enqueue rejection roll back all ownership");
    LLMediaFrame::UploadState state;
    const LLMediaFrame::Identity current{1, 1, "A"};
    state.pending = true;
    bool texture_pending = true;
    int impl_refs = 1, texture_refs = 1;
    {
        LLMediaFrame::TerminalCleanup rejected;
        ++impl_refs; ++texture_refs;
        rejected.release = [&]() { texture_pending = false; state.pending = false; --impl_refs; --texture_refs; };
        ensure("rejection retains failed work", !state.finish(current, current, false) && state.retry);
    }
    ensure("pending latch rolled back", !state.pending && !texture_pending);
    ensure_equals("impl ref rolled back", impl_refs, 1);
    ensure_equals("texture ref rolled back", texture_refs, 1);
    ensure("retry succeeds without another frame", state.finish(current, current, true));
    ensure("ready exactly once", state.ready && !state.retry && state.serial == 1);
    state.invalidate();
    ensure("layout replacement discards retries", !state.ready && !state.retry);
    ensure("stale failure cannot re-dirty", !state.finish(current, {1, 2, "A"}, false) && !state.retry);
}

template<> template<>
void prism_projector_object::test<24>()
{
    set_test_name("source aspect is independent of square/padded MoaP allocations");
    using namespace LLPrismLens;
    ensure("Auto fallback", fabsf(resolveSourceAspect(ESourceAspect::AUTO, 0, 0) - 16.f/9.f) < .0001f);
    ensure("PAL anamorphic widescreen", fabsf(resolveSourceAspect(ESourceAspect::AUTO, 720, 576, 64, 45) - 16.f/9.f) < .0001f);
    ensure("PAL anamorphic standard", fabsf(resolveSourceAspect(ESourceAspect::AUTO, 720, 576, 16, 15) - 4.f/3.f) < .0001f);
    ensure("NTSC anamorphic widescreen", fabsf(resolveSourceAspect(ESourceAspect::AUTO, 720, 480, 32, 27) - 16.f/9.f) < .0001f);
    ensure("invalid SAR uses square pixels", fabsf(resolveSourceAspect(ESourceAspect::AUTO, 720, 576, 0, 45) - 1.25f) < .0001f);
    ensure("manual ignores SAR", fabsf(resolveSourceAspect(ESourceAspect::STANDARD_4_3, 720, 576, 64, 45) - 4.f/3.f) < .0001f);
    ensure("Auto portrait", fabsf(resolveSourceAspect(ESourceAspect::AUTO, 1080, 1920) - 9.f/16.f) < .0001f);
    ensure("manual wide wins", fabsf(resolveSourceAspect(ESourceAspect::WIDE_16_9, 4, 3) - 16.f/9.f) < .0001f);
    ensure("manual standard wins", fabsf(resolveSourceAspect(ESourceAspect::STANDARD_4_3, 1920, 1080) - 4.f/3.f) < .0001f);
    ensure("manual cinema wins", fabsf(resolveSourceAspect(ESourceAspect::CINEMA_239, 1, 1) - 2.39f) < .0001f);
    ensure("mapping bounds", LLMediaFrame::validPixels(1024*1024*4, 1024, 1024, 4, 0, 0, 1024, 1024));
    ensure("short region rejected", !LLMediaFrame::validPixels(15, 2, 2, 4, 0, 0, 2, 2));
    ensure("overflow rejected", !LLMediaFrame::validPixels(1024, 0x7fffffff, 0x7fffffff, 4, 0, 0, 1, 1));
}

template<> template<>
void prism_projector_object::test<25>()
{
    set_test_name("existing untagged source survives OFF ON OFF tracking transitions");
    LLMediaFrame::TrackingState source;
    source.configure(false, true);
    ensure("OFF accepts original untagged pixels", source.acceptsUpdate("", ""));
    ensure("OFF uses legacy uploads and loads", !source.active && !source.tokenLoads);

    source.configure(true, true);
    ensure("ON requests token loads", source.tokenLoads);
    ensure("existing untagged player stays legacy pending notification", !source.active);
    ensure("untagged pixels are consumed while enabling", source.acceptsUpdate("A", ""));
    ensure("untagged pixels do not activate tracking", !source.active);
    source.configure(false, true); // Disable before the plugin processes enable.
    ensure("OFF again accepts untagged pixels", source.acceptsUpdate("", ""));
    ensure("late enable reply cannot activate OFF", source.acceptsUpdate("", "A") && !source.active);
    ensure("OFF stops generating token loads", !source.tokenLoads);

    source.configure(true, true);
    ensure("older subscription is rejected", !source.acceptsUpdate("B", "A"));
    ensure("still legacy until current tagged frame", !source.active);
    ensure("retained current-player frame activates tracking", source.acceptsUpdate("B", "B") && source.active);
    ensure("tracked source rejects stale untagged callbacks", !source.acceptsUpdate("B", ""));
    ensure("repeated ON leaves active subscription intact", !source.configure(true, true) && source.active);
    source.configure(false, true);
    ensure("tracked player returns to legacy immediately", !source.active && !source.tokenLoads);
    ensure("legacy notifications resume", source.acceptsUpdate("", ""));

    source.configure(true, true);
    ensure("re-enable waits for its own token", !source.acceptsUpdate("C", "B") && !source.active);
    ensure("re-enable accepts retained current pixels", source.acceptsUpdate("C", "C") && source.active);
    source.awaitFrame(); // Navigation while ON must also wait for that load.
    ensure("new load uses legacy until tagged", !source.active);
    ensure("previous load cannot activate new load", !source.acceptsUpdate("D", "C"));
    ensure("new load's tagged pixels activate", source.acceptsUpdate("D", "D") && source.active);
}

template<> template<>
void prism_projector_object::test<26>()
{
    set_test_name("disable and source replacement retain the tracked upload guard");
    LLMediaFrame::TrackingState source;
    LLMediaFrame::UploadState upload;
    const LLMediaFrame::Identity vlc{1, 1, "A"}, cef{2, 1, ""};
    source.configure(true, true);
    ensure("VLC tracking active", source.acceptsUpdate("A", "A") && source.active);
    {
        LLMediaFrame::TerminalCleanup terminal;
        upload.pending = true;
        terminal.release = [&]() { upload.pending = false; };
        source.configure(false, true);
        upload.invalidate(); // Disable invalidates the accepted layout.
        ensure("disable preserves upload protection", !source.active && upload.pending);
        source = LLMediaFrame::TrackingState(); // Replacement accelerated CEF.
        upload.invalidate();
        ensure("replacement preserves upload protection", upload.pending);
        ensure("stale VLC completion cannot publish", !upload.finish(vlc, cef, true));
        ensure("no stale completed image", !upload.ready && upload.serial == 0);
    }
    ensure("terminal cleanup permits OFF imports", !upload.pending);
    ensure("OFF without tracked jobs uses legacy", !source.active && !upload.pending);
}

template<> template<>
void prism_projector_object::test<27>()
{
    set_test_name("accepted cookies survive pending frames and reject replaced identities");
    using namespace LLPrismLens;
    ProjectorFeed accepted;
    accepted.mProjector.mId.generate();
    accepted.mProjector.mGeneration = 3;
    accepted.mCapture.mId.generate();
    accepted.mCapture.mGeneration = 7;
    accepted.mSourceKind = ESourceSlotKind::MEDIA_FACE;
    accepted.mSourceEpoch = 11;
    accepted.mLoadToken = "load-A";
    accepted.mGeneration = accepted.mAcceptedSerial = 23;
    auto pending = accepted;
    pending.mGeneration = pending.mAcceptedSerial = 24;
    pending.mOutputWidth = 1024;
    pending.mSettings.mResolution = 1024;
    pending.mSettings.mFlipH = true;
    ensure("failed recompose retains old pixels and metadata", sameProjectorSource(accepted, pending));
    ensure_equals("accepted resolution remains independent", accepted.mSettings.mResolution, U16(512));
    pending.mLoadToken = "load-B";
    ensure("same impl navigation rejects old cookie", !sameProjectorSource(accepted, pending));
    pending = accepted; ++pending.mSourceEpoch;
    ensure("policy loss/source replacement invalidates", !sameProjectorSource(accepted, pending));
    pending = accepted; ++pending.mCapture.mGeneration;
    ensure("slot reuse invalidates", !sameProjectorSource(accepted, pending));
    pending = accepted; ++pending.mProjector.mGeneration;
    ensure("binding reuse invalidates", !sameProjectorSource(accepted, pending));
    pending = accepted; pending.mSourceKind = ESourceSlotKind::CAMERA;
    ensure("source kind is identity", !sameProjectorSource(accepted, pending));
}

template<> template<>
void prism_projector_object::test<28>()
{
    set_test_name("slot demand releases on freeze policy loss and final consumer loss");
    using namespace LLPrismLens;
    const F64 face = mediaDemandArea(true, true, true, 512, 256);
    const F64 light = mediaDemandArea(true, true, true, 1024, 1024);
    ensure_equals("fan-out takes maximum, never sum", llmax(face, light), 1048576.0);
    ensure_equals("frozen owner releases demand", mediaDemandArea(false, true, true, 1024, 1024), 0.0);
    ensure_equals("other owner still contributes", llmax(face, mediaDemandArea(false, true, true, 1024, 1024)), face);
    ensure_equals("policy revocation releases demand", mediaDemandArea(true, false, true, 1024, 1024), 0.0);
    ensure_equals("last consumer loss releases demand", mediaDemandArea(true, true, false, 1024, 1024), 0.0);
    ensure_equals("demand never exceeds slot capacity", mediaDemandArea(true, true, true, 4096, 4096), light);
}

template<> template<>
void prism_projector_object::test<29>()
{
    set_test_name("media volumetric acquisition requires both enables on every query");
    using namespace LLPrismLens;
    for (bool master : {false, true})
        for (bool media : {false, true})
        {
            ensure("media uses both gates", projectorVolumetricsEligible(master, media, true) == (master && media));
            ensure("camera depends only on master", projectorVolumetricsEligible(master, media, false) == master);
        }
}


template<> template<>
void prism_projector_object::test<30>()
{
    set_test_name("camera-only scene replacement releases private playback and demand");
    using namespace LLPrismLens;
    CaptureDefinition slot;
    slot.mHandle.mGeneration = 7;
    MediaUrlSource video;
    video.mAspect = ESourceAspect::CINEMA_239;
    slot.setMediaUrlData(video);
    ensure("URL fixture has no face payload",!slot.mediaData());
    ensure("URL aspect uses tagged accessor",slot.mediaAspect() == ESourceAspect::CINEMA_239);
    std::array<std::shared_ptr<int>,1> owners{std::make_shared<int>(7)};
    std::weak_ptr<int> owner = owners[0];
    auto matches = [&](unsigned)
    { return ALMediaFile::matchingPrivateOwner(true,slot.mediaUrlData()!=nullptr,
        slot.mHandle.mGeneration,7,true); };
    ensure_equals("playing URL has bootstrap demand",ALMediaFile::privateOwnerDemand(matches(0),false,true,true),1048576.);
    // Apply a camera-only scene (new generation); no player/GL/world required.
    slot = CaptureDefinition();
    slot.mHandle.mGeneration = 8;
    slot.setCameraData(CameraPayload());
    ensure_equals("orphan demand disappears before reconciliation",ALMediaFile::privateOwnerDemand(matches(0),false,true,true),0.);
    ALMediaFile::sweepPrivateOwners(owners,matches,[&](unsigned index) { owners[index].reset(); });
    ensure("reconcile releases the private owner",owner.expired());
    ensure_equals("camera-only scene has zero private demand",ALMediaFile::privateOwnerDemand(matches(0),false,true,true),0.);
    slot.setMediaUrlData(MediaUrlSource());
    ensure("same kind with reused slot is still stale",!matches(0));
}

template<> template<>
void prism_projector_object::test<31>()
{
    set_test_name("scene apply core retires every owner before replacing source slots");
    using namespace LLPrismLens;
    std::array<CaptureDefinition,2> slots;
    slots[0].setMediaUrlData(MediaUrlSource());
    slots[1].setCameraData(CameraPayload());
    std::array<std::shared_ptr<int>,2> owners{std::make_shared<int>(7),std::make_shared<int>(8)};
    std::weak_ptr<int> video = owners[0], orphan = owners[1];
    unsigned stopped = 0;
    ALMediaFile::replaceSceneSources(slots,[&](unsigned slot)
    {
        ensure("all owners retire before any source is replaced",slots[0].mediaUrlData() != nullptr);
        owners[slot].reset(); ++stopped;
    });
    slots[0].setCameraData(CameraPayload());
    ensure_equals("scene commit visits camera and media slots",stopped,2u);
    ensure("scene commit retires playback and pre-existing orphan",video.expired() && orphan.expired());
    ensure("new scene contains no private source",!slots[0].mediaUrlData() && !slots[1].mediaUrlData());
}

template<> template<>
void prism_projector_object::test<32>()
{
    set_test_name("face Browse survives reconciliation and converts only on a current pick");
    using namespace LLPrismLens;
    SourceSlotData slot;
    MediaFaceSource face; face.mAspect = ESourceAspect::CINEMA_239;
    slot.setMediaData(face);
    ALMediaFile::FilePickerTicket picker;
    const auto ticket = picker.begin();
    std::array<std::shared_ptr<int>,1> owners{}; // Browse creates no privateOwner
    ALMediaFile::sweepPrivateOwners(owners,[](unsigned) { return false; },[&](unsigned) { picker.cancel(); });
    ensure("opening picker preserves face source",slot.mediaData() != nullptr);
    ensure("current picker survives owner reconciliation",picker.picked(ticket,true,[&]() { slot.setMediaUrlData(slot.fileBrowseCue()); }));
    ensure("chosen path converts to URL source",slot.mediaUrlData() != nullptr);
    ensure("conversion preserves face aspect",slot.mediaAspect() == ESourceAspect::CINEMA_239);
    ensure("converted cue is empty pending validation",slot.mediaUrlData()->mValue.empty());
    ensure("conversion defaults remain muted and looping",slot.mediaUrlData()->mVolume == 0.f && slot.mediaUrlData()->mLoop);
    const auto stale = picker.begin();
    picker.cancel();
    bool applied = false;
    ensure("replacement cancels picker",!picker.picked(stale,true,[&]() { applied = true; }));
    ensure("caller selection loss cancels picker",!picker.picked(picker.begin(),false,[&]() { applied = true; }));
    ensure("stale picker cannot mutate cue",!applied);
}

template<> template<> void prism_projector_object::test<33>()
{
    set_test_name("Browse preflight failure and cancellation retain the committed face");
    using namespace LLPrismLens;
    SourceSlotData slot;
    MediaFaceSource face;
    face.mObjectId.generate(); face.mTE = 7; face.mAspect = ESourceAspect::CINEMA_239;
    slot.setMediaData(face);
    std::string status;
    const auto commit = [&]()
    {
        auto cue = slot.fileBrowseCue(); cue.mValue = "C:/valid.mov";
        cue.mLocation = MediaUrlSource::Location::LOCAL_FILE;
        slot.setMediaUrlData(cue);
    };
    ensure("failed preflight cannot commit",!ALMediaFile::commitFileBrowse(status,ALMediaFile::NOT_VIDEO,false,true,commit));
    ensure("face kind survives failure",slot.mediaData() != nullptr && !slot.mediaUrlData());
    ensure("face object survives failure",slot.mediaData()->mObjectId == face.mObjectId);
    ensure_equals("face TE survives failure",slot.mediaData()->mTE,face.mTE);
    ensure_equals("failed preflight status visible",status,std::string(ALMediaFile::NOT_VIDEO));
    ensure("cancelled pick cannot commit",!ALMediaFile::commitFileBrowse(status,"",true,false,commit));
    ensure("cancelled pick preserves face",slot.mediaData() && slot.mediaData()->mObjectId == face.mObjectId);
    ensure("only successful current preflight commits",ALMediaFile::commitFileBrowse(status,"",true,true,commit));
    ensure("successful Browse converts to URL",slot.mediaUrlData() && !slot.mediaData());
    ensure_equals("successful cue path",slot.mediaUrlData()->mValue,std::string("C:/valid.mov"));
    ensure("successful conversion preserves aspect",slot.mediaAspect() == ESourceAspect::CINEMA_239);
}

template<> template<> void prism_projector_object::test<34>()
{
    set_test_name("frame tracking belongs only to consumed impls with both enables");
    for (const bool vlc : {false, true})
    {
        LLMediaFrame::TrackingState plain, off;
        // configure(false) must be a no-op: enableMediaFrames returns before
        // changing layout epoch, tokens, geometry or sending a plugin message.
        ensure("ON without a slot leaves bookkeeping byte-identical to OFF",
            !plain.configure(LLMediaFrame::trackingEnabled(true, true, false), vlc));
        ensure("OFF is also a no-op",
            !off.configure(LLMediaFrame::trackingEnabled(false, false, false), vlc));
        for (const auto* incoming : {"", "late", "current"})
            ensure("ordinary update acceptance identical", plain.acceptsUpdate("", incoming) == off.acceptsUpdate("", incoming));
        ensure("ordinary frames never enter tracked upload path", !plain.active && !plain.enabled && !plain.tokenLoads);
        ensure("binding enables bookkeeping", plain.configure(LLMediaFrame::trackingEnabled(true, true, true), vlc));
        ensure("bound impl tracks", plain.enabled && plain.tokenLoads == vlc);
        ensure("unbind restores legacy", plain.configure(LLMediaFrame::trackingEnabled(true, true, false), vlc));
        ensure("unbound impl accepts ordinary frames", plain.acceptsUpdate("", "") && !plain.active && !plain.tokenLoads);
        ensure("master-off disables bound impl", !LLMediaFrame::trackingEnabled(false, true, true));
        ensure("feature-off disables bound impl", !LLMediaFrame::trackingEnabled(true, false, true));
    }
}

template<> template<> void prism_projector_object::test<35>()
{
    set_test_name("source toggles retain authored URL and picked face without starting a player");
    using namespace LLPrismLens;
    SourceSlotData slot;
    MediaUrlSource video;
    video.mValue = "D:\\Footage\\clip.mov";
    video.mLocation = MediaUrlSource::Location::LOCAL_FILE;
    video.mLoop = false; video.mVolume = .35f;
    slot.setMediaUrlData(video);
    slot.switchMediaKind(false);
    MediaFaceSource face;
    face.mObjectId.generate(); face.mTE = 7; face.mAspect = ESourceAspect::STANDARD_4_3;
    slot.setMediaData(face);
    slot.switchMediaKind(true);
    ensure("typed backslashes survive", slot.mediaUrlData() && slot.mediaUrlData()->mValue == video.mValue);
    ensure("URL options survive", !slot.mediaUrlData()->mLoop && slot.mediaUrlData()->mVolume == .35f);
    SourceSlotData copy;
    copy.copySourceData(slot);
    copy.switchMediaKind(false);
    ensure("snapshot retains picked face", copy.mediaData() && copy.mediaData()->mObjectId == face.mObjectId && copy.mediaData()->mTE == 7);
    ensure("shared aspect survives", copy.mediaAspect() == ESourceAspect::STANDARD_4_3);
    ALMediaFile::Descriptor internal; std::string reason;
    ensure("typed path classifies internally", ALMediaFile::classify(video.mValue, internal, reason, true));
    ensure_equals("internal file URI normalized", ALMediaFile::fileUri(internal.value), std::string("file:///D:/Footage/clip.mov"));
    ensure_equals("classification never rewrites authored path", slot.mediaUrlData()->mValue, video.mValue);
}

template<> template<> void prism_projector_object::test<36>()
{
    set_test_name("ordinary CEF and VLC updated messages match OFF bytes, binding tracks, unbinding restores");
    const auto state_bytes = [](LLPluginClassMedia& plugin)
    {
        LLRect dirty;
        std::ostringstream bytes;
        bytes << plugin.getDirty(&dirty) << ':' << dirty.mLeft << ':' << dirty.mTop << ':'
            << dirty.mRight << ':' << dirty.mBottom << ':' << plugin.getCurrentTime() << ':'
            << plugin.getDuration() << ':' << plugin.getLayoutEpoch() << ':' << plugin.getLoadToken()
            << ':' << plugin.getDirtyLoadToken() << ':' << plugin.tracksMediaFrames() << ':'
            << plugin.getVideoWidth() << ':' << plugin.getVideoHeight() << ':'
            << plugin.getVideoSarNum() << ':' << plugin.getVideoSarDen();
        return bytes.str();
    };
    for (const bool vlc : {false, true})
    {
        LLPluginClassMedia off(nullptr), plain(nullptr);
        off.enableMediaFrames(LLMediaFrame::trackingEnabled(false, false, false), vlc);
        plain.enableMediaFrames(LLMediaFrame::trackingEnabled(true, true, false), vlc);
        ensure_equals("no consumer changes no bookkeeping bytes", state_bytes(plain), state_bytes(off));
        LLPluginMessage update(LLPLUGIN_MESSAGE_CLASS_MEDIA, "updated");
        update.setValueS32("left", 0); update.setValueS32("right", 8);
        update.setValueS32("bottom", 0); update.setValueS32("top", 4);
        update.setValueReal("current_time", 1.25); update.setValueReal("duration", 10.);
        off.receivePluginMessage(update); plain.receivePluginMessage(update);
        ensure_equals("real ordinary update handler byte-identical to OFF", state_bytes(plain), state_bytes(off));
        // An ordinary upload may already be in flight at loadURI/destroy.
        // Exercise the same epoch gate used by both production entry points.
        LLMediaFrame::UploadState offUpload, plainUpload;
        U64 offEpoch = 7, plainEpoch = 7;
        const LLMediaFrame::Identity job{7, 0, ""};
        offUpload.pending = plainUpload.pending = true;
        for (int change = 0; change < 2; ++change)
        {
            ensure("OFF source change leaves epoch alone",
                !offUpload.sourceChanged(LLMediaFrame::trackingEnabled(false, false, false), offEpoch));
            ensure("ON/no-consumer source change leaves epoch alone",
                !plainUpload.sourceChanged(LLMediaFrame::trackingEnabled(true, true, false), plainEpoch));
            ensure_equals("ordinary source epoch byte-identical to OFF", plainEpoch, offEpoch);
            ensure_equals("ordinary epoch does not advance", plainEpoch, U64(7));
        }
        ensure("OFF in-flight upload completes", offUpload.finish(job, {offEpoch, 0, ""}, true));
        ensure("ON/no-consumer in-flight upload completes", plainUpload.finish(job, {plainEpoch, 0, ""}, true));
        ensure_equals("ordinary upload completion serial matches OFF", plainUpload.serial, offUpload.serial);
        ensure("ordinary upload remains ready", plainUpload.ready && offUpload.ready);
        struct TextureHandoff
        {
            U32 threaded = 0, direct = 0;
            void syncToMainThread(U32 name) { threaded = name; }
            void syncTexName(U32 name) { direct = name; }
        } offTexture, plainTexture;
        for (const bool worker : {false, true})
        {
            // The production ordinary upload uses this handoff even when a
            // source-change event occurs between enqueue and completion.
            ensure("ordinary ON plugin remains on legacy upload path", !plain.tracksMediaFrames());
            ensure("ordinary OFF plugin remains on legacy upload path", !off.tracksMediaFrames());
            LLMediaFrame::syncLegacyUpload(offTexture, 42, worker);
            LLMediaFrame::syncLegacyUpload(plainTexture, 42, worker);
            ensure_equals("ordinary direct handoff equals OFF", plainTexture.direct, offTexture.direct);
            ensure_equals("ordinary worker handoff equals OFF", plainTexture.threaded, offTexture.threaded);
            ensure_equals("ordinary upload is synced", worker ? plainTexture.threaded : plainTexture.direct, U32(42));
        }
        ensure("consumed source still invalidates old uploads", plainUpload.sourceChanged(true, plainEpoch));
        ensure("obsolete consumed upload is rejected", !plainUpload.finish(job, {plainEpoch, 0, ""}, true));
        plain.enableMediaFrames(LLMediaFrame::trackingEnabled(true, true, true), vlc);
        if (vlc) update.setValue("load_token", plain.getLoadToken());
        plain.receivePluginMessage(update);
        ensure("slot-bound plugin enters tracked frame path", plain.tracksMediaFrames());
        plain.enableMediaFrames(LLMediaFrame::trackingEnabled(true, true, false), vlc);
        ensure("unbind returns to ordinary frame path", !plain.tracksMediaFrames());
        update.setValue("load_token", "");
        update.setValueReal("current_time", 2.5);
        plain.resetDirty();
        plain.receivePluginMessage(update);
        ensure("unbound ordinary dirty accepted", plain.getDirty());
        ensure_equals("unbound ordinary time update accepted", plain.getCurrentTime(), 2.5);
    }
}

template<> template<> void prism_projector_object::test<37>()
{
    set_test_name("obsolete private source never matches after a valid replacement or clear");
    using namespace LLPrismLens;
    const MediaUrlSource oldSource{MediaUrlSource::Location::HTTP_URL, "https://example.invalid/A.mp4"};
    for (const std::string replacement : {"https://example.invalid/B.mp4", ""})
    {
        MediaUrlSource source = oldSource;
        source.mValue = replacement;
        const bool matches = ALMediaFile::matchingPrivateOwner(true, true, 7, 7,
            source.mLocation == oldSource.mLocation && source.mValue == oldSource.mValue);
        ensure("obsolete source cannot match even in the same slot generation", !matches);
        ensure_equals("obsolete source has no player/bootstrap demand",
            ALMediaFile::privateOwnerDemand(matches, false, true, true), 0.);
        std::array<std::shared_ptr<int>, 1> owners{std::make_shared<int>(7)};
        ALMediaFile::sweepPrivateOwners(owners, [&](unsigned) { return matches; },
            [&](unsigned slot) { owners[slot].reset(); });
        ensure("late completion has no current owner to create a player", !owners[0]);
    }
    ensure("unchanged valid source still matches",
        ALMediaFile::matchingPrivateOwner(true, true, 7, 7, true));
}

template<> template<> void prism_projector_object::test<38>()
{
    set_test_name("explicit loop edit hold is scoped to admitted load; rebuilt pixels stay gated");
    LLPluginClassMedia plugin(nullptr);
    plugin.enableMediaFrames(true, true);
    plugin.configurePrivateFile("mp4", 2, true);
    plugin.loadURI("file:///D:/clip.mp4");
    LLPluginMessage verdict(LLPLUGIN_MESSAGE_CLASS_MEDIA, "private_file_admission");
    verdict.setValue("load_token", plugin.getLoadToken());
    verdict.setValueBoolean("opened", true);
    verdict.setValue("player_token", "1");
    verdict.setValueS32("verdict", 1);
    plugin.receivePluginMessage(verdict);
    const auto epoch = plugin.getLayoutEpoch();
    const auto token = plugin.getLoadToken();
    plugin.receivePluginMessage(verdict); // actual repeated admitted protocol message
    ensure_equals("repeated admission preserves viewer epoch", plugin.getLayoutEpoch(), epoch);
    ensure_equals("repeated admission preserves load token", plugin.getLoadToken(), token);
    ensure_equals("repeated admission remains admitted", plugin.privateFileVerdict(), 1);
    verdict.setValue("player_token", "2");
    verdict.setValueS32("verdict", 0);
    verdict.setValueBoolean("loop_reverify", true);
    plugin.receivePluginMessage(verdict);
    ensure("explicit edit holds ownership/pixels", plugin.privateFileRetaining());
    ensure_equals("replacement still pending", plugin.privateFileVerdict(), 0);
    LLPluginMessage update(LLPLUGIN_MESSAGE_CLASS_MEDIA, "updated");
    update.setValue("load_token", token);
    update.setValueS32("left", 0); update.setValueS32("right", 8);
    update.setValueS32("bottom", 0); update.setValueS32("top", 8);
    plugin.receivePluginMessage(update);
    ensure("pending pixels never published", !plugin.getDirty());
    ensure("rebuild invalidates old upload provenance", plugin.getLayoutEpoch() > epoch);
    verdict.setValueS32("verdict", 1);
    plugin.receivePluginMessage(verdict);
    plugin.receivePluginMessage(update);
    ensure("admitted replacement pixels publish", plugin.getDirty());
    plugin.loadURI("file:///D:/new.mp4");
    ensure("new URI resets hold", !plugin.privateFileRetaining() && plugin.privateFileVerdict() == 0);
    verdict.setValue("load_token", plugin.getLoadToken());
    verdict.setValueS32("verdict", 0);
    plugin.receivePluginMessage(verdict);
    ensure("unadmitted load cannot claim edit hold", !plugin.privateFileRetaining());
}

template<> template<> void prism_projector_object::test<40>()
{
    set_test_name("viewer seam aggregation bounds logging and measures excess over each source frame");
    using Kind = LLPluginLoopSeam::Kind;
    LLPluginLoopSeam seams;
    for (int i = 0; i < 3; ++i)
        ensure("first three individually", seams.sample(i, 40., 30.).kind == Kind::INDIVIDUAL);
    ensure("subsequent seams aggregate", seams.sample(3., 40., 30.).kind == Kind::NONE);
    seams.sample(4., 20., 30.);
    ensure("summary not before ten seconds", seams.poll(11.999).kind == Kind::NONE);
    const auto summary = seams.poll(12.);
    ensure("idle flush needs no next seam", summary.kind == Kind::SUMMARY);
    ensure_equals("window count", summary.count, uint64_t(2));
    ensure_equals("minimum gap", summary.min, 20.);
    ensure_equals("maximum gap", summary.max, 40.);
    ensure_equals("mean gap", summary.mean, 30.);
    ensure_equals("mean excess clamps sub-frame gaps to zero", summary.excess, 5.);
    ensure("empty window never logs", seams.poll(30.).kind == Kind::NONE);
    ensure("summary resumes after quiet period", seams.sample(31., 50., 20.).kind == Kind::SUMMARY);
    ensure("next window throttled", seams.sample(31.1, 60., 20.).kind == Kind::NONE);
    ensure("no second summary in ten seconds", seams.poll(40.999).kind == Kind::NONE);
    ensure_equals("next window independent count", seams.poll(41.).count, uint64_t(1));
    ensure("invalid gap ignored", seams.sample(42., std::numeric_limits<double>::quiet_NaN(), 20.).kind == Kind::NONE);
    ensure("invalid frame ignored", seams.sample(42., 30., -1.).kind == Kind::NONE);
}

template<> template<> void prism_projector_object::test<41>()
{
    set_test_name("loop_seam protocol reaches viewer INFO log; stale loads ignored and first-three budget resets per load");
    struct LogCapture
    {
        LLError::SettingsStoragePtr settings = LLError::saveAndResetSettings();
        std::vector<std::string> messages;
        LLError::RecorderPtr recorder;
        LogCapture()
        {
            LLError::setDefaultLevel(LLError::LEVEL_INFO);
            recorder = LLError::addGenericRecorder([this](LLError::ELevel level, const std::string& text)
            {
                if (level == LLError::LEVEL_INFO && text.find("loop seam gap") != std::string::npos) messages.push_back(text);
            });
        }
        ~LogCapture() { LLError::removeRecorder(recorder); LLError::restoreSettings(settings); }
    } log;
    LLPluginClassMedia plugin(nullptr);
    plugin.enableMediaFrames(true, true);
    plugin.configurePrivateFile("mp4", 2, true);
    plugin.loadURI("file:///D:/clip.mp4");
    LLPluginMessage seam(LLPLUGIN_MESSAGE_CLASS_MEDIA, "loop_seam");
    seam.setValueReal("gap_ms", 45.); seam.setValueReal("frame_ms", 40.);
    seam.setValue("load_token", "stale");
    plugin.receivePluginMessage(seam);
    ensure_equals("stale load ignored", log.messages.size(), size_t(0));
    const auto token = plugin.getLoadToken();
    seam.setValue("load_token", token);
    for (int i = 0; i < 4; ++i) plugin.receivePluginMessage(seam);
    ensure_equals("exactly three viewer log lines", log.messages.size(), size_t(3));
    ensure("viewer line contains source frame duration", log.messages[0].find("frame_ms=40") != std::string::npos);
    ensure("viewer line contains excess", log.messages[0].find("excess_ms=5") != std::string::npos);
    plugin.loadURI("file:///D:/replacement.mp4");
    plugin.receivePluginMessage(seam);
    ensure_equals("late old load never consumes new budget", log.messages.size(), size_t(3));
    seam.setValue("load_token", plugin.getLoadToken());
    plugin.receivePluginMessage(seam);
    ensure_equals("new load starts first-three budget", log.messages.size(), size_t(4));
}

template<> template<> void prism_projector_object::test<39>()
{
    set_test_name("flare energy EMA has frame-rate-independent authored release / 100ms attack and soft knee");
    using namespace ALFlareEnergy;
    ensure("100ms attack", std::abs(smooth(0.f, 1.f, .100f) - (1.f - std::exp(-1.f))) < .00001f);
    ensure("authored 100ms release", std::abs(smooth(1.f, 0.f, .100f, .100f, .100f) - std::exp(-1.f)) < .00001f);
    for (int fps : {24, 30, 60, 144})
    {
        float value = 0.f;
        for (int i = 0; i < fps; ++i) value = smooth(value, 2.f, 1.f / fps);
        ensure("attack independent of frame rate", std::abs(value - (2.f * (1.f - std::exp(-1.f / .100f)))) < .00001f);
        for (int i = 0; i < fps; ++i) value = smooth(value, 0.f, 1.f / fps);
        ensure("release independent of frame rate", std::abs(value - ((2.f * (1.f - std::exp(-1.f / .100f))) * std::exp(-1.f / .450f))) < .00001f);
    }
    ensure_equals("release zero is immediate", smooth(1.f, 0.f, .016f, .100f, 0.f), 0.f);
    ensure("Release slider changes CPU/GPU shared alpha", alpha(.016f, .2f) > alpha(.016f, .8f));
    const float nan = std::numeric_limits<float>::quiet_NaN();
    ensure_equals("NaN dt holds finite state", smooth(.5f, 1.f, nan), .5f);
    ensure_equals("NaN history recovers to current value", smooth(nan, .7f, .016f), .7f);
    ensure_equals("NaN target resets safely", smooth(nan, nan, nan), 0.f);
    ensure("subsequent finite dt resumes response", smooth(smooth(.5f, 1.f, nan), 1.f, .016f) > .5f);
    ensure_equals("zero dt holds", smooth(.5f, 1.f, 0.f), .5f);
    ensure_equals("steady state unchanged", smooth(2.f, 2.f, .1f), 2.f);
    ensure_equals("below floor stays dark", energy(.24f, .25f), 0.f);
    ensure("soft knee continuous", energy(.251f, .25f) > 0.f && energy(.251f, .25f) < .001f);
    ensure_equals("bright steady look preserved", energy(.75f, .25f), 2.f);
}

}
