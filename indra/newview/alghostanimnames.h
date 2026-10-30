/** Session-local animation display names; no inventory walks or asset requests. */
#ifndef AL_ALGHOSTANIMNAMES_H
#define AL_ALGHOSTANIMNAMES_H

#include "lluuid.h"
#include <map>
#include <string>
#include <utility>
#include <vector>

class ALGhostAnimNames
{
public:
    using Library = std::vector<std::pair<std::string, LLUUID>>;
    using Builtin = const char* (*)(const LLUUID&);
    static bool hasDisplayName(const std::string& name)
    { return name.find_first_not_of(" \t\r\n\f\v") != std::string::npos; }

    const std::string& resolve(const LLUUID& id, const Library& library,
                               U64 revision, Builtin builtin,
                               const std::string& learned = std::string(), U64 metadata_revision = 0)
    {
        if (!mInitialized || revision != mRevision)
        {
            mInitialized = true;
            mRevision = revision;
            mInventory.clear();
            mCache.clear();
            mLearned.clear();
            // Library order is authoritative. Duplicate assets keep the first name.
            for (const auto& entry : library)
                if (hasDisplayName(entry.first)) mInventory.emplace(entry.second, entry.first);
        }
        if (metadata_revision != mMetadataRevision)
        {
            mMetadataRevision = metadata_revision;
            mLearned.clear(); // inventory and built-in lookup indexes are retained
        }
        auto inventory = mInventory.find(id);
        if (inventory != mInventory.end()) return inventory->second;
        auto cached = mCache.find(id);
        if (cached == mCache.end())
        {
            const char* name = builtin ? builtin(id) : nullptr;
            cached = mCache.emplace(id, name && *name ? name : "Unknown animation").first;
        }
        // A late session name may replace an unknown, never a built-in name.
        if (cached->second != "Unknown animation") return cached->second;
        if (hasDisplayName(learned)) mLearned.emplace(id, learned);
        const auto session = mLearned.find(id);
        return session != mLearned.end() ? session->second : cached->second;
    }

private:
    bool mInitialized = false;
    U64 mRevision = 0;
    U64 mMetadataRevision = 0;
    std::map<LLUUID, std::string> mInventory;
    std::map<LLUUID, std::string> mCache;
    std::map<LLUUID, std::string> mLearned;
};
#endif
