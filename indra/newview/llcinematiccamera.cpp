/**
 * @file llcinematiccamera.cpp
 * @brief Automated cinematic camera: bone-lock (GoPro) and motion patterns.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Phoenix / Firestorm Viewer Source Code
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "llcinematiccamera.h"

#include <cmath>

#include "alcameracurve.h"
#include "aldirectorswitcher.h"
#include "llagentpilot.h"
#include "llcameraoperator.h"
#include "llappviewer.h"            // gFrameIntervalSeconds
#include "lldirectorcast.h"         // [Director] Subject A/B/C/D
#include "llflycamrecorder.h"
#include "lljoint.h"
#include "llmath.h"
#include "llpanel.h"
#include "llpathcamera.h"
#include "llpresentationtime.h"
#include "llselectmgr.h"
#include "llviewerobjectlist.h"     // gObjectList (locked follow target)
#include "llviewercamera.h"
#include "llviewercontrol.h"        // gSavedSettings, LLCachedControl
#include "llviewerjoystick.h"
#include "llviewerobject.h"
#include "llvoavatar.h"
#include "llvoavatarself.h"         // gAgentAvatarp, isAgentAvatarValid()
#include "m3math.h"

namespace
{
constexpr F32 PHASE_WRAP = 4096.f;  // seconds of pattern clock before wrap
constexpr F32 AUTO_FRAME_MIN_DISTANCE = 0.3f;
constexpr F32 AUTO_FRAME_MAX_DISTANCE = 64.f;
constexpr F32 AUTO_FRAME_CROWN = 0.18f;
constexpr F32 AUTO_FRAME_EYE_BAND = 0.18f;

// The shared Frame panel is embedded directly by the Director Console. Keep
// the mode-2-only timing row synchronized in every live panel instance.
class ALCineCamFramePanel final : public LLPanel
{
public:
    bool postBuild() override
    {
        if (LLControlVariable* control =
                gSavedSettings.getControl("CinematicAutoFrameSettleMode"))
        {
            mSettleModeConnection = control->getCommitSignal()->connect(
                [this](LLControlVariable*, const LLSD&, const LLSD&)
                {
                    updateSettleTimeEnabled();
                });
        }
        if (LLControlVariable* control =
                gSavedSettings.getControl("CinematicAutoFrameEnabled"))
        {
            mAutoFrameEnabledConnection = control->getCommitSignal()->connect(
                [this](LLControlVariable*, const LLSD&, const LLSD&)
                {
                    updateSettleTimeEnabled();
                });
        }
        if (LLControlVariable* control =
                gSavedSettings.getControl("CinematicAutoFrameSettleCurve"))
        {
            mSettleCurveConnection = control->getCommitSignal()->connect(
                [this](LLControlVariable*, const LLSD&, const LLSD&)
                {
                    updateSettleTimeEnabled();
                });
        }
        updateSettleTimeEnabled();
        return true;
    }

private:
    void updateSettleTimeEnabled()
    {
        const bool enabled =
            gSavedSettings.getBOOL("CinematicAutoFrameEnabled") &&
            gSavedSettings.getS32("CinematicAutoFrameSettleMode") == 2;
        getChild<LLView>("auto_frame_settle_seconds")->setEnabled(enabled);
        getChild<LLView>("reset_CinematicAutoFrameSettleSeconds")
            ->setEnabled(enabled);
        const bool custom_curve =
            gSavedSettings.getBOOL("CinematicAutoFrameEnabled") &&
            gSavedSettings.getS32("CinematicAutoFrameSettleCurve") ==
                ALCameraCurve::CUSTOM_BEZIER;
        const char* curve_spinners[] = {
            "auto_frame_settle_bezier_x1",
            "auto_frame_settle_bezier_y1",
            "auto_frame_settle_bezier_x2",
            "auto_frame_settle_bezier_y2",
        };
        for (const char* name : curve_spinners)
        {
            getChild<LLView>(name)->setEnabled(custom_curve);
        }
    }

    boost::signals2::scoped_connection mSettleModeConnection;
    boost::signals2::scoped_connection mAutoFrameEnabledConnection;
    boost::signals2::scoped_connection mSettleCurveConnection;
};

static LLPanelInjector<ALCineCamFramePanel> sCineCamFramePanel(
    "panel_cinecam_frame");

inline F32 cc_frac(F32 x)               { return x - floorf(x); }
inline F32 cc_lerp(F32 a, F32 b, F32 u) { return a + (b - a) * u; }
inline F32 cc_smootherstep(F32 u)
{
    u = llclamp(u, 0.f, 1.f);
    return u * u * u * (u * (u * 6.f - 15.f) + 10.f);
}

// Preserve the mode-authored vertical FOV at the native window gate, then add
// the image-plane field change caused by mounting the same focal length behind
// a target gate with a different width. The virtual film back is 24 mm high,
// so its width is 24 * aspect and its half-height is 12 mm:
//
//   p_out = tan(vfov_plain / 2) + (12 / focal_mm) * (target / window - 1)
//   vfov_out = 2 * atan(p_out)
//
// This differential form is exactly identity at target == window and retains
// every cinematic mode's existing zoom/FOV authorship instead of replacing it.
bool cc_targetFrameAspect(F32& target_aspect)
{
    static LLCachedControl<F32> configured_aspect(
        gSavedSettings, "CinematicFrameAspectRatio", 0.f);
    static LLCachedControl<F32> custom_aspect(
        gSavedSettings, "CinematicFrameCustomRatio", 2.35f);

    target_aspect = configured_aspect;
    if (target_aspect == 0.f)
    {
        return false;
    }
    if (target_aspect < 0.f)
    {
        target_aspect = custom_aspect;
    }
    return true;
}

F32 cc_applyFrameLens(F32 plain_fov, LLViewerCamera* cam)
{
    static LLCachedControl<bool> lens_enabled(
        gSavedSettings, "CinematicFrameLensEnabled", false);
    if (!lens_enabled)
    {
        return plain_fov; // hard default-off identity path
    }

    static LLCachedControl<F32> focal_length_mm(
        gSavedSettings, "CinematicFrameFocalLengthMM", 50.f);

    F32 target_aspect = 0.f;
    if (!cc_targetFrameAspect(target_aspect))
    {
        return plain_fov; // Off / Native is the original path byte-for-byte
    }

    const F32 window_aspect = cam->getAspect();
    const F32 focal_mm = focal_length_mm;
    if (!std::isfinite(plain_fov) || !std::isfinite(target_aspect) ||
        !std::isfinite(window_aspect) || !std::isfinite(focal_mm) ||
        target_aspect <= 0.f || window_aspect <= 0.f || focal_mm <= 0.f)
    {
        const F32 fallback = std::isfinite(plain_fov)
            ? plain_fov : cam->getDefaultFOV();
        return llclamp(fallback, cam->getMinView(), cam->getMaxView());
    }

    // Avoid a tan/atan round trip at the native gate: this is an exact
    // identity for Custom == window and a stable identity for decimal labels
    // such as 1.78 on a 16:9 window.
    if (fabsf(target_aspect - window_aspect) <=
        0.0001f * llmax(target_aspect, window_aspect))
    {
        return llclamp(plain_fov, cam->getMinView(), cam->getMaxView());
    }

    const F32 projected_half_height =
        tanf(plain_fov * 0.5f) +
        (12.f / focal_mm) * (target_aspect / window_aspect - 1.f);
    const F32 framed_fov = 2.f * atanf(projected_half_height);
    if (!std::isfinite(projected_half_height) || projected_half_height <= 0.f ||
        !std::isfinite(framed_fov))
    {
        return llclamp(plain_fov, cam->getMinView(), cam->getMaxView());
    }
    return llclamp(framed_fov, cam->getMinView(), cam->getMaxView());
}

// Vertical FOV of the pixels that survive the delivery-frame crop. A wider
// target aspect letterboxes the rendered view and therefore retains only the
// window_aspect/target_aspect fraction of its image-plane half-height. A
// narrower target pillarboxes and retains the full vertical field.
F32 cc_retainedFrameFov(F32 plain_fov, LLViewerCamera* cam)
{
    const F32 rendered_fov = cc_applyFrameLens(plain_fov, cam);
    const F32 window_aspect = cam->getAspect();
    F32 target_aspect = window_aspect;
    cc_targetFrameAspect(target_aspect);
    if (!std::isfinite(rendered_fov) || !std::isfinite(window_aspect) ||
        !std::isfinite(target_aspect) || window_aspect <= 0.f ||
        target_aspect <= 0.f)
    {
        return rendered_fov;
    }
    const F32 retained_height = llmin(1.f, window_aspect / target_aspect);
    const F32 framed_fov = 2.f * atanf(
        tanf(rendered_fov * 0.5f) * retained_height);
    return std::isfinite(framed_fov) && framed_fov > 0.f
        ? framed_fov : rendered_fov;
}

LLVector3 cc_subjectBase(LLVOAvatar* av)
{
    if (LLJoint* root = av->getRootJoint())
    {
        LLVector3 foot = root->getWorldPosition();
        foot.mV[VZ] -= av->getPelvisToFoot();
        return foot;
    }
    return av->getPositionAgent();
}

LLVector3 cc_scaleAboutSubjectBase(LLVOAvatar* av, const LLVector3& point)
{
    const F32 scale = av->getUniformScale();
    if (scale == 1.f)
    {
        return point;
    }
    const LLVector3 base = cc_subjectBase(av);
    return base + (point - base) * scale;
}

bool cc_jointPoint(LLVOAvatar* av, const char* name, LLVector3& point)
{
    LLJoint* joint = av->getJoint(name);
    if (!joint)
    {
        return false;
    }
    point = cc_scaleAboutSubjectBase(av, joint->getWorldPosition());
    return point.isFinite();
}

bool cc_jointMidpoint(LLVOAvatar* av, const char* left_name,
                      const char* right_name, LLVector3& point)
{
    LLVector3 left;
    LLVector3 right;
    if (!cc_jointPoint(av, left_name, left) ||
        !cc_jointPoint(av, right_name, right))
    {
        return false;
    }
    point = (left + right) * 0.5f;
    return point.isFinite();
}

enum EAutoFrameShot
{
    AUTO_SHOT_FULL,
    AUTO_SHOT_MEDIUM,
    AUTO_SHOT_CLOSE,
    AUTO_SHOT_EYES,
    AUTO_SHOT_PRIMARY_FACE,
    AUTO_SHOT_NONE
};

EAutoFrameShot cc_autoFrameShot(S32 mode)
{
    switch (mode)
    {
        case LLCinematicCamera::MODE_BONE_LOCK:
        case LLCinematicCamera::MODE_CRANE:
        case LLCinematicCamera::MODE_DOLLY_ZOOM:
        case LLCinematicCamera::MODE_PUSH_IN:
        case LLCinematicCamera::MODE_OVERHEAD:
        case LLCinematicCamera::MODE_REVEAL:
        case LLCinematicCamera::MODE_PULL_BACK:
        case LLCinematicCamera::MODE_SPIRAL:
        case LLCinematicCamera::MODE_PEDESTAL:
        case LLCinematicCamera::MODE_CORKSCREW:
        case LLCinematicCamera::MODE_FISHEYE_LUNGE:
        case LLCinematicCamera::MODE_BOOST_RISE:
        case LLCinematicCamera::MODE_TILT_WHIP:
        case LLCinematicCamera::MODE_BODY_HELIX:
        case LLCinematicCamera::MODE_DESCENT:
        case LLCinematicCamera::MODE_BREATHING_HOLD:
            return AUTO_SHOT_NONE;
        case LLCinematicCamera::MODE_ECU_EYES:
        case LLCinematicCamera::MODE_FLOATING_ECU:
            return AUTO_SHOT_EYES;
        case LLCinematicCamera::MODE_OTS:
        case LLCinematicCamera::MODE_TWO_SHOT:
            return AUTO_SHOT_PRIMARY_FACE;
        case LLCinematicCamera::MODE_CRASH_ZOOM:
        case LLCinematicCamera::MODE_SLOW_ZOOM:
        case LLCinematicCamera::MODE_DETAIL_SWEEP:
        case LLCinematicCamera::MODE_STATIC_CLOSE:
        case LLCinematicCamera::MODE_STATIC_PROFILE_L:
        case LLCinematicCamera::MODE_STATIC_PROFILE_R:
            return AUTO_SHOT_CLOSE;
        case LLCinematicCamera::MODE_LOW_HERO:
        case LLCinematicCamera::MODE_WHIP_ARC:
        case LLCinematicCamera::MODE_ARC:
        case LLCinematicCamera::MODE_LEAD_FOLLOW:
        case LLCinematicCamera::MODE_BARREL_ROLL:
        case LLCinematicCamera::MODE_PENDULUM:
        case LLCinematicCamera::MODE_PARALLAX_SLIDE:
        case LLCinematicCamera::MODE_STATIC_MEDIUM:
        case LLCinematicCamera::MODE_STATIC_LOW:
        case LLCinematicCamera::MODE_STATIC_HIGH:
            return AUTO_SHOT_MEDIUM;
        default:
            return AUTO_SHOT_FULL;
    }
}

bool cc_autoFrameAnchors(LLVOAvatar* av, S32 mode, LLVector3& top,
                         LLVector3& bottom, bool& eye_level)
{
    const EAutoFrameShot shot = cc_autoFrameShot(mode);
    const F32 subject_scale = av->getUniformScale();
    eye_level = shot == AUTO_SHOT_EYES;
    if (shot == AUTO_SHOT_NONE || !std::isfinite(subject_scale) ||
        subject_scale <= 0.01f)
    {
        return false;
    }

    if (shot == AUTO_SHOT_EYES)
    {
        LLVector3 left_eye;
        LLVector3 right_eye;
        F32 eye_band = AUTO_FRAME_EYE_BAND * subject_scale;
        if (cc_jointPoint(av, "mEyeLeft", left_eye) &&
            cc_jointPoint(av, "mEyeRight", right_eye))
        {
            top = (left_eye + right_eye) * 0.5f;
            // Eye separation is the skeleton's available face-scale measure;
            // turn it into a compact vertical eye band with sane limits.
            eye_band = llclamp(
                (left_eye - right_eye).magVec() * 1.5f,
                0.12f * subject_scale, 0.24f * subject_scale);
        }
        else if (!cc_jointPoint(av, "mHead", top))
        {
            return false;
        }
        bottom = top - LLVector3(0.f, 0.f, eye_band);
        return bottom.isFinite();
    }

    if (!cc_jointPoint(av, "mHead", top))
    {
        return false;
    }
    top.mV[VZ] += AUTO_FRAME_CROWN * subject_scale;

    if (shot == AUTO_SHOT_FULL)
    {
        if (!cc_jointMidpoint(av, "mAnkleLeft", "mAnkleRight", bottom))
        {
            bottom = cc_subjectBase(av);
        }
    }
    else if (shot == AUTO_SHOT_MEDIUM)
    {
        LLVector3 hips;
        LLVector3 knees;
        if (cc_jointMidpoint(av, "mHipLeft", "mHipRight", hips) &&
            cc_jointMidpoint(av, "mKneeLeft", "mKneeRight", knees))
        {
            bottom = (hips + knees) * 0.5f;
        }
        else if (!cc_jointPoint(av, "mPelvis", bottom))
        {
            return false;
        }
    }
    else
    {
        if (!cc_jointMidpoint(av, "mCollarLeft", "mCollarRight", bottom) &&
            !cc_jointPoint(av, "mChest", bottom) &&
            !cc_jointMidpoint(av, "mShoulderLeft", "mShoulderRight", bottom))
        {
            return false;
        }
    }
    const F32 height = top.mV[VZ] - bottom.mV[VZ];
    return top.isFinite() && bottom.isFinite() && height > 0.01f &&
        ((shot != AUTO_SHOT_FULL && shot != AUTO_SHOT_MEDIUM) ||
         height >= 0.3f * subject_scale);
}

// small periodic value noise (same construction as the camera operator)
F32 cc_hash(F32 p)
{
    p = p - floorf(p / 256.f) * 256.f;
    p = cc_frac(p * 0.1031f);
    p *= p + 33.33f;
    p *= p + p;
    return cc_frac(p);
}

F32 cc_noise(F32 x)
{
    F32 i = floorf(x);
    F32 f = cc_frac(x);
    F32 u = f * f * f * (f * (f * 6.f - 15.f) + 10.f);
    return cc_lerp(cc_hash(i), cc_hash(i + 1.f), u) - 0.5f;
}

F32 cc_fbm(F32 x)
{
    return (cc_noise(x) + 0.5f * cc_noise(x * 2.f + 17.3f) + 0.25f * cc_noise(x * 4.f + 41.9f)) * 1.4f;
}

// build a level (Z-up) look-at orientation: X=at, Y=left, Z=up
LLQuaternion cc_lookAt(const LLVector3& from, const LLVector3& to)
{
    LLVector3 at = to - from;
    if (at.magVecSquared() < 1e-8f)
    {
        return LLQuaternion();
    }
    at.normVec();
    LLVector3 up_world(0.f, 0.f, 1.f);
    LLVector3 left = up_world % at;     // cross: y = z x x
    if (left.magVecSquared() < 1e-6f)   // looking straight up/down
    {
        left = LLVector3(0.f, 1.f, 0.f);
    }
    left.normVec();
    LLVector3 up = at % left;
    LLMatrix3 mat;
    mat.setRows(at, left, up);
    return LLQuaternion(mat);
}

// strip roll from an orientation, keeping its at-axis (horizon lock)
LLQuaternion cc_levelHorizon(const LLQuaternion& q)
{
    LLMatrix3 m(q);
    LLVector3 at(m.mMatrix[0]);
    LLVector3 up_world(0.f, 0.f, 1.f);
    LLVector3 left = up_world % at;
    if (left.magVecSquared() < 1e-6f)
    {
        return q;   // degenerate (looking straight up/down): keep as-is
    }
    left.normVec();
    LLVector3 up = at % left;
    LLMatrix3 level;
    level.setRows(at, left, up);
    return LLQuaternion(level);
}
} // anonymous namespace

// ---------------------------------------------------------------------------
LLCinematicCamera& LLCinematicCamera::instance()
{
    static LLCinematicCamera sInstance;
    return sInstance;
}

//static
F32 LLCinematicCamera::applyFrameLens(F32 plain_fov, LLViewerCamera* cam)
{
    return cc_applyFrameLens(plain_fov, cam);
}

void LLCinematicCamera::requestAutoReframe()
{
    ++mAutoFrameRequestSerial;
}

//static
S32 LLCinematicCamera::migrateLegacyMode(S32 mode)
{
    // Step Orbit was persisted as 40 before being folded back into Orbit.
    return mode == 40 ? MODE_ORBIT : mode;
}

//static
const char* LLCinematicCamera::modeName(S32 mode)
{
    static const char* const names[] = {
        "Off", "Bone Lock", "Orbit", "Fly Hover", "Sweep", "Crane",
        "Dolly Zoom", "Push-In", "Low Hero", "Overhead",
        "Over-the-Shoulder", "Crash Zoom", "Slow Zoom", "Whip Arc",
        "Arc Move", "Reveal Rise", "Pull-Back", "Two-Shot",
        "Lead Follow", "ECU Eyes", "Long Lens", "Spiral",
        "Pedestal Rise", "Barrel Roll", "Corkscrew", "Pendulum",
        "Contra-Orbit", "Fisheye Lunge", "Floor Skimmer",
        "Boost Rise", "Boom Over", "Top Spin", "Turntable Crane",
        "Floating Close-Up", "Tilt Whip", "Body Helix Reveal",
        "Descent Pedestal", "Parallax Slide", "Figure-8",
        "Detail Sweep", "Unknown", "Cable Cam Fly-by",
        "Breathing Hold", "Static Wide", "Static Medium",
        "Static Close", "Static Profile Left", "Static Profile Right",
        "Static Low Hero", "Static High Angle", "Static Full Body",
    };
    constexpr S32 count = (S32)(sizeof(names) / sizeof(names[0]));
    static_assert(count == MODE_STATIC_FULL + 1,
                  "Every persisted CineCam mode needs one stable label");
    mode = migrateLegacyMode(mode);
    return mode >= 0 && mode < count ? names[mode] : "Unknown";
}

// session-only locked follow subject
static LLUUID sCinematicFollowTarget;

//static
void LLCinematicCamera::toggleFollowTarget(const LLUUID& id)
{
    sCinematicFollowTarget = (sCinematicFollowTarget == id) ? LLUUID::null : id;
}

//static
bool LLCinematicCamera::isFollowTarget(const LLUUID& id)
{
    return id.notNull() && sCinematicFollowTarget == id;
}

//static
void LLCinematicCamera::onRuntimeTargetReplaced(const LLUUID& old_id,
                                                 const LLUUID& new_id)
{
    if (old_id.notNull() && sCinematicFollowTarget == old_id)
    {
        sCinematicFollowTarget = new_id;
    }
}

bool LLCinematicCamera::isActive() const
{
    static LLCachedControl<bool> enabled(gSavedSettings, "CinematicCamEnabled", false);
    static LLCachedControl<S32>  mode(gSavedSettings, "CinematicCamMode", 1);
    const S32 configured_mode = mode;
    const S32 migrated_mode = migrateLegacyMode(configured_mode);
    if (migrated_mode != configured_mode)
    {
        gSavedSettings.setS32("CinematicCamMode", migrated_mode);
    }
    if (!enabled)
    {
        return false;
    }
    const ALDirectorSwitcher& switcher = ALDirectorSwitcher::instance();
    const S32 effective_mode =
        switcher.isDrivingCamera() ? switcher.activeMode() : migrated_mode;
    if (effective_mode <= MODE_OFF || effective_mode > MODE_STATIC_FULL)
    {
        return false;
    }
    return resolveTarget() != nullptr;
}

bool LLCinematicCamera::isActiveBoneLockTarget(const LLUUID& avatar_id) const
{
    static LLCachedControl<bool> enabled(gSavedSettings, "CinematicCamEnabled", false);
    static LLCachedControl<S32>  mode(gSavedSettings, "CinematicCamMode", 1);
    if (avatar_id.isNull() || !enabled)
    {
        return false;
    }
    const ALDirectorSwitcher& switcher = ALDirectorSwitcher::instance();
    const S32 effective_mode =
        switcher.isDrivingCamera() ? switcher.activeMode() : (S32)mode;
    if (effective_mode != MODE_BONE_LOCK)
    {
        return false;
    }
    LLVOAvatar* target = resolveTarget();
    return target && !target->isDead() && target->getID() == avatar_id;
}

bool LLCinematicCamera::isActiveHeadFramingTarget(const LLUUID& avatar_id) const
{
    static LLCachedControl<bool> enabled(gSavedSettings, "CinematicCamEnabled", false);
    static LLCachedControl<S32>  mode(gSavedSettings, "CinematicCamMode", 1);
    static LLCachedControl<bool> look_at_head(gSavedSettings, "CinematicCamLookAtHead", true);
    if (avatar_id.isNull() || !enabled || !look_at_head)
    {
        return false;
    }
    const ALDirectorSwitcher& switcher = ALDirectorSwitcher::instance();
    const S32 effective_mode =
        switcher.isDrivingCamera() ? switcher.activeMode() : (S32)mode;
    if (effective_mode <= MODE_OFF || effective_mode > MODE_STATIC_FULL ||
        effective_mode == MODE_BONE_LOCK)
    {
        return false;
    }
    LLVOAvatar* target = resolveTarget();
    return target && !target->isDead() && target->getID() == avatar_id;
}

bool LLCinematicCamera::isActiveOrbitAnchor(const LLUUID& avatar_id) const
{
    static LLCachedControl<bool> orbit_enabled(
        gSavedSettings, "FlycamOrbitEnabled", false);
    if (avatar_id.isNull() || !orbit_enabled ||
        !LLViewerJoystick::getInstance()->getOverrideCamera())
    {
        return false;
    }

    // Mirror the idle camera-owner precedence. An engaged flycam is not the
    // active driver while any higher-priority camera owns this frame.
    if ((gAgentPilot.isPlaying() && gAgentPilot.getOverrideCamera()) ||
        LLFlycamRecorder::instance().isPlaybackActive() ||
        LLPathCamera::instance().isActive() || isActive())
    {
        return false;
    }

    LLVOAvatar* target = resolveDefaultTarget();
    return target && !target->isDead() && target->getID() == avatar_id;
}

LLVOAvatar* LLCinematicCamera::resolveTarget() const
{
    const ALDirectorSwitcher& switcher = ALDirectorSwitcher::instance();
    if (switcher.isDrivingCamera())
    {
        const S32 subject = switcher.activePrimarySubject();
        if (subject != ALDirectorSwitcher::SUBJECT_DEFAULT)
        {
            // A missing/dead per-slot mark falls through to the exact legacy
            // target chain rather than dropping camera ownership.
            if (LLVOAvatar* marked = resolveMarkedTarget(subject))
            {
                return marked;
            }
        }
    }
    return resolveDefaultTarget();
}

LLVOAvatar* LLCinematicCamera::resolveDefaultTarget() const
{
    // [Director] Subject A beats everything while set and alive; unset (or
    // out-of-world) falls through to the stock chain, so an empty cast is
    // byte-identical to pre-Director behavior.
    if (LLVOAvatar* subject = LLDirectorCast::instance().resolveSubjectA())
    {
        return subject;
    }

    // locked follow subject wins (session-only, set from the avatar context
    // menu); a dead/derezzed subject falls through rather than dropping out
    if (sCinematicFollowTarget.notNull())
    {
        LLViewerObject* obj = gObjectList.findObject(sCinematicFollowTarget);
        LLVOAvatar* av = obj ? obj->asAvatar() : nullptr;
        if (av && !av->isDead())
        {
            return av;
        }
    }

    static LLCachedControl<bool> use_selected(gSavedSettings, "CinematicCamUseSelected", false);
    if (use_selected)
    {
        LLViewerObject* obj = LLSelectMgr::getInstance()->getSelection()->getPrimaryObject();
        if (obj)
        {
            LLVOAvatar* av = obj->getAvatar();  // avatar itself or attachment parent
            if (av && !av->isDead())
            {
                return av;
            }
        }
        // fall through to self so the camera doesn't drop out mid-shot
    }
    return isAgentAvatarValid() ? (LLVOAvatar*)gAgentAvatarp : nullptr;
}

LLVOAvatar* LLCinematicCamera::resolveMarkedTarget(S32 subject) const
{
    LLDirectorCast& cast = LLDirectorCast::instance();
    switch (subject)
    {
        case ALDirectorSwitcher::SUBJECT_A: return cast.resolveSubjectA();
        case ALDirectorSwitcher::SUBJECT_B: return cast.resolveSubjectB();
        case ALDirectorSwitcher::SUBJECT_C: return cast.resolveSubjectC();
        case ALDirectorSwitcher::SUBJECT_D: return cast.resolveSubjectD();
        default: return nullptr;
    }
}

LLVOAvatar* LLCinematicCamera::resolveSecondaryTarget() const
{
    const ALDirectorSwitcher& switcher = ALDirectorSwitcher::instance();
    if (switcher.isDrivingCamera())
    {
        if (LLVOAvatar* marked =
                resolveMarkedTarget(switcher.activeSecondarySubject()))
        {
            return marked;
        }
    }
    // Legacy A-over-B dialogue behavior and fail-soft fallback.
    return LLDirectorCast::instance().resolveSubjectB();
}

// anchor transform for external riders (Flycam Orbit): same target/joint
// resolution as Bone Lock, without the mount/aim trim
bool LLCinematicCamera::resolveAnchor(LLVector3& pos, LLQuaternion& rot, bool level_horizon) const
{
    // Flycam Orbit is an external rider, not a switcher-owned CineCam shot.
    // Preserve its legacy A/follow/selected/self anchor while slots target
    // their own marks inside the cinematic-camera path.
    LLVOAvatar* av = resolveDefaultTarget();
    if (!av)
    {
        return false;
    }

    static LLCachedControl<std::string> joint_name(gSavedSettings, "CinematicCamJoint", std::string("mHead"));
    LLJoint* joint = av->getJoint(std::string(joint_name));
    if (!joint)
    {
        joint = av->getJoint("mHead");
    }
    if (joint)
    {
        pos = joint->getWorldPosition();
        rot = joint->getWorldRotation();
    }
    else
    {
        pos = av->getPositionAgent() + LLVector3(0.f, 0.f, 1.f);
        rot = av->getRenderRotation();
    }
    if (level_horizon)
    {
        rot = cc_levelHorizon(rot);
    }
    pos = cc_scaleAboutSubjectBase(av, pos);
    return true;
}

// ---------------------------------------------------------------------------
// pattern generators (agent region coordinates, Z up)
// ---------------------------------------------------------------------------
void LLCinematicCamera::patternBoneLock(LLVOAvatar* av, F32 /*phase*/,
                                        LLVector3& pos, LLQuaternion& rot, bool& have_rot)
{
    static LLCachedControl<std::string> joint_name(gSavedSettings, "CinematicCamJoint", std::string("mHead"));
    static LLCachedControl<F32> off_fwd(gSavedSettings, "CinematicCamBoneOffsetForward", 0.1f);
    static LLCachedControl<F32> off_left(gSavedSettings, "CinematicCamBoneOffsetLeft", 0.f);
    static LLCachedControl<F32> off_up(gSavedSettings, "CinematicCamBoneOffsetUp", 0.05f);
    static LLCachedControl<F32> aim_yaw(gSavedSettings, "CinematicCamBoneAimYaw", 0.f);
    static LLCachedControl<F32> aim_pitch(gSavedSettings, "CinematicCamBoneAimPitch", 0.f);
    static LLCachedControl<F32> aim_roll(gSavedSettings, "CinematicCamBoneAimRoll", 0.f);
    static LLCachedControl<bool> horizon(gSavedSettings, "CinematicCamBoneHorizonLock", false);

    LLJoint* joint = av->getJoint(std::string(joint_name));
    if (!joint)
    {
        joint = av->getJoint("mHead");
    }
    if (!joint)
    {
        pos = av->getPositionAgent() + LLVector3(0.f, 0.f, 1.f);
        have_rot = false;
        return;
    }

    LLQuaternion jrot = joint->getWorldRotation();
    // aim trim (degrees) applied in the joint's local frame
    LLQuaternion trim;
    trim.setEulerAngles(aim_roll * DEG_TO_RAD, aim_pitch * DEG_TO_RAD, aim_yaw * DEG_TO_RAD);
    rot = trim * jrot;
    if (horizon)
    {
        rot = cc_levelHorizon(rot);
    }
    have_rot = true;

    // local mount offset in the (trimmed) camera frame: X=at, Y=left, Z=up
    LLVector3 offset = LLVector3(off_fwd, off_left, off_up) * rot;
    pos = joint->getWorldPosition() + offset;
}

