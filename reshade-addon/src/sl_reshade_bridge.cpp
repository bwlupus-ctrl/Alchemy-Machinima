/**
 * @file sl_reshade_bridge.cpp
 * @brief ReShade 6.x add-on that feeds the Second Life viewer's G-buffer and
 *        camera state to ReShade effects.
 *
 * Counterpart of the viewer's llreshadebridge.cpp. The viewer executable
 * exports SLReShade_GetFrame() (see llreshadebridgeabi.h -- the ONLY shared
 * header); this add-on resolves it at runtime, and each frame:
 *
 *   1. seqlock-reads the published SLReShadeFrame,
 *   2. reads the ABI 1.1 tail (SLReShadeFrame_GetV11Tail on the LOCAL
 *      snapshot) for explicit per-semantic validity, reset flags, generation,
 *   3. copies each VALID viewer texture (optionally demand-gated, see
 *      SL_DEMAND_COPY) into an add-on-OWNED immutable resource created
 *      through the ReShade device API,
 *   4. binds those via update_texture_bindings() semantics -- and binds a
 *      deterministic NEUTRAL (all-zero) texture for every semantic that is
 *      invalid, missing, unmapped or failed (FAIL CLOSED, contract v2 section 4:
 *      never retain the previous destination, never serve stale pixels),
 *   5. pushes camera uniforms to effect uniforms annotated
 *      < source = "sl_..."; > and the tail status (sl_semantic_valid,
 *      sl_reset_flags, ...) as uint uniforms so the FX layer can gate on
 *      EXPLICIT validity and reset temporal history.
 *
 * A 1.0 writer (older viewer, no tail) fails closed by design: the tail
 * helper returns a zeroed tail, semantic_valid is 0, every semantic goes
 * neutral. "Nothing valid", never garbage.
 *
 * WHY THE COPY: viewer render targets are mutable glTexImage2D level-0-only
 * textures. ReShade's GL samplers always use mipmapped min filters, so those
 * textures are mipmap-INCOMPLETE under ReShade's samplers and read as
 * constant (0,0,0,1). Binding them directly is the historical failure mode
 * (see doc/RESHADE_BRIDGE_ROOT_CAUSE.md on alchemy's archive branch).
 * Resources created through device::create_resource are immutable
 * (glTexStorage) and always complete -- exactly how ReShade's own
 * generic_depth add-on works. Depth itself is deliberately NOT provided:
 * generic_depth already handles it.
 *
 * COST (contract v2 section 6.2): five 4K slots is ~232 MB/frame of logical
 * copy payload; per-slot copied-byte counters are logged periodically so the
 * real cost is measured, not assumed. An OPT-IN demand-copy gate exists
 * (SL_DEMAND_COPY, default OFF) that skips slots no loaded effect declares --
 * but it keys on SL_Bridge.fxh variable names, which is only a heuristic, so
 * gated-out slots are failed closed (neutral + invalid), never left stale.
 *
 * Robustness rules honored throughout:
 *   - ABI major mismatch, missing export, or missing viewer data => neutral
 *     bindings + semantic_valid 0 (fail closed), no GL faults.
 *   - Never cache viewer GL names across frames; slots revalidate against
 *     {name, size, format} every frame and recreate on any change, and the
 *     tail's render_target_generation forces a rebuild on viewer-side RT
 *     reallocation instead of inferring it.
 *   - Effect-side handles invalidated on reshade_reloaded_effects.
 *   - All state torn down on destroy_effect_runtime.
 *
 * License: viewerlgpl (matches the viewer fork this ships with).
 */

#include <Windows.h>

#include <cstring>
#include <cstdio>
#include <vector>

#define RESHADE_ADDON 1
#include "reshade.hpp"

#include "../../indra/newview/llreshadebridgeabi.h"

// -----------------------------------------------------------------------------
// Add-on metadata (shown in the ReShade overlay's Add-ons tab).
// -----------------------------------------------------------------------------
extern "C" __declspec(dllexport) const char *NAME        = "SL G-Buffer Bridge";
extern "C" __declspec(dllexport) const char *DESCRIPTION =
    "Feeds the Second Life viewer's normals/motion/material G-buffer and camera "
    "matrices to effects via SL_* texture semantics and sl_* uniform annotations. "
    "Requires a viewer build that exports SLReShade_GetFrame.";

using namespace reshade::api;

// GL constants used for the source-handle encoding; defined locally so the
// add-on needs no GL headers. Encoding confirmed against ReShade's GL backend
// (make_resource_handle): (target << 40) | object.
static constexpr uint64_t kGlTexture2D = 0x0DE1;

static inline resource wrap_gl_texture(uint32_t gl_name)
{
    return resource { (kGlTexture2D << 40) | static_cast<uint64_t>(gl_name) };
}

// Demand-copy gate. When 1, a slot is only created/copied while some loaded
// effect declares its SL_Bridge.fxh texture VARIABLE NAME. The add-on API has
// no way to enumerate consumers by SEMANTIC (only find_texture_variable by
// name), so the name is the only signal available -- which makes the gate a
// HEURISTIC: a consumer that declares its own texture on an SL_* semantic
// under a different variable name is invisible to it and gets gated out.
// Consequences (cross-review 2026-07-25):
//   - DEFAULT OFF: every valid slot is copied; correctness never depends on
//     the gate. It is an opt-in optimisation whose real cost is measured by
//     the periodic copy-stats log, not assumed.
//   - When ON, a gated-out slot is FAILED CLOSED through the choke point
//     (neutral binding + validity bit clear), never skipped in place -- a
//     consumer the gate failed to see reads deterministic zeros and sees the
//     semantic reported invalid, instead of silently receiving stale pixels.
//     The gate's verdict for all eight slots is logged at every effect reload.
#ifndef SL_DEMAND_COPY
#define SL_DEMAND_COPY 0
#endif

