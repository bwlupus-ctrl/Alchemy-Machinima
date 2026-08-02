/**
 * @file aldirectorswitcher.cpp
 * @brief Viewer adapter for the deterministic Director camera switcher.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "aldirectorswitcher.h"

#include "alcameracurve.h"
#include "llcinematiccamera.h"
#include "llcontrol.h"
#include "lldirectorcast.h"
#include "llmath.h"
#include "llpresentationtime.h"
#include "llstring.h"
#include "llviewercontrol.h"
#include "llviewerjoystick.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr S32 BANK_VERSION = 1;
constexpr F32 CUSTOM_YAW_MIN = -180.f;
constexpr F32 CUSTOM_YAW_MAX = 180.f;
constexpr F32 CUSTOM_PITCH_MIN = -80.f;
constexpr F32 CUSTOM_PITCH_MAX = 80.f;
constexpr F32 CUSTOM_DISTANCE_MIN = 0.3f;
constexpr F32 CUSTOM_DISTANCE_MAX = 64.f;
constexpr F32 CUSTOM_HEIGHT_MIN = -10.f;
constexpr F32 CUSTOM_HEIGHT_MAX = 20.f;
constexpr F32 CUSTOM_FOV_MIN = 5.f;
constexpr F32 CUSTOM_FOV_MAX = 175.f;
constexpr F32 TEMPORAL_MAX_SCALE = 8.f;
constexpr F32 MAX_EASE_SECONDS = 10.f;

const S32 DEFAULT_MODES[ALDirectorSwitcher::SLOT_COUNT] = {
    LLCinematicCamera::MODE_STATIC_WIDE,
    LLCinematicCamera::MODE_STATIC_MEDIUM,
    LLCinematicCamera::MODE_STATIC_CLOSE,
    LLCinematicCamera::MODE_ECU_EYES,
    LLCinematicCamera::MODE_OTS,
    LLCinematicCamera::MODE_TWO_SHOT,
    LLCinematicCamera::MODE_STATIC_PROFILE_L,
    LLCinematicCamera::MODE_STATIC_PROFILE_R,
    LLCinematicCamera::MODE_STATIC_LOW,
    LLCinematicCamera::MODE_STATIC_HIGH,
    LLCinematicCamera::MODE_STATIC_FULL,
    LLCinematicCamera::MODE_ORBIT,
};

const char* const DEFAULT_LABELS[ALDirectorSwitcher::SLOT_COUNT] = {
    "Wide", "Medium", "Close", "Tight", "OTS", "Two-shot",
    "Profile L", "Profile R", "Low hero", "High angle", "Full", "Orbit",
};

F64 presentation_now()
{
    return LLPresentationTime::currentFrame().presentation_time;
}

F32 finite_clamp(F32 value, F32 fallback, F32 minimum, F32 maximum)
{
    return std::isfinite(value)
        ? llclamp(value, minimum, maximum)
        : fallback;
}

void latch_curve(const char* curve_key,
                 const char* x1_key, const char* y1_key,
                 const char* x2_key, const char* y2_key,
                 S32& curve_id, F32 bezier[4])
{
    curve_id = ALCameraCurve::sanitizeId(
        gSavedSettings.getS32(curve_key));
    bezier[0] = ALCameraCurve::sanitizeX(
        gSavedSettings.getF32(x1_key), 0.42f);
    bezier[1] = ALCameraCurve::sanitizeY(
        gSavedSettings.getF32(y1_key), 0.f);
    bezier[2] = ALCameraCurve::sanitizeX(
        gSavedSettings.getF32(x2_key), 0.58f);
    bezier[3] = ALCameraCurve::sanitizeY(
        gSavedSettings.getF32(y2_key), 1.f);
}

void set_transient_control(LLControlVariable* control, const LLSD& value)
{
    if (control)
    {
        // A cut effect is live-session state. Keep the operator's persisted
        // Temporal Capture / Freeze World preferences untouched.
        control->setValue(value, false);
    }
}
} // anonymous namespace

//static
ALDirectorSwitcher& ALDirectorSwitcher::instance()
{
    static ALDirectorSwitcher sInstance;
    return sInstance;
}

//static
std::vector<ALDirectorSwitcher::Slot> ALDirectorSwitcher::defaultBank()
{
    std::vector<Slot> bank(SLOT_COUNT);
    for (S32 i = 0; i < SLOT_COUNT; ++i)
    {
        bank[i].mEnabled = true;
        bank[i].mMode = DEFAULT_MODES[i];
        bank[i].mLabel = DEFAULT_LABELS[i];
    }
    return bank;
}

//static
S32 ALDirectorSwitcher::sanitizeMode(S32 mode)
{
    mode = LLCinematicCamera::migrateLegacyMode(mode);
    return (mode > LLCinematicCamera::MODE_OFF &&
            mode <= LLCinematicCamera::MODE_STATIC_FULL)
        ? mode : LLCinematicCamera::MODE_ORBIT;
}

//static
S32 ALDirectorSwitcher::sanitizePrimarySubject(S32 subject)
{
    return subject >= SUBJECT_DEFAULT && subject <= SUBJECT_D
        ? subject : SUBJECT_DEFAULT;
}

//static
S32 ALDirectorSwitcher::sanitizeSecondarySubject(S32 subject)
{
    return subject >= SUBJECT_A && subject <= SUBJECT_D
        ? subject : SUBJECT_B;
}

//static
void ALDirectorSwitcher::sanitizeSlot(Slot& slot)
{
    slot.mMode = sanitizeMode(slot.mMode);
    slot.mLabel = utf8str_symbol_truncate(slot.mLabel, 40);
    slot.mPrimarySubject = sanitizePrimarySubject(slot.mPrimarySubject);
    slot.mSecondarySubject =
        sanitizeSecondarySubject(slot.mSecondarySubject);
    slot.mCustomYawOffsetDeg = finite_clamp(
        slot.mCustomYawOffsetDeg, 0.f, CUSTOM_YAW_MIN, CUSTOM_YAW_MAX);
    slot.mCustomPitchDeg = finite_clamp(
        slot.mCustomPitchDeg, 0.f, CUSTOM_PITCH_MIN, CUSTOM_PITCH_MAX);
    slot.mCustomDistanceM = finite_clamp(
        slot.mCustomDistanceM, 3.f,
        CUSTOM_DISTANCE_MIN, CUSTOM_DISTANCE_MAX);
    slot.mCustomHeightM = finite_clamp(
        slot.mCustomHeightM, 1.3f, CUSTOM_HEIGHT_MIN, CUSTOM_HEIGHT_MAX);
    slot.mCustomFovDeg = finite_clamp(
        slot.mCustomFovDeg, 60.f, CUSTOM_FOV_MIN, CUSTOM_FOV_MAX);
}

//static
std::vector<ALDirectorSwitcher::Slot> ALDirectorSwitcher::loadBank()
{
    std::vector<Slot> bank = defaultBank();
    const LLSD data = gSavedSettings.getLLSD("DirectorSwitcherBank");
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
        if (item.has("mode"))
        {
            bank[i].mMode = sanitizeMode(item["mode"].asInteger());
        }
        if (item["label"].isString())
        {
            bank[i].mLabel =
                utf8str_symbol_truncate(item["label"].asString(), 40);
        }
        if (item.has("subject"))
        {
            bank[i].mPrimarySubject =
                sanitizePrimarySubject(item["subject"].asInteger());
        }
        if (item.has("subject2"))
        {
            bank[i].mSecondarySubject =
                sanitizeSecondarySubject(item["subject2"].asInteger());
        }
        if (item.has("custom_enable"))
        {
            bank[i].mCustomEnabled = item["custom_enable"].asBoolean();
        }
        if (item.has("yaw_offset_deg"))
        {
            bank[i].mCustomYawOffsetDeg =
                (F32)item["yaw_offset_deg"].asReal();
        }
        if (item.has("pitch_deg"))
        {
            bank[i].mCustomPitchDeg = (F32)item["pitch_deg"].asReal();
        }
        if (item.has("distance_m"))
        {
            bank[i].mCustomDistanceM = (F32)item["distance_m"].asReal();
        }
        if (item.has("height_m"))
        {
            bank[i].mCustomHeightM = (F32)item["height_m"].asReal();
        }
        if (item.has("fov"))
        {
            bank[i].mCustomFovDeg = (F32)item["fov"].asReal();
        }
        sanitizeSlot(bank[i]);
    }
    return bank;
}

//static
void ALDirectorSwitcher::saveBank(const std::vector<Slot>& input)
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
        item["mode"] = slot.mMode;
        item["label"] = slot.mLabel;
        item["subject"] = slot.mPrimarySubject;
        item["subject2"] = slot.mSecondarySubject;
        item["custom_enable"] = slot.mCustomEnabled;
        item["yaw_offset_deg"] = slot.mCustomYawOffsetDeg;
        item["pitch_deg"] = slot.mCustomPitchDeg;
        item["distance_m"] = slot.mCustomDistanceM;
        item["height_m"] = slot.mCustomHeightM;
        item["fov"] = slot.mCustomFovDeg;
        data["slots"].append(item);
    }
    gSavedSettings.setLLSD("DirectorSwitcherBank", data);
}

//static
ALDirectorSwitcherModel::Config ALDirectorSwitcher::readConfig(
    const std::vector<Slot>& bank)
{
    ALDirectorSwitcherModel::Config config;
    config.mAuto = gSavedSettings.getBOOL("DirectorSwitcherAuto");
    config.mSequence = gSavedSettings.getBOOL("DirectorSwitcherSequence");
    config.mIntervalSeconds =
        gSavedSettings.getF32("DirectorSwitcherIntervalSec");
    config.mJitterSeconds =
        gSavedSettings.getF32("DirectorSwitcherJitterSec");
    config.mSeed =
        static_cast<U64>(gSavedSettings.getU32("DirectorSwitcherSeed"));
    for (S32 i = 0; i < SLOT_COUNT; ++i)
    {
        config.mEnabled[i] =
            i < (S32)bank.size() ? bank[i].mEnabled : false;
    }
    return config;
}

void ALDirectorSwitcher::writeCameraEnabled(bool enabled)
{
    if (enabled && shouldYieldToFlycam())
    {
        return;
    }
    const bool current = gSavedSettings.getBOOL("CinematicCamEnabled");
    if (current == enabled)
    {
        return;
    }
    // Ownership is live-session state. An armed viewer exit or crash must not
    // serialize this temporary override as the operator's CineCam preference.
    if (LLControlVariable* control =
            gSavedSettings.getControl("CinematicCamEnabled"))
    {
        control->setValue(LLSD(enabled), false);
    }
    else
    {
        return;
    }
    mWroteEnable = true;
    mLastWrittenEnable = enabled;
}

bool ALDirectorSwitcher::shouldYieldToFlycam() const
{
    static LLCachedControl<bool> overrides_flycam(
        gSavedSettings, "DirectorSwitcherOverridesFlycam", true);
    return !overrides_flycam &&
           LLViewerJoystick::getInstance()->getOverrideCamera();
}

void ALDirectorSwitcher::enterArmed(
    F64 now, const std::vector<Slot>& bank)
{
    mHaveEnableSnapshot = true;
    const bool camera_enabled =
        gSavedSettings.getBOOL("CinematicCamEnabled");
    // ACTION can already own a temporary false->true enable when the operator
    // arms the switcher mid-take. Snapshot the value ACTION will restore, not
    // its temporary live value, so disarming after CUT cannot resurrect it.
    mEnableWasEnabled =
        LLDirectorCast::instance().ownsCameraEnable()
            ? false : camera_enabled;
    mWroteEnable = false;
    writeCameraEnabled(true);
    mWasCameraEnabled =
        gSavedSettings.getBOOL("CinematicCamEnabled");
    mYieldingToFlycam = shouldYieldToFlycam();

    mController.reset();
    mLastPresentationTime = now;
    S32 first = 0;
    for (S32 i = 0; i < SLOT_COUNT; ++i)
    {
        if (i < (S32)bank.size() && bank[i].mEnabled)
        {
            first = i;
            break;
        }
    }
    applySlot(first, now, now, bank, true);
}

void ALDirectorSwitcher::leaveArmed()
{
    restoreEaseWorldTime();

    if (mHaveEnableSnapshot && mWroteEnable &&
        !LLDirectorCast::instance().ownsCameraEnable() &&
        gSavedSettings.getBOOL("CinematicCamEnabled") ==
            mLastWrittenEnable)
    {
        if (LLControlVariable* control =
                gSavedSettings.getControl("CinematicCamEnabled"))
        {
            control->setValue(LLSD(mEnableWasEnabled), false);
        }
    }

    mController.reset();
    mWasArmed = false;
    mWasCameraEnabled = false;
    mHaveEnableSnapshot = false;
    mWroteEnable = false;
    mYieldingToFlycam = false;
    mActiveSlot = -1;
    mActiveMode = LLCinematicCamera::MODE_OFF;
    mActiveSlotConfig = Slot();
    mActiveSince = 0.0;
    mCutEaseSeconds = 0.f;
}

void ALDirectorSwitcher::startEaseWorldTime(F64 now)
{
    latch_curve(
        "DirectorSwitcherFreezeCurve",
        "DirectorSwitcherFreezeBezierX1",
        "DirectorSwitcherFreezeBezierY1",
        "DirectorSwitcherFreezeBezierX2",
        "DirectorSwitcherFreezeBezierY2",
        mFreezeCurveId, mFreezeBezier);
    mFreezeFeather = finite_clamp(
        gSavedSettings.getF32("DirectorSwitcherFreezeFeather"),
        0.f, 0.f, 1.f);
    if (!gSavedSettings.getBOOL("DirectorSwitcherArmed") ||
        !gSavedSettings.getBOOL("DirectorSwitcherEaseCuts") ||
        !gSavedSettings.getBOOL("DirectorSwitcherEaseFreeze") ||
        mCutEaseSeconds <= 0.f || !std::isfinite(now))
    {
        return;
    }

    const F64 cut_age = now - mActiveSince;
    if (cut_age < 0.0 || cut_age >= (F64)mCutEaseSeconds)
    {
        return; // the render camera also rejects this stale ease
    }

    mEaseTargetFactor = finite_clamp(
        gSavedSettings.getF32("DirectorSwitcherEaseTimeFactor"),
        0.15f, 0.f, 1.f);
    if (mEaseTargetFactor >= 1.f)
    {
        return; // explicitly normal speed: do not perturb Temporal state
    }

    LLControlVariable* mode = gSavedSettings.getControl("TemporalMode");
    LLControlVariable* scale =
        gSavedSettings.getControl("TemporalWorldScale");
    LLControlVariable* animation =
        gSavedSettings.getControl("TemporalDriveAnimation");
    LLControlVariable* objects =
        gSavedSettings.getControl("TemporalDriveObjects");
    LLControlVariable* texture_anim =
        gSavedSettings.getControl("TemporalDriveTextureAnim");
    LLControlVariable* particles =
        gSavedSettings.getControl("TemporalDriveParticles");
    if (!mode || !scale || !animation || !objects ||
        !texture_anim || !particles)
    {
        return; // fail soft when Temporal Capture is unavailable
    }

    mSavedTemporalMode = mode->getValue().asInteger();
    mSavedTemporalWorldScale = (F32)scale->getValue().asReal();
    mSavedDriveAnimation = animation->getValue().asBoolean();
    mSavedDriveObjects = objects->getValue().asBoolean();
    mSavedDriveTextureAnim = texture_anim->getValue().asBoolean();
    mSavedDriveParticles = particles->getValue().asBoolean();
    mEaseBaseWorldScale =
        mSavedTemporalMode == 1
            ? finite_clamp(mSavedTemporalWorldScale, 1.f,
                           0.f, TEMPORAL_MAX_SCALE)
            : 1.f;

    if (mEaseTargetFactor <= 0.f)
    {
        // Freeze World holds stock object/interpolation paths immediately.
        // Temporal Capture additionally owns all presentation drives at 0 so
        // LLPresentationTime, its sole writer, halts every motion controller.
        // The cut envelope uses mCutEaseTimer, not the now-paused presentation
        // clock, so this lease still reaches its guaranteed restore.
        LLControlVariable* gate =
            gSavedSettings.getControl("BDMergeFreezeWorld");
        LLControlVariable* freeze =
            gSavedSettings.getControl("UseFreezeWorld");
        LLControlVariable* freeze_time =
            gSavedSettings.getControl("FreezeTime");
        if (!gate || !freeze || !freeze_time)
        {
            return; // fail soft: the visual camera ease still runs
        }

        mSavedBDMergeFreezeWorld = gate->getValue().asBoolean();
        mSavedUseFreezeWorld = freeze->getValue().asBoolean();
        mSavedFreezeTime = freeze_time->getValue().asBoolean();
        if (mSavedUseFreezeWorld && !mSavedBDMergeFreezeWorld)
        {
            return; // inconsistent external state; do not assume ownership
        }

        mEaseWorldTimeActive = true;
        mEaseUsesFreezeWorld = true;
        applyEaseWorldScale(0.f);
        set_transient_control(gate, LLSD(true));
        set_transient_control(freeze, LLSD(true));
        set_transient_control(freeze_time, LLSD(true));
        return;
    }

    if (mEaseBaseWorldScale <= 0.f)
    {
        return; // respect an operator-owned manual freeze
    }

    mEaseWorldTimeActive = true;
    mEaseUsesFreezeWorld = false;
    updateEaseWorldTime(now);
}

void ALDirectorSwitcher::applyEaseWorldScale(F32 scale)
{
    LLControlVariable* world_scale =
        gSavedSettings.getControl("TemporalWorldScale");
    LLControlVariable* mode = gSavedSettings.getControl("TemporalMode");
    LLControlVariable* animation =
        gSavedSettings.getControl("TemporalDriveAnimation");
    LLControlVariable* objects =
        gSavedSettings.getControl("TemporalDriveObjects");
    LLControlVariable* texture_anim =
        gSavedSettings.getControl("TemporalDriveTextureAnim");
    LLControlVariable* particles =
        gSavedSettings.getControl("TemporalDriveParticles");
    if (!world_scale || !mode || !animation || !objects ||
        !texture_anim || !particles)
    {
        restoreEaseWorldTime();
        return;
    }

    set_transient_control(
        world_scale,
        LLSD((F64)llclamp(scale, 0.f, TEMPORAL_MAX_SCALE)));
    set_transient_control(animation, LLSD(true));
    set_transient_control(objects, LLSD(true));
    set_transient_control(texture_anim, LLSD(true));
    set_transient_control(particles, LLSD(true));
    set_transient_control(mode, LLSD(1));
}

void ALDirectorSwitcher::updateEaseWorldTime(F64 now)
{
    if (!mEaseWorldTimeActive)
    {
        return;
    }
    if (!std::isfinite(now) || now < mActiveSince ||
        !gSavedSettings.getBOOL("DirectorSwitcherArmed") ||
        !gSavedSettings.getBOOL("DirectorSwitcherEaseCuts") ||
        !gSavedSettings.getBOOL("DirectorSwitcherEaseFreeze") ||
        !gSavedSettings.getBOOL("CinematicCamEnabled") ||
        mCutEaseSeconds <= 0.f)
    {
        restoreEaseWorldTime();
        return;
    }

    const F64 ease_elapsed = mCutEaseTimer.getElapsedTimeF64();
    if (!std::isfinite(ease_elapsed) || ease_elapsed < 0.0)
    {
        restoreEaseWorldTime();
        return;
    }
    const F32 u = (F32)(ease_elapsed / (F64)mCutEaseSeconds);
    if (u >= 1.f)
    {
        restoreEaseWorldTime();
        return;
    }

    if (mEaseUsesFreezeWorld)
    {
        LLControlVariable* gate =
            gSavedSettings.getControl("BDMergeFreezeWorld");
        LLControlVariable* freeze =
            gSavedSettings.getControl("UseFreezeWorld");
        LLControlVariable* freeze_time =
            gSavedSettings.getControl("FreezeTime");
        if (!gate || !freeze || !freeze_time)
        {
            restoreEaseWorldTime();
            return;
        }
        applyEaseWorldScale(0.f);
        if (!mEaseWorldTimeActive)
        {
            return;
        }
        set_transient_control(gate, LLSD(true));
        set_transient_control(freeze, LLSD(true));
        set_transient_control(freeze_time, LLSD(true));
        return;
    }

    // A symmetric pulse on unscaled time: curve down during the first half of
    // the cut duration, then the same latched curve back during the second.
    const F32 half_weight =
        llclamp(
            ALCameraCurve::evalFeathered(
                mFreezeCurveId,
                u < 0.5f ? u * 2.f : (u - 0.5f) * 2.f,
                mFreezeBezier[0], mFreezeBezier[1],
                mFreezeBezier[2], mFreezeBezier[3], mFreezeFeather),
            0.f, 1.f);
    const F32 factor =
        u < 0.5f
            ? 1.f + (mEaseTargetFactor - 1.f) * half_weight
            : mEaseTargetFactor +
                (1.f - mEaseTargetFactor) * half_weight;
    applyEaseWorldScale(mEaseBaseWorldScale * factor);
}

void ALDirectorSwitcher::restoreEaseWorldTime()
{
    if (!mEaseWorldTimeActive)
    {
        return;
    }

    set_transient_control(
        gSavedSettings.getControl("TemporalDriveAnimation"),
        LLSD(mSavedDriveAnimation));
    set_transient_control(
        gSavedSettings.getControl("TemporalDriveObjects"),
        LLSD(mSavedDriveObjects));
    set_transient_control(
        gSavedSettings.getControl("TemporalDriveTextureAnim"),
        LLSD(mSavedDriveTextureAnim));
    set_transient_control(
        gSavedSettings.getControl("TemporalDriveParticles"),
        LLSD(mSavedDriveParticles));
    set_transient_control(
        gSavedSettings.getControl("TemporalWorldScale"),
        LLSD((F64)mSavedTemporalWorldScale));
    set_transient_control(
        gSavedSettings.getControl("TemporalMode"),
        LLSD(mSavedTemporalMode));

    if (mEaseUsesFreezeWorld)
    {
        // Temporal controls are restored first; LLPresentationTime observes
        // them at the next frame boundary. Release UseFreezeWorld before its
        // gate because its listener owns avatar pause handles. FreezeTime is
        // last so an unrelated pre-existing stock freeze-frame is preserved.
        set_transient_control(
            gSavedSettings.getControl("UseFreezeWorld"),
            LLSD(mSavedUseFreezeWorld));
        set_transient_control(
            gSavedSettings.getControl("BDMergeFreezeWorld"),
            LLSD(mSavedBDMergeFreezeWorld));
        set_transient_control(
            gSavedSettings.getControl("FreezeTime"),
            LLSD(mSavedFreezeTime));
    }

    mEaseWorldTimeActive = false;
    mEaseUsesFreezeWorld = false;
    mEaseBaseWorldScale = 1.f;
    mEaseTargetFactor = 1.f;
}

void ALDirectorSwitcher::cancelEaseWorldTime()
{
    restoreEaseWorldTime();
}

bool ALDirectorSwitcher::applySlot(
    S32 slot, F64 now, F64 boundary,
    const std::vector<Slot>& bank, bool manual, bool force)
{
    if (slot < 0 || slot >= SLOT_COUNT ||
        slot >= (S32)bank.size() || !std::isfinite(now))
    {
        return false;
    }

    const S32 next_mode = sanitizeMode(bank[slot].mMode);
    Slot next_config = bank[slot];
    sanitizeSlot(next_config);
    const bool custom_relevant =
        next_mode >= LLCinematicCamera::MODE_STATIC_WIDE &&
        next_mode <= LLCinematicCamera::MODE_STATIC_FULL;
    const bool secondary_relevant =
        next_mode == LLCinematicCamera::MODE_OTS ||
        next_mode == LLCinematicCamera::MODE_TWO_SHOT;
    const bool changed =
        slot != mActiveSlot || next_mode != mActiveMode ||
        next_config.mPrimarySubject !=
            mActiveSlotConfig.mPrimarySubject ||
        (secondary_relevant &&
         next_config.mSecondarySubject !=
            mActiveSlotConfig.mSecondarySubject) ||
        (custom_relevant &&
         (next_config.mCustomEnabled != mActiveSlotConfig.mCustomEnabled ||
          next_config.mCustomYawOffsetDeg !=
              mActiveSlotConfig.mCustomYawOffsetDeg ||
          next_config.mCustomPitchDeg != mActiveSlotConfig.mCustomPitchDeg ||
          next_config.mCustomDistanceM !=
              mActiveSlotConfig.mCustomDistanceM ||
          next_config.mCustomHeightM != mActiveSlotConfig.mCustomHeightM ||
          next_config.mCustomFovDeg != mActiveSlotConfig.mCustomFovDeg));
    if (!changed && !force)
    {
        return false;
    }

    // A real new take interrupts any prior cut envelope. Restore its exact
    // temporal baseline before snapshotting state for the next ease.
    restoreEaseWorldTime();

    if (manual)
    {
        mController.manualPunch(slot, now);
    }
    mActiveSlot = slot;
    mActiveMode = next_mode;
    mActiveSlotConfig = next_config;
    mActiveSince =
        std::isfinite(boundary) && boundary <= now ? boundary : now;
    mCutEaseSeconds =
        gSavedSettings.getBOOL("DirectorSwitcherEaseCuts")
        ? llclamp(gSavedSettings.getF32("DirectorSwitcherEaseSec"),
                  0.f, MAX_EASE_SECONDS)
        : 0.f;
    latch_curve(
        "DirectorSwitcherEaseCurve",
        "DirectorSwitcherEaseBezierX1",
        "DirectorSwitcherEaseBezierY1",
        "DirectorSwitcherEaseBezierX2",
        "DirectorSwitcherEaseBezierY2",
        mCutEaseCurveId, mCutEaseBezier);
    mCutEaseFeather = finite_clamp(
        gSavedSettings.getF32("DirectorSwitcherEaseFeather"),
        0.f, 0.f, 1.f);
    mCutEaseTimer.reset();
    if (++mCutSerial == 0)
    {
        ++mCutSerial;
    }
    startEaseWorldTime(now);
    return true;
}

void ALDirectorSwitcher::tick(F64 now)
{
    static LLCachedControl<bool> armed_setting(
        gSavedSettings, "DirectorSwitcherArmed", false);
    const bool armed = armed_setting;
    if (!armed)
    {
        restoreEaseWorldTime();
        if (mWasArmed)
        {
            leaveArmed();
        }
        return;
    }

    if (!std::isfinite(now) || now < 0.0)
    {
        restoreEaseWorldTime();
        return;
    }

    const std::vector<Slot> bank = loadBank();
    if (!mWasArmed)
    {
        mWasArmed = true;
        enterArmed(now, bank);
    }

    const bool yield_to_flycam = shouldYieldToFlycam();
    if (yield_to_flycam != mYieldingToFlycam)
    {
        if (yield_to_flycam)
        {
            // Relinquish only the transient enable value still owned by this
            // switcher. An independently enabled CineCam remains untouched.
            if (mHaveEnableSnapshot && mWroteEnable && mLastWrittenEnable &&
                gSavedSettings.getBOOL("CinematicCamEnabled"))
            {
                writeCameraEnabled(mEnableWasEnabled);
            }
            restoreEaseWorldTime();
        }
        else
        {
            writeCameraEnabled(true);
        }
        mYieldingToFlycam = yield_to_flycam;
    }

    const bool camera_enabled =
        gSavedSettings.getBOOL("CinematicCamEnabled");
    if (now < mLastPresentationTime)
    {
        restoreEaseWorldTime();
        mController.rebase(now);
        mActiveSince = now;
        mCutEaseSeconds = 0.f;
        if (++mCutSerial == 0)
        {
            ++mCutSerial;
        }
    }
    if (camera_enabled != mWasCameraEnabled)
    {
        mController.rebase(now);
        mWasCameraEnabled = camera_enabled;
        if (!camera_enabled)
        {
            restoreEaseWorldTime();
        }
        if (camera_enabled && mActiveSlot >= 0)
        {
            // ACTION/resume is a hard take boundary, never a stale ease.
            mActiveSince = now;
            mCutEaseSeconds = 0.f;
            if (++mCutSerial == 0)
            {
                ++mCutSerial;
            }
        }
    }
    mLastPresentationTime = now;
    updateEaseWorldTime(now);

    if (!camera_enabled)
    {
        return; // CUT suspends; manual punch or ACTION resumes.
    }

    const ALDirectorSwitcherModel::Frame frame =
        mController.update(now, readConfig(bank));
    if (frame.mCut)
    {
        // Every elapsed boundary is a take, even when an O(1) hitch lookup
        // lands back on the currently live slot. Fine-grained evaluation would
        // have reset that slot's pattern phase at the same final boundary.
        applySlot(frame.mSlot, now, frame.mBoundary, bank, false, true);
    }
}

bool ALDirectorSwitcher::punch(S32 slot)
{
    if (slot < 0 || slot >= SLOT_COUNT ||
        !gSavedSettings.getBOOL("DirectorSwitcherArmed"))
    {
        return false;
    }

    const F64 now = presentation_now();
    if (!std::isfinite(now) || now < 0.0)
    {
        return false;
    }
    const bool camera_was_enabled =
        gSavedSettings.getBOOL("CinematicCamEnabled");
    tick(now);
    writeCameraEnabled(true);
    mWasCameraEnabled = true;
    // Punching the already-live source is a no-op while it is on program, as
    // on a hardware switcher. After CUT, however, the same button is a genuine
    // resume/take and must re-anchor motion and the auto interval.
    return applySlot(
        slot, now, now, loadBank(), true, !camera_was_enabled);
}

void ALDirectorSwitcher::onDirectorAction()
{
    if (!gSavedSettings.getBOOL("DirectorSwitcherArmed") ||
        !gSavedSettings.getBOOL("DirectorArmCamera"))
    {
        return;
    }
    const F64 now = presentation_now();
    if (!std::isfinite(now) || now < 0.0)
    {
        return;
    }
    tick(now);
    writeCameraEnabled(true);
    mWasCameraEnabled = true;
    mController.rebase(now);
    restoreEaseWorldTime();
    if (mActiveSlot >= 0)
    {
        mActiveSince = now;
        mCutEaseSeconds = 0.f;
        if (++mCutSerial == 0)
        {
            ++mCutSerial;
        }
    }
}

void ALDirectorSwitcher::onDirectorCut()
{
    if (!gSavedSettings.getBOOL("DirectorSwitcherArmed") ||
        !gSavedSettings.getBOOL("DirectorArmCamera"))
    {
        return;
    }
    const F64 now = presentation_now();
    if (!std::isfinite(now) || now < 0.0)
    {
        return;
    }
    writeCameraEnabled(false);
    mWasCameraEnabled = false;
    mController.rebase(now);
    restoreEaseWorldTime();
    mCutEaseSeconds = 0.f;
}

bool ALDirectorSwitcher::isDrivingCamera() const
{
    static LLCachedControl<bool> armed(
        gSavedSettings, "DirectorSwitcherArmed", false);
    static LLCachedControl<bool> camera_enabled(
        gSavedSettings, "CinematicCamEnabled", false);
    return !shouldYieldToFlycam() && mWasArmed &&
           armed && camera_enabled &&
           mActiveMode > LLCinematicCamera::MODE_OFF &&
           mActiveMode <= LLCinematicCamera::MODE_STATIC_FULL;
}

bool ALDirectorSwitcher::activeCustomAngle(Slot& slot) const
{
    if (!isDrivingCamera() || !mActiveSlotConfig.mCustomEnabled ||
        mActiveMode < LLCinematicCamera::MODE_STATIC_WIDE ||
        mActiveMode > LLCinematicCamera::MODE_STATIC_FULL)
    {
        return false;
    }
    slot = mActiveSlotConfig;
    sanitizeSlot(slot);
    return true;
}

S32 ALDirectorSwitcher::activePrimarySubject() const
{
    return isDrivingCamera()
        ? sanitizePrimarySubject(mActiveSlotConfig.mPrimarySubject)
        : SUBJECT_DEFAULT;
}

S32 ALDirectorSwitcher::activeSecondarySubject() const
{
    return isDrivingCamera()
        ? sanitizeSecondarySubject(mActiveSlotConfig.mSecondarySubject)
        : SUBJECT_B;
}

bool ALDirectorSwitcher::cameraEnableBaseline(bool& enabled) const
{
    if (!mWasArmed || !mHaveEnableSnapshot ||
        !gSavedSettings.getBOOL("DirectorSwitcherArmed"))
    {
        return false;
    }
    enabled = mEnableWasEnabled;
    return true;
}
