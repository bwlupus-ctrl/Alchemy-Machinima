/**
 * @file contactShadowUtil.glsl
 *
 * $LicenseInfo:firstyear=2007&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2007, Linden Research, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

// [BDMerge CS v2] Screen-space contact shadows tuned for FINE DETAIL (AO-like
// scale, but directional per-light). v1 field failures fixed:
//  - whole-screen self-shadowing on close-ups / camera-axis lights: the
//    same-plane depth gap grows ~linearly along the ray (t * -dir.z) and fell
//    inside the fixed thickness window. The occlusion epsilon now grows at
//    exactly that slope, so a surface can NEVER self-occlude at any light
//    angle; only geometry standing off the plane (a layer above it) hits.
//  - normal-offset ray start escapes the surface before the first sample.
//  - hard screen-footprint cap (fraction of screen) so close-ups
//    automatically shorten the effective radius -> stays micro-detail.
//  - soft proximity-weighted accumulation instead of a binary first hit.
// Frame-stable spatial dither only (video-capture-safe).

uniform vec4 contact_shadow_params; // x=range(m) y=thickness(m) z=intensity w=steps
uniform vec4 contact_shadow_flags;  // x=sun/moon on, y=local lights on, zw spare

// The SCENE projection, uploaded explicitly by the C++ bind helper (velocity
// system's un-jittered capture). The sun / fullscreen-light passes draw with
// identity matrices, so the auto-synced projection_matrix is useless here.
uniform mat4 projection_matrix_unjittered;
#define CS_PROJ projection_matrix_unjittered

vec4 getPosition(vec2 pos_screen);  // deferredUtil.glsl
uniform vec2 screen_res;            // duplicate declaration merges at link

float bdmergeContactShadowMarch(vec3 pos, vec3 norm, vec3 dir_n, vec2 pos_screen)
{
    float range     = contact_shadow_params.x;
    float thickness = contact_shadow_params.y;
    float intensity = contact_shadow_params.z;
    int   steps     = int(contact_shadow_params.w);

    if (range <= 0.0 || intensity <= 0.0 || steps < 1)
    {
        return 1.0;
    }

    // grazing lights are where screen-space marching lies the most; fade the
    // whole effect out as the light drops toward the surface plane
    float ndl = dot(norm, dir_n);
    if (ndl <= 0.02)
    {
        return 1.0;
    }
    float graze_fade = smoothstep(0.02, 0.15, ndl);

    // escape the surface plane before sampling
    vec3 ro = pos + norm * 0.02;

    // interleaved-gradient noise from screen position only: per-pixel offset,
    // identical every frame -> stable in video capture
    float jitter = fract(52.9829189 * fract(dot(pos_screen, vec2(0.06711056, 0.00583715))));

    float t_step = range / float(steps);
    float t      = t_step * (0.25 + 0.75 * jitter);
    float occl   = 0.0;

    vec2 uv0 = pos_screen / screen_res;

    for (int i = 0; i < steps; ++i)
    {
        vec3 sp = ro + dir_n * t;

        vec4 clip = CS_PROJ * vec4(sp, 1.0);
        if (clip.w <= 0.0)
        {
            break;
        }
        vec2 uv = (clip.xy / clip.w) * 0.5 + 0.5;
        if (uv.x <= 0.0 || uv.x >= 1.0 || uv.y <= 0.0 || uv.y >= 1.0)
        {
            break;
        }
        // FINE-DETAIL CONTRACT: never let the march cover more than a small
        // fraction of the screen. On close-ups this is what keeps the effect
        // at AO scale instead of smearing shadows across the frame.
        if (distance(uv, uv0) > 0.06)
        {
            break;
        }

        float scene_z = getPosition(uv * screen_res).z;

        // slope-aware epsilon: a flat surface produces diff == t * (-dir_n.z)
        // exactly; requiring MORE than that (plus margins) means only geometry
        // standing off the receiver plane can register as an occluder.
        float eps  = 0.012 + 0.03 * t + t * max(0.0, -dir_n.z);
        float diff = scene_z - sp.z;
        if (diff > eps && diff < thickness)
        {
            // proximity-weighted, distance-softened accumulation (AO-like):
            // nearer hits and closer occluders darken more; no binary edge
            float w = (1.0 - t / range);
            w *= 1.0 - 0.5 * clamp(diff / thickness, 0.0, 1.0);
            occl = max(occl, w);
        }

        t += t_step;
    }

    // screen-edge fade on the RECEIVER (cheap; per-sample fade handled by cap)
    vec2 ef = min(uv0, vec2(1.0) - uv0);
    float edge = clamp(min(ef.x, ef.y) * 12.0, 0.0, 1.0);

    return 1.0 - occl * intensity * graze_fade * edge;
}

float bdmergeContactShadowSun(vec3 pos, vec3 norm, vec3 dir_n, vec2 pos_screen)
{
    if (contact_shadow_flags.x < 0.5) return 1.0;
    return bdmergeContactShadowMarch(pos, norm, dir_n, pos_screen);
}

float bdmergeContactShadowLocal(vec3 pos, vec3 norm, vec3 dir_n, vec2 pos_screen)
{
    if (contact_shadow_flags.y < 0.5) return 1.0;
    return bdmergeContactShadowMarch(pos, norm, dir_n, pos_screen);
}
