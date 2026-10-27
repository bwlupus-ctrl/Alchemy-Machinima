/**
 * Environment-only F0 / F1 / F2 / F1-prime instrument.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "llviewerprecompiledheaders.h"
#include "alenvonlydiagnostics.h"
#include "alsettingsundohistory.h"
#include "alkithistory.h"
#include "llrender.h"
#include "llrendertarget.h"
#include "llviewercontrol.h"
#include "lleventtimer.h"
#include "llcallbacklist.h"
#include "lltimer.h"
#include "pipeline.h"
#include <array>
#include <cmath>
#include <cstring>
#include <vector>

namespace
{
constexpr const char* COMMAND = "AlchemyEnvEnvironmentOnlyTest";
constexpr const char* MODE = "AlchemyEnvEnvironmentOnly";
constexpr const char* SUN = "AlchemyEnvSunEV";
struct Frame
{
    U32 width = 0, height = 0;
    std::vector<F32> deferred, post, normals, depth;
};

// Saves all GL readback state touched by the instrument. No framebuffer or
// buffer mutation occurs in the normal-play path.
bool read(LLRenderTarget& target, U32 attachment, std::vector<F32>& out, bool depth = false)
{
    if (!target.getFBO() || !target.getWidth() || !target.getHeight()) return false;
    GLint fbo = 0, buffer = 0, pack = 0, alignment = 0, row_length = 0, skip_rows = 0, skip_pixels = 0;
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &fbo);
    glGetIntegerv(GL_READ_BUFFER, &buffer);
    glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &pack);
    glGetIntegerv(GL_PACK_ALIGNMENT, &alignment);
    glGetIntegerv(GL_PACK_ROW_LENGTH, &row_length);
    glGetIntegerv(GL_PACK_SKIP_ROWS, &skip_rows);
    glGetIntegerv(GL_PACK_SKIP_PIXELS, &skip_pixels);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, target.getFBO());
    glReadBuffer(GL_COLOR_ATTACHMENT0 + attachment);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glPixelStorei(GL_PACK_ROW_LENGTH, 0);
    glPixelStorei(GL_PACK_SKIP_ROWS, 0);
    glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
    for (U32 i = 0; i < 16 && glGetError() != GL_NO_ERROR; ++i) {}
    out.resize(static_cast<size_t>(target.getWidth()) * target.getHeight() * (depth ? 1 : 4));
    glReadPixels(0, 0, target.getWidth(), target.getHeight(), depth ? GL_DEPTH_COMPONENT : GL_RGBA, GL_FLOAT, out.data());
    const bool ok = glGetError() == GL_NO_ERROR;
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
    glReadBuffer(buffer);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, pack);
    glPixelStorei(GL_PACK_ALIGNMENT, alignment);
    glPixelStorei(GL_PACK_ROW_LENGTH, row_length);
    glPixelStorei(GL_PACK_SKIP_ROWS, skip_rows);
    glPixelStorei(GL_PACK_SKIP_PIXELS, skip_pixels);
    return ok;
}
bool close(F32 a, F32 b) { return std::isfinite(a) && std::isfinite(b) && fabsf(a-b) <= .001f + .01f * llmax(fabsf(a), fabsf(b)); }
bool avatar(F32 flag) { return (static_cast<U32>(floorf(flag * 6.f + .5f)) & 1U) != 0; }

class Instrument final : public LLEventTimer
{
public:
    Instrument() : LLEventTimer(.5f) {}
    bool tick() override
    {
        if (active && !timeout_pending && heartbeat.getElapsedTimeF32() > 5.f)
        {
            timeout_pending = true;
            // Restore once outside the recurring timer callback. Suspend capture
            // until it runs; cancel() can still restore synchronously at shutdown.
            doOnIdleOneTime(boost::bind(&Instrument::onTimeout, this));
        }
        return false;
    }
    void onTimeout()
    {
        if (timeout_pending) stop("main display stopped rendering for five seconds");
    }
    LLTimer heartbeat;
    bool active = false;
    bool timeout_pending = false;
    S32 step = -1, settle = 0;
    LLSD saved;
    std::array<Frame, 4> frames;

    void restore()
    {
        ALKitHistory::ScopedSuspend history;
        for (auto it = saved.beginMap(); it != saved.endMap(); ++it)
            if (LLControlVariable* control = gSavedSettings.getControl(it->first)) control->setValue(it->second);
        saved = LLSD::emptyMap();
        active = false;
        timeout_pending = false;
        gSavedSettings.setU32(COMMAND, 0);
        for (auto& frame : frames) frame = Frame{};
    }
    void stop(const char* reason)
    {
        LL_INFOS("EnvOnly") << "ENV-ONLY INCONCLUSIVE: " << reason << LL_ENDL;
        restore();
    }
    void arm()
    {
        if (!gPipeline.worldHDREnabled()) { stop("needs HDR"); return; }
        if (gSavedSettings.getU32("CineLightRigActorMaskTest") != 0)
        { stop("another lighting diagnostic is armed"); return; }
        saved = LLSD::emptyMap();
        ALKitHistory::ScopedSuspend history;
        // Match the preset-owned surface factors without applying a preset or
        // taking ownership of the user's targeting/include-rig preferences.
        const char* keys[] = { SUN, "AlchemyEnvMoonEV", "AlchemyEnvSkyGIEV", "AlchemyEnvGIAmbientEV",
            "AlchemyEnvGIProbeDiffuseEV", "AlchemyEnvGIProbeSpecEV", "AlchemyEnvLocalLightEV",
            "AlchemyEnvLocalLightSize", "AlchemyEnvSunKelvin", "AlchemyEnvSunTintColor",
            "AlchemyEnvSunTintStrength", "AlchemyEnvMoonTintColor", "AlchemyEnvMoonTintStrength",
            "AlchemyEnvAmbientTintColor", "AlchemyEnvAmbientTintStrength", "AlchemyEnvShadowLiftEV",
            "AlchemyEnvMoonLinked", MODE };
        for (const char* key : keys)
        {
            LLControlVariable* control = gSavedSettings.getControl(key);
            saved[key] = control->get();
            control->resetToDefault(true);
        }
        active = true; step = 0; settle = 2; heartbeat.reset();
        LL_INFOS("EnvOnly") << "ENV-ONLY armed: freeze camera/actors/sky/probes; daytime, auto exposure off. "
            << "F0 identity OFF / F1 sun+3 OFF / F2 sun+3 ON / F1-prime OFF. "
            << "Forward-alpha verdict requires isolated hair over a lit wall; fog/water/wrap are compositing exceptions." << LL_ENDL;
    }
    void verify()
    {
        const Frame& a = frames[0]; const Frame& b = frames[1]; const Frame& c = frames[2]; const Frame& d = frames[3];
        for (const auto& frame : frames)
        {
            if (frame.width != a.width || frame.height != a.height || frame.depth != a.depth)
            { stop("view, coverage, depth or normal buffers changed; freeze the scene"); return; }
            if (frame.normals.size() != a.normals.size()) { stop("normal buffer size changed"); return; }
            for (size_t i = 0; i < frame.normals.size(); ++i)
                if (i % 4 != 3 && frame.normals[i] != a.normals[i])
                { stop("surface normal/material carrier changed; freeze the scene"); return; }
        }
        // A raw float read of an RGBA16F target is lossless. Compare bytes, not
        // hashes, and refuse to attribute moving/unstable frames to isolation.
        const bool off_bytes = b.post.size() == d.post.size() &&
            std::memcmp(b.post.data(), d.post.data(), b.post.size() * sizeof(F32)) == 0;
        if (!off_bytes) { stop("F1/F1-prime OFF bytes differ (motion/history/probes or regression)"); return; }
        U32 people = 0, set = 0, changed = 0, changed_people = 0, failures = 0, alpha_candidates = 0, alpha_failures = 0;
        const F32 known_alpha = gSavedSettings.getF32("AlchemyEnvEnvironmentOnlyTestAlpha");
        const bool alpha_fixture = known_alpha > 0.f && known_alpha < 1.f;
        for (U32 y = 2; y + 2 < a.height; ++y) for (U32 x = 2; x + 2 < a.width; ++x)
        {
            const size_t pixel = static_cast<size_t>(y) * a.width + x;
            if (a.depth[pixel] >= .999999f) continue;
            const bool subject = avatar(c.normals[pixel*4+3]);
            bool interior = true;
            for (S32 dy = -2; dy <= 2; ++dy) for (S32 dx = -2; dx <= 2; ++dx)
                if (avatar(c.normals[(static_cast<size_t>(y+dy)*a.width+x+dx)*4+3]) != subject) interior = false;
            if (!interior) continue;
            bool good = true, responds = false;
            for (U32 k = 0; k < 3; ++k)
            {
                const size_t p = pixel*4+k;
                good &= close(c.deferred[p], subject ? a.deferred[p] : b.deferred[p]);
                responds |= !close(a.deferred[p], b.deferred[p]);
            }
            failures += !good;
            if (subject) { ++people; changed_people += responds; } else { ++set; changed += responds; }
            // Alpha is evaluated independently of the opaque G-buffer tag.
            // Coverage must be supplied independently for a controlled constant
            // alpha card. Fitting transmission from F2 could certify relit hair
            // with a similarly coloured wall and is not a valid identity test.
            if (!subject && responds)
            {
                F32 denominator = 0.f, foreground_change = 0.f;
                for (U32 k = 0; k < 3; ++k)
                {
                    const size_t p = pixel*4+k;
                    F32 wall = b.deferred[p]-a.deferred[p];
                    denominator += wall*wall;
                    foreground_change += fabsf(b.post[p]-c.post[p]);
                }
                if (denominator > .001f && foreground_change > .01f)
                {
                    const F32 transmission = 1.f - known_alpha;
                    bool alpha_ok = alpha_fixture;
                    for (U32 k = 0; k < 3; ++k)
                    {
                        size_t p = pixel*4+k;
                        alpha_ok &= close(c.post[p], a.post[p]+transmission*(b.deferred[p]-a.deferred[p]));
                    }
                    ++alpha_candidates; alpha_failures += !alpha_ok;
                }
            }
        }
        LL_INFOS("EnvOnly") << "ENV-ONLY OFF-BYTES PASS (F1/F1-prime). ISOLATION "
            << ((!people || !set || !changed || !changed_people) ? "INCONCLUSIVE" : failures ? "FAIL" : "PASS")
            << " avatar=" << people << " responding-avatar=" << changed_people << " set=" << set << " responding-set=" << changed << " errors=" << failures
            << "; ALPHA-COMPOSITE " << ((!alpha_fixture || !alpha_candidates) ? "INCONCLUSIVE" : alpha_failures ? "FAIL" : "PASS")
            << " candidates=" << alpha_candidates << " errors=" << alpha_failures
            << " (requires independent TestAlpha coverage and isolated hair/wall fixture; otherwise INCONCLUSIVE)." << LL_ENDL;
        restore();
    }
};
Instrument& instrument() { static Instrument value; return value; }
}

namespace ALEnvOnlyDiagnostics
{
void captureDeferred(LLRenderTarget& color, LLRenderTarget& gbuffer)
{
    static LLCachedControl<U32> command(gSavedSettings, COMMAND, 0);
    Instrument& test = instrument();
    if (test.timeout_pending) return;
    if (test.active) test.heartbeat.reset();
    if (command() == 2) { if (test.active) test.stop("cancelled"); else gSavedSettings.setU32(COMMAND, 0); return; }
    if (command() == 1 && !test.active) { test.arm(); return; }
    if (!test.active) return;
    if (!gPipeline.worldHDREnabled()) { test.stop("HDR became unavailable"); return; }
    if (command() != 1) { test.stop("cancelled by setting"); return; }
    if (test.settle > 0) { --test.settle; return; }
    Frame& frame = test.frames[test.step];
    frame.width = color.getWidth(); frame.height = color.getHeight();
    if (!read(color, 0, frame.deferred) || !read(gbuffer, 2, frame.normals) || !read(gbuffer, 0, frame.depth, true))
        test.stop("GL deferred readback failed");
}
void endView(LLRenderTarget& color)
{
    Instrument& test = instrument();
    if (!test.active || test.timeout_pending || test.settle > 0 || test.frames[test.step].deferred.empty()) return;
    if (!read(color, 0, test.frames[test.step].post)) { test.stop("GL forward readback failed"); return; }
    if (test.step == 3) { test.verify(); return; }
    ALKitHistory::ScopedSuspend history;
    ++test.step; test.settle = 2;
    gSavedSettings.setF32(SUN, 3.f);
    gSavedSettings.setBOOL(MODE, test.step == 2);
}
void cancel()
{
    Instrument& test = instrument();
    if (test.active) test.stop("settings persistence or viewer shutdown");
}
}
