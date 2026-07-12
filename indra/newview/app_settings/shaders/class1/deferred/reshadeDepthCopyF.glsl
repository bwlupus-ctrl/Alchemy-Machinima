/**
 * @file reshadeDepthCopyF.glsl
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Second Life Viewer Source Code
 * $/LicenseInfo$
 *
 * [RTGI] Copy the scene's raw hardware depth into an R32F COLOR texture that
 * ReShade can sample for its DEPTH semantic. A raw GL depth-format texture bound
 * directly to ReShade samples flat/constant (depth-format sampling quirks), and
 * ReShade's own generic_depth detection is unreliable for OpenGL apps like SL.
 * This is generic_depth's copy trick applied to the engine's EXACT, known scene
 * depth: output the raw [0,1] window-space depth; ReShade then applies its own
 * RESHADE_DEPTH_INPUT_* linearization (REVERSED/UPSIDE_DOWN/etc.) as usual.
 */

/*[EXTRA_CODE_HERE]*/

out vec4 frag_color;

uniform sampler2D depthMap;

in vec2 vary_fragcoord;

void main()
{
    // Raw window-space depth in R; ReShade reads .x and linearizes.
    float d = texture(depthMap, vary_fragcoord.xy).r;
    frag_color = vec4(d, 0.0, 0.0, 1.0);
}
