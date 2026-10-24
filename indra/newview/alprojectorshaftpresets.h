/**
 * @file alprojectorshaftpresets.h
 * @brief Applies projector-shaft looks (and saved user looks) to the settings.
 *
 * The look data and ownership list live in alprojectorshaftlooks.{h,cpp} (pure, unit tested).
 */

#ifndef AL_PROJECTOR_SHAFT_PRESETS_H
#define AL_PROJECTOR_SHAFT_PRESETS_H

#include "llsd.h"
#include "stdtypes.h"

#include <string>
#include <vector>

namespace ALProjectorShaftPresets
{
bool classicEngine();
const std::vector<std::string>& classicLabels();
LLSD sceneData();
void applySceneData(const LLSD& data);

// Classic accepts its original "0".."7" ids. New accepts "b:*" and maps legacy "0".."7" ids.
// An unknown id writes nothing
// and returns false.
bool apply(const std::string& id);
// Applies a saved user look: reset the owned set, then the map's values (missing keys keep the
// default). The Dust permutation is written once, last, and only if it changes.
bool applyValues(const LLSD& values);
// Union of both engines' settings plus engine/id metadata for snapshots and strip watches.
const std::vector<std::string>& trackedSettings();
// All page controls for page history; look ownership stays narrower.
const std::vector<std::string>& pageSettings();
}

#endif // AL_PROJECTOR_SHAFT_PRESETS_H
