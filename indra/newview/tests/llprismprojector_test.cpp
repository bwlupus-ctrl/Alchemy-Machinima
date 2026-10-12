/**
 * @file llprismprojector_test.cpp
 * @brief Pure VP1/VP2 projector contract regressions.
 */

#include "linden_common.h"

#include "../test/lltut.h"
#include "../llprismlens.h"

namespace tut
{
struct prism_projector_data {};
typedef test_group<prism_projector_data> prism_projector_group;
typedef prism_projector_group::object prism_projector_object;
prism_projector_group prism_projector_tests("LLPrismProjector");

template<> template<>
void prism_projector_object::test<1>()
{
    set_test_name("projector schema defaults are pipeline-neutral");
    LLPrismLens::ProjectorDefinition projector;
    ensure("object is the VP1 target kind",
           projector.mTarget == LLPrismLens::EProjectorTarget::OBJECT);
    ensure("default fit is FIT",
           projector.mSettings.mFitMode == LLPrismLens::EFitMode::FIT);
    ensure_equals("default prep resolution", projector.mSettings.mResolution,
                  static_cast<U16>(512));
    ensure("default no-signal preserves the object's cookie",
           projector.mSettings.mNoSignal ==
               LLPrismLens::ENoSignal::OWN_COOKIE);
    ensure("unresolved targets read OFFLINE",
           projector.mRuntime.mHealth == LLPrismLens::EDisplayHealth::OFFLINE);
}

template<> template<>
void prism_projector_object::test<2>()
{
    set_test_name("bootstrap demand expires after thirty frames");
    using LLPrismLens::ProjectorDemandPolicy::active;
    ensure("bind frame is immediately demanded", active(100, 0, 100, true));
    ensure("thirtieth bootstrap frame is demanded", active(129, 0, 100, true));
    ensure("bootstrap has no thirty-first-frame credit",
           !active(130, 0, 100, true));
    ensure("offline targets never bootstrap", !active(100, 0, 100, false));
}

template<> template<>
void prism_projector_object::test<3>()
{
    set_test_name("consumption demand stops within two frames");
    using LLPrismLens::ProjectorDemandPolicy::active;
    ensure("consumption frame is demanded", active(200, 200, 0, true));
    ensure("two-frame grace remains demanded", active(202, 200, 0, true));
    ensure("third frame is idle", !active(203, 200, 0, true));
}

template<> template<>
void prism_projector_object::test<4>()
{
    set_test_name("capacity and handle domains remain separate from displays");
    ensure_equals("projector capacity", LLPrismLens::MAX_PROJECTOR_BINDINGS, 8u);
    ensure("display capacity is untouched",
           LLPrismLens::MAX_DISPLAY_BINDINGS == 16u);
    LLPrismLens::ProjectorHandle projector;
    LLPrismLens::DisplayHandle display;
    ensure("new projector handle is stale", projector.mId.isNull() &&
           projector.mGeneration == 0);
    ensure("new display handle remains independent", display.mId.isNull() &&
           display.mGeneration == 0);
}

template<> template<>
void prism_projector_object::test<5>()
{
    set_test_name("empty registry snapshot hides projector state");
    LLPrismLens::RegistrySnapshot snapshot;
    ensure_equals("no projector rows", snapshot.mProjectorCount, 0u);
    ensure_equals("fixed projector storage", snapshot.mProjectors.size(),
                  static_cast<std::size_t>(LLPrismLens::MAX_PROJECTOR_BINDINGS));
}
}
