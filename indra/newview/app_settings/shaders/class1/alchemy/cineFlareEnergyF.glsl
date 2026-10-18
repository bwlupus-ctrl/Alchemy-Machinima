/*[EXTRA_CODE_HERE]*/
uniform sampler2D uCineFlareShaft;
uniform sampler2D uCineFlareHistory;
uniform vec4 uEnergyInputs[20]; // uv, active, reset (stable rig/light index)
uniform vec2 uEnergyResolution;
uniform vec2 uEnergyResponse; // frame-rate independent attack/release alpha
uniform float uCineFlareProbeRadius;
uniform float uCineFlareThreshold;
uniform int uEnergyValid;
out vec4 frag_color;
const vec3 LUMA = vec3(0.2126, 0.7152, 0.0722);
void main()
{
    int slot = int(gl_FragCoord.x);
    vec4 inputSource = uEnergyInputs[slot];
    float target = 0.0;
    if (inputSource.z > 0.0)
    {
        vec2 suv = clamp(inputSource.xy, vec2(0.0), vec2(1.0));
        // Radius is authored as a fraction of screen HEIGHT, then converted
        // back through uEnergyResolution per axis. The resulting ring is circular
        // in pixels instead of stretching on ultrawide frames. Preserve a
        // 3-pixel minimum for sub-pixel emission points when the setting is 0.
        float probe_px = max(uCineFlareProbeRadius * uEnergyResolution.y, 3.0);
        vec2  ring     = probe_px / uEnergyResolution;
        vec2  diag     = ring * 0.70710678;

        float local_lum =          dot(texture(uCineFlareShaft, suv).rgb, LUMA);
        local_lum = max(local_lum, dot(texture(uCineFlareShaft, clamp(suv + vec2( ring.x, 0.0), vec2(0.0), vec2(1.0))).rgb, LUMA));
        local_lum = max(local_lum, dot(texture(uCineFlareShaft, clamp(suv - vec2( ring.x, 0.0), vec2(0.0), vec2(1.0))).rgb, LUMA));
        local_lum = max(local_lum, dot(texture(uCineFlareShaft, clamp(suv + vec2(0.0,  ring.y), vec2(0.0), vec2(1.0))).rgb, LUMA));
        local_lum = max(local_lum, dot(texture(uCineFlareShaft, clamp(suv - vec2(0.0,  ring.y), vec2(0.0), vec2(1.0))).rgb, LUMA));
        local_lum = max(local_lum, dot(texture(uCineFlareShaft, clamp(suv + vec2( diag.x,  diag.y), vec2(0.0), vec2(1.0))).rgb, LUMA));
        local_lum = max(local_lum, dot(texture(uCineFlareShaft, clamp(suv + vec2(-diag.x,  diag.y), vec2(0.0), vec2(1.0))).rgb, LUMA));
        local_lum = max(local_lum, dot(texture(uCineFlareShaft, clamp(suv + vec2( diag.x, -diag.y), vec2(0.0), vec2(1.0))).rgb, LUMA));
        local_lum = max(local_lum, dot(texture(uCineFlareShaft, clamp(suv - vec2( diag.x,  diag.y), vec2(0.0), vec2(1.0))).rgb, LUMA));

        // Soft threshold as a DIRECT luma floor (linear-HDR luma sampled at the
        // shaft emission-face anchor). The previous fixed 2.0 diffuse-white
        // floor was calibrated for the SUN and rejected the far-dimmer projector
        // shafts entirely -- nothing flared even at threshold 0. Because the
        // anchor is now the shaft near-plane (setupSpotLightVolumetric geometry)
        // rather than an arbitrary surface, gating on the luma THERE keys on the
        // actual volumetric source: raise uCineFlareThreshold to require a
        // brighter shaft, lower toward 0 to catch faint ones. A small knee keeps
        // low thresholds responsive.
        float knee = max(uCineFlareThreshold * 0.5, 0.1);
        float gate = smoothstep(uCineFlareThreshold, uCineFlareThreshold + knee, local_lum);
        float rig_energy = gate * min(local_lum / max(uCineFlareThreshold + knee, 1e-3), 4.0);

        target = rig_energy * inputSource.z;
    }
    if (isnan(target) || isinf(target)) target = 0.0;
    float previous = 0.0;
    if (uEnergyValid != 0 && inputSource.w == 0.0)
        previous = texelFetch(uCineFlareHistory, ivec2(slot, 0), 0).r;
    if (isnan(previous) || isinf(previous)) previous = target;
    float response = target > previous ? uEnergyResponse.x : uEnergyResponse.y;
    frag_color = vec4(mix(previous, target, response), 0.0, 0.0, 1.0);
}
