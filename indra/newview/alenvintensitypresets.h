/**
 * @file alenvintensitypresets.h
 * @brief [EnvIntensity userpresets] Save / Load / Delete presets for the Personal
 *        Lighting "Light Intensity" strip.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 *
 * A user preset is a named snapshot of exactly the 16 AlchemyEnv* viewer
 * settings the built-in Quick Presets write (Sun / Sky GI EV, Moon link + EV,
 * GI ambient / probe diffuse / probe spec EV, Local lights EV, Sun Kelvin,
 * the three tint swatch + strength pairs, Shadow lift EV). It never touches
 * AlchemyEnvIntensityAdvanced, AlchemyEnvLocalLightIncludeRig or auto
 * exposure.
 *
 * Storage: LLSD XML "light_intensity_presets.xml" in the user settings
 * directory, root map: preset name -> { version, <setting name> = value ... }.
 * The file is treated as untrusted on load (version / type / range / finite
 * checks, 200 presets, 64-character names) and written atomically
 * (temp file -> rename). All functions are main-thread only.
 */

#pragma once

#ifndef AL_ENVINTENSITYPRESETS_H
#define AL_ENVINTENSITYPRESETS_H

#include "v4color.h"

#include <boost/signals2.hpp>

#include <cstddef>
#include <string>
#include <vector>

namespace ALEnvIntensityPresets
{
    // The 16 values a preset owns. The member initialisers are the stock
    // (settings_alchemy.xml default) values; defaultValues() re-reads the live
    // defaults from the settings so they cannot drift from the XML.
    struct Values
    {
        F32      mSunEV              = 0.f;
        F32      mSkyGIEV            = 0.f;
        bool     mMoonLinked         = true;
        F32      mMoonEV             = 0.f;
        F32      mGIAmbientEV        = 0.f;
        F32      mGIProbeDiffuseEV   = 0.f;
        F32      mGIProbeSpecEV      = 0.f;
        F32      mLocalLightEV       = 0.f;
        F32      mSunKelvin          = 6500.f;
        LLColor4 mSunTintColor       = LLColor4(1.f, 1.f, 1.f, 1.f);
        F32      mSunTintStrength    = 1.f;
        LLColor4 mMoonTintColor      = LLColor4(1.f, 1.f, 1.f, 1.f);
        F32      mMoonTintStrength   = 1.f;
        LLColor4 mAmbientTintColor   = LLColor4(1.f, 1.f, 1.f, 1.f);
        F32      mAmbientTintStrength = 1.f;
        F32      mShadowLiftEV       = 0.f;
    };

    // ---- live settings ----------------------------------------------------
    Values currentValues();                                  // read the 16 live settings
    Values defaultValues();                                  // each setting's default
    bool   nearlyEqual(const Values& a, const Values& b);   // epsilon compare (F32 LLSD round-trip)

    // Writes the 16 settings (in the order the built-in presets always used).
    // While it runs isApplying() is true, so the Quick Presets combo does not
    // flash "Custom" from the per-setting change signals; when it finishes the
    // "applied" signal fires once so listeners re-derive their state.
    void   apply(const Values& values);
    bool   isApplying();

    // ---- names --------------------------------------------------------------
    enum NameCheck
    {
        NAME_OK,        // out_clean is the trimmed, valid name
        NAME_INVALID,   // empty, > 64 characters, or contains a control character
        NAME_RESERVED   // collides (case-insensitive) with a built-in Quick Preset name
    };
    const size_t MAX_NAME_CHARS = 64;
    const size_t MAX_PRESETS    = 200;

    NameCheck checkName(const std::string& raw, std::string& out_clean);

    // ---- user presets ---------------------------------------------------------
    std::vector<std::string> listNames();                    // sorted, case-insensitive
    bool exists(const std::string& name);                    // case-insensitive
    bool getValues(const std::string& name, Values& out_values, std::string* out_canonical_name = nullptr);
    // Preset whose values match `values` (epsilon), or an empty string.
    std::string findMatchingName(const Values& values);

    enum SaveResult
    {
        SAVE_OK,
        SAVE_INVALID_NAME,
        SAVE_RESERVED_NAME,
        SAVE_LIMIT_REACHED,   // MAX_PRESETS and `name` is new
        SAVE_WRITE_FAILED     // file could not be written; the in-memory list is rolled back
    };
    // Stores (or overwrites, case-insensitively) `values` under `raw_name`.
    // Values are clamped to each setting's valid range (non-finite -> default) so
    // what is saved always reloads.
    SaveResult save(const std::string& raw_name, const Values& values);
    SaveResult saveCurrent(const std::string& raw_name);     // save(raw_name, currentValues())
    bool       remove(const std::string& name);              // false = missing or write failed (rolled back)
    bool       applyNamed(const std::string& name);          // false = no such preset

    // ---- signals ----------------------------------------------------------------
    typedef boost::signals2::signal<void()> signal_t;
    // the list of user presets changed (save / overwrite / delete)
    boost::signals2::connection connectListChanged(const signal_t::slot_type& slot);
    // apply() finished writing the 16 settings
    boost::signals2::connection connectApplied(const signal_t::slot_type& slot);
}

#endif // AL_ENVINTENSITYPRESETS_H
