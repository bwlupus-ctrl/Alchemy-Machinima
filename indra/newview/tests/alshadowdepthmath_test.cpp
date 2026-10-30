#include "linden_common.h"
#include "../test/lltut.h"
#include "../alshadowdepthmath.h"
#include <limits>

namespace tut
{
struct shadow_depth_data {};
using shadow_depth_group = test_group<shadow_depth_data>;
using shadow_depth_object = shadow_depth_group::object;
shadow_depth_group shadow_depth_tests("alshadowdepthmath");

template<> template<> void shadow_depth_object::test<1>()
{
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();
    ensure_equals("NaN is off even with fast math",ALShadowDepth::amount(nan),0.f);
    ensure_equals("infinity is off",ALShadowDepth::amount(inf),0.f);
    ensure_equals("negative clamps",ALShadowDepth::amount(-1.f),0.f);
    ensure_equals("positive clamps",ALShadowDepth::amount(2.f),1.f);
    ensure_equals("depth zero never touches poisoned inputs",ALShadowDepth::fillFactor(0.f,nullptr,nullptr,8,nan),1.f);
    const float vis[] = {1.f,1.f}, energy[] = {10.f,10.f};
    ensure_equals("visible is identity",ALShadowDepth::fillFactor(1.f,vis,energy,2,.1f),1.f);
    ensure_equals("invalid energy ignored",ALShadowDepth::weight(nan,1.f),0.f);
}

template<> template<> void shadow_depth_object::test<2>()
{
    const float vis[] = {0.f,.5f}, energy[] = {2.f,8.f};
    const float expected = std::max(ALShadowDepth::weight(2.f,1.f),.5f*ALShadowDepth::weight(8.f,1.f));
    ensure_equals("overlap uses max, never sum",ALShadowDepth::occlusion(vis,energy,2,1.f),expected);
    ensure("fill suppresses darkening",ALShadowDepth::fillFactor(.3f,vis,energy,2,1000000.f) > .99999f);
    ensure("night darkens",ALShadowDepth::fillFactor(.3f,vis,energy,2,0.f) < .701f);
    ensure("bounded by authored depth",ALShadowDepth::fillFactor(.3f,vis,energy,2,0.f) >= .7f);
}

template<> template<> void shadow_depth_object::test<3>()
{
    using namespace ALShadowDepth;
    const std::array<float,4> zero = {};
    ensure("switch OFF is exact zeros",farReachUniform(false,24.f,8.f) == zero);
    ensure("degenerate is exact zeros",farReachUniform(true,8.f,8.f) == zero);
    ensure("NaN is exact zeros",farReachUniform(true,std::numeric_limits<float>::quiet_NaN(),8.f) == zero);
    ensure("infinite distance is exact zeros",farReachUniform(true,24.f,std::numeric_limits<float>::infinity()) == zero);
    const auto u = farReachUniform(true,24.f,8.f);
    ensure_equals("default reach",u[0],24.f);
    ensure_equals("near ramp",u[1],12.f);
    ensure_equals("far ramp",u[2],36.f);
    ensure("thickness sqrt three",std::abs(u[3]-1.7320508f) < .000001f);
    for (float z : {-10.f,0.f,11.f,12.f})
        ensure("near is exact identity",farTrace(z,u) == std::array<float,2>{8.f,1.f});
    auto previous = farTrace(12.f,u);
    for (float z=13.f; z<=60.f; z+=1.f)
    {
        const auto next = farTrace(z,u);
        ensure("monotone distance and thickness",next[0] >= previous[0] && next[1] >= previous[1]);
        ensure("bounded reach",next[0] <= 24.f && next[1] <= u[3]);
        previous = next;
    }
    ensure("zero uniform leaves far pixels alone",farTrace(100.f,zero) == std::array<float,2>{8.f,1.f});
}
}
