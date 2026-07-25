/**
 * SL_GBufferProvider.fx
 *
 * Feeds the SL viewer's REAL G-buffer (via the sl_reshade_bridge add-on) into
 * MartysMods iMMERSE, replacing Launchpad's depth-derived normals with true
 * gbuffer normals. This is the bridge from "debug view works" to "RTGI/MXAO use
 * real SL normals."
 *
 * ── PROVIDER MODE (contract v2 section 7.3) ─────────────────────────────────
 * Two DIFFERENT contracts, selected explicitly — never an accident of which
 * effects happen to be loaded. SL_PROVIDER_MODE is a PREPROCESSOR define
 * because the distinction is structural: RenderTargetWriteMask is fixed pass
 * state (a uniform cannot change it), and OWNED must force the motion/albedo
 * passes to exist at all. Set it in ReShade's per-effect preprocessor UI.
 *
 *   SL_PROVIDER_MODE 0  COEXIST (default — today's behaviour):
 *     Launchpad owns initial shared-resource contents. This effect may discard
 *     ONLY where explicit validity is false; wherever it discards, Launchpad's
 *     estimate survives. NormalsTexV3 is written .xy-only (write mask 3) so
 *     Launchpad's smooth .zw geometry normal stays intact — clobbering .zw
 *     caused sparkle, so the mask is a correctness fix in this mode.
 *
 *   SL_PROVIDER_MODE 1  OWNED (Launchpad retired / absent):
 *     This effect owns every destination and writes or clears EVERY pixel
 *     EVERY frame; no discard may rely on Launchpad having written anything.
 *     NormalsTexV3 .zw (the smooth geometry normal consumers rely on) is
 *     GENERATED here — a 5-tap average (centre + four DIAGONAL taps) of
 *     decoded SL normals — because in cold absence nothing else writes it
 *     (the v2 retirement blocker).
 *     Write mask is full in this mode.
 *
 * ── INSTALL ──────────────────────────────────────────────────────────────────
 *   Place this file in the iMMERSE shader folder, next to
 *   MartysMods_LAUNCHPAD.fx (so the ".\MartysMods\" includes resolve). Keep
 *   SL_Bridge.fxh (ships with the add-on) on a ReShade effect search path.
 *
 * ── EFFECT ORDER (ReShade list, top → bottom) ────────────────────────────────
 *   iMMERSE: Launchpad            (COEXIST: enabled + first; OWNED: absent)
 *   SL G-Buffer Provider          ← THIS: writes the Deferred:: resources
 *   iMMERSE Pro: RTGI / MXAO ...  (read what we just wrote)
 *   In COEXIST, if placed above Launchpad, Launchpad overwrites us.
 *
 * ── SAFETY ───────────────────────────────────────────────────────────────────
 *   Gating is EXPLICIT (contract v2 section 4): each pass keys off the
 *   SL_SemValid() bits the add-on pushes, never off payload values (zero
 *   motion, black albedo and (0,0) octahedral are all legal data). The albedo
 *   pass adds ONE per-pixel gate that is NOT payload inference:
 *   SL_ALBEDO_FLAG_COVERAGE_GATE reads the viewer's GBUFFER_FLAG_*
 *   classification channel (normals .w — metadata the viewer writes on
 *   purpose, not a data value abused as a sentinel), superseded by
 *   SURFACE_COVERAGE the moment the viewer publishes it — see the block
 *   comment above PS_ProvideAlbedo. With the bridge dead, a
 *   1.0 viewer, or an old add-on that doesn't push the bits, SL_SemanticValid
 *   reads 0 and, in COEXIST, every pass discards — a pure no-op, exactly as
 *   before. In OWNED there is no Launchpad to fall back to, so invalid
 *   semantics write deterministic neutrals instead.
 *
 * ── IP ───────────────────────────────────────────────────────────────────────
 *   Uses the shared Deferred:: interface by #including Marty's PUBLIC headers,
 *   exactly as RTGI does. Contains none of his shader source.
 */

// NOTE: do NOT include ReShade.fxh here. Modern ReShade.fxh defines
// BUFFER_PIXEL_SIZE / BUFFER_SCREEN_SIZE / BUFFER_ASPECT_RATIO as MACROS, while
// Marty's mmx_global.fxh declares them as `static const` — including ReShade.fxh
// first makes the preprocessor rewrite those declarations into
// `static const float2 float2(...)` (X3000 syntax errors). Launchpad's headers
// are self-sufficient; we mirror that and provide our own fullscreen VS below.
#include "MartysMods/mmx_global.fxh"
#include "MartysMods/mmx_math.fxh"
#include "MartysMods/mmx_deferred.fxh"
#include "SL_Bridge.fxh"

