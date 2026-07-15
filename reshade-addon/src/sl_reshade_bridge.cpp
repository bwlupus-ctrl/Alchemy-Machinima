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
 *   2. copies the viewer's G-buffer textures into add-on-OWNED immutable
 *      resources created through the ReShade device API,
 *   3. binds those via update_texture_bindings() semantics, and
 *   4. pushes camera uniforms to effect uniforms annotated
 *      < source = "sl_..."; >.
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
 * Robustness rules honored throughout:
 *   - ABI major mismatch, missing export, or missing viewer data => inert no-op.
 *   - Never cache viewer GL names across frames; slots revalidate against
 *     {name, size, format} every frame and recreate on any change.
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

// -----------------------------------------------------------------------------
// GL internal format -> ReShade api::format for the resources we replicate.
// Unmapped formats disable that slot (fail soft, log once).
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
    default:                         return format::unknown;
    }
}

// -----------------------------------------------------------------------------
// One replicated G-buffer slot: an add-on-owned immutable texture + SRV bound
// to a ReShade FX semantic, revalidated against the published source each frame.
// -----------------------------------------------------------------------------
struct GBufferSlot
{
    const char *semantic;                   // FX: texture Tex : SEMANTIC;
    const SLReShadeTexture *(*pick)(const SLReShadeFrame &); // field selector

    // live state
    resource      dest      = { 0 };
    resource_view dest_srv  = { 0 };
    uint32_t      width     = 0;
    uint32_t      height    = 0;
    format        fmt       = format::unknown;
    bool          warned    = false;        // one-shot unmapped-format warning

