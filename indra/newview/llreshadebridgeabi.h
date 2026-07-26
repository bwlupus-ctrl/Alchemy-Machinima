/**
 * @file llreshadebridgeabi.h
 * @brief C ABI shared between the viewer and the sl_reshade_bridge ReShade
 *        add-on DLL. This header is the ENTIRE coupling surface between the
 *        two modules -- keep it pure C, self-contained, and versioned.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Phoenix / Firestorm Viewer Source Code
 * $/LicenseInfo$
 *
 * CONTRACT
 * --------
 * The viewer executable exports one function:
 *
 *     extern "C" const SLReShadeFrame* SLReShade_GetFrame(uint32_t requested_abi);
 *
 * The add-on (loaded by ReShade into the same process) resolves it with
 * GetProcAddress(GetModuleHandleW(NULL), SLRESHADE_EXPORT_NAME) and calls it
 * once per frame. The returned pointer is stable for the lifetime of the
 * process; only the pointed-to contents change. Returns NULL if
 * requested_abi's major version (high 16 bits) does not match, or if the
 * bridge is disabled -- the add-on must fail soft to a no-op in that case.
 *
 * THREADING / TEAR PROTECTION (seqlock)
 * -------------------------------------
 * In practice ReShade's GL effect pass runs inside SwapBuffers on the same
 * thread that writes this struct, so reads and writes never overlap. The
 * seqlock is belt-and-suspenders for any future thread the add-on might poll
 * from. Reader protocol:
 *
 *     do {
 *         uint32_t end = frame->write_end;      // read END first
 *         <copy the struct>
 *         uint32_t begin = frame->write_begin;  // then BEGIN
 *     } while (begin != end);                   // retry on torn read
 *
 * The writer increments write_begin (with a release barrier) BEFORE touching
 * the payload and sets write_end = write_begin AFTER. Equal counters ==
 * consistent snapshot.
 *
 * LIFETIME OF GL NAMES
 * --------------------
 * The GL texture names published here belong to the viewer and may be
 * DELETED AND REALLOCATED whenever the window resizes or graphics settings
 * change. They are only guaranteed alive during the frame in which they were
 * published (frame_counter unchanged). The add-on must never cache them
 * across frames; re-read every frame and compare {gl_name, width, height,
 * gl_internal_format} before reuse. The published textures are level-0-only
 * mutable textures -- NOT mipmap-complete. Copying from them is fine;
 * binding them directly to ReShade effect samplers is NOT (samples black --
 * see doc/RESHADE_BRIDGE_ROOT_CAUSE.md on the alchemy archive branch).
 * Copy into add-on-owned immutable resources instead.
 */

#ifndef LL_LLRESHADEBRIDGEABI_H
#define LL_LLRESHADEBRIDGEABI_H

#include <stdint.h>
#include <string.h>     /* memset/memcpy for the v1.1 tail helpers */

#ifdef __cplusplus
#define SLRESHADE_INLINE inline
#else
#define SLRESHADE_INLINE static inline
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* 'SLRB' -- sanity check that the pointer really is ours. */
#define SLRESHADE_ABI_MAGIC     0x534C5242u

/* Version packs MAJOR (high 16, breaking layout changes) and MINOR (low 16,
 * append-only additions inside the reserved tail). */
#define SLRESHADE_ABI_MAJOR     1u
#define SLRESHADE_ABI_MINOR     2u
#define SLRESHADE_ABI_VERSION   ((SLRESHADE_ABI_MAJOR << 16) | SLRESHADE_ABI_MINOR)

/* Minor at which the v1.1 tail (SLReShadeFrameV11Tail, below) became valid.
 * MAJOR is unchanged: the tail lives entirely inside the existing reserved[24],
 * so SLReShadeFrame's layout and size are byte-identical to 1.0 and an old
 * add-on keeps working untouched. Readers MUST gate on minor >= this value. */
