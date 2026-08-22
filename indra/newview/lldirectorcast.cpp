/**
 * @file lldirectorcast.cpp
 * @brief Director Console engine -- see lldirectorcast.h.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "lldirectorcast.h"

#include "aldirectorswitcher.h"
#include "alghoststudio.h"
#include "llghostavatar.h"          // LLGhostAvatar complete type for resolveEntityClone() upcast
#include "llactormover.h"           // startAll/stopAll/placeAt (ACTION, marks)
#include "lleventtimer.h"           // one-shot countdown timer
#include "llpresentationtime.h"
#include "llsdutil_math.h"          // ll_sd_from_vector3 (scene marks)
#include "llflycamrecorder.h"       // armed recorder playback/capture
#include "llviewercontrol.h"        // gSavedSettings, LLCachedControl
#include "llviewerobjectlist.h"     // gObjectList
#include "llvoavatar.h"
#include "llvoavatarself.h"         // gAgentAvatarp, isAgentAvatarValid()

#include <algorithm>
#include <cmath>
#include <iterator>

namespace
{
LLSD writeGazeAimTarget(const LLActorMover::GazeTarget& target)
{
    LLSD data = LLSD::emptyMap();
    data["mode"] = static_cast<S32>(target.mMode);
    if (target.mCastRef.notNull())
    {
        data["cast_ref"] = target.mCastRef;
    }
    if (!target.mFixedPoint.isExactlyZero())
    {
        data["fixed_point"] = ll_sd_from_vector3d(target.mFixedPoint);
    }
    if (target.mObjectRef.notNull())
    {
        data["object_ref"] = target.mObjectRef;
    }
    return data;
}

bool readEyeGazeAimTarget(const LLSD& data, LLActorMover::GazeTarget& target)
{
    if (!data.isMap() || !data.has("mode"))
    {
        return false;
    }
    const S32 mode = data["mode"].asInteger();
    // CAMERA..NEAR_LENS: the aimed independent targets plus "Relaxed (eyes
    // idle)" and "Near-lens (just off camera)", which carry no aim data.
    if (mode < static_cast<S32>(LLActorMover::GazeTarget::CAMERA) ||
        mode > static_cast<S32>(LLActorMover::GazeTarget::NEAR_LENS))
    {
        return false;
    }

    target = LLActorMover::GazeTarget();
    target.mMode = static_cast<LLActorMover::GazeTarget::EMode>(mode);
    if (data.has("cast_ref"))
    {
        target.mCastRef = data["cast_ref"].asUUID();
    }
    if (data.has("fixed_point"))
    {
        target.mFixedPoint = ll_vector3d_from_sd(data["fixed_point"]);
    }
    if (data.has("object_ref"))
    {
        target.mObjectRef = data["object_ref"].asUUID();
    }
    return true;
}

void writeGazeExpression(LLSD& data, const LLActorMover::GazeTarget& target,
                         bool include_persona = true)
{
    if (include_persona)
    {
        data["persona_dom"] = target.mPersonaDominance;
        data["persona_aff"] = target.mPersonaAffection;
        data["persona_anx"] = target.mPersonaAnxiety;
    }
    if (target.mHeadEyeBlendOverride >= 0.f)
    {
        data["perf_head_eye"] = target.mHeadEyeBlendOverride;
    }
    if (target.mTorsoAmountOverride >= 0.f)
    {
        data["perf_torso"] = target.mTorsoAmountOverride;
    }
    if (target.mIntensityOverride >= 0.f)
    {
        data["perf_intensity"] = target.mIntensityOverride;
    }
    if (target.mSmoothingOverride >= 0.f)
    {
        data["perf_smoothing"] = target.mSmoothingOverride;
    }
    if (target.mEyelineOverride)
    {
        data["perf_eyeline_yaw"] = target.mEyelineYawDegOverride;
        data["perf_eyeline_pitch"] = target.mEyelinePitchDegOverride;
    }
    if (target.mMicroLifeOverride >= 0.f)
    {
        data["perf_micro_life"] = target.mMicroLifeOverride;
    }
    if (target.mBlinksOverride >= 0)
    {
        data["perf_blinks"] = target.mBlinksOverride != 0;
    }
    if (target.mVariationOverride >= 0.f)
    {
        data["perf_variation"] = target.mVariationOverride;
    }
    if (target.mBreakFrequencyOverride >= 0.f)
    {
        data["perf_break_frequency"] = target.mBreakFrequencyOverride;
    }
    if (target.mEaseAcquireOverride >= 0.f)
    {
        data["perf_acquire"] = target.mEaseAcquireOverride;
    }
    if (target.mEaseReleaseOverride >= 0.f)
    {
        data["perf_release"] = target.mEaseReleaseOverride;
    }
    if (target.mDeadZoneDegOverride >= 0.f)
    {
        data["perf_dead_zone"] = target.mDeadZoneDegOverride;
    }
    if (fabsf(target.mBlinkRateScale - 1.f) > 1e-5f)
    {
        data["blink_rate_scale"] = target.mBlinkRateScale;
    }
    if (fabsf(target.mVergenceScale - 1.f) > 1e-5f)
    {
        data["vergence_scale"] = target.mVergenceScale;
    }
    // [Machinima] Schema cleanup: the existing discrete overrides (scope, SL
    // anim priority, camera mode) never round-tripped through the scene file.
    // Persist them now alongside the newer Phase 2-5 structural overrides so
    // no discrete axis is the odd one left out of a reload. Absent -> the
    // GazeTarget constructor's inherit sentinels (readGazeExpression()
    // never touches a field its key doesn't have).
    if (target.mGazePriorityOverride >= 0)
    {
        data["perf_scope"] = target.mGazePriorityOverride;
    }
    if (target.mAnimPriorityOverride >= -1)
    {
        data["perf_anim_priority"] = target.mAnimPriorityOverride;
    }
    if (target.mCameraModeOverride >= 0)
    {
        data["perf_camera_mode"] = target.mCameraModeOverride;
    }
    if (target.mCompositionOverride >= 0)
    {
        data["perf_composition"] = target.mCompositionOverride;
    }
    if (target.mCompositionMixOverride >= 0.f)
    {
        data["perf_composition_mix"] = target.mCompositionMixOverride;
    }
    if (target.mChestShareOverride >= 0.f)
    {
        data["perf_chest_share"] = target.mChestShareOverride;
    }
    if (target.mLimitProfileModeOverride >= 0)
    {
        data["perf_limit_mode"] = target.mLimitProfileModeOverride;
        if (target.mLimitProfileModeOverride == 1)
        {
            // Only the actor-custom mode consults mLimitProfile; legacy/inherit
            // scenes never wrote it and should stay byte-identical.
            const ALGazeMath::AnatomicalLimitProfile& p = target.mLimitProfile;
            LLSD profile_sd = LLSD::emptyMap();
            profile_sd["eye_yaw"] = p.mEyeYawDeg;
            profile_sd["eye_pitch"] = p.mEyePitchDeg;
            profile_sd["head_yaw"] = p.mHeadYawDeg;
            profile_sd["head_pitch"] = p.mHeadPitchDeg;
            profile_sd["neck_yaw"] = p.mNeckYawDeg;
            profile_sd["neck_pitch"] = p.mNeckPitchDeg;
            profile_sd["spine_yaw"] = p.mSpineYawDeg;
            profile_sd["spine_pitch"] = p.mSpinePitchDeg;
            profile_sd["hips_yaw"] = p.mHipsYawDeg;
            profile_sd["hips_pitch"] = p.mHipsPitchDeg;
            profile_sd["eye_apply_yaw"] = p.mEyeApplyYawDeg;
            profile_sd["eye_apply_pitch"] = p.mEyeApplyPitchDeg;
            profile_sd["eye_radial"] = p.mEyeRadialDeg;
            data["perf_limit_profile"] = profile_sd;
        }
    }
    if (target.mLeanCurveOverride >= 0)
    {
        data["perf_lean_curve"] = target.mLeanCurveOverride;
    }
    if (target.mLeanThresholdDegOverride >= 0.f)
    {
        data["perf_lean_threshold"] = target.mLeanThresholdDegOverride;
    }
    if (target.mLeanSoftnessDegOverride >= 0.f)
    {
        data["perf_lean_softness"] = target.mLeanSoftnessDegOverride;
    }
    if (target.mLeanMaxDegOverride >= 0.f)
    {
        data["perf_lean_max"] = target.mLeanMaxDegOverride;
    }
}

void readGazeExpression(const LLSD& data, LLActorMover::GazeTarget& target)
{
    target.mPersonaOverride = data.has("persona_dom") ||
        data.has("persona_aff") || data.has("persona_anx");
    if (data.has("persona_dom"))
    {
        target.mPersonaDominance = llclamp((F32)data["persona_dom"].asReal(), -1.f, 1.f);
    }
    if (data.has("persona_aff"))
    {
        target.mPersonaAffection = llclamp((F32)data["persona_aff"].asReal(), -1.f, 1.f);
    }
    if (data.has("persona_anx"))
    {
        target.mPersonaAnxiety = llclamp((F32)data["persona_anx"].asReal(), -1.f, 1.f);
    }
    if (data.has("perf_head_eye"))
    {
        target.mHeadEyeBlendOverride = llclamp((F32)data["perf_head_eye"].asReal(), 0.f, 1.f);
    }
    if (data.has("perf_torso"))
    {
        target.mTorsoAmountOverride = llclamp((F32)data["perf_torso"].asReal(), 0.f, 1.f);
    }
    if (data.has("perf_intensity"))
    {
        target.mIntensityOverride = llclamp((F32)data["perf_intensity"].asReal(), 0.f, 1.f);
    }
    if (data.has("perf_smoothing"))
    {
        target.mSmoothingOverride = llclamp((F32)data["perf_smoothing"].asReal(), 0.f, 1.f);
    }
    if (data.has("perf_eyeline_yaw") || data.has("perf_eyeline_pitch"))
    {
        target.mEyelineOverride = true;
        target.mEyelineYawDegOverride = data.has("perf_eyeline_yaw")
            ? llclamp((F32)data["perf_eyeline_yaw"].asReal(), -15.f, 15.f)
            : 0.f;
        target.mEyelinePitchDegOverride = data.has("perf_eyeline_pitch")
            ? llclamp((F32)data["perf_eyeline_pitch"].asReal(), -10.f, 10.f)
            : 0.f;
    }
    if (data.has("perf_micro_life"))
    {
        target.mMicroLifeOverride = llclamp((F32)data["perf_micro_life"].asReal(), 0.f, 1.f);
    }
    if (data.has("perf_blinks"))
    {
        target.mBlinksOverride = data["perf_blinks"].asBoolean() ? 1 : 0;
    }
    if (data.has("perf_variation"))
    {
        target.mVariationOverride = llclamp((F32)data["perf_variation"].asReal(), 0.f, 1.f);
    }
    if (data.has("perf_break_frequency"))
    {
        target.mBreakFrequencyOverride = llclamp((F32)data["perf_break_frequency"].asReal(), 0.f, 1.f);
    }
    if (data.has("perf_acquire"))
    {
        target.mEaseAcquireOverride = llclamp((F32)data["perf_acquire"].asReal(), 0.01f, 3.f);
    }
    if (data.has("perf_release"))
    {
        target.mEaseReleaseOverride = llclamp((F32)data["perf_release"].asReal(), 0.01f, 4.f);
    }
    if (data.has("perf_dead_zone"))
    {
        target.mDeadZoneDegOverride = llclamp((F32)data["perf_dead_zone"].asReal(), 0.f, 15.f);
    }
    if (data.has("blink_rate_scale"))
    {
        target.mBlinkRateScale = llclamp((F32)data["blink_rate_scale"].asReal(), 0.f, 3.f);
    }
    if (data.has("vergence_scale"))
    {
        target.mVergenceScale = llclamp((F32)data["vergence_scale"].asReal(), -1.f, 1.f);
    }
    // [Machinima] Schema cleanup + Phase 2-5 structural overrides. Absent
    // keys leave the field at the GazeTarget constructor's inherit sentinel
    // (target is freshly default-constructed by every caller of this
    // function), so old scenes reconstruct byte-identically.
    if (data.has("perf_scope"))
    {
        target.mGazePriorityOverride = llclamp(data["perf_scope"].asInteger(),
            (S32)LLActorMover::GAZE_PRIORITY_BLEND,
            (S32)LLActorMover::GAZE_PRIORITY_PLANTED_SPINE);
    }
    if (data.has("perf_anim_priority"))
    {
        target.mAnimPriorityOverride = llclamp(data["perf_anim_priority"].asInteger(), -1, 6);
    }
    if (data.has("perf_camera_mode"))
    {
        target.mCameraModeOverride = llclamp(data["perf_camera_mode"].asInteger(), 0, 1);
    }
    if (data.has("perf_composition"))
    {
        target.mCompositionOverride = llclamp(data["perf_composition"].asInteger(),
            (S32)LLActorMover::GAZE_COMPOSE_REPLACE,
            (S32)LLActorMover::GAZE_COMPOSE_BLEND);
    }
    if (data.has("perf_composition_mix"))
    {
        target.mCompositionMixOverride = llclamp((F32)data["perf_composition_mix"].asReal(), 0.f, 1.f);
    }
    if (data.has("perf_chest_share"))
    {
        target.mChestShareOverride = llclamp((F32)data["perf_chest_share"].asReal(), 0.f, 1.f);
    }
    if (data.has("perf_limit_mode"))
    {
        target.mLimitProfileModeOverride = llclamp(data["perf_limit_mode"].asInteger(), 0, 1);
        if (target.mLimitProfileModeOverride == 1 && data.has("perf_limit_profile"))
        {
            const LLSD& profile_sd = data["perf_limit_profile"];
            ALGazeMath::AnatomicalLimitProfile p; // constructor defaults for any absent field
            if (profile_sd.has("eye_yaw"))
            {
                p.mEyeYawDeg = llclamp((F32)profile_sd["eye_yaw"].asReal(), 0.f, 180.f);
            }
            if (profile_sd.has("eye_pitch"))
            {
                p.mEyePitchDeg = llclamp((F32)profile_sd["eye_pitch"].asReal(), 0.f, 180.f);
            }
            if (profile_sd.has("head_yaw"))
            {
                p.mHeadYawDeg = llclamp((F32)profile_sd["head_yaw"].asReal(), 0.f, 180.f);
            }
            if (profile_sd.has("head_pitch"))
            {
                p.mHeadPitchDeg = llclamp((F32)profile_sd["head_pitch"].asReal(), 0.f, 180.f);
            }
            if (profile_sd.has("neck_yaw"))
            {
                p.mNeckYawDeg = llclamp((F32)profile_sd["neck_yaw"].asReal(), 0.f, 180.f);
            }
            if (profile_sd.has("neck_pitch"))
            {
                p.mNeckPitchDeg = llclamp((F32)profile_sd["neck_pitch"].asReal(), 0.f, 180.f);
            }
            if (profile_sd.has("spine_yaw"))
            {
                p.mSpineYawDeg = llclamp((F32)profile_sd["spine_yaw"].asReal(), 0.f, 180.f);
            }
            if (profile_sd.has("spine_pitch"))
            {
                p.mSpinePitchDeg = llclamp((F32)profile_sd["spine_pitch"].asReal(), 0.f, 180.f);
            }
            if (profile_sd.has("hips_yaw"))
            {
                p.mHipsYawDeg = llclamp((F32)profile_sd["hips_yaw"].asReal(), 0.f, 180.f);
            }
            if (profile_sd.has("hips_pitch"))
            {
                p.mHipsPitchDeg = llclamp((F32)profile_sd["hips_pitch"].asReal(), 0.f, 180.f);
            }
            if (profile_sd.has("eye_apply_yaw"))
            {
                p.mEyeApplyYawDeg = llclamp((F32)profile_sd["eye_apply_yaw"].asReal(), 0.f, 180.f);
            }
            if (profile_sd.has("eye_apply_pitch"))
            {
                p.mEyeApplyPitchDeg = llclamp((F32)profile_sd["eye_apply_pitch"].asReal(), 0.f, 180.f);
            }
            if (profile_sd.has("eye_radial"))
            {
                p.mEyeRadialDeg = llclamp((F32)profile_sd["eye_radial"].asReal(), 0.f, 90.f);
            }
            target.mLimitProfile = p;
        }
    }
    if (data.has("perf_lean_curve"))
    {
        target.mLeanCurveOverride = llclamp(data["perf_lean_curve"].asInteger(),
            (S32)LLActorMover::GAZE_LEAN_LEGACY,
            (S32)LLActorMover::GAZE_LEAN_ANGLE_EASE);
    }
    if (data.has("perf_lean_threshold"))
    {
        target.mLeanThresholdDegOverride = llclamp((F32)data["perf_lean_threshold"].asReal(), 0.f, 180.f);
    }
    if (data.has("perf_lean_softness"))
    {
        target.mLeanSoftnessDegOverride = llclamp((F32)data["perf_lean_softness"].asReal(), 0.f, 180.f);
    }
    if (data.has("perf_lean_max"))
    {
        target.mLeanMaxDegOverride = llclamp((F32)data["perf_lean_max"].asReal(), 0.f, 180.f);
    }
}

LLSD writeCueTarget(const LLActorMover::GazeTarget& target)
{
    LLSD data = LLSD::emptyMap();
    data["mode"] = static_cast<S32>(target.mMode);
    if (target.mCastRef.notNull())
    {
        data["cast_ref"] = target.mCastRef;
    }
    if (!target.mFixedPoint.isExactlyZero())
    {
        data["fixed_point"] = ll_sd_from_vector3d(target.mFixedPoint);
    }
    if (target.mObjectRef.notNull())
    {
        data["object_ref"] = target.mObjectRef;
    }
    writeGazeExpression(data, target, target.mPersonaOverride);
    return data;
}

LLActorMover::GazeTarget readCueTarget(const LLSD& data)
{
    LLActorMover::GazeTarget target;
    if (data.has("mode"))
    {
        target.mMode = static_cast<LLActorMover::GazeTarget::EMode>(
            llclamp(data["mode"].asInteger(),
                    static_cast<S32>(LLActorMover::GazeTarget::MOTION),
                    static_cast<S32>(LLActorMover::GazeTarget::OBJECT)));
    }
    if (data.has("cast_ref"))
    {
        target.mCastRef = data["cast_ref"].asUUID();
    }
    if (data.has("fixed_point"))
    {
        target.mFixedPoint = ll_vector3d_from_sd(data["fixed_point"]);
    }
    if (data.has("object_ref"))
    {
        target.mObjectRef = data["object_ref"].asUUID();
    }
    readGazeExpression(data, target);
    return target;
}

F32 cueStyleSeconds(LLDirectorCast::EGazeCueStyle style, bool acquire,
                    const LLActorMover::GazeTarget& target)
{
    const F32 authored = acquire
        ? target.mEaseAcquireOverride : target.mEaseReleaseOverride;
    const F32 base = authored >= 0.f ? authored : (acquire ? 0.25f : 0.60f);
    switch (style)
    {
        case LLDirectorCast::GAZE_CUE_STYLE_SNAP:
            return acquire ? llmin(base, 0.08f) : llmin(base, 0.12f);
        case LLDirectorCast::GAZE_CUE_STYLE_DRIFT:
            return llmax(base, acquire ? 0.65f : 0.90f);
        case LLDirectorCast::GAZE_CUE_STYLE_CASUAL:
            return llmax(base, acquire ? 0.45f : 1.00f);
        case LLDirectorCast::GAZE_CUE_STYLE_DEFAULT:
        default:
            return llmax(base, 0.f);
    }
}

void sanitizeGazeCue(LLDirectorCast::GazeCue& cue)
{
    if (!std::isfinite(cue.mStartSec) || cue.mStartSec < 0.0)
    {
        cue.mStartSec = 0.0;
    }
    if (!std::isfinite(cue.mDurationSec) || cue.mDurationSec < 0.0)
    {
        cue.mDurationSec = 0.0;
    }
    cue.mAcquireStyle = static_cast<LLDirectorCast::EGazeCueStyle>(
        llclamp(static_cast<S32>(cue.mAcquireStyle),
                static_cast<S32>(LLDirectorCast::GAZE_CUE_STYLE_DEFAULT),
                static_cast<S32>(LLDirectorCast::GAZE_CUE_STYLE_CASUAL)));
    cue.mReleaseStyle = static_cast<LLDirectorCast::EGazeCueStyle>(
        llclamp(static_cast<S32>(cue.mReleaseStyle),
                static_cast<S32>(LLDirectorCast::GAZE_CUE_STYLE_DEFAULT),
                static_cast<S32>(LLDirectorCast::GAZE_CUE_STYLE_CASUAL)));
    cue.mMacro = static_cast<ALGazeMath::EGazeCueMacro>(
        llclamp(static_cast<S32>(cue.mMacro),
                static_cast<S32>(ALGazeMath::GAZE_MACRO_NONE),
                static_cast<S32>(ALGazeMath::GAZE_MACRO_OBJECT_GLANCE)));
    if (!std::isfinite(cue.mMacroGapSec))
    {
        cue.mMacroGapSec = 0.35f;
    }
    if (!std::isfinite(cue.mMacroDwellSec))
    {
        cue.mMacroDwellSec = 0.60f;
    }
    cue.mMacroGapSec = llclamp(cue.mMacroGapSec, 0.20f, 0.50f);
    cue.mMacroDwellSec = llclamp(cue.mMacroDwellSec, 0.f, 10.f);
    cue.mHoldFramesPast = llclamp(cue.mHoldFramesPast, 0, 300);
}

LLSD writeGazeCues(const LLDirectorCast::GazeCueList& cues)
{
    LLSD array = LLSD::emptyArray();
    for (const LLDirectorCast::GazeCue& cue : cues)
    {
        LLSD data = LLSD::emptyMap();
        data["start_sec"] = cue.mStartSec;
        if (cue.mDurationSec > 0.0)
        {
            data["duration_sec"] = cue.mDurationSec;
        }
        data["target"] = writeCueTarget(cue.mTarget);
        data["acquire_style"] = static_cast<S32>(cue.mAcquireStyle);
        data["release_style"] = static_cast<S32>(cue.mReleaseStyle);
        data["macro"] = static_cast<S32>(cue.mMacro);
        data["macro_gap_sec"] = cue.mMacroGapSec;
        data["macro_dwell_sec"] = cue.mMacroDwellSec;
        data["hold_frames_past"] = cue.mHoldFramesPast;
        data["thought_residue"] = cue.mThoughtResidue;
        array.append(data);
    }
    return array;
}

LLDirectorCast::GazeCueList readGazeCues(const LLSD& array)
{
    LLDirectorCast::GazeCueList cues;
    if (!array.isArray())
    {
        return cues;
    }
    for (LLSD::array_const_iterator it = array.beginArray();
         it != array.endArray(); ++it)
    {
        const LLSD& data = *it;
        LLDirectorCast::GazeCue cue;
        cue.mStartSec = data["start_sec"].asReal();
        cue.mDurationSec = data["duration_sec"].asReal();
        if (data.has("target"))
        {
            cue.mTarget = readCueTarget(data["target"]);
        }
        cue.mAcquireStyle = static_cast<LLDirectorCast::EGazeCueStyle>(
            data["acquire_style"].asInteger());
        cue.mReleaseStyle = static_cast<LLDirectorCast::EGazeCueStyle>(
            data["release_style"].asInteger());
        cue.mMacro = static_cast<ALGazeMath::EGazeCueMacro>(
            data["macro"].asInteger());
        if (data.has("macro_gap_sec"))
        {
            cue.mMacroGapSec = static_cast<F32>(data["macro_gap_sec"].asReal());
        }
        if (data.has("macro_dwell_sec"))
        {
            cue.mMacroDwellSec = static_cast<F32>(data["macro_dwell_sec"].asReal());
        }
        if (data.has("hold_frames_past"))
        {
            cue.mHoldFramesPast = data["hold_frames_past"].asInteger();
        }
        cue.mThoughtResidue = data["thought_residue"].asBoolean();
        cues.push_back(cue);
    }
    return cues;
}

// [Machinima] Goal 3b: keyframable gaze influence lane, serialized parallel to
// the cue list above. Written/read raw -- evaluateGazeInfluence() is the pure
// sanitize/sort/evaluate point, so this side stays a plain field-for-field
// round trip like writeCueTarget()/readCueTarget() above it.
LLSD writeGazeInfluenceKeys(const LLDirectorCast::GazeInfluenceKeyList& keys)
{
    LLSD array = LLSD::emptyArray();
    for (const LLDirectorCast::GazeInfluenceKey& key : keys)
    {
        LLSD data = LLSD::emptyMap();
        data["time_sec"] = key.mTimeSec;
        data["value"] = key.mValue;
        data["interp"] = static_cast<S32>(key.mInterpolation);
        array.append(data);
    }
    return array;
}

LLDirectorCast::GazeInfluenceKeyList readGazeInfluenceKeys(const LLSD& array)
{
    LLDirectorCast::GazeInfluenceKeyList keys;
    if (!array.isArray())
    {
        return keys;
    }
    for (LLSD::array_const_iterator it = array.beginArray();
         it != array.endArray(); ++it)
    {
        const LLSD& data = *it;
        LLDirectorCast::GazeInfluenceKey key;
        key.mTimeSec = data["time_sec"].asReal();
        key.mValue = llclamp((F32)data["value"].asReal(), 0.f, 1.f);
        key.mInterpolation = static_cast<LLDirectorCast::EGazeKeyInterpolation>(
            llclamp(data["interp"].asInteger(),
                    static_cast<S32>(LLDirectorCast::GAZE_KEY_STEP),
                    static_cast<S32>(LLDirectorCast::GAZE_KEY_SMOOTHSTEP)));
        keys.push_back(key);
    }
    return keys;
}
} // anonymous namespace

// ---------------------------------------------------------------------------
LLDirectorCast& LLDirectorCast::instance()
{
    static LLDirectorCast sInstance;
    return sInstance;
}

LLDirectorCast::LLDirectorCast()
{
    mSelfGazeTarget.mMode = LLActorMover::GazeTarget::CAMERA;
}

// ---------------------------------------------------------------------------
// membership
// ---------------------------------------------------------------------------
void LLDirectorCast::add(const LLUUID& id)
{
    if (id.isNull() || contains(id))
    {
        return;
    }
    CastMember member;
    member.mId = id;
    mCast.push_back(member);
    mIds.push_back(id);
    mMemberIndex[id] = mCast.size() - 1;
    // seed the cached name right away when the actor is in world
    resolve(id);
}

void LLDirectorCast::remove(const LLUUID& id)
{
    auto it = std::find_if(mCast.begin(), mCast.end(),
                           [&id](const CastMember& m) { return m.mId == id; });
    if (it == mCast.end())
    {
        return;
    }
    const std::string group = it->mGroup;
    LLActorMover::instance().clearDirectorLookAtRuntime(id);
    mCast.erase(it);
    ++mGazeCueRevision;
    mIds.erase(std::find(mIds.begin(), mIds.end(), id));
    rebuildMemberIndex();
    mLookAtCameraIds.erase(id);
    // a subject that leaves the cast stops being a subject
    if (mSubjectA == id)
    {
        mSubjectA.setNull();
    }
    if (mSubjectB == id)
    {
        mSubjectB.setNull();
    }
    if (mSubjectC == id)
    {
        mSubjectC.setNull();
    }
    if (mSubjectD == id)
    {
        mSubjectD.setNull();
    }
    // leaving the cast also leaves the start queue, and may retire the group
    cancelPendingStart(id);
    pruneGroupDelay(group);
}

void LLDirectorCast::toggle(const LLUUID& id)
{
    if (id.isNull())
    {
        return;
    }
    contains(id) ? remove(id) : add(id);
}

bool LLDirectorCast::contains(const LLUUID& id) const
{
    return id.notNull() && mMemberIndex.find(id) != mMemberIndex.end();
}

LLDirectorCast::CastMember* LLDirectorCast::getMember(const LLUUID& id)
{
    const auto it = mMemberIndex.find(id);
    if (it == mMemberIndex.end() || it->second >= mCast.size())
    {
        return nullptr;
    }
    return &mCast[it->second];
}

const LLDirectorCast::CastMember* LLDirectorCast::getMember(const LLUUID& id) const
{
    return const_cast<LLDirectorCast*>(this)->getMember(id);
}

void LLDirectorCast::setLookAtCamera(const LLUUID& id, bool selected)
{
    if (id.isNull() || (isAgentAvatarValid() && id == gAgentAvatarp->getID()))
    {
        mSelfLookAtCamera = selected;
        return;
    }
    if (!contains(id))
    {
        return;
    }
    if (selected)
    {
        mLookAtCameraIds.insert(id);
    }
    else
    {
        mLookAtCameraIds.erase(id);
    }
}

bool LLDirectorCast::isLookAtCamera(const LLUUID& id) const
{
    if (id.isNull() || (isAgentAvatarValid() && id == gAgentAvatarp->getID()))
    {
        return mSelfLookAtCamera;
    }
    return mLookAtCameraIds.find(id) != mLookAtCameraIds.end();
}

void LLDirectorCast::rebuildMemberIndex()
{
    mMemberIndex.clear();
    for (std::size_t i = 0; i < mCast.size(); ++i)
    {
        mMemberIndex[mCast[i].mId] = i;
    }
}

void LLDirectorCast::setGazeTarget(const LLUUID& id, const LLActorMover::GazeTarget& target)
{
    if (id.isNull() || (isAgentAvatarValid() && id == gAgentAvatarp->getID()))
    {
        mSelfGazeTarget = target;
        LLActorMover::instance().setGazeTargetConfig(LLUUID::null, target);
        return;
    }
    if (CastMember* m = getMember(id))
    {
        m->mGazeTarget = target;
        LLActorMover::instance().setGazeTargetConfig(id, target);
    }
}

LLActorMover::GazeTarget LLDirectorCast::getGazeTarget(const LLUUID& id) const
{
    if (id.isNull() || (isAgentAvatarValid() && id == gAgentAvatarp->getID()))
    {
        return mSelfGazeTarget;
    }
    if (const CastMember* m = getMember(id))
    {
        return m->mGazeTarget;
    }
    return LLActorMover::GazeTarget();
}

void LLDirectorCast::setEyeGazeTarget(
    const LLUUID& id, const LLActorMover::GazeTarget& target)
{
    // Valid independent eye modes are CAMERA..OBJECT (the aimed targets) plus
    // RELAXED (eyes idle) and NEAR_LENS (just off camera), which carry no aim
    // data. MOTION and anything else fall back to "no eye target" (eyes follow
    // the head).
    if (target.mMode < LLActorMover::GazeTarget::CAMERA ||
        target.mMode > LLActorMover::GazeTarget::NEAR_LENS)
    {
        clearEyeGazeTarget(id);
        return;
    }
    if (id.isNull() || (isAgentAvatarValid() && id == gAgentAvatarp->getID()))
    {
        mSelfEyeGazeTarget = target;
        mSelfEyeGazeTargetEnabled = true;
        return;
    }
    if (CastMember* m = getMember(id))
    {
        m->mEyeGazeTarget = target;
        m->mEyeGazeTargetEnabled = true;
    }
}

void LLDirectorCast::clearEyeGazeTarget(const LLUUID& id)
{
    if (id.isNull() || (isAgentAvatarValid() && id == gAgentAvatarp->getID()))
    {
        mSelfEyeGazeTarget = LLActorMover::GazeTarget();
        mSelfEyeGazeTargetEnabled = false;
        return;
    }
    if (CastMember* m = getMember(id))
    {
        m->mEyeGazeTarget = LLActorMover::GazeTarget();
        m->mEyeGazeTargetEnabled = false;
    }
}

bool LLDirectorCast::hasEyeGazeTarget(const LLUUID& id) const
{
    if (id.isNull() || (isAgentAvatarValid() && id == gAgentAvatarp->getID()))
    {
        return mSelfEyeGazeTargetEnabled;
    }
    const CastMember* m = getMember(id);
    return m && m->mEyeGazeTargetEnabled;
}

LLActorMover::GazeTarget LLDirectorCast::getEyeGazeTarget(const LLUUID& id) const
{
    if (id.isNull() || (isAgentAvatarValid() && id == gAgentAvatarp->getID()))
    {
        return mSelfEyeGazeTarget;
    }
    if (const CastMember* m = getMember(id))
    {
        return m->mEyeGazeTarget;
    }
    return LLActorMover::GazeTarget();
}

void LLDirectorCast::setGazeCues(const LLUUID& id, const GazeCueList& input)
{
    GazeCueList cues = input;
    for (GazeCue& cue : cues)
    {
        sanitizeGazeCue(cue);
    }
    std::stable_sort(cues.begin(), cues.end(),
        [](const GazeCue& a, const GazeCue& b)
        {
            return a.mStartSec < b.mStartSec;
        });

    LLUUID runtime_id = id;
    if (id.isNull() || (isAgentAvatarValid() && id == gAgentAvatarp->getID()))
    {
        mSelfGazeCues = cues;
        if (isAgentAvatarValid())
        {
            runtime_id = gAgentAvatarp->getID();
        }
    }
    else if (CastMember* member = getMember(id))
    {
        member->mGazeCues = cues;
    }
    else
    {
        return;
    }

    ++mGazeCueRevision;
    LLActorMover::instance().clearDirectorLookAtRuntime(runtime_id);
}

const LLDirectorCast::GazeCueList& LLDirectorCast::getGazeCues(const LLUUID& id) const
{
    static const GazeCueList empty;
    if (id.isNull() || (isAgentAvatarValid() && id == gAgentAvatarp->getID()))
    {
        return mSelfGazeCues;
    }
    if (const CastMember* member = getMember(id))
    {
        return member->mGazeCues;
    }
    return empty;
}

// [Machinima] Goal 3b: influence-lane accessors, mirroring the cue accessors
// (same self/member routing and shared mGazeCueRevision bump so a floater
// refresh keyed on the revision picks up either lane changing).
void LLDirectorCast::setGazeInfluenceKeys(const LLUUID& id,
                                          const GazeInfluenceKeyList& input)
{
    // Store as authored; evaluateGazeInfluence() sanitizes/sorts on read so a
    // scrub is always stable regardless of authoring order.
    GazeInfluenceKeyList keys = input;
    std::stable_sort(keys.begin(), keys.end(),
        [](const GazeInfluenceKey& a, const GazeInfluenceKey& b)
        {
            return a.mTimeSec < b.mTimeSec;
        });

    LLUUID runtime_id = id;
    if (id.isNull() || (isAgentAvatarValid() && id == gAgentAvatarp->getID()))
    {
        mSelfGazeInfluenceKeys = keys;
        if (isAgentAvatarValid())
        {
            runtime_id = gAgentAvatarp->getID();
        }
    }
    else if (CastMember* member = getMember(id))
    {
        member->mGazeInfluenceKeys = keys;
    }
    else
    {
        return;
    }

    ++mGazeCueRevision;
    LLActorMover::instance().clearDirectorLookAtRuntime(runtime_id);
}

const LLDirectorCast::GazeInfluenceKeyList&
LLDirectorCast::getGazeInfluenceKeys(const LLUUID& id) const
{
    static const GazeInfluenceKeyList empty;
    if (id.isNull() || (isAgentAvatarValid() && id == gAgentAvatarp->getID()))
    {
        return mSelfGazeInfluenceKeys;
    }
    if (const CastMember* member = getMember(id))
    {
        return member->mGazeInfluenceKeys;
    }
    return empty;
}

//static
LLDirectorCast::GazeCueEvaluation LLDirectorCast::evaluateGazeCues(
    const GazeCueList& cues, U64 persona_seed, F64 presentation_time)
{
    GazeCueEvaluation result;
    if (cues.empty() || !std::isfinite(presentation_time))
    {
        return result;
    }

    const auto after = std::upper_bound(
        cues.begin(), cues.end(), presentation_time,
        [](F64 time, const GazeCue& cue)
        {
            return time < cue.mStartSec;
        });
    if (after == cues.begin())
    {
        return result; // cue track has not started; preserve the live scene aim
    }

    const std::size_t index = static_cast<std::size_t>(
        std::distance(cues.begin(), after) - 1);
    const GazeCue& cue = cues[index];
    result.mHasOverride = true;
    result.mCueIndex = static_cast<S32>(index);
    result.mToTarget = cue.mTarget;
    if (cue.mMacro == ALGazeMath::GAZE_MACRO_BUTTON_LOOK)
    {
        // Button Look always addresses the live gated/render camera, regardless
        // of the editor's target-mode value.
        result.mToTarget.mMode = LLActorMover::GazeTarget::CAMERA;
    }
    result.mEffectiveTarget = result.mToTarget;

    if (index > 0)
    {
        result.mHasFromTarget = true;
        result.mFromTarget = cues[index - 1].mTarget;
        if (cues[index - 1].mMacro == ALGazeMath::GAZE_MACRO_BUTTON_LOOK)
        {
            result.mFromTarget.mMode = LLActorMover::GazeTarget::CAMERA;
        }

        // Freeze the previous cue's resolved aim at this cue's boundary. This
        // preserves a prior cue that is already releasing/returning instead of
        // always treating its raw target as the crossfade origin.
        if (index > 1)
        {
            result.mHasFromBaseTarget = true;
            result.mFromBaseTarget = cues[index - 2].mTarget;
            if (cues[index - 2].mMacro == ALGazeMath::GAZE_MACRO_BUTTON_LOOK)
            {
                result.mFromBaseTarget.mMode = LLActorMover::GazeTarget::CAMERA;
            }
        }
        const GazeCue& previous = cues[index - 1];
        ALGazeMath::GazeMacroParams previous_params;
        previous_params.mAcquireSec = cueStyleSeconds(
            previous.mAcquireStyle, true, result.mFromTarget);
        previous_params.mReleaseSec = cueStyleSeconds(
            previous.mReleaseStyle, false, result.mFromTarget);
        previous_params.mDurationSec = previous.mDurationSec;
        previous_params.mGapSec = previous.mMacroGapSec;
        previous_params.mDwellSec = previous.mMacroDwellSec;
        previous_params.mHoldFramesPast = previous.mHoldFramesPast;
        previous_params.mThoughtResidue = previous.mThoughtResidue;
        result.mFromTargetBlend = ALGazeMath::evalGazeMacro(
            previous.mMacro, persona_seed,
            cue.mStartSec - previous.mStartSec,
            previous_params).mTargetWeight;
    }

    result.mAcquireSec = cueStyleSeconds(
        cue.mAcquireStyle, true, result.mToTarget);
    result.mReleaseSec = cueStyleSeconds(
        cue.mReleaseStyle, false, result.mToTarget);

    ALGazeMath::GazeMacroParams params;
    params.mAcquireSec = result.mAcquireSec;
    params.mReleaseSec = result.mReleaseSec;
    params.mDurationSec = cue.mDurationSec;
    params.mGapSec = cue.mMacroGapSec;
    params.mDwellSec = cue.mMacroDwellSec;
    params.mHoldFramesPast = cue.mHoldFramesPast;
    params.mThoughtResidue = cue.mThoughtResidue;
    const ALGazeMath::GazeMacroEnvelope envelope =
        ALGazeMath::evalGazeMacro(
            cue.mMacro, persona_seed,
            presentation_time - cue.mStartSec, params);
    result.mTargetBlend = envelope.mTargetWeight;
    result.mEyeWeight = envelope.mEyeWeight;
    result.mHeadWeight = envelope.mHeadWeight;
    result.mBodyWeight = envelope.mBodyWeight;
    result.mLidWiden = envelope.mLidWiden;
    result.mHeadRecoilPitch = envelope.mHeadRecoilPitch;
    result.mPhase = envelope.mPhase;
    return result;
}

//static
// [Machinima] Goal 3b: pure, stateless, scrub-stable evaluation of the
// keyframed influence lane. Never touches mIntensity/mEnv -- this only
// produces the multiplier the caller applies to the final write envelope.
F32 LLDirectorCast::evaluateGazeInfluence(const GazeInfluenceKeyList& keys,
                                          F64 presentation_time)
{
    if (keys.empty() || !std::isfinite(presentation_time))
    {
        return 1.f;
    }

    // Sanitize into a working copy: drop any key whose time or value is not
    // finite (rather than guessing a replacement), clamp the rest to
    // [0,1], and clamp the interpolation enum defensively.
    GazeInfluenceKeyList sanitized;
    sanitized.reserve(keys.size());
    for (const GazeInfluenceKey& key : keys)
    {
        if (!std::isfinite(key.mTimeSec) || !std::isfinite(key.mValue))
        {
            continue;
        }
        GazeInfluenceKey clean = key;
        clean.mValue = llclamp(clean.mValue, 0.f, 1.f);
        clean.mInterpolation = static_cast<EGazeKeyInterpolation>(
            llclamp(static_cast<S32>(clean.mInterpolation),
                    static_cast<S32>(GAZE_KEY_STEP),
                    static_cast<S32>(GAZE_KEY_SMOOTHSTEP)));
        sanitized.push_back(clean);
    }
    if (sanitized.empty())
    {
        return 1.f;
    }

    // Stably sort by time, mirroring the cue-sort approach at
    // lldirectorcast.cpp:634 (see setGazeCues()).
    std::stable_sort(sanitized.begin(), sanitized.end(),
        [](const GazeInfluenceKey& a, const GazeInfluenceKey& b)
        {
            return a.mTimeSec < b.mTimeSec;
        });

    // Duplicate-time keys resolve to the LAST key at that time: collapse
    // stable runs of equal mTimeSec down to their final entry.
    GazeInfluenceKeyList sorted;
    sorted.reserve(sanitized.size());
    for (const GazeInfluenceKey& key : sanitized)
    {
        if (!sorted.empty() && sorted.back().mTimeSec == key.mTimeSec)
        {
            sorted.back() = key;
        }
        else
        {
            sorted.push_back(key);
        }
    }

    if (presentation_time <= sorted.front().mTimeSec)
    {
        return llclamp(sorted.front().mValue, 0.f, 1.f);
    }
    if (presentation_time >= sorted.back().mTimeSec)
    {
        return llclamp(sorted.back().mValue, 0.f, 1.f);
    }

    // presentation_time is strictly between the first and last key's times,
    // so upper_bound here can be neither begin() nor end().
    const auto after = std::upper_bound(
        sorted.begin(), sorted.end(), presentation_time,
        [](F64 time, const GazeInfluenceKey& key)
        {
            return time < key.mTimeSec;
        });
    const std::size_t next_index = static_cast<std::size_t>(
        std::distance(sorted.begin(), after));
    const std::size_t index = next_index - 1;

    const GazeInfluenceKey& a = sorted[index];
    const GazeInfluenceKey& b = sorted[next_index];

    const F64 span = b.mTimeSec - a.mTimeSec;
    if (!(span > 0.0))
    {
        // Identical times after collapsing shouldn't reach here, but guard
        // the division regardless.
        return llclamp(a.mValue, 0.f, 1.f);
    }

    const F32 t = static_cast<F32>(
        llclamp((presentation_time - a.mTimeSec) / span, 0.0, 1.0));

    F32 value;
    switch (a.mInterpolation)
    {
        case GAZE_KEY_STEP:
            value = a.mValue;
            break;
        case GAZE_KEY_LINEAR:
            value = a.mValue + (b.mValue - a.mValue) * t;
            break;
        case GAZE_KEY_SMOOTHSTEP:
        default:
        {
            const F32 s = t * t * (3.f - 2.f * t);
            value = a.mValue + (b.mValue - a.mValue) * s;
            break;
        }
    }
    return llclamp(value, 0.f, 1.f);
}

// ---------------------------------------------------------------------------
// resolution
// ---------------------------------------------------------------------------
LLVOAvatar* LLDirectorCast::resolve(const LLUUID& id)
{
    if (id.isNull())
    {
        return isAgentAvatarValid() ? (LLVOAvatar*)gAgentAvatarp : nullptr;
    }
    LLViewerObject* obj = gObjectList.findObject(id);
    LLVOAvatar* av = obj ? obj->asAvatar() : nullptr;
    if (av && av->isGhostAvatar())
    {
        // Director subjects are the one intentional targeting exception for
        // client-only entity clones. Return the runtime clone itself before
        // any resident/name based fallback can substitute its appearance
        // source. General autopilot, listener lookAt and resident actions keep
        // their ghost exclusions.
        return av->isDead() ? nullptr : av;
    }
    if (!av)
    {
        // Stable Studio instance ids are client-only actor handles. This
        // bypasses resident/name-cache paths and resolves only via the local
        // Ghost Studio registry.
        av = ALGhostStudio::instance().resolveEntityClone(id);
    }
    if (!av || av->isDead())
    {
        return nullptr;
    }
    // refresh the cached display name whenever one is available, so the
    // cast list can keep labelling an actor who later leaves the region
    if (CastMember* m = getMember(id))
    {
        std::string name = av->getFullname();
        if (!name.empty() && name != m->mLastName)
        {
            m->mLastName = name;
        }
    }
    return av;
}

LLVOAvatar* LLDirectorCast::resolveSubjectA()
{
    return mSubjectA.notNull() ? resolve(mSubjectA) : nullptr;
}

LLVOAvatar* LLDirectorCast::resolveSubjectB()
{
    return mSubjectB.notNull() ? resolve(mSubjectB) : nullptr;
}

LLVOAvatar* LLDirectorCast::resolveSubjectC()
{
    return mSubjectC.notNull() ? resolve(mSubjectC) : nullptr;
}

LLVOAvatar* LLDirectorCast::resolveSubjectD()
{
    return mSubjectD.notNull() ? resolve(mSubjectD) : nullptr;
}

// ---------------------------------------------------------------------------
// groups
// ---------------------------------------------------------------------------
void LLDirectorCast::setGroup(const LLUUID& id, const std::string& name)
{
    if (CastMember* m = getMember(id))
    {
        const std::string old = m->mGroup;
        m->mGroup = name;   // "" = ungroup; no registry to keep in sync
        if (old != name)
        {
            pruneGroupDelay(old);   // retagging the last member retires the group
        }
    }
}

std::string LLDirectorCast::getGroup(const LLUUID& id) const
{
    const CastMember* m = getMember(id);
    return m ? m->mGroup : std::string();
}

std::vector<std::string> LLDirectorCast::getGroupNames() const
{
    // first-appearance cast order (NOT sorted): a combo rebuilt from this list
    // keeps its rows where the operator last saw them
    std::vector<std::string> names;
    for (const CastMember& m : mCast)
    {
        if (!m.mGroup.empty()
            && std::find(names.begin(), names.end(), m.mGroup) == names.end())
        {
            names.push_back(m.mGroup);
        }
    }
    return names;
}

uuid_vec_t LLDirectorCast::membersInGroup(const std::string& name) const
{
    uuid_vec_t ids;
    if (name.empty())
    {
        return ids;     // "" means ungrouped, never a startable group
    }
    for (const CastMember& m : mCast)
    {
        if (m.mGroup == name)
        {
            ids.push_back(m.mId);
        }
    }
    return ids;
}

bool LLDirectorCast::renameGroup(const std::string& old_name, const std::string& new_name)
{
    if (old_name.empty() || new_name.empty() || old_name == new_name)
    {
        return false;
    }
    bool any = false;
    for (CastMember& m : mCast)
    {
        if (m.mGroup == old_name)
        {
            m.mGroup = new_name;
            any = true;
        }
    }
    if (!any)
    {
        return false;   // unknown group: nothing carried the tag
    }
    // carry the start delay across. Renaming ONTO an existing group merges the
    // two; the target's own delay wins when it has one (the surviving name
    // keeps behaving the way the operator last saw it), else the source's
    // rides along.
    auto old_it = mGroupDelays.find(old_name);
    if (old_it != mGroupDelays.end())
    {
        if (mGroupDelays.find(new_name) == mGroupDelays.end())
        {
            mGroupDelays[new_name] = old_it->second;
        }
        mGroupDelays.erase(old_name);
    }
    return true;
}

bool LLDirectorCast::dissolveGroup(const std::string& name)
{
    if (name.empty())
    {
        return false;
    }
    bool any = false;
    for (CastMember& m : mCast)
    {
        if (m.mGroup == name)
        {
            m.mGroup.clear();   // untag only -- nobody leaves the cast
            any = true;
        }
    }
    mGroupDelays.erase(name);
    return any;
}

void LLDirectorCast::setGroupDelay(const std::string& name, F32 seconds)
{
    if (name.empty())
    {
        return;
    }
    if (seconds > 0.01f)
    {
        mGroupDelays[name] = seconds;
    }
    else
    {
        mGroupDelays.erase(name);   // 0 = no entry, so the map never bloats
    }
}

F32 LLDirectorCast::getGroupDelay(const std::string& name) const
{
    auto it = mGroupDelays.find(name);
    return it != mGroupDelays.end() ? it->second : 0.f;
}

void LLDirectorCast::pruneGroupDelay(const std::string& name)
{
    if (!name.empty() && membersInGroup(name).empty())
    {
        mGroupDelays.erase(name);
    }
}

// ---------------------------------------------------------------------------
// staggered starts (pending-start queue)
// ---------------------------------------------------------------------------
void LLDirectorCast::startMovesStaggered(const uuid_vec_t& ids)
{
    LLActorMover& mover = LLActorMover::instance();
    if (ids.empty())
    {
        // mirrors startAll(): an empty cast still means "my avatar" (self is
        // never grouped, so there is nothing to stagger)
        mover.start(LLUUID::null);
        return;
    }

    const F64 now = LLTimer::getElapsedSeconds().value();
    std::vector<F32> delays;    // distinct delays queued by THIS call
    for (const LLUUID& id : ids)
    {
        const F32 delay = getGroupDelay(getGroup(id));
        if (delay <= 0.01f)
        {
            mover.start(id);    // undelayed: byte-identical to the old loop
            continue;
        }
        // re-queue rather than double-queue on a repeated Start: drop any
        // older pending entry for this member first
        cancelPendingStart(id);
        mPendingStarts.push_back({ id, now + delay });
        if (std::find(delays.begin(), delays.end(), delay) == delays.end())
        {
            delays.push_back(delay);
        }
    }
    // one self-deleting one-shot per distinct delay; each fire sweeps the
    // whole queue for everything due (so a late timer still starts everyone
    // it should, and a timer whose members were canceled is a clean no-op).
    // The lambda removes ITS OWN pointer from mStaggerTimers before anything
    // else -- after tick() returns, updateClass() deletes the timer object
    // (run_after contract), so this is the same "pointer in the list is live
    // by construction" discipline mCountdownTimer uses. The shared cell only
    // exists because the pointer is not known until run_after returns.
    for (F32 delay : delays)
    {
        auto cell = std::make_shared<LLEventTimer*>(nullptr);
        *cell = LLEventTimer::run_after(delay, [this, cell]()
        {
            mStaggerTimers.erase(
                std::remove(mStaggerTimers.begin(), mStaggerTimers.end(), *cell),
                mStaggerTimers.end());
            servicePendingStarts();
        });
        mStaggerTimers.push_back(*cell);
    }
    if (!mPendingStarts.empty())
    {
        LL_INFOS("DirectorCast") << mPendingStarts.size()
                                 << " staggered start(s) queued" << LL_ENDL;
    }
}

void LLDirectorCast::servicePendingStarts()
{
    // start everything due (small epsilon: LLEventTimer fires on frame
    // granularity, so a fire can land a hair before its own deadline)
    const F64 now = LLTimer::getElapsedSeconds().value() + 0.02;
    LLActorMover& mover = LLActorMover::instance();
    for (size_t i = 0; i < mPendingStarts.size(); )
    {
        if (mPendingStarts[i].mDueAt <= now)
        {
            const LLUUID id = mPendingStarts[i].mId;
            mPendingStarts.erase(mPendingStarts.begin() + i);
            mover.start(id);    // may re-enter cancelPendingStart(id): erase FIRST
        }
        else
        {
            ++i;
        }
    }
}

void LLDirectorCast::cancelPendingStarts()
{
    mPendingStarts.clear();
    // every pointer in the list is live by construction (a fired timer
    // removed itself first), so deleting cancels the pending one-shots
    for (LLEventTimer* t : mStaggerTimers)
    {
        delete t;
    }
    mStaggerTimers.clear();
}

void LLDirectorCast::cancelPendingStart(const LLUUID& id)
{
    // the member leaves the queue; its timer (possibly shared with others)
    // stays scheduled and fires as a no-op sweep -- harmless and simpler than
    // reference-counting timers per member
    mPendingStarts.erase(
        std::remove_if(mPendingStarts.begin(), mPendingStarts.end(),
                       [&id](const PendingStart& p) { return p.mId == id; }),
        mPendingStarts.end());
}

// ---------------------------------------------------------------------------
// marks
// ---------------------------------------------------------------------------
void LLDirectorCast::setMarks()
{
    for (CastMember& m : mCast)
    {
        LLVOAvatar* av = resolve(m.mId);
        if (av && av->getRootJoint())
        {
            // current RENDERED root, so a mark taken mid ghost-move (or on
            // a posed actor) captures what the camera actually sees
            m.mMark = av->getRootJoint()->getWorldPosition();
            m.mHasMark = true;
        }
    }
}

void LLDirectorCast::resetToMarks()
{
    for (const CastMember& m : mCast)
    {
        if (m.mHasMark)
        {
            // ActorMover-style local root placement: stops any active move,
            // then pins the rendered root at the mark until the next
            // walk/stop (nothing is sent to the sim)
            LLActorMover::instance().placeAt(m.mId, m.mMark);
        }
    }
}

bool LLDirectorCast::setMark(const LLUUID& id)
{
    CastMember* m = getMember(id);
    if (!m)
    {
        return false;   // only cast members carry marks
    }
    LLVOAvatar* av = resolve(id);
    if (!av || !av->getRootJoint())
    {
        return false;
    }
    // same capture as setMarks(): the current RENDERED root
    m->mMark = av->getRootJoint()->getWorldPosition();
    m->mHasMark = true;
    return true;
}

bool LLDirectorCast::resetToMark(const LLUUID& id)
{
    const CastMember* m = getMember(id);
    if (!m || !m->mHasMark)
    {
        return false;
    }
    LLActorMover::instance().placeAt(id, m->mMark);
    return true;
}

// ---------------------------------------------------------------------------
// scene serialization
// ---------------------------------------------------------------------------
LLSD LLDirectorCast::sceneData() const
{
    LLSD data = LLSD::emptyMap();
    LLSD cast_arr = LLSD::emptyArray();
    for (const CastMember& m : mCast)
    {
        LLSD e = LLSD::emptyMap();
        e["id"] = m.mId;
        e["name"] = m.mLastName;
        e["has_mark"] = m.mHasMark;
        if (m.mHasMark)
        {
            e["mark"] = ll_sd_from_vector3(m.mMark);
        }
        e["loco_anim"] = m.mLocoAnim;
        e["group"] = m.mGroup;
        e["look_at_camera"] = isLookAtCamera(m.mId);

        LLSD gaze_sd = LLSD::emptyMap();
        gaze_sd["mode"] = static_cast<S32>(m.mGazeTarget.mMode);
        if (m.mGazeTarget.mCastRef.notNull())
        {
            gaze_sd["cast_ref"] = m.mGazeTarget.mCastRef;
        }
        if (!m.mGazeTarget.mFixedPoint.isExactlyZero())
        {
            gaze_sd["fixed_point"] = ll_sd_from_vector3d(m.mGazeTarget.mFixedPoint);
        }
        if (m.mGazeTarget.mObjectRef.notNull())
        {
            gaze_sd["object_ref"] = m.mGazeTarget.mObjectRef;
        }
        writeGazeExpression(gaze_sd, m.mGazeTarget);
        e["gaze_target"] = gaze_sd;
        if (m.mEyeGazeTargetEnabled)
        {
            e["eye_gaze_target"] = writeGazeAimTarget(m.mEyeGazeTarget);
        }
        if (!m.mGazeCues.empty())
        {
            e["gaze_cues"] = writeGazeCues(m.mGazeCues);
        }
        if (!m.mGazeInfluenceKeys.empty())
        {
            e["gaze_influence_keys"] = writeGazeInfluenceKeys(m.mGazeInfluenceKeys);
        }

        cast_arr.append(e);
    }
    data["cast"] = cast_arr;
    data["subject_a"] = mSubjectA;
    data["subject_b"] = mSubjectB;
    data["subject_c"] = mSubjectC;
    data["subject_d"] = mSubjectD;

    LLSD self_gaze_sd = LLSD::emptyMap();
    self_gaze_sd["mode"] = static_cast<S32>(mSelfGazeTarget.mMode);
    self_gaze_sd["look_at_camera"] = mSelfLookAtCamera;
    if (mSelfGazeTarget.mCastRef.notNull())
    {
        self_gaze_sd["cast_ref"] = mSelfGazeTarget.mCastRef;
    }
    if (!mSelfGazeTarget.mFixedPoint.isExactlyZero())
    {
        self_gaze_sd["fixed_point"] = ll_sd_from_vector3d(mSelfGazeTarget.mFixedPoint);
    }
    if (mSelfGazeTarget.mObjectRef.notNull())
    {
        self_gaze_sd["object_ref"] = mSelfGazeTarget.mObjectRef;
    }
    writeGazeExpression(self_gaze_sd, mSelfGazeTarget);
    data["self_gaze_target"] = self_gaze_sd;
    if (mSelfEyeGazeTargetEnabled)
    {
        data["self_eye_gaze_target"] = writeGazeAimTarget(mSelfEyeGazeTarget);
    }
    if (!mSelfGazeCues.empty())
    {
        data["self_gaze_cues"] = writeGazeCues(mSelfGazeCues);
    }
    if (!mSelfGazeInfluenceKeys.empty())
    {
        data["self_gaze_influence_keys"] = writeGazeInfluenceKeys(mSelfGazeInfluenceKeys);
    }

    // per-group start delays, only for groups that still exist (the members
    // above carry the tags; this map just annotates them)
    LLSD delays = LLSD::emptyMap();
    for (const std::string& name : getGroupNames())
    {
        const F32 d = getGroupDelay(name);
        if (d > 0.01f)
        {
            delays[name] = d;
        }
    }
    data["group_delays"] = delays;
    return data;
}

void LLDirectorCast::applySceneData(const LLSD& data)
{
    // replace membership wholesale; a running transport is the caller's
    // problem (the console cuts before loading). Queued staggered starts and
    // group delays belong to the outgoing cast, so both reset here.
    cancelPendingStarts();
    LLActorMover::instance().clearAllDirectorLookAtRuntime();
    mGroupDelays.clear();
    mCast.clear();
    mIds.clear();
    mMemberIndex.clear();
    mLookAtCameraIds.clear();
    mSelfLookAtCamera = false;
    mSubjectA.setNull();
    mSubjectB.setNull();
    mSubjectC.setNull();
    mSubjectD.setNull();
    mSelfGazeTarget = LLActorMover::GazeTarget();
    mSelfGazeTarget.mMode = LLActorMover::GazeTarget::CAMERA;
    mSelfEyeGazeTarget = LLActorMover::GazeTarget();
    mSelfEyeGazeTargetEnabled = false;
    mSelfGazeCues.clear();
    mSelfGazeInfluenceKeys.clear();

    if (data.has("self_gaze_target"))
    {
        const LLSD& sg = data["self_gaze_target"];
        if (sg.has("look_at_camera"))
        {
            mSelfLookAtCamera = sg["look_at_camera"].asBoolean();
        }
        if (sg.has("mode"))
        {
            mSelfGazeTarget.mMode = static_cast<LLActorMover::GazeTarget::EMode>(
                llclamp(sg["mode"].asInteger(),
                        static_cast<S32>(LLActorMover::GazeTarget::MOTION),
                        static_cast<S32>(LLActorMover::GazeTarget::OBJECT)));
        }
        if (sg.has("cast_ref"))
        {
            mSelfGazeTarget.mCastRef = sg["cast_ref"].asUUID();
        }
        if (sg.has("fixed_point"))
        {
            mSelfGazeTarget.mFixedPoint = ll_vector3d_from_sd(sg["fixed_point"]);
        }
        if (sg.has("object_ref"))
        {
            mSelfGazeTarget.mObjectRef = sg["object_ref"].asUUID();
        }
        readGazeExpression(sg, mSelfGazeTarget);
        LLActorMover::instance().setGazeTargetConfig(LLUUID::null, mSelfGazeTarget);
    }
    if (data.has("self_eye_gaze_target"))
    {
        mSelfEyeGazeTargetEnabled = readEyeGazeAimTarget(
            data["self_eye_gaze_target"], mSelfEyeGazeTarget);
    }
    if (data.has("self_gaze_cues"))
    {
        setGazeCues(LLUUID::null, readGazeCues(data["self_gaze_cues"]));
    }
    if (data.has("self_gaze_influence_keys"))
    {
        mSelfGazeInfluenceKeys = readGazeInfluenceKeys(data["self_gaze_influence_keys"]);
    }

    const LLSD& cast_arr = data["cast"];
    for (LLSD::array_const_iterator it = cast_arr.beginArray();
         it != cast_arr.endArray(); ++it)
    {
        const LLSD& e = *it;
        CastMember m;
        m.mId = e["id"].asUUID();
        if (m.mId.isNull() || contains(m.mId))
        {
            continue;
        }
        m.mLastName = e["name"].asString();
        m.mHasMark = e["has_mark"].asBoolean();
        if (m.mHasMark && e.has("mark"))
        {
            m.mMark = ll_vector3_from_sd(e["mark"]);
        }
        m.mLocoAnim = e["loco_anim"].asUUID();
        m.mGroup = e["group"].asString();   // absent in pre-group scenes -> ""
        if (e.has("gaze_target"))
        {
            const LLSD& gaze_sd = e["gaze_target"];
            if (gaze_sd.has("mode"))
            {
                m.mGazeTarget.mMode = static_cast<LLActorMover::GazeTarget::EMode>(
                    llclamp(gaze_sd["mode"].asInteger(),
                            static_cast<S32>(LLActorMover::GazeTarget::MOTION),
                            static_cast<S32>(LLActorMover::GazeTarget::OBJECT)));
            }
            if (gaze_sd.has("cast_ref"))
            {
                m.mGazeTarget.mCastRef = gaze_sd["cast_ref"].asUUID();
            }
            if (gaze_sd.has("fixed_point"))
            {
                m.mGazeTarget.mFixedPoint = ll_vector3d_from_sd(gaze_sd["fixed_point"]);
            }
            if (gaze_sd.has("object_ref"))
            {
                m.mGazeTarget.mObjectRef = gaze_sd["object_ref"].asUUID();
            }
            readGazeExpression(gaze_sd, m.mGazeTarget);
        }
        if (e.has("eye_gaze_target"))
        {
            m.mEyeGazeTargetEnabled = readEyeGazeAimTarget(
                e["eye_gaze_target"], m.mEyeGazeTarget);
        }
        const GazeCueList loaded_gaze_cues = e.has("gaze_cues")
            ? readGazeCues(e["gaze_cues"]) : GazeCueList();
        mCast.push_back(m);
        mIds.push_back(m.mId);
        mMemberIndex[m.mId] = mCast.size() - 1;
        if (e.has("gaze_cues"))
        {
            setGazeCues(m.mId, loaded_gaze_cues);
        }
        if (e.has("gaze_influence_keys"))
        {
            // No public setter for this lane (unlike setGazeCues -- it has no
            // revision/runtime-clear side effects to fire), so write it
            // straight into the member just pushed onto mCast.
            mCast.back().mGazeInfluenceKeys = readGazeInfluenceKeys(e["gaze_influence_keys"]);
        }
        if (e.has("gaze_target"))
        {
            LLActorMover::instance().setGazeTargetConfig(m.mId, m.mGazeTarget);
        }
        if (e["look_at_camera"].asBoolean()) // absent in older scenes -> false
        {
            mLookAtCameraIds.insert(m.mId);
        }
        // refresh the cached name when the actor is in world; a resolve
        // failure just leaves the member "(away)" -- never dropped
        resolve(m.mId);
    }

    // subjects only survive when they point into the loaded cast
    const LLUUID a = data["subject_a"].asUUID();
    const LLUUID b = data["subject_b"].asUUID();
    const LLUUID c = data["subject_c"].asUUID();
    const LLUUID d = data["subject_d"].asUUID();
    if (contains(a))
    {
        mSubjectA = a;
    }
    if (contains(b))
    {
        mSubjectB = b;
    }
    if (contains(c))
    {
        mSubjectC = c;
    }
    if (contains(d))
    {
        mSubjectD = d;
    }
    ++mGazeCueRevision;

    // group start delays (absent in pre-delay scenes); only names some loaded
    // member actually carries are kept -- no stale registry
    const LLSD& delays = data["group_delays"];
    if (delays.isMap())
    {
        for (LLSD::map_const_iterator it = delays.beginMap(); it != delays.endMap(); ++it)
        {
            if (!membersInGroup(it->first).empty())
            {
                setGroupDelay(it->first, (F32)it->second.asReal());
            }
        }
    }
}

// ---------------------------------------------------------------------------
// ACTION / CUT
// ---------------------------------------------------------------------------
bool LLDirectorCast::isCountingDown() const
{
    // non-null exactly while the one-shot is pending: the fire lambda nulls
    // it before running, and cancelCountdown() nulls it after deleting
    return mCountdownTimer != nullptr;
}

F32 LLDirectorCast::countdownRemaining() const
{
    if (!isCountingDown())
    {
        return 0.f;
    }
    return llmax(0.f, (F32)(mCountdownEndsAt - LLTimer::getElapsedSeconds().value()));
}

void LLDirectorCast::cancelCountdown()
{
    // safe: the pointer is only non-null while the timer is alive and
    // unfired (see isCountingDown()), so this delete never dangles
    delete mCountdownTimer;
    mCountdownTimer = nullptr;
}

void LLDirectorCast::action()
{
    if (mRunning || isCountingDown())
    {
        return;     // ACTION morphs to CUT in the UI; a second press is CUT
    }

    static LLCachedControl<F32> delay(gSavedSettings, "DirectorActionDelay", 0.f);
    const F32 d = llmax((F32)delay, 0.f);
    if (d > 0.01f)
    {
        // countdown so a solo operator can get into frame; the timer is a
        // self-deleting one-shot (run_after contract), so the lambda MUST
        // null our pointer before anything else -- after tick() returns,
        // updateClass() deletes the timer object
        mCountdownEndsAt = LLTimer::getElapsedSeconds().value() + d;
        mCountdownTimer = LLEventTimer::run_after(d, [this]()
        {
            mCountdownTimer = nullptr;
            fireAction();
        });
        LL_INFOS("DirectorCast") << "ACTION armed, firing in " << d << " s" << LL_ENDL;
    }
    else
    {
        fireAction();
    }
}

void LLDirectorCast::fireAction()
{
    static LLCachedControl<bool> arm_moves(gSavedSettings, "DirectorArmMoves", true);
    static LLCachedControl<bool> arm_camera(gSavedSettings, "DirectorArmCamera", true);
    static LLCachedControl<bool> arm_play(gSavedSettings, "DirectorArmRecorderPlay", false);
    static LLCachedControl<bool> arm_capture(gSavedSettings, "DirectorArmRecorderCapture", false);

    mFiredMoves = false;
    mFiredCamera = false;
    mFiredPlay = false;
    mFiredCapture = false;
    mCameraEnableTransient = false;

    // Synchronize a just-toggled arm/disarm before either owner snapshots the
    // effective camera control. This closes both ACTION->arm and arm->ACTION
    // ordering windows within one UI frame.
    ALDirectorSwitcher& switcher = ALDirectorSwitcher::instance();
    switcher.tick(
        LLPresentationTime::currentFrame().presentation_time);

    if (arm_moves)
    {
        // start-all-cast (self when the cast is empty), each move capturing
        // the shared parameters at start. Delay-aware: members of a group
        // carrying a start delay are queued and begin N seconds later
        // (staggered starts); with no delays configured this is
        // behavior-identical to the old startAll().
        startMovesStaggered(mIds);
        mFiredMoves = true;
    }
    if (arm_camera)
    {
        // enable the cinematic camera; orbit is untouched (it rides its own
        // enable + resolveAnchor()). Remember the prior state so CUT
        // restores it instead of hammering it off.
        const bool camera_enabled =
            gSavedSettings.getBOOL("CinematicCamEnabled");
        bool switcher_baseline = false;
        mCameraEnableTransient =
            switcher.cameraEnableBaseline(switcher_baseline);
        mCameraWasEnabled =
            mCameraEnableTransient
                ? switcher_baseline : camera_enabled;
        if (!camera_enabled)
        {
            if (mCameraEnableTransient)
            {
                if (LLControlVariable* control =
                        gSavedSettings.getControl("CinematicCamEnabled"))
                {
                    control->setValue(LLSD(true), false);
                }
            }
            else
            {
                gSavedSettings.setBOOL("CinematicCamEnabled", true);
            }
        }
        mFiredCamera = true;
    }
    if (arm_capture)
    {
        // capture wins when both recorder arms are set: playback would be
        // recording its own output over the take it is playing
        if (arm_play)
        {
            LL_WARNS("DirectorCast") << "recorder play AND capture armed; capturing"
                                     << LL_ENDL;
        }
        LLFlycamRecorder::instance().startRecording();
        mFiredCapture = true;
    }
    else if (arm_play)
    {
        LLFlycamRecorder::instance().startPlayback();
        mFiredPlay = true;
    }

    mRunning = true;
    // An armed switcher owns the effective CineCam program but cooperates with
    // this transport gate. Run after mRunning is published so a switcher first
    // armed on this ACTION can identify the transport-owned enable snapshot.
    switcher.onDirectorAction();
    LL_INFOS("DirectorCast") << "ACTION: moves " << mFiredMoves
                             << " camera " << mFiredCamera
                             << " play " << mFiredPlay
                             << " capture " << mFiredCapture << LL_ENDL;
}

void LLDirectorCast::cut()
{
    cancelCountdown();
    // queued staggered starts die with the take -- BEFORE the running check,
    // so a CUT pressed while only group-Start waves are pending (transport
    // never "running") still disarms them
    cancelPendingStarts();
    // CUT is also the switcher's emergency camera release, even when no other
    // Director subsystem currently marks the transport running.
    ALDirectorSwitcher::instance().onDirectorCut();
    if (!mRunning)
    {
        return;
    }
    if (mFiredMoves)
    {
        LLActorMover::instance().stopAll();
    }
    if (mFiredCamera && !mCameraWasEnabled)
    {
        // only un-enable what ACTION enabled: a camera the operator had
        // running before ACTION keeps running after CUT
        if (mCameraEnableTransient)
        {
            if (LLControlVariable* control =
                    gSavedSettings.getControl("CinematicCamEnabled"))
            {
                control->setValue(LLSD(false), false);
            }
        }
        else
        {
            gSavedSettings.setBOOL("CinematicCamEnabled", false);
        }
    }
    if (mFiredPlay)
    {
        LLFlycamRecorder::instance().stopPlayback();
    }
    if (mFiredCapture)
    {
        LLFlycamRecorder::instance().stopRecording();
    }
    mRunning = false;
    LL_INFOS("DirectorCast") << "CUT" << LL_ENDL;
}
