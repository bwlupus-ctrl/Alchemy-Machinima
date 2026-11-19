/**
 * @file aldirectoranimswitcher.h
 * @brief Viewer adapter for the Director's Animation Switchboard.
 *
 * A twelve-slot switchboard that punches a directed animation onto the current
 * scene subjects. Ghost clones are driven through their existing per-clone
 * directed-animation ledger (loop + speed); self (gAgentAvatarp) is driven
 * locally via startMotion with an optional per-instance priority override for
 * local capture. The renderer-independent schedule is the SAME domain-agnostic
 * ALDirectorSwitcherModel used by the camera switcher -- no new model.
 *
 * All settings default OFF: while disarmed the engine is dormant and applies
 * nothing to any avatar.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#ifndef AL_ALDIRECTORANIMSWITCHER_H
#define AL_ALDIRECTORANIMSWITCHER_H

#include "aldirectorswitchermodel.h"   // REUSED scheduler; no new model
#include "fsposeranimator.h"
#include "lluuid.h"
#include "stdtypes.h"

#include <set>
#include <string>
#include <vector>

class LLVOAvatar;

class ALDirectorAnimSwitcher
{
public:
    static constexpr S32 SLOT_COUNT =
        static_cast<S32>(ALDirectorSwitcherModel::SLOT_COUNT); // 12

    // Per-slot delivery target. CAST reproduces the spec's default "self +
    // ghosts from the cast (self when the cast is empty)" behavior; SELF and
    // GHOSTS narrow it. Real residents in the cast are still driven locally
    // only -- there is no TARGET_EVERYONE (server-authoritative, impossible).
    enum ETarget : S32
    {
        TARGET_CAST = 0,
        TARGET_SELF = 1,
        TARGET_GHOSTS = 2,
    };

    enum EKind : S32
    {
        KIND_ANIM = 0,
        KIND_POSE = 1,
    };

    struct Slot
    {
        bool        mEnabled   = true;   // auto eligibility; manual punch ignores it
        S32         mKind      = KIND_ANIM;
        LLUUID      mAnimID;             // animation asset UUID
        std::string mPoseName;            // local pose basename, without .xml
        S32         mPoseLoadMethod = ROT_POS_AND_SCALES;
        std::string mLabel;
        S32         mTarget    = TARGET_CAST; // who this slot punches
        S32         mPriority  = -1;     // -1 = asset default; 0..7 = LLJoint priority (self only)
        F32         mSpeed     = 1.f;    // 0.1..5.0 playback speed (ghosts only; self ignores)
        bool        mLoop      = true;   // LOOP_RETRIGGER vs LOOP_PLAY_ONCE (ghosts)
        bool        mSnapOnCut = false;  // true = stop_immediate; false = asset-default ease-out
    };

    static ALDirectorAnimSwitcher& instance();

    // Called once per viewer frame after presentation time is frozen, beside
    // the camera switcher tick. Inert while disarmed.
    void tick(F64 presentation_time);

    // Manual/auto cut. Armed-gated: returns true only when a real cut issued.
    bool punch(S32 slot);

    // Release the active program across every current target. Idempotent.
    void stopAll();

    S32  activeSlot() const { return mActiveSlot; }

    static std::vector<Slot> loadBank();
    static void              saveBank(const std::vector<Slot>& bank);

private:
    ALDirectorAnimSwitcher() = default;

    static std::vector<Slot> defaultBank();
    static void              sanitizeSlot(Slot& slot);
    static S32               sanitizeTarget(S32 target);
    static bool              slotHasPose(const Slot& slot);
    static ALDirectorSwitcherModel::Config readConfig(
        const std::vector<Slot>& bank);

    std::vector<LLVOAvatar*> getTargetAvatars(S32 target) const; // per ETarget
    bool canPoseAvatar(LLVOAvatar* av) const;
    void releasePoseFromAvatar(LLVOAvatar* av);
    void applySlotToAvatar(LLVOAvatar* av, const Slot& slot, const Slot& prev);
    bool applySlot(S32 slot, F64 now, const std::vector<Slot>& bank, bool manual);

    ALDirectorSwitcherModel::Controller mController;
    bool mWasArmed = false;
    S32  mActiveSlot = -1;
    Slot mActiveSlotConfig;
    FSPoserAnimator mPoseAnimator;
    std::set<LLUUID> mPosedAvatars;
};

#endif // AL_ALDIRECTORANIMSWITCHER_H