// -----------------------------------------------------------------------------
// GL internal format -> ReShade api::format for the resources we replicate.
// Unmapped formats FAIL CLOSED for that slot (neutral binding, log once).
// -----------------------------------------------------------------------------
static format map_gl_internal_format(uint32_t glfmt)
{
    switch (glfmt)
    {
    case 0x1908 /* GL_RGBA (unsized; driver resolves to RGBA8) */:
    case 0x8058 /* GL_RGBA8      */: return format::r8g8b8a8_unorm;
    // The viewer's deferredScreen (=> SL_ALBEDO, attachment 0) is allocated
    // GL_SRGB8_ALPHA8 (pipeline.cpp: deferredScreen.allocate(..., GL_SRGB8_ALPHA8)).
    // This was UNMAPPED -> format::unknown -> the ALBEDO slot silently disabled
    // itself, SLAlbedoTex read all-zero, PS_ProvideAlbedo discarded every pixel,
    // and iMMERSE fell back to Launchpad's *estimated* albedo for the whole
    // session. Mapping it to the _srgb view means the hardware decodes sRGB->
    // linear on sample, which is what iMMERSE wants (it mixes albedo with LINEAR
    // GI) -- so SL_ALBEDO_TO_LINEAR must stay 0; setting it to 1 would decode twice.
    case 0x8C43 /* GL_SRGB8_ALPHA8 */: return format::r8g8b8a8_unorm_srgb;
    case 0x805B /* GL_RGBA16     */: return format::r16g16b16a16_unorm;
    case 0x8059 /* GL_RGB10_A2   */: return format::r10g10b10a2_unorm;
    case 0x881A /* GL_RGBA16F    */: return format::r16g16b16a16_float;
    case 0x822F /* GL_RG16F      */: return format::r16g16_float;
    case 0x8230 /* GL_RG32F      */: return format::r32g32_float;
    case 0x822B /* GL_RG8        */: return format::r8g8_unorm;   // motion_meta / surface_coverage
    default:                         return format::unknown;
    }
}

// Bytes per pixel for the formats above -- copied-byte instrumentation
// (contract v2 section 6.2). 0 = unknown (never copied anyway).
static uint32_t format_bytes_per_pixel(format f)
{
    switch (f)
    {
    case format::r8g8_unorm:            return 2;
    case format::r8g8b8a8_unorm:
    case format::r8g8b8a8_unorm_srgb:
    case format::r10g10b10a2_unorm:
    case format::r16g16_float:          return 4;
    case format::r16g16b16a16_unorm:
    case format::r16g16b16a16_float:
    case format::r32g32_float:          return 8;
    default:                            return 0;
    }
}

// -----------------------------------------------------------------------------
// One replicated G-buffer slot: an add-on-owned immutable texture + SRV bound
// to a ReShade FX semantic, revalidated against the published source AND the
// tail's explicit semantic-valid bit every frame. When invalid the semantic is
// bound to the shared neutral (all-zero) texture -- never left pointing at the
// last good destination.
// -----------------------------------------------------------------------------
enum SlotSource : uint8_t
{
    SRC_NORMALS, SRC_MOTION, SRC_ALBEDO, SRC_ORM, SRC_COLOR_HDR,
    SRC_MOTION_META, SRC_SURFACE_COVERAGE, SRC_VISIBLE_DIFFUSE,
};

struct GBufferSlot
{
    const char *semantic;         // FX: texture Tex : SEMANTIC;
    const char *fx_texture_name;  // SL_Bridge.fxh texture variable name (demand gate)
    uint32_t    sem_valid_bit;    // SLRESHADE_SEM_VALID_* bit that gates this slot
    uint8_t     source_id;        // SlotSource field selector

    // binding state -- what update_texture_bindings currently points at.
    // INVARIANT (see fail_slot_closed): only three writers exist -- the FED
    // transition in update_slot (BIND_DEST), the CLOSED transition in
    // fail_slot_closed (BIND_NEUTRAL), and forget() (BIND_NONE, device gone).
    // destroy() deliberately does NOT touch this; call it only inside those
    // transitions, never bare at a call site.
    enum : uint8_t { BIND_NONE, BIND_DEST, BIND_NEUTRAL };
    uint8_t       bound     = BIND_NONE;

    // live destination
    resource      dest      = { 0 };
    resource_view dest_srv  = { 0 };
    uint32_t      width     = 0;
    uint32_t      height    = 0;
    format        fmt       = format::unknown;
    bool          warned    = false;        // one-shot unmapped-format warning
    bool          consumed  = true;         // some loaded effect declares fx_texture_name

    // instrumentation window (reset every stats interval)
    uint32_t      win_copies = 0;
    uint64_t      win_bytes  = 0;

    void destroy(device *dev)
    {
        if (dest_srv != 0) { dev->destroy_resource_view(dest_srv); dest_srv = { 0 }; }
        if (dest != 0)     { dev->destroy_resource(dest);          dest     = { 0 }; }
        width = height = 0;
        fmt = format::unknown;
    }

    // Drop handles WITHOUT a GL destroy (owning context gone / wrong device).
    void forget()
    {
        dest = { 0 }; dest_srv = { 0 };
        width = height = 0;
        fmt = format::unknown;
        bound = BIND_NONE;
    }
};

// -----------------------------------------------------------------------------
// TRUE DEPTH -- replaces ReShade's built-in generic_depth heuristic.
//
// Bound DIRECTLY (no replicate/copy), unlike the colour G-buffer slots above:
//   * depth<->colour copies are ILLEGAL (glCopyImageSubData requires a matching
//     internal-format class), so the replicate path simply cannot carry depth;
//   * the replicate path only exists to dodge GL mip-INcompleteness when
//     sampling, which does not apply here -- the viewer allocates depth as a
//     single-level NEAREST texture, so it is already complete and safe to
//     sample in place.
// This mirrors what generic_depth itself does, minus the guesswork: we bind the
// viewer's real deferredScreen depth, so DEPTH is guaranteed to be the exact
// buffer our normals/albedo/motion came from (no heuristic misdetection during
// shadow / reflection-probe / impostor passes).
//
// REQUIRES the built-in "Generic depth" add-on to be DISABLED -- it rebinds the
// DEPTH semantic every frame and would otherwise fight us for the binding.
//
// STATUS 2026-07-14: DISABLED (rolled back). The direct bind does NOT work --
// create_resource_view() fails on the viewer's GL depth texture:
//     "[SLBridge] DEPTH: create_resource_view failed for GL 3044 (fmt 0x81A6)"
// Almost certainly because ReShade's GL backend builds views via glTextureView,
// which requires IMMUTABLE storage (glTexStorage*), while LLRenderTarget
// allocates with glTexImage2D (mutable). That is very likely the original reason
// the colour slots replicate into add-on-OWNED textures (ReShade creates those
// immutable, so they are viewable) rather than binding the source directly.
//
// => Direct-bind is a dead end. See doc/SL_TRUTH_RESHADE_BRIEF.md §2: the
//    pre-compensated-depth design routes around this entirely, because the client
//    writes depth into an R32F COLOUR texture, which rides the existing proven
//    copy path and needs no view of a depth resource at all.
//
// Code kept (inert) as the delivery mechanism for that design.
// Set SL_PROVIDE_DEPTH=1 only if the view problem is solved.
// -----------------------------------------------------------------------------
#ifndef SL_PROVIDE_DEPTH
#define SL_PROVIDE_DEPTH 0
#endif

struct DepthSlot
{
    resource_view srv     = { 0 };
    uint32_t      gl_name = 0;
    uint32_t      width   = 0;
    uint32_t      height  = 0;
    bool          warned  = false;