#define SLRESHADE_ABI_MINOR_V1_1  1u
#define SLRESHADE_ABI_MINOR_V1_2  2u

#define SLRESHADE_EXPORT_NAME   "SLReShade_GetFrame"

/* flags bits */
#define SLRESHADE_FLAG_VALID        0x1u  /* frame has usable G-buffer data   */
#define SLRESHADE_FLAG_HDR          0x2u  /* normals=RGBA16, screen=RGBA16F   */
#define SLRESHADE_FLAG_SNAPSHOT     0x4u  /* frame rendered for a snapshot    */

/* One published texture. gl_internal_format is the GL internal format enum
 * (e.g. 0x805B GL_RGBA16, 0x881A GL_RGBA16F, 0x8058 GL_RGBA8, 0x81A6
 * GL_DEPTH_COMPONENT24). gl_name == 0 means "not available this frame". */
typedef struct SLReShadeTexture
{
    uint32_t gl_name;
    uint32_t gl_internal_format;
    uint32_t width;
    uint32_t height;
} SLReShadeTexture;

typedef struct SLReShadeFrame
{
    /* --- header / validation ------------------------------------------ */
    uint32_t magic;          /* SLRESHADE_ABI_MAGIC                        */
    uint32_t abi_version;    /* SLRESHADE_ABI_VERSION of the writer        */
    uint32_t struct_size;    /* sizeof(SLReShadeFrame) as the writer built */
    uint32_t flags;          /* SLRESHADE_FLAG_*                           */

    /* --- seqlock (see header comment) ---------------------------------- */
    uint32_t write_begin;
    uint32_t write_end;

    /* --- monotonic frame id (stalls when viewer is minimized/paused) --- */
    uint64_t frame_counter;

    /* --- camera scalars ------------------------------------------------ */
    float    near_clip;      /* meters                                     */
    float    far_clip;       /* meters (tracks RenderFarClip)              */
    float    fov_y;          /* vertical FOV, radians                      */
    float    aspect;         /* width / height                             */

    /* --- world-space camera frame (SL: X fwd, Y left, Z up convention
     *     as reported by LLViewerCamera axes) ---------------------------- */
    float    origin[3];
    float    at_axis[3];
    float    left_axis[3];
    float    up_axis[3];

    /* --- matrices: 16 floats, row-major exactly as LLMatrix4 (mMatrix[r][c],
     *     row-vector convention: v' = v * M). Standard GL depth (NOT
     *     reversed-Z): proj maps near->-1, far->+1 in NDC. ---------------- */
    float    view[16];       /* world -> camera (modelview)                */
    float    proj[16];       /* camera -> clip                             */

    /* --- render target dimensions (all published textures match) ------- */
    uint32_t width;
    uint32_t height;

    /* --- G-buffer / scene textures ------------------------------------- */
    SLReShadeTexture color_hdr;  /* scene color pre-tonemap ("screen" RT)  */
    SLReShadeTexture depth;      /* shared depth (GL_DEPTH_COMPONENT24).
                                    Informational: generic_depth is the
                                    intended depth source for effects.     */
    SLReShadeTexture albedo;     /* deferredScreen 0                       */
    SLReShadeTexture orm;        /* deferredScreen 1 (occ/rough/metal)     */
    SLReShadeTexture normals;    /* deferredScreen 2 (encoded, see below)  */
    SLReShadeTexture emissive;   /* deferredScreen 3 (0 if disabled)       */
    SLReShadeTexture motion;     /* A5.4 velocity (mVelocityMap, GL_RG16F).
                                    Published by THIS (alchemy) tree when
                                    BDMergeVelocityBuffer is on; gl_name 0
                                    when off. The enve tree has no velocity
                                    buffer and always publishes 0.          */

    /* --- append-only growth area (MINOR version bumps) ------------------ */
    uint32_t reserved[24];
} SLReShadeFrame;

