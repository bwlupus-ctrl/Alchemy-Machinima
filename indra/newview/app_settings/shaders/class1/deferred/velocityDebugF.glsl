/**
 * @file velocityDebugF.glsl
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

// [BDMerge A5.4-1a] Velocity-buffer debug visualization (BDMergeVelocityDebug).
// Maps the NDC velocity vector into a colour: rightward motion -> red, upward ->
// green, static -> neutral grey (0.5, 0.5). Amplified so small motions are
// visible. Used to VALIDATE motion vectors in-world before Phase 1b/2 build on
// them.

/*[EXTRA_CODE_HERE]*/

out vec4 frag_color;

uniform sampler2D velocityMap;

// amplification so sub-pixel NDC deltas are visible
uniform float motion_blur_strength; // reused as debug gain; defaults handled CPU-side

in vec2 vary_fragcoord;

void main()
{
    vec2 v = texture(velocityMap, vary_fragcoord).rg;

    float gain = motion_blur_strength > 0.0 ? motion_blur_strength : 16.0;
    vec2 c = clamp(v * gain, -1.0, 1.0) * 0.5 + 0.5;

    frag_color = vec4(c, 0.5, 1.0);
}
