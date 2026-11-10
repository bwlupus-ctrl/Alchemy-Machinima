/**
 * @file alghostanimdrop.cpp
 * @brief Pure rules for playing an inventory animation on Ghost Studio clones.
 */
#include "linden_common.h"

#include "alghostanimdrop.h"

#include <algorithm>
#include <cctype>

namespace
{
    bool hasDisplayName(const std::string& name)
    {
        return name.find_first_not_of(" \t\r\n\f\v") != std::string::npos;
    }

    std::string lower(std::string text)
    {
        std::transform(text.begin(), text.end(), text.begin(),
                       [](unsigned char c) { return (char)std::tolower(c); });
        return text;
    }

    std::string stripDashes(const std::string& text)
    {
        std::string out;
        out.reserve(text.size());
        for (char c : text)
        {
            if (c != '-') out += c;
        }
        return out;
    }
}

namespace ALGhostAnimDrop
{
    EVerdict classify3D(bool over_entity_clone, bool cargo_is_animation,
                        bool from_agent_inventory, bool owned_by_agent,
                        bool asset_known)
    {
        if (!over_entity_clone)
        {
            return VERDICT_PASS_THROUGH;
        }
        if (cargo_is_animation && from_agent_inventory && owned_by_agent && asset_known)
        {
            return VERDICT_PLAY_ON_CLONE;
        }
        return VERDICT_REFUSE;
    }

    ApplyPlan planApply(const std::vector<SelectedClone>& selected)
    {
        ApplyPlan plan;
        if (selected.empty())
        {
            plan.mNothingSelected = true;
            return plan;
        }
        for (const SelectedClone& clone : selected)
        {
            if (clone.second) plan.mPlay.push_back(clone.first);
        }
        if (plan.mPlay.empty())
        {
            plan.mSwitchToDirected = true;
            for (const SelectedClone& clone : selected)
            {
                plan.mPlay.push_back(clone.first);
            }
        }
        return plan;
    }

    std::string directedAnimEditorText(const LLUUID& asset_id, bool exposable,
                                       const std::string& name)
    {
        if (asset_id.isNull())
        {
            return std::string();
        }
        if (exposable)
        {
            return asset_id.asString();
        }
        if (hasDisplayName(name))
        {
            return name + " (inventory animation, UUID hidden)";
        }
        return "(inventory animation, UUID hidden)";
    }

    std::string hoverText(const std::string& anim_name, const std::string& clone_name)
    {
        return "Play " + anim_name + " on " + clone_name;
    }

    std::string playingToast(const std::string& anim_name, const std::string& clone_name)
    {
        return "Playing " + anim_name + " on " + clone_name;
    }

    std::string playingToast(const std::string& anim_name, size_t clone_count)
    {
        return "Playing " + anim_name + " on " + std::to_string(clone_count) +
               (clone_count == 1 ? " clone" : " clones");
    }

    const char* selectCloneFirstToast()
    {
        return "Select a clone first";
    }

    const char* menuDisabledReason()
    {
        return "Select at least one entity clone in Ghost Studio first";
    }

    std::string cloneLabel(const std::string& instance_name)
    {
        return hasDisplayName(instance_name) ? instance_name : std::string("the clone");
    }

    bool mentionsUuid(const std::string& text, const LLUUID& id)
    {
        if (id.isNull() || text.empty())
        {
            return false;
        }
        const std::string haystack = lower(text);
        const std::string needle = lower(id.asString());
        if (haystack.find(needle) != std::string::npos)
        {
            return true;
        }
        return stripDashes(haystack).find(stripDashes(needle)) != std::string::npos;
    }

    bool acceptWornEntry(bool is_animation, bool asset_known,
                         const std::string& name, WornNameTally& tally)
    {
        if (!is_animation)
        {
            return false;
        }
        if (!asset_known)
        {
            ++tally.mMasked;
            return false;
        }
        if (!hasDisplayName(name))
        {
            return false;
        }
        ++tally.mNamed;
        return true;
    }

    bool RestrictionLedger::note(const LLUUID& asset_id, bool exposable)
    {
        if (asset_id.isNull() || exposable)
        {
            return false;   // monotonic: an exposable sighting never un-hides
        }
        if (!mHidden.insert(asset_id).second)
        {
            return false;
        }
        ++mRevision;
        return true;
    }

    bool RestrictionLedger::hidden(const LLUUID& asset_id) const
    {
        return asset_id.notNull() && mHidden.find(asset_id) != mHidden.end();
    }

    bool clipboardValueAllowed(const LLSD& value, const RestrictionLedger& ledger)
    {
        if (value.isUUID())
        {
            return !ledger.hidden(value.asUUID());
        }
        if (value.isString())
        {
            const std::string text = value.asString();
            LLUUID exact;
            if (LLUUID::parseUUID(text, &exact) && ledger.hidden(exact))
            {
                return false;
            }
            for (const LLUUID& id : ledger.ids())
            {
                if (mentionsUuid(text, id)) return false;
            }
        }
        return true;
    }

    bool cloneInFront(bool real_pick, bool hud_pick, F32 clone_distance, F32 pick_distance)
    {
        if (hud_pick)
        {
            return false;
        }
        return !real_pick || clone_distance < pick_distance;
    }
}
