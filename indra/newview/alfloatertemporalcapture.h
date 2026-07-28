/**
 * @file alfloatertemporalcapture.h
 * @brief Standalone Temporal Capture floater (World Time Scale).
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 *
 * Landing 1 of Temporal Capture. A thin host: all controls live in the embedded
 * shared panel (ALPanelTemporalCapture, class "panel_temporal_capture"), the SAME
 * panel the Director Console Temporal tab hosts, so the standalone floater and the
 * console can never drift. See doc/TEMPORAL_CAPTURE_WORLD_TIME_SCALE_BRIEF.md.
 */

#ifndef AL_ALFLOATERTEMPORALCAPTURE_H
#define AL_ALFLOATERTEMPORALCAPTURE_H

#include "llfloater.h"

class ALFloaterTemporalCapture final : public LLFloater
{
public:
    ALFloaterTemporalCapture(const LLSD& key);
    ~ALFloaterTemporalCapture() override = default;

    bool postBuild() override;
};

#endif // AL_ALFLOATERTEMPORALCAPTURE_H
