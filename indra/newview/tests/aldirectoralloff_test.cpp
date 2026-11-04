#include "linden_common.h"
#include "../test/lltut.h"
#include "../aldirectoralloff.h"
#include "llsdserialize.h"
#include "llsdutil.h"
#include <set>
#include <sstream>
#include <limits>

namespace tut
{
struct aldirectoralloff_data
{
    // Stand-in for LLControlVariable::type(), with the shipped control types.
    static eControlType controlType(const ALDirectorAllOff::Feature& feature)
    {
        const std::string name(feature.setting);
        if (feature.operation != ALDirectorAllOff::Operation::Default ||
            name == "AlchemyEnvMoonLinked" || name == "AlchemyEnvLocalLightIncludeRig") return TYPE_BOOLEAN;
        if (name.find("TintColor") != std::string::npos) return TYPE_COL4;
        return TYPE_F32;
    }

    static LLSD valueFor(const ALDirectorAllOff::Feature& feature)
    {
        switch (controlType(feature))
        {
        case TYPE_BOOLEAN: return LLSD(true);
        case TYPE_COL4:
        {
            LLSD color = LLSD::emptyArray();
            for (F64 component : {0.25, 0.5, 0.75, 1.0}) color.append(component);
            return color;
        }
        default: return LLSD(1.25);
        }
    }
};
typedef test_group<aldirectoralloff_data> aldirectoralloff_group;
typedef aldirectoralloff_group::object aldirectoralloff_object;
aldirectoralloff_group gALDirectorAllOff("aldirectoralloff");

template<> template<>
void aldirectoralloff_object::test<1>()
{
    using namespace ALDirectorAllOff;
    std::set<std::string> keys;
    S32 defaults = 0, rigs = 0;
    for (const Feature& feature : FEATURES)
    {
        ensure("unique feature", keys.insert(feature.setting).second);
        if (feature.operation == Operation::Default) ++defaults;
        if (feature.operation == Operation::Rig) ++rigs;
    }
    ensure_equals("complete contract", keys.size(), size_t(43));
    ensure_equals("nineteen Personal Lighting values", defaults, 19);
    ensure_equals("one all-slot rig dispatcher", rigs, 1);
    ensure("mode preference excluded", keys.count("DirectorActionAllOffMode") == 0);
    ensure("ALR master excluded", keys.count("AvatarLightResponseEnabled") == 0);
    ensure("ALR persistence excluded", keys.count("AvatarLightResponsePersist") == 0);
    ensure("recipient preference excluded", keys.count("AlchemyEnvEnvironmentOnly") == 0);
}

template<> template<>
void aldirectoralloff_object::test<2>()
{
    using namespace ALDirectorAllOff;
    S32 count = 0;
    LLSD saved = captureSettings([&](const Feature& feature)
    {
        ++count;
        return aldirectoralloff_data::valueFor(feature);
    });
    ensure("captured settings valid", validSettings(saved, controlType));
    saved["CineLightRigEnabled"] = true;
    LLSD backup;
    backup["version"] = 1;
    backup["settings"] = saved;
    // The account file remains a full byte string, even when the session has
    // no live entries. Exercise XML escaping and more than the live ALR cap.
    std::string file;
    for (S32 i = 0; i < 600; ++i) file += "<entry label='saved & dormant'>" + std::to_string(i) + "</entry>\n";
    file.append("\0\xff\x01", 3); // arbitrary prior file bytes survive too
    backup["light_response"]["file_bytes"] = LLSD::Binary(file.begin(), file.end());
    backup["light_response"]["live"] = LLSD::emptyMap();
    backup["actors"] = LLSD::emptyArray();
    backup["rig_mask"] = 21;
    backup["rig_selected"] = 2;
    backup["rig_group"] = true;
    std::stringstream xml;
    LLSDSerialize::toPrettyXML(backup, xml);
    LLSD decoded;
    ensure("XML parse", LLSDSerialize::fromXML(decoded, xml) != LLSDParser::PARSE_FAILURE);
    ensure("entire backup round trip", llsd_equals(backup, decoded));
    ensure("round trip settings valid", validSettings(decoded["settings"], controlType));
    LLSD restored = LLSD::emptyMap();
    applySettings(decoded["settings"], true, [&](const Feature& feature, const LLSD& value)
        { restored[feature.setting] = value; });
    ensure("same table restores every captured value", llsd_equals(saved, restored));
    const LLSD::Binary raw = decoded["light_response"]["file_bytes"].asBinary();
    ensure_equals("dormant saved file preserved", std::string(raw.begin(), raw.end()), file);
    saved.erase("AlchemyEnvSunEV");
    ensure("partial backup rejected", !validSettings(saved, controlType));
}

template<> template<>
void aldirectoralloff_object::test<3>()
{
    using namespace ALDirectorAllOff;
    LLSD saved = captureSettings(valueFor);
    S32 visited = 0;
    applySettings(saved, false, [&](const Feature&, const LLSD& value)
    {
        ++visited;
        ensure("off requests operation rather than backup value", value.isUndefined());
    });
    ensure_equals("off uses complete table", visited, 43);
    saved["AlchemyEnvSunEV"] = "bad value";
    ensure("invalid numeric rejected", !validSettings(saved, controlType));
}

template<> template<>
void aldirectoralloff_object::test<4>()
{
    using namespace ALDirectorAllOff;
    const LLSD clean = captureSettings(valueFor);
    ensure("all shipped types accepted", validSettings(clean, controlType));
    // Every row rejects wrong scalar types; Color4 also checks shape/components.
    for (const Feature& feature : FEATURES)
    {
        LLSD bad = clean;
        bad[feature.setting] = "wrong type";
        ensure("string rejected", !validSettings(bad, controlType));
        bad[feature.setting] = controlType(feature) == TYPE_BOOLEAN ? LLSD(1.0) : LLSD(true);
        ensure("bool/real coercion rejected", !validSettings(bad, controlType));
        bad[feature.setting] = 1;
        ensure("integer coercion rejected", !validSettings(bad, controlType));
        if (controlType(feature) == TYPE_F32)
        {
            bad[feature.setting] = std::numeric_limits<F64>::infinity();
            ensure("nonfinite F32 rejected", !validSettings(bad, controlType));
        }
        if (controlType(feature) == TYPE_COL4)
        {
            bad[feature.setting] = clean[feature.setting];
            bad[feature.setting].erase(3);
            ensure("short color rejected", !validSettings(bad, controlType));
            bad[feature.setting] = clean[feature.setting];
            bad[feature.setting].append(1.0);
            ensure("long color rejected", !validSettings(bad, controlType));
            for (S32 component = 0; component < 4; ++component)
            {
                bad[feature.setting] = clean[feature.setting];
                bad[feature.setting][component] = 1;
                ensure("integer color component rejected", !validSettings(bad, controlType));
                bad[feature.setting][component] = std::numeric_limits<F64>::quiet_NaN();
                ensure("nonfinite color component rejected", !validSettings(bad, controlType));
            }
        }
    }
    ensure("unsupported real control type rejected", !validSettings(clean, [](const Feature&) { return TYPE_STRING; }));
}
}