// Standard fullscreen-triangle VS (same body as ReShade's PostProcessVS).
void SL_FullscreenVS(in uint id : SV_VertexID,
                     out float4 pos : SV_Position, out float2 uv : TEXCOORD)
{
    uv.x = (id == 2) ? 2.0 : 0.0;
    uv.y = (id == 1) ? 2.0 : 0.0;
    pos  = float4(uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
}

// Provide real motion vectors from the viewer's velocity buffer
// (BDMergeVelocityBuffer / A5.4). Gated on SL_SEM_VALID_MOTION: when the
// viewer's velocity buffer is off the bit is clear and (in COEXIST) the pass
// is a pure no-op, preserving Launchpad's estimated motion.
#ifndef SL_PROVIDE_MOTION
#define SL_PROVIDE_MOTION 1
#endif

// Provide real unlit albedo into Deferred::AlbedoTex so RTGI bounces true
// surface color (colored GI) instead of Launchpad's lit-backbuffer estimate.
// Gated on SL_SEM_VALID_ALBEDO (frame level) + SL_SURFACE_COVERAGE (per pixel,
// when the viewer publishes it).
#ifndef SL_PROVIDE_ALBEDO
#define SL_PROVIDE_ALBEDO 1
#endif

// ── Provider mode ─────────────────────────────────────────────────────────────
// 0 = COEXIST (default, Launchpad present), 1 = OWNED (Launchpad retired).
// See the header block. Must be a preprocessor define — see rationale there.
#ifndef SL_PROVIDER_MODE
#define SL_PROVIDER_MODE 0
#endif
#define SL_MODE_OWNED (SL_PROVIDER_MODE != 0)

#if SL_MODE_OWNED
// OWNED owns EVERY destination — the motion and albedo passes cannot be
// compiled out, or cold absence would leave those resources undefined.
#undef  SL_PROVIDE_MOTION
#define SL_PROVIDE_MOTION 1
#undef  SL_PROVIDE_ALBEDO
#define SL_PROVIDE_ALBEDO 1
#endif

// iMMERSE mixes albedo with linear GI, so it wants LINEAR albedo. The add-on
// binds SL_ALBEDO through the _srgb view, so the HARDWARE already decodes
// sRGB->linear on sample — this must stay 0; setting it to 1 decodes TWICE
// (contract v2 section 6.1; gate A1 fails if 1 ever looks better).
#ifndef SL_ALBEDO_TO_LINEAR
#define SL_ALBEDO_TO_LINEAR 0
#endif

// GL eye-space (SL, right-handed, camera looks down -Z) → iMMERSE/D3D view-space
// (left-handed, camera looks down +Z) is a Z negate. This is the one piece that
// warrants in-world confirmation: if RTGI lighting/AO looks inverted (light from
// the wrong side, occlusion in the wrong places), set SL_NORMAL_FLIP_Z to 0.
#ifndef SL_NORMAL_FLIP_Z
#define SL_NORMAL_FLIP_Z 1
#endif

// Diagnostic view. Set SL_ENABLE_DEBUG to 0 for a production build to compile
// out the readback pass entirely (saves a fullscreen pass; the provider then has
// no on-screen output, which is the intended steady state).
#ifndef SL_ENABLE_DEBUG
#define SL_ENABLE_DEBUG 1
#endif

#if SL_ENABLE_DEBUG
// Paint the screen with what iMMERSE will actually read back AFTER our write —
// ground truth for what RTGI consumes.
//   Normals: compare vs SL_BridgeDebug "Normals (decoded)" — same R/G with BLUE
//            inverted = correct (Z-flip); blocky depth-normals = wrong ORDER.
//   Motion:  hue = direction, brightness = speed. Pan the camera and compare to
//            the motion when this effect is OFF (Launchpad's own estimate) — they
//            should point the SAME way. Opposite hue => flip a Motion axis below.
// Any non-Off view also draws a status strip (top-left): 10 semantic-validity
// cells, then a mode cell (blue=COEXIST, orange=OWNED, magenta=FX mode
// disagrees with the viewer's SLRESHADE_CONFIG_PROVIDER_OWNED flag), then a
// reset cell that flashes white on any SL_ResetFlags bit.
uniform int SL_DEBUG_VIEW <
    ui_type = "combo";
    ui_items = "Off\0Normals iMMERSE receives\0Motion iMMERSE receives\0Albedo iMMERSE receives\0";
    ui_label = "DEBUG view";
> = 0;
#endif

// Motion conversion (live-tunable so you can dial signs without a reload).
// SL velocity = (cur_ndc - last_ndc), NDC space, forward. iMMERSE wants
// (prev_uv - cur_uv), UV space (it reprojects with uv + motion). Default maps
// that: flip X for direction, keep Y, x0.5 for NDC->UV (exact algebra, not a
// guess — contract v2 section 3.3). If RTGI ghosts/smears when panning, flip an
// axis; if it under/over-corrects, adjust scale.
uniform float SL_MOTION_SCALE <
    ui_type = "slider"; ui_min = 0.0; ui_max = 2.0; ui_step = 0.05;
    ui_label = "Motion scale"; ui_category = "Motion";
> = 1.0;
uniform bool SL_MOTION_FLIP_X < ui_label = "Motion flip X"; ui_category = "Motion"; > = true;
uniform bool SL_MOTION_FLIP_Y < ui_label = "Motion flip Y"; ui_category = "Motion"; > = false;

float3 sl_gl_to_view(float3 n_gl)
{
#if SL_NORMAL_FLIP_Z
    return float3(n_gl.x, n_gl.y, -n_gl.z);
#else
    return n_gl;
#endif
}

// POINT sampler for the read — matches iMMERSE's sNormalsTexV3. Linear across a
// geometry edge blends two ENCODED normals into a meaningless in-between value
// (edge shimmer); point sampling avoids it. 1:1 pass so this is also exact.
sampler SL_sNormalsPoint
{
    Texture = SLNormalsTex;
    MinFilter = POINT; MagFilter = POINT; MipFilter = POINT;
    AddressU = CLAMP; AddressV = CLAMP;
};

// Decode one SL normal sample into iMMERSE view space. Always the full
// decode → transform → encode chain: a direct .xy copy is NOT available even
// though both ends are octahedral — the Z negation folds hemispheres, which is
// exactly the OctWrap branch (contract v2 section 7.2).
float3 sl_view_normal_at(float2 uv)
{
    float4 raw = tex2Dlod(SL_sNormalsPoint, float4(SL_UV(uv), 0, 0));
    return sl_gl_to_view(SL_DecodeNormal(raw.xy));
}

#if SL_MODE_OWNED
// OWNED-only: the smooth GEOMETRY normal for NormalsTexV3.zw. Launchpad
// normally derives this from depth; in its absence we synthesize a
// low-frequency normal by averaging DECODED view-space normals over 5 taps —
// the CENTRE plus four DIAGONAL neighbours at ±1.5 px (NOT an axial cross;
// earlier comments and reports called this a "cross", which was wrong — the
// diagonal footprint is deliberate, it smooths over a slightly wider area) —
// and renormalizing (averaging ENCODED .xy across edges would be meaningless,
// same reason the samplers are POINT). This is coarser than Launchpad's
// depth-derived smoothing; residual fine-detail shimmer in consumers that
// lean on .zw is a known quality boundary of OWNED mode, validated by gate R1.
float3 sl_smooth_view_normal_at(float2 uv)
{
    static const float2 kTap[5] = {
        float2( 0.0,  0.0),
        float2( 1.5,  1.5), float2(-1.5,  1.5),
        float2( 1.5, -1.5), float2(-1.5, -1.5)
    };
    float3 acc = 0.0;
    [unroll]
    for (int i = 0; i < 5; i++)
    {
        acc += sl_view_normal_at(uv + kTap[i] * BUFFER_PIXEL_SIZE);
    }
    // Opposing decoded normals across the taps can cancel to (near) zero
    // (thin geometry, backface pairs); normalize(0) is UNDEFINED, and OWNED
    // promises every destination pixel is defined. Degenerate sums fall back
    // to the camera-facing neutral N = (0,0,-1) in iMMERSE view space — the
    // same normal the invalid branch encodes as (0.5, 0.5), since the caller
    // stores octahedral_enc(-N) = enc(0,0,1) = (0.5, 0.5).
    return dot(acc, acc) > 1e-8 ? normalize(acc) : float3(0.0, 0.0, -1.0);
}
#endif

// ── Normals ──────────────────────────────────────────────────────────────────
// RenderTarget is Deferred::NormalsTexV3 (RGBA16, DLSS-size): XY = gbuffer
// normal (octahedral), ZW = geometry normal (octahedral). iMMERSE's
// get_normals() returns -octahedral_dec(xy), so to hand it view-space normal N
// we store octahedral_enc(-N).
//   COEXIST: write .xy only (RenderTargetWriteMask=3 on the pass) — Launchpad's
//            smooth .zw geometry normal is left intact so RTGI keeps its stable
//            coarse structure; overwriting .zw caused fine-detail shimmer.
//            Invalid ⇒ discard (Launchpad's normals survive).
//   OWNED:   write all four channels — .zw is synthesized above, because
//            nothing else writes it. Invalid ⇒ deterministic camera-facing
//            neutral (enc(0,0,1) = (0.5, 0.5)), NEVER undefined memory.
void PS_ProvideNormals(in float4 vpos : SV_Position, in float2 uv : TEXCOORD,
                       out float4 o : SV_Target)
{
    // NO payload-based coverage inference here, in EITHER mode (contract v2
    // section 4 — enforced by the 2026-07-25 cross-review). enc = (0,0)
    // decodes to (0,0,-1): a LEGAL view-space normal (backfaces, two-sided
    // materials at grazing angles), not a clear-value sentinel. The removed
    // raw.xy==(0,0) proxy misclassified those pixels and produced WRONG
    // NORMALS — the exact class of bug behind this project's months of
    // grazing-angle corruption. Per-pixel gating uses ONLY the explicit
    // surface_coverage mask; until the viewer publishes it (it does not yet —
    // llreshadebridge.cpp never sets SL_SEM_VALID_SURFACE_COVERAGE), sky /
    // alpha pixels carry the decoded clear value. That coverage gap is
    // accepted: it is the viewer's debt to pay, not this shader's to paper
    // over with inference.
#if SL_MODE_OWNED
    if (!SL_SemValid(SL_SEM_VALID_NORMALS) ||
        (SL_SemValid(SL_SEM_VALID_SURFACE_COVERAGE) && SL_SurfaceCoverage(uv).r < 0.5))
    {
        // Camera-facing neutral: get_normals() = -oct_dec(0.5,0.5) = (0,0,-1).
        // Deterministic — never undefined memory, never a decoded clear value.
        o = float4(0.5, 0.5, 0.5, 0.5);
        return;
    }
    float4 raw  = tex2Dlod(SL_sNormalsPoint, float4(SL_UV(uv), 0, 0));
    float2 enc  = Math::octahedral_enc(-sl_gl_to_view(SL_DecodeNormal(raw.xy)));
    float2 encg = Math::octahedral_enc(-sl_smooth_view_normal_at(uv));
    o = float4(enc, encg);
#else
    // Frame-level gate is EXPLICIT (contract v2 section 4); per-pixel gate is
    // the explicit coverage mask when published, nothing otherwise.
    if (!SL_SemValid(SL_SEM_VALID_NORMALS))
        discard;
    if (SL_SemValid(SL_SEM_VALID_SURFACE_COVERAGE) && SL_SurfaceCoverage(uv).r < 0.5)
        discard;   // explicit per-pixel: not G-buffer covered
    float4 raw = tex2Dlod(SL_sNormalsPoint, float4(SL_UV(uv), 0, 0));
    float2 enc = Math::octahedral_enc(-sl_gl_to_view(SL_DecodeNormal(raw.xy)));
    o = float4(enc, enc);   // only .xy lands (see RenderTargetWriteMask)
#endif
}

#if SL_PROVIDE_MOTION
sampler SL_sMotionPoint
{
    Texture = SLMotionTex;
    MinFilter = POINT; MagFilter = POINT; MipFilter = POINT;
    AddressU = CLAMP; AddressV = CLAMP;
};

// Convert the SL velocity delta to iMMERSE's motion convention.
float2 sl_to_immerse_motion(float2 d_ndc)   // d_ndc = cur_ndc - last_ndc
{
    float2 mv = d_ndc * 0.5 * SL_MOTION_SCALE;   // NDC(-2..2) -> UV(-1..1)
    if (SL_MOTION_FLIP_X) mv.x = -mv.x;          // forward -> backward (prev-cur)
    if (SL_MOTION_FLIP_Y) mv.y = -mv.y;          // GL NDC +Y up -> UV +Y down
    return mv;
}

// RenderTarget is Deferred::MotionVectorsTex (RG16F, full-res).
//   COEXIST invalid frame ⇒ discard everything (Launchpad's motion survives).
//           Valid frame  ⇒ write every pixel: ZERO MOTION IS LEGAL DATA (a
//           static pixel), and the viewer's camera-first velocity pass covers
//           all pixels — the old zero-discard sentinel is gone per contract v2
//           section 4. Per-pixel invalidity (when the viewer publishes
//           motion_meta) still discards, because there the truth is "unknown",
//           not "static".
//   OWNED ⇒ write or clear every pixel; unknown/invalid becomes exactly 0.
void PS_ProvideMotion(in float4 vpos : SV_Position, in float2 uv : TEXCOORD,
                      out float2 o : SV_Target)
{
#if SL_MODE_OWNED
    if (!SL_SemValid(SL_SEM_VALID_MOTION))
    {
        o = 0.0;
        return;
    }
    if (SL_SemValid(SL_SEM_VALID_MOTION_META) && SL_MotionMeta(uv).r < 0.5)
    {
        o = 0.0;   // per-pixel invalid: deterministic zero, not garbage
        return;
    }
    o = sl_to_immerse_motion(tex2Dlod(SL_sMotionPoint, float4(SL_UV(uv), 0, 0)).xy);
#else
    if (!SL_SemValid(SL_SEM_VALID_MOTION))
        discard;
    if (SL_SemValid(SL_SEM_VALID_MOTION_META) && SL_MotionMeta(uv).r < 0.5)
        discard;
    o = sl_to_immerse_motion(tex2Dlod(SL_sMotionPoint, float4(SL_UV(uv), 0, 0)).xy);
#endif
}
#endif

#if SL_PROVIDE_ALBEDO
sampler SL_sAlbedoPoint
{
    Texture = SLAlbedoTex;
    MinFilter = POINT; MagFilter = POINT; MipFilter = POINT;
    AddressU = CLAMP; AddressV = CLAMP;
};

// ── Visible-diffuse sidecar (ABI 1.2, RenderVisibleDiffuseSidecar on) ────────
// Highest-trust albedo source: the viewer's own answer to "what diffuse
// reflectance is VISIBLE at this pixel", with per-pixel exactness K in .a
// (1 = fully known, 0 = unwritten/sky; attenuated through semi-transparent
// forward layers). Where K clears the threshold the sidecar RGB supersedes
// every gate below -- it already resolves forward-over-opaque, sky and metal
// classification that the coverage/flag gates only approximate. Where K falls
// short, or the semantic is invalid (old viewer / setting off / bridge dead),
// the pass falls through to the pre-existing gate chain.
//
// ⚠️ SCOPE NOTE (FX4). That fallback is BEHAVIOURALLY equivalent, not
// byte-identical, and an earlier version of this comment claimed otherwise.
// PS_ProvideAlbedo was genuinely restructured to put the sidecar branch ahead
// of every pre-existing gate, so:
//   - When the VISIBLE_DIFFUSE semantic is INVALID the sidecar block is not
//     entered at all and the remaining chain is the original code, unchanged.
//     That case is exact.
//   - When the semantic is VALID but K falls below the threshold, control
//     reaches the original chain having already sampled the sidecar. The
//     result is the same; the instruction sequence is not.
// Anyone auditing "does enabling the bridge change the incumbent path" should
// read that as: identical when the semantic is invalid, equivalent-but-
// restructured otherwise.
// Live uniform, not a define: the threshold is the first thing anyone will
// want to sweep, and it must be tunable without an effect reload.
// ui_min is deliberately one UNORM step (1/255), NOT 0.0. K is stored in an
// 8-bit UNORM alpha channel where K = 0 means "no answer" -- the sidecar
// explicitly declining to classify that pixel. With ui_min = 0.0 and a
// >= comparison, dragging the slider to the bottom makes every no-answer pixel
// authoritative, which is the exact inversion of what the exactness channel is
// for. The floor keeps "no answer" unreachable at any slider position.
#define SL_VISDIFF_K_MIN (1.0 / 255.0)

uniform float SL_VISDIFF_K_THRESHOLD <
    ui_type = "slider"; ui_min = 0.00392156862; ui_max = 1.0; ui_step = 0.01;
    ui_label = "Visible-diffuse exactness (K) threshold";
    ui_tooltip = "Sidecar pixels with exactness K >= this write their diffuse "
                 "RGB directly; below it the pass falls back to the coverage-"
                 "gated G-buffer albedo. 0.5 = trust only majority-known pixels.";
    ui_category = "Albedo";
> = 0.5;

// ── Per-pixel coverage via the viewer's G-buffer FLAG channel ────────────────
// SL_ALBEDO_FLAG_COVERAGE_GATE: the normals attachment's .w carries the
// viewer's own surface classification (llshadermgr.cpp:645-648):
//   GBUFFER_FLAG_SKIP_ATMOS  0.0   sky, clouds, stars, moon, sun disc, ...
//   GBUFFER_FLAG_HAS_ATMOS   0.34  ordinary deferred geometry
//   GBUFFER_FLAG_HAS_PBR     0.67  PBR geometry
//   GBUFFER_FLAG_HAS_HDRI    1.0   HDRI sky
// This is deliberate METADATA the viewer writes every frame — not a payload
// value abused as a sentinel — so gating on it does not violate contract v2
// section 4. It replaces the retired SL_COEXIST_ALBEDO_BLACK_PROXY, which WAS
// a section-4 violation: black is legal albedo, and the proxy misclassified
// genuinely black materials as "unwritten" (the primary reason it had to go).
// The flag gate additionally catches what the black test could never see:
// HDRI sky (skyF.glsl:131 writes HAS_HDRI, albedo non-black), unwritten /
// cleared pixels, and — in the non-default RenderEnableEmissiveBuffer=off
// config only — WL sky radiance landing in attachment 0 (skyF.glsl:220; the
// DEFAULT emissive-on path writes frag_data[0]=vec4(0) at :216-218).
//
// ⛔ Do NOT "simplify" this to `w > 0.0`. The viewer's G-buffer clear is
// glClearColor(1,0,1,1) (llviewerdisplay.cpp:1020) and bindTarget() clears
// ALL attachments (llrendertarget.cpp:533-585), so UNWRITTEN pixels carry
// .w = 1.0 — colliding exactly with HAS_HDRI. `w > 0` admits both unwritten
// pixels and HDRI sky, and it LOOKS like it works because sky is usually
// behind geometry. Only the windowed two-bucket test below is sound. Windowed
// compare, never equality: equality breaks in both G-buffer formats, while
// the window is precision-safe in both — even 2-bit GL_RGB10_A2 alpha
// quantizes to exactly {0, 1/3, 2/3, 1}, and 0.34/0.67 land 0.0067/0.0033
// from 1/3 and 2/3, far inside the ±0.1 window, with level spacing over
// twice the window so buckets cannot collide.
//
// KNOWN LIMIT (documented, not solved here): forward-over-opaque. Alpha-
// blended, water and fullbright passes never write frag_data[2], so .w still
// holds the flag of the opaque surface BEHIND them; the gate passes and
// serves the behind-surface albedo. Fixing that is the explicit
// surface_coverage mask's job — which is also why this gate is
// unconditionally superseded the moment SL_SEM_VALID_SURFACE_COVERAGE is set
// (the explicit-coverage branch is checked first, the gate lives in its
// `else`). The viewer does not publish coverage yet (llreshadebridge.cpp
// never sets SL_SEM_VALID_SURFACE_COVERAGE); delete this define when it does.
#ifndef SL_ALBEDO_FLAG_COVERAGE_GATE
#define SL_ALBEDO_FLAG_COVERAGE_GATE 1
#endif

#if SL_ALBEDO_FLAG_COVERAGE_GATE
// True where the flag channel says a deferred surface was written this frame
// (HAS_ATMOS or HAS_PBR bucket). POINT-sampled: linear filtering across a
// flag boundary would blend buckets into meaningless in-between values, same
// reason the normal samplers are POINT.
bool sl_flag_covered(float2 uv)
{
    float w = tex2Dlod(SL_sNormalsPoint, float4(SL_UV(uv), 0, 0)).w;
    return abs(w - 0.34) < 0.1 || abs(w - 0.67) < 0.1;
}
#endif

// RenderTarget is Deferred::AlbedoTex (RGBA16F, full-res). G-buffer albedo is
// NOT valid everywhere (contract v2 section 2.4): sky, alpha-blended, water,
// fullbright and HUD pixels hold the clear value or the opaque surface BEHIND —
// so per-pixel coverage matters, not just the frame-level bit.
//   Per-pixel gate: the viewer's surface_coverage mask when published
//   (SL_SEM_VALID_SURFACE_COVERAGE); until then, the G-buffer FLAG gate above.
//   COEXIST uncovered ⇒ discard (Launchpad's estimate survives — the right
//   fallback). OWNED has no incumbent to fall back to, so uncovered becomes
//   the same deterministic black as invalid: no bounce light beats serving
//   sky/behind-surface color as albedo, and a discard there would leave the
//   pixel undefined, breaking OWNED's every-pixel-defined promise.
//   OWNED invalid/uncovered ⇒ deterministic black (no bounce light), never
//   undefined, never Launchpad-dependent, and never payload-inferred (the
//   flag channel is viewer-written metadata, not payload — see the gate
//   block above).
void PS_ProvideAlbedo(in float4 vpos : SV_Position, in float2 uv : TEXCOORD,
                      out float3 o : SV_Target)
{
    // Priority 1 (both modes): the visible-diffuse sidecar, where its
    // exactness clears the threshold. RGB is already LINEAR (the add-on binds
    // the GL_SRGB8_ALPHA8 source through an _srgb view -- hardware decode on
    // sample, same as SL_ALBEDO) and is used AS-IS: never divide by K, never
    // re-decode. K = 1 with black RGB is a metal -- a real answer, written.
    // Below-threshold pixels fall through to the existing gates: in COEXIST
    // they can still discard (incumbent estimate survives), in OWNED they
    // still resolve to G-buffer albedo or deterministic black.
    if (SL_SemValid(SL_SEM_VALID_VISIBLE_DIFFUSE))
    {
        float4 vd = SL_VisibleDiffuse(uv);   // POINT sampler (SL_Bridge.fxh)
        // max() so a preset saved with an older build (or a hand-edited ini)
        // that stored 0.0 cannot resurrect the K = 0 hole: the UI floor alone
        // is not a guarantee, because the value is loaded from disk. (FX2)
        if (vd.a >= max(SL_VISDIFF_K_THRESHOLD, SL_VISDIFF_K_MIN))
        {
            o = vd.rgb;
            return;
        }
    }
#if SL_MODE_OWNED
    if (!SL_SemValid(SL_SEM_VALID_ALBEDO))
    {
        o = 0.0;
        return;
    }
    if (SL_SemValid(SL_SEM_VALID_SURFACE_COVERAGE))
    {
        if (SL_SurfaceCoverage(uv).r < 0.5)
        {
            o = 0.0;   // not covered by the G-buffer: defined black, not behind-surface color
            return;
        }
    }
#if SL_ALBEDO_FLAG_COVERAGE_GATE
    else if (!sl_flag_covered(uv))
    {
        o = 0.0;   // flag says no deferred surface here: defined black, not sky/clear color
        return;
    }
#endif
    float3 a = tex2Dlod(SL_sAlbedoPoint, float4(SL_UV(uv), 0, 0)).rgb;
#else
    if (!SL_SemValid(SL_SEM_VALID_ALBEDO))
        discard;
    if (SL_SemValid(SL_SEM_VALID_SURFACE_COVERAGE))
    {
        if (SL_SurfaceCoverage(uv).r < 0.5)
            discard;   // explicit per-pixel: not G-buffer covered
    }
#if SL_ALBEDO_FLAG_COVERAGE_GATE
    else if (!sl_flag_covered(uv))
    {
        discard;       // no deferred surface written — Launchpad's estimate survives
    }
#endif
    float3 a = tex2Dlod(SL_sAlbedoPoint, float4(SL_UV(uv), 0, 0)).rgb;
#endif
#if SL_ALBEDO_TO_LINEAR
    a = pow(a, 2.2);   // sRGB -> approx linear (MUST STAY OFF — double decode)
#endif
    o = a;
}
#endif

#if SL_ENABLE_DEBUG
// Diagnostic pass: reads back what we wrote (via iMMERSE's own accessors) and
// paints it to the backbuffer. Discards when Off (no-op).
float3 PS_ShowReceived(in float4 vpos : SV_Position, in float2 uv : TEXCOORD) : SV_Target
{
    float3 o = 0.0;
    if (SL_DEBUG_VIEW == 1)          // normals iMMERSE receives
    {
        o = Deferred::get_normals(uv) * 0.5 + 0.5;
    }
    else if (SL_DEBUG_VIEW == 2)     // motion iMMERSE receives (hue=dir, bright=speed)
    {
        float2 m = Deferred::get_motion(uv);
        float  ang = atan2(m.y, m.x);
        float3 rgb = saturate(3.0 * abs(2.0 * frac(ang / 6.2831853 + float3(0.0, -1.0/3.0, 1.0/3.0)) - 1.0) - 1.0);
        o = lerp(0.5, rgb, saturate(length(m) * 250.0));
    }
    else if (SL_DEBUG_VIEW == 3)     // albedo iMMERSE receives (unlit surface color)
    {
        o = Deferred::get_albedo(uv);
    }
    else                             // Off → don't touch the backbuffer
    {
        discard;
    }

    // Status strip (top-left): cells 0-9 = SL_SEM_VALID_* bits in bit order
    // (green=valid, dark red=invalid; cell 9 = VISIBLE_DIFFUSE, ABI 1.2),
    // cell 10 = mode (blue=COEXIST, orange=OWNED, magenta=mismatch vs viewer's
    // SLRESHADE_CONFIG_PROVIDER_OWNED), cell 11 = white flash on any reset
    // flag. Gate L1 in a glance.
    if (uv.y < 0.02 && uv.x < 0.30)
    {
        int cell = int(uv.x / 0.025);
        if (cell < 10)
        {
            o = SL_SemValid(1u << uint(cell)) ? float3(0.1, 0.9, 0.2)
                                              : float3(0.45, 0.05, 0.05);
        }
        else if (cell == 10)
        {
            bool viewer_owned = (SL_ConfigFlags & SL_CONFIG_PROVIDER_OWNED) != 0u;
#if SL_MODE_OWNED
            o = viewer_owned ? float3(1.0, 0.55, 0.1) : float3(1.0, 0.0, 1.0);
#else
            o = viewer_owned ? float3(1.0, 0.0, 1.0) : float3(0.15, 0.4, 1.0);
#endif
        }
        else
        {
            o = SL_HistoryResetAny() ? float3(1.0, 1.0, 1.0) : float3(0.1, 0.1, 0.1);
        }
    }
    return o;
}
#endif // SL_ENABLE_DEBUG

technique SL_GBufferProvider <
    ui_tooltip = "Feeds real SL G-buffer data (normals/motion/albedo) from the "
                 "sl_reshade_bridge add-on into the iMMERSE Deferred:: resources.\n"
                 "SL_PROVIDER_MODE 0 (COEXIST): place directly BELOW Launchpad, "
                 "ABOVE RTGI/MXAO; no-op wherever the bridge is invalid.\n"
                 "SL_PROVIDER_MODE 1 (OWNED): Launchpad absent; this effect "
                 "defines every pixel of every destination itself.";
>
{
    pass ProvideNormals
    {
        VertexShader = SL_FullscreenVS;
        PixelShader  = PS_ProvideNormals;
        RenderTarget = Deferred::NormalsTexV3;
#if !SL_MODE_OWNED
        // COEXIST correctness fix, not an optimisation: preserve Launchpad's
        // .zw geometry normal (clobbering it caused sparkle). OWNED writes all
        // channels because it must define .zw itself.
        RenderTargetWriteMask = 3;
#endif
    }
#if SL_PROVIDE_MOTION
    pass ProvideMotion
    {
        VertexShader = SL_FullscreenVS;
        PixelShader  = PS_ProvideMotion;
        RenderTarget = Deferred::MotionVectorsTex;
    }
#endif
#if SL_PROVIDE_ALBEDO
    pass ProvideAlbedo
    {
        VertexShader = SL_FullscreenVS;
        PixelShader  = PS_ProvideAlbedo;
        RenderTarget = Deferred::AlbedoTex;
        // Correctness fix, BOTH modes: the PS outputs float3 but AlbedoTex is
        // RGBA16F, so without the mask .a receives an UNDEFINED value on every
        // covered pixel. iMMERSE's public accessor reads only .rgb, but whether
        // proprietary internals read .a is unknowable — never touch it. In
        // COEXIST this preserves the incumbent's .a (same principle as the
        // mask-3 on ProvideNormals); in OWNED the masked-off channel keeps its
        // deterministic creation-time clear instead of per-frame garbage.
        // (ProvideMotion needs no mask: MotionVectorsTex is two-channel RG16F
        // and the PS writes float2 — full channel coverage already.)
        RenderTargetWriteMask = 7;
    }
#endif
#if SL_ENABLE_DEBUG
    // must be LAST so it reads the final NormalsTexV3; writes backbuffer only
    // when SL_DEBUG_VIEW is not Off. Compiles out entirely when SL_ENABLE_DEBUG=0.
    pass ShowReceived
    {
        VertexShader = SL_FullscreenVS;
        PixelShader  = PS_ShowReceived;
    }
#endif
}
