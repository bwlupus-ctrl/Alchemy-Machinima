/**
 * @file alprojectorshaftpresets.cpp
 * @brief Applies projector-shaft looks and saved user looks.
 *
 * Ownership (which settings a look resets and writes) is ALProjectorShaftLooks::ownedSettings().
 * Quality, calibration and the other shader permutations are never touched by a look.
 */

#include "llviewerprecompiledheaders.h"
#include "alpresetlook.h"

#include "alprojectorshaftpresets.h"
#include "alprojectorshaftlooks.h"

#include "llcontrol.h"
#include "llviewercontrol.h"

namespace
{
void resetOwned()
{
    for (const std::string& name : ALProjectorShaftLooks::ownedSettings())
    {
        if (LLControlVariable* control = gSavedSettings.getControl(name))
        {
            control->resetToDefault(true);
        }
    }
}

bool dustDefault()
{
    if (LLControlVariable* control = gSavedSettings.getControl(ALProjectorShaftLooks::dustSetting()))
    {
        return control->getDefault().asBoolean();
    }
    return false;
}

// Dust is a compile-time shader permutation. It stays out of the reset loop and is written once,
// after every scalar control, and only when it changes, so any genuine transition costs at most
// one rebuild and reselecting a look never rebuilds off/on.
void writeDust(bool wants_dust)
{
    if (gSavedSettings.getBOOL(ALProjectorShaftLooks::dustSetting()) != wants_dust)
    {
        gSavedSettings.setBOOL(ALProjectorShaftLooks::dustSetting(), wants_dust);
    }
}
}

namespace ALProjectorShaftPresets
{
const std::vector<std::string>& trackedSettings()
{
    static const std::vector<std::string> settings = []
    {
        std::vector<std::string> result = ALProjectorShaftLooks::ownedSettings();
        result.emplace_back(ALProjectorShaftLooks::dustSetting());
        return result;
    }();
    return settings;
}

bool apply(const std::string& id)
{
    // Validate before touching anything: an unknown id (including Custom) writes nothing.
    const ALProjectorShaftLooks::Look* look = ALProjectorShaftLooks::findLook(ALProjectorShaftLooks::canonicalLookId(id));
    if (!look)
    {
        return false;
    }
    if (ALPresetLook::replaying()) return false;

    ALPresetLook::Load load;
    resetOwned();
    // Selecting a look is an explicit request to see the effect. The master is not owned.
    gSavedSettings.setBOOL("BDMergeProjectorVolumetrics", true);

    bool wants_dust = dustDefault();
    for (size_t i = 0; i < look->count; ++i)
    {
        const ALProjectorShaftLooks::Value& value = look->values[i];
        if (std::string(value.setting) == ALProjectorShaftLooks::dustSetting())
        {
            wants_dust = value.f[0] != 0.f;
            continue;
        }
        switch (value.type)
        {
        case ALProjectorShaftLooks::Type::F32:
            gSavedSettings.setF32(value.setting, value.f[0]);
            break;
        case ALProjectorShaftLooks::Type::U32:
            gSavedSettings.setU32(value.setting, static_cast<U32>(value.f[0] + 0.5f));
            break;
        case ALProjectorShaftLooks::Type::BOOL:
            gSavedSettings.setBOOL(value.setting, value.f[0] != 0.f);
            break;
        case ALProjectorShaftLooks::Type::COLOR3:
        {
            if (LLControlVariable* control = gSavedSettings.getControl(value.setting))
            {
                LLSD color = LLSD::emptyArray();
                color.append(value.f[0]);
                color.append(value.f[1]);
                color.append(value.f[2]);
                control->set(color);
            }
            break;
        }
        }
    }
    writeDust(wants_dust);
    return true;
}

bool applyValues(const LLSD& values)
{
    if (!values.isMap())
    {
        return false;
    }
    if (ALPresetLook::replaying()) return false;

    ALPresetLook::Load load;
    resetOwned();
    for (const std::string& name : ALProjectorShaftLooks::ownedSettings())
    {
        if (!values.has(name) || values[name].isUndefined())
        {
            continue;
        }
        if (LLControlVariable* control = gSavedSettings.getControl(name).get())
        {
            control->set(values[name]);
        }
    }
    const std::string dust = ALProjectorShaftLooks::dustSetting();
    writeDust(values.has(dust) && !values[dust].isUndefined() ? values[dust].asBoolean() : dustDefault());
    return true;
}
}
