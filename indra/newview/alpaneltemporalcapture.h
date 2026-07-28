/**
 * @file alpaneltemporalcapture.h
 * @brief Temporal Capture transport panel: World Time Scale mode, the world-speed
 *        slider + presets, the per-subsystem Drive checkboxes, and the effective /
 *        recommended-post-speed readouts.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 *
 * Landing 1 of Temporal Capture. See doc/TEMPORAL_CAPTURE_WORLD_TIME_SCALE_BRIEF.md.
 *
 * The SAME panel is embedded by the standalone Temporal Capture floater and the
 * Director Console Temporal tab (superset rule), so the two hosts render the
 * identical transport from one source and can never drift. Every parameter is
 * settings-backed (Temporal*), so scene-save and multiple live instances stay in
 * lockstep through gSavedSettings with no fork -- exactly like ALPanelActorMover.
 */

#ifndef AL_ALPANELTEMPORALCAPTURE_H
#define AL_ALPANELTEMPORALCAPTURE_H

#include "llpanel.h"

class LLButton;
class LLTextBox;

class ALPanelTemporalCapture final : public LLPanel
{
public:
    ALPanelTemporalCapture() = default;
    ~ALPanelTemporalCapture() override = default;

    bool postBuild() override;
    void draw() override;

private:
    // preset shortcut: set the world scale and (manual) engage Manual mode, or
    // (Realtime) return to Live.
    void onClickScalePreset(F32 scale, bool manual);
    // touching the speed while Live implies the user wants Manual scaling
    void onScaleCommit();

    LLTextBox* mEffectiveText = nullptr;
    LLTextBox* mPostSpeedText = nullptr;
    LLTextBox* mStatusText = nullptr;

    // draw()-rate diffing so text is re-set only when it changes
    std::string mLastEffective;
    std::string mLastPost;
    std::string mLastStatus;
};

#endif // AL_ALPANELTEMPORALCAPTURE_H
