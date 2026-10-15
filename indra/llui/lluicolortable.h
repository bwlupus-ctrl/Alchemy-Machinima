/**
 * @file lluicolortable.h
 * @brief brief LLUIColorTable class header file
 *
 * $LicenseInfo:firstyear=2009&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#ifndef LL_LLUICOLORTABLE_H_
#define LL_LLUICOLORTABLE_H_

#include <boost/signals2.hpp>
#include <boost/unordered_map.hpp>
#include <map>

#include "llinitparam.h"
#include "llsingleton.h"

#include "v4color.h"

class LLUIColor;

class LLUIColorTable : public LLSingleton<LLUIColorTable>
{
    LLSINGLETON_EMPTY_CTOR(LLUIColorTable);
    LOG_CLASS(LLUIColorTable);

    // consider using sorted vector, can be much faster
    typedef boost::unordered_map<std::string, LLUIColor, ll::string_hash, std::equal_to<>>  string_color_map_t;

public:
    struct ColorParams : LLInitParam::ChoiceBlock<ColorParams>
    {
        Alternative<LLColor4>    value;
        Alternative<std::string> reference;

        ColorParams();
    };

    struct ColorEntryParams : LLInitParam::Block<ColorEntryParams>
    {
        Mandatory<std::string> name;
        Mandatory<ColorParams> color;

        ColorEntryParams();
    };

    struct Params : LLInitParam::Block<Params>
    {
        Multiple<ColorEntryParams> color_entries;

        Params();
    };

    // define colors by passing in a param block that can be generated via XUI file or manually
    void insertFromParams(const Params& p);

    // reset all colors to default magenta color
    void clear();

    // color lookup
    LLUIColor getColor(std::string_view name, const LLColor4& default_color = LLColor4::magenta) const;

    // if the color is in the table, it's value is changed, otherwise it is added
    void setColor(std::string_view name, const LLColor4& color);

    // Changes a skin colour's value in place, so existing LLUIColor references see it,
    // without marking it user-modified: saveUserSettings() ignores it. Used by colour
    // themes. Returns false when name is not a skin colour.
    bool setLoadedColor(std::string_view name, const LLColor4& color);

    // returns true if color_name exists in the table
    bool colorExists(std::string_view color_name) const;

    bool isDefault(std::string_view color_name) const;

    void resetToDefault(std::string_view color_name);

    // loads colors from settings files
    bool loadFromSettings();

    // Called at the end of every loadFromSettings(), after the skin and user files are re-read.
    boost::signals2::connection setReloadCallback(const boost::signals2::signal<void()>::slot_type& cb)
    {
        return mReloadSignal.connect(cb);
    }

    // saves colors specified by the user to the users skin directory
    void saveUserSettings(const bool scrub = false) const;

    const auto& getLoadedColors() { return mLoadedColors; }
    const auto& getUserColors() { return mUserSetColors; }

private:
    bool loadFromFilename(const std::string& filename, string_color_map_t& table);

    void insertFromParams(const Params& p, string_color_map_t& table);

    void clearTable(string_color_map_t& table);
    void setColor(std::string_view name, const LLColor4& color, string_color_map_t& table);

    string_color_map_t mLoadedColors;
    string_color_map_t mUserSetColors;

    // Skin values captured right after the skin files load. saveUserSettings() compares
    // against these, so a theme value in mLoadedColors is never mistaken for a user change.
    std::map<std::string, LLColor4, std::less<>> mSkinColors;

    boost::signals2::signal<void()> mReloadSignal;
};

#endif // LL_LLUICOLORTABLE_H