    // Drop handles WITHOUT a GL destroy (owning context gone / wrong device) --
    // same rule as the colour slots, see on_destroy_effect_runtime.
    void forget() { srv = { 0 }; gl_name = width = height = 0; }
};

// -----------------------------------------------------------------------------
// Tail status pushed to the FX layer as uint uniforms each frame.
// -----------------------------------------------------------------------------
struct FrameStatus
{
    uint32_t semantic_valid;    // EFFECTIVE validity: tail bit AND slot actually fed
    uint32_t reset_flags;       // tail reset flags OR add-on-detected events
    uint32_t motion_encoding;
    uint32_t orientation_flags;
    uint32_t motion_coverage;
    uint32_t config_flags;
    uint32_t tail_valid;        // 1 = a v1.1 tail was read; 0 = 1.0 writer / no data
};

// -----------------------------------------------------------------------------
// Global add-on state (ReShade fires all our events on the app's render
// thread inside SwapBuffers, and the viewer is single-runtime, so plain
// statics are sufficient).
// -----------------------------------------------------------------------------
struct BridgeState
{
    effect_runtime      *runtime = nullptr;
    device              *dev = nullptr;      // device our slot resources belong to
    SLReShade_GetFrame_t get_frame = nullptr;
    bool                 export_missing_logged = false;
    uint64_t             last_frame_counter = ~0ull;

    // Crash containment: if any of our GL work faults (multi-context churn,
    // stale handles, driver quirk), we log it and go permanently inert for the
    // session instead of taking ReShade's hook down (the "soft-crash").
    volatile bool        dead = false;
    const char          *stage = "idle";     // breadcrumb for the fault log

    GBufferSlot slots[8] = {
        { "SL_NORMALS",          "SLNormalsTex",         SLRESHADE_SEM_VALID_NORMALS,          SRC_NORMALS          },
        { "SL_MOTION_NDC",       "SLMotionTex",          SLRESHADE_SEM_VALID_MOTION,           SRC_MOTION           },
        { "SL_ALBEDO",           "SLAlbedoTex",          SLRESHADE_SEM_VALID_ALBEDO,           SRC_ALBEDO           },
        { "SL_ORM",              "SLOrmTex",             SLRESHADE_SEM_VALID_ORM,              SRC_ORM              },
        { "SL_COLOR_HDR",        "SLColorHdrTex",        SLRESHADE_SEM_VALID_COLOR_HDR,        SRC_COLOR_HDR        },
        { "SL_MOTION_META",      "SLMotionMetaTex",      SLRESHADE_SEM_VALID_MOTION_META,      SRC_MOTION_META      },
        { "SL_SURFACE_COVERAGE", "SLSurfaceCoverageTex", SLRESHADE_SEM_VALID_SURFACE_COVERAGE, SRC_SURFACE_COVERAGE },
        // ABI 1.2: visible-surface linear diffuse (RGB) + exactness K (A).
        // GL_SRGB8_ALPHA8 rides the same _srgb-view mapping as SL_ALBEDO, so
        // the FX layer samples LINEAR RGB; K passes through the (linear) alpha
        // channel of the sRGB view untouched (GL sRGB decode never applies to
        // alpha). Validity is additionally minor-gated in do_begin_effects: a
        // 1.1 writer left tail words 20..23 reserved, so its bit is masked off
        // and the slot fails closed like any other invalid semantic.
        { "SL_VISIBLE_DIFFUSE",  "SLVisibleDiffuseTex",  SLRESHADE_SEM_VALID_VISIBLE_DIFFUSE,  SRC_VISIBLE_DIFFUSE  },
    };

    DepthSlot depth;   // ReShade's DEPTH semantic, bound direct (see DepthSlot)

    // Shared 4x4 all-zero immutable texture: the deterministic NEUTRAL every
    // invalid semantic is bound to (fail closed, contract v2 section 4).
    resource      neutral     = { 0 };
    resource_view neutral_srv = { 0 };
    bool          neutral_failed_logged = false;

    // v1.1 tail tracking
    uint64_t last_generation = 0;
    bool     generation_seen = false;
    bool     seen_valid      = false;   // FIRST_VALID_FRAME latch
    uint32_t local_reset     = 0;       // add-on-detected SLRESHADE_RESET_* events

    // instrumentation
    uint32_t stats_frames = 0;

    // uniform push list, rebuilt on effect (re)load
    struct UniformTarget
    {
        effect_uniform_variable var;
        uint8_t source;   // index into the source table below
    };
    std::vector<UniformTarget> uniforms;
    bool uniforms_dirty = true;
};
static BridgeState g;

static const SLReShadeTexture *slot_source(const SLReShadeFrame &f,
                                           const SLReShadeFrameV11Tail &t,
                                           uint8_t id)
{
    switch (id)
    {
    case SRC_NORMALS:          return &f.normals;
    case SRC_MOTION:           return &f.motion;
    case SRC_ALBEDO:           return &f.albedo;
    case SRC_ORM:              return &f.orm;
    case SRC_COLOR_HDR:        return &f.color_hdr;
    case SRC_MOTION_META:      return &t.motion_meta;
    case SRC_SURFACE_COVERAGE: return &t.surface_coverage;
    case SRC_VISIBLE_DIFFUSE:  return &t.visible_diffuse;
    default:                   return nullptr;
    }
}

// uniform annotation sources: < source = "sl_view_matrix"; > etc.
// is_uint entries are pushed with set_uniform_value_uint from FrameStatus.
struct UniformSourceDef { const char *name; uint32_t count; bool is_uint; };
static const UniformSourceDef kUniformSources[] = {
    { "sl_view_matrix",       16, false },  // row-major LLMatrix4 (row-vector convention)
    { "sl_proj_matrix",       16, false },
    { "sl_camera_pos",         3, false },
    { "sl_camera_at",          3, false },
    { "sl_camera_left",        3, false },
    { "sl_camera_up",          3, false },
    { "sl_near_far",           2, false },
    { "sl_fov_y",              1, false },
    { "sl_aspect",             1, false },
    { "sl_frame_counter",      1, false },
    // --- ABI 1.1 tail status (uint) -----------------------------------------
    { "sl_semantic_valid",     1, true  },
    { "sl_reset_flags",        1, true  },
    { "sl_motion_encoding",    1, true  },
    { "sl_orientation_flags",  1, true  },
    { "sl_motion_coverage",    1, true  },
    { "sl_config_flags",       1, true  },
    { "sl_tail_valid",         1, true  },
};

static const float *uniform_source_data(const SLReShadeFrame &f, uint8_t idx,
                                        float *scratch /* >= 16 floats */)
{
    switch (idx)
    {
    case 0: return f.view;
    case 1: return f.proj;
    case 2: return f.origin;
    case 3: return f.at_axis;
    case 4: return f.left_axis;
    case 5: return f.up_axis;
    case 6: scratch[0] = f.near_clip; scratch[1] = f.far_clip; return scratch;
    case 7: return &f.fov_y;
    case 8: return &f.aspect;
    case 9: scratch[0] = static_cast<float>(f.frame_counter); return scratch;
    default: return nullptr;
    }
}

