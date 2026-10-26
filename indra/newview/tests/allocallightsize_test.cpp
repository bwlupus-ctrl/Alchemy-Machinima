// The identical checks can run without a viewer build:
// cl /std:c++17 /EHsc /DAL_LOCAL_LIGHT_SIZE_STANDALONE allocallightsize_test.cpp
#include "../allocallightsize.h"
#include <stdexcept>
#include <iostream>
#ifndef AL_LOCAL_LIGHT_SIZE_STANDALONE
#include "linden_common.h"
#include "../test/lltut.h"
#endif

namespace
{
void require(bool value, const char* message)
{
    if (!value) throw std::runtime_error(message);
}
void identity()
{
    for (double alpha : {0.0, .0016, .04, .25, 1.0})
        for (double distance : {0.0, 1.e-12, .1, 1.0})
            for (bool spot : {false, true})
            {
                require(ALLocalLightSize::ggxRatio(.6, alpha, 0.0, distance, spot) == 1.0, "zero GGX identity");
                const auto b = ALLocalLightSize::blinn(368.0, 0.0, distance, spot);
                require(b.exponent == 368.0 && b.normalization == 1.0, "zero Blinn identity");
            }
}
void peaks()
{
    for (double alpha : {.0016, .04, .25, 1.0})
        for (double distance : {.1, .3, 1.0})
            for (bool spot : {false, true})
            {
                double previous = 1.0;
                double previous_blinn = 1.0;
                const double original_peak = ALLocalLightSize::ggx(1.0, alpha);
                for (double radius : {0.0, .05, .15, .3, .5})
                {
                    const double peak = ALLocalLightSize::ggxRatio(1.0, alpha, radius, distance, spot);
                    require(peak <= previous + 1.e-12 && peak <= 1.0, "GGX peak must decrease");
                    previous = peak;
                    const auto b = ALLocalLightSize::blinn(368.0, radius, distance, spot);
                    require(b.normalization <= previous_blinn + 1.e-12, "Blinn peak must decrease");
                    previous_blinn = b.normalization;
                    for (int i = 0; i <= 1000; ++i)
                    {
                        const double nh = double(i) / 1000.0;
                        require(ALLocalLightSize::ggx(nh, alpha) * ALLocalLightSize::ggxRatio(nh, alpha, radius, distance, spot)
                            <= original_peak * (1.0 + 1.e-10), "widened lobe never exceeds original peak");
                    }
                }
                if (alpha < 1.0) require(previous < 1.0, "GGX radius must actually reduce the peak");
                require(previous_blinn < 1.0, "Blinn radius must actually reduce the peak");
            }
}
void energy()
{
    // Integrate D(h) cos(theta) d(omega). Change variable to GGX's original
    // CDF so very sharp lobes receive the same quadrature accuracy as rough ones.
    constexpr int samples = 20000;
    for (double alpha : {.0016, .04, .25, 1.0})
        for (double radius : {0.0, .05, .15, .3})
            for (bool spot : {false, true})
            {
                const double widened = ALLocalLightSize::alphaPrime(alpha, radius, .5, spot);
                double integral = 0.0;
                double blinn_integral = 0.0;
                const auto b = ALLocalLightSize::blinn(368.0, radius, .5, spot);
                for (int i = 0; i < samples; ++i)
                {
                    const double u = (i + .5) / samples;
                    const double nh = std::sqrt((1.0-u)/(1.0+(widened*widened-1.0)*u));
                    integral += ALLocalLightSize::ggx(nh, alpha) * ALLocalLightSize::ggxRatio(nh, alpha, radius, .5, spot)
                        / ALLocalLightSize::ggx(nh, widened) / samples;
                    // n+2 normalization conserves projected Blinn-lobe energy.
                    blinn_integral += (368.0+2.0)*b.normalization*std::pow(u,b.exponent)*u / samples;
                }
                require(std::abs(integral - 1.0) < 1.e-5, "GGX integrated energy within 0.001 percent");
                require(std::abs(blinn_integral - 1.0) < 1.e-4, "Blinn integrated energy within 0.01 percent");
            }
}
void finite()
{
    for (double alpha : {0.0, 1.e-30, .0016, 1.0})
        for (double radius : {0.0, 1.e-30, .15, .5})
            for (double distance : {0.0, 1.e-30, .1})
                for (double nh : {0.0, .5, 1.0})
                    for (bool spot : {false, true})
                    {
                        require(std::isfinite(ALLocalLightSize::ggxRatio(nh, alpha, radius, distance, spot)), "finite GGX limits");
                        const auto b = ALLocalLightSize::blinn(368.0, radius, distance, spot);
                        require(std::isfinite(b.exponent) && std::isfinite(b.normalization), "finite Blinn limits");
                    }
}
}

#ifdef AL_LOCAL_LIGHT_SIZE_STANDALONE
int main()
{
    try { identity(); peaks(); energy(); finite(); }
    catch (const std::exception& e) { std::cerr << "FAIL " << e.what() << '\n'; return 1; }
    std::cout << "PASS local light size: identity, peaks, integrated energy, singular limits\n";
}
#else
namespace tut
{
struct local_light_size_data {};
using local_light_size_group = test_group<local_light_size_data>;
using local_light_size_object = local_light_size_group::object;
local_light_size_group local_light_size_tests("ALLocalLightSize");
template<> template<> void local_light_size_object::test<1>() { identity(); }
template<> template<> void local_light_size_object::test<2>() { peaks(); }
template<> template<> void local_light_size_object::test<3>() { energy(); }
template<> template<> void local_light_size_object::test<4>() { finite(); }
}
#endif
