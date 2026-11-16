/**
 * @file alclonetransition.cpp
 * @brief Compiles the pure clone transition maths and lifecycle state machine (alclonetransition.h)
 *        standalone, so the unit test (tests/alclonetransition_test.cpp) links the production header.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */
#include "linden_common.h"

#include "alclonetransition.h"

namespace ALCloneTransition
{
static_assert(MAX_RUNNING == 2, "D20: at most two running transitions");
static_assert(ST_RUNNING < ST_DONE, "state order: a record leaves RUNNING only forward");
}