static uint32_t uniform_source_uint(const FrameStatus &st, uint8_t idx)
{
    switch (idx)
    {
    case 10: return st.semantic_valid;
    case 11: return st.reset_flags;
    case 12: return st.motion_encoding;
    case 13: return st.orientation_flags;
    case 14: return st.motion_coverage;
    case 15: return st.config_flags;
    case 16: return st.tail_valid;
    default: return 0;
    }
}

// -----------------------------------------------------------------------------
// logging helper
// -----------------------------------------------------------------------------
static void logf(reshade::log::level lvl, const char *fmt, ...)
{
    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    reshade::log::message(lvl, buf);
}

// -----------------------------------------------------------------------------
// Seqlock read of the viewer's published frame. Returns false if the viewer
// export is absent, ABI-incompatible, or the frame can't be read untorn.
// -----------------------------------------------------------------------------
static bool read_viewer_frame(SLReShadeFrame &out)
{
    if (g.get_frame == nullptr)
    {
        // Resolve lazily: the export lives in the host executable.
        HMODULE exe = GetModuleHandleW(nullptr);
        g.get_frame = reinterpret_cast<SLReShade_GetFrame_t>(
            GetProcAddress(exe, SLRESHADE_EXPORT_NAME));
        if (g.get_frame == nullptr)
        {
            if (!g.export_missing_logged)
            {
                g.export_missing_logged = true;
                logf(reshade::log::level::warning,
                     "[SLBridge] host executable does not export %s -- bridge inactive "
                     "(viewer build without ReShade bridge?)", SLRESHADE_EXPORT_NAME);
            }
            return false;
        }
        logf(reshade::log::level::info, "[SLBridge] resolved %s", SLRESHADE_EXPORT_NAME);
    }

    const SLReShadeFrame *src = g.get_frame(SLRESHADE_ABI_VERSION);
    if (src == nullptr)
    {
        if (!g.export_missing_logged)
        {
            g.export_missing_logged = true;
            logf(reshade::log::level::warning,
                 "[SLBridge] viewer rejected ABI version 0x%08X -- bridge inactive",
                 SLRESHADE_ABI_VERSION);
        }
        return false;
    }

    // Basic sanity before trusting the layout.
    if (src->magic != SLRESHADE_ABI_MAGIC || src->struct_size < sizeof(SLReShadeFrame))
    {
        return false;
    }

    // Seqlock: copy until begin/end counters agree (bounded; writer is on this
    // same thread in practice, so one pass is the norm).
    for (int attempt = 0; attempt < 4; ++attempt)
    {
        const uint32_t end = src->write_end;
        memcpy(&out, src, sizeof(SLReShadeFrame));
        const uint32_t begin = src->write_begin;
        if (begin == end && out.write_end == end)
        {
            return true;
        }
    }
    return false;
}

// -----------------------------------------------------------------------------
// Neutral resource: a 4x4 all-zero immutable texture created once per device.
// Deterministic "nothing" -- what every invalid semantic is bound to.
// -----------------------------------------------------------------------------
static bool ensure_neutral(device *dev)
{
    if (g.neutral_srv != 0)
    {
        return true;
    }
    g.stage = "create_neutral";

    static const uint32_t zeros[16] = { 0 };   // 4x4 RGBA8 = 64 bytes of zero
    subresource_data init = {};
    init.data      = const_cast<uint32_t *>(zeros);
    init.row_pitch = 4 * 4;

    const resource_desc desc(
        4, 4, 1 /*layers*/, 1 /*levels*/, format::r8g8b8a8_unorm,
        1 /*samples*/, memory_heap::gpu_only, resource_usage::shader_resource);

    if (!dev->create_resource(desc, &init, resource_usage::shader_resource, &g.neutral))
    {
        g.neutral = { 0 };
        if (!g.neutral_failed_logged)
        {
            g.neutral_failed_logged = true;
            logf(reshade::log::level::error,
                 "[SLBridge] neutral texture create_resource failed -- invalid "
                 "slots fall back to a null-view binding (ReShade blank texture)");
        }
        return false;
    }
    if (!dev->create_resource_view(g.neutral, resource_usage::shader_resource,
                                   resource_view_desc(format::r8g8b8a8_unorm),
                                   &g.neutral_srv))
    {
        dev->destroy_resource(g.neutral);
        g.neutral = { 0 };
        g.neutral_srv = { 0 };
        if (!g.neutral_failed_logged)
        {
            g.neutral_failed_logged = true;
            logf(reshade::log::level::error,
                 "[SLBridge] neutral texture create_resource_view failed -- invalid "
                 "slots fall back to a null-view binding (ReShade blank texture)");
        }
        return false;
    }
    return true;
}

// -----------------------------------------------------------------------------
// SLOT STATE CHOKE POINT (contract v2 section 4).
//
// A slot has exactly TWO steady states, and only two places may establish
// them:
//   FED:    bound == BIND_DEST, dest/dest_srv live, destination copied from
//           the current viewer frame -- established ONLY by the success path
//           at the bottom of update_slot();
//   CLOSED: bound == BIND_NEUTRAL, dest == 0, semantic bound to the neutral
//           texture (=> its effective-validity bit reads 0, since that bit
//           requires BIND_DEST) -- established ONLY here.
// (BIND_NONE additionally means "never bound on this device": ReShade's own
// blank texture serves zeros and the status uniforms read 0 -- CLOSED in
// effect, promoted to an explicit BIND_NEUTRAL the first time this runs.)
//
// Every path that stops feeding a slot -- explicit-invalid semantic, missing
// source, unmapped format, allocation failure, torn read / missing tail,
// render-target generation change, demand-gate exclusion -- funnels through
// this function. No return anywhere may leave `bound` bookkeeping that
// disagrees with what update_texture_bindings last pointed the semantic at;
// the 2026-07-25 cross-review found three paths that did (generation-change
// destruction, the demand-copy skip, neutral-allocation failure), which is why
// this is now a single choke point instead of correct-by-inspection returns.
//
// Order matters: the replacement is bound BEFORE the old destination is
// destroyed, so the semantic never dangles. If even the 4x4 neutral cannot be
// allocated (device-loss territory), the semantic is bound to a NULL view --
// which ReShade resolves to its built-in blank texture, the same mechanism
// generic_depth uses to drop its DEPTH binding -- and the destination is
// still destroyed, so even that path cannot retain stale pixels. Only called
// from begin_effects, where the GL context is current, so destroys are safe.
//
// Returns true when live data was actually lost (caller raises SOURCE_LOSS).
// -----------------------------------------------------------------------------
static bool fail_slot_closed(GBufferSlot &slot, effect_runtime *runtime, device *dev)
{
    if (slot.bound == GBufferSlot::BIND_NEUTRAL && slot.dest == 0)
    {
        return false;   // already CLOSED (idempotent)
    }

    g.stage = "fail_slot_closed";
    const bool had_data = (slot.bound == GBufferSlot::BIND_DEST);

    const resource_view nv =
        ensure_neutral(dev) ? g.neutral_srv : resource_view { 0 };
    runtime->update_texture_bindings(slot.semantic, nv, nv);
    slot.bound = GBufferSlot::BIND_NEUTRAL;
    slot.destroy(dev);   // destination gone -- stale pixels are unreachable

    if (had_data)
    {
        logf(reshade::log::level::info,
             "[SLBridge] %s: source invalid/lost -- failed closed to neutral",
             slot.semantic);
    }
    return had_data;
}

