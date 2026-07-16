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

// [BDMerge CS] Screen-space contact shadows: short per-pixel ray march against
// the scene depth toward a specific light. Catches occlusion BELOW the shadow
// map's bias threshold (thin worn-mesh layers, cloth-on-skin, jewelry) and
// gives point lights -- which never get shadow maps in this engine -- their
// first directional shadowing. Cinematic design choices: spatial-only dither
// (stable across frames, capture-safe), distance-weighted softening (near
// hits darker), screen-edge and thickness guards against SS artifacts.
//
// No main(); linked as a second fragment object into the sun and local-light
// programs (froxelUtil pattern). getPosition() comes from deferredUtil.glsl,
// which is already linked into every consumer.

uniform vec4 contact_shadow_params; // x=range(m) y=thickness(m) z=intensity w=steps
uniform vec4 contact_shadow_flags;  // x=sun/moon on, y=local lights on, zw spare

uniform mat4 projection_matrix;     // auto-fed by matrix sync

vec4 getPosition(vec2 pos_screen);  // deferredUtil.glsl
uniform vec2 screen_res;            // duplicate declaration is legal; merged at link

float bdmergeContactShadowMarch(vec3 pos, vec3 dir_n, vec2 pos_screen)
{
    float range     = contact_shadow_params.x;
    float thickness = contact_shadow_params.y;
    float intensity = contact_shadow_params.z;
    int   steps     = int(contact_shadow_params.w);

    if (range <= 0.0 || intensity <= 0.0 || steps < 1)
    {
        return 1.0;
    }

    // interleaved-gradient noise from SCREEN POSITION only: per-pixel offsets
    // decorrelate banding, but the pattern is identical every frame so video
    // capture never shimmers (deliberately NOT framecount-animated).
    float jitter = fract(52.9829189 * fract(dot(pos_screen, vec2(0.06711056, 0.00583715))));

    float t_step = range / float(steps);
    float t      = t_step * (0.35 + 0.65 * jitter); // start off the surface
    float occl   = 0.0;

    for (int i = 0; i < steps; ++i)
    {
        vec3 sp = pos + dir_n * t;

        vec4 clip = projection_matrix * vec4(sp, 1.0);
        if (clip.w <= 0.0)
        {
            break; // marched behind the camera
        }
        vec2 uv = (clip.xy / clip.w) * 0.5 + 0.5;
        if (uv.x <= 0.0 || uv.x >= 1.0 || uv.y <= 0.0 || uv.y >= 1.0)
        {
            break; // left the screen: unknown territory, assume unoccluded
        }

        float scene_z = getPosition(uv * screen_res).z;

        // view space looks down -Z: nearer surfaces have GREATER z. The sample
        // is occluded when the depth buffer's surface sits in front of it by
        // more than a self-hit epsilon but less than the assumed occluder
        // thickness (prevents thin foreground silhouettes casting onto the
        // whole world behind them).
        float diff = scene_z - sp.z;
        if (diff > 0.008 && diff < thickness)
        {
            // near hits shadow harder than far hits -> pseudo-penumbra
            float o = 1.0 - 0.7 * (t / range);
            // fade near screen edges where the march is unreliable
            vec2 ef = min(uv, vec2(1.0) - uv);
            o *= clamp(min(ef.x, ef.y) * 12.0, 0.0, 1.0);
            occl = max(occl, o);
            break;
        }

        t += t_step;
    }

    return 1.0 - occl * intensity;
}

// Gated wrappers so apply sites stay one-liners and the master/sub toggles
// live in the uniforms (zeroed by C++ when off or during cube snapshots).
float bdmergeContactShadowSun(vec3 pos, vec3 dir_n, vec2 pos_screen)
{
    if (contact_shadow_flags.x < 0.5) return 1.0;
    return bdmergeContactShadowMarch(pos, dir_n, pos_screen);
}

float bdmergeContactShadowLocal(vec3 pos, vec3 dir_n, vec2 pos_screen)
{
    if (contact_shadow_flags.y < 0.5) return 1.0;
    return bdmergeContactShadowMarch(pos, dir_n, pos_screen);
}
