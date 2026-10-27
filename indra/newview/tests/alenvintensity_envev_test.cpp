// Standalone TUT runner uses extracted production compute/local/SSAO functions.
// python scripts/code_tools/env_ev_math_check.py [--source-ref HEAD]
#ifdef AL_ENV_EV_STANDALONE
#include "env_ev_adapter.h"
#include <tut/tut.hpp>
#include <tut/tut_reporter.hpp>
#include <iostream>

namespace
{
void reset()
{
    gSavedSettings.values.clear();
    gSavedSettings.setBOOL("AlchemyEnvEnvironmentOnly", true);
    gGLManager.mGLVersion = 4.6f;
    neutral = capture = LLRender::sClassicMode = false;
}
void baseEVs(float value)
{
    for (const char* key : {"AlchemyEnvSunEV", "AlchemyEnvMoonEV", "AlchemyEnvSkyGIEV",
         "AlchemyEnvGIProbeSpecEV", "AlchemyEnvLocalLightEV"}) gSavedSettings.setF32(key, value);
}
}
namespace tut
{
struct env_ev_data {};
using env_ev_group = test_group<env_ev_data>;
using env_ev_object = env_ev_group::object;
env_ev_group env_ev_tests("Environment EV production factors");
template<> template<> void env_ev_object::test<1>()
{
    for (float offset : {-4.f, -1.25f, 0.f, .05f, 2.f, 4.f})
    for (float base : {-6.f, -2.f, 0.f, 3.f, 6.f})
    for (bool sun : {false, true})
    {
        reset(); baseEVs(base);
        gSavedSettings.setBOOL("AlchemyEnvMoonLinked", false);
        gSavedSettings.setF32("AlchemyEnvEnvironmentEV", offset);
        gSavedSettings.setF32("AlchemyEnvGIAmbientEV", .5f);
        gSavedSettings.setF32("AlchemyEnvGIProbeDiffuseEV", -.25f);
        const auto f = ALEnvIntensity::compute(false, false, false, sun);
        auto expected = [](float stops) { return exp2f(std::clamp(stops, -6.f, 6.f)); };
        ensure_equals("sun/moon summed before clamp", f.mSunMul, expected(base + offset));
        ensure_equals("ambient summed before clamp", f.mAmbMul, expected(base + .5f + offset));
        ensure_equals("diffuse summed before clamp", f.mDiffMul, expected(base - .25f + offset));
        ensure_equals("reflections summed before clamp", f.mSpecMul, expected(base + offset));
        ensure_equals("local summed before clamp", f.mLocalMul, expected(base + offset));
        ensure_equals("local emitter matches factors", ALEnvIntensity::localLightEVScale(nullptr), f.mLocalMul);
        ensure_equals("SSAO follows diffuse", ALEnvIntensity::ssaoCeilingScale(), f.mDiffMul);
        ensure_equals("lift unchanged", f.mLiftGain, 0.f);
        for (int i = 0; i < 3; ++i) {
            ensure_equals("sun tint unchanged", f.mSunTint.mV[i], 1.f);
            ensure_equals("ambient tint unchanged", f.mAmbTint.mV[i], 1.f);
        }
        // Production forward sets and deferred shaders invert the combined EV.
        const float local = ALEnvIntensity::localLightEVScale(nullptr);
        ensure_equals("avatar local inversion identity", (0.75f * local) / local, .75f);
    }
}
template<> template<> void env_ev_object::test<2>()
{
    for (int inactive = 0; inactive < 4; ++inactive)
    for (float offset : {-4.f, 0.f, 4.f})
    {
        reset(); baseEVs(.5f);
        if (inactive == 0) gSavedSettings.setBOOL("AlchemyEnvEnvironmentOnly", false);
        if (inactive == 1) gSavedSettings.setBOOL("RenderHDREnabled", false);
        if (inactive == 2) gGLManager.mGLVersion = 4.05f;
        if (inactive == 3) neutral = true;
        gSavedSettings.setF32("AlchemyEnvEnvironmentEV", offset);
        const auto f = ALEnvIntensity::compute(false, false, false, true);
        const float expected = neutral ? 1.f : exp2f(.5f);
        for (float actual : {f.mSunMul, f.mAmbMul, f.mDiffMul, f.mSpecMul, f.mLocalMul,
             ALEnvIntensity::localLightEVScale(nullptr), ALEnvIntensity::ssaoCeilingScale()})
            ensure_equals("inactive exact baseline", actual, expected);
    }
}
template<> template<> void env_ev_object::test<3>()
{
    reset(); baseEVs(.5f); capture = true;
    gSavedSettings.setF32("AlchemyEnvEnvironmentEV", 1.f);
    ensure("capture never enables avatar routing", !ALEnvIntensity::envOnlyActive());
    const auto f = ALEnvIntensity::compute(false, false, true, true);
    ensure_equals("capture sun source", f.mSunMul, exp2f(1.5f));
    ensure_equals("capture local source", f.mLocalMul, exp2f(1.5f));
    ensure_equals("capture local emitter", ALEnvIntensity::localLightEVScale(nullptr), exp2f(1.5f));
    for (float actual : {f.mAmbMul, f.mDiffMul, f.mSpecMul}) ensure_equals("capture sampling identity", actual, 1.f);
    LLViewerObject rig; rig.rig = true;
    ensure_equals("rig exclusion preserved", ALEnvIntensity::localLightEVScale(&rig), 1.f);
    gSavedSettings.setBOOL("AlchemyEnvLocalLightIncludeRig", true);
    ensure_equals("included rig combined EV", ALEnvIntensity::localLightEVScale(&rig), f.mLocalMul);
    const auto classic = ALEnvIntensity::compute(true, false, false, true);
    ensure_equals("classic sky identity", classic.mSunMul, 1.f);
    const auto legacy = ALEnvIntensity::compute(false, true, false, true);
    ensure_equals("legacy sun cap", legacy.mSunMul, 1.f);
}
template<> template<> void env_ev_object::test<4>()
{
    for (bool enabled : {false, true}) for (bool hdr : {false, true})
    for (bool bake : {false, true}) for (float offset : {-4.f, 0.f, 1.25f, 4.f})
    {
        reset(); baseEVs(.5f); capture = bake;
        gSavedSettings.setBOOL("AlchemyEnvEnvironmentOnly", enabled);
        gSavedSettings.setBOOL("RenderHDREnabled", hdr);
        gSavedSettings.setF32("AlchemyEnvEnvironmentEV", offset);
        const auto st = captureSignature();
        const float expected = .5f + (enabled && hdr ? offset : 0.f);
        ensure_equals("sun-only probe cache invalidation", st.mSunEV, expected);
        ensure_equals("moon probe cache invalidation", st.mMoonEV, expected);
        ensure_equals("local probe cache invalidation", st.mLocalLightEV, expected);
    }
}
}