// -----------------------------------------------------------------------------
// Ensure a slot's add-on-owned destination matches the published source; then
// record the copy. sem_valid is the tail's EXPLICIT availability bit -- payload
// values are never used as sentinels (contract v2 section 4).
//
// force_recreate: the viewer signalled a render-target generation change, so
// the existing destination's contents are suspect even when {name, size,
// format} match -- take the recreate branch unconditionally. Routing the
// generation change through here (instead of destroying destinations at the
// call site) keeps every state transition inside the two choke points: the
// slot leaves this function either FED (fresh destination + fresh copy) or
// CLOSED (fail_slot_closed), never with bookkeeping pointing at a destroyed
// destination.
// -----------------------------------------------------------------------------
static void update_slot(GBufferSlot &slot, bool sem_valid,
                        const SLReShadeTexture *src_tex,
                        effect_runtime *runtime, command_list *cmd,
                        bool force_recreate)
{
    device *dev = runtime->get_device();

#if SL_DEMAND_COPY
    if (!slot.consumed)
    {
        // Gated out (see SL_DEMAND_COPY): fail CLOSED, do not skip in place.
        // The variable-name gate is a heuristic -- a consumer it failed to see
        // must read deterministic neutral with the validity bit clear, never
        // whatever the destination held when the gate last changed.
        if (fail_slot_closed(slot, runtime, dev))
        {
            g.local_reset |= SLRESHADE_RESET_SOURCE_LOSS;
        }
        return;
    }
#endif

    if (!sem_valid || src_tex == nullptr || src_tex->gl_name == 0 ||
        src_tex->width == 0 || src_tex->height == 0)
    {
        // Explicitly invalid, or the source texture is missing: FAIL CLOSED.
        if (fail_slot_closed(slot, runtime, dev))
        {
            g.local_reset |= SLRESHADE_RESET_SOURCE_LOSS;
        }
        return;
    }

    const format want = map_gl_internal_format(src_tex->gl_internal_format);
    if (want == format::unknown)
    {
        if (!slot.warned)
        {
            slot.warned = true;
            logf(reshade::log::level::warning,
                 "[SLBridge] %s: unmapped GL internal format 0x%04X -- failing closed",
                 slot.semantic, src_tex->gl_internal_format);
        }
        if (fail_slot_closed(slot, runtime, dev))
        {
            g.local_reset |= SLRESHADE_RESET_SOURCE_LOSS;
        }
        return;
    }

    // (Re)create the destination when size/format changes (window resize,
    // graphics-setting change, HDR toggle), after a fail-closed episode, or
    // when the viewer's generation counter says the source storage changed
    // behind identical {name, size, format} (force_recreate).
    if (force_recreate || slot.dest == 0 || slot.width != src_tex->width ||
        slot.height != src_tex->height || slot.fmt != want)
    {
        // BIND BEFORE DESTROY. On entry the semantic is still bound to
        // slot.dest_srv. Destroying the resource first leaves ReShade holding a
        // dangling SRV for the entire creation window -- and both failure paths
        // below (create_resource, create_resource_view) return out of the middle
        // of that window. Neutral-bind first, so the worst observable case is a
        // consumer sampling deterministic neutral rather than freed memory.
        //
        // Deliberately NOT routed through fail_slot_closed(): this is a PLANNED
        // recreation (resize / format change / generation bump), not a source
        // loss, and must not raise SLRESHADE_RESET_SOURCE_LOSS on an ordinary
        // window resize -- the recreate path signals TARGET_RECREATED instead.
        // The failure paths below raise SOURCE_LOSS explicitly, because
        // fail_slot_closed() is idempotent and returns false once the slot is
        // already neutral.
        if (slot.bound != GBufferSlot::BIND_NEUTRAL)
        {
            g.stage = "neutral_bind_before_destroy";
            const resource_view nv =
                ensure_neutral(dev) ? g.neutral_srv : resource_view { 0 };
            runtime->update_texture_bindings(slot.semantic, nv, nv);
            slot.bound = GBufferSlot::BIND_NEUTRAL;
        }

        g.stage = "destroy_stale_slot";
        slot.destroy(dev);
        g.stage = "create_resource";

        const resource_desc desc(
            src_tex->width, src_tex->height, 1 /*layers*/, 1 /*levels*/, want,
            1 /*samples*/, memory_heap::gpu_only,   // ("default_" in newer SDKs)
            resource_usage::shader_resource | resource_usage::copy_dest);

        if (!dev->create_resource(desc, nullptr, resource_usage::shader_resource, &slot.dest))
        {
            logf(reshade::log::level::error,
                 "[SLBridge] %s: create_resource %ux%u fmt %d failed",
                 slot.semantic, src_tex->width, src_tex->height, (int)want);
            slot.dest = { 0 };
            // Unconditional: the slot was already neutral-bound above, so
            // fail_slot_closed() is a no-op returning false. The loss is real
            // regardless -- consumers must be told.
            fail_slot_closed(slot, runtime, dev);
            g.local_reset |= SLRESHADE_RESET_SOURCE_LOSS;
            return;
        }
        g.stage = "create_resource_view";
        if (!dev->create_resource_view(slot.dest, resource_usage::shader_resource,
                                       resource_view_desc(want), &slot.dest_srv))
        {
            logf(reshade::log::level::error,
                 "[SLBridge] %s: create_resource_view failed", slot.semantic);
            slot.destroy(dev);
            // See above: already neutral, so fail_slot_closed() cannot report
            // the loss for us.
            fail_slot_closed(slot, runtime, dev);
            g.local_reset |= SLRESHADE_RESET_SOURCE_LOSS;
            return;
        }

        slot.width  = src_tex->width;
        slot.height = src_tex->height;
        slot.fmt    = want;

        // Bind the (new) SRV to the semantic for all loaded effects.
        g.stage = "update_texture_bindings";
        runtime->update_texture_bindings(slot.semantic, slot.dest_srv, slot.dest_srv);
        slot.bound = GBufferSlot::BIND_DEST;
        g.local_reset |= SLRESHADE_RESET_TARGET_RECREATED;
        logf(reshade::log::level::info,
             "[SLBridge] %s: bound %ux%u fmt %d (src GL %u)",
             slot.semantic, slot.width, slot.height, (int)want, src_tex->gl_name);
    }
    else if (slot.bound != GBufferSlot::BIND_DEST)
    {
        // Destination survived but the semantic points elsewhere (defensive;
        // fail_slot_closed destroys dest, so this is not normally reachable).
        g.stage = "rebind_dest";
        runtime->update_texture_bindings(slot.semantic, slot.dest_srv, slot.dest_srv);
        slot.bound = GBufferSlot::BIND_DEST;
    }

    // Copy viewer texture -> our immutable texture. Source completeness is
    // irrelevant for copies (only sampling needs mip-complete textures).
    g.stage = "copy_texture_region";
    const resource src = wrap_gl_texture(src_tex->gl_name);
    cmd->barrier(slot.dest, resource_usage::shader_resource, resource_usage::copy_dest);
    cmd->copy_texture_region(src, 0, nullptr, slot.dest, 0, nullptr,
                             filter_mode::min_mag_mip_point);
    cmd->barrier(slot.dest, resource_usage::copy_dest, resource_usage::shader_resource);
    g.stage = "post_copy";

    slot.win_copies += 1;
    slot.win_bytes  += (uint64_t)slot.width * slot.height * format_bytes_per_pixel(want);
}

