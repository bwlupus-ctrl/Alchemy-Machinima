/**
 * @file alvcamautovol_test.cpp
 * @brief Pure VCam volumetric ownership regressions, executable at compile time.
 */

#include "linden_common.h"
#include "../test/lltut.h"
#include "../alvcamautovol.h"

namespace
{
using ALVCamAutoVol::EnableOverride;

constexpr bool decisionMatrix()
{
    // Exercise pre-existing on/off, untouched/edited, final on/off, and one to
    // three bindings (including two bindings resolving to the same emitter).
    for (int initial = 0; initial != 2; ++initial)
    for (int edited = 0; edited != 2; ++edited)
    for (int final = 0; final != 2; ++final)
    for (unsigned users = 1; users != 4; ++users)
    {
        EnableOverride state;
        if (state.acquire(initial != 0) != (initial == 0)) return false;
        for (unsigned user = 1; user < users; ++user)
            if (state.acquire(true)) return false;
        if (state.mUsers != users) return false;
        if (edited) state.changed();
        for (unsigned user = 1; user < users; ++user)
            if (state.release(final != 0)) return false;
        const bool restore = !initial && !edited && final;
        if (state.release(final != 0) != restore) return false;
        if (state.mUsers != 0 || state.mAutoEnabled) return false;
    }
    return true;
}

constexpr bool userOffOnWins()
{
    EnableOverride state;
    if (!state.acquire(false)) return false;
    state.changed(); // user switches off
    state.changed(); // then back on, before any registry refresh
    return !state.release(true);
}

constexpr bool anotherBindingRespectsUserOff()
{
    EnableOverride state;
    if (!state.acquire(false)) return false;
    state.changed();
    return !state.acquire(false) && !state.release(false) &&
        !state.release(false) && state.mUsers == 0;
}

constexpr bool sharedMasterAndIndependentLights()
{
    EnableOverride first_light;
    EnableOverride second_light;
    EnableOverride master;
    if (!first_light.acquire(false) || !master.acquire(false)) return false;
    if (!second_light.acquire(false) || master.acquire(true)) return false;
    // Removing/reassigning the first output restores its light, but the master
    // must stay on until the second output is removed too.
    if (!first_light.release(true) || master.release(true)) return false;
    if (!second_light.release(true) || !master.release(true)) return false;
    return true;
}

constexpr bool manualChoicesAreIndependent()
{
    EnableOverride light;
    EnableOverride master;
    if (!light.acquire(false) || !master.acquire(false)) return false;
    light.changed(); // capturing a look also claims the per-light flag
    if (light.release(true) || !master.release(true)) return false;
    if (!light.acquire(false) || !master.acquire(false)) return false;
    master.changed(); // master off/on must not prevent restoring the light
    return light.release(true) && !master.release(true);
}

constexpr bool balancedReuse()
{
    EnableOverride state;
    if (state.release(true) || state.mUsers != 0) return false;
    if (!state.acquire(false) || !state.release(true)) return false;
    if (state.acquire(true) || state.release(true)) return false;
    return state.acquire(false) && state.release(true);
}

constexpr bool declinedSurvivesRebuild()
{
    EnableOverride master, emitter;
    if (!master.acquire(false) || !emitter.acquire(false)) return false;
    master.changed(false);
    emitter.changed(false);
    const bool binding_declined = emitter.mDeclined;
    if (emitter.release(false) || master.release(false)) return false;
    emitter = EnableOverride(); // region crossing replaces the emitter, not the binding
    if (emitter.acquire(false, !binding_declined) || master.acquire(false)) return false;
    if (!emitter.mDeclined || !master.mDeclined) return false;
    emitter.changed(true);
    master.changed(true);
    if (emitter.mDeclined || master.mDeclined) return false;
    if (emitter.release(true) || master.release(true)) return false;
    return emitter.acquire(false) && master.acquire(false);
}

constexpr bool automaticTeardownAllowsRebuild()
{
    EnableOverride emitter;
    if (!emitter.acquire(false)) return false;
    // Teardown removes the old flag without reporting a manual changed(false).
    const bool binding_declined = emitter.mDeclined;
    if (emitter.release(false)) return false;
    emitter = EnableOverride();
    return emitter.acquire(false, !binding_declined) && !emitter.mDeclined;
}

constexpr bool sharedLightDoesNotInheritBorrowerDecline()
{
    EnableOverride light;
    light.acquire(true);
    // A second projector's stale decline must not become this light's choice.
    if (light.acquire(false, false) || light.mDeclined) return false;
    light.release(false);
    light.release(false);
    return light.acquire(false);
}

constexpr bool sharedLightKeepsOwnDecline()
{
    EnableOverride light;
    light.acquire(false);
    light.changed(false);
    if (light.acquire(true) || !light.mDeclined) return false;
    light.release(false);
    light.release(false);
    return !light.acquire(false) && light.mDeclined;
}

// /Zs validates these regressions without a build or test executable.
static_assert(decisionMatrix(), "restore only untouched auto-enabled flags");
static_assert(userOffOnWins(), "an off/on round trip relinquishes ownership");
static_assert(anotherBindingRespectsUserOff(), "new borrowers respect edits");
static_assert(sharedMasterAndIndependentLights(), "master waits for last output");
static_assert(manualChoicesAreIndependent(), "light and master ownership differ");
static_assert(balancedReuse(), "release is balanced and ownership can be reused");
static_assert(declinedSurvivesRebuild(), "user off survives emitter rebuilds until manual on");
static_assert(automaticTeardownAllowsRebuild(), "automatic teardown is not a user decline");
static_assert(sharedLightDoesNotInheritBorrowerDecline(), "borrowers cannot decline another binding's light");
static_assert(sharedLightKeepsOwnDecline(), "borrowers cannot clear a light's decline");
} // anonymous namespace

namespace tut
{
struct alvcamautovol_data {};
typedef test_group<alvcamautovol_data> alvcamautovol_group;
typedef alvcamautovol_group::object alvcamautovol_object;
alvcamautovol_group alvcamautovol_tests("ALVCamAutoVol");

template<> template<>
void alvcamautovol_object::test<1>()
{
    set_test_name("auto-enable and conditional restore decision matrix");
    ensure("24 combinations", decisionMatrix());
}

template<> template<>
void alvcamautovol_object::test<2>()
{
    set_test_name("manual edits survive and are not re-enabled by borrowers");
    ensure("off/on round trip", userOffOnWins());
    ensure("additional binding after user off", anotherBindingRespectsUserOff());
}

template<> template<>
void alvcamautovol_object::test<3>()
{
    set_test_name("multiple lights share master ownership independently");
    ensure("last output restores master", sharedMasterAndIndependentLights());
    ensure("user owns only the setting they edited", manualChoicesAreIndependent());
}

template<> template<>
void alvcamautovol_object::test<4>()
{
    set_test_name("extra release and repeated assignment are safe");
    ensure("ownership reuse", balancedReuse());
    ensure("declined survives emitter replacement", declinedSurvivesRebuild());
    ensure("untouched binding re-enables replacement", automaticTeardownAllowsRebuild());
    ensure("two projectors do not transfer declines", sharedLightDoesNotInheritBorrowerDecline());
    ensure("two projectors respect this light's decline", sharedLightKeepsOwnDecline());
}
} // namespace tut
