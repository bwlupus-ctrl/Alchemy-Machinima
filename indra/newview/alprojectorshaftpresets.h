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
// Applies a built-in look. Accepts a "b:*" id or a legacy "0".."7" id. An unknown id writes nothing
// and returns false.
bool apply(const std::string& id);
// Applies a saved user look: reset the owned set, then the map's values (missing keys keep the
// default). The Dust permutation is written once, last, and only if it changes.
bool applyValues(const LLSD& values);
// Owned set plus the Dust permutation: what a user look saves and what the strip tracks.
const std::vector<std::string>& trackedSettings();
}

#endif // AL_PROJECTOR_SHAFT_PRESETS_H
