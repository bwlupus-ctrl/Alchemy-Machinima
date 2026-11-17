/** Clone transitions, render side: pure uniform packing helpers (no viewer or GL dependencies).
 *
 * Shared by the viewer uploads (lldrawpool.cpp, llactormover.cpp, pipeline.cpp) and the render gate
 * (scripts/code_tools/clone_transition_render_check.py compiles this header into a native fixture).
 * Contract: alclonetransition.h. PLAN.md rev 3 sections 3.1 / 3.2.
 *
 * Transition coordinate (3.1): every avatar vertex shader writes the FINAL skinned eye-space position
 * through one affine map (uniform transitionEyeToBody) into body space,
 *     body = S^-1 * Rz(-heading) * (agent - rootAgent),    agent = inverse(view) * eye.
 * `view` is the pass view matrix (gGLModelView): beauty, every shadow cascade/face, velocity, silhouette
 * cover and the overlay all skin into view-relative eye space, so the same body point gets the same
 * coordinate in every program family.
 */
#ifndef AL_CLONETRANSITIONRENDER_H
#define AL_CLONETRANSITIONRENDER_H
#include "alclonetransition.h"
#include <cmath>
#include <cstdint>

namespace ALCloneTransitionRender
{
// General 4x4 inverse (column-major, as GL). Returns false for a singular matrix.
inline bool invert4(const float m[16], float out[16])
{
    float inv[16];
    inv[0] = m[5]*m[10]*m[15] - m[5]*m[11]*m[14] - m[9]*m[6]*m[15] + m[9]*m[7]*m[14] + m[13]*m[6]*m[11] - m[13]*m[7]*m[10];
    inv[4] = -m[4]*m[10]*m[15] + m[4]*m[11]*m[14] + m[8]*m[6]*m[15] - m[8]*m[7]*m[14] - m[12]*m[6]*m[11] + m[12]*m[7]*m[10];
    inv[8] = m[4]*m[9]*m[15] - m[4]*m[11]*m[13] - m[8]*m[5]*m[15] + m[8]*m[7]*m[13] + m[12]*m[5]*m[11] - m[12]*m[7]*m[9];
    inv[12] = -m[4]*m[9]*m[14] + m[4]*m[10]*m[13] + m[8]*m[5]*m[14] - m[8]*m[6]*m[13] - m[12]*m[5]*m[10] + m[12]*m[6]*m[9];
    inv[1] = -m[1]*m[10]*m[15] + m[1]*m[11]*m[14] + m[9]*m[2]*m[15] - m[9]*m[3]*m[14] - m[13]*m[2]*m[11] + m[13]*m[3]*m[10];
    inv[5] = m[0]*m[10]*m[15] - m[0]*m[11]*m[14] - m[8]*m[2]*m[15] + m[8]*m[3]*m[14] + m[12]*m[2]*m[11] - m[12]*m[3]*m[10];
    inv[9] = -m[0]*m[9]*m[15] + m[0]*m[11]*m[13] + m[8]*m[1]*m[15] - m[8]*m[3]*m[13] - m[12]*m[1]*m[11] + m[12]*m[3]*m[9];
    inv[13] = m[0]*m[9]*m[14] - m[0]*m[10]*m[13] - m[8]*m[1]*m[14] + m[8]*m[2]*m[13] + m[12]*m[1]*m[10] - m[12]*m[2]*m[9];
    inv[2] = m[1]*m[6]*m[15] - m[1]*m[7]*m[14] - m[5]*m[2]*m[15] + m[5]*m[3]*m[14] + m[13]*m[2]*m[7] - m[13]*m[3]*m[6];
    inv[6] = -m[0]*m[6]*m[15] + m[0]*m[7]*m[14] + m[4]*m[2]*m[15] - m[4]*m[3]*m[14] - m[12]*m[2]*m[7] + m[12]*m[3]*m[6];
    inv[10] = m[0]*m[5]*m[15] - m[0]*m[7]*m[13] - m[4]*m[1]*m[15] + m[4]*m[3]*m[13] + m[12]*m[1]*m[7] - m[12]*m[3]*m[5];
    inv[14] = -m[0]*m[5]*m[14] + m[0]*m[6]*m[13] + m[4]*m[1]*m[14] - m[4]*m[2]*m[13] - m[12]*m[1]*m[6] + m[12]*m[2]*m[5];
    inv[3] = -m[1]*m[6]*m[11] + m[1]*m[7]*m[10] + m[5]*m[2]*m[11] - m[5]*m[3]*m[10] - m[9]*m[2]*m[7] + m[9]*m[3]*m[6];
    inv[7] = m[0]*m[6]*m[11] - m[0]*m[7]*m[10] - m[4]*m[2]*m[11] + m[4]*m[3]*m[10] + m[8]*m[2]*m[7] - m[8]*m[3]*m[6];
    inv[11] = -m[0]*m[5]*m[11] + m[0]*m[7]*m[9] + m[4]*m[1]*m[11] - m[4]*m[3]*m[9] - m[8]*m[1]*m[7] + m[8]*m[3]*m[5];
    inv[15] = m[0]*m[5]*m[10] - m[0]*m[6]*m[9] - m[4]*m[1]*m[10] + m[4]*m[2]*m[9] + m[8]*m[1]*m[6] - m[8]*m[2]*m[5];
    const float det = m[0]*inv[0] + m[1]*inv[4] + m[2]*inv[8] + m[3]*inv[12];
    if (!(std::fabs(det) > 1e-20f) || !std::isfinite(det)) return false;
    const float rd = 1.f / det;
    for (int i = 0; i < 16; ++i) out[i] = inv[i] * rd;
    return true;
}

// Agent-space point -> body space (the reference the GLSL matrix must reproduce).
inline void bodyFromAgent(const ALCloneTransition::BodyFrame& f, const float agent[3], float body[3])
{
    const float s = f.scale > 1e-6f ? f.scale : 1.f;
    const float dx = agent[0] - f.rootAgent[0], dy = agent[1] - f.rootAgent[1], dz = agent[2] - f.rootAgent[2];
    const float c = std::cos(f.heading), sn = std::sin(f.heading);
    body[0] = (c * dx + sn * dy) / s;
    body[1] = (-sn * dx + c * dy) / s;
    body[2] = dz / s;
}

// Column-major agent -> body matrix for a frame.
inline void agentToBody(const ALCloneTransition::BodyFrame& f, float out[16])
{
    const float s = f.scale > 1e-6f ? f.scale : 1.f;
    const float c = std::cos(f.heading), sn = std::sin(f.heading);
    const float rx = f.rootAgent[0], ry = f.rootAgent[1], rz = f.rootAgent[2];
    const float m[16] = {
        c / s, -sn / s, 0.f, 0.f,      // column 0
        sn / s, c / s, 0.f, 0.f,       // column 1
        0.f, 0.f, 1.f / s, 0.f,        // column 2
        -(c * rx + sn * ry) / s, -(-sn * rx + c * ry) / s, -rz / s, 1.f};
    for (int i = 0; i < 16; ++i) out[i] = m[i];
}

// out = agentToBody(frame) * inverse(view), column-major. false when `view` is singular (caller uploads
// the transition as inactive for that draw: never feed garbage coordinates to a mask).
inline bool packEyeToBody(const ALCloneTransition::RenderParams& r, const float view[16], float out[16])
{
    float inv[16], a2b[16];
    if (!invert4(view, inv)) return false;
    agentToBody(r.frame, a2b);
    for (int col = 0; col < 4; ++col)
        for (int row = 0; row < 4; ++row)
        {
            float v = 0.f;
            for (int k = 0; k < 4; ++k) v += a2b[k * 4 + row] * inv[col * 4 + k];
            out[col * 4 + row] = v;
        }
    return true;
}

// Everything the fragment side reads. active == false (or style NONE) packs all zeros: the per-draw reset.
struct Uniforms
{
    float transition[4] = {0.f, 0.f, 0.f, 0.f};   // actorFxTransition: style, p, side, direction
    float transition2[4] = {0.f, 0.f, 0.f, 0.f};  // actorFxTransition2: bodyHeight, edgeWidth, seed lo16, seed hi16
    float edge[4] = {0.f, 0.f, 0.f, 0.f};         // actorFxTransitionEdge: rgb, intensity (shell only)
    uint32_t seed = 0;                            // also carried (exactly) in transition2.zw
    bool active = false;
};

inline Uniforms packUniforms(const ALCloneTransition::RenderParams& r)
{
    Uniforms u;
    if (!r.active || r.style == ALCloneTransition::STYLE_NONE) return u;
    u.active = true;
    u.transition[0] = float(r.style);
    u.transition[1] = r.p < 0.f ? 0.f : (r.p > 1.f ? 1.f : r.p);
    u.transition[2] = float(r.side);
    u.transition[3] = float(r.direction);
    u.transition2[0] = r.bodyHeight;
    u.transition2[1] = r.edgeWidth;
    u.edge[0] = r.edgeColor[0]; u.edge[1] = r.edgeColor[1]; u.edge[2] = r.edgeColor[2];
    u.edge[3] = r.edgeIntensity;
    u.seed = r.seed;
    u.transition2[2] = float(r.seed & 0xFFFFu);   // exact in a float: 16-bit halves
    u.transition2[3] = float(r.seed >> 16);
    return u;
}
}
#endif
