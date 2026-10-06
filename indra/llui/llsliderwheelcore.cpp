/**
 * @file llsliderwheelcore.cpp
 * @brief [SliderWheel] Pure logic of the viewer-wide slider mouse wheel. See llsliderwheelcore.h.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "linden_common.h"

#include "llsliderwheelcore.h"

namespace LLSliderWheel
{
Resolution resolve(EWheelAdjust state, bool vertical, bool switch_on)
{
    Resolution r;
    r.participates  = (state == WHEEL_ON) || (state == WHEEL_UNSET && switch_on);
    r.hwheelEnabled = (state != WHEEL_OFF);
    if (state == WHEEL_OFF)
    {
        r.wheelEnabled = false;
    }
    else if (r.participates)
    {
        r.wheelEnabled = true;
    }
    else
    {
        // UNSET with the switch off = stock: only vertical sliders take the wheel.
        r.wheelEnabled = vertical;
    }
    return r;
}

F32 nice125(F32 x)
{
    if (!(x >= 1.f)) // also catches NaN
    {
        return 1.f;
    }
    F64 p = 1.0;
    while (p * 10.0 <= (F64)x && p < 1.0e18)
    {
        p *= 10.0;
    }
    if ((F64)x >= 5.0 * p) return (F32)(5.0 * p);
    if ((F64)x >= 2.0 * p) return (F32)(2.0 * p);
    return (F32)p;
}

F32 plainStep(F32 min_value, F32 max_value, F32 increment)
{
    if (!(increment > 0.f))
    {
        return increment;
    }
    // Computed in double with a 1e-6 relative nudge, so a quotient that is exactly 2, 5 or 10
    // in real numbers is not pushed just below the threshold by F32 rounding of the increment.
    const F64 x = ((F64)max_value - (F64)min_value) / (100.0 * (F64)increment);
    return nice125((F32)(x * (1.0 + 1.0e-6))) * increment;
}

F32 wheelStep(F32 min_value, F32 max_value, F32 increment, bool legacy, bool shift, bool ctrl_alt)
{
    if (legacy)
    {
        return ctrl_alt ? increment * 10.f : increment;
    }
    if (shift)
    {
        return increment;
    }
    const F32 base = plainStep(min_value, max_value, increment);
    return ctrl_alt ? base * 10.f : base;
}

bool shouldSchedule(bool has_mouse_up_signal, bool captured, bool quitting)
{
    return has_mouse_up_signal && !captured && !quitting;
}

TickAction tickDecision(bool alive, bool gen_matches, bool pending, bool captured, F64 now, F64 deadline)
{
    if (!alive || !gen_matches || !pending) return TickAction::Drop;
    if (captured)                           return TickAction::Defer;
    if (now < deadline)                     return TickAction::Wait;
    return TickAction::Fire;
}
} // namespace LLSliderWheel
