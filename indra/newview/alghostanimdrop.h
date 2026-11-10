/**
 * @file alghostanimdrop.h
 * @brief Pure rules for playing an inventory animation on Ghost Studio clones:
 *        3D drop-target classification, selection apply plans and the
 *        never-expose rule for non-full-perm asset UUIDs (TPV policy 2.a.i).
 *
 * No viewer dependencies: every decision takes plain inputs so the rules are
 * unit-testable (indra/newview/tests/alghostanimdrop_test.cpp).
 */
#ifndef AL_GHOSTANIMDROP_H
#define AL_GHOSTANIMDROP_H

#include "llsd.h"
#include "lluuid.h"

#include <set>
#include <string>
#include <utility>
#include <vector>

namespace ALGhostAnimDrop
{
    // What a 3D drag does over the thing under the cursor.
    enum EVerdict
    {
        VERDICT_PASS_THROUGH = 0,   // not over an entity clone: today's behaviour exactly
        VERDICT_PLAY_ON_CLONE,      // agent-owned inventory animation over an entity clone
        VERDICT_REFUSE              // over an entity clone, but nothing a clone can play
    };

    // over_entity_clone: the pick resolved to a live entity clone instance.
    // cargo_is_animation: the dragged cargo type is DAD_ANIMATION.
    // from_agent_inventory: the drag source is the agent's own inventory
    //   (SOURCE_AGENT), not a task/notecard/library source.
    // owned_by_agent: the (link-resolved) item's owner is the agent.
    // asset_known: the item's internal asset id is non-null.
    EVerdict classify3D(bool over_entity_clone, bool cargo_is_animation,
                        bool from_agent_inventory, bool owned_by_agent,
                        bool asset_known);

    // One selected entity clone: instance id and whether it is already Directed.
    typedef std::pair<LLUUID, bool> SelectedClone;

    struct ApplyPlan
    {
        std::vector<LLUUID> mPlay;      // instances that receive the animation
        bool mSwitchToDirected = false; // true when the plan switches drive modes
        bool mNothingSelected = false;  // toast "Select a clone first"
    };

    // Card / menu path: play on every selected Directed clone; when none of the
    // selected entity clones is Directed, switch all of them to Directed first.
    ApplyPlan planApply(const std::vector<SelectedClone>& selected);

    // ---- never-expose rule --------------------------------------------------
    // The asset UUID of a non-full-perm inventory item may be used to play but
    // never shown. exposable == item is full perm (modify+copy+transfer) or the
    // id is not backed by any of the agent's non-full-perm items at all.
    // Returns the text the "Anim UUID" editor may display.
    std::string directedAnimEditorText(const LLUUID& asset_id, bool exposable,
                                       const std::string& name);

    // UI strings. None of these ever contain an asset id.
    std::string hoverText(const std::string& anim_name, const std::string& clone_name);
    std::string playingToast(const std::string& anim_name, const std::string& clone_name);
    std::string playingToast(const std::string& anim_name, size_t clone_count);
    const char* selectCloneFirstToast();
    const char* menuDisabledReason();

    // Fallback label for a clone without a session name.
    std::string cloneLabel(const std::string& instance_name);

    // Test oracle for the never-expose rule: true when `text` carries `id` in
    // any case or with dashes stripped.
    bool mentionsUuid(const std::string& text, const LLUUID& id);

    // Worn-attachment names: which task inventory entries are usable. The
    // server may send a null asset id for entries it masks; those cannot be
    // matched and are counted, never guessed by name.
    struct WornNameTally
    {
        size_t mNamed = 0;   // animation entries with a usable asset id
        size_t mMasked = 0;  // animation entries whose asset id was null
    };
    // Returns true when the entry should be recorded.
    bool acceptWornEntry(bool is_animation, bool asset_known,
                         const std::string& name, WornNameTally& tally);

    // ---- session restriction ledger ------------------------------------------
    // Monotonic: once an asset id is known to be backed by a non-full-perm
    // item (own inventory, a link's target, a worn attachment's contents) it
    // stays hidden for the session, whatever a later entry says.
    class RestrictionLedger
    {
    public:
        // Returns true when the hidden state changed; revision() bumps then.
        bool note(const LLUUID& asset_id, bool exposable);
        bool hidden(const LLUUID& asset_id) const;
        U64 revision() const { return mRevision; }
        size_t size() const { return mHidden.size(); }
        const std::set<LLUUID>& ids() const { return mHidden; }
    private:
        std::set<LLUUID> mHidden;
        U64 mRevision = 0;
    };

    // Kit row "Copy value" (typed row clipboard + text clipboard): a value
    // that is, or mentions, a hidden asset id never leaves the viewer.
    bool clipboardValueAllowed(const LLSD& value, const RestrictionLedger& ledger);

    // 3D drop: the screen-space clone pick may override a real object /
    // avatar / land pick only when the clone probe is nearer the camera. A
    // HUD hit (HUD coordinates: its "distance" is meaningless) always wins.
    bool cloneInFront(bool real_pick, bool hud_pick, F32 clone_distance, F32 pick_distance);
}

#endif
