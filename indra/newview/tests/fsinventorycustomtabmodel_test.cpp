/**
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026 Alchemy Viewer
 * SPDX-License-Identifier: LGPL-2.1-only
 * $/LicenseInfo$
 */
#include "linden_common.h"
#include "../test/lltut.h"
#include "../fsinventorycustomtabmodel.h"
#include "llsdserialize.h"
#include "llsdutil.h"
#include <boost/signals2/signal.hpp>

#include <sstream>

namespace tut
{
struct custom_tabs_data
{
    LLSD tab(const std::string& id = "a")
    {
        LLSD entry;
        entry["id"] = id;
        entry["name"] = "My animation";
        entry["filter"] = "dance";
        entry["states"]["filter_ops"]["types"] = 32;
        entry["states"]["filter_ops"]["show_folder_state"] = 1;
        entry["states"]["filter_ops"]["creator_type"] = 2;
        entry["states"]["filter_ops"]["links"] = 2;
        entry["states"]["search_type"] = 3;
        entry["tabhidden"] = true;
        return entry;
    }

    LLSD array(const LLSD& entry)
    {
        LLSD result = LLSD::emptyArray();
        result.append(entry);
        return result;
    }
};
using custom_tabs_group = test_group<custom_tabs_data>;
using custom_tabs_object = custom_tabs_group::object;
custom_tabs_group custom_tabs_tests("FSInventoryCustomTabModel");

template<> template<>
void custom_tabs_object::test<1>()
{
    const LLSD original = array(tab());
    std::stringstream xml;
    LLSDSerialize::toPrettyXML(original, xml);
    LLSD decoded;
    LLSDSerialize::fromXML(decoded, xml);
    const auto loaded = FSInventoryCustomTabModel::load(decoded);
    ensure("complete round trip", loaded.loadedOk);
    ensure("name/filter/states/search_type/hidden/identity survive LLSD XML", llsd_equals(original, loaded.tabs));
    ensure_equals("explicit name", loaded.tabs[0]["name"].asString(), "My animation");
    ensure_equals("original substring", loaded.tabs[0]["filter"].asString(), "dance");
    ensure_equals("UUID search type", loaded.tabs[0]["states"]["search_type"].asInteger(), 3);
    ensure("hidden", loaded.tabs[0]["tabhidden"].asBoolean());
}

template<> template<>
void custom_tabs_object::test<2>()
{
    LLSD tabs = LLSD::emptyArray();
    for (S32 i = 0; i < FSInventoryCustomTabModel::MAX_TABS; ++i) tabs.append(tab(std::to_string(i)));
    ensure("64 accepted", FSInventoryCustomTabModel::load(tabs).loadedOk);
    tabs.append(tab("65"));
    const auto partial = FSInventoryCustomTabModel::load(tabs);
    ensure("65 refuses save", !partial.loadedOk);
    ensure_equals("never creates more than 64 panels", partial.tabs.size(), 64);
}

template<> template<>
void custom_tabs_object::test<3>()
{
    LLSD entry = tab();
    entry["name"] = std::string(128, 'n');
    entry["filter"] = std::string(FSInventoryCustomTabModel::MAX_FILTER_LENGTH, 'f');
    ensure("exact limits accepted", FSInventoryCustomTabModel::load(array(entry)).loadedOk);
    entry["name"] = std::string(129, 'n');
    auto loaded = FSInventoryCustomTabModel::load(array(entry));
    ensure("overlong name keeps the tab", loaded.loadedOk);
    ensure_equals("name truncated at limit", loaded.tabs[0]["name"].asString().size(), size_t(128));
    entry["name"] = "valid";
    entry["filter"] = std::string(FSInventoryCustomTabModel::MAX_FILTER_LENGTH + 1, 'f');
    loaded = FSInventoryCustomTabModel::load(array(entry));
    ensure("overlong filter keeps the tab", loaded.loadedOk);
    ensure_equals("filter truncated at limit", loaded.tabs[0]["filter"].asString().size(), size_t(FSInventoryCustomTabModel::MAX_FILTER_LENGTH));
}

template<> template<>
void custom_tabs_object::test<4>()
{
    LLSD tabs = array(tab());
    tabs.append("broken entry");
    auto loaded = FSInventoryCustomTabModel::load(tabs);
    ensure("partial load refuses save (mLoadedOk contract)", !loaded.loadedOk);
    ensure_equals("valid portion remains available", loaded.tabs.size(), 1);
    ensure("non-array refuses save", !FSInventoryCustomTabModel::load(LLSD("broken array")).loadedOk);
    LLSD entry = tab();
    entry["states"]["search_type"] = 4;
    ensure("invalid enum refuses save", !FSInventoryCustomTabModel::load(array(entry)).loadedOk);
    entry = tab();
    entry["states"]["filter_ops"] = "broken ops";
    ensure("invalid states refuse save", !FSInventoryCustomTabModel::load(array(entry)).loadedOk);
    tabs = array(tab());
    tabs.append(tab());
    ensure("duplicate identities refuse save", !FSInventoryCustomTabModel::load(tabs).loadedOk);
}

template<> template<>
void custom_tabs_object::test<5>()
{
    const LLSD original = array(tab());
    ensure("no-op", FSInventoryCustomTabModel::isUnchangedSetting(original, original));
    for (const char* key : { "name", "filter", "id" })
    {
        LLSD changed = original;
        changed[0][key] = "changed";
        ensure("string field change", !FSInventoryCustomTabModel::isUnchangedSetting(original, changed));
    }
    LLSD changed = original;
    changed[0]["tabhidden"] = false;
    ensure("hidden change", !FSInventoryCustomTabModel::isUnchangedSetting(original, changed));
    changed = original;
    changed[0]["states"]["search_type"] = 0;
    ensure("states change", !FSInventoryCustomTabModel::isUnchangedSetting(original, changed));
    ensure("empty default", FSInventoryCustomTabModel::isUnchangedSetting(LLSD(), LLSD::emptyArray()));
    ensure("malformed is not empty default", !FSInventoryCustomTabModel::isUnchangedSetting(LLSD("bad"), LLSD::emptyArray()));
}

template<> template<>
void custom_tabs_object::test<6>()
{
    const LLSD stored = array(tab());
    const auto disabled = FSInventoryCustomTabModel::load(stored, false);
    ensure("off is a valid empty view", disabled.loadedOk);
    ensure_equals("off creates no entries/panels", disabled.tabs.size(), 0);
    ensure("off leaves storage intact", llsd_equals(stored, array(tab())));
    ensure("on restores storage", FSInventoryCustomTabModel::load(stored).loadedOk);
}

template<> template<>
void custom_tabs_object::test<7>()
{
    LLSD window_b = array(tab("a"));
    window_b.append(tab("b"));
    LLSD saved_a = window_b;
    saved_a[0]["filter"] = "changed in A";
    S32 synchronous_rebuilds = 0;
    S32 writes = 0;
    LLSD storage = window_b;
    boost::signals2::signal<void()> setting_signal;
    auto connection = setting_signal.connect([&]()
    {
        if (!FSInventoryCustomTabModel::SaveGuard::saving()) ++synchronous_rebuilds;
    });
    // Invoke the production transaction used by FSInventoryCustomTabs::save.
    // The fixture supplies only setLLSD's synchronous storage/signal boundary.
    const auto saved = FSInventoryCustomTabModel::save(storage, saved_a, true, false,
        [&](const LLSD& value)
        {
            ++writes;
            storage = value;
            setting_signal();
        });
    ensure("production transaction wrote", saved.status == FSInventoryCustomTabModel::SaveStatus::WRITTEN);
    ensure_equals("real writer invoked once", writes, 1);
    ensure("storage actually changed", llsd_equals(storage, saved_a));
    ensure_equals("no synchronous rebuild", synchronous_rebuilds, 0);
    ensure("guard restored", !FSInventoryCustomTabModel::SaveGuard::saving());
    const auto reuse = FSInventoryCustomTabModel::reconcile(window_b, saved_a);
    ensure_equals("changed tab updates in place", reuse[0], 0);
    ensure_equals("unchanged tab retains its tree", reuse[1], 1);
    saved_a.erase(0);
    const auto after_close = FSInventoryCustomTabModel::reconcile(window_b, saved_a);
    ensure_equals("closing A does not rebuild surviving B", after_close[0], 1);
    saved_a.append(tab("c"));
    const auto after_add = FSInventoryCustomTabModel::reconcile(window_b, saved_a);
    ensure_equals("only the new ID gets a new lazy panel", after_add[1], -1);
    connection.disconnect();
}

template<> template<>
void custom_tabs_object::test<8>()
{
    LLSD legacy = array(tab());
    legacy[0].erase("id");
    const LLSD modern = array(tab());
    const auto reuse = FSInventoryCustomTabModel::reconcile(legacy, modern);
    ensure_equals("legacy migration retains the panel", reuse[0], 0);
    LLSD duplicate = legacy;
    duplicate.append(legacy[0]);
    const auto distinct = FSInventoryCustomTabModel::reconcile(duplicate, duplicate);
    ensure_equals("identical legacy tab one", distinct[0], 0);
    ensure_equals("identical legacy tab two", distinct[1], 1);
}

template<> template<>
void custom_tabs_object::test<9>()
{
    LLSD mixed = array(tab());
    mixed[0].erase("id");
    mixed.append(tab("legacy-0"));
    const auto first = FSInventoryCustomTabModel::load(mixed);
    const auto second = FSInventoryCustomTabModel::load(mixed);
    ensure("mixed legacy/modern input accepted", first.loadedOk);
    ensure("two windows migrate identically", llsd_equals(first.tabs, second.tabs));
    ensure("migration cannot collide with an explicit ID",
           first.tabs[0]["id"].asString() != first.tabs[1]["id"].asString());
    ensure("migrated state is itself valid", FSInventoryCustomTabModel::load(first.tabs).loadedOk);
    ensure("load does not mutate the stored legacy setting", !mixed[0].has("id"));
}

template<> template<>
void custom_tabs_object::test<10>()
{
    LLSD renamed = tab();
    renamed["name"] = "renamed in A";
    ensure("rename does not refilter", !FSInventoryCustomTabModel::filterStateChanged(tab(), renamed));
    renamed["tabhidden"] = false;
    ensure("hide/show does not refilter", !FSInventoryCustomTabModel::filterStateChanged(tab(), renamed));
    renamed["filter"] = "different substring";
    ensure("substring does refilter", FSInventoryCustomTabModel::filterStateChanged(tab(), renamed));
    renamed = tab();
    renamed["states"]["search_type"] = 0;
    ensure("states do refilter", FSInventoryCustomTabModel::filterStateChanged(tab(), renamed));
}

template<> template<>
void custom_tabs_object::test<11>()
{
    LLSD proposed = LLSD::emptyArray();
    proposed.append(FSInventoryCustomTabModel::makeEntry("a", "valid name", "first filter", tab()["states"], false, true));
    proposed.append(FSInventoryCustomTabModel::makeEntry("b", "other tab", "second filter", tab()["states"], true, false));
    LLSD storage = LLSD::emptyArray();
    const auto saved = FSInventoryCustomTabModel::save(storage, proposed, true, false,
        [&](const LLSD& value) { storage = value; });
    ensure("one invalid parameter block does not refuse the write", saved.status == FSInventoryCustomTabModel::SaveStatus::WRITTEN);
    ensure_equals("both tabs persist", storage.size(), 2);
    ensure("invalid states omitted", !storage[0].has("states"));
    ensure("other states retained", llsd_equals(storage[1]["states"], tab()["states"]));
    ensure_equals("invalid-state tab keeps substring", storage[0]["filter"].asString(), "first filter");
}

template<> template<>
void custom_tabs_object::test<12>()
{
    // 300 four-byte characters exactly match the editor limit. Larger imported
    // filters truncate on a code-point boundary without dropping any other tab.
    std::string filter;
    for (S32 i = 0; i < 300; ++i) filter += "\xF0\x9F\x8E\xAC";
    LLSD proposed = array(tab());
    proposed[0]["filter"] = filter + "\xC3\xA9";
    proposed.append(tab("b"));
    LLSD storage = LLSD::emptyArray();
    const auto saved = FSInventoryCustomTabModel::save(storage, proposed, true, false,
        [&](const LLSD& value) { storage = value; });
    ensure("long filter cannot refuse all tabs", saved.status == FSInventoryCustomTabModel::SaveStatus::WRITTEN);
    ensure_equals("all tabs retained", storage.size(), 2);
    ensure_equals("full 300-character editor text retained", storage[0]["filter"].asString(), filter);
    proposed[0]["filter"] = std::string(1199, 'f') + "\xC3\xA9";
    const auto loaded = FSInventoryCustomTabModel::load(proposed);
    ensure("UTF-8 boundary truncation remains a complete load", loaded.loadedOk);
    ensure_equals("never split a UTF-8 code point", loaded.tabs[0]["filter"].asString().size(), size_t(1199));
}

template<> template<>
void custom_tabs_object::test<13>()
{
    LLSD partial = array(tab());
    partial.append("broken tab");
    const auto loaded = FSInventoryCustomTabModel::load(partial);
    LLSD storage = partial;
    S32 writes = 0;
    auto result = FSInventoryCustomTabModel::save(storage, loaded.tabs, loaded.loadedOk, false,
        [&](const LLSD& value) { ++writes; storage = value; });
    ensure("partial-load flag reaches the real save policy", result.status == FSInventoryCustomTabModel::SaveStatus::BLOCKED);
    ensure_equals("partial load never reaches the writer", writes, 0);
    ensure("partial storage untouched", llsd_equals(storage, partial));
    result = FSInventoryCustomTabModel::save(array(tab()), array(tab()), true, false,
        [&](const LLSD&) { ++writes; });
    ensure("no-op suppression in production save", result.status == FSInventoryCustomTabModel::SaveStatus::UNCHANGED);
    ensure_equals("no-op never reaches the writer", writes, 0);
}
}