LLVector3 LLCinematicCamera::patternOrbit(const LLVector3& center, F32 phase)
{
    static LLCachedControl<F32> radius(gSavedSettings, "CinematicCamOrbitRadius", 3.f);
    static LLCachedControl<F32> speed(gSavedSettings, "CinematicCamOrbitSpeed", 20.f);   // deg/s
    static LLCachedControl<F32> height(gSavedSettings, "CinematicCamOrbitHeight", 0.5f);
    static LLCachedControl<F32> bob(gSavedSettings, "CinematicCamOrbitBob", 0.f);

    const F32 a = motionStartAzimuth(0.f) +
                  mMotionDir * phase * speed * DEG_TO_RAD;
    return center + LLVector3(cosf(a) * radius,
                              sinf(a) * radius,
                              height + bob * sinf(a * 2.7f));
}

LLVector3 LLCinematicCamera::patternHover(const LLVector3& center, F32 phase)
{
    static LLCachedControl<F32> distance(gSavedSettings, "CinematicCamHoverDistance", 2.5f);
    static LLCachedControl<F32> wander(gSavedSettings, "CinematicCamHoverWander", 1.f);
    static LLCachedControl<F32> speed(gSavedSettings, "CinematicCamHoverSpeed", 0.35f);
    static LLCachedControl<F32> height(gSavedSettings, "CinematicCamHoverHeight", 0.6f);

    // a fly: azimuth drifts on noise, elevation and range breathe on
    // decorrelated noise, plus fine jitter
    const F32 t = phase * speed;
    const F32 az = motionStartAzimuth(0.f) +
                   mMotionDir * (t * 0.9f + cc_fbm(t * 0.7f) * 3.f);
    const F32 el = cc_fbm(t * 0.55f + 31.7f) * 0.6f;
    const F32 rr = distance * (1.f + 0.25f * cc_fbm(t * 0.8f + 57.1f) * wander);
    LLVector3 p(cosf(az) * cosf(el) * rr,
                sinf(az) * cosf(el) * rr,
                height + sinf(el) * rr * 0.5f);
    // fine wing-jitter
    p += LLVector3(cc_fbm(t * 5.3f + 11.f), cc_fbm(t * 5.9f + 23.f), cc_fbm(t * 6.7f + 47.f)) * 0.06f * wander;
    return center + p;
}

LLVector3 LLCinematicCamera::patternSweep(const LLVector3& center, F32 phase)
{
    static LLCachedControl<F32> length(gSavedSettings, "CinematicCamSweepLength", 8.f);
    static LLCachedControl<F32> distance(gSavedSettings, "CinematicCamSweepDistance", 3.f);
    static LLCachedControl<F32> speed(gSavedSettings, "CinematicCamSweepSpeed", 1.f);    // m/s
    static LLCachedControl<F32> heading(gSavedSettings, "CinematicCamSweepHeading", 0.f); // deg
    static LLCachedControl<F32> height(gSavedSettings, "CinematicCamSweepHeight", 0.5f);
    static LLCachedControl<bool> pingpong(gSavedSettings, "CinematicCamSweepPingPong", true);

    const F32 h = motionStartAzimuth(0.f) + heading * DEG_TO_RAD;
    const LLVector3 dir(cosf(h), sinf(h), 0.f);         // travel direction
    const LLVector3 perp(-sinf(h), cosf(h), 0.f);       // offset from subject

    const F32 len = llmax((F32)length, 0.1f);
    F32 s = phase * llmax((F32)speed, 0.01f) / len;     // path cycles
    F32 t;
    if (pingpong)
    {
        const F32 c = cc_frac(s * 0.5f) * 2.f;          // 0..2
        t = (c < 1.f) ? c : 2.f - c;                    // triangle 0..1..0
    }
    else
    {
        t = cc_frac(s);
    }
    if (mMotionDir < 0.f)
    {
        t = 1.f - t;
    }
    return center + perp * distance + dir * ((t - 0.5f) * len) + LLVector3(0.f, 0.f, height);
}

LLVector3 LLCinematicCamera::patternCrane(const LLVector3& center, F32 phase)
{
    static LLCachedControl<F32> radius(gSavedSettings, "CinematicCamCraneRadius", 4.f);
    static LLCachedControl<F32> speed(gSavedSettings, "CinematicCamCraneSpeed", 8.f);    // deg/s
    static LLCachedControl<F32> min_h(gSavedSettings, "CinematicCamCraneMinHeight", 0.2f);
    static LLCachedControl<F32> max_h(gSavedSettings, "CinematicCamCraneMaxHeight", 4.f);
    static LLCachedControl<F32> rise_period(gSavedSettings, "CinematicCamCraneRisePeriod", 14.f);

    const F32 a = motionStartAzimuth(0.f) +
                  mMotionDir * phase * speed * DEG_TO_RAD;
    const F32 u = 0.5f + 0.5f * sinf(phase * F_TWO_PI / llmax((F32)rise_period, 1.f));
    return center + LLVector3(cosf(a) * radius, sinf(a) * radius, cc_lerp(min_h, max_h, u));
}

