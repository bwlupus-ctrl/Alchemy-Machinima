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
// Inside the source (distance < radius), mirror of relightRatio (deferredUtil.glsl):
//   continuous (RenderLocalLightContinuousSource, the default): the clipped-sphere formula below with s2
//     held at 1, which is exactly (1 + cosine)/2: the value that formula reaches just outside, so nothing
//     steps on the source's surface. Centre guard: within kSourceCentreCore of the centre the cosine is
//     scaled by distance/core, so the term tends to 1/2 for every normal (finite, no undefined direction).
//   legacy (false; pre-11/14, kept for A/B): 1 everywhere inside, a step of up to 1 at distance == radius
//     that flickered on distant sub-pixel sources.
constexpr double kSourceCentreCore = 1.e-4; // metres; the shader's max(d, 1.e-4)
inline double diffuseRatio(double cosine, double radius, double distance, bool continuous_source = true)
{
    if (radius <= 0.0) return std::max(cosine, 0.0);
    if (distance < radius)
        return continuous_source ? std::clamp(0.5 + 0.5*cosine*std::min(distance/kSourceCentreCore, 1.0), 0.0, 1.0) : 1.0;
    const double s2 = std::min(1.0, radius*radius/std::max(distance*distance, 1.e-12));
    if (cosine >= std::sqrt(s2)) return cosine;
    return sphere(cosine, s2) / (3.14159265358979323846 * std::max(s2, 1.e-20));
}
// ---------------------------------------------------------------------------------------- shadow source size
// One source radius (metres) for every local-light shadow path. `authored` is LLPipeline's
// area_source_radius() (rig fixture size, RenderLocalLightSizeFromPrim or
// RenderLocalLightDefaultRadius; 0 = unsized), the value all three paths start from:
//   - the world cube resolve uploads min(authored > 0 ? authored : 0.1, 2) as area_world_info.z
//     (LLPipeline::renderAreaShadows, kept inline there);
//   - the screen-space area trace uploads it raw as area_lights[].w and areaShadowF.glsl applies
//     the same 0.1 m to an unsized light;
//   - the metric projector penumbra below uses this function.
// relight_spot_metric_gl_check.py holds the three to the same default and cap.
constexpr double kShadowSourceDefaultRadius = 0.1;
constexpr double kShadowSourceMaxRadius = 2.0;
constexpr double shadowSourceRadius(double authored)
{
    return authored > 0.0 ? (authored < kShadowSourceMaxRadius ? authored : kShadowSourceMaxRadius)
                          : kShadowSourceDefaultRadius;
}
// Metric projector penumbra (RenderSpotShadowPenumbraModel 1): how the dimensionless softness
// controls of the old model act on the source radius.
//   - A rig light in fixture mode has an authored physical source size, and its softness
//     override is DERIVED from that same size: the radius is used as is (trim 1), so the
//     projector map and a world cube of the same fixture agree.
//   - Any other light: radius * softness / 3, softness = the light's rig override when it has
//     one, the global Softness (BDMergeSoftShadowSoftness) otherwise. 3 is that setting's
//     default, so an untouched slider is exactly the physical radius.
constexpr double kSpotPenumbraSoftnessUnit = 3.0;
constexpr double kSpotPenumbraSoftnessMax = 16.0;
constexpr double spotPenumbraTrim(bool fixture_sized, double override_softness, double global_softness)
{
    if (fixture_sized) return 1.0;
    const double softness = override_softness > 0.0 ? override_softness : global_softness;
    if (!(softness > 0.0)) return 0.0;
    return (softness < kSpotPenumbraSoftnessMax ? softness : kSpotPenumbraSoftnessMax) / kSpotPenumbraSoftnessUnit;
}
// A perspective shadow map (OpenGL clip space, depth range 0..1) with projection P stores window
// depth d(z) = A - K/z for axial distance z from the projection origin:
//   A = (1 - P[2][2])/2 = far/(far-near),   K = -P[3][2]/2 = far*near/(far-near).
constexpr double spotDepthA(double p22) { return 0.5 * (1.0 - p22); }
constexpr double spotDepthK(double p32) { return -0.5 * p32; }
// Texels of penumbra radius per unit of window depth between blocker and receiver, on one axis:
//   focal = P[0][0] or P[1][1] = 1 / tan(half the field of view) on that axis.
// A disk of radius R at the projection origin, seen past a blocker at z_b, leaves a penumbra of
// radius R*(z_r-z_b)/z_b on a receiver at z_r; one texel there is 2*z_r/(focal*texels) wide, so
// the radius is R*texels*focal/2 * (1/z_b - 1/z_r) = R*texels*focal/(2K) * (d_r - d_b) texels.
// 0 = no valid projection (the caller keeps the old model for that slot).
constexpr double spotPenumbraScale(double radius_m, double map_texels, double focal, double p32)
{
    const double depth_k = spotDepthK(p32);
    return radius_m > 0.0 && map_texels > 0.0 && focal > 0.0 && depth_k > 1.e-9
        ? radius_m * map_texels * focal / (2.0 * depth_k) : 0.0;
}
// The source disk sits at the LIGHT'S CENTRE (where the cube and area paths put it), which is
// centre_depth metres in front of the projection origin (the origin is set back behind the lamp
// prim; negative when the centre is behind it). Similar triangles then divide by (z_b - c)
// instead of z_b: the radius above times z_b/(z_b - c) = 1/(1 - s*(A - d_b)), s = c/K.
constexpr double spotPenumbraSetback(double centre_depth, double p32)
{
    const double depth_k = spotDepthK(p32);
    return depth_k > 1.e-9 ? centre_depth / depth_k : 0.0;
}
// The shader's evaluation (shadowUtil.glsl spotMetricPenumbra), for the checks below.
constexpr double spotPenumbraTexels(double scale, double depth_a, double setback, double receiver_d, double blocker_d)
{
    const double lever = 1.0 - setback * (depth_a - blocker_d);
    return scale * (receiver_d - blocker_d) / (lever > 0.05 ? lever : 0.05);
}
namespace penumbra_selfcheck
{
constexpr bool within(double a, double b, double tolerance) { return a - b < tolerance && b - a < tolerance; }
// 90 degree square map (focal 1), near 0.5 m, far 10.5 m: K = 10.5*0.5/10 = 0.525 (P32 = -1.05),
// A = 10.5/10 = 1.05 (P22 = -1.1). R = 0.1 m on 2048 texels: 0.1*2048/(2*1*0.525) = 195.0476
// texels per unit depth. Blocker 4 m and receiver 5 m from the origin: d = 0.91875 and 0.945,
// 0.02625 apart: 5.12 texels of 2*5/2048 m = 0.025 m = R*(5-4)/4.
constexpr double kA = spotDepthA(-1.1), kScale = spotPenumbraScale(0.1, 2048.0, 1.0, -1.05);
static_assert(within(kA, 1.05, 1.e-12) && within(spotDepthK(-1.05), 0.525, 1.e-12), "spot depth constants");
static_assert(within(kScale, 195.047619, 1.e-5), "spot penumbra scale");
static_assert(within(spotPenumbraTexels(kScale, kA, 0.0, 0.945, 0.91875) * (2.0 * 5.0 / 2048.0), 0.025, 1.e-9), "spot penumbra metres");
// The light's centre 0.4 m in front of the origin: R*(5-4)/(4-0.4) = 0.0277778 m.
static_assert(within(spotPenumbraTexels(kScale, kA, spotPenumbraSetback(0.4, -1.05), 0.945, 0.91875) * (2.0 * 5.0 / 2048.0),
                     0.1 * 1.0 / 3.6, 1.e-9), "spot penumbra setback");
// ... and 0.3 m behind it: R*(5-4)/(4+0.3).
static_assert(within(spotPenumbraTexels(kScale, kA, spotPenumbraSetback(-0.3, -1.05), 0.945, 0.91875) * (2.0 * 5.0 / 2048.0),
                     0.1 * 1.0 / 4.3, 1.e-9), "spot penumbra negative setback");
// Twice the focal length (a narrower cone) doubles the texels per metre, and so the radius in texels.
static_assert(within(spotPenumbraScale(0.1, 2048.0, 2.0, -1.05), 2.0 * 195.047619, 1.e-4), "spot penumbra focal");
static_assert(spotPenumbraScale(0.1, 2048.0, 1.0, 0.0) == 0.0 && spotPenumbraScale(0.0, 2048.0, 1.0, -1.05) == 0.0 &&
              spotPenumbraSetback(0.4, 0.0) == 0.0, "spot penumbra invalid");
static_assert(shadowSourceRadius(0.0) == 0.1 && shadowSourceRadius(-1.0) == 0.1 && shadowSourceRadius(0.3) == 0.3 &&
              shadowSourceRadius(5.0) == 2.0, "shadow source radius");
static_assert(spotPenumbraTrim(true, 8.0, 0.5) == 1.0 && spotPenumbraTrim(false, 0.0, 3.0) == 1.0 &&
              spotPenumbraTrim(false, 6.0, 3.0) == 2.0 && spotPenumbraTrim(false, 0.0, 0.0) == 0.0 &&
              within(spotPenumbraTrim(false, 0.0, 99.0), 16.0 / 3.0, 1.e-12), "spot penumbra trim");
}
}
#endif
