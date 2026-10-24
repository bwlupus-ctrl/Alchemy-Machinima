#include "linden_common.h"

#include "../test/lltut.h"
#include "../alprojectorshaftlooks.h"

#include <algorithm>
#include <map>
#include <set>
#include <string>

namespace
{
using ALProjectorShaftLooks::Look;
using ALProjectorShaftLooks::Type;
using ALProjectorShaftLooks::Value;

const std::string P = "BDMergeProjectorVolumetrics";

struct Range
{
    F32 lo, hi;
};

// Hard-coded from the settings.xml comments / Director slider limits (plan section 0), not read from the
// code under test: a typo in a look value must fail here.
const std::map<std::string, Range>& ranges()
{
    static const std::map<std::string, Range> r = {
        {P + "Multiplier", {0.01f, 30.f}},
        {P + "Density", {0.f, 4.f}},
        {P + "Anisotropy", {0.f, 0.95f}},
        {P + "TintStrength", {0.f, 1.f}},
        {P + "MaxLuminance", {1.f, 64.f}},
        {P + "MaxDistance", {0.f, 64.f}},
        {P + "Feather", {0.f, 0.5f}},
        {P + "BloomFeed", {0.f, 2.f}},
        {P + "BloomFeedAnamorphic", {1.f, 4.f}},
        {P + "RimStrength", {0.f, 4.f}},
        {P + "RimPower", {0.5f, 8.f}},
        {P + "RimWrap", {0.f, 1.f}},
        {P + "RimThreshold", {0.f, 1.f}},
        {P + "RimSoftness", {0.f, 1.f}},
        {P + "NoiseStrength", {0.f, 1.f}},
        {P + "NoiseScale", {0.01f, 2.f}},
        {P + "NoiseSpeed", {0.f, 2.f}},
        {P + "DustIntensity", {0.f, 2.f}},
        {P + "DustScale", {0.05f, 2.f}},
        {P + "DustDrift", {0.f, 1.f}},
        {P + "FogStrength", {0.f, 1.f}},
        {P + "FogGroundDensity", {0.f, 4.f}},
        {P + "FogFalloff", {0.5f, 256.f}},
        {P + "ShadowSamples", {1.f, 4.f}},
        {P + "ShadowTint", {0.f, 1.f}},
        {P + "Resolution", {8.f, 64.f}},
        {"BDMergeSoftShadowSoftness", {0.f, 16.f}},
        {"BDMergeSoftShadowMaxPenumbra", {1.f, 16.f}},
        {"BDMergeSoftShadowFill", {0.f, 1.f}},
    };
    return r;
}

bool isAB(const std::string& name)
{
    return name == P + "Equiangular" || name == P + "AirborneTap" || name == P + "CookieFootprint";
}

std::set<std::string> ownedPlusDust()
{
    std::set<std::string> owned(ALProjectorShaftLooks::ownedSettings().begin(),
                                ALProjectorShaftLooks::ownedSettings().end());
    owned.insert(ALProjectorShaftLooks::dustSetting());
    return owned;
}
}

