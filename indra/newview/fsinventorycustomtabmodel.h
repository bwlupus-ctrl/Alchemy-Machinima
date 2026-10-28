/**
 * @file fsinventorycustomtabmodel.h
 * @brief Pure persistence and reconciliation rules for custom inventory tabs.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026 Alchemy Viewer
 * SPDX-License-Identifier: LGPL-2.1-only
 * $/LicenseInfo$
 */
#ifndef FS_INVENTORY_CUSTOM_TAB_MODEL_H
#define FS_INVENTORY_CUSTOM_TAB_MODEL_H

#include "llsd.h"
#include <functional>
#include <vector>

namespace FSInventoryCustomTabModel
{
constexpr S32 MAX_TABS = 64;
constexpr S32 MAX_NAME_LENGTH = 128;
// The editor allows 300 Unicode characters, up to four UTF-8 bytes each.
constexpr S32 MAX_FILTER_LENGTH = 300 * 4;

struct LoadResult
{
    LLSD tabs = LLSD::emptyArray();
    bool loadedOk = false;
};

// Invalid structure/excess tabs refuse saving; long UTF-8 strings are truncated.
LoadResult load(const LLSD& setting, bool enabled = true);
bool isUnchangedSetting(const LLSD& current, const LLSD& proposed);
bool filterStateChanged(const LLSD& previous, const LLSD& incoming);
LLSD makeEntry(const std::string& id, const std::string& name, const std::string& filter,
               const LLSD& states, bool states_valid, bool hidden);

enum class SaveStatus { BLOCKED, INVALID, UNCHANGED, WRITTEN };
struct SaveResult
{
    SaveStatus status = SaveStatus::BLOCKED;
    LLSD tabs;
};
// The real controller and headless fixtures share this entire write transaction.
// The writer supplies setLLSD and its synchronous signal, not the save policy.
SaveResult save(const LLSD& current, const LLSD& proposed, bool loaded_ok, bool loading,
                const std::function<void(const LLSD&)>& write);

// Match stable IDs first, then legacy Firestorm entries. -1 means a new panel.
// Unchanged panels keep their folder trees, observers, selection and open state.
std::vector<S32> reconcile(const LLSD& previous, const LLSD& incoming);

// Settings signals are synchronous. Callers reconcile other windows explicitly
// after this guard leaves scope, rather than rebuilding from inside setLLSD().
class SaveGuard
{
public:
    SaveGuard();
    ~SaveGuard();
    SaveGuard(const SaveGuard&) = delete;
    SaveGuard& operator=(const SaveGuard&) = delete;
    static bool saving();
private:
    static U32 sDepth;
};
}
#endif
