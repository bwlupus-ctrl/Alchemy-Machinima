/**
 * @file tronTrailV.glsl
 *
 * [TronT3] Light-cycle trail ribbons: vertex stage. Fed by LLRender
 * immediate mode (gGL.begin(TRIANGLE_STRIP) / texCoord2f / vertex3f) from
 * ALTron::renderTrails() inside LLPipeline::renderTronWorld(), drawn into
 * the mWaterDis scratch after the Tron World fullscreen draw (or after the
 * TRAILS_ONLY colour copy-through). See scratchpad/tron_t3_contract.md.
 *
 * `position` is CAMERA-RELATIVE (the host subtracts the pass camera's
 * global position in double before uploading F32) and the loaded modelview
 * is the pass's view matrix with its translation column zeroed, so
 *   modelview * [position, 1] == R * (p_agent - cam_agent)
 * exactly up to float rounding of a small vector -- no 4 km-altitude jitter,
 * and a region crossing / agent-origin shift changes nothing. Both matrices
 * are uploaded by LLRender::syncMatrices() from the gGL stack.
 *
 * `texcoord0.x` = fade01 (1 = fresh point .. 0 = about to expire; computed
 * on the CPU from the shared Tron clock so pause freezes it),
 * `texcoord0.y` = across01 (0 = base / edge A .. 1 = top / edge B).
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

uniform mat4 modelview_projection_matrix;
uniform mat4 modelview_matrix;

in vec3 position;     // camera-relative metres
in vec2 texcoord0;    // x = fade01, y = across01

out vec2  vary_texcoord0;
out float vary_zc;    // view-space depth (m, >= 0) for the analytic fog

void main()
{
    vec4 pos       = vec4(position.xyz, 1.0);
    gl_Position    = modelview_projection_matrix * pos;
    vary_zc        = max(-(modelview_matrix * pos).z, 0.0);
    vary_texcoord0 = texcoord0;
}