namespace tut
{
struct alprojectorshaftlooks_data {};
using alprojectorshaftlooks_group = test_group<alprojectorshaftlooks_data>;
using alprojectorshaftlooks_object = alprojectorshaftlooks_group::object;
alprojectorshaftlooks_group gALProjectorShaftLooksTests("ALProjectorShaftLooks");

template<> template<>
void alprojectorshaftlooks_object::test<1>()
{
    set_test_name("13 looks, unique ids, label and description limits");
    const std::vector<Look>& looks = ALProjectorShaftLooks::looks();
    ensure_equals("look count", looks.size(), (size_t)13);
    std::set<std::string> ids, labels;
    for (const Look& look : looks)
    {
        const std::string id = look.id;
        ensure("id has b: prefix: " + id, id.compare(0, 2, "b:") == 0);
        ensure("unique id " + id, ids.insert(id).second);
        ensure("unique label " + id, labels.insert(look.label).second);
        ensure("label <= 22 chars: " + id, std::string(look.label).size() <= 22);
        ensure("label not empty: " + id, std::string(look.label).size() >= 3);
        ensure("description <= 95 chars: " + id, std::string(look.description).size() <= 95);
        ensure("description not empty: " + id, std::string(look.description).size() >= 10);
        ensure("look has values: " + id, look.count > 0 && look.values != nullptr);
        ensure("findLook round trip: " + id, ALProjectorShaftLooks::findLook(id) == &look);
    }
    ensure("unknown id not found", ALProjectorShaftLooks::findLook("b:nope") == nullptr);
    ensure("legacy id is not a look id", ALProjectorShaftLooks::findLook("3") == nullptr);
}

template<> template<>
void alprojectorshaftlooks_object::test<2>()
{
    set_test_name("every value key is owned; keys unique inside a look");
    const std::set<std::string> owned = ownedPlusDust();
    for (const Look& look : ALProjectorShaftLooks::looks())
    {
        std::set<std::string> seen;
        for (size_t i = 0; i < look.count; ++i)
        {
            const std::string setting = look.values[i].setting;
            ensure("owned: " + setting + " in " + look.id, owned.count(setting) == 1);
            ensure("no duplicate: " + setting + " in " + look.id, seen.insert(setting).second);
        }
    }
}

template<> template<>
void alprojectorshaftlooks_object::test<3>()
{
    set_test_name("ownership excludes quality, calibration, permutations and masters");
    const std::set<std::string> dropped = {
        P + "HalfRes", P + "Temporal", P + "TemporalBlend", P + "TemporalReject", P + "TemporalBeamDepth",
        P + "Scissor", P + "Adaptive", P + "MinResolution", P + "Dither", P + "TemporalDither",
        P + "FrustumClip", P + "ShadowJitterTap", P + "FogBase", P + "PyramidScissor", P + "ConservativeShadow",
        "BDMergeSoftShadowSun", "BDMergeGoboAnisotropic", "BDMergeProjectorVolumetrics",
    };
    std::set<std::string> unique;
    for (const std::string& name : ALProjectorShaftLooks::ownedSettings())
    {
        ensure("not dropped: " + name, dropped.count(name) == 0);
        ensure("unique in owned list: " + name, unique.insert(name).second);
        ensure("not the Dust permutation: " + name, name != ALProjectorShaftLooks::dustSetting());
        ensure("BD prefix: " + name, name.compare(0, 7, "BDMerge") == 0);
    }
    ensure_equals("owned count", ALProjectorShaftLooks::ownedSettings().size(), (size_t)34);
    ensure_equals("dust setting", std::string(ALProjectorShaftLooks::dustSetting()), P + "Dust");
}

template<> template<>
void alprojectorshaftlooks_object::test<4>()
{
    set_test_name("legacy ids map exactly");
    const char* const expected[8] = {"b:clean", "b:clean", "b:cinematic", "b:stage",
                                     "b:clear_night", "b:searchlight", "b:dust_storm", "b:dream_mist"};
    for (int i = 0; i < 8; ++i)
    {
        ensure_equals("legacy string id", ALProjectorShaftLooks::canonicalLookId(std::to_string(i)), std::string(expected[i]));
        ensure_equals("legacy combo int", ALProjectorShaftLooks::canonicalLookId((S32)i), std::string(expected[i]));
    }
    for (const Look& look : ALProjectorShaftLooks::looks())
    {
        ensure_equals("b: ids map to themselves", ALProjectorShaftLooks::canonicalLookId(look.id), std::string(look.id));
    }
    ensure_equals("8 is unknown", ALProjectorShaftLooks::canonicalLookId("8"), std::string());
    ensure_equals("-1 string is unknown", ALProjectorShaftLooks::canonicalLookId("-1"), std::string());
    ensure_equals("empty is unknown", ALProjectorShaftLooks::canonicalLookId(""), std::string());
    ensure_equals("x is unknown", ALProjectorShaftLooks::canonicalLookId("x"), std::string());
    ensure_equals("unknown b: id is unknown", ALProjectorShaftLooks::canonicalLookId("b:nope"), std::string());
    ensure_equals("combo -1 is Custom", ALProjectorShaftLooks::canonicalLookId((S32)-1), std::string());
    ensure_equals("combo 8 is unknown", ALProjectorShaftLooks::canonicalLookId((S32)8), std::string());
}

template<> template<>
void alprojectorshaftlooks_object::test<5>()
{
    set_test_name("A/B switches only in the four named looks");
    std::map<std::string, std::string> expected = {
        {P + "Equiangular", "b:searchlight,b:pipe_fog"},
        {P + "AirborneTap", "b:smoky_club"},
        {P + "CookieFootprint", "b:stained_glass"},
    };
    std::map<std::string, std::string> found;
    for (const Look& look : ALProjectorShaftLooks::looks())
    {
        for (size_t i = 0; i < look.count; ++i)
        {
            const std::string setting = look.values[i].setting;
            if (!isAB(setting)) continue;
            ensure("A/B value is on: " + setting, look.values[i].f[0] != 0.f);
            std::string& list = found[setting];
            list += (list.empty() ? "" : ",") + std::string(look.id);
        }
    }
    ensure("A/B usage matches the plan", found == expected);
}

template<> template<>
void alprojectorshaftlooks_object::test<6>()
{
    set_test_name("values are inside the settings ranges and typed correctly");
    for (const Look& look : ALProjectorShaftLooks::looks())
    {
        for (size_t i = 0; i < look.count; ++i)
        {
            const Value& v = look.values[i];
            const std::string setting = v.setting;
            const std::string where = setting + " in " + look.id;
            if (v.type == Type::BOOL)
            {
                ensure("bool is 0 or 1: " + where, v.f[0] == 0.f || v.f[0] == 1.f);
                ensure("bool setting: " + where, isAB(setting) || setting == P + "Dust" || setting == "BDMergeSoftProjectorShadows");
                continue;
            }
            if (v.type == Type::COLOR3)
            {
                ensure("only Tint is a color: " + where, setting == P + "Tint");
                for (int c = 0; c < 3; ++c)
                {
                    ensure("tint channel in 0..1: " + where, v.f[c] >= 0.f && v.f[c] <= 1.f);
                }
                continue;
            }
            const auto it = ranges().find(setting);
            ensure("known range for " + where, it != ranges().end());
            ensure("in range: " + where, v.f[0] >= it->second.lo && v.f[0] <= it->second.hi);
            if (v.type == Type::U32)
            {
                ensure("U32 is integral: " + where, v.f[0] == (F32)(U32)v.f[0]);
                ensure("U32 setting: " + where, setting == P + "Resolution" || setting == P + "ShadowSamples");
            }
            else
            {
                ensure("F32 setting is not a U32 one: " + where, setting != P + "Resolution" && setting != P + "ShadowSamples");
            }
        }
    }
}

template<> template<>
void alprojectorshaftlooks_object::test<7>()
{
    set_test_name("per-look structural rules");
    auto has = [](const Look& look, const std::string& setting, F32* out = nullptr)
    {
        for (size_t i = 0; i < look.count; ++i)
        {
            if (setting == look.values[i].setting)
            {
                if (out) *out = look.values[i].f[0];
                return true;
            }
        }
        return false;
    };
    const Look* silhouette = ALProjectorShaftLooks::findLook("b:silhouette");
    ensure("silhouette exists", silhouette != nullptr);
    F32 v = 0.f;
    ensure("silhouette has a rim", has(*silhouette, P + "RimStrength", &v) && v > 0.f);
    ensure("silhouette keeps 4 shadow taps", has(*silhouette, P + "ShadowSamples", &v) && v == 4.f);
    ensure("silhouette has soft shadows off", has(*silhouette, "BDMergeSoftProjectorShadows", &v) && v == 0.f);
    ensure("silhouette does not use A/B switches", !has(*silhouette, P + "Equiangular") && !has(*silhouette, P + "AirborneTap"));
    ensure("silhouette tint amount is not set (light colour)", !has(*silhouette, P + "TintStrength"));
    int dust_looks = 0;
    for (const Look& look : ALProjectorShaftLooks::looks())
    {
        if (has(look, P + "Dust", &v) && v != 0.f) ++dust_looks;
        if (has(look, P + "DustIntensity")) ensure("dust values only with Dust on: " + std::string(look.id), has(look, P + "Dust", &v) && v != 0.f);
    }
    ensure_equals("exactly two looks turn Dust on", dust_looks, 2);
}
}
