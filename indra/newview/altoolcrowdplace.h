/**
 * @file altoolcrowdplace.h
 * @brief Persistent in-world interaction tool for Ghost Studio crowd drafts.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 *
 * Crowd placement is a base tool, not a transient one.  This matters because
 * Alt camera navigation must temporarily override the tool and then return to
 * the same placement session.  Only a middle-button height drag takes mouse
 * capture; every other gesture remains ordinary world input.
 */

#ifndef AL_ALTOOLCROWDPLACE_H
#define AL_ALTOOLCROWDPLACE_H

#include "alghostinteractionstate.h"
#include "llsingleton.h"
#include "lltool.h"
#include "lluuid.h"

#include <string>

class LLToolset;

class ALToolCrowdPlace final
    : public LLTool,
      public LLSingleton<ALToolCrowdPlace>
{
    LLSINGLETON(ALToolCrowdPlace);

public:
    bool handleMouseDown(S32 x, S32 y, MASK mask) override;
    bool handleMiddleMouseDown(S32 x, S32 y, MASK mask) override;
    bool handleMiddleMouseUp(S32 x, S32 y, MASK mask) override;
    bool handleHover(S32 x, S32 y, MASK mask) override;
    bool handleScrollWheel(S32 x, S32 y, LLScrollDelta delta) override;
    bool handleRightMouseDown(S32 x, S32 y, MASK mask) override;
    bool handleKey(KEY key, MASK mask) override;

    void handleSelect() override;
    void handleDeselect() override;
    void onMouseCaptureLost() override;
    void draw() override;
    LLTool* getOverrideTool(MASK mask) override;

    // The draft must already exist.  Its owner/session pair is deliberately
    // supplied by the panel, so a late callback can never take over a newer
    // draft belonging to the other Ghost Studio host.
    bool armFor(const LLUUID& owner_id, const LLUUID& session_id);

    // Used by panel buttons as well as lifecycle cleanup.  Cancellation is
    // scoped to the armed owner/session and therefore cannot erase a newer
    // draft that replaced this tool's session.
    void stopPlacement(bool cancel_draft = true, bool restore_tool = true);
    bool unpin();
    bool commitNow();

    bool isActive() const;
    const LLUUID& armedOwner() const { return mOwnerId; }
    const LLUUID& armedSession() const { return mSessionId; }
    const std::string& lastMessage() const { return mLastMessage; }

private:
    struct ScreenHit
    {
        bool mPreview = false;
        bool mAnchor = false;
    };

    bool sessionMatchesDraft() const;
    bool ensureLiveSession();
    bool groundPointAt(S32 x, S32 y, LLVector3d& out_global) const;
    bool projectedNear(const LLVector3d& point_global, S32 x, S32 y,
                       F32 radius_pixels) const;
    ScreenHit screenHitAt(S32 x, S32 y) const;

    ALGhostInteractionState::EventContext eventContext(
        S32 x, S32 y, MASK mask) const;
    bool reduceEvent(const ALGhostInteractionState::Event& event,
                     bool restore_on_terminal = true);
    void restoreReducerAfterFailedCommit(bool pinned);
    void restoreSavedTool();

    ALGhostInteractionState::State mState;
    ALGhostInteractionState::Config mConfig;
    LLUUID mOwnerId;
    LLUUID mSessionId;

    LLToolset* mSavedToolset = nullptr;
    LLTool* mSavedBaseTool = nullptr;
    U64 mWheelSequence = 0;
    bool mStopping = false;
    bool mRestorePending = false;
    bool mIgnoreCaptureLoss = false;
    bool mLastCommitSucceeded = false;
    std::string mLastMessage;
};

#endif // AL_ALTOOLCROWDPLACE_H
