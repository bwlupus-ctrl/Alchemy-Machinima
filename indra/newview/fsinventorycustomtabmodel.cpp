/**
 * @file fsinventorycustomtabmodel.cpp
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026 Alchemy Viewer
 * SPDX-License-Identifier: LGPL-2.1-only
 * $/LicenseInfo$
 */
#include "linden_common.h"
#include "fsinventorycustomtabmodel.h"
#include "llsdutil.h"
#include "llstring.h"

#include <set>

namespace FSInventoryCustomTabModel
{
namespace
{
bool sameEntry(const LLSD& a, const LLSD& b)
{
    return a["name"].asString() == b["name"].asString()
        && a["filter"].asString() == b["filter"].asString()
        && a["tabhidden"].asBoolean() == b["tabhidden"].asBoolean()
        && llsd_equals(a["states"], b["states"]);
}

bool validEntry(const LLSD& entry)
{
    if (!entry.isMap()
        || (entry.has("name") && !entry["name"].isString())
        || (entry.has("filter") && !entry["filter"].isString())
        || (entry.has("states") && !entry["states"].isMap())
        || (entry.has("tabhidden") && !entry["tabhidden"].isBoolean())
        || (entry.has("id") && (!entry["id"].isString() || entry["id"].asString().empty()
                               || entry["id"].asString().size() > static_cast<size_t>(MAX_NAME_LENGTH))))
    {
        return false;
    }
    const LLSD& states = entry["states"];
    if (states.has("search_type") && (!states["search_type"].isInteger()
        || states["search_type"].asInteger() < 0 || states["search_type"].asInteger() > 3))
    {
        return false;
    }
    const LLSD& ops = states["filter_ops"];
    if (states.has("filter_ops") && !ops.isMap())
    {
        return false;
    }
    // EFolderShow, EFilterCreatorType and EFilterLink have three values each.
    for (const char* key : { "show_folder_state", "creator_type", "links" })
    {
        if (ops.has(key) && (!ops[key].isInteger() || ops[key].asInteger() < 0
                            || ops[key].asInteger() > 2))
        {
            return false;
        }
    }
    return true;
}
}

LoadResult load(const LLSD& setting, bool enabled)
{
    LoadResult result;
    if (!enabled || setting.isUndefined())
    {
        result.loadedOk = true;
        return result;
    }
    if (!setting.isArray())
    {
        return result;
    }
    result.loadedOk = setting.size() <= MAX_TABS;
    std::set<std::string> reserved_ids;
    for (LLSD::Integer i = 0; i < setting.size() && i < MAX_TABS; ++i)
    {
        if (setting[i].isMap() && setting[i]["id"].isString())
        {
            reserved_ids.insert(setting[i]["id"].asString());
        }
    }
    std::set<std::string> ids;
    for (LLSD::Integer i = 0; i < setting.size() && i < MAX_TABS; ++i)
    {
        LLSD entry = setting[i];
        if (!validEntry(entry) || (entry.has("id") && !ids.insert(entry["id"].asString()).second))
        {
            result.loadedOk = false;
            continue;
        }
        if (!entry.has("id"))
        {
            // Stable migration without writing storage on load. Reserve explicit
            // IDs first so mixed old/new arrays cannot acquire duplicate IDs.
            std::string id = "legacy-" + std::to_string(i);
            while (reserved_ids.count(id) || ids.count(id)) id += "_";
            entry["id"] = id;
            ids.insert(id);
        }
        // Length alone never drops a tab or poisons the whole save transaction.
        entry["name"] = utf8str_truncate(entry["name"].asString(), MAX_NAME_LENGTH);
        entry["filter"] = utf8str_truncate(entry["filter"].asString(), MAX_FILTER_LENGTH);
        result.tabs.append(entry);
    }
    return result;
}

bool isUnchangedSetting(const LLSD& current, const LLSD& proposed)
{
    if (current.isUndefined())
    {
        return proposed.isArray() && proposed.size() == 0;
    }
    if (!current.isArray() || !proposed.isArray() || current.size() != proposed.size())
    {
        return false;
    }
    for (LLSD::Integer i = 0; i < current.size(); ++i)
    {
        if (!sameEntry(current[i], proposed[i])
            || current[i]["id"].asString() != proposed[i]["id"].asString())
        {
            return false;
        }
    }
    return true;
}

bool filterStateChanged(const LLSD& previous, const LLSD& incoming)
{
    return !previous.isMap() || previous["filter"].asString() != incoming["filter"].asString()
        || !llsd_equals(previous["states"], incoming["states"]);
}

LLSD makeEntry(const std::string& id, const std::string& name, const std::string& filter,
               const LLSD& states, bool states_valid, bool hidden)
{
    LLSD entry;
    entry["id"] = id;
    entry["name"] = utf8str_truncate(name, MAX_NAME_LENGTH);
    entry["filter"] = utf8str_truncate(filter, MAX_FILTER_LENGTH);
    if (states_valid) entry["states"] = states;
    entry["tabhidden"] = hidden;
    return entry;
}

SaveResult save(const LLSD& current, const LLSD& proposed, bool loaded_ok, bool loading,
                const std::function<void(const LLSD&)>& write)
{
    SaveResult result;
    if (!loaded_ok || loading || SaveGuard::saving()) return result;
    const auto normalized = load(proposed);
    if (!normalized.loadedOk)
    {
        result.status = SaveStatus::INVALID;
        return result;
    }
    result.tabs = normalized.tabs;
    if (isUnchangedSetting(current, result.tabs))
    {
        result.status = SaveStatus::UNCHANGED;
        return result;
    }
    {
        SaveGuard guard;
        write(result.tabs);
    }
    result.status = SaveStatus::WRITTEN;
    return result;
}

std::vector<S32> reconcile(const LLSD& previous, const LLSD& incoming)
{
    std::vector<S32> result(static_cast<size_t>(incoming.size()), -1);
    std::set<S32> used;
    for (LLSD::Integer i = 0; i < incoming.size(); ++i)
    {
        const std::string id = incoming[i]["id"].asString();
        for (LLSD::Integer j = 0; j < previous.size(); ++j)
        {
            if (used.count(j)) continue;
            const std::string old_id = previous[j]["id"].asString();
            if ((!id.empty() && id == old_id)
                || ((id.empty() || old_id.empty()) && sameEntry(previous[j], incoming[i])))
            {
                result[static_cast<size_t>(i)] = j;
                used.insert(j);
                break;
            }
        }
    }
    return result;
}

U32 SaveGuard::sDepth = 0;
SaveGuard::SaveGuard() { ++sDepth; }
SaveGuard::~SaveGuard() { --sDepth; }
bool SaveGuard::saving() { return sDepth != 0; }
}