/* Note on normals encoding: deferredScreen attachment 2 stores OCTAHEDRAL-
 * encoded VIEW-SPACE normals in .xy; .z is environment intensity, .w is the
 * gbuffer flags byte. The add-on publishes the raw texture; decoding is the
 * consuming shader's job.
 *
 * CORRECTED 2026-07-25 -- this comment previously described SPHEREMAP
 * encoding (fenc = xy*4-2; g = sqrt(1-f/4); n = (fenc*g, 1-f/2)). That was
 * wrong and it propagated into SL_Bridge.fxh's decoder, so consumers were fed
 * incorrect normals at grazing angles for months while looking correct
 * head-on (the two encodings agree only near screen centre). The authoritative
 * source is globalF.glsl encodeNormal/decodeNormal, which is Knarkowicz
 * octahedron encoding:
 *
 *   encode: n /= (|n.x|+|n.y|+|n.z|);
 *           n.xy = n.z >= 0 ? n.xy : OctWrap(n.xy);
 *           store n.xy * 0.5 + 0.5
 *   decode: f = xy*2-1;
 *           n = (f.x, f.y, 1 - |f.x| - |f.y|);
 *           t = clamp(-n.z, 0, 1);
 *           n.xy += (n.x >= 0 ? -t : t, n.y >= 0 ? -t : t);
 *           normalize(n)
 *
 * If you change the viewer's encoding, this comment, SL_Bridge.fxh's
 * SL_DecodeNormal(), and doc/RESHADE_BRIDGE_CONTRACT.md must change together. */

/* ===================================================================== *
 *  ABI 1.1 TAIL -- overlays reserved[24]. See doc/RESHADE_BRIDGE_CONTRACT.md
 *  v2 sections 4 (validity), 5.4 (this tail) and 7.3 (COEXIST/OWNED).
 *
 *  WHY: zero motion, black albedo and (0,0) octahedral are all LEGAL DATA --
 *  a stationary surface, a black material, an extreme normal direction. None
 *  of them means "unfed". Availability and temporal discontinuity therefore
 *  have to be signalled EXPLICITLY; they cannot be inferred from payload.
 * ===================================================================== */

/* Frame-level semantic availability. Per-pixel validity lives in motion_meta
 * / surface_coverage below. */
#define SLRESHADE_SEM_VALID_COLOR_HDR        (1u << 0)
#define SLRESHADE_SEM_VALID_DEPTH            (1u << 1)
#define SLRESHADE_SEM_VALID_ALBEDO           (1u << 2)
#define SLRESHADE_SEM_VALID_ORM              (1u << 3)
#define SLRESHADE_SEM_VALID_NORMALS          (1u << 4)
#define SLRESHADE_SEM_VALID_EMISSIVE         (1u << 5)
#define SLRESHADE_SEM_VALID_MOTION           (1u << 6)
#define SLRESHADE_SEM_VALID_MOTION_META      (1u << 7)
#define SLRESHADE_SEM_VALID_SURFACE_COVERAGE (1u << 8)
#define SLRESHADE_SEM_VALID_VISIBLE_DIFFUSE   (1u << 9)

/* Temporal history discontinuities; OR together. Consumers must reset history. */
#define SLRESHADE_RESET_FIRST_VALID_FRAME    (1u << 0)
#define SLRESHADE_RESET_FRAME_DISCONTINUITY  (1u << 1)
#define SLRESHADE_RESET_CAMERA_CUT           (1u << 2)
#define SLRESHADE_RESET_TELEPORT             (1u << 3)
#define SLRESHADE_RESET_PROJECTION_CHANGE    (1u << 4)
#define SLRESHADE_RESET_RESIZE               (1u << 5)
#define SLRESHADE_RESET_TARGET_RECREATED     (1u << 6)
#define SLRESHADE_RESET_HDR_MODE_CHANGE      (1u << 7)
#define SLRESHADE_RESET_SOURCE_LOSS          (1u << 8)
#define SLRESHADE_RESET_CONTEXT_CHANGE       (1u << 9)
#define SLRESHADE_RESET_SNAPSHOT_TRANSITION  (1u << 10)
#define SLRESHADE_RESET_PAUSE_RESUME         (1u << 11)

