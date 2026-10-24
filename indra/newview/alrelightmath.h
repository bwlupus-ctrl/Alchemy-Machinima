/** @file alrelightmath.h
 * Deterministic Phase 1 light-shape maths. Public references: Karis 2013
 * (equations 10, 14), Lagarde & de Rousiers 2014 (sphere illuminance).
 */
#ifndef AL_RELIGHT_MATH_H
#define AL_RELIGHT_MATH_H
#include <algorithm>
#include <cmath>
#include <array>
namespace ALRelight
{
using Vec = std::array<double, 3>;
// Inscribed sphere of the emitting prim's own metre scale, never linkset/AABB.
// Projectors and attachments use the same rotation-independent XYZ convention.
inline double primSourceRadius(const Vec& scale)
{
    for (double dimension : scale)
        if (!std::isfinite(dimension) || dimension <= 0.0) return 0.0;
    return std::clamp(0.5 * std::min({scale[0], scale[1], scale[2]}), 0.0, 1.0);
}
inline double dot(const Vec& a, const Vec& b) { return a[0]*b[0]+a[1]*b[1]+a[2]*b[2]; }
inline Vec unit(const Vec& v, const Vec& fallback)
{
    const double length = std::sqrt(dot(v,v));
    return length > 1.e-6 ? Vec{v[0]/length,v[1]/length,v[2]/length} : fallback;
}
inline Vec representative(const Vec& n, const Vec& v, const Vec& light, double radius)
{
    const double nv = dot(n,v);
    const Vec ray{2.0*nv*n[0]-v[0],2.0*nv*n[1]-v[1],2.0*nv*n[2]-v[2]};
    const double distance = std::sqrt(dot(light,light));
    if (distance == 0.0) return n; // direction is undefined only at the centre
    const Vec centre{light[0]/distance,light[1]/distance,light[2]/distance};
    const double projection = dot(centre,ray); // signed Karis closest-line point
    const Vec delta{ray[0]*projection-centre[0],ray[1]*projection-centre[1],ray[2]*projection-centre[2]};
    // Contact regularization: keep >=5% of the centre direction, even inside
    // the sphere. This bounds angular sensitivity and cannot produce a zero.
    const double t = std::min(std::min(radius/distance,.95)/std::max(std::sqrt(dot(delta,delta)),1.e-6),1.0);
    return unit(Vec{centre[0]+delta[0]*t,centre[1]+delta[1]*t,centre[2]+delta[2]*t},n);
}
inline double legacy(double d, double range, double falloff)
{
    if (range <= 0.0) return 0.0;
    const double a = std::clamp(1.0 - (d / range + falloff) / (1.0 + falloff), 0.0, 1.0);
    return 2.0 * a * a;
}
inline double physical(double d, double range, double /*radius*/, double floor)
{
    if (range <= 0.0 || d >= range) return 0.0;
    const double q = d / range;
    const double w = std::max(0.0, 1.0 - q*q*q*q);
    // Source size belongs to the angular integral. Only the near floor
    // regularizes distance, so resizing a source cannot change exposure.
    const double near_distance = std::max(floor, 0.0001);
    return w*w / std::max(d*d, near_distance*near_distance);
}
inline double gain(double reference, double range, double falloff, double /*radius*/, double floor)
{
    const double p = physical(reference, range, 0.0, floor);
    return p > 0.0 ? legacy(reference, range, falloff) / p : 0.0;
}
inline double capped(double d, double range, double falloff, double radius, double floor, double calibration, double boost)
{
    return std::min(calibration * physical(d, range, radius, floor),
                    std::max(0.0, boost) * legacy(0.0, range, falloff));
}
inline double normalization(double alpha, double radius, double distance)
{
    const double widened = std::clamp(alpha + radius / (3.0 * std::max(distance, 0.0001)), 0.000001, 1.0);
    const double ratio = alpha / widened;
    return ratio * ratio;
}
// Unit-radiance irradiance, including the horizon. Analytic spherical-cap integral.
inline double sphere(double cosine, double sin2)
{
    constexpr double pi = 3.14159265358979323846;
    const double c = std::clamp(cosine, -1.0, 1.0);
    const double s2 = std::clamp(sin2, 0.0, 1.0);
    if (s2 < 1.e-6) return pi*s2*std::max(c, 0.0); // stable point-source limit
    if (c*c > s2) return pi*s2*std::max(c, 0.0);
    const double sint = std::sqrt(std::max(1.0-c*c, 0.00000001));
    const double x = std::sqrt(std::max(1.0/std::max(s2, 0.00000001)-1.0, 0.0));
    const double y = std::clamp(-x*c/sint, -1.0, 1.0);
    const double sy = std::sqrt(std::max(1.0-y*y, 0.0));
    return std::clamp((c*std::acos(y)-x*sint*sy)*s2 + std::atan2(sint*sy, x), 0.0, pi);
}
inline double diffuseRatio(double cosine, double radius, double distance)
{
    if (radius <= 0.0) return std::max(cosine, 0.0);
    if (distance < radius) return 1.0; // full hemisphere, also at the centre
    const double s2 = std::min(1.0, radius*radius/std::max(distance*distance, 1.e-12));
    if (cosine >= std::sqrt(s2)) return cosine;
    return sphere(cosine, s2) / (3.14159265358979323846 * std::max(s2, 1.e-20));
}
}
#endif
