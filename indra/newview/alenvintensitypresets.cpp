/**
 * @file alenvintensitypresets.cpp
 * @brief [EnvIntensity userpresets] Save / Load / Delete presets for the Personal
 *        Lighting "Light Intensity" strip. See alenvintensitypresets.h.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "alenvintensitypresets.h"

#include "lldir.h"
#include "llfile.h"
#include "llsd.h"
#include "llsdserialize.h"
#include "llstring.h"
#include "lluuid.h"
#include "llviewercontrol.h"

#include <algorithm>
#include <cmath>
#include <map>

namespace ALEnvIntensityPresets
{

namespace
{
    const char* const PRESETS_FILENAME = "light_intensity_presets.xml";
    const S32         PRESET_VERSION   = 1;
    // A presets file bigger than this is not ours (200 presets are ~0.5 MB at most).
    const S64         MAX_FILE_BYTES   = static_cast<S64>(4) * 1024 * 1024;
    const F32         PRESET_EPS(1.e-4f);

    const char* const KEY_MOON_LINKED = "AlchemyEnvMoonLinked";

    // ---- the 16 owned settings -------------------------------------------------
    // Ranges are the Light Intensity slider ranges (floater_adjust_environment.xml),
    // i.e. what the UI can produce and the renderer's own clamps accept.
    struct FloatField
    {
        const char* mKey;
        F32 Values::* mMember;
        F64         mMin;
        F64         mMax;
    };
    const FloatField FLOAT_FIELDS[] =
    {
        { "AlchemyEnvSunEV",              &Values::mSunEV,               -4.0,     6.0 },
        { "AlchemyEnvSkyGIEV",            &Values::mSkyGIEV,             -4.0,     4.0 },
        { "AlchemyEnvMoonEV",             &Values::mMoonEV,              -4.0,     6.0 },
        { "AlchemyEnvGIAmbientEV",        &Values::mGIAmbientEV,         -3.0,     3.0 },
        { "AlchemyEnvGIProbeDiffuseEV",   &Values::mGIProbeDiffuseEV,    -3.0,     3.0 },
        { "AlchemyEnvGIProbeSpecEV",      &Values::mGIProbeSpecEV,       -3.0,     3.0 },
        { "AlchemyEnvLocalLightEV",       &Values::mLocalLightEV,        -4.0,     4.0 },
        { "AlchemyEnvSunKelvin",          &Values::mSunKelvin,         2000.0, 12000.0 },
        { "AlchemyEnvSunTintStrength",    &Values::mSunTintStrength,      0.0,     1.0 },
        { "AlchemyEnvMoonTintStrength",   &Values::mMoonTintStrength,     0.0,     1.0 },
        { "AlchemyEnvAmbientTintStrength",&Values::mAmbientTintStrength,  0.0,     1.0 },
        { "AlchemyEnvShadowLiftEV",       &Values::mShadowLiftEV,         0.0,     2.0 },
    };

    struct ColorField
    {
        const char*    mKey;
        LLColor4 Values::* mMember;
    };
    const ColorField COLOR_FIELDS[] =
    {
        { "AlchemyEnvSunTintColor",     &Values::mSunTintColor },
        { "AlchemyEnvMoonTintColor",    &Values::mMoonTintColor },
        { "AlchemyEnvAmbientTintColor", &Values::mAmbientTintColor },
    };

    F32 defaultF32(const char* key, F32 fallback)
    {
        LLControlVariablePtr ctrl = gSavedSettings.getControl(key);
        return ctrl ? static_cast<F32>(ctrl->getDefault().asReal()) : fallback;
    }

    // ---- name helpers -------------------------------------------------------------
    std::string lowerCopy(const std::string& s)
    {
        std::string out(s);
        LLStringUtil::toLower(out);
        return out;
    }

    // lower-case, '_' and '-' -> ' ' so "golden_hour", "Golden Hour" and
    // "High-Key Studio" / "high_key_studio" all normalise alike
    std::string normalizeName(const std::string& s)
    {
        std::string out = lowerCopy(s);
        for (char& c : out)
        {
            if (c == '_' || c == '-')
            {
                c = ' ';
            }
        }
        return out;
    }

    // Built-in Quick Preset labels (floater_adjust_environment.xml combo
    // env_intensity_preset) + the combo's own "Custom" and section header. The
    // keys ("golden_hour") normalise onto the same strings. A new built-in preset
    // must be added here too.
    bool isReservedName(const std::string& name)
    {
        static const char* const RESERVED[] =
        {
            "Stock", "Golden Hour", "Blue Hour", "Overcast Soft", "Noon Punch",
            "High-Key Studio", "Low-Key Noir", "Moonlit Night", "Neon Night",
            "Candlelit Interior", "Desert Heat", "Arctic Cold", "Soft Fill",
            "Reflections Pop", "Custom", "My presets",
        };
        const std::string norm = normalizeName(name);
        for (const char* r : RESERVED)
        {
            if (norm == normalizeName(r))
            {
                return true;
            }
        }
        return false;
    }

    // ---- store ------------------------------------------------------------------
    struct Preset
    {
        std::string mName;      // display name (as saved)
        Values      mValues;
    };
    typedef std::map<std::string, Preset> store_t;   // key = lower-case name -> sorted case-insensitively

    store_t& store()
    {
        static store_t sStore;
        return sStore;
    }

    bool sLoaded   = false;
    bool sApplying = false;

    signal_t& listChangedSignal()
    {
        static signal_t sSignal;
        return sSignal;
    }
    signal_t& appliedSignal()
    {
        static signal_t sSignal;
        return sSignal;
    }

    std::string presetsPath()
    {
        return gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, PRESETS_FILENAME);
    }

    // Clamp every value into its valid range; non-finite -> the setting default.
    // Used on save so that whatever is written always passes the loader.
    void sanitize(Values& v)
    {
        const Values defaults = defaultValues();
        for (const FloatField& f : FLOAT_FIELDS)
        {
            const F32 cur = v.*(f.mMember);
            if (!std::isfinite(cur))
            {
                v.*(f.mMember) = defaults.*(f.mMember);
            }
            else
            {
                v.*(f.mMember) = static_cast<F32>(llclamp(static_cast<F64>(cur), f.mMin, f.mMax));
            }
        }
        for (const ColorField& f : COLOR_FIELDS)
        {
            LLColor4& c = v.*(f.mMember);
            for (S32 i = 0; i < 4; ++i)
            {
                c.mV[i] = std::isfinite(c.mV[i]) ? llclamp(c.mV[i], 0.f, 1.f) : 1.f;
            }
        }
    }

    LLSD toLLSD(const Values& v)
    {
        LLSD e = LLSD::emptyMap();
        e["version"] = PRESET_VERSION;
        for (const FloatField& f : FLOAT_FIELDS)
        {
            e[f.mKey] = static_cast<F64>(v.*(f.mMember));
        }
        e[KEY_MOON_LINKED] = v.mMoonLinked;
        for (const ColorField& f : COLOR_FIELDS)
        {
            const LLColor4& c = v.*(f.mMember);
            LLSD arr = LLSD::emptyArray();
            for (S32 i = 0; i < 4; ++i)
            {
                arr.append(LLSD(static_cast<F64>(c.mV[i])));
            }
            e[f.mKey] = arr;
        }
        return e;
    }

    // Untrusted input. false = reject the whole preset (bad version / wrong type /
    // non-finite); out-of-range numbers are clamped; a missing key keeps its default.
    bool fromLLSD(const LLSD& entry, Values& out)
    {
        if (!entry.isMap())
        {
            return false;
        }
        const LLSD& version = entry["version"];
        if (!version.isInteger() || version.asInteger() != PRESET_VERSION)
        {
            return false;
        }

        Values v = defaultValues();
        for (const FloatField& f : FLOAT_FIELDS)
        {
            if (!entry.has(f.mKey))
            {
                continue;
            }
            const LLSD& s = entry[f.mKey];
            if (!s.isReal() && !s.isInteger())
            {
                return false;
            }
            F64 x = s.asReal();
            if (!std::isfinite(x))
            {
                return false;
            }
            x = llclamp(x, f.mMin, f.mMax);
            v.*(f.mMember) = static_cast<F32>(x);
        }
        if (entry.has(KEY_MOON_LINKED))
        {
            const LLSD& s = entry[KEY_MOON_LINKED];
            if (!s.isBoolean())
            {
                return false;
            }
            v.mMoonLinked = s.asBoolean();
        }
        for (const ColorField& f : COLOR_FIELDS)
        {
            if (!entry.has(f.mKey))
            {
                continue;
            }
            const LLSD& s = entry[f.mKey];
            if (!s.isArray() || s.size() != 4)
            {
                return false;
            }
            LLColor4 c;
            for (S32 i = 0; i < 4; ++i)
            {
                const LLSD& ch = s[i];
                if (!ch.isReal() && !ch.isInteger())
                {
                    return false;
                }
                const F64 x = ch.asReal();
                if (!std::isfinite(x))
                {
                    return false;
                }
                c.mV[i] = static_cast<F32>(llclamp(x, 0.0, 1.0));
            }
            v.*(f.mMember) = c;
        }
        out = v;
        return true;
    }

    // Crash-safe write: temp -> flush -> close -> rename, all checked; a failure
    // removes the temp file and leaves the existing file untouched.
    bool writeAtomic(const std::string& path, const LLSD& content)
    {
        const std::string temporary = path + "." + LLUUID::generateNewID().asString() + ".tmp";
        llofstream output(temporary.c_str());
        if (!output.is_open())
        {
            return false;
        }
        const S32 serialized = LLSDSerialize::toPrettyXML(content, output);
        output.flush();
        const bool succeeded = serialized > 0 && output.good();
        output.close();
        if (!succeeded || output.fail() || LLFile::rename(temporary, path) != 0)
        {
            LLFile::remove(temporary);
            return false;
        }
        return true;
    }

    bool writeStore()
    {
        LLSD root = LLSD::emptyMap();
        for (const auto& kv : store())
        {
            root[kv.second.mName] = toLLSD(kv.second.mValues);
        }
        return writeAtomic(presetsPath(), root);
    }

    void loadFromDisk()
    {
        sLoaded = true;
        store().clear();

        const std::string path = presetsPath();
        if (!LLFile::isfile(path))
        {
            return;   // first run: nothing saved yet
        }
        const S64 bytes = LLFile::size(path);
        if (bytes < 0 || bytes > MAX_FILE_BYTES)
        {
            LL_WARNS("EnvIntensity") << "[EnvIntensity userpresets] ignoring \"" << path
                << "\" (" << bytes << " bytes)" << LL_ENDL;
            return;
        }
        llifstream input(path.c_str());
        if (!input.is_open())
        {
            return;
        }
        LLSD root;
        const S32 ret = LLSDSerialize::fromXML(root, input);
        input.close();
        if (ret == LLSDParser::PARSE_FAILURE || !root.isMap())
        {
            LL_WARNS("EnvIntensity") << "[EnvIntensity userpresets] ignoring unreadable \"" << path << "\"" << LL_ENDL;
            return;
        }

        S32 rejected = 0;
        for (LLSD::map_const_iterator it = root.beginMap(); it != root.endMap(); ++it)
        {
            std::string clean;
            if (checkName(it->first, clean) != NAME_OK || clean != it->first)
            {
                ++rejected;
                continue;
            }
            const std::string key = lowerCopy(clean);
            if (store().size() >= MAX_PRESETS || store().find(key) != store().end())
            {
                ++rejected;
                continue;
            }
            Preset p;
            p.mName = clean;
            if (!fromLLSD(it->second, p.mValues))
            {
                ++rejected;
                continue;
            }
            store()[key] = p;
        }
        if (rejected > 0)
        {
            LL_WARNS("EnvIntensity") << "[EnvIntensity userpresets] skipped " << rejected
                << " invalid preset(s) in \"" << path << "\"" << LL_ENDL;
        }
    }

    void ensureLoaded()
    {
        if (!sLoaded)
        {
            loadFromDisk();
        }
    }

    struct ApplyGuard
    {
        ApplyGuard() : mPrev(sApplying) { sApplying = true; }
        ~ApplyGuard() { sApplying = mPrev; }
        bool mPrev;
    };

    inline bool nearlyEqualF(F32 a, F32 b)
    {
        return std::fabs(a - b) < PRESET_EPS;
    }
    inline bool nearlyEqualColor(const LLColor4& a, const LLColor4& b)
    {
        return nearlyEqualF(a.mV[0], b.mV[0]) && nearlyEqualF(a.mV[1], b.mV[1])
            && nearlyEqualF(a.mV[2], b.mV[2]) && nearlyEqualF(a.mV[3], b.mV[3]);
    }
}   // anonymous namespace

//-------------------------------------------------------------------------
Values currentValues()
{
    Values v;
    v.mSunEV               = gSavedSettings.getF32("AlchemyEnvSunEV");
    v.mSkyGIEV             = gSavedSettings.getF32("AlchemyEnvSkyGIEV");
    v.mMoonLinked          = gSavedSettings.getBOOL("AlchemyEnvMoonLinked");
    v.mMoonEV              = gSavedSettings.getF32("AlchemyEnvMoonEV");
    v.mGIAmbientEV         = gSavedSettings.getF32("AlchemyEnvGIAmbientEV");
    v.mGIProbeDiffuseEV    = gSavedSettings.getF32("AlchemyEnvGIProbeDiffuseEV");
    v.mGIProbeSpecEV       = gSavedSettings.getF32("AlchemyEnvGIProbeSpecEV");
    v.mLocalLightEV        = gSavedSettings.getF32("AlchemyEnvLocalLightEV");
    v.mSunKelvin           = gSavedSettings.getF32("AlchemyEnvSunKelvin");
    v.mSunTintColor        = gSavedSettings.getColor4("AlchemyEnvSunTintColor");
    v.mSunTintStrength     = gSavedSettings.getF32("AlchemyEnvSunTintStrength");
    v.mMoonTintColor       = gSavedSettings.getColor4("AlchemyEnvMoonTintColor");
    v.mMoonTintStrength    = gSavedSettings.getF32("AlchemyEnvMoonTintStrength");
    v.mAmbientTintColor    = gSavedSettings.getColor4("AlchemyEnvAmbientTintColor");
    v.mAmbientTintStrength = gSavedSettings.getF32("AlchemyEnvAmbientTintStrength");
    v.mShadowLiftEV        = gSavedSettings.getF32("AlchemyEnvShadowLiftEV");
    return v;
}

Values defaultValues()
{
    Values v;   // member initialisers = the shipped defaults; overridden from the live defaults
    for (const FloatField& f : FLOAT_FIELDS)
    {
        v.*(f.mMember) = defaultF32(f.mKey, v.*(f.mMember));
    }
    if (LLControlVariablePtr ctrl = gSavedSettings.getControl(KEY_MOON_LINKED))
    {
        v.mMoonLinked = ctrl->getDefault().asBoolean();
    }
    for (const ColorField& f : COLOR_FIELDS)
    {
        if (LLControlVariablePtr ctrl = gSavedSettings.getControl(f.mKey))
        {
            const LLSD def = ctrl->getDefault();
            if (def.isArray() && def.size() == 4)
            {
                v.*(f.mMember) = LLColor4(def);
            }
        }
    }
    return v;
}

bool nearlyEqual(const Values& a, const Values& b)
{
    return nearlyEqualF(a.mSunEV, b.mSunEV)
        && nearlyEqualF(a.mSkyGIEV, b.mSkyGIEV)
        && a.mMoonLinked == b.mMoonLinked
        && nearlyEqualF(a.mMoonEV, b.mMoonEV)
        && nearlyEqualF(a.mGIAmbientEV, b.mGIAmbientEV)
        && nearlyEqualF(a.mGIProbeDiffuseEV, b.mGIProbeDiffuseEV)
        && nearlyEqualF(a.mGIProbeSpecEV, b.mGIProbeSpecEV)
        && nearlyEqualF(a.mLocalLightEV, b.mLocalLightEV)
        && nearlyEqualF(a.mSunKelvin, b.mSunKelvin)
        && nearlyEqualColor(a.mSunTintColor, b.mSunTintColor)
        && nearlyEqualF(a.mSunTintStrength, b.mSunTintStrength)
        && nearlyEqualColor(a.mMoonTintColor, b.mMoonTintColor)
        && nearlyEqualF(a.mMoonTintStrength, b.mMoonTintStrength)
        && nearlyEqualColor(a.mAmbientTintColor, b.mAmbientTintColor)
        && nearlyEqualF(a.mAmbientTintStrength, b.mAmbientTintStrength)
        && nearlyEqualF(a.mShadowLiftEV, b.mShadowLiftEV);
}

void apply(const Values& values)
{
    {
        ApplyGuard guard;   // suppresses the Quick Presets combo refresh while the 16 signals fire
        gSavedSettings.setF32("AlchemyEnvSunEV", values.mSunEV);
        gSavedSettings.setF32("AlchemyEnvSkyGIEV", values.mSkyGIEV);
        gSavedSettings.setBOOL("AlchemyEnvMoonLinked", values.mMoonLinked);
        gSavedSettings.setF32("AlchemyEnvMoonEV", values.mMoonEV);
        gSavedSettings.setF32("AlchemyEnvGIAmbientEV", values.mGIAmbientEV);
        gSavedSettings.setF32("AlchemyEnvGIProbeDiffuseEV", values.mGIProbeDiffuseEV);
        gSavedSettings.setF32("AlchemyEnvGIProbeSpecEV", values.mGIProbeSpecEV);
        gSavedSettings.setF32("AlchemyEnvLocalLightEV", values.mLocalLightEV);
        gSavedSettings.setF32("AlchemyEnvSunKelvin", values.mSunKelvin);
        gSavedSettings.setColor4("AlchemyEnvSunTintColor", values.mSunTintColor);
        gSavedSettings.setF32("AlchemyEnvSunTintStrength", values.mSunTintStrength);
        gSavedSettings.setColor4("AlchemyEnvMoonTintColor", values.mMoonTintColor);
        gSavedSettings.setF32("AlchemyEnvMoonTintStrength", values.mMoonTintStrength);
        gSavedSettings.setColor4("AlchemyEnvAmbientTintColor", values.mAmbientTintColor);
        gSavedSettings.setF32("AlchemyEnvAmbientTintStrength", values.mAmbientTintStrength);
        gSavedSettings.setF32("AlchemyEnvShadowLiftEV", values.mShadowLiftEV);
    }
    appliedSignal()();
}

bool isApplying()
{
    return sApplying;
}

//-------------------------------------------------------------------------
NameCheck checkName(const std::string& raw, std::string& out_clean)
{
    out_clean = raw;
    LLStringUtil::trim(out_clean);
    if (out_clean.empty()
        || out_clean.front() == ' ' || out_clean.back() == ' '
        || out_clean.size() > MAX_NAME_CHARS * 4   // UTF-8 worst case, before the exact character count
        || utf8str_to_wstring(out_clean).size() > MAX_NAME_CHARS)
    {
        return NAME_INVALID;
    }
    for (const char c : out_clean)
    {
        const unsigned char u = static_cast<unsigned char>(c);
        if (u < 0x20 || u == 0x7f)
        {
            return NAME_INVALID;
        }
    }
    if (isReservedName(out_clean))
    {
        return NAME_RESERVED;
    }
    return NAME_OK;
}

//-------------------------------------------------------------------------
std::vector<std::string> listNames()
{
    ensureLoaded();
    std::vector<std::string> names;
    names.reserve(store().size());
    for (const auto& kv : store())
    {
        names.push_back(kv.second.mName);
    }
    return names;
}

bool exists(const std::string& name)
{
    ensureLoaded();
    return store().find(lowerCopy(name)) != store().end();
}

bool getValues(const std::string& name, Values& out_values, std::string* out_canonical_name)
{
    ensureLoaded();
    const auto it = store().find(lowerCopy(name));
    if (it == store().end())
    {
        return false;
    }
    out_values = it->second.mValues;
    if (out_canonical_name)
    {
        *out_canonical_name = it->second.mName;
    }
    return true;
}

std::string findMatchingName(const Values& values)
{
    ensureLoaded();
    for (const auto& kv : store())
    {
        if (nearlyEqual(values, kv.second.mValues))
        {
            return kv.second.mName;
        }
    }
    return std::string();
}

SaveResult save(const std::string& raw_name, const Values& values)
{
    ensureLoaded();
    std::string clean;
    const NameCheck check = checkName(raw_name, clean);
    if (check == NAME_INVALID)
    {
        return SAVE_INVALID_NAME;
    }
    if (check == NAME_RESERVED)
    {
        return SAVE_RESERVED_NAME;
    }

    const std::string key = lowerCopy(clean);
    const auto found = store().find(key);
    const bool existed = (found != store().end());
    if (!existed && store().size() >= MAX_PRESETS)
    {
        return SAVE_LIMIT_REACHED;
    }
    Preset previous;
    if (existed)
    {
        previous = found->second;
    }

    Preset p;
    p.mName   = clean;
    p.mValues = values;
    sanitize(p.mValues);
    store()[key] = p;

    if (!writeStore())
    {
        if (existed)
        {
            store()[key] = previous;
        }
        else
        {
            store().erase(key);
        }
        return SAVE_WRITE_FAILED;
    }
    listChangedSignal()();
    return SAVE_OK;
}

SaveResult saveCurrent(const std::string& raw_name)
{
    return save(raw_name, currentValues());
}

bool remove(const std::string& name)
{
    ensureLoaded();
    const std::string key = lowerCopy(name);
    const auto found = store().find(key);
    if (found == store().end())
    {
        return false;
    }
    const Preset previous = found->second;
    store().erase(found);
    if (!writeStore())
    {
        store()[key] = previous;
        return false;
    }
    listChangedSignal()();
    return true;
}

bool applyNamed(const std::string& name)
{
    Values v;
    if (!getValues(name, v))
    {
        return false;
    }
    apply(v);
    return true;
}

//-------------------------------------------------------------------------
boost::signals2::connection connectListChanged(const signal_t::slot_type& slot)
{
    return listChangedSignal().connect(slot);
}

boost::signals2::connection connectApplied(const signal_t::slot_type& slot)
{
    return appliedSignal().connect(slot);
}

}   // namespace ALEnvIntensityPresets