/* Declares the exact motion convention rather than leaving it to be inferred. */
#define SLRESHADE_MOTION_ENCODING_UNSPECIFIED                          0u
#define SLRESHADE_MOTION_ENCODING_CUR_NDC_MINUS_PREV_NDC_GL_UNJITTERED 1u
#define SLRESHADE_MOTION_ENCODING_PREV_UV_MINUS_CUR_UV_TOP_LEFT        2u
#define SLRESHADE_MOTION_ENCODING_PREV_PIXEL_MINUS_CUR_PIXEL           3u

/* Explicit orientation facts. Do NOT infer these from config or comments. */
#define SLRESHADE_ORIENT_SOURCE_UV_ORIGIN_BOTTOM_LEFT (1u << 0)
#define SLRESHADE_ORIENT_SOURCE_REQUIRES_V_FLIP       (1u << 1)
#define SLRESHADE_ORIENT_MOTION_NDC_Y_UP              (1u << 2)
#define SLRESHADE_ORIENT_DESTINATION_UV_Y_DOWN        (1u << 3)

/* Diagnostic: which motion categories the viewer attempted this frame. */
#define SLRESHADE_MOTION_COVERAGE_CAMERA_DEPTH   (1u << 0)
#define SLRESHADE_MOTION_COVERAGE_RIGID          (1u << 1)
#define SLRESHADE_MOTION_COVERAGE_RIGGED_MESH    (1u << 2)
#define SLRESHADE_MOTION_COVERAGE_CLASSIC_AVATAR (1u << 3)
#define SLRESHADE_MOTION_COVERAGE_ALPHA_TEST     (1u << 4)
#define SLRESHADE_MOTION_COVERAGE_ALPHA_BLEND    (1u << 5)
#define SLRESHADE_MOTION_COVERAGE_PARTICLES      (1u << 6)
#define SLRESHADE_MOTION_COVERAGE_WATER          (1u << 7)
#define SLRESHADE_MOTION_COVERAGE_SKY            (1u << 8)
#define SLRESHADE_MOTION_COVERAGE_FULLBRIGHT     (1u << 9)

/* Effective configuration actually in force this frame. */
#define SLRESHADE_CONFIG_VELOCITY_ENABLED (1u << 0)
#define SLRESHADE_CONFIG_HDR_ENABLED      (1u << 1)
#define SLRESHADE_CONFIG_SNAPSHOT         (1u << 2)
#define SLRESHADE_CONFIG_FORCE_10BIT      (1u << 3)
#define SLRESHADE_CONFIG_PROVIDER_OWNED   (1u << 4)

/* Exactly 24 uint32 words = 96 bytes, overlaying SLReShadeFrame::reserved.
 *
 * motion_meta      recommended GL_RG8: R = per-pixel motion validity [0,1],
 *                                      G = temporal reactivity / history reject [0,1]
 * surface_coverage recommended GL_RG8: R = deferred/G-buffer visible coverage [0,1],
 *                                      G = forward/transparent/fullbright/water coverage [0,1]
 * visible_diffuse recommended GL_SRGB8_ALPHA8: RGB = visible linear diffuse
 *                                      colour, A = exactness [0,1] */
