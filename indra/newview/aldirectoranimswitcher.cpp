/**
 * @file aldirectoranimswitcher.cpp
 * @brief Viewer adapter for the Director's Animation Switchboard.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "aldirectoranimswitcher.h"

#include "alghoststudio.h"      // ALGhostStudio::DRIVE_DIRECTED / LOOP_*
#include "lldirectorcast.h"
#include "llghostavatar.h"
#include "lljoint.h"            // LLJoint priority range
#include "llmath.h"
#include "llmotion.h"
#include "llpresentationtime.h"
#include "llstring.h"
#include "llviewercontrol.h"
#include "llvoavatar.h"
#include "llvoavatarself.h"     // gAgentAvatarp

#include <cmath>
#include <cstddef>

namespace
{
constexpr S32 BANK_VERSION = 1;
constexpr F32 SPEED_MIN = 0.1f;
constexpr F32 SPEED_MAX = 5.f;

const char* const DEFAULT_LABELS[ALDirectorAnimSwitcher::SLOT_COUNT] = {
    "Anim 1", "Anim 2", "Anim 3", "Anim 4", "Anim 5", "Anim 6",
    "Anim 7", "Anim 8", "Anim 9", "Anim 10", "Anim 11", "Anim 12",
};

F64 presentation_now()
{
    return LLPresentationTime::currentFrame().presentation_time;
}

F32 finite_clamp(F32 value, F32 fallback, F32 minimum, F32 maximum)
{
    return std::isfinite(value) ? llclamp(value, minimum, maximum) : fallback;
}
} // anonymous namespace

//static
ALDirectorAnimSwitcher& ALDirectorAnimSwitcher::instance()
{
    static ALDirectorAnimSwitcher sInstance;
    return sInstance;
}

//static
std::vector<ALDirectorAnimSwitcher::Slot> ALDirectorAnimSwitcher::defaultBank()
{
    std::vector<Slot> bank(SLOT_COUNT);
    for (S32 i = 0; i < SLOT_COUNT; ++i)
    {
        bank[i].mEnabled = true;
        bank[i].mLabel = DEFAULT_LABELS[i];
    }
    return bank;
}

//static
S32 ALDirectorAnimSwitcher::sanitizeTarget(S32 target)
{
    return (target >= TARGET_CAST && target <= TARGET_GHOSTS)
        ? target : TARGET_CAST;
}

//static
void ALDirectorAnimSwitcher::sanitizeSlot(Slot& slot)
{
    slot.mLabel = utf8str_symbol_truncate(slot.mLabel, 40);
    slot.mTarget = sanitizeTarget(slot.mTarget);
    // Valid priorities are the contiguous range {-1} u [0,7]; -1 keeps the
    // asset-authored priority (LLJoint::USE_MOTION_PRIORITY).
    slot.mPriority = llclamp(slot.mPriority, -1, (S32)LL_CHARACTER_MAX_PRIORITY);
    slot.mSpeed = finite_clamp(slot.mSpeed, 1.f, SPEED_MIN, SPEED_MAX);
}

//static
std::vector<ALDirectorAnimSwitcher::Slot> ALDirectorAnimSwitcher::loadBank()
{
    std::vector<Slot> bank = defaultBank();
    const LLSD data = gSavedSettings.getLLSD("DirectorAnimSwitcherBank");
    if (!data.isMap() ||
        data["version"].asInteger() != BANK_VERSION ||
        !data["slots"].isArray())
    {
        return bank;
    }

    const LLSD& slots = data["slots"];
    const S32 count = llmin((S32)slots.size(), SLOT_COUNT);
    for (S32 i = 0; i < count; ++i)
    {
        const LLSD& item = slots[i];
        if (!item.isMap())
        {
            continue;
        }
        if (item.has("enabled"))
        {
            bank[i].mEnabled = item["enabled"].asBoolean();
        }
        if (item.has("anim"))
        {
            bank[i].mAnimID = item["anim"].asUUID();
        }
        if (item["label"].isString())
        {
            bank[i].mLabel =
                utf8str_symbol_truncate(item["label"].asString(), 40);
        }
        if (item.has("target"))
        {
            bank[i].mTarget = sanitizeTarget(item["target"].asInteger());
        }
        if (item.has("priority"))
        {
            bank[i].mPriority = item["priority"].asInteger();
        }
        if (item.has("speed"))
        {
            bank[i].mSpeed = static_cast<F32>(item["speed"].asReal());
        }
        if (item.has("loop"))
        {
            bank[i].mLoop = item["loop"].asBoolean();
        }
        if (item.has("snap"))
        {
            bank[i].mSnapOnCut = item["snap"].asBoolean();
        }
        sanitizeSlot(bank[i]);
    }
    return bank;
}

//static
void ALDirectorAnimSwitcher::saveBank(const std::vector<Slot>& input)
{
    std::vector<Slot> bank = defaultBank();
    const S32 count = llmin((S32)input.size(), SLOT_COUNT);
    for (S32 i = 0; i < count; ++i)
    {
        bank[i] = input[i];
        sanitizeSlot(bank[i]);
    }

    LLSD data = LLSD::emptyMap();
    data["version"] = BANK_VERSION;
    data["slots"] = LLSD::emptyArray();
    for (const Slot& slot : bank)
    {
        LLSD item = LLSD::emptyMap();
        item["enabled"] = slot.mEnabled;
        item["anim"] = slot.mAnimID;
        item["label"] = slot.mLabel;
        item["target"] = slot.mTarget;
        item["priority"] = slot.mPriority;
        item["speed"] = (F64)slot.mSpeed;
        item["loop"] = slot.mLoop;
        item["snap"] = slot.mSnapOnCut;
        data["slots"].append(item);
    }
    gSavedSettings.setLLSD("DirectorAnimSwitcherBank", data);
}

//static
ALDirectorSwitcherModel::Config ALDirectorAnimSwitcher::readConfig(
    const std::vector<Slot>& bank)
{
    ALDirectorSwitcherModel::Config config;
    config.mAuto = gSavedSettings.getBOOL("DirectorAnimSwitcherAuto");
    config.mSequence = gSavedSettings.getBOOL("DirectorAnimSwitcherSequence");
    config.mIntervalSeconds =
        gSavedSettings.getF32("DirectorAnimSwitcherIntervalSec");
    config.mJitterSeconds =
        gSavedSettings.getF32("DirectorAnimSwitcherJitterSec");
    config.mSeed =
        static_cast<U64>(gSavedSettings.getU32("DirectorAnimSwitcherSeed"));
    for (S32 i = 0; i < SLOT_COUNT; ++i)
    {
        config.mEnabled[i] =
            i < (S32)bank.size() ? bank[i].mEnabled : false;
    }
    return config;
}

std::vector<LLVOAvatar*> ALDirectorAnimSwitcher::getTargetAvatars(
    S32 target) const
{
    std::vector<LLVOAvatar*> targets;

    if (target == TARGET_SELF)
    {
        if (gAgentAvatarp && !gAgentAvatarp->isDead())
        {
            targets.push_back(gAgentAvatarp.get());
        }
        return targets;
    }

    LLDirectorCast& cast = LLDirectorCast::instance();
    // getIds() is the uuid_vec_t; resolve() maps a null id -> my avatar and a
    // stale id -> nullptr, and handles control avatars. Never raw gObjectList.
    for (const LLUUID& id : cast.getIds())
    {
        LLVOAvatar* av = cast.resolve(id);
        if (!av || av->isDead())
        {
            continue;
        }
        if (target == TARGET_GHOSTS && !dynamic_cast<LLGhostAvatar*>(av))
        {
            continue;
        }
        targets.push_back(av);
    }
    // TARGET_CAST falls back to self when the cast is empty, exactly as the
    // camera switcher's chain does. GHOSTS never falls back (no ghost = no-op).
    if (target == TARGET_CAST && targets.empty() &&
        gAgentAvatarp && !gAgentAvatarp->isDead())
    {
        targets.push_back(gAgentAvatarp.get());
    }
    return targets;
}

void ALDirectorAnimSwitcher::applySlotToAvatar(
    LLVOAvatar* av, const Slot& slot, const Slot& prev)
{
    if (!av || av->isDead())
    {
        return;
    }

    if (LLGhostAvatar* ghost = dynamic_cast<LLGhostAvatar*>(av))
    {
        // GHOST: drive through the per-clone ledger (correct, animesh-aware).
        if (slot.mAnimID.notNull())
        {
            ghost->setEntityLoopMode(slot.mLoop ? ALGhostStudio::LOOP_RETRIGGER
                                                : ALGhostStudio::LOOP_PLAY_ONCE);
            ghost->setEntityAnimTimeFactor(
                llclamp(slot.mSpeed, SPEED_MIN, SPEED_MAX));
            ghost->setEntityDriveMode(
                ALGhostStudio::DRIVE_DIRECTED, slot.mAnimID);
        }
        else
        {
            ghost->setEntityDriveMode(ALGhostStudio::DRIVE_MIRROR, LLUUID::null);
        }
        return;
    }

    // SELF (or a real avatar the cast resolved -- local-only, for capture):
    if (prev.mAnimID.notNull() && av->isMotionActive(prev.mAnimID))
    {
        av->stopMotion(prev.mAnimID, prev.mSnapOnCut); // false = asset ease-out
    }
    if (slot.mAnimID.notNull())
    {
        av->startMotion(slot.mAnimID, 0.f);
        if (slot.mPriority >= LLJoint::LOW_PRIORITY &&
            slot.mPriority <= LL_CHARACTER_MAX_PRIORITY)
        {
            if (LLMotion* motion = av->findMotion(slot.mAnimID))
            {
                motion->setPriorityOverride(slot.mPriority);
            }
        }
        // No per-slot speed for self: LLCharacter::setAnimTimeFactor is
        // whole-avatar and would warp AO/walk/everything. See the spec table.
    }
}

bool ALDirectorAnimSwitcher::applySlot(
    S32 slot, F64 now, const std::vector<Slot>& bank, bool manual)
{
    if (slot < 0 || (std::size_t)slot >= bank.size())
    {
        return false;
    }
    if (manual)
    {
        mController.manualPunch(slot, now); // re-anchor the auto interval
    }
    const Slot& next = bank[slot];
    const Slot  prev = mActiveSlotConfig;
    for (LLVOAvatar* av : getTargetAvatars(next.mTarget))
    {
        applySlotToAvatar(av, next, prev);
    }
    mActiveSlot = slot;
    mActiveSlotConfig = next;
    return true;
}

void ALDirectorAnimSwitcher::tick(F64 presentation_time)
{
    static LLCachedControl<bool> armed(
        gSavedSettings, "DirectorAnimSwitcherArmed", false);
    if (!armed)
    {
        if (mWasArmed)
        {
            // Dormant on disarm: stop scheduling and forget the live program so
            // a fresh arm starts clean. Already-applied motions are left as-is.
            mController.reset();
            mWasArmed = false;
            mActiveSlot = -1;
            mActiveSlotConfig = Slot();
        }
        return;
    }

    if (!std::isfinite(presentation_time) || presentation_time < 0.0)
    {
        return;
    }

    const std::vector<Slot> bank = loadBank();
    if (!mWasArmed)
    {
        // Anchor the schedule at the arm boundary so a long disarmed gap cannot
        // replay as one huge forward jump into an immediate auto cut.
        mWasArmed = true;
        mController.rebase(presentation_time);
    }

    const ALDirectorSwitcherModel::Frame frame =
        mController.update(presentation_time, readConfig(bank));
    if (frame.mCut)
    {
        applySlot(frame.mSlot, presentation_time, bank, /*manual=*/false);
    }
}

