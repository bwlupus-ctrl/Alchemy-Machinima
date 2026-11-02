#include "llviewerprecompiledheaders.h"
#include "alpovblink.h"
#include "alpovblinkmath.h"
#include "llviewercontrol.h"
#include "llviewershadermgr.h"
#include "llrendertarget.h"
#include "llvertexbuffer.h"
#include "lltimer.h"

namespace
{
ALPOVBlinkMath::State state;
LLRenderTarget presentation;
bool ui_held = false, pad_held = false;
ALPOVBlinkMath::Params params()
{
    static LLCachedControl<F32> close(gSavedSettings, "POVBlinkCloseMs", 90.f);
    static LLCachedControl<F32> hold(gSavedSettings, "POVBlinkHoldMs", 40.f);
    static LLCachedControl<F32> open(gSavedSettings, "POVBlinkOpenMs", 180.f);
    static LLCachedControl<F32> depth(gSavedSettings, "POVBlinkDepth", 1.f);
    return {close()*.001, hold()*.001, open()*.001, depth()};
}
void updateHold() { state.hold(LLTimer::getTotalSeconds(), ui_held || pad_held, params()); }
}
void ALPOVBlink::blink() { state.blink(LLTimer::getTotalSeconds(), params()); }
void ALPOVBlink::setUIHeld(bool held) { ui_held = held; updateHold(); }
bool ALPOVBlink::uiHeld() { return ui_held; }
void ALPOVBlink::setPadHeld(bool held)
{
    if (pad_held != held) { pad_held = held; updateHold(); }
}
LLRenderTarget* ALPOVBlink::beginPresentation(unsigned width, unsigned height, float& amount)
{
    static LLCachedControl<bool> automatic(gSavedSettings, "POVBlinkAuto", false);
    static LLCachedControl<F32> rate(gSavedSettings, "POVBlinkRate", 15.f);
    amount = (F32)state.update(LLTimer::getTotalSeconds(), params(), automatic(), rate());
    if (!ALPOVBlinkMath::shouldDraw(amount, true) || !gPOVBlinkProgram.isComplete()) return nullptr;
    if (presentation.getWidth() != width || presentation.getHeight() != height)
    {
        presentation.release();
        // Preserve final colour precision and the final blit's warped scene depth.
        if (!presentation.allocate(width, height, GL_RGBA16F, true)) return nullptr;
    }
    if (!presentation.isComplete()) return nullptr;
    presentation.bindTarget();
    return &presentation;
}
void ALPOVBlink::endPresentation(LLVertexBuffer* triangle, float amount)
{
    LL_PROFILE_GPU_ZONE("POV eye blink");
    presentation.flush();
    static LLCachedControl<F32> curve(gSavedSettings, "POVBlinkCurve", .35f);
    static LLCachedControl<F32> soft(gSavedSettings, "POVBlinkSoftness", .02f);
    static LLCachedControl<F32> dark(gSavedSettings, "POVBlinkDarken", .35f);
    static LLCachedControl<F32> blur(gSavedSettings, "POVBlinkBlur", .4f);
    static LLCachedControl<U32> style(gSavedSettings, "POVBlinkStyle", 0U);
    LLGLDepthTest depth(GL_TRUE, GL_TRUE, GL_ALWAYS);
    LLGLDisable blend(GL_BLEND);
    auto& shader = gPOVBlinkProgram;
    shader.bind();
    shader.uniform2f(LLStaticHashedString("screen_res"), (F32)presentation.getWidth(), (F32)presentation.getHeight());
    shader.uniform4f(LLStaticHashedString("blink_shape"), amount,
        (F32)ALPOVBlinkMath::unit(curve()), (F32)ALPOVBlinkMath::unit(soft()), style() == 1 ? 1.f : 0.f);
    shader.uniform2f(LLStaticHashedString("blink_look"),
        (F32)ALPOVBlinkMath::unit(dark()), (F32)ALPOVBlinkMath::unit(blur()));
    shader.bindTexture(LLShaderMgr::DIFFUSE_MAP, &presentation, false, LLTexUnit::TFO_BILINEAR);
    shader.bindTexture(LLShaderMgr::DEFERRED_DEPTH, &presentation, true);
    triangle->setBuffer();
    triangle->drawArrays(LLRender::TRIANGLES, 0, 3);
    shader.unbind();
}
void ALPOVBlink::releaseGL() { presentation.release(); }