// ---------------------------------------------------------------------------
// film-grammar patterns
// ---------------------------------------------------------------------------
namespace
{
// avatar facing yaw (radians, region frame)
F32 cc_avatarYaw(LLVOAvatar* av)
{
    LLVector3 at = LLVector3(1.f, 0.f, 0.f) * av->getRenderRotation();
    return atan2f(at.mV[VY], at.mV[VX]);
}

// raw (linear) one-shot / ping-pong / loop progress over a duration
// end_mode: 0 = hold at end, 1 = ping-pong, 2 = loop
F32 cc_progress_raw(F32 phase, F32 duration, S32 end_mode)
{
    const F32 d = llmax(duration, 0.1f);
    switch (end_mode)
    {
        case 1: { const F32 c = cc_frac(phase / (2.f * d)) * 2.f; return (c < 1.f) ? c : 2.f - c; }
        case 2:  return cc_frac(phase / d);
        default: return llclamp(phase / d, 0.f, 1.f);
    }
}

// eased (smoothstep) variant of the above
F32 cc_progress(F32 phase, F32 duration, S32 end_mode)
{
    const F32 u = cc_progress_raw(phase, duration, end_mode);
    return u * u * (3.f - 2.f * u);     // smoothstep ease in/out
}

U64 cc_motionHash(S32 seed, S32 mode, S32 slot, U64 shot_index, U64 lane)
{
    // Addressed SplitMix64 lanes keep every choice a pure function of the
    // authored seed and shot identity. No mutable RNG state is consumed.
    U64 value = seed != 0
        ? static_cast<U64>(static_cast<U32>(seed))
        : 0x6a09e667f3bcc909ULL;
    value ^= static_cast<U64>(static_cast<U32>(mode)) *
             0x9e3779b97f4a7c15ULL;
    value ^= static_cast<U64>(static_cast<U32>(slot)) *
             0xbf58476d1ce4e5b9ULL;
    value ^= shot_index * 0x94d049bb133111ebULL;
    value ^= lane * 0xd6e8feb86659fd93ULL;
    value += 0x9e3779b97f4a7c15ULL;
    value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31);
}

F32 cc_motionUnit(S32 seed, S32 mode, S32 slot, U64 shot_index, U64 lane)
{
    const U64 hash = cc_motionHash(seed, mode, slot, shot_index, lane);
    return static_cast<F32>((hash >> 40) & 0x00ffffffULL) / 16777216.f;
}
} // anonymous namespace

void LLCinematicCamera::captureMotionStart(LLVOAvatar* av,
                                            const LLVector3& center,
                                            S32 mode, S32 switcher_slot,
                                            U64 shot_index)
{
    static LLCachedControl<S32> start_mode_setting(
        gSavedSettings, "CinematicCamMotionStartMode", 2);
    static LLCachedControl<F32> offset_deg_setting(
        gSavedSettings, "CinematicCamMotionStartOffsetDeg", 0.f);
    static LLCachedControl<S32> direction_setting(
        gSavedSettings, "CinematicCamMotionDirection", 0);
    static LLCachedControl<S32> seed_setting(
        gSavedSettings, "CinematicCamMotionSeed", 0);

    const S32 start_mode = llclamp((S32)start_mode_setting, 0, 4);
    const S32 direction = llclamp((S32)direction_setting, 0, 3);
    const S32 seed = seed_setting;
    const F32 offset_deg = std::isfinite((F32)offset_deg_setting)
        ? (F32)offset_deg_setting : 0.f;

    LLVector3 camera_delta = mTripodPos - center;
    camera_delta.mV[VZ] = 0.f;
    const bool have_camera_azimuth = camera_delta.isFinite() &&
        camera_delta.magVecSquared() > 1e-6f;
    const F32 camera_azimuth = have_camera_azimuth
        ? atan2f(camera_delta.mV[VY], camera_delta.mV[VX]) : 0.f;
    const F32 subject_azimuth = cc_avatarYaw(av);

    mMotionStartClassic = start_mode == 0;
    switch (start_mode)
    {
        case 1: // SubjectFacing
            mMotionStartAzimuth =
                subject_azimuth + offset_deg * DEG_TO_RAD;
            break;
        case 2: // CameraRelative
            if (have_camera_azimuth)
            {
                mMotionStartAzimuth = camera_azimuth;
            }
            else
            {
                // Camera exactly above/below center has no horizontal azimuth.
                // Classic is the only stable, backwards-compatible fallback.
                mMotionStartClassic = true;
                mMotionStartAzimuth = 0.f;
            }
            break;
        case 3: // Explicit region azimuth
            mMotionStartAzimuth = offset_deg * DEG_TO_RAD;
            break;
        case 4: // Random, deterministically addressed by this shot
            mMotionStartAzimuth =
                cc_motionUnit(seed, mode, switcher_slot, shot_index, 0ULL) *
                F_TWO_PI;
            break;
        case 0: // Classic/Absolute
        default:
            mMotionStartAzimuth = 0.f;
            break;
    }
    if (!std::isfinite(mMotionStartAzimuth))
    {
        mMotionStartClassic = true;
        mMotionStartAzimuth = 0.f;
    }

    switch (direction)
    {
        case 1: // CW
            mMotionDir = -1.f;
            break;
        case 2: // CCW
            mMotionDir = 1.f;
            break;
        case 3: // Random
            mMotionDir =
                (cc_motionHash(seed, mode, switcher_slot, shot_index, 1ULL) &
                 1ULL) != 0ULL ? 1.f : -1.f;
            break;
        case 0: // Auto
        default:
        {
            // Classic+Auto is the legacy + direction exactly. Otherwise head
            // toward the pre-shot view by its shortest arc. CameraRelative is
            // already there, so continue away from the subject's frontal axis.
            mMotionDir = 1.f;
            if (!mMotionStartClassic && have_camera_azimuth)
            {
                const F32 toward_camera =
                    llsimple_angle(camera_azimuth - mMotionStartAzimuth);
                if (fabsf(toward_camera) > 1e-4f)
                {
                    mMotionDir = toward_camera < 0.f ? -1.f : 1.f;
                }
                else
                {
                    const F32 facing_side = llsimple_angle(
                        mMotionStartAzimuth - subject_azimuth);
                    if (fabsf(facing_side) > 1e-4f)
                    {
                        mMotionDir = facing_side < 0.f ? -1.f : 1.f;
                    }
                }
            }
            break;
        }
    }
    mMotionStartCaptured = true;
}

bool LLCinematicCamera::captureCurrentSwitcherView(
    S32 subject, F32& yaw_offset_deg, F32& pitch_deg, F32& distance_m,
    F32& height_m, F32& fov_deg) const
{
    LLVOAvatar* av =
        subject == ALDirectorSwitcher::SUBJECT_DEFAULT
            ? resolveTarget() : resolveMarkedTarget(subject);
    if (!av)
    {
        av = resolveDefaultTarget();
    }
    LLViewerCamera* cam = LLViewerCamera::getInstance();
    if (!av || av->isDead() || !cam)
    {
        return false;
    }

    const LLVector3 render_pos = cam->getOrigin();
    LLVector3 at = cam->getAtAxis();
    const F32 render_fov = cam->getView();
    const F32 subject_scale = av->getUniformScale();
    if (!render_pos.isFinite() || !at.isFinite() ||
        !std::isfinite(render_fov) || !std::isfinite(subject_scale) ||
        subject_scale < 0.01f || at.magVecSquared() < 1e-8f)
    {
        return false;
    }
    at.normVec();

    // Static-shot geometry is authored before the existing clone-scale pass.
    // Invert that pass so a capture of a scaled subject reapplies only once.
    const LLVector3 subject_base = cc_subjectBase(av);
    const LLVector3 logical_pos =
        subject_base + (render_pos - subject_base) * (1.f / subject_scale);
    static LLCachedControl<F32> frame_up(
        gSavedSettings, "CinematicCamFrameOffsetUp", 0.f);
    const LLVector3 rig_base =
        subject_base + LLVector3(0.f, 0.f, (F32)frame_up);
    const LLVector3 horizontal(
        logical_pos.mV[VX] - rig_base.mV[VX],
        logical_pos.mV[VY] - rig_base.mV[VY], 0.f);
    const F32 horizontal_distance = horizontal.magVec();

    // Pitch is the camera's elevation around its focus point. The render
    // camera's at-axis points the opposite way (camera -> focus).
    const F32 at_horizontal =
        sqrtf(at.mV[VX] * at.mV[VX] + at.mV[VY] * at.mV[VY]);
    const F32 pitch = llclamp(
        -atan2f(at.mV[VZ], at_horizontal),
        -80.f * DEG_TO_RAD, 80.f * DEG_TO_RAD);
    const F32 cos_pitch = llmax(cosf(pitch), 0.173648f);
    const F32 distance = horizontal_distance / cos_pitch;

    F32 yaw = horizontal_distance > 0.001f
        ? atan2f(horizontal.mV[VY], horizontal.mV[VX])
        : atan2f(-at.mV[VY], -at.mV[VX]);
    yaw = (yaw - cc_avatarYaw(av)) * RAD_TO_DEG;
    while (yaw > 180.f) yaw -= 360.f;
    while (yaw < -180.f) yaw += 360.f;

    yaw_offset_deg = llclamp(yaw, -180.f, 180.f);
    pitch_deg = pitch * RAD_TO_DEG;
    distance_m = llclamp(distance, 0.3f, 64.f);
    height_m = llclamp(
        logical_pos.mV[VZ] - rig_base.mV[VZ] -
            sinf(pitch) * distance_m,
        -10.f, 20.f);
    fov_deg = llclamp(render_fov * RAD_TO_DEG, 5.f, 175.f);
    return true;
}

// Vertigo shot: camera travels between two distances along a bearing fixed to
// the subject's facing while the FOV compensates so the SUBJECT keeps constant
// angular size -- the background stretches or compresses around them.
LLVector3 LLCinematicCamera::patternDollyZoom(LLVOAvatar* av, const LLVector3& focus,
                                              F32 phase, F32& fov_mul)
{
    static LLCachedControl<F32> d_start(gSavedSettings, "CinematicCamVertigoStartDist", 2.f);
    static LLCachedControl<F32> d_end(gSavedSettings, "CinematicCamVertigoEndDist", 7.f);
    static LLCachedControl<F32> duration(gSavedSettings, "CinematicCamVertigoDuration", 8.f);
    static LLCachedControl<F32> heading(gSavedSettings, "CinematicCamVertigoHeading", 0.f);  // deg from facing
    static LLCachedControl<F32> height(gSavedSettings, "CinematicCamVertigoHeight", 0.f);    // rel focus
    static LLCachedControl<S32> end_mode(gSavedSettings, "CinematicCamVertigoEndMode", 1);   // ping-pong

    const F32 u  = cc_progress(phase, duration, end_mode);
    const F32 d0 = llmax((F32)d_start, 0.3f);
    const F32 d  = llmax(cc_lerp(d0, llmax((F32)d_end, 0.3f), u), 0.3f);

    // keep the subject's angular size constant: tan(fov/2) * d == const
    LLViewerCamera* cam = LLViewerCamera::getInstance();
    const F32 half0 = 0.5f * cam->getDefaultFOV();
    const F32 half  = atanf(tanf(half0) * d0 / d);
    fov_mul = llclamp(half / llmax(half0, 0.001f), 0.05f, 4.f);

    const F32 yaw = cc_avatarYaw(av) + heading * DEG_TO_RAD;
    return focus + LLVector3(cosf(yaw) * d, sinf(yaw) * d, (F32)height);
}

// slow creep from a wide start to a close-up on the face; holds at the end
LLVector3 LLCinematicCamera::patternPushIn(LLVOAvatar* av, const LLVector3& focus, F32 phase)
{
    static LLCachedControl<F32> d_start(gSavedSettings, "CinematicCamPushStartDist", 4.f);
    static LLCachedControl<F32> d_end(gSavedSettings, "CinematicCamPushEndDist", 0.8f);
    static LLCachedControl<F32> duration(gSavedSettings, "CinematicCamPushDuration", 12.f);
    static LLCachedControl<F32> heading(gSavedSettings, "CinematicCamPushHeading", 0.f);   // deg from facing
    static LLCachedControl<F32> height(gSavedSettings, "CinematicCamPushHeight", 0.f);     // rel focus (eye level)
    static LLCachedControl<S32> end_mode(gSavedSettings, "CinematicCamPushEndMode", 0);    // hold

    const F32 u = cc_progress(phase, duration, end_mode);
    const F32 d = llmax(cc_lerp(llmax((F32)d_start, 0.3f), llmax((F32)d_end, 0.3f), u), 0.3f);
    const F32 yaw = cc_avatarYaw(av) + heading * DEG_TO_RAD;
    return focus + LLVector3(cosf(yaw) * d, sinf(yaw) * d, (F32)height);
}

// low-angle hero shot: camera near the ground in front of the subject looking
// up, drifting on a slow arc with a gentle breathing push
LLVector3 LLCinematicCamera::patternLowHero(LLVOAvatar* av, const LLVector3& center, F32 phase)
{
    static LLCachedControl<F32> distance(gSavedSettings, "CinematicCamHeroDistance", 2.2f);
    static LLCachedControl<F32> height(gSavedSettings, "CinematicCamHeroHeight", 0.35f);   // above feet
    static LLCachedControl<F32> arc(gSavedSettings, "CinematicCamHeroArc", 30.f);          // deg total drift
    static LLCachedControl<F32> period(gSavedSettings, "CinematicCamHeroPeriod", 14.f);
    static LLCachedControl<F32> heading(gSavedSettings, "CinematicCamHeroHeading", 0.f);   // deg from facing

    const F32 w = F_TWO_PI / llmax((F32)period, 1.f);
    const F32 yaw = motionStartAzimuth(cc_avatarYaw(av)) +
                    ((F32)heading + mMotionDir * 0.5f * (F32)arc *
                     sinf(phase * w)) * DEG_TO_RAD;
    const F32 d = llmax((F32)distance, 0.3f) * (1.f - 0.12f * sinf(phase * w * 0.5f));
    return center + LLVector3(cosf(yaw) * d, sinf(yaw) * d, (F32)height);
}

// God's-eye: straight down on the subject, rising (or descending) between two
// heights, with an optional slow spin carried out as camera roll
LLVector3 LLCinematicCamera::patternOverhead(const LLVector3& center, F32 phase, F32& roll_out)
{
    static LLCachedControl<F32> h_start(gSavedSettings, "CinematicCamOverheadStart", 4.f);
    static LLCachedControl<F32> h_end(gSavedSettings, "CinematicCamOverheadEnd", 14.f);
    static LLCachedControl<F32> duration(gSavedSettings, "CinematicCamOverheadDuration", 16.f);
    static LLCachedControl<F32> spin(gSavedSettings, "CinematicCamOverheadSpin", 4.f);     // deg/s
    static LLCachedControl<S32> end_mode(gSavedSettings, "CinematicCamOverheadEndMode", 0); // hold

    const F32 u = cc_progress(phase, duration, end_mode);
    roll_out = mMotionDir * spin * phase * DEG_TO_RAD;
    // tiny lateral epsilon keeps the straight-down look-at well-defined
    const F32 a = motionStartAzimuth(0.f);
    return center + LLVector3(
        cosf(a) * 0.02f, sinf(a) * 0.02f,
        llmax(cc_lerp((F32)h_start, (F32)h_end, u), 0.5f));
}

// snap zoom with a slight overshoot from a tripod position captured at
// activation; toggle the mode off/on to retrigger the hit
LLVector3 LLCinematicCamera::patternCrashZoom(F32 phase, F32& fov_mul)
{
    static LLCachedControl<F32> zoom(gSavedSettings, "CinematicCamCrashZoom", 0.35f);      // end fov mul
    static LLCachedControl<F32> duration(gSavedSettings, "CinematicCamCrashDuration", 0.4f);

    const F32 u = llclamp(phase / llmax((F32)duration, 0.05f), 0.f, 1.f);
    // sharp attack with a ~8% overshoot that settles
    const F32 e = 1.f - powf(1.f - u, 3.f);
    const F32 over = 1.f + 0.08f * sinf(llmin(u * 2.f, 1.f) * F_PI) * (1.f - u);
    fov_mul = llclamp(cc_lerp(1.f, (F32)zoom, e) * (u < 1.f ? over : 1.f), 0.05f, 4.f);
    return mTripodPos;
}

// Kubrick creep: position frozen, FOV drifts imperceptibly over a long time
LLVector3 LLCinematicCamera::patternSlowZoom(F32 phase, F32& fov_mul)
{
    static LLCachedControl<F32> zoom(gSavedSettings, "CinematicCamSlowZoomTarget", 0.55f); // <1 in, >1 out
    static LLCachedControl<F32> duration(gSavedSettings, "CinematicCamSlowZoomDuration", 45.f);

    const F32 u = cc_progress(phase, duration, 0);
    fov_mul = llclamp(cc_lerp(1.f, (F32)zoom, u), 0.05f, 4.f);
    return mTripodPos;
}

// violent sub-second arc around the subject; reads as a whip with motion blur
LLVector3 LLCinematicCamera::patternWhipArc(LLVOAvatar* av, const LLVector3& center, F32 phase)
{
    static LLCachedControl<F32> from_deg(gSavedSettings, "CinematicCamWhipFrom", -60.f);
    static LLCachedControl<F32> to_deg(gSavedSettings, "CinematicCamWhipTo", 60.f);
    static LLCachedControl<F32> duration(gSavedSettings, "CinematicCamWhipDuration", 0.45f);
    static LLCachedControl<F32> distance(gSavedSettings, "CinematicCamWhipDistance", 3.f);
    static LLCachedControl<F32> height(gSavedSettings, "CinematicCamWhipHeight", 1.2f);

    F32 u = llclamp(phase / llmax((F32)duration, 0.05f), 0.f, 1.f);
    u = u * u * (3.f - 2.f * u); u = u * u * (3.f - 2.f * u);   // double smoothstep: hard whip
    if (mMotionDir < 0.f)
    {
        u = 1.f - u;
    }
    const F32 yaw = motionStartAzimuth(cc_avatarYaw(av)) +
                    cc_lerp((F32)from_deg, (F32)to_deg, u) *
                    DEG_TO_RAD;
    return center + LLVector3(cosf(yaw) * distance, sinf(yaw) * distance, (F32)height);
}

// the universal oner building block: one eased partial orbit, then hold
LLVector3 LLCinematicCamera::patternArc(LLVOAvatar* av, const LLVector3& center, F32 phase)
{
    static LLCachedControl<F32> from_deg(gSavedSettings, "CinematicCamArcFrom", -40.f);
    static LLCachedControl<F32> to_deg(gSavedSettings, "CinematicCamArcTo", 40.f);
    static LLCachedControl<F32> duration(gSavedSettings, "CinematicCamArcDuration", 9.f);
    static LLCachedControl<F32> distance(gSavedSettings, "CinematicCamArcDistance", 2.6f);
    static LLCachedControl<F32> height(gSavedSettings, "CinematicCamArcHeight", 1.3f);
    static LLCachedControl<S32> end_mode(gSavedSettings, "CinematicCamArcEndMode", 0);     // hold

    F32 u = cc_progress(phase, duration, end_mode);
    if (mMotionDir < 0.f)
    {
        u = 1.f - u;
    }
    const F32 yaw = motionStartAzimuth(cc_avatarYaw(av)) +
                    cc_lerp((F32)from_deg, (F32)to_deg, u) *
                    DEG_TO_RAD;
    return center + LLVector3(cosf(yaw) * distance, sinf(yaw) * distance, (F32)height);
}