typedef struct SLReShadeFrameV11Tail
{
    uint32_t semantic_valid_bits;         /* word 0      SLRESHADE_SEM_VALID_*      */
    uint32_t history_reset_flags;         /* word 1      SLRESHADE_RESET_*          */
    uint32_t render_target_generation_lo; /* word 2                                 */
    uint32_t render_target_generation_hi; /* word 3                                 */
    uint32_t motion_encoding;             /* word 4      SLRESHADE_MOTION_ENCODING_**/
    uint32_t source_orientation_flags;    /* word 5      SLRESHADE_ORIENT_*         */
    uint32_t motion_coverage_bits;        /* word 6      SLRESHADE_MOTION_COVERAGE_**/
    uint32_t effective_config_flags;      /* word 7      SLRESHADE_CONFIG_*         */

    SLReShadeTexture motion_meta;         /* words 8..11                            */
    SLReShadeTexture surface_coverage;    /* words 12..15                           */

    /* Word 16 is a modulo-2^32 counter incremented once per reset event. Unlike
     * history_reset_flags (word 1), which is TRANSIENT and cleared after each
     * publish, this is monotonic: a consumer that samples at its own rate
     * detects any event it slept through by comparing the value with the one it
     * last saw. It was being written as reserved[0] while the header still
     * described the whole block as "future growth" -- a stealth field, which is
     * how a consumer ends up reading a live counter as padding, or a later
     * change hands out word 16 twice.
     * Naming it changes NO offsets and NO size, so this stays binary-compatible
     * with every shipped writer and reader; the static_assert below is unmoved. */
    uint32_t reset_event_counter;         /* word 16                                */
    uint32_t reserved[3];                 /* words 17..19  future MINOR growth      */
    SLReShadeTexture visible_diffuse;     /* words 20..23                           */
} SLReShadeFrameV11Tail;

#if defined(__cplusplus)
static_assert(sizeof(SLReShadeFrameV11Tail) == 24u * sizeof(uint32_t),
              "ABI 1.x tail must exactly fill SLReShadeFrame::reserved[24]");
#else
typedef char SLReShadeFrameV11Tail_size_must_be_96_bytes[
    (sizeof(SLReShadeFrameV11Tail) == 24u * sizeof(uint32_t)) ? 1 : -1];
#endif

SLRESHADE_INLINE uint32_t SLReShade_ABI_Major(uint32_t version)
{
    return version >> 16;
}

SLRESHADE_INLINE uint32_t SLReShade_ABI_Minor(uint32_t version)
{
    return version & 0xFFFFu;
}

SLRESHADE_INLINE uint64_t SLReShadeV11_GetGeneration(
    const SLReShadeFrameV11Tail* tail)
{
    return ((uint64_t)tail->render_target_generation_hi << 32) |
           (uint64_t)tail->render_target_generation_lo;
}

SLRESHADE_INLINE void SLReShadeV11_SetGeneration(
    SLReShadeFrameV11Tail* tail, uint64_t generation)
{
    tail->render_target_generation_lo = (uint32_t)(generation & 0xFFFFFFFFull);
    tail->render_target_generation_hi = (uint32_t)(generation >> 32);
}

/* Nonzero when a v1.1 tail was copied. Call ONLY on a seqlock-consistent local
 * snapshot -- never on the live pointer. Fails closed (zeroed tail) against a
 * 1.0 writer, so an old viewer plus a new add-on degrades to "nothing valid"
 * rather than to garbage. */
SLRESHADE_INLINE int SLReShadeFrame_GetV11Tail(
    const SLReShadeFrame* frame, SLReShadeFrameV11Tail* out_tail)
{
    if (frame == NULL || out_tail == NULL)
    {
        return 0;
    }
    if (frame->magic != SLRESHADE_ABI_MAGIC ||
        SLReShade_ABI_Major(frame->abi_version) != SLRESHADE_ABI_MAJOR ||
        SLReShade_ABI_Minor(frame->abi_version) < SLRESHADE_ABI_MINOR_V1_1 ||
        frame->struct_size < sizeof(SLReShadeFrame))
    {
        memset(out_tail, 0, sizeof(*out_tail));
        return 0;
    }
    memcpy(out_tail, frame->reserved, sizeof(*out_tail));
    return 1;
}

typedef const SLReShadeFrame* (*SLReShade_GetFrame_t)(uint32_t requested_abi);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* LL_LLRESHADEBRIDGEABI_H */
