/**
 * @file alghostclearanim.h
 * @brief "Clear animations" on an entity clone: pure rules (header-only, no
 *        viewer or UI dependencies).
 *
 * A cleared clone is in Directed drive with no animation and has every motion
 * of its own stopped, so its skeleton rests in the neutral pose and the Poser
 * can start from there. It stays cleared until the user picks an animation or
 * another drive mode. While cleared, nothing the clone runs by itself (motion
 * cues, eye/blink, body physics, AutoAnimate) may start a motion again; only
 * the Poser's posing motion is allowed.
 */

#ifndef AL_GHOST_CLEAR_ANIM_H
#define AL_GHOST_CLEAR_ANIM_H

#include <cstddef>
#include <functional>
#include <memory>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace ALGhostClearAnim
{
    // Same index as ALGhostStudio::DRIVE_DIRECTED (static_assert in alghoststudio.cpp).
    inline constexpr int kDriveDirected = 1;

    // Who wants to start a motion on the clone's own controller.
    enum EOwner
    {
        OWNER_DRIVE,     // the drive itself: mirrored or directed animation
        OWNER_CUE,       // hover / jump / land motion cues
        OWNER_EYE,       // eye and blink motion (authored gaze opts it in)
        OWNER_PHYSICS,   // body physics motion
        OWNER_AUTOANIM,  // AutoAnimate assignments
        OWNER_POSER      // the Poser's private posing motion
    };

    // Cleared is only meaningful in Directed drive with no animation.
    inline bool canHold(int drive, bool has_directed_anim)
    {
        return drive == kDriveDirected && !has_directed_anim;
    }

    // Does a cleared clone stay cleared after its drive is set to (drive, anim)?
    // Picking an animation or another drive mode ends it.
    inline bool holds(bool cleared, int drive, bool has_directed_anim)
    {
        return cleared && canHold(drive, has_directed_anim);
    }

    // May this owner start a motion on the clone now?
    inline bool mayStart(bool cleared, EOwner owner)
    {
        return !cleared || owner == OWNER_POSER;
    }

    // Is a running motion stopped by Clear? Everything except the Poser's own.
    inline bool stopsOnClear(bool is_poser_motion)
    {
        return !is_poser_motion;
    }

    // Only entity clones with a live runtime body have animations to clear.
    inline bool eligible(bool is_entity_clone, bool has_live_body)
    {
        return is_entity_clone && has_live_body;
    }

    // Clear refuses a clone the Poser (or a Switchboard pose) is posing: the
    // posing motion's own targets would overwrite the neutral pose at once.
    // Release the Poser on the clone first.
    inline bool canClear(bool is_eligible, bool is_posed)
    {
        return is_eligible && !is_posed;
    }

    // Switchboard cuts and cleanups that nobody asked for (armed schedule, auto
    // cut, disarm cleanup) leave a cleared clone alone. An explicit manual punch
    // is the user picking an animation, so it ends the cleared state.
    inline bool mayAutomate(bool cleared, bool manual)
    {
        return manual || !cleared;
    }

    // The ordinary gaze layer, the Director look-at layer and Hot Gaze all paint
    // head / eye / torso joints; none may move a cleared clone.
    inline bool mayPaintGaze(bool cleared)
    {
        return !cleared;
    }

    // Neutral rest position of one joint: the default skeleton position plus the
    // applied body-shape offset, or the attachment joint-offset override when one
    // is active (the same winner LLJoint::updatePos picks).
    struct Vec3
    {
        float x = 0.f, y = 0.f, z = 0.f;
        bool operator==(const Vec3& o) const { return x == o.x && y == o.y && z == o.z; }
    };
    inline Vec3 restPosition(const Vec3& skeleton_default, const Vec3& shape_offset,
                             bool has_override, const Vec3& override_pos)
    {
        if (has_override)
        {
            return override_pos;
        }
        return Vec3{skeleton_default.x + shape_offset.x, skeleton_default.y + shape_offset.y,
                    skeleton_default.z + shape_offset.z};
    }

    // One held joint of a Frozen clone, keyed by joint NAME so a snapshot stays
    // valid across skeleton rebuilds and runtime body replacement.
    struct HeldJoint
    {
        std::string mName;
        float       mRot[4] = {0.f, 0.f, 0.f, 1.f};
        float       mPos[3] = {0.f, 0.f, 0.f};
    };
    typedef std::vector<HeldJoint> HeldPose;

    // The held pose is what the clone shows UNDER a transient Directed Mirror stamp (when one
    // is written), so undo hands the pose back and the normal stamp mirrors it exactly once.
    inline HeldJoint heldJoint(const std::string& name, const float displayed_rot[4],
                               const float displayed_pos[3], const float* under_rot,
                               const float* under_pos)
    {
        HeldJoint held;
        held.mName = name;
        for (int i = 0; i < 4; ++i) held.mRot[i] = under_rot ? under_rot[i] : displayed_rot[i];
        for (int i = 0; i < 3; ++i) held.mPos[i] = under_pos ? under_pos[i] : displayed_pos[i];
        return held;
    }

    // The authored Directed Mirror setting stays for later playback, but a cleared clone
    // is never stamped: the shape-aware neutral positions must not be mirrored.
    inline bool mayStampMirror(bool cleared)
    {
        return !cleared;
    }

    // Saved state (a parked Hot Gaze) owned by ONE Clear snapshot through a token. An undo
    // takes exactly its own token; a respawn migrates the owner; deletion drops it.
    // Token 0 is "nothing was parked".
    template <typename Owner, typename State>
    class ParkedStore
    {
    public:
        unsigned park(const Owner& owner, State state)
        {
            const unsigned token = mNext++;
            mEntries.emplace(token, Entry{owner, std::move(state)});
            return token;
        }
        // Moves the state out only for the same owner; the token is spent either way.
        bool take(unsigned token, const Owner& owner, State& out)
        {
            const auto it = mEntries.find(token);
            if (it == mEntries.end()) return false;
            const bool mine = it->second.mOwner == owner;
            if (mine) out = std::move(it->second.mState);
            mEntries.erase(it);
            return mine;
        }
        // new_owner == Owner() (null) deletes what old_owner parked.
        void migrate(const Owner& old_owner, const Owner& new_owner)
        {
            for (auto it = mEntries.begin(); it != mEntries.end();)
            {
                if (it->second.mOwner == old_owner)
                {
                    if (new_owner == Owner()) { it = mEntries.erase(it); continue; }
                    it->second.mOwner = new_owner;
                }
                ++it;
            }
        }
        void drop(const Owner& owner) { migrate(owner, Owner()); }
        // Token-specific disposal: a discarded snapshot removes exactly its own entry.
        bool discard(unsigned token) { return mEntries.erase(token) > 0; }
        void clear() { mEntries.clear(); }
        bool empty() const { return mEntries.empty(); }
        std::size_t size() const { return mEntries.size(); }
        template <typename Fn>
        void forEach(Fn&& fn) { for (auto& entry : mEntries) fn(entry.second.mState); }
    private:
        struct Entry { Owner mOwner; State mState; };
        std::map<unsigned, Entry> mEntries;
        unsigned mNext = 1;
    };

    // Shared RAII ownership of one parked token. Copies of a snapshot share one owner; when the
    // last copy is destroyed (history eviction, barrier, shell destruction, early return) the
    // token's parked state is disposed. Token 0 ("nothing parked") yields an empty owner.
    inline std::shared_ptr<unsigned> ownParkedToken(unsigned token, std::function<void(unsigned)> dispose)
    {
        if (!token) return nullptr;
        return std::shared_ptr<unsigned>(new unsigned(token), [dispose](unsigned* owned)
        {
            if (*owned && dispose) dispose(*owned);
            delete owned;
        });
    }
    inline unsigned parkedTokenValue(const std::shared_ptr<unsigned>& owner)
    {
        return owner ? *owner : 0u;
    }

    // Only a Frozen clone has a held pose that Clear would destroy.
    inline bool needsHeldPose(int drive, int frozen_drive)
    {
        return drive == frozen_drive;
    }

    // What one undo step puts back for one clone.
    template <typename Id>
    struct Before
    {
        Id   mId {};
        int  mDrive = 0;
        Id   mAnim {};
        bool mAutoAnimate = false;
        bool mGazeEnabled = false;
        bool mWasCleared = false;
    };

    // Undo must end the cleared state unless the clone was already cleared before.
    template <typename Id>
    inline bool clearedAfterUndo(const Before<Id>& before)
    {
        return before.mWasCleared && canHold(before.mDrive, !(before.mAnim == Id()));
    }

    inline std::string clearedToast(std::size_t count)
    {
        return "Cleared animations on " + std::to_string(count) + " clone(s)";
    }

    inline const char* overlayTooltip()
    {
        return "Overlay clones copy their source's pose and have no animations of their own to clear. "
               "Select an entity clone.";
    }

    inline const char* posedTooltip()
    {
        return "A selected clone is being posed (Poser or Switchboard pose). Release it there first: "
               "Clear animations cannot rest a pose that is being held.";
    }

    inline const char* buttonTooltip()
    {
        return "Stop every animation on the selected entity clones and rest them in the neutral pose, "
               "a clean start for the Poser. They stay still until you pick an animation or another Drive mode.";
    }
} // namespace ALGhostClearAnim

#endif // AL_GHOST_CLEAR_ANIM_H
