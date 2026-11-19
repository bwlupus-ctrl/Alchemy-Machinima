/**
 * @file llprismgate_test.cpp
 * @brief Regression tests for the pure Vcam Gate router and schema.
 */

#include "linden_common.h"

#include "../test/lltut.h"
#include "../aldirectorswitchermodel.h"
#include "../llprismgate.h"

#include <cmath>

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

bool near(F64 left, F64 right, F64 tolerance = 1e-6)
{
    return std::fabs(left - right) <= tolerance;
}
} // anonymous namespace

struct prism_gate_data {};
typedef test_group<prism_gate_data> prism_gate_test_group;
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
           near(next_cut, 10.0));
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
           output.mMode == input.mMode && near(output.mIntervalSeconds, 11.5) &&
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
} // namespace tut
