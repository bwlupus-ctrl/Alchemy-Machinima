#include "llviewerprecompiledheaders.h"
#include "alpovblink.h"
#include "alpovblinkmath.h"
#include "llviewercontrol.h"
#include "llviewershadermgr.h"
#include "llvertexbuffer.h"
#include "lltimer.h"
#include "lluuid.h"
#include "llimagegl.h"

namespace
{
ALPOVBlinkMath::State state;
// Texture only: even expiry has no framebuffer bind and no depth attachment.
struct SamplingTexture
{
    U32 name = 0, width = 0, height = 0;
    void release()
    {
        if (name) LLImageGL::deleteTextures(1, &name);
        name = width = height = 0;
    }
    U32 getWidth() const { return width; }
    U32 getHeight() const { return height; }
    U32 getTexture() const { return name; }
    bool isComplete() const { return name != 0; }
    bool allocate(int w, int h, U32 format)
    {
        LLImageGL::generateTextures(1, &name);
        if (!name) return false;
        gGL.getTexUnit(0)->bindManual(LLTexUnit::TT_TEXTURE, name);
        clear_glerror();
        LLImageGL::setManualImage(GL_TEXTURE_2D, 0, format, w, h,
                                 GL_RGBA, GL_UNSIGNED_BYTE, nullptr, false);
        if (glGetError() != GL_NO_ERROR)
        {
            LLImageGLMemory::free_cur_tex_image();
            release();
            return false;
        }
        width = w; height = h;
        gGL.getTexUnit(0)->setTextureFilteringOption(LLTexUnit::TFO_POINT);
        gGL.getTexUnit(0)->setTextureAddressMode(LLTexUnit::TAM_CLAMP);
        return true;
    }
} sample_target;
bool ui_held = false, pad_held = false, pad_escape = false;
unsigned failed_width = 0, failed_height = 0;
double last_active = 0.;
ALPOVBlinkMath::Params params()
{
    static LLCachedControl<F32> close(gSavedSettings, "POVBlinkCloseMs", 90.f);
    static LLCachedControl<F32> hold(gSavedSettings, "POVBlinkHoldMs", 40.f);
    static LLCachedControl<F32> open(gSavedSettings, "POVBlinkOpenMs", 180.f);
    static LLCachedControl<F32> depth(gSavedSettings, "POVBlinkDepth", 1.f);
    return {close()*.001, hold()*.001, open()*.001, depth()};
}
void updateHold() { state.hold(LLTimer::getTotalSeconds(), ui_held || pad_held, params()); }
struct Overlay
{
    LLVertexBuffer* triangle;
    float amount, curve, soft, dark, blur;
    bool warm, sampling;
    GLint viewport[4] = {};
    U32 source = 0;
    bool acquire(int width, int height)
    {
        GLint bits = 8, draw_fbo = 0, draw_buffer = 0;
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw_fbo);
        glGetIntegerv(GL_DRAW_BUFFER, &draw_buffer);
        glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER,
            draw_fbo ? (GLenum)draw_buffer : GL_BACK_LEFT,
            GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE, &bits);
        // Never borrow AA scratch: a later sensor/debug frame or capture can read
        // it without refreshing, even after settings change and blinking stops.
        if (failed_width == (U32)width && failed_height == (U32)height) return false;
        if (sample_target.getWidth() != (U32)width || sample_target.getHeight() != (U32)height)
        {
            sample_target.release();
            if (!sample_target.allocate(width, height, bits > 10 ? GL_RGBA16 : bits > 8 ? GL_RGB10_A2 : GL_RGBA8))
            {
                failed_width = width; failed_height = height;
                sample_target.release();
                return false; // Latched until resize / releaseGL.
            }
        }
        source = sample_target.getTexture();
        return sample_target.isComplete();
    }
    void copy(int y, int height)
    {
        gGL.flush();
        GLint draw_fbo, draw_buffer, read_fbo, read_buffer, source_read_buffer;
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw_fbo);
        glGetIntegerv(GL_DRAW_BUFFER, &draw_buffer);
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read_fbo);
        glGetIntegerv(GL_READ_BUFFER, &read_buffer);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, draw_fbo);
        glGetIntegerv(GL_READ_BUFFER, &source_read_buffer);
        glReadBuffer(draw_buffer);
        gGL.getTexUnit(0)->bindManual(LLTexUnit::TT_TEXTURE, source);
        glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, y,
            viewport[0], viewport[1]+y, viewport[2], height);
        glReadBuffer(source_read_buffer);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, read_fbo);
        glReadBuffer(read_buffer);
    }
    void bind()
    {
        auto& shader = gPOVBlinkProgram;
        shader.bind();
        shader.uniform2f(LLStaticHashedString("screen_res"), (F32)viewport[2], (F32)viewport[3]);
        shader.uniform4f(LLStaticHashedString("blink_shape"), amount, curve, soft, warm ? 1.f : 0.f);
        shader.uniform2f(LLStaticHashedString("blink_look"), dark, blur);
        shader.uniform1i(LLStaticHashedString("blink_sampling"), sampling ? 1 : 0);
        shader.uniform1i(LLShaderMgr::DITHER_BITS, LLRender::s10bitBackBuffer ? 10 : 8);
        if (sampling)
        {
            const S32 unit = shader.enableTexture(LLShaderMgr::DIFFUSE_MAP);
            gGL.getTexUnit(unit)->bindManual(LLTexUnit::TT_TEXTURE, source);
            gGL.getTexUnit(unit)->setTextureFilteringOption(LLTexUnit::TFO_POINT);
        }
    }
    void drawSceneImage()
    {
        LL_PROFILE_GPU_ZONE("POV eye blink");
        LLGLDepthTest depth(GL_FALSE, GL_FALSE);
        LLGLEnable blend(GL_BLEND);
        LLRender::eBlendFactor cs, cd, as, ad;
        gGL.getBlendFunc(cs, cd, as, ad);
        // Separate alpha factors preserve destination alpha without mask changes.
        gGL.blendFunc(LLRender::BF_SOURCE_ALPHA, LLRender::BF_ONE_MINUS_SOURCE_ALPHA,
                      LLRender::BF_ZERO, LLRender::BF_ONE);
        triangle->setBuffer();
        triangle->drawArrays(LLRender::TRIANGLES, 0, 3);
        gPOVBlinkProgram.unbind();
        gGL.blendFunc(cs, cd, as, ad);
    }
};
}
void ALPOVBlink::blink()
{
    // F9 / pad tap is an escape even with the entire UI hidden.
    if (ui_held) { setUIHeld(false); return; }
    state.blink(LLTimer::getTotalSeconds(), params());
}
void ALPOVBlink::setUIHeld(bool held) { ui_held = held; updateHold(); }
bool ALPOVBlink::uiHeld() { return ui_held; }
void ALPOVBlink::setPadHeld(bool held)
{
    if (!held) pad_escape = false;
    if (held && ui_held) { ui_held = false; pad_escape = true; }
    const bool effective = held && !pad_escape;
    if (pad_held != effective) { pad_held = effective; updateHold(); }
    else if (pad_escape) updateHold();
}
void ALPOVBlink::renderLastSceneImage(bool live, LLVertexBuffer* triangle)
{
    if (!live) return;
    static const bool seeded = []
    {
        LLUUID random; random.generate();
        U32 seed = 0;
        for (U8 byte : random.mData) seed = seed*33U + byte;
        state.scheduler.seed = seed ? seed : 1U;
        return true;
    }();
    (void)seeded;
    static LLCachedControl<bool> automatic(gSavedSettings, "POVBlinkAuto", false);
    static LLCachedControl<F32> rate(gSavedSettings, "POVBlinkRate", 15.f);
    static LLCachedControl<F32> curve(gSavedSettings, "POVBlinkCurve", .35f);
    static LLCachedControl<F32> soft(gSavedSettings, "POVBlinkSoftness", .02f);
    static LLCachedControl<F32> dark(gSavedSettings, "POVBlinkDarken", .35f);
    static LLCachedControl<F32> blur(gSavedSettings, "POVBlinkBlur", .4f);
    static LLCachedControl<U32> style(gSavedSettings, "POVBlinkStyle", 0U);
    const double now = LLTimer::getTotalSeconds();
    const F32 amount = (F32)state.update(now, params(), automatic(), rate());
    if (amount > 0.f) last_active = now;
    else if (!automatic() && now-last_active >= 10.) sample_target.release();
    Overlay overlay{triangle, amount,
        (F32)ALPOVBlinkMath::unit(curve()), (F32)ALPOVBlinkMath::unit(soft()),
        (F32)ALPOVBlinkMath::unit(dark()), (F32)ALPOVBlinkMath::unit(blur()),
        style() == 1, style() == 1 || dark() > 0.f || blur() > 0.f};
    if (!overlay.sampling) sample_target.release();
    std::copy(gGLViewport, gGLViewport+4, overlay.viewport);
    // Tested wrapper is the only admission path to allocation/bind/copy/draw.
    ALPOVBlinkMath::present(amount, live && gPOVBlinkProgram.isComplete(),
        overlay.viewport[2], overlay.viewport[3], overlay.curve, overlay.soft,
        overlay.blur, overlay.sampling, overlay);
}
void ALPOVBlink::releaseGL()
{
    sample_target.release();
    failed_width = failed_height = 0;
}