// epic arrival: starts low behind the subject, rises over their shoulder while
// the framing lifts from the subject to the horizon ahead of them
LLVector3 LLCinematicCamera::patternReveal(LLVOAvatar* av, const LLVector3& center,
                                           F32 phase, LLVector3& focus_io)
{
    static LLCachedControl<F32> behind(gSavedSettings, "CinematicCamRevealBehind", 1.4f);
    static LLCachedControl<F32> low_h(gSavedSettings, "CinematicCamRevealLowHeight", 0.4f);
    static LLCachedControl<F32> high_h(gSavedSettings, "CinematicCamRevealHighHeight", 2.1f);
    static LLCachedControl<F32> ahead(gSavedSettings, "CinematicCamRevealAhead", 14.f);
    static LLCachedControl<F32> duration(gSavedSettings, "CinematicCamRevealDuration", 10.f);

    const F32 u = cc_progress(phase, duration, 0);
    const F32 yaw = cc_avatarYaw(av);
    const LLVector3 fwd(cosf(yaw), sinf(yaw), 0.f);

    const LLVector3 subject = focus_io;     // head focus from the caller
    focus_io = subject * (1.f - u) + (subject + fwd * (F32)ahead + LLVector3(0.f, 0.f, 1.f)) * u;
    return center - fwd * (F32)behind + LLVector3(0.f, 0.f, cc_lerp((F32)low_h, (F32)high_h, u));
}

// closing shot: eased retreat and rise, leaving the subject in the frame
LLVector3 LLCinematicCamera::patternPullBack(LLVOAvatar* av, const LLVector3& focus, F32 phase)
{
    static LLCachedControl<F32> d_start(gSavedSettings, "CinematicCamPullStartDist", 1.2f);
    static LLCachedControl<F32> d_end(gSavedSettings, "CinematicCamPullEndDist", 10.f);
    static LLCachedControl<F32> h_end(gSavedSettings, "CinematicCamPullEndHeight", 2.5f);
    static LLCachedControl<F32> duration(gSavedSettings, "CinematicCamPullDuration", 14.f);
    static LLCachedControl<F32> heading(gSavedSettings, "CinematicCamPullHeading", 0.f);

    const F32 u = cc_progress(phase, duration, 0);
    const F32 yaw = cc_avatarYaw(av) + heading * DEG_TO_RAD;
    const F32 d = cc_lerp(llmax((F32)d_start, 0.3f), llmax((F32)d_end, 0.3f), u);
    return focus + LLVector3(cosf(yaw) * d, sinf(yaw) * d, (F32)h_end * u);
}

// dialogue master: perpendicular to the line between me and the selected
// avatar, framing the midpoint; degrades to a profile shot of the subject
LLVector3 LLCinematicCamera::patternTwoShot(LLVOAvatar* target, LLVector3& focus_io)
{
    static LLCachedControl<S32> side(gSavedSettings, "CinematicCamTwoShotSide", 1);
    static LLCachedControl<F32> pad(gSavedSettings, "CinematicCamTwoShotPad", 1.3f);       // dist = sep * pad
    static LLCachedControl<F32> min_d(gSavedSettings, "CinematicCamTwoShotMinDist", 2.5f);
    static LLCachedControl<F32> height(gSavedSettings, "CinematicCamTwoShotHeight", 1.4f);

    // A switcher slot's secondary mark supplies the second body. Outside the
    // switcher (or when that mark is unavailable), legacy Subject B supplies
    // it; otherwise this falls back to the stock self+target pairing.
    LLVOAvatar* second = resolveSecondaryTarget();
    LLVOAvatar* self = second ? second
                              : (isAgentAvatarValid() ? (LLVOAvatar*)gAgentAvatarp : target);
    const F32 s = ((S32)side != 0) ? 1.f : -1.f;

    LLVector3 a = self->getPositionAgent();
    LLVector3 b = (target && target != self) ? target->getPositionAgent()
                                             : a + LLVector3(cosf(cc_avatarYaw(self)), sinf(cc_avatarYaw(self)), 0.f) * 2.f;
    LLVector3 line = b - a; line.mV[VZ] = 0.f;
    const F32 sep = llmax(line.normVec(), 0.5f);
    const LLVector3 perp(-line.mV[VY] * s, line.mV[VX] * s, 0.f);

    const LLVector3 mid = (a + b) * 0.5f;
    const F32 self_scale = self->getUniformScale();
    const F32 target_scale = target ? target->getUniformScale() : 1.f;
    const F32 framing_scale = llmax(self_scale, target_scale);
    if (self_scale == 1.f && target_scale == 1.f)
    {
        focus_io = mid + LLVector3(0.f, 0.f, (F32)height);
    }
    else
    {
        // Midpoint the two bodies' individually-scaled head-height proxies.
        focus_io = mid + LLVector3(0.f, 0.f,
                                  (F32)height * 0.5f * (self_scale + target_scale));
    }
    return focus_io + perp * llmax(sep * (F32)pad, (F32)min_d * framing_scale);
}

// walk-and-talk: camera ahead of the subject looking back, backpedaling as
// they advance (the smoothing constant supplies the steadicam lag)
LLVector3 LLCinematicCamera::patternLeadFollow(LLVOAvatar* av, const LLVector3& focus, F32 phase)
{
    static LLCachedControl<F32> distance(gSavedSettings, "CinematicCamLeadDistance", 2.2f);
    static LLCachedControl<F32> height(gSavedSettings, "CinematicCamLeadHeight", 0.f);      // rel focus
    static LLCachedControl<F32> sway(gSavedSettings, "CinematicCamLeadSway", 0.15f);

    const F32 yaw = cc_avatarYaw(av);
    LLVector3 fwd(cosf(yaw), sinf(yaw), 0.f);
    LLVector3 left(-fwd.mV[VY], fwd.mV[VX], 0.f);
    return focus + fwd * llmax((F32)distance, 0.5f)
                 + left * (cc_fbm(phase * 0.25f) * (F32)sway)
                 + LLVector3(0.f, 0.f, (F32)height);
}

// Leone standoff: locked micro-frame on the face through a narrow lens, with
// a barely-there float so it breathes
LLVector3 LLCinematicCamera::patternECU(LLVOAvatar* av, const LLVector3& focus,
                                        F32 phase, F32& fov_mul)
{
    static LLCachedControl<F32> distance(gSavedSettings, "CinematicCamECUDistance", 0.5f);
    static LLCachedControl<F32> zoom(gSavedSettings, "CinematicCamECUZoom", 0.55f);
    static LLCachedControl<F32> drift(gSavedSettings, "CinematicCamECUDrift", 0.006f);

    fov_mul = llclamp((F32)zoom, 0.05f, 1.5f);
    const F32 yaw = cc_avatarYaw(av);
    LLVector3 p = focus + LLVector3(cosf(yaw), sinf(yaw), 0.f) * llmax((F32)distance, 0.25f);
    p += LLVector3(cc_fbm(phase * 0.31f), cc_fbm(phase * 0.27f + 13.f), cc_fbm(phase * 0.23f + 29.f)) * (F32)drift;
    return p;
}

// surveillance: far off through a long lens; the compression plus a slow
// drift reads as "someone is watching"
LLVector3 LLCinematicCamera::patternLongLens(LLVOAvatar* av, const LLVector3& focus,
                                             F32 phase, F32& fov_mul)
{
    static LLCachedControl<F32> distance(gSavedSettings, "CinematicCamLongDistance", 15.f);
    static LLCachedControl<F32> zoom(gSavedSettings, "CinematicCamLongZoom", 0.22f);
    static LLCachedControl<F32> heading(gSavedSettings, "CinematicCamLongHeading", 35.f);  // deg from facing
    static LLCachedControl<F32> height(gSavedSettings, "CinematicCamLongHeight", 0.4f);
    static LLCachedControl<F32> drift(gSavedSettings, "CinematicCamLongDrift", 0.05f);

    fov_mul = llclamp((F32)zoom, 0.05f, 1.f);
    const F32 yaw = cc_avatarYaw(av) + heading * DEG_TO_RAD;
    LLVector3 p = focus + LLVector3(cosf(yaw), sinf(yaw), 0.f) * llmax((F32)distance, 3.f)
                        + LLVector3(0.f, 0.f, (F32)height);
    p += LLVector3(cc_fbm(phase * 0.11f), cc_fbm(phase * 0.13f + 7.f), cc_fbm(phase * 0.09f + 17.f)) * (F32)drift;
    return p;
}

// oner flourish: the orbit tightens and rises as it turns, ending close
LLVector3 LLCinematicCamera::patternSpiral(const LLVector3& center, F32 phase)
{
    static LLCachedControl<F32> r_start(gSavedSettings, "CinematicCamSpiralStartRadius", 6.f);
    static LLCachedControl<F32> r_end(gSavedSettings, "CinematicCamSpiralEndRadius", 1.4f);
    static LLCachedControl<F32> h_start(gSavedSettings, "CinematicCamSpiralStartHeight", 0.3f);
    static LLCachedControl<F32> h_end(gSavedSettings, "CinematicCamSpiralEndHeight", 2.f);
    static LLCachedControl<F32> speed(gSavedSettings, "CinematicCamSpiralSpeed", 40.f);    // deg/s
    static LLCachedControl<F32> duration(gSavedSettings, "CinematicCamSpiralDuration", 12.f);

    const F32 u = cc_progress(phase, duration, 0);
    const F32 a = motionStartAzimuth(0.f) +
                  mMotionDir * phase * speed * DEG_TO_RAD;
    const F32 r = cc_lerp((F32)r_start, llmax((F32)r_end, 0.3f), u);
    return center + LLVector3(cosf(a) * r, sinf(a) * r, cc_lerp((F32)h_start, (F32)h_end, u));
}

// character introduction: boots-to-face vertical rise at a fixed frontal
// distance, the gaze staying level with whatever the frame is passing
LLVector3 LLCinematicCamera::patternPedestal(LLVOAvatar* av, const LLVector3& center,
                                             F32 phase, LLVector3& focus_io)
{
    static LLCachedControl<F32> distance(gSavedSettings, "CinematicCamPedestalDistance", 1.8f);
    static LLCachedControl<F32> h_start(gSavedSettings, "CinematicCamPedestalStart", 0.2f);
    static LLCachedControl<F32> h_end(gSavedSettings, "CinematicCamPedestalEnd", 1.75f);
    static LLCachedControl<F32> duration(gSavedSettings, "CinematicCamPedestalDuration", 8.f);
    static LLCachedControl<F32> heading(gSavedSettings, "CinematicCamPedestalHeading", 0.f);

    const F32 u = cc_progress(phase, duration, 0);
    const F32 h = cc_lerp((F32)h_start, (F32)h_end, u);
    const F32 yaw = cc_avatarYaw(av) + heading * DEG_TO_RAD;
    focus_io = center + LLVector3(0.f, 0.f, h);
    return center + LLVector3(cosf(yaw) * llmax((F32)distance, 0.4f), sinf(yaw) * llmax((F32)distance, 0.4f), h);
}

// over-the-shoulder: anchored behind MY shoulder, framing the resolved target
// (the selected avatar; falls back to what I'm facing when nothing is selected)
LLVector3 LLCinematicCamera::patternOTS(LLVOAvatar* target, LLVector3& focus_io)
{
    static LLCachedControl<S32> side(gSavedSettings, "CinematicCamOTSSide", 1);        // 1=right, 0=left
    static LLCachedControl<F32> back(gSavedSettings, "CinematicCamOTSBack", 0.45f);
    static LLCachedControl<F32> out(gSavedSettings, "CinematicCamOTSOut", 0.22f);
    static LLCachedControl<F32> up(gSavedSettings, "CinematicCamOTSUp", 0.12f);

    // A switcher slot's secondary mark supplies the shoulder. Outside the
    // switcher (or when that mark is unavailable), legacy Subject B anchors
    // the shot (B looking at A); otherwise use stock behind-my-shoulder framing.
    LLVOAvatar* second = resolveSecondaryTarget();
    LLVOAvatar* self = second ? second
                              : (isAgentAvatarValid() ? (LLVOAvatar*)gAgentAvatarp : target);

    const char* joint_name = ((S32)side != 0) ? "mShoulderRight" : "mShoulderLeft";
    LLVector3 shoulder = self->getPositionAgent() + LLVector3(0.f, 0.f, 1.4f);
    if (LLJoint* j = self->getJoint(joint_name))
    {
        shoulder = j->getWorldPosition();
    }
    shoulder = cc_scaleAboutSubjectBase(self, shoulder);

    const F32 yaw = cc_avatarYaw(self);
    const LLVector3 fwd(cosf(yaw), sinf(yaw), 0.f);
    const LLVector3 right(sinf(yaw), -cosf(yaw), 0.f);
    const F32 s = ((S32)side != 0) ? 1.f : -1.f;

    // frame the conversation partner's head; with no distinct target, frame
    // the space I'm facing so the shot still composes
    if (target && target != self)
    {
        if (LLJoint* th = target->getJoint("mHead"))
        {
            focus_io = cc_scaleAboutSubjectBase(target, th->getWorldPosition());
        }
        else
        {
            focus_io = cc_scaleAboutSubjectBase(
                target, target->getPositionAgent() + LLVector3(0.f, 0.f, 1.5f));
        }
    }
    else
    {
        focus_io = shoulder + fwd * 3.f * self->getUniformScale();
    }

    const F32 scale = self->getUniformScale();
    return shoulder - fwd * (F32)back * scale + right * s * (F32)out * scale
                    + LLVector3(0.f, 0.f, (F32)up * scale);
}

// ---------------------------------------------------------------------------
// acrobatic / dance / closeup / impact patterns
// ---------------------------------------------------------------------------

// held frontal frame with the camera rolling: continuous spin (RollSpeed) or an
// eased oscillation (RollAmplitude / RollPeriod). Reads as a dance flourish.
LLVector3 LLCinematicCamera::patternBarrelRoll(LLVOAvatar* av, const LLVector3& center,
                                               F32 phase, F32& roll_out)
{
    static LLCachedControl<F32> distance(gSavedSettings, "CinematicCamBarrelDistance", 3.f);
    static LLCachedControl<F32> height(gSavedSettings, "CinematicCamBarrelHeight", 1.f);
    static LLCachedControl<F32> roll_speed(gSavedSettings, "CinematicCamBarrelRollSpeed", 45.f);   // deg/s
    static LLCachedControl<F32> roll_amp(gSavedSettings, "CinematicCamBarrelRollAmplitude", 30.f); // deg
    static LLCachedControl<F32> roll_period(gSavedSettings, "CinematicCamBarrelRollPeriod", 4.f);  // s
    static LLCachedControl<bool> oscillate(gSavedSettings, "CinematicCamBarrelOscillate", false);

    if (oscillate)
    {
        const F32 w = F_TWO_PI / llmax((F32)roll_period, 0.1f);
        roll_out = mMotionDir * (F32)roll_amp *
                   sinf(phase * w) * DEG_TO_RAD;
    }
    else
    {
        roll_out = mMotionDir * (F32)roll_speed * phase * DEG_TO_RAD;
    }

    const F32 yaw = motionStartAzimuth(cc_avatarYaw(av));
    return center + LLVector3(cosf(yaw) * llmax((F32)distance, 0.3f),
                             sinf(yaw) * llmax((F32)distance, 0.3f), (F32)height);
}

// aggressive spiral: orbit whose radius and height lerp start->end across Turns
// revolutions in Duration, rolling RollPerTurn per revolution. Loop/ping-pong.
LLVector3 LLCinematicCamera::patternCorkscrew(const LLVector3& center, F32 phase, F32& roll_out)
{
    static LLCachedControl<F32> r_start(gSavedSettings, "CinematicCamCorkStartRadius", 5.f);
    static LLCachedControl<F32> r_end(gSavedSettings, "CinematicCamCorkEndRadius", 1.5f);
    static LLCachedControl<F32> h_start(gSavedSettings, "CinematicCamCorkStartHeight", 0.3f);
    static LLCachedControl<F32> h_end(gSavedSettings, "CinematicCamCorkEndHeight", 2.5f);
    static LLCachedControl<F32> turns(gSavedSettings, "CinematicCamCorkTurns", 2.f);
    static LLCachedControl<F32> duration(gSavedSettings, "CinematicCamCorkDuration", 8.f);
    static LLCachedControl<F32> roll_per_turn(gSavedSettings, "CinematicCamCorkRollPerTurn", 90.f); // deg
    static LLCachedControl<S32> end_mode(gSavedSettings, "CinematicCamCorkEndMode", 2);             // loop

    const F32 u = cc_progress(phase, duration, end_mode);
    const F32 revs = (F32)turns * u;                    // revolutions completed
    const F32 a = motionStartAzimuth(0.f) +
                  mMotionDir * revs * F_TWO_PI;
    const F32 r = cc_lerp((F32)r_start, llmax((F32)r_end, 0.3f), u);
    roll_out = mMotionDir * revs * (F32)roll_per_turn * DEG_TO_RAD;
    return center + LLVector3(cosf(a) * r, sinf(a) * r, cc_lerp((F32)h_start, (F32)h_end, u));
}

