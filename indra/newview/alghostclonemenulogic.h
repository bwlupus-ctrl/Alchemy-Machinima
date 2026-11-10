/**
 * @file alghostclonemenulogic.h
 * @brief Pure (viewer-free) decisions for the clone right-click menus and
 *        the Director "Select" helper. Header-only, standard library only, so
 *        scripts/code_tools/director_clone_menu_check.py can compile and run
 *        it, with planted negatives, without a viewer build.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#ifndef AL_ALGHOSTCLONEMENULOGIC_H
#define AL_ALGHOSTCLONEMENULOGIC_H

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

namespace ALGhostCloneMenuLogic
{
// Accumulated pointer travel, per axis, that turns a right press into a drag.
// This is LLToolCamera's own test (lltoolfocus.cpp SLOP_RANGE = 4, mAccumX /
// mAccumY += abs(dx / dy), outside at >= SLOP_RANGE), so "this is a camera
// drag" and "this is no longer a click" can never disagree.
constexpr int RIGHT_CLICK_SLOP_PX = 4;

// Press / move / release of the right button. A menu opens only on a release
// that follows a press whose accumulated travel never reached the slop, so a
// drag that comes back to where it started is still a drag.
class RightClickGesture
{
public:
    void press(int x, int y)
    {
        mActive = true;
        mAccumX = 0;
        mAccumY = 0;
        mX = x;
        mY = y;
        mLastX = x;
        mLastY = y;
    }
    void move(int x, int y)
    {
        if (!mActive)
        {
            return;
        }
        mAccumX += x > mLastX ? x - mLastX : mLastX - x;
        mAccumY += y > mLastY ? y - mLastY : mLastY - y;
        mLastX = x;
        mLastY = y;
    }
    bool dragged() const
    {
        return mAccumX >= RIGHT_CLICK_SLOP_PX || mAccumY >= RIGHT_CLICK_SLOP_PX;
    }
    // True when the menu should open. Always ends the gesture.
    bool release(int x, int y)
    {
        move(x, y);
        const bool open = mActive && !dragged();
        mActive = false;
        return open;
    }
    void cancel() { mActive = false; }
    bool active() const { return mActive; }
    int pressX() const { return mX; }
    int pressY() const { return mY; }

private:
    bool mActive = false;
    int mAccumX = 0;
    int mAccumY = 0;
    int mX = 0;
    int mY = 0;
    int mLastX = 0;
    int mLastY = 0;
};

// Which clones a menu item acts on: the whole multi-selection when the picked
// clone belongs to it, otherwise only the picked clone (selected first).
template <class Id>
struct TargetPlan
{
    bool mSelectPickedFirst = false;
    std::vector<Id> mTargets;
};

template <class Id>
TargetPlan<Id> planTargets(const Id& picked, const std::vector<Id>& selection)
{
    TargetPlan<Id> plan;
    if (std::find(selection.begin(), selection.end(), picked) != selection.end())
    {
        plan.mTargets = selection;
        return plan;
    }
    plan.mSelectPickedFirst = true;
    plan.mTargets.push_back(picked);
    return plan;
}

// The validator the planted-negative tests aim at: every target a menu action
// touched must belong to the picked-or-selected set.
template <class Id>
bool targetsAllowed(const Id& picked, const std::vector<Id>& selection,
                    const std::vector<Id>& touched)
{
    const TargetPlan<Id> plan = planTargets(picked, selection);
    for (const Id& id : touched)
    {
        if (std::find(plan.mTargets.begin(), plan.mTargets.end(), id) == plan.mTargets.end())
        {
            return false;
        }
    }
    return !touched.empty();
}

// Ctrl-click add on a Clones list that may not have mirrored the shared
// selection yet (a hidden host does not draw): when the shared selection
// moved since the list last saw it, the list is REPLACED by the shared primary
// row first (the panel's own mirror rule), then the clicked row is added.
template <class Id>
std::vector<Id> additiveRows(const std::vector<Id>& list_rows, bool shared_moved,
                             const Id& shared_row, const Id& add_row, const Id& none)
{
    std::vector<Id> rows;
    if (shared_moved)
    {
        if (!(shared_row == none))
        {
            rows.push_back(shared_row);
        }
    }
    else
    {
        rows = list_rows;
    }
    if (std::find(rows.begin(), rows.end(), add_row) == rows.end())
    {
        rows.push_back(add_row);
    }
    return rows;
}

// Play/Pause is one toggle: all targets paused -> resume, otherwise pause.
inline bool pauseAction(std::size_t paused, std::size_t total)
{
    return !(total > 0 && paused == total);   // true = pause the targets
}

// Mirror (reflect) is checked only when every target reflects.
inline bool allOf(std::size_t matching, std::size_t total)
{
    return total > 0 && matching == total;
}

// The overlay submenu exists only on an avatar that carries a Studio overlay.
inline bool overlaySubmenuVisible(std::size_t overlay_count)
{
    return overlay_count > 0;
}

// The overlay the submenu acts on: the user's "Which overlay" pick while it is
// still on the avatar, else the topmost / most recent one (the last).
template <class Id>
Id chooseOverlay(const std::vector<Id>& overlays, const Id& chosen, const Id& none)
{
    if (overlays.empty())
    {
        return none;
    }
    if (std::find(overlays.begin(), overlays.end(), chosen) != overlays.end())
    {
        return chosen;
    }
    return overlays.back();
}

// Size steps (clone scale): multiplicative so a step feels the same at 0.1 and 10.
constexpr float SIZE_STEP = 1.25f;
inline float steppedScale(float scale, int direction, float lo, float hi)
{
    float next = scale;
    if (direction < 0) next = scale / SIZE_STEP;
    else if (direction > 0) next = scale * SIZE_STEP;
    else next = 1.f;
    return std::min(hi, std::max(lo, next));
}

// ---- Director "Select" ------------------------------------------------------
enum class SelectAction
{
    Refuse,
    AddToCastThenSetActor,   // avatar not in the Cast yet: existing add path, then the Actor bar
    SetActor,                // avatar already in the Cast
    CloneListReplace,        // Clones page: replace the clone selection
    CloneListAdd,            // Ctrl-click: add to the clone multi-selection
    CloneActor               // other page: the clone becomes the Actor bar actor
};

struct SelectInput
{
    bool mIsClone = false;
    bool mAdditive = false;       // Ctrl held
    bool mOnClonesPage = false;
    bool mInCast = false;
    bool mCastRefuses = false;    // the existing Cast add path would refuse
};

inline SelectAction planSelect(const SelectInput& in)
{
    if (in.mIsClone)
    {
        if (in.mAdditive) return SelectAction::CloneListAdd;
        return in.mOnClonesPage ? SelectAction::CloneListReplace : SelectAction::CloneActor;
    }
    // Avatars stay single: Ctrl on an avatar is a plain click.
    if (in.mInCast) return SelectAction::SetActor;
    return in.mCastRefuses ? SelectAction::Refuse : SelectAction::AddToCastThenSetActor;
}

// Pick mode lifecycle. Every exit the user can reach ends the mode.
class PickMode
{
public:
    enum class Exit { Esc, Button, DirectorClosed, ToolReplaced };
    void toggle() { mActive = !mActive; }
    void start() { mActive = true; }
    void exit(Exit) { mActive = false; }
    bool active() const { return mActive; }

private:
    bool mActive = false;
};

// Pick mode re-arm after a forwarded camera gesture (idle callback). The
// camera tool cleared the transient slot; the mode comes back only when this
// is still the same activation, the picker is not already installed, and
// nothing about the situation changed. EndMode is only ever answered while
// the picker is NOT installed, so the mode can never be switched off while its
// tool stays in the slot.
enum class RearmAction { Ignore, EndMode, Install };

inline RearmAction rearmDecision(bool active, bool same_activation, bool installed,
                                 bool director_visible, bool mouselook,
                                 bool base_unchanged, bool slot_taken_by_other)
{
    if (!active || !same_activation || installed)
    {
        return RearmAction::Ignore;
    }
    if (!director_visible || mouselook || !base_unchanged || slot_taken_by_other)
    {
        return RearmAction::EndMode;
    }
    return RearmAction::Install;
}

// Animation drop on a clone: a clone in a COLLAPSED (solid) group plays on
// every member of the group; an expanded group's member, or an ungrouped
// clone, plays on itself only.
template <class Id>
std::vector<Id> dropTargets(const Id& hit, bool in_collapsed_group, const std::vector<Id>& group_members)
{
    if (in_collapsed_group && !group_members.empty())
    {
        return group_members;
    }
    return std::vector<Id>{ hit };
}

// Stagger submenu: only when the menu's unit is a collapsed group.
inline bool staggerVisible(bool unit_is_collapsed_group)
{
    return unit_is_collapsed_group;
}

// Which Stagger item is checked: "In sync" while every member starts together,
// else the mode last applied to THIS group (1 Random, 2 Ripple), else none.
// Returns 0 In sync, 1 Random, 2 Ripple, -1 no check.
inline int staggerChecked(int last_mode_for_group, bool all_offsets_zero)
{
    if (all_offsets_zero)
    {
        return 0;
    }
    return last_mode_for_group == 1 || last_mode_for_group == 2 ? last_mode_for_group : -1;
}

// Ctrl-click joins the clone multi-selection; an avatar never joins a set.
inline bool ctrlAddsToSet(bool is_clone, bool ctrl)
{
    return is_clone && ctrl;
}

// ---- Look / turn, Animation and Appearance submenus -------------------------
// Dynamic lists (Nearby people, Recent animations, Swap source) show NAMES only.
// An id is carried by the item's callback, never by its label.

// Real avatars shown in a Look at / Turn to face "Nearby people" list.
constexpr std::size_t NEARBY_MAX = 16;
// Inventory animations remembered for the Animation > Recent list.
constexpr std::size_t RECENT_ANIM_MAX = 8;

inline bool isHexDigit(char c)
{
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

// True when the text carries a UUID: dashed (8-4-4-4-12) or 32 hex digits in a
// row. The guard every dynamic label passes before it is shown.
inline bool containsUuidText(const std::string& text)
{
    static const int GROUPS[5] = { 8, 4, 4, 4, 12 };
    for (std::size_t start = 0; start + 36 <= text.size(); ++start)
    {
        std::size_t p = start;
        bool ok = true;
        for (int g = 0; g < 5 && ok; ++g)
        {
            for (int k = 0; k < GROUPS[g]; ++k, ++p)
            {
                if (!isHexDigit(text[p])) { ok = false; break; }
            }
            if (ok && g < 4)
            {
                if (text[p] != '-') { ok = false; }
                else { ++p; }
            }
        }
        if (ok) return true;
    }
    std::size_t run = 0;
    for (char c : text)
    {
        run = isHexDigit(c) ? run + 1 : 0;
        if (run >= 32) return true;
    }
    return false;
}

// A nearby person's label: the display name, with the cast suffix for Director
// cast members. Empty (the person is left out) when there is no name or the
// "name" carries a UUID; a person is never listed by id.
inline std::string nearbyLabel(const std::string& name, bool cast, const std::string& cast_suffix)
{
    if (name.empty() || containsUuidText(name))
    {
        return std::string();
    }
    return cast && !cast_suffix.empty() ? name + " (" + cast_suffix + ")" : name;
}

// Nearest first (stable for equal distances), capped. Entry needs mDistance.
template <class Entry>
std::vector<Entry> nearbySelect(std::vector<Entry> entries, std::size_t cap = NEARBY_MAX)
{
    std::stable_sort(entries.begin(), entries.end(),
                     [](const Entry& a, const Entry& b) { return a.mDistance < b.mDistance; });
    if (entries.size() > cap)
    {
        entries.resize(cap);
    }
    return entries;
}

// Recent animations: newest first, one entry per animation (replaying moves it
// to the front), capped. Entry needs mAsset and mName.
template <class Entry, class Id>
void recentPush(std::vector<Entry>& list, const Id& asset, const std::string& name,
                std::size_t cap = RECENT_ANIM_MAX)
{
    list.erase(std::remove_if(list.begin(), list.end(),
                              [&asset](const Entry& e) { return e.mAsset == asset; }),
               list.end());
    Entry entry;
    entry.mAsset = asset;
    entry.mName = name;
    list.insert(list.begin(), entry);
    if (list.size() > cap)
    {
        list.resize(cap);
    }
}

// A recent animation's label: its name only. The fallback stands in when the
// name is empty or carries a UUID.
inline std::string recentLabel(const std::string& name, const std::string& fallback)
{
    return name.empty() || containsUuidText(name) ? fallback : name;
}

// The Animation > Recent submenu exists only while something was played.
inline bool recentSubmenuVisible(std::size_t count)
{
    return count > 0;
}

// The look / turn / animation actions touch exactly the picked-or-selected set
// (not a subset, not more): every target, and nothing else.
template <class Id>
bool targetsExact(const Id& picked, const std::vector<Id>& selection, const std::vector<Id>& touched)
{
    const TargetPlan<Id> plan = planTargets(picked, selection);
    if (touched.size() != plan.mTargets.size())
    {
        return false;
    }
    for (const Id& id : plan.mTargets)
    {
        if (std::find(touched.begin(), touched.end(), id) == touched.end())
        {
            return false;
        }
    }
    return true;
}

// ---- Fade out / Fade in (hidden-only: never deletes, session only) ----------------
// An entity clone takes Fade out while it is not user-hidden and Fade in while it is (an
// explicit state, never inferred from a mid-ramp coverage); an already-faded clone is skipped,
// and a menu with no eligible target disables the item.
inline bool fadeEligible(bool fade_out, bool hidden)
{
    return fade_out ? !hidden : hidden;
}

// Fade acts on ENTITY clones only: an overlay hidden from the menu would have no way back.
inline bool fadeApplies(bool entity, bool fade_out, bool hidden)
{
    return entity && fadeEligible(fade_out, hidden);
}

// ALGhostStudio::updateFades: a finished fade removes its clone ONLY when that clone was
// marked for deletion (requestRemoveInstance). A hidden-only fade ends at alpha 0 and stays.
inline bool fadeRemovesClone(bool removing, bool deleting)
{
    return removing && deleting;
}
} // namespace ALGhostCloneMenuLogic

#endif // AL_ALGHOSTCLONEMENULOGIC_H