    void destroy(device *dev)
    {
        if (dest_srv != 0) { dev->destroy_resource_view(dest_srv); dest_srv = { 0 }; }
        if (dest != 0)     { dev->destroy_resource(dest);          dest     = { 0 }; }
        width = height = 0;
        fmt = format::unknown;
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

static const SLReShadeTexture *pick_normals (const SLReShadeFrame &f) { return &f.normals;   }
static const SLReShadeTexture *pick_motion  (const SLReShadeFrame &f) { return &f.motion;    }
static const SLReShadeTexture *pick_albedo  (const SLReShadeFrame &f) { return &f.albedo;    }
static const SLReShadeTexture *pick_orm     (const SLReShadeFrame &f) { return &f.orm;       }
static const SLReShadeTexture *pick_colorhdr(const SLReShadeFrame &f) { return &f.color_hdr; }

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

    GBufferSlot slots[5] = {
        { "SL_NORMALS",   &pick_normals  },
        { "SL_MOTION_NDC",&pick_motion   },
        { "SL_ALBEDO",    &pick_albedo   },
        { "SL_ORM",       &pick_orm      },
        { "SL_COLOR_HDR", &pick_colorhdr },
    };

    DepthSlot depth;   // ReShade's DEPTH semantic, bound direct (see DepthSlot)

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

// uniform annotation sources: < source = "sl_view_matrix"; > etc.
struct UniformSourceDef { const char *name; uint32_t count; };
static const UniformSourceDef kUniformSources[] = {
    { "sl_view_matrix",   16 },  // row-major LLMatrix4 (row-vector convention)
    { "sl_proj_matrix",   16 },
    { "sl_camera_pos",     3 },
    { "sl_camera_at",      3 },
    { "sl_camera_left",    3 },
    { "sl_camera_up",      3 },
    { "sl_near_far",       2 },
    { "sl_fov_y",          1 },
    { "sl_aspect",         1 },
    { "sl_frame_counter",  1 },
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
// Ensure a slot's add-on-owned destination matches the published source; then
// record the copy. Returns true if the slot is bound and copied this frame.
// -----------------------------------------------------------------------------
static void update_slot(GBufferSlot &slot, const SLReShadeTexture &src_tex,
                        effect_runtime *runtime, command_list *cmd)
{
    device *dev = runtime->get_device();

    if (src_tex.gl_name == 0 || src_tex.width == 0 || src_tex.height == 0)
    {
        // Source gone (feature off, RT rebuild in progress): keep the stale
        // dest texture alive so effects sample the last good frame instead of
        // black, but skip the copy.
        return;
    }

    const format want = map_gl_internal_format(src_tex.gl_internal_format);
    if (want == format::unknown)
    {
        if (!slot.warned)
        {
            slot.warned = true;
            logf(reshade::log::level::warning,
                 "[SLBridge] %s: unmapped GL internal format 0x%04X -- slot disabled",
                 slot.semantic, src_tex.gl_internal_format);
        }
        return;
    }

    // (Re)create the destination when size/format changes (window resize,
    // graphics-setting change, HDR toggle).
    if (slot.dest == 0 || slot.width != src_tex.width ||
        slot.height != src_tex.height || slot.fmt != want)
    {
        g.stage = "destroy_stale_slot";
        slot.destroy(dev);
        g.stage = "create_resource";

        const resource_desc desc(
            src_tex.width, src_tex.height, 1 /*layers*/, 1 /*levels*/, want,
            1 /*samples*/, memory_heap::gpu_only,   // ("default_" in newer SDKs)
            resource_usage::shader_resource | resource_usage::copy_dest);

        if (!dev->create_resource(desc, nullptr, resource_usage::shader_resource, &slot.dest))
        {
            logf(reshade::log::level::error,
                 "[SLBridge] %s: create_resource %ux%u fmt %d failed",
                 slot.semantic, src_tex.width, src_tex.height, (int)want);
            slot.dest = { 0 };
            return;
        }
        g.stage = "create_resource_view";
        if (!dev->create_resource_view(slot.dest, resource_usage::shader_resource,
                                       resource_view_desc(want), &slot.dest_srv))
        {
            logf(reshade::log::level::error,
                 "[SLBridge] %s: create_resource_view failed", slot.semantic);
            slot.destroy(dev);
            return;
        }

        slot.width  = src_tex.width;
        slot.height = src_tex.height;
        slot.fmt    = want;

        // Bind the (new) SRV to the semantic for all loaded effects.
        g.stage = "update_texture_bindings";
        runtime->update_texture_bindings(slot.semantic, slot.dest_srv, slot.dest_srv);
        logf(reshade::log::level::info,
             "[SLBridge] %s: bound %ux%u fmt %d (src GL %u)",
             slot.semantic, slot.width, slot.height, (int)want, src_tex.gl_name);
    }

    // Copy viewer texture -> our immutable texture. Source completeness is
    // irrelevant for copies (only sampling needs mip-complete textures).
    g.stage = "copy_texture_region";
    const resource src = wrap_gl_texture(src_tex.gl_name);
    cmd->barrier(slot.dest, resource_usage::shader_resource, resource_usage::copy_dest);
    cmd->copy_texture_region(src, 0, nullptr, slot.dest, 0, nullptr,
                             filter_mode::min_mag_mip_point);
    cmd->barrier(slot.dest, resource_usage::copy_dest, resource_usage::shader_resource);
    g.stage = "post_copy";
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
// Uniform cache: rebuilt after every effect (re)load.
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
        s.dest = { 0 }; s.dest_srv = { 0 };
        s.width = s.height = 0; s.fmt = format::unknown;
    }
    g.depth.forget();
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
        s.dest = { 0 }; s.dest_srv = { 0 };
        s.width = s.height = 0; s.fmt = format::unknown;
    }
    g.depth.forget();   // view belongs to the dying context; drop, don't destroy
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
    // Re-issue semantic bindings: effect reload resets texture bindings.
    // Only if the SRVs belong to this runtime's device (multi-context guard).
    if (runtime->get_device() != g.dev)
    {
        return;
    }
    for (GBufferSlot &s : g.slots)
    {
        if (s.dest_srv != 0)
        {
            runtime->update_texture_bindings(s.semantic, s.dest_srv, s.dest_srv);
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
            s.dest = { 0 }; s.dest_srv = { 0 };
            s.width = s.height = 0; s.fmt = format::unknown;
        }
        g.depth.forget();
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
    if (!read_viewer_frame(f) || !(f.flags & SLRESHADE_FLAG_VALID))
    {
        return;
    }

#if defined(SL_PROBE) && SL_PROBE == 2
    return;   // probe 2: viewer export called + frame read, but no GL work
#endif

    // Same frame republished (paused / minimized)? Uniforms are cheap; still
    // push them (effects may reload between viewer frames), but skip copies.
    const bool new_frame = (f.frame_counter != g.last_frame_counter);
    g.last_frame_counter = f.frame_counter;

    if (new_frame)
    {
        for (GBufferSlot &s : g.slots)
        {
            update_slot(s, *s.pick(f), runtime, cmd);
        }
#if SL_PROVIDE_DEPTH
        update_depth_binding(f.depth, runtime);
#endif
    }

    if (g.uniforms_dirty)
    {
        g.stage = "rebuild_uniforms";
        rebuild_uniform_cache(runtime);
    }
    g.stage = "push_uniforms";
    float scratch[16];
    for (const BridgeState::UniformTarget &u : g.uniforms)
    {
        const float *data = uniform_source_data(f, u.source, scratch);
        if (data != nullptr)
        {
            runtime->set_uniform_value_float(u.var, data, kUniformSources[u.source].count);
        }
    }
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
