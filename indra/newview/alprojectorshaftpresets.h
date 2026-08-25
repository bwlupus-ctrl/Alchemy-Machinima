/**
 * @file alprojectorshaftpresets.h
 * @brief Shared quick presets for the Lightbox/Director projector-shaft UI.
 */

#ifndef AL_PROJECTOR_SHAFT_PRESETS_H
#define AL_PROJECTOR_SHAFT_PRESETS_H

#include "stdtypes.h"

namespace ALProjectorShaftPresets
{
enum Preset : S32
{
    CUSTOM = -1,
    FAST_PREVIEW = 0,
    BALANCED,
    CINEMATIC,
    HAZY_STAGE,
};

void apply(S32 preset);
}

#endif // AL_PROJECTOR_SHAFT_PRESETS_H
