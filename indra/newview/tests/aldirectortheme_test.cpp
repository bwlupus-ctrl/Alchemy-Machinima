#include "linden_common.h"
#include "../test/lltut.h"
#include "../aldirectortheme.h"

#include <cmath>
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

    // LLSD::append returns the appended element, not the array, so it can't be chained.
    LLSD array(std::initializer_list<LLSD> items)
    {
        LLSD out = LLSD::emptyArray();
        for (const LLSD& item : items)
        {
            out.append(item);
        }
        return out;
    }

    LLSD accent(F64 r, F64 g, F64 b, F64 a)
    {
        return array({ r, g, b, a });
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
        theme("x", LLSD().with("KitAccent", array({ 0.1, 0.2, 0.3 }))),
        "x", knownNames, out).empty());
    ensure("string component is rejected", !ALDirectorTheme::validate(
        theme("x", LLSD().with("KitAccent", array({ "0.1", 0.2, 0.3, 1 }))),
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

template<> template<> void aldirectortheme_object::test<7>()
{
    const LLColor4 pink(1.f, 0.41f, 0.71f, 0.5f);
    const LLColor4 flipped = ALDirectorTheme::invertedColor(pink);
    ensure_approximately_equals("red keeps its place in the hue", flipped.mV[0], 0.59f, 8);
    ensure_approximately_equals("green shifts with it", flipped.mV[1], 0.f, 8);
    ensure_approximately_equals("blue shifts with it", flipped.mV[2], 0.30f, 8);
    ensure_equals("alpha is kept", flipped.mV[3], 0.5f);

    const LLColor4 twice = ALDirectorTheme::invertedColor(flipped);
    ensure_approximately_equals("two flips return the colour", twice.mV[0], pink.mV[0], 8);
    ensure_approximately_equals("two flips return the colour", twice.mV[2], pink.mV[2], 8);
}

template<> template<> void aldirectortheme_object::test<8>()
{
    ensure("Record is kept", ALDirectorTheme::keepsWhenInverted("KitRecord"));
    ensure("record label is kept", ALDirectorTheme::keepsWhenInverted("KitRecordTextOn"));
    ensure("shadows are kept", ALDirectorTheme::keepsWhenInverted("KitShadowWindow"));
    ensure("scrim is kept", ALDirectorTheme::keepsWhenInverted("KitOverlayScrim"));
    ensure("text flips", !ALDirectorTheme::keepsWhenInverted("KitText"));
    ensure("accent flips", !ALDirectorTheme::keepsWhenInverted("KitAccent"));
    ensure("window flips", !ALDirectorTheme::keepsWhenInverted("KitWindowBg"));
}

// Same input and output as INVERT_VECTOR_* in director_theme_check.py.
template<> template<> void aldirectortheme_object::test<9>()
{
    ALDirectorTheme::ColorMap colors = {
        { "KitWindowBg", LLColor4(0.10f, 0.05f, 0.10f, 1.f) },
        { "KitText", LLColor4(0.55f, 0.45f, 0.50f, 1.f) },
        { "KitIconTint", LLColor4(0.90f, 0.80f, 0.85f, 0.75f) },
        { "KitAccent", LLColor4(1.00f, 0.41f, 0.71f, 1.f) },
        { "KitAccentTextOn", LLColor4(0.12f, 0.04f, 0.10f, 1.f) },
        { "KitRecord", LLColor4(0.77f, 0.28f, 0.28f, 1.f) },
        { "KitShadowWindow", LLColor4(0.f, 0.f, 0.f, 0.5f) },
    };
    ALDirectorTheme::invertColors(colors);
    ensure("KitWindowBg[0]", std::fabs(colors["KitWindowBg"].mV[0] - 0.9500f) < 0.0005f);
    ensure("KitWindowBg[1]", std::fabs(colors["KitWindowBg"].mV[1] - 0.9000f) < 0.0005f);
    ensure("KitWindowBg[2]", std::fabs(colors["KitWindowBg"].mV[2] - 0.9500f) < 0.0005f);
    ensure("KitWindowBg[3]", std::fabs(colors["KitWindowBg"].mV[3] - 1.0000f) < 0.0005f);
    ensure("KitText[0]", std::fabs(colors["KitText"].mV[0] - 0.4655f) < 0.0005f);
    ensure("KitText[1]", std::fabs(colors["KitText"].mV[1] - 0.3809f) < 0.0005f);
    ensure("KitText[2]", std::fabs(colors["KitText"].mV[2] - 0.4232f) < 0.0005f);
    ensure("KitText[3]", std::fabs(colors["KitText"].mV[3] - 1.0000f) < 0.0005f);
    ensure("KitIconTint[0]", std::fabs(colors["KitIconTint"].mV[0] - 0.2000f) < 0.0005f);
    ensure("KitIconTint[1]", std::fabs(colors["KitIconTint"].mV[1] - 0.1000f) < 0.0005f);
    ensure("KitIconTint[2]", std::fabs(colors["KitIconTint"].mV[2] - 0.1500f) < 0.0005f);
    ensure("KitIconTint[3]", std::fabs(colors["KitIconTint"].mV[3] - 0.7500f) < 0.0005f);
    ensure("KitAccent[0]", std::fabs(colors["KitAccent"].mV[0] - 0.5900f) < 0.0005f);
    ensure("KitAccent[1]", std::fabs(colors["KitAccent"].mV[1] - 0.0000f) < 0.0005f);
    ensure("KitAccent[2]", std::fabs(colors["KitAccent"].mV[2] - 0.3000f) < 0.0005f);
    ensure("KitAccent[3]", std::fabs(colors["KitAccent"].mV[3] - 1.0000f) < 0.0005f);
    ensure("KitAccentTextOn[0]", std::fabs(colors["KitAccentTextOn"].mV[0] - 0.9600f) < 0.0005f);
    ensure("KitAccentTextOn[1]", std::fabs(colors["KitAccentTextOn"].mV[1] - 0.8800f) < 0.0005f);
    ensure("KitAccentTextOn[2]", std::fabs(colors["KitAccentTextOn"].mV[2] - 0.9400f) < 0.0005f);
    ensure("KitAccentTextOn[3]", std::fabs(colors["KitAccentTextOn"].mV[3] - 1.0000f) < 0.0005f);
    ensure("KitRecord[0]", std::fabs(colors["KitRecord"].mV[0] - 0.7700f) < 0.0005f);
    ensure("KitRecord[1]", std::fabs(colors["KitRecord"].mV[1] - 0.2800f) < 0.0005f);
    ensure("KitRecord[2]", std::fabs(colors["KitRecord"].mV[2] - 0.2800f) < 0.0005f);
    ensure("KitRecord[3]", std::fabs(colors["KitRecord"].mV[3] - 1.0000f) < 0.0005f);
    ensure("KitShadowWindow[0]", std::fabs(colors["KitShadowWindow"].mV[0] - 0.0000f) < 0.0005f);
    ensure("KitShadowWindow[1]", std::fabs(colors["KitShadowWindow"].mV[1] - 0.0000f) < 0.0005f);
    ensure("KitShadowWindow[2]", std::fabs(colors["KitShadowWindow"].mV[2] - 0.0000f) < 0.0005f);
    ensure("KitShadowWindow[3]", std::fabs(colors["KitShadowWindow"].mV[3] - 0.5000f) < 0.0005f);
    ensure("text is readable after Invert",
           ALDirectorTheme::contrastRatio(colors["KitText"], colors["KitWindowBg"]) >= 4.5f);
}

// Review reproduction: normal blue control surface and near-white text are valid,
// but no completed inverted palette can satisfy both blue and the light panels.
template<> template<> void aldirectortheme_object::test<10>()
{
    ALDirectorTheme::ColorMap colors = {
        { "KitAccent", LLColor4(0.910000f, 0.639000f, 0.239000f, 1.000000f) },
        { "KitAccentTextOn", LLColor4(0.078000f, 0.078000f, 0.075000f, 1.000000f) },
        { "KitActiveFill", LLColor4(0.184000f, 0.204000f, 0.231000f, 1.000000f) },
        { "KitCardBg", LLColor4(0.106000f, 0.118000f, 0.137000f, 1.000000f) },
        { "KitControlBg", LLColor4(0.149000f, 0.165000f, 0.192000f, 1.000000f) },
        { "KitDialFace", LLColor4(0.082000f, 0.090000f, 0.106000f, 1.000000f) },
        { "KitDialNeedle", LLColor4(0.910000f, 0.639000f, 0.239000f, 1.000000f) },
        { "KitDim", LLColor4(0.608000f, 0.639000f, 0.682000f, 1.000000f) },
        { "KitIconOnAccent", LLColor4(0.078000f, 0.078000f, 0.075000f, 1.000000f) },
        { "KitIconTint", LLColor4(1.000000f, 1.000000f, 1.000000f, 0.750000f) },
        { "KitIconTintStrong", LLColor4(1.000000f, 1.000000f, 1.000000f, 1.000000f) },
        { "KitInputBg", LLColor4(0.071000f, 0.078000f, 0.094000f, 1.000000f) },
        { "KitKnobOn", LLColor4(0.078000f, 0.078000f, 0.075000f, 1.000000f) },
        { "KitLabel", LLColor4(0.788000f, 0.808000f, 0.839000f, 1.000000f) },
        { "KitMuted", LLColor4(0.541000f, 0.573000f, 0.616000f, 1.000000f) },
        { "KitPanelBg", LLColor4(0.082000f, 0.090000f, 0.106000f, 1.000000f) },
        { "KitRecord", LLColor4(0.898000f, 0.282000f, 0.302000f, 1.000000f) },
        { "KitRecordText", LLColor4(0.949000f, 0.655000f, 0.663000f, 1.000000f) },
        { "KitRecordTextOn", LLColor4(0.078000f, 0.078000f, 0.075000f, 1.000000f) },
        { "KitText", LLColor4(0.902000f, 0.910000f, 0.922000f, 1.000000f) },
        { "KitTextLabel", LLColor4(0.788000f, 0.808000f, 0.839000f, 1.000000f) },
        { "KitTextMuted", LLColor4(0.541000f, 0.573000f, 0.616000f, 1.000000f) },
        { "KitTextPrimary", LLColor4(0.902000f, 0.910000f, 0.922000f, 1.000000f) },
        { "KitTextSecondary", LLColor4(0.608000f, 0.639000f, 0.682000f, 1.000000f) },
        { "KitTextStrong", LLColor4(0.945000f, 0.949000f, 0.957000f, 1.000000f) },
        { "KitWarnBandText", LLColor4(0.953000f, 0.745000f, 0.416000f, 1.000000f) },
        { "KitWindowBg", LLColor4(0.067000f, 0.075000f, 0.086000f, 1.000000f) },
    };
    colors["KitControlBg"] = LLColor4(0.f, 0.f, 1.f, 1.f);
    ALDirectorTheme::visitContrastPairs([&](const char* fg, const char*, F32 minimum, bool)
    {
        const std::string name(fg);
        if (minimum == 4.5f && (name.find("KitText") == 0 || name == "KitLabel"
            || name == "KitDim" || name == "KitMuted"))
            colors[name] = LLColor4(.95f, .95f, .95f, 1.f);
    });
    // The new semantic text rule additionally catches the original red text
    // at 4.45:1 on blue. Adjust that tier to get a normal-valid custom theme.
    ensure_equals("exact review reproduction catches semantic red on blue",
                  ALDirectorTheme::contrastFailureCount(colors), size_t(1));
    colors["KitRecordText"] = LLColor4(.95f, .95f, .95f, 1.f);
    ensure_equals("normal custom palette passes", ALDirectorTheme::contrastFailureCount(colors), size_t(0));
    const auto original = colors;
    auto inverted = colors;
    ALDirectorTheme::invertColors(inverted);
    ensure("completed inversion still fails", ALDirectorTheme::contrastFailureCount(inverted) > 0);
    ensure("unsafe inversion is rejected", !ALDirectorTheme::tryInvertColors(colors));
    ensure("un-inverted palette is kept exactly", colors == original);
    ALDirectorTheme::ColorMap simple = {
        { "KitText", LLColor4(1.f, 1.f, 1.f, 1.f) },
        { "KitWindowBg", LLColor4(0.f, 0.f, 0.f, 1.f) }
    };
    ensure("safe inversion is applied", ALDirectorTheme::tryInvertColors(simple));
    ensure_equals("safe inverted background", simple.at("KitWindowBg"), LLColor4::white);
}
} // namespace tut
