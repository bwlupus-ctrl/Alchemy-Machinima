/**
 * @file llghostdeferreddiagnostics.h
 * @brief GL-state invariant checking for Ghost Studio deferred rendering.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 *
 * Scoped guard that snapshots the GL render state on entry and compares it on
 * exit. If the wrapped Ghost Studio deferred submission leaked any tracked state
 * (modelview / texture matrix / colour mask / depth / blend / cull / shader /
 * bound buffers / FBO / viewport / gGLLastMatrix), it emits ONE verdict line and
 * increments a violation counter -- it does NOT restore, because restoring would
 * hide the contamination from the caller. The (glGet-heavy) capture only runs
 * when GhostDeferredDebugLog is on, so it is free in normal play.
 */

#ifndef LL_LLGHOSTDEFERREDDIAGNOSTICS_H
#define LL_LLGHOSTDEFERREDDIAGNOSTICS_H

#include "stdtypes.h"

#include <memory>

class LLScopedGhostRenderInvariant
{
public:
    LLScopedGhostRenderInvariant(const char* scope, U64* violation_counter);
    ~LLScopedGhostRenderInvariant();

    LLScopedGhostRenderInvariant(const LLScopedGhostRenderInvariant&) = delete;
    LLScopedGhostRenderInvariant& operator=(const LLScopedGhostRenderInvariant&) = delete;

    // Captures and compares the post-scope state. Safe to call more than once.
    void finish();

private:
    struct Snapshot;

    const char* mScope = nullptr;
    U64* mViolationCounter = nullptr;
    std::unique_ptr<Snapshot> mBefore;
    bool mFinished = false;
};

#endif // LL_LLGHOSTDEFERREDDIAGNOSTICS_H