// Periodic per-slot copy-cost report (contract v2 sections 6.2 / gate P1).
static void maybe_log_copy_stats()
{
    if (++g.stats_frames < 600)
    {
        return;
    }
    char line[500];
    int n = snprintf(line, sizeof(line), "[SLBridge] copy stats over %u frames:",
                     g.stats_frames);
    bool any = false;
    for (GBufferSlot &s : g.slots)
    {
        if (s.win_copies != 0 && n > 0 && n < (int)sizeof(line))
        {
            n += snprintf(line + n, sizeof(line) - n, " %s %.1f MB/f (%u copies);",
                          s.semantic,
                          (double)s.win_bytes / (1024.0 * 1024.0) / (double)g.stats_frames,
                          s.win_copies);
            any = true;
        }
        s.win_copies = 0;
        s.win_bytes  = 0;
    }
    if (any)
    {
        reshade::log::message(reshade::log::level::info, line);
    }
    g.stats_frames = 0;
}

// -----------------------------------------------------------------------------
// Point ReShade's DEPTH semantic at the viewer's real depth buffer.
// Cheap: only (re)creates the view when the source texture actually changes
// (resize / RT rebuild); steady-state frames early-out.
// -----------------------------------------------------------------------------
#if SL_PROVIDE_DEPTH
static void update_depth_binding(const SLReShadeTexture &src, effect_runtime *runtime)
{
    device *dev = runtime->get_device();

    if (src.gl_name == 0 || src.width == 0 || src.height == 0)
    {
        // Not published this frame (RT rebuild in progress): keep the last good
        // binding rather than dropping DEPTH to nothing.
        return;
    }

    // Same source as last time -> existing view is still valid.
    if (g.depth.srv != 0 && g.depth.gl_name == src.gl_name &&
        g.depth.width == src.width && g.depth.height == src.height)
    {
        return;
    }

    g.stage = "depth_create_resource_view";
    if (g.depth.srv != 0)
    {
        dev->destroy_resource_view(g.depth.srv);
        g.depth.srv = { 0 };
    }

    // D24 sampled as R24 -> depth arrives in .r, which is what ReShade's
    // RESHADE_DEPTH_* macros / ReShade::GetLinearizedDepth() expect. Feed RAW
    // (non-linear) depth: linearisation is the FX layer's job, do NOT pre-do it.
    if (!dev->create_resource_view(wrap_gl_texture(src.gl_name),
                                   resource_usage::shader_resource,
                                   resource_view_desc(format::r24_unorm_x8_uint),
                                   &g.depth.srv))
    {
        if (!g.depth.warned)
        {
            g.depth.warned = true;
            logf(reshade::log::level::error,
                 "[SLBridge] DEPTH: create_resource_view failed for GL %u (fmt 0x%04X) "
                 "-- leaving the DEPTH semantic alone",
                 src.gl_name, src.gl_internal_format);
        }
        g.depth.srv = { 0 };
        return;
    }

    g.depth.gl_name = src.gl_name;
    g.depth.width   = src.width;
    g.depth.height  = src.height;

    g.stage = "depth_update_texture_bindings";
    runtime->update_texture_bindings("DEPTH", g.depth.srv, g.depth.srv);
    logf(reshade::log::level::info,
         "[SLBridge] DEPTH: bound TRUE SL depth %ux%u (GL %u). Disable the built-in "
         "\"Generic depth\" add-on or it will fight us for this semantic.",
         g.depth.width, g.depth.height, g.depth.gl_name);
}
#endif // SL_PROVIDE_DEPTH

// -----------------------------------------------------------------------------
// Uniform cache: rebuilt after every effect (re)load (always BEFORE the slot
// update loop -- see do_begin_effects). Also recomputes the per-slot
// demand-copy gate: a slot is "consumed" when any loaded effect declares its
// SL_Bridge.fxh texture VARIABLE NAME. That is a HEURISTIC, not a guarantee:
// the API cannot enumerate consumers by semantic, so a consumer declaring its
// own differently-named texture on an SL_* semantic is gated out. This is why
// SL_DEMAND_COPY defaults OFF, why gated-out slots are failed closed (neutral
// + invalid) instead of skipped, and why the verdict below is logged at every
// reload -- a missed consumer is diagnosable, never silently stale.
// -----------------------------------------------------------------------------
static void rebuild_uniform_cache(effect_runtime *runtime)
{
    g.uniforms.clear();
    runtime->enumerate_uniform_variables(nullptr,
        [](effect_runtime *rt, effect_uniform_variable var)
        {
            char source[64] = "";
            if (!rt->get_annotation_string_from_uniform_variable(var, "source", source))
            {
                return;
            }
            for (uint8_t i = 0; i < _countof(kUniformSources); ++i)
            {
                if (strcmp(source, kUniformSources[i].name) == 0)
                {
                    g.uniforms.push_back({ var, i });
                    return;
                }
            }
        });
    g.uniforms_dirty = false;
    if (!g.uniforms.empty())
    {
        logf(reshade::log::level::info,
             "[SLBridge] %zu sl_* uniform(s) wired", g.uniforms.size());
    }

#if SL_DEMAND_COPY
    char gate[300];
    int n = snprintf(gate, sizeof(gate), "[SLBridge] demand-copy gate:");
    for (GBufferSlot &s : g.slots)
    {
        s.consumed = runtime->find_texture_variable(nullptr, s.fx_texture_name) != 0;
        if (n > 0 && n < (int)sizeof(gate))
        {
            n += snprintf(gate + n, sizeof(gate) - n, " %s=%d",
                          s.semantic, s.consumed ? 1 : 0);
        }
    }
    reshade::log::message(reshade::log::level::info, gate);
#endif
}

