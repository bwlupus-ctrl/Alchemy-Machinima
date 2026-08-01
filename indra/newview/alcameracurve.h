/**
 * @file alcameracurve.h
 * @brief Deterministic easing curves shared by machinima camera transitions.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#ifndef AL_ALCAMERACURVE_H
#define AL_ALCAMERACURVE_H

#include "llmath.h"
#include "stdtypes.h"

#include <cmath>

namespace ALCameraCurve
{
enum ECurve : S32
{
    SMOOTHERSTEP = 0,
    LINEAR = 1,
    CSS_EASE = 2,
    CSS_EASE_IN = 3,
    CSS_EASE_OUT = 4,
    CSS_EASE_IN_OUT = 5,
    QUAD_IN = 6,
    QUAD_OUT = 7,
    QUAD_IN_OUT = 8,
    CUBIC_IN = 9,
    CUBIC_OUT = 10,
    CUBIC_IN_OUT = 11,
    QUART_IN = 12,
    QUART_OUT = 13,
    QUART_IN_OUT = 14,
    QUINT_IN = 15,
    QUINT_OUT = 16,
    QUINT_IN_OUT = 17,
    EXPO_IN = 18,
    EXPO_OUT = 19,
    EXPO_IN_OUT = 20,
    CIRC_IN = 21,
    CIRC_OUT = 22,
    CIRC_IN_OUT = 23,
    BACK_IN = 24,
    BACK_OUT = 25,
    BACK_IN_OUT = 26,
    ELASTIC_IN = 27,
    ELASTIC_OUT = 28,
    ELASTIC_IN_OUT = 29,
    BOUNCE_IN = 30,
    BOUNCE_OUT = 31,
    BOUNCE_IN_OUT = 32,
    CUSTOM_BEZIER = 100,
};

struct Bezier
{
    F32 mX1;
    F32 mY1;
    F32 mX2;
    F32 mY2;
};

inline bool isValidId(S32 id)
{
    return (id >= SMOOTHERSTEP && id <= BOUNCE_IN_OUT) ||
        id == CUSTOM_BEZIER;
}

inline S32 sanitizeId(S32 id)
{
    return isValidId(id) ? id : (S32)SMOOTHERSTEP;
}

inline F32 sanitizeX(F32 value, F32 fallback)
{
    return std::isfinite(value) ? llclamp(value, 0.f, 1.f) : fallback;
}

inline F32 sanitizeY(F32 value, F32 fallback)
{
    return std::isfinite(value) ? llclamp(value, -2.f, 3.f) : fallback;
}

// This is the existing cc_smootherstep quintic verbatim. Keep the expression
// stable: persisted curve id 0 is the byte-identical legacy default.
inline F32 smootherstep(F32 u)
{
    u = llclamp(u, 0.f, 1.f);
    return u * u * u * (u * (u * 6.f - 15.f) + 10.f);
}

inline F32 bezierComponent(F32 t, F32 p1, F32 p2)
{
    const F32 c = 3.f * p1;
    const F32 b = 3.f * (p2 - p1) - c;
    const F32 a = 1.f - c - b;
    return ((a * t + b) * t + c) * t;
}

inline F32 bezierComponentDeriv(F32 t, F32 p1, F32 p2)
{
    const F32 c = 3.f * p1;
    const F32 b = 3.f * (p2 - p1) - c;
    const F32 a = 1.f - c - b;
    return (3.f * a * t + 2.f * b) * t + c;
}

// CSS/WebKit UnitBezier: solve x(t)=u, then return y(t). The fixed Newton and
// bisection budgets make this pure, deterministic, and independent of clocks.
inline F32 evalBezier(F32 u, F32 x1, F32 y1, F32 x2, F32 y2)
{
    if (!std::isfinite(u))
    {
        return 1.f;
    }
    u = llclamp(u, 0.f, 1.f);
    x1 = sanitizeX(x1, 0.42f);
    x2 = sanitizeX(x2, 0.58f);
    y1 = sanitizeY(y1, 0.f);
    y2 = sanitizeY(y2, 1.f);
    if (u <= 0.f)
    {
        return 0.f;
    }
    if (u >= 1.f)
    {
        return 1.f;
    }
    if (x1 == y1 && x2 == y2)
    {
        return u;
    }

    // Solve x(t)=u for t by BRACKETED guarded Newton-bisection. x(t) is
    // monotone on [0,1] with x(0)=0, x(1)=1 (u is already in the open interval).
    // Terminate on the t-bracket width, NEVER on x-error: near a flat-x endpoint
    // (e.g. a custom x1=x2=0 curve, x(t)=t^3) a tiny |x(t)-u| can occur at a t
    // far from the true root, so an x-error test would return y at the wrong t.
    // A Newton step is taken only when it stays strictly inside the live bracket;
    // otherwise we bisect. Fixed iteration budget => pure/deterministic. y is
    // evaluated exactly once, after t has converged.
    F32 lo = 0.f;
    F32 hi = 1.f;
    F32 t  = u; // x(t) ~= t near the diagonal: a good first guess
    for (S32 i = 0; i < 32; ++i)
    {
        const F32 x = bezierComponent(t, x1, x2);
        if (x < u)
        {
            lo = t;
        }
        else
        {
            hi = t;
        }
        if (hi - lo < 0.0000001f)
        {
            break; // t converged (bracket, not x-error)
        }
        const F32 derivative = bezierComponentDeriv(t, x1, x2);
        const F32 newton = (fabsf(derivative) > 0.000001f)
            ? t - (x - u) / derivative
            : t;
        t = (newton > lo && newton < hi) ? newton : 0.5f * (lo + hi);
    }
    return llclamp(bezierComponent(t, y1, y2), -1.f, 2.f);
}

inline F32 standardBounceOut(F32 u)
{
    constexpr F32 N1 = 7.5625f;
    constexpr F32 D1 = 2.75f;
    if (u < 1.f / D1)
    {
        return N1 * u * u;
    }
    if (u < 2.f / D1)
    {
        u -= 1.5f / D1;
        return N1 * u * u + 0.75f;
    }
    if (u < 2.5f / D1)
    {
        u -= 2.25f / D1;
        return N1 * u * u + 0.9375f;
    }
    u -= 2.625f / D1;
    return N1 * u * u + 0.984375f;
}

// Camera-safe bounce: retain half of the standard bounce character and blend
// the other half toward monotone smootherstep for gentler SL camera landings.
// bounceOut(u) = 0.5 * standardBounceOut(u) + 0.5 * smootherstep(u).
inline F32 bounceOut(F32 u)
{
    const F32 standard = standardBounceOut(u);
    return standard + (smootherstep(u) - standard) * 0.5f;
}

inline F32 eval(S32 id, F32 u,
                F32 bx1 = 0.42f, F32 by1 = 0.f,
                F32 bx2 = 0.58f, F32 by2 = 1.f)
{
    if (!std::isfinite(u))
    {
        return 1.f;
    }
    u = llclamp(u, 0.f, 1.f);
    if (u <= 0.f)
    {
        return 0.f;
    }
    if (u >= 1.f)
    {
        return 1.f;
    }

    constexpr F32 C1 = 1.70158f;
    constexpr F32 C2 = C1 * 1.525f;
    constexpr F32 C3 = C1 + 1.f;
    constexpr F32 PI = 3.14159265358979323846f;
    constexpr F32 C4 = 2.f * PI / 3.f;
    constexpr F32 C5 = 2.f * PI / 4.5f;
    F32 result = 0.f;
    switch (id)
    {
        case SMOOTHERSTEP: result = smootherstep(u); break;
        case LINEAR: result = u; break;
        case CSS_EASE:
            result = evalBezier(u, 0.25f, 0.10f, 0.25f, 1.f); break;
        case CSS_EASE_IN:
            result = evalBezier(u, 0.42f, 0.f, 1.f, 1.f); break;
        case CSS_EASE_OUT:
            result = evalBezier(u, 0.f, 0.f, 0.58f, 1.f); break;
        case CSS_EASE_IN_OUT:
            result = evalBezier(u, 0.42f, 0.f, 0.58f, 1.f); break;
        case QUAD_IN: result = u * u; break;
        case QUAD_OUT: result = 1.f - (1.f - u) * (1.f - u); break;
        case QUAD_IN_OUT:
            result = u < 0.5f
                ? 2.f * u * u
                : 1.f - powf(-2.f * u + 2.f, 2.f) * 0.5f;
            break;
        case CUBIC_IN: result = u * u * u; break;
        case CUBIC_OUT: result = 1.f - powf(1.f - u, 3.f); break;
        case CUBIC_IN_OUT:
            result = u < 0.5f
                ? 4.f * u * u * u
                : 1.f - powf(-2.f * u + 2.f, 3.f) * 0.5f;
            break;
        case QUART_IN: result = u * u * u * u; break;
        case QUART_OUT: result = 1.f - powf(1.f - u, 4.f); break;
        case QUART_IN_OUT:
            result = u < 0.5f
                ? 8.f * u * u * u * u
                : 1.f - powf(-2.f * u + 2.f, 4.f) * 0.5f;
            break;
        case QUINT_IN: result = u * u * u * u * u; break;
        case QUINT_OUT: result = 1.f - powf(1.f - u, 5.f); break;
        case QUINT_IN_OUT:
            result = u < 0.5f
                ? 16.f * u * u * u * u * u
                : 1.f - powf(-2.f * u + 2.f, 5.f) * 0.5f;
            break;
        case EXPO_IN: result = powf(2.f, 10.f * u - 10.f); break;
        case EXPO_OUT: result = 1.f - powf(2.f, -10.f * u); break;
        case EXPO_IN_OUT:
            result = u < 0.5f
                ? powf(2.f, 20.f * u - 10.f) * 0.5f
                : (2.f - powf(2.f, -20.f * u + 10.f)) * 0.5f;
            break;
        case CIRC_IN: result = 1.f - sqrtf(1.f - u * u); break;
        case CIRC_OUT:
            result = sqrtf(1.f - (u - 1.f) * (u - 1.f)); break;
        case CIRC_IN_OUT:
            result = u < 0.5f
                ? (1.f - sqrtf(1.f - 4.f * u * u)) * 0.5f
                : (sqrtf(1.f - (-2.f * u + 2.f) *
                                  (-2.f * u + 2.f)) + 1.f) * 0.5f;
            break;
        case BACK_IN:
            result = C3 * u * u * u - C1 * u * u; break;
        case BACK_OUT:
            result = 1.f + C3 * powf(u - 1.f, 3.f) +
                C1 * (u - 1.f) * (u - 1.f);
            break;
        case BACK_IN_OUT:
            result = u < 0.5f
                ? (4.f * u * u * ((C2 + 1.f) * 2.f * u - C2)) * 0.5f
                : (powf(2.f * u - 2.f, 2.f) *
                   ((C2 + 1.f) * (2.f * u - 2.f) + C2) + 2.f) * 0.5f;
            break;
        case ELASTIC_IN:
            result = -powf(2.f, 10.f * u - 10.f) *
                sinf((10.f * u - 10.75f) * C4);
            break;
        case ELASTIC_OUT:
            result = powf(2.f, -10.f * u) *
                sinf((10.f * u - 0.75f) * C4) + 1.f;
            break;
        case ELASTIC_IN_OUT:
            result = u < 0.5f
                ? -(powf(2.f, 20.f * u - 10.f) *
                    sinf((20.f * u - 11.125f) * C5)) * 0.5f
                : (powf(2.f, -20.f * u + 10.f) *
                   sinf((20.f * u - 11.125f) * C5)) * 0.5f + 1.f;
            break;
        case BOUNCE_IN: result = 1.f - bounceOut(1.f - u); break;
        case BOUNCE_OUT: result = bounceOut(u); break;
        case BOUNCE_IN_OUT:
            result = u < 0.5f
                ? (1.f - bounceOut(1.f - 2.f * u)) * 0.5f
                : (1.f + bounceOut(2.f * u - 1.f)) * 0.5f;
            break;
        case CUSTOM_BEZIER:
            result = evalBezier(u, bx1, by1, bx2, by2); break;
        default: result = smootherstep(u); break;
    }
    return std::isfinite(result) ? llclamp(result, -1.f, 2.f) : 1.f;
}

// Edge-localized feathering leaves the selected curve's midpoint untouched
// while progressively replacing its endpoint slopes with smootherstep:
// W(u) = feather * (2u - 1)^2; out = lerp(eval(...), smootherstep(u), W(u)).
inline F32 evalFeathered(S32 id, F32 u,
                         F32 bx1, F32 by1, F32 bx2, F32 by2,
                         F32 feather)
{
    const F32 curved = eval(id, u, bx1, by1, bx2, by2);
    if (!std::isfinite(feather) || feather <= 0.f)
    {
        return curved; // preserve the exact unfeathered evaluation path
    }
    if (!std::isfinite(u))
    {
        return curved;
    }

    u = llclamp(u, 0.f, 1.f);
    feather = llclamp(feather, 0.f, 1.f);
    const F32 edge = 2.f * u - 1.f;
    const F32 weight = feather * edge * edge;
    const F32 result = curved + (smootherstep(u) - curved) * weight;
    return std::isfinite(result) ? llclamp(result, -1.f, 2.f) : curved;
}

// Cubic seeds from the common Ceaser/easings.net approximation table. These
// are editor seeds only; ids 6..32 always evaluate with the closed forms above.
inline Bezier presetBezier(S32 id)
{
    switch (id)
    {
        case LINEAR: return { 0.f, 0.f, 1.f, 1.f };
        case CSS_EASE: return { 0.25f, 0.10f, 0.25f, 1.f };
        case CSS_EASE_IN: return { 0.42f, 0.f, 1.f, 1.f };
        case CSS_EASE_OUT: return { 0.f, 0.f, 0.58f, 1.f };
        case CSS_EASE_IN_OUT: return { 0.42f, 0.f, 0.58f, 1.f };
        case QUAD_IN: return { 0.55f, 0.085f, 0.68f, 0.53f };
        case QUAD_OUT: return { 0.25f, 0.46f, 0.45f, 0.94f };
        case QUAD_IN_OUT: return { 0.455f, 0.03f, 0.515f, 0.955f };
        case CUBIC_IN: return { 0.55f, 0.055f, 0.675f, 0.19f };
        case CUBIC_OUT: return { 0.215f, 0.61f, 0.355f, 1.f };
        case CUBIC_IN_OUT: return { 0.645f, 0.045f, 0.355f, 1.f };
        case QUART_IN: return { 0.895f, 0.03f, 0.685f, 0.22f };
        case QUART_OUT: return { 0.165f, 0.84f, 0.44f, 1.f };
        case QUART_IN_OUT: return { 0.77f, 0.f, 0.175f, 1.f };
        case QUINT_IN: return { 0.755f, 0.05f, 0.855f, 0.06f };
        case QUINT_OUT: return { 0.23f, 1.f, 0.32f, 1.f };
        case QUINT_IN_OUT: return { 0.86f, 0.f, 0.07f, 1.f };
        case EXPO_IN: return { 0.95f, 0.05f, 0.795f, 0.035f };
        case EXPO_OUT: return { 0.19f, 1.f, 0.22f, 1.f };
        case EXPO_IN_OUT: return { 1.f, 0.f, 0.f, 1.f };
        case CIRC_IN: return { 0.6f, 0.04f, 0.98f, 0.335f };
        case CIRC_OUT: return { 0.075f, 0.82f, 0.165f, 1.f };
        case CIRC_IN_OUT: return { 0.785f, 0.135f, 0.15f, 0.86f };
        case BACK_IN: return { 0.6f, -0.28f, 0.735f, 0.045f };
        case BACK_OUT: return { 0.175f, 0.885f, 0.32f, 1.275f };
        case BACK_IN_OUT: return { 0.68f, -0.55f, 0.265f, 1.55f };
        default: return { 0.42f, 0.f, 0.58f, 1.f };
    }
}
} // namespace ALCameraCurve

#endif // AL_ALCAMERACURVE_H
