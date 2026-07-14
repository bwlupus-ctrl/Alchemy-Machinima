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

#ifdef __cplusplus
extern "C" {
#endif

/* 'SLRB' -- sanity check that the pointer really is ours. */
#define SLRESHADE_ABI_MAGIC     0x534C5242u

/* Version packs MAJOR (high 16, breaking layout changes) and MINOR (low 16,
 * append-only additions inside the reserved tail). */
#define SLRESHADE_ABI_MAJOR     1u
#define SLRESHADE_ABI_MINOR     0u
#define SLRESHADE_ABI_VERSION   ((SLRESHADE_ABI_MAJOR << 16) | SLRESHADE_ABI_MINOR)

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
    SLReShadeTexture motion;     /* reserved for velocity buffer; 0 today  */

    /* --- append-only growth area (MINOR version bumps) ------------------ */
    uint32_t reserved[24];
} SLReShadeFrame;

/* Note on normals encoding: deferredScreen attachment 2 stores spheremap-
 * encoded normals in .xy (decode per globalF.glsl decodeNormal: fenc =
 * xy*4-2; f = dot(fenc,fenc); g = sqrt(1-f/4); n = (fenc*g, 1-f/2)); .z is
 * environment intensity, .w is the gbuffer flags byte. The add-on publishes
 * the raw texture; decoding is the consuming shader's job. */

typedef const SLReShadeFrame* (*SLReShade_GetFrame_t)(uint32_t requested_abi);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* LL_LLRESHADEBRIDGEABI_H */
