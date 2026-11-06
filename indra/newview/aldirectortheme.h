/**
 * @file aldirectortheme.h
 * @brief Director colour themes: pure validation (header-only) plus the
 *        loader/applier declared for aldirectortheme.cpp.
 *
 * A theme file is an LLSD XML map:
 *   {version:1, id:"<stem>", label:"...", colors:{KitName:[r,g,b,a], ...}}
 * Files live in app_settings/director_themes/<id>.xml.  Theme files are
 * untrusted input: any defect rejects the whole theme and the viewer keeps
 * the previous colours (or Default).
 */

#ifndef AL_DIRECTOR_THEME_H
#define AL_DIRECTOR_THEME_H

#include "llsd.h"
#include "v4color.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace ALDirectorTheme
{
    inline constexpr S32 kSchemaVersion = 1;
    inline constexpr std::size_t kMaxEntries = 200;
    inline constexpr std::size_t kMaxFileBytes = 64 * 1024;
    inline constexpr const char* kSettingName = "DirectorTheme";
    inline constexpr const char* kInvertSettingName = "DirectorThemeInvert";
    inline constexpr const char* kDefaultId = "default";

    struct Theme
    {
        std::string id;
        std::string label;
        // Only validated Kit* names with finite components in 0..1.
        std::map<std::string, LLColor4> colors;
    };

    // Name predicate supplied by the caller: true when the colour exists in the table.
    using NameCheck = std::function<bool(const std::string&)>;

    inline bool isKitName(const std::string& name)
    {
        return name.size() > 3 && name.compare(0, 3, "Kit") == 0;
    }

    // Invert theme. scripts/code_tools/director_theme_check.py carries the same
    // rules and pair table so every theme is gated inverted; the unit test and
    // the gate's selftest pin the same vector, so a change here must change both.
    using ColorMap = std::map<std::string, LLColor4>;

    // Record stays recognisably Record. Shadows and scrims darken whatever is
    // beneath them, so they stay black on a light theme too.
    inline bool keepsWhenInverted(const std::string& name)
    {
        static const char* const kKept[] = {
            "KitRecord", "KitRecordTextOn", "KitErrorRed", "KitFlash", "KitRedTint22",
            "KitDimBlack65", "KitOverlayScrim", "KitMapArcBackFill"
        };
        for (const char* kept : kKept)
        {
            if (name == kept)
            {
                return true;
            }
        }
        return name.compare(0, 9, "KitShadow") == 0;
    }

    // HSL lightness L -> 1 - L with hue and saturation unchanged. In HSL every
    // channel is L plus a fixed offset, so that is one shift of 1 - (max + min).
    inline LLColor4 invertedColor(const LLColor4& color)
    {
        const F32 high = std::max({ color.mV[0], color.mV[1], color.mV[2] });
        const F32 low = std::min({ color.mV[0], color.mV[1], color.mV[2] });
        const F32 shift = 1.f - (high + low);
        LLColor4 out = color;
        for (S32 i = 0; i < 3; ++i)
        {
            out.mV[i] = std::clamp(color.mV[i] + shift, 0.f, 1.f);
        }
        return out;
    }

    inline F32 relativeLuminance(const LLColor4& color)
    {
        F32 linear[3];
        for (S32 i = 0; i < 3; ++i)
        {
            const F32 v = color.mV[i];
            linear[i] = v <= 0.03928f ? v / 12.92f : std::pow((v + 0.055f) / 1.055f, 2.4f);
        }
        return 0.2126f * linear[0] + 0.7152f * linear[1] + 0.0722f * linear[2];
    }

    inline F32 contrastRatio(const LLColor4& first, const LLColor4& second)
    {
        const F32 a = relativeLuminance(first);
        const F32 b = relativeLuminance(second);
        return (std::max(a, b) + 0.05f) / (std::min(a, b) + 0.05f);
    }

    // Moves fg toward black or white, whichever the background allows, until it
    // reaches minimum over bg. Translucent colours are judged as drawn.
    inline void nudgeForContrast(ColorMap& colors, const char* fg_name, const char* bg_name, F32 minimum)
    {
        const auto fg = colors.find(fg_name);
        const auto bg = colors.find(bg_name);
        if (fg == colors.end() || bg == colors.end() || bg->second.mV[3] < 1.f)
        {
            return;
        }
        const auto drawn = [&bg](const LLColor4& color)
        {
            const F32 alpha = color.mV[3];
            LLColor4 out = color;
            for (S32 i = 0; i < 3; ++i)
            {
                out.mV[i] = color.mV[i] * alpha + bg->second.mV[i] * (1.f - alpha);
            }
            out.mV[3] = 1.f;
            return out;
        };
        const F32 target = relativeLuminance(bg->second) > 0.18f ? 0.f : 1.f;
        for (S32 step = 0; step < 40 && contrastRatio(drawn(fg->second), bg->second) < minimum; ++step)
        {
            for (S32 i = 0; i < 3; ++i)
            {
                fg->second.mV[i] += (target - fg->second.mV[i]) * 0.08f;
            }
        }
    }

    // One ordered table for correction and final validation. Python selftest parses it.
    template<typename Visitor>
    inline void visitContrastPairs(Visitor visitor)
    {
        struct Pair { const char* fg; const char* bg; F32 minimum; bool composite; };
        static const Pair kContrastPairs[] = {
            { "KitText", "KitWindowBg", 4.5f, false },
            { "KitText", "KitCardBg", 4.5f, false },
            { "KitText", "KitPanelBg", 4.5f, false },
            { "KitText", "KitInputBg", 4.5f, false },
            { "KitText", "KitControlBg", 4.5f, false },
            { "KitTextPrimary", "KitWindowBg", 4.5f, false },
            { "KitTextPrimary", "KitCardBg", 4.5f, false },
            { "KitTextPrimary", "KitPanelBg", 4.5f, false },
            { "KitTextPrimary", "KitInputBg", 4.5f, false },
            { "KitTextPrimary", "KitControlBg", 4.5f, false },
            { "KitTextStrong", "KitWindowBg", 4.5f, false },
            { "KitTextStrong", "KitCardBg", 4.5f, false },
            { "KitTextStrong", "KitPanelBg", 4.5f, false },
            { "KitTextStrong", "KitInputBg", 4.5f, false },
            { "KitTextStrong", "KitControlBg", 4.5f, false },
            { "KitTextLabel", "KitWindowBg", 4.5f, false },
            { "KitTextLabel", "KitCardBg", 4.5f, false },
            { "KitTextLabel", "KitPanelBg", 4.5f, false },
            { "KitTextLabel", "KitInputBg", 4.5f, false },
            { "KitTextLabel", "KitControlBg", 4.5f, false },
            { "KitTextSecondary", "KitWindowBg", 4.5f, false },
            { "KitTextSecondary", "KitCardBg", 4.5f, false },
            { "KitTextSecondary", "KitPanelBg", 4.5f, false },
            { "KitTextSecondary", "KitInputBg", 4.5f, false },
            { "KitTextSecondary", "KitControlBg", 4.5f, false },
            { "KitTextMuted", "KitWindowBg", 4.5f, false },
            { "KitTextMuted", "KitCardBg", 4.5f, false },
            { "KitTextMuted", "KitPanelBg", 4.5f, false },
            { "KitTextMuted", "KitInputBg", 4.5f, false },
            { "KitTextMuted", "KitControlBg", 4.5f, false },
            { "KitLabel", "KitWindowBg", 4.5f, false },
            { "KitLabel", "KitCardBg", 4.5f, false },
            { "KitLabel", "KitPanelBg", 4.5f, false },
            { "KitLabel", "KitInputBg", 4.5f, false },
            { "KitLabel", "KitControlBg", 4.5f, false },
            { "KitDim", "KitWindowBg", 4.5f, false },
            { "KitDim", "KitCardBg", 4.5f, false },
            { "KitDim", "KitPanelBg", 4.5f, false },
            { "KitDim", "KitInputBg", 4.5f, false },
            { "KitDim", "KitControlBg", 4.5f, false },
            { "KitMuted", "KitWindowBg", 4.5f, false },
            { "KitMuted", "KitCardBg", 4.5f, false },
            { "KitMuted", "KitPanelBg", 4.5f, false },
            { "KitMuted", "KitInputBg", 4.5f, false },
            { "KitMuted", "KitControlBg", 4.5f, false },
            { "KitWarnBandText", "KitWindowBg", 4.5f, false },
            { "KitWarnBandText", "KitCardBg", 4.5f, false },
            { "KitWarnBandText", "KitPanelBg", 4.5f, false },
            { "KitWarnBandText", "KitInputBg", 4.5f, false },
            { "KitWarnBandText", "KitControlBg", 4.5f, false },
            { "KitWarnBandText", "KitRowHover", 4.5f, false },
            { "KitRecordText", "KitWindowBg", 4.5f, false },
            { "KitRecordText", "KitCardBg", 4.5f, false },
            { "KitRecordText", "KitPanelBg", 4.5f, false },
            { "KitRecordText", "KitInputBg", 4.5f, false },
            { "KitRecordText", "KitControlBg", 4.5f, false },
            { "KitRecordText", "KitRowHover", 4.5f, false },
            { "KitKnobOn", "KitText", 4.5f, false },
            { "KitAccent", "KitKnobOn", 4.5f, false },
            { "KitTextStrong", "KitActiveFill", 4.5f, false },
            { "KitAccentTextOn", "KitAccent", 4.5f, false },
            { "KitAccent", "KitCardBg", 3.0f, false },
            { "KitKnobOn", "KitAccent", 4.5f, false },
            { "KitKnobOn", "KitText", 4.5f, false },
            { "KitRecordTextOn", "KitRecord", 4.5f, false },
            { "KitDialNeedle", "KitDialFace", 3.0f, false },
            { "KitDialNeedle", "KitCardBg", 3.0f, false },
            { "KitDialNeedle", "KitRowHover", 3.0f, false },
            { "KitIconOnAccent", "KitAccent", 3.0f, true },
            { "KitIconOnAccent", "KitRecord", 3.0f, true },
            { "KitIconTint", "KitWindowBg", 3.0f, true },
            { "KitIconTint", "KitCardBg", 3.0f, true },
            { "KitIconTint", "KitPanelBg", 3.0f, true },
            { "KitIconTint", "KitInputBg", 3.0f, true },
            { "KitIconTint", "KitControlBg", 3.0f, true },
            { "KitIconTintStrong", "KitWindowBg", 3.0f, true },
            { "KitIconTintStrong", "KitCardBg", 3.0f, true },
            { "KitIconTintStrong", "KitPanelBg", 3.0f, true },
            { "KitIconTintStrong", "KitInputBg", 3.0f, true },
            { "KitIconTintStrong", "KitControlBg", 3.0f, true },
        };
        for (const auto& pair : kContrastPairs)
            visitor(pair.fg, pair.bg, pair.minimum, pair.composite);
    }

    inline std::size_t contrastFailureCount(const ColorMap& colors)
    {
        std::size_t failures = 0;
        visitContrastPairs([&](const char* fg_name, const char* bg_name, F32 minimum, bool composite)
        {
            const auto fg = colors.find(fg_name), bg = colors.find(bg_name);
            if (fg == colors.end() || bg == colors.end()) return;
            LLColor4 drawn = fg->second;
            const F32 alpha = drawn.mV[3];
            if (bg->second.mV[3] < 1.f || (!composite && alpha < 1.f))
            {
                ++failures;
                return;
            }
            for (S32 i = 0; i < 3; ++i)
                drawn.mV[i] = drawn.mV[i] * alpha + bg->second.mV[i] * (1.f - alpha);
            if (contrastRatio(drawn, bg->second) < minimum) ++failures;
        });
        return failures;
    }

    inline void makeReadable(ColorMap& colors)
    {
        // Later pairs can move a colour an earlier pair uses as its background.
        for (S32 pass = 0; pass < 4; ++pass)
            visitContrastPairs([&](const char* fg, const char* bg, F32 minimum, bool)
            {
                nudgeForContrast(colors, fg, bg, minimum);
            });
    }

    inline void invertColors(ColorMap& colors)
    {
        for (auto& entry : colors)
        {
            if (!keepsWhenInverted(entry.first))
            {
                entry.second = invertedColor(entry.second);
            }
        }
        makeReadable(colors);
    }

    // On an impossible custom palette, leave the complete un-inverted map intact.
    inline bool tryInvertColors(ColorMap& colors)
    {
        ColorMap inverted = colors;
        invertColors(inverted);
        if (contrastFailureCount(inverted)) return false;
        colors = std::move(inverted);
        return true;
    }

    // Byte-size cap, checked before the file is parsed. Returns "" when within the cap.
    inline std::string checkFileSize(std::size_t bytes)
    {
        if (bytes > kMaxFileBytes)
        {
            return "theme file exceeds " + std::to_string(kMaxFileBytes) + " bytes";
        }
        return std::string();
    }

    // Pure validation of an already-parsed theme. expected_id is the file stem.
    // Returns "" when the theme is accepted (out is filled), otherwise a reason
    // (out is left unspecified).
    inline std::string validate(const LLSD& data, const std::string& expected_id,
                                const NameCheck& known_name, Theme& out)
    {
        if (!data.isMap())
        {
            return "theme root is not a map";
        }
        if (!data.has("version") || !data["version"].isInteger()
            || data["version"].asInteger() != kSchemaVersion)
        {
            return "unsupported or missing theme version";
        }
        if (!data.has("id") || !data["id"].isString() || data["id"].asString() != expected_id)
        {
            return "theme id does not match its file name";
        }
        if (!data.has("label") || !data["label"].isString() || data["label"].asString().empty())
        {
            return "theme label missing";
        }
        if (!data.has("colors") || !data["colors"].isMap())
        {
            return "theme colors must be a map";
        }

        const LLSD& colors = data["colors"];
        if (colors.size() > kMaxEntries)
        {
            return "too many colour entries";
        }

        std::map<std::string, LLColor4> accepted;
        for (LLSD::map_const_iterator it = colors.beginMap(); it != colors.endMap(); ++it)
        {
            const std::string& name = it->first;
            const LLSD& value = it->second;
            if (!isKitName(name))
            {
                return "colour name is not a Kit* name: " + name;
            }
            if (!known_name || !known_name(name))
            {
                return "unknown colour name: " + name;
            }
            if (!value.isArray() || value.size() != 4)
            {
                return "colour must be [r,g,b,a]: " + name;
            }
            F64 comp[4] = { 0.0, 0.0, 0.0, 0.0 };
            for (S32 i = 0; i < 4; ++i)
            {
                const LLSD& c = value[i];
                if (!c.isReal() && !c.isInteger())
                {
                    return "colour component is not a number: " + name;
                }
                comp[i] = c.asReal();
                if (!std::isfinite(comp[i]) || comp[i] < 0.0 || comp[i] > 1.0)
                {
                    return "colour component outside 0..1: " + name;
                }
            }
            accepted[name] = LLColor4(static_cast<F32>(comp[0]), static_cast<F32>(comp[1]),
                                      static_cast<F32>(comp[2]), static_cast<F32>(comp[3]));
        }

        out.id = expected_id;
        out.label = data["label"].asString();
        out.colors = std::move(accepted);
        return std::string();
    }

    // ---- Viewer side (aldirectortheme.cpp) -------------------------------

    struct ThemeInfo
    {
        std::string id;
        std::string label;
    };

    // Every valid theme in app_settings/director_themes, Default always first.
    std::vector<ThemeInfo> listThemes();

    // Loads and validates one theme by id. Returns false with a reason in error.
    bool loadTheme(const std::string& id, Theme& out, std::string& error);

    // Overrides the named colours in LLUIColorTable. Default restores the originals.
    void applyTheme(const Theme& theme);

    // Loads the theme named by id and applies it; falls back to Default on failure.
    void applyThemeById(const std::string& id);

    // Call once after LLUIColorTable::loadFromSettings(). Applies the saved
    // DirectorTheme and re-applies whenever that setting changes.
    void init();
} // namespace ALDirectorTheme

#endif // AL_DIRECTOR_THEME_H
