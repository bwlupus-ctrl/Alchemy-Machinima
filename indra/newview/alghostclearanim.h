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
#include <string>
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

    inline const char* buttonTooltip()
    {
        return "Stop every animation on the selected entity clones and rest them in the neutral pose, "
               "a clean start for the Poser. They stay still until you pick an animation or another Drive mode.";
    }
} // namespace ALGhostClearAnim

#endif // AL_GHOST_CLEAR_ANIM_H
