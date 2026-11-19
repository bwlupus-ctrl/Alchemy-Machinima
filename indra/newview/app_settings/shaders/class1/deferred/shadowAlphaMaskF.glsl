/**
 * @file shadowAlphaMaskF.glsl
 *
 * $LicenseInfo:firstyear=2011&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2011, Linden Research, Inc.
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

/*[EXTRA_CODE_HERE]*/

out vec4 frag_color;

in vec4 post_pos;
in float target_pos_x;
in vec4 vertex_color;
in vec2 vary_texcoord0;
in vec3 vary_actor_fx_position;
uniform float minimum_alpha;

// [ShadowDist P3] shadow-pass alpha policy (design v4 s3 / v5 s2). Mode 0 = stock (the
// legacy branches below run verbatim). Mode 1 = one LOW cutoff, no column dither: thin
// alpha-cut jewelry / hair whose mip-averaged alpha lands between the cutoff and 0.88 casts
// a CONTINUOUS shadow instead of 50% dithered columns, and blended surfaces (pass 1, whose
// legacy minimum_alpha is 0.598) may cast from the cutoff upward. Alpha-MASK materials keep
// the creator's cutoff (pass 0). Uploaded by LLPipeline::uploadShadowAlphaMode at every
// shadow-program bind (incl. rigged variants); initialisers = stock.
uniform int   shadow_alpha_mode = 0;
uniform float shadow_alpha_cutoff = 0.25;
uniform int   shadow_alpha_pass = 0;     // 1 = blended-alpha shadow pass (renderAlphaObjects)

bool actorFxDissolveDiscard(vec3 object_position);

void main()
{
    float alpha = diffuseLookup(vary_texcoord0.xy).a;

    float cut = minimum_alpha;
    if (shadow_alpha_mode == 1)
    {
        if (shadow_alpha_pass == 1)
        {
            cut = min(minimum_alpha, shadow_alpha_cutoff);
        }
        if (alpha < cut)
        {
            discard;
        }
    }
    else
    {
        if (alpha < minimum_alpha)
        {
            discard;
        }
    }

#if !defined(IS_FULLBRIGHT)
    alpha *= vertex_color.a;
#endif

    if (shadow_alpha_mode == 1)
    {
        if (alpha < max(0.05, min(cut, 0.88))) // no dither: continuous coverage above the cutoff
        {
            discard;
        }
    }
    else
    {
        if (alpha < 0.05) // treat as totally transparent
        {
            discard;
        }

        if (alpha < 0.88) // treat as semi-transparent
        {
            if (fract(0.5*floor(target_pos_x / post_pos.w )) < 0.25)
            {
                discard;
            }
        }
    }

    if (actorFxDissolveDiscard(vary_actor_fx_position))
    {
        discard;
    }

    frag_color = vec4(1,1,1,1);
}
