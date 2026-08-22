/**
 * @file alcinelightrigmanager.cpp
 * @brief Five-slot owner and editing-buffer router for cinematic light rigs.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "alcinelightrigmanager.h"

#include "llagent.h"
#include "llagentcamera.h"
#include "lldirectorcast.h"
#include "llgl.h"
#include "llviewercontrol.h"
#include "llviewercamera.h"
#include "llviewerobject.h"
#include "llviewerobjectlist.h"
#include "llvoavatar.h"
#include "pipeline.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

namespace
{
using Slot = ALCineLightRigSlot;
using ParamBlob = ALCineLightRigParamBlob;
using namespace ALCineLightRigManagerModel;

constexpr char INSTANCE_SETTING[] = "CineLightRigInstances";
constexpr S32 INSTANCE_STORE_VERSION = 1;

bool validSlotValue(S32 value)
{
    return value >= 0 && value < static_cast<S32>(Slot::COUNT);
}

S32 slotIndex(Slot slot)
{
    return static_cast<S32>(slot);
}

LLUUID legacyAnchorForSlot(Slot slot)
{
    const LLDirectorCast& cast = LLDirectorCast::instance();
    switch (slot)
    {
        case Slot::A: return cast.getSubjectA();
        case Slot::B: return cast.getSubjectB();
        case Slot::C: return cast.getSubjectC();
        case Slot::D: return cast.getSubjectD();
        default: return LLUUID::null;
    }
}

bool blobMapWellFormed(const LLSD& data)
{
    static const char* const boolean_keys[] = {
        "CineLightRigEnabled", "CineLightRigScaleAware",
        "CineLightRigPower", "CineLightRigBounceEnabled",
        "CineLightRigMirror", "CineLightRigGizmo", "group"
    };
    static const char* const integer_keys[] = {
        "CineLightRigTrackMode", "CineLightRigFX",
        "CineLightRigShadowMode", "group_slots", "pending_fx_id", "slot"
    };
    static const char* const real_keys[] = {
        "CineLightRigRadius",
        "CineLightRigMasterEV", "CineLightRigMasterTempMired",
        "CineLightRigOffsetZ", "CineLightRigHeadroomStops",
        "CineLightRigBounceRatio",
        "CineLightRigTransitionSec", "CineLightRigDamping",
        "CineLightRigSeed",
        "CineLightRigOrbitYaw", "CineLightRigOrbitPitch",
        "fx_phase", "pending_fx_phase"
    };
    if (!data.isMap())
    {
        return false;
    }
    for (const char* key : boolean_keys)
    {
        if (!data[key].isBoolean())
        {
            return false;
        }
    }
    for (const char* key : integer_keys)
    {
        if (!data[key].isInteger())
        {
            return false;
        }
    }
    for (const char* key : real_keys)
    {
        if (!data[key].isReal() && !data[key].isInteger())
        {
            return false;
        }
    }
    // Additive fields were added to the already-shipped v1 instance
    // envelope. Missing means the feature's inert ParamBlob default, while a
    // present value must still have its declared type. This keeps existing v1
    // stores readable without weakening validation of newly written stores.
    if ((data.has("CineLightRigRatioLock") &&
         !data["CineLightRigRatioLock"].isBoolean()) ||
        (data.has("CineLightRigCatchlight") &&
         !data["CineLightRigCatchlight"].isBoolean()) ||
        (data.has("cue_list") &&
         (!data["cue_list"].isMap() ||
          !ALCineLightRig::validateCueListData(data["cue_list"]))))
    {
        return false;
    }
    static const char* const optional_real_keys[] = {
        "CineLightRigRatio", "CineLightRigCatchlightEV",
        "CineLightRigCatchlightSize", "CineLightRigCatchlightAngle"
    };
    for (const char* key : optional_real_keys)
    {
        if (data.has(key) && !data[key].isReal() && !data[key].isInteger())
        {
            return false;
        }
    }
    static const char* const optional_uuid_keys[] = {
        "CineLightRigObjectTarget"
    };
    for (const char* key : optional_uuid_keys)
    {
        if (data.has(key) && !data[key].isUUID())
        {
            return false;
        }
    }
    if (!data["CineLightRigCookieUUID"].isString() ||
        !data["anchor"].isUUID() ||
        !data["lights"].isArray() ||
        data["lights"].size() != ALCineLightRigModel::LIGHT_COUNT ||
        !data["shafts"].isArray() ||
        data["shafts"].size() != ALCineLightRigModel::LIGHT_COUNT ||
        !data["heroes"].isArray() ||
        data["heroes"].size() != ALCineLightRigModel::LIGHT_COUNT)
    {
        return false;
    }
    for (S32 i = 0; i < ALCineLightRigModel::LIGHT_COUNT; ++i)
    {
        const LLSD& light = data["lights"][i];
        if (!light.isMap() ||
            (!light["yaw"].isReal() && !light["yaw"].isInteger()) ||
            (!light["pitch"].isReal() && !light["pitch"].isInteger()) ||
            !light["profile"].isInteger() ||
            (!light["ev"].isReal() && !light["ev"].isInteger()) ||
            !light["beam"].isInteger() ||
            !light["gobo"].isInteger() ||
            (light.has("gel") && !light["gel"].isInteger()) ||
             (light.has("shadow_soft") &&
              !light["shadow_soft"].isReal() &&
              !light["shadow_soft"].isInteger()) ||
            (light.has("flicker_program") &&
             !light["flicker_program"].isInteger()) ||
            (light.has("flicker_amount") &&
             !light["flicker_amount"].isReal() &&
             !light["flicker_amount"].isInteger()) ||
            (light.has("fixture_mode") &&
             !light["fixture_mode"].isBoolean()) ||
            (light.has("kelvin") &&
             !light["kelvin"].isReal() &&
             !light["kelvin"].isInteger()) ||
            (light.has("source_size_m") &&
             !light["source_size_m"].isReal() &&
             !light["source_size_m"].isInteger()) ||
            (light.has("fixture_preset") &&
             !light["fixture_preset"].isInteger()) ||
            (light.has("fixture_gel_slots") &&
             (!light["fixture_gel_slots"].isArray() ||
              light["fixture_gel_slots"].size() !=
                  ALCineLightRigModel::FIXTURE_GEL_SLOT_COUNT)) ||
             !light["on"].isBoolean() ||
            !data["shafts"][i].isBoolean() ||
            !data["heroes"][i].isBoolean())
        {
            return false;
        }
        if (light.has("fixture_gel_slots"))
        {
            for (S32 slot = 0;
                 slot < ALCineLightRigModel::FIXTURE_GEL_SLOT_COUNT; ++slot)
            {
                if (!light["fixture_gel_slots"][slot].isInteger())
                {
                    return false;
                }
            }
        }
    }
    return true;
}

}

//static
ALCineLightRigParamBlob ALCineLightRigParamBlob::fromSettings(
    const ALCineLightRig* rig)
{
    ALCineLightRigParamBlob blob = fromSettingsStore(gSavedSettings);
    if (rig)
    {
        blob.captureRigState(*rig);
    }
    return blob;
}

void ALCineLightRigParamBlob::toSettings() const
{
    toSettingsStore(gSavedSettings);
}

void ALCineLightRigParamBlob::captureRigState(const ALCineLightRig& rig)
{
    mAnchor = rig.mAnchor;
    mObjectTarget = rig.mObjectTarget;
    mGroupEnabled = rig.mGroupEnabled;
    mGroupSlots = rig.mGroupSlots;
    for (S32 i = 0; i < ALCineLightRigModel::LIGHT_COUNT; ++i)
    {
        mShaftEnabled[i] = rig.mShaftEnabled[i];
        mHeroEnabled[i] = rig.mHeroEnabled[i];
    }
    if (rig.mActiveFX >= 0 && rig.mLastPresentationTime >= rig.mFXStart)
    {
        mFXPhase = rig.mLastPresentationTime - rig.mFXStart;
    }
    else if (rig.mPendingFXId == mFX && rig.mPendingFXPhase >= 0.0)
    {
        mFXPhase = rig.mPendingFXPhase;
    }
    else
    {
        mFXPhase = 0.0;
    }
    mPendingFXPhase = rig.mPendingFXPhase;
    mPendingFXId = rig.mPendingFXId;
    mCueList = rig.cueListData();
}

void ALCineLightRigParamBlob::applyRigState(ALCineLightRig& rig) const
{
    rig.setAnchor(mAnchor);
    rig.setObjectTarget(mObjectTarget);
    rig.setGroupEnabled(mGroupEnabled);
    rig.setGroupSlots(mGroupSlots);
    for (S32 i = 0; i < ALCineLightRigModel::LIGHT_COUNT; ++i)
    {
        rig.mShaftEnabled[i] = mShaftEnabled[i];
        rig.mHeroEnabled[i] = mHeroEnabled[i];
    }
    rig.mPendingFXPhase = mFX >= 0 ? std::max(0.0, mFXPhase)
                                   : mPendingFXPhase;
    rig.mPendingFXId = mFX >= 0 ? mFX : mPendingFXId;
    if (mCueList.isMap() && !rig.applyCueListData(mCueList))
    {
        LL_WARNS("CineLightRig")
            << "Ignoring malformed per-instance cue list" << LL_ENDL;
    }
    rig.mActiveFX = -1;
    rig.mHaveTarget = false;
    rig.mTransitionActive = false;
    rig.updateProjectorFlags();
}

ALCineLightRigManager::ALCineLightRigManager()
  : mInstances{
        {SLOT_SELF}, {SLOT_A}, {SLOT_B}, {SLOT_C}, {SLOT_D}}
{
    const ParamBlob baseline = ParamBlob::fromSettings(&mInstances[0]);
    mBlobs[0] = baseline;
    for (S32 i = 1; i < SLOT_COUNT; ++i)
    {
        mBlobs[i] = baseline;
        mBlobs[i].mEnabled = false;
        mBlobs[i].mAnchor.setNull();
        mBlobs[i].mObjectTarget.setNull();
        mBlobs[i].mGroupEnabled = false;
        mBlobs[i].mGroupSlots = 0;
        for (S32 light = 0; light < ALCineLightRigModel::LIGHT_COUNT; ++light)
        {
            mBlobs[i].mShaftEnabled[light] = false;
            mBlobs[i].mHeroEnabled[light] = false;
        }
    }
    if (!restore(gSavedSettings.getLLSD(INSTANCE_SETTING)))
    {
        mSelected = SLOT_SELF;
    }
}

//static
ALCineLightRigManager& ALCineLightRigManager::instance()
{
    static ALCineLightRigManager manager;
    return manager;
}

//static
bool ALCineLightRigManager::validSlot(Slot slot)
{
    return validSlotValue(slotIndex(slot));
}

ALCineLightRig& ALCineLightRigManager::selected()
{
    return at(mSelected);
}

const ALCineLightRig& ALCineLightRigManager::selected() const
{
    return at(mSelected);
}

ALCineLightRig& ALCineLightRigManager::at(Slot slot)
{
    return mInstances[validSlot(slot) ? slotIndex(slot) : 0];
}

const ALCineLightRig& ALCineLightRigManager::at(Slot slot) const
{
    return mInstances[validSlot(slot) ? slotIndex(slot) : 0];
}

ALCineLightRigManager::ParamBlob ALCineLightRigManager::captureSelected() const
{
    return ParamBlob::fromSettings(&selected());
}

void ALCineLightRigManager::setSelectedSlot(Slot slot)
{
    if (!validSlot(slot) || slot == mSelected)
    {
        return;
    }
    mBlobs[slotIndex(mSelected)] = captureSelected();
    mSelected = slot;
    mBlobs[slotIndex(mSelected)].toSettings();
    persist();
}

void ALCineLightRigManager::resetAllToSelf()
{
    // The panel has already reset all 63 live keys. Omitting the rig keeps
    // every session-only field at its shipped default as well.
    const ParamBlob defaults = ParamBlob::fromSettings();
    for (S32 i = 0; i < SLOT_COUNT; ++i)
    {
        mBlobs[i] = defaults;
    }
    mSelected = SLOT_SELF;
    mBlobs[slotIndex(mSelected)].toSettings();

    clearShadowSuppression();
    for (S32 i = 0; i < SLOT_COUNT; ++i)
    {
        mInstances[i].shutdown();
        mBlobs[i].applyRigState(mInstances[i]);
    }
    mFocusState.mFocus = SLOT_SELF;
    mFocusState.mChallenger = SLOT_SELF;
    mFocusState.mChallengeTicks = 0;
    mNeedsSessionRestore = false;
    persist();
}

bool ALCineLightRigManager::isSlotEnabled(Slot slot) const
{
    return validSlot(slot) && mBlobs[slotIndex(slot)].mEnabled;
}

bool ALCineLightRigManager::isSlotLit(Slot slot) const
{
    if (!isSlotEnabled(slot))
    {
        return false;
    }
    const ALCineLightRig& rig = at(slot);
    if (rig.getObjectTarget().notNull())
    {
        LLViewerObject* object =
            gObjectList.findObject(rig.getObjectTarget());
        return object && !object->isDead();
    }
    return rig.isGroupEnabled() ? rig.lastResolvedGroupSlots() != 0
                                : rig.resolveSlotAvatar() != nullptr;
}

U32 ALCineLightRigManager::enabledMask() const
{
    U32 mask = 0;
    for (S32 i = 0; i < SLOT_COUNT; ++i)
    {
        if (mBlobs[i].mEnabled)
        {
            mask |= 1u << i;
        }
    }
    return mask;
}

S32 ALCineLightRigManager::enabledCount() const
{
    return ALCineLightRigManagerModel::enabledCount(enabledMask());
}

U32 ALCineLightRigManager::requestedShadowSlots(U32 ceiling) const
{
    mBlobs[slotIndex(mSelected)] = captureSelected();
    return ALCineLightRigManagerModel::requestedShadowSlots(
        mBlobs, SLOT_COUNT, ceiling);
}

void ALCineLightRigManager::updateAutoShadowSlots()
{
    const bool enabled =
        gSavedSettings.getBOOL("CineLightRigAutoShadowSlots");
    const U32 hardware_max = LLPipeline::maxSpotShadowsForTextureUnits(
        gGLManager.mNumTextureImageUnits);
    const U32 requested = requestedShadowSlots(
        LLPipeline::MAX_SPOT_SHADOWS);
    const U32 current =
        gSavedSettings.getU32("BDMergeMaxSpotShadows");
    if (mAutoShadowInputsInitialized &&
        enabled == mLastAutoShadowEnabled &&
        requested == mLastAutoShadowRequest &&
        hardware_max == mLastAutoShadowHardwareMax)
    {
        // The input cache must not hide an intervening manual setting write.
        // Keep the captured baseline only while the live value is still ours.
        ALCineLightRigManagerModel::relinquishAutoShadowSlotsIfExternallyChanged(
            mAutoShadowSlotState, current);
        return;
    }

    const AutoShadowSlotUpdate update =
        ALCineLightRigManagerModel::updateAutoShadowSlots(
            mAutoShadowSlotState, enabled, requested, hardware_max, current);
    if (update.mWrite && update.mValue != current)
    {
        gSavedSettings.setU32("BDMergeMaxSpotShadows", update.mValue);
    }
    mLastAutoShadowEnabled = enabled;
    mLastAutoShadowRequest = requested;
    mLastAutoShadowHardwareMax = hardware_max;
    mAutoShadowInputsInitialized = true;
}

void ALCineLightRigManager::restoreAutoShadowSlots()
{
    const U32 current =
        gSavedSettings.getU32("BDMergeMaxSpotShadows");
    const AutoShadowSlotUpdate update =
        ALCineLightRigManagerModel::updateAutoShadowSlots(
            mAutoShadowSlotState, false, 0, 0, current);
    if (update.mWrite && update.mValue != current)
    {
        gSavedSettings.setU32("BDMergeMaxSpotShadows", update.mValue);
    }
    mAutoShadowInputsInitialized = false;
}

void ALCineLightRigManager::resolveCameraFocus()
{
    const U32 enabled_mask = enabledMask();
    if (!enabled_mask)
    {
        mFocusState.mChallengeTicks = 0;
        return;
    }

    FocusPoint points[SLOT_COUNT];
    for (S32 i = 0; i < SLOT_COUNT; ++i)
    {
        if (!(enabled_mask & (1u << i)))
        {
            continue;
        }
        if (LLVOAvatar* avatar = mInstances[i].resolveSlotAvatar())
        {
            const LLVector3d global = gAgent.getPosGlobalFromAgent(
                avatar->getRenderPosition());
            points[i].mX = global.mdV[VX];
            points[i].mY = global.mdV[VY];
            points[i].mZ = global.mdV[VZ];
            points[i].mValid = std::isfinite(points[i].mX) &&
                std::isfinite(points[i].mY) && std::isfinite(points[i].mZ);
        }
    }

    const LLVector3d focus_global = gAgentCamera.getFocusTargetGlobal();
    FocusPoint focus = { focus_global.mdV[VX], focus_global.mdV[VY],
                         focus_global.mdV[VZ], true };
    const LLVector3 at = LLViewerCamera::getInstance()->getAtAxis();
    FocusPoint axis = { at.mV[VX], at.mV[VY], at.mV[VZ], at.isFinite() };
    F64 raw_distance = 0.0;
    Slot raw = rawFocusSlot(points, enabled_mask, focus, axis, &raw_distance);
    const bool geometric_choice = raw != Slot::COUNT;
    if (!geometric_choice)
    {
        raw = (enabled_mask & slotToGroupBit(mSelected))
            ? mSelected : lowestEnabled(enabled_mask);
    }
    if (raw == Slot::COUNT)
    {
        return;
    }

    const U32 incumbent_bit = slotToGroupBit(mFocusState.mFocus);
    if (!(enabled_mask & incumbent_bit) ||
        (!points[slotIndex(mFocusState.mFocus)].mValid && geometric_choice))
    {
        mFocusState.mFocus = raw;
        mFocusState.mChallenger = raw;
        mFocusState.mChallengeTicks = 0;
        return;
    }
    if (!geometric_choice)
    {
        if (raw != mFocusState.mFocus)
        {
            mFocusState.mFocus = raw;
        }
        mFocusState.mChallenger = raw;
        mFocusState.mChallengeTicks = 0;
        return;
    }

    const FocusPoint& incumbent = points[slotIndex(mFocusState.mFocus)];
    const F64 dx = incumbent.mX - focus.mX;
    const F64 dy = incumbent.mY - focus.mY;
    const F64 dz = incumbent.mZ - focus.mZ;
    const F64 incumbent_distance = std::sqrt(dx * dx + dy * dy + dz * dz);
    focusFilter(mFocusState, raw, raw_distance, incumbent_distance);
}

void ALCineLightRigManager::tick(F64 presentation_time)
{
    if (mNeedsSessionRestore)
    {
        for (S32 i = 0; i < SLOT_COUNT; ++i)
        {
            mBlobs[i].applyRigState(mInstances[i]);
        }
        mBlobs[slotIndex(mSelected)].toSettings();
        mNeedsSessionRestore = false;
    }
    mBlobs[slotIndex(mSelected)] = captureSelected();
    updateAutoShadowSlots();
    updateRandomCycle(presentation_time);
    resolveCameraFocus();
    const U32 enabled_mask = enabledMask();
    for (S32 i = 0; i < SLOT_COUNT; ++i)
    {
        const Slot slot = static_cast<Slot>(i);
        const bool owns_shadows = slot == mFocusState.mFocus;
        switch (pathFor(enabled_mask, mSelected, slot))
        {
            case TickPath::TICK_SELECTED_VERBATIM:
                mInstances[i].tickSelected(presentation_time, owns_shadows);
                break;
            case TickPath::TICK_FROM_BLOB:
                mInstances[i].tickFromBlob(
                    mBlobs[i], presentation_time, owns_shadows);
                mBlobs[i].captureRigState(mInstances[i]);
                break;
            case TickPath::SHUTDOWN:
            default:
                mInstances[i].shutdown();
                break;
        }
    }
    // No blanket non-focus suppression: each enabled rig casts per its own
    // ShadowMode and the pipeline caps the total at MAX_SPOT_SHADOWS(10) by
    // priority. Force-suppressing non-focus rigs starved multi-light shadow/
    // shaft setups (a suppressed projector loses its shadow slot AND its shaft).
    // applyShadowSuppression()/suppressedSlotMask() are retained (unused here)
    // for a future soft focus-priority bias.
}

void ALCineLightRigManager::updateRandomCycle(F64 presentation_time)
{
    static LLCachedControl<bool> random_cycle(
        gSavedSettings, "CineLightRigRandomCycle", false);
    static LLCachedControl<F32> random_interval(
        gSavedSettings, "CineLightRigRandomInterval", 20.f);

    if (!random_cycle)
    {
        mRandomCycleNextTime = -1.0;
        mRandomCycleLastTime = presentation_time;
        return;
    }

    if (!std::isfinite(presentation_time))
    {
        return;
    }

    const F64 interval = std::max(1.0, static_cast<F64>(random_interval));

    // (Re)arm the timer on enable, on first tick, or after a scrub backwards
    // so we never fire from a stale schedule. presentation_time is scrub-safe.
    if (mRandomCycleNextTime < 0.0 ||
        !std::isfinite(mRandomCycleNextTime) ||
        presentation_time < mRandomCycleLastTime)
    {
        mRandomCycleNextTime = presentation_time + interval;
        mRandomCycleLastTime = presentation_time;
        return;
    }

    if (presentation_time >= mRandomCycleNextTime)
    {
        ALCineLightRig& rig = selected();
        // Exclude decorations AND the currently-lit named setup. The rig's
        // loaded-setup name is updated on every loadSetup (manual or random),
        // so we never cross-fade to the setup that is already active, and a
        // pool holding only the active setup collapses to "no valid choice".
        const std::string& current = rig.loadedSetupName();
        std::vector<std::string> pool;
        for (const ALCineLightRig::SetupEntry& entry : rig.setupNamesGrouped())
        {
            if (!ALCineLightRig::isSetupDecorationName(entry.mName) &&
                entry.mName != current)
            {
                pool.push_back(entry.mName);
            }
        }
        if (!pool.empty())
        {
            static std::mt19937 rng(std::random_device{}());
            std::uniform_int_distribution<size_t> dist(0, pool.size() - 1);
            // Cross-fades automatically: loadSetup preserves the transition
            // target so the next tick eases to this setup.
            rig.loadSetup(pool[dist(rng)]);
        }
        // Re-arm even when nothing was eligible so we retry next interval
        // instead of reloading the already-active setup.
        mRandomCycleNextTime = presentation_time + interval;
    }

    mRandomCycleLastTime = presentation_time;
}

void ALCineLightRigManager::applyShadowSuppression()
{
    const U32 suppressed_mask = suppressedSlotMask(
        enabledMask(), mFocusState.mFocus);
    for (S32 i = 0; i < SLOT_COUNT; ++i)
    {
        if (!(suppressed_mask & slotToGroupBit(static_cast<Slot>(i))))
        {
            continue;
        }
        for (S32 light = 0; light < ALCineLightRigModel::LIGHT_COUNT; ++light)
        {
            const LLUUID id = mInstances[i].projectorId(light);
            if (id.notNull() && !LLPipeline::isProjectorNoShadow(id))
            {
                LLPipeline::toggleProjectorCastShadows(id);
            }
        }
    }
}

void ALCineLightRigManager::clearShadowSuppression()
{
    for (S32 i = 0; i < SLOT_COUNT; ++i)
    {
        for (S32 light = 0; light < ALCineLightRigModel::LIGHT_COUNT; ++light)
        {
            const LLUUID id = mInstances[i].projectorId(light);
            if (id.notNull() && LLPipeline::isProjectorNoShadow(id))
            {
                LLPipeline::toggleProjectorCastShadows(id);
            }
        }
    }
}

void ALCineLightRigManager::shutdown()
{
    restoreAutoShadowSlots();
    if (!mNeedsSessionRestore)
    {
        mBlobs[slotIndex(mSelected)] = captureSelected();
        for (S32 i = 0; i < SLOT_COUNT; ++i)
        {
            if (i != slotIndex(mSelected) && mBlobs[i].mEnabled)
            {
                mBlobs[i].captureRigState(mInstances[i]);
            }
        }
        persist();
    }
    clearShadowSuppression();
    for (ALCineLightRig& rig : mInstances)
    {
        rig.shutdown();
    }
    mNeedsSessionRestore = true;
}

void ALCineLightRigManager::renderGizmo() const
{
    selected().renderGizmo();
}

void ALCineLightRigManager::persist() const
{
    LLSD data = LLSD::emptyMap();
    data["version"] = INSTANCE_STORE_VERSION;
    data["selected_slot"] = slotIndex(mSelected);
    data["instances"] = LLSD::emptyArray();
    for (S32 i = 0; i < SLOT_COUNT; ++i)
    {
        LLSD entry = mBlobs[i].toLLSD();
        entry["slot"] = i;
        data["instances"].append(entry);
    }
    gSavedSettings.setLLSD(INSTANCE_SETTING, data);
}

bool ALCineLightRigManager::restore(const LLSD& data)
{
    if (!data.isMap() || !data["version"].isInteger() ||
        data["version"].asInteger() != INSTANCE_STORE_VERSION ||
        !data["selected_slot"].isInteger() ||
        !validSlotValue(data["selected_slot"].asInteger()) ||
        !data["instances"].isArray() ||
        data["instances"].size() != SLOT_COUNT)
    {
        return false;
    }
    ParamBlob restored[SLOT_COUNT];
    bool seen[SLOT_COUNT] = {};
    for (LLSD::array_const_iterator it = data["instances"].beginArray();
         it != data["instances"].endArray(); ++it)
    {
        if (!blobMapWellFormed(*it))
        {
            return false;
        }
        const S32 slot = (*it)["slot"].asInteger();
        if (!validSlotValue(slot) || seen[slot])
        {
            return false;
        }
        restored[slot] = ParamBlob::fromLLSD(*it);
        seen[slot] = true;
    }
    for (S32 i = 0; i < SLOT_COUNT; ++i)
    {
        if (!seen[i])
        {
            return false;
        }
    }
    for (S32 i = 0; i < SLOT_COUNT; ++i)
    {
        mBlobs[i] = restored[i];
        mBlobs[i].applyRigState(mInstances[i]);
    }
    mSelected = static_cast<Slot>(data["selected_slot"].asInteger());
    mSelected = normalizeSelected(enabledMask(), mSelected);
    if (!validSlot(mSelected))
    {
        mSelected = SLOT_SELF;
    }
    mBlobs[slotIndex(mSelected)].toSettings();
    mFocusState.mFocus = (enabledMask() & slotToGroupBit(mSelected))
        ? mSelected : lowestEnabled(enabledMask());
    if (!validSlot(mFocusState.mFocus))
    {
        mFocusState.mFocus = SLOT_SELF;
    }
    mFocusState.mChallenger = mFocusState.mFocus;
    mFocusState.mChallengeTicks = 0;
    return true;
}

LLSD ALCineLightRigManager::sceneData() const
{
    mBlobs[slotIndex(mSelected)] = captureSelected();
    for (S32 i = 0; i < SLOT_COUNT; ++i)
    {
        if (i != slotIndex(mSelected) && mBlobs[i].mEnabled)
        {
            mBlobs[i].captureRigState(mInstances[i]);
        }
    }
    LLSD data = selected().sceneData();
    if (mSelected != SLOT_SELF)
    {
        // The selected instance is the legacy single-rig fallback. Its normal
        // tick anchor is implicit, but an old viewer still needs the UUID.
        data["anchor"] = legacyAnchorForSlot(mSelected);
    }
    data["selected_slot"] = slotIndex(mSelected);
    data["instances"] = LLSD::emptyArray();
    for (S32 i = 0; i < SLOT_COUNT; ++i)
    {
        LLSD entry = mBlobs[i].toLLSD();
        entry["slot"] = i;
        data["instances"].append(entry);
    }
    persist();
    return data;
}

void ALCineLightRigManager::applySceneData(const LLSD& data)
{
    // This call owns the frozen no-block / unknown-version / v1 split. All
    // multi-instance reads remain additive and happen only after it returns.
    selected().applySceneData(data);
    if (!data.isMap() || !data["version"].isInteger() ||
        data["version"].asInteger() != 1)
    {
        return;
    }

    if (data["instances"].isArray() &&
        data["instances"].size() == SLOT_COUNT)
    {
        LLSD store = LLSD::emptyMap();
        store["version"] = INSTANCE_STORE_VERSION;
        store["selected_slot"] = data["selected_slot"];
        store["instances"] = data["instances"];
        if (!restore(store))
        {
            LL_WARNS("CineLightRig")
                << "Director scene has a malformed multi-instance light-rig "
                   "block; falling back to its legacy single-rig data"
                << LL_ENDL;
        }
        else
        {
            clearShadowSuppression();
            for (ALCineLightRig& rig : mInstances)
            {
                rig.shutdown();
            }
            for (S32 i = 0; i < SLOT_COUNT; ++i)
            {
                mBlobs[i].applyRigState(mInstances[i]);
            }
            mBlobs[slotIndex(mSelected)].toSettings();
            // Instance blobs own each list, while the legacy scene envelope
            // carries the selected console's live transport position.
            selected().applyCueSceneState(data);
            persist();
            return;
        }
    }

    // A missing array and any corrupt multi-instance array intentionally
    // degrade through this one old-scene migration path.
    migrateLegacyScene(data);
}

void ALCineLightRigManager::migrateLegacyScene(const LLSD& data)
{
    ParamBlob migrated = captureSelected();
    // Object targeting did not exist in the legacy single-rig scene block.
    migrated.mObjectTarget.setNull();
    const LLDirectorCast& cast = LLDirectorCast::instance();
    const LLUUID subjects[4] = {
        cast.getSubjectA(), cast.getSubjectB(),
        cast.getSubjectC(), cast.getSubjectD()
    };
    const MigrationSlot mapping = migrateAnchorToSlot(
        data.has("anchor") ? data["anchor"].asUUID() : LLUUID::null,
        subjects);

    clearShadowSuppression();
    for (ALCineLightRig& rig : mInstances)
    {
        rig.shutdown();
    }
    for (S32 i = 0; i < SLOT_COUNT; ++i)
    {
        mBlobs[i] = ParamBlob();
        mBlobs[i].mEnabled = false;
    }
    mBlobs[slotIndex(mapping.mSlot)] = migrated;
    if (!mapping.mKeepLegacyAnchor)
    {
        // Known subject slots are implicit; retain the field only as the
        // legacy block's carrier, never as their tick anchor.
        mBlobs[slotIndex(mapping.mSlot)].mAnchor =
            data.has("anchor") ? data["anchor"].asUUID() : LLUUID::null;
    }
    for (S32 i = 0; i < SLOT_COUNT; ++i)
    {
        mBlobs[i].applyRigState(mInstances[i]);
    }
    mSelected = mapping.mSlot;
    mSelected = normalizeSelected(enabledMask(), mSelected);
    mBlobs[slotIndex(mSelected)].toSettings();
    mFocusState.mFocus = mSelected;
    mFocusState.mChallenger = mSelected;
    mFocusState.mChallengeTicks = 0;
    persist();
}
