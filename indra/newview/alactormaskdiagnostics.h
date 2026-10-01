/**
 * @file alactormaskdiagnostics.h
 * @brief [ActorMask] WP7 instrumentation: summary line, query verdicts,
 *        debug view and the isolation/parity test instrument.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 *
 * Design: doc/CINE_RIG_ACTOR_MASK_BRIEF_V3.md sec. 9. Every entry point is reached
 * only from code that already sits behind ALCineLightRigManager::anyActorOnly(),
 * so the off path never enters this file (no log lines, no draws, no
 * readbacks). The single exception is the CineLightRigActorMaskTest commit
 * listener, which runs only when the user arms the test.
 */

#ifndef AL_ACTORMASKDIAGNOSTICS_H
#define AL_ACTORMASKDIAGNOSTICS_H

#include "stdtypes.h"

#include <memory>

class LLGLSLShader;
class LLPipeline;
class LLRenderTarget;

// [ActorMask] Debug-view program, registered/unloaded by LLViewerShaderMgr.
extern LLGLSLShader gActorMaskDebugProgram;

class ALActorMaskDiagnostics
{
public:
    static ALActorMaskDiagnostics& instance();

    // Once, from LLPipeline::init: listener that answers an arm request while
    // no Actor-only light exists (the per-frame tick never runs then).
    void connectSettings();

    // Draws issued by one recipient's real (tight) mask replay, any MASK view.
    void noteMaskDraws(U32 draws);

    // Every lighting view with anyActorOnly(), after classification and before
    // the test-mode routing is read. Main view: drains queries, writes verdicts
    // and the 5 s summary, and sequences the test instrument.
    void onLightingView(LLPipeline& pipe, bool main_view);

    // Main view, right after the local-light block (screen still bound): C_deferred.
    void captureDeferred(LLPipeline& pipe, LLRenderTarget& screen);

    // Main view, at the end of renderDeferredLighting before the screen flush:
    // C_post capture, then the debug view (CineLightRigActorMaskDebug 1/2).
    void endMainView(LLPipeline& pipe, LLRenderTarget& screen);

    // Extended-guard leak counter (LLScopedGhostRenderInvariant ACTOR_MASK mode).
    U64* stateLeakCounter() { return &mStateLeaks; }

private:
    ALActorMaskDiagnostics();
    ~ALActorMaskDiagnostics();
    ALActorMaskDiagnostics(const ALActorMaskDiagnostics&) = delete;
    ALActorMaskDiagnostics& operator=(const ALActorMaskDiagnostics&) = delete;

    struct Impl;
    std::unique_ptr<Impl> mImpl;
    U64 mStateLeaks = 0;
};

#endif // AL_ACTORMASKDIAGNOSTICS_H