// eases at the extremes, fastest through center: a horizontal swing of half-
// width SwingAngle about a facing-relative heading, always aimed at the subject
LLVector3 LLCinematicCamera::patternPendulum(LLVOAvatar* av, const LLVector3& center, F32 phase)
{
    static LLCachedControl<F32> radius(gSavedSettings, "CinematicCamPendulumRadius", 3.5f);
    static LLCachedControl<F32> height(gSavedSettings, "CinematicCamPendulumHeight", 1.2f);
    static LLCachedControl<F32> swing(gSavedSettings, "CinematicCamPendulumSwing", 45.f);   // deg half-width
    static LLCachedControl<F32> period(gSavedSettings, "CinematicCamPendulumPeriod", 6.f);  // s
    static LLCachedControl<F32> heading(gSavedSettings, "CinematicCamPendulumHeading", 0.f);// deg, arc facing

    const F32 w = F_TWO_PI / llmax((F32)period, 0.5f);
    const F32 ang = mMotionDir * (F32)swing *
                    sinf(phase * w);                    // eased at the extremes
    const F32 yaw = motionStartAzimuth(cc_avatarYaw(av)) +
                    ((F32)heading + ang) * DEG_TO_RAD;
    return center + LLVector3(cosf(yaw) * llmax((F32)radius, 0.3f),
                             sinf(yaw) * llmax((F32)radius, 0.3f), (F32)height);
}

// dolly-zoom-while-circling: a steady orbit while the FOV warps between two
// multipliers, so the perspective breathes as the camera comes around
LLVector3 LLCinematicCamera::patternContraOrbit(const LLVector3& center, F32 phase, F32& fov_mul)
{
    static LLCachedControl<F32> radius(gSavedSettings, "CinematicCamContraRadius", 4.f);
    static LLCachedControl<F32> height(gSavedSettings, "CinematicCamContraHeight", 1.f);
    static LLCachedControl<F32> speed(gSavedSettings, "CinematicCamContraSpeed", 25.f);     // deg/s
    static LLCachedControl<F32> fov_start(gSavedSettings, "CinematicCamContraFovStart", 0.6f);
    static LLCachedControl<F32> fov_end(gSavedSettings, "CinematicCamContraFovEnd", 1.5f);
    static LLCachedControl<F32> warp_period(gSavedSettings, "CinematicCamContraWarpPeriod", 8.f); // s

    const F32 a = motionStartAzimuth(0.f) +
                  mMotionDir * phase * speed * DEG_TO_RAD;
    const F32 w = F_TWO_PI / llmax((F32)warp_period, 0.5f);
    const F32 t = 0.5f + 0.5f * sinf(phase * w);
    fov_mul = llclamp(cc_lerp((F32)fov_start, (F32)fov_end, t), 0.05f, 4.f);
    return center + LLVector3(cosf(a) * radius, sinf(a) * radius, (F32)height);
}

// wide-lens impact: a constant wide FOV while the camera pushes from far to
// near at the face and recoils over Duration (eased). One-shot / ping-pong.
LLVector3 LLCinematicCamera::patternFisheyeLunge(LLVOAvatar* av, const LLVector3& focus,
                                                 F32 phase, F32& fov_mul)
{
    static LLCachedControl<F32> near_d(gSavedSettings, "CinematicCamFisheyeNear", 0.6f);
    static LLCachedControl<F32> far_d(gSavedSettings, "CinematicCamFisheyeFar", 4.f);
    static LLCachedControl<F32> wide_fov(gSavedSettings, "CinematicCamFisheyeFov", 1.7f);  // >1 = wide
    static LLCachedControl<F32> duration(gSavedSettings, "CinematicCamFisheyeDuration", 2.5f);
    static LLCachedControl<F32> height(gSavedSettings, "CinematicCamFisheyeHeight", 0.f);  // rel focus
    static LLCachedControl<S32> end_mode(gSavedSettings, "CinematicCamFisheyeEndMode", 1); // ping-pong

    fov_mul = llclamp((F32)wide_fov, 0.05f, 4.f);
    const F32 u = cc_progress(phase, duration, end_mode);
    const F32 lunge = sinf(F_PI * u);                   // 0 -> 1 -> 0 (near at the middle)
    const F32 d = llmax(cc_lerp(llmax((F32)far_d, 0.3f), llmax((F32)near_d, 0.2f), lunge), 0.2f);
    const F32 yaw = cc_avatarYaw(av);
    return focus + LLVector3(cosf(yaw) * d, sinf(yaw) * d, (F32)height);
}

// ground-level lateral track (ping-pong) at a very low height, aimed up at the
// subject so the low angle reads
LLVector3 LLCinematicCamera::patternFloorSkimmer(LLVOAvatar* av, const LLVector3& center, F32 phase)
{
    static LLCachedControl<F32> height(gSavedSettings, "CinematicCamSkimmerHeight", 0.25f);
    static LLCachedControl<F32> distance(gSavedSettings, "CinematicCamSkimmerDistance", 3.f);
    static LLCachedControl<F32> length(gSavedSettings, "CinematicCamSkimmerLength", 6.f);
    static LLCachedControl<F32> speed(gSavedSettings, "CinematicCamSkimmerSpeed", 1.5f);    // m/s
    static LLCachedControl<F32> heading(gSavedSettings, "CinematicCamSkimmerHeading", 0.f); // deg

    const F32 h = motionStartAzimuth(0.f) + heading * DEG_TO_RAD;
    const LLVector3 dir(cosf(h), sinf(h), 0.f);
    const LLVector3 perp(-sinf(h), cosf(h), 0.f);

    const F32 len = llmax((F32)length, 0.1f);
    const F32 s = phase * llmax((F32)speed, 0.01f) / len;
    const F32 c = cc_frac(s * 0.5f) * 2.f;              // 0..2
    F32 t = (c < 1.f) ? c : 2.f - c;                    // triangle 0..1..0 (ping-pong)
    if (mMotionDir < 0.f)
    {
        t = 1.f - t;
    }
    return center + perp * llmax((F32)distance, 0.3f) + dir * ((t - 0.5f) * len)
                  + LLVector3(0.f, 0.f, (F32)height);
}

// one-shot rocket launch: from near the floor, accelerating (ease-in) straight
// up to EndHeight; the look-at tilts down to keep the subject as it passes
LLVector3 LLCinematicCamera::patternBoostRise(LLVOAvatar* av, const LLVector3& center, F32 phase)
{
    static LLCachedControl<F32> h_start(gSavedSettings, "CinematicCamBoostStartHeight", 0.2f);
    static LLCachedControl<F32> h_end(gSavedSettings, "CinematicCamBoostEndHeight", 8.f);
    static LLCachedControl<F32> distance(gSavedSettings, "CinematicCamBoostDistance", 3.f);
    static LLCachedControl<F32> duration(gSavedSettings, "CinematicCamBoostDuration", 4.f);
    static LLCachedControl<F32> heading(gSavedSettings, "CinematicCamBoostHeading", 0.f);
    static LLCachedControl<S32> end_mode(gSavedSettings, "CinematicCamBoostEndMode", 0);    // hold

    const F32 p = cc_progress_raw(phase, duration, end_mode);
    const F32 e = p * p;                                // accelerate (ease-in)
    const F32 h = cc_lerp((F32)h_start, (F32)h_end, e);
    const F32 yaw = cc_avatarYaw(av) + heading * DEG_TO_RAD;
    return center + LLVector3(cosf(yaw) * llmax((F32)distance, 0.3f),
                             sinf(yaw) * llmax((F32)distance, 0.3f), h);
}

// jib over the top: an elliptical vertical arc that lifts from one side, up
// over the apex above the subject, and down the far side (eased)
LLVector3 LLCinematicCamera::patternBoomOver(LLVOAvatar* av, const LLVector3& center, F32 phase)
{
    static LLCachedControl<F32> radius(gSavedSettings, "CinematicCamBoomRadius", 4.f);      // horizontal
    static LLCachedControl<F32> apex(gSavedSettings, "CinematicCamBoomApex", 5.f);          // peak height
    static LLCachedControl<F32> span(gSavedSettings, "CinematicCamBoomSpan", 180.f);        // deg total
    static LLCachedControl<F32> duration(gSavedSettings, "CinematicCamBoomDuration", 6.f);
    static LLCachedControl<F32> axis(gSavedSettings, "CinematicCamBoomAxis", 0.f);          // deg heading
    static LLCachedControl<S32> end_mode(gSavedSettings, "CinematicCamBoomEndMode", 0);     // hold

    const F32 u = cc_progress(phase, duration, end_mode);
    const F32 half = 0.5f * (F32)span * DEG_TO_RAD;
    const F32 phi = mMotionDir * cc_lerp(-half, half, u); // reverse side/travel together
    const F32 ax = motionStartAzimuth(0.f) + axis * DEG_TO_RAD;
    const LLVector3 dir(cosf(ax), sinf(ax), 0.f);
    return center + dir * ((F32)radius * sinf(phi)) + LLVector3(0.f, 0.f, (F32)apex * cosf(phi));
}

// Busby Berkeley: locked directly overhead looking straight down, the frame
// spinning about the vertical (carried as camera roll). Offset 0 = pure top-down.
LLVector3 LLCinematicCamera::patternTopSpin(const LLVector3& center, F32 phase, F32& roll_out)
{
    static LLCachedControl<F32> height(gSavedSettings, "CinematicCamTopSpinHeight", 5.f);
    static LLCachedControl<F32> speed(gSavedSettings, "CinematicCamTopSpinSpeed", 30.f);    // deg/s
    static LLCachedControl<F32> offset(gSavedSettings, "CinematicCamTopSpinOffset", 0.f);   // small radius

    const F32 spin = mMotionDir * phase * speed * DEG_TO_RAD;
    const F32 a = motionStartAzimuth(0.f) + spin;
    roll_out = spin;                                    // spin the straight-down view
    // a tiny lateral epsilon keeps the look-straight-down orientation well
    // defined even with Offset 0
    const F32 r = (F32)offset;
    return center + LLVector3(cosf(a) * r + 0.02f, sinf(a) * r, llmax((F32)height, 0.5f));
}

// showcase crane: a subject-centered orbit while the height eases between two
// levels for a full-body reveal
LLVector3 LLCinematicCamera::patternTurntable(const LLVector3& center, F32 phase)
{
    static LLCachedControl<F32> radius(gSavedSettings, "CinematicCamTurntableRadius", 4.f);
    static LLCachedControl<F32> speed(gSavedSettings, "CinematicCamTurntableSpeed", 15.f);  // deg/s
    static LLCachedControl<F32> min_h(gSavedSettings, "CinematicCamTurntableMinHeight", 0.5f);
    static LLCachedControl<F32> max_h(gSavedSettings, "CinematicCamTurntableMaxHeight", 3.f);
    static LLCachedControl<F32> period(gSavedSettings, "CinematicCamTurntablePeriod", 20.f);// s

    const F32 a = motionStartAzimuth(0.f) +
                  mMotionDir * phase * speed * DEG_TO_RAD;
    const F32 u = 0.5f + 0.5f * sinf(phase * F_TWO_PI / llmax((F32)period, 1.f));
    return center + LLVector3(cosf(a) * radius, sinf(a) * radius, cc_lerp((F32)min_h, (F32)max_h, u));
}

// intimate face close-up: a narrow lens, drifting on low-amplitude noise with a
// slight distance breathing so it never locks off
LLVector3 LLCinematicCamera::patternFloatingECU(LLVOAvatar* av, const LLVector3& focus,
                                                F32 phase, F32& fov_mul)
{
    static LLCachedControl<F32> distance(gSavedSettings, "CinematicCamFloatDistance", 0.8f);
    static LLCachedControl<F32> drift(gSavedSettings, "CinematicCamFloatDrift", 0.04f);
    static LLCachedControl<F32> speed(gSavedSettings, "CinematicCamFloatSpeed", 0.5f);
    static LLCachedControl<F32> zoom(gSavedSettings, "CinematicCamFloatFov", 0.6f);         // <1 = narrow

    fov_mul = llclamp((F32)zoom, 0.05f, 1.5f);
    const F32 t = phase * llmax((F32)speed, 0.01f);
    const F32 d = llmax((F32)distance, 0.25f) * (1.f + 0.08f * sinf(t * 0.7f));   // breathing
    const F32 yaw = cc_avatarYaw(av);
    LLVector3 p = focus + LLVector3(cosf(yaw), sinf(yaw), 0.f) * d;
    p += LLVector3(cc_fbm(t + 3.1f), cc_fbm(t + 13.7f), cc_fbm(t + 29.3f)) * (F32)drift;
    return p;
}

// percussive accent: held frontal frame while the aim whips vertically -- a
// sharp attack each Period settling on an exponential decay, alternating up/down
LLVector3 LLCinematicCamera::patternTiltWhip(LLVOAvatar* av, const LLVector3& center,
                                             F32 phase, LLVector3& focus_io)
{
    static LLCachedControl<F32> distance(gSavedSettings, "CinematicCamTiltWhipDistance", 3.f);
    static LLCachedControl<F32> height(gSavedSettings, "CinematicCamTiltWhipHeight", 1.2f);
    static LLCachedControl<F32> amplitude(gSavedSettings, "CinematicCamTiltWhipAmplitude", 25.f); // deg
    static LLCachedControl<F32> period(gSavedSettings, "CinematicCamTiltWhipPeriod", 2.f);        // s
    static LLCachedControl<F32> snap(gSavedSettings, "CinematicCamTiltWhipSnap", 6.f);            // decay

    const F32 per = llmax((F32)period, 0.1f);
    const F32 idx = floorf(phase / per);
    const F32 ph = cc_frac(phase / per);                // 0..1 within a whip
    const F32 sign = (cc_frac(idx * 0.5f) < 0.25f) ? 1.f : -1.f;   // alternate up/down
    const F32 env = expf(-llmax((F32)snap, 0.f) * ph);  // sharp attack, eased settle
    const F32 pitch = llclamp((F32)amplitude * DEG_TO_RAD * env * sign, -1.4f, 1.4f);

    const F32 yaw = cc_avatarYaw(av);
    const F32 d = llmax((F32)distance, 0.3f);
    // whip the aim by raising/lowering the framing point: tan(pitch) * distance
    focus_io.mV[VZ] += d * tanf(pitch);
    return center + LLVector3(cosf(yaw) * d, sinf(yaw) * d, (F32)height);
}

// slow body-scale introduction: orbit at a constant radius while both camera
// and gaze travel from the feet to the actual head joint, holding on the face
LLVector3 LLCinematicCamera::patternBodyHelix(LLVOAvatar* av, const LLVector3& center,
                                              F32 phase, LLVector3& focus_io)
{
    static LLCachedControl<F32> radius(gSavedSettings, "CinematicCamBodyHelixRadius", 2.4f);
    static LLCachedControl<F32> revolutions(gSavedSettings, "CinematicCamBodyHelixRevolutions", 1.25f);
    static LLCachedControl<F32> duration(gSavedSettings, "CinematicCamBodyHelixDuration", 14.f);
    static LLCachedControl<F32> start_offset(gSavedSettings, "CinematicCamBodyHelixStartOffset", 0.08f);
    static LLCachedControl<F32> end_offset(gSavedSettings, "CinematicCamBodyHelixEndOffset", 0.f);

    const LLVector3 frame_off = center - av->getPositionAgent();
    const LLVector3 feet = cc_subjectBase(av) + frame_off;
    LLVector3 head = feet + LLVector3(0.f, 0.f, 1.75f);
    if (LLJoint* joint = av->getJoint("mHead"))
    {
        head = joint->getWorldPosition() + frame_off;
    }
    const F32 u = cc_progress(phase, duration, 0);
    const F32 a = motionStartAzimuth(cc_avatarYaw(av)) +
                  mMotionDir * (F32)revolutions * F_TWO_PI * u;
    const F32 z = cc_lerp(feet.mV[VZ] + (F32)start_offset,
                          head.mV[VZ] + (F32)end_offset, u);
    focus_io = LLVector3(cc_lerp(feet.mV[VX], head.mV[VX], u),
                         cc_lerp(feet.mV[VY], head.mV[VY], u), z);
    return focus_io + LLVector3(cosf(a) * llmax((F32)radius, 0.3f),
                                sinf(a) * llmax((F32)radius, 0.3f), 0.f);
}

// menace reveal: a fixed-heading reverse pedestal from above the actual head
// down to the feet, with camera and gaze remaining level throughout
LLVector3 LLCinematicCamera::patternDescent(LLVOAvatar* av, const LLVector3& center,
                                            F32 phase, LLVector3& focus_io)
{
    static LLCachedControl<F32> distance(gSavedSettings, "CinematicCamDescentDistance", 2.2f);
    static LLCachedControl<F32> above_head(gSavedSettings, "CinematicCamDescentAboveHead", 0.45f);
    static LLCachedControl<F32> foot_offset(gSavedSettings, "CinematicCamDescentFootOffset", 0.12f);
    static LLCachedControl<F32> duration(gSavedSettings, "CinematicCamDescentDuration", 9.f);
    static LLCachedControl<F32> heading(gSavedSettings, "CinematicCamDescentHeading", 0.f);

    const LLVector3 frame_off = center - av->getPositionAgent();
    const LLVector3 feet = cc_subjectBase(av) + frame_off;
    LLVector3 head = feet + LLVector3(0.f, 0.f, 1.75f);
    if (LLJoint* joint = av->getJoint("mHead"))
    {
        head = joint->getWorldPosition() + frame_off;
    }
    const F32 u = cc_progress(phase, duration, 0);
    const F32 z = cc_lerp(head.mV[VZ] + (F32)above_head,
                          feet.mV[VZ] + (F32)foot_offset, u);
    focus_io = LLVector3(cc_lerp(head.mV[VX], feet.mV[VX], u),
                         cc_lerp(head.mV[VY], feet.mV[VY], u), z);
    const F32 yaw = motionStartAzimuth(cc_avatarYaw(av)) +
                    mMotionDir * (F32)heading * DEG_TO_RAD;
    return focus_io + LLVector3(cosf(yaw) * llmax((F32)distance, 0.3f),
                                sinf(yaw) * llmax((F32)distance, 0.3f), 0.f);
}

// one-shot lateral truck: a straight rail perpendicular to the chosen heading
LLVector3 LLCinematicCamera::patternParallaxSlide(LLVOAvatar* av, const LLVector3& center, F32 phase)
{
    static LLCachedControl<F32> length(gSavedSettings, "CinematicCamParallaxLength", 8.f);
    static LLCachedControl<F32> distance(gSavedSettings, "CinematicCamParallaxDistance", 3.f);
    static LLCachedControl<F32> height(gSavedSettings, "CinematicCamParallaxHeight", 1.2f);
    static LLCachedControl<F32> duration(gSavedSettings, "CinematicCamParallaxDuration", 10.f);
    static LLCachedControl<F32> heading(gSavedSettings, "CinematicCamParallaxHeading", 0.f);

    F32 u = cc_progress(phase, duration, 0);
    if (mMotionDir < 0.f)
    {
        u = 1.f - u;
    }
    const F32 yaw = motionStartAzimuth(cc_avatarYaw(av)) +
                    (F32)heading * DEG_TO_RAD;
    const LLVector3 away(cosf(yaw), sinf(yaw), 0.f);
    const LLVector3 rail(-sinf(yaw), cosf(yaw), 0.f);
    return center + away * llmax((F32)distance, 0.3f)
                  + rail * ((u - 0.5f) * (F32)length)
                  + LLVector3(0.f, 0.f, (F32)height);
}

