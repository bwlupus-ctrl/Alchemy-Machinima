/**
 * @file lluicolortable.cpp
 * @brief brief LLUIColorTable class implementation file
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

#include "linden_common.h"

#include <queue>

#include "lldir.h"
#include "llui.h"
#include "lluicolortable.h"
#include "lluictrlfactory.h"

LLUIColorTable::ColorParams::ColorParams()
:   value("value"),
    reference("reference")
{
}

LLUIColorTable::ColorEntryParams::ColorEntryParams()
:   name("name"),
    color("")
{
}

LLUIColorTable::Params::Params()
:   color_entries("color")
{
}

void LLUIColorTable::insertFromParams(const Params& p, string_color_map_t& table)
{
    // this map will contain all color references after the following loop
    typedef std::map<std::string, std::string> string_string_map_t;
    string_string_map_t unresolved_refs;

    for(LLInitParam::ParamIterator<ColorEntryParams>::const_iterator it = p.color_entries.begin();
        it != p.color_entries.end();
        ++it)
    {
        ColorEntryParams color_entry = *it;
        if(color_entry.color.value.isChosen())
        {
            setColor(color_entry.name(), color_entry.color.value, table);
        }
        else
        {
            unresolved_refs.insert(string_string_map_t::value_type(color_entry.name, color_entry.color.reference));
        }
    }

    // maintain an in order queue of visited references for better debugging of cycles
    typedef std::queue<std::string> string_queue_t;
    string_queue_t ref_chain;

    // maintain a map of the previously visited references in the reference chain for detecting cycles
    typedef std::map<std::string, string_string_map_t::iterator> string_color_ref_iter_map_t;
    string_color_ref_iter_map_t visited_refs;

    // loop through the unresolved color references until there are none left
    while(!unresolved_refs.empty())
    {
        // we haven't visited any references yet
        visited_refs.clear();

        string_string_map_t::iterator current = unresolved_refs.begin();
        string_string_map_t::iterator previous;

        while(true)
        {
            if(current != unresolved_refs.end())
            {
                // locate the current reference in the previously visited references...
                string_color_ref_iter_map_t::iterator visited = visited_refs.lower_bound(current->first);
                if(visited != visited_refs.end()
                && !(visited_refs.key_comp()(current->first, visited->first)))
                {
                    // ...if we find the current reference in the previously visited references
                    // we know that there is a cycle
                    std::string ending_ref = current->first;
                    std::string warning("The following colors form a cycle: ");

                    // warn about the references in the chain and remove them from
                    // the unresolved references map because they cannot be resolved
                    for(string_color_ref_iter_map_t::iterator iter = visited_refs.begin();
                        iter != visited_refs.end();
                        ++iter)
                    {
                        if(!ref_chain.empty())
                        {
                            warning += ref_chain.front() + "->";
                            ref_chain.pop();
                        }
                        unresolved_refs.erase(iter->second);
                    }

                    LL_WARNS() << warning + ending_ref << LL_ENDL;

                    break;
                }
                else
                {
                    // ...continue along the reference chain
                    ref_chain.push(current->first);
                    visited_refs.insert(visited, string_color_ref_iter_map_t::value_type(current->first, current));
                }
            }
            else
            {
                // since this reference does not refer to another reference it must refer to an
                // actual color, lets find it...
                string_color_map_t::iterator color_value = mLoadedColors.find(previous->second);

                if(color_value != mLoadedColors.end())
                {
                    // ...we found the color, and we now add every reference in the reference chain
                    // to the color map
                    for(string_color_ref_iter_map_t::iterator iter = visited_refs.begin();
                        iter != visited_refs.end();
                        ++iter)
                    {
                        setColor(iter->first, color_value->second, mLoadedColors);
                        unresolved_refs.erase(iter->second);
                    }

                    break;
                }
                else
                {
                    // ... we did not find the color which imples that the current reference
                    // references a non-existant color
                    for(string_color_ref_iter_map_t::iterator iter = visited_refs.begin();
                        iter != visited_refs.end();
                        ++iter)
                    {
                        LL_WARNS() << iter->first << " references a non-existent color" << LL_ENDL;
                        unresolved_refs.erase(iter->second);
                    }

                    break;
                }
            }

            // find the next color reference in the reference chain
            previous = current;
            current = unresolved_refs.find(current->second);
        }
    }
}

void LLUIColorTable::clear()
{
    clearTable(mLoadedColors);
    clearTable(mUserSetColors);
}

LLUIColor LLUIColorTable::getColor(std::string_view name, const LLColor4& default_color) const
{
    string_color_map_t::const_iterator iter = mUserSetColors.find(name);

    if(iter != mUserSetColors.end())
    {
        return LLUIColor(&iter->second);
    }

    iter = mLoadedColors.find(name);

    if(iter != mLoadedColors.end())
    {
        return LLUIColor(&iter->second);
    }

    return  LLUIColor(default_color);
}

// update user color, loaded colors are parsed on initialization
void LLUIColorTable::setColor(std::string_view name, const LLColor4& color)
{
    // An explicit edit: the colour no longer follows the skin / colour theme.
    mFollowLoaded.erase(std::string(name));
    auto it = mUserSetColors.find(name);
    if(it != mUserSetColors.end())
    {
        it->second = color;
    }
    else
    {
        string_color_map_t::iterator base_iter = mLoadedColors.find(name);
        if (base_iter != mLoadedColors.end())
        {
            LLColor4 original_color = base_iter->second.get();
            auto color_handle = mLoadedColors.extract(base_iter);
            auto new_color_pair = mUserSetColors.insert(std::move(color_handle));
            new_color_pair.position->second = color;
            mLoadedColors.emplace(name, LLUIColor(original_color));
        }
        else
        {
            mUserSetColors.insert(it, std::make_pair(name, color));
        }
    }
}

bool LLUIColorTable::setLoadedColor(std::string_view name, const LLColor4& color)
{
    auto it = mLoadedColors.find(name);
    if (it == mLoadedColors.end())
    {
        return false;
    }
    // Same node, same address: widgets holding this colour pick up the new value on their next draw.
    it->second.set(color);
    // A colour the user reset still has a user node (it is never erased, see resetToDefault()); it
    // mirrors the loaded value, so it follows the theme again.
    if (mFollowLoaded.find(name) != mFollowLoaded.end())
    {
        auto user_iter = mUserSetColors.find(name);
        if (user_iter != mUserSetColors.end())
        {
            user_iter->second.set(color);
        }
    }
    return true;
}

bool LLUIColorTable::isDefault(std::string_view name) const
{
    // "Default" is the skin value, even while a colour theme overrides the loaded layer.
    if (mFollowLoaded.find(name) != mFollowLoaded.end())
    {
        return true; // reset: the user node only mirrors the skin / theme value
    }
    auto base_iter = mSkinColors.find(name);
    string_color_map_t::const_iterator user_iter = mUserSetColors.find(name);
    if (base_iter != mSkinColors.end())
    {
        if(user_iter != mUserSetColors.end())
            return user_iter->second.get() == base_iter->second;

        return true;
    }
    else if (user_iter != mUserSetColors.end()) // user only color ???
    {
        return true;
    }

    return false;
}

void LLUIColorTable::resetToDefault(std::string_view name)
{
    // INVARIANT: no node of mLoadedColors or mUserSetColors is ever erased or moved here. LLUIColor
    // holds a raw pointer to a node (getColor() hands out the user node while one exists, otherwise the
    // loaded node), and widgets keep those pointers across Advanced > Reload Color Settings and further
    // resets. Erasing or re-homing a node would leave them dangling. So a reset only changes VALUES:
    // the user node is set to the value the loaded layer shows (the skin colour, or the active colour
    // theme's value) and the name is marked as "following the loaded layer". Such a colour is default
    // (isDefault()), is not saved (saveUserSettings()), and tracks later theme changes (setLoadedColor()).
    auto user_iter = mUserSetColors.find(name);
    if (user_iter == mUserSetColors.end())
    {
        return;
    }
    auto loaded_iter = mLoadedColors.find(name);
    if (loaded_iter == mLoadedColors.end())
    {
        return; // a user-only colour has nothing to reset to
    }
    user_iter->second.set(loaded_iter->second.get());
    mFollowLoaded.insert(std::string(name));
}

bool LLUIColorTable::loadFromSettings()
{
    bool result = false;

    // pass constraint=LLDir::ALL_SKINS because we want colors.xml from every
    // skin dir
    for (const std::string& colors_path :
                  gDirUtilp->findSkinnedFilenames(LLDir::SKINBASE, "colors.xml", LLDir::ALL_SKINS))
    {
        result |= loadFromFilename(colors_path, mLoadedColors);
    }

    mSkinColors.clear();
    for (const auto& color_pair : mLoadedColors)
    {
        mSkinColors[color_pair.first] = color_pair.second.get();
    }

    // The user file is read into a scratch table and merged by value, so no existing node moves. A
    // colour the file sets is a user edit again (it stops following the loaded layer); every other
    // reset colour is re-synced to the freshly reloaded loaded value.
    std::string user_filename = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, "colors.xml");
    string_color_map_t from_file;
    loadFromFilename(user_filename, from_file);
    for (const auto& color_pair : from_file)
    {
        mFollowLoaded.erase(color_pair.first);
        setColor(color_pair.first, color_pair.second.get(), mUserSetColors);
    }
    for (const std::string& name : mFollowLoaded)
    {
        auto user_iter = mUserSetColors.find(name);
        auto loaded_iter = mLoadedColors.find(name);
        if (user_iter != mUserSetColors.end() && loaded_iter != mLoadedColors.end())
        {
            user_iter->second.set(loaded_iter->second.get());
        }
    }

    // Listeners (the Director colour theme) re-apply their overrides over the freshly loaded skin values.
    mReloadSignal();

    return result;
}

void LLUIColorTable::saveUserSettings(const bool scrub /* = false */) const
{
    Params params;

    for (const auto& color_pair : mUserSetColors)
    {
        // A reset colour only mirrors the skin / theme value: never saved.
        if (mFollowLoaded.find(color_pair.first) != mFollowLoaded.end())
            continue;

        // Compare user color value with the skin default, skip if equal
        auto itd = mSkinColors.find(color_pair.first);
        if(itd != mSkinColors.end() && itd->second == color_pair.second.get())
            continue;

        if (!scrub || color_pair.first.find("ColorPaletteEntry") != std::string::npos)
        {
            ColorEntryParams color_entry;
            color_entry.name = color_pair.first;
            color_entry.color.value = color_pair.second;

            params.color_entries.add(color_entry);
        }
    }

    LLXMLNodePtr output_node = new LLXMLNode("colors", false);
    LLXUIParser parser;
    parser.writeXUI(output_node, params);

    if(!output_node->isNull())
    {
        const std::string& filename = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, "colors.xml");
        LLFILE *fp = LLFile::fopen(filename, LLFILE_MODE("w"));

        if(fp != NULL)
        {
            LLXMLNode::writeHeaderToFile(fp);
            output_node->writeToFile(fp);

            fclose(fp);
        }
    }
}

