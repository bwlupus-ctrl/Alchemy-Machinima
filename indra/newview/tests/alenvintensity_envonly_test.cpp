/** Standalone production predicate and real settings-signal tests, no viewer build.
 * env_only_check.py --cpu compiles the complete settings_setup_listeners body,
 * both production signal helpers and handleSetShaderChanged with real llcontrol.
 */
#ifdef AL_ENV_ONLY_STANDALONE
#include "env_only_adapter.h"
#include <cassert>
#include <iostream>
int main()
{
    for (bool enabled : {false, true}) for (bool hdr : {false, true})
    for (bool preview : {false, true}) for (bool bake : {false, true})
    for (float version : {4.0f, 4.05f, 4.6f})
    {
        gSavedSettings.enabled = enabled;
        gSavedSettings.hdr = hdr;
        neutral = preview; capture = bake; gGLManager.mGLVersion = version;
        assert(ALEnvIntensity::envOnlyActive() == (enabled && hdr && version > 4.05f && !preview && !bake));
    }
    // Cached controls must track toggles rather than latch the startup value.
    neutral = capture = false; gSavedSettings.hdr = true; gGLManager.mGLVersion = 4.6f;
    for (bool enabled : {false, true, false, true, false})
    {
        gSavedSettings.enabled = enabled;
        assert(ALEnvIntensity::envOnlyActive() == enabled);
    }
    std::cout << "PASS envOnlyActive: 48 HDR/capture/preview/GL cases and runtime toggles\n";
}
#elif defined(AL_ENV_ONLY_SIGNAL_STANDALONE)
#include "env_only_signal_adapter.h"
#include "llcommon.h"
#include "llerrorcontrol.h"
#include <cstdlib>
int main()
{
    _set_error_mode(_OUT_TO_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    LLCommon::initClass();
    LLError::setDefaultLevel(LLError::LEVEL_ERROR);
    std::cerr << "Loading production settings\n";
    // Real control declarations use types/defaults read from production XML.
    declare_test_settings();
    std::cerr << "Registering production listeners\n";
    settings_setup_listeners();
    std::cerr << "Firing positive control\n";
    // Positive control proves that registration and the rebuild instruments work.
    LLControlVariable* reload = gSavedSettings.getControl("RenderGlowNoise");
    const auto before = settings_shader_change_count();
    reload->setValue(!reload->get().asBoolean());
    assert(settings_shader_change_count() == before + 1);
    assert(LLViewerShaderMgr::calls == 1);
    for (const char* key : {"AlchemyEnvEnvironmentOnly", "RenderGBufferAvatarTag"})
    {
        LLControlVariable* control = gSavedSettings.getControl(key);
        int commits = 0;
        auto connection = control->getSignal()->connect(
            [&commits](LLControlVariable*, const LLSD&, const LLSD&) { ++commits; });
        const auto handlers = settings_shader_change_count();
        const auto shaders = LLViewerShaderMgr::calls;
        for (bool value : {false, true, false, true, false})
        {
            gSavedSettings.setBOOL(key, value);
            assert(settings_shader_change_count() == handlers);
            assert(LLViewerShaderMgr::calls == shaders);
        }
        assert(commits >= 4);
        connection.disconnect();
    }
    std::cout << "PASS real settings signals: both preferences emit commits without shader reloads\n";
}
#endif
