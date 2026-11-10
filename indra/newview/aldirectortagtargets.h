/**
 * @file aldirectortagtargets.h
 * @brief Name-tag highlight and Director group labels.
 *
 * Two local, never-saved name-tag features share this resolver:
 *
 *  - PAGE-AWARE HIGHLIGHT. While the v2 Director is open, the tags of whoever the
 *    current page would act on are drawn with the theme accent as background and a
 *    readable text colour. The Director publishes that set (about 10 Hz, only while
 *    it is visible); name-tag code asks a cheap per-tag lookup per frame.
 *  - DIRECTOR GROUP LABEL. A cast member's Director group name replaces the group-title
 *    line of its name tag. Rebuilt only when the cast or group revision moves.
 *
 * The ALTagHighlight namespace is pure (no viewer state) so the rules can be compiled
 * and attacked by fixtures without a viewer build.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#ifndef AL_DIRECTOR_TAG_TARGETS_H
#define AL_DIRECTOR_TAG_TARGETS_H

#include "aldirectortheme.h"
#include "lluuid.h"
#include "v4color.h"

#include <algorithm>
#include <cstring>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace ALTagHighlight
{
    // What a Director page would act on. The page decides; this file never guesses
    // from widget contents.
    enum class PageTarget : U8
    {
        NONE,            // no actor target: nothing is highlighted
        ACTOR_BAR,       // the shared Actor bar's actor (Clear = the page's default: You)
        GAZE,            // the Gaze page's effective edit actors (actor, Director group, All)
        MOVE,            // the Move page's Walk/Stop targets (Everyone, pinned Who, roster selection)
        CLONES,          // the Director's own Clones panel selection (groups expand to members)
        MANAGED_GROUP,   // the Groups page's selected Director group
        LIGHT_RIG,       // whoever the rig being edited actually lights
        LIGHT_ACTORS,    // Light > Actors: the avatar list's multi-selection
        ROTO,            // Roto Ink: its subject / target set
        TRON,            // Tron: its subject and trail target sets
        SHADOWS          // Shadows: the follow-subject target
    };

    inline PageTarget pageTarget(const std::string& page_key)
    {
        struct Entry { const char* key; PageTarget target; };
        static const Entry kPages[] = {
            { "cast_group/move_tab", PageTarget::MOVE },
            { "cast_group/groups_tab", PageTarget::MANAGED_GROUP },
            { "cast_group/path_tab", PageTarget::ACTOR_BAR },
            { "cast_group/ghosts_tab", PageTarget::CLONES },
            { "performance_group/animate_tab", PageTarget::ACTOR_BAR },
            { "performance_group/gaze_tab", PageTarget::GAZE },
            { "performance_group/gaze_cues_tab", PageTarget::ACTOR_BAR },
            { "light_group/cine_light_rig_tab", PageTarget::LIGHT_RIG },
            { "light_group/light_rim_tab", PageTarget::LIGHT_RIG },
            { "light_group/actors_tab", PageTarget::LIGHT_ACTORS },
            { "look_group/stepped_tab", PageTarget::ACTOR_BAR },
            { "look_group/actor_style_tab", PageTarget::ACTOR_BAR },
            { "look_group/stylize_tab", PageTarget::ROTO },
            { "look_group/tron_tab", PageTarget::TRON },
            { "world_group/shadows_probes_tab", PageTarget::SHADOWS }
        };
        for (const Entry& entry : kPages)
        {
            if (page_key == entry.key)
            {
                return entry.target;
            }
        }
        return PageTarget::NONE;
    }

    inline constexpr F32 kMinTextContrast = 4.5f;
    // The Director publishes about every 100 ms; a missed beat window means it is closed,
    // hidden or stalled, and the highlight must not linger.
    inline constexpr F64 kHeartbeatWindow = 0.5;
    // ... or published within the last few frames, so one very long frame (teleport, rez
    // stall) cannot expire a Director that is still drawing.
    inline constexpr U32 kHeartbeatFrames = 3;
    // A renderer record (who Roto / Tron actually affected) is current only while the effect is
    // still rendering: not refreshed within this many frames means the effect is off = nobody.
    inline constexpr U32 kRecordFrames = 3;

    struct TagStyle
    {
        LLColor4 background = LLColor4(1.f, 1.f, 1.f, 1.f);
        LLColor4 text = LLColor4(0.f, 0.f, 0.f, 1.f);
    };

    // Text colour for a tag drawn on background: black or white, whichever reads better.
    // The better of the two is always at least 4.58:1 against any opaque colour.
    inline LLColor4 readableTextOn(const LLColor4& background)
    {
        const LLColor4 black(0.f, 0.f, 0.f, 1.f), white(1.f, 1.f, 1.f, 1.f);
        return ALDirectorTheme::contrastRatio(black, background) >=
               ALDirectorTheme::contrastRatio(white, background) ? black : white;
    }

    inline TagStyle tagStyleFor(const LLColor4& accent)
    {
        TagStyle style;
        style.background = accent;
        style.background.mV[VALPHA] = 1.f;
        style.text = readableTextOn(style.background);
        return style;
    }

    inline bool styleReadable(const TagStyle& style)
    {
        return ALDirectorTheme::contrastRatio(style.text, style.background) >= kMinTextContrast;
    }

    // Highlighted background opacity: fully opaque, fading only with the tag's own distance
    // and alpha fade. The text contrast guarantee is measured against the opaque accent, so
    // no ChatBubbleOpacity (or other cap) may thin it out.
    inline F32 highlightedBackgroundAlpha(F32 tag_fade)
    {
        return std::clamp(tag_fade, 0.f, 1.f);
    }

    inline bool heartbeatFresh(F64 now, F64 beat, U32 frame, U32 beat_frame)
    {
        if (beat <= 0.0)
        {
            return false;   // never published, or withdrawn
        }
        return (now >= beat && now - beat <= kHeartbeatWindow) ||
               static_cast<U32>(frame - beat_frame) <= kHeartbeatFrames;
    }

    inline bool recordFresh(bool recorded, U32 frame, U32 record_frame)
    {
        return recorded && static_cast<U32>(frame - record_frame) <= kRecordFrames;
    }

    // One decision for every tag, every frame: touch the tag only when its override
    // differs from what it should have. A tag that is not wanted never keeps one.
    enum class OverrideAction : U8 { KEEP, APPLY, CLEAR };

    inline OverrideAction overrideAction(bool wanted, bool has_override,
                                         U64 applied_stamp, U64 style_stamp)
    {
        if (!wanted)
        {
            return has_override ? OverrideAction::CLEAR : OverrideAction::KEEP;
        }
        return !has_override || applied_stamp != style_stamp ? OverrideAction::APPLY
                                                              : OverrideAction::KEEP;
    }

    // Sorted unique identity set. Cast, runtime and Studio ids may all appear: a lookup
    // by any identity of the same actor must hit.
    class IdSet
    {
    public:
        // Normalises ids (drops nulls, sorts, removes duplicates) and replaces the set.
        // True when the set changed. ids is left holding the previous contents.
        bool assign(std::vector<LLUUID>& ids)
        {
            ids.erase(std::remove_if(ids.begin(), ids.end(),
                [](const LLUUID& id) { return id.isNull(); }), ids.end());
            std::sort(ids.begin(), ids.end());
            ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
            if (ids == mIds)
            {
                return false;
            }
            mIds.swap(ids);
            return true;
        }
        bool clear()
        {
            if (mIds.empty())
            {
                return false;
            }
            mIds.clear();
            return true;
        }
        bool contains(const LLUUID& id) const
        {
            return !id.isNull() && std::binary_search(mIds.begin(), mIds.end(), id);
        }
        bool empty() const { return mIds.empty(); }
        size_t size() const { return mIds.size(); }

    private:
        std::vector<LLUUID> mIds;
    };

    // Director group name per cast identity. Rebuilt wholesale whenever the cast or
    // group revision moves, so a renamed, merged, undone or dissolved group can never
    // leave an old label behind.
    class GroupLabelCache
    {
    public:
        // visit(add) must call add(id, group_name) for every cast member; members with an
        // empty group name carry no label. Returns true when it rebuilt.
        template <typename Visit>
        bool refresh(U64 cast_revision, U64 group_revision, Visit visit)
        {
            if (mValid && cast_revision == mCastRevision && group_revision == mGroupRevision)
            {
                return false;
            }
            mLabels.clear();
            visit([this](const LLUUID& id, const std::string& group)
            {
                if (!id.isNull() && !group.empty())
                {
                    mLabels[id] = group;
                }
            });
            mCastRevision = cast_revision;
            mGroupRevision = group_revision;
            mValid = true;
            ++mRevision;
            return true;
        }
        // The cast no longer exists (or never did): no label may survive.
        bool invalidate()
        {
            const bool had = mValid || !mLabels.empty();
            mLabels.clear();
            mValid = false;
            if (had)
            {
                ++mRevision;
            }
            return had;
        }
        const std::string& label(const LLUUID& id) const
        {
            static const std::string none;
            const auto found = mLabels.find(id);
            return found == mLabels.end() ? none : found->second;
        }
        U64 revision() const { return mRevision; }

    private:
        std::map<LLUUID, std::string> mLabels;
        U64 mCastRevision = 0, mGroupRevision = 0, mRevision = 1;
        bool mValid = false;
    };
} // namespace ALTagHighlight

class ALDirectorTagTargets
{
public:
    static ALDirectorTagTargets& instance();

    // ---- Director side ----
    // The ids the open page acts on plus the live theme accent. Called about 10 Hz while
    // the owner Director is visible. ids is consumed (normalised and swapped). The newest
    // publisher becomes the owner; a withdraw from anyone else is ignored, so a v1 or a
    // second Director instance can never wipe the visible v2's highlight.
    void publish(const void* owner, std::vector<LLUUID>& ids, const LLColor4& accent);
    // The owner Director closed, minimized or hid: nothing is highlighted any more.
    void withdraw(const void* owner);
    // The owner is still drawing: refresh the heartbeat (every frame) without republishing ids.
    void touch(const void* owner);
    // One place for self: a null id (and the agent id) is You, animesh resolve to their root.
    static LLUUID canonicalId(const LLUUID& id);
    // Logged in, connected and not mid-teleport: the only time a highlight may show.
    static bool sessionAllowsHighlight();
    // Who a subject-driven look page (Roto Ink, Tron, Shadows) really acts on. Roto and Tron read
    // what the renderer recorded the last time the effect rendered (stale = nobody); Shadows reads
    // its own follow-subject setting. Appends avatar ids. Never runs any selection of its own.
    static void appendSubjectTargets(ALTagHighlight::PageTarget kind, std::vector<LLUUID>& out);

    // ---- name-tag side: cheap, no allocation ----
    // The "Highlight edited tags" setting.
    bool highlightEnabled() const;
    bool isHighlighted(const LLUUID& id);
    bool isHighlighted(const LLUUID& first, const LLUUID& second);
    const ALTagHighlight::TagStyle& style() const { return mStyle; }
    // True only while a published set is active (non-empty, setting on, session allowed, heartbeat
    // fresh) AND the override's style stamp is the current one. LLHUDNameTag draws an override only
    // while this holds, so a tag that missed its clear can never show a stale highlight.
    bool overrideActive(U64 stamp) const;
    // Changes whenever the colours do, so tags re-apply a live theme or Invert change.
    U64 styleStamp() const { return mStyleStamp; }

    // The Director group a cast identity belongs to, or "" (no group, or no Director cast).
    const std::string& groupLabel(const LLUUID& id);
    // First non-empty label of two identities (runtime id, then Studio id for a clone).
    const std::string& groupLabel(const LLUUID& first, const LLUUID& second);

private:
    void refreshGroups();

    ALTagHighlight::IdSet mIds;
    ALTagHighlight::TagStyle mStyle;
    U64 mStyleStamp = 1;
    F64 mBeat = 0.0;
    U32 mBeatFrame = 0;
    const void* mOwner = nullptr;
    ALTagHighlight::GroupLabelCache mGroups;
    U32 mGroupFrame = 0;
    bool mGroupFrameSet = false;
};

#endif // AL_DIRECTOR_TAG_TARGETS_H
