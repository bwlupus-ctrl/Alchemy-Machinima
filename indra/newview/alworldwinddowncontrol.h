/**
 * @file alworldwinddowncontrol.h
 * @brief Shared clock-write cancellation binding, testable with real controls.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */
#ifndef AL_ALWORLDWINDDOWNCONTROL_H
#define AL_ALWORLDWINDDOWNCONTROL_H

#include "llcontrol.h"
#include <functional>

namespace ALWorldWindDown
{
inline boost::signals2::connection observeClockWrites(
    LLControlVariable* control, std::function<void()> cancel)
{
    if (!control) return {};
    return control->getWriteSignal()->connect(
        [cancel](LLControlVariable*) { cancel(); });
}
}
#endif // AL_ALWORLDWINDDOWNCONTROL_H
