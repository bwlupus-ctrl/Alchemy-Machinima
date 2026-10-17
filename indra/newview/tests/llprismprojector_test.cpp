/**
 * @file llprismprojector_test.cpp
 * @brief Pure VP1/VP2 projector contract regressions.
 */

#include "linden_common.h"

#include "../test/lltut.h"
#include "../llprismlens.h"
#include "../alvcamautovol.h"
#include "../../llplugin/llmediaframestate.h"

#include <limits>
#include <map>

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

}
