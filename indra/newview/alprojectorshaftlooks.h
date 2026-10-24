/**
 * @file alprojectorshaftlooks.h
 * @brief Projector-shaft look data: the 13 built-in looks, the owned setting list, legacy id mapping.
 *
 * Pure data and string logic. No llui, no gSavedSettings: the settings-touching layer is
 * alprojectorshaftpresets.{h,cpp}. This file is unit tested (tests/alprojectorshaftlooks_test.cpp).
 */

#ifndef AL_PROJECTOR_SHAFT_LOOKS_H
#define AL_PROJECTOR_SHAFT_LOOKS_H

#include "stdtypes.h"

#include <cstddef>
#include <string>
#include <vector>

namespace ALProjectorShaftLooks
{
enum class Type { F32, U32, BOOL, COLOR3 };

// One authored value. BOOL uses f[0] != 0; COLOR3 uses f[0..2]; the others use f[0].
struct Value
{
    const char* setting;
    Type        type;
    F32         f[3];
};

struct Look
{
    const char*  id;          // "b:" prefix
    const char*  label;       // <= 22 chars
    const char*  description; // <= 95 chars
    const Value* values;
    size_t       count;
};

const std::vector<Look>& looks();
const Look* findLook(const std::string& id);

// Settings a look owns: reset to the settings.xml default, then the look's values. Excludes
// dustSetting(), which is a shader permutation and is never put through a reset loop.
const std::vector<std::string>& ownedSettings();
// BDMergeProjectorVolumetricsDust. Owned and tracked, but written once, last, and only if it changes.
const char* dustSetting();

// "0".."7" are the built-in ids of the previous 8 presets; "b:*" ids map to themselves; anything
// else maps to "" (nothing to apply).
std::string canonicalLookId(const std::string& id);
// The old Lightbox combo integers (-1 = Custom, no-op).
std::string canonicalLookId(S32 legacy_combo_value);
}

#endif // AL_PROJECTOR_SHAFT_LOOKS_H
