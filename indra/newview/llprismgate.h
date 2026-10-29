/**
 * @file llprismgate.h
 * @brief Pure Vcam Gate sequence and persistence helpers.
 */

#ifndef LL_LLPRISMGATE_H
#define LL_LLPRISMGATE_H

#include "llprismlens.h"

namespace LLPrismLens
{
// The switcher Controller owns cut timing and the current slot; these helpers
// provide only the future sequence/boundary information its public API does not
// expose and the additive Gate persistence vocabulary.
S32 gateNextEnabledIndex(const std::vector<GateArmedCamera>& armed,
                         S32 current_index);
S32 gateClampEnabledIndex(const std::vector<GateArmedCamera>& armed,
                          S32 preferred_index);
S32 gateResolveArmIndex(const std::vector<GateArmedCamera>& armed,
                        S32 preferred_index,
                        const std::vector<LLUUID>& live_capture_ids);
void gateFilterResolvableArms(GateSettings& settings,
                              const std::vector<LLUUID>& live_capture_ids);
F64 gateNextCutTime(F64 past_boundary, F64 interval_seconds);
bool gateWarmActive(F64 now, F64 next_cut_time, U32 prewarm_frames,
                    F64 frame_dt);
bool gateWarmSeedValid(U32 width, U32 height);
bool gateConsumePreviewWarmFrame(U32& frames_remaining);
bool gateIsRealCut(S32 current_index, S32 candidate_index);
ERegistryResult gateFeedbackResult(const LLUUID& camera_source_object_id,
                                   const LLUUID& display_object_id,
                                   bool display_gate_subscribed);
bool gateDisplayCaptureReferenceAccepted(bool gate_source,
                                         bool capture_id_resolves);
void gateRouteDisplayBinding(S32 capture_slot, U64 capture_generation,
                             U32& display_slot, U64& display_generation);

// Malformed armed-camera entries are dropped while a valid outer Gate record
// remains loadable, matching the scene best-effort contract.
LLSD gateSettingsToLLSD(const GateSettings& settings);
bool gateSettingsFromLLSD(const LLSD& data, GateSettings& settings,
                          std::string* reason = nullptr);
bool gateSettingsFromScene(const LLSD& scene, GateSettings& settings,
                           bool& present, std::string* reason = nullptr);
} // namespace LLPrismLens

#endif // LL_LLPRISMGATE_H
