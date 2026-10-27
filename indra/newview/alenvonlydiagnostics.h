/** Environment-only lighting diagnostic; viewer execution is user-owned. */
#pragma once
class LLRenderTarget;
namespace ALEnvOnlyDiagnostics
{
    // Called only for the main display view, never mirrors/captures/Prism.
    void captureDeferred(LLRenderTarget& color, LLRenderTarget& gbuffer);
    void endView(LLRenderTarget& color);
    // Non-GL restoration, also called before persistent settings are saved.
    void cancel();
}