// dance loop: horizontal lemniscate with opposite parallax on each lobe
LLVector3 LLCinematicCamera::patternFigureEight(LLVOAvatar* av, const LLVector3& center, F32 phase)
{
    static LLCachedControl<F32> radius(gSavedSettings, "CinematicCamFigureEightRadius", 3.f);
    static LLCachedControl<F32> height(gSavedSettings, "CinematicCamFigureEightHeight", 1.2f);
    static LLCachedControl<F32> period(gSavedSettings, "CinematicCamFigureEightPeriod", 10.f);
    static LLCachedControl<F32> heading(gSavedSettings, "CinematicCamFigureEightHeading", 0.f);

    const F32 t = mMotionDir * phase * F_TWO_PI /
                  llmax((F32)period, 0.5f);
    const F32 yaw = motionStartAzimuth(cc_avatarYaw(av)) +
                    (F32)heading * DEG_TO_RAD;
    const LLVector3 fwd(cosf(yaw), sinf(yaw), 0.f);
    const LLVector3 side(-sinf(yaw), cosf(yaw), 0.f);
    // Offset the lemniscate's crossover in front of the subject so the
    // mathematically central crossing never drives the camera through them.
    return center + fwd * ((F32)radius * (1.f + 0.5f * sinf(2.f * t)))
                  + side * ((F32)radius * sinf(t))
                  + LLVector3(0.f, 0.f, (F32)height);
}

// costume insert: narrow close framing drifting laterally across a selectable
// body-height band derived from the feet-to-head span
LLVector3 LLCinematicCamera::patternDetailSweep(LLVOAvatar* av, const LLVector3& center,
                                                F32 phase, LLVector3& focus_io, F32& fov_mul)
{
    static LLCachedControl<F32> distance(gSavedSettings, "CinematicCamDetailDistance", 0.9f);
    static LLCachedControl<F32> length(gSavedSettings, "CinematicCamDetailLength", 1.2f);
    static LLCachedControl<F32> band(gSavedSettings, "CinematicCamDetailBand", 0.68f);
    static LLCachedControl<F32> duration(gSavedSettings, "CinematicCamDetailDuration", 12.f);
    static LLCachedControl<F32> fov(gSavedSettings, "CinematicCamDetailFov", 0.65f);

    const LLVector3 frame_off = center - av->getPositionAgent();
    const LLVector3 feet = cc_subjectBase(av) + frame_off;
    LLVector3 head = feet + LLVector3(0.f, 0.f, 1.75f);
    if (LLJoint* joint = av->getJoint("mHead"))
    {
        head = joint->getWorldPosition() + frame_off;
    }
    focus_io = feet + (head - feet) * llclamp((F32)band, 0.f, 1.f);
    const F32 yaw = motionStartAzimuth(cc_avatarYaw(av));
    const LLVector3 away(cosf(yaw), sinf(yaw), 0.f);
    const LLVector3 side(-sinf(yaw), cosf(yaw), 0.f);
    F32 u = cc_progress(phase, duration, 0);
    if (mMotionDir < 0.f)
    {
        u = 1.f - u;
    }
    fov_mul = llclamp((F32)fov, 0.1f, 1.5f);
    return focus_io + away * llmax((F32)distance, 0.25f)
                    + side * ((u - 0.5f) * (F32)length);
}

// drone/sports pass: a fast one-shot straight chord with look-at supplying
// the continuous yaw needed to keep the subject framed
LLVector3 LLCinematicCamera::patternCableCam(LLVOAvatar* av, const LLVector3& center, F32 phase)
{
    static LLCachedControl<F32> length(gSavedSettings, "CinematicCamCableLength", 14.f);
    static LLCachedControl<F32> miss(gSavedSettings, "CinematicCamCableMissDistance", 2.f);
    static LLCachedControl<F32> height(gSavedSettings, "CinematicCamCableHeight", 1.6f);
    static LLCachedControl<F32> duration(gSavedSettings, "CinematicCamCableDuration", 5.f);
    static LLCachedControl<F32> heading(gSavedSettings, "CinematicCamCableHeading", 0.f);

    F32 u = cc_progress(phase, duration, 0);
    if (mMotionDir < 0.f)
    {
        u = 1.f - u;
    }
    const F32 yaw = motionStartAzimuth(cc_avatarYaw(av)) +
                    (F32)heading * DEG_TO_RAD;
    const LLVector3 path(cosf(yaw), sinf(yaw), 0.f);
    const LLVector3 side(-sinf(yaw), cosf(yaw), 0.f);
    return center + path * ((u - 0.5f) * (F32)length)
                  + side * (F32)miss + LLVector3(0.f, 0.f, (F32)height);
}

// dialogue hold: deterministic millimetric breathing, no wandering noise
LLVector3 LLCinematicCamera::patternBreathingHold(LLVOAvatar* av, const LLVector3& center, F32 phase)
{
    static LLCachedControl<F32> distance(gSavedSettings, "CinematicCamBreathingDistance", 2.2f);
    static LLCachedControl<F32> height(gSavedSettings, "CinematicCamBreathingHeight", 1.4f);
    static LLCachedControl<F32> amplitude(gSavedSettings, "CinematicCamBreathingAmplitude", 0.025f);
    static LLCachedControl<F32> period(gSavedSettings, "CinematicCamBreathingPeriod", 6.f);
    static LLCachedControl<F32> heading(gSavedSettings, "CinematicCamBreathingHeading", 0.f);

    const F32 t = phase * F_TWO_PI / llmax((F32)period, 1.f);
    const F32 yaw = cc_avatarYaw(av) + (F32)heading * DEG_TO_RAD;
    const LLVector3 away(cosf(yaw), sinf(yaw), 0.f);
    const LLVector3 side(-sinf(yaw), cosf(yaw), 0.f);
    return center + away * (llmax((F32)distance, 0.3f) + (F32)amplitude * sinf(t))
                  + side * ((F32)amplitude * 0.45f * sinf(t * 0.5f))
                  + LLVector3(0.f, 0.f, (F32)height + (F32)amplitude * 0.35f * cosf(t));
}

// Fixed, subject-relative switcher coverage. These authored framings have no
// hidden clock; camera life remains available as the existing Breathing Hold
// bank assignment. Tight/OTS/Two-Shot deliberately reuse modes 19/10/17.
LLVector3 LLCinematicCamera::patternStaticShot(
    LLVOAvatar* av, const LLVector3& center, S32 mode,
    LLVector3& focus_io, F32& fov_mul)
{
    // Preserve the global frame offset already folded into center.
    const LLVector3 base =
        cc_subjectBase(av) + (center - av->getPositionAgent());
    F32 yaw = cc_avatarYaw(av);
    F32 distance = 2.5f;
    F32 camera_z = 1.45f;
    F32 aim_z = 1.35f;

    ALDirectorSwitcher::Slot custom;
    if (ALDirectorSwitcher::instance().activeCustomAngle(custom))
    {
        const F32 custom_yaw =
            yaw + custom.mCustomYawOffsetDeg * DEG_TO_RAD;
        const F32 custom_pitch = custom.mCustomPitchDeg * DEG_TO_RAD;
        const F32 cos_pitch = cosf(custom_pitch);
        const F32 distance =
            llclamp(custom.mCustomDistanceM, 0.3f, 64.f);
        focus_io = base + LLVector3(
            0.f, 0.f, llclamp(custom.mCustomHeightM, -10.f, 20.f));
        fov_mul = llclamp(
            custom.mCustomFovDeg * DEG_TO_RAD /
                llmax(LLViewerCamera::getInstance()->getDefaultFOV(),
                      1.f * DEG_TO_RAD),
            0.05f, 4.f);
        return focus_io + LLVector3(
            cosf(custom_yaw) * cos_pitch * distance,
            sinf(custom_yaw) * cos_pitch * distance,
            sinf(custom_pitch) * distance);
    }

    switch (mode)
    {
        case MODE_STATIC_WIDE:
        {
            static LLCachedControl<F32> heading(
                gSavedSettings, "CinematicCamStaticWideHeading", 15.f);
            static LLCachedControl<F32> shot_distance(
                gSavedSettings, "CinematicCamStaticWideDistance", 5.5f);
            static LLCachedControl<F32> camera_up(
                gSavedSettings, "CinematicCamStaticWideCameraUp", 1.30f);
            static LLCachedControl<F32> aim_up(
                gSavedSettings, "CinematicCamStaticWideAimUp", 1.05f);
            static LLCachedControl<F32> shot_fov(
                gSavedSettings, "CinematicCamStaticWideFov", 1.10f);
            yaw += (F32)heading * DEG_TO_RAD;
            distance = (F32)shot_distance;
            camera_z = (F32)camera_up;
            aim_z = (F32)aim_up;
            fov_mul = llclamp((F32)shot_fov, 0.05f, 4.f);
            break;
        }
        case MODE_STATIC_MEDIUM:
        {
            static LLCachedControl<F32> heading(
                gSavedSettings, "CinematicCamStaticMediumHeading", 15.f);
            static LLCachedControl<F32> shot_distance(
                gSavedSettings, "CinematicCamStaticMediumDistance", 2.2f);
            static LLCachedControl<F32> camera_up(
                gSavedSettings, "CinematicCamStaticMediumCameraUp", 1.45f);
            static LLCachedControl<F32> aim_up(
                gSavedSettings, "CinematicCamStaticMediumAimUp", 1.35f);
            static LLCachedControl<F32> shot_fov(
                gSavedSettings, "CinematicCamStaticMediumFov", 0.82f);
            yaw += (F32)heading * DEG_TO_RAD;
            distance = (F32)shot_distance;
            camera_z = (F32)camera_up;
            aim_z = (F32)aim_up;
            fov_mul = llclamp((F32)shot_fov, 0.05f, 4.f);
            break;
        }
        case MODE_STATIC_CLOSE:
        {
            static LLCachedControl<F32> heading(
                gSavedSettings, "CinematicCamStaticCloseHeading", -10.f);
            static LLCachedControl<F32> shot_distance(
                gSavedSettings, "CinematicCamStaticCloseDistance", 1.25f);
            static LLCachedControl<F32> camera_up(
                gSavedSettings, "CinematicCamStaticCloseCameraUp", 0.f);
            static LLCachedControl<F32> aim_up(
                gSavedSettings, "CinematicCamStaticCloseAimUp", 0.f);
            static LLCachedControl<F32> shot_fov(
                gSavedSettings, "CinematicCamStaticCloseFov", 0.70f);
            yaw += (F32)heading * DEG_TO_RAD;
            distance = llmax((F32)shot_distance, 0.3f);
            fov_mul = llclamp((F32)shot_fov, 0.05f, 4.f);
            // A close-up is head-authored regardless of the global chest/head
            // preference. Camera/Aim Up remain tunable offsets from that live
            // joint; zero preserves the authored framing. Fall back to a
            // stable anatomical height when the joint is unavailable.
            LLVector3 head_anchor = base + LLVector3(0.f, 0.f, 1.55f);
            if (LLJoint* head = av->getJoint("mHead"))
            {
                head_anchor = head->getWorldPosition() +
                    (center - av->getPositionAgent());
            }
            focus_io = head_anchor +
                LLVector3(0.f, 0.f, (F32)aim_up);
            return head_anchor +
                LLVector3(cosf(yaw) * distance,
                          sinf(yaw) * distance, (F32)camera_up);
        }
        case MODE_STATIC_PROFILE_L:
        case MODE_STATIC_PROFILE_R:
        {
            static LLCachedControl<F32> heading_l(
                gSavedSettings, "CinematicCamStaticProfileLHeading", 90.f);
            static LLCachedControl<F32> distance_l(
                gSavedSettings, "CinematicCamStaticProfileLDistance", 2.f);
            static LLCachedControl<F32> camera_up_l(
                gSavedSettings, "CinematicCamStaticProfileLCameraUp", 0.f);
            static LLCachedControl<F32> aim_up_l(
                gSavedSettings, "CinematicCamStaticProfileLAimUp", 0.f);
            static LLCachedControl<F32> fov_l(
                gSavedSettings, "CinematicCamStaticProfileLFov", 0.75f);
            static LLCachedControl<F32> heading_r(
                gSavedSettings, "CinematicCamStaticProfileRHeading", -90.f);
            static LLCachedControl<F32> distance_r(
                gSavedSettings, "CinematicCamStaticProfileRDistance", 2.f);
            static LLCachedControl<F32> camera_up_r(
                gSavedSettings, "CinematicCamStaticProfileRCameraUp", 0.f);
            static LLCachedControl<F32> aim_up_r(
                gSavedSettings, "CinematicCamStaticProfileRAimUp", 0.f);
            static LLCachedControl<F32> fov_r(
                gSavedSettings, "CinematicCamStaticProfileRFov", 0.75f);
            const bool left_profile = mode == MODE_STATIC_PROFILE_L;
            yaw += (left_profile ? (F32)heading_l : (F32)heading_r) *
                   DEG_TO_RAD;
            distance = llmax(
                left_profile ? (F32)distance_l : (F32)distance_r, 0.3f);
            camera_z =
                left_profile ? (F32)camera_up_l : (F32)camera_up_r;
            aim_z = left_profile ? (F32)aim_up_l : (F32)aim_up_r;
            fov_mul = llclamp(
                left_profile ? (F32)fov_l : (F32)fov_r, 0.05f, 4.f);
            LLVector3 head_anchor = base + LLVector3(0.f, 0.f, 1.55f);
            if (LLJoint* head = av->getJoint("mHead"))
            {
                head_anchor = head->getWorldPosition() +
                    (center - av->getPositionAgent());
            }
            focus_io = head_anchor + LLVector3(0.f, 0.f, aim_z);
            return head_anchor +
                LLVector3(cosf(yaw) * distance,
                          sinf(yaw) * distance, camera_z);
        }
        case MODE_STATIC_LOW:
        {
            static LLCachedControl<F32> heading(
                gSavedSettings, "CinematicCamStaticLowHeading", 10.f);
            static LLCachedControl<F32> shot_distance(
                gSavedSettings, "CinematicCamStaticLowDistance", 2.3f);
            static LLCachedControl<F32> camera_up(
                gSavedSettings, "CinematicCamStaticLowCameraUp", 0.28f);
            static LLCachedControl<F32> aim_up(
                gSavedSettings, "CinematicCamStaticLowAimUp", 1.25f);
            static LLCachedControl<F32> shot_fov(
                gSavedSettings, "CinematicCamStaticLowFov", 0.90f);
            yaw += (F32)heading * DEG_TO_RAD;
            distance = (F32)shot_distance;
            camera_z = (F32)camera_up;
            aim_z = (F32)aim_up;
            fov_mul = llclamp((F32)shot_fov, 0.05f, 4.f);
            break;
        }
        case MODE_STATIC_HIGH:
        {
            static LLCachedControl<F32> heading(
                gSavedSettings, "CinematicCamStaticHighHeading", 10.f);
            static LLCachedControl<F32> shot_distance(
                gSavedSettings, "CinematicCamStaticHighDistance", 2.4f);
            static LLCachedControl<F32> camera_up(
                gSavedSettings, "CinematicCamStaticHighCameraUp", 3.0f);
            static LLCachedControl<F32> aim_up(
                gSavedSettings, "CinematicCamStaticHighAimUp", 1.35f);
            static LLCachedControl<F32> shot_fov(
                gSavedSettings, "CinematicCamStaticHighFov", 0.85f);
            yaw += (F32)heading * DEG_TO_RAD;
            distance = (F32)shot_distance;
            camera_z = (F32)camera_up;
            aim_z = (F32)aim_up;
            fov_mul = llclamp((F32)shot_fov, 0.05f, 4.f);
            break;
        }
        case MODE_STATIC_FULL:
        {
            static LLCachedControl<F32> heading(
                gSavedSettings, "CinematicCamStaticFullHeading", 0.f);
            static LLCachedControl<F32> shot_distance(
                gSavedSettings, "CinematicCamStaticFullDistance", 4.0f);
            static LLCachedControl<F32> camera_up(
                gSavedSettings, "CinematicCamStaticFullCameraUp", 1.05f);
            static LLCachedControl<F32> aim_up(
                gSavedSettings, "CinematicCamStaticFullAimUp", 0.95f);
            static LLCachedControl<F32> shot_fov(
                gSavedSettings, "CinematicCamStaticFullFov", 0.95f);
            yaw += (F32)heading * DEG_TO_RAD;
            distance = (F32)shot_distance;
            camera_z = (F32)camera_up;
            aim_z = (F32)aim_up;
            fov_mul = llclamp((F32)shot_fov, 0.05f, 4.f);
            break;
        }
        default:
            break;
    }

    focus_io = base + LLVector3(0.f, 0.f, aim_z);
    distance = llmax(distance, 0.3f);
    return base + LLVector3(cosf(yaw) * distance,
                            sinf(yaw) * distance, camera_z);
}