// -----------------------------------------------------------------------------
// Push camera (float) + tail status (uint) uniforms. f may be null when no
// consistent frame exists -- then only the status uniforms are pushed (they
// carry semantic_valid = 0, which is what gates every consumer off).
// -----------------------------------------------------------------------------
static void push_uniforms(effect_runtime *runtime, const SLReShadeFrame *f,
                          const FrameStatus &st)
{
    g.stage = "push_uniforms";
    float scratch[16];
    for (const BridgeState::UniformTarget &u : g.uniforms)
    {
        const UniformSourceDef &def = kUniformSources[u.source];
        if (def.is_uint)
        {
            const uint32_t v = uniform_source_uint(st, u.source);
            runtime->set_uniform_value_uint(u.var, &v, 1);
        }
        else if (f != nullptr)
        {
            const float *data = uniform_source_data(*f, u.source, scratch);
            if (data != nullptr)
            {
                runtime->set_uniform_value_float(u.var, data, def.count);
            }
        }
    }
}

// -----------------------------------------------------------------------------
// event callbacks
// -----------------------------------------------------------------------------
static void on_init_effect_runtime(effect_runtime *runtime)
{
    g.runtime = runtime;
    g.uniforms_dirty = true;
    // slots hold device objects from a previous runtime? init means fresh
    // device -- clear handles without destroy (old device is gone).
    for (GBufferSlot &s : g.slots)
    {
        s.forget();
    }
    g.depth.forget();
    g.neutral = { 0 };
    g.neutral_srv = { 0 };
    logf(reshade::log::level::info, "[SLBridge] effect runtime initialized");
}

static void on_destroy_effect_runtime(effect_runtime *runtime)
{
    if (runtime != g.runtime)
    {
        return;
    }
    // CRITICAL: do NOT call destroy_resource here. destroy_effect_runtime fires
    // during the viewer's GL context teardown (wglDeleteContext, e.g. on world
    // entry) where the owning context may no longer be current -- issuing
    // glDeleteTextures then faults and takes ReShade's runtime down with it
    // (soft-crash: overlay dies, game keeps rendering via passthrough).
    // The context's own destruction frees these textures; we just drop handles.
    // (Resize/format changes still destroy safely in update_slot, where the
    // context is current.) Mirrors on_init_effect_runtime's handling.
    for (GBufferSlot &s : g.slots)
    {
        s.forget();
    }
    g.depth.forget();   // view belongs to the dying context; drop, don't destroy
    g.neutral = { 0 };
    g.neutral_srv = { 0 };
    g.uniforms.clear();
    g.runtime = nullptr;
    g.dev = nullptr;
}

static void on_reloaded_effects(effect_runtime *runtime)
{
    if (g.dead)
    {
        return;
    }
    g.uniforms_dirty = true;
    // Re-issue semantic bindings: effect reload resets texture bindings. Each
    // slot is re-pointed at whatever it was bound to BEFORE the reload -- its
    // live destination or the neutral texture (never left dangling).
    // Only if the SRVs belong to this runtime's device (multi-context guard).
    if (runtime->get_device() != g.dev)
    {
        return;
    }
    for (GBufferSlot &s : g.slots)
    {
        if (s.bound == GBufferSlot::BIND_DEST && s.dest_srv != 0)
        {
            runtime->update_texture_bindings(s.semantic, s.dest_srv, s.dest_srv);
        }
        else if (s.bound == GBufferSlot::BIND_NEUTRAL && g.neutral_srv != 0)
        {
            runtime->update_texture_bindings(s.semantic, g.neutral_srv, g.neutral_srv);
        }
    }
}