bool ALDirectorAnimSwitcher::punch(S32 slot)
{
    if (slot < 0 || slot >= SLOT_COUNT ||
        !gSavedSettings.getBOOL("DirectorAnimSwitcherArmed")) // armed gate
    {
        return false;
    }

    const F64 now = presentation_now();
    if (!std::isfinite(now) || now < 0.0)
    {
        return false;
    }
    tick(now);
    return applySlot(slot, now, loadBank(), /*manual=*/true);
}

void ALDirectorAnimSwitcher::stopAll()
{
    const Slot released; // null anim -> ghosts revert to mirror, self stops
    for (LLVOAvatar* av : getTargetAvatars(mActiveSlotConfig.mTarget))
    {
        if (LLGhostAvatar* ghost = dynamic_cast<LLGhostAvatar*>(av))
        {
            ghost->setEntityDriveMode(ALGhostStudio::DRIVE_MIRROR, LLUUID::null);
        }
        else if (av && !av->isDead() &&
                 mActiveSlotConfig.mAnimID.notNull() &&
                 av->isMotionActive(mActiveSlotConfig.mAnimID))
        {
            av->stopMotion(mActiveSlotConfig.mAnimID,
                           mActiveSlotConfig.mSnapOnCut);
        }
    }
    mActiveSlot = -1;
    mActiveSlotConfig = released;
}