// Solve only at authored boundaries, then replace the generated rig's radius
// and vertical composition while retaining its current orbit direction. This
// is deliberately downstream of every pattern and clone-scale transform, and
// upstream of smoothing/operator/shake.
void LLCinematicCamera::applyAutoReframe(
    LLVOAvatar* av, S32 mode, F32 plain_fov, bool force_solve,
    bool allow_glide,
    LLVector3& pos, LLVector3& focus)
{
    static LLCachedControl<bool> enabled(
        gSavedSettings, "CinematicAutoFrameEnabled", false);
    static LLCachedControl<F32> fill_setting(
        gSavedSettings, "CinematicAutoFrameFill", 0.85f);
    static LLCachedControl<F32> compose_setting(
        gSavedSettings, "CinematicAutoFrameComposeLine", 0.33f);
    static LLCachedControl<F32> distance_trim(
        gSavedSettings, "CinematicAutoFrameDistanceTrim", 0.f);
    static LLCachedControl<F32> frame_up(
        gSavedSettings, "CinematicCamFrameOffsetUp", 0.f);
    static LLCachedControl<bool> lens_enabled(
        gSavedSettings, "CinematicFrameLensEnabled", false);
    static LLCachedControl<F32> aspect(
        gSavedSettings, "CinematicFrameAspectRatio", 0.f);
    static LLCachedControl<F32> custom_aspect(
        gSavedSettings, "CinematicFrameCustomRatio", 2.35f);
    static LLCachedControl<F32> focal_mm(
        gSavedSettings, "CinematicFrameFocalLengthMM", 50.f);

    if (!enabled || mode == MODE_BONE_LOCK)
    {
        mAutoFrameHaveSolve = false;
        mAutoFrameSettleActive = false;
        mAutoFrameHaveApplied = false;
        mAutoFrameLastEnabled = enabled;
        return; // strict disabled path: do not read or write rig geometry
    }

    static LLCachedControl<S32> settle_mode_setting(
        gSavedSettings, "CinematicAutoFrameSettleMode", 2);
    static LLCachedControl<F32> settle_seconds_setting(
        gSavedSettings, "CinematicAutoFrameSettleSeconds", 0.70f);
    static LLCachedControl<F32> settle_max_ratio_setting(
        gSavedSettings, "CinematicAutoFrameSettleMaxRatio", 0.f);
    static LLCachedControl<S32> settle_curve_setting(
        gSavedSettings, "CinematicAutoFrameSettleCurve", 0);
    static LLCachedControl<F32> settle_feather_setting(
        gSavedSettings, "CinematicAutoFrameSettleFeather", 0.f);
    static LLCachedControl<F32> settle_bezier_x1_setting(
        gSavedSettings, "CinematicAutoFrameSettleBezierX1", 0.42f);
    static LLCachedControl<F32> settle_bezier_y1_setting(
        gSavedSettings, "CinematicAutoFrameSettleBezierY1", 0.f);
    static LLCachedControl<F32> settle_bezier_x2_setting(
        gSavedSettings, "CinematicAutoFrameSettleBezierX2", 0.58f);
    static LLCachedControl<F32> settle_bezier_y2_setting(
        gSavedSettings, "CinematicAutoFrameSettleBezierY2", 1.f);
    const S32 settle_mode = llclamp((S32)settle_mode_setting, 0, 2);
    const F32 settle_seconds = (F32)settle_seconds_setting;
    const F32 settle_max_ratio = (F32)settle_max_ratio_setting;

    const F32 legacy_frame_up = std::isfinite((F32)frame_up)
        ? (F32)frame_up : 0.f;
    const auto apply_legacy_frame_up = [&]()
    {
        focus.mV[VZ] += legacy_frame_up;
        if (mode != MODE_CRASH_ZOOM && mode != MODE_SLOW_ZOOM)
        {
            pos.mV[VZ] += legacy_frame_up;
        }
    };

    LLViewerCamera* cam = LLViewerCamera::getInstance();
    const F32 window_aspect = cam->getAspect();
    const F32 base_fov = cam->getDefaultFOV();
    const F32 signature_fill = std::isfinite((F32)fill_setting)
        ? (F32)fill_setting : 0.f;
    const F32 signature_compose = std::isfinite((F32)compose_setting)
        ? (F32)compose_setting : 0.f;
    const F32 signature_aspect = std::isfinite((F32)aspect)
        ? (F32)aspect : 0.f;
    const F32 signature_custom_aspect = std::isfinite((F32)custom_aspect)
        ? (F32)custom_aspect : 0.f;
    const F32 signature_focal = std::isfinite((F32)focal_mm)
        ? (F32)focal_mm : 0.f;
    const F32 signature_window_aspect = std::isfinite(window_aspect)
        ? window_aspect : 0.f;
    const F32 signature_base_fov = std::isfinite(base_fov)
        ? ll_round(base_fov, 0.0001f) : 0.f;
    const bool settings_changed =
        !mAutoFrameLastEnabled ||
        mAutoFrameLastLensEnabled != (bool)lens_enabled ||
        mAutoFrameLastFill != signature_fill ||
        mAutoFrameLastCompose != signature_compose ||
        mAutoFrameLastAspect != signature_aspect ||
        mAutoFrameLastCustomAspect != signature_custom_aspect ||
        mAutoFrameLastFocalMM != signature_focal ||
        mAutoFrameLastWindowAspect != signature_window_aspect ||
        mAutoFrameLastBaseFOV != signature_base_fov ||
        mAutoFrameSolvedRequestSerial != mAutoFrameRequestSerial;

    if (force_solve || settings_changed)
    {
        // Capture this before acknowledging the request serial below.
        const bool manual_reframe =
            mAutoFrameSolvedRequestSerial != mAutoFrameRequestSerial;
        mAutoFrameLastEnabled = true;
        mAutoFrameLastLensEnabled = lens_enabled;
        mAutoFrameLastFill = signature_fill;
        mAutoFrameLastCompose = signature_compose;
        mAutoFrameLastAspect = signature_aspect;
        mAutoFrameLastCustomAspect = signature_custom_aspect;
        mAutoFrameLastFocalMM = signature_focal;
        mAutoFrameLastWindowAspect = signature_window_aspect;
        mAutoFrameLastBaseFOV = signature_base_fov;
        mAutoFrameSolvedRequestSerial = mAutoFrameRequestSerial;
        mAutoFrameHaveSolve = false;

        LLVector3 top;
        LLVector3 bottom;
        bool eye_level = false;
        const F32 framed_fov = cc_retainedFrameFov(plain_fov, cam);
        const F32 tan_half_fov = tanf(framed_fov * 0.5f);
        if (!std::isfinite((F32)fill_setting) ||
            !std::isfinite((F32)compose_setting))
        {
            apply_legacy_frame_up();
            return;
        }
        const F32 compose = llclamp((F32)compose_setting, 0.02f, 0.90f);
        // A top anchor at compose leaves only (1-compose) of the frame below
        // it. Reserve two percent at the bottom so Full/Wide never crop feet.
        const F32 available_fill = llmax(0.08f, 0.98f - compose);
        const F32 fill = llmin(
            llclamp((F32)fill_setting, 0.10f, 0.98f), available_fill);
        LLVector3 rig_direction = pos - focus;
        const F32 old_distance = rig_direction.normVec();

        if (!cc_autoFrameAnchors(av, mode, top, bottom, eye_level) ||
            !std::isfinite(framed_fov) || framed_fov <= 0.f ||
            !std::isfinite(tan_half_fov) || tan_half_fov <= 0.f ||
            !rig_direction.isFinite() || old_distance <= 0.001f ||
            fabsf(rig_direction.mV[VZ]) > 0.95f)
        {
            apply_legacy_frame_up();
            return; // fail soft: leave this frame's existing authored rig
        }
        const F32 height = top.mV[VZ] - bottom.mV[VZ];
        if (!std::isfinite(height) || height <= 0.01f)
        {
            apply_legacy_frame_up();
            return;
        }

        const F32 solved_distance = llclamp(
            (height / (2.f * fill)) / tan_half_fov,
            AUTO_FRAME_MIN_DISTANCE, AUTO_FRAME_MAX_DISTANCE);
        if (!std::isfinite(solved_distance))
        {
            apply_legacy_frame_up();
            return;
        }

        const LLVector3 base = cc_subjectBase(av);
        if (!base.isFinite())
        {
            apply_legacy_frame_up();
            return;
        }
        mAutoFrameDistance = solved_distance;
        mAutoFrameEyeLevel = eye_level;
        if (eye_level)
        {
            LLVector3 horizontal(
                rig_direction.mV[VX], rig_direction.mV[VY], 0.f);
            if (!horizontal.isFinite() || horizontal.normVec() <= 0.001f)
            {
                apply_legacy_frame_up();
                return;
            }
            const F32 projected_top = 1.f - 2.f * compose;
            mAutoFrameEyeAimSlope = projected_top * tan_half_fov;
            mAutoFrameFocusXOffset = top.mV[VX] - base.mV[VX];
            mAutoFrameFocusYOffset = top.mV[VY] - base.mV[VY];
            mAutoFrameEyeZOffset = top.mV[VZ] - base.mV[VZ];
            mAutoFrameFocusZOffset =
                mAutoFrameEyeZOffset - solved_distance * mAutoFrameEyeAimSlope;
        }
        else
        {
            LLVector3 solved_focus = focus;
            LLVector3 solved_pos = focus + rig_direction * solved_distance;
            const LLQuaternion solved_rot = cc_lookAt(solved_pos, solved_focus);
            const LLMatrix3 axes(solved_rot);
            const LLVector3 at(axes.mMatrix[0]);
            const LLVector3 up(axes.mMatrix[2]);
            const LLVector3 to_top = top - solved_pos;
            const F32 depth = to_top * at;
            const F32 image_up = to_top * up;
            const F32 projected_top = 1.f - 2.f * compose;
            const F32 denominator =
                up.mV[VZ] - projected_top * tan_half_fov * at.mV[VZ];
            if (!std::isfinite(depth) || depth <= 0.01f ||
                !std::isfinite(image_up) || !std::isfinite(denominator) ||
                fabsf(denominator) <= 0.001f)
            {
                apply_legacy_frame_up();
                return;
            }
            const F32 vertical_shift =
                (image_up - projected_top * tan_half_fov * depth) /
                denominator;
            const F32 post_shift_depth =
                depth - vertical_shift * at.mV[VZ];
            if (!std::isfinite(vertical_shift) ||
                fabsf(vertical_shift) > AUTO_FRAME_MAX_DISTANCE ||
                !std::isfinite(post_shift_depth) ||
                post_shift_depth < llmax(0.05f, 0.25f * solved_distance))
            {
                apply_legacy_frame_up();
                return;
            }
            solved_focus.mV[VZ] += vertical_shift;
            solved_pos.mV[VZ] += vertical_shift;
            if (!solved_focus.isFinite() || !solved_pos.isFinite())
            {
                apply_legacy_frame_up();
                return;
            }
            mAutoFrameFocusXOffset =
                solved_focus.mV[VX] - base.mV[VX];
            mAutoFrameFocusYOffset =
                solved_focus.mV[VY] - base.mV[VY];
            mAutoFrameFocusZOffset =
                solved_focus.mV[VZ] - base.mV[VZ];
        }
        mAutoFrameHaveSolve = true;
        if (settle_mode == 0 || !allow_glide)
        {
            // Mode Off and stale camera-owner re-entry both use today's snap.
            mAutoFrameSettleActive = false;
        }
        else if (force_solve || manual_reframe)
        {
            F32 from = mAutoFrameHaveApplied
                ? mAutoFrameAppliedDistance : old_distance;
            if (!std::isfinite(from) || from <= 0.f)
            {
                from = solved_distance;
            }
            if (settle_max_ratio > 1.f)
            {
                from = llclamp(
                    from, solved_distance / settle_max_ratio,
                    solved_distance * settle_max_ratio);
            }
            mAutoFrameSettleFromDistance = from;
            mAutoFrameSettleStartPhase = force_solve ? 0.f : mPhase;
            mAutoFrameSettleModeLatched = settle_mode;
            mAutoFrameSettleDurationLatched = llclamp(
                settle_seconds, 0.05f, 10.f);
            mAutoFrameSettleCurveLatched = ALCameraCurve::sanitizeId(
                (S32)settle_curve_setting);
            mAutoFrameSettleBezierLatched[0] = ALCameraCurve::sanitizeX(
                (F32)settle_bezier_x1_setting, 0.42f);
            mAutoFrameSettleBezierLatched[1] = ALCameraCurve::sanitizeY(
                (F32)settle_bezier_y1_setting, 0.f);
            mAutoFrameSettleBezierLatched[2] = ALCameraCurve::sanitizeX(
                (F32)settle_bezier_x2_setting, 0.58f);
            mAutoFrameSettleBezierLatched[3] = ALCameraCurve::sanitizeY(
                (F32)settle_bezier_y2_setting, 1.f);
            mAutoFrameSettleFeatherLatched =
                std::isfinite((F32)settle_feather_setting)
                ? llclamp((F32)settle_feather_setting, 0.f, 1.f)
                : 0.f;
            mAutoFrameSettleActive = settle_mode != 2 ||
                (std::isfinite(settle_seconds) && settle_seconds > 0.05f);
        }
    }

    if (!mAutoFrameHaveSolve)
    {
        apply_legacy_frame_up();
        return;
    }

    const F32 effective_distance_trim =
        std::isfinite((F32)distance_trim) ? (F32)distance_trim : 0.f;
    F32 w = 1.f;
    if (mAutoFrameSettleActive && settle_mode != 0)
    {
        bool settle_complete = false;
        if (mAutoFrameSettleModeLatched == 1)
        {
            // Finish with the switcher's own freeze-safe wall-clock ease.
            if (mEaseActive && mEaseDuration > 0.f)
            {
                const F32 u = (F32)(
                    ALDirectorSwitcher::instance().cutEaseElapsedSeconds() /
                    mEaseDuration);
                w = (!std::isfinite(u) || u >= 1.f || u < 0.f)
                    ? 1.f
                    : ALCameraCurve::evalFeathered(
                        mEaseCurveId, u,
                        mEaseBezier[0], mEaseBezier[1],
                        mEaseBezier[2], mEaseBezier[3], mEaseFeather);
                settle_complete = !std::isfinite(u) || u >= 1.f;
            }
            // A hard cut or legacy path has no ease and therefore snaps.
            settle_complete = settle_complete || !mEaseActive;
        }
        else
        {
            // Post-cut settle is a closed function of presentation phase.
            const F32 elapsed = mPhase - mAutoFrameSettleStartPhase;
            w = elapsed < 0.f
                ? 1.f
                : ALCameraCurve::evalFeathered(
                    mAutoFrameSettleCurveLatched,
                    llmin(elapsed / mAutoFrameSettleDurationLatched, 1.f),
                    mAutoFrameSettleBezierLatched[0],
                    mAutoFrameSettleBezierLatched[1],
                    mAutoFrameSettleBezierLatched[2],
                    mAutoFrameSettleBezierLatched[3],
                    mAutoFrameSettleFeatherLatched);
            settle_complete = elapsed >= mAutoFrameSettleDurationLatched;
        }
        if (settle_complete)
        {
            mAutoFrameSettleActive = false;
        }
    }
    else if (settle_mode == 0)
    {
        mAutoFrameSettleActive = false;
    }
    const F32 blended = !mAutoFrameSettleActive || w == 1.f
        ? mAutoFrameDistance
        : expf(cc_lerp(logf(mAutoFrameSettleFromDistance),
                       logf(mAutoFrameDistance), w));
    const F32 effective_distance = llclamp(
        blended + effective_distance_trim,
        AUTO_FRAME_MIN_DISTANCE, AUTO_FRAME_MAX_DISTANCE);
    const F32 vertical_trim = legacy_frame_up;
    const LLVector3 base = cc_subjectBase(av);
    if (!std::isfinite(effective_distance) || !base.isFinite())
    {
        apply_legacy_frame_up();
        return;
    }

    if (mAutoFrameEyeLevel)
    {
        LLVector3 horizontal(pos.mV[VX] - focus.mV[VX],
                             pos.mV[VY] - focus.mV[VY], 0.f);
        if (!horizontal.isFinite() || horizontal.normVec() <= 0.001f)
        {
            apply_legacy_frame_up();
            return;
        }
        const F32 eye_z = base.mV[VZ] + mAutoFrameEyeZOffset + vertical_trim;
        focus = base + LLVector3(
            mAutoFrameFocusXOffset, mAutoFrameFocusYOffset,
            mAutoFrameEyeZOffset -
                effective_distance * mAutoFrameEyeAimSlope + vertical_trim);
        pos = LLVector3(focus.mV[VX], focus.mV[VY], eye_z) +
            horizontal * effective_distance;
    }
    else
    {
        LLVector3 rig_direction = pos - focus;
        if (!rig_direction.isFinite() || rig_direction.normVec() <= 0.001f)
        {
            apply_legacy_frame_up();
            return;
        }
        focus = base + LLVector3(
            mAutoFrameFocusXOffset, mAutoFrameFocusYOffset,
            mAutoFrameFocusZOffset + vertical_trim);
        pos = focus + rig_direction * effective_distance;
    }
    mAutoFrameAppliedDistance = blended;
    mAutoFrameHaveApplied = true;
}

