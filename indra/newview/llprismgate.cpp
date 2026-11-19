/**
 * @file llprismgate.cpp
 * @brief Pure Vcam Gate router and persistence helpers.
 */

#include "linden_common.h"

#include "llprismgate.h"

#include <algorithm>
#include <cmath>
#include <set>

namespace
{
constexpr U32 MAX_GATE_ARMS = LLPrismLens::MAX_CAPTURES;
constexpr U32 MAX_GATE_PREWARM_FRAMES = 5;
constexpr F64 MIN_GATE_INTERVAL_SECONDS = 0.5;
constexpr F64 MAX_GATE_INTERVAL_SECONDS = 120.0;

bool isNumeric(const LLSD& value)
{
    return value.isInteger() || value.isReal();
}

bool containsCapture(const std::vector<LLUUID>& captures, const LLUUID& id)
{
    return id.notNull() &&
        std::find(captures.begin(), captures.end(), id) != captures.end();
}
} // anonymous namespace

namespace LLPrismLens
{
S32 gateNextEnabledIndex(const std::vector<GateArmedCamera>& armed,
                         S32 current_index)
{
    if (armed.empty()) return -1;
    const S32 count = static_cast<S32>(armed.size());
    for (S32 offset = 1; offset <= count; ++offset)
    {
        S32 candidate = (current_index + offset) % count;
        if (candidate < 0) candidate += count;
        if (armed[static_cast<std::size_t>(candidate)].mEnabled)
            return candidate;
    }
    return -1;
}

S32 gateClampEnabledIndex(const std::vector<GateArmedCamera>& armed,
                          S32 preferred_index)
{
    if (armed.empty()) return -1;
    const S32 count = static_cast<S32>(armed.size());
    const S32 preferred = llclamp(preferred_index, 0, count - 1);
    if (armed[static_cast<std::size_t>(preferred)].mEnabled) return preferred;
    return gateNextEnabledIndex(armed, preferred - 1);
}

S32 gateResolveArmIndex(const std::vector<GateArmedCamera>& armed,
                        S32 preferred_index,
                        const std::vector<LLUUID>& live_capture_ids)
{
    if (armed.empty()) return -1;
    const S32 count = static_cast<S32>(armed.size());
    const bool preferred_valid = preferred_index >= 0 && preferred_index < count;
    if (preferred_valid && containsCapture(
            live_capture_ids,
            armed[static_cast<std::size_t>(preferred_index)].mCaptureId))
    {
        return preferred_index;
    }

    const S32 start = preferred_valid ? preferred_index : -1;
    for (S32 offset = 1; offset <= count; ++offset)
    {
        const S32 candidate = (start + offset) % count;
        const GateArmedCamera& arm =
            armed[static_cast<std::size_t>(candidate)];
        if (arm.mEnabled && containsCapture(live_capture_ids, arm.mCaptureId))
            return candidate;
    }
    return -1;
}

void gateFilterResolvableArms(GateSettings& settings,
                              const std::vector<LLUUID>& live_capture_ids)
{
    const std::vector<GateArmedCamera> stored = settings.mArmed;
    std::vector<S32> index_map(stored.size(), -1);
    settings.mArmed.clear();
    for (std::size_t index = 0; index < stored.size(); ++index)
    {
        if (!containsCapture(live_capture_ids, stored[index].mCaptureId)) continue;
        index_map[index] = static_cast<S32>(settings.mArmed.size());
        settings.mArmed.push_back(stored[index]);
    }
    const auto remap = [&index_map](S32 stored_index)
    {
        return stored_index >= 0 &&
                stored_index < static_cast<S32>(index_map.size())
            ? index_map[static_cast<std::size_t>(stored_index)] : -1;
    };
    settings.mProgramArmIndex = remap(settings.mProgramArmIndex);
    settings.mPreviewArmIndex = remap(settings.mPreviewArmIndex);
    if (settings.mProgramArmIndex < 0)
    {
        settings.mProgramArmIndex = gateClampEnabledIndex(settings.mArmed, 0);
        if (settings.mProgramArmIndex < 0 && !settings.mArmed.empty())
            settings.mProgramArmIndex = 0;
    }
    if (settings.mArmed.empty()) settings.mActive = false;
}

F64 gateNextCutTime(F64 past_boundary, F64 interval_seconds)
{
    if (!std::isfinite(past_boundary) || !std::isfinite(interval_seconds) ||
        interval_seconds <= 0.0)
    {
        return 0.0;
    }
    return past_boundary + interval_seconds;
}

bool gateWarmActive(F64 now, F64 next_cut_time, U32 prewarm_frames,
                    F64 frame_dt)
{
    return prewarm_frames > 0 && std::isfinite(now) &&
        std::isfinite(next_cut_time) && next_cut_time > 0.0 &&
        std::isfinite(frame_dt) && frame_dt > 0.0 &&
        now >= next_cut_time - static_cast<F64>(prewarm_frames) * frame_dt;
}

bool gateWarmSeedValid(U32 width, U32 height)
{
    return width > 0 && height > 0;
}

bool gateConsumePreviewWarmFrame(U32& frames_remaining)
{
    if (frames_remaining == 0) return false;
    --frames_remaining;
    return true;
}

bool gateIsRealCut(S32 current_index, S32 candidate_index)
{
    return current_index >= 0 && candidate_index >= 0 &&
        current_index != candidate_index;
}

ERegistryResult gateFeedbackResult(const LLUUID& camera_source_object_id,
                                   const LLUUID& display_object_id,
                                   bool display_gate_subscribed)
{
    return display_gate_subscribed && camera_source_object_id.notNull() &&
            camera_source_object_id == display_object_id
        ? ERegistryResult::INVALID_CONFIGURATION
        : ERegistryResult::OK;
}

bool gateDisplayCaptureReferenceAccepted(bool gate_source,
                                         bool capture_id_resolves)
{
    return gate_source || capture_id_resolves;
}

void gateRouteDisplayBinding(S32 capture_slot, U64 capture_generation,
                             U32& display_slot, U64& display_generation)
{
    if (capture_slot < 0 ||
        capture_slot >= static_cast<S32>(MAX_CAPTURES))
    {
        display_slot = MAX_CAPTURES;
        display_generation = 0;
        return;
    }
    display_slot = static_cast<U32>(capture_slot);
    display_generation = capture_generation;
}

LLSD gateSettingsToLLSD(const GateSettings& settings)
{
    LLSD gate = LLSD::emptyMap();
    gate["gate_id"] = settings.mGateId;
    gate["active"] = settings.mActive;
    gate["mode"] = settings.mMode == EGateMode::AUTO_CYCLE
        ? "auto_cycle" : "manual";
    gate["interval_seconds"] = settings.mIntervalSeconds;
    gate["prewarm_frames"] = static_cast<S32>(settings.mPrewarmFrames);
    gate["program_arm_index"] = settings.mProgramArmIndex;
    gate["preview_arm_index"] = settings.mPreviewArmIndex;
    gate["armed"] = LLSD::emptyArray();
    for (const GateArmedCamera& armed : settings.mArmed)
    {
        LLSD item = LLSD::emptyMap();
        item["arm_id"] = armed.mArmId;
        item["label"] = armed.mLabel;
        item["enabled"] = armed.mEnabled;
        item["capture_id"] = armed.mCaptureId;
        gate["armed"].append(item);
    }
    return gate;
}

bool gateSettingsFromLLSD(const LLSD& data, GateSettings& settings,
                          std::string* reason)
{
    const auto fail = [reason](const std::string& message)
    {
        if (reason) *reason = message;
        return false;
    };
    if (!data.isMap() || !data.has("gate_id") || !data.has("active") ||
        !data.has("mode") || !data.has("interval_seconds") ||
        !data.has("prewarm_frames") || !data.has("program_arm_index") ||
        !data.has("armed") || !data["armed"].isArray() ||
        !isNumeric(data["interval_seconds"]) ||
        !data["prewarm_frames"].isInteger() ||
        !data["program_arm_index"].isInteger())
    {
        return fail("A Prism Gate is missing required fields.");
    }

    GateSettings parsed;
    parsed.mGateId = data["gate_id"].asUUID();
    if (parsed.mGateId.isNull()) return fail("A Prism Gate ID must be nonnull.");
    parsed.mActive = data["active"].asBoolean();
    const std::string mode = data["mode"].asString();
    if (mode == "manual") parsed.mMode = EGateMode::MANUAL;
    else if (mode == "auto_cycle") parsed.mMode = EGateMode::AUTO_CYCLE;
    else return fail("Unknown Prism Gate mode.");
    parsed.mIntervalSeconds = data["interval_seconds"].asReal();
    if (!std::isfinite(parsed.mIntervalSeconds) ||
        parsed.mIntervalSeconds < MIN_GATE_INTERVAL_SECONDS ||
        parsed.mIntervalSeconds > MAX_GATE_INTERVAL_SECONDS)
    {
        return fail("Prism Gate interval must be between 0.5 and 120 seconds.");
    }
    const S32 prewarm = data["prewarm_frames"].asInteger();
    if (prewarm < 0 || prewarm > static_cast<S32>(MAX_GATE_PREWARM_FRAMES))
        return fail("Prism Gate pre-warm must be between 0 and 5 frames.");
    parsed.mPrewarmFrames = static_cast<U32>(prewarm);
    parsed.mProgramArmIndex = data["program_arm_index"].asInteger();
    parsed.mPreviewArmIndex = data.has("preview_arm_index") &&
            data["preview_arm_index"].isInteger()
        ? data["preview_arm_index"].asInteger() : -1;

    std::set<LLUUID> arm_ids;
    std::set<LLUUID> capture_ids;
    std::vector<S32> index_map;
    const LLSD& armed_data = data["armed"];
    index_map.reserve(static_cast<std::size_t>(armed_data.size()));
    for (LLSD::array_const_iterator it = armed_data.beginArray();
         it != armed_data.endArray(); ++it)
    {
        const LLSD& item = *it;
        index_map.push_back(-1);
        if (parsed.mArmed.size() >= MAX_GATE_ARMS || !item.isMap() ||
            !item.has("arm_id") || !item.has("label") ||
            !item.has("enabled") || !item.has("capture_id") ||
            !item["label"].isString())
        {
            continue;
        }
        GateArmedCamera armed;
        armed.mArmId = item["arm_id"].asUUID();
        armed.mLabel = item["label"].asString();
        armed.mEnabled = item["enabled"].asBoolean();
        armed.mCaptureId = item["capture_id"].asUUID();
        if (armed.mArmId.isNull() || armed.mCaptureId.isNull() ||
            armed.mLabel.size() > 128 ||
            !arm_ids.insert(armed.mArmId).second ||
            !capture_ids.insert(armed.mCaptureId).second)
        {
            continue;
        }
        index_map.back() = static_cast<S32>(parsed.mArmed.size());
        parsed.mArmed.push_back(armed);
    }

    const auto remapIndex = [&index_map](S32 stored)
    {
        return stored >= 0 && stored < static_cast<S32>(index_map.size())
            ? index_map[static_cast<std::size_t>(stored)] : -1;
    };
    parsed.mProgramArmIndex = remapIndex(parsed.mProgramArmIndex);
    parsed.mPreviewArmIndex = remapIndex(parsed.mPreviewArmIndex);
    if (parsed.mProgramArmIndex < 0)
    {
        parsed.mProgramArmIndex = gateClampEnabledIndex(parsed.mArmed, 0);
        if (parsed.mProgramArmIndex < 0 && !parsed.mArmed.empty())
            parsed.mProgramArmIndex = 0;
    }
    if (parsed.mArmed.empty()) parsed.mActive = false;
    settings = parsed;
    if (reason) reason->clear();
    return true;
}

bool gateSettingsFromScene(const LLSD& scene, GateSettings& settings,
                           bool& present, std::string* reason)
{
    settings = GateSettings();
    present = false;
    if (!scene.isMap() || !scene.has("prism_gates"))
    {
        if (reason) reason->clear();
        return true;
    }
    const LLSD& gates = scene["prism_gates"];
    if (!gates.isArray())
    {
        if (reason) *reason =
            "Ignored unusable Prism Gate metadata; displays keep fixed feeds.";
        return true;
    }
    for (LLSD::array_const_iterator it = gates.beginArray();
         it != gates.endArray(); ++it)
    {
        GateSettings candidate;
        if (gateSettingsFromLLSD(*it, candidate, nullptr))
        {
            settings = candidate;
            present = true;
            break;
        }
    }
    if (reason) reason->clear();
    return true;
}
} // namespace LLPrismLens
