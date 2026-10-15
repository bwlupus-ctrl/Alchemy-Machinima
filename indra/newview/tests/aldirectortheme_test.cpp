#include "linden_common.h"
#include "../test/lltut.h"
#include "../aldirectortheme.h"

#include <limits>

namespace tut
{
struct aldirectortheme_data {};
typedef test_group<aldirectortheme_data> aldirectortheme_group;
typedef aldirectortheme_group::object aldirectortheme_object;
aldirectortheme_group gAlDirectorTheme("aldirectortheme");

namespace
{
    bool knownNames(const std::string& name)
    {
        return name == "KitAccent" || name == "KitText" || name == "KitWindowBg";
    }

    LLSD theme(const std::string& id, const LLSD& colors, S32 version = 1)
    {
        return LLSD().with("version", version)
                     .with("id", id)
                     .with("label", "Fixture")
                     .with("colors", colors);
    }

    LLSD accent(F64 r, F64 g, F64 b, F64 a)
    {
        return LLSD::emptyArray().append(r).append(g).append(b).append(a);
    }
}

template<> template<> void aldirectortheme_object::test<1>()
{
    const LLSD data = theme("second_life", LLSD().with("KitAccent", accent(0.1, 0.8, 0.7, 1.0)));
    ALDirectorTheme::Theme out;
    const std::string error = ALDirectorTheme::validate(data, "second_life", knownNames, out);
    ensure_equals("valid theme is accepted", error, std::string());
    ensure_equals("id is the file stem", out.id, std::string("second_life"));
    ensure_equals("label is kept", out.label, std::string("Fixture"));
    ensure_equals("one override is kept", out.colors.size(), size_t(1));
    ensure("accent components survive", out.colors.at("KitAccent").mV[VRED] > 0.09f
                                         && out.colors.at("KitAccent").mV[VRED] < 0.11f);
}

template<> template<> void aldirectortheme_object::test<2>()
{
    ALDirectorTheme::Theme out;
    const std::string error = ALDirectorTheme::validate(theme("default", LLSD::emptyMap()),
                                                        "default", knownNames, out);
    ensure_equals("default theme with no overrides is accepted", error, std::string());
    ensure_equals("default theme has no overrides", out.colors.size(), size_t(0));
}

template<> template<> void aldirectortheme_object::test<3>()
{
    ALDirectorTheme::Theme out;
    ensure("version 2 is rejected", !ALDirectorTheme::validate(
        theme("second_life", LLSD::emptyMap(), 2), "second_life", knownNames, out).empty());
    ensure("real version 1.0 is rejected", !ALDirectorTheme::validate(
        LLSD().with("version", 1.0).with("id", "x").with("label", "x").with("colors", LLSD::emptyMap()),
        "x", knownNames, out).empty());
    ensure("id must match the file stem", !ALDirectorTheme::validate(
        theme("other", LLSD::emptyMap()), "second_life", knownNames, out).empty());
    ensure("label is required", !ALDirectorTheme::validate(
        LLSD().with("version", 1).with("id", "x").with("colors", LLSD::emptyMap()),
        "x", knownNames, out).empty());
    ensure("colors must be a map", !ALDirectorTheme::validate(
        LLSD().with("version", 1).with("id", "x").with("label", "x").with("colors", LLSD::emptyArray()),
        "x", knownNames, out).empty());
}

template<> template<> void aldirectortheme_object::test<4>()
{
    ALDirectorTheme::Theme out;
    ensure("non-Kit names are rejected", !ALDirectorTheme::validate(
        theme("x", LLSD().with("ChatFontSize", accent(0, 0, 0, 1))), "x", knownNames, out).empty());
    ensure("unknown Kit names are rejected", !ALDirectorTheme::validate(
        theme("x", LLSD().with("KitNotReal", accent(0, 0, 0, 1))), "x", knownNames, out).empty());
    ensure("names are case-sensitive Kit prefixes", !ALDirectorTheme::isKitName("kitAccent"));
    ensure("Kit prefix is recognised", ALDirectorTheme::isKitName("KitAccent"));
}

template<> template<> void aldirectortheme_object::test<5>()
{
    ALDirectorTheme::Theme out;
    const F64 nan = std::numeric_limits<F64>::quiet_NaN();
    ensure("NaN component is rejected", !ALDirectorTheme::validate(
        theme("x", LLSD().with("KitAccent", accent(nan, 0.5, 0.5, 1))), "x", knownNames, out).empty());
    ensure("infinite component is rejected", !ALDirectorTheme::validate(
        theme("x", LLSD().with("KitAccent", accent(std::numeric_limits<F64>::infinity(), 0.5, 0.5, 1))),
        "x", knownNames, out).empty());
    ensure("component above 1 is rejected", !ALDirectorTheme::validate(
        theme("x", LLSD().with("KitAccent", accent(1.5, 0.5, 0.5, 1))), "x", knownNames, out).empty());
    ensure("negative component is rejected", !ALDirectorTheme::validate(
        theme("x", LLSD().with("KitAccent", accent(-0.1, 0.5, 0.5, 1))), "x", knownNames, out).empty());
    ensure("three components are rejected", !ALDirectorTheme::validate(
        theme("x", LLSD().with("KitAccent", LLSD::emptyArray().append(0.1).append(0.2).append(0.3))),
        "x", knownNames, out).empty());
    ensure("string component is rejected", !ALDirectorTheme::validate(
        theme("x", LLSD().with("KitAccent", LLSD::emptyArray().append("0.1").append(0.2).append(0.3).append(1))),
        "x", knownNames, out).empty());
}

template<> template<> void aldirectortheme_object::test<6>()
{
    ALDirectorTheme::Theme out;
    LLSD colors = LLSD::emptyMap();
    for (S32 i = 0; i <= 200; ++i)
    {
        colors[std::string("KitAccent") + std::to_string(i)] = accent(0.5, 0.5, 0.5, 1);
    }
    const auto everyName = [](const std::string& name) { return name.compare(0, 9, "KitAccent") == 0; };
    ensure("more than 200 entries is rejected", !ALDirectorTheme::validate(
        theme("x", colors), "x", everyName, out).empty());

    ensure("file at the byte cap is accepted", ALDirectorTheme::checkFileSize(ALDirectorTheme::kMaxFileBytes).empty());
    ensure("file above the byte cap is rejected", !ALDirectorTheme::checkFileSize(ALDirectorTheme::kMaxFileBytes + 1).empty());
}
} // namespace tut
