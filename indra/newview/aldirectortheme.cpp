/**
 * @file aldirectortheme.cpp
 * @brief Director colour themes: loader and LLUIColorTable applier.
 *
 * Widgets hold LLUIColor references into LLUIColorTable nodes.  A theme is
 * written with LLUIColorTable::setLoadedColor(), which changes the skin node in
 * place, so an override reaches existing widgets on their next draw and never
 * becomes a user-set colour (colors.xml is not written because of a theme).
 * Originals are snapshotted from the skin table once, before the first
 * override, and Default writes them back.
 */

#include "llviewerprecompiledheaders.h"

#include "aldirectortheme.h"

#include <algorithm>

#include "alkitstyle.h"
#include "llcontrol.h"
#include "lldir.h"
#include "llfile.h"
#include "llsdserialize.h"
#include "lluicolortable.h"
#include "llviewercontrol.h"

namespace ALDirectorTheme
{
namespace
{
    constexpr const char* kThemesSubdir = "director_themes";
    constexpr std::size_t kMaxIdLength = 64;

    // Kit colour name -> skin default, captured before the first override.
    std::map<std::string, LLColor4>& originals()
    {
        static std::map<std::string, LLColor4> sOriginals;
        return sOriginals;
    }

    bool sSnapshotTaken = false;
    bool sSettingConnected = false;
    bool sReloadConnected = false;

    void takeSnapshot()
    {
        if (sSnapshotTaken)
        {
            return;
        }
        // getLoadedColors() holds the skin values; user-layer entries are not in it.
        for (const auto& entry : LLUIColorTable::instance().getLoadedColors())
        {
            if (isKitName(entry.first))
            {
                originals()[entry.first] = entry.second.get();
            }
        }
        // Only mark the snapshot once the skin table has loaded; an earlier call sees nothing.
        sSnapshotTaken = !originals().empty();
    }

    // Theme ids come from a saved setting, so only plain file-name characters pass.
    bool isValidId(const std::string& id)
    {
        if (id.empty() || id.size() > kMaxIdLength)
        {
            return false;
        }
        for (char c : id)
        {
            const bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
            if (!ok)
            {
                return false;
            }
        }
        return true;
    }

    std::string themesDirectory()
    {
        return gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, kThemesSubdir);
    }

    std::string themePath(const std::string& id)
    {
        return gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, kThemesSubdir, id + ".xml");
    }

    Theme builtInDefault()
    {
        Theme theme;
        theme.id = kDefaultId;
        theme.label = "Default";
        return theme;
    }

    bool colorKnown(const std::string& name)
    {
        return LLUIColorTable::instance().colorExists(name);
    }
} // namespace

std::vector<ThemeInfo> listThemes()
{
    std::vector<ThemeInfo> result;
    std::vector<std::string> files = gDirUtilp->getFilesInDir(themesDirectory());
    std::sort(files.begin(), files.end());

    bool hasDefault = false;
    for (const std::string& file : files)
    {
        if (file.size() <= 4 || file.compare(file.size() - 4, 4, ".xml") != 0)
        {
            continue;
        }
        const std::string id = file.substr(0, file.size() - 4);
        Theme theme;
        std::string error;
        if (!loadTheme(id, theme, error))
        {
            LL_WARNS("DirectorTheme") << "Skipping theme '" << id << "': " << error << LL_ENDL;
            continue;
        }
        hasDefault = hasDefault || id == kDefaultId;
        result.push_back({ theme.id, theme.label });
    }

    if (!hasDefault)
    {
        result.insert(result.begin(), { kDefaultId, "Default" });
    }
    else
    {
        // Keep Default first in the list.
        std::stable_partition(result.begin(), result.end(),
                              [](const ThemeInfo& info) { return info.id == kDefaultId; });
    }
    return result;
}

bool loadTheme(const std::string& id, Theme& out, std::string& error)
{
    if (!isValidId(id))
    {
        error = "invalid theme id";
        return false;
    }

    const std::string path = themePath(id);
    llstat file_status;
    if (LLFile::stat(path, &file_status) != 0)
    {
        if (id == kDefaultId)
        {
            out = builtInDefault();
            return true;
        }
        error = "theme file not found";
        return false;
    }

    error = checkFileSize(static_cast<std::size_t>(file_status.st_size));
    if (!error.empty())
    {
        return false;
    }

    llifstream input(path.c_str());
    if (!input.is_open())
    {
        error = "cannot open theme file";
        return false;
    }
    LLSD data;
    const S32 parsed = LLSDSerialize::fromXML(data, input);
    input.close();
    if (parsed == LLSDParser::PARSE_FAILURE)
    {
        error = "theme file is not valid LLSD XML";
        return false;
    }

    error = validate(data, id, colorKnown, out);
    return error.empty();
}

void applyTheme(const Theme& theme)
{
    takeSnapshot();
    LLUIColorTable& table = LLUIColorTable::instance();
    for (const auto& original : originals())
    {
        const auto override_it = theme.colors.find(original.first);
        const LLColor4& value = (override_it != theme.colors.end()) ? override_it->second : original.second;
        table.setLoadedColor(original.first, value);
    }
    // Scroll-list colours are plain LLColor4 values on the list, so the kit lists read them again here.
    ALKitStyle::restyleLists();
}

void applyThemeById(const std::string& id)
{
    Theme theme;
    std::string error;
    if (!loadTheme(id, theme, error))
    {
        LL_WARNS("DirectorTheme") << "Theme '" << id << "' rejected (" << error << "); using Default" << LL_ENDL;
        theme = builtInDefault();
    }
    applyTheme(theme);
}

void init()
{
    applyThemeById(gSavedSettings.getString(kSettingName));

    if (!sSettingConnected)
    {
        LLControlVariable* control = gSavedSettings.getControl(kSettingName);
        if (control)
        {
            control->getSignal()->connect(
                [](LLControlVariable*, const LLSD& new_value, const LLSD&)
                {
                    applyThemeById(new_value.asString());
                });
            sSettingConnected = true;
        }
    }

    if (!sReloadConnected)
    {
        // Advanced > Reload Color Settings re-reads the skin values, which overwrite the theme.
        // Take a fresh snapshot of the reloaded values and apply the saved theme over them again.
        LLUIColorTable::instance().setReloadCallback(
            []()
            {
                sSnapshotTaken = false;
                originals().clear();
                applyThemeById(gSavedSettings.getString(kSettingName));
            });
        sReloadConnected = true;
    }
}
} // namespace ALDirectorTheme