static void do_begin_effects(effect_runtime *runtime, command_list *cmd)
{
    // Fault-isolation probes (compile with /DSL_PROBE=N):
    //   1 = events registered but begin_effects does NOTHING
    //   2 = only resolves+reads the viewer frame, no GL work
    // If the overlay dies even at probe 1, the problem is registration itself
    // (or another addon interaction); if it dies first at 2, it's the viewer
    // export call; if only at full, it's our GL work.
#if defined(SL_PROBE) && SL_PROBE == 1
    return;
#endif
    // Multi-context guard: the viewer runs several GL contexts (main + a
    // worker sharing 0x20000); ReShade can host runtimes on more than one.
    // Our slot resources belong to exactly ONE device -- if this callback
    // arrives on a different device, drop the handles (no GL destroy; wrong
    // context) and rebuild everything on the presenting device.
    g.stage = "device_key";
    device *dev = runtime->get_device();
    if (g.dev != dev)
    {
        for (GBufferSlot &s : g.slots)
        {
            s.forget();
        }
        g.depth.forget();
        g.neutral = { 0 };
        g.neutral_srv = { 0 };
        g.uniforms_dirty = true;
        if (g.dev != nullptr)
        {
            logf(reshade::log::level::info,
                 "[SLBridge] runtime device changed -- rebuilding resources");
        }
        g.dev = dev;
    }

    g.stage = "read_frame";
    SLReShadeFrame f;
    const bool frame_ok = read_viewer_frame(f) && (f.flags & SLRESHADE_FLAG_VALID) != 0;

#if defined(SL_PROBE) && SL_PROBE == 2
    return;   // probe 2: viewer export called + frame read, but no GL work
#endif

    // ABI 1.1 tail, read from the seqlock-consistent LOCAL snapshot (never the
    // live pointer). A 1.0 writer yields a zeroed tail and returns 0 -- which
    // drops us into the fail-closed path below: an older viewer plus this
    // add-on degrades to "nothing valid", not to stale/garbage pixels.
    g.stage = "read_tail";
    SLReShadeFrameV11Tail tail = {};
    const bool tail_ok = frame_ok && SLReShadeFrame_GetV11Tail(&f, &tail) != 0;

    if (!tail_ok)
    {
        // No usable data: dead bridge, login screen (VALID unset), torn read,
        // or a 1.0 writer. FAIL CLOSED (contract v2 section 4): every semantic
        // is bound to the neutral texture and reported invalid. The old
        // behaviour -- returning early and silently serving the last good
        // destination -- is exactly the defect this replaces.
        g.stage = "fail_closed_all";
        for (GBufferSlot &s : g.slots)
        {
            if (fail_slot_closed(s, runtime, dev))
            {
                g.local_reset |= SLRESHADE_RESET_SOURCE_LOSS;
            }
        }
        if (g.uniforms_dirty)
        {
            g.stage = "rebuild_uniforms";
            rebuild_uniform_cache(runtime);
        }
        FrameStatus st = {};   // semantic_valid = 0, tail_valid = 0
        st.reset_flags = g.local_reset;
        push_uniforms(runtime, frame_ok ? &f : nullptr, st);
        g.local_reset = 0;
        g.stage = "idle";
        return;
    }

    // ABI 1.2 gate: the visible_diffuse slot occupies tail words 20..23, which
    // a 1.1 writer still treats as reserved (zeroed) space. Only a writer whose
    // MINOR says the slot exists may assert its validity bit -- mask it off
    // otherwise, BEFORE the slot loop and the status assembly, so a 1.1 viewer
    // degrades to "not valid" (slot fails closed to neutral) and never to
    // garbage read out of reserved words. The texture words themselves are
    // never dereferenced when the bit is clear (update_slot short-circuits).
    g.stage = "abi_minor_gate";
    if (SLReShade_ABI_Minor(f.abi_version) < SLRESHADE_ABI_MINOR_V1_2)
    {
        tail.semantic_valid_bits &= ~SLRESHADE_SEM_VALID_VISIBLE_DIFFUSE;
    }

    // Rebuild the uniform cache and the demand gate BEFORE touching slots:
    // slot updates must see THIS frame's loaded-effect set, or the first frame
    // after an effect reload runs against the previous set's consumed flags
    // (the reload-ordering hole the 2026-07-25 cross-review flagged).
    if (g.uniforms_dirty)
    {
        g.stage = "rebuild_uniforms";
        rebuild_uniform_cache(runtime);
    }

    // Same frame republished (paused / minimized)? Uniforms are cheap; still
    // push them (effects may reload between viewer frames), but skip copies --
    // the destinations already hold copies of THIS viewer frame, so FED slots
    // remain fresh by definition.
    const bool new_frame = (f.frame_counter != g.last_frame_counter);
    g.last_frame_counter = f.frame_counter;

    if (new_frame)
    {
        // Viewer-side render-target reallocation is signalled EXPLICITLY via
        // the tail's generation counter -- rebuild every slot rather than
        // inferring staleness from {name,size,format} alone (those can be
        // reallocated identically and still hold different storage). Handled
        // by FORCING update_slot's recreate branch, NOT by destroying
        // destinations here: destroying at this level left slots whose
        // update_slot exit did not rebuild (demand-gated, newly invalid) with
        // `bound` bookkeeping pointing at a destroyed destination -- the
        // inconsistent third state the choke point forbids.
        g.stage = "generation_check";
        bool force_recreate = false;
        const uint64_t gen = SLReShadeV11_GetGeneration(&tail);
        if (g.generation_seen && gen != g.last_generation)
        {
            force_recreate = true;
            g.local_reset |= SLRESHADE_RESET_TARGET_RECREATED;
            logf(reshade::log::level::info,
                 "[SLBridge] render_target_generation %llu -> %llu -- slots rebuilt",
                 (unsigned long long)g.last_generation, (unsigned long long)gen);
        }
        g.last_generation = gen;
        g.generation_seen = true;

        for (GBufferSlot &s : g.slots)
        {
            update_slot(s, (tail.semantic_valid_bits & s.sem_valid_bit) != 0,
                        slot_source(f, tail, s.source_id), runtime, cmd,
                        force_recreate);
        }
        maybe_log_copy_stats();
#if SL_PROVIDE_DEPTH
        update_depth_binding(f.depth, runtime);
#endif
    }

    // EFFECTIVE validity for the FX layer: the viewer's claim AND the slot
    // actually being fed by this add-on this frame. Non-slot semantics (DEPTH
    // via generic_depth, EMISSIVE unpublished) pass through the tail's bits.
    g.stage = "assemble_status";
    FrameStatus st = {};
    st.tail_valid        = 1;
    st.motion_encoding   = tail.motion_encoding;
    st.orientation_flags = tail.source_orientation_flags;
    st.motion_coverage   = tail.motion_coverage_bits;
    st.config_flags      = tail.effective_config_flags;

    uint32_t eff = tail.semantic_valid_bits &
                   (SLRESHADE_SEM_VALID_DEPTH | SLRESHADE_SEM_VALID_EMISSIVE);
    for (const GBufferSlot &s : g.slots)
    {
        if ((tail.semantic_valid_bits & s.sem_valid_bit) != 0 &&
            s.bound == GBufferSlot::BIND_DEST)
        {
            eff |= s.sem_valid_bit;
        }
    }
    st.semantic_valid = eff;
    st.reset_flags    = tail.history_reset_flags | g.local_reset;
    if (!g.seen_valid && eff != 0)
    {
        g.seen_valid = true;
        st.reset_flags |= SLRESHADE_RESET_FIRST_VALID_FRAME;
    }

    push_uniforms(runtime, &f, st);
    g.local_reset = 0;
    g.stage = "idle";
}

// SEH-guarded wrapper: whatever faults inside our per-frame work, log the
// exception code + the stage breadcrumb and permanently self-disable -- the
// bridge goes inert but ReShade's hook SURVIVES (no more silent soft-crash).
// NOTE: this function must contain no C++ objects needing unwinding (C2712).
static void on_begin_effects(effect_runtime *runtime, command_list *cmd,
                             resource_view /*rtv*/, resource_view /*rtv_srgb*/)
{
    if (g.dead)
    {
        return;
    }
    unsigned long code = 0;
    __try
    {
        do_begin_effects(runtime, cmd);
    }
    __except ((code = GetExceptionCode()), EXCEPTION_EXECUTE_HANDLER)
    {
        g.dead = true;
        logf(reshade::log::level::error,
             "[SLBridge] FATAL: exception 0x%08lX at stage '%s' -- bridge "
             "self-disabled for this session (ReShade keeps running)",
             code, g.stage);
    }
}

// -----------------------------------------------------------------------------
// DLL entry
// -----------------------------------------------------------------------------
BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID)
{
    switch (fdwReason)
    {
    case DLL_PROCESS_ATTACH:
        if (!reshade::register_addon(hModule))
        {
            return FALSE;   // ReShade not present or incompatible -- refuse load
        }
        reshade::register_event<reshade::addon_event::init_effect_runtime>(&on_init_effect_runtime);
        reshade::register_event<reshade::addon_event::destroy_effect_runtime>(&on_destroy_effect_runtime);
        reshade::register_event<reshade::addon_event::reshade_reloaded_effects>(&on_reloaded_effects);
        reshade::register_event<reshade::addon_event::reshade_begin_effects>(&on_begin_effects);
        break;

    case DLL_PROCESS_DETACH:
        reshade::unregister_addon(hModule);
        break;
    }
    return TRUE;
}