bool LLUIColorTable::colorExists(std::string_view color_name) const
{
    return ((mLoadedColors.find(color_name) != mLoadedColors.end())
         || (mUserSetColors.find(color_name) != mUserSetColors.end()));
}

void LLUIColorTable::clearTable(string_color_map_t& table)
{
    for(string_color_map_t::iterator it = table.begin();
        it != table.end();
        ++it)
    {
        it->second = LLColor4::magenta;
    }
}

// this method inserts a color into the table if it does not exist
// if the color already exists it changes the color
void LLUIColorTable::setColor(std::string_view name, const LLColor4& color, string_color_map_t& table)
{
    string_color_map_t::iterator it = table.find(name);
    if(it != table.end())
    {
        it->second = color;
    }
    else
    {
        table.insert(it, string_color_map_t::value_type(name, color));
    }
}

bool LLUIColorTable::loadFromFilename(const std::string& filename, string_color_map_t& table)
{
    LLXMLNodePtr root;

    if(!LLXMLNode::parseFile(filename, root, NULL))
    {
        LL_WARNS() << "Unable to parse color file " << filename << LL_ENDL;
        return false;
    }

    if(!root->hasName("colors"))
    {
        LL_WARNS() << filename << " is not a valid color definition file" << LL_ENDL;
        return false;
    }

    Params params;
    LLXUIParser parser;
    parser.readXUI(root, params, filename);

    if(params.validateBlock())
    {
        insertFromParams(params, table);
    }
    else
    {
        LL_WARNS() << filename << " failed to load" << LL_ENDL;
        return false;
    }

    return true;
}

void LLUIColorTable::insertFromParams(const Params& p)
{
    insertFromParams(p, mUserSetColors);
}

// EOF