int main(int argc, char** argv)
{
    if (argc == 2 && std::string(argv[1]) == "--baseline")
    {
        std::cout << std::hexfloat;
        for (int gate = 0; gate < 5; ++gate)
        for (float offset : {-4.f, 0.f, 4.f})
        for (float base : {-6.f, -1.25f, 0.f, .05f, 5.5f})
        for (int flags = 0; flags < 32; ++flags)
        {
            reset(); baseEVs(base);
            if (gate == 0) gSavedSettings.setBOOL("AlchemyEnvEnvironmentOnly", false);
            if (gate == 1) offset = 0.f;
            if (gate == 2) gSavedSettings.setBOOL("RenderHDREnabled", false);
            if (gate == 3) gGLManager.mGLVersion = 4.05f;
            if (gate == 4) neutral = true;
            capture = (flags & 1) != 0;
            LLRender::sClassicMode = (flags & 2) != 0;
            gSavedSettings.setBOOL("AlchemyEnvMoonLinked", (flags & 4) != 0);
            gSavedSettings.setF32("AlchemyEnvMoonEV", base + .5f);
            gSavedSettings.setF32("AlchemyEnvGIAmbientEV", .75f);
            gSavedSettings.setF32("AlchemyEnvGIProbeDiffuseEV", -.25f);
            gSavedSettings.setF32("AlchemyEnvShadowLiftEV", 1.f);
            gSavedSettings.setF32("AlchemyEnvEnvironmentEV", offset);
            const auto f = ALEnvIntensity::compute(LLRender::sClassicMode, (flags & 8) != 0, capture, (flags & 16) != 0);
            const auto signature = captureSignature();
            for (float value : {f.mSunMul, f.mAmbMul, f.mDiffMul, f.mSpecMul, f.mLocalMul, f.mLiftGain,
                 ALEnvIntensity::localLightEVScale(nullptr), ALEnvIntensity::ssaoCeilingScale(),
                 f.mSunTint.mV[0], f.mSunTint.mV[1], f.mSunTint.mV[2],
                 f.mAmbTint.mV[0], f.mAmbTint.mV[1], f.mAmbTint.mV[2],
                 signature.mSunEV, signature.mMoonEV, signature.mLocalLightEV}) std::cout << value << ',';
            std::cout << '\n';
        }
        return 0;
    }
    if (argc == 2 && std::string(argv[1]) == "--dump")
    {
        std::cout << '[';
        bool first = true;
        for (bool enabled : {false, true}) for (float offset : {-4.f, 0.f, 1.25f, 4.f})
        {
            reset(); baseEVs(.5f);
            gSavedSettings.setBOOL("AlchemyEnvEnvironmentOnly", enabled);
            gSavedSettings.setF32("AlchemyEnvEnvironmentEV", offset);
            const auto f = ALEnvIntensity::compute(false, false, false, true);
            std::cout.precision(9);
            std::cout << (first ? "" : ",") << "{\"enabled\":" << enabled << ",\"ev\":" << offset
                << ",\"sun\":" << f.mSunMul << ",\"ambient\":" << f.mAmbMul
                << ",\"diffuse\":" << f.mDiffMul << ",\"reflection\":" << f.mSpecMul
                << ",\"local\":" << ALEnvIntensity::localLightEVScale(nullptr)
                << ",\"ssao\":" << ALEnvIntensity::ssaoCeilingScale() << '}';
            first = false;
        }
        std::cout << "]\n";
        return 0;
    }
    tut::reporter reporter;
    tut::runner.get().set_callback(&reporter);
    tut::runner.get().run_tests();
    return reporter.all_ok() ? 0 : 1;
}
#endif