// ---------------------------------------------------------------------------
void LLCinematicCamera::updateCamera()
{
    static LLCachedControl<S32>  mode(gSavedSettings, "CinematicCamMode", 1);
    static LLCachedControl<F32>  smoothing(gSavedSettings, "CinematicCamSmoothing", 0.35f);   // seconds
    static LLCachedControl<bool> bonelock_bypass(gSavedSettings, "CinematicCamBoneLockBypassSmoothing", true);
    static LLCachedControl<bool> look_at_head(gSavedSettings, "CinematicCamLookAtHead", true);
    static LLCachedControl<bool> use_operator(gSavedSettings, "CinematicCamUseOperator", false);
    static LLCachedControl<bool> apply_active(gSavedSettings, "CameraShakeApplyActive", false);
    static LLCachedControl<F32>  frame_up(gSavedSettings, "CinematicCamFrameOffsetUp", 0.f);
    static LLCachedControl<bool> auto_frame_enabled(
        gSavedSettings, "CinematicAutoFrameEnabled", false);

    LLVOAvatar* av = resolveTarget();
    if (!av)
    {
        return;
    }

    const ALDirectorSwitcher& switcher = ALDirectorSwitcher::instance();
    const bool switcher_driving = switcher.isDrivingCamera();
    const S32 current_mode =
        switcher_driving ? switcher.activeMode() : (S32)mode;
    const bool auto_frame_toggled =
        current_mode != MODE_BONE_LOCK &&
        (bool)auto_frame_enabled != mAutoFrameLastEnabled;
    const U64 switcher_serial =
        switcher_driving ? switcher.cutSerial() : 0;
    const F64 presentation_sample =
        LLPresentationTime::currentFrame().presentation_time;
    // Presentation time is normally guaranteed by the frame context. Fail
    // closed if a corrupt sample arrives: hold the last authored phase rather
    // than feeding NaN/negative values into camera geometry or easing.
    const F64 presentation_time =
        std::isfinite(presentation_sample) && presentation_sample >= 0.0
            ? presentation_sample
            : mSwitcherPhaseAnchor + (F64)mPhase;
    const LLUUID current_target = av->getID();
    const bool fresh_activation =
        gFrameCount > mLastUpdateFrame + 3;
    const bool mode_changed =
        current_mode != mLastMode;
    const bool target_changed =
        current_target != mLastTargetId;
    const bool serial_changed =
        switcher_serial != mLastSwitcherCutSerial;
    const bool stale_reentry =
        mLastUpdateFrame != 0 && fresh_activation;
    if (switcher_driving)
    {
        if (serial_changed)
        {
            // A real take retains its exact boundary even if the render thread
            // first observes it after a hitch.
            mSwitcherPhaseAnchor = switcher.activeSince();
        }
        else if (mode_changed || target_changed)
        {
            // A target/mode edit is a new authored shot even without a bank
            // punch. Restart it on this deterministic presentation sample.
            mSwitcherPhaseAnchor = presentation_time;
        }
        // A fresh re-entry after recorder/path/pilot pre-emption deliberately
        // keeps the existing take anchor and resumes at its current phase.
    }
    // Every mode and resolved-target change is a camera cut. Restart pattern,
    // tripod, smoothing, velocity, and operator state. A switcher serial also
    // makes two different slots carrying the same mode a real cut.
    if (fresh_activation || mode_changed || target_changed || serial_changed)
    {
        const bool motion_shot_changed = mode_changed || target_changed ||
            serial_changed || (!switcher_driving && fresh_activation) ||
            !mMotionStartCaptured;
        const F32 ease_seconds =
            switcher_driving && serial_changed
                ? switcher.cutEaseSeconds() : 0.f;
        const F64 cut_age =
            presentation_time - switcher.activeSince();
        const F64 ease_elapsed =
            switcher.cutEaseElapsedSeconds();
        // Never glide back from a stale pose after a higher-priority camera
        // pre-empted CineCam. First activation is allowed to ease from the
        // currently presented agent camera; a later >3-frame gap is not.
        if (ease_seconds > 0.f && cut_age >= 0.0 &&
            cut_age < ease_seconds && ease_elapsed >= 0.0 &&
            ease_elapsed < ease_seconds && !stale_reentry)
        {
            LLViewerCamera* cam = LLViewerCamera::getInstance();
            mEaseActive = true;
            mEaseDuration = ease_seconds;
            mEaseCurveId = switcher.cutEaseCurve();
            mEaseFeather = switcher.cutEaseFeather();
            const F32* cut_bezier = switcher.cutEaseBezier();
            for (S32 i = 0; i < 4; ++i)
            {
                mEaseBezier[i] = cut_bezier[i];
            }
            mEaseFromPos = cam->getOrigin();
            mEaseFromRot = cam->getQuaternion();
            mEaseFromFov = cam->getView();
        }
        else
        {
            mEaseActive = false;
        }
        mPhase = 0.f;
        mHavePose = false;
        mWasActive = false;
        mTripodPos = LLViewerCamera::getInstance()->getOrigin();
        mPrevPos = mTripodPos;
        mPrevRot = LLViewerCamera::getInstance()->getQuaternion();
        if (motion_shot_changed)
        {
            mMotionStartCaptured = false;
            mMotionShotIndex = switcher_driving
                ? static_cast<U64>(llround(switcher.activeSince() * 1000.0))
                : mMotionShotIndex + 1;
        }
        LLCameraOperator::instance().reset();
    }
    mLastMode = current_mode;
    mLastTargetId = current_target;
    mLastSwitcherCutSerial = switcher_serial;
    mLastUpdateFrame = gFrameCount;

    F32 dt = llclamp(gFrameIntervalSeconds.value(), 0.0005f, 0.25f);
    if (switcher_driving)
    {
        // Absolute presentation age removes render-frame grouping from a
        // switcher-authored motion shot. Legacy CineCam retains its exact
        // gFrameIntervalSeconds accumulator below.
        const F64 age =
            llmax(0.0, presentation_time - mSwitcherPhaseAnchor);
        mPhase = (F32)fmod(age, (F64)PHASE_WRAP);
    }
    else
    {
        mPhase += dt;
        if (mPhase > PHASE_WRAP)
        {
            mPhase -= PHASE_WRAP;
        }
    }

    // the point patterns frame: head when available, else chest height
    LLVector3 focus = av->getPositionAgent() + LLVector3(0.f, 0.f, 1.f);
    if (look_at_head)
    {
        if (LLJoint* head = av->getJoint("mHead"))
        {
            focus = head->getWorldPosition();
        }
    }
    LLVector3 center = av->getPositionAgent();

    // global frame offset: raise/lower the point every target-framing mode
    // circles around and aims at. Bone lock has its own mount offsets.
    if (current_mode != MODE_BONE_LOCK && !auto_frame_enabled)
    {
        const LLVector3 frame_off(0.f, 0.f, (F32)frame_up);
        focus += frame_off;
        center += frame_off;
    }

    if (!mMotionStartCaptured)
    {
        captureMotionStart(av, center, current_mode,
                           switcher_driving ? switcher.activeSlot() : -1,
                           mMotionShotIndex);
    }

    LLVector3 pos;
    LLQuaternion rot;
    bool have_rot = false;
    F32 mode_fov_mul = 1.f;     // dolly zoom writes this
    F32 mode_roll = 0.f;        // overhead spin writes this (radians)

    switch (current_mode)
    {
        case MODE_BONE_LOCK:  patternBoneLock(av, mPhase, pos, rot, have_rot); break;
        case MODE_ORBIT:      pos = patternOrbit(center, mPhase); break;
        case MODE_FLY_HOVER:  pos = patternHover(center, mPhase); break;
        case MODE_SWEEP:      pos = patternSweep(center, mPhase); break;
        case MODE_CRANE:      pos = patternCrane(center, mPhase); break;
        case MODE_DOLLY_ZOOM: pos = patternDollyZoom(av, focus, mPhase, mode_fov_mul); break;
        case MODE_PUSH_IN:    pos = patternPushIn(av, focus, mPhase); break;
        case MODE_LOW_HERO:   pos = patternLowHero(av, center, mPhase); break;
        case MODE_OVERHEAD:   pos = patternOverhead(center, mPhase, mode_roll); break;
        case MODE_OTS:        pos = patternOTS(av, focus); break;
        case MODE_CRASH_ZOOM: pos = patternCrashZoom(mPhase, mode_fov_mul); break;
        case MODE_SLOW_ZOOM:  pos = patternSlowZoom(mPhase, mode_fov_mul); break;
        case MODE_WHIP_ARC:   pos = patternWhipArc(av, center, mPhase); break;
        case MODE_ARC:        pos = patternArc(av, center, mPhase); break;
        case MODE_REVEAL:     pos = patternReveal(av, center, mPhase, focus); break;
        case MODE_PULL_BACK:  pos = patternPullBack(av, focus, mPhase); break;
        case MODE_TWO_SHOT:   pos = patternTwoShot(av, focus); break;
        case MODE_LEAD_FOLLOW:pos = patternLeadFollow(av, focus, mPhase); break;
        case MODE_ECU_EYES:   pos = patternECU(av, focus, mPhase, mode_fov_mul); break;
        case MODE_LONG_LENS:  pos = patternLongLens(av, focus, mPhase, mode_fov_mul); break;
        case MODE_SPIRAL:     pos = patternSpiral(center, mPhase); break;
        case MODE_PEDESTAL:   pos = patternPedestal(av, center, mPhase, focus); break;
        case MODE_BARREL_ROLL:  pos = patternBarrelRoll(av, center, mPhase, mode_roll); break;
        case MODE_CORKSCREW:    pos = patternCorkscrew(center, mPhase, mode_roll); break;
        case MODE_PENDULUM:     pos = patternPendulum(av, center, mPhase); break;
        case MODE_CONTRA_ORBIT: pos = patternContraOrbit(center, mPhase, mode_fov_mul); break;
        case MODE_FISHEYE_LUNGE:pos = patternFisheyeLunge(av, focus, mPhase, mode_fov_mul); break;
        case MODE_FLOOR_SKIMMER:pos = patternFloorSkimmer(av, center, mPhase); break;
        case MODE_BOOST_RISE:   pos = patternBoostRise(av, center, mPhase); break;
        case MODE_BOOM_OVER:    pos = patternBoomOver(av, center, mPhase); break;
        case MODE_TOP_SPIN:     pos = patternTopSpin(center, mPhase, mode_roll); break;
        case MODE_TURNTABLE:    pos = patternTurntable(center, mPhase); break;
        case MODE_FLOATING_ECU: pos = patternFloatingECU(av, focus, mPhase, mode_fov_mul); break;
        case MODE_TILT_WHIP:    pos = patternTiltWhip(av, center, mPhase, focus); break;
        case MODE_BODY_HELIX:    pos = patternBodyHelix(av, center, mPhase, focus); break;
        case MODE_DESCENT:       pos = patternDescent(av, center, mPhase, focus); break;
        case MODE_PARALLAX_SLIDE:pos = patternParallaxSlide(av, center, mPhase); break;
        case MODE_FIGURE_EIGHT:  pos = patternFigureEight(av, center, mPhase); break;
        case MODE_DETAIL_SWEEP:  pos = patternDetailSweep(av, center, mPhase, focus, mode_fov_mul); break;
        case MODE_CABLE_CAM:     pos = patternCableCam(av, center, mPhase); break;
        case MODE_BREATHING_HOLD:pos = patternBreathingHold(av, center, mPhase); break;
        case MODE_STATIC_WIDE:
        case MODE_STATIC_MEDIUM:
        case MODE_STATIC_CLOSE:
        case MODE_STATIC_PROFILE_L:
        case MODE_STATIC_PROFILE_R:
        case MODE_STATIC_LOW:
        case MODE_STATIC_HIGH:
        case MODE_STATIC_FULL:
            pos = patternStaticShot(
                av, center, current_mode, focus, mode_fov_mul);
            break;
        default:              return;
    }

    // Entity-clone scale is a render-only outer matrix, so both logical joint
    // positions and pattern meter offsets are still scale-1 here. Reproduce
    // that matrix for camera geometry about the same root/foot pivot. Keep the
    // scale-1 branch completely untouched.
    const F32 subject_scale = av->getUniformScale();
    if (subject_scale != 1.f)
    {
        switch (current_mode)
        {
            case MODE_OTS:
            case MODE_TWO_SHOT:
                // These multi-subject modes scale each body's geometry inside
                // their generators; a second A-pivot transform would distort
                // the real separation between the actors.
                break;
            case MODE_CRASH_ZOOM:
            case MODE_SLOW_ZOOM:
                // Tripod position is an absolute captured camera location.
                focus = cc_scaleAboutSubjectBase(av, focus);
                break;
            default:
                focus = cc_scaleAboutSubjectBase(av, focus);
                pos = cc_scaleAboutSubjectBase(av, pos);
                break;
        }
    }
    applyAutoReframe(
        av, current_mode,
        LLViewerCamera::getInstance()->getDefaultFOV() * mode_fov_mul,
        fresh_activation || mode_changed || target_changed || serial_changed,
        !stale_reentry,
        pos, focus);
    if (auto_frame_toggled)
    {
        // Toggling the assist is a discrete rig change. Do not leak the old
        // auto/authored smoothed pose into the newly selected base.
        mHavePose = false;
    }
    if (!have_rot)
    {
        rot = cc_lookAt(pos, focus);
    }

    // dutch angle (composable unease dial for every mode) + overhead spin,
    // as a roll in the camera's local frame -- same trim idiom as bone lock
    static LLCachedControl<F32> dutch(gSavedSettings, "CinematicCamDutchAngle", 0.f);   // deg
    const F32 total_roll = (F32)dutch * DEG_TO_RAD + mode_roll;
    if (fabsf(total_roll) > 0.0001f)
    {
        LLQuaternion roll_q;
        roll_q.setEulerAngles(total_roll, 0.f, 0.f);
        rot = roll_q * rot;
    }

    // ---- temporal smoothing (one-pole, framerate-independent) -------------
    // Bone Lock is a RIGID mount: patternBoneLock() already returns the exact
    // joint pose every frame, so the absolute one-pole would only lag it and
    // rubber-band against a MOVING mount (the dominant Bone Lock jitter). Auto-
    // bypass smoothing for Bone Lock (snap) unless the operator opts back in;
    // every other mode smooths exactly as before.
    const bool bypass_smoothing =
        (current_mode == MODE_BONE_LOCK) && bonelock_bypass;
    const F32 tau = bypass_smoothing ? 0.f : llmax((F32)smoothing, 0.f);
    if (!mHavePose || tau < 1e-3f)
    {
        mSmPos = pos;
        mSmRot = rot;
        mHavePose = true;
    }
    else
    {
        const F32 alpha = 1.f - expf(-dt / tau);
        mSmPos = mSmPos + (pos - mSmPos) * alpha;
        mSmRot = nlerp(alpha, mSmRot, rot);
    }

    LLVector3 out_pos = mSmPos;
    LLQuaternion out_rot = mSmRot;
    bool easing_this_frame = false;
    F32 ease_weight = 1.f;
    if (mEaseActive)
    {
        // Cut blends share the switcher's unscaled per-cut timer. A full
        // Temporal freeze therefore cannot deadlock the visual camera ease.
        const F64 elapsed = switcher.cutEaseElapsedSeconds();
        const F32 u = mEaseDuration > 0.f
            ? (F32)(elapsed / mEaseDuration) : 1.f;
        if (u >= 1.f || u < 0.f)
        {
            mEaseActive = false;
        }
        else
        {
            easing_this_frame = true;
            ease_weight = ALCameraCurve::evalFeathered(
                mEaseCurveId, u,
                mEaseBezier[0], mEaseBezier[1],
                mEaseBezier[2], mEaseBezier[3], mEaseFeather);
            out_pos = mEaseFromPos +
                (mSmPos - mEaseFromPos) * ease_weight;
            out_rot = nlerp(ease_weight, mEaseFromRot, mSmRot);
        }
    }
    const LLVector3 base_pos = out_pos;
    const LLQuaternion base_rot = out_rot;
    F32 fov_mul = 1.f;

    // ---- optional handheld texture on top ---------------------------------
    if (use_operator || apply_active)
    {
        static LLCachedControl<S32> operator_locomotion(
            gSavedSettings, "FlycamOperatorLocomotionMode", 0);
        if (apply_active)
        {
            static U32 sLastOperatorFrame = 0;
            if (sLastOperatorFrame == 0 || gFrameCount > sLastOperatorFrame + 1)
            {
                LLCameraOperator::instance().reset();
            }
            sLastOperatorFrame = gFrameCount;
        }
        if (!mWasActive)
        {
            LLCameraOperator::instance().reset();
        }
        LLCameraOperatorOutput op;
        if ((S32)operator_locomotion == 0)
        {
            // Legacy retains the exact variable-frame velocity path.
            LLMatrix3 axes(base_rot);
            const LLVector3 world_vel =
                (base_pos - mPrevPos) * (1.f / dt);
            LLQuaternion dq = base_rot * ~mPrevRot;
            F32 d_roll, d_pitch, d_yaw;
            LLMatrix3(dq).getEulerAngles(
                &d_roll, &d_pitch, &d_yaw);

            LLCameraOperatorInput opin;
            opin.mDeltaTime = dt;
            opin.mLinearVel = LLVector3(
                world_vel * LLVector3(axes.mMatrix[0]),
                world_vel * LLVector3(axes.mMatrix[1]),
                world_vel * LLVector3(axes.mMatrix[2]));
            opin.mAngularVel =
                LLVector3(d_roll, d_pitch, d_yaw) * (1.f / dt);
            op = LLCameraOperator::instance().update(opin);
        }
        else
        {
            // Procedural paths are sampled as absolute poses at fixed tick
            // boundaries, rather than as render-frame average velocities.
            op = LLCameraOperator::instance().updateFromPose(
                dt, base_pos, base_rot);
        }
        LLMatrix3 wobble(op.mRoll, op.mPitch, op.mYaw);
        out_rot = LLQuaternion(wobble) * out_rot;
        LLMatrix3 out_axes(out_rot);
        out_pos += LLVector3(out_axes.mMatrix[0]) * op.mPosOffset.mV[VX]
                 + LLVector3(out_axes.mMatrix[1]) * op.mPosOffset.mV[VY]
                 + LLVector3(out_axes.mMatrix[2]) * op.mPosOffset.mV[VZ];
        fov_mul = op.mFovMul;
    }

    mPrevPos = base_pos;
    mPrevRot = base_rot;
    mWasActive = true;

    // ---- write the render camera -------------------------------------------
    LLViewerCamera* cam = LLViewerCamera::getInstance();
    LLMatrix3 final_axes(out_rot);
    F32 final_fov = cam->getDefaultFOV() * mode_fov_mul * fov_mul;
    // Aspect-aware lens is applied BEFORE the cut ease so both ends of the
    // blend live in the same (lensed) space. mEaseFromFov (= cam->getView())
    // is already lens-transformed; applying the lens AFTER the lerp re-lensed
    // the "from" end and popped the FOV by the full lens amount at the start
    // of every eased cut (the "zoom" seen only with aspect-aware lens on).
    // cc_applyFrameLens is identity when the lens is disabled, so with the
    // feature off this is byte-for-byte the previous behavior.
    final_fov = cc_applyFrameLens(final_fov, cam);
    if (easing_this_frame)
    {
        final_fov = cc_lerp(mEaseFromFov, final_fov, ease_weight);
    }
    if (easing_this_frame)
    {
        // A short eased lens move is local camera presentation; broadcasting
        // every intermediate FOV to the simulator would create message churn.
        cam->setViewNoBroadcast(
            llclamp(final_fov, cam->getMinView(), cam->getMaxView()));
    }
    else
    {
        // Hard cuts and the final eased value use the normal path so simulator
        // interest calculations receive the settled lens.
        cam->setView(final_fov);
    }
    cam->setOrigin(out_pos);
    cam->mXAxis = LLVector3(final_axes.mMatrix[0]);
    cam->mYAxis = LLVector3(final_axes.mMatrix[1]);
    cam->mZAxis = LLVector3(final_axes.mMatrix[2]);
}
