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
