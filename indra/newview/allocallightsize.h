/** Pure specular-lobe math for Personal Lighting's local light radius (metres).
 * Karis, Real Shading in Unreal Engine 4 (2013), sphere-light normalization.
 * No viewer, settings, or GL dependency. Keep in sync with deferredUtil.glsl.
 */
#pragma once
#include <algorithm>
#include <cmath>

namespace ALLocalLightSize
{
inline double alphaPrime(double alpha, double radius, double distance, bool spot = false)
{
    return std::min(1.0, alpha + std::max(radius, 0.0) /
        ((spot ? 3.0 : 2.0) * std::max(distance, 1.e-6)));
}

inline double ggxRatio(double nh, double alpha, double radius, double distance, bool spot = false)
{
    if (radius <= 0.0) return 1.0; // exact off path, including singular inputs
    alpha = std::max(alpha, 1.e-4);
    const double widened = alphaPrime(alpha, radius, distance, spot);
    const double norm = alpha / widened;
    const double c2 = std::clamp(nh * nh, 0.0, 1.0);
    const double original_den = (1.0 - c2) / (alpha * alpha) + c2;
    const double widened_den = (1.0 - c2) / (widened * widened) + c2;
    const double shape_ratio = original_den / widened_den;
    return norm * norm * shape_ratio * shape_ratio;
}

inline double ggx(double nh, double alpha)
{
    alpha = std::max(alpha, 1.e-4);
    const double c2 = std::clamp(nh * nh, 0.0, 1.0);
    const double den = (1.0 - c2) + alpha * alpha * c2;
    return alpha * alpha / (3.14159265358979323846 * den * den);
}

struct BlinnLobe { double exponent; double normalization; };
inline BlinnLobe blinn(double exponent, double radius, double distance, bool spot = false)
{
    if (radius <= 0.0) return {exponent, 1.0};
    exponent = std::max(exponent, 0.0);
    const double alpha = std::sqrt(2.0 / (exponent + 2.0));
    const double widened = alphaPrime(alpha, radius, distance, spot);
    const double norm = alpha / widened;
    return {std::max(2.0 / (widened * widened) - 2.0, 0.0), norm * norm};
}
}
