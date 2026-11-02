// Pure logic regression cases; syntax-checkable without building the viewer.
#include "linden_common.h"
#include "../test/lltut.h"
#include "../alghostbounds.h"
#include <limits>

namespace tut
{
struct ghost_bounds_data {};
typedef test_group<ghost_bounds_data> ghost_bounds_group;
typedef ghost_bounds_group::object ghost_bounds_object;
ghost_bounds_group ghost_bounds_tests("ALGhostBounds");

template<> template<> void ghost_bounds_object::test<1>()
{
    using namespace ALGhostBounds;
    Point center, size;
    ensure("seated extents valid", centerAndSize(
        {10.35f, 19.7f, .45f}, {11.25f, 20.3f, 1.85f}, center, size));
    ensure("visible center x", std::abs(center[0] - 10.8f) < 1e-5f);
    ensure("visible center z", std::abs(center[2] - 1.15f) < 1e-5f);
    ensure("displaced seated", displaced(false, true, center, {10.f, 20.f, 1.f}));
    ensure("non-seated displaced", displaced(false, false, center, {10.f, 20.f, 1.f}));
}

template<> template<> void ghost_bounds_object::test<2>()
{
    using namespace ALGhostBounds;
    const Point root{0.f, 0.f, 1.f};
    ensure("standing keeps historical box", !displaced(false, false, {.05f, 0.f, 1.f}, root));
    ensure("enter above 20cm", displaced(false, false, {.201f, 0.f, 1.f}, root));
    ensure("band retains fitted", displaced(true, false, {.18f, 0.f, 1.f}, root));
    ensure("band retains standing", !displaced(false, false, {.18f, 0.f, 1.f}, root));
    ensure("leave below 15cm", !displaced(true, false, {.149f, 0.f, 1.f}, root));
    ensure("vertical seated still fits", displaced(false, true, {0.f, 0.f, .3f}, root));
    ensure("vertical standing stays unchanged", !displaced(false, false, {0.f, 0.f, .3f}, root));
}

template<> template<> void ghost_bounds_object::test<3>()
{
    using namespace ALGhostBounds;
    Point center{42.f, 42.f, 42.f}, size = center;
    ensure("reject empty", !centerAndSize({0.f, 0.f, 0.f}, {0.f, 1.f, 1.f}, center, size));
    ensure("reject reversed", !centerAndSize({1.f, 0.f, 0.f}, {0.f, 1.f, 1.f}, center, size));
    ensure("reject NaN under fp:fast", !centerAndSize(
        {std::numeric_limits<float>::quiet_NaN(), 0.f, 0.f}, {1.f, 1.f, 1.f}, center, size));
    ensure("failure leaves outputs alone", center[0] == 42.f && size[0] == 42.f);
}
}
