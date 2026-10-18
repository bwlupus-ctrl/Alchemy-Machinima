/**
 * @file llprismgate_test.cpp
 * @brief Regression tests for the pure Vcam Gate router and schema.
 */

#include "linden_common.h"
#include "llplugingateclock.h"

#include "../test/lltut.h"
#include "../aldirectorswitchermodel.h"
#include "../llprismgate.h"
#include "../llprismpreview.h"

#include <cmath>
#include <limits>
#include <algorithm>

namespace tut
{
namespace
{
LLPrismLens::GateArmedCamera armedCamera(const char* arm_id,
                                         const char* capture_id,
                                         const char* label,
                                         bool enabled = true)
{
    LLPrismLens::GateArmedCamera armed;
    armed.mArmId.set(arm_id);
    armed.mCaptureId.set(capture_id);
    armed.mLabel = label;
    armed.mEnabled = enabled;
    return armed;
}

ALDirectorSwitcherModel::Config gateConfig(
    const std::vector<LLPrismLens::GateArmedCamera>& armed,
    F64 interval = 5.0)
{
    ALDirectorSwitcherModel::Config config;
    config.mAuto = true;
    config.mSequence = true;
    config.mIntervalSeconds = interval;
    config.mJitterSeconds = 0.0;
    for (std::size_t index = 0;
         index < armed.size() && index < config.mEnabled.size(); ++index)
    {
        config.mEnabled[index] = armed[index].mEnabled;
    }
    return config;
}

bool approxNear(F64 left, F64 right, F64 tolerance = 1e-6)
{
    return std::fabs(left - right) <= tolerance;
}
} // anonymous namespace

struct prism_gate_data {};
typedef test_group<prism_gate_data, 64> prism_gate_test_group;
typedef prism_gate_test_group::object prism_gate_test_object;
prism_gate_test_group prism_gate_tests("LLPrismGate");

template<> template<>
void prism_gate_test_object::test<1>()
{
    set_test_name("router cycle, live resolution, routing, deletion, and dark sentinel");
    using namespace LLPrismLens;
    std::vector<GateArmedCamera> armed = {
        armedCamera("10000000-0000-0000-0000-000000000001",
                    "11000000-0000-0000-0000-000000000001", "A"),
        armedCamera("10000000-0000-0000-0000-000000000002",
                    "11000000-0000-0000-0000-000000000002", "B"),
        armedCamera("10000000-0000-0000-0000-000000000003",
                    "11000000-0000-0000-0000-000000000003", "C")
    };
    std::vector<LLUUID> live = {
        armed[0].mCaptureId, armed[1].mCaptureId, armed[2].mCaptureId
    };
    const std::size_t capture_count_before_arm = live.size();
    GateSettings gate;
    gate.mArmed = armed;
    ensure_equals("arming stores three references", gate.mArmed.size(),
                  static_cast<std::size_t>(3));
    ensure_equals("arming allocates no capture", live.size(),
                  capture_count_before_arm);

    ALDirectorSwitcherModel::Controller controller;
    const ALDirectorSwitcherModel::Config config = gateConfig(armed);
    ensure("initial program punch accepted", controller.manualPunch(0, 0.0));
    ensure("installing config does not cut", !controller.update(0.0, config).mCut);
    ensure("before boundary remains A", !controller.update(4.999, config).mCut &&
           controller.activeSlot() == 0);
    const ALDirectorSwitcherModel::Frame to_b = controller.update(5.001, config);
    ensure("first elapsed event cuts to B", to_b.mCut && to_b.mSlot == 1);
    const ALDirectorSwitcherModel::Frame to_c = controller.update(10.001, config);
    ensure("second event cuts to C", to_c.mCut && to_c.mSlot == 2);
    const ALDirectorSwitcherModel::Frame wrap = controller.update(15.001, config);
    ensure("third event wraps C to A", wrap.mCut && wrap.mSlot == 0);

    ensure_equals("on-air arm resolves against live capture IDs",
                  gateResolveArmIndex(armed, 1, live), 1);
    U32 display_slot = MAX_CAPTURES;
    U64 display_generation = 0;
    gateRouteDisplayBinding(4, 27, display_slot, display_generation);
    ensure("monitor routing writes the live slot and generation",
           display_slot == 4 && display_generation == 27);

    live.erase(live.begin());
    ensure_equals("deleted on-air capture advances to next resolvable arm",
                  gateResolveArmIndex(armed, 0, live), 1);
    live.clear();
    ensure_equals("all deleted arms resolve dark",
                  gateResolveArmIndex(armed, 0, live), -1);
    gateRouteDisplayBinding(-1, 99, display_slot, display_generation);
    ensure("dark routing never writes a freed slot",
           display_slot == MAX_CAPTURES && display_generation == 0);

    std::vector<GateArmedCamera> empty;
    ensure_equals("zero armed cameras resolve dark",
                  gateResolveArmIndex(empty, 0, live), -1);

    ALDirectorSwitcherModel::Controller hitch_controller;
    hitch_controller.manualPunch(0, 0.0);
    hitch_controller.update(0.0, config);
    const ALDirectorSwitcherModel::Frame hitch =
        hitch_controller.update(500.001, config);
    ensure("large jump emits one deterministic final cut",
           hitch.mCut && hitch.mEventsElapsed == 100 && hitch.mSlot == 1);
}

template<> template<>
void prism_gate_test_object::test<2>()
{
    set_test_name("past boundary, next cut, transient watch target, seed, and expiry");
    using namespace LLPrismLens;
    std::vector<GateArmedCamera> armed = {
        armedCamera("20000000-0000-0000-0000-000000000001",
                    "21000000-0000-0000-0000-000000000001", "A"),
        armedCamera("20000000-0000-0000-0000-000000000002",
                    "21000000-0000-0000-0000-000000000002", "B"),
        armedCamera("20000000-0000-0000-0000-000000000003",
                    "21000000-0000-0000-0000-000000000003", "C")
    };
    const F64 interval = 5.0;
    const F64 past_boundary = 5.0;
    const F64 next_cut = gateNextCutTime(past_boundary, interval);
    ensure("next cut adds one interval to the just-fired boundary",
           approxNear(next_cut, 10.0));
    const F64 dt = 1.0 / 60.0;
    ensure("warm stays closed before corrected threshold",
           !gateWarmActive(9.949, next_cut, 3, dt));
    ensure("warm opens inclusively at corrected threshold",
           gateWarmActive(9.95, next_cut, 3, dt));
    ensure("past boundary is never treated as the future cut",
           !gateWarmActive(4.96, next_cut, 3, dt));

    ALDirectorSwitcherModel::Controller controller;
    const ALDirectorSwitcherModel::Config config = gateConfig(armed, interval);
    controller.manualPunch(0, 0.0);
    controller.update(0.0, config);
    const S32 predicted = gateNextEnabledIndex(armed, 0);
    const ALDirectorSwitcherModel::Frame actual = controller.update(5.0, config);
    ensure("watch target agrees with the Controller's next sequence cut",
           actual.mCut && actual.mSlot == predicted && predicted == 1);
    ensure("nonzero on-air dimensions admit a transient watch",
           gateWarmSeedValid(960, 540));
    ensure("zero width prevents watch admission", !gateWarmSeedValid(0, 540));
    ensure("zero height prevents watch admission", !gateWarmSeedValid(960, 0));

    U32 gate_watch_frames = 3;
    ensure("watch frame one is consumed",
           gateConsumePreviewWarmFrame(gate_watch_frames));
    ensure("watch frame two is consumed",
           gateConsumePreviewWarmFrame(gate_watch_frames));
    ensure("watch frame three is consumed",
           gateConsumePreviewWarmFrame(gate_watch_frames));
    ensure_equals("watch self-expires after exactly N frames",
                  gate_watch_frames, 0u);
    ensure("expired watch cannot become permanent",
           !gateConsumePreviewWarmFrame(gate_watch_frames));
}

template<> template<>
void prism_gate_test_object::test<3>()
{
    set_test_name("manual take, interval restart, mode flip, and bounded preview watch");
    using namespace LLPrismLens;
    ensure("no preview cannot cut", !gateIsRealCut(0, -1));
    ensure("preview equal to program cannot cut", !gateIsRealCut(0, 0));
    ensure("distinct preview is a real cut", gateIsRealCut(0, 2));

    std::vector<GateArmedCamera> armed = {
        armedCamera("30000000-0000-0000-0000-000000000001",
                    "31000000-0000-0000-0000-000000000001", "A"),
        armedCamera("30000000-0000-0000-0000-000000000002",
                    "31000000-0000-0000-0000-000000000002", "B"),
        armedCamera("30000000-0000-0000-0000-000000000003",
                    "31000000-0000-0000-0000-000000000003", "C")
    };
    ALDirectorSwitcherModel::Controller controller;
    ALDirectorSwitcherModel::Config config = gateConfig(armed);
    controller.manualPunch(0, 0.0);
    controller.update(0.0, config);
    ensure("manual TAKE punch accepted", controller.manualPunch(2, 2.0));
    ensure_equals("preview becomes program", controller.activeSlot(), 2);
    ensure("old interval boundary was cancelled",
           !controller.update(5.0, config).mCut);
    const ALDirectorSwitcherModel::Frame restarted = controller.update(7.0, config);
    ensure("interval restarts from TAKE and resumes sequence",
           restarted.mCut && restarted.mSlot == 0);

    ALDirectorSwitcherModel::Controller mode_controller;
    mode_controller.manualPunch(1, 0.0);
    mode_controller.update(0.0, config);
    config.mAuto = false;
    ensure("Auto to Manual freezes without a spurious cut",
           !mode_controller.update(20.0, config).mCut &&
           mode_controller.activeSlot() == 1);
}

template<> template<>
void prism_gate_test_object::test<4>()
{
    set_test_name("reference persistence, best-effort dangling arms, and gate_source exemption");
    using namespace LLPrismLens;
    GateSettings input;
    input.mGateId.set("40000000-0000-0000-0000-000000000001");
    input.mActive = true;
    input.mMode = EGateMode::AUTO_CYCLE;
    input.mIntervalSeconds = 11.5;
    input.mPrewarmFrames = 4;
    input.mProgramArmIndex = 0;
    input.mPreviewArmIndex = 1;
    input.mArmed.push_back(armedCamera(
        "40000000-0000-0000-0000-000000000002",
        "41000000-0000-0000-0000-000000000001", "Wide"));
    input.mArmed.push_back(armedCamera(
        "40000000-0000-0000-0000-000000000003",
        "41000000-0000-0000-0000-000000000002", "Close"));

    const LLSD encoded = gateSettingsToLLSD(input);
    ensure("router schema has no program capture",
           !encoded.has("program_capture_id"));
    ensure("armed rows persist a capture reference, not a camera snapshot",
           encoded["armed"][0].has("capture_id") &&
           !encoded["armed"][0].has("camera"));
    GateSettings output;
    std::string reason;
    ensure("serialized Gate parses", gateSettingsFromLLSD(encoded, output, &reason));
    ensure("outer Gate fields round trip",
           output.mGateId == input.mGateId && output.mActive == input.mActive &&
           output.mMode == input.mMode && approxNear(output.mIntervalSeconds, 11.5) &&
           output.mPrewarmFrames == 4 && output.mProgramArmIndex == 0 &&
           output.mPreviewArmIndex == 1);
    ensure("armed order and capture IDs round trip",
           output.mArmed.size() == 2 &&
           output.mArmed[0].mCaptureId == input.mArmed[0].mCaptureId &&
           output.mArmed[1].mCaptureId == input.mArmed[1].mCaptureId);

    std::vector<LLUUID> only_wide = { input.mArmed[0].mCaptureId };
    gateFilterResolvableArms(output, only_wide);
    ensure("dangling arm is dropped without failing the Gate",
           output.mArmed.size() == 1 && output.mArmed[0].mLabel == "Wide");
    ensure_equals("preview of a dropped arm is cleared",
                  output.mPreviewArmIndex, -1);

    LLSD gate_source_display = LLSD::emptyMap();
    gate_source_display["gate_source"] = true;
    ensure("gate_source with no capture_id is accepted",
           !gate_source_display.has("capture_id") &&
           gateDisplayCaptureReferenceAccepted(
               gate_source_display["gate_source"].asBoolean(), false));
    gate_source_display["capture_id"] =
        LLUUID("41000000-0000-0000-0000-000000000099");
    ensure("gate_source ignores a dangling compatibility hint",
           gateDisplayCaptureReferenceAccepted(true, false));
    ensure("non-gate display still rejects an absent or dangling capture",
           !gateDisplayCaptureReferenceAccepted(false, false));
    ensure("non-gate display still accepts a resolved capture",
           gateDisplayCaptureReferenceAccepted(false, true));

    LLSD old_scene = LLSD::emptyMap();
    old_scene["prism_captures"] = LLSD::emptyArray();
    old_scene["prism_displays"] = LLSD::emptyArray();
    GateSettings absent_gate;
    bool gate_present = true;
    ensure("old scene is accepted by the Gate extractor",
           gateSettingsFromScene(old_scene, absent_gate, gate_present, &reason));
    ensure("old scene produces an empty inactive Gate",
           !gate_present && !absent_gate.mActive && absent_gate.mArmed.empty());
}

template<> template<>
void prism_gate_test_object::test<5>()
{
    set_test_name("feedback guard remains scoped to gate-subscribed monitors");
    using namespace LLPrismLens;
    const LLUUID display_object("50000000-0000-0000-0000-000000000001");
    ensure("subscribed display cannot be its armed camera source",
           gateFeedbackResult(display_object, display_object, true) ==
               ERegistryResult::INVALID_CONFIGURATION);
    ensure("fixed display does not create Gate feedback",
           gateFeedbackResult(display_object, display_object, false) ==
               ERegistryResult::OK);
    ensure("different source and display are accepted",
           gateFeedbackResult(
               LLUUID("50000000-0000-0000-0000-000000000002"),
               display_object, true) == ERegistryResult::OK);
}

namespace
{
struct CycleFixture
{
    LLPrismLens::GateReducer gate;
    LLPrismLens::GateInput input;
    bool pauseAck = true, accept = true;
    CycleFixture()
    {
        using namespace LLPrismLens;
        input.now = 1.; input.settings.mGateId.generate(); input.settings.mActive = true;
        input.settings.mMode = EGateMode::AUTO_CYCLE; input.settings.mIntervalSeconds = 1.;
        input.destinations.add(512, 512, true);
        for (U32 i = 0; i < 3; ++i)
        {
            GateSourceEvent source; source.handle.mId.generate(); source.handle.mGeneration = i + 1;
            source.kind = ESourceSlotKind::CAMERA; source.available = source.permitted = source.ready = true;
            source.error = .03; source.sampleAge = source.frameAge = 0.; source.frame = 1;
            source.seekable = true; source.duration = 20.;
            GateArmedCamera arm; arm.mArmId.generate(); arm.mCaptureId = source.handle.mId; arm.mLabel = "Feed";
            input.sources.push_back(source); input.settings.mArmed.push_back(arm);
        }
    }
    std::vector<LLPrismLens::GateAction> tick(F64 dt = .01)
    {
        using namespace LLPrismLens;
        input.dt = dt; input.now += dt;
        for (auto& source : input.sources) if (source.resumed && !source.eof) source.position += dt;
        auto actions = gate.update(input);
        input.activate = input.playVideos = input.take = input.changed = false;
        for (auto& source : input.sources) ++source.frame;
        for (const auto& action : actions)
        {
            if (action.kind == EGateAction::PROPOSE && accept) input.accepted = action.token;
            for (auto& source : input.sources) if (source.handle.mId == action.handle.mId)
            {
                if (action.kind == EGateAction::WARM) source.admitted = true;
                if (action.kind == EGateAction::SEEK || action.kind == EGateAction::RESUME)
                { source.token = action.token; source.position = action.position;
                  source.paused = action.kind == EGateAction::SEEK; source.resumed = !source.paused; }
                if (action.kind == EGateAction::PARK || (action.kind == EGateAction::RETIRE && pauseAck))
                { source.token = action.token; source.paused = true; source.resumed = false; }
            }
        }
        return actions;
    }
    void settle(U32 n = 12) { while (n--) tick(); }
};
}

template<> template<> void prism_gate_test_object::test<6>()
{
    using namespace LLPrismLens;
    set_test_name("additive Gate defaults, strict enum/types and finite fade range");
    GateSettings settings; settings.mGateId.generate();
    const LLSD saved = gateSettingsToLLSD(settings);
    GateSettings restored; ensure(gateSettingsFromLLSD(saved, restored));
    ensure(restored.mTransition == EGateTransition::CUT && restored.mFadeOverBudget == EGateFadeOverBudget::CUT_INSTEAD);
    ensure(restored.mVideoDwell == EGateVideoDwell::INTERVAL && restored.mCrossfadeSeconds == .5);
    for (const auto* key : {"transition", "fade_over_budget", "video_dwell"})
    { LLSD bad = saved; bad[key] = "unknown"; ensure(!gateSettingsFromLLSD(bad, restored)); }
    for (F64 length : {0., .09, 3.1, std::numeric_limits<F64>::infinity(), std::numeric_limits<F64>::quiet_NaN()})
    { LLSD bad = saved; bad["crossfade_seconds"] = length; ensure(!gateSettingsFromLLSD(bad, restored)); }
    LLSD old = saved; for (const auto* key : {"transition", "crossfade_seconds", "fade_over_budget", "video_dwell"}) old.erase(key);
    ensure(gateSettingsFromLLSD(old, restored)); ensure(restored.mCrossfadeSeconds == .5);
    ensure(MediaUrlSource().mOnCut == EMediaOnCut::LIVE);
}

template<> template<> void prism_gate_test_object::test<7>()
{
    using namespace LLPrismLens;
    set_test_name("Live commit-time math, circular tolerance, unknown length and Loop off");
    ensure(approxNear(gatePosition(EMediaOnCut::LIVE, true, 3., 10., 35., 12., true, .04).seconds, 4.));
    const auto terminal = gatePosition(EMediaOnCut::LIVE, true, 3., 10., 35., 12., false, .04);
    ensure(terminal.terminal && approxNear(terminal.seconds, 11.96));
    const auto unknown = gatePosition(EMediaOnCut::LIVE, true, 3., 10., 35., 0., true, .04);
    ensure(unknown.restart && unknown.seconds == 0. && unknown.reason == "Live needs a known length - restarting");
    ensure(gatePosition(EMediaOnCut::RESUME, true, 3., 10., 35., 12., true, .04).seconds == 3.);
    ensure(gatePosition(EMediaOnCut::RESTART, true, 3., 10., 35., 12., true, .04).seconds == 0.);
    ensure(gatePosition(EMediaOnCut::LIVE, false, 3., 10., 35., 12., true, .04).seconds == 0.);
    ensure(approxNear(gatePositionDistance(.02, 9.98, 10., true), .04));
    ensure(gatePositionQualified(.1, .05, .1, .04, .04));
    ensure(!gatePositionQualified(.101, .05, .1, .04, .04));
    ensure(!gatePositionQualified(0., .15, .1, .04, .04));
    ensure(!gatePositionQualified(0., .05, .501, .04, .04));
    ensure(!gatePosition(EMediaOnCut::LIVE, true, NAN, 10., 20., 10., true, .04).valid);
}

template<> template<> void prism_gate_test_object::test<8>()
{
    using namespace LLPrismLens;
    set_test_name("cold activation prewarm zero, source readiness and publication acknowledgement");
    CycleFixture f; f.input.settings.mPrewarmFrames = 0; f.input.sources[0].ready = false;
    f.tick(); ensure(f.gate.phase() == EGatePhase::WARMING && f.gate.program().mId.isNull());
    f.tick(3.); ensure("initial bootstrap is not a boundary overrun", f.gate.program().mId.isNull() && f.gate.incoming().mId.notNull());
    f.input.sources[0].ready = true; f.accept = false; f.tick();
    ensure("proposal alone cannot publish", f.gate.program().mId.isNull());
    f.accept = true; f.input.accepted = f.gate.token(); f.tick();
    ensure(f.gate.program().mId == f.input.sources[0].handle.mId);
}

template<> template<> void prism_gate_test_object::test<9>()
{
    using namespace LLPrismLens;
    set_test_name("bounded enabled-order cycle skips refusal and never catches up");
    CycleFixture f; f.input.sources[1].refusal = "This file isn't a video";
    f.settle(); ensure(f.gate.program().mId == f.input.sources[0].handle.mId);
    f.settle(); f.tick(1.1); f.settle();
    ensure(f.gate.program().mId == f.input.sources[2].handle.mId);
    ensure(f.gate.status(f.input.sources[1].handle).find("Skipped: ") == 0);
    f.tick(100.); // one transition at most, no catch-up chain
    ensure(f.gate.program().mId == f.input.sources[2].handle.mId);
}

template<> template<> void prism_gate_test_object::test<10>()
{
    using namespace LLPrismLens;
    set_test_name("private grant scope includes disabled arms; restored active camera keeps Program");
    CycleFixture f; f.input.sources[1].kind = f.input.sources[2].kind = ESourceSlotKind::MEDIA_URL;
    f.input.settings.mArmed[2].mEnabled = false;
    f.gate.restoreProgram(f.input.sources[0].handle, f.input.now);
    f.tick(); ensure(!f.gate.granted(f.input.sources[1].handle));
    ensure(f.gate.status(f.input.sources[1].handle) == "Waiting for Play videos");
    f.input.playVideos = true; f.tick();
    ensure(f.gate.granted(f.input.sources[1].handle) && f.gate.granted(f.input.sources[2].handle));
    ensure(f.gate.program().mId == f.input.sources[0].handle.mId);
    f.input.settings.mActive = false; f.tick(); ensure(!f.gate.granted(f.input.sources[1].handle));
}

template<> template<> void prism_gate_test_object::test<11>()
{
    using namespace LLPrismLens;
    set_test_name("one charged warm role and delayed retirement barrier including reset");
    CycleFixture f; for (auto& s : f.input.sources) { s.kind = ESourceSlotKind::MEDIA_URL; s.onCut = EMediaOnCut::RESTART; }
    f.input.activate = true; f.pauseAck = false; f.settle();
    ensure(f.gate.program().mId == f.input.sources[0].handle.mId);
    f.settle(); ensure(f.gate.charges() <= 2);
    f.tick(1.); f.settle(); ensure(f.gate.charges() == 2);
    f.tick(2.1); f.settle(); ensure("no third role before retirement receipt", f.gate.charges() == 2);
    f.gate.reset(); f.tick(); ensure("reset retains outstanding charges", f.gate.charges() == 2);
    for (auto& s : f.input.sources) s.retired = true;
    f.tick(); ensure(f.gate.charges() == 0);
}

template<> template<> void prism_gate_test_object::test<12>()
{
    using namespace LLPrismLens;
    set_test_name("deterministic fade arithmetic includes pass count, pixels, variants and rate-independent caps");
    GateFadeDemand budget; for (U32 i = 0; i < 8; ++i) budget.add(1024, 1024, true);
    ensure(budget.fits()); budget.add(1, 1, false); ensure(!budget.fits());
    GateFadeDemand wide; wide.add(1920, 1080, false); ensure(wide.fits()); wide.add(1921, 1080, false); ensure(!wide.fits());
    GateFadeDemand pixels; for (U32 i = 0; i < 5; ++i) pixels.add(1920, 1080, false); ensure(!pixels.fits());
    GateFadeDemand variant; variant.add(512, 512, true, false); ensure(!variant.fits());
}

template<> template<> void prism_gate_test_object::test<13>()
{
    using namespace LLPrismLens;
    set_test_name("over-budget Cut instead, Fade anyway and bounded Hold");
    for (auto policy : {EGateFadeOverBudget::CUT_INSTEAD, EGateFadeOverBudget::FADE_ANYWAY, EGateFadeOverBudget::HOLD})
    {
        CycleFixture f; f.input.settings.mTransition = EGateTransition::CROSSFADE;
        f.input.settings.mFadeOverBudget = policy; f.input.destinations.passes = 9;
        f.settle(); f.tick(1.1); f.settle();
        if (policy == EGateFadeOverBudget::CUT_INSTEAD)
        { ensure(f.gate.program().mId == f.input.sources[1].handle.mId); ensure(f.gate.reason() == "Crossfade over budget - cut"); }
        else if (policy == EGateFadeOverBudget::FADE_ANYWAY)
        { ensure(f.gate.phase() == EGatePhase::CROSSFADING); f.tick(.6); f.tick(); ensure(f.gate.program().mId == f.input.sources[1].handle.mId); }
        else
        {
            ensure(f.gate.program().mId == f.input.sources[0].handle.mId);
            ensure(f.gate.reason() == "Crossfade over budget - holding (up to 2 s)");
            f.tick(2.01); ensure(f.gate.reason() == "Crossfade over budget - hold timed out");
            ensure(f.gate.program().mId == f.input.sources[0].handle.mId && !f.gate.pending());
        }
    }
}

template<> template<> void prism_gate_test_object::test<14>()
{
    using namespace LLPrismLens;
    set_test_name("Manual queued Take and Preview replacement keep Program until ready");
    CycleFixture f; f.input.settings.mMode = EGateMode::MANUAL; f.settle();
    f.input.settings.mPreviewArmIndex = 1; f.input.sources[1].ready = false;
    f.input.changed = f.input.take = true; f.tick();
    ensure(f.gate.pending() && f.gate.program().mId == f.input.sources[0].handle.mId);
    f.input.settings.mPreviewArmIndex = 2; f.input.changed = true; f.tick(); f.input.take = true; f.settle();
    ensure(f.gate.program().mId == f.input.sources[2].handle.mId);
}

template<> template<> void prism_gate_test_object::test<15>()
{
    using namespace LLPrismLens;
    set_test_name("borrowed face readiness never emits private transport or volume");
    CycleFixture f; for (auto& s : f.input.sources) s.kind = ESourceSlotKind::MEDIA_FACE;
    for (U32 i = 0; i < 160; ++i)
    {
        const auto actions = f.tick(.02);
        for (const auto& action : actions)
            ensure(action.kind == EGateAction::WARM || action.kind == EGateAction::PROPOSE || action.kind == EGateAction::ENVELOPE);
        ensure(f.gate.charges() == 0);
    }
}

template<> template<> void prism_gate_test_object::test<16>()
{
    using namespace LLPrismLens;
    set_test_name("single-arm Play-to-end holds; media kill switch retains camera-only routing");
    CycleFixture f; f.input.settings.mArmed.resize(1); f.input.sources.resize(1);
    f.input.sources[0].kind = ESourceSlotKind::MEDIA_URL; f.input.activate = true;
    f.input.settings.mVideoDwell = EGateVideoDwell::PLAY_TO_END;
    f.settle(); f.input.sources[0].eof = true; f.tick(21.); f.settle();
    ensure(f.gate.program().mId == f.input.sources[0].handle.mId);
    CycleFixture camera; camera.input.mediaEnabled = false; camera.settle(); camera.tick(1.1); camera.settle();
    ensure(camera.gate.program().mId == camera.input.sources[1].handle.mId);
}

template<> template<> void prism_gate_test_object::test<17>()
{
    using namespace LLPrismLens;
    set_test_name("predicted Play-to-end and wrap-window Cut win before matching EOF/native repeat");
    CycleFixture f; f.input.sources[0].kind = ESourceSlotKind::MEDIA_URL;
    f.input.activate = true; f.input.settings.mVideoDwell = EGateVideoDwell::PLAY_TO_END;
    f.settle(); f.input.sources[0].position = 19.99; f.tick();
    ensure("predicted end proposes without EOF", !f.input.sources[0].eof && f.input.accepted != 0);
    f.tick(); ensure(f.gate.program().mId == f.input.sources[1].handle.mId);
    CycleFixture wrap; wrap.input.sources[0].kind = ESourceSlotKind::MEDIA_URL;
    wrap.input.activate = true; wrap.settle();
    wrap.input.sources[0].duration = 1.; wrap.input.sources[0].position = .99;
    wrap.input.now = 2.01;
    const auto actions = wrap.tick();
    ensure("ready interval boundary in wrap window proposes early", std::any_of(actions.begin(), actions.end(),
        [](const auto& a) { return a.kind == EGateAction::PROPOSE; }));
    ensure("ready cut never asks outgoing input to seek/restart", std::none_of(actions.begin(), actions.end(),
        [&wrap](const auto& a) { return a.kind == EGateAction::SEEK && a.handle.mId == wrap.input.sources[0].handle.mId; }));
}

template<> template<> void prism_gate_test_object::test<18>()
{
    using namespace LLPrismLens;
    set_test_name("starvation pauses seek/frame clock; boundary cap and refusal stay bounded");
    CycleFixture f; f.input.sources[0].kind = ESourceSlotKind::MEDIA_URL;
    f.input.activate = true; f.input.sources[0].opportunity = false; f.tick();
    f.tick(11.); ensure(f.gate.incoming().mId.notNull());
    f.input.sources[0].opportunity = true; f.tick(10.1);
    ensure(f.gate.status(f.input.sources[0].handle) == "Skipped: Seek/frame deadline");
    CycleFixture boundary; boundary.settle(); boundary.input.sources[1].ready = false;
    boundary.tick(1.); boundary.tick(2.01);
    ensure(boundary.gate.program().mId == boundary.input.sources[0].handle.mId);
    ensure(boundary.gate.status(boundary.input.sources[1].handle) == "Skipped: Take timed out");
}

template<> template<> void prism_gate_test_object::test<19>()
{
    using namespace LLPrismLens;
    set_test_name("restored private-only Gate becomes eligible on Play videos; unknown EOF advances once");
    CycleFixture f; f.input.sources[0].kind = ESourceSlotKind::MEDIA_URL;
    f.input.settings.mArmed.resize(1); f.input.sources.resize(1);
    f.input.settings.mProgramArmIndex = -1;
    f.tick(); ensure(f.gate.program().mId.isNull());
    f.input.playVideos = true; f.settle(); ensure(f.gate.program().mId == f.input.sources[0].handle.mId);
    CycleFixture eof; eof.input.sources[0].kind = ESourceSlotKind::MEDIA_URL;
    eof.input.sources[0].duration = 0.; eof.input.activate = true;
    eof.input.settings.mVideoDwell = EGateVideoDwell::PLAY_TO_END;
    eof.settle(); eof.tick(5.); ensure(eof.gate.program().mId == eof.input.sources[0].handle.mId);
    eof.input.sources[0].eof = true; eof.tick(); eof.tick();
    ensure(eof.gate.program().mId == eof.input.sources[1].handle.mId);
}

template<> template<> void prism_gate_test_object::test<20>()
{
    using namespace LLPrismLens;
    set_test_name("idle release retains bookkeeping and kill switch cancels fade without a third role");
    CycleFixture f; f.input.sources[0].kind = ESourceSlotKind::MEDIA_URL;
    f.input.activate = true; f.settle();
    f.input.settings.mMode = EGateMode::MANUAL; f.input.settings.mPreviewArmIndex = 1;
    f.input.take = true; f.settle(12); f.tick();
    const auto actions = f.tick(61.);
    ensure(std::any_of(actions.begin(), actions.end(), [](const auto& a) { return a.kind == EGateAction::RELEASE; }));
    ensure(f.gate.status(f.input.sources[0].handle) == "Idle video released - bookmark kept");
    CycleFixture fade; fade.input.settings.mTransition = EGateTransition::CROSSFADE;
    fade.settle(); fade.tick(1.1); fade.settle(); ensure(fade.gate.phase() == EGatePhase::CROSSFADING);
    fade.input.mediaEnabled = false; fade.tick();
    ensure(fade.gate.phase() != EGatePhase::CROSSFADING && !fade.gate.pending());
}

template<> template<> void prism_gate_test_object::test<21>()
{
    using namespace LLPrismLens;
    set_test_name("URL On cut and display/projector Gate subscription serialization compatibility");
    LLSD slot = LLSD::emptyMap(); EMediaOnCut policy;
    ensure(gateOnCutFromLLSD(slot, policy) && policy == EMediaOnCut::LIVE);
    for (auto authored : {EMediaOnCut::RESTART, EMediaOnCut::RESUME, EMediaOnCut::LIVE})
    { slot["source_on_cut"] = gateOnCutName(authored); ensure(gateOnCutFromLLSD(slot, policy) && policy == authored); }
    slot["source_on_cut"] = "future"; ensure(!gateOnCutFromLLSD(slot, policy));
    slot["source_on_cut"] = 2; ensure(!gateOnCutFromLLSD(slot, policy));
    LLSD projector = LLSD::emptyMap(); bool gate = true;
    ensure(gateOutputSourceFromLLSD(projector, gate) && !gate);
    projector["gate_source"] = true; projector["capture_id"] = LLUUID::null;
    ensure(gateOutputSourceFromLLSD(projector, gate) && gate);
    ensure(gateDisplayCaptureReferenceAccepted(gate, false));
    projector["gate_source"] = "true"; ensure(!gateOutputSourceFromLLSD(projector, gate));
}
template<> template<> void prism_gate_test_object::test<22>()
{
    using namespace LLPrismLens;
    set_test_name("Manual private READY stays parked through a long dwell until Take");
    CycleFixture f; f.input.settings.mMode = EGateMode::MANUAL;
    f.input.sources[1].kind = ESourceSlotKind::MEDIA_URL;
    f.input.sources[1].onCut = EMediaOnCut::RESTART;
    f.input.settings.mPreviewArmIndex = 1; f.input.activate = true;
    f.settle(30);
    ensure(f.gate.program().mId == f.input.sources[0].handle.mId);
    ensure(f.gate.phase() == EGatePhase::READY && f.input.sources[1].paused);
    for (U32 i = 0; i < 20; ++i)
    {
        const auto actions = f.tick(3.);
        ensure(std::none_of(actions.begin(), actions.end(), [](const auto& a) { return a.kind == EGateAction::RESUME; }));
    }
    f.input.take = true; f.settle(20);
    ensure(f.gate.program().mId == f.input.sources[1].handle.mId);
}
template<> template<> void prism_gate_test_object::test<23>()
{
    using namespace LLPrismLens;
    set_test_name("pending Manual Take coalesces without re-seeking or resetting its proposal");
    CycleFixture f; f.input.settings.mMode = EGateMode::MANUAL;
    f.input.sources[1].kind = ESourceSlotKind::MEDIA_URL;
    f.input.settings.mPreviewArmIndex = 1; f.input.activate = true; f.settle(20); f.accept = false;
    U32 seeks = 0; U64 proposal = 0;
    for (U32 i = 0; i < 10; ++i)
    {
        f.input.take = true;
        for (const auto& a : f.tick())
        {
            if (a.kind == EGateAction::SEEK) ++seeks;
            if (a.kind == EGateAction::PROPOSE) { ensure(!proposal); proposal = a.token; }
        }
    }
    ensure_equals(seeks, 1u); ensure(proposal != 0 && f.gate.pending());
    f.input.accepted = proposal; f.tick(); ensure(f.gate.program().mId == f.input.sources[1].handle.mId);
}

template<> template<> void prism_gate_test_object::test<24>()
{
    using namespace LLPrismLens;
    set_test_name("250ms clock steps, one paused-seek image, held pause, and observed resume baseline");
    LLPluginGateClock clock;
    clock.sample(1., 0., true, false, .0445);
    clock.sample(1.25, .25, true, false, .0445);
    auto s = clock.sample(1.5, .5, true, false, .0445);
    ensure(clock.period() >= .249 && s.error < .15);
    clock.seek(0.);
    s = clock.sample(1.51, 0., false, true, .0445);
    ensure(gatePositionQualified(s.position, s.error, 0., 0., .0445));
    s = clock.sample(60., 0., false, true, .0445);
    ensure_equals(s.position, 0.);
    clock.sample(60.01, 0., true, false, .0445);
    s = clock.sample(60.05, 0., true, false, .0445);
    ensure(s.position > .039 && s.position < .041);
    ensure(gatePositionQualified(s.position, s.error + .01, .04, .01, .0445));
    ensure(!gatePositionQualified(s.position, s.error, .6, .01, .0445));
    ensure(!gatePositionQualified(.2, s.error, 0., .01, .0445));
    clock.reset(); s = clock.sample(1., 0., true, false, .0445); ensure(s.error >= .15);
}

template<> template<> void prism_gate_test_object::test<25>()
{
    using namespace LLPrismLens;
    set_test_name("cold Live waits for admitted metadata instead of latching transient unknown length");
    CycleFixture f; f.input.settings.mMode = EGateMode::MANUAL;
    f.input.settings.mPreviewArmIndex = 1; f.input.sources[1].kind = ESourceSlotKind::MEDIA_URL;
    f.input.sources[1].metadataReady = false; f.input.sources[1].duration = 0.; f.input.activate = true;
    f.settle(20);
    ensure(f.gate.status(f.input.sources[1].handle) != "Live needs a known length - restarting");
    f.input.sources[1].metadataReady = true; f.input.sources[1].duration = 20.; f.settle(20);
    ensure(f.gate.phase() == EGatePhase::READY);
}
template<> template<> void prism_gate_test_object::test<26>()
{
    using namespace LLPrismLens;
    set_test_name("non-loop clock clamps at EOF and a reached predicted-end cut stays due");
    ensure_equals(LLPluginGateClock::boundedPosition(20.1, 20., false), 20.);
    ensure(LLPluginGateClock::boundedPosition(20.1, 20., true) < .11);
    CycleFixture f; f.input.sources[0].kind = ESourceSlotKind::MEDIA_URL;
    f.input.settings.mVideoDwell = EGateVideoDwell::PLAY_TO_END; f.input.activate = true; f.settle();
    f.input.sources[1].ready = false; f.input.sources[0].position = 19.95; f.tick(); f.tick(.1);
    f.input.sources[0].eof = true; f.input.sources[0].position = 0.; f.tick();
    f.input.sources[1].ready = true; f.settle(3);
    ensure(f.gate.program().mId == f.input.sources[1].handle.mId);
}
template<> template<> void prism_gate_test_object::test<27>()
{
    using namespace LLPrismLens;
    set_test_name("kill-off preserves standalone Play; known early EOF waits for the predicted end");
    ensure(!gateInterceptsPlay(true, false, true)); ensure(gateInterceptsPlay(true, true, true));
    ensure(!gateInterceptsPlay(false, true, true)); ensure(!gateInterceptsPlay(true, true, false));
    CycleFixture f; f.input.sources[0].kind = ESourceSlotKind::MEDIA_URL;
    f.input.settings.mVideoDwell = EGateVideoDwell::PLAY_TO_END; f.input.activate = true; f.settle();
    f.input.sources[0].position = 19.; f.tick();
    f.input.sources[0].eof = true; f.tick(.1); f.settle(3);
    ensure(f.gate.program().mId == f.input.sources[0].handle.mId);
    f.tick(1.); f.settle(3); ensure(f.gate.program().mId == f.input.sources[1].handle.mId);
}

template<> template<> void prism_gate_test_object::test<28>()
{
    using namespace LLPrismLens;
    set_test_name("outgoing removal during a private fade promotes the current incoming image through publication");
    CycleFixture f; f.input.sources[1].kind = ESourceSlotKind::MEDIA_URL;
    f.input.settings.mTransition = EGateTransition::CROSSFADE; f.input.activate = true;
    f.settle(); f.tick(1.1); f.settle(10); ensure(f.gate.phase() == EGatePhase::CROSSFADING);
    f.tick(.2); f.input.sources[0].available = false; f.tick(); f.tick();
    ensure(f.gate.program().mId == f.input.sources[1].handle.mId);
}

template<> template<> void prism_gate_test_object::test<29>()
{
    using namespace LLPrismLens;
    set_test_name("Hold keeps a fixed-start private candidate paused until budget fits");
    CycleFixture f; f.input.settings.mMode = EGateMode::MANUAL; f.input.settings.mPreviewArmIndex = 1;
    f.input.sources[1].kind = ESourceSlotKind::MEDIA_URL; f.input.sources[1].onCut = EMediaOnCut::RESTART;
    f.input.settings.mTransition = EGateTransition::CROSSFADE; f.input.settings.mFadeOverBudget = EGateFadeOverBudget::HOLD;
    f.input.activate = true; f.input.destinations.qualified = false; f.settle(20);
    ensure(f.input.sources[1].paused); f.input.take = true; f.settle(10);
    for (U32 i = 0; i < 5; ++i)
        for (const auto& a : f.tick(.1)) ensure(a.kind != EGateAction::RESUME);
    f.input.destinations.qualified = true; f.settle(15);
    ensure(f.gate.phase() == EGatePhase::CROSSFADING);
}
template<> template<> void prism_gate_test_object::test<30>()
{
    using namespace LLPrismLens;
    set_test_name("Hold budget loss during private pre-roll parks once and re-finalizes when it fits");
    CycleFixture f; f.input.settings.mMode = EGateMode::MANUAL; f.input.settings.mPreviewArmIndex = 1;
    f.input.sources[1].kind = ESourceSlotKind::MEDIA_URL; f.input.sources[1].onCut = EMediaOnCut::RESTART;
    f.input.settings.mTransition = EGateTransition::CROSSFADE; f.input.settings.mFadeOverBudget = EGateFadeOverBudget::HOLD;
    f.input.activate = true; f.settle(20); f.input.take = true;
    bool resumed = false;
    for (U32 i = 0; i < 12 && !resumed; ++i)
        for (const auto& a : f.tick()) if (a.kind == EGateAction::RESUME) resumed = true;
    ensure(resumed); f.input.destinations.qualified = false;
    const auto park = f.tick(); ensure(std::any_of(park.begin(), park.end(), [](const auto& a) { return a.kind == EGateAction::PARK; }));
    f.tick(.5); ensure(f.input.sources[1].paused);
    f.input.destinations.qualified = true; f.settle(15); ensure(f.gate.phase() == EGatePhase::CROSSFADING);
}

template<> template<> void prism_gate_test_object::test<31>()
{
    using namespace LLPrismLens;
    set_test_name("early known EOF does not pin an obsolete future fade length");
    CycleFixture f; f.input.sources[0].kind = ESourceSlotKind::MEDIA_URL;
    f.input.settings.mVideoDwell = EGateVideoDwell::PLAY_TO_END;
    f.input.settings.mTransition = EGateTransition::CROSSFADE; f.input.settings.mCrossfadeSeconds = 3.;
    f.input.activate = true; f.settle(); f.input.sources[0].position = 16.; f.tick();
    f.input.sources[0].eof = true; f.tick(.1);
    f.input.settings.mCrossfadeSeconds = .1; f.input.changed = true; f.tick(1.1); f.settle(3);
    ensure(f.gate.program().mId == f.input.sources[0].handle.mId && f.gate.phase() != EGatePhase::CROSSFADING);
}

// Fix round 1: exercise the production protocol, including asynchronous frames.
template<> template<> void prism_gate_test_object::test<32>()
{
    using namespace LLPrismLens;
    set_test_name("fade endpoint remains an incoming blend until the output ACK");
    for (double endpoint : {.5, .50001, .51667})
    {
        CycleFixture f; f.input.settings.mTransition = EGateTransition::CROSSFADE;
        f.settle(); f.tick(1.1); f.settle(); ensure(f.gate.phase() == EGatePhase::CROSSFADING);
        f.accept = false; const auto outgoing = f.gate.program();
        const auto actions = f.tick(endpoint);
        bool proposed = false;
        for (const auto& a : actions) if (a.kind == EGateAction::PROPOSE)
        { proposed = true; ensure("endpoint stays a two-feed publication", a.seconds > 0.); }
        ensure(proposed && f.gate.alpha() == 1. && f.gate.program().mId == outgoing.mId);
        f.input.accepted = f.gate.token(); f.tick();
        ensure(f.gate.program().mId != outgoing.mId);
    }
}
template<> template<> void prism_gate_test_object::test<33>()
{
    using namespace LLPrismLens;
    set_test_name("first Restart/Resume contribution stays within 150 ms of TARGET, never target plus dispatch delay");
    for (auto mode : {EMediaOnCut::RESTART, EMediaOnCut::RESUME, EMediaOnCut::LIVE})
        for (F64 advancement : {.08, .18, .5})
        {
            CycleFixture f; f.input.settings.mMode = EGateMode::MANUAL;
            f.input.settings.mPreviewArmIndex = 1; f.input.sources[1].kind = ESourceSlotKind::MEDIA_URL;
            f.input.sources[1].onCut = mode;
            if (mode == EMediaOnCut::RESUME) f.input.settings.mProgramArmIndex = 1;
            f.input.activate = true; f.settle(30);
            if (mode == EMediaOnCut::RESUME)
            {
                // Contribute, leave air, and receive the real retirement ACK so
                // Resume measures a nonzero bookmark, not its cold zero fallback.
                f.input.settings.mPreviewArmIndex = 0; f.input.changed = f.input.take = true; f.settle(20);
                ensure(f.gate.program().mId == f.input.sources[0].handle.mId);
                f.input.settings.mPreviewArmIndex = 1; f.input.changed = true; f.settle(30);
                ensure("Resume has a contributed bookmark", f.gate.targetPosition() > 0.);
            }
            f.input.take = true; bool resumed = false;
            for (U32 i = 0; i < 15 && !resumed; ++i)
                for (const auto& a : f.tick()) if (a.kind == EGateAction::RESUME) resumed = true;
            ensure(resumed);
            f.tick(advancement); f.tick();
            const auto& incoming = f.input.sources[1];
            const bool committed = f.gate.program().mId == incoming.handle.mId;
            ensure_equals("E=30ms plus actual advancement must fit 150ms", committed, advancement + incoming.error <= .15);
            if (committed) ensure(gatePositionDistance(incoming.position, f.gate.targetPosition(), incoming.duration, incoming.loop) + incoming.error <= .150001);
        }
    CycleFixture delayed; delayed.input.settings.mMode = EGateMode::MANUAL;
    delayed.input.settings.mPreviewArmIndex = 1; delayed.input.sources[1].kind = ESourceSlotKind::MEDIA_URL;
    delayed.input.sources[1].onCut = EMediaOnCut::RESTART; delayed.input.activate = true; delayed.settle(30);
    delayed.input.take = true; U64 resume = 0;
    for (U32 i = 0; i < 15 && !resume; ++i)
        for (const auto& a : delayed.tick()) if (a.kind == EGateAction::RESUME) resume = a.token;
    ensure(resume != 0);
    delayed.input.now += .5; // command in transit, no playback advancement
    delayed.input.sources[1].position = delayed.gate.targetPosition();
    delayed.tick(); delayed.tick();
    ensure("dispatch delay must not be counted as playback", delayed.gate.program().mId == delayed.input.sources[1].handle.mId);
}
template<> template<> void prism_gate_test_object::test<34>()
{
    using namespace LLPrismLens;
    set_test_name("READY successor never expires or cuts a long Play-to-end Program mid-clip");
    CycleFixture f; f.input.sources[0].kind = ESourceSlotKind::MEDIA_URL;
    f.input.sources[0].duration = 7200.; f.input.settings.mVideoDwell = EGateVideoDwell::PLAY_TO_END;
    f.input.activate = true; f.settle(30);
    for (U32 i = 0; i < 200; ++i)
    {
        f.tick(10.);
        ensure("READY is not subject to warm deadlines", f.gate.phase() == EGatePhase::READY);
        f.tick();
        ensure(f.gate.program().mId == f.input.sources[0].handle.mId);
        ensure(f.gate.phase() == EGatePhase::READY);
    }
}
template<> template<> void prism_gate_test_object::test<35>()
{
    using namespace LLPrismLens;
    set_test_name("deactivation retains the accepted Program and revokes private grants");
    CycleFixture f; f.input.activate = true; f.settle(); const auto accepted = f.gate.program();
    f.input.settings.mActive = false; f.tick();
    ensure(f.gate.phase() == EGatePhase::INACTIVE);
    ensure("outputs keep their last accepted feed", f.gate.program().mId == accepted.mId);
}
template<> template<> void prism_gate_test_object::test<36>()
{
    using namespace LLPrismLens;
    set_test_name("failed Auto warm candidate advances to healthy later arm in the same pass");
    CycleFixture f; f.input.sources[1].ready = false; f.settle();
    f.tick(1.1); f.tick(2.1); f.settle(5);
    ensure("B failure cannot starve C", f.gate.program().mId == f.input.sources[2].handle.mId);
}
template<> template<> void prism_gate_test_object::test<37>()
{
    using namespace LLPrismLens;
    set_test_name("cold preferred index enumerates each arm including zero after media disable");
    CycleFixture f; f.input.settings.mArmed.resize(2); f.input.sources.resize(2);
    f.input.settings.mProgramArmIndex = 1; f.input.sources[1].kind = ESourceSlotKind::MEDIA_URL;
    f.input.mediaEnabled = false; f.settle(5);
    ensure(f.gate.program().mId == f.input.sources[0].handle.mId);
}
template<> template<> void prism_gate_test_object::test<38>()
{
    using namespace LLPrismLens;
    set_test_name("disabling WARMING or READY Auto arm cancels its pending Take");
    for (bool ready : {false, true})
    {
        CycleFixture f; f.input.sources[1].ready = ready; f.settle();
        ensure(f.gate.incoming().mId == f.input.sources[1].handle.mId);
        f.input.settings.mArmed[1].mEnabled = false; f.input.changed = true;
        f.tick(); f.tick(1.1); f.settle(3);
        ensure("disabled candidate must never become Program", f.gate.program().mId == f.input.sources[2].handle.mId);
    }
}
template<> template<> void prism_gate_test_object::test<39>()
{
    using namespace LLPrismLens;
    set_test_name("activation claims audible off-air URLs even with no Manual Preview or enabled Auto arm");
    CycleFixture f; f.input.settings.mMode = EGateMode::MANUAL; f.input.settings.mPreviewArmIndex = -1;
    f.input.sources[1].kind = f.input.sources[2].kind = ESourceSlotKind::MEDIA_URL;
    f.input.sources[1].resident = f.input.sources[2].resident = true;
    f.input.settings.mArmed[2].mEnabled = false; f.input.activate = true;
    const auto actions = f.tick();
    for (U32 i : {1u, 2u})
    {
        ensure(std::any_of(actions.begin(), actions.end(), [&](const auto& a) { return a.kind == EGateAction::CLAIM && a.handle.mId == f.input.sources[i].handle.mId; }));
        ensure(std::any_of(actions.begin(), actions.end(), [&](const auto& a) { return a.kind == EGateAction::RETIRE && a.handle.mId == f.input.sources[i].handle.mId; }));
        ensure(f.gate.charged(f.input.sources[i].handle));
    }
    f.tick(); ensure_equals(f.gate.charges(), 0u);
}
template<> template<> void prism_gate_test_object::test<40>()
{
    using namespace LLPrismLens;
    set_test_name("camera watch honors authored prewarm lead with a minimum of one frame");
    for (U32 frames : {0u, 1u, 3u, 5u})
    {
        CycleFixture f; f.input.settings.mIntervalSeconds = 30.; f.input.settings.mPrewarmFrames = frames;
        f.settle(); f.input.sources[1].frame = 1; // cold/stale retained output
        ensure(!f.gate.wantsImage(f.input));
        f.tick(29. - f.input.now + 1.02); ensure(!f.gate.wantsImage(f.input));
        // Use ordinary frame steps, never count the preceding long wall-clock step as a frame.
        f.input.now = 31.04 - std::max(1u, frames) * .02; f.input.dt = .02; f.gate.update(f.input);
        ensure(f.gate.wantsImage(f.input));
    }
}
template<> template<> void prism_gate_test_object::test<41>()
{
    using namespace LLPrismLens;
    set_test_name("output acknowledgement invalidates qualification lost during delayed preparation");
    CycleFixture f; f.input.settings.mMode = EGateMode::MANUAL; f.input.settings.mPreviewArmIndex = 1;
    f.input.sources[1].kind = ESourceSlotKind::MEDIA_URL; f.input.sources[1].onCut = EMediaOnCut::RESTART;
    f.input.activate = true; f.settle(20); f.accept = false; f.input.take = true;
    U64 proposal = 0;
    for (U32 i = 0; i < 20 && !proposal; ++i)
        for (const auto& a : f.tick()) if (a.kind == EGateAction::PROPOSE) proposal = a.token;
    ensure(proposal != 0);
    f.input.now += .2; f.input.sources[1].position += .2;
    f.gate.acknowledgeOutputs(proposal, f.input);
    ensure("stale proposal cannot publish or unmute", f.gate.program().mId != f.input.sources[1].handle.mId && f.gate.proposal() == 0);
}

template<> template<> void prism_gate_test_object::test<42>()
{
    using namespace LLPrismLens;
    set_test_name("offscreen borrowed-face watch supplies admission-sized demand before routing");
    CycleFixture f; f.input.sources[1].kind = ESourceSlotKind::MEDIA_FACE;
    f.input.sources[1].ready = false; f.settle();
    f.input.now = 2.01; f.input.frameDt = .02; f.input.dt = .02; f.gate.update(f.input);
    ensure(f.gate.wantsImage(f.input));
    U32 width = 0, height = 0; F64 area = 0.;
    ensure(gateImageDemand(1, 1024, 768, width, height, area));
    ensure(width == 1024 && height == 768 && area == 1024. * 768.);
    f.input.sources[1].admitted = area > 0.; f.input.sources[1].ready = true;
    f.tick(1.1); f.tick(); ensure(f.gate.program().mId == f.input.sources[1].handle.mId);
    ensure(!gateImageDemand(0, 1024, 768, width, height, area));
}
template<> template<> void prism_gate_test_object::test<43>()
{
    using namespace LLPrismLens;
    set_test_name("production allocator and reducer reserve a fresh cold successor within each prewarm lead and commit");
    for (auto kind : {ESourceSlotKind::CAMERA, ESourceSlotKind::MEDIA_FACE, ESourceSlotKind::MEDIA_URL})
        for (U32 lead : {0u, 1u, 3u, 5u})
            for (F32 capacity : {1.f, 2.f, 30.f, 60.f})
            {
                CycleFixture f; f.input.sources.resize(2); f.input.settings.mArmed.resize(2);
                f.input.settings.mPrewarmFrames = lead; f.input.sources[1].kind = kind;
                f.input.sources[1].onCut = EMediaOnCut::RESTART;
                f.input.activate = true; f.gate.restoreProgram(f.input.sources[0].handle, 1.);
                auto& cold = f.input.sources[1]; cold.ready = true; cold.frame = 5; // deliberately stale mHasOutput
                cold.admitted = true;
                F64 first_image = 0.; U32 reservations = 0; F64 copy_due = 0.;
                const F64 dt = 1. / 60.; f.input.frameDt = dt;
                std::array<bool, MAX_CAPTURES> visible{}, preview{};
                std::array<F32, MAX_CAPTURES> demand{}, entitlement{}, head{};
                auto actions = f.gate.update(f.input); f.input.activate = false;
                auto dispatch = [&](const auto& commands)
                {
                    for (const auto& a : commands)
                    {
                        if (a.kind == EGateAction::SEEK || a.kind == EGateAction::PARK || a.kind == EGateAction::RESUME)
                        { cold.token = a.token; cold.position = a.position; cold.paused = a.kind != EGateAction::RESUME; cold.resumed = !cold.paused; }
                        if (a.kind == EGateAction::PROPOSE) f.gate.acknowledgeOutputs(a.token, f.input);
                    }
                };
                dispatch(actions);
                for (U32 frame = 0; frame < 70 && f.gate.program().mId != cold.handle.mId; ++frame)
                {
                    f.input.dt = dt; f.input.now += dt;
                    if (cold.resumed) cold.position += dt;
                    dispatch(f.gate.update(f.input));
                    visible.fill(false); demand.fill(0.f); visible[0] = true; demand[0] = capacity;
                    const bool watched = f.gate.wantsImage(f.input);
                    preview[1] = watched; if (watched) demand[1] = capacity;
                    const S32 reserved = f.gate.reservesImage(f.input) ? 1 : -1;
                    gateCameraCadence(visible, preview, demand, capacity, 0, 1, false, entitlement, reserved);
                    if (!watched)
                    {
                        head.fill(0.f); LLPrismPreviewPolicy::waterFillTier(visible, demand, capacity, head);
                        ensure_equals("Program outside lead is unchanged from HEAD", entitlement[0], head[0]);
                    }
                    // Media has an independent copy allocation; it cannot use
                    // or reduce the camera's normal allocation.
                    const F32 copy_hz = kind == ESourceSlotKind::CAMERA ? entitlement[1] : watched ? capacity : 0.f;
                    if (gateCadenceDue(copy_hz, f.input.now, copy_due, reserved == 1))
                    {
                        if (!first_image) first_image = f.input.now;
                        if (reserved == 1) ++reservations;
                        if (copy_hz > 0.f) copy_due = f.input.now + 1. / copy_hz;
                        ++cold.frame; cold.ready = true; cold.frameAge = cold.sampleAge = 0.;
                        ensure_equals("reservation preserves Program HEAD share", entitlement[0], capacity);
                    }
                    f.input.dt = 0.; dispatch(f.gate.update(f.input));
                }
                const F64 lead_start = kind == ESourceSlotKind::CAMERA ?
                    2. - gateLeadSeconds(lead, dt, false) : 1.;
                ensure("no stale output may bypass prewarm", first_image >= lead_start - 1e-6);
                ensure("fresh image produced by boundary, including a one-frame lead", first_image > 0. && first_image <= 2. + 1e-6);
                ensure(llformat("Automatic cold successor commits kind=%d lead=%u capacity=%.1f phase=%d target=%.4f pos=%.4f reason=%s", int(kind), lead, capacity, int(f.gate.phase()), f.gate.targetPosition(), cold.position, f.gate.reason().c_str()), f.gate.program().mId == cold.handle.mId);
                ensure("only first copies and one correction reserve frames", reservations <= (kind == ESourceSlotKind::MEDIA_URL ? 4u : 1u));
            }
}

template<> template<> void prism_gate_test_object::test<44>()
{
    using namespace LLPrismLens;
    set_test_name("cold camera waits for its prewarm lead throughout a multi-hour Play-to-end Program");
    CycleFixture f; f.input.sources[0].kind = ESourceSlotKind::MEDIA_URL; f.input.activate = true;
    f.input.sources[0].duration = 7200.; f.input.settings.mVideoDwell = EGateVideoDwell::PLAY_TO_END;
    f.input.sources[1].ready = false; f.settle();
    f.input.sources[0].position += 1900.; f.tick(1900.); f.input.dt = .02; f.gate.update(f.input);
    ensure(f.gate.program().mId == f.input.sources[0].handle.mId);
    ensure(f.gate.incoming().mId == f.input.sources[1].handle.mId);
    ensure(!f.gate.wantsImage(f.input));
}
template<> template<> void prism_gate_test_object::test<45>()
{
    using namespace LLPrismLens;
    set_test_name("reactivation reclaims standalone owners and reparks them under retirement accounting");
    CycleFixture f; f.input.sources[1].kind = ESourceSlotKind::MEDIA_URL;
    f.input.settings.mMode = EGateMode::MANUAL; f.input.settings.mPreviewArmIndex = -1;
    f.input.activate = true; f.settle();
    f.input.settings.mActive = false; f.tick();
    f.input.sources[1].resident = true; f.input.settings.mActive = true; f.input.activate = true;
    const auto actions = f.tick();
    ensure(std::any_of(actions.begin(), actions.end(), [&](const auto& a) { return a.kind == EGateAction::CLAIM && a.handle.mId == f.input.sources[1].handle.mId; }));
    ensure(f.gate.charged(f.input.sources[1].handle));
}
template<> template<> void prism_gate_test_object::test<46>()
{
    using namespace LLPrismLens;
    set_test_name("newly parked claimed input keeps its idle grace from its actual pause ACK");
    CycleFixture f; f.input.now = 1000.; f.input.sources[1].kind = ESourceSlotKind::MEDIA_URL;
    f.input.sources[1].resident = true; f.input.activate = true;
    f.input.settings.mMode = EGateMode::MANUAL; f.input.settings.mPreviewArmIndex = -1;
    f.tick(); const auto actions = f.tick();
    ensure(std::none_of(actions.begin(), actions.end(), [](const auto& a) { return a.kind == EGateAction::RELEASE; }));
    ensure(f.gate.keepsParked(f.input.sources[1].handle));
}

template<> template<> void prism_gate_test_object::test<47>()
{
    using namespace LLPrismLens;
    set_test_name("inactive Play videos authorizes without playback; kill-off cannot authorize");
    CycleFixture f; f.input.settings.mActive = false; f.input.sources[1].kind = ESourceSlotKind::MEDIA_URL;
    f.input.playVideos = true; const auto actions = f.tick();
    ensure(f.gate.granted(f.input.sources[1].handle)); ensure(actions.empty());
    ensure(f.gate.phase() == EGatePhase::INACTIVE);
    f.gate.reset(); f.input.mediaEnabled = false; f.input.playVideos = true; f.tick();
    ensure(!f.gate.granted(f.input.sources[1].handle));
}

template<> template<> void prism_gate_test_object::test<48>()
{
    using namespace LLPrismLens;
    set_test_name("retiring video arm cannot block a healthy later camera in the bounded cycle pass");
    CycleFixture f; f.input.sources[1].kind = ESourceSlotKind::MEDIA_URL;
    f.input.sources[1].resident = true; f.pauseAck = false; f.input.activate = true;
    f.settle(); ensure(f.gate.charged(f.input.sources[1].handle));
    ensure(f.gate.incoming().mId == f.input.sources[2].handle.mId);
    f.tick(1.1); f.tick();
    ensure(f.gate.program().mId == f.input.sources[2].handle.mId);
}
template<> template<> void prism_gate_test_object::test<49>()
{
    using namespace LLPrismLens;
    set_test_name("fade-start ACK is one-shot and cannot replay resident claims or old retirement receipts");
    CycleFixture f; f.input.settings.mTransition = EGateTransition::CROSSFADE;
    f.input.sources[1].kind = ESourceSlotKind::MEDIA_URL; f.input.activate = true;
    f.settle(); f.accept = false; U64 proposal = 0;
    for (U32 i = 0; i < 40 && !proposal; ++i)
        for (const auto& a : f.tick(i ? .01 : 1.)) if (a.kind == EGateAction::PROPOSE) proposal = a.token;
    ensure(proposal != 0); f.gate.acknowledgeOutputs(proposal, f.input);
    ensure(f.gate.phase() == EGatePhase::CROSSFADING);
    f.input.settings.mActive = false; f.gate.update(f.input);
    const auto charges = f.gate.charges(); const auto token = f.gate.token();
    for (U32 i = 0; i < 10; ++i) ensure(f.gate.acknowledgeOutputs(proposal, f.input).empty());
    ensure_equals(f.gate.charges(), charges); ensure_equals(f.gate.token(), token);
    ensure(f.gate.phase() == EGatePhase::INACTIVE);
}
template<> template<> void prism_gate_test_object::test<50>()
{
    using namespace LLPrismLens;
    set_test_name("Program and another visible monitor match HEAD with two previews and an additive reservation");
    std::array<bool, MAX_CAPTURES> visible{}, preview{};
    std::array<F32, MAX_CAPTURES> demand{}, entitlement{}, head{};
    visible[0] = visible[1] = true; preview[2] = preview[3] = true;
    demand[0] = demand[1] = 60.f; demand[2] = demand[3] = 5.f;
    for (F32 capacity : {1.f, 2.f, 10.f, 30.f, 60.f})
        for (S32 reservation : {-1, 2})
        {
            head.fill(0.f);
            const F32 left = LLPrismPreviewPolicy::waterFillTier(visible, demand, capacity, head);
            LLPrismPreviewPolicy::waterFillTier(preview, demand, left, head);
            gateCameraCadence(visible, preview, demand, capacity, 0, 2, false, entitlement, reservation);
            for (U32 i = 0; i < MAX_CAPTURES; ++i) ensure_equals(entitlement[i], head[i]);
        }
    visible[1] = false;
    gateCameraCadence(visible, preview, demand, 10.f, 0, 2, false, entitlement);
    ensure_equals("Automatic Program retains HEAD 10 Hz with two 5 Hz previews", entitlement[0], 10.f);

}
template<> template<> void prism_gate_test_object::test<51>()
{
    using namespace LLPrismLens;
    set_test_name("production frame adapter preserves Program during Manual video preview, camera/media warm and Auto video to video");
    for (bool automatic : {false, true})
        for (bool camera : {false, true})
        {
            GateFrameBudget budget;
            U32 program = 0, preview = 0, monitor = 0, warm = 0;
            F64 program_due = 0., preview_due = 0., monitor_due = 0.;
            for (U32 frame = 0; frame < 180; ++frame)
            {
                const F64 now = frame / 60.;
                const bool reservation = frame >= 57 && frame < 60;
                // The ordinary Program and monitor copies precede/overlap the
                // reserved copy; neither camera token is consumed by any copy.
                if (budget.mediaDue(10.f, now, monitor_due, false))
                { ++monitor; monitor_due = now + .1; }
                if (!camera && budget.mediaDue(10.f, now, program_due, false))
                { ++program; program_due = now + .1; }
                if (budget.mediaDue(5.f, now, preview_due, reservation))
                { ++preview; if (now >= preview_due) preview_due = now + .2; }
                ensure("media leaves the ordinary world-render token available", budget.cameraAvailable(frame));
                if (camera && now >= program_due && budget.cameraAvailable(frame))
                { ++program; program_due = now + .1; budget.cameraAttempt(frame); }
                const bool normal_available = budget.cameraAvailable(frame);
                if (budget.warmCameraAttempt(frame, reservation)) ++warm;
                ensure_equals("extra render never changes ordinary admission", budget.cameraAvailable(frame), normal_available);
                ensure("at most one extra camera attempt per frame", !budget.warmCameraAttempt(frame, reservation));
                if (!reservation) ensure(!budget.warmCameraAttempt(frame, false));
            }
            ensure_equals("Program retains its normal cadence", program, monitor);
            ensure_equals("three authored lead frames give at most three extra renders", warm, 3u);
            ensure(preview >= 15u);
        }
}
template<> template<> void prism_gate_test_object::test<52>()
{
    using namespace LLPrismLens;
    set_test_name("Manual Preview uses ordinary demand and a pending Take cannot extend its lead reservation");
    CycleFixture f; f.input.settings.mMode = EGateMode::MANUAL;
    f.input.settings.mPreviewArmIndex = 1; f.input.settings.mPrewarmFrames = 3;
    f.input.sources[0].kind = f.input.sources[1].kind = ESourceSlotKind::MEDIA_URL;
    f.input.sources[0].onCut = f.input.sources[1].onCut = EMediaOnCut::RESTART;
    f.input.activate = true; f.settle(20);
    const auto program = f.gate.program(); ensure(program.mId == f.input.sources[0].handle.mId);
    f.input.sources[1].ready = false; f.input.sources[1].metadataReady = false;
    f.input.frameDt = f.input.dt = .02;
    ensure(f.gate.wantsImage(f.input)); ensure(!f.gate.reservesImage(f.input));
    f.input.take = true; f.gate.update(f.input); f.input.take = false;
    ensure(f.gate.reservesImage(f.input));
    f.input.now += .31; f.gate.update(f.input);
    ensure(f.gate.pending() && f.gate.wantsImage(f.input));
    ensure("late readiness cannot hold a reservation", !f.gate.reservesImage(f.input));
    for (U32 i = 0; i < 20; ++i)
    {
        f.input.now += .02;
        const auto actions = f.gate.update(f.input);
        ensure(f.gate.program().mId == program.mId && !f.gate.reservesImage(f.input));
        ensure(std::none_of(actions.begin(), actions.end(), [&](const auto& a)
            { return a.handle.mId == program.mId && (a.kind == EGateAction::PARK || a.kind == EGateAction::RETIRE || a.kind == EGateAction::ENVELOPE); }));
    }
}
template<> template<> void prism_gate_test_object::test<53>()
{
    using namespace LLPrismLens;
    set_test_name("Restart Resume and Live proposals are invalidated at delayed publication, with no late envelope");
    for (auto mode : {EMediaOnCut::RESTART, EMediaOnCut::RESUME, EMediaOnCut::LIVE})
    {
        CycleFixture f; f.input.settings.mMode = EGateMode::MANUAL;
        f.input.settings.mPreviewArmIndex = 1; f.input.sources[1].kind = ESourceSlotKind::MEDIA_URL;
        f.input.sources[1].onCut = mode;
        if (mode != EMediaOnCut::RESTART) f.input.settings.mProgramArmIndex = 1;
        f.input.activate = true; f.settle(25);
        if (mode != EMediaOnCut::RESTART)
        {
            f.input.sources[1].position = 4.; f.input.settings.mPreviewArmIndex = 0;
            f.input.take = true; f.settle(15); f.tick();
            f.input.settings.mPreviewArmIndex = 1; f.input.changed = true; f.settle(15);
        }
        f.accept = false; f.input.accepted = 0; f.input.take = true;
        U64 proposal = 0;
        for (U32 i = 0; i < 20 && !proposal; ++i)
            for (const auto& a : f.tick()) if (a.kind == EGateAction::PROPOSE) proposal = a.token;
        ensure(llformat("proposal policy=%d phase=%d reason=%s", int(mode), int(f.gate.phase()), f.gate.reason().c_str()), proposal != 0);
        const auto program = f.gate.program();
        f.input.now += .5;
        f.input.sources[1].position += mode == EMediaOnCut::LIVE ? .9 : .5;
        f.input.sources[1].frameAge = f.input.sources[1].sampleAge = 0.;
        ensure("current qualification expires before publication", !f.gate.validatePublication(proposal, f.input));
        ensure_equals(f.gate.proposal(), U64(0));
        const auto actions = f.gate.acknowledgeOutputs(proposal, f.input);
        ensure(actions.empty() && f.gate.program().mId == program.mId);
        f.input.sources[1].position = f.gate.targetPosition();
        ensure("invalidated tokens cannot resurrect if the clock later recovers", f.gate.acknowledgeOutputs(proposal, f.input).empty());
    }
}
template<> template<> void prism_gate_test_object::test<54>()
{
    using namespace LLPrismLens;
    set_test_name("disarm deactivate and kill switch release every parked or idle-released Gate owner");
    for (U32 exit = 0; exit < 3; ++exit)
        for (bool released : {false, true})
        {
            CycleFixture f; f.input.settings.mMode = EGateMode::MANUAL;
            f.input.settings.mPreviewArmIndex = -1;
            f.input.sources[1].kind = ESourceSlotKind::MEDIA_URL;
            f.input.sources[1].resident = true; f.input.activate = true; f.settle(15);
            ensure(f.gate.keepsParked(f.input.sources[1].handle));
            if (released)
            {
                const auto actions = f.tick(61.);
                ensure(std::any_of(actions.begin(), actions.end(), [](const auto& a) { return a.kind == EGateAction::RELEASE; }));
            }
            if (exit == 0)
            {
                f.gate.revoke(f.input.sources[1].handle); // public gateDisarm order
                f.input.settings.mArmed.erase(f.input.settings.mArmed.begin() + 1);
            }
            if (exit == 1) f.input.settings.mActive = false;
            if (exit == 2) f.input.mediaEnabled = false;
            const auto actions = f.tick();
            ensure(std::any_of(actions.begin(), actions.end(), [&](const auto& a)
                { return a.kind == EGateAction::UNCLAIM && a.handle.mId == f.input.sources[1].handle.mId; }));
            ensure(!f.gate.keepsParked(f.input.sources[1].handle) && !f.gate.charged(f.input.sources[1].handle));
        }
}
template<> template<> void prism_gate_test_object::test<55>()
{
    using namespace LLPrismLens;
    set_test_name("probe reflection and auxiliary scissor covers the full active target");
    for (bool virtual_display : {false, true}) for (bool main : {false, true})
        for (U32 extent : {128u, 512u, 1920u})
        {
            S32 scissor[4] = {123, 345, 17, 19};
            const bool full = prismFullTargetScissor(virtual_display, main, extent, extent / 2, scissor);
            ensure_equals(full, virtual_display || !main);
            if (full)
            {
                ensure_equals(scissor[0], 0); ensure_equals(scissor[1], 0);
                ensure_equals(scissor[2], S32(extent)); ensure_equals(scissor[3], S32(extent / 2));
            }
            else ensure(scissor[0] == 123 && scissor[1] == 345 && scissor[2] == 17 && scissor[3] == 19);
        }
}
template<> template<> void prism_gate_test_object::test<56>()
{
    using namespace LLPrismLens;
    set_test_name("Auto and Manual video successor gets fair copies and commits fresh within the 300ms lead at Automatic and prewarm 3");
    for (auto mode : {EGateMode::MANUAL, EGateMode::AUTO_CYCLE})
    {
        CycleFixture f;
        f.input.sources.resize(2); f.input.settings.mArmed.resize(2);
        f.input.settings.mMode = mode; f.input.settings.mPreviewArmIndex = 1;
        f.input.settings.mPrewarmFrames = 3; f.input.settings.mIntervalSeconds = 1.;
        for (auto& source : f.input.sources)
        {
            source.kind = ESourceSlotKind::MEDIA_URL; source.onCut = EMediaOnCut::RESTART;
            source.admitted = true;
        }
        // Establish a real accepted Program first. restoreProgram intentionally
        // cannot restore a private player's session grant or running transport.
        f.input.settings.mPreviewArmIndex = -1; f.input.activate = true;
        for (U32 step = 0; step < 20 && f.gate.program().mId.isNull(); ++step) f.tick();
        ensure(f.gate.program().mId == f.input.sources[0].handle.mId);
        const F64 started = f.input.now;
        f.input.settings.mPreviewArmIndex = 1;
        auto& cold = f.input.sources[1];
        cold.ready = false;
        f.input.dt = f.input.frameDt = 1. / 60.;
        f.input.settings.mTransition = EGateTransition::CUT;
        GateFrameBudget budget;
        F64 program_due = 0., baseline_due = 0., cold_due = 0.;
        U32 program_copies = 0, baseline_copies = 0, cold_copies = 0;
        U32 reservations = 0, decoded_roundtrips = 0;
        bool committed = false, saw_seek = false, saw_park = false, saw_resume = false;
        GateAction pending{EGateAction::CLAIM, {}}; S32 receipt_at = -1, image_at = -1;
        auto dispatch = [&](const auto& actions, U32 frame)
        {
            for (const auto& a : actions)
            {
                if (a.handle.mId != cold.handle.mId) continue;
                if (a.kind == EGateAction::SEEK || a.kind == EGateAction::PARK || a.kind == EGateAction::RESUME)
                {
                    pending = a; receipt_at = S32(frame) + 1;
                    if (a.kind != EGateAction::PARK) image_at = S32(frame) + 2;
                    if (a.kind == EGateAction::SEEK) saw_seek = true;
                    if (a.kind == EGateAction::PARK) saw_park = true;
                    if (a.kind == EGateAction::RESUME) saw_resume = true;
                }
                if (a.kind == EGateAction::PROPOSE)
                {
                    ensure("both image round trips precede publication", decoded_roundtrips >= 2 && saw_seek && saw_park && saw_resume);
                    ensure("fresh post-resume contribution meets the unchanged 150ms bound",
                        gatePositionDistance(cold.position, f.gate.targetPosition(), cold.duration, cold.loop) + cold.error <= .150001);
                    f.gate.acknowledgeOutputs(a.token, f.input);
                    committed = f.gate.program().mId == cold.handle.mId;
                    const F64 deadline = started + (mode == EGateMode::MANUAL ? .5 + .3 : 1.);
                    ensure("commit finishes within the lead", f.input.now <= deadline + 1e-6);
                }
            }
        };
        for (U32 frame = 0; frame < 90 && !committed; ++frame)
        {
            f.input.now = started + frame / 60.; f.input.dt = f.input.frameDt;
            cold.sampleAge = 0.; cold.frameAge += f.input.dt;
            if (cold.resumed) cold.position += f.input.dt;
            if (receipt_at == S32(frame))
            {
                cold.token = pending.token; cold.position = pending.position;
                cold.paused = pending.kind != EGateAction::RESUME; cold.resumed = !cold.paused;
            }
            if (image_at == S32(frame)) { ++decoded_roundtrips; }
            f.input.take = mode == EGateMode::MANUAL && frame == 30;
            dispatch(f.gate.update(f.input), frame); f.input.take = f.input.activate = false;
            if (committed) break;
            const bool reserved = f.gate.reservesImage(f.input);
            if (reserved) ++reservations;
            std::array<bool, MAX_CAPTURES> visible{}, preview{};
            std::array<F32, MAX_CAPTURES> demand{}, entitlement{};
            visible[0] = true; preview[1] = f.gate.wantsImage(f.input);
            const CaptureRateSettings automatic_rate;
            demand[0] = gateProducerDemand(automatic_rate, 10.f);
            demand[1] = 0.f; // UI preview warmup/throttling cannot starve transport.
            gateMediaCadence(visible, preview, demand, 10.f, 1,
                gateProducerDemand(automatic_rate, 10.f), entitlement);
            ensure_equals("Program priority retains HEAD Automatic cadence", entitlement[0], 10.f);
            ensure_equals("warming receives its normal max-min fair share, even outside the lead", entitlement[1], 5.f);
            if (f.input.now + 1e-9 >= baseline_due) { ++baseline_copies; baseline_due = f.input.now + .1; }
            if (budget.mediaDue(entitlement[0], f.input.now + 1e-9, program_due, false))
            { ++program_copies; program_due = f.input.now + .1; ++f.input.sources[0].frame; }
            ensure_equals("every ordinary HEAD Program copy executes", program_copies, baseline_copies);
            ensure(budget.cameraAvailable(frame));
            // Image completion and image copy are separate; no test frame may
            // manufacture an image merely because a copy reservation exists.
            if (decoded_roundtrips > cold_copies && budget.mediaDue(entitlement[1], f.input.now + 1e-9, cold_due, reserved))
            {
                cold_copies = decoded_roundtrips; ++cold.frame; cold.ready = true;
                cold.frameAge = 0.;
                if (f.input.now + 1e-9 >= cold_due) cold_due = f.input.now + 1. / entitlement[1];
            }
            f.input.dt = 0.; dispatch(f.gate.update(f.input), frame);
        }
        ensure(llformat("video B commits while video A stays live mode=%d phase=%d program=%s rounds=%u copies=%u seek=%d park=%d resume=%d now=%.4f reason=%s", int(mode), int(f.gate.phase()), f.gate.program().mId.asString().c_str(), decoded_roundtrips, cold_copies, saw_seek, saw_park, saw_resume, f.input.now, f.gate.reason().c_str()), committed);
        ensure("fast transport stays within the 300ms reservation lead", reservations <= 19u);
    }
}
template<> template<> void prism_gate_test_object::test<57>()
{
    using namespace LLPrismLens;
    set_test_name("video lead is at least 300ms; camera lead and ordinary media cadence remain HEAD");
    CaptureRateSettings rate;
    ensure_equals("default Automatic producer demand ignores UI preview rates", gateProducerDemand(rate, 10.f), 10.f);
    rate.mMode = EOutputRateMode::TARGET_FPS; rate.mTargetFps = 2.f;
    ensure_equals(gateProducerDemand(rate, 10.f), 2.f);
    for (F64 dt : {1. / 240., 1. / 60., .1}) for (U32 frames : {0u, 1u, 3u, 5u})
    {
        ensure(approxNear(gateLeadSeconds(frames, dt, true), std::max(.3, std::max(1u, frames) * dt)));
        ensure(approxNear(gateLeadSeconds(frames, dt, false), std::max(1u, frames) * dt));
    }
    std::array<bool, MAX_CAPTURES> visible{}, preview{};
    std::array<F32, MAX_CAPTURES> demand{}, entitlement{}, head{}, fair{};
    visible[0] = visible[2] = true; preview[1] = preview[3] = true;
    demand[0] = demand[2] = 60.f; demand[1] = demand[3] = 5.f;
    for (F32 capacity : {1.f, 2.f, 10.f, 30.f, 60.f})
    {
        gateCameraCadence(visible, preview, demand, capacity, -1, -1, false, head);
        gateMediaCadence(visible, preview, demand, capacity, -1, 0.f, entitlement);
        ensure(entitlement == head);
        auto active = visible; active[1] = true; fair.fill(0.f);
        LLPrismPreviewPolicy::waterFillTier(active, demand, capacity, fair);
        gateMediaCadence(visible, preview, demand, capacity, 1, demand[1], entitlement);
        for (U32 i = 0; i < MAX_CAPTURES; ++i)
            ensure_equals("only candidate gains its fair share; every ordinary producer retains HEAD", entitlement[i], i == 1 ? std::max(head[i], fair[i]) : head[i]);
        ensure(entitlement[1] > 0.f);
        for (F32 preview_request : {0.f, 1.f, 5.f})
        {
            demand[1] = preview_request;
            gateCameraCadence(visible, preview, demand, capacity, -1, -1, false, head);
            auto fair_demand = demand; fair_demand[1] = capacity; fair.fill(0.f);
            LLPrismPreviewPolicy::waterFillTier(active, fair_demand, capacity, fair);
            gateMediaCadence(visible, preview, demand, capacity, 1, capacity, entitlement);
            for (U32 i = 0; i < MAX_CAPTURES; ++i)
                ensure_equals("preview throttling cannot change ordinary shares or starve the fair video lane", entitlement[i], i == 1 ? std::max(head[i], fair[i]) : head[i]);
        }
        demand[1] = 5.f;
    }
}
namespace
{
void latencyContribution(LLPrismLens::EMediaOnCut policy, bool automatic, bool late,
    F64 resume_delay = 0., bool delay_publication = false)
{
    using namespace LLPrismLens;
    CycleFixture f;
    f.input.sources.resize(2); f.input.settings.mArmed.resize(2);
    f.input.settings.mMode = EGateMode::MANUAL;
    f.input.settings.mProgramArmIndex = 1; f.input.settings.mPreviewArmIndex = -1;
    auto& video = f.input.sources[1];
    video.kind = ESourceSlotKind::MEDIA_URL; video.onCut = policy;
    video.error = .05;
    f.input.activate = true;
    if (resume_delay == .4)
    {
        // Establish a measured 200ms RESUME estimate before retiring the
        // bookmarked player. Startup delay itself does not advance playback.
        for (U32 step = 0; step < 20 && f.gate.program().mId.isNull(); ++step)
            for (const auto& a : f.tick()) if (a.kind == EGateAction::RESUME)
            {
                video.position -= .2;
                f.tick(.2);
            }
    }
    else f.settle(20);
    ensure(f.gate.program().mId == video.handle.mId);
    // A real retirement ACK supplies the nonzero Resume/Live bookmark.
    video.position = 4.; video.pauseTime = f.input.now;
    const F64 off = video.pauseTime, bookmark = video.position;
    f.input.settings.mPreviewArmIndex = 0; f.input.take = true;
    f.settle(15);
    ensure(f.gate.program().mId == f.input.sources[0].handle.mId);
    f.input.settings.mPreviewArmIndex = 1;
    f.input.settings.mMode = automatic ? EGateMode::AUTO_CYCLE : EGateMode::MANUAL;
    video.ready = false; video.metadataReady = !late;
    f.input.accepted = 0;
    const F64 start = f.input.now;
    // Camera cut was accepted on the second fixture tick after Take.
    const F64 boundary = start - .13 + f.input.settings.mIntervalSeconds;
    GateAction command{EGateAction::CLAIM, {}};
    F64 receipt = -1., image = -1., copy_due = start, decoded = -1.;
    bool committed = false, post_boundary_reservation = false;
    F64 first_target = 0.;
    U32 seeks = 0, resumes = 0, copies = 0;
    GateFrameBudget budget;
    for (U32 frame = 0; frame < 240 && !committed; ++frame)
    {
        f.input.now = start + frame / 60.; f.input.dt = f.input.frameDt = 1. / 60.;
        video.frameAge += f.input.dt; video.sampleAge = 0.;
        if (video.resumed) video.position += f.input.dt;
        if (late && f.input.now >= boundary + .1) video.metadataReady = true;
        if (receipt >= 0. && f.input.now + 1e-9 >= receipt)
        {
            video.token = command.token; video.position = command.position;
            video.paused = command.kind != EGateAction::RESUME;
            video.resumed = !video.paused; receipt = -1.;
        }
        if (image >= 0. && f.input.now + 1e-9 >= image) { decoded = image; image = -1.; }
        f.input.take = !automatic && frame == 0;
        const auto actions = f.gate.update(f.input); f.input.take = false;
        for (const auto& a : actions)
        {
            if (a.handle.mId != video.handle.mId) continue;
            if (a.kind == EGateAction::SEEK || a.kind == EGateAction::PARK || a.kind == EGateAction::RESUME)
            {
                command = a;
                // SEEK -> parked image 100ms, PARK ACK next frame, RESUME ->
                // image 150ms (Live) or 200ms (Restart/Resume). Startup is async.
                const F64 delay = a.kind == EGateAction::RESUME ?
                    (resume_delay > 0. ? resume_delay : policy == EMediaOnCut::LIVE ? .15 : .2) : .1;
                receipt = f.input.now + (a.kind == EGateAction::RESUME ? delay : 1. / 60.);
                if (a.kind != EGateAction::PARK) image = f.input.now + delay;
                decoded = -1.;
                if (a.kind == EGateAction::SEEK)
                {
                    ++seeks; video.resumed = false;
                    if (seeks == 1) first_target = a.position;
                    if (resume_delay == .4 && seeks == 2)
                    {
                        const auto live = gatePosition(policy, true, bookmark, off, f.input.now,
                            video.duration, video.loop, video.framePeriod);
                        ensure("first late image satisfies frozen target but exceeds current Live tolerance",
                            gatePositionDistance(video.position, first_target, video.duration, video.loop) + video.error <= .15 &&
                            gatePositionDistance(video.position, live.seconds, video.duration, video.loop) + video.error > .15);
                    }
                }
                if (a.kind == EGateAction::RESUME)
                {
                    ++resumes;
                    if (resume_delay == .4 && resumes == 1)
                        ensure("scheduled RESUME uses the measured 200ms estimate",
                            std::abs(f.input.now - (boundary - .2)) <= 1. / 60. + 1e-9);
                }
            }
            if (a.kind == EGateAction::PROPOSE)
            {
                if (resume_delay == .4)
                    ensure("stale first scheduled image requires exactly one corrective re-seek", seeks == 2 && resumes == 2);
                ensure("first contribution retains the 150ms target bound",
                    gatePositionDistance(video.position, f.gate.targetPosition(), video.duration, video.loop) + video.error <= .150001);
                if (policy == EMediaOnCut::LIVE)
                {
                    const auto live = gatePosition(policy, true, bookmark, off, f.input.now,
                        video.duration, video.loop, video.framePeriod);
                    ensure("actual Live contribution is within 150ms including clock error",
                        gatePositionDistance(video.position, live.seconds, video.duration, video.loop) + video.error <= .150001);
                }
                if (delay_publication)
                {
                    const auto program = f.gate.program();
                    // The proposed image still matches its frozen seek target,
                    // but the Live clock advances during output preparation.
                    f.input.now += .2;
                    ensure("final publication rejects an off-clock Live image", !f.gate.validatePublication(a.token, f.input));
                    ensure(f.gate.acknowledgeOutputs(a.token, f.input).empty());
                    ensure(f.gate.program().mId == program.mId);
                    const auto correction = f.gate.update(f.input);
                    ensure("publication failure also triggers the single corrective seek",
                        std::count_if(correction.begin(), correction.end(), [](const auto& action)
                            { return action.kind == EGateAction::SEEK; }) == 1);
                    return;
                }
                f.gate.acknowledgeOutputs(a.token, f.input);
                committed = f.gate.program().mId == video.handle.mId;
            }
        }
        const bool reserved = f.gate.reservesImage(f.input);
        if (reserved && f.input.now > boundary) post_boundary_reservation = true;
        // Production fair-share adapter: two Automatic producers at 30Hz give
        // the candidate 15Hz. Reservations do not consume the Program lane.
        std::array<bool, MAX_CAPTURES> visible{}, preview{};
        std::array<F32, MAX_CAPTURES> demand{}, entitlement{};
        visible[0] = true; preview[1] = f.gate.wantsImage(f.input);
        demand[0] = gateProducerDemand(CaptureRateSettings(), 30.f);
        gateMediaCadence(visible, preview, demand, 30.f, 1, 30.f, entitlement);
        ensure_equals(entitlement[0], 30.f);
        if (preview[1]) ensure_equals(entitlement[1], 15.f);
        if (decoded >= 0. && budget.mediaDue(entitlement[1], f.input.now + 1e-9, copy_due, reserved))
        {
            ++video.frame; ++copies; video.ready = true; video.frameAge = 0.; decoded = -1.;
            if (f.input.now + 1e-9 >= copy_due) copy_due = f.input.now + 1. / 15.;
        }
    }
    ensure(llformat("async contribution policy=%d automatic=%d late=%d seeks=%u resumes=%u copies=%u reason=%s",
        int(policy), automatic, late, seeks, resumes, copies, f.gate.reason().c_str()), committed);
    ensure("one corrective seek at most", seeks <= 2 && resumes >= 1);
    if (late) ensure("transport reservation survives the boundary", post_boundary_reservation);
}
}

template<> template<> void prism_gate_test_object::test<58>()
{
    set_test_name("Manual bookmarked Live Take and late Auto survive 250ms seek/resume latency");
    latencyContribution(LLPrismLens::EMediaOnCut::LIVE, false, false);
    latencyContribution(LLPrismLens::EMediaOnCut::LIVE, true, true);
}

template<> template<> void prism_gate_test_object::test<59>()
{
    set_test_name("scheduled Restart and Resume commit with 200ms resume latency and 15Hz fair copies");
    latencyContribution(LLPrismLens::EMediaOnCut::RESTART, true, false);
    latencyContribution(LLPrismLens::EMediaOnCut::RESUME, true, false);
}

template<> template<> void prism_gate_test_object::test<60>()
{
    using namespace LLPrismLens;
    set_test_name("per-player latency EMA is conservative, bounded, finite and independent");
    GateLatency a, b;
    ensure_equals(a.seek, .2); ensure_equals(a.resume, .2);
    a.resume = GateLatency::sample(a.resume, .4);
    ensure(approxNear(a.resume, .35)); ensure_equals(a.seek, .2); ensure_equals(b.resume, .2);
    for (U32 i = 0; i < 100; ++i)
    {
        a.resume = GateLatency::sample(a.resume, 100.);
        a.seek = GateLatency::sample(a.seek, 0.);
        ensure(a.resume >= .05 && a.resume <= .6 && a.seek >= .05 && a.seek <= .6);
    }
    ensure(approxNear(a.resume, .6) && approxNear(a.seek, .05));
    for (F64 bad : {-1., std::numeric_limits<F64>::infinity(), std::numeric_limits<F64>::quiet_NaN()})
        ensure_equals(GateLatency::sample(.2, bad), .2);
}

template<> template<> void prism_gate_test_object::test<61>()
{
    using namespace LLPrismLens;
    set_test_name("missing post-resume copy retains only the fixed pre-roll plus overrun reservation");
    CycleFixture f; f.input.settings.mMode = EGateMode::MANUAL;
    f.input.settings.mPreviewArmIndex = 1;
    f.input.sources[1].kind = ESourceSlotKind::MEDIA_URL;
    f.input.sources[1].onCut = EMediaOnCut::RESTART;
    f.input.activate = true; f.settle(20);
    const auto program = f.gate.program();
    const F64 take = f.input.now + .01;
    f.input.take = true;
    bool resumed = false;
    for (U32 i = 0; i < 15 && !resumed; ++i)
        for (const auto& a : f.tick()) if (a.kind == EGateAction::RESUME) resumed = true;
    ensure(resumed);
    // tick increments the image serial before dispatching RESUME: undo that
    // fixture copy so only the receipt is visible, with no post-resume image.
    --f.input.sources[1].frame;
    f.input.now = take + .4; f.input.dt = 0.;
    f.gate.update(f.input);
    ensure("reservation survives the former boundary expiry", f.gate.reservesImage(f.input));
    f.input.now = take + 2.301;
    ensure("fixed reservation cannot slide with a late receipt", !f.gate.reservesImage(f.input));
    f.gate.update(f.input);
    ensure(f.gate.program().mId == program.mId);
    ensure(f.gate.reason() == "Take timed out");
}

template<> template<> void prism_gate_test_object::test<62>()
{
    set_test_name("scheduled bookmarked Live cut retains the 150ms contribution tolerance");
    latencyContribution(LLPrismLens::EMediaOnCut::LIVE, true, false);
}

template<> template<> void prism_gate_test_object::test<63>()
{
    set_test_name("scheduled Live corrects 400ms actual resume against 200ms estimate before publishing");
    latencyContribution(LLPrismLens::EMediaOnCut::LIVE, true, false, .4);
}

template<> template<> void prism_gate_test_object::test<64>()
{
    set_test_name("scheduled Live revalidates its current clock at final publication and corrects once");
    latencyContribution(LLPrismLens::EMediaOnCut::LIVE, true, false, .15, true);
}
} // namespace tut
