/**
 * @file waterF.glsl
 *
 * $LicenseInfo:firstyear=2023&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2023, Linden Research, Inc.
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

 // error fallback on compilation failure

#ifdef HAS_VISIBLE_DIFFUSE
layout(location = 0) out vec4 frag_color;
layout(location = 1) out vec4 visible_diffuse;
layout(location = 2) out vec2 surface_coverage;
#else
out vec4 frag_color;
#endif

void main()
{
    frag_color = vec4(1,0,1,1);
#ifdef HAS_VISIBLE_DIFFUSE
    // S3: water was previously OUTSIDE the sidecar's modified set, so the
    // attachment kept whatever the seed pass wrote for the surface BEHIND the
    // water -- i.e. it published SEABED ALBEDO at K=1. Confidently wrong is
    // worse than absent.
    //
    // This renderer models water as specular/reflective only; it has no diffuse
    // lobe. So zero diffuse with K=1 is a statement about the BRDF, exactly as
    // the seed already does for metals -- a real answer, not a gap. Forward
    // coverage is claimed so the consumer knows the sidecar owns these pixels
    // and must not fall back to the seabed underneath.
    visible_diffuse = vec4(vec3(0.0), 1.0);
    surface_coverage = vec2(0.0, 1.0);
#endif
}

