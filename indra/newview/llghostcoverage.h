/**
 * @file llghostcoverage.h
 * @brief [GhostDeferred] Shared per-category coverage vocabulary for scene-lit
 *        Ghost Studio clones.
 *
 * The deferred submission (LLPipeline::renderGhostDeferredOpaqueMasked) records
 * WHICH render categories of a clone instance it actually drew into the
 * G-buffer this frame; the overlay (LLActorMover::renderStudioGhosts /
 * drawGeometryGhost) then colors ONLY the categories deferred did NOT cover.
 * A single all-or-nothing "submitted" bit is exactly wrong here: one rigged
 * opaque batch drawing must not suppress the clone's blended hair, additive
 * glow, or static attachment faces -- they would render NOWHERE (the in-world
 * face-shaped holes / broken hair / vanishing scripted attachment symptoms).
 *
 * Five categories, matching the overlay's sweep structure (SWEEP_SOLID/BLEND/
 * GLOW over rigged batches, solid/blend over static faces). They suppress
 * independently: the blend slice can cover rigged blend without static blend,
 * and the static slice can cover static solid without static blend -- folding
 * any pair together would reintroduce all-or-nothing suppression at a smaller
 * scale. Slice 1 only ever sets RIGGED_SOLID; later slices set theirs without
 * another interface change. This header stays dependency-light on purpose:
 * both LLPipeline (producer) and LLActorMover (consumer) need the vocabulary,
 * and neither should include the other's world for it.
 */
#ifndef LL_LLGHOSTCOVERAGE_H
#define LL_LLGHOSTCOVERAGE_H

#include "stdtypes.h"

typedef U32 GhostCoverageMask;

enum EGhostCoverage : U32
{
    GHOST_COVERAGE_NONE         = 0,
    GHOST_COVERAGE_RIGGED_SOLID = 1u << 0,  // rigged opaque + cutoff-masked
    GHOST_COVERAGE_RIGGED_BLEND = 1u << 1,  // rigged alpha-blended
    GHOST_COVERAGE_RIGGED_GLOW  = 1u << 2,  // rigged additive emissive (duplicate geometry)
    GHOST_COVERAGE_STATIC_SOLID = 1u << 3,  // non-rigged attachment faces, opaque + masked
    GHOST_COVERAGE_STATIC_BLEND = 1u << 4,  // non-rigged attachment faces, alpha-blended

    GHOST_COVERAGE_ALL = GHOST_COVERAGE_RIGGED_SOLID
                       | GHOST_COVERAGE_RIGGED_BLEND
                       | GHOST_COVERAGE_RIGGED_GLOW
                       | GHOST_COVERAGE_STATIC_SOLID
                       | GHOST_COVERAGE_STATIC_BLEND
};

// [GhostDeferred] Overlay sweep classification of a harvested rigged pass
// (an LLRenderPass::PASS_*_RIGGED value). Defined in llactormover.cpp; shared
// with the deferred submission so coverage and suppression use ONE domain: a
// batch the overlay would draw SOLID (neither blend nor glow) that the
// deferred pass cannot cover MUST block the RIGGED_SOLID bit, or the
// suppressed solid sweep would leave that batch rendering nowhere.
bool ghost_pass_is_blend(U32 pass);
bool ghost_pass_is_glow(U32 pass);

#endif // LL_LLGHOSTCOVERAGE_H
