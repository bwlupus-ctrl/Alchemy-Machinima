/**
 * @file alfloateractorgaze.h
 * @brief Standalone host for the shared Actor Gaze panel.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#ifndef AL_ALFLOATERACTORGAZE_H
#define AL_ALFLOATERACTORGAZE_H

#include "llfloater.h"
#include "lluuid.h"

class ALPanelLensGaze;

class ALFloaterActorGaze final : public LLFloater
{
public:
    enum class ESelectionSource
    {
        NONE,
        DIRECTOR,
        ACTOR_MOVER
    };

    ALFloaterActorGaze(const LLSD& key);
    ~ALFloaterActorGaze() override = default;

    bool postBuild() override;

    static void showForSelection(ESelectionSource source,
                                 const uuid_vec_t& actors);
    static void updateSelection(ESelectionSource source,
                                const uuid_vec_t& actors);

private:
    void takeSelection(ESelectionSource source, const uuid_vec_t& actors);

    ESelectionSource mSelectionSource = ESelectionSource::NONE;
    ALPanelLensGaze* mGazePanel = nullptr;
};

#endif // AL_ALFLOATERACTORGAZE_H
